#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_WORKBENCH_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_WORKBENCH_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/review_cycle_projection.hpp"
#include "bpr/application/review_cycle_response.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "bpr/core/review_cycle_tabs.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
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

		/// \brief How a chosen file is read (BPR-1164): on the effect lane,
		/// within the bound, never following a link. Set by the composition.
		using file_reader = std::function<std::expected<std::vector<std::uint8_t>, cycle_export_failure>(const std::string&, std::uint64_t, std::stop_token)>;
		void set_file_reader(file_reader aReader);
		/// \brief Appends the structured response in the chosen file to round `aRound`.
		void import_response(core::cycle_round_id aRound, std::string aPath);
		/// \brief Appends a reply the reviewer entered for round `aRound`.
		void attach_reply(core::cycle_round_id aRound, core::cycle_manual_reply aReply);

		/// \brief The reviewer's decision about a note (BPR-1165), checked
		/// against the revision the active tab shows on its right (or the
		/// latest capture); absent clears the earlier decision.
		void decide(const core::cycle_feedback_item_ref& aItem, std::optional<core::cycle_reviewer_decision> aDecision);
		/// \brief Saves the next round's draft from the latest frozen round and
		/// the notes still open. Nothing is frozen, copied or sent.
		void prepare_next_round();

		/// \brief What Share feedback may use (BPR-1161), set by the
		/// composition: the configured disclosure policy and the routes this
		/// installation can reach. Without a setting, the defaults apply and
		/// only Copy and Save are reachable.
		void set_share_environment(core::cycle_disclosure_policy aPolicy, core::cycle_share_capabilities aCapabilities);
		/// \brief The clipboard Copy writes to; it must outlive the host.
		void set_clipboard(review_cycle_clipboard* aClipboard);
		/// \brief Makes the destination for a chosen file; used on the effect lane only.
		using destination_factory = std::function<std::unique_ptr<cycle_export_destination>(const std::string&)>;
		void set_destination_factory(destination_factory aFactory);

		/// \brief Compose feedback: opens the cycle's latest round draft, or
		/// starts the first with every open note, and shows exactly what
		/// sharing it sends. Nothing is frozen or sent.
		void compose();
		/// \brief Leaves Compose feedback; the draft stays as saved.
		void close_compose();
		/// \brief Saves one edit to the draft as its next revision.
		void edit_composition(core::cycle_compose_edit aEdit);
		/// \brief Freezes the shown revision and shares it by one route, in one
		/// gesture: Copy to Clipboard, Save to File or Save as Bundle to
		/// `aPath`, or Use in external agent. A refused route shares nothing
		/// and uses no other route instead. What was observed is recorded.
		void share(core::cycle_share_route aRoute, std::string aPath = { });
		/// \brief Answers a pending replace: replace exactly the bytes named, or keep them.
		void resolve_replace(bool aReplace);

		/// \brief Another interface wrote cycle `aCycle` (BPR-1168): when it is
		/// the open cycle, read it again, and Compose feedback's draft with it.
		/// Nothing the reviewer is typing is touched.
		void observe_external_change(core::review_cycle_id aCycle);
		/// \brief A change to Compose feedback's draft is waiting or being saved.
		[[nodiscard]] auto saving_draft() const noexcept -> bool;

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
