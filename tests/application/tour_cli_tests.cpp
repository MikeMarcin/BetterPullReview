#include "../../src/app/src/tour_cli.hpp"
#include "../../src/app/src/tour_owner_service.hpp"
#include "../sqlite_contract/scratch_root.hpp"
#include "../support/review_tour_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <span>
#include <sstream>
#include <thread>
namespace {
	using namespace bpr;
	using tests::expect;
	struct fixture_state
	{
		int           Opens   = 0;
		std::uint64_t Account = 18446744073709551615ULL;
		bool          Roots = false, Multiple = false;
	};
	class fixture_reader final : public app::tour_remote_reader
	{
		fixture_state& State;

	public:
		explicit fixture_reader(fixture_state& aState)
			: State{ aState }
		{ }
		auto source(const core::tour_code_destination& aDestination) -> app::tour_cli_result<app::tour_source_material> override
		{
			const auto comparison = tests::review_tour_fixture().Stops[0].Original->First.Comparison;
			return app::tour_source_material{ { { comparison, aDestination.Side, aDestination.FirstLine }, { comparison, aDestination.Side, aDestination.LastLine }, aDestination.Commit, { }, aDestination.Path }, "fixture source\n" };
		}
		auto connect(std::string, bool, bool) -> app::tour_cli_result<std::string> override { return std::unexpected(app::tour_cli_error{ "fixture_no_auth", "Authentication is not available in this fixture.", 3 }); }
		auto open(std::string aHost, core::repository_locator aRepo, core::pull_request_number aNumber, bool) -> app::tour_cli_result<app::tour_remote_read> override
		{
			++State.Opens;
			const auto            seed = tests::review_tour_fixture();
			app::tour_remote_read result;
			result.Target               = { .Owner = seed.Owner, .Draft = seed.Id, .HostProfile = core::host_profile_id{ 1 }, .Hostname = std::move(aHost), .Repository = std::move(aRepo), .Number = aNumber, .Base = seed.Base, .Head = seed.Head };
			result.Target.Owner.Account = core::github_account_id{ State.Account };
			auto& s                     = result.Snapshot;
			s.Profile                   = result.Target.HostProfile;
			s.Host                      = seed.Owner.Host;
			s.Account                   = result.Target.Owner.Account;
			s.Viewer                    = { application::github_actor_id{ "actor" }, "actor" };
			s.Repository                = { .Repository = seed.Owner.Repository, .Node = application::github_node_id{ "repo" }, .Owner = result.Target.Repository.Owner, .Name = result.Target.Repository.Name };
			s.PullRequest               = { .PullRequest = seed.Owner.PullRequest, .Repository = seed.Owner.Repository, .Number = aNumber, .Node = application::github_node_id{ "pull" }, .Body = "## Problem\r\nProblem **raw**\r\n## Solution\r\nSolution\r\n" };
			s.ReviewThreadsAvailability = application::github_collection_availability::available;
			if (State.Roots) {
				const auto prefix = "https://" + result.Target.Hostname + "/" + result.Target.Repository.Owner + "/" + result.Target.Repository.Name + "/pull/" + std::to_string(aNumber.value());
				s.IssueComments.push_back({ .Comment = application::github_comment_id{ "comment8" }, .Author = s.Viewer, .Observation = { .DatabaseId = 8, .WebUrl = prefix + "#issuecomment-8", .UpdatedAt = "v1", .RawBody = "raw\x1b[2J body\r\n" } });
				for (int i = 0; i < (State.Multiple ? 2 : 1); ++i) {
					s.Reviews.push_back({ .DatabaseId = static_cast<std::uint64_t>(9 + i), .Review = application::github_review_id{ "review" + std::to_string(9 + i) }, .Author = s.Viewer, .State = application::github_review_state::commented, .Observation = { .DatabaseId = static_cast<std::uint64_t>(9 + i), .WebUrl = prefix + "#pullrequestreview-" + std::to_string(9 + i), .UpdatedAt = "v1", .RawBody = "1. [First](" + prefix + "#issuecomment-8)\n" } });
				}
			}
			return result;
		}
	};
	struct reply
	{
		int         Exit;
		std::string Out, Err;
		auto        json() const -> QJsonObject { return QJsonDocument::fromJson(QByteArray::fromStdString(Out)).object(); }
	};
	auto run(QStringList aArgs, fixture_state& aState) -> reply
	{
		std::ostringstream output, error;
		auto*              priorOut = std::cout.rdbuf(output.rdbuf());
		auto*              priorErr = std::cerr.rdbuf(error.rdbuf());
		auto               status   = app::run_tour_cli(aArgs, [&](auto, auto) { return std::make_unique<fixture_reader>(aState); });
		std::cout.rdbuf(priorOut);
		std::cerr.rdbuf(priorErr);
		return { status, output.str(), error.str() };
	}
	void cold_open()
	{
		tests::sqlite_contract::scratch_root scratch{ "cli-public" };
		const auto                           root = QString::fromStdString((scratch.path() / "profile data").string());
		fixture_state                        state;
		auto                                 call  = [&](QStringList args) {args.append({"--data-root",root});return run(args,state); };
		auto                                 first = call({ "tour", "open", "https://github.com/o/r/pull/7", "--json" });
		if (first.Exit != 0) {
			std::cerr << first.Out << first.Err;
		}
		expect(first.Exit == 0, "cold public CLI open initializes private profile");
		const auto data = first.json()["data"].toObject();
		expect(data["account_id"] == "18446744073709551615", "all uint64 IDs are lossless decimal strings");
		const auto id = data["draft_id"].toString();
		expect(id.size() == 64 && first.Err.empty(), "machine open returns opaque exact target and no prompt");
		auto status = call({ "tour", "status", "--profile", "1", "--draft", id, "--json" });
		expect(status.Exit == 0 && state.Opens == 1, "local reads perform no provider calls");
		expect(call({ "tour", "status", "--json" }).json()["error"].toObject()["code"] == "target_ambiguous", "machine context never follows an implicit active draft");
		expect(call({ "tour", "status", "--profile", "1" }).Exit == 2, "piped human text also requires an exact machine draft");
		expect(call({ "tour", "open", "7", "--json" }).json()["error"].toObject()["code"] == "target_ambiguous", "number outside explicit repository is refused");
		auto offline = call({ "tour", "open", "https://github.com/o/r/pull/7", "--offline", "--json" });
		expect(offline.Exit == 0 && state.Opens == 1 && !offline.json()["warnings"].toArray().isEmpty(), "offline exact lookup is visibly stale");
		expect(call({ "tour", "capabilities", "--host", "github.com", "--json" }).Exit == 2 && state.Opens == 1, "unsupported command option refuses before effects");
		{
			app::tour_runtime owner{ scratch.path() / "profile data", "1" };
			expect(owner.admission().has_value(), "first owner lease admitted");
			auto busy = call({ "tour", "status", "--profile", "1", "--draft", id, "--json" });
			expect(busy.Exit == 4 && busy.json()["error"].toObject()["code"] == "owner_protocol_unsupported", "simultaneous caller with no owner tour channel cannot become another writer");
		}
		state.Account = 42;
		auto second   = call({ "tour", "open", "https://github.com/o/r/pull/7", "--json" });
		expect(second.Exit == 0 && second.json()["data"].toObject()["draft_id"] != id, "different account cannot alias draft");
		expect(call({ "tour", "open", "https://github.com/o/r/pull/7", "--offline", "--json" }).Exit == 2, "offline account ambiguity never picks newest");
		expect(call({ "tour", "status", "--profile", "1", "--draft", id, "--json" }).json()["data"].toObject()["account_id"] == "18446744073709551615", "another session cannot redirect explicit first draft");
		state.Account  = 43;
		state.Roots    = true;
		state.Multiple = true;
		expect(call({ "tour", "open", "https://github.com/o/r/pull/7", "--json" }).Exit == 2, "multiple roots require choice");
		auto imported = call({ "tour", "open", "https://github.com/o/r/pull/7", "--root", "9", "--json" });
		expect(imported.Exit == 0, "exact root imports through parent service");
		const auto importedId = imported.json()["data"].toObject()["draft_id"].toString();
		auto       shown      = call({ "tour", "show", "problem", "--profile", "1", "--draft", importedId, "--json" });
		expect(shown.Exit == 0 && shown.json()["data"].toObject()["text"] == "Problem **raw**\r\n", "cold importer retains exact Markdown section");
		auto raw = call({ "tour", "show", "stop:1", "--profile", "1", "--draft", importedId, "--json" });
		expect(raw.json()["data"].toObject()["text"] == "raw\x1b[2J body\r\n", "JSON preserves inert raw body bytes");
		auto human = call({ "tour", "show", "stop:1", "--profile", "1", "--draft", importedId });
		expect(human.Exit == 0 && human.Out.find('\x1b') == std::string::npos && human.Out.find("\\u001b[2J") != std::string::npos, "human output neutralizes terminal controls");
		const auto missingSource = call({ "tour", "show", "stop:1", "--source", "--profile", "1", "--draft", importedId, "--json" });
		expect(missingSource.Exit == 0 && missingSource.json()["data"].toObject()["source_availability"] == "unavailable" && !missingSource.json()["warnings"].toArray().isEmpty(), "missing pinned source remains explicit beside the current comment");
		state.Account       = 50;
		state.Roots         = false;
		state.Multiple      = false;
		auto       draft    = call({ "tour", "open", "https://github.com/o/r/pull/7", "--json" });
		const auto editId   = draft.json()["data"].toObject()["draft_id"].toString();
		auto       rev      = draft.json()["data"].toObject()["revision"].toString();
		const auto file     = scratch.path() / "input with spaces.md";
		auto       write    = [&](std::string_view bytes) {std::ofstream out{file,std::ios::binary};out.write(bytes.data(),static_cast<std::streamsize>(bytes.size())); };
		const auto fileName = QString::fromStdString(file.string());
		auto       edit     = [&](QStringList args, bool expected = true) {args.append({"--profile","1","--draft",editId,"--json"});if(expected){args.append({"--expected-revision",rev});}return call(args); };
		write("Precise α\r\n\r\n");
		expect(edit({ "tour", "edit", "problem", "--file", fileName }, false).json()["error"].toObject()["code"] == "revision_required", "machine edit requires captured revision");
		auto edited = edit({ "tour", "edit", "problem", "--file", fileName });
		expect(edited.Exit == 0, "exact file import commits through shared host");
		expect(edit({ "tour", "edit", "solution", "--file", fileName }).Exit == 4, "old document revision cannot overwrite another field after concurrent edit");
		rev          = edited.json()["data"].toObject()["revision"].toString();
		auto problem = call({ "tour", "show", "problem", "--profile", "1", "--draft", editId, "--json" });
		expect(problem.json()["data"].toObject()["text"] == QString::fromUtf8("Precise α\r\n\r\n"), "raw UTF-8 and CRLF are preserved");
		write(std::string(65537, 'x'));
		expect(edit({ "tour", "edit", "solution", "--file", fileName }).json()["error"].toObject()["code"] == "input_too_large", "oversized input is rejected");
		write(std::string{ "\xc0\xaf", 2 });
		expect(edit({ "tour", "edit", "solution", "--file", fileName }).json()["error"].toObject()["code"] == "invalid_utf8", "invalid Unicode is rejected");
		write("Comment body\n");
		auto added = edit({ "tour", "add", "src/α.cpp:2-3", "--file", fileName });
		expect(added.Exit == 0, "manual source add commits local stop");
		rev = added.json()["data"].toObject()["revision"].toString();
		expect(!added.json()["data"].toObject()["stops"].toArray()[0].toObject()["ready"].toBool(), "having enough text to be ready is not a readiness decision");
		auto ready = edit({ "tour", "ready", "1" });
		expect(ready.Exit == 0 && ready.json()["data"].toObject()["stops"].toArray()[0].toObject()["ready"].toBool(), "explicit readiness is local only");
		rev             = ready.json()["data"].toObject()["revision"].toString();
		auto secondStop = edit({ "tour", "add", "--path", "src/second.cpp", "--lines", "1-1", "--side", "LEFT", "--file", fileName });
		expect(secondStop.Exit == 0, "explicit path and one-based left lines accepted");
		rev        = secondStop.json()["data"].toObject()["revision"].toString();
		auto moved = edit({ "tour", "move", "2", "--before", "1" });
		expect(moved.Exit == 0 && moved.json()["data"].toObject()["stops"].toArray()[0].toObject()["stop_id"] == "2", "move follows stable IDs");
		rev          = moved.json()["data"].toObject()["revision"].toString();
		auto removed = edit({ "tour", "remove", "1" });
		expect(removed.Exit == 0 && removed.json()["data"].toObject()["stops"].toArray().size() == 1, "remove only changes local route");
		expect(edit({ "tour", "move", "2", "--after", "1", "--before", "1" }).Exit != 0, "invalid extra move arguments cannot execute");
	}
	/// \brief The owner side of the channel on its own thread, as the desktop's effect lane serves it.
	class owner_thread
	{
	public:
		enum class fault : int { none,
								 drop_reply };
		explicit owner_thread(app::tour_runtime::owner_binding aBinding)
			: Thread{ [this, binding = std::move(aBinding)] {
				auto server = adapters::sqlite::make_tour_owner_server(binding.Root, binding.Profile, binding.Lease);
				expect(server.has_value(), "the owner binds its tour endpoint");
				const app::tour_owner_service         service{ binding.Root, binding.Profile };
				const application::tour_owner_handler handler = [&](std::span<const std::uint8_t> aPayload) {
					auto answer = service.serve(aPayload);
					// A committed request whose answer never leaves the owner: the lost-response case.
					const auto request = adapters::sqlite::decode_tour_request(aPayload);
					return Fault.load() == static_cast<int>(fault::drop_reply) && request && request->Operation == application::tour_owner_operation::mutate ? application::tour_channel_bytes{ } : answer;
				};
				Ready = true;
				while (!Stop) {
					if (!application::serve_tour_owner_once(**server, handler)) {
						std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
					}
				}
			} }
		{
			while (!Ready) {
				std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
			}
		}
		~owner_thread()
		{
			Stop = true;
			Thread.join();
		}
		void set(fault aFault) { Fault = static_cast<int>(aFault); }

	private:
		std::atomic<bool> Stop{ false }, Ready{ false };
		std::atomic<int>  Fault{ 0 };
		std::thread       Thread;
	};
	void forwarded_owner()
	{
		tests::sqlite_contract::scratch_root scratch{ "cli-forward" };
		const auto                           dataRoot = scratch.path() / "profile data";
		const auto                           root     = QString::fromStdString(dataRoot.string());
		fixture_state                        state;
		auto                                 call   = [&](QStringList args) {args.append({"--data-root",root});return run(args,state); };
		auto                                 opened = call({ "tour", "open", "https://github.com/o/r/pull/7", "--json" });
		expect(opened.Exit == 0, "open the draft before any owner exists");
		const auto id            = opened.json()["data"].toObject()["draft_id"].toString();
		auto       rev           = opened.json()["data"].toObject()["revision"].toString();
		const auto file          = scratch.path() / "forwarded input.md";
		auto       write         = [&](std::string_view bytes) {std::ofstream out{file,std::ios::binary};out.write(bytes.data(),static_cast<std::streamsize>(bytes.size())); };
		const auto fileName      = QString::fromStdString(file.string());
		auto       edit          = [&](const QString& revision, const QString& requestId) { return call({ "tour", "edit", "problem", "--file", fileName, "--profile", "1", "--draft", id, "--json", "--expected-revision", revision, "--request-id", requestId }); };
		auto       status        = [&]() { return call({ "tour", "status", "--profile", "1", "--draft", id, "--json" }); };
		auto       requestStatus = [&](const QString& requestId) { return call({ "tour", "status", "--profile", "1", "--draft", id, "--json", "--request-id", requestId }); };
		auto       revision      = [](const reply& aReply) { return aReply.json()["data"].toObject()["revision"].toString(); };
		auto       problem       = [&]() { return call({ "tour", "show", "problem", "--profile", "1", "--draft", id, "--json" }).json()["data"].toObject()["text"].toString(); };
		// Standalone: no owner, so this process holds the lease for one short command.
		write("Standalone \xce\xb1\n");
		auto standalone = edit(rev, "standalone-1");
		expect(standalone.Exit == 0, "a standalone edit commits");
		const auto standaloneRevision = revision(standalone);
		expect(standaloneRevision != rev, "the edit advanced the revision");
		auto replayed = edit(rev, "standalone-1");
		expect(replayed.Exit == 0 && revision(replayed) == standaloneRevision, "the same request ID and payload returns the stored outcome, not a second edit or a conflict");
		write("Another payload\n");
		auto clashed = edit(rev, "standalone-1");
		expect(clashed.Exit != 0 && clashed.json()["error"].toObject()["code"] == "command_refused", "the same request ID with another payload refuses");
		expect(revision(status()) == standaloneRevision && problem() == QString::fromUtf8("Standalone \xce\xb1\n"), "neither the replay nor the clash changed the draft");
		rev = standaloneRevision;
		{
			app::tour_runtime holder{ dataRoot, "1" };
			const auto        binding = holder.owner();
			expect(binding.has_value(), "the holder owns the profile lease");
			expect(status().json()["error"].toObject()["code"] == "owner_protocol_unsupported", "an owner with no tour channel is reported, not bypassed");
			write("Never written\n");
			expect(edit(rev, "no-channel-1").json()["error"].toObject()["code"] == "owner_protocol_unsupported", "no second writer opens the store beside an owner");
			owner_thread serving{ *binding };
			auto         forwardedStatus = status();
			expect(forwardedStatus.Exit == 0 && revision(forwardedStatus) == rev && forwardedStatus.json()["data"].toObject()["draft_id"] == id, "a read is forwarded to the owner and names the same draft");
			expect(problem() == QString::fromUtf8("Standalone \xce\xb1\n"), "the owner returns the exact committed text");
			write("Forwarded \xce\xb2\r\n");
			auto forwarded = edit(rev, "forward-1");
			expect(forwarded.Exit == 0, "an edit is forwarded to the owner and commits");
			const auto forwardedRevision = revision(forwarded);
			expect(forwardedRevision != rev, "the owner advanced the revision");
			expect(forwarded.json()["data"].toObject()["operation_recorded"].toBool(), "the answer carries the durable request record");
			auto duplicate = edit(rev, "forward-1");
			expect(duplicate.Exit == 0 && revision(duplicate) == forwardedRevision, "a duplicate forwarded request returns the stored outcome");
			write("Different forwarded\n");
			auto clash = edit(rev, "forward-1");
			expect(clash.Exit != 0 && clash.json()["error"].toObject()["code"] == "command_refused", "a forwarded request ID reused with another payload refuses");
			auto recorded = requestStatus("forward-1");
			expect(recorded.Exit == 0 && recorded.json()["data"].toObject()["operation_recorded"].toBool() && recorded.json()["data"].toObject()["operation_revision"] == forwardedRevision, "the owner reports the exact request outcome by ID");
			expect(!requestStatus("never-sent").json()["data"].toObject()["operation_recorded"].toBool(), "an unknown request ID is reported as unrecorded");
			expect(problem() == QString::fromUtf8("Forwarded \xce\xb2\r\n") && revision(status()) == forwardedRevision, "the owner holds exactly one forwarded edit");
			rev = forwardedRevision;
			expect(call({ "tour", "open", "https://github.com/o/r/pull/7", "--json" }).json()["error"].toObject()["code"] == "interaction_required", "provider observation is not a forwarded command");
			// The owner commits and the answer is lost on the way back.
			serving.set(owner_thread::fault::drop_reply);
			write("Committed, reply lost\n");
			auto lost = edit(rev, "lost-1");
			expect(lost.Exit == 6 && lost.json()["error"].toObject()["code"] == "outcome_unknown", "a lost answer is uncertain and is never replayed by the client");
			serving.set(owner_thread::fault::none);
			auto after = requestStatus("lost-1");
			expect(after.json()["data"].toObject()["operation_recorded"].toBool(), "the exact request ID proves the owner committed before the reply was lost");
			const auto lostRevision = after.json()["data"].toObject()["operation_revision"].toString();
			expect(lostRevision != rev && revision(status()) == lostRevision, "the committed edit is visible exactly once");
			auto retried = edit(rev, "lost-1");
			expect(retried.Exit == 0 && revision(retried) == lostRevision, "the retry under the same ID returns the stored outcome without a second effect");
			expect(problem() == QString::fromUtf8("Committed, reply lost\n") && revision(status()) == lostRevision, "no duplicate edit followed the lost reply");
			rev = lostRevision;
		}
		// The owner is gone: the draft it committed is durable and readable standalone.
		expect(problem() == QString::fromUtf8("Committed, reply lost\n") && revision(status()) == rev, "the next process reads exactly what the owner committed");
		auto afterOwner = requestStatus("forward-1");
		expect(afterOwner.json()["data"].toObject()["operation_recorded"].toBool(), "the request record outlives the owner");
	}
}
/// \brief A separate owner process for crash courses: `--serve-owner DATA_ROOT PROFILE [--stall-after-commit]`.
/// It holds the profile lease and serves the tour endpoint until it is killed. With the stall flag
/// it commits a forwarded mutation and then never answers, so a course can kill it at that instant.
int serve_owner_process(const QStringList& aArguments)
{
	if (aArguments.size() < 3) {
		return 2;
	}
	app::tour_runtime holder{ std::filesystem::path{ aArguments[1].toStdString() }, aArguments[2].toStdString() };
	const auto        binding = holder.owner();
	if (!binding) {
		return 3;
	}
	auto server = adapters::sqlite::make_tour_owner_server(binding->Root, binding->Profile, binding->Lease);
	if (!server) {
		return 4;
	}
	const bool                            stall = aArguments.contains("--stall-after-commit");
	const app::tour_owner_service         service{ binding->Root, binding->Profile };
	const application::tour_owner_handler handler = [&](std::span<const std::uint8_t> aPayload) {
		auto       answer  = service.serve(aPayload);
		const auto request = adapters::sqlite::decode_tour_request(aPayload);
		if (stall && request && request->Operation == application::tour_owner_operation::mutate) {
			std::cout << "committed\n"
					  << std::flush;
			std::this_thread::sleep_for(std::chrono::hours{ 1 });
		}
		return answer;
	};
	std::cout << "ready\n"
			  << std::flush;
	for (;;) {
		if (!application::serve_tour_owner_once(**server, handler)) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
	}
}
int main(int argc, char** argv)
{
	QCoreApplication app{ argc, argv };
	if (argc > 1 && std::string_view{ argv[1] } == "--serve-owner") {
		return serve_owner_process(QCoreApplication::arguments().mid(1));
	}
	if (argc > 1) {
		fixture_state state;
		return app::run_tour_cli(QCoreApplication::arguments().mid(1), [&](auto, auto) { return std::make_unique<fixture_reader>(state); });
	}
	cold_open();
	forwarded_owner();
}
