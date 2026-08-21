#include "github_public_path_fixture.hpp"

#include "github_capability_probe.hpp"
#include "github_read_contract.hpp"
#include "github_wire_transport.hpp"

#include "bpr/application/git_smart_transport_port.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::app::detail {
	namespace {
		using adapters::github::detail::capability_probe_response;
		using adapters::github::detail::github_capability_probe_wire_cancelled;
		using adapters::github::detail::github_capability_probe_wire_completion;
		using adapters::github::detail::github_capability_probe_wire_error;
		using adapters::github::detail::github_capability_probe_wire_request;
		using adapters::github::detail::github_capability_probe_wire_success;
		using adapters::github::detail::github_wire_cancelled;
		using adapters::github::detail::github_wire_completion;
		using adapters::github::detail::github_wire_error;
		using adapters::github::detail::github_wire_request;
		using adapters::github::detail::github_wire_success;
		using adapters::github::detail::github_wire_transport_factory_port;
		using adapters::github::detail::github_wire_transport_factory_result;
		using adapters::github::detail::github_wire_transport_port;
		using adapters::github::detail::http_header;
		using adapters::github::detail::page_completeness;
		using adapters::github::detail::parse_recorded_http_method;
		using adapters::github::detail::read_operation;
		using adapters::github::detail::read_operation_catalog;
		using adapters::github::detail::read_operation_from_name;
		using adapters::github::detail::recorded_exchange;
		using adapters::github::detail::server_contract;
		using adapters::github::detail::strict_recorded_transport;
		using adapters::github::detail::wire_response;

		[[nodiscard]] auto provider_error(
			const application::github_provider_failure aFailure)
			-> application::github_provider_error
		{
			return {
				.Failure           = aFailure,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = { },
				.RateLimitCause    = std::nullopt,
			};
		}

		[[nodiscard]] auto read_all(const std::filesystem::path& aPath) -> std::string
		{
			std::ifstream input{ aPath, std::ios::binary };
			if (!input.good()) {
				throw std::runtime_error{ "GitHub public-path fixture is unavailable" };
			}
			return {
				std::istreambuf_iterator<char>{ input },
				std::istreambuf_iterator<char>{ },
			};
		}

		class deterministic_git_fixture final
		{
		public:
			// Fixed packs from the documented synthetic three-commit graph keep
			// process ownership in the installed-Git adapter while still exercising
			// strict connected-pack import through the public application. The
			// commit and blob identifiers these packs carry are stated once, by the
			// serving fixture's descriptor, so the served identity has one source.
			deterministic_git_fixture()
				: PackOne{ QByteArray::fromBase64(QByteArrayLiteral(
					  "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")) }
				, PackTwo{ QByteArray::fromBase64(QByteArrayLiteral(
					  "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")) }
			{
				if (PackOne.size() < 32 || PackTwo.size() < 32
					|| !PackOne.startsWith("PACK") || !PackTwo.startsWith("PACK")) {
					throw std::runtime_error{ "Git fixture packs are invalid" };
				}
			}

			/// \brief Serves one recorded pull request's pack from a file.
			///
			/// \details A recorded repository's objects are tens of kilobytes of
			/// binary, which is why they are committed beside the recording
			/// rather than embedded above. The path is relative to the recorded
			/// pull-request fixture root and the same two invariants hold: a
			/// served pack is long enough to have a header and begins with one.
			///
			/// `PackTwo` stays empty because a recorded fixture serves a single
			/// head. That is not a weaker second pack, it is the absence of one:
			/// the descriptor names no second head, so `fixture_smart_transport`
			/// never selects it, and an empty pack is refused there rather than
			/// served.
			explicit deterministic_git_fixture(const std::string_view aPackFile)
				: PackOne{ QByteArray::fromStdString(read_all(
					  std::filesystem::path{
						  BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT }
					  / aPackFile)) }
			{
				if (PackOne.size() < 32 || !PackOne.startsWith("PACK")) {
					throw std::runtime_error{ "Git fixture pack is invalid" };
				}
			}

			QByteArray PackOne;
			QByteArray PackTwo;
		};

		void replace_all(std::string&           aValue,
						 const std::string_view aFrom,
						 const std::string_view aTo)
		{
			auto position = std::size_t{ 0U };
			while ((position = aValue.find(aFrom, position)) != std::string::npos) {
				aValue.replace(position, aFrom.size(), aTo);
				position += aTo.size();
			}
		}

		[[nodiscard]] auto parse_object(const std::filesystem::path& aPath)
			-> QJsonObject
		{
			QJsonParseError error;
			const auto      bytes    = QByteArray::fromStdString(read_all(aPath));
			const auto      document = QJsonDocument::fromJson(bytes, &error);
			if (error.error != QJsonParseError::NoError || !document.isObject()) {
				throw std::runtime_error{ "GitHub public-path fixture is malformed" };
			}
			return document.object();
		}

		[[nodiscard]] auto utf8(const QString& aValue) -> std::string
		{
			return aValue.toUtf8().toStdString();
		}

		/// \brief Says where the packs one fixture serves come from.
		///
		/// \details Two shapes, and a descriptor picks one. The synthetic
		/// three-commit graph is small enough to state in the source that serves
		/// it, and it is the only graph with a second head, so its pair stays
		/// embedded. A recorded pull request names a file instead. An absent
		/// `File` selects the embedded pair; a present one is relative to
		/// `BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT`.
		struct fixture_pack_source
		{
			std::optional<std::string_view> File;
		};

		/// \brief States everything one served fixture is, in one place.
		///
		/// \details The public path used to hardcode a single pull request's
		/// identity across its recording loader, its pack selection, and its
		/// smart-transport authority gate. A fixture that serves a different pull
		/// request cannot satisfy those literals, so they are named here instead
		/// and every site reads them from the descriptor of the fixture it is
		/// serving.
		///
		/// `HeadTwoOid` is the only optional identity: a fixture that serves one
		/// head leaves it empty. An empty value is not a wildcard and must never
		/// match an observed head - see
		/// `fixture_recorded_wire_transport::acquire`.
		struct fixture_descriptor
		{
			std::string_view          ContractDirectory;
			std::string_view          CapabilityDirectory;
			server_contract           Server;
			core::github_account_id   AccountId;
			core::repository_id       RepositoryId;
			core::pull_request_id     PullRequestId;
			core::pull_request_number Number;
			std::string_view          Owner;
			std::string_view          Name;
			std::string_view          BaseRef;
			std::string_view          BaseOid;
			std::string_view          HeadOneOid;
			std::string_view          HeadTwoOid;
			std::string_view          BaseBlob;
			std::string_view          HeadOneBlob;
			std::string_view          HeadTwoBlob;
			fixture_pack_source       Packs;
		};

		/// \brief Selects the descriptor the named fixture serves.
		///
		/// \details A switch rather than a table lookup, so `-Wswitch` makes a new
		/// fixture value state its own identity rather than silently inheriting
		/// the synthetic one these two share.
		[[nodiscard]] constexpr auto descriptor(
			const github_public_path_fixture aFixture) noexcept -> fixture_descriptor
		{
			switch (aFixture) {
			case github_public_path_fixture::github_com:
				return {
					.ContractDirectory   = "github-com",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1001U },
					.RepositoryId        = core::repository_id{ 2001U },
					.PullRequestId       = core::pull_request_id{ 3001U },
					.Number              = core::pull_request_number{ 17U },
					.Owner               = "octo-org",
					.Name                = "review-repository",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "757e8f259231f0cb31b6774c14e6acdbf54b48f3",
					.HeadOneOid          = "f9108913c053354eab2e4771eef27e957a400966",
					.HeadTwoOid          = "6458d96bf0ccddc1ef2fcf60e61c93a28e69d665",
					.BaseBlob            = "7fc38b17790c8f590618f6d1884466dd2b0d42d8",
					.HeadOneBlob         = "88979e3f655e5b605ffbfe52ef40f9ff4d4766a0",
					.HeadTwoBlob         = "6492ffe72d75f6c783b8ea948f3a94732cbdd628",
					.Packs               = { .File = std::nullopt },
				};
			case github_public_path_fixture::ghes_3_18:
				return {
					.ContractDirectory   = "ghes-3.18",
					.CapabilityDirectory = "ghes-3.18",
					.Server              = server_contract::ghes_3_18,
					.AccountId           = core::github_account_id{ 1001U },
					.RepositoryId        = core::repository_id{ 2001U },
					.PullRequestId       = core::pull_request_id{ 3001U },
					.Number              = core::pull_request_number{ 17U },
					.Owner               = "octo-org",
					.Name                = "review-repository",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "757e8f259231f0cb31b6774c14e6acdbf54b48f3",
					.HeadOneOid          = "f9108913c053354eab2e4771eef27e957a400966",
					.HeadTwoOid          = "6458d96bf0ccddc1ef2fcf60e61c93a28e69d665",
					.BaseBlob            = "7fc38b17790c8f590618f6d1884466dd2b0d42d8",
					.HeadOneBlob         = "88979e3f655e5b605ffbfe52ef40f9ff4d4766a0",
					.HeadTwoBlob         = "6492ffe72d75f6c783b8ea948f3a94732cbdd628",
					.Packs               = { .File = std::nullopt },
				};
			// google/UIforETW#74, recorded 2026-08-08 and replayed. The
			// capability directory is `github-com` by reference rather than by
			// copy: this pull request lives on github.com, so the honest
			// capability claim is GitHub.com's own pinned snapshot, and a second
			// copy of it would be a second thing to keep true.
			//
			// The commit identifiers are the ones
			// `scripts/build_recorded_pull_request_pack.py` writes, not the ones
			// GitHub served. Rebuilding GitHub's requires trees, parents and
			// author records the recording does not carry, and the divergence is
			// recorded in the fixture's README rather than hidden.
			//
			// The blob identifiers are empty because nothing this fixture can
			// run reads them: they exist for `request_fixture_diff`, which the
			// diff-workbench and native-appearance probes reach and which both
			// refuse every fixture but `github-com`.
			case github_public_path_fixture::uiforetw_74:
				return {
					.ContractDirectory   = "uiforetw-74",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1U },
					.RepositoryId        = core::repository_id{ 2U },
					.PullRequestId       = core::pull_request_id{ 3U },
					.Number              = core::pull_request_number{ 74U },
					.Owner               = "google",
					.Name                = "UIforETW",
					.BaseRef             = "refs/heads/master",
					.BaseOid             = "8b70ee179fe55b705ee4e613ecbc89a0acebc1bc",
					.HeadOneOid          = "e40058be7d5fe319342dffb567ce30e42bc6f4de",
					.HeadTwoOid          = "",
					.BaseBlob            = "",
					.HeadOneBlob         = "",
					.HeadTwoBlob         = "",
					.Packs               = { .File = "uiforetw-74/objects.pack" },
				};
			}
			return { };
		}

		/// \brief Builds the pack pair one descriptor asks for.
		[[nodiscard]] auto make_git_fixture(const fixture_pack_source aSource)
			-> std::shared_ptr<deterministic_git_fixture>
		{
			return aSource.File.has_value()
				? std::make_shared<deterministic_git_fixture>(*aSource.File)
				: std::make_shared<deterministic_git_fixture>();
		}

		[[nodiscard]] auto response_headers()
			-> std::vector<http_header>
		{
			return {
				{
					.Name  = "Content-Type",
					.Value = "application/json",
				},
			};
		}

		void append_rate_limit_headers(wire_response&       aResponse,
									   const read_operation aOperation)
		{
			auto resource  = std::string{ "core" };
			auto limit     = std::string{ "5000" };
			auto remaining = std::string{ "4999" };
			if (aOperation == read_operation::search_pull_requests) {
				resource  = "search";
				limit     = "30";
				remaining = "29";
			} else if (aOperation == read_operation::list_review_threads) {
				resource = "graphql";
			}
			aResponse.Headers.push_back(
				{ .Name = "X-RateLimit-Limit", .Value = std::move(limit) });
			aResponse.Headers.push_back(
				{ .Name = "X-RateLimit-Remaining", .Value = std::move(remaining) });
			aResponse.Headers.push_back(
				{ .Name = "X-RateLimit-Reset", .Value = "2000000000" });
			aResponse.Headers.push_back(
				{ .Name = "X-RateLimit-Resource", .Value = std::move(resource) });
		}

		[[nodiscard]] auto load_recording(
			const fixture_descriptor& aDescriptor,
			const std::string_view    aHead)
			-> std::vector<recorded_exchange>
		{
			const auto root = std::filesystem::path{ BPR_GITHUB_CONTRACT_FIXTURE_ROOT }
				/ aDescriptor.ContractDirectory;
			const auto manifest     = parse_object(root / "manifest.json");
			const auto interactions = manifest.value("interactions").toArray();
			if (interactions.size()
				!= static_cast<qsizetype>(read_operation_catalog.size())) {
				throw std::runtime_error{ "GitHub public-path fixture is incomplete" };
			}

			auto exchanges = std::vector<recorded_exchange>{ };
			exchanges.reserve(read_operation_catalog.size());
			for (qsizetype index = 0; index < interactions.size(); ++index) {
				const auto interaction = interactions[index].toObject();
				const auto operation   = read_operation_from_name(
					utf8(interaction.value("operation").toString()));
				if (!operation.has_value()
					|| *operation
						!= read_operation_catalog[static_cast<std::size_t>(index)]) {
					throw std::runtime_error{
						"GitHub public-path fixture catalog is invalid"
					};
				}

				auto       headers     = std::vector<http_header>{ };
				const auto safeHeaders = interaction.value("safe_headers").toObject();
				headers.reserve(static_cast<std::size_t>(safeHeaders.size()));
				for (auto item = safeHeaders.constBegin();
					 item != safeHeaders.constEnd();
					 ++item) {
					headers.push_back(
						{
							.Name  = utf8(item.key()),
							.Value = utf8(item.value().toString()),
						});
				}

				const auto responseFile = root
					/ utf8(interaction.value("response_file").toString());
				const auto body   = interaction.value("body");
				const auto method = parse_recorded_http_method(
					utf8(interaction.value("method").toString()));
				if (!method.has_value()) {
					throw std::runtime_error{
						"GitHub public-path fixture method is invalid"
					};
				}
				auto requestPath  = utf8(interaction.value("path").toString());
				auto requestQuery = utf8(interaction.value("canonical_query").toString());
				auto requestBody  = body.isNull()
					? std::string{ }
					: utf8(body.toString());
				replace_all(requestPath,
							"2222222222222222222222222222222222222222",
							aHead);
				replace_all(requestQuery,
							"2222222222222222222222222222222222222222",
							aHead);
				replace_all(requestBody,
							"2222222222222222222222222222222222222222",
							aHead);
				auto responseBody = read_all(responseFile);
				replace_all(responseBody,
							"1111111111111111111111111111111111111111",
							aDescriptor.BaseOid);
				const auto observedHead = *operation == read_operation::list_reviews
					? aDescriptor.HeadOneOid
					: aHead;
				replace_all(responseBody,
							"2222222222222222222222222222222222222222",
							observedHead);
				exchanges.push_back(
					{
						.Request = {
							.Semantic  = { },
							.Server    = aDescriptor.Server,
							.Operation = *operation,
							.Method    = *method,
							.Path      = std::move(requestPath),
							.Query     = std::move(requestQuery),
							.Headers   = std::move(headers),
							.Body      = std::move(requestBody),
						},
						.Response = {
							.Status       = 200,
							.Headers      = response_headers(),
							.Body         = std::move(responseBody),
							.Completeness = page_completeness::complete,
						},
					});
			}
			return exchanges;
		}

		[[nodiscard]] auto load_capability_response(
			const std::string_view aCapabilityDirectory) -> std::string
		{
			return read_all(
				std::filesystem::path{ BPR_GITHUB_CAPABILITY_FIXTURE_ROOT }
				/ aCapabilityDirectory / "supported.json");
		}

		[[nodiscard]] auto same_profile(
			const adapters::github::validated_github_host_profile& aLeft,
			const adapters::github::validated_github_host_profile& aRight) noexcept
			-> bool
		{
			return aLeft.profile() == aRight.profile() && aLeft.host() == aRight.host()
				&& aLeft.revision() == aRight.revision()
				&& aLeft.server_contract() == aRight.server_contract()
				&& aLeft.authentication_transport()
				== aRight.authentication_transport()
				&& aLeft.proxy_mode() == aRight.proxy_mode()
				&& aLeft.trust_mode() == aRight.trust_mode()
				&& aLeft.provenance() == aRight.provenance()
				&& aLeft.hostname() == aRight.hostname()
				&& aLeft.rest_root() == aRight.rest_root()
				&& aLeft.graphql_root() == aRight.graphql_root();
		}

		[[nodiscard]] auto load_bootstrap_recording(
			const fixture_descriptor& aDescriptor)
			-> std::vector<recorded_exchange>
		{
			auto exchanges = load_recording(aDescriptor, aDescriptor.HeadOneOid);
			exchanges.resize(1U);
			return exchanges;
		}

		struct fixture_recording_state
		{
			fixture_recording_state(
				const fixture_descriptor&                              aDescriptor,
				const adapters::github::validated_github_host_profile& aSelectedProfile,
				std::shared_ptr<deterministic_git_fixture>             aGit,
				const github_epoch_fixture_phase                       aPhase)
				: Descriptor{ aDescriptor }
				, Profile{ aSelectedProfile }
				, Git{ std::move(aGit) }
				, BootstrapRecording{ load_bootstrap_recording(Descriptor) }
				, Recording{ [&] {
					auto result = load_recording(
						Descriptor,
						aPhase == github_epoch_fixture_phase::reuse
							? Descriptor.HeadTwoOid
							: Descriptor.HeadOneOid);
					if (aPhase == github_epoch_fixture_phase::seed) {
						auto second = load_recording(
							Descriptor, Descriptor.HeadTwoOid);
						if (second.empty()) {
							throw std::runtime_error{
								"GitHub fixture refresh recording is empty"
							};
						}
						second.erase(second.begin());
						result.insert(result.end(),
									  std::make_move_iterator(
										  second.begin()),
									  std::make_move_iterator(second.end()));
					}
					return result;
				}() }
				, CapabilityBody{ load_capability_response(
					  Descriptor.CapabilityDirectory) }
			{
			}

			// Fixed at construction and read from the transport threads without the
			// mutex, which the const keeps true.
			const fixture_descriptor                        Descriptor;
			adapters::github::validated_github_host_profile Profile;
			std::shared_ptr<deterministic_git_fixture>      Git;
			strict_recorded_transport                       BootstrapRecording;
			strict_recorded_transport                       Recording;
			std::string                                     CapabilityBody;
			std::size_t                                     CapabilityCalls       = 0U;
			std::size_t                                     BootstrapFactoryCalls = 0U;
			std::size_t                                     AccountFactoryCalls   = 0U;
			std::size_t                                     AccountReadCalls      = 0U;
			std::size_t                                     SmartAcquireCalls     = 0U;
			std::size_t                                     SmartDiscoveryCalls   = 0U;
			std::size_t                                     SmartLsRefsCalls      = 0U;
			std::size_t                                     SmartFetchCalls       = 0U;
			std::optional<core::pull_request_authority>     LastSmartAuthority;
			int                                             LastReadOperation = -1;
			int                                             RecordingFailure  = -1;
			mutable std::mutex                              Mutex;
		};

		enum class fixture_transport_role {
			bootstrap,
			account,
		};

		[[nodiscard]] auto byte_vector(const std::string_view aValue)
			-> std::vector<std::byte>
		{
			return {
				reinterpret_cast<const std::byte*>(aValue.data()),
				reinterpret_cast<const std::byte*>(aValue.data() + aValue.size()),
			};
		}

		void append_packet(std::vector<std::byte>&          aOutput,
						   const std::span<const std::byte> aPayload)
		{
			constexpr std::string_view digits = "0123456789abcdef";
			const auto                 total  = aPayload.size() + 4U;
			if (total > 0xffffU) {
				throw std::runtime_error{ "Git fixture packet is too large" };
			}
			auto header    = std::array<char, 4U>{ };
			auto remaining = total;
			for (auto index = header.size(); index-- > 0U;) {
				header[index] = digits[remaining & 0x0fU];
				remaining >>= 4U;
			}
			const auto headerBytes = std::as_bytes(std::span{ header });
			aOutput.insert(aOutput.end(), headerBytes.begin(), headerBytes.end());
			aOutput.insert(aOutput.end(), aPayload.begin(), aPayload.end());
		}

		void append_packet(std::vector<std::byte>& aOutput,
						   const std::string_view  aPayload)
		{
			append_packet(aOutput, std::as_bytes(std::span{ aPayload }));
		}

		void append_flush(std::vector<std::byte>& aOutput)
		{
			const auto flush = byte_vector("0000");
			aOutput.insert(aOutput.end(), flush.begin(), flush.end());
		}

		[[nodiscard]] auto discovery_body() -> std::vector<std::byte>
		{
			auto result = std::vector<std::byte>{ };
			append_packet(result, "version 2\n");
			append_packet(result, "ls-refs=unborn\n");
			append_packet(result, "fetch=shallow wait-for-done\n");
			append_packet(result, "object-format=sha1\n");
			append_flush(result);
			return result;
		}

		[[nodiscard]] auto refs_body(
			const application::git_smart_request_context& aContext)
			-> std::vector<std::byte>
		{
			auto result = std::vector<std::byte>{ };
			append_packet(
				result,
				std::string{ core::canonical_hex(aContext.Revision.Base).view() }
					+ " " + std::string{ aContext.BaseRef.view() } + "\n");
			append_packet(
				result,
				std::string{ core::canonical_hex(aContext.Revision.Head).view() }
					+ " refs/pull/" + std::to_string(aContext.Authority.Number.value())
					+ "/head\n");
			append_flush(result);
			return result;
		}

		[[nodiscard]] auto fetch_body(const QByteArray& aPack)
			-> std::vector<std::byte>
		{
			auto result = std::vector<std::byte>{ };
			append_packet(result, "packfile\n");
			constexpr auto maximumChunk = std::size_t{ 60U * 1024U };
			const auto     packBytes    = std::span{
				reinterpret_cast<const std::byte*>(aPack.constData()),
				static_cast<std::size_t>(aPack.size())
			};
			for (auto offset = std::size_t{ 0U }; offset < packBytes.size();) {
				const auto size    = std::min(maximumChunk, packBytes.size() - offset);
				auto       payload = std::vector<std::byte>{ };
				payload.reserve(size + 1U);
				payload.push_back(std::byte{ 1U });
				payload.insert(payload.end(),
							   packBytes.begin() + static_cast<std::ptrdiff_t>(offset),
							   packBytes.begin()
								   + static_cast<std::ptrdiff_t>(offset + size));
				append_packet(result, payload);
				offset += size;
			}
			append_flush(result);
			return result;
		}

		class fixture_smart_transport final
			: public application::git_smart_transport_port
		{
		public:
			fixture_smart_transport(
				std::shared_ptr<fixture_recording_state> aState,
				application::git_smart_request_context   aContext)
				: State{ std::move(aState) }
				, Context{ std::move(aContext) }
			{
			}

			void revoke() noexcept override { Revoked.store(true); }

			[[nodiscard]] auto revoked() const noexcept -> bool override
			{
				return Revoked.load();
			}

			[[nodiscard]] auto discover(
				const application::git_smart_discovery_request& aRequest,
				const std::stop_token                           aStop)
				-> application::git_smart_transport_result<
					application::git_smart_discovery_response> override
			{
				if (aStop.stop_requested() || revoked()) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::cancelled } };
				}
				if (aRequest.Context != Context) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::
							invalid_request } };
				}
				{
					const std::scoped_lock lock{ State->Mutex };
					++State->SmartDiscoveryCalls;
				}
				return application::git_smart_discovery_response{
					.Body = discovery_body()
				};
			}

			[[nodiscard]] auto ls_refs(
				const application::git_smart_ls_refs_request& aRequest,
				const std::stop_token                         aStop)
				-> application::git_smart_transport_result<
					application::git_smart_ls_refs_response> override
			{
				if (aStop.stop_requested() || revoked()) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::cancelled } };
				}
				if (aRequest.Context != Context) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::
							invalid_request } };
				}
				{
					const std::scoped_lock lock{ State->Mutex };
					++State->SmartLsRefsCalls;
				}
				return application::git_smart_ls_refs_response{
					.Body = refs_body(Context)
				};
			}

			[[nodiscard]] auto fetch(
				const application::git_smart_fetch_request& aRequest,
				application::git_smart_wire_consumer&       aConsumer,
				const std::stop_token                       aStop)
				-> application::git_smart_transport_result<void> override
			{
				if (aStop.stop_requested() || revoked()) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::cancelled } };
				}
				if (aRequest.Context != Context) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::
							invalid_request } };
				}
				const auto head = std::string{
					core::canonical_hex(Context.Revision.Head).view()
				};
				const auto& fixture = State->Descriptor;
				const auto& pack    = head == fixture.HeadOneOid
					? State->Git->PackOne
					: (!fixture.HeadTwoOid.empty() && head == fixture.HeadTwoOid)
					? State->Git->PackTwo
					: QByteArray{ };
				if (pack.isEmpty()) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::
							invalid_request } };
				}
				const auto body = fetch_body(pack);
				if (!aConsumer.consume(body)) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::
							consumer_rejected } };
				}
				{
					const std::scoped_lock lock{ State->Mutex };
					++State->SmartFetchCalls;
				}
				return { };
			}

		private:
			std::shared_ptr<fixture_recording_state> State;
			application::git_smart_request_context   Context;
			std::atomic_bool                         Revoked = false;
		};

		class fixture_recorded_wire_transport final
			: public github_wire_transport_port,
			  public application::git_smart_transport_factory_port
		{
		public:
			explicit fixture_recorded_wire_transport(
				std::shared_ptr<fixture_recording_state> aState,
				const fixture_transport_role             aRole)
				: State{ std::move(aState) }
				, Role{ aRole }
			{
			}

			[[nodiscard]] auto perform(
				const github_wire_request& aRequest,
				const std::stop_token      aStop) -> github_wire_completion override
			{
				if (aStop.stop_requested()) {
					return github_wire_cancelled{ .Correlation = aRequest.Correlation };
				}
				const std::scoped_lock lock{ State->Mutex };
				if (aRequest.Correlation.ProfileRevision != State->Profile.revision()
					|| aRequest.Correlation.Host != State->Profile.host()) {
					State->RecordingFailure = -2;
					return github_wire_error{
						.Correlation = aRequest.Correlation,
						.Error       = provider_error(
							application::github_provider_failure::malformed_response),
					};
				}
				auto& recording = Role == fixture_transport_role::bootstrap
					? State->BootstrapRecording
					: State->Recording;
				auto  response  = recording.perform(aRequest.Wire);
				if (!response.has_value()) {
					State->RecordingFailure = static_cast<int>(response.error().Code);
					return github_wire_error{
						.Correlation = aRequest.Correlation,
						.Error       = provider_error(
							application::github_provider_failure::malformed_response),
					};
				}
				if (Role == fixture_transport_role::account) {
					++State->AccountReadCalls;
					State->LastReadOperation = static_cast<int>(aRequest.Wire.Operation);
				}
				append_rate_limit_headers(*response, aRequest.Wire.Operation);
				return github_wire_success{
					.Correlation = aRequest.Correlation,
					.Response    = std::move(*response),
				};
			}

			[[nodiscard]] auto perform_capability_probe(
				const github_capability_probe_wire_request& aRequest,
				const std::stop_token                       aStop)
				-> github_capability_probe_wire_completion override
			{
				if (aStop.stop_requested()) {
					return github_capability_probe_wire_cancelled{
						.Correlation = aRequest.Correlation,
					};
				}
				const std::scoped_lock lock{ State->Mutex };
				const auto             expected = adapters::github::detail::compile_capability_probe_request(
					State->Profile);
				if (!expected.has_value() || aRequest.Wire != *expected
					|| aRequest.Correlation.ProfileRevision
						!= State->Profile.revision()
					|| aRequest.Correlation.Host != State->Profile.host()
					|| aRequest.Correlation.Session.Profile
						!= State->Profile.profile()
					|| aRequest.Correlation.ObservedAccount.value() == 0U
					|| State->CapabilityCalls != 0U) {
					return github_capability_probe_wire_error{
						.Correlation = aRequest.Correlation,
						.Error       = provider_error(
							application::github_provider_failure::malformed_response),
					};
				}
				++State->CapabilityCalls;
				auto response = capability_probe_response{
					.Status               = 200,
					.Authenticated        = true,
					.ContentType          = "application/json",
					.Body                 = State->CapabilityBody,
					.RateLimitHeaders     = { },
					.RateLimitHeaderCount = 4U,
				};
				response.RateLimitHeaders[0U] = {
					.Name  = "X-RateLimit-Limit",
					.Value = "5000",
				};
				response.RateLimitHeaders[1U] = {
					.Name  = "X-RateLimit-Remaining",
					.Value = "4999",
				};
				response.RateLimitHeaders[2U] = {
					.Name  = "X-RateLimit-Reset",
					.Value = "2000000000",
				};
				response.RateLimitHeaders[3U] = {
					.Name  = "X-RateLimit-Resource",
					.Value = "graphql",
				};
				return github_capability_probe_wire_success{
					.Correlation = aRequest.Correlation,
					.Response    = std::move(response),
				};
			}

			[[nodiscard]] auto acquire(
				const application::git_smart_request_context& aContext)
				-> application::git_smart_transport_result<
					application::git_smart_transport> override
			{
				const auto base = std::string{ core::canonical_hex(aContext.Revision.Base).view() };
				const auto head = std::string{ core::canonical_hex(aContext.Revision.Head).view() };
				// Every field below is compared exactly against the descriptor of the
				// one fixture this transport serves. Nothing here is a wildcard.
				const auto& fixture = State->Descriptor;
				// The second head is the one optional identity, and absence is not
				// permission: an empty `HeadTwoOid` describes a fixture that serves a
				// single head, so it is matched only when present, and then exactly.
				const auto acceptedHead = head == fixture.HeadOneOid
					|| (!fixture.HeadTwoOid.empty() && head == fixture.HeadTwoOid);
				if (Role != fixture_transport_role::account
					|| !application::valid(aContext)
					|| aContext.Authority.Account != fixture.AccountId
					|| aContext.Authority.Repository != fixture.RepositoryId
					|| aContext.Authority.PullRequest != fixture.PullRequestId
					|| aContext.Authority.Number != fixture.Number
					|| aContext.Repository
						!= core::repository_locator{
							.Owner = std::string{ fixture.Owner },
							.Name  = std::string{ fixture.Name },
						}
					|| aContext.BaseRef.view() != fixture.BaseRef || base != fixture.BaseOid || !acceptedHead) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::
							invalid_request } };
				}
				{
					const std::scoped_lock lock{ State->Mutex };
					++State->SmartAcquireCalls;
					State->LastSmartAuthority = aContext.Authority;
				}
				return application::git_smart_transport{
					new fixture_smart_transport{ State, aContext }
				};
			}

		private:
			std::shared_ptr<fixture_recording_state> State;
			fixture_transport_role                   Role;
		};

		class fixture_transport_factory final
			: public github_wire_transport_factory_port
		{
		public:
			explicit fixture_transport_factory(
				std::shared_ptr<fixture_recording_state> aState)
				: State{ std::move(aState) }
				, BootstrapTransport{
					std::make_shared<fixture_recorded_wire_transport>(
						State, fixture_transport_role::bootstrap)
				}
				, AccountTransport{ std::make_shared<fixture_recorded_wire_transport>(State, fixture_transport_role::account) }
			{
			}

			[[nodiscard]] auto make_bootstrap(
				const adapters::github::validated_github_host_profile& aProfile,
				const core::github_session_request_context             aContext,
				application::github_credential_material                aCredential) noexcept
				-> github_wire_transport_factory_result override
			{
				const std::scoped_lock lock{ State->Mutex };
				if (!same_profile(aProfile, State->Profile)
					|| aContext.Profile != aProfile.profile()
					|| !application::valid(aCredential)
					|| State->BootstrapFactoryCalls != 0U) {
					return std::unexpected{ provider_error(
						application::github_provider_failure::unavailable) };
				}
				++State->BootstrapFactoryCalls;
				return BootstrapTransport;
			}

			[[nodiscard]] auto make_account(
				const adapters::github::validated_github_host_profile& aProfile,
				application::github_credential_retrieve_success        aCredential) noexcept
				-> github_wire_transport_factory_result override
			{
				const std::scoped_lock lock{ State->Mutex };
				if (!same_profile(aProfile, State->Profile)
					|| !application::valid(aCredential.Credential)
					|| aCredential.Context.Authority.Profile != aProfile.profile()
					|| aCredential.Context.Authority.ProfileRevision
						!= aProfile.revision()
					|| aCredential.Context.Authority.Host != aProfile.host()
					|| aCredential.Context.Authority.Hostname.value()
						!= aProfile.hostname()
					|| aCredential.Context.Authority.ServerContract
						!= aProfile.server_contract()
					|| State->AccountFactoryCalls != 0U) {
					return std::unexpected{ provider_error(
						application::github_provider_failure::unavailable) };
				}
				++State->AccountFactoryCalls;
				return AccountTransport;
			}

		private:
			std::shared_ptr<fixture_recording_state>         State;
			std::shared_ptr<fixture_recorded_wire_transport> BootstrapTransport;
			std::shared_ptr<fixture_recorded_wire_transport> AccountTransport;
		};

		struct fixture_vault_state
		{
			std::optional<application::github_credential_operation_context> Context;
			std::optional<application::github_credential_material>          Credential;
			bool                                                            Stored    = false;
			bool                                                            Retrieved = false;
			bool                                                            Deleted   = false;
			mutable std::mutex                                              Mutex;
		};

		class fixture_credential_vault final
			: public application::github_credential_vault_port
		{
		public:
			fixture_credential_vault(
				application::effect_executor&        aEffects,
				std::shared_ptr<fixture_vault_state> aState)
				: Effects{ aEffects }
				, State{ std::move(aState) }
			{
			}

			void store(
				application::github_credential_enrollment               aEnrollment,
				application::github_credential_store_completion_handler aCompletion)
				override
			{
				auto pending = std::make_shared<
					application::github_credential_enrollment>(
					std::move(aEnrollment));
				Effects.post(
					[state      = State,
					 pending    = std::move(pending),
					 completion = std::move(aCompletion)]() mutable {
						if (!application::valid(*pending)) {
							completion(application::github_credential_store_error{
								.Context = pending->Context,
								.Failure = application::
									github_credential_vault_failure::invalid_request,
							});
							return;
						}
						const auto context = pending->Context;
						{
							const std::scoped_lock lock{ state->Mutex };
							state->Context    = context;
							state->Credential = std::move(pending->Credential);
							state->Stored     = true;
						}
						completion(application::github_credential_store_success{
							.Context     = context,
							.Disposition = application::
								github_credential_store_disposition::stored,
						});
					});
			}

			void retrieve(
				const application::github_credential_operation_context     aContext,
				application::github_credential_retrieve_completion_handler aCompletion)
				override
			{
				Effects.post(
					[state = State, aContext, completion = std::move(aCompletion)]() mutable {
						auto material = std::optional<
							application::github_credential_material>{ };
						{
							const std::scoped_lock lock{ state->Mutex };
							if (state->Context == aContext
								&& state->Credential.has_value()) {
								auto copy = application::github_credential_material::create(
									state->Credential->bytes());
								if (copy.has_value()) {
									material.emplace(std::move(*copy));
									state->Retrieved = true;
								}
							}
						}
						if (!material.has_value()) {
							completion(application::github_credential_retrieve_error{
								.Context = aContext,
								.Failure = application::
									github_credential_vault_failure::operation_failed,
							});
							return;
						}
						completion(application::github_credential_retrieve_success{
							.Context    = aContext,
							.Credential = std::move(*material),
						});
					});
			}

			void delete_credential(
				const application::github_credential_operation_context   aContext,
				application::github_credential_delete_completion_handler aCompletion)
				override
			{
				Effects.post(
					[state = State, aContext, completion = std::move(aCompletion)]() mutable {
						auto disposition  = application::github_credential_delete_disposition::absent;
						auto exactContext = true;
						{
							const std::scoped_lock lock{ state->Mutex };
							if (state->Context.has_value()
								&& state->Context != aContext) {
								exactContext = false;
							} else {
								if (state->Credential.has_value()) {
									state->Credential.reset();
									disposition = application::
										github_credential_delete_disposition::deleted;
								}
								state->Context.reset();
								state->Deleted = true;
							}
						}
						if (!exactContext) {
							completion(application::github_credential_delete_error{
								.Context = aContext,
								.Failure = application::
									github_credential_vault_failure::invalid_request,
							});
							return;
						}
						completion(application::github_credential_delete_success{
							.Context     = aContext,
							.Disposition = disposition,
						});
					});
			}

			void cancel(
				const application::github_credential_operation_context) override
			{
				// Cancellation is advisory. Every accepted fixture task still emits
				// its one terminal so shutdown can establish exact cleanup.
			}

		private:
			application::effect_executor&        Effects;
			std::shared_ptr<fixture_vault_state> State;
		};

		[[nodiscard]] auto make_fixture_credential()
			-> application::github_credential_material
		{
			auto               bytes = std::array<std::byte, 32>{ };
			std::random_device random;
			for (auto& byte : bytes) {
				byte = static_cast<std::byte>(random() & 0xffU);
			}
			auto material = application::github_credential_material::create(
				std::span<const std::byte>{ bytes });
			std::ranges::fill(bytes, std::byte{ });
			if (!material.has_value()) {
				throw std::runtime_error{
					"GitHub public-path fixture credential allocation failed"
				};
			}
			return std::move(*material);
		}

		class fixture_review_mutation_provider final
			: public application::review_mutation_port
		{
		public:
			explicit fixture_review_mutation_provider(const bool aLoseFirst) noexcept
				: LoseFirst{ aLoseFirst }
			{
			}

			void prepare(core::review_mutation_target aTarget)
			{
				Designated = std::move(aTarget);
				Reported   = Designated;
			}

			void report(core::review_mutation_target aTarget)
			{
				Reported = std::move(aTarget);
			}

			auto observe_target() -> std::expected<
				core::review_mutation_target,
				application::review_mutation_failure> override
			{
				++Evidence.TargetObservations;
				return Reported;
			}

			auto submit(const application::review_mutation_request& aRequest)
				-> std::expected<
					application::review_mutation_receipt,
					application::review_mutation_failure> override
			{
				++Evidence.Submissions;
				if (aRequest.Target != Designated) {
					return std::unexpected{
						application::review_mutation_failure::undesignated_target
					};
				}
				const auto known = Identifiers.find(aRequest.IdempotencyKey);
				if (known != Identifiers.end()) {
					Evidence.DuplicateReturned = true;
					return application::review_mutation_receipt{
						.ServerCommentId = known->second,
						.Duplicate       = true,
					};
				}
				auto identifier = "fixture-comment-"
					+ std::to_string(Identifiers.size() + 1U);
				Identifiers.emplace(aRequest.IdempotencyKey, identifier);
				Comments.push_back(application::review_comment_projection{
					.ServerId    = identifier,
					.Body        = aRequest.Proposal.Body,
					.EditVersion = 1U,
					.Resolved    = false,
				});
				Evidence.StoredComments = Comments.size();
				if (LoseFirst) {
					LoseFirst           = false;
					Evidence.LostResult = true;
					return std::unexpected{
						application::review_mutation_failure::unavailable
					};
				}
				return application::review_mutation_receipt{
					.ServerCommentId = std::move(identifier),
					.Duplicate       = false,
				};
			}

			[[nodiscard]] auto capabilities() const
				-> application::review_provider_capabilities override
			{
				return {
					.ResolvesThreads    = false,
					.RecordsViewedState = false,
				};
			}

			[[nodiscard]] auto comments() const
				-> std::vector<application::review_comment_projection>
			{
				return Comments;
			}

			[[nodiscard]] auto evidence() const noexcept
				-> review_mutation_fixture_evidence
			{
				return Evidence;
			}

		private:
			core::review_mutation_target                        Designated;
			core::review_mutation_target                        Reported;
			std::map<std::string, std::string>                  Identifiers;
			std::vector<application::review_comment_projection> Comments;
			review_mutation_fixture_evidence                    Evidence;
			bool                                                LoseFirst = false;
		};
	}

	auto github_public_path_fixture_serves_second_head(
		const github_public_path_fixture aFixture) noexcept -> bool
	{
		return !descriptor(aFixture).HeadTwoOid.empty();
	}

	class github_public_path_fixture_components::implementation final
	{
	public:
		implementation(
			const github_public_path_fixture                       aFixture,
			const adapters::github::validated_github_host_profile& aProfile,
			application::effect_executor&                          aVaultEffects,
			github_public_path_fixture_options                     aOptions)
		{
			if (aOptions.GitExecutable.has_value()) {
				GitExecutable = aOptions.GitExecutable->lexically_normal();
			} else {
				GitExecutable = QFileInfo{
					QStandardPaths::findExecutable(QStringLiteral("git"))
				}
									.canonicalFilePath()
									.toStdString();
			}
			if (GitExecutable.empty()) {
				throw std::runtime_error{
					"GitHub public-path installed Git is unavailable"
				};
			}
			const auto served = descriptor(aFixture);
			Git               = make_git_fixture(served.Packs);
			Recording         = std::make_shared<fixture_recording_state>(
				served, aProfile, Git, aOptions.EpochPhase);
			Factory    = std::make_shared<fixture_transport_factory>(Recording);
			VaultState = std::make_shared<fixture_vault_state>();
			Vault      = std::make_unique<fixture_credential_vault>(
				aVaultEffects, VaultState);
			Mutation = std::make_unique<fixture_review_mutation_provider>(
				aOptions.LoseFirstReviewMutationResult);

			if (aOptions.ApplicationDataRoot) {
				ApplicationDataRoot = aOptions.ApplicationDataRoot->lexically_normal();
				std::error_code error;
				static_cast<void>(std::filesystem::create_directories(
					ApplicationDataRoot, error));
				std::filesystem::permissions(
					ApplicationDataRoot,
					std::filesystem::perms::owner_all,
					std::filesystem::perm_options::replace,
					error);
				if (error) {
					throw std::runtime_error{
						"GitHub public-path application data is unavailable"
					};
				}
				ApplicationDataRoot = std::filesystem::canonical(ApplicationDataRoot, error);
				if (error) {
					throw std::runtime_error{
						"GitHub public-path application data is unavailable"
					};
				}
				return;
			}

			// Absolute, because a relative template resolves against the
			// process working directory: launched from the repository root,
			// this fixture wrote its scratch state into the repository. A
			// clean run auto-removes it and hid that, so what survived was
			// exactly the crash runs.
			ApplicationData = std::make_unique<QTemporaryDir>(
				QDir::temp().filePath(QStringLiteral("bpr-public-path-XXXXXX")));
			if (!ApplicationData->isValid()) {
				throw std::runtime_error{
					"GitHub public-path application data is unavailable"
				};
			}
			ApplicationDataRoot = QFileInfo{ ApplicationData->path() }.canonicalFilePath().toStdString();
			if (ApplicationDataRoot.empty()) {
				throw std::runtime_error{
					"GitHub public-path installed Git is unavailable"
				};
			}
		}

		std::shared_ptr<fixture_recording_state>          Recording;
		std::shared_ptr<deterministic_git_fixture>        Git;
		std::shared_ptr<fixture_transport_factory>        Factory;
		std::shared_ptr<fixture_vault_state>              VaultState;
		std::unique_ptr<fixture_credential_vault>         Vault;
		std::unique_ptr<fixture_review_mutation_provider> Mutation;
		std::unique_ptr<QTemporaryDir>                    ApplicationData;
		std::filesystem::path                             ApplicationDataRoot;
		std::filesystem::path                             GitExecutable;
	};

	github_public_path_fixture_components::
		github_public_path_fixture_components(
			const github_public_path_fixture                       aFixture,
			const adapters::github::validated_github_host_profile& aProfile,
			application::effect_executor&                          aVaultEffects,
			github_public_path_fixture_options                     aOptions)
		: Implementation{ std::make_unique<implementation>(
			  aFixture, aProfile, aVaultEffects, std::move(aOptions)) }
	{
	}

	github_public_path_fixture_components::
		~github_public_path_fixture_components() = default;

	auto github_public_path_fixture_components::transport_factory() const noexcept
		-> std::shared_ptr<github_wire_transport_factory_port>
	{
		return Implementation->Factory;
	}

	auto github_public_path_fixture_components::credential_vault() const noexcept
		-> application::github_credential_vault_port&
	{
		return *Implementation->Vault;
	}

	auto github_public_path_fixture_components::take_enrollment_credential()
		-> application::github_credential_material
	{
		return make_fixture_credential();
	}

	auto github_public_path_fixture_components::evidence() const noexcept
		-> github_public_path_fixture_evidence
	{
		auto result = github_public_path_fixture_evidence{ };
		{
			const std::scoped_lock lock{ Implementation->Recording->Mutex };
			result.RecordingConsumed = Implementation->Recording->BootstrapRecording.consumed_all()
				&& Implementation->Recording->Recording.consumed_all();
			result.CapabilityProbeConsumed    = Implementation->Recording->CapabilityCalls == 1U;
			result.BootstrapTransportSelected = Implementation->Recording->BootstrapFactoryCalls == 1U;
			result.AccountTransportSelected   = Implementation->Recording->AccountFactoryCalls == 1U;
			result.RecordingFailure           = Implementation->Recording->RecordingFailure;
			result.AccountReadCount           = static_cast<int>(
				Implementation->Recording->AccountReadCalls);
			result.LastReadOperation = Implementation->Recording->LastReadOperation;
			result.SmartAcquireCount = static_cast<int>(
				Implementation->Recording->SmartAcquireCalls);
			result.SmartDiscoveryCount = static_cast<int>(
				Implementation->Recording->SmartDiscoveryCalls);
			result.SmartLsRefsCount = static_cast<int>(
				Implementation->Recording->SmartLsRefsCalls);
			result.SmartFetchCount = static_cast<int>(
				Implementation->Recording->SmartFetchCalls);
			const auto& fixture   = Implementation->Recording->Descriptor;
			result.BaseOid        = fixture.BaseOid;
			result.HeadOneOid     = fixture.HeadOneOid;
			result.HeadTwoOid     = fixture.HeadTwoOid;
			result.BaseBlobOid    = fixture.BaseBlob;
			result.HeadOneBlobOid = fixture.HeadOneBlob;
			result.HeadTwoBlobOid = fixture.HeadTwoBlob;
		}
		{
			const std::scoped_lock lock{ Implementation->VaultState->Mutex };
			result.CredentialStored    = Implementation->VaultState->Stored;
			result.CredentialRetrieved = Implementation->VaultState->Retrieved;
			result.CredentialDeleted   = Implementation->VaultState->Deleted;
			result.SecretRetained      = Implementation->VaultState->Credential.has_value();
		}
		return result;
	}

	auto github_public_path_fixture_components::last_smart_authority() const
		noexcept -> std::optional<core::pull_request_authority>
	{
		const std::scoped_lock lock{ Implementation->Recording->Mutex };
		return Implementation->Recording->LastSmartAuthority;
	}

	auto github_public_path_fixture_components::application_data_root() const
		-> const std::filesystem::path&
	{
		return Implementation->ApplicationDataRoot;
	}

	auto github_public_path_fixture_components::git_executable() const
		-> const std::filesystem::path&
	{
		return Implementation->GitExecutable;
	}

	void github_public_path_fixture_components::prepare_review_mutation(
		core::review_mutation_target aTarget)
	{
		Implementation->Mutation->prepare(std::move(aTarget));
	}

	void github_public_path_fixture_components::report_review_mutation_target(
		core::review_mutation_target aTarget)
	{
		Implementation->Mutation->report(std::move(aTarget));
	}

	auto github_public_path_fixture_components::review_mutation_provider() noexcept
		-> application::review_mutation_port&
	{
		return *Implementation->Mutation;
	}

	auto github_public_path_fixture_components::review_comments() const
		-> std::vector<application::review_comment_projection>
	{
		return Implementation->Mutation->comments();
	}

	auto github_public_path_fixture_components::review_mutation_evidence() const
		noexcept -> review_mutation_fixture_evidence
	{
		return Implementation->Mutation->evidence();
	}
}
