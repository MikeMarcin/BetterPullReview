// [[BPR-1741]]. The live reaction and resolution course's own process:
// never a CTest row and not built by default (BPR_BUILD_LIVE_REACTION_COURSE).
//
// tests/scripts/reaction_live_course.py makes a pull request with a review
// thread, a pull request comment and a review on MikeMarcin/BPR-Test, then
// runs this tool with a JSON description of them. The tool runs the shipped
// GitHub session (`review_mutation_session::set_reaction`, `resolve_thread`,
// `observe_target`) and the shipped `ListReactions` reader over `gh api`:
// every request is the product's compiled request, every answer is parsed by
// the product's parser, and `gh` only carries the bytes. It prints one
// `REACTION_LIVE:` line per step and `X-RateLimit-Remaining` per request, stops
// below the floor, and keeps each answer in the output directory.
//
// [[BPR-1774]]. tests/scripts/description_live_course.py runs its
// `--description` (the author) and `--description-second` (another account)
// modes on a pull request the owner opened, through the shipped
// `review_mutation_session::edit_description`.
//
// [[BPR-1775]]. tests/scripts/comment_edit_live_course.py runs its
// `--comments` (the author of three comments, one of each kind) and
// `--comments-other` (asked to edit someone else's comments) modes through
// the shipped `review_mutation_session::edit_comment`.
//
// [[BPR-1804]]. tests/scripts/reviewer_live_course.py runs its `--reviewers`
// (the author) and `--reviewers-second` (another account) modes through the
// shipped `review_mutation_session::reviewers`.
#include "gh_cli_response.hpp"
#include "github_host_profile.hpp"
#include "github_mutation_contract.hpp"
#include "github_paged_read.hpp"
#include "github_read_contract.hpp"

#include "bpr/application/review_mutation_recovery_port.hpp"
#include "bpr/core/comment_edit.hpp"
#include "bpr/core/conversation_reactions.hpp"
#include "bpr/core/description_edit.hpp"
#include "bpr/core/review_role.hpp"
#include "bpr/core/reviewer_request.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStringList>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	struct course_configuration
	{
		QJsonObject Json;
		QString     Output;
		int         Floor = 500;
	};

	[[nodiscard]] auto course() -> course_configuration&
	{
		static auto configuration = course_configuration{ };
		return configuration;
	}
	int Requests = 0;

	[[noreturn]] void fail(const std::string_view aWhy)
	{
		std::cout << "REACTION_LIVE:FAILED:" << aWhy << '\n'
				  << std::flush;
		std::exit(EXIT_FAILURE);
	}

	[[nodiscard]] auto text(const char* aKey) -> std::string
	{
		return course().Json.value(QString::fromLatin1(aKey)).toString().toStdString();
	}

	[[nodiscard]] auto number(const char* aKey) -> std::uint64_t
	{
		return static_cast<std::uint64_t>(course().Json.value(QString::fromLatin1(aKey)).toInteger());
	}

	[[nodiscard]] auto oid(const std::string& aHex) -> core::git_commit_oid
	{
		const auto parsed = core::parse_git_commit_oid(aHex);
		if (!parsed) {
			fail("bad-oid");
		}
		return *parsed;
	}

	/// One `gh api` call carrying the product's exact request; the answer as
	/// `gh --include` prints it, parsed by the product's own reader.
	[[nodiscard]] auto run_gh(const std::string& aEndpoint, const std::string& aMethod, const std::vector<http_header>& aHeaders, const std::string& aBody, const std::string& aName)
		-> std::optional<wire_response>
	{
		auto arguments = QStringList{ QStringLiteral("api"), QString::fromStdString(aEndpoint), QStringLiteral("--include"), QStringLiteral("--method"), QString::fromStdString(aMethod) };
		for (const auto& header : aHeaders) {
			if (header.Name == "Authorization") {
				continue;
			}
			arguments << QStringLiteral("--header") << QString::fromStdString(header.Name + ": " + header.Value);
		}
		if (!aBody.empty()) {
			arguments << QStringLiteral("--input") << QStringLiteral("-");
		}
		auto process = QProcess{ };
		process.start(QStringLiteral("gh"), arguments);
		if (!process.waitForStarted(10000)) {
			fail("gh-not-started");
		}
		process.write(QByteArray::fromStdString(aBody));
		process.closeWriteChannel();
		if (!process.waitForFinished(60000)) {
			process.kill();
			process.waitForFinished();
			return std::nullopt;
		}
		const auto output   = process.readAllStandardOutput().toStdString();
		auto       response = parse_gh_cli_response(output);
		++Requests;
		if (!response) {
			std::cout << "REACTION_LIVE:request:" << aName << ":unparsed" << '\n'
					  << std::flush;
			return std::nullopt;
		}
		for (const auto& header : response->Headers) {
			if (header.Name == "x-ratelimit-remaining" || header.Name == "X-RateLimit-Remaining") {
				const auto remaining = std::stoi(header.Value);
				std::cout << "REACTION_LIVE:rate-remaining:" << remaining << " (" << aName << ")" << '\n'
						  << std::flush;
				if (remaining < course().Floor) {
					fail("rate-limit-below-floor");
				}
			}
		}
		if (!course().Output.isEmpty()) {
			auto file = QFile{ course().Output + QStringLiteral("/%1-%2.json").arg(Requests, 2, 10, QLatin1Char{ '0' }).arg(QString::fromStdString(aName)) };
			if (file.open(QIODevice::WriteOnly)) {
				file.write(QByteArray::fromStdString(response->Body));
			}
		}
		return std::move(*response);
	}

	[[nodiscard]] auto operation_name(const review_mutation_operation aOperation) -> std::string
	{
		switch (aOperation) {
		case review_mutation_operation::observe_pull_request:             return "observe-pull-request";
		case review_mutation_operation::observe_viewer:                   return "observe-viewer";
		case review_mutation_operation::observe_reaction_subject:         return "observe-reaction-subject";
		case review_mutation_operation::add_reaction:                     return "add-reaction";
		case review_mutation_operation::remove_reaction:                  return "remove-reaction";
		case review_mutation_operation::observe_pull_request_description: return "observe-description";
		case review_mutation_operation::update_pull_request_description:  return "update-description";
		case review_mutation_operation::observe_comment_for_edit:         return "observe-comment";
		case review_mutation_operation::edit_review_comment:              return "edit-review-comment";
		case review_mutation_operation::edit_issue_comment:               return "edit-issue-comment";
		case review_mutation_operation::edit_review_body:                 return "edit-review-body";
		case review_mutation_operation::observe_reviewers:                return "observe-reviewers";
		case review_mutation_operation::request_reviews:                  return "request-reviews";
		case review_mutation_operation::observe_review_thread:            return "observe-thread";
		case review_mutation_operation::resolve_review_thread:            return "resolve-thread";
		case review_mutation_operation::unresolve_review_thread:          return "unresolve-thread";
		default:                                                          return "other";
		}
	}

	/// The product's mutation wire carried by `gh api`, as the CLI connection
	/// carries it: the GraphQL root is `graphql`, a REST path loses its root.
	class gh_mutation_transport final : public github_mutation_wire_transport
	{
	public:
		[[nodiscard]] auto perform_review_mutation(const github_mutation_wire_request& aRequest, std::stop_token, github_mutation_transmission_witness)
			-> github_mutation_wire_completion override
		{
			const auto& wire = aRequest.Wire;
			if (!validate_outbound_review_mutation_request(wire).has_value()) {
				fail("product-request-not-canonical");
			}
			const auto exchange = review_mutation_exchange_for(wire.Operation);
			if (!exchange) {
				fail("no-exchange");
			}
			Operations.push_back(wire.Operation);
			const auto endpoint = exchange->Graphql ? std::string{ "graphql" } : wire.Path.substr(1U);
			const auto method   = wire.Method == http_method::post ? "POST" : wire.Method == http_method::get ? "GET"
																											  : "PATCH";
			const auto response = run_gh(endpoint, method, wire.Headers, wire.Body, operation_name(wire.Operation));
			if (!response) {
				return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable, .Transmission = github_mutation_transmission::indeterminate };
			}
			return github_mutation_wire_success{ .Response = { .Status = response->Status, .Headers = response->Headers, .Body = response->Body } };
		}

		std::vector<review_mutation_operation> Operations;
	};

	/// The product's read wire carried by `gh api graphql`.
	class gh_read_transport final : public github_wire_transport_port
	{
	public:
		[[nodiscard]] auto perform(const github_wire_request& aRequest, std::stop_token) -> github_wire_completion override
		{
			if (!validate_outbound_read_request(aRequest.Wire).has_value()) {
				fail("product-read-not-canonical");
			}
			auto response = run_gh("graphql", "POST", aRequest.Wire.Headers, aRequest.Wire.Body, "list-reactions");
			if (!response) {
				return github_wire_error{ .Correlation = aRequest.Correlation, .Error = { .Failure = application::github_provider_failure::unavailable } };
			}
			return github_wire_success{ .Correlation = aRequest.Correlation, .Response = std::move(*response) };
		}
	};

	class lease final : public application::review_mutation_transport_lease
	{
	};

	/// No recovery record is kept for a reaction or a resolution.
	class no_recovery final : public application::review_mutation_recovery_port
	{
	public:
		auto load(const std::string&) -> std::expected<std::optional<application::review_mutation_recovery_record>, application::review_mutation_recovery_failure> override
		{
			return std::unexpected(application::review_mutation_recovery_failure::unavailable);
		}
		auto admit(const application::review_mutation_recovery_record&) -> std::expected<application::review_mutation_recovery_admission, application::review_mutation_recovery_failure> override
		{
			return std::unexpected(application::review_mutation_recovery_failure::unavailable);
		}
		auto mark_possibly_transmitted(const std::string&, const std::array<std::uint8_t, 32>&)
			-> std::expected<application::review_mutation_recovery_record, application::review_mutation_recovery_failure> override
		{
			return std::unexpected(application::review_mutation_recovery_failure::unavailable);
		}
		auto resolve(const std::string&, const std::array<std::uint8_t, 32>&, const application::review_mutation_receipt&)
			-> std::expected<application::review_mutation_recovery_record, application::review_mutation_recovery_failure> override
		{
			return std::unexpected(application::review_mutation_recovery_failure::unavailable);
		}
	};

	[[nodiscard]] auto target() -> application::review_mutation_target_binding
	{
		return {
			.Semantic = {
				.Host         = core::git_host_id{ 11U },
				.Repository   = core::repository_id{ 12U },
				.PullRequest  = core::pull_request_id{ 13U },
				.ExpectedHead = oid(text("head")),
			},
			.Hostname                 = application::github_canonical_hostname{ "github.com" },
			.ServerContract           = application::github_server_contract::github_com_2022_11_28,
			.Repository               = { .Owner = text("owner"), .Name = text("name") },
			.Number                   = core::pull_request_number{ static_cast<std::uint32_t>(number("number")) },
			.RepositoryDatabaseId     = number("repositoryDatabaseId"),
			.RepositoryNodeId         = text("repositoryNode"),
			.PullRequestDatabaseId    = number("pullRequestDatabaseId"),
			.PullRequestNodeId        = text("pullRequestNode"),
			.BaseRepositoryDatabaseId = number("repositoryDatabaseId"),
			.BaseOid                  = oid(text("base")),
			.BaseRef                  = text("baseRef"),
			.HeadRef                  = text("headRef"),
			.Open                     = true,
		};
	}

	[[nodiscard]] auto actor() -> application::review_mutation_acting_identity
	{
		return {
			.Account    = core::github_account_id{ number("actorId") },
			.DatabaseId = number("actorId"),
			.NodeId     = text("actorNode"),
			.Login      = text("actorLogin"),
		};
	}

	[[nodiscard]] auto configuration() -> live_review_mutation_configuration
	{
		return {
			.CredentialContext = {
				.Request            = core::request_id{ 31U },
				.ApplicationSession = core::application_session_id{ 32U },
				.Authority          = {
					.Profile         = core::host_profile_id{ 33U },
					.ProfileRevision = application::github_profile_revision{ 34U },
					.Host            = core::git_host_id{ 11U },
					.Hostname        = application::github_canonical_hostname{ "github.com" },
					.Account         = core::github_account_id{ number("actorId") },
					.ServerContract  = application::github_server_contract::github_com_2022_11_28,
				},
				.IdentityGeneration = core::github_identity_generation{ 35U },
			},
			.Target         = target(),
			.ActingIdentity = actor(),
		};
	}

	[[nodiscard]] auto binding() -> application::review_mutation_live_binding
	{
		return { .Target = target(), .ActingIdentity = actor() };
	}

	[[nodiscard]] auto reaction(const std::string& aSubject, const core::reaction_content aContent, const bool aAdd) -> application::review_reaction_request
	{
		return { .Target = target().Semantic, .ActingIdentity = actor().Account, .Subject = aSubject, .Content = aContent, .Add = aAdd, .LiveBinding = binding() };
	}

	[[nodiscard]] auto describe(const core::reaction_summary& aSummary) -> std::string
	{
		auto words = std::string{ };
		for (const auto& group : aSummary.Groups) {
			words += std::string{ words.empty() ? "" : "," } + std::string{ core::reaction_graphql_name(group.Content) } + "=" + std::to_string(group.Count)
				+ (group.ViewerReacted ? "(mine)" : "");
		}
		return words.empty() ? "none" : words;
	}

	void react(review_mutation_session& aSession, gh_mutation_transport& aWire, const std::string& aStep, const std::string& aSubject, const core::reaction_content aContent,
			   const bool aAdd, const bool aExpectWrite)
	{
		const auto before  = aWire.Operations.size();
		const auto receipt = aSession.set_reaction(reaction(aSubject, aContent, aAdd));
		if (!receipt) {
			fail(aStep + ":" + std::string{ application::name(receipt.error()) });
		}
		const auto writes = static_cast<std::size_t>(std::ranges::count_if(aWire.Operations.begin() + static_cast<std::ptrdiff_t>(before), aWire.Operations.end(), [](const auto aOperation) {
			return aOperation == review_mutation_operation::add_reaction || aOperation == review_mutation_operation::remove_reaction;
		}));
		if (receipt->Reacted != aAdd || receipt->Written != aExpectWrite || writes != (aExpectWrite ? 1U : 0U)) {
			fail(aStep + ":unexpected-receipt");
		}
		std::cout << "REACTION_LIVE:" << aStep << ":ok written=" << receipt->Written << " readBack=" << receipt->ConfirmedByReadBack << " reactions=" << describe(receipt->Reactions)
				  << '\n'
				  << std::flush;
	}

	void resolve(review_mutation_session& aSession, gh_mutation_transport& aWire, const std::string& aStep, const bool aResolve, const bool aExpectWrite)
	{
		const auto before  = aWire.Operations.size();
		const auto receipt = aSession.resolve_thread(application::review_thread_resolution_request{
			.Target = target().Semantic, .ActingIdentity = actor().Account, .Thread = text("thread"), .Resolve = aResolve, .LiveBinding = binding() });
		if (!receipt) {
			fail(aStep + ":" + std::string{ application::name(receipt.error()) });
		}
		const auto writes = static_cast<std::size_t>(std::ranges::count_if(aWire.Operations.begin() + static_cast<std::ptrdiff_t>(before), aWire.Operations.end(), [](const auto aOperation) {
			return aOperation == review_mutation_operation::resolve_review_thread || aOperation == review_mutation_operation::unresolve_review_thread;
		}));
		if (receipt->Resolved != aResolve || writes != (aExpectWrite ? 1U : 0U)) {
			fail(aStep + ":unexpected-receipt");
		}
		std::cout << "REACTION_LIVE:" << aStep << ":ok resolved=" << receipt->Resolved << " written=" << writes << '\n'
				  << std::flush;
	}

	[[nodiscard]] auto read_profile() -> validated_github_host_profile
	{
		const auto result = validate_github_host_profile(github_host_profile_candidate{
			.Profile        = core::host_profile_id{ 33U },
			.Host           = core::git_host_id{ 11U },
			.Revision       = application::github_profile_revision{ 34U },
			.Server         = application::github_server_contract::github_com_2022_11_28,
			.Authentication = application::github_authentication_transport::native_https,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::explicit_user,
			.Hostname       = "github.com",
			.RestRoot       = "https://api.github.com",
			.GraphqlRoot    = "https://api.github.com/graphql",
		});
		if (!result) {
			fail("read-profile");
		}
		return *result;
	}

	class account_lease final : public application::github_account_transport_lease
	{
	};

	[[nodiscard]] auto read_request() -> application::github_pull_request_provider_request
	{
		const application::github_capability_authority_key authority{
			.Profile               = core::host_profile_id{ 33U },
			.ProfileRevision       = application::github_profile_revision{ 34U },
			.Host                  = core::git_host_id{ 11U },
			.Account               = core::github_account_id{ number("actorId") },
			.ServerContract        = application::github_server_contract::github_com_2022_11_28,
			.ProbeContractRevision = application::github_probe_contract_revision{ 1U },
		};
		std::array<application::github_capability_fact, application::github_capability_fact_count> aggregate{ };
		for (std::size_t index = 0U; index < aggregate.size(); ++index) {
			aggregate[index] = { .Capability = application::canonical_github_capabilities[index],
								 .State      = core::github_capability_state::supported,
								 .Source     = application::github_capability_evidence_source::observed_contract };
		}
		std::array<application::github_schema_capability_fact, application::github_schema_capability_fact_count> schema{ };
		for (std::size_t index = 0U; index < schema.size(); ++index) {
			schema[index] = { .Capability = application::canonical_github_schema_capabilities[index],
							  .State      = core::github_capability_state::supported,
							  .Source     = application::github_capability_evidence_source::observed_contract };
		}
		const auto observation = application::github_capability_probe_observation{
			.Authority   = authority,
			.ObservedAt  = application::github_freshness_epoch{ 100U },
			.Facts       = aggregate,
			.SchemaFacts = schema,
		};
		return {
			.Context = {
				.Request            = core::request_id{ 41U },
				.ApplicationSession = core::application_session_id{ 32U },
				.Profile            = core::host_profile_id{ 33U },
				.Host               = core::git_host_id{ 11U },
				.Account            = core::github_account_id{ number("actorId") },
				.Repository         = { .Owner = text("owner"), .Name = text("name") },
				.Number             = core::pull_request_number{ static_cast<std::uint32_t>(number("number")) },
				.Capabilities       = core::capability_revision{ 44U },
				.IdentityGeneration = core::github_identity_generation{ 35U },
				.Generation         = core::request_generation{ 9U },
			},
			.ProfileAuthority = {
				.Profile               = core::host_profile_id{ 33U },
				.ProfileRevision       = application::github_profile_revision{ 34U },
				.Host                  = core::git_host_id{ 11U },
				.Hostname              = application::github_canonical_hostname{ "github.com" },
				.ServerContract        = application::github_server_contract::github_com_2022_11_28,
				.Authentication        = application::github_authentication_transport::native_https,
				.ProbeContractRevision = application::github_probe_contract_revision{ 1U },
			},
			.Viewer = {
				.Account = core::github_account_id{ number("actorId") },
				.Node    = application::github_actor_id{ text("actorNode") },
				.Login   = text("actorLogin"),
			},
			.CapabilityEvidence = {
				.Authority     = authority,
				.Facts         = aggregate,
				.SchemaFacts   = schema,
				.Capabilities  = application::projected_capabilities(observation),
				.ReadShape     = application::projected_read_shape(observation),
				.Revision      = core::capability_revision{ 44U },
				.ObservedAt    = application::github_freshness_epoch{ 100U },
				.ExpiresBefore = application::github_freshness_epoch{ 400U },
			},
			.AccountTransport = std::make_shared<account_lease>(),
			.Attempt          = application::github_provider_attempt{ 1U },
		};
	}

	[[nodiscard]] auto read_reactions(const std::string& aStep) -> std::vector<application::github_reaction_record>
	{
		auto       transport = gh_read_transport{ };
		const auto subjects  = std::vector<std::string>{ text("reviewComment"), text("issueComment"), text("review") };
		const auto read      = perform_github_reaction_read(transport, read_profile(), read_request(), core::repository_id{ 12U }, text("repositoryNode"), text("pullRequestNode"), subjects, { });
		if (!read) {
			const auto* const error = std::get_if<application::github_provider_error>(&read.error());
			fail(aStep + ":read-failed:" + (error != nullptr ? std::to_string(static_cast<int>(error->Failure)) : std::string{ "cancelled" }) + ":requests=" + std::to_string(Requests));
		}
		for (const auto& record : read->Records) {
			std::cout << "REACTION_LIVE:" << aStep << ":" << record.Subject << " canReact=" << record.Reactions.ViewerCanReact << " " << describe(record.Reactions) << '\n'
					  << std::flush;
		}
		return read->Records;
	}

	[[nodiscard]] auto find(const std::vector<application::github_reaction_record>& aRecords, const std::string& aSubject) -> core::reaction_summary
	{
		for (const auto& record : aRecords) {
			if (record.Subject == aSubject) {
				return record.Reactions;
			}
		}
		fail("subject-not-read");
	}
	/// [[BPR-1774]]. One description save through the shipped session; the
	/// receipt and the writes it made are held to what is expected.
	[[nodiscard]] auto edit(review_mutation_session& aSession, gh_mutation_transport& aWire, const std::string& aStep, const std::string& aOriginal, const std::string& aBody)
		-> std::pair<std::expected<application::review_description_receipt, application::review_mutation_failure>, std::size_t>
	{
		const auto before  = aWire.Operations.size();
		auto       receipt = aSession.edit_description(application::review_description_request{
			.Target = target().Semantic, .ActingIdentity = actor().Account, .Original = aOriginal, .Body = aBody, .LiveBinding = binding() });
		const auto writes  = static_cast<std::size_t>(std::ranges::count(aWire.Operations.begin() + static_cast<std::ptrdiff_t>(before), aWire.Operations.end(),
																		 review_mutation_operation::update_pull_request_description));
		std::cout << "REACTION_LIVE:" << aStep << ":" << (receipt ? "answered" : std::string{ application::name(receipt.error()) }) << " writes=" << writes
				  << (receipt ? " written=" + std::to_string(receipt->Written) + " conflict=" + std::to_string(receipt->Conflict) + " readBack=" + std::to_string(receipt->ConfirmedByReadBack) : std::string{ })
				  << '\n'
				  << std::flush;
		return { std::move(receipt), writes };
	}

	/// [[BPR-1774]]. The author edits the description, sees GitHub's answer,
	/// saves the same text again (nothing sent), is refused a stale original
	/// as a conflict (nothing sent, GitHub's body back), and restores it.
	void description_course(review_mutation_session& aSession, gh_mutation_transport& aWire)
	{
		const auto original = text("originalBody");
		const auto edited   = text("editedBody");
		if (!core::description_edit_offered(text("author"), text("actorLogin"))) {
			fail("description:author-not-offered");
		}
		const auto [saved, savedWrites] = edit(aSession, aWire, "description-saved", original, edited);
		if (!saved || !saved->Written || saved->Conflict || saved->Body != edited || savedWrites != 1U) {
			fail("description-saved:unexpected");
		}
		const auto [again, againWrites] = edit(aSession, aWire, "description-saved-again", original, edited);
		if (!again || again->Written || again->Conflict || againWrites != 0U) {
			fail("description-saved-again:unexpected");
		}
		const auto [stale, staleWrites] = edit(aSession, aWire, "description-conflict", "A body GitHub no longer has.", "Must not be written.");
		if (!stale || !stale->Conflict || stale->Written || stale->Body != edited || staleWrites != 0U) {
			fail("description-conflict:unexpected");
		}
		const auto [restored, restoredWrites] = edit(aSession, aWire, "description-restored", edited, original);
		if (!restored || !restored->Written || restored->Body != original || restoredWrites != 1U) {
			fail("description-restored:unexpected");
		}
	}

	/// [[BPR-1774]]. Another account: the composition offers it no Edit, and
	/// the session, asked anyway with a stale original, writes nothing:
	/// refused by GitHub when it may not update, a conflict when it may.
	void description_second_course(review_mutation_session& aSession, gh_mutation_transport& aWire)
	{
		const auto offered = core::description_edit_offered(text("author"), text("actorLogin"));
		std::cout << "REACTION_LIVE:description-second-offered:" << offered << '\n'
				  << std::flush;
		if (offered) {
			fail("description-second:offered");
		}
		const auto [answer, writes] = edit(aSession, aWire, "description-second-asked", "A body GitHub no longer has.", "Must not be written.");
		const auto refused          = !answer && answer.error() == application::review_mutation_failure::refused_by_server;
		const auto conflict         = answer && answer->Conflict && !answer->Written;
		if (writes != 0U || (!refused && !conflict)) {
			fail("description-second:unexpected");
		}
	}

	/// [[BPR-1775]]. One comment save through the shipped session; the
	/// receipt and the writes it made are held to what is expected.
	[[nodiscard]] auto edit_comment(review_mutation_session& aSession, gh_mutation_transport& aWire, const std::string& aStep, const std::string& aPost, const std::string& aOriginal, const std::string& aBody)
		-> std::pair<std::expected<application::review_comment_edit_receipt, application::review_mutation_failure>, std::size_t>
	{
		const auto subject = core::comment_edit_subject_of(aPost);
		if (!subject.has_value()) {
			fail(aStep + ":not-a-comment");
		}
		const auto before  = aWire.Operations.size();
		auto       receipt = aSession.edit_comment(application::review_comment_edit_request{ .Target = target().Semantic, .ActingIdentity = actor().Account, .Kind = subject->Kind, .Subject = std::string{ subject->Node }, .Original = aOriginal, .Body = aBody, .LiveBinding = binding() });
		const auto writes  = static_cast<std::size_t>(std::ranges::count_if(aWire.Operations.begin() + static_cast<std::ptrdiff_t>(before), aWire.Operations.end(), [](const review_mutation_operation aOperation) {
			return aOperation == review_mutation_operation::edit_review_comment || aOperation == review_mutation_operation::edit_issue_comment || aOperation == review_mutation_operation::edit_review_body;
		}));
		std::cout << "REACTION_LIVE:" << aStep << ":" << (receipt ? "answered" : std::string{ application::name(receipt.error()) }) << " writes=" << writes
				  << (receipt ? " written=" + std::to_string(receipt->Written) + " conflict=" + std::to_string(receipt->Conflict) + " readBack=" + std::to_string(receipt->ConfirmedByReadBack) : std::string{ })
				  << '\n'
				  << std::flush;
		return { std::move(receipt), writes };
	}

	/// [[BPR-1775]]. The author of one comment of each kind edits it, sees
	/// GitHub's answer, saves the same text again (nothing sent), is refused a
	/// stale original as a conflict (nothing sent, GitHub's body back), and
	/// restores it.
	void comments_course(review_mutation_session& aSession, gh_mutation_transport& aWire)
	{
		for (const auto& [key, name] : { std::pair{ "reviewComment", "review-comment" }, std::pair{ "issueComment", "issue-comment" }, std::pair{ "review", "review-body" } }) {
			const auto post     = text(key);
			const auto original = text((std::string{ key } + "Body").c_str());
			const auto edited   = original + "\r\nEdited through the shipped comment session.\n";
			const auto step     = std::string{ name };
			if (!core::comment_edit_offered(post, true, false)) {
				fail(step + ":not-offered");
			}
			const auto [saved, savedWrites] = edit_comment(aSession, aWire, step + "-saved", post, original, edited);
			if (!saved || !saved->Written || saved->Conflict || saved->Body != edited || savedWrites != 1U) {
				fail(step + "-saved:unexpected");
			}
			const auto [again, againWrites] = edit_comment(aSession, aWire, step + "-saved-again", post, original, edited);
			if (!again || again->Written || again->Conflict || againWrites != 0U) {
				fail(step + "-saved-again:unexpected");
			}
			const auto [stale, staleWrites] = edit_comment(aSession, aWire, step + "-conflict", post, "A body GitHub no longer has.", "Must not be written.");
			if (!stale || !stale->Conflict || stale->Written || stale->Body != edited || staleWrites != 0U) {
				fail(step + "-conflict:unexpected");
			}
			const auto [restored, restoredWrites] = edit_comment(aSession, aWire, step + "-restored", post, edited, original);
			if (!restored || !restored->Written || restored->Body != original || restoredWrites != 1U) {
				fail(step + "-restored:unexpected");
			}
		}
	}

	/// [[BPR-1775]]. Someone else's comments: the composition offers no Edit
	/// (not mine), and the session, asked anyway with GitHub's own body,
	/// writes nothing because GitHub says this viewer may not update them.
	void comments_other_course(review_mutation_session& aSession, gh_mutation_transport& aWire)
	{
		const auto others = course().Json.value(QStringLiteral("otherComments")).toArray();
		if (others.isEmpty()) {
			fail("comments-other:none");
		}
		for (const auto& entry : others) {
			const auto post = entry.toObject().value(QStringLiteral("post")).toString().toStdString();
			const auto body = entry.toObject().value(QStringLiteral("body")).toString().toStdString();
			const auto step = "other-" + post.substr(0, post.find(':'));
			std::cout << "REACTION_LIVE:" << step << "-offered:" << core::comment_edit_offered(post, false, false) << '\n'
					  << std::flush;
			const auto [answer, writes] = edit_comment(aSession, aWire, step + "-asked", post, body, body + " Must not be written.");
			if (writes != 0U || answer || answer.error() != application::review_mutation_failure::refused_by_server) {
				fail(step + ":unexpected");
			}
		}
	}
	/// [[BPR-1804]]. One reviewer operation through the shipped session; the
	/// receipt and the writes it made are printed and returned.
	[[nodiscard]] auto reviewers(review_mutation_session& aSession, gh_mutation_transport& aWire, const std::string& aStep, const application::review_reviewer_operation aOperation,
								 const std::string& aQuery, const std::string& aUser = { }, const std::string& aLogin = { })
		-> std::pair<std::expected<application::review_reviewer_receipt, application::review_mutation_failure>, std::size_t>
	{
		const auto before  = aWire.Operations.size();
		auto       receipt = aSession.reviewers(application::review_reviewer_request{
			.Target = target().Semantic, .ActingIdentity = actor().Account, .Operation = aOperation, .Query = aQuery, .User = aUser, .Login = aLogin, .LiveBinding = binding() });
		const auto writes  = static_cast<std::size_t>(std::ranges::count(aWire.Operations.begin() + static_cast<std::ptrdiff_t>(before), aWire.Operations.end(),
																		 review_mutation_operation::request_reviews));
		auto       facts   = std::string{ };
		if (receipt) {
			const auto rows = core::reviewer_rows(receipt->Answer, aQuery);
			facts += " author=" + receipt->Answer.Author + " suggested=" + std::to_string(receipt->Answer.Suggested.size()) + " matches=" + std::to_string(receipt->Answer.Matches.size())
				+ " rows=" + std::to_string(rows.size()) + " requested=" + std::to_string(receipt->Answer.Requested.size()) + " written=" + std::to_string(receipt->Written);
		}
		std::cout << "REACTION_LIVE:" << aStep << ":" << (receipt ? "answered" : std::string{ application::name(receipt.error()) }) << " writes=" << writes << facts << '\n'
				  << std::flush;
		return { std::move(receipt), writes };
	}

	/// [[BPR-1804]]. The author searches (everyone, then their own login,
	/// which GitHub lists but the picker never shows), asks for a review
	/// from someone who may not review here (written once, and GitHub's
	/// answer does not list them: the core says not added), and is refused
	/// a request of themselves before anything is written.
	void reviewers_course(review_mutation_session& aSession, gh_mutation_transport& aWire)
	{
		using operation      = application::review_reviewer_operation;
		const auto [all, _a] = reviewers(aSession, aWire, "reviewers-search-all", operation::search, "");
		const auto rowsAll   = all ? core::reviewer_rows(all->Answer, "") : std::vector<core::reviewer_row>{ };
		if (!all || all->Answer.Author != text("author") || std::ranges::any_of(rowsAll, [](const core::reviewer_row& aRow) { return core::same_login(aRow.Candidate.Login, text("author")); })) {
			fail("reviewers-search-all:unexpected");
		}
		const auto [own, _o] = reviewers(aSession, aWire, "reviewers-search-author", operation::search, text("author"));
		if (!own || std::ranges::none_of(own->Answer.Matches, [](const core::reviewer_candidate& aPerson) { return aPerson.Login == text("author"); })
			|| !core::reviewer_rows(own->Answer, text("author")).empty()) {
			fail("reviewers-search-author:unexpected");
		}
		auto state                     = core::accept_reviewer_search(core::open_reviewer_picker({ }).State, 1U, all->Answer);
		const auto [outsider, written] = reviewers(aSession, aWire, "reviewers-request-outsider", operation::request, "", text("secondNode"), text("secondLogin"));
		if (!outsider || written != 1U) {
			fail("reviewers-request-outsider:unexpected");
		}
		state.Rows.push_back(core::reviewer_row{ .Candidate = { .NodeId = text("secondNode"), .Login = text("secondLogin") } });
		const auto chose = core::choose_reviewer(state, text("secondLogin"));
		const auto said  = core::finish_reviewer_request(chose.State, chose.State.Request, { .Requested = outsider->Answer.Requested, .Written = outsider->Written });
		std::cout << "REACTION_LIVE:reviewers-outsider-notice:" << core::name(said.Notice) << '\n'
				  << std::flush;
		if (said.Notice != core::reviewer_notice::not_added) {
			fail("reviewers-outsider:not-said-not-added");
		}
		const auto [self, selfWrites] = reviewers(aSession, aWire, "reviewers-request-author", operation::request, "", text("actorNode"), text("author"));
		if (self || self.error() != application::review_mutation_failure::refused_by_server || selfWrites != 0U) {
			fail("reviewers-request-author:unexpected");
		}
	}

	/// [[BPR-1804]]. Another account, not a collaborator: its search is
	/// answered or refused, and asking for its own review is never claimed.
	void reviewers_second_course(review_mutation_session& aSession, gh_mutation_transport& aWire)
	{
		using operation           = application::review_reviewer_operation;
		const auto [found, _f]    = reviewers(aSession, aWire, "reviewers-second-search", operation::search, "");
		const auto [asked, wrote] = reviewers(aSession, aWire, "reviewers-second-request-self", operation::request, "", text("actorNode"), text("actorLogin"));
		const auto listed         = asked && std::ranges::any_of(asked->Answer.Requested, [](const std::string& aLogin) { return core::same_login(aLogin, text("actorLogin")); });
		if (wrote > 1U || listed) {
			fail("reviewers-second:unexpected");
		}
		static_cast<void>(found);
	}
} // namespace

auto main(int aCount, char** aValues) -> int
{
	QCoreApplication application{ aCount, aValues };
	const auto       arguments = QCoreApplication::arguments();
	if (arguments.size() < 4) {
		std::cerr << "usage: reaction_live_course (--main|--second-add|--second-remove|--read|--description|--description-second|--comments|--comments-other|--reviewers|--reviewers-second) CONFIG OUTPUT [FLOOR]\n";
		return EXIT_FAILURE;
	}
	auto file = QFile{ arguments[2] };
	if (!file.open(QIODevice::ReadOnly)) {
		fail("config");
	}
	course().Json    = QJsonDocument::fromJson(file.readAll()).object();
	course().Output  = arguments[3];
	course().Floor   = arguments.size() > 4 ? arguments[4].toInt() : 500;
	const auto& mode = arguments[1];
	if (text("owner") != "MikeMarcin" || text("name") != "BPR-Test") {
		fail("not-the-designated-repository");
	}
	auto wire    = gh_mutation_transport{ };
	auto session = review_mutation_session{ wire, configuration(), std::make_shared<lease>(), std::make_unique<no_recovery>() };
	if (mode != QStringLiteral("--read")) {
		// The coordinator's rule before every write: GitHub still describes the
		// designated pull request.
		const auto observed = session.observe_target();
		if (!observed) {
			fail("observe-target:" + std::string{ application::name(observed.error()) });
		}
		std::cout << "REACTION_LIVE:target:ok" << '\n'
				  << std::flush;
	}
	const auto comment = text("reviewComment");
	if (mode == QStringLiteral("--second-add")) {
		react(session, wire, "second-adds-thumbs-up", comment, core::reaction_content::thumbs_up, true, true);
	} else if (mode == QStringLiteral("--second-remove")) {
		react(session, wire, "second-removes-thumbs-up", comment, core::reaction_content::thumbs_up, false, true);
	} else if (mode == QStringLiteral("--read")) {
		static_cast<void>(read_reactions("read"));
	} else if (mode == QStringLiteral("--description")) {
		description_course(session, wire);
	} else if (mode == QStringLiteral("--description-second")) {
		description_second_course(session, wire);
	} else if (mode == QStringLiteral("--comments")) {
		comments_course(session, wire);
	} else if (mode == QStringLiteral("--reviewers")) {
		reviewers_course(session, wire);
	} else if (mode == QStringLiteral("--reviewers-second")) {
		reviewers_second_course(session, wire);
	} else if (mode == QStringLiteral("--comments-other")) {
		comments_other_course(session, wire);
	} else if (mode == QStringLiteral("--main")) {
		const auto first  = find(read_reactions("before"), comment);
		const auto thumbs = core::find_reaction_group(first, core::reaction_content::thumbs_up);
		if (thumbs == nullptr || thumbs->Count != 1U || thumbs->ViewerReacted) {
			fail("before:second-identity-thumbs-up-not-read");
		}
		react(session, wire, "adds-heart", comment, core::reaction_content::heart, true, true);
		react(session, wire, "adds-heart-again", comment, core::reaction_content::heart, true, false);
		react(session, wire, "adds-thumbs-up", comment, core::reaction_content::thumbs_up, true, true);
		const auto both    = find(read_reactions("after-adding"), comment);
		const auto counted = core::find_reaction_group(both, core::reaction_content::thumbs_up);
		if (counted == nullptr || counted->Count != 2U || !counted->ViewerReacted) {
			fail("after-adding:thumbs-up-not-two-with-mine");
		}
		react(session, wire, "removes-thumbs-up", comment, core::reaction_content::thumbs_up, false, true);
		react(session, wire, "removes-heart", comment, core::reaction_content::heart, false, true);
		react(session, wire, "removes-heart-again", comment, core::reaction_content::heart, false, false);
		react(session, wire, "issue-comment-rocket", text("issueComment"), core::reaction_content::rocket, true, true);
		react(session, wire, "issue-comment-rocket-removed", text("issueComment"), core::reaction_content::rocket, false, true);
		react(session, wire, "review-eyes", text("review"), core::reaction_content::eyes, true, true);
		react(session, wire, "review-eyes-removed", text("review"), core::reaction_content::eyes, false, true);
		resolve(session, wire, "resolves", true, true);
		resolve(session, wire, "resolves-again", true, false);
		resolve(session, wire, "unresolves", false, true);
		const auto mine = find(read_reactions("after"), comment);
		const auto left = core::find_reaction_group(mine, core::reaction_content::thumbs_up);
		if (left == nullptr || left->Count != 1U || left->ViewerReacted || core::find_reaction_group(mine, core::reaction_content::heart) != nullptr) {
			fail("after:not-back-to-the-second-identity-alone");
		}
	} else {
		fail("mode");
	}
	std::cout << "REACTION_LIVE:requests:" << Requests << '\n'
			  << std::flush;
	std::cout << "REACTION_LIVE:OK" << '\n'
			  << std::flush;
	return EXIT_SUCCESS;
}
