#include "bpr/qt/github_conversation_panel.hpp"

#include "bpr/qt/github_conversation_delegate.hpp"
#include "bpr/qt/github_conversation_model.hpp"

#include "github_presentation_text.hpp"

#include <QAction>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <utility>

namespace bpr::qt {
	github_conversation_panel::github_conversation_panel(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("pullRequestConversationGroup"));
		setAccessibleName(QStringLiteral("Read-only GitHub conversations"));

		auto* const layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);

		AvailabilityLabel = new QLabel{ this };
		AvailabilityLabel->setObjectName(
			QStringLiteral("reviewThreadAvailability"));
		AvailabilityLabel->setAccessibleName(
			QStringLiteral("Review-thread availability"));
		AvailabilityLabel->setWordWrap(true);
		detail::set_plain_text_label(AvailabilityLabel);

		auto* const header = new QHBoxLayout;
		header->addWidget(AvailabilityLabel, 1);
		RevealButton = new QToolButton{ this };
		RevealButton->setObjectName(QStringLiteral("showReviewThreadsButton"));
		RevealButton->setAccessibleName(QStringLiteral("Show review threads"));
		RevealButton->setFocusPolicy(Qt::StrongFocus);
		RevealButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		header->addWidget(RevealButton);
		layout->addLayout(header);

		Stack = new QStackedWidget{ this };
		Stack->setObjectName(QStringLiteral("githubConversationState"));
		auto* const empty = new QWidget{ Stack };
		empty->setObjectName(QStringLiteral("githubConversationEmptyState"));
		Stack->addWidget(empty);

		Model = new github_conversation_model{ this };
		View  = new QTreeView{ Stack };
		View->setObjectName(QStringLiteral("githubConversationView"));
		View->setAccessibleName(QStringLiteral("Read-only GitHub conversations"));
		View->setAccessibleDescription(
			QStringLiteral("Reviews, review threads, and pull request comments"));
		View->setModel(Model);
		View->setSelectionBehavior(QAbstractItemView::SelectRows);
		View->setSelectionMode(QAbstractItemView::SingleSelection);
		View->setEditTriggers(QAbstractItemView::NoEditTriggers);
		View->setRootIsDecorated(true);
		View->setItemsExpandable(true);
		View->setWordWrap(true);
		// Rows are as tall as their comment, so no two are alike and none of
		// them can be assumed uniform.
		View->setUniformRowHeights(false);
		View->setTextElideMode(Qt::ElideNone);
		View->header()->hide();
		View->setItemDelegate(new github_conversation_delegate{ View });
		Stack->addWidget(View);
		layout->addWidget(Stack, 1);

		setTabOrder(RevealButton, View);
	}

	void github_conversation_panel::adopt(
		const std::vector<application::github_conversation_projection>&
				aConversations,
		QString aAvailabilityText)
	{
		// Captured before the model changes. Restoring by identity rather than
		// by row is what keeps a reviewer on the comment they were reading when
		// a reply lands above it.
		const auto selected = Model->id_for_index(View->currentIndex());

		AvailabilityLabel->setText(std::move(aAvailabilityText));
		Model->apply(aConversations);
		Stack->setCurrentIndex(aConversations.empty() ? 0 : 1);
		View->doItemsLayout();
		if (ExpandOnNextProjection && !aConversations.empty()) {
			View->expandAll();
			ExpandOnNextProjection = false;
		}
		if (!selected.empty()) {
			const auto restored = Model->index_for_id(selected);
			if (restored.isValid()) {
				// Blocked, because restoring a selection is not the reviewer
				// making one, and anything listening should not be told it was.
				const QSignalBlocker blocker{ View->selectionModel() };
				View->setCurrentIndex(restored);
			}
		}
	}

	void github_conversation_panel::clear(QString aAvailabilityText)
	{
		ExpandOnNextProjection = true;
		AvailabilityLabel->setText(std::move(aAvailabilityText));
		Model->clear();
		Stack->setCurrentIndex(0);
	}

	void github_conversation_panel::set_reveal_action(QAction* const aAction)
	{
		RevealButton->setDefaultAction(aAction);
		// setDefaultAction adopts the action's name, and the action names the
		// gesture while the button names the control. Restored so the two do
		// not answer to one accessible name between them.
		RevealButton->setAccessibleName(QStringLiteral("Show review threads"));
	}

	void github_conversation_panel::relayout()
	{
		View->doItemsLayout();
	}

	auto github_conversation_panel::view() const noexcept -> QTreeView*
	{
		return View;
	}

	auto github_conversation_panel::model() const noexcept
		-> github_conversation_model*
	{
		return Model;
	}

	auto github_conversation_panel::availability_label() const noexcept -> QLabel*
	{
		return AvailabilityLabel;
	}

	auto github_conversation_panel::reveal_button() const noexcept -> QToolButton*
	{
		return RevealButton;
	}
} // namespace bpr::qt
