#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_PROMOTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_PROMOTION_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/review_cycle_projection.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/application/review_tour_host.hpp"
#include "bpr/application/review_tour_store.hpp"
#include "bpr/core/review_cycle_promotion.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief [[BPR-1273]]. Add Notes to Review Tour: the reviewer chooses notes
/// of a linked cycle and adds them, unchanged, as stops of the linked pull
/// request's Review Tour (RFC-BPR-0059, RC59-09).
/// \details Adding is a local edit of the tour's private draft, made where
/// that draft lives: through `review_cycle_tour_port`, which the composition
/// gives the profile's tour owner (an open window's tour, or the stored
/// draft). Nothing is sent to GitHub; the tour is published only from the
/// pull request, through its own preview and confirmation. Each note is one
/// request whose identity is the cycle, the note and the pull request, so a
/// request whose answer was lost is asked again under the same identity and
/// the tour's receipts answer it without a second stop.
namespace bpr::application {
	/// \brief Why the tour owner did not answer with the draft.
	enum class review_cycle_tour_failure : std::uint8_t {
		/// The draft changed since it was read: read it again.
		conflict,
		/// The tour refused the request: a full or read-only tour, or the
		/// same request identity with other content.
		refused,
		/// The owner is busy, unavailable or did not answer: whether the
		/// request took effect is unknown until it is asked again.
		unanswered,
	};

	/// \brief The tour's draft as its owner holds it after a read or an edit,
	/// with the receipt of the request just asked, when there was one.
	struct review_cycle_tour_answer
	{
		core::review_tour                   Draft;
		std::optional<tour_request_receipt> Receipt;
	};
	using review_cycle_tour_result = std::expected<review_cycle_tour_answer, review_cycle_tour_failure>;

	/// \brief Where a pull request's Review Tour lives. Replies may arrive on
	/// any thread, once each; the promoter moves them onto its decision lane.
	class review_cycle_tour_port
	{
	public:
		using targets_reply               = std::function<void(std::expected<std::vector<tour_saved_target>, review_cycle_tour_failure>)>;
		using reply                       = std::function<void(review_cycle_tour_result)>;
		virtual ~review_cycle_tour_port() = default;
		/// \brief The tours this profile knows: one for each pull request opened here.
		virtual void targets(targets_reply aReply) = 0;
		/// \brief The draft of one tour as its owner holds it now, with the
		/// receipt of request `aRequest` when the tour holds one.
		virtual void load(const tour_saved_target& aTarget, std::string aRequest, reply aReply) = 0;
		/// \brief One request with an identity, answered once it is durable.
		virtual void execute(const tour_saved_target& aTarget, tour_service_request aRequest, reply aReply) = 0;
	};

	/// \brief Whether the linked pull request's tour was found.
	enum class review_cycle_promotion_target : std::uint8_t {
		/// Reading the notes and finding the tour.
		looking,
		/// Found: notes can be added.
		ready,
		/// The pull request was never opened on this profile, so it has no tour yet.
		not_opened,
		/// More than one account's tour for the pull request; open it to choose.
		ambiguous,
		/// The tours could not be read.
		unavailable,
	};

	/// \brief What adding one note did.
	enum class review_cycle_promotion_outcome : std::uint8_t {
		none,
		/// A new stop, now in the tour.
		added,
		/// The tour already held this note's stop: nothing changed.
		already,
		/// The tour refused it.
		refused,
		/// No answer: asking again adds it at most once.
		unanswered,
	};

	/// \brief One note as the sheet lists it.
	struct review_cycle_promotion_row
	{
		core::cycle_feedback_item_ref Item;
		core::cycle_feedback_target   Primary;
		/// The words the stop takes, unchanged.
		std::string Body;
		/// Where its stop points; absent when the note cannot be promoted.
		std::optional<core::cycle_promotion_source>  Source;
		std::optional<core::cycle_promotion_refusal> Refusal;
		/// The exact lines, when proved.
		std::optional<core::tour_code_destination> Lines;
		/// What the tour's own policy would make of it on GitHub, once the
		/// tour's head is known.
		std::optional<core::tour_destination_plan> Planned;
		bool                                       Chosen                                                      = false;
		review_cycle_promotion_outcome             Outcome                                                     = review_cycle_promotion_outcome::none;
		auto                                       operator==(const review_cycle_promotion_row&) const -> bool = default;
	};

	/// \brief Add Notes to Review Tour, as presentation shows it.
	struct review_cycle_promotion_view
	{
		core::cycle_pull_association            Link;
		std::vector<review_cycle_promotion_row> Rows;
		review_cycle_promotion_target           Target = review_cycle_promotion_target::looking;
		/// The tour's draft head, when found.
		std::optional<core::git_commit_oid> TourHead;
		bool                                Busy = false;
		/// Plain words about the last step.
		std::string Status;
		auto        operator==(const review_cycle_promotion_view&) const -> bool = default;
	};

	/// \brief The host of Add Notes to Review Tour for one window.
	/// \details Reads on the effect lane, decides and publishes on the
	/// decision lane, and adds through the tour port one note at a time.
	class review_cycle_promoter final
	{
	public:
		using publication = std::function<void(const std::optional<review_cycle_promotion_view>&)>;
		review_cycle_promoter(std::unique_ptr<review_cycle_store> aStore, review_cycle_tour_port& aTours, effect_executor& aEffects, decision_executor& aDecisions, publication aPublish);
		~review_cycle_promoter();
		review_cycle_promoter(const review_cycle_promoter&)                    = delete;
		auto operator=(const review_cycle_promoter&) -> review_cycle_promoter& = delete;
		review_cycle_promoter(review_cycle_promoter&&)                         = delete;
		auto operator=(review_cycle_promoter&&) -> review_cycle_promoter&      = delete;

		/// \brief Opens the sheet for cycle `aSubject`, listing `aNotes` as the
		/// cycle shows them, for the most recent of `aLinks` whose pull request
		/// has a tour on this profile (the latest link when none has one yet).
		/// Nothing is chosen and nothing is added.
		void open(core::local_cycle_subject aSubject, std::vector<core::cycle_pull_association> aLinks, const std::vector<review_cycle_note_entry>& aNotes);
		/// \brief Chooses a note, or leaves it out.
		void choose(core::cycle_feedback_item_id aItem, bool aChosen);
		/// \brief Adds the chosen notes to the tour, in the cycle's order.
		void add();
		/// \brief Reads the tours and the notes again, after the pull request was opened.
		void refresh();
		/// \brief Leaves the sheet; what was added stays in the tour.
		void               close();
		[[nodiscard]] auto view() const noexcept -> const std::optional<review_cycle_promotion_view>&;
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};

	/// \brief The note's current wording and location, as the cycle lists it.
	[[nodiscard]] auto promotion_note(const review_cycle_note_entry& aEntry) -> core::cycle_promotion_note;
	/// \brief Whether `aTarget` is the tour of `aLink`'s pull request: its
	/// host, owner, repository and number, under any account.
	[[nodiscard]] auto names_pull_request(const tour_saved_target& aTarget, const core::cycle_pull_address& aPull) -> bool;
}

#endif
