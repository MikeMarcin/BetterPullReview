#ifndef BPR_SRC_APP_SRC_REVIEW_CYCLE_DESKTOP_HPP
#define BPR_SRC_APP_SRC_REVIEW_CYCLE_DESKTOP_HPP

#include "bpr/app/worker_executor.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_program_resolution.hpp"
#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/claude_process_wrapper.hpp"
#include "bpr/application/github_publication_port.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/review_cycle_promotion.hpp"
#include "bpr/application/review_cycle_send.hpp"
#include "bpr/application/review_cycle_workbench.hpp"
#include "bpr/application/syntax_classification_application_host.hpp"
#include "bpr/application/tour_owner_channel.hpp"
#include "bpr/application/workspace_source_port.hpp"
#include "bpr/core/agent_profile.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_command.hpp"

#include <array>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bpr::adapters::syntax {
	class packaged_definition_catalog;
	class packaged_syntax_classifier;
}

namespace bpr::qt {
	class appearance_profile;
	class workbench_window;
	class review_cycle_view;
	struct source_syntax_request;
	struct suggestion_syntax_request;
}

namespace bpr::app {
	class desktop_tour_port;
	class suggestion_text_source;
	class code_syntax_relay;

	/// \brief The packaged syntax definitions and the effect lane their
	/// classifier already runs on, so Suggest change is coloured by the same
	/// classifier as the review panes (BPR-1156).
	/// \details The lane must be the one every classifier over these
	/// definitions uses: the definitions load lazily and are not shared across
	/// threads.
	struct review_cycle_syntax
	{
		const adapters::syntax::packaged_definition_catalog* Definitions = nullptr;
		application::effect_executor*                        Effects     = nullptr;
		std::shared_ptr<application::insights_observer>      Insights;
	};

	/// \brief [[BPR-1157]]. Composes local Review Cycles into one desktop window.
	///
	/// \details Owns the read-only workspace source and the cycle surface host
	/// over this profile's own cycle store, adopts the cycle page into the
	/// window and wires File > Open Project…, Home's Review Cycles section and
	/// the page's gestures. Nothing here needs an account, a remote, an agent or
	/// the network; the chosen folder is only read. With `review_cycle_syntax`
	/// it also classifies the file the source pane shows ([[BPR-1157]]) and
	/// what Suggest change shows ([[BPR-1156]]): latest text wins on each
	/// slot, and each surface adopts only spans of the text it still shows.
	/// Share notes' code views ([[BPR-1161]]) ask through a second host over
	/// the same definitions and lane, one text at a time, so a sheet of open
	/// notes never crowds out the file pane or Suggest change.
	class review_cycle_desktop final : public application::review_cycle_workbench_port, private application::appearance_projection_port
	{
	public:
		/// \param aCaptures Where captures run ([[BPR-1184]]); absent, the
		/// desktop runs them on a worker thread of its own. A course on
		/// manual lanes names its lane here.
		review_cycle_desktop(qt::workbench_window& aWindow, const std::filesystem::path& aProfileRoot, core::bpr_profile_id aProfile, application::effect_executor& aEffects,
							 application::decision_executor& aDecisions, review_cycle_syntax aSyntax = { }, application::effect_executor* aCaptures = nullptr,
							 std::shared_ptr<const application::claude_process_wrapper_source> aClaudeWrapper = nullptr, application::claude_locator aLocateClaude = { },
							 std::shared_ptr<const application::agent_program_source> aPrograms = nullptr, application::agent_program_locator aLocatePrograms = { });
		~review_cycle_desktop() override;
		review_cycle_desktop(const review_cycle_desktop&)                    = delete;
		auto operator=(const review_cycle_desktop&) -> review_cycle_desktop& = delete;
		review_cycle_desktop(review_cycle_desktop&&)                         = delete;
		auto operator=(review_cycle_desktop&&) -> review_cycle_desktop&      = delete;

		void publish(const application::review_cycle_workbench_projection& aProjection) override;
		void publish(const std::vector<application::review_cycle_recent>& aRecents) override;

		/// \brief Opens a chosen location as a cycle, as the dialog would.
		void open_project(const std::string& aLocation);
		/// \brief Gives the source pane and Suggest change the applied syntax colours.
		void adopt_appearance(const qt::appearance_profile& aProfile);
		/// \brief Chooses the file Save to File or Save as Bundle writes, given
		/// the suggested name; empty when the reviewer chose none (BPR-1161).
		using save_chooser = std::function<std::string(const std::string& aSuggested, bool aBundle)>;
		/// \brief Replaces the native save dialog, for a course that cannot answer one.
		void set_save_chooser(save_chooser aChooser);
		/// \brief What Publish as Pull Request uses ([[BPR-1177]]): the installed
		/// Git the local catalog runs, in its own owner-private isolation root
		/// under the profile, and the account native publication acts as, asked
		/// each time it is needed. Without an account, planning and Save Patch
		/// still work and nothing can be sent.
		struct publishing
		{
			std::filesystem::path                                                   GitExecutable;
			std::optional<std::filesystem::path>                                    WindowsSystemRoot;
			std::function<std::optional<application::github_publication_account>()> Account;
		};
		void set_publishing(publishing aPublishing);
		/// \brief [[BPR-1273]]. Where Add Notes to Review Tour edits a pull
		/// request's tour: the profile's tour owner, which runs a request on the
		/// window holding that draft, or on the stored draft, as `bpr tour`
		/// commands are run. Without one, no note can be added.
		using tour_route = std::function<void(application::tour_channel_bytes, std::function<void(application::tour_channel_bytes)>)>;
		void set_tour_route(tour_route aRoute);
		/// \brief [[BPR-1180]]. Replaces the agent profiles Send to Agent offers,
		/// keeping the installed adapter: a course points them at its own
		/// executables. Nothing changes where no adapter is installed.
		void set_send_profiles(std::vector<core::agent_profile> aProfiles);
		/// \brief [[BPR-1749]], RFC-BPR-0092. The profile setup saved
		/// is offered first; the others keep their order. Absent, the order is
		/// the one given.
		void prefer_send_profile(std::optional<core::agent_profile_id> aProfile);
		/// \brief [[BPR-1753]], owner decision 2026-10-23. Send to Agent
		/// offers the agents the reviewer added and launches each as its entry
		/// says; with none added the profiles given stay.
		void offer_agents(std::vector<core::agent_entry> aEntries);
		/// \brief Replaces where the account comes from, keeping the catalog.
		void set_publication_account(std::function<std::optional<application::github_publication_account>()> aAccount);
		/// \brief What this window holds of cycle `aCycle`'s round draft that
		/// the store does not (BPR-1168), for a forwarded terminal command.
		[[nodiscard]] auto draft_editor(core::review_cycle_id aCycle) const -> core::cycle_draft_editor;
		/// \brief Saves the goal typed for cycle `aCycle`'s draft, as Return would.
		void save_draft_editor(core::review_cycle_id aCycle);
		/// \brief A forwarded terminal command wrote cycle `aCycle`: show it as it is now.
		void external_change(core::review_cycle_id aCycle);
		/// \brief The window's pull request took the review surface back.
		void               leave();
		[[nodiscard]] auto host() noexcept -> application::review_cycle_workbench&;
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		/// The text a surface last asked about on one slot, and whether it
		/// has been handed to the classifier yet.
		struct wanted_syntax
		{
			application::token_span_cache_key  Key;
			std::shared_ptr<const std::string> Document;
			bool                               Submitted = false;
		};

		/// The composer's two sides, then the source pane.
		static constexpr std::size_t source_slot = 2U;

		/// Share feedback by one route; Save asks for the file first.
		void share(core::cycle_share_route aRoute);
		/// Save Patch asks for the file first ([[BPR-1177]]).
		void save_patch();
		/// Open Pull Request: the Home bar opens the linked pull request, as a pasted link does.
		void open_pull_request();
		/// The Home bar opens `aAddress` in the existing pull request workflow.
		void open_address(const std::string& aAddress);
		/// Add Notes to Review Tour for the cycle's latest link ([[BPR-1273]]).
		void open_promotion();
		void open_promotion_pull_request();
		void want_syntax(const qt::suggestion_syntax_request& aRequest);
		void want_source_syntax(const qt::source_syntax_request& aRequest);
		void want(std::size_t aSlot, const std::string& aPath, const std::string& aDocument);
		void submit_syntax();
		void want_code_syntax(const qt::source_syntax_request& aRequest);
		void submit_code_syntax();
		/// The code host finished the text it was classifying, with `aSpans` or none.
		void code_syntax_done(const application::token_span_publication* aSpans);
		void publish(const application::appearance_projection& aProjection) override;
		void fail(const application::appearance_failure_publication& aFailure) override;
		void rejected(application::appearance_terminal_rejection aRejection) override;

		qt::workbench_window&                               Window;
		qt::review_cycle_view*                              View = nullptr;
		std::unique_ptr<application::workspace_source_port> Source;
		/// [[BPR-1184]]. Captures run on this worker unless the composition
		/// named a lane, so the reviewer reads and moves between files while
		/// one runs and no other workbench effect waits. Declared before the
		/// host, which it outlives.
		std::unique_ptr<worker_executor> OwnCaptures;
		/// [[BPR-1180]]. Send to Agent's process adapter and the worker a send
		/// runs on, declared before the host, which they outlive. Composed only
		/// with the agent integrations; an agent-free build has neither.
		/// [[BPR-1301]]. The supervised launcher, and in front of it the port
		/// that points a Claude send at the window's profile wrapper.
		std::unique_ptr<application::agent_process_port> SendLauncher;
		/// [[BPR-1749]]. The profiles Send to Agent offers, as given,
		/// and the one setup saved, offered first.
		std::vector<core::agent_profile>      SendProfiles;
		std::optional<core::agent_profile_id> PreferredSendProfile;
		/// [[BPR-1748]]. The Claude wrapper's port, in front of which
		/// `SendProcesses` gives Codex and the plain-text harness a program.
		std::unique_ptr<application::agent_process_port>     SendClaude;
		std::unique_ptr<application::agent_process_port>     SendProcesses;
		std::shared_ptr<application::agent_entry_source>     SendEntries;
		std::unique_ptr<application::agent_process_port>     SendEntryProcess;
		std::unique_ptr<worker_executor>                     SendLane;
		std::shared_ptr<application::review_cycle_sender>    Sender;
		std::unique_ptr<application::review_cycle_workbench> Host;
		/// [[BPR-1273]]. The tour owner Add Notes to Review Tour adds through,
		/// declared before the promoter, which it outlives.
		std::unique_ptr<desktop_tour_port>                                   Tours;
		std::unique_ptr<application::review_cycle_promoter>                  Promoter;
		core::local_cycle_subject                                            LastSubject;
		bool                                                                 Closing           = false;
		const adapters::syntax::packaged_definition_catalog*                 SyntaxDefinitions = nullptr;
		std::unique_ptr<suggestion_text_source>                              SyntaxTexts;
		std::unique_ptr<adapters::syntax::packaged_syntax_classifier>        SyntaxClassifier;
		std::unique_ptr<application::syntax_classification_application_host> SyntaxHost;
		std::array<std::optional<wanted_syntax>, 3>                          Wanted;
		/// Share notes' code views: their texts wait here, newest last and
		/// bounded, and one is classified at a time.
		std::unique_ptr<suggestion_text_source>                              CodeTexts;
		std::unique_ptr<adapters::syntax::packaged_syntax_classifier>        CodeClassifier;
		std::unique_ptr<code_syntax_relay>                                   CodeRelay;
		std::unique_ptr<application::syntax_classification_application_host> CodeHost;
		std::deque<wanted_syntax>                                            CodeWanted;
		std::optional<wanted_syntax>                                         CodeClassifying;
		/// The platform clipboard Copy to Clipboard writes to (BPR-1161).
		std::unique_ptr<application::review_cycle_clipboard> Clipboard;
		save_chooser                                         ChooseSave;
		/// Where Publish as Pull Request's catalog runs, and whose account it acts as.
		std::filesystem::path ProfileRoot;
		publishing            Publishing;

		/// Whether the reviewer has asked for a cycle, so a refusal is said on
		/// the page rather than taking Home over at start.
		bool Asked = false;
		/// \brief [[BPR-1171]]. Whether the reviewer's own Open Project or resume
		/// is still to be answered: only that answer brings the cycle page
		/// forward. A projection after another interface wrote the cycle, or
		/// after any background step, leaves Home or a pull request in front.
		bool BringForward = false;
	};
}

#endif
