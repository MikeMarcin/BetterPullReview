#include "github_application/window_composition.hpp"

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/note_placement.hpp"
#include "bpr/core/private_thread_lineage.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/event_loop_executor.hpp"

#include <QAction>
#include <QDesktopServices>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTabBar>
#include <QTextStream>

#include <algorithm>
#include <limits>
#include <variant>

#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/adapters/mcp/mcp_context_helper.hpp"
	#include "bpr/application/review_brief.hpp"
	#include "bpr/core/agent_entry.hpp"
#endif
#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/workbench_preferences_host.hpp"
#endif

namespace bpr::app {
	auto github_application::implementation::current_agent_draft_observation() const
		-> std::optional<application::agent_draft_observation>
	{
		if (!ReviewDraft.has_value() || ReviewDraftRevision == 0U) {
			return std::nullopt;
		}
		return application::agent_draft_observation{
			.Choice = {
				.Id = ReviewDraft->Id, .Revision = ReviewDraftRevision },
			.Draft = *ReviewDraft,
		};
	}

	/// \brief The name a file note shows: its file's current or previous path,
	/// else the path kept when it was written ([[BPR-1210]]), else an honest
	/// "earlier update" for a note written before paths were kept.
	auto github_application::implementation::private_note_file_label(const application::local_note_record& aNote) -> std::string
	{
		const auto& aFile = aNote.Attachment->File;
		if (!NotePaths && WindowRead->accepted_snapshot()) {
			NotePaths.emplace();
			for (const auto& file : WindowRead->accepted_snapshot()->Files) {
				NotePaths->try_emplace(core::file_identity_of(file.Path), file.Path);
				if (file.PreviousPath) {
					NotePaths->try_emplace(core::file_identity_of(*file.PreviousPath), *file.PreviousPath);
				}
			}
		}
		if (NotePaths) {
			if (const auto found = NotePaths->find(aFile); found != NotePaths->end()) {
				return found->second;
			}
		}
		if (aNote.FilePath) {
			return *aNote.FilePath;
		}
		return "File comparison from an earlier update";
	}

	/// \brief Copies one selected private item through the existing Comment target.
	/// \brief Copies one private item's text into Comment's draft, then
	/// shows that draft.
	///
	/// \details `aRefused` is called, at most once, when the copy is not
	/// made. [[BPR-1246]]: with private review the draft is accepted on the
	/// private review's answer, after this returns, so the result is read
	/// there. Reading it on return refused every promotion the reviewer
	/// then saw arrive in the draft.
	void github_application::implementation::promote_private_item(std::string aText, std::function<void()> aRefused, std::function<void()> aDone)
	{
		if (Closing || Window == nullptr || !ReviewDraft || !SharedDrafts
			|| !Window->comment_action()->isEnabled() || Window->review_editor()->body_editor()->isReadOnly()) {
			aRefused();
			return;
		}
		const auto anchor = ReviewDraft->Anchor;
		const auto scope  = DraftScope;
		// The same public gesture prepares the exact selected-thread reply,
		// or opens the current-line draft. It cannot publish.
		Window->comment_action()->trigger();
		if (!ReviewDraft || DraftScope != scope || ReviewDraft->Anchor != anchor
			|| !Window->comment_action()->isEnabled() || !Window->review_editor()->publish_action()->isEnabled()) {
			aRefused();
			return;
		}
		const auto command = core::promote_private_text(aText, core::revisioned_review_draft{ *ReviewDraft, ReviewDraftRevision, DraftScope });
		if (!command) {
			aRefused();
			return;
		}
		dispatch_review_draft(*command, [this, anchor, scope, id = command->Command.Id, text = std::move(aText), refused = std::move(aRefused), done = std::move(aDone)](const bool aAccepted) {
			if (Closing || Window == nullptr) {
				return;
			}
			// [[BPR-1862]]. A line's first draft is a placeholder until its
			// first save gives it the store's identity: the saved draft is
			// this one under its new identity, not another.
			const auto placeholder = id.value() > core::maximum_private_identity;
			if (!aAccepted || !ReviewDraft || DraftScope != scope || ReviewDraft->Anchor != anchor
				|| (ReviewDraft->Id != id && !placeholder) || ReviewDraft->Body != text
				|| Window->review_editor()->rejected_candidate()) {
				refused();
				return;
			}
			// [[BPR-1753]]. A finding card stays where it is and
			// says Drafted; every other promotion opens the draft.
			if (done) {
				done();
				return;
			}
			Window->tray_tabs()->setCurrentIndex(0);
			Window->open_review_composition(true);
		});
	}

	/// \brief [[BPR-1592]]. Draft comment on a note at its line: the line is
	/// chosen, and once its draft is the one selected the note's words
	/// become that draft, exactly as Comment's own target would. Nothing
	/// is published.
	void github_application::implementation::promote_line_note(const application::local_note_record& aNote, std::function<void()> aRefused)
	{
		const auto* const surface = Window != nullptr ? Window->comparison_surface() : nullptr;
		const auto* const shown   = surface != nullptr && surface->current_result() != nullptr ? &surface->current_result()->Alignment : nullptr;
		const auto        place   = core::place_note(application::note_anchor_of(aNote), shown);
		if (place.Kind != core::note_placement_kind::placed || !place.At) {
			aRefused();
			return;
		}
		const auto at = *place.At;
		if (ReviewDraft && ReviewDraft->Anchor.Comparison == at.Comparison && ReviewDraft->Anchor.Side == at.Side && ReviewDraft->Anchor.Line == at.Line) {
			promote_private_item(aNote.Note.Text, std::move(aRefused));
			return;
		}
		PendingNotePromotion = pending_note_promotion{ .Text = aNote.Note.Text, .At = at, .Refused = std::move(aRefused) };
		static_cast<void>(Window->comparison_surface()->select_conversation_line(at.Side, at.Line));
	}

	/// \brief [[BPR-1592]]. A note's promotion waiting for its line's draft.
	void github_application::implementation::resume_note_promotion()
	{
		if (!PendingNotePromotion || !ReviewDraft) {
			return;
		}
		const auto& at = PendingNotePromotion->At;
		if (ReviewDraft->Anchor.Comparison != at.Comparison || ReviewDraft->Anchor.Side != at.Side || ReviewDraft->Anchor.Line != at.Line) {
			return;
		}
		auto pending = std::move(*PendingNotePromotion);
		PendingNotePromotion.reset();
		promote_private_item(std::move(pending.Text), std::move(pending.Refused), std::move(pending.Done));
	}

	void github_application::implementation::dispatch_private_note(const qt::private_note_intent& aIntent)
	{
		if (Window == nullptr) {
			return;
		}
		auto* panel = Window->private_notes();
		if (Closing || !ReviewLedgerHost || !ReviewLedgerAvailable || aIntent.Scope != PrivateNoteScope
			|| PrivateNoteScope == 0U || PrivateNoteScope == std::numeric_limits<std::uint64_t>::max()) {
			panel->refuse(QStringLiteral("This note belongs to an unavailable review. Your text is kept."));
			return;
		}
		if (aIntent.Action == qt::private_note_action::discard) {
			ReviewLedgerHost->dismiss_rejected_note();
			refresh_agent_private_context_choices();
			return;
		}
		const auto found = std::ranges::find(AcceptedPrivateNotes, application::local_note_id{ aIntent.Id }, &application::local_note_record::Id);
		if (aIntent.Id != 0U && (found == AcceptedPrivateNotes.end() || found->Revision.value() != aIntent.Revision)) {
			panel->refuse(QStringLiteral("This note changed in another window. Your text is kept; discard changes to reload it."));
			return;
		}
		if (aIntent.Action == qt::private_note_action::ask_agent) {
			if (found != AcceptedPrivateNotes.end()) {
				ask_agent_about_note(*found);
			}
			return;
		}
		if (aIntent.Action == qt::private_note_action::promote) {
			const auto refuse = [guarded = QPointer{ panel }] {
				if (guarded != nullptr) {
					guarded->refuse(QStringLiteral("Select Comment's current-head target and resolve any draft conflict before promoting."));
				}
			};
			if (found == AcceptedPrivateNotes.end()) {
				refuse();
			} else if (found->Attachment && found->Attachment->Line) {
				promote_line_note(*found, refuse);
			} else {
				promote_private_item(found->Note.Text, refuse);
			}
			return;
		}
		const auto refuse = [panel](const application::local_note_failure& aFailure) {
			panel->refuse(QStringLiteral("The note change was refused (%1). Your text is kept. Discard changes to reload the accepted note.")
							  .arg(QString::fromUtf8(application::name(aFailure.Reason).data(), static_cast<qsizetype>(application::name(aFailure.Reason).size()))));
		};
		if (aIntent.Action == qt::private_note_action::erase) {
			if (found == AcceptedPrivateNotes.end()) {
				return;
			}
			// [[BPR-1211]]. Deleted off the decision lane; acknowledged once durable.
			ReviewLedgerHost->erase(found->Id, application::local_note_revision{ aIntent.Revision },
									ledger_answer([this, refuse](const std::expected<void, application::local_note_failure>& aErased) {
										if (!aErased) {
											refuse(aErased.error());
											return;
										}
										ReviewLedgerHost->dismiss_rejected_note();
										Window->private_notes()->acknowledge(0U);
										refresh_agent_private_context_choices();
									}));
			return;
		}
		auto note     = application::local_note_record{ };
		auto revision = std::optional<application::local_note_revision>{ };
		if (found != AcceptedPrivateNotes.end()) {
			note     = *found;
			revision = application::local_note_revision{ aIntent.Revision };
		} else {
			auto next = std::uint64_t{ 1U };
			for (const auto& accepted : AcceptedPrivateNotes) {
				if (accepted.Id.value() == std::numeric_limits<std::uint64_t>::max()) {
					panel->refuse(QStringLiteral("The note identity space is exhausted. Your text is kept."));
					return;
				}
				next = std::max(next, accepted.Id.value() + 1U);
			}
			note.Id = application::local_note_id{ next };
			if (aIntent.Attachment) {
				auto offered = application::local_note_attachment{ aIntent.Attachment->File, aIntent.Attachment->Comparison };
				if (current_note_attachment() != std::optional{ offered }) {
					panel->refuse(QStringLiteral("The selected file changed. Your text is kept; return to that comparison before saving."));
					return;
				}
				// [[BPR-1591]]. A note on a line keeps exactly the line the
				// shown comparison has there, and its blob.
				if (aIntent.Attachment->Line) {
					const auto* const surface = Window->comparison_surface();
					const auto        shown   = surface != nullptr && surface->current_result() != nullptr && surface->current_result()->Alignment.Digest == offered.Comparison
						? core::note_line_anchor_at(surface->current_result()->Alignment, aIntent.Attachment->Line->Side, aIntent.Attachment->Line->Line)
						: std::nullopt;
					if (shown != aIntent.Attachment->Line) {
						panel->refuse(QStringLiteral("That line is not in the shown comparison now. Your text is kept; return to it before saving."));
						return;
					}
					offered.Line = shown;
				}
				note.Attachment = offered;
				// [[BPR-1210]]. Kept with the note, so it can still name the
				// file after a later update drops it.
				if (const auto& path = SelectedDiffPublication->Request.File.PresentationPath;
					!path.empty() && path.size() <= application::maximum_note_path_bytes) {
					note.FilePath = path;
				}
			}
		}
		note.Note.Text = aIntent.Text;
		// [[BPR-1211]]. Written off the decision lane; acknowledged once durable.
		ReviewLedgerHost->persist(note, revision,
								  ledger_answer([this, refuse](const std::expected<application::local_note_record, application::local_note_failure>& aStored) {
									  if (!aStored) {
										  refuse(aStored.error());
										  return;
									  }
									  ReviewLedgerHost->dismiss_rejected_note();
									  Window->private_notes()->acknowledge(aStored->Id.value());
									  refresh_agent_private_context_choices();
								  }));
	}

	void github_application::implementation::refresh_agent_private_context_choices()
	{
		if (Window == nullptr || Window->agent_panel() == nullptr) {
			return;
		}
		auto notes      = std::vector<application::local_note_id>{ };
		auto projection = qt::private_notes_projection{ .Scope = PrivateNoteScope };
		if (const auto attachment = current_note_attachment()) {
			projection.CurrentFile = qt::private_note_target{ attachment->File, attachment->Comparison };
			// This path is already owned by the selected comparison.
			projection.CurrentFileLabel = SelectedDiffPublication->Request.File.PresentationPath;
		}
		if (ReviewLedgerHost != nullptr) {
			if (auto observed = ReviewLedgerHost->private_observation(); observed.has_value()) {
				AcceptedPrivateNotes = std::move(observed->Notes);
				projection.Available = ReviewLedgerAvailable && PrivateNoteScope != std::numeric_limits<std::uint64_t>::max();
				notes.reserve(AcceptedPrivateNotes.size());
				for (const auto& note : AcceptedPrivateNotes) {
					notes.push_back(note.Id);
					projection.Notes.push_back({ .Id = note.Id.value(), .Revision = note.Revision.value(), .Attachment = note.Attachment ? std::optional{ qt::private_note_target{ note.Attachment->File, note.Attachment->Comparison, note.Attachment->Line } } : std::nullopt, .Label = note.Attachment ? private_note_file_label(note) : "Pull request", .Text = note.Note.Text });
				}
				std::ranges::sort(projection.Notes, std::greater{ }, &qt::private_note_item::Id);
			}
		}
		Window->private_notes()->present(std::move(projection));
		auto draft = current_agent_draft_observation();
		if (draft.has_value()
			&& draft->Draft.State != core::review_draft_state::drafting) {
			draft.reset();
		}
		Window->agent_panel()->offer_private_context(
			notes,
			draft.has_value()
				? std::optional{ draft->Choice }
				: std::nullopt);
	}
#if defined(BPR_AGENT_INTEGRATIONS)
	/// \brief What the reviewer is pointing at, in RFC-BPR-0004 identities.
	///
	/// \details Read from the alignment on screen, and only when the
	/// selection and the published comparison are the same one. **There is
	/// no coordinate here and none reachable from here**: a file identity
	/// and a semantic selection are the whole of it.
	auto github_application::implementation::current_agent_chat_selection() const
		-> std::optional<core::agent_selection>
	{
		if (Window == nullptr || !SelectedDiffPublication.has_value()) {
			return std::nullopt;
		}
		const auto&       selected  = *SelectedDiffPublication;
		const auto* const workbench = Window->comparison_surface();
		if (
			workbench == nullptr || !workbench->selection().has_value()
			|| workbench->current_result() == nullptr
			|| workbench->current_result()->Alignment.Key != selected.Key
			|| workbench->selection()->Comparison
				!= workbench->current_result()->Alignment.Digest) {
			return std::nullopt;
		}
		return core::agent_selection{
			.File      = selected.Request.File.File,
			.Selection = *workbench->selection(),
		};
	}

	void github_application::implementation::clear_side_chat()
	{
		if (ChatHost != nullptr) {
			ChatHost->clear();
		}
	}

	void github_application::implementation::refresh_agent_authority()
	{
		observe_private_review();
		if (AgentHost == nullptr || EpochHost == nullptr || Window == nullptr) {
			return;
		}
		const auto& state = EpochHost->state();
		if (!state.Authority.has_value() || !state.AcceptedRevision.has_value()
			|| !state.AcceptedComparison.has_value() || !AgentReview.has_value()) {
			AgentHost->clear();
			if (TourSession) {
				TourSession->clear();
			}
			clear_side_chat();
			Window->clear_dif_context();
			return;
		}
		const auto& authority  = *state.Authority;
		const auto& revision   = *state.AcceptedRevision;
		const auto& comparison = *state.AcceptedComparison;
		if (authority.Host != AgentReview->Host
			|| authority.Repository != AgentReview->Repository
			|| authority.PullRequest != AgentReview->PullRequest) {
			AgentHost->clear();
			if (TourSession) {
				TourSession->clear();
			}
			clear_side_chat();
			Window->clear_dif_context();
			return;
		}
		const auto observedBinding = core::agent_revision_binding{
			.Host        = authority.Host,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.BaseOid     = revision.Base,
			.HeadOid     = revision.Head,
			.LeftEpoch   = comparison.Left.Identity.Epoch,
			.RightEpoch  = comparison.Right.Identity.Epoch,
		};
		Window->observe_dif_context(
			state.ApplicationSession, authority, observedBinding);
		AgentHost->observe(
			state.ApplicationSession, authority, observedBinding);
		if (TourSession) {
			TourSession->observe(state.ApplicationSession, authority, observedBinding);
		}
		if (AgentSetup) {
			// [[BPR-1749]]. Setup's rows carry this review's host
			// policy, the same the agent host decides by.
			AgentSetup->observe_policy(core::policy_for_host(AgentPolicies, authority.Host));
		}
		refresh_agent_private_context_choices();
		if (ChatHost != nullptr) {
			// Recorded, never retargeted. The chat host presents itself
			// through its own projection, so a head that moved makes the
			// session stale on screen instead of quietly re-pointing it.
			ChatHost->observe_selection(current_agent_chat_selection());
			ChatHost->observe(observedBinding);
		} else {
			present_side_chat(observedBinding);
		}
		observe_conversation_agent();
		Window->agent_panel()->disclose_scope(
			QStringLiteral(
				"The selected agent receives the exact accepted PR/diff identities, "
				"durable defects, current semantic selection, and only private notes or "
				"the current draft checked for this invocation. Native conversations retain their exact-head workspace "
				"and earlier agent context outside the Git cache. Removing context from a later turn does not erase "
				"what the agent already received.%1")
				.arg(ExistingCheckout.has_value()
						 ? QStringLiteral(
							   " Choose the working-copy option only for this invocation; its state is disclosed as unknown.")
						 : QString{ }));
	}

	void github_application::implementation::dispatch_agent_intent(qt::agent_intent aIntent)
	{
		if (Closing || AgentHost == nullptr || EpochHost == nullptr
			|| Window == nullptr) {
			return;
		}
		if (std::holds_alternative<qt::cancel_agent_intent>(aIntent)) {
			AgentHost->cancel();
			return;
		}
		const auto start = std::get<qt::start_agent_intent>(aIntent);
		// A start gesture is not a chat turn, so it asks nothing: the
		// supplement's message stays empty on this path.
		static_cast<void>(dispatch_agent_invocation(
			start.Profile,
			start.IncludedNotes,
			start.IncludedDraft,
			start.ExistingCheckoutChosen,
			std::string_view{ },
			std::nullopt,
			core::agent_entry_point::ask_about_selection,
			std::nullopt));
	}

	/// \brief The one launch path, and there is deliberately no second.
	///
	/// \details Both the launch panel's start gesture and the side chat's
	/// send arrive here, so the agent host's own `dispatch` has exactly one
	/// call site in this file. A chat that had grown its own launch would have its own
	/// workspace, its own frozen snapshot, and its own answer about the
	/// same revision; `application_agent_chat_composition_contract` reads
	/// this file and fails if a second call site appears.
	///
	/// `aChatMessage` is what the reviewer typed for this turn, and is
	/// empty for every gesture that is not a chat send. It is an argument
	/// rather than something read from the chat host here, so the one place
	/// a question enters an invocation is a value passed at a call site a
	/// reader can see.
	///
	/// `aTurnSession` is the session a chat turn is sent under, and is
	/// absent for the launch panel's start gesture. When that session is
	/// bound to a revision the review has moved away from — which is what
	/// RFC-BPR-0012's `continue_against_old_revision` leaves behind — the
	/// revision *it* names is the one everything below composes from.
	///
	/// `aEntryPoint` is the reviewer gesture this turn is for, and
	/// `aWorkshopOperation` is which of the six comment-workshop
	/// operations was pressed. Both are arguments for the same reason
	/// `aChatMessage` is: the one place a gesture's intent enters an
	/// invocation is a value passed at a call site a reader can see.
	/// `build_agent_context_supplement` refuses an operation the entry
	/// point does not admit, so a workshop operation cannot ride along on
	/// a start gesture or a chat send.
	///
	/// \returns Whether exactly one invocation was dispatched. A refusal is
	/// already named on the launch panel before this returns `false`.
	auto github_application::implementation::dispatch_agent_invocation(
		const core::agent_profile_id                          aProfile,
		const std::vector<application::local_note_id>&        aIncludedNotes,
		const std::optional<application::agent_draft_choice>& aIncludedDraft,
		const bool                                            aExistingCheckoutChosen,
		const std::string_view                                aChatMessage,
		const std::optional<core::agent_session>&             aTurnSession,
		const core::agent_entry_point                         aEntryPoint,
		const std::optional<core::workshop_operation>& aWorkshopOperation, bool aWritingOnly, const tour_launch& aTour)
		-> bool
	{
		if (Closing || AgentHost == nullptr || EpochHost == nullptr
			|| Window == nullptr) {
			return false;
		}
		if (!aWritingOnly && !aExistingCheckoutChosen && AgentConversation && !AgentConversation->projection().Loaded) {
			Window->agent_panel()->refuse("Saved conversation is still loading. No question was sent.");
			return false;
		}
		const auto& state = EpochHost->state();
		if (!state.Authority.has_value() || !state.AcceptedRevision.has_value()
			|| !state.AcceptedComparison.has_value() || !AgentReview.has_value()
			|| ReviewLedgerHost == nullptr) {
			Window->agent_panel()->refuse("no exact accepted review context");
			return false;
		}
		const auto& authority          = *state.Authority;
		const auto& revision           = *state.AcceptedRevision;
		const auto& comparison         = *state.AcceptedComparison;
		auto        privateObservation = application::private_ledger_observation{ };
		if (ReviewLedgerHost != nullptr) {
			auto observed = ReviewLedgerHost->private_observation();
			if (!observed.has_value()) {
				Window->agent_panel()->refuse("private review context is unavailable");
				return false;
			}
			privateObservation = std::move(*observed);
		}
		const auto binding = core::agent_revision_binding{
			.Host        = authority.Host,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.BaseOid     = revision.Base,
			.HeadOid     = revision.Head,
			.LeftEpoch   = comparison.Left.Identity.Epoch,
			.RightEpoch  = comparison.Right.Identity.Epoch,
		};
		// RFC-BPR-0012's `continue_against_old_revision`, composed and not
		// only kept. A chat turn whose session is bound to a revision the
		// epoch host has moved past is a question about *that* revision,
		// so the revision it names — not the one under review — is what
		// this invocation is built from. The next line is the whole of it;
		// without it the turn would be framed from the current head while
		// still naming the older session, and `agent_invocation_host`
		// refuses that disagreement as
		// `agent_continuation_binding_mismatch`.
		const auto continuing
			= aTurnSession.has_value() && aTurnSession->Binding != binding;
		auto turnBinding = binding;
		if (continuing) {
			turnBinding = aTurnSession->Binding;
		}
		// [[BPR-1935]]. A read-only tour run's prompt is composed, not typed,
		// and travels in the tour prompt's own carrier and bound.
		const auto tourPrompt  = aWritingOnly && aTour.ReadOnly;
		auto       observation = application::agent_context_observation{
			.ApplicationSession = state.ApplicationSession,
			.Binding            = turnBinding,
			.PullRequestUrl     = "https://" + AgentReview->Hostname + "/"
				+ AgentReview->Locator.Owner + "/" + AgentReview->Locator.Name
				+ "/pull/" + std::to_string(AgentReview->Number.value()),
			.Private           = std::move(privateObservation),
			.IncludedNotes     = aIncludedNotes,
			.IncludedDraft     = aIncludedDraft,
			.ChatMessage       = tourPrompt ? std::string{ } : std::string{ aChatMessage },
			.EntryPoint        = aEntryPoint,
			.WorkshopOperation = aWorkshopOperation,
		};
		if (tourPrompt) {
			observation.TourPrompt = std::string{ aChatMessage };
		}
		// [[BPR-1916]]. The skills an Agent tab question names ride with
		// it, inline. A tour writing run's come from its hook (see
		// skill_run_for), not from its prompt's template text.
		if (!aWritingOnly && !aChatMessage.empty()) {
			const auto texts  = std::array{ std::string_view{ aChatMessage } };
			auto       skills = prepare_skill_run(SkillLibrary, texts);
			if (!skills.Inline.empty() && observation.ChatMessage.size() + skills.Directive.size() <= core::maximum_chat_message_bytes) {
				observation.Skills = std::move(skills.Inline);
				observation.ChatMessage += skills.Directive;
			}
		}
		// [[BPR-1091]]. A comment-workshop press, and nothing else, sends
		// the profile's review-feedback guidance, read now from the
		// preferences the profile has published, and only while its label
		// helper is on. With the helper off the press is the request it
		// was before the hook existed. The guidance is text for the
		// agent's prose; the launch it rides on is the one every press
		// already makes.
		if (aEntryPoint == core::agent_entry_point::workshop_comment && aWorkshopOperation.has_value() && Profile && Profile->WorkbenchPreferences) {
			if (const auto& preferences = Profile->WorkbenchPreferences->accepted(); preferences) {
				observation.FeedbackGuidance = application::workshop_feedback_supplement(preferences->WritingHooks, preferences->Behaviour.LabelHelper);
			}
		}
		// [[BPR-1773]]. A question about a conversation carries it,
		// read now; one that is no longer in this pull request asks
		// nothing.
		if (aEntryPoint == core::agent_entry_point::ask_about_conversation) {
			observation.Conversation = read_asked_conversation();
	#if defined(BPR_BUILD_TESTING)
			LastAgentConversation = observation.Conversation;
	#endif
			if (!observation.Conversation) {
				dismiss_asked_conversation();
				Window->agent_panel()->refuse("the conversation asked about is no longer in this pull request");
				return false;
			}
		}
		observation.CurrentDraft = current_agent_draft_observation();
		// [[BPR-1752]], RFC-BPR-0092. A review request carries the
		// brief: what the reviewer can read on GitHub about this pull
		// request, bounded, and the review skill's text (Q9 A). A comment
		// workshop press and a writing request are not review requests.
		if (!aWritingOnly && aEntryPoint != core::agent_entry_point::workshop_comment && WindowRead->accepted_snapshot()) {
			auto        brief              = application::read_review_brief(*WindowRead->accepted_snapshot(), ReviewSkill ? ReviewSkill->brief_skill() : std::nullopt);
			const auto* entry              = AgentSetup ? core::entry_of_profile(AgentSetup->projection().Preferences.Agents, aProfile) : nullptr;
			const auto  nativeConversation = !aExistingCheckoutChosen && entry && !entry->Identity.empty() && entry->Protocol != core::agent_protocol::external_terminal;
			// [[BPR-1753]], owner decision (b). The conversation so
			// far about this pull request, so a follow-up continues it.
			if (AgentConversation && !nativeConversation) {
				const auto earlier = AgentConversation->projection().Turns;
				for (const auto& turn : earlier) {
					brief.Conversation.push_back({ .Asked = turn.Asked, .Answered = turn.Answered });
				}
			}
			observation.Brief = core::bound_review_brief(std::move(brief));
		}
		// Which file, and where in it. The two are told apart because
		// only one of them names a comparison. The file is a path and a
		// review receipt, which belong to the file rather than to any pair
		// of commits, so a continuation may say them: the turn is answered
		// against the frozen snapshot of its own revision, and a finding
		// that names no file cannot be anchored to one. BPR-0532.
		//
		// The semantic selection below is read off the alignment the
		// reviewer is looking at, and its segments, groups, and comparison
		// digest belong to the revision under review. Under a continuation
		// that is a different revision than the one being asked about, so
		// none of it is sent: carrying a newer comparison's identities
		// beneath an older binding is the silent re-anchoring BPR-0521
		// refused, and this card does not un-refuse it. What a
		// continuation carries instead is the position the reviewer is
		// pointing at — a side and a line and nothing identified — held
		// here and said with the old revision's own identities once the
		// re-alignment below has produced them.
		auto pointedAt = std::optional<continuation_anchor>{ };
		if (SelectedDiffPublication.has_value()) {
			const auto& selected = *SelectedDiffPublication;
			const auto  lock     = std::scoped_lock{ ReviewLedgerStateMutex };
			if (ReviewLedgerReceipt.has_value()
				&& ReviewLedgerReceipt->File == receipt_file(selected)) {
				observation.Files.push_back(application::agent_file_context_input{
					.Path               = selected.Request.File.PresentationPath,
					.Receipt            = *ReviewLedgerReceipt,
					.ChangedSinceReview = observation.Private.LastReviewedHead.has_value()
						&& *observation.Private.LastReviewedHead != revision.Head,
				});
			}
			const auto* const workbench = Window->comparison_surface();
			const auto        pointing  = workbench->selection().has_value()
				&& workbench->current_result() != nullptr
				&& workbench->current_result()->Alignment.Key == selected.Key
				&& workbench->selection()->Comparison
					== workbench->current_result()->Alignment.Digest;
			if (pointing && !continuing) {
				observation.Selection = core::supplement_semantic_context{
					.File       = selected.Request.File.File,
					.Selection  = *workbench->selection(),
					.Confidence = core::supplement_mapping_confidence::exact,
					.Segments   = workbench->current_result()->Alignment.Segments,
					.Groups     = workbench->current_result()->Alignment.Groups,
				};
			} else if (pointing) {
				pointedAt = continuation_anchor{
					.File = selected.Request.File.File,
					.Side = workbench->selection()->PreferredLine.Side,
					.Line = workbench->selection()->PreferredLine.Line,
				};
			}
		}
		// A continuation asks for the old revision's alignment before it
		// asks the agent anything, and asks for it off this gesture:
		// recomputing an alignment queues behind whatever epoch work the
		// review is already doing, and a gesture that waited for it would
		// freeze the window for seconds. The reviewer's send returns here;
		// the dispatch follows the answer.
		if (continuing) {
			return begin_continuation_realignment(
				aProfile,
				authority,
				state.ApplicationSession,
				std::move(observation),
				aExistingCheckoutChosen,
				*aTurnSession,
				pointedAt);
		}
		if (aWritingOnly) {
			observation.Files.clear();
			observation.Selection.reset();
			observation.CurrentDraft.reset();
			observation.Private.Notes.clear();
			observation.Private.Defects.clear();
		}
		return launch_agent_invocation(
			aProfile,
			authority,
			state.ApplicationSession,
			std::move(observation),
			aExistingCheckoutChosen,
			std::nullopt,
			aWritingOnly ? std::vector<core::agent_finding_snapshot_file>{ } : frozen_snapshot_files(), aWritingOnly, aTour);
	}

	/// \brief Freezes one turn's identities, pins them, and dispatches it.
	///
	/// \details The tail every gesture reaches, whether it got here
	/// straight from the composition above or from a re-alignment that
	/// answered later. The agent host's own dispatch is called here and
	/// nowhere else, which is the property
	/// `application_agent_chat_composition_contract` reads this file for
	/// by counting that call site — so this sentence deliberately does not
	/// spell the token it counts.
	///
	/// The frozen snapshot is stated once and used twice: the host
	/// validates every returned finding against it, and the MCP helper is
	/// pinned to the same values on disk. Two answers about one revision
	/// cannot disagree if they are one value. An empty list is a complete
	/// answer rather than a missing one — every finding then arrives
	/// unanchored — and it is what a continuation whose re-alignment was
	/// refused is dispatched with.
	auto github_application::implementation::launch_agent_invocation(
		const core::agent_profile_id                   aProfile,
		const core::pull_request_authority&            aAuthority,
		const core::application_session_id             aApplicationSession,
		application::agent_context_observation         aObservation,
		const bool                                     aExistingCheckoutChosen,
		const std::optional<core::agent_session>&      aContinuedSession,
		std::vector<core::agent_finding_snapshot_file> aFrozen, bool aWritingOnly, const tour_launch& aTour) -> bool
	{
		if (Closing || AgentHost == nullptr) {
			return false;
		}
		if (McpHelper != nullptr && !aWritingOnly) {
			const auto encoded = adapters::mcp::encode_revision_binding(
				aObservation.Binding);
			if (freeze_mcp_snapshot(McpSnapshotRoot, encoded, aFrozen)) {
				++FrozenSnapshotWrites;
				McpHelper->pin(McpSnapshotRoot, encoded);
			}
		}
		if (!aWritingOnly && (!aObservation.IncludedNotes.empty() || aObservation.IncludedDraft)) {
			Window->agent_panel()->refuse("Choose immutable revisions or findings in Private history, then Ask agent using selected sources.");
			return false;
		}
		// [[BPR-1940]]. Keep the empty path explicit in optimized builds.
		auto privateRun = std::optional<application::private_run_intent>{ };
		if (!aWritingOnly) {
			privateRun = ExplicitPrivateRun;
		}
		// [[BPR-1375]], owner 2026-10-09. A Claude request names the
		// profile's saved model unless the request already names one.
		auto requested = PrivateRequested;
		if (aProfile == core::claude_stream_json_profile_id && !requested.Model && !ClaudeModel.empty()) {
			requested.Model = ClaudeModel;
		}
		// [[BPR-1753]], owner look v002. The model and effort the tab
		// shows are the ones this request runs with.
		if (AgentSetup) {
			const auto preferences = AgentSetup->projection().Preferences;
			if (aProfile == core::claude_stream_json_profile_id && !requested.Effort && !preferences.ClaudeEffort.empty()) {
				requested.Effort = preferences.ClaudeEffort;
			}
			if ((aProfile == core::codex_app_server_profile_id || aProfile == core::codex_jsonl_profile_id) && (!aWritingOnly || aTour.ReadOnly)) {
				if (!requested.Model && !preferences.CodexModel.empty()) {
					requested.Model = preferences.CodexModel;
				}
				if (!requested.Effort && !preferences.CodexEffort.empty()) {
					requested.Effort = preferences.CodexEffort;
				}
			}
		}
		// [[BPR-1753]], owner amendment 2026-10-23. An added agent runs
		// with its own model and effort unless the request names one.
		auto checks = application::agent_run_checks::strict;
		if (AgentSetup) {
			const auto agents = AgentSetup->projection().Preferences.Agents;
			if (const auto* const entry = core::entry_of_profile(agents, aProfile); entry != nullptr) {
				// [[BPR-1918]]. A custom agent may opt out of strict run checks.
				if (!entry->StrictRunChecks && !entry->Preset) {
					checks = application::agent_run_checks::relaxed;
				}
				if (!requested.Model && !entry->Model.empty()) {
					requested.Model = entry->Model;
				}
				if (!requested.Effort && !entry->Effort.empty()) {
					requested.Effort = entry->Effort;
				}
			}
		}
		if (!aWritingOnly && !privateRun && PrivateReview) {
			privateRun = PrivateReview->default_run(aObservation.Binding, !aObservation.ChatMessage.empty());
		}
		auto invocation = application::agent_invocation_request{
			.ApplicationSession     = aApplicationSession,
			.Authority              = aAuthority,
			.Profile                = aProfile,
			.Context                = std::move(aObservation),
			.ExistingCheckoutChosen = aExistingCheckoutChosen,
			.ExistingCheckout       = ExistingCheckout,
			.FrozenFiles            = std::move(aFrozen),
			.ContinuedSession       = aContinuedSession,
			.WritingOnly            = aWritingOnly,
			.RequestedSettings      = std::move(requested),
			.PrivateRun             = std::move(privateRun),
			.HistoryClone           = linked_history_clone(aWritingOnly),
			.HistoryCloneManaged    = linked_history_clone_managed(aWritingOnly),
			// Owner decision 2026-10-23: a time limit only when the reviewer set one.
			.RunLimitMilliseconds = run_limit_milliseconds(),
		};
		// [[BPR-1900]]. Every tour writing run is the tour session's, never
		// the Agent tab's: a drafting run in the review's workspace with
		// read-only tools, or a small rewrite tool-free.
		if (aWritingOnly) {
			if (TourSession == nullptr) {
				return false;
			}
			invocation.WritingOnly         = !aTour.ReadOnly;
			invocation.ReadOnlyTools       = aTour.ReadOnly;
			invocation.HistoryClone        = aTour.ReadOnly ? linked_history_clone(false) : std::nullopt;
			invocation.HistoryCloneManaged = aTour.ReadOnly && linked_history_clone_managed(false);
			invocation.MeasurePaths        = aTour.Measure;
			invocation.RunChecks           = checks;
			// [[BPR-1916]]. The skills the prompt names ride in the supplement.
			invocation.Context.Skills = aTour.Skills.Inline;
			if (!aTour.NativeScope.empty() && AgentSetup) {
				const auto preferences = AgentSetup->projection().Preferences;
				if (const auto* entry = core::entry_of_profile(preferences.Agents, aProfile); entry) {
					invocation.NativeAgentIdentity = entry->Identity;
					invocation.NativeScope         = aTour.NativeScope;
					invocation.NativeQuestion      = aTour.NativeQuestion;
				}
			}
			TourSession->dispatch(std::move(invocation));
			return true;
		}
		if (AgentSetup && !aExistingCheckoutChosen) {
			const auto preferences = AgentSetup->projection().Preferences;
			if (const auto* const entry = core::entry_of_profile(preferences.Agents, aProfile); entry != nullptr && entry->Protocol != core::agent_protocol::external_terminal) {
				invocation.NativeAgentIdentity          = entry->Identity;
				invocation.NativeScope                  = "agent-tab";
				invocation.NativeStartAtCurrentRevision = NativeFreshAtCurrentRevision && NativeFreshAtCurrentRevision->Profile == aProfile
					&& NativeFreshAtCurrentRevision->Binding == invocation.Context.Binding;
			}
		}
		AgentHost->dispatch(std::move(invocation));
		return true;
	}

	/// \brief Asks for the kept revision's own alignment, then dispatches.
	///
	/// \details BPR-0532. The reviewer kept a session bound to a revision
	/// the review has moved past, so the identities this turn must be
	/// answered against belong to that revision and not to the comparison
	/// on screen. They are recomputed rather than retained: the request
	/// below carries the current selection's authority, options, and file
	/// and nothing else, and `realignment_lookup` retargets it onto the
	/// kept binding's epoch pair.
	///
	/// Two bounds, both stated in the application layer rather than here:
	/// `maximum_agent_realignment_files` and the request's own compute
	/// limits — the limits a fresh alignment has — cap the work, and
	/// `maximum_agent_realignment_wait` caps the wait. Past the wait the
	/// request is stopped and the turn is dispatched with nothing frozen,
	/// which is the same answer any other refusal produces. Nothing here
	/// blocks and nothing here waits twice: the dispatch happens in the
	/// completion below, once.
	auto github_application::implementation::begin_continuation_realignment(
		const core::agent_profile_id              aProfile,
		const core::pull_request_authority&       aAuthority,
		const core::application_session_id        aApplicationSession,
		application::agent_context_observation    aObservation,
		const bool                                aExistingCheckoutChosen,
		const core::agent_session&                aTurnSession,
		const std::optional<continuation_anchor>& aPointedAt) -> bool
	{
		LastContinuationRealignmentRetargeted = false;
		auto realignment
			= std::optional<application::agent_realignment_request>{ };
		if (Realigner != nullptr && SelectedDiffPublication.has_value()) {
			const auto& selected = *SelectedDiffPublication;
			// The warrant, asked of the host that owns the cache the
			// recompute will read. The reviewer's own selected diff is
			// authorized for the comparison on screen and nothing else, so
			// a turn about an older pair is refused as stale unless the
			// host has decided to answer for that pair; asking for the
			// decision is what this is, and a host that declines leaves
			// this turn on the documented empty freeze.
			const auto warrant = EpochHost->realignment_context(
				selected.Request.WorkbenchAuthority, aTurnSession.Binding);
			if (!warrant.has_value()) {
				return refuse_continuation_realignment(
					pending_continuation{
						.Profile                = aProfile,
						.Authority              = aAuthority,
						.ApplicationSession     = aApplicationSession,
						.Observation            = std::move(aObservation),
						.ExistingCheckoutChosen = aExistingCheckoutChosen,
						.Session                = aTurnSession,
						.PointedAt              = aPointedAt,
					},
					application::agent_realignment_refusal::host_failure);
			}
			realignment = application::agent_realignment_request{
				.Binding            = aTurnSession.Binding,
				.Context            = selected.Request.Context,
				.WorkbenchAuthority = selected.Request.WorkbenchAuthority,
				.Diff               = *warrant,
				.Files              = { selected.Request.File },
				.Options            = warrant->Options,
				.Limits             = core::diff_compute_limits{ },
			};
			// Whether the retarget is a retarget at all. When the kept
			// binding's pair is the pair already on screen this asks for
			// what is already there, and a course that proved anything
			// about re-alignment from such a turn would be proving
			// nothing.
			LastContinuationRealignmentRetargeted
				= application::realignment_lookup(
					  *realignment, realignment->Files.front())
				!= selected.Request;
		}
		auto turn = pending_continuation{
			.Profile                = aProfile,
			.Authority              = aAuthority,
			.ApplicationSession     = aApplicationSession,
			.Observation            = std::move(aObservation),
			.ExistingCheckoutChosen = aExistingCheckoutChosen,
			.Session                = aTurnSession,
			.PointedAt              = aPointedAt,
		};
		if (!realignment.has_value() || Window == nullptr) {
			return refuse_continuation_realignment(
				std::move(turn),
				application::agent_realignment_refusal::host_failure);
		}
		auto        stop      = std::make_shared<std::stop_source>();
		const auto  token     = stop->get_token();
		auto* const realigner = Realigner.get();
		auto* const lane      = &DecisionLane;
		ContinuationRealignmentsInFlight.fetch_add(
			1U, std::memory_order_release);
		try {
			AgentEffects.post(
				[this,
				 lane,
				 realigner,
				 token,
				 asked   = *realignment,
				 waiting = std::move(turn)]() mutable {
					auto outcome = realigner->realign(asked, token);
					try {
						lane->post([this,
									produced = std::move(outcome),
									resumed  = std::move(waiting)]() mutable {
							finish_continuation_realignment(
								std::move(resumed), std::move(produced));
						});
					} catch (...) {
						ContinuationRealignmentsInFlight.fetch_sub(
							1U, std::memory_order_release);
					}
				});
		} catch (...) {
			ContinuationRealignmentsInFlight.fetch_sub(
				1U, std::memory_order_release);
			return refuse_continuation_realignment(
				std::move(turn),
				application::agent_realignment_refusal::host_failure);
		}
		LastContinuationRealignment = "pending";
		ContinuationRealignmentStop = stop;
		// The wall-clock bound, armed on the gesture and disarmed by the
		// completion resetting the handle it compares against. A timer
		// that arrives after its own turn finished belongs to no request
		// and stops nothing.
		QTimer::singleShot(
			static_cast<int>(
				application::maximum_agent_realignment_wait.count()),
			Window.get(),
			[this, stop] {
				if (ContinuationRealignmentStop != stop) {
					return;
				}
				ContinuationRealignmentDeadlineFired = true;
				static_cast<void>(stop->request_stop());
			});
		return true;
	}

	/// \brief Dispatches a continued turn whose alignment was refused.
	auto github_application::implementation::refuse_continuation_realignment(
		pending_continuation                         aTurn,
		const application::agent_realignment_refusal aRefusal) -> bool
	{
		LastContinuationRealignment
			= std::string{ application::stable_name(aRefusal) };
		return launch_agent_invocation(
			aTurn.Profile,
			aTurn.Authority,
			aTurn.ApplicationSession,
			std::move(aTurn.Observation),
			aTurn.ExistingCheckoutChosen,
			aTurn.Session,
			{ });
	}

	/// \brief Dispatches the continued turn its alignment answered for.
	void github_application::implementation::finish_continuation_realignment(
		pending_continuation                   aTurn,
		application::agent_realignment_outcome aOutcome)
	{
		ContinuationRealignmentsInFlight.fetch_sub(
			1U, std::memory_order_release);
		const auto expired = ContinuationRealignmentDeadlineFired;
		ContinuationRealignmentStop.reset();
		ContinuationRealignmentDeadlineFired = false;
		// The reason, recorded whether or not anything was frozen. A turn
		// that froze nothing and a turn nobody asked to freeze anything
		// are different events, and only a recorded reason tells them
		// apart afterwards.
		LastContinuationRealignment = expired
			? std::string{ "deadline" }
			: std::string{ application::stable_name(aOutcome.Refusal) };
		if (Closing) {
			return;
		}
		// Where the reviewer is pointing, said in the old revision's own
		// terms. The position crossed over by line and side, which is a
		// positional mapping and not an identity one, so the confidence
		// stated is `approximate` and no segment or group identity is
		// claimed: this composition did not compute one, and inventing an
		// identity for a line is the whole failure mode here. What it does
		// state is the re-aligned pair's own comparison digest, segments,
		// and groups, so every identity the turn carries and every
		// identity it is validated against come from the one alignment the
		// re-alignment produced. Out of the old revision's range the
		// position is simply not said - that revision has no such line,
		// and the turn is still answered against its frozen identities.
		if (!aOutcome.Alignments.empty() && aTurn.PointedAt.has_value()) {
			const auto& realigned = aOutcome.Alignments.front();
			const auto  anchor    = core::source_line_ref{
				.Comparison = realigned.Digest,
				.Side       = aTurn.PointedAt->Side,
				.Line       = aTurn.PointedAt->Line,
			};
			if (core::valid(
					anchor,
					realigned.LeftLineCount,
					realigned.RightLineCount)) {
				aTurn.Observation.Selection
					= core::supplement_semantic_context{
						  .File      = aTurn.PointedAt->File,
						  .Selection = core::semantic_selection{
							  .Comparison    = realigned.Digest,
							  .Kind          = core::semantic_selection_kind::line,
							  .PreferredLine = anchor,
						  },
						  .Confidence = core::supplement_mapping_confidence::approximate,
						  .Segments   = realigned.Segments,
						  .Groups     = realigned.Groups,
					  };
			}
		}
		static_cast<void>(launch_agent_invocation(
			aTurn.Profile,
			aTurn.Authority,
			aTurn.ApplicationSession,
			std::move(aTurn.Observation),
			aTurn.ExistingCheckoutChosen,
			aTurn.Session,
			std::move(aOutcome.Files)));
	}

	/// \brief The frozen alignment facts one invocation is answered against.
	///
	/// \details Read from the alignment the reviewer is actually looking
	/// at, once, and carried as values. An empty list is a complete answer
	/// rather than a missing one: every finding then arrives unanchored,
	/// which is what a run that froze nothing has evidence for.
	auto github_application::implementation::frozen_snapshot_files() const
		-> std::vector<core::agent_finding_snapshot_file>
	{
		auto files = std::vector<core::agent_finding_snapshot_file>{ };
		if (Window == nullptr || !SelectedDiffPublication.has_value()) {
			return files;
		}
		const auto* const workbench = Window->comparison_surface();
		if (workbench == nullptr || workbench->current_result() == nullptr
			|| workbench->current_result()->Alignment.Key
				!= SelectedDiffPublication->Key) {
			return files;
		}
		const auto& alignment = workbench->current_result()->Alignment;
		auto        file      = core::agent_finding_snapshot_file{
			.Path           = SelectedDiffPublication->Request.File.PresentationPath,
			.Comparison     = alignment.Digest,
			.LeftLineCount  = alignment.LeftLineCount,
			.RightLineCount = alignment.RightLineCount,
		};
		file.Segments.reserve(alignment.Segments.size());
		for (const auto& segment : alignment.Segments) {
			file.Segments.push_back(segment.Id);
		}
		file.Groups.reserve(alignment.Groups.size());
		for (const auto& group : alignment.Groups) {
			file.Groups.push_back(group.Id);
		}
		file.Moves.reserve(alignment.Moves.size());
		for (const auto& move : alignment.Moves) {
			file.Moves.push_back(move.Id);
		}
		files.push_back(std::move(file));
		return files;
	}

	void github_application::implementation::present_agent_refusal(
		const std::string_view aSubject, const std::string_view aReason)
	{
		if (Window == nullptr || Window->agent_panel() == nullptr) {
			return;
		}
		Window->agent_panel()->refuse(
			std::string{ aSubject } + " refused: " + std::string{ aReason });
	}

	/// \brief Starts one agent turn for one asked-for workshop operation.
	///
	/// \details The reviewer's half of the workshop: the operation is
	/// admitted under the one entry point it means anything for, held
	/// against the draft it was asked about, and handed to the same
	/// `dispatch_agent_invocation` a start gesture and a chat send use.
	/// There is no second launch path, no workshop agent, and no host of
	/// this method's own — the turn it starts is the one this application
	/// already knows how to start, with `workshop_comment` named as the
	/// gesture and the operation in the supplement.
	///
	/// Nothing here writes a draft revision. The press records the draft's
	/// text in Private history first ([[BPR-1245]]); the answer arrives as
	/// this turn's outcome and is proposed by the composed workshop host;
	/// inventing text here would be this composition answering a question
	/// it forwarded.
	///
	/// One press, one turn. A second press while the first turn is still
	/// unanswered is refused and named rather than queued: a queued press
	/// would either start a second turn about a comment the first is
	/// already revising, or sit silently until the reviewer concluded the
	/// control did nothing.
	void github_application::implementation::ask_comment_workshop(const qt::comment_workshop_request& aRequest)
	{
		const auto admitted = core::workshop_operation_for(
			core::agent_entry_point::workshop_comment, aRequest.Operation);
		if (!admitted.has_value()) {
			present_agent_refusal("comment workshop", "unknown_operation");
			return;
		}
		if (!ReviewDraft.has_value() || ReviewDraft->Id != aRequest.Draft) {
			present_agent_refusal(
				"comment workshop",
				application::name(application::
									  agent_comment_workshop_refusal::
										  different_draft));
			return;
		}
		// One press, one *running* turn. A record left over from a turn
		// that has already ended is not a turn in flight: it is only the
		// name of the operation the next outcome may still be labelled
		// with, and asking again replaces it below.
		if (PendingWorkshopOperation.has_value()
			&& PendingWorkshopOperation->Running) {
			present_agent_refusal(
				"comment workshop", "workshop_turn_in_flight");
			return;
		}
		if (!PrivateReview) {
			present_comment_workshop_settlement(
				aRequest.Draft, "Private history is unavailable");
			return;
		}
		PendingWorkshopOperation = pending_comment_workshop{
			.Draft     = aRequest.Draft,
			.Operation = *admitted,
		};
		if (PrivateDraftCommandPending && ReviewDraft->Id.value() > core::maximum_private_identity) {
			PendingWorkshopOperation->AwaitingMaterialization = core::revisioned_review_draft{ *ReviewDraft, ReviewDraftRevision, DraftScope };
			return;
		}
		record_comment_workshop_press();
	}

	void github_application::implementation::settle_materialized_comment_workshop(
		const core::revisioned_review_draft&                aPlaceholder,
		const std::optional<core::revisioned_review_draft>& aAccepted)
	{
		if (!PendingWorkshopOperation || PendingWorkshopOperation->AwaitingMaterialization != aPlaceholder) {
			return;
		}
		if (Closing || !aAccepted || !ReviewDraft || ReviewDraft->Id != aAccepted->Draft.Id
			|| aAccepted->Scope != aPlaceholder.Scope || aAccepted->Draft.Anchor != aPlaceholder.Draft.Anchor
			|| aAccepted->Draft.ReplyToThread != aPlaceholder.Draft.ReplyToThread) {
			PendingWorkshopOperation.reset();
			present_comment_workshop_settlement(aPlaceholder.Draft.Id, "the draft could not be saved in Private history");
			return;
		}
		PendingWorkshopOperation->Draft = aAccepted->Draft.Id;
		PendingWorkshopOperation->AwaitingMaterialization.reset();
		// Queue the immutable press before acknowledging the edit: the
		// acknowledgement can dispatch text typed after the press. Both
		// effects use the same FIFO private-review lane.
		record_comment_workshop_press();
	}

	void github_application::implementation::record_comment_workshop_press()
	{
		// [[BPR-1245]], RFC-BPR-0023. The draft is mutable, so it is not
		// sent. The press first records the draft's text, as the private
		// lane holds it after every edit already queued, as one immutable
		// reviewer revision in Private history, and asks with that
		// revision as the turn's only selected source. No private notes
		// cross: the reviewer asked how to say this better, not what else
		// is known. Until the revision is written the six stay shut, so
		// there is still one press per turn.
		const auto press = PendingWorkshopOperation;
		PrivateReview->record_workshop_press(
			press->Draft,
			[this, press](
				application::private_run_result<application::private_workshop_press> aPressed) {
				if (Closing || PendingWorkshopOperation != press) {
					return;
				}
				auto sent = false;
				if (aPressed) {
					PendingWorkshopOperation->Recording = false;
					ExplicitPrivateRun                  = std::move(aPressed->Run);
					sent                                = dispatch_agent_invocation(
						core::codex_jsonl_profile_id,
						std::vector<application::local_note_id>{ },
						std::nullopt,
						false,
						std::string_view{ },
						std::nullopt,
						core::agent_entry_point::workshop_comment,
						press->Operation);
					ExplicitPrivateRun.reset();
				}
				if (!sent) {
					// The launch panel or Private history has already
					// named why. The operation is cleared rather than left
					// recorded, because a question no turn was started for
					// is not waiting for an answer, and a later unrelated
					// revision must not be labelled with it.
					PendingWorkshopOperation.reset();
					// BPR-0538. And said on the surface that asked. The
					// reviewer is looking at the six controls that just
					// shut, and they reopen here with the reason where the
					// answer would have been.
					present_comment_workshop_settlement(
						press->Draft,
						aPressed ? "the turn was refused before it started"
								 : "the draft could not be saved in Private history");
				}
			});
	}

	/// \brief Ends the press whose turn has just ended.
	///
	/// \details BPR-0538. The third sink's whole decision, in one place.
	/// Three questions in order, and all three are about *this* turn:
	///
	/// - Is this the turn the press is waiting on? A press bound to
	///   another session is untouched, so a second, unrelated turn
	///   finishing — a launch-panel start, a chat send — neither reopens
	///   the six nor consumes the operation the press is holding.
	/// - Whatever it produced, it is over, so the six reopen. The tooltip
	///   they carried said a turn was running; it is not.
	/// - Did it produce anything the reviewer could still convert into the
	///   revision that was asked for? If it did, the record stays so that
	///   conversion is still labelled with the operation that produced it.
	///   If it did not — no finding delivered, a failed run, a session the
	///   review moved out from under — nothing is coming, so the record
	///   goes and the surface says why.
	///
	/// Nothing here retries and nothing here proposes: a turn that
	/// answered nothing is reported as having answered nothing.
	void github_application::implementation::settle_comment_workshop(
		const core::agent_session_id         aSession,
		const application::agent_run_outcome aOutcome)
	{
		if (!PendingWorkshopOperation.has_value()
			|| !PendingWorkshopOperation->Session.has_value()
			|| *PendingWorkshopOperation->Session != aSession) {
			return;
		}
		PendingWorkshopOperation->Running = false;
		const auto draft                  = PendingWorkshopOperation->Draft;
		const auto answered               = PendingWorkshopOperation->Delivered
			&& aOutcome == application::agent_run_outcome::success;
		auto reason = std::string{ };
		if (!answered) {
			reason = aOutcome == application::agent_run_outcome::success
				? std::string{ "the turn proposed no revision" }
				: "the turn ended "
					+ std::string{ application::name(aOutcome) };
			PendingWorkshopOperation.reset();
		}
		present_comment_workshop_settlement(draft, reason);
	}

	/// \brief Reopens the six controls, and says why if nothing is coming.
	///
	/// \details Presented on the workshop surface rather than on the
	/// launch panel: the reviewer pressed a control beside their own text
	/// and is watching the space the before/after would fill. A reason
	/// delivered to the invocation panel is a reason they are not reading.
	void github_application::implementation::present_comment_workshop_settlement(
		const core::review_draft_id aDraft, const std::string& aReason)
	{
		if (Window == nullptr || Window->review_editor() == nullptr) {
			return;
		}
		Window->review_editor()->present(qt::comment_workshop_settlement{
			.Draft  = aDraft,
			.Reason = aReason,
		});
	}

	/// \brief Presents what one asked-for finding action produced.
	///
	/// \details A promoted comment becomes a *proposal* rather than a
	/// draft: the reviewer reads the before and the after on the shipped
	/// editor and accepts it there, which is the one gesture that emits a
	/// local draft command. Nothing here writes, and nothing here reaches
	/// the review-mutation host.
	void github_application::implementation::present_finding_outcome(
		const application::agent_finding_outcome& aOutcome)
	{
		if (aOutcome.Draft.has_value() && WorkshopHost != nullptr
			&& ReviewDraft.has_value()) {
			// The operation the reviewer asked for, spent by the turn it
			// was asked in and by no other. The pairing is the session
			// identity carried from the press: a finding promoted from a
			// different turn — or from this one before the workshop turn
			// was admitted — is a revision nobody workshopped, and
			// labelling it with a pending operation would name a question
			// that produced none of this text.
			const auto matched = PendingWorkshopOperation.has_value()
				&& PendingWorkshopOperation->Session.has_value()
				&& *PendingWorkshopOperation->Session == aOutcome.Session
				&& PendingWorkshopOperation->Draft == ReviewDraft->Id;
			const auto asked = matched
				? std::optional<core::workshop_operation>{
					  PendingWorkshopOperation->Operation
				  }
				: std::optional<core::workshop_operation>{ };
			if (matched) {
				PendingWorkshopOperation.reset();
			}
			const auto refusal = WorkshopHost->propose(
				*ReviewDraft, *aOutcome.Draft, asked);
			if (refusal == application::agent_comment_workshop_refusal::none
				&& Window != nullptr && Window->review_editor() != nullptr
				&& LatestAgentSession.has_value()
				&& LatestAgentSession->Id == aOutcome.Session) {
				Window->open_draft_in_discuss();
				Window->present_dif_workshop(
					*LatestAgentSession,
					aOutcome.ActionGeneration,
					aOutcome.Draft->Id,
					ReviewDraftRevision,
					aOutcome.Draft->Body);
			}
			return;
		}
		if (aOutcome.Defect.has_value()) {
			dispatch_defect(*aOutcome.Defect);
			return;
		}
		if (aOutcome.Note.has_value()) {
			refresh_agent_private_context_choices();
			return;
		}
		// A refused conversion is presented, not dropped. A reviewer who
		// is shown nothing cannot tell a refusal from a swallowed gesture.
		++FindingRefusalsPresented;
		present_agent_refusal(
			application::name(aOutcome.Action),
			aOutcome.Refusal
					!= application::agent_finding_action_refusal::none
				? application::name(aOutcome.Refusal)
				: promotion_refusal_name(aOutcome.Promotion));
	}

	auto github_application::implementation::next_private_note_id() const
		-> application::local_note_id
	{
		auto next = std::uint64_t{ 1U };
		if (ReviewLedgerHost != nullptr) {
			if (const auto observed = ReviewLedgerHost->private_observation();
				observed.has_value()) {
				for (const auto& note : observed->Notes) {
					next = std::max(next, note.Id.value() + 1U);
				}
			}
		}
		return application::local_note_id{ next };
	}
#endif
	auto github_application::implementation::current_note_attachment() const
		-> std::optional<application::local_note_attachment>
	{
		if (!SelectedDiffPublication.has_value()) {
			return std::nullopt;
		}
		return application::local_note_attachment{
			.File       = SelectedDiffPublication->Request.File.File,
			.Comparison = core::comparison_digest(SelectedDiffPublication->Key),
		};
	}
#if defined(BPR_AGENT_INTEGRATIONS)
	/// \brief The durable defect anchored exactly where the finding is.
	///
	/// \details Nothing is moved onto a nearby defect. `promote_agent_finding`
	/// refuses `anchor_elsewhere` when the two disagree, and supplying the
	/// nearest one here would be that refusal arriving from the other
	/// direction.
	auto github_application::implementation::anchored_durable_defect(
		const core::agent_finding& aFinding) const
		-> std::optional<core::defect_record>
	{
		if (!aFinding.Anchor.Line.has_value()) {
			return std::nullopt;
		}
		for (const auto& durable : DurableDefects) {
			if (durable.History.Defect.Anchor == *aFinding.Anchor.Line) {
				return durable.History.Defect;
			}
		}
		return std::nullopt;
	}

	void github_application::implementation::dispatch_finding_intent(qt::finding_action_intent aIntent)
	{
		if (Closing || FindingHost == nullptr) {
			return;
		}
		auto request = application::agent_finding_action_request{
			.Session = aIntent.Subject.Session,
			.Ordinal = aIntent.Subject.Ordinal,
			.Action  = composed_finding_action(aIntent.Action),
		};
		switch (request.Action) {
		case application::agent_finding_action::convert_to_draft_comment:
			request.Draft = ReviewDraft;
			break;
		case application::agent_finding_action::convert_to_draft_defect:
			request.Defect = anchored_durable_defect(aIntent.Subject.Finding);
			break;
		case application::agent_finding_action::add_to_private_notes:
			request.Note       = next_private_note_id();
			request.Attachment = current_note_attachment();
			break;
		case application::agent_finding_action::open_evidence:
		case application::agent_finding_action::ask_follow_up:
		case application::agent_finding_action::dismiss:
			break;
		}
		FindingHost->act(request);
	}

	/// \brief Carries one side-chat gesture to the composed chat host.
	///
	/// \details Translation and nothing else. The Qt intents are restated
	/// as the application's own request values because this host may not
	/// depend on the adapter that emits them, and every decision below the
	/// translation belongs to `agent_chat_session_host` and, through it, to
	/// the core. With the chat host cut, a send is refused by name rather
	/// than quietly doing nothing.
	void github_application::implementation::dispatch_side_chat_intent(qt::side_chat_intent aIntent)
	{
		++SideChatIntents;
		if (ChatHost == nullptr) {
			if (std::holds_alternative<qt::send_chat_message_intent>(
					aIntent)) {
				present_agent_refusal(
					"side chat", "no composed chat session host");
			}
			return;
		}
		if (const auto* const send
			= std::get_if<qt::send_chat_message_intent>(&aIntent);
			send != nullptr) {
			// Read at the gesture, from the alignment on screen. A
			// selection observed earlier would answer for where the
			// reviewer was, not where they are.
			ChatHost->observe_selection(current_agent_chat_selection());
			// [[BPR-1817]]. And to the agent the composer's menu shows,
			// read at the gesture as well. Before this the turn went to
			// the chat's default profile whatever was chosen, so an added
			// agent was never asked, and a server that refused the default
			// dropped the question.
			if (Window != nullptr && Window->agent_panel() != nullptr) {
				if (const auto chosen = Window->agent_panel()->selected_profile()) {
					ChatHost->choose_agent(*chosen);
				}
			}
			// [[BPR-1773]]. A note is mutable, so it is not sent: its text
			// is first saved in Private history as one revision, and the
			// turn runs with that revision as its only private source.
			if (AskedConversation && AskedConversation->Note) {
				const auto* const note = asked_note();
				if (note == nullptr || !PrivateReview) {
					dismiss_asked_conversation();
					present_agent_refusal("side chat", "the note asked about is gone or Private history is unavailable");
					return;
				}
				PrivateReview->record_note_press(note->Note.Text, [this, turn = *send](application::private_run_result<application::private_workshop_press> aPressed) {
					if (Closing || ChatHost == nullptr) {
						return;
					}
					if (!aPressed) {
						present_agent_refusal("side chat", "the note could not be saved in Private history");
						return;
					}
					ExplicitPrivateRun = std::move(aPressed->Run);
					static_cast<void>(ChatHost->send(application::agent_chat_send{
						.Session = turn.Session,
						.Action  = core::agent_action_intent{ core::agent_entry_point::ask_about_conversation },
						.Scope   = turn.Scope,
						.Text    = turn.Text,
					}));
					ExplicitPrivateRun.reset();
				});
				return;
			}
			// [[BPR-1773]]. With a conversation attached the turn is
			// about it, whatever is selected.
			const auto action = AskedConversation ? core::agent_action_intent{ core::agent_entry_point::ask_about_conversation } : send->Action;
			static_cast<void>(
				ChatHost->send(application::agent_chat_send{
					.Session = send->Session,
					.Action  = action,
					.Scope   = send->Scope,
					.Text    = send->Text,
				}));
			return;
		}
		if (const auto* const widen
			= std::get_if<qt::widen_chat_scope_intent>(&aIntent);
			widen != nullptr) {
			static_cast<void>(
				ChatHost->widen(application::agent_chat_widening{
					.Session  = widen->Session,
					.Widening = widen->Widening,
				}));
			return;
		}
		if (const auto* const optIn
			= std::get_if<qt::chat_private_context_intent>(&aIntent);
			optIn != nullptr) {
			static_cast<void>(ChatHost->set_private_context(optIn->OptIn));
			return;
		}
		if (const auto* promote = std::get_if<qt::promote_chat_reply_intent>(&aIntent)) {
			const auto projection = ChatHost->projection();
			if (!projection || projection->Session.Id != promote->Session) {
				return;
			}
			const auto found  = std::ranges::find(projection->Replies, promote->Reply, &application::agent_chat_reply::Id);
			const auto refuse = [this] {
				present_agent_refusal("promote reply", "select a current-head Comment target and resolve any draft conflict");
			};
			if (found == projection->Replies.end()) {
				refuse();
			} else {
				promote_private_item(found->Text, refuse);
			}
			return;
		}
		const auto& choice
			= std::get<qt::choose_chat_continuation_intent>(aIntent);
		const auto outcome = ChatHost->choose(application::agent_chat_continuation_choice{
			.Session = choice.Session, .Choice = choice.Choice, .OfferedBinding = choice.OfferedBinding });
		if (outcome.Started && choice.Choice == core::agent_continuation::start_new_session) {
			NativeFreshAtCurrentRevision = outcome.Started;
		}
	}

	/// \brief Says on the panel what one chat gesture produced.
	///
	/// \details A refusal is presented, never dropped. The reason is the
	/// host's own name for it, which carries the core's verdict where there
	/// is one, so a reviewer whose send was refused is told which of the
	/// core's answers refused it rather than watching a chat do nothing.
	void github_application::implementation::present_chat_outcome(const application::agent_chat_outcome& aOutcome)
	{
		if (aOutcome.Refusal == application::agent_chat_refusal::none) {
			if (aOutcome.Turn.has_value()) {
				++SideChatTurns;
			}
			return;
		}
		++SideChatRefusals;
		auto reason = std::string{ application::name(aOutcome.Refusal) };
		switch (aOutcome.Refusal) {
		case application::agent_chat_refusal::session_stale:
			reason += " (";
			reason += core::stable_name(aOutcome.Staleness);
			reason += ')';
			break;
		case application::agent_chat_refusal::action_refused:
			reason += " (";
			reason += core::stable_name(aOutcome.Action);
			reason += ')';
			break;
		case application::agent_chat_refusal::scope_refused:
			reason += " (";
			reason += core::stable_name(aOutcome.Scope);
			reason += ')';
			break;
		case application::agent_chat_refusal::none:
		case application::agent_chat_refusal::no_session:
		case application::agent_chat_refusal::other_session:
		case application::agent_chat_refusal::private_context_refused:
		case application::agent_chat_refusal::empty_message:
		case application::agent_chat_refusal::unknown_continuation:
		case application::agent_chat_refusal::binding_mismatch:
		case application::agent_chat_refusal::launch_refused:
			break;
		}
		// [[BPR-1817]]. The launch path names its own refusal before the
		// chat's answer arrives; it is the detail of `launch_refused`.
		const auto launchSaid = aOutcome.Send == core::agent_send_refusal::launch_refused && Window != nullptr && Window->agent_panel() != nullptr
			? Window->agent_panel()->last_refusal()
			: QString{ };
		present_agent_refusal("side chat", reason);
		// And said beside the composer, in the core's vocabulary.
		if (aOutcome.Send != core::agent_send_refusal::none && Window != nullptr && Window->agent_tab() != nullptr) {
			Window->agent_tab()->refuse_send(aOutcome.Send, launchSaid);
		}
	}

	/// \brief Every private note the review holds, by identity.
	auto github_application::implementation::all_private_note_ids() const
		-> std::vector<application::local_note_id>
	{
		auto notes = std::vector<application::local_note_id>{ };
		if (ReviewLedgerHost == nullptr) {
			return notes;
		}
		const auto observed = ReviewLedgerHost->private_observation();
		if (!observed.has_value()) {
			return notes;
		}
		notes.reserve(observed->Notes.size());
		for (const auto& note : observed->Notes) {
			notes.push_back(note.Id);
		}
		return notes;
	}

	/// \brief The draft a chat turn may carry, when there is one to carry.
	auto github_application::implementation::current_agent_draft_choice() const
		-> std::optional<application::agent_draft_choice>
	{
		const auto draft = current_agent_draft_observation();
		if (
			!draft.has_value()
			|| draft->Draft.State != core::review_draft_state::drafting) {
			return std::nullopt;
		}
		return draft->Choice;
	}

	/// \brief The reviewer's own prose, as the core admission takes it.
	///
	/// \details Read at call time and never held. This is the *material*
	/// the core decides about; whether any of it crosses is
	/// `core::admit_private_context`'s answer, keyed by session.
	auto github_application::implementation::current_chat_private_prose() const
		-> application::agent_chat_private_prose
	{
		auto prose = application::agent_chat_private_prose{ };
		if (ReviewLedgerHost != nullptr) {
			if (const auto observed = ReviewLedgerHost->private_observation();
				observed.has_value()) {
				prose.Notes.reserve(observed->Notes.size());
				for (const auto& note : observed->Notes) {
					prose.Notes.push_back(note.Note);
				}
			}
		}
		if (const auto draft = current_agent_draft_observation();
			draft.has_value()
			&& draft->Draft.State == core::review_draft_state::drafting) {
			prose.Drafts.push_back(
				core::supplement_draft{ .Text = draft->Draft.Body });
		}
		return prose;
	}

	/// \brief Shows the chat the composed host is holding, and nothing else.
	///
	/// \details Every revision-bearing field comes from the host's own
	/// projection, which carries the *session's* binding. The composition
	/// adds only the four fields the core has no opinion about — provider
	/// name, Git host, workspace, and chips — so there is still no path by
	/// which the head now under review reaches this widget.
	void github_application::implementation::present_side_chat(const application::agent_chat_projection& aChat)
	{
		if (SideChat == nullptr || !AgentReview.has_value()) {
			return;
		}
		++SideChatPresentations;
		auto replies = std::vector<qt::side_chat_reply>{ };
		for (const auto& reply : aChat.Replies) {
			replies.push_back({ reply.Id, reply.Text });
		}
		SideChat->present(qt::side_chat_projection{
			.Session        = aChat.Session,
			.Continuations  = aChat.Continuations,
			.Chat           = aChat.Chat,
			.PrivateContext = aChat.PrivateContext,
			.EntryPoint     = aChat.EntryPoint,
			.Provider       = agent_provider_name(aChat.Session.Profile),
			.GitHost        = AgentReview->Hostname,
			.Workspace      = LastAgentWorkspacePath,
			.Replies        = std::move(replies),
		});
		Window->present_dif_continuation(
			aChat.Session, aChat.Continuations);
	}

	/// \brief Shows the chat when its host was cut out of the composition.
	///
	/// \details The panel stays on screen and stays truthful: it is handed
	/// the session it would have, presented as current, so the send gesture
	/// is still reachable and still observable. What is gone is the turn —
	/// `dispatch_side_chat_intent` refuses it by name — which is exactly
	/// what a construction cut is supposed to leave behind.
	void github_application::implementation::present_side_chat(const core::agent_revision_binding& aBinding)
	{
		if (SideChat == nullptr || !AgentReview.has_value()) {
			return;
		}
		const auto session = core::agent_session{
			.Id      = core::agent_session_id{ 1U },
			.Profile = core::codex_jsonl_profile_id,
			.Binding = aBinding,
		};
		present_side_chat(application::agent_chat_projection{
			.Session       = session,
			.Continuations = core::offer_agent_continuations(session, aBinding),
			.Chat          = core::agent_chat_state{
				.Scope = core::agent_chat_scope{
					.Kind = core::agent_chat_scope_kind::pull_request,
				},
			},
			.PrivateContext = core::agent_private_context_optin{
				.Session = session.Id,
			},
			.EntryPoint = core::agent_entry_point::ask_about_selection,
		});
	}

	auto github_application::implementation::agent_provider_name(
		const core::agent_profile_id aProfile) -> std::string
	{
		const auto profiles = core::built_in_agent_profiles();
		for (const auto& profile : profiles) {
			if (profile.Id == aProfile) {
				return profile.DisplayName;
			}
		}
		return { };
	}
#endif
}
