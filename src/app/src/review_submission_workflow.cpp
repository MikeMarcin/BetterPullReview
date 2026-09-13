#include "review_submission_workflow.hpp"

#include "bpr/application/review_submission_recovery_port.hpp"

#include <QUuid>
#include <algorithm>
#include <limits>

namespace bpr::app {
	using command_kind = core::review_submission_command_kind;
	using draft_state  = core::review_submission_state;

	review_submission_workflow::review_submission_workflow(application::effect_executor&   aEffects,
														   application::decision_executor& aDecisions, qt::review_submission_editor& aEditor, grant_factory aIssueGrant, write_confirmation aConfirmWrite)
		: Effects{ aEffects }
		, Decisions{ aDecisions }
		, Editor{ aEditor }
		, IssueGrant{ std::move(aIssueGrant) }
		, ConfirmWrite{ std::move(aConfirmWrite) }
	{ }

	void review_submission_workflow::adopt(application::review_submission_live_binding   aBinding,
										   application::workbench_pull_request_authority aAuthority, application::review_submission_provider_factory aFactory)
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
		Next = selection{ std::move(aBinding), std::move(aAuthority), std::move(aFactory) };
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
		Current = std::move(Next);
		Next.reset();
		if (Generation.value() == std::numeric_limits<std::uint64_t>::max()) {
			Status = QStringLiteral("Review session exhausted. Reopen the application.");
			present();
			return;
		}
		Generation = core::request_generation{ Generation.value() + 1U };
		Host       = std::make_unique<application::review_submission_application_host>(Current->Factory, Current->Binding,
																					   Current->Authority.ApplicationSession, Generation, Effects, Decisions, *this);
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

	void review_submission_workflow::invalidate()
	{
		Next.reset();
		retire();
		Status = QStringLiteral("Review authority changed. Open or refresh the pull request to continue.");
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
		Draft        = core::review_submission_draft{ .Id            = core::review_submission_draft_id{ NextDraft++ },
													  .Target        = DraftBinding->Target.Semantic,
													  .ActingAccount = DraftBinding->ActingIdentity.Account };
		Pending.reset();
		Status = QStringLiteral("Choose a review type and enter a summary. Approve may have an empty summary.");
	}

	void review_submission_workflow::restore(application::review_submission_request aRequest)
	{
		const auto& proposal = aRequest.Proposal;
		Draft                = core::review_submission_draft{ .Id = proposal.Draft, .Target = proposal.Target, .ActingAccount = proposal.ActingAccount, .Body = proposal.Body, .Disposition = proposal.Disposition, .State = draft_state::submitting, .Attempt = proposal.Attempt };
		DraftBinding         = aRequest.LiveBinding;
		NextDraft            = std::max(NextDraft, proposal.Draft.value() == std::numeric_limits<std::uint64_t>::max() ? proposal.Draft.value() : proposal.Draft.value() + 1U);
		Pending              = std::move(aRequest);
		Status               = QStringLiteral("An unfinished review was restored. Check status before starting another review.");
	}

	auto review_submission_workflow::decided_controls() const -> core::review_submission_controls
	{
		if (!Draft || !Ready || !Host || Context || Closing) {
			return { };
		}
		auto result = core::controls(*Draft);
		if (!Current || DraftBinding != std::optional{ Current->Binding }) {
			result.Edit   = false;
			result.Submit = false;
		}
		return result;
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
			status = QStringLiteral("The pull request revision changed. Discard this frozen draft and start a new review.");
		}
		Editor.present({ .Draft = Draft, .Controls = decided_controls(), .TargetDescription = std::move(target), .Status = std::move(status) });
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

	void review_submission_workflow::dispatch(qt::review_submission_editor_intent aIntent)
	{
		// Refuse stale draft identity before touching the supplied summary.
		if (!Draft || aIntent.Command.Id != Draft->Id) {
			return;
		}
		const auto controls = decided_controls();
		using operation     = qt::review_submission_editor_operation;
		if (aIntent.Operation == operation::new_review) {
			if (controls.NewReview) {
				if (Unresolved.empty()) {
					new_draft();
				} else {
					restore(Unresolved.front());
				}
			}
		} else if (aIntent.Operation == operation::reconcile) {
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
				return;
			}
			if (kind == command_kind::begin_submit && ConfirmWrite) {
				if (!DraftBinding) {
					return;
				}
				const auto confirmedDraft   = *Draft;
				const auto confirmedBinding = *DraftBinding;
				if (!ConfirmWrite(confirmedBinding, confirmedDraft.Disposition)
					|| !Draft || *Draft != confirmedDraft || !Current
					|| DraftBinding != confirmedBinding || !decided_controls().Submit) {
					return;
				}
			}
			auto outcome = core::apply(*Draft, aIntent.Command);
			if (!outcome.Draft) {
				Status = QStringLiteral("The summary must be valid text, at most 64 KiB, and nonblank unless approving.");
				present();
				return;
			}
			Draft = std::move(outcome.Draft);
			if (kind == command_kind::begin_submit) {
				Pending = application::review_submission_request{ .Proposal       = *core::propose(*Draft),
																  .IdempotencyKey = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
																  .LiveBinding    = DraftBinding };
				Context = Host->submit(*Pending, IssueGrant(Current->Authority.ApplicationSession, Generation, *Pending));
				if (Context) {
					Status = QStringLiteral("Submitting the exact review shown…");
				} else {
					complete(command_kind::fail_submit);
					Pending.reset();
					Status = QStringLiteral("Review was not submitted. Refresh the pull request and try again.");
				}
			} else if (kind == command_kind::discard) {
				Status = QStringLiteral("Review draft discarded.");
			}
		}
		present();
	}

	void review_submission_workflow::publish(const application::review_submission_host_terminal& aTerminal)
	{
		if (Closing || !Context || aTerminal.Context != *Context) {
			// Retired hosts can finish, but cannot inspect or replace the current draft.
			start_selection();
			return;
		}
		Context.reset();
		if (const auto* discovery = std::get_if<application::review_submission_discovery>(&aTerminal.Outcome)) {
			Ready = discovery->has_value();
			if (!Ready) {
				Status = QStringLiteral("Review recovery is unavailable. Refresh the pull request to try again.");
			} else {
				Unresolved = **discovery;
				if (Pending) {
					reconcile();
				} else if (!Unresolved.empty()) {
					restore(Unresolved.front());
					reconcile();
				} else if (!Draft) {
					new_draft();
				}
			}
		} else if (const auto* result = std::get_if<application::review_submission_result>(&aTerminal.Outcome)) {
			if (!Pending || !Draft || result->Request.Proposal.Draft != Draft->Id
				|| result->Request.Proposal.Attempt != Draft->Attempt || result->Request != *Pending) {
				return;
			}
			if (result->Publication == application::review_submission_publication::published
				&& result->Receipt && application::valid(*result->Receipt, *Pending)) {
				complete(command_kind::complete_submit);
				std::erase_if(Unresolved, [this](const auto& aRequest) { return aRequest.IdempotencyKey == Pending->IdempotencyKey; });
				Pending.reset();
				Status = QStringLiteral("Review submitted and confirmed on GitHub.");
				if (!Unresolved.empty()) {
					Status += QStringLiteral(" Choose New review to inspect the next unfinished review.");
				}
			} else if (result->Publication == application::review_submission_publication::definitely_not_published) {
				complete(command_kind::fail_submit);
				Pending.reset();
				Status = QStringLiteral("Review was not published. Your summary is retained.");
			} else {
				Status = QStringLiteral("Review status is unknown. Check status to reconcile this attempt; do not submit it again.");
			}
		}
		present();
	}
}
