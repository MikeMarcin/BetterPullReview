#include "../support/capability_error_content_canaries.hpp"
#include "canonical_json_expectations.hpp"
#include "fixture_json.hpp"
#include "github_capability_probe.hpp"

#include <QCryptographicHash>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace {
	using namespace bpr;
	using namespace bpr::tests;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto fixture_root() -> std::filesystem::path
	{
#if defined(BPR_GITHUB_CAPABILITY_FIXTURE_ROOT)
		return BPR_GITHUB_CAPABILITY_FIXTURE_ROOT;
#elif defined(BPR_GITHUB_CONTRACT_FIXTURE_ROOT)
		return std::filesystem::path{ BPR_GITHUB_CONTRACT_FIXTURE_ROOT }
				   .parent_path()
				   .parent_path()
			/ "capabilities/v1";
#else
		return std::filesystem::path{ __FILE__ }.parent_path().parent_path()
			/ "fixtures/github/capabilities/v1";
#endif
	}

	[[nodiscard]] auto read_file(const std::filesystem::path& aPath)
		-> std::string
	{
		std::ifstream input{ aPath, std::ios::binary };
		expect(input.good(), "capability fixture opens");
		return {
			std::istreambuf_iterator<char>{ input },
			std::istreambuf_iterator<char>{ },
		};
	}

	[[nodiscard]] auto sha256(const std::string_view aValue) -> std::string
	{
		return QCryptographicHash::hash(
				   QByteArray{
					   aValue.data(),
					   static_cast<qsizetype>(aValue.size()),
				   },
				   QCryptographicHash::Sha256)
			.toHex()
			.toStdString();
	}

	[[nodiscard]] auto reverse_arrays(const fixture_value& aValue) -> fixture_value
	{
		if (aValue.is_array()) {
			const auto&   source = aValue.get_array();
			fixture_array reversed;
			for (auto it = source.rbegin(); it != source.rend(); ++it) {
				reversed.push_back(reverse_arrays(*it));
			}
			return reversed;
		}
		if (aValue.is_object()) {
			fixture_object reversed;
			for (const auto& [key, value] : aValue.get_object()) {
				reversed.emplace(key, reverse_arrays(value));
			}
			return reversed;
		}
		return aValue;
	}

	[[nodiscard]] auto load_supported_fixture(const std::filesystem::path& aPath) -> std::string
	{
		return read_file(aPath);
	}

	[[nodiscard]] auto load_manifest_fixture(
		const std::filesystem::path& aDirectory,
		const std::string&           aExpectedProduct,
		const std::string&           aExpectedVersion,
		const std::size_t            aExpectedQueryFields,
		const std::size_t            aExpectedMutationFields,
		const std::size_t            aExpectedRepositoryFields,
		const std::size_t            aExpectedActorPossibleTypes,
		const std::size_t            aExpectedNodePossibleTypes,
		const std::size_t            aExpectedPullRequestFields) -> std::string
	{
		const auto manifest     = parse_fixture_object(read_file(aDirectory / "manifest.json"));
		const auto responseFile = manifest.at("response_file").get_string();
		const auto projection   = manifest.at("projection").get_object();
		const auto expected     = manifest.at("expected").get_object();
		expect(manifest.at("schema_revision").get<std::uint64_t>() == 1
				   && manifest.at("product").get_string() == aExpectedProduct
				   && manifest.at("version").get_string() == aExpectedVersion
				   && responseFile == "supported.json",
			   "capability fixture manifest identifies the exact host contract");
		for (const auto key : {
				 "review_threads",
				 "viewed_file_mutations",
				 "resolve_thread",
				 "unresolve_thread",
			 }) {
			expect(expected.at(key).get_string() == "supported",
				   "fixture manifest records each expected capability");
		}

		const auto body                     = read_file(aDirectory / responseFile);
		const auto response                 = parse_fixture_object(body);
		const auto data                     = response.at("data").get_object();
		const auto actualQueryFields        = data.at("schemaRoot")
												  .get_object()
												  .at("queryType")
												  .get_object()
												  .at("fields")
												  .get_array()
												  .size();
		const auto actualMutationFields     = data.at("schemaRoot")
												  .get_object()
												  .at("mutationType")
												  .get_object()
												  .at("fields")
												  .get_array()
												  .size();
		const auto actualRepositoryFields   = data.at("repository").get_object().at("fields").get_array().size();
		const auto actualActorPossibleTypes = data.at("actorPossibleTypes")
												  .get_object()
												  .at("possibleTypes")
												  .get_array()
												  .size();
		const auto actualNodePossibleTypes  = data.at("nodePossibleTypes")
												  .get_object()
												  .at("possibleTypes")
												  .get_array()
												  .size();
		const auto actualPullRequestFields  = data.at("pullRequest").get_object().at("fields").get_array().size();
		expect(actualQueryFields == aExpectedQueryFields
				   && actualMutationFields == aExpectedMutationFields
				   && actualRepositoryFields == aExpectedRepositoryFields
				   && actualActorPossibleTypes == aExpectedActorPossibleTypes
				   && actualNodePossibleTypes == aExpectedNodePossibleTypes
				   && actualPullRequestFields == aExpectedPullRequestFields
				   && projection.at("query_fields").get<std::uint64_t>()
					   == aExpectedQueryFields
				   && projection.at("mutation_fields").get<std::uint64_t>()
					   == aExpectedMutationFields
				   && projection.at("repository_fields").get<std::uint64_t>()
					   == aExpectedRepositoryFields
				   && projection.at("actor_possible_types").get<std::uint64_t>()
					   == aExpectedActorPossibleTypes
				   && projection.at("node_possible_types").get<std::uint64_t>()
					   == aExpectedNodePossibleTypes
				   && projection.at("pull_request_fields").get<std::uint64_t>()
					   == aExpectedPullRequestFields,
			   "manifest field counts match its independent response");
		expect(projection.at("query_sha256").get_string()
					   == sha256(read_file(fixture_root().parent_path() / "v2-parts/original-v1-query.graphql"))
				   && projection.at("response_sha256").get_string()
					   == sha256(body),
			   "manifest hashes bind the exact query and response bytes");
		expect(static_cast<std::size_t>(actualQueryFields)
					   <= capability_probe_limits{ }.MaximumQueryFields
				   && static_cast<std::size_t>(actualMutationFields)
					   <= capability_probe_limits{ }.MaximumMutationFields
				   && static_cast<std::size_t>(actualRepositoryFields)
					   <= capability_probe_limits{ }.MaximumRepositoryFields
				   && static_cast<std::size_t>(actualActorPossibleTypes)
					   <= capability_probe_limits{ }.MaximumNamedTypes
				   && static_cast<std::size_t>(actualNodePossibleTypes)
					   <= capability_probe_limits{ }.MaximumNamedTypes
				   && static_cast<std::size_t>(
						  actualPullRequestFields)
					   <= capability_probe_limits{ }.MaximumPullRequestFields,
			   "hard defaults admit the official complete schema sizes");
		return body;
	}

	[[nodiscard]] auto set_mutation_fields(fixture_object aDocument, fixture_array aFields) -> fixture_object
	{
		aDocument.at("data").get_object().at("schemaRoot").get_object().at("mutationType").get_object().at("fields") = std::move(aFields);
		return aDocument;
	}

	[[nodiscard]] auto mutation_fields(const fixture_object& aDocument)
		-> fixture_array
	{
		return aDocument
			.at("data")
			.get_object()
			.at("schemaRoot")
			.get_object()
			.at("mutationType")
			.get_object()
			.at("fields")
			.get_array();
	}

	[[nodiscard]] auto synthetic_fixture(
		const std::filesystem::path& aPath) -> std::string
	{
		const auto descriptor = parse_fixture_object(read_file(aPath));
		const auto base       = aPath.parent_path()
			/ descriptor.at("base_response_file").get_string();
		auto       document  = parse_fixture_object(read_file(base));
		auto       fields    = mutation_fields(document);
		const auto target    = descriptor.at("field").get_string();
		const auto operation = descriptor.at("operation").get_string();

		fixture_array changed;
		for (const auto& value : fields) {
			auto field = value.get_object();
			if (
				field.at("name").get_string() == target
				&& operation == "remove_field") {
				continue;
			}
			if (
				field.at("name").get_string() == target
				&& operation == "replace_return_type") {
				field.insert_or_assign("type", descriptor.at("replacement"));
			}
			changed.emplace_back(field);
		}
		return fixture_json(set_mutation_fields(document, std::move(changed)));
	}

	[[nodiscard]] auto github_com_candidate() -> github_host_profile_candidate
	{
		return {
			.Profile        = core::host_profile_id{ 11 },
			.Host           = core::git_host_id{ 21 },
			.Revision       = application::github_profile_revision{ 31 },
			.Server         = application::github_server_contract::github_com_2022_11_28,
			.Authentication = application::github_authentication_transport::github_cli,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::builtin,
			.Hostname       = "github.com",
			.RestRoot       = "https://api.github.com",
			.GraphqlRoot    = "https://api.github.com/graphql",
		};
	}

	[[nodiscard]] auto enterprise_candidate() -> github_host_profile_candidate
	{
		return {
			.Profile        = core::host_profile_id{ 12 },
			.Host           = core::git_host_id{ 22 },
			.Revision       = application::github_profile_revision{ 32 },
			.Server         = application::github_server_contract::ghes_3_18_2022_11_28,
			.Authentication = application::github_authentication_transport::native_https,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::explicit_user,
			.Hostname       = "ghe.example.invalid",
			.RestRoot       = "https://ghe.example.invalid/api/v3",
			.GraphqlRoot    = "https://ghe.example.invalid/api/graphql",
		};
	}

	[[nodiscard]] auto observe(
		std::string                    aBody,
		const bool                     aAuthenticated = true,
		const int                      aStatus        = 200,
		std::string                    aContentType   = "application/json",
		const capability_probe_limits& aLimits        = { },
		const std::initializer_list<capability_probe_header>
			aRateLimitHeaders = { })
		-> capability_probe_observation
	{
		expect(
			aRateLimitHeaders.size()
				<= maximum_capability_probe_rate_limit_headers,
			"test rate-limit facts fit the bounded response");
		auto boundedHeaders = std::array<
			capability_probe_header,
			maximum_capability_probe_rate_limit_headers>{ };
		auto headerIndex = std::size_t{ 0U };
		for (const auto& header : aRateLimitHeaders) {
			boundedHeaders[headerIndex++] = header;
		}

		const capability_probe_transport_result response{
			capability_probe_response{
				.Status               = aStatus,
				.Authenticated        = aAuthenticated,
				.ContentType          = std::move(aContentType),
				.Body                 = std::move(aBody),
				.RateLimitHeaders     = std::move(boundedHeaders),
				.RateLimitHeaderCount = aRateLimitHeaders.size(),
			},
		};
		return normalize_capability_probe(response, aLimits);
	}

	[[nodiscard]] auto fact(
		const capability_probe_observation& aObservation,
		const capability_schema_fact        aValue)
		-> capability_schema_fact_state
	{
		return aObservation.Facts[static_cast<std::size_t>(aValue)];
	}

	auto expect_all_unknown(
		const capability_probe_observation&   aObservation,
		const capability_probe_unavailability aReason,
		const std::string_view                aMessage) -> void
	{
		expect(!aObservation.Complete, aMessage);
		expect(aObservation.Unavailability == aReason, aMessage);
		expect(
			aObservation.Capabilities == core::github_capabilities{ },
			aMessage);
		for (const auto state : aObservation.Facts) {
			expect(
				state == capability_schema_fact_state::unknown,
				aMessage);
		}
		expect(valid(aObservation), "unavailable observation invariant holds");
	}

	auto expect_all_supported(
		const capability_probe_observation& aObservation,
		const std::string_view              aMessage) -> void
	{
		expect(aObservation.Complete, aMessage);
		expect(
			aObservation.Unavailability
				== capability_probe_unavailability::none,
			aMessage);
		expect(
			aObservation.Capabilities
				== core::github_capabilities{
					.ReviewThreads       = core::github_capability_state::supported,
					.ViewedFileMutations = core::github_capability_state::supported,
					.ResolveThread       = core::github_capability_state::supported,
					.UnresolveThread     = core::github_capability_state::supported,
				},
			aMessage);
		for (const auto state : aObservation.Facts) {
			expect(
				state == capability_schema_fact_state::matched,
				aMessage);
		}
		expect(valid(aObservation), "complete observation invariant holds");
	}

	auto test_requests_bind_validated_profile_authority() -> void
	{
		const auto githubProfile     = validate_github_host_profile(github_com_candidate());
		const auto enterpriseProfile = validate_github_host_profile(enterprise_candidate());
		expect(
			githubProfile.has_value() && enterpriseProfile.has_value(),
			"probe profiles validate");

		const auto& copiedGithubProfile = *githubProfile;
		const auto  githubRequest       = compile_capability_probe_request(copiedGithubProfile);
		expect(githubRequest.has_value(), "GitHub.com probe compiles");
		expect(
			githubRequest->Profile == githubProfile->profile()
				&& githubRequest->Host == githubProfile->host()
				&& githubRequest->ProfileRevision
					== githubProfile->revision()
				&& githubRequest->Contract
					== githubProfile->server_contract()
				&& githubRequest->Method
					== capability_probe_http_method::post
				&& valid(githubRequest->Method),
			"probe carries exact validated profile identity");
		expect(
			githubRequest->Authority == "api.github.com"
				&& githubRequest->Root
					== "https://api.github.com/graphql"
				&& githubRequest->Path == "/graphql",
			"GitHub.com probe binds exact validated GraphQL authority");

		const auto enterpriseRequest = compile_capability_probe_request(*enterpriseProfile);
		expect(enterpriseRequest.has_value(), "GHES probe compiles");
		expect(
			enterpriseRequest->Authority == "ghe.example.invalid"
				&& enterpriseRequest->Root
					== "https://ghe.example.invalid/api/graphql"
				&& enterpriseRequest->Path == "/api/graphql"
				&& enterpriseRequest->Contract
					== enterpriseProfile->server_contract()
				&& enterpriseRequest->Body == githubRequest->Body,
			"GHES probe binds exact validated GraphQL authority");
		expect(
			githubRequest->Headers
				== std::array<capability_probe_header, 3>{
					capability_probe_header{
						.Name  = "Accept",
						.Value = "application/json",
					},
					capability_probe_header{
						.Name  = "Content-Type",
						.Value = "application/json",
					},
					capability_probe_header{
						.Name  = "User-Agent",
						.Value = "BetterPullReview/0.1",
					},
				},
			"probe uses the exact non-secret GraphQL headers");

		const auto payload = parse_fixture_object(githubRequest->Body);
		expect(
			payload.size() == 1 && payload.at("query").is_string(),
			"probe body contains only the static query");
		const auto query = payload.at("query").get_string();
		expect(query == capability_probe_graphql_document(),
			   "probe payload contains the canonical query");
		expect(query.starts_with("query BetterPullReviewCapabilityProbeV1"),
			   "probe operation uses the stable query name");
		expect(capability_probe_document_is_query_only(query),
			   "probe operation is a query and never a mutation document");
		expect(query == read_file(fixture_root().parent_path() / "v2-parts/part-0.graphql"), "compiled first part binds the deterministic query fixture");
		expect(
			capability_probe_document_is_query_only(
				R"(query Safe{mutationType{name} probe(value:"mutation"){name}} fragment MutationFields on __Type{name})")
				&& !capability_probe_document_is_query_only(
					"mutation "
					"Unsafe{markFileAsViewed(input:{}){clientMutationId}}")
				&& !capability_probe_document_is_query_only(
					"query Safe{__typename} mutation Unsafe{__typename}")
				&& !capability_probe_document_is_query_only(
					"query Broken{__typename"),
			"operation parser distinguishes mutation names from operations");
		expect(!valid(static_cast<capability_probe_http_method>(255U)),
			   "unknown HTTP methods fail closed");

		auto limits                = capability_probe_limits{ };
		limits.MaximumRequestBytes = 1U;
		const auto bounded         = compile_capability_probe_request(*githubProfile, limits);
		expect(!bounded.has_value()
				   && bounded.error()
					   == capability_probe_unavailability::response_limit,
			   "request byte limit fails closed");

		limits = { };
		limits.MaximumDepth += 1U;
		const auto invalid = compile_capability_probe_request(*githubProfile, limits);
		expect(!invalid.has_value()
				   && invalid.error()
					   == capability_probe_unavailability::invalid_limits,
			   "probe limits cannot relax hard ceilings");
	}

	auto test_paired_supported_contracts() -> void
	{
		const auto root       = fixture_root();
		const auto githubBody = load_manifest_fixture(
			root / "github-com", "github.com",
			"cloud", 31, 271, 145, 5, 280, 113);
		const auto enterpriseBody = load_manifest_fixture(
			root / "ghes-3.18", "github-enterprise-server",
			"3.18", 24, 224, 133, 5, 244,
			109);
		const auto github     = observe(githubBody);
		const auto enterprise = observe(enterpriseBody);
		expect_all_supported(github, "GitHub.com schema supports all facts");
		expect_all_supported(
			enterprise,
			"GHES 3.18 schema supports all facts");
		expect(
			github == enterprise,
			"paired host contracts normalize to identical evidence");

		const auto reordered = observe(fixture_json(reverse_arrays(parse_fixture_object(githubBody)).get_object()));
		expect_all_supported(
			reordered,
			"schema member names are independent of response order");
		expect(
			reordered == github,
			"schema reordering preserves exact normalized evidence");
	}

	auto test_synthetic_missing_and_incompatible_contracts() -> void
	{
		const auto root    = fixture_root();
		const auto missing = observe(synthetic_fixture(
			root / "synthetic/missing-unresolve.json"));
		expect(missing.Complete, "missing field is complete schema evidence");
		expect(
			missing.Capabilities.ReviewThreads
					== core::github_capability_state::supported
				&& missing.Capabilities.ViewedFileMutations
					== core::github_capability_state::supported
				&& missing.Capabilities.ResolveThread
					== core::github_capability_state::supported
				&& missing.Capabilities.UnresolveThread
					== core::github_capability_state::unsupported,
			"missing unresolve affects only its dependent capability");
		expect(
			fact(
				missing,
				capability_schema_fact::unresolve_review_thread)
				== capability_schema_fact_state::missing,
			"missing mutation is retained as exact evidence");
		expect(valid(missing), "mixed complete observation is valid");

		const auto incompatible = observe(synthetic_fixture(
			root / "synthetic/incompatible-mark.json"));
		expect(incompatible.Complete, "wrong signature is complete evidence");
		expect(
			incompatible.Capabilities.ReviewThreads
					== core::github_capability_state::supported
				&& incompatible.Capabilities.ViewedFileMutations
					== core::github_capability_state::unsupported
				&& incompatible.Capabilities.ResolveThread
					== core::github_capability_state::supported
				&& incompatible.Capabilities.UnresolveThread
					== core::github_capability_state::supported,
			"incompatible mark signature disables combined viewed mutations");
		expect(
			fact(
				incompatible,
				capability_schema_fact::mark_file_as_viewed)
				== capability_schema_fact_state::incompatible,
			"incompatible signature is retained as exact evidence");
		expect(valid(incompatible), "incompatible complete evidence is valid");
	}

	/// The recorded answer with one field taken out of one introspected type.
	[[nodiscard]] auto with_type_field_removed(
		const std::string& aBody,
		const std::string& aAlias,
		const std::string& aField) -> std::string
	{
		auto envelope = parse_fixture_object(aBody);
		auto data     = envelope.at("data").get_object();
		auto type     = data.at(aAlias).get_object();
		auto kept     = fixture_array{ };
		for (const auto& value : type.at("fields").get_array()) {
			if (value.get_object().at("name").get_string() != aField) {
				kept.emplace_back(value);
			}
		}
		type.insert_or_assign("fields", std::move(kept));
		data.insert_or_assign(aAlias, type);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	// [[BPR-PENDING-AX]]. GitHub.com and GHES 3.18 both record
	// `PullRequestReviewThread.viewerCanReply: Boolean!` and
	// `PullRequest.locked: Boolean!`, so both facts match and the read shape
	// asks for them. A schema without either reads without it: the fact is
	// missing, and no capability changes, so nothing is refused on it.
	auto test_reply_permission_and_lock_facts() -> void
	{
		const auto root   = fixture_root();
		const auto bodies = std::array{
			load_manifest_fixture(root / "github-com", "github.com", "cloud", 31, 271, 145, 5, 280, 113),
			load_manifest_fixture(root / "ghes-3.18", "github-enterprise-server", "3.18", 24, 224, 133, 5, 244, 109),
		};
		for (const auto& body : bodies) {
			const auto recorded = observe(body);
			expect(fact(recorded, capability_schema_fact::reply_permission) == capability_schema_fact_state::matched
					   && fact(recorded, capability_schema_fact::conversation_lock) == capability_schema_fact_state::matched,
				   "the recorded schema carries viewerCanReply and the lock");
			const auto without = observe(with_type_field_removed(with_type_field_removed(body, "reviewThread", "viewerCanReply"), "pullRequest", "locked"));
			expect(without.Complete && valid(without) && without.Capabilities == recorded.Capabilities
					   && fact(without, capability_schema_fact::reply_permission) == capability_schema_fact_state::missing
					   && fact(without, capability_schema_fact::conversation_lock) == capability_schema_fact_state::missing,
				   "a schema without viewerCanReply and the lock is complete evidence that lacks only them");
			const auto replyOnly = observe(with_type_field_removed(body, "reviewThread", "viewerCanReply"));
			expect(fact(replyOnly, capability_schema_fact::reply_permission) == capability_schema_fact_state::missing
					   && fact(replyOnly, capability_schema_fact::conversation_lock) == capability_schema_fact_state::matched,
				   "each fact stands on its own field");
		}
	}

	[[nodiscard]] auto with_data_alias_removed(
		const std::string& aBody,
		const std::string& aAlias) -> std::string
	{
		auto envelope = parse_fixture_object(aBody);
		auto data     = envelope.at("data").get_object();
		data.erase(aAlias);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	[[nodiscard]] auto with_data_alias_null(
		const std::string& aBody,
		const std::string& aAlias) -> std::string
	{
		auto envelope = parse_fixture_object(aBody);
		auto data     = envelope.at("data").get_object();
		data.insert_or_assign(aAlias, nullptr);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	[[nodiscard]] auto with_alias_member(
		const std::string&   aBody,
		const std::string&   aAlias,
		const std::string&   aMember,
		const fixture_value& aReplacement) -> std::string
	{
		auto envelope = parse_fixture_object(aBody);
		auto data     = envelope.at("data").get_object();
		auto type     = data.at(aAlias).get_object();
		type.insert_or_assign(aMember, aReplacement);
		data.insert_or_assign(aAlias, type);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	[[nodiscard]] auto without_named_field(
		const std::string& aBody,
		const std::string& aAlias,
		const std::string& aFieldName) -> std::string
	{
		auto       envelope = parse_fixture_object(aBody);
		auto       data     = envelope.at("data").get_object();
		auto       type     = data.at(aAlias).get_object();
		const auto fields   = type.at("fields").get_array();
		auto       retained = fixture_array{ };
		for (const auto& value : fields) {
			if (
				value.get_object().at("name").get_string()
				!= aFieldName) {
				retained.push_back(value);
			}
		}
		expect(
			retained.size() + 1 == fields.size(),
			"targeted schema field exists exactly once");
		type.insert_or_assign("fields", retained);
		data.insert_or_assign(aAlias, type);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	[[nodiscard]] auto with_named_field_type(const std::string& aBody,
											 const std::string& aAlias,
											 const std::string& aFieldName,
											 fixture_object     aReplacement) -> std::string
	{
		auto envelope     = parse_fixture_object(aBody);
		auto data         = envelope.at("data").get_object();
		auto type         = data.at(aAlias).get_object();
		auto fields       = type.at("fields").get_array();
		auto replacements = 0;
		for (std::size_t index = 0; index < fields.size(); ++index) {
			auto field = fields.at(index).get_object();
			if (field.at("name").get_string() == aFieldName) {
				field.insert_or_assign("type", aReplacement);
				fields.at(index) = field;
				++replacements;
			}
		}
		expect(replacements == 1, "targeted schema field exists exactly once");
		type.insert_or_assign("fields", fields);
		data.insert_or_assign(aAlias, type);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	[[nodiscard]] auto without_schema_root_field(const std::string& aBody,
												 const std::string& aRootName,
												 const std::string& aFieldName)
		-> std::string
	{
		auto       envelope = parse_fixture_object(aBody);
		auto       data     = envelope.at("data").get_object();
		auto       schema   = data.at("schemaRoot").get_object();
		auto       root     = schema.at(aRootName).get_object();
		const auto fields   = root.at("fields").get_array();
		auto       retained = fixture_array{ };
		for (const auto& value : fields) {
			if (value.get_object().at("name").get_string() != aFieldName) {
				retained.push_back(value);
			}
		}
		expect(retained.size() + 1 == fields.size(),
			   "targeted schema-root field exists exactly once");
		root.insert_or_assign("fields", retained);
		schema.insert_or_assign(aRootName, root);
		data.insert_or_assign("schemaRoot", schema);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	[[nodiscard]] auto with_schema_root_field_type(const std::string&    aBody,
												   const std::string&    aRootName,
												   const std::string&    aFieldName,
												   const fixture_object& aReplacement)
		-> std::string
	{
		auto envelope     = parse_fixture_object(aBody);
		auto data         = envelope.at("data").get_object();
		auto schema       = data.at("schemaRoot").get_object();
		auto root         = schema.at(aRootName).get_object();
		auto fields       = root.at("fields").get_array();
		auto replacements = 0;
		for (std::size_t index = 0; index < fields.size(); ++index) {
			auto field = fields.at(index).get_object();
			if (field.at("name").get_string() == aFieldName) {
				field.insert_or_assign("type", aReplacement);
				fields.at(index) = field;
				++replacements;
			}
		}
		expect(replacements == 1, "targeted schema-root field exists exactly once");
		root.insert_or_assign("fields", fields);
		schema.insert_or_assign(aRootName, root);
		data.insert_or_assign("schemaRoot", schema);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	[[nodiscard]] auto
	without_named_type_member(const std::string& aBody, const std::string& aAlias,
							  const std::string& aMemberName, const std::string& aTypeName)
		-> std::string
	{
		auto       envelope = parse_fixture_object(aBody);
		auto       data     = envelope.at("data").get_object();
		auto       type     = data.at(aAlias).get_object();
		const auto members  = type.at(aMemberName).get_array();
		auto       retained = fixture_array{ };
		for (const auto& value : members) {
			if (value.get_object().at("name").get_string() != aTypeName) {
				retained.push_back(value);
			}
		}
		expect(retained.size() + 1 == members.size(),
			   "targeted named type exists exactly once");
		type.insert_or_assign(aMemberName, retained);
		data.insert_or_assign(aAlias, type);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	[[nodiscard]] auto with_graphql_errors(const std::string& aBody) -> std::string
	{
		auto envelope = parse_fixture_object(aBody);
		envelope.insert_or_assign("errors",
								  fixture_array{
									  fixture_object{ { "message", "sanitized probe error" } },
								  });
		return fixture_json(envelope);
	}

	[[nodiscard]] auto with_rate_limited_graphql_errors(
		const std::string& aBody) -> std::string
	{
		auto envelope = parse_fixture_object(aBody);
		envelope.insert_or_assign(
			"errors",
			fixture_array{
				fixture_object{
					{ "message", "sanitized throttling error" },
					{ "type", "RATE_LIMITED" },
				},
			});
		return fixture_json(envelope);
	}

	auto test_partial_and_unavailable_evidence_stays_unknown() -> void
	{
		const auto supported = load_supported_fixture(
			fixture_root() / "github-com/supported.json");

		expect_all_unknown(
			observe(with_graphql_errors(supported)),
			capability_probe_unavailability::partial_response,
			"GraphQL errors reject all partial evidence");
		expect_all_unknown(
			observe(with_data_alias_removed(supported, "resolvePayload")),
			capability_probe_unavailability::partial_response,
			"missing alias rejects all partial evidence");
		expect_all_unknown(
			observe(supported, false),
			capability_probe_unavailability::authentication,
			"unauthenticated schema cannot prove unsupported");
		expect_all_unknown(
			observe(supported, true, 401),
			capability_probe_unavailability::authentication,
			"HTTP authentication failure remains unknown");
		expect_all_unknown(
			observe(supported, true, 403),
			capability_probe_unavailability::permission,
			"HTTP permission failure remains unknown");
		expect_all_unknown(
			observe(supported, true, 404),
			capability_probe_unavailability::api_version,
			"missing endpoint remains unknown");
		expect_all_unknown(
			observe(supported, true, 429),
			capability_probe_unavailability::rate_limit,
			"rate limit remains unknown");
		expect_all_unknown(
			observe(supported, true, 500),
			capability_probe_unavailability::invalid_status,
			"other HTTP failure remains unknown");
		expect_all_unknown(
			observe(supported, true, 200, "text/html"),
			capability_probe_unavailability::invalid_content_type,
			"non-JSON response remains unknown");

		for (const auto failure : {
				 capability_probe_unavailability::network,
				 capability_probe_unavailability::certificate,
				 capability_probe_unavailability::redirect,
				 capability_probe_unavailability::timeout,
				 capability_probe_unavailability::cancelled,
			 }) {
			const capability_probe_transport_result transport{
				std::unexpected{ failure }
			};
			expect_all_unknown(
				normalize_capability_probe(transport),
				failure,
				"transport failure cannot become unsupported");
		}

		const capability_probe_transport_result malformedReason{
			std::unexpected{
				static_cast<capability_probe_unavailability>(255U) }
		};
		expect_all_unknown(
			normalize_capability_probe(malformedReason),
			capability_probe_unavailability::malformed_response,
			"malformed transport reason fails closed to a valid category");
	}

	auto test_rate_limit_classification_is_bounded() -> void
	{
		const auto supported = load_supported_fixture(
			fixture_root() / "github-com/supported.json");
		const auto errors = with_graphql_errors(supported);

		expect_all_unknown(
			observe(
				errors,
				true,
				200,
				"application/json",
				{ },
				{
					{ "x-ratelimit-limit", "5000" },
					{ "x-ratelimit-remaining", "0" },
					{ "x-ratelimit-reset", "2000000000" },
					{ "x-ratelimit-resource", "graphql" },
				}),
			capability_probe_unavailability::rate_limit,
			"GitHub GraphQL 200 exhaustion is classified as rate limit");
		expect_all_unknown(observe(supported, true, 403, "application/json", { },
								   { { "Retry-After", "60" } }),
						   capability_probe_unavailability::rate_limit,
						   "GitHub 403 secondary limit uses bounded Retry-After");
		expect_all_unknown(
			observe(supported, true, 403, "application/json", { },
					{
						{ "X-RateLimit-Limit", "5000" },
						{ "X-RateLimit-Remaining", " \t0\t " },
						{ "X-RateLimit-Reset", "2000000000" },
						{ "X-RateLimit-Resource", "graphql" },
					}),
			capability_probe_unavailability::rate_limit,
			"rate-limit numeric headers admit only outer HTTP whitespace");
		expect_all_unknown(observe(with_rate_limited_graphql_errors(supported)),
						   capability_probe_unavailability::rate_limit,
						   "structured GraphQL RATE_LIMITED error is classified");
		expect_all_unknown(
			observe(with_rate_limited_graphql_errors(supported), true, 403),
			capability_probe_unavailability::rate_limit,
			"GraphQL 403 structured RATE_LIMITED error is classified");
		expect_all_unknown(
			observe(errors, true, 200, "application/json", { },
					{
						{ "X-RateLimit-Limit", "5000" },
						{ "X-RateLimit-Remaining", "1" },
						{ "X-RateLimit-Reset", "2000000000" },
						{ "X-RateLimit-Resource", "graphql" },
					}),
			capability_probe_unavailability::partial_response,
			"non-exhausted GraphQL errors remain generic partial evidence");
		expect_all_supported(observe(supported, true, 200, "application/json", { },
									 {
										 { "X-RateLimit-Limit", "5000" },
										 { "X-RateLimit-Remaining", "0" },
										 { "X-RateLimit-Reset", "2000000000" },
										 { "X-RateLimit-Resource", "graphql" },
									 }),
							 "a complete successful observation remains usable");

		const auto expectMalformedHeaders =
			[&supported](
				const std::initializer_list<capability_probe_header> aHeaders,
				const std::string_view                               aMessage) {
				expect_all_unknown(
					observe(supported, true, 403, "application/json", { }, aHeaders),
					capability_probe_unavailability::malformed_response, aMessage);
			};
		expectMalformedHeaders({ { "X-Unbounded-Provider-Header", "0" } },
							   "non-allowlisted response headers are rejected");
		expectMalformedHeaders(
			{
				{ "X-RateLimit-Remaining", "0" },
				{ "x-ratelimit-remaining", "0" },
			},
			"case-insensitive duplicate rate-limit headers are rejected");
		expectMalformedHeaders({ { "X-RateLimit-Remaining", "-1" } },
							   "signed rate-limit values are rejected");
		expectMalformedHeaders({ { "X-RateLimit-Remaining", "0\r\nInjected: 1" } },
							   "header-value injection is rejected");
		expectMalformedHeaders(
			{ { "X-RateLimit-Remaining", "18446744073709551616" } },
			"overflowing rate-limit values are rejected");
		expectMalformedHeaders(
			{
				{ "X-RateLimit-Limit", "5" },
				{ "X-RateLimit-Remaining", "6" },
				{ "X-RateLimit-Reset", "2000000000" },
				{ "X-RateLimit-Resource", "graphql" },
			},
			"inconsistent rate-limit facts are rejected");
		expectMalformedHeaders(
			{
				{ "X-RateLimit-Limit", "5" },
				{ "X-RateLimit-Remaining", "4" },
				{ "X-RateLimit-Reset", "2000000000" },
				{ "X-RateLimit-Resource", "core" },
			},
			"capability response cannot inject a non-GraphQL rate resource");
		expectMalformedHeaders(
			{ { "Retry-After", std::string(33U, '9') } },
			"oversized rate-limit values are rejected");

		auto malformedResponse = capability_probe_response{
			.Status               = 403,
			.Authenticated        = true,
			.ContentType          = "application/json",
			.Body                 = supported,
			.RateLimitHeaders     = { },
			.RateLimitHeaderCount = maximum_capability_probe_rate_limit_headers + 1U,
		};
		expect_all_unknown(
			normalize_capability_probe(
				capability_probe_transport_result{ malformedResponse }),
			capability_probe_unavailability::malformed_response,
			"rate-limit header count cannot exceed fixed storage");

		malformedResponse.RateLimitHeaderCount     = 0U;
		malformedResponse.RateLimitHeaders.front() = {
			.Name  = "X-RateLimit-Remaining",
			.Value = "0",
		};
		expect_all_unknown(
			normalize_capability_probe(
				capability_probe_transport_result{ malformedResponse }),
			capability_probe_unavailability::malformed_response,
			"unaccounted trailing response facts are rejected");
	}

	auto test_complete_absence_is_unsupported() -> void
	{
		const auto supported = load_supported_fixture(
			fixture_root() / "github-com/supported.json");
		const auto missingObject = observe(with_data_alias_null(supported, "reviewThread"));
		expect(
			missingObject.Complete
				&& missingObject.Capabilities.ReviewThreads
					== core::github_capability_state::unsupported
				&& fact(
					   missingObject,
					   capability_schema_fact::review_threads_read)
					== capability_schema_fact_state::missing
				&& valid(missingObject),
			"null object alias is definitive missing schema evidence");

		const auto missingInput = observe(with_data_alias_null(supported, "markInput"));
		expect(
			missingInput.Complete
				&& missingInput.Capabilities.ViewedFileMutations
					== core::github_capability_state::unsupported
				&& fact(
					   missingInput,
					   capability_schema_fact::mark_file_as_viewed)
					== capability_schema_fact_state::missing
				&& valid(missingInput),
			"null input alias is definitive missing schema evidence");

		const auto missingEnum = observe(with_data_alias_null(supported, "fileViewedState"));
		expect(missingEnum.Complete, "null __type is complete absence");
		expect(
			missingEnum.Capabilities.ReviewThreads
					== core::github_capability_state::supported
				&& missingEnum.Capabilities.ViewedFileMutations
					== core::github_capability_state::unsupported
				&& missingEnum.Capabilities.ResolveThread
					== core::github_capability_state::supported
				&& missingEnum.Capabilities.UnresolveThread
					== core::github_capability_state::supported,
			"definitively absent viewed enum is unsupported");
		expect(
			fact(
				missingEnum,
				capability_schema_fact::viewed_file_state_read)
				== capability_schema_fact_state::missing,
			"absent type remains missing evidence");
		expect(valid(missingEnum), "complete missing-type evidence is valid");
	}

	auto test_null_selected_members_are_incomplete() -> void
	{
		const auto supported = load_supported_fixture(
			fixture_root() / "github-com/supported.json");
		expect_all_unknown(
			observe(with_alias_member(
				supported,
				"reviewThread",
				"fields",
				nullptr)),
			capability_probe_unavailability::malformed_response,
			"null fields on an object cannot prove schema absence");
		expect_all_unknown(
			observe(with_alias_member(
				supported,
				"markInput",
				"inputFields",
				nullptr)),
			capability_probe_unavailability::malformed_response,
			"null inputFields on an input object cannot prove absence");
		expect_all_unknown(
			observe(with_alias_member(
				supported,
				"fileViewedState",
				"enumValues",
				nullptr)),
			capability_probe_unavailability::malformed_response,
			"null enumValues on an enum cannot prove absence");
	}

	auto test_capability_dependencies_remain_independent() -> void
	{
		const auto supported = load_supported_fixture(
			fixture_root() / "github-com/supported.json");
		const auto missingDiffSide = observe(without_named_field(
			supported,
			"reviewThread",
			"diffSide"));
		expect(
			missingDiffSide.Complete
				&& missingDiffSide.Capabilities.ReviewThreads
					== core::github_capability_state::unsupported
				&& fact(
					   missingDiffSide,
					   capability_schema_fact::review_threads_read)
					== capability_schema_fact_state::missing,
			"missing diffSide makes review-thread reads unsupported");

		const auto wrongDiffSide = observe(with_named_field_type(
			supported,
			"reviewThread",
			"diffSide",
			fixture_object{
				{ "kind", "NON_NULL" },
				{ "name", nullptr },
				{ "ofType", fixture_object{ { "kind", "SCALAR" }, { "name", "String" } } },
			}));
		expect(
			wrongDiffSide.Complete
				&& wrongDiffSide.Capabilities.ReviewThreads
					== core::github_capability_state::unsupported
				&& fact(
					   wrongDiffSide,
					   capability_schema_fact::review_threads_read)
					== capability_schema_fact_state::incompatible,
			"wrong diffSide type makes review-thread reads incompatible");
		const auto missingResolvePermission = observe(without_named_field(
			supported,
			"reviewThread",
			"viewerCanResolve"));
		expect(
			missingResolvePermission.Complete
				&& missingResolvePermission.Capabilities.ReviewThreads
					== core::github_capability_state::supported
				&& missingResolvePermission.Capabilities.ResolveThread
					== core::github_capability_state::unsupported
				&& missingResolvePermission.Capabilities.UnresolveThread
					== core::github_capability_state::supported
				&& fact(
					   missingResolvePermission,
					   capability_schema_fact::resolve_permission)
					== capability_schema_fact_state::missing
				&& valid(missingResolvePermission),
			"resolve permission is independent from thread readability");

		const auto missingUnresolvePermission = observe(without_named_field(
			supported,
			"reviewThread",
			"viewerCanUnresolve"));
		expect(
			missingUnresolvePermission.Complete
				&& missingUnresolvePermission.Capabilities.ReviewThreads
					== core::github_capability_state::supported
				&& missingUnresolvePermission.Capabilities.ResolveThread
					== core::github_capability_state::supported
				&& missingUnresolvePermission.Capabilities.UnresolveThread
					== core::github_capability_state::unsupported
				&& fact(
					   missingUnresolvePermission,
					   capability_schema_fact::unresolve_permission)
					== capability_schema_fact_state::missing
				&& valid(missingUnresolvePermission),
			"unresolve permission is independent from thread readability");

		const auto missingViewedState = observe(without_named_field(
			supported,
			"changedFile",
			"viewerViewedState"));
		expect(
			missingViewedState.Complete
				&& missingViewedState.Capabilities.ReviewThreads
					== core::github_capability_state::supported
				&& missingViewedState.Capabilities.ViewedFileMutations
					== core::github_capability_state::unsupported
				&& missingViewedState.Capabilities.ResolveThread
					== core::github_capability_state::supported
				&& missingViewedState.Capabilities.UnresolveThread
					== core::github_capability_state::supported
				&& fact(
					   missingViewedState,
					   capability_schema_fact::viewed_file_state_read)
					== capability_schema_fact_state::missing
				&& valid(missingViewedState),
			"viewed-state reads are independent from review threads");
	}

	auto test_compiled_connection_counts_are_required() -> void
	{
		const auto supported = load_supported_fixture(fixture_root() / "github-com/supported.json");
		const auto wrongType = fixture_object{
			{ "kind", "NON_NULL" },
			{ "name", nullptr },
			{
				"ofType",
				fixture_object{
					{ "kind", "SCALAR" },
					{ "name", "String" },
				},
			},
		};

		for (const auto& alias : {
				 "threadConnection",
				 "commentConnection",
			 }) {
			const auto missing = observe(without_named_field(
				supported, alias, "totalCount"));
			expect(missing.Complete
					   && missing.Capabilities.ReviewThreads
						   == core::github_capability_state::unsupported
					   && missing.Capabilities.ViewedFileMutations
						   == core::github_capability_state::supported
					   && missing.Capabilities.ResolveThread
						   == core::github_capability_state::unsupported
					   && missing.Capabilities.UnresolveThread
						   == core::github_capability_state::unsupported
					   && fact(missing, capability_schema_fact::review_threads_read)
						   == capability_schema_fact_state::missing
					   && valid(missing),
				   "missing review connection totalCount disables its read shape");

			const auto incompatible = observe(with_named_field_type(
				supported, alias, "totalCount", wrongType));
			expect(
				incompatible.Complete
					&& fact(incompatible,
							capability_schema_fact::review_threads_read)
						== capability_schema_fact_state::incompatible
					&& incompatible.Capabilities.ReviewThreads
						== core::github_capability_state::unsupported
					&& valid(incompatible),
				"incompatible review connection totalCount disables its read shape");
		}

		const auto missingFiles = observe(
			without_named_field(supported, "changedFileConnection",
								"totalCount"));
		expect(missingFiles.Complete
				   && missingFiles.Capabilities.ReviewThreads
					   == core::github_capability_state::supported
				   && missingFiles.Capabilities.ViewedFileMutations
					   == core::github_capability_state::unsupported
				   && fact(missingFiles,
						   capability_schema_fact::viewed_file_state_read)
					   == capability_schema_fact_state::missing
				   && valid(missingFiles),
			   "missing file connection totalCount disables viewed-state reads");

		const auto incompatibleFiles = observe(with_named_field_type(
			supported, "changedFileConnection",
			"totalCount", wrongType));
		expect(
			incompatibleFiles.Complete
				&& incompatibleFiles.Capabilities.ReviewThreads
					== core::github_capability_state::supported
				&& incompatibleFiles.Capabilities.ViewedFileMutations
					== core::github_capability_state::unsupported
				&& fact(incompatibleFiles,
						capability_schema_fact::viewed_file_state_read)
					== capability_schema_fact_state::incompatible
				&& valid(incompatibleFiles),
			"incompatible file connection totalCount disables viewed-state reads");
	}

	auto test_compiled_graphql_baseline_is_required() -> void
	{
		const auto supported = load_supported_fixture(fixture_root() / "github-com/supported.json");
		const auto wrongType = fixture_object{
			{ "kind", "NON_NULL" },
			{ "name", nullptr },
			{
				"ofType",
				fixture_object{
					{ "kind", "SCALAR" },
					{ "name", "String" },
				},
			},
		};
		const auto expectFacts =
			[](const capability_probe_observation& aObservation,
			   const capability_schema_fact_state  aReview,
			   const capability_schema_fact_state  aViewed,
			   const std::string_view              aMessage) {
				expect(aObservation.Complete
						   && fact(aObservation,
								   capability_schema_fact::review_threads_read)
							   == aReview
						   && fact(aObservation,
								   capability_schema_fact::viewed_file_state_read)
							   == aViewed
						   && valid(aObservation),
					   aMessage);
			};

		expectFacts(observe(without_schema_root_field(
						supported, "queryType",
						"repository")),
					capability_schema_fact_state::missing,
					capability_schema_fact_state::missing,
					"missing Query.repository disables both compiled read shapes");
		expectFacts(
			observe(with_schema_root_field_type(
				supported, "queryType",
				"repository", wrongType)),
			capability_schema_fact_state::incompatible,
			capability_schema_fact_state::incompatible,
			"incompatible Query.repository disables both compiled read shapes");

		for (const auto& fieldName : {
				 "id",
				 "pullRequest",
			 }) {
			expectFacts(
				observe(without_named_field(supported, "repository",
											fieldName)),
				capability_schema_fact_state::missing,
				capability_schema_fact_state::missing,
				"missing Repository baseline field disables both read shapes");
			expectFacts(
				observe(with_named_field_type(supported,
											  "repository",
											  fieldName, wrongType)),
				capability_schema_fact_state::incompatible,
				capability_schema_fact_state::incompatible,
				"incompatible Repository baseline field disables both read shapes");
		}

		expectFacts(
			observe(without_named_field(supported, "pullRequest",
										"id")),
			capability_schema_fact_state::missing,
			capability_schema_fact_state::missing,
			"missing PullRequest.id disables both compiled read shapes");
		expectFacts(
			observe(with_named_field_type(supported, "pullRequest",
										  "id", wrongType)),
			capability_schema_fact_state::incompatible,
			capability_schema_fact_state::incompatible,
			"incompatible PullRequest.id disables both compiled read shapes");

		expectFacts(
			observe(without_named_field(supported, "pullRequest",
										"headRefOid")),
			capability_schema_fact_state::missing,
			capability_schema_fact_state::missing,
			"missing PullRequest.headRefOid disables revision-bound read shapes");
		expectFacts(
			observe(with_named_field_type(supported, "pullRequest",
										  "headRefOid", wrongType)),
			capability_schema_fact_state::incompatible,
			capability_schema_fact_state::incompatible,
			"incompatible PullRequest.headRefOid disables revision-bound read shapes");

		expectFacts(observe(without_schema_root_field(supported,
													  "queryType",
													  "node")),
					capability_schema_fact_state::missing,
					capability_schema_fact_state::matched,
					"missing Query.node disables only comment paging");
		expectFacts(observe(with_schema_root_field_type(
						supported, "queryType",
						"node", wrongType)),
					capability_schema_fact_state::incompatible,
					capability_schema_fact_state::matched,
					"incompatible Query.node disables only comment paging");

		expectFacts(
			observe(with_data_alias_null(supported,
										 "reviewThreadInterfaces")),
			capability_schema_fact_state::missing,
			capability_schema_fact_state::matched,
			"missing review-thread interface evidence disables comment paging");
		expectFacts(
			observe(without_named_type_member(
				supported, "reviewThreadInterfaces",
				"interfaces", "Node")),
			capability_schema_fact_state::incompatible,
			capability_schema_fact_state::matched,
			"review thread without Node compatibility disables comment paging");

		expectFacts(
			observe(with_data_alias_null(supported,
										 "actorPossibleTypes")),
			capability_schema_fact_state::missing,
			capability_schema_fact_state::matched,
			"missing Actor possible-type evidence disables author identity reads");
		const auto actorType = parse_fixture_object(supported)

								   .at("data")
								   .get_object()
								   .at("actorPossibleTypes")
								   .get_object()
								   .at("possibleTypes")
								   .get_array()
								   .at(0)
								   .get_object()
								   .at("name")
								   .get_string();
		expect(!actorType.empty(), "fixture has an Actor possible type");
		expectFacts(
			observe(without_named_type_member(
				supported, "nodePossibleTypes",
				"possibleTypes", actorType)),
			capability_schema_fact_state::incompatible,
			capability_schema_fact_state::matched,
			"one non-Node Actor disables total author identity normalization");
	}

	[[nodiscard]] auto with_duplicate_review_field(
		const std::string& aBody) -> std::string
	{
		auto envelope = parse_fixture_object(aBody);
		auto data     = envelope.at("data").get_object();
		auto thread   = data.at("reviewThread").get_object();
		auto fields   = thread.at("fields").get_array();
		expect(!fields.empty(), "review thread fixture has fields");
		fields.push_back(fields.at(0));
		thread.insert_or_assign("fields", fields);
		data.insert_or_assign("reviewThread", thread);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	[[nodiscard]] auto with_duplicate_alias_member(
		const std::string& aBody,
		const std::string& aAlias,
		const std::string& aMemberName) -> std::string
	{
		auto envelope = parse_fixture_object(aBody);
		auto data     = envelope.at("data").get_object();
		auto type     = data.at(aAlias).get_object();
		auto members  = type.at(aMemberName).get_array();
		expect(!members.empty(), "schema fixture member list is nonempty");
		members.push_back(members.at(0));
		type.insert_or_assign(aMemberName, members);
		data.insert_or_assign(aAlias, type);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	[[nodiscard]] auto with_duplicate_mark_argument(
		const std::string& aBody) -> std::string
	{
		auto document = parse_fixture_object(aBody);
		auto fields   = mutation_fields(document);
		auto changed  = fixture_array{ };
		for (const auto& value : fields) {
			auto field = value.get_object();
			if (
				field.at("name").get_string()
				== "markFileAsViewed") {
				auto arguments = field.at("args").get_array();
				expect(!arguments.empty(), "mark mutation fixture has arguments");
				arguments.push_back(arguments.at(0));
				field.insert_or_assign("args", arguments);
			}
			changed.emplace_back(field);
		}
		return fixture_json(set_mutation_fields(document, std::move(changed)));
	}

	[[nodiscard]] auto with_extra_mark_argument(
		const std::string& aBody,
		const bool         aRequired) -> std::string
	{
		auto          document = parse_fixture_object(aBody);
		auto          fields   = mutation_fields(document);
		fixture_array changed;
		for (const auto& value : fields) {
			auto field = value.get_object();
			if (
				field.at("name").get_string()
				== "markFileAsViewed") {
				auto       arguments      = field.at("args").get_array();
				const auto futureArgument = fixture_value(fixture_object{
					{ "name", "futureArgument" },
					{ "defaultValue", nullptr },
					{
						"type",
						aRequired
							? fixture_value(fixture_object{
								  { "kind", "NON_NULL" },
								  { "name", nullptr },
								  {
									  "ofType",
									  fixture_object{
										  { "kind", "SCALAR" },
										  { "name", "String" },
									  },
								  },
							  })
							: fixture_value(fixture_object{
								  { "kind", "SCALAR" },
								  { "name", "String" },
								  { "ofType", nullptr },
							  }),
					},
				});
				arguments.push_back(futureArgument);
				field.insert_or_assign("args", arguments);
			}
			changed.emplace_back(field);
		}
		return fixture_json(set_mutation_fields(document, std::move(changed)));
	}

	[[nodiscard]] auto with_extra_mark_input_field(
		const std::string& aBody,
		const bool         aRequired) -> std::string
	{
		auto       envelope    = parse_fixture_object(aBody);
		auto       data        = envelope.at("data").get_object();
		auto       input       = data.at("markInput").get_object();
		auto       fields      = input.at("inputFields").get_array();
		const auto futureField = fixture_value(fixture_object{
			{ "name", "futureInputField" },
			{ "defaultValue", nullptr },
			{
				"type",
				aRequired
					? fixture_value(fixture_object{
						  { "kind", "NON_NULL" },
						  { "name", nullptr },
						  {
							  "ofType",
							  fixture_object{
								  { "kind", "SCALAR" },
								  { "name", "String" },
							  },
						  },
					  })
					: fixture_value(fixture_object{
						  { "kind", "SCALAR" },
						  { "name", "String" },
						  { "ofType", nullptr },
					  }),
			},
		});
		fields.push_back(futureField);
		input.insert_or_assign("inputFields", fields);
		data.insert_or_assign("markInput", input);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	[[nodiscard]] auto without_enum_value(
		const std::string& aBody,
		const std::string& aAlias,
		const std::string& aName) -> std::string
	{
		auto       envelope = parse_fixture_object(aBody);
		auto       data     = envelope.at("data").get_object();
		auto       type     = data.at(aAlias).get_object();
		const auto values   = type.at("enumValues").get_array();
		auto       retained = fixture_array{ };
		for (const auto& value : values) {
			if (value.get_object().at("name").get_string() != aName) {
				retained.push_back(value);
			}
		}
		expect(
			retained.size() + 1 == values.size(),
			"targeted enum value exists exactly once");
		type.insert_or_assign("enumValues", retained);
		data.insert_or_assign(aAlias, type);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	[[nodiscard]] auto with_wrong_enum_member_shape(
		const std::string& aBody) -> std::string
	{
		auto envelope = parse_fixture_object(aBody);
		auto data     = envelope.at("data").get_object();
		auto type     = data.at("fileViewedState").get_object();
		auto values   = type.at("enumValues").get_array();
		expect(!values.empty(), "viewed-state enum fixture has values");
		values.at(0) = fixture_object{ { "name", 7 } };
		type.insert_or_assign("enumValues", values);
		data.insert_or_assign("fileViewedState", type);
		envelope.insert_or_assign("data", data);
		return fixture_json(envelope);
	}

	auto test_malformed_duplicates_additions_and_limits() -> void
	{
		const auto supported = load_supported_fixture(
			fixture_root() / "github-com/supported.json");

		expect_all_unknown(
			observe(R"({"data":{},"data":{}})"),
			capability_probe_unavailability::malformed_response,
			"duplicate JSON key rejects all evidence");
		expect_all_unknown(
			observe("{"),
			capability_probe_unavailability::malformed_response,
			"malformed JSON rejects all evidence");
		expect_all_unknown(
			observe(with_duplicate_review_field(supported)),
			capability_probe_unavailability::malformed_response,
			"duplicate introspection field name rejects all evidence");
		expect_all_unknown(
			observe(with_duplicate_mark_argument(supported)),
			capability_probe_unavailability::malformed_response,
			"duplicate introspection argument name rejects all evidence");
		expect_all_unknown(
			observe(with_duplicate_alias_member(
				supported,
				"markInput",
				"inputFields")),
			capability_probe_unavailability::malformed_response,
			"duplicate introspection input name rejects all evidence");
		expect_all_unknown(
			observe(with_duplicate_alias_member(
				supported,
				"fileViewedState",
				"enumValues")),
			capability_probe_unavailability::malformed_response,
			"duplicate introspection enum name rejects all evidence");
		expect_all_unknown(
			observe(with_wrong_enum_member_shape(supported)),
			capability_probe_unavailability::malformed_response,
			"wrong enum member shape rejects all evidence");

		const auto missingEnumValue = observe(without_enum_value(
			supported,
			"fileViewedState",
			"VIEWED"));
		expect(
			missingEnumValue.Complete
				&& missingEnumValue.Capabilities.ViewedFileMutations
					== core::github_capability_state::unsupported
				&& fact(
					   missingEnumValue,
					   capability_schema_fact::viewed_file_state_read)
					== capability_schema_fact_state::incompatible
				&& valid(missingEnumValue),
			"missing required enum value is complete incompatibility");

		const auto additiveOptional = observe(with_extra_mark_argument(supported, false));
		expect_all_supported(
			additiveOptional,
			"additive optional argument preserves compatibility");

		const auto additiveRequired = observe(with_extra_mark_argument(supported, true));
		expect(additiveRequired.Complete, "required addition is complete");
		expect(
			additiveRequired.Capabilities.ViewedFileMutations
					== core::github_capability_state::unsupported
				&& fact(
					   additiveRequired,
					   capability_schema_fact::mark_file_as_viewed)
					== capability_schema_fact_state::incompatible,
			"additive required argument is incompatible");
		expect(valid(additiveRequired), "required-addition evidence is valid");

		const auto optionalInput = observe(with_extra_mark_input_field(supported, false));
		expect_all_supported(
			optionalInput,
			"additive optional input field preserves compatibility");

		const auto requiredInput = observe(with_extra_mark_input_field(supported, true));
		expect(
			requiredInput.Complete
				&& requiredInput.Capabilities.ViewedFileMutations
					== core::github_capability_state::unsupported
				&& fact(
					   requiredInput,
					   capability_schema_fact::mark_file_as_viewed)
					== capability_schema_fact_state::incompatible
				&& valid(requiredInput),
			"additive required input field is incompatible");

		const auto expectResponseLimit =
			[&supported](
				const capability_probe_limits& aLimits,
				const std::string_view         aMessage) {
				expect_all_unknown(
					observe(
						supported,
						true,
						200,
						"application/json; charset=utf-8",
						aLimits),
					capability_probe_unavailability::response_limit,
					aMessage);
			};

		auto limits               = capability_probe_limits{ };
		limits.MaximumQueryFields = 30U;
		expectResponseLimit(limits, "query field limit rejects all evidence");

		limits                       = { };
		limits.MaximumMutationFields = 270U;
		expectResponseLimit(limits, "mutation field limit rejects all evidence");

		limits                         = { };
		limits.MaximumRepositoryFields = 144U;
		expectResponseLimit(limits,
							"repository field limit rejects all evidence");

		limits                          = { };
		limits.MaximumPullRequestFields = 112U;
		expectResponseLimit(limits,
							"pull-request field limit rejects all evidence");

		limits                        = { };
		limits.MaximumOtherTypeFields = 46U;
		expectResponseLimit(
			limits, "other selected-type field limit rejects all evidence");

		limits                   = { };
		limits.MaximumNamedTypes = 279U;
		expectResponseLimit(limits,
							"named-type membership limit rejects all evidence");

		limits                      = { };
		limits.MaximumArrayElements = 3U;
		expectResponseLimit(
			limits,
			"global array limit rejects all evidence");

		limits              = { };
		limits.MaximumDepth = 2U;
		expectResponseLimit(
			limits,
			"JSON depth limit rejects all evidence");

		limits                      = { };
		limits.MaximumObjectMembers = 2U;
		expectResponseLimit(
			limits,
			"JSON object-member limit rejects all evidence");

		limits                  = { };
		limits.MaximumArguments = 1U;
		expectResponseLimit(
			limits,
			"schema argument limit rejects all evidence");

		limits                    = { };
		limits.MaximumInputFields = 2U;
		expectResponseLimit(
			limits,
			"schema input-field limit rejects all evidence");

		limits                   = { };
		limits.MaximumEnumValues = 2U;
		expectResponseLimit(
			limits,
			"schema enum-value limit rejects all evidence");

		limits                    = { };
		limits.MaximumTotalValues = 100U;
		expectResponseLimit(
			limits,
			"JSON total-value limit rejects all evidence");

		limits                    = { };
		limits.MaximumStringBytes = 4U;
		expectResponseLimit(
			limits,
			"JSON string-byte limit rejects all evidence");

		std::string oversized(
			capability_probe_limits{ }.MaximumResponseBytes + 1U,
			'x');
		expect_all_unknown(
			observe(std::move(oversized)),
			capability_probe_unavailability::response_limit,
			"response byte limit rejects all evidence");
	}

	auto test_observation_invariants_reject_tampering() -> void
	{
		const auto supported = observe(load_supported_fixture(
			fixture_root() / "github-com/supported.json"));
		expect(valid(supported), "baseline complete observation is valid");

		auto tampered           = supported;
		tampered.Unavailability = capability_probe_unavailability::network;
		expect(
			!valid(tampered),
			"complete evidence cannot carry an unavailable reason");

		tampered               = supported;
		tampered.Facts.front() = capability_schema_fact_state::unknown;
		expect(
			!valid(tampered),
			"complete evidence cannot contain an unknown fact");

		tampered                            = supported;
		tampered.Capabilities.ResolveThread = core::github_capability_state::unsupported;
		expect(
			!valid(tampered),
			"capabilities must agree with their exact dependency facts");

		auto invalidIncomplete           = capability_probe_observation{ };
		invalidIncomplete.Unavailability = capability_probe_unavailability::none;
		expect(
			!valid(invalidIncomplete),
			"incomplete evidence requires a valid unavailable reason");

		invalidIncomplete                            = { };
		invalidIncomplete.Capabilities.ReviewThreads = core::github_capability_state::supported;
		expect(
			!valid(invalidIncomplete),
			"incomplete evidence cannot expose a supported capability");

		invalidIncomplete               = { };
		invalidIncomplete.Facts.front() = capability_schema_fact_state::matched;
		expect(
			!valid(invalidIncomplete),
			"incomplete evidence cannot retain partial schema facts");
	}
	auto test_graphql_error_code_details_do_not_change_refusal() -> void
	{
		using diagnostic = application::github_capability_diagnostic;
		const std::array codes{
			std::pair{ "FORBIDDEN", diagnostic::graphql_code_forbidden },
			std::pair{ "UNAUTHORIZED", diagnostic::graphql_code_unauthorized },
			std::pair{ "NOT_FOUND", diagnostic::graphql_code_not_found },
			std::pair{ "MAX_NODE_LIMIT_EXCEEDED", diagnostic::graphql_code_max_node_limit_exceeded },
			std::pair{ "INTERNAL", diagnostic::graphql_code_internal },
			std::pair{ "SERVICE_UNAVAILABLE", diagnostic::graphql_code_service_unavailable },
			std::pair{ "GRAPHQL_PARSE_FAILED", diagnostic::graphql_code_parse_failed },
			std::pair{ "GRAPHQL_VALIDATION_FAILED", diagnostic::graphql_code_validation_failed },
			std::pair{ "undefinedField", diagnostic::graphql_code_undefined_field },
			std::pair{ "argumentNotAccepted", diagnostic::graphql_code_argument_not_accepted },
			std::pair{ "undefinedType", diagnostic::graphql_code_undefined_type },
			std::pair{ "selectionMismatch", diagnostic::graphql_code_selection_mismatch },
			std::pair{ "variableMismatch", diagnostic::graphql_code_variable_mismatch },
			std::pair{ "variableNotUsed", diagnostic::graphql_code_variable_not_used },
			std::pair{ "variableRequiresValidType", diagnostic::graphql_code_variable_requires_valid_type },
			std::pair{ "variableNotDefined", diagnostic::graphql_code_variable_not_defined }
		};
		const auto check = [](const fixture_value& aErrors, const diagnostic aExpected) {
			const auto body        = fixture_json(fixture_object{ { "errors", aErrors } });
			const auto observation = observe(body);
			expect_all_unknown(observation, capability_probe_unavailability::partial_response,
							   "error-code refinement retains the existing non-rate refusal");
			expect(observation.CapabilityDetail == aExpected, "error-code detail reports only the exact finite marker or shape");
		};
		for (const auto& [code, detail] : codes) {
			check(fixture_array{ fixture_object{ { "type", code } } }, detail);
			check(fixture_array{ fixture_object{ { "extensions", fixture_object{ { "type", code } } } } }, detail);
			check(fixture_array{ fixture_object{ { "extensions", fixture_object{ { "code", code } } } } }, detail);
			const fixture_object agreeing{ { "type", code }, { "extensions", fixture_object{ { "type", code }, { "code", code } } } };
			check(fixture_array{ agreeing, agreeing }, detail);
		}
		check(nullptr, diagnostic::graphql_errors_not_array);
		check(fixture_object{ }, diagnostic::graphql_errors_not_array);
		check(fixture_array{ }, diagnostic::graphql_errors_empty);
		check(fixture_array{ nullptr }, diagnostic::graphql_errors_malformed);
		check(fixture_array{ fixture_object{ { "type", 42 } } }, diagnostic::graphql_errors_malformed);
		check(fixture_array{ fixture_object{ { "extensions", 42 } } }, diagnostic::graphql_errors_malformed);
		check(fixture_array{ fixture_object{ { "extensions", fixture_object{ { "code", nullptr } } } } }, diagnostic::graphql_errors_malformed);
		const fixture_object known{ { "type", "FORBIDDEN" } };
		const fixture_object unknown{ { "type", "fixture-secret-marker" } };
		check(fixture_array{ unknown }, diagnostic::graphql_errors);
		check(fixture_array{ fixture_object{ { "type", " FORBIDDEN" } } }, diagnostic::graphql_errors);
		check(fixture_array{ fixture_object{ { "message", "FORBIDDEN fixture-secret-marker" }, { "path", fixture_array{ "UNAUTHORIZED" } }, { "locations", fixture_array{ } } } }, diagnostic::graphql_errors);
		check(fixture_array{ known, unknown }, diagnostic::graphql_errors_mixed);
		check(fixture_array{ unknown, known }, diagnostic::graphql_errors_mixed);
		check(fixture_array{ known, fixture_object{ } }, diagnostic::graphql_errors_mixed);
		check(fixture_array{ unknown, unknown }, diagnostic::graphql_errors_mixed);
		check(fixture_array{ known, fixture_object{ { "type", "UNAUTHORIZED" } } }, diagnostic::graphql_errors_mixed);
		check(fixture_array{ fixture_object{ { "type", "FORBIDDEN" }, { "extensions", fixture_object{ { "code", "UNAUTHORIZED" } } } } }, diagnostic::graphql_errors_mixed);
		const auto rate = observe(R"({"errors":[{"type":"FORBIDDEN"},{"type":"RATE_LIMITED"}]})");
		expect_all_unknown(rate, capability_probe_unavailability::rate_limit, "existing rate-limit precedence is unchanged");
		expect(rate.CapabilityDetail == diagnostic::graphql_errors, "rate-limit branch retains its existing detail");
	}

	auto test_closed_capability_refusal_details() -> void
	{
		const auto supported = load_supported_fixture(fixture_root() / "github-com/supported.json");
		using diagnostic     = application::github_capability_diagnostic;
		const std::array cases{
			std::pair{ observe(""), diagnostic::json_shape },
			std::pair{ observe("[]"), diagnostic::json_shape },
			std::pair{ observe(R"({"errors":[]})"), diagnostic::graphql_errors_empty },
			std::pair{ observe("{}"), diagnostic::graphql_data },
			std::pair{ observe(R"({"data":null})"), diagnostic::graphql_data },
			std::pair{ observe(R"({"data":{}})"), diagnostic::graphql_aliases },
			std::pair{ observe(supported, true, 200, "text/plain"), diagnostic::transport_invalid_content_type },
			std::pair{ observe(with_alias_member(supported, "reviewThread", "fields", nullptr)), diagnostic::schema_shape },
			std::pair{ observe(supported, true, 200, "application/json", capability_probe_limits{ .MaximumResponseBytes = 1U }), diagnostic::normalizer_body_limit },
			std::pair{ observe(supported, true, 200, "application/json", capability_probe_limits{ .MaximumDepth = 1U }), diagnostic::json_limits },
			std::pair{ observe(supported, true, 200, "application/json", capability_probe_limits{ .MaximumMutationFields = 1U }), diagnostic::schema_limits },
		};
		for (const auto& [observation, expected] : cases) {
			expect(!observation.Complete && valid(observation)
					   && observation.CapabilityDetail == expected,
				   "normalizer refusal preserves policy and exposes its fixed branch category");
		}
		expect(observe(supported).CapabilityDetail == diagnostic::none,
			   "supported normalization carries no refusal detail");
	}

	void test_safe_capability_error_logging()
	{
		const std::string secret     = "runtime-only-secret-canary";
		const auto        credential = std::as_bytes(std::span{ secret });
		const auto        body       = fixture_json(fixture_object{ { "errors", fixture_array{ fixture_object{ { "message", "Unknown schema field 'newField' is unavailable. " + secret + "\n\x1b[31m forged line https://example.invalid/private?token=x ghp_synthetic_secret_marker" }, { "type", "UNFAMILIAR_SCHEMA_CODE" }, { "extensions", fixture_object{ { "code", secret } } } } } } });
		const auto        result     = sanitize_capability_error(body, credential);
		expect(result.State == application::diagnostic_summary_state::sanitized, "sanitized bounded GraphQL prose");
		expect(result.text().find("Unknown schema field") != std::string_view::npos && result.text().find("unclassified-symbol") != std::string_view::npos && result.text().find("unclassified-code") != std::string_view::npos && result.text().find("newField") == std::string_view::npos && result.text().find("UNFAMILIAR_SCHEMA_CODE") == std::string_view::npos, "unknown errors retain classified explanations without arbitrary symbols or codes");
		for (const auto& forbidden : { secret, std::string{ "ghp_synthetic_secret_marker" }, std::string{ "https://" }, std::string{ "\n" }, std::string{ "\x1b" } }) {
			expect(result.text().find(forbidden) == std::string_view::npos, "forbidden span absent before logging");
		}
		auto        escaped = body;
		std::string encoded;
		for (const auto character : secret) {
			const char digits[] = "0123456789abcdef";
			encoded += "\\u00";
			encoded += digits[(static_cast<unsigned int>(static_cast<unsigned char>(character)) >> 4U) & 15U];
			encoded += digits[static_cast<unsigned char>(character) & 15U];
		}
		for (auto at = escaped.find(secret); at != std::string::npos; at = escaped.find(secret)) {
			escaped.replace(at, secret.size(), encoded);
		}
		const auto escapedResult = sanitize_capability_error(escaped, credential);
		expect(escapedResult.State == application::diagnostic_summary_state::sanitized && escapedResult.text().find(secret) == std::string_view::npos, "JSON escaped reflection removed");
		expect(sanitize_capability_error(R"({"errors":[],"errors":[{"message":"unsafe"}]})", credential).State == application::diagnostic_summary_state::omitted_invalid, "duplicate keys omitted without raw fallback");
		expect(sanitize_capability_error(R"({"access_token":"authentication body"})", credential).text().empty(), "non-error fields excluded");
		expect(sanitize_capability_error(R"({"errors":[{"message":42}]})", credential).text().empty(), "invalid prose omitted");
		expect(sanitize_capability_error(body, { }).text().empty(), "missing credential boundary omits prose");
		for (const auto* message : { "Unknown schema field __schema", "Field 'fields' doesn't exist on type '__Type'", "Field '__type' has a max repetition count of 2" }) {
			const auto useful = sanitize_capability_error(fixture_json(fixture_object{ { "errors", fixture_array{ fixture_object{ { "message", message }, { "type", "NEVER_SEEN_CODE" } } } } }), credential);
			expect(useful.text().find("message=\"[omitted") == std::string_view::npos && useful.text().find("unclassified-code") != std::string_view::npos && useful.text().find("NEVER_SEEN_CODE") == std::string_view::npos, "unknown codes retain useful classified schema explanations");
		}

		const std::string punctuationSecret = "abcd+efgh/ijkl";
		const auto        selective         = sanitize_capability_error(R"({"errors":[{"message":"Unknown schema field newField abcd%2Befgh%2Fijkl"}]})", std::as_bytes(std::span{ punctuationSecret }));
		expect(selective.State == application::diagnostic_summary_state::sanitized && selective.text().find("Unknown schema field") != std::string_view::npos && selective.text().find("abcd") == std::string_view::npos, "selective percent-encoded active credential is removed");
	}
	// Opt-in acceptance probe: failure records the open privacy finding; it
	// must not make retention of a synthetic content marker a passing contract.
	auto audit_capability_error_content() -> int
	{
		const auto credential         = std::as_bytes(std::span{ capability_error_canary_credential });
		bool       contentRetained    = false;
		bool       credentialRetained = false;
		for (const auto& canary : capability_error_content_canaries) {
			const auto summary  = sanitize_capability_error(canary.Body, credential);
			const bool retained = summary.text().find(canary.Marker) != std::string_view::npos;
			contentRetained     = retained || contentRetained;
			credentialRetained  = summary.text().find(capability_error_canary_credential) != std::string_view::npos || credentialRetained;
			std::cout << "sanitizer-content-case=" << canary.Name << " retained=" << retained << '\n';
		}
		std::cout << "sanitizer-content-audit credential_retained=" << credentialRetained << " content_retained=" << contentRetained << '\n';
		return credentialRetained || contentRetained ? EXIT_FAILURE : EXIT_SUCCESS;
	}

	void test_glaze_capability_raw_json_boundaries()
	{
		constexpr std::string_view secret     = "synthetic_boundary_credential";
		const auto                 credential = std::as_bytes(std::span{ secret });
		for (const auto body : { R"({"errors":[{"message":null}]})", R"({"errors":[{"message":"ok","extensions":null}]})",
								 R"({"errors":[{"message":"ok","extensions":{"code":null}}]})", R"({"errors":[{"message":"ok"}]} {})",
								 R"({"errors":[{"message":"ok"}],"extensions":"\ud800"})", R"({"errors":[{"message":"ok"}],"extensions":"\udc00"})",
								 "{\"errors\":[{\"message\":\"ok\"}],\"extensions\":\"\xc3\x28\"}" }) {
			expect(sanitize_capability_error(body, credential).State == application::diagnostic_summary_state::omitted_invalid,
				   "explicit null, trailing documents and malformed skipped Unicode remain invalid diagnostics");
		}
		const auto absent = sanitize_capability_error(R"({"errors":[{"message":"ok"}]})", credential);
		expect(absent.State == application::diagnostic_summary_state::sanitized && absent.text() == "message=\"[omitted:unclassified-content]\" ", "absent optional markers differ from explicit null");
		for (const auto* extension : { "\"\\ud800\"", "\"\\udc00\"", "\"\xc3\x28\"", "[true,]" }) {
			const auto body   = std::string{ "{\"errors\":[{\"type\":\"RATE_LIMITED\"}],\"extensions\":" } + extension + "}";
			const auto result = normalize_capability_probe(capability_probe_response{ .Status = 200, .Authenticated = true, .ContentType = "application/json", .Body = body });
			expect(!result.Complete && result.Unavailability == capability_probe_unavailability::malformed_response, "malformed envelope cannot yield rate or capability facts through a skipped extension");
		}
	}

	void test_capability_batch_aggregate_bounds()
	{
		for (const bool valueLimit : { false, true }) {
			capability_probe_batch batch;
			bool                   refused = false;
			for (std::uint8_t part = 0U; part < 9U; ++part) {
				auto response = capability_probe_response{ .Status = 200, .Authenticated = true, .ContentType = "application/json", .Body = read_file(fixture_root().parent_path() / "v2-parts" / "github-com" / ("supported-" + std::to_string(part) + ".json")) };
				if (valueLimit) {
					auto          envelope = parse_fixture_object(response.Body);
					fixture_array values;
					for (unsigned index = 0U; index < 900U; ++index) {
						values.emplace_back(0);
					}
					fixture_array groups;
					for (unsigned index = 0U; index < 10U; ++index) {
						groups.emplace_back(values);
					}
					envelope.insert_or_assign("extensions", groups);
					response.Body = fixture_json(envelope);
				} else {
					response.Body.append(180000U, ' ');
				}
				expect(normalize_capability_probe(response).Unavailability != capability_probe_unavailability::response_limit, "each selected response independently fits existing limits");
				const auto appended = batch.append(response, part);
				if (!appended) {
					expect(appended.error().Unavailability == capability_probe_unavailability::response_limit, "cumulative byte or value budget refuses before promotion");
					refused = true;
					break;
				}
			}
			expect(refused && !batch.finish().Complete, "cumulative limits prevent a complete capability result");
		}
		auto response = capability_probe_response{ .Status = 200, .Authenticated = true, .ContentType = "application/json", .Body = read_file(fixture_root().parent_path() / "v2-parts" / "github-com" / "supported-0.json") };
		for (const auto key : { "missing", "unexpected" }) {
			auto changed  = response;
			auto envelope = parse_fixture_object(changed.Body);
			auto data     = envelope.at("data").get_object();
			if (std::string_view{ key } == "missing") {
				data.erase("schemaRoot");
			} else {
				data.insert_or_assign("unexpected", fixture_object{ });
			}
			envelope.insert_or_assign("data", data);
			changed.Body = fixture_json(envelope);
			capability_probe_batch batch;
			expect(!batch.append(changed, 0U) && !batch.finish().Complete, "missing or unexpected part aliases refuse without partial facts");
		}
	}

	void test_nine_part_capability_plan()
	{
		for (const auto host : { "github-com", "ghes-3.18" }) {
			capability_probe_batch batch;
			auto                   full     = capability_probe_response{ .Status = 200, .Authenticated = true, .ContentType = "application/json", .Body = read_file(fixture_root() / host / "supported.json") };
			const auto             expected = normalize_capability_probe(full);
			std::size_t            aliases  = 0U;
			for (std::uint8_t part = 0U; part < capability_probe_part_count; ++part) {
				const auto document = capability_probe_graphql_document(part);
				expect(capability_probe_document_is_query_only(document), "every canonical part is query-only");
				expect(document == read_file(fixture_root().parent_path() / "v2-parts" / ("part-" + std::to_string(part) + ".graphql")), "canonical indexed document matches fixture");
				const auto profile       = validate_github_host_profile(github_com_candidate());
				const auto compiled      = compile_capability_probe_request(*profile, { }, part);
				const auto previousBytes = tests::canonical_json::capability_requests.at(part);
				expect(compiled && compiled->Body == previousBytes, "all nine Glaze request bodies retain exact canonical bytes");
				aliases += capability_probe_part_aliases(part).size();
				auto response = full;
				response.Body = read_file(fixture_root().parent_path() / "v2-parts" / host / ("supported-" + std::to_string(part) + ".json"));
				expect(batch.append(response, part).has_value(), "ordered part merges under unchanged bounds");
				if (part + 1U < capability_probe_part_count) {
					expect(!batch.finish().Complete, "partial plan cannot promote capability facts");
				}
			}
			expect(aliases == 26U && batch.finish() == expected && expected.Complete, "merged GitHub/GHES facts equal original selected response");
			expect(!batch.append(full, 0U), "duplicate or restarted part invalidates batch");
			expect(!batch.finish().Complete, "failed batch cannot later promote partials");
		}
		expect(capability_probe_graphql_document(9U).empty() && capability_probe_part_aliases(9U).empty(), "unknown part has no canonical document");
	}

} // namespace

int main(int aArgc, char** aArgv)
{
	test_safe_capability_error_logging();
	if (aArgc == 2 && std::string_view{ aArgv[1] } == "--audit-content-policy") {
		return audit_capability_error_content();
	}
	test_capability_batch_aggregate_bounds();
	test_nine_part_capability_plan();
	test_glaze_capability_raw_json_boundaries();
	test_closed_capability_refusal_details();
	test_graphql_error_code_details_do_not_change_refusal();
	test_requests_bind_validated_profile_authority();
	test_paired_supported_contracts();
	test_synthetic_missing_and_incompatible_contracts();
	test_reply_permission_and_lock_facts();
	test_partial_and_unavailable_evidence_stays_unknown();
	test_rate_limit_classification_is_bounded();
	test_complete_absence_is_unsupported();
	test_null_selected_members_are_incomplete();
	test_capability_dependencies_remain_independent();
	test_compiled_connection_counts_are_required();
	test_compiled_graphql_baseline_is_required();
	test_malformed_duplicates_additions_and_limits();
	test_observation_invariants_reject_tampering();
	std::cout << "github capability probe contracts passed\n";
	return EXIT_SUCCESS;
}
