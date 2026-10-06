#include "bpr/qt/conversation_cards_view.hpp"

#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/feedback_label_controls.hpp"

#include "conversation_glyphs.hpp"
#include "conversation_open_card.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextLayout>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
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
		/// The prototype's measures ([[BPR-1290]], `BPR-1290-inline-drawer-v1`).
		constexpr int              note_margin     = 8;
		constexpr int              note_gap        = 6;
		constexpr int              note_nudge      = -4;
		constexpr int              note_top        = 6;
		constexpr int              note_bottom     = 7;
		constexpr int              note_right      = 8;
		constexpr int              rail_right      = 8;
		constexpr int              gut_left        = 12;
		constexpr int              avatar_size     = 18;
		constexpr int              mark_gap        = 3;
		constexpr int              mark_size       = 16;
		constexpr int              text_left       = gut_left + avatar_size + 9;
		constexpr int              path_row_height = 22;
		constexpr int              edge_band       = 34;
		constexpr int              edge_reach      = 30;
		constexpr int              chip_height     = 22;
		constexpr std::size_t      chips_each_way  = 3U;
		constexpr std::string_view draft_card_id   = "draft:unpublished";

		[[nodiscard]] auto text(const std::string& aValue) -> QString
		{
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		}

		[[nodiscard]] auto semibold(QFont aFont) -> QFont
		{
			aFont.setWeight(QFont::DemiBold);
			return aFont;
		}

		[[nodiscard]] auto scaled(QFont aFont, const qreal aFactor) -> QFont
		{
			if (aFont.pointSizeF() > 0.0) {
				aFont.setPointSizeF(aFont.pointSizeF() * aFactor);
			} else if (aFont.pixelSize() > 0) {
				aFont.setPixelSize(std::max(1, static_cast<int>(std::lround(aFont.pixelSize() * aFactor))));
			}
			return aFont;
		}

		[[nodiscard]] auto name_height(const QFont& aFont) -> int
		{
			return std::max(19, QFontMetrics{ semibold(aFont) }.height());
		}

		[[nodiscard]] auto body_line_height(const QFont& aFont) -> int
		{
			return static_cast<int>(std::lround(QFontMetricsF{ aFont }.height() * 1.38));
		}

		[[nodiscard]] auto has_code_anchor(const thread_card& aCard) -> bool
		{
			return !aCard.Path.isEmpty() && !aCard.Outdated && !aCard.AnchorUnavailable
				&& aCard.Side.has_value() && aCard.Line.has_value() && *aCard.Line > 0U;
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

		[[nodiscard]] auto header_length_of(const std::string_view aBody) -> qsizetype
		{
			const auto annotation = core::recognize_feedback_prefix(aBody);
			if (annotation.Status != core::feedback_prefix_status::recognized || !annotation.Header.has_value()) {
				return 0;
			}
			return feedback_text_index(aBody, annotation.Header->End);
		}

		/// \brief A closed note's body: one flowing paragraph, the header in
		/// semibold and inline code in the code font, backticks dropped as the
		/// prototype sets them.
		struct display_text
		{
			QString                         Text;
			QList<QTextLayout::FormatRange> Formats;
		};

		[[nodiscard]] auto display_of(const card_post& aPost, const QFont& aFont, const QFont& aCodeFont, const QColor& aCodeFill) -> display_text
		{
			auto result = display_text{ };
			auto body   = aPost.Body;
			body.replace(QLatin1Char{ '\n' }, QLatin1Char{ ' ' });
			const auto header = std::clamp<qsizetype>(aPost.HeaderLength, 0, body.size());
			result.Text       = body.left(header);
			if (header > 0) {
				auto format = QTextCharFormat{ };
				format.setFontWeight(QFont::DemiBold);
				result.Formats.push_back(QTextLayout::FormatRange{ .start = 0, .length = static_cast<int>(header), .format = format });
			}
			auto at = header;
			while (at < body.size()) {
				const auto open  = body.indexOf(QLatin1Char{ '`' }, at);
				const auto close = open < 0 ? -1 : body.indexOf(QLatin1Char{ '`' }, open + 1);
				if (open < 0 || close < 0 || close == open + 1) {
					result.Text += body.mid(at);
					break;
				}
				result.Text += body.mid(at, open - at);
				auto code = QTextCharFormat{ };
				code.setFontFamilies(aCodeFont.families());
				code.setFontFixedPitch(true);
				if (aFont.pointSizeF() > 0.0) {
					code.setFontPointSize(aFont.pointSizeF() * 0.9);
				}
				code.setBackground(aCodeFill);
				result.Formats.push_back(QTextLayout::FormatRange{ .start = static_cast<int>(result.Text.size()), .length = static_cast<int>(close - open - 1), .format = code });
				result.Text += body.mid(open + 1, close - open - 1);
				at = close + 1;
			}
			return result;
		}

		/// \brief Lays \p aDisplay out at \p aWidth, at most \p aLines lines,
		/// eliding the last when the text goes on.
		[[nodiscard]] auto clamp_lines(display_text aDisplay, const QFont& aFont, const int aWidth, const int aLines) -> display_text
		{
			auto layout = QTextLayout{ aDisplay.Text, aFont };
			layout.setFormats(aDisplay.Formats);
			auto option = QTextOption{ };
			option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
			layout.setTextOption(option);
			layout.beginLayout();
			auto starts = std::vector<int>{ };
			for (auto line = layout.createLine(); line.isValid(); line = layout.createLine()) {
				line.setLineWidth(aWidth);
				starts.push_back(line.textStart());
				if (static_cast<int>(starts.size()) > aLines) {
					break;
				}
			}
			layout.endLayout();
			if (static_cast<int>(starts.size()) <= aLines) {
				return aDisplay;
			}
			const auto last = starts[static_cast<std::size_t>(aLines - 1)];
			const auto rest = aDisplay.Text.mid(last);
			// Room for the semibold header when it is on this line too.
			const auto elided = QFontMetrics{ aFont }.elidedText(rest, Qt::ElideRight, std::max(1, aWidth - 6));
			aDisplay.Text     = aDisplay.Text.left(last) + elided;
			for (auto& format : aDisplay.Formats) {
				format.length = std::max(0, std::min(format.start + format.length, static_cast<int>(aDisplay.Text.size())) - format.start);
			}
			return aDisplay;
		}

		/// \brief Sets a chip's words only when they change: a scroll frame that
		/// leaves a chip as it was announces nothing.
		template <typename Setter>
		void set_if_changed(const QString& aCurrent, Setter&& aSetter, const QString& aValue)
		{
			if (aCurrent != aValue) {
				std::forward<Setter>(aSetter)(aValue);
			}
		}

		/// \brief The fade at the drawer's top or bottom edge, under its chips
		/// and over the notes, with an arrow saying which way the threads are.
		/// Presses go through it to whatever is under it.
		class edge_fade final : public QWidget
		{
		public:
			edge_fade(const bool aDown, QWidget* aParent)
				: QWidget{ aParent }
				, Down{ aDown }
			{
				setObjectName(aDown ? QStringLiteral("threadEdgeBelow") : QStringLiteral("threadEdgeAbove"));
				setAttribute(Qt::WA_TransparentForMouseEvents, true);
			}

			note_colours Colours;

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter  = QPainter{ this };
				auto gradient = QLinearGradient{ 0.0, 0.0, 0.0, static_cast<qreal>(height()) };
				auto clear    = Colours.Drawer;
				clear.setAlpha(0);
				gradient.setColorAt(0.0, Down ? clear : Colours.Drawer);
				gradient.setColorAt(Down ? 0.45 : 0.55, Colours.Drawer);
				gradient.setColorAt(1.0, Down ? Colours.Drawer : clear);
				if (Down) {
					gradient.setColorAt(0.45, Colours.Drawer);
				}
				painter.fillRect(rect(), gradient);
				painter.setRenderHint(QPainter::Antialiasing, true);
				paint_conversation_glyph(painter, QRectF{ static_cast<qreal>(note_margin + 3), (height() - 12) / 2.0, 12.0, 12.0 },
										 Down ? conversation_glyph::down : conversation_glyph::up, Colours.Faint);
			}

		private:
			bool Down = false;
		};

		/// \brief An out-of-view thread: its mark (or a plain or dashed
		/// square), its line, and a dot when it is new.
		class edge_chip final : public QAbstractButton
		{
		public:
			explicit edge_chip(QWidget* aParent)
				: QAbstractButton{ aParent }
			{
				setObjectName(QStringLiteral("threadEdgeChip"));
				setFocusPolicy(Qt::StrongFocus);
				setCursor(Qt::PointingHandCursor);
				setAttribute(Qt::WA_Hover, true);
				setFont(semibold(scaled(font(), 0.86)));
			}

			std::string                         Thread;
			std::optional<core::feedback_label> Label;
			feedback_mark_emphasis              Emphasis;
			bool                                Draft  = false;
			bool                                Unread = false;
			bool                                More   = false;
			feedback_mark_theme                 Marks;
			note_colours                        Colours;

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto metrics = QFontMetrics{ font() };
				if (More) {
					return { metrics.horizontalAdvance(text()) + 14, chip_height };
				}
				return { 5 + 14 + 5 + metrics.horizontalAdvance(text()) + (Unread ? 9 : 0) + 7, chip_height };
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
				painter.setPen(QPen{ underMouse() || hasFocus() ? Colours.Faint : Colours.Rule2, 1.0 });
				painter.setBrush(Colours.Panel);
				painter.drawRoundedRect(box, height() / 2.0, height() / 2.0);
				if (hasFocus()) {
					painter.setPen(QPen{ Colours.Signal, 2.0 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(box, height() / 2.0, height() / 2.0);
				}
				auto x = 5.0;
				if (!More) {
					const auto face = QRectF{ x, (height() - 14) / 2.0, 14.0, 14.0 };
					if (Label.has_value()) {
						paint_feedback_mark(painter, face, *Label, Emphasis,
											feedback_mark_colours{ .Tint = Marks.Tints[static_cast<std::size_t>(*Label)], .Panel = Colours.Panel, .Ink = Colours.Ink, .Style = Marks.Style });
					} else {
						painter.setPen(QPen{ Colours.Faint, 1.5, Draft ? Qt::DashLine : Qt::SolidLine });
						painter.setBrush(Draft ? QColor{ Qt::transparent } : Colours.Hover);
						painter.drawRoundedRect(face.adjusted(1.5, 1.5, -1.5, -1.5), 3.0, 3.0);
					}
					x += 14 + 5;
				} else {
					x = 7.0;
				}
				painter.setPen(underMouse() ? Colours.Ink : Colours.Ink2);
				const auto words = QFontMetrics{ font() }.horizontalAdvance(text());
				painter.drawText(QRectF{ x, 0.0, static_cast<qreal>(words + 1), static_cast<qreal>(height()) }, Qt::AlignLeft | Qt::AlignVCenter, text());
				if (Unread && !More) {
					painter.setPen(Qt::NoPen);
					painter.setBrush(Colours.New);
					painter.drawEllipse(QRectF{ x + words + 3.0, height() / 2.0 - 3.0, 6.0, 6.0 });
				}
			}
		};
	} // namespace

	conversation_cards_view::conversation_cards_view(QWidget* const aParent)
		: QAbstractScrollArea{ aParent }
		, CodeFont{ to_presentation_font(default_typography_settings().Code) }
	{
		setObjectName(QStringLiteral("conversationCards"));
		MarkTheme = default_feedback_mark_theme(palette());
		setAccessibleName(QStringLiteral("Threads in this file"));
		setFocusPolicy(Qt::StrongFocus);
		setFrameShape(QFrame::NoFrame);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		viewport()->setAutoFillBackground(true);
		viewport()->setBackgroundRole(QPalette::Window);
		viewport()->setMouseTracking(true);
		connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
			place_open_card();
			sync_edges();
			viewport()->update();
		});
	}

	conversation_cards_view::~conversation_cards_view() = default;

	// ---------------------------------------------------------------- inputs

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
		const auto add = [this, menu, identity = card.Id](const card_action aKind, const QString& aText, const bool aEnabled, const QString& aReason) {
			auto* action = menu->addAction(aText);
			action->setEnabled(aEnabled && static_cast<bool>(ActionDispatch));
			action->setToolTip(aEnabled ? action->text() : aReason);
			connect(action, &QAction::triggered, menu, [this, identity, aKind] { if (ActionDispatch) { ActionDispatch(aKind, identity); } });
		};
		add(card_action::reply, QStringLiteral("Reply"), has_code_anchor(card), tr("This thread has no current code anchor. Open its current comparison before replying."));
		add(card_action::mark_read, QStringLiteral("Mark read"), card.UnreadCount > 0U, tr("This thread has no unread posts."));
		add(card.Resolved ? card_action::unresolve : card_action::resolve, card.Resolved ? QStringLiteral("Unresolve") : QStringLiteral("Resolve"),
			ResolutionOffered && card.Kind == QStringLiteral("Inline thread"), tr("Thread resolution is not available for this conversation."));
		add(Feed ? card_action::open_in_code : card_action::show_line, Feed ? QStringLiteral("Open in code") : QStringLiteral("Show line"),
			has_code_anchor(card), tr("This thread's code anchor is unavailable."));
		if (!card.Posts.empty()) {
			auto* copy = menu->addAction(QStringLiteral("Copy first post"));
			copy->setObjectName(QStringLiteral("threadCopySource"));
			copy->setToolTip(QStringLiteral("Copy the first post as written, its label prefix included"));
			connect(copy, &QAction::triggered, menu, [body = card.Posts.front().Body] {
				if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
					clipboard->setText(body);
				}
			});
		}
		if (ContextMenuBuilder) {
			menu->addSeparator();
			ContextMenuBuilder(*menu, card.Id);
		}
		return menu;
	}

	void conversation_cards_view::show_thread_menu(const std::string& aIdentity, QPoint aGlobalPosition)
	{
		if (auto* menu = thread_menu(aIdentity)) {
			if (aGlobalPosition.isNull()) {
				const auto note = note_rect(aIdentity);
				aGlobalPosition = viewport()->mapToGlobal(note.isEmpty() ? viewport()->rect().center() : QPoint{ note.left() + text_left, note.top() + name_height(font()) });
			}
			menu->popup(aGlobalPosition);
		}
	}

	void conversation_cards_view::contextMenuEvent(QContextMenuEvent* aEvent)
	{
		if (aEvent->reason() == QContextMenuEvent::Keyboard) {
			show_thread_menu(Current);
		} else if (const auto hit = hit_test(aEvent->pos())) {
			const auto identity = Cards[*hit].Id;
			show_thread_menu(identity, aEvent->globalPos());
		}
		aEvent->accept();
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
			// A changed snapshot need not change any text. Retain measurements
			// only for still-present roots; each use validates its inputs.
			auto roots = std::set<std::string>{ };
			for (const auto& node : aConversations) {
				if (node.Kind == application::github_conversation_kind::review_thread && !node.ParentId.has_value()) {
					roots.insert(node.Id);
				}
			}
			forget_measurements_if([&roots](const std::string& aId) {
				return !roots.contains(aId) && aId != draft_card_id;
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
		Open.clear();
		Hovered.clear();
		Replying.clear();
		ReadState.clear();
		ReadStateByThread.clear();
		ReadStateKnown = false;
		UnreadThreads.reset();
		verticalScrollBar()->setValue(0);
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
		RetainedMarks.clear();
		RetainedHeaders.clear();
		RetainedMarks.reserve(Retained.size());
		RetainedHeaders.reserve(Retained.size());
		RetainedFiles.reserve(Retained.size());
		RetainedById.reserve(Retained.size());
		for (std::size_t index = 0U; index < Retained.size(); ++index) {
			RetainedFiles.push_back(core::file_identity_of(Retained[index].Path));
			// [[BPR-1083]]. Marks follow the accepted body; an unchanged forest
			// is not read again by a relayout.
			const auto& body       = Retained[index].ReviewerProse.has_value() ? *Retained[index].ReviewerProse : Retained[index].Body;
			const auto  annotation = core::recognize_feedback_prefix(body);
			const auto  recognized = annotation.Status == core::feedback_prefix_status::recognized && annotation.Label.has_value()
				&& annotation.Subject.has_value();
			RetainedMarks.push_back(recognized ? feedback_body_mark{ .Label    = annotation.Label,
																	 .Emphasis = feedback_mark_emphasis_of(annotation),
																	 .Name     = feedback_mark_name(annotation, body) }
											   : feedback_body_mark{ });
			RetainedHeaders.push_back(recognized && annotation.Header.has_value() ? feedback_text_index(body, annotation.Header->End) : 0);
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
		verticalScrollBar()->setValue(0);
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
		verticalScrollBar()->setValue(0);
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
		// [[BPR-1290]]. A reply is written in place inside its thread and
		// draws no note of its own, so typing it changes no note.
		const auto noted = [](const std::optional<core::review_comment_draft>& aValue) {
			return aValue.has_value() && !aValue->ReplyToThread.has_value() && !aValue->shared();
		};
		const auto affects = noted(Draft) || noted(aDraft) || CurrentComparison != aCurrentComparison;
		if (!noted(aDraft)) {
			forget_measurements_if([](const std::string& aId) { return aId == draft_card_id; });
		}
		Draft             = std::move(aDraft);
		CurrentComparison = std::move(aCurrentComparison);
		if (affects) {
			relayout();
		}
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
		setVerticalScrollBarPolicy(Feed ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
		verticalScrollBar()->setValue(0);
		setAccessibleName(Feed ? QStringLiteral("Conversations") : QStringLiteral("Threads in this file"));
		OpenCard = nullptr;
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

	void conversation_cards_view::set_marks_observer(std::function<void()> aObserver)
	{
		MarksObserver = std::move(aObserver);
	}

	void conversation_cards_view::set_scroll_forward(std::function<bool(QWheelEvent&)> aForward)
	{
		ScrollForward = std::move(aForward);
	}

	void conversation_cards_view::set_link_dispatch(std::function<void(const std::string&, const std::string&)> aDispatch)
	{
		LinkDispatch = std::move(aDispatch);
	}

	void conversation_cards_view::set_reply_text_dispatch(std::function<void(const std::string&, const QString&)> aDispatch)
	{
		ReplyTextDispatch = std::move(aDispatch);
	}

	// ---------------------------------------------------------------- counts

	auto conversation_cards_view::thread_count() const noexcept -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(Cards, [](const thread_card& aCard) { return !aCard.Draft; }));
	}

	auto conversation_cards_view::unresolved_count() const noexcept -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(Cards, [](const thread_card& aCard) {
			return !aCard.Draft && !aCard.Resolved && aCard.Kind == QStringLiteral("Inline thread");
		}));
	}

	auto conversation_cards_view::unread_thread_count() const noexcept -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(Cards, [](const thread_card& aCard) { return aCard.UnreadCount > 0U; }));
	}

	void conversation_cards_view::set_code_font(const QFont& aFont)
	{
		if (CodeFont == aFont) {
			return;
		}
		CodeFont = aFont;
		clear_measurements();
		OpenCard = nullptr;
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

	auto conversation_cards_view::line_marks(const core::diff_side aSide) const -> std::vector<thread_line_mark>
	{
		auto marks = std::map<std::uint32_t, thread_line_mark>{ };
		for (const auto& card : Cards) {
			if (card.Side != aSide || !card.Line.has_value() || *card.Line == 0U || card.Outdated || card.AnchorUnavailable) {
				continue;
			}
			const auto line           = *card.Line - 1U;
			const auto [found, fresh] = marks.try_emplace(line);
			auto& mark                = found->second;
			if (fresh) {
				mark.Line     = line;
				mark.Resolved = card.Resolved;
				mark.Draft    = card.Draft;
				if (!card.Posts.empty()) {
					mark.Label    = card.Posts.front().Label;
					mark.Emphasis = card.Posts.front().Emphasis;
				}
				const auto posts = card.Posts.size();
				mark.Description = card.Draft ? QStringLiteral("Line %1: your draft").arg(*card.Line)
											  : QStringLiteral("Line %1: %2 %3").arg(*card.Line).arg(posts).arg(posts == 1U ? QStringLiteral("post") : QStringLiteral("posts"));
				if (card.UnreadCount > 0U) {
					mark.Description += QStringLiteral(", %1 new").arg(card.UnreadCount);
				}
				if (card.Resolved) {
					mark.Description += QStringLiteral(", resolved");
				}
			} else {
				mark.Resolved = mark.Resolved && card.Resolved;
				mark.Draft    = mark.Draft && card.Draft;
			}
			mark.Unread  = mark.Unread || card.UnreadCount > 0U;
			mark.Open    = mark.Open || (!Open.empty() && card.Id == Open);
			mark.Hovered = mark.Hovered || (!Hovered.empty() && card.Id == Hovered);
		}
		auto result = std::vector<thread_line_mark>{ };
		result.reserve(marks.size());
		for (auto& [line, mark] : marks) {
			result.push_back(std::move(mark));
		}
		return result;
	}

	void conversation_cards_view::set_current_thread(const std::string& aId)
	{
		// A reply's identity names its thread: the selection may be on a reply.
		auto current = aId;
		for (const auto& card : Cards) {
			if (card.Draft) {
				continue;
			}
			if (card.Id == aId) {
				break;
			}
			if (std::ranges::any_of(card.Posts, [&aId](const card_post& aPost) { return aPost.Id == aId; })) {
				current = card.Id;
				break;
			}
		}
		if (Current == current) {
			return;
		}
		Current = std::move(current);
		viewport()->update();
	}

	void conversation_cards_view::set_colours(const QColor& aAccent, const QColor& aMarker)
	{
		if (Accent == aAccent && Marker == aMarker) {
			return;
		}
		Accent   = aAccent;
		Marker   = aMarker;
		OpenCard = nullptr;
		relayout();
	}

	auto conversation_cards_view::accent() const noexcept -> const QColor&
	{
		return Accent;
	}

	auto conversation_cards_view::marker() const noexcept -> const QColor&
	{
		return Marker;
	}

	auto conversation_cards_view::cards() const noexcept -> const std::vector<thread_card>&
	{
		return Cards;
	}

	auto conversation_cards_view::current_thread() const noexcept -> const std::string&
	{
		return Current;
	}

	auto conversation_cards_view::open_thread_id() const noexcept -> const std::string&
	{
		return Open;
	}

	auto conversation_cards_view::hovered_thread() const noexcept -> const std::string&
	{
		return Hovered;
	}

	auto conversation_cards_view::layout_nodes_visited() const noexcept -> std::uint64_t
	{
		return LayoutNodesVisited;
	}

	auto conversation_cards_view::layouts() const noexcept -> std::uint64_t
	{
		return Layouts;
	}

	auto conversation_cards_view::measurements() const noexcept -> std::uint64_t
	{
		return MeasurementCount;
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

	auto conversation_cards_view::reply_editor() const -> QPlainTextEdit*
	{
		return OpenCard != nullptr ? OpenCard->reply_editor() : nullptr;
	}

	auto conversation_cards_view::reply_label_controls() const -> feedback_label_controls*
	{
		return OpenCard != nullptr && OpenCard->reply_editor() != nullptr ? OpenCard->label_controls() : nullptr;
	}

	auto conversation_cards_view::open_card() const -> QWidget*
	{
		return OpenCard;
	}

	auto conversation_cards_view::edge_chips() const -> std::vector<QAbstractButton*>
	{
		return { Chips.begin(), Chips.begin() + static_cast<std::ptrdiff_t>(std::min(ChipsShown, Chips.size())) };
	}

	auto conversation_cards_view::overflow() const -> int
	{
		return verticalScrollBar()->value();
	}

	auto conversation_cards_view::note_rect(const std::string& aId) const -> QRect
	{
		const auto index = card_index(aId);
		return index.has_value() ? to_viewport(Cards[*index].Rect) : QRect{ };
	}

	auto conversation_cards_view::card_index(const std::string& aId) const -> std::optional<std::size_t>
	{
		if (aId.empty()) {
			return std::nullopt;
		}
		const auto at = std::ranges::find(Cards, aId, &thread_card::Id);
		return at == Cards.end() ? std::nullopt : std::optional{ static_cast<std::size_t>(std::distance(Cards.begin(), at)) };
	}

	auto conversation_cards_view::to_content(const QPoint aViewportPoint) const -> QPoint
	{
		return { aViewportPoint.x(), aViewportPoint.y() + verticalScrollBar()->value() };
	}

	auto conversation_cards_view::to_viewport(const QRect aContentRect) const -> QRect
	{
		return aContentRect.translated(0, -verticalScrollBar()->value());
	}

	auto conversation_cards_view::hit_test(const QPoint aViewportPoint) const -> std::optional<std::size_t>
	{
		const auto point = to_content(aViewportPoint);
		for (std::size_t index = 0U; index < Cards.size(); ++index) {
			if (Cards[index].Rect.contains(point)) {
				return index;
			}
		}
		return std::nullopt;
	}

	auto conversation_cards_view::aligned() const noexcept -> bool
	{
		return !Feed && Scope == cards_scope::this_file;
	}

	// ----------------------------------------------------------- measurement

	void conversation_cards_view::clear_measurements()
	{
		Measurements         = { };
		PreviousMeasurements = { };
	}

	void conversation_cards_view::forget_measurements_if(
		const std::function<bool(const std::string&)>& aForget)
	{
		for (auto* const generation : { &Measurements, &PreviousMeasurements }) {
			std::erase_if(generation->Notes, [&aForget](const auto& aEntry) { return aForget(aEntry.first); });
		}
	}

	auto conversation_cards_view::closed_measure_of(const thread_card& aCard) const -> const closed_measure*
	{
		const auto found = Measurements.Notes.find(aCard.Id);
		return found == Measurements.Notes.end() ? nullptr : &found->second;
	}

	void conversation_cards_view::measure_closed(thread_card& aCard, const int aWidth)
	{
		if (MeasurementFont != font()) {
			clear_measurements();
			MeasurementFont = font();
		}
		if (Measurements.Width != aWidth) {
			// The other width is kept rather than discarded, so a layout that
			// returns to it finds its notes already measured.
			std::swap(Measurements, PreviousMeasurements);
			if (Measurements.Width != aWidth) {
				Measurements = measurement_generation{ .Width = aWidth };
			}
		}
		const auto  body   = aCard.Posts.empty() ? QString{ } : aCard.Posts.front().Body;
		const auto  marked = !aCard.Posts.empty() && aCard.Posts.front().Label.has_value();
		const auto& cached = Measurements.Notes.find(aCard.Id);
		if (cached != Measurements.Notes.end() && cached->second.Body == body && cached->second.Resolved == aCard.Resolved
			&& cached->second.Draft == aCard.Draft) {
			aCard.Rect.setHeight(cached->second.Height);
			return;
		}
		++MeasurementCount;
		auto       measure = closed_measure{ .Body = body, .Resolved = aCard.Resolved, .Draft = aCard.Draft };
		const auto lines   = aCard.Resolved ? 1 : 2;
		const auto width   = std::max(40, aWidth - text_left - note_right);
		auto       count   = 0;
		if (!aCard.Posts.empty() && !body.isEmpty()) {
			// One line when the whole body fits the width, otherwise as many as
			// the note shows. Measured by advance, not laid out: a large review
			// measures a thousand notes when its pull request arrives, and the
			// paint lays out only the few on screen.
			auto flowing = body;
			flowing.replace(QLatin1Char{ '\n' }, QLatin1Char{ ' ' });
			const auto header  = std::clamp<qsizetype>(aCard.Posts.front().HeaderLength, 0, flowing.size());
			const auto advance = QFontMetrics{ semibold(font()) }.horizontalAdvance(flowing.left(header))
				+ QFontMetrics{ font() }.horizontalAdvance(flowing.mid(header));
			count = advance <= width ? 1 : lines;
		}
		measure.Lines   = count;
		const auto text = name_height(font()) + 1 + count * body_line_height(font());
		const auto gut  = marked ? avatar_size + mark_gap + mark_size + 1 : avatar_size + 1;
		measure.Height  = note_top + std::max(text, gut) + note_bottom;
		aCard.Rect.setHeight(measure.Height);
		Measurements.Notes.insert_or_assign(aCard.Id, std::move(measure));
	}

	// ---------------------------------------------------------------- layout

	void conversation_cards_view::relayout()
	{
		++Layouts;
		Cards.clear();
		// [[BPR-0972]]. Scoped to one file, only that file's conversations are
		// visited: the per-forest index names them, in retained order.
		auto       candidates = std::vector<std::size_t>{ };
		const auto scoped     = Scope == cards_scope::this_file && AnchorFile.has_value();
		if (scoped) {
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
		auto cardsById = std::unordered_map<std::string_view, std::size_t>{ };
		for (const auto index : candidates) {
			const auto& node = Retained[index];
			// Beside the code the notes are review threads. Discuss also lists
			// the pull request's comments and review summaries, each its own
			// first post.
			const auto isThread = node.Kind == application::github_conversation_kind::review_thread;
			if (node.ParentId.has_value() || (!isThread && (!Feed || node.Deleted))) {
				continue;
			}
			if (scoped && RetainedFiles[index] != *AnchorFile) {
				continue;
			}
			auto card = thread_card{ };
			card.Id   = node.Id;
			if (const auto badge = DefectBadges.find(card.Id); badge != DefectBadges.end()) {
				card.DefectBadge       = badge->second.first;
				card.DefectDescription = badge->second.second;
			}
			card.Kind              = isThread                                        ? QStringLiteral("Inline thread")
				: node.Kind == application::github_conversation_kind::review         ? QStringLiteral("Review summary")
				: node.Kind == application::github_conversation_kind::review_comment ? QStringLiteral("Inline comment")
																					 : QStringLiteral("PR comment");
			card.Path              = text(node.Path);
			card.Side              = side_of(node);
			card.Line              = node.Line;
			card.Resolved          = node.Resolved;
			card.AnchorUnavailable = !node.Path.empty() && node.AnchorAvailability != application::github_conversation_anchor_availability::current;
			card.Outdated          = node.Outdated || card.AnchorUnavailable;
			card.FileLevel         = node.SemanticAnchor.has_value() && node.RemoteSubject == application::github_conversation_remote_subject::file;
			if (!isThread) {
				const auto& mark   = RetainedMarks[index];
				const auto  author = text(node.AuthorLogin);
				card.Posts.push_back(card_post{
					.Id           = node.Id,
					.Author       = author,
					.Body         = text(node.ReviewerProse.value_or(node.Body)),
					.Mine         = !Viewer.isEmpty() && author == Viewer,
					.Role         = !PullRequestAuthor.isEmpty() && author == PullRequestAuthor ? QStringLiteral("author") : QStringLiteral("reviewer"),
					.Label        = mark.Label,
					.Emphasis     = mark.Emphasis,
					.Name         = feedback_post_name(author, mark),
					.HeaderLength = RetainedHeaders[index],
				});
			}
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
			if (node.Kind == application::github_conversation_kind::review_thread || !node.ParentId.has_value() || node.Deleted
				|| cardsById.contains(node.Id)) {
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
			// [[BPR-1290]]. A deep thread is read in place, so a reply that
			// answers anything but the thread's first post says whom it
			// answers. GitHub's own replies all answer the first post.
			auto replyingTo = QString{ };
			if (node.ParentId.has_value() && *node.ParentId != root.Id && !target.Posts.empty()
				&& *node.ParentId != target.Posts.front().Id) {
				if (const auto parent = RetainedById.find(*node.ParentId); parent != RetainedById.end()) {
					replyingTo = text(Retained[parent->second].AuthorLogin);
				}
			}
			const auto& mark = RetainedMarks[index];
			target.Posts.push_back(card_post{
				.Id           = node.Id,
				.Author       = author,
				.Body         = text(node.ReviewerProse.value_or(node.Body)),
				.Depth        = depth,
				.Mine         = !Viewer.isEmpty() && author == Viewer,
				.Unread       = unread,
				.Role         = !PullRequestAuthor.isEmpty() && author == PullRequestAuthor ? QStringLiteral("author") : QStringLiteral("reviewer"),
				.ReplyingTo   = replyingTo,
				.Label        = mark.Label,
				.Emphasis     = mark.Emphasis,
				.Name         = feedback_post_name(author, mark),
				.HeaderLength = RetainedHeaders[index],
			});
		}
		// [[BPR-1290]]. Only new, Hide resolved. The open thread and my draft
		// are never filtered out.
		if (Filters.UnreadOnly || Filters.UnresolvedOnly) {
			std::erase_if(Cards, [this](const thread_card& aCard) {
				return !aCard.Draft && aCard.Id != Open
					&& ((Filters.UnreadOnly && aCard.UnreadCount == 0U)
						|| (Filters.UnresolvedOnly && aCard.Resolved));
			});
		}
		if (Feed && ExcerptLocator) {
			for (auto& card : Cards) {
				if (!card.Draft && card.Side.has_value() && card.Line.has_value() && *card.Line > 0U) {
					card.Excerpt = ExcerptLocator(card.Path, *card.Side, *card.Line).value_or(QString{ });
				}
			}
		}
		// My unpublished note, at its line, while it is on this comparison. A
		// reply is written inside its thread instead.
		if (Scope == cards_scope::this_file && Draft.has_value() && CurrentComparison.has_value()
			&& Draft->Anchor.Comparison == *CurrentComparison && !Draft->shared() && !Draft->ReplyToThread.has_value()
			&& !Draft->Body.empty()) {
			auto card       = thread_card{ };
			card.Id         = std::string{ draft_card_id };
			card.Kind       = QStringLiteral("Draft");
			card.Draft      = true;
			card.Side       = Draft->Anchor.Side;
			card.Line       = Draft->Anchor.Line + 1U;
			const auto mark = feedback_body_mark_of(Draft->Body);
			card.Posts.push_back(card_post{
				.Author       = Viewer.isEmpty() ? QStringLiteral("you") : Viewer,
				.Body         = text(Draft->Body),
				.Mine         = true,
				.Label        = mark.Label,
				.Emphasis     = mark.Emphasis,
				.Name         = feedback_post_name(QStringLiteral("Your note, not published"), mark),
				.HeaderLength = mark.Label.has_value() ? header_length_of(Draft->Body) : 0,
			});
			Cards.push_back(std::move(card));
		}
		auto order = std::size_t{ 0U };
		for (auto& card : Cards) {
			card.ProjectionOrder = order++;
		}
		if (!Open.empty() && !card_index(Open).has_value()) {
			Open.clear();
			Replying.clear();
		}
		if (!Hovered.empty() && !card_index(Hovered).has_value()) {
			Hovered.clear();
		}
		const auto width = std::max(120, viewport()->width()) - 2 * note_margin;
		for (auto& card : Cards) {
			card.Rect = QRect{ note_margin, 0, width, 0 };
			measure_closed(card, width);
		}
		rebuild_open_card();
		place_cards(true);
		if (LayoutObserver) {
			LayoutObserver();
		}
		setAccessibleDescription(QStringLiteral("%1 threads, %2 posts, %3 new; Up and Down move between them, Return opens one, "
												"Esc closes it, R replies, N goes to the next new thread; Shift+F10 opens its actions")
									 .arg(thread_count())
									 .arg(post_count())
									 .arg(unread_post_count()));
		notify_marks();
	}

	void conversation_cards_view::rebuild_open_card()
	{
		const auto index = card_index(Open);
		if (!index.has_value()) {
			if (OpenCard != nullptr) {
				OpenCard->hide();
				OpenCard->deleteLater();
				OpenCard = nullptr;
			}
			return;
		}
		const auto& card = Cards[*index];
		// A signature of everything the open card draws: an unchanged thread
		// keeps its widgets, its focus and the reply being typed.
		auto key = QString::fromStdString(card.Id) + QChar{ 0x1f } + card.DefectBadge + QChar{ 0x1f }
			+ QString::number(static_cast<int>(card.Resolved)) + QString::number(static_cast<int>(card.Outdated))
			+ QString::number(static_cast<int>(ResolutionOffered)) + QString::number(static_cast<int>(Feed)) + QString::number(card.UnreadCount)
			+ card.Excerpt + Viewer;
		for (const auto& post : card.Posts) {
			key += QChar{ 0x1e } + QString::fromStdString(post.Id) + QChar{ 0x1f } + post.Body + QString::number(static_cast<int>(post.Unread));
		}
		if (OpenCard != nullptr && OpenCard->property("threadKey").toString() == key) {
			return;
		}
		auto* const previous    = OpenCard;
		const auto  hadFocus    = previous != nullptr && previous->isAncestorOf(QApplication::focusWidget());
		const auto  focusedName = hadFocus ? QApplication::focusWidget()->objectName() : QString{ };
		auto        style       = open_card_style{
			.Marks             = MarkTheme,
			.Colours           = note_colours_of(palette(), Accent, Marker),
			.CodeFont          = CodeFont,
			.Viewer            = Viewer,
			.ResolutionOffered = ResolutionOffered,
			.Feed              = Feed,
		};
		style.Colours.CodeBackground = palette().color(QPalette::Active, QPalette::Base);
		style.Colours.CodeInk        = CodeInk;
		OpenCard                     = new open_thread_card{ card, style, viewport() };
		OpenCard->setProperty("threadKey", key);
		const auto id    = card.Id;
		OpenCard->Action = [this, id](const card_action aAction) {
			switch (aAction) {
			case card_action::reply:
				if (ActionDispatch) {
					ActionDispatch(card_action::reply, id);
				}
				return;
			case card_action::cancel_reply:
				if (ActionDispatch) {
					ActionDispatch(card_action::cancel_reply, id);
				}
				Replying.clear();
				if (OpenCard != nullptr) {
					OpenCard->set_replying(false);
				}
				setFocus(Qt::OtherFocusReason);
				return;
			case card_action::publish_reply:
				if (ActionDispatch) {
					ActionDispatch(card_action::publish_reply, id);
				}
				Replying.clear();
				if (OpenCard != nullptr) {
					OpenCard->set_replying(false);
				}
				setFocus(Qt::OtherFocusReason);
				return;
			case card_action::show_line:
				if (ActionDispatch) {
					ActionDispatch(Feed ? card_action::open_in_code : card_action::show_line, id);
				}
				return;
			default:
				if (ActionDispatch) {
					ActionDispatch(aAction, id);
				}
				return;
			}
		};
		OpenCard->Link = [this](const std::string& aPost, const QString& aDestination) {
			if (LinkDispatch) {
				LinkDispatch(aPost, aDestination.toStdString());
			}
		};
		OpenCard->ReplyText = [this, id](const QString& aText) {
			if (ReplyTextDispatch) {
				ReplyTextDispatch(id, aText);
			}
		};
		OpenCard->HeightChanged  = [this] { place_cards(true); };
		OpenCard->CloseRequested = [this] {
			close_thread();
			setFocus(Qt::OtherFocusReason);
		};
		OpenCard->MoreRequested = [this, id](const QPoint aGlobal) { show_thread_menu(id, aGlobal); };
		OpenCard->present_reply(Reply);
		if (Replying == id || (Reply.has_value() && Reply->Thread == id && !Reply->Body.trimmed().isEmpty())) {
			Replying = id;
			OpenCard->set_replying(true);
		}
		OpenCard->installEventFilter(this);
		for (auto* const child : OpenCard->findChildren<QWidget*>()) {
			child->installEventFilter(this);
		}
		OpenCard->show();
		if (previous != nullptr) {
			previous->hide();
			previous->deleteLater();
		}
		if (hadFocus) {
			auto* target = focusedName.isEmpty() ? nullptr : OpenCard->findChild<QWidget*>(focusedName);
			if (target != nullptr && target->isVisible()) {
				target->setFocus(Qt::OtherFocusReason);
			} else {
				setFocus(Qt::OtherFocusReason);
			}
		}
	}

	void conversation_cards_view::refresh_geometry()
	{
		// Comparison frames move the lines without changing a conversation:
		// keep the forest and its measurements, and place the notes again.
		// Discuss's excerpts follow the comparison, which can arrive later.
		auto changed = false;
		if (Feed && ExcerptLocator) {
			for (auto& card : Cards) {
				if (card.Draft || !card.Side.has_value() || !card.Line.has_value() || *card.Line == 0U) {
					continue;
				}
				auto excerpt = ExcerptLocator(card.Path, *card.Side, *card.Line).value_or(QString{ });
				if (card.Excerpt != excerpt) {
					card.Excerpt = std::move(excerpt);
					changed      = changed || card.Id == Open;
				}
			}
		}
		if (changed) {
			rebuild_open_card();
		}
		place_cards(changed);
	}

	void conversation_cards_view::place_cards(const bool aForce)
	{
		auto changed = aForce;
		for (auto& card : Cards) {
			const auto wasPlaced = card.Placed;
			const auto targetY   = card.TargetY;
			card.Placed          = false;
			if (aligned() && Locator && card.Side.has_value() && card.Line.has_value() && *card.Line > 0U && !card.Outdated) {
				if (const auto y = Locator(*card.Side, *card.Line); y.has_value()) {
					card.Placed  = true;
					card.TargetY = *y;
				}
			}
			changed = changed || wasPlaced != card.Placed || (card.Placed && targetY != card.TargetY);
		}
		if (!changed) {
			return;
		}
		// Placed threads in line order, then the rest in file order; Discuss
		// lists everything in file order.
		std::ranges::stable_sort(Cards, [this](const thread_card& aLeft, const thread_card& aRight) {
			if (!aligned()) {
				return std::tuple{ aLeft.Path, aLeft.Line.value_or(0U), aLeft.ProjectionOrder }
				< std::tuple{ aRight.Path, aRight.Line.value_or(0U), aRight.ProjectionOrder };
			}
			if (aLeft.Placed != aRight.Placed) {
				return aLeft.Placed;
			}
			if (aLeft.Placed) {
				return std::pair{ aLeft.TargetY, aLeft.ProjectionOrder } < std::pair{ aRight.TargetY, aRight.ProjectionOrder };
			}
			return std::tuple{ aLeft.Path, aLeft.Line.value_or(0U), aLeft.ProjectionOrder }
			< std::tuple{ aRight.Path, aRight.Line.value_or(0U), aRight.ProjectionOrder };
		});
		const auto width = std::max(120, viewport()->width()) - 2 * note_margin;
		for (auto& card : Cards) {
			card.Rect.setLeft(note_margin);
			card.Rect.setWidth(width);
			if (OpenCard != nullptr && card.Id == Open) {
				card.Rect.setHeight(OpenCard->heightForWidth(width));
			} else if (const auto* measure = closed_measure_of(card); measure != nullptr) {
				card.Rect.setHeight(measure->Height);
			}
		}
		if (aligned()) {
			// [[BPR-1290]]. Each thread hangs from its line. The open one is
			// pinned exactly at its line; the others pack above and below it
			// without overlapping. With none open, they pack downward.
			const auto placed   = static_cast<std::size_t>(std::ranges::count_if(Cards, &thread_card::Placed));
			auto       tops     = std::vector<int>(Cards.size(), 0);
			const auto target   = [this](const std::size_t aIndex) { return Cards[aIndex].TargetY + note_nudge; };
			const auto packDown = [&](const std::size_t aFrom) {
				for (auto index = aFrom; index < placed; ++index) {
					tops[index] = index == 0U ? target(index)
											  : std::max(target(index), tops[index - 1U] + Cards[index - 1U].Rect.height() + note_gap);
				}
			};
			const auto open = std::ranges::find(Cards.begin(), Cards.begin() + static_cast<std::ptrdiff_t>(placed), Open, &thread_card::Id);
			if (Open.empty() || open == Cards.begin() + static_cast<std::ptrdiff_t>(placed)) {
				packDown(0U);
			} else {
				const auto at = static_cast<std::size_t>(std::distance(Cards.begin(), open));
				tops[at]      = target(at);
				for (auto index = at; index-- > 0U;) {
					tops[index] = std::min(target(index), tops[index + 1U] - note_gap - Cards[index].Rect.height());
				}
				packDown(at + 1U);
			}
			auto bottom = placed == 0U ? 4 : tops[placed - 1U] + Cards[placed - 1U].Rect.height();
			for (auto index = placed; index < Cards.size(); ++index) {
				// Unplaceable threads follow the placed ones.
				tops[index] = bottom + (index == 0U ? 0 : note_gap);
				bottom      = tops[index] + Cards[index].Rect.height();
			}
			auto lowest = 0;
			for (std::size_t index = 0U; index < Cards.size(); ++index) {
				Cards[index].Rect.moveTop(tops[index]);
				lowest = std::max(lowest, Cards[index].Rect.bottom());
			}
			// The notes take only what the code cannot give them: the foot of a
			// thread taller than what is left below its line.
			ContentHeight = lowest + note_margin;
			verticalScrollBar()->setRange(0, std::max(0, ContentHeight - viewport()->height()));
			verticalScrollBar()->setPageStep(viewport()->height());
			verticalScrollBar()->setSingleStep(fontMetrics().lineSpacing());
		} else {
			auto bottom   = 4;
			auto lastPath = QString{ };
			auto first    = true;
			for (auto& card : Cards) {
				if (card.Path != lastPath || first) {
					bottom += path_row_height;
					lastPath = card.Path;
				}
				const auto top = bottom + (first ? 0 : note_gap);
				card.Rect.moveTop(top);
				bottom = card.Rect.bottom();
				first  = false;
			}
			ContentHeight = bottom + note_margin;
			verticalScrollBar()->setRange(0, std::max(0, ContentHeight - viewport()->height()));
			verticalScrollBar()->setPageStep(viewport()->height());
			verticalScrollBar()->setSingleStep(fontMetrics().lineSpacing());
		}
		place_open_card();
		sync_edges();
		viewport()->update();
	}

	void conversation_cards_view::place_open_card()
	{
		if (OpenCard == nullptr) {
			return;
		}
		const auto index = card_index(Open);
		if (!index.has_value()) {
			OpenCard->hide();
			return;
		}
		const auto& card = Cards[*index];
		const auto  rect = to_viewport(card.Rect);
		if (OpenCard->geometry() != rect) {
			OpenCard->setGeometry(rect);
		}
		const auto notch = card.Placed ? std::clamp(card.TargetY - card.Rect.top() + 10, 12, std::max(12, card.Rect.height() - 12)) : 17;
		OpenCard->set_notch(notch);
		if (!OpenCard->isVisible()) {
			OpenCard->show();
		}
	}

	void conversation_cards_view::sync_edges()
	{
		auto above = std::vector<std::size_t>{ };
		auto below = std::vector<std::size_t>{ };
		if (aligned()) {
			const auto height = viewport()->height();
			for (std::size_t index = 0U; index < Cards.size(); ++index) {
				const auto rect = to_viewport(Cards[index].Rect);
				if (rect.bottom() < edge_reach) {
					above.push_back(index);
				} else if (rect.top() > height - edge_reach) {
					below.push_back(index);
				}
			}
			// Nearest first.
			std::ranges::reverse(above);
		}
		const auto colours   = note_colours_of(palette(), Accent, Marker);
		auto       used      = std::size_t{ 0U };
		const auto layoutRow = [&](const std::vector<std::size_t>& aList, const int aY, const bool aDown) {
			if (aList.empty()) {
				return;
			}
			auto       x     = note_margin + 20;
			const auto right = viewport()->width() - note_margin;
			for (std::size_t shown = 0U; shown <= std::min(chips_each_way, aList.size()); ++shown) {
				const auto more = shown == chips_each_way || shown == aList.size();
				if (more && shown == aList.size()) {
					break;
				}
				if (used == Chips.size()) {
					auto* const chip = new edge_chip{ viewport() };
					connect(chip, &QAbstractButton::clicked, this, [this, chip] {
						const auto id = static_cast<edge_chip*>(chip)->Thread;
						open_thread(id);
						if (ActionDispatch && !Feed) {
							ActionDispatch(card_action::show_line, id);
						}
					});
					chip->installEventFilter(this);
					Chips.push_back(chip);
				}
				auto* const chip = static_cast<edge_chip*>(Chips[used++]);
				chip->Marks      = MarkTheme;
				chip->Colours    = colours;
				chip->More       = more;
				if (more) {
					const auto rest = aList.size() - chips_each_way;
					chip->Thread    = Cards[aList[chips_each_way]].Id;
					chip->Label.reset();
					chip->Draft  = false;
					chip->Unread = false;
					set_if_changed(chip->text(), [chip](const QString& aValue) { chip->setText(aValue); }, QStringLiteral("+%1").arg(rest));
					auto lines = QStringList{ };
					for (auto at = chips_each_way; at < aList.size(); ++at) {
						lines.push_back(QStringLiteral("Line %1").arg(Cards[aList[at]].Line.value_or(0U)));
					}
					set_if_changed(chip->toolTip(), [chip](const QString& aValue) { chip->setToolTip(aValue); }, lines.join(QStringLiteral(", ")));
					set_if_changed(chip->accessibleName(), [chip](const QString& aValue) { chip->setAccessibleName(aValue); }, QStringLiteral("%1 more threads %2").arg(rest).arg(aDown ? QStringLiteral("below") : QStringLiteral("above")));
				} else {
					const auto& card = Cards[aList[shown]];
					chip->Thread     = card.Id;
					chip->Label      = card.Posts.empty() ? std::nullopt : card.Posts.front().Label;
					chip->Emphasis   = card.Posts.empty() ? feedback_mark_emphasis{ } : card.Posts.front().Emphasis;
					chip->Draft      = card.Draft;
					chip->Unread     = card.UnreadCount > 0U;
					set_if_changed(chip->text(), [chip](const QString& aValue) { chip->setText(aValue); }, QString::number(card.Line.value_or(0U)));
					const auto author = card.Posts.empty() ? QString{ } : (card.Posts.front().Mine ? QStringLiteral("you") : card.Posts.front().Author);
					auto       first  = card.Posts.empty() ? QString{ } : card.Posts.front().Body;
					first.replace(QLatin1Char{ '`' }, QString{ });
					first.replace(QLatin1Char{ '\n' }, QLatin1Char{ ' ' });
					set_if_changed(chip->toolTip(), [chip](const QString& aValue) { chip->setToolTip(aValue); }, QStringLiteral("Line %1, %2: %3").arg(card.Line.value_or(0U)).arg(author, first.size() > 70 ? first.left(70) + QChar{ 0x2026 } : first));
					set_if_changed(chip->accessibleName(), [chip](const QString& aValue) { chip->setAccessibleName(aValue); }, QStringLiteral("Thread on line %1, %2%3, %4").arg(card.Line.value_or(0U)).arg(author).arg(card.UnreadCount > 0U ? QStringLiteral(", %1 new").arg(card.UnreadCount) : QString{ }).arg(aDown ? QStringLiteral("below") : QStringLiteral("above")));
				}
				chip->updateGeometry();
				const auto size = chip->sizeHint();
				if (x + size.width() > right && shown > 0U) {
					chip->hide();
					--used;
					break;
				}
				chip->setGeometry(x, aY, size.width(), chip_height);
				chip->show();
				chip->raise();
				chip->update();
				x += size.width() + 4;
				if (more) {
					break;
				}
			}
		};
		for (auto* const band : { &BandAbove, &BandBelow }) {
			if (*band == nullptr) {
				*band = new edge_fade{ band == &BandBelow, viewport() };
			}
			static_cast<edge_fade*>(*band)->Colours = colours;
		}
		BandAbove->setGeometry(0, 0, viewport()->width(), edge_band);
		BandBelow->setGeometry(0, viewport()->height() - edge_band, viewport()->width(), edge_band);
		BandAbove->setVisible(!above.empty());
		BandBelow->setVisible(!below.empty());
		// Over the open thread, under the chips.
		BandAbove->raise();
		BandBelow->raise();
		BandAbove->update();
		BandBelow->update();
		layoutRow(above, (edge_band - chip_height) / 2, false);
		layoutRow(below, viewport()->height() - edge_band + (edge_band - chip_height) / 2, true);
		ChipsShown = used;
		for (auto index = used; index < Chips.size(); ++index) {
			Chips[index]->hide();
		}
	}

	// -------------------------------------------------------------- painting

	void conversation_cards_view::changeEvent(QEvent* const aEvent)
	{
		QAbstractScrollArea::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange || aEvent->type() == QEvent::PaletteChange) {
			clear_measurements();
			if (OpenCard != nullptr) {
				OpenCard->setProperty("threadKey", QString{ });
			}
			relayout();
		}
	}

	void conversation_cards_view::paint_closed(QPainter& aPainter, const thread_card& aCard, const bool aHovered, const bool aFocused) const
	{
		const auto  colours = note_colours_of(palette(), Accent, Marker);
		const auto  rect    = to_viewport(aCard.Rect);
		const auto  box     = QRectF{ rect }.adjusted(0.5, 0.5, -0.5, -0.5);
		const auto* measure = closed_measure_of(aCard);
		aPainter.save();
		if (aCard.Resolved && !aHovered && !aFocused) {
			aPainter.setOpacity(0.62);
		}
		if (aHovered || aFocused) {
			aPainter.setPen(aFocused ? QPen{ colours.Signal, 2.0 } : QPen{ colours.Rule, 1.0 });
			aPainter.setBrush(colours.Panel);
			aPainter.drawRoundedRect(box, 8.0, 8.0);
		}
		if (aCard.Draft) {
			aPainter.setPen(QPen{ colours.Rule2, 1.5, Qt::DashLine });
			aPainter.setBrush(Qt::NoBrush);
			aPainter.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);
		}
		if (aCard.Posts.empty()) {
			aPainter.restore();
			return;
		}
		const auto& root = aCard.Posts.front();
		const auto  rail = rail_rect(aCard);
		paint_feedback_rail(aPainter, rail.right(), rail.top(), rail.bottom(),
							root.Label.has_value() ? MarkTheme.Tints[static_cast<std::size_t>(*root.Label)] : colours.Rule2, root.Emphasis);
		paint_avatar(aPainter, QRectF{ static_cast<qreal>(rect.left() + gut_left), static_cast<qreal>(rect.top() + note_top + 1), avatar_size, avatar_size },
					 root.Author, root.Mine, colours, font());
		if (const auto mark = mark_rect(aCard); !mark.isEmpty()) {
			paint_feedback_mark(aPainter, QRectF{ mark }, *root.Label, root.Emphasis,
								feedback_mark_colours{ .Tint = MarkTheme.Tints[static_cast<std::size_t>(*root.Label)], .Panel = colours.Panel, .Ink = colours.Ink, .Style = MarkTheme.Style });
		}
		const auto bold  = semibold(font());
		const auto small = scaled(font(), 0.92);
		const auto nameH = name_height(font());
		const auto top   = rect.top() + note_top;
		const auto left  = rect.left() + text_left;
		auto       right = rect.right() - note_right;
		// The meta at the right: who replied and how often, what is new, and
		// whether it is resolved; my draft says so instead.
		aPainter.setFont(small);
		const auto smallMetrics = QFontMetrics{ small };
		if (aCard.Draft) {
			const auto words = QStringLiteral("Draft");
			const auto w     = smallMetrics.horizontalAdvance(words);
			aPainter.setPen(colours.Faint);
			aPainter.drawText(QRect{ right - w, top, w + 1, nameH }, Qt::AlignRight | Qt::AlignVCenter, words);
			paint_conversation_glyph(aPainter, QRectF{ static_cast<qreal>(right - w - 16), top + (nameH - 12) / 2.0, 12.0, 12.0 }, conversation_glyph::lock, colours.Faint);
			right -= w + 20;
		} else {
			if (aCard.Resolved) {
				paint_conversation_glyph(aPainter, QRectF{ static_cast<qreal>(right - 14), top + (nameH - 14) / 2.0, 14.0, 14.0 }, conversation_glyph::resolved, colours.Ok);
				right -= 14 + 6;
			}
			if (aCard.UnreadCount > 0U) {
				const auto count = QString::number(aCard.UnreadCount);
				auto       pill  = semibold(scaled(font(), 0.8));
				pill.setWeight(QFont::Bold);
				const auto w       = std::max(16, QFontMetrics{ pill }.horizontalAdvance(count) + 10);
				const auto pillBox = QRectF{ static_cast<qreal>(right - w), top + (nameH - 16) / 2.0, static_cast<qreal>(w), 16.0 };
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(colours.New);
				aPainter.drawRoundedRect(pillBox, 8.0, 8.0);
				aPainter.setFont(pill);
				aPainter.setPen(colours.OnNew);
				aPainter.drawText(pillBox, Qt::AlignCenter, count);
				aPainter.setFont(small);
				right -= w + 6;
			}
			const auto replies = aCard.Posts.size() > 1U ? aCard.Posts.size() - 1U : 0U;
			if (replies > 0U) {
				const auto count = QString::number(replies);
				const auto w     = smallMetrics.horizontalAdvance(count);
				aPainter.setPen(colours.Faint);
				aPainter.drawText(QRect{ right - w, top, w + 1, nameH }, Qt::AlignRight | Qt::AlignVCenter, count);
				right -= w + 3;
				paint_conversation_glyph(aPainter, QRectF{ static_cast<qreal>(right - 12), top + (nameH - 12) / 2.0, 12.0, 12.0 }, conversation_glyph::reply, colours.Faint);
				right -= 12 + 3;
				// Up to three of the people who replied, overlapping.
				auto people = std::vector<const card_post*>{ };
				for (auto post = std::next(aCard.Posts.begin()); post != aCard.Posts.end() && people.size() < 3U; ++post) {
					if (std::ranges::none_of(people, [&post](const card_post* aSeen) { return aSeen->Author == post->Author; })) {
						people.push_back(&*post);
					}
				}
				const auto span = 14 + static_cast<int>(people.size() - 1U) * 9;
				auto       x    = right - span;
				for (const auto* person : people) {
					const auto face = QRectF{ static_cast<qreal>(x), top + (nameH - 14) / 2.0, 14.0, 14.0 };
					aPainter.setPen(QPen{ aHovered || aFocused ? colours.Panel : colours.Drawer, 1.5 });
					aPainter.setBrush(Qt::NoBrush);
					aPainter.drawEllipse(face.adjusted(-0.75, -0.75, 0.75, 0.75));
					paint_avatar(aPainter, face, person->Author, false, colours, font());
					x += 9;
				}
				right -= span + 6;
			}
		}
		aPainter.setFont(bold);
		aPainter.setPen(colours.Ink);
		const auto who = aCard.Draft ? QStringLiteral("Your note") : (root.Mine ? QStringLiteral("you") : root.Author);
		aPainter.drawText(QRect{ left, top, std::max(0, right - left), nameH }, Qt::AlignLeft | Qt::AlignVCenter,
						  QFontMetrics{ bold }.elidedText(who, Qt::ElideRight, std::max(0, right - left)));
		if (measure != nullptr && measure->Lines > 0) {
			const auto display = clamp_lines(display_of(root, font(), CodeFont, colours.Hover), font(), std::max(40, rect.width() - text_left - note_right), measure->Lines);
			auto       layout  = QTextLayout{ display.Text, font() };
			layout.setFormats(display.Formats);
			auto option = QTextOption{ };
			option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
			layout.setTextOption(option);
			layout.beginLayout();
			auto y     = 0.0;
			auto lines = 0;
			for (auto line = layout.createLine(); line.isValid() && lines < measure->Lines; line = layout.createLine()) {
				line.setLineWidth(std::max(40, rect.width() - text_left - note_right));
				line.setPosition(QPointF{ 0.0, y + (body_line_height(font()) - line.height()) / 2.0 });
				y += body_line_height(font());
				++lines;
			}
			layout.endLayout();
			aPainter.setPen(colours.Ink);
			aPainter.setClipRect(QRect{ left, top + nameH + 1, rect.width() - text_left - note_right + 2, lines * body_line_height(font()) + 2 }, Qt::IntersectClip);
			layout.draw(&aPainter, QPointF{ static_cast<qreal>(left), static_cast<qreal>(top + nameH + 1) });
		}
		aPainter.restore();
	}

	void conversation_cards_view::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ viewport() };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto colours  = note_colours_of(palette(), Accent, Marker);
		const auto small    = scaled(font(), 0.92);
		auto       lastPath = QString{ };
		auto       first    = true;
		for (const auto& card : Cards) {
			const auto rect = to_viewport(card.Rect);
			if (!aligned() && (card.Path != lastPath || first)) {
				lastPath = card.Path;
				painter.setFont(semibold(small));
				painter.setPen(colours.Faint);
				painter.drawText(QRect{ note_margin + 4, rect.top() - path_row_height - (first ? 0 : note_gap) + 2, width() - 2 * note_margin, path_row_height },
								 Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics{ painter.font() }.elidedText(card.Path.isEmpty() ? QStringLiteral("Pull request") : card.Path, Qt::ElideMiddle, width() - 2 * note_margin));
			}
			first = false;
			if (rect.bottom() < 0 || rect.top() > viewport()->height()) {
				continue;
			}
			if (OpenCard != nullptr && card.Id == Open) {
				// The open card draws itself; its shadow and the notch that
				// points at its line are drawn here, behind and beside it.
				painter.save();
				painter.setPen(Qt::NoPen);
				for (auto ring = 1; ring <= 4; ++ring) {
					auto shade = QColor{ 0, 0, 0 };
					shade.setAlphaF(static_cast<float>((colours.Dark ? 0.16 : 0.05) / ring));
					painter.setBrush(shade);
					painter.drawRoundedRect(QRectF{ rect }.adjusted(-ring, -ring + 2.0, ring, ring + 3.0), 8.0 + ring, 8.0 + ring);
				}
				if (card.Placed) {
					const auto y     = static_cast<qreal>(rect.top() + OpenCard->notch());
					auto       notch = QPainterPath{ };
					notch.moveTo(rect.left() - 7.0, y);
					notch.lineTo(rect.left() + 0.5, y - 6.0);
					notch.lineTo(rect.left() + 0.5, y + 6.0);
					notch.closeSubpath();
					painter.setBrush(colours.Route);
					painter.drawPath(notch);
				}
				painter.restore();
				continue;
			}
			const auto focused = HasFocus && card.Id == Current && Open != card.Id;
			paint_closed(painter, card, card.Id == Hovered, focused);
		}
		if (Cards.empty()) {
			painter.setFont(font());
			painter.setPen(colours.Faint);
			painter.drawText(QRect{ note_margin + 4, note_margin + 4, width() - 2 * note_margin - 8, 60 },
							 Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
							 Filters.UnreadOnly || Filters.UnresolvedOnly
								 ? QStringLiteral("No threads match these filters.")
								 : Scope == cards_scope::this_file ? QStringLiteral("No threads on this file.")
																   : QStringLiteral("No threads on this pull request."));
		}
	}

	void conversation_cards_view::set_label_marks(const feedback_mark_theme& aTheme)
	{
		MarkTheme = aTheme;
		if (OpenCard != nullptr) {
			OpenCard->setProperty("threadKey", QString{ });
		}
		rebuild_open_card();
		place_cards(true);
		viewport()->update();
	}

	auto conversation_cards_view::label_marks() const noexcept -> const feedback_mark_theme&
	{
		return MarkTheme;
	}

	auto conversation_cards_view::mark_rect(const thread_card& aCard) const -> QRect
	{
		if (aCard.Posts.empty() || !aCard.Posts.front().Label.has_value() || aCard.Rect.isEmpty()) {
			return { };
		}
		const auto rect = to_viewport(aCard.Rect);
		// Centred under the 18 px avatar: one pixel either side at any whole scale.
		return QRect{ rect.left() + gut_left + (avatar_size - mark_size) / 2, rect.top() + note_top + 1 + avatar_size + mark_gap, mark_size, mark_size };
	}

	auto conversation_cards_view::rail_rect(const thread_card& aCard) const -> QRectF
	{
		const auto width = aCard.Posts.empty() ? 2.0 : feedback_rail_width(aCard.Posts.front().Emphasis);
		const auto rect  = to_viewport(aCard.Rect);
		// The rail's right edge stays put, so blocking grows it outward.
		return QRectF{ rect.left() + rail_right - width, static_cast<qreal>(rect.top() + note_top + 3), width,
					   static_cast<qreal>(std::max(0, rect.height() - note_top - note_bottom - 6)) };
	}

	auto conversation_cards_view::viewportEvent(QEvent* const aEvent) -> bool
	{
		if (aEvent->type() == QEvent::ToolTip) {
			const auto* const help  = static_cast<QHelpEvent*>(aEvent);
			const auto        point = help->pos();
			for (const auto& card : Cards) {
				if (card.Id == Open || card.Posts.empty() || !to_viewport(card.Rect).contains(point)) {
					continue;
				}
				// The name sits right below the mark it names ([[BPR-1083]]
				// v004), also when the rail is hovered.
				const auto mark = mark_rect(card);
				if (mark.contains(point) || rail_rect(card).adjusted(-3.0, 0.0, 3.0, 0.0).contains(QPointF{ point })) {
					show_anchored_tooltip(*viewport(), mark.isEmpty() ? rail_rect(card).toAlignedRect() : mark, card.Posts.front().Name);
					return true;
				}
			}
			hide_anchored_tooltip();
			aEvent->ignore();
			return true;
		}
		if (aEvent->type() == QEvent::Leave) {
			set_hovered({ });
		}
		return QAbstractScrollArea::viewportEvent(aEvent);
	}

	void conversation_cards_view::resizeEvent(QResizeEvent* const aEvent)
	{
		QAbstractScrollArea::resizeEvent(aEvent);
		relayout();
	}

	// ------------------------------------------------------------- gestures

	void conversation_cards_view::choose(const std::size_t aCard, const bool aOpen)
	{
		if (aCard >= Cards.size()) {
			return;
		}
		if (Cards[aCard].Draft) {
			if (aOpen) {
				Open = Cards[aCard].Id;
				relayout();
			}
			return;
		}
		const auto chosen = Cards[aCard].Id;
		Current           = chosen;
		if (aOpen && Open != chosen) {
			Open = chosen;
			if (Replying != chosen) {
				Replying.clear();
			}
			relayout();
		}
		if (!aligned()) {
			if (const auto index = card_index(chosen); index.has_value()) {
				ensure_visible(to_viewport(Cards[*index].Rect));
			}
		}
		viewport()->update();
		if (ThreadDispatch) {
			// [[BPR-1029]]. The dispatch may make another thread current, so it
			// is handed its own copy of the identity rather than a reference
			// into state it can change.
			ThreadDispatch(chosen, aOpen);
		}
	}

	void conversation_cards_view::open_thread(const std::string& aId)
	{
		if (const auto index = card_index(aId); index.has_value()) {
			choose(*index, true);
		}
	}

	void conversation_cards_view::toggle_thread(const std::string& aId)
	{
		if (!aId.empty() && Open == aId) {
			close_thread();
		} else {
			open_thread(aId);
		}
	}

	void conversation_cards_view::close_thread()
	{
		if (Open.empty()) {
			return;
		}
		Open.clear();
		Replying.clear();
		verticalScrollBar()->setValue(Feed ? verticalScrollBar()->value() : 0);
		relayout();
	}

	void conversation_cards_view::begin_reply(const std::string& aThread)
	{
		if (Open != aThread) {
			if (const auto index = card_index(aThread); index.has_value()) {
				Open    = aThread;
				Current = aThread;
				relayout();
			}
		}
		if (OpenCard == nullptr || Open != aThread) {
			return;
		}
		Replying = aThread;
		OpenCard->set_replying(true);
		OpenCard->present_reply(Reply);
		if (auto* const editor = OpenCard->reply_editor(); editor != nullptr) {
			editor->setFocus(Qt::OtherFocusReason);
			place_cards(true);
			ensure_visible(QRect{ editor->mapTo(viewport(), QPoint{ 0, 0 }), editor->size() });
		}
	}

	void conversation_cards_view::present_reply(std::optional<thread_reply_presentation> aReply)
	{
		if (Reply == aReply) {
			return;
		}
		Reply = std::move(aReply);
		if (OpenCard != nullptr) {
			OpenCard->present_reply(Reply);
		}
	}

	void conversation_cards_view::ensure_visible(const QRect& aViewportRect)
	{
		auto* const bar = verticalScrollBar();
		if (aViewportRect.bottom() > viewport()->height()) {
			bar->setValue(bar->value() + aViewportRect.bottom() - viewport()->height() + note_margin);
		} else if (aViewportRect.top() < 0 && !aligned()) {
			bar->setValue(bar->value() + aViewportRect.top() - note_margin);
		}
	}

	void conversation_cards_view::set_hovered(const std::string& aId)
	{
		if (Hovered == aId) {
			return;
		}
		Hovered = aId;
		viewport()->update();
		notify_marks();
	}

	void conversation_cards_view::notify_marks()
	{
		if (MarksObserver) {
			MarksObserver();
		}
	}

	void conversation_cards_view::step_new(const int aDirection)
	{
		auto unread = std::vector<const thread_card*>{ };
		for (const auto& card : Cards) {
			if (!card.Draft && card.UnreadCount > 0U) {
				unread.push_back(&card);
			}
		}
		if (unread.empty()) {
			show_anchored_tooltip(*viewport(), QRect{ note_margin, 0, viewport()->width() - 2 * note_margin, 4 }, QStringLiteral("Nothing new in this file."));
			return;
		}
		std::ranges::stable_sort(unread, [](const thread_card* aLeft, const thread_card* aRight) {
			return std::pair{ aLeft->Line.value_or(0U), aLeft->ProjectionOrder } < std::pair{ aRight->Line.value_or(0U), aRight->ProjectionOrder };
		});
		const auto  from = card_index(Open.empty() ? Current : Open);
		const auto  line = from.has_value() ? Cards[*from].Line.value_or(0U) : 0U;
		const auto* next = aDirection > 0 ? unread.front() : unread.back();
		if (aDirection > 0) {
			if (const auto found = std::ranges::find_if(unread, [line](const thread_card* aCard) { return aCard->Line.value_or(0U) > line; }); found != unread.end()) {
				next = *found;
			}
		} else {
			for (auto at = unread.rbegin(); at != unread.rend(); ++at) {
				if ((*at)->Line.value_or(0U) < line) {
					next = *at;
					break;
				}
			}
		}
		const auto id = next->Id;
		open_thread(id);
		if (ActionDispatch && !Feed) {
			ActionDispatch(card_action::show_line, id);
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
		setFocus(Qt::MouseFocusReason);
		choose(*hit, true);
		aEvent->accept();
	}

	void conversation_cards_view::mouseMoveEvent(QMouseEvent* const aEvent)
	{
		const auto hit = hit_test(aEvent->position().toPoint());
		set_hovered(hit.has_value() && Cards[*hit].Id != Open ? Cards[*hit].Id : std::string{ });
		viewport()->setCursor(hit.has_value() && Cards[*hit].Id != Open ? Qt::PointingHandCursor : Qt::ArrowCursor);
		QAbstractScrollArea::mouseMoveEvent(aEvent);
	}

	void conversation_cards_view::wheelEvent(QWheelEvent* const aEvent)
	{
		if (!aligned()) {
			QAbstractScrollArea::wheelEvent(aEvent);
			return;
		}
		// [[BPR-1290]]. The drawer moves with the code: a wheel over the notes
		// scrolls the code, and only what the code cannot take scrolls the
		// notes' own overflow.
		auto* const bar   = verticalScrollBar();
		const auto  delta = !aEvent->pixelDelta().isNull() ? aEvent->pixelDelta().y() : aEvent->angleDelta().y() / 8 * 3;
		if (delta > 0 && bar->value() > bar->minimum()) {
			bar->setValue(bar->value() - delta);
			aEvent->accept();
			return;
		}
		if (ScrollForward && ScrollForward(*aEvent)) {
			aEvent->accept();
			return;
		}
		if (delta < 0) {
			bar->setValue(bar->value() - delta);
		}
		aEvent->accept();
	}

	void conversation_cards_view::focusInEvent(QFocusEvent* const aEvent)
	{
		QAbstractScrollArea::focusInEvent(aEvent);
		HasFocus = true;
		if (Current.empty() || !card_index(Current).has_value()) {
			const auto first = std::ranges::find_if(Cards, [](const thread_card& aCard) { return !aCard.Draft; });
			if (first != Cards.end()) {
				Current = first->Id;
			}
		}
		viewport()->update();
	}

	void conversation_cards_view::focusOutEvent(QFocusEvent* const aEvent)
	{
		QAbstractScrollArea::focusOutEvent(aEvent);
		HasFocus = false;
		viewport()->update();
	}

	auto conversation_cards_view::handle_thread_key(QKeyEvent& aEvent, const bool aTyping) -> bool
	{
		const auto plain = !aEvent.modifiers().testAnyFlags(Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier);
		if (aTyping) {
			return false;
		}
		switch (aEvent.key()) {
		case Qt::Key_Escape:
			if (!Open.empty()) {
				const auto open = Open;
				close_thread();
				Current = open;
				setFocus(Qt::OtherFocusReason);
				viewport()->update();
				return true;
			}
			return false;
		case Qt::Key_R:
			if (plain && !aEvent.modifiers().testFlag(Qt::ShiftModifier) && !Open.empty() && Open != draft_card_id && ActionDispatch) {
				const auto open = Open;
				ActionDispatch(card_action::reply, open);
				return true;
			}
			return false;
		case Qt::Key_N:
			if (plain) {
				step_new(aEvent.modifiers().testFlag(Qt::ShiftModifier) ? -1 : 1);
				return true;
			}
			return false;
		default:
			return false;
		}
	}

	void conversation_cards_view::keyPressEvent(QKeyEvent* const aEvent)
	{
		if (aEvent->key() == Qt::Key_Menu || (aEvent->key() == Qt::Key_F10 && aEvent->modifiers().testFlag(Qt::ShiftModifier))) {
			show_thread_menu(Current);
			aEvent->accept();
			return;
		}
		if (handle_thread_key(*aEvent, false)) {
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
				choose(position == threads.end() || std::next(position) == threads.end() ? threads.front() : *std::next(position), false);
			}
			aEvent->accept();
			return;
		case Qt::Key_Up:
			if (!threads.empty()) {
				choose(position == threads.end() || position == threads.begin() ? threads.back() : *std::prev(position), false);
			}
			aEvent->accept();
			return;
		case Qt::Key_Return:
		case Qt::Key_Enter:
		case Qt::Key_Space:
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

	auto conversation_cards_view::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent->type() == QEvent::KeyPress && OpenCard != nullptr && (aWatched == OpenCard || OpenCard->isAncestorOf(qobject_cast<QWidget*>(aWatched)))) {
			auto&      key    = *static_cast<QKeyEvent*>(aEvent);
			auto*      editor = OpenCard->reply_editor();
			const auto typing = editor != nullptr && aWatched == editor;
			if (typing) {
				if ((key.key() == Qt::Key_Return || key.key() == Qt::Key_Enter) && key.modifiers().testFlag(Qt::ControlModifier)) {
					if (OpenCard->Action) {
						OpenCard->Action(card_action::publish_reply);
					}
					return true;
				}
				if (key.key() == Qt::Key_Escape) {
					// An open label completion is the label controls' to close
					// first ([[BPR-1083]]); the reply keeps its text and focus.
					if (const auto* const labels = OpenCard->label_controls(); labels != nullptr && labels->completion()->isVisible()) {
						return false;
					}
					if (editor->toPlainText().trimmed().isEmpty() && OpenCard->Action) {
						OpenCard->Action(card_action::cancel_reply);
					} else {
						setFocus(Qt::OtherFocusReason);
					}
					return true;
				}
				return false;
			}
			if (key.key() == Qt::Key_Up || key.key() == Qt::Key_Down) {
				setFocus(Qt::OtherFocusReason);
				keyPressEvent(&key);
				return true;
			}
			return handle_thread_key(key, false);
		}
		if (aEvent->type() == QEvent::Wheel && OpenCard != nullptr && OpenCard->isAncestorOf(qobject_cast<QWidget*>(aWatched))) {
			wheelEvent(static_cast<QWheelEvent*>(aEvent));
			return true;
		}
		if (aEvent->type() == QEvent::Wheel && aWatched == OpenCard) {
			wheelEvent(static_cast<QWheelEvent*>(aEvent));
			return true;
		}
		return QAbstractScrollArea::eventFilter(aWatched, aEvent);
	}
} // namespace bpr::qt
