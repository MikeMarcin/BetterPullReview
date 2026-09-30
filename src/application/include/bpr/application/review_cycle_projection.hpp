#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_PROJECTION_HPP

#include "bpr/application/review_material_bridge.hpp"
#include "bpr/core/review_cycle_feedback.hpp"
#include "bpr/core/review_cycle_material.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "bpr/core/review_cycle_suggestion.hpp"
#include "bpr/core/review_cycle_tabs.hpp"
#include "bpr/core/review_cycle_verification.hpp"
#include "bpr/core/review_cycle_views.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief What presentation receives of a local Review Cycle, and what it
/// hands back (RFC-BPR-0061, BPR-1157). Values only: no store, runtime,
/// workspace or effect.
namespace bpr::application {
	struct review_cycle_file_entry
	{
		std::string              Path;
		core::cycle_path_state   State                                             = core::cycle_path_state::present;
		core::material_text_kind Text                                              = core::material_text_kind::empty;
		std::uint64_t            Size                                              = 0;
		auto                     operator<=>(const review_cycle_file_entry&) const = default;
	};

	/// \brief One note as the surface lists it: its current wording and
	/// where its original attachment stands in the active tab's revision.
	struct review_cycle_note_entry
	{
		core::cycle_feedback_item_ref              Item;
		core::cycle_feedback_target                Primary;
		std::vector<core::cycle_related_reference> Related;
		std::string                                Body;
		core::authored_provenance                  Provenance  = core::authored_provenance::human;
		std::size_t                                Turns       = 0;
		std::size_t                                Suggestions = 0;
		core::private_thread_lifecycle             Lifecycle   = core::private_thread_lifecycle::open;
		/// The thread version a change must name.
		std::uint64_t                                    Version = 0;
		std::optional<core::cycle_target_correspondence> Correspondence;
		/// What recipients said about this note, in append order (BPR-1164).
		std::vector<core::cycle_item_statement> Statements;
		/// Where it stands against the latest capture, each fact kept apart (BPR-1165).
		std::optional<core::cycle_item_reconciliation> Reconciliation;
		auto                                           operator<=>(const review_cycle_note_entry&) const = default;
	};

	/// \brief One appended statement as the surface lists it: how it arrived,
	/// who it claims to be from and what it covers (BPR-1164).
	struct review_cycle_response_entry
	{
		core::cycle_response_ref     Id;
		core::cycle_round_id         Round;
		core::cycle_response_channel Channel = core::cycle_response_channel::manual_entry;
		std::string                  ForeignId;
		std::string                  From;
		/// The author a structured response claims; absent for prose.
		std::optional<core::cycle_claimed_author> ClaimedAuthor;
		/// Items it speaks about; zero for a reply about the round as a whole.
		std::size_t Items = 0;
		/// The body of a reply about the round as a whole.
		std::string Body;
		/// The result revision it names, and whether this cycle captured it.
		std::optional<std::string>                    ClaimedResult;
		std::optional<core::cycle_source_revision_id> CapturedResult;
		auto                                          operator<=>(const review_cycle_response_entry&) const = default;
	};

	/// \brief The active tab's exact retained bytes, read on their own.
	struct review_cycle_source_view
	{
		core::document_tab_id          Tab;
		core::cycle_source_revision_id Revision;
		std::string                    Path;
		review_material_status         Status = review_material_status::failure;
		std::vector<std::uint8_t>      Bytes;
		core::material_text_kind       Text                                               = core::material_text_kind::empty;
		std::uint64_t                  Line                                               = 0;
		auto                           operator<=>(const review_cycle_source_view&) const = default;
	};

	/// \brief The active tab's named comparison of one file (BPR-1163): what
	/// was asked, the exact pair it shows and the shared engine's rows.
	struct review_cycle_comparison_view
	{
		core::document_tab_id                      Tab;
		core::cycle_view_request                   Request;
		core::cycle_comparison_pair                Pair;
		std::string                                Path;
		bool                                       Pinned = false;
		review_material_status                     Status = review_material_status::failure;
		std::optional<core::cycle_path_comparison> File;
		std::vector<core::cycle_diff_row>          Rows;
		core::diff_reason_code                     Reason                                                 = core::diff_reason_code::none;
		auto                                       operator<=>(const review_cycle_comparison_view&) const = default;
	};

	/// \brief The next round's draft the reviewer prepared (BPR-1165).
	struct review_cycle_next_round
	{
		core::cycle_round_draft_ref Draft;
		core::cycle_round_id        From;
		std::size_t                 Carried                                           = 0;
		std::size_t                 Added                                             = 0;
		std::size_t                 Resolved                                          = 0;
		std::size_t                 Archived                                          = 0;
		auto                        operator<=>(const review_cycle_next_round&) const = default;
	};

	enum class review_cycle_activity : std::uint8_t {
		idle,
		opening,
		capturing,
		loading,
		saving,
	};

	struct review_cycle_workbench_projection
	{
		std::optional<core::review_cycle_projection> Cycle;
		/// The root as the reviewer chose it.
		std::string                                    Location;
		std::vector<core::cycle_source_revision_id>    Revisions;
		std::optional<core::cycle_capture_explanation> Quality;
		std::vector<review_cycle_file_entry>           Files;
		/// More files exist than the list holds.
		bool                                    FilesTruncated = false;
		std::vector<core::cycle_tab>            Tabs;
		std::optional<core::document_tab_id>    ActiveTab;
		std::optional<review_cycle_source_view> Source;
		/// Present instead of `Source` when the active tab shows a comparison.
		std::optional<review_cycle_comparison_view> Comparison;
		/// A refresh of the active tab would show newer material; the tab
		/// itself never moves without that gesture.
		bool NewerAvailable = false;
		/// The reviewer's latest checkpoint and each round's bound revision.
		core::cycle_view_facts Facts;
		/// Captures in local order, with their Git and imported chronologies.
		std::optional<core::cycle_history>   History;
		std::vector<review_cycle_note_entry> Notes;
		/// Every statement recipients sent back, in append order.
		std::vector<review_cycle_response_entry> Responses;
		/// The draft prepared for the next round, when one was prepared here.
		std::optional<review_cycle_next_round> NextRound;
		review_cycle_activity                  Activity = review_cycle_activity::idle;
		/// Plain words about the last refusal or failure; empty otherwise.
		std::string Message;
		/// Plain words about the last capture when it recorded nothing new.
		std::string Notice;
		auto        operator<=>(const review_cycle_workbench_projection&) const = default;
	};

	struct review_cycle_recent
	{
		core::review_cycle_projection Cycle;
		std::string                   Location;
		auto                          operator<=>(const review_cycle_recent&) const = default;
	};

	/// \brief Receives the surface's projections on the decision lane.
	class review_cycle_workbench_port
	{
	public:
		virtual ~review_cycle_workbench_port() = default;

		virtual void publish(const review_cycle_workbench_projection& aProjection) = 0;
		virtual void publish(const std::vector<review_cycle_recent>& aRecents)     = 0;
	};

	/// \brief A selection's note and optional exact suggestion, as the
	/// composer hands them over.
	struct review_cycle_suggestion_request
	{
		core::cycle_source_anchor     Selection;
		std::string                   Replacement;
		core::cycle_suggestion_intent Intent = core::cycle_suggestion_intent::exact_replacement;
		/// The note's body: the rationale, as written.
		std::string Rationale;
	};

}

#endif
