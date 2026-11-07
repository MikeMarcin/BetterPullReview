#include "github_application/fixture_course_support.hpp"
#include "review_submission_workflow.hpp"

#include <QByteArray>

#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/appearance_application_host.hpp"
	#include "bpr/core/appearance_model.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/qt/appearance_profile.hpp"
	#include "bpr/qt/appearance_settings_panel.hpp"
	#include "bpr/qt/colour_role_list.hpp"
	#include "bpr/qt/comparison_scroll_controller.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/settings_window.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_probe_states.hpp"
	#include "github_application_fixture_access.hpp"
	#include <QAbstractButton>
	#include <QCheckBox>
	#include <QColor>
	#include <QComboBox>
	#include <QCoreApplication>
	#include <QDesktopServices>
	#include <QEvent>
	#include <QFileInfo>
	#include <QLineEdit>
	#include <QList>
	#include <QObject>
	#include <QPushButton>
	#include <QString>
	#include <QTextStream>
	#include <QWidget>
	#include <algorithm>
	#include <array>
	#include <cstddef>
	#include <functional>
	#include <span>
	#include <string_view>
	#include <utility>
	#include <vector>
#endif

namespace bpr::app {
	namespace {
#if defined(BPR_BUILD_TESTING)
		/// \brief Edits one colour through the swatch and the picker seam.
		///
		/// \details [[BPR-0790]]. The path the rebuilt Colours card offers: a swatch
		/// opens the picker, every colour the picker moves through previews in the
		/// review window, and the colour it lands on stays previewed. The picker is a
		/// settable seam exactly so this composed probe can drive it without a modal
		/// panel; the widget still performs no effect of its own.
		void pick_colour(
			qt::appearance_settings_panel& aPanel,
			const core::appearance_role    aRole,
			std::vector<QColor>            aMoves,
			const QColor&                  aAccepted)
		{
			aPanel.set_colour_picker(
				[moves = std::move(aMoves), accepted = aAccepted](
					const qt::colour_pick_request& aRequest)
					-> std::optional<QColor> {
					for (const auto& moved : moves) {
						aRequest.Changed(moved);
					}
					return accepted;
				});
			aPanel.colours()->swatch_of(aRole)->click();
		}
#endif
	}

#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::select_other_appearance_mode(
		github_application& aApplication) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr
			|| implementation->AppearanceHost == nullptr) {
			return false;
		}
		const auto current = implementation->AppearanceHost->settings().Mode;
		const auto other   = current == core::appearance_mode::dark
			? core::appearance_mode::light
			: core::appearance_mode::dark;
		implementation->AppearanceHost->select(other);
		return implementation->AppearanceHost->settings().Mode == other;
	}

	auto detail::github_application_fixture_access::apply_appearance_gesture(
		github_application&         aApplication,
		const core::appearance_mode aMode,
		const std::uint32_t         aTabWidth) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->Window == nullptr
			|| implementation->AppearanceHost == nullptr) {
			return false;
		}
		auto* const panel = implementation->Window->appearance_settings();
		if (panel == nullptr) {
			return false;
		}
		// Through the controls, which is the whole claim. Choosing a theme tile and
		// clicking a tab-width segment is what a reviewer's click produces; if the
		// wiring between them and the host is missing, nothing below moves.
		panel->choose_mode(aMode);
		auto* const segment = panel->tab_width_option(aTabWidth);
		if (segment == nullptr) {
			// The segment offers the three widths the accepted design names; a
			// probe asking for another would not be driving a control that
			// exists.
			return false;
		}
		segment->click();
		panel->dif_visibility_switch()->setChecked(false);
		panel->motion_selector()->setCurrentIndex(static_cast<int>(core::motion_mode::full));
		const auto& settings = implementation->AppearanceHost->settings();
		return settings.Mode == aMode
			&& settings.Typography.Code.TabWidth == aTabWidth
			&& settings.Dif.Visibility == core::dif_visibility::hidden
			&& settings.Dif.Motion == core::motion_mode::full;
	}

	auto detail::github_application_fixture_access::refuse_appearance_gesture(
		github_application& aApplication) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->Window == nullptr
			|| implementation->AppearanceHost == nullptr) {
			return false;
		}
		auto* const panel = implementation->Window->appearance_settings();
		if (panel == nullptr) {
			return false;
		}
		const auto before = implementation->AppearanceHost->settings();
		// An empty family is the one illegal value a control can express: the
		// numeric controls carry the core's own bounds, so they cannot.
		panel->code_font_chooser()->setCurrentText(QString{ });
		Q_EMIT panel->code_font_chooser()->lineEdit()->editingFinished();
		const auto& after = implementation->AppearanceHost->settings();
		return after == before
			&& !implementation->AppearanceHost->last_rejections().empty()
			&& panel->status_text().startsWith(QStringLiteral("Refused"))
			&& panel->code_font_chooser()->currentText().toStdString()
			== before.Typography.Code.Font.Family;
	}

	auto detail::github_application_fixture_access::
		exercise_appearance_theme_customization(
			github_application&    aApplication,
			const std::string_view aThemePath)
			-> detail::appearance_theme_customization_probe_result
	{
		using result               = detail::appearance_theme_customization_probe_result;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->AppearanceHost == nullptr
			|| implementation->AppearanceProfile == nullptr) {
			return result::failed;
		}
		auto* const panel     = implementation->Window->appearance_settings();
		auto* const workbench = implementation->Window->comparison_surface();
		if (panel == nullptr || workbench == nullptr) {
			return result::failed;
		}
		const auto reportStage = [](const char* const aStage) {
			QTextStream stream{ stdout };
			stream << "APPEARANCE_THEME_PROBE_STAGE:" << aStage << '\n';
			stream.flush();
		};

		using state_type = appearance_theme_probe_state;
		if (!implementation->AppearanceThemeProbe.has_value()) {
			implementation->AppearanceThemeProbe.emplace();
			implementation->AppearanceThemeProbe->Path = std::string{ aThemePath };
		}

		auto& probe = *implementation->AppearanceThemeProbe;
		if (probe.Path != aThemePath || probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}

		const auto unchanged = [&]() {
			const auto statistics = workbench->statistics();
			return workbench->current_result() == probe.Result
				&& workbench->current_context() == probe.Context
				&& workbench->selection() == probe.Selection
				&& workbench->scroll_controller()->position() == probe.Position
				&& workbench->token_spans(core::diff_side::left) == probe.LeftSpans
				&& workbench->token_spans(core::diff_side::right) == probe.RightSpans
				&& statistics.GeometryRebuilds == probe.Statistics.GeometryRebuilds
				&& statistics.SemanticIndexAdoptions == probe.Statistics.SemanticIndexAdoptions
				&& statistics.SelectionChanges == probe.Statistics.SelectionChanges
				&& statistics.TokenSpanAdoptions == probe.Statistics.TokenSpanAdoptions
				&& implementation->AppearanceHost->classification_submissions()
				== probe.ClassificationSubmissions;
		};
		const auto reportIdentity = [&]() {
			const auto  statistics = workbench->statistics();
			QTextStream stream{ stdout };
			stream << "APPEARANCE_THEME_PROBE_IDENTITY_FACTS:"
				   << (workbench->current_result() == probe.Result)
				   << (workbench->current_context() == probe.Context)
				   << (workbench->selection() == probe.Selection)
				   << (workbench->scroll_controller()->position() == probe.Position)
				   << (workbench->token_spans(core::diff_side::left) == probe.LeftSpans)
				   << (workbench->token_spans(core::diff_side::right) == probe.RightSpans)
				   << (statistics.GeometryRebuilds == probe.Statistics.GeometryRebuilds)
				   << (statistics.SemanticIndexAdoptions == probe.Statistics.SemanticIndexAdoptions)
				   << (statistics.SelectionChanges == probe.Statistics.SelectionChanges)
				   << (statistics.TokenSpanAdoptions == probe.Statistics.TokenSpanAdoptions)
				   << (implementation->AppearanceHost->classification_submissions()
					   == probe.ClassificationSubmissions)
				   << '\n';
			stream.flush();
		};
		const auto fail = [&]() {
			reportStage("failed");
			probe.Phase = appearance_theme_probe_state::phase::failed;
			return result::failed;
		};
		// [[BPR-1037]]. Every phase that waits is bounded and names what it
		// was waiting for; see `keep_waiting`.
		const auto wait = [&](const char* const aTerm, const auto& aFacts) {
			return keep_waiting(aApplication, probe.Wait, aTerm, aFacts) ? result::pending : fail();
		};
		const auto present = [](const bool aPresent) {
			return aPresent ? "present" : "absent";
		};
		const auto truth = [](const bool aValue) {
			return aValue ? "true" : "false";
		};
		const auto phaseName = [](const state_type::phase aPhase) {
			switch (aPhase) {
			case state_type::phase::awaiting_comparison:
				return "awaiting_comparison";
			case state_type::phase::awaiting_stable_comparison:
				return "awaiting_stable_comparison";
			case state_type::phase::awaiting_manual_preview:
				return "awaiting_manual_preview";
			case state_type::phase::awaiting_preview_adoption:
				return "awaiting_preview_adoption";
			case state_type::phase::awaiting_preview_restoration:
				return "awaiting_preview_restoration";
			case state_type::phase::awaiting_first_preview:
				return "awaiting_first_preview";
			case state_type::phase::awaiting_second_preview:
				return "awaiting_second_preview";
			case state_type::phase::awaiting_store:
				return "awaiting_store";
			case state_type::phase::awaiting_stored_preview:
				return "awaiting_stored_preview";
			case state_type::phase::awaiting_stored_preview_adoption:
				return "awaiting_stored_preview_adoption";
			case state_type::phase::awaiting_stored_preview_restoration:
				return "awaiting_stored_preview_restoration";
			case state_type::phase::awaiting_equal_stored_preview:
				return "awaiting_equal_stored_preview";
			case state_type::phase::awaiting_equal_stored_commit:
				return "awaiting_equal_stored_commit";
			case state_type::phase::awaiting_export:
				return "awaiting_export";
			case state_type::phase::awaiting_export_refusal:
				return "awaiting_export_refusal";
			case state_type::phase::passed:
				return "passed";
			case state_type::phase::failed:
				return "failed";
			}
			return "unknown";
		};
		// What an imported theme's preview depends on.
		const auto previewFacts = [&](QTextStream& aStream) {
			aStream << "previewing=" << truth(implementation->AppearanceProfile->previewing_palette())
					<< " panel_preview=" << present(panel->preview_palette().has_value())
					<< " profile_generation=" << static_cast<qulonglong>(implementation->AppearanceProfile->generation())
					<< " baseline_generation=" << static_cast<qulonglong>(probe.ProfileGeneration)
					<< " status=\"" << panel->status_text() << '"';
		};

		// Every phase reads the comparison and its spans, and none may run while
		// the appearance host has a request outstanding.
		if (workbench->current_result() == nullptr
			|| !workbench->current_context().has_value()
			|| !workbench->selection().has_value()
			|| !workbench->token_spans(core::diff_side::left)
			|| !workbench->token_spans(core::diff_side::right)
			|| implementation->AppearanceHost->outstanding_requests() != 0U) {
			return wait("APPEARANCE_THEME_COMPARISON_STALLED", [&](QTextStream& aStream) {
				aStream << "phase=" << phaseName(probe.Phase)
						<< " comparison_result=" << present(workbench->current_result() != nullptr)
						<< " comparison_context=" << present(workbench->current_context().has_value())
						<< " comparison_selection=" << present(workbench->selection().has_value())
						<< " left_spans=" << present(static_cast<bool>(workbench->token_spans(core::diff_side::left)))
						<< " right_spans=" << present(static_cast<bool>(workbench->token_spans(core::diff_side::right)))
						<< " appearance_outstanding=" << static_cast<qulonglong>(implementation->AppearanceHost->outstanding_requests());
			});
		}

		// The baselines every later phase compares against are taken the first
		// time the comparison is ready.
		if (probe.Phase == state_type::phase::awaiting_comparison) {
			probe.BeforeSettings    = implementation->AppearanceHost->settings();
			probe.BeforePalette     = implementation->AppearanceProfile->palette();
			probe.NativeObservation = github_application::implementation::
				native_appearance_observation();
			probe.Result            = workbench->current_result();
			probe.Context           = workbench->current_context();
			probe.Selection         = workbench->selection();
			probe.LeftSpans         = workbench->token_spans(core::diff_side::left);
			probe.RightSpans        = workbench->token_spans(core::diff_side::right);
			probe.Statistics        = workbench->statistics();
			probe.Position          = workbench->scroll_controller()->position();
			probe.ProfileGeneration = implementation->AppearanceProfile->generation();
			probe.ClassificationSubmissions
				= implementation->AppearanceHost->classification_submissions();
			probe.PersistedSettings
				= implementation->AppearanceHost->persisted_settings();
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_stable_comparison);
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_stable_comparison) {
			if (!unchanged()) {
				// Which of the comparison's facts moved since the last call,
				// in the order APPEARANCE_THEME_PROBE_IDENTITY_FACTS uses.
				const auto statistics = workbench->statistics();
				const auto same       = std::array{
					workbench->current_result() == probe.Result,
					workbench->current_context() == probe.Context,
					workbench->selection() == probe.Selection,
					workbench->scroll_controller()->position() == probe.Position,
					workbench->token_spans(core::diff_side::left) == probe.LeftSpans,
					workbench->token_spans(core::diff_side::right) == probe.RightSpans,
					statistics.GeometryRebuilds == probe.Statistics.GeometryRebuilds,
					statistics.SemanticIndexAdoptions == probe.Statistics.SemanticIndexAdoptions,
					statistics.SelectionChanges == probe.Statistics.SelectionChanges,
					statistics.TokenSpanAdoptions == probe.Statistics.TokenSpanAdoptions,
					implementation->AppearanceHost->classification_submissions()
						== probe.ClassificationSubmissions,
				};
				probe.Result     = workbench->current_result();
				probe.Context    = workbench->current_context();
				probe.Selection  = workbench->selection();
				probe.LeftSpans  = workbench->token_spans(core::diff_side::left);
				probe.RightSpans = workbench->token_spans(core::diff_side::right);
				probe.Statistics = workbench->statistics();
				probe.Position   = workbench->scroll_controller()->position();
				probe.ClassificationSubmissions
					= implementation->AppearanceHost->classification_submissions();
				return wait("APPEARANCE_THEME_STABLE_COMPARISON_STALLED", [same](QTextStream& aStream) {
					aStream << "unchanged=";
					for (const auto fact : same) {
						aStream << (fact ? '1' : '0');
					}
				});
			}
			const auto role       = core::appearance_role::diff_added_background;
			probe.ManualCandidate = probe.BeforePalette;
			auto& color           = probe.ManualCandidate[core::index_of(role)];
			color.Red             = color.Red == 0U ? 1U : 0U;
			// Through the swatch and the picker seam, which is the accepted path:
			// the picker reports every colour it moves through and then the one it
			// landed on, so one gesture offers the burst of candidates the three
			// preview clicks used to.
			pick_colour(
				*panel,
				role,
				{ QColor{ 0x11, 0x22, 0x33 }, QColor{ 0x44, 0x55, 0x66 } },
				qt::to_presentation_color(color));
			if (implementation->AppearanceProfile->generation() != probe.ProfileGeneration) {
				return fail();
			}
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_manual_preview);
			reportStage("manual-preview-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_manual_preview) {
			const auto statistics = workbench->statistics();
			const auto profilePreviewed
				= implementation->AppearanceProfile->previewing_palette();
			const auto profileMatches = implementation->AppearanceProfile->palette()
				== probe.ManualCandidate;
			const auto panelMatches
				= panel->preview_palette() == std::optional{ probe.ManualCandidate };
			const auto settingsUnchanged
				= implementation->AppearanceHost->settings() == probe.BeforeSettings;
			const auto writesUnchanged
				= implementation->AppearanceHost->persisted_settings()
				== probe.PersistedSettings;
			const auto generationChanged
				= implementation->AppearanceProfile->generation()
				== probe.ProfileGeneration + 1U;
			const auto paintChanged = statistics.AlignmentSegmentsVisited
				> probe.Statistics.AlignmentSegmentsVisited;
			const auto identityUnchanged = unchanged();
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_MANUAL_FACTS:"
					   << profilePreviewed << profileMatches << panelMatches
					   << settingsUnchanged << writesUnchanged << generationChanged
					   << paintChanged << identityUnchanged << '\n';
				stream.flush();
			}
			if (!profilePreviewed || !profileMatches || !panelMatches
				|| !settingsUnchanged || !writesUnchanged || !generationChanged
				|| !paintChanged || !identityUnchanged) {
				return fail();
			}
			reportStage("manual-preview");
			auto alternateObservation = probe.NativeObservation;
			alternateObservation.Scheme
				= alternateObservation.Scheme == core::os_color_scheme::dark
				? core::os_color_scheme::light
				: core::os_color_scheme::dark;
			implementation->AppearanceHost->observe(alternateObservation);
			implementation->AppearanceProfile->observe(alternateObservation);
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_preview_adoption);
			reportStage("unrelated-adoption-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_preview_adoption) {
			const auto adoptionFacts
				= std::array{
					  implementation->AppearanceProfile->previewing_palette(),
					  implementation->AppearanceProfile->palette()
						  == probe.ManualCandidate,
					  panel->preview_palette()
						  == std::optional{ probe.ManualCandidate },
					  panel->preview_bar() != nullptr,
					  panel->keep_changes_button() != nullptr,
					  implementation->AppearanceHost->settings()
						  == probe.BeforeSettings,
					  implementation->AppearanceHost->persisted_settings()
						  == probe.PersistedSettings,
					  unchanged(),
				  };
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_ADOPTION_FACTS:";
				for (const auto fact : adoptionFacts) {
					stream << fact;
				}
				stream << '\n';
				stream.flush();
			}
			if (!std::ranges::all_of(adoptionFacts, std::identity{ })) {
				return fail();
			}
			implementation->AppearanceHost->observe(probe.NativeObservation);
			implementation->AppearanceProfile->observe(probe.NativeObservation);
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_preview_restoration);
			reportStage("native-observation-restored");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_preview_restoration) {
			if (!implementation->AppearanceProfile->previewing_palette()
				|| implementation->AppearanceProfile->palette()
					!= probe.ManualCandidate
				|| panel->preview_palette()
					!= std::optional{ probe.ManualCandidate }
				|| panel->preview_bar() == nullptr
				|| panel->keep_changes_button() == nullptr
				|| implementation->AppearanceHost->settings()
					!= probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings
				|| !unchanged()) {
				return fail();
			}
			implementation->Window->settings()->close();
			if (implementation->AppearanceProfile->previewing_palette()
				|| panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette()
					!= probe.BeforePalette
				|| implementation->AppearanceHost->settings() != probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings
				|| !unchanged()) {
				return fail();
			}
			reportStage("manual-cancelled");
			implementation->Window->open_settings(qt::settings_page::appearance);
			pick_colour(
				*panel,
				core::appearance_role::diff_added_background,
				{ },
				QColor{ 0x11, 0x22, 0x33 });
			implementation->Window->settings()->close();
			QCoreApplication::sendPostedEvents(panel, QEvent::MetaCall);
			if (implementation->AppearanceProfile->previewing_palette()
				|| panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette() != probe.BeforePalette) {
				return fail();
			}
			implementation->Window->open_settings(qt::settings_page::appearance);
			pick_colour(
				*panel,
				core::appearance_role::diff_added_background,
				{ },
				QColor{ 0x11, 0x22, 0x33 });
			if (implementation->AppearanceProfile->previewing_palette() || !panel->preview_palette()) {
				return fail();
			}
			auto* reset = static_cast<QPushButton*>(nullptr);
			for (auto* button : panel->findChildren<QPushButton*>()) {
				if (button->accessibleName() == QStringLiteral("Reset all colours")) {
					reset = button;
				}
			}
			if (reset == nullptr) {
				return fail();
			}
			reset->click();
			QCoreApplication::sendPostedEvents(panel, QEvent::MetaCall);
			if (implementation->AppearanceProfile->previewing_palette() || panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette() != probe.BeforePalette
				|| implementation->AppearanceHost->settings() != probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings() != probe.PersistedSettings) {
				return fail();
			}
			reportStage("palette-reset-cleared-preview");
			probe.Statistics = workbench->statistics();
			probe.ProfileGeneration
				= implementation->AppearanceProfile->generation();
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_first_preview);
			panel->request_theme_import(QString::fromStdString(probe.Path));
			reportStage("import-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_first_preview) {
			if (!implementation->AppearanceProfile->previewing_palette()) {
				if (panel->status_text().contains(QStringLiteral("refused"), Qt::CaseInsensitive)
					|| panel->status_text().contains(QStringLiteral("failed"), Qt::CaseInsensitive)) {
					return fail();
				}
				return wait("APPEARANCE_THEME_FIRST_PREVIEW_STALLED", previewFacts);
			}
			if (!panel->preview_palette().has_value()) {
				return fail();
			}
			probe.Candidate = implementation->AppearanceProfile->palette();
			reportStage("first-preview");
			const auto statistics       = workbench->statistics();
			const auto candidateChanged = probe.Candidate != probe.BeforePalette;
			const auto panelMatches     = *panel->preview_palette() == probe.Candidate;
			const auto settingsUnchanged
				= implementation->AppearanceHost->settings() == probe.BeforeSettings;
			const auto writesUnchanged
				= implementation->AppearanceHost->persisted_settings()
				== probe.PersistedSettings;
			const auto generationChanged
				= implementation->AppearanceProfile->generation()
				> probe.ProfileGeneration;
			const auto paintChanged = statistics.SyntaxBlocksFormatted
				> probe.Statistics.SyntaxBlocksFormatted;
			const auto identityUnchanged = unchanged();
			reportIdentity();
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_FIRST_FACTS:"
					   << candidateChanged << panelMatches << settingsUnchanged
					   << writesUnchanged << generationChanged << paintChanged
					   << identityUnchanged << '\n';
				stream.flush();
			}
			if (!candidateChanged || !panelMatches || !settingsUnchanged
				|| !writesUnchanged || !generationChanged || !paintChanged
				|| !identityUnchanged) {
				return fail();
			}
			panel->cancel_changes_button()->click();
			reportStage("cancelled");
			if (implementation->AppearanceProfile->previewing_palette()
				|| panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette()
					!= probe.BeforePalette
				|| implementation->AppearanceHost->settings() != probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings
				|| !unchanged()) {
				return fail();
			}
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_second_preview);
			panel->request_theme_import(QString::fromStdString(probe.Path));
			reportStage("second-import-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_second_preview) {
			if (!implementation->AppearanceProfile->previewing_palette()) {
				return wait("APPEARANCE_THEME_SECOND_PREVIEW_STALLED", previewFacts);
			}
			if (!panel->preview_palette().has_value()
				|| *panel->preview_palette() != probe.Candidate
				|| implementation->AppearanceHost->settings() != probe.BeforeSettings
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings
				|| !unchanged()) {
				return fail();
			}
			reportStage("second-preview");
			probe.CustomisedVariant = implementation->AppearanceProfile->variant();
			panel->keep_changes_button()->click();
			reportStage("commit-dispatched");
			const auto committed   = application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant);
			const auto commitFacts = std::array{
				!implementation->AppearanceProfile->previewing_palette(),
				!panel->preview_palette().has_value(),
				implementation->AppearanceProfile->palette() == probe.Candidate,
				committed.Palette == probe.Candidate,
				committed.Stored,
				unchanged(),
			};
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_COMMIT_FACTS:";
				for (const auto fact : commitFacts) {
					stream << fact;
				}
				stream << '\n';
				// [[BPR-1479]]. The theme the colours were kept on, which the
				// export names as its base.
				const auto variant = core::stable_name(probe.CustomisedVariant);
				stream << "APPEARANCE_THEME_PROBE_CUSTOMISED_VARIANT:"
					   << QString::fromUtf8(variant.data(), static_cast<qsizetype>(variant.size())) << '\n';
				// A refused commit says why only in the panel, as a contrast
				// rejection naming each role.
				if (!std::ranges::all_of(commitFacts, std::identity{ })) {
					stream << "APPEARANCE_THEME_PROBE_COMMIT_STATUS:" << panel->status_text().simplified() << '\n';
				}
				stream.flush();
			}
			if (!std::ranges::all_of(commitFacts, std::identity{ })) {
				reportIdentity();
				return fail();
			}
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_store);
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_store) {
			if (implementation->AppearanceHost->persisted_settings()
				== probe.PersistedSettings) {
				return wait("APPEARANCE_THEME_STORE_STALLED", [&](QTextStream& aStream) {
					aStream << "persisted_settings=" << static_cast<qulonglong>(implementation->AppearanceHost->persisted_settings())
							<< " expected_persisted_settings=" << static_cast<qulonglong>(probe.PersistedSettings) + 1U
							<< " stored=" << truth(application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Stored)
							<< " previewing=" << truth(implementation->AppearanceProfile->previewing_palette())
							<< " panel_preview=" << present(panel->preview_palette().has_value());
				});
			}
			if (implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| !unchanged()) {
				return fail();
			}
			reportStage("persisted");
			probe.StoredCandidate = probe.Candidate;
			const auto role       = core::appearance_role::diff_removed_background;
			auto&      color      = probe.StoredCandidate[core::index_of(role)];
			color.Blue            = color.Blue == 0U ? 1U : 0U;
			pick_colour(*panel, role, { }, qt::to_presentation_color(color));
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_stored_preview);
			reportStage("stored-palette-preview-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_stored_preview) {
			if (!implementation->AppearanceProfile->previewing_palette()
				|| implementation->AppearanceProfile->palette()
					!= probe.StoredCandidate
				|| panel->preview_palette()
					!= std::optional{ probe.StoredCandidate }
				|| application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| !unchanged()) {
				return fail();
			}
			auto alternateObservation = probe.NativeObservation;
			alternateObservation.Scheme
				= alternateObservation.Scheme == core::os_color_scheme::dark
				? core::os_color_scheme::light
				: core::os_color_scheme::dark;
			implementation->AppearanceHost->observe(alternateObservation);
			implementation->AppearanceProfile->observe(alternateObservation);
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_stored_preview_adoption);
			reportStage("stored-palette-adoption-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_stored_preview_adoption) {
			const auto adoptionFacts = std::array{
				implementation->AppearanceProfile->previewing_palette(),
				implementation->AppearanceProfile->palette()
					== probe.StoredCandidate,
				application::customisation_of(implementation->AppearanceProfile->settings(), probe.CustomisedVariant).Palette
					== probe.Candidate,
				panel->preview_palette() == std::optional{ probe.StoredCandidate },
				panel->preview_bar() != nullptr,
				panel->keep_changes_button() != nullptr,
				application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette
					== probe.Candidate,
				implementation->AppearanceHost->persisted_settings()
					== probe.PersistedSettings + 1U,
				unchanged(),
			};
			{
				QTextStream stream{ stdout };
				stream << "APPEARANCE_THEME_PROBE_STORED_ADOPTION_FACTS:";
				for (const auto fact : adoptionFacts) {
					stream << fact;
				}
				stream << '\n';
				stream.flush();
			}
			if (!std::ranges::all_of(adoptionFacts, std::identity{ })) {
				return fail();
			}
			implementation->AppearanceHost->observe(probe.NativeObservation);
			implementation->AppearanceProfile->observe(probe.NativeObservation);
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_stored_preview_restoration);
			reportStage("stored-palette-observation-restored");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_stored_preview_restoration) {
			if (!implementation->AppearanceProfile->previewing_palette()
				|| implementation->AppearanceProfile->palette()
					!= probe.StoredCandidate
				|| application::customisation_of(implementation->AppearanceProfile->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| panel->preview_palette()
					!= std::optional{ probe.StoredCandidate }
				|| panel->preview_bar() == nullptr
				|| panel->keep_changes_button() == nullptr
				|| application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| !unchanged()) {
				return fail();
			}
			panel->cancel_changes_button()->click();
			if (implementation->AppearanceProfile->previewing_palette()
				|| panel->preview_palette().has_value()
				|| implementation->AppearanceProfile->palette() != probe.Candidate
				|| application::customisation_of(implementation->AppearanceProfile->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| !unchanged()) {
				return fail();
			}
			reportStage("stored-palette-preview-cancelled");
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_equal_stored_preview);
			panel->request_theme_import(QString::fromStdString(probe.Path));
			reportStage("equal-stored-import-requested");
			return result::pending;
		}

		if (probe.Phase
			== appearance_theme_probe_state::
				phase::awaiting_equal_stored_preview) {
			if (!implementation->AppearanceProfile->previewing_palette()
				|| implementation->AppearanceProfile->palette() != probe.Candidate
				|| panel->preview_palette() != std::optional{ probe.Candidate }
				|| panel->preview_bar() == nullptr
				|| panel->keep_changes_button() == nullptr
				|| application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette
					!= probe.Candidate
				|| implementation->AppearanceHost->persisted_settings()
					!= probe.PersistedSettings + 1U
				|| !unchanged()) {
				return fail();
			}
			panel->keep_changes_button()->click();
			enter_phase(probe.Phase, probe.Wait, state_type::phase::awaiting_equal_stored_commit);
			reportStage("equal-stored-commit-dispatched");
			return result::pending;
		}

		const auto equalCommitFacts = std::array{
			!implementation->AppearanceProfile->previewing_palette(),
			implementation->AppearanceProfile->palette() == probe.Candidate,
			application::customisation_of(implementation->AppearanceProfile->settings(), probe.CustomisedVariant).Palette == probe.Candidate,
			!panel->preview_palette().has_value(),
			panel->preview_bar() == nullptr,
			panel->keep_changes_button() == nullptr,
			application::customisation_of(implementation->AppearanceHost->settings(), probe.CustomisedVariant).Palette == probe.Candidate,
			implementation->AppearanceHost->persisted_settings()
				== probe.PersistedSettings + 1U,
			unchanged(),
		};
		{
			QTextStream stream{ stdout };
			stream << "APPEARANCE_THEME_PROBE_EQUAL_COMMIT_FACTS:";
			for (const auto fact : equalCommitFacts) {
				stream << fact;
			}
			stream << '\n';
			stream.flush();
		}
		if (!std::ranges::all_of(equalCommitFacts, std::identity{ })) {
			return fail();
		}
		using phase = appearance_theme_probe_state::phase;
		if (probe.Phase == phase::awaiting_equal_stored_commit) {
			enter_phase(probe.Phase, probe.Wait, phase::awaiting_export);
			panel->request_theme_export(QString::fromStdString(probe.Path + ".exported.bprtheme"));
			return result::pending;
		}
		if (probe.Phase == phase::awaiting_export) {
			if (!panel->status_text().startsWith(QStringLiteral("Saved theme exported."))) {
				return fail();
			}
			enter_phase(probe.Phase, probe.Wait, phase::awaiting_export_refusal);
			// The existing source's parent directory is a nonregular destination.
			panel->request_theme_export(QFileInfo{ QString::fromStdString(probe.Path) }.absolutePath());
			return result::pending;
		}
		if (!panel->status_text().startsWith(QStringLiteral("Theme export failed."))) {
			return fail();
		}
		reportStage("export-and-directory-refusal-acknowledged");
		probe.Phase = phase::passed;
		reportStage("equal-stored-commit-acknowledged");
		return result::passed;
	}
#endif
}
