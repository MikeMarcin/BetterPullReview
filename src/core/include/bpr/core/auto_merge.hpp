#ifndef BPR_CORE_AUTO_MERGE_HPP
#define BPR_CORE_AUTO_MERGE_HPP

#include <compare>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2329]]. Whether auto-merge may be enabled or disabled here,
/// whether a change may be written, and what the window shows while reads
/// catch up with it.
///
/// \details The owner, 2026-11-20: "It also doesn't look like we can view or
/// control auto-merge." Enable is offered when the repository allows
/// auto-merge, GitHub says the viewer may enable it, the pull request is open
/// and auto-merge is off; Disable when it is on and GitHub says the viewer may
/// disable it. As a draft-state change ([[BPR-1852]]), a change is decided on
/// the gesture from the accepted read and again on what GitHub says just
/// before the write, and GitHub's answer moves the window in place until a
/// read agrees.
namespace bpr::core {
	enum class auto_merge_change : std::uint8_t {
		/// GitHub's `enablePullRequestAutoMerge`.
		enable,
		/// GitHub's `disablePullRequestAutoMerge`.
		disable,
	};

	/// \brief GitHub's `PullRequestMergeMethod`.
	enum class merge_method : std::uint8_t {
		merge,
		squash,
		rebase,
	};

	[[nodiscard]] auto name(auto_merge_change aChange) noexcept -> std::string_view;
	[[nodiscard]] auto name(merge_method aMethod) noexcept -> std::string_view;

	/// \brief The merge methods a repository allows.
	struct allowed_merge_methods
	{
		bool Merge  = false;
		bool Squash = false;
		bool Rebase = false;

		[[nodiscard]] auto allows(merge_method aMethod) const noexcept -> bool;
		/// In GitHub's own order: merge commit, squash, rebase.
		[[nodiscard]] auto list() const -> std::vector<merge_method>;

		auto operator<=>(const allowed_merge_methods&) const = default;
	};

	/// \brief What a read says of auto-merge on a pull request.
	struct auto_merge_facts
	{
		/// The pull request is open: not closed and not merged.
		bool Open = false;
		/// The repository's `autoMergeAllowed`.
		bool Allowed = false;
		/// Auto-merge is on, as the window shows it.
		bool Enabled = false;
		/// GitHub's `viewerCanEnableAutoMerge` and `viewerCanDisableAutoMerge`.
		bool                  ViewerCanEnable  = false;
		bool                  ViewerCanDisable = false;
		allowed_merge_methods Methods;
	};

	/// \brief What the Overview offers: Enable with the methods to choose
	/// from, or Disable, or nothing.
	struct auto_merge_offer
	{
		std::optional<auto_merge_change> Change;
		std::vector<merge_method>        Methods;

		auto operator<=>(const auto_merge_offer&) const = default;
	};

	[[nodiscard]] auto auto_merge_offered(const auto_merge_facts& aFacts) -> auto_merge_offer;

	/// \brief Why a change is refused, on the gesture or before the write.
	enum class auto_merge_refusal : std::uint8_t {
		none,
		/// The repository does not allow auto-merge.
		not_allowed,
		/// The pull request is closed or merged.
		not_open,
		/// Auto-merge is already in the state asked.
		already,
		/// GitHub says this viewer may not change it.
		not_permitted,
		/// Enable asked for a method the repository does not allow, or none.
		method_not_allowed,
	};

	[[nodiscard]] auto name(auto_merge_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Decides a change, in order: not open, not allowed, already,
	/// not permitted, method not allowed. The same order decides a gesture
	/// from the read and a write from GitHub's answer just before it.
	[[nodiscard]] auto decide_auto_merge_change(const auto_merge_facts& aFacts, auto_merge_change aChange, std::optional<merge_method> aMethod) noexcept -> auto_merge_refusal;

	/// \brief [[BPR-2329]]. The viewer's change of auto-merge that reads have
	/// not caught up with, as `draft_state_overlay` holds a draft-state change.
	struct auto_merge_overlay
	{
		std::uint64_t Request        = 0U;
		bool          PendingEnabled = false;
		/// The method a pending or answered enable asked for.
		std::optional<merge_method> Method;
		std::optional<bool>         Answered;
		std::uint8_t                Disagreements = 0U;

		[[nodiscard]] auto pending() const noexcept -> bool { return Request != 0U; }
		[[nodiscard]] auto held() const noexcept -> bool { return pending() || Answered.has_value(); }

		auto operator<=>(const auto_merge_overlay&) const = default;
	};

	void begin_auto_merge_change(auto_merge_overlay& aOverlay, std::uint64_t aRequest, auto_merge_change aChange, std::optional<merge_method> aMethod) noexcept;
	/// \brief GitHub answered `aRequest`: auto-merge is now `aEnabled`.
	void settle_auto_merge_change(auto_merge_overlay& aOverlay, std::uint64_t aRequest, bool aEnabled) noexcept;
	void abandon_auto_merge_change(auto_merge_overlay& aOverlay, std::uint64_t aRequest) noexcept;
	/// \brief A new read says auto-merge is `aEnabled`; an answer it agrees
	/// with is done, one it disagrees with twice is given up.
	void observe_auto_merge(auto_merge_overlay& aOverlay, bool aEnabled) noexcept;

	struct projected_auto_merge
	{
		bool Enabled = false;
		bool Pending = false;
		/// The overlay's method when it holds an enable; absent otherwise.
		std::optional<merge_method> Method;

		auto operator<=>(const projected_auto_merge&) const = default;
	};

	[[nodiscard]] auto project_auto_merge(bool aReadEnabled, const auto_merge_overlay& aOverlay) noexcept -> projected_auto_merge;
} // namespace bpr::core

#endif
