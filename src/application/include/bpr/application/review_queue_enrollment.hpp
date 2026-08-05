#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_ENROLLMENT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_ENROLLMENT_HPP

#include "bpr/application/review_queue_refresh.hpp"
#include "bpr/core/review_queue.hpp"
#include "bpr/core/review_queue_attention.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace bpr::application {
	/// \brief Why an enrolled pull request left the queue.
	///
	/// \details A pull request that simply disappeared would leave the reviewer
	/// wondering whether they had imagined it, so every departure has a reason
	/// they can read.
	enum class enrollment_departure : std::uint8_t {
		// Still here.
		none,
		// The pull request was closed or merged.
		closed,
		// This account can no longer read it.
		access_lost,
		// It no longer exists under this identity — transferred, or deleted.
		gone,
		// The reviewer removed it.
		removed_by_reviewer,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(enrollment_departure aDeparture) noexcept
		-> std::string_view;

	/// \brief Returns the sentence a reviewer reads about a departure.
	[[nodiscard]] auto explanation(enrollment_departure aDeparture) noexcept
		-> std::string_view;

	/// \brief One enrolled pull request, as the host retains it.
	///
	/// \details `Membership` is kept as it was when the item was enrolled, not
	/// recomputed on every refresh. That is what makes enrollment survive the
	/// path that matched: a reviewer watching `src/parser` keeps the pull
	/// request when the last parser file leaves it, because the reason they are
	/// watching has not stopped being true — it is why they wanted to see the
	/// change in the first place.
	///
	/// A recomputing queue would drop exactly the pull requests a path rule
	/// exists to catch.
	struct enrollment_record
	{
		core::pull_request_ref Ref;
		core::queue_membership Membership;

		enrollment_departure Departed = enrollment_departure::none;

		[[nodiscard]] auto present() const noexcept -> bool
		{
			return Departed == enrollment_departure::none;
		}

		auto operator<=>(const enrollment_record&) const = default;
	};

	/// \brief Retains enrollments and projects the queue from them.
	///
	/// \details Holds no provider, no clock, and no database. The durable store
	/// is a separate concern; this is the policy that decides what is retained
	/// and what the queue shows.
	class queue_enrollment_register final
	{
	public:
		/// \brief Enrolls or re-enrolls one pull request.
		///
		/// \details Enrolling something already present updates its membership
		/// only when it gained a GitHub role. A rule match must not overwrite a
		/// role, and a role must not overwrite the rule that first brought the
		/// item in — the reviewer is owed both reasons.
		void enroll(
			const core::pull_request_ref& aRef,
			const core::queue_membership& aMembership);

		/// \brief Records that an item left, and why.
		void depart(
			const core::pull_request_ref& aRef,
			enrollment_departure          aDeparture);

		/// \brief Applies what a refresh concluded about one item.
		///
		/// \details `withdrawn` is the only disposition that moves an item out,
		/// and it needs the status to say which departure it was: forbidden and
		/// gone are different facts with different next steps.
		void observe(
			const queue_refresh_completion& aCompletion,
			refresh_disposition             aDisposition);

		/// \brief Every retained enrollment, departed ones included.
		[[nodiscard]] auto records() const noexcept
			-> std::span<const enrollment_record>;

		/// \brief The record for one pull request, if it is retained.
		[[nodiscard]] auto record_for(const core::pull_request_ref& aRef) const
			-> std::optional<enrollment_record>;

		/// \brief Builds the ordered queue from what is present.
		///
		/// \details Departed items are retained but not projected. They are
		/// kept so the reviewer can be told what happened, and dropped from the
		/// queue because a closed pull request is not asking for attention.
		[[nodiscard]] auto project(
			std::span<const core::queue_activity_observation> aActivity) const
			-> std::vector<core::queue_item>;

	private:
		[[nodiscard]] auto find(const core::pull_request_ref& aRef)
			-> enrollment_record*;

		std::vector<enrollment_record> Records;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_ENROLLMENT_HPP
