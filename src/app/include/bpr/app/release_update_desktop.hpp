#ifndef BPR_SRC_APP_INCLUDE_BPR_APP_RELEASE_UPDATE_DESKTOP_HPP
#define BPR_SRC_APP_INCLUDE_BPR_APP_RELEASE_UPDATE_DESKTOP_HPP
#include "bpr/application/release_update_recovery.hpp"
#include "bpr/application/release_update_service.hpp"
#include <filesystem>

namespace bpr::qt {
	class workbench_window;
	class release_update_panel;
	class settings_window;
}
class QMenuBar;
namespace bpr::app {
	/// \brief One offered release access; acquisition stays at the vault boundary. Nothing is
	/// picked by hand: the stored binding is kept while offered, otherwise the first offer is used.
	struct release_update_account
	{
		std::string Binding;
		std::string Label;
		std::function<void(core::release_update_access_id, std::shared_ptr<application::release_update_archive_store>,
						   std::function<void(std::shared_ptr<application::release_update_access_port>)>)>
			Acquire;
	};
	/// \brief [[BPR-1669]]. The credential-free public release access the
	/// native composition offers first.
	/// \details [[BPR-1672]]. Its binding is the lowercase hex SHA-256 of a
	/// fixed label, the form the update store keeps for any account, so update
	/// settings save with it as they do with a signed-in account.
	[[nodiscard]] auto release_update_public_account() -> release_update_account;
	/// \brief Public composition dependencies, supplied by native startup or isolated fixtures.
	struct release_update_components
	{
		std::string                                                    Installed;
		core::release_update_capability                                Capability = core::release_update_capability::untrusted_installation;
		std::shared_ptr<application::release_update_clock_port>        Clock;
		std::shared_ptr<application::release_update_settings_port>     Settings;
		std::shared_ptr<application::release_update_verification_port> Verifier;
		std::shared_ptr<application::release_update_archive_store>     Archives;
		std::function<void(std::string)>                               Browser;
		std::shared_ptr<application::release_update_install_port>      Installer{ };
	};
	/// \brief One desktop-wide service shared by menus, windows and Settings.
	class release_update_desktop final
	{
	public:
		[[nodiscard]] static auto start_native(std::filesystem::path aRoot) -> std::unique_ptr<release_update_desktop>;
		[[nodiscard]] static auto start(release_update_components aComponents) -> std::unique_ptr<release_update_desktop>;
		~release_update_desktop();
		void               attach(qt::workbench_window& aWindow);
		void               attach(qt::settings_window& aSettings);
		void               attach(QMenuBar& aMenu);
		void               set_accounts(std::vector<release_update_account> aAccounts);
		void               shutdown();
		[[nodiscard]] auto quiescent() const -> bool;
		[[nodiscard]] auto durable() const -> bool;
		[[nodiscard]] auto observe_installation_result(bool aInstalled) -> bool;
		[[nodiscard]] auto panel() const -> qt::release_update_panel*;
		[[nodiscard]] auto projection() const -> application::release_update_projection;

	private:
		struct implementation;
		explicit release_update_desktop(std::shared_ptr<implementation> aImpl);
		std::shared_ptr<implementation> Impl;
	};
}
#endif
