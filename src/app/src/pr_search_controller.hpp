#ifndef BPR_SRC_APP_SRC_PR_SEARCH_CONTROLLER_HPP
#define BPR_SRC_APP_SRC_PR_SEARCH_CONTROLLER_HPP
#include "bpr/application/source_pr_search_execution.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/source_search_panel.hpp"
#include <QTimer>
namespace bpr::app {
	/// \brief Window-owned PR query, worker and fully sealed ordinary-file reveal.
	class pr_search_controller final : public application::source_search_projection_port
	{
	public:
		/// \brief [[BPR-PENDING-T]]. The tab a result opens in: the active one,
		/// or one of its own (the result menu's Open in New Tab).
		enum class result_tab : std::uint8_t {
			active,
			new_tab,
		};
		using scope_supplier   = std::function<std::optional<core::source_search_scope>()>;
		using open_file        = std::function<std::optional<core::document_tab_id>(const core::source_search_scope&, const application::source_corpus_entry&, result_tab)>;
		using tab_supplier     = std::function<std::optional<core::document_tab_id>()>;
		using reveal_validator = std::function<bool(const core::source_search_scope&, core::file_identity, const std::string&, const core::source_match_location&)>;
		pr_search_controller(qt::source_search_panel& aPanel, qt::diff_workbench& aSurface, application::source_corpus_port& aCorpus, const core::source_regex_engine& aEngine, application::decision_executor& aDecisions, application::effect_executor& aEffects, scope_supplier aScope, open_file aOpen, reveal_validator aValidate, tab_supplier aTab);
		~pr_search_controller() override;
		void               open();
		void               dispatch(qt::source_find_gesture aGesture);
		void               navigate(core::change_navigation_direction aDirection);
		void               activate(std::uint32_t aIndex, result_tab aTab = result_tab::active);
		void               observe_scope();
		void               cancel_reveal();
		void               reset();
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const -> bool;
		[[nodiscard]] auto counters() const -> application::source_search_host_counters;
		[[nodiscard]] auto statistics() const -> application::source_search_statistics;
		void               publish(const application::source_search_projection& aProjection) override;

		void observe_work(std::shared_ptr<application::insights_observer> aObserver) { Host.observe_work(std::move(aObserver)); }

	private:
		qt::source_search_panel&                Panel;
		qt::diff_workbench&                     Surface;
		scope_supplier                          Scope;
		open_file                               OpenFile;
		reveal_validator                        Validate;
		tab_supplier                            Tab;
		application::source_pr_search_execution Execution;
		application::source_search_host         Host;
		struct reveal
		{
			core::source_search_scope            Scope;
			std::optional<core::document_tab_id> Tab;
			std::uint64_t                        Generation;
			std::uint64_t                        ReadingGeneration;
			core::file_identity                  File;
			std::string                          Path;
			core::source_match_location          Location;
		};
		std::optional<reveal> Pending;
		QTimer                RevealTimer;
		std::uint64_t         ReadingGeneration = 0U;
		void                  try_reveal();
		/// The next or previous match the panel's side filter shows, wrapping.
		[[nodiscard]] auto shown_neighbour(core::change_navigation_direction aDirection) const -> std::optional<std::uint32_t>;
	};
}
#endif
