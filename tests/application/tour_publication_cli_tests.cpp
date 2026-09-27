#include "../../src/adapters/sqlite/src/review_tour_codec.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../../src/app/src/tour_cancellation.hpp"
#include "../../src/app/src/tour_cli.hpp"
#include "../sqlite_contract/scratch_root.hpp"
#include "../support/review_tour_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
namespace {
	using namespace bpr;
	using tests::expect;
	auto binding() -> application::review_submission_live_binding
	{
		const auto seed = tests::review_tour_fixture();
		return { .Target         = { .Semantic = { seed.Owner.Host, seed.Owner.Repository, seed.Owner.PullRequest, seed.Head }, .Hostname = application::github_canonical_hostname{ "github.com" }, .Repository = { "o", "r" }, .Number = core::pull_request_number{ 7 }, .RepositoryDatabaseId = 4, .RepositoryNodeId = "R_fixture", .PullRequestDatabaseId = 5, .PullRequestNodeId = "PR_fixture", .BaseRepositoryDatabaseId = 4, .BaseOid = seed.Base, .BaseRef = "main", .HeadRef = "feature", .Open = true },
				 .ActingIdentity = { seed.Owner.Account, 3, "U_fixture", "author" } };
	}
	struct fixture_state
	{
		std::shared_ptr<app::detail::review_submission_fixture_state> Server    = std::make_shared<app::detail::review_submission_fixture_state>();
		bool                                                          Qualified = true, WrongAccount = false, Partial = false, FailRead = false;
	};
	class fixture_reader final : public app::tour_remote_reader
	{
	public:
		fixture_reader(fixture_state& aState, std::filesystem::path aRoot)
			: State{ aState }
			, Root{ std::move(aRoot) }
		{ }
		auto connect(std::string, bool, bool) -> app::tour_cli_result<std::string> override { return std::unexpected(app::tour_cli_error{ "fixture", "No credentials in fixtures", 5 }); }
		auto open(std::string aHost, core::repository_locator aRepository, core::pull_request_number aNumber, bool) -> app::tour_cli_result<app::tour_remote_read> override
		{
			if (State.FailRead) {
				return std::unexpected(app::tour_cli_error{ "read_unavailable", "Incomplete remote read", 7 });
			}
			const auto            seed = tests::review_tour_fixture();
			app::tour_remote_read read{ .Target = { .Owner = seed.Owner, .Draft = seed.Id, .HostProfile = core::host_profile_id{ 1 }, .Hostname = std::move(aHost), .Repository = std::move(aRepository), .Number = aNumber, .Base = seed.Base, .Head = seed.Head } };
			if (State.WrongAccount) {
				read.Target.Owner.Account = core::github_account_id{ 99 };
			}
			auto& snapshot                     = read.Snapshot;
			snapshot.Profile                   = read.Target.HostProfile;
			snapshot.Host                      = seed.Owner.Host;
			snapshot.Account                   = read.Target.Owner.Account;
			snapshot.Viewer                    = { application::github_actor_id{ "U_fixture" }, "author" };
			snapshot.Repository                = { .Repository = seed.Owner.Repository, .Node = application::github_node_id{ "R_fixture" }, .Owner = "o", .Name = "r" };
			snapshot.PullRequest               = { .PullRequest = seed.Owner.PullRequest, .Repository = seed.Owner.Repository, .Number = aNumber, .Node = application::github_node_id{ "PR_fixture" }, .Body = State.Server->StagedDescription.value_or(""), .State = application::github_pull_request_state::open };
			snapshot.ReviewThreadsAvailability = State.Partial ? application::github_collection_availability::unavailable : application::github_collection_availability::available;
			auto reviews                       = State.Server->PreviousReviews;
			if (State.Server->StagedReview) {
				reviews.push_back(*State.Server->StagedReview);
			}
			for (const auto& review : reviews) {
				snapshot.Reviews.push_back({ .DatabaseId = review.Id.value(), .Review = application::github_review_id{ review.NodeId }, .Author = snapshot.Viewer, .State = review.State == application::staged_review_state::pending ? application::github_review_state::pending : application::github_review_state::commented, .Observation = { .DatabaseId = review.Id.value(), .WebUrl = review.WebUrl, .UpdatedAt = "version1", .RawBody = review.Body } });
			}
			auto comments = State.Server->PreviousComments;
			comments.insert(comments.end(), State.Server->StagedComments.begin(), State.Server->StagedComments.end());
			for (const auto& comment : comments) {
				if (!comment.Source) {
					continue;
				}
				application::github_review_comment_record c{ .Comment = application::github_comment_id{ comment.NodeId }, .Author = snapshot.Viewer, .Path = comment.Source->Path, .Line = comment.Source->LastLine + 1, .OriginalLine = comment.Source->LastLine + 1, .Observation = { .DatabaseId = comment.Id, .WebUrl = comment.WebUrl, .UpdatedAt = "version1", .RawBody = comment.Body, .ReviewNode = "PRR_" + std::to_string(comment.Review.value()), .Commit = std::string{ core::canonical_hex(comment.Source->Commit).view() }, .OriginalCommit = std::string{ core::canonical_hex(comment.Source->Commit).view() }, .StartLine = comment.Source->FirstLine + 1, .OriginalStartLine = comment.Source->FirstLine + 1 } };
				snapshot.ReviewThreads.push_back({ .Thread = application::github_thread_id{ "thread" + std::to_string(comment.Id) }, .Subject = application::github_review_thread_subject::line, .DiffSide = application::github_diff_side::right, .Comments = { c } });
			}
			for (const auto& comment : State.Server->CompatibilityComments) {
				snapshot.IssueComments.push_back({ .Comment = application::github_comment_id{ comment.NodeId }, .Author = snapshot.Viewer, .Observation = { .DatabaseId = comment.Id, .WebUrl = comment.WebUrl, .UpdatedAt = "version1", .RawBody = comment.Body } });
			}
			return read;
		}
		auto publication() -> app::tour_cli_result<app::tour_publication_context> override
		{
			const auto                                                     live = binding();
			const adapters::sqlite::review_mutation_recovery_configuration configuration{ .StorageRoot = Root, .Host = "github.com", .Account = "3", .Repository = "o/r", .PullRequest = 7 };
			auto                                                           factory = [configuration, live, server = State.Server]() -> std::expected<std::unique_ptr<application::review_submission_port>, application::review_mutation_failure> {
				auto store = adapters::sqlite::make_review_submission_recovery_store(configuration);
				if (!store) {
					return std::unexpected(application::review_mutation_failure::unavailable);
				}
				return app::detail::make_staged_review_fixture_provider(server, std::move(*store), live, "");
			};
			return app::tour_publication_context{ live, std::move(factory), core::application_session_id{ 123 }, State.Qualified };
		}

	private:
		fixture_state&        State;
		std::filesystem::path Root;
	};
	auto readers(fixture_state& aState) -> app::tour_reader_factory
	{
		return [&aState](auto root, auto) { return std::make_unique<fixture_reader>(aState, std::move(root)); };
	}
	auto edit(app::tour_runtime& aRuntime, const app::tour_local_snapshot& aSnapshot, core::tour_command aCommand) -> app::tour_local_snapshot
	{
		aCommand.ExpectedRevision = aSnapshot.Draft.Revision;
		auto changed              = aRuntime.mutate(app::tour_runtime::draft_token(aSnapshot.Target), { .Command = aCommand });
		expect(changed.has_value(), "fixture local edit accepted");
		return *changed;
	}
	auto seed(app::tour_runtime& aRuntime) -> app::tour_local_snapshot
	{
		auto opened = aRuntime.open("github.com", { "o", "r" }, core::pull_request_number{ 7 }, false, { }, false);
		expect(opened.has_value(), "public open");
		auto snapshot = *opened;
		if (!snapshot.Draft.Problem.empty()) {
			return snapshot;
		}
		snapshot            = edit(aRuntime, snapshot, { .Kind = core::tour_command_kind::problem, .Text = "Explain a precise problem." });
		snapshot            = edit(aRuntime, snapshot, { .Kind = core::tour_command_kind::solution, .Text = "Use a snapshot." });
		auto stop           = tests::review_tour_fixture().Stops.front();
		stop.Body           = "Retain exact source and discussion.";
		stop.Original->Blob = *core::parse_git_blob_oid("2222222222222222222222222222222222222222");
		snapshot            = edit(aRuntime, snapshot, { .Kind = core::tour_command_kind::insert, .Value = stop });
		return snapshot;
	}

	void server_codec(adapters::sqlite::detail::tour_codec& c, app::detail::review_submission_fixture_state& s)
	{
		const auto actor   = [&](auto& a) { c.number(a.DatabaseId); c.text(a.NodeId); c.text(a.Login); };
		const auto review  = [&](auto& r) { c.id(r.Id); c.text(r.NodeId); c.text(r.WebUrl); actor(r.Actor); c.oid(r.Commit); c.text(r.Body); c.enumeration(r.State); };
		const auto comment = [&](auto& v) { c.number(v.Id); c.text(v.NodeId); c.text(v.WebUrl); actor(v.Actor); c.id(v.Review); c.optional(v.Source, [&](auto& d) { c.destination(d); }); c.text(v.Body); c.number(v.Reply); };
		c.optional(s.StagedReview, review);
		c.list(s.PreviousReviews, 100, review);
		c.list(s.StagedComments, 100, comment);
		c.list(s.PreviousComments, 100, comment);
		c.list(s.CompatibilityComments, 100, comment);
		c.optional(s.StagedDescription, [&](auto& d) { c.text(d); });
		c.number(s.StagedWrites);
		c.number(s.StagedCreates);
		c.number(s.StagedSubmits);
	}
	void save_server(const std::filesystem::path& aRoot, app::detail::review_submission_fixture_state& aServer)
	{
		adapters::sqlite::detail::tour_codec codec;
		server_codec(codec, aServer);
		expect(codec.Good, "synthetic server serialization");
		std::ofstream file{ aRoot / "server", std::ios::binary };
		file.write(reinterpret_cast<const char*>(codec.Bytes.data()), static_cast<std::streamsize>(codec.Bytes.size()));
		file.close();
		expect(file.good(), "synthetic server durable before checkpoint");
	}
	void load_server(const std::filesystem::path& aRoot, app::detail::review_submission_fixture_state& aServer)
	{
		if (!std::filesystem::exists(aRoot / "server")) {
			return;
		}
		std::ifstream                        file{ aRoot / "server", std::ios::binary };
		const std::vector<std::uint8_t>      bytes{ std::istreambuf_iterator<char>{ file }, { } };
		adapters::sqlite::detail::tour_codec codec;
		codec.Reading = true;
		codec.Input   = bytes;
		server_codec(codec, aServer);
		expect(codec.Good && codec.Input.empty(), "synthetic server decode");
	}
	void checkpoint(const std::filesystem::path& aRoot)
	{
		{
			std::ofstream marker{ aRoot / "kill-ready" };
			marker << "ready";
		}
		std::this_thread::sleep_for(std::chrono::seconds{ 30 });
		expect(false, "fixture must be killed");
	}
	auto terminal_course(const std::string& aMode, const std::filesystem::path& aRoot) -> int
	{
		fixture_state state;
		load_server(aRoot, *state.Server);
		std::string token, attempt;
		{
			app::tour_runtime runtime{ aRoot / "data", "1", readers(state) };
			const auto        snapshot = seed(runtime);
			token                      = app::tour_runtime::draft_token(snapshot.Target);
			if (aMode == "edit") {
				static_cast<void>(edit(runtime, snapshot, { .Kind = core::tour_command_kind::problem, .Text = "Concurrent terminal edit." }));
				return 0;
			}
			if (aMode == "read") {
				return 0;
			}
			const auto prepared = runtime.prepare_publication(token, false, { .Description = true });
			if (prepared && prepared->Plan) {
				attempt = prepared->Plan->Submission.IdempotencyKey;
			}
		}
		if (aMode == "unqualified") {
			state.Qualified = false;
		}
		state.Server->Boundary = [&](std::size_t step, std::string_view boundary) {
			if (boundary == "applied") {
				save_server(aRoot, *state.Server);
			}
			if (aMode == "kill-" + std::to_string(step) + "-" + std::string{ boundary }) {
				checkpoint(aRoot);
			}
		};
		QStringList args{ "tour", aMode == "recover" ? "recover" : "publish", "--data-root", QString::fromStdString((aRoot / "data").string()), "--profile", "1", "--draft", QString::fromStdString(token), "--description" };
		if (aMode == "recover") {
			args.append("--json");
		}
		if (aMode == "resume") {
			args.append({ "--resume", QString::fromStdString(attempt) });
		}
		const auto code = app::run_tour_cli(args, readers(state));
		save_server(aRoot, *state.Server);
		std::cout << "writes=" << state.Server->StagedWrites << " creates=" << state.Server->StagedCreates << " submits=" << state.Server->StagedSubmits << '\n';
		if (aMode == "acknowledged") {
			checkpoint(aRoot);
		}
		return code;
	}
	void course()
	{
		tests::sqlite_contract::scratch_root scratch{ "tour-publish" };
		fixture_state                        state;
		std::string                          token;
		app::tour_publication_preview        prepared;
		{
			app::tour_runtime runtime{ scratch.path() / "data", "1", readers(state) };
			auto              snapshot = seed(runtime);
			token                      = app::tour_runtime::draft_token(snapshot.Target);
			auto preview               = runtime.prepare_publication(token, false, { .Description = true });
			if (preview && !preview->Plan) {
				std::cerr << preview->Status << '\n';
			}
			expect(preview && preview->Plan && state.Server->StagedWrites == 0, "preview constructs shared finite plan with zero writes");
			prepared = *preview;
			snapshot = edit(runtime, snapshot, { .Kind = core::tour_command_kind::problem, .Text = "Changed after preview." });
			expect(!runtime.publish(prepared, false) && state.Server->StagedWrites == 0, "stale confirmation refuses");
			state.WrongAccount = true;
			expect(!runtime.prepare_publication(token, false, { }) && state.Server->StagedWrites == 0, "wrong current actor refuses");
			state.WrongAccount = false;
			state.Qualified    = false;
			expect(!runtime.prepare_publication(token, false, { })->Plan && state.Server->StagedWrites == 0, "unqualified provider cannot construct authority");
			state.Qualified = true;
			prepared        = *runtime.prepare_publication(token, false, { .Description = true });
		}
		// Independent owners reobserve the same exact plan. No grant is serialized.
		{
			app::tour_runtime runtime{ scratch.path() / "data", "1", readers(state) };
			auto              published = runtime.publish(prepared, false);
			if (!published) {
				std::cerr << published.error().Message << '\n';
			} else {
				std::cerr << published->Status << '\n';
			}
			expect(published && published->Result.State == application::staged_publication_state::published, "public runtime stages patches submits and attributes");
			expect(state.Server->StagedCreates == 1 && state.Server->StagedSubmits == 1 && state.Server->StagedWrites == 4, "one create one patch one submit one description");
			auto recovered = runtime.prepare_publication(token, false, { }, { }, true);
			expect(recovered && state.Server->StagedWrites == 4, "recovery is read-only after completion");
			auto snapshot = *runtime.load(token, true);
			auto stop     = snapshot.Draft.Stops.front();
			stop.Body     = "An explicit maintained explanation.";
			snapshot      = edit(runtime, snapshot, { .Kind = core::tour_command_kind::edit_stop, .Stop = stop.Id, .Value = stop });
			auto update   = runtime.prepare_publication(token, false, { });
			expect(update && update->Plan && update->Plan->Update, "maintenance uses same immutable parent plan");
			auto updated = runtime.publish(*update, false);
			expect(updated && updated->Result.State == application::staged_publication_state::published && state.Server->StagedCreates == 1 && state.Server->StagedSubmits == 1, "maintenance preserves original remote identities");
		}
		QStringList args{ "tour", "publish", "--data-root", QString::fromStdString((scratch.path() / "data").string()), "--profile", "1", "--draft", QString::fromStdString(token), "--json" };
		const auto  writes = state.Server->StagedWrites;
		expect(app::run_tour_cli(args, readers(state)) == 3 && writes == state.Server->StagedWrites, "JSON cannot authorize publication");
		args.append("--yes");
		expect(app::run_tour_cli(args, readers(state)) == 2 && writes == state.Server->StagedWrites, "no unscoped bypass");
	}
}
int main(int argc, char** argv)
{
	QCoreApplication app{ argc, argv };
	if (argc == 4 && std::string_view{ argv[1] } == "terminal") {
		return terminal_course(argv[2], argv[3]);
	}
	course();
	std::cout << "tour-publication-cli:pass\n";
}
