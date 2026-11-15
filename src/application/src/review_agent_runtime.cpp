#include "bpr/application/review_agent_runtime.hpp"

#include "bpr/application/agent_runtime_steer.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace bpr::application {
	namespace {
		/// [[BPR-2181]]. A turn no window saw, added to the end of its pull
		/// request's stored conversation (the oldest going first past the
		/// bound), so it outlives a quit. A store that cannot be read is left
		/// alone: the turn is still kept in memory for the next window.
		void keep_unshown_turn(agent_conversation_store_port& aStore, const agent_conversation_key& aConversation, const agent_conversation_turn& aTurn)
		{
			auto turns = aStore.load(aConversation);
			if (!turns) {
				return;
			}
			turns->push_back(aTurn);
			if (turns->size() > maximum_conversation_turns) {
				turns->erase(turns->begin(), turns->begin() + static_cast<std::ptrdiff_t>(turns->size() - maximum_conversation_turns));
			}
			static_cast<void>(aStore.store(aConversation, *turns));
		}
	} // namespace

	auto queued_finish_of(const agent_run_result& aResult) noexcept -> core::queued_finish
	{
		if (aResult.Outcome == agent_run_outcome::protocol_failure && aResult.ProtocolFailure == agent_protocol_failure::usage_limit) {
			return core::queued_finish::usage_limit;
		}
		switch (aResult.Outcome) {
		case agent_run_outcome::success:
		case agent_run_outcome::stale_result:            return core::queued_finish::went;
		case agent_run_outcome::cancelled:               return core::queued_finish::stopped;
		case agent_run_outcome::private_storage_failure:
		case agent_run_outcome::failed_to_start:
		case agent_run_outcome::workspace_unavailable:
		case agent_run_outcome::nonzero_exit:
		case agent_run_outcome::protocol_failure:
		case agent_run_outcome::timeout:                 return core::queued_finish::failed;
		}
		return core::queued_finish::failed;
	}

	review_agent_runtime::review_agent_runtime(std::string aSession)
		: Session(std::move(aSession))
	{ }

	review_agent_runtime::~review_agent_runtime() = default;

	void review_agent_runtime::bind(std::unique_ptr<review_agent_turn_port> aTurns) { Turns = std::move(aTurns); }

	void review_agent_runtime::adopt(review_agent_snapshot aSnapshot) { Snapshot = std::move(aSnapshot); }

	auto review_agent_runtime::snapshot() const -> const std::optional<review_agent_snapshot>& { return Snapshot; }

	auto review_agent_runtime::attach(review_agent_presenter aPresenter) -> std::uint64_t
	{
		const auto token = ++LastPresenter;
		Presenters.emplace(token, std::move(aPresenter));
		return token;
	}

	void review_agent_runtime::detach(const std::uint64_t aPresenter) { Presenters.erase(aPresenter); }

	auto review_agent_runtime::presented() const -> bool { return !Presenters.empty(); }

	auto review_agent_runtime::state(const core::queued_entry* const aFront) const -> core::queue_agent
	{
		if (Turns == nullptr) {
			return core::queue_agent::not_set_up;
		}
		if (Running || !Turns->is_quiescent()) {
			return core::queue_agent::busy;
		}
		if (!Turns->authorized()) {
			return core::queue_agent::not_set_up;
		}
		if (aFront == nullptr) {
			return Snapshot || std::ranges::any_of(Presenters, [](const auto& aPresenter) { return aPresenter.second.releases(); })
				? core::queue_agent::idle
				: core::queue_agent::not_set_up;
		}
		if (std::ranges::any_of(Presenters, [](const auto& aPresenter) { return aPresenter.second.releases(); })) {
			return core::queue_agent::idle;
		}
		// A typed message is built from the snapshot alone. A quick ask needs
		// its decided expansion (RFC-BPR-0126 AC13, slice 6): until a release
		// carries one, it waits for a window that shows the session.
		return Snapshot && aFront->Input.QuickAsk.empty() ? core::queue_agent::idle : core::queue_agent::not_set_up;
	}

	auto review_agent_runtime::release(const core::queued_entry& aEntry) -> bool
	{
		if (state(&aEntry) != core::queue_agent::idle) {
			return false;
		}
		for (const auto& [token, presenter] : Presenters) {
			if (presenter.releases()) {
				return presenter.Release(aEntry);
			}
		}
		// (b) Built from the review the runtime holds: the Agent tab's turn on
		// that review, asking what was typed.
		auto request                = Snapshot->Request;
		request.Context.ChatMessage = aEntry.Input.Text;
		request.NativeQuestion.clear();
		return dispatch(std::move(request));
	}

	auto review_agent_runtime::dispatch(agent_invocation_request aRequest) -> bool
	{
		if (Turns == nullptr || Running || !Turns->is_quiescent() || (!Turns->authorized() && !Turns->accepting())) {
			return false;
		}
		Running   = true;
		TurnShown = presented();
		Finished.reset();
		// [[BPR-2181]]. A native session's turn is kept in its native history,
		// the transcript a window shows it from; only another is kept here.
		TurnNative = !aRequest.NativeAgentIdentity.empty();
		Turn       = agent_conversation_turn{ .Asked = aRequest.NativeQuestion.empty() ? aRequest.Context.ChatMessage : aRequest.NativeQuestion };
		if (!Turns->authorized()) {
			// The window said ready before the runtime's review was accepted:
			// the turn waits for that rather than being refused.
			Waiting = std::move(aRequest);
			return true;
		}
		Turns->dispatch(std::move(aRequest));
		return true;
	}

	void review_agent_runtime::ready()
	{
		// A copy: a presenter may detach while it is told.
		const auto presenters = Presenters;
		for (const auto& [token, presenter] : presenters) {
			if (presenter.Accepted) {
				presenter.Accepted();
			}
		}
		if (!Waiting || Turns == nullptr || !Turns->authorized()) {
			return;
		}
		auto request = std::move(*Waiting);
		Waiting.reset();
		Turns->dispatch(std::move(request));
	}

	void review_agent_runtime::cancel()
	{
		if (Waiting) {
			// A turn that never started stops here, as stopped.
			Waiting.reset();
			Turn.reset();
			Finished = core::queued_finish::stopped;
			Running  = false;
			if (Done) {
				Done();
			}
			return;
		}
		if (Turns != nullptr) {
			Turns->cancel();
		}
	}

	void review_agent_runtime::retired()
	{
		if (!Running || (Turns != nullptr && !Turns->is_quiescent())) {
			return;
		}
		Running = false;
		if (Done) {
			Done();
		}
	}

	void review_agent_runtime::on_retired(std::function<void()> aDone) { Done = std::move(aDone); }

	auto review_agent_runtime::running() const -> bool { return Running; }

	auto review_agent_runtime::last_finish() const -> core::queued_finish { return Finished.value_or(core::queued_finish::failed); }

	auto review_agent_runtime::is_quiescent() const -> bool { return !Running && (Turns == nullptr || Turns->is_quiescent()); }

	void review_agent_runtime::shutdown()
	{
		Waiting.reset();
		if (Turns != nullptr) {
			Turns->shutdown();
		}
	}

	void review_agent_runtime::retry_cleanup()
	{
		if (Turns != nullptr) {
			Turns->retry_cleanup();
		}
		// Once shut down, the machinery is quiescent only after its review's
		// hosts are too, which can be after its run announced retirement:
		// the turn's retirement is looked at again here.
		retired();
	}

	auto review_agent_runtime::unshown_turns() const -> const std::vector<agent_conversation_turn>& { return Unshown; }

	void review_agent_runtime::forget_unshown() { Unshown.clear(); }

	void review_agent_runtime::on_unshown(std::function<void(const agent_conversation_key&, const agent_conversation_turn&)> aKeep) { Keep = std::move(aKeep); }

	auto review_agent_runtime::session() const -> const std::string& { return Session; }

	template <typename Event>
	void review_agent_runtime::present(Event&& aEvent)
	{
		// A copy: a presenter may detach while it is shown the event.
		const auto presenters = Presenters;
		for (const auto& [token, presenter] : presenters) {
			if (presenter.Projection != nullptr) {
				aEvent(*presenter.Projection);
			}
		}
	}

	void review_agent_runtime::offer(const std::span<const core::agent_profile> aProfiles)
	{
		present([aProfiles](agent_invocation_projection_port& aPort) { aPort.offer(aProfiles); });
	}

	void review_agent_runtime::refuse(const std::string_view aReason)
	{
		present([aReason](agent_invocation_projection_port& aPort) { aPort.refuse(aReason); });
	}

	auto review_agent_runtime::steer(const core::queued_entry& aEntry, agent_queue_host* const aQueue) -> std::optional<std::string>
	{
		auto alive = std::weak_ptr<bool>{ Alive };
		auto steer = agent_runtime_steer({
			.Support  = [this](const agent_invocation_projection_identity& aRun) { return Turns ? Turns->steer_support(aRun) : core::agent_steer_support::not_running; },
			.Steer    = [this](const agent_invocation_projection_identity& aRun, std::string aText, agent_invocation_host::steer_answer aAnswer) {
                if (Turns) {
                    Turns->steer(aRun, std::move(aText), std::move(aAnswer));
                } else if (aAnswer) {
                    aAnswer({ .Support = core::agent_steer_support::not_running });
                } },
			.Queue    = aQueue,
			.Shown    = [this] { return Active; },
			.Returned = [this, alive](const std::string&, const std::string& aWhy) {
				if (!alive.expired()) {
					refuse(aWhy);
				} },
		});
		return steer(Session, aEntry);
	}

	void review_agent_runtime::admit(const agent_invocation_projection_identity& aIdentity)
	{
		Active = aIdentity;
		present([&aIdentity](agent_invocation_projection_port& aPort) { aPort.admit(aIdentity); });
	}

	void review_agent_runtime::start(const agent_invocation_projection_identity& aIdentity, const prepared_workspace& aWorkspace)
	{
		present([&](agent_invocation_projection_port& aPort) { aPort.start(aIdentity, aWorkspace); });
	}

	void review_agent_runtime::stream(const agent_invocation_projection_identity& aIdentity, const agent_stream_event& aEvent)
	{
		if (Turn && aEvent.Kind == agent_stream_kind::text) {
			Turn->Answered += aEvent.Text;
		}
		// [[BPR-2153]]. The reviewer's steered message is part of the turn,
		// where it went in.
		if (Turn && aEvent.Kind == agent_stream_kind::steered && aEvent.Steered && Turn->Steered.size() < core::maximum_steers_per_run) {
			Turn->Steered.push_back({ .Text = aEvent.Steered->Text, .At = Turn->Answered.size(), .AfterSteps = aEvent.Steered->AfterSteps });
		}
		present([&](agent_invocation_projection_port& aPort) { aPort.stream(aIdentity, aEvent); });
	}

	void review_agent_runtime::finish(const agent_invocation_projection_identity& aIdentity, const agent_run_result& aResult)
	{
		if (Running) {
			Finished = queued_finish_of(aResult);
		}
		if (Active == std::optional{ aIdentity }) {
			Active.reset();
		}
		if (Turn) {
			// [[BPR-2153]]. Each steered message as it ended.
			for (std::size_t index = 0U; index < Turn->Steered.size() && index < aResult.SteeredInputs.size(); ++index) {
				Turn->Steered[index].Disposition = aResult.SteeredInputs[index].Disposition;
			}
			Turn->Ending  = std::string{ name(aResult.Outcome) };
			Turn->Outcome = turn_outcome(aResult.Outcome);
			if (Turn->Answered.empty()) {
				for (const auto& item : aResult.AssistantItems) {
					Turn->Answered += item;
				}
			}
			// (c) A turn no window saw to its end is kept for the next one,
			// unless its native history keeps it: each turn has one transcript.
			if ((!TurnShown || !presented()) && !TurnNative) {
				if (Keep && Snapshot && Snapshot->Conversation) {
					Keep(*Snapshot->Conversation, *Turn);
				}
				Unshown.push_back(std::move(*Turn));
				if (Unshown.size() > maximum_unshown_turns) {
					Unshown.erase(Unshown.begin());
				}
			}
			Turn.reset();
		}
		present([&](agent_invocation_projection_port& aPort) { aPort.finish(aIdentity, aResult); });
	}

	void review_agent_runtime::deliver_findings(const agent_invocation_projection_identity& aIdentity, const agent_finding_delivery& aDelivery)
	{
		present([&](agent_invocation_projection_port& aPort) { aPort.deliver_findings(aIdentity, aDelivery); });
	}

	void review_agent_runtime::private_changed(const agent_invocation_projection_identity& aIdentity, const private_review_delta& aDelta)
	{
		present([&](agent_invocation_projection_port& aPort) { aPort.private_changed(aIdentity, aDelta); });
	}

	void review_agent_runtime::private_refused(const agent_invocation_projection_identity& aIdentity, const private_run_error& aError)
	{
		present([&](agent_invocation_projection_port& aPort) { aPort.private_refused(aIdentity, aError); });
	}

	void review_agent_runtime::restore_native_session(const core::agent_session& aSession)
	{
		present([&](agent_invocation_projection_port& aPort) { aPort.restore_native_session(aSession); });
	}

	void review_agent_runtime::present_native_history(const native_agent_history_projection& aView)
	{
		present([&](agent_invocation_projection_port& aPort) { aPort.present_native_history(aView); });
	}

	void review_agent_runtime::present_session_catalog(const agent_session_catalog_projection& aProjection)
	{
		present([&](agent_invocation_projection_port& aPort) { aPort.present_session_catalog(aProjection); });
	}

	review_agent_runtimes::review_agent_runtimes(review_agent_runtime_dependencies aDependencies)
		: Dependencies(std::move(aDependencies))
	{
		if (Dependencies.Runner != nullptr) {
			Offered = Dependencies.Runner->offer(agent_session_runtime{
				.Runs  = [this](const std::string& aSession) { return !Closing && find(aSession) != nullptr; },
				.State = [this](const std::string& aSession) {
					const auto* const runtime = find(aSession);
					return runtime == nullptr ? core::queue_agent::not_set_up : runtime->state(front(aSession)); },
				.Start = [this](const std::string& aSession, const core::queued_entry& aEntry) {
					auto* const runtime = find(aSession);
					return runtime != nullptr && runtime->release(aEntry); },
				// [[BPR-2153]]. A queued message offered to the session's
				// running turn goes to the run its runtime runs.
				.Steer = [this](const std::string& aSession, const core::queued_entry& aEntry) -> std::optional<std::string> {
					auto* const runtime = find(aSession);
					return runtime == nullptr ? agent_steer_goes_next_words(core::agent_steer_support::not_running) : runtime->steer(aEntry, Dependencies.Queue); },
			});
		}
		if (Dependencies.Queue != nullptr) {
			Subscription = Dependencies.Queue->subscribe(agent_queue_subscriber{
				.Changed = [this](const std::string& aSession, const core::agent_queue&) { settle(aSession); },
				.Waiting = { },
			});
		}
	}

	review_agent_runtimes::~review_agent_runtimes()
	{
		*Alive = false;
		if (Offered && Dependencies.Runner != nullptr) {
			Dependencies.Runner->withdraw(*Offered);
		}
		if (Subscription && Dependencies.Queue != nullptr) {
			Dependencies.Queue->unsubscribe(*Subscription);
		}
	}

	auto review_agent_runtimes::open(const std::string& aSession) -> review_agent_runtime*
	{
		if (Closing || aSession.empty() || (Dependencies.Runs && !Dependencies.Runs(aSession))) {
			return nullptr;
		}
		if (auto* const found = find(aSession); found != nullptr) {
			return found;
		}
		if (!Dependencies.Compose) {
			return nullptr;
		}
		auto  runtime = std::make_unique<review_agent_runtime>(aSession);
		auto& made    = *runtime;
		auto  turns   = Dependencies.Compose(aSession, made, [alive = std::weak_ptr<bool>{ Alive }, &made] {
			if (const auto live = alive.lock(); live && *live) {
				made.retired();
			}
		});
		if (turns == nullptr) {
			return nullptr;
		}
		made.bind(std::move(turns));
		made.on_retired([this, aSession] { retired(aSession); });
		if (Dependencies.Conversations != nullptr && Dependencies.ConversationEffects != nullptr) {
			made.on_unshown([store = Dependencies.Conversations, effects = Dependencies.ConversationEffects](const agent_conversation_key& aConversation, const agent_conversation_turn& aTurn) {
				effects->post([store, aConversation, aTurn] { keep_unshown_turn(*store, aConversation, aTurn); });
			});
		}
		Runtimes.emplace(aSession, std::move(runtime));
		return &made;
	}

	auto review_agent_runtimes::find(const std::string& aSession) const -> review_agent_runtime*
	{
		const auto found = Runtimes.find(aSession);
		return found == Runtimes.end() ? nullptr : found->second.get();
	}

	void review_agent_runtimes::detach(const std::string& aSession, const std::uint64_t aPresenter)
	{
		if (auto* const runtime = find(aSession); runtime != nullptr) {
			runtime->detach(aPresenter);
			settle(aSession);
		}
	}

	void review_agent_runtimes::authority_ended(const std::string& aSession)
	{
		if (Dependencies.Queue != nullptr) {
			Dependencies.Queue->pause(aSession, core::queue_pause::lost_access);
		}
		if (auto* const runtime = find(aSession); runtime != nullptr) {
			runtime->cancel();
		}
		settle(aSession);
	}

	auto review_agent_runtimes::sessions() const -> std::vector<std::string>
	{
		auto sessions = std::vector<std::string>{ };
		for (const auto& [session, runtime] : Runtimes) {
			sessions.push_back(session);
		}
		return sessions;
	}

	void review_agent_runtimes::shutdown()
	{
		Closing = true;
		for (const auto& [session, runtime] : Runtimes) {
			runtime->cancel();
			runtime->shutdown();
		}
	}

	void review_agent_runtimes::retry_cleanup()
	{
		for (const auto& [session, runtime] : Runtimes) {
			runtime->retry_cleanup();
		}
		for (const auto& runtime : Retiring) {
			runtime->retry_cleanup();
		}
	}

	auto review_agent_runtimes::is_quiescent() const -> bool
	{
		return std::ranges::all_of(Runtimes, [](const auto& aRuntime) { return aRuntime.second->is_quiescent(); })
			&& std::ranges::all_of(Retiring, [](const auto& aRuntime) { return aRuntime->is_quiescent(); });
	}

	auto review_agent_runtimes::front(const std::string& aSession) const -> const core::queued_entry*
	{
		const auto* const queue = Dependencies.Queue != nullptr ? Dependencies.Queue->queue(aSession) : nullptr;
		if (queue == nullptr) {
			return nullptr;
		}
		const auto entry = std::ranges::find(queue->Entries, core::queued_state::queued, &core::queued_entry::State);
		return entry == queue->Entries.end() ? nullptr : &*entry;
	}

	auto review_agent_runtimes::holds_work(const std::string& aSession) const -> bool
	{
		if (Dependencies.Queue == nullptr) {
			return false;
		}
		if (Dependencies.Queue->sending(aSession)) {
			return true;
		}
		// Turns no window has seen yet are kept until one attaches.
		if (const auto* const runtime = find(aSession); runtime != nullptr && !runtime->unshown_turns().empty()) {
			return true;
		}
		const auto* const queue   = Dependencies.Queue->queue(aSession);
		const auto* const next    = front(aSession);
		const auto* const runtime = find(aSession);
		// Work the runtime can do itself: a paused queue, or a quick ask only a
		// window can release, waits for a window, which opens the runtime again.
		return queue != nullptr && queue->Paused == core::queue_pause::none && next != nullptr && runtime != nullptr
			&& runtime->state(next) != core::queue_agent::not_set_up;
	}

	/// The runtime is let go once no window shows it, no turn of it runs, it
	/// keeps no turn a window has not seen and its queue holds no work it can
	/// do; never inside its own callback.
	void review_agent_runtimes::settle(const std::string& aSession)
	{
		if (Dependencies.DecisionLane == nullptr) {
			return;
		}
		Dependencies.DecisionLane->post([alive = std::weak_ptr<bool>{ Alive }, this, aSession] {
			const auto live = alive.lock();
			if (!live || !*live || Closing) {
				return;
			}
			const auto found = Runtimes.find(aSession);
			if (found == Runtimes.end() || found->second->presented() || !found->second->is_quiescent() || holds_work(aSession)) {
				return;
			}
			auto runtime = std::move(found->second);
			Runtimes.erase(found);
			runtime->shutdown();
			Retiring.push_back(std::move(runtime));
			Dependencies.DecisionLane->post([alive, this] {
				if (const auto still = alive.lock(); still && *still) {
					std::erase_if(Retiring, [](const auto& aRuntime) { return aRuntime->is_quiescent(); });
				}
			});
		});
	}

	void review_agent_runtimes::retired(const std::string& aSession)
	{
		// The run the queue host started settles its message; a turn a window
		// sent directly frees the session for the queue's next.
		if (Dependencies.Runner != nullptr && Offered && Dependencies.Runner->running(aSession) == Offered) {
			const auto* const runtime = find(aSession);
			Dependencies.Runner->retired(*Offered, aSession, runtime != nullptr ? runtime->last_finish() : core::queued_finish::failed);
		} else if (Dependencies.Queue != nullptr) {
			Dependencies.Queue->agent_changed(aSession);
		}
		settle(aSession);
	}
} // namespace bpr::application
