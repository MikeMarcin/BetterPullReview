#include "bpr/qt/appearance_settings_panel.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/type_preview.hpp"

#include <QAbstractButton>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMetaObject>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QValidator>

#include <algorithm>
#include <string>
#include <utility>

namespace bpr::qt {
	auto settings_section_band(const QPalette& aPalette) -> QColor
	{
		// About 1.2:1 against the surface: enough to read as a band, and in
		// Dark (#2C3036 on #1C2026) still far from the bright strip the owner
		// rejected. Titles keep at least 11:1 in the built-in themes.
		constexpr auto step    = 0.08F;
		const auto     surface = aPalette.color(QPalette::Window);
		const auto     text    = aPalette.color(QPalette::WindowText);
		const auto     mix     = [](const float aFrom, const float aTo) {
			return aFrom + step * (aTo - aFrom);
		};
		return QColor::fromRgbF(
			mix(surface.redF(), text.redF()),
			mix(surface.greenF(), text.greenF()),
			mix(surface.blueF(), text.blueF()));
	}

	namespace {
		// The row language of RFC-BPR-0032 as RFC-BPR-0034 amends it, on the same
		// 8-point grid the Accounts page uses. A row is one line, so its height is a
		// line's height and not a stacked label's.
		constexpr auto card_inset = 16;
		constexpr auto row_inset  = 10;
		constexpr auto row_height = 32;
		constexpr auto column_gap = 12;
		constexpr auto glyph_gap  = 4;
		/// The three widths the accepted design's tab segment offers.
		constexpr auto tab_width_options = std::array<std::uint32_t, 3U>{ 2U, 4U, 8U };

		[[nodiscard]] auto make_hairline(QWidget* const aParent) -> QFrame*
		{
			auto* const line = new QFrame{ aParent };
			line->setFrameShape(QFrame::HLine);
			line->setFrameShadow(QFrame::Plain);
			line->setForegroundRole(QPalette::Mid);
			line->setFixedHeight(1);
			return line;
		}

		/// \brief A size stepper that reads in points and carries hundredths.
		///
		/// \details The core stores centipoints so that every legal size is exactly
		/// representable, and `core::bounds_of` bounds that storage. The control
		/// therefore keeps the stored value — which is what makes the range the
		/// core's own rather than a converted copy of it — and shows points, with the
		/// two decimals dropped when they are zero. The page this replaces showed
		/// "1300" for thirteen points.
		///
		/// Prefix and suffix are deliberately not used: Qt's own validation would
		/// then read the typed text as the stored integer and refuse "13" as below
		/// the minimum. Rendering and parsing are both done here instead, so typing a
		/// size and stepping one agree.
		class point_spin_box final : public QSpinBox
		{
		public:
			explicit point_spin_box(QWidget* const aParent)
				: QSpinBox{ aParent }
			{
				// One whole point per step, which is what the design's stepper moves.
				setSingleStep(100);
			}

		protected:
			[[nodiscard]] auto textFromValue(const int aValue) const
				-> QString override
			{
				return QStringLiteral("%1 pt").arg(point_size_text(
					static_cast<std::uint32_t>(std::max(aValue, 0))));
			}

			[[nodiscard]] auto valueFromText(const QString& aText) const
				-> int override
			{
				return centipoints_of(aText).value_or(value());
			}

			[[nodiscard]] auto validate(QString& aInput, int&) const
				-> QValidator::State override
			{
				const auto centipoints = centipoints_of(aInput);
				if (!centipoints.has_value()) {
					return aInput.trimmed().isEmpty() ? QValidator::Intermediate
													  : QValidator::Invalid;
				}
				return *centipoints < minimum() || *centipoints > maximum()
					? QValidator::Intermediate
					: QValidator::Acceptable;
			}

		private:
			[[nodiscard]] static auto centipoints_of(const QString& aText)
				-> std::optional<int>
			{
				auto text = aText.trimmed();
				if (text.endsWith(QStringLiteral("pt"), Qt::CaseInsensitive)) {
					text.chop(2);
					text = text.trimmed();
				}
				auto       accepted = false;
				const auto points   = QLocale{ }.toDouble(text, &accepted);
				return accepted
					? std::optional{ static_cast<int>(qRound(points * 100.0)) }
					: std::nullopt;
			}
		};

		/// \brief A banner strip painted from its palette's live section band.
		///
		/// \details Painted rather than given a palette of its own, so a theme
		/// change reaches it through ordinary palette propagation (BPR-1513).
		class section_band final : public QWidget
		{
		public:
			using QWidget::QWidget;

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				auto painter = QPainter{ this };
				painter.fillRect(aEvent->rect(), settings_section_band(palette()));
			}
		};

		/// \brief One card: a header that names it and carries its Reset, and a body.
		struct settings_card
		{
			QFrame*      Frame  = nullptr;
			QHBoxLayout* Header = nullptr;
			QVBoxLayout* Body   = nullptr;
			QLabel*      Title  = nullptr;
		};

		[[nodiscard]] auto make_card(
			QWidget* const aParent,
			const QString& aTitle,
			const QString& aObjectName) -> settings_card
		{
			auto card  = settings_card{ };
			card.Frame = new QFrame{ aParent };
			card.Frame->setObjectName(aObjectName);
			// The platform's own panel frame rather than a style sheet, which
			// RFC-BPR-0034 prohibits along with a second palette.
			card.Frame->setFrameShape(QFrame::StyledPanel);
			card.Frame->setAccessibleName(aTitle);
			auto* const column = new QVBoxLayout{ card.Frame };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(0);

			// A band derived from the application surface and text; AlternateBase
			// is the independently customisable code current-line fill (BPR-1513).
			auto* const header = new section_band{ card.Frame };
			header->setObjectName(aObjectName + QStringLiteral("Header"));
			card.Header = new QHBoxLayout{ header };
			card.Header->setContentsMargins(
				card_inset, row_inset, card_inset, row_inset);
			card.Header->setSpacing(column_gap / 2);
			card.Title = new QLabel{ aTitle, header };
			card.Title->setObjectName(aObjectName + QStringLiteral("Title"));
			auto title = card.Title->font();
			title.setWeight(QFont::DemiBold);
			card.Title->setFont(title);
			card.Header->addWidget(card.Title);
			card.Header->addStretch(1);
			column->addWidget(header);
			column->addWidget(make_hairline(card.Frame));

			auto* const body = new QWidget{ card.Frame };
			body->setObjectName(aObjectName + QStringLiteral("Body"));
			card.Body = new QVBoxLayout{ body };
			card.Body->setContentsMargins(
				card_inset, row_inset / 2, card_inset, row_inset / 2);
			card.Body->setSpacing(0);
			column->addWidget(body);
			return card;
		}

		/// \brief A card header's button: flat, named, and never a default button.
		[[nodiscard]] auto make_header_button(
			QWidget* const aParent,
			const QString& aText,
			const QString& aAccessibleName,
			const QString& aObjectName) -> QPushButton*
		{
			auto* const button = new QPushButton{ aText, aParent };
			button->setObjectName(aObjectName);
			button->setAccessibleName(aAccessibleName);
			button->setFlat(true);
			button->setAutoDefault(false);
			button->setFocusPolicy(Qt::StrongFocus);
			return button;
		}

		/// \brief What one row of the shared language is made of.
		struct language_row
		{
			QWidget*    Frame = nullptr;
			QLabel*     Label = nullptr;
			info_glyph* Glyph = nullptr;
		};

		/// \brief Adds one row: label, info glyph, control, on one line.
		///
		/// \details The accepted shape, the same one `server_card` builds: the
		/// consequence sentence lives in the glyph's popover and is the glyph's
		/// accessible description, and it is put on the control as well so a reviewer
		/// who reaches the control alone is told what it does. Nothing here is
		/// reachable by pointer only.
		///
		/// The pieces are returned for a caller that needs them; most rows are built
		/// and never spoken of again, so the result is deliberately not required.
		auto add_row(
			QVBoxLayout&   aRows,
			QWidget* const aParent,
			const QString& aObjectName,
			const QString& aLabel,
			const QString& aConsequence,
			QWidget* const aControl) -> language_row
		{
			auto row  = language_row{ };
			row.Frame = new QWidget{ aParent };
			row.Frame->setObjectName(aObjectName);
			row.Frame->setMinimumHeight(row_height);
			auto* const line = new QHBoxLayout{ row.Frame };
			line->setContentsMargins(0, 0, 0, 0);
			line->setSpacing(glyph_gap);

			row.Label = new QLabel{ aLabel, row.Frame };
			row.Label->setObjectName(aObjectName + QStringLiteral("Name"));
			row.Label->setTextInteractionFlags(Qt::NoTextInteraction);
			row.Glyph = new info_glyph{ aConsequence, row.Frame };
			row.Glyph->setObjectName(aObjectName + QStringLiteral("Glyph"));
			row.Glyph->setAccessibleName(
				QStringLiteral("About %1").arg(aLabel.toLower()));

			line->addWidget(row.Label, 0, Qt::AlignVCenter);
			line->addWidget(row.Glyph, 0, Qt::AlignVCenter);
			line->addStretch(1);
			if (aControl != nullptr) {
				aControl->setParent(row.Frame);
				if (aControl->accessibleName().isEmpty()) {
					aControl->setAccessibleName(aLabel);
				}
				aControl->setAccessibleDescription(row.Glyph->consequence());
				line->addWidget(aControl, 0, Qt::AlignVCenter);
			}
			if (aRows.count() > 0) {
				aRows.addWidget(make_hairline(aParent));
			}
			aRows.addWidget(row.Frame);
			return row;
		}

		// There is deliberately no mode wording here any more. The six-way combo box
		// is gone and the tiles are the control, so the only place a mode is named is
		// the strip that draws it.

		[[nodiscard]] auto target_text(const core::typography_target aTarget)
			-> QString
		{
			switch (aTarget) {
			case core::typography_target::ui_font:
				return QStringLiteral("Interface font");
			case core::typography_target::code_font:
				return QStringLiteral("Code font");
			}
			return { };
		}

		[[nodiscard]] auto field_text(const core::typography_field aField) -> QString
		{
			switch (aField) {
			case core::typography_field::family:
				return QStringLiteral("family");
			case core::typography_field::point_size:
				return QStringLiteral("size");
			case core::typography_field::weight:
				return QStringLiteral("weight");
			case core::typography_field::line_height:
				return QStringLiteral("line height");
			case core::typography_field::tab_width:
				return QStringLiteral("tab width");
			case core::typography_field::white_space:
				return QStringLiteral("white space");
			case core::typography_field::code_zoom:
				return QStringLiteral("zoom");
			}
			return { };
		}

		[[nodiscard]] auto reason_text(const core::typography_reason aReason)
			-> QString
		{
			switch (aReason) {
			case core::typography_reason::none:
				return QStringLiteral("is not in effect");
			case core::typography_reason::below_minimum:
				return QStringLiteral("is below the smallest allowed value");
			case core::typography_reason::above_maximum:
				return QStringLiteral("is above the largest allowed value");
			case core::typography_reason::malformed_value:
				return QStringLiteral("is not a usable value");
			}
			return { };
		}

		[[nodiscard]] auto theme_reason_text(const core::theme_import_reason aReason)
			-> QString
		{
			switch (aReason) {
			case core::theme_import_reason::document_too_large:
				return QStringLiteral("the document is larger than the safe limit");
			case core::theme_import_reason::malformed_document:
				return QStringLiteral("the document structure is malformed");
			case core::theme_import_reason::unsupported_version:
				return QStringLiteral("the theme version is unsupported");
			case core::theme_import_reason::unknown_field:
				return QStringLiteral("the document contains an unknown field");
			case core::theme_import_reason::duplicate_field:
				return QStringLiteral("the document repeats a field");
			case core::theme_import_reason::missing_field:
				return QStringLiteral("the document omits a required field");
			case core::theme_import_reason::external_reference:
				return QStringLiteral("external references are not allowed");
			case core::theme_import_reason::include_directive:
				return QStringLiteral("include directives are not allowed");
			case core::theme_import_reason::style_payload:
				return QStringLiteral("style payloads are not allowed");
			case core::theme_import_reason::executable_payload:
				return QStringLiteral("executable payloads are not allowed");
			case core::theme_import_reason::malformed_color:
				return QStringLiteral("a semantic colour is malformed");
			case core::theme_import_reason::unknown_base:
				return QStringLiteral("the base names no built-in theme");
			}
			return QStringLiteral("the document was refused");
		}

		[[nodiscard]] auto variant_text(const core::appearance_variant aVariant)
			-> QString
		{
			switch (aVariant) {
			case core::appearance_variant::light:
				return QStringLiteral("Light");
			case core::appearance_variant::dark:
				return QStringLiteral("Dark");
			case core::appearance_variant::high_contrast_light:
				return QStringLiteral("High contrast light");
			case core::appearance_variant::high_contrast_dark:
				return QStringLiteral("High contrast dark");
			}
			return { };
		}

		/// \brief Applies the core's own bounds to one numeric control.
		///
		/// \details Taken from `core::bounds_of` rather than restated, so the
		/// control cannot offer a value the validator would refuse. That is the
		/// reason nearly every rejection path here is unreachable by pointing at
		/// a control - and why the one that is reachable, the family, is the one
		/// the tests drive.
		void bind_bounds(QSpinBox& aBox, const core::typography_field aField)
		{
			const auto bounds = core::bounds_of(aField);
			aBox.setRange(
				static_cast<int>(bounds.Minimum), static_cast<int>(bounds.Maximum));
		}

		/// \brief Builds a font chooser showing the family in effect.
		///
		/// \details Editable on purpose. The installed families are what a reviewer
		/// chooses from, and the family is also the one typography value the core
		/// bounds by byte length rather than by value, so it is the only field a
		/// control can express illegally — a refusal this page must be able to
		/// provoke and show. The system font is not a pseudo-entry: a fresh
		/// installation already carries the platform's own family, and the card's
		/// Reset is what puts it back.
		[[nodiscard]] auto make_font_chooser(
			QWidget* const aParent,
			const QString& aAccessibleName,
			const QString& aObjectName) -> QComboBox*
		{
			auto* const chooser = new QComboBox{ aParent };
			chooser->setObjectName(aObjectName);
			chooser->setAccessibleName(aAccessibleName);
			chooser->setEditable(true);
			chooser->setInsertPolicy(QComboBox::NoInsert);
			// Bounded width: a family name is arbitrarily long, and a chooser sized
			// to the longest installed one would push the whole page sideways.
			chooser->setSizeAdjustPolicy(
				QComboBox::AdjustToMinimumContentsLengthWithIcon);
			chooser->setMinimumContentsLength(12);
			chooser->setMaxVisibleItems(20);
			chooser->addItems(QFontDatabase::families());
			return chooser;
		}
	} // namespace

	appearance_settings_panel::appearance_settings_panel(
		application::appearance_intent_dispatch aDispatch,
		QWidget* const                          aParent)
		: QWidget{ aParent }
		, Dispatch{ std::move(aDispatch) }
	{
		setAccessibleName(QStringLiteral("Appearance settings"));

		auto* const page = new QVBoxLayout{ this };
		page->setContentsMargins(0, 0, 0, 0);
		page->setSpacing(column_gap);

		auto* const lede = new QLabel{
			QStringLiteral("Changes apply as you make them and show in the review "
						   "window behind this one."),
			this
		};
		lede->setObjectName(QStringLiteral("appearanceLede"));
		lede->setWordWrap(true);
		lede->setForegroundRole(QPalette::PlaceholderText);
		page->addWidget(lede);

		// ── Theme. The tiles are the control; the six-way combo box is gone.
		auto theme = make_card(
			this, QStringLiteral("Theme"), QStringLiteral("appearanceThemeCard"));
		auto* const themeReset = make_header_button(
			theme.Frame,
			QStringLiteral("Reset"),
			QStringLiteral("Reset appearance mode"),
			QStringLiteral("appearanceThemeReset"));
		theme.Header->addWidget(themeReset);
		QObject::connect(themeReset, &QPushButton::clicked, this, [this] {
			dispatch_reset(application::appearance_reset_scope::mode);
		});
		ThemePreviews = new theme_preview_strip{ theme.Frame };
		ThemePreviews->set_dispatch([this](const core::appearance_mode aMode) {
			dispatch_mode(aMode);
		});
		theme.Body->addWidget(ThemePreviews);
		page->addWidget(theme.Frame);

		// ── Type. Eleven one-line rows, and the preview beside them.
		auto type = make_card(
			this, QStringLiteral("Type"), QStringLiteral("appearanceTypeCard"));
		auto* const typeReset = make_header_button(
			type.Frame,
			QStringLiteral("Reset"),
			QStringLiteral("Reset type"),
			QStringLiteral("appearanceTypeReset"));
		// Type is two durable groups, and the design gives the card one Reset, so
		// the one gesture resets both. Each still reaches the host as its own
		// existing scope; no new scope is invented.
		typeReset->setToolTip(QStringLiteral(
			"Puts the interface font and the code font back to the system "
			"defaults."));
		typeReset->setAccessibleDescription(typeReset->toolTip());
		type.Header->addWidget(typeReset);
		QObject::connect(typeReset, &QPushButton::clicked, this, [this] {
			dispatch_reset(application::appearance_reset_scope::interface_font);
			dispatch_reset(application::appearance_reset_scope::code_font);
		});

		auto* const typeColumns = new QHBoxLayout{ };
		typeColumns->setContentsMargins(0, 0, 0, 0);
		typeColumns->setSpacing(card_inset);
		auto* const typeRows = new QWidget{ type.Frame };
		typeRows->setObjectName(QStringLiteral("appearanceTypeRows"));
		auto* const rows = new QVBoxLayout{ typeRows };
		rows->setContentsMargins(0, 0, 0, 0);
		rows->setSpacing(0);

		UiFamily = make_font_chooser(
			typeRows,
			QStringLiteral("Interface font family"),
			QStringLiteral("appearanceUiFamily"));
		add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceUiFamilyRow"),
			QStringLiteral("Interface font"),
			QStringLiteral("Menus, drawers, headers. The system font unless chosen."),
			UiFamily);

		UiSize = new point_spin_box{ typeRows };
		UiSize->setAccessibleName(
			QStringLiteral("Interface font size, in points"));
		bind_bounds(*UiSize, core::typography_field::point_size);
		add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceUiSizeRow"),
			QStringLiteral("Interface size"),
			QStringLiteral(
				"The size of menus, drawers and headers, in points. Your desktop's "
				"own size unless changed."),
			UiSize);

		UiWeight = new QSpinBox{ typeRows };
		UiWeight->setAccessibleName(QStringLiteral("Interface font weight"));
		UiWeight->setSingleStep(100);
		bind_bounds(*UiWeight, core::typography_field::weight);
		add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceUiWeightRow"),
			QStringLiteral("Interface weight"),
			QStringLiteral("400 is regular; 500 and 600 are heavier."),
			UiWeight);

		UiLineHeight = new QSpinBox{ typeRows };
		UiLineHeight->setAccessibleName(
			QStringLiteral("Interface line height, percent"));
		UiLineHeight->setSuffix(QStringLiteral(" %"));
		UiLineHeight->setSingleStep(5);
		bind_bounds(*UiLineHeight, core::typography_field::line_height);
		add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceUiLineHeightRow"),
			QStringLiteral("Line height"),
			QStringLiteral("Of the font's natural height."),
			UiLineHeight);

		CodeFamily = make_font_chooser(
			typeRows,
			QStringLiteral("Code font family"),
			QStringLiteral("appearanceCodeFamily"));
		add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceCodeFamilyRow"),
			QStringLiteral("Code font"),
			QStringLiteral("Diffs, paths and identifiers."),
			CodeFamily);

		CodeSize = new point_spin_box{ typeRows };
		CodeSize->setAccessibleName(QStringLiteral("Code font size, in points"));
		bind_bounds(*CodeSize, core::typography_field::point_size);
		add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceCodeSizeRow"),
			QStringLiteral("Code size"),
			QStringLiteral("The size of every diff, path and identifier, in points."),
			CodeSize);

		// [[BPR-1336]]. The comparison's zoom, under the size it multiplies. One
		// item per step of the core's ladder, each saying the size it shows, so
		// the row reads what View > Zoom In and Zoom Out do without a second
		// number to reconcile.
		CodeZoom = new QComboBox{ typeRows };
		CodeZoom->setObjectName(QStringLiteral("appearanceCodeZoom"));
		CodeZoom->setAccessibleName(QStringLiteral("Diff zoom"));
		for (const auto zoom : core::code_zoom_steps) {
			CodeZoom->addItem(QStringLiteral("%1%").arg(zoom), zoom);
		}
		add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceCodeZoomRow"),
			QStringLiteral("Diff zoom"),
			QStringLiteral(
				"Scales the code in the comparison, its line numbers and Line "
				"details from Code size. View > Zoom In (%1), Zoom Out (%2) and "
				"Actual Size (%3) change it; drawers keep Code size.")
				.arg(QKeySequence{ QStringLiteral("Ctrl+=") }.toString(QKeySequence::NativeText),
					 QKeySequence{ QStringLiteral("Ctrl+-") }.toString(QKeySequence::NativeText),
					 QKeySequence{ QStringLiteral("Ctrl+Alt+0") }.toString(QKeySequence::NativeText)),
			CodeZoom);

		CodeWeight = new QSpinBox{ typeRows };
		CodeWeight->setAccessibleName(QStringLiteral("Code font weight"));
		CodeWeight->setSingleStep(100);
		bind_bounds(*CodeWeight, core::typography_field::weight);
		add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceCodeWeightRow"),
			QStringLiteral("Code weight"),
			QStringLiteral(
				"400 is regular; a heavier code font thickens every glyph in a "
				"diff."),
			CodeWeight);

		auto* const segment = new QWidget{ typeRows };
		segment->setObjectName(QStringLiteral("appearanceTabWidthSegment"));
		segment->setAccessibleName(QStringLiteral("Tab width, in spaces"));
		auto* const segmentRow = new QHBoxLayout{ segment };
		segmentRow->setContentsMargins(0, 0, 0, 0);
		segmentRow->setSpacing(0);
		for (auto index = std::size_t{ 0U }; index < tab_width_options.size();
			 ++index) {
			const auto width  = tab_width_options[index];
			const auto bounds = core::bounds_of(core::typography_field::tab_width);
			// Offered only while the core admits it. The three are the design's, and
			// the bound is the validator's own, so the segment cannot offer a width
			// the core would refuse.
			if (width < bounds.Minimum || width > bounds.Maximum) {
				continue;
			}
			auto* const option = new QToolButton{ segment };
			option->setObjectName(
				QStringLiteral("appearanceTabWidth%1").arg(width));
			option->setText(QString::number(width));
			option->setCheckable(true);
			option->setFocusPolicy(Qt::StrongFocus);
			option->setAccessibleName(
				QStringLiteral("Tab width %1").arg(width));
			QObject::connect(option, &QAbstractButton::clicked, this, [this, width] {
				choose_tab_width(width);
			});
			segmentRow->addWidget(option);
			TabWidths[index] = option;
		}
		const auto tabWidthRow = add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceTabWidthRow"),
			QStringLiteral("Tab width"),
			QStringLiteral("Spaces per tab stop."),
			segment);
		// Each segment is its own focusable control, so each one says the row's
		// consequence. Leaving it on the container that holds them would put the
		// sentence on the one thing in the row nobody can focus.
		for (auto* const option : TabWidths) {
			if (option != nullptr) {
				option->setAccessibleDescription(tabWidthRow.Glyph->consequence());
			}
		}

		// [[BPR-1107]]. When the panes draw white space, Sublime Text's way: a
		// space as a dot, a tab as a line to its stop. One choice at a time, in
		// the vocabulary's own order, each item carrying its ordinal.
		WhiteSpace = new QComboBox{ typeRows };
		WhiteSpace->setObjectName(QStringLiteral("appearanceWhiteSpace"));
		WhiteSpace->setAccessibleName(QStringLiteral("Draw white space"));
		for (const auto& [drawing, text] : std::array{
				 std::pair{ core::white_space_drawing::none, QStringLiteral("None") },
				 std::pair{ core::white_space_drawing::selection, QStringLiteral("Selection") },
				 std::pair{ core::white_space_drawing::leading, QStringLiteral("Leading") },
				 std::pair{ core::white_space_drawing::trailing, QStringLiteral("Trailing") },
				 std::pair{ core::white_space_drawing::boundary, QStringLiteral("Leading and trailing") },
				 std::pair{ core::white_space_drawing::all, QStringLiteral("All") },
			 }) {
			WhiteSpace->addItem(text, static_cast<int>(drawing));
		}
		add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceWhiteSpaceRow"),
			QStringLiteral("White space"),
			QStringLiteral(
				"When the review panes draw white space: a space as a dot, a tab "
				"as a line to its stop."),
			WhiteSpace);

		Ligatures = new QCheckBox{ typeRows };
		Ligatures->setAccessibleName(QStringLiteral("Ligatures"));
		add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceLigaturesRow"),
			QStringLiteral("Ligatures"),
			QStringLiteral("Off keeps every character visible in a diff."),
			Ligatures);

		SyntaxBold = new QCheckBox{ typeRows };
		SyntaxBold->setAccessibleName(QStringLiteral("Bold syntax emphasis"));
		add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceSyntaxBoldRow"),
			QStringLiteral("Bold syntax emphasis"),
			QStringLiteral("Keywords and types in bold."),
			SyntaxBold);

		SyntaxItalic = new QCheckBox{ typeRows };
		SyntaxItalic->setAccessibleName(QStringLiteral("Italic syntax emphasis"));
		add_row(
			*rows,
			typeRows,
			QStringLiteral("appearanceSyntaxItalicRow"),
			QStringLiteral("Italic syntax emphasis"),
			QStringLiteral("Comments and documentation in italic."),
			SyntaxItalic);
		rows->addStretch(1);

		Specimen = new type_preview{ type.Frame };
		typeColumns->addWidget(typeRows, 1);
		typeColumns->addWidget(Specimen, 0);
		type.Body->addLayout(typeColumns);
		page->addWidget(type.Frame);

		// ── Colours. The whole vocabulary, for the theme on screen.
		auto colours = make_card(
			this,
			QStringLiteral("Colours"),
			QStringLiteral("appearanceColoursCard"));
		ColoursCard  = colours.Frame;
		ColoursTitle = colours.Title;
		ImportButton = make_header_button(
			colours.Frame,
			QStringLiteral("Import…"),
			QStringLiteral("Import theme"),
			QStringLiteral("appearanceImportTheme"));
		ExportButton = make_header_button(
			colours.Frame,
			QStringLiteral("Export…"),
			QStringLiteral("Export saved theme"),
			QStringLiteral("appearanceExportTheme"));
		auto* const coloursReset = make_header_button(
			colours.Frame,
			QStringLiteral("Reset all"),
			QStringLiteral("Reset all colours"),
			QStringLiteral("appearanceColoursReset"));
		colours.Header->addWidget(ImportButton);
		colours.Header->addWidget(ExportButton);
		colours.Header->addWidget(coloursReset);
		QObject::connect(ImportButton, &QPushButton::clicked, this, [this] {
			select_theme_file();
		});
		QObject::connect(ExportButton, &QPushButton::clicked, this, [this] {
			select_theme_destination();
		});
		QObject::connect(coloursReset, &QPushButton::clicked, this, [this] {
			dispatch_reset(application::appearance_reset_scope::palette);
		});

		ColoursSubtitle = new QLabel{ colours.Frame };
		ColoursSubtitle->setObjectName(QStringLiteral("appearanceColoursSubtitle"));
		ColoursSubtitle->setWordWrap(true);
		ColoursSubtitle->setForegroundRole(QPalette::PlaceholderText);
		colours.Body->addWidget(ColoursSubtitle);
		Colours = new colour_role_list{ colours.Frame };
		Colours->set_dispatch(
			[this](
				const core::appearance_role  aRole,
				const core::appearance_color aColour) {
				edit_colour(aRole, aColour);
			});
		colours.Body->addWidget(Colours);
		page->addWidget(colours.Frame);

		// ── Dif. RFC-BPR-0031's B3 controls, in the new row language.
		auto dif = make_card(
			this, QStringLiteral("Dif"), QStringLiteral("appearanceDifCard"));
		auto* const difReset = make_header_button(
			dif.Frame,
			QStringLiteral("Reset"),
			QStringLiteral("Reset Dif preferences"),
			QStringLiteral("appearanceDifReset"));
		dif.Header->addWidget(difReset);
		QObject::connect(difReset, &QPushButton::clicked, this, [this] {
			dispatch_reset(application::appearance_reset_scope::dif);
		});
		auto* const difRows   = new QWidget{ dif.Frame };
		auto* const difColumn = new QVBoxLayout{ difRows };
		difColumn->setContentsMargins(0, 0, 0, 0);
		difColumn->setSpacing(0);

		DifVisibility = new QCheckBox{ difRows };
		// The accessible name RFC-BPR-0031's evidence names. The row's label is what
		// a sighted reviewer reads; this is what a screen reader says, and it stays
		// exactly as it was.
		DifVisibility->setAccessibleName(QStringLiteral("Show Dif mascot"));
		add_row(
			*difColumn,
			difRows,
			QStringLiteral("appearanceDifVisibilityRow"),
			QStringLiteral("Show Dif mascot"),
			QStringLiteral(
				"Hides Dif in the window's corner. Nothing else about the review "
				"changes."),
			DifVisibility);

		MotionSelector = new QComboBox{ difRows };
		MotionSelector->setAccessibleName(QStringLiteral("Dif motion"));
		MotionSelector->addItem(
			QStringLiteral("System"), static_cast<int>(core::motion_mode::system));
		MotionSelector->addItem(
			QStringLiteral("Full"), static_cast<int>(core::motion_mode::full));
		MotionSelector->addItem(
			QStringLiteral("Reduced"), static_cast<int>(core::motion_mode::reduced));
		add_row(
			*difColumn,
			difRows,
			QStringLiteral("appearanceDifMotionRow"),
			QStringLiteral("Dif motion"),
			QStringLiteral(
				"System follows your desktop's reduced-motion setting. Full "
				"overrides it."),
			MotionSelector);

		// A value row rather than a consequence row: the amendment keeps a row whose
		// second text is a value in the row, and what the system asked for is a
		// value.
		auto* const motionRow = new QWidget{ difRows };
		motionRow->setObjectName(QStringLiteral("appearanceDifMotionStatusRow"));
		motionRow->setMinimumHeight(row_height);
		auto* const motionLine = new QHBoxLayout{ motionRow };
		motionLine->setContentsMargins(0, 0, 0, 0);
		motionLine->setSpacing(column_gap);
		auto* const motionLabel = new QLabel{
			QStringLiteral("Motion now"), motionRow
		};
		MotionStatus = new QLabel{ motionRow };
		MotionStatus->setObjectName(QStringLiteral("appearanceDifMotionStatus"));
		MotionStatus->setWordWrap(true);
		MotionStatus->setAccessibleName(QStringLiteral("Dif motion status"));
		MotionStatus->setForegroundRole(QPalette::PlaceholderText);
		motionLine->addWidget(motionLabel, 0, Qt::AlignVCenter);
		motionLine->addWidget(MotionStatus, 1, Qt::AlignVCenter);
		difColumn->addWidget(make_hairline(difRows));
		difColumn->addWidget(motionRow);
		dif.Body->addWidget(difRows);
		page->addWidget(dif.Frame);

		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("appearanceStatus"));
		Status->setWordWrap(true);
		Status->setAccessibleName(QStringLiteral("Appearance settings status"));
		page->addWidget(Status);
		page->addStretch(1);

		QObject::connect(DifVisibility, &QCheckBox::toggled, this, [this](bool) {
			dispatch_dif();
		});
		QObject::connect(
			MotionSelector, &QComboBox::currentIndexChanged, this, [this](int) {
				dispatch_dif();
			});

		const auto onTypography = [this] { dispatch_typography(); };
		for (auto* const chooser : { UiFamily, CodeFamily }) {
			QObject::connect(
				chooser->lineEdit(),
				&QLineEdit::editingFinished,
				this,
				onTypography);
			QObject::connect(
				chooser, &QComboBox::activated, this, [onTypography](int) {
					onTypography();
				});
		}
		for (auto* const box :
			 { UiSize, UiWeight, UiLineHeight, CodeSize, CodeWeight }) {
			QObject::connect(
				box, &QSpinBox::valueChanged, this, [onTypography](int) {
					onTypography();
				});
		}
		for (auto* const box : { Ligatures, SyntaxBold, SyntaxItalic }) {
			QObject::connect(
				box, &QCheckBox::toggled, this, [onTypography](bool) {
					onTypography();
				});
		}
		QObject::connect(
			WhiteSpace, &QComboBox::currentIndexChanged, this, [onTypography](int) {
				onTypography();
			});
		QObject::connect(
			CodeZoom, &QComboBox::currentIndexChanged, this, [onTypography](int) {
				onTypography();
			});

		// Nothing is on screen yet and no variant has been named, so there is no
		// palette to show. The first adoption supplies both.
		CommittedPalette = core::appearance_palette{ };
		show_adopted();
	}

	void appearance_settings_panel::set_resolved_variant(
		const core::appearance_variant aVariant)
	{
		ResolvedVariant = aVariant;
		ThemePreviews->set_system_variant(aVariant);
		Specimen->set_theme_name(variant_text(aVariant));
		refresh_colours();
		refresh_preview_bar();
	}

	auto appearance_settings_panel::resolved_variant() const noexcept
		-> const std::optional<core::appearance_variant>&
	{
		return ResolvedVariant;
	}

	void appearance_settings_panel::set_customised_counts(
		std::array<std::size_t, core::appearance_variant_count> aCounts)
	{
		ThemePreviews->set_customised_counts(aCounts);
	}

	void appearance_settings_panel::set_theme_palettes(
		std::array<core::appearance_palette, core::appearance_variant_count>
			aPalettes)
	{
		ThemePreviews->set_theme_palettes(std::move(aPalettes));
	}

	void appearance_settings_panel::set_code_font(QFont aFont)
	{
		ThemePreviews->set_code_font(aFont);
		Colours->set_value_font(std::move(aFont));
	}

	void appearance_settings_panel::adopt_appearance(appearance_profile& aProfile)
	{
		set_resolved_variant(aProfile.variant());
		auto palettes
			= std::array<core::appearance_palette, core::appearance_variant_count>{ };
		auto counts = std::array<std::size_t, core::appearance_variant_count>{ };
		for (auto index = std::size_t{ 0U }; index < palettes.size(); ++index) {
			const auto variant = static_cast<core::appearance_variant>(
				static_cast<std::uint8_t>(index));
			palettes[index] = aProfile.palette_for(variant);
			counts[index]   = aProfile.customised_roles(variant);
		}
		set_theme_palettes(std::move(palettes));
		set_customised_counts(counts);
		set_code_font(aProfile.code_font());
		ThemePreviews->set_white_space(aProfile.typography().Code.WhiteSpace);
		Specimen->adopt_appearance(aProfile);
	}

	void appearance_settings_panel::set_colour_picker(colour_picker aPicker)
	{
		Colours->set_picker(std::move(aPicker));
	}

	void appearance_settings_panel::choose_mode(const core::appearance_mode aMode)
	{
		ThemePreviews->set_current(aMode);
		dispatch_mode(aMode);
	}

	auto appearance_settings_panel::theme_text() const -> QString
	{
		return ResolvedVariant.has_value() ? variant_text(*ResolvedVariant)
										   : QStringLiteral("the theme on screen");
	}

	void appearance_settings_panel::dispatch_dif()
	{
		if (Echoing || MotionSelector->currentIndex() < 0 || !Dispatch) {
			return;
		}
		Dispatch(application::apply_dif_preferences_intent{
			.Preferences = {
				.Visibility = DifVisibility->isChecked()
					? core::dif_visibility::shown
					: core::dif_visibility::hidden,
				.Motion     = static_cast<core::motion_mode>(
					MotionSelector->currentData().toInt()),
			} });
	}

	void appearance_settings_panel::adopt_motion(
		const core::dif_resolution aResolution)
	{
		MotionResolution = aResolution;
		QString text;
		if (aResolution.Preferences.Visibility == core::dif_visibility::hidden) {
			text = QStringLiteral("Dif is hidden. Motion is stopped.");
		} else if (aResolution.Preferences.Motion == core::motion_mode::full) {
			text = QStringLiteral(
				"Full motion is selected, overriding the system preference.");
		} else if (aResolution.Preferences.Motion == core::motion_mode::reduced) {
			text = QStringLiteral(
				"Reduced motion is selected. Dif remains still.");
		} else {
			switch (aResolution.Observation) {
			case core::motion_observation::full:
				text = QStringLiteral("System allows full motion.");
				break;
			case core::motion_observation::reduced:
				text = QStringLiteral(
					"System requests reduced motion. Dif remains still.");
				break;
			case core::motion_observation::unsupported:
				text = QStringLiteral(
					"System motion observation is unsupported. Dif remains still; "
					"select Full to override.");
				break;
			case core::motion_observation::failed:
				text = QStringLiteral(
					"System motion observation failed. Dif remains still; select "
					"Full to override.");
				break;
			case core::motion_observation::unknown:
				text = QStringLiteral(
					"System motion preference is unknown. Dif remains still; "
					"select Full to override.");
				break;
			}
		}
		MotionStatus->setText(text);
	}

	auto appearance_settings_panel::dif_visibility_switch() const noexcept
		-> QCheckBox*
	{
		return DifVisibility;
	}

	auto appearance_settings_panel::motion_selector() const noexcept -> QComboBox*
	{
		return MotionSelector;
	}

	auto appearance_settings_panel::white_space_selector() const noexcept -> QComboBox*
	{
		return WhiteSpace;
	}

	void appearance_settings_panel::set_dispatch(
		application::appearance_intent_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void appearance_settings_panel::report_persistence_unavailable()
	{
		PersistenceAvailable = false;
		Status->setText(with_persistence_status(QString{ }));
	}

	void appearance_settings_panel::adopt(
		const application::appearance_settings& aSettings)
	{
		// A convenience for a caller with no resolved palette to offer. The
		// composition always uses the three-argument form, which carries the
		// palette actually on screen. RFC-BPR-0034 forbids this adapter deciding
		// which variant applies, so the theme shown here is the one the
		// composition last supplied through `set_resolved_variant`. Light is a
		// stated default for a caller that has named no theme at all; it is not
		// a resolution, it consults no mode and no observation, and it never
		// reaches the durable record.
		adopt(
			aSettings,
			application::customisation_of(
				aSettings,
				ResolvedVariant.value_or(core::appearance_variant::light))
				.Palette,
			std::nullopt);
	}

	void appearance_settings_panel::adopt(
		const application::appearance_settings& aSettings,
		const core::appearance_palette&         aResolvedPalette)
	{
		adopt(aSettings, aResolvedPalette, std::nullopt);
	}

	void appearance_settings_panel::adopt(
		const application::appearance_settings& aSettings,
		const core::appearance_palette&         aResolvedPalette,
		std::optional<core::appearance_palette> aActivePreview)
	{
		PendingPreview.reset();
		AdoptedSettings  = aSettings;
		CommittedPalette = aResolvedPalette;
		PreviewPalette   = std::move(aActivePreview);
		++Statistics.Adoptions;
		show_adopted();
		Status->setText(with_persistence_status(
			PreviewPalette.has_value()
				? QStringLiteral("Appearance settings are in effect; the palette "
								 "preview remains unsaved.")
				: QStringLiteral("Appearance settings are in effect.")));
	}

	void appearance_settings_panel::preview_import(
		const core::appearance_palette& aPalette)
	{
		PendingPreview.reset();
		// [[BPR-1479]]. Roles an older theme omits arrive filled from the
		// theme's own base, so the preview is exactly what Keep changes saves.
		PreviewPalette = aPalette;
		refresh_colours();
		refresh_preview_bar();
		// Names the theme it previews into, because it previews into exactly one
		// and keeping the changes changes only that one.
		Status->setText(with_persistence_status(
			QStringLiteral("Imported theme is previewed into %1 only; Keep changes "
						   "saves it.")
				.arg(theme_text())));
	}

	void appearance_settings_panel::report(
		const std::span<const core::theme_import_rejection> aRejections)
	{
		++Statistics.Rejections;
		auto text = QStringLiteral(
			"Theme import refused; the committed palette is unchanged:");
		for (const auto& rejection : aRejections) {
			text += QStringLiteral("\n· Line %1, %2: %3")
						.arg(static_cast<qulonglong>(rejection.Line))
						.arg(QString::fromStdString(rejection.Subject))
						.arg(theme_reason_text(rejection.Reason));
		}
		Status->setText(with_persistence_status(std::move(text)));
	}

	void appearance_settings_panel::report(
		const std::span<const application::appearance_contrast_rejection>
			aRejections)
	{
		++Statistics.Rejections;
		auto text = QStringLiteral(
			"Palette refused; the committed palette is unchanged:");
		for (const auto& rejection : aRejections) {
			const auto foreground
				= core::stable_name(rejection.Finding.Pair.Foreground);
			const auto background
				= core::stable_name(rejection.Finding.Pair.Background);
			text += QStringLiteral("\n· %1: %2 on %3 is %4:1; %5:1 is required")
						.arg(variant_text(rejection.Variant))
						.arg(QString::fromUtf8(
							foreground.data(),
							static_cast<qsizetype>(foreground.size())))
						.arg(QString::fromUtf8(
							background.data(),
							static_cast<qsizetype>(background.size())))
						.arg(
							static_cast<double>(rejection.Finding.RatioHundredths)
								/ 100.0,
							0,
							'f',
							2)
						.arg(
							static_cast<double>(
								rejection.Finding.MinimumHundredths)
								/ 100.0,
							0,
							'f',
							2);
		}
		Status->setText(with_persistence_status(std::move(text)));
	}

	void appearance_settings_panel::request_theme_export(const QString& aPath)
	{
		if (!aPath.isEmpty() && Dispatch) {
			Dispatch(
				application::export_appearance_theme_intent{ aPath.toStdString() });
		}
	}

	void appearance_settings_panel::report_theme_export(const bool aSucceeded)
	{
		// Names the theme it wrote. An export takes the colours of exactly one
		// theme, so a message that did not say which left the reviewer to assume
		// it had written all of them.
		Status->setText(with_persistence_status(
			aSucceeded
				? QStringLiteral("Saved theme exported. %1 was written; unsaved "
								 "previews are excluded.")
					  .arg(theme_text())
				: QStringLiteral("Theme export failed. Choose a writable regular "
								 "file and try again.")));
	}

	void appearance_settings_panel::report_theme_document_unavailable()
	{
		++Statistics.Rejections;
		Status->setText(with_persistence_status(QStringLiteral(
			"Theme import failed because the selected local file could not be read; "
			"the committed palette is unchanged.")));
	}

	void appearance_settings_panel::request_theme_import(const QString& aPath)
	{
		if (aPath.isEmpty()) {
			return;
		}
		PendingPreview.reset();
		++Statistics.ThemeImports;
		if (Dispatch) {
			Dispatch(application::import_appearance_theme_intent{
				.Path = aPath.toStdString(),
			});
		}
	}

	void appearance_settings_panel::report(
		const std::span<const core::typography_rejection> aRejections)
	{
		++Statistics.Rejections;
		// Restored first. A reviewer who is told their value was refused must
		// be looking at the value that is actually in effect, not at the one
		// that was not adopted.
		show_adopted();
		if (aRejections.empty()) {
			Status->setText(with_persistence_status(
				QStringLiteral("Appearance settings were refused.")));
			return;
		}
		auto text = QStringLiteral("Refused, and nothing changed:");
		for (const auto& rejection : aRejections) {
			text += QStringLiteral("\n· %1 %2 %3")
						.arg(
							target_text(rejection.Target),
							field_text(rejection.Field),
							reason_text(rejection.Reason));
		}
		Status->setText(with_persistence_status(std::move(text)));
	}

	auto appearance_settings_panel::theme_previews() const noexcept
		-> theme_preview_strip*
	{
		return ThemePreviews;
	}

	auto appearance_settings_panel::specimen() const noexcept -> type_preview*
	{
		return Specimen;
	}

	auto appearance_settings_panel::colours() const noexcept -> colour_role_list*
	{
		return Colours;
	}

	auto appearance_settings_panel::colours_title() const noexcept -> QLabel*
	{
		return ColoursTitle;
	}

	auto appearance_settings_panel::colours_subtitle() const noexcept -> QLabel*
	{
		return ColoursSubtitle;
	}

	auto appearance_settings_panel::ui_font_chooser() const noexcept -> QComboBox*
	{
		return UiFamily;
	}

	auto appearance_settings_panel::code_font_chooser() const noexcept -> QComboBox*
	{
		return CodeFamily;
	}

	auto appearance_settings_panel::ui_size_editor() const noexcept -> QSpinBox*
	{
		return UiSize;
	}

	auto appearance_settings_panel::ui_weight_editor() const noexcept -> QSpinBox*
	{
		return UiWeight;
	}

	auto appearance_settings_panel::ui_line_height_editor() const noexcept
		-> QSpinBox*
	{
		return UiLineHeight;
	}

	auto appearance_settings_panel::code_zoom_selector() const noexcept -> QComboBox*
	{
		return CodeZoom;
	}

	auto appearance_settings_panel::code_size_editor() const noexcept -> QSpinBox*
	{
		return CodeSize;
	}

	auto appearance_settings_panel::code_weight_editor() const noexcept -> QSpinBox*
	{
		return CodeWeight;
	}

	auto appearance_settings_panel::tab_width_option(
		const std::uint32_t aWidth) const -> QAbstractButton*
	{
		for (auto index = std::size_t{ 0U }; index < tab_width_options.size();
			 ++index) {
			if (tab_width_options[index] == aWidth) {
				return TabWidths[index];
			}
		}
		return nullptr;
	}

	auto appearance_settings_panel::tab_width() const -> std::uint32_t
	{
		for (auto index = std::size_t{ 0U }; index < TabWidths.size(); ++index) {
			if (TabWidths[index] != nullptr && TabWidths[index]->isChecked()) {
				return tab_width_options[index];
			}
		}
		// No segment chosen, which happens exactly when the adopted record carries a
		// legal width the design does not offer. The record's own width is then what
		// is in effect, and an unrelated font gesture must not quietly change it.
		return AdoptedSettings.Typography.Code.TabWidth;
	}

	auto appearance_settings_panel::ligatures_switch() const noexcept -> QCheckBox*
	{
		return Ligatures;
	}

	auto appearance_settings_panel::syntax_bold_switch() const noexcept -> QCheckBox*
	{
		return SyntaxBold;
	}

	auto appearance_settings_panel::syntax_italic_switch() const noexcept
		-> QCheckBox*
	{
		return SyntaxItalic;
	}

	auto appearance_settings_panel::import_button() const noexcept -> QPushButton*
	{
		return ImportButton;
	}

	auto appearance_settings_panel::export_button() const noexcept -> QPushButton*
	{
		return ExportButton;
	}

	auto appearance_settings_panel::preview_bar() const noexcept -> QWidget*
	{
		return PreviewBar;
	}

	auto appearance_settings_panel::preview_bar_text() const -> QString
	{
		return PreviewBarLabel == nullptr ? QString{ } : PreviewBarLabel->text();
	}

	auto appearance_settings_panel::keep_changes_button() const noexcept
		-> QPushButton*
	{
		return KeepChanges;
	}

	auto appearance_settings_panel::cancel_changes_button() const noexcept
		-> QPushButton*
	{
		return CancelChanges;
	}

	auto appearance_settings_panel::preview_palette() const noexcept
		-> const std::optional<core::appearance_palette>&
	{
		return PreviewPalette;
	}

	auto appearance_settings_panel::status_text() const -> QString
	{
		return Status->text();
	}

	auto appearance_settings_panel::adopted() const noexcept
		-> const application::appearance_settings&
	{
		return AdoptedSettings;
	}

	auto appearance_settings_panel::statistics() const noexcept
		-> appearance_settings_panel_statistics
	{
		return Statistics;
	}

	void appearance_settings_panel::dispatch_mode(const core::appearance_mode aMode)
	{
		if (Echoing) {
			++Statistics.SuppressedEchoes;
			return;
		}
		if (!application::known(aMode)) {
			return;
		}
		++Statistics.ModeGestures;
		if (Dispatch) {
			Dispatch(application::select_appearance_mode_intent{ .Mode = aMode });
		}
	}

	void appearance_settings_panel::dispatch_typography()
	{
		if (Echoing) {
			++Statistics.SuppressedEchoes;
			return;
		}
		++Statistics.FontGestures;
		if (Dispatch) {
			Dispatch(application::apply_typography_intent{
				.Typography = composed_typography(),
			});
		}
	}

	void appearance_settings_panel::choose_tab_width(const std::uint32_t aWidth)
	{
		for (auto index = std::size_t{ 0U }; index < TabWidths.size(); ++index) {
			if (TabWidths[index] != nullptr) {
				TabWidths[index]->setChecked(tab_width_options[index] == aWidth);
			}
		}
		if (aWidth != AdoptedSettings.Typography.Code.TabWidth) {
			dispatch_typography();
		}
	}

	void appearance_settings_panel::dispatch_reset(
		const application::appearance_reset_scope aScope)
	{
		PendingPreview.reset();
		if (Dispatch) {
			Dispatch(application::reset_appearance_intent{ aScope });
		}
	}

	void appearance_settings_panel::edit_colour(
		const core::appearance_role  aRole,
		const core::appearance_color aColour)
	{
		auto palette                   = PreviewPalette.value_or(CommittedPalette);
		palette[core::index_of(aRole)] = aColour;
		if (palette == CommittedPalette) {
			// Back to the colours the host last adopted, which is what a cancelled
			// picker reports. There is nothing left to preview, so the preview ends
			// rather than standing with no difference in it.
			dispatch_palette_cancel();
			return;
		}
		PreviewPalette = palette;
		PendingPreview = palette;
		refresh_colours();
		refresh_preview_bar();
		Status->setText(with_persistence_status(
			QStringLiteral("Colours are previewed only; Keep changes saves them to "
						   "%1.")
				.arg(theme_text())));
		offer_preview();
	}

	void appearance_settings_panel::offer_preview()
	{
		if (PreviewDeliveryQueued) {
			return;
		}
		PreviewDeliveryQueued = true;
		// One widget-bound delivery consumes the latest candidate in this event
		// burst, so dragging a picker across a gradient offers the colour it landed
		// on rather than every colour it crossed.
		QMetaObject::invokeMethod(
			this,
			[this] {
				PreviewDeliveryQueued = false;
				auto pending          = std::exchange(PendingPreview, std::nullopt);
				if (pending && Dispatch) {
					++Statistics.PalettePreviews;
					Dispatch(application::preview_appearance_palette_intent{
						.Palette = std::move(*pending),
					});
				}
			},
			Qt::QueuedConnection);
	}

	void appearance_settings_panel::dispatch_palette_commit()
	{
		if (!PreviewPalette.has_value()) {
			return;
		}
		PendingPreview.reset();
		++Statistics.PaletteCommits;
		if (Dispatch) {
			Dispatch(application::commit_appearance_palette_intent{
				.Palette = *PreviewPalette,
			});
		}
	}

	void appearance_settings_panel::cancel_preview()
	{
		dispatch_palette_cancel();
	}

	void appearance_settings_panel::dispatch_palette_cancel()
	{
		PendingPreview.reset();
		PreviewPalette.reset();
		++Statistics.PaletteCancels;
		refresh_colours();
		refresh_preview_bar();
		Status->setText(with_persistence_status(
			QStringLiteral("Preview cancelled; the committed palette is restored.")));
		if (Dispatch) {
			Dispatch(application::cancel_appearance_palette_preview_intent{ });
		}
	}

	void appearance_settings_panel::select_theme_file()
	{
		request_theme_import(QFileDialog::getOpenFileName(
			this,
			QStringLiteral("Import appearance theme"),
			{ },
			QStringLiteral(
				"BetterPullReview themes (*.bprtheme *.theme);;All files (*)")));
	}

	void appearance_settings_panel::select_theme_destination()
	{
		request_theme_export(QFileDialog::getSaveFileName(
			this,
			QStringLiteral("Export saved theme"),
			QStringLiteral("theme.bprtheme"),
			QStringLiteral("Theme files (*.bprtheme)")));
	}

	/// \brief [[BPR-1336]]. The zoom the row shows, or the record's own when
	/// the record's zoom is between steps and so has no item.
	auto appearance_settings_panel::code_zoom() const -> std::uint32_t
	{
		const auto current = CodeZoom->currentData();
		return current.isValid() ? current.toUInt() : AdoptedSettings.Typography.Code.ZoomPercent;
	}

	/// \brief [[BPR-1336]]. Selects the record's zoom and says what each step shows.
	void appearance_settings_panel::show_code_zoom(const core::code_font_settings& aCode)
	{
		for (auto index = 0; index < CodeZoom->count(); ++index) {
			auto step        = aCode;
			step.ZoomPercent = CodeZoom->itemData(index).toUInt();
			CodeZoom->setItemText(index, QStringLiteral("%1% (%2 pt)").arg(step.ZoomPercent).arg(point_size_text(core::zoomed_point_size(step))));
		}
		CodeZoom->setCurrentIndex(CodeZoom->findData(aCode.ZoomPercent));
	}

	auto appearance_settings_panel::composed_typography() const
		-> core::typography_settings
	{
		return core::typography_settings{
			.Ui = {
				.Font = {
					.Family               = UiFamily->currentText().toStdString(),
					.PointSizeCentipoints = static_cast<std::uint32_t>(UiSize->value()),
					.Weight               = static_cast<std::uint32_t>(UiWeight->value()),
				},
				.LineHeightPercent = static_cast<std::uint32_t>(UiLineHeight->value()),
			},
			.Code = {
				.Font = {
					.Family               = CodeFamily->currentText().toStdString(),
					.PointSizeCentipoints = static_cast<std::uint32_t>(CodeSize->value()),
					.Weight               = static_cast<std::uint32_t>(CodeWeight->value()),
				},
				.TabWidth     = tab_width(),
				.Ligatures    = Ligatures->isChecked(),
				.SyntaxBold   = SyntaxBold->isChecked(),
				.SyntaxItalic = SyntaxItalic->isChecked(),
				.WhiteSpace   = static_cast<core::white_space_drawing>(WhiteSpace->currentData().toInt()),
				.ZoomPercent  = code_zoom(),
			},
		};
	}

	void appearance_settings_panel::show_adopted()
	{
		Echoing            = true;
		const auto& record = AdoptedSettings;
		ThemePreviews->set_current(record.Mode);
		DifVisibility->setChecked(
			record.Dif.Visibility == core::dif_visibility::shown);
		MotionSelector->setCurrentIndex(static_cast<int>(record.Dif.Motion));
		adopt_motion(core::resolve_dif(record.Dif, MotionResolution.Observation));
		UiFamily->setCurrentText(
			QString::fromStdString(record.Typography.Ui.Font.Family));
		UiSize->setValue(
			static_cast<int>(record.Typography.Ui.Font.PointSizeCentipoints));
		UiWeight->setValue(static_cast<int>(record.Typography.Ui.Font.Weight));
		UiLineHeight->setValue(
			static_cast<int>(record.Typography.Ui.LineHeightPercent));
		CodeFamily->setCurrentText(
			QString::fromStdString(record.Typography.Code.Font.Family));
		CodeSize->setValue(
			static_cast<int>(record.Typography.Code.Font.PointSizeCentipoints));
		CodeWeight->setValue(static_cast<int>(record.Typography.Code.Font.Weight));
		for (auto index = std::size_t{ 0U }; index < TabWidths.size(); ++index) {
			if (TabWidths[index] != nullptr) {
				TabWidths[index]->setChecked(
					tab_width_options[index] == record.Typography.Code.TabWidth);
			}
		}
		WhiteSpace->setCurrentIndex(WhiteSpace->findData(static_cast<int>(record.Typography.Code.WhiteSpace)));
		show_code_zoom(record.Typography.Code);
		Ligatures->setChecked(record.Typography.Code.Ligatures);
		SyntaxBold->setChecked(record.Typography.Code.SyntaxBold);
		SyntaxItalic->setChecked(record.Typography.Code.SyntaxItalic);
		Echoing = false;
		refresh_colours();
		refresh_preview_bar();
	}

	void appearance_settings_panel::refresh_colours()
	{
		Colours->set_theme_colours(CommittedPalette);
		Colours->set_previewed_colours(PreviewPalette);
		ColoursTitle->setText(
			QStringLiteral("Colours for %1").arg(theme_text()));
		ColoursSubtitle->setText(
			QStringLiteral("Edits change %1 only. Each theme keeps its own "
						   "colours.")
				.arg(theme_text()));
		ColoursCard->setAccessibleName(ColoursTitle->text());
	}

	void appearance_settings_panel::refresh_preview_bar()
	{
		if (!PreviewPalette.has_value()) {
			if (PreviewBar == nullptr) {
				return;
			}
			// Removed rather than hidden: RFC-BPR-0034 says the bar exists only
			// while something is previewed, and a hidden widget is still in the
			// accessibility tree. Detaching it first makes it absent from this
			// panel at once, and the deferred delete is what keeps a click on
			// Cancel from destroying the button it came from.
			PreviewBar->hide();
			PreviewBar->setParent(nullptr);
			PreviewBar->deleteLater();
			PreviewBar      = nullptr;
			PreviewBarLabel = nullptr;
			KeepChanges     = nullptr;
			CancelChanges   = nullptr;
			return;
		}

		if (PreviewBar == nullptr) {
			PreviewBar = new QWidget{ ColoursCard };
			PreviewBar->setObjectName(QStringLiteral("appearancePreviewBar"));
			auto* const column = new QVBoxLayout{ PreviewBar };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(0);
			column->addWidget(make_hairline(PreviewBar));
			auto* const foot = new section_band{ PreviewBar };
			foot->setObjectName(QStringLiteral("appearancePreviewBarFoot"));
			auto* const line = new QHBoxLayout{ foot };
			line->setContentsMargins(card_inset, row_inset, card_inset, row_inset);
			line->setSpacing(column_gap / 2);
			PreviewBarLabel = new QLabel{ foot };
			PreviewBarLabel->setWordWrap(true);
			CancelChanges = make_header_button(
				foot,
				QStringLiteral("Cancel"),
				QStringLiteral("Cancel colour changes"),
				QStringLiteral("appearancePreviewCancel"));
			KeepChanges = new QPushButton{ QStringLiteral("Keep changes"), foot };
			KeepChanges->setObjectName(QStringLiteral("appearancePreviewKeep"));
			KeepChanges->setAccessibleName(QStringLiteral("Keep changes"));
			KeepChanges->setAutoDefault(false);
			KeepChanges->setFocusPolicy(Qt::StrongFocus);
			line->addWidget(PreviewBarLabel, 1);
			line->addWidget(CancelChanges);
			line->addWidget(KeepChanges);
			column->addWidget(foot);
			QObject::connect(CancelChanges, &QPushButton::clicked, this, [this] {
				dispatch_palette_cancel();
			});
			QObject::connect(KeepChanges, &QPushButton::clicked, this, [this] {
				dispatch_palette_commit();
			});
			if (auto* const card = qobject_cast<QVBoxLayout*>(
					ColoursCard->layout());
				card != nullptr) {
				card->addWidget(PreviewBar);
			}
		}

		auto changes = std::size_t{ 0U };
		for (auto index = std::size_t{ 0U }; index < core::appearance_role_count;
			 ++index) {
			changes += (*PreviewPalette)[index] != CommittedPalette[index] ? 1U : 0U;
		}
		const auto sentence = [this, changes]() -> QString {
			if (changes == 1U) {
				return QStringLiteral(
						   "Previewing 1 colour change to %1 in the review window.")
					.arg(theme_text());
			}
			if (changes > 1U) {
				return QStringLiteral(
						   "Previewing %1 colour changes to %2 in the review window.")
					.arg(static_cast<qulonglong>(changes))
					.arg(theme_text());
			}
			// An imported theme can be equal to the one on screen. Saying it is
			// previewing nothing is the truth, and it keeps the bar's presence
			// meaning exactly "something is previewed".
			return QStringLiteral(
					   "Previewing %1 in the review window; no colour differs.")
				.arg(theme_text());
		}();
		PreviewBarLabel->setText(sentence);
		PreviewBar->setAccessibleName(sentence);
		KeepChanges->setAccessibleDescription(
			QStringLiteral("Saves the previewed colours to %1.").arg(theme_text()));
		CancelChanges->setAccessibleDescription(QStringLiteral(
			"Puts the previewed colours back to the ones in effect."));
	}

	auto appearance_settings_panel::with_persistence_status(QString aStatus) const
		-> QString
	{
		if (PersistenceAvailable) {
			return aStatus;
		}
		if (!aStatus.isEmpty()) {
			aStatus += QLatin1Char('\n');
		}
		aStatus += QStringLiteral(
			"Settings storage is unavailable. Appearance changes apply only "
			"for this session; if storage remains unavailable, the next launch "
			"uses System defaults.");
		return aStatus;
	}
} // namespace bpr::qt
