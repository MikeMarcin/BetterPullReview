/// \file
/// \brief Proves the settings surface translates gestures and owns no state.

#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

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

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
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
			for (auto index = Intents.size(); index > 0U; --index) {
				if (const auto* const mode
					= std::get_if<application::select_appearance_mode_intent>(
						&Intents[index - 1U])) {
					return mode->Mode;
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto last_typography() const
			-> std::optional<core::typography_settings>
		{
			for (auto index = Intents.size(); index > 0U; --index) {
				if (const auto* const typography
					= std::get_if<application::apply_typography_intent>(
						&Intents[index - 1U])) {
					return typography->Typography;
				}
			}
			return std::nullopt;
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
		return {
			.Mode    = core::appearance_mode::light,
			.Palette = qt::builtin_appearance_palette(
				core::appearance_variant::light),
			.Typography = qt::default_typography_settings(),
		};
	}

	/// \brief Every declared mode must be offerable.
	void test_the_mode_control_offers_the_core_vocabulary()
	{
		auto recorded = recorder{ };
		auto panel    = qt::appearance_settings_panel{ recorded.sink() };
		panel.adopt(legal_settings());
		recorded.clear();

		// Read from the core's published count rather than from a list restated
		// here, which is the drift this assertion exists to catch.
		expect(
			static_cast<std::size_t>(panel.mode_selector()->count())
				== core::appearance_mode_count,
			"the mode control offers exactly the modes the core declares");

		for (auto index = std::size_t{ 0U };
			 index < core::appearance_mode_count;
			 ++index) {
			panel.mode_selector()->setCurrentIndex(static_cast<int>(index));
			const auto dispatched = recorded.last_mode();
			expect(
				dispatched.has_value()
					&& *dispatched == static_cast<core::appearance_mode>(index),
				"selecting a mode dispatches exactly that mode as an intent");
			expect(
				!core::stable_name(static_cast<core::appearance_mode>(index))
					 .empty(),
				"every offered mode has a stable name in the core");
		}
	}

	/// \brief Every font gesture must leave as an intent, carrying its value.
	void test_font_gestures_translate_to_intents()
	{
		auto recorded = recorder{ };
		auto panel    = qt::appearance_settings_panel{ recorded.sink() };
		panel.adopt(legal_settings());
		recorded.clear();

		panel.code_family_editor()->setText(QStringLiteral("Fixture Mono"));
		Q_EMIT panel.code_family_editor()->editingFinished();
		panel.ui_size_editor()->setValue(1'400);
		panel.ui_weight_editor()->setValue(700);
		panel.ui_line_height_editor()->setValue(120);
		panel.code_size_editor()->setValue(1'250);
		panel.code_weight_editor()->setValue(500);
		panel.tab_width_editor()->setValue(8);
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
				&& offered->Code.Font.Weight == 500U
				&& offered->Code.TabWidth == 8U && offered->Code.Ligatures
				&& !offered->Code.SyntaxBold && !offered->Code.SyntaxItalic,
			"every code font gesture reaches the intent");

		// The surface must not have applied any of it. It translated.
		expect(
			panel.adopted().Typography == legal_settings().Typography,
			"a gesture changes nothing the surface believes is in effect until "
			"the host says so");
	}

	/// \brief The controls must never offer a value the core would refuse.
	void test_numeric_controls_carry_the_core_bounds()
	{
		auto       panel = qt::appearance_settings_panel{ };
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
				&& matches(
					*panel.ui_weight_editor(), core::typography_field::weight)
				&& matches(
					*panel.code_weight_editor(), core::typography_field::weight)
				&& matches(
					*panel.ui_line_height_editor(),
					core::typography_field::line_height)
				&& matches(
					*panel.tab_width_editor(), core::typography_field::tab_width),
			"every numeric control carries the core's own bounds, so no value it "
			"can express is one the validator would refuse");
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
		// bounds it by byte length, not by value, so a spin box cannot cover it.
		panel.code_family_editor()->setText(QString{ });
		Q_EMIT panel.code_family_editor()->editingFinished();
		const auto offered = recorded.last_typography();
		expect(
			offered.has_value() && offered->Code.Font.Family.empty(),
			"the illegal family reaches the host rather than being corrected "
			"here");
		const auto rejections = core::validate_typography(*offered);
		expect(
			!rejections.empty(),
			"the core does refuse it, so the reporting below is about a real "
			"refusal");

		const auto beforeReport = recorded.count();
		panel.report(rejections);
		expect(
			panel.code_family_editor()->text().toStdString()
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
			!status.contains(QStringLiteral("in effect."))
				|| status.startsWith(QStringLiteral("Refused")),
			"a refusal does not read as a success");
	}

	/// \brief Session-only settings must stay visibly session-only after adoption.
	void test_persistence_failure_remains_visible_across_session_changes()
	{
		auto panel = qt::appearance_settings_panel{ };
		panel.report_persistence_unavailable();
		expect(
			panel.status_text().contains(QStringLiteral("storage is unavailable"))
				&& panel.status_text().contains(QStringLiteral("only for this session"))
				&& panel.status_text().contains(QStringLiteral("System defaults")),
			"the surface names the session-only fallback and restart default");

		panel.adopt(legal_settings());
		expect(
			panel.status_text().contains(QStringLiteral("in effect"))
				&& panel.status_text().contains(QStringLiteral("only for this session")),
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
			panel.mode_selector()->currentIndex()
					== static_cast<int>(core::appearance_mode::high_contrast_dark)
				&& panel.tab_width_editor()->value() == 8
				&& panel.ui_line_height_editor()->value() == 130
				&& panel.ligatures_switch()->isChecked(),
			"every adopted value is shown");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewAppearanceSettingsPanelTests"));
	test_the_mode_control_offers_the_core_vocabulary();
	test_font_gestures_translate_to_intents();
	test_numeric_controls_carry_the_core_bounds();
	test_a_refusal_restores_and_reports();
	test_persistence_failure_remains_visible_across_session_changes();
	test_adoption_shows_without_dispatching();
	std::cout << "appearance-settings-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
