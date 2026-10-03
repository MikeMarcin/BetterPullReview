#ifndef BPR_SRC_APP_SRC_REVIEW_CYCLE_DESKTOP_HPP
#define BPR_SRC_APP_SRC_REVIEW_CYCLE_DESKTOP_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/review_cycle_workbench.hpp"
#include "bpr/application/syntax_classification_application_host.hpp"
#include "bpr/application/workspace_source_port.hpp"
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
		review_cycle_desktop(qt::workbench_window& aWindow, const std::filesystem::path& aProfileRoot, core::bpr_profile_id aProfile, application::effect_executor& aEffects,
							 application::decision_executor& aDecisions, review_cycle_syntax aSyntax = { });
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

		qt::workbench_window&                                                Window;
		qt::review_cycle_view*                                               View = nullptr;
		std::unique_ptr<application::workspace_source_port>                  Source;
		std::unique_ptr<application::review_cycle_workbench>                 Host;
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

		/// Whether the reviewer has asked for a cycle, so a refusal is said on
		/// the page rather than taking Home over at start.
		bool Asked = false;
	};
}

#endif
