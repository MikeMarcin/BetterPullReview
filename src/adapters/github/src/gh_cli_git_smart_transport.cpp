#include "gh_cli_git_smart_transport.hpp"
#include "github_cli_process_runner.hpp"
#include "github_paged_read.hpp"
#include "github_viewer_named_blob_read.hpp"

#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/core/git_objects.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <thread>

namespace bpr::adapters::github::detail {
	namespace {
		using namespace application;
		using failure                                                          = git_smart_transport_failure;
		constexpr std::size_t                                       pack_limit = 32U * 1024U * 1024U;
		constexpr application::detail::git_protocol_v2_capabilities capabilities{
			.ObjectFormat = core::git_hash_algorithm::sha1,
			.LsRefs       = true,
			.Fetch        = true,
		};

		/// [[BPR-1450]]. A fetched blob's identity through the core's
		/// collision-detecting SHA-1, as the host checks it; never through
		/// QCryptographicHash.
		[[nodiscard]] auto blob_identity(const QByteArray& aBytes) -> std::expected<core::git_blob_oid, core::git_object_refusal>
		{
			const auto bytes = std::span{ reinterpret_cast<const std::uint8_t*>(aBytes.constData()), static_cast<std::size_t>(aBytes.size()) };
			return core::git_blob_identity(bytes, core::git_hash_algorithm::sha1);
		}

		// [[BPR-1857]]. A path Git reads from an argument or a file: UTF-8 with
		// forward slashes, which Git for Windows takes as it does a POSIX path.
		// A narrow `string()` would throw on a non-ASCII Windows profile.
		[[nodiscard]] auto git_path_text(const std::filesystem::path& aPath) -> std::string
		{
			const auto bytes = aPath.generic_u8string();
			return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
		}

		/// [[BPR-1857]]. A new clone root directly inside the platform's one
		/// admitted parent, or null when it cannot be made. On macOS that is
		/// `/private/tmp`, as before; on Windows and Linux the canonical system
		/// temporary directory.
		[[nodiscard]] auto make_clone_root() -> std::unique_ptr<QTemporaryDir>
		{
			const auto parent = gh_cli_git_temporary_parent();
			if (parent.empty()) {
				return nullptr;
			}
			auto directory = std::make_unique<QTemporaryDir>(
				QString::fromStdU16String((parent / "bpr-cli-git-XXXXXX").generic_u16string()));
			if (!directory->isValid()) {
				return nullptr;
			}
			return directory;
		}

		/// [[BPR-1873]]. Whether a file under \p aRoot reached the runner's
		/// per-file limit: Git stopped by `RLIMIT_FSIZE` dies with its pack
		/// unfinished, and its clone then only exits non-zero. Said apart so a
		/// size wall is reported as one, never as an unreadable head.
		[[nodiscard]] auto reached_file_limit(const std::filesystem::path& aRoot) -> bool
		{
			std::error_code error;
			auto            entries = std::filesystem::recursive_directory_iterator{ aRoot, std::filesystem::directory_options::none, error };
			for (std::size_t seen = 0U; !error && entries != std::filesystem::recursive_directory_iterator{ } && seen < 4096U; entries.increment(error), ++seen) {
				std::error_code entryError;
				if (entries->is_regular_file(entryError) && !entryError && entries->file_size(entryError) >= pack_limit && !entryError) {
					return true;
				}
			}
			return false;
		}

		/// [[BPR-1879]]. The bytes of the regular files under \p aRoot, without
		/// following a link: how much a kept clone has received so far.
		[[nodiscard]] auto received_bytes(const std::filesystem::path& aRoot) -> std::uint64_t
		{
			auto            total = std::uint64_t{ 0U };
			std::error_code error;
			auto            entries = std::filesystem::recursive_directory_iterator{ aRoot, std::filesystem::directory_options::none, error };
			for (std::size_t seen = 0U; !error && entries != std::filesystem::recursive_directory_iterator{ } && seen < 4096U; entries.increment(error), ++seen) {
				std::error_code entryError;
				if (entries->is_regular_file(entryError) && !entryError) {
					const auto size = entries->file_size(entryError);
					total += entryError ? 0U : size;
				}
			}
			return total;
		}

		/// The canonical path of \p aDirectory, which a clone request admits
		/// only when it is already canonical; empty when it cannot be resolved.
		[[nodiscard]] auto clone_root_path(const QTemporaryDir& aDirectory) -> std::filesystem::path
		{
			std::error_code error;
			auto            root = std::filesystem::canonical(std::filesystem::path{ aDirectory.path().toStdU16String() }, error);
			return error ? std::filesystem::path{ } : root;
		}

		// Git 2.49 introduced clone --revision, which keeps acquisition on exact accepted IDs.
		[[nodiscard]] auto supports_exact_revision_version(std::string_view aBanner) -> bool
		{
			constexpr std::string_view prefix = "git version ";
			if (!aBanner.starts_with(prefix)) {
				return false;
			}
			aBanner.remove_prefix(prefix.size());
			std::array<unsigned, 3U> version{ };
			for (std::size_t index = 0U; index < version.size(); ++index) {
				const auto parsed = std::from_chars(aBanner.data(), aBanner.data() + aBanner.size(), version[index]);
				if (parsed.ec != std::errc{ } || parsed.ptr == aBanner.data()) {
					return false;
				}
				aBanner.remove_prefix(static_cast<std::size_t>(parsed.ptr - aBanner.data()));
				if (index + 1U < version.size()) {
					if (aBanner.empty() || aBanner.front() != '.') {
						return false;
					}
					aBanner.remove_prefix(1U);
				}
			}
			return (aBanner.empty() || aBanner.front() == ' ' || aBanner.front() == '\n' || aBanner.front() == '.' || aBanner.front() == '-')
				&& (version[0U] > 2U || (version[0U] == 2U && version[1U] >= 49U));
		}

		[[nodiscard]] auto clone_option_available(std::string_view aHelp, std::string_view aOption) -> bool
		{
			for (const std::string_view prefix : { "--", "--[no-]" }) {
				const auto token    = std::string{ prefix } + std::string{ aOption };
				const auto position = aHelp.find(token);
				if (position == std::string_view::npos) {
					continue;
				}
				const auto end = position + token.size();
				if ((position == 0U || aHelp[position - 1U] == ' ' || aHelp[position - 1U] == '\n')
					&& (end == aHelp.size() || aHelp[end] == ' ' || aHelp[end] == '=' || aHelp[end] == '\n')) {
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] auto bytes(std::string_view aText) -> std::vector<std::byte>
		{
			const auto span = std::as_bytes(std::span{ aText.data(), aText.size() });
			return { span.begin(), span.end() };
		}

		[[nodiscard]] auto text(std::span<const std::byte> aBytes) -> std::string_view
		{
			return { reinterpret_cast<const char*>(aBytes.data()), aBytes.size() };
		}

		[[nodiscard]] auto packet(std::string_view aText) -> std::string
		{
			constexpr std::string_view digits = "0123456789abcdef";
			const auto                 size   = aText.size() + 4U;
			std::string                result(4U, '0');
			for (std::size_t index = 0U; index < 4U; ++index) {
				result[3U - index] = digits[(size >> (index * 4U)) & 15U];
			}
			result.append(aText);
			return result;
		}

		[[nodiscard]] auto fetch_wants(std::span<const std::byte>       aBody,
									   const git_smart_request_context& aContext) -> std::optional<std::vector<core::git_commit_oid>>
		{
			auto       remaining = text(aBody);
			const auto prelude   = packet("command=fetch\n") + packet("object-format=sha1\n") + "0001";
			if (!remaining.starts_with(prelude)) {
				return std::nullopt;
			}
			remaining.remove_prefix(prelude.size());
			std::vector<core::git_commit_oid> wants;
			std::size_t                       haves = 0U;
			bool                              done  = false;
			while (remaining.size() >= 4U) {
				unsigned   length = 0U;
				const auto header = remaining.substr(0U, 4U);
				const auto parsed = std::from_chars(header.data(), header.data() + header.size(), length, 16);
				if (parsed.ec != std::errc{ } || parsed.ptr != header.data() + header.size()) {
					return std::nullopt;
				}
				if (length == 0U) {
					if (!done || remaining.size() != 4U || wants.empty()) {
						return std::nullopt;
					}
					if (wants.size() == 2U && (wants[0] != aContext.Revision.Base || wants[1] != aContext.Revision.Head)) {
						return std::nullopt;
					}
					// A single want is the application's evidenced historical head.
					if (wants.size() == 2U && wants[0] == wants[1]) {
						return std::nullopt;
					}
					return wants;
				}
				if (done || length < 5U || length > remaining.size()) {
					return std::nullopt;
				}
				const auto line = remaining.substr(4U, length - 4U);
				remaining.remove_prefix(length);
				if (line == "done\n") {
					done = true;
					continue;
				}
				if (line.size() != 46U || line.back() != '\n') {
					return std::nullopt;
				}
				const auto oid = core::parse_git_commit_oid(line.substr(5U, 40U));
				if (!oid || core::canonical_hex(*oid).view() != line.substr(5U, 40U)) {
					return std::nullopt;
				}
				if (line.starts_with("want ") && haves == 0U && wants.size() < 2U) {
					wants.push_back(*oid);
				} else if (line.starts_with("have ") && !wants.empty() && ++haves <= maximum_git_protocol_v2_haves) {
					// A full local pack does not rely on a peer's have claims.
				} else {
					return std::nullopt;
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto process_error(github_cli_process_failure aFailure) -> failure
		{
			switch (aFailure) {
			case github_cli_process_failure::cancelled:             return failure::cancelled;
			case github_cli_process_failure::timeout:               return failure::timeout;
			case github_cli_process_failure::standard_output_limit:
			case github_cli_process_failure::standard_error_limit:  return failure::response_limit;
			default:                                                return failure::unavailable;
			}
		}

		struct executable_observation
		{
			std::filesystem::path           Path;
			std::uintmax_t                  Size;
			std::filesystem::file_time_type Modified;
			auto                            operator==(const executable_observation&) const -> bool = default;
		};

		[[nodiscard]] auto observe_git(const std::filesystem::path& aGit) -> std::optional<executable_observation>
		{
			std::error_code error;
			if (!aGit.is_absolute() || std::filesystem::canonical(aGit, error) != aGit || error
				|| !std::filesystem::is_regular_file(aGit, error) || error) {
				return std::nullopt;
			}
			const auto size = std::filesystem::file_size(aGit, error);
			if (error) {
				return std::nullopt;
			}
			const auto modified = std::filesystem::last_write_time(aGit, error);
			if (error) {
				return std::nullopt;
			}
			return executable_observation{ aGit, size, modified };
		}

		/// \brief RFC-BPR-0089. The output bound of one tree diff or lookup
		/// over the chain store: 10,000 raw entries of long paths.
		constexpr std::size_t chain_output_limit = 16U * 1024U * 1024U;
		/// The cache's own tree-diff bound (`maximum_git_changed_entries`).
		constexpr std::size_t chain_maximum_entries = 10000U;

		[[nodiscard]] auto parse_hex_oid(std::string_view aText) -> std::optional<core::git_blob_oid>
		{
			auto parsed = core::parse_git_blob_oid(aText);
			return parsed && core::canonical_hex(*parsed).view() == aText ? parsed : std::nullopt;
		}

		[[nodiscard]] auto valid_tree_path(std::string_view aPath) noexcept -> bool
		{
			return !aPath.empty() && aPath.size() <= 4096U && aPath.front() != '/' && aPath.find('\0') == std::string_view::npos
				&& aPath.find('\n') == std::string_view::npos;
		}

		/// \brief Parses `diff-tree -r -z --raw --no-abbrev` output exactly.
		[[nodiscard]] auto parse_raw_tree_diff(std::string_view aOutput) -> std::optional<std::vector<git_chain_changed_entry>>
		{
			auto entries = std::vector<git_chain_changed_entry>{ };
			auto next    = [&aOutput]() -> std::optional<std::string_view> {
				const auto end = aOutput.find('\0');
				if (end == std::string_view::npos) {
					return std::nullopt;
				}
				const auto token = aOutput.substr(0U, end);
				aOutput.remove_prefix(end + 1U);
				return token;
			};
			while (!aOutput.empty()) {
				const auto header = next();
				if (!header || header->size() < 2U || header->front() != ':') {
					return std::nullopt;
				}
				// ":<mode> <mode> <oid> <oid> <status>"
				auto fields = std::vector<std::string_view>{ };
				for (auto rest = header->substr(1U); !rest.empty();) {
					const auto space = rest.find(' ');
					fields.push_back(rest.substr(0U, space));
					rest = space == std::string_view::npos ? std::string_view{ } : rest.substr(space + 1U);
				}
				if (fields.size() != 5U || fields[4].empty()) {
					return std::nullopt;
				}
				const auto mode = [](std::string_view aMode) -> std::optional<std::uint32_t> {
					auto       value  = std::uint32_t{ 0U };
					const auto parsed = std::from_chars(aMode.data(), aMode.data() + aMode.size(), value, 8);
					return parsed.ec == std::errc{ } && parsed.ptr == aMode.data() + aMode.size() && aMode.size() == 6U ? std::optional{ value } : std::nullopt;
				};
				const auto leftMode  = mode(fields[0]);
				const auto rightMode = mode(fields[1]);
				const auto change    = fields[4].front();
				if (!leftMode || !rightMode || std::string_view{ "ACDMRT" }.find(change) == std::string_view::npos) {
					return std::nullopt;
				}
				const auto first = next();
				if (!first || !valid_tree_path(*first)) {
					return std::nullopt;
				}
				auto second = std::optional<std::string_view>{ };
				if (change == 'R' || change == 'C') {
					second = next();
					if (!second || !valid_tree_path(*second)) {
						return std::nullopt;
					}
				}
				auto entry   = git_chain_changed_entry{ };
				entry.Change = change;
				if (change != 'A') {
					const auto oid = parse_hex_oid(fields[2]);
					if (!oid || !oid->valid()) {
						return std::nullopt;
					}
					entry.Left = git_chain_changed_side{ .Path = std::string{ *first }, .Object = *oid, .Mode = *leftMode };
				}
				if (change != 'D') {
					const auto oid = parse_hex_oid(fields[3]);
					if (!oid || !oid->valid()) {
						return std::nullopt;
					}
					entry.Right = git_chain_changed_side{ .Path = std::string{ second.value_or(*first) }, .Object = *oid, .Mode = *rightMode };
				}
				entries.push_back(std::move(entry));
				if (entries.size() > chain_maximum_entries) {
					return std::nullopt;
				}
			}
			return entries;
		}

		/// \brief RFC-BPR-0089. The chain's commits and trees in a private
		/// temporary root, read by local Git with no network, lazy fetch or
		/// ambient configuration. Removed with the store; its runner is reaped
		/// before its directory goes, as the acquisition's is.
		class gh_cli_chain_tree_store final : public git_chain_tree_store
		{
		public:
			gh_cli_chain_tree_store(std::unique_ptr<QTemporaryDir> aDirectory, std::filesystem::path aRoot, const validated_gh_cli_configuration& aConfiguration, std::filesystem::path aGit,
									std::unique_ptr<github_cli_process_port> aProcess, std::vector<core::git_commit_oid> aHeld)
				: Directory{ std::move(aDirectory) }
				, Root{ std::move(aRoot) }
				, Configuration{ aConfiguration }
				, Git{ std::move(aGit) }
				, Process{ std::move(aProcess) }
				, Held{ std::move(aHeld) }
			{
				std::ranges::sort(Held);
			}

			[[nodiscard]] auto holds(const core::git_commit_oid& aCommit) const noexcept -> bool override
			{
				return std::ranges::binary_search(Held, aCommit);
			}

			[[nodiscard]] auto changed_entries(const core::git_commit_oid& aLeft, const core::git_commit_oid& aRight, std::stop_token aStop)
				-> git_smart_transport_result<std::vector<git_chain_changed_entry>> override
			{
				if (!holds(aLeft) || !holds(aRight)) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				std::lock_guard lock{ Mutex };
				const auto      request = compile_gh_cli_local_git_request(
					Configuration, Git, Root,
					{ scope(), "diff-tree", "-r", "-z", "--raw", "--no-abbrev", "--no-commit-id", "--no-ext-diff", "--no-textconv", "--find-renames=100%",
					  "--find-copies=100%", "-l10000", std::string{ core::canonical_hex(aLeft).view() }, std::string{ core::canonical_hex(aRight).view() }, "--" },
					{ }, chain_output_limit);
				const auto result = Process->run(request, aStop);
				if (!result) {
					return std::unexpected{ git_smart_transport_error{ process_error(result.error()) } };
				}
				if (result->ExitCode != 0) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				auto entries = parse_raw_tree_diff(result->StandardOutput);
				if (!entries) {
					return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
				}
				return std::move(*entries);
			}

			[[nodiscard]] auto find_blob_at_path(const core::git_commit_oid& aCommit, const std::string& aPath, std::stop_token aStop)
				-> git_smart_transport_result<std::optional<core::git_blob_oid>> override
			{
				if (!holds(aCommit) || !valid_tree_path(aPath)) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				std::lock_guard lock{ Mutex };
				// The trees alone name the blob: the store holds no blob, so a
				// lookup that checked the object would answer missing.
				const auto request
					= compile_gh_cli_local_git_request(Configuration, Git, Root, { scope(), "--literal-pathspecs", "ls-tree", "-z", std::string{ core::canonical_hex(aCommit).view() }, "--", aPath },
													   { }, 64U * 1024U);
				const auto result = Process->run(request, aStop);
				if (!result) {
					return std::unexpected{ git_smart_transport_error{ process_error(result.error()) } };
				}
				if (result->ExitCode != 0) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				// Nothing, or exactly "<mode> <type> <oid>\t<path>\0" for the path.
				const auto output = std::string_view{ result->StandardOutput };
				if (output.empty()) {
					return std::optional<core::git_blob_oid>{ };
				}
				const auto tab = output.find('\t');
				if (tab == std::string_view::npos || output.substr(tab + 1U) != aPath + std::string{ '\0' }) {
					return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
				}
				const auto record = output.substr(0U, tab);
				if (record.size() < 52U) {
					return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
				}
				const auto type = record.substr(7U, record.size() - 7U - 41U);
				if (type != "blob") {
					// A tree or a gitlink at the path is not a file's text.
					return type == "tree" || type == "commit" ? git_smart_transport_result<std::optional<core::git_blob_oid>>{ std::optional<core::git_blob_oid>{ } }
															  : git_smart_transport_result<std::optional<core::git_blob_oid>>{ std::unexpected{ git_smart_transport_error{ failure::malformed_response } } };
				}
				const auto oid = parse_hex_oid(record.substr(record.size() - 40U));
				if (!oid || !oid->valid() || record[6] != ' ' || record[record.size() - 41U] != ' ') {
					return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
				}
				return oid;
			}

		private:
			[[nodiscard]] auto scope() const -> std::string
			{
				return "--git-dir=" + git_path_text(gh_cli_git_clone_store(Root, 0U));
			}

			std::unique_ptr<QTemporaryDir>           Directory;
			std::filesystem::path                    Root;
			validated_gh_cli_configuration           Configuration;
			std::filesystem::path                    Git;
			std::unique_ptr<github_cli_process_port> Process;
			std::vector<core::git_commit_oid>        Held;
			std::mutex                               Mutex;
		};

		/// \brief [[BPR-1873]]. What a linked clone lacks of one head, as a
		/// pack kept in the transport's private temporary root until the host
		/// lets it go.
		class gh_cli_workspace_delta final : public git_workspace_delta
		{
		public:
			gh_cli_workspace_delta(std::unique_ptr<QTemporaryDir> aDirectory, const core::git_commit_oid& aHead, std::filesystem::path aPack,
								   const std::uint64_t aBytes)
				: Directory{ std::move(aDirectory) }
				, Head{ aHead }
				, Pack{ std::move(aPack) }
				, Bytes{ aBytes }
			{
			}

			[[nodiscard]] auto head() const noexcept -> const core::git_commit_oid& override { return Head; }
			[[nodiscard]] auto pack() const noexcept -> const std::filesystem::path& override { return Pack; }
			[[nodiscard]] auto pack_bytes() const noexcept -> std::uint64_t override { return Bytes; }

		private:
			std::unique_ptr<QTemporaryDir> Directory;
			core::git_commit_oid           Head;
			std::filesystem::path          Pack;
			std::uint64_t                  Bytes = 0U;
		};

		class gh_cli_git_smart_transport final : public git_smart_transport_port, public viewer_named_blob_reader
		{
		public:
			gh_cli_git_smart_transport(const validated_github_host_profile&  aProfile,
									   const validated_gh_cli_configuration& aConfiguration, const std::filesystem::path& aGit,
									   core::github_session_request_context aSession, const git_smart_request_context& aContext,
									   std::function<bool()> aCurrent, std::shared_ptr<github_insights_channel> aInsights)
				: Profile{ aProfile }
				, Configuration{ aConfiguration }
				, Git{ aGit }
				, Session{ aSession }
				, Context{ aContext }
				, ConfigurationCurrent{ std::move(aCurrent) }
				, GitObservation{ observe_git(aGit) }
				, Insights{ std::move(aInsights) }
			{ }

			void revoke() noexcept override { Revocation.request_stop(); }
			auto revoked() const noexcept -> bool override { return Revocation.stop_requested(); }

			auto discover(const git_smart_discovery_request& aRequest, std::stop_token aStop)
				-> git_smart_transport_result<git_smart_discovery_response> override
			{
				std::lock_guard lock{ Mutex };
				if (const auto error = admission(aRequest.Context, aStop)) {
					return std::unexpected{ git_smart_transport_error{ *error } };
				}
				return git_smart_discovery_response{ bytes(packet("version 2\n") + packet("ls-refs\n")
														   + packet("fetch\n") + packet("object-format=sha1\n") + "0000") };
			}

			auto ls_refs(const git_smart_ls_refs_request& aRequest, std::stop_token aStop)
				-> git_smart_transport_result<git_smart_ls_refs_response> override
			{
				std::lock_guard lock{ Mutex };
				if (const auto error = admission(aRequest.Context, aStop)) {
					return std::unexpected{ git_smart_transport_error{ *error } };
				}
				const auto expected = application::detail::make_git_protocol_v2_ls_refs_request(Context, capabilities);
				if (!expected || !std::ranges::equal(*expected, aRequest.Body)) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				// Snapshot-derived refs: the exact objects are checked after clone and
				// again by the existing pack/cache path, not inferred from these lines.
				return git_smart_ls_refs_response{ bytes(packet(std::string{ core::canonical_hex(Context.Revision.Base).view() }
																+ " " + std::string{ Context.BaseRef.view() } + "\n")
														 + packet(std::string{ core::canonical_hex(Context.Revision.Head).view() } + " refs/pull/"
																  + std::to_string(Context.Authority.Number.value()) + "/head\n")
														 + "0000") };
			}

			auto read_exact_blob(const git_smart_exact_blob_request& aRequest, std::stop_token aStop)
				-> git_smart_transport_result<std::vector<std::byte>> override
			{
				std::lock_guard lock{ Mutex };
				if (const auto error = admission(aRequest.Context, aStop)) {
					return std::unexpected{ git_smart_transport_error{ *error } };
				}
				auto invocation = compile_gh_cli_exact_blob_request(Configuration, Profile, aRequest);
				if (!invocation) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				invocation->Insights         = Insights;
				invocation->InsightsResource = core::insights_resource::core;
				invocation->InsightsCategory = core::insights_category::opening;
				std::stop_source   stop;
				std::stop_callback external{ aStop, [&] { stop.request_stop(); } };
				std::stop_callback revoked{ Revocation.get_token(), [&] { stop.request_stop(); } };
				try {
					auto process = make_github_cli_process_runner();
					if (!process) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					process->set_insights_channel(Insights);
					auto credential = acquire_gh_cli_credential(*process, Configuration, stop.get_token());
					if (!credential) {
						return std::unexpected{ git_smart_transport_error{ stop.stop_requested() ? failure::cancelled : (credential.error().CredentialUnavailable ? failure::authentication : process_error(credential.error().Failure)) } };
					}
					gh_cli_transport identity{ Profile, Configuration, *process, *credential };
					identity.set_insights_channel(Insights);
					const git_smart_exact_blobs_request batch{ .Context = aRequest.Context, .ExpectedBlobs = { aRequest.ExpectedBlob }, .MaximumBytes = std::min(aRequest.MaximumBytes, maximum_git_exact_blob_batch_bytes) };
					if (auto query = compile_gh_cli_exact_blobs_request(Configuration, Profile, batch)) {
						query->Insights         = Insights;
						query->InsightsResource = core::insights_resource::graphql;
						query->InsightsCategory = core::insights_category::opening;
						const auto response     = run_gh_cli(*process, Configuration, *query, stop.get_token(), *credential);
						if (response && response->ExitCode == 0) {
							auto named = verified_blobs(batch, response->StandardOutput);
							if (named) {
								if (named->Viewer.Account != Context.Authority.Account || named->Viewer.Login != Profile.cli_account_login()) {
									revoke();
									return std::unexpected{ git_smart_transport_error{ failure::authentication } };
								}
								if (named->Blobs.size() == 1U && named->Blobs.front()) {
									if (const auto error = admission(aRequest.Context, stop.get_token())) {
										return std::unexpected{ git_smart_transport_error{ *error } };
									}
									if (!identity.configuration_unchanged()) {
										return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
									}
									return std::move(*named->Blobs.front());
								}
							}
						}
					}
					const auto before = viewer(identity, stop.get_token());
					if (!before) {
						return std::unexpected{ before.error() };
					}
					const auto response = run_gh_cli(*process, Configuration, *invocation, stop.get_token(), *credential);
					if (!response) {
						return std::unexpected{ git_smart_transport_error{ process_error(response.error()) } };
					}
					if (response->ExitCode != 0) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					const auto after = viewer(identity, stop.get_token());
					if (!after) {
						return std::unexpected{ after.error() };
					}
					if (before->Account != after->Account || before->Login != after->Login) {
						revoke();
						return std::unexpected{ git_smart_transport_error{ failure::authentication } };
					}
					if (const auto error = admission(aRequest.Context, stop.get_token())) {
						return std::unexpected{ git_smart_transport_error{ *error } };
					}
					if (!identity.configuration_unchanged()) {
						revoke();
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					const auto      malformed = std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
					QJsonParseError error;
					const auto      document = QJsonDocument::fromJson(QByteArray::fromStdString(response->StandardOutput), &error);
					if (error.error != QJsonParseError::NoError || !document.isObject()) {
						return malformed;
					}
					const auto object = document.object();
					const auto size   = object.value("size");
					if (!size.isDouble() || size.toDouble() < 0 || std::floor(size.toDouble()) != size.toDouble()) {
						return malformed;
					}
					if (size.toDouble() > static_cast<double>(aRequest.MaximumBytes)) {
						return std::unexpected{ git_smart_transport_error{ failure::response_limit } };
					}
					const auto count = static_cast<std::size_t>(size.toDouble());
					if (static_cast<double>(count) != size.toDouble() || object.value("encoding").toString() != "base64"
						|| object.value("sha").toString().toStdString() != core::canonical_hex(aRequest.ExpectedBlob).view()
						|| !object.value("content").isString()) {
						return malformed;
					}
					auto encoded = object.value("content").toString().toUtf8();
					encoded.replace("\n", "");
					const auto decoded = QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
					if (!decoded || decoded.decoded.toBase64() != encoded || static_cast<std::size_t>(decoded.decoded.size()) != count) {
						return malformed;
					}
					// The core's collision-detecting SHA-1, the same check as the host's.
					const auto blob = blob_identity(decoded.decoded);
					if (!blob && blob.error() == core::git_object_refusal::collision_detected) {
						return std::unexpected{ git_smart_transport_error{ failure::collision_detected, git_smart_transport_step::object_check } };
					}
					if (!blob || *blob != aRequest.ExpectedBlob) {
						return malformed;
					}
					if (stop.stop_requested()) {
						return std::unexpected{ git_smart_transport_error{ failure::cancelled } };
					}
					return bytes(std::string_view{ decoded.decoded.constData(), count });
				} catch (...) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
			}

			/// [[BPR-1327]]. One GraphQL read for a batch of blobs. Every answer
			/// is verified against its object ID; anything else in the batch is
			/// answered absent rather than failing the rest. [[BPR-1370]]: the
			/// answer names the account that read it, so no observation brackets
			/// the read; one naming an account other than this authority's
			/// revokes the transport and refuses.
			auto read_exact_blobs(const git_smart_exact_blobs_request& aRequest, std::stop_token aStop)
				-> git_smart_transport_result<git_smart_exact_blobs> override
			{
				auto answer = read_exact_blobs_naming_viewer(aRequest, aStop);
				if (!answer) {
					return std::unexpected{ answer.error() };
				}
				if (revoked()) {
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::authentication } };
				}
				return std::move(answer->Blobs);
			}

			auto read_exact_blobs_naming_viewer(const git_smart_exact_blobs_request& aRequest, std::stop_token aStop)
				-> git_smart_transport_result<viewer_named_blobs> override
			{
				std::lock_guard lock{ Mutex };
				if (const auto error = admission(aRequest.Context, aStop)) {
					return std::unexpected{ git_smart_transport_error{ *error } };
				}
				auto invocation = compile_gh_cli_exact_blobs_request(Configuration, Profile, aRequest);
				if (!invocation) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				invocation->Insights         = Insights;
				invocation->InsightsResource = core::insights_resource::graphql;
				invocation->InsightsCategory = core::insights_category::opening;
				std::stop_source   stop;
				std::stop_callback external{ aStop, [&] { stop.request_stop(); } };
				std::stop_callback revoked{ Revocation.get_token(), [&] { stop.request_stop(); } };
				try {
					auto process = make_github_cli_process_runner();
					if (!process) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					// Constructed before the read so that its configuration
					// baseline precedes it, as the observation's did.
					process->set_insights_channel(Insights);
					auto credential = acquire_gh_cli_credential(*process, Configuration, stop.get_token());
					if (!credential) {
						return std::unexpected{ git_smart_transport_error{ stop.stop_requested() ? failure::cancelled : (credential.error().CredentialUnavailable ? failure::authentication : process_error(credential.error().Failure)) } };
					}
					gh_cli_transport identity{ Profile, Configuration, *process, *credential };
					identity.set_insights_channel(Insights);
					const auto response = run_gh_cli(*process, Configuration, *invocation, stop.get_token(), *credential);
					if (!response) {
						return std::unexpected{ git_smart_transport_error{ process_error(response.error()) } };
					}
					if (response->ExitCode != 0) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					if (const auto error = admission(aRequest.Context, stop.get_token())) {
						return std::unexpected{ git_smart_transport_error{ *error } };
					}
					if (!identity.configuration_unchanged()) {
						revoke();
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					auto answer = verified_blobs(aRequest, response->StandardOutput);
					if (!answer) {
						return answer;
					}
					if (answer->Viewer.Account != Context.Authority.Account || answer->Viewer.Login != Profile.cli_account_login()) {
						revoke();
						answer->Blobs.assign(aRequest.ExpectedBlobs.size(), std::nullopt);
					}
					return answer;
				} catch (...) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
			}

			[[nodiscard]] static auto verified_blobs(const git_smart_exact_blobs_request& aRequest, const std::string& aOutput)
				-> git_smart_transport_result<viewer_named_blobs>
			{
				const auto      malformed = std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
				QJsonParseError error;
				const auto      document = QJsonDocument::fromJson(QByteArray::fromStdString(aOutput), &error);
				if (error.error != QJsonParseError::NoError || !document.isObject()) {
					return malformed;
				}
				const auto data       = document.object().value("data").toObject();
				const auto repository = data.value("repository");
				if (!repository.isObject()) {
					return malformed;
				}
				// [[BPR-1370]]. The account GitHub resolved this request's token
				// to: the REST `id` and `login` that `gh api user` reports.
				const auto viewer   = data.value("viewer").toObject();
				const auto login    = viewer.value("login");
				const auto database = viewer.value("databaseId");
				if (!login.isString() || login.toString().isEmpty() || login.toString().size() > 256 || !database.isDouble()
					|| database.toDouble() < 1.0 || database.toDouble() > 9007199254740991.0
					|| std::floor(database.toDouble()) != database.toDouble()) {
					return malformed;
				}
				auto named   = viewer_named_blobs{ };
				named.Viewer = github_answering_account{
					.Account = core::github_account_id{ static_cast<std::uint64_t>(database.toDouble()) },
					.Login   = login.toString().toStdString(),
				};
				auto& result = named.Blobs;
				result.resize(aRequest.ExpectedBlobs.size());
				for (std::size_t index = 0U; index < aRequest.ExpectedBlobs.size(); ++index) {
					const auto object   = repository.toObject().value(QString::fromStdString("b" + std::to_string(index))).toObject();
					const auto expected = std::string{ core::canonical_hex(aRequest.ExpectedBlobs[index]).view() };
					const auto size     = object.value("byteSize");
					if (object.value("oid").toString().toStdString() != expected || object.value("isBinary").toBool(true)
						|| object.value("isTruncated").toBool(true) || !object.value("text").isString() || !size.isDouble()
						|| size.toDouble() < 0 || size.toDouble() > static_cast<double>(aRequest.MaximumBytes)) {
						continue;
					}
					const auto text = object.value("text").toString().toUtf8();
					if (static_cast<double>(text.size()) != size.toDouble()) {
						continue;
					}
					// A mismatch or a collision detection leaves the answer absent.
					const auto blob = blob_identity(text);
					if (!blob || *blob != aRequest.ExpectedBlobs[index]) {
						continue;
					}
					result[index] = bytes(std::string_view{ text.constData(), static_cast<std::size_t>(text.size()) });
				}
				return named;
			}

			/// \brief RFC-BPR-0089. Clones the head's first-parent chain, trees
			/// and no blobs, into a private temporary root under the same
			/// qualification, identity observations and bounds as an epoch's
			/// sparse acquisition, and keeps it for local reads.
			auto acquire_chain_trees(const git_smart_chain_request& aRequest, std::stop_token aStop)
				-> git_smart_transport_result<std::shared_ptr<git_chain_tree_store>> override
			{
				std::lock_guard lock{ Mutex };
				auto            step = git_smart_transport_step::admission;
				if (const auto error = admission(aRequest.Context, aStop)) {
					return std::unexpected{ git_smart_transport_error{ *error, step } };
				}
				if (!application::valid(aRequest) || aRequest.Context.AcquisitionMode != git_smart_acquisition_mode::sparse_commit_trees) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				std::stop_source   stop;
				std::stop_callback external{ aStop, [&] { stop.request_stop(); } };
				std::stop_callback revoked{ Revocation.get_token(), [&] { stop.request_stop(); } };
				try {
					auto result = acquire_chain(aRequest, stop.get_token(), step);
					if (!result) {
						result.error().Step = step;
					}
					return result;
				} catch (...) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable, step } };
				}
			}

			/// \brief [[BPR-1873]]. Fetches what a linked clone lacks of one
			/// head, under the same qualification, identity observations and
			/// bounds as an epoch's sparse acquisition, and packs exactly that
			/// for an agent workspace.
			auto acquire_workspace_delta(const git_smart_workspace_delta_request& aRequest, std::stop_token aStop)
				-> git_smart_transport_result<std::shared_ptr<git_workspace_delta>> override
			{
				std::lock_guard lock{ Mutex };
				auto            step = git_smart_transport_step::admission;
				if (const auto error = admission(aRequest.Context, aStop)) {
					return std::unexpected{ git_smart_transport_error{ *error, step } };
				}
				if (!application::valid(aRequest)) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				std::stop_source   stop;
				std::stop_callback external{ aStop, [&] { stop.request_stop(); } };
				std::stop_callback revoked{ Revocation.get_token(), [&] { stop.request_stop(); } };
				try {
					auto result = acquire_delta(aRequest.Head, aRequest.Clone, stop.get_token(), step);
					if (!result) {
						result.error().Step = step;
					}
					return result;
				} catch (...) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable, step } };
				}
			}

			/// \brief [[BPR-1879]]. Makes the full clone BetterPullReview keeps
			/// for this repository, under the same qualification, identity
			/// observations and write confinement as every sparse acquisition,
			/// but outside its size limits.
			auto acquire_managed_clone(const git_smart_managed_clone_request& aRequest, const git_smart_clone_progress& aProgress, std::stop_token aStop)
				-> git_smart_transport_result<void> override
			{
				std::lock_guard lock{ Mutex };
				auto            step = git_smart_transport_step::admission;
				if (const auto error = admission(aRequest.Context, aStop)) {
					return std::unexpected{ git_smart_transport_error{ *error, step } };
				}
				if (!application::valid(aRequest)) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				std::stop_source   stop;
				std::stop_callback external{ aStop, [&] { stop.request_stop(); } };
				std::stop_callback revoked{ Revocation.get_token(), [&] { stop.request_stop(); } };
				try {
					auto result = acquire_kept_clone(aRequest, aProgress, stop.get_token(), step);
					if (!result) {
						result.error().Step = step;
					}
					return result;
				} catch (...) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable, step } };
				}
			}

			/// \brief [[BPR-1884]]. Brings the kept clone up to the pull
			/// request's head and base, under the same qualification, identity
			/// observations, pinned configuration and write confinement as a
			/// workspace delta, but on the kept clone's runner with no per-file
			/// bound.
			auto refresh_managed_clone(const git_smart_managed_clone_refresh_request& aRequest, std::stop_token aStop)
				-> git_smart_transport_result<void> override
			{
				std::lock_guard lock{ Mutex };
				auto            step = git_smart_transport_step::admission;
				if (const auto error = admission(aRequest.Context, aStop)) {
					return std::unexpected{ git_smart_transport_error{ *error, step } };
				}
				if (!application::valid(aRequest)) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				std::stop_source   stop;
				std::stop_callback external{ aStop, [&] { stop.request_stop(); } };
				std::stop_callback revoked{ Revocation.get_token(), [&] { stop.request_stop(); } };
				try {
					auto result = refresh_kept_clone(aRequest, stop.get_token(), step);
					if (!result) {
						result.error().Step = step;
					}
					return result;
				} catch (...) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable, step } };
				}
			}

			auto fetch(const git_smart_fetch_request& aRequest, git_smart_wire_consumer& aConsumer, std::stop_token aStop)
				-> git_smart_transport_result<void> override
			{
				std::lock_guard lock{ Mutex };
				auto            step = git_smart_transport_step::admission;
				if (const auto error = admission(aRequest.Context, aStop)) {
					return std::unexpected{ git_smart_transport_error{ *error, step } };
				}
				if (!application::valid(aRequest)) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				const auto wants = fetch_wants(aRequest.Body, Context);
				if (!wants) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				std::stop_source   stop;
				std::stop_callback external{ aStop, [&] { stop.request_stop(); } };
				std::stop_callback revoked{ Revocation.get_token(), [&] { stop.request_stop(); } };
				try {
					auto result = acquire_pack(*wants, aRequest.MaximumResponseBytes, aConsumer, stop.get_token(), step);
					if (!result) {
						result.error().Step = step;
					}
					return result;
				} catch (...) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable, step } };
				}
			}

		private:
			[[nodiscard]] auto admission(const git_smart_request_context& aContext, std::stop_token aStop) -> std::optional<failure>
			{
				if (aStop.stop_requested() || revoked()) {
					return failure::cancelled;
				}
				if (aContext != Context || !application::valid(aContext)
					|| (aContext.AcquisitionMode != git_smart_acquisition_mode::complete_closure && aContext.AcquisitionMode != git_smart_acquisition_mode::sparse_commit_trees)) {
					return failure::invalid_request;
				}
				if (!ConfigurationCurrent || !ConfigurationCurrent() || !GitObservation || observe_git(Git) != GitObservation) {
					revoke();
					return failure::unavailable;
				}
				return std::nullopt;
			}

			[[nodiscard]] auto local_request(const std::filesystem::path& aRoot, std::vector<std::string> aArguments,
											 std::string aInput = { }) const -> github_cli_process_request
			{
				return compile_gh_cli_local_git_request(Configuration, Git, aRoot, std::move(aArguments), std::move(aInput), pack_limit);
			}

			[[nodiscard]] auto viewer(gh_cli_transport& aTransport, std::stop_token aStop) -> git_smart_transport_result<github_viewer_record>
			{
				const auto server = Profile.server_contract() == github_server_contract::github_com_2022_11_28
					? server_contract::github_com
					: server_contract::ghes_3_18;
				const auto wire   = compile_read_request(semantic_read_request{ .Server = server, .Operation = read_operation::get_viewer_identity });
				if (!wire) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				const github_wire_request request{ .Correlation = github_wire_correlation{ .Parent = Session, .ProfileRevision = Profile.revision(), .Host = Profile.host(), .Operation = read_operation::get_viewer_identity, .Page = 1U }, .Wire = *wire };
				const auto                response = aTransport.perform(request, aStop);
				if (std::holds_alternative<github_wire_cancelled>(response)) {
					return std::unexpected{ git_smart_transport_error{ failure::cancelled } };
				}
				if (const auto* error = std::get_if<github_wire_error>(&response)) {
					const auto kind   = error->Error.Failure;
					const auto mapped = kind == github_provider_failure::timeout ? failure::timeout
						: kind == github_provider_failure::network               ? failure::network
						: kind == github_provider_failure::authentication        ? failure::authentication
						: kind == github_provider_failure::response_limit        ? failure::response_limit
																				 : failure::unavailable;
					return std::unexpected{ git_smart_transport_error{ mapped } };
				}
				const auto* success = std::get_if<github_wire_success>(&response);
				if (!success || success->Correlation != request.Correlation) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				const auto observed = normalize_observed_github_viewer(success->Response, server);
				if (!observed) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				if (observed->Viewer.Account != Context.Authority.Account || observed->Viewer.Login != Profile.cli_account_login()) {
					revoke();
					return std::unexpected{ git_smart_transport_error{ failure::authentication } };
				}
				return observed->Viewer;
			}

			auto acquire_pack(const std::vector<core::git_commit_oid>& aWants, std::uint64_t aMaximum,
							  git_smart_wire_consumer& aConsumer, std::stop_token aStop, git_smart_transport_step& aStep) -> git_smart_transport_result<void>
			{
				aStep                = git_smart_transport_step::temporary_store;
				const auto directory = make_clone_root();
				const auto root      = directory ? clone_root_path(*directory) : std::filesystem::path{ };
				if (root.empty()) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				std::filesystem::create_directory(root / "empty");
				// Destruction reaps the worker before QTemporaryDir removes its store.
				aStep        = git_smart_transport_step::process_runner;
				auto process = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, { }, false, Git });
				if (!process) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				process->set_insights_channel(Insights);
				if (auto qualified = qualify(*process, root, aStop, aStep); !qualified) {
					return std::unexpected{ qualified.error() };
				}
				auto credential = acquire_gh_cli_credential(*process, Configuration, aStop);
				if (!credential) {
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : (credential.error().CredentialUnavailable ? failure::authentication : process_error(credential.error().Failure)) } };
				}
				gh_cli_transport identity{ Profile, Configuration, *process, *credential };
				identity.set_insights_channel(Insights);
				aStep             = git_smart_transport_step::identity_before;
				const auto before = viewer(identity, aStop);
				if (!before) {
					return std::unexpected{ before.error() };
				}
				return clone_and_pack(aWants, aMaximum, aConsumer, aStop, aStep, root, *process, *credential, identity, *before);
			}

			auto acquire_chain(const git_smart_chain_request& aRequest, std::stop_token aStop, git_smart_transport_step& aStep)
				-> git_smart_transport_result<std::shared_ptr<git_chain_tree_store>>
			{
				aStep                = git_smart_transport_step::temporary_store;
				auto       directory = make_clone_root();
				const auto root      = directory ? clone_root_path(*directory) : std::filesystem::path{ };
				if (root.empty()) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				std::filesystem::create_directory(root / "empty");
				aStep        = git_smart_transport_step::process_runner;
				auto process = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, { }, false, Git });
				if (!process) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				process->set_insights_channel(Insights);
				if (auto qualified = qualify(*process, root, aStop, aStep, true); !qualified) {
					return std::unexpected{ qualified.error() };
				}
				auto credential = acquire_gh_cli_credential(*process, Configuration, aStop);
				if (!credential) {
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : (credential.error().CredentialUnavailable ? failure::authentication : process_error(credential.error().Failure)) } };
				}
				{
					gh_cli_transport identity{ Profile, Configuration, *process, *credential };
					identity.set_insights_channel(Insights);
					aStep             = git_smart_transport_step::identity_before;
					const auto before = viewer(identity, aStop);
					if (!before) {
						return std::unexpected{ before.error() };
					}
					aStep = git_smart_transport_step::admission;
					if (const auto error = admission(Context, aStop)) {
						return std::unexpected{ git_smart_transport_error{ *error } };
					}
					aStep                 = git_smart_transport_step::clone;
					const auto invocation = compile_gh_cli_git_clone_request(Configuration, Profile, Context, aRequest.Head, Git, root, 0U, aRequest.Depth);
					if (!invocation) {
						return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
					}
					auto cloneProcess = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, { }, true, Git });
					if (!cloneProcess) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					cloneProcess->set_insights_channel(Insights);
					const auto cloned = run_gh_cli(*cloneProcess, Configuration, *invocation, aStop, *credential);
					if (!cloned) {
						return std::unexpected{ git_smart_transport_error{ process_error(cloned.error()) } };
					}
					if (cloned->ExitCode != 0) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					aStep            = git_smart_transport_step::identity_after;
					const auto after = viewer(identity, aStop);
					if (!after) {
						return std::unexpected{ after.error() };
					}
					if (before->Account != after->Account || before->Login != after->Login) {
						revoke();
						return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::authentication } };
					}
				}
				// Every commit the host will read must be held, with its tree.
				aStep      = git_smart_transport_step::object_check;
				auto names = std::string{ };
				for (const auto& commit : aRequest.Commits) {
					const auto hex = std::string{ core::canonical_hex(commit).view() };
					names.append(hex).append("\n").append(hex).append("^{tree}\n");
				}
				auto       check   = compile_gh_cli_local_git_request(Configuration, Git, root, { "--git-dir=" + git_path_text(gh_cli_git_clone_store(root, 0U)), "cat-file", "--batch-check=%(objectname) %(objecttype)" },
																	  names, chain_output_limit);
				const auto checked = process->run(check, aStop);
				if (!checked) {
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : process_error(checked.error()) } };
				}
				auto lines = std::vector<std::string_view>{ };
				for (auto rest = std::string_view{ checked->StandardOutput }; !rest.empty();) {
					const auto end = rest.find('\n');
					lines.push_back(rest.substr(0U, end));
					rest = end == std::string_view::npos ? std::string_view{ } : rest.substr(end + 1U);
				}
				if (checked->ExitCode != 0 || lines.size() != 2U * aRequest.Commits.size()) {
					return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
				}
				for (std::size_t index = 0U; index < aRequest.Commits.size(); ++index) {
					const auto hex = std::string{ core::canonical_hex(aRequest.Commits[index]).view() };
					if (lines[2U * index] != hex + " commit" || !lines[2U * index + 1U].ends_with(" tree")) {
						// Not within the depth asked for, or absent: the store holds
						// less than the chain, which the host reports as such.
						return std::unexpected{ git_smart_transport_error{ failure::response_limit } };
					}
				}
				return std::make_shared<gh_cli_chain_tree_store>(std::move(directory), root, Configuration, Git, std::move(process), aRequest.Commits);
			}

			/// \brief [[BPR-1873]]. A sparse review's cache holds no blobs, so
			/// an agent workspace borrows the linked clone, and this fetches only
			/// what the clone lacks of the head.
			///
			/// \details `gh repo clone` with `--reference` makes a private store
			/// that borrows the clone through one alternates line, so Git's
			/// negotiation offers the clone's refs as haves and the server sends
			/// only the missing objects. Nothing is written to the clone: on macOS
			/// the clone's `gh` may write only inside the temporary root. The pack
			/// handed on holds exactly the objects reachable from the head and
			/// from no ref of the clone, and is self-contained.
			auto acquire_delta(const core::git_commit_oid& aHead, const std::filesystem::path& aClone, std::stop_token aStop,
							   git_smart_transport_step& aStep) -> git_smart_transport_result<std::shared_ptr<git_workspace_delta>>
			{
				aStep = git_smart_transport_step::temporary_store;
				std::error_code cloneError;
				const auto      clone = std::filesystem::canonical(aClone, cloneError);
				if (cloneError || clone != aClone || !std::filesystem::is_directory(clone, cloneError) || cloneError) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				auto       directory = make_clone_root();
				const auto root      = directory ? clone_root_path(*directory) : std::filesystem::path{ };
				if (root.empty()) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				std::filesystem::create_directory(root / "empty");
				aStep        = git_smart_transport_step::process_runner;
				auto process = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, { }, false, Git });
				if (!process) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				process->set_insights_channel(Insights);
				if (auto qualified = qualify(*process, root, aStop, aStep, false, true); !qualified) {
					return std::unexpected{ qualified.error() };
				}
				auto credential = acquire_gh_cli_credential(*process, Configuration, aStop);
				if (!credential) {
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : (credential.error().CredentialUnavailable ? failure::authentication : process_error(credential.error().Failure)) } };
				}
				{
					gh_cli_transport identity{ Profile, Configuration, *process, *credential };
					identity.set_insights_channel(Insights);
					aStep             = git_smart_transport_step::identity_before;
					const auto before = viewer(identity, aStop);
					if (!before) {
						return std::unexpected{ before.error() };
					}
					aStep = git_smart_transport_step::admission;
					if (const auto error = admission(Context, aStop)) {
						return std::unexpected{ git_smart_transport_error{ *error } };
					}
					aStep                 = git_smart_transport_step::clone;
					const auto invocation = compile_gh_cli_git_clone_request(Configuration, Profile, Context, aHead, Git, root, 0U, 0U, gh_cli_clone_content::delta, clone);
					if (!invocation) {
						return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
					}
					auto cloneProcess = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, { }, true, Git });
					if (!cloneProcess) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					cloneProcess->set_insights_channel(Insights);
					const auto cloned = run_gh_cli(*cloneProcess, Configuration, *invocation, aStop, *credential);
					if (!cloned) {
						const auto ended = process_error(cloned.error());
						return std::unexpected{ git_smart_transport_error{ ended == failure::unavailable && reached_file_limit(root) ? failure::response_limit : ended } };
					}
					if (cloned->ExitCode != 0) {
						// A pack stopped at the per-file limit is a size wall.
						return std::unexpected{ git_smart_transport_error{ reached_file_limit(root) ? failure::response_limit : failure::unavailable } };
					}
					aStep            = git_smart_transport_step::identity_after;
					const auto after = viewer(identity, aStop);
					if (!after) {
						return std::unexpected{ after.error() };
					}
					if (before->Account != after->Account || before->Login != after->Login) {
						revoke();
						return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::authentication } };
					}
				}
				aStep             = git_smart_transport_step::object_check;
				const auto store  = gh_cli_git_clone_store(root, 0U);
				const auto hex    = std::string{ core::canonical_hex(aHead).view() };
				const auto scope  = "--git-dir=" + git_path_text(store);
				const auto commit = process->run(local_request(root, { scope, "cat-file", "-t", hex }), aStop);
				const auto tree   = process->run(local_request(root, { scope, "cat-file", "-t", hex + "^{tree}" }), aStop);
				if (!commit || !tree) {
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::malformed_response } };
				}
				if (commit->ExitCode != 0 || tree->ExitCode != 0 || commit->StandardOutput != "commit\n" || tree->StandardOutput != "tree\n") {
					return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
				}
				// What the clone lacks: the objects reachable from the head and
				// from no ref of the clone. The received pack is not handed on
				// as it is, because completing its thin deltas copied in bases
				// the clone already holds.
				aStep                              = git_smart_transport_step::local_store;
				auto missingRequest                = local_request(root, { scope, "rev-list", "--objects", "--no-object-names", hex, "--not", "--alternate-refs" });
				missingRequest.StandardOutputLimit = github_cli_process_max_standard_input_bytes;
				const auto missing                 = process->run(missingRequest, aStop);
				if (!missing) {
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : process_error(missing.error()) } };
				}
				if (missing->ExitCode != 0 || !missing->StandardOutput.starts_with(hex + "\n")) {
					return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
				}
				aStep                  = git_smart_transport_step::local_pack;
				const auto spool       = root / "delta.pack";
				auto       packProcess = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, spool, false, Git });
				if (!packProcess) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				auto packRequest               = local_request(root, { scope, "pack-objects", "--stdout", "--quiet" }, missing->StandardOutput);
				packRequest.StandardInputLimit = github_cli_process_max_standard_input_bytes;
				const auto      packed         = packProcess->run(packRequest, aStop);
				std::error_code sizeError;
				const auto      bytes = std::filesystem::file_size(spool, sizeError);
				if (!packed || packed->ExitCode != 0) {
					if (!sizeError && bytes >= pack_limit) {
						return std::unexpected{ git_smart_transport_error{ failure::response_limit } };
					}
					return std::unexpected{ git_smart_transport_error{ packed ? failure::unavailable : process_error(packed.error()) } };
				}
				aStep = git_smart_transport_step::pack_read;
				std::array<char, 4> magic{ };
				{
					std::ifstream pack{ spool, std::ios::binary };
					pack.read(magic.data(), magic.size());
					if (sizeError || bytes < 32U || !pack || std::string_view{ magic.data(), magic.size() } != "PACK") {
						return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
					}
				}
				// The workspace reads it as an owner-private file only.
				std::filesystem::permissions(spool, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
											 std::filesystem::perm_options::replace, sizeError);
				if (sizeError) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				return std::make_shared<gh_cli_workspace_delta>(std::move(directory), aHead, spool, bytes);
			}

			/// \brief [[BPR-1879]]. The kept clone: `gh repo clone` of every
			/// object, with no checkout, into a private temporary root, then
			/// moved whole into the profile's folder of kept clones.
			///
			/// \details The clone writes only inside the temporary root, as every
			/// acquisition does (on macOS `gh` may write nowhere else), so the
			/// profile's folder is touched only by the final move. The download
			/// has no byte or time limit (owner decision 2026-11-05); received
			/// bytes are sampled from the root twice a second while it runs, and
			/// the stop is honoured at every step. A cancelled or failed clone
			/// leaves nothing behind: the root is removed with its directory.
			auto acquire_kept_clone(const git_smart_managed_clone_request& aRequest, const git_smart_clone_progress& aProgress, std::stop_token aStop,
									git_smart_transport_step& aStep) -> git_smart_transport_result<void>
			{
				aStep = git_smart_transport_step::temporary_store;
				std::error_code existing;
				// Nothing, not even a link, may be at the destination yet.
				if (std::filesystem::symlink_status(aRequest.Destination, existing).type() != std::filesystem::file_type::not_found) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				auto       directory = make_clone_root();
				const auto root      = directory ? clone_root_path(*directory) : std::filesystem::path{ };
				if (root.empty()) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				std::filesystem::create_directory(root / "empty");
				// The qualification, identity and object-check launches keep the
				// usual bounds, over their own empty folder: supervising the
				// whole root would count the clone itself, which has no limit.
				std::filesystem::create_directory(root / "control");
				aStep        = git_smart_transport_step::process_runner;
				auto process = make_github_cli_git_acquisition_process_runner({ pack_limit, root / "control", 96U * 1024U * 1024U, 128U, { }, false, Git });
				if (!process) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				process->set_insights_channel(Insights);
				if (auto qualified = qualify(*process, root, aStop, aStep, false, false, true); !qualified) {
					return std::unexpected{ qualified.error() };
				}
				auto credential = acquire_gh_cli_credential(*process, Configuration, aStop);
				if (!credential) {
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : (credential.error().CredentialUnavailable ? failure::authentication : process_error(credential.error().Failure)) } };
				}
				{
					gh_cli_transport identity{ Profile, Configuration, *process, *credential };
					identity.set_insights_channel(Insights);
					aStep             = git_smart_transport_step::identity_before;
					const auto before = viewer(identity, aStop);
					if (!before) {
						return std::unexpected{ before.error() };
					}
					aStep = git_smart_transport_step::admission;
					if (const auto error = admission(Context, aStop)) {
						return std::unexpected{ git_smart_transport_error{ *error } };
					}
					aStep                 = git_smart_transport_step::clone;
					const auto invocation = compile_gh_cli_git_clone_request(Configuration, Profile, Context, Context.Revision.Head, Git, root, 0U, 0U, gh_cli_clone_content::managed);
					if (!invocation) {
						return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
					}
					auto cloneProcess = make_github_cli_managed_clone_process_runner(Git);
					if (!cloneProcess) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					cloneProcess->set_insights_channel(Insights);
					auto cloned = github_cli_process_result{ std::unexpected{ github_cli_process_failure::cancelled } };
					{
						// Received bytes, sampled while the clone runs; the
						// sampler ends before the result is read.
						auto sampler = std::jthread{ [&aProgress, root](const std::stop_token aSampling) {
							std::mutex                  waiting;
							std::condition_variable_any wake;
							auto                        held = std::unique_lock{ waiting };
							while (!aSampling.stop_requested()) {
								// Woken early only by the stop, which ends the loop.
								static_cast<void>(wake.wait_for(held, aSampling, std::chrono::milliseconds{ 500 }, [] { return false; }));
								if (aProgress && !aSampling.stop_requested()) {
									aProgress(received_bytes(root));
								}
							}
						} };
						cloned = run_gh_cli(*cloneProcess, Configuration, *invocation, aStop, *credential);
					}
					if (!cloned) {
						return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : process_error(cloned.error()) } };
					}
					if (cloned->ExitCode != 0) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					aStep            = git_smart_transport_step::identity_after;
					const auto after = viewer(identity, aStop);
					if (!after) {
						return std::unexpected{ after.error() };
					}
					if (before->Account != after->Account || before->Login != after->Login) {
						revoke();
						return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::authentication } };
					}
				}
				// The clone holds a commit at its HEAD: the repository's default
				// branch, every object of it present.
				aStep             = git_smart_transport_step::object_check;
				const auto store  = gh_cli_git_clone_store(root, 0U);
				const auto scope  = "--git-dir=" + git_path_text(gh_cli_git_kept_clone_directory(root, 0U));
				const auto commit = process->run(local_request(root, { scope, "cat-file", "-t", "HEAD" }), aStop);
				if (!commit) {
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::malformed_response } };
				}
				if (commit->ExitCode != 0 || commit->StandardOutput != "commit\n") {
					return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
				}
				if (aStop.stop_requested()) {
					return std::unexpected{ git_smart_transport_error{ failure::cancelled } };
				}
				// Owner-private, as everything in the profile is: the purge
				// inventory refuses a file another account could read.
				aStep = git_smart_transport_step::local_store;
				{
					std::error_code walked;
					const auto      privateMode = [](const std::filesystem::path& aPath, std::error_code& aError) {
						std::filesystem::permissions(aPath, std::filesystem::perms::group_all | std::filesystem::perms::others_all, std::filesystem::perm_options::remove | std::filesystem::perm_options::nofollow, aError);
					};
					privateMode(store, walked);
					auto entries = std::filesystem::recursive_directory_iterator{ store, std::filesystem::directory_options::none, walked };
					for (; !walked && entries != std::filesystem::recursive_directory_iterator{ }; entries.increment(walked)) {
						if (entries->is_symlink(walked) || walked) {
							return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
						}
						privateMode(entries->path(), walked);
					}
					if (walked) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
				}
				// Moved whole into the profile's owner-private folder of kept
				// clones; only then does it exist there.
				std::error_code moved;
				const auto      parent = aRequest.Destination.parent_path();
				std::filesystem::create_directories(parent, moved);
				if (!moved) {
					std::filesystem::permissions(parent, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, moved);
				}
				if (moved || std::filesystem::is_symlink(std::filesystem::symlink_status(parent, moved)) || moved) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				if (aStop.stop_requested()) {
					return std::unexpected{ git_smart_transport_error{ failure::cancelled } };
				}
				std::filesystem::rename(store, aRequest.Destination, moved);
				if (moved) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				if (aProgress) {
					aProgress(received_bytes(aRequest.Destination));
				}
				return { };
			}

			/// \brief [[BPR-1884]]. The kept clone's maintenance fetch: for the
			/// head and then the base, whichever the clone lacks, a `delta`
			/// clone that borrows the kept clone into a private temporary root,
			/// on the kept clone's runner; then each received pack, which is
			/// self-contained, is added to the clone and named by
			/// `refs/bpr/pull/N/head` and `.../base`.
			///
			/// \details The `gh` launch writes only inside the temporary root,
			/// as every acquisition does; the clone is written afterwards by
			/// this process and by credential-free local Git only. A pack is
			/// copied under a temporary name and renamed, its index last, so a
			/// pack Git can see is always whole: a failed or cancelled fetch
			/// leaves the clone as valid as it was. The default branch is never
			/// fetched (owner, 2026-11-06): a head fetch brings every ancestor
			/// the clone lacks.
			auto refresh_kept_clone(const git_smart_managed_clone_refresh_request& aRequest, std::stop_token aStop, git_smart_transport_step& aStep)
				-> git_smart_transport_result<void>
			{
				aStep = git_smart_transport_step::temporary_store;
				std::error_code cloneError;
				const auto      clone = std::filesystem::canonical(aRequest.Clone, cloneError);
				// Only a kept clone, in its working-folder form: never a link,
				// and never a folder outside the profile's folder of kept clones.
				if (cloneError || clone != aRequest.Clone || std::filesystem::is_symlink(std::filesystem::symlink_status(aRequest.Clone, cloneError)) || cloneError
					|| std::filesystem::symlink_status(gh_cli_git_kept_clone_directory(clone), cloneError).type() != std::filesystem::file_type::directory || cloneError) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				const auto gitDirectory = gh_cli_git_kept_clone_directory(clone);
				auto       directory    = make_clone_root();
				const auto root         = directory ? clone_root_path(*directory) : std::filesystem::path{ };
				if (root.empty()) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				std::filesystem::create_directory(root / "empty");
				std::filesystem::create_directory(root / "control");
				aStep        = git_smart_transport_step::process_runner;
				auto process = make_github_cli_git_acquisition_process_runner({ pack_limit, root / "control", 96U * 1024U * 1024U, 128U, { }, false, Git });
				if (!process) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				process->set_insights_channel(Insights);
				const auto scope   = "--git-dir=" + git_path_text(gitDirectory);
				const auto commits = std::array{ std::pair{ Context.Revision.Head, std::string_view{ "head" } }, std::pair{ Context.Revision.Base, std::string_view{ "base" } } };
				// What the clone lacks: the head first, then the base.
				aStep       = git_smart_transport_step::local_store;
				auto wanted = std::vector<core::git_commit_oid>{ };
				for (const auto& [commit, name] : commits) {
					if (!commit.valid() || commit.algorithm() != core::git_hash_algorithm::sha1 || std::ranges::find(wanted, commit) != wanted.end()) {
						continue;
					}
					const auto hex = std::string{ core::canonical_hex(commit).view() };
					// `--batch-check` answers present or missing with a zero exit;
					// no local Git request here opts into a nonzero one.
					const auto present = process->run(local_request(root, { scope, "cat-file", "--batch-check=%(objecttype)" }, hex + "\n"), aStop);
					if (!present) {
						return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : process_error(present.error()) } };
					}
					if (present->StandardOutput != "commit\n") {
						wanted.push_back(commit);
					}
				}
				if (!wanted.empty()) {
					if (auto fetched = fetch_into_kept_clone(clone, root, wanted, *process, aStop, aStep); !fetched) {
						return fetched;
					}
				}
				// The clone holds the pull request's commits; its refs name them,
				// so they are offered as haves and kept by Git's maintenance.
				aStep             = git_smart_transport_step::local_store;
				const auto number = std::to_string(Context.Authority.Number.value());
				for (const auto& [commit, name] : commits) {
					if (!commit.valid() || commit.algorithm() != core::git_hash_algorithm::sha1) {
						continue;
					}
					if (aStop.stop_requested()) {
						return std::unexpected{ git_smart_transport_error{ failure::cancelled } };
					}
					const auto hex     = std::string{ core::canonical_hex(commit).view() };
					const auto checked = process->run(local_request(root, { scope, "cat-file", "-t", hex + "^{tree}" }), aStop);
					if (!checked) {
						return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : process_error(checked.error()) } };
					}
					if (checked->ExitCode != 0 || checked->StandardOutput != "tree\n") {
						return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
					}
					const auto named = process->run(local_request(root, { scope, "update-ref", "refs/bpr/pull/" + number + "/" + std::string{ name }, hex }), aStop);
					if (!named) {
						return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : process_error(named.error()) } };
					}
					if (named->ExitCode != 0) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
				}
				return { };
			}

			/// \brief [[BPR-1884]]. Fetches each of \p aWanted, as clone 0 and
			/// clone 1 of \p aRoot, each borrowing \p aClone, then adds each
			/// received pack whole to \p aClone.
			auto fetch_into_kept_clone(const std::filesystem::path& aClone, const std::filesystem::path& aRoot, const std::vector<core::git_commit_oid>& aWanted,
									   github_cli_process_port& aProcess, std::stop_token aStop, git_smart_transport_step& aStep) -> git_smart_transport_result<void>
			{
				if (auto qualified = qualify(aProcess, aRoot, aStop, aStep, false, true); !qualified) {
					return std::unexpected{ qualified.error() };
				}
				auto credential = acquire_gh_cli_credential(aProcess, Configuration, aStop);
				if (!credential) {
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : (credential.error().CredentialUnavailable ? failure::authentication : process_error(credential.error().Failure)) } };
				}
				{
					gh_cli_transport identity{ Profile, Configuration, aProcess, *credential };
					identity.set_insights_channel(Insights);
					aStep             = git_smart_transport_step::identity_before;
					const auto before = viewer(identity, aStop);
					if (!before) {
						return std::unexpected{ before.error() };
					}
					for (std::size_t index = 0U; index < aWanted.size(); ++index) {
						aStep = git_smart_transport_step::admission;
						if (const auto error = admission(Context, aStop)) {
							return std::unexpected{ git_smart_transport_error{ *error } };
						}
						aStep                 = git_smart_transport_step::clone;
						const auto invocation = compile_gh_cli_git_clone_request(Configuration, Profile, Context, aWanted[index], Git, aRoot, index, 0U, gh_cli_clone_content::delta, aClone);
						if (!invocation) {
							return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
						}
						// The kept clone's own runner: no per-file bound.
						auto cloneProcess = make_github_cli_managed_clone_process_runner(Git);
						if (!cloneProcess) {
							return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
						}
						cloneProcess->set_insights_channel(Insights);
						const auto cloned = run_gh_cli(*cloneProcess, Configuration, *invocation, aStop, *credential);
						if (!cloned) {
							return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : process_error(cloned.error()) } };
						}
						if (cloned->ExitCode != 0) {
							return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::unavailable } };
						}
						aStep             = git_smart_transport_step::object_check;
						const auto hex    = std::string{ core::canonical_hex(aWanted[index]).view() };
						const auto within = "--git-dir=" + git_path_text(gh_cli_git_clone_store(aRoot, index));
						const auto commit = aProcess.run(local_request(aRoot, { within, "cat-file", "-t", hex }), aStop);
						if (!commit) {
							return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::malformed_response } };
						}
						if (commit->ExitCode != 0 || commit->StandardOutput != "commit\n") {
							return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
						}
					}
					aStep            = git_smart_transport_step::identity_after;
					const auto after = viewer(identity, aStop);
					if (!after) {
						return std::unexpected{ after.error() };
					}
					if (before->Account != after->Account || before->Login != after->Login) {
						revoke();
						return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::authentication } };
					}
				}
				// Each received pack, whole, into the clone: the pack and its
				// reverse index under temporary names, then renamed, the index
				// last, so Git sees a pack only once all of it is there.
				aStep             = git_smart_transport_step::local_pack;
				const auto target = gh_cli_git_kept_clone_directory(aClone) / "objects" / "pack";
				for (std::size_t index = 0U; index < aWanted.size(); ++index) {
					if (aStop.stop_requested()) {
						return std::unexpected{ git_smart_transport_error{ failure::cancelled } };
					}
					std::error_code walked;
					auto            packs = std::vector<std::filesystem::path>{ };
					for (auto entries = std::filesystem::directory_iterator{ gh_cli_git_clone_store(aRoot, index) / "objects" / "pack", walked };
						 !walked && entries != std::filesystem::directory_iterator{ }; entries.increment(walked)) {
						std::error_code kind;
						if (entries->path().extension() == ".pack" && entries->is_regular_file(kind) && !kind) {
							packs.push_back(entries->path());
						}
					}
					if (walked || packs.empty()) {
						return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
					}
					if (!std::filesystem::is_directory(target, walked) || walked) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					for (const auto& pack : packs) {
						auto indexFile = pack;
						indexFile.replace_extension(".idx");
						if (std::filesystem::exists(target / indexFile.filename(), walked)) {
							continue;
						}
						auto parts   = std::vector<std::filesystem::path>{ pack };
						auto reverse = pack;
						reverse.replace_extension(".rev");
						if (std::filesystem::exists(reverse, walked)) {
							parts.push_back(reverse);
						}
						parts.push_back(indexFile);
						for (const auto& part : parts) {
							const auto      staged = target / ("tmp_bpr_" + part.filename().string());
							std::error_code copied;
							std::filesystem::copy_file(part, staged, std::filesystem::copy_options::overwrite_existing, copied);
							if (!copied) {
								// Owner-private, as everything in the profile is.
								std::filesystem::permissions(staged, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace, copied);
							}
							if (!copied) {
								std::filesystem::rename(staged, target / part.filename(), copied);
							}
							if (copied) {
								std::error_code ignored;
								std::filesystem::remove(staged, ignored);
								return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
							}
						}
					}
				}
				return { };
			}

			/// \brief Proves this `gh` and Git, by version and local help, before
			/// any credentialed use: every operation either acquisition path
			/// invokes, and the clone options its mode needs. [[BPR-1873]]. A
			/// workspace delta also needs `--reference`.
			auto qualify(github_cli_process_port& aProcess, const std::filesystem::path& aRoot, std::stop_token aStop,
						 git_smart_transport_step& aStep, const bool aDeepen = false, const bool aReference = false, const bool aKept = false) -> git_smart_transport_result<void>
			{
				auto*       process  = &aProcess;
				const auto& root     = aRoot;
				aStep                = git_smart_transport_step::cli_version;
				const auto ghVersion = run_gh_cli(*process, Configuration, compile_gh_cli_version_request(Configuration), aStop);
				if (!ghVersion) {
					return std::unexpected{ git_smart_transport_error{ process_error(ghVersion.error()) } };
				}
				aStep = git_smart_transport_step::cli_qualification;
				if (!supported_gh_cli_version(ghVersion->StandardOutput)) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				// Local help proves each gh operation this path invokes before any
				// credentialed gh use; only a completed zero exit returns its text.
				for (const auto topic : gh_cli_required_help_topics) {
					aStep            = git_smart_transport_step::cli_capability_probe;
					const auto probe = run_gh_cli(*process, Configuration, compile_gh_cli_help_request(Configuration, topic), aStop);
					if (!probe) {
						return std::unexpected{ git_smart_transport_error{ process_error(probe.error()) } };
					}
					aStep = git_smart_transport_step::cli_qualification;
					if (!gh_cli_help_admits(topic, probe->StandardOutput)) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
				}
				aStep                              = git_smart_transport_step::git_version;
				auto versionRequest                = local_request(root, { "--version" });
				versionRequest.StandardOutputLimit = 4096U;
				versionRequest.Timeout             = std::chrono::seconds{ 10 };
				const auto gitVersion              = process->run(versionRequest, aStop);
				if (!gitVersion) {
					return std::unexpected{ git_smart_transport_error{ process_error(gitVersion.error()) } };
				}
				aStep = git_smart_transport_step::git_qualification;
				if (!supports_exact_revision_version(gitVersion->StandardOutput)) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				aStep                                 = git_smart_transport_step::git_capability_probe;
				auto capabilityRequest                = local_request(root, { "clone", "-h" });
				capabilityRequest.StandardOutputLimit = 16U * 1024U;
				capabilityRequest.Timeout             = std::chrono::seconds{ 10 };
				// clone -h conventionally returns 129. Only this completed local help probe
				// opts in; failure/timeout/cancellation/limit paths never expose partial output.
				capabilityRequest.PreserveCompletedOutput = true;
				const auto capability                     = process->run(capabilityRequest, aStop);
				if (!capability) {
					return std::unexpected{ git_smart_transport_error{ process_error(capability.error()) } };
				}
				if (capability->ExitCode != 0 && capability->ExitCode != 129) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				aStep = git_smart_transport_step::git_qualification;
				if (!clone_option_available(capability->StandardOutput, "revision")
					|| !clone_option_available(capability->StandardOutput, "bare")
					|| !clone_option_available(capability->StandardOutput, "template")
					|| !clone_option_available(capability->StandardOutput, "config")
					|| (aReference && !clone_option_available(capability->StandardOutput, "reference"))
					// [[BPR-1879]]. A kept clone is made with no checkout.
					|| (aKept && !clone_option_available(capability->StandardOutput, "no-checkout") && !clone_option_available(capability->StandardOutput, "checkout"))
					|| ((Context.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees || aDeepen)
						&& (!clone_option_available(capability->StandardOutput, "filter") || !clone_option_available(capability->StandardOutput, "depth")))) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				return { };
			}

			auto clone_and_pack(const std::vector<core::git_commit_oid>& aWants, std::uint64_t aMaximum, git_smart_wire_consumer& aConsumer,
								std::stop_token aStop, git_smart_transport_step& aStep, const std::filesystem::path& aRoot,
								github_cli_process_port& aProcess, const cli_transient_credential& aCredential, gh_cli_transport& aIdentity,
								const github_viewer_record& aBefore) -> git_smart_transport_result<void>
			{
				const auto& root       = aRoot;
				auto*       process    = &aProcess;
				const auto* credential = &aCredential;
				auto&       identity   = aIdentity;
				const auto* before     = &aBefore;
				for (std::size_t index = 0U; index < aWants.size(); ++index) {
					aStep = git_smart_transport_step::admission;
					if (const auto error = admission(Context, aStop)) {
						return std::unexpected{ git_smart_transport_error{ *error } };
					}
					aStep                 = git_smart_transport_step::clone;
					const auto invocation = compile_gh_cli_git_clone_request(Configuration, Profile, Context, aWants[index], Git, root, index);
					if (!invocation) {
						return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
					}
					auto cloneProcess = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, { }, true, Git });
					if (!cloneProcess) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					cloneProcess->set_insights_channel(Insights);
					// [[BPR-1373]]. Pinned, the clone and the credential helper it
					// launches read the same private configuration.
					const auto result = run_gh_cli(*cloneProcess, Configuration, *invocation, aStop, *credential);
					if (!result) {
						return std::unexpected{ git_smart_transport_error{ process_error(result.error()) } };
					}
					if (result->ExitCode != 0) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					if (Context.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees) {
						aStep             = git_smart_transport_step::object_check;
						const auto scope  = "--git-dir=" + git_path_text(gh_cli_git_clone_store(root, index));
						const auto oid    = std::string{ core::canonical_hex(aWants[index]).view() };
						const auto commit = process->run(local_request(root, { scope, "cat-file", "-t", oid }), aStop);
						const auto tree   = process->run(local_request(root, { scope, "cat-file", "-t", oid + "^{tree}" }), aStop);
						if (!commit || !tree) {
							return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::malformed_response } };
						}
						if (commit->StandardOutput != "commit\n" || tree->StandardOutput != "tree\n") {
							return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
						}
					}
				}
				aStep            = git_smart_transport_step::identity_after;
				const auto after = viewer(identity, aStop);
				if (!after) {
					return std::unexpected{ after.error() };
				}
				if (before->Account != after->Account || before->Login != after->Login) {
					revoke();
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::authentication } };
				}
				aStep            = git_smart_transport_step::local_store;
				const auto store = root / "serve.git";
				std::filesystem::create_directories(store / "objects/info");
				std::filesystem::create_directories(store / "refs/heads");
				const auto write = [](const std::filesystem::path& aPath, const std::string& aContent) {
					std::ofstream stream{ aPath, std::ios::binary };
					stream.write(aContent.data(), static_cast<std::streamsize>(aContent.size()));
					stream.close();
					if (!stream) {
						throw std::runtime_error{ "local store" };
					}
				};
				write(store / "HEAD", "ref: refs/heads/main\n");
				write(store / "config", "[core]\nrepositoryformatversion = 0\nbare = true\n");
				std::string alternates;
				std::string revisions;
				for (std::size_t index = 0U; index < aWants.size(); ++index) {
					alternates += git_path_text(gh_cli_git_clone_store(root, index) / "objects") + "\n";
					revisions += std::string{ core::canonical_hex(aWants[index]).view() } + "\n";
				}
				write(store / "objects/info/alternates", alternates);
				if (Context.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees) {
					// Alternates expose objects, not clone shallow metadata. Each exact
					// wanted commit is a boundary; ancestors are deliberately excluded.
					write(store / "shallow", revisions);
				}
				aStep                  = git_smart_transport_step::local_pack;
				const auto spool       = root / "wire.pack";
				auto       packProcess = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, spool, false, Git });
				if (!packProcess) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				std::vector<std::string> packArguments{ "--git-dir=" + git_path_text(store), "pack-objects", "--stdout", "--revs", "--quiet" };
				if (Context.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees) {
					packArguments.emplace_back("--filter=blob:none");
				}
				const auto packed = packProcess->run(local_request(root, std::move(packArguments), revisions), aStop);
				if (!packed) {
					std::error_code error;
					if (std::filesystem::file_size(spool, error) >= pack_limit && !error) {
						return std::unexpected{ git_smart_transport_error{ failure::response_limit } };
					}
					return std::unexpected{ git_smart_transport_error{ process_error(packed.error()) } };
				}
				aStep = git_smart_transport_step::admission;
				if (const auto error = admission(Context, aStop)) {
					return std::unexpected{ git_smart_transport_error{ *error } };
				}
				aStep                        = git_smart_transport_step::pack_read;
				const auto          packSize = std::filesystem::file_size(spool);
				std::ifstream       pack{ spool, std::ios::binary };
				std::array<char, 4> magic{ };
				pack.read(magic.data(), 4);
				if (!pack || std::string_view{ magic.data(), magic.size() } != "PACK" || packSize < 32U || packSize > pack_limit) {
					return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
				}
				pack.seekg(0);
				constexpr std::size_t chunk     = 60U * 1024U;
				const auto            wireBytes = packet("packfile\n").size() + packSize + ((packSize + chunk - 1U) / chunk) * 5U + 4U;
				if (wireBytes > aMaximum) {
					return std::unexpected{ git_smart_transport_error{ failure::response_limit } };
				}
				if (!aConsumer.consume(bytes(packet("packfile\n")))) {
					return std::unexpected{ git_smart_transport_error{ failure::consumer_rejected } };
				}
				for (std::uintmax_t offset = 0U; offset < packSize; offset += chunk) {
					if (aStop.stop_requested()) {
						return std::unexpected{ git_smart_transport_error{ failure::cancelled } };
					}
					const auto  count = static_cast<std::size_t>(std::min<std::uintmax_t>(chunk, packSize - offset));
					std::string band(count + 1U, '\1');
					pack.read(band.data() + 1U, static_cast<std::streamsize>(count));
					if (!pack) {
						return std::unexpected{ git_smart_transport_error{ failure::malformed_response } };
					}
					if (!aConsumer.consume(bytes(packet(band)))) {
						return std::unexpected{ git_smart_transport_error{ failure::consumer_rejected } };
					}
				}
				if (!aConsumer.consume(bytes("0000"))) {
					return std::unexpected{ git_smart_transport_error{ failure::consumer_rejected } };
				}
				return { };
			}

			validated_github_host_profile            Profile;
			validated_gh_cli_configuration           Configuration;
			std::filesystem::path                    Git;
			core::github_session_request_context     Session;
			git_smart_request_context                Context;
			std::function<bool()>                    ConfigurationCurrent;
			std::optional<executable_observation>    GitObservation;
			std::stop_source                         Revocation;
			std::mutex                               Mutex;
			std::shared_ptr<github_insights_channel> Insights;
		};
	}

	auto make_gh_cli_git_smart_transport(const validated_github_host_profile&  aProfile,
										 const validated_gh_cli_configuration& aConfiguration, const std::filesystem::path& aGit,
										 core::github_session_request_context aSession, const application::git_smart_request_context& aContext,
										 std::function<bool()> aConfigurationCurrent, std::shared_ptr<github_insights_channel> aInsights)
		-> application::git_smart_transport_result<application::git_smart_transport>
	{
		if (!application::valid(aContext) || aProfile.authentication_transport() != application::github_authentication_transport::github_cli
			|| aContext.Authority.Profile != aProfile.profile() || aContext.Authority.ProfileRevision.value() != aProfile.revision().value()
			|| !aConfiguration.pin() || aConfiguration.pin()->Hostname != aProfile.hostname()
			|| aConfiguration.pin()->Login != aProfile.cli_account_login()
			|| aContext.Authority.Host != aProfile.host() || aContext.Revision.Base.algorithm() != core::git_hash_algorithm::sha1
			|| !observe_git(aGit) || !aConfigurationCurrent || !aConfigurationCurrent()) {
			return std::unexpected{ application::git_smart_transport_error{ application::git_smart_transport_failure::unavailable } };
		}
		return std::make_shared<gh_cli_git_smart_transport>(aProfile, aConfiguration, aGit, aSession, aContext, std::move(aConfigurationCurrent), std::move(aInsights));
	}
}
