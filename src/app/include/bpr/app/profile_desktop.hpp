#ifndef BPR_SRC_APP_INCLUDE_BPR_APP_PROFILE_DESKTOP_HPP
#define BPR_SRC_APP_INCLUDE_BPR_APP_PROFILE_DESKTOP_HPP
#include "bpr/app/github_application.hpp"
namespace bpr::qt {
	class desktop_menu_bar;
	class profile_management_window;
}
namespace bpr::app {
	using profile_desktop_startup_result     = std::expected<github_application*, github_application_startup_failure>;
	using profile_desktop_startup_completion = std::function<void(profile_desktop_startup_result)>;
	/// \brief What the desktop does once no profile runtime remains.
	enum class empty_desktop_outcome : std::uint8_t {
		stay_resident,
		quit
	};
	/// \brief The composition's named answer for an empty desktop.
	///
	/// \details RFC-BPR-0033 prohibits per-platform behaviour that is not a named
	/// policy in the composition, so the desktop asks this and never tests the
	/// platform itself. Injected at construction and therefore drivable by a course.
	using empty_desktop_policy = std::function<empty_desktop_outcome()>;
	/// \brief macOS stays resident; every other platform quits.
	///
	/// \details A macOS application with no window keeps its menu bar and Dock icon,
	/// and is reopened by activating it. Elsewhere a program with no window is
	/// expected to be gone, so the empty desktop quits through the existing Quit
	/// path. This function is the one place that mapping exists.
	[[nodiscard]] auto native_empty_desktop_policy() -> empty_desktop_policy;
	/// \brief macOS keeps one menu bar status item for the process; elsewhere none.
	///
	/// \details [[BPR-1250]]. A resident macOS desktop with no window still has its
	/// Dif in the menu bar, so the item belongs to the desktop, not to a profile
	/// that retires with its last window. It shows Dif's template image and every
	/// profile's notification sink submits through it. Linux and Windows keep a
	/// tray per profile, as before; their empty desktop quits anyway. Null also
	/// where the platform offers no tray. This function is the one place that
	/// mapping exists.
	[[nodiscard]] auto native_menu_bar_status_item() -> std::shared_ptr<QSystemTrayIcon>;
	/// Owns profile runtimes and exact window lifetimes independently of any one window.
	class profile_desktop final
	{
	public:
		[[nodiscard]] static auto start(github_application_config aConfig, profile_desktop_startup_completion aCompletion, empty_desktop_policy aPolicy = { }) -> std::unique_ptr<profile_desktop>;
		~profile_desktop();
		profile_desktop(const profile_desktop&)                                  = delete;
		auto               operator=(const profile_desktop&) -> profile_desktop& = delete;
		[[nodiscard]] auto runtime(core::bpr_profile_id aProfile) const noexcept -> github_application*;
		[[nodiscard]] auto windows(core::bpr_profile_id aProfile) const -> std::vector<core::bpr_window_context>;
		[[nodiscard]] auto window(core::bpr_window_context aContext) const noexcept -> qt::workbench_window*;
		[[nodiscard]] auto management_window() const noexcept -> qt::profile_management_window*;
		/// \brief The application's own menu bar, once a resident desktop has no window.
		///
		/// \details Absent until then, and absent entirely on a platform whose empty
		/// desktop quits.
		[[nodiscard]] auto desktop_menu() const noexcept -> qt::desktop_menu_bar*;
		/// \brief The application's menu bar status item, for the desktop's whole
		/// life, or null where `native_menu_bar_status_item` gives none.
		[[nodiscard]] auto status_item() const noexcept -> QSystemTrayIcon*;
		/// \brief The outcome the injected policy last gave for an empty desktop.
		///
		/// \details Empty until the last runtime retires, so what the composition
		/// decided is readable rather than inferred from the process staying alive.
		[[nodiscard]] auto last_empty_desktop_outcome() const noexcept -> std::optional<empty_desktop_outcome>;
		/// \brief Requests durable orderly Quit.
		///
		/// \details A refused Quit with exit code 0 leaves the windows usable. A
		/// nonzero exit code is a failure verdict already reported, so while that
		/// Quit is refused the desktop retries it on its poll, and forces the
		/// shutdown after a bound.
		void quit(int aExitCode = 0);
		/// \brief Saves all private draft text before requesting the existing orderly Quit barrier.
		/// \details Failure leaves the windows live. Success is observed only through
		/// quiescence plus exit_code(); this method never publishes or retries a review.
		void               quit_for_update(std::function<void(bool)> aPrepared);
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto exit_code() const noexcept -> int;

	private:
		[[nodiscard]] static auto configuration_for_profile(const github_application_config& aConfig, core::bpr_profile_id aProfile, const std::filesystem::path& aRoot) -> github_application_config;
		/// The reset is [[BPR-1602]]'s: present only for the runtime that reopens a
		/// profile after a confirmed Troubleshooting reset.
		using startup_factory = std::function<std::unique_ptr<github_application_startup>(core::bpr_profile_id, const std::filesystem::path&, core::profile_launch_intent, std::optional<core::profile_data_reset_request>, github_application_startup_completion)>;
		[[nodiscard]] static auto start_with_factory(core::bpr_profile_id aProfile, startup_factory aFactory, profile_desktop_startup_completion aCompletion, core::profile_launch_intent aIntent = core::profile_launch_intent::activate, empty_desktop_policy aPolicy = { }, bool aNativeLifetime = false, std::shared_ptr<QSystemTrayIcon> aStatusItem = { }) -> std::unique_ptr<profile_desktop>;
		class implementation;
		explicit profile_desktop(std::shared_ptr<implementation> aImplementation);
		std::shared_ptr<implementation> Implementation;
		friend class detail::github_application_fixture_access;
	};
}
#endif
