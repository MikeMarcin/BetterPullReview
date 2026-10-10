#ifndef BPR_TESTS_END_TO_END_DRAWER_SINGLE_COMMENT_HPP
#define BPR_TESTS_END_TO_END_DRAWER_SINGLE_COMMENT_HPP

/// \file
/// \brief Finds and sends the drawer draft's Add single comment as a person
/// does.
///
/// \details [[BPR-1345]] v002 renamed the drawer draft's Publish to Add single
/// comment, and offers it as its own pill only where the drawer has room for
/// it beside Start a review. In a narrow drawer, which includes the default
/// 320 px one, it is the split arrow's menu item. A course that waited for a
/// visible Publish pill then timed out at "the new comment is written in the
/// drawer" (BPR-1329's and BPR-1093's courses).

#include "../support/test_expectation.hpp"
#include "bpr/qt/conversation_cards_view.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QPointer>
#include <QString>

namespace bpr::e2e {
	/// The open draft's Add single comment pill, shown or folded away.
	[[nodiscard]] inline auto draft_publish(const qt::conversation_cards_view& aCards) -> QAbstractButton*
	{
		auto* const open = aCards.open_card();
		return open == nullptr ? nullptr : open->findChild<QAbstractButton*>(QStringLiteral("threadPublishDraft"));
	}
	/// The open draft's split arrow beside Start a review.
	[[nodiscard]] inline auto draft_more(const qt::conversation_cards_view& aCards) -> QAbstractButton*
	{
		auto* const open = aCards.open_card();
		return open == nullptr ? nullptr : open->findChild<QAbstractButton*>(QStringLiteral("threadReviewDraftMore"));
	}
	/// Add single comment is offered: as its own pill, or as the split arrow's
	/// menu item.
	[[nodiscard]] inline auto single_offered(const qt::conversation_cards_view& aCards) -> bool
	{
		auto* const publish = draft_publish(aCards);
		auto* const more    = draft_more(aCards);
		return publish != nullptr && (publish->isVisible() || (more != nullptr && more->isVisible()));
	}
	/// Sends Add single comment: its pill, or the split arrow's menu item,
	/// which closes its menu before it acts.
	inline void send_single(const qt::conversation_cards_view& aCards)
	{
		auto* const publish = draft_publish(aCards);
		tests::expect(publish != nullptr, "the open draft has Add single comment");
		if (publish->isVisible()) {
			publish->click();
			return;
		}
		auto* const more = draft_more(aCards);
		tests::expect(more != nullptr && more->isVisible(), "the split arrow carries Add single comment");
		more->click();
		QApplication::processEvents();
		const QPointer menu = aCards.findChild<QMenu*>(QStringLiteral("threadReviewDraftMenu"));
		auto* const    item = menu != nullptr ? menu->findChild<QAction*>(QStringLiteral("threadReviewDraftSingle")) : nullptr;
		tests::expect(item != nullptr && item->isEnabled() && item->text() == publish->text(), "the split arrow's menu offers the draft's single comment");
		menu->hide();
		item->trigger();
		if (menu != nullptr) {
			menu->close();
		}
	}
}

#endif // BPR_TESTS_END_TO_END_DRAWER_SINGLE_COMMENT_HPP
