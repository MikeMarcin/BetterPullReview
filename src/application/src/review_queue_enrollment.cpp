#include "bpr/application/review_queue_enrollment.hpp"

#include <algorithm>

namespace bpr::application {
	auto name(const enrollment_departure aDeparture) noexcept -> std::string_view
	{
		switch (aDeparture) {
		case enrollment_departure::none:
			return "present";
		case enrollment_departure::closed:
			return "closed";
		case enrollment_departure::access_lost:
			return "access lost";
		case enrollment_departure::gone:
			return "gone";
		case enrollment_departure::removed_by_reviewer:
			return "removed";
		}
		return "present";
	}

	auto explanation(const enrollment_departure aDeparture) noexcept
		-> std::string_view
	{
		switch (aDeparture) {
		case enrollment_departure::none:
			return "This is in your queue.";
		case enrollment_departure::closed:
			return "This pull request was closed or merged.";
		case enrollment_departure::access_lost:
			return "You can no longer read this pull request.";
		case enrollment_departure::gone:
			return "This pull request no longer exists where it was.";
		case enrollment_departure::removed_by_reviewer:
			return "You removed this from your queue.";
		}
		return "This is in your queue.";
	}

	auto queue_enrollment_register::find(const core::pull_request_ref& aRef)
		-> enrollment_record*
	{
		const auto found
			= std::ranges::find(Records, aRef, &enrollment_record::Ref);
		return found == Records.end() ? nullptr : &*found;
	}

	void queue_enrollment_register::enroll(
		const core::pull_request_ref& aRef,
		const core::queue_membership& aMembership)
	{
		if (auto* const existing = find(aRef); existing != nullptr) {
			// Both reasons are kept. A rule match must not overwrite a role, or
			// removing the rule would silently drop a pull request GitHub
			// assigned; a role must not overwrite the rule, or the reviewer
			// loses the answer to why an unfamiliar item is in their queue.
			if (aMembership.Role != core::github_role::none) {
				existing->Membership.Role = aMembership.Role;
			}
			if (aMembership.MatchedRule && !existing->Membership.MatchedRule) {
				existing->Membership.MatchedRule = true;
				existing->Membership.Rule        = aMembership.Rule;
			}
			existing->Departed = enrollment_departure::none;
			return;
		}
		Records.push_back(enrollment_record{
			.Ref        = aRef,
			.Membership = aMembership,
			.Departed   = enrollment_departure::none,
		});
	}

	void queue_enrollment_register::depart(
		const core::pull_request_ref& aRef,
		const enrollment_departure    aDeparture)
	{
		if (auto* const existing = find(aRef); existing != nullptr) {
			existing->Departed = aDeparture;
		}
	}

	void queue_enrollment_register::observe(
		const queue_refresh_completion& aCompletion,
		const refresh_disposition       aDisposition)
	{
		if (aDisposition != refresh_disposition::withdrawn) {
			return;
		}
		// Forbidden and gone are different facts with different next steps: one
		// is a permission the reviewer may be able to ask for, the other is a
		// pull request that is not there any more. Collapsing them would send
		// somebody to the wrong person.
		depart(
			aCompletion.Ref,
			aCompletion.Status == queue_refresh_status::forbidden
				? enrollment_departure::access_lost
				: enrollment_departure::gone);
	}

	auto queue_enrollment_register::records() const noexcept
		-> std::span<const enrollment_record>
	{
		return Records;
	}

	auto queue_enrollment_register::record_for(
		const core::pull_request_ref& aRef) const
		-> std::optional<enrollment_record>
	{
		const auto found
			= std::ranges::find(Records, aRef, &enrollment_record::Ref);
		if (found == Records.end()) {
			return std::nullopt;
		}
		return *found;
	}

	auto queue_enrollment_register::project(
		const std::span<const core::queue_activity_observation> aActivity) const
		-> std::vector<core::queue_item>
	{
		auto items = std::vector<core::queue_item>{ };
		items.reserve(Records.size());
		for (auto index = std::size_t{ 0U }; index < Records.size(); ++index) {
			const auto& record = Records[index];
			if (!record.present()) {
				continue;
			}
			// A missing observation is not an error and must not be guessed at.
			// The host supplies what it read; an item it has not read yet is
			// quiet, which is what `bucket_for` says about an empty one.
			const auto activity = index < aActivity.size()
				? aActivity[index]
				: core::queue_activity_observation{ };
			items.push_back(core::queue_item{
				.Ref        = record.Ref,
				.Membership = record.Membership,
				.Bucket     = core::bucket_for(record.Membership, activity),
			});
		}
		return core::ordered_queue(items);
	}
} // namespace bpr::application
