#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MANAGED_CLONE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MANAGED_CLONE_HPP

#include "bpr/core/epoch_model.hpp"
#include "bpr/core/epoch_transition.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::application {
	/// \brief [[BPR-1879]]. The profile folder, beside `repositories`, that
	/// holds the clones BetterPullReview keeps: one folder per repository.
	/// Insights › Storage measures it and profile purge removes it.
	inline constexpr std::string_view managed_clone_folder = "agent-clones-v1";

	/// \brief [[BPR-1879]], RFC-BPR-0092. Why the clone BetterPullReview
	/// keeps for a repository was not made.
	enum class managed_clone_failure : std::uint8_t {
		/// The reviewer pressed Cancel, or the window closed.
		cancelled,
		/// The review on screen cannot make one: it is not the review asked
		/// about, or its sign-in cannot read the repository through `gh`.
		unavailable,
		/// GitHub refused the signed-in account.
		sign_in_refused,
		/// GitHub could not be reached.
		connection_failed,
		/// The repository is larger than a kept clone may be.
		too_large,
		/// The download or the move into the profile's folder failed.
		failed,
	};

	[[nodiscard]] constexpr auto stable_name(const managed_clone_failure aFailure) noexcept -> std::string_view
	{
		switch (aFailure) {
		case managed_clone_failure::cancelled:         return "cancelled";
		case managed_clone_failure::unavailable:       return "unavailable";
		case managed_clone_failure::sign_in_refused:   return "sign_in_refused";
		case managed_clone_failure::connection_failed: return "connection_failed";
		case managed_clone_failure::too_large:         return "too_large";
		case managed_clone_failure::failed:            return "failed";
		}
		return "failed";
	}

	/// \brief [[BPR-1879]]. One kept clone to make, through the review's own
	/// lease, for the repository of the review on screen.
	struct managed_clone_request
	{
		core::application_session_id ApplicationSession;
		core::pull_request_authority Authority;
		/// The review's accepted head: its object format names the review's
		/// repository actor.
		core::git_commit_oid Head;
		/// Absolute; directly inside the profile's folder of kept clones.
		std::filesystem::path Destination;

		auto operator<=>(const managed_clone_request&) const = default;
	};

	[[nodiscard]] inline auto valid(const managed_clone_request& aRequest) noexcept -> bool
	{
		return aRequest.ApplicationSession.value() != 0U && core::valid(aRequest.Authority) && aRequest.Head.valid() && aRequest.Destination.is_absolute()
			&& aRequest.Destination == aRequest.Destination.lexically_normal() && aRequest.Destination.has_filename();
	}

	/// \brief Why a kept clone was not made, with the transport's own words
	/// for the log; never a path or a credential.
	struct managed_clone_error
	{
		managed_clone_failure Failure = managed_clone_failure::failed;
		std::string           Detail;

		auto operator<=>(const managed_clone_error&) const = default;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MANAGED_CLONE_HPP
