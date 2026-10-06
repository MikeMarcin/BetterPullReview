/// \file
/// \brief Proves RFC-BPR-0034's rebuilt Appearance page translates and owns nothing.
///
/// \details Criteria C3 and C5 as the page composes them: twelve Type rows that are
/// one line each, every consequence reachable by hover, by keyboard focus and as the
/// glyph's accessible description, sizes shown in points, tab width as a 2, 4, 8
/// segment, switches for ligatures and the two emphases, each card's header Reset
/// dispatching its existing scope, the Colours card titled for the theme on screen,
/// the one preview bar that exists only while something is previewed — and the
/// absence of the retired controls from both the widget tree and the accessibility
/// tree.

#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/appearance_role_display.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/colour_role_list.hpp"
#include "bpr/qt/type_preview.hpp"

#include <QAbstractButton>
#include <QAccessible>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QToolButton>
#include <QToolTip>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;
	using role = core::appearance_role;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		QApplication::processEvents();
		QApplication::sendPostedEvents();
		QApplication::processEvents();
	}

	/// \brief Collects the intents a surface dispatched.
	class recorder final
	{
	public:
		[[nodiscard]] auto sink() -> application::appearance_intent_dispatch
		{
			return [this](application::appearance_intent aIntent) {
				Intents.push_back(std::move(aIntent));
			};
		}

		[[nodiscard]] auto count() const noexcept -> std::size_t
		{
			return Intents.size();
		}

		[[nodiscard]] auto last_mode() const
			-> std::optional<core::appearance_mode>
		{
			const auto* const mode = last<application::select_appearance_mode_intent>();
			return mode == nullptr ? std::nullopt : std::optional{ mode->Mode };
		}

		[[nodiscard]] auto last_typography() const
			-> std::optional<core::typography_settings>
		{
			const auto* const offer = last<application::apply_typography_intent>();
			return offer == nullptr ? std::nullopt
									: std::optional{ offer->Typography };
		}

		template <typename Intent>
		[[nodiscard]] auto last() const -> const Intent*
		{
			for (auto index = Intents.size(); index > 0U; --index) {
				if (const auto* const intent
					= std::get_if<Intent>(&Intents[index - 1U])) {
					return intent;
				}
			}
			return nullptr;
		}

		template <typename Intent>
		[[nodiscard]] auto count_of() const -> std::size_t
		{
			auto found = std::size_t{ 0U };
			for (const auto& intent : Intents) {
				found += std::holds_alternative<Intent>(intent) ? 1U : 0U;
			}
			return found;
		}

		[[nodiscard]] auto reset_scopes() const
			-> std::vector<application::appearance_reset_scope>
		{
			auto scopes = std::vector<application::appearance_reset_scope>{ };
			for (const auto& intent : Intents) {
				if (const auto* const reset
					= std::get_if<application::reset_appearance_intent>(&intent)) {
					scopes.push_back(reset->Scope);
				}
			}
			return scopes;
		}

		void clear()
		{
			Intents.clear();
		}

	private:
		std::vector<application::appearance_intent> Intents;
	};

	[[nodiscard]] auto legal_settings() -> application::appearance_settings
	{
		auto settings = application::appearance_settings{ };
		settings.Mode = core::appearance_mode::light;
		// Light customised to its own built-in colours: a stored slot, so the
		// panel has a palette to show, carrying no override of anything.
		application::customisation_of(settings, core::appearance_variant::light)
			= application::appearance_palette_customisation{
				  .Palette = qt::builtin_appearance_palette(
					  core::appearance_variant::light),
				  .Stored = true,
			  };
		settings.Typography = qt::default_typography_settings();
		return settings;
	}

	/// \brief A picker a course can drive, standing in for the platform panel.
	[[nodiscard]] auto picker_answering(
		std::vector<QColor>   aMoves,
		std::optional<QColor> aAccepted) -> qt::colour_picker
	{
		return [moves = std::move(aMoves), accepted = std::move(aAccepted)](
				   const qt::colour_pick_request& aRequest)
				   -> std::optional<QColor> {
			for (const auto& moved : moves) {
				aRequest.Changed(moved);
			}
			return accepted;
		};
	}

	void show(qt::appearance_settings_panel& aPanel)
	{
		aPanel.resize(560, 900);
		aPanel.show();
		settle();
	}

	[[nodiscard]] auto button_named(
		const qt::appearance_settings_panel& aPanel,
		const QString&                       aAccessibleName) -> QPushButton*
	{
		for (auto* const button : aPanel.findChildren<QPushButton*>()) {
			if (button->accessibleName() == aAccessibleName) {
				return button;
			}
		}
		return nullptr;
	}

	/// \brief One Type row, named by the object name the page gives it.
	struct row_under_test
	{
		QString  Object;
		QString  Label;
		QWidget* Control = nullptr;
	};

	[[nodiscard]] auto type_rows(const qt::appearance_settings_panel& aPanel)
		-> std::vector<row_under_test>
	{
		return {
			{ QStringLiteral("appearanceUiFamilyRow"),
			  QStringLiteral("Interface font"),
			  aPanel.ui_font_chooser() },
			{ QStringLiteral("appearanceUiSizeRow"),
			  QStringLiteral("Interface size"),
			  aPanel.ui_size_editor() },
			{ QStringLiteral("appearanceUiWeightRow"),
			  QStringLiteral("Interface weight"),
			  aPanel.ui_weight_editor() },
			{ QStringLiteral("appearanceUiLineHeightRow"),
			  QStringLiteral("Line height"),
			  aPanel.ui_line_height_editor() },
			{ QStringLiteral("appearanceCodeFamilyRow"),
			  QStringLiteral("Code font"),
			  aPanel.code_font_chooser() },
			{ QStringLiteral("appearanceCodeSizeRow"),
			  QStringLiteral("Code size"),
			  aPanel.code_size_editor() },
			{ QStringLiteral("appearanceCodeWeightRow"),
			  QStringLiteral("Code weight"),
			  aPanel.code_weight_editor() },
			{ QStringLiteral("appearanceTabWidthRow"),
			  QStringLiteral("Tab width"),
			  aPanel.tab_width_option(4U) },
			{ QStringLiteral("appearanceWhiteSpaceRow"),
			  QStringLiteral("White space"),
			  aPanel.white_space_selector() },
			{ QStringLiteral("appearanceLigaturesRow"),
			  QStringLiteral("Ligatures"),
			  aPanel.ligatures_switch() },
			{ QStringLiteral("appearanceSyntaxBoldRow"),
			  QStringLiteral("Bold syntax emphasis"),
			  aPanel.syntax_bold_switch() },
			{ QStringLiteral("appearanceSyntaxItalicRow"),
			  QStringLiteral("Italic syntax emphasis"),
			  aPanel.syntax_italic_switch() },
		};
	}

	/// \brief Collects every accessible name and description of a widget subtree.
	///
	/// \details Through `QAccessible`, which is the tree a screen reader walks, so a
	/// control removed from the widget tree is shown to be absent from the tree that
	/// actually matters rather than merely from `findChildren`.
	void collect_accessible(QAccessibleInterface* aNode, std::vector<QString>& aSaid)
	{
		if (aNode == nullptr) {
			return;
		}
		aSaid.push_back(aNode->text(QAccessible::Name));
		aSaid.push_back(aNode->text(QAccessible::Description));
		for (auto child = 0; child < aNode->childCount(); ++child) {
			collect_accessible(aNode->child(child), aSaid);
		}
	}

	[[nodiscard]] auto accessible_text(QWidget& aWidget) -> std::vector<QString>
	{
		auto said = std::vector<QString>{ };
		collect_accessible(QAccessible::queryAccessibleInterface(&aWidget), said);
		return said;
	}

	[[nodiscard]] auto any_says(
		const std::vector<QString>& aSaid,
		const QString&              aNeedle) -> bool
	{
		return std::ranges::any_of(aSaid, [&aNeedle](const QString& aText) {
			return aText.contains(aNeedle, Qt::CaseInsensitive);
		});
	}

	/// \brief Whether anything in the tree is named exactly this.
	///
	/// \details Exact, because a retired control's identity was its exact accessible
	/// name and a live control may legitimately contain it: the Theme card's Reset is
	/// named "Reset appearance mode", which is not the mode combo box.
	[[nodiscard]] auto any_is(
		const std::vector<QString>& aSaid,
		const QString&              aName) -> bool
	{
		return std::ranges::any_of(aSaid, [&aName](const QString& aText) {
			return aText == aName;
		});
	}

	/// \brief The page must offer every mode through the tiles, and no combo box.
	void test_the_tiles_are_the_mode_control()
	{
		auto recorded = recorder{ };
		auto panel    = qt::appearance_settings_panel{ recorded.sink() };
		panel.adopt(legal_settings());
		recorded.clear();

		auto* const previews = panel.theme_previews();
		expect(
			previews != nullptr
				&& previews->cards().size() == qt::theme_preview_count
				&& qt::theme_preview_count == core::appearance_mode_count,
			"the strip carries one tile per mode the core declares");
		for (auto index = std::size_t{ 0U }; index < core::appearance_mode_count;
			 ++index) {
			const auto mode = static_cast<core::appearance_mode>(
				static_cast<std::uint8_t>(index));
			panel.choose_mode(mode);
			expect(
				recorded.last_mode() == std::optional{ mode },
				"choosing each theme dispatches exactly that mode as an intent");
		}

		auto adopted = legal_settings();
		adopted.Mode = core::appearance_mode::dark;
		recorded.clear();
		panel.adopt(adopted);
		expect(
			previews->current() == core::appearance_mode::dark
				&& !recorded.last_mode().has_value(),
			"adoption marks the adopted theme without dispatching");

		show(panel);
		const auto dark  = previews->cards()[1].Rect.center();
		auto       press = QMouseEvent{ QEvent::MouseButtonPress,
										QPointF{ dark },
										QPointF{ dark },
										Qt::LeftButton,
										Qt::LeftButton,
										Qt::NoModifier };
		recorded.clear();
		QApplication::sendEvent(previews, &press);
		settle();
		expect(
			recorded.last_mode() == std::optional{ core::appearance_mode::light }
				&& previews->current() == core::appearance_mode::light,
			"clicking a tile dispatches its mode, so the tiles are the control");
		panel.hide();
	}

	/// \brief Eleven rows, each one line of label, glyph and control.
	void test_the_type_rows_are_one_line_each()
	{
		auto panel = qt::appearance_settings_panel{ };
		panel.adopt(legal_settings());
		show(panel);

		const auto rows = type_rows(panel);
		// [[BPR-1107]] adds White space beside Tab width, so eleven became twelve.
		expect(rows.size() == 12U, "the Type card carries twelve rows");
		for (const auto& row : rows) {
			auto* const frame = panel.findChild<QWidget*>(row.Object);
			expect(
				frame != nullptr && row.Control != nullptr,
				"each named row exists and carries its control");
			auto* const label
				= panel.findChild<QLabel*>(row.Object + QStringLiteral("Name"));
			auto* const glyph = panel.findChild<QToolButton*>(
				row.Object + QStringLiteral("Glyph"));
			expect(
				label != nullptr && glyph != nullptr
					&& label->text() == row.Label,
				"each row reads label, info glyph, control");

			// One line, asserted as geometry rather than as presence: a row that
			// stacked its control under its label would be twice as tall and the
			// two centres would be a line apart.
			const auto labelCentre
				= label->mapTo(frame, label->rect().center()).y();
			const auto glyphCentre
				= glyph->mapTo(frame, glyph->rect().center()).y();
			const auto controlCentre
				= row.Control->mapTo(frame, row.Control->rect().center()).y();
			const auto tallest = std::max(
				{ label->height(), glyph->height(), row.Control->height() });
			expect(
				frame->height() > 0 && tallest > 0,
				"the row is laid out, so its geometry means something");
			expect(
				std::abs(labelCentre - controlCentre) <= 4
					&& std::abs(labelCentre - glyphCentre) <= 4,
				"the label, the glyph and the control sit on one line");
			expect(
				frame->height() <= tallest + 12,
				"and the row is one line tall rather than a stacked pair");
		}
	}

	/// \brief Every consequence must be reachable by hover, by focus, and by name.
	void test_every_consequence_is_reachable_three_ways()
	{
		auto panel = qt::appearance_settings_panel{ };
		panel.adopt(legal_settings());
		show(panel);

		for (const auto& row : type_rows(panel)) {
			auto* const glyph = panel.findChild<QToolButton*>(
				row.Object + QStringLiteral("Glyph"));
			expect(glyph != nullptr, "the row has its glyph");
			// The sentence as the *control* carries it, so none of the three routes
			// below is compared against itself.
			const auto sentence = row.Control->accessibleDescription();
			expect(
				!sentence.isEmpty() && sentence.endsWith(QLatin1Char{ '.' }),
				"the control's accessible description is the consequence sentence");
			// Hover: Qt shows a tool tip on hover, and the popover is that tip.
			expect(
				glyph->toolTip() == sentence,
				"hovering the glyph shows the same consequence");
			expect(
				glyph->accessibleDescription() == sentence,
				"so does the glyph's own accessible description, so the sentence is "
				"never reachable by pointer only");
			// Keyboard focus: the glyph is a focusable control, and focusing it
			// from the keyboard shows the popover — the third route, and the one a
			// reviewer who never touches the pointer travels.
			//
			// This is asserted rather than assumed because it was broken.
			// `info_glyph::show_popover` used to pass the glyph's rect to
			// `QToolTip::showText`, and a rect-bound tip hides the moment the
			// pointer is outside that rect. A keyboard reviewer's pointer is
			// somewhere else entirely, so the sentence was shown to nobody while
			// every route that only inspects properties still looked healthy
			// ([[BPR-0790]]).
			expect(
				glyph->focusPolicy() == Qt::StrongFocus,
				"the glyph is reachable by keyboard");
			QToolTip::hideText();
			settle();
			glyph->clearFocus();
			glyph->setFocus(Qt::TabFocusReason);
			settle();
			expect(
				glyph->hasFocus(),
				"and it takes keyboard focus, so the consequence is reachable "
				"without a pointer");
			// The popover is shown one turn of the event loop after the focus
			// event, so let that turn run before looking for it.
			settle();
			expect(
				QToolTip::isVisible() && QToolTip::text() == sentence,
				"and focusing it shows the popover carrying that same sentence, "
				"with no pointer anywhere near the glyph");
			glyph->clearFocus();
			settle();
			expect(
				!QToolTip::isVisible() || QToolTip::text() != sentence,
				"and the popover goes away when the glyph loses focus");
		}
	}

	/// \brief Sizes read in points; the controls still carry the core's own bounds.
	void test_sizes_read_in_points_within_the_core_bounds()
	{
		auto panel = qt::appearance_settings_panel{ };
		panel.adopt(legal_settings());

		const auto matches
			= [](const QSpinBox& aBox, const core::typography_field aField) {
				  const auto bounds = core::bounds_of(aField);
				  return aBox.minimum() == static_cast<int>(bounds.Minimum)
					  && aBox.maximum() == static_cast<int>(bounds.Maximum);
			  };
		expect(
			matches(*panel.ui_size_editor(), core::typography_field::point_size)
				&& matches(
					*panel.code_size_editor(), core::typography_field::point_size)
				&& matches(*panel.ui_weight_editor(), core::typography_field::weight)
				&& matches(
					*panel.code_weight_editor(), core::typography_field::weight)
				&& matches(
					*panel.ui_line_height_editor(),
					core::typography_field::line_height),
			"every stepper carries the core's own bounds, so no value it can "
			"express is one the validator would refuse");

		panel.ui_size_editor()->setValue(1'300);
		panel.code_size_editor()->setValue(1'250);
		expect(
			panel.ui_size_editor()->text() == QStringLiteral("13 pt"),
			"a whole size reads as whole points");
		expect(
			panel.code_size_editor()->text() == QStringLiteral("12.50 pt"),
			"a fractional size keeps its two decimals");
		expect(
			!panel.ui_size_editor()->text().contains(QStringLiteral("1300"))
				&& !panel.code_size_editor()->text().contains(
					QStringLiteral("1250")),
			"and never in the hundredths the core stores, which is what the page "
			"this replaces showed");
		expect(
			panel.ui_size_editor()->value() == 1'300
				&& panel.code_size_editor()->value() == 1'250,
			"while the value offered to the host stays the exact stored hundredths");

		// Typing points is the same gesture as stepping them: the control's own
		// validation reads what was typed as points, so a reviewer who types 16 does
		// not get sixteen hundredths of a point.
		auto* const typed = panel.ui_size_editor()->findChild<QLineEdit*>();
		expect(typed != nullptr, "the stepper can be typed into");
		typed->setText(QStringLiteral("16"));
		panel.ui_size_editor()->interpretText();
		expect(
			panel.ui_size_editor()->value() == 1'600,
			"typing sixteen means sixteen points, not sixteen hundredths");
	}

	/// \brief Tab width is a 2, 4, 8 segment inside the core's own bounds.
	void test_tab_width_is_a_segment()
	{
		auto recorded                    = recorder{ };
		auto panel                       = qt::appearance_settings_panel{ recorded.sink() };
		auto adopted                     = legal_settings();
		adopted.Typography.Code.TabWidth = 4U;
		panel.adopt(adopted);
		recorded.clear();

		const auto bounds = core::bounds_of(core::typography_field::tab_width);
		for (const auto width : { 2U, 4U, 8U }) {
			expect(
				panel.tab_width_option(width) != nullptr
					&& width >= bounds.Minimum && width <= bounds.Maximum,
				"the segment offers 2, 4 and 8, and each is a width the core "
				"admits");
		}
		expect(
			panel.tab_width_option(3U) == nullptr,
			"and offers nothing else");
		expect(
			panel.tab_width_option(4U)->isChecked()
				&& !panel.tab_width_option(2U)->isChecked()
				&& !panel.tab_width_option(8U)->isChecked()
				&& panel.tab_width() == 4U,
			"the adopted width is the chosen segment");

		panel.tab_width_option(8U)->click();
		const auto offered = recorded.last_typography();
		expect(
			offered.has_value() && offered->Code.TabWidth == 8U
				&& panel.tab_width_option(8U)->isChecked()
				&& !panel.tab_width_option(4U)->isChecked(),
			"choosing a segment offers exactly that width and unchooses the rest");

		// A durable record may carry a legal width the design does not offer. The
		// segment must then say nothing rather than claim a width that is not in
		// effect, and an unrelated gesture must not change it.
		auto unusual                     = legal_settings();
		unusual.Typography.Code.TabWidth = 5U;
		panel.adopt(unusual);
		recorded.clear();
		expect(
			!panel.tab_width_option(2U)->isChecked()
				&& !panel.tab_width_option(4U)->isChecked()
				&& !panel.tab_width_option(8U)->isChecked()
				&& panel.tab_width() == 5U,
			"a width the segment does not offer leaves every segment unchosen");
		panel.ligatures_switch()->setChecked(
			!unusual.Typography.Code.Ligatures);
		expect(
			recorded.last_typography().has_value()
				&& recorded.last_typography()->Code.TabWidth == 5U,
			"and an unrelated gesture carries that width on rather than quietly "
			"replacing it");
	}

	/// \brief [[BPR-1107]]. When the panes draw white space is one row beside
	/// Tab width: six choices in the vocabulary's order, the adopted one shown
	/// without a gesture, and a choice leaving as a typography intent.
	void test_white_space_is_a_row_beside_tab_width()
	{
		auto recorded                      = recorder{ };
		auto panel                         = qt::appearance_settings_panel{ recorded.sink() };
		auto adopted                       = legal_settings();
		adopted.Typography.Code.WhiteSpace = core::white_space_drawing::trailing;
		panel.adopt(adopted);
		show(panel);
		recorded.clear();

		auto* const selector = panel.white_space_selector();
		expect(selector != nullptr && selector->count() == static_cast<int>(core::white_space_drawing_count),
			   "the row offers one item per choice the core declares");
		const auto labels = std::array{ QStringLiteral("None"), QStringLiteral("Selection"), QStringLiteral("Leading"),
										QStringLiteral("Trailing"), QStringLiteral("Leading and trailing"), QStringLiteral("All") };
		for (auto index = 0; index < selector->count(); ++index) {
			expect(selector->itemData(index).toInt() == index && selector->itemText(index) == labels.at(static_cast<std::size_t>(index)),
				   "in the vocabulary's own order, each named the way Sublime Text's list names it");
		}
		expect(selector->currentData().toInt() == static_cast<int>(core::white_space_drawing::trailing) && !recorded.last_typography().has_value(),
			   "the adopted choice is shown, and showing it dispatches nothing");
		auto* const row      = panel.findChild<QWidget*>(QStringLiteral("appearanceWhiteSpaceRow"));
		auto* const tabWidth = panel.findChild<QWidget*>(QStringLiteral("appearanceTabWidthRow"));
		expect(row != nullptr && tabWidth != nullptr && row->parentWidget() == tabWidth->parentWidget()
				   && row->geometry().top() > tabWidth->geometry().top()
				   && row->geometry().top() - tabWidth->geometry().bottom() < row->geometry().height(),
			   "the row sits directly beneath Tab width on the Type card");

		selector->setCurrentIndex(selector->findData(static_cast<int>(core::white_space_drawing::all)));
		const auto offered = recorded.last_typography();
		expect(offered.has_value() && offered->Code.WhiteSpace == core::white_space_drawing::all
				   && offered->Code.TabWidth == adopted.Typography.Code.TabWidth,
			   "choosing All offers exactly that choice and carries the rest of the typography on");
		panel.hide();
	}

	/// \brief [[BPR-1107]]. The Type card for the design page, in the four
	/// themes. Written only when a directory is named.
	void test_white_space_row_design_captures()
	{
		const auto directory = qEnvironmentVariable("BPR_WHITE_SPACE_CAPTURE_DIR");
		if (directory.isEmpty()) {
			return;
		}
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto panel   = qt::appearance_settings_panel{ };
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			auto adopted = legal_settings();
			adopted.Mode = mode;
			panel.adopt(adopted);
			profile.apply_to(panel);
			panel.adopt_appearance(profile);
			show(panel);
			auto* const card = panel.findChild<QWidget*>(QStringLiteral("appearanceTypeCard"));
			expect(card != nullptr, "the Type card is composed");
			const auto name = core::stable_name(mode);
			expect(card->grab().save(QStringLiteral("%1/type-card-%2.png").arg(directory, QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()))), "PNG"),
				   "the Type card capture is written");
			panel.hide();
		}
	}

	/// \brief Every font gesture must leave as an intent, carrying its value.
	void test_font_gestures_translate_to_intents()
	{
		auto recorded = recorder{ };
		auto panel    = qt::appearance_settings_panel{ recorded.sink() };
		panel.adopt(legal_settings());
		recorded.clear();

		panel.code_font_chooser()->setCurrentText(QStringLiteral("Fixture Mono"));
		Q_EMIT panel.code_font_chooser()->lineEdit()->editingFinished();
		panel.ui_size_editor()->setValue(1'400);
		panel.ui_weight_editor()->setValue(700);
		panel.ui_line_height_editor()->setValue(120);
		panel.code_size_editor()->setValue(1'250);
		panel.code_weight_editor()->setValue(500);
		panel.tab_width_option(8U)->click();
		panel.ligatures_switch()->setChecked(true);
		panel.syntax_bold_switch()->setChecked(false);
		panel.syntax_italic_switch()->setChecked(false);

		const auto offered = recorded.last_typography();
		expect(offered.has_value(), "a font gesture dispatches a typography intent");
		expect(
			offered->Ui.Font.PointSizeCentipoints == 1'400U
				&& offered->Ui.Font.Weight == 700U
				&& offered->Ui.LineHeightPercent == 120U,
			"every interface font gesture reaches the intent");
		expect(
			offered->Code.Font.Family == "Fixture Mono"
				&& offered->Code.Font.PointSizeCentipoints == 1'250U
				&& offered->Code.Font.Weight == 500U && offered->Code.TabWidth == 8U
				&& offered->Code.Ligatures && !offered->Code.SyntaxBold
				&& !offered->Code.SyntaxItalic,
			"every code font gesture reaches the intent");
		expect(
			panel.adopted().Typography == legal_settings().Typography,
			"a gesture changes nothing the surface believes is in effect until "
			"the host says so");
	}

	/// \brief A refusal must restore the surface and name the reason.
	void test_a_refusal_restores_and_reports()
	{
		auto       recorded = recorder{ };
		auto       panel    = qt::appearance_settings_panel{ recorded.sink() };
		const auto adopted  = legal_settings();
		panel.adopt(adopted);
		recorded.clear();

		// The family is the one field a control can express illegally: the core
		// bounds it by byte length, not by value, so a stepper cannot cover it.
		panel.code_font_chooser()->setCurrentText(QString{ });
		Q_EMIT panel.code_font_chooser()->lineEdit()->editingFinished();
		const auto offered = recorded.last_typography();
		expect(
			offered.has_value() && offered->Code.Font.Family.empty(),
			"the illegal family reaches the host rather than being corrected here");
		const auto rejections = core::validate_typography(*offered);
		expect(
			!rejections.empty(),
			"the core does refuse it, so the reporting below is about a real "
			"refusal");

		const auto beforeReport = recorded.count();
		panel.report(rejections);
		expect(
			panel.code_font_chooser()->currentText().toStdString()
				== adopted.Typography.Code.Font.Family,
			"a refused control is put back to the value that is in effect");
		expect(
			panel.adopted() == adopted,
			"a refusal changes nothing about what the surface believes is in "
			"effect");
		expect(
			recorded.count() == beforeReport,
			"restoring the controls dispatches nothing, so a refusal cannot "
			"re-offer the record that was just refused");

		const auto status = panel.status_text();
		expect(
			status.contains(QStringLiteral("Code font"))
				&& status.contains(QStringLiteral("family")),
			"the typed reason names the font and the field it belongs to");
		expect(
			status.startsWith(QStringLiteral("Refused")),
			"a refusal does not read as a success");
	}

	/// \brief Session-only settings must stay visibly session-only after adoption.
	void test_persistence_failure_remains_visible_across_session_changes()
	{
		auto panel = qt::appearance_settings_panel{ };
		panel.report_persistence_unavailable();
		expect(
			panel.status_text().contains(QStringLiteral("storage is unavailable"))
				&& panel.status_text().contains(
					QStringLiteral("only for this session"))
				&& panel.status_text().contains(QStringLiteral("System defaults")),
			"the surface names the session-only fallback and restart default");

		panel.adopt(legal_settings());
		expect(
			panel.status_text().contains(QStringLiteral("in effect"))
				&& panel.status_text().contains(
					QStringLiteral("only for this session")),
			"a later session adoption cannot hide that persistence is unavailable");
	}

	/// \brief Adopting must show, not dispatch.
	void test_adoption_shows_without_dispatching()
	{
		auto recorded = recorder{ };
		auto panel    = qt::appearance_settings_panel{ recorded.sink() };

		auto settings                            = legal_settings();
		settings.Mode                            = core::appearance_mode::high_contrast_dark;
		settings.Typography.Code.TabWidth        = 8U;
		settings.Typography.Ui.LineHeightPercent = 130U;
		settings.Typography.Code.Ligatures       = true;
		panel.adopt(settings);

		expect(
			recorded.count() == 0U,
			"showing what the host published dispatches nothing; without echo "
			"suppression an adoption would re-offer the record that caused it");
		expect(
			panel.statistics().SuppressedEchoes > 0U,
			"the suppression is exercised rather than merely present");
		expect(
			panel.theme_previews()->current()
					== core::appearance_mode::high_contrast_dark
				&& panel.tab_width() == 8U
				&& panel.ui_line_height_editor()->value() == 130
				&& panel.ligatures_switch()->isChecked(),
			"every adopted value is shown");
	}

	/// \brief The Colours card must be titled for the theme on screen.
	void test_the_colours_card_names_the_theme_on_screen()
	{
		auto panel = qt::appearance_settings_panel{ };
		panel.adopt(legal_settings());
		expect(
			panel.colours_title()->text()
					== QStringLiteral("Colours for the theme on screen")
				&& panel.colours_subtitle()->text().contains(
					QStringLiteral("Each theme keeps its own colours.")),
			"until the composition names the theme the card says what is true of "
			"any of them rather than guessing one");

		panel.set_resolved_variant(core::appearance_variant::dark);
		expect(
			panel.colours_title()->text() == QStringLiteral("Colours for Dark")
				&& panel.colours_subtitle()->text()
					== QStringLiteral("Edits change Dark only. Each theme keeps "
									  "its own colours."),
			"named, the card is titled for the theme on screen and says edits "
			"change only that one");

		panel.set_resolved_variant(core::appearance_variant::high_contrast_light);
		expect(
			panel.colours_title()->text()
					== QStringLiteral("Colours for High contrast light")
				&& panel.colours_subtitle()->text().startsWith(
					QStringLiteral("Edits change High contrast light only.")),
			"and it follows the theme on screen when that changes");

		expect(
			panel.colours() != nullptr && panel.colours()->listed_roles().size() == 58U,
			"the card lists the 58 roles it offers (RFC-BPR-0034 C5, amended "
			"2026-09-24, for the label tints by BPR-1083 and for the attention "
			"amber by BPR-1085)");
	}

	/// \brief A swatch previews through the picker, and one bar offers the choice.
	void test_the_preview_bar_exists_only_while_previewing()
	{
		auto recorded = recorder{ };
		auto panel    = qt::appearance_settings_panel{ recorded.sink() };
		auto adopted  = legal_settings();
		adopted.Mode  = core::appearance_mode::dark;
		const auto resolved
			= qt::builtin_appearance_palette(core::appearance_variant::dark);
		application::customisation_of(adopted, core::appearance_variant::dark)
			= application::appearance_palette_customisation{ .Palette = resolved,
															 .Stored  = true };
		panel.set_resolved_variant(core::appearance_variant::dark);
		panel.adopt(adopted, resolved);
		show(panel);
		recorded.clear();

		expect(
			panel.preview_bar() == nullptr
				&& panel.keep_changes_button() == nullptr
				&& panel.cancel_changes_button() == nullptr
				&& panel.findChild<QWidget*>(
					   QStringLiteral("appearancePreviewBar"))
					== nullptr,
			"with nothing previewed there is no bar, in the widget tree or "
			"anywhere else");

		panel.set_colour_picker(picker_answering(
			{ QColor{ 0x11, 0x22, 0x33 } }, QColor{ 0x1a, 0x4a, 0x2e }));
		panel.colours()->swatch_of(role::diff_added_background)->click();

		expect(
			panel.preview_bar() != nullptr
				&& panel.preview_bar_text()
					== QStringLiteral("Previewing 1 colour change to Dark in the "
									  "review window."),
			"one edited colour raises one bar that says what is previewed, to "
			"which theme, and where");
		expect(
			panel.preview_palette().has_value()
				&& (*panel.preview_palette())[core::index_of(
					   role::diff_added_background)]
					== core::appearance_color{ 0x1aU, 0x4aU, 0x2eU, 255U }
				&& (*panel.preview_palette())[core::index_of(role::code_background)]
					== resolved[core::index_of(role::code_background)],
			"the previewed palette is the adopted one with exactly that role "
			"changed");
		expect(
			recorded.count() == 0U,
			"and the preview waits for event delivery rather than dispatching once "
			"per colour the picker moved through");
		settle();
		expect(
			recorded.count_of<application::preview_appearance_palette_intent>() == 1U
				&& recorded.last<application::preview_appearance_palette_intent>()
						->Palette
					== *panel.preview_palette(),
			"one preview intent carries the colour the picker landed on");

		panel.set_colour_picker(picker_answering({ }, QColor{ 0x93, 0x30, 0x30 }));
		panel.colours()->swatch_of(role::diff_removed_background)->click();
		settle();
		expect(
			panel.preview_bar_text()
				== QStringLiteral("Previewing 2 colour changes to Dark in the "
								  "review window."),
			"a second edited colour is counted, in the sentence the design writes");

		const auto previewed = *panel.preview_palette();
		panel.keep_changes_button()->click();
		expect(
			recorded.last<application::commit_appearance_palette_intent>() != nullptr
				&& recorded.last<application::commit_appearance_palette_intent>()
						->Palette
					== previewed,
			"Keep changes is the existing commit, and it offers exactly what was "
			"previewed");

		// The host's terminal adoption is what ends the preview.
		panel.adopt(adopted, previewed);
		expect(
			panel.preview_bar() == nullptr
				&& panel.findChild<QWidget*>(
					   QStringLiteral("appearancePreviewBar"))
					== nullptr
				&& !panel.preview_palette().has_value(),
			"an accepted commit removes the bar from the widget tree rather than "
			"hiding it");

		// Cancel ends a preview too, and dispatches the existing intent.
		panel.set_colour_picker(picker_answering({ }, QColor{ 0x11, 0x22, 0x33 }));
		panel.colours()->swatch_of(role::syntax_keyword)->click();
		settle();
		expect(panel.preview_bar() != nullptr, "the bar is back while previewing");
		panel.cancel_changes_button()->click();
		settle();
		expect(
			panel.preview_bar() == nullptr
				&& !panel.preview_palette().has_value()
				&& recorded.last<
					   application::cancel_appearance_palette_preview_intent>()
					!= nullptr,
			"Cancel clears the preview, dispatches the existing cancellation and "
			"takes the bar with it");

		// A picker the reviewer cancels puts the colour back, which leaves nothing
		// previewed and therefore no bar.
		panel.set_colour_picker(
			picker_answering({ QColor{ 0x11, 0x22, 0x33 } }, std::nullopt));
		panel.colours()->swatch_of(role::syntax_keyword)->click();
		settle();
		expect(
			panel.preview_bar() == nullptr
				&& !panel.preview_palette().has_value(),
			"a cancelled picker leaves no preview standing");
		panel.hide();
	}

	/// \brief A burst of picker movement must dispatch only the latest palette.
	void test_preview_bursts_and_invalidation()
	{
		auto recorded = recorder{ };
		auto panel    = qt::appearance_settings_panel{ recorded.sink() };
		panel.adopt(legal_settings());
		const auto offer = [&panel](const QColor& aColour) {
			panel.set_colour_picker(picker_answering({ }, aColour));
			panel.colours()->swatch_of(role::diff_added_background)->click();
		};

		offer(QColor{ 0x11, 0x22, 0x33 });
		offer(QColor{ 0x44, 0x55, 0x66 });
		offer(QColor{ 0x77, 0x88, 0x99 });
		const auto latest = *panel.preview_palette();
		expect(recorded.count() == 0U, "a preview burst waits for event delivery");
		settle();
		expect(
			recorded.count() == 1U
				&& recorded.last<application::preview_appearance_palette_intent>()
						->Palette
					== latest,
			"three distinct candidates dispatch only the latest total palette once");
		expect(
			panel.statistics().PalettePreviews == 1U,
			"preview statistics count delivered candidates");
		panel.cancel_preview();
		recorded.clear();

		offer(QColor{ 0x11, 0x22, 0x33 });
		panel.cancel_preview();
		settle();
		expect(
			recorded.count() == 1U
				&& recorded.last<application::preview_appearance_palette_intent>()
					== nullptr,
			"cancel invalidates a candidate before delivery");
		recorded.clear();

		offer(QColor{ 0x44, 0x55, 0x66 });
		const auto committed = *panel.preview_palette();
		panel.keep_changes_button()->click();
		settle();
		expect(
			recorded.count() == 1U
				&& recorded.last<application::commit_appearance_palette_intent>()
						->Palette
					== committed,
			"commit offers the latest candidate without a delayed preview");
		recorded.clear();

		offer(QColor{ 0x11, 0x22, 0x33 });
		panel.request_theme_import(QStringLiteral("/fixture/replacement.bprtheme"));
		settle();
		expect(
			recorded.count() == 1U
				&& recorded.last<application::preview_appearance_palette_intent>()
					== nullptr,
			"import replacement invalidates a pending manual candidate");
		recorded.clear();

		offer(QColor{ 0x44, 0x55, 0x66 });
		panel.preview_import(
			qt::builtin_appearance_palette(core::appearance_variant::light));
		settle();
		expect(
			recorded.count() == 0U,
			"an imported preview supersedes a pending candidate");
		offer(QColor{ 0x11, 0x22, 0x33 });
		panel.adopt(legal_settings());
		settle();
		expect(
			recorded.count() == 0U && !panel.preview_palette(),
			"adoption invalidates a pending candidate");
		{
			auto temporary = qt::appearance_settings_panel{ recorded.sink() };
			temporary.adopt(legal_settings());
			temporary.set_colour_picker(
				picker_answering({ }, QColor{ 0x11, 0x22, 0x33 }));
			temporary.colours()->swatch_of(role::diff_added_background)->click();
		}
		settle();
		expect(
			recorded.count() == 0U,
			"destroying the widget discards deferred delivery");
	}

	/// \brief An imported theme is previewed into the theme on screen alone.
	void test_an_imported_theme_previews_into_the_theme_on_screen()
	{
		auto       recorded = recorder{ };
		auto       panel    = qt::appearance_settings_panel{ recorded.sink() };
		auto       adopted  = legal_settings();
		const auto resolved
			= qt::builtin_appearance_palette(core::appearance_variant::dark);
		panel.set_resolved_variant(core::appearance_variant::dark);
		panel.adopt(adopted, resolved);
		recorded.clear();

		panel.request_theme_import(QStringLiteral("/fixture/theme.bprtheme"));
		const auto* const import
			= recorded.last<application::import_appearance_theme_intent>();
		expect(
			import != nullptr && import->Path == "/fixture/theme.bprtheme",
			"Qt dispatches only the selected path and performs no document read");

		auto imported                                      = resolved;
		imported[core::index_of(role::syntax_macro)].Alpha = 91U;
		const auto before                                  = recorded.count();
		panel.preview_import(imported);
		expect(
			panel.preview_palette() == std::optional{ imported }
				&& recorded.count() == before,
			"a host-validated import is mirrored as preview without control echo");
		expect(
			panel.preview_bar() != nullptr
				&& panel.status_text().contains(QStringLiteral("into Dark only")),
			"and the bar and the status both name the theme it previews into");

		auto unrelated = adopted;
		unrelated.Typography.Code.TabWidth += 1U;
		panel.adopt(unrelated, resolved, std::optional{ imported });
		expect(
			panel.preview_palette() == std::optional{ imported }
				&& panel.preview_bar() != nullptr
				&& panel.status_text().contains(QStringLiteral("remains unsaved")),
			"an unrelated host adoption retains the exact active preview, its bar "
			"and its unsaved reading");

		panel.adopt(unrelated, imported);
		expect(
			!panel.preview_palette().has_value()
				&& panel.preview_bar() == nullptr,
			"a terminal palette adoption clears the preview and its bar");
	}

	/// \brief Each card's header Reset must dispatch its existing scope.
	void test_each_card_resets_its_own_scope()
	{
		auto intents = recorder{ };
		auto panel   = qt::appearance_settings_panel{ intents.sink() };
		panel.adopt(qt::default_appearance_settings());

		const auto expect_scope
			= [&panel, &intents](
				  const QString&                            aName,
				  const application::appearance_reset_scope aScope) {
				  auto* const button = button_named(panel, aName);
				  expect(
					  button != nullptr,
					  "each card's Reset is a named native button in its header");
				  intents.clear();
				  button->click();
				  expect(
					  intents.last<application::reset_appearance_intent>() != nullptr
						  && intents.last<application::reset_appearance_intent>()
								  ->Scope
							  == aScope,
					  "and dispatches its own existing scope");
			  };
		expect_scope(
			QStringLiteral("Reset appearance mode"),
			application::appearance_reset_scope::mode);
		expect_scope(
			QStringLiteral("Reset all colours"),
			application::appearance_reset_scope::palette);
		expect_scope(
			QStringLiteral("Reset Dif preferences"),
			application::appearance_reset_scope::dif);

		// Type is two durable groups and one card, so its one Reset resets both —
		// each as its own existing scope, and no new scope is invented.
		auto* const typeReset = button_named(panel, QStringLiteral("Reset type"));
		expect(typeReset != nullptr, "the Type card has its Reset");
		intents.clear();
		typeReset->click();
		const auto scopes = intents.reset_scopes();
		expect(
			scopes.size() == 2U
				&& std::ranges::find(
					   scopes, application::appearance_reset_scope::interface_font)
					!= scopes.end()
				&& std::ranges::find(
					   scopes, application::appearance_reset_scope::code_font)
					!= scopes.end(),
			"the Type Reset resets the interface font and the code font");

		const auto count = intents.count();
		panel.request_theme_export({ });
		expect(
			intents.count() == count,
			"cancelled destination selection dispatches nothing");
		panel.request_theme_export(QStringLiteral("chosen.bprtheme"));
		expect(
			intents.last<application::export_appearance_theme_intent>()->Path
				== "chosen.bprtheme",
			"export dispatches only the chosen path");
		expect(
			panel.import_button() != nullptr && panel.export_button() != nullptr,
			"Import and Export sit in the Colours card's header");
	}

	/// \brief The retired controls must be gone, from both trees.
	void test_the_retired_controls_are_absent()
	{
		auto panel = qt::appearance_settings_panel{ };
		panel.adopt(legal_settings());
		show(panel);

		for (auto* const combo : panel.findChildren<QComboBox*>()) {
			expect(
				static_cast<std::size_t>(combo->count())
					!= core::appearance_role_count,
				"no combo box offers the roles: the role combo box is gone");
			expect(
				combo->findText(QStringLiteral("Follow the system")) < 0
					&& combo->findText(
						   QStringLiteral("High contrast, follow the system"))
						< 0,
				"and none offers the six modes: the mode combo box is gone");
			expect(
				combo->accessibleName() != QStringLiteral("Appearance mode")
					&& combo->accessibleName()
						!= QStringLiteral("Semantic colour role"),
				"nor is one of them the retired control under another name");
		}
		for (auto* const edit : panel.findChildren<QLineEdit*>()) {
			expect(
				edit->placeholderText()
						!= QStringLiteral("#rrggbb or #aarrggbb")
					&& edit->accessibleName()
						!= QStringLiteral("Semantic colour value"),
				"the hex field is gone");
		}
		for (auto* const spin : panel.findChildren<QSpinBox*>()) {
			expect(
				!spin->accessibleName().contains(QStringLiteral("hundredths")),
				"no spin box reads in hundredths of a point any more");
		}
		for (auto* const button : panel.findChildren<QAbstractButton*>()) {
			expect(
				button->text() != QStringLiteral("Preview")
					&& button->text() != QStringLiteral("Commit")
					&& button->text() != QStringLiteral("Cancel preview")
					&& button->text() != QStringLiteral("Reset semantic colours"),
				"the separate Preview, Commit and Cancel preview buttons are gone");
		}
		// No group boxes: the three that stacked spin boxes are cards now.
		expect(
			panel.findChildren<QGroupBox*>().isEmpty(),
			"the group boxes the page stacked are gone");

		// And the same is true of the tree a screen reader walks.
		const auto said = accessible_text(panel);
		expect(
			any_says(said, QStringLiteral("Filter roles"))
				&& any_says(said, QStringLiteral("Show Dif mascot"))
				&& any_says(said, QStringLiteral("Interface font")),
			"the accessibility walk does reach this page's own controls, so its "
			"absences below mean something");
		expect(
			!any_is(said, QStringLiteral("Appearance mode"))
				&& !any_is(said, QStringLiteral("Semantic colour role"))
				&& !any_is(said, QStringLiteral("Semantic colour value"))
				&& !any_says(said, QStringLiteral("hundredths"))
				&& !any_says(said, QStringLiteral("Cancel preview")),
			"none of the retired controls is in the accessibility tree either");

		// A displayed name is never the durable exchange identifier.
		for (auto index = std::size_t{ 0U }; index < core::appearance_role_count;
			 ++index) {
			const auto durable = core::stable_name(
				static_cast<role>(static_cast<std::uint8_t>(index)));
			const auto shown = QString::fromUtf8(
				durable.data(), static_cast<qsizetype>(durable.size()));
			expect(
				!any_says(said, shown),
				"and no stable exchange identifier is said anywhere on the page");
		}
		panel.hide();
	}

	/// \brief The page must not demand more width than a Settings window has.
	///
	/// \details The sufficient claim - that a 760 by 480 Settings window needs no
	/// horizontal scrolling with this page live inside it - belongs to
	/// `qt_settings_window`, which asserts it in four appearances and at four scales
	/// through the composed window. What is panel-local, and what makes that
	/// possible, is that the two large collections bound themselves: the six tiles
	/// and the 58 colour rows each scroll inside their own well instead of asking the
	/// page for their full width and height. The measured minimum is recorded as
	/// context rather than compared against an invented figure.
	void test_the_page_does_not_demand_width()
	{
		auto panel = qt::appearance_settings_panel{ };
		panel.adopt(legal_settings());
		show(panel);

		std::cout << "appearance-page-minimum: width="
				  << panel.minimumSizeHint().width()
				  << " height=" << panel.minimumSizeHint().height() << '\n';
		expect(
			panel.minimumSizeHint().width() < 760,
			"the page asks for less width than the whole minimum Settings window, "
			"which is the necessary condition for the composed page to need no "
			"sideways scrolling");

		auto* const strip = panel.theme_previews();
		expect(
			strip->well() != nullptr
				&& strip->minimumSizeHint().width() < strip->sizeHint().width(),
			"the six tiles scroll inside their well: the strip asks for one tile "
			"rather than for all six");
		auto* const list = panel.colours();
		expect(
			list->well() != nullptr
				&& list->well()->verticalScrollBar()->maximum() > 0,
			"and the 58 rows scroll inside theirs rather than lengthening the page");
		panel.hide();
	}

	void test_theme_and_contrast_refusals_remain_typed_and_visible()
	{
		auto       panel      = qt::appearance_settings_panel{ };
		const auto structural = std::array{
			core::theme_import_rejection{
				.Reason  = core::theme_import_reason::external_reference,
				.Subject = "https://example.invalid/theme",
				.Line    = 4U,
			},
		};
		panel.report(structural);
		expect(
			panel.status_text().contains(QStringLiteral("Line 4"))
				&& panel.status_text().contains(
					QStringLiteral("external references")),
			"the structural refusal preserves its typed line and reason");

		const auto contrast = std::array{
			application::appearance_contrast_rejection{
				.Variant = core::appearance_variant::high_contrast_dark,
				.Finding = core::contrast_finding{
					.Pair = core::contrast_pair{
						.Foreground = role::syntax_keyword,
						.Background = role::code_background,
					},
					.RatioHundredths   = 320U,
					.MinimumHundredths = 700U,
					.Meets             = false,
				},
			},
		};
		panel.report(contrast);
		expect(
			panel.status_text().contains(QStringLiteral("High contrast dark"))
				&& panel.status_text().contains(QStringLiteral("syntax_keyword"))
				&& panel.status_text().contains(QStringLiteral("3.20:1"))
				&& panel.status_text().contains(QStringLiteral("7.00:1")),
			"the contrast refusal preserves variant, roles, ratio, and threshold");
	}

	void test_dif_controls_translate_and_explain()
	{
		auto intents  = recorder{ };
		auto panel    = qt::appearance_settings_panel{ intents.sink() };
		auto settings = qt::default_appearance_settings();
		panel.adopt(settings);
		expect(
			panel.dif_visibility_switch()->isChecked()
				&& panel.motion_selector()->currentIndex() == 0
				&& intents.count() == 0U,
			"shown/System adoption emits no gesture");

		// The Dif controls are rows in the same language now, so each carries its
		// consequence where every other row carries one.
		for (const auto& object : { QStringLiteral("appearanceDifVisibilityRow"),
									QStringLiteral("appearanceDifMotionRow") }) {
			auto* const glyph
				= panel.findChild<QToolButton*>(object + QStringLiteral("Glyph"));
			expect(
				glyph != nullptr && !glyph->toolTip().isEmpty(),
				"each Dif row is label, info glyph and control");
		}

		panel.motion_selector()->setCurrentIndex(1);
		expect(
			intents.last<application::apply_dif_preferences_intent>()
					->Preferences.Motion
				== core::motion_mode::full,
			"Full control emits typed preference intent");
		panel.dif_visibility_switch()->click();
		const auto choice
			= intents.last<application::apply_dif_preferences_intent>()->Preferences;
		expect(
			choice.Visibility == core::dif_visibility::hidden
				&& choice.Motion == core::motion_mode::full,
			"visibility gesture retains chosen motion");

		settings.Dif     = choice;
		const auto count = intents.count();
		panel.adopt(settings);
		panel.adopt_motion(
			core::resolve_dif(settings.Dif, core::motion_observation::unsupported));
		expect(
			intents.count() == count && !panel.dif_visibility_switch()->isChecked(),
			"accepted hidden projection updates controls without echo");

		settings.Dif = { };
		panel.adopt(settings);
		panel.adopt_motion(
			core::resolve_dif(settings.Dif, core::motion_observation::unsupported));
		auto explained = false;
		for (const auto* const label : panel.findChildren<QLabel*>()) {
			explained
				= explained || label->text().contains(QStringLiteral("unsupported"));
		}
		expect(explained, "Settings explains unsupported System motion honestly");
	}

	/// \brief The composition's presentation facts must reach the cards.
	void test_the_profile_supplies_what_only_it_knows()
	{
		auto panel   = qt::appearance_settings_panel{ };
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::dark));
		panel.adopt(profile.settings(), profile.palette());
		panel.adopt_appearance(profile);
		show(panel);

		expect(
			panel.resolved_variant() == std::optional{ profile.variant() }
				&& panel.colours_title()->text()
					== QStringLiteral("Colours for Dark"),
			"the theme on screen comes from the profile's own resolution");
		expect(
			panel.specimen() != nullptr
				&& panel.specimen()->statistics().Adoptions == 1U
				&& panel.specimen()->caption_text().contains(
					QStringLiteral("from Dark.")),
			"the Type preview repaints from the profile and names the theme");
		expect(
			panel.colours()->value_label_of(role::code_background)->font().family()
				== profile.code_font().family(),
			"and the Colours list sets its values in the accepted code font");
		panel.hide();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewAppearanceSettingsPanelTests"));
	test_the_tiles_are_the_mode_control();
	test_the_type_rows_are_one_line_each();
	test_every_consequence_is_reachable_three_ways();
	test_sizes_read_in_points_within_the_core_bounds();
	test_tab_width_is_a_segment();
	test_white_space_is_a_row_beside_tab_width();
	test_white_space_row_design_captures();
	test_font_gestures_translate_to_intents();
	test_a_refusal_restores_and_reports();
	test_persistence_failure_remains_visible_across_session_changes();
	test_adoption_shows_without_dispatching();
	test_the_colours_card_names_the_theme_on_screen();
	test_the_preview_bar_exists_only_while_previewing();
	test_preview_bursts_and_invalidation();
	test_an_imported_theme_previews_into_the_theme_on_screen();
	test_each_card_resets_its_own_scope();
	test_the_retired_controls_are_absent();
	test_the_page_does_not_demand_width();
	test_theme_and_contrast_refusals_remain_typed_and_visible();
	test_dif_controls_translate_and_explain();
	test_the_profile_supplies_what_only_it_knows();
	std::cout << "appearance-settings-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
