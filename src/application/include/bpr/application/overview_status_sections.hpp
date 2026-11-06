#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OVERVIEW_STATUS_SECTIONS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OVERVIEW_STATUS_SECTIONS_HPP

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

namespace bpr::application {
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

	/// \brief How GitHub's merge assessment reads at a glance.
	enum class overview_merge_tone : std::uint8_t {
		/// Not observed, still being computed, or a draft.
		unknown,
		blocked,
		/// Mergeable by GitHub's account, with something still not passing.
		caution,
		mergeable,
	};

	/// \brief [[BPR-1405]]. One outstanding review request, as one roster row.
	struct overview_review_request
	{
		/// `login` or `organization/slug`; empty when the provider hid the actor.
		std::string Name;
		/// A team's readable name, when the provider supplied one.
		std::string DisplayName;
		bool        Team      = false;
		bool        CodeOwner = false;
		/// [[BPR-1849]]. The viewer just asked for it and GitHub has not
		/// answered; the row is the core's projection, not the read's.
		bool Pending = false;

		auto operator<=>(const overview_review_request&) const = default;
	};

	/// Read-only people, independent of merge rules and local progress.
	struct overview_people_projection
	{
		std::vector<overview_participant>    Participants;
		std::vector<overview_review_request> ReviewRequests;
		std::string                          ReviewRequestsNote;
		auto                                 operator<=>(const overview_people_projection&) const = default;
	};

	/// GitHub's assessment, before its independent rule reads complete.
	struct overview_merge_projection
	{
		std::string         State;
		overview_merge_tone Tone = overview_merge_tone::unknown;
		std::string         Details;
		auto                operator<=>(const overview_merge_projection&) const = default;
	};
}
#endif
