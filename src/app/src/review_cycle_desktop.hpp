#ifndef BPR_SRC_APP_SRC_REVIEW_CYCLE_DESKTOP_HPP
#define BPR_SRC_APP_SRC_REVIEW_CYCLE_DESKTOP_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/review_cycle_workbench.hpp"
#include "bpr/application/workspace_source_port.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/review_cycle.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace bpr::qt {
	class workbench_window;
	class review_cycle_view;
}

namespace bpr::app {
	/// \brief [[BPR-1157]]. Composes local Review Cycles into one desktop window.
	///
	/// \details Owns the read-only workspace source and the cycle surface host
	/// over this profile's own cycle store, adopts the cycle page into the
	/// window and wires File > Open Project…, Home's Review Cycles section and
	/// the page's gestures. Nothing here needs an account, a remote, an agent or
	/// the network; the chosen folder is only read.
	class review_cycle_desktop final : public application::review_cycle_workbench_port
	{
	public:
		review_cycle_desktop(qt::workbench_window& aWindow, const std::filesystem::path& aProfileRoot, core::bpr_profile_id aProfile, application::effect_executor& aEffects,
							 application::decision_executor& aDecisions);
		~review_cycle_desktop() override;
		review_cycle_desktop(const review_cycle_desktop&)                    = delete;
		auto operator=(const review_cycle_desktop&) -> review_cycle_desktop& = delete;
		review_cycle_desktop(review_cycle_desktop&&)                         = delete;
		auto operator=(review_cycle_desktop&&) -> review_cycle_desktop&      = delete;

		void publish(const application::review_cycle_workbench_projection& aProjection) override;
		void publish(const std::vector<application::review_cycle_recent>& aRecents) override;

		/// \brief Opens a chosen location as a cycle, as the dialog would.
		void open_project(const std::string& aLocation);
		/// \brief The window's pull request took the review surface back.
		void               leave();
		[[nodiscard]] auto host() noexcept -> application::review_cycle_workbench&;
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		qt::workbench_window&                                Window;
		qt::review_cycle_view*                               View = nullptr;
		std::unique_ptr<application::workspace_source_port>  Source;
		std::unique_ptr<application::review_cycle_workbench> Host;
		core::local_cycle_subject                            LastSubject;
		bool                                                 Closing = false;
	};
}

#endif
