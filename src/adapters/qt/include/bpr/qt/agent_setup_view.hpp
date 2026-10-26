#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SETUP_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SETUP_VIEW_HPP

#include "bpr/application/agent_setup_host.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/clone_link_host.hpp"
#include "bpr/core/agent_setup.hpp"

#include "bpr/qt/agent_entry_dialog.hpp"
#include <QString>

#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief [[BPR-1753]], RFC-BPR-0092. What the reviewer can do in
	/// the setup; each is a request to the host, never an effect here.
	struct agent_setup_intents
	{
		std::function<void(core::setup_agent)> Choose;
		/// Step-specific metadata searches, neither reruns a check.
		std::function<void()> FindAgents;
		std::function<void()> FindClones;
		std::function<void()> OpenSettings;
		std::function<void()> OpenRepositorySettings;
		/// Opens the agent's install page in the browser, through the host,
		/// which knows the address.
		std::function<void(core::setup_agent)> OpenInstallPage;
		std::function<void(std::string)>       ChooseClone;
		std::function<void(std::string)>       AddCloneFolder;
		/// The whole ordered clone-folder list after an edit in the step.
		std::function<void(std::vector<std::string>)> SetCloneFolders;
		std::function<void()>                         UsePrivateCopy;
		std::function<void()>                         Check;
		std::function<void()>                         Stop;
		/// [[BPR-1753]], owner amendment 2026-10-23. A custom agent the
		/// reviewer described in setup: added to the list and chosen.
		std::function<void(core::agent_entry)> AddCustomAgent;
	};

	/// \brief What the setup's words need besides the two projections.
	struct agent_setup_facts
	{
		/// The pull request's repository, `OWNER/NAME`; empty with no review.
		std::string Repository;
		/// The head, seven characters.
		std::string Head;
		/// The review was read sparsely: a private copy holds its files only.
		bool Sparse = false;
		/// Why the last workspace check could not make the workspace.
		std::optional<application::agent_workspace_unavailable_reason> WorkspaceReason;

		auto operator<=>(const agent_setup_facts&) const = default;
	};

	/// \brief [[BPR-1753]], RFC-BPR-0092 (owner answer Q1 A). The
	/// Agent tab's setup, in place of the conversation: Agent, Repository,
	/// Check it works, each folding to one line with **Change** once done.
	///
	/// \details Presentation only. The host's projections say what was found,
	/// chosen and checked; the words for each state are written here. Which
	/// step is open is this view's own: the current one, or the one whose
	/// **Change** was pressed. The setup shows once the host knows a step is
	/// missing (the agents looked for and the repository's link checked), and
	/// keeps showing until both checks pass, so a new window with a saved
	/// agent and link opens on the conversation.
	class agent_setup_view final : public QWidget
	{
	public:
		explicit agent_setup_view(QWidget* aParent = nullptr);

		void set_intents(agent_setup_intents aIntents);
		/// \brief How Custom agent… asks for the entry; a test answers itself.
		void set_entry_editor(agent_entry_editor aEditor);
		/// Called whenever `needed()` or the ready line may have changed.
		void set_change_observer(std::function<void()> aObserver);

		void present(const application::agent_setup_projection& aSetup, const application::clone_link_projection& aClones, const agent_setup_facts& aFacts);
		/// **Set up agent…** and **Change repository folder…**: show the
		/// setup with that step open.
		void begin(core::setup_step aStep);

		/// The setup is in place of the conversation.
		[[nodiscard]] auto needed() const noexcept -> bool { return Active; }
		/// The setup just finished; the conversation shows the ready line once.
		[[nodiscard]] auto take_ready() -> bool;
		/// "Claude Code is ready" and its line, for the conversation.
		[[nodiscard]] auto ready_title() const -> QString;
		[[nodiscard]] auto ready_text() const -> QString;
		[[nodiscard]] auto open_step() const noexcept -> core::setup_step { return Open; }
		[[nodiscard]] auto step_text() const -> QString;

	private:
		void               rebuild();
		void               build_agent(QVBoxLayout& aBody);
		void               build_repository(QVBoxLayout& aBody);
		void               build_check(QVBoxLayout& aBody);
		[[nodiscard]] auto agent_name() const -> QString;
		/// The chosen custom agent's own name for its kind, or the plain name.
		[[nodiscard]] auto name_for(core::setup_agent aAgent) const -> QString;
		void               add_custom_agent();
		agent_entry_editor Editor = default_agent_entry_editor();
		[[nodiscard]] auto workspace_words() const -> QString;

		agent_setup_intents                 Intents;
		std::function<void()>               Changed;
		application::agent_setup_projection Setup;
		application::clone_link_projection  Clones;
		agent_setup_facts                   Facts;
		std::optional<core::setup_step>     Editing;
		std::optional<core::setup_agent>    Picked;
		std::optional<std::string>          PickedClone;
		core::setup_step                    Open             = core::setup_step::agent;
		bool                                Active           = false;
		bool                                Ready            = false;
		bool                                AgentDetailsOpen = false;
		bool                                WorkspaceLogOpen = false;
		QVBoxLayout*                        Steps            = nullptr;
		QLabel*                             Intro            = nullptr;
	};

	/// \brief [[BPR-1753]], RFC-BPR-0092 Ready. What one review
	/// request's brief holds, in numbers and names, for **What the agent gets**.
	struct agent_ready_facts
	{
		std::string                Repository;
		std::uint64_t              Number = 0U;
		std::string                Title;
		std::string                BaseBranch;
		std::string                Base;
		std::string                Head;
		std::size_t                Files      = 0U;
		std::size_t                Threads    = 0U;
		std::size_t                Unresolved = 0U;
		std::optional<std::string> Clone;
		bool                       Sparse = false;
	};

	/// \brief **What the agent gets**, one line per part of the brief.
	[[nodiscard]] auto agent_ready_gets(const agent_ready_facts& aFacts) -> QString;

	/// \brief [[BPR-1753]]. The words for a workspace that could not be
	/// made: what happened, then what to do ([[BPR-1738]]).
	[[nodiscard]] auto workspace_unavailable_words(application::agent_workspace_unavailable_reason aReason) -> QString;

	/// \brief [[BPR-1753]]. The words for a setup check that failed,
	/// for the setup and for a later Ask that failed for the same reason.
	[[nodiscard]] auto setup_failure_words(core::agent_setup_failure aFailure, const QString& aAgent, const QString& aDetail) -> QString;

	/// \brief The command a reviewer runs to sign the agent in.
	[[nodiscard]] auto sign_in_command(core::setup_agent aAgent) -> QString;
	/// \brief The command the check runs, as the setup names it.
	[[nodiscard]] auto status_command(core::setup_agent aAgent) -> QString;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SETUP_VIEW_HPP
