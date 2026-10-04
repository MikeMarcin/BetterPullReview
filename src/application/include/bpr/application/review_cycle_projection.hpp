#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_PROJECTION_HPP

#include "bpr/application/review_material_bridge.hpp"
#include "bpr/core/review_cycle_association.hpp"
#include "bpr/core/review_cycle_feedback.hpp"
#include "bpr/core/review_cycle_material.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "bpr/core/review_cycle_share.hpp"
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

	/// \brief A save that would replace other bytes, waiting for the
	/// reviewer's word (BPR-1161). Nothing was written.
	struct review_cycle_replace_request
	{
		core::cycle_share_route     Route = core::cycle_share_route::save_document;
		std::string                 Path;
		std::string                 Name;
		core::cycle_material_digest Existing;
		std::uint64_t               Size                                                   = 0;
		auto                        operator<=>(const review_cycle_replace_request&) const = default;
	};

	/// \brief One note's code as the share sheet shows it (BPR-1161): the
	/// file it is about, read at the revision it was written against, and
	/// the excerpt the core cut from it.
	struct review_cycle_note_context
	{
		core::cycle_feedback_item_id Item;
		/// The file and revision; empty for a note about a folder or the whole cycle.
		std::string                                   Path;
		std::optional<core::cycle_source_revision_id> Revision;
		/// Whether the file could be read; the excerpt is empty otherwise.
		review_material_status   Status = review_material_status::missing;
		core::cycle_note_excerpt Excerpt;
		/// Suggested changes the note makes to other files, not in this excerpt.
		std::uint64_t Elsewhere                                           = 0;
		auto          operator<=>(const review_cycle_note_context&) const = default;
	};

	/// \brief Compose feedback and Share feedback (BPR-1161): the draft, the
	/// exact document sharing it sends, the routes and every delivery
	/// observation of the cycle.
	struct review_cycle_share_view
	{
		core::cycle_round_draft     Draft;
		core::cycle_round_selection Selection;
		/// The round this revision freezes, or froze, as.
		core::cycle_round_id Round;
		/// This revision is frozen as `Round`: sharing it again reuses it.
		bool Frozen = false;
		/// A round frozen from an earlier revision of this draft, which
		/// sharing this revision amends.
		std::optional<core::cycle_round_id> Amends;
		/// The readable document exactly as Copy and Save send it, while it is
		/// small enough to show; `DocumentBytes` is its full size either way.
		std::string                             Document;
		std::uint64_t                           DocumentBytes = 0;
		bool                                    DocumentShown = false;
		std::optional<core::cycle_round_digest> Digest;
		std::uint64_t                           Items       = 0;
		std::uint64_t                           Suggestions = 0;
		std::uint64_t                           Sources     = 0;
		/// Why this selection cannot be shared as it stands.
		std::optional<core::cycle_round_refusal> Refusal;
		/// File names a save offers: identities only, never a path.
		std::string                                DocumentName;
		std::string                                BundleName;
		std::vector<core::cycle_share_route_offer> Routes;
		/// Every observation of every round of the cycle, in order.
		std::vector<core::cycle_delivery_record>    Deliveries;
		std::optional<review_cycle_replace_request> Replace;
		/// What to ask an external agent to read, once its round is ready.
		std::string Invocation;
		/// Plain words about the last share, refusal or limit.
		std::string Status;
		/// A goal the reviewer entered that was not saved because the draft
		/// changed elsewhere first (BPR-1168): kept for them to save again.
		std::optional<std::string> UnsavedGoal;
		/// Each listed note's code, short, as far as it has been read.
		std::vector<review_cycle_note_context> Contexts;
		/// The note the larger view shows, with as much of its file as that
		/// view holds; present while the reviewer has it open.
		std::optional<review_cycle_note_context> Larger;
		auto                                     operator<=>(const review_cycle_share_view&) const = default;
	};

	/// \brief What the reviewer chose in Publish as Pull Request (BPR-1177).
	struct review_cycle_publication_form
	{
		core::outgoing_selection_mode Mode = core::outgoing_selection_mode::starting_work_and_improvements;
		/// P: `HEAD`, a `refs/` name or an exact commit, read in the checkout.
		std::string Base = "HEAD";
		/// T: a `refs/` name in the checkout, or empty for none.
		std::string Target;
		std::string Owner;
		std::string Repository;
		/// The existing branch the pull request targets.
		std::string BaseBranch = "main";
		/// The branch to create; it must not exist.
		std::string NewBranch;
		std::string Title;
		/// The description's two fields, as a Review Tour maps them.
		std::string Problem;
		std::string Solution;
		bool        Draft = false;
		/// The commit's author and committer, as the reviewer confirms them.
		std::string                          AuthorName;
		std::string                          AuthorEmail;
		std::vector<core::outgoing_override> Overrides;
		std::vector<std::string>             NormalizeLineEndings;
		auto                                 operator<=>(const review_cycle_publication_form&) const = default;
	};

	/// \brief The account native publication would act as.
	struct review_cycle_publication_account_view
	{
		std::string             Host;
		std::string             Login;
		core::github_account_id Account;
		auto                    operator<=>(const review_cycle_publication_account_view&) const = default;
	};

	/// \brief One recorded publication: its finite plan and its journal.
	struct review_cycle_publication_record
	{
		core::cycle_publication_id Id;
		std::uint64_t              Revision = 0;
		core::publication_plan     Plan;
		core::publication_journal  Journal;
		auto                       operator<=>(const review_cycle_publication_record&) const = default;
	};

	enum class review_cycle_publication_stage : std::uint8_t {
		/// Choosing what goes out and where; nothing is recorded as a plan.
		choosing,
		/// A plan is recorded and waits for the reviewer's confirmation.
		planned,
		/// Confirmed; the steps are being sent.
		publishing,
		/// The pull request exists and is associated.
		linked,
		/// A step was not confirmed: uncertain, refused, drift or cancelled.
		stopped,
	};

	/// \brief Publish as Pull Request (BPR-1177): the three baselines and what
	/// goes out, where and as whom, the exact plan a confirmation authorizes,
	/// and the journal of what was sent.
	struct review_cycle_publication_view
	{
		review_cycle_publication_form                        Form;
		std::optional<review_cycle_publication_account_view> Account;
		/// What the form would send: R0, Rn, P and T and every changed path.
		std::optional<core::outgoing_plan> Outgoing;
		/// Why no outgoing plan could be made, in plain words.
		std::string                            OutgoingProblem;
		std::optional<core::candidate_refusal> Limit;
		/// The publication recorded for this form, while there is one.
		std::optional<review_cycle_publication_record> Current;
		/// The plan's digest a confirmation names, in hexadecimal.
		std::string Digest;
		/// Everything a confirmation authorizes, as text.
		std::string                    Preview;
		review_cycle_publication_stage Stage = review_cycle_publication_stage::choosing;
		bool                           Busy  = false;
		/// Plain words about the last step.
		std::string Status;
		/// After Save Patch: what the reviewer's own Git tools must do.
		std::string Instructions;
		/// The file name Save Patch suggests: an identity, never a path.
		std::string PatchName;
		auto        operator<=>(const review_cycle_publication_view&) const = default;
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
		/// Present while Compose feedback is open.
		std::optional<review_cycle_share_view> Share;
		/// Present while Publish as Pull Request is open (BPR-1177).
		std::optional<review_cycle_publication_view> Publication;
		/// Every pull request this cycle is associated with: published here,
		/// then linked after an external publication. GitHub holds their
		/// shared facts; this cycle keeps its own history.
		std::vector<core::cycle_pull_association> Links;
		review_cycle_activity                     Activity = review_cycle_activity::idle;
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

	/// \brief One changed run of lines and what replaces it.
	struct review_cycle_suggested_edit
	{
		/// Whole original lines; empty for an insertion at a line start.
		core::cycle_source_anchor Selection;
		std::string               Replacement;
	};

	/// \brief One note and its exact suggestions, as the composer hands them
	/// over after an in-place edit of one file (owner amendment to BPR-1156,
	/// 2026-10-01).
	/// \details One edit per separate change, in file order and in the
	/// original file's coordinates; the first is the note's own location.
	struct review_cycle_suggestion_request
	{
		std::vector<review_cycle_suggested_edit> Edits;
		core::cycle_suggestion_intent            Intent = core::cycle_suggestion_intent::exact_replacement;
		/// The note's body: the rationale, as written.
		std::string Rationale;
	};

}

#endif
