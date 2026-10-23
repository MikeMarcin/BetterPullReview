// [[BPR-1745]]. The live outdated-thread reply course's own process:
// never a CTest row and not built by default
// (BPR_BUILD_LIVE_OUTDATED_REPLY_COURSE).
//
// tests/scripts/outdated_reply_live_course.py makes a pull request on
// MikeMarcin/BPR-Test, a review thread on its line 3 by the second identity,
// and a push that changes the line, so GitHub reads the thread outdated. It
// then runs this tool with a JSON description of them. The tool takes the
// product's decided path: the core decides the route from GitHub's thread
// data (`core::decide_thread_reply`), the application host holds the reply
// to its thread (`review_mutation_application_host::publish` with the
// thread's binding), and the shipped GitHub session writes, journals in the
// SQLite recovery store and reconciles it against the thread. Every request is
// the product's compiled request and every answer is parsed by the product's
// parser; `gh` only carries the bytes, as the CLI connection's transport would
// for a reply. It prints one `OUTDATED_REPLY_LIVE:` line per step and
// `X-RateLimit-Remaining` per request, stops below the floor, and keeps each
// answer in the output directory.
//
// Modes, over one journal directory:
//   --reply        publish the reply; a new receipt
//   --again        the same reply again, a new process: answered from the
//                  journal, nothing written
//   --lose-answer  a second reply whose answer is dropped after it is sent:
//                  the outcome is unclear, and the session's reconciliation
//                  finds the reply in the thread, where GitHub holds it with
//                  no line, and records it in the journal
//   --recover      that reply again, a new process: answered from the
//                  journal; nothing written
#include "gh_cli_response.hpp"
#include "github_mutation_contract.hpp"
#include "live_review_mutation_provider.hpp"

#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_mutation_application_host.hpp"
#include "bpr/core/thread_reply.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStringList>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
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
		std::cout << "OUTDATED_REPLY_LIVE:FAILED:" << aWhy << '\n'
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

	[[nodiscard]] auto flag(const char* aKey) -> std::optional<bool>
	{
		const auto value = course().Json.value(QString::fromLatin1(aKey));
		return value.isBool() ? std::optional{ value.toBool() } : std::nullopt;
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
			std::cout << "OUTDATED_REPLY_LIVE:request:" << aName << ":unparsed" << '\n'
					  << std::flush;
			return std::nullopt;
		}
		for (const auto& header : response->Headers) {
			if (header.Name == "x-ratelimit-remaining" || header.Name == "X-RateLimit-Remaining") {
				const auto remaining = std::stoi(header.Value);
				std::cout << "OUTDATED_REPLY_LIVE:rate-remaining:" << remaining << " (" << aName << ")" << '\n'
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
		case review_mutation_operation::observe_pull_request:           return "observe-pull-request";
		case review_mutation_operation::observe_viewer:                 return "observe-viewer";
		case review_mutation_operation::observe_review_thread:          return "observe-thread";
		case review_mutation_operation::observe_review_thread_comments: return "observe-thread-comments";
		case review_mutation_operation::reply_review_thread:            return "reply";
		default:                                                        return "other";
		}
	}

	/// The product's mutation wire carried by `gh api`: the GraphQL root is
	/// `graphql`, a REST path loses its root. With `LoseAnswer`, the reply's
	/// answer is dropped after GitHub received it, as a lost connection would.
	class gh_mutation_transport final : public github_mutation_wire_transport
	{
	public:
		[[nodiscard]] auto perform_review_mutation(const github_mutation_wire_request& aRequest, std::stop_token, github_mutation_transmission_witness aWitness)
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
			// The journal's witness: the attempt is durably possibly sent
			// before anything leaves.
			if (exchange->Witnessed && (!aWitness || !aWitness())) {
				fail("no-durable-witness");
			}
			Operations.push_back(wire.Operation);
			const auto endpoint = exchange->Graphql ? std::string{ "graphql" } : wire.Path.substr(1U);
			const auto method   = wire.Method == http_method::post ? "POST" : wire.Method == http_method::get ? "GET"
																											  : "PATCH";
			const auto response = run_gh(endpoint, method, wire.Headers, wire.Body, operation_name(wire.Operation));
			if (!response) {
				return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable, .Transmission = github_mutation_transmission::indeterminate };
			}
			if (LoseAnswer && wire.Operation == review_mutation_operation::reply_review_thread) {
				std::cout << "OUTDATED_REPLY_LIVE:answer-dropped-after-send" << '\n'
						  << std::flush;
				return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable, .Transmission = github_mutation_transmission::indeterminate };
			}
			return github_mutation_wire_success{ .Response = { .Status = response->Status, .Headers = response->Headers, .Body = response->Body } };
		}

		std::vector<review_mutation_operation> Operations;
		bool                                   LoseAnswer = false;
	};

	class lease final : public application::review_mutation_transport_lease
	{
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

	/// The application host's port over the shipped session.
	class session_port final : public application::review_mutation_port
	{
	public:
		explicit session_port(review_mutation_session& aSession) noexcept
			: Session{ &aSession }
		{
		}
		auto observe_target() -> std::expected<core::review_mutation_target, application::review_mutation_failure> override { return Session->observe_target(); }
		auto observe_target(const std::stop_token aStop) -> std::expected<core::review_mutation_target, application::review_mutation_failure> override { return Session->observe_target(aStop); }
		auto submit(const application::review_mutation_request& aRequest) -> std::expected<application::review_mutation_receipt, application::review_mutation_failure> override { return Session->submit(aRequest); }
		auto submit_and_reconcile(const application::review_mutation_request& aRequest, const std::stop_token aStop) -> application::review_mutation_submission override
		{
			return Session->submit_and_reconcile(aRequest, aStop);
		}
		[[nodiscard]] auto capabilities() const -> application::review_provider_capabilities override { return { }; }

	private:
		review_mutation_session* Session = nullptr;
	};

	class recording_projection final : public application::review_mutation_projection_port
	{
	public:
		void                                               publish(const application::review_mutation_terminal& aTerminal) override { Published.push_back(aTerminal); }
		std::vector<application::review_mutation_terminal> Published;
	};

	/// The reply a reviewer writes in the drawer: bound to the thread.
	[[nodiscard]] auto reply_request(const std::uint64_t aDraft, std::string aBody) -> application::review_mutation_publication_request
	{
		auto comparison  = std::array<std::uint8_t, 32>{ };
		comparison[0]    = 0xA7U;
		comparison[31]   = 0x2EU;
		auto request     = application::review_mutation_publication_request{ };
		request.Proposal = core::review_publication_proposal{
			.Draft         = core::review_draft_id{ aDraft },
			.Anchor        = core::thread_reply_anchor(core::file_comparison_digest{ comparison }),
			.Body          = std::move(aBody),
			.ReplyToThread = text("thread"),
		};
		request.Acknowledged = true;
		request.ThreadReply  = application::review_mutation_thread_reply_binding{ .Path = text("path"), .OriginalCommit = oid(text("originalCommit")) };
		return request;
	}

	void run(const QString& aMode, const std::filesystem::path& aJournal)
	{
		// The core's decision, from GitHub's thread data as the driver read it.
		const auto decision = core::decide_thread_reply({
			.ReviewThread   = true,
			.Outdated       = flag("outdated").value_or(false),
			.AnonymousRead  = false,
			.ViewerCanReply = flag("viewerCanReply"),
			.Locked         = flag("locked"),
		});
		std::cout << "OUTDATED_REPLY_LIVE:decision:" << core::name(decision.Route) << " refusal=" << core::name(decision.Refusal) << " note=" << decision.OutdatedNote << '\n'
				  << std::flush;
		if (decision.Route != core::thread_reply_route::on_thread) {
			fail("not-answered-by-the-thread");
		}
		auto recovery = adapters::sqlite::make_review_mutation_recovery_store({
			.StorageRoot = aJournal,
			.Host        = "github.com",
			.Account     = std::to_string(number("actorId")),
			.Repository  = text("owner") + "/" + text("name"),
			.PullRequest = static_cast<std::int64_t>(number("number")),
		});
		if (!recovery) {
			fail("journal");
		}
		auto wire       = gh_mutation_transport{ };
		wire.LoseAnswer = aMode == QStringLiteral("--lose-answer");
		auto session    = review_mutation_session{ wire, configuration(), std::make_shared<lease>(), std::move(*recovery) };
		auto effects    = application::manual_executor{ };
		auto decisions  = application::manual_executor{ };
		auto projection = recording_projection{ };
		auto host       = application::review_mutation_application_host{
			std::make_unique<session_port>(session),
			target(),
			actor(),
			core::application_session_id{ 32U },
			core::request_generation{ 8U },
			application::make_review_mutation_testing_grant(core::application_session_id{ 32U }, core::request_generation{ 8U }, target(), actor()),
			effects,
			decisions,
			projection,
			nullptr,
		};
		const auto second  = aMode == QStringLiteral("--lose-answer") || aMode == QStringLiteral("--recover");
		const auto request = second ? reply_request(902U, text("secondBody")) : reply_request(901U, text("body"));
		const auto context = host.publish(request);
		if (context.Request.value() == 0U) {
			fail("host-refused");
		}
		effects.drain();
		decisions.drain();
		host.shutdown();
		if (projection.Published.empty()) {
			fail("no-terminal");
		}
		const auto& terminal = projection.Published.back();
		const auto  writes   = static_cast<std::size_t>(std::ranges::count(wire.Operations, review_mutation_operation::reply_review_thread));
		std::cout << "OUTDATED_REPLY_LIVE:" << aMode.mid(2).toStdString() << ":result=" << (terminal.Result ? terminal.Result->ServerCommentId : std::string{ application::name(terminal.Result.error()) })
				  << " duplicate=" << (terminal.Result && terminal.Result->Duplicate) << " attempted=" << terminal.MutationAttempted << " writes=" << writes
				  << " reconciled=" << (terminal.Reconciliation ? terminal.Reconciliation->size() : 0U) << '\n'
				  << std::flush;
		const auto reread = adapters::sqlite::make_review_mutation_recovery_store({
			.StorageRoot = aJournal,
			.Host        = "github.com",
			.Account     = std::to_string(number("actorId")),
			.Repository  = text("owner") + "/" + text("name"),
			.PullRequest = static_cast<std::int64_t>(number("number")),
		});
		const auto key    = application::idempotency_key_for(request.Proposal, target().Semantic);
		const auto stored = reread ? (*reread)->load(key) : std::unexpected(application::review_mutation_recovery_failure::unavailable);
		const auto state  = stored && stored->has_value() ? (**stored).State : application::review_mutation_recovery_state::unresolved;
		std::cout << "OUTDATED_REPLY_LIVE:journal:key=" << key.substr(0U, 12U) << " present=" << (stored && stored->has_value()) << " state=" << static_cast<int>(state)
				  << " threadBound=" << (stored && stored->has_value() && core::thread_bound_anchor((**stored).Anchor.Semantic))
				  << " commit=" << (stored && stored->has_value() ? std::string{ core::canonical_hex((**stored).Anchor.Commit).view() } : std::string{ }) << '\n'
				  << std::flush;
		const auto succeeded = state == application::review_mutation_recovery_state::succeeded && stored && stored->has_value();
		if (aMode == QStringLiteral("--reply") && !(terminal.Result && !terminal.Result->Duplicate && writes == 1U && succeeded)) {
			fail("reply");
		}
		if (aMode == QStringLiteral("--again") && !(terminal.Result && terminal.Result->Duplicate && writes == 0U && succeeded)) {
			fail("again");
		}
		if (aMode == QStringLiteral("--lose-answer") && !(terminal.Result && !terminal.Result->Duplicate && writes == 1U && terminal.MutationAttempted && succeeded)) {
			fail("lose-answer");
		}
		if (aMode == QStringLiteral("--recover") && !(terminal.Result && terminal.Result->Duplicate && writes == 0U && succeeded)) {
			fail("recover");
		}
	}
} // namespace

auto main(int aCount, char** aValues) -> int
{
	QCoreApplication application{ aCount, aValues };
	const auto       arguments = QCoreApplication::arguments();
	if (arguments.size() < 5) {
		std::cerr << "usage: outdated_reply_live_course (--reply|--again|--lose-answer|--recover) CONFIG OUTPUT JOURNAL [FLOOR]\n";
		return EXIT_FAILURE;
	}
	auto file = QFile{ arguments[2] };
	if (!file.open(QIODevice::ReadOnly)) {
		fail("config");
	}
	course().Json   = QJsonDocument::fromJson(file.readAll()).object();
	course().Output = arguments[3];
	course().Floor  = arguments.size() > 5 ? arguments[5].toInt() : 500;
	if (text("owner") != "MikeMarcin" || text("name") != "BPR-Test") {
		fail("not-the-designated-repository");
	}
	const auto journal = std::filesystem::path{ arguments[4].toStdString() };
	std::filesystem::create_directories(journal);
	if (!adapters::sqlite::make_storage_root_owner_private(journal)) {
		fail("journal-root");
	}
	const auto& mode = arguments[1];
	if (mode != QStringLiteral("--reply") && mode != QStringLiteral("--again") && mode != QStringLiteral("--lose-answer") && mode != QStringLiteral("--recover")) {
		fail("mode");
	}
	run(mode, std::filesystem::canonical(journal));
	std::cout << "OUTDATED_REPLY_LIVE:requests:" << Requests << '\n'
			  << std::flush;
	std::cout << "OUTDATED_REPLY_LIVE:OK" << '\n'
			  << std::flush;
	return EXIT_SUCCESS;
}
