/// \file
/// \brief RFC-BPR-0126 AC01 ([[BPR-2154]]). The tour chats' and the local
/// draft discussion's queues in the window. Each tour part (the whole tour
/// after its first draft, a stop) and each part of
/// a local draft's discussion is a session of the profile's queue host; tour
/// parts never share inputs. The window presents each part's pending list in
/// the tour chat, forwards its controls to the host, and offers the runtime
/// the host starts their turns on: the window's tour host for a pull
/// request's tour, the local draft's own lane for its discussion. The host
/// alone decides when a message goes.

#include "github_application/window_composition.hpp"

#include "bpr/application/agent_runtime_steer.hpp"
#include "bpr/qt/agent_footer.hpp"

#include <QMetaObject>
#include <QPointer>
#include <QTimer>

#include <algorithm>

namespace bpr::app {
#if defined(BPR_AGENT_INTEGRATIONS)
	namespace {
		[[nodiscard]] auto is_chat_session(const std::optional<core::queued_session>& aSession) -> bool
		{
			return aSession && (aSession->Kind == core::queued_session_kind::tour_part || aSession->Kind == core::queued_session_kind::local_draft_discussion);
		}

		/// The queue's state in words, as the Agent tab says its own.
		[[nodiscard]] auto queue_words(const core::agent_queue& aQueue, const bool aReady) -> QString
		{
			switch (aQueue.Paused) {
			case core::queue_pause::reviewer:         return QStringLiteral("Paused");
			case core::queue_pause::stopped:          return QStringLiteral("Paused: Stop");
			case core::queue_pause::failed:           return QStringLiteral("Paused: a run failed");
			case core::queue_pause::usage_limit:      return QStringLiteral("Paused: the agent's usage limit");
			case core::queue_pause::restarted:        return QStringLiteral("Paused since the app restarted");
			case core::queue_pause::delivery_unknown: return QStringLiteral("Paused: a message may not have been delivered");
			case core::queue_pause::changed_target:   return QStringLiteral("Paused: what a message is about has changed");
			case core::queue_pause::lost_access:      return QStringLiteral("Paused: this pull request can no longer be read with your account");
			case core::queue_pause::none:             break;
			}
			return aReady ? QStringLiteral("Queued, waiting its turn") : QStringLiteral("Queued, waiting for an agent to be chosen");
		}

		[[nodiscard]] auto finish_of(const application::agent_run_outcome aOutcome) -> core::queued_finish
		{
			return aOutcome == application::agent_run_outcome::success  ? core::queued_finish::went
				: aOutcome == application::agent_run_outcome::cancelled ? core::queued_finish::stopped
																		: core::queued_finish::failed;
		}
	} // namespace

	auto github_application::implementation::tour_part_key(const core::tour_thread& aThread) const -> std::string
	{
		const auto session = core::parse_session_key(ask_session_key());
		return session ? core::session_key(core::tour_part_session(*session, aThread)) : std::string{ };
	}

	auto github_application::implementation::chat_queue_key(const core::tour_thread& aThread) const -> std::string
	{
		if (local_tour_shown()) {
			return core::session_key(core::local_draft_session(DraftDesktop->shown_draft(), aThread));
		}
		return tour_part_key(aThread);
	}

	/// Offered once, with the window's own runtime (`attach_agent_queue`).
	void github_application::implementation::bind_chat_queues()
	{
		if (ChatQueueRuntime || Closing || Window == nullptr || Window->tour_editor() == nullptr || !AgentQueue) {
			return;
		}
		ChatQueueRuntime = AgentRunner.offer(application::agent_session_runtime{
			.Runs  = [this](const std::string& aSession) { return chat_queue_runs(aSession); },
			.State = [this](const std::string& aSession) { return chat_queue_agent(aSession); },
			.Start = [this](const std::string& aSession, const core::queued_entry& aEntry) { return release_chat_queued(aSession, aEntry); },
			.Steer = [this](const std::string& aSession, const core::queued_entry& aEntry) { return steer_chat_queued(aSession, aEntry); },
			.Bound = [this](const std::string& aSession) { return chat_queue_bound(aSession); },
		});
		if (DraftDesktop) {
			// A local draft's question started or ended: its own lane's, so
			// its end is the turn's real completion.
			DraftDesktop->set_asking_observer([this](const bool aAsking, const bool aFailed, const bool aStopped) {
				if (Closing) {
					return;
				}
				if (aAsking) {
					// [[BPR-2154]]. The question's session and number, so
					// what is steered into it settles when it ends.
					const auto asking = DraftDesktop->asking_question();
					LocalSteerSession = asking ? chat_queue_key(asking->first) : std::string{ };
					LocalSteerRun     = asking ? asking->second : 0U;
					present_local_question_footer(asking.has_value());
					return;
				}
				present_local_question_footer(false);
				settle_local_draft_steers();
				const auto sending = core::parse_session_key(ChatQueueSending);
				if (sending && sending->Kind == core::queued_session_kind::local_draft_discussion) {
					chat_queue_retired(aStopped ? core::queued_finish::stopped : aFailed ? core::queued_finish::failed
																						 : core::queued_finish::went);
					return;
				}
				drain_chat_queues();
			});
			DraftDesktop->set_tour_observer([this] { present_chat_queues(); });
		}
		present_chat_queues();
	}

	/// [[BPR-2154]], RFC-BPR-0126 AC09. A local draft's discussion question
	/// stands in the footer's Tour chat slot while it runs, as a pull
	/// request's tour chat turn does: Stop all stops it too.
	void github_application::implementation::present_local_question_footer(const bool aRunning)
	{
		auto* const footer = Window != nullptr ? Window->agent_footer_widget() : nullptr;
		if (footer == nullptr || (!aRunning && !std::exchange(LocalQuestionInFooter, false))) {
			return;
		}
		if (!aRunning) {
			footer->present_run(qt::agent_footer_place::tour_chat, std::nullopt);
			if (Window->tour_editor() != nullptr) {
				Window->tour_editor()->present_run_status(std::nullopt);
			}
			return;
		}
		LocalQuestionInFooter = true;
		LocalQuestionStarted.start();
		auto status  = qt::agent_run_status{ };
		status.Agent = DraftDesktop ? QString::fromStdString(DraftDesktop->agent_name()) : QString{ };
		if (status.Agent.isEmpty()) {
			status.Agent = QStringLiteral("The tour agent");
		}
		status.ActivityStartedMs = qt::agent_activity_clock_ms();
		// The chat's running line names the agent, as a pull request's does.
		if (Window->tour_editor() != nullptr) {
			Window->tour_editor()->present_run_status(status);
		}
		footer->present_run(qt::agent_footer_place::tour_chat, qt::agent_footer_run{ .Status = std::move(status), .Started = LocalQuestionStarted, .Stop = [this, weak = std::weak_ptr<const int>{ WindowLifetime }] {
																						if (!weak.expired() && !Closing && DraftDesktop) {
																							DraftDesktop->tour_stop();
																						}
																					} });
	}

	/// Every open part chat's pending list, from the host's queues. The chat
	/// starts over when it shows another pull request's tour or another
	/// draft: part names are only unique within one.
	void github_application::implementation::present_chat_queues()
	{
		if (Closing || Window == nullptr || Window->tour_editor() == nullptr || !AgentQueue) {
			return;
		}
		auto* const chat  = Window->tour_editor()->side_chat();
		const auto  local = local_tour_shown();
		const auto  owner = local ? "local:" + DraftDesktop->shown_draft() : ask_session_key();
		if (owner != ChatQueueOwner) {
			ChatQueueOwner = owner;
			ChatQueueSeen.clear();
			chat->set_queue_dispatch([this](const core::tour_thread& aThread) { return chat_queue_dispatch(aThread); });
		}
		const auto ready = local ? DraftDesktop->agent_ready() : writing_agent().has_value();
		for (const auto& thread : chat->open_threads()) {
			const auto key = chat_queue_key(thread);
			if (key.empty()) {
				continue;
			}
			const auto* const queue = AgentQueue->queue(key);
			if (queue == nullptr) {
				chat->present_queue(thread, { }, { }, false);
				continue;
			}
			ChatQueueSeen[key] = queue->Revision;
			auto rows          = std::vector<qt::agent_pending_row>{ };
			for (const auto& entry : queue->Entries) {
				if (!core::listed(entry)) {
					continue;
				}
				rows.push_back({
					.Id        = entry.Id,
					.Label     = QString::fromStdString(entry.Input.Text.substr(0U, entry.Input.Text.find('\n'))),
					.Text      = QString::fromStdString(entry.Input.Text),
					.Held      = entry.Held,
					.Sending   = entry.State != core::queued_state::queued,
					.Uncertain = entry.State == core::queued_state::delivery_unknown,
					.Sent      = entry.State == core::queued_state::sent,
				});
			}
			chat->present_queue(thread, std::move(rows), queue_words(*queue, ready), queue->Paused != core::queue_pause::none);
		}
	}

	/// The pending list's controls for one part, on that part's queue as it
	/// is named at the press. A press on what another window changed first
	/// is refused and says so.
	auto github_application::implementation::chat_queue_dispatch(const core::tour_thread& aThread) -> qt::agent_pending_dispatch
	{
		// The chat shows this part: its queue as the host has it now.
		QTimer::singleShot(0, Window.get(), [this, weak = std::weak_ptr<const int>{ WindowLifetime }] {
			if (!weak.expired()) {
				present_chat_queues();
			}
		});
		const auto onQueue = [this, aThread](auto aAct) {
			return [this, aThread, aAct](auto... aArgs) {
				const auto key = chat_queue_key(aThread);
				if (Closing || Window == nullptr || !AgentQueue || key.empty()) {
					return;
				}
				const auto seen    = ChatQueueSeen.contains(key) ? std::optional{ ChatQueueSeen[key] } : std::nullopt;
				const auto refused = aAct(*AgentQueue, key, seen, aArgs...);
				if (refused == core::queue_refusal::stale || refused == core::queue_refusal::unknown_entry || refused == core::queue_refusal::not_editable) {
					Window->tour_editor()->show_writing_notice(QStringLiteral("Another window changed this queue first, so nothing changed here."));
					AgentQueue->show(key);
				}
				present_chat_queues();
			};
		};
		using seen = std::optional<std::uint64_t>;
		using host = application::agent_queue_host;
		return qt::agent_pending_dispatch{
			.Edit = onQueue([](host& aQueue, const std::string& aKey, const seen aSeen, const std::uint64_t aId) { return aQueue.hold(aKey, aId, aSeen); }),
			.Save = onQueue([](host& aQueue, const std::string& aKey, seen, const std::uint64_t aId, const QString& aText) {
				return aQueue.release_hold(aKey, aId, aText.trimmed().isEmpty() ? std::nullopt : std::optional{ aText.toStdString() });
			}),
			.Cancel = onQueue([](host& aQueue, const std::string& aKey, seen, const std::uint64_t aId) { return aQueue.release_hold(aKey, aId, std::nullopt); }),
			.Remove = onQueue([](host& aQueue, const std::string& aKey, const seen aSeen, const std::uint64_t aId) { return aQueue.withdraw(aKey, aId, aSeen); }),
			.Move = onQueue([](host& aQueue, const std::string& aKey, const seen aSeen, const std::uint64_t aId, const int aBy) { return aQueue.reorder(aKey, aId, aBy, aSeen); }),
			.Steer = [this, aThread](const std::uint64_t aId) {
                const auto key = chat_queue_key(aThread);
                if (Closing || Window == nullptr || !AgentQueue || key.empty()) {
                    return;
                }
                const auto lastSeen = ChatQueueSeen.contains(key) ? std::optional{ ChatQueueSeen[key] } : std::nullopt;
                if (AgentQueue->steer(key, aId, lastSeen, [this, weak = std::weak_ptr<const int>{ WindowLifetime }](const application::agent_queue_steer_result& aResult) {
                        if (!weak.expired() && !Closing && Window != nullptr) {
                            Window->tour_editor()->show_writing_notice(QString::fromStdString(aResult.Sent ? std::string{ application::agent_steered_words } : aResult.Why));
                        }
                    })
                    != core::queue_refusal::none) {
                    Window->tour_editor()->show_writing_notice(QStringLiteral("Another window changed this queue first, so nothing changed here."));
                } },
			.Pause = onQueue([](host& aQueue, const std::string& aKey, seen) {
				aQueue.pause(aKey, core::queue_pause::reviewer);
				return core::queue_refusal::none;
			}),
			.Resume = onQueue([](host& aQueue, const std::string& aKey, const seen aSeen) { return aQueue.resume(aKey, aSeen); }),
			.Discard = onQueue([](host& aQueue, const std::string& aKey, const seen aSeen, const std::uint64_t aId) { return aQueue.discard(aKey, aId, aSeen); }),
		};
	}

	/// Words typed in a part's chat while the tour agent runs, or while that
	/// part's queue holds messages: queued on the part's session; steered,
	/// the host then offers it to the part's running turn and says why when it
	/// goes next instead.
	void github_application::implementation::queue_chat_follow_up(const qt::tour_ask& aAsk)
	{
		const auto key = chat_queue_key(aAsk.Thread);
		if (Closing || Window == nullptr || !AgentQueue || key.empty() || !aAsk.Steer) {
			return;
		}
		const auto text  = aAsk.Question.trimmed().toStdString();
		auto       label = text.substr(0U, text.find('\n'));
		if (label.size() > 80U) {
			label = label.substr(0U, 79U) + "…";
		}
		if (const auto refused = AgentQueue->enqueue(key, core::queued_input{ .Scope = "message", .Label = std::move(label), .Text = text }); refused != core::queue_refusal::none) {
			auto* const composer = Window->tour_editor()->side_chat()->composer();
			if (composer->toPlainText().trimmed().isEmpty()) {
				composer->setPlainText(aAsk.Question);
			}
			Window->tour_editor()->show_writing_notice(refused == core::queue_refusal::full || refused == core::queue_refusal::too_large ? QStringLiteral("The queue is full: nothing more was queued.")
														   : refused == core::queue_refusal::not_saved                                   ? QStringLiteral("This message couldn't be saved to the queue, so it wasn't queued.")
																																		 : QStringLiteral("That message can't be queued."));
			return;
		}
		if (const auto* const queue = AgentQueue->queue(key); *aAsk.Steer && queue != nullptr && !queue->Entries.empty()) {
			static_cast<void>(AgentQueue->steer(key, queue->Entries.back().Id, std::nullopt, [this, weak = std::weak_ptr<const int>{ WindowLifetime }](const application::agent_queue_steer_result& aResult) {
				if (!weak.expired() && !Closing && Window != nullptr) {
					Window->tour_editor()->show_writing_notice(QString::fromStdString(aResult.Sent ? std::string{ application::agent_steered_words } : aResult.Why));
				}
			}));
		}
		present_chat_queues();
	}

	auto github_application::implementation::chat_queue_runs(const std::string& aSession) const -> bool
	{
		if (Closing || Window == nullptr) {
			return false;
		}
		const auto session = core::parse_session_key(aSession);
		if (!is_chat_session(session)) {
			return false;
		}
		if (session->Kind == core::queued_session_kind::local_draft_discussion) {
			return DraftDesktop && DraftDesktop->shown_draft() == session->Draft;
		}
		return !local_tour_shown() && TourHost && TourHost->presentation().Draft && core::session_key(core::with_kind(*session, core::queued_session_kind::agent_tab)) == ask_session_key();
	}

	/// What the part's agent can take now. One tour host runs one tour run
	/// at a time, and one local draft's lane one question: any run keeps
	/// every part's queue waiting; the first draft's feedback goes first.
	auto github_application::implementation::chat_queue_agent(const std::string& aSession) const -> core::queue_agent
	{
		const auto session = core::parse_session_key(aSession);
		if (!chat_queue_runs(aSession)) {
			return core::queue_agent::not_set_up;
		}
		if (session->Kind == core::queued_session_kind::local_draft_discussion) {
			if (!DraftDesktop->agent_ready()) {
				return core::queue_agent::not_set_up;
			}
			return DraftDesktop->asking() || !ChatQueueSending.empty() ? core::queue_agent::busy : core::queue_agent::idle;
		}
		if (!writing_agent()) {
			return core::queue_agent::not_set_up;
		}
		if (TourDraftPending || TourAskPending || TourQueueSending || TourFeedbackQueued || !ChatQueueSending.empty() || !tour_session_idle()) {
			return core::queue_agent::busy;
		}
		return core::queue_agent::idle;
	}

	/// A part's next message, as its chat would send it now: the part's
	/// conversation, with the tour (or the draft's candidate) as it is when
	/// it goes; its run's end settles it.
	auto github_application::implementation::release_chat_queued(const std::string& aSession, const core::queued_entry& aEntry) -> application::queued_admission
	{
		const auto session = core::parse_session_key(aSession);
		const auto thread  = session ? core::tour_part_thread(session->Part) : std::nullopt;
		if (Closing || Window == nullptr || !thread || aEntry.Input.Text.empty()) {
			return { };
		}
		ChatQueueSending = aSession;
		ChatQueueFinish.reset();
		if (session->Kind == core::queued_session_kind::local_draft_discussion) {
			DraftDesktop->tour_ask(qt::tour_ask{ .Thread = *thread, .Question = QString::fromStdString(aEntry.Input.Text) });
			if (!DraftDesktop->asking()) {
				// Nothing started: it goes back first and the queue pauses.
				ChatQueueSending.clear();
				return { .Kind = application::queued_admission_kind::refused };
			}
			return chat_queue_began(aSession);
		}
		request_tour_ask(*thread, std::nullopt, QString::fromStdString(aEntry.Input.Text), false);
		if (!TourAskPending) {
			// It could not run: the part's chat holds it as a turn that says
			// why, with the message as written, so it was answered.
			ChatQueueSending.clear();
			return { .Kind = application::queued_admission_kind::answered };
		}
		return chat_queue_began(aSession);
	}

	/// [[BPR-2368]]. A part's released message is the chat's running turn
	/// now (the tour's ask, or the draft's question, shows it): it leaves the
	/// pending list (`core::begin_turn`) and still holds the session until
	/// its run's end settles it.
	auto github_application::implementation::chat_queue_began(const std::string& aSession) -> application::queued_admission
	{
		const auto run = ++ChatQueueRun;
		if (ChatQueueRuntime) {
			AgentRunner.began(*ChatQueueRuntime, aSession, run);
		}
		return { .Kind = application::queued_admission_kind::started, .Run = run };
	}

	/// A queued message offered to its part's running turn goes to the tour
	/// run's kept connection, as the Agent tab's does; another part's turn,
	/// or a local draft's one-shot question, takes none, and it goes next.
	auto github_application::implementation::steer_chat_queued(const std::string& aSession, const core::queued_entry& aEntry) -> std::optional<std::string>
	{
		const auto session = core::parse_session_key(aSession);
		if (session && session->Kind == core::queued_session_kind::local_draft_discussion && LocalDraftEntryProcess) {
			// [[BPR-2154]]. A local draft's question runs on a kept
			// connection named by its draft and number: the correction
			// goes there, answered back on this lane.
			const auto target = [draft = session->Draft](const application::agent_invocation_projection_identity& aRun) {
				return application::agent_steer_request{ .Conversation = application::local_draft_native_conversation(draft, aRun.InvocationGeneration),
														 .Run          = core::agent_run_id{ aRun.InvocationGeneration },
														 .NativeTurn   = 2U };
			};
			auto steer = application::agent_runtime_steer({
				.Support  = [this, target](const application::agent_invocation_projection_identity& aRun) { return LocalDraftEntryProcess ? LocalDraftEntryProcess->steer_support(target(aRun)) : core::agent_steer_support::not_running; },
				.Steer    = [this, target](const application::agent_invocation_projection_identity& aRun, std::string aText, application::agent_invocation_host::steer_answer aAnswer) {
					auto request = target(aRun);
					request.Text = std::move(aText);
					if (!LocalDraftEntryProcess) {
						if (aAnswer) {
							aAnswer({ .Support = core::agent_steer_support::not_running });
						}
						return;
					}
					LocalDraftEntryProcess->steer(std::move(request), [window = QPointer<QObject>{ Window.get() }, weak = std::weak_ptr<const int>{ WindowLifetime },
																		answer = std::move(aAnswer)](const application::agent_steer_receipt& aReceipt) {
						if (window.isNull()) {
							return;
						}
						QMetaObject::invokeMethod(
							window.data(), [weak, answer, aReceipt] {
								if (!weak.expired() && answer) {
									answer(aReceipt);
								}
							},
							Qt::QueuedConnection);
					}); },
				.Queue    = AgentQueue.get(),
				.Shown    = [this, aSession] { return local_steer_shown(aSession); },
				.Returned = [this, weak = std::weak_ptr<const int>{ WindowLifetime }](const std::string&, const std::string& aWhy) {
					if (!weak.expired() && !Closing && Window != nullptr) {
						Window->tour_editor()->show_writing_notice(QString::fromStdString(aWhy));
					} },
			});
			return steer(aSession, aEntry);
		}
		if (!session || session->Kind != core::queued_session_kind::tour_part || !TourSession) {
			return application::agent_steer_goes_next_words(core::agent_steer_support::no_connection);
		}
		auto steer = application::agent_runtime_steer({
			.Support  = [this](const application::agent_invocation_projection_identity& aRun) { return TourSession ? TourSession->steer_support(aRun) : core::agent_steer_support::not_running; },
			.Steer    = [this](const application::agent_invocation_projection_identity& aRun, std::string aText, application::agent_invocation_host::steer_answer aAnswer) {
                if (TourSession) {
                    TourSession->steer(aRun, std::move(aText), std::move(aAnswer));
                } else if (aAnswer) {
                    aAnswer({ .Support = core::agent_steer_support::not_running });
                } },
			.Queue    = AgentQueue.get(),
			.Shown    = [this, aSession]() -> std::optional<application::agent_invocation_projection_identity> {
                const auto thread = running_tour_thread();
                return thread && tour_part_key(*thread) == aSession ? TourInvocation : std::nullopt; },
			.Returned = [this, weak = std::weak_ptr<const int>{ WindowLifetime }](const std::string&, const std::string& aWhy) {
				if (!weak.expired() && !Closing && Window != nullptr) {
					Window->tour_editor()->show_writing_notice(QString::fromStdString(aWhy));
				} },
		});
		return steer(aSession, aEntry);
	}

	auto github_application::implementation::local_steer_shown(const std::string& aSession) const -> std::optional<application::agent_invocation_projection_identity>
	{
		const auto asking = DraftDesktop ? DraftDesktop->asking_question() : std::nullopt;
		if (Closing || !asking || chat_queue_key(asking->first) != aSession) {
			return std::nullopt;
		}
		return application::agent_invocation_projection_identity{ .InvocationGeneration = asking->second };
	}

	/// [[BPR-2154]]. A local draft question ended: what was steered into
	/// it settles by its receipts before anything else is released.
	void github_application::implementation::settle_local_draft_steers()
	{
		const auto session = std::exchange(LocalSteerSession, std::string{ });
		const auto run     = std::exchange(LocalSteerRun, 0U);
		if (!AgentQueue || !DraftDesktop || session.empty() || run == 0U || AgentQueue->queue(session) == nullptr) {
			return;
		}
		const auto [answered, steered] = DraftDesktop->answered_steers();
		auto endings                   = std::vector<core::steered_settlement>{ };
		if (answered == run) {
			for (const auto& input : steered) {
				const auto ending = input.Disposition == core::agent_steer_disposition::consumed                                                ? core::steered_ending::consumed
					: input.Disposition == core::agent_steer_disposition::refused || input.Disposition == core::agent_steer_disposition::unsent ? core::steered_ending::not_taken
																																				: core::steered_ending::unknown;
				endings.push_back({ .Receipt = input.Identity, .Ending = ending });
			}
		}
		AgentQueue->settle_steers(session, run, endings);
	}

	auto github_application::implementation::chat_queue_bound(const std::string& aSession) const -> core::queued_claim
	{
		if (const auto local = local_steer_shown(aSession)) {
			return { .Run = local->InvocationGeneration };
		}
		const auto thread = running_tour_thread();
		if (!thread || !TourInvocation || tour_part_key(*thread) != aSession) {
			return { };
		}
		return { .Run = TourInvocation->InvocationGeneration };
	}

	auto github_application::implementation::running_tour_thread() const -> std::optional<core::tour_thread>
	{
		if (!TourRun) {
			return std::nullopt;
		}
		const auto* const run = core::find_tour_run(TourAgent, *TourRun);
		return run != nullptr ? std::optional{ run->Thread } : std::nullopt;
	}

	/// [[BPR-2152]] (AC06). The corrections claimed for a tour part's run
	/// settle by their receipts before its retirement releases anything; and
	/// the released message's run says how it ended.
	void github_application::implementation::settle_tour_part_steers(const application::agent_invocation_projection_identity& aIdentity, const application::agent_run_result& aResult)
	{
		const auto thread = running_tour_thread();
		if (!thread || !AgentQueue) {
			return;
		}
		const auto key = tour_part_key(*thread);
		if (key.empty()) {
			return;
		}
		if (key == ChatQueueSending) {
			ChatQueueFinish = finish_of(aResult.Outcome);
		}
		if (AgentQueue->queue(key) == nullptr) {
			return;
		}
		auto endings = std::vector<core::steered_settlement>{ };
		for (const auto& steered : aResult.SteeredInputs) {
			const auto ending = steered.Disposition == core::agent_steer_disposition::consumed                                                  ? core::steered_ending::consumed
				: steered.Disposition == core::agent_steer_disposition::refused || steered.Disposition == core::agent_steer_disposition::unsent ? core::steered_ending::not_taken
																																				: core::steered_ending::unknown;
			endings.push_back({ .Receipt = steered.Identity, .Ending = ending });
		}
		AgentQueue->settle_steers(key, aIdentity.InvocationGeneration, endings);
	}

	/// The released message's run has really ended (the tour host retired,
	/// or the draft's question ended): it settles, and every part may go on.
	void github_application::implementation::chat_queue_retired(const std::optional<core::queued_finish> aFinish)
	{
		if (Closing || !ChatQueueRuntime) {
			return;
		}
		if (!ChatQueueSending.empty() && aFinish) {
			const auto key = std::exchange(ChatQueueSending, std::string{ });
			ChatQueueFinish.reset();
			AgentRunner.retired(*ChatQueueRuntime, key, ChatQueueRun, *aFinish);
		}
		drain_chat_queues();
	}

	/// Tells the host each part with waiting messages that the agent may
	/// take one now; the host decides.
	void github_application::implementation::drain_chat_queues()
	{
		if (Closing || Window == nullptr || !AgentQueue || !ChatQueueSending.empty()) {
			return;
		}
		auto threads = std::vector<core::tour_thread>{ core::tour_thread{ } };
		if (local_tour_shown()) {
			for (const auto& thread : Window->tour_editor()->side_chat()->open_threads()) {
				threads.push_back(thread);
			}
		} else if (TourHost && TourHost->presentation().Draft) {
			for (const auto& stop : TourHost->presentation().Draft->Stops) {
				threads.push_back({ .Kind = core::tour_thread_kind::stop, .Stop = stop.Id });
			}
		}
		for (const auto& thread : threads) {
			const auto key = chat_queue_key(thread);
			if (const auto* const queue = key.empty() ? nullptr : AgentQueue->queue(key); queue != nullptr && !queue->Entries.empty()) {
				AgentQueue->agent_changed(key);
				if (!ChatQueueSending.empty()) {
					// One started: the others wait for its end.
					return;
				}
			}
		}
	}
#endif
} // namespace bpr::app
