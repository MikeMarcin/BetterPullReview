#include "bpr/qt/destructive_question.hpp"

#include "bpr/qt/markdown_body_view.hpp"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QEnterEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSpacerItem>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto words(const char* aText) -> QString
		{
			return QCoreApplication::translate("bpr::qt::destructive_question", aText);
		}

		[[nodiscard]] auto blend(const QColor& aFrom, const QColor& aTo, const qreal aAmount) -> QColor
		{
			const auto amount = std::clamp(aAmount, 0.0, 1.0);
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() + (aTo.redF() - aFrom.redF()) * amount),
									static_cast<float>(aFrom.greenF() + (aTo.greenF() - aFrom.greenF()) * amount),
									static_cast<float>(aFrom.blueF() + (aTo.blueF() - aFrom.blueF()) * amount));
		}

		[[nodiscard]] auto luminance(const QColor& aColour) -> double
		{
			const auto channel = [](const double aValue) { return aValue <= 0.03928 ? aValue / 12.92 : std::pow((aValue + 0.055) / 1.055, 2.4); };
			return 0.2126 * channel(aColour.redF()) + 0.7152 * channel(aColour.greenF()) + 0.0722 * channel(aColour.blueF());
		}

		/// `aInk` as it is where it reads on `aGround` at `aRatio`, else moved
		/// toward `aToward` just until it does, so it keeps its hue where it can.
		[[nodiscard]] auto reading(const QColor& aInk, const QColor& aGround, const QColor& aToward, const double aRatio) -> QColor
		{
			for (auto step = 0; step <= 20; ++step) {
				if (const auto ink = blend(aInk, aToward, step / 20.0); destructive_contrast(ink, aGround) >= aRatio) {
					return ink;
				}
			}
			return aToward;
		}

		/// The panel's inner width: what the words and the comment's box wrap at.
		constexpr int panel_width   = 460;
		constexpr int side_left     = 24;
		constexpr int side_right    = 20;
		constexpr int box_padding_x = 14;
		constexpr int box_padding_y = 10;
		/// A long comment scrolls inside its box rather than growing the question.
		constexpr int subject_height = 240;
		/// The drawer's mark and the gap between its gutter and the words.
		constexpr int mark_size = 16;
		constexpr int mark_gap  = 9;

		/// The close mark: an × in the quiet ink, the surface lifted under the
		/// pointer, the highlight's ring on focus.
		class close_mark final : public QAbstractButton
		{
		public:
			explicit close_mark(QWidget* aParent)
				: QAbstractButton{ aParent }
			{
				setCursor(Qt::PointingHandCursor);
				setFocusPolicy(Qt::StrongFocus);
				setAttribute(Qt::WA_Hover);
			}
			[[nodiscard]] auto sizeHint() const -> QSize override { return { 28, 28 }; }

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto       painter = QPainter{ this };
				const auto colours = destructive_colours_of(palette());
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
				if (underMouse() || isDown()) {
					painter.setPen(Qt::NoPen);
					painter.setBrush(colours.Neutral);
					painter.drawRoundedRect(box, 7.0, 7.0);
				}
				const auto side   = 10.0;
				const auto centre = box.center();
				painter.setPen(QPen{ underMouse() ? colours.Ink : colours.Muted, 1.6, Qt::SolidLine, Qt::RoundCap });
				painter.drawLine(QPointF{ centre.x() - side / 2.0, centre.y() - side / 2.0 }, QPointF{ centre.x() + side / 2.0, centre.y() + side / 2.0 });
				painter.drawLine(QPointF{ centre.x() + side / 2.0, centre.y() - side / 2.0 }, QPointF{ centre.x() - side / 2.0, centre.y() + side / 2.0 });
				if (hasFocus()) {
					painter.setPen(QPen{ colours.Focus, 2.0 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(box, 7.0, 7.0);
				}
			}
			void enterEvent(QEnterEvent* aEvent) override
			{
				QAbstractButton::enterEvent(aEvent);
				update();
			}
			void leaveEvent(QEvent* aEvent) override
			{
				QAbstractButton::leaveEvent(aEvent);
				update();
			}
		};

		/// The comment's box: recessed onto the window's own ground, with a
		/// hairline, so the comment reads apart from the question's words.
		class subject_frame final : public QWidget
		{
		public:
			explicit subject_frame(QWidget* aParent)
				: QWidget{ aParent }
			{ }

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto       painter = QPainter{ this };
				const auto colours = destructive_colours_of(palette());
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(QPen{ colours.RecessEdge, 1.0 });
				painter.setBrush(colours.Recess);
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);
			}
		};
	} // namespace

	auto destructive_contrast(const QColor& aFirst, const QColor& aSecond) -> double
	{
		const auto first  = luminance(aFirst);
		const auto second = luminance(aSecond);
		return (std::max(first, second) + 0.05) / (std::min(first, second) + 0.05);
	}

	auto destructive_colours_of(const QPalette& aPalette, const std::optional<QColor> aSurface) -> destructive_colours
	{
		auto       colours   = destructive_colours{ };
		const auto window    = aPalette.color(QPalette::Active, QPalette::Window);
		const auto base      = aPalette.color(QPalette::Active, QPalette::Base);
		const auto text      = aPalette.color(QPalette::Active, QPalette::WindowText);
		colours.Dark         = window.lightnessF() < 0.5;
		colours.HighContrast = destructive_contrast(text, window) >= 15.0;
		const auto ratio     = colours.HighContrast ? 7.0 : 4.5;
		// Raised from the window: in a dark theme a little toward the words,
		// in a light one the text ground. A high-contrast panel keeps the
		// window's own ground and is drawn by its edge.
		colours.Surface     = aSurface.has_value() ? *aSurface
			: colours.HighContrast                 ? window
			: colours.Dark                         ? blend(window, text, 0.07)
												   : base;
		const auto& surface = colours.Surface;
		colours.Edge        = blend(surface, text, colours.HighContrast ? 0.85 : 0.16);
		colours.Ink         = text;
		colours.Muted       = reading(blend(text, surface, 0.42), surface, text, ratio);
		colours.Recess      = colours.HighContrast ? surface : window;
		if (destructive_contrast(colours.Recess, surface) < 1.04) {
			colours.Recess = blend(surface, text, colours.HighContrast ? 0.0 : 0.04);
		}
		colours.RecessEdge  = blend(colours.Recess, text, colours.HighContrast ? 0.85 : 0.15);
		colours.Neutral     = colours.HighContrast ? surface : blend(surface, text, colours.Dark ? 0.08 : 0.05);
		colours.NeutralEdge = blend(surface, text, colours.HighContrast ? 0.85 : colours.Dark ? 0.22
																							  : 0.24);
		// The red of each variant: the brightest that still reads on its tint.
		const auto red     = colours.Dark
			? (colours.HighContrast ? QColor{ 0xFF, 0x9A, 0x94 } : QColor{ 0xFF, 0x7B, 0x72 })
			: (colours.HighContrast ? QColor{ 0x8F, 0x0D, 0x17 } : QColor{ 0xC2, 0x1F, 0x2B });
		colours.DangerTint = blend(surface, red, colours.Dark ? (colours.HighContrast ? 0.22 : 0.16) : (colours.HighContrast ? 0.10 : 0.09));
		colours.Danger     = reading(red, colours.DangerTint, text, ratio);
		colours.DangerEdge = colours.HighContrast ? colours.Danger : blend(colours.DangerTint, colours.Danger, colours.Dark ? 0.30 : 0.35);
		colours.Focus      = aPalette.color(QPalette::Active, QPalette::Highlight);
		return colours;
	}

	auto comment_lines(const core::diff_side aSide, const std::uint32_t aLine, const std::optional<std::uint32_t> aStartLine) -> QString
	{
		const auto lines = aStartLine ? QStringLiteral("%1–%2").arg(*aStartLine + 1U).arg(aLine + 1U) : QString::number(aLine + 1U);
		if (aSide == core::diff_side::left) {
			return (aStartLine ? words("base lines %1") : words("base line %1")).arg(lines);
		}
		return (aStartLine ? words("lines %1") : words("line %1")).arg(lines);
	}

	question_button::question_button(const QString& aText, const question_button_role aRole, QWidget* aParent)
		: QPushButton{ aText, aParent }
		, Role{ aRole }
	{
		setProperty("questionRole", aRole == question_button_role::destructive ? QStringLiteral("destructive") : QStringLiteral("neutral"));
		setAccessibleName(aText);
		setCursor(Qt::PointingHandCursor);
		setFocusPolicy(Qt::StrongFocus);
		setAttribute(Qt::WA_Hover);
	}

	auto question_button::role() const noexcept -> question_button_role
	{
		return Role;
	}

	auto question_button::sizeHint() const -> QSize
	{
		auto medium = font();
		medium.setWeight(QFont::Medium);
		const auto metrics = QFontMetrics{ medium };
		return { std::max(84, metrics.horizontalAdvance(text()) + 36), std::max(34, metrics.height() + 16) };
	}

	auto question_button::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void question_button::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto       painter     = QPainter{ this };
		const auto colours     = destructive_colours_of(palette());
		const auto destructive = Role == question_button_role::destructive;
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto box  = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
		const auto ink  = destructive ? colours.Danger : colours.Ink;
		auto       fill = destructive ? colours.DangerTint : colours.Neutral;
		if (isDown()) {
			fill = blend(fill, ink, 0.14);
		} else if (underMouse()) {
			fill = blend(fill, ink, 0.07);
		}
		painter.setPen(QPen{ destructive ? colours.DangerEdge : colours.NeutralEdge, 1.0 });
		painter.setBrush(fill);
		painter.drawRoundedRect(box, 8.0, 8.0);
		if (hasFocus()) {
			painter.setPen(QPen{ colours.Focus, 2.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 7.5, 7.5);
		}
		auto medium = font();
		medium.setWeight(QFont::Medium);
		painter.setFont(medium);
		painter.setPen(ink);
		painter.drawText(rect(), Qt::AlignCenter, text());
	}

	void question_button::enterEvent(QEnterEvent* aEvent)
	{
		QPushButton::enterEvent(aEvent);
		update();
	}

	void question_button::leaveEvent(QEvent* aEvent)
	{
		QPushButton::leaveEvent(aEvent);
		update();
	}

	danger_tool_button::danger_tool_button(QWidget* aParent)
		: QToolButton{ aParent }
	{
		setProperty("questionRole", QStringLiteral("destructive"));
		setAutoRaise(true);
		setCursor(Qt::PointingHandCursor);
		setFocusPolicy(Qt::StrongFocus);
		setToolButtonStyle(Qt::ToolButtonTextOnly);
		setAttribute(Qt::WA_Hover);
	}

	auto danger_tool_button::sizeHint() const -> QSize
	{
		const auto metrics = QFontMetrics{ font() };
		return { metrics.horizontalAdvance(text()) + 24, std::max(28, metrics.height() + 10) };
	}

	auto danger_tool_button::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	auto danger_tool_button::ink() const -> QColor
	{
		const auto colours = destructive_colours_of(palette(), palette().color(QPalette::Active, QPalette::Window));
		return isEnabled() ? colours.Danger : colours.Muted;
	}

	void danger_tool_button::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto       painter = QPainter{ this };
		const auto colours = destructive_colours_of(palette(), palette().color(QPalette::Active, QPalette::Window));
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto box = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
		// At rest only the words are red; under the pointer the tint and edge
		// say what a press would do.
		if (isEnabled() && (underMouse() || isDown())) {
			painter.setPen(QPen{ colours.DangerEdge, 1.0 });
			painter.setBrush(isDown() ? blend(colours.DangerTint, colours.Danger, 0.12) : colours.DangerTint);
			painter.drawRoundedRect(box, 6.0, 6.0);
		}
		if (hasFocus()) {
			painter.setPen(QPen{ colours.Focus, 2.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(box, 6.0, 6.0);
		}
		painter.setPen(ink());
		painter.setFont(font());
		painter.drawText(rect(), Qt::AlignCenter, text());
	}

	void danger_tool_button::enterEvent(QEnterEvent* aEvent)
	{
		QToolButton::enterEvent(aEvent);
		update();
	}

	void danger_tool_button::leaveEvent(QEvent* aEvent)
	{
		QToolButton::leaveEvent(aEvent);
		update();
	}

	destructive_question::destructive_question(QWidget& aOwner, const QString& aName, const QString& aQuestion, const QString& aDetail, const QString& aConfirm)
		: QDialog{ &aOwner, Qt::Dialog | Qt::FramelessWindowHint }
	{
		setObjectName(aName);
		setAttribute(Qt::WA_DeleteOnClose);
		setAttribute(Qt::WA_TranslucentBackground);
		// The themes are applied per window: the question takes its window's,
		// and follows it while open.
		Owner = &aOwner;
		setPalette(aOwner.palette());
		setFont(aOwner.font());
		aOwner.installEventFilter(this);
		setWindowModality(Qt::WindowModal);
		setWindowTitle(aQuestion);
		setAccessibleName(aQuestion);
		setAccessibleDescription(aDetail);
		setFixedWidth(panel_width);

		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(side_left, 18, side_right, 20);
		column->setSpacing(0);

		// The question, and the close mark at its top right.
		auto* const header = new QHBoxLayout;
		header->setSpacing(12);
		Title = new QLabel{ aQuestion, this };
		Title->setObjectName(aName + QStringLiteral("Title"));
		Title->setTextFormat(Qt::PlainText);
		Title->setWordWrap(true);
		auto heading = font();
		heading.setPointSizeF(heading.pointSizeF() * 1.3);
		heading.setWeight(QFont::Bold);
		Title->setFont(heading);
		Title->setContentsMargins(0, 4, 0, 0);
		header->addWidget(Title, 1, Qt::AlignTop);
		Close = new close_mark{ this };
		Close->setObjectName(aName + QStringLiteral("Close"));
		Close->setAccessibleName(words("Close"));
		Close->setAccessibleDescription(words("Closes this question without deleting anything"));
		Close->setToolTip(words("Close  Esc"));
		header->addWidget(Close, 0, Qt::AlignTop);
		column->addLayout(header);

		// What is deleted: where it is, then the comment in its box.
		GapAbove = new QSpacerItem{ 0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed };
		column->addSpacerItem(GapAbove);
		Location           = new QWidget{ this };
		auto* const across = new QHBoxLayout{ Location };
		across->setContentsMargins(0, 0, 0, 0);
		across->setSpacing(6);
		File = new QLabel{ Location };
		File->setObjectName(aName + QStringLiteral("File"));
		File->setTextFormat(Qt::PlainText);
		auto file = font();
		file.setWeight(QFont::DemiBold);
		File->setFont(file);
		Lines = new QLabel{ Location };
		Lines->setObjectName(aName + QStringLiteral("Lines"));
		Lines->setTextFormat(Qt::PlainText);
		across->addWidget(File);
		across->addWidget(Lines);
		across->addStretch(1);
		Location->hide();
		column->addWidget(Location);
		GapBox = new QSpacerItem{ 0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed };
		column->addSpacerItem(GapBox);
		Box = new subject_frame{ this };
		Box->setObjectName(aName + QStringLiteral("Subject"));
		auto* const inside = new QHBoxLayout{ Box };
		inside->setContentsMargins(box_padding_x, box_padding_y, box_padding_x, box_padding_y);
		inside->setSpacing(mark_gap);
		// The label's mark in a left gutter beside the words, as the drawer
		// draws a post's.
		Mark = new QLabel{ Box };
		Mark->setObjectName(aName + QStringLiteral("Mark"));
		Mark->setFixedWidth(mark_size);
		Mark->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
		Mark->hide();
		inside->addWidget(Mark, 0, Qt::AlignTop);
		Scroll = new QScrollArea{ Box };
		Scroll->setObjectName(aName + QStringLiteral("SubjectScroll"));
		Scroll->setFrameShape(QFrame::NoFrame);
		Scroll->setWidgetResizable(true);
		Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		Scroll->viewport()->setAutoFillBackground(false);
		Scroll->setFocusPolicy(Qt::NoFocus);
		inside->addWidget(Scroll);
		Box->hide();
		column->addWidget(Box);

		// The muted words, ending "This cannot be undone.".
		GapDetail = new QSpacerItem{ 0, 8, QSizePolicy::Minimum, QSizePolicy::Fixed };
		column->addSpacerItem(GapDetail);
		Detail = new QLabel{ aDetail, this };
		Detail->setObjectName(aName + QStringLiteral("Detail"));
		Detail->setTextFormat(Qt::PlainText);
		Detail->setWordWrap(true);
		column->addWidget(Detail);
		column->addSpacing(22);

		// Cancel, the default, then the deleting button at the far right.
		auto* const buttons = new QHBoxLayout;
		buttons->setSpacing(10);
		buttons->addStretch(1);
		Cancel = new question_button{ words("Cancel"), question_button_role::neutral, this };
		Cancel->setObjectName(aName + QStringLiteral("Cancel"));
		Cancel->setAccessibleDescription(words("Keeps it and closes this question"));
		Confirm = new question_button{ aConfirm, question_button_role::destructive, this };
		Confirm->setObjectName(aName + QStringLiteral("Confirm"));
		Confirm->setAccessibleDescription(aDetail);
		Confirm->setAutoDefault(false);
		Cancel->setAutoDefault(true);
		Cancel->setDefault(true);
		buttons->addWidget(Cancel);
		buttons->addWidget(Confirm);
		column->addLayout(buttons);

		setTabOrder(Cancel, Confirm);
		setTabOrder(Confirm, Close);
		connect(Cancel, &QAbstractButton::clicked, this, &QDialog::reject);
		connect(Close, &QAbstractButton::clicked, this, &QDialog::reject);
		connect(Confirm, &QAbstractButton::clicked, this, &QDialog::accept);
		refresh_colours();
		fit_height();
	}

	void destructive_question::set_subject(const QString& aFile, const QString& aLines, const std::string& aBody, const std::string& aIdentity, const feedback_mark_theme& aMarks, const QFont& aCodeFont)
	{
		File->setText(aFile);
		Lines->setText(aLines);
		Lines->setVisible(!aLines.isEmpty());
		Where = aLines.isEmpty() ? aFile : QStringLiteral("%1, %2").arg(aFile, aLines);
		if (Reader == nullptr) {
			Reader = new markdown_body_view{ Scroll };
			Reader->setObjectName(objectName() + QStringLiteral("Comment"));
			Reader->set_in_place(true);
			Reader->set_height_observer([this] { fit_subject(); });
			Scroll->setWidget(Reader);
		}
		Reader->set_code_font(aCodeFont, 4.0 * QFontMetricsF{ aCodeFont }.horizontalAdvance(QLatin1Char{ ' ' }));
		Reader->set_body(aBody, aIdentity);
		Body      = aBody;
		MarkTheme = aMarks;
		refresh_mark();
		// The question's own keys stay on its buttons: the comment is read,
		// not visited.
		Reader->setFocusPolicy(Qt::NoFocus);
		Reader->rendered_view()->setFocusPolicy(Qt::NoFocus);
		Reader->source_view()->setFocusPolicy(Qt::NoFocus);
		Reader->rendered_view()->setAccessibleName(words("Comment"));
		Location->show();
		Box->show();
		GapAbove->changeSize(0, 12, QSizePolicy::Minimum, QSizePolicy::Fixed);
		GapBox->changeSize(0, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
		GapDetail->changeSize(0, 14, QSizePolicy::Minimum, QSizePolicy::Fixed);
		setAccessibleDescription(QStringLiteral("%1. %2 %3").arg(Where, Reader->rendered_view()->toPlainText().left(400), Detail->text()));
		refresh_colours();
		fit_subject();
	}

	auto destructive_question::question() const -> QString { return Title->text(); }
	auto destructive_question::detail() const -> QString { return Detail->text(); }
	auto destructive_question::location() const -> QString { return Where; }
	auto destructive_question::title_label() const noexcept -> QLabel* { return Title; }
	auto destructive_question::detail_label() const noexcept -> QLabel* { return Detail; }
	auto destructive_question::location_label() const noexcept -> QLabel* { return File; }
	auto destructive_question::subject_box() const noexcept -> QWidget* { return Box; }
	auto destructive_question::subject_view() const noexcept -> markdown_body_view* { return Reader; }
	auto destructive_question::subject_mark() const noexcept -> QLabel* { return Mark; }

	void destructive_question::refresh_mark()
	{
		const auto mark = feedback_body_mark_of(Body);
		if (!mark.Label.has_value() || !MarkTheme.has_value()) {
			Mark->hide();
			return;
		}
		const auto colours = feedback_mark_colours_for(*MarkTheme, *mark.Label, palette());
		Mark->setPixmap(feedback_mark_pixmap(*mark.Label, mark.Emphasis, colours, devicePixelRatioF()));
		Mark->setAccessibleName(mark.Name);
		Mark->setToolTip(mark.Name);
		// Centred on the first line of words.
		Mark->setContentsMargins(0, std::max(0, (QFontMetrics{ font() }.height() - mark_size) / 2), 0, 0);
		Mark->show();
	}
	auto destructive_question::confirm_button() const noexcept -> question_button* { return Confirm; }
	auto destructive_question::cancel_button() const noexcept -> question_button* { return Cancel; }
	auto destructive_question::close_button() const noexcept -> QAbstractButton* { return Close; }
	auto destructive_question::colours() const -> destructive_colours { return destructive_colours_of(palette()); }

	void destructive_question::refresh_colours()
	{
		const auto colours = destructive_colours_of(palette());
		const auto paint   = [](QLabel& aLabel, const QColor& aInk) {
			auto ink = aLabel.palette();
			ink.setColor(QPalette::WindowText, aInk);
			ink.setColor(QPalette::Text, aInk);
			aLabel.setPalette(ink);
		};
		paint(*Title, colours.Ink);
		paint(*File, colours.Ink);
		paint(*Lines, colours.Muted);
		paint(*Detail, colours.Muted);
		update();
	}

	void destructive_question::fit_subject()
	{
		if (Reader == nullptr) {
			return;
		}
		const auto width  = panel_width - side_left - side_right - 2 * box_padding_x - (Mark->isHidden() ? 0 : mark_size + mark_gap);
		const auto wanted = Reader->heightForWidth(width);
		Scroll->setFixedHeight(std::clamp(wanted, 1, subject_height));
		fit_height();
	}

	void destructive_question::fit_height()
	{
		auto* const column = layout();
		column->activate();
		const auto height = column->hasHeightForWidth() ? column->totalHeightForWidth(panel_width) : column->totalSizeHint().height();
		setFixedHeight(std::max(height, column->totalMinimumSize().height()));
	}

	void destructive_question::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto       painter = QPainter{ this };
		const auto colours = destructive_colours_of(palette());
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ colours.Edge, 1.0 });
		painter.setBrush(colours.Surface);
		painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), corner_radius, corner_radius);
	}

	void destructive_question::showEvent(QShowEvent* aEvent)
	{
		fit_height();
		QDialog::showEvent(aEvent);
		// Cancel takes the focus: Return keeps, as Esc does.
		Cancel->setFocus(Qt::OtherFocusReason);
	}

	auto destructive_question::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aWatched == Owner && Owner != nullptr) {
			if (aEvent->type() == QEvent::PaletteChange) {
				setPalette(Owner->palette());
			} else if (aEvent->type() == QEvent::FontChange) {
				setFont(Owner->font());
			}
		}
		return QDialog::eventFilter(aWatched, aEvent);
	}

	void destructive_question::changeEvent(QEvent* aEvent)
	{
		QDialog::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange) {
			if (Title != nullptr) {
				refresh_colours();
				if (MarkTheme.has_value()) {
					refresh_mark();
				}
			}
		}
	}
} // namespace bpr::qt
