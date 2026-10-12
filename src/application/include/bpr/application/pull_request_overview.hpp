#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PULL_REQUEST_OVERVIEW_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PULL_REQUEST_OVERVIEW_HPP

#include "bpr/application/epoch_projection.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/review_ledger_port.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/file_progress_mark.hpp"
#include "bpr/core/review_ledger.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace bpr::application {
	/// \brief What GitHub did to a file in the pull request.
	enum class overview_file_operation : std::uint8_t {
		added,
		modified,
		removed,
		renamed,
		copied,
		type_changed,
		unchanged,
	};

	/// \brief Whether a file changed since my last review.
	///
	/// \details `unknown` is an honest answer: the pull request moved since
	/// my last review, and the set of files that moved with it has not been
	/// resolved. It is never rendered as "unchanged".
	enum class overview_change_since_review : std::uint8_t {
		unknown,
		unchanged,
		changed,
		/// The file did not exist at my last review.
		added,
	};

	/// \brief One file row of the overview's table.
	struct overview_file_row
	{
		std::string                Path;
		std::optional<std::string> PreviousPath;
		/// The folder the row is grouped under, empty at the root.
		std::string                  Folder;
		std::string                  Name;
		overview_file_operation      Operation = overview_file_operation::modified;
		std::uint64_t                Additions = 0U;
		std::uint64_t                Deletions = 0U;
		overview_change_since_review Changed   = overview_change_since_review::unchanged;
		/// My read state, absent when the ledger holds no receipt for the file.
		std::optional<core::file_read_state> Read;
		/// [[BPR-1339]]. The one mark the row shows, `core::progress_mark` of
		/// the receipt: accepted is stronger than read, seen and unread. Absent
		/// with no receipt.
		std::optional<core::file_progress_mark> Mark;
		std::size_t                             ThreadCount           = 0U;
		std::size_t                             UnresolvedThreadCount = 0U;
		/// Unread posts on the file; absent until the ledger keeps post read
		/// state ([[BPR-0648]]).
		std::optional<std::size_t> UnreadCount;
		/// My private note on the file, if I left one.
		std::optional<std::string> Note;

		auto operator<=>(const overview_file_row&) const = default;
	};

	enum class overview_participant_role : std::uint8_t {
		author,
		reviewer,
		observer,
	};

	/// \brief A participant's standing on the pull request.
	enum class overview_signoff : std::uint8_t {
		none,
		/// The author's own standing: the latest upload is theirs.
		uploaded,
		commented,
		approved,
		changes_requested,
		dismissed,
		pending,
	};

	struct overview_participant
	{
		std::string               Login;
		overview_participant_role Role    = overview_participant_role::observer;
		overview_signoff          Signoff = overview_signoff::none;
		/// True for the reviewer this overview is for.
		bool Viewer = false;

		auto operator<=>(const overview_participant&) const = default;
	};

	struct overview_check_summary
	{
		std::size_t Passed  = 0U;
		std::size_t Failed  = 0U;
		std::size_t Pending = 0U;
		std::size_t Skipped = 0U;
		std::size_t Other   = 0U;

		auto operator<=>(const overview_check_summary&) const = default;

		[[nodiscard]] auto total() const noexcept -> std::size_t
		{
			return Passed + Failed + Pending + Skipped + Other;
		}
	};

	/// \brief A bounded, literal result row, in failure/waiting/settled order.
	struct overview_check_row
	{
		std::string Identity, Name, Producer, Kind, Outcome, Description, DetailsUrl;
		std::string Requirement                                  = "Requirement unknown";
		bool        Failed                                       = false;
		bool        Pending                                      = false;
		bool        LinkAvailable                                = false;
		auto        operator<=>(const overview_check_row&) const = default;
	};

	/// \brief What happened since my last review.
	///
	/// \details Every count that the inputs cannot answer is absent rather
	/// than zero: an absent count reads as "not known", a zero as "none".
	struct overview_since_last_review
	{
		/// False while the ledger names no last-reviewed head.
		bool Known = false;
		/// When the update I last reviewed was observed, if it is retained.
		std::optional<core::observed_utc_milliseconds> ReviewedAt;
		/// Updates after the one I last reviewed; absent when that update is
		/// not among the retained rows.
		std::optional<std::size_t> Uploads;
		/// Files that changed since; absent until the change set is resolved.
		std::optional<std::size_t> FilesChanged;
		/// Posts since; absent because the read model carries no post times.
		std::optional<std::size_t> NewComments;
		/// Replies to posts of mine, over the whole pull request.
		std::size_t RepliesToMe = 0U;
		/// Threads I started that are not resolved.
		std::size_t MyUnresolvedThreads = 0U;

		auto operator<=>(const overview_since_last_review&) const = default;
	};

	/// Header facts supplied by retained epoch history, independent of files.
	struct overview_history_update
	{
		bool                                           Known = false;
		std::optional<core::observed_utc_milliseconds> ReviewedAt;
		std::optional<std::size_t>                     Uploads;
		auto                                           operator<=>(const overview_history_update&) const = default;
	};

	[[nodiscard]] auto project_overview_history(std::span<const epoch_row_projection>      aUpdates,
												const std::optional<core::git_commit_oid>& aReviewed) -> overview_history_update;

	struct overview_review_progress
	{
		/// [[BPR-1339]]. Files read or accepted (`core::progress_done`).
		std::size_t                FilesRead    = 0U;
		std::size_t                FilesTotal   = 0U;
		std::size_t                PrivateNotes = 0U;
		std::optional<std::size_t> DraftedComments;

		auto operator<=>(const overview_review_progress&) const = default;
	};

	/// \brief Which one action the overview offers.
	enum class overview_entry_kind : std::uint8_t {
		/// First visit: nothing read, no last review.
		start_review,
		continue_review,
	};

	/// \brief RFC-BPR-0028's overview: what changed and who has signed off.
	///
	/// \details [[BPR-0647]]. Everything here is decided from projections the
	/// host already holds: the accepted snapshot, the private ledger's
	/// receipts and notes, and the retained update rows. The Qt page draws it
	/// and asks for nothing else; where the inputs cannot answer, the field
	/// says so instead of guessing.
	struct pull_request_overview_projection
	{
		std::string               Owner;
		std::string               RepositoryName;
		core::pull_request_number Number{ };
		std::string               Title;
		std::string               Body;
		std::string               AuthorLogin;
		std::string               BaseRef;
		std::string               HeadRef;
		github_pull_request_state State               = github_pull_request_state::open;
		bool                      Draft               = false;
		std::size_t               CommitCount         = 0U;
		std::size_t               ReturnedCommitCount = 0U;
		/// Rows in folder order, ready to be grouped by `Folder`.
		std::vector<overview_file_row>    Files;
		std::vector<overview_participant> Participants;
		overview_since_last_review        SinceLastReview;
		overview_check_summary            Checks;
		std::vector<overview_check_row>   CheckRows;
		bool                              ChecksComplete = true;
		std::string                       ChecksNote;
		std::string                       MergeSummary, PendingReviewers, RequirementDetails;
		overview_review_progress          Progress;
		overview_entry_kind               Entry = overview_entry_kind::start_review;
		/// Where the one action lands: the first unseen change's file.
		std::optional<std::string> EntryPath;

		auto operator<=>(const pull_request_overview_projection&) const = default;
	};

	/// \brief The ledger and history the overview is decided against.
	struct overview_context
	{
		/// Receipts for any of the pull request's files, keyed by the
		/// identity `core::file_identity_of` gives their path.
		std::vector<core::file_receipt>           Receipts;
		std::optional<private_ledger_observation> Private;
		/// The retained update rows, in any order.
		std::vector<epoch_row_projection> Updates;
		/// The accepted revision's head, when the epoch host has one.
		std::optional<core::git_commit_oid> Head;
		/// Paths changed between my last review and the head, when resolved.
		std::optional<std::vector<std::string>> ChangedSinceReviewPaths;
		std::optional<std::size_t>              DraftedComments;
	};

	/// \brief Decides the overview from the snapshot and the context.
	[[nodiscard]] auto project_pull_request_overview(
		const github_pull_request_snapshot& aSnapshot,
		const overview_context&             aContext)
		-> pull_request_overview_projection;

	/// \brief What one or more durable receipt changes did to a published overview.
	///
	/// \details [[BPR-0972]]. The changed rows carry their new read state and
	/// mark ([[BPR-1339]]); the progress and the one action are the overview's
	/// new values. Nothing else in the overview is a function of receipts.
	struct overview_receipt_update
	{
		std::vector<overview_file_row> Rows;
		overview_review_progress       Progress;
		overview_entry_kind            Entry = overview_entry_kind::start_review;
		std::optional<std::string>     EntryPath;

		auto operator<=>(const overview_receipt_update&) const = default;
	};

	/// \brief Applies receipt changes to a published overview one file at a time.
	///
	/// \details [[BPR-0972]]. `adopt` indexes a complete projection once, in
	/// O(N log N). `apply` then answers each changed receipt with a binary
	/// search and keeps the rows not yet done ordered, so the first such file
	/// is known without walking the files. It decides exactly what
	/// `project_pull_request_overview` decides for the same inputs; a file it
	/// was not given refuses the update, and the caller projects again.
	class overview_receipt_index final
	{
	public:
		/// \brief Indexes a projection built with `aReceipts` and a known or
		/// absent last-reviewed head.
		void adopt(const pull_request_overview_projection& aProjection,
				   bool                                    aLastReviewedKnown,
				   bool                                    aAnyReceipt);

		/// \brief Forgets the adopted projection.
		void clear() noexcept;

		/// \brief Applies durable receipts to the adopted projection.
		/// \returns The changed rows and new summary, or nothing when a receipt
		/// names a file the projection does not hold.
		[[nodiscard]] auto apply(pull_request_overview_projection&   aProjection,
								 std::span<const core::file_receipt> aReceipts)
			-> std::optional<overview_receipt_update>;

	private:
		std::vector<std::pair<core::file_identity, std::size_t>> Rows;
		/// Rows not yet done: neither read nor accepted ([[BPR-1339]]).
		std::set<std::size_t>      NotDone;
		std::optional<std::size_t> FirstChanged;
		bool                       Adopted           = false;
		bool                       LastReviewedKnown = false;
		bool                       AnyReceipt        = false;
	};

	/// \brief Retains the update rows the epoch host publishes.
	///
	/// \details The host publishes its catalog as a stream of row changes;
	/// this keeps the same rows the review-history model keeps, so the
	/// overview counts the updates the epoch line shows.
	class epoch_row_ledger final
	{
	public:
		void               apply(const epoch_catalog_projection& aProjection);
		void               clear();
		[[nodiscard]] auto rows() const noexcept
			-> const std::vector<epoch_row_projection>&;

	private:
		std::optional<core::epoch_catalog_identity> Catalog;
		std::vector<epoch_row_projection>           Rows;
	};
} // namespace bpr::application

#endif
