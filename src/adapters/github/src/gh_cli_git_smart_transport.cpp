#include "gh_cli_git_smart_transport.hpp"
#include "github_cli_process_runner.hpp"
#include "github_paged_read.hpp"
#include "github_viewer_named_blob_read.hpp"

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

		class gh_cli_git_smart_transport final : public git_smart_transport_port, public viewer_named_blob_reader
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
					auto credential = acquire_gh_cli_credential(*process, Configuration, stop.get_token());
					if (!credential) {
						return std::unexpected{ git_smart_transport_error{ stop.stop_requested() ? failure::cancelled : failure::authentication } };
					}
					gh_cli_transport                    identity{ Profile, Configuration, *process, *credential };
					const git_smart_exact_blobs_request batch{ .Context = aRequest.Context, .ExpectedBlobs = { aRequest.ExpectedBlob }, .MaximumBytes = std::min(aRequest.MaximumBytes, maximum_git_exact_blob_batch_bytes) };
					if (const auto query = compile_gh_cli_exact_blobs_request(Configuration, Profile, batch)) {
						const auto response = run_gh_cli(*process, Configuration, *query, stop.get_token(), *credential);
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
				const auto invocation = compile_gh_cli_exact_blobs_request(Configuration, Profile, aRequest);
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
					// Constructed before the read so that its configuration
					// baseline precedes it, as the observation's did.
					auto credential = acquire_gh_cli_credential(*process, Configuration, stop.get_token());
					if (!credential) {
						return std::unexpected{ git_smart_transport_error{ stop.stop_requested() ? failure::cancelled : failure::authentication } };
					}
					gh_cli_transport identity{ Profile, Configuration, *process, *credential };
					const auto       response = run_gh_cli(*process, Configuration, *invocation, stop.get_token(), *credential);
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
					QCryptographicHash hash{ QCryptographicHash::Sha1 };
					auto               header = QByteArray{ "blob " } + QByteArray::number(static_cast<qulonglong>(text.size()));
					header.append('\0');
					hash.addData(header);
					hash.addData(text);
					if (hash.result().toHex().toStdString() != expected) {
						continue;
					}
					result[index] = bytes(std::string_view{ text.constData(), static_cast<std::size_t>(text.size()) });
				}
				return named;
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
					|| (Context.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees
						&& (!clone_option_available(capability->StandardOutput, "filter") || !clone_option_available(capability->StandardOutput, "depth")))) {
					return std::unexpected{ git_smart_transport_error{ failure::unavailable } };
				}
				auto credential = acquire_gh_cli_credential(*process, Configuration, aStop);
				if (!credential) {
					return std::unexpected{ git_smart_transport_error{ aStop.stop_requested() ? failure::cancelled : failure::authentication } };
				}
				gh_cli_transport identity{ Profile, Configuration, *process, *credential };
				aStep             = git_smart_transport_step::identity_before;
				const auto before = viewer(identity, aStop);
				if (!before) {
					return std::unexpected{ before.error() };
				}
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
			|| !aConfiguration.pin() || aConfiguration.pin()->Hostname != aProfile.hostname()
			|| aConfiguration.pin()->Login != aProfile.cli_account_login()
			|| aContext.Authority.Host != aProfile.host() || aContext.Revision.Base.algorithm() != core::git_hash_algorithm::sha1
			|| !observe_git(aGit) || !aConfigurationCurrent || !aConfigurationCurrent()) {
			return std::unexpected{ application::git_smart_transport_error{ application::git_smart_transport_failure::unavailable } };
		}
		return std::make_shared<gh_cli_git_smart_transport>(aProfile, aConfiguration, aGit, aSession, aContext, std::move(aConfigurationCurrent));
	}
}
