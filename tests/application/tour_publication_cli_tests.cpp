#include "../../src/adapters/sqlite/src/review_tour_codec.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../../src/app/src/tour_cancellation.hpp"
#include "../../src/app/src/tour_cli.hpp"
#include "../../src/app/src/tour_observation.hpp"
#include "../sqlite_contract/scratch_root.hpp"
#include "../support/review_tour_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"
#include <QCoreApplication>
#include <QJsonArray>
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
		std::shared_ptr<app::detail::review_submission_fixture_state>   Server    = std::make_shared<app::detail::review_submission_fixture_state>();
		bool                                                            Qualified = true, WrongAccount = false, Partial = false, FailRead = false;
		std::function<void(application::github_pull_request_snapshot&)> Observe;
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
			if (State.Observe) {
				State.Observe(snapshot);
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
			return app::tour_publication_context{ live, std::move(factory), core::application_session_id{ 123 }, State.Qualified, [configuration](std::string_view aKey) -> app::tour_cli_result<std::optional<application::staged_review_plan>> {
													 auto store = adapters::sqlite::make_review_submission_recovery_store(configuration);
													 expect(store.has_value(), "existing fixture journal opens");
													 auto record = (*store)->load_staged(std::string{ aKey });
													 expect(record.has_value(), "keyed fixture receipt load");
													 return *record ? std::optional{ (**record).Plan } : std::nullopt;
												 } };
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
	void cold_course(fixture_state& aState, const std::filesystem::path& aRoot)
	{
		auto& server = *aState.Server;
		expect(server.PreviousReviews.size() == 1 && server.PreviousComments.size() == 1 && server.StagedComments.size() == 1, "cold fixture spans two actually published batches");
		const auto root                      = server.PreviousReviews.front().WebUrl;
		const auto first                     = server.PreviousComments.front().WebUrl;
		const auto second                    = server.StagedComments.front().WebUrl;
		server.StagedDescription             = "Template retained.\r\n## Problem\r\nWebsite problem.\r\n## Solution\r\nWebsite solution.\r\n## Checks\r\nPreserved.\r\n";
		server.PreviousComments.front().Body = "Website-maintained **raw** first stop.\r\n";
		const auto compatibility             = "https://github.com/o/r/pull/7#issuecomment-801";
		const auto historical                = "https://github.com/o/r/pull/7#issuecomment-802";
		const auto base                      = std::string{ core::canonical_hex(tests::review_tour_fixture().Base).view() };
		server.CompatibilityComments.push_back({ .Id = 801, .NodeId = "IC_801", .WebUrl = compatibility, .Actor = server.StagedReview->Actor, .Body = "https://github.com/o/r/blob/" + base + "/src/left.cpp#L2-L4\n\nLeft-side explanation." });
		server.CompatibilityComments.push_back({ .Id = 802, .NodeId = "IC_802", .WebUrl = historical, .Actor = server.StagedReview->Actor, .Body = "https://github.com/o/r/blob/3333333333333333333333333333333333333333/src/old.cpp#L1-L2\n\nHistorical explanation." });
		server.PreviousReviews.front().Body     = "Website route\n\n1. [Later batch](" + second + ")\n1. [Original](" + first + ")\n1. [Left](" + compatibility + ")\n1. [Historical](" + historical + ")\n";
		const auto                       writes = server.StagedWrites;
		std::optional<core::review_tour> expected;
		std::string                      token;
		for (const auto name : { "cold-one", "cold-two" }) {
			app::tour_runtime runtime{ aRoot / name / "data", "1", readers(aState) };
			auto              opened = runtime.open("github.com", { "o", "r" }, core::pull_request_number{ 7 }, false, root, false);
			expect(opened && opened->PublishedObservation == "complete", "fresh profile gets complete ordinary GitHub observation");
			const auto& draft = opened->Draft;
			expect(draft.Stops.size() == 4 && draft.Problem == "Website problem.\r\n" && draft.Solution == "Website solution.\r\n", "cold reconstruction preserves website brief and full route");
			expect(draft.Stops[0].Remote->WebUrl == second && draft.Stops[1].Remote->WebUrl == first && draft.Stops[1].Body == server.PreviousComments.front().Body, "current website order and raw narrative override former publication order");
			expect(draft.IndexBaseline->Raw == server.PreviousReviews.front().Body && draft.ProblemBaseline->Raw == *server.StagedDescription, "raw website baselines are exact");
			expect(draft.Stops[0].Destination && draft.Stops[0].Destination->Path == "src/later.cpp" && draft.Stops[1].Destination && draft.Stops[1].Destination->Path == "src/item.cpp", "native subjects from different batches retain original sources");
			expect(draft.Stops[2].Destination && draft.Stops[2].Destination->Side == core::diff_side::left && draft.Stops[2].Destination->FirstLine == 1 && draft.Stops[2].Destination->LastLine == 3, "same desktop scoped parser recovers compatibility left source");
			expect(draft.Stops[3].Destination && core::canonical_hex(draft.Stops[3].Destination->Commit).view() == "3333333333333333333333333333333333333333", "historical pinned commit never replaced by current head");
			expect(!draft.HasPriorObservation && std::ranges::all_of(draft.Stops, [](const auto& stop) { return !stop.Original && !stop.Suggestion && stop.CompletionOrigin == core::tour_completion_origin::unspecified && stop.Preparation != core::tour_preparation::ready && !stop.Destination->Blob; }), "cold observation invents no private source visits readiness provenance or blobs");
			if (expected) {
				expect(*expected == draft, "two fresh roots without shared cache or manifest reconstruct identical published content");
			} else {
				expected = draft;
			}
			token = app::tour_runtime::draft_token(opened->Target);
		}
		{
			std::ostringstream output;
			auto*              before = std::cout.rdbuf(output.rdbuf());
			const auto         code   = app::run_tour_cli({ "tour", "status", "--data-root", QString::fromStdString((aRoot / "cold-one" / "data").string()), "--profile", "1", "--draft", QString::fromStdString(token), "--json" }, readers(aState));
			std::cout.rdbuf(before);
			const auto data = QJsonDocument::fromJson(QByteArray::fromStdString(output.str())).object()["data"].toObject();
			expect(code == 0 && data["published_root"].toObject()["url"].toString() == QString::fromStdString(root) && data["prior_private_context"].toString().startsWith("unknown") && data["stops"].toArray().size() == 4 && data["stops"].toArray()[2].toObject()["source_binding"].toObject()["side"] == "LEFT", "actual CLI JSON exposes remote bindings and unknown private context");
		}
		aState.Partial = true;
		{
			app::tour_runtime runtime{ aRoot / "partial" / "data", "1", readers(aState) };
			auto              opened = runtime.open("github.com", { "o", "r" }, core::pull_request_number{ 7 }, false, root, false);
			expect(opened && opened->Draft.ReadOnly && opened->PublishedObservation == "partial" && opened->Draft.Stops.size() == 4, "partial accessible content stays visible and read-only");
			expect(!runtime.prepare_publication(app::tour_runtime::draft_token(opened->Target), false, { }), "partial read cannot authorize maintenance");
		}
		aState.Observe = [](auto& snapshot) {snapshot.Reviews.clear();snapshot.IssueComments.clear();snapshot.ReviewThreads.clear(); };
		{
			app::tour_runtime runtime{ aRoot / "partial-empty" / "data", "1", readers(aState) };
			auto              opened = runtime.open("github.com", { "o", "r" }, core::pull_request_number{ 7 }, false, { }, false);
			expect(!opened && opened.error().Code == "remote_incomplete", "missing collections never become an empty new tour");
		}
		aState.Partial = false;
		aState.Observe = [](auto& snapshot) { snapshot.ReviewThreads.front().Comments.front().Deleted = true; };
		{
			app::tour_runtime runtime{ aRoot / "deleted-stop" / "data", "1", readers(aState) };
			auto              opened = runtime.open("github.com", { "o", "r" }, core::pull_request_number{ 7 }, false, root, false);
			expect(opened && opened->Draft.ReadOnly && opened->Draft.Stops.size() == 4 && !opened->Draft.Stops[1].Remote && opened->Draft.Stops[1].Body.starts_with("Unresolved"), "deleted inaccessible destination retained as unresolved route entry");
		}
		aState.Observe = [](auto& snapshot) {auto& comment=snapshot.ReviewThreads.front().Comments.front();comment.Observation.Commit.clear();comment.Observation.OriginalCommit.clear(); };
		{
			app::tour_runtime runtime{ aRoot / "unknown-source" / "data", "1", readers(aState) };
			auto              opened = runtime.open("github.com", { "o", "r" }, core::pull_request_number{ 7 }, false, root, false);
			expect(opened && !opened->Draft.Stops[1].Destination && opened->Draft.Stops[1].Body == server.PreviousComments.front().Body, "unknown source preserves accessible prose without fabricated anchor");
		}
		aState.Observe = [](auto& snapshot) { snapshot.Reviews.front().Observation.RawBody.reset(); };
		{
			app::tour_runtime runtime{ aRoot / "missing-body" / "data", "1", readers(aState) };
			auto              opened = runtime.open("github.com", { "o", "r" }, core::pull_request_number{ 7 }, false, { }, false);
			expect(!opened && opened.error().Code == "remote_incomplete", "unavailable raw root body never implies absence");
		}
		aState.Observe = [](auto& snapshot) {auto other=snapshot.Reviews.front();other.DatabaseId=999;other.Review=application::github_review_id{"PRR_999"};other.Observation.DatabaseId=999;other.Observation.WebUrl="https://github.com/o/r/pull/7#pullrequestreview-999";snapshot.Reviews.push_back(other); };
		{
			app::tour_runtime runtime{ aRoot / "multiple" / "data", "1", readers(aState) };
			auto              opened = runtime.open("github.com", { "o", "r" }, core::pull_request_number{ 7 }, false, { }, false);
			expect(!opened && opened.error().Code == "target_ambiguous", "multiple current roots require explicit selection");
			expect(runtime.open("github.com", { "o", "r" }, core::pull_request_number{ 7 }, false, root, false).has_value(), "explicit stable root selects intended story");
		}
		aState.Observe = [](auto& snapshot) { snapshot.Reviews.clear(); };
		{
			app::tour_runtime runtime{ aRoot / "cold-one" / "data", "1", readers(aState) };
			auto              opened = runtime.open("github.com", { "o", "r" }, core::pull_request_number{ 7 }, false, { }, false);
			expect(opened && opened->Draft.RemoteStale && opened->PublishedObservation == "stale" && opened->Draft.Stops == expected->Stops, "missing old index preserves complete retained local story as stale");
		}
		aState.Observe  = { };
		aState.FailRead = true;
		{
			app::tour_runtime runtime{ aRoot / "cold-two" / "data", "1", readers(aState) };
			expect(!runtime.open("github.com", { "o", "r" }, core::pull_request_number{ 7 }, false, { }, false), "provider read failure remains unavailable");
			expect(runtime.load(token, true)->Draft == *expected, "failed read retains exact local content and revisions");
		}
		aState.FailRead = false;
		expect(server.StagedWrites == writes, "all cold/import/status/partial courses perform zero writes");
		fixture_state pending;
		pending.Server->StagedReview        = *server.StagedReview;
		pending.Server->StagedReview->State = application::staged_review_state::pending;
		pending.Server->StagedComments      = server.StagedComments;
		{
			app::tour_runtime runtime{ aRoot / "pending" / "data", "1", readers(pending) };
			auto              opened = runtime.open("github.com", { "o", "r" }, core::pull_request_number{ 7 }, false, { }, false);
			expect(opened && !opened->Draft.Remote && opened->Draft.Stops.empty() && opened->PendingServerReviews == 1 && opened->PublishedObservation == "no_published_root", "cold pending review is a candidate, never a recovered public tour");
			const auto pendingToken = app::tour_runtime::draft_token(opened->Target);
			for (int i = 0; i < 2; ++i) {
				auto recovered = runtime.prepare_publication(pendingToken, false, { }, { }, true);
				expect(recovered && !recovered->Plan && !recovered->Result.Record && recovered->PendingServerReviews == 1, "lost journal and authority are explicitly unrecoverable");
			}
			expect(!runtime.prepare_publication(pendingToken, false, { }, "unknown-attempt", true), "arbitrary attempt name grants no adoption");
		}
		expect(pending.Server->StagedWrites == 0, "pending discovery never adopts edits deletes or submits");
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
			auto receipt = runtime.prepare_publication(token, false, { }, prepared.Plan->Submission.IdempotencyKey, true);
			expect(receipt && receipt->Result.Record && receipt->Result.State == application::staged_publication_state::published && state.Server->StagedWrites == 4, "explicit completed receipt survives local attribution revision and remains read-only");
			auto snapshot = *runtime.load(token, true);
			auto stop     = snapshot.Draft.Stops.front();
			stop.Body     = "An explicit maintained explanation.";
			snapshot      = edit(runtime, snapshot, { .Kind = core::tour_command_kind::edit_stop, .Stop = stop.Id, .Value = stop });
			auto update   = runtime.prepare_publication(token, false, { });
			expect(update && update->Plan && update->Plan->Update, "maintenance uses same immutable parent plan");
			auto updated = runtime.publish(*update, false);
			expect(updated && updated->Result.State == application::staged_publication_state::published && state.Server->StagedCreates == 1 && state.Server->StagedSubmits == 1, "maintenance preserves original remote identities");
			snapshot                  = *runtime.load(token, true);
			auto additional           = tests::review_tour_fixture().Stops.front();
			additional.Original->Path = "src/later.cpp";
			additional.Original->Blob = *core::parse_git_blob_oid("2222222222222222222222222222222222222222");
			additional.Title          = "Second batch";
			snapshot                  = edit(runtime, snapshot, { .Kind = core::tour_command_kind::insert, .Value = additional });
			auto insertion            = runtime.prepare_publication(token, false, { });
			expect(insertion && insertion->Plan && runtime.publish(*insertion, false)->Result.State == application::staged_publication_state::published, "second actual publication batch succeeds before cold recovery");
		}
		cold_course(state, scratch.path());
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
