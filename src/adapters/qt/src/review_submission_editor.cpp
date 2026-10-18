#include "bpr/qt/review_submission_editor.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/thread_pick_gestures.hpp"
#include "conversation_glyphs.hpp"

#include <QAbstractButton>
#include <QAbstractTextDocumentLayout>
#include <QAccessible>
#include <QAction>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPolygonF>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		constexpr auto decision_order = std::array{ core::review_disposition::comment, core::review_disposition::approve, core::review_disposition::request_changes };

		[[nodiscard]] auto decision_index(const core::review_disposition aDisposition) noexcept -> std::size_t
		{
			switch (aDisposition) {
			case core::review_disposition::comment:         return 0U;
			case core::review_disposition::approve:         return 1U;
			case core::review_disposition::request_changes: return 2U;
			}
			return 0U;
		}

		[[nodiscard]] auto text_of(const std::string& aText) -> QString
		{
			return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
		}

		/// [[BPR-1085]]. A 16 px icon drawn in one ink, on the marks' own
		/// geometry (BPR-1083): the round "unknown" state (states are round,
		/// labels square), the dashed "not sent" square my drafts carry beside
		/// the code (BPR-1290) with a pencil stroke, or the attention dot.
		enum class preflight_icon : std::uint8_t {
			unknown,
			draft,
			attention,
		};
		[[nodiscard]] auto preflight_pixmap(const preflight_icon aIcon, const QPalette& aPalette, const qreal aRatio, const QColor& aAttention = { }) -> QPixmap
		{
			const auto ratio  = std::max<qreal>(1.0, aRatio);
			const auto pixels = static_cast<int>(std::ceil(feedback_mark_size * ratio));
			auto       image  = QImage{ pixels, pixels, QImage::Format_ARGB32_Premultiplied };
			image.fill(Qt::transparent);
			image.setDevicePixelRatio(ratio);
			{
				auto painter = QPainter{ &image };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto ink   = aPalette.color(QPalette::WindowText);
				const auto inset = feedback_mark_border / 2.0;
				const auto frame = QRectF{ inset, inset, feedback_mark_size - feedback_mark_border, feedback_mark_size - feedback_mark_border };
				switch (aIcon) {
				case preflight_icon::unknown: {
					painter.setPen(QPen{ ink, feedback_mark_border });
					painter.setBrush(Qt::NoBrush);
					painter.drawEllipse(frame);
					auto font = painter.font();
					font.setPixelSize(10);
					font.setBold(true);
					painter.setFont(font);
					painter.drawText(QRectF{ 0.0, 0.0, feedback_mark_size, feedback_mark_size }, Qt::AlignCenter, QStringLiteral("?"));
					break;
				}
				case preflight_icon::draft: {
					// The accepted open border: flat dashes, as a non-blocking
					// mark draws them, so the corners stay square-cut.
					auto border = QPen{ ink, feedback_mark_border };
					border.setCapStyle(Qt::FlatCap);
					border.setDashPattern({ feedback_mark_dash / feedback_mark_border, feedback_mark_dash_gap / feedback_mark_border });
					painter.setPen(border);
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(frame, feedback_mark_corner - inset, feedback_mark_corner - inset);
					// A pencil: its shaft, short of the border, and its tip.
					painter.setPen(QPen{ ink, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
					painter.drawLine(QPointF{ 6.5, 9.5 }, QPointF{ 10.5, 5.5 });
					painter.setPen(Qt::NoPen);
					painter.setBrush(ink);
					painter.drawPolygon(QPolygonF{ { QPointF{ 4.6, 11.4 }, QPointF{ 5.4, 8.9 }, QPointF{ 7.1, 10.6 } } });
					break;
				}
				case preflight_icon::attention: {
					painter.setPen(Qt::NoPen);
					painter.setBrush(aAttention);
					painter.drawEllipse(QRectF{ 4.0, 4.0, 8.0, 8.0 });
					break;
				}
				}
			}
			return QPixmap::fromImage(image);
		}

		/// [[BPR-1085]]. One chip, several marks each with its count (owner,
		/// preflight B). Drawn rather than composed from labels so it stays
		/// one keyboard stop with one accessible name.
		class preflight_chip final : public QToolButton
		{
		public:
			using QToolButton::QToolButton;

			void set_segments(std::vector<std::pair<QPixmap, QString>> aSegments)
			{
				Segments = std::move(aSegments);
				updateGeometry();
				update();
			}
			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto metrics = fontMetrics();
				auto       width   = padding * 2 + arrow;
				for (const auto& [mark, count] : Segments) {
					width += static_cast<int>(feedback_mark_size) + gap + metrics.horizontalAdvance(count) + spacing;
				}
				return { width, std::max(static_cast<int>(feedback_mark_size) + 8, metrics.height() + 8) };
			}
			[[nodiscard]] auto minimumSizeHint() const -> QSize override { return sizeHint(); }

		protected:
			void paintEvent(QPaintEvent*) override
			{
				auto painter = QStylePainter{ this };
				auto option  = QStyleOptionToolButton{ };
				initStyleOption(&option);
				option.text.clear();
				option.icon = QIcon{ };
				painter.drawComplexControl(QStyle::CC_ToolButton, option);
				const auto metrics = fontMetrics();
				auto       x       = padding;
				const auto middle  = height() / 2;
				painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText));
				for (const auto& [mark, count] : Segments) {
					const auto side = static_cast<int>(feedback_mark_size);
					painter.drawPixmap(QRect{ x, middle - side / 2, side, side }, mark);
					x += side + gap;
					const auto advance = metrics.horizontalAdvance(count);
					painter.drawText(QRect{ x, 0, advance, height() }, Qt::AlignVCenter | Qt::AlignLeft, count);
					x += advance + spacing;
				}
			}

		private:
			static constexpr int                     padding = 6;
			static constexpr int                     gap     = 3;
			static constexpr int                     spacing = 8;
			static constexpr int                     arrow   = 10;
			std::vector<std::pair<QPixmap, QString>> Segments;
		};

		[[nodiscard]] auto collection_words(const core::feedback_collection aCollection) -> QString
		{
			switch (aCollection) {
			case core::feedback_collection::complete:    return { };
			case core::feedback_collection::partial:     return review_submission_editor::tr("Not every comment was read");
			case core::feedback_collection::stale:       return review_submission_editor::tr("Comments are from an earlier read");
			case core::feedback_collection::unavailable: return review_submission_editor::tr("Comments could not be read");
			}
			return { };
		}

		[[nodiscard]] auto uncertainty_lines(const core::review_preflight& aPreflight) -> QStringList
		{
			const auto& doubt = aPreflight.Uncertain;
			auto        lines = QStringList{ };
			if (const auto words = collection_words(doubt.Collection); !words.isEmpty()) {
				lines.push_back(words);
			}
			if (doubt.ResolutionUnknown > 0U) {
				lines.push_back(review_submission_editor::tr("%n of yours: GitHub didn't say if resolved", nullptr, static_cast<int>(doubt.ResolutionUnknown)));
			}
			if (doubt.Unclassified > 0U) {
				lines.push_back(review_submission_editor::tr("%n of yours: label not readable", nullptr, static_cast<int>(doubt.Unclassified)));
			}
			if (doubt.UnknownAuthor > 0U) {
				lines.push_back(review_submission_editor::tr("%n blocking or unreadable, author unknown", nullptr, static_cast<int>(doubt.UnknownAuthor)));
			}
			return lines;
		}

		[[nodiscard]] auto guidance_line(const core::review_preflight_guidance& aGuidance) -> QString
		{
			return review_submission_editor::tr("%n %1, not marked blocking", nullptr, static_cast<int>(aGuidance.Published + aGuidance.Pending)).arg(feedback_label_text(aGuidance.Label));
		}

		[[nodiscard]] auto decisions_name(const core::review_disposition aDisposition) -> QString
		{
			switch (aDisposition) {
			case core::review_disposition::comment:         return review_submission_editor::tr("Comment");
			case core::review_disposition::approve:         return review_submission_editor::tr("Approve");
			case core::review_disposition::request_changes: return review_submission_editor::tr("Request changes");
			}
			return { };
		}

		[[nodiscard]] auto blend(const QColor& aFrom, const QColor& aTo, const qreal aAmount) -> QColor
		{
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() + (aTo.redF() - aFrom.redF()) * aAmount),
									static_cast<float>(aFrom.greenF() + (aTo.greenF() - aFrom.greenF()) * aAmount),
									static_cast<float>(aFrom.blueF() + (aTo.blueF() - aFrom.blueF()) * aAmount));
		}

		[[nodiscard]] auto luminance(const QColor& aColour) -> double
		{
			const auto channel = [](const double aValue) { return aValue <= 0.03928 ? aValue / 12.92 : std::pow((aValue + 0.055) / 1.055, 2.4); };
			return 0.2126 * channel(aColour.redF()) + 0.7152 * channel(aColour.greenF()) + 0.0722 * channel(aColour.blueF());
		}

		/// [[BPR-1345]] v002. The highlight as ink for words on the text
		/// ground: as it is where it reads (4.5:1), else moved toward the
		/// text's own ink just until it does, so it keeps its hue where it can.
		/// A high-contrast theme's highlight is a fill, not a colour for words.
		/// [[BPR-1590]] v002. The same for any hue: the amber of the lines.
		[[nodiscard]] auto accent_ink(const QPalette& aPalette, const std::optional<QColor>& aHue = std::nullopt) -> QColor
		{
			const auto ground    = aPalette.color(QPalette::Active, QPalette::Base);
			const auto highlight = aHue.value_or(aPalette.color(QPalette::Active, QPalette::Highlight));
			const auto text      = aPalette.color(QPalette::Active, QPalette::Text);
			const auto reads     = [&ground](const QColor& aInk) {
				const auto light = std::max(luminance(ground), luminance(aInk));
				const auto dark  = std::min(luminance(ground), luminance(aInk));
				return (light + 0.05) / (dark + 0.05) >= 4.5;
			};
			for (auto step = 0; step <= 10; ++step) {
				if (const auto ink = blend(highlight, text, step / 10.0); reads(ink)) {
					return ink;
				}
			}
			return text;
		}

		/// [[BPR-1345]] v002. One of the three verdicts that finish a review:
		/// an outlined pill in the drawer's own shape (6 px corners, the
		/// palette's rule for its edge), the highlight's tint under the
		/// pointer and its ring on focus. No verdict is coloured: on this
		/// page colour is not a judgement, and none is nudged.
		class verdict_button final : public QToolButton
		{
		public:
			explicit verdict_button(QWidget* aParent)
				: QToolButton{ aParent }
			{
				setAttribute(Qt::WA_Hover, true);
				setCursor(Qt::PointingHandCursor);
				setFocusPolicy(Qt::StrongFocus);
				setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
				setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto metrics = QFontMetrics{ font() };
				return { metrics.horizontalAdvance(text()) + 32 + (icon().isNull() ? 0 : 20), std::max(30, metrics.height() + 14) };
			}

			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				return sizeHint();
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto        painter = QPainter{ this };
				const auto& colours = palette();
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box       = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
				const auto ink       = colours.color(QPalette::Active, QPalette::ButtonText);
				const auto ground    = colours.color(QPalette::Active, QPalette::Base);
				const auto highlight = colours.color(QPalette::Active, QPalette::Highlight);
				const auto hovered   = isEnabled() && (underMouse() || isDown());
				painter.setOpacity(isEnabled() ? 1.0 : 0.45);
				painter.setPen(QPen{ hovered ? highlight : blend(ground, ink, 0.28), 1.0 });
				painter.setBrush(hovered ? blend(ground, highlight, isDown() ? 0.22 : 0.12) : ground);
				painter.drawRoundedRect(box, 6.0, 6.0);
				const auto metrics = QFontMetrics{ font() };
				const auto glyph   = icon().isNull() ? 0 : 20;
				auto       x       = (width() - metrics.horizontalAdvance(text()) - glyph) / 2;
				if (glyph > 0) {
					icon().paint(&painter, QRect{ x, (height() - 16) / 2, 16, 16 });
					x += glyph;
				}
				painter.setPen(ink);
				painter.drawText(QRect{ x, 0, width() - x, height() }, Qt::AlignLeft | Qt::AlignVCenter, text());
				painter.setOpacity(1.0);
				if (hasFocus()) {
					painter.setPen(QPen{ highlight, 2.0 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(box, 6.0, 6.0);
				}
			}
		};

		/// [[BPR-1541]] v002. The panel a review awaiting GitHub's confirmation
		/// sits in, under the page's title: the text ground warmed a little
		/// toward the theme's attention colour, the page's hairline edge, and
		/// the rail Home's sections carry on its leading side, the one accent. A caution, not an
		/// alarm: nothing in it is red.
		/// How far the waiting panel's ground leans toward the attention colour.
		constexpr auto waiting_tint = 0.05;
		class waiting_frame final : public QWidget
		{
		public:
			explicit waiting_frame(QWidget* aParent)
				: QWidget{ aParent }
			{ }

			void set_attention(const QColor& aColour)
			{
				Attention = aColour;
				update();
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto        painter = QPainter{ this };
				const auto& colours = palette();
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto ground = colours.color(QPalette::Active, QPalette::Base);
				const auto box    = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
				// One accent only: the rail. The edge is the page's own hairline.
				painter.setPen(QPen{ blend(ground, colours.color(QPalette::Active, QPalette::Text), 0.16), 1.0 });
				painter.setBrush(blend(ground, Attention, waiting_tint));
				painter.drawRoundedRect(box, 6.0, 6.0);
				auto edge = QPainterPath{ };
				edge.addRoundedRect(box, 6.0, 6.0);
				painter.setClipPath(edge);
				painter.fillRect(QRectF{ box.left(), box.top(), 3.0, box.height() }, Attention);
			}

		private:
			QColor Attention{ 0xB0, 0x7A, 0x12 };
		};

		/// [[BPR-1541]] v002. Check status, the panel's one action: the
		/// highlight's tint and edge, as a verdict is drawn under the pointer.
		class panel_action_button final : public QToolButton
		{
		public:
			explicit panel_action_button(QWidget* aParent)
				: QToolButton{ aParent }
			{
				setAttribute(Qt::WA_Hover, true);
				setCursor(Qt::PointingHandCursor);
				setFocusPolicy(Qt::StrongFocus);
				setToolButtonStyle(Qt::ToolButtonTextOnly);
				setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto metrics = QFontMetrics{ font() };
				return { metrics.horizontalAdvance(text()) + 28, std::max(30, metrics.height() + 14) };
			}

			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				return sizeHint();
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto        painter = QPainter{ this };
				const auto& colours = palette();
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box       = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
				const auto ink       = colours.color(QPalette::Active, QPalette::ButtonText);
				const auto ground    = colours.color(QPalette::Active, QPalette::Base);
				const auto highlight = colours.color(QPalette::Active, QPalette::Highlight);
				const auto hovered   = isEnabled() && underMouse();
				painter.setOpacity(isEnabled() ? 1.0 : 0.5);
				painter.setPen(QPen{ highlight, 1.0 });
				painter.setBrush(blend(ground, highlight, isDown() ? 0.34 : hovered ? 0.26
																					: 0.16));
				painter.drawRoundedRect(box, 6.0, 6.0);
				auto medium = font();
				medium.setWeight(QFont::Medium);
				painter.setFont(medium);
				painter.setPen(ink);
				painter.drawText(rect(), Qt::AlignCenter, text());
				painter.setOpacity(1.0);
				if (hasFocus()) {
					painter.setPen(QPen{ highlight, 2.0 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(box, 6.0, 6.0);
				}
			}
		};

		/// [[BPR-1541]] v002. The page's "unknown" mark, a ring with a
		/// question mark, as Compose review's preflight draws it, in the
		/// attention colour.
		[[nodiscard]] auto waiting_mark(const QColor& aInk, const qreal aRatio) -> QPixmap
		{
			constexpr auto size   = 18.0;
			const auto     ratio  = std::max<qreal>(1.0, aRatio);
			const auto     pixels = static_cast<int>(std::ceil(size * ratio));
			auto           image  = QImage{ pixels, pixels, QImage::Format_ARGB32_Premultiplied };
			image.fill(Qt::transparent);
			image.setDevicePixelRatio(ratio);
			{
				auto painter = QPainter{ &image };
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(QPen{ aInk, 1.6 });
				painter.setBrush(Qt::NoBrush);
				painter.drawEllipse(QRectF{ 1.3, 1.3, size - 2.6, size - 2.6 });
				auto font = painter.font();
				font.setPixelSize(11);
				font.setBold(true);
				painter.setFont(font);
				painter.drawText(QRectF{ 0.0, 0.0, size, size }, Qt::AlignCenter, QStringLiteral("?"));
			}
			return QPixmap::fromImage(image);
		}

		/// [[BPR-1345]] v002. The box the pending comments sit in: the text
		/// ground with a hairline edge, the one boxed thing on the page
		/// besides the summary.
		class pending_box final : public QWidget
		{
		public:
			explicit pending_box(QWidget* aParent)
				: QWidget{ aParent }
			{ }

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto        painter = QPainter{ this };
				const auto& colours = palette();
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto ground = colours.color(QPalette::Active, QPalette::Base);
				painter.setPen(QPen{ blend(ground, colours.color(QPalette::Active, QPalette::Text), 0.16), 1.0 });
				painter.setBrush(ground);
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
			}
		};

		/// [[BPR-1590]] v003. The field the summary is written in: the text
		/// ground with a clear edge, and the window's focus colour round it while
		/// the cursor is in it, so it reads as a field and not as a row.
		class summary_field final : public QWidget
		{
		public:
			explicit summary_field(QWidget* aParent)
				: QWidget{ aParent }
			{
				setObjectName(QStringLiteral("wholeReviewSummaryBox"));
			}

			void watch(QWidget* aEditor)
			{
				Editor = aEditor;
				aEditor->installEventFilter(this);
			}

		protected:
			auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override
			{
				if (aWatched == Editor && (aEvent->type() == QEvent::FocusIn || aEvent->type() == QEvent::FocusOut)) {
					update();
				}
				return QWidget::eventFilter(aWatched, aEvent);
			}

			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto        painter = QPainter{ this };
				const auto& colours = palette();
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto ground  = colours.color(QPalette::Active, QPalette::Base);
				const auto focused = Editor != nullptr && Editor->hasFocus();
				const auto edge    = focused ? colours.color(QPalette::Active, QPalette::Highlight) : blend(ground, colours.color(QPalette::Active, QPalette::Text), 0.34);
				painter.setPen(QPen{ edge, focused ? 2.0 : 1.0 });
				painter.setBrush(ground);
				const auto inset = focused ? 1.0 : 0.5;
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(inset, inset, -inset, -inset), 6.0, 6.0);
			}

		private:
			QPointer<QWidget> Editor;
		};

		/// [[BPR-1590]] v002. A hairline between the box's parts.
		class box_rule final : public QWidget
		{
		public:
			explicit box_rule(QWidget* aParent)
				: QWidget{ aParent }
			{
				setFixedHeight(1);
				setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto        painter = QPainter{ this };
				const auto& colours = palette();
				painter.fillRect(rect(), blend(colours.color(QPalette::Active, QPalette::Base), colours.color(QPalette::Active, QPalette::Text), 0.12));
			}
		};

		/// [[BPR-1590]] v003. One pending comment on one line, as the chosen
		/// layouts draw it (the owner: "the comments in the mockup are
		/// contained to one line"): the label's mark, the file and its lines in
		/// amber, then the words, cut to the line however long they are; the
		/// whole comment is in the tooltip. Show and its chevron at the end say
		/// the row goes to the comment. It takes Threads' gestures ([[BPR-1582]],
		/// shared in `thread_pick_gestures`): a click selects it, a double click
		/// or Return opens it in Review, with Shift in Discuss, and its menu adds
		/// Show in Threads.
		class pending_row final : public QAbstractButton
		{
		public:
			pending_row(QString aFile, QString aLines, QString aBody, const feedback_mark_theme& aTheme, QWidget* aParent)
				: QAbstractButton{ aParent }
				, File{ std::move(aFile) }
				, Lines{ std::move(aLines) }
				, Theme{ aTheme }
			{
				setObjectName(QStringLiteral("wholeReviewPendingOpen"));
				setAttribute(Qt::WA_Hover, true);
				setCursor(Qt::PointingHandCursor);
				setFocusPolicy(Qt::StrongFocus);
				setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
				Mark = feedback_body_mark_of(aBody.toStdString());
				// One line of words: fenced blocks become "suggests a change",
				// paragraphs run together.
				auto words = QStringList{ };
				auto fence = false;
				for (const auto& line : aBody.split(QLatin1Char{ '\n' })) {
					const auto trimmed = line.trimmed();
					if (trimmed.startsWith(QStringLiteral("```"))) {
						Suggests = Suggests || trimmed.startsWith(QStringLiteral("```suggestion"));
						fence    = !fence;
						continue;
					}
					if (!fence && !trimmed.isEmpty()) {
						words.push_back(trimmed);
					}
				}
				Words = words.join(QLatin1Char{ ' ' });
				if (Words.isEmpty()) {
					Words = Suggests ? review_submission_editor::tr("suggests a change") : review_submission_editor::tr("(no text yet)");
				}
			}

			void set_repeat(const bool aRepeat)
			{
				Repeat = aRepeat;
				update();
			}

			void set_columns(const int aFile, const int aLines, const QFont& aCode)
			{
				FileColumn  = aFile;
				LinesColumn = aLines;
				Code        = aCode;
				Code.setWeight(QFont::DemiBold);
				update();
			}

			[[nodiscard]] auto file_width() const -> int
			{
				auto bold = font();
				bold.setWeight(QFont::DemiBold);
				return QFontMetrics{ bold }.horizontalAdvance(File) + 2;
			}

			[[nodiscard]] static auto lines_width(const QString& aLines, QFont aCode) -> int
			{
				aCode.setWeight(QFont::DemiBold);
				return QFontMetrics{ aCode }.horizontalAdvance(aLines) + 2;
			}

			/// \brief One line, whatever the comment's length.
			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				return { FileColumn + LinesColumn + 240, std::max(32, QFontMetrics{ font() }.height() + 14) };
			}

			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				return { 120, sizeHint().height() };
			}

			/// Where an opening goes.
			std::function<void(core::workbench_thread_destination)> Open;

		protected:
			// A click selects the row and opens nothing.
			void mousePressEvent(QMouseEvent* aEvent) override
			{
				if (aEvent->button() == Qt::LeftButton) {
					setFocus(Qt::MouseFocusReason);
					update();
				}
				aEvent->accept();
			}

			void mouseReleaseEvent(QMouseEvent* aEvent) override
			{
				aEvent->accept();
			}

			void mouseDoubleClickEvent(QMouseEvent* aEvent) override
			{
				if (aEvent->button() == Qt::LeftButton && Open) {
					Open(thread_pick_destination(aEvent->modifiers()));
				}
				aEvent->accept();
			}

			void keyPressEvent(QKeyEvent* aEvent) override
			{
				if (const auto destination = thread_open_key(*aEvent); destination.has_value()) {
					if (Open) {
						Open(*destination);
					}
					return;
				}
				QAbstractButton::keyPressEvent(aEvent);
			}

			void contextMenuEvent(QContextMenuEvent* aEvent) override
			{
				setFocus(Qt::MouseFocusReason);
				const auto row = make_thread_row_menu(this, QStringLiteral("wholeReviewPendingMenu"), QStringLiteral("Pending comment"), QStringLiteral("Open this comment beside its code"),
													  QStringLiteral("Open this comment in Discuss, in its composer"), true);
				const auto go  = [this](const core::workbench_thread_destination aDestination) {
					if (Open) {
						Open(aDestination);
					}
				};
				QObject::connect(row.OpenReview, &QAction::triggered, this, [go] { go(core::workbench_thread_destination::review); });
				QObject::connect(row.OpenDiscuss, &QAction::triggered, this, [go] { go(core::workbench_thread_destination::discuss); });
				QObject::connect(row.ShowThreads, &QAction::triggered, this, [go] { go(core::workbench_thread_destination::threads); });
				LastMenu = row.Menu;
				row.Menu->popup(aEvent->globalPos());
				aEvent->accept();
			}

			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto        painter = QPainter{ this };
				const auto& colours = palette();
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto ground    = colours.color(QPalette::Active, QPalette::Base);
				const auto ink       = colours.color(QPalette::Active, QPalette::Text);
				const auto faint     = colours.color(QPalette::Active, QPalette::PlaceholderText);
				const auto highlight = colours.color(QPalette::Active, QPalette::Highlight);
				const auto box       = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
				const auto hovered   = underMouse() || isDown() || hasFocus();
				if (hovered) {
					painter.setPen(Qt::NoPen);
					painter.setBrush(blend(ground, highlight, isDown() ? 0.18 : 0.08));
					painter.drawRoundedRect(box, 4.0, 4.0);
				}
				const auto metrics = QFontMetrics{ font() };
				const auto middle  = height() / 2;
				auto       x       = gutter;
				// The label's mark, or the plain one.
				if (Mark.Label) {
					paint_feedback_mark(painter, QRectF{ static_cast<qreal>(x), middle - 8.0, 16.0, 16.0 }, *Mark.Label, Mark.Emphasis, feedback_mark_colours_for(Theme, *Mark.Label, colours));
				} else {
					painter.drawPixmap(QPointF{ static_cast<qreal>(x), middle - 8.0 }, feedback_plain_mark_pixmap(colours, devicePixelRatioF()));
				}
				x += 16 + spacing;
				// The file, then its lines in amber in the code font.
				auto bold = font();
				bold.setWeight(QFont::DemiBold);
				painter.setFont(bold);
				painter.setPen(Repeat ? faint : ink);
				painter.drawText(QRect{ x, 0, FileColumn, height() }, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics{ bold }.elidedText(File, Qt::ElideMiddle, FileColumn));
				x += FileColumn + spacing;
				painter.setFont(Code);
				painter.setPen(accent_ink(colours, drawer_card_colours{ }.Reading));
				painter.drawText(QRect{ x, 0, LinesColumn, height() }, Qt::AlignLeft | Qt::AlignVCenter, Lines);
				x += LinesColumn + spacing;
				// The chevron at the end, stronger while the row is under the
				// pointer or the keyboard; no words (owner, compose-sheet v003).
				painter.setFont(font());
				{
					auto       chevron = QPainterPath{ };
					const auto cx      = static_cast<qreal>(width() - gutter - 4);
					chevron.moveTo(cx - 3.0, middle - 4.0);
					chevron.lineTo(cx + 1.0, middle);
					chevron.lineTo(cx - 3.0, middle + 4.0);
					painter.setBrush(Qt::NoBrush);
					painter.setPen(QPen{ hovered ? accent_ink(colours) : faint, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
					painter.drawPath(chevron);
				}
				// The words: the label's header in bold, then the rest, cut to
				// the one line at the chevron.
				const auto room   = std::max(0, width() - gutter - 16 - 8 - x);
				auto       prefix = QString{ };
				auto       rest   = Words;
				if (Mark.Label) {
					const auto colon = Words.indexOf(QLatin1Char{ ':' });
					if (colon > 0) {
						prefix = Words.left(colon + 1);
						rest   = Words.mid(colon + 1);
					}
				}
				rest = rest.trimmed();
				painter.setFont(bold);
				painter.setPen(ink);
				// Whole while it fits: the label is never cut for the words.
				const auto prefixWidth = prefix.isEmpty() ? 0 : std::min(room, QFontMetrics{ bold }.horizontalAdvance(prefix) + 2);
				painter.drawText(QRect{ x, 0, prefixWidth, height() }, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics{ bold }.elidedText(prefix, Qt::ElideRight, prefixWidth));
				painter.setFont(font());
				const auto gap      = prefix.isEmpty() ? 0 : metrics.horizontalAdvance(QLatin1Char{ ' ' });
				const auto restRoom = std::max(0, room - prefixWidth - gap);
				painter.drawText(QRect{ x + prefixWidth + gap, 0, restRoom, height() }, Qt::AlignLeft | Qt::AlignVCenter, metrics.elidedText(rest, Qt::ElideRight, restRoom));
				if (hasFocus()) {
					painter.setPen(QPen{ highlight, 2.0 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(box, 4.0, 4.0);
				}
			}

		private:
			static constexpr int gutter  = 10;
			static constexpr int spacing = 12;
			QString              File;
			QString              Lines;
			QString              Words;
			feedback_body_mark   Mark;
			feedback_mark_theme  Theme;
			QFont                Code;
			int                  FileColumn  = 0;
			int                  LinesColumn = 0;
			bool                 Suggests    = false;
			bool                 Repeat      = false;

		public:
			/// The last menu opened, while it is open (for courses).
			QPointer<QMenu> LastMenu;
		};

	} // namespace

	auto review_preflight_summary(const std::optional<core::review_preflight>& aPreflight) -> QString
	{
		if (!aPreflight) {
			return review_submission_editor::tr("Your feedback on this revision has not been read: unknown, not none.");
		}
		auto lines = QStringList{ review_submission_editor::tr("Yours on GitHub: %n open blocking", nullptr, static_cast<int>(aPreflight->Blockers.size())) };
		if (!aPreflight->Pending.empty()) {
			lines.push_back(review_submission_editor::tr("Yours, not sent: %n blocking", nullptr, static_cast<int>(aPreflight->Pending.size())));
		}
		for (const auto& guidance : aPreflight->Guidance) {
			lines.push_back(guidance_line(guidance));
		}
		lines.append(uncertainty_lines(*aPreflight));
		if (aPreflight->Others > 0U) {
			lines.push_back(review_submission_editor::tr("Others: %n open blocking (not yours)", nullptr, static_cast<int>(aPreflight->Others)));
		}
		return lines.join(QLatin1Char{ '\n' });
	}

	auto review_submission_unknown_text(const core::review_submission_unknown_notice& aNotice) -> QString
	{
		// [[BPR-1541]]. What happened, then what to do, in a reviewer's words.
		// Only the counts the notice carries; never a host, repository,
		// account, path or comment.
		using reason        = core::review_submission_unknown_reason;
		const auto comments = [](const std::uint16_t aCount) {
			return aCount == 1U ? review_submission_editor::tr("1 comment") : review_submission_editor::tr("%1 comments").arg(aCount);
		};
		switch (aNotice.Reason) {
		case reason::review_not_listed:
			return review_submission_editor::tr("GitHub hasn't listed this review yet. Check status again in a moment.");
		case reason::several_reviews_listed:
			return review_submission_editor::tr("GitHub lists more than one matching review from you, so this one can't be picked out. Look at the pull request on GitHub.");
		case reason::review_list_unreadable:
			return review_submission_editor::tr("GitHub's list of reviews couldn't be read. Check status again; if this repeats, look at the pull request on GitHub.");
		case reason::review_list_too_large:
			return review_submission_editor::tr("This pull request's list of reviews is too large to read here. Look at the pull request on GitHub.");
		case reason::review_list_paged:
			return review_submission_editor::tr("This pull request has more than 100 reviews, more than can be read here. Look at the pull request on GitHub.");
		case reason::comment_page_unreadable:
			return review_submission_editor::tr("A page of this pull request's comments couldn't be read. Check status again; if this repeats, look at the pull request on GitHub.");
		case reason::comment_page_too_large:
			return review_submission_editor::tr("A page of this pull request's comments is too large to read here. Look at the pull request on GitHub.");
		case reason::comment_page_link_refused:
			return review_submission_editor::tr("GitHub pointed to a next page of comments that isn't this pull request's, so it wasn't read. Look at the pull request on GitHub.");
		case reason::comment_collection_too_long:
			return review_submission_editor::tr("This pull request has more than 1,000 comments, more than can be read here. Look at the pull request on GitHub.");
		case reason::comments_mismatch:
			if (aNotice.Count == core::review_submission_unknown_count::matched_of_carried) {
				return review_submission_editor::tr("A new review from you was found, but only %1 of %2 matched. Look at it on GitHub before submitting anything.").arg(aNotice.Value).arg(comments(aNotice.Of));
			}
			if (aNotice.Count == core::review_submission_unknown_count::held_of_carried) {
				return review_submission_editor::tr("A new review from you was found, but it holds %1 and this one has %2. Look at it on GitHub before submitting anything.").arg(comments(aNotice.Value), comments(aNotice.Of));
			}
			return review_submission_editor::tr("A new review from you was found, but its comments don't match this one. Look at it on GitHub before submitting anything.");
		case reason::comment_moved_without_original_position:
			return review_submission_editor::tr("A comment moved after a new commit, and GitHub doesn't say where it was written. Look at the review on GitHub.");
		case reason::http_status:
			if (aNotice.StatusClass == 5U) {
				return review_submission_editor::tr("GitHub had a server error. Check status again in a few minutes.");
			}
			if (aNotice.StatusClass == 4U) {
				return review_submission_editor::tr("GitHub refused to show this pull request's reviews. Make sure you can still open it, then check status again.");
			}
			return review_submission_editor::tr("GitHub answered with an error. Check status again.");
		case reason::rate_limited:
			return review_submission_editor::tr("GitHub's rate limit for your account was reached. Check status again later.");
		case reason::timeout:
			return review_submission_editor::tr("GitHub took too long to answer. Check status again.");
		case reason::network:
			return review_submission_editor::tr("GitHub couldn't be reached. Check your connection, then check status again.");
		case reason::account_mismatch:
			return review_submission_editor::tr("The account signed in now isn't the one that submitted this review. Sign in with that account, then check status.");
		case reason::pull_request_mismatch:
			return review_submission_editor::tr("This pull request no longer matches the one the review was sent to. Look at the pull request on GitHub.");
		case reason::connection_unavailable:
			return review_submission_editor::tr("There's no connection to GitHub for this account right now. Sign in again, then check status.");
		case reason::local_record_unavailable:
			return review_submission_editor::tr("This computer's record of the submission couldn't be read. Look at the pull request on GitHub.");
		case reason::cancelled:
			return review_submission_editor::tr("The check stopped before it finished. Check status again.");
		}
		return { };
	}

	review_submission_editor::review_submission_editor(review_submission_editor_dispatch aDispatch, QWidget* aParent)
		: QWidget{ aParent }
		, Dispatch{ std::move(aDispatch) }
	{
		// [[BPR-1345]] v002. Finish your review, made calm (owner,
		// discard-review v001: "Make this page beautiful, and remove some of
		// this duplicate text and reduce the wall of text feel."): the title,
		// the pending comments in one box, the summary, and the three
		// verdicts that submit it; what is published is said once; the host,
		// head and account wait behind a disclosure. One column, no wider
		// than a comfortable line, left aligned on the page.
		setObjectName(QStringLiteral("wholeReviewEditor"));
		setMaximumWidth(760);
		auto* layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(10);
		Title = new QLabel{ tr("Compose review"), this };
		Title->setObjectName(QStringLiteral("wholeReviewTitle"));
		auto titleFont = Title->font();
		titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
		titleFont.setWeight(QFont::DemiBold);
		Title->setFont(titleFont);
		layout->addWidget(Title);
		// [[BPR-1541]] v002. A review awaiting GitHub's confirmation is the
		// page's news, so it is said first, in one calm panel: the mark and a
		// plain title, why, what is safe, and the one thing to do. The draft
		// below is only context while it waits.
		auto* const panel = new waiting_frame{ this };
		Waiting           = panel;
		Waiting->setObjectName(QStringLiteral("wholeReviewWaiting"));
		auto* const panelRow = new QHBoxLayout{ Waiting };
		panelRow->setContentsMargins(15, 12, 14, 13);
		panelRow->setSpacing(10);
		WaitingMark = new QLabel{ Waiting };
		WaitingMark->setObjectName(QStringLiteral("wholeReviewWaitingMark"));
		WaitingMark->setFixedSize(18, 18);
		WaitingMark->setAccessibleName(tr("Status unknown"));
		panelRow->addWidget(WaitingMark, 0, Qt::AlignTop);
		auto* const panelText = new QVBoxLayout;
		panelText->setContentsMargins(0, 0, 0, 0);
		panelText->setSpacing(4);
		WaitingTitle = new QLabel{ Waiting };
		WaitingTitle->setObjectName(QStringLiteral("wholeReviewWaitingTitle"));
		WaitingTitle->setTextFormat(Qt::PlainText);
		WaitingTitle->setWordWrap(true);
		auto waitingFont = WaitingTitle->font();
		waitingFont.setWeight(QFont::DemiBold);
		waitingFont.setPointSizeF(waitingFont.pointSizeF() * 1.08);
		WaitingTitle->setFont(waitingFont);
		WaitingTitle->setMinimumWidth(0);
		WaitingTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		panelText->addWidget(WaitingTitle);
		const auto words = [this](const QString& aName) {
			auto* const label = new QLabel{ Waiting };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			label->setMinimumWidth(0);
			label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
			return label;
		};
		// [[BPR-1541]]. Why the review is still unknown, the panel's body.
		UnknownReason = words(QStringLiteral("wholeReviewUnknownReason"));
		UnknownReason->setAccessibleName(tr("Why the review status is unknown"));
		UnknownReason->setVisible(false);
		panelText->addWidget(UnknownReason);
		WaitingFallback = words(QStringLiteral("wholeReviewWaitingFallback"));
		panelText->addWidget(WaitingFallback);
		WaitingAssure = words(QStringLiteral("wholeReviewWaitingAssurance"));
		WaitingAssure->setText(tr("Nothing will be sent twice. Your review may already be on GitHub."));
		panelText->addWidget(WaitingAssure);
		panelText->addSpacing(6);
		panelRow->addLayout(panelText, 1);
		Waiting->hide();
		layout->addWidget(Waiting);
		// [[BPR-1590]] v002. The pull request this review is for, in a quiet
		// line under the title, as the chosen layouts draw it.
		Context = new QLabel{ this };
		Context->setObjectName(QStringLiteral("wholeReviewContext"));
		Context->setTextFormat(Qt::PlainText);
		Context->setForegroundRole(QPalette::PlaceholderText);
		Context->setWordWrap(true);
		Context->setMinimumWidth(0);
		Context->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		Context->hide();
		layout->insertWidget(layout->indexOf(Title) + 1, Context);
		DraftState = new QLabel{ this };
		DraftState->setObjectName(QStringLiteral("wholeReviewDraftState"));
		DraftState->setWordWrap(true);
		DraftState->setTextFormat(Qt::PlainText);
		layout->addWidget(DraftState);
		// [[BPR-1590]] v002. One box, as the chosen layouts draw it: its
		// heading (how many comments, and my blocking feedback's chip at its
		// end), the pending comments, then the summary beneath them.
		auto* const whole = new pending_box{ this };
		whole->setObjectName(QStringLiteral("wholeReviewBox"));
		auto* const inside = new QVBoxLayout{ whole };
		inside->setContentsMargins(1, 1, 1, 1);
		inside->setSpacing(0);
		Heading = new QWidget{ whole };
		Heading->setObjectName(QStringLiteral("wholeReviewHeading"));
		auto* const heading = new QHBoxLayout{ Heading };
		heading->setContentsMargins(14, 8, 10, 8);
		heading->setSpacing(8);
		Carries = new QLabel{ Heading };
		Carries->setObjectName(QStringLiteral("wholeReviewCarries"));
		Carries->setTextFormat(Qt::PlainText);
		auto headingFont = Carries->font();
		headingFont.setWeight(QFont::DemiBold);
		Carries->setFont(headingFont);
		Carries->hide();
		heading->addWidget(Carries);
		heading->addStretch();
		Preflight = new preflight_chip{ Heading };
		Preflight->setObjectName(QStringLiteral("wholeReviewPreflight"));
		Preflight->setFocusPolicy(Qt::StrongFocus);
		Preflight->setAccessibleName(tr("Your blocking feedback"));
		Preflight->setPopupMode(QToolButton::InstantPopup);
		PreflightMenu = new QMenu{ Preflight };
		PreflightMenu->setObjectName(QStringLiteral("wholeReviewPreflightMenu"));
		PreflightMenu->setToolTipsVisible(true);
		Preflight->setMenu(PreflightMenu);
		heading->addWidget(Preflight);
		inside->addWidget(Heading);
		HeadingRule = new box_rule{ whole };
		inside->addWidget(HeadingRule);
		// [[BPR-1345]]. The comments the review carries, each where it is and
		// what it says; v002: each goes to its comment, and can be deleted.
		Pending = new QWidget{ whole };
		Pending->setObjectName(QStringLiteral("wholeReviewPending"));
		Pending->setAccessibleName(tr("Pending in this review"));
		PendingRows = new QVBoxLayout{ Pending };
		PendingRows->setContentsMargins(4, 2, 4, 2);
		PendingRows->setSpacing(0);
		Pending->hide();
		inside->addWidget(Pending);
		SummaryRule = new box_rule{ whole };
		SummaryRule->hide();
		inside->addWidget(SummaryRule);
		layout->addWidget(whole);
		// [[BPR-1590]] v003. The summary is its own section, never read as the
		// last comment (the owner: "At first I thought I was reading or
		// editing the last comment"): space, a heading, and a bordered field
		// to write in, where the comments above are read-only rows.
		layout->addSpacing(10);
		SummaryHeading = new QLabel{ tr("Review summary"), this };
		SummaryHeading->setObjectName(QStringLiteral("wholeReviewSummaryHeading"));
		SummaryHeading->setTextFormat(Qt::PlainText);
		SummaryHeading->setFont(headingFont);
		layout->addWidget(SummaryHeading);
		auto* const summaryBox = new summary_field{ this };
		summaryBox->setObjectName(QStringLiteral("wholeReviewSummaryBox"));
		auto* const summaryRows = new QVBoxLayout{ summaryBox };
		summaryRows->setContentsMargins(8, 6, 8, 6);
		Body = new QPlainTextEdit{ summaryBox };
		Body->setObjectName(QStringLiteral("wholeReviewSummary"));
		Body->setAccessibleName(tr("Review summary"));
		Body->setPlaceholderText(tr("Write a review summary…"));
		Body->setTabChangesFocus(true);
		Body->setFrameShape(QFrame::NoFrame);
		Body->viewport()->setAutoFillBackground(false);
		// Room for a paragraph or two; the page, not the field, takes what is
		// left, so an empty summary is not a wall.
		Body->setMinimumHeight(120);
		// [[BPR-1590]] v002. The field is as tall as what is written, from
		// 120 pixels up to 320, then it scrolls: no empty wall under a short
		// summary, as the chosen layouts draw it.
		Body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		const auto fit = [this] {
			const auto lines  = static_cast<int>(std::ceil(Body->document()->size().height()));
			const auto height = (lines * Body->fontMetrics().lineSpacing()) + static_cast<int>(2.0 * Body->document()->documentMargin()) + (2 * Body->frameWidth()) + 8;
			Body->setFixedHeight(std::clamp(height, 120, 320));
		};
		connect(Body->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this, fit);
		Body->setFixedHeight(120);
		summaryRows->addWidget(Body);
		static_cast<summary_field*>(summaryBox)->watch(Body);
		layout->addWidget(summaryBox);
		RevealHint = new QLabel{ tr("Double-click a comment to open it in Review; Shift+double-click opens it in Discuss."), this };
		RevealHint->setObjectName(QStringLiteral("wholeReviewRevealHint"));
		RevealHint->setTextFormat(Qt::PlainText);
		RevealHint->setForegroundRole(QPalette::PlaceholderText);
		RevealHint->setWordWrap(true);
		RevealHint->setMinimumWidth(0);
		RevealHint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		RevealHint->hide();
		layout->addWidget(RevealHint);
		// The three verdicts, and Discard at the far end, quiet.
		auto* decision = new QHBoxLayout;
		decision->setSpacing(8);
		const auto names = std::array{ tr("Comment"), tr("Approve"), tr("Request changes") };
		const auto ids   = std::array{ QStringLiteral("wholeReviewComment"), QStringLiteral("wholeReviewApprove"), QStringLiteral("wholeReviewRequestChanges") };
		for (std::size_t index = 0U; index < Decisions.size(); ++index) {
			auto* button = new verdict_button{ this };
			button->setObjectName(ids[index]);
			button->setText(names[index]);
			button->setAccessibleName(names[index]);
			decision->addWidget(button);
			Decisions[index] = button;
			connect(button, &QToolButton::clicked, this, [this, index] { submit_as(decision_order[index]); });
		}
		decision->addStretch();
		Submit    = new QAction{ tr("Submit review"), this };
		Discard   = new QAction{ tr("Discard"), this };
		Reconcile = new QAction{ tr("Check status"), this };
		layout->addLayout(decision);
		// Said once, briefly (owner: "nothing is published until..." twice),
		// with Discard at the far end of the same line, quiet.
		auto* const after = new QHBoxLayout;
		after->setSpacing(8);
		Purpose = new QLabel{ tr("Nothing is published until you submit."), this };
		Purpose->setObjectName(QStringLiteral("wholeReviewPurpose"));
		Purpose->setTextFormat(Qt::PlainText);
		Purpose->setForegroundRole(QPalette::PlaceholderText);
		Purpose->setWordWrap(true);
		Purpose->setMinimumWidth(0);
		Purpose->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		after->addWidget(Purpose, 1);
		// [[BPR-1345]] v003. Discard review deletes, so it is drawn as the
		// question's deleting button is, quieter: red words, no fill at rest
		// (owner, discard-review v002: "Style the discard button similar to
		// the feedback I gave on delete comment.").
		DiscardButton = new danger_tool_button{ this };
		DiscardButton->setObjectName(QStringLiteral("wholeReviewDiscard"));
		DiscardButton->setDefaultAction(Discard);
		DiscardButton->setAccessibleName(Discard->text());
		after->addWidget(DiscardButton);
		layout->addLayout(after);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("wholeReviewStatus"));
		Status->setAccessibleName(tr("Review submission status"));
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		Status->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		Status->setMinimumWidth(0);
		Status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		layout->addWidget(Status);
		// Recovery appears only when it can be used: Check status is the
		// waiting panel's own action ([[BPR-1541]] v002).
		Recovery            = new QWidget{ Waiting };
		auto* const recover = new QHBoxLayout{ Recovery };
		recover->setContentsMargins(0, 0, 0, 0);
		auto* const check = new panel_action_button{ Recovery };
		check->setObjectName(QStringLiteral("wholeReviewReconcile"));
		check->setDefaultAction(Reconcile);
		check->setAccessibleName(Reconcile->text());
		ReconcileButton = check;
		recover->addWidget(ReconcileButton);
		recover->addStretch();
		panelText->addWidget(Recovery);
		// The host, the head and the account, behind a disclosure closed by
		// default (owner: "These details on finish review should be hidden by
		// default.").
		// [[BPR-1579]]. A disclosure bar drawn from the theme, not the style.
		Details = new disclosure_button{ this };
		Details->setObjectName(QStringLiteral("wholeReviewDetails"));
		Details->setText(tr("Pull request and account"));
		Details->setFocusPolicy(Qt::StrongFocus);
		Details->setAccessibleName(tr("Pull request and account"));
		Details->setToolTip(tr("Show the host, the pull request, its head and the account that submits"));
		// [[BPR-1590]] v002. Saved comments stays, quietly, beside the details.
		auto* const lastRow = new QHBoxLayout;
		lastRow->setSpacing(8);
		lastRow->addWidget(Details);
		lastRow->addStretch();
		auto* savedComments = new QToolButton(this);
		savedComments->setObjectName(QStringLiteral("wholeReviewSavedComments"));
		savedComments->setText(tr("Saved comments…"));
		savedComments->setAutoRaise(true);
		savedComments->setFocusPolicy(Qt::StrongFocus);
		savedComments->setToolTip(tr("Inspect comments saved under this pull request, including earlier revisions."));
		lastRow->addWidget(savedComments);
		connect(savedComments, &QToolButton::clicked, this, [this] { if (SavedCommentsReveal) { SavedCommentsReveal(); } });
		layout->addLayout(lastRow);
		Target = new QTextEdit{ this };
		Target->setObjectName(QStringLiteral("wholeReviewTarget"));
		Target->setReadOnly(true);
		Target->setAcceptRichText(false);
		Target->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
		Target->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Target->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Target->setFrameShape(QFrame::NoFrame);
		Target->setForegroundRole(QPalette::PlaceholderText);
		Target->viewport()->setAutoFillBackground(false);
		Target->setAccessibleName(tr("Review target and acting account"));
		Target->setMinimumWidth(0);
		Target->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		connect(Target->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
				this, [this](QSizeF aSize) { Target->setFixedHeight(static_cast<int>(std::ceil(aSize.height())) + 2 * Target->frameWidth()); });
		Target->hide();
		layout->addWidget(Target);
		connect(Details, &QToolButton::toggled, this, [this](const bool aOpen) { Target->setVisible(aOpen && !Shown.TargetDescription.isEmpty()); });
		layout->addStretch(1);
		connect(Body, &QPlainTextEdit::textChanged, this, [this] { emit_command(core::review_submission_command_kind::edit); });
		connect(Submit, &QAction::triggered, this, [this] { emit_command(core::review_submission_command_kind::begin_submit); });
		connect(Discard, &QAction::triggered, this, [this] { confirm_discard(); });
		connect(Reconcile, &QAction::triggered, this, [this] { emit_operation(review_submission_editor_operation::reconcile); });
		setFocusProxy(Body);
		restyle_waiting();
		present({ .Status = tr("Sign in and open an active pull request to write a review. Back returns to the pull request.") });
	}
	void review_submission_editor::present(const review_submission_editor_projection& aProjection)
	{
		const QSignalBlocker bodyBlock{ Body };
		Draft           = aProjection.Draft;
		Shown           = aProjection;
		const auto text = Draft ? QString::fromUtf8(Draft->Body) : QString{ };
		if (Body->toPlainText() != text) {
			Body->setPlainText(text);
		}
		if (Target->toPlainText() != aProjection.TargetDescription) {
			Target->setPlainText(aProjection.TargetDescription);
		}
		Details->setVisible(!aProjection.TargetDescription.isEmpty());
		Target->setVisible(Details->isChecked() && !aProjection.TargetDescription.isEmpty());
		// [[BPR-1541]] v002. While GitHub may hold the review, an empty
		// summary is not offered as a field to write in.
		const auto awaiting = Draft.has_value() && Draft->State == core::review_submission_state::submitting;
		Body->parentWidget()->setVisible(Draft.has_value() && !(awaiting && text.isEmpty()));
		Body->setVisible(Draft.has_value());
		for (auto* button : Decisions) {
			button->setVisible(Draft.has_value());
		}
		Preflight->setVisible(Draft.has_value());
		present_pending();
		// What the draft is, said only when it is not simply being written.
		QString    state;
		const auto drafting = Draft.has_value() && Draft->State == core::review_submission_state::drafting;
		if (!Draft) {
			state = tr("No review draft is available.");
		} else {
			switch (Draft->State) {
			case core::review_submission_state::drafting:
				state = Draft->Comments.empty() && core::kept(*Draft) ? tr("Your review draft is kept.") : QString{ };
				break;
			case core::review_submission_state::submitting:
			case core::review_submission_state::submitted:
			case core::review_submission_state::discarded:
				// [[BPR-1541]] v002. A review awaiting confirmation is said in
				// the waiting panel. [[BPR-1345]] v003. A finished review is
				// not shown: the page closes, and the next review starts at once.
				break;
			}
		}
		DraftState->setText(state);
		DraftState->setVisible(!state.isEmpty());
		Purpose->setVisible(drafting);
		// [[BPR-1541]] v002. A review awaiting GitHub's confirmation is told in
		// the waiting panel, not in the status line: its title, why, what is
		// safe, and Check status when it can be used.
		const auto waiting   = Draft.has_value() && Draft->State == core::review_submission_state::submitting;
		const auto checkable = waiting && aProjection.Controls.Reconcile;
		Status->setText(aProjection.Status);
		Status->setVisible(!aProjection.Status.isEmpty() && !waiting);
		const auto unknown = aProjection.Unknown && waiting ? review_submission_unknown_text(*aProjection.Unknown) : QString{ };
		UnknownReason->setText(unknown);
		UnknownReason->setVisible(!unknown.isEmpty());
		Status->setAccessibleDescription(unknown);
		WaitingTitle->setText(checkable ? tr("We couldn't confirm this review") : tr("Waiting for GitHub to confirm this review…"));
		const auto fallback = checkable && unknown.isEmpty() ? tr("GitHub didn't say whether it arrived.") : QString{ };
		WaitingFallback->setText(fallback);
		WaitingFallback->setVisible(!fallback.isEmpty());
		Waiting->setVisible(waiting);
		Waiting->setAccessibleName(WaitingTitle->text());
		Waiting->setAccessibleDescription((unknown.isEmpty() ? fallback : unknown) + QStringLiteral(" ") + WaitingAssure->text());
		const auto connected = Draft.has_value() && static_cast<bool>(Dispatch);
		Body->setReadOnly(!connected || !aProjection.Controls.Edit);
		Submit->setEnabled(connected && aProjection.Controls.Submit);
		Discard->setEnabled(connected && aProjection.Controls.Discard);
		const auto carried = Draft.has_value() && !Draft->Comments.empty();
		Discard->setText(carried ? tr("Discard review") : tr("Discard"));
		DiscardButton->setAccessibleName(Discard->text());
		// Nothing can be discarded while GitHub may hold the review.
		DiscardButton->setVisible(Draft.has_value() && !waiting);
		Reconcile->setEnabled(connected && aProjection.Controls.Reconcile);
		ReconcileButton->setVisible(Reconcile->isEnabled());
		Recovery->setVisible(Reconcile->isEnabled());
		Body->setToolTip(Body->isReadOnly() ? aProjection.Status : tr("Your summary is kept as a draft until you submit it."));
		present_preflight(aProjection);
		present_verdicts();
		Submit->setToolTip(Submit->isEnabled() ? tr("Publish this review with the displayed account and revision.")
											   : (aProjection.Status.isEmpty() ? tr("This review cannot be submitted as it is.") : aProjection.Status));
		Discard->setToolTip(Discard->isEnabled() ? (carried ? tr("Delete this review's pending comments and its summary. You are asked first.")
															: tr("Discard this unpublished review draft."))
												 : tr("Only an editable, unpublished draft can be discarded. %1").arg(aProjection.Status));
		Reconcile->setToolTip(Reconcile->isEnabled() ? tr("Check whether this exact review was submitted.") : tr("Check status is available only for a review awaiting confirmation."));
		const auto chosen       = Draft ? decisions_name(Draft->Disposition) : QString{ };
		const auto announcement = aProjection.TargetDescription + QStringLiteral("\n") + chosen + QStringLiteral(". ")
			+ (waiting ? WaitingTitle->text() + QStringLiteral(". ") + Waiting->accessibleDescription() : aProjection.Status);
		if (announcement != LastAnnouncement) {
			LastAnnouncement = announcement;
			QAccessibleAnnouncementEvent event{ this, announcement };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
		if (PresentationObserver) {
			PresentationObserver();
		}
	}
	void review_submission_editor::present_preflight(const review_submission_editor_projection& aProjection)
	{
		const auto& preflight = aProjection.Preflight;
		const auto  ratio     = devicePixelRatioF();
		const auto  theme     = MarkTheme.value_or(default_feedback_mark_theme(palette()));
		const auto  mark      = [&](core::feedback_label aLabel, feedback_mark_emphasis aEmphasis) {
			return feedback_mark_pixmap(aLabel, aEmphasis, feedback_mark_colours_for(theme, aLabel, palette()), ratio);
		};
		const auto blocking = feedback_mark_emphasis{ .Weight = feedback_mark_weight::blocking };
		const auto summary  = review_preflight_summary(preflight);
		auto       segments = std::vector<std::pair<QPixmap, QString>>{ };
		if (preflight) {
			segments.emplace_back(mark(core::feedback_label::issue, blocking), QString::number(preflight->Blockers.size()));
			if (!preflight->Pending.empty()) {
				segments.emplace_back(preflight_pixmap(preflight_icon::draft, palette(), ratio), QString::number(preflight->Pending.size()));
			}
			if (!preflight->Exact) {
				// An incomplete read shows its mark even when it counts nothing.
				segments.emplace_back(preflight_pixmap(preflight_icon::unknown, palette(), ratio), QString::number(core::uncertain(*preflight)));
			}
		} else {
			segments.emplace_back(preflight_pixmap(preflight_icon::unknown, palette(), ratio), QString{ });
		}
		static_cast<preflight_chip*>(Preflight)->set_segments(std::move(segments));
		Preflight->setToolTip(summary);
		Preflight->setAccessibleDescription(summary);

		// Approve keeps working while I have open blocking feedback; its dot
		// and tooltip say what approving does not do (owner, preflight B).
		auto* approve = Decisions[decision_index(core::review_disposition::approve)];
		if (const auto open = preflight ? core::open_blocking(*preflight) : 0U; open > 0U) {
			approve->setIcon(QIcon{ preflight_pixmap(preflight_icon::attention, palette(), ratio, attention_colour()) });
			approve->setToolTip(tr("Approve. You have %n open blocking comment(s) of your own; approving does not resolve them.", nullptr, static_cast<int>(open)));
		} else {
			approve->setIcon(QIcon{ });
			approve->setToolTip(tr("Approve this pull request."));
		}
		approve->setAccessibleDescription(approve->toolTip());

		PreflightMenu->clear();
		const auto reveal = [this](QAction* aAction, const std::string& aId) {
			connect(aAction, &QAction::triggered, this, [this, aId] {
				if (Reveal) {
					Reveal(aId);
				}
			});
		};
		const auto row = [&](const std::string& aId, const QPixmap& aMark, const QString& aState) {
			const auto found = aProjection.References.find(aId);
			const auto words = found == aProjection.References.end() ? text_of(aId) : found->second.Where + QStringLiteral(" · ") + found->second.Subject;
			auto*      added = PreflightMenu->addAction(QIcon{ aMark }, aState.isEmpty() ? words : words + QStringLiteral(" · ") + aState);
			added->setToolTip(tr("Show it"));
			reveal(added, aId);
		};
		const auto note = [&](const QPixmap& aMark, const QString& aWords) {
			auto* added = aMark.isNull() ? PreflightMenu->addAction(aWords) : PreflightMenu->addAction(QIcon{ aMark }, aWords);
			added->setEnabled(false);
		};
		if (!preflight) {
			note(preflight_pixmap(preflight_icon::unknown, palette(), ratio), summary);
			return;
		}
		PreflightMenu->addSection(tr("Yours, on GitHub"));
		for (const auto& blocker : preflight->Blockers) {
			row(blocker.Conversation, mark(blocker.Label, blocking), blocker.Resolution == core::feedback_resolution::unknown ? tr("state unknown") : QString{ });
		}
		if (preflight->Blockers.empty()) {
			note({ }, preflight->Exact ? tr("None open") : tr("None found"));
		}
		if (!preflight->Pending.empty()) {
			PreflightMenu->addSection(tr("Yours, not sent"));
			for (const auto& pending : preflight->Pending) {
				row(pending.Draft, mark(pending.Label, blocking), { });
			}
		}
		if (!preflight->Guidance.empty() || !preflight->Exact || preflight->Others > 0U) {
			PreflightMenu->addSeparator();
		}
		for (const auto& guidance : preflight->Guidance) {
			note(mark(guidance.Label, { }), guidance_line(guidance));
		}
		for (const auto& line : uncertainty_lines(*preflight)) {
			note(preflight_pixmap(preflight_icon::unknown, palette(), ratio), line);
		}
		if (preflight->Others > 0U) {
			note({ }, tr("Others: %n open blocking (not yours)", nullptr, static_cast<int>(preflight->Others)));
		}
	}
	void review_submission_editor::present_pending()
	{
		// [[BPR-1345]]. The title, the heading and the list follow the
		// comments the review carries.
		const auto  carried  = Draft.has_value() ? Draft->Comments : std::vector<core::review_submission_comment>{ };
		const auto& comments = carried;
		Title->setText(comments.empty() ? tr("Compose review") : tr("Finish your review"));
		const auto relist = std::exchange(Listed, comments) != comments || ListedRows.size() != static_cast<qsizetype>(comments.size());
		while (relist && PendingRows->count() > 0) {
			auto* const item = PendingRows->takeAt(0);
			if (auto* const widget = item->widget(); widget != nullptr) {
				widget->hide();
				widget->deleteLater();
			}
			delete item;
		}
		if (relist) {
			ListedRows.clear();
			auto code = QFontDatabase::systemFont(QFontDatabase::FixedFont);
			code.setPointSizeF(font().pointSizeF() * 0.95);
			auto shown = std::vector<std::pair<QString, QString>>{ };
			for (const auto& comment : comments) {
				const auto name  = text_of(comment.Path).section(QLatin1Char{ '/' }, -1);
				const auto lines = comment.StartLine ? QStringLiteral("%1–%2").arg(*comment.StartLine + 1U).arg(comment.Line + 1U) : QString::number(comment.Line + 1U);
				shown.emplace_back(name, comment.Side == core::diff_side::left ? tr("base %1").arg(lines) : lines);
			}
			const auto theme       = MarkTheme.value_or(default_feedback_mark_theme(palette()));
			auto       fileColumn  = 0;
			auto       linesColumn = pending_row::lines_width(QStringLiteral("0000"), code);
			auto       made        = std::vector<pending_row*>{ };
			for (std::size_t index = 0U; index < comments.size(); ++index) {
				const auto& comment       = comments[index];
				const auto  path          = text_of(comment.Path);
				const auto& [name, where] = shown[index];
				const auto lines          = comment.StartLine ? QStringLiteral("%1–%2").arg(*comment.StartLine + 1U).arg(comment.Line + 1U) : QString::number(comment.Line + 1U);
				auto       first          = text_of(comment.Body).section(QLatin1Char{ '\n' }, 0, 0).trimmed();
				if (first.isEmpty()) {
					first = tr("(no text yet)");
				}
				const auto spoken = comment.Side == core::diff_side::left
					? (comment.StartLine ? tr("%1, base lines %2: %3") : tr("%1, base line %2: %3"))
					: (comment.StartLine ? tr("%1, lines %2: %3") : tr("%1, line %2: %3"));
				const auto said   = spoken.arg(name, lines, first);
				if (index != 0U) {
					PendingRows->addWidget(new box_rule{ Pending });
				}
				auto* const row = new QWidget{ Pending };
				row->setObjectName(QStringLiteral("wholeReviewPendingItem"));
				auto* const across = new QHBoxLayout{ row };
				across->setContentsMargins(0, 0, 4, 0);
				across->setSpacing(2);
				auto* const open = new pending_row{ name, where, text_of(comment.Body), theme, row };
				open->set_repeat(index > 0U && comments[index - 1U].Path == comment.Path);
				fileColumn  = std::max(fileColumn, std::min(open->file_width(), 200));
				linesColumn = std::max(linesColumn, pending_row::lines_width(where, code));
				made.push_back(open);
				open->setAccessibleName(said);
				open->setAccessibleDescription(tr("Double-click or Return opens it in Review; with Shift, in Discuss"));
				// The whole comment is the tooltip: the row is one line however long.
				open->setToolTip(tr("Double-click to open in Review, Shift+double-click in Discuss.\n%1, %2\n\n%3").arg(path, where, text_of(comment.Body)));
				open->Open = [this, comment](const core::workbench_thread_destination aDestination) {
					if (PendingOpen) {
						PendingOpen(comment, aDestination);
					} else if (aDestination == core::workbench_thread_destination::review && PendingReveal) {
						PendingReveal(comment);
					}
				};
				across->addWidget(open, 1);
				auto* const remove = new QToolButton{ row };
				remove->setObjectName(QStringLiteral("wholeReviewPendingDelete"));
				remove->setAutoRaise(true);
				remove->setFocusPolicy(Qt::StrongFocus);
				remove->setIcon(conversation_icon(conversation_glyph::trash, palette()));
				remove->setIconSize(QSize{ 16, 16 });
				remove->setToolTip(tr("Delete this comment from your review"));
				remove->setAccessibleName(tr("Delete comment: %1").arg(said));
				connect(remove, &QToolButton::clicked, this, [this, comment] { confirm_delete(comment); });
				across->addWidget(remove, 0, Qt::AlignVCenter);
				ListedRows.push_back(said);
				PendingRows->addWidget(row);
			}
			for (auto* const open : made) {
				open->set_columns(fileColumn, linesColumn, code);
			}
		}
		Pending->setVisible(!comments.empty());
		// The heading holds my blocking feedback's chip whenever there is a
		// draft, and the count when the review carries comments.
		Heading->setVisible(Draft.has_value());
		RevealHint->setVisible(HintWanted && !comments.empty());
		HeadingRule->setVisible(Draft.has_value() && !comments.empty());
		SummaryRule->setVisible(false);
		SummaryHeading->setVisible(Draft.has_value());
		Carries->setText(comments.size() == 1U ? tr("1 pending comment") : tr("%1 pending comments").arg(comments.size()));
		Carries->setVisible(!comments.empty());
		Body->setPlaceholderText(!comments.empty() ? tr("Leave a summary (optional)") : tr("Write a review summary…"));
	}
	void review_submission_editor::present_verdicts()
	{
		// [[BPR-1345]] v002. Each verdict submits as itself, when the core
		// says it can; a disabled one says why.
		const auto connected = Draft.has_value() && static_cast<bool>(Dispatch);
		const auto editable  = connected && Shown.Controls.Edit;
		const auto carried   = Draft.has_value() && !Draft->Comments.empty();
		const auto blank     = Draft.has_value() && QString::fromUtf8(Draft->Body).trimmed().isEmpty();
		const auto count     = Draft.has_value() ? Draft->Comments.size() : 0U;
		const auto blocked   = Shown.Status.isEmpty() ? tr("This review cannot be submitted now.") : Shown.Status;
		for (const auto disposition : decision_order) {
			auto* const button  = Decisions[decision_index(disposition)];
			const auto  enabled = connected && core::submits_as(Shown.Controls, disposition);
			button->setEnabled(enabled);
			auto tip = QString{ };
			switch (disposition) {
			case core::review_disposition::comment:
				tip = enabled                       ? (count == 1U   ? tr("Submit your review with its comment, without approving.")
														   : carried ? tr("Submit your review with its %1 comments, without approving.").arg(count)
																	 : tr("Submit your review as a comment, without approving."))
					: editable && blank && !carried ? tr("Comment needs a summary when the review has no line comments.")
													: blocked;
				break;
			case core::review_disposition::approve:
				// The preflight's words: what approving does not do.
				tip = enabled ? button->toolTip() : blocked;
				break;
			case core::review_disposition::request_changes:
				tip = enabled           ? tr("Submit your review and request changes.")
					: editable && blank ? tr("Request changes needs a summary. Write one above first.")
										: blocked;
				break;
			}
			button->setToolTip(tip);
			button->setAccessibleDescription(tip);
		}
	}
	void review_submission_editor::submit_as(const core::review_disposition aDisposition)
	{
		if (!Draft || !Dispatch || !core::submits_as(Shown.Controls, aDisposition)) {
			return;
		}
		if (Draft->Disposition != aDisposition) {
			emit_command(core::review_submission_command_kind::choose_disposition, aDisposition);
		}
		// The verdict pressed is the one submitted, or nothing is.
		if (!Draft || Draft->Disposition != aDisposition || !core::submits_as(Shown.Controls, aDisposition)) {
			return;
		}
		emit_command(core::review_submission_command_kind::begin_submit);
	}
	void review_submission_editor::choose_disposition(const core::review_disposition aDisposition)
	{
		emit_command(core::review_submission_command_kind::choose_disposition, aDisposition);
	}
	void review_submission_editor::confirm_discard()
	{
		if (!Draft || !Dispatch || !Shown.Controls.Discard) {
			return;
		}
		// [[BPR-1345]] v002. "Discard review should delete all the review
		// comments and should come with a confirmation so it's not done
		// accidentally." A draft with nothing in it goes without asking.
		const auto count   = Draft->Comments.size();
		const auto summary = !QString::fromUtf8(Draft->Body).trimmed().isEmpty();
		if (count == 0U && !summary) {
			discard_now();
			return;
		}
		if (DiscardQuestion != nullptr) {
			DiscardQuestion->close();
		}
		// [[BPR-1345]] v003. The shared question, its title naming what goes
		// and its words only that it cannot be undone (owner, delete-comment
		// v002: "The cannot be undone warning is sufficient").
		const auto  text = count == 0U ? tr("Discard your review summary?")
			: count == 1U              ? tr("Discard your review and its 1 pending comment?")
									   : tr("Discard your review and its %1 pending comments?").arg(count);
		auto* const box  = new destructive_question{ *window(), QStringLiteral("discardReviewConfirmation"), text, tr("This cannot be undone."), count == 0U ? tr("Discard") : tr("Discard review") };
		DiscardQuestion  = box;
		const auto id    = Draft->Id;
		connect(box, &QDialog::accepted, this, [this, id] {
			if (Draft && Draft->Id == id) {
				discard_now();
			}
		});
		box->open();
	}
	void review_submission_editor::discard_now()
	{
		if (!Draft) {
			return;
		}
		const auto id = Draft->Id;
		emit_command(core::review_submission_command_kind::discard);
		// Discarded, the page has nothing left to show: the window closes it.
		if (DiscardObserver && (!Draft || Draft->Id != id || Draft->State == core::review_submission_state::discarded)) {
			DiscardObserver();
		}
	}
	void review_submission_editor::confirm_delete(const core::review_submission_comment& aComment)
	{
		if (!PendingDelete) {
			return;
		}
		if (DeleteQuestion != nullptr) {
			DeleteQuestion->close();
		}
		// [[BPR-1345]] v003. Where the comment is, then the comment itself in
		// a box as the drawer reads it, apart from the question's words.
		auto* const box  = new destructive_question{ *window(), QStringLiteral("deleteCommentConfirmation"), tr("Delete this comment from your review?"), tr("This cannot be undone."), tr("Delete comment") };
		auto        code = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		code.setPointSizeF(font().pointSizeF() * 0.95);
		box->set_subject(text_of(aComment.Path).section(QLatin1Char{ '/' }, -1), comment_lines(aComment.Side, aComment.Line, aComment.StartLine), aComment.Body,
						 "pending:" + std::to_string(aComment.Draft.value()) + ":" + std::to_string(aComment.DraftRevision), MarkTheme.value_or(default_feedback_mark_theme(palette())), code);
		DeleteQuestion = box;
		connect(box, &QDialog::accepted, this, [this, aComment] {
			if (PendingDelete) {
				PendingDelete(aComment);
			}
		});
		box->open();
	}
	void review_submission_editor::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange) {
			restyle_waiting();
		}
		// A theme change redraws the icons in the new ink.
		if (aEvent->type() == QEvent::PaletteChange && Pending != nullptr) {
			for (auto* const remove : Pending->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingDelete"))) {
				remove->setIcon(conversation_icon(conversation_glyph::trash, palette()));
			}
		}
	}
	auto review_submission_editor::title_label() const noexcept -> QLabel* { return Title; }
	auto review_submission_editor::pending_list() const noexcept -> QWidget* { return Pending; }
	auto review_submission_editor::context_label() const noexcept -> QLabel* { return Context; }
	void review_submission_editor::set_reveal_hint(const bool aShown)
	{
		HintWanted = aShown;
		RevealHint->setVisible(HintWanted && Draft.has_value() && !Draft->Comments.empty());
	}
	void review_submission_editor::set_context(const QString& aContext)
	{
		Context->setText(aContext);
		Context->setVisible(!aContext.isEmpty());
	}
	auto review_submission_editor::carries_label() const noexcept -> QLabel* { return Carries; }
	auto review_submission_editor::note_label() const noexcept -> QLabel* { return Purpose; }
	auto review_submission_editor::details_toggle() const noexcept -> QToolButton* { return Details; }
	auto review_submission_editor::discard_confirmation() const noexcept -> destructive_question* { return DiscardQuestion.data(); }
	auto review_submission_editor::discard_button() const noexcept -> danger_tool_button* { return DiscardButton; }
	void review_submission_editor::confirm_submitted(const core::review_submission_draft& aSubmitted)
	{
		LastSubmitted = aSubmitted;
		++Confirmed;
		if (SubmittedObserver) {
			SubmittedObserver(aSubmitted.Comments.size());
		}
	}
	void review_submission_editor::set_submitted_observer(std::function<void(std::size_t)> aObserver)
	{
		SubmittedObserver = std::move(aObserver);
	}
	auto review_submission_editor::last_submitted() const noexcept -> const std::optional<core::review_submission_draft>& { return LastSubmitted; }
	auto review_submission_editor::confirmed_submissions() const noexcept -> std::size_t { return Confirmed; }
	auto review_submission_editor::delete_confirmation() const noexcept -> destructive_question* { return DeleteQuestion.data(); }
	void review_submission_editor::set_pending_reveal(std::function<void(const core::review_submission_comment&)> aReveal)
	{
		PendingReveal = std::move(aReveal);
	}
	void review_submission_editor::set_pending_open(std::function<void(const core::review_submission_comment&, core::workbench_thread_destination)> aOpen)
	{
		PendingOpen = std::move(aOpen);
	}
	void review_submission_editor::set_pending_delete(std::function<void(const core::review_submission_comment&)> aDelete)
	{
		PendingDelete = std::move(aDelete);
	}
	void review_submission_editor::set_discard_observer(std::function<void()> aObserver)
	{
		DiscardObserver = std::move(aObserver);
	}
	auto review_submission_editor::pending_rows() const -> QStringList
	{
		return ListedRows;
	}
	void review_submission_editor::set_presentation_observer(std::function<void()> aObserver)
	{
		PresentationObserver = std::move(aObserver);
	}
	void review_submission_editor::set_item_reveal(std::function<void(const std::string&)> aReveal)
	{
		Reveal = std::move(aReveal);
	}
	void review_submission_editor::set_label_marks(const feedback_mark_theme& aTheme)
	{
		MarkTheme = aTheme;
		present_preflight(Shown);
		present_verdicts();
	}
	void review_submission_editor::set_attention_colour(const QColor& aColour)
	{
		if (Attention == aColour) {
			return;
		}
		Attention = aColour;
		present_preflight(Shown);
		present_verdicts();
		restyle_waiting();
	}
	void review_submission_editor::restyle_waiting()
	{
		if (Waiting == nullptr) {
			return;
		}
		const auto attention = attention_colour();
		static_cast<waiting_frame*>(Waiting)->set_attention(attention);
		WaitingMark->setPixmap(waiting_mark(attention, devicePixelRatioF()));
		// What is safe, said more quietly than why: the text mixed a quarter
		// toward the panel's ground, never below 4.5:1 against it.
		const auto ground = blend(palette().color(QPalette::Active, QPalette::Base), attention, waiting_tint);
		const auto text   = palette().color(QPalette::Active, QPalette::WindowText);
		auto       quiet  = blend(text, ground, 0.28);
		if (destructive_contrast(quiet, ground) < 4.5) {
			quiet = text;
		}
		auto colours = WaitingAssure->palette();
		colours.setColor(QPalette::WindowText, quiet);
		WaitingAssure->setPalette(colours);
	}
	auto review_submission_editor::attention_colour() const -> QColor
	{
		if (Attention.has_value()) {
			return *Attention;
		}
		const auto variant = palette().color(QPalette::Base).lightnessF() < 0.5 ? core::appearance_variant::dark : core::appearance_variant::light;
		return to_presentation_color(builtin_appearance_palette(variant)[core::index_of(core::appearance_role::state_attention)]);
	}
	void review_submission_editor::emit_command(core::review_submission_command_kind aKind, const std::optional<core::review_disposition> aDisposition)
	{
		if (!Draft || !Dispatch) {
			return;
		}
		auto command = core::review_submission_command{ .Id = Draft->Id, .Kind = aKind };
		if (aKind == core::review_submission_command_kind::edit) {
			command.Body = Body->toPlainText().toUtf8().toStdString();
		}
		if (aKind == core::review_submission_command_kind::choose_disposition) {
			command.Disposition = aDisposition.value_or(Draft->Disposition);
		}
		auto intent = review_submission_editor_intent{ .Command = std::move(command) };
		if (aKind == core::review_submission_command_kind::begin_submit) {
			intent.Prepared = Shown.Prepared;
		}
		Dispatch(std::move(intent));
	}
	void review_submission_editor::emit_operation(review_submission_editor_operation aOperation)
	{
		if (Draft && Dispatch) {
			Dispatch({ .Operation = aOperation, .Command = { .Id = Draft->Id } });
		}
	}
	void review_submission_editor::set_saved_comments_reveal(std::function<void()> aReveal) { SavedCommentsReveal = std::move(aReveal); }
	auto review_submission_editor::body_editor() const noexcept -> QPlainTextEdit* { return Body; }
	auto review_submission_editor::disposition_button(core::review_disposition aDisposition) const noexcept -> QToolButton* { return Decisions[decision_index(aDisposition)]; }
	auto review_submission_editor::preflight_button() const noexcept -> QToolButton* { return Preflight; }
	auto review_submission_editor::preflight_menu() const noexcept -> QMenu* { return PreflightMenu; }
	auto review_submission_editor::submit_action() const noexcept -> QAction* { return Submit; }
	auto review_submission_editor::discard_action() const noexcept -> QAction* { return Discard; }
	auto review_submission_editor::reconcile_action() const noexcept -> QAction* { return Reconcile; }
	auto review_submission_editor::status_label() const noexcept -> QLabel* { return Status; }
	auto review_submission_editor::unknown_reason_label() const noexcept -> QLabel* { return UnknownReason; }
	auto review_submission_editor::waiting_panel() const noexcept -> QWidget* { return Waiting; }
	auto review_submission_editor::waiting_title_label() const noexcept -> QLabel* { return WaitingTitle; }
	auto review_submission_editor::waiting_assurance_label() const noexcept -> QLabel* { return WaitingAssure; }
	auto review_submission_editor::reconcile_button() const noexcept -> QToolButton* { return ReconcileButton; }
	auto review_submission_editor::target_editor() const noexcept -> QTextEdit* { return Target; }
	auto review_submission_editor::shown_draft() const noexcept -> const std::optional<core::review_submission_draft>& { return Draft; }
	auto review_submission_editor::shown_projection() const noexcept -> const review_submission_editor_projection& { return Shown; }
}
