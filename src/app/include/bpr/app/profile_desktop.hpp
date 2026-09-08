#ifndef BPR_SRC_APP_INCLUDE_BPR_APP_PROFILE_DESKTOP_HPP
#define BPR_SRC_APP_INCLUDE_BPR_APP_PROFILE_DESKTOP_HPP
#include "bpr/app/github_application.hpp"
namespace bpr::qt {
	class profile_management_window;
}
namespace bpr::app {
	using profile_desktop_startup_result     = std::expected<github_application*, github_application_startup_failure>;
	using profile_desktop_startup_completion = std::function<void(profile_desktop_startup_result)>;
	/// Owns profile runtimes and exact window lifetimes independently of any one window.
	class profile_desktop final
	{
	public:
		[[nodiscard]] static auto start(github_application_config aConfig, profile_desktop_startup_completion aCompletion) -> std::unique_ptr<profile_desktop>;
		~profile_desktop();
		profile_desktop(const profile_desktop&)                                  = delete;
		auto               operator=(const profile_desktop&) -> profile_desktop& = delete;
		[[nodiscard]] auto runtime(core::bpr_profile_id aProfile) const noexcept -> github_application*;
		[[nodiscard]] auto windows(core::bpr_profile_id aProfile) const -> std::vector<core::bpr_window_context>;
		[[nodiscard]] auto window(core::bpr_window_context aContext) const noexcept -> qt::workbench_window*;
		[[nodiscard]] auto management_window() const noexcept -> qt::profile_management_window*;
		/// Requests durable orderly Quit; failed admission leaves windows usable.
		void               quit(int aExitCode = 0);
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto exit_code() const noexcept -> int;

	private:
		[[nodiscard]] static auto configuration_for_profile(const github_application_config& aConfig, core::bpr_profile_id aProfile, const std::filesystem::path& aRoot) -> github_application_config;
		using startup_factory = std::function<std::unique_ptr<github_application_startup>(core::bpr_profile_id, const std::filesystem::path&, github_application_startup_completion)>;
		[[nodiscard]] static auto start_with_factory(core::bpr_profile_id aProfile, startup_factory aFactory, profile_desktop_startup_completion aCompletion) -> std::unique_ptr<profile_desktop>;
		class implementation;
		explicit profile_desktop(std::shared_ptr<implementation> aImplementation);
		std::shared_ptr<implementation> Implementation;
		friend class detail::github_application_fixture_access;
	};
}
#endif
