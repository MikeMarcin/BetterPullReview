/// \file
/// \brief RFC-BPR-0128 ([[BPR-2163]], [[BPR-2164]]). The Ask menus in the
/// window: the facts each menu is decided on, read from what the window
/// already holds (no request, no agent call), and where a press goes. Every
/// press goes through the existing launch paths of RFC-BPR-0101: a quick ask
/// is the suggested prompt's own send, and an action row is the action's own.

#include "github_application/window_composition.hpp"

#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/core/prompt_surfaces.hpp"
#include "bpr/core/quick_ask.hpp"
#include "bpr/qt/agent_footer.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QTimer>

#include <cstdio>

#include <algorithm>

namespace bpr::app {
#if defined(BPR_AGENT_INTEGRATIONS)
	namespace {
		/// Commits listed after `aCommit`, or none when it is not listed.
		[[nodiscard]] auto commits_after(const std::vector<application::github_commit_record>& aCommits, const std::string& aCommit) -> std::optional<std::uint32_t>
		{
			if (aCommit.empty()) {
				return std::nullopt;
			}
			const auto found = std::ranges::find_if(aCommits, [&aCommit](const application::github_commit_record& aRecord) { return aRecord.Oid.value() == aCommit; });
			if (found == aCommits.end()) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(std::distance(found, aCommits.end()) - 1);
		}
	} // namespace

	void github_application::implementation::bind_quick_ask()
	{
		if (Window == nullptr || Window->agent_tab() == nullptr) {
			return;
		}
		Window->set_quick_ask([this](const qt::ask_menu_request& aRequest) { return quick_ask_menu(aRequest); },
							  qt::ask_menu_dispatch{
								  .Ask    = [this](const qt::ask_menu_request& aRequest, const core::quick_ask_row& aRow) { queue_quick_ask(aRequest, aRow); },
								  .Draft  = [this](const qt::ask_menu_request& aRequest, const core::quick_ask_row& aRow) { draft_quick_ask(aRequest, aRow); },
								  .Custom = [this](const qt::ask_menu_request& aRequest) { ask_custom_question(aRequest); },
								  .Edit   = [this] { Window->reveal_prompt_macros(); },
							  });
		if (!AgentQueue) {
			AgentQueueStore      = adapters::sqlite::make_agent_queue_store(ApplicationDataRoot);
			auto dependencies    = application::agent_queue_dependencies{ };
			dependencies.Store   = AgentQueueStore.get();
			dependencies.Release = [this](const std::string&, const core::queued_entry& aEntry) { return release_queued(aEntry); };
			dependencies.Changed = [this](const std::string& aSession, const core::agent_queue& aQueue) {
				// RFC-BPR-0126 AC09: Stop all shows while any session's queue
				// holds a message.
				if (!Closing && Window != nullptr) {
					QTimer::singleShot(0, Window.get(), [this] {
						if (!Closing && Window != nullptr && Window->agent_footer_widget() != nullptr && AgentQueue) {
							Window->agent_footer_widget()->present_queued(AgentQueue->queued_count());
						}
					});
				}
				if (Closing || Window == nullptr || aSession != ask_session_key()) {
					return;
				}
				auto waiting = QStringList{ };
				for (const auto& entry : aQueue.Entries) {
					if (entry.State == core::queued_state::queued) {
						waiting << QString::fromStdString(entry.Input.Label);
					}
				}
				const auto state = aQueue.Paused != core::queue_pause::none                      ? QStringLiteral("paused")
					: Window->agent_tab()->setup_readiness() != qt::agent_setup_readiness::ready ? QStringLiteral("waiting for the agent's setup")
																								 : QStringLiteral("waiting its turn");
				Window->agent_tab()->present_queue(waiting, state);
				// RFC-BPR-0126 ([[BPR-2151]]): the pending list with its
				// controls; a paused queue says why.
				auto rows = std::vector<qt::agent_tab_view::queue_row>{ };
				for (const auto& entry : aQueue.Entries) {
					rows.push_back({
						.Id = entry.Id,
						// A typed message reads as its text, edited or not.
						.Label   = QString::fromStdString(entry.Input.QuickAsk.empty() && !entry.Input.Text.empty() ? entry.Input.Text.substr(0U, entry.Input.Text.find('\n')) : entry.Input.Label),
						.Text    = QString::fromStdString(entry.Input.QuickAsk.empty() ? entry.Input.Text : std::string{ }),
						.Held    = entry.Held,
						.Sending = entry.State != core::queued_state::queued,
					});
				}
				const auto why = aQueue.Paused == core::queue_pause::reviewer ? QStringLiteral("Paused")
					: aQueue.Paused == core::queue_pause::stopped             ? QStringLiteral("Paused: Stop all")
					: aQueue.Paused == core::queue_pause::failed              ? QStringLiteral("Paused: a run failed")
					: aQueue.Paused == core::queue_pause::usage_limit         ? QStringLiteral("Paused: the agent's usage limit")
					: aQueue.Paused == core::queue_pause::restarted           ? QStringLiteral("Paused since the app restarted")
					: aQueue.Paused == core::queue_pause::delivery_unknown    ? QStringLiteral("Paused: a message may not have been delivered")
					: aQueue.Paused == core::queue_pause::changed_target      ? QStringLiteral("Paused: what a message is about has changed")
																			  : QStringLiteral("Queued, %1").arg(state);
				Window->agent_tab()->present_queue_rows(std::move(rows), why, aQueue.Paused != core::queue_pause::none);
			};
			AgentQueue = std::make_unique<application::agent_queue_host>(std::move(dependencies));
			AgentQueue->load();
			// RFC-BPR-0126 AC09: Stop all, after the footer stopped every run,
			// pauses every queue, so nothing starts next.
			if (auto* const footer = Window->agent_footer_widget(); footer != nullptr) {
				footer->set_stop_all_dispatch([this] {
					if (!Closing && AgentQueue) {
						AgentQueue->pause_all(core::queue_pause::stopped);
					}
				});
			}
		}
		Window->agent_tab()->set_availability_observer([this](const bool aSettled) { observe_ask_queue(aSettled); });
		auto* const tab = Window->agent_tab();
		// RFC-BPR-0126 ([[BPR-2151]]). The pending list's controls, on the
		// session's one queue.
		const auto onQueue = [this](auto aAct) {
			return [this, aAct](auto... aArgs) {
				if (!Closing && AgentQueue && !ask_session_key().empty()) {
					static_cast<void>(aAct(*AgentQueue, ask_session_key(), aArgs...));
					observe_ask_queue(false);
				}
			};
		};
		tab->set_queue_dispatch(qt::agent_tab_view::queue_dispatch{
			.Edit   = onQueue([](application::agent_queue_host& aQueue, const std::string& aSession, const std::uint64_t aId) { return aQueue.hold(aSession, aId); }),
			.Save   = onQueue([](application::agent_queue_host& aQueue, const std::string& aSession, const std::uint64_t aId, const QString& aText) {
				return aQueue.release_hold(aSession, aId, aText.trimmed().isEmpty() ? std::nullopt : std::optional{ aText.toStdString() });
			}),
			.Cancel = onQueue([](application::agent_queue_host& aQueue, const std::string& aSession, const std::uint64_t aId) { return aQueue.release_hold(aSession, aId, std::nullopt); }),
			.Remove = onQueue([](application::agent_queue_host& aQueue, const std::string& aSession, const std::uint64_t aId) { return aQueue.withdraw(aSession, aId); }),
			.Move   = onQueue([](application::agent_queue_host& aQueue, const std::string& aSession, const std::uint64_t aId, const int aBy) { return aQueue.reorder(aSession, aId, aBy); }),
			// Look prompts-v003: a queued message's Steer. No driver steers
			// a running turn yet ([[BPR-2153]]), so, as a typed steer does
			// (Q17), it goes next instead, ahead of the rest, and says why.
			.Steer = [this, onQueue](const std::uint64_t aId) {
				onQueue([](application::agent_queue_host& aQueue, const std::string& aSession, const std::uint64_t aEntry) {
					return aQueue.reorder(aSession, aEntry, -static_cast<int>(core::maximum_queued_inputs));
				})(aId);
				if (!Closing && Window != nullptr) {
					Window->agent_panel()->refuse("Steer isn't available for this agent yet, so the message goes next, right after this answer.");
				}
			},
			.Pause  = onQueue([](application::agent_queue_host& aQueue, const std::string& aSession) {
				aQueue.pause(aSession, core::queue_pause::reviewer);
				return core::queue_refusal::none;
			}),
			.Resume = onQueue([](application::agent_queue_host& aQueue, const std::string& aSession) { return aQueue.resume(aSession); }),
		});
		// Q4/Q5 as answered: a typed message while a turn runs or messages
		// wait is queued on the session (Steer is not built: a driver that
		// cannot steer keeps Queue and says so, Q17).
		tab->set_follow_up([this](const QString& aText, const bool aSteer) {
			const auto session = ask_session_key();
			if (Closing || Window == nullptr || !AgentQueue || session.empty()) {
				return false;
			}
			auto label = aText.section(QLatin1Char('\n'), 0, 0).trimmed();
			if (label.size() > 80) {
				label = label.left(79) + QStringLiteral("\u2026");
			}
			auto input = core::queued_input{ .Scope = "message", .File = Window->active_document_path().toStdString(), .Label = label.toStdString(), .Text = aText.toStdString() };
			if (AskedConversation) {
				input.Thread = AskedConversation->Identity;
			}
			if (const auto refused = AgentQueue->enqueue(session, std::move(input)); refused != core::queue_refusal::none) {
				Window->agent_panel()->refuse(refused == core::queue_refusal::full || refused == core::queue_refusal::too_large ? "The queue is full: nothing more was queued."
																																: "That message can't be queued.");
				return false;
			}
			if (aSteer) {
				Window->agent_panel()->refuse("Steer isn't available for this agent yet, so the message was queued.");
			}
			observe_ask_queue(false);
			return true;
		});
		// RFC-BPR-0128 ([[BPR-2166]]). A report card's Open: the report as a
		// tab beside the files; its references go where the Agent tab's go.
		tab->set_report_dispatch([this](const qt::agent_report_document& aReport) {
			if (!Closing && Window != nullptr) {
				Window->open_report("turn:" + std::to_string(aReport.Turn), aReport);
			}
		});
		if (auto* const report = Window->report_view(); report != nullptr) {
			report->set_link_dispatch([tab](const QString& aLink) { tab->open_reference(aLink); });
			report->set_dresser([tab](qt::markdown_body_view& aView) { tab->dress_answer_view(aView); });
			// docs/design/report-templates-v2: a suggestion's Add as draft
			// comment drafts it at its lines as a finding card's does. Nothing
			// is published.
			report->set_draft_dispatch([this](const qt::report_draft& aDraft, qt::report_draft_done aDone) {
				auto request                     = qt::agent_finding_card_request{ };
				request.Finding.Path             = aDraft.Path.toStdString();
				request.Finding.Lines            = { .Begin = aDraft.First > 0U ? aDraft.First - 1U : 0U, .End = aDraft.Last };
				request.Finding.Claim            = aDraft.Comment.toStdString();
				request.Finding.SuggestedComment = aDraft.Comment.toStdString();
				request.Action                   = qt::agent_finding_card_action::draft_comment;
				act_on_agent_card(request, [aDone](const qt::agent_finding_card_state aState, const QString& aWhy, std::uint64_t) {
					if (aState == qt::agent_finding_card_state::drafted) {
						aDone(true, { });
					} else if (aState == qt::agent_finding_card_state::refused) {
						aDone(false, aWhy);
					}
				});
			});
			// docs/design/report-card-v4: the question under the title and
			// Show in conversation: the Agent tab at the turn, the report's tab
			// staying open and active.
			report->set_conversation_dispatch([this, tab](const std::size_t aTurn) {
				if (!Closing && Window != nullptr) {
					Window->open_agent_question();
					tab->reveal_turn(aTurn);
				}
			});
		}
		tab->set_claim_observer([this](const std::string& aThread, const core::addressed_claim aClaim, const std::size_t aTurn) { record_thread_claim(aThread, aClaim, aTurn); });
		if (Window->conversation_panel() != nullptr) {
			Window->conversation_panel()->cards()->set_claim_answer_dispatch([this, tab](const std::size_t aTurn) {
				if (!Closing && Window != nullptr) {
					Window->open_agent_question();
					tab->reveal_turn(aTurn);
				}
			});
		}
	}

	auto github_application::implementation::quick_ask_facts(const qt::ask_menu_request& aRequest) const -> core::quick_ask_facts
	{
		auto  facts = core::quick_ask_facts{ };
		auto* tab   = Window != nullptr ? Window->agent_tab() : nullptr;
		if (tab != nullptr) {
			const auto context       = tab->suggestion_context_now();
			facts.Base.PullRequest   = context.PullRequest;
			facts.Base.Repository    = context.Repository;
			facts.Base.Draft         = context.Draft;
			facts.Base.AuthorIsMe    = context.AuthorIsMe;
			facts.Base.ChecksFailing = context.ChecksFailing;
			facts.Base.ChatEmpty     = tab->turn_count() == 0U;
			facts.Agent              = tab->setup_readiness() != qt::agent_setup_readiness::ready ? core::quick_ask_agent::not_set_up
				: tab->state() == qt::agent_tab_state::working                                    ? core::quick_ask_agent::busy
																								  : core::quick_ask_agent::ready;
		}
		if (!aRequest.File.isEmpty()) {
			facts.File = aRequest.File.toStdString();
		}
		if (!aRequest.Thread.isEmpty()) {
			facts.Thread = aRequest.Thread.toStdString();
		}
		facts.Reviewed       = ReturningReview.Earlier.has_value();
		facts.LatestUpload   = ReturningReview.Offer.State;
		const auto& snapshot = WindowRead != nullptr ? WindowRead->accepted_snapshot() : std::nullopt;
		if (!snapshot) {
			return facts;
		}
		if (ReturningReview.Earlier) {
			facts.CommitsSinceReview = commits_after(snapshot->Commits, ReturningReview.Earlier->Head).value_or(0U);
		}
		for (const auto& file : snapshot->Files) {
			facts.Files.push_back(file.Path);
		}
		const auto head = std::string{ snapshot->PullRequest.HeadOid.value() };
		for (const auto& thread : snapshot->ReviewThreads) {
			auto entry         = core::quick_ask_thread{ .Identity = thread_identity(thread), .Path = thread.Path };
			entry.TakesReplies = thread.ViewerReplyPermission != application::github_permission_state::denied;
			// Q6: the baseline is the commit the thread started on.
			if (!thread.Comments.empty()) {
				const auto& first    = thread.Comments.front().Observation;
				const auto  baseline = !first.OriginalCommit.empty() ? first.OriginalCommit : first.Commit;
				if (!baseline.empty()) {
					entry.Changed      = baseline != head;
					entry.CommitsSince = commits_after(snapshot->Commits, baseline);
				}
			}
			facts.Threads.push_back(std::move(entry));
		}
		return facts;
	}

	auto github_application::implementation::quick_ask_menu(const qt::ask_menu_request& aRequest) const -> std::optional<core::quick_ask_menu>
	{
		if (Closing || Window == nullptr || Window->agent_tab() == nullptr) {
			return std::nullopt;
		}
		return core::decide_quick_asks(core::placed_prompt_macros(core::prompt_set_for_repository(PromptMacroSet, prompt_repository())), quick_ask_facts(aRequest), aRequest.Anchor);
	}

	/// A press queues the quick ask on its session (owner, 2026-11-14).
	void github_application::implementation::press_quick_ask(const qt::ask_menu_request& aRequest, const core::quick_ask_row& aRow) { queue_quick_ask(aRequest, aRow); }

	auto github_application::implementation::ask_session_key() const -> std::string
	{
		// The Agent tab's session of this pull request: the queue is the
		// session's, not the window's (RFC-BPR-0126 Q10).
		if (!AgentReview) {
			return { };
		}
		return "agent-tab:" + AgentReview->Hostname + "/" + AgentReview->Locator.Owner + "/" + AgentReview->Locator.Name + "#" + std::to_string(AgentReview->Number.value());
	}

	/// A plain click: each run the row asks (one a thread or file at a wider
	/// scope) is queued on the session; a wide press asks first (Q5).
	void github_application::implementation::queue_quick_ask(const qt::ask_menu_request& aRequest, const core::quick_ask_row& aRow)
	{
		const auto  menu    = quick_ask_menu(aRequest);
		const auto* row     = menu ? core::find_quick_ask(*menu, aRow.Id, aRow.Scope) : nullptr;
		const auto  session = ask_session_key();
		if (row == nullptr || session.empty() || !AgentQueue || Window == nullptr) {
			return;
		}
		if (!row->Enabled) {
			return;
		}
		const auto scope = std::string{ core::stable_name(row->Scope) };
		auto       items = row->Each;
		if (items.empty()) {
			items.emplace_back();
		}
		const auto byThread = row->Subject == core::prompt_subject::conversation;
		const auto byFile   = row->Subject == core::prompt_subject::open_file && row->Scope == core::prompt_scope::pull_request;
		// RFC-BPR-0128 as amended (look ask-v005): Was this addressed? at a
		// file or the pull request is one ask carrying every thread, its
		// answer one claim a thread.
		if (row->Action == core::prompt_action::was_addressed && row->Scope != core::prompt_scope::thread && !row->Each.empty()) {
			auto joined = std::string{ };
			for (const auto& item : row->Each) {
				joined += (joined.empty() ? "" : "\n") + item;
			}
			items = { joined };
		}
		for (const auto& item : items) {
			auto input = core::queued_input{ .QuickAsk = row->Id, .Scope = scope, .File = aRequest.File.toStdString(), .Thread = aRequest.Thread.toStdString(), .Label = row->Label };
			if (byThread && !item.empty()) {
				input.Thread = item;
			}
			if (byFile && !item.empty()) {
				input.File = item;
			}
			if (AgentQueue->enqueue(session, std::move(input)) != core::queue_refusal::none) {
				Window->open_agent_question();
				Window->agent_panel()->refuse("The queue is full: nothing more was queued.");
				break;
			}
		}
		Window->open_agent_question();
		if (Window->agent_tab()->setup_readiness() != qt::agent_setup_readiness::ready) {
			// Queued; the Agent tab offers its setup, and the ask runs after.
			Window->open_agent_setup();
		}
		observe_ask_queue(false);
	}

	/// Shift+click: the quick ask's message in the Agent tab's field, on its
	/// subject; nothing is sent.
	void github_application::implementation::draft_quick_ask(const qt::ask_menu_request& aRequest, const core::quick_ask_row& aRow)
	{
		if (Window == nullptr) {
			return;
		}
		auto* const tab = Window->agent_tab();
		if (aRow.Subject == core::prompt_subject::conversation && !aRow.Each.empty()) {
			ask_agent_about_thread(aRow.Each.front());
		} else if (aRow.Subject == core::prompt_subject::conversation && !aRequest.Thread.isEmpty()) {
			ask_agent_about_thread(aRequest.Thread.toStdString());
		} else {
			dismiss_asked_conversation();
			Window->open_agent_question();
		}
		if (aRow.Action == core::prompt_action::none) {
			tab->draft_prompt(QString::fromStdString(aRow.Id));
		} else {
			tab->draft_text(QString::fromStdString(aRow.Label));
		}
	}

	/// What the session's agent can take now; a settled turn first settles
	/// the entry it ran.
	void github_application::implementation::observe_ask_queue([[maybe_unused]] const bool aSettled)
	{
		if (Closing || Window == nullptr || !AgentQueue) {
			return;
		}
		const auto  session = ask_session_key();
		auto* const tab     = Window->agent_tab();
		if (session.empty() || tab == nullptr) {
			return;
		}
		const auto working = tab->state() == qt::agent_tab_state::working;
		// The run retires before the next input goes (RFC-BPR-0126: real
		// completion, not a raw result).
		const auto retired = AgentHost == nullptr || AgentHost->is_quiescent();
		if (AgentQueue->sending(session)) {
			if (working) {
				QueueTurnSeen = true;
			} else if (QueueTurnSeen && retired) {
				QueueTurnSeen = false;
				AgentQueue->settle(session, core::queue_pause::none);
			} else if (QueueTurnSeen && QueueRetireChecks < 100U) {
				// The turn ended and its run is still retiring: look again
				// shortly. This observes retirement; it never resends.
				++QueueRetireChecks;
				QTimer::singleShot(100, Window.get(), [this] { observe_ask_queue(false); });
				return;
			}
		}
		const auto agent = tab->setup_readiness() != qt::agent_setup_readiness::ready ? core::queue_agent::not_set_up
			: working || !retired || !tab->ready_to_send()                            ? core::queue_agent::busy
																					  : core::queue_agent::idle;
		AgentQueue->observe(session, agent);
	}

	/// Starts one queued entry's turn through the existing launch paths, and
	/// opens the Agent tab at it (owner look ask-v001, img 1: "just opening
	/// the agent drawer in the session and scrolling to the question and
	/// response").
	auto github_application::implementation::release_queued(const core::queued_entry& aEntry) -> bool
	{
		if (Closing || Window == nullptr) {
			return false;
		}
		auto* const tab = Window->agent_tab();
		// RFC-BPR-0126: a typed message, sent as typed on the subject it was
		// queued with.
		if (aEntry.Input.QuickAsk.empty()) {
			QueueTurnSeen     = false;
			QueueRetireChecks = 0U;
			const auto turns  = tab->turn_count();
			if (!aEntry.Input.Thread.empty()) {
				ask_agent_about_thread(aEntry.Input.Thread);
			}
			Window->open_agent_question();
			const auto refused = tab->send_released(QString::fromStdString(aEntry.Input.Text));
			QTimer::singleShot(0, Window.get(), [this, turns] {
				if (!Closing && Window != nullptr && Window->agent_tab()->turn_count() > turns) {
					Window->agent_tab()->reveal_turn(Window->agent_tab()->turn_count() - 1U);
					observe_ask_queue(false);
				}
			});
			return refused.isEmpty();
		}
		// [[BPR-2179]]. The copy on the menu it was queued from: placement
		// decides what it asks about.
		const auto set   = core::placed_prompt_macros(core::prompt_set_for_repository(PromptMacroSet, prompt_repository()));
		auto       macro = std::ranges::find_if(set, [&aEntry](const core::prompt_macro& aMacro) {
			return aMacro.Id == aEntry.Input.QuickAsk && aMacro.Surface && core::is_menu(*aMacro.Surface) && core::surface_scope(*aMacro.Surface)
				&& core::stable_name(*core::surface_scope(*aMacro.Surface)) == aEntry.Input.Scope;
		});
		if (macro == set.end()) {
			macro = std::ranges::find(set, aEntry.Input.QuickAsk, &core::prompt_macro::Id);
		}
		if (macro == set.end()) {
			return false;
		}
		const auto turns  = tab->turn_count();
		const auto id     = QString::fromStdString(macro->Id);
		QueueTurnSeen     = false;
		QueueRetireChecks = 0U;
		switch (macro->Action) {
		case core::prompt_action::what_changed:
			dismiss_asked_conversation();
			ask_what_changed();
			break;
		case core::prompt_action::explain_file: ask_explain_file(aEntry.Input.File); break;
		case core::prompt_action::was_addressed:
			if (aEntry.Input.Thread.find('\n') != std::string::npos || aEntry.Input.Scope != "thread") {
				auto threads = std::vector<std::string>{ };
				for (auto rest = std::string_view{ aEntry.Input.Thread }; !rest.empty();) {
					const auto end = rest.find('\n');
					threads.emplace_back(rest.substr(0U, end));
					rest = end == std::string_view::npos ? std::string_view{ } : rest.substr(end + 1U);
				}
				ask_were_addressed(threads);
			} else {
				ask_was_addressed(aEntry.Input.Thread);
			}
			break;
		case core::prompt_action::none:
			if (macro->Body.Subject == core::prompt_subject::conversation) {
				ask_agent_about_thread(aEntry.Input.Thread);
			} else {
				if (macro->Body.Subject == core::prompt_subject::open_file && !aEntry.Input.File.empty()) {
					static_cast<void>(Window->open_file_in_review(QString::fromStdString(aEntry.Input.File)));
				}
				dismiss_asked_conversation();
			}
			tab->send_prompt(id);
			break;
		}
		Window->open_agent_question();
		QTimer::singleShot(0, Window.get(), [this, turns] {
			if (Closing || Window == nullptr) {
				return;
			}
			auto* const shown = Window->agent_tab();
			if (shown->turn_count() > turns) {
				shown->reveal_turn(shown->turn_count() - 1U);
				observe_ask_queue(false);
			}
		});
		// A turn may start on the next pass (Explain this file reads its
		// material first): the press went out unless the tab refused it.
		return tab->send_refusal_text().isEmpty();
	}

	/// RFC-BPR-0128 ([[BPR-2165]]). A Was this addressed? answer's claim
	/// becomes its thread's chip, with the lines the question read.
	void github_application::implementation::record_thread_claim(const std::string& aThread, const core::addressed_claim aClaim, const std::size_t aTurn)
	{
		if (Closing || Window == nullptr || Window->conversation_panel() == nullptr) {
			return;
		}
		auto claim = qt::thread_claim{ .Claim = aClaim, .Turn = aTurn };
		if (const auto found = ReturningReview.Threads.find(aThread); found != ReturningReview.Threads.end()) {
			claim.Then = found->second.Then;
			claim.Now  = found->second.Now;
		}
		ThreadClaims[aThread] = std::move(claim);
		Window->conversation_panel()->cards()->set_thread_claims(ThreadClaims);
	}

	/// Custom question…: the Agent tab on the narrowest subject the menu
	/// names, with nothing sent.
	void github_application::implementation::ask_custom_question(const qt::ask_menu_request& aRequest)
	{
		if (Window == nullptr) {
			return;
		}
		if (aRequest.Anchor != core::quick_ask_anchor::file && !aRequest.Thread.isEmpty()) {
			ask_agent_about_thread(aRequest.Thread.toStdString());
			return;
		}
		dismiss_asked_conversation();
		Window->open_agent_question();
	}

	/// RFC-BPR-0125 milestone 3 ([[BPR-2146]]). The profile's suggested
	/// prompts reach the chips, the tour chats, the Ask menus and Settings.
	void github_application::implementation::present_prompt_macros(const application::prompt_macro_projection& aProjection)
	{
		PromptMacroSet = aProjection.Set;
		if (Window != nullptr) {
			Window->present_prompt_macros(aProjection, prompt_repository());
			auto repositories = QStringList{ };
			if (const auto open = prompt_repository(); !open.empty()) {
				repositories << QString::fromStdString(open);
			}
			Window->prompts()->set_repositories(repositories);
		}
	}

	auto github_application::implementation::prompt_repository() const -> std::string
	{
		return AgentReview ? AgentReview->Locator.Owner + "/" + AgentReview->Locator.Name : std::string{ };
	}

	void github_application::implementation::refresh_prompt_repository()
	{
		if (PromptMacros) {
			present_prompt_macros(PromptMacros->projection());
		}
	}
#endif
} // namespace bpr::app
