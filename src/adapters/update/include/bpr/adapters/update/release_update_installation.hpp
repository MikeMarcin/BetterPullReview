#ifndef BPR_SRC_ADAPTERS_UPDATE_INCLUDE_BPR_ADAPTERS_UPDATE_RELEASE_UPDATE_INSTALLATION_HPP
#define BPR_SRC_ADAPTERS_UPDATE_INCLUDE_BPR_ADAPTERS_UPDATE_RELEASE_UPDATE_INSTALLATION_HPP

#include "bpr/application/release_update_port.hpp"
#include <filesystem>

namespace bpr::adapters::update {
	struct release_update_installation_result
	{
		std::string Transaction;
		bool        Installed = false;
	};
	/// \brief Process-lifetime installation ownership, acquired before opening profile stores.
	/// \details File locks bind live owners, not reusable process numbers. Calls perform
	/// bounded local effects and belong on an effect lane. Keep the lease until every
	/// profile, worker and store is closed. A pending transaction refuses new owners.
	class release_update_installation final
	{
	public:
		~release_update_installation();
		release_update_installation(const release_update_installation&)                                         = delete;
		auto                      operator=(const release_update_installation&) -> release_update_installation& = delete;
		[[nodiscard]] static auto join(std::filesystem::path aInstalled,
									   std::string aStartupTransaction = { }, std::string_view aRunningVersion = { }) -> std::expected<std::unique_ptr<release_update_installation>, core::release_update_reason>;
		[[nodiscard]] auto        shutdown_requested() -> std::optional<std::string>;
		[[nodiscard]] auto        last_result() -> std::optional<release_update_installation_result>;
		/// \brief Transfer the two bounded lifetime locks to kernel process-exit cleanup.
		/// \details Native composition calls this once. Even desktop destruction is
		/// not process exit; the helper must remain excluded through final teardown.
		void keep_until_process_exit() noexcept;
		/// \brief Records the exact transaction's durable shutdown result; never releases the lifetime lease.
		[[nodiscard]] auto acknowledge_shutdown(std::string_view aTransaction, bool aDurable) -> bool;
		/// \brief Called only after startup and state stores are ready, with the lifetime lease still held.
		[[nodiscard]] auto healthy_start(std::string_view aVersion) -> bool;

	private:
		struct implementation;
		explicit release_update_installation(std::unique_ptr<implementation> aImpl);
		std::unique_ptr<implementation> Impl;
	};

	/// \brief Bundled helper entry, with bounded arguments and no release-supplied commands.
	[[nodiscard]] auto run_release_update_helper(const std::filesystem::path& aInstalled,
												 std::string_view aTransaction, bool aRecover) -> int;
	/// \brief Native installer factory; composition must qualify its package before selecting it.
	[[nodiscard]] auto make_release_update_installer(std::filesystem::path aInstalled)
		-> std::shared_ptr<application::release_update_install_port>;
}
#endif
