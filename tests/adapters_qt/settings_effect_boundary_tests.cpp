/// \file
/// \brief Records capability-port thread entry from composed Settings gestures.
#include "bpr/application/appearance_application_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>

#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
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

	/// \brief Records actual caller identity at the supplied capability boundary.
	struct capability_log
	{
		void enter(const std::string_view aOperation)
		{
			const auto caller = std::this_thread::get_id();
			expect(caller != Gui, "capability entry must run outside the GUI thread");
			Calls.emplace_back(aOperation);
			Threads.push_back(caller);
		}

		const std::thread::id        Gui = std::this_thread::get_id();
		std::vector<std::string>     Calls;
		std::vector<std::thread::id> Threads;
	};

	/// \brief Defers all work until the test explicitly runs a worker outside gesture dispatch.
	class worker_executor final : public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			Tasks.push_back(std::move(aTask));
		}

		[[nodiscard]] auto pending() const -> std::size_t { return Tasks.size(); }

		void run_one()
		{
			expect(Tasks.size() == 1, "one effect was queued by the gesture");
			auto task = std::move(Tasks.front());
			Tasks.pop_front();
			// This join belongs only to test orchestration after the gesture has
			// returned. Production dispatch and effect post never execute or wait.
			auto worker = std::thread{ std::move(task) };
			worker.join();
		}

	private:
		std::deque<std::function<void()>> Tasks;
	};

	class queue_store final : public application::queue_enrollment_store_port
	{
	public:
		explicit queue_store(capability_log& aLog)
			: Log{ aLog }
		{ }
		auto load(std::stop_token) -> std::expected<application::durable_queue_state, application::queue_store_failure> override
		{
			Log.enter("queue.load");
			return State;
		}
		auto store_preferences(const application::queue_presentation_preferences& aPreferences, std::stop_token)
			-> std::expected<void, application::queue_store_failure> override
		{
			Log.enter("queue.store_preferences");
			State.Preferences = aPreferences;
			return { };
		}
		auto apply_rule_change(const application::queue_rule_intent&, std::span<const application::enrollment_record>, std::stop_token)
			-> std::expected<void, application::queue_store_failure> override
		{
			expect(false, "preference gesture must not change rules");
			return { };
		}
		auto store_enrollment(const application::enrollment_record&, std::stop_token)
			-> std::expected<void, application::queue_store_failure> override
		{
			expect(false, "preference gesture must not store enrollment");
			return { };
		}
		auto apply_saved_repository_change(const application::saved_repository_intent&, std::stop_token)
			-> std::expected<void, application::queue_store_failure> override
		{
			expect(false, "preference gesture must not store repositories");
			return { };
		}
		auto apply_saved_server_change(const application::saved_server_intent&, std::stop_token)
			-> std::expected<void, application::queue_store_failure> override
		{
			expect(false, "preference gesture must not store servers");
			return { };
		}
		auto apply_home_ignores(std::span<const core::home_ignore_delta>, std::stop_token)
			-> std::expected<void, application::queue_store_failure> override
		{
			expect(false, "preference gesture must not store ignores");
			return { };
		}
		application::durable_queue_state State;

	private:
		capability_log& Log;
	};

	class unused_refresh final : public application::queue_refresh_effect_port
	{
	public:
		auto refresh(const application::queue_refresh_request&, std::stop_token) -> application::queue_refresh_completion override
		{
			expect(false, "Settings preference gesture must not refresh a provider");
			return { };
		}
	};

	class queue_projection final : public application::queue_landing_projection_port
	{
	public:
		queue_projection(qt::workbench_window& aWindow, const std::thread::id aGui)
			: Window{ aWindow }
			, Gui{ aGui }
		{ }
		void publish(const application::queue_landing_projection& aProjection) override
		{
			expect(std::this_thread::get_id() == Gui, "queue projection returns on the GUI decision lane");
			++Publications;
			Window.publish(aProjection);
		}
		std::size_t Publications = 0;

	private:
		qt::workbench_window& Window;
		std::thread::id       Gui;
	};

	class appearance_store final : public application::appearance_settings_port
	{
	public:
		explicit appearance_store(capability_log& aLog)
			: Log{ aLog }
		{ }
		auto load(std::stop_token) -> application::appearance_settings_result<std::optional<application::appearance_settings>> override
		{
			Log.enter("appearance.load");
			return Stored;
		}
		auto store(const application::appearance_settings& aSettings, std::stop_token) -> application::appearance_settings_result<void> override
		{
			Log.enter("appearance.store");
			Stored = aSettings;
			return { };
		}
		std::optional<application::appearance_settings> Stored;

	private:
		capability_log& Log;
	};

	[[nodiscard]] auto contrast_safe_palette() -> core::appearance_palette
	{
		auto palette = core::appearance_palette{ };
		palette.fill(core::appearance_color{ .Alpha = 255U });
		for (const auto& pair : core::required_contrast_pairs()) {
			palette[core::index_of(pair.Background)] = core::appearance_color{ .Alpha = 255U };
			if (pair.Wash.has_value()) {
				palette[core::index_of(*pair.Wash)] = core::appearance_color{ .Alpha = 255U };
			}
			palette[core::index_of(pair.Foreground)] = { .Red = 255U, .Green = 255U, .Blue = 255U, .Alpha = 255U };
		}
		return palette;
	}

	class documents final : public application::theme_document_port
	{
	public:
		explicit documents(capability_log& aLog)
			: Log{ aLog }
		{ }
		auto read(const std::string_view aPath, const std::size_t aMaximumBytes, std::stop_token) -> application::theme_document_result<std::string> override
		{
			Log.enter("document.read");
			expect(aPath == "fixture.theme", "the selected import path reaches the document port");
			expect(aMaximumBytes == core::theme_document_byte_limit + 1U, "import read retains the parser overflow byte bound");
			return core::export_theme(contrast_safe_palette());
		}
		auto write(const std::string_view aPath, const std::string_view aDocument, std::stop_token) -> application::theme_document_result<void> override
		{
			Log.enter("document.write");
			expect(aPath == "export.theme" && !aDocument.empty(), "export reaches the selected document port with serialized content");
			return { };
		}

	private:
		capability_log& Log;
	};

	class unused_classifier final : public application::syntax_classification_port
	{
	public:
		auto classify(const application::syntax_classification_request&, std::stop_token) -> application::syntax_classification override
		{
			expect(false, "appearance gesture must not reclassify syntax");
			return { };
		}
	};

	class appearance_projection final : public application::appearance_projection_port
	{
	public:
		appearance_projection(qt::appearance_settings_panel& aPanel, const std::thread::id aGui)
			: Panel{ aPanel }
			, Gui{ aGui }
		{ }
		void publish(const application::appearance_projection& aProjection) override
		{
			expect(std::this_thread::get_id() == Gui, "appearance publication returns on the GUI decision lane");
			++Publications;
			if (aProjection.ThemePreview) {
				Panel.preview_import(aProjection.ThemePreview->Palette);
			}
			if (aProjection.ThemeExported) {
				++Exports;
				Panel.report_theme_export(true);
			}
		}
		void fail(const application::appearance_failure_publication&) override
		{
			expect(false, "covered appearance gestures must not fail");
		}
		std::size_t Publications = 0;
		std::size_t Exports      = 0;

	private:
		qt::appearance_settings_panel& Panel;
		std::thread::id                Gui;
	};

	void dispatch_appearance(application::appearance_application_host& aHost, application::appearance_intent aIntent)
	{
		std::visit([&aHost](auto&& aValue) {
			using intent = std::decay_t<decltype(aValue)>;
			if constexpr (std::is_same_v<intent, application::select_appearance_mode_intent>) {
				aHost.select(aValue.Mode);
			} else if constexpr (std::is_same_v<intent, application::import_appearance_theme_intent>) {
				aHost.import_theme_file(std::move(aValue.Path));
			} else if constexpr (std::is_same_v<intent, application::export_appearance_theme_intent>) {
				aHost.export_theme_file(std::move(aValue.Path), application::customisation_of(aHost.settings(), aHost.resolution().Variant).Palette, aHost.resolution().Variant);
			} else if constexpr (std::is_same_v<intent, application::cancel_appearance_palette_preview_intent>) {
				aHost.cancel_theme_import();
			} else {
				expect(false, "fixture receives only its named Settings gestures");
			}
		},
				   std::move(aIntent));
	}

	void test_queue_preference_effect_boundary()
	{
		auto window     = qt::workbench_window{ };
		auto log        = capability_log{ };
		auto effects    = worker_executor{ };
		auto lane       = application::manual_executor{ };
		auto store      = queue_store{ log };
		auto refresh    = unused_refresh{ };
		auto projection = queue_projection{ window, log.Gui };
		auto host       = application::review_queue_application_host{ store, refresh, effects, lane, projection, { } };
		window.set_queue_preferences_dispatch([&host](application::queue_preferences_intent aIntent) { host.apply(std::move(aIntent)); });
		host.restore();
		expect(log.Calls.empty(), "restore dispatch queues the database read");
		effects.run_one();
		lane.drain();
		window.settings()->present(qt::settings_page::notifications);
		auto* const paused = window.settings()->findChild<QCheckBox*>(QStringLiteral("notificationsPaused"));
		expect(paused != nullptr && paused->isEnabled(), "the live Settings notification preference is usable");
		const auto calls        = log.Calls.size();
		const auto publications = projection.Publications;
		paused->click();
		expect(log.Calls.size() == calls && effects.pending() == 1 && !host.preferences().Notifications.Paused, "queue gesture returns without database entry or premature adoption");
		effects.run_one();
		expect(log.Calls.back() == "queue.store_preferences" && store.State.Preferences.Notifications.Paused, "worker writes the selected preference");
		expect(projection.Publications == publications && !host.preferences().Notifications.Paused && lane.pending() == 1, "worker completion cannot publish before decision-lane drain");
		lane.drain();
		expect(host.preferences().Notifications.Paused && paused->isChecked() && projection.Publications == publications + 1, "decision lane adopts and projects the durable preference");
		expect(host.is_quiescent(), "queue host drains its effect lifecycle");
		window.set_queue_preferences_dispatch({ });
	}

	void test_appearance_effect_boundaries()
	{
		auto        window         = qt::workbench_window{ };
		auto* const panel          = window.appearance_settings();
		auto        log            = capability_log{ };
		auto        effects        = worker_executor{ };
		auto        lane           = application::manual_executor{ };
		auto        store          = appearance_store{ log };
		auto        themeDocuments = documents{ log };
		auto        classifier     = unused_classifier{ };
		auto        projection     = appearance_projection{ *panel, log.Gui };
		auto        host           = application::appearance_application_host{ qt::default_appearance_settings(), qt::builtin_appearance_palettes(), store, themeDocuments, classifier, effects, lane, projection, core::application_session_id{ 1U } };
		panel->adopt(host.settings());
		panel->set_dispatch([&host](application::appearance_intent aIntent) { dispatch_appearance(host, std::move(aIntent)); });
		window.settings()->present(qt::settings_page::appearance);
		// [[BPR-0790]]. The tiles are the mode control now; `choose_mode` is the
		// gesture choosing one performs, and the six-way combo box is gone.
		panel->choose_mode(core::appearance_mode::dark);
		expect(log.Calls.empty() && effects.pending() == 1 && host.persisted_settings() == 0, "appearance mode gesture queues persistence without capability entry");
		// Appearance intentionally adopts session settings before durability; only
		// its durable completion must wait for the decision lane.
		const auto sessionPublications = projection.Publications;
		effects.run_one();
		expect(log.Calls.back() == "appearance.store" && host.persisted_settings() == 0 && projection.Publications == sessionPublications, "worker store cannot acknowledge durability inline");
		lane.drain();
		expect(host.persisted_settings() == 1 && store.Stored->Mode == core::appearance_mode::dark, "decision lane acknowledges appearance persistence");

		const auto beforeImport  = log.Calls.size();
		const auto beforePreview = projection.Publications;
		panel->request_theme_import(QStringLiteral("fixture.theme"));
		expect(log.Calls.size() == beforeImport && !panel->preview_palette(), "path-selection gesture does not read a document inline");
		effects.run_one();
		expect(log.Calls.back() == "document.read" && projection.Publications == beforePreview && !panel->preview_palette(), "worker import cannot expose a preview before decision drain");
		lane.drain();
		expect(panel->preview_palette().has_value() && host.persisted_settings() == 1, "decision lane publishes import preview without persisting it");

		const auto beforeExport = log.Calls.size();
		panel->request_theme_export(QStringLiteral("export.theme"));
		expect(log.Calls.size() == beforeExport && projection.Exports == 0, "export gesture queues document write");
		effects.run_one();
		expect(log.Calls.back() == "document.write" && projection.Exports == 0, "worker export cannot acknowledge before decision drain");
		lane.drain();
		expect(projection.Exports == 1, "export acknowledgement returns through decision lane");

		panel->request_theme_import(QStringLiteral("fixture.theme"));
		effects.run_one();
		const auto beforeClose            = log.Calls.size();
		const auto beforeCancelledPreview = projection.Publications;
		window.settings()->close();
		expect(log.Calls.size() == beforeClose && !panel->preview_palette(), "Settings close cancels preview without capability entry or worker wait");
		lane.drain();
		expect(projection.Publications == beforeCancelledPreview && !panel->preview_palette() && host.outstanding_requests() == 0, "cancelled staged import cannot repaint the hidden Settings window");
		host.shutdown();
		expect(host.is_quiescent(), "appearance host finishes shutdown after all worker terminals");
		panel->set_dispatch({ });
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_queue_preference_effect_boundary();
	test_appearance_effect_boundaries();
	std::cout << "settings-effect-boundary-tests:pass capability-port-entry=worker decision-return=gui paths=queue-preference,appearance-mode,theme-import,theme-export,settings-close-cancel\n";
	return EXIT_SUCCESS;
}
