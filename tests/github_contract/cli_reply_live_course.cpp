// [[BPR-PENDING-AX]]. The live course of thread replies on the GitHub CLI
// connection: never a CTest row and not built by default
// (BPR_BUILD_LIVE_CLI_REPLY_COURSE).
//
// tests/scripts/cli_reply_live_course.py makes a pull request on
// MikeMarcin/BPR-Test with two review threads by the second identity, one
// on a line a push leaves alone (current) and one on a line it changes
// (outdated). It then runs this tool with a JSON description of them. Unlike
// the BPR-1741 and BPR-1745 courses, nothing here carries bytes by hand: the
// writes go through the app's own gh path, `gh_cli_transport` over the
// shipped process runner, pinned to the signed-in account and acquiring its
// named credential per operation, exactly as the app composes it for a
// GitHub CLI profile. Above it are the shipped session, the application host
// (for the reply bound to the outdated thread) and the SQLite journal. The
// core decides each route from GitHub's thread data, `viewerCanReply` and
// the lock included.
//
// Modes, over one journal directory:
//   --current      reply on the current thread's line; a new receipt
//   --outdated     reply bound to the outdated thread, through the host
//   --again        both again, a new process: answered from the journal
//   --lose-answer  another outdated reply whose answer is dropped after gh
//                  returns it: the reconciliation finds it in the thread
//   --recover      that reply again, a new process: from the journal
//   --decide       the core's decision for viewerCanReply/locked as given
// It prints one `CLI_REPLY_LIVE:` line per step and `X-RateLimit-Remaining`
// per answer, and stops below the floor.
#include "gh_cli_transport.hpp"
#include "github_cli_process_runner.hpp"
#include "github_host_profile.hpp"
#include "github_mutation_contract.hpp"

#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_mutation_application_host.hpp"
#include "bpr/core/thread_reply.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	struct course_configuration
	{
		QJsonObject Json;
		int         Floor = 500;
	};

	[[nodiscard]] auto course() -> course_configuration&
	{
		static auto configuration = course_configuration{ };
		return configuration;
	}

	[[noreturn]] void fail(const std::string_view aWhy)
	{
		std::cout << "CLI_REPLY_LIVE:FAILED:" << aWhy << '\n'
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

	[[nodiscard]] auto operation_name(const review_mutation_operation aOperation) -> std::string_view
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

	struct meter
	{
		std::vector<review_mutation_operation> Operations;
		bool                                   LoseAnswer = false;
	};

	/// Sits above the app's gh path only to count, to read the rate limit
	/// GitHub returns, and in `--lose-answer` to drop the reply's answer after
	/// gh has returned it. Every request is the product's and goes through
	/// `gh_cli_transport` unchanged.
	class metered_transport final : public github_mutation_wire_transport
	{
	public:
		metered_transport(github_mutation_wire_transport& aInner, meter& aMeter, std::shared_ptr<github_mutation_wire_transport> aOwner = { })
			: Inner{ &aInner }
			, Meter{ &aMeter }
			, Owner{ std::move(aOwner) }
		{
		}

		[[nodiscard]] auto begin_mutation_operation(const std::stop_token aStop) -> github_mutation_operation override
		{
			auto operation = Inner->begin_mutation_operation(aStop);
			if (!operation || *operation == nullptr) {
				return operation;
			}
			auto* const scoped = operation->get();
			return std::shared_ptr<github_mutation_wire_transport>{ std::make_shared<metered_transport>(*scoped, *Meter, std::move(*operation)) };
		}

		[[nodiscard]] auto perform_review_mutation(const github_mutation_wire_request& aRequest, const std::stop_token aStop, github_mutation_transmission_witness aWitness)
			-> github_mutation_wire_completion override
		{
			Meter->Operations.push_back(aRequest.Wire.Operation);
			auto              completion = Inner->perform_review_mutation(aRequest, aStop, std::move(aWitness));
			const auto* const success    = std::get_if<github_mutation_wire_success>(&completion);
			std::cout << "CLI_REPLY_LIVE:request:" << operation_name(aRequest.Wire.Operation) << ":" << (success != nullptr ? std::to_string(success->Response.Status) : std::string{ "no-answer" }) << '\n'
					  << std::flush;
			if (success != nullptr) {
				for (const auto& header : success->Response.Headers) {
					auto name = header.Name;
					std::ranges::transform(name, name.begin(), [](const unsigned char aChar) { return static_cast<char>(std::tolower(aChar)); });
					if (name == "x-ratelimit-remaining") {
						const auto remaining = std::stoi(header.Value);
						std::cout << "CLI_REPLY_LIVE:rate-remaining:" << remaining << '\n'
								  << std::flush;
						if (remaining < course().Floor) {
							fail("rate-limit-below-floor");
						}
					}
				}
			}
			if (success != nullptr && Meter->LoseAnswer && aRequest.Wire.Operation == review_mutation_operation::reply_review_thread) {
				Meter->LoseAnswer = false;
				std::cout << "CLI_REPLY_LIVE:answer-dropped-after-send" << '\n'
						  << std::flush;
				return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable, .Transmission = github_mutation_transmission::indeterminate };
			}
			return completion;
		}

	private:
		github_mutation_wire_transport*                 Inner = nullptr;
		meter*                                          Meter = nullptr;
		std::shared_ptr<github_mutation_wire_transport> Owner;
	};

	class lease final : public application::review_mutation_transport_lease
	{
	};

	constexpr auto profile_id = std::uint64_t{ 33U };
	constexpr auto host_id    = std::uint64_t{ 11U };
	constexpr auto revision   = std::uint64_t{ 34U };

	[[nodiscard]] auto target() -> application::review_mutation_target_binding
	{
		return {
			.Semantic = {
				.Host         = core::git_host_id{ host_id },
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
					.Profile         = core::host_profile_id{ profile_id },
					.ProfileRevision = application::github_profile_revision{ revision },
					.Host            = core::git_host_id{ host_id },
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

	/// The app's gh path for the signed-in account: the live gh found the way
	/// the app finds it, pinned to the account, over the shipped runner.
	struct gh_path
	{
		std::optional<validated_github_host_profile>  Profile;
		std::optional<validated_gh_cli_configuration> Configuration;
		std::unique_ptr<github_cli_process_port>      Process;
		std::unique_ptr<gh_cli_transport>             Transport;
	};

	[[nodiscard]] auto open_gh_path() -> gh_path
	{
		auto       path       = gh_path{ };
		const auto executable = QFileInfo{ QStandardPaths::findExecutable(QStringLiteral("gh")) }.canonicalFilePath();
		if (executable.isEmpty()) {
			fail("gh-not-found");
		}
		const auto live = validate_gh_cli_configuration({
			.Executable              = executable.toStdString(),
			.NeutralWorkingDirectory = QDir::tempPath().toStdString(),
			.Platform                = gh_cli_macos_environment_facts{ .HomeDirectory = QDir::homePath().toStdString() },
			.Https                   = { },
		});
		if (!live) {
			fail("gh-configuration");
		}
		const auto pinned = pin_gh_cli_configuration(*live, "github.com", text("actorLogin"));
		if (!pinned) {
			fail("gh-pin");
		}
		path.Configuration.emplace(*pinned);
		const auto profile = validate_github_host_profile({
			.Profile         = core::host_profile_id{ profile_id },
			.Host            = core::git_host_id{ host_id },
			.Revision        = application::github_profile_revision{ revision },
			.Server          = application::github_server_contract::github_com_2022_11_28,
			.Authentication  = application::github_authentication_transport::github_cli,
			.Proxy           = application::github_proxy_mode::system,
			.Trust           = application::github_trust_mode::system,
			.Provenance      = github_profile_provenance::builtin,
			.Hostname        = "github.com",
			.RestRoot        = "https://api.github.com",
			.GraphqlRoot     = "https://api.github.com/graphql",
			.CliAccountLogin = text("actorLogin"),
		});
		if (!profile) {
			fail("profile");
		}
		path.Profile.emplace(*profile);
		path.Process = make_github_cli_process_runner();
		if (!path.Process) {
			fail("runner");
		}
		path.Transport = std::make_unique<gh_cli_transport>(*path.Profile, *path.Configuration, *path.Process);
		return path;
	}

	[[nodiscard]] auto open_journal(const std::filesystem::path& aJournal) -> std::unique_ptr<application::review_mutation_recovery_port>
	{
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
		return std::move(*recovery);
	}

	[[nodiscard]] auto comparison() -> core::file_comparison_digest
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = 0xA7U;
		bytes[31]  = 0x2EU;
		return core::file_comparison_digest{ bytes };
	}

	/// The reply on the current thread's line, as the host hands it down.
	[[nodiscard]] auto current_request() -> application::review_mutation_request
	{
		const auto proposal = core::review_publication_proposal{
			.Draft         = core::review_draft_id{ 801U },
			.Anchor        = { .Comparison = comparison(), .Side = core::diff_side::right, .Line = static_cast<std::uint32_t>(number("currentLine") - 1U) },
			.Body          = text("currentBody"),
			.Disposition   = core::review_disposition::comment,
			.ReplyToThread = text("currentThread"),
		};
		return {
			.Proposal       = proposal,
			.Target         = target().Semantic,
			.ActingIdentity = actor().Account,
			.IdempotencyKey = application::idempotency_key_for(proposal, target().Semantic),
			.LiveBinding    = application::review_mutation_live_binding{
				.Target         = target(),
				.ActingIdentity = actor(),
				.Anchor         = { .Semantic = proposal.Anchor, .Path = text("path"), .Commit = target().Semantic.ExpectedHead },
			},
		};
	}

	/// The reply a reviewer writes in the drawer on the outdated thread.
	[[nodiscard]] auto outdated_request(const std::uint64_t aDraft, std::string aBody) -> application::review_mutation_publication_request
	{
		auto request     = application::review_mutation_publication_request{ };
		request.Proposal = core::review_publication_proposal{
			.Draft         = core::review_draft_id{ aDraft },
			.Anchor        = core::thread_reply_anchor(comparison()),
			.Body          = std::move(aBody),
			.ReplyToThread = text("outdatedThread"),
		};
		request.Acknowledged = true;
		request.ThreadReply  = application::review_mutation_thread_reply_binding{ .Path = text("path"), .OriginalCommit = oid(text("originalCommit")) };
		return request;
	}

	[[nodiscard]] auto decide(const bool aOutdated) -> core::thread_reply_decision
	{
		const auto decision = core::decide_thread_reply({
			.ReviewThread   = true,
			.Outdated       = aOutdated,
			.AnonymousRead  = false,
			.ViewerCanReply = flag(aOutdated ? "outdatedViewerCanReply" : "currentViewerCanReply"),
			.Locked         = flag("locked"),
		});
		std::cout << "CLI_REPLY_LIVE:decision:" << (aOutdated ? "outdated" : "current") << ":" << core::name(decision.Route) << " refusal=" << core::name(decision.Refusal) << '\n'
				  << std::flush;
		return decision;
	}

	void report_journal(const std::filesystem::path& aJournal, const std::string& aKey, const std::string_view aName)
	{
		const auto stored = open_journal(aJournal)->load(aKey);
		const auto record = stored ? *stored : std::optional<application::review_mutation_recovery_record>{ };
		std::cout << "CLI_REPLY_LIVE:journal:" << aName << ":present=" << record.has_value()
				  << " state=" << (record ? static_cast<int>(record->State) : -1)
				  << " threadBound=" << (record && core::thread_bound_anchor(record->Anchor.Semantic))
				  << " receipt=" << (record && record->Receipt ? record->Receipt->ServerCommentId : std::string{ "-" }) << '\n'
				  << std::flush;
		if (!record || record->State != application::review_mutation_recovery_state::succeeded) {
			fail("journal-not-succeeded");
		}
	}

	auto submit_current(const std::filesystem::path& aJournal, const bool aExpectDuplicate) -> void
	{
		if (decide(false).Route != core::thread_reply_route::on_line) {
			fail("current-not-on-line");
		}
		auto       path       = open_gh_path();
		auto       counts     = meter{ };
		auto       metered    = metered_transport{ *path.Transport, counts };
		auto       session    = review_mutation_session{ metered, configuration(), std::make_shared<lease>(), open_journal(aJournal) };
		const auto request    = current_request();
		const auto submission = session.submit_and_reconcile(request, { });
		const auto writes     = std::ranges::count(counts.Operations, review_mutation_operation::reply_review_thread);
		std::cout << "CLI_REPLY_LIVE:current:result=" << (submission.Result ? submission.Result->ServerCommentId : std::string{ application::name(submission.Result.error()) })
				  << " duplicate=" << (submission.Result && submission.Result->Duplicate) << " writes=" << writes << '\n'
				  << std::flush;
		if (!submission.Result || submission.Result->Duplicate != aExpectDuplicate || writes != (aExpectDuplicate ? 0 : 1)) {
			fail("current");
		}
		report_journal(aJournal, request.IdempotencyKey, "current");
	}

	auto publish_outdated(const std::filesystem::path& aJournal, const application::review_mutation_publication_request& aRequest, const std::string_view aName,
						  const bool aExpectDuplicate, const bool aLoseAnswer) -> void
	{
		if (decide(true).Route != core::thread_reply_route::on_thread) {
			fail("outdated-not-on-thread");
		}
		auto path         = open_gh_path();
		auto counts       = meter{ };
		counts.LoseAnswer = aLoseAnswer;
		auto metered      = metered_transport{ *path.Transport, counts };
		auto session      = review_mutation_session{ metered, configuration(), std::make_shared<lease>(), open_journal(aJournal) };
		auto effects      = application::manual_executor{ };
		auto decisions    = application::manual_executor{ };
		auto projection   = recording_projection{ };
		auto host         = application::review_mutation_application_host{
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
		if (host.publish(aRequest).Request.value() == 0U) {
			fail("host-refused");
		}
		effects.drain();
		decisions.drain();
		host.shutdown();
		if (projection.Published.empty()) {
			fail("no-terminal");
		}
		const auto& terminal = projection.Published.back();
		const auto  writes   = std::ranges::count(counts.Operations, review_mutation_operation::reply_review_thread);
		std::cout << "CLI_REPLY_LIVE:" << aName << ":result=" << (terminal.Result ? terminal.Result->ServerCommentId : std::string{ application::name(terminal.Result.error()) })
				  << " duplicate=" << (terminal.Result && terminal.Result->Duplicate) << " attempted=" << terminal.MutationAttempted << " writes=" << writes << '\n'
				  << std::flush;
		if (!terminal.Result || terminal.Result->Duplicate != aExpectDuplicate || writes != (aExpectDuplicate ? 0 : 1)) {
			fail(aName);
		}
		report_journal(aJournal, application::idempotency_key_for(aRequest.Proposal, target().Semantic), aName);
	}
} // namespace

auto main(int aCount, char** aValues) -> int
{
	QCoreApplication application{ aCount, aValues };
	const auto       arguments = QCoreApplication::arguments();
	if (arguments.size() < 4) {
		std::cerr << "usage: cli_reply_live_course (--current|--outdated|--again|--lose-answer|--recover|--decide) CONFIG JOURNAL [FLOOR]\n";
		return EXIT_FAILURE;
	}
	auto file = QFile{ arguments[2] };
	if (!file.open(QIODevice::ReadOnly)) {
		fail("config");
	}
	course().Json  = QJsonDocument::fromJson(file.readAll()).object();
	course().Floor = arguments.size() > 4 ? arguments[4].toInt() : 500;
	if (text("owner") != "MikeMarcin" || text("name") != "BPR-Test") {
		fail("not-the-designated-repository");
	}
	const auto journalPath = std::filesystem::path{ arguments[3].toStdString() };
	std::filesystem::create_directories(journalPath);
	if (!adapters::sqlite::make_storage_root_owner_private(journalPath)) {
		fail("journal-root");
	}
	const auto  journal = std::filesystem::canonical(journalPath);
	const auto& mode    = arguments[1];
	const auto  first   = outdated_request(901U, text("outdatedBody"));
	const auto  second  = outdated_request(902U, text("lostBody"));
	if (mode == QStringLiteral("--current")) {
		submit_current(journal, false);
	} else if (mode == QStringLiteral("--outdated")) {
		publish_outdated(journal, first, "outdated", false, false);
	} else if (mode == QStringLiteral("--again")) {
		submit_current(journal, true);
		publish_outdated(journal, first, "outdated-again", true, false);
	} else if (mode == QStringLiteral("--lose-answer")) {
		publish_outdated(journal, second, "lose-answer", false, true);
	} else if (mode == QStringLiteral("--recover")) {
		publish_outdated(journal, second, "recover", true, false);
	} else if (mode == QStringLiteral("--decide")) {
		const auto current  = decide(false);
		const auto outdated = decide(true);
		std::cout << "CLI_REPLY_LIVE:refused:" << (current.Route == core::thread_reply_route::refused && outdated.Route == core::thread_reply_route::refused) << '\n'
				  << std::flush;
	} else {
		fail("mode");
	}
	std::cout << "CLI_REPLY_LIVE:OK" << '\n'
			  << std::flush;
	return EXIT_SUCCESS;
}
