#ifndef BPR_SRC_APP_SRC_LOCAL_DRAFT_DESKTOP_HPP
#define BPR_SRC_APP_SRC_LOCAL_DRAFT_DESKTOP_HPP

#include "bpr/application/local_draft_workbench.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/workspace_source_port.hpp"

#include <QFont>
#include <QString>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace bpr::qt {
	class workbench_window;
	class local_draft_view;
}

namespace bpr::app {
	/// \brief RFC-BPR-0108 ([[BPR-2021]]). Composes local PR drafts into one
	/// desktop window: Home's Prepare pull request… and In preparation, the
	/// draft page, and the harness handoff.
	/// \details Owns the read-only workspace source, the installed-Git
	/// local-draft reader in its own isolation root under the profile
	/// (`local-draft-git-v1`), and the host over the profile's
	/// `local-drafts.sqlite3`. Nothing here needs an account, a remote, an
	/// agent or the network, and nothing writes the chosen worktree.
	class local_draft_desktop final : public application::local_draft_workbench_port
	{
	public:
		local_draft_desktop(qt::workbench_window& aWindow, const std::filesystem::path& aProfileRoot, std::filesystem::path aGitExecutable,
							std::optional<std::filesystem::path> aWindowsSystemRoot, application::effect_executor& aEffects, application::decision_executor& aDecisions);
		~local_draft_desktop() override;
		local_draft_desktop(const local_draft_desktop&)                    = delete;
		auto operator=(const local_draft_desktop&) -> local_draft_desktop& = delete;
		local_draft_desktop(local_draft_desktop&&)                         = delete;
		auto operator=(local_draft_desktop&&) -> local_draft_desktop&      = delete;

		void publish(const application::local_draft_workbench_projection& aProjection) override;
		void publish(const application::local_draft_home_projection& aHome) override;

		/// \brief Opens a worktree as Prepare pull request does, with a
		/// harness's context when one handed it over.
		void open_location(const std::string& aLocation, std::optional<std::string> aContext = std::nullopt);
		/// \brief Replaces the native folder chooser, for a course that cannot answer one.
		using folder_chooser = std::function<QString()>;
		void set_folder_chooser(folder_chooser aChooser);
		void set_code_font(const QFont& aFont);
		/// \brief Shows a draft another interface opened (`bpr draft open`).
		void               show_draft(const core::local_draft_id& aId);
		[[nodiscard]] auto host() noexcept -> application::local_draft_workbench&;
		[[nodiscard]] auto view() noexcept -> qt::local_draft_view*;
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		qt::workbench_window&                               Window;
		qt::local_draft_view*                               View = nullptr;
		std::unique_ptr<application::workspace_source_port> Source;
		std::unique_ptr<application::local_draft_workbench> Host;
		folder_chooser                                      Chooser;
		/// The page comes forward only when the author asked for it.
		bool BringForward = false;
		bool Closing      = false;
	};
}

#endif
