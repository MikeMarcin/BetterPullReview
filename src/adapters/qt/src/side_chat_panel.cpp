#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/core/revisioned_review_draft.hpp"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>
#include <array>
#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief Renders a core-supplied name as presentable text.
		[[nodiscard]] auto text(const std::string_view aName) -> QString
		{
			return QString::fromUtf8(
				aName.data(), static_cast<qsizetype>(aName.size()));
		}

		/// \brief The scopes a reviewer may widen to, in core's own order.
		///
		/// \details Written out so a fifth declared scope fails here rather
		/// than quietly never being offered.
		constexpr auto offered_scopes = std::array{
			core::agent_chat_scope_kind::selected_lines,
			core::agent_chat_scope_kind::file,
			core::agent_chat_scope_kind::pull_request,
			core::agent_chat_scope_kind::repository,
		};

		static_assert(
			offered_scopes.size() == core::agent_chat_scope_kind_count,
			"every declared chat scope must be offerable");

		/// \brief [[BPR-1594]]. A scope as the reviewer reads it.
		[[nodiscard]] auto scope_words(const core::agent_chat_scope_kind aScope) -> QString
		{
			switch (aScope) {
			case core::agent_chat_scope_kind::selected_lines:
				return QStringLiteral("Selected lines");
			case core::agent_chat_scope_kind::file:
				return QStringLiteral("This file");
			case core::agent_chat_scope_kind::pull_request:
				return QStringLiteral("Pull request");
			case core::agent_chat_scope_kind::repository:
				return QStringLiteral("Repository");
			}
			return QStringLiteral("Selected lines");
		}

		/// \brief Creates one named field label owned by the panel.
		[[nodiscard]] auto make_field(
			QWidget*       aParent,
			const QString& aObjectName,
			const QString& aAccessibleName) -> QLabel*
		{
			auto* const field = new QLabel{ aParent };
			field->setObjectName(aObjectName);
			field->setAccessibleName(aAccessibleName);
			field->setTextFormat(Qt::PlainText);
			field->setWordWrap(true);
			return field;
		}

		/// \brief Creates one named action owned by the panel.
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
	} // namespace

	side_chat_panel::side_chat_panel(
		side_chat_intent_dispatch aDispatch,
		QWidget* const            aParent)
		: QWidget{ aParent }
	{
		Members.Dispatch = std::move(aDispatch);

		setObjectName(QStringLiteral("sideChatPanel"));
		setAccessibleName(QStringLiteral("AI side chat"));

		// The visible session contract, one field per fact. A reviewer who
		// has to read a provider out of a sentence about a workspace has not
		// been shown either.
		Members.Provider = make_field(
			this,
			QStringLiteral("sideChatProvider"),
			QStringLiteral("AI provider"));
		Members.GitHost = make_field(
			this,
			QStringLiteral("sideChatGitHost"),
			QStringLiteral("Git host"));
		Members.Workspace = make_field(
			this,
			QStringLiteral("sideChatWorkspace"),
			QStringLiteral("Workspace"));
		Members.EpochPair = make_field(
			this,
			QStringLiteral("sideChatEpochPair"),
			QStringLiteral("Selected epoch pair"));
		Members.HeadOid = make_field(
			this,
			QStringLiteral("sideChatHeadOid"),
			QStringLiteral("Exact head commit"));
		Members.SessionState = make_field(
			this,
			QStringLiteral("sideChatSessionState"),
			QStringLiteral("Session state"));
		Members.Scope = make_field(
			this,
			QStringLiteral("sideChatScope"),
			QStringLiteral("Chat scope"));
		Members.Chips = make_field(
			this,
			QStringLiteral("sideChatContextChips"),
			QStringLiteral("Supplemental context"));
		Members.PrivateContext = make_field(
			this,
			QStringLiteral("sideChatPrivateContext"),
			QStringLiteral("Private notes and drafts"));
		Members.Continuations = make_field(
			this,
			QStringLiteral("sideChatContinuations"),
			QStringLiteral("Continuation choices"));

		Members.ScopeSelector = new QComboBox{ this };
		Members.ScopeSelector->setObjectName(
			QStringLiteral("sideChatScopeSelector"));
		Members.ScopeSelector->setAccessibleName(
			QStringLiteral("Context to add"));
		for (const auto scope : offered_scopes) {
			Members.ScopeSelector->addItem(
				scope_words(scope),
				QVariant::fromValue<uint>(static_cast<uint>(scope)));
		}

		Members.NoteOptIn = new QCheckBox{
			QStringLiteral("Include my private notes"), this
		};
		Members.NoteOptIn->setObjectName(QStringLiteral("sideChatNoteOptIn"));
		Members.NoteOptIn->setAccessibleName(
			QStringLiteral("Include private notes in this session"));
		Members.DraftOptIn = new QCheckBox{
			QStringLiteral("Include my unpublished drafts"), this
		};
		Members.DraftOptIn->setObjectName(QStringLiteral("sideChatDraftOptIn"));
		Members.DraftOptIn->setAccessibleName(
			QStringLiteral("Include unpublished drafts in this session"));

		Members.Composer = new QPlainTextEdit{ this };
		Members.Composer->setObjectName(QStringLiteral("sideChatComposer"));
		Members.Composer->setAccessibleName(
			QStringLiteral("Message to the agent"));
		Members.Composer->setPlaceholderText(
			QStringLiteral("Ask about the selected lines"));

		auto* const layout = new QVBoxLayout{ this };
		auto* const form   = new QFormLayout{ };
		form->addRow(QStringLiteral("Provider"), Members.Provider);
		form->addRow(QStringLiteral("Git host"), Members.GitHost);
		form->addRow(QStringLiteral("Workspace"), Members.Workspace);
		form->addRow(QStringLiteral("Epochs"), Members.EpochPair);
		form->addRow(QStringLiteral("Head"), Members.HeadOid);
		form->addRow(QStringLiteral("Session"), Members.SessionState);
		form->addRow(QStringLiteral("Scope"), Members.Scope);
		form->addRow(QStringLiteral("Context"), Members.Chips);
		form->addRow(QStringLiteral("Private"), Members.PrivateContext);
		form->addRow(QStringLiteral("Continue"), Members.Continuations);
		layout->addLayout(form);
		layout->addWidget(Members.NoteOptIn);
		layout->addWidget(Members.DraftOptIn);

		auto* const context = new QHBoxLayout{ };
		context->addWidget(Members.ScopeSelector);
		layout->addLayout(context);
		layout->addWidget(Members.Composer);
		Members.Replies = new QListWidget{ this };
		Members.Replies->setObjectName(QStringLiteral("sideChatReplies"));
		Members.Replies->setAccessibleName(tr("Recent agent replies in this session"));
		Members.Replies->setMaximumHeight(120);
		layout->addWidget(Members.Replies);
		Members.ReplyBody = new QPlainTextEdit{ this };
		Members.ReplyBody->setObjectName(QStringLiteral("sideChatReplyBody"));
		Members.ReplyBody->setAccessibleName(tr("Selected agent reply"));
		Members.ReplyBody->setReadOnly(true);
		Members.ReplyBody->setMaximumHeight(180);
		layout->addWidget(Members.ReplyBody);
		Members.Promote     = make_action(this, tr("Promote reply to draft"), QStringLiteral("promoteChatReplyAction"));
		auto* promoteButton = new QToolButton{ this };
		promoteButton->setDefaultAction(Members.Promote);
		promoteButton->setFocusPolicy(Qt::StrongFocus);
		promoteButton->setAccessibleName(tr("Promote reply to draft"));
		promoteButton->setAccessibleDescription(tr("Copy the selected reply to a local draft without publishing."));
		layout->addWidget(promoteButton);
		connect(Members.Replies, &QListWidget::currentRowChanged, this, [this] { refresh_enablement(); });
		connect(Members.Promote, &QAction::triggered, this, [this] {
			refresh_enablement();
			if (!Members.Promote->isEnabled() || !Members.Dispatch || !Members.Shown) {
				return;
			}
			Members.Dispatch(promote_chat_reply_intent{ Members.Shown->Session.Id, Members.Replies->currentItem()->data(Qt::UserRole).toULongLong() });
		});

		Members.Send = make_action(
			this, tr("Send"), QStringLiteral("sideChatSend"));
		Members.AddContext = make_action(
			this, tr("Add context"), QStringLiteral("sideChatAddContext"));
		Members.NewSession = make_action(
			this,
			tr("Start a new session"),
			QStringLiteral("sideChatStartNewSession"));
		Members.ContinueOldRevision = make_action(
			this,
			tr("Continue against the old revision"),
			QStringLiteral("sideChatContinueOldRevision"));

		connect(Members.Send, &QAction::triggered, this, [this]() {
			if (!Members.Shown.has_value() || !Members.Dispatch) {
				return;
			}
			++Members.Statistics.SendIntents;
			// The message leaves; nothing here moves. The transcript is a
			// projection, so a sent turn appears when the core accepts it.
			Members.Dispatch(send_chat_message_intent{
				.Session = Members.Shown->Session.Id,
				.Action  = core::agent_action_intent{
					Members.Shown->EntryPoint },
				.Scope = Members.Shown->Chat.Scope,
				.Text  = Members.Composer->toPlainText().toStdString(),
			});
		});
		connect(Members.AddContext, &QAction::triggered, this, [this]() {
			if (!Members.Shown.has_value() || !Members.Dispatch) {
				return;
			}
			const auto index = Members.ScopeSelector->currentIndex();
			if (index < 0) {
				return;
			}
			const auto target = static_cast<core::agent_chat_scope_kind>(
				Members.ScopeSelector->itemData(index).toUInt());
			if (!core::declared(target)) {
				return;
			}
			++Members.Statistics.ScopeIntents;
			// Asking to widen adds no chip here. The chip is what the core
			// agreed to send, and it arrives in the next projection.
			Members.Dispatch(widen_chat_scope_intent{
				.Session  = Members.Shown->Session.Id,
				.Widening = core::agent_scope_widening_intent{ target },
			});
		});
		connect(Members.NewSession, &QAction::triggered, this, [this]() {
			emit_continuation(core::agent_continuation::start_new_session);
		});
		connect(
			Members.ContinueOldRevision,
			&QAction::triggered,
			this,
			[this]() {
				emit_continuation(
					core::agent_continuation::continue_against_old_revision);
			});
		connect(Members.NoteOptIn, &QCheckBox::toggled, this, [this](bool) {
			emit_private_context();
		});
		connect(Members.DraftOptIn, &QCheckBox::toggled, this, [this](bool) {
			emit_private_context();
		});

		refresh_enablement();
	}

	void side_chat_panel::set_dispatch(side_chat_intent_dispatch aDispatch)
	{
		Members.Dispatch = std::move(aDispatch);
	}

	void side_chat_panel::present(const side_chat_projection& aProjection)
	{
		++Members.Statistics.Presentations;
		if (!Members.Shown || Members.Shown->Session.Id != aProjection.Session.Id || Members.Shown->Replies != aProjection.Replies) {
			const auto* item     = Members.Replies->currentItem();
			const auto  selected = Members.Shown && Members.Shown->Session.Id == aProjection.Session.Id && item ? item->data(Qt::UserRole).toULongLong() : 0U;
			const auto  blocker  = QSignalBlocker{ Members.Replies };
			Members.Replies->clear();
			for (const auto& reply : aProjection.Replies) {
				auto* row = new QListWidgetItem{ tr("Reply #%1").arg(reply.Id), Members.Replies };
				row->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(reply.Id));
				if (selected == reply.Id) {
					Members.Replies->setCurrentItem(row);
				}
			}
		}
		Members.Shown       = aProjection;
		Members.Applying    = true;
		const auto& binding = aProjection.Session.Binding;
		Members.Provider->setText(QString::fromStdString(aProjection.Provider));
		Members.GitHost->setText(QString::fromStdString(aProjection.GitHost));
		Members.Workspace->setText(
			QString::fromStdString(aProjection.Workspace));
		Members.EpochPair->setText(QStringLiteral("%1–%2")
									   .arg(binding.LeftEpoch.value())
									   .arg(binding.RightEpoch.value()));
		const auto head = core::canonical_hex(binding.HeadOid);
		Members.HeadOid->setText(text(head.view()));
		Members.SessionState->setText(
			QStringLiteral("%1 (%2)")
				.arg(text(core::stable_name(
					aProjection.Continuations.Status.Staleness)))
				.arg(
					aProjection.Continuations.Status.Current
						? QStringLiteral("current")
						: QStringLiteral("stale")));
		auto scope = text(core::stable_name(aProjection.Chat.Scope.Kind));
		if (aProjection.Chat.WidenedFrom.has_value()) {
			scope += QStringLiteral(", widened from %1")
						 .arg(text(core::stable_name(
							 aProjection.Chat.WidenedFrom->Kind)));
		}
		Members.Scope->setText(scope);
		auto chips = QString{ };
		for (const auto& chip : aProjection.Chips) {
			if (!chips.isEmpty()) {
				chips += QStringLiteral(" · ");
			}
			chips += QStringLiteral("%1: %2")
						 .arg(text(core::stable_name(chip.Scope)))
						 .arg(QString::fromStdString(chip.Label));
		}
		Members.Chips->setText(
			chips.isEmpty() ? QStringLiteral("No added context") : chips);
		Members.PrivateContext->setText(
			QStringLiteral("notes %1, drafts %2")
				.arg(
					aProjection.PrivateContext.Notes
						? QStringLiteral("included")
						: QStringLiteral("excluded"))
				.arg(
					aProjection.PrivateContext.Drafts
						? QStringLiteral("included")
						: QStringLiteral("excluded")));
		if (aProjection.Continuations.Required) {
			auto choices = QString{ };
			for (const auto choice : aProjection.Continuations.Choices) {
				if (!choices.isEmpty()) {
					choices += QStringLiteral(" or ");
				}
				choices += text(core::stable_name(choice));
			}
			Members.Continuations->setText(
				QStringLiteral("Choose: %1").arg(choices));
		} else {
			Members.Continuations->setText(QStringLiteral("No choice needed"));
		}
		Members.NoteOptIn->setChecked(aProjection.PrivateContext.Notes);
		Members.DraftOptIn->setChecked(aProjection.PrivateContext.Drafts);
		Members.Applying = false;
		refresh_enablement();
	}

	void side_chat_panel::observe(const core::agent_observation aObservation)
	{
		// Recording it is all an observation may become. There is no dispatch
		// call in this function, no chip appended by it, and no presented
		// field written by it - which is what "navigation does not silently
		// add context" means when it is a property rather than a habit.
		++Members.Statistics.Observations;
		Members.LastObservation = core::note_agent_observation(aObservation);
	}

	void side_chat_panel::emit_private_context()
	{
		if (
			Members.Applying || !Members.Shown.has_value()
			|| !Members.Dispatch) {
			return;
		}
		++Members.Statistics.PrivateContextIntents;
		Members.Dispatch(chat_private_context_intent{
			.OptIn = {
				.Session = Members.Shown->Session.Id,
				.Notes   = Members.NoteOptIn->isChecked(),
				.Drafts  = Members.DraftOptIn->isChecked(),
			},
		});
	}

	void side_chat_panel::emit_continuation(
		const core::agent_continuation aChoice)
	{
		if (!Members.Shown.has_value() || !Members.Dispatch) {
			return;
		}
		const auto& offer = Members.Shown->Continuations;
		// Only a choice this offer actually contains. The panel has no
		// continuation of its own to fall back on.
		if (std::ranges::find(offer.Choices, aChoice) == offer.Choices.end()) {
			return;
		}
		++Members.Statistics.ContinuationIntents;
		Members.Dispatch(choose_chat_continuation_intent{
			.Session        = Members.Shown->Session.Id,
			.Choice         = aChoice,
			.OfferedBinding = offer.ContinueAgainst,
		});
	}

	void side_chat_panel::refresh_enablement()
	{
		// Enablement follows the accepted projection, never a local edit.
		const auto shown    = Members.Shown.has_value();
		const auto required = shown && Members.Shown->Continuations.Required;
		// A session that stopped matching is answered before it is talked to
		// again, and answering it is an intent like everything else here.
		Members.Send->setEnabled(shown && !required);
		Members.AddContext->setEnabled(shown && !required);
		Members.ScopeSelector->setEnabled(shown && !required);
		Members.NoteOptIn->setEnabled(shown);
		Members.DraftOptIn->setEnabled(shown);
		Members.Composer->setReadOnly(!shown);
		Members.NewSession->setEnabled(required);
		Members.ContinueOldRevision->setEnabled(required);
		const auto* selected = Members.Replies->currentItem();
		auto        body     = std::string{ };
		if (Members.Shown && selected) {
			const auto found = std::ranges::find(Members.Shown->Replies, selected->data(Qt::UserRole).toULongLong(), &side_chat_reply::Id);
			if (found != Members.Shown->Replies.end()) {
				body = found->Text;
			}
		}
		const auto text = QString::fromStdString(body);
		if (Members.ReplyBody->toPlainText() != text) {
			Members.ReplyBody->setPlainText(text);
		}
		const auto hasReplies = Members.Shown && !Members.Shown->Replies.empty();
		Members.Replies->setVisible(hasReplies);
		Members.ReplyBody->setVisible(!body.empty());
		Members.Promote->setVisible(hasReplies);
		Members.Promote->setEnabled(Members.PromotionAvailable && !text.trimmed().isEmpty() && body.size() <= core::maximum_shared_draft_body_bytes);
		Members.Promote->setToolTip(!selected ? tr("Select one reply to promote.") : !Members.PromotionAvailable ? Members.PromotionReason
										: body.size() > core::maximum_shared_draft_body_bytes                    ? tr("This reply is too long for a draft.")
																												 : tr("Copy this reply to Comment's target. Keep the reply; publish nothing."));
	}
	void side_chat_panel::set_promotion_target(const bool aAvailable, const QString& aReason)
	{
		Members.PromotionAvailable = aAvailable;
		Members.PromotionReason    = aReason;
		refresh_enablement();
	}
	auto side_chat_panel::replies_list() const noexcept -> QListWidget* { return Members.Replies; }
	auto side_chat_panel::promote_action() const noexcept -> QAction* { return Members.Promote; }

	auto side_chat_panel::provider_text() const -> QString
	{
		return Members.Provider->text();
	}

	auto side_chat_panel::git_host_text() const -> QString
	{
		return Members.GitHost->text();
	}

	auto side_chat_panel::workspace_text() const -> QString
	{
		return Members.Workspace->text();
	}

	auto side_chat_panel::epoch_pair_text() const -> QString
	{
		return Members.EpochPair->text();
	}

	auto side_chat_panel::head_oid_text() const -> QString
	{
		return Members.HeadOid->text();
	}

	auto side_chat_panel::session_state_text() const -> QString
	{
		return Members.SessionState->text();
	}

	auto side_chat_panel::scope_text() const -> QString
	{
		return Members.Scope->text();
	}

	auto side_chat_panel::chips_text() const -> QString
	{
		return Members.Chips->text();
	}

	auto side_chat_panel::private_context_text() const -> QString
	{
		return Members.PrivateContext->text();
	}

	auto side_chat_panel::continuations_text() const -> QString
	{
		return Members.Continuations->text();
	}

	auto side_chat_panel::offered_continuations() const
		-> std::vector<core::agent_continuation>
	{
		if (
			!Members.Shown.has_value()
			|| !Members.Shown->Continuations.Required) {
			return { };
		}
		const auto& choices = Members.Shown->Continuations.Choices;
		return { choices.begin(), choices.end() };
	}

	auto side_chat_panel::chip_count() const noexcept -> std::size_t
	{
		return Members.Shown.has_value() ? Members.Shown->Chips.size() : 0U;
	}

	auto side_chat_panel::composer() const noexcept -> QPlainTextEdit*
	{
		return Members.Composer;
	}

	auto side_chat_panel::scope_selector() const noexcept -> QComboBox*
	{
		return Members.ScopeSelector;
	}

	auto side_chat_panel::note_optin() const noexcept -> QCheckBox*
	{
		return Members.NoteOptIn;
	}

	auto side_chat_panel::draft_optin() const noexcept -> QCheckBox*
	{
		return Members.DraftOptIn;
	}

	auto side_chat_panel::send_action() const noexcept -> QAction*
	{
		return Members.Send;
	}

	auto side_chat_panel::add_context_action() const noexcept -> QAction*
	{
		return Members.AddContext;
	}

	auto side_chat_panel::new_session_action() const noexcept -> QAction*
	{
		return Members.NewSession;
	}

	auto side_chat_panel::continue_old_revision_action() const noexcept
		-> QAction*
	{
		return Members.ContinueOldRevision;
	}

	auto side_chat_panel::last_observation() const noexcept
		-> core::agent_observation_record
	{
		return Members.LastObservation;
	}

	auto side_chat_panel::statistics() const noexcept
		-> side_chat_panel_statistics
	{
		return Members.Statistics;
	}
} // namespace bpr::qt
