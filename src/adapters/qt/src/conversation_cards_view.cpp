#include "bpr/qt/conversation_cards_view.hpp"

#include "bpr/core/comment_edit.hpp"
#include "bpr/core/review_ledger.hpp"
#include "bpr/core/workbench_interaction.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"

#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/thread_pick_gestures.hpp"

#include "bpr/core/feedback_conversation.hpp"
#include "bpr/core/thread_reply.hpp"

#include "conversation_file_identities.hpp"
#include "conversation_glyphs.hpp"
#include "conversation_open_card.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QFocusEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLayout>
#include <QLinearGradient>
#include <QListWidget>
#include <QLocale>
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
#include <QShowEvent>
#include <QTextEdit>
#include <QTextLayout>
#include <QTimer>
#include <QToolButton>
#include <QVariantAnimation>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
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

		[[nodiscard]] auto semibold(QFont aFont) -> QFont;
		[[nodiscard]] auto scaled(QFont aFont, qreal aFactor) -> QFont;

		/// Same pill geometry as Notes' top-edge count, with a public PR face.
		class pull_request_chip final : public QAbstractButton
		{
		public:
			explicit pull_request_chip(QWidget* aParent)
				: QAbstractButton{ aParent }
			{
				setFocusPolicy(Qt::StrongFocus);
				setCursor(Qt::PointingHandCursor);
				setFont(semibold(scaled(font(), 0.86)));
			}
			note_colours Colours;
			std::size_t  New  = 0U;
			bool         Face = true;
			/// [[BPR-1781]]. A quiet link: no pill until hovered or focused.
			bool Link = false;
			auto sizeHint() const -> QSize override
			{
				return { QFontMetrics{ font() }.horizontalAdvance(text()) + (Face ? 28 : 18) + (New > 0U ? 24 : 0), chip_height };
			}

		protected:
			void paintEvent(QPaintEvent*) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing);
				const auto box = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
				if (!Link || hasFocus() || underMouse()) {
					painter.setPen(QPen{ hasFocus() ? Colours.Signal : Colours.Rule2, hasFocus() ? 2.0 : 1.0 });
					painter.setBrush(Link && !hasFocus() ? Qt::NoBrush : QBrush{ Colours.Panel });
					painter.drawRoundedRect(box, height() / 2.0, height() / 2.0);
				}
				if (Face) {
					drawer_icon(drawer_glyph::pull_request, palette()).paint(&painter, QRect{ 5, 4, 14, 14 });
				}
				const auto left = Face ? 24 : 9;
				painter.setPen(Colours.Ink2);
				painter.drawText(QRect{ left, 0, width() - left - (New > 0U ? 24 : 0), height() }, Qt::AlignLeft | Qt::AlignVCenter, text());
				if (New > 0U) {
					const auto badge = QRectF{ static_cast<qreal>(width() - 22), 3.0, 18.0, 16.0 };
					painter.setPen(Qt::NoPen);
					painter.setBrush(Colours.New);
					painter.drawRoundedRect(badge, 8, 8);
					painter.setPen(Colours.OnNew);
					painter.drawText(badge, Qt::AlignCenter, QString::number(New));
				}
			}
		};

		/// [[BPR-1471]]. The green resolved check, and the slot every closed
		/// note keeps for it so what follows stands in one column.
		constexpr int resolved_size = 14;
		constexpr int resolved_slot = resolved_size + 6;

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

		/// The bold figure in a closed note's blue new-post count.
		[[nodiscard]] auto new_count_font(const QFont& aFont) -> QFont
		{
			auto pill = semibold(scaled(aFont, 0.8));
			pill.setWeight(QFont::Bold);
			return pill;
		}

		[[nodiscard]] auto body_line_height(const QFont& aFont) -> int
		{
			return static_cast<int>(std::lround(QFontMetricsF{ aFont }.height() * 1.38));
		}

		/// [[BPR-1374]]. One line of a note's notice, in the small size.
		[[nodiscard]] auto notice_line_height(const QFont& aFont) -> int
		{
			return static_cast<int>(std::lround(QFontMetricsF{ scaled(aFont, 0.92) }.height() * 1.3));
		}

		/// [[BPR-1374]]. How many lines a notice wraps to at a width. Notices
		/// are few, so it is laid out rather than estimated.
		[[nodiscard]] auto notice_line_count(const QString& aNotice, const QFont& aSmall, const int aWidth) -> int
		{
			auto layout = QTextLayout{ aNotice, aSmall };
			auto option = QTextOption{ };
			option.setWrapMode(QTextOption::WordWrap);
			layout.setTextOption(option);
			layout.beginLayout();
			auto lines = 0;
			for (auto line = layout.createLine(); line.isValid(); line = layout.createLine()) {
				line.setLineWidth(std::max(40, aWidth));
				++lines;
			}
			layout.endLayout();
			return lines;
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

		/// \brief [[BPR-1086]]. Where a closed note's words start. A published
		/// tour stop's start is its pinned source and generated heading, so its
		/// note reads from its narrative's first word; any other body from its
		/// first byte.
		[[nodiscard]] auto lead_of(const std::string_view aBody, const core::feedback_body_segment& aSegment) -> qsizetype
		{
			if (!aSegment.Managed || !aSegment.UserBody.has_value()) {
				return 0;
			}
			auto begin = std::min(aSegment.UserBody->Begin, aBody.size());
			while (begin < aSegment.UserBody->End && begin < aBody.size() && (aBody[begin] == ' ' || aBody[begin] == '\t' || aBody[begin] == '\r' || aBody[begin] == '\n')) {
				++begin;
			}
			return feedback_text_index(aBody, begin);
		}

		/// \brief How much of the body after its lead is its header, set in
		/// semibold. [[BPR-1086]]. A published tour stop's header sits inside
		/// its narrative, so it is counted from the narrative's first word.
		[[nodiscard]] auto header_length_of(const std::string_view aBody, const core::feedback_annotation& aAnnotation, const qsizetype aLead) -> qsizetype
		{
			if (aAnnotation.Status != core::feedback_prefix_status::recognized || !aAnnotation.Header.has_value()) {
				return 0;
			}
			return std::max<qsizetype>(0, feedback_text_index(aBody, aAnnotation.Header->End) - aLead);
		}
		[[nodiscard]] auto header_length_of(const std::string_view aBody) -> qsizetype
		{
			const auto segment = core::published_feedback_segment(aBody);
			return header_length_of(aBody, core::recognize_feedback_body(aBody, segment), lead_of(aBody, segment));
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
			auto body   = aPost.Body.mid(std::clamp<qsizetype>(aPost.Lead, 0, aPost.Body.size()));
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

	auto thread_reply_refusal_text(const core::thread_reply_refusal aRefusal) -> QString
	{
		// [[BPR-1746]]. GitHub's own two refusals, in plain words. The
		// others are not GitHub's to say and keep their own wording.
		switch (aRefusal) {
		case core::thread_reply_refusal::locked:              return QObject::tr("This conversation is locked. Only collaborators can reply.");
		case core::thread_reply_refusal::viewer_cannot_reply: return QObject::tr("GitHub doesn't let you reply in this thread.");
		case core::thread_reply_refusal::none:
		case core::thread_reply_refusal::not_a_review_thread:
		case core::thread_reply_refusal::anonymous_read:      return { };
		}
		return { };
	}

	auto suggestion_placement_of(const application::github_conversation_projection& aNode) -> core::suggestion_placement_observation
	{
		using subject = core::suggestion_subject;
		if (aNode.Kind == application::github_conversation_kind::issue_comment || aNode.Kind == application::github_conversation_kind::review) {
			return { .Subject = subject::conversation };
		}
		// GitHub's own facts, not the selected comparison's: a thread GitHub
		// stores as a file comment, or marks outdated, takes no suggestion,
		// wherever BPR draws it.
		if (aNode.RemoteSubject == application::github_conversation_remote_subject::file) {
			return { .Subject = subject::file };
		}
		if (aNode.Outdated || aNode.AnchorAvailability == application::github_conversation_anchor_availability::outdated_revision) {
			return { .Subject = subject::outdated_line };
		}
		const auto side = side_of(aNode);
		if (!side || !aNode.Line || *aNode.Line == 0U) {
			return { .Subject = subject::unavailable };
		}
		return { .Subject = subject::current_line, .Side = *side };
	}

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
		// [[BPR-1781]]. Beside the code the drawer is about code: the pull
		// request's own conversation is Overview's Comments tab, and this quiet
		// link is the way there. The window decides what it opens.
		PullRequestChip = new pull_request_chip{ viewport() };
		PullRequestChip->setObjectName(QStringLiteral("pullRequestCommentsChip"));
		static_cast<pull_request_chip*>(PullRequestChip)->Link = true;
		PullRequestChip->setAttribute(Qt::WA_Hover);
		connect(PullRequestChip, &QAbstractButton::clicked, this, [this] {
			if (PullRequestLinkDispatch) {
				PullRequestLinkDispatch(std::string{ });
			}
		});
		PullRequestChip->hide();
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
		if (at == Cards.end() || (at->Draft && (at->UnsentReply.has_value() || at->MovedSuggestion))) {
			return nullptr;
		}
		if (at->Draft) {
			// [[BPR-1581]]. My draft's menu: Discuss shows it in its composer.
			auto* menu = new QMenu{ this };
			menu->setObjectName(QStringLiteral("draftContextMenu"));
			menu->setToolTipsVisible(true);
			menu->setAttribute(Qt::WA_DeleteOnClose);
			auto* const show = menu->addAction(QStringLiteral("Show in Discuss"));
			show->setObjectName(QStringLiteral("draftShowInDiscussAction"));
			show->setEnabled(static_cast<bool>(ActionDispatch));
			show->setToolTip(QStringLiteral("Open this draft in Discuss's composer, with room to write"));
			connect(show, &QAction::triggered, menu, [this, identity = at->Id] {
				if (ActionDispatch) {
					ActionDispatch(card_action::show_in_discuss, identity);
				}
			});
			// [[BPR-1586]]. My draft's own row in Threads.
			auto* const threads = menu->addAction(QStringLiteral("Show in Threads"));
			threads->setObjectName(QStringLiteral("draftShowInThreadsAction"));
			threads->setEnabled(static_cast<bool>(ActionDispatch));
			threads->setToolTip(QStringLiteral("Show this draft's row in Threads"));
			connect(threads, &QAction::triggered, menu, [this, identity = at->Id] {
				if (ActionDispatch) {
					ActionDispatch(card_action::show_in_threads, identity);
				}
			});
			return menu;
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
		// [[BPR-1745]]. An outdated thread takes a reply by the thread.
		// [[BPR-1746]]. One GitHub refuses this viewer says why.
		add(card_action::reply, QStringLiteral("Reply"), (has_code_anchor(card) || card.ReplyOnThread) && card.ReplyRefusal == core::thread_reply_refusal::none,
			card.ReplyRefusal != core::thread_reply_refusal::none ? thread_reply_refusal_text(card.ReplyRefusal) : tr("This thread's line isn't in the code shown, so it can't take a reply here."));
		const auto readAction = core::conversation_read_action_for(card.UnreadCount > 0U,
																   std::ranges::any_of(card.Posts, [](const card_post& aPost) { return !aPost.Mine; }));
		add(readAction == core::conversation_read_action::mark_read ? card_action::mark_read : card_action::mark_unread,
			readAction == core::conversation_read_action::mark_read ? QStringLiteral("Mark read") : QStringLiteral("Mark unread"),
			ReadStateKnown && readAction != core::conversation_read_action::none,
			!ReadStateKnown ? tr("Read state is not available yet.") : tr("Every post in this conversation is yours."));
		menu->actions().back()->setObjectName(QStringLiteral("threadReadAction"));
		add(card.Resolved ? card_action::unresolve : card_action::resolve, card.Resolved ? QStringLiteral("Unresolve") : QStringLiteral("Resolve"),
			ResolutionOffered && card.Kind == QStringLiteral("Inline thread"), tr("Resolution is not available for this thread."));
		if (Feed) {
			// [[BPR-1581]]. The core's route decides where Open in Review
			// can go: any thread on a file, an outdated one among its file's
			// threads without a line.
			const auto route = core::route_workbench_thread(core::open_workbench_thread_intent{
				.Destination = core::workbench_thread_destination::review,
				.Origin      = core::workbench_thread_origin::discuss,
				.OnFile      = !card.Path.isEmpty(),
				.Placed      = has_code_anchor(card),
			});
			add(card_action::open_in_code, QStringLiteral("Open in Review"), route.Available, tr("This comment is not on a file; Review shows a file's threads beside its code."));
			menu->actions().back()->setObjectName(QStringLiteral("threadOpenInReviewAction"));
		} else {
			add(card_action::show_line, QStringLiteral("Show line"), has_code_anchor(card), tr("This thread's code anchor is unavailable."));
			// [[BPR-1581]]. The same thread with the rest of the conversation;
			// at its open card's picked post when it has one.
			const auto post = card.Id == Open && !Picked.empty() ? Picked : card.Id;
			auto*      show = menu->addAction(QStringLiteral("Show in Discuss"));
			show->setObjectName(QStringLiteral("threadShowInDiscussAction"));
			show->setEnabled(static_cast<bool>(ActionDispatch));
			show->setToolTip(QStringLiteral("Show this thread in Discuss, with the rest of the conversation"));
			connect(show, &QAction::triggered, menu, [this, post] { if (ActionDispatch) { ActionDispatch(card_action::show_in_discuss, post); } });
			// [[BPR-1586]]. The same thread's row in Threads.
			auto* threads = menu->addAction(QStringLiteral("Show in Threads"));
			threads->setObjectName(QStringLiteral("threadShowInThreadsAction"));
			threads->setEnabled(static_cast<bool>(ActionDispatch));
			threads->setToolTip(QStringLiteral("Show this thread's row in Threads"));
			connect(threads, &QAction::triggered, menu, [this, post] { if (ActionDispatch) { ActionDispatch(card_action::show_in_threads, post); } });
		}
		// [[BPR-1773]]. Ask the agent about a review thread on a file.
		if (card.Kind == QStringLiteral("Inline thread") && !card.Path.isEmpty() && !card.Posts.empty()) {
			add(card_action::ask_agent, QStringLiteral("Ask agent about this conversation"), true, { });
			menu->actions().back()->setObjectName(QStringLiteral("threadAskAgentAction"));
			menu->actions().back()->setToolTip(QStringLiteral("Open the Agent tab with this conversation and its lines attached to your next question"));
		}
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
		// [[BPR-1699]]. Mark all from the thread's author as read, for a
		// thread someone else started (a spammy bot's, typically). It reaches
		// past this thread, so it stands apart from the thread's own items.
		if (!card.Posts.empty() && !card.Posts.front().Mine && !card.Posts.front().Author.isEmpty()) {
			menu->addSeparator();
			const auto  login  = card.Posts.front().Author;
			auto* const author = add_author_read_action(*menu, login, unread_post_count_from(login), ReadStateKnown, false);
			author->setEnabled(author->isEnabled() && static_cast<bool>(AuthorReadDispatch));
			connect(author, &QAction::triggered, menu, [this, chosen = login.toStdString()] {
				const auto dispatch = AuthorReadDispatch;
				if (dispatch) {
					dispatch(chosen);
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
				return !roots.contains(aId) && !aId.starts_with(draft_card_id);
			});
			Retained = aConversations;
			index_retained();
		}
		AnchorFile = std::move(aFile);
		relayout();
	}

	void conversation_cards_view::clear()
	{
		PullRequestComments = false;
		NewConversationBoundary.clear();
		clear_measurements();
		Retained.clear();
		AnnotatedBodies.clear();
		RetainedMarks.clear();
		RetainedHeaders.clear();
		RetainedLeads.clear();
		index_retained();
		Draft.reset();
		Current.clear();
		Open.clear();
		Picked.clear();
		PickedNext.clear();
		Hovered.clear();
		Replying.clear();
		ReadState.clear();
		ReadStateByThread.clear();
		ReadStateKnown = false;
		UnreadThreads.reset();
		UnreadConversations.reset();
		PullRequestCounts.reset();
		verticalScrollBar()->setValue(0);
		relayout();
	}

	void conversation_cards_view::index_retained()
	{
		UnreadThreads.reset();
		UnreadConversations.reset();
		PullRequestCounts.reset();
		RetainedFiles = detail::conversation_file_identities(Retained);
		RetainedById.clear();
		PullRequestRoots.clear();
		ThreadOrigins.clear();
		PullRequestCounts.reset();
		auto threadRoots = std::unordered_map<std::string_view, std::size_t>{ };
		for (std::size_t index = 0; index < Retained.size(); ++index) {
			const auto& node = Retained[index];
			if (!node.ParentId && node.Path.empty() && !node.Deleted && (node.Kind == application::github_conversation_kind::issue_comment || node.Kind == application::github_conversation_kind::review)) {
				PullRequestRoots.push_back(index);
			}
			if (node.Kind == application::github_conversation_kind::review_thread && !node.ParentId) {
				threadRoots.emplace(node.Id, index);
			}
		}
		// [[BPR-1779]]. A thread was posted in its first comment's review, at
		// its first comment's time.
		for (const auto& node : Retained) {
			if (node.Kind != application::github_conversation_kind::review_comment || !node.ParentId) {
				continue;
			}
			if (const auto root = threadRoots.find(*node.ParentId); root != threadRoots.end()) {
				ThreadOrigins.try_emplace(root->second, node.ReviewId, node.CreatedAt);
			}
		}
		RetainedRoot.clear();
		RetainedDepth.clear();
		RetainedByFile.clear();
		const auto bodyOf = [](const application::github_conversation_projection& aNode) -> const std::string& {
			return aNode.ReviewerProse.has_value() ? *aNode.ReviewerProse : aNode.Body;
		};
		const auto reuseBodies = std::ranges::equal(Retained, AnnotatedBodies, { }, bodyOf);
		if (!Retained.empty() && !reuseBodies) {
			AnnotatedBodies.clear();
			RetainedMarks.clear();
			RetainedHeaders.clear();
			RetainedLeads.clear();
			AnnotatedBodies.reserve(Retained.size());
			RetainedMarks.reserve(Retained.size());
			RetainedHeaders.reserve(Retained.size());
			RetainedLeads.reserve(Retained.size());
			for (const auto& node : Retained) {
				// [[BPR-1083, BPR-1086]]. An annotation depends on the exact
				// accepted body, including a tour stop's narrative boundary.
				const auto& body       = bodyOf(node);
				const auto  segment    = core::published_feedback_segment(body);
				const auto  annotation = core::recognize_feedback_body(body, segment);
				const auto  recognized = annotation.Status == core::feedback_prefix_status::recognized && annotation.Label.has_value()
					&& annotation.Subject.has_value();
				AnnotatedBodies.push_back(body);
				RetainedMarks.push_back(recognized ? feedback_body_mark{ .Label    = annotation.Label,
																		 .Emphasis = feedback_mark_emphasis_of(annotation),
																		 .Name     = feedback_mark_name(annotation, body) }
												   : feedback_body_mark{ });
				const auto lead = lead_of(body, segment);
				RetainedLeads.push_back(lead);
				RetainedHeaders.push_back(recognized ? header_length_of(body, annotation, lead) : 0);
				++BodyAnnotations;
			}
		}
		RetainedById.reserve(Retained.size());
		for (std::size_t index = 0U; index < Retained.size(); ++index) {
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
		auto affects = noted(Draft) || noted(aDraft) || CurrentComparison != aCurrentComparison;
		// [[BPR-1212]]. While the draft is written in the drawer its composer
		// shows the text, so typing it changes no note either.
		if (Writing && Draft.has_value() && aDraft.has_value() && CurrentComparison == aCurrentComparison
			&& Draft->Id == aDraft->Id && Draft->Anchor == aDraft->Anchor && Draft->ReplyToThread == aDraft->ReplyToThread
			&& Draft->State == aDraft->State) {
			affects = false;
		}
		if (!noted(aDraft)) {
			forget_measurements_if([](const std::string& aId) { return aId == draft_card_id; });
		}
		Draft             = std::move(aDraft);
		CurrentComparison = std::move(aCurrentComparison);
		if (affects) {
			relayout();
		}
	}

	void conversation_cards_view::set_saved_drafts(std::vector<core::review_comment_draft> aDrafts)
	{
		set_saved_drafts(std::move(aDrafts), { });
	}

	void conversation_cards_view::set_saved_drafts(std::vector<core::review_comment_draft> aDrafts, std::vector<core::reply_draft_decision> aReplies)
	{
		if (SavedDrafts == aDrafts && UnsentReplies == aReplies) {
			return;
		}
		forget_measurements_if([](const std::string& aId) { return aId.starts_with(draft_card_id) && aId != draft_card_id; });
		// [[BPR-1374]]. A thread's note changes with the reply marked on it.
		if (UnsentReplies != aReplies) {
			forget_measurements_if([](const std::string& aId) { return aId.starts_with("review-thread:"); });
		}
		SavedDrafts   = std::move(aDrafts);
		UnsentReplies = std::move(aReplies);
		relayout();
	}

	auto conversation_cards_view::unsent_reply_notice(const core::reply_draft_standing aStanding) -> QString
	{
		switch (aStanding) {
		case core::reply_draft_standing::thread_resolved: return tr("The thread this replies to was resolved on GitHub.");
		case core::reply_draft_standing::thread_deleted:  return tr("The thread this replies to was deleted on GitHub.");
		case core::reply_draft_standing::thread_unknown:  return tr("GitHub did not say whether the thread this replies to is still there.");
		case core::reply_draft_standing::none:
		case core::reply_draft_standing::answerable:      break;
		}
		return { };
	}

	auto conversation_cards_view::kept_reply_identity(const core::review_draft_id aDraft) -> std::string
	{
		return std::string{ draft_card_id } + ":reply:" + std::to_string(aDraft.value());
	}

	void conversation_cards_view::set_moved_suggestions(std::vector<moved_suggestion_card> aMoved)
	{
		if (MovedSuggestions == aMoved) {
			return;
		}
		forget_measurements_if([](const std::string& aId) { return aId.starts_with(std::string{ draft_card_id } + ":moved:"); });
		MovedSuggestions = std::move(aMoved);
		relayout();
	}

	auto conversation_cards_view::moved_suggestion_identity(const core::review_draft_id aDraft) -> std::string
	{
		return std::string{ draft_card_id } + ":moved:" + std::to_string(aDraft.value());
	}

	auto conversation_cards_view::moved_suggestion_notice(const moved_suggestion_card& aMoved) -> QString
	{
		const auto lines = [](const std::uint32_t aFirst, const std::uint32_t aLast) {
			return aFirst == aLast ? tr("line %1").arg(aLast + 1U) : tr("lines %1–%2").arg(aFirst + 1U).arg(aLast + 1U);
		};
		const auto& draft   = aMoved.Draft;
		const auto  written = tr("Written on %1 at %2.").arg(aMoved.EarlierHead, lines(draft.StartLine.value_or(draft.Anchor.Line), draft.Anchor.Line));
		const auto& move    = aMoved.Move;
		if (move.Carry.has_value()) {
			return tr("%1 Those lines are unchanged here, at %2.").arg(written, lines(move.Carry->first, move.Carry->second));
		}
		switch (move.Refusal) {
		case core::suggestion_head_move_refusal::changed_where_edited:
			return tr("%1 The file changed where it was edited, so it is not carried. Copy the change to write it again.").arg(written);
		case core::suggestion_head_move_refusal::second_block:
			return tr("%1 Its words hold a suggestion of their own, so it does not open in the code. Copy the change to write it again.").arg(written);
		case core::suggestion_head_move_refusal::lines_unknown:
		case core::suggestion_head_move_refusal::none:
			break;
		}
		return tr("%1 Its lines could not be compared with this head, so it is not carried. Copy the change to write it again.").arg(written);
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
		if (Feed) {
			PullRequestComments = false;
			NewConversationBoundary.clear();
		}
		setVerticalScrollBarPolicy(Feed ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
		verticalScrollBar()->setValue(0);
		setAccessibleName(Feed ? QStringLiteral("Threads") : QStringLiteral("Threads in this file"));
		// The open card is rebuilt, and the old one deleted, on the next layout.
		if (OpenCard != nullptr) {
			OpenCard->setProperty("threadKey", QString{ });
		}
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

	void conversation_cards_view::present_reactions(std::map<std::string, core::reaction_summary> aBySubject)
	{
		if (ReactionsBySubject == aBySubject) {
			return;
		}
		ReactionsBySubject = std::move(aBySubject);
		if (OpenCard != nullptr) {
			OpenCard->present_reactions([this](const std::string& aPost) { return reactions_of(aPost); });
		}
	}

	auto conversation_cards_view::reactions_of(const std::string_view aPost) const -> std::optional<core::reaction_summary>
	{
		// A post is named by its kind and its GitHub node; reactions by the node.
		for (const auto prefix : { std::string_view{ "review-comment:" }, std::string_view{ "issue-comment:" }, std::string_view{ "review:" } }) {
			if (aPost.starts_with(prefix)) {
				const auto found = ReactionsBySubject.find(std::string{ aPost.substr(prefix.size()) });
				return found == ReactionsBySubject.end() ? std::nullopt : std::optional{ found->second };
			}
		}
		return std::nullopt;
	}

	void conversation_cards_view::set_reaction_dispatch(std::function<void(const std::string&, core::reaction_content)> aDispatch)
	{
		ReactionDispatch = std::move(aDispatch);
	}

	void conversation_cards_view::set_comment_edit_offered(const bool aOffered)
	{
		if (CommentEditOffered == aOffered) {
			return;
		}
		CommentEditOffered = aOffered;
		// [[BPR-1775]]. A withdrawn offer ends an edit; nothing is sent.
		if (!aOffered) {
			CommentEdit.reset();
		}
		rebuild_open_card();
	}

	auto conversation_cards_view::comment_edit_offered() const noexcept -> bool
	{
		return CommentEditOffered;
	}

	auto conversation_cards_view::comment_editable(const std::string_view aPost) const -> bool
	{
		if (!CommentEditOffered) {
			return false;
		}
		const auto node = std::ranges::find_if(Retained, [aPost](const auto& aNode) { return aNode.Id == aPost && !aNode.Deleted; });
		if (node == Retained.end()) {
			return false;
		}
		const auto mine = !Viewer.isEmpty() && text(node->AuthorLogin) == Viewer;
		return core::comment_edit_offered(aPost, mine, false);
	}

	void conversation_cards_view::set_comment_edit_dispatch(std::function<void(const comment_edit_save&)> aDispatch)
	{
		CommentEditDispatch = std::move(aDispatch);
	}

	auto conversation_cards_view::begin_comment_edit(const std::string& aPost) -> bool
	{
		if (!comment_editable(aPost)) {
			return false;
		}
		if (CommentEdit.has_value() && CommentEdit->Post == aPost) {
			present_comment_edit();
			return true;
		}
		if (CommentEdit.has_value() && CommentEdit->Saving) {
			return false;
		}
		const auto holder = std::ranges::find_if(Cards, [&aPost](const thread_card& aCard) {
			return !aCard.Draft && std::ranges::any_of(aCard.Posts, [&aPost](const card_post& aCardPost) { return aCardPost.Id == aPost; });
		});
		if (holder == Cards.end()) {
			return false;
		}
		// [[BPR-1775]]. GitHub's exact body as the read gave it, never the
		// prose the card shows: a save is compared with what GitHub has.
		const auto node = std::ranges::find_if(Retained, [&aPost](const auto& aNode) { return aNode.Id == aPost; });
		CommentEdit     = comment_edit_session{ .Post = aPost, .Original = node->Body, .Text = node->Body };
		if (Open != holder->Id) {
			open_thread(holder->Id);
		}
		present_comment_edit();
		return true;
	}

	auto conversation_cards_view::comment_editing() const -> std::optional<std::string>
	{
		return CommentEdit.has_value() ? std::optional{ CommentEdit->Post } : std::nullopt;
	}

	void conversation_cards_view::present_comment_edit_status(const QString& aText, const bool aSaving)
	{
		if (!CommentEdit.has_value()) {
			return;
		}
		CommentEdit->Status = aText;
		CommentEdit->Saving = aSaving;
		present_comment_edit();
	}

	void conversation_cards_view::offer_newer_comment(std::string aNewer)
	{
		if (!CommentEdit.has_value()) {
			return;
		}
		CommentEdit->Newer = std::move(aNewer);
		present_comment_edit();
	}

	void conversation_cards_view::close_comment_edit()
	{
		if (!CommentEdit.has_value()) {
			return;
		}
		const auto hadFocus = OpenCard != nullptr && OpenCard->comment_editor() != nullptr && OpenCard->comment_editor()->hasFocus();
		CommentEdit.reset();
		present_comment_edit();
		if (hadFocus) {
			setFocus(Qt::OtherFocusReason);
		}
	}

	auto conversation_cards_view::comment_edit_original() const -> std::string
	{
		return CommentEdit.has_value() ? CommentEdit->Original : std::string{ };
	}

	auto conversation_cards_view::comment_editor() const -> scoped_prose_editor*
	{
		return OpenCard != nullptr ? OpenCard->comment_editor() : nullptr;
	}

	auto conversation_cards_view::comment_edit_save_button() const -> QAbstractButton*
	{
		return OpenCard != nullptr ? OpenCard->comment_edit_save_button() : nullptr;
	}

	auto conversation_cards_view::comment_edit_cancel_button() const -> QAbstractButton*
	{
		return OpenCard != nullptr ? OpenCard->comment_edit_cancel_button() : nullptr;
	}

	auto conversation_cards_view::comment_edit_newer_button() const -> QAbstractButton*
	{
		return OpenCard != nullptr ? OpenCard->comment_edit_newer_button() : nullptr;
	}

	auto conversation_cards_view::comment_edit_status() const -> QLabel*
	{
		return OpenCard != nullptr ? OpenCard->comment_edit_status() : nullptr;
	}

	void conversation_cards_view::present_comment_edit()
	{
		if (OpenCard == nullptr) {
			return;
		}
		if (!CommentEdit.has_value()) {
			OpenCard->present_comment_edit(std::nullopt);
			return;
		}
		OpenCard->present_comment_edit(comment_edit_presentation{
			.Post   = CommentEdit->Post,
			.Text   = CommentEdit->Text,
			.Status = CommentEdit->Status,
			.Saving = CommentEdit->Saving,
			.Newer  = CommentEdit->Newer.has_value(),
		});
	}

	void conversation_cards_view::save_comment_edit()
	{
		if (!CommentEdit.has_value() || CommentEdit->Saving) {
			return;
		}
		const auto dispatch = CommentEditDispatch;
		if (dispatch) {
			dispatch(comment_edit_save{ .Post = CommentEdit->Post, .Original = CommentEdit->Original, .Body = CommentEdit->Text });
		}
	}

	void conversation_cards_view::present_thread_resolutions(std::map<std::string, thread_resolution_presentation> aByThread)
	{
		if (ResolutionsByThread == aByThread) {
			return;
		}
		ResolutionsByThread = std::move(aByThread);
		relayout();
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

	void conversation_cards_view::set_reference_dispatch(std::function<void(core::reference_page)> aDispatch)
	{
		ReferenceDispatch = std::move(aDispatch);
		if (OpenCard != nullptr && OpenCard->label_controls() != nullptr) {
			bind_reference(*OpenCard->label_controls());
		}
	}

	void conversation_cards_view::bind_reference(feedback_label_controls& aLabels)
	{
		if (!ReferenceDispatch) {
			aLabels.set_reference_dispatch({ });
			return;
		}
		aLabels.set_reference_dispatch([this](const core::reference_page aPage) {
			// The view's dispatch can be replaced while a card is open; ask
			// the current one.
			const auto dispatch = ReferenceDispatch;
			if (dispatch) {
				dispatch(aPage);
			}
		});
	}

	void conversation_cards_view::set_image_dispatch(std::function<void(const std::string&, std::vector<core::markdown_image_reference>)> aDispatch)
	{
		ImageDispatch = std::move(aDispatch);
	}

	void conversation_cards_view::set_suggestion_locator(card_suggestion_locator aLocator)
	{
		SuggestionLocator = std::move(aLocator);
	}

	void conversation_cards_view::set_code_theme(const markdown_code_theme& aTheme)
	{
		CodeTheme = aTheme;
		if (OpenCard != nullptr) {
			OpenCard->set_code_theme(aTheme);
		}
	}

	void conversation_cards_view::set_code_syntax_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch)
	{
		CodeDispatch = std::move(aDispatch);
	}

	void conversation_cards_view::apply_markdown_code_syntax(const application::markdown_code_syntax_publication& aSpans)
	{
		if (OpenCard != nullptr) {
			OpenCard->apply_code_syntax(aSpans);
		}
	}

	void conversation_cards_view::apply_markdown_image(const application::markdown_image_projection& aImage)
	{
		if (OpenCard != nullptr) {
			OpenCard->apply_image(aImage);
		}
	}

	void conversation_cards_view::set_images_shown(const bool aShown)
	{
		ImagesShown = aShown;
		if (OpenCard != nullptr) {
			OpenCard->set_images_shown(aShown);
		}
	}

	void conversation_cards_view::request_images()
	{
		if (OpenCard != nullptr) {
			OpenCard->request_images();
		}
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

	auto conversation_cards_view::unread_thread_count(const core::file_identity aFile) const -> std::size_t
	{
		static_cast<void>(unread_threads_in_file_order());
		const auto found = UnreadThreadsByFile.find(aFile);
		return found != UnreadThreadsByFile.end() ? found->second : 0U;
	}

	void conversation_cards_view::set_code_font(const QFont& aFont)
	{
		if (CodeFont == aFont) {
			return;
		}
		CodeFont = aFont;
		clear_measurements();
		// The open card is rebuilt, and the old one deleted, on the next layout.
		if (OpenCard != nullptr) {
			OpenCard->setProperty("threadKey", QString{ });
		}
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
		UnreadConversations.reset();
		PullRequestCounts.reset();
		relayout_read_state();
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
		if (UnreadConversations.has_value() && wasNew && now.empty()) {
			std::erase(*UnreadConversations, aThread);
		}
		if (UnreadThreads.has_value() && wasNew && now.empty()) {
			if (const auto removed = std::erase(*UnreadThreads, aThread); removed > 0U) {
				const auto index = RetainedById.at(aThread);
				UnreadThreadsByFile.at(RetainedFiles[index]) -= removed;
			}
		} else if (wasNew != !now.empty()) {
			// A thread becoming unread takes its place in file order when the
			// list is next asked for.
			UnreadThreads.reset();
			UnreadConversations.reset();
			PullRequestCounts.reset();
		}
		if (std::ranges::any_of(Cards, [&aThread](const thread_card& aCard) { return aCard.Id == aThread; })) {
			relayout_read_state();
		}
		return previous;
	}

	void conversation_cards_view::relayout_read_state()
	{
		auto anchor = QPointer<QWidget>{ };
		auto before = 0;
		if (OpenCard != nullptr) {
			for (auto* const post : OpenCard->posts()) {
				const auto box = QRect{ post->mapTo(viewport(), QPoint{ }), post->size() };
				if (post->isVisible() && box.intersects(viewport()->rect())) {
					anchor = post;
					before = box.top();
					break;
				}
			}
		}
		relayout();
		if (anchor && OpenCard != nullptr && OpenCard->isAncestorOf(anchor) && anchor->isVisible()) {
			OpenCard->layout()->activate();
			const auto shift = anchor->mapTo(viewport(), QPoint{ }).y() - before;
			verticalScrollBar()->setValue(verticalScrollBar()->value() + shift);
		}
	}

	auto conversation_cards_view::unread_threads_in_file_order() const -> const std::vector<std::string>&
	{
		if (UnreadThreads.has_value()) {
			return *UnreadThreads;
		}
		// Every thread of the pull request, whatever the scope shows, so the
		// strip's next new comment walks the whole review in file order.
		auto threads = std::vector<const application::github_conversation_projection*>{ };
		UnreadThreadsByFile.clear();
		for (std::size_t index = 0U; index < Retained.size(); ++index) {
			const auto& node = Retained[index];
			if (node.Kind != application::github_conversation_kind::review_thread
				|| node.ParentId.has_value()) {
				continue;
			}
			const auto state = ReadStateByThread.find(node.Id);
			if (state != ReadStateByThread.end() && !ReadState[state->second].UnreadPosts.empty()) {
				threads.push_back(&node);
				++UnreadThreadsByFile[RetainedFiles[index]];
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

	auto conversation_cards_view::unread_conversations_in_navigation_order() const -> const std::vector<std::string>&
	{
		if (UnreadConversations.has_value()) {
			return *UnreadConversations;
		}
		auto ids = unread_threads_in_file_order();
		// Pathless roots retain projection order in Discuss. Reading this
		// retained forest also works while the cards are scoped to one file.
		for (const auto& node : Retained) {
			if (node.ParentId.has_value() || node.Deleted
				|| (node.Kind != application::github_conversation_kind::review
					&& node.Kind != application::github_conversation_kind::issue_comment)) {
				continue;
			}
			// [[BPR-1779]]. A review with no body is no comment: its comments
			// are the threads, walked above.
			if (node.Kind == application::github_conversation_kind::review && !core::review_body_is_comment(node.ReviewerProse.value_or(node.Body))) {
				continue;
			}
			const auto state = ReadStateByThread.find(node.Id);
			if (state != ReadStateByThread.end() && !ReadState[state->second].UnreadPosts.empty()) {
				ids.push_back(node.Id);
			}
		}
		UnreadConversations = std::move(ids);
		return *UnreadConversations;
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

	auto conversation_cards_view::unread_post_count_from(const QString& aLogin) const -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (const auto& card : Cards) {
			if (card.Draft) {
				continue;
			}
			count += static_cast<std::size_t>(std::ranges::count_if(card.Posts, [&aLogin](const card_post& aPost) {
				return aPost.Unread && !aPost.Mine && aPost.Author.compare(aLogin, Qt::CaseInsensitive) == 0;
			}));
		}
		return count;
	}

	void conversation_cards_view::set_author_read_dispatch(std::function<void(const std::string&)> aDispatch)
	{
		AuthorReadDispatch = std::move(aDispatch);
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
				// [[BPR-1212]] slice 1. A draft on several lines names them.
				// [[BPR-1721]]. So does a published thread on several,
				// from GitHub's `start_line`: it read as one line before.
				const auto first = thread_first_line(card);
				const auto lines = first.has_value() ? QStringLiteral("Lines %1–%2").arg(*first).arg(*card.Line)
													 : QStringLiteral("Line %1").arg(*card.Line);
				if (first.has_value() && *first > 0U) {
					mark.FirstLine = *first - 1U;
				}
				mark.Description = card.Writing                  ? QStringLiteral("%1: your draft, being written").arg(lines)
					: card.Draft && card.UnsentReply.has_value() ? QStringLiteral("%1: your reply, not sent").arg(lines)
					: card.Draft && card.MovedSuggestion         ? QStringLiteral("%1: your suggestion from an earlier head").arg(lines)
					: card.Draft                                 ? QStringLiteral("%1: your draft").arg(lines)
																 : QStringLiteral("%1: %2 %3").arg(lines).arg(posts).arg(posts == 1U ? QStringLiteral("post") : QStringLiteral("posts"));
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
			mark.Writing = mark.Writing || card.Writing;
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
		Accent = aAccent;
		Marker = aMarker;
		// The open card is rebuilt, and the old one deleted, on the next layout.
		if (OpenCard != nullptr) {
			OpenCard->setProperty("threadKey", QString{ });
		}
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

	auto conversation_cards_view::body_annotations() const noexcept -> std::uint64_t
	{
		return BodyAnnotations;
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

	void conversation_cards_view::show_pull_request_comments(const bool aShown)
	{
		if (Feed || PullRequestComments == aShown) {
			return;
		}
		// [[BPR-1781]]. The pull request's conversation is Overview's and the
		// threads at their lines are Review's; the window moves this one view
		// between them, so each keeps its own place: the current and open
		// card and how far it was scrolled.
		auto& leaving       = PullRequestComments ? ConversationPlace : LinesPlace;
		leaving             = { .Open = Open, .Current = Current, .Scroll = verticalScrollBar()->value() };
		PullRequestComments = aShown;
		setVerticalScrollBarPolicy(aShown ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
		const auto& entering = aShown ? ConversationPlace : LinesPlace;
		Open.clear();
		Current.clear();
		Picked.clear();
		NewConversationBoundary.clear();
		verticalScrollBar()->setValue(0);
		relayout();
		const auto present = [this](const std::string& aId) { return !aId.empty() && std::ranges::any_of(Cards, [&aId](const thread_card& aCard) { return aCard.Id == aId; }); };
		if (present(entering.Current)) {
			Current = entering.Current;
		}
		if (present(entering.Open)) {
			Open = entering.Open;
			relayout();
		}
		// [[BPR-1779]]. The New rule stands over the first comment, in time
		// order, with a post not yet read; reading does not move it.
		if (aShown) {
			if (const auto fresh = std::ranges::find_if(Cards, [](const thread_card& aCard) { return aCard.UnreadCount > 0U; }); fresh != Cards.end()) {
				NewConversationBoundary = fresh->Id;
				place_cards(true);
			}
		}
		verticalScrollBar()->setValue(std::min(entering.Scroll, verticalScrollBar()->maximum()));
		viewport()->update();
	}

	void conversation_cards_view::set_pull_request_link_dispatch(std::function<void(const std::string&)> aDispatch)
	{
		PullRequestLinkDispatch = std::move(aDispatch);
		sync_pull_request_edge();
	}

	auto conversation_cards_view::pull_request_comments_shown() const noexcept -> bool { return PullRequestComments; }
	auto conversation_cards_view::pull_request_comments_chip() const noexcept -> QAbstractButton* { return PullRequestChip; }
	void conversation_cards_view::sync_pull_request_edge()
	{
		// [[BPR-1779]]. The chip counts the conversation's comments, as the
		// Pull request view lists them: every comment, review body and thread
		// a review carried, and no review.
		if (!PullRequestCounts.has_value()) {
			auto items = std::vector<core::timeline_item>{ };
			for (std::size_t index = 0U; index < Retained.size(); ++index) {
				const auto& node = Retained[index];
				if (!node.ParentId && !node.Deleted
					&& (node.Kind == application::github_conversation_kind::review_thread || node.Kind == application::github_conversation_kind::review
						|| node.Kind == application::github_conversation_kind::issue_comment || node.Kind == application::github_conversation_kind::review_comment)) {
					items.push_back(timeline_item_of(index));
				}
			}
			const auto arranged = core::arrange_conversation_timeline(items);
			PullRequestCounts   = std::pair{ arranged.Comments, arranged.Unread };
		}
		const auto [count, unread] = *PullRequestCounts;
		auto* const chip           = static_cast<pull_request_chip*>(PullRequestChip);
		chip->Colours              = note_colours_of(palette(), Accent, Marker);
		chip->New                  = unread;
		// [[BPR-1781]]. One quiet link to Overview's Comments tab, offered
		// whether or not the conversation has comments yet (a new one is
		// written there); its new comments are counted on it.
		chip->setText(tr("Pull request conversation"));
		chip->setAccessibleName(unread > 0U ? tr("Pull request conversation: %1 comments, %2 new").arg(count).arg(unread) : tr("Pull request conversation: %1 comments").arg(count));
		chip->setAccessibleDescription(tr("Opens the pull request's comments and reviews on Overview's Comments tab"));
		chip->setToolTip(tr("The pull request's comments and reviews, on Overview"));
		chip->setGeometry(note_margin, 5, std::min(viewport()->width() - 2 * note_margin, chip->sizeHint().width()), chip_height);
		chip->setVisible(!Feed && !PullRequestComments && PullRequestLinkDispatch);
		chip->raise();
		chip->update();
	}

	auto conversation_cards_view::timeline() const noexcept -> bool
	{
		return PullRequestComments || (Feed && Scope == cards_scope::whole_pull_request);
	}

	auto conversation_cards_view::timeline_lines() const noexcept -> const std::vector<timeline_line>& { return TimelineLines; }

	auto conversation_cards_view::timeline_item_of(const std::size_t aIndex) const -> core::timeline_item
	{
		using kind       = application::github_conversation_kind;
		using state      = application::github_review_state_projection;
		const auto& node = Retained[aIndex];
		auto        item = core::timeline_item{ .Id = node.Id, .Author = node.AuthorLogin, .CreatedAt = node.CreatedAt, .Path = node.Path, .Line = node.Line, .Resolved = node.Resolved };
		if (const auto found = ReadStateByThread.find(node.Id); found != ReadStateByThread.end()) {
			item.Unread = !ReadState[found->second].UnreadPosts.empty();
		}
		if (const auto presented = ResolutionsByThread.find(node.Id); presented != ResolutionsByThread.end()) {
			item.Resolved = presented->second.Resolved;
		}
		switch (node.Kind) {
		case kind::review: {
			item.Kind        = core::timeline_item_kind::review;
			item.Review      = node.Id;
			item.Verdict     = node.ReviewState == state::approved ? core::timeline_verdict::approved
				: node.ReviewState == state::changes_requested     ? core::timeline_verdict::changes_requested
				: node.ReviewState == state::dismissed             ? core::timeline_verdict::dismissed
																   : core::timeline_verdict::commented;
			const auto& body = node.ReviewerProse.value_or(node.Body);
			item.HasBody     = body.find_first_not_of(" \t\r\n") != std::string::npos;
			break;
		}
		case kind::review_thread:
			item.Kind = core::timeline_item_kind::thread;
			if (const auto origin = ThreadOrigins.find(aIndex); origin != ThreadOrigins.end()) {
				item.Review    = origin->second.first;
				item.CreatedAt = origin->second.second;
			}
			break;
		case kind::review_comment:
			item.Kind   = core::timeline_item_kind::thread;
			item.Review = node.ReviewId;
			break;
		default: item.Kind = core::timeline_item_kind::pull_request_comment; break;
		}
		return item;
	}

	void conversation_cards_view::arrange_timeline()
	{
		auto items = std::vector<core::timeline_item>{ };
		items.reserve(Cards.size());
		for (const auto& card : Cards) {
			const auto found = RetainedById.find(card.Id);
			if (found == RetainedById.end()) {
				continue;
			}
			auto item     = timeline_item_of(found->second);
			item.Unread   = card.UnreadCount > 0U;
			item.Resolved = card.Resolved;
			item.Kept     = card.Id == Open;
			items.push_back(std::move(item));
		}
		// The Pull request view lists everything; Discuss's filters act on
		// the comments.
		const auto filters  = PullRequestComments ? core::timeline_filters{ } : core::timeline_filters{ .UnreadOnly = Filters.UnreadOnly, .UnresolvedOnly = Filters.UnresolvedOnly, .Listed = Filters.Listed };
		auto       arranged = core::arrange_conversation_timeline(items, filters);
		auto       byId     = std::unordered_map<std::string, std::size_t>{ };
		for (std::size_t index = 0U; index < Cards.size(); ++index) {
			byId.emplace(Cards[index].Id, index);
		}
		auto order = std::vector<std::pair<std::size_t, const core::timeline_row*>>{ };
		order.reserve(arranged.Comments);
		for (const auto& row : arranged.Rows) {
			if (const auto found = byId.find(row.Id); row.Kind == core::timeline_row_kind::comment && found != byId.end()) {
				order.emplace_back(found->second, &row);
			}
		}
		auto ordered = std::vector<thread_card>{ };
		ordered.reserve(order.size());
		for (const auto& [index, row] : order) {
			auto& card         = Cards[index];
			card.Bundle        = row->Review;
			card.TimelineOrder = ordered.size();
			ordered.push_back(std::move(card));
		}
		Cards        = std::move(ordered);
		TimelineRows = std::move(arranged.Rows);
	}

	void conversation_cards_view::place_timeline()
	{
		// [[BPR-1779]]. A review's comments hang under its quiet line, each
		// thread under its path and line; units of the conversation stand a
		// little apart.
		constexpr auto unitGap = note_gap + 8;
		TimelineLines.clear();
		const auto width   = std::max(120, viewport()->width()) - 2 * note_margin;
		const auto verdict = [](const core::timeline_verdict aVerdict) {
			return aVerdict == core::timeline_verdict::approved ? tr("Approved") : aVerdict == core::timeline_verdict::changes_requested ? tr("Changes requested")
																																		 : QString{ };
		};
		const auto when = [](const std::string& aAt) {
			const auto time = QDateTime::fromString(QString::fromStdString(aAt), Qt::ISODate);
			return time.isValid() ? QLocale{ }.toString(time.toLocalTime(), QStringLiteral("d MMM yyyy")) : QString{ };
		};
		auto bottom    = PullRequestComments ? edge_band : 4;
		auto first     = true;
		auto afterHead = false;
		auto bundle    = std::optional<std::size_t>{ };
		auto previous  = std::string{ };
		auto cursor    = std::size_t{ 0U };
		for (std::size_t at = 0U; at < TimelineRows.size(); ++at) {
			const auto& row = TimelineRows[at];
			if (row.Kind != core::timeline_row_kind::comment) {
				auto top = bottom + (first ? 0 : unitGap);
				if (PullRequestComments && row.Kind == core::timeline_row_kind::bundle && at + 1U < TimelineRows.size() && TimelineRows[at + 1U].Id == NewConversationBoundary) {
					TimelineLines.push_back(timeline_line{ .Kind = timeline_line::kind::fresh, .Rect = QRect{ note_margin, top, width, path_row_height } });
					top += path_row_height;
				}
				TimelineLines.push_back(timeline_line{
					.Kind    = row.Kind == core::timeline_row_kind::bundle ? timeline_line::kind::bundle : timeline_line::kind::verdict,
					.Id      = row.Id,
					.Author  = text(row.Author),
					.Verdict = verdict(row.Verdict),
					.When    = when(row.CreatedAt),
					.Rect    = QRect{ note_margin, top, width, path_row_height },
				});
				bottom    = top + path_row_height;
				bundle    = row.Kind == core::timeline_row_kind::bundle ? std::optional{ TimelineLines.size() - 1U } : std::nullopt;
				previous  = row.Review;
				afterHead = row.Kind == core::timeline_row_kind::bundle;
				first     = false;
				continue;
			}
			while (cursor < Cards.size() && Cards[cursor].Id != row.Id) {
				++cursor;
			}
			if (cursor == Cards.size()) {
				break;
			}
			auto&      card     = Cards[cursor++];
			const auto sameUnit = !row.Review.empty() && row.Review == previous;
			if (!sameUnit) {
				bundle.reset();
			}
			auto top = bottom + (first ? 0 : afterHead ? 2
									 : sameUnit        ? note_gap
													   : unitGap);
			if (PullRequestComments && card.Id == NewConversationBoundary && !afterHead) {
				TimelineLines.push_back(timeline_line{ .Kind = timeline_line::kind::fresh, .Rect = QRect{ note_margin, top, width, path_row_height } });
				top += path_row_height;
			}
			if (!card.Path.isEmpty()) {
				TimelineLines.push_back(timeline_line{
					.Kind = timeline_line::kind::path,
					.Id   = card.Id,
					.Path = card.Line.has_value() ? QStringLiteral("%1:%2").arg(card.Path).arg(*card.Line) : card.Path,
					.Rect = QRect{ note_margin, top, width, path_row_height },
				});
				top += path_row_height;
			}
			card.Rect.moveTop(top);
			bottom = card.Rect.bottom();
			if (bundle.has_value()) {
				TimelineLines[*bundle].Reach = bottom;
			}
			previous  = row.Review;
			afterHead = false;
			first     = false;
		}
		ContentHeight = bottom + note_margin;
		set_scroll_range();
	}

	auto conversation_cards_view::timeline_path_at(const QPoint aViewportPoint) const -> const timeline_line*
	{
		if (!timeline()) {
			return nullptr;
		}
		const auto point = to_content(aViewportPoint);
		const auto found = std::ranges::find_if(TimelineLines, [&point](const timeline_line& aLine) { return aLine.Kind == timeline_line::kind::path && aLine.Rect.contains(point); });
		return found == TimelineLines.end() ? nullptr : &*found;
	}

	void conversation_cards_view::paint_timeline(QPainter& aPainter, const QRect& aClip)
	{
		const auto colours = note_colours_of(palette(), Accent, Marker);
		const auto small   = scaled(font(), 0.92);
		aPainter.save();
		for (const auto& line : TimelineLines) {
			const auto rect = to_viewport(line.Rect);
			// A bundle's rail runs down beside the comments it carried.
			if (line.Kind == timeline_line::kind::bundle && line.Reach > line.Rect.bottom()) {
				const auto foot = to_viewport(QRect{ line.Rect.left(), line.Reach, 1, 1 }).top();
				if (foot >= 0 && rect.bottom() <= aClip.bottom()) {
					aPainter.setPen(QPen{ colours.Rule, 2.0, Qt::SolidLine, Qt::RoundCap });
					aPainter.drawLine(QPointF{ note_margin / 2.0, rect.bottom() + 3.0 }, QPointF{ note_margin / 2.0, static_cast<qreal>(foot) });
				}
			}
			if (rect.bottom() < aClip.top() || rect.top() > aClip.bottom()) {
				continue;
			}
			switch (line.Kind) {
			case timeline_line::kind::fresh:
				aPainter.setPen(colours.New);
				aPainter.setFont(semibold(small));
				aPainter.drawText(rect, Qt::AlignRight | Qt::AlignVCenter, tr("New"));
				aPainter.drawLine(rect.left(), rect.center().y(), viewport()->width() - 50, rect.center().y());
				break;
			case timeline_line::kind::path: {
				aPainter.setFont(semibold(small));
				aPainter.setPen(!HoveredPath.empty() && line.Id == HoveredPath ? colours.Ink : colours.Faint);
				const auto box = rect.adjusted(4, 0, -4, 0);
				aPainter.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics{ aPainter.font() }.elidedText(line.Path, Qt::ElideMiddle, box.width()));
				break;
			}
			case timeline_line::kind::bundle:
			case timeline_line::kind::verdict: {
				// Who, the verdict when it matters, and when: quiet, so the
				// comments under it are what is read.
				auto       x    = rect.left() + 2;
				const auto face = QRectF{ static_cast<qreal>(x), rect.center().y() - 7.0, 14.0, 14.0 };
				paint_avatar(aPainter, face, line.Author, !Viewer.isEmpty() && line.Author == Viewer, colours, small);
				x += 14 + 6;
				const auto bold = semibold(small);
				aPainter.setFont(bold);
				aPainter.setPen(colours.Ink2);
				aPainter.drawText(QRect{ x, rect.top(), std::max(0, rect.right() - x), rect.height() }, Qt::AlignLeft | Qt::AlignVCenter, line.Author);
				x += QFontMetrics{ bold }.horizontalAdvance(line.Author) + 7;
				if (!line.Verdict.isEmpty()) {
					aPainter.setPen(line.Verdict == tr("Approved") ? colours.Ok : MarkTheme.Tints[static_cast<std::size_t>(core::feedback_label::issue)]);
					aPainter.drawText(QRect{ x, rect.top(), std::max(0, rect.right() - x), rect.height() }, Qt::AlignLeft | Qt::AlignVCenter, line.Verdict);
					x += QFontMetrics{ bold }.horizontalAdvance(line.Verdict) + 7;
				}
				if (!line.When.isEmpty()) {
					aPainter.setFont(small);
					aPainter.setPen(colours.Faint);
					aPainter.drawText(QRect{ x, rect.top(), std::max(0, rect.right() - x), rect.height() }, Qt::AlignLeft | Qt::AlignVCenter, line.When);
				}
				break;
			}
			}
		}
		aPainter.restore();
	}

	auto conversation_cards_view::aligned() const noexcept -> bool
	{
		return !Feed && !PullRequestComments && Scope == cards_scope::this_file;
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
		// [[BPR-1086]]. From its lead: a published tour stop's narrative.
		const auto  body   = aCard.Posts.empty() ? QString{ } : aCard.Posts.front().Body.mid(std::clamp<qsizetype>(aCard.Posts.front().Lead, 0, aCard.Posts.front().Body.size()));
		const auto  marked = !aCard.Posts.empty() && aCard.Posts.front().Label.has_value();
		const auto& cached = Measurements.Notes.find(aCard.Id);
		if (cached != Measurements.Notes.end() && cached->second.Body == body && cached->second.Resolved == aCard.Resolved
			&& cached->second.Draft == aCard.Draft && cached->second.Notice == aCard.Notice) {
			aCard.Rect.setHeight(cached->second.Height);
			return;
		}
		++MeasurementCount;
		auto       measure = closed_measure{ .Body = body, .Resolved = aCard.Resolved, .Draft = aCard.Draft, .Notice = aCard.Notice };
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
		measure.Lines = count;
		// [[BPR-1374]]. What became of a reply's thread, under the name, in
		// full: it is the one thing the note must say.
		if (!aCard.Notice.isEmpty()) {
			measure.NoticeLines = notice_line_count(aCard.Notice, scaled(font(), 0.92), width);
		}
		const auto text = name_height(font()) + 1 + measure.NoticeLines * notice_line_height(font()) + count * body_line_height(font());
		const auto gut  = marked ? avatar_size + mark_gap + mark_size + 1 : avatar_size + 1;
		measure.Height  = note_top + std::max(text, gut) + note_bottom + (aCard.Verdict.isEmpty() ? 0 : name_height(font()));
		aCard.Rect.setHeight(measure.Height);
		Measurements.Notes.insert_or_assign(aCard.Id, std::move(measure));
	}

	// ---------------------------------------------------------------- layout

	void conversation_cards_view::relayout()
	{
		++Layouts;
		Cards.clear();
		TimelineRows.clear();
		TimelineLines.clear();
		// [[BPR-0972]]. Scoped to one file, only that file's conversations are
		// visited: the per-forest index names them, in retained order.
		auto       candidates = std::vector<std::size_t>{ };
		const auto scoped     = !PullRequestComments && Scope == cards_scope::this_file && AnchorFile.has_value();
		// [[BPR-1779]]. The Pull request view lists the whole conversation:
		// its comments, and every thread a review carried.
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
			if (node.ParentId.has_value() || (!isThread && ((!Feed && !PullRequestComments) || node.Deleted))) {
				continue;
			}
			if (scoped && RetainedFiles[index] != *AnchorFile) {
				continue;
			}
			auto card      = thread_card{ };
			card.Id        = node.Id;
			card.CreatedAt = node.CreatedAt;
			if (node.Kind == application::github_conversation_kind::review) {
				using state = application::github_review_state_projection;
				// [[BPR-1779]]. In time order the review's line carries its
				// verdict, and its body is one more comment in the bundle.
				if (!timeline()) {
					card.Verdict = node.ReviewState == state::approved ? tr("Approved") : node.ReviewState == state::changes_requested ? tr("Changes requested")
																																	   : tr("Commented");
				}
			}
			if (const auto badge = DefectBadges.find(card.Id); badge != DefectBadges.end()) {
				card.DefectBadge       = badge->second.first;
				card.DefectDescription = badge->second.second;
			}
			card.Kind      = isThread                                                ? QStringLiteral("Inline thread")
				: node.Kind == application::github_conversation_kind::review         ? QStringLiteral("Review summary")
				: node.Kind == application::github_conversation_kind::review_comment ? QStringLiteral("Inline comment")
																					 : QStringLiteral("PR comment");
			card.Path      = text(node.Path);
			card.Side      = side_of(node);
			card.Line      = node.Line;
			card.StartLine = node.StartLine;
			card.Resolved  = node.Resolved;
			// [[BPR-1741]]. My resolve or reopen shows at once, and
			// GitHub's permission decides whether it is offered.
			if (const auto presented = ResolutionsByThread.find(node.Id); isThread && presented != ResolutionsByThread.end()) {
				card.Resolved            = presented->second.Resolved;
				card.ResolutionPending   = presented->second.Pending;
				card.ResolutionPermitted = presented->second.Resolved ? presented->second.CanUnresolve : presented->second.CanResolve;
			}

			card.AnchorUnavailable = !node.Path.empty() && node.AnchorAvailability != application::github_conversation_anchor_availability::current;
			card.Outdated          = node.Outdated || card.AnchorUnavailable;
			// [[BPR-1745]]. The core decides where a reply to the thread
			// goes; an outdated one takes it without a line, as GitHub does.
			// [[BPR-1746]]. GitHub's `viewerCanReply` and the lock,
			// when read, refuse a reply that could not succeed before it is
			// written.
			const auto replyDecision = core::decide_thread_reply({ .ReviewThread = isThread && !node.Path.empty(), .Outdated = node.Outdated, .ViewerCanReply = node.ViewerCanReply, .Locked = node.ConversationLocked });
			card.ReplyOnThread       = replyDecision.Route == core::thread_reply_route::on_thread;
			card.ReplyRefusal        = isThread && !thread_reply_refusal_text(replyDecision.Refusal).isEmpty() ? replyDecision.Refusal : core::thread_reply_refusal::none;
			card.FileLevel           = node.SemanticAnchor.has_value() && node.RemoteSubject == application::github_conversation_remote_subject::file;
			card.Suggestion          = suggestion_placement_of(node);
			if (!isThread) {
				const auto state = ReadStateByThread.find(node.Id);
				card.RootUnread  = state != ReadStateByThread.end()
					&& std::ranges::find(ReadState[state->second].UnreadPosts, node.Id) != ReadState[state->second].UnreadPosts.end();
				card.UnreadCount   = card.RootUnread ? 1U : 0U;
				const auto& mark   = RetainedMarks[index];
				const auto  author = text(node.AuthorLogin);
				card.Posts.push_back(card_post{
					.Id           = node.Id,
					.Author       = author,
					.Body         = text(node.ReviewerProse.value_or(node.Body)),
					.Mine         = !Viewer.isEmpty() && author == Viewer,
					.Unread       = card.RootUnread,
					.Role         = !PullRequestAuthor.isEmpty() && author == PullRequestAuthor ? QStringLiteral("author") : QStringLiteral("reviewer"),
					.Label        = mark.Label,
					.Emphasis     = mark.Emphasis,
					.Name         = feedback_post_name(author, mark),
					.HeaderLength = RetainedHeaders[index],
					.Lead         = RetainedLeads[index],
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
				.Lead         = RetainedLeads[index],
			});
		}
		// [[BPR-1779]]. In time order the core keeps the comments it lists, in
		// its order, its filters acting on comments.
		if (timeline()) {
			arrange_timeline();
		}
		// [[BPR-1290]]. Only new, Hide resolved. The open thread and my draft
		// are never filtered out.
		if (!timeline() && !PullRequestComments && (Filters.UnreadOnly || Filters.UnresolvedOnly)) {
			std::erase_if(Cards, [this](const thread_card& aCard) {
				return !aCard.Draft && aCard.Id != Open && !aCard.UnsentReply.has_value()
					&& ((Filters.UnreadOnly && aCard.UnreadCount == 0U)
						|| (Filters.UnresolvedOnly && aCard.Resolved));
			});
		}
		// [[BPR-1089]]. A drill-down lists exactly the counted conversations:
		// no open thread or draft is kept beside them.
		if (!timeline() && !PullRequestComments && Filters.Listed) {
			std::erase_if(Cards, [this](const thread_card& aCard) { return !Filters.Listed->contains(aCard.Id); });
		}
		if ((Feed || PullRequestComments) && ExcerptLocator) {
			for (auto& card : Cards) {
				if (!card.Draft && card.Side.has_value() && card.Line.has_value() && *card.Line > 0U) {
					card.Excerpt = ExcerptLocator(card.Path, *card.Side, *card.Line).value_or(QString{ });
				}
			}
		}
		// [[BPR-1374]]. My unsent replies, as the core decided where each
		// stands: one whose thread is shown marks that thread's note; one
		// whose thread is gone or not known is kept as a note of its own at
		// its line, with what became of the thread.
		for (const auto& reply : PullRequestComments ? std::vector<core::reply_draft_decision>{ } : UnsentReplies) {
			const auto kept = reply.Standing == core::reply_draft_standing::thread_deleted || reply.Standing == core::reply_draft_standing::thread_unknown;
			if (!kept) {
				const auto thread = std::ranges::find(Cards, "review-thread:" + reply.Thread, &thread_card::Id);
				if (thread != Cards.end()) {
					thread->UnsentReply = reply.Standing;
					thread->Notice      = unsent_reply_notice(reply.Standing);
				}
				continue;
			}
			const auto saved = std::ranges::find(SavedDrafts, reply.Draft, &core::review_comment_draft::Id);
			if (saved == SavedDrafts.end() || Scope != cards_scope::this_file || !CurrentComparison.has_value() || saved->Anchor.Comparison != *CurrentComparison) {
				continue;
			}
			auto card              = thread_card{ };
			card.Id                = kept_reply_identity(saved->Id);
			card.Kind              = QStringLiteral("Draft");
			card.Draft             = true;
			card.UnsentReply       = reply.Standing;
			card.Notice            = unsent_reply_notice(reply.Standing);
			card.KeepableAsComment = reply.KeepableAsComment;
			card.Side              = saved->Anchor.Side;
			// [[BPR-1745]]. A reply bound to its thread has no line.
			card.Line       = core::thread_bound_anchor(saved->Anchor) ? std::nullopt : std::optional{ saved->Anchor.Line + 1U };
			const auto mark = feedback_body_mark_of(saved->Body);
			card.Posts.push_back(card_post{
				.Author       = Viewer.isEmpty() ? QStringLiteral("you") : Viewer,
				.Body         = text(saved->Body),
				.Mine         = true,
				.Label        = mark.Label,
				.Emphasis     = mark.Emphasis,
				.Name         = feedback_post_name(QStringLiteral("Your reply, not sent"), mark),
				.HeaderLength = mark.Label.has_value() ? header_length_of(saved->Body) : 0,
			});
			Cards.push_back(std::move(card));
		}
		// My unpublished notes, each at its line while it is on this
		// comparison: the editor's draft as the editor holds it, and every
		// other draft as it was saved ([[BPR-1326]]). A reply is written
		// inside its thread instead. [[BPR-1212]]: the editor's draft shows
		// while it is written in the drawer, empty or not.
		const auto noted = [this](const core::review_comment_draft& aDraft, const bool aWriting) {
			return !PullRequestComments && Scope == cards_scope::this_file && CurrentComparison.has_value() && aDraft.Anchor.Comparison == *CurrentComparison
				&& !aDraft.shared() && !aDraft.ReplyToThread.has_value() && (!aDraft.Body.empty() || aWriting);
		};
		auto drafts = std::vector<std::pair<std::string, const core::review_comment_draft*>>{ };
		if (Draft.has_value() && noted(*Draft, Writing)) {
			drafts.emplace_back(std::string{ draft_card_id }, &*Draft);
		}
		for (const auto& saved : SavedDrafts) {
			if (noted(saved, false) && (!Draft.has_value() || (saved.Id != Draft->Id && saved.Anchor != Draft->Anchor))) {
				drafts.emplace_back(std::string{ draft_card_id } + ":" + std::to_string(saved.Id.value()), &saved);
			}
		}
		for (const auto& [id, draft] : drafts) {
			auto card  = thread_card{ };
			card.Id    = id;
			card.Kind  = QStringLiteral("Draft");
			card.Draft = true;
			// [[BPR-1212]]. Written in the drawer, the draft is its composer
			// and carries no post.
			const auto writing = Writing && id == draft_card_id;
			card.Writing       = writing;
			// [[BPR-1345]]. Pending in my review, or held by its submission.
			card.Pending = core::pending_in_review(*draft) || core::held_by_review(*draft);
			card.Side    = draft->Anchor.Side;
			card.Line    = draft->Anchor.Line + 1U;
			if (draft->StartLine.has_value()) {
				card.FirstLine = *draft->StartLine + 1U;
			}
			const auto mark = feedback_body_mark_of(draft->Body);
			if (!writing) {
				card.Posts.push_back(card_post{
					.Author       = Viewer.isEmpty() ? QStringLiteral("you") : Viewer,
					.Body         = text(draft->Body),
					.Mine         = true,
					.Label        = mark.Label,
					.Emphasis     = mark.Emphasis,
					.Name         = feedback_post_name(card.Pending ? QStringLiteral("Pending in your review") : QStringLiteral("Your note, not published"), mark),
					.HeaderLength = mark.Label.has_value() ? header_length_of(draft->Body) : 0,
				});
			}
			Cards.push_back(std::move(card));
		}
		// [[BPR-1733]]. My kept suggestions from an earlier head of this file,
		// each at the lines it is carried to, or at its own.
		for (const auto& moved : MovedSuggestions) {
			if (PullRequestComments || Scope != cards_scope::this_file || !CurrentComparison.has_value() || moved.Comparison != *CurrentComparison) {
				continue;
			}
			const auto& draft    = moved.Draft;
			auto        card     = thread_card{ };
			card.Id              = moved_suggestion_identity(draft.Id);
			card.Kind            = QStringLiteral("Draft");
			card.Draft           = true;
			card.MovedSuggestion = true;
			card.Carryable       = moved.Move.Carry.has_value();
			card.Change          = QString::fromStdString(moved.Move.Change);
			card.Notice          = moved_suggestion_notice(moved);
			card.Side            = core::diff_side::right;
			const auto first     = moved.Move.Carry ? moved.Move.Carry->first : draft.StartLine.value_or(draft.Anchor.Line);
			const auto last      = moved.Move.Carry ? moved.Move.Carry->second : draft.Anchor.Line;
			card.Line            = last + 1U;
			if (first < last) {
				card.FirstLine = first + 1U;
			}
			const auto mark = feedback_body_mark_of(draft.Body);
			card.Posts.push_back(card_post{
				.Author       = Viewer.isEmpty() ? QStringLiteral("you") : Viewer,
				.Body         = text(draft.Body),
				.Mine         = true,
				.Label        = mark.Label,
				.Emphasis     = mark.Emphasis,
				.Name         = feedback_post_name(tr("Your suggestion from %1").arg(moved.EarlierHead), mark),
				.HeaderLength = mark.Label.has_value() ? header_length_of(draft.Body) : 0,
			});
			Cards.push_back(std::move(card));
		}
		auto order = std::size_t{ 0U };
		for (auto& card : Cards) {
			card.ProjectionOrder = order++;
		}
		if (!Open.empty() && !card_index(Open).has_value()) {
			Writing = Writing && Open != draft_card_id;
			Open.clear();
			Picked.clear();
			Replying.clear();
		}
		if (!Hovered.empty() && !card_index(Hovered).has_value()) {
			Hovered.clear();
		}
		// [[BPR-1305]]. Keep cards and their counts current while hidden,
		// but measure their text only when the drawer can present them.
		// Overview's initial snapshot otherwise measures every PR thread.
		GeometryPending = true;
		if (isVisible() || !window()->isVisible()) {
			measure_cards();
		}
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

	void conversation_cards_view::measure_cards()
	{
		GeometryPending  = false;
		const auto width = std::max(120, viewport()->width()) - 2 * note_margin;
		for (auto& card : Cards) {
			card.Rect = QRect{ note_margin, 0, width, 0 };
			measure_closed(card, width);
		}
		rebuild_open_card();
		place_cards(true);
	}

	void conversation_cards_view::showEvent(QShowEvent* const aEvent)
	{
		QAbstractScrollArea::showEvent(aEvent);
		if (GeometryPending) {
			measure_cards();
			notify_marks();
		}
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
			+ QString::number(static_cast<int>(card.Resolved)) + QString::number(static_cast<int>(card.Outdated)) + QString::number(static_cast<int>(card.ReplyOnThread)) + QString::number(static_cast<int>(card.ReplyRefusal))
			+ QString::number(static_cast<int>(card.ResolutionPending)) + QString::number(static_cast<int>(card.ResolutionPermitted))

			+ QString::number(static_cast<int>(ResolutionOffered)) + QString::number(static_cast<int>(Feed))
			+ card.Excerpt + card.Verdict + Viewer + QChar{ 0x1f } + QString::fromStdString(Picked) + QString::number(static_cast<int>(card.Writing))
			+ QChar{ 0x1f } + card.Notice + QString::number(static_cast<int>(card.KeepableAsComment)) + QString::number(static_cast<int>(CommentEditOffered));
		for (const auto& post : card.Posts) {
			// [[BPR-1775]]. Who wrote it decides Edit, so it is drawn too.
			key += QChar{ 0x1e } + QString::fromStdString(post.Id) + QChar{ 0x1f } + post.Body + QChar{ 0x1f } + post.Author + QString::number(static_cast<int>(post.Mine));
		}
		if (OpenCard != nullptr && OpenCard->property("threadKey").toString() == key) {
			OpenCard->present_read_state(card);
			OpenCard->present_reactions([this](const std::string& aPost) { return reactions_of(aPost); });
			present_comment_edit();
			return;
		}

		auto* const previous    = OpenCard;
		const auto  hadFocus    = previous != nullptr && previous->isAncestorOf(QApplication::focusWidget());
		const auto  focusedName = hadFocus ? QApplication::focusWidget()->objectName() : QString{ };
		// Every post's text has the same name, so the post is kept too.
		auto focusedPost = QVariant{ };
		for (auto* widget = hadFocus ? QApplication::focusWidget() : nullptr; widget != nullptr && widget != previous; widget = widget->parentWidget()) {
			if (widget->objectName() == QStringLiteral("threadPost")) {
				focusedPost = widget->property("postIdentity");
				break;
			}
		}
		auto style = open_card_style{
			.Marks             = MarkTheme,
			.Colours           = note_colours_of(palette(), Accent, Marker),
			.CodeFont          = CodeFont,
			.Viewer            = Viewer,
			.ResolutionOffered = ResolutionOffered,
			.Feed              = Feed || PullRequestComments,
			.HasCodeAnchor     = has_code_anchor(card),
			.OnFile            = !card.Path.isEmpty(),
			.Picked            = Picked,
		};
		style.CommentEditOffered     = CommentEditOffered;
		style.Colours.CodeBackground = palette().color(QPalette::Active, QPalette::Base);
		style.Colours.CodeInk        = CodeInk;
		style.CodeTheme              = CodeTheme;
		// [[BPR-1564]]. A published comment on lines shows its suggestions as
		// GitHub does: against the commented lines while the thread is current
		// on the compared file, otherwise the suggested lines alone.
		if (!card.Draft && !card.Path.isEmpty() && card.Line.has_value() && !card.FileLevel) {
			auto shown = markdown_suggestion_presentation{ .Path = card.Path.toStdString() };
			if (!card.Outdated && card.Side.has_value() && SuggestionLocator) {
				const auto first = card.StartLine.value_or(*card.Line);
				if (auto lines = SuggestionLocator(card.Path, *card.Side, first, *card.Line); lines.has_value() && !lines->empty()) {
					shown.Lines = core::markdown_suggestion_context{ .FirstLine = first, .Lines = std::move(*lines) };
				}
			}
			style.Suggestions = std::move(shown);
		}
		OpenCard = new open_thread_card{ card, style, viewport() };
		OpenCard->setProperty("threadKey", key);
		if (OpenCard->label_controls() != nullptr) {
			bind_reference(*OpenCard->label_controls());
		}
		const auto id     = card.Id;
		const auto words  = card.UnsentReply.has_value() && card.Draft && !card.Posts.empty() ? card.Posts.front().Body : QString{ };
		const auto change = card.MovedSuggestion ? card.Change : QString{ };
		OpenCard->Action  = [this, id, words, change](const card_action aAction) {
			switch (aAction) {
			case card_action::copy_change:
				// [[BPR-1733]]. The suggestion's lines, as its block holds them.
				if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr && !change.isEmpty()) {
					clipboard->setText(change);
				}
				if (ActionDispatch) {
					ActionDispatch(card_action::copy_change, id);
				}
				return;
			case card_action::copy_reply:
				// [[BPR-1374]]. The kept reply's words, as written; nothing
				// else changes.
				if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr && !words.isEmpty()) {
					clipboard->setText(words);
				}
				if (ActionDispatch) {
					ActionDispatch(card_action::copy_reply, id);
				}
				return;
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
					ActionDispatch(Feed || PullRequestComments ? card_action::open_in_code : card_action::show_line, id);
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
		OpenCard->CodeRequest = [this](std::vector<application::markdown_code_syntax_request> aRequests) {
			if (CodeDispatch) {
				CodeDispatch(std::move(aRequests));
			}
		};
		OpenCard->ImageRequest = [this](const std::string& aPost, std::vector<core::markdown_image_reference> aReferences) {
			if (ImageDispatch) {
				ImageDispatch(aPost, std::move(aReferences));
			}
		};
		OpenCard->Reaction = [this](const std::string& aPost, const core::reaction_content aContent) {
			const auto dispatch = ReactionDispatch;
			if (dispatch) {
				dispatch(aPost, aContent);
			}
		};
		OpenCard->present_reactions([this](const std::string& aPost) { return reactions_of(aPost); });
		// [[BPR-1775]]. Edit on my own comment, and its editor's gestures.
		OpenCard->EditRequested = [this](const std::string& aPost) { static_cast<void>(begin_comment_edit(aPost)); };
		OpenCard->EditText      = [this](const std::string& aText) {
			if (CommentEdit.has_value() && !CommentEdit->Saving) {
				CommentEdit->Text = aText;
			}
		};
		OpenCard->EditSave   = [this] { save_comment_edit(); };
		OpenCard->EditCancel = [this] {
			if (CommentEdit.has_value() && !CommentEdit->Saving) {
				close_comment_edit();
			}
		};
		OpenCard->EditNewer = [this] {
			if (!CommentEdit.has_value() || !CommentEdit->Newer.has_value()) {
				return;
			}
			CommentEdit->Original = std::move(*CommentEdit->Newer);
			CommentEdit->Newer.reset();
			CommentEdit->Text   = CommentEdit->Original;
			CommentEdit->Status = QStringLiteral("Editing GitHub's newer text.");
			present_comment_edit();
		};
		OpenCard->set_images_shown(ImagesShown);
		OpenCard->request_images();

		OpenCard->request_code_syntax();
		OpenCard->ReplyText = [this, id](const QString& aText) {
			if (ReplyTextDispatch) {
				ReplyTextDispatch(id, aText);
			}
		};
		OpenCard->DraftText = [this](const QString& aText) {
			if (DraftTextDispatch) {
				DraftTextDispatch(aText);
			}
		};
		OpenCard->HeightChanged  = [this] { place_cards(true); };
		OpenCard->CloseRequested = [this, writing = card.Writing] {
			// [[BPR-1212]]. Closing the draft being written keeps it unless it
			// is empty, which the window decides.
			if (writing) {
				if (ActionDispatch) {
					ActionDispatch(card_action::close_draft, std::string{ draft_card_id });
				}
				return;
			}
			close_thread();
			setFocus(Qt::OtherFocusReason);
		};
		OpenCard->MoreRequested         = [this, id](const QPoint aGlobal) { show_thread_menu(id, aGlobal); };
		OpenCard->OpenInReviewRequested = [this](const std::string& aPost) {
			const auto dispatch = ActionDispatch;
			if (dispatch) {
				dispatch(card_action::open_in_code, aPost);
			}
		};
		OpenCard->ShowInDiscussRequested = [this](const std::string& aPost) {
			const auto dispatch = ActionDispatch;
			if (dispatch) {
				dispatch(card_action::show_in_discuss, aPost);
			}
		};
		OpenCard->ShowInThreadsRequested = [this](const std::string& aPost) {
			const auto dispatch = ActionDispatch;
			if (dispatch) {
				dispatch(card_action::show_in_threads, aPost);
			}
		};
		// [[BPR-1699]]. A post's Mark all from its author as read.
		OpenCard->AuthorUnread = [this](const QString& aLogin) -> std::optional<std::size_t> {
			return ReadStateKnown ? std::optional{ unread_post_count_from(aLogin) } : std::nullopt;
		};
		OpenCard->AuthorReadRequested = [this](const QString& aLogin) {
			const auto dispatch = AuthorReadDispatch;
			if (dispatch) {
				dispatch(aLogin.toStdString());
			}
		};
		// [[BPR-1581]]. A card rebuilt while an arrival is marked carries it.
		if (ArrivalAnimation != nullptr && !ArrivalPost.empty()) {
			for (auto* const post : OpenCard->posts()) {
				if (post->post().Id == ArrivalPost) {
					post->set_arrival(ArrivalAnimation->currentValue().toReal());
				}
			}
		}
		OpenCard->present_reply(Reply);
		if (card.Writing) {
			OpenCard->present_draft(Composer);
		}
		if (Replying == id || (Reply.has_value() && Reply->Thread == id && !Reply->Body.trimmed().isEmpty())) {
			Replying = id;
			OpenCard->set_replying(true);
		}
		OpenCard->installEventFilter(this);
		for (auto* const child : OpenCard->findChildren<QWidget*>()) {
			child->installEventFilter(this);
		}
		// RFC-BPR-0036. A caret moved by the keyboard in a post stays in view.
		for (auto* const post : OpenCard->posts()) {
			auto* const reader = post->reader();
			connect(reader->rendered_view(), &QTextEdit::cursorPositionChanged, OpenCard, [this, view = reader->rendered_view()] {
				if (view->hasFocus()) {
					reveal_reading(QRect{ view->viewport()->mapTo(viewport(), view->cursorRect().topLeft()), view->cursorRect().size() });
				}
			});
			connect(reader->source_view(), &QPlainTextEdit::cursorPositionChanged, OpenCard, [this, view = reader->source_view()] {
				if (view->hasFocus()) {
					reveal_reading(QRect{ view->viewport()->mapTo(viewport(), view->cursorRect().topLeft()), view->cursorRect().size() });
				}
			});
		}
		OpenCard->show();
		if (previous != nullptr) {
			previous->hide();
			previous->deleteLater();
		}
		present_comment_edit();
		if (hadFocus) {
			auto* scope = static_cast<QWidget*>(OpenCard);
			if (focusedPost.isValid()) {
				for (auto* const post : OpenCard->posts()) {
					if (post->property("postIdentity") == focusedPost) {
						scope = post;
					}
				}
			}
			auto* target = focusedName.isEmpty() ? nullptr : scope->findChild<QWidget*>(focusedName);
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
		if ((Feed || PullRequestComments) && ExcerptLocator) {
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
			// [[BPR-1779]]. In time order, as the core arranged them.
			if (timeline()) {
				return aLeft.TimelineOrder < aRight.TimelineOrder;
			}
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
			set_scroll_range();
		} else if (timeline()) {
			place_timeline();
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
			set_scroll_range();
		}
		place_open_card();
		sync_edges();
		viewport()->update();
	}

	void conversation_cards_view::set_scroll_range()
	{
		// [[BPR-1740]]. The owner, 2026-10-22: "Much like the code
		// window I want to be able to scroll the comments drawer up until only
		// the last line is visible." The furthest position leaves the last
		// note's last line, and the inset under it, at the top.
		const auto line    = fontMetrics().lineSpacing();
		const auto overrun = std::max(0, ContentHeight - viewport()->height());
		const auto pastEnd = Feed ? 0 : std::max(0, ContentHeight - note_margin - 2 * line);
		verticalScrollBar()->setRange(0, std::max(overrun, pastEnd));
		verticalScrollBar()->setPageStep(viewport()->height());
		verticalScrollBar()->setSingleStep(line);
	}

	auto conversation_cards_view::open_thread_hangs() const -> bool
	{
		const auto open = card_index(Open);
		return !open.has_value() || Cards[*open].Placed;
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
		// [[BPR-1293]]. A picked post is brought into view once, after the
		// code has settled with the card hanging at its line: every frame of
		// the line's reveal restarts the wait, so the two never pull apart.
		if (RevealPicked && !Picked.empty()) {
			if (PickedReveal == nullptr) {
				PickedReveal = new QTimer{ this };
				PickedReveal->setSingleShot(true);
				PickedReveal->setInterval(120);
				connect(PickedReveal, &QTimer::timeout, this, [this] {
					if (!RevealPicked || OpenCard == nullptr || Picked.empty()) {
						return;
					}
					const auto opened = card_index(Open);
					if (PickedRevealSteps == 0 && opened.has_value() && aligned() && !(Cards[*opened].Placed && Cards[*opened].TargetY >= 0 && Cards[*opened].TargetY < viewport()->height())) {
						return;
					}
					// The code takes a long move in parts: a step that leaves the
					// post outside the view is taken again, while it moves.
					for (auto* const view : OpenCard->posts()) {
						if (view->post().Id != Picked) {
							continue;
						}
						const auto place = QRect{ view->mapTo(viewport(), QPoint{ 0, 0 }), view->size() };
						// Clear of the chips on the top and bottom edges.
						const auto visible = viewport()->rect().adjusted(0, edge_reach + 8, 0, -(edge_reach + 8));
						if (visible.contains(place) || (place.height() > visible.height() && place.top() >= visible.top() && place.top() < visible.bottom())
							|| PickedRevealSteps >= 8 || place.top() == PickedRevealLast) {
							RevealPicked = false;
							return;
						}
						++PickedRevealSteps;
						PickedRevealLast = place.top();
						// The foot of a thread taller than what is left below
						// its line is the notes' own overflow; the code moves
						// only for what the overflow cannot give.
						auto* const bar   = verticalScrollBar();
						const auto  move  = place.bottom() > visible.bottom() ? std::min(place.bottom() - visible.bottom(), place.top() - visible.top())
							: place.top() < visible.top()                     ? place.top() - visible.top()
																			  : 0;
						const auto  taken = std::clamp(bar->value() + move, bar->minimum(), bar->maximum()) - bar->value();
						bar->setValue(bar->value() + taken);
						if (taken != move) {
							reveal_reading(place.translated(0, -taken));
						}
						PickedReveal->start();
						return;
					}
					RevealPicked = false;
				});
			}
			PickedReveal->start();
		}
	}

	void conversation_cards_view::sync_edges()
	{
		sync_pull_request_edge();
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
			auto       x     = note_margin + 20 + (!aDown && PullRequestChip->isVisible() ? PullRequestChip->width() + 6 : 0);
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
			// [[BPR-1345]]. A draft pending in my review says so in the
			// writing blue, as the start-a-review frames tag it.
			// In a narrow drawer, "Pending", so the name keeps its room.
			const auto whole = QStringLiteral("Pending in your review");
			// [[BPR-1374]]. My kept reply was never sent, and says so.
			const auto words = aCard.UnsentReply.has_value()                      ? QStringLiteral("Not sent")
				: aCard.MovedSuggestion                                           ? QStringLiteral("Earlier head")
				: !aCard.Pending                                                  ? QStringLiteral("Draft")
				: right - left - smallMetrics.horizontalAdvance(whole) - 20 >= 90 ? whole
																				  : QStringLiteral("Pending");
			const auto ink   = aCard.Pending ? colours.Signal : colours.Faint;
			const auto w     = smallMetrics.horizontalAdvance(words);
			aPainter.setPen(ink);
			aPainter.drawText(QRect{ right - w, top, w + 1, nameH }, Qt::AlignRight | Qt::AlignVCenter, words);
			paint_conversation_glyph(aPainter, QRectF{ static_cast<qreal>(right - w - 16), top + (nameH - 12) / 2.0, 12.0, 12.0 }, conversation_glyph::lock, ink);
			right -= w + 20;
		} else {
			// [[BPR-1471]], [[BPR-1473]]. The resolved check's slot is kept on
			// every note and the blue count follows it, so the counts line up
			// down the list whatever else a note shows.
			const auto meta = closed_meta(aCard);
			if (!meta.Resolved.isEmpty()) {
				paint_conversation_glyph(aPainter, meta.Resolved, conversation_glyph::resolved, colours.Ok);
			}
			// [[BPR-1374]]. My reply written in this thread and not sent, in
			// the writing blue, at full strength on a resolved note too.
			if (!meta.UnsentReply.isEmpty()) {
				const auto words = QStringLiteral("Your reply");
				aPainter.save();
				aPainter.setOpacity(1.0);
				aPainter.setPen(colours.Signal);
				aPainter.drawText(meta.UnsentReply.adjusted(16.0, 0.0, 1.0, 0.0), Qt::AlignRight | Qt::AlignVCenter, words);
				paint_conversation_glyph(aPainter, QRectF{ meta.UnsentReply.left(), top + (nameH - 12) / 2.0, 12.0, 12.0 }, conversation_glyph::edit, colours.Signal);
				aPainter.restore();
			}
			if (!meta.NewCount.isEmpty()) {
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(colours.New);
				aPainter.drawRoundedRect(meta.NewCount, 8.0, 8.0);
				aPainter.setFont(new_count_font(font()));
				aPainter.setPen(colours.OnNew);
				aPainter.drawText(meta.NewCount, Qt::AlignCenter, QString::number(aCard.UnreadCount));
				aPainter.setFont(small);
			}
			right              = meta.Replies;
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
		const auto who = aCard.Draft ? (aCard.UnsentReply.has_value() ? QStringLiteral("Your reply") : aCard.MovedSuggestion ? QStringLiteral("Your suggestion")
																															 : QStringLiteral("Your note"))
									 : (root.Mine ? QStringLiteral("you") : root.Author);
		aPainter.drawText(QRect{ left, top, std::max(0, right - left), nameH }, Qt::AlignLeft | Qt::AlignVCenter,
						  QFontMetrics{ bold }.elidedText(who, Qt::ElideRight, std::max(0, right - left)));
		// [[BPR-1374]]. What became of the thread a reply answers, whole,
		// under the name and above the words.
		auto bodyTop = top + nameH + 1;
		if (!aCard.Verdict.isEmpty()) {
			aPainter.setFont(small);
			aPainter.setPen(aCard.Verdict == tr("Approved") ? colours.Ok : aCard.Verdict == tr("Changes requested") ? MarkTheme.Tints[static_cast<std::size_t>(core::feedback_label::issue)]
																													: colours.Faint);
			aPainter.drawText(QRect{ left, bodyTop, std::max(0, right - left), nameH }, Qt::AlignLeft | Qt::AlignVCenter, aCard.Verdict);
			bodyTop += nameH;
		}
		if (measure != nullptr && measure->NoticeLines > 0) {
			const auto noticeHeight = measure->NoticeLines * notice_line_height(font());
			aPainter.save();
			aPainter.setOpacity(1.0);
			aPainter.setFont(small);
			aPainter.setPen(colours.Ink2);
			aPainter.drawText(QRect{ left, bodyTop, std::max(40, rect.width() - text_left - note_right), noticeHeight }, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, aCard.Notice);
			aPainter.restore();
			bodyTop += noticeHeight;
		}
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
			aPainter.setClipRect(QRect{ left, bodyTop, rect.width() - text_left - note_right + 2, lines * body_line_height(font()) + 2 }, Qt::IntersectClip);
			layout.draw(&aPainter, QPointF{ static_cast<qreal>(left), static_cast<qreal>(bodyTop) });
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
		if (timeline()) {
			paint_timeline(painter, viewport()->rect());
		}
		for (const auto& card : Cards) {
			const auto rect = to_viewport(card.Rect);
			if (!aligned() && !timeline() && (card.Path != lastPath || first)) {
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
					// [[BPR-1212]]. Amber is where you read, blue where you write.
					painter.setBrush(card.Writing ? colours.Signal : colours.Route);
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
							 Filters.UnreadOnly || Filters.UnresolvedOnly || Filters.Listed
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

	auto conversation_cards_view::closed_meta(const thread_card& aCard) const -> closed_note_meta
	{
		if (aCard.Draft || aCard.Posts.empty() || aCard.Rect.isEmpty()) {
			return { };
		}
		const auto rect  = to_viewport(aCard.Rect);
		const auto top   = static_cast<qreal>(rect.top() + note_top);
		const auto nameH = name_height(font());
		auto       right = rect.right() - note_right;
		auto       meta  = closed_note_meta{ };
		if (aCard.Resolved) {
			meta.Resolved = QRectF{ static_cast<qreal>(right - resolved_size), top + ((nameH - resolved_size) / 2.0), resolved_size, resolved_size };
		}
		right -= resolved_slot;
		// [[BPR-1473]]. The blue count comes straight after the check's slot,
		// so nothing else on the row moves it.
		if (aCard.UnreadCount > 0U) {
			const auto w  = std::max(16, QFontMetrics{ new_count_font(font()) }.horizontalAdvance(QString::number(aCard.UnreadCount)) + 10);
			meta.NewCount = QRectF{ static_cast<qreal>(right - w), top + ((nameH - 16) / 2.0), static_cast<qreal>(w), 16.0 };
			right -= w + 6;
		}
		if (aCard.UnsentReply.has_value()) {
			const auto w     = QFontMetrics{ scaled(font(), 0.92) }.horizontalAdvance(QStringLiteral("Your reply"));
			meta.UnsentReply = QRectF{ static_cast<qreal>(right - w - 16), top, static_cast<qreal>(w + 16), static_cast<qreal>(nameH) };
			right -= w + 16 + 8;
		}
		meta.Replies = right;
		return meta;
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
		// [[BPR-1740]]. A notice under the drawer takes its height from
		// it: the open thread's focused control stays in view rather than
		// going under the notice, as the Reply field went under a refusal.
		if (aEvent->size().height() < aEvent->oldSize().height() && OpenCard != nullptr) {
			if (auto* const focus = QApplication::focusWidget(); focus != nullptr && OpenCard->isAncestorOf(focus)) {
				reveal_focus(*focus);
			}
		}
	}

	void conversation_cards_view::reveal_focus(QWidget& aWidget)
	{
		auto area = QRect{ aWidget.mapTo(viewport(), QPoint{ 0, 0 }), aWidget.size() };
		if (const auto* const text = qobject_cast<const QTextEdit*>(&aWidget); text != nullptr) {
			area = QRect{ text->viewport()->mapTo(viewport(), text->cursorRect().topLeft()), text->cursorRect().size() };
		} else if (const auto* const source = qobject_cast<const QPlainTextEdit*>(&aWidget); source != nullptr) {
			area = QRect{ source->viewport()->mapTo(viewport(), source->cursorRect().topLeft()), source->cursorRect().size() };
		}
		reveal_reading(area);
	}

	// ------------------------------------------------------------- gestures

	void conversation_cards_view::choose(const std::size_t aCard, const bool aOpen)
	{
		if (aCard >= Cards.size()) {
			return;
		}
		// [[BPR-1374]]. My kept reply opens in place, like a thread, where its
		// words and what can be done with them are; it is not the editor's.
		// [[BPR-1733]]. So does my suggestion from an earlier head.
		if (Cards[aCard].Draft && (Cards[aCard].UnsentReply.has_value() || Cards[aCard].MovedSuggestion)) {
			const auto chosen = Cards[aCard].Id;
			Current           = chosen;
			if (aOpen && Open != chosen) {
				Open = chosen;
				Picked.clear();
				Replying.clear();
				relayout();
			}
			viewport()->update();
			return;
		}
		if (Cards[aCard].Draft) {
			// [[BPR-1212]]. Opening my draft makes it editable again, in place.
			// Another line's saved draft ([[BPR-1326]]) is the window's to
			// bring into the editor first, by choosing its line.
			if (aOpen && Cards[aCard].Id == draft_card_id) {
				begin_draft();
			} else if (aOpen && ActionDispatch) {
				const auto id = Cards[aCard].Id;
				ActionDispatch(card_action::edit_draft, id);
			}
			return;
		}
		const auto chosen   = Cards[aCard].Id;
		const auto markRead = aOpen && (PullRequestComments || (Feed && Cards[aCard].UnreadCount > 0U));
		Current             = chosen;
		// [[BPR-1293]]. A pick names its post; any other opening drops it.
		auto picked = std::exchange(PickedNext, std::string{ });
		if (aOpen && (Open != chosen || picked != Picked)) {
			Open   = chosen;
			Picked = std::move(picked);
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
		// [[BPR-1668]]. Opening a new Discuss card uses the same durable read
		// command as its explicit action. Selection and refresh do not open it.
		if (markRead && ActionDispatch) {
			ActionDispatch(card_action::mark_read, chosen);
		}
		if (ThreadDispatch) {
			// [[BPR-1029]]. The dispatch may make another thread current, so it
			// is handed its own copy of the identity rather than a reference
			// into state it can change.
			ThreadDispatch(chosen, aOpen);
		}
	}

	void conversation_cards_view::open_thread(const std::string& aId)
	{
		if (!Feed && !PullRequestComments && std::ranges::any_of(Retained, [&aId](const auto& aNode) { return aNode.Id == aId && aNode.Path.empty() && !aNode.ParentId && !aNode.Deleted && (aNode.Kind == application::github_conversation_kind::issue_comment || aNode.Kind == application::github_conversation_kind::review); })) {
			// [[BPR-1781]]. A comment on the pull request is in its
			// conversation, on Overview; the window takes it there.
			if (PullRequestLinkDispatch) {
				PullRequestLinkDispatch(aId);
			}
			return;
		}
		if (const auto index = card_index(aId); index.has_value()) {
			choose(*index, true);
		}
	}

	void conversation_cards_view::open_thread_at(const std::string& aId, const std::string& aPost)
	{
		const auto index = card_index(aId);
		if (!index.has_value()) {
			return;
		}
		const auto& posts = Cards[*index].Posts;
		PickedNext        = std::ranges::any_of(posts, [&aPost](const card_post& aCardPost) { return aCardPost.Id == aPost; }) ? aPost : std::string{ };
		choose(*index, true);
		if (Picked.empty() || OpenCard == nullptr) {
			return;
		}
		// The post is brought into view once the card hangs at its line on
		// screen: the line's own reveal moves the code first.
		RevealPicked      = true;
		PickedRevealSteps = 0;
		PickedRevealLast  = std::numeric_limits<int>::min();
		place_open_card();
	}

	auto conversation_cards_view::picked_post() const noexcept -> const std::string&
	{
		return Picked;
	}

	void conversation_cards_view::mark_arrival()
	{
		if (Picked.empty()) {
			return;
		}
		if (ArrivalAnimation != nullptr) {
			ArrivalAnimation->stop();
			ArrivalAnimation->deleteLater();
		}
		ArrivalPost = Picked;
		// Held, then dissolving to the resting wash: 0.7 s full, 0.65 s out.
		auto* const animation = new QVariantAnimation{ this };
		animation->setDuration(1'350);
		animation->setStartValue(1.0);
		animation->setKeyValueAt(0.52, 1.0);
		animation->setEndValue(0.0);
		const auto apply = [this](const qreal aStrength) {
			if (OpenCard == nullptr) {
				return;
			}
			for (auto* const post : OpenCard->posts()) {
				post->set_arrival(post->post().Id == ArrivalPost ? aStrength : 0.0);
			}
		};
		connect(animation, &QVariantAnimation::valueChanged, this, [apply](const QVariant& aValue) { apply(aValue.toReal()); });
		connect(animation, &QVariantAnimation::finished, this, [this, apply, animation] {
			apply(0.0);
			if (ArrivalAnimation == animation) {
				ArrivalPost.clear();
			}
			animation->deleteLater();
		});
		ArrivalAnimation = animation;
		apply(1.0);
		animation->start();
	}

	auto conversation_cards_view::arrival_post() const -> std::string
	{
		return ArrivalAnimation != nullptr && ArrivalAnimation->state() == QAbstractAnimation::Running ? ArrivalPost : std::string{ };
	}

	auto conversation_cards_view::arrival_strength() const noexcept -> qreal
	{
		return ArrivalAnimation != nullptr && ArrivalAnimation->state() == QAbstractAnimation::Running ? ArrivalAnimation->currentValue().toReal() : 0.0;
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
		Writing = Writing && Open != draft_card_id;
		Open.clear();
		Picked.clear();
		RevealPicked = false;
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

	void conversation_cards_view::begin_draft()
	{
		if (!Writing || Open != draft_card_id) {
			Writing = true;
			Open    = std::string{ draft_card_id };
			Picked.clear();
			Replying.clear();
			relayout();
		}
		if (OpenCard == nullptr || Open != draft_card_id) {
			return;
		}
		OpenCard->present_draft(Composer);
		if (auto* const editor = OpenCard->reply_editor(); editor != nullptr) {
			editor->setFocus(Qt::OtherFocusReason);
			place_cards(true);
			ensure_visible(QRect{ editor->mapTo(viewport(), QPoint{ 0, 0 }), editor->size() });
		}
	}

	void conversation_cards_view::end_draft()
	{
		if (!Writing) {
			return;
		}
		Writing = false;
		if (Open == draft_card_id) {
			Open.clear();
		}
		forget_measurements_if([](const std::string& aId) { return aId == draft_card_id; });
		relayout();
	}

	auto conversation_cards_view::writing_draft() const noexcept -> bool
	{
		return Writing && Open == draft_card_id;
	}

	void conversation_cards_view::present_draft(std::optional<draft_composer_presentation> aDraft)
	{
		if (Composer == aDraft) {
			return;
		}
		Composer = std::move(aDraft);
		if (OpenCard != nullptr && Open == draft_card_id) {
			OpenCard->present_draft(Composer);
		}
	}

	void conversation_cards_view::set_draft_text_dispatch(std::function<void(const QString&)> aDispatch)
	{
		DraftTextDispatch = std::move(aDispatch);
	}

	auto conversation_cards_view::draft_editor() const -> QPlainTextEdit*
	{
		return OpenCard != nullptr && Open == draft_card_id && Writing ? OpenCard->reply_editor() : nullptr;
	}

	auto conversation_cards_view::draft_identity() -> const std::string&
	{
		static const auto identity = std::string{ draft_card_id };
		return identity;
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

	void conversation_cards_view::reveal_reading(const QRect& aViewportRect)
	{
		const auto room    = fontMetrics().lineSpacing();
		const auto visible = viewport()->rect().adjusted(0, room, 0, -room);
		auto       move    = 0;
		if (aViewportRect.bottom() > visible.bottom()) {
			move = aViewportRect.bottom() - visible.bottom();
		}
		if (aViewportRect.top() - move < visible.top()) {
			move = aViewportRect.top() - visible.top();
		}
		if (move == 0) {
			return;
		}
		if (!aligned()) {
			verticalScrollBar()->setValue(verticalScrollBar()->value() + move);
			return;
		}
		// The drawer follows the code: move it as a wheel over the notes does.
		const auto centre = QPointF{ viewport()->rect().center() };
		const auto before = OpenCard != nullptr ? OpenCard->y() : 0;
		auto       wheel  = QWheelEvent{ centre, viewport()->mapToGlobal(centre), QPoint{ 0, -move }, QPoint{ 0, -move }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
		wheelEvent(&wheel);
		// [[BPR-1740]]. What the code could not take, near the end of a
		// short file, the notes' own overflow does: focus on the Reply field
		// of a thread taller than the drawer left the field out of view.
		const auto moved = OpenCard != nullptr ? before - OpenCard->y() : 0;
		if (const auto rest = move - moved; rest != 0 && (rest > 0) == (move > 0)) {
			verticalScrollBar()->setValue(verticalScrollBar()->value() + rest);
		}
	}

	auto conversation_cards_view::focusNextPrevChild(const bool aNext) -> bool
	{
		if (aNext && hasFocus() && OpenCard != nullptr && OpenCard->isVisible()) {
			if (auto* const entry = OpenCard->reading_entry(); entry != nullptr) {
				entry->setFocus(Qt::TabFocusReason);
				return true;
			}
		}
		return QAbstractScrollArea::focusNextPrevChild(aNext);
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
			show_anchored_tooltip(*viewport(), QRect{ note_margin, 0, viewport()->width() - 2 * note_margin, 4 }, QStringLiteral("Nothing new in this file."), anchored_tooltip_hold::focus);
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
		// [[BPR-1779]]. A thread's path and line, in time order, opens it in
		// Review, as its Open in Review does.
		if (const auto* const path = timeline_path_at(aEvent->position().toPoint()); path != nullptr) {
			if (ActionDispatch) {
				const auto id = path->Id;
				ActionDispatch(card_action::open_in_code, id);
			}
			aEvent->accept();
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
		const auto* const path = timeline_path_at(aEvent->position().toPoint());
		viewport()->setCursor((hit.has_value() && Cards[*hit].Id != Open) || path != nullptr ? Qt::PointingHandCursor : Qt::ArrowCursor);
		if (const auto id = path != nullptr ? path->Id : std::string{ }; id != HoveredPath) {
			HoveredPath = id;
			viewport()->update();
		}
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
		if (!open_thread_hangs()) {
			// [[BPR-1740]]. An open thread with no line to hang from (an
			// outdated one opens among its file's unplaced threads,
			// [[BPR-1581]]) does not move with the code, so the notes take the
			// wheel first and the code only what they cannot. The code took
			// every step of a long file, and the thread never moved.
			const auto before = bar->value();
			bar->setValue(before - delta);
			if (bar->value() == before && ScrollForward) {
				static_cast<void>(ScrollForward(*aEvent));
			}
			aEvent->accept();
			return;
		}
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
			if (writing_draft()) {
				if (ActionDispatch) {
					ActionDispatch(card_action::close_draft, std::string{ draft_card_id });
				}
				return true;
			}
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
			if (plain && !aEvent.modifiers().testFlag(Qt::ShiftModifier) && !Open.empty() && !Open.starts_with(draft_card_id) && ActionDispatch) {
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
		// [[BPR-1740]]. Home and End, and the platform's document start
		// and end, go to the notes' top and their end, as in the code panes.
		const auto edge = document_edge_for(*aEvent);
		const auto home = edge == document_edge::start || (aEvent->key() == Qt::Key_Home && aEvent->modifiers() == Qt::NoModifier);
		const auto end  = edge == document_edge::end || (aEvent->key() == Qt::Key_End && aEvent->modifiers() == Qt::NoModifier);
		if (home || end) {
			verticalScrollBar()->triggerAction(home ? QAbstractSlider::SliderToMinimum : QAbstractSlider::SliderToMaximum);
			aEvent->accept();
			return;
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
				const auto drafting = OpenCard->id() == draft_card_id;
				if ((key.key() == Qt::Key_Return || key.key() == Qt::Key_Enter) && key.modifiers().testFlag(Qt::ControlModifier)) {
					// [[BPR-1345]] v002 (owner, finish-review v001): on a draft
					// ⌘↩ is the review's action (Start a review, Add review
					// comment, or keep a pending one), ⌘⇧↩ is Add single
					// comment and ⌘⌥↩ is Finish review. In a reply ⌘↩
					// publishes it and the others do nothing.
					const auto single = key.modifiers().testFlag(Qt::ShiftModifier);
					const auto finish = key.modifiers().testFlag(Qt::AltModifier);
					if (OpenCard->Action && drafting) {
						OpenCard->Action(finish ? card_action::finish_review : single ? card_action::publish_draft
																					  : OpenCard->primary_action());
					} else if (OpenCard->Action && !single && !finish) {
						OpenCard->Action(card_action::publish_reply);
					}
					return true;
				}
				if (key.key() == Qt::Key_M && key.modifiers().testFlag(Qt::ControlModifier) && key.modifiers().testFlag(Qt::ShiftModifier)
					&& !key.modifiers().testFlag(Qt::AltModifier)) {
					// [[BPR-1212]]. Maximize takes the draft to Discuss; ⌘⇧M
					// since [[BPR-1345]] v002 gave ⌘⇧↩ to Add single comment.
					if (OpenCard->Action && drafting) {
						OpenCard->Action(card_action::maximize_draft);
					}
					return true;
				}
				if (key.key() == Qt::Key_Escape) {
					if (drafting) {
						if (OpenCard->Action) {
							OpenCard->Action(card_action::close_draft);
						}
						return true;
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
			// In a post's text the arrows move the reading caret; elsewhere in
			// the thread they move between threads.
			if ((key.key() == Qt::Key_Up || key.key() == Qt::Key_Down) && !OpenCard->is_reading(aWatched)) {
				setFocus(Qt::OtherFocusReason);
				keyPressEvent(&key);
				return true;
			}
			return handle_thread_key(key, false);
		}
		if (aEvent->type() == QEvent::FocusIn && OpenCard != nullptr && OpenCard->isAncestorOf(qobject_cast<QWidget*>(aWatched))) {
			// Tab reaching a post's text or control brings it into view.
			const auto reason = static_cast<QFocusEvent*>(aEvent)->reason();
			if (reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason) {
				reveal_focus(*static_cast<QWidget*>(aWatched));
			}
			return false;
		}
		if (aEvent->type() == QEvent::Wheel && OpenCard != nullptr && OpenCard->isAncestorOf(qobject_cast<QWidget*>(aWatched))) {
			// A sideways wheel is a wide code block's or table's own.
			auto*      wheel = static_cast<QWheelEvent*>(aEvent);
			const auto delta = wheel->pixelDelta().isNull() ? wheel->angleDelta() : wheel->pixelDelta();
			if (std::abs(delta.x()) > std::abs(delta.y())) {
				return false;
			}
			wheelEvent(wheel);
			return true;
		}
		if (aEvent->type() == QEvent::Wheel && aWatched == OpenCard) {
			wheelEvent(static_cast<QWheelEvent*>(aEvent));
			return true;
		}
		return QAbstractScrollArea::eventFilter(aWatched, aEvent);
	}
} // namespace bpr::qt
