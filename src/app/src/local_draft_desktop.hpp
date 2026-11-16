#ifndef BPR_SRC_APP_SRC_LOCAL_DRAFT_DESKTOP_HPP
#define BPR_SRC_APP_SRC_LOCAL_DRAFT_DESKTOP_HPP

#include "bpr/app/worker_executor.hpp"
#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/author_git_port.hpp"
#include "bpr/application/local_changes_host.hpp"
#include "bpr/application/local_draft_workbench.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/workspace_source_port.hpp"
#include "bpr/core/local_tour_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"

#include "bpr/qt/local_draft_parts.hpp"

#include <QFont>
#include <QPointer>
#include <QString>

#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace bpr::adapters::syntax {
	class packaged_definition_catalog;
	class packaged_syntax_classifier;
}

namespace bpr::application {
	class insights_observer;
	class syntax_classification_application_host;
}

namespace bpr::qt {
	class appearance_profile;
	class workbench_window;
	class local_draft_parts;
}

namespace bpr::app {
	class suggestion_text_source;
	class code_syntax_relay;
	class local_draft_corpus;
	class pr_search_controller;

	/// \brief The packaged syntax definitions and the lane their classifier
	/// runs on, so a draft's comparison and Edit view are coloured as a pull
	/// request's are.
	struct local_draft_syntax
	{
		const adapters::syntax::packaged_definition_catalog* Definitions = nullptr;
		application::effect_executor*                        Effects     = nullptr;
		std::shared_ptr<application::insights_observer>      Insights;
	};

	/// \brief RFC-BPR-0108 ([[BPR-2021]]). Composes local PR drafts into one
	/// desktop window: Home's Prepare pull request… and In preparation, the
	/// draft page, and the harness handoff.
	/// \details Owns the read-only workspace source, the installed-Git
	/// local-draft reader in its own isolation root under the profile
	/// (`local-draft-git-v1`), and the host over the profile's
	/// `local-drafts.sqlite3`. Nothing here needs an account, a remote, an
	/// agent or the network, and nothing writes the chosen worktree.
	class local_draft_desktop final : public application::local_draft_workbench_port, public application::local_changes_port
	{
	public:
		local_draft_desktop(qt::workbench_window& aWindow, const std::filesystem::path& aProfileRoot, std::filesystem::path aGitExecutable,
							std::optional<std::filesystem::path> aWindowsSystemRoot, application::effect_executor& aEffects, application::decision_executor& aDecisions,
							local_draft_syntax aSyntax = { }, application::effect_executor* aAuthorGitLane = nullptr);
		~local_draft_desktop() override;
		local_draft_desktop(const local_draft_desktop&)                    = delete;
		auto operator=(const local_draft_desktop&) -> local_draft_desktop& = delete;
		local_draft_desktop(local_draft_desktop&&)                         = delete;
		auto operator=(local_draft_desktop&&) -> local_draft_desktop&      = delete;

		void publish(const application::local_draft_workbench_projection& aProjection) override;
		void publish(const application::local_draft_home_projection& aHome) override;
		/// \brief Author state (RFC-BPR-0113, 0114 as amended 2026-11-12).
		void publish(const application::local_changes_projection& aProjection) override;
		/// \brief RFC-BPR-0120, RFC-BPR-0103. A running check's activity.
		void publish_check_activity(const std::optional<core::agent_turn_activity>& aActivity) override;

		/// \brief Opens a worktree as Prepare pull request does, with a
		/// harness's context when one handed it over.
		void open_location(const std::string& aLocation, std::optional<std::string> aContext = std::nullopt);
		/// \brief Replaces the native folder chooser, for a course that cannot answer one.
		using folder_chooser = std::function<QString()>;
		void set_folder_chooser(folder_chooser aChooser);
		void set_code_font(const QFont& aFont);
		/// \brief RFC-BPR-0110. The agent a question goes to, as the window
		/// has it chosen, with the local-authoring prompts' text and versions.
		struct agent_choice
		{
			std::string   Identity;
			std::string   Name;
			std::uint64_t Profile = 0;
			std::string   Description;
			std::string   DescriptionVersion;
			std::string   Discussion;
			std::string   DiscussionVersion;
			/// RFC-BPR-0114 as amended 2026-11-12: `local.commit-message`.
			std::string CommitMessage;
			/// [[BPR-2140]]. Where the agent's instructions go (its switch).
			core::agent_instruction_delivery Delivery = core::agent_instruction_delivery::system_prompt;
			/// RFC-BPR-0120: `local-draft.run-check`'s effective text and version.
			std::string Check;
			std::string CheckVersion;
		};
		using agent_chooser = std::function<std::optional<agent_choice>()>;
		/// \brief Composes the agent runs; a null runner leaves authoring by hand only.
		void set_agent(application::local_draft_agent_runner aRunner, application::effect_executor* aLane, agent_chooser aChooser);
		/// \brief RFC-BPR-0120. The check copies (under the agents' scratch
		/// root) and the most bytes one may hold; without them no check is planned.
		void set_check_copies(std::unique_ptr<application::local_check_copy_port> aCopies, std::function<std::uint64_t()> aLimitBytes);
		/// \brief Replaces the first build question, for a course.
		void set_build_question(std::function<bool()> aQuestion);
		/// \brief A report's code reference (`bpr-check:PATH:LINE`): the
		/// checked candidate at that line. False when it is not one.
		auto follow_check_reference(const std::string& aLink) -> bool;
		/// \brief RFC-BPR-0111. The account Create draft PR acts as, asked each
		/// time; its GitHub calls run on this desktop's own lane, apart from
		/// every review submission (X22).
		using publication_account = std::function<std::optional<application::github_publication_account>()>;
		void set_publishing(publication_account aAccount);
		/// \brief RFC-BPR-0112. Reads the profile's workflows again, after Settings imported some.
		void reload_workflows();
		/// \brief Replaces the clipboard for the named push, for a course.
		void set_copier(std::function<void(const std::string&)> aCopier);
		/// \brief Shows a draft another interface opened (`bpr draft open`).
		void               show_draft(const core::local_draft_id& aId);
		[[nodiscard]] auto host() noexcept -> application::local_draft_workbench&;
		/// \brief RFC-BPR-0114 AC06. The author's own Git (status, commit,
		/// push) runs on this lane of its own, so a push waiting on its hooks
		/// or the network holds up no other work, a review submission least.
		[[nodiscard]] auto author_git_lane() noexcept -> application::effect_executor&;
		/// \brief RFC-BPR-0111 AC07. Create Draft PR's own lane, where a
		/// creation waits on GitHub; null before publishing is composed.
		[[nodiscard]] auto publication_lane() noexcept -> application::effect_executor*;
		/// \brief Author state's host, while a draft on its own worktree shows.
		[[nodiscard]] auto changes() noexcept -> application::local_changes_host&;
		/// \brief An attended look's pull request opened by its author: the
		/// shown draft is associated with pull request `aNumber` in its store.
		void associate_for_look(std::uint64_t aNumber);
		/// \brief Replaces the one-time hooks question, for a course.
		void               set_consent_question(std::function<bool()> aQuestion);
		[[nodiscard]] auto view() noexcept -> qt::local_draft_parts*;
		/// \brief Find in files over the shown candidate, through the window's Search tab.
		void search(int aGesture);
		/// \brief The Search tab's results answer the draft's search.
		void attach_search();
		/// \brief The panes' code colours for the Edit view.
		void adopt_appearance(const qt::appearance_profile& aProfile);
		/// \brief RFC-BPR-0110 as amended (owner: "I thought we chose to stick
		/// with the PR view and just augment it"). While the window shows this
		/// draft, its Tour panel shows the draft's tour: presented from the
		/// draft through `core::view_local_tour`, and its gestures turned into
		/// the draft's own edits, questions and decisions here.
		[[nodiscard]] auto tour_shown() const -> bool;
		void               present_tour();
		void               tour_command(const core::tour_command& aCommand);
		/// \returns Whether the action was the draft's (it always is while
		/// `tour_shown`); a pull request's publication actions do nothing.
		void tour_action(qt::tour_editor_action aAction);
		void tour_ask(const qt::tour_ask& aAsk);
		/// \brief RFC-BPR-0126 Q2 ([[BPR-2154]]). The draft whose tour the
		/// window shows; empty when none shows.
		[[nodiscard]] auto shown_draft() const -> std::string;
		/// \brief Whether a question of the draft is with its agent now.
		[[nodiscard]] auto asking() const -> bool;
		/// \brief [[BPR-2154]]. The discussion question with the agent: its
		/// part and number (`local_draft_workbench_projection::AskingRun`);
		/// none while nothing, a check or a first description runs.
		[[nodiscard]] auto asking_question() const -> std::optional<std::pair<core::tour_thread, std::uint64_t>>;
		/// \brief [[BPR-2154]]. The last question that ended, by its number,
		/// and what was steered into it, settled.
		[[nodiscard]] auto answered_steers() const -> std::pair<std::uint64_t, std::vector<application::local_draft_steered>>;
		/// \brief Whether an agent is chosen for the draft's questions.
		[[nodiscard]] auto agent_ready() const -> bool;
		/// \brief Told when a question goes to the agent (`aAsking`) and when
		/// it has ended: failed, or stopped by Stop.
		using asking_observer = std::function<void(bool aAsking, bool aFailed, bool aStopped)>;
		void set_asking_observer(asking_observer aObserver) { AskingObserver = std::move(aObserver); }
		/// \brief Told each time the draft's tour is presented.
		void set_tour_observer(std::function<void()> aObserver) { TourObserver = std::move(aObserver); }
		/// \brief A one-shot writing request (a field's draft) asks the part's chat.
		void tour_writing(core::writing_hook aHook);
		void tour_stop();
		/// \brief A one-shot writing request was let go (the window lost
		/// focus): it stops a question, never a check (RFC-BPR-0120: the
		/// author keeps working while a check runs).
		void writing_stop();
		void tour_decide(std::size_t aIndex, core::tour_proposal_decision aDecision, const QString& aText);
		/// \brief Overview's description Save, for this draft: the body, with the draft's title.
		void save_description(const std::string& aBody);
		/// \brief Create Draft PR…: Overview's Status tab, and GitHub read for the plan.
		void open_publication();
		/// \brief RFC-BPR-0111 as drawn: when the shown draft's pull request is
		/// created (or linked), the window becomes that pull request.
		using pull_request_opener = std::function<void(const std::string& aHost, const std::string& aOwner, const std::string& aName, std::uint64_t aNumber)>;
		void               set_pull_request_opener(pull_request_opener aOpener);
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		/// The draft's edits wait for the one before to be written, so the
		/// panel's typing is never lost to a stale version; a text edit
		/// replaces the one waiting for the same field.
		struct tour_edit
		{
			std::string           Key;
			std::function<void()> Apply;
		};
		std::deque<tour_edit> TourEdits;
		asking_observer       AskingObserver;
		std::function<void()> TourObserver;
		bool                  AskingSeen = false;
		bool                  StopAsked  = false;
		/// The question with the agent is a one-shot writing request's
		/// (`tour_writing`), which `writing_stop` lets go.
		bool                WritingAsk       = false;
		bool                AskingForWriting = false;
		bool                TourEditInFlight = false;
		pull_request_opener OpenPullRequest;
		/// Whether each draft seen was linked when last published.
		std::map<std::string, bool> LinkedSeen;
		std::optional<std::string>  TypedProblem;
		std::optional<std::string>  TypedSolution;
		void                        queue_tour_edit(std::string aKey, std::function<void()> aApply);
		void                        flush_tour_edits();
		/// The view last presented, for the panel's proposals by index.
		std::optional<core::local_tour_view> TourView;
		void                                 ask_local(application::local_draft_question aQuestion);
		/// RFC-BPR-0112. An agent action asks the agent it is bound to.
		void                  ask_for_action(const std::string& aAction);
		qt::workbench_window& Window;
		/// RFC-BPR-0111. Create draft PR's own lane; outlives the host's work on it.
		std::unique_ptr<worker_executor>        PublishLane;
		std::function<void(const std::string&)> Copier;
		/// The window owns the page; a window torn down first takes it with it.
		QPointer<qt::local_draft_parts>                     View;
		std::unique_ptr<application::workspace_source_port> Source;
		/// RFC-BPR-0112. Workflow request and answer files; outlives the host's use of it.
		std::unique_ptr<application::workflow_exchange_port> Exchange;
		/// RFC-BPR-0120. Outlives the host, whose effect lane writes the copies.
		std::unique_ptr<application::local_check_copy_port> CheckCopies;
		std::unique_ptr<application::local_draft_workbench> Host;
		/// The agent the draft's checks run as: the one last asked, or the
		/// whole-change conversation's.
		std::string           CheckAgent;
		std::string           CheckAgentName;
		std::function<bool()> BuildQuestion;
		std::uint64_t         ReportCandidate = 0U;
		qint64                CheckStartedMs  = 0;
		void                  present_checks(core::local_tour_view& aView);
		void                  check_grants(bool aScratch, bool aBuilds);
		void                  open_check_report(core::tour_agent_run_id aRun);
		void                  run_check(core::tour_agent_run_id aRun, bool aAgain);
		void                  plan_check(const QString& aWords);
		[[nodiscard]] auto    check_question(const std::string& aWords) -> std::optional<application::local_check_question>;
		/// Author state: the author's own Git, the one file Save writes, and
		/// the one-time question's answers (`author-git-consent.txt`).
		std::unique_ptr<application::author_git_port>    AuthorGit;
		std::unique_ptr<application::worktree_file_port> WorktreeFiles;
		/// Outlives the host whose effects run on it; a course may lend its own.
		std::unique_ptr<worker_executor>                 OwnAuthorGitLane;
		application::effect_executor*                    AuthorGitLane = nullptr;
		std::unique_ptr<application::local_changes_host> Changes;
		std::filesystem::path                            ConsentFile;
		std::unique_ptr<worker_executor>                 CommitAgentLane;
		void                                             follow_author_state(const application::local_draft_workbench_projection& aProjection);
		folder_chooser                                   Chooser;
		agent_chooser                                    AgentChooser;
		/// The page comes forward only when the author asked for it.
		/// A local text classified by the window's own definitions.
		struct wanted_syntax
		{
			application::token_span_cache_key  Key;
			std::shared_ptr<const std::string> Document;
		};
		void                                                                 want_syntax(const std::string& aPath, const std::string& aDocument);
		void                                                                 submit_syntax();
		void                                                                 syntax_done(const application::token_span_publication* aSpans);
		const adapters::syntax::packaged_definition_catalog*                 SyntaxDefinitions = nullptr;
		std::unique_ptr<suggestion_text_source>                              SyntaxTexts;
		std::unique_ptr<adapters::syntax::packaged_syntax_classifier>        SyntaxClassifier;
		std::unique_ptr<code_syntax_relay>                                   SyntaxRelay;
		std::unique_ptr<application::syntax_classification_application_host> SyntaxHost;
		std::deque<wanted_syntax>                                            SyntaxWanted;
		std::optional<wanted_syntax>                                         SyntaxClassifying;
		std::unique_ptr<local_draft_corpus>                                  SearchCorpus;
		std::unique_ptr<worker_executor>                                     SearchLane;
		std::unique_ptr<pr_search_controller>                                Find;
		bool                                                                 BringForward = false;
		bool                                                                 Closing      = false;
	};
}

#endif
