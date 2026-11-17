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

#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
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
		bool                                               Running    = false;
		std::size_t                                        Cancels    = 0U;
		bool                                               Shutdown   = false;
		bool                                               Authorized = true;
		bool                                               Accepting  = false;

		void answer(const std::string& aText, const application::agent_run_outcome aOutcome = application::agent_run_outcome::success)
		{
			const auto identity = application::agent_invocation_projection_identity{ };
			Projection->admit(identity);
			Projection->stream(identity, application::agent_stream_event{ .Text = aText });
			Projection->finish(identity, application::agent_run_result{ .Outcome = aOutcome });
			Running = false;
			Retired();
		}
		/// [[BPR-2243]]. The turn's answer and finish, its run still going
		/// (its process and folder not yet gone); `retire` ends it.
		void finish_only(const std::string& aText)
		{
			const auto identity = application::agent_invocation_projection_identity{ };
			Projection->admit(identity);
			Projection->stream(identity, application::agent_stream_event{ .Text = aText });
			Projection->finish(identity, application::agent_run_result{ .Outcome = application::agent_run_outcome::success });
		}
		void retire()
		{
			Running = false;
			Retired();
		}
		/// The machinery refused the turn before it started: no finish.
		void refuse()
		{
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
		[[nodiscard]] auto authorized() const -> bool override { return Machinery.Authorized; }
		[[nodiscard]] auto accepting() const -> bool override { return Machinery.Accepting; }

	private:
		agent_machinery& Machinery;
	};

	/// A window's Agent tab: what it was shown.
	class shown_turns final : public application::agent_invocation_projection_port
	{
	public:
		void offer(std::span<const core::agent_profile>) override { }
		void refuse(std::string_view aReason) override { Refused.emplace_back(aReason); }
		void admit(const application::agent_invocation_projection_identity&) override { }
		void start(const application::agent_invocation_projection_identity&, const application::prepared_workspace&) override { }
		void stream(const application::agent_invocation_projection_identity&, const application::agent_stream_event& aEvent) override { Text += aEvent.Text; }
		void finish(const application::agent_invocation_projection_identity&, const application::agent_run_result&) override { ++Finished; }
		void present_session_catalog(const application::agent_session_catalog_projection&) override { }

		std::string              Text;
		std::size_t              Finished = 0U;
		std::vector<std::string> Refused;
	};

	/// The workbench lane the profile writes conversations on.
	class manual_effects final : public application::effect_executor
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

	/// The profile's conversation store, as the next launch reads it.
	class memory_conversations final : public application::agent_conversation_store_port
	{
	public:
		[[nodiscard]] auto load(const application::agent_conversation_key& aKey) -> std::optional<std::vector<application::agent_conversation_turn>> override
		{
			const auto found = Turns.find(aKey);
			return found == Turns.end() ? std::vector<application::agent_conversation_turn>{ } : found->second;
		}
		[[nodiscard]] auto store(const application::agent_conversation_key& aKey, const std::vector<application::agent_conversation_turn>& aTurns) -> bool override
		{
			Turns[aKey] = aTurns;
			return true;
		}
		[[nodiscard]] auto load_agent(const application::agent_conversation_key&) -> std::optional<std::uint32_t> override { return std::nullopt; }
		[[nodiscard]] auto store_agent(const application::agent_conversation_key&) -> bool override { return true; }
		[[nodiscard]] auto forget_agents() -> bool override { return true; }

		std::map<application::agent_conversation_key, std::vector<application::agent_conversation_turn>> Turns;
	};

	struct profile
	{
		manual_lane                                         Lane;
		manual_effects                                      Effects;
		memory_conversations                                Conversations;
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
				.Runs                = [](const std::string& aSession) { return aSession.starts_with("agent-tab:"); },
				.Conversations       = &Conversations,
				.ConversationEffects = &Effects,
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

		// [[BPR-2181]]. The turns no window has seen keep the runtime until a
		// window takes them into the session's conversation.
		app.Lane.drain();
		expect(app.Runtimes->find(session) == runtime, "a runtime keeps the turns no window has seen");
		const auto later = runtime->attach(application::review_agent_presenter{ });
		runtime->forget_unshown();
		app.Runtimes->detach(session, later);
		// With nothing left to do and no window, the runtime is let go.
		app.Lane.drain();
		expect(app.Runtimes->find(session) == nullptr, "an idle runtime no window shows is let go");
	}

	/// [[BPR-2181]]. A turn no window saw is written to its pull request's
	/// conversation on the profile's lane, after what the conversation held,
	/// so a quit before a window attaches loses none; a turn a window saw is
	/// left to that window, and a native one to its native history.
	void test_unshown_turns_are_written_to_the_conversation()
	{
		auto       app                        = profile{ };
		const auto session                    = std::string{ "agent-tab:github.com/o/r#7" };
		const auto conversation               = application::agent_conversation_key{ .Host = "github.com", .Owner = "o", .Name = "r", .Number = 7U, .Agent = 1U };
		app.Conversations.Turns[conversation] = { { .Asked = "earlier", .Answered = "kept before" } };
		auto* runtime                         = app.Runtimes->open(session);
		auto  snapshot                        = review_snapshot();
		snapshot.Conversation                 = conversation;
		runtime->adopt(snapshot);

		// Shown to a window: the window keeps it, nothing is written here.
		auto       window         = shown_turns{ };
		const auto presenter      = runtime->attach({ .Projection = &window, .Release = { } });
		auto       shown          = snapshot.Request;
		shown.Context.ChatMessage = "seen";
		expect(runtime->dispatch(shown), "a window's turn goes through the runtime");
		static_cast<void>(app.Queue.enqueue(session, typed("first")));
		static_cast<void>(app.Queue.enqueue(session, typed("second")));
		app.Machinery[session].answer("seen answer");
		app.Lane.drain();
		app.Effects.drain();
		expect(app.Conversations.Turns[conversation].size() == 1U, "a turn a window saw is that window's to keep");
		expect(app.Machinery[session].Requests.size() == 2U, "the first message runs next");

		// The window closes mid-turn: that turn and the next, which no window
		// saw end, are each written, in order.
		app.Runtimes->detach(session, presenter);
		app.Lane.drain();
		app.Machinery[session].answer("one");
		app.Lane.drain();
		app.Machinery[session].answer("two");
		app.Lane.drain();
		expect(app.Conversations.Turns[conversation].size() == 1U, "nothing is written on the decision lane");
		app.Effects.drain();
		const auto& stored = app.Conversations.Turns[conversation];
		expect(stored.size() == 3U && stored[0].Asked == "earlier" && stored[1].Asked == "first" && stored[1].Answered == "one" && stored[2].Asked == "second" && stored[2].Answered == "two",
			   "each turn no window saw joins the stored conversation after what it held");

		// A native turn is kept by its native history, not here.
		auto native                = snapshot.Request;
		native.NativeAgentIdentity = "agent:1";
		native.NativeQuestion      = "native";
		expect(runtime->dispatch(native), "a native turn runs with no window");
		app.Machinery[session].answer("native answer");
		app.Effects.drain();
		expect(app.Conversations.Turns[conversation].size() == 3U, "a native turn is not written to the conversation");
		app.Lane.drain();
	}

	/// RFC-BPR-0126 AC02, AC03 ([[BPR-2181]] slice 3). A run's finish carries
	/// its outcome to the queue: a failure pauses it as failed and a Stop as
	/// stopped, with what remains still queued; a turn its machinery refused
	/// never leaves its message sending.
	void test_a_run_finish_pauses_the_queue_with_its_reason()
	{
		const auto failing = [](const application::agent_run_outcome aOutcome, const core::queue_pause aPause, const std::string_view aWhat) {
			auto       app     = profile{ };
			const auto session = std::string{ "agent-tab:github.com/o/r#7" };
			auto*      runtime = app.Runtimes->open(session);
			runtime->adopt(review_snapshot());
			static_cast<void>(app.Queue.enqueue(session, typed("first")));
			static_cast<void>(app.Queue.enqueue(session, typed("second")));
			expect(app.Machinery[session].Requests.size() == 1U, "the first message runs");
			app.Machinery[session].answer("ended", aOutcome);
			app.Lane.drain();
			const auto* const queue = app.Queue.queue(session);
			expect(queue->Paused == aPause && queue->Entries.size() == 1U && queue->Entries.front().Input.Text == "second" && queue->Entries.front().State == core::queued_state::queued
					   && !app.Queue.sending(session) && app.Machinery[session].Requests.size() == 1U,
				   aWhat);
		};
		failing(application::agent_run_outcome::nonzero_exit, core::queue_pause::failed, "a failing run leaves the queue paused as failed, the second message still queued");
		failing(application::agent_run_outcome::protocol_failure, core::queue_pause::failed, "an unreadable answer pauses as failed");
		failing(application::agent_run_outcome::cancelled, core::queue_pause::stopped, "Stop pauses as stopped");

		auto       app     = profile{ };
		const auto session = std::string{ "agent-tab:github.com/o/r#7" };
		auto*      runtime = app.Runtimes->open(session);
		runtime->adopt(review_snapshot());
		static_cast<void>(app.Queue.enqueue(session, typed("first")));
		static_cast<void>(app.Queue.enqueue(session, typed("second")));
		app.Machinery[session].refuse();
		app.Lane.drain();
		expect(!app.Queue.sending(session) && app.Queue.queue(session)->Paused == core::queue_pause::failed && app.Queue.queue(session)->Entries.size() == 1U,
			   "a turn refused before it started settles its message and pauses; nothing is left sending");
		expect(application::queued_finish_of({ .Outcome = application::agent_run_outcome::stale_result }) == core::queued_finish::went, "an answer set aside went");
	}

	/// [[BPR-2181]]. A window may say ready before the runtime's review is
	/// accepted: a turn sent then waits for it, and starts once the runtime
	/// is ready; stopped while it waits, it ends as stopped.
	void test_a_turn_sent_before_ready_waits_for_it()
	{
		auto       app     = profile{ };
		const auto session = std::string{ "agent-tab:github.com/o/r#7" };
		auto*      runtime = app.Runtimes->open(session);
		runtime->adopt(review_snapshot());
		app.Machinery[session].Authorized = false;
		app.Machinery[session].Accepting  = true;
		auto early                        = review_snapshot().Request;
		early.Context.ChatMessage         = "sent early";
		expect(runtime->dispatch(early) && runtime->running() && app.Machinery[session].Requests.empty(), "a turn sent before the review is accepted waits, and is not refused");
		runtime->ready();
		expect(app.Machinery[session].Requests.empty(), "it waits while the review is not accepted");
		app.Machinery[session].Authorized = true;
		app.Machinery[session].Accepting  = false;
		runtime->ready();
		expect(app.Machinery[session].Requests.size() == 1U && app.Machinery[session].Requests.front().Context.ChatMessage == "sent early", "once ready, it starts");
		app.Machinery[session].answer("answered");
		app.Lane.drain();
		app.Machinery[session].Authorized = false;
		expect(!runtime->dispatch(early), "with no authority lent at all, a turn is refused");
		app.Machinery[session].Accepting = true;
		expect(runtime->dispatch(early), "another waits while the review is accepted");
		runtime->cancel();
		expect(!runtime->running() && runtime->last_finish() == core::queued_finish::stopped && app.Machinery[session].Requests.size() == 1U, "stopped while it waits, it never starts");
		app.Lane.drain();
	}

	/// [[BPR-2243]]. A window sends its next turn once the last one's
	/// answer is shown, which can be before that run retires: the turn is
	/// held, not refused, and goes at the retirement ahead of the queue. A
	/// turn sent while the last one still answers is refused as before, and
	/// one held is dropped, said, by Stop.
	void test_a_turn_sent_while_the_last_retires_goes_at_its_retirement()
	{
		auto       app       = profile{ };
		const auto session   = std::string{ "agent-tab:github.com/o/r#7" };
		auto*      runtime   = app.Runtimes->open(session);
		auto       window    = shown_turns{ };
		auto&      machinery = app.Machinery[session];
		const auto presenter = runtime->attach(application::review_agent_presenter{ .Projection = &window });
		const auto turn      = [](const std::string& aText) {
			auto request                = review_snapshot().Request;
			request.Context.ChatMessage = aText;
			return request;
		};
		runtime->adopt(review_snapshot());
		expect(runtime->dispatch(turn("first")) && machinery.Requests.size() == 1U, "the first turn runs");
		expect(!runtime->dispatch(turn("too early")), "a turn sent while the last one still answers is refused");
		machinery.finish_only("first answered");
		expect(window.Finished == 1U && runtime->running(), "the window shows the answer while the run has not retired");
		expect(runtime->dispatch(turn("second")) && machinery.Requests.size() == 1U && window.Refused.empty(), "the next turn sent now is held, not refused");
		expect(!runtime->dispatch(turn("third")), "one held turn at most");
		expect(app.Queue.enqueue(session, typed("queued")) == core::queue_refusal::none, "a message is queued meanwhile");
		expect(!runtime->is_quiescent() && runtime->state(nullptr) == core::queue_agent::busy, "the held turn keeps the runtime busy");
		machinery.retire();
		app.Lane.drain();
		expect(machinery.Requests.size() == 2U && machinery.Requests.back().Context.ChatMessage == "second" && runtime->running(), "at the retirement the held turn goes");
		expect(app.Queue.queue(session)->Entries.size() == 1U && !app.Queue.sending(session), "ahead of the queue, which waits");
		machinery.answer("second answered");
		app.Lane.drain();
		expect(machinery.Requests.size() == 3U && machinery.Requests.back().Context.ChatMessage == "queued", "the queued message goes after it");
		machinery.finish_only("queued answered");
		expect(runtime->dispatch(turn("stopped")), "another is held");
		runtime->cancel();
		machinery.retire();
		app.Lane.drain();
		expect(machinery.Requests.size() == 3U && window.Refused.size() == 1U && window.Refused.front() == "Stopped. Nothing was sent.", "Stop drops the held turn and says so");
		runtime->detach(presenter);
		app.Lane.drain();
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
				[&](const core::queued_entry& aEntry, std::uint64_t) {
					asked.push_back(aEntry.Input.Label);
					auto request                = review_snapshot().Request;
					request.Context.ChatMessage = "expanded: " + aEntry.Input.Label;
					static_cast<void>(runtime->dispatch(std::move(request)));
					return application::queued_admission_kind::refused;
				},
		});
		static_cast<void>(app.Queue.enqueue(session, core::queued_input{ .QuickAsk = "what-changed", .Scope = "pull-request", .Label = "What changed?" }));
		expect(asked == std::vector<std::string>{ "What changed?" } && app.Machinery[session].Requests.back().Context.ChatMessage == "expanded: What changed?",
			   "a window that shows the session releases a quick ask with its material");
		app.Machinery[session].answer("it changed");
		expect(window.Text == "it changed" && window.Finished == 1U && runtime->unshown_turns().empty(), "a turn a window showed is presented, not kept as unshown");
		// An action's quick ask waits for a window once none shows the
		// session (it needs what a window shows).
		app.Runtimes->detach(session, presenter);
		app.Lane.drain();
		expect(app.Runtimes->find(session) == nullptr, "no window, no snapshot and no work: let go");
		runtime = app.Runtimes->open(session);
		runtime->adopt(review_snapshot());
		static_cast<void>(app.Queue.enqueue(session, core::queued_input{ .QuickAsk = "what-changed", .Scope = "pull-request", .Label = "What changed?" }));
		app.Lane.drain();
		expect(app.Machinery[session].Requests.size() == 1U && app.Queue.queue(session)->Entries.size() == 1U && app.Queue.queue(session)->Paused == core::queue_pause::none,
			   "a quick ask with no window stays queued, unpaused");
		// AC13: a suggested prompt about the pull request carries the
		// expansion decided at its press, and is sent with no window as it
		// was decided; one about the open file still waits for a window.
		const auto withdrawn = app.Queue.withdraw(session, app.Queue.queue(session)->Entries.front().Id);
		expect(withdrawn == core::queue_refusal::none, "the action's quick ask is taken off");
		// With no work and no window the runtime was let go; the profile's
		// runtime of the session holds the review again.
		runtime = app.Runtimes->open(session);
		runtime->adopt(review_snapshot());
		auto offer      = core::prompt_offer{ .Id = "own.look-again", .Label = "Ready to look again?" };
		offer.Expansion = core::prompt_expansion{ .Text = "Has it changed enough since my review?" };
		static_cast<void>(app.Queue.enqueue(session, core::queued_input{ .QuickAsk = "own.look-again", .Scope = "pull-request", .Label = "Ready to look again?", .Prompt = offer }));
		app.Lane.drain();
		expect(app.Machinery[session].Requests.size() == 2U && app.Machinery[session].Requests.back().Context.ChatMessage == "Has it changed enough since my review?",
			   "a decided prompt about the pull request is sent with no window, as decided at the press");
		app.Machinery[session].answer("yes");
		app.Lane.drain();
		auto aboutFile    = offer;
		aboutFile.Subject = core::prompt_subject::open_file;
		static_cast<void>(app.Queue.enqueue(session, core::queued_input{ .QuickAsk = "own.file", .Scope = "file", .File = "a.cpp", .Label = "About a.cpp", .Prompt = aboutFile }));
		app.Lane.drain();
		expect(app.Machinery[session].Requests.size() == 2U && app.Queue.queue(session)->Entries.size() == 1U, "one about the open file waits for a window");
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

	/// [[BPR-2181]]. The review's authority the profile lent the runtime ends
	/// (here, a lost lease): the queue pauses with that reason, the running
	/// turn is cancelled, and no turn starts until a live authority returns
	/// and the reviewer resumes.
	void test_a_lost_authority_pauses_the_queue_and_starts_nothing()
	{
		auto       app     = profile{ };
		const auto session = std::string{ "agent-tab:github.com/o/r#11" };
		auto*      runtime = app.Runtimes->open(session);
		runtime->adopt(review_snapshot());
		// A window shows the session throughout, so the runtime stays.
		auto window = shown_turns{ };
		static_cast<void>(runtime->attach({ .Projection = &window, .Release = { } }));
		static_cast<void>(app.Queue.enqueue(session, typed("one")));
		static_cast<void>(app.Queue.enqueue(session, typed("two")));
		auto& machinery = app.Machinery[session];
		expect(machinery.Requests.size() == 1U, "the first message is running");

		machinery.Authorized = false;
		app.Runtimes->authority_ended(session);
		expect(machinery.Cancels == 1U && app.Queue.queue(session)->Paused == core::queue_pause::lost_access,
			   "the running turn is cancelled and the queue pauses with lost access");
		machinery.answer("cut short");
		app.Lane.drain();
		expect(machinery.Requests.size() == 1U && app.Queue.queue(session)->Entries.size() == 1U, "the next message does not start");
		auto direct                = review_snapshot().Request;
		direct.Context.ChatMessage = "typed";
		expect(!runtime->dispatch(direct) && runtime->state(nullptr) == core::queue_agent::not_set_up,
			   "no turn starts without a live authority, not even a window's own");
		expect(app.Queue.resume(session) == core::queue_refusal::none && machinery.Requests.size() == 1U,
			   "resuming without a live authority still starts nothing");

		machinery.Authorized = true;
		app.Queue.agent_changed(session);
		expect(machinery.Requests.size() == 2U && machinery.Requests.back().Context.ChatMessage == "two",
			   "with a live authority again the queue goes on");
		machinery.answer("two answered");
		app.Lane.drain();
	}
	/// RFC-BPR-0126 AC03 and AC12 ([[BPR-2181]]). A release's admission is
	/// keyed by its run and never read from a window's words. A queued
	/// Explain this file whose material read is refused on a later pass, and
	/// a slash command answered with no turn, leave nothing sending; the
	/// awaited turn, once sent, takes the release's run and its retirement
	/// settles it; a window closing while a release awaits its turn returns
	/// it; and Resume after a stale refusal note sends the message once.
	void test_release_admission_keyed_by_run()
	{
		auto       app       = profile{ };
		const auto session   = std::string{ "agent-tab:github.com/o/r#12" };
		auto*      runtime   = app.Runtimes->open(session);
		auto&      machinery = app.Machinery[session];
		runtime->adopt(review_snapshot());
		auto window = shown_turns{ };
		// The window's release: an explain awaits its material, a slash
		// command is answered, a typed message is sent at once.
		auto       awaited   = std::optional<std::uint64_t>{ };
		const auto presenter = runtime->attach({
			.Projection = &window,
			.Release =
				[&](const core::queued_entry& aEntry, const std::uint64_t aRun) {
					if (aEntry.Input.QuickAsk == "explain-file") {
						awaited = aRun;
						return application::queued_admission_kind::awaiting;
					}
					if (aEntry.Input.Text.starts_with('/')) {
						return application::queued_admission_kind::answered;
					}
					auto request                = review_snapshot().Request;
					request.Context.ChatMessage = aEntry.Input.Text;
					static_cast<void>(runtime->dispatch(std::move(request)));
					// A stale note from an earlier refused send says
					// nothing about this one: the runtime saw the turn.
					return application::queued_admission_kind::refused;
				},
		});
		const auto explain   = core::queued_input{ .QuickAsk = "explain-file", .Scope = "file", .File = "a.cpp", .Label = "Explain a.cpp" };

		// A refused material read: back first, paused, not sending.
		static_cast<void>(app.Queue.enqueue(session, explain));
		expect(awaited.has_value() && app.Queue.sending(session) && runtime->state(nullptr) == core::queue_agent::busy, "an awaiting release holds the session");
		runtime->refuse_release(*awaited + 1U);
		expect(app.Queue.sending(session).has_value(), "a refusal of another run settles nothing");
		runtime->refuse_release(*awaited);
		app.Lane.drain();
		expect(!app.Queue.sending(session) && app.Queue.queue(session)->Paused == core::queue_pause::failed && app.Queue.queue(session)->Entries.size() == 1U
				   && app.Queue.queue(session)->Entries.front().State == core::queued_state::queued && machinery.Requests.empty(),
			   "a refused material read leaves the message queued, paused, never sending");

		// Resumed, the read succeeds this time: the turn sent on the later
		// pass takes the release's run, and its retirement settles it.
		expect(app.Queue.resume(session) == core::queue_refusal::none && app.Queue.sending(session), "resumed, it awaits again");
		auto later                = review_snapshot().Request;
		later.Context.ChatMessage = "Explain a.cpp";
		expect(runtime->dispatch(later) && runtime->last_run() == *awaited, "the awaited turn takes the release's run");
		machinery.answer("explained");
		app.Lane.drain();
		expect(!app.Queue.sending(session) && app.Queue.queue(session)->Entries.empty() && app.Queue.queue(session)->Paused == core::queue_pause::none, "its run's retirement settles it");

		// A slash command is answered with no turn, and the queue goes on.
		static_cast<void>(app.Queue.enqueue(session, typed("/model opus")));
		app.Lane.drain();
		expect(app.Queue.queue(session)->Entries.empty() && !app.Queue.sending(session) && machinery.Requests.size() == 1U, "a slash command never stays sending");

		// Resume after a stale note sends once.
		app.Queue.pause(session, core::queue_pause::reviewer);
		static_cast<void>(app.Queue.enqueue(session, typed("once")));
		const auto seen = app.Queue.queue(session)->Revision;
		expect(app.Queue.resume(session, seen) == core::queue_refusal::none && machinery.Requests.size() == 2U && machinery.Requests.back().Context.ChatMessage == "once",
			   "Resume sends the message, though the window's words said refused");
		expect(app.Queue.resume(session, seen) == core::queue_refusal::stale && app.Queue.resume(session) == core::queue_refusal::none && machinery.Requests.size() == 2U,
			   "a second Resume sends nothing more");
		machinery.answer("once answered");
		app.Lane.drain();
		expect(app.Queue.queue(session)->Entries.empty() && app.Queue.queue(session)->Paused == core::queue_pause::none && machinery.Requests.size() == 2U, "it went once and settled");

		// The window closes while a release awaits its turn: it goes back.
		static_cast<void>(app.Queue.enqueue(session, explain));
		expect(app.Queue.sending(session).has_value(), "awaiting again");
		app.Runtimes->detach(session, presenter);
		app.Lane.drain();
		expect(!app.Queue.sending(session) && app.Queue.queue(session)->Entries.size() == 1U && app.Queue.queue(session)->Entries.front().State == core::queued_state::queued,
			   "a release whose window closed before its turn went is queued again");
	}
	/// RFC-BPR-0126 AC15 ([[BPR-2181]]). Stop all is the host's, across the
	/// profile: a turn no window shows and one a window shows both stop,
	/// every queue pauses as stopped, and nothing starts next.
	void test_stop_all_is_the_hosts()
	{
		auto       app    = profile{ };
		const auto unseen = std::string{ "agent-tab:github.com/o/r#13" };
		const auto shown  = std::string{ "agent-tab:github.com/o/r#14" };
		for (const auto& session : { unseen, shown }) {
			app.Runtimes->open(session)->adopt(review_snapshot());
			static_cast<void>(app.Queue.enqueue(session, typed("running")));
			static_cast<void>(app.Queue.enqueue(session, typed("waiting")));
		}
		auto window = shown_turns{ };
		static_cast<void>(app.Runtimes->find(shown)->attach({ .Projection = &window }));
		expect(app.Machinery[unseen].Requests.size() == 1U && app.Machinery[shown].Requests.size() == 1U, "each session runs its first message");
		app.Runtimes->stop_all();
		expect(app.Machinery[unseen].Cancels == 1U && app.Machinery[shown].Cancels == 1U, "every session's turn is stopped, shown or not");
		app.Machinery[unseen].answer("cut", application::agent_run_outcome::cancelled);
		app.Machinery[shown].answer("cut", application::agent_run_outcome::cancelled);
		app.Lane.drain();
		for (const auto& session : { unseen, shown }) {
			expect(app.Queue.queue(session)->Paused == core::queue_pause::stopped && app.Queue.queue(session)->Entries.size() == 1U && app.Machinery[session].Requests.size() == 1U,
				   "every queue pauses as stopped and nothing starts next");
		}
	}
	/// RFC-BPR-0126 AC08 ([[BPR-2181]]). A window switches pull request in
	/// the middle of a released turn: it detaches from the old session and
	/// presents the new one. The old session's Sending message settles from
	/// its own run's retirement, per session, and the new session runs
	/// beside it; neither waits on the window.
	void test_a_pull_request_switch_mid_turn_settles_per_session()
	{
		auto       app    = profile{ };
		const auto before = std::string{ "agent-tab:github.com/o/r#15" };
		const auto after  = std::string{ "agent-tab:github.com/o/r#16" };
		auto*      old    = app.Runtimes->open(before);
		old->adopt(review_snapshot());
		auto       window    = shown_turns{ };
		const auto presenter = old->attach({ .Projection = &window });
		static_cast<void>(app.Queue.enqueue(before, typed("first")));
		static_cast<void>(app.Queue.enqueue(before, typed("second")));
		expect(app.Queue.sending(before).has_value() && app.Machinery[before].Requests.size() == 1U, "the first message runs");
		// The switch: the window leaves the old session for the new one.
		app.Runtimes->detach(before, presenter);
		auto* next = app.Runtimes->open(after);
		next->adopt(review_snapshot());
		static_cast<void>(next->attach({ .Projection = &window }));
		static_cast<void>(app.Queue.enqueue(after, typed("elsewhere")));
		app.Lane.drain();
		expect(app.Machinery[before].Cancels == 0U && app.Runtimes->find(before) != nullptr && app.Machinery[after].Requests.size() == 1U,
			   "the old turn goes on untouched and the new session runs beside it");
		app.Machinery[before].answer("first answered");
		app.Lane.drain();
		expect(app.Queue.queue(before)->Entries.size() == 1U && app.Queue.sending(before).has_value() && app.Machinery[before].Requests.back().Context.ChatMessage == "second",
			   "its retirement settles the old session's message and its next goes, with no window");
		app.Machinery[before].answer("second answered");
		app.Machinery[after].answer("elsewhere answered");
		app.Lane.drain();
		expect(app.Queue.queue(before)->Entries.empty() && app.Queue.queue(after)->Entries.empty() && !app.Queue.sending(before) && !app.Queue.sending(after),
			   "each session settles from its own run");
		expect(app.Runtimes->find(before) != nullptr && app.Runtimes->find(before)->unshown_turns().size() == 2U, "the old session keeps both turns no window saw end");
	}
} // namespace

auto main() -> int
{
	test_a_pull_request_switch_mid_turn_settles_per_session();
	test_stop_all_is_the_hosts();
	test_release_admission_keyed_by_run();
	test_zero_windows_drain_and_keep_the_transcript();
	test_unshown_turns_are_written_to_the_conversation();
	test_a_run_finish_pauses_the_queue_with_its_reason();
	test_a_turn_sent_before_ready_waits_for_it();
	test_a_turn_sent_while_the_last_retires_goes_at_its_retirement();
	test_window_releases_and_runtime_stays_while_work_waits();
	test_shutdown_stops_every_runtime();
	test_a_lost_authority_pauses_the_queue_and_starts_nothing();
	return EXIT_SUCCESS;
}
