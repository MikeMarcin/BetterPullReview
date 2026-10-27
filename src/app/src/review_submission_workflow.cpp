#include "review_submission_workflow.hpp"

#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/review_tour_reconciliation.hpp"
#include "bpr/qt/github_cli_consent.hpp"
#include "tour_publication_plan.hpp"

#include "bpr/application/review_submission_recovery_port.hpp"

#include <QUuid>
#include <algorithm>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace bpr::app {
	using command_kind = core::review_submission_command_kind;
	using draft_state  = core::review_submission_state;

	namespace {
		/// [[BPR-1085]]. Why a Submit gesture was refused, naming what changed.
		[[nodiscard]] auto changed_status(const std::vector<core::review_preflight_change>& aChanges) -> QString
		{
			if (aChanges.empty()) {
				return QStringLiteral("Not submitted: the review shown could not be confirmed. Check it and submit again.");
			}
			auto words = QStringList{ };
			for (const auto change : aChanges) {
				switch (change) {
				case core::review_preflight_change::draft:       words.push_back(QStringLiteral("review draft")); break;
				case core::review_preflight_change::account:     words.push_back(QStringLiteral("account")); break;
				case core::review_preflight_change::target:      words.push_back(QStringLiteral("pull request")); break;
				case core::review_preflight_change::head:        words.push_back(QStringLiteral("pull request revision")); break;
				case core::review_preflight_change::summary:     words.push_back(QStringLiteral("summary")); break;
				case core::review_preflight_change::disposition: words.push_back(QStringLiteral("review type")); break;
				case core::review_preflight_change::selection:   words.push_back(QStringLiteral("unsent comments")); break;
				case core::review_preflight_change::comments:    words.push_back(QStringLiteral("pending comments")); break;
				}
			}
			return QStringLiteral("Not submitted: the %1 changed after you chose Submit review. Check the review and submit again.").arg(words.join(QStringLiteral(", ")));
		}

		/// [[BPR-1345]] v002. A new review needs no words of its own: the
		/// page says what each verdict needs where it is (owner, finish-review
		/// v001: "remove some of this duplicate text").
		[[nodiscard]] auto new_draft_status() -> QString
		{
			return { };
		}

		/// [[BPR-1345]]. Why the pending comments cannot ride this review.
		[[nodiscard]] auto comments_status(const core::review_submission_rejection aRefusal) -> QString
		{
			switch (aRefusal) {
			case core::review_submission_rejection::too_many_comments:
				return QStringLiteral("This review holds more than %1 pending comments. Add some on their own with Add single comment, then finish the review.").arg(core::maximum_review_submission_comments);
			case core::review_submission_rejection::review_too_large:
				return QStringLiteral("This review is too long to send at once. Shorten the summary or add some comments on their own with Add single comment.");
			case core::review_submission_rejection::blank_comment:
				return QStringLiteral("A pending comment has no text. Write it or discard it, then submit the review.");
			default:
				return QStringLiteral("A pending comment cannot be sent with this review. Open it beside the code to check it.");
			}
		}
	}

	review_submission_workflow::review_submission_workflow(application::effect_executor&   aEffects,
														   application::decision_executor& aDecisions, qt::review_submission_editor& aEditor, grant_factory aIssueGrant, publication_admission aAdmitPublication)
		: Effects{ aEffects }
		, Decisions{ aDecisions }
		, Editor{ aEditor }
		, IssueGrant{ std::move(aIssueGrant) }
		, AdmitPublication{ std::move(aAdmitPublication) }
	{ }

	void review_submission_workflow::adopt(application::review_submission_live_binding   aBinding,
										   application::workbench_pull_request_authority aAuthority, application::review_submission_provider_factory aFactory, application::insights_review_capture aInsights)
	{
		if (Closing) {
			return;
		}
		// Snapshot refresh and file selection do not replace authentication authority.
		aAuthority.Snapshot              = { };
		aAuthority.PullRequestGeneration = { };
		if (Host && (Ready || Context) && Current && Current->Binding == aBinding && Current->Authority == aAuthority) {
			return;
		}
		Next = selection{ std::move(aBinding), std::move(aAuthority), std::move(aFactory), std::move(aInsights) };
		retire();
		Status = QStringLiteral("Updating review authority…");
		present();
		start_selection();
	}

	void review_submission_workflow::retire()
	{
		Ready = false;
		Context.reset();
		if (Host) {
			Host->shutdown();
			Retired.push_back(std::move(Host));
		}
		std::erase_if(Retired, [](const auto& aHost) { return aHost->is_quiescent(); });
	}

	void review_submission_workflow::start_selection()
	{
		std::erase_if(Retired, [](const auto& aHost) { return aHost->is_quiescent(); });
		if (Closing || !Next || !Retired.empty()) {
			return;
		}
		if (DraftBinding && !application::same_review_submission_identity(*DraftBinding, Next->Binding)) {
			// A different account or PR receives a new empty draft. Any admitted
			// attempt remains recoverable in its original durable scope.
			Draft.reset();
			DraftBinding.reset();
			Pending.reset();
		}
		Unresolved.clear();
		Lookups.clear();
		LookedUp.clear();
		LookupKey.reset();
		Recovering.reset();
		Current = std::move(Next);
		Next.reset();
		if (Generation.value() == std::numeric_limits<std::uint64_t>::max()) {
			Status = QStringLiteral("Review session exhausted. Reopen the application.");
			present();
			return;
		}
		Generation = core::request_generation{ Generation.value() + 1U };
		Host       = std::make_unique<application::review_submission_application_host>(Current->Factory, Current->Binding,
																					   Current->Authority.ApplicationSession, Generation, Effects, Decisions, *this, Current->Insights);
		Status     = QStringLiteral("Checking for an unfinished review…");
		Context    = Host->discover();
		if (!Context) {
			Status = QStringLiteral("Review recovery is unavailable. Refresh the pull request to try again.");
		}
		present();
	}

	void review_submission_workflow::invalidate(const application::pull_request_authority_invalidation& aInvalidation)
	{
		const auto matches = [&aInvalidation](const selection& aSelection) {
			const auto& authority = aSelection.Authority;
			return authority.ApplicationSession == aInvalidation.ApplicationSession
				&& authority.Profile == aInvalidation.Profile && authority.ProfileRevision == aInvalidation.ProfileRevision
				&& authority.Host == aInvalidation.Host && authority.HostAuthority == aInvalidation.HostAuthority
				&& authority.Account == aInvalidation.Account && authority.Viewer == aInvalidation.Viewer
				&& authority.Repository == aInvalidation.Repository && authority.PullRequest == aInvalidation.PullRequest
				&& authority.IdentityGeneration == aInvalidation.IdentityGeneration;
		};
		if (Next ? matches(*Next) : Current && matches(*Current)) {
			invalidate();
		}
	}

	void review_submission_workflow::invalidate(QString aReason)
	{
		Next.reset();
		retire();
		Status = aReason.isEmpty()
			? QStringLiteral("Review authority changed. Sign in and reopen this pull request to continue. Any existing draft is retained read-only until its original account and revision are available.")
			: std::move(aReason);
		present();
	}

	void review_submission_workflow::shutdown()
	{
		Closing = true;
		Next.reset();
		retire();
	}

	auto review_submission_workflow::is_quiescent() const -> bool
	{
		return (!Host || Host->is_quiescent()) && std::ranges::all_of(Retired, [](const auto& aHost) { return aHost->is_quiescent(); });
	}

	void review_submission_workflow::new_draft()
	{
		if (!Current || NextDraft == std::numeric_limits<std::uint64_t>::max()) {
			return;
		}
		DraftBinding = Current->Binding;
		Unknown.reset();
		Draft = core::review_submission_draft{ .Id            = core::review_submission_draft_id{ NextDraft++ },
											   .Target        = DraftBinding->Target.Semantic,
											   .ActingAccount = DraftBinding->ActingIdentity.Account };
		Pending.reset();
		Status = new_draft_status();
		apply_review_comments();
	}

	void review_submission_workflow::set_review_comment_hooks(comment_lock aLock, review_answer aAnswer, review_discard aDiscard)
	{
		LockComments  = std::move(aLock);
		Answer        = std::move(aAnswer);
		DiscardReview = std::move(aDiscard);
	}

	void review_submission_workflow::set_publication_observer(publication_observer aObserver) { PublicationObserved = std::move(aObserver); }
	void review_submission_workflow::set_unplaceable_refusal_observer(unplaceable_refusal_observer aObserver) { UnplaceableRefusalObserved = std::move(aObserver); }

	void review_submission_workflow::observe_review_comments(std::vector<core::review_submission_comment> aComments)
	{
		if (Closing || aComments == Comments) {
			return;
		}
		Comments = std::move(aComments);
		apply_review_comments();
		present();
	}

	void review_submission_workflow::apply_review_comments()
	{
		// [[BPR-1345]] v002. A review that is finished, submitted or
		// discarded, is never the next one: when pending drafts arrive while
		// it is the draft shown, a new review starts for them. The owner's
		// v001 flow kept a discarded summary draft here, so the next review's
		// comments were never carried and finishing it said "Review draft
		// discarded.".
		if (Draft && (Draft->State == draft_state::submitted || Draft->State == draft_state::discarded) && !Comments.empty() && !Pending && Unresolved.empty()
			&& !Locking && Current && Ready) {
			new_draft();
			return;
		}
		// [[BPR-1345]]. Only a review still being written takes the pending
		// drafts; Submit freezes the ones it carries.
		if (!Draft || Draft->State != draft_state::drafting) {
			return;
		}
		// [[BPR-1715]]. The pending drafts are the current revision's.
		// A review frozen on an earlier revision takes none of them, so it
		// cannot list them or delete them when it is discarded.
		const auto frozen  = Current && DraftBinding != std::optional{ Current->Binding };
		auto       outcome = core::apply(*Draft, { .Id = Draft->Id, .Kind = command_kind::set_comments, .Comments = frozen ? std::vector<core::review_submission_comment>{ } : Comments });
		if (outcome.Draft) {
			Draft           = std::move(outcome.Draft);
			CommentsRefusal = core::review_comments_rejection(Draft->Body, Draft->Comments, true);
			if (CommentsRefusal == core::review_submission_rejection::blank_comment) {
				// Shown, and refused by Submit itself.
				CommentsRefusal = core::review_submission_rejection::none;
			}
			return;
		}
		CommentsRefusal = outcome.Rejection;
		if (auto cleared = core::apply(*Draft, { .Id = Draft->Id, .Kind = command_kind::set_comments }); cleared.Draft) {
			Draft = std::move(cleared.Draft);
		}
	}

	void review_submission_workflow::resolve_held(std::vector<std::string> aKeys)
	{
		if (Closing) {
			return;
		}
		const auto busy = Context.has_value();
		for (auto& key : aKeys) {
			const auto settling = (Pending && Pending->IdempotencyKey == key) || (LookupKey && *LookupKey == key)
				|| (Recovering && Recovering->IdempotencyKey == key);
			if (!settling && std::ranges::find(Lookups, key) == Lookups.end() && std::ranges::find(LookedUp, key) == LookedUp.end()) {
				Lookups.push_back(std::move(key));
			}
		}
		next_lookup();
		// [[BPR-1778]]. A lookup holds every control until it answers; the
		// page shows that rather than verdicts the workflow would refuse.
		if (!busy && Context) {
			present();
		}
	}

	void review_submission_workflow::next_lookup()
	{
		if (Closing || !Ready || !Host || Context || Locking || Lookups.empty()) {
			return;
		}
		auto key = Lookups.front();
		Lookups.erase(Lookups.begin());
		LookedUp.push_back(key);
		if (Pending && Pending->IdempotencyKey == key) {
			// This workflow's own attempt answers for its drafts.
			next_lookup();
			return;
		}
		Context = Host->lookup(key);
		if (Context) {
			LookupKey = std::move(key);
		}
	}

	void review_submission_workflow::answer(const application::review_submission_request& aRequest, const application::review_submission_publication aPublication)
	{
		if (Answer && !aRequest.Proposal.Comments.empty() && aPublication != application::review_submission_publication::indeterminate) {
			Answer(aRequest.IdempotencyKey, aPublication, aRequest);
		}
	}

	void review_submission_workflow::restore(application::review_submission_request aRequest)
	{
		const auto& proposal = aRequest.Proposal;
		Draft                = core::review_submission_draft{ .Id = proposal.Draft, .Target = proposal.Target, .ActingAccount = proposal.ActingAccount, .Body = proposal.Body, .Disposition = proposal.Disposition, .State = draft_state::submitting, .Attempt = proposal.Attempt, .Comments = proposal.Comments };
		DraftBinding         = aRequest.LiveBinding;
		NextDraft            = std::max(NextDraft, proposal.Draft.value() == std::numeric_limits<std::uint64_t>::max() ? proposal.Draft.value() : proposal.Draft.value() + 1U);
		// [[BPR-1541]]. Its record says why it was left unknown, until the
		// next Check status replaces it.
		const auto kept = KeptUnknown.find(aRequest.IdempotencyKey);
		Unknown         = kept != KeptUnknown.end() ? std::optional{ kept->second } : std::nullopt;
		Pending         = std::move(aRequest);
		Status          = QStringLiteral("An unfinished review was restored. Check status before starting another review.");
	}

	auto review_submission_workflow::decided_controls() const -> core::review_submission_controls
	{
		if (!Draft || !Ready || !Host || Context || Closing || Locking) {
			return { };
		}
		auto result = core::controls(*Draft);
		if (!Current || DraftBinding != std::optional{ Current->Binding }) {
			result.Edit     = false;
			result.Submit   = false;
			result.SubmitAs = { };
		}
		// [[BPR-1345]]. Pending comments this review cannot carry stop Submit,
		// as every verdict.
		if (CommentsRefusal != core::review_submission_rejection::none) {
			result.Submit   = false;
			result.SubmitAs = { };
		}
		return result;
	}

	void review_submission_workflow::observe_preflight(application::review_preflight_observation aObservation)
	{
		if (Closing) {
			return;
		}
		Observation = std::move(aObservation);
		++PreflightInputs;
		present();
	}

	void review_submission_workflow::observe_selection(std::vector<core::review_preflight_draft>                        aSelection,
													   std::map<std::string, qt::review_submission_preflight_reference> aReferences)
	{
		if (Closing || (aSelection == Selection && aReferences == SelectionReferences)) {
			return;
		}
		Selection           = std::move(aSelection);
		SelectionReferences = std::move(aReferences);
		++PreflightInputs;
		present();
	}

	void review_submission_workflow::project_preflight()
	{
		if (!DraftBinding) {
			Preflight.reset();
			PreflightReferences.clear();
			PreflightProjected.reset();
			return;
		}
		const auto carried = Draft ? std::span<const core::review_submission_comment>{ Draft->Comments } : std::span<const core::review_submission_comment>{ };
		if (PreflightProjected && std::get<0>(*PreflightProjected) == PreflightInputs && std::get<1>(*PreflightProjected) == *DraftBinding && std::ranges::equal(std::get<2>(*PreflightProjected), carried)) {
			return;
		}
		PreflightProjected = preflight_key{ PreflightInputs, *DraftBinding, { carried.begin(), carried.end() } };
		Preflight.reset();
		PreflightReferences.clear();
		// [[BPR-1085]]. Only a read of this draft's account and revision is
		// its context; any other is unknown, never "none".
		if (!Observation || Observation->Account != DraftBinding->ActingIdentity.Account
			|| Observation->Head != core::canonical_hex(DraftBinding->Target.Semantic.ExpectedHead).view()) {
			return;
		}
		// [[BPR-1605]]. The comments this review carries are mine and not sent,
		// as the composer's draft is: each is read once, by its draft.
		auto drafts     = Selection;
		auto references = SelectionReferences;
		for (const auto& comment : carried) {
			auto id = "draft:" + std::to_string(comment.Draft.value());
			if (std::ranges::any_of(drafts, [&id](const core::review_preflight_draft& aDraft) { return aDraft.Id == id; })) {
				continue;
			}
			const auto annotation = core::recognize_feedback_prefix(comment.Body);
			auto       subject    = annotation.Subject ? core::feedback_text(comment.Body, *annotation.Subject) : std::string_view{ comment.Body };
			subject               = subject.substr(0U, subject.find('\n'));
			const auto slash      = comment.Path.find_last_of('/');
			const auto file       = slash == std::string::npos ? std::string_view{ comment.Path } : std::string_view{ comment.Path }.substr(slash + 1U);
			references.emplace(id, qt::review_submission_preflight_reference{ .Where = QStringLiteral("%1, line %2").arg(QString::fromUtf8(file.data(), static_cast<qsizetype>(file.size()))).arg(comment.Line + 1U), .Subject = QString::fromUtf8(subject.data(), static_cast<qsizetype>(subject.size())).trimmed() });
			drafts.push_back({ .Id = std::move(id), .Body = comment.Body, .Selected = true });
		}
		Preflight = application::project_review_preflight(*Observation, drafts);
		if (!Preflight) {
			return;
		}
		const auto text = [](const std::string& aText) { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); };
		for (const auto& blocker : Preflight->Blockers) {
			if (const auto found = Observation->References.find(blocker.Conversation); found != Observation->References.end()) {
				PreflightReferences.emplace(blocker.Conversation, qt::review_submission_preflight_reference{ text(found->second.Where), text(found->second.Subject) });
			}
		}
		for (const auto& pending : Preflight->Pending) {
			if (const auto found = references.find(pending.Draft); found != references.end()) {
				PreflightReferences.emplace(pending.Draft, found->second);
			}
		}
	}

	void review_submission_workflow::present()
	{
		QString target;
		if (DraftBinding) {
			const auto& binding = *DraftBinding;
			target              = QStringLiteral("%1 · %2/%3#%4\nHead: %5\nSubmitting as %6 (account %7)")
									  .arg(QString::fromStdString(std::string{ binding.Target.Hostname.value() }))
									  .arg(QString::fromStdString(binding.Target.Repository.Owner))
									  .arg(QString::fromStdString(binding.Target.Repository.Name))
									  .arg(binding.Target.Number.value())
									  .arg(QString::fromStdString(std::string{ core::canonical_hex(binding.Target.Semantic.ExpectedHead).view() }))
									  .arg(QString::fromStdString(binding.ActingIdentity.Login))
									  .arg(binding.ActingIdentity.Account.value());
		}
		auto status = Status;
		if (Ready && Draft && Draft->State == draft_state::drafting && Current && DraftBinding != std::optional{ Current->Binding }) {
			// [[BPR-1715]]. Say what Discard deletes: the summary, never
			// a comment.
			status = QStringLiteral("The pull request revision changed after you wrote this summary. Discard starts a new review on the current revision; it deletes the summary, never a comment.");
		} else if (Draft && Draft->State == draft_state::drafting && CommentsRefusal != core::review_submission_rejection::none) {
			status = comments_status(CommentsRefusal);
		} else if (Locking) {
			status = QStringLiteral("Holding the pending comments for this review…");
		}
		project_preflight();
		Editor.present({ .Draft             = Draft,
						 .Controls          = decided_controls(),
						 .TargetDescription = std::move(target),
						 .Status            = std::move(status),
						 .Preflight         = Preflight,
						 .References        = PreflightReferences,
						 .Prepared          = Draft ? core::prepare_review_decision(*Draft, Selection) : std::nullopt,
						 .Unknown           = Unknown && Pending ? std::optional{ core::notice(*Unknown) } : std::nullopt });
	}

	void review_submission_workflow::complete(command_kind aKind)
	{
		if (!Draft) {
			return;
		}
		auto outcome = core::apply(*Draft, { .Id = Draft->Id, .Kind = aKind, .Attempt = Draft->Attempt });
		if (outcome.Draft) {
			Draft = std::move(outcome.Draft);
		}
	}

	void review_submission_workflow::reconcile()
	{
		if (!Host || !Pending || Context) {
			return;
		}
		Context = Host->reconcile(*Pending);
		Status  = Context ? QStringLiteral("Checking the original review. No new review is being submitted…")
						  : QStringLiteral("Review status is still unknown. Check status again.");
	}

	void review_submission_workflow::hold_comments(std::vector<core::review_draft_id> aDrafts)
	{
		UnplaceableDrafts = std::move(aDrafts);
	}

	void review_submission_workflow::dispatch(qt::review_submission_editor_intent aIntent)
	{
		// Refuse stale draft identity before touching the supplied summary.
		if (!Draft || aIntent.Command.Id != Draft->Id) {
			return;
		}
		const auto controls = decided_controls();
		using operation     = qt::review_submission_editor_operation;
		if (aIntent.Operation == operation::reconcile) {
			if (controls.Reconcile) {
				reconcile();
			}
		} else if (aIntent.Operation == operation::command) {
			const auto kind    = aIntent.Command.Kind;
			const auto edits   = kind == command_kind::edit || kind == command_kind::choose_disposition;
			const auto allowed = (edits && controls.Edit)
				|| (kind == command_kind::begin_submit && controls.Submit)
				|| (kind == command_kind::discard && controls.Discard);
			if (!allowed) {
				// [[BPR-1778]]. A refused gesture never passes in silence: the
				// page is shown again with the controls this workflow allows
				// and why, so a verdict it shows is one it accepts.
				if (kind == command_kind::begin_submit && (LookupKey || Recovering)) {
					Status = QStringLiteral("Not submitted yet: an earlier review's comments are still being checked. Submit again in a moment.");
				}
				present();
				return;
			}
			// [[BPR-1735]]. GitHub refuses a whole review carrying a comment it
			// cannot place; such a comment is published as a file comment or
			// deleted first.
			if (kind == command_kind::begin_submit && std::ranges::any_of(Draft->Comments, [this](const core::review_submission_comment& aComment) {
					return std::ranges::find(UnplaceableDrafts, aComment.Draft) != UnplaceableDrafts.end();
				})) {
				Status = QStringLiteral("A comment GitHub can't place in a review is held. Publish it as a file comment or delete it, then submit.");
				present();
				return;
			}
			if (kind == command_kind::begin_submit) {
				// [[BPR-1085]]. Submit binds what the editor showed when it was
				// pressed: the draft, its account and revision, the summary, the
				// decision and the selected drafts. A change to any of them since
				// refuses this gesture; context (a resolution, another label)
				// binds nothing and refuses nothing.
				const auto shown   = aIntent.Prepared;
				const auto current = core::prepare_review_decision(*Draft, Selection);
				const auto changes = shown && current ? core::review_decision_changes(*shown, *current) : std::vector<core::review_preflight_change>{ };
				if (!shown || !current || !changes.empty()) {
					Status = changed_status(changes);
					present();
					return;
				}
				if (AdmitPublication) {
					if (!DraftBinding) {
						Status = QStringLiteral("Not published. The current account or pull request changed. Your draft is kept.");
						present();
						return;
					}
					const auto confirmedDraft   = *Draft;
					const auto confirmedBinding = *DraftBinding;
					const auto admission        = AdmitPublication(confirmedBinding);
					if (!admission) {
						Status = qt::github_cli_publication_failure_text(admission.error(), QString::fromStdString(confirmedBinding.ActingIdentity.Login), QString::fromStdString(std::string{ confirmedBinding.Target.Hostname.value() }));
						if (Status.isEmpty()) {
							Status = QStringLiteral("Not published. The current account or pull request changed. Your draft is kept.");
						}
						present();
						return;
					}
					if (!Draft || *Draft != confirmedDraft || !Current
						|| DraftBinding != confirmedBinding || !decided_controls().Submit) {
						Status = QStringLiteral("Not published. The current account or pull request changed. Your draft is kept.");
						present();
						return;
					}
					const auto confirmed = core::prepare_review_decision(*Draft, Selection);
					if (!confirmed || !core::review_decision_changes(*shown, *confirmed).empty()) {
						Status = changed_status(confirmed ? core::review_decision_changes(*shown, *confirmed) : std::vector<core::review_preflight_change>{ });
						present();
						return;
					}
				}
			}
			// [[BPR-1345]] v002. Discard review deletes every pending comment
			// of the review with its summary (owner, discard-review v001:
			// "Discard review should delete all the review comments").
			const auto discardsReview = kind == command_kind::discard && !Draft->Comments.empty();
			const auto carried        = Draft->Comments;
			const auto carriedHead    = Draft->Target.ExpectedHead;
			auto       outcome        = core::apply(*Draft, aIntent.Command);
			if (!outcome.Draft) {
				Status = outcome.Rejection == core::review_submission_rejection::blank_comment || outcome.Rejection == core::review_submission_rejection::review_too_large
						|| outcome.Rejection == core::review_submission_rejection::too_many_comments
					? comments_status(outcome.Rejection)
					: QStringLiteral("The summary must be valid text, at most 64 KiB, and nonblank unless approving or carrying line comments.");
				present();
				return;
			}
			Draft = std::move(outcome.Draft);
			if (kind == command_kind::begin_submit) {
				auto request = application::review_submission_request{ .Proposal       = *core::propose(*Draft),
																	   .IdempotencyKey = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
																	   .LiveBinding    = DraftBinding };
				Pending      = request;
				Unknown.reset();
				if (!request.Proposal.Comments.empty() && LockComments) {
					// Every carried draft is held for this submission, durably,
					// before anything can be sent.
					Locking = true;
					present();
					LockComments(request, [this, request](const bool aHeld) {
						Locking = false;
						if (Closing || !Pending || *Pending != request || !Draft || Draft->State != draft_state::submitting) {
							// Superseded before it was sent: what was held is released.
							if (aHeld) {
								answer(request, application::review_submission_publication::definitely_not_published);
							}
							present();
							return;
						}
						if (!aHeld) {
							complete(command_kind::fail_submit);
							Pending.reset();
							answer(request, application::review_submission_publication::definitely_not_published);
							Status = QStringLiteral("Not submitted: a pending comment changed or could not be held for this review. Check the comments and submit again.");
							present();
							return;
						}
						send(request);
						present();
					});
					return;
				}
				if (!request.Proposal.Comments.empty()) {
					complete(command_kind::fail_submit);
					Pending.reset();
					Status = QStringLiteral("Review was not submitted: its pending comments could not be held. Refresh the pull request and try again.");
				} else {
					send(std::move(request));
				}
			} else if (kind == command_kind::discard) {
				if (discardsReview && DiscardReview) {
					// [[BPR-1715]]. Exactly the drafts this review carries,
					// at the revisions it carries them; nothing written on
					// another revision.
					DiscardReview(carried, carriedHead);
				}
				// [[BPR-1345]] v002. A discarded review is gone: the next one
				// starts at once, so nothing it said is shown again. v003: an
				// unfinished submission is the next one, as New review did.
				next_review();
			}
		}
		present();
	}

	void review_submission_workflow::send(application::review_submission_request aRequest)
	{
		const auto admission = aRequest.LiveBinding && Current && *aRequest.LiveBinding == Current->Binding
			? (AdmitPublication ? AdmitPublication(*aRequest.LiveBinding) : std::expected<void, application::review_mutation_failure>{ })
			: std::expected<void, application::review_mutation_failure>{ std::unexpected(application::review_mutation_failure::undesignated_target) };
		if (!admission) {
			complete(command_kind::fail_submit);
			Pending.reset();
			answer(aRequest, application::review_submission_publication::definitely_not_published);
			Status = QStringLiteral("Not published. The current account or pull request changed. Your draft is kept.");
			return;
		}
		Context = Host ? Host->submit(aRequest, IssueGrant(Current->Authority.ApplicationSession, Generation, aRequest)) : std::nullopt;
		if (Context) {
			Status = QStringLiteral("Submitting the exact review shown…");
			return;
		}
		complete(command_kind::fail_submit);
		Pending.reset();
		answer(aRequest, application::review_submission_publication::definitely_not_published);
		Status = QStringLiteral("Review was not submitted. Refresh the pull request and try again.");
	}

	void review_submission_workflow::next_review()
	{
		if (Unresolved.empty()) {
			new_draft();
		} else {
			restore(Unresolved.front());
		}
	}

	void review_submission_workflow::publish(const application::review_submission_host_terminal& aTerminal)
	{
		auto confirmed = std::optional<core::review_submission_draft>{ };
		if (Closing || !Context || aTerminal.Context != *Context) {
			// Retired hosts can finish, but cannot inspect or replace the current draft.
			start_selection();
			return;
		}
		Context.reset();
		// [[BPR-1345]]. A held draft's submission, looked up by its key: none
		// admitted means none sent; an admitted one is reconciled, read only.
		if (const auto* lookup = std::get_if<application::review_submission_lookup>(&aTerminal.Outcome)) {
			LookupKey.reset();
			if (!lookup->Request) {
				// Unread: asked again when the drafts are next presented.
				std::erase(LookedUp, lookup->Key);
			} else if (!*lookup->Request) {
				if (Answer) {
					Answer(lookup->Key, application::review_submission_publication::definitely_not_published, std::nullopt);
				}
			} else if (Host) {
				Recovering = **lookup->Request;
				Context    = Host->reconcile(*Recovering);
				if (!Context) {
					Recovering.reset();
				}
			}
			if (!Context) {
				next_lookup();
			}
			present();
			return;
		}
		if (const auto* result = std::get_if<application::review_submission_result>(&aTerminal.Outcome); result != nullptr && Recovering) {
			const auto recovered = std::exchange(Recovering, std::nullopt);
			if (result->Request == *recovered
				&& (result->Publication != application::review_submission_publication::published || (result->Receipt && application::valid(*result->Receipt, *recovered)))) {
				answer(*recovered, result->Publication);
				if (result->Publication == application::review_submission_publication::published && PublicationObserved) {
					PublicationObserved();
				}
			}
			next_lookup();
			present();
			return;
		}
		if (const auto* discovery = std::get_if<application::review_submission_discovery>(&aTerminal.Outcome)) {
			Ready       = discovery->has_value();
			KeptUnknown = aTerminal.Unknown;
			if (Pending && !Unknown) {
				if (const auto kept = KeptUnknown.find(Pending->IdempotencyKey); kept != KeptUnknown.end()) {
					Unknown = kept->second;
				}
			}
			if (!Ready) {
				Status = QStringLiteral("Review recovery is unavailable. Refresh the pull request to try again.");
			} else {
				Unresolved = **discovery;
				if (Pending) {
					reconcile();
				} else if (!Unresolved.empty()) {
					restore(Unresolved.front());
					reconcile();
				} else if (!Draft || Draft->State == draft_state::submitted || Draft->State == draft_state::discarded
						   || (Draft->State == draft_state::drafting && Draft->Body.empty() && DraftBinding != std::optional{ Current->Binding })) {
					// [[BPR-1345]] v003. A finished review is never shown again.
					// [[BPR-1715]]. Nor is one frozen on an earlier revision
					// that keeps no summary: the review goes on at the current
					// revision, with its comments. [[BPR-1778]]. A chosen
					// verdict is not words to keep: each verdict button chooses
					// itself again. Approve that GitHub refused left Approve
					// chosen, and a new revision then froze every verdict until
					// the application restarted.
					new_draft();
				} else {
					// A finished review never takes the next one's comments.
					apply_review_comments();
				}
			}
		} else if (const auto* result = std::get_if<application::review_submission_result>(&aTerminal.Outcome)) {
			if (!Pending || !Draft || result->Request.Proposal.Draft != Draft->Id
				|| result->Request.Proposal.Attempt != Draft->Attempt || result->Request != *Pending) {
				return;
			}
			if (result->Publication == application::review_submission_publication::published
				&& result->Receipt && application::valid(*result->Receipt, *Pending)) {
				const auto submitted = *Pending;
				Unknown.reset();
				complete(command_kind::complete_submit);
				std::erase_if(Unresolved, [this](const auto& aRequest) { return aRequest.IdempotencyKey == Pending->IdempotencyKey; });
				Pending.reset();
				answer(submitted, application::review_submission_publication::published);
				if (PublicationObserved) {
					PublicationObserved();
				}
				// [[BPR-1345]] v003. Confirmed: the page closes and the window
				// says so (owner, finish-page v002: "once the review is
				// submitted I just want it to close and return to the PR
				// instead"). The next review starts at once, so the page never
				// shows a submitted one or asks for New review.
				confirmed = Draft;
				next_review();
			} else if (result->Publication == application::review_submission_publication::definitely_not_published) {
				const auto refused = *Pending;
				Unknown.reset();
				complete(command_kind::fail_submit);
				// [[BPR-1715]]. The review carries what is pending now, not
				// the revisions the refused attempt held: those come back as
				// its answer releases them, and only then can Submit hold them.
				apply_review_comments();
				std::erase_if(Unresolved, [&refused](const auto& aRequest) { return aRequest.IdempotencyKey == refused.IdempotencyKey; });
				Pending.reset();
				Status                 = refused.Proposal.Comments.empty()
					? QStringLiteral("Review was not published. Your summary is retained.")
					: QStringLiteral("Review was not published. Your summary and its pending comments are kept; check them and submit again.");
				const auto failureText = refused.LiveBinding ? qt::github_cli_publication_failure_text(result->Failure,
																									   QString::fromStdString(refused.LiveBinding->ActingIdentity.Login), QString::fromStdString(std::string{ refused.LiveBinding->Target.Hostname.value() }))
															 : QString{ };
				if (!failureText.isEmpty()) {
					Status = failureText;
				}
				// [[BPR-1715]]. Why GitHub refused it, as far as its answer
				// says. It names no comment, so neither does the page.
				if (result->Refusal == application::review_submission_refusal::unplaceable_line) {
					Status = QStringLiteral("Not published: GitHub refused this review because one of its comments is on a line GitHub can't place in the pull request's diff, such as a line outside the changes. GitHub doesn't say which. Your summary and comments are kept: delete or edit that comment, then submit again.");
				} else if (result->Refusal == application::review_submission_refusal::review) {
					Status = QStringLiteral("Not published: GitHub refused this review. Your summary and comments are kept; check them and submit again.");
				}
				answer(refused, application::review_submission_publication::definitely_not_published);
				if (result->Refusal == application::review_submission_refusal::unplaceable_line && UnplaceableRefusalObserved) {
					UnplaceableRefusalObserved(refused);
				}
			} else {
				// [[BPR-1475]]. Not reread: Check status is how an unknown outcome
				// is settled, and a read accepted while it runs can replace the
				// authority it runs under. Once it finds the review published,
				// that answer rereads.
				Status = QStringLiteral("Review status is unknown. Check status to reconcile this attempt; do not submit it again.");
				// [[BPR-1541]]. Each answer replaces the reason.
				Unknown = result->Unknown;
			}
		}
		if (!Context) {
			next_lookup();
		}
		present();
		if (confirmed) {
			Editor.confirm_submitted(*confirmed);
		}
	}
}

namespace bpr::app {
	review_tour_publication_workflow::review_tour_publication_workflow(application::effect_executor& aEffects, application::decision_executor& aDecisions, qt::review_tour_editor& aEditor, grant_factory aIssueGrant, review_submission_workflow::publication_admission aAdmitPublication, receipt_sink aReceiptSink)
		: Effects{ aEffects }
		, Decisions{ aDecisions }
		, Editor{ aEditor }
		, IssueGrant{ std::move(aIssueGrant) }
		, AdmitPublication{ std::move(aAdmitPublication) }
		, ReceiptSink{ std::move(aReceiptSink) }
	{ }
	void review_tour_publication_workflow::invalidate()
	{
		if (Host) {
			Host->shutdown();
			Retired.push_back(std::move(Host));
		}
		std::erase_if(Retired, [](const auto& aHost) { return aHost->is_quiescent(); });
		Context.reset();
		Binding.reset();
		Plan.reset();
		Record.reset();
		Ready     = false;
		Previewed = false;
		Status    = "Publication authority changed. Refresh and prepare a new preview.";
		present();
	}
	void review_tour_publication_workflow::adopt(application::review_submission_live_binding aBinding, application::workbench_pull_request_authority aAuthority, application::review_submission_provider_factory aFactory, std::string aDescription, bool aQualified, application::insights_review_capture aInsights)
	{
		if (Closing) {
			return;
		}
		aAuthority.Snapshot              = { };
		aAuthority.PullRequestGeneration = { };
		if (Binding == aBinding && Authority == aAuthority && Host) {
			Description = std::move(aDescription);
			return;
		}
		invalidate();
		Binding     = std::move(aBinding);
		Authority   = aAuthority;
		Description = std::move(aDescription);
		Qualified   = aQualified;
		Generation  = core::request_generation{ Generation.value() + 1 };
		Host        = std::make_unique<application::review_submission_application_host>(std::move(aFactory), *Binding, Authority.ApplicationSession, Generation, Effects, Decisions, *this, std::move(aInsights));
		Context     = Host->discover_tours();
		Status      = "Checking saved publication state. No writes are scheduled.";
		present();
	}
	void review_tour_publication_workflow::observe(const application::tour_presentation& aDraft)
	{
		// [[BPR-1501]]. Only a change of the observed draft can stale the plan.
		// The first draft a window loads, and a repeated projection of the same
		// revision, change nothing, whether they arrive before or after a
		// restart's discovery adopts the captured plan.
		const bool changed             = Draft.Draft && aDraft.Draft && (Draft.Draft->Owner != aDraft.Draft->Owner || Draft.Draft->Revision != aDraft.Draft->Revision);
		Draft                          = aDraft;
		const auto publishedEntity     = Record && Record->Published ? (Record->Plan.Update ? Record->Plan.Update->Remote->Entity : (Record->Index ? Record->Index->NodeId : Record->Review->NodeId)) : std::string{ };
		const bool observedPublication = aDraft.Draft && aDraft.Draft->Remote && !publishedEntity.empty() && aDraft.Draft->Remote->Entity == publishedEntity;
		if (Plan && changed && !observedPublication && (Plan->Owner != aDraft.Draft->Owner || Plan->Generation != aDraft.Draft->Revision)) {
			if (Context && Host) {
				Host->cancel(*Context);
			}
			Previewed = false;
			if (!Record && !Context) {
				Plan.reset();
			}
			Status = "Draft changed. Prepare a fresh preview before publishing or resuming the captured plan.";
		}
		present();
	}
	void review_tour_publication_workflow::prepare()
	{
		if (!Ready || !Binding || !Host || Context || !Draft.Draft) {
			return;
		}
		if (Draft.Refusal != core::tour_refusal::none || Draft.Conflict) {
			Previewed = false;
			Status    = "Resolve the refused edit or saved-copy conflict before previewing publication.";
			present();
			return;
		}
		if (Record && Record->Published && State == application::staged_publication_state::published) {
			Record.reset();
			Plan.reset();
		}
		if (Record) {
			Plan      = Record->Plan;
			Previewed = true;
			Status    = "Previewing the captured publication. Resume only continues this exact text and destinations.";
			present();
			return;
		}
		const auto prepared = prepare_tour_plan(*Draft.Draft, *Binding, Description, { Editor.compatibility_publication(), Editor.description_publication() }, Qualified);
		Plan                = prepared.Plan;
		Previewed           = Plan.has_value();
		if (Plan) {
			State = application::staged_publication_state::ready;
		}
		Status = prepared.Status;
		present();
	}

	void review_tour_publication_workflow::dispatch(qt::tour_editor_action aAction)
	{
		using action = qt::tour_editor_action;
		if (Closing || !Host) {
			return;
		}
		if (aAction == action::stop_publication) {
			if (Context) {
				Host->cancel(*Context);
			}
			Status    = "Stopping. Confirmed receipts are retained; check status before Resume.";
			Previewed = false;
			present();
			return;
		}
		if (aAction == action::preview_publication) {
			prepare();
			return;
		}
		if (Context || !Plan) {
			return;
		}
		if (aAction == action::reconcile_publication) {
			Context = Host->reconcile_tour(*Plan);
			Status  = "Checking exact saved identities. No writes are scheduled.";
			present();
			return;
		}
		if (aAction != action::publish_publication && aAction != action::resume_publication) {
			return;
		}
		if (!Previewed || *Plan->Submission.LiveBinding != *Binding) {
			return;
		}
		if (!Record && (!Draft.Draft || Plan->Generation != Draft.Draft->Revision || Plan->Compatibility != Editor.compatibility_publication() || (!Plan->Update && Plan->Description.has_value() != Editor.description_publication()))) {
			return;
		}
		if (Record && (State == application::staged_publication_state::outcome_unknown || State == application::staged_publication_state::conflict || State == application::staged_publication_state::published)) {
			return;
		}
		if (AdmitPublication) {
			const auto admission = AdmitPublication(*Binding);
			if (!admission) {
				Status = qt::github_cli_publication_failure_text(admission.error(), QString::fromStdString(Binding->ActingIdentity.Login), QString::fromStdString(std::string{ Binding->Target.Hostname.value() }), Record.has_value()).toStdString();
				if (Status.empty()) {
					Status = "Publication refused: the current account or pull request changed. Your draft is kept.";
				}
				present();
				return;
			}
		}
		const auto grant = IssueGrant ? IssueGrant(Authority.ApplicationSession, Generation, *Plan) : application::review_submission_execution_grant{ };
		Context          = Host->publish_tour(*Plan, grant);
		Previewed        = false;
		Status           = Context ? "Publishing the captured tour. Stop preserves completed operations." : "Publication authority is unavailable. Prepare a new preview.";
		present();
	}
	void review_tour_publication_workflow::publish(const application::review_submission_host_terminal& aTerminal)
	{
		std::erase_if(Retired, [](const auto& aHost) { return aHost->is_quiescent(); });
		if (Closing || !Context || aTerminal.Context != *Context) {
			return;
		}
		Context.reset();
		if (const auto* discovered = std::get_if<application::staged_review_discovery>(&aTerminal.Outcome)) {
			Ready = discovered->has_value();
			if (!Ready) {
				Status = "Saved publication state is unavailable. Local preparation remains available.";
				present();
				return;
			}
			if (discovered->value().size() > 1) {
				Ready  = false;
				Status = "Multiple saved publications require reconciliation before starting another tour.";
				present();
				return;
			}
			if (!discovered->value().empty()) {
				Plan    = discovered->value().front();
				Context = Host->reconcile_tour(*Plan);
				Status  = "Reconciling the captured publication. Resume requires a fresh preview.";
			} else {
				Status = "Saved locally. Prepare a publication preview.";
			}
		} else if (const auto* result = std::get_if<application::staged_publication_result>(&aTerminal.Outcome)) {
			const auto preflightRefusal = !result->Record && result->Failure
				&& (*result->Failure == application::review_mutation_failure::credential_unavailable
					|| *result->Failure == application::review_mutation_failure::credential_tool_failed
					|| *result->Failure == application::review_mutation_failure::cancelled);
			const auto retainedOutcome = !result->Record && result->Failure && Record
				&& (preflightRefusal || State == application::staged_publication_state::outcome_unknown || Record->Published);
			if (result->Record || !result->Failure) {
				Record = result->Record;
			}
			if (Record && Record->Published && ReceiptSink) {
				ReceiptSink(*Record);
			}
			if (!retainedOutcome) {
				State = result->State;
			}
			Previewed   = false;
			using state = application::staged_publication_state;
			if (Record && Record->Published && State != state::published) {
				Status = "Tour published; description update needs attention. The tour will not be posted again.";
			} else {
				switch (State) {
				case state::published:             Status = Plan && Plan->Compatibility ? "Published as compatibility comments. All returned links are wired." : "Published: one COMMENT review with verified tour links."; break;
				case state::ready:                 Status = "Prepared locally. Preview the captured plan before Resume."; break;
				case state::staged:                Status = "Staged on GitHub (visible to you). Preview the captured plan before Resume."; break;
				case state::partial:               Status = "Partially published compatibility comments. Completed identities are retained. Preview before Resume."; break;
				case state::outcome_unknown:       Status = "Outcome unknown. Check publication status; no write will be repeated blindly."; break;
				case state::conflict:              Status = "Publication conflict. The pending review, body, actor or head changed. No further writes are authorized."; break;
				case state::description_attention: Status = "Tour published; description update needs attention."; break;
				case state::unavailable:           Status = "Publication is unavailable. The private draft is retained."; break;
				}
			}
			if (result->Failure && Binding && State != state::outcome_unknown && !(Record && Record->Published)) {
				const auto status = qt::github_cli_publication_failure_text(*result->Failure, QString::fromStdString(Binding->ActingIdentity.Login), QString::fromStdString(std::string{ Binding->Target.Hostname.value() }), Record.has_value());
				if (!status.isEmpty()) {
					Status = status.toStdString();
				}
			}
		}
		present();
	}
	void review_tour_publication_workflow::present()
	{
		application::tour_publication_presentation view{ .Status = Status };
		if (Plan) {
			view.Markdown = tour_plan_markdown(*Plan);
			if (Record) {
				const auto bodies = application::staged_final_bodies(*Record);
				if (bodies) {
					view.Markdown += "\n\n### Observed index\n\n" + bodies->first;
				}
			}
		}
		view.Publish   = Ready && Previewed && Plan && !Record && !Context;
		view.Resume    = Ready && Previewed && Record && !Context && (State == application::staged_publication_state::ready || State == application::staged_publication_state::staged || State == application::staged_publication_state::partial || State == application::staged_publication_state::description_attention);
		view.Reconcile = Plan.has_value() && !Context;
		view.Stop      = Context.has_value();
		Editor.present_publication(view);
	}
	void review_tour_publication_workflow::shutdown()
	{
		Closing = true;
		if (Host) {
			Host->shutdown();
		}
		for (auto& h : Retired) {
			h->shutdown();
		}
	}
	auto review_tour_publication_workflow::is_quiescent() const -> bool
	{
		return (!Host || Host->is_quiescent()) && std::ranges::all_of(Retired, [](const auto& aHost) { return aHost->is_quiescent(); });
	}
	conversation_reply_workflow::conversation_reply_workflow(application::effect_executor& aEffects, application::decision_executor& aDecisions, qt::workbench_window& aWindow, grant_factory aGrant, review_submission_workflow::publication_admission aAdmit, std::function<void()> aPublished)
		: Effects{ aEffects }
		, Decisions{ aDecisions }
		, Window{ aWindow }
		, IssueGrant{ std::move(aGrant) }
		, Admit{ std::move(aAdmit) }
		, Published{ std::move(aPublished) }
	{ }
	void conversation_reply_workflow::invalidate()
	{
		if (Host) {
			Host->shutdown();
			Retired.push_back(std::move(Host));
		}
		std::erase_if(Retired, [](const auto& aHost) { return aHost->is_quiescent(); });
		Binding.reset();
		Context.reset();
		Plans.clear();
		Records.clear();
		Desired.reset();
		PendingEdits.clear();
		ComposerOpen = false;
		Saving.reset();
		SendText.reset();
		Ready   = false;
		Sending = false;
		Failed  = false;
		Window.present_conversation_reply(std::nullopt);
	}
	void conversation_reply_workflow::shutdown()
	{
		Closing = true;
		SendText.reset();
		ComposerOpen = false;
		if (Sending && Host) {
			Host->shutdown();
		}
		pump();
		if (!Context) {
			invalidate();
		}
	}
	auto conversation_reply_workflow::is_quiescent() const -> bool
	{
		return (!Host || Host->is_quiescent()) && std::ranges::all_of(Retired, [](const auto& aHost) { return aHost->is_quiescent(); });
	}
	void conversation_reply_workflow::adopt(application::review_submission_live_binding aBinding, application::workbench_pull_request_authority aAuthority, core::bpr_profile_id aProfile, application::review_submission_provider_factory aFactory, application::insights_review_capture aInsights)
	{
		aAuthority.Snapshot              = { };
		aAuthority.PullRequestGeneration = { };
		if (Closing || (Binding == aBinding && Authority == aAuthority && Host)) {
			return;
		}
		invalidate();
		Binding    = std::move(aBinding);
		Authority  = aAuthority;
		Profile    = aProfile;
		Generation = core::request_generation{ Generation.value() + 1 };
		Host       = std::make_unique<application::review_submission_application_host>(std::move(aFactory), *Binding, Authority.ApplicationSession, Generation, Effects, Decisions, *this, std::move(aInsights));
		Context    = Host->discover_conversation_replies();
		Status     = QStringLiteral("Loading saved replies.");
	}
	void conversation_reply_workflow::dispatch(const core::conversation_reply_action aAction, core::conversation_reply_draft aDraft)
	{
		if (Closing || !Host || !Binding || !core::valid(aDraft)) {
			return;
		}
		if (aAction == core::conversation_reply_action::close) {
			ComposerOpen = false;
			SendText.reset();
			pump();
			Window.present_conversation_reply(std::nullopt);
			return;
		}
		if (Sending) {
			return;
		}
		if (!Ready && aAction != core::conversation_reply_action::open) {
			return;
		}
		if (aAction == core::conversation_reply_action::open) {
			ComposerOpen = true;
			SendText.reset();
			if (const auto saved = Plans.find(aDraft.Conversation); saved != Plans.end()) {
				aDraft = saved->second.ReplyDraft;
			}
			if (const auto pending = PendingEdits.find(aDraft.Conversation); pending != PendingEdits.end()) {
				aDraft = pending->second;
			}
			Desired = std::move(aDraft);
			if (Ready && !Context && Plans.contains(Desired->Conversation) && !Records.contains(Desired->Conversation)) {
				Context = Host->reconcile_tour(Plans.at(Desired->Conversation));
			}
			Failed = false;
			Status = QStringLiteral("Saved locally. Send posts a quoted comment to the pull request.");
		} else if (Desired && Desired->Conversation == aDraft.Conversation) {
			if (aAction == core::conversation_reply_action::edit) {
				Desired->Text = std::move(aDraft.Text);
				SendText.reset();
				Failed = false;
			}
			if (aAction == core::conversation_reply_action::send && core::conversation_reply_can_send(*Desired)) {
				SendText = Desired->Text;
				Failed   = false;
			}
		}
		if (Desired && Ready) {
			PendingEdits[Desired->Conversation] = *Desired;
		}
		pump();
		present();
	}
	void conversation_reply_workflow::pump()
	{
		if (!Ready || !Host || Context || Failed) {
			return;
		}
		for (const auto& [identity, draft] : PendingEdits) {
			const auto at     = Plans.find(identity);
			const auto frozen = Records.find(identity);
			if (frozen != Records.end() && !frozen->second.Drafting) {
				continue;
			}
			if (at != Plans.end() && at->second.ReplyDraft == draft) {
				continue;
			}
			application::staged_review_plan plan;
			if (at != Plans.end()) {
				plan = at->second;
				++plan.Generation;
			} else {
				plan.ConversationReply                 = true;
				plan.Compatibility                     = true;
				plan.Generation                        = 1;
				plan.Owner                             = { Profile, Authority.Host, Authority.HostAuthority, Authority.Account, Authority.Repository, Authority.PullRequest };
				plan.Submission.Proposal.Draft         = core::review_submission_draft_id{ 1 };
				plan.Submission.Proposal.Target        = Binding->Target.Semantic;
				plan.Submission.Proposal.ActingAccount = Binding->ActingIdentity.Account;
				plan.Submission.Proposal.Disposition   = core::review_disposition::comment;
				plan.Submission.LiveBinding            = *Binding;
				plan.Submission.IdempotencyKey         = "conversation-" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
			}
			plan.ReplyDraft                  = draft;
			plan.Submission.Proposal.Body    = *core::render_conversation_reply(draft);
			plan.Submission.Proposal.Attempt = core::review_submission_attempt_id{ plan.Generation };
			Saving                           = plan;
			Context                          = Host->save_conversation_reply(plan);
			Status                           = QStringLiteral("Saving reply locally…");
			if (!Context) {
				Failed = true;
				Status = QStringLiteral("Reply could not be saved. Your text is kept; press Send to retry.");
			}
			return;
		}
		if (!Desired) {
			return;
		}
		const auto at = Plans.find(Desired->Conversation);
		if (!SendText || *SendText != Desired->Text || at == Plans.end()) {
			return;
		}
		const auto& plan = at->second;
		if (plan.ReplyDraft != *Desired || plan.Submission.LiveBinding != Binding) {
			Status = QStringLiteral("The pull request changed. Your reply is kept; refresh before sending.");
			Failed = true;
			return;
		}
		if (Admit) {
			const auto admitted = Admit(*Binding);
			if (!admitted) {
				Status = QStringLiteral("Reply not sent: %1. Your draft is kept.").arg(QString::fromUtf8(application::name(admitted.error())));
				Failed = true;
				return;
			}
		}
		auto grant = IssueGrant ? IssueGrant(Authority.ApplicationSession, Generation, plan) : application::review_submission_execution_grant{ };
		Context    = Host->publish_tour(plan, std::move(grant));
		Sending    = Context.has_value();
		SendText.reset();
		Status = Sending ? QStringLiteral("Sending reply…") : QStringLiteral("Reply not sent. Check the connection and press Send to retry.");
	}
	void conversation_reply_workflow::present()
	{
		if (!Desired || !ComposerOpen) {
			return;
		}
		Window.present_conversation_reply(qt::thread_reply_presentation{ .Thread = Desired->Conversation, .Body = QString::fromStdString(Desired->Text), .Publishable = Ready && !Sending && core::conversation_reply_can_send(*Desired), .Editable = Ready && !Sending && (!Plans.contains(Desired->Conversation) || (Records.contains(Desired->Conversation) && Records.at(Desired->Conversation).Drafting)), .Notice = Status });
	}
	void conversation_reply_workflow::publish(const application::review_submission_host_terminal& aTerminal)
	{
		if (!Context || *Context != aTerminal.Context) {
			return;
		}
		Context.reset();
		if (const auto* discovery = std::get_if<application::staged_review_discovery>(&aTerminal.Outcome)) {
			Ready = discovery->has_value();
			if (*discovery) {
				for (const auto& plan : **discovery) {
					if (Plans.contains(plan.ReplyDraft.Conversation)) {
						Ready  = false;
						Status = QStringLiteral("More than one saved reply answers this comment. Open private recovery before sending.");
						break;
					}
					Plans.emplace(plan.ReplyDraft.Conversation, plan);
				}
			}
			if (!Ready) {
				Status = QStringLiteral("Saved replies are unavailable. Nothing will be sent.");
			} else if (Desired) {
				if (const auto saved = Plans.find(Desired->Conversation); saved != Plans.end()) {
					Desired = saved->second.ReplyDraft;
					Context = Host->reconcile_tour(saved->second);
					Status  = QStringLiteral("Checking the saved reply. Nothing will be sent.");
				} else {
					PendingEdits[Desired->Conversation] = *Desired;
				}
			}
		} else if (const auto* result = std::get_if<application::staged_publication_result>(&aTerminal.Outcome)) {
			if (result->Record) {
				const auto& row                           = *result->Record;
				Plans[row.Plan.ReplyDraft.Conversation]   = row.Plan;
				Records[row.Plan.ReplyDraft.Conversation] = row;
				if (const auto pending = PendingEdits.find(row.Plan.ReplyDraft.Conversation); pending != PendingEdits.end() && pending->second == row.Plan.ReplyDraft) {
					PendingEdits.erase(pending);
				}
			}
			if (Saving) {
				if (!result->Record || result->Record->Plan != *Saving || !result->Record->Drafting) {
					Failed = true;
					Status = QStringLiteral("Reply could not be saved. Your text is kept; press Send to retry.");
				} else {
					Status = QStringLiteral("Saved locally. Send posts a quoted comment to the pull request.");
				}
				Saving.reset();
			} else if (Sending) {
				Sending = false;
				if (result->State == application::staged_publication_state::published && result->Record) {
					const auto id = result->Record->Plan.ReplyDraft.Conversation;
					Plans.erase(id);
					Records.erase(id);
					Desired.reset();
					Window.present_conversation_reply(std::nullopt);
					Window.conversation_panel()->cards()->close_thread();
					if (Published) {
						Published();
					}
					return;
				}
				Failed = true;
				Status = result->State == application::staged_publication_state::outcome_unknown ? QStringLiteral("Send outcome is unknown. Your reply is kept; Send checks the existing attempt before retrying.") : QStringLiteral("Reply was not sent: %1. Your draft is kept; press Send to retry.").arg(result->Failure ? QString::fromUtf8(application::name(*result->Failure)) : QStringLiteral("publication refused"));
			}
		}
		pump();
		if (Closing && !Context) {
			invalidate();
			return;
		}
		present();
	}

}
