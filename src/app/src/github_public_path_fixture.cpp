#include "github_public_path_fixture.hpp"
#include "large_review_fixture.hpp"
#include "review_submission_fixture.hpp"

#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "github_capability_probe.hpp"
#include "github_device_authorization_contract.hpp"
#include "github_mutation_contract.hpp"
#include "github_read_contract.hpp"

#include "bpr/adapters/sqlite/storage_root.hpp"
#include "github_wire_request_validation.hpp"
#include "github_wire_transport.hpp"

#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/application/git_smart_transport_port.hpp"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace bpr::app::detail {
	namespace {
		/// [[BPR-1407]]. A discovered pull request's title; #4002's is long, so
		/// Home is seen with a title that wraps beside its facts.
		[[nodiscard]] auto discovered_title(const QString& aNumber) -> QString
		{
			return aNumber == QStringLiteral("4002")
				? QStringLiteral("Keep the review queue's discovery cache warm across restarts so one slow repository never holds back the rows of every other saved repository")
				: QStringLiteral("Discovered pull request ") + aNumber;
		}

		constexpr std::string_view capability_diagnostic_failure_body = R"({"errors":[{"message":"Field '__type' has a max repetition count of 2","extensions":{"code":"NEVER_SEEN_CODE"}}]})";
		using adapters::github::detail::anonymous_read_operations;
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

		constexpr auto disconnected_history_head = std::string_view{ "67c9afbd86e28b19e6bd224f966b4a48e1294e6a" };

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
					  "UEFDSwAAAAIAAAAGmxF4nK3MSwoCMRAE0H1Okb0g3ZPvgIi4cC3eoDvTwcA4IyGjHt8f3sBa1eJVtSqimTl23RCJKUUwYnpEa71B4yX1CTwQoYOsrlRlajq4IDF3ru8MZkhskH0INqEVT2ng7CzbmI2ipZ3nqvfSmtTjMo4nuRW560N5tKWK3uRv2fFHXF+ifsS6TDcay7DVGMCiMx5Ar+AVlebLpbz1f2/Vb3wWGvQ8iXoC4b5cAfkF+RCJE8BTNU6rLkdx7vJ+lXpACWZ4nJvN9JxRt6QoNVUhMSXFyDQt0dAyzcQ80SDVwMA0NSXV1Mgg1SzNyNAiOdkoycjYzMjcfGKsB4uBmbnRxJXB0iDawEBB2wAIuLjcMitKSotSFZISi1O5AJhXGUGmAnicMzQwMDMxUShKLctMLddLLihg6Jg+zz41Ljoh/ve/oPcOP//7uqctAAAHLxA4socDeJyt1stu2zwQBeB18hRqVgrgpp4hfUsvL1IUgsZiHLayZEhU0wv67pXtGj+jmTCz+LOy5HOUAOeL6Xfvsrf/60/Wu23wbZNROzRV2f2cZZWrPbmuDK7+mdVts7u+2rZNH9yPQ5eVQ2izQ+ce/I9inn3Mtu3+MARXnG/l8+H2/Utx4HFIxJHHMRE3PG4SccvjNhFf8PgiEV/y+DIRX/H4KhFf8/g6Ed/w+CY1kzArJHeVhk0tC8K0kNoWhHEhtS4I80JqXxAGhtTCIEwMqY1BGBlSK4MwM6R2BmFoSC2NwtKYWhqFpTH5Pyz9E6eWRmFpTC2NwtKYWhqFpTG1NApLY2ppFJbGaOlzuN+WtRuDnSur4nSRj+8++N0kR77sL7Hjazm1K/f78hI7XUQ5/5Dlb76Xta/Gz/T89Ltmp+fOzr3b2+z39dVV58LQNVnly13T9sFv7+99c6oVY8XTeCKMB8X4vD/Xl+yDb8Z3fokPfX96ZD/U4W7vq6p2Rfl5/mX8G6l2TZX37dBt3d3x3ix7cn73GIoyn9+KPRB6EPdA7qHQw7iHcs8IPRP3jNyzQs/GPSv3FkJvEfcWcm8p9JZxbyn3VkJvFfdWcm8t9NZxby33NkJvE/c2L+wugYFnYuAlMqKZ52hENSQopVgpyUpJUEqxUpKVkqCUYqUkKyVBKcVKSVZKglKKlZKslASlFCslWSkJSilWSrJSEpRSrJRkpSQopVgpyUpJUEqxUpKVkqSUnimlF5SSpJSeKaWL0s7t/HjYdMXehc5v85voM/muDMHtD6G/mWWXl692Ovf1/NX/2Prv4tVeNZxfFPtj0dXloXfVeHEsHgaqff/4r9fnvW++nR/Yu/DvbvHkm6p9yo/3+zA+aluUfe+6kIfS18XxyOl82YQP80/39+PxM4znys3xrWyePbZ11d+82oRpE7RNnDZR2zTTptE27bRptc3FtLnQNpfT5lLbXE2bK21zPW2utc3NtLlRS2CIQK+IM1I7AgYJ1JKAUQK1JWCYQK0JGCdQewIGCtSigJECtSlgqECtChgrULtC5grVrpC5Qv3nE/+AUrtC5grVrpC5QrUrZK5Q7QqZK1S7QuYK1a6QuUK1K2SuUO3KMFdG7cowV0btyjBXRn/y8aNP7cowV0btyjBXRu3KMFdG7cowV0btyjBXRu3KMFdG7coyV1btyjJXVu3KMldW7coyV1b/nYp/qVK7ssyVVbuyzJVVu7LMlVW7ssyVVbuyzJWNXP0FAylC0qYCeJwzNDAwMzFRKEoty0wt10suKGCoP9wtXsnTH8km8e1ih0vaXW1epxsA8l0N6vMSiJeeP2VeW2Bf+/5S70D5/01HZqB4nG2OMW4CMRBFJSQQGyJukJ5UoecS3GBk7L9rK6OxNfYqUNFxAc5AkwakLShzoVQ5QdhESBsp9Xvz/nwtry+Xj/G+wKiLb0IKcVBKIYGDYOGNOMbz6qFSMEwGGWbKxTRBGtq0dQ3Ni547jakHDAq3xpYgRQN+ocQS6h1l35afGQ65QO6nVXeenEb7mtvsySEprClwZI31/70wUFKMPDBaUTR9W4eSj/F1IFmOfyP2xgTTe6hbV++jI+ZP3efjYfYNBKBs1pkOvRjOzrNo/oia0+E6JLaRvdB3")) }
				, PackTwo{ QByteArray::fromBase64(QByteArrayLiteral(
					  "UEFDSwAAAAIAAAAGmxF4nK3MTQrCMBAF4H1Okb0gnWTSpCAiLlyLN8jPhAb6R0hbj29b8QbO4vEW35uSibhHwhqsUNSg9TJgNAagRm1chFA1QgXhGhBsspmGwrXSZKJQjZAQK+8kuFpr9IBUWx9cVOjQRMnsXNox8zuVQvk5d92LlkQrf6R3mTPxS/yWmzvEtIl8iHMaFtulcOWgKxRYbcdPezI/9n3a9X/fst+4JRt4WUf2AZZJW8P5BWRY2WvwzN3B7y/PYOYck6KOadZleJybzfScUbekKDVVITElxcg0LdHQMs3EPNEg1cDANDUl1dTIINUszcjQIjnZKMnI2MzI3HxirAeLgZm50cSVwdIg2sBAQdsACLi43DIrSkqLUhWSEotTuQCYVxlBpgJ4nDM0MDAzMVEoSi3LTC3XSy4oYEiZ9P+5bum34807Xk3pt5pSrLP3mgYAGuMQ/rmKA3icrdfbbuM2EAbg6+QptLlSAG/WM6RP2bYvUhQCx6IddmXJ0KHZtOi7V7bXWEYzZuaivtJhfsbA/1lUvnzJPv+vn6zz2z40dUbNUJeufZtlpa8C+db1vnrLqqbe399tm7rr/fdjm7mhb7Jj63fhezHPfs22zeE49L64XMrnw+PXW+PAxyExjnwcE+OGj5vEuOXjNjG+4OOLxPiSjy8T4ys+vkqMr/n4OjG+4eObVE1CrZDsVSo21SwI1UKqWxDKhVS7INQLqX5BKBhSDYNQMaQ6BqFkSLUMQs2Q6hmEoiHVNApNY6ppFJrG5G9Y+hGnmkahaUw1jULTmGoahaYx1TQKTWOqaRSaxqjpy3C3dZUfB1vvyuJ8ko93d2E/maPguuvY6Vie2rvDwV3HzifRXNhl+ae/XBXK8Zmen//W7Lzu7JJ7fMz+ub+7a30/tHVWBrevm64P2+fnUJ9jxRgJNO4I40Yxrvfv/XV2F+rxzt/iol/PS3ZD1T8dQllWvnC/z/8YvyNVvi7zrhnarX86XZtlrz7sX/rC5fNHMQdCDuIcyDkUchjnUM4ZIWfinJFzVsjZOGfl3ELILeLcQs4thdwyzi3l3ErIreLcSs6thdw6zq3l3EbIbeLc5kbvEhh4JwZukRHNvEcjqiFBKcVKSVZKglKKlZKslASlFCslWSkJSilWSrJSEpRSrJRkpSQopVgpyUpJUEqxUpKVkqCUYqUkKyVBKcVKSVZKglKKlZKslCSl9E4p3VBKklJ6p5SuSlu/D+Nm0xYH37dhmz9Ez+Qn1/f+cOy7h1l2Pfww0/o/L6/+p9TPkw9z5XA5KA6noK/csfPleHIKHgeqQvfyI9flXai/XRbsfP/javEa6rJ5zU/Xu35calu4rvNtn/cuVMVpy2mDq/tf5r89P4/bzzDuKw+nW9k8e2mqsnv4MAnTJGiTOE2iNmmmSaNN2mnSapOLaXKhTS6nyaU2uZomV9rkeppca5ObaXKjlsAQgV4RZ6R2BAwSqCUBowRqS8AwgVoTME6g9gQMFKhFASMFalPAUIFaFTBWoHaFzBWqXSFzhfrnE39AqV0hc4VqV8hcodoVMleodoXMFapdIXOFalfIXKHaFTJXqHZlmCujdmWYK6N2ZZgro9/5+NandmWYK6N2ZZgro3ZlmCujdmWYK6N2ZZgro3ZlmCujdmWZK6t2ZZkrq3ZlmSurdmWZK6t/p+IvVWpXlrmyaleWubJqV5a5smpXlrmyaleWubK3Xe2a8X+U4jiML/lNXb2Nic5vm7rMXrwrs4Nrv/n2FPsPpXxWmaYCeJwzNDAwMzFRKEoty0wt10suKGCoP9wtXsnTH8km8e1ih0vaXW1epxsA8l0N6vMSZJL/5y119seDuOqUjzqUcyy91ih4nG2OMW4CMRBFJSSiLIm4QXpSodS5BDcYGfvv2mI0tsZeJanocgHOQBEakLZImQtxCNggpEWifm/en5+33/nhb7wuMOrih5BCHJRSSOAgmHkjjvH6PqkUDJNBhplyMU2QhpZtXUPzrOdOY+oBg8K58UmQogEXKLGE+ouyb8v/DIdcINfTqts/bEfrmtvsySEprClwZI31914YKClGHhitKJq+rUPJx7gaSJbjbcSemeDxGuoW1W60wfSlOz5/P50AsrhsjvB8T7yjeTsVsAYnuOJCIRyPmUN9")) }
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
			/// A single-head descriptor omits the second file and leaves PackTwo
			/// empty. The window-reading descriptor supplies two self-contained packs.
			explicit deterministic_git_fixture(const std::string_view aPackFile, const std::optional<std::string_view> aSecondFile = std::nullopt)
				: PackOne{ QByteArray::fromStdString(read_all(
					  std::filesystem::path{
						  BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT }
					  / aPackFile)) }
			{
				if (aSecondFile) {
					PackTwo = QByteArray::fromStdString(read_all(std::filesystem::path{ BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT } / *aSecondFile));
					if (PackTwo.size() < 32 || !PackTwo.startsWith("PACK")) {
						throw std::runtime_error{ "Second Git fixture pack is invalid" };
					}
				}
				if (PackOne.size() < 32 || !PackOne.startsWith("PACK")) {
					throw std::runtime_error{ "Git fixture pack is invalid" };
				}
			}

			QByteArray PackWithoutBase{ QByteArray::fromBase64(QByteArrayLiteral("UEFDSwAAAAIAAAAClQx4nJ2LTQrCMBQG9znF2wsSSxoTEOlCvEd+vtIHaVPSV+nxRfQEzmKYzUgDyETX9TlZ06VoEX3QVqM3cXTZ+s65MQLGa6PCLlNt9ORD9ga6jd8YcIR5LTjz8gqF850uV/2DTh+rVOeZRfDXrB68pbosSIJME29SG6dQaF9zEKg3KOk/WiB4nAMAAAAAAaPXhyKfPVtKY7oyn3VT6PJNILee")) };
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

		struct fixture_json_options : glz::opts
		{
			// Glaze discovers these option overrides by their exact library-defined names.
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool escape_control_characters = true;
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool validate_skipped = true;
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool validate_trailing_whitespace = true;
			constexpr fixture_json_options()
			{
				null_terminated       = false;
				error_on_unknown_keys = false;
				error_on_missing_keys = true;
			}
		};

		struct interaction_json
		{
			std::string                                     Operation;
			std::string                                     Method;
			std::string                                     Path;
			std::string                                     CanonicalQuery;
			std::map<std::string, std::string, std::less<>> SafeHeaders;
			std::optional<std::string>                      Body;
			std::string                                     ResponseFile;
			struct glaze
			{
				static constexpr auto value = glz::object("operation", &interaction_json::Operation, "method", &interaction_json::Method, "path", &interaction_json::Path, "canonical_query", &interaction_json::CanonicalQuery, "safe_headers", &interaction_json::SafeHeaders, "body", &interaction_json::Body, "response_file", &interaction_json::ResponseFile);
			};
		};
		struct recording_json
		{
			std::vector<interaction_json> Interactions;
			struct glaze
			{
				static constexpr auto value = glz::object("interactions", &recording_json::Interactions);
			};
		};
		struct projection_json
		{
			std::vector<std::string>                          ObservedFields;
			std::map<std::string, glz::raw_json, std::less<>> Values;
			struct glaze
			{
				static constexpr auto value = glz::object("observed_fields", &projection_json::ObservedFields, "values", &projection_json::Values);
			};
		};
		struct device_recording_json
		{
			int             HttpStatus = 0;
			projection_json Projection;
			struct glaze
			{
				static constexpr auto value = glz::object("http_status", &device_recording_json::HttpStatus, "projection", &device_recording_json::Projection);
			};
		};

		[[nodiscard]] auto parse_recording(const std::filesystem::path& aPath) -> recording_json
		{
			recording_json recording;
			if (glz::read<fixture_json_options{ }>(recording, read_all(aPath))) {
				throw std::runtime_error{ "GitHub public-path fixture is malformed" };
			}
			return recording;
		}

		/// \brief Says where the packs one fixture serves come from.
		///
		/// \details Two shapes, and a descriptor picks one. The synthetic
		/// legacy three-commit graph stays embedded. Recorded graphs name files,
		/// with an optional second pack for the window-reading graph. An absent
		/// `File` selects the embedded pair; a present one is relative to
		/// `BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT`.
		struct fixture_pack_source
		{
			std::optional<std::string_view> File;
			std::optional<std::string_view> SecondFile = std::nullopt;
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
			std::string_view ContractDirectory;
			std::string_view LaterRecordingDirectory;
			std::string_view CapabilityDirectory;
			/// Optional recorded-wire source when the semantic descriptor overlays
			/// another contract. Empty means `ContractDirectory`.
			std::string_view          RecordingDirectory;
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
			bool                      Paged          = false;
			bool                      WindowReading  = false;
			bool                      OrdinaryUpdate = false;
			/// [[BPR-1331]]. A one-head fixture's next push: under an epoch
			/// phase it is served as `HeadTwoOid` from `NextPushPack`, which
			/// holds every object reachable from it. Outside a phase the
			/// fixture serves its one head exactly as before.
			std::string_view                NextPushOid;
			std::optional<std::string_view> NextPushPack = std::nullopt;
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
			case github_public_path_fixture::profile_window_restoration_v1:
				return {
					.ContractDirectory   = "profile-window-restoration-v1",
					.CapabilityDirectory = "github-com",
					.RecordingDirectory  = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1001U },
					.RepositoryId        = core::repository_id{ 2001U },
					.PullRequestId       = core::pull_request_id{ 3001U },
					.Number              = core::pull_request_number{ 17U },
					.Owner               = "octo-org",
					.Name                = "review-repository",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "a61a212aa0484929bc5f20a26e9a7c1c6541c726",
					.HeadOneOid          = "3b60c8324eee117ce72e6446fe640d85a60b37a2",
					.HeadTwoOid          = "2de36395329f23d1a5f49f3c591b76991fc73b4b",
					.BaseBlob            = "3456e31deaf7dc9a4c9a56b384c1b0ee9ff24e5b",
					.HeadOneBlob         = "1522997381f979112468fccd2be62e69f93b96c9",
					.HeadTwoBlob         = "8768eb6926e9a68f8332bc1085c990231fbc80f3",
					.Packs               = { .File = "profile-window-restoration-v1/initial.pack", .SecondFile = "profile-window-restoration-v1/replacement.pack" },
					.WindowReading       = true,
				};
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
			case github_public_path_fixture::large_review_v1:
				return {
					.ContractDirectory   = "large-review-v1",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 800001U },
					.RepositoryId        = core::repository_id{ 800002U },
					.PullRequestId       = core::pull_request_id{ 800003U },
					.Number              = core::pull_request_number{ 17U },
					.Owner               = "bpr-fixtures",
					.Name                = "large-review",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = large_review_fixture::base_oid,
					.HeadOneOid          = large_review_fixture::head_oid,
					.HeadTwoOid          = "",
					.BaseBlob            = "",
					.HeadOneBlob         = "",
					.HeadTwoBlob         = "",
					.Packs               = { .File = "large-review-v1/objects.pack" },
					.Paged               = true,
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
			// [[BPR-1213]]. Constructed by `scripts/build_nested_folders_fixture.py`
			// and served like `uiforetw_74`: one head, github.com's capability
			// snapshot, and the commits the pack builder makes from its blobs.
			// [[BPR-1083]]. The nested-folders pull request, pack and identity
			// alike, with its own contract: review threads whose posts carry the
			// Conventional Comments labels (`scripts/build_conventional_comments_fixture.py`).
			case github_public_path_fixture::conventional_comments_v1:
				return {
					.ContractDirectory   = "conventional-comments-v1",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1U },
					.RepositoryId        = core::repository_id{ 2U },
					.PullRequestId       = core::pull_request_id{ 3U },
					.Number              = core::pull_request_number{ 21U },
					.Owner               = "bpr-fixtures",
					.Name                = "nested-folders",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "c58160c2ffe27d7cf5e6759d78b80f023547c789",
					.HeadOneOid          = "b20cb443428212543f5911e091a0ffeb407d23cf",
					.HeadTwoOid          = "",
					.BaseBlob            = "",
					.HeadOneBlob         = "",
					.HeadTwoBlob         = "",
					.Packs               = { .File = "nested-folders-v1/objects.pack" },
					// [[BPR-1331]]. Clamps renderer.cpp's resize and changes no
					// other file (`scripts/build_conventional_comments_next_push_pack.py`).
					.NextPushOid  = "cd24836cfb58a9c14f4d3464d68a57ff8aa2a186",
					.NextPushPack = "conventional-comments-next-push-v1/next-push.pack",
				};
			// [[BPR-1089]]. The nested-folders pull request, pack and identity
			// alike, with many labelled threads, pull request comments and
			// review summaries (`scripts/build_overview_labels_fixture.py`).
			case github_public_path_fixture::overview_labels_v1:
				return {
					.ContractDirectory   = "overview-labels-v1",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1U },
					.RepositoryId        = core::repository_id{ 2U },
					.PullRequestId       = core::pull_request_id{ 3U },
					.Number              = core::pull_request_number{ 21U },
					.Owner               = "bpr-fixtures",
					.Name                = "nested-folders",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "c58160c2ffe27d7cf5e6759d78b80f023547c789",
					.HeadOneOid          = "b20cb443428212543f5911e091a0ffeb407d23cf",
					.HeadTwoOid          = "",
					.BaseBlob            = "",
					.HeadOneBlob         = "",
					.HeadTwoBlob         = "",
					.Packs               = { .File = "nested-folders-v1/objects.pack" },
				};
			// [[BPR-1290]]. Constructed by `scripts/build_inline_drawer_fixture.py`:
			// the conventional-comments files and threads plus a long file with
			// threads at its lines, its own pack and one head.
			case github_public_path_fixture::inline_drawer_v1:
				return {
					.ContractDirectory   = "inline-drawer-v1",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1U },
					.RepositoryId        = core::repository_id{ 2U },
					.PullRequestId       = core::pull_request_id{ 3U },
					.Number              = core::pull_request_number{ 22U },
					.Owner               = "bpr-fixtures",
					.Name                = "inline-drawer",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "916aad158d0a7cd7aaf15d6ca64123c71deaea2d",
					.HeadOneOid          = "89df5e592e5db97a526a05a445735733395e4832",
					.HeadTwoOid          = "",
					.BaseBlob            = "",
					.HeadOneBlob         = "",
					.HeadTwoBlob         = "",
					.Packs               = { .File = "inline-drawer-v1/objects.pack" },
				};
			// [[BPR-1293]]. Constructed by `scripts/build_conversations_fixture.py`:
			// prototype v2's ten files and twenty-one conversations, one head.
			case github_public_path_fixture::conversations_v1:
				return {
					.ContractDirectory   = "conversations-v1",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1U },
					.RepositoryId        = core::repository_id{ 2U },
					.PullRequestId       = core::pull_request_id{ 3U },
					.Number              = core::pull_request_number{ 418U },
					.Owner               = "bpr-fixtures",
					.Name                = "conversations",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "9a0c46377e514e4760639ffa972cd5812b30c5b2",
					.HeadOneOid          = "f05373e1184e7067994839ecbe6e2db7a933fa97",
					.HeadTwoOid          = "",
					.BaseBlob            = "",
					.HeadOneBlob         = "",
					.HeadTwoBlob         = "",
					.Packs               = { .File = "conversations-v1/objects.pack" },
				};
			case github_public_path_fixture::cpp_identifiers_v1:
				return {
					.ContractDirectory   = "cpp-identifiers-v1",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1U },
					.RepositoryId        = core::repository_id{ 2U },
					.PullRequestId       = core::pull_request_id{ 3U },
					.Number              = core::pull_request_number{ 72U },
					.Owner               = "bpr-fixtures",
					.Name                = "cpp-identifiers",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "c587f5b879dc20b319b5de8fb75f512d493bf36b",
					.HeadOneOid          = "7b0b05bac636051f80198ef1ad6c52aff0aedf36",
					.HeadTwoOid          = "",
					.BaseBlob            = "",
					.HeadOneBlob         = "",
					.HeadTwoBlob         = "",
					.Packs               = { .File = "cpp-identifiers-v1/objects.pack" },
				};
			case github_public_path_fixture::source_pr_search_large_v1:
				return {
					.ContractDirectory       = "source-pr-search-large-v1",
					.LaterRecordingDirectory = "source-pr-search-large-later-v1",
					.CapabilityDirectory     = "github-com",
					.Server                  = server_contract::github_com,
					.AccountId               = core::github_account_id{ 1U },
					.RepositoryId            = core::repository_id{ 2U },
					.PullRequestId           = core::pull_request_id{ 3U },
					.Number                  = core::pull_request_number{ 78U },
					.Owner                   = "bpr-fixtures",
					.Name                    = "source-pr-search-large",
					.BaseRef                 = "refs/heads/main",
					.BaseOid                 = "bf800d840cd764e38517f06d348194b513c09fb5",
					.HeadOneOid              = "1bc7774584fc5c9b8001358c837ac956db6c5498",
					.HeadTwoOid              = "f573297f4533006cdd4055c1cd26fcbd6c8f0072",
					.BaseBlob                = "",
					.HeadOneBlob             = "",
					.HeadTwoBlob             = "",
					.Packs                   = { .File = "source-pr-search-large-v1/objects.pack", .SecondFile = "source-pr-search-large-v1/objects.pack" },
				};
			case github_public_path_fixture::source_pr_search_v1:
				return {
					.ContractDirectory       = "source-pr-search-v1",
					.LaterRecordingDirectory = "source-pr-search-later-v1",
					.CapabilityDirectory     = "github-com",
					.Server                  = server_contract::github_com,
					.AccountId               = core::github_account_id{ 1U },
					.RepositoryId            = core::repository_id{ 2U },
					.PullRequestId           = core::pull_request_id{ 3U },
					.Number                  = core::pull_request_number{ 78U },
					.Owner                   = "bpr-fixtures",
					.Name                    = "source-pr-search",
					.BaseRef                 = "refs/heads/main",
					.BaseOid                 = "2d1b33154f67e9a649702eec6afc4ec319cbdfe6",
					.HeadOneOid              = "4ee2d050fd427ccadc7f23e6bd630cd1f81953fd",
					.HeadTwoOid              = "f7de4366ea00c840b15ece6b445eae7a320ef363",
					.BaseBlob                = "",
					.HeadOneBlob             = "",
					.HeadTwoBlob             = "",
					.Packs                   = { .File = "source-pr-search-v1/objects.pack", .SecondFile = "source-pr-search-v1/objects.pack" },
				};
			case github_public_path_fixture::source_find_v1:
				return {
					.ContractDirectory   = "source-find-v1",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1U },
					.RepositoryId        = core::repository_id{ 2U },
					.PullRequestId       = core::pull_request_id{ 3U },
					.Number              = core::pull_request_number{ 77U },
					.Owner               = "bpr-fixtures",
					.Name                = "source-find",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "3de354820bebc4e2644112d994d20d6d88ae13fd",
					.HeadOneOid          = "755992e699b42939e3bff7abbdd1d66827ce439f",
					.HeadTwoOid          = "",
					.BaseBlob            = "",
					.HeadOneBlob         = "",
					.HeadTwoBlob         = "",
					.Packs               = { .File = "source-find-v1/objects.pack" },
				};
			case github_public_path_fixture::header_source_v1:
				return {
					.ContractDirectory   = "header-source-v1",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1U },
					.RepositoryId        = core::repository_id{ 2U },
					.PullRequestId       = core::pull_request_id{ 3U },
					.Number              = core::pull_request_number{ 74U },
					.Owner               = "bpr-fixtures",
					.Name                = "header-source",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "71dc98a8939742f51fa98dd44936ba727c3e5796",
					.HeadOneOid          = "24537f5b9f51511d4ee94c420192f04a972e7548",
					.HeadTwoOid          = "",
					.BaseBlob            = "",
					.HeadOneBlob         = "",
					.HeadTwoBlob         = "",
					.Packs               = { .File = "header-source-v1/objects.pack" },
				};
			case github_public_path_fixture::nested_folders_v1:
				return {
					.ContractDirectory   = "nested-folders-v1",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1U },
					.RepositoryId        = core::repository_id{ 2U },
					.PullRequestId       = core::pull_request_id{ 3U },
					.Number              = core::pull_request_number{ 21U },
					.Owner               = "bpr-fixtures",
					.Name                = "nested-folders",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "c58160c2ffe27d7cf5e6759d78b80f023547c789",
					.HeadOneOid          = "b20cb443428212543f5911e091a0ffeb407d23cf",
					.HeadTwoOid          = "",
					.BaseBlob            = "",
					.HeadOneBlob         = "",
					.HeadTwoBlob         = "",
					.Packs               = { .File = "nested-folders-v1/objects.pack" },
				};
			// [[BPR-1334]]. Constructed by `scripts/build_line_details_fixture.py`:
			// four files whose changed lines are long, one head.
			case github_public_path_fixture::line_details_v1:
				return {
					.ContractDirectory   = "line-details-v1",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1U },
					.RepositoryId        = core::repository_id{ 2U },
					.PullRequestId       = core::pull_request_id{ 3U },
					.Number              = core::pull_request_number{ 34U },
					.Owner               = "bpr-fixtures",
					.Name                = "line-details",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "37b3ffda05bf3693487e4f92a5a1118d8647bede",
					.HeadOneOid          = "a7603d7f2fe157ae72a0d4e13dada90fab83b31a",
					.HeadTwoOid          = "",
					.BaseBlob            = "",
					.HeadOneBlob         = "",
					.HeadTwoBlob         = "",
					.Packs               = { .File = "line-details-v1/objects.pack" },
				};
			// [[BPR-1537]]. Constructed by `scripts/build_intraline_fixture.py`:
			// five files of everyday changed lines, one head.
			case github_public_path_fixture::intraline_v1:
				return {
					.ContractDirectory   = "intraline-v1",
					.CapabilityDirectory = "github-com",
					.Server              = server_contract::github_com,
					.AccountId           = core::github_account_id{ 1U },
					.RepositoryId        = core::repository_id{ 2U },
					.PullRequestId       = core::pull_request_id{ 3U },
					.Number              = core::pull_request_number{ 37U },
					.Owner               = "bpr-fixtures",
					.Name                = "intraline",
					.BaseRef             = "refs/heads/main",
					.BaseOid             = "a3f915b5baa6b05b2ffc1ac6502b218b5d16bec7",
					.HeadOneOid          = "d7e2f04de93e5e51097627e58093a42b68de36ce",
					.HeadTwoOid          = "",
					.BaseBlob            = "",
					.HeadOneBlob         = "",
					.HeadTwoBlob         = "",
					.Packs               = { .File = "intraline-v1/objects.pack" },
				};
			}
			return { };
		}

		/// \brief Builds the pack pair one descriptor asks for.
		[[nodiscard]] auto make_git_fixture(const fixture_pack_source aSource)
			-> std::shared_ptr<deterministic_git_fixture>
		{
			return aSource.File.has_value()
				? std::make_shared<deterministic_git_fixture>(*aSource.File, aSource.SecondFile)
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
			if (aOperation == read_operation::search_pull_requests || aOperation == read_operation::search_personal_pull_requests) {
				resource  = "search";
				limit     = "30";
				remaining = "29";
			} else if (graphql_read(aOperation)) {
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

		[[nodiscard]] auto rate_limit_response(const github_rate_limit_fixture& aFixture, const unsigned aMode, const read_operation aOperation) -> wire_response
		{
			const auto secondary = aMode == 2U;
			auto       limited   = wire_response{ .Status = 403, .Headers = response_headers(), .Body = secondary ? R"({"message":"You have exceeded a secondary rate limit. Please wait."})" : R"({"message":"API rate limit exceeded."})" };
			append_rate_limit_headers(limited, aOperation);
			for (auto& header : limited.Headers) {
				if (header.Name == "X-RateLimit-Reset") {
					header.Value = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() + aFixture.ResetAfterSeconds);
				}
				if (header.Name == "X-RateLimit-Remaining" && !secondary) {
					header.Value = "0";
				}
			}
			limited.Headers.push_back({ .Name = "Retry-After", .Value = std::to_string(aFixture.RetryAfterSeconds) });
			return limited;
		}

		/// \brief Keeps the two-file reading graph's metadata local to its descriptor.
		void overlay_window_reading(std::string& aBody, read_operation aOperation, const fixture_descriptor& aDescriptor)
		{
			if (!aDescriptor.WindowReading) {
				return;
			}
			auto document = QJsonDocument::fromJson(QByteArray::fromStdString(aBody));
			if (aOperation == read_operation::get_pull_request) {
				auto body = document.object();
				body.insert(QStringLiteral("changed_files"), 2);
				document = QJsonDocument{ body };
			} else if (aOperation == read_operation::list_pull_request_files) {
				QJsonArray files;
				files.append(QJsonObject{ { QStringLiteral("filename"), QStringLiteral("review.cpp") }, { QStringLiteral("status"), QStringLiteral("modified") }, { QStringLiteral("additions"), 6 }, { QStringLiteral("deletions"), 6 } });
				files.append(QJsonObject{ { QStringLiteral("filename"), QStringLiteral("secondary.cpp") }, { QStringLiteral("status"), QStringLiteral("modified") }, { QStringLiteral("additions"), 6 }, { QStringLiteral("deletions"), 6 } });
				document = QJsonDocument{ files };
			} else if (aOperation == read_operation::list_review_threads) {
				auto body       = document.object();
				auto data       = body.value(QStringLiteral("data")).toObject();
				auto repository = data.value(QStringLiteral("repository")).toObject();
				auto pull       = repository.value(QStringLiteral("pullRequest")).toObject();
				auto files      = pull.value(QStringLiteral("files")).toObject();
				files.insert(QStringLiteral("totalCount"), 2);
				files.insert(QStringLiteral("nodes"), QJsonArray{ QJsonObject{ { QStringLiteral("path"), QStringLiteral("review.cpp") }, { QStringLiteral("viewerViewedState"), QStringLiteral("UNVIEWED") } }, QJsonObject{ { QStringLiteral("path"), QStringLiteral("secondary.cpp") }, { QStringLiteral("viewerViewedState"), QStringLiteral("UNVIEWED") } } });
				pull.insert(QStringLiteral("files"), files);
				auto       threads   = pull.value(QStringLiteral("reviewThreads")).toObject();
				const auto prototype = threads.value(QStringLiteral("nodes")).toArray().first().toObject();
				QJsonArray nodes;
				for (const auto& path : { QStringLiteral("review.cpp"), QStringLiteral("secondary.cpp") }) {
					auto thread = prototype;
					thread.insert(QStringLiteral("id"), QStringLiteral("PRRT_fixture_reading_") + path);
					thread.insert(QStringLiteral("path"), path);
					thread.insert(QStringLiteral("line"), 2);
					thread.insert(QStringLiteral("originalLine"), 2);
					auto comments = thread.value(QStringLiteral("comments")).toObject();
					auto comment  = comments.value(QStringLiteral("nodes")).toArray().first().toObject();
					comment.insert(QStringLiteral("id"), QStringLiteral("PRRC_fixture_reading_") + path);
					comment.insert(QStringLiteral("path"), path);
					comment.insert(QStringLiteral("line"), 2);
					comment.insert(QStringLiteral("originalLine"), 2);
					comment.insert(QStringLiteral("bodyText"), QStringLiteral("Check the exact retained reading position."));
					comment.insert(QStringLiteral("author"), QJsonObject{ { QStringLiteral("login"), QStringLiteral("octo-author") }, { QStringLiteral("id"), QStringLiteral("U_fixture_author") } });
					comments.insert(QStringLiteral("nodes"), QJsonArray{ comment });
					thread.insert(QStringLiteral("comments"), comments);
					nodes.append(thread);
				}
				threads.insert(QStringLiteral("nodes"), nodes);
				threads.insert(QStringLiteral("totalCount"), 2);
				pull.insert(QStringLiteral("reviewThreads"), threads);
				repository.insert(QStringLiteral("pullRequest"), pull);
				data.insert(QStringLiteral("repository"), repository);
				body.insert(QStringLiteral("data"), data);
				document = QJsonDocument{ body };
			} else {
				return;
			}
			aBody = document.toJson(QJsonDocument::Compact).toStdString();
		}

		void overlay_review_tour(std::string& aBody, read_operation aOperation, const fixture_descriptor& aDescriptor)
		{
			const auto prefix   = QStringLiteral("https://github.com/octo-org/review-repository/pull/17");
			auto       document = QJsonDocument::fromJson(QByteArray::fromStdString(aBody));
			if (aOperation == read_operation::get_pull_request) {
				auto pull = document.object();
				pull.insert("body", "Preface α\r\n\r\n## Problem\r\n\r\n**Keep raw** problem.\r\n\r\n## Solution\r\n\r\nSmall exact solution.\r\n\r\n## Testing\r\n\r\nUntouched <details>notes</details>.\r\n\r\n[Tour](" + prefix + "#pullrequestreview-4001)\r\n");
				document = QJsonDocument{ pull };
			} else if (aOperation == read_operation::list_reviews) {
				auto review = document.array().first().toObject();
				review.insert("body", "## A hand-authored route\r\n\r\n1. [same](" + prefix + "#discussion_r71002)\r\n1. [same](" + prefix + "#discussion_r71001)\r\n1. [Historical](" + prefix + "#issuecomment-71003)\r\n");
				review.insert("html_url", prefix + "#pullrequestreview-4001");
				review.insert("updated_at", "2026-09-26T00:00:00Z");
				document = QJsonDocument{ QJsonArray{ review } };
			} else if (aOperation == read_operation::list_issue_comments) {
				const auto body = QStringLiteral("https://github.com/octo-org/review-repository/blob/1111111111111111111111111111111111111111/entirely-unchanged.md?plain=1#L1-L2\r\n\r\nHistorical explanation remains visible.");
				document        = QJsonDocument{ QJsonArray{ QJsonObject{ { "id", 71003 }, { "node_id", "IC_tour_history" }, { "body", body }, { "html_url", prefix + "#issuecomment-71003" }, { "user", QJsonObject{ { "node_id", "U_fixture_viewer" }, { "login", "octo-reviewer" } } } } } };
			} else if (aOperation == read_operation::list_review_threads) {
				auto body    = document.object();
				auto data    = body["data"].toObject();
				auto repo    = data["repository"].toObject();
				auto pull    = repo["pullRequest"].toObject();
				auto threads = pull["reviewThreads"].toObject();
				auto nodes   = threads["nodes"].toArray();
				for (qsizetype i = 0; i < nodes.size(); ++i) {
					auto thread   = nodes[i].toObject();
					auto comments = thread["comments"].toObject();
					auto comment  = comments["nodes"].toArray().first().toObject();
					comment.insert("body", "### Author's tour · 8 — same\r\n\r\n**Raw** explanation " + QString::number(i) + ".\r\n\r\n[Tour overview](" + prefix + "#pullrequestreview-4001)\r\n");
					comment.insert("url", prefix + "#discussion_r" + QString::number(71001 + i));
					comment.insert("fullDatabaseId", QString::number(71001 + i));
					comment.insert("updatedAt", "2026-09-26T00:00:00Z");
					comment.insert("commit", QJsonObject{ { "oid", QString::fromUtf8(aDescriptor.HeadOneOid.data(), static_cast<qsizetype>(aDescriptor.HeadOneOid.size())) } });
					comment.insert("originalCommit", comment["commit"]);
					comment.insert("line", 4);
					comment.insert("originalLine", 4);
					comment.insert("startLine", 2);
					comment.insert("originalStartLine", 2);
					thread.insert("line", 4);
					thread.insert("originalLine", 4);
					if (i == 0) {
						const auto permalink = "https://github.com/octo-org/review-repository/blob/" + QString::fromUtf8(aDescriptor.HeadOneOid.data(), static_cast<qsizetype>(aDescriptor.HeadOneOid.size())) + "/review.cpp#L2-L4";
						comment.insert("body", permalink + "\r\n\r\n" + comment["body"].toString());
						thread.insert("subjectType", "FILE");
						thread.insert("line", QJsonValue::Null);
						thread.insert("originalLine", QJsonValue::Null);
						comment.insert("line", QJsonValue::Null);
						comment.insert("originalLine", QJsonValue::Null);
						comment.insert("startLine", QJsonValue::Null);
						comment.insert("originalStartLine", QJsonValue::Null);
					}

					comment.insert("pullRequestReview", QJsonObject{ { "id", "PRR_fixture_1" } });
					comment.insert("author", QJsonObject{ { "id", "U_fixture_viewer" }, { "login", "octo-reviewer" } });
					comments.insert("nodes", QJsonArray{ comment });
					thread.insert("comments", comments);
					nodes[i] = thread;
				}
				threads.insert("nodes", nodes);
				pull.insert("reviewThreads", threads);
				repo.insert("pullRequest", pull);
				data.insert("repository", repo);
				body.insert("data", data);
				document = QJsonDocument{ body };
			} else {
				return;
			}
			aBody = document.toJson(QJsonDocument::Compact).toStdString();
		}

		void overlay_published_tour(std::string& aBody, read_operation aOperation, review_submission_fixture_state& aState)
		{
			const std::scoped_lock lock{ aState.Mutex };
			if (aState.PreviousReviews.empty() && !aState.StagedReview && !aState.StagedDescription && !aState.CheckRuns && !(aState.DraftPullRequest && (aOperation == read_operation::get_pull_request || aOperation == read_operation::search_pull_requests))) {
				return;
			}
			if (aOperation == read_operation::list_reviews && aState.ReviewHistory) {
				aBody = *aState.ReviewHistory;
				return;
			}
			auto document = QJsonDocument::fromJson(QByteArray::fromStdString(aBody));
			auto reviews  = aState.PreviousReviews;
			if (aState.StagedReview && aState.StagedReview->State == application::staged_review_state::commented) {
				reviews.push_back(*aState.StagedReview);
			}
			auto comments = aState.PreviousComments;
			if (aState.StagedReview && aState.StagedReview->State == application::staged_review_state::commented) {
				comments.insert(comments.end(), aState.StagedComments.begin(), aState.StagedComments.end());
			}
			const auto text = [](std::string_view aValue) { return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size())); };
			if (aOperation == read_operation::list_checks && aState.CheckRuns) {
				document = QJsonDocument::fromJson(QByteArray::fromStdString(*aState.CheckRuns));
			} else if (aOperation == read_operation::get_pull_request) {
				auto pull = document.object();
				if (aState.StagedDescription) {
					pull.insert("body", text(*aState.StagedDescription));
				}
				pull.insert("draft", aState.DraftPullRequest);
				document = QJsonDocument{ pull };
			} else if (aOperation == read_operation::search_pull_requests) {
				auto search = document.object();
				auto items  = search["items"].toArray();
				for (qsizetype i = 0; i < items.size(); ++i) {
					auto item = items[i].toObject();
					if (item["node_id"] == "PR_fixture_17") {
						item.insert("draft", aState.DraftPullRequest);
						items[i] = item;
					}
				}
				search.insert("items", items);
				document = QJsonDocument{ search };
			} else if (aOperation == read_operation::list_reviews) {
				auto       nodes = document.array();
				const auto seed  = nodes.first().toObject();
				for (const auto& review : reviews) {
					auto value = seed;
					value.insert("id", static_cast<qint64>(review.Id.value()));
					value.insert("node_id", text(review.NodeId));
					value.insert("body", text(review.Body));
					value.insert("state", "COMMENTED");
					value.insert("commit_id", text(core::canonical_hex(review.Commit).view()));
					value.insert("html_url", text(review.WebUrl));
					value.insert("user", QJsonObject{ { "id", static_cast<qint64>(review.Actor.DatabaseId) }, { "node_id", text(review.Actor.NodeId) }, { "login", text(review.Actor.Login) } });
					nodes.append(value);
				}
				document = QJsonDocument{ nodes };
			} else if (aOperation == read_operation::list_review_threads) {
				auto body = document.object(), data = body["data"].toObject(), repo = data["repository"].toObject(), pull = repo["pullRequest"].toObject(), threads = pull["reviewThreads"].toObject();
				auto nodes = threads["nodes"].toArray();
				for (qsizetype i = 0; i < nodes.size(); ++i) {
					auto thread     = nodes[i].toObject();
					auto connection = thread["comments"].toObject();
					auto children   = connection["nodes"].toArray();
					for (qsizetype j = 0; j < children.size(); ++j) {
						auto child = children[j].toObject();
						if (!child.contains("body")) {
							child.insert("body", child["bodyText"]);
						}
						children[j] = child;
					}
					connection.insert("nodes", children);
					thread.insert("comments", connection);
					nodes[i] = thread;
				}
				const auto seed = nodes.first().toObject();
				for (const auto& comment : comments) {
					if (comment.Reply || !comment.Source) {
						continue;
					}
					auto       thread     = seed;
					auto       connection = thread["comments"].toObject();
					auto       value      = connection["nodes"].toArray().first().toObject();
					const auto review     = std::ranges::find(reviews, comment.Review, &application::staged_review_entity::Id);
					if (review == reviews.end()) {
						continue;
					}
					const auto& source = *comment.Source;
					value.insert("id", text(comment.NodeId));
					value.insert("fullDatabaseId", QString::number(comment.Id));
					value.insert("body", text(comment.Body));
					value.insert("url", text(comment.WebUrl));
					value.insert("author", QJsonObject{ { "id", text(comment.Actor.NodeId) }, { "login", text(comment.Actor.Login) } });
					value.insert("pullRequestReview", QJsonObject{ { "id", text(review->NodeId) } });
					value.insert("commit", QJsonObject{ { "oid", text(core::canonical_hex(source.Commit).view()) } });
					value.insert("originalCommit", value["commit"]);
					value.insert("path", text(source.Path));
					value.insert("line", static_cast<int>(source.LastLine + 1));
					value.insert("originalLine", static_cast<int>(source.LastLine + 1));
					value.insert("startLine", static_cast<int>(source.FirstLine + 1));
					value.insert("originalStartLine", static_cast<int>(source.FirstLine + 1));
					value.insert("replyTo", QJsonValue::Null);
					value.insert("outdated", false);
					QJsonArray discussion{ value };
					for (const auto& reply : comments) {
						if (reply.Reply && aState.ReplyParents.contains(reply.Id) && aState.ReplyParents.at(reply.Id) == comment.Id) {
							auto child = value;
							child.insert("id", text(reply.NodeId));
							child.insert("fullDatabaseId", QString::number(reply.Id));
							child.insert("body", text(reply.Body));
							child.insert("url", text(reply.WebUrl));
							child.insert("replyTo", QJsonObject{ { "id", text(comment.NodeId) } });
							discussion.append(child);
						}
					}
					connection.insert("nodes", discussion);
					connection.insert("totalCount", discussion.size());
					connection.insert("pageInfo", QJsonObject{ { "hasNextPage", false }, { "endCursor", "published-" + text(comment.NodeId) } });
					thread.insert("comments", connection);
					thread.insert("id", "thread-" + text(comment.NodeId));
					thread.insert("path", text(source.Path));
					thread.insert("line", static_cast<int>(source.LastLine + 1));
					thread.insert("originalLine", static_cast<int>(source.LastLine + 1));
					thread.insert("diffSide", source.Side == core::diff_side::right ? "RIGHT" : "LEFT");
					thread.insert("subjectType", "LINE");
					thread.insert("isOutdated", false);
					thread.insert("isResolved", false);
					nodes.append(thread);
				}
				threads.insert("nodes", nodes);
				threads.insert("totalCount", nodes.size());
				pull.insert("reviewThreads", threads);
				repo.insert("pullRequest", pull);
				data.insert("repository", repo);
				body.insert("data", data);
				document = QJsonDocument{ body };
			} else {
				return;
			}
			aBody = document.toJson(QJsonDocument::Compact).toStdString();
		}

		/// \brief Replays essential snapshot reads before optional result collections.
		void order_snapshot_reads(std::vector<recorded_exchange>& aExchanges)
		{
			std::stable_partition(aExchanges.begin(), aExchanges.end(), [](const auto& aExchange) {
				return aExchange.Request.Operation != read_operation::list_checks
					&& aExchange.Request.Operation != read_operation::list_commit_statuses
					&& aExchange.Request.Operation != read_operation::list_pull_request_timeline;
			});
		}

		[[nodiscard]] auto load_recording(
			const fixture_descriptor& aDescriptor,
			const std::string_view    aHead)
			-> std::vector<recorded_exchange>
		{
			if (aDescriptor.Paged) {
				auto exchanges = large_review_fixture::load(std::filesystem::path{ BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT } / aDescriptor.ContractDirectory, aHead);
				order_snapshot_reads(exchanges);
				return exchanges;
			}
			const auto recordingDirectory = aHead == aDescriptor.HeadTwoOid && !aDescriptor.LaterRecordingDirectory.empty()
				? aDescriptor.LaterRecordingDirectory
				: aDescriptor.RecordingDirectory.empty()
				? aDescriptor.ContractDirectory
				: aDescriptor.RecordingDirectory;
			const auto root               = std::filesystem::path{ BPR_GITHUB_CONTRACT_FIXTURE_ROOT }
				/ recordingDirectory;
			const auto  manifest     = parse_recording(root / "manifest.json");
			const auto& interactions = manifest.Interactions;
			if (interactions.size()
				!= read_operation_catalog.size()) {
				throw std::runtime_error{ "GitHub public-path fixture is incomplete" };
			}

			auto exchanges = std::vector<recorded_exchange>{ };
			exchanges.reserve(read_operation_catalog.size());
			for (std::size_t index = 0U; index < interactions.size(); ++index) {
				const auto& interaction = interactions[index];
				const auto  operation   = read_operation_from_name(
					interaction.Operation);
				if (!operation.has_value()
					|| *operation
						!= read_operation_catalog[static_cast<std::size_t>(index)]) {
					throw std::runtime_error{
						"GitHub public-path fixture catalog is invalid"
					};
				}

				auto headers = std::vector<http_header>{ };
				headers.reserve(interaction.SafeHeaders.size());
				for (const auto& [name, value] : interaction.SafeHeaders) {
					headers.push_back({ .Name = name, .Value = value });
				}

				const auto responseFile = root
					/ interaction.ResponseFile;
				const auto method = parse_recorded_http_method(
					interaction.Method);
				if (!method.has_value()) {
					throw std::runtime_error{
						"GitHub public-path fixture method is invalid"
					};
				}
				auto requestPath  = interaction.Path;
				auto requestQuery = interaction.CanonicalQuery;
				auto requestBody  = interaction.Body.value_or(std::string{ });
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
				overlay_window_reading(responseBody, *operation, aDescriptor);
				if (aDescriptor.OrdinaryUpdate && *operation == read_operation::get_pull_request) {
					auto pull = QJsonDocument::fromJson(QByteArray::fromStdString(responseBody)).object();
					pull.insert(QStringLiteral("commits"), aHead == aDescriptor.HeadTwoOid ? 4 : 1);
					responseBody = QJsonDocument{ pull }.toJson(QJsonDocument::Compact).toStdString();
				}
				if (aDescriptor.OrdinaryUpdate && *operation == read_operation::list_pull_request_commits) {
					const auto prototype = QJsonDocument::fromJson(QByteArray::fromStdString(responseBody)).array().first().toObject();
					QJsonArray commits;
					const auto append = [&](const std::string_view aOid) {
						auto commit = prototype;
						commit.insert(QStringLiteral("sha"), QString::fromUtf8(aOid.data(), static_cast<qsizetype>(aOid.size())));
						commit.insert(QStringLiteral("node_id"), QStringLiteral("C_ordinary_") + QString::fromUtf8(aOid.data(), static_cast<qsizetype>(aOid.size())));
						commits.append(commit);
					};
					append(aDescriptor.HeadOneOid);
					if (aHead == aDescriptor.HeadTwoOid) {
						append("e9e0fbcba1f5f7ccb31b5c3734cf2f8d20d44569");
						append("c9d8dc7949a538978fb33945f2f6e482d66e14c0");
						append(aDescriptor.HeadTwoOid);
					}
					responseBody = QJsonDocument{ commits }.toJson(QJsonDocument::Compact).toStdString();
				}
				exchanges.push_back(
					{
						// [[BPR-0822]]. Name the nested types: GCC 15 destroys a braced
						// aggregate member twice when a later member initializer throws.
						.Request = adapters::github::detail::wire_request{
							.Semantic  = adapters::github::detail::semantic_read_request{ },
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
			order_snapshot_reads(exchanges);
			return exchanges;
		}

		/// \brief Three exact synthetic reviews, including one authored, and an inaccessible repository for independent windows.
		[[nodiscard]] auto independent_window_recording(const fixture_descriptor& aDescriptor, std::string_view aHead)
			-> std::vector<recorded_exchange>
		{
			if (aDescriptor.Paged || aDescriptor.Number.value() != 17U
				|| aDescriptor.PullRequestId.value() != 3001U) {
				throw std::runtime_error{ "Independent windows require the bounded synthetic review fixture" };
			}
			const auto first  = load_recording(aDescriptor, aHead);
			auto       result = first;
			for (const auto number : { 18U, 4001U }) {
				auto second = first;
				for (auto& exchange : second) {
					const auto oldPath = exchange.Request.Path;
					const auto oldBody = exchange.Request.Body;
					replace_all(exchange.Request.Path, "/pulls/17", "/pulls/" + std::to_string(number));
					replace_all(exchange.Request.Path, "/issues/17/", "/issues/" + std::to_string(number) + "/");
					replace_all(exchange.Request.Body, "\"number\":17", "\"number\":" + std::to_string(number));
					if (exchange.Request.Path == oldPath && exchange.Request.Body == oldBody) {
						continue; // Viewer, repository, checks and search have one exact shared answer.
					}
					replace_all(exchange.Response.Body, "PR_fixture_17", "PR_fixture_" + std::to_string(number));
					replace_all(exchange.Response.Body, "PRR_fixture_", "PRR_fixture_" + std::to_string(number) + "_");
					replace_all(exchange.Response.Body, "PRRT_fixture_", "PRRT_fixture_" + std::to_string(number) + "_");
					replace_all(exchange.Response.Body, "PRRC_fixture_", "PRRC_fixture_" + std::to_string(number) + "_");
					replace_all(exchange.Response.Body, "IC_fixture_", "IC_fixture_" + std::to_string(number) + "_");
					if (exchange.Request.Operation == read_operation::list_reviews) {
						auto reviews = QJsonDocument::fromJson(QByteArray::fromStdString(exchange.Response.Body)).array();
						if (reviews.size() != 1) {
							throw std::runtime_error{ "Independent review recording must contain one review" };
						}
						auto review = reviews.at(0).toObject();
						review.insert(QStringLiteral("id"), number == 18U ? 4002 : 8001);
						reviews.replace(0, review);
						exchange.Response.Body = QJsonDocument{ reviews }.toJson(QJsonDocument::Compact).toStdString();
					}
					if (exchange.Request.Operation == read_operation::get_pull_request) {
						auto body = QJsonDocument::fromJson(QByteArray::fromStdString(exchange.Response.Body)).object();
						body.insert(QStringLiteral("id"), number == 18U ? 3002 : 7001);
						body.insert(QStringLiteral("number"), static_cast<qint64>(number));
						if (number == 4001U) {
							body.insert(QStringLiteral("user"), QJsonObject{ { QStringLiteral("node_id"), QStringLiteral("U_fixture_viewer") }, { QStringLiteral("login"), QStringLiteral("octo-reviewer") } });
						}
						exchange.Response.Body = QJsonDocument{ body }.toJson(QJsonDocument::Compact).toStdString();
					}
					result.push_back(std::move(exchange));
				}
			}
			const auto repository = std::ranges::find_if(result, [](const auto& aExchange) {
				return aExchange.Request.Operation == read_operation::get_repository;
			});
			if (repository == result.end()) {
				throw std::runtime_error{ "Independent review recording must contain its repository" };
			}
			auto denied = *repository;
			replace_all(denied.Request.Path, "/review-repository", "/inaccessible-repository");
			result.push_back(std::move(denied));
			return result;
		}

		/// \brief Replays one exact request without consuming another window's position.
		[[nodiscard]] auto perform_independent_read(
			const std::vector<recorded_exchange>&         aExchanges,
			const adapters::github::detail::wire_request& aRequest)
			-> adapters::github::detail::response_result
		{
			const auto found = std::ranges::find_if(aExchanges, [&aRequest](const recorded_exchange& aExchange) {
				const auto& expected = aExchange.Request;
				return expected.Operation == aRequest.Operation && expected.Method == aRequest.Method
					&& expected.Server == aRequest.Server && expected.Path == aRequest.Path
					&& expected.Query == aRequest.Query && expected.Body == aRequest.Body;
			});
			// Keep the ordinary strict validator authoritative for headers, semantic
			// request validity and response bounds, including unknown-request refusal.
			auto replay   = strict_recorded_transport{ found == aExchanges.end()
														   ? std::vector<recorded_exchange>{ }
														   : std::vector<recorded_exchange>{ *found } };
			auto response = replay.perform(aRequest);
			// The strict recording validates successful response shapes. Overlay
			// this refusal only after its exact outbound request has passed replay.
			if (response && aRequest.Operation == read_operation::get_repository
				&& aRequest.Path == "/repos/octo-org/inaccessible-repository") {
				response->Status = 403;
				response->Body   = R"({"message":"Resource not accessible"})";
			}
			return response;
		}

		/// \brief Builds the recording a signed-out read replays.
		///
		/// \details One recording rather than a second fixture. Every REST body
		/// below is the body the authenticated script already serves, re-ordered
		/// into the order the anonymous authority issues its operations and cut
		/// down to the operations it may issue: viewer identity and the GraphQL threads
		/// are absent because that authority refuses both before a request
		/// exists, so a recording that carried them could only be reached by a
		/// read that had stopped being anonymous.
		///
		/// The inline review conversation and update timeline are not in the
		/// authenticated baseline script. Their requests are derived from the
		/// recorded reviews request and checked by the exact replay transport.
		/// Deriving it rather than writing it out is what keeps the two in step;
		/// a compiled path that ever stopped matching fails exact replay
		/// rather than passing against a hand-copied constant.
		[[nodiscard]] auto load_anonymous_recording(
			const fixture_descriptor& aDescriptor,
			const std::string_view    aHead,
			const bool                aEmptyTimeline,
			const bool aUnfetchableFirstHead, const std::string& aTimelineCourse)
			-> std::vector<recorded_exchange>
		{
			const auto authenticated = load_recording(aDescriptor, aHead);
			const auto recorded      = [&authenticated](
										   const read_operation aOperation)
				-> const recorded_exchange& {
				const auto match = std::ranges::find(
					authenticated,
					aOperation,
					[](const recorded_exchange& aExchange) {
						return aExchange.Request.Operation;
					});
				if (match == authenticated.end()) {
					throw std::runtime_error{
						"GitHub anonymous fixture is missing a recorded operation"
					};
				}
				return *match;
			};

			auto exchanges = std::vector<recorded_exchange>{ };
			exchanges.reserve(anonymous_read_operations.size() + 1U);
			for (const auto operation : anonymous_read_operations) {
				if (operation != read_operation::list_review_comments) {
					exchanges.push_back(recorded(operation));
					continue;
				}
				auto comments = recorded(
					read_operation::list_reviews);
				constexpr std::string_view reviews = "/reviews";
				if (!comments.Request.Path.ends_with(reviews)) {
					throw std::runtime_error{
						"GitHub anonymous fixture cannot derive its conversation path"
					};
				}
				comments.Request.Path.resize(comments.Request.Path.size()
											 - reviews.size());
				comments.Request.Path += "/comments";
				comments.Request.Operation = read_operation::list_review_comments;
				// The promoted live body, byte for byte. It carries no placeholder
				// to substitute because it was captured rather than synthesized.
				comments.Response.Body = read_all(
					std::filesystem::path{ BPR_RFC0017_ANONYMOUS_FIXTURE_ROOT }
					/ "captures" / "pull-request-comments.json");
				if (aDescriptor.WindowReading) {
					QJsonArray nodes;
					for (const auto& path : { QStringLiteral("review.cpp"), QStringLiteral("secondary.cpp") }) {
						nodes.append(QJsonObject{ { QStringLiteral("id"), 71001 + nodes.size() }, { QStringLiteral("body"), QStringLiteral("Check the exact retained reading position.") }, { QStringLiteral("path"), path }, { QStringLiteral("line"), 2 }, { QStringLiteral("original_line"), 2 }, { QStringLiteral("position"), 2 }, { QStringLiteral("commit_id"), QString::fromStdString(std::string{ aHead }) }, { QStringLiteral("original_commit_id"), QString::fromStdString(std::string{ aHead }) }, { QStringLiteral("user"), QJsonObject{ { QStringLiteral("node_id"), QStringLiteral("U_fixture_viewer") }, { QStringLiteral("login"), QStringLiteral("octo-reviewer") } } } });
					}
					comments.Response.Body = QJsonDocument{ nodes }.toJson(QJsonDocument::Compact).toStdString();
				}
				exchanges.push_back(std::move(comments));
			}
			// The authenticated script's count check has a mirror here rather than
			// an exception: a script that grew or lost an operation must fail the
			// same way on both authorities.
			if (exchanges.size() != anonymous_read_operations.size()) {
				throw std::runtime_error{
					"GitHub anonymous fixture is incomplete"
				};
			}
			// The independent REST read follows the completed baseline snapshot.
			order_snapshot_reads(exchanges);
			// A→B→A exercises repeated-head identity through the composed path.
			auto       timeline = recorded(read_operation::list_reviews);
			const auto pulls    = timeline.Request.Path.find("/pulls/");
			if (pulls == std::string::npos
				|| !timeline.Request.Path.ends_with("/reviews")) {
				throw std::runtime_error{
					"GitHub anonymous timeline fixture cannot derive its path"
				};
			}
			timeline.Request.Path.replace(pulls, 7U, "/issues/");
			timeline.Request.Path.resize(
				timeline.Request.Path.size() - std::string_view{ "/reviews" }.size());
			timeline.Request.Path += "/timeline";
			timeline.Request.Operation = read_operation::list_pull_request_timeline;
			const auto priorHead       = aHead == aDescriptor.HeadOneOid
					&& !aDescriptor.HeadTwoOid.empty()
				? aDescriptor.HeadTwoOid
				: aDescriptor.HeadOneOid;
			const auto event           = [](const int aId, const std::string_view aOid,
											const QString& aTime) {
				return QJsonObject{
					{ QStringLiteral("id"), aId },
					{ QStringLiteral("event"), QStringLiteral("head_ref_force_pushed") },
					{ QStringLiteral("commit_id"), QString::fromUtf8(aOid.data(), static_cast<qsizetype>(aOid.size())) },
					{ QStringLiteral("created_at"), aTime },
				};
			};
			auto events = QJsonArray{ };
			if (!aDescriptor.HeadTwoOid.empty() && !aEmptyTimeline) {
				constexpr auto missingHead         = std::string_view{ "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee" };
				const auto     firstHistoricalHead = aTimelineCourse == "missing-base" || aTimelineCourse == "rewritten-base"
					? disconnected_history_head
					: aUnfetchableFirstHead ? missingHead
											: aHead;
				events.append(event(91'701, firstHistoricalHead,
									QStringLiteral("2026-08-01T16:45:08Z")));
				events.append(event(91'702, priorHead, QStringLiteral("2026-08-05T19:12:33Z")));
			}
			if (!aEmptyTimeline) {
				events.append(event(91'703, aHead, QStringLiteral("2026-08-09T22:19:26Z")));
			}
			timeline.Response.Body = QJsonDocument{ events }.toJson(QJsonDocument::Compact).toStdString();
			exchanges.push_back(std::move(timeline));
			return exchanges;
		}

		/// \brief Reports whether a request carries a credential of any kind.
		///
		/// \details The anonymous role refuses one rather than ignoring it. The
		/// contract validator already rejects these two names on the way out, so
		/// this is the second of two independent refusals and the only one on the
		/// serving side: a transport that started attaching a header would be
		/// answered by the recording it asked for, not by the recording it earned.
		[[nodiscard]] auto carries_credential(
			const std::vector<http_header>& aHeaders) -> bool
		{
			constexpr auto named = [](const std::string_view aLeft,
									  const std::string_view aRight) {
				return std::ranges::equal(
					aLeft,
					aRight,
					[](const char aFirst, const char aSecond) {
						const auto fold = [](const char aCharacter) {
							return aCharacter >= 'A' && aCharacter <= 'Z'
								? static_cast<char>(aCharacter - 'A' + 'a')
								: aCharacter;
						};
						return fold(aFirst) == fold(aSecond);
					});
			};
			return std::ranges::any_of(
				aHeaders,
				[named](const http_header& aHeader) {
					return named(aHeader.Name, "Authorization")
						|| named(aHeader.Name, "Proxy-Authorization");
				});
		}

		[[nodiscard]] auto load_capability_response(const std::string_view aCapabilityDirectory) -> std::array<std::string, adapters::github::detail::capability_probe_part_count>
		{
			std::array<std::string, adapters::github::detail::capability_probe_part_count> bodies;
			for (std::size_t part = 0U; part < bodies.size(); ++part) {
				bodies[part] = read_all(std::filesystem::path{ BPR_GITHUB_CAPABILITY_FIXTURE_ROOT }.parent_path() / "v2-parts" / aCapabilityDirectory / ("supported-" + std::to_string(part) + ".json"));
			}
			return bodies;
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

		/// \brief Keeps legacy strict single-page validation behind an explicit replay choice.
		class fixture_replay final
		{
		public:
			explicit fixture_replay(std::vector<recorded_exchange> aExchanges, bool aPaged = false, std::string aTimelineCourse = { })
				: Timeline{ aPaged ? std::optional<recorded_exchange>{ }
								   : take_timeline(aExchanges) }
				, TimelineCourse{ std::move(aTimelineCourse) }
				, Strict{ aPaged ? std::vector<recorded_exchange>{ } : std::exchange(aExchanges, { }) }
			{
				if (aPaged) {
					Paged.emplace(std::move(aExchanges));
				}
			}
			[[nodiscard]] auto perform(const adapters::github::detail::wire_request& aRequest)
				-> adapters::github::detail::response_result
			{
				if (!Paged && aRequest.Operation == read_operation::list_pull_request_timeline) {
					using namespace adapters::github::detail;
					if (const auto valid = validate_outbound_read_request(aRequest);
						!valid.has_value()) {
						return std::unexpected{ valid.error() };
					}
					if (!Timeline || TimelineConsumed || !Strict.consumed_all()) {
						return std::unexpected{ contract_failure{
							.Code = contract_failure_code::unexpected_operation } };
					}
					const auto& expected = Timeline->Request;
					if (aRequest.Server != expected.Server
						|| aRequest.Operation != expected.Operation
						|| aRequest.Method != expected.Method
						|| aRequest.Path != expected.Path
						|| aRequest.Query != (TimelinePages == 0U ? expected.Query : "page=" + std::to_string(TimelinePages + 1U) + "&per_page=100")
						|| aRequest.Headers != expected.Headers
						|| aRequest.Body != expected.Body) {
						return std::unexpected{ contract_failure{
							.Code = contract_failure_code::unexpected_path } };
					}
					++TimelinePages;
					QTextStream{ stdout } << "PUBLIC_PREVIEW_TIMELINE_PAGE:" << TimelinePages << '\n';
					TimelineConsumed = true;
					auto response    = Timeline->Response;
					if (TimelineCourse == "failed" || TimelineCourse == "refused") {
						response.Status = TimelineCourse == "failed" ? 500 : 403;
						response.Body   = "{\"message\":\"fixture timeline unavailable\"}";
					} else if (TimelineCourse == "capped") {
						TimelineConsumed = TimelinePages == 10U;
						auto rows        = QJsonArray{ };
						for (int index = 0; index < 100; ++index) {
							rows.append(QJsonObject{ { QStringLiteral("event"), QStringLiteral("committed") } });
						}
						response.Body         = QJsonDocument{ rows }.toJson(QJsonDocument::Compact).toStdString();
						response.Completeness = page_completeness::has_next_page;
						response.Headers.push_back({ .Name = "Link", .Value = "<https://api.github.com" + expected.Path + "?page=" + std::to_string(TimelinePages + 1U) + "&per_page=100>; rel=\"next\"" });
					}
					return response;
				}
				auto response = Paged ? Paged->perform(aRequest) : Strict.perform(aRequest);
				if (TimelineCourse == "ordinary" && response && aRequest.Operation == read_operation::list_pull_request_commits) {
					QTextStream{ stdout } << "ORDINARY_SURVIVING_COMMITS:" << QJsonDocument::fromJson(QByteArray::fromStdString(response->Body)).array().size() << '\n';
				}
				return response;
			}
			[[nodiscard]] auto consumed_all() const noexcept -> bool
			{
				return Paged ? Paged->consumed_all()
							 : Strict.consumed_all() && (!Timeline || TimelineConsumed);
			}
			void replace_completed_paged_script(std::vector<recorded_exchange> aExchanges)
			{
				if (!Paged || !Paged->consumed_all()) {
					throw std::logic_error{ "Only a completed paged fixture may advance" };
				}
				Paged.emplace(std::move(aExchanges));
			}
			void rewind() noexcept
			{
				TimelineConsumed = false;
				TimelinePages    = 0U;
				if (Paged) {
					Paged->rewind();
				} else {
					Strict.rewind();
				}
			}
			/// [[BPR-0657]]. Replays the strict script from the exchange one
			/// operation starts at, for a refresh; a paged script has its own
			/// history and is left alone unless the rate-limit course explicitly
			/// repeats that same head.
			void replay_from(const read_operation aOperation, const bool aRepeatPaged = false) noexcept
			{
				if (Paged) {
					if (aRepeatPaged) {
						Paged->replay_from(aOperation);
					}
					return;
				}
				Strict.rewind();
				TimelineConsumed = false;
				TimelinePages    = 0U;
				Strict.skip_until(aOperation);
			}

		private:
			[[nodiscard]] static auto take_timeline(std::vector<recorded_exchange>& aExchanges)
				-> std::optional<recorded_exchange>
			{
				if (aExchanges.empty() || aExchanges.back().Request.Operation != read_operation::list_pull_request_timeline) {
					return std::nullopt;
				}
				auto timeline = std::move(aExchanges.back());
				aExchanges.pop_back();
				return timeline;
			}
			std::optional<recorded_exchange>                       Timeline;
			bool                                                   TimelineConsumed = false;
			std::size_t                                            TimelinePages    = 0U;
			std::string                                            TimelineCourse;
			strict_recorded_transport                              Strict;
			std::optional<large_review_fixture::sequential_replay> Paged;
		};

		[[nodiscard]] auto history_heads(const fixture_descriptor& aDescriptor, bool aEnabled) -> std::vector<std::string>
		{
			if (!aEnabled) {
				return { };
			}
			if (!aDescriptor.Paged) {
				throw std::runtime_error{ "History replay requires the stored large-review fixture" };
			}
			const auto               manifest = large_review_fixture::read_json(std::filesystem::path{ BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT } / aDescriptor.ContractDirectory / "manifest.json");
			std::vector<std::string> heads;
			for (const auto& head : manifest["heads"].get_array()) {
				heads.push_back(head.get_string());
			}
			if (heads.size() != 32U || heads.back() != aDescriptor.HeadOneOid) {
				throw std::runtime_error{ "History replay head inventory mismatch" };
			}
			return heads;
		}

		struct fixture_recording_state
		{
			fixture_recording_state(
				const fixture_descriptor&                              aDescriptor,
				const adapters::github::validated_github_host_profile& aSelectedProfile,
				std::shared_ptr<deterministic_git_fixture>             aGit,
				const github_epoch_fixture_phase aPhase, bool aHistory, bool aIndependentWindowReads,
				bool aEmptyTimeline, bool aUnfetchableFirstHead, const std::string& aTimelineCourse)
				: Descriptor{ aDescriptor }
				, Profile{ aSelectedProfile }
				, Git{ std::move(aGit) }
				, IndependentWindowReads{ aIndependentWindowReads }
				, RewrittenBase{ aTimelineCourse == "rewritten-base" }
				, IndependentHead{ aPhase == github_epoch_fixture_phase::reuse ? aDescriptor.HeadTwoOid : aDescriptor.HeadOneOid }
				, IndependentRecording{ aIndependentWindowReads ? independent_window_recording(aDescriptor, IndependentHead) : std::vector<recorded_exchange>{ } }
				, IndependentBootstrap{ aIndependentWindowReads ? load_bootstrap_recording(aDescriptor) : std::vector<recorded_exchange>{ } }
				, HistoryHeads{ history_heads(aDescriptor, aHistory) }
				, BootstrapRecording{ load_bootstrap_recording(Descriptor) }
				, Recording{ [&] {
								auto result = load_recording(
									Descriptor,
									!HistoryHeads.empty() ? std::string_view{ HistoryHeads.front() }
										: aPhase == github_epoch_fixture_phase::reuse
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
							}(),
							 Descriptor.Paged }
				, AnonymousRecording{ Descriptor.Paged ? std::vector<recorded_exchange>{ } : load_anonymous_recording(Descriptor, aPhase == github_epoch_fixture_phase::reuse && !RewrittenBase ? Descriptor.HeadTwoOid : Descriptor.HeadOneOid, aEmptyTimeline, aUnfetchableFirstHead, aTimelineCourse), false, aTimelineCourse }
				, CapabilityBody{ load_capability_response(
					  Descriptor.CapabilityDirectory) }
			{
			}

			[[nodiscard]] auto advance_history(const adapters::github::detail::wire_request& aRequest, std::unique_lock<std::mutex>& aStateLock) -> bool
			{
				if (HistoryHeads.empty() || HistoryIndex + 1U == HistoryHeads.size()
					|| !Recording.consumed_all() || aRequest.Operation != read_operation::get_repository) {
					return true;
				}
				// Compile only the next refresh, outside the evidence mutex so GUI
				// observations cannot block on synthetic JSON construction. The caller
				// serializes history compilation before acquiring this state lock.
				const auto nextIndex       = HistoryIndex + 1U;
				const auto expectedSession = AuthenticationSession;
				aStateLock.unlock();
				if (HistoryCompileInterleave) {
					HistoryCompileInterleave();
				}
				auto exchanges = load_recording(Descriptor, HistoryHeads[nextIndex]);
				aStateLock.lock();
				if (AuthenticationSession != expectedSession) {
					return false;
				}
				exchanges.erase(exchanges.begin()); // /user belongs to initial session establishment.
				if (HistoryIndex + 1U != nextIndex || !Recording.consumed_all() || exchanges.front().Request != aRequest) {
					return true;
				}
				Recording.replace_completed_paged_script(std::move(exchanges));
				++HistoryIndex;
				return true;
			}

			[[nodiscard]] auto serves_head(std::string_view aHead) const -> bool
			{
				return aHead == disconnected_history_head || aHead == Descriptor.HeadOneOid || (!Descriptor.HeadTwoOid.empty() && aHead == Descriptor.HeadTwoOid)
					|| std::ranges::find(HistoryHeads, aHead) != HistoryHeads.end();
			}

			// Fixed at construction and read from the transport threads without the
			// mutex, which the const keeps true.
			const fixture_descriptor                        Descriptor;
			adapters::github::validated_github_host_profile Profile;
			std::shared_ptr<deterministic_git_fixture>      Git;
			const bool                                      IndependentWindowReads;
			const bool                                      RewrittenBase;
			const std::string_view                          IndependentHead;
			const std::vector<recorded_exchange>            IndependentRecording;
			const std::vector<recorded_exchange>            IndependentBootstrap;
			const std::vector<std::string>                  HistoryHeads;
			std::size_t                                     HistoryIndex = 0U;
			std::mutex                                      HistoryCompileMutex;
			// Set only by the deterministic standalone session-race regression.
			std::function<void()>                HistoryCompileInterleave;
			std::function<void(std::stop_token)> BeforeFilesPage;
			std::function<void(std::stop_token)> BeforeViewerIdentity;
			fixture_replay                       BootstrapRecording;
			fixture_replay                       Recording;
			// The signed-out script. A separate cursor as well as a separate
			// order: the two authorities never share a position, so a course that
			// ran both would not be able to advance one by driving the other.
			fixture_replay                                                                 AnonymousRecording;
			std::array<std::string, adapters::github::detail::capability_probe_part_count> CapabilityBody;
			std::size_t                                                                    CapabilityCalls             = 0U;
			bool                                                                           CapabilityDiagnosticFailure = false;
			application::diagnostic_summary                                                CapabilityFailureSummary;
			std::size_t                                                                    BootstrapFactoryCalls    = 0U;
			std::size_t                                                                    BootstrapFactoryAttempts = 0U;
			std::size_t                                                                    AccountFactoryAttempts   = 0U;
			std::size_t                                                                    AccountFactoryCalls      = 0U;
			// [[BPR-0536]]. Both factories are once-only, which is what makes
			// "the transport was selected exactly once" a claim the other
			// courses can make. The refresh course establishes a session more
			// than once by construction -- the device grant enrolls after the
			// fixture's own enrollment, and every rotation enrolls again under
			// the same authority -- so under that course, and only there, the
			// recorded transports serve each of them. The counts are still
			// kept, so a course that reads them still reads the truth.
			bool                                             RepeatSessions          = false;
			bool                                             LargeReviewForeignPosts = false;
			bool                                             ReviewTourCourse        = false;
			std::shared_ptr<review_submission_fixture_state> PublishedTour;
			bool                                             LargeReviewRepeatReads = false;
			bool                                             CliEnabled             = false;
			std::shared_ptr<std::atomic_bool>                CliStrictCapability;
			bool                                             RefuseFirstSmartAcquire  = false;
			bool                                             FirstSmartAcquireRefused = false;
			std::optional<core::application_session_id>      AuthenticationSession;
			// [[BPR-0657]]. A refresh replays the recorded read script from
			// its start once it is consumed, so the product's re-read is
			// served with the mutation overlay rather than refused.
			bool                                        RepeatReads = false;
			std::shared_ptr<std::atomic<std::uint64_t>> RecentAccount;
			bool                                        DistinctAccount      = false;
			bool                                        UnboundViewer        = false;
			std::size_t                                 AccountReadCalls     = 0U;
			std::size_t                                 SmartAcquireCalls    = 0U;
			std::size_t                                 SmartAcquireRefusals = 0U;
			std::size_t                                 SmartDiscoveryCalls  = 0U;
			std::size_t                                 SmartLsRefsCalls     = 0U;
			std::size_t                                 SmartFetchCalls      = 0U;
			std::optional<core::pull_request_authority> LastSmartAuthority;
			int                                         LastReadOperation = -1;
			int                                         RecordingFailure  = -1;
			// [[BPR-0657]]. What the fixture's mutation provider changed on
			// its threads, overlaid on the recorded review-thread answers so
			// the projection the app reads shows a reply under its thread and
			// a thread resolved or unresolved, as GitHub would on the next
			// read.
			struct thread_overlay
			{
				std::vector<std::pair<std::string, std::string>> Replies;
				std::optional<bool>                              Resolved;
				std::optional<std::string>                       RootBody;
				/// [[BPR-1085]]. The root post's author, as GitHub's identity and login.
				std::optional<std::pair<std::string, std::string>> RootAuthor;
				/// [[BPR-1374]]. Another client deleted the thread's root, so the
				/// thread is gone from every later read.
				bool Deleted = false;
			};
			std::map<std::string, thread_overlay> ThreadOverlays;
			std::vector<QJsonObject>              CreatedReviewThreads;
			// [[BPR-0990]]. The repository's open pull requests discovery
			// searches, and what it asked.
			std::optional<github_personal_discovery_fixture>                  PersonalDiscovery;
			std::shared_ptr<github_rate_limit_fixture>                        RateLimit;
			std::shared_ptr<github_offline_fixture>                           Offline;
			std::shared_ptr<github_read_delay_fixture>                        ReadDelay;
			std::size_t                                                       DiscoverySearches    = 0U;
			std::size_t                                                       DiscoveryDetailReads = 0U;
			std::vector<std::string>                                          DiscoverySearchTerms;
			std::vector<std::string>                                          AccountReadOperations;
			std::vector<adapters::github::detail::github_wire_parent_context> SnapshotReads;
			mutable std::mutex                                                Mutex;
		};

		/// [[BPR-0990]]. One answer of the fixture server to a personal
		/// discovery read: the search evaluated over the repository's open pull
		/// requests, or one pull request's requested reviewers.
		[[nodiscard]] auto personal_discovery_response(const fixture_recording_state& aState, const adapters::github::detail::wire_request& aRequest)
			-> adapters::github::detail::wire_response
		{
			const auto& fixture  = *aState.PersonalDiscovery;
			const auto& semantic = aRequest.Semantic;
			auto        response = adapters::github::detail::wire_response{ .Status = 200, .Completeness = page_completeness::complete };
			response.Headers.push_back({ .Name = "Content-Type", .Value = "application/json; charset=utf-8" });
			const auto ours = semantic.Owner == aState.Descriptor.Owner && semantic.Repository == aState.Descriptor.Name;
			const auto fold = [](std::string aText) {
				std::ranges::transform(aText, aText.begin(), [](const unsigned char aCharacter) { return static_cast<char>(std::tolower(aCharacter)); });
				return aText;
			};
			if (aRequest.Operation == read_operation::get_pull_request || aRequest.Operation == read_operation::list_reviews) {
				const auto found = std::ranges::find(fixture.OpenPullRequests, semantic.PullRequestNumber, &github_personal_discovery_fixture::pull_request::Number);
				if (!ours || found == fixture.OpenPullRequests.end()) {
					response.Status = 404;
					response.Body   = R"({"message":"Not Found"})";
					return response;
				}
				const auto head = found->WaitingDetails ? QString::fromUtf8(aState.Descriptor.HeadOneOid) : QString{ 40, QLatin1Char('a') };
				if (aRequest.Operation == read_operation::list_reviews) {
					auto reviews = QJsonArray{ };
					if (found->ReviewedByViewer && found->ReviewedByViewer->load()) {
						reviews.append(QJsonObject{
							{ QStringLiteral("id"), 71001 },
							{ QStringLiteral("node_id"), QStringLiteral("R_home_review") },
							{ QStringLiteral("user"), QJsonObject{ { QStringLiteral("login"), QStringLiteral("octo-reviewer") }, { QStringLiteral("node_id"), QStringLiteral("U_fixture_viewer") } } },
							{ QStringLiteral("state"), QStringLiteral("APPROVED") },
							{ QStringLiteral("commit_id"), head },
							{ QStringLiteral("body"), QStringLiteral("Approved") },
						});
					}
					response.Body = QJsonDocument{ reviews }.toJson(QJsonDocument::Compact).toStdString();
					return response;
				}
				auto       users           = QJsonArray{ };
				const auto requestsPresent = !found->ReviewerRequestsPresent || found->ReviewerRequestsPresent->load();
				for (const auto& login : found->RequestedUsers) {
					if (!requestsPresent) {
						break;
					}
					users.append(QJsonObject{ { QStringLiteral("login"), QString::fromStdString(login) }, { QStringLiteral("node_id"), QStringLiteral("U_") + QString::fromStdString(login) } });
				}
				auto teams = QJsonArray{ };
				for (std::size_t team = 0U; team < (requestsPresent ? found->RequestedTeams : 0U); ++team) {
					teams.append(QJsonObject{ { QStringLiteral("slug"), QStringLiteral("team-%1").arg(team) } });
				}
				const auto number = QString::number(found->Number);
				response.Body     = QJsonDocument{ QJsonObject{
													   { QStringLiteral("number"), static_cast<qint64>(found->Number) },
													   { QStringLiteral("title"), discovered_title(number) },
													   { QStringLiteral("draft"), found->Draft && found->Draft->load() },
													   { QStringLiteral("state"), QStringLiteral("open") },
													   { QStringLiteral("url"), QString::fromStdString(std::string{ aState.Profile.rest_root() }) + QStringLiteral("/repos/%1/%2/pulls/%3").arg(QString::fromStdString(semantic.Owner), QString::fromStdString(semantic.Repository), number) },
													   { QStringLiteral("user"), QJsonObject{ { QStringLiteral("login"), QString::fromStdString(found->Author) }, { QStringLiteral("node_id"), found->Author == "octo-reviewer" ? QStringLiteral("U_fixture_viewer") : QStringLiteral("U_") + QString::fromStdString(found->Author) } } },
													   { QStringLiteral("head"), QJsonObject{ { QStringLiteral("sha"), head } } },
													   { QStringLiteral("base"), QJsonObject{ { QStringLiteral("repo"), QJsonObject{ { QStringLiteral("id"), static_cast<qint64>(aState.Descriptor.RepositoryId.value()) }, { QStringLiteral("full_name"), QString::fromStdString(semantic.Owner + "/" + semantic.Repository) } } } } },
													   { QStringLiteral("requested_reviewers"), users },
													   { QStringLiteral("requested_teams"), teams },
												   } }
										.toJson(QJsonDocument::Compact)
										.toStdString();
				{
					// [[BPR-1407]]. Each row's age, size and conversation, so Home
					// has a large, a small and a middling pull request to show.
					auto       document = QJsonDocument::fromJson(QByteArray::fromStdString(response.Body)).object();
					const auto large = found->Number == 4001U, small = found->Number == 4002U;
					document.insert(QStringLiteral("created_at"), large ? QStringLiteral("2026-09-21T16:04:00Z") : small ? QStringLiteral("2026-10-12T08:30:00Z")
																														 : QStringLiteral("2026-10-06T11:15:00Z"));
					document.insert(QStringLiteral("additions"), large ? 1204 : small ? 18
																					  : 236);
					document.insert(QStringLiteral("deletions"), large ? 318 : small ? 3
																					 : 41);
					document.insert(QStringLiteral("comments"), large ? 9 : small ? 0
																				  : 1);
					document.insert(QStringLiteral("review_comments"), large ? 5 : small ? 0
																						 : 2);
					response.Body = QJsonDocument{ document }.toJson(QJsonDocument::Compact).toStdString();
				}
				if (found->WaitingDetails) {
					auto document = QJsonDocument::fromJson(QByteArray::fromStdString(response.Body)).object();
					document.insert("node_id", "PR_fixture_17");
					document.insert("user", QJsonObject{ { "node_id", found->Author == "octo-reviewer" ? "U_fixture_viewer" : "U_fixture_author" }, { "login", QString::fromStdString(found->Author) } });
					document.insert("base", QJsonObject{ { "sha", QString::fromUtf8(aState.Descriptor.BaseOid) }, { "ref", "main" }, { "repo", QJsonObject{ { "id", static_cast<qint64>(aState.Descriptor.RepositoryId.value()) }, { "node_id", "R_fixture_repository" }, { "full_name", QString::fromStdString(semantic.Owner + "/" + semantic.Repository) } } } });
					response.Body = QJsonDocument{ document }.toJson(QJsonDocument::Compact).toStdString();
				}
				return response;
			}
			if (aRequest.Operation == read_operation::list_requested_reviewers) {
				const auto found = std::ranges::find(fixture.OpenPullRequests, semantic.PullRequestNumber, &github_personal_discovery_fixture::pull_request::Number);
				auto       users = QJsonArray{ };
				auto       teams = QJsonArray{ };
				if (ours && found != fixture.OpenPullRequests.end() && (!found->ReviewerRequestsPresent || found->ReviewerRequestsPresent->load())) {
					for (const auto& login : found->RequestedUsers) {
						users.append(QJsonObject{ { QStringLiteral("login"), QString::fromStdString(login) }, { QStringLiteral("node_id"), QStringLiteral("U_") + QString::fromStdString(login) } });
					}
					for (std::size_t team = 0U; team < found->RequestedTeams; ++team) {
						teams.append(QJsonObject{ { QStringLiteral("slug"), QStringLiteral("team-%1").arg(team) }, { QStringLiteral("node_id"), QStringLiteral("T_%1").arg(team) } });
					}
				}
				response.Body = QJsonDocument{ QJsonObject{ { QStringLiteral("users"), users }, { QStringLiteral("teams"), teams } } }.toJson(QJsonDocument::Compact).toStdString();
				return response;
			}
			const auto terms     = std::string_view{ semantic.SearchTerms };
			const auto separator = terms.find(':');
			const auto qualifier = terms.substr(0U, separator);
			const auto login     = fold(std::string{ terms.substr(separator + 1U) });
			auto       items     = QJsonArray{ };
			auto       total     = 0;
			const auto root      = QString::fromStdString(std::string{ aState.Profile.rest_root() });
			const auto path      = QStringLiteral("/repos/%1/%2").arg(QString::fromUtf8(aState.Descriptor.Owner.data(), static_cast<qsizetype>(aState.Descriptor.Owner.size())), QString::fromUtf8(aState.Descriptor.Name.data(), static_cast<qsizetype>(aState.Descriptor.Name.size())));
			for (const auto& pullRequest : ours ? fixture.OpenPullRequests : std::vector<github_personal_discovery_fixture::pull_request>{ }) {
				// An author is never asked to review their own pull request.
				const auto matches = qualifier == "author"
					? fold(pullRequest.Author) == login
					: fold(pullRequest.Author) != login
						&& (pullRequest.RequestedTeams != 0U || std::ranges::any_of(pullRequest.RequestedUsers, [&](const std::string& aUser) { return fold(aUser) == login; }));
				if (!matches) {
					continue;
				}
				++total;
				if (items.size() >= 100) {
					continue;
				}
				const auto number = QString::number(pullRequest.Number);
				items.append(QJsonObject{
					{ QStringLiteral("number"), static_cast<qint64>(pullRequest.Number) },
					{ QStringLiteral("title"), discovered_title(number) },
					{ QStringLiteral("state"), QStringLiteral("open") },
					{ QStringLiteral("draft"), pullRequest.Draft && pullRequest.Draft->load() },
					{ QStringLiteral("pull_request"), QJsonObject{ { QStringLiteral("url"), root + path + QStringLiteral("/pulls/") + number } } },
					{ QStringLiteral("repository_url"), root + path },
					{ QStringLiteral("user"), QJsonObject{ { QStringLiteral("login"), QString::fromStdString(pullRequest.Author) }, { QStringLiteral("node_id"), QStringLiteral("U_") + QString::fromStdString(pullRequest.Author) } } },
				});
			}
			response.Body = QJsonDocument{ QJsonObject{ { QStringLiteral("total_count"), total }, { QStringLiteral("incomplete_results"), false }, { QStringLiteral("items"), items } } }
								.toJson(QJsonDocument::Compact)
								.toStdString();
			return response;
		}

		enum class fixture_transport_role {
			bootstrap,
			account,
			// RFC-BPR-0017's credential-free read. It replays the same
			// recording the account role replays, because the recorded public
			// pull request is the same pull request either way; what differs
			// is that nothing selected this transport with a credential and
			// nothing can, so the authenticated evidence counters stay
			// untouched by anything it serves.
			anonymous,
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
			// [[BPR-0667]]. A real remote advertises where the base branch is
			// now, which is ahead of the base the pull request recorded as
			// soon as anything else merges. Advertising the recorded base
			// here made every course agree with a client that required them
			// to be equal, and hid a product that could not open a pull
			// request on any active repository.
			constexpr auto movedBaseBranch
				= std::string_view{ "a1b2c3d4e5f60718293a4b5c6d7e8f90a1b2c3d4" };
			append_packet(
				result,
				std::string{ movedBaseBranch } + " "
					+ std::string{ aContext.BaseRef.view() } + "\n");
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
				if (State->Offline && State->Offline->Offline.load()) {
					++State->Offline->GitRefusals;
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::unavailable } };
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
				if (State->Offline && State->Offline->Offline.load()) {
					++State->Offline->GitRefusals;
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::unavailable } };
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
				if (State->Offline && State->Offline->Offline.load()) {
					++State->Offline->GitRefusals;
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::unavailable } };
				}
				if (aRequest.Context != Context) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::
							invalid_request } };
				}
				const auto requestBody = std::string{
					reinterpret_cast<const char*>(aRequest.Body.data()), aRequest.Body.size()
				};
				const auto& fixture = State->Descriptor;
				const auto  wants   = [&](const std::string_view aHead) {
					return !aHead.empty() && requestBody.find(std::string{ "want " } + std::string{ aHead } + "\n") != std::string::npos;
				};
				// The stored large-review pack contains every admitted history head.
				// Require the one exact head from this acquired context, never an
				// arbitrary or additional member of the fixture's inventory.
				const auto contextHead = core::canonical_hex(Context.Revision.Head);
				const auto historical  = !State->HistoryHeads.empty();
				if (historical && (std::ranges::find(State->HistoryHeads, contextHead.view()) == State->HistoryHeads.end() || !large_review_fixture::exact_history_fetch_wants(requestBody, core::canonical_hex(Context.Revision.Base).view(), contextHead.view()))) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::invalid_request } };
				}
				const auto wantsFirst        = historical || wants(fixture.HeadOneOid);
				const auto wantsSecond       = wants(fixture.HeadTwoOid);
				const auto wantsDisconnected = wants(disconnected_history_head);
				if (static_cast<int>(wantsFirst) + static_cast<int>(wantsSecond) + static_cast<int>(wantsDisconnected) != 1
					&& !(State->RewrittenBase && wants(fixture.BaseOid) && !wantsDisconnected)) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::invalid_request } };
				}
				const auto& pack = wantsDisconnected
					? State->Git->PackWithoutBase
					: wantsSecond
					? State->Git->PackTwo
					: State->Git->PackOne;
				if (pack.isEmpty()) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::
							invalid_request } };
				}
				if (wantsDisconnected) {
					QTextStream{ stdout } << "PUBLIC_PREVIEW_DISCONNECTED_HEAD_FETCHED\n";
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

		/// \brief Checks the acquired historical transport boundary without consuming a pack.
		[[nodiscard]] auto verify_history_fetch_refusals(application::git_smart_transport_port&        aTransport,
														 const application::git_smart_request_context& aContext) -> bool
		{
			class refusal_consumer final : public application::git_smart_wire_consumer
			{
			public:
				[[nodiscard]] auto consume(std::span<const std::byte> aBytes) noexcept -> bool override
				{
					++Calls;
					Bytes += aBytes.size();
					return false;
				}
				std::size_t Calls = 0U, Bytes = 0U;
			};
			const auto capabilities = application::detail::git_protocol_v2_capabilities{
				.ObjectFormat = aContext.Revision.Head.algorithm(), .LsRefs = true, .Fetch = true
			};
			const auto encoded = application::detail::make_git_protocol_v2_fetch_request(aContext, capabilities, { });
			if (!encoded) {
				return false;
			}
			const auto original = std::string{ reinterpret_cast<const char*>(encoded->data()), encoded->size() };
			const auto head     = core::canonical_hex(aContext.Revision.Head);
			for (const auto substitute : { true, false }) {
				auto body = original;
				if (substitute) {
					const auto position = body.find(head.view());
					if (position == std::string::npos) {
						return false;
					}
					body.replace(position, head.view().size(), disconnected_history_head);
				} else {
					const auto position = body.find("0009done\n");
					if (position == std::string::npos) {
						return false;
					}
					body.insert(position, "0032want " + std::string{ disconnected_history_head } + "\n");
				}
				const auto bytes   = byte_vector(body);
				const auto request = application::git_smart_fetch_request{
					.Context = aContext, .Body = bytes, .MaximumResponseBytes = 64U * 1024U * 1024U
				};
				refusal_consumer consumer;
				if (!application::valid(request)) {
					return false;
				}
				const auto result = aTransport.fetch(request, consumer, { });
				if (result || result.error().Failure != application::git_smart_transport_failure::invalid_request
					|| consumer.Calls != 0U || consumer.Bytes != 0U) {
					return false;
				}
			}
			QTextStream{ stdout } << "LARGE_REVIEW_HISTORY_FETCH_REFUSALS_OK:2\n";
			return true;
		}

		/// \brief [[BPR-0657]]. Rewrites a recorded review-thread answer with
		/// the fixture's replies and resolutions.
		[[nodiscard]] auto overlay_review_threads(
			const std::string&                                                    aBody,
			const std::map<std::string, fixture_recording_state::thread_overlay>& aOverlays,
			const bool                                                            aForeignPosts,
			const std::vector<QJsonObject>&                                       aCreated)
			-> std::string
		{
			auto document = QJsonDocument::fromJson(QByteArray::fromStdString(aBody));
			if (!document.isObject()) {
				return aBody;
			}
			auto root        = document.object();
			auto data        = root.value(QStringLiteral("data")).toObject();
			auto repository  = data.value(QStringLiteral("repository")).toObject();
			auto pullRequest = repository.value(QStringLiteral("pullRequest")).toObject();
			auto threads     = pullRequest.value(QStringLiteral("reviewThreads")).toObject();
			auto nodes       = threads.value(QStringLiteral("nodes")).toArray();
			// Paged final-audit answers share the operation but have no thread connection.
			if (!pullRequest.contains(QStringLiteral("reviewThreads"))) {
				return aBody;
			}
			for (auto index = 0; index < nodes.size(); ++index) {
				auto node = nodes.at(index).toObject();
				if (aForeignPosts) {
					auto comments = node.value(QStringLiteral("comments")).toObject();
					auto posts    = comments.value(QStringLiteral("nodes")).toArray();
					for (auto entry = posts.begin(); entry != posts.end(); ++entry) {
						auto post = entry->toObject();
						post.insert(QStringLiteral("author"), QJsonObject{ { QStringLiteral("id"), QStringLiteral("SYNTHETIC_USER_800099") }, { QStringLiteral("login"), QStringLiteral("fixture-other-reviewer") } });
						*entry = post;
					}
					comments.insert(QStringLiteral("nodes"), posts);
					node.insert(QStringLiteral("comments"), comments);
					nodes.replace(index, node);
				}
				const auto overlay = aOverlays.find(node.value(QStringLiteral("id")).toString().toStdString());
				if (overlay == aOverlays.end()) {
					continue;
				}
				if (overlay->second.Resolved.has_value()) {
					node.insert(QStringLiteral("isResolved"), *overlay->second.Resolved);
					node.insert(QStringLiteral("viewerCanResolve"), !*overlay->second.Resolved);
					node.insert(QStringLiteral("viewerCanUnresolve"), *overlay->second.Resolved);
				}
				auto comments = node.value(QStringLiteral("comments")).toObject();
				auto posts    = comments.value(QStringLiteral("nodes")).toArray();
				if (!posts.isEmpty() && overlay->second.RootBody) {
					auto rootPost = posts.first().toObject();
					rootPost.insert(QStringLiteral("bodyText"), QString::fromStdString(*overlay->second.RootBody));
					posts.replace(0, rootPost);
				}
				if (!posts.isEmpty() && overlay->second.RootAuthor) {
					auto rootPost = posts.first().toObject();
					rootPost.insert(QStringLiteral("author"), QJsonObject{ { QStringLiteral("id"), QString::fromStdString(overlay->second.RootAuthor->first) }, { QStringLiteral("login"), QString::fromStdString(overlay->second.RootAuthor->second) } });
					posts.replace(0, rootPost);
				}
				const auto rootId = posts.isEmpty()
					? QString{ }
					: posts.at(0).toObject().value(QStringLiteral("id")).toString();
				for (const auto& [identifier, body] : overlay->second.Replies) {
					auto author = QJsonObject{ };
					author.insert(QStringLiteral("login"), QStringLiteral("octo-reviewer"));
					author.insert(QStringLiteral("id"), QStringLiteral("U_fixture_viewer"));
					auto replyTo = QJsonObject{ };
					replyTo.insert(QStringLiteral("id"), rootId);
					auto post = QJsonObject{ };
					post.insert(QStringLiteral("id"), QString::fromStdString(identifier));
					post.insert(QStringLiteral("bodyText"), QString::fromStdString(body));
					post.insert(QStringLiteral("path"), node.value(QStringLiteral("path")));
					post.insert(QStringLiteral("line"), node.value(QStringLiteral("line")));
					post.insert(QStringLiteral("originalLine"), node.value(QStringLiteral("originalLine")));
					post.insert(QStringLiteral("outdated"), false);
					post.insert(QStringLiteral("author"), author);
					post.insert(QStringLiteral("replyTo"),
								rootId.isEmpty() ? QJsonValue{ QJsonValue::Null } : QJsonValue{ replyTo });
					posts.append(post);
				}
				comments.insert(QStringLiteral("nodes"), posts);
				comments.insert(QStringLiteral("totalCount"), posts.size());
				node.insert(QStringLiteral("comments"), comments);
				nodes.replace(index, node);
			}
			// [[BPR-1374]]. A thread deleted elsewhere is no longer answered.
			auto deleted = 0;
			for (auto index = nodes.size() - 1; index >= 0; --index) {
				const auto overlay = aOverlays.find(nodes.at(index).toObject().value(QStringLiteral("id")).toString().toStdString());
				if (overlay != aOverlays.end() && overlay->second.Deleted) {
					nodes.removeAt(index);
					++deleted;
				}
			}
			for (const auto& created : aCreated) {
				nodes.append(created);
			}
			threads.insert(QStringLiteral("totalCount"), threads.value(QStringLiteral("totalCount")).toInt() + static_cast<int>(aCreated.size()) - deleted);
			threads.insert(QStringLiteral("nodes"), nodes);
			pullRequest.insert(QStringLiteral("reviewThreads"), threads);
			repository.insert(QStringLiteral("pullRequest"), pullRequest);
			data.insert(QStringLiteral("repository"), repository);
			root.insert(QStringLiteral("data"), data);
			return QJsonDocument{ root }.toJson(QJsonDocument::Compact).toStdString();
		}

		[[nodiscard]] auto home_waiting_response(const fixture_recording_state& aState, const github_wire_request& aRequest) -> std::optional<wire_response>
		{
			if (!aState.PersonalDiscovery) {
				return std::nullopt;
			}
			const auto* parent = std::get_if<core::pull_request_request_context>(&aRequest.Correlation.Parent);
			const auto  number = parent ? parent->Number.value() : aRequest.Wire.Semantic.PullRequestNumber;
			const auto  found  = std::ranges::find(aState.PersonalDiscovery->OpenPullRequests, number, &github_personal_discovery_fixture::pull_request::Number);
			if (found == aState.PersonalDiscovery->OpenPullRequests.end() || !found->WaitingDetails) {
				return std::nullopt;
			}
			const auto operation = aRequest.Wire.Operation;
			if (operation != read_operation::list_review_requests && operation != read_operation::get_merge_assessment && operation != read_operation::list_checks) {
				return std::nullopt;
			}
			auto body = QJsonObject{ };
			if (operation == read_operation::list_checks) {
				auto rows = QJsonArray{ };
				for (int index = 0; index < 24; ++index) {
					rows.append(QJsonObject{ { "node_id", QStringLiteral("home-check-%1").arg(index) }, { "head_sha", QString::fromUtf8(aState.Descriptor.HeadOneOid) }, { "name", index == 0 ? "Branch Freshness" : "CI" }, { "status", "completed" }, { "conclusion", index == 0 && (!found->CheckFailed || found->CheckFailed->load()) ? "failure" : index <= 16 ? "success"
																																																																																									: "skipped" },
											 { "output", QJsonObject{ { "title", "Branch behind base" }, { "summary", "Refresh branch before merging." } } } });
				}
				body = QJsonObject{ { "total_count", 24 }, { "check_runs", rows } };
			} else {
				auto pull = QJsonObject{ { "id", "PR_fixture_17" }, { "headRefOid", QString::fromUtf8(aState.Descriptor.HeadOneOid) }, { "baseRefOid", QString::fromUtf8(aState.Descriptor.BaseOid) }, { "baseRefName", "main" } };
				if (operation == read_operation::get_merge_assessment) {
					pull.insert("mergeStateStatus", "BLOCKED");
					pull.insert("reviewDecision", !found->RequiredReviewPresent || found->RequiredReviewPresent->load() ? "REVIEW_REQUIRED" : "APPROVED");
					pull.insert("autoMergeRequest", QJsonValue::Null);
				} else {
					auto requests = QJsonArray{ };
					if (!found->ReviewerRequestsPresent || found->ReviewerRequestsPresent->load()) {
						for (int index = 0; index < 3; ++index) {
							requests.append(QJsonObject{ { "id", QStringLiteral("home-request-%1").arg(index) }, { "asCodeOwner", true }, { "requestedReviewer", QJsonObject{ { "__typename", "Team" }, { "id", QStringLiteral("home-team-%1").arg(index) }, { "slug", index == 1 ? QStringLiteral("renderer-and-cross-platform-maintainers") : QStringLiteral("team-%1").arg(index) }, { "name", "Platform owners" }, { "organization", QJsonObject{ { "login", "octo-org" } } } } } });
						}
					}
					pull.insert("reviewRequests", QJsonObject{ { "totalCount", requests.size() }, { "nodes", requests }, { "pageInfo", QJsonObject{ { "hasNextPage", false }, { "endCursor", requests.isEmpty() ? QJsonValue{ QJsonValue::Null } : QJsonValue{ "HOME_END" } } } } });
				}
				body = QJsonObject{ { "data", QJsonObject{ { "repository", QJsonObject{ { "id", "R_fixture_repository" }, { "pullRequest", pull } } } } } };
			}
			return wire_response{ .Status = 200, .Headers = response_headers(), .Body = QJsonDocument{ body }.toJson(QJsonDocument::Compact).toStdString(), .Completeness = page_completeness::complete };
		}

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

			[[nodiscard]] auto strict_cli() const noexcept -> bool override
			{
				return State->CliEnabled && Role == fixture_transport_role::account
					&& (!State->CliStrictCapability || State->CliStrictCapability->load());
			}

			[[nodiscard]] auto perform(
				const github_wire_request& aRequest,
				const std::stop_token      aStop) -> github_wire_completion override
			{
				if (aStop.stop_requested()) {
					return github_wire_cancelled{ .Correlation = aRequest.Correlation };
				}
				if (aRequest.Wire.Operation == read_operation::get_viewer_identity && State->BeforeViewerIdentity) {
					State->BeforeViewerIdentity(aStop);
					if (aStop.stop_requested()) {
						return github_wire_cancelled{ .Correlation = aRequest.Correlation };
					}
				}
				if (auto summary = home_waiting_response(*State, aRequest)) {
					if (!validate_github_wire_request(State->Profile, aRequest)) {
						return github_wire_error{ .Correlation = aRequest.Correlation, .Error = provider_error(application::github_provider_failure::malformed_response) };
					}
					append_rate_limit_headers(*summary, aRequest.Wire.Operation);
					return github_wire_success{ .Correlation = aRequest.Correlation, .Response = std::move(*summary) };
				}
				if (aRequest.Wire.Operation == read_operation::get_merge_assessment || aRequest.Wire.Operation == read_operation::list_review_requests || aRequest.Wire.Operation == read_operation::list_branch_rules || aRequest.Wire.Operation == read_operation::get_branch_protection) {
					if (!validate_github_wire_request(State->Profile, aRequest)) {
						return github_wire_error{ .Correlation = aRequest.Correlation, .Error = provider_error(application::github_provider_failure::malformed_response) };
					}
					auto body = std::optional<std::string>{ };
					if (State->PublishedTour) {
						const std::scoped_lock lock{ State->PublishedTour->Mutex };
						switch (aRequest.Wire.Operation) {
						case read_operation::get_merge_assessment:  body = State->PublishedTour->MergeAssessment; break;
						case read_operation::list_review_requests:  body = State->PublishedTour->ReviewRequests; break;
						case read_operation::list_branch_rules:     body = State->PublishedTour->BranchRules; break;
						case read_operation::get_branch_protection: body = State->PublishedTour->BranchProtection; break;
						default:                                    break;
						}
					}
					if (!body) {
						return github_wire_error{ .Correlation = aRequest.Correlation, .Error = provider_error(application::github_provider_failure::unsupported_capability) };
					}
					auto response = wire_response{ .Status = 200, .Headers = response_headers(), .Body = *body, .Completeness = page_completeness::complete };
					append_rate_limit_headers(response, aRequest.Wire.Operation);
					return github_wire_success{ .Correlation = aRequest.Correlation, .Response = std::move(response) };
				}
				if (aRequest.Wire.Operation == read_operation::list_requested_reviewers && !State->PersonalDiscovery) {
					return github_wire_error{ .Correlation = aRequest.Correlation, .Error = provider_error(application::github_provider_failure::unavailable) };
				}
				if (aRequest.Wire.Operation == read_operation::list_commit_statuses && !State->Descriptor.Paged) {
					auto body = std::string{ "[]" };
					if (State->PublishedTour) {
						const std::scoped_lock lock{ State->PublishedTour->Mutex };
						body = State->PublishedTour->CommitStatuses.value_or(body);
					}
					auto response = wire_response{ .Status = 200, .Headers = response_headers(), .Body = std::move(body), .Completeness = page_completeness::complete };
					append_rate_limit_headers(response, read_operation::list_commit_statuses);
					return github_wire_success{ .Correlation = aRequest.Correlation, .Response = std::move(response) };
				}
				if (aRequest.Wire.Operation == read_operation::list_pull_request_files && State->BeforeFilesPage) {
					State->BeforeFilesPage(aStop);
					if (aStop.stop_requested()) {
						return github_wire_cancelled{ .Correlation = aRequest.Correlation };
					}
				}
				// [[BPR-1357]]. A slow network for a pull request's first
				// answers, held before any lock so nothing else waits on it.
				if (const auto* read = std::get_if<core::pull_request_request_context>(&aRequest.Correlation.Parent);
					State->ReadDelay && read != nullptr && !read->ExpectedPullRequest.has_value()
					&& (aRequest.Wire.Operation == read_operation::get_repository || aRequest.Wire.Operation == read_operation::get_pull_request)) {
					const auto milliseconds = State->ReadDelay->Milliseconds.load();
					if (milliseconds != 0U) {
						++State->ReadDelay->Delayed;
						const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds{ milliseconds };
						while (std::chrono::steady_clock::now() < until && !aStop.stop_requested()) {
							std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
						}
						if (aStop.stop_requested()) {
							++State->ReadDelay->Cancelled;
							return github_wire_cancelled{ .Correlation = aRequest.Correlation };
						}
					}
					// Whichever of the two answers comes first fails the read.
					auto remaining = State->ReadDelay->FailNext.load();
					while (remaining != 0U && !State->ReadDelay->FailNext.compare_exchange_weak(remaining, remaining - 1U)) { }
					const auto failNumber = State->ReadDelay->FailNumber.load();
					if (remaining != 0U || (failNumber != 0U && read->Number.value() == failNumber)) {
						++State->ReadDelay->Failed;
						return github_wire_error{ .Correlation = aRequest.Correlation,
												  .Error       = provider_error(application::github_provider_failure::network) };
					}
				}
				std::unique_lock historyLock{ State->HistoryCompileMutex, std::defer_lock };
				if (Role == fixture_transport_role::account && !State->HistoryHeads.empty()
					&& aRequest.Wire.Operation == read_operation::get_repository) {
					historyLock.lock();
				}
				std::unique_lock lock{ State->Mutex };
				if (aRequest.Correlation.ProfileRevision != State->Profile.revision()
					|| aRequest.Correlation.Host != State->Profile.host()) {
					State->RecordingFailure = -2;
					return github_wire_error{
						.Correlation = aRequest.Correlation,
						.Error       = provider_error(
							application::github_provider_failure::malformed_response),
					};
				}
				const auto parentSession = std::visit([](const auto& aParent) { return aParent.ApplicationSession; }, aRequest.Correlation.Parent);
				if ((aRequest.Correlation.AuthenticationSession && (Role != fixture_transport_role::account || aRequest.Correlation.AuthenticationSession->value() == 0U))
					|| (Role == fixture_transport_role::account && (!State->AuthenticationSession || aRequest.Correlation.AuthenticationSession.value_or(parentSession) != *State->AuthenticationSession))) {
					State->RecordingFailure = -4;
					return github_wire_error{ .Correlation = aRequest.Correlation, .Error = provider_error(application::github_provider_failure::authentication) };
				}
				if (Role == fixture_transport_role::anonymous
					&& carries_credential(aRequest.Wire.Headers)) {
					State->RecordingFailure = -3;
					return github_wire_error{
						.Correlation = aRequest.Correlation,
						.Error       = provider_error(
							application::github_provider_failure::authentication),
					};
				}
				if (State->Offline && State->Offline->Offline.load()) {
					++State->Offline->WireRefusals;
					return github_wire_error{ .Correlation = aRequest.Correlation,
											  .Error       = provider_error(application::github_provider_failure::network) };
				}
				if (Role == fixture_transport_role::account && State->RateLimit && aRequest.Wire.Operation == read_operation::get_repository) {
					State->RateLimit->LastRequestNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
					++State->RateLimit->Requests;
				}
				if (Role == fixture_transport_role::account && State->RateLimit
					&& (aRequest.Wire.Operation == read_operation::get_repository || aRequest.Wire.Operation == read_operation::search_personal_pull_requests)
					&& State->RateLimit->Mode.load() != 0U) {
					if (!adapters::github::detail::validate_github_wire_request(State->Profile, aRequest).has_value()) {
						return github_wire_error{ .Correlation = aRequest.Correlation, .Error = provider_error(application::github_provider_failure::malformed_response) };
					}
					auto limited                              = rate_limit_response(*State->RateLimit, State->RateLimit->Mode.load(), aRequest.Wire.Operation);
					State->RateLimit->LastResponseNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
					++State->RateLimit->Responses;
					return github_wire_success{ .Correlation = aRequest.Correlation, .Response = std::move(limited) };
				}
				// [[BPR-0990]]. Personal discovery is answered by the fixture
				// server, not the recorded review script, and only on the
				// account's transport.
				if (Role == fixture_transport_role::account) {
					State->AccountReadOperations.emplace_back(adapters::github::detail::operation_name(aRequest.Wire.Operation));
				}
				// [[BPR-1387]]. Snapshot reads start with get_repository; Home's
				// bounded metadata reads start at get_pull_request. Both carry
				// PR parent contexts, so keep the exact snapshot request identity
				// to avoid feeding a partial body to an open or advancing its
				// ordered recording for an unrelated Home refresh.
				const auto retainedMetadata = Role == fixture_transport_role::account
					&& (aRequest.Wire.Operation == read_operation::get_pull_request || aRequest.Wire.Operation == read_operation::list_reviews)
					&& std::ranges::find(State->SnapshotReads, aRequest.Correlation.Parent) == State->SnapshotReads.end();
				if (retainedMetadata && !State->PersonalDiscovery) {
					if (!adapters::github::detail::validate_github_wire_request(State->Profile, aRequest)) {
						State->RecordingFailure = -5;
						return github_wire_error{ .Correlation = aRequest.Correlation,
												  .Error       = provider_error(application::github_provider_failure::malformed_response) };
					}
					return github_wire_error{ .Correlation = aRequest.Correlation,
											  .Error       = provider_error(application::github_provider_failure::unavailable) };
				}
				const auto syntheticMetadata = retainedMetadata && State->PersonalDiscovery
					&& std::ranges::any_of(State->PersonalDiscovery->OpenPullRequests, [&](const auto& aPullRequest) { return aPullRequest.Number == aRequest.Wire.Semantic.PullRequestNumber; });
				if (State->PersonalDiscovery.has_value()
					&& (aRequest.Wire.Operation == read_operation::search_personal_pull_requests || aRequest.Wire.Operation == read_operation::list_requested_reviewers || syntheticMetadata)) {
					// The exact canonical request production validation admits,
					// under the repository it names, or nothing is answered.
					if (Role != fixture_transport_role::account || !adapters::github::detail::validate_github_wire_request(State->Profile, aRequest).has_value()) {
						State->RecordingFailure = -5;
						return github_wire_error{ .Correlation = aRequest.Correlation, .Error = provider_error(application::github_provider_failure::authentication) };
					}
					if (aRequest.Wire.Operation == read_operation::search_personal_pull_requests) {
						++State->DiscoverySearches;
						State->DiscoverySearchTerms.push_back(aRequest.Wire.Semantic.SearchTerms);
					} else if (aRequest.Wire.Operation == read_operation::list_requested_reviewers) {
						++State->DiscoveryDetailReads;
					}
					auto discovered = personal_discovery_response(*State, aRequest.Wire);
					append_rate_limit_headers(discovered, aRequest.Wire.Operation);
					return github_wire_success{ .Correlation = aRequest.Correlation, .Response = std::move(discovered) };
				}
				if (Role == fixture_transport_role::account) {
					if (!State->advance_history(aRequest.Wire, lock)) {
						return github_wire_error{ .Correlation = aRequest.Correlation, .Error = provider_error(application::github_provider_failure::authentication) };
					}
				}
				auto& recording = Role == fixture_transport_role::bootstrap
					? State->BootstrapRecording
					: Role == fixture_transport_role::anonymous
					? State->AnonymousRecording
					: State->Recording;
				// [[BPR-0657]], [[BPR-0932]]. Refresh and Recent reopen start at
				// the repository on either read authority. Bootstrap stays one-shot;
				// replay still validates the complete exact recorded request below.
				if (Role != fixture_transport_role::bootstrap && State->RepeatReads
					&& aRequest.Wire.Operation == read_operation::get_repository) {
					recording.replay_from(read_operation::get_repository, State->RateLimit != nullptr || State->LargeReviewRepeatReads);
				}
				// [[BPR-1200]]. Signed out, the inaccessible repository is refused
				// as GitHub refuses a reader without an account: not found. The
				// exact outbound request still passes the strict replay first.
				const auto deniedSignedOut = State->IndependentWindowReads && Role == fixture_transport_role::anonymous
					&& aRequest.Wire.Operation == read_operation::get_repository
					&& aRequest.Wire.Path == "/repos/octo-org/inaccessible-repository";
				auto response = deniedSignedOut
					? perform_independent_read(State->IndependentRecording, aRequest.Wire)
					: State->IndependentWindowReads && Role != fixture_transport_role::anonymous
					? perform_independent_read(Role == fixture_transport_role::bootstrap ? State->IndependentBootstrap : State->IndependentRecording, aRequest.Wire)
					: recording.perform(aRequest.Wire);
				if (deniedSignedOut && response) {
					response->Status = 404;
					response->Body   = R"({"message":"Not Found"})";
				}
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
					if (aRequest.Wire.Operation == read_operation::get_repository
						&& std::ranges::find(State->SnapshotReads, aRequest.Correlation.Parent) == State->SnapshotReads.end()) {
						State->SnapshotReads.push_back(aRequest.Correlation.Parent);
					}
				}
				append_rate_limit_headers(*response, aRequest.Wire.Operation);
				if (State->PersonalDiscovery && aRequest.Wire.Operation == read_operation::get_pull_request && response->Status == 200) {
					// [[BPR-1379]]. The older snapshot recording omitted fields a
					// real PR response also supplies to the lightweight Home read.
					const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(response->Body));
					auto       object   = document.object();
					object.insert(QStringLiteral("url"), QString::fromStdString(std::string{ State->Profile.rest_root() }) + QStringLiteral("/repos/%1/%2/pulls/%3").arg(QString::fromStdString(aRequest.Wire.Semantic.Owner), QString::fromStdString(aRequest.Wire.Semantic.Repository)).arg(aRequest.Wire.Semantic.PullRequestNumber));
					const auto found = std::ranges::find(State->PersonalDiscovery->OpenPullRequests, aRequest.Wire.Semantic.PullRequestNumber, &github_personal_discovery_fixture::pull_request::Number);
					if (found != State->PersonalDiscovery->OpenPullRequests.end() && found->WaitingDetails) {
						object.insert("user", QJsonObject{ { "node_id", found->Author == "octo-reviewer" ? "U_fixture_viewer" : "U_fixture_author" }, { "login", QString::fromStdString(found->Author) } });
					}
					object.insert(QStringLiteral("requested_reviewers"), QJsonArray{ });
					object.insert(QStringLiteral("requested_teams"), QJsonArray{ });
					response->Body = QJsonDocument{ object }.toJson(QJsonDocument::Compact).toStdString();
				}
				if (State->ReviewTourCourse) {
					overlay_review_tour(response->Body, aRequest.Wire.Operation, State->Descriptor);
				}
				if (State->PublishedTour) {
					overlay_published_tour(response->Body, aRequest.Wire.Operation, *State->PublishedTour);
				}

				// [[BPR-0978]]. A second profile's account is a different actor
				// entirely: database id, node id and login all change together, so
				// the recorded octo-reviewer records keep their own consistent
				// identity beside the substituted viewer.
				if (State->DistinctAccount && aRequest.Wire.Operation == read_operation::get_viewer_identity) {
					auto identity = QJsonDocument::fromJson(QByteArray::fromStdString(response->Body)).object();
					identity.insert(QStringLiteral("id"), static_cast<qint64>(State->Descriptor.AccountId.value()));
					identity.insert(QStringLiteral("node_id"), QStringLiteral("U_fixture_other_profile_viewer"));
					identity.insert(QStringLiteral("login"), QStringLiteral("other-profile-reviewer"));
					response->Body = QJsonDocument{ identity }.toJson(QJsonDocument::Compact).toStdString();
				}
				if (State->RecentAccount && aRequest.Wire.Operation == read_operation::get_viewer_identity) {
					const auto account  = State->RecentAccount->load();
					auto       identity = QJsonDocument::fromJson(QByteArray::fromStdString(response->Body)).object();
					identity.insert(QStringLiteral("id"), static_cast<qint64>(account));
					identity.insert(QStringLiteral("node_id"), QStringLiteral("U_recent_%1").arg(account));
					identity.insert(QStringLiteral("login"), QStringLiteral("recent-reviewer-%1").arg(account));
					response->Body = QJsonDocument{ identity }.toJson(QJsonDocument::Compact).toStdString();
				}
				if (State->RecentAccount && aRequest.Wire.Operation == read_operation::get_pull_request) {
					auto review = QJsonDocument::fromJson(QByteArray::fromStdString(response->Body)).object();
					review.insert(QStringLiteral("title"), QStringLiteral("Private title for account %1").arg(State->RecentAccount->load()));
					response->Body = QJsonDocument{ review }.toJson(QJsonDocument::Compact).toStdString();
				}
				if (State->RecentAccount && aRequest.Wire.Operation == read_operation::search_pull_requests) {
					auto search = QJsonDocument::fromJson(QByteArray::fromStdString(response->Body)).object();
					auto items  = search.value(QStringLiteral("items")).toArray();
					for (auto item = items.begin(); item != items.end(); ++item) {
						auto review = item->toObject();
						review.insert(QStringLiteral("title"), QStringLiteral("Private title for account %1").arg(State->RecentAccount->load()));
						*item = review;
					}
					search.insert(QStringLiteral("items"), items);
					response->Body = QJsonDocument{ search }.toJson(QJsonDocument::Compact).toStdString();
				}
				// [[BPR-0644]]. The token signs in as an account no binding names:
				// a different actor, whatever the recorded viewer was.
				if (State->UnboundViewer && aRequest.Wire.Operation == read_operation::get_viewer_identity) {
					auto identity = QJsonDocument::fromJson(QByteArray::fromStdString(response->Body)).object();
					identity.insert(QStringLiteral("id"), static_cast<qint64>(9'106'440));
					identity.insert(QStringLiteral("node_id"), QStringLiteral("U_fixture_unbound_viewer"));
					identity.insert(QStringLiteral("login"), QStringLiteral("unbound-reviewer"));
					response->Body = QJsonDocument{ identity }.toJson(QJsonDocument::Compact).toStdString();
				}
				if (aRequest.Wire.Operation == read_operation::list_review_threads
					&& (!State->ThreadOverlays.empty() || State->LargeReviewForeignPosts || !State->CreatedReviewThreads.empty())) {
					response->Body = overlay_review_threads(response->Body, State->ThreadOverlays, State->LargeReviewForeignPosts, State->CreatedReviewThreads);
				}
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
					State->Profile, { }, aRequest.Correlation.Part);
				if (!expected.has_value() || aRequest.Wire != *expected
					|| aRequest.Correlation.ProfileRevision
						!= State->Profile.revision()
					|| aRequest.Correlation.Host != State->Profile.host()
					|| aRequest.Correlation.Session.Profile
						!= State->Profile.profile()
					|| aRequest.Correlation.ObservedAccount.value() == 0U
					// [[BPR-0536]]. The same relaxation as the factories
					// above, and for the same session: the host invalidates
					// the identity's cached capabilities when the generation
					// advances, so a second establishment probes again.
					|| (!State->IndependentWindowReads && aRequest.Correlation.Part != State->CapabilityCalls % adapters::github::detail::capability_probe_part_count)
					|| (!State->RepeatSessions && State->CapabilityCalls >= adapters::github::detail::capability_probe_part_count)) {
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
					.Body                 = State->CapabilityBody[aRequest.Correlation.Part],
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
				application::diagnostic_summary summary;
				if (State->CapabilityDiagnosticFailure) {
					response.Body = capability_diagnostic_failure_body;
					summary       = State->CapabilityFailureSummary;
				}
				return github_capability_probe_wire_success{
					.Correlation = aRequest.Correlation,
					.Response    = std::move(response),
					.Summary     = summary,
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
				// Ordinary fixtures serve one or two exact heads. History replay
				// additionally accepts only the explicit stored head inventory.
				const auto acceptedHead   = State->IndependentWindowReads ? head == State->IndependentHead : State->serves_head(head);
				const auto acceptedReview = (aContext.Authority.PullRequest == fixture.PullRequestId && aContext.Authority.Number == fixture.Number)
					|| (State->IndependentWindowReads && aContext.Authority.PullRequest == core::pull_request_id{ 3002U }
						&& aContext.Authority.Number == core::pull_request_number{ 18U });
				// [[BPR-0715]]. The anonymous role serves the signed-out
				// authority, which names no account; the account role keeps
				// serving the recorded account and nothing else.
				const auto anonymousRole = Role == fixture_transport_role::anonymous;
				const auto acceptedRole  = anonymousRole
					? aContext.Authority.Mode == core::github_access_mode::public_preview
						&& aContext.Authority.Account.value() == 0U
					: Role == fixture_transport_role::account
						&& aContext.Authority.Mode == core::github_access_mode::authenticated
						&& aContext.Authority.Account == (State->RecentAccount ? core::github_account_id{ State->RecentAccount->load() } : fixture.AccountId);
				if (!acceptedRole
					|| !application::valid(aContext)
					|| aContext.Authority.Repository != fixture.RepositoryId
					|| !acceptedReview
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
					if (State->RefuseFirstSmartAcquire
						&& !State->FirstSmartAcquireRefused) {
						State->FirstSmartAcquireRefused = true;
						++State->SmartAcquireRefusals;
						return std::unexpected{ application::git_smart_transport_error{
							.Failure = application::git_smart_transport_failure::unavailable } };
					}
					++State->SmartAcquireCalls;
					State->LastSmartAuthority = aContext.Authority;
				}
				auto transport = application::git_smart_transport{ new fixture_smart_transport{ State, aContext } };
				if (!State->HistoryHeads.empty() && !verify_history_fetch_refusals(*transport, aContext)) {
					return std::unexpected{ application::git_smart_transport_error{
						.Failure = application::git_smart_transport_failure::invalid_request } };
				}
				return transport;
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
				, AnonymousTransport{ std::make_shared<fixture_recorded_wire_transport>(State, fixture_transport_role::anonymous) }
			{
			}

			[[nodiscard]] auto supports_cli() const noexcept -> bool override { return State->CliEnabled; }

			[[nodiscard]] auto make_cli(
				const adapters::github::validated_github_host_profile& aProfile,
				const core::github_session_request_context             aContext) noexcept
				-> github_wire_transport_factory_result override
			{
				const std::scoped_lock lock{ State->Mutex };
				if (!State->CliEnabled || aProfile.hostname() != State->Profile.hostname()
					|| aProfile.host() != State->Profile.host() || aProfile.server_contract() != State->Profile.server_contract()
					|| aProfile.authentication_transport() != application::github_authentication_transport::github_cli
					|| aContext.Profile != aProfile.profile() || (!State->RepeatSessions && State->AccountFactoryCalls != 0U)) {
					return std::unexpected{ provider_error(application::github_provider_failure::unavailable) };
				}
				if (State->RepeatSessions && State->AccountFactoryCalls != 0U) {
					State->Recording.rewind();
				}
				State->Profile               = aProfile;
				State->AuthenticationSession = aContext.ApplicationSession;
				++State->AccountFactoryCalls;
				return AccountTransport;
			}

			[[nodiscard]] auto make_bootstrap(
				const adapters::github::validated_github_host_profile& aProfile,
				const core::github_session_request_context             aContext,
				application::github_credential_material                aCredential) noexcept
				-> github_wire_transport_factory_result override
			{
				const std::scoped_lock lock{ State->Mutex };
				++State->BootstrapFactoryAttempts;
				if ((!same_profile(aProfile, State->Profile)
					 && (!State->RepeatSessions || aProfile.hostname() != State->Profile.hostname() || aProfile.host() != State->Profile.host() || aProfile.server_contract() != State->Profile.server_contract()))
					|| aContext.Profile != aProfile.profile()
					|| !application::valid(aCredential)
					|| (!State->RepeatSessions
						&& State->BootstrapFactoryCalls != 0U)) {
					return std::unexpected{ provider_error(
						application::github_provider_failure::unavailable) };
				}
				// BPR-1410: a successful replacement has a new connection ID on
				// this same recorded server; RepeatSessions serves that authority.
				State->Profile = aProfile;
				// [[BPR-0536]]. A repeat session is a repeat of the recorded
				// script, not a continuation of it: the renewal course's
				// rotation re-enrolls the rotated access token, the product
				// establishes the session again to bind it, and that second
				// establishment reads `/user` exactly as the first one did.
				// Off the course the cursor is untouched and the recording
				// stays consumed-once.
				if (State->RepeatSessions && State->BootstrapFactoryCalls != 0U) {
					State->BootstrapRecording.rewind();
				}
				if (State->CapabilityDiagnosticFailure) {
					State->CapabilityFailureSummary = adapters::github::detail::sanitize_capability_error(
						capability_diagnostic_failure_body,
						aCredential.bytes());
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
				++State->AccountFactoryAttempts;
				if ((!same_profile(aProfile, State->Profile)
					 && (!State->RepeatSessions || aProfile.hostname() != State->Profile.hostname() || aProfile.host() != State->Profile.host() || aProfile.server_contract() != State->Profile.server_contract()))
					|| !application::valid(aCredential.Credential)
					|| aCredential.Context.Authority.Profile != aProfile.profile()
					|| aCredential.Context.Authority.ProfileRevision
						!= aProfile.revision()
					|| aCredential.Context.Authority.Host != aProfile.host()
					|| aCredential.Context.Authority.Hostname.value()
						!= aProfile.hostname()
					|| aCredential.Context.Authority.ServerContract
						!= aProfile.server_contract()
					|| (!State->RepeatSessions
						&& State->AccountFactoryCalls != 0U)) {
					return std::unexpected{ provider_error(
						application::github_provider_failure::unavailable) };
				}
				State->Profile = aProfile;
				// [[BPR-0536]]. The bootstrap rewind's counterpart. The
				// account script opens with the same viewer read the session
				// makes on the account transport, so a second establishment
				// needs the cursor back at that exchange rather than at the
				// pull-request read behind it.
				if (State->RepeatSessions && State->AccountFactoryCalls != 0U) {
					State->Recording.rewind();
				}
				if (State->CapabilityDiagnosticFailure) {
					State->CapabilityFailureSummary = adapters::github::detail::sanitize_capability_error(
						capability_diagnostic_failure_body,
						aCredential.Credential.bytes());
				}
				State->AuthenticationSession = aCredential.Context.ApplicationSession;
				++State->AccountFactoryCalls;
				return AccountTransport;
			}

			// RFC-BPR-0017. Deliberately overridden rather than left to the
			// base's refusal: the shipped factory beside this one composes a
			// real anonymous transport, so a fixture that refused here would
			// make the composed graph unreachable under test and prove nothing
			// about it. There is no credential parameter on this entry, so
			// nothing the course does can hand one over.
			[[nodiscard]] auto make_anonymous(
				const adapters::github::validated_github_host_profile& aProfile,
				const core::github_session_request_context             aContext) noexcept
				-> github_wire_transport_factory_result override
			{
				const std::scoped_lock lock{ State->Mutex };
				if (State->Descriptor.Paged || !same_profile(aProfile, State->Profile)
					|| aContext.Profile != aProfile.profile()) {
					return std::unexpected{ provider_error(
						application::github_provider_failure::unavailable) };
				}
				return AnonymousTransport;
			}

			[[nodiscard]] auto anonymous_smart_transport() const noexcept
				-> std::shared_ptr<application::git_smart_transport_factory_port>
			{
				return AnonymousTransport;
			}

		private:
			std::shared_ptr<fixture_recording_state>         State;
			std::shared_ptr<fixture_recorded_wire_transport> BootstrapTransport;
			std::shared_ptr<fixture_recorded_wire_transport> AccountTransport;
			std::shared_ptr<fixture_recorded_wire_transport> AnonymousTransport;
		};

		// Replays BPR-0276's recorded device grant through the real wire
		// contract. The response bytes are rebuilt from the recorded
		// projection exactly as the contract tests rebuild them, and then
		// handed to `parse_device_code_response` and
		// `parse_device_token_response` — so this fakes the socket and
		// nothing else. The socket itself is proved separately by
		// `native_https_device_authorization_fixture`.
		//
		// It exists so the registered course can reach the composed
		// application's real enrollment and real vault, which is the only
		// place "one enrollment, no settings or SQLite copy" can be observed.
		class recorded_device_authorization_transport final
			: public application::device_authorization_transport_port
		{
		public:
			// Two roots, because the two answers have different provenance.
			// The grant is BPR-0276's recording of a real exchange; the
			// renewal is BPR-0450's documented projection, which GitHub's
			// documentation states is the user-access-token answer and which
			// nobody has captured from the wire. Keeping them apart is what
			// stops a documented shape from being read as a recorded one.
			recorded_device_authorization_transport(
				std::filesystem::path             aFixtureRoot,
				std::filesystem::path             aDocumentedRoot,
				const bool                        aRefreshCourse,
				const bool                        aRejectRefresh,
				const bool                        aOfflineRefresh,
				std::shared_ptr<std::atomic_bool> aAuthorizationPending)
				: FixtureRoot{ std::move(aFixtureRoot) }
				, DocumentedRoot{ std::move(aDocumentedRoot) }
				, RefreshCourse{ aRefreshCourse }
				, RejectRefresh{ aRejectRefresh }
				, OfflineRefresh{ aOfflineRefresh }
				, AuthorizationPending{ std::move(aAuthorizationPending) }
			{
			}

			void acquire_device_code(
				core::device_authorization_request_context  aContext,
				application::device_code_completion_handler aCompletion) override
			{
				++Submissions;
				const auto parsed = adapters::github::detail::
					parse_device_code_response(recorded(FixtureRoot, "device_code_success", { }));
				if (!parsed.has_value()
					|| !std::holds_alternative<
						adapters::github::detail::device_code_grant>(*parsed)) {
					aCompletion(
						application::device_authorization_transport_error{
							.Context = aContext,
							.Failure = application::
								device_authorization_transport_failure::
									malformed_response,
						});
					return;
				}
				const auto& grant = std::get<
					adapters::github::detail::device_code_grant>(*parsed);
				auto code = material_of(grant.DeviceCode);
				if (!code.has_value()) {
					aCompletion(
						application::device_authorization_transport_error{
							.Context = aContext,
							.Failure = application::
								device_authorization_transport_failure::
									malformed_response,
						});
					return;
				}
				aCompletion(application::device_code_acquired{
					.Context = aContext,
					.Grant   = application::device_code_acquisition{
						.DeviceCode       = std::move(*code),
						.UserCode         = grant.UserCode,
						.VerificationUri  = grant.VerificationUri,
						.ExpiresInSeconds = grant.ExpiresInSeconds,
						.IntervalSeconds  = grant.IntervalSeconds,
					},
				});
			}

			void exchange_device_token(
				application::device_token_exchange           aExchange,
				application::device_token_completion_handler aCompletion) override
			{
				++Submissions;
				if (AuthorizationPending != nullptr && AuthorizationPending->load()) {
					// [[BPR-0803]]. The reviewer has not finished in the browser.
					aCompletion(application::device_token_deferred{
						.Context = aExchange.Context,
						.Reason  = application::device_token_delay_reason::authorization_pending,
					});
					return;
				}
				const auto parsed = adapters::github::detail::
					parse_device_token_response(recorded(FixtureRoot, "token_success", { }));
				if (!parsed.has_value()
					|| !std::holds_alternative<
						adapters::github::detail::device_token_grant>(*parsed)) {
					aCompletion(
						application::device_authorization_transport_error{
							.Context = aExchange.Context,
							.Failure = application::
								device_authorization_transport_failure::
									malformed_response,
						});
					return;
				}
				const auto& grant = std::get<
					adapters::github::detail::device_token_grant>(*parsed);
				auto access  = material_of(grant.AccessToken);
				auto refresh = material_of(grant.RefreshToken);
				if (!access.has_value() || !refresh.has_value()) {
					aCompletion(
						application::device_authorization_transport_error{
							.Context = aExchange.Context,
							.Failure = application::
								device_authorization_transport_failure::
									malformed_response,
						});
					return;
				}
				AccessToken = grant.AccessToken;
				aCompletion(application::device_token_issued{
					.Context = aExchange.Context,
					.Grant   = application::device_token_material{
						.AccessToken      = std::move(*access),
						.RefreshToken     = std::move(refresh),
						.ExpiresInSeconds = grant.ExpiresInSeconds,
						.RefreshTokenExpiresInSeconds
						= grant.RefreshTokenExpiresInSeconds,
					},
				});
			}

			void refresh_access_token(
				application::device_token_exchange           aExchange,
				application::device_token_completion_handler aCompletion) override
			{
				++Submissions;
				if (!RefreshCourse) {
					// The registered course enrolls and nothing renews it, so
					// the slot it never asks for answers the way an
					// unreachable network does.
					aCompletion(
						application::device_authorization_transport_error{
							.Context = aExchange.Context,
							.Failure = application::
								device_authorization_transport_failure::network,
						});
					return;
				}
				++Renewals;
				if (OfflineRefresh) {
					// [[BPR-0644]]. An offline launch: the exchange never
					// reaches the server, so nothing is spent.
					aCompletion(
						application::device_authorization_transport_error{
							.Context = aExchange.Context,
							.Failure = application::
								device_authorization_transport_failure::network,
						});
					return;
				}
				if (RejectRefresh) {
					// [[BPR-0644]]. The token endpoint's refusal of spent or
					// revoked refresh material, which is a terminal answer and
					// never a reason to try the same material again.
					aCompletion(application::device_token_refused{
						.Context = aExchange.Context,
						.Reason  = application::device_token_refusal_reason::expired_token,
					});
					return;
				}
				// The documented answer parses with the grant's parser
				// because it is the same answer: BPR-0450 recorded that the
				// renewal response *is* the user-access-token response. Using
				// a second parser here would test a shape GitHub does not
				// send.
				const auto parsed = adapters::github::detail::
					parse_device_token_response(recorded(
						DocumentedRoot,
						"refresh_success",
						QStringLiteral("-renewal-%1")
							.arg(static_cast<qulonglong>(Renewals))));
				if (!parsed.has_value()
					|| !std::holds_alternative<
						adapters::github::detail::device_token_grant>(*parsed)) {
					aCompletion(
						application::device_authorization_transport_error{
							.Context = aExchange.Context,
							.Failure = application::
								device_authorization_transport_failure::
									malformed_response,
						});
					return;
				}
				const auto& grant = std::get<
					adapters::github::detail::device_token_grant>(*parsed);
				auto access  = material_of(grant.AccessToken);
				auto refresh = material_of(grant.RefreshToken);
				if (!access.has_value() || !refresh.has_value()) {
					aCompletion(
						application::device_authorization_transport_error{
							.Context = aExchange.Context,
							.Failure = application::
								device_authorization_transport_failure::
									malformed_response,
						});
					return;
				}
				AccessToken = grant.AccessToken;
				aCompletion(application::device_token_issued{
					.Context = aExchange.Context,
					.Grant   = application::device_token_material{
						.AccessToken      = std::move(*access),
						.RefreshToken     = std::move(refresh),
						.ExpiresInSeconds = grant.ExpiresInSeconds,
						.RefreshTokenExpiresInSeconds
						= grant.RefreshTokenExpiresInSeconds,
					},
				});
			}

			[[nodiscard]] auto renewals() const noexcept -> std::size_t
			{
				return Renewals;
			}

			void cancel(core::device_authorization_request_context) override
			{
			}

			[[nodiscard]] auto submissions() const noexcept -> std::size_t
			{
				return Submissions;
			}

			[[nodiscard]] auto access_token() const -> std::string
			{
				return AccessToken;
			}

		private:
			[[nodiscard]] static auto material_of(const std::string& aText)
				-> std::optional<application::github_credential_material>
			{
				auto bytes = std::vector<std::byte>{ };
				bytes.resize(aText.size());
				std::ranges::transform(
					aText,
					bytes.begin(),
					[](const char aCharacter) {
						return static_cast<std::byte>(aCharacter);
					});
				auto material
					= application::github_credential_material::create(bytes);
				std::ranges::fill(bytes, std::byte{ 0 });
				if (!material.has_value()) {
					return std::nullopt;
				}
				return std::optional{ std::move(*material) };
			}

			// The recorded projection lists the fields the capture observed and
			// the non-secret values it retained. The transient fields it
			// deliberately omitted get a stand-in here, exactly as the contract
			// tests do, so the shape on the wire is the recorded shape.
			// The suffix distinguishes one renewal's stand-ins from the next.
			// The capture omitted the token values deliberately, so every
			// rotation would otherwise rebuild the identical bytes and a
			// rotation that changed nothing would be indistinguishable from
			// one that never happened.
			[[nodiscard]] static auto recorded(
				const std::filesystem::path& aRoot,
				const std::string&           aOutcome,
				const QString&               aSuffix)
				-> adapters::github::detail::wire_response
			{
				const auto path = aRoot / "outcomes" / (aOutcome + ".json");
				auto       file = QFile{ QString::fromStdString(path.string()) };
				if (!file.open(QIODevice::ReadOnly)) {
					return { };
				}
				const auto            bytes = file.readAll();
				device_recording_json fixture;
				if (glz::read<fixture_json_options{ }>(fixture,
													   std::string_view{ bytes.constData(), static_cast<std::size_t>(bytes.size()) })) {
					return { };
				}
				// Own the retained values and serialize omitted transient fields as
				// strings. Sorted keys preserve the previous fixture wire order.
				std::map<std::string, glz::raw_json, std::less<>> body;
				for (const auto& name : fixture.Projection.ObservedFields) {
					if (const auto found = fixture.Projection.Values.find(name);
						found != fixture.Projection.Values.end()) {
						body.emplace(name, found->second);
						continue;
					}
					std::string encoded;
					if (glz::write<fixture_json_options{ }>(stand_in(QString::fromStdString(name), aSuffix).toStdString(), encoded)) {
						return { };
					}
					body.emplace(name, glz::raw_json{ std::move(encoded) });
				}
				std::string encoded;
				if (glz::write<fixture_json_options{ }>(body, encoded)) {
					return { };
				}
				return {
					.Status  = fixture.HttpStatus,
					.Headers = { { .Name = "Content-Type", .Value = "application/json" } },
					.Body    = std::move(encoded),
				};
			}

			[[nodiscard]] static auto stand_in(
				const QString& aField,
				const QString& aSuffix) -> QString
			{
				if (aField == QStringLiteral("device_code")) {
					return QStringLiteral("fixture-device-code-0276");
				}
				if (aField == QStringLiteral("user_code")) {
					return QStringLiteral("FIXT-URE1");
				}
				if (aField == QStringLiteral("access_token")) {
					return QStringLiteral("fixture-access-token-0276") + aSuffix;
				}
				if (aField == QStringLiteral("refresh_token")) {
					return QStringLiteral("fixture-refresh-token-0276") + aSuffix;
				}
				return QStringLiteral("fixture");
			}

			std::filesystem::path             FixtureRoot;
			std::filesystem::path             DocumentedRoot;
			bool                              RefreshCourse  = false;
			bool                              RejectRefresh  = false;
			bool                              OfflineRefresh = false;
			std::shared_ptr<std::atomic_bool> AuthorizationPending;
			std::size_t                       Submissions = 0U;
			std::size_t                       Renewals    = 0U;
			std::string                       AccessToken;
		};

		// [[BPR-0539]]. The identity of credential material, short enough to
		// print beside a course's other facts and one-way, so the evidence a
		// course prints is not the secret it is about. Tests-enabled only,
		// like everything else in this file, and used on a fixture credential
		// whose plaintext this process already scans the disk for.
		[[nodiscard]] auto credential_digest(
			const std::span<const std::byte> aBytes) -> std::string
		{
			auto hash = QCryptographicHash{ QCryptographicHash::Sha256 };
			hash.addData(QByteArrayView{
				reinterpret_cast<const char*>(aBytes.data()),
				static_cast<qsizetype>(aBytes.size()) });
			return hash.result().toHex().left(16).toStdString();
		}

	} // namespace

	[[nodiscard]] auto verify_large_review_history_session_interleaving() -> bool
	{
		using namespace adapters::github;
		const auto profile = validate_github_host_profile(github_host_profile_candidate{
			.Profile = core::host_profile_id{ 11U }, .Host = core::git_host_id{ 22U }, .Revision = application::github_profile_revision{ 3U }, .Server = application::github_server_contract::github_com_2022_11_28, .Authentication = application::github_authentication_transport::native_https, .Proxy = application::github_proxy_mode::system, .Trust = application::github_trust_mode::system, .Provenance = github_profile_provenance::explicit_user, .Hostname = "github.com", .RestRoot = "https://api.github.com", .GraphqlRoot = "https://api.github.com/graphql" });
		if (!profile) {
			return false;
		}
		const auto served     = descriptor(github_public_path_fixture::large_review_v1);
		auto       state      = std::make_shared<fixture_recording_state>(served, *profile, nullptr,
																		  github_epoch_fixture_phase::single, true, false, false, false, "");
		state->RepeatSessions = true;
		fixture_transport_factory factory{ state };
		const auto                renew = [&](core::application_session_id aSession) {
			auto material = application::github_credential_material::create(byte_vector("synthetic-session-race-token"));
			return factory.make_account(*profile, application::github_credential_retrieve_success{ .Context = { .Request = core::request_id{ 1U }, .ApplicationSession = aSession, .Authority = { .Profile = profile->profile(), .ProfileRevision = profile->revision(), .Host = profile->host(), .Hostname = application::github_canonical_hostname{ profile->hostname() }, .Account = served.AccountId, .ServerContract = profile->server_contract() }, .IdentityGeneration = core::github_identity_generation{ 1U } }, .Credential = std::move(*material) });
		};
		const auto firstSession  = core::application_session_id{ 1U };
		const auto secondSession = core::application_session_id{ 2U };
		auto       transport     = renew(firstSession);
		if (!transport) {
			return false;
		}
		for (const auto& exchange : load_recording(served, state->HistoryHeads.front())) {
			if (!state->Recording.perform(exchange.Request)) {
				return false;
			}
		}
		auto second = load_recording(served, state->HistoryHeads[1]);
		second.erase(second.begin());
		state->Recording.replace_completed_paged_script(second);
		state->HistoryIndex = 1U;
		for (const auto& exchange : second) {
			if (!state->Recording.perform(exchange.Request)) {
				return false;
			}
		}
		// [[BPR-1262]]. The correlation names its type: braced without it, the
		// nested aggregate is GCC 15's double-destroy shape ([[BPR-0816]]).
		github_wire_request request{
			.Correlation = adapters::github::detail::github_wire_correlation{ .Parent                = core::github_session_request_context{ .ApplicationSession = firstSession },
																			  .ProfileRevision       = profile->revision(),
																			  .Host                  = profile->host(),
																			  .Operation             = read_operation::get_repository,
																			  .AuthenticationSession = firstSession },
			.Wire        = second.front().Request
		};
		bool renewed = false;
		// Deterministically run the real renewal factory while S1 has released
		// the state lock, exactly where another thread could interleave.
		state->HistoryCompileInterleave = [&] { renewed = renew(secondSession).has_value(); };
		const auto stale                = (*transport)->perform(request, { });
		state->HistoryCompileInterleave = { };
		const auto* refused             = std::get_if<github_wire_error>(&stale);
		if (!renewed || !refused || refused->Error.Failure != application::github_provider_failure::authentication
			|| state->HistoryIndex != 1U || state->AccountReadCalls != 0U || state->AccountFactoryCalls != 2U) {
			return false;
		}
		request.Correlation.Parent                = core::github_session_request_context{ .ApplicationSession = secondSession };
		request.Correlation.AuthenticationSession = secondSession;
		const auto  current                       = (*transport)->perform(request, { });
		const auto* success                       = std::get_if<github_wire_success>(&current);
		return success && success->Response.Status == 200
			&& success->Response.Body == second.front().Response.Body
			&& success->Response.Completeness == second.front().Response.Completeness
			&& state->HistoryIndex == 1U && state->AccountReadCalls == 1U;
	}

	struct github_fixture_vault_state
	{
		std::size_t                                                     VaultStoreAttempts      = 0U;
		std::size_t                                                     VaultRetrieveAttempts   = 0U;
		std::size_t                                                     VaultDeleteAttempts     = 0U;
		std::size_t                                                     RefreshStoreAttempts    = 0U;
		std::size_t                                                     RefreshRetrieveAttempts = 0U;
		std::size_t                                                     RefreshDeleteAttempts   = 0U;
		std::optional<application::github_credential_operation_context> Context;
		std::optional<application::github_credential_material>          Credential;
		bool                                                            Stored    = false;
		bool                                                            Retrieved = false;
		bool                                                            Deleted   = false;
		// Counted, not just flagged. "The credential reached the vault"
		// and "the credential reached the vault exactly once" are
		// different claims, and RFC-BPR-0016 makes the second one.
		std::size_t Stores = 0U;
		// [[BPR-0539]]. The identity of the last material this vault
		// accepted, kept beside the count. A superseding delete may empty
		// the slot before a course reads its evidence, so what the store
		// counted has to be recorded at the store.
		std::string LastStoredDigest;
		// [[BPR-0540]]. The identity of the last credential the fixture
		// handed out for a connect, recorded where the store's own digest
		// is so a course can ask whether the material that reached the
		// vault is the one the reviewer pasted.
		std::string LastEnrollmentDigest;
		// The refresh slot, kept beside the credential rather than in
		// place of it, exactly as the port's contract describes. It exists
		// only under the refresh course; every other fixture run leaves it
		// composed-out and the slot answers `unavailable` as before.
		std::optional<application::github_credential_material> Refresh;
		std::size_t                                            RefreshStores    = 0U;
		std::size_t                                            RefreshRetrieves = 0U;
		// [[BPR-0547]]. How many erasures of the slot this vault accepted.
		// Retention is not visible in the slot alone: a course that reads
		// only whether material is there at the end cannot tell one
		// erasure from two, and the defect this counts for was exactly a
		// second erasure -- a rotation's own re-enrollment destroying what
		// the rotation had just stored -- hidden behind the shutdown's.
		std::size_t RefreshDeletes = 0U;
		// [[BPR-0547]]. The identity of the last material written into the
		// slot, recorded at the store for the reason the credential's own
		// digest above is: the orderly shutdown empties the slot before a
		// course reads its evidence, so what is still there at the end cannot
		// answer which rotation wrote it. A course that rotates twice needs
		// exactly that answer -- the second rotation's material, not the
		// first's, and no erasure between the two.
		std::string LastRefreshDigest;
		// [[BPR-0548]]. The contexts whose operation this vault has
		// accepted and not yet begun executing, which is what every
		// platform vault keys its pending map by. Guarded by `Mutex` with
		// the rest of the slot state.
		std::set<application::github_credential_operation_context> Pending;
		mutable std::mutex                                         Mutex;
	};

	auto make_shared_github_fixture_vault() -> std::shared_ptr<github_fixture_vault_state> { return std::make_shared<github_fixture_vault_state>(); }

	auto export_github_fixture_vault(const std::shared_ptr<github_fixture_vault_state>& aState) -> std::string
	{
		if (!aState) {
			return { };
		}
		const std::scoped_lock lock{ aState->Mutex };
		if (!aState->Context || !aState->Credential || !aState->Refresh || !aState->Pending.empty()) {
			return { };
		}
		if (aState->Credential->size() > 128U || aState->Refresh->size() > 128U) {
			return { };
		}
		const auto& context   = *aState->Context;
		const auto& authority = context.Authority;
		if (!application::valid(context) || authority.Hostname.value() != "github.com" || authority.BprProfile != core::default_bpr_profile_id
			|| authority.ServerContract != application::github_server_contract::github_com_2022_11_28) {
			return { };
		}
		QByteArray  bytes;
		QDataStream stream{ &bytes, QIODevice::WriteOnly };
		stream.setVersion(QDataStream::Qt_6_8);
		stream << quint64{ context.Request.value() } << quint64{ context.ApplicationSession.value() }
			   << quint64{ context.IdentityGeneration.value() } << quint64{ authority.Profile.value() }
			   << quint64{ authority.ProfileRevision.value() } << quint64{ authority.Host.value() } << quint64{ authority.Account.value() };
		const auto material = [](const application::github_credential_material& aMaterial) {
			return QByteArray{ reinterpret_cast<const char*>(aMaterial.bytes().data()), static_cast<qsizetype>(aMaterial.size()) };
		};
		stream << material(*aState->Credential) << material(*aState->Refresh);
		// Hex keeps the dedicated pipe portable across Windows text-mode streams.
		return bytes.size() <= 1024 ? bytes.toHex().toStdString() : std::string{ };
	}

	auto import_github_fixture_vault(std::string_view aBytes) -> std::shared_ptr<github_fixture_vault_state>
	{
		if (aBytes.empty() || aBytes.size() > 2048U) {
			return nullptr;
		}
		const QByteArray encoded{ aBytes.data(), static_cast<qsizetype>(aBytes.size()) };
		const auto       decoded = QByteArray::fromHex(encoded);
		if (decoded.toHex() != encoded) {
			return nullptr;
		}
		QDataStream stream{ decoded };
		stream.setVersion(QDataStream::Qt_6_8);
		quint64 request{ }, session{ }, generation{ }, profile{ }, revision{ }, host{ }, account{ };
		stream >> request >> session >> generation >> profile >> revision >> host >> account;
		const auto readMaterial = [&stream]() {
			quint32 size{ };
			stream >> size;
			if (size > 128U) {
				stream.setStatus(QDataStream::ReadCorruptData);
				return QByteArray{ };
			}
			QByteArray result(static_cast<qsizetype>(size), '\0');
			if (stream.readRawData(result.data(), static_cast<qint64>(size)) != static_cast<qint64>(size)) {
				stream.setStatus(QDataStream::ReadPastEnd);
			}
			return result;
		};
		const auto access  = readMaterial();
		const auto refresh = readMaterial();
		if (stream.status() != QDataStream::Ok || !stream.atEnd()
			|| access != "fixture-access-token-0276" || refresh != "fixture-refresh-token-0276") {
			return nullptr;
		}
		auto state     = make_shared_github_fixture_vault();
		state->Context = application::github_credential_operation_context{
			.Request = core::request_id{ request }, .ApplicationSession = core::application_session_id{ session }, .Authority = { .Profile = core::host_profile_id{ profile }, .ProfileRevision = application::github_profile_revision{ revision }, .Host = core::git_host_id{ host }, .Hostname = application::github_canonical_hostname{ "github.com" }, .Account = core::github_account_id{ account } }, .IdentityGeneration = core::github_identity_generation{ generation }
		};
		if (!application::valid(*state->Context)) {
			return nullptr;
		}
		auto credential      = application::github_credential_material::create(std::as_bytes(std::span{ access.data(), static_cast<std::size_t>(access.size()) }));
		auto refreshMaterial = application::github_credential_material::create(std::as_bytes(std::span{ refresh.data(), static_cast<std::size_t>(refresh.size()) }));
		if (!credential || !refreshMaterial) {
			return nullptr;
		}
		state->Credential = std::move(*credential);
		state->Refresh    = std::move(*refreshMaterial);
		return state;
	}

	namespace {
		// [[BPR-0548]]. One pending operation per operation context, claimed
		// before the work is queued and released when it begins, exactly as
		// `queue`/`execute` do in every platform vault
		// (`macos_keychain.cpp`, `linux_secret_service.cpp`,
		// `windows_credential_manager.cpp`).
		[[nodiscard]] auto claim_pending(
			const std::shared_ptr<github_fixture_vault_state>&      aState,
			const application::github_credential_operation_context& aContext)
			-> bool
		{
			const std::scoped_lock lock{ aState->Mutex };
			return aState->Pending.insert(aContext).second;
		}

		void release_pending(
			const std::shared_ptr<github_fixture_vault_state>&      aState,
			const application::github_credential_operation_context& aContext)
		{
			const std::scoped_lock lock{ aState->Mutex };
			aState->Pending.erase(aContext);
		}

		// [[BPR-0548]]. This vault stands in for a platform vault, so it has
		// to refuse everything a platform vault refuses -- a fixture more
		// permissive than the thing it stands in for cannot falsify anything
		// about it. Two rules are mirrored from
		// `src/adapters/github/src/macos_keychain.cpp` and its Linux and
		// Windows siblings, which enforce both on all six port methods:
		//
		//  1. `valid()` on the context or enrollment, and a callable
		//     completion, checked before anything is queued; the refusal is
		//     `invalid_request`. It is there because a context that names no
		//     request, no application session, no authority, or identity
		//     generation zero cannot address a stored secret, and a vault that
		//     guesses would answer one session's query from another's slot.
		//     Both slots are checked, because the refresh slot holds material
		//     of exactly the same kind.
		//  2. One pending operation per `github_credential_operation_context`,
		//     a `std::map`/`std::set` keyed by that context with
		//     `try_emplace`, the duplicate refused `invalid_request` before it
		//     is queued (pinned as "duplicate store is not queued" in
		//     `tests/github_contract/macos_keychain_contract_tests.cpp`). It
		//     is there because the context is the key a completion, a
		//     cancellation, and the executing task are all routed by: two live
		//     operations under one context could not be told apart by
		//     `cancel`, and their terminals could not be attributed.
		//
		// What is deliberately *not* mirrored is the platform's storage --
		// this vault stays the in-memory, authority-keyed stand-in it was.
		// Only its refusals match.
		class fixture_credential_vault final
			: public application::github_credential_vault_port
		{
		public:
			// Two flags rather than one, because they answer different
			// questions. `aRefreshSlot` is whether this vault has a second
			// slot to rotate material through at all, which only the renewal
			// course needs. `aAuthorityKeyed` is which key the slot it
			// already has is matched on, and that is a property of every
			// course whose credential arrives from the device grant --
			// [[BPR-0539]] made the registered course one of them, so the two
			// courses match the way the product's vault does rather than one
			// each way.
			fixture_credential_vault(
				application::effect_executor&               aEffects,
				std::shared_ptr<github_fixture_vault_state> aState,
				const bool                                  aRefreshSlot,
				const bool aAuthorityKeyed, core::bpr_profile_id aProfile, const bool aLocked)
				: Effects{ aEffects }
				, State{ std::move(aState) }
				, RefreshSlot{ aRefreshSlot }
				, AuthorityKeyed{ aAuthorityKeyed }
				, Profile{ aProfile }
				, Locked{ aLocked }
			{
			}

			void store(
				application::github_credential_enrollment               aEnrollment,
				application::github_credential_store_completion_handler aCompletion)
				override
			{
				count_attempt(&github_fixture_vault_state::VaultStoreAttempts);
				if (!admit_store(aEnrollment, aCompletion)) {
					return;
				}
				auto pending = std::make_shared<
					application::github_credential_enrollment>(
					std::move(aEnrollment));
				Effects.post(
					[state             = State,
					 pendingEnrollment = std::move(pending),
					 completion        = std::move(aCompletion)]() mutable {
						const auto context = pendingEnrollment->Context;
						release_pending(state, context);
						auto digest = credential_digest(
							pendingEnrollment->Credential.bytes());
						{
							const std::scoped_lock lock{ state->Mutex };
							state->Context          = context;
							state->Credential       = std::move(pendingEnrollment->Credential);
							state->Stored           = true;
							state->LastStoredDigest = std::move(digest);
							++state->Stores;
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
				count_attempt(&github_fixture_vault_state::VaultRetrieveAttempts);
				if (!admit_retrieve(aContext, aCompletion)) {
					return;
				}
				Effects.post(
					[state  = State,
					 keyed  = AuthorityKeyed,
					 locked = Locked,
					 aContext,
					 completion = std::move(aCompletion)]() mutable {
						release_pending(state, aContext);
						if (locked) {
							completion(application::github_credential_retrieve_error{
								.Context = aContext,
								.Failure = application::github_credential_vault_failure::locked,
							});
							return;
						}
						auto material = std::optional<
							application::github_credential_material>{ };
						{
							const std::scoped_lock lock{ state->Mutex };
							// Off the device courses the match is the exact
							// operation context, which is what the fixture's
							// own enrollment produces and what keeps this slot
							// from answering for a session it never stored.
							// Under them the credential arrives from the
							// device grant instead, and the retrieve that
							// re-establishes it is a different operation under
							// the same authority -- which is the key the slot
							// is defined by, and the one the product's vault
							// matches on.
							const auto matched = state->Context.has_value()
								&& (keyed
										? state->Context->Authority
											== aContext.Authority
										: *state->Context == aContext);
							if (matched && state->Credential.has_value()) {
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
				count_attempt(&github_fixture_vault_state::VaultDeleteAttempts);
				if (!admit_delete(aContext, aCompletion)) {
					return;
				}
				Effects.post(
					[state = State,
					 keyed = AuthorityKeyed,
					 aContext,
					 completion = std::move(aCompletion)]() mutable {
						release_pending(state, aContext);
						auto disposition  = application::github_credential_delete_disposition::absent;
						auto exactContext = true;
						{
							const std::scoped_lock lock{ state->Mutex };
							// The same key the retrieve above is defined by.
							// A real vault deletes what the authority names;
							// the operation context also carries the request
							// and the identity generation, and the product
							// deletes a superseded credential under the *new*
							// generation -- so an exact-context match refuses
							// the one delete every re-enrollment performs.
							const auto matched = !state->Context.has_value()
								|| (keyed ? state->Context->Authority
											== aContext.Authority
										  : *state->Context == aContext);
							if (!matched) {
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

			// Without the refresh course the fixture models the public read
			// path, which has no refresh material to hold, and the slot reports
			// itself unavailable rather than pretending to store anything.
			// Under the course it is a real second slot: renewal has to be able
			// to write material, read it back on the next turn, and overwrite
			// it, because a rotation that does not replace what it rotated is
			// not a rotation.
			void store_refresh(
				application::github_credential_enrollment               aEnrollment,
				application::github_credential_store_completion_handler aCompletion)
				override
			{
				count_attempt(&github_fixture_vault_state::RefreshStoreAttempts);
				if (!admit_store(aEnrollment, aCompletion)) {
					return;
				}
				auto pending = std::make_shared<
					application::github_credential_enrollment>(
					std::move(aEnrollment));
				Effects.post(
					[state             = State,
					 slot              = RefreshSlot,
					 pendingEnrollment = std::move(pending),
					 completion        = std::move(aCompletion)]() mutable {
						const auto context = pendingEnrollment->Context;
						release_pending(state, context);
						if (!slot) {
							pendingEnrollment->Credential.reset();
							completion(
								application::github_credential_store_error{
									.Context = context,
									.Failure = application::
										github_credential_vault_failure::
											unavailable,
								});
							return;
						}
						{
							const std::lock_guard guard{ state->Mutex };
							state->Refresh = std::move(pendingEnrollment->Credential);
							++state->RefreshStores;
							if (state->Refresh.has_value()) {
								state->LastRefreshDigest
									= credential_digest(state->Refresh->bytes());
							}
						}
						completion(application::github_credential_store_success{
							.Context     = context,
							.Disposition = application::
								github_credential_store_disposition::stored,
						});
					});
			}

			void retrieve_refresh(
				const application::github_credential_operation_context     aContext,
				application::github_credential_retrieve_completion_handler aCompletion)
				override
			{
				count_attempt(&github_fixture_vault_state::RefreshRetrieveAttempts);
				if (!admit_retrieve(aContext, aCompletion)) {
					return;
				}
				Effects.post(
					[state = State,
					 slot  = RefreshSlot,
					 aContext,
					 completion = std::move(aCompletion)]() mutable {
						release_pending(state, aContext);
						if (!slot) {
							completion(
								application::github_credential_retrieve_error{
									.Context = aContext,
									.Failure = application::
										github_credential_vault_failure::
											unavailable,
								});
							return;
						}
						auto copy
							= std::optional<application::
												github_credential_material>{ };
						{
							const std::lock_guard guard{ state->Mutex };
							++state->RefreshRetrieves;
							if (state->Refresh.has_value()) {
								auto created = application::
									github_credential_material::create(
										state->Refresh->bytes());
								if (created.has_value()) {
									copy.emplace(std::move(*created));
								}
							}
						}
						if (!copy.has_value()) {
							completion(
								application::github_credential_retrieve_absent{
									.Context = aContext,
								});
							return;
						}
						completion(
							application::github_credential_retrieve_success{
								.Context    = aContext,
								.Credential = std::move(*copy),
							});
					});
			}

			void delete_refresh(
				const application::github_credential_operation_context   aContext,
				application::github_credential_delete_completion_handler aCompletion)
				override
			{
				count_attempt(&github_fixture_vault_state::RefreshDeleteAttempts);
				if (!admit_delete(aContext, aCompletion)) {
					return;
				}
				Effects.post(
					[state = State,
					 slot  = RefreshSlot,
					 aContext,
					 completion = std::move(aCompletion)]() mutable {
						release_pending(state, aContext);
						{
							const std::lock_guard countGuard{ state->Mutex };
							++state->RefreshDeletes;
						}
						if (!slot) {
							completion(
								application::github_credential_delete_error{
									.Context = aContext,
									.Failure = application::
										github_credential_vault_failure::
											unavailable,
								});
							return;
						}
						auto held = false;
						{
							const std::lock_guard guard{ state->Mutex };
							held = state->Refresh.has_value();
							state->Refresh.reset();
						}
						completion(application::github_credential_delete_success{
							.Context     = aContext,
							.Disposition = held
								? application::
									  github_credential_delete_disposition::deleted
								: application::
									  github_credential_delete_disposition::absent,
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
			// The two mirrored rules, applied before anything is queued and
			// answered with the platform vaults' typed refusal. A refused
			// request is never queued, and the refusal is still delivered on
			// the effect lane, so this vault keeps its own discipline of
			// exactly one terminal per accepted call, published from the lane.
			void count_attempt(std::size_t github_fixture_vault_state::* aCounter)
			{
				const std::scoped_lock lock{ State->Mutex };
				++(State.get()->*aCounter);
			}
			[[nodiscard]] auto admit_store(
				application::github_credential_enrollment& aEnrollment,
				application::github_credential_store_completion_handler&
					aCompletion) -> bool
			{
				const auto context = aEnrollment.Context;
				if (application::valid(aEnrollment) && aEnrollment.Context.Authority.BprProfile == Profile && aCompletion
					&& claim_pending(State, context)) {
					return true;
				}
				aEnrollment.Credential.reset();
				if (!aCompletion) {
					return false;
				}
				Effects.post(
					[context, completion = std::move(aCompletion)]() mutable {
						completion(application::github_credential_store_error{
							.Context = context,
							.Failure = application::
								github_credential_vault_failure::invalid_request,
						});
					});
				return false;
			}

			[[nodiscard]] auto admit_retrieve(
				const application::github_credential_operation_context& aContext,
				application::github_credential_retrieve_completion_handler&
					aCompletion) -> bool
			{
				if (application::valid(aContext) && aContext.Authority.BprProfile == Profile && aCompletion
					&& claim_pending(State, aContext)) {
					return true;
				}
				if (!aCompletion) {
					return false;
				}
				Effects.post(
					[aContext, completion = std::move(aCompletion)]() mutable {
						completion(application::github_credential_retrieve_error{
							.Context = aContext,
							.Failure = application::
								github_credential_vault_failure::invalid_request,
						});
					});
				return false;
			}

			[[nodiscard]] auto admit_delete(
				const application::github_credential_operation_context& aContext,
				application::github_credential_delete_completion_handler&
					aCompletion) -> bool
			{
				if (application::valid(aContext) && aContext.Authority.BprProfile == Profile && aCompletion
					&& claim_pending(State, aContext)) {
					return true;
				}
				if (!aCompletion) {
					return false;
				}
				Effects.post(
					[aContext, completion = std::move(aCompletion)]() mutable {
						completion(application::github_credential_delete_error{
							.Context = aContext,
							.Failure = application::
								github_credential_vault_failure::invalid_request,
						});
					});
				return false;
			}

			application::effect_executor&               Effects;
			std::shared_ptr<github_fixture_vault_state> State;
			bool                                        RefreshSlot    = false;
			bool                                        AuthorityKeyed = false;
			core::bpr_profile_id                        Profile;
			bool                                        Locked = false;
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

		// [[BPR-0571]]. The ordinary fixture provider delegates root publication
		// to the same session as production. Only HTTP answers and its lease are
		// synthetic; request compilation, observations, negotiation and SQLite
		// durability all run through the product implementation.
		class fixture_publication_journal final : public application::review_mutation_recovery_port
		{
		public:
			using failure     = application::review_mutation_recovery_failure;
			using record      = application::review_mutation_recovery_record;
			using publication = application::review_publication_recovery_record;
			fixture_publication_journal(std::unique_ptr<application::review_mutation_recovery_port> aInner, std::string& aKey)
				: Inner{ std::move(aInner) }
				, Key{ aKey }
			{ }
			auto load(const std::string& aKey) -> std::expected<std::optional<record>, failure> override { return Inner->load(aKey); }
			auto admit(const record& aRecord) -> std::expected<application::review_mutation_recovery_admission, failure> override { return Inner->admit(aRecord); }
			auto mark_possibly_transmitted(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint) -> std::expected<record, failure> override { return Inner->mark_possibly_transmitted(aKey, aFingerprint); }
			auto resolve(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint, const application::review_mutation_receipt& aReceipt) -> std::expected<record, failure> override { return Inner->resolve(aKey, aFingerprint, aReceipt); }
			auto load_publication(const std::string& aKey) -> std::expected<std::optional<publication>, failure> override
			{
				Key = aKey;
				return Inner->load_publication(aKey);
			}
			auto advance_publication(const std::optional<publication>& aExpected, const publication& aNext) -> std::expected<publication, failure> override { return Inner->advance_publication(aExpected, aNext); }

		private:
			std::unique_ptr<application::review_mutation_recovery_port> Inner;
			std::string&                                                Key;
		};
		class fixture_publication_lease final : public application::review_mutation_transport_lease
		{ };
		// The same synthetic acquisition seam applies to the coordinator's target
		// observation and the session. Refusal can precede any journal read.
		auto admit_fixture_cli_operation(const std::shared_ptr<fixture_recording_state>& aState, const std::stop_token aStop)
			-> std::expected<void, application::review_mutation_failure>
		{
			if (aState && aState->CliEnabled && aState->PublishedTour) {
				const auto lock = std::scoped_lock{ aState->PublishedTour->Mutex };
				++aState->PublishedTour->CliAcquisitions;
				if (aStop.stop_requested()) {
					return std::unexpected(application::review_mutation_failure::cancelled);
				}
				if (aState->PublishedTour->CliAcquisitionFailure) {
					return std::unexpected(*aState->PublishedTour->CliAcquisitionFailure);
				}
			}
			return { };
		}
		class fixture_publication_wire final : public adapters::github::detail::github_mutation_wire_transport, public std::enable_shared_from_this<fixture_publication_wire>
		{
		public:
			fixture_publication_wire(std::shared_ptr<fixture_recording_state>  aState,
									 application::review_mutation_live_binding aBinding, review_mutation_fixture_evidence& aEvidence, bool aLoseFirst)
				: State{ std::move(aState) }
				, Binding{ std::move(aBinding) }
				, Evidence{ aEvidence }
				, LoseFirst{ aLoseFirst }
			{ }
			auto begin_mutation_operation(std::stop_token aStop) -> adapters::github::detail::github_mutation_operation override
			{
				if (const auto admitted = admit_fixture_cli_operation(State, aStop); !admitted) {
					return std::unexpected(admitted.error());
				}
				if (State->CliEnabled && State->PublishedTour) {
					return std::shared_ptr<adapters::github::detail::github_mutation_wire_transport>{ shared_from_this() };
				}
				return std::shared_ptr<adapters::github::detail::github_mutation_wire_transport>{ };
			}

			auto perform_review_mutation(const adapters::github::detail::github_mutation_wire_request& aRequest,
										 const std::stop_token aStop, adapters::github::detail::github_mutation_transmission_witness aWitness)
				-> adapters::github::detail::github_mutation_wire_completion override
			{
				using namespace adapters::github::detail;
				using operation  = review_mutation_operation;
				const auto& wire = aRequest.Wire;
				if (aStop.stop_requested() || !validate_outbound_review_mutation_request(wire)) {
					return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unsupported };
				}
				auto        response = review_mutation_wire_response{ .Status = 200, .Headers = { { .Name = "Content-Type", .Value = "application/json" } } };
				const auto& target   = Binding.Target;
				const auto& actor    = Binding.ActingIdentity;
				const auto  text     = [](const std::string_view aValue) { return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size())); };
				const auto  user     = QJsonObject{ { "id", static_cast<qint64>(actor.DatabaseId) }, { "node_id", text(actor.NodeId) }, { "login", text(actor.Login) } };
				QJsonObject object;
				if (wire.Operation == operation::observe_viewer) {
					object = user;
				} else if (wire.Operation == operation::observe_pull_request) {
					object = { { "id", static_cast<qint64>(target.PullRequestDatabaseId) }, { "node_id", text(target.PullRequestNodeId) }, { "number", static_cast<qint64>(target.Number.value()) }, { "state", "open" }, { "base", QJsonObject{ { "sha", text(core::canonical_hex(target.BaseOid).view()) }, { "ref", text(target.BaseRef) }, { "repo", QJsonObject{ { "id", static_cast<qint64>(target.RepositoryDatabaseId) }, { "node_id", text(target.RepositoryNodeId) }, { "full_name", text(target.Repository.Owner + "/" + target.Repository.Name) } } } } }, { "head", QJsonObject{ { "sha", text(core::canonical_hex(target.Semantic.ExpectedHead).view()) }, { "ref", text(target.HeadRef) } } } };
				} else if (wire.Operation == operation::observe_review_comments) {
					if (HideReads) {
						return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable };
					}
					response.Body = QJsonDocument{ Comments }.toJson(QJsonDocument::Compact).toStdString();
					return github_mutation_wire_success{ .Response = std::move(response) };
				} else if (wire.Operation == operation::create_review_comment) {
					if (!aWitness || !aWitness() || aWitness()) {
						return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable };
					}
					const auto& request = std::get<application::review_mutation_request>(wire.Semantic);
					const auto  file    = request.Representation == core::review_publication_representation::file_comment_with_permalink;
					if (file) {
						++Evidence.FilePosts;
					} else {
						++Evidence.NativePosts;
					}
					// [[BPR-1084]]. The decoded bytes and the line of each attempt
					// as the fixture server receives them, refused or not.
					{
						const auto sent   = QJsonDocument::fromJson(QByteArray::fromStdString(wire.Body)).object();
						Evidence.LastBody = sent.value("body").toString().toStdString();
						Evidence.LastPath = sent.value("path").toString().toStdString();
						Evidence.LastSide = sent.value("side").toString().toStdString();
						Evidence.LastLine = sent.value("line").isDouble() ? std::optional<std::uint32_t>{ static_cast<std::uint32_t>(sent.value("line").toInt()) } : std::nullopt;
						Evidence.LastFile = file;
						// [[BPR-1212]]. Present only when the request carried them.
						Evidence.LastStartLine = sent.contains("start_line") && sent.value("start_line").isDouble() ? std::optional<std::uint32_t>{ static_cast<std::uint32_t>(sent.value("start_line").toInt()) } : std::nullopt;
						Evidence.LastStartSide = sent.contains("start_side") ? std::optional<std::string>{ sent.value("start_side").toString().toStdString() } : std::nullopt;
					}
					// [[BPR-1084]]. A suggested change the label-authoring course
					// writes on a line GitHub will not take an inline comment on.
					if (!file && (request.Proposal.Body == "BPR-0571 off-hunk publication." || request.Proposal.Body.find("BPR-1084 off-hunk") != std::string::npos)) {
						return github_mutation_wire_success{ .Response = { .Status = 422, .Headers = response.Headers, .Body = R"({"message":"Validation Failed","errors":[{"resource":"PullRequestReviewComment","field":"line","code":"fixture_off_hunk_unsupported","message":"Fixture: no comment created for off-hunk line"}]})" } };
					}
					object        = QJsonDocument::fromJson(QByteArray::fromStdString(wire.Body)).object();
					const auto id = QStringLiteral("PRRC_composed_%1").arg(Comments.size() + 1);
					object.insert("id", Comments.size() + 100);
					object.insert("node_id", id);
					object.insert("user", user);
					Comments.append(object);
					Evidence.StoredComments = static_cast<std::size_t>(Comments.size());
					const auto line         = file ? QJsonValue{ QJsonValue::Null } : object.value("line");
					auto       post         = QJsonObject{ { "id", id }, { "body", object.value("body") }, { "bodyText", object.value("body") }, { "path", object.value("path") }, { "line", line }, { "originalLine", line }, { "outdated", false }, { "author", QJsonObject{ { "id", text(actor.NodeId) }, { "login", text(actor.Login) } } }, { "replyTo", QJsonValue{ QJsonValue::Null } } };
					// [[BPR-1212]]. A range's thread reads back with its first line.
					if (!file && object.contains("start_line")) {
						post.insert("startLine", object.value("start_line"));
						post.insert("originalStartLine", object.value("start_line"));
					}
					const auto thread = QJsonObject{ { "id", QStringLiteral("PRRT_composed_%1").arg(Comments.size()) },
													 { "isResolved", false },
													 { "isOutdated", false },
													 { "viewerCanResolve", true },
													 { "viewerCanUnresolve", false },
													 { "subjectType", file ? "FILE" : "LINE" },
													 { "path", object.value("path") },
													 { "diffSide", file ? QJsonValue{ "RIGHT" } : object.value("side") },
													 { "line", line },
													 { "originalLine", line },
													 { "comments", QJsonObject{ { "totalCount", 1 }, { "nodes", QJsonArray{ post } }, { "pageInfo", QJsonObject{ { "hasNextPage", false }, { "endCursor", "composed-end" } } } } } };
					{
						const std::scoped_lock lock{ State->Mutex };
						State->CreatedReviewThreads.push_back(thread);
					}
					response.Status = 201;
					if (LoseFirst && !Evidence.LostResult) {
						Evidence.LostResult = true;
						HideReads           = true;
						return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable, .Transmission = github_mutation_transmission::indeterminate };
					}
				} else {
					return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unsupported };
				}
				response.Body = QJsonDocument{ object }.toJson(QJsonDocument::Compact).toStdString();
				return github_mutation_wire_success{ .Response = std::move(response) };
			}
			void               allow_reads() { HideReads = false; }
			[[nodiscard]] auto projections() const -> std::vector<application::review_comment_projection>
			{
				auto result = std::vector<application::review_comment_projection>{ };
				for (const auto& comment : Comments) {
					result.push_back({ .ServerId = comment.toObject().value("node_id").toString().toStdString(), .Body = comment.toObject().value("body").toString().toStdString(), .EditVersion = 1U });
				}
				return result;
			}

		private:
			std::shared_ptr<fixture_recording_state>  State;
			application::review_mutation_live_binding Binding;
			review_mutation_fixture_evidence&         Evidence;
			QJsonArray                                Comments;
			bool                                      HideReads = false;
			bool                                      LoseFirst = false;
		};

		class fixture_review_mutation_provider final
			: public application::review_mutation_port,
			  public application::review_publication_lineage_port
		{
		public:
			explicit fixture_review_mutation_provider(
				const bool                               aLoseFirst,
				std::shared_ptr<fixture_recording_state> aState           = { },
				const bool                               aResolvesThreads = false,
				std::filesystem::path                    aStorageRoot     = { }) noexcept
				: RealPublication{ aLoseFirst }
				, StorageRoot{ std::move(aStorageRoot) }
				, LoseFirst{ aLoseFirst }
				, ResolvesThreads{ aResolvesThreads }
				, State{ std::move(aState) }
			{
			}

			void set_lineage(std::shared_ptr<application::review_publication_lineage_port> aLineage)
			{
				const auto lock = std::scoped_lock{ Mutex };
				std::erase_if(Lineages, [](const auto& aWeak) { return aWeak.expired(); });
				if (std::ranges::none_of(Lineages, [&](const auto& aWeak) { return aWeak.lock() == aLineage; })) {
					Lineages.push_back(aLineage);
				}
			}
			auto prepare(const application::review_mutation_request& aRequest, std::string_view aLogical, std::string_view aWire) -> bool override
			{
				if (!aRequest.Proposal.Durable) {
					return true;
				}
				for (const auto& weak : Lineages) {
					if (auto host = weak.lock(); host && host->prepare(aRequest, aLogical, aWire)) {
						return true;
					}
				}
				return false;
			}
			auto complete(const application::review_mutation_request& aRequest, std::string_view aLogical, std::string_view aWire, const application::review_mutation_receipt& aReceipt) -> bool override
			{
				if (!aRequest.Proposal.Durable) {
					return true;
				}
				for (const auto& weak : Lineages) {
					if (auto host = weak.lock(); host && host->complete(aRequest, aLogical, aWire, aReceipt)) {
						return true;
					}
				}
				return false;
			}
			void prepare(core::review_mutation_target aTarget)
			{
				const std::scoped_lock providerLock{ Mutex };
				Designated = std::move(aTarget);
				Reported   = Designated;
			}

			void report(core::review_mutation_target aTarget)
			{
				const std::scoped_lock providerLock{ Mutex };
				Reported = std::move(aTarget);
			}

			auto observe_target() -> std::expected<
				core::review_mutation_target,
				application::review_mutation_failure> override
			{
				return observe_target({ });
			}

			auto observe_target(const std::stop_token aStop) -> std::expected<
				core::review_mutation_target,
				application::review_mutation_failure> override
			{
				const std::scoped_lock providerLock{ Mutex };
				++Evidence.TargetObservations;
				if (const auto admitted = admit_fixture_cli_operation(State, aStop); !admitted) {
					return std::unexpected(admitted.error());
				}
				return Reported;
			}

			auto submit(const application::review_mutation_request& aRequest)
				-> std::expected<
					application::review_mutation_receipt,
					application::review_mutation_failure> override
			{
				const std::scoped_lock providerLock{ Mutex };
				++Evidence.Submissions;
				if (aRequest.Target != Designated) {
					return std::unexpected{
						application::review_mutation_failure::undesignated_target
					};
				}
				if ((RealPublication || aRequest.Proposal.Durable) && !aRequest.Proposal.ReplyToThread) {
					return submit_publication(aRequest);
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
				// [[BPR-0657]]. A reply lands under its thread on the next
				// read, as GitHub's would.
				if (aRequest.Proposal.ReplyToThread.has_value()) {
					++Evidence.Replies;
					if (State != nullptr) {
						const std::scoped_lock lock{ State->Mutex };
						State->ThreadOverlays[*aRequest.Proposal.ReplyToThread].Replies.emplace_back(
							identifier, aRequest.Proposal.Body);
					}
				}
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

			// [[BPR-0657]]. The thread flips on the next read; already in the
			// state asked for is the core's refusal, not the provider's.
			auto resolve_thread(
				const application::review_thread_resolution_request& aRequest,
				std::stop_token)
				-> std::expected<
					application::review_thread_resolution_receipt,
					application::review_mutation_failure> override
			{
				const std::scoped_lock providerLock{ Mutex };
				++Evidence.Resolutions;
				if (!ResolvesThreads) {
					return std::unexpected{ application::review_mutation_failure::unsupported };
				}
				if (aRequest.Target != Designated || aRequest.Thread.empty()) {
					return std::unexpected{
						application::review_mutation_failure::undesignated_target
					};
				}
				if (State != nullptr) {
					const std::scoped_lock lock{ State->Mutex };
					State->ThreadOverlays[aRequest.Thread].Resolved = aRequest.Resolve;
				}
				return application::review_thread_resolution_receipt{
					.Thread   = aRequest.Thread,
					.Resolved = aRequest.Resolve,
				};
			}

			[[nodiscard]] auto capabilities() const
				-> application::review_provider_capabilities override
			{
				return {
					.ResolvesThreads    = ResolvesThreads,
					.RecordsViewedState = false,
				};
			}

			[[nodiscard]] auto comments() const
				-> std::vector<application::review_comment_projection>
			{
				const std::scoped_lock providerLock{ Mutex };
				return Comments;
			}

			[[nodiscard]] auto evidence() const noexcept
				-> review_mutation_fixture_evidence
			{
				const std::scoped_lock providerLock{ Mutex };
				return Evidence;
			}

		private:
			auto submit_publication(const application::review_mutation_request& aRequest)
				-> std::expected<application::review_mutation_receipt, application::review_mutation_failure>
			{
				using namespace adapters::github::detail;
				if (!aRequest.LiveBinding || !aRequest.Source) {
					return std::unexpected{ application::review_mutation_failure::unsupported };
				}
				const auto& binding       = *aRequest.LiveBinding;
				const auto  configuration = adapters::sqlite::review_mutation_recovery_configuration{
					.StorageRoot = StorageRoot, .Host = std::string{ binding.Target.Hostname.value() }, .Account = std::to_string(binding.ActingIdentity.Account.value()), .Repository = binding.Target.Repository.Owner + "/" + binding.Target.Repository.Name, .PullRequest = static_cast<std::int64_t>(binding.Target.Number.value())
				};
				if (!Wire) {
					Wire = std::make_shared<fixture_publication_wire>(State, binding, Evidence, RealPublication);
				}
				Wire->allow_reads();
				if (!Session) {
					auto recovery = adapters::sqlite::make_review_mutation_recovery_store(configuration);
					if (!recovery) {
						return std::unexpected{ application::review_mutation_failure::unavailable };
					}
					const auto& authority = aRequest.Source->Comparison.Authority;
					auto        config    = live_review_mutation_configuration{
						.CredentialContext = { .Request = core::request_id{ 1U }, .ApplicationSession = core::application_session_id{ 1U }, .Authority = { .Profile = authority.Profile, .ProfileRevision = application::github_profile_revision{ authority.ProfileRevision.value() }, .Host = binding.Target.Semantic.Host, .Hostname = binding.Target.Hostname, .Account = binding.ActingIdentity.Account, .ServerContract = binding.Target.ServerContract }, .IdentityGeneration = core::github_identity_generation{ 1U } }, .Target = binding.Target, .ActingIdentity = binding.ActingIdentity
					};
					Session = std::make_unique<review_mutation_session>(*Wire, config, std::make_shared<fixture_publication_lease>(), std::make_unique<fixture_publication_journal>(std::move(*recovery), PublicationKey),
																		off_hunk_unsupported_contract{ .Server = binding.Target.ServerContract, .Resource = "PullRequestReviewComment", .Field = "line", .Code = "fixture_off_hunk_unsupported", .Message = "Fixture: no comment created for off-hunk line" }, this);
					++Evidence.SessionConstructions;
				}
				auto result                = Session->submit(aRequest);
				Comments                   = Wire->projections();
				Evidence.DuplicateReturned = Evidence.DuplicateReturned || (result && result->Duplicate);
				// A second connection reads what the product session committed.
				auto       reopened = adapters::sqlite::make_review_mutation_recovery_store(configuration);
				const auto stored   = reopened ? (*reopened)->load_publication(PublicationKey) : std::expected<std::optional<application::review_publication_recovery_record>, application::review_mutation_recovery_failure>{ std::unexpected{ application::review_mutation_recovery_failure::unavailable } };
				if (!stored || !*stored) {
					if (!result) {
						Session.reset();
						return result;
					}
					return std::unexpected{ application::review_mutation_failure::unavailable };
				}
				++Evidence.DurableJournalReads;
				if (!result) {
					Session.reset(); // retry must recover with a fresh session
				}
				return result;
			}
			std::vector<std::weak_ptr<application::review_publication_lineage_port>> Lineages;
			mutable std::mutex                                                       Mutex;
			bool                                                                     RealPublication = false;
			std::filesystem::path                                                    StorageRoot;
			std::string                                                              PublicationKey;
			std::shared_ptr<fixture_publication_wire>                                Wire;
			std::unique_ptr<adapters::github::detail::review_mutation_session>       Session;

			core::review_mutation_target                        Designated;
			core::review_mutation_target                        Reported;
			std::map<std::string, std::string>                  Identifiers;
			std::vector<application::review_comment_projection> Comments;
			review_mutation_fixture_evidence                    Evidence;
			bool                                                LoseFirst       = false;
			bool                                                ResolvesThreads = false;
			std::shared_ptr<fixture_recording_state>            State;
		};
	}

	auto github_public_path_fixture_serves_second_head(
		const github_public_path_fixture aFixture) noexcept -> bool
	{
		const auto served = descriptor(aFixture);
		return !served.HeadTwoOid.empty() || !served.NextPushOid.empty();
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
			auto served = descriptor(aFixture);
			if (aOptions.EpochPhase != github_epoch_fixture_phase::single && !served.NextPushOid.empty()) {
				served.HeadTwoOid       = served.NextPushOid;
				served.Packs.SecondFile = served.NextPushPack;
			}
			const auto distinctAccount = aOptions.DistinctProfileAccounts && aOptions.Profile != core::default_bpr_profile_id;
			if (distinctAccount) {
				served.AccountId = core::github_account_id{ 1000U + aOptions.Profile.value() };
			}
			if (aOptions.ModifiedMissingEndpointCourse) {
				if (aFixture != github_public_path_fixture::profile_window_restoration_v1
					|| aOptions.EpochPhase != github_epoch_fixture_phase::single) {
					throw std::runtime_error{ "Missing endpoint course requires the single reading fixture" };
				}
				served.BaseOid    = "3360e4b77ededc45be518f46a428dac9cffedf85";
				served.HeadOneOid = "523f7a961e06657890c604bce7ea86e4fef1a16a";
				served.HeadTwoOid = "";
				served.BaseBlob   = "";
				served.Packs      = { .File = "modified-missing-endpoint-v1/objects.pack" };
			}
			if (aOptions.GhesCliSourceUpdateCourse) {
				if (aFixture != github_public_path_fixture::ghes_3_18) {
					throw std::runtime_error{
						"GHES CLI source-update course requires the GHES fixture"
					};
				}
				const auto reading = descriptor(
					github_public_path_fixture::profile_window_restoration_v1);
				served.BaseOid       = reading.BaseOid;
				served.HeadOneOid    = reading.HeadOneOid;
				served.HeadTwoOid    = reading.HeadTwoOid;
				served.BaseBlob      = reading.BaseBlob;
				served.HeadOneBlob   = reading.HeadOneBlob;
				served.HeadTwoBlob   = reading.HeadTwoBlob;
				served.Packs         = reading.Packs;
				served.WindowReading = true;
			}
			if (aOptions.TimelineCourse == "ordinary") {
				if (aFixture != github_public_path_fixture::github_com || !aOptions.EmptyHistoricalTimeline) {
					throw std::runtime_error{ "Ordinary update requires the synthetic GitHub fixture and empty timeline" };
				}
				served.HeadTwoOid     = "b7b8dc0e5c4fbac8079b2c44bacfff5d36519c13";
				served.Packs          = { .File = "ordinary-update-v1/initial.pack", .SecondFile = "ordinary-update-v1/replacement.pack" };
				served.OrdinaryUpdate = true;
			}
			if (aOptions.TimelineCourse == "rewritten-base"
				&& aOptions.EpochPhase == github_epoch_fixture_phase::reuse) {
				// Replace the base with a separate root of identical material. The
				// old base is not its ancestor, and no historical head has a merge base.
				served.BaseOid = "80ec898e013c97d16dce3f6eae67e29701a988e5";
				served.Packs   = { .File = "../../git/epoch-history/v1/base-rewrite.pack", .SecondFile = "../../git/epoch-history/v1/base-rewrite.pack" };
			}
			Git       = make_git_fixture(served.Packs);
			Recording = std::make_shared<fixture_recording_state>(
				served, aProfile, Git, aOptions.EpochPhase, aOptions.LargeReviewHistory, aOptions.IndependentWindowReads,
				aOptions.EmptyHistoricalTimeline, aOptions.UnfetchableFirstHistoricalHead, aOptions.TimelineCourse);
			Recording->LargeReviewForeignPosts     = aFixture == github_public_path_fixture::large_review_v1 && aOptions.LargeReviewForeignPosts;
			Recording->CapabilityDiagnosticFailure = aOptions.CapabilityDiagnosticFailure;
			Recording->LargeReviewRepeatReads      = aFixture == github_public_path_fixture::large_review_v1 && aOptions.LargeReviewRepeatReads;
			Recording->RepeatSessions              = aOptions.DeviceRefreshCourse
				|| aOptions.PersistDeviceBindings
				|| aOptions.IndependentWindowReads;
			Recording->RepeatReads             = aOptions.RepeatReadSessions || aOptions.LoseFirstReviewMutationResult;
			Recording->RecentAccount           = aOptions.RecentAccount;
			Recording->DistinctAccount         = distinctAccount;
			Recording->ReviewTourCourse        = aOptions.ReviewTourCourse;
			Recording->PublishedTour           = aOptions.ReviewSubmissionServer;
			Recording->UnboundViewer           = aOptions.ViewerNamesAnotherAccount;
			Recording->BeforeFilesPage         = aOptions.BeforeFilesPage;
			Recording->BeforeViewerIdentity    = aOptions.BeforeViewerIdentity;
			Recording->CliEnabled              = aOptions.GithubCliDiscoveryAccounts.has_value();
			Recording->CliStrictCapability     = aOptions.GithubCliStrictCapability;
			Recording->RefuseFirstSmartAcquire = aOptions.RefuseFirstSmartAcquire;
			Recording->PersonalDiscovery       = std::move(aOptions.PersonalDiscovery);
			Recording->RateLimit               = std::move(aOptions.RateLimit);
			Recording->Offline                 = std::move(aOptions.Offline);
			Recording->ReadDelay               = aOptions.ReadDelay;
			Factory                            = std::make_shared<fixture_transport_factory>(Recording);
			VaultState                         = aOptions.SharedDeviceVault ? aOptions.SharedDeviceVault : make_shared_github_fixture_vault();
			// Every shipped platform vault has a real second slot since
			// [[BPR-0542]] and [[BPR-0543]], so the fixture's has one on every
			// course too: a shutdown that erases the refresh slot beside the
			// credential ([[BPR-0540]]) must meet a store that answers, not a
			// stub whose `unavailable` the host now rightly reports as a failed
			// erasure.
			Vault = std::make_unique<fixture_credential_vault>(
				aVaultEffects,
				VaultState,
				true,
				aOptions.RegisteredDeviceCourse, aOptions.Profile, aOptions.LockedDeviceVault);
			if (aOptions.SeedOrphanedRefreshMaterial) {
				// The shape a grant leaves behind: the documented refresh
				// token this fixture's own recorded exchange answers with.
				// Written straight into the slot rather than through the
				// port, because no session in this process put it there and
				// none could -- the run that did has exited.
				const std::string orphaned{ "fixture-refresh-token-0276" };
				auto              seeded = application::github_credential_material::create(
					std::as_bytes(std::span{ orphaned }));
				if (!seeded.has_value()) {
					throw std::runtime_error{
						"GitHub public-path fixture orphaned refresh seed failed"
					};
				}
				const std::lock_guard seedGuard{ VaultState->Mutex };
				VaultState->Refresh = std::move(*seeded);
			}
			DeviceTransport
				= std::make_unique<recorded_device_authorization_transport>(
					std::filesystem::path{
						BPR_GITHUB_DEVICE_AUTHORIZATION_FIXTURE_ROOT },
					std::filesystem::path{
						BPR_GITHUB_DEVICE_DOCUMENTED_FIXTURE_ROOT },
					aOptions.DeviceRefreshCourse || aOptions.PersistDeviceBindings,
					aOptions.RejectDeviceRefresh,
					aOptions.OfflineDeviceRefresh,
					aOptions.DeviceAuthorizationPending);
			Mutation = std::make_unique<fixture_review_mutation_provider>(
				aOptions.LoseFirstReviewMutationResult, Recording, aOptions.ResolvesThreads, aOptions.ApplicationDataRoot.value_or(std::filesystem::path{ }));

			if (!aOptions.ApplicationDataRoot) {
				throw std::runtime_error{ "fixture requires admitted storage root" };
			}
			ApplicationDataRoot = *aOptions.ApplicationDataRoot;
		}

		std::shared_ptr<fixture_recording_state>                 Recording;
		std::shared_ptr<deterministic_git_fixture>               Git;
		std::shared_ptr<fixture_transport_factory>               Factory;
		std::shared_ptr<github_fixture_vault_state>              VaultState;
		std::unique_ptr<fixture_credential_vault>                Vault;
		std::unique_ptr<recorded_device_authorization_transport> DeviceTransport;
		std::unique_ptr<fixture_review_mutation_provider>        Mutation;
		std::filesystem::path                                    ApplicationDataRoot;
		std::filesystem::path                                    GitExecutable;
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
		// [[BPR-0540]]. Digested on the way out rather than at the store,
		// because the question a connecting course asks is whether what the
		// vault accepted is what the fixture handed the connect path, and
		// only this end knows the second half. Each call mints new random
		// bytes, so the digest is recorded per hand-out and not per fixture.
		auto material = make_fixture_credential();
		{
			const std::scoped_lock enrollmentLock{ Implementation->VaultState->Mutex };
			Implementation->VaultState->LastEnrollmentDigest
				= credential_digest(material.bytes());
		}
		return material;
	}

	auto github_public_path_fixture_components::device_authorization_transport() noexcept
		-> application::device_authorization_transport_port&
	{
		return *Implementation->DeviceTransport;
	}

	auto github_public_path_fixture_components::device_authorization_access_token()
		const -> std::string
	{
		return Implementation->DeviceTransport->access_token();
	}

	auto github_public_path_fixture_components::evidence() const noexcept
		-> github_public_path_fixture_evidence
	{
		auto result = github_public_path_fixture_evidence{ };
		{
			const std::scoped_lock lock{ Implementation->Recording->Mutex };
			result.RecordingConsumed = Implementation->Recording->BootstrapRecording.consumed_all()
				&& Implementation->Recording->Recording.consumed_all();
			result.CapabilityProbeConsumed    = Implementation->Recording->CapabilityCalls == adapters::github::detail::capability_probe_part_count;
			result.BootstrapTransportSelected = Implementation->Recording->BootstrapFactoryCalls == 1U;
			result.AccountTransportSelected   = Implementation->Recording->AccountFactoryCalls == 1U;
			result.BootstrapFactoryAttempts   = Implementation->Recording->BootstrapFactoryAttempts;
			result.AccountFactoryAttempts     = Implementation->Recording->AccountFactoryAttempts;
			result.BootstrapFactoryCalls      = Implementation->Recording->BootstrapFactoryCalls;
			result.AccountFactoryCalls        = Implementation->Recording->AccountFactoryCalls;
			result.RecordingFailure           = Implementation->Recording->RecordingFailure;
			result.AccountReadCount           = static_cast<int>(
				Implementation->Recording->AccountReadCalls);
			result.LastReadOperation = Implementation->Recording->LastReadOperation;
			result.SmartAcquireCount = static_cast<int>(
				Implementation->Recording->SmartAcquireCalls);
			result.SmartAcquireRefusalCount = static_cast<int>(
				Implementation->Recording->SmartAcquireRefusals);
			result.SmartDiscoveryCount = static_cast<int>(
				Implementation->Recording->SmartDiscoveryCalls);
			result.SmartLsRefsCount = static_cast<int>(
				Implementation->Recording->SmartLsRefsCalls);
			result.SmartFetchCount = static_cast<int>(
				Implementation->Recording->SmartFetchCalls);
			result.DiscoverySearches     = Implementation->Recording->DiscoverySearches;
			result.DiscoveryDetailReads  = Implementation->Recording->DiscoveryDetailReads;
			result.DiscoverySearchTerms  = Implementation->Recording->DiscoverySearchTerms;
			result.AccountReadOperations = Implementation->Recording->AccountReadOperations;
			{
				const std::scoped_lock vaultLock{
					Implementation->VaultState->Mutex
				};
				result.VaultStores       = Implementation->VaultState->Stores;
				result.VaultStoredDigest = Implementation->VaultState->LastStoredDigest;
			}
			result.DeviceTransportSubmissions
				= Implementation->DeviceTransport->submissions();
			// [[BPR-0539]]. The comparison the store itself cannot make: the
			// grant's access token is known only once the recorded exchange
			// has answered, so the identity of what was stored is settled
			// here, against the token the transport issued. Both sides are
			// digests of the same bytes -- the enrolled material is the
			// token's own octets -- so equality is the claim that the one
			// store this course counts is the grant and not the 32 random
			// bytes a fixture connect would have enrolled.
			const auto grantToken           = Implementation->DeviceTransport->access_token();
			result.VaultStoredIsDeviceGrant = !grantToken.empty()
				&& !result.VaultStoredDigest.empty()
				&& result.VaultStoredDigest
					== credential_digest(std::as_bytes(std::span{ grantToken }));
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
			result.CredentialStored        = Implementation->VaultState->Stored;
			result.VaultStoreAttempts      = Implementation->VaultState->VaultStoreAttempts;
			result.VaultRetrieveAttempts   = Implementation->VaultState->VaultRetrieveAttempts;
			result.VaultDeleteAttempts     = Implementation->VaultState->VaultDeleteAttempts;
			result.RefreshStoreAttempts    = Implementation->VaultState->RefreshStoreAttempts;
			result.RefreshRetrieveAttempts = Implementation->VaultState->RefreshRetrieveAttempts;
			result.RefreshDeleteAttempts   = Implementation->VaultState->RefreshDeleteAttempts;
			result.CredentialRetrieved     = Implementation->VaultState->Retrieved;
			result.CredentialDeleted       = Implementation->VaultState->Deleted;
			result.SecretRetained          = Implementation->VaultState->Credential.has_value();
			result.RefreshRetained         = Implementation->VaultState->Refresh.has_value();
			result.RefreshStores           = Implementation->VaultState->RefreshStores;
			result.RefreshRetrieves        = Implementation->VaultState->RefreshRetrieves;
			result.RefreshDeletes          = Implementation->VaultState->RefreshDeletes;
			result.RefreshStoredDigest
				= Implementation->VaultState->LastRefreshDigest;
			result.LastEnrollmentDigest
				= Implementation->VaultState->LastEnrollmentDigest;
		}
		return result;
	}

	auto github_public_path_fixture_components::anonymous_smart_transport_factory() const noexcept
		-> std::shared_ptr<application::git_smart_transport_factory_port>
	{
		return Implementation->Factory->anonymous_smart_transport();
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

	void github_public_path_fixture_components::stage_server_conversation(std::string aThread, const bool aAddedAndResolved)
	{
		const std::scoped_lock lock{ Implementation->Recording->Mutex };
		auto&                  overlay = Implementation->Recording->ThreadOverlays[std::move(aThread)];
		overlay.Resolved               = aAddedAndResolved;
		overlay.RootBody               = aAddedAndResolved ? "Edited server root" : "Reopened server root";
		overlay.Replies.clear();
		if (aAddedAndResolved) {
			overlay.Replies.emplace_back("PRRC_shared_server_reply", "New server reply");
		}
	}

	void github_public_path_fixture_components::stage_server_root(std::string aThread, std::optional<std::string> aRootBody, std::optional<bool> aResolved,
																  std::optional<std::pair<std::string, std::string>> aRootAuthor)
	{
		const std::scoped_lock lock{ Implementation->Recording->Mutex };
		auto&                  overlay = Implementation->Recording->ThreadOverlays[std::move(aThread)];
		if (aRootBody) {
			overlay.RootBody = std::move(aRootBody);
		}
		if (aResolved) {
			overlay.Resolved = aResolved;
		}
		if (aRootAuthor) {
			overlay.RootAuthor = std::move(aRootAuthor);
		}
	}

	void github_public_path_fixture_components::stage_server_thread_deleted(std::string aThread)
	{
		const std::scoped_lock lock{ Implementation->Recording->Mutex };
		Implementation->Recording->ThreadOverlays[std::move(aThread)].Deleted = true;
	}

	void github_public_path_fixture_components::report_review_mutation_target(
		core::review_mutation_target aTarget)
	{
		Implementation->Mutation->report(std::move(aTarget));
	}

	void github_public_path_fixture_components::set_private_lineage(std::shared_ptr<application::review_publication_lineage_port> aLineage) { Implementation->Mutation->set_lineage(aLineage); }
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
