#include "bpr/app/diagnostic_controls.hpp"
#include "bpr/adapters/sqlite/diagnostic_preferences.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/qt/diagnostic_settings_panel.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/settings_window.hpp"
#include <QDesktopServices>
#include <QPointer>
#include <QUrl>
namespace bpr::app {
	namespace {
		class settings final : public application::diagnostic_settings_port
		{
		public:
			explicit settings(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }
			auto load() -> application::diagnostic_preference_load override { return adapters::sqlite::load_diagnostic_preferences(Root); }
			auto store(application::diagnostic_preferences aPreferences) -> bool override { return adapters::sqlite::store_diagnostic_preferences(Root, aPreferences); }
			auto open_folder() -> bool override
			{
				const auto      path = Root / "logs";
				std::error_code error;
				if (!std::filesystem::is_directory(std::filesystem::symlink_status(path, error)) || error) {
					return false;
				}
				return QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdU16String(path.u16string())));
			}

		private:
			std::filesystem::path Root;
		};
	}
	struct diagnostic_controls::implementation
	{
		std::shared_ptr<application::diagnostic_control_port> Control;
		settings                                              Store;
		worker_executor                                       Effects;
		qt::event_loop_executor                               Decisions;
		application::diagnostic_settings_host                 Host;
		QPointer<qt::diagnostic_settings_panel>               Panel;
		implementation(std::filesystem::path aRoot, std::shared_ptr<application::diagnostic_control_port> aControl, application::diagnostic_preference_load aLoaded)
			: Control{ std::move(aControl) }
			, Store{ std::move(aRoot) }
			, Host{ *Control, Store, Effects, Decisions, aLoaded }
		{ }
	};
	diagnostic_controls::diagnostic_controls(std::filesystem::path aRoot, std::shared_ptr<application::diagnostic_control_port> aControl, application::diagnostic_preference_load aLoaded)
		: Impl{ std::make_unique<implementation>(std::move(aRoot), std::move(aControl), aLoaded) }
	{ }
	diagnostic_controls::~diagnostic_controls()
	{
		shutdown();
		if (Impl->Panel) {
			Impl->Panel->set_dispatch({ });
		}
	}
	void diagnostic_controls::shutdown()
	{
		Impl->Host.shutdown();
		if (Impl->Panel) {
			Impl->Panel->set_dispatch({ });
		}
	}
	auto diagnostic_controls::quiescent() const -> bool { return Impl->Host.quiescent() && Impl->Effects.idle(); }
	void diagnostic_controls::attach(qt::settings_window& aSettings)
	{
		if (!Impl->Panel) {
			Impl->Panel = new qt::diagnostic_settings_panel;
			Impl->Panel->set_dispatch({ [this](auto aValue) { Impl->Host.apply(aValue); }, [this] { Impl->Host.clear(); }, [this] { Impl->Host.open_folder(); }, [this] { Impl->Host.refresh(); } });
			Impl->Host.observe([this](const auto& aProjection) { if (Impl->Panel) { Impl->Panel->adopt(aProjection); } });
		}
		if (Impl->Panel->parentWidget() != aSettings.page_widget(qt::settings_page::storage_and_privacy)) {
			aSettings.adopt_panel(qt::settings_page::storage_and_privacy, Impl->Panel);
		}
		Impl->Host.refresh();
	}
}
