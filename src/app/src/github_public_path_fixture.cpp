#include "github_public_path_fixture.hpp"

#include "github_capability_probe.hpp"
#include "github_read_contract.hpp"
#include "github_wire_transport.hpp"

#include "bpr/application/git_smart_transport_port.hpp"

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
			// strict connected-pack import through the public application.
			deterministic_git_fixture()
				: PackOne{ QByteArray::fromBase64(QByteArrayLiteral(
					  "UEFDSwAAAAIAAAAGmxF4nK3MTQrCMBAF4H1Okb0gSSeZKIiIC9fiDWaSCQZqW0JaPb5/eAPf6i2+91oV0Ra76CW6kIUhYvCMwBZcdLLJnDpAQuqA1URVhqbT1pucSQLDltiFkLNncSFasoY9IoANTKhobtex6qO0JvU89/1FliJ3fSqPNlfRu/wtB/6I6SXqR6zLsFBf0l7bYJz1gMbolXlFxfF2K2/931v1G1+Fkh4HUU+ndlzM+QX/SXLsYYVTmE5L1Zqo4G0OrBFoTnicm830nFG3pCg1VcE8xcTCONHEwMgkJdnYzMzU3NjAzMDMNNXI2MQyxdLM3MzMKM3YzNBwYqwHi4GZudHElcHSINrAQEHbAAi4uNwyK0pKi1IVkhKLU7kAMlYXqaYCeJwzNDAwMzFRKEoty0wt1yupKGHou7Y5vbO/RotHS0qMr6S1o2hvXhsA9qANxLlFeJyt0FtuwjAQheH37MVS55LbHtgEIQZFyqWiSO3yQfaZmSygPHh+IfGN8eXyv5+0Hvsj3Y5tO/a0Lntuvp/5vvylLwuyYAuxUIvWorPoLQaL0cGg3SbHyXVyntwnX0C+gXwF+Q72HRz39x3sO9h3sO9g38F9s+ffdC3nVM5bOedy5nLey/lotmWe15yun5fzpEiOlEiNbCO7yD5yiBxPK87rfN8Ut5jS6VuOlEiNbCO7yD5yiBxPK87rqFn2n/x8fR4OMVncLGaL3Lyuy/q5bhlUB9chdWgdbR1dHX0dQx0jfm4MHAJEkAgUwSJgBI3AETyGx3YveAyP4TE8hsfwGB7DE3gCT+yPwhN4Ak/gCTyBJ/AUnsJTeGovB0/hKTyFp/B0bN56UVTPpgJ4nDM0MDAzMVEoSi3LTC3XK6koYThRGKDn6Cp7OKg/fkKYbUjr9f1XpQHyyQ5P8AWO1rNniY98KgwqGhYOdIWIcr1uhnicu8mxj2NDAaN4fk6KbiIXiEwCk8lgMmXyTMZ25ZTUnNSSVKA0lJEEYyTDGCmT5ZnaN19mamMEAFNDG4HNliAqf8Seh8P6abcBumeVz+/4kg==")) }
				, PackTwo{ QByteArray::fromBase64(QByteArrayLiteral(
					  "UEFDSwAAAAIAAAAGmxF4nK2MTQrCMBCF9znF7AVJmqRpQURcuBZvMGlmaKB/hLT1+LYVb+BbPN7i+15OROCqqmTrmcnYmpVSLANSY0woPCrS2rPRrAsxYaIhQ6itZEZyXtfojXO82WRco1BJb8tSa+U8lgLn3I4J7pQzpefcdS9aIq3wiO88J4ILf8fNH8S0EekgznFYsIvhCspJUxi5BU57i2bs+7jT/70VP7klDJDXUXwA3GNdCvkF0vJfAkmKRyuFfRv/zxHOy/WZgah4nJvN9JxRt6QoNVXBPMXEwjjRxMDIJCXZ2MzM1NzYwMzAzDTVyNjEMsXSzNzMzCjN2MzQcGKsB4uBmbnRxJXB0iDawEBB2wAIuLjcMitKSotSFZISi1O5ADJWF6mmAnicMzQwMDMxUShKLctMLdcrqShhOMm1ZV/aypOM5tEujLLdlSIvbl8rBgD56g6muUZ4nK3Qb27CMAyH4e+5S6TZTv/dgUvQNrBKpUXAtO32Q8nPdg8wPsSvkHgccjr97yeu+3aN03677Vtcly2H+yNflp/4oUEarCEaSaPRaDU6jV5jMNBps8lwMp2MJ/PJFpBtIFtBtoNtB/v9bQfbDrYdbDvYdnAXtvwdz+UcyzmVcy5nLuelnNdwW+Z5zfH8fjlL8mRP8UyejWfr2Xn2nsNhxXGd7Rv9FmM8fMue4pk8G8/Ws/PsPYfDiuM6Csv2zI/X++EQo8akMWvk8Dov6/u6ZVAdXIfUkepo6mjr6Oro6xjwc2XgECCCRKAIFgEjaASO4DE81nvBY3gMj+ExPIbH8BiewBN4on8UnsATeAJP4Ak8gZfgJXgJXtKXg5fgJXgJXoKXhnDZH1OO96/nZ9y39Tf8Afx5WsSmAnicMzQwMDMxUShKLctMLdcrqShhOFEYoOfoKns4qD9+QphtSOv1/VelAfLJDk/wBckKtL5mqckBN1tEAR2LeRTo29ZzeJx7ybGPY0MBo3h+TopuIheITAKTyWAyZfJMxnbllNSc1JJUoDSUkQRjJMMYKZPlmdo3X2ZqYwQAWEMbkYNLFqfGGcqscJR3T8aHV+JRYnKV")) }
			{
				if (PackOne.size() < 32 || PackTwo.size() < 32
					|| !PackOne.startsWith("PACK") || !PackTwo.startsWith("PACK")) {
					throw std::runtime_error{ "Git fixture packs are invalid" };
				}
			}

			std::string Base        = "d950ffae7b39ab477ff5be47c1a10b5663317ba6";
			std::string HeadOne     = "ff4972ec618553984e4bd59aa8e06d0eac11684e";
			std::string HeadTwo     = "d2f25f02498a472b857d1bffcf11cecbf59981a8";
			std::string BaseBlob    = "c871502e41451dc3528f5f90563d5485d7bfd51b";
			std::string HeadOneBlob = "8ed6b367898f7c2a0c2a1a160e74858872bd6e86";
			std::string HeadTwoBlob = "c90ab4be66a9c901375b44011d8b7914e8dbd673";
			QByteArray  PackOne;
			QByteArray  PackTwo;
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

		[[nodiscard]] constexpr auto fixture_directory(
			const github_public_path_fixture aFixture) noexcept -> std::string_view
		{
			switch (aFixture) {
			case github_public_path_fixture::github_com:
				return "github-com";
			case github_public_path_fixture::ghes_3_18:
				return "ghes-3.18";
			}
			return { };
		}

		[[nodiscard]] constexpr auto fixture_server(
			const github_public_path_fixture aFixture) noexcept -> server_contract
		{
			switch (aFixture) {
			case github_public_path_fixture::github_com:
				return server_contract::github_com;
			case github_public_path_fixture::ghes_3_18:
				return server_contract::ghes_3_18;
			}
			return server_contract::github_com;
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
			const github_public_path_fixture aFixture,
			const deterministic_git_fixture& aGit,
			const std::string_view           aHead)
			-> std::vector<recorded_exchange>
		{
			const auto root = std::filesystem::path{ BPR_GITHUB_CONTRACT_FIXTURE_ROOT }
				/ fixture_directory(aFixture);
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
							aGit.Base);
				const auto observedHead = *operation == read_operation::list_reviews
					? std::string_view{ aGit.HeadOne }
					: aHead;
				replace_all(responseBody,
							"2222222222222222222222222222222222222222",
							observedHead);
				exchanges.push_back(
					{
						.Request = {
							.Semantic  = { },
							.Server    = fixture_server(aFixture),
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
			const github_public_path_fixture aFixture) -> std::string
		{
			return read_all(
				std::filesystem::path{ BPR_GITHUB_CAPABILITY_FIXTURE_ROOT }
				/ fixture_directory(aFixture) / "supported.json");
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
			const github_public_path_fixture aFixture,
			const deterministic_git_fixture& aGit)
			-> std::vector<recorded_exchange>
		{
			auto exchanges = load_recording(aFixture, aGit, aGit.HeadOne);
			exchanges.resize(1U);
			return exchanges;
		}

		struct fixture_recording_state
		{
			fixture_recording_state(
				const github_public_path_fixture                       aSelectedFixture,
				const adapters::github::validated_github_host_profile& aSelectedProfile,
				std::shared_ptr<deterministic_git_fixture>             aGit,
				const github_epoch_fixture_phase                       aPhase)
				: Profile{ aSelectedProfile }
				, Git{ std::move(aGit) }
				, BootstrapRecording{ load_bootstrap_recording(aSelectedFixture, *Git) }
				, Recording{ [&] {
					auto result = load_recording(
						aSelectedFixture,
						*Git,
						aPhase == github_epoch_fixture_phase::reuse
							? std::string_view{ Git->HeadTwo }
							: std::string_view{ Git->HeadOne });
					if (aPhase == github_epoch_fixture_phase::seed) {
						auto second = load_recording(
							aSelectedFixture, *Git, Git->HeadTwo);
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
				, CapabilityBody{ load_capability_response(aSelectedFixture) }
			{
			}

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
				const auto& pack = head == State->Git->HeadOne
					? State->Git->PackOne
					: head == State->Git->HeadTwo ? State->Git->PackTwo
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
				if (Role != fixture_transport_role::account
					|| !application::valid(aContext)
					|| aContext.Authority.Account != core::github_account_id{ 1001U }
					|| aContext.Authority.Repository != core::repository_id{ 2001U }
					|| aContext.Authority.PullRequest != core::pull_request_id{ 3001U }
					|| aContext.Authority.Number != core::pull_request_number{ 17U }
					|| aContext.Repository
						!= core::repository_locator{
							.Owner = "octo-org",
							.Name  = "review-repository",
						}
					|| aContext.BaseRef.view() != "refs/heads/main" || base != State->Git->Base || (head != State->Git->HeadOne && head != State->Git->HeadTwo)) {
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
			GitExecutable = QFileInfo{
				QStandardPaths::findExecutable(QStringLiteral("git"))
			}
								.canonicalFilePath()
								.toStdString();
			if (GitExecutable.empty()) {
				throw std::runtime_error{
					"GitHub public-path installed Git is unavailable"
				};
			}
			Git       = std::make_shared<deterministic_git_fixture>();
			Recording = std::make_shared<fixture_recording_state>(
				aFixture, aProfile, Git, aOptions.EpochPhase);
			Factory    = std::make_shared<fixture_transport_factory>(Recording);
			VaultState = std::make_shared<fixture_vault_state>();
			Vault      = std::make_unique<fixture_credential_vault>(
				aVaultEffects, VaultState);

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

			ApplicationData = std::make_unique<QTemporaryDir>(
				QStringLiteral("bpr-public-path-XXXXXX"));
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

		std::shared_ptr<fixture_recording_state>   Recording;
		std::shared_ptr<deterministic_git_fixture> Git;
		std::shared_ptr<fixture_transport_factory> Factory;
		std::shared_ptr<fixture_vault_state>       VaultState;
		std::unique_ptr<fixture_credential_vault>  Vault;
		std::unique_ptr<QTemporaryDir>             ApplicationData;
		std::filesystem::path                      ApplicationDataRoot;
		std::filesystem::path                      GitExecutable;
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
			result.BaseOid        = Implementation->Git->Base;
			result.HeadOneOid     = Implementation->Git->HeadOne;
			result.HeadTwoOid     = Implementation->Git->HeadTwo;
			result.BaseBlobOid    = Implementation->Git->BaseBlob;
			result.HeadOneBlobOid = Implementation->Git->HeadOneBlob;
			result.HeadTwoBlobOid = Implementation->Git->HeadTwoBlob;
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
}
