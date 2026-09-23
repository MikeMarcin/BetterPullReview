#include "gh_cli_git_smart_transport.hpp"
#include "github_cli_process_runner.hpp"
#include "github_paged_read.hpp"

#include "bpr/application/git_protocol_v2.hpp"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <mutex>
#include <optional>
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
				const auto parsed = std::from_chars(remaining.data(), remaining.data() + 4U, length, 16);
				if (parsed.ec != std::errc{ } || parsed.ptr != remaining.data() + 4U) {
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

		class gh_cli_git_smart_transport final : public git_smart_transport_port
		{
		public:
			gh_cli_git_smart_transport(const validated_github_host_profile&  aProfile,
									   const validated_gh_cli_configuration& aConfiguration, const std::filesystem::path& aGit,
									   core::github_session_request_context aSession, const git_smart_request_context& aContext,
									   std::function<bool()> aCurrent)
				: Profile{ aProfile }
				, Configuration{ aConfiguration }
				, Git{ aGit }
				, Session{ aSession }
				, Context{ aContext }
				, ConfigurationCurrent{ std::move(aCurrent) }
				, GitObservation{ observe_git(aGit) }
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
				const auto invocation = compile_gh_cli_exact_blob_request(Configuration, Profile, aRequest);
				if (!invocation) {
					return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
				}
				std::stop_source   stop;
				std::stop_callback external{ aStop, [&] { stop.request_stop(); } };
				std::stop_callback revoked{ Revocation.get_token(), [&] { stop.request_stop(); } };
				try {
					auto process = make_github_cli_process_runner();
					if (!process) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					gh_cli_transport identity{ Profile, Configuration, *process };
					const auto       before = viewer(identity, stop.get_token());
					if (!before) {
						return std::unexpected{ before.error() };
					}
					const auto response = process->run(*invocation, stop.get_token());
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
					QCryptographicHash hash{ QCryptographicHash::Sha1 };
					auto               header = QByteArray{ "blob " } + QByteArray::number(static_cast<qulonglong>(count));
					header.append('\0');
					hash.addData(header);
					hash.addData(decoded.decoded);
					if (hash.result().toHex().toStdString() != core::canonical_hex(aRequest.ExpectedBlob).view()) {
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

			auto fetch(const git_smart_fetch_request& aRequest, git_smart_wire_consumer& aConsumer, std::stop_token aStop)
				-> git_smart_transport_result<void> override
			{
				std::lock_guard lock{ Mutex };
				if (const auto error = admission(aRequest.Context, aStop)) {
					return std::unexpected{ git_smart_transport_error{ *error } };
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
					return acquire_pack(*wants, aRequest.MaximumResponseBytes, aConsumer, stop.get_token());
				} catch (...) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
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
				if (observed->Viewer.Account != Context.Authority.Account) {
					revoke();
					return std::unexpected{ git_smart_transport_error{ failure::authentication } };
				}
				return observed->Viewer;
			}

			auto acquire_pack(const std::vector<core::git_commit_oid>& aWants, std::uint64_t aMaximum,
							  git_smart_wire_consumer& aConsumer, std::stop_token aStop) -> git_smart_transport_result<void>
			{
				QTemporaryDir directory{ QStringLiteral("/private/tmp/bpr-cli-git-XXXXXX") };
				if (!directory.isValid()) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				const std::filesystem::path root{ directory.path().toStdString() };
				std::filesystem::create_directory(root / "empty");
				// Destruction reaps the worker before QTemporaryDir removes its store.
				auto process = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, { }, false });
				if (!process) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				gh_cli_transport identity{ Profile, Configuration, *process };
				const auto       before = viewer(identity, aStop);
				if (!before) {
					return std::unexpected{ before.error() };
				}
				const auto ghVersion   = process->run(compile_gh_cli_version_request(Configuration), aStop);
				const auto gitVersion  = process->run(local_request(root, { "--version" }), aStop);
				const auto supportedGh = ghVersion && (ghVersion->StandardOutput.starts_with("gh version 2.100.0 ") || ghVersion->StandardOutput.starts_with("gh version 2.101.0 "));
				if (!supportedGh || !gitVersion
					|| gitVersion->StandardOutput != "git version 2.50.1 (Apple Git-155)\n") {
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::unavailable } };
				}
				for (std::size_t index = 0U; index < aWants.size(); ++index) {
					if (const auto error = admission(Context, aStop)) {
						return std::unexpected{ git_smart_transport_error{ *error } };
					}
					const auto invocation = compile_gh_cli_git_clone_request(Configuration, Profile, Context, aWants[index], Git, root, index);
					if (!invocation) {
						return std::unexpected{ git_smart_transport_error{ failure::invalid_request } };
					}
					auto cloneProcess = make_github_cli_git_acquisition_process_runner({ pack_limit, root, 96U * 1024U * 1024U, 128U, { }, true });
					if (!cloneProcess) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					const auto result = cloneProcess->run(*invocation, aStop);
					if (!result) {
						return std::unexpected{ git_smart_transport_error{ process_error(result.error()) } };
					}
					if (result->ExitCode != 0) {
						return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
					}
					if (Context.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees) {
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
				const auto after = viewer(identity, aStop);
				if (!after) {
					return std::unexpected{ after.error() };
				}
				if (before->Account != after->Account || before->Login != after->Login) {
					revoke();
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::authentication } };
				}
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
				if (const auto error = admission(Context, aStop)) {
					return std::unexpected{ git_smart_transport_error{ *error } };
				}
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

			validated_github_host_profile         Profile;
			validated_gh_cli_configuration        Configuration;
			std::filesystem::path                 Git;
			core::github_session_request_context  Session;
			git_smart_request_context             Context;
			std::function<bool()>                 ConfigurationCurrent;
			std::optional<executable_observation> GitObservation;
			std::stop_source                      Revocation;
			std::mutex                            Mutex;
		};
	}

	auto make_gh_cli_git_smart_transport(const validated_github_host_profile&  aProfile,
										 const validated_gh_cli_configuration& aConfiguration, const std::filesystem::path& aGit,
										 core::github_session_request_context aSession, const application::git_smart_request_context& aContext,
										 std::function<bool()> aConfigurationCurrent)
		-> application::git_smart_transport_result<application::git_smart_transport>
	{
		if (!application::valid(aContext) || aProfile.authentication_transport() != application::github_authentication_transport::github_cli
			|| aContext.Authority.Profile != aProfile.profile() || aContext.Authority.ProfileRevision.value() != aProfile.revision().value()
			|| aContext.Authority.Host != aProfile.host() || aContext.Revision.Base.algorithm() != core::git_hash_algorithm::sha1
			|| !observe_git(aGit) || !aConfigurationCurrent || !aConfigurationCurrent()) {
			return std::unexpected{ application::git_smart_transport_error{ application::git_smart_transport_failure::unavailable } };
		}
		return std::make_shared<gh_cli_git_smart_transport>(aProfile, aConfiguration, aGit, aSession, aContext, std::move(aConfigurationCurrent));
	}
}
