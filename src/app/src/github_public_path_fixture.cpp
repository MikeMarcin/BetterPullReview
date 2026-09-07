#include "github_public_path_fixture.hpp"
#include "large_review_fixture.hpp"

#include "github_capability_probe.hpp"
#include "github_device_authorization_contract.hpp"
#include "github_read_contract.hpp"

#include "bpr/adapters/sqlite/storage_root.hpp"
#include "github_wire_transport.hpp"

#include "bpr/application/git_smart_transport_port.hpp"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <glaze/json.hpp>

#include <algorithm>
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
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::app::detail {
	namespace {
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

		struct fixture_json_options : glz::opts
		{
			bool escape_control_characters    = true;
			bool validate_skipped             = true;
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
			bool                      Paged = false;
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
			if (aDescriptor.Paged) {
				return large_review_fixture::load(std::filesystem::path{ BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT } / aDescriptor.ContractDirectory, aHead);
			}
			const auto root = std::filesystem::path{ BPR_GITHUB_CONTRACT_FIXTURE_ROOT }
				/ aDescriptor.ContractDirectory;
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

		/// \brief Builds the recording a signed-out read replays.
		///
		/// \details One recording rather than a second fixture. Every REST body
		/// below is the body the authenticated script already serves, re-ordered
		/// into the order the anonymous authority issues its operations and cut
		/// down to the nine it may issue: viewer identity and the GraphQL threads
		/// are absent because that authority refuses both before a request
		/// exists, so a recording that carried them could only be reached by a
		/// read that had stopped being anonymous.
		///
		/// The inline review conversation is the one operation the authenticated
		/// script never records -- an authenticated read has the GraphQL threads
		/// instead -- so its request is derived from the recorded reviews request,
		/// which is exactly what the contract compiles for it: the same pull
		/// request path with its last segment replaced and the same first page.
		/// Deriving it rather than writing it out is what keeps the two in step;
		/// a compiled path that ever stopped matching fails the strict transport
		/// rather than passing against a hand-copied constant.
		[[nodiscard]] auto load_anonymous_recording(
			const fixture_descriptor& aDescriptor,
			const std::string_view    aHead)
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
			exchanges.reserve(anonymous_read_operations.size());
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
			explicit fixture_replay(std::vector<recorded_exchange> aExchanges, bool aPaged = false)
				: Strict{ aPaged ? std::vector<recorded_exchange>{ } : std::move(aExchanges) }
			{
				if (aPaged) {
					Paged.emplace(std::move(aExchanges));
				}
			}
			[[nodiscard]] auto perform(const adapters::github::detail::wire_request& aRequest)
				-> adapters::github::detail::response_result
			{
				return Paged ? Paged->perform(aRequest) : Strict.perform(aRequest);
			}
			[[nodiscard]] auto consumed_all() const noexcept -> bool
			{
				return Paged ? Paged->consumed_all() : Strict.consumed_all();
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
				if (Paged) {
					Paged->rewind();
				} else {
					Strict.rewind();
				}
			}
			/// [[BPR-0657]]. Replays the strict script from the exchange one
			/// operation starts at, for a refresh; a paged script has its own
			/// history and is left alone.
			void replay_from(const read_operation aOperation) noexcept
			{
				if (Paged) {
					return;
				}
				Strict.rewind();
				Strict.skip_until(aOperation);
			}

		private:
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
				const github_epoch_fixture_phase aPhase, bool aHistory)
				: Descriptor{ aDescriptor }
				, Profile{ aSelectedProfile }
				, Git{ std::move(aGit) }
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
				, AnonymousRecording{ Descriptor.Paged ? std::vector<recorded_exchange>{ } : load_anonymous_recording(Descriptor, aPhase == github_epoch_fixture_phase::reuse ? Descriptor.HeadTwoOid : Descriptor.HeadOneOid) }
				, CapabilityBody{ load_capability_response(
					  Descriptor.CapabilityDirectory) }
			{
			}

			void advance_history(const adapters::github::detail::wire_request& aRequest)
			{
				if (HistoryHeads.empty() || HistoryIndex + 1U == HistoryHeads.size()
					|| !Recording.consumed_all() || aRequest.Operation != read_operation::get_repository) {
					return;
				}
				// Compile one refresh at a time; retaining 32 duplicated comment
				// scripts would distort the resource measurements of the app.
				auto exchanges = load_recording(Descriptor, HistoryHeads[HistoryIndex + 1U]);
				exchanges.erase(exchanges.begin()); // /user belongs to initial session establishment.
				if (exchanges.front().Request != aRequest) {
					return;
				}
				Recording.replace_completed_paged_script(std::move(exchanges));
				++HistoryIndex;
			}

			[[nodiscard]] auto serves_head(std::string_view aHead) const -> bool
			{
				return aHead == Descriptor.HeadOneOid || (!Descriptor.HeadTwoOid.empty() && aHead == Descriptor.HeadTwoOid)
					|| std::ranges::find(HistoryHeads, aHead) != HistoryHeads.end();
			}

			// Fixed at construction and read from the transport threads without the
			// mutex, which the const keeps true.
			const fixture_descriptor                        Descriptor;
			adapters::github::validated_github_host_profile Profile;
			std::shared_ptr<deterministic_git_fixture>      Git;
			const std::vector<std::string>                  HistoryHeads;
			std::size_t                                     HistoryIndex = 0U;
			fixture_replay                                  BootstrapRecording;
			fixture_replay                                  Recording;
			// The signed-out script. A separate cursor as well as a separate
			// order: the two authorities never share a position, so a course that
			// ran both would not be able to advance one by driving the other.
			fixture_replay                                                                 AnonymousRecording;
			std::array<std::string, adapters::github::detail::capability_probe_part_count> CapabilityBody;
			std::size_t                                                                    CapabilityCalls       = 0U;
			std::size_t                                                                    BootstrapFactoryCalls = 0U;
			std::size_t                                                                    AccountFactoryCalls   = 0U;
			// [[BPR-0536]]. Both factories are once-only, which is what makes
			// "the transport was selected exactly once" a claim the other
			// courses can make. The refresh course establishes a session more
			// than once by construction -- the device grant enrolls after the
			// fixture's own enrollment, and every rotation enrolls again under
			// the same authority -- so under that course, and only there, the
			// recorded transports serve each of them. The counts are still
			// kept, so a course that reads them still reads the truth.
			bool RepeatSessions = false;
			// [[BPR-0657]]. A refresh replays the recorded read script from
			// its start once it is consumed, so the product's re-read is
			// served with the mutation overlay rather than refused.
			bool                                        RepeatReads         = false;
			std::size_t                                 AccountReadCalls    = 0U;
			std::size_t                                 SmartAcquireCalls   = 0U;
			std::size_t                                 SmartDiscoveryCalls = 0U;
			std::size_t                                 SmartLsRefsCalls    = 0U;
			std::size_t                                 SmartFetchCalls     = 0U;
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
			};
			std::map<std::string, thread_overlay> ThreadOverlays;
			mutable std::mutex                    Mutex;
		};

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
				const auto& pack    = (head == fixture.HeadOneOid || (!State->HistoryHeads.empty() && State->serves_head(head)))
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

		/// \brief [[BPR-0657]]. Rewrites a recorded review-thread answer with
		/// the fixture's replies and resolutions.
		[[nodiscard]] auto overlay_review_threads(
			const std::string&                                                    aBody,
			const std::map<std::string, fixture_recording_state::thread_overlay>& aOverlays)
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
			for (auto index = 0; index < nodes.size(); ++index) {
				auto       node    = nodes.at(index).toObject();
				const auto overlay = aOverlays.find(node.value(QStringLiteral("id")).toString().toStdString());
				if (overlay == aOverlays.end()) {
					continue;
				}
				if (overlay->second.Resolved.has_value()) {
					node.insert(QStringLiteral("isResolved"), *overlay->second.Resolved);
					node.insert(QStringLiteral("viewerCanResolve"), !*overlay->second.Resolved);
					node.insert(QStringLiteral("viewerCanUnresolve"), *overlay->second.Resolved);
				}
				auto       comments = node.value(QStringLiteral("comments")).toObject();
				auto       posts    = comments.value(QStringLiteral("nodes")).toArray();
				const auto rootId   = posts.isEmpty()
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
			threads.insert(QStringLiteral("nodes"), nodes);
			pullRequest.insert(QStringLiteral("reviewThreads"), threads);
			repository.insert(QStringLiteral("pullRequest"), pullRequest);
			data.insert(QStringLiteral("repository"), repository);
			root.insert(QStringLiteral("data"), data);
			return QJsonDocument{ root }.toJson(QJsonDocument::Compact).toStdString();
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
				if (Role == fixture_transport_role::anonymous
					&& carries_credential(aRequest.Wire.Headers)) {
					State->RecordingFailure = -3;
					return github_wire_error{
						.Correlation = aRequest.Correlation,
						.Error       = provider_error(
							application::github_provider_failure::authentication),
					};
				}
				if (Role == fixture_transport_role::account) {
					State->advance_history(aRequest.Wire);
				}
				auto& recording = Role == fixture_transport_role::bootstrap
					? State->BootstrapRecording
					: Role == fixture_transport_role::anonymous
					? State->AnonymousRecording
					: State->Recording;
				// [[BPR-0657]]. A refresh asks for the repository first; the
				// course that asked for repeat reads is served the script again
				// from there, replies and resolutions overlaid.
				if (Role == fixture_transport_role::account && State->RepeatReads
					&& aRequest.Wire.Operation == read_operation::get_repository) {
					recording.replay_from(read_operation::get_repository);
				}
				auto response = recording.perform(aRequest.Wire);
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
				if (aRequest.Wire.Operation == read_operation::list_review_threads
					&& !State->ThreadOverlays.empty()) {
					response->Body = overlay_review_threads(response->Body, State->ThreadOverlays);
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
					|| aRequest.Correlation.Part != State->CapabilityCalls % adapters::github::detail::capability_probe_part_count
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
				// Ordinary fixtures serve one or two exact heads. History replay
				// additionally accepts only the explicit stored head inventory.
				const auto acceptedHead = State->serves_head(head);
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
				, AnonymousTransport{ std::make_shared<fixture_recorded_wire_transport>(State, fixture_transport_role::anonymous) }
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
					|| (!State->RepeatSessions
						&& State->BootstrapFactoryCalls != 0U)) {
					return std::unexpected{ provider_error(
						application::github_provider_failure::unavailable) };
				}
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
					|| (!State->RepeatSessions
						&& State->AccountFactoryCalls != 0U)) {
					return std::unexpected{ provider_error(
						application::github_provider_failure::unavailable) };
				}
				// [[BPR-0536]]. The bootstrap rewind's counterpart. The
				// account script opens with the same viewer read the session
				// makes on the account transport, so a second establishment
				// needs the cursor back at that exchange rather than at the
				// pull-request read behind it.
				if (State->RepeatSessions && State->AccountFactoryCalls != 0U) {
					State->Recording.rewind();
				}
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
				std::filesystem::path aFixtureRoot,
				std::filesystem::path aDocumentedRoot,
				const bool            aRefreshCourse)
				: FixtureRoot{ std::move(aFixtureRoot) }
				, DocumentedRoot{ std::move(aDocumentedRoot) }
				, RefreshCourse{ aRefreshCourse }
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

			std::filesystem::path FixtureRoot;
			std::filesystem::path DocumentedRoot;
			bool                  RefreshCourse = false;
			std::size_t           Submissions   = 0U;
			std::size_t           Renewals      = 0U;
			std::string           AccessToken;
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

	struct github_fixture_vault_state
	{
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
				const bool                                  aAuthorityKeyed)
				: Effects{ aEffects }
				, State{ std::move(aState) }
				, RefreshSlot{ aRefreshSlot }
				, AuthorityKeyed{ aAuthorityKeyed }
			{
			}

			void store(
				application::github_credential_enrollment               aEnrollment,
				application::github_credential_store_completion_handler aCompletion)
				override
			{
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
				if (!admit_retrieve(aContext, aCompletion)) {
					return;
				}
				Effects.post(
					[state = State,
					 keyed = AuthorityKeyed,
					 aContext,
					 completion = std::move(aCompletion)]() mutable {
						release_pending(state, aContext);
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
			[[nodiscard]] auto admit_store(
				application::github_credential_enrollment& aEnrollment,
				application::github_credential_store_completion_handler&
					aCompletion) -> bool
			{
				const auto context = aEnrollment.Context;
				if (application::valid(aEnrollment) && aCompletion
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
				if (application::valid(aContext) && aCompletion
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
				if (application::valid(aContext) && aCompletion
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
			explicit fixture_review_mutation_provider(
				const bool                               aLoseFirst,
				std::shared_ptr<fixture_recording_state> aState           = { },
				const bool                               aResolvesThreads = false) noexcept
				: LoseFirst{ aLoseFirst }
				, ResolvesThreads{ aResolvesThreads }
				, State{ std::move(aState) }
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
			bool                                                LoseFirst       = false;
			bool                                                ResolvesThreads = false;
			std::shared_ptr<fixture_recording_state>            State;
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
				served, aProfile, Git, aOptions.EpochPhase, aOptions.LargeReviewHistory);
			Recording->RepeatSessions = aOptions.DeviceRefreshCourse || aOptions.PersistDeviceBindings;
			Recording->RepeatReads    = aOptions.RepeatReadSessions;
			Factory                   = std::make_shared<fixture_transport_factory>(Recording);
			VaultState                = aOptions.SharedDeviceVault ? aOptions.SharedDeviceVault : make_shared_github_fixture_vault();
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
				aOptions.RegisteredDeviceCourse);
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
					aOptions.DeviceRefreshCourse || aOptions.PersistDeviceBindings);
			Mutation = std::make_unique<fixture_review_mutation_provider>(
				aOptions.LoseFirstReviewMutationResult, Recording, aOptions.ResolvesThreads);

			if (aOptions.ApplicationDataRoot) {
				ApplicationDataRoot = aOptions.ApplicationDataRoot->lexically_normal();
				std::error_code error;
				static_cast<void>(std::filesystem::create_directories(
					ApplicationDataRoot, error));
				if (error
					|| !adapters::sqlite::make_storage_root_owner_private(
						ApplicationDataRoot)) {
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
			// Keep the persistent workspace neighbourhood beside the cache but
			// inside this fixture's temporary ownership boundary.
			ApplicationDataRoot /= "data";
			std::error_code createError;
			if (!std::filesystem::create_directory(ApplicationDataRoot, createError) || createError) {
				throw std::runtime_error{ "GitHub public-path application data is unavailable" };
			}
			// A QTemporaryDir carries the entries its parent grants; the stores
			// refuse that, so it is narrowed the way every other root is
			// (BPR-0436).
			if (!adapters::sqlite::make_storage_root_owner_private(ApplicationDataRoot)) {
				throw std::runtime_error{
					"GitHub public-path application data is unavailable"
				};
			}
			// The Git cache accepts a root only in its canonical native
			// spelling; Qt's forward slashes are not that on Windows.
			std::error_code canonicalError;
			ApplicationDataRoot = std::filesystem::canonical(ApplicationDataRoot, canonicalError);
			if (canonicalError) {
				throw std::runtime_error{
					"GitHub public-path application data is unavailable"
				};
			}
		}

		std::shared_ptr<fixture_recording_state>                 Recording;
		std::shared_ptr<deterministic_git_fixture>               Git;
		std::shared_ptr<fixture_transport_factory>               Factory;
		std::shared_ptr<github_fixture_vault_state>              VaultState;
		std::unique_ptr<fixture_credential_vault>                Vault;
		std::unique_ptr<recorded_device_authorization_transport> DeviceTransport;
		std::unique_ptr<fixture_review_mutation_provider>        Mutation;
		std::unique_ptr<QTemporaryDir>                           ApplicationData;
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
			result.CredentialStored    = Implementation->VaultState->Stored;
			result.CredentialRetrieved = Implementation->VaultState->Retrieved;
			result.CredentialDeleted   = Implementation->VaultState->Deleted;
			result.SecretRetained      = Implementation->VaultState->Credential.has_value();
			result.RefreshRetained     = Implementation->VaultState->Refresh.has_value();
			result.RefreshStores       = Implementation->VaultState->RefreshStores;
			result.RefreshRetrieves    = Implementation->VaultState->RefreshRetrieves;
			result.RefreshDeletes      = Implementation->VaultState->RefreshDeletes;
			result.RefreshStoredDigest
				= Implementation->VaultState->LastRefreshDigest;
			result.LastEnrollmentDigest
				= Implementation->VaultState->LastEnrollmentDigest;
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
