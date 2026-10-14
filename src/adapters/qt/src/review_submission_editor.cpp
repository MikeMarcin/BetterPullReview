#include "bpr/qt/review_submission_editor.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "conversation_glyphs.hpp"

#include <QAbstractButton>
#include <QAbstractTextDocumentLayout>
#include <QAccessible>
#include <QAction>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QPlainTextEdit>
#include <QPolygonF>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

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
		[[nodiscard]] auto accent_ink(const QPalette& aPalette) -> QColor
		{
			const auto ground    = aPalette.color(QPalette::Active, QPalette::Base);
			const auto highlight = aPalette.color(QPalette::Active, QPalette::Highlight);
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

		/// [[BPR-1345]] v002. One pending comment, as a button that goes to it:
		/// its file, its lines in the code font and the highlight, and its
		/// first line of words, in three aligned columns.
		class pending_row final : public QAbstractButton
		{
		public:
			pending_row(QString aFile, QString aLines, QString aWords, QWidget* aParent)
				: QAbstractButton{ aParent }
				, File{ std::move(aFile) }
				, Lines{ std::move(aLines) }
				, Words{ std::move(aWords) }
			{
				setObjectName(QStringLiteral("wholeReviewPendingOpen"));
				setAttribute(Qt::WA_Hover, true);
				setCursor(Qt::PointingHandCursor);
				setFocusPolicy(Qt::StrongFocus);
				setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			}

			/// \brief The widths this row's file name and lines need, in its own
			/// fonts.
			[[nodiscard]] auto file_width() const -> int
			{
				auto bold = font();
				bold.setWeight(QFont::DemiBold);
				return QFontMetrics{ bold }.horizontalAdvance(File) + 2;
			}

			[[nodiscard]] static auto lines_width(const QString& aLines, const QFont& aCode) -> int
			{
				return QFontMetrics{ aCode }.horizontalAdvance(aLines) + 2;
			}

			/// \brief The same file as the row above: its name is drawn faint, so
			/// each file is read once.
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
				update();
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				return { FileColumn + LinesColumn + 160, std::max(28, QFontMetrics{ font() }.height() + 10) };
			}

			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				return { 80, sizeHint().height() };
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto        painter = QPainter{ this };
				const auto& colours = palette();
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto ground    = colours.color(QPalette::Active, QPalette::Base);
				const auto ink       = colours.color(QPalette::Active, QPalette::Text);
				const auto highlight = colours.color(QPalette::Active, QPalette::Highlight);
				const auto box       = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
				if (underMouse() || isDown()) {
					painter.setPen(Qt::NoPen);
					painter.setBrush(blend(ground, highlight, isDown() ? 0.18 : 0.09));
					painter.drawRoundedRect(box, 4.0, 4.0);
				}
				auto bold = font();
				bold.setWeight(QFont::DemiBold);
				auto x = gutter;
				painter.setFont(bold);
				painter.setPen(Repeat ? colours.color(QPalette::Active, QPalette::PlaceholderText) : ink);
				painter.drawText(QRect{ x, 0, FileColumn, height() }, Qt::AlignLeft | Qt::AlignVCenter,
								 QFontMetrics{ bold }.elidedText(File, Qt::ElideMiddle, FileColumn));
				x += FileColumn + spacing;
				painter.setFont(Code);
				painter.setPen(accent_ink(colours));
				painter.drawText(QRect{ x, 0, LinesColumn, height() }, Qt::AlignRight | Qt::AlignVCenter, Lines);
				x += LinesColumn + spacing;
				painter.setFont(font());
				painter.setPen(ink);
				painter.drawText(QRect{ x, 0, std::max(0, width() - x - gutter), height() }, Qt::AlignLeft | Qt::AlignVCenter,
								 QFontMetrics{ font() }.elidedText(Words, Qt::ElideRight, std::max(0, width() - x - gutter)));
				if (hasFocus()) {
					painter.setPen(QPen{ highlight, 2.0 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(box, 4.0, 4.0);
				}
			}

		private:
			static constexpr int gutter  = 8;
			static constexpr int spacing = 14;
			QString              File;
			QString              Lines;
			QString              Words;
			QFont                Code;
			int                  FileColumn  = 0;
			int                  LinesColumn = 0;
			bool                 Repeat      = false;
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
		auto* savedComments = new QToolButton(this);
		savedComments->setObjectName(QStringLiteral("wholeReviewSavedComments"));
		savedComments->setText(tr("Saved comments…"));
		savedComments->setAutoRaise(true);
		savedComments->setToolTip(tr("Inspect comments saved under this pull request, including earlier revisions."));
		layout->addWidget(savedComments, 0, Qt::AlignLeft);
		connect(savedComments, &QToolButton::clicked, this, [this] { if (SavedCommentsReveal) { SavedCommentsReveal(); } });
		DraftState = new QLabel{ this };
		DraftState->setObjectName(QStringLiteral("wholeReviewDraftState"));
		DraftState->setWordWrap(true);
		DraftState->setTextFormat(Qt::PlainText);
		layout->addWidget(DraftState);
		// The list's heading, with my blocking feedback's chip at its end.
		auto* const heading = new QHBoxLayout;
		heading->setSpacing(8);
		Carries = new QLabel{ this };
		Carries->setObjectName(QStringLiteral("wholeReviewCarries"));
		Carries->setTextFormat(Qt::PlainText);
		auto headingFont = Carries->font();
		headingFont.setWeight(QFont::DemiBold);
		Carries->setFont(headingFont);
		Carries->hide();
		heading->addWidget(Carries);
		heading->addStretch();
		Preflight = new preflight_chip{ this };
		Preflight->setObjectName(QStringLiteral("wholeReviewPreflight"));
		Preflight->setFocusPolicy(Qt::StrongFocus);
		Preflight->setAccessibleName(tr("Your blocking feedback"));
		Preflight->setPopupMode(QToolButton::InstantPopup);
		PreflightMenu = new QMenu{ Preflight };
		PreflightMenu->setObjectName(QStringLiteral("wholeReviewPreflightMenu"));
		PreflightMenu->setToolTipsVisible(true);
		Preflight->setMenu(PreflightMenu);
		heading->addWidget(Preflight);
		layout->addLayout(heading);
		// [[BPR-1345]]. The comments the review carries, each where it is and
		// what it says; v002: each goes to its comment, and can be deleted.
		Pending = new pending_box{ this };
		Pending->setObjectName(QStringLiteral("wholeReviewPending"));
		Pending->setAccessibleName(tr("Pending in this review"));
		PendingRows = new QVBoxLayout{ Pending };
		PendingRows->setContentsMargins(4, 4, 4, 4);
		PendingRows->setSpacing(0);
		Pending->hide();
		layout->addWidget(Pending);
		// The summary sits in the same hairline box as the list, so the two
		// read as one family in every theme, high contrast included.
		auto* const summaryBox = new pending_box{ this };
		summaryBox->setObjectName(QStringLiteral("wholeReviewSummaryBox"));
		auto* const summaryRows = new QVBoxLayout{ summaryBox };
		summaryRows->setContentsMargins(6, 6, 6, 6);
		Body = new QPlainTextEdit{ summaryBox };
		Body->setObjectName(QStringLiteral("wholeReviewSummary"));
		Body->setAccessibleName(tr("Review summary"));
		Body->setPlaceholderText(tr("Write a review summary…"));
		Body->setTabChangesFocus(true);
		Body->setFrameShape(QFrame::NoFrame);
		// Room for a few paragraphs; the page, not the field, takes what is
		// left, so an empty summary is not a wall.
		Body->setMinimumHeight(160);
		summaryRows->addWidget(Body);
		layout->addWidget(summaryBox);
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
		Submit          = new QAction{ tr("Submit review"), this };
		Discard         = new QAction{ tr("Discard"), this };
		Reconcile       = new QAction{ tr("Check status"), this };
		const auto flat = [this](QAction* aAction, const QString& aName) {
			auto* button = new QToolButton{ this };
			button->setObjectName(aName);
			button->setDefaultAction(aAction);
			button->setAutoRaise(true);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setAccessibleName(aAction->text());
			return button;
		};
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
		// Recovery appears only when it can be used.
		Recovery            = new QWidget{ this };
		auto* const recover = new QHBoxLayout{ Recovery };
		recover->setContentsMargins(0, 0, 0, 0);
		ReconcileButton = flat(Reconcile, QStringLiteral("wholeReviewReconcile"));
		ReconcileButton->setParent(Recovery);
		recover->addWidget(ReconcileButton);
		recover->addStretch();
		layout->addWidget(Recovery);
		// The host, the head and the account, behind a disclosure closed by
		// default (owner: "These details on finish review should be hidden by
		// default.").
		Details = new QToolButton{ this };
		Details->setObjectName(QStringLiteral("wholeReviewDetails"));
		Details->setText(tr("Pull request and account"));
		Details->setCheckable(true);
		Details->setChecked(false);
		Details->setAutoRaise(true);
		Details->setArrowType(Qt::RightArrow);
		Details->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		Details->setFocusPolicy(Qt::StrongFocus);
		Details->setAccessibleName(tr("Pull request and account"));
		Details->setToolTip(tr("Show the host, the pull request, its head and the account that submits"));
		layout->addWidget(Details);
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
		connect(Details, &QToolButton::toggled, this, [this](const bool aOpen) {
			Details->setArrowType(aOpen ? Qt::DownArrow : Qt::RightArrow);
			Target->setVisible(aOpen && !Shown.TargetDescription.isEmpty());
		});
		layout->addStretch(1);
		connect(Body, &QPlainTextEdit::textChanged, this, [this] { emit_command(core::review_submission_command_kind::edit); });
		connect(Submit, &QAction::triggered, this, [this] { emit_command(core::review_submission_command_kind::begin_submit); });
		connect(Discard, &QAction::triggered, this, [this] { confirm_discard(); });
		connect(Reconcile, &QAction::triggered, this, [this] { emit_operation(review_submission_editor_operation::reconcile); });
		setFocusProxy(Body);
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
		Body->parentWidget()->setVisible(Draft.has_value());
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
				state = tr("Submission is in progress or awaiting confirmation. Check status before trying again.");
				break;
			case core::review_submission_state::submitted:
			case core::review_submission_state::discarded:
				// [[BPR-1345]] v003. A finished review is not shown: the page
				// closes, and the next review starts at once.
				break;
			}
		}
		DraftState->setText(state);
		DraftState->setVisible(!state.isEmpty());
		Purpose->setVisible(drafting);
		Status->setText(aProjection.Status);
		Status->setVisible(!aProjection.Status.isEmpty());
		const auto connected = Draft.has_value() && static_cast<bool>(Dispatch);
		Body->setReadOnly(!connected || !aProjection.Controls.Edit);
		Submit->setEnabled(connected && aProjection.Controls.Submit);
		Discard->setEnabled(connected && aProjection.Controls.Discard);
		const auto carried = Draft.has_value() && !Draft->Comments.empty();
		Discard->setText(carried ? tr("Discard review") : tr("Discard"));
		DiscardButton->setAccessibleName(Discard->text());
		DiscardButton->setVisible(Draft.has_value());
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
		const auto announcement = aProjection.TargetDescription + QStringLiteral("\n") + chosen + QStringLiteral(". ") + aProjection.Status;
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
			// The columns line up: the widest file name (to a limit) and the
			// widest lines, measured in the rows' own fonts once they exist.
			auto fileColumn  = 0;
			auto linesColumn = pending_row::lines_width(QStringLiteral("0000"), code);
			auto made        = std::vector<pending_row*>{ };
			for (std::size_t index = 0U; index < comments.size(); ++index) {
				const auto& comment       = comments[index];
				const auto  path          = text_of(comment.Path);
				const auto& [name, where] = shown[index];
				const auto lines          = comment.StartLine ? QStringLiteral("%1–%2").arg(*comment.StartLine + 1U).arg(comment.Line + 1U) : QString::number(comment.Line + 1U);
				auto       first          = text_of(comment.Body).section(QLatin1Char{ '\n' }, 0, 0).trimmed();
				if (first.isEmpty()) {
					first = tr("(no text yet)");
				}
				const auto  spoken = comment.Side == core::diff_side::left
					? (comment.StartLine ? tr("%1, base lines %2: %3") : tr("%1, base line %2: %3"))
					: (comment.StartLine ? tr("%1, lines %2: %3") : tr("%1, line %2: %3"));
				const auto  said   = spoken.arg(name, lines, first);
				auto* const row    = new QWidget{ Pending };
				row->setObjectName(QStringLiteral("wholeReviewPendingItem"));
				auto* const across = new QHBoxLayout{ row };
				across->setContentsMargins(0, 0, 0, 0);
				across->setSpacing(2);
				auto* const open = new pending_row{ name, where, first, row };
				open->set_repeat(index > 0U && comments[index - 1U].Path == comment.Path);
				fileColumn  = std::max(fileColumn, std::min(open->file_width(), 240));
				linesColumn = std::max(linesColumn, pending_row::lines_width(where, code));
				made.push_back(open);
				open->setAccessibleName(said);
				open->setAccessibleDescription(tr("Go to this comment beside the code"));
				open->setToolTip(QStringLiteral("%1\n%2").arg(path, text_of(comment.Body)));
				connect(open, &QAbstractButton::clicked, this, [this, comment] {
					if (PendingReveal) {
						PendingReveal(comment);
					}
				});
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
				across->addWidget(remove);
				ListedRows.push_back(said);
				PendingRows->addWidget(row);
			}
			for (auto* const open : made) {
				open->set_columns(fileColumn, linesColumn, code);
			}
		}
		Pending->setVisible(!comments.empty());
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
		// A theme change redraws the icons in the new ink.
		if (aEvent->type() == QEvent::PaletteChange && Pending != nullptr) {
			for (auto* const remove : Pending->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingDelete"))) {
				remove->setIcon(conversation_icon(conversation_glyph::trash, palette()));
			}
		}
	}
	auto review_submission_editor::title_label() const noexcept -> QLabel* { return Title; }
	auto review_submission_editor::pending_list() const noexcept -> QWidget* { return Pending; }
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
	auto review_submission_editor::target_editor() const noexcept -> QTextEdit* { return Target; }
	auto review_submission_editor::shown_draft() const noexcept -> const std::optional<core::review_submission_draft>& { return Draft; }
	auto review_submission_editor::shown_projection() const noexcept -> const review_submission_editor_projection& { return Shown; }
}
