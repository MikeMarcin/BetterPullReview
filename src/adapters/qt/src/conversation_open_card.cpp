#include "conversation_open_card.hpp"

#include "conversation_glyphs.hpp"

#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/suggestion_mark.hpp"

#include <QApplication>
#include <QFontMetricsF>
#include <QTextEdit>

#include <QAbstractTextDocumentLayout>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The prototype's measures: the rail's right edge, the avatar column,
		/// where a post's text starts, and its padding.
		constexpr int rail_right    = 8;
		constexpr int gut_left      = 12;
		constexpr int avatar_size   = 18;
		constexpr int mark_gap      = 3;
		constexpr int mark_size     = 16;
		constexpr int text_left     = gut_left + avatar_size + 9;
		constexpr int text_right    = 10;
		constexpr int post_padding  = 8;
		constexpr int header_height = 34;
		constexpr int reply_minimum = 54;
		constexpr int reply_maximum = 220;

		[[nodiscard]] auto mix(const QColor& aFrom, const QColor& aTo, const qreal aAmount) -> QColor
		{
			const auto blend = [aAmount](const int aA, const int aB) {
				return static_cast<int>(std::lround(aA + (aB - aA) * aAmount));
			};
			return QColor{ blend(aFrom.red(), aTo.red()), blend(aFrom.green(), aTo.green()), blend(aFrom.blue(), aTo.blue()) };
		}

		[[nodiscard]] auto with_alpha(QColor aColour, const qreal aAlpha) -> QColor
		{
			aColour.setAlphaF(static_cast<float>(aAlpha));
			return aColour;
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

		/// \brief [[BPR-1086]]. Where a published tour stop's label line is in
		/// the post, as the reader shows it now: the stop opens with its pinned
		/// source and generated heading, so its narrative's label is further
		/// down and its mark goes beside it. Nothing for any other post, or
		/// when the label is not found as shown.
		[[nodiscard]] auto narrative_label_line(const card_post& aPost, const markdown_body_view& aReader, const QWidget& aPostView) -> std::optional<QRect>
		{
			if (aPost.Lead <= 0 || aPost.HeaderLength <= 0) {
				return std::nullopt;
			}
			// The source shows the body as written, so the label is looked for
			// from the narrative on; the rendered body has no pinned source or
			// heading markup before it, only their text.
			const auto header = aPost.Body.mid(aPost.Lead, aPost.HeaderLength).trimmed();
			const auto source = aReader.source_visible();
			auto       found  = source ? aReader.source_view()->document()->find(header, static_cast<int>(aPost.Lead)) : aReader.rendered_view()->document()->find(header);
			if (found.isNull()) {
				return std::nullopt;
			}
			found.setPosition(found.selectionStart());
			const auto  line = source ? aReader.source_view()->cursorRect(found) : aReader.rendered_view()->cursorRect(found);
			const auto* view = source ? static_cast<const QAbstractScrollArea*>(aReader.source_view()) : aReader.rendered_view();
			if (line.isEmpty()) {
				return std::nullopt;
			}
			return QRect{ view->viewport()->mapTo(&aPostView, line.topLeft()), line.size() };
		}

		[[nodiscard]] auto mark_colours(const open_card_style& aStyle, const core::feedback_label aLabel) -> feedback_mark_colours
		{
			return feedback_mark_colours{ .Tint  = aStyle.Marks.Tints[static_cast<std::size_t>(aLabel)],
										  .Panel = aStyle.Colours.Panel,
										  .Ink   = aStyle.Colours.Ink,
										  .Style = aStyle.Marks.Style };
		}

		/// \brief A one-line state beside Show line: an icon and its words.
		class state_label final : public QWidget
		{
		public:
			state_label(const QString& aText, std::optional<conversation_glyph> aGlyph, QColor aInk, QWidget* aParent)
				: QWidget{ aParent }
				, Text{ aText }
				, Glyph{ aGlyph }
				, Ink{ std::move(aInk) }
			{
				setObjectName(QStringLiteral("threadState"));
				setAccessibleName(aText);
				setFont(scaled(font(), 0.92));
				setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto metrics = QFontMetrics{ font() };
				return { (Glyph ? 16 : 0) + metrics.horizontalAdvance(Text) + 6, std::max(18, metrics.height()) };
			}

			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				return { Glyph ? 16 : 8, sizeHint().height() };
			}

			void set_text(const QString& aText)
			{
				if (Text == aText) {
					return;
				}
				Text = aText;
				setAccessibleName(aText);
				updateGeometry();
				update();
			}

			/// \brief [[BPR-1212]] v002. Shows the words whole or not at all:
			/// where they do not fit, the glyph alone, with the sentence as its
			/// tooltip (owner, publish-draft-v001: "just noticed this 'only
			/// you see it unti...' doesnt fit.").
			void show_whole_or_glyph(const QString& aSentence)
			{
				Whole = true;
				setToolTip(aSentence);
				setAccessibleDescription(aSentence);
				update();
			}

			/// \brief Whether the words are painted, not only the glyph.
			[[nodiscard]] auto words_shown() const -> bool
			{
				return !Whole || QFontMetrics{ font() }.horizontalAdvance(Text) <= width() - text_left();
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				if (Glyph) {
					paint_conversation_glyph(painter, QRectF{ 2.0, (height() - 12) / 2.0, 12.0, 12.0 }, *Glyph, Ink);
				}
				// Kept beside the paint, so a reader of the card can tell what
				// the reader of the screen sees.
				setProperty("wordsShown", words_shown());
				if (!words_shown()) {
					return;
				}
				const auto x = text_left();
				painter.setPen(Ink);
				painter.drawText(QRect{ x, 0, width() - x, height() }, Qt::AlignLeft | Qt::AlignVCenter,
								 QFontMetrics{ font() }.elidedText(Text, Qt::ElideRight, std::max(0, width() - x)));
			}

		private:
			[[nodiscard]] auto text_left() const -> int
			{
				return Glyph ? 18 : 2;
			}

			QString                           Text;
			std::optional<conversation_glyph> Glyph;
			QColor                            Ink;
			bool                              Whole = false;
		};

		/// \brief The "New" rule between a read post and the first new one.
		///
		/// \details Never a thread's first element: when the first post is
		/// already new, the header's `new_pill` says so instead (the owner,
		/// rejecting the built drawer v002: a rule under the header "kind of
		/// looks like it belongs to the thread above it"). It takes the place
		/// of the hairline between the two posts and is inset from both sides,
		/// so the card's border and amber edge run through it unbroken; its
		/// word sits at the right end, a marker on the line, not a heading.
		class new_rule final : public QWidget
		{
		public:
			new_rule(QColor aInk, QWidget* aParent)
				: QWidget{ aParent }
				, Ink{ std::move(aInk) }
			{
				setObjectName(QStringLiteral("threadNewRule"));
				setAccessibleName(QStringLiteral("New posts follow"));
				auto small = semibold(scaled(font(), 0.82));
				small.setWeight(QFont::Bold);
				setFont(small);
				setFixedHeight(QFontMetrics{ font() }.height() + 4);
			}

			/// \brief Where the line runs, in the rule's coordinates: inside
			/// the card's edge on the left and short of the word on the right.
			[[nodiscard]] auto line_span() const -> std::pair<int, int>
			{
				return { gut_left, word_left() - 6 };
			}

			[[nodiscard]] auto word_left() const -> int
			{
				return width() - text_right - QFontMetrics{ font() }.horizontalAdvance(QStringLiteral("New"));
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto       painter    = QPainter{ this };
				const auto words      = QStringLiteral("New");
				const auto [from, to] = line_span();
				painter.setPen(QPen{ with_alpha(Ink, 0.6), 1.0 });
				painter.drawLine(QPointF{ static_cast<qreal>(from), height() / 2.0 }, QPointF{ static_cast<qreal>(to), height() / 2.0 });
				painter.setPen(Ink);
				painter.drawText(QRect{ word_left(), 0, width() - word_left(), height() }, Qt::AlignLeft | Qt::AlignVCenter, words);
			}

		private:
			QColor Ink;
		};

		/// \brief The header's "N new": the closed note's blue count, kept
		/// beside the line number while the thread is open.
		class new_pill final : public QWidget
		{
		public:
			new_pill(const std::size_t aCount, QColor aFill, QColor aInk, QWidget* aParent)
				: QWidget{ aParent }
				, Words{ QStringLiteral("%1 new").arg(aCount) }
				, Fill{ std::move(aFill) }
				, Ink{ std::move(aInk) }
			{
				setObjectName(QStringLiteral("threadNewPill"));
				const auto spoken = aCount == 1U ? QStringLiteral("1 new post") : QStringLiteral("%1 new posts").arg(aCount);
				setAccessibleName(spoken);
				setToolTip(spoken);
				auto small = semibold(scaled(font(), 0.8));
				small.setWeight(QFont::Bold);
				setFont(small);
				setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			}

			[[nodiscard]] auto text() const -> const QString&
			{
				return Words;
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto metrics = QFontMetrics{ font() };
				return { metrics.horizontalAdvance(Words) + 12, std::max(16, metrics.height() + 2) };
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(Qt::NoPen);
				painter.setBrush(Fill);
				painter.drawRoundedRect(QRectF{ rect() }, height() / 2.0, height() / 2.0);
				painter.setPen(Ink);
				painter.drawText(rect(), Qt::AlignCenter, Words);
			}

		private:
			QString Words;
			QColor  Fill;
			QColor  Ink;
		};

		/// \brief The header row: Show line, the state, the actions, and a rule
		/// under them. A press on its empty space closes the thread.
		class card_header final : public QWidget
		{
		public:
			explicit card_header(QColor aRule, QWidget* aParent)
				: QWidget{ aParent }
				, Rule{ std::move(aRule) }
			{
				setObjectName(QStringLiteral("threadHeader"));
				setFixedHeight(header_height);
				setCursor(Qt::PointingHandCursor);
			}
			std::function<void()> Pressed;

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setPen(QPen{ Rule, 1.0 });
				painter.drawLine(QPointF{ 0.0, height() - 0.5 }, QPointF{ static_cast<qreal>(width()), height() - 0.5 });
			}
			void mousePressEvent(QMouseEvent* aEvent) override
			{
				if (aEvent->button() == Qt::LeftButton && childAt(aEvent->position().toPoint()) == nullptr && Pressed) {
					Pressed();
					aEvent->accept();
					return;
				}
				QWidget::mousePressEvent(aEvent);
			}

		private:
			QColor Rule;
		};

		/// \brief The reply's editor: plain text, growing with what is typed.
		/// \details A `QPlainTextEdit`, as the composer's body is, so the
		/// composer's own label controls work on it ([[BPR-1083]]).
		class reply_text final : public QPlainTextEdit
		{
		public:
			explicit reply_text(QWidget* aParent)
				: QPlainTextEdit{ aParent }
			{
				setObjectName(QStringLiteral("threadReplyText"));
				setFrameShape(QFrame::NoFrame);
				setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
				setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
				setTabChangesFocus(true);
				document()->setDocumentMargin(0.0);
				viewport()->setAutoFillBackground(false);
				setAutoFillBackground(false);
				setPlaceholderText(QStringLiteral("Reply. Start with a label, as in question: or issue (blocking):"));
				fit();
				connect(this, &QPlainTextEdit::textChanged, this, [this] { fit(); });
			}

			void fit()
			{
				// The plain layout measures the document in lines, wrapped
				// lines included.
				const auto lines  = std::max<qreal>(1.0, document()->size().height());
				const auto pixels = static_cast<int>(std::ceil(lines * fontMetrics().lineSpacing()));
				const auto height = std::clamp(pixels + 2, reply_minimum, reply_maximum);
				if (height != this->height() || minimumHeight() != height) {
					setFixedHeight(height);
				}
			}

		protected:
			void resizeEvent(QResizeEvent* aEvent) override
			{
				QPlainTextEdit::resizeEvent(aEvent);
				fit();
			}
		};

		/// \brief The editor's frame: a signal border with a soft glow.
		class reply_frame final : public QWidget
		{
		public:
			reply_frame(QColor aSignal, QColor aSurface, QWidget* aParent)
				: QWidget{ aParent }
				, Signal{ std::move(aSignal) }
				, Surface{ std::move(aSurface) }
			{
				setObjectName(QStringLiteral("threadReplyEditor"));
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box = QRectF{ rect() }.adjusted(3.0, 3.0, -3.0, -3.0);
				painter.setPen(QPen{ with_alpha(Signal, 0.22), 4.0 });
				painter.setBrush(Qt::NoBrush);
				painter.drawRoundedRect(box, 7.0, 7.0);
				painter.setPen(QPen{ Signal, 1.0 });
				painter.setBrush(Surface);
				painter.drawRoundedRect(box, 7.0, 7.0);
			}

		private:
			QColor Signal;
			QColor Surface;
		};

		/// \brief Draws the composer's label chip as the drawer's quiet pill:
		/// a hairline in the rule colour on a tint of the panel, its mark and
		/// name in the secondary ink, and the signal colour when it has focus.
		/// \brief [[BPR-1212]]. What a suggestion draft changes: the lines its
		/// block removes and the lines it adds, under the draft's field.
		class suggestion_change_view final : public QWidget
		{
		public:
			suggestion_change_view(const open_card_style& aStyle, QWidget* aParent)
				: QWidget{ aParent }
				, Style{ aStyle }
			{
				setObjectName(QStringLiteral("threadDraftChange"));
				setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
				setFocusPolicy(Qt::NoFocus);
			}

			void present(const std::optional<core::suggested_change>& aChange, const QString& aLines)
			{
				if (Change == aChange && Lines == aLines) {
					return;
				}
				Change     = aChange;
				Lines      = aLines;
				auto words = QString{ };
				if (Change.has_value()) {
					if (Change->unchanged()) {
						words = QStringLiteral("Suggests a change to %1. Edit the lines in the block; the change shows here.").arg(Lines.toLower());
					} else {
						words = QStringLiteral("What it changes in %1: removes %2, adds %3.").arg(Lines.toLower()).arg(Change->Removed.size()).arg(Change->Added.size());
					}
				}
				setAccessibleName(QStringLiteral("What it changes"));
				setAccessibleDescription(words);
				setToolTip(words);
				setVisible(Change.has_value());
				updateGeometry();
				update();
			}

			[[nodiscard]] auto hasHeightForWidth() const -> bool override
			{
				return true;
			}

			[[nodiscard]] auto heightForWidth(int /*aWidth*/) const -> int override
			{
				if (!Change.has_value()) {
					return 0;
				}
				const auto code = QFontMetrics{ Style.CodeFont }.lineSpacing() + 2;
				const auto rows = Change->unchanged() ? 1 : static_cast<int>(Change->Removed.size() + Change->Added.size());
				return caption_height() + 6 + std::max(1, rows) * code + 2;
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				return { 200, heightForWidth(200) };
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				if (!Change.has_value()) {
					return;
				}
				auto        painter = QPainter{ this };
				const auto& colours = Style.Colours;
				painter.setRenderHint(QPainter::Antialiasing, true);
				auto caption = scaled(font(), 0.92);
				painter.setFont(caption);
				painter.setPen(colours.Faint);
				paint_conversation_glyph(painter, QRectF{ 0.0, (caption_height() - 12) / 2.0, 12.0, 12.0 }, conversation_glyph::suggest, colours.Faint);
				const auto title = Change->unchanged() ? QStringLiteral("Suggests a change to %1").arg(Lines.toLower())
													   : QStringLiteral("What it changes in %1").arg(Lines.toLower());
				painter.drawText(QRect{ 16, 0, width() - 16, caption_height() }, Qt::AlignLeft | Qt::AlignVCenter,
								 QFontMetrics{ caption }.elidedText(title, Qt::ElideRight, std::max(0, width() - 16)));
				const auto top  = caption_height() + 6;
				const auto box  = QRectF{ 0.5, top + 0.5, width() - 1.0, height() - top - 1.0 };
				auto       clip = QPainterPath{ };
				clip.addRoundedRect(box, 6.0, 6.0);
				painter.setPen(QPen{ colours.Rule, 1.0 });
				painter.setBrush(colours.CodeBackground.isValid() ? colours.CodeBackground : colours.Panel);
				painter.drawPath(clip);
				painter.save();
				painter.setClipPath(clip);
				painter.setFont(Style.CodeFont);
				const auto metrics = QFontMetrics{ Style.CodeFont };
				const auto row     = metrics.lineSpacing() + 2;
				auto       y       = top + 1;
				if (Change->unchanged()) {
					painter.setFont(caption);
					painter.setPen(colours.Faint);
					painter.drawText(QRect{ 8, y, width() - 16, row }, Qt::AlignLeft | Qt::AlignVCenter,
									 QFontMetrics{ caption }.elidedText(QStringLiteral("Edit the lines in the block above; the change shows here."), Qt::ElideRight, std::max(0, width() - 16)));
					painter.restore();
					return;
				}
				// Red and green held to the panel, as a diff's own rows are.
				const auto removedInk = colours.Dark ? QColor{ 0xFF, 0x9C, 0x94 } : QColor{ 0xB4, 0x23, 0x18 };
				const auto addedInk   = colours.Dark ? QColor{ 0x6F, 0xD5, 0x8A } : QColor{ 0x1A, 0x7F, 0x37 };
				const auto line       = [&](const std::string& aText, const QColor& aInk, const QChar aSign) {
					auto fill = aInk;
					fill.setAlphaF(colours.Dark ? 0.16F : 0.10F);
					painter.fillRect(QRect{ 1, y, width() - 2, row }, fill);
					painter.setPen(aInk);
					painter.drawText(QRect{ 8, y, 12, row }, Qt::AlignLeft | Qt::AlignVCenter, QString{ aSign });
					auto text = QString::fromStdString(aText);
					text.replace(QLatin1Char{ '\t' }, QStringLiteral("    "));
					painter.drawText(QRect{ 22, y, width() - 30, row }, Qt::AlignLeft | Qt::AlignVCenter,
									 metrics.elidedText(text, Qt::ElideRight, std::max(0, width() - 30)));
					y += row;
				};
				for (const auto& removed : Change->Removed) {
					line(removed, removedInk, QChar{ 0x2212 });
				}
				for (const auto& added : Change->Added) {
					line(added, addedInk, QLatin1Char{ '+' });
				}
				painter.restore();
			}

		private:
			[[nodiscard]] auto caption_height() const -> int
			{
				return std::max(16, QFontMetrics{ scaled(font(), 0.92) }.height());
			}

			open_card_style                       Style;
			std::optional<core::suggested_change> Change;
			QString                               Lines;
		};

		void style_reply_chip(QToolButton& aChip, const open_card_style& aStyle)
		{
			const auto& colours = aStyle.Colours;
			aChip.setAutoRaise(false);
			aChip.setFont(semibold(scaled(aChip.font(), 0.92)));
			aChip.setStyleSheet(QStringLiteral("QToolButton { border: 1px solid %1; border-radius: 6px; background: %2; color: %3; padding: 2px 8px 2px 4px; }"
											   "QToolButton:hover { background: %4; }"
											   "QToolButton:pressed { background: %4; }"
											   "QToolButton:focus { border-color: %5; }"
											   "QToolButton:disabled { color: %6; }")
									.arg(colours.Rule.name(), mix(colours.Panel, colours.Ink, 0.04).name(), colours.Ink2.name(),
										 mix(colours.Panel, colours.Ink, 0.09).name(), colours.Signal.name(), colours.Faint.name()));
		}

		[[nodiscard]] auto tool(QWidget* aParent, const QString& aName, const QString& aTip, const conversation_glyph aGlyph, const QPalette& aPalette) -> QToolButton*
		{
			auto* const button = new QToolButton{ aParent };
			button->setObjectName(aName);
			button->setAutoRaise(true);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setIcon(conversation_icon(aGlyph, aPalette));
			button->setProperty("conversationGlyph", static_cast<int>(aGlyph));
			button->setIconSize(QSize{ 16, 16 });
			button->setToolButtonStyle(Qt::ToolButtonIconOnly);
			button->setToolTip(aTip);
			button->setAccessibleName(aTip.section(QStringLiteral("  "), 0, 0));
			button->setCursor(Qt::ArrowCursor);
			return button;
		}
	} // namespace

	auto note_colours_of(const QPalette& aPalette, const QColor& aNew, const QColor& aRoute) -> note_colours
	{
		auto       colours = note_colours{ };
		const auto ink     = aPalette.color(QPalette::Active, QPalette::Text);
		const auto base    = aPalette.color(QPalette::Active, QPalette::Base);
		colours.Dark       = base.lightness() < 128;
		colours.Ink        = ink;
		colours.Ink2       = mix(ink, base, 0.28);
		colours.Faint      = aPalette.color(QPalette::Active, QPalette::PlaceholderText);
		colours.Rule       = mix(ink, base, 0.86);
		colours.Rule2      = mix(ink, base, 0.74);
		colours.Panel      = base;
		colours.Drawer     = aPalette.color(QPalette::Active, QPalette::Window);
		colours.Hover      = mix(base, ink, 0.035);
		colours.New        = aNew;
		colours.OnNew      = aNew.lightness() > 150 ? QColor{ 0x0D, 0x11, 0x17 } : QColor{ Qt::white };
		colours.Route      = aRoute;
		colours.RouteSoft  = with_alpha(aRoute, colours.Dark ? 0.24 : 0.16);
		colours.RouteLine  = mix(aRoute, base, 0.55);
		colours.Ok         = colours.Dark ? QColor{ 0x3F, 0xB9, 0x50 } : QColor{ 0x1A, 0x7F, 0x37 };
		colours.Signal     = aPalette.color(QPalette::Active, QPalette::Highlight);
		return colours;
	}

	auto avatar_colours_of(const QString& aLogin, const bool aDark) -> avatar_colours
	{
		// A hue a login keeps on every run, from its own characters.
		auto hash = 2166136261U;
		for (const auto character : aLogin) {
			hash = (hash ^ character.unicode()) * 16777619U;
		}
		const auto hue = static_cast<int>(hash % 360U);
		return aDark ? avatar_colours{ QColor::fromHsl(hue, 70, 64), QColor::fromHsl(hue, 150, 204) }
					 : avatar_colours{ QColor::fromHsl(hue, 150, 212), QColor::fromHsl(hue, 150, 52) };
	}

	auto avatar_initials(const QString& aLogin) -> QString
	{
		if (aLogin.isEmpty()) {
			return QStringLiteral("?");
		}
		const auto parts = aLogin.split(QLatin1Char{ '-' }, Qt::SkipEmptyParts);
		if (parts.size() > 1) {
			return (parts[0].left(1) + parts[1].left(1)).toUpper();
		}
		return aLogin.left(1).toUpper();
	}

	void paint_avatar(QPainter& aPainter, const QRectF& aBox, const QString& aLogin, const bool aMine, const note_colours& aColours, const QFont& aFont)
	{
		const auto colours = avatar_colours_of(aLogin, aColours.Dark);
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		if (aMine) {
			aPainter.setPen(QPen{ aColours.Faint, 1.0 });
			aPainter.setBrush(Qt::NoBrush);
			aPainter.drawEllipse(aBox.adjusted(-2.0, -2.0, 2.0, 2.0));
		}
		aPainter.setPen(Qt::NoPen);
		aPainter.setBrush(colours.Background);
		aPainter.drawEllipse(aBox);
		auto font = aFont;
		font.setWeight(QFont::Bold);
		font.setPixelSize(std::max(6, static_cast<int>(std::lround(aBox.height() * (avatar_initials(aLogin).size() > 1 ? 0.44 : 0.53)))));
		aPainter.setFont(font);
		aPainter.setPen(colours.Ink);
		aPainter.drawText(aBox, Qt::AlignCenter, avatar_initials(aLogin));
		aPainter.restore();
	}

	post_tool::post_tool(const conversation_glyph aGlyph, const note_colours& aColours, QWidget* const aParent)
		: QAbstractButton{ aParent }
		, Glyph{ aGlyph }
		, Colours{ aColours }
	{
		setFocusPolicy(Qt::StrongFocus);
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_Hover, true);
		anchor_tooltips(*this);
	}

	void post_tool::set_glyph(const conversation_glyph aGlyph)
	{
		Glyph = aGlyph;
		update();
	}

	void post_tool::set_revealed(const bool aRevealed)
	{
		if (Revealed != aRevealed) {
			Revealed = aRevealed;
			update();
		}
	}

	void post_tool::set_on(const bool aOn)
	{
		if (On != aOn) {
			On = aOn;
			update();
		}
	}

	auto post_tool::revealed() const noexcept -> bool
	{
		return Revealed || On || hasFocus();
	}

	auto post_tool::sizeHint() const -> QSize
	{
		return { 22, 22 };
	}

	void post_tool::paintEvent(QPaintEvent* /*aEvent*/)
	{
		// Out of the way until the post is hovered or holds focus: every post
		// has them, and the conversation is what is read.
		if (!revealed()) {
			return;
		}
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto box = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
		if (On || underMouse() || isDown()) {
			painter.setPen(Qt::NoPen);
			painter.setBrush(On ? with_alpha(Colours.Signal, Colours.Dark ? 0.26 : 0.14) : Colours.Hover);
			painter.drawRoundedRect(box, 5.0, 5.0);
		}
		const auto ink  = On ? Colours.Signal : (underMouse() ? Colours.Ink : Colours.Faint);
		const auto side = 14.0;
		paint_conversation_glyph(painter, QRectF{ (width() - side) / 2.0, (height() - side) / 2.0, side, side }, Glyph, ink);
		if (hasFocus()) {
			painter.setPen(QPen{ Colours.Signal, 1.5 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(box, 5.0, 5.0);
		}
	}

	thread_post_view::thread_post_view(const card_post& aPost, const open_card_style& aStyle, const bool aRuleAbove, QWidget* const aParent)
		: QWidget{ aParent }
		, Post{ aPost }
		, Style{ aStyle }
		, RuleAbove{ aRuleAbove }
	{
		setObjectName(QStringLiteral("threadPost"));
		setProperty("postIdentity", QString::fromStdString(aPost.Id));
		setAccessibleName(aPost.Name.isEmpty() ? aPost.Author : aPost.Name);
		setAccessibleDescription(aPost.Unread ? QStringLiteral("New") : QString{ });
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		// RFC-BPR-0036. The body reads through the shared Markdown reader, in
		// place: as tall as its content, its links through the application's
		// admission, its source a press away.
		Reader = new markdown_body_view{ this };
		Reader->setObjectName(QStringLiteral("threadPostReader"));
		Reader->set_in_place(true);
		Reader->set_code_font(aStyle.CodeFont, 4.0 * QFontMetricsF{ aStyle.CodeFont }.horizontalAdvance(QLatin1Char{ ' ' }));
		Reader->set_body(aPost.Body.toStdString(), aPost.Id);
		auto* const rendered = Reader->rendered_view();
		rendered->setObjectName(QStringLiteral("threadPostBody"));
		rendered->setAccessibleName(QStringLiteral("Post by %1").arg(aPost.Author));
		rendered->setAccessibleDescription(QStringLiteral("Read-only post. Tab visits its links; Return opens the link. Wide code and tables scroll sideways."));
		auto* const source = Reader->source_view();
		source->setObjectName(QStringLiteral("threadPostSourceText"));
		source->setAccessibleName(QStringLiteral("Source of the post by %1").arg(aPost.Author));
		Reader->set_height_observer([this] {
			updateGeometry();
			place_children();
			// [[BPR-1086]]. A tour stop's mark follows its label line.
			update();
			if (HeightChanged) {
				HeightChanged();
			}
		});
		SourceButton = new post_tool{ conversation_glyph::source, aStyle.Colours, this };
		SourceButton->setObjectName(QStringLiteral("threadPostSource"));
		connect(SourceButton, &QAbstractButton::clicked, this, [this] {
			Reader->show_source(!Reader->source_visible());
			refresh_source_button();
		});
		CopyButton = new post_tool{ conversation_glyph::copy, aStyle.Colours, this };
		CopyButton->setObjectName(QStringLiteral("threadPostCopySource"));
		CopyButton->setToolTip(QStringLiteral("Copy source"));
		CopyButton->setAccessibleName(QStringLiteral("Copy original Markdown source"));
		connect(CopyButton, &QAbstractButton::clicked, this, [this] {
			Reader->copy_source();
			show_anchored_tooltip(*CopyButton, CopyButton->rect(), QStringLiteral("Source copied"));
		});
		// A fallback body already shows its source; there is nothing to switch to.
		SourceButton->setEnabled(Reader->rendered_available());
		connect(qApp, &QApplication::focusChanged, this, [this] { refresh_tools(); });
		refresh_source_button();
	}

	void thread_post_view::refresh_source_button()
	{
		const auto source = Reader->source_visible() && Reader->rendered_available();
		SourceButton->set_glyph(source ? conversation_glyph::rendered : conversation_glyph::source);
		SourceButton->set_on(source);
		SourceButton->setToolTip(source ? QStringLiteral("Show rendered") : QStringLiteral("View source"));
		SourceButton->setAccessibleName(source ? QStringLiteral("Show rendered Markdown") : QStringLiteral("View original Markdown source"));
	}

	auto thread_post_view::name_height() const -> int
	{
		return std::max(19, QFontMetrics{ semibold(font()) }.height());
	}

	void thread_post_view::place_children()
	{
		const auto name  = name_height();
		const auto width = std::max(1, this->width() - text_left - text_right);
		Reader->setGeometry(text_left, post_padding + name + 1, width, Reader->heightForWidth(width));
		const auto size = SourceButton->sizeHint();
		const auto top  = post_padding + (name - size.height()) / 2;
		CopyButton->setGeometry(this->width() - text_right + 4 - size.width(), top, size.width(), size.height());
		SourceButton->setGeometry(CopyButton->x() - 2 - size.width(), top, size.width(), size.height());
	}

	void thread_post_view::refresh_tools()
	{
		const auto revealed = Hovered || isAncestorOf(QApplication::focusWidget());
		SourceButton->set_revealed(revealed);
		CopyButton->set_revealed(revealed);
	}

	void thread_post_view::enterEvent(QEnterEvent* const aEvent)
	{
		QWidget::enterEvent(aEvent);
		Hovered = true;
		refresh_tools();
	}

	void thread_post_view::leaveEvent(QEvent* const aEvent)
	{
		QWidget::leaveEvent(aEvent);
		Hovered = false;
		refresh_tools();
	}

	auto thread_post_view::hasHeightForWidth() const -> bool
	{
		return true;
	}

	auto thread_post_view::heightForWidth(const int aWidth) const -> int
	{
		const auto text = name_height() + 1 + Reader->heightForWidth(std::max(1, aWidth - text_left - text_right));
		const auto gut  = Post.Label ? avatar_size + mark_gap + mark_size + 1 : avatar_size + 1;
		return post_padding + std::max(text, gut) + post_padding;
	}

	auto thread_post_view::sizeHint() const -> QSize
	{
		const auto width = std::max(200, this->width());
		return { width, heightForWidth(width) };
	}

	auto thread_post_view::reader() const noexcept -> markdown_body_view*
	{
		return Reader;
	}

	auto thread_post_view::source_button() const noexcept -> post_tool*
	{
		return SourceButton;
	}

	auto thread_post_view::copy_button() const noexcept -> post_tool*
	{
		return CopyButton;
	}

	auto thread_post_view::post() const noexcept -> const card_post&
	{
		return Post;
	}

	auto thread_post_view::mark_rect() const -> QRect
	{
		if (!Post.Label) {
			return { };
		}
		// [[BPR-1086]]. A labelled tour stop's mark sits beside its
		// narrative's label line, below the pinned source and heading.
		const auto beneath = post_padding + 1 + avatar_size + mark_gap;
		if (const auto label = narrative_label_line(Post, *Reader, *this)) {
			return QRect{ gut_left + (avatar_size - mark_size) / 2, std::max(beneath, label->center().y() - mark_size / 2 + 1), mark_size, mark_size };
		}
		return QRect{ gut_left + (avatar_size - mark_size) / 2, beneath, mark_size, mark_size };
	}

	auto thread_post_view::rail_rect() const -> QRectF
	{
		const auto width = feedback_rail_width(Post.Emphasis);
		return QRectF{ rail_right - width, 10.0, width, static_cast<qreal>(std::max(0, height() - 16)) };
	}

	void thread_post_view::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		place_children();
	}

	void thread_post_view::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto        painter = QPainter{ this };
		const auto& colours = Style.Colours;
		painter.setRenderHint(QPainter::Antialiasing, true);
		// [[BPR-1293]]. The post picked in Threads is washed amber with
		// an amber edge, as the prototype marks it.
		if (!Style.Picked.empty() && Style.Picked == Post.Id) {
			painter.fillRect(rect(), colours.RouteSoft);
			painter.fillRect(QRect{ 0, 0, 3, height() }, colours.Route);
		}
		if (RuleAbove) {
			painter.setPen(QPen{ with_alpha(colours.Rule, 0.6), 1.0 });
			painter.drawLine(QPointF{ 0.0, 0.5 }, QPointF{ static_cast<qreal>(width()), 0.5 });
		}
		const auto rail = rail_rect();
		paint_feedback_rail(painter, rail.right(), rail.top(), rail.bottom(),
							Post.Label ? Style.Marks.Tints[static_cast<std::size_t>(*Post.Label)] : colours.Rule2, Post.Emphasis);
		paint_avatar(painter, QRectF{ static_cast<qreal>(gut_left), static_cast<qreal>(post_padding + 1), static_cast<qreal>(avatar_size), static_cast<qreal>(avatar_size) },
					 Post.Author, Post.Mine, colours, font());
		if (Post.Label) {
			paint_feedback_mark(painter, QRectF{ mark_rect() }, *Post.Label, Post.Emphasis, mark_colours(Style, *Post.Label));
		}
		const auto bold = semibold(font());
		const auto name = std::max(19, QFontMetrics{ bold }.height());
		auto       x    = text_left;
		const auto who  = Post.Mine ? QStringLiteral("you") : Post.Author;
		painter.setFont(bold);
		painter.setPen(colours.Ink);
		const auto tools = SourceButton->x() - 4;
		painter.drawText(QRect{ x, post_padding, std::max(0, tools - x), name }, Qt::AlignLeft | Qt::AlignVCenter, who);
		x += QFontMetrics{ bold }.horizontalAdvance(who) + 7;
		if (!Post.ReplyingTo.isEmpty()) {
			const auto small = scaled(font(), 0.92);
			const auto words = QStringLiteral("↩ %1").arg(Post.ReplyingTo);
			painter.setFont(small);
			painter.setPen(colours.Faint);
			painter.drawText(QRect{ x, post_padding, std::max(0, tools - x), name }, Qt::AlignLeft | Qt::AlignVCenter, words);
			x += QFontMetrics{ small }.horizontalAdvance(words) + 7;
		}
		if (Post.Unread) {
			painter.setPen(Qt::NoPen);
			painter.setBrush(colours.New);
			painter.drawEllipse(QRectF{ static_cast<qreal>(x), post_padding + name / 2.0 - 3.5, 7.0, 7.0 });
		}
	}

	auto thread_post_view::event(QEvent* const aEvent) -> bool
	{
		if (aEvent->type() == QEvent::ToolTip) {
			const auto* const help  = static_cast<QHelpEvent*>(aEvent);
			const auto        point = help->pos();
			const auto        mark  = mark_rect();
			// [[BPR-1083]] v004: the name sits right below the mark it names,
			// also when the rail is hovered.
			if (mark.contains(point) || rail_rect().adjusted(-3.0, 0.0, 3.0, 0.0).contains(QPointF{ point })) {
				show_anchored_tooltip(*this, mark.isEmpty() ? rail_rect().toAlignedRect() : mark, Post.Name);
				return true;
			}
			if (QRect{ gut_left, post_padding, avatar_size, avatar_size }.contains(point)) {
				show_anchored_tooltip(*this, QRect{ gut_left, post_padding, avatar_size, avatar_size }, Post.Mine ? QStringLiteral("%1 (you)").arg(Post.Author) : Post.Author);
				return true;
			}
			hide_anchored_tooltip();
			aEvent->ignore();
			return true;
		}
		return QWidget::event(aEvent);
	}

	note_button::note_button(const kind aKind, const QString& aText, QWidget* const aParent)
		: QAbstractButton{ aParent }
		, Kind{ aKind }
	{
		setText(aText);
		setFocusPolicy(Qt::StrongFocus);
		setCursor(aKind == kind::reply_field ? Qt::IBeamCursor : Qt::PointingHandCursor);
		if (aKind == kind::primary || aKind == kind::secondary || aKind == kind::split) {
			setFont(semibold(font()));
		}
		setSizePolicy(aKind == kind::reply_field ? QSizePolicy::Expanding : QSizePolicy::Maximum, QSizePolicy::Fixed);
		setAttribute(Qt::WA_Hover, true);
	}

	void note_button::set_style(const open_card_style& aStyle)
	{
		Style = aStyle;
		update();
	}

	void note_button::set_joined(const bool aJoined)
	{
		if (Joined == aJoined) {
			return;
		}
		Joined = aJoined;
		update();
	}

	auto note_button::sizeHint() const -> QSize
	{
		const auto metrics = QFontMetrics{ font() };
		if (Kind == kind::reply_field) {
			return { 200, std::max(32, metrics.height() + 12) };
		}
		if (Kind == kind::split) {
			return { 24, std::max(26, metrics.height() + 8) };
		}
		return { metrics.horizontalAdvance(text()) + (icon().isNull() ? 22 : 42), std::max(26, metrics.height() + 8) };
	}

	void note_button::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto        painter = QPainter{ this };
		const auto& colours = Style.Colours;
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto box = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
		if (Kind == kind::split) {
			// [[BPR-1345]] v002. The primary pill's own fill, its left edge
			// square against the pill, a hairline between, and a chevron.
			auto fill = colours.Signal;
			if (underMouse() && isEnabled()) {
				fill = fill.lighter(107);
			}
			const auto ink = colours.Signal.lightness() > 150 ? QColor{ 0x0D, 0x11, 0x17 } : QColor{ Qt::white };
			painter.setOpacity(isEnabled() ? 1.0 : 0.45);
			painter.setPen(Qt::NoPen);
			painter.setBrush(fill);
			painter.drawRoundedRect(box.adjusted(-1.0, 0.0, 0.0, 0.0), 6.0, 6.0);
			painter.drawRect(QRectF{ box.left() - 1.0, box.top(), 7.0, box.height() });
			auto divider = ink;
			divider.setAlphaF(0.35F);
			painter.setPen(QPen{ divider, 1.0 });
			painter.drawLine(QPointF{ box.left(), box.top() + 5.0 }, QPointF{ box.left(), box.bottom() - 5.0 });
			const auto middle = QPointF{ box.center().x() + 0.5, box.center().y() };
			auto       arrow  = QPainterPath{ };
			arrow.moveTo(middle + QPointF{ -3.5, -1.5 });
			arrow.lineTo(middle + QPointF{ 0.0, 2.0 });
			arrow.lineTo(middle + QPointF{ 3.5, -1.5 });
			painter.setPen(QPen{ ink, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
			painter.setBrush(Qt::NoBrush);
			painter.drawPath(arrow);
			painter.setOpacity(1.0);
		} else if (Kind == kind::primary) {
			auto fill = colours.Signal;
			if (underMouse() && isEnabled()) {
				fill = fill.lighter(107);
			}
			painter.setOpacity(isEnabled() ? 1.0 : 0.45);
			painter.setPen(Qt::NoPen);
			painter.setBrush(fill);
			painter.drawRoundedRect(Joined ? box.adjusted(0.0, 0.0, 1.0, 0.0) : box, 6.0, 6.0);
			if (Joined) {
				// Joined to its split arrow: the right edge is square.
				painter.drawRect(QRectF{ box.right() - 6.0, box.top(), 7.0, box.height() });
			}
			const auto ink     = colours.Signal.lightness() > 150 ? QColor{ 0x0D, 0x11, 0x17 } : QColor{ Qt::white };
			const auto metrics = QFontMetrics{ font() };
			auto       x       = (width() - metrics.horizontalAdvance(text()) - (icon().isNull() ? 0 : 20)) / 2;
			if (!icon().isNull()) {
				paint_conversation_glyph(painter, QRectF{ static_cast<qreal>(x), (height() - 14) / 2.0, 14.0, 14.0 }, conversation_glyph::send, ink);
				x += 20;
			}
			painter.setPen(ink);
			painter.drawText(QRect{ x, 0, width() - x, height() }, Qt::AlignLeft | Qt::AlignVCenter, text());
			painter.setOpacity(1.0);
		} else if (Kind == kind::secondary) {
			// [[BPR-1345]]. The same pill, outlined: the panel's own ground,
			// the rule for its edge and the ink for its words.
			painter.setOpacity(isEnabled() ? 1.0 : 0.45);
			painter.setPen(QPen{ underMouse() && isEnabled() ? colours.Faint : colours.Rule2, 1.0 });
			painter.setBrush(colours.Panel);
			painter.drawRoundedRect(box, 6.0, 6.0);
			const auto metrics = QFontMetrics{ font() };
			const auto x       = (width() - metrics.horizontalAdvance(text())) / 2;
			painter.setPen(colours.Ink);
			painter.drawText(QRect{ x, 0, width() - x, height() }, Qt::AlignLeft | Qt::AlignVCenter, text());
			painter.setOpacity(1.0);
		} else {
			painter.setPen(QPen{ underMouse() ? colours.Faint : colours.Rule2, 1.0 });
			painter.setBrush(colours.CodeBackground.isValid() ? colours.CodeBackground : colours.Panel);
			painter.drawRoundedRect(box, 7.0, 7.0);
			paint_avatar(painter, QRectF{ 9.0, (height() - 18) / 2.0, 18.0, 18.0 }, Style.Viewer, true, colours, font());
			painter.setPen(colours.Faint);
			painter.drawText(QRect{ 36, 0, width() - 70, height() }, Qt::AlignLeft | Qt::AlignVCenter, text());
			const auto small   = semibold(scaled(font(), 0.84));
			const auto metrics = QFontMetrics{ small };
			const auto key     = QStringLiteral("R");
			const auto keyBox  = QRectF{ width() - 9.0 - metrics.horizontalAdvance(key) - 11.0, (height() - metrics.height() - 2) / 2.0,
										 metrics.horizontalAdvance(key) + 11.0, metrics.height() + 2.0 };
			painter.setPen(QPen{ colours.Rule2, 1.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(keyBox, 4.0, 4.0);
			painter.setFont(small);
			painter.setPen(colours.Faint);
			painter.drawText(keyBox, Qt::AlignCenter, key);
		}
		if (hasFocus()) {
			painter.setPen(QPen{ colours.Signal, 2.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(box.adjusted(0.0, 0.0, 0.0, 0.0), 7.0, 7.0);
		}
	}

	open_thread_card::open_thread_card(const thread_card& aCard, const open_card_style& aStyle, QWidget* const aParent)
		: QWidget{ aParent }
		, Id{ aCard.Id }
		, Style{ aStyle }
		, Draft{ aCard.Draft }
		, Writing{ aCard.Writing }
	{
		setObjectName(QStringLiteral("openThreadCard"));
		// [[BPR-1212]]. A draft on the base side says so.
		const auto base = aCard.Side == core::diff_side::left ? QStringLiteral("base ") : QString{ };
		setAccessibleName(Writing          ? (aCard.Line && aCard.FirstLine ? QStringLiteral("Your draft on %1lines %2–%3").arg(base).arg(*aCard.FirstLine).arg(*aCard.Line)
												  : aCard.Line              ? QStringLiteral("Your draft on %1line %2").arg(base).arg(*aCard.Line)
																			: QStringLiteral("Your draft"))
							  : aCard.Line ? QStringLiteral("Thread on line %1").arg(*aCard.Line)
										   : QStringLiteral("Thread"));
		setAttribute(Qt::WA_StyledBackground, false);
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		build(aCard);
	}

	void open_thread_card::build(const thread_card& aCard)
	{
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);
		auto* const header = new card_header{ Style.Colours.Rule, this };
		Header             = header;
		// A press on the header's empty space closes the thread.
		header->Pressed = [this] {
			if (CloseRequested) {
				CloseRequested();
			}
		};
		auto* const row = new QHBoxLayout{ header };
		row->setContentsMargins(8, 0, 4, 0);
		row->setSpacing(2);
		const auto& palette = this->palette();
		if (aCard.Line.has_value()) {
			auto* const where = new QToolButton{ header };
			where->setObjectName(QStringLiteral("threadShowLine"));
			where->setAutoRaise(true);
			where->setFocusPolicy(Qt::StrongFocus);
			where->setIcon(conversation_icon(conversation_glyph::target, palette));
			where->setProperty("conversationGlyph", static_cast<int>(conversation_glyph::target));
			where->setIconSize(QSize{ 12, 12 });
			where->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			auto mono = semibold(Style.CodeFont);
			mono.setPointSizeF(font().pointSizeF() > 0.0 ? font().pointSizeF() * 0.92 : mono.pointSizeF());
			where->setFont(mono);
			where->setText(Style.Feed && !aCard.Path.isEmpty() ? QStringLiteral("%1:%2").arg(aCard.Path.section(QLatin1Char{ '/' }, -1)).arg(*aCard.Line)
															   : QString::number(*aCard.Line));
			where->setToolTip(Style.Feed ? QStringLiteral("Open in Review at %1:%2").arg(aCard.Path).arg(*aCard.Line) : QStringLiteral("Show line %1 in the code").arg(*aCard.Line));
			where->setAccessibleName(Style.Feed ? QStringLiteral("Open in Review at %1:%2").arg(aCard.Path).arg(*aCard.Line) : QStringLiteral("Show line %1").arg(*aCard.Line));
			where->setCursor(Qt::ArrowCursor);
			connect(where, &QToolButton::clicked, this, [this] { if (Action) { Action(card_action::show_line); } });
			row->addWidget(where);
			if (aCard.Writing) {
				// [[BPR-1212]]. The lines being written on, in the writing blue.
				Where    = where;
				auto ink = where->palette();
				ink.setColor(QPalette::ButtonText, Style.Colours.Signal);
				where->setPalette(ink);
				where->setIcon(conversation_icon(conversation_glyph::target, Style.Colours.Signal));
				where->setProperty("conversationGlyph", QVariant{ });
				// [[BPR-1212]] slice 1. "Lines a–b" for a draft on several.
				// [[BPR-1212]]. "Base line N" for a draft on the base side.
				const auto words = aCard.FirstLine ? QStringLiteral("Lines %1–%2").arg(*aCard.FirstLine).arg(*aCard.Line) : QStringLiteral("Line %1").arg(*aCard.Line);
				where->setText(aCard.Side == core::diff_side::left ? QStringLiteral("Base %1").arg(words.toLower()) : words);
			}
		}
		if (!aCard.Draft && aCard.UnreadCount > 0U) {
			row->addSpacing(4);
			row->addWidget(new new_pill{ aCard.UnreadCount, Style.Colours.New, Style.Colours.OnNew, header }, 0, Qt::AlignVCenter);
			row->addSpacing(4);
		}
		if (aCard.Writing) {
			auto* const state = new state_label{ QStringLiteral("Draft"), conversation_glyph::lock, Style.Colours.Signal, header };
			DraftState        = state;
			row->addWidget(state);
		} else if (aCard.Draft && aCard.Pending) {
			// [[BPR-1345]]. Sent with the review when it is finished.
			row->addWidget(new state_label{ QStringLiteral("Pending in your review"), conversation_glyph::lock, Style.Colours.Signal, header });
		} else if (aCard.Draft) {
			row->addWidget(new state_label{ QStringLiteral("Only you see it"), conversation_glyph::lock, Style.Colours.Faint, header });
		} else if (aCard.Resolved) {
			row->addWidget(new state_label{ aCard.Outdated ? QStringLiteral("Resolved, outdated") : QStringLiteral("Resolved"), conversation_glyph::resolved, Style.Colours.Ok, header });
		} else if (aCard.Outdated) {
			row->addWidget(new state_label{ QStringLiteral("Outdated"), std::nullopt, Style.Colours.Faint, header });
		}
		row->addStretch(1);
		if (!aCard.Draft) {
			if (Style.ResolutionOffered && aCard.Kind == QStringLiteral("Inline thread")) {
				auto* const resolve = tool(header, QStringLiteral("threadResolve"), aCard.Resolved ? QStringLiteral("Unresolve") : QStringLiteral("Resolve"),
										   aCard.Resolved ? conversation_glyph::unresolve : conversation_glyph::resolve, palette);
				connect(resolve, &QToolButton::clicked, this, [this, resolved = aCard.Resolved] {
					if (Action) {
						Action(resolved ? card_action::unresolve : card_action::resolve);
					}
				});
				row->addWidget(resolve);
			}
			const auto  unread = aCard.UnreadCount;
			auto* const read   = tool(header, QStringLiteral("threadMarkRead"),
									  unread == 0U ? QStringLiteral("Nothing new") : unread == 1U ? QStringLiteral("Mark 1 new as read")
																								  : QStringLiteral("Mark %1 new as read").arg(unread),
									  conversation_glyph::read, palette);
			read->setEnabled(unread > 0U);
			connect(read, &QToolButton::clicked, this, [this] { if (Action) { Action(card_action::mark_read); } });
			row->addWidget(read);
			auto* const more = tool(header, QStringLiteral("threadMore"), QStringLiteral("More: reply, mark read, resolve, classify"), conversation_glyph::more, palette);
			connect(more, &QToolButton::clicked, this, [this, more] {
				if (MoreRequested) {
					MoreRequested(more->mapToGlobal(QPoint{ 0, more->height() }));
				}
			});
			row->addWidget(more);
		}
		if (aCard.Writing) {
			// [[BPR-1212]]. Maximize: the same draft in Discuss, with room.
			// [[BPR-1345]] v002. ⌘⇧↩ is Add single comment now; Maximize
			// is ⌘⇧M.
			const auto  key      = QKeySequence{ Qt::CTRL | Qt::SHIFT | Qt::Key_M }.toString(QKeySequence::NativeText);
			auto* const maximize = tool(header, QStringLiteral("threadMaximizeDraft"), QStringLiteral("Open in Discuss, with room to write  %1").arg(key), conversation_glyph::maximize, palette);
			maximize->setAccessibleName(QStringLiteral("Open in Discuss"));
			connect(maximize, &QToolButton::clicked, this, [this] { if (Action) { Action(card_action::maximize_draft); } });
			row->addWidget(maximize);
		}
		auto* const close = tool(header, QStringLiteral("threadClose"), aCard.Writing ? QStringLiteral("Close; the draft is kept  Esc") : QStringLiteral("Close  Esc"), conversation_glyph::collapse, palette);
		connect(close, &QToolButton::clicked, this, [this] { if (CloseRequested) { CloseRequested(); } });
		row->addWidget(close);
		column->addWidget(header);

		if (!aCard.DefectBadge.isEmpty()) {
			auto* const badgeRow = new QHBoxLayout{ };
			badgeRow->setContentsMargins(text_left - 4, 6, text_right, 0);
			auto* const badge = new QToolButton{ this };
			badge->setObjectName(QStringLiteral("threadDefectBadge"));
			badge->setText(aCard.DefectBadge);
			badge->setAccessibleName(aCard.DefectBadge);
			badge->setAccessibleDescription(aCard.DefectDescription);
			badge->setToolTip(aCard.DefectDescription);
			badge->setFocusPolicy(Qt::StrongFocus);
			connect(badge, &QToolButton::clicked, this, [this, badge] {
				if (MoreRequested) {
					MoreRequested(badge->mapToGlobal(QPoint{ 0, badge->height() }));
				}
			});
			badgeRow->addWidget(badge);
			badgeRow->addStretch(1);
			column->addLayout(badgeRow);
		}
		if (aCard.FileLevel) {
			auto* const support = new QLabel{ this };
			support->setObjectName(QStringLiteral("threadSupportNote"));
			support->setTextFormat(Qt::PlainText);
			support->setWordWrap(true);
			support->setFont(scaled(font(), 0.9));
			support->setText(QStringLiteral("GitHub stores this as a file review comment. The code link in its first post places the thread at this line."));
			support->setContentsMargins(text_left, 6, text_right, 0);
			column->addWidget(support);
		}
		if (Style.Feed && !aCard.Excerpt.isEmpty()) {
			auto* const excerpt = new QLabel{ this };
			excerpt->setObjectName(QStringLiteral("threadExcerpt"));
			excerpt->setTextFormat(Qt::PlainText);
			excerpt->setFont(Style.CodeFont);
			excerpt->setText(aCard.Excerpt);
			excerpt->setContentsMargins(text_left, 8, text_right, 0);
			column->addWidget(excerpt);
		}
		// The New rule goes only where a read post is followed by the first
		// new one; a thread whose first post is new says so in its header.
		auto seenNew = false;
		auto first   = true;
		for (const auto& post : aCard.Posts) {
			auto ruleAbove = !first;
			if (post.Unread && !seenNew && !aCard.Draft) {
				seenNew = true;
				if (!first) {
					column->addWidget(new new_rule{ Style.Colours.New, this });
					ruleAbove = false;
				}
			}
			auto* const view = new thread_post_view{ post, Style, ruleAbove, this };
			view->reader()->set_link_dispatch([this, id = post.Id](const std::string& aDestination) {
				if (Link) {
					Link(id, QString::fromStdString(aDestination));
				}
			});
			view->HeightChanged = [this] {
				if (HeightChanged) {
					HeightChanged();
				}
			};
			Posts.push_back(view);
			column->addWidget(view);
			first = false;
		}

		Foot                = new QWidget{ this };
		auto* const footRow = new QVBoxLayout{ Foot };
		// [[BPR-1374]]. What became of the thread my unsent reply answers,
		// said once, above what can be done about it.
		auto* notice = static_cast<QLabel*>(nullptr);
		if (!aCard.Notice.isEmpty()) {
			notice = new QLabel{ aCard.Notice, Foot };
			notice->setObjectName(QStringLiteral("threadReplyNotice"));
			notice->setWordWrap(true);
			notice->setTextFormat(Qt::PlainText);
			notice->setAccessibleName(aCard.Notice);
			auto ink = notice->palette();
			ink.setColor(QPalette::WindowText, Style.Colours.Ink2);
			notice->setPalette(ink);
		}
		if (aCard.Writing) {
			build_composer(aCard, *footRow);
		} else if (aCard.Draft && aCard.UnsentReply.has_value()) {
			// [[BPR-1374]]. My kept reply: its thread is gone, so it is never
			// sent there. Its words can become a comment on its line, be
			// copied, or be discarded; nothing here publishes.
			footRow->setContentsMargins(text_left, 2, text_right, 10);
			footRow->setSpacing(8);
			if (notice != nullptr) {
				footRow->addWidget(notice);
			}
			auto* const actions = new QHBoxLayout{ };
			actions->setSpacing(4);
			if (aCard.KeepableAsComment) {
				auto* const keep = new note_button{ note_button::kind::primary, QStringLiteral("Keep as comment"), Foot };
				keep->setObjectName(QStringLiteral("threadKeepReplyAsComment"));
				keep->setIcon(conversation_icon(conversation_glyph::edit, palette));
				keep->setToolTip(aCard.Line ? QStringLiteral("Keep these words as a new comment on line %1. Nothing is published.").arg(*aCard.Line)
											: QStringLiteral("Keep these words as a new comment on their line. Nothing is published."));
				keep->set_style(Style);
				connect(keep, &QAbstractButton::clicked, this, [this] { if (Action) { Action(card_action::keep_reply_as_comment); } });
				actions->addWidget(keep);
			}
			auto* const copy = new QToolButton{ Foot };
			copy->setObjectName(QStringLiteral("threadCopyReply"));
			copy->setAutoRaise(true);
			copy->setText(QStringLiteral("Copy text"));
			copy->setToolTip(QStringLiteral("Copy the reply as written"));
			copy->setIcon(conversation_icon(conversation_glyph::copy, palette));
			copy->setProperty("conversationGlyph", static_cast<int>(conversation_glyph::copy));
			copy->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			copy->setFocusPolicy(Qt::StrongFocus);
			connect(copy, &QToolButton::clicked, this, [this] { if (Action) { Action(card_action::copy_reply); } });
			actions->addWidget(copy);
			actions->addStretch(1);
			auto* const discard = tool(Foot, QStringLiteral("threadDiscardReply"), QStringLiteral("Discard reply"), conversation_glyph::trash, palette);
			connect(discard, &QToolButton::clicked, this, [this] { if (Action) { Action(card_action::discard_reply); } });
			actions->addWidget(discard);
			footRow->addLayout(actions);
		} else if (aCard.Draft) {
			footRow->setContentsMargins(text_left, 4, text_right, 10);
			auto* const actions = new QHBoxLayout{ };
			actions->setSpacing(4);
			auto* const publish = new note_button{ note_button::kind::primary, QStringLiteral("Publish"), Foot };
			publish->setObjectName(QStringLiteral("threadPublishDraft"));
			publish->setIcon(conversation_icon(conversation_glyph::send, palette));
			publish->set_style(Style);
			connect(publish, &QAbstractButton::clicked, this, [this] { if (Action) { Action(card_action::publish_draft); } });
			auto* const edit = new QToolButton{ Foot };
			edit->setObjectName(QStringLiteral("threadEditDraft"));
			edit->setAutoRaise(true);
			edit->setText(QStringLiteral("Edit"));
			edit->setIcon(conversation_icon(conversation_glyph::edit, palette));
			edit->setProperty("conversationGlyph", static_cast<int>(conversation_glyph::edit));
			edit->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			edit->setFocusPolicy(Qt::StrongFocus);
			connect(edit, &QToolButton::clicked, this, [this] { if (Action) { Action(card_action::edit_draft); } });
			auto* const remove = tool(Foot, QStringLiteral("threadDeleteDraft"), QStringLiteral("Delete draft"), conversation_glyph::trash, palette);
			connect(remove, &QToolButton::clicked, this, [this] { if (Action) { Action(card_action::delete_draft); } });
			// [[BPR-1345]]. A pending draft goes with the review; it is opened
			// to change it, and nothing here publishes it on its own.
			if (aCard.Pending) {
				publish->hide();
			} else {
				actions->addWidget(publish);
			}
			actions->addWidget(edit);
			actions->addStretch(1);
			actions->addWidget(remove);
			footRow->addLayout(actions);
		} else if (aCard.Kind == QStringLiteral("Inline thread")) {
			footRow->setContentsMargins(gut_left, 8, text_right, 10);
			if (notice != nullptr) {
				notice->setContentsMargins(text_left - gut_left, 0, 0, 0);
				footRow->addWidget(notice);
			}
			// [[BPR-1374]]. My unsent reply in this thread is opened by Reply.
			ReplyField = new note_button{ note_button::kind::reply_field, aCard.UnsentReply.has_value() ? QStringLiteral("Continue your reply") : QStringLiteral("Reply"), Foot };
			ReplyField->setObjectName(QStringLiteral("threadReplyField"));
			ReplyField->setToolTip(QStringLiteral("Reply  R"));
			ReplyField->setAccessibleName(QStringLiteral("Reply"));
			ReplyField->set_style(Style);
			connect(ReplyField, &QAbstractButton::clicked, this, [this] { if (Action) { Action(card_action::reply); } });
			footRow->addWidget(ReplyField);
			Editor             = new reply_frame{ Style.Colours.Signal, Style.Colours.CodeBackground.isValid() ? Style.Colours.CodeBackground : Style.Colours.Panel, Foot };
			auto* const editor = new QVBoxLayout{ Editor };
			editor->setContentsMargins(12, 11, 12, 11);
			editor->setSpacing(7);
			ReplyBox = new reply_text{ Editor };
			ReplyBox->setAccessibleName(aCard.Line ? QStringLiteral("Reply to the thread on line %1").arg(*aCard.Line) : QStringLiteral("Reply"));
			editor->addWidget(ReplyBox);
			auto* const controls = new QHBoxLayout{ };
			controls->setSpacing(6);
			// The composer's own label controls ([[BPR-1083]]): the chip and
			// its menu, ⌘L, and with the helper on the `:` completion and the
			// `:xx:` replacement. The drawer is too narrow for the decoration
			// toggles; they stay in the menu, and the chip's mark shows them.
			Labels = new feedback_label_controls{ ReplyBox, Editor };
			Labels->setObjectName(QStringLiteral("threadReplyLabels"));
			Labels->set_decoration_toggles(false);
			Labels->set_theme(Style.Marks);
			style_reply_chip(*Labels->chip(), Style);
			controls->addWidget(Labels);
			// [[BPR-1084]]. A small mark beside the chip, shown only for a
			// suggested change; its tooltip says what GitHub will do with it.
			Placement  = aCard.Suggestion;
			Suggestion = new suggestion_mark{ Editor };
			Suggestion->setObjectName(QStringLiteral("threadReplySuggestionMark"));
			controls->addWidget(Suggestion);
			controls->addStretch(1);
			auto* const cancel = new QToolButton{ Editor };
			cancel->setObjectName(QStringLiteral("threadReplyCancel"));
			cancel->setAutoRaise(true);
			cancel->setText(QStringLiteral("Cancel"));
			cancel->setFocusPolicy(Qt::StrongFocus);
			connect(cancel, &QToolButton::clicked, this, [this] { if (Action) { Action(card_action::cancel_reply); } });
			controls->addWidget(cancel);
			Publish = new note_button{ note_button::kind::primary, QStringLiteral("Publish"), Editor };
			Publish->setObjectName(QStringLiteral("threadReplyPublish"));
			Publish->setIcon(conversation_icon(conversation_glyph::send, palette));
			Publish->setToolTip(QStringLiteral("Publish reply  %1").arg(QKeySequence{ Qt::CTRL | Qt::Key_Return }.toString(QKeySequence::NativeText)));
			Publish->set_style(Style);
			connect(Publish, &QAbstractButton::clicked, this, [this] { if (Action) { Action(card_action::publish_reply); } });
			controls->addWidget(Publish);
			editor->addLayout(controls);
			ReplyStatus = new QLabel{ Editor };
			ReplyStatus->setObjectName(QStringLiteral("threadReplyStatus"));
			ReplyStatus->setWordWrap(true);
			ReplyStatus->setTextFormat(Qt::PlainText);
			ReplyStatus->setFont(scaled(font(), 0.92));
			ReplyStatus->hide();
			editor->addWidget(ReplyStatus);
			Labels->set_status_sink([this](const QString& aText) {
				ReplyStatus->setText(aText);
				ReplyStatus->setAccessibleName(aText);
				ReplyStatus->setVisible(!aText.isEmpty());
				if (HeightChanged) {
					HeightChanged();
				}
			});
			footRow->addWidget(Editor);
			Editor->hide();
			connect(ReplyBox, &QPlainTextEdit::textChanged, this, [this] {
				if (!ReplyStatus->isHidden()) {
					ReplyStatus->hide();
				}
				refresh_reply_controls();
				if (!Applying && ReplyText) {
					ReplyText(ReplyBox->toPlainText());
				}
				if (HeightChanged) {
					HeightChanged();
				}
			});
		}
		if (notice != nullptr && footRow->indexOf(notice) < 0) {
			delete notice;
		}
		column->addWidget(Foot);
		refresh_reply_controls();
	}

	void open_thread_card::build_composer(const thread_card& aCard, QVBoxLayout& aFoot)
	{
		// [[BPR-1212]]. The new comment, written in place at its line: the
		// field, the label chip and its codes, what a suggestion changes, and
		// a foot that says who sees it, with Discard and Publish.
		const auto& palette = this->palette();
		aFoot.setContentsMargins(10, 8, text_right, 10);
		aFoot.setSpacing(7);
		Editor = new reply_frame{ Style.Colours.Signal, Style.Colours.CodeBackground.isValid() ? Style.Colours.CodeBackground : Style.Colours.Panel, Foot };
		Editor->setObjectName(QStringLiteral("threadDraftEditor"));
		auto* const editor = new QVBoxLayout{ Editor };
		editor->setContentsMargins(12, 11, 12, 11);
		editor->setSpacing(7);
		ReplyBox = new reply_text{ Editor };
		ReplyBox->setObjectName(QStringLiteral("threadDraftText"));
		const auto line = aCard.Line.value_or(0U);
		ReplyBox->setAccessibleName(QStringLiteral("Comment on line %1").arg(line));
		ReplyBox->setPlaceholderText(QStringLiteral("Comment on line %1. Type : at the start for a label.").arg(line));
		editor->addWidget(ReplyBox);
		auto* const controls = new QHBoxLayout{ };
		controls->setSpacing(6);
		Labels = new feedback_label_controls{ ReplyBox, Editor };
		Labels->setObjectName(QStringLiteral("threadDraftLabels"));
		Labels->set_decoration_toggles(false);
		Labels->set_theme(Style.Marks);
		style_reply_chip(*Labels->chip(), Style);
		controls->addWidget(Labels);
		Placement  = aCard.Suggestion;
		Suggestion = new suggestion_mark{ Editor };
		Suggestion->setObjectName(QStringLiteral("threadDraftSuggestionMark"));
		controls->addWidget(Suggestion);
		controls->addStretch(1);
		editor->addLayout(controls);
		ReplyStatus = new QLabel{ Editor };
		ReplyStatus->setObjectName(QStringLiteral("threadDraftStatus"));
		ReplyStatus->setWordWrap(true);
		ReplyStatus->setTextFormat(Qt::PlainText);
		ReplyStatus->setFont(scaled(font(), 0.92));
		ReplyStatus->hide();
		editor->addWidget(ReplyStatus);
		Labels->set_status_sink([this](const QString& aText) {
			ReplyStatus->setText(aText);
			ReplyStatus->setAccessibleName(aText);
			ReplyStatus->setVisible(!aText.isEmpty());
			if (HeightChanged) {
				HeightChanged();
			}
		});
		aFoot.addWidget(Editor);
		Change = new suggestion_change_view{ Style, Foot };
		Change->hide();
		aFoot.addWidget(Change);
		// [[BPR-1345]] v002. One row (owner, discard-review v001: "I don't
		// like this button layout, stacked buttons is too much space"): who
		// sees it, Delete, then Add single comment beside the main action.
		// Where the two do not fit, the main action keeps the row and its
		// split arrow carries Add single comment.
		DraftActions = new QBoxLayout{ QBoxLayout::LeftToRight };
		DraftActions->setSpacing(6);
		auto* const privacy = new state_label{ QStringLiteral("Private until you publish"), conversation_glyph::lock, Style.Colours.Faint, Foot };
		privacy->setObjectName(QStringLiteral("threadDraftPrivacy"));
		privacy->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		privacy->show_whole_or_glyph(QStringLiteral("Only you see this draft until you publish it."));
		Privacy = privacy;
		DraftActions->addWidget(privacy, 1);
		auto* const discard = tool(Foot, QStringLiteral("threadDeleteDraft"), QStringLiteral("Delete this comment"), conversation_glyph::trash, palette);
		discard->setAccessibleName(QStringLiteral("Delete comment"));
		connect(discard, &QToolButton::clicked, this, [this] { if (Action) { Action(card_action::delete_draft); } });
		DraftActions->addWidget(discard);
		// The owner, 2026-10-08: "I don't know how to publish my draft
		// comment". Add single comment says what it does: this one comment,
		// now, exactly as Publish did; Start a review keeps it pending.
		Publish = new note_button{ note_button::kind::secondary, QStringLiteral("Add single comment"), Foot };
		Publish->setObjectName(QStringLiteral("threadPublishDraft"));
		Publish->setAccessibleName(QStringLiteral("Add single comment"));
		Publish->setAccessibleDescription(QStringLiteral("Posts this one comment to the pull request now, on its own. No review is submitted."));
		Publish->set_style(Style);
		connect(Publish, &QAbstractButton::clicked, this, [this] { if (Action) { Action(card_action::publish_draft); } });
		DraftActions->addWidget(Publish);
		ReviewAction = new note_button{ note_button::kind::primary, QStringLiteral("Start a review"), Foot };
		ReviewAction->setObjectName(QStringLiteral("threadReviewDraft"));
		ReviewAction->set_style(Style);
		connect(ReviewAction, &QAbstractButton::clicked, this, [this] {
			if (Action) {
				Action(Review == draft_review_state::pending ? card_action::close_draft : card_action::review_draft);
			}
		});
		// The arrow sits flush against the pill it belongs to.
		auto* const pair = new QHBoxLayout{ };
		pair->setSpacing(0);
		pair->addWidget(ReviewAction);
		SplitAction = new note_button{ note_button::kind::split, QString{ }, Foot };
		SplitAction->setObjectName(QStringLiteral("threadReviewDraftMore"));
		SplitAction->setAccessibleName(QStringLiteral("More ways to send this comment"));
		SplitAction->setToolTip(QStringLiteral("More ways to send this comment"));
		SplitAction->set_style(Style);
		SplitAction->hide();
		connect(SplitAction, &QAbstractButton::clicked, this, [this] {
			auto* const menu = new QMenu{ this };
			menu->setObjectName(QStringLiteral("threadReviewDraftMenu"));
			menu->setAttribute(Qt::WA_DeleteOnClose);
			auto* const single = menu->addAction(Publish->text());
			single->setObjectName(QStringLiteral("threadReviewDraftSingle"));
			single->setShortcut(QKeySequence{ Qt::CTRL | Qt::SHIFT | Qt::Key_Return });
			single->setShortcutContext(Qt::WidgetShortcut);
			single->setShortcutVisibleInContextMenu(true);
			single->setToolTip(Publish->toolTip());
			single->setEnabled(Publish->isEnabled());
			connect(single, &QAction::triggered, this, [this] { if (Action) { Action(card_action::publish_draft); } });
			menu->popup(SplitAction->mapToGlobal(QPoint{ SplitAction->width() - menu->sizeHint().width(), SplitAction->height() }));
		});
		pair->addWidget(SplitAction);
		DraftActions->addLayout(pair);
		aFoot.addLayout(DraftActions);
		present_review_state();
		connect(ReplyBox, &QPlainTextEdit::textChanged, this, [this] {
			if (!ReplyStatus->isHidden() && LastNotice.isEmpty()) {
				ReplyStatus->hide();
			}
			refresh_reply_controls();
			if (!Applying && DraftText) {
				DraftText(ReplyBox->toPlainText());
			}
			if (HeightChanged) {
				HeightChanged();
			}
		});
	}

	void open_thread_card::present_draft(const std::optional<draft_composer_presentation>& aDraft)
	{
		if (!Writing || ReplyBox == nullptr) {
			return;
		}
		// [[BPR-1329]]. A draft that is publishing is not written, but a retry
		// of that same publication is still offered.
		Publishable = aDraft.has_value() && aDraft->Publishable && (aDraft->Editable || aDraft->Retry);
		Labels->set_helper(aDraft.has_value() && aDraft->LabelHelper);
		ReplyBox->setReadOnly(!aDraft.has_value() || !aDraft->Editable);
		if (aDraft.has_value() && ReplyBox->toPlainText() != aDraft->Body) {
			// The draft editor's text arrives. A suggestion's caret goes to
			// the end of the last line inside its block; any other to the end.
			Applying = true;
			ReplyBox->setPlainText(aDraft->Body);
			auto cursor = ReplyBox->textCursor();
			cursor.movePosition(QTextCursor::End);
			if (aDraft->Change.has_value() && !aDraft->Body.isEmpty()) {
				if (const auto last = aDraft->Body.lastIndexOf(QLatin1Char{ '\n' }); last > 0) {
					cursor.setPosition(static_cast<int>(last));
				}
			}
			ReplyBox->setTextCursor(cursor);
			Applying = false;
		}
		// A suggestion is code: monospaced and unwrapped, as it will apply.
		const auto fenced = aDraft.has_value() && aDraft->Change.has_value();
		ReplyBox->setFont(fenced ? Style.CodeFont : font());
		ReplyBox->setTabStopDistance(QFontMetricsF{ ReplyBox->font() }.horizontalAdvance(QLatin1Char{ ' ' }) * 4.0);
		ReplyBox->setLineWrapMode(fenced ? QPlainTextEdit::NoWrap : QPlainTextEdit::WidgetWidth);
		ReplyBox->setHorizontalScrollBarPolicy(fenced ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
		const auto lines = aDraft.has_value() && !aDraft->Lines.isEmpty() ? aDraft->Lines : ReplyBox->accessibleName().section(QLatin1Char{ ' ' }, 2, -1);
		if (aDraft.has_value() && !aDraft->Lines.isEmpty()) {
			ReplyBox->setAccessibleName(QStringLiteral("%1 on %2").arg(fenced ? QStringLiteral("Suggested change") : QStringLiteral("Comment"), aDraft->Lines.toLower()));
			ReplyBox->setPlaceholderText(fenced ? QStringLiteral("Edit the lines in the block. Say why above it.")
												: QStringLiteral("Comment on %1. Type : at the start for a label.").arg(aDraft->Lines.toLower()));
			if (Where != nullptr) {
				Where->setText(aDraft->Lines);
				Where->setAccessibleName(QStringLiteral("Show %1").arg(aDraft->Lines.toLower()));
			}
		}
		if (DraftState != nullptr) {
			static_cast<state_label*>(DraftState)->set_text(fenced ? QStringLiteral("Suggested change") : QStringLiteral("Draft"));
		}
		static_cast<suggestion_change_view*>(Change)->present(aDraft.has_value() ? aDraft->Change : std::nullopt, lines);
		const auto former = std::exchange(LastNotice, aDraft.has_value() ? aDraft->Notice : QString{ });
		if (!LastNotice.isEmpty()) {
			ReplyStatus->setText(LastNotice);
			ReplyStatus->setAccessibleName(LastNotice);
			ReplyStatus->show();
		} else if (!former.isEmpty() && ReplyStatus->text() == former) {
			// [[BPR-1329]]. A notice that no longer holds goes with it.
			ReplyStatus->hide();
		}
		Review = aDraft.has_value() ? aDraft->Review : draft_review_state::none;
		present_review_state();
		if (Publish != nullptr && aDraft.has_value() && aDraft->Retry) {
			Publish->setText(QStringLiteral("Retry"));
			Publish->setAccessibleName(QStringLiteral("Retry publishing this comment"));
			Publish->updateGeometry();
		}
		if (DraftState != nullptr && Review == draft_review_state::pending) {
			static_cast<state_label*>(DraftState)->set_text(QStringLiteral("Pending"));
		}
		refresh_reply_controls();
		fit_draft_actions();
		if (HeightChanged) {
			HeightChanged();
		}
	}

	auto open_thread_card::writing() const noexcept -> bool
	{
		return Writing;
	}

	auto open_thread_card::primary_action() const noexcept -> card_action
	{
		switch (Review) {
		case draft_review_state::none:
		case draft_review_state::in_progress: return card_action::review_draft;
		case draft_review_state::pending:     return card_action::close_draft;
		}
		return card_action::review_draft;
	}

	void open_thread_card::present_review_state()
	{
		// [[BPR-1345]]. The frames' two ways out. v002: ⌘↩ is the review's
		// action in every state (Start a review, Add review comment, or on a
		// pending draft the close that keeps it pending), ⌘⇧↩ is Add single
		// comment, and each tooltip names its key.
		if (Publish == nullptr || ReviewAction == nullptr) {
			return;
		}
		const auto key    = QKeySequence{ Qt::CTRL | Qt::Key_Return }.toString(QKeySequence::NativeText);
		const auto single = QKeySequence{ Qt::CTRL | Qt::SHIFT | Qt::Key_Return }.toString(QKeySequence::NativeText);
		Publish->setText(QStringLiteral("Add single comment"));
		Publish->setAccessibleName(QStringLiteral("Add single comment"));
		switch (Review) {
		case draft_review_state::none:
			Publish->setToolTip(QStringLiteral("Post this one comment to the pull request now  %1").arg(single));
			ReviewAction->setText(QStringLiteral("Start a review"));
			ReviewAction->setToolTip(QStringLiteral("Keep this comment pending in a review you finish later  %1").arg(key));
			ReviewAction->setAccessibleDescription(QStringLiteral("Keeps this comment pending, unpublished, in a review you finish later from the review bar."));
			break;
		case draft_review_state::in_progress:
			Publish->setToolTip(QStringLiteral("Post this one comment to the pull request now, outside your review  %1").arg(single));
			ReviewAction->setText(QStringLiteral("Add review comment"));
			ReviewAction->setToolTip(QStringLiteral("Add this comment to your pending review  %1").arg(key));
			ReviewAction->setAccessibleDescription(QStringLiteral("Adds this comment to your review, pending until you finish the review. Nothing is published."));
			break;
		case draft_review_state::pending:
			Publish->setToolTip(QStringLiteral("Take this comment out of your review and post it now on its own  %1").arg(single));
			ReviewAction->setText(QStringLiteral("Update review comment"));
			ReviewAction->setToolTip(QStringLiteral("Keep this comment pending in your review  %1").arg(key));
			ReviewAction->setAccessibleDescription(QStringLiteral("Closes this comment; it stays pending in your review with what you wrote."));
			break;
		}
		Publish->setAccessibleDescription(Review == draft_review_state::pending
											  ? QStringLiteral("Takes this comment out of your review and posts it to the pull request now, on its own.")
											  : QStringLiteral("Posts this one comment to the pull request now, on its own. No review is submitted."));
		ReviewAction->setAccessibleName(ReviewAction->text());
		if (auto* const privacy = static_cast<state_label*>(Privacy); privacy != nullptr) {
			const auto reviewing = Review != draft_review_state::none;
			privacy->set_text(reviewing ? QStringLiteral("Pending until you finish the review") : QStringLiteral("Private until you publish"));
			privacy->show_whole_or_glyph(reviewing ? QStringLiteral("Only you see your review's comments until you finish the review.")
												   : QStringLiteral("Only you see this draft until you publish it."));
		}
		Publish->updateGeometry();
		ReviewAction->updateGeometry();
	}

	void open_thread_card::fit_draft_actions()
	{
		if (DraftActions == nullptr || Publish == nullptr || ReviewAction == nullptr || SplitAction == nullptr || Foot == nullptr) {
			return;
		}
		// Room for the lock, Delete and both pills side by side, or the
		// main pill and its arrow; never a second row.
		const auto margins   = Foot->layout() != nullptr ? Foot->layout()->contentsMargins() : QMargins{ };
		const auto available = Foot->width() - margins.left() - margins.right();
		const auto lock      = Privacy != nullptr ? Privacy->minimumSizeHint().width() : 0;
		auto*      trash     = Foot->findChild<QToolButton*>(QStringLiteral("threadDeleteDraft"));
		const auto fixed     = lock + (trash != nullptr ? trash->sizeHint().width() : 0) + 3 * DraftActions->spacing() + 2;
		const auto needed    = fixed + Publish->sizeHint().width() + ReviewAction->sizeHint().width();
		const auto split     = available > 0 && needed > available;
		if (split == !SplitAction->isHidden()) {
			return;
		}
		Publish->setVisible(!split);
		SplitAction->setVisible(split);
		ReviewAction->set_joined(split);
		if (HeightChanged) {
			HeightChanged();
		}
	}

	void open_thread_card::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		fit_draft_actions();
	}

	auto open_thread_card::change_view() const noexcept -> QWidget*
	{
		return Change;
	}

	auto open_thread_card::id() const noexcept -> const std::string&
	{
		return Id;
	}

	auto open_thread_card::hasHeightForWidth() const -> bool
	{
		return true;
	}

	auto open_thread_card::heightForWidth(const int aWidth) const -> int
	{
		if (layout() == nullptr) {
			return 0;
		}
		// [[BPR-1212]]. A draft's composer holds no post, and a layout with no
		// height-for-width child answers -1; its size hint is its height.
		return layout()->hasHeightForWidth() ? layout()->totalHeightForWidth(aWidth) : layout()->totalSizeHint().height();
	}

	void open_thread_card::set_replying(const bool aReplying)
	{
		if (Draft || ReplyField == nullptr || Replying == aReplying) {
			return;
		}
		Replying = aReplying;
		ReplyField->setVisible(!aReplying);
		Editor->setVisible(aReplying);
		refresh_reply_controls();
		if (HeightChanged) {
			HeightChanged();
		}
	}

	auto open_thread_card::replying() const noexcept -> bool
	{
		return Replying;
	}

	void open_thread_card::present_reply(const std::optional<thread_reply_presentation>& aReply)
	{
		if (Draft || ReplyBox == nullptr) {
			return;
		}
		const auto ours = aReply.has_value() && aReply->Thread == Id;
		Publishable     = ours && aReply->Publishable;
		// The helper is the draft editor's switch; a reply that is not yet
		// its draft has none.
		Labels->set_helper(ours && aReply->LabelHelper);
		if (ours && ReplyBox->toPlainText() != aReply->Body) {
			// The draft editor's text arrives; a caret at the end keeps typing
			// where it was.
			Applying = true;
			ReplyBox->setPlainText(aReply->Body);
			auto cursor = ReplyBox->textCursor();
			cursor.movePosition(QTextCursor::End);
			ReplyBox->setTextCursor(cursor);
			Applying = false;
		}
		refresh_reply_controls();
	}

	void open_thread_card::refresh_reply_controls()
	{
		if (Publish != nullptr && ReplyBox != nullptr) {
			Publish->setEnabled(Publishable && !ReplyBox->toPlainText().trimmed().isEmpty());
		}
		// [[BPR-1345]]. A review takes a comment with words; closing a
		// pending one is always possible.
		if (ReviewAction != nullptr && ReplyBox != nullptr) {
			ReviewAction->setEnabled(Review == draft_review_state::pending || (!ReplyBox->isReadOnly() && !ReplyBox->toPlainText().trimmed().isEmpty()));
		}
		if (Suggestion != nullptr && ReplyBox != nullptr) {
			Suggestion->present(core::decide_suggestion_applicability(ReplyBox->toPlainText().toStdString(), Placement));
		}
	}

	auto open_thread_card::reply_editor() const noexcept -> QPlainTextEdit*
	{
		return Replying || Writing ? ReplyBox : nullptr;
	}

	auto open_thread_card::label_controls() const noexcept -> feedback_label_controls*
	{
		return Labels;
	}

	auto open_thread_card::suggestion_mark_widget() const noexcept -> suggestion_mark*
	{
		return Suggestion;
	}

	auto open_thread_card::reply_field() const noexcept -> QAbstractButton*
	{
		return ReplyField;
	}

	auto open_thread_card::posts() const -> std::vector<thread_post_view*>
	{
		return Posts;
	}

	auto open_thread_card::is_reading(const QObject* const aWatched) const -> bool
	{
		return aWatched != nullptr && std::ranges::any_of(Posts, [aWatched](const thread_post_view* aPost) {
				   return aWatched == aPost->reader()->rendered_view() || aWatched == aPost->reader()->source_view();
			   });
	}

	auto open_thread_card::reading_entry() const -> QWidget*
	{
		return Posts.empty() ? nullptr : Posts.front()->reader();
	}

	void open_thread_card::set_notch(const int aY)
	{
		Notch = aY;
	}

	auto open_thread_card::notch() const noexcept -> int
	{
		return Notch;
	}

	void open_thread_card::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto        painter = QPainter{ this };
		const auto& colours = Style.Colours;
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto box = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		painter.setBrush(colours.Panel);
		// [[BPR-1212]]. A draft being written is ringed and edged in the
		// writing blue: amber is where you read, blue is where you write.
		auto ring = colours.Signal;
		ring.setAlphaF(0.55F);
		painter.setPen(Writing ? QPen{ ring, 1.5 } : Draft ? QPen{ colours.Rule2, 1.5 }
														   : QPen{ colours.RouteLine, 1.0 });
		painter.drawRoundedRect(box, 8.0, 8.0);
		// The amber edge down the left: the thread being read.
		auto edge = QPainterPath{ };
		edge.addRoundedRect(QRectF{ -1.0, 6.0, 4.0, static_cast<qreal>(std::max(0, height() - 12)) }, 1.5, 1.5);
		painter.setPen(Qt::NoPen);
		painter.setBrush(Writing ? colours.Signal : colours.Route);
		painter.drawPath(edge);
	}

	void open_thread_card::mousePressEvent(QMouseEvent* const aEvent)
	{
		aEvent->accept();
	}

	void open_thread_card::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		// The icons are drawn in the palette's ink, so a theme change redraws them.
		if (aEvent->type() == QEvent::PaletteChange) {
			for (auto* const button : findChildren<QToolButton*>()) {
				if (const auto glyph = button->property("conversationGlyph"); glyph.isValid()) {
					button->setIcon(conversation_icon(static_cast<conversation_glyph>(glyph.toInt()), palette()));
				}
			}
		}
	}
} // namespace bpr::qt
