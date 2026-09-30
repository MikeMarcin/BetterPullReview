#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_WORKBENCH_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_WORKBENCH_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/review_cycle_projection.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "bpr/core/review_cycle_tabs.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief The desktop's local Review Cycles surface host (RFC-BPR-0061,
/// BPR-1157): Open Project, recents, the first capture, full-source tabs and
/// notes, with no account, provider or agent.
/// \details It composes the cycle runtime with the read-only workspace
/// source and publishes one presentation projection. Every store and
/// workspace call runs on the effect lane through the runtime; every
/// projection is published on the decision lane. A late completion for a
/// cycle that is no longer open is dropped.
namespace bpr::application {
	class review_cycle_workbench final
	{
	public:
		/// Files listed at most, from bounded comparison pages.
		static constexpr std::size_t maximum_listed_files = 4096U;
		static constexpr std::size_t maximum_recents      = 32U;

		/// \param aSeed A random seed for operation identities, chosen by the composition.
		review_cycle_workbench(std::unique_ptr<review_cycle_store> aStore, workspace_source_port& aSource, effect_executor& aEffects, decision_executor& aDecisions,
							   review_cycle_workbench_port& aPort, const std::array<std::uint8_t, 32>& aSeed);
		~review_cycle_workbench();
		review_cycle_workbench(const review_cycle_workbench&)                    = delete;
		auto operator=(const review_cycle_workbench&) -> review_cycle_workbench& = delete;
		review_cycle_workbench(review_cycle_workbench&&)                         = delete;
		auto operator=(review_cycle_workbench&&) -> review_cycle_workbench&      = delete;

		/// \brief Publishes this profile's recent cycles.
		void refresh_recents();
		/// \brief Opens a chosen folder or file: resumes the cycle bound to the
		/// same root, or creates, binds and captures R0 of a new one.
		void open_project(std::string aLocation);
		/// \brief Opens a known cycle.
		void resume(core::local_cycle_subject aSubject);
		/// \brief Leaves the cycle; nothing is lost.
		void close();
		void tab(const core::document_tab_intent& aIntent);
		void open_file(const std::string& aPath, bool aNewTab);
		void observe_line(core::document_tab_id aTab, std::uint64_t aLine);
		/// \brief Adds a note on the open cycle.
		void comment(core::begin_cycle_feedback aNote);
		/// \brief Adds a note carrying one exact suggestion.
		void suggest(review_cycle_suggestion_request aRequest);
		/// \brief Links another location to a note; the note's own location stays.
		void relate(const core::cycle_feedback_item_ref& aItem, std::uint64_t aVersion, core::cycle_related_reference aReference);

		/// \brief Capture revision (BPR-1163): reads the bound folder now, on
		/// the reviewer's gesture only. Identical bytes record nothing unless
		/// `aDeliberate` asks for another observation. Open tabs keep their
		/// revisions; new tabs take the new capture.
		void capture_revision(bool aDeliberate = false);
		/// \brief Shows a named comparison of the active tab's file.
		void compare(const core::cycle_view_request& aRequest);
		/// \brief Returns the active tab to full source.
		void full_source();
		/// \brief The explicit refresh: a semantic view re-resolves, full
		/// source moves to the latest capture; a chosen pair never moves.
		void refresh_view();
		/// \brief Marks the active tab's right revision (or the latest capture)
		/// as reviewed: the checkpoint "Since my last review" starts from.
		void mark_reviewed();

		[[nodiscard]] auto projection() const noexcept -> const review_cycle_workbench_projection&;
		[[nodiscard]] auto selected() const noexcept -> std::optional<core::local_cycle_subject>;
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}

#endif
