/// \file
/// \brief Proves the settings surface translates gestures and owns no state.

#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>

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

		// [[BPR-0652]]. The four previews above the selector are the same
		// gesture: adopting marks the current mode, choosing a card sends it.
		auto* const previews = panel.theme_previews();
		expect(previews != nullptr && previews->cards().size() == qt::theme_preview_count
				   && previews->cards()[0].Mode == core::appearance_mode::system
				   && previews->cards()[1].Mode == core::appearance_mode::light
				   && previews->cards()[2].Mode == core::appearance_mode::dark
				   && previews->cards()[3].Mode == core::appearance_mode::high_contrast_system,
			   "the previews are System, Light, Dark and High contrast");
		auto adopted = legal_settings();
		adopted.Mode = core::appearance_mode::light;
		panel.adopt(adopted);
		recorded.clear();
		expect(previews->current() == core::appearance_mode::light,
			   "adoption marks the adopted mode's preview without dispatching");
		expect(!recorded.last_mode().has_value(), "adoption dispatches nothing");
		panel.show();
		QApplication::processEvents();
		const auto dark  = previews->cards()[2].Rect.center();
		auto       press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ dark }, QPointF{ dark },
										Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(previews, &press);
		QApplication::processEvents();
		expect(recorded.last_mode() == std::optional{ core::appearance_mode::dark }
				   && panel.mode_selector()->currentIndex() == static_cast<int>(core::appearance_mode::dark)
				   && previews->current() == core::appearance_mode::dark,
			   "choosing a preview dispatches its mode once, through the selector");
		panel.hide();
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

	void test_semantic_palette_preview_commit_cancel_and_import_are_intents()
	{
		auto recorded       = recorder{ };
		auto panel          = qt::appearance_settings_panel{ recorded.sink() };
		auto adopted        = legal_settings();
		adopted.Mode        = core::appearance_mode::system;
		const auto resolved = qt::builtin_appearance_palette(
			core::appearance_variant::dark);
		application::customisation_of(adopted, core::appearance_variant::dark)
			= application::appearance_palette_customisation{ .Palette = resolved,
															 .Stored  = true };
		panel.adopt(adopted, resolved);
		recorded.clear();

		expect(static_cast<std::size_t>(panel.role_selector()->count())
				   == core::appearance_role_count,
			   "the colour editor offers every role in the core vocabulary");
		const auto roleIndex = core::index_of(
			core::appearance_role::diff_group_boundary_rule);
		panel.role_selector()->setCurrentIndex(static_cast<int>(roleIndex));
		panel.color_editor()->setText(QStringLiteral("#aabbccdd"));
		panel.preview_button()->click();
		QApplication::processEvents();

		const auto* const preview
			= recorded.last<application::preview_appearance_palette_intent>();
		expect(preview != nullptr
				   && preview->Palette[roleIndex]
					   == core::appearance_color{ 187U, 204U, 221U, 170U }
				   && preview->Palette[core::index_of(
						  core::appearance_role::code_background)]
					   == resolved[core::index_of(
						   core::appearance_role::code_background)],
			   "preview dispatches one total semantic palette using core colours");
		expect(panel.adopted() == adopted,
			   "preview does not alter the host-adopted record mirrored by the widget");
		expect(panel.commit_button()->isEnabled()
				   && panel.cancel_button()->isEnabled(),
			   "a preview exposes explicit commit and cancel actions");

		const auto previewedPalette = preview->Palette;
		panel.commit_button()->click();
		const auto* const commit
			= recorded.last<application::commit_appearance_palette_intent>();
		expect(commit != nullptr && commit->Palette == previewedPalette,
			   "commit sends exactly the total palette that was previewed");
		panel.cancel_button()->click();
		expect(recorded.last<application::cancel_appearance_palette_preview_intent>()
					   != nullptr
				   && !panel.preview_palette().has_value(),
			   "cancel clears the local preview and dispatches no replacement palette");
		expect(panel.color_editor()->text()
				   == qt::to_presentation_color(resolved[roleIndex])
						  .name(QColor::HexArgb),
			   "cancel restores the exact resolved System colour in the editor");

		panel.request_theme_import(QStringLiteral("/fixture/theme.bprtheme"));
		const auto* const import
			= recorded.last<application::import_appearance_theme_intent>();
		expect(import != nullptr && import->Path == "/fixture/theme.bprtheme",
			   "Qt dispatches only the selected path and performs no document read");

		auto imported             = resolved;
		imported[roleIndex].Alpha = 91U;
		const auto before         = recorded.count();
		panel.preview_import(imported);
		expect(panel.preview_palette() == std::optional{ imported }
				   && recorded.count() == before,
			   "a host-validated import is mirrored as preview without control echo");

		auto unrelatedAdoption = adopted;
		unrelatedAdoption.Typography.Code.TabWidth += 1U;
		panel.adopt(
			unrelatedAdoption,
			resolved,
			std::optional{ imported });
		expect(
			panel.preview_palette() == std::optional{ imported },
			"an unrelated host adoption retains the exact active preview");
		expect(
			panel.commit_button()->isEnabled()
				&& panel.cancel_button()->isEnabled(),
			"an unrelated host adoption retains commit and cancel authority");
		expect(
			panel.status_text().contains(QStringLiteral("remains unsaved")),
			"an unrelated host adoption still identifies the palette as a preview");

		panel.adopt(unrelatedAdoption, imported);
		expect(
			!panel.preview_palette().has_value()
				&& !panel.commit_button()->isEnabled()
				&& !panel.cancel_button()->isEnabled(),
			"a terminal palette adoption clears the preview and both actions");
	}

	void test_preview_bursts_and_invalidation()
	{
		auto recorded = recorder{ };
		auto panel    = qt::appearance_settings_panel{ recorded.sink() };
		panel.adopt(legal_settings());
		const auto offer = [&panel](const QString& aColor) {
			panel.color_editor()->setText(aColor);
			panel.preview_button()->click();
		};
		offer(QStringLiteral("#112233"));
		offer(QStringLiteral("#445566"));
		offer(QStringLiteral("#778899"));
		const auto latest = *panel.preview_palette();
		expect(recorded.count() == 0U, "a preview burst waits for event delivery");
		QApplication::processEvents();
		expect(recorded.count() == 1U && recorded.last<application::preview_appearance_palette_intent>()->Palette == latest,
			   "three distinct candidates dispatch only the latest total palette once");
		expect(panel.statistics().PalettePreviews == 1U, "preview statistics count delivered candidates");
		panel.cancel_preview();
		recorded.clear();

		offer(QStringLiteral("#112233"));
		panel.cancel_preview();
		QApplication::processEvents();
		expect(recorded.count() == 1U && recorded.last<application::preview_appearance_palette_intent>() == nullptr,
			   "cancel invalidates a candidate before delivery");
		recorded.clear();
		offer(QStringLiteral("#445566"));
		const auto committed = *panel.preview_palette();
		panel.commit_button()->click();
		QApplication::processEvents();
		expect(recorded.count() == 1U && recorded.last<application::commit_appearance_palette_intent>()->Palette == committed,
			   "commit offers the latest candidate without a delayed preview");
		recorded.clear();
		offer(QStringLiteral("#112233"));
		panel.request_theme_import(QStringLiteral("/fixture/replacement.bprtheme"));
		QApplication::processEvents();
		expect(recorded.count() == 1U && recorded.last<application::preview_appearance_palette_intent>() == nullptr,
			   "import replacement invalidates a pending manual candidate");
		recorded.clear();
		offer(QStringLiteral("#445566"));
		panel.preview_import(
			qt::builtin_appearance_palette(core::appearance_variant::light));
		QApplication::processEvents();
		expect(recorded.count() == 0U, "an imported preview supersedes a pending candidate");
		offer(QStringLiteral("#112233"));
		panel.adopt(legal_settings());
		QApplication::processEvents();
		expect(recorded.count() == 0U && !panel.preview_palette(), "adoption invalidates a pending candidate");
		{
			auto temporary = qt::appearance_settings_panel{ recorded.sink() };
			temporary.adopt(legal_settings());
			temporary.color_editor()->setText(QStringLiteral("#112233"));
			temporary.preview_button()->click();
		}
		QApplication::processEvents();
		expect(recorded.count() == 0U, "destroying the widget discards deferred delivery");
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
		expect(panel.status_text().contains(QStringLiteral("Line 4"))
				   && panel.status_text().contains(QStringLiteral("external references")),
			   "the structural refusal preserves its typed line and reason");

		const auto contrast = std::array{
			application::appearance_contrast_rejection{
				.Variant = core::appearance_variant::high_contrast_dark,
				.Finding = core::contrast_finding{
					.Pair = core::contrast_pair{
						.Foreground = core::appearance_role::syntax_keyword,
						.Background = core::appearance_role::code_background,
					},
					.RatioHundredths   = 320U,
					.MinimumHundredths = 700U,
					.Meets             = false,
				},
			},
		};
		panel.report(contrast);
		expect(panel.status_text().contains(QStringLiteral("High contrast dark"))
				   && panel.status_text().contains(QStringLiteral("syntax_keyword"))
				   && panel.status_text().contains(QStringLiteral("3.20:1"))
				   && panel.status_text().contains(QStringLiteral("7.00:1")),
			   "the contrast refusal preserves variant, roles, ratio, and threshold");
	}
	void test_scoped_resets_and_export_are_intents()
	{
		auto       intents  = recorder{ };
		auto       panel    = qt::appearance_settings_panel{ intents.sink() };
		const auto settings = qt::default_appearance_settings();
		panel.adopt(settings);
		const auto labels = std::array{ QStringLiteral("Reset appearance mode"), QStringLiteral("Reset interface font"), QStringLiteral("Reset code font"), QStringLiteral("Reset semantic colours") };
		const auto scopes = std::array{ application::appearance_reset_scope::mode, application::appearance_reset_scope::interface_font, application::appearance_reset_scope::code_font, application::appearance_reset_scope::palette };
		for (auto i = std::size_t{ 0U }; i < labels.size(); ++i) {
			auto* found = static_cast<QPushButton*>(nullptr);
			for (auto* button : panel.findChildren<QPushButton*>()) {
				if (button->accessibleName() == labels[i]) {
					found = button;
				}
			}
			expect(found != nullptr, "each reset is exposed as a named native button");
			found->click();
			expect(intents.last<application::reset_appearance_intent>()->Scope == scopes[i], "reset dispatch names only its selected scope");
		}
		const auto count = intents.count();
		panel.request_theme_export({ });
		expect(intents.count() == count, "cancelled destination selection dispatches nothing");
		panel.request_theme_export(QStringLiteral("chosen.bprtheme"));
		expect(intents.last<application::export_appearance_theme_intent>()->Path == "chosen.bprtheme", "export dispatches only the chosen path");
	}
	void test_dif_controls_translate_and_explain()
	{
		recorder                      intents;
		qt::appearance_settings_panel panel{ intents.sink() };
		auto                          settings = qt::default_appearance_settings();
		panel.adopt(settings);
		expect(panel.dif_visibility_switch()->isChecked() && panel.motion_selector()->currentIndex() == 0 && intents.count() == 0U, "shown/System adoption emits no gesture");
		panel.motion_selector()->setCurrentIndex(1);
		expect(intents.last<application::apply_dif_preferences_intent>()->Preferences.Motion == core::motion_mode::full, "Full control emits typed preference intent");
		panel.dif_visibility_switch()->click();
		const auto choice = intents.last<application::apply_dif_preferences_intent>()->Preferences;
		expect(choice.Visibility == core::dif_visibility::hidden && choice.Motion == core::motion_mode::full, "visibility gesture retains chosen motion");
		settings.Dif     = choice;
		const auto count = intents.count();
		panel.adopt(settings);
		panel.adopt_motion(core::resolve_dif(settings.Dif, core::motion_observation::unsupported));
		expect(intents.count() == count && !panel.dif_visibility_switch()->isChecked(), "accepted hidden projection updates controls without echo");
		settings.Dif = { };
		panel.adopt(settings);
		panel.adopt_motion(core::resolve_dif(settings.Dif, core::motion_observation::unsupported));
		bool explained = false;
		for (const auto* label : panel.findChildren<QLabel*>()) {
			explained = explained || label->text().contains(QStringLiteral("unsupported"));
		}
		expect(explained, "Settings explains unsupported System motion honestly");
		for (auto* button : panel.findChildren<QPushButton*>()) {
			if (button->text() == QStringLiteral("Reset Dif preferences")) {
				button->click();
			}
		}
		expect(intents.last<application::reset_appearance_intent>() && intents.last<application::reset_appearance_intent>()->Scope == application::appearance_reset_scope::dif, "Dif has a scoped native reset button");
	}

} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewAppearanceSettingsPanelTests"));
	test_dif_controls_translate_and_explain();
	test_preview_bursts_and_invalidation();
	test_scoped_resets_and_export_are_intents();
	test_the_mode_control_offers_the_core_vocabulary();
	test_font_gestures_translate_to_intents();
	test_numeric_controls_carry_the_core_bounds();
	test_a_refusal_restores_and_reports();
	test_persistence_failure_remains_visible_across_session_changes();
	test_adoption_shows_without_dispatching();
	test_semantic_palette_preview_commit_cancel_and_import_are_intents();
	test_theme_and_contrast_refusals_remain_typed_and_visible();
	std::cout << "appearance-settings-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
