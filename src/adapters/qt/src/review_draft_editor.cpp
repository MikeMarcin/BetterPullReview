#include "bpr/qt/review_draft_editor.hpp"

#include "bpr/qt/feedback_label_controls.hpp"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>
#include <cstddef>
#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief Creates one named action owned by the editor.
		///
		/// \details A `QAction` is not a widget and carries no accessible name
		/// of its own; assistive technology reads its text. The text is
		/// therefore the identity a person hears, and each one here is
		/// deliberately distinct — "Accept file" and "Exclude file" would be
		/// ambiguous as "Accept" and "Exclude" beside a file list. The object
		/// name is set as well so a test can find the action without matching
		/// on user-visible text.
		[[nodiscard]] auto make_action(
			QWidget*       aParent,
			const QString& aText,
			const QString& aObjectName) -> QAction*
		{
			auto* const action = new QAction{ aText, aParent };
			action->setObjectName(aObjectName);
			aParent->addAction(action);
			return action;
		}

		/// \brief Creates one read-only workshop surface owned by the editor.
		///
		/// \details Plain text, and deliberately so: no Markdown rendering
		/// surface exists in this tree, and adding one for this view would be
		/// a renderer nobody else uses. The decision is recorded in
		/// `docs/verification/RFC-BPR-0012.md`.
		[[nodiscard]] auto make_workshop_surface(
			QWidget*       aParent,
			const QString& aObjectName,
			const QString& aAccessibleName) -> QPlainTextEdit*
		{
			auto* const surface = new QPlainTextEdit{ aParent };
			surface->setObjectName(aObjectName);
			surface->setAccessibleName(aAccessibleName);
			surface->setReadOnly(true);
			return surface;
		}
	} // namespace

	review_draft_editor::review_draft_editor(
		draft_dispatch   aDraftDispatch,
		receipt_dispatch aReceiptDispatch,
		QWidget* const   aParent)
		: QWidget{ aParent }
	{
		Members.DraftDispatch   = std::move(aDraftDispatch);
		Members.ReceiptDispatch = std::move(aReceiptDispatch);

		auto* const layout = new QVBoxLayout{ this };

		Members.TargetLabel = new QLabel{ this };
		Members.TargetLabel->setObjectName(QStringLiteral("reviewPublishTarget"));
		Members.TargetLabel->setAccessibleName(
			QStringLiteral("Publication target"));
		Members.TargetLabel->setWordWrap(true);
		layout->addWidget(Members.TargetLabel);
		Members.ReplyTarget = new QLabel{ this };
		Members.ReplyTarget->setObjectName(QStringLiteral("reviewReplyTarget"));
		Members.ReplyTarget->setAccessibleName(QStringLiteral("Replying to"));
		Members.ReplyTarget->setWordWrap(true);
		Members.ReplyTarget->hide();
		layout->addWidget(Members.ReplyTarget);
		Members.MutationStatus = new QLabel{ this };
		Members.MutationStatus->setObjectName(
			QStringLiteral("reviewMutationStatus"));
		Members.MutationStatus->setAccessibleName(
			QStringLiteral("Review publication status"));
		Members.MutationStatus->setWordWrap(true);
		layout->addWidget(Members.MutationStatus);

		Members.Revisioned.Reload  = make_action(this, tr("Reload accepted draft"), QStringLiteral("reviewDraftReload"));
		Members.Revisioned.Reapply = make_action(this, tr("Reapply my text"), QStringLiteral("reviewDraftReapply"));
		auto* conflictActions      = new QHBoxLayout;
		for (auto* action : { Members.Revisioned.Reload, Members.Revisioned.Reapply }) {
			auto* button = new QToolButton{ this };
			button->setDefaultAction(action);
			button->setObjectName(action->objectName() + QStringLiteral("Button"));
			connect(action, &QAction::changed, button, [button, action] { button->setVisible(action->isVisible()); });
			conflictActions->addWidget(button);
		}
		layout->addLayout(conflictActions);
		connect(Members.Revisioned.Reload, &QAction::triggered, this, [this] {
			auto& state = Members.Revisioned;
			if (state.Replacement) {
				state.Accepted = std::exchange(state.Replacement, std::nullopt);
				state.Selected = true;
			}
			if (!state.Selected || !state.Accepted) {
				return;
			}
			state.Candidate.reset();
			state.Conflict = false;
			settle_in_flight();
			Members.MutationStatus->clear();
			render_draft(state.Accepted->Draft);
		});
		connect(Members.Revisioned.Reapply, &QAction::triggered, this, [this] {
			auto& state = Members.Revisioned;
			if (!state.Selected || !state.Accepted || !state.Candidate || !state.Dispatch || state.Candidate->Scope != state.Accepted->Scope || state.Candidate->Command.Id != state.Accepted->Draft.Id) {
				return;
			}
			auto command             = *state.Candidate;
			command.ExpectedRevision = state.Accepted->Revision;
			state.Candidate          = command;
			state.Conflict           = false;
			settle_in_flight();
			state.InFlight = true;
			refresh_enablement();
			state.Dispatch(command);
		});
		// A change that waited on an answer that never comes goes as it
		// always did, on the revision this editor last accepted.
		Members.Revisioned.Flush = new QTimer{ this };
		Members.Revisioned.Flush->setSingleShot(true);
		Members.Revisioned.Flush->setInterval(500);
		connect(Members.Revisioned.Flush, &QTimer::timeout, this, [this] {
			if (std::exchange(Members.Revisioned.Deferred, false)) {
				Members.Revisioned.InFlight = false;
				emit_draft(core::review_draft_command_kind::edit);
			}
		});
		Members.Body = new QPlainTextEdit{ this };
		Members.Body->setObjectName(QStringLiteral("reviewDraftBody"));
		Members.Body->setAccessibleName(QStringLiteral("Review comment body"));
		Members.Body->setAccessibleDescription(QStringLiteral(
			"Edit the selected semantic source-line review comment."));
		// [[BPR-1083]]. The label tool row sits on the field and is the next
		// tab stop after it. Its edits change the body's text, which this
		// widget's one typed-edit path below turns into one draft edit.
		Members.Labels = new feedback_label_controls{ Members.Body, this };
		Members.Labels->set_status_sink([this](const QString& aText) { Members.MutationStatus->setText(aText); });
		layout->addWidget(Members.Labels);
		layout->addWidget(Members.Body);
		connect(Members.Body, &QPlainTextEdit::textChanged, this, [this] {
			if (!Members.ApplyingPresentation) {
				emit_draft(core::review_draft_command_kind::edit);
			}
		});
		QWidget::setTabOrder(Members.Body, Members.Labels->chip());

		// Two surfaces, side by side and never the same one. A workshop that
		// showed the proposal in the original's place would have destroyed
		// the thing the reviewer is being asked to compare it against.
		Members.WorkshopOriginal = make_workshop_surface(
			this,
			QStringLiteral("reviewWorkshopOriginal"),
			QStringLiteral("Your original comment"));
		Members.WorkshopProposal = make_workshop_surface(
			this,
			QStringLiteral("reviewWorkshopProposal"),
			QStringLiteral("Proposed revision"));
		Members.WorkshopOperation = new QLabel{ this };
		Members.WorkshopOperation->setObjectName(
			QStringLiteral("reviewWorkshopOperation"));
		Members.WorkshopOperation->setAccessibleName(
			QStringLiteral("Comment workshop operation"));
		Members.WorkshopOperation->setWordWrap(true);
		layout->addWidget(Members.WorkshopOperation);
		layout->addWidget(Members.WorkshopOriginal);
		layout->addWidget(Members.WorkshopProposal);

		Members.Publish
			= make_action(this, tr("Publish"), QStringLiteral("reviewPublish"));
		Members.Discard
			= make_action(this, tr("Discard"), QStringLiteral("reviewDiscard"));
		auto* const publicationControls = new QHBoxLayout{ };
		for (auto* const action : { Members.Publish, Members.Discard }) {
			auto* const button = new QToolButton{ this };
			button->setObjectName(action->objectName() + QStringLiteral("Button"));
			button->setDefaultAction(action);
			button->setToolButtonStyle(Qt::ToolButtonTextOnly);
			button->setFocusPolicy(Qt::StrongFocus);
			publicationControls->addWidget(button);
		}
		publicationControls->addStretch();
		layout->insertLayout(2, publicationControls);

		Members.MarkRead = make_action(
			this, tr("Mark read"), QStringLiteral("reviewMarkRead"));
		Members.Accept = make_action(
			this, tr("Accept file"), QStringLiteral("reviewAccept"));
		Members.Exclude = make_action(
			this, tr("Exclude file"), QStringLiteral("reviewExclude"));
		Members.AcceptRevision = make_action(
			this,
			tr("Accept revision"),
			QStringLiteral("reviewAcceptRevision"));
		Members.RejectRevision = make_action(
			this,
			tr("Reject revision"),
			QStringLiteral("reviewRejectRevision"));

		// One control per declared operation, in the enumerator's own order, so
		// the array index *is* the operation. A seventh operation adds a row
		// here and fails to compile until it does, which is what keeps the six
		// controls and the closed set from drifting apart.
		const auto controls
			= std::array<std::pair<QString, QString>,
						 core::workshop_operation_count>{ {
				{ tr("Clarify"), QStringLiteral("reviewWorkshopClarify") },
				{ tr("Shorten"), QStringLiteral("reviewWorkshopShorten") },
				{ tr("Make actionable"),
				  QStringLiteral("reviewWorkshopMakeActionable") },
				{ tr("Phrase as a question"),
				  QStringLiteral("reviewWorkshopPhraseAsAQuestion") },
				{ tr("Make more direct"),
				  QStringLiteral("reviewWorkshopMakeMoreDirect") },
				{ tr("Check the premise"),
				  QStringLiteral("reviewWorkshopCheckThePremise") },
			} };
		for (auto index = std::size_t{ 0U }; index < controls.size(); ++index) {
			auto* const control
				= make_action(this, controls[index].first, controls[index].second);
			Members.WorkshopOperations[index] = control;
			const auto asked
				= static_cast<core::workshop_operation>(index);
			connect(control, &QAction::triggered, this, [this, asked]() {
				request_workshop(asked);
			});
		}

		connect(Members.Publish, &QAction::triggered, this, [this]() {
			emit_draft(core::review_draft_command_kind::begin_publish);
		});
		connect(Members.Discard, &QAction::triggered, this, [this]() {
			emit_draft(core::review_draft_command_kind::discard);
		});
		connect(Members.MarkRead, &QAction::triggered, this, [this]() {
			emit_receipt(core::file_receipt_command_kind::mark_read);
		});
		connect(Members.Accept, &QAction::triggered, this, [this]() {
			emit_receipt(core::file_receipt_command_kind::accept);
		});
		connect(Members.Exclude, &QAction::triggered, this, [this]() {
			emit_receipt(core::file_receipt_command_kind::exclude);
		});
		connect(Members.AcceptRevision, &QAction::triggered, this, [this]() {
			accept_revision();
		});
		connect(Members.RejectRevision, &QAction::triggered, this, [this]() {
			reject_revision();
		});

		refresh_enablement();
	}

	auto review_draft_editor::body_editor() const noexcept -> QPlainTextEdit*
	{
		return Members.Body;
	}

	auto review_draft_editor::label_controls() const noexcept -> feedback_label_controls*
	{
		return Members.Labels;
	}

	void review_draft_editor::set_label_marks(const feedback_mark_theme& aTheme)
	{
		Members.Labels->set_theme(aTheme);
	}

	void review_draft_editor::set_label_helper(const bool aEnabled)
	{
		Members.Labels->set_helper(aEnabled);
	}

	auto review_draft_editor::target_label() const noexcept -> QLabel*
	{
		return Members.TargetLabel;
	}

	auto review_draft_editor::mutation_status_label() const noexcept -> QLabel*
	{
		return Members.MutationStatus;
	}

	auto review_draft_editor::mutation_status_text() const -> QString
	{
		return Members.MutationStatus->text();
	}

	void review_draft_editor::allow_publication_retry(bool aAllowed)
	{
		Members.RetryPublication = aAllowed;
		refresh_enablement();
	}
	auto review_draft_editor::publish_action() const noexcept -> QAction*
	{
		return Members.Publish;
	}

	auto review_draft_editor::discard_action() const noexcept -> QAction*
	{
		return Members.Discard;
	}

	auto review_draft_editor::mark_read_action() const noexcept -> QAction*
	{
		return Members.MarkRead;
	}

	auto review_draft_editor::accept_action() const noexcept -> QAction*
	{
		return Members.Accept;
	}

	auto review_draft_editor::exclude_action() const noexcept -> QAction*
	{
		return Members.Exclude;
	}

	auto review_draft_editor::workshop_original_editor() const noexcept
		-> QPlainTextEdit*
	{
		return Members.WorkshopOriginal;
	}

	auto review_draft_editor::workshop_proposal_editor() const noexcept
		-> QPlainTextEdit*
	{
		return Members.WorkshopProposal;
	}

	auto review_draft_editor::workshop_original_text() const -> QString
	{
		return Members.WorkshopOriginal->toPlainText();
	}

	auto review_draft_editor::workshop_proposal_text() const -> QString
	{
		return Members.WorkshopProposal->toPlainText();
	}

	auto review_draft_editor::accept_revision_action() const noexcept -> QAction*
	{
		return Members.AcceptRevision;
	}

	auto review_draft_editor::reject_revision_action() const noexcept -> QAction*
	{
		return Members.RejectRevision;
	}

	auto review_draft_editor::workshop_action(
		const core::workshop_operation aOperation) const noexcept -> QAction*
	{
		if (!core::declared(aOperation)) {
			return nullptr;
		}
		return Members.WorkshopOperations[static_cast<std::size_t>(aOperation)];
	}

	auto review_draft_editor::clarify_action() const noexcept -> QAction*
	{
		return workshop_action(core::workshop_operation::clarify);
	}

	auto review_draft_editor::shorten_action() const noexcept -> QAction*
	{
		return workshop_action(core::workshop_operation::shorten);
	}

	auto review_draft_editor::make_actionable_action() const noexcept -> QAction*
	{
		return workshop_action(core::workshop_operation::make_actionable);
	}

	auto review_draft_editor::phrase_as_a_question_action() const noexcept
		-> QAction*
	{
		return workshop_action(core::workshop_operation::phrase_as_a_question);
	}

	auto review_draft_editor::make_more_direct_action() const noexcept -> QAction*
	{
		return workshop_action(core::workshop_operation::make_more_direct);
	}

	auto review_draft_editor::check_the_premise_action() const noexcept -> QAction*
	{
		return workshop_action(core::workshop_operation::check_the_premise);
	}

	auto review_draft_editor::workshop_operation_label() const noexcept -> QLabel*
	{
		return Members.WorkshopOperation;
	}

	auto review_draft_editor::workshop_operation_text() const -> QString
	{
		return Members.WorkshopOperation->text();
	}

	void review_draft_editor::set_workshop_dispatch(
		comment_workshop_dispatch aDispatch)
	{
		Members.WorkshopDispatch = std::move(aDispatch);
		refresh_enablement();
	}

	auto review_draft_editor::shown_draft() const noexcept
		-> const std::optional<core::review_comment_draft>&
	{
		return Members.ShownDraft;
	}

	auto review_draft_editor::shown_receipt() const noexcept
		-> const std::optional<core::file_receipt>&
	{
		return Members.ShownReceipt;
	}

	auto review_draft_editor::shown_proposal() const noexcept
		-> const std::optional<comment_workshop_proposal>&
	{
		return Members.ShownProposal;
	}

	auto review_draft_editor::statistics() const noexcept
		-> review_draft_editor_statistics
	{
		return Members.Statistics;
	}

	void review_draft_editor::describe_target(const QString& aDescription)
	{
		Members.TargetLabel->setText(aDescription);
	}

	void review_draft_editor::emit_draft(
		const core::review_draft_command_kind aKind)
	{
		auto& revisioned = Members.Revisioned;
		if (revisioned.Dispatch) {
			if (!revisioned.Selected || !revisioned.Accepted || revisioned.Conflict) {
				return;
			}
			if (aKind == core::review_draft_command_kind::edit && revisioned.InFlight) {
				revisioned.Deferred = true;
				revisioned.Flush->start();
				return;
			}
			auto body = Members.Body->toPlainText().toStdString();
			if (body.size() > core::maximum_shared_draft_body_bytes) {
				Members.MutationStatus->setText(tr("Draft text exceeds the size limit."));
				return;
			}
			auto command = core::revisioned_review_draft_command{ { revisioned.Accepted->Draft.Id, aKind, std::move(body) }, revisioned.Accepted->Revision, false, { }, revisioned.Accepted->Scope };
			if (aKind == core::review_draft_command_kind::edit) {
				revisioned.Candidate = command;
				revisioned.InFlight  = true;
			}
			++Members.Statistics.DraftIntents;
			revisioned.Dispatch(command);
			return;
		}
		if (!Members.ShownDraft || !Members.DraftDispatch) {
			return;
		}
		++Members.Statistics.DraftIntents;
		Members.DraftDispatch(core::review_draft_command{
			.Id   = Members.ShownDraft->Id,
			.Kind = aKind,
			.Body = Members.Body->toPlainText().toStdString(),
		});
	}

	void review_draft_editor::emit_receipt(
		const core::file_receipt_command_kind aKind)
	{
		if (!Members.ShownReceipt || !Members.ReceiptDispatch) {
			return;
		}
		++Members.Statistics.ReceiptIntents;
		Members.ReceiptDispatch(core::file_receipt_command{
			.File     = Members.ShownReceipt->File,
			.Material = Members.ShownReceipt->Material,
			.Kind     = aKind,
		});
	}

	void review_draft_editor::accept_revision()
	{
		if (Members.Revisioned.Dispatch) {
			auto& state = Members.Revisioned;
			if (!state.Selected || state.Conflict || !state.Accepted || !Members.ShownProposal || Members.ShownProposal->Draft != state.Accepted->Draft.Id || Members.ShownProposal->Proposed.size() > core::maximum_shared_draft_body_bytes) {
				return;
			}
			auto command    = core::revisioned_review_draft_command{ { state.Accepted->Draft.Id, core::review_draft_command_kind::edit, Members.ShownProposal->Proposed }, state.Accepted->Revision, false, { }, state.Accepted->Scope };
			state.Candidate = command;
			++Members.Statistics.DraftIntents;
			state.DispatchingWorkshopAcceptance = true;
			state.Dispatch(command);
			state.DispatchingWorkshopAcceptance = false;
			return;
		}

		if (!Members.ShownProposal || !Members.ShownDraft
			|| !Members.DraftDispatch
			|| Members.ShownDraft->Id != Members.ShownProposal->Draft) {
			return;
		}
		++Members.Statistics.DraftIntents;
		// One command, and nothing else. The body is deliberately not written
		// here: the editor moves when an accepted edit projection comes back,
		// and writing it would both pre-empt that answer and emit a second
		// command through the body's own change signal.
		Members.DraftDispatch(core::review_draft_command{
			.Id   = Members.ShownProposal->Draft,
			.Kind = core::review_draft_command_kind::edit,
			.Body = Members.ShownProposal->Proposed,
		});
	}

	void review_draft_editor::reject_revision()
	{
		if (!Members.ShownProposal) {
			return;
		}
		++Members.Statistics.RejectedRevisions;
		// The reviewer's own text is never touched on this path, which is what
		// makes rejection leave it byte-identical rather than restore it.
		clear_workshop();
		refresh_enablement();
	}

	void review_draft_editor::request_workshop(
		const core::workshop_operation aOperation)
	{
		// The same shape as every other outbound path here: nothing is asked
		// for without a draft to ask about, and an unwired editor asks nobody.
		// The draft named is the one on screen, read now rather than resolved
		// later by whoever answers.
		const auto* const control = workshop_action(aOperation);
		if (!Members.ShownDraft || !Members.WorkshopDispatch
			|| control == nullptr
			|| Members.ShownDraft->State != core::review_draft_state::drafting) {
			return;
		}
		++Members.Statistics.WorkshopRequests;
		// Asked, and until it is answered no second question may be asked
		// about this comment. Set before the dispatch, because the answer can
		// arrive inside it.
		Members.WorkshopAsked = true;
		refresh_enablement();
		// Said on screen at once, and said as a question rather than as an
		// answer: what is shown beside it is still the reviewer's own text
		// until a proposal arrives. Nothing here proposes anything.
		Members.WorkshopOperation->setText(
			tr("Asked for: %1").arg(control->text()));
		Members.WorkshopDispatch(comment_workshop_request{
			.Draft     = Members.ShownDraft->Id,
			.Operation = aOperation,
		});
	}

	void review_draft_editor::clear_workshop()
	{
		Members.ShownProposal.reset();
		Members.WorkshopAsked = false;
		Members.WorkshopOriginal->clear();
		Members.WorkshopProposal->clear();
		Members.WorkshopOperation->clear();
	}

	auto review_draft_editor::reply_target_label() const noexcept -> QLabel*
	{
		return Members.ReplyTarget;
	}

	void review_draft_editor::describe_reply_target(const QString& aDescription)
	{
		Members.ReplyTarget->setText(aDescription);
		Members.ReplyTarget->setVisible(
			!aDescription.isEmpty() && Members.ShownDraft.has_value()
			&& Members.ShownDraft->ReplyToThread.has_value());
	}

	void review_draft_editor::present(const core::review_comment_draft& aDraft)
	{
		if (Members.Revisioned.Dispatch) {
			return;
		}
		render_draft(aDraft);
	}
	void review_draft_editor::set_presentation_observer(std::function<void()> aObserver)
	{
		Members.PresentationObserver = std::move(aObserver);
	}
	void review_draft_editor::set_revisioned_dispatch(std::function<void(core::revisioned_review_draft_command)> aDispatch)
	{
		Members.Revisioned.Dispatch = std::move(aDispatch);
		refresh_enablement();
	}
	auto review_draft_editor::reload_draft_action() const noexcept -> QAction* { return Members.Revisioned.Reload; }
	auto review_draft_editor::reapply_draft_action() const noexcept -> QAction* { return Members.Revisioned.Reapply; }
	auto review_draft_editor::rejected_candidate() const noexcept -> const std::optional<core::revisioned_review_draft_command>& { return Members.Revisioned.Candidate; }

	auto review_draft_editor::workshop_accepting(
		const core::revisioned_review_draft_command& aCommand) const -> bool
	{
		if (!Members.Revisioned.DispatchingWorkshopAcceptance
			|| !Members.Revisioned.Candidate.has_value()) {
			return false;
		}
		const auto& candidate = *Members.Revisioned.Candidate;
		return candidate.Command == aCommand.Command
			&& candidate.ExpectedRevision == aCommand.ExpectedRevision
			&& candidate.ChangeReplyTarget == aCommand.ChangeReplyTarget
			&& candidate.ReplyToThread == aCommand.ReplyToThread
			&& candidate.Scope == aCommand.Scope;
	}
	void review_draft_editor::present(const core::revisioned_review_draft& aDraft)
	{
		auto& state = Members.Revisioned;
		if (!state.Dispatch || aDraft.Revision == 0 || aDraft.Scope.value() == 0) {
			return;
		}
		if (state.Accepted && state.Accepted->Scope == aDraft.Scope && state.Accepted->Draft.Id == aDraft.Draft.Id && aDraft.Revision < state.Accepted->Revision) {
			return;
		}
		if (state.Candidate && (state.Candidate->Scope != aDraft.Scope || state.Candidate->Command.Id != aDraft.Draft.Id)) {
			state.Conflict    = true;
			state.Selected    = false;
			state.Replacement = aDraft;
			Members.MutationStatus->setText(tr("Your draft text is retained. Reload to discard it and show the selected draft, or return to reapply it."));
			refresh_enablement();
			return;
		}
		state.Replacement.reset();
		state.Accepted = aDraft;
		state.Selected = true;
		if (state.Candidate) {
			state.Conflict     = true;
			Members.ShownDraft = aDraft.Draft;
			Members.MutationStatus->setText(tr("Draft changed in another window. Reload accepted text or reapply your text."));
			refresh_enablement();
			return;
		}
		render_draft(aDraft.Draft);
	}
	void review_draft_editor::acknowledge(const core::revisioned_review_draft_command& aCommand, const core::revisioned_review_draft& aAccepted)
	{
		auto& state = Members.Revisioned;
		if (!state.Dispatch || !state.Candidate || !state.Accepted
			|| (!(state.Selected && !state.Replacement)
				&& (!state.Replacement || *state.Replacement != aAccepted))) {
			return;
		}
		const auto& candidate = *state.Candidate;
		if (candidate.Command != aCommand.Command || candidate.Scope != aCommand.Scope || candidate.ExpectedRevision != aCommand.ExpectedRevision
			|| candidate.ChangeReplyTarget != aCommand.ChangeReplyTarget || candidate.ReplyToThread != aCommand.ReplyToThread
			|| aCommand.Command.Kind != core::review_draft_command_kind::edit || aCommand.ChangeReplyTarget
			|| aAccepted.Scope != aCommand.Scope || aAccepted.Draft.Id != aCommand.Command.Id || aAccepted.Draft.Body != aCommand.Command.Body
			|| aAccepted.Revision < aCommand.ExpectedRevision || aAccepted.Revision - aCommand.ExpectedRevision > 1
			|| state.Accepted->Scope != aAccepted.Scope || state.Accepted->Draft.Id != aAccepted.Draft.Id || state.Accepted->Revision > aAccepted.Revision) {
			return;
		}
		state.Candidate.reset();
		state.Conflict = false;
		state.Accepted = aAccepted;
		Members.MutationStatus->clear();
		const auto deferred = state.Deferred;
		settle_in_flight();
		// The text typed while this edit was answered goes next, on the
		// revision just accepted; showing the accepted body would undo it.
		if (deferred && Members.Body->toPlainText().toStdString() != aAccepted.Draft.Body) {
			Members.ShownDraft = aAccepted.Draft;
			emit_draft(core::review_draft_command_kind::edit);
			refresh_enablement();
			return;
		}
		render_draft(aAccepted.Draft);
	}
	void review_draft_editor::acknowledge_published_body(const core::revisioned_review_draft& aPublished)
	{
		auto& state = Members.Revisioned;
		if (!state.Candidate || state.Candidate->Command.Kind != core::review_draft_command_kind::edit
			|| state.Candidate->Scope != aPublished.Scope
			|| state.Candidate->Command.Id != aPublished.Draft.Id
			|| state.Candidate->Command.Body != aPublished.Draft.Body) {
			return;
		}
		state.Candidate.reset();
		state.Conflict = false;
		state.Replacement.reset();
		settle_in_flight();
		refresh_enablement();
	}

	void review_draft_editor::present_conflict(const core::revisioned_draft_outcome& aOutcome)
	{
		auto& state = Members.Revisioned;
		if (!state.Candidate || state.Candidate->Scope != aOutcome.Accepted.Scope || state.Candidate->Command.Id != aOutcome.Accepted.Draft.Id) {
			return;
		}
		settle_in_flight();
		state.Conflict = true;
		present(aOutcome.Accepted);
	}
	void review_draft_editor::settle_in_flight()
	{
		Members.Revisioned.InFlight = false;
		Members.Revisioned.Deferred = false;
		if (Members.Revisioned.Flush != nullptr) {
			Members.Revisioned.Flush->stop();
		}
	}

	void review_draft_editor::render_draft(const core::review_comment_draft& aDraft)
	{
		++Members.Statistics.Presentations;
		Members.ShownDraft = aDraft;
		// [[BPR-0657]]. The thread named is shown only while the draft answers one.
		Members.ReplyTarget->setVisible(
			aDraft.ReplyToThread.has_value() && !Members.ReplyTarget->text().isEmpty());
		if (Members.Body->toPlainText().toStdString() != aDraft.Body) {
			Members.ApplyingPresentation = true;
			Members.Body->setPlainText(QString::fromStdString(aDraft.Body));
			Members.ApplyingPresentation = false;
		}
		refresh_enablement();
	}

	void review_draft_editor::present(
		const comment_workshop_proposal& aProposal)
	{
		++Members.Statistics.WorkshopPresentations;
		// Answered. The offer reopens here rather than on a timer or on the
		// run finishing, because a before/after on screen is the only evidence
		// this widget has that the question it asked was answered.
		Members.WorkshopAsked = false;
		Members.ShownProposal = aProposal;
		Members.WorkshopOriginal->setPlainText(
			QString::fromStdString(aProposal.Original));
		Members.WorkshopProposal->setPlainText(
			QString::fromStdString(aProposal.Proposed));
		// The proposal says which operation produced it, or says nothing at
		// all: a promoted finding is a revision nobody workshopped, and
		// labelling it with one of the six would name an operation that never
		// ran.
		const auto* const named = aProposal.Operation
			? workshop_action(*aProposal.Operation)
			: nullptr;
		if (named != nullptr) {
			Members.WorkshopOperation->setText(
				tr("Proposed by: %1").arg(named->text()));
		} else {
			Members.WorkshopOperation->clear();
		}
		refresh_enablement();
	}

	void review_draft_editor::present(
		const comment_workshop_settlement& aSettlement)
	{
		// About this comment or about none. A settlement for a draft the
		// reviewer has already navigated away from reopens nothing here: the
		// controls it would reopen belong to the comment on screen now.
		if (!Members.ShownDraft || Members.ShownDraft->Id != aSettlement.Draft) {
			return;
		}
		// The turn is over, so "a workshop turn is already running for this
		// comment" is over with it. The six reopen on the turn ending rather
		// than on an answer arriving, because a turn that answered nothing
		// still ended.
		Members.WorkshopAsked = false;
		if (!aSettlement.Reason.empty()) {
			// Said where "Proposed by: …" would have been said, and only when
			// there is no proposal coming. A reason written beside a shown
			// before/after would contradict it.
			Members.WorkshopOperation->setText(
				tr("No revision: %1")
					.arg(QString::fromStdString(aSettlement.Reason)));
		}
		refresh_enablement();
	}

	void review_draft_editor::present(
		const application::review_provider_capabilities aCapabilities)
	{
		++Members.Statistics.MutationUpdates;
		if (aCapabilities.ResolvesThreads && aCapabilities.RecordsViewedState) {
			Members.MutationStatus->setText(
				QStringLiteral("Review publication is ready."));
			return;
		}
		Members.MutationStatus->setText(QStringLiteral(
			"Publication is available; native thread resolution and viewed-state "
			"recording are unavailable."));
	}

	void review_draft_editor::present(
		const application::review_mutation_failure aFailure)
	{
		++Members.Statistics.MutationUpdates;
		switch (aFailure) {
		case application::review_mutation_failure::unavailable:
			Members.MutationStatus->setText(QStringLiteral(
				"Publication result is unavailable; retry is safe."));
			break;
		case application::review_mutation_failure::target_unavailable:
			Members.MutationStatus->setText(QStringLiteral(
				"The account cannot see the publication target; refresh and check access."));
			break;
		case application::review_mutation_failure::malformed_response:
			Members.MutationStatus->setText(QStringLiteral(
				"The server returned an invalid publication response; retry is safe."));
			break;
		case application::review_mutation_failure::refused_by_server:
			Members.MutationStatus->setText(
				QStringLiteral("The server refused publication."));
			break;
		case application::review_mutation_failure::undesignated_target:
			Members.MutationStatus->setText(QStringLiteral(
				"Publication was refused because the pull request changed."));
			break;
		case application::review_mutation_failure::target_moved:
			Members.MutationStatus->setText(QStringLiteral(
				"Publication target can no longer be named."));
			break;
		case application::review_mutation_failure::stale_head:
			Members.MutationStatus->setText(QStringLiteral(
				"Publication was refused because the pull request head moved."));
			break;
		case application::review_mutation_failure::incompatible_body:
			Members.MutationStatus->setText(QStringLiteral(
				"This comment needs a supported inline line anchor. Your draft text is kept."));
			break;
		case application::review_mutation_failure::unsupported:
			Members.MutationStatus->setText(QStringLiteral(
				"Publication is unavailable for this target. Your draft text is kept."));
			break;
		case application::review_mutation_failure::cancelled:
			Members.MutationStatus->setText(
				QStringLiteral("Publication was cancelled."));
			break;
		}
	}

	void review_draft_editor::present(
		const application::review_mutation_receipt& aReceipt,
		const application::reconciliation_result&   aReconciliation)
	{
		++Members.Statistics.MutationUpdates;
		Members.MutationStatus->setText(
			QStringLiteral("Published %1%2; refreshed %3 comment(s).")
				.arg(QString::fromStdString(aReceipt.ServerCommentId))
				.arg(aReceipt.Duplicate ? QStringLiteral(" by safe retry")
										: QString{ })
				.arg(aReconciliation.Projection.size()));
	}

	void review_draft_editor::present(
		const review_draft_editor_clear_reason aReason)
	{
		++Members.Statistics.Presentations;
		Members.ShownDraft.reset();
		Members.ShownReceipt.reset();
		Members.ApplyingPresentation = true;
		if (!Members.Revisioned.Candidate) {
			Members.Body->clear();
		}
		Members.Revisioned.Selected  = false;
		Members.ApplyingPresentation = false;
		// A proposal is about a draft that is no longer selected, so it goes
		// with it rather than sitting beside a body nobody is editing.
		clear_workshop();
		switch (aReason) {
		case review_draft_editor_clear_reason::selection_cleared:
			Members.MutationStatus->setText(QStringLiteral(
				"Select a current-head diff line to compose a review comment."));
			break;
		case review_draft_editor_clear_reason::selection_invalidated:
			Members.MutationStatus->setText(
				QStringLiteral("The selected diff is no longer available."));
			break;
		case review_draft_editor_clear_reason::composition_retired:
			Members.TargetLabel->clear();
			Members.MutationStatus->clear();
			break;
		}
		refresh_enablement();
	}

	void review_draft_editor::present(const core::file_receipt& aReceipt)
	{
		++Members.Statistics.Presentations;
		Members.ShownReceipt = aReceipt;
		refresh_enablement();
	}

	void review_draft_editor::refresh_enablement()
	{
		// Enablement follows the accepted projection, never the local edit.
		// A publish button enabled because the user typed something would be
		// the widget deciding legality the core owns.
		const auto revisionReady = !Members.Revisioned.Dispatch || (Members.Revisioned.Selected && !Members.Revisioned.Conflict);
		const auto reload        = (Members.Revisioned.Selected || Members.Revisioned.Replacement.has_value()) && Members.Revisioned.Candidate.has_value();
		const auto reapply       = Members.Revisioned.Selected && Members.Revisioned.Candidate.has_value();
		Members.Revisioned.Reload->setVisible(reload);
		Members.Revisioned.Reapply->setVisible(reapply);
		Members.Revisioned.Reload->setEnabled(reload);
		Members.Revisioned.Reapply->setEnabled(reapply);
		const auto drafting = revisionReady && Members.ShownDraft
			&& Members.ShownDraft->State == core::review_draft_state::drafting;
		const auto supportedTarget = Members.ShownDraft && core::supported_comment_target(Members.ShownDraft->Anchor.Side, Members.ShownDraft->ReplyToThread.has_value());
		const auto retry           = revisionReady && Members.RetryPublication && Members.ShownDraft && Members.ShownDraft->State == core::review_draft_state::publishing;
		Members.Publish->setText(retry ? tr("Retry publication") : tr("Publish"));
		Members.Publish->setEnabled((drafting || retry) && supportedTarget && !Members.Revisioned.Candidate);
		Members.Discard->setEnabled(drafting);
		Members.Body->setReadOnly(!drafting);
		Members.Body->setVisible(Members.ShownDraft.has_value() || Members.Revisioned.Candidate.has_value());
		Members.Labels->setVisible(Members.ShownDraft.has_value() || Members.Revisioned.Candidate.has_value());
		Members.Labels->refresh();
		Members.TargetLabel->setVisible(!Members.TargetLabel->text().isEmpty());
		const auto unavailable = tr("No editable draft is selected. Return to an open pull request and select a current-head line.");
		Members.Publish->setToolTip(retry ? tr("Retry this exact publication identity; already transmitted work is reconciled.") : !drafting ? unavailable
										: supportedTarget                                                                                    ? tr("Publish this draft comment.")
																																			 : tr("New comments need a line on the right, current-head side. Your draft text is kept."));
		Members.Discard->setToolTip(drafting ? tr("Discard this local draft.") : unavailable);

		const auto receipt = Members.ShownReceipt.has_value();
		Members.MarkRead->setEnabled(receipt);
		Members.Accept->setEnabled(receipt && !Members.ShownReceipt->Excluded);
		Members.Exclude->setEnabled(receipt && !Members.ShownReceipt->Accepted);

		// A revision may only be accepted onto the draft it was proposed
		// against, and only while that draft is still the reviewer's to edit.
		// Rejecting is always available, because dropping a proposal asks
		// nothing of anybody.
		// The six operations are offered whenever there is a draft to revise
		// and refused, visibly, when there is not. Disabled rather than
		// hidden, with the reason on the control: a workshop whose controls
		// vanished would read as a workshop that does not exist, which is the
		// misunderstanding this widget was built to end.
		// A press already waiting for its answer closes the offer until that
		// answer arrives. Disabled and saying so, never queued: a second press
		// would either ask about a comment the first turn is already revising
		// or sit silently until the reviewer concluded the control was dead.
		const auto offered = drafting && Members.WorkshopDispatch != nullptr;
		for (auto* const control : Members.WorkshopOperations) {
			control->setEnabled(offered && !Members.WorkshopAsked);
			if (!offered) {
				control->setToolTip(tr("Select a draft comment to workshop it."));
				continue;
			}
			control->setToolTip(
				Members.WorkshopAsked
					? tr("A workshop turn is already running for this comment.")
					: tr("Ask the agent to revise your comment."));
		}

		const auto proposed = Members.ShownProposal.has_value();
		Members.AcceptRevision->setEnabled(
			proposed && drafting
			&& Members.ShownDraft->Id == Members.ShownProposal->Draft);
		Members.RejectRevision->setEnabled(proposed);
		Members.WorkshopOriginal->setVisible(proposed);
		Members.WorkshopProposal->setVisible(proposed);
		Members.WorkshopOperation->setVisible(proposed || Members.WorkshopAsked);
		if (Members.PresentationObserver) {
			Members.PresentationObserver();
		}
	}
} // namespace bpr::qt
