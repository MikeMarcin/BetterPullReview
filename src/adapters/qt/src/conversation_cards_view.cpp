#include "bpr/qt/conversation_cards_view.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include <QContextMenuEvent>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QResizeEvent>
#include <QScrollBar>
#include <QToolButton>

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {

		constexpr int card_margin     = 8;
		constexpr int card_padding    = 10;
		constexpr int card_gap        = 8;
		constexpr int avatar_size     = 18;
		constexpr int avatar_column   = 26;
		constexpr int post_gap        = 6;
		constexpr int action_height   = 18;
		constexpr int header_height   = 18;
		constexpr int path_row_height = 20;
		constexpr int excerpt_padding = 6;

		[[nodiscard]] auto has_code_anchor(const thread_card& aCard) -> bool
		{
			return !aCard.Path.isEmpty() && !aCard.Outdated && !aCard.AnchorUnavailable
				&& aCard.Side.has_value() && aCard.Line.has_value() && *aCard.Line > 0U;
		}

		[[nodiscard]] auto compact_label(const thread_card& aCard) -> QString
		{
			auto label = aCard.Draft ? QStringLiteral("Your unpublished draft") : aCard.Kind;
			if (!aCard.Posts.empty()) {
				label += QStringLiteral(" · @%1").arg(aCard.Posts.front().Author);
			}
			label += QStringLiteral("\n%1 posts · %2 unread").arg(aCard.Posts.size()).arg(aCard.UnreadCount);
			if (aCard.Resolved) {
				label += QStringLiteral(" · Resolved");
			}
			if (aCard.Outdated) {
				label += QStringLiteral(" · Outdated");
			}
			if (aCard.AnchorUnavailable) {
				label += QStringLiteral("\nCode anchor unavailable");
			}
			return label;
		}

		[[nodiscard]] auto text(const std::string& aValue) -> QString
		{
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		}

		[[nodiscard]] auto action_text(const card_action aAction) -> QString
		{
			switch (aAction) {
			case card_action::publish_draft:
				return QStringLiteral("Publish");
			case card_action::edit_draft:
				return QStringLiteral("Edit");
			case card_action::mark_read:
				return QStringLiteral("Mark read");
			case card_action::reply:
				return QStringLiteral("Reply");
			case card_action::open_in_code:
				return QStringLiteral("Open in code");
			case card_action::resolve:
				return QStringLiteral("Resolve");
			case card_action::unresolve:
				return QStringLiteral("Unresolve");
			case card_action::defect:
				return QStringLiteral("Defect actions");
			case card_action::delete_draft:
				break;
			}
			return QStringLiteral("Delete");
		}

		[[nodiscard]] auto small_font(const QFont& aFont) -> QFont
		{
			auto font = aFont;
			font.setPointSizeF(font.pointSizeF() * 0.85);
			return font;
		}

		[[nodiscard]] auto bold_font(const QFont& aFont) -> QFont
		{
			auto font = aFont;
			font.setWeight(QFont::DemiBold);
			return font;
		}

		[[nodiscard]] auto side_of(const application::github_conversation_projection& aNode)
			-> std::optional<core::diff_side>
		{
			if (!aNode.DiffSide.has_value()) {
				return std::nullopt;
			}
			switch (*aNode.DiffSide) {
			case application::github_conversation_diff_side::left:
				return core::diff_side::left;
			case application::github_conversation_diff_side::right:
				return core::diff_side::right;
			}
			return std::nullopt;
		}
	} // namespace

	conversation_cards_view::conversation_cards_view(QWidget* const aParent)
		: QAbstractScrollArea{ aParent }
		, CodeFont{ to_presentation_font(default_typography_settings().Code) }
	{
		setObjectName(QStringLiteral("conversationCards"));
		setAccessibleName(QStringLiteral("Comment cards"));
		setAccessibleDescription(QStringLiteral(
			"This file's threads as cards at their lines; Up and Down move "
			"between them, Return reveals the line; Shift+F10 opens its thread actions"));
		setFocusPolicy(Qt::StrongFocus);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		viewport()->setAutoFillBackground(true);
		viewport()->setBackgroundRole(QPalette::Window);
		connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
			sync_badge_buttons();
			viewport()->update();
		});
	}

	void conversation_cards_view::set_context_menu_builder(std::function<void(QMenu&, const std::string&)> aBuilder)
	{
		ContextMenuBuilder = std::move(aBuilder);
	}

	void conversation_cards_view::set_defect_badges(defect_badges aBadges)
	{
		if (DefectBadges == aBadges) {
			return;
		}
		DefectBadges = std::move(aBadges);
		relayout();
	}

	auto conversation_cards_view::thread_menu(const std::string& aIdentity) -> QMenu*
	{
		const auto at = std::ranges::find(Cards, aIdentity, &thread_card::Id);
		if (at == Cards.end() || at->Draft) {
			return nullptr;
		}
		// Selection can rebuild Cards; retain this exact card across the dispatch.
		const auto card = *at; // NOLINT(performance-unnecessary-copy-initialization)
		choose(static_cast<std::size_t>(std::distance(Cards.begin(), at)), false);
		auto* menu = new QMenu{ this };
		menu->setObjectName(QStringLiteral("threadContextMenu"));
		menu->setToolTipsVisible(true);
		menu->setAttribute(Qt::WA_DeleteOnClose);
		const auto add = [this, menu, identity = card.Id](card_action aKind, bool aEnabled, const QString& aReason) {
			auto* action = menu->addAction(action_text(aKind));
			action->setEnabled(aEnabled && static_cast<bool>(ActionDispatch));
			action->setToolTip(aEnabled ? action->text() : aReason);
			connect(action, &QAction::triggered, menu, [this, identity, aKind] { if (ActionDispatch) { ActionDispatch(aKind, identity); } });
		};
		add(card_action::reply, has_code_anchor(card), tr("This thread has no current code anchor. Open its current comparison before replying."));
		add(card_action::mark_read, card.UnreadCount > 0U, tr("This thread has no unread posts."));
		add(card.Resolved ? card_action::unresolve : card_action::resolve, ResolutionOffered && card.Kind == QStringLiteral("Inline thread"), tr("Thread resolution is not available for this conversation."));
		add(card_action::open_in_code, has_code_anchor(card), tr("This thread's code anchor is unavailable."));
		if (ContextMenuBuilder) {
			menu->addSeparator();
			ContextMenuBuilder(*menu, card.Id);
		}
		return menu;
	}

	void conversation_cards_view::show_thread_menu(const std::string& aIdentity, QPoint aGlobalPosition)
	{
		if (auto* menu = thread_menu(aIdentity)) {
			menu->popup(aGlobalPosition.isNull() ? viewport()->mapToGlobal(viewport()->rect().center()) : aGlobalPosition);
		}
	}

	void conversation_cards_view::contextMenuEvent(QContextMenuEvent* aEvent)
	{
		if (aEvent->reason() == QContextMenuEvent::Keyboard) {
			show_thread_menu(Current);
		} else if (const auto hit = hit_test(aEvent->pos())) {
			const auto identity = Cards[hit->first].Id;
			show_thread_menu(identity, aEvent->globalPos());
		}
		aEvent->accept();
	}

	void conversation_cards_view::sync_badge_buttons()
	{
		auto used = std::size_t{ 0 };
		for (const auto& card : Cards) {
			if (card.DefectBadge.isEmpty()) {
				continue;
			}
			const auto badge = std::ranges::find(card.Actions, card_action::defect, &std::pair<card_action, QRect>::first);
			if (badge == card.Actions.end()) {
				continue;
			}
			const auto rect = badge->second.translated(0, -verticalScrollBar()->value());
			if (!rect.intersects(viewport()->rect())) {
				continue;
			}
			if (used == BadgeButtons.size()) {
				auto* button = new QToolButton{ viewport() };
				button->setObjectName(QStringLiteral("threadDefectBadge"));
				button->setFocusPolicy(Qt::StrongFocus);
				connect(button, &QToolButton::clicked, this, [this, button] {
					show_thread_menu(button->property("threadIdentity").toString().toStdString(), button->mapToGlobal(QPoint{ 0, button->height() }));
				});
				BadgeButtons.push_back(button);
			}
			auto* button = BadgeButtons[used++];
			button->setProperty("threadIdentity", QString::fromStdString(card.Id));
			button->setText(button->fontMetrics().elidedText(card.DefectBadge, Qt::ElideRight, std::max(1, rect.width() - 18)));
			button->setAccessibleName(card.DefectBadge);
			button->setAccessibleDescription(card.DefectDescription);
			button->setToolTip(card.DefectDescription);
			button->setGeometry(rect);
			button->show();
		}
		for (; used < BadgeButtons.size(); ++used) {
			BadgeButtons[used]->hide();
		}
	}

	void conversation_cards_view::adopt(
		const std::vector<application::github_conversation_projection>& aConversations)
	{
		adopt(aConversations, AnchorFile);
	}

	void conversation_cards_view::adopt(
		const std::vector<application::github_conversation_projection>& aConversations,
		std::optional<core::file_identity>                              aFile)
	{
		if (Retained == aConversations && AnchorFile == aFile) {
			return;
		}
		if (Retained != aConversations) {
			// A changed snapshot need not change any text. Retain rectangles only
			// for still-present roots; each use validates exact measurement inputs.
			auto roots = std::set<std::string>{ };
			for (const auto& node : aConversations) {
				if (node.Kind == application::github_conversation_kind::review_thread && !node.ParentId.has_value()) {
					roots.insert(node.Id);
				}
			}
			forget_measurements_if([&roots, this](const std::string& aId) {
				return !roots.contains(aId) && !(aId.empty() && Draft.has_value());
			});
			Retained = aConversations;
			index_retained();
		}
		AnchorFile = std::move(aFile);
		relayout();
	}

	void conversation_cards_view::clear()
	{
		clear_measurements();
		Retained.clear();
		index_retained();
		Draft.reset();
		Current.clear();
		ReadState.clear();
		ReadStateByThread.clear();
		ReadStateKnown = false;
		UnreadThreads.reset();
		relayout();
	}

	void conversation_cards_view::index_retained()
	{
		UnreadThreads.reset();
		RetainedFiles.clear();
		RetainedById.clear();
		RetainedRoot.clear();
		RetainedDepth.clear();
		RetainedByFile.clear();
		RetainedFiles.reserve(Retained.size());
		RetainedById.reserve(Retained.size());
		for (std::size_t index = 0U; index < Retained.size(); ++index) {
			RetainedFiles.push_back(core::file_identity_of(Retained[index].Path));
			// The first node with an identity answers for it.
			RetainedById.emplace(Retained[index].Id, index);
		}
		// A root is the topmost retained ancestor: a parent that is not
		// retained makes its child a root, and the walk is bounded by the
		// node count so no cycle can spin.
		RetainedRoot.reserve(Retained.size());
		RetainedDepth.reserve(Retained.size());
		RetainedByFile.reserve(Retained.size());
		for (std::size_t index = 0U; index < Retained.size(); ++index) {
			auto current = index;
			auto depth   = std::size_t{ 0U };
			while (Retained[current].ParentId.has_value() && depth < RetainedById.size()) {
				const auto parent = RetainedById.find(*Retained[current].ParentId);
				if (parent == RetainedById.end()) {
					break;
				}
				current = parent->second;
				++depth;
			}
			RetainedRoot.push_back(current);
			RetainedDepth.push_back(depth);
			RetainedByFile.emplace_back(RetainedFiles[current], index);
		}
		std::ranges::sort(RetainedByFile);
	}

	void conversation_cards_view::set_scope(const cards_scope aScope)
	{
		if (Scope == aScope) {
			return;
		}
		Scope = aScope;
		relayout();
	}

	auto conversation_cards_view::scope() const noexcept -> cards_scope
	{
		return Scope;
	}

	void conversation_cards_view::set_anchor_file(std::optional<core::file_identity> aFile)
	{
		if (AnchorFile == aFile) {
			return;
		}
		AnchorFile = std::move(aFile);
		relayout();
	}

	void conversation_cards_view::set_line_locator(card_line_locator aLocator)
	{
		Locator = std::move(aLocator);
		relayout();
	}

	void conversation_cards_view::set_draft(
		std::optional<core::review_comment_draft>   aDraft,
		std::optional<core::file_comparison_digest> aCurrentComparison)
	{
		if (Draft == aDraft && CurrentComparison == aCurrentComparison) {
			return;
		}
		if (!aDraft.has_value()) {
			forget_measurements_if([](const std::string& aId) { return aId.empty(); });
		}
		Draft             = std::move(aDraft);
		CurrentComparison = std::move(aCurrentComparison);
		relayout();
	}

	void conversation_cards_view::set_viewer_login(const QString& aLogin)
	{
		if (Viewer == aLogin) {
			return;
		}
		Viewer = aLogin;
		relayout();
	}

	void conversation_cards_view::set_thread_dispatch(card_thread_dispatch aDispatch)
	{
		ThreadDispatch = std::move(aDispatch);
	}

	void conversation_cards_view::set_action_dispatch(card_action_dispatch aDispatch)
	{
		ActionDispatch = std::move(aDispatch);
	}

	void conversation_cards_view::set_feed(const bool aFeed)
	{
		if (Feed == aFeed) {
			return;
		}
		Feed = aFeed;
		relayout();
	}

	void conversation_cards_view::set_compact(const bool aCompact)
	{
		if (Compact == aCompact) {
			return;
		}
		Compact = aCompact;
		clear_measurements();
		relayout();
	}

	auto conversation_cards_view::feed() const noexcept -> bool
	{
		return Feed;
	}

	void conversation_cards_view::set_filters(const discuss_filters aFilters)
	{
		if (Filters == aFilters) {
			return;
		}
		Filters = aFilters;
		relayout();
	}

	auto conversation_cards_view::filters() const noexcept -> discuss_filters
	{
		return Filters;
	}

	void conversation_cards_view::set_resolution_offered(const bool aOffered)
	{
		if (ResolutionOffered == aOffered) {
			return;
		}
		ResolutionOffered = aOffered;
		relayout();
	}

	auto conversation_cards_view::resolution_offered() const noexcept -> bool
	{
		return ResolutionOffered;
	}

	void conversation_cards_view::set_pull_request_author(const QString& aLogin)
	{
		if (PullRequestAuthor == aLogin) {
			return;
		}
		PullRequestAuthor = aLogin;
		relayout();
	}

	void conversation_cards_view::set_excerpt_locator(card_excerpt_locator aLocator)
	{
		ExcerptLocator = std::move(aLocator);
		relayout();
	}

	void conversation_cards_view::set_layout_observer(std::function<void()> aObserver)
	{
		LayoutObserver = std::move(aObserver);
	}

	auto conversation_cards_view::thread_count() const noexcept -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (const auto& card : Cards) {
			count += card.Draft ? 0U : 1U;
		}
		return count;
	}

	auto conversation_cards_view::unresolved_count() const noexcept -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (const auto& card : Cards) {
			count += !card.Draft && !card.Resolved && (!Compact || card.Kind == QStringLiteral("Inline thread")) ? 1U : 0U;
		}
		return count;
	}

	auto conversation_cards_view::unread_thread_count() const noexcept -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (const auto& card : Cards) {
			count += card.UnreadCount > 0U ? 1U : 0U;
		}
		return count;
	}

	void conversation_cards_view::set_code_font(const QFont& aFont)
	{
		if (CodeFont == aFont) {
			return;
		}
		CodeFont = aFont;
		clear_measurements();
		relayout();
	}

	auto conversation_cards_view::code_font() const -> const QFont&
	{
		return CodeFont;
	}

	void conversation_cards_view::set_code_colours(const QColor& aBackground, const QColor& aInk)
	{
		CodeBackground = aBackground;
		CodeInk        = aInk;
		viewport()->update();
	}

	void conversation_cards_view::set_read_state(std::vector<thread_read_state> aState)
	{
		if (ReadStateKnown && ReadState == aState) {
			return;
		}
		ReadState      = std::move(aState);
		ReadStateKnown = true;
		ReadStateByThread.clear();
		ReadStateByThread.reserve(ReadState.size());
		for (std::size_t index = 0U; index < ReadState.size(); ++index) {
			// The first state recorded for a thread answers for it.
			ReadStateByThread.emplace(ReadState[index].Thread, index);
		}
		UnreadThreads.reset();
		relayout();
	}

	auto conversation_cards_view::set_thread_read_state(const std::string& aThread, std::vector<std::string> aUnreadPosts)
		-> std::vector<std::string>
	{
		auto previous = std::vector<std::string>{ };
		if (const auto found = ReadStateByThread.find(aThread); found != ReadStateByThread.end()) {
			previous = ReadState[found->second].UnreadPosts;
			if (ReadStateKnown && previous == aUnreadPosts) {
				return previous;
			}
			ReadState[found->second].UnreadPosts = std::move(aUnreadPosts);
		} else {
			if (ReadStateKnown && aUnreadPosts.empty()) {
				return previous;
			}
			ReadState.push_back(thread_read_state{ .Thread = aThread, .UnreadPosts = std::move(aUnreadPosts) });
			ReadStateByThread.emplace(aThread, ReadState.size() - 1U);
		}
		ReadStateKnown     = true;
		const auto& now    = ReadState[ReadStateByThread.at(aThread)].UnreadPosts;
		const auto  wasNew = !previous.empty();
		if (UnreadThreads.has_value() && wasNew && now.empty()) {
			std::erase(*UnreadThreads, aThread);
		} else if (wasNew != !now.empty()) {
			// A thread becoming unread takes its place in file order when the
			// list is next asked for.
			UnreadThreads.reset();
		}
		if (std::ranges::any_of(Cards, [&aThread](const thread_card& aCard) { return aCard.Id == aThread; })) {
			relayout();
		}
		return previous;
	}

	auto conversation_cards_view::unread_threads_in_file_order() const -> const std::vector<std::string>&
	{
		if (UnreadThreads.has_value()) {
			return *UnreadThreads;
		}
		// Every thread of the pull request, whatever the scope shows, so the
		// strip's next new comment walks the whole review in file order.
		auto threads = std::vector<const application::github_conversation_projection*>{ };
		for (const auto& node : Retained) {
			if (node.Kind != application::github_conversation_kind::review_thread
				|| node.ParentId.has_value()) {
				continue;
			}
			const auto state = ReadStateByThread.find(node.Id);
			if (state != ReadStateByThread.end() && !ReadState[state->second].UnreadPosts.empty()) {
				threads.push_back(&node);
			}
		}
		std::ranges::stable_sort(threads, [](const auto* aLeft, const auto* aRight) {
			return std::pair{ aLeft->Path, aLeft->Line.value_or(0U) }
			< std::pair{ aRight->Path, aRight->Line.value_or(0U) };
		});
		auto ids = std::vector<std::string>{ };
		ids.reserve(threads.size());
		for (const auto* thread : threads) {
			ids.push_back(thread->Id);
		}
		UnreadThreads = std::move(ids);
		return *UnreadThreads;
	}

	auto conversation_cards_view::unread_marked_lines(const core::diff_side aSide) const
		-> std::vector<std::uint32_t>
	{
		auto lines = std::vector<std::uint32_t>{ };
		for (const auto& card : Cards) {
			if (!card.Draft && card.RootUnread && card.Side == aSide && card.Line.has_value()
				&& *card.Line > 0U) {
				lines.push_back(*card.Line - 1U);
			}
		}
		std::ranges::sort(lines);
		lines.erase(std::ranges::unique(lines).begin(), lines.end());
		return lines;
	}

	auto conversation_cards_view::unread_post_count() const noexcept -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (const auto& card : Cards) {
			count += card.UnreadCount;
		}
		return count;
	}

	void conversation_cards_view::set_current_thread(const std::string& aId)
	{
		// A reply's identity names its card: the tree may be on a reply.
		Current = aId;
		for (const auto& card : Cards) {
			if (card.Draft) {
				continue;
			}
			if (card.Id == aId) {
				break;
			}
			if (std::ranges::any_of(card.Posts, [&aId](const card_post& aPost) { return aPost.Id == aId; })) {
				Current = card.Id;
				break;
			}
		}
		viewport()->update();
	}

	void conversation_cards_view::set_colours(const QColor& aAccent, const QColor& aMarker)
	{
		Accent = aAccent;
		Marker = aMarker;
		viewport()->update();
	}

	auto conversation_cards_view::cards() const noexcept -> const std::vector<thread_card>&
	{
		return Cards;
	}

	auto conversation_cards_view::current_thread() const noexcept -> const std::string&
	{
		return Current;
	}

	auto conversation_cards_view::layout_nodes_visited() const noexcept -> std::uint64_t
	{
		return LayoutNodesVisited;
	}

	auto conversation_cards_view::layouts() const noexcept -> std::uint64_t
	{
		return Layouts;
	}

	auto conversation_cards_view::post_count() const noexcept -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (const auto& card : Cards) {
			count += card.Draft ? 0U : card.Posts.size();
		}
		return count;
	}

	auto conversation_cards_view::marked_lines(const core::diff_side aSide) const
		-> std::vector<std::uint32_t>
	{
		auto lines = std::vector<std::uint32_t>{ };
		for (const auto& card : Cards) {
			if (!card.Draft && card.Side == aSide && card.Line.has_value() && *card.Line > 0U) {
				lines.push_back(*card.Line - 1U);
			}
		}
		std::ranges::sort(lines);
		lines.erase(std::ranges::unique(lines).begin(), lines.end());
		return lines;
	}

	auto conversation_cards_view::to_content(const QPoint aViewportPoint) const -> QPoint
	{
		return { aViewportPoint.x(), aViewportPoint.y() + verticalScrollBar()->value() };
	}

	auto conversation_cards_view::hit_test(const QPoint aViewportPoint) const
		-> std::optional<std::pair<std::size_t, std::optional<card_action>>>
	{
		const auto point = to_content(aViewportPoint);
		for (std::size_t index = 0U; index < Cards.size(); ++index) {
			const auto& card = Cards[index];
			if (!card.Rect.contains(point)) {
				continue;
			}
			for (const auto& [action, rect] : card.Actions) {
				if (rect.contains(point)) {
					return std::pair{ index, std::optional{ action } };
				}
			}
			return std::pair{ index, std::optional<card_action>{ } };
		}
		return std::nullopt;
	}

	void conversation_cards_view::measure(thread_card& aCard, const int aWidth) const
	{
		const auto metrics   = QFontMetrics{ font() };
		const auto bodyWidth = std::max(40, aWidth - 2 * card_padding - avatar_column);
		auto       y         = card_padding;
		if (Compact) {
			y += metrics.boundingRect(QRect{ 0, 0, std::max(1, aWidth - 2 * card_padding), 100'000 }, Qt::TextWordWrap | Qt::TextWrapAnywhere, compact_label(aCard)).height() + post_gap;
		}
		if (!Compact && (aCard.Resolved || aCard.Outdated || aCard.Draft)) {
			y += header_height;
		}
		// [[BPR-0649]]. The code at the thread's line, one line each side,
		// in the code font on the code background.
		if (!Compact && Feed && !aCard.Excerpt.isEmpty()) {
			const auto lines = static_cast<int>(aCard.Excerpt.count(QLatin1Char{ '\n' })) + 1;
			y += lines * QFontMetrics{ CodeFont }.height() + 2 * excerpt_padding + post_gap;
		}
		for (auto& post : aCard.Posts) {
			if (Compact) {
				post.Rect = { };
				continue;
			}
			// In the feed the header line carries role and "replying to"
			// beside the author; the small font keeps it on one line.
			const auto authorHeight = metrics.height();
			const auto bodyHeight   = post.Body.isEmpty()
				? 0
				: metrics.boundingRect(QRect{ 0, 0, bodyWidth, 100'000 }, Qt::TextWordWrap, post.Body).height();
			const auto height       = std::max(avatar_size, authorHeight + bodyHeight + 2);
			post.Rect               = QRect{ card_padding, y, aWidth - 2 * card_padding, height };
			y += height + post_gap;
		}
		aCard.Actions.clear();
		auto actionX = card_padding + avatar_column;
		auto actions = aCard.Draft
			? std::vector{ card_action::publish_draft, card_action::edit_draft, card_action::delete_draft }
			: std::vector{ card_action::reply };
		if (!aCard.Draft && Compact && !has_code_anchor(aCard)) {
			actions.clear();
		}
		if (!aCard.Draft && aCard.UnreadCount > 0U) {
			actions.push_back(card_action::mark_read);
		}
		// [[BPR-0657]]. Resolve, or Unresolve on a resolved thread, once the
		// provider and the server both resolve threads.
		if (!aCard.Draft && ResolutionOffered && (!Compact || aCard.Kind == QStringLiteral("Inline thread"))) {
			actions.push_back(aCard.Resolved ? card_action::unresolve : card_action::resolve);
		}
		if (!aCard.Draft && Feed && (!Compact || has_code_anchor(aCard))) {
			actions.push_back(card_action::open_in_code);
		}
		const auto actionMetrics = QFontMetrics{ small_font(font()) };
		const auto actionRight   = aWidth - card_padding;
		for (const auto action : actions) {
			const auto width = actionMetrics.horizontalAdvance(action_text(action)) + 4;
			// A second action that does not fit beside the first starts a
			// new row inside the card rather than leaving it.
			if (actionX + width > actionRight && actionX > card_padding + avatar_column) {
				actionX = card_padding + avatar_column;
				y += action_height + 2;
			}
			aCard.Actions.emplace_back(action, QRect{ actionX, y, width, action_height });
			actionX += width + 12;
		}
		y += action_height + card_padding;
		if (!aCard.DefectBadge.isEmpty()) {
			aCard.Actions.emplace_back(card_action::defect, QRect{ card_padding, y, aWidth - 2 * card_padding, 28 });
			y += 28 + card_padding;
		}
		aCard.Rect.setHeight(y);
	}

	auto conversation_cards_view::card_height(const thread_card& aCard, const int aWidth) const -> int
	{
		auto copy = aCard;
		measure(copy, aWidth);
		return copy.Rect.height();
	}

	auto conversation_cards_view::measurements() const noexcept -> std::uint64_t
	{
		return MeasurementCount;
	}

	void conversation_cards_view::clear_measurements()
	{
		Measurements         = { };
		PreviousMeasurements = { };
	}

	void conversation_cards_view::forget_measurements_if(
		const std::function<bool(const std::string&)>& aForget)
	{
		for (auto* const generation : { &Measurements, &PreviousMeasurements }) {
			std::erase_if(generation->Cards, [&aForget](const auto& aEntry) { return aForget(aEntry.first); });
		}
	}

	void conversation_cards_view::measure_cached(thread_card& aCard, const int aWidth, compact_measurements* aCompact)
	{
		if (MeasurementFont != font() || MeasurementFeed != Feed
			|| MeasurementResolutionOffered != ResolutionOffered) {
			clear_measurements();
			MeasurementFont              = font();
			MeasurementFeed              = Feed;
			MeasurementResolutionOffered = ResolutionOffered;
		}
		if (Measurements.Width != aWidth) {
			// The other width is kept rather than discarded, so a layout that
			// returns to it finds its cards already measured.
			std::swap(Measurements, PreviousMeasurements);
			if (Measurements.Width != aWidth) {
				Measurements = measurement_generation{ .Width = aWidth };
			}
		}
		const auto cached = Measurements.Cards.find(aCard.Id);
		const auto header = aCard.Resolved || aCard.Outdated || aCard.Draft;
		const auto label  = (Compact ? compact_label(aCard) : QString{ }) + QChar{ 0x1f } + aCard.DefectBadge;
		if (cached != Measurements.Cards.end() && cached->second.Excerpt == aCard.Excerpt
			&& cached->second.Label == label
			&& cached->second.AnchorAvailable == has_code_anchor(aCard)
			&& cached->second.Header == header && cached->second.Draft == aCard.Draft
			&& cached->second.Resolved == aCard.Resolved && cached->second.Unread == (aCard.UnreadCount > 0U)
			&& std::ranges::equal(cached->second.Bodies, aCard.Posts, { }, std::identity{ }, &card_post::Body)) {
			aCard.Rect.setHeight(cached->second.Height);
			for (std::size_t index = 0U; index < aCard.Posts.size(); ++index) {
				aCard.Posts[index].Rect = cached->second.Posts[index];
			}
			aCard.Actions = cached->second.Actions;
			return;
		}
		// During one layout font, width and action policy are fixed. Compact
		// geometry depends on its complete label and action flags; the body is
		// hidden. Keep source, identity and unread facts on each card. Include
		// flags explicitly: provider-authored text can mimic a label's suffix.
		if (Compact && aCompact) {
			const auto [found, inserted] = aCompact->try_emplace({ label, has_code_anchor(aCard), aCard.Draft,
																   aCard.Resolved, aCard.UnreadCount > 0U, aCard.Kind == QStringLiteral("Inline thread") });
			if (inserted) {
				measure(aCard, aWidth);
				found->second = { aCard.Rect.height(), aCard.Actions };
			} else {
				aCard.Rect.setHeight(found->second.Height);
				aCard.Actions = found->second.Actions;
				for (auto& post : aCard.Posts) {
					post.Rect = { };
				}
			}
		} else {
			measure(aCard, aWidth);
		}
		++MeasurementCount;
		auto measured = measured_card{
			.Label           = label,
			.Excerpt         = aCard.Excerpt,
			.Header          = header,
			.Draft           = aCard.Draft,
			.Resolved        = aCard.Resolved,
			.Unread          = aCard.UnreadCount > 0U,
			.AnchorAvailable = has_code_anchor(aCard),
			.Height          = aCard.Rect.height(),
			.Actions         = aCard.Actions,
		};
		for (const auto& post : aCard.Posts) {
			measured.Bodies.push_back(post.Body);
			measured.Posts.push_back(post.Rect);
		}
		Measurements.Cards.insert_or_assign(aCard.Id, std::move(measured));
	}

	void conversation_cards_view::relayout()
	{
		++Layouts;
		Cards.clear();
		// [[BPR-0972]]. Scoped to one file, only that file's conversations are
		// visited: the per-forest index names them, in retained order.
		auto candidates = std::vector<std::size_t>{ };
		if (Scope == cards_scope::this_file && AnchorFile.has_value()) {
			for (const auto& [file, index] : std::ranges::equal_range(RetainedByFile, *AnchorFile, { }, &std::pair<core::file_identity, std::size_t>::first)) {
				candidates.push_back(index);
			}
		} else {
			candidates.resize(Retained.size());
			for (std::size_t index = 0U; index < Retained.size(); ++index) {
				candidates[index] = index;
			}
		}
		LayoutNodesVisited += candidates.size();
		// Compact navigation admits every top-level conversation kind;
		// the legacy full-card mode admits only anchored review threads.
		auto cardsById = std::unordered_map<std::string_view, std::size_t>{ };
		for (const auto index : candidates) {
			const auto& node = Retained[index];
			if ((!Compact && node.Kind != application::github_conversation_kind::review_thread)
				|| (node.ParentId.has_value() && (!Compact || RetainedById.contains(*node.ParentId)))) {
				continue;
			}
			if (Scope == cards_scope::this_file && AnchorFile.has_value()
				&& RetainedFiles[index] != *AnchorFile) {
				continue;
			}
			auto card = thread_card{ };
			card.Id   = node.Id;
			if (const auto badge = DefectBadges.find(card.Id); badge != DefectBadges.end()) {
				card.DefectBadge       = badge->second.first;
				card.DefectDescription = badge->second.second;
			}
			card.Kind              = node.Kind == application::github_conversation_kind::review_thread
				? QStringLiteral("Inline thread")
				: node.Kind == application::github_conversation_kind::review
				? QStringLiteral("Review summary")
				: node.Kind == application::github_conversation_kind::review_comment
				? QStringLiteral("Inline comment")
				: QStringLiteral("PR comment");
			card.Path              = text(node.Path);
			card.Side              = side_of(node);
			card.Line              = node.Line;
			card.Resolved          = node.Resolved;
			card.AnchorUnavailable = !node.Path.empty() && node.AnchorAvailability != application::github_conversation_anchor_availability::current;
			card.Outdated          = node.Outdated || (!Compact && card.AnchorUnavailable);
			cardsById.emplace(node.Id, Cards.size());
			Cards.push_back(std::move(card));
		}
		// The unread posts of the shown threads' first recorded states, so a
		// post is judged by hash rather than a walk of its thread's list.
		auto unreadPosts = std::unordered_map<std::string_view, std::set<std::string_view>>{ };
		for (const auto& card : Cards) {
			if (const auto state = ReadStateByThread.find(card.Id); state != ReadStateByThread.end()) {
				auto& posts = unreadPosts[card.Id];
				for (const auto& post : ReadState[state->second].UnreadPosts) {
					posts.insert(post);
				}
			}
		}
		for (const auto index : candidates) {
			const auto& node = Retained[index];
			if (node.Kind == application::github_conversation_kind::review_thread
				|| (!Compact && !node.ParentId.has_value()) || node.Deleted) {
				continue;
			}
			const auto& root  = Retained[RetainedRoot[index]];
			const auto  depth = RetainedDepth[index];
			const auto  card  = cardsById.find(root.Id);
			if (card == cardsById.end()) {
				continue;
			}
			const auto author = text(node.AuthorLogin);
			auto&      target = Cards[card->second];
			const auto state  = unreadPosts.find(root.Id);
			const auto unread = state != unreadPosts.end() && state->second.contains(node.Id);
			if (unread) {
				if (target.Posts.empty()) {
					target.RootUnread = true;
				}
				++target.UnreadCount;
			}
			// [[BPR-0649]]. The feed's header: the pull request's author is
			// "author", anyone else "reviewer" (the projection carries no
			// finer role); a reply to a reply names whom it answers.
			auto replyingTo = QString{ };
			if (Feed && node.ParentId.has_value() && *node.ParentId != root.Id) {
				if (const auto parent = RetainedById.find(*node.ParentId); parent != RetainedById.end()) {
					replyingTo = text(Retained[parent->second].AuthorLogin);
				}
			}
			target.Posts.push_back(card_post{
				.Id         = node.Id,
				.Author     = author,
				.Body       = text(node.ReviewerProse.value_or(node.Body)),
				.Depth      = depth,
				.Mine       = !Viewer.isEmpty() && author == Viewer,
				.Unread     = unread,
				.Role       = !Feed ? QString{ }
					: !PullRequestAuthor.isEmpty() && author == PullRequestAuthor
					? QStringLiteral("author")
					: QStringLiteral("reviewer"),
				.ReplyingTo = replyingTo,
			});
		}
		// [[BPR-0649]]. Discuss's filters: only threads with unread posts,
		// only unresolved threads. The draft card is never filtered out.
		if (Filters.UnreadOnly || Filters.UnresolvedOnly) {
			std::erase_if(Cards, [this](const thread_card& aCard) {
				return !aCard.Draft
					&& ((Filters.UnreadOnly && aCard.UnreadCount == 0U)
						|| (Filters.UnresolvedOnly && (aCard.Resolved || (Compact && aCard.Kind != QStringLiteral("Inline thread")))));
			});
		}
		if (Feed && ExcerptLocator) {
			for (auto& card : Cards) {
				if (!card.Draft && card.Side.has_value() && card.Line.has_value() && *card.Line > 0U) {
					card.Excerpt = ExcerptLocator(card.Path, *card.Side, *card.Line).value_or(QString{ });
				}
			}
		}
		// My draft, at its line, while it is on this comparison.
		if (Scope == cards_scope::this_file && Draft.has_value() && CurrentComparison.has_value()
			&& Draft->Anchor.Comparison == *CurrentComparison && !Draft->shared()) {
			auto card  = thread_card{ };
			card.Draft = true;
			card.Side  = Draft->Anchor.Side;
			card.Line  = Draft->Anchor.Line + 1U;
			card.Posts.push_back(card_post{
				.Author = QStringLiteral("Your note, not published"),
				.Body   = text(Draft->Body),
				.Mine   = true,
			});
			Cards.push_back(std::move(card));
		}

		auto       compactMeasurements = compact_measurements{ };
		const auto width               = std::max(120, viewport()->width());
		auto       order               = std::size_t{ 0U };
		for (auto& card : Cards) {
			card.ProjectionOrder = order++;
			card.Rect            = QRect{ card_margin, 0, width - 2 * card_margin, 0 };
			measure_cached(card, width - 2 * card_margin, &compactMeasurements);
			for (auto& post : card.Posts) {
				post.Rect.translate(card.Rect.left(), 0);
			}
			for (auto& [action, rect] : card.Actions) {
				rect.translate(card.Rect.left(), 0);
			}
		}
		place_cards(true);
		if (LayoutObserver) {
			LayoutObserver();
		}
		setAccessibleDescription((Compact
									  ? QStringLiteral("%1 conversations, %2 posts, %3 unread; Up and Down move between conversations, Return opens the selected detail; Shift+F10 opens its thread actions")
									  : QStringLiteral("%1 threads as cards, %2 posts, %3 unread; Up and Down move between them, Return reveals the line; Shift+F10 opens its thread actions"))
									 .arg(thread_count())
									 .arg(post_count())
									 .arg(unread_post_count()));
	}

	void conversation_cards_view::refresh_geometry()
	{
		// Comparison frames can change line positions and feed excerpts without
		// changing a single conversation. Keep their forest and text measurements.
		auto changed = false;
		if (Feed && ExcerptLocator) {
			for (auto& card : Cards) {
				if (card.Draft || !card.Side.has_value() || !card.Line.has_value() || *card.Line == 0U) {
					continue;
				}
				auto excerpt = ExcerptLocator(card.Path, *card.Side, *card.Line).value_or(QString{ });
				if (card.Excerpt == excerpt) {
					continue;
				}
				changed      = true;
				card.Excerpt = std::move(excerpt);
				measure_cached(card, card.Rect.width());
				for (auto& post : card.Posts) {
					post.Rect.translate(card.Rect.topLeft());
				}
				for (auto& [action, rect] : card.Actions) {
					rect.translate(card.Rect.topLeft());
				}
			}
		}
		place_cards(changed);
	}

	void conversation_cards_view::place_cards(const bool aForce)
	{
		const auto scroll  = verticalScrollBar()->value();
		auto       changed = aForce;
		for (auto& card : Cards) {
			const auto wasPlaced = card.Placed;
			const auto targetY   = card.TargetY;
			card.Placed          = false;
			// The feed is a list in file order, never placed at lines.
			if (!Feed && Scope == cards_scope::this_file && Locator && card.Side.has_value()
				&& card.Line.has_value() && *card.Line > 0U && !card.Outdated) {
				if (const auto y = Locator(*card.Side, *card.Line); y.has_value()) {
					card.Placed  = true;
					card.TargetY = *y + scroll;
				}
			}
			changed = changed || wasPlaced != card.Placed || (card.Placed && targetY != card.TargetY);
		}
		if (!changed) {
			return;
		}
		// Placed cards in line order, then the rest; whole PR in file order.
		std::ranges::stable_sort(Cards, [this](const thread_card& aLeft, const thread_card& aRight) {
			if (Feed || Scope == cards_scope::whole_pull_request) {
				return std::tuple{ aLeft.Path, aLeft.Line.value_or(0U), aLeft.ProjectionOrder }
				< std::tuple{ aRight.Path, aRight.Line.value_or(0U), aRight.ProjectionOrder };
			}
			if (aLeft.Placed != aRight.Placed) {
				return aLeft.Placed;
			}
			if (aLeft.Placed) {
				return std::pair{ aLeft.TargetY, aLeft.ProjectionOrder }
				< std::pair{ aRight.TargetY, aRight.ProjectionOrder };
			}
			return std::tuple{ aLeft.Path, aLeft.Line.value_or(0U), aLeft.ProjectionOrder }
			< std::tuple{ aRight.Path, aRight.Line.value_or(0U), aRight.ProjectionOrder };
		});
		auto bottom   = card_margin / 2;
		auto lastPath = QString{ };
		for (auto& card : Cards) {
			if (Scope == cards_scope::whole_pull_request && card.Path != lastPath) {
				bottom += path_row_height;
				lastPath = card.Path;
			}
			auto top = bottom + (bottom == card_margin / 2 ? 0 : card_gap);
			if (card.Placed) {
				// At its line, or beneath the card above: never over it.
				top = std::max(top, card.TargetY);
			}
			const auto offset = top - card.Rect.top();
			card.Rect.moveTop(top);
			for (auto& post : card.Posts) {
				post.Rect.translate(0, offset);
			}
			for (auto& [action, rect] : card.Actions) {
				rect.translate(0, offset);
			}
			bottom = card.Rect.bottom();
		}
		ContentHeight = bottom + card_margin;
		verticalScrollBar()->setRange(0, std::max(0, ContentHeight - viewport()->height()));
		verticalScrollBar()->setPageStep(viewport()->height());
		verticalScrollBar()->setSingleStep(fontMetrics().lineSpacing());
		sync_badge_buttons();
		viewport()->update();
	}

	void conversation_cards_view::changeEvent(QEvent* const aEvent)
	{
		QAbstractScrollArea::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange) {
			clear_measurements();
			relayout();
		}
	}

	void conversation_cards_view::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ viewport() };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.translate(0, -verticalScrollBar()->value());
		const auto& colours  = palette();
		const auto  ink      = colours.color(QPalette::Active, QPalette::Text);
		const auto  faint    = colours.color(QPalette::Active, QPalette::PlaceholderText);
		const auto  rule     = colours.color(QPalette::Active, QPalette::Mid);
		const auto  surface  = colours.color(QPalette::Active, QPalette::Base);
		auto        lastPath = QString{ };
		const auto  small    = small_font(font());
		const auto  bold     = bold_font(font());
		for (const auto& card : Cards) {
			if (Scope == cards_scope::whole_pull_request && card.Path != lastPath) {
				lastPath = card.Path;
				painter.setFont(small);
				painter.setPen(faint);
				painter.drawText(QRect{ card_margin, card.Rect.top() - path_row_height, width() - 2 * card_margin, path_row_height },
								 Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics{ small }.elidedText(card.Path, Qt::ElideMiddle, width() - 2 * card_margin));
			}
			const auto current = !card.Draft && card.Id == Current;
			auto       pen     = QPen{ current ? Accent : rule, current ? 1.5 : 1.0 };
			if (card.Draft) {
				pen = QPen{ Accent, 1.0, Qt::DashLine };
			}
			painter.setPen(pen);
			painter.setBrush(surface);
			painter.drawRoundedRect(QRectF{ card.Rect }.adjusted(0.5, 0.5, -0.5, -0.5), 7.0, 7.0);
			if (Compact) {
				painter.setFont(font());
				painter.setPen(ink);
				painter.drawText(card.Rect.adjusted(card_padding, card_padding, -card_padding, -card_padding),
								 Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap | Qt::TextWrapAnywhere, compact_label(card));
				painter.setFont(small);
				painter.setPen(Accent);
				for (const auto& [action, rect] : card.Actions) {
					if (action != card_action::defect) {
						painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, action_text(action));
					}
				}
				continue;
			}
			const auto dim  = card.Resolved;
			auto       tagY = card.Rect.top() + card_padding;
			if (card.Resolved || card.Outdated || card.Draft) {
				painter.setFont(small);
				painter.setPen(card.Draft ? Accent : faint);
				const auto tag = card.Draft
					? QStringLiteral("Draft on line %1").arg(card.Line.value_or(0U))
					: card.Resolved && card.Outdated ? QStringLiteral("Resolved, outdated")
					: card.Resolved                  ? QStringLiteral("Resolved")
													 : QStringLiteral("Outdated");
				painter.drawText(QRect{ card.Rect.left() + card_padding, tagY, card.Rect.width() - 2 * card_padding, header_height },
								 Qt::AlignLeft | Qt::AlignVCenter, tag);
			}
			// [[BPR-0649]]. The excerpt sits above the posts on the code
			// background, in the code font; the theme's own colours.
			if (Feed && !card.Excerpt.isEmpty()) {
				const auto lines  = static_cast<int>(card.Excerpt.count(QLatin1Char{ '\n' })) + 1;
				const auto height = lines * QFontMetrics{ CodeFont }.height() + 2 * excerpt_padding;
				const auto top    = tagY + (card.Resolved || card.Outdated || card.Draft ? header_height : 0);
				const auto box    = QRect{ card.Rect.left() + card_padding, top, card.Rect.width() - 2 * card_padding, height };
				painter.setPen(Qt::NoPen);
				painter.setBrush(CodeBackground);
				painter.drawRoundedRect(box, 4.0, 4.0);
				painter.setFont(CodeFont);
				painter.setPen(CodeInk);
				painter.drawText(box.adjusted(excerpt_padding, excerpt_padding, -excerpt_padding, -excerpt_padding),
								 Qt::AlignLeft | Qt::AlignTop, card.Excerpt);
			}
			// The rail: a line between the first and last avatars.
			if (card.Posts.size() > 1U) {
				const auto x = card.Posts.front().Rect.left() + avatar_size / 2;
				painter.setPen(QPen{ rule, 2.0 });
				painter.drawLine(QPoint{ x, card.Posts.front().Rect.top() + avatar_size },
								 QPoint{ x, card.Posts.back().Rect.top() });
			}
			for (const auto& post : card.Posts) {
				const auto avatar = QRectF{ static_cast<qreal>(post.Rect.left()), static_cast<qreal>(post.Rect.top()),
											static_cast<qreal>(avatar_size), static_cast<qreal>(avatar_size) };
				painter.setPen(Qt::NoPen);
				painter.setBrush(post.Mine ? Accent : rule);
				painter.drawEllipse(avatar);
				painter.setFont(small);
				painter.setPen(post.Mine ? surface : ink);
				painter.drawText(avatar, Qt::AlignCenter, post.Author.left(1).toUpper());
				const auto textLeft  = post.Rect.left() + avatar_column;
				const auto textWidth = post.Rect.width() - avatar_column;
				// [[BPR-0656]]. An unread post: a blue dot before a full-ink
				// name. A read post, once the ledger has said so, steps its
				// name back; before any read state every name is full ink.
				const auto read = ReadStateKnown && !post.Unread && !post.Mine;
				painter.setFont(bold);
				painter.setPen(dim || read ? faint : ink);
				const auto authorHeight = QFontMetrics{ font() }.height();
				if (post.Unread) {
					painter.save();
					painter.setPen(Qt::NoPen);
					painter.setBrush(Accent);
					painter.drawEllipse(QRectF{ static_cast<qreal>(textLeft) - 8.5,
												static_cast<qreal>(post.Rect.top()) + authorHeight / 2.0 - 3.0, 6.0, 6.0 });
					painter.restore();
				}
				painter.drawText(QRect{ textLeft, post.Rect.top(), textWidth, authorHeight },
								 Qt::AlignLeft | Qt::AlignVCenter, post.Author);
				if (Feed && (!post.Role.isEmpty() || !post.ReplyingTo.isEmpty())) {
					auto header = post.Role;
					if (!post.ReplyingTo.isEmpty()) {
						header += QStringLiteral("  replying to %1").arg(post.ReplyingTo);
					}
					const auto after = QFontMetrics{ bold }.horizontalAdvance(post.Author) + 8;
					painter.setFont(small);
					painter.setPen(faint);
					painter.drawText(QRect{ textLeft + after, post.Rect.top(), std::max(0, textWidth - after), authorHeight },
									 Qt::AlignLeft | Qt::AlignVCenter,
									 QFontMetrics{ small }.elidedText(header, Qt::ElideRight, std::max(0, textWidth - after)));
				}
				painter.setFont(font());
				painter.setPen(dim ? faint : ink);
				painter.drawText(QRect{ textLeft, post.Rect.top() + authorHeight + 2, textWidth, post.Rect.height() - authorHeight },
								 Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, post.Body);
			}
			painter.setFont(small);
			for (const auto& [action, rect] : card.Actions) {
				painter.setPen(Accent);
				if (action != card_action::defect) {
					painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, action_text(action));
				}
			}
		}
		if (Cards.empty()) {
			painter.setFont(font());
			painter.setPen(faint);
			painter.drawText(QRect{ card_margin, card_margin, width() - 2 * card_margin, 60 },
							 Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
							 Scope == cards_scope::this_file
								 ? QStringLiteral("No threads on this file.")
								 : QStringLiteral("No threads on this pull request."));
		}
	}

	void conversation_cards_view::resizeEvent(QResizeEvent* const aEvent)
	{
		QAbstractScrollArea::resizeEvent(aEvent);
		relayout();
	}

	void conversation_cards_view::choose(
		const std::size_t aCard, const bool aOpenDetail)
	{
		if (aCard >= Cards.size() || Cards[aCard].Draft) {
			return;
		}
		Current           = Cards[aCard].Id;
		const auto top    = Cards[aCard].Rect.top();
		const auto bottom = Cards[aCard].Rect.bottom();
		if (top < verticalScrollBar()->value()) {
			verticalScrollBar()->setValue(top);
		} else if (bottom > verticalScrollBar()->value() + viewport()->height()) {
			verticalScrollBar()->setValue(bottom - viewport()->height());
		}
		viewport()->update();
		if (ThreadDispatch) {
			// [[BPR-1029]]. The dispatch may make another thread current, so it
			// is handed its own copy of the identity rather than a reference
			// into state it can change.
			const auto chosen = Current;
			ThreadDispatch(chosen, aOpenDetail);
		}
	}

	void conversation_cards_view::mousePressEvent(QMouseEvent* const aEvent)
	{
		if (aEvent->button() != Qt::LeftButton) {
			QAbstractScrollArea::mousePressEvent(aEvent);
			return;
		}
		const auto hit = hit_test(aEvent->position().toPoint());
		if (!hit.has_value()) {
			QAbstractScrollArea::mousePressEvent(aEvent);
			return;
		}
		const auto& [index, action] = *hit;
		setFocus(Qt::MouseFocusReason);
		if (action.has_value()) {
			if (!Cards[index].Draft) {
				Current = Cards[index].Id;
				viewport()->update();
			}
			if (ActionDispatch) {
				// [[BPR-1029]]. The window's dispatch can relay these cards out
				// before it reads the identity, which rebuilds the vector the
				// card lives in; a reference into it would then name whichever
				// card was rebuilt in that slot.
				const auto identity = Cards[index].Id;
				ActionDispatch(*action, identity);
			}
			aEvent->accept();
			return;
		}
		choose(index, true);
		aEvent->accept();
	}

	void conversation_cards_view::keyPressEvent(QKeyEvent* const aEvent)
	{
		if (aEvent->key() == Qt::Key_Menu || (aEvent->key() == Qt::Key_F10 && aEvent->modifiers().testFlag(Qt::ShiftModifier))) {
			show_thread_menu(Current);
			aEvent->accept();
			return;
		}
		auto threads = std::vector<std::size_t>{ };
		for (std::size_t index = 0U; index < Cards.size(); ++index) {
			if (!Cards[index].Draft) {
				threads.push_back(index);
			}
		}
		const auto position = std::ranges::find_if(threads, [this](const std::size_t aIndex) {
			return Cards[aIndex].Id == Current;
		});
		switch (aEvent->key()) {
		case Qt::Key_Down:
			if (!threads.empty()) {
				choose(position == threads.end() || std::next(position) == threads.end()
						   ? threads.front()
						   : *std::next(position),
					   false);
			}
			aEvent->accept();
			return;
		case Qt::Key_Up:
			if (!threads.empty()) {
				choose(position == threads.end() || position == threads.begin()
						   ? threads.back()
						   : *std::prev(position),
					   false);
			}
			aEvent->accept();
			return;
		case Qt::Key_Return:
		case Qt::Key_Enter:
			if (position != threads.end()) {
				choose(*position, true);
			}
			aEvent->accept();
			return;
		default:
			break;
		}
		QAbstractScrollArea::keyPressEvent(aEvent);
	}
} // namespace bpr::qt
