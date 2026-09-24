#include "bpr/qt/type_preview.hpp"

#include <QFontMetricsF>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QTextBlock>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		/// Breathing room either side of the preview gutter's widest number.
		constexpr int PreviewGutterPadding = 6;
		/// The sample's own line count, which fixes the surface's height: a preview
		/// that scrolled would hide the line the caption claims it shows.
		constexpr int PreviewRows = 6;
	} // namespace

	auto point_size_text(const std::uint32_t aCentipoints) -> QString
	{
		const auto whole      = aCentipoints / 100U;
		const auto hundredths = aCentipoints % 100U;
		return hundredths == 0U
			? QString::number(whole)
			: QStringLiteral("%1.%2").arg(whole).arg(
				  hundredths, 2, 10, QLatin1Char{ '0' });
	}

	/// \brief The preview's code surface: a code pane, painted as one.
	///
	/// \details A read-only `QPlainTextEdit`, which is what the review window's
	/// panes are, so the sample is laid out, tab-stopped and formatted by the same
	/// machinery rather than by a picture of it. What it adds over a bare pane is
	/// the two things the review panes do not draw and the criterion asks for: the
	/// faint tab markers and a fixed line-number column. Both come from the shared
	/// painter.
	class type_preview::code_pane final : public QPlainTextEdit
	{
	public:
		explicit code_pane(QWidget* const aParent)
			: QPlainTextEdit{ aParent }
		{
			setObjectName(QStringLiteral("typePreviewCode"));
			setReadOnly(true);
			setLineWrapMode(QPlainTextEdit::NoWrap);
			setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			// Not a control and not a tab stop: it is a specimen of the choices the
			// rows beside it make. The accessible name and description are what
			// carry it to a screen reader, which is why they are set rather than
			// left to the text.
			setFocusPolicy(Qt::NoFocus);
			setAccessibleName(QStringLiteral("Code preview"));
			// QWindows11Style clears the viewport's autoFillBackground during
			// polish, after which QPalette::Base is never painted (BPR-0475). The
			// applied appearance profile owns the code background, so the Qt
			// default is restored here as the comparison panes restore it.
			viewport()->setAutoFillBackground(true);
			Gutter = new QWidget{ this };
			Gutter->setObjectName(QStringLiteral("typePreviewGutter"));
			Gutter->setFocusPolicy(Qt::NoFocus);
			Gutter->installEventFilter(this);
			connect(
				this,
				&QPlainTextEdit::updateRequest,
				this,
				[this](const QRect&, int) { Gutter->update(); });
		}

		void set_line_numbers(std::vector<std::uint32_t> aNumbers)
		{
			Numbers = std::move(aNumbers);
			refresh_gutter_width();
		}

		void set_marker_colors(const QColor& aBackground, const QColor& aText)
		{
			GutterBackground = aBackground;
			GutterText       = aText;
			MarkerColor      = aText;
			Gutter->update();
			viewport()->update();
		}

		/// \brief [[BPR-1107]]. When white space is drawn, and which rows the
		/// sample's selection holds.
		void set_white_space(const core::white_space_drawing aDrawing, std::vector<bool> aSelectedRows)
		{
			WhiteSpace   = aDrawing;
			SelectedRows = std::move(aSelectedRows);
			viewport()->update();
		}

		[[nodiscard]] auto white_space_painted() const noexcept -> std::uint64_t
		{
			return WhiteSpaceMarks;
		}

		[[nodiscard]] auto gutter_numbers_painted() const noexcept -> std::uint64_t
		{
			return GutterNumbers;
		}

		[[nodiscard]] auto gutter_width() const -> int
		{
			return Numbers.empty()
				? 0
				: fontMetrics().horizontalAdvance(QStringLiteral("99"))
					+ 2 * PreviewGutterPadding;
		}

		void refresh_gutter_width()
		{
			setViewportMargins(gutter_width(), 0, 0, 0);
			place_gutter();
		}

	protected:
		auto eventFilter(QObject* const aWatched, QEvent* const aEvent)
			-> bool override
		{
			if (
				aWatched == Gutter && aEvent != nullptr
				&& aEvent->type() == QEvent::Paint) {
				paint_gutter();
				return true;
			}
			return QPlainTextEdit::eventFilter(aWatched, aEvent);
		}

		void paintEvent(QPaintEvent* const aEvent) override
		{
			QPlainTextEdit::paintEvent(aEvent);
			WhiteSpaceMarks = 0U;
			if (!MarkerColor.isValid() || WhiteSpace == core::white_space_drawing::none) {
				return;
			}
			// [[BPR-1107]]. The review panes' own helper, so the preview draws
			// white space exactly as the review window does.
			auto painter = QPainter{ viewport() };
			for (auto block = firstVisibleBlock(); block.isValid();
				 block      = block.next()) {
				const auto geometry = blockBoundingGeometry(block).translated(
					contentOffset());
				if (geometry.top() > static_cast<qreal>(viewport()->height())) {
					break;
				}
				const auto row = static_cast<std::size_t>(std::max(block.blockNumber(), 0));
				WhiteSpaceMarks += paint_white_space_of_block(
					painter, *this, block, WhiteSpace, row < SelectedRows.size() && SelectedRows[row], MarkerColor);
			}
		}

		void resizeEvent(QResizeEvent* const aEvent) override
		{
			QPlainTextEdit::resizeEvent(aEvent);
			place_gutter();
		}

		void changeEvent(QEvent* const aEvent) override
		{
			QPlainTextEdit::changeEvent(aEvent);
			// A different font is a different digit width, and the code beside it
			// has to move by exactly the same amount.
			if (aEvent != nullptr && aEvent->type() == QEvent::FontChange) {
				refresh_gutter_width();
			}
		}

	private:
		void place_gutter()
		{
			const auto area = contentsRect();
			Gutter->setGeometry(
				QRect{ area.left(), area.top(), gutter_width(), area.height() });
		}

		void paint_gutter()
		{
			auto       painter = QPainter{ Gutter };
			const auto rows    = gutter_rows();
			painter.setFont(font());
			GutterNumbers = paint_code_gutter(
				painter,
				QRectF{ 0.0,
						0.0,
						static_cast<qreal>(Gutter->width()),
						static_cast<qreal>(Gutter->height()) },
				rows,
				GutterBackground,
				GutterText);
		}

		[[nodiscard]] auto gutter_rows() const -> std::vector<code_gutter_row>
		{
			auto rows = std::vector<code_gutter_row>{ };
			for (auto block = firstVisibleBlock(); block.isValid();
				 block      = block.next()) {
				const auto geometry = blockBoundingGeometry(block).translated(
					contentOffset());
				if (geometry.top() > static_cast<qreal>(viewport()->height())) {
					break;
				}
				const auto index = static_cast<std::size_t>(
					std::max(block.blockNumber(), 0));
				if (!block.isVisible() || index >= Numbers.size()) {
					continue;
				}
				rows.push_back(code_gutter_row{
					.Top    = geometry.top(),
					.Height = geometry.height(),
					.Number = Numbers[index],
				});
			}
			return rows;
		}

		QWidget*                   Gutter = nullptr;
		std::vector<std::uint32_t> Numbers;
		QColor                     GutterBackground;
		QColor                     GutterText;
		QColor                     MarkerColor;
		core::white_space_drawing  WhiteSpace = core::white_space_drawing::none;
		std::vector<bool>          SelectedRows;
		std::uint64_t              WhiteSpaceMarks = 0U;
		std::uint64_t              GutterNumbers   = 0U;
	};

	type_preview::type_preview(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("appearanceTypePreview"));
		setAccessibleName(QStringLiteral("Preview"));

		Heading = new QLabel{ QStringLiteral("Preview"), this };
		Heading->setObjectName(QStringLiteral("typePreviewHeading"));

		auto* const sample = new QFrame{ this };
		sample->setObjectName(QStringLiteral("typePreviewInterface"));
		sample->setFrameShape(QFrame::StyledPanel);
		InterfaceSample = sample;
		InterfaceRow    = new QLabel{ QStringLiteral("Files"), sample };
		InterfaceCount  = new QLabel{ QStringLiteral("12 of 40 read"), sample };
		InterfacePath   = new QLabel{
			QStringLiteral("Engine/Source/Runtime/Renderer"), sample
		};
		// Quiet through the palette rather than through a colour: the placeholder
		// role is the one the application palette already carries for secondary
		// text, so these follow the theme with everything else.
		for (auto* const quiet : { InterfaceCount, InterfacePath }) {
			quiet->setForegroundRole(QPalette::PlaceholderText);
		}
		auto* const sampleLayout = new QVBoxLayout{ sample };
		auto* const row          = new QHBoxLayout{ };
		row->addWidget(InterfaceRow);
		row->addStretch(1);
		row->addWidget(InterfaceCount);
		sampleLayout->addLayout(row);
		sampleLayout->addWidget(InterfacePath);

		Code = new code_pane{ this };

		Caption = new QLabel{ this };
		Caption->setObjectName(QStringLiteral("typePreviewCaption"));
		Caption->setWordWrap(true);
		Caption->setForegroundRole(QPalette::PlaceholderText);

		auto* const layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(Heading);
		layout->addWidget(sample);
		layout->addWidget(Code);
		layout->addWidget(Caption);
		layout->addStretch(1);

		// The colours a surface nobody has adopted into paints with: none. Every
		// role stays an invalid colour, so the sample shows the inherited native
		// text on the inherited background instead of a palette this widget chose.
		refresh_code();
		refresh_interface();
		refresh_caption();
	}

	void type_preview::adopt_appearance(appearance_profile& aProfile)
	{
		Colors     = code_role_colors_of(aProfile.palette());
		Typography = aProfile.typography();
		Emphasis   = code_syntax_emphasis{
			.Bold   = Typography.Code.SyntaxBold,
			.Italic = Typography.Code.SyntaxItalic,
		};
		// Applied through the profile's own members, in the order every code
		// surface gets them: the palette first, so the selection wash the bands
		// read is this theme's selection colour, then the fonts, so the tab stop
		// and the measured row heights below are the ones now in effect.
		aProfile.apply_to(*Code);
		aProfile.apply_to(*InterfaceSample);
		aProfile.apply_ui_font_to(*InterfaceSample);
		aProfile.apply_code_font_to(*Code);
		++Statistics.Adoptions;
		refresh_code();
		refresh_interface();
		refresh_caption();
	}

	void type_preview::set_theme_name(QString aName)
	{
		if (ThemeName == aName) {
			return;
		}
		ThemeName = std::move(aName);
		refresh_caption();
	}

	auto type_preview::heading() const noexcept -> QLabel*
	{
		return Heading;
	}

	auto type_preview::interface_sample() const noexcept -> QWidget*
	{
		return InterfaceSample;
	}

	auto type_preview::code_surface() const noexcept -> QPlainTextEdit*
	{
		return Code;
	}

	auto type_preview::caption() const noexcept -> QLabel*
	{
		return Caption;
	}

	auto type_preview::caption_text() const -> QString
	{
		return Caption->text();
	}

	auto type_preview::statistics() const noexcept -> type_preview_statistics
	{
		auto statistics                 = Statistics;
		statistics.WhiteSpacePainted    = Code->white_space_painted();
		statistics.GutterNumbersPainted = Code->gutter_numbers_painted();
		return statistics;
	}

	void type_preview::refresh_code()
	{
		const auto sample  = type_preview_code_sample();
		const auto built   = build_code_sample_document(sample);
		auto       numbers = std::vector<std::uint32_t>{ };
		numbers.reserve(sample.size());
		for (const auto& row : sample) {
			numbers.push_back(row.Number);
		}
		Code->set_line_numbers(std::move(numbers));
		Code->set_marker_colors(
			Colors[core::index_of(core::appearance_role::code_gutter_background)],
			Colors[core::index_of(core::appearance_role::code_line_number_text)]);
		auto selectedRows = std::vector<bool>{ };
		for (const auto& row : sample) {
			selectedRows.push_back(row.Selected);
		}
		Code->set_white_space(Typography.Code.WhiteSpace, std::move(selectedRows));
		// Replaced only when the text actually differs. Setting it unconditionally
		// would drop every block layout on each adoption, so a typography change
		// that moved no glyph would still relayout the whole sample.
		if (Code->toPlainText() != built.Text) {
			Code->setPlainText(built.Text);
		}
		Code->setFixedHeight(
			static_cast<int>(
				QFontMetricsF{ Code->font() }.lineSpacing()
				* static_cast<qreal>(PreviewRows))
			+ 2 * Code->frameWidth() + 2 * static_cast<int>(Code->document()->documentMargin()));

		Statistics.FormattedRows = 0U;
		for (auto row = std::size_t{ 0U }; row < sample.size(); ++row) {
			const auto block = Code->document()->findBlockByNumber(
				static_cast<int>(row));
			if (!block.isValid()) {
				continue;
			}
			const auto& bytes   = built.RowBytes[row];
			const auto  request = code_syntax_format_request{
				.LineBytes = std::span<const std::uint8_t>{
					reinterpret_cast<const std::uint8_t*>(bytes.constData()),
					static_cast<std::size_t>(bytes.size()) },
				.Spans    = spans_of_line(built.Spans, static_cast<std::uint32_t>(row)),
				.Colors   = &Colors,
				.Emphasis = Emphasis,
			};
			if (apply_syntax_format_ranges(
					*Code->document(),
					block,
					code_syntax_format_ranges(request))) {
				++Statistics.FormattedRows;
			}
		}

		auto selections = QList<QTextEdit::ExtraSelection>{ };
		for (auto row = std::size_t{ 0U }; row < sample.size(); ++row) {
			const auto line = static_cast<std::uint32_t>(row);
			const auto span = core::line_span{ .Begin = line, .End = line + 1U };
			append_line_band(
				selections,
				*Code,
				span,
				code_sample_row_fill(sample[row].Kind, Colors));
			if (sample[row].Selected) {
				append_selection_band(selections, *Code, span, span);
			}
		}
		Statistics.BandsPainted = static_cast<std::uint64_t>(selections.size());
		Code->setExtraSelections(selections);
		Code->setAccessibleDescription(built.Text);
		Code->viewport()->update();
	}

	void type_preview::refresh_interface()
	{
		// The line height the record asks for, shown the one way a row can show it:
		// the row is that tall. `QPlainTextDocumentLayout` ignores every
		// line-height mode Qt offers (BPR-0118), so a sample that claimed to apply
		// one inside a text document would be claiming something untrue.
		const auto height = static_cast<int>(line_height_of(Typography.Ui));
		for (auto* const label : { InterfaceRow, InterfaceCount, InterfacePath }) {
			label->setMinimumHeight(height);
		}
	}

	void type_preview::refresh_caption()
	{
		const auto& ui       = Typography.Ui;
		const auto& code     = Typography.Code;
		const auto  emphasis = [&code]() -> QString {
			if (code.SyntaxBold && code.SyntaxItalic) {
				return QStringLiteral("bold and italic emphasis on");
			}
			if (!code.SyntaxBold && !code.SyntaxItalic) {
				return QStringLiteral("bold and italic emphasis off");
			}
			return code.SyntaxBold
				? QStringLiteral("bold emphasis on, italic off")
				: QStringLiteral("italic emphasis on, bold off");
		}();
		// [[BPR-1107]]. When white space is drawn, in the words its row uses.
		const auto whiteSpace = [&code]() -> QString {
			switch (code.WhiteSpace) {
			case core::white_space_drawing::none:
				return QStringLiteral("white space not drawn");
			case core::white_space_drawing::selection:
				return QStringLiteral("white space drawn under the selection");
			case core::white_space_drawing::leading:
				return QStringLiteral("leading white space drawn");
			case core::white_space_drawing::trailing:
				return QStringLiteral("trailing white space drawn");
			case core::white_space_drawing::boundary:
				return QStringLiteral("leading and trailing white space drawn");
			case core::white_space_drawing::all:
				return QStringLiteral("all white space drawn");
			}
			return { };
		}();
		Caption->setText(
			QStringLiteral(
				"Interface %1 pt, weight %2, line height %3 %. Code %4 pt, weight "
				"%5, tabs of %6, ligatures %7, %8, %9. Diff, selection and "
				"current-line colours from %10.")
				.arg(point_size_text(ui.Font.PointSizeCentipoints))
				.arg(ui.Font.Weight)
				.arg(ui.LineHeightPercent)
				.arg(point_size_text(code.Font.PointSizeCentipoints))
				.arg(code.Font.Weight)
				.arg(code.TabWidth)
				.arg(
					code.Ligatures ? QStringLiteral("on") : QStringLiteral("off"))
				.arg(emphasis)
				.arg(whiteSpace)
				.arg(
					ThemeName.isEmpty()
						? QStringLiteral("the theme on screen")
						: ThemeName));
		Caption->setAccessibleName(Caption->text());
	}
} // namespace bpr::qt
