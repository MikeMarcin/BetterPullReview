#include "gh_cli_git_smart_transport.hpp"
#include "github_cli_process_runner.hpp"
#include "github_paged_read.hpp"
#include "github_viewer_named_blob_read.hpp"

#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/core/git_objects.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>

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
			gh_cli_chain_tree_store(std::unique_ptr<QTemporaryDir> aDirectory, std::filesystem::path aRoot, std::filesystem::path aGit,
									std::unique_ptr<github_cli_process_port> aProcess, std::vector<core::git_commit_oid> aHeld)
				: Directory{ std::move(aDirectory) }
				, Root{ std::move(aRoot) }
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
					Git, Root,
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
					= compile_gh_cli_local_git_request(Git, Root, { scope(), "--literal-pathspecs", "ls-tree", "-z", std::string{ core::canonical_hex(aCommit).view() }, "--", aPath },
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
				return std::optional{ *oid };
			}

		private:
			[[nodiscard]] auto scope() const -> std::string
			{
				return "--git-dir=" + gh_cli_git_clone_store(Root, 0U).string();
			}

			std::unique_ptr<QTemporaryDir>           Directory;
			std::filesystem::path                    Root;
			std::filesystem::path                    Git;
			std::unique_ptr<github_cli_process_port> Process;
			std::vector<core::git_commit_oid>        Held;
			std::mutex                               Mutex;
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
				return compile_gh_cli_local_git_request(Git, aRoot, std::move(aArguments), std::move(aInput), pack_limit);
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
				aStep = git_smart_transport_step::temporary_store;
				QTemporaryDir directory{ QStringLiteral("/private/tmp/bpr-cli-git-XXXXXX") };
				if (!directory.isValid()) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				const std::filesystem::path root{ directory.path().toStdString() };
				std::filesystem::create_directory(root / "empty");
				// Destruction reaps the worker before QTemporaryDir removes its store.
				aStep        = git_smart_transport_step::process_runner;
				auto process = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, { }, false });
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
				aStep          = git_smart_transport_step::temporary_store;
				auto directory = std::make_unique<QTemporaryDir>(QStringLiteral("/private/tmp/bpr-cli-git-XXXXXX"));
				if (!directory->isValid()) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				const std::filesystem::path root{ directory->path().toStdString() };
				std::filesystem::create_directory(root / "empty");
				aStep        = git_smart_transport_step::process_runner;
				auto process = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, { }, false });
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
					auto cloneProcess = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, { }, true });
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
					names += hex + "\n" + hex + "^{tree}\n";
				}
				auto       check   = compile_gh_cli_local_git_request(Git, root, { "--git-dir=" + gh_cli_git_clone_store(root, 0U).string(), "cat-file", "--batch-check=%(objectname) %(objecttype)" },
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
				return std::make_shared<gh_cli_chain_tree_store>(std::move(directory), root, Git, std::move(process), aRequest.Commits);
			}

			/// \brief Proves this `gh` and Git, by version and local help, before
			/// any credentialed use: every operation either acquisition path
			/// invokes, and the clone options its mode needs.
			auto qualify(github_cli_process_port& aProcess, const std::filesystem::path& aRoot, std::stop_token aStop,
						 git_smart_transport_step& aStep, const bool aDeepen = false) -> git_smart_transport_result<void>
			{
				auto*      process   = &aProcess;
				const auto root      = aRoot;
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
					auto cloneProcess = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, { }, true });
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
						const auto scope  = "--git-dir=" + gh_cli_git_clone_store(root, index).string();
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
					alternates += (gh_cli_git_clone_store(root, index) / "objects").string() + "\n";
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
				auto       packProcess = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, spool, false });
				if (!packProcess) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				std::vector<std::string> packArguments{ "--git-dir=" + store.string(), "pack-objects", "--stdout", "--revs", "--quiet" };
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
