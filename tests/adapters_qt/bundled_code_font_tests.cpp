/// \file
/// \brief [[BPR-1270]]. The bundled JetBrains Mono NL is the default
/// code font: it loads and resolves fixed-pitch, a fresh profile codes in it, a
/// saved code font is kept, a failed load falls back to the platform's fixed
/// font, and the cards' excerpt follows the profile's code font.

#include "bpr/application/appearance_preferences_application_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/bundled_code_font.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/github_conversation_panel.hpp"

#include <QApplication>
#include <QChar>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QString>

#include <cstdlib>
#include <expected>
#include <iostream>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto family_of(const core::code_font_settings& aCode) -> QString
	{
		return QString::fromStdString(aCode.Font.Family);
	}

	/// \brief Whether every printable ASCII character advances by the same width.
	///
	/// \details What the comparison panes rely on when they line code up in
	/// columns. A proportional face fails it, which the negative control below
	/// shows.
	[[nodiscard]] auto advances_on_one_grid(const QFont& aFont) -> bool
	{
		const auto metrics = QFontMetricsF{ aFont };
		const auto cell    = metrics.horizontalAdvance(QLatin1Char{ 'M' });
		for (auto code = 0x20; code <= 0x7E; ++code) {
			if (metrics.horizontalAdvance(QChar{ code }) != cell) {
				return false;
			}
		}
		return cell > 0.0;
	}

	/// \brief Stands in for the durable appearance record.
	class fake_settings final : public application::appearance_settings_port
	{
	public:
		[[nodiscard]] auto load(const std::stop_token)
			-> application::appearance_settings_result<std::optional<application::appearance_settings>> override
		{
			return Stored;
		}

		[[nodiscard]] auto store(const application::appearance_settings& aSettings, const std::stop_token)
			-> application::appearance_settings_result<void> override
		{
			++Stores;
			Stored = aSettings;
			return { };
		}

		std::optional<application::appearance_settings> Stored;
		std::size_t                                     Stores = 0U;
	};

	class no_theme_documents final : public application::theme_document_port
	{
	public:
		[[nodiscard]] auto read(const std::string_view, const std::size_t, const std::stop_token)
			-> application::theme_document_result<std::string> override
		{
			return std::unexpected{ application::theme_document_error{ } };
		}

		[[nodiscard]] auto write(const std::string_view, const std::string_view, const std::stop_token)
			-> application::theme_document_result<void> override
		{
			return std::unexpected{ application::theme_document_error{ } };
		}
	};

	class ignored_projection final : public application::appearance_projection_port
	{
	public:
		void publish(const application::appearance_projection&) override { }
		void fail(const application::appearance_failure_publication&) override { }
	};

	/// \brief The composition's appearance owner over one durable record.
	///
	/// \details Composed exactly as `github_application.cpp` composes it: with
	/// `qt::default_appearance_settings()` as the fresh-installation record.
	struct composed_preferences
	{
		fake_settings                                                         Settings;
		no_theme_documents                                                    Documents;
		application::manual_executor                                          Effects;
		application::manual_executor                                          Lane;
		std::shared_ptr<application::appearance_preferences_application_host> Host;
		application::appearance_consumer_id                                   Consumer{ };

		explicit composed_preferences(std::optional<application::appearance_settings> aStored)
		{
			Settings.Stored = std::move(aStored);
			Host            = std::make_shared<application::appearance_preferences_application_host>(
				qt::default_appearance_settings(), Settings, Documents, Effects, Lane, core::application_session_id{ 1U });
			const auto attached = Host->attach(std::make_shared<ignored_projection>());
			expect(attached.has_value(), "the composed appearance owner accepts a window");
			Consumer = *attached;
			Host->restore(Consumer);
			settle();
		}

		composed_preferences(const composed_preferences&)                    = delete;
		auto operator=(const composed_preferences&) -> composed_preferences& = delete;

		~composed_preferences()
		{
			Host->shutdown();
			settle();
		}

		void settle()
		{
			while (Effects.pending() || Lane.pending()) {
				Effects.drain();
				Lane.drain();
			}
		}
	};

	// Before anything registers: a registration that fails leaves the font
	// database as it found it, and the default falls back to the platform's
	// fixed font.
	void test_a_failed_registration_falls_back_to_the_platform_fixed_font()
	{
		const auto installed = qt::bundled_code_font_available();
		const auto family    = qt::bundled_code_font_family();
		expect(!qt::register_code_font_files({ }, family), "no files is not a registration");
		expect(!qt::register_code_font_files({ QStringLiteral(":/bpr/fonts/absent.ttf") }, family),
			   "a face that is not there is refused");
		expect(!qt::register_code_font_files(qt::bundled_code_font_files(), QStringLiteral("Not The Bundled Family")),
			   "faces that name another family are refused");
		expect(!qt::register_code_font_files({ qt::bundled_code_font_files().front(), QStringLiteral(":/bpr/fonts/absent.ttf") }, family),
			   "a registration that fails part way is refused");
		// Meaningful wherever the family is not installed system-wide; where
		// it is, the database resolves it either way.
		expect(qt::bundled_code_font_available() == installed,
			   "a refused registration removes every face it added");

		const auto fallback = qt::default_typography_settings(false);
		const auto platform = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		expect(core::validate_typography(fallback).empty(), "the fallback typography is legal");
		expect(family_of(fallback.Code) == platform.family() && family_of(fallback.Code) != family,
			   "without the bundled family the default code font is the platform's fixed font");
		expect(fallback.Ui == qt::default_typography_settings(true).Ui,
			   "the bundled code font changes nothing about the UI font");
	}

	void test_the_bundled_faces_load_and_resolve_fixed_pitch()
	{
		const auto family = qt::bundled_code_font_family();
		expect(qt::bundled_code_font_files().size() == 4, "four faces are bundled");
		expect(qt::register_bundled_code_font(), "the bundled faces register with the font database");
		expect(qt::bundled_code_font_available(), "the font database resolves the bundled family");

		for (const auto bold : { false, true }) {
			for (const auto italic : { false, true }) {
				auto font = QFont{ family };
				font.setPointSizeF(12.0);
				font.setBold(bold);
				font.setItalic(italic);
				const auto resolved = QFontInfo{ font };
				expect(resolved.family() == family, "each face resolves to the bundled family, not a substitute");
				expect(resolved.bold() == bold && resolved.italic() == italic,
					   "regular, bold, italic and bold italic each resolve to their own face");
				expect(resolved.fixedPitch(), "the font database reports the bundled face fixed-pitch");
				expect(advances_on_one_grid(font), "every printable ASCII character advances one cell");
			}
		}
		auto regular = QFont{ family };
		regular.setPointSizeF(12.0);
		auto bold = regular;
		bold.setBold(true);
		expect(QFontMetricsF{ regular }.horizontalAdvance(QLatin1Char{ 'M' }) == QFontMetricsF{ bold }.horizontalAdvance(QLatin1Char{ 'M' }),
			   "syntax bold keeps the regular face's column grid");

		// Negative control: the grid check fails a proportional face.
		auto proportional = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
		proportional.setPointSizeF(12.0);
		expect(!advances_on_one_grid(proportional), "the grid check refuses a proportional face");
	}

	void test_a_fresh_profile_codes_in_the_bundled_family()
	{
		const auto family   = qt::bundled_code_font_family();
		const auto defaults = qt::default_typography_settings();
		expect(core::validate_typography(defaults).empty(), "the fresh typography is legal by the core's own validator");
		expect(family_of(defaults.Code) == family, "a fresh profile's code font is the bundled family");
		expect(defaults == qt::default_typography_settings(true), "the default is the bundled case once it is registered");
		expect(qt::default_appearance_settings().Typography == defaults, "a fresh installation starts from that typography");
		const auto platform = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		expect(qt::to_presentation_font(defaults.Code).pointSizeF() == platform.pointSizeF(),
			   "at the platform's own code size");

		const auto profile = qt::appearance_profile{ };
		expect(profile.code_font().family() == family && QFontInfo{ profile.code_font() }.family() == family,
			   "a profile with no durable record applies and resolves the bundled family");

		// The composed owner with no durable record keeps the fresh default and
		// writes nothing.
		auto fresh = composed_preferences{ std::nullopt };
		expect(family_of(fresh.Host->settings().Typography.Code) == family, "no durable record: the composed default stands");
		expect(fresh.Settings.Stores == 0U, "an untouched fresh profile is not written");
	}

	void test_a_saved_code_font_is_kept()
	{
		const auto family = qt::bundled_code_font_family();
		// A profile stored before this change: its code family is the platform
		// fixed font the old default wrote, beside a stored theme choice. The
		// store keeps one whole record, so it cannot say whether the family was
		// chosen; it is kept either way.
		auto saved          = qt::default_typography_settings(false);
		auto record         = qt::default_appearance_settings();
		record.Typography   = saved;
		record.Mode         = core::appearance_mode::dark;
		auto       restored = composed_preferences{ record };
		const auto kept     = restored.Host->settings().Typography.Code;
		expect(kept == saved.Code && family_of(kept) != family, "a saved code font is never replaced by the bundled default");
		expect(restored.Settings.Stores == 0U, "restoring rewrites nothing");
		expect(qt::appearance_profile{ restored.Host->settings() }.code_font().family() == family_of(saved.Code),
			   "the profile applies the saved family");

		// Any other family the reviewer chose, likewise.
		record.Typography.Code.Font.Family = "Courier New";
		auto chosen                        = composed_preferences{ record };
		expect(family_of(chosen.Host->settings().Typography.Code) == QStringLiteral("Courier New"), "a chosen family is kept");

		// Resetting the code font is the one gesture that takes the new default.
		chosen.Host->reset(chosen.Consumer, application::appearance_reset_scope::code_font, core::appearance_variant::light);
		chosen.settle();
		expect(family_of(chosen.Host->settings().Typography.Code) == family && chosen.Settings.Stored.has_value()
				   && family_of(chosen.Settings.Stored->Typography.Code) == family,
			   "reset code font takes and stores the bundled default");
	}

	void test_the_cards_excerpt_follows_the_profile_code_font()
	{
		const auto defaults = qt::to_presentation_font(qt::default_typography_settings().Code);
		auto       view     = qt::conversation_cards_view{ };
		expect(view.code_font() == defaults, "before a profile arrives the excerpt uses the default typography's code font");

		auto record                                      = qt::default_appearance_settings();
		record.Typography.Code.Font.Family               = "Courier New";
		record.Typography.Code.Font.PointSizeCentipoints = 1'700U;
		const auto profile                               = qt::appearance_profile{ record };
		auto       panel                                 = qt::github_conversation_panel{ };
		panel.adopt_appearance(profile);
		expect(panel.cards()->code_font() == profile.code_font(),
			   "the conversation panel hands the cards the profile's code font");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(QStringLiteral("BetterPullReviewBundledCodeFontTests"));
	test_a_failed_registration_falls_back_to_the_platform_fixed_font();
	test_the_bundled_faces_load_and_resolve_fixed_pitch();
	test_a_fresh_profile_codes_in_the_bundled_family();
	test_a_saved_code_font_is_kept();
	test_the_cards_excerpt_follows_the_profile_code_font();
	std::cout << "bundled-code-font-tests:pass\n";
	return EXIT_SUCCESS;
}
