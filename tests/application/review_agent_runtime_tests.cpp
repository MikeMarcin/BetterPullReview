/// \file
/// \brief RFC-BPR-0126 AC01 ([[BPR-2181]] slice 2). The profile's agent
/// runtime runs a session's turns with no window: a queue with zero windows
/// drains through it, each released message's request is built from the
/// review snapshot the runtime holds, and the turns no window saw are kept
/// for the window that opens next. The runtime lives while its queue can
/// drain or a turn runs, and is let go once neither holds and no window
/// shows it. A window that attaches releases through itself and sends its
/// own turns through the runtime; closing it does not touch a running turn.

#include "bpr/application/review_agent_runtime.hpp"

#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class manual_lane final : public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		void drain()
		{
			while (!Tasks.empty()) {
				auto task = std::move(Tasks.front());
				Tasks.pop_front();
				task();
			}
		}
		std::deque<std::function<void()>> Tasks;
	};

	/// The composed agent machinery: records each request and answers when
	/// told, through the projection the runtime gave it, then retires.
	struct agent_machinery
	{
		std::vector<application::agent_invocation_request> Requests;
		application::agent_invocation_projection_port*     Projection = nullptr;
		std::function<void()>                              Retired;
		bool                                               Running  = false;
		std::size_t                                        Cancels  = 0U;
		bool                                               Shutdown = false;

		void answer(const std::string& aText)
		{
			const auto identity = application::agent_invocation_projection_identity{ };
			Projection->admit(identity);
			Projection->stream(identity, application::agent_stream_event{ .Text = aText });
			Projection->finish(identity, application::agent_run_result{ .Outcome = application::agent_run_outcome::success });
			Running = false;
			Retired();
		}
	};

	class turn_port final : public application::review_agent_turn_port
	{
	public:
		explicit turn_port(agent_machinery& aMachinery)
			: Machinery(aMachinery)
		{ }
		void dispatch(application::agent_invocation_request aRequest) override
		{
			Machinery.Requests.push_back(std::move(aRequest));
			Machinery.Running = true;
		}
		void               cancel() override { ++Machinery.Cancels; }
		[[nodiscard]] auto is_quiescent() const -> bool override { return !Machinery.Running; }
		void               shutdown() override { Machinery.Shutdown = true; }

	private:
		agent_machinery& Machinery;
	};

	/// A window's Agent tab: what it was shown.
	class shown_turns final : public application::agent_invocation_projection_port
	{
	public:
		void offer(std::span<const core::agent_profile>) override { }
		void refuse(std::string_view) override { }
		void admit(const application::agent_invocation_projection_identity&) override { }
		void start(const application::agent_invocation_projection_identity&, const application::prepared_workspace&) override { }
		void stream(const application::agent_invocation_projection_identity&, const application::agent_stream_event& aEvent) override { Text += aEvent.Text; }
		void finish(const application::agent_invocation_projection_identity&, const application::agent_run_result&) override { ++Finished; }
		void present_session_catalog(const application::agent_session_catalog_projection&) override { }

		std::string Text;
		std::size_t Finished = 0U;
	};

	struct profile
	{
		manual_lane                                         Lane;
		application::agent_session_runner                   Runner;
		application::agent_queue_host                       Queue{ { .Runner = &Runner } };
		std::map<std::string, agent_machinery>              Machinery;
		std::unique_ptr<application::review_agent_runtimes> Runtimes;

		profile()
		{
			Runner.bind(Queue);
			Runtimes = std::make_unique<application::review_agent_runtimes>(application::review_agent_runtime_dependencies{
				.Queue        = &Queue,
				.Runner       = &Runner,
				.DecisionLane = &Lane,
				.Compose      = [this](const std::string& aSession, application::agent_invocation_projection_port& aProjection, std::function<void()> aRetired)
					-> std::unique_ptr<application::review_agent_turn_port> {
					auto& machinery      = Machinery[aSession];
					machinery.Projection = &aProjection;
					machinery.Retired    = std::move(aRetired);
					return std::make_unique<turn_port>(machinery);
				},
				.Runs = [](const std::string& aSession) { return aSession.starts_with("agent-tab:"); },
			});
		}
	};

	[[nodiscard]] auto typed(const std::string& aText) -> core::queued_input { return { .Label = aText, .Text = aText }; }

	[[nodiscard]] auto review_snapshot() -> application::review_agent_snapshot
	{
		auto snapshot                                = application::review_agent_snapshot{ };
		snapshot.Request.Profile                     = core::claude_stream_json_profile_id;
		snapshot.Request.NativeScope                 = "agent-tab";
		snapshot.Request.Context.Binding.PullRequest = core::pull_request_id{ 7U };
		return snapshot;
	}

	void test_zero_windows_drain_and_keep_the_transcript()
	{
		auto       app     = profile{ };
		const auto session = std::string{ "agent-tab:github.com/o/r#7" };
		// A window showed the review and handed its snapshot over, then
		// closed, with two messages waiting behind a turn of its own.
		auto* runtime = app.Runtimes->open(session);
		expect(runtime != nullptr, "the profile makes the session's runtime");
		runtime->adopt(review_snapshot());
		auto       window          = shown_turns{ };
		const auto presenter       = runtime->attach({ .Projection = &window, .Release = { } });
		auto       direct          = review_snapshot().Request;
		direct.Context.ChatMessage = "typed in the tab";
		expect(runtime->dispatch(direct), "the Agent tab's own turn goes through the runtime");
		static_cast<void>(app.Queue.enqueue(session, typed("first")));
		static_cast<void>(app.Queue.enqueue(session, typed("second")));
		expect(app.Machinery[session].Requests.size() == 1U, "the queue waits behind the running turn");
		app.Runtimes->detach(session, presenter);
		app.Lane.drain();
		expect(app.Runtimes->find(session) == runtime && app.Machinery[session].Cancels == 0U && !app.Machinery[session].Shutdown,
			   "closing the window does not touch the running turn, and the runtime stays");

		// No window from here on.
		app.Machinery[session].answer("answer zero");
		expect(window.Finished == 0U, "a window that left is shown nothing");
		expect(app.Machinery[session].Requests.size() == 2U && app.Machinery[session].Requests.back().Context.ChatMessage == "first",
			   "the run's retirement releases the first message, with no window");
		expect(app.Machinery[session].Requests.back().Profile == core::claude_stream_json_profile_id && app.Machinery[session].Requests.back().NativeScope == "agent-tab"
				   && app.Machinery[session].Requests.back().Context.Binding.PullRequest == core::pull_request_id{ 7U },
			   "its request is built from the snapshot the runtime holds");
		app.Machinery[session].answer("answer one");
		app.Lane.drain();
		expect(app.Machinery[session].Requests.size() == 3U && app.Machinery[session].Requests.back().Context.ChatMessage == "second", "and then the second");
		app.Machinery[session].answer("answer two");
		expect(app.Queue.queue(session)->Entries.empty() && app.Queue.queue(session)->Paused == core::queue_pause::none, "the queue drained with zero windows");
		const auto& kept = runtime->unshown_turns();
		expect(kept.size() == 3U && kept[0].Asked == "typed in the tab" && kept[0].Answered == "answer zero" && kept[1].Asked == "first" && kept[1].Answered == "answer one"
				   && kept[2].Asked == "second" && kept[2].Answered == "answer two" && kept[2].Outcome == core::agent_turn_outcome::answered,
			   "every turn no window saw end is kept in the session's transcript");

		// With nothing left to do and no window, the runtime is let go.
		app.Lane.drain();
		expect(app.Runtimes->find(session) == nullptr, "an idle runtime no window shows is let go");
	}

	void test_window_releases_and_runtime_stays_while_work_waits()
	{
		auto       app     = profile{ };
		const auto session = std::string{ "agent-tab:github.com/o/r#8" };
		auto*      runtime = app.Runtimes->open(session);
		auto       window  = shown_turns{ };
		auto       asked   = std::vector<std::string>{ };
		// The window releases with its whole context, sending through the
		// runtime.
		const auto presenter = runtime->attach({
			.Projection = &window,
			.Release =
				[&](const core::queued_entry& aEntry) {
					asked.push_back(aEntry.Input.Label);
					auto request                = review_snapshot().Request;
					request.Context.ChatMessage = "expanded: " + aEntry.Input.Label;
					return runtime->dispatch(std::move(request));
				},
		});
		static_cast<void>(app.Queue.enqueue(session, core::queued_input{ .QuickAsk = "what-changed", .Scope = "pull-request", .Label = "What changed?" }));
		expect(asked == std::vector<std::string>{ "What changed?" } && app.Machinery[session].Requests.back().Context.ChatMessage == "expanded: What changed?",
			   "a window that shows the session releases a quick ask with its material");
		app.Machinery[session].answer("it changed");
		expect(window.Text == "it changed" && window.Finished == 1U && runtime->unshown_turns().empty(), "a turn a window showed is presented, not kept as unshown");
		// A quick ask waits for a window once none shows the session: its
		// decided expansion is slice 6's (AC13).
		app.Runtimes->detach(session, presenter);
		app.Lane.drain();
		expect(app.Runtimes->find(session) == nullptr, "no window, no snapshot and no work: let go");
		runtime = app.Runtimes->open(session);
		runtime->adopt(review_snapshot());
		static_cast<void>(app.Queue.enqueue(session, core::queued_input{ .QuickAsk = "what-changed", .Scope = "pull-request", .Label = "What changed?" }));
		app.Lane.drain();
		expect(app.Machinery[session].Requests.size() == 1U && app.Queue.queue(session)->Entries.size() == 1U && app.Queue.queue(session)->Paused == core::queue_pause::none,
			   "a quick ask with no window stays queued, unpaused");
	}

	void test_shutdown_stops_every_runtime()
	{
		auto       app     = profile{ };
		const auto session = std::string{ "agent-tab:github.com/o/r#9" };
		auto*      runtime = app.Runtimes->open(session);
		runtime->adopt(review_snapshot());
		static_cast<void>(app.Queue.enqueue(session, typed("one")));
		expect(!app.Runtimes->is_quiescent(), "a running turn keeps the profile from quiescence");
		app.Runtimes->shutdown();
		expect(app.Machinery[session].Cancels == 1U && app.Machinery[session].Shutdown, "quitting the profile stops its runtimes' turns");
		expect(app.Runtimes->open("agent-tab:github.com/o/r#10") == nullptr && app.Runtimes->open("tour:github.com/o/r#9") == nullptr,
			   "nothing opens while closing, and a tour session is not the registry's");
	}
} // namespace

auto main() -> int
{
	test_zero_windows_drain_and_keep_the_transcript();
	test_window_releases_and_runtime_stays_while_work_waits();
	test_shutdown_stops_every_runtime();
	return EXIT_SUCCESS;
}
