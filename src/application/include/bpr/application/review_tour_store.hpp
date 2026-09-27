#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_STORE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_STORE_HPP
#include "bpr/application/review_tour_projection.hpp"
#include <algorithm>
#include <deque>
#include <expected>
#include <stop_token>
namespace bpr::application {
	/// \brief Local lookup metadata captured from an accepted provider read, never a write grant.
	struct tour_saved_target
	{
		core::tour_owner          Owner;
		core::tour_draft_id       Draft;
		core::host_profile_id     HostProfile;
		std::string               Hostname;
		core::repository_locator  Repository;
		core::pull_request_number Number;
		core::git_commit_oid      Base, Head;
		auto                      operator==(const tour_saved_target&) const -> bool = default;
	};
	inline constexpr std::size_t maximum_tour_history_entries = 16;
	inline constexpr std::size_t maximum_tour_history_bytes   = 8U * 1024U * 1024U;
	struct tour_history_entry
	{
		core::review_tour  Draft;
		core::tour_stop_id Selected;
		std::string        Label;
		auto               operator==(const tour_history_entry&) const -> bool = default;
	};
	/// One compare-and-swap record binds current content and the shared local journal.
	struct tour_stored_state
	{
		core::review_tour              Draft;
		std::deque<tour_history_entry> Undo, Redo;
		auto                           operator==(const tour_stored_state&) const -> bool = default;
	};
	[[nodiscard]] inline auto valid(const tour_stored_state& aState) -> bool
	{
		if (core::validate(aState.Draft) != core::tour_refusal::none) {
			return false;
		}
		for (const auto* history : { &aState.Undo, &aState.Redo }) {
			std::size_t bytes = 0;
			if (history->size() > maximum_tour_history_entries) {
				return false;
			}
			for (const auto& item : *history) {
				bytes += core::retained_tour_bytes(item.Draft);
				if (bytes > maximum_tour_history_bytes || item.Label.empty() || item.Label.size() > 128 || item.Label.find('\0') != std::string::npos || core::validate(item.Draft) != core::tour_refusal::none || item.Draft.Owner != aState.Draft.Owner || item.Draft.Id != aState.Draft.Id || item.Draft.Revision >= aState.Draft.Revision || (item.Selected.value() && std::ranges::find(item.Draft.Stops, item.Selected, &core::tour_stop::Id) == item.Draft.Stops.end())) {
					return false;
				}
			}
		}
		return true;
	}

	struct tour_store_error
	{
		tour_store_failure               Failure = tour_store_failure::unavailable;
		std::optional<core::review_tour> Current;
	};
	template <typename T>
	using tour_store_result = std::expected<T, tour_store_error>;
	/// \brief Admitted profile-private storage. All calls run on a serialized effect lane.
	class review_tour_store
	{
	public:
		virtual ~review_tour_store()                                                                                                                                = default;
		virtual auto load(const core::tour_owner& aOwner, core::tour_draft_id aDraft, std::stop_token aStop) -> tour_store_result<std::optional<core::review_tour>> = 0;
		/// Expected zero creates an absent draft; otherwise exact CAS is mandatory.
		virtual auto save(const core::review_tour& aDraft, std::uint64_t aExpectedRevision, std::stop_token aStop) -> tour_store_result<core::review_tour> = 0;
		/// Compatibility ports may omit durable history; the production store commits it atomically.
		virtual auto load_state(const core::tour_owner& aOwner, core::tour_draft_id aDraft, std::stop_token aStop) -> tour_store_result<std::optional<tour_stored_state>>
		{
			auto value = load(aOwner, aDraft, aStop);
			if (!value) {
				return std::unexpected(value.error());
			}
			if (!*value) {
				return std::nullopt;
			}
			return tour_stored_state{ **value, { }, { } };
		}
		virtual auto save_state(const tour_stored_state& aState, std::uint64_t aExpectedRevision, std::stop_token aStop) -> tour_store_result<core::review_tour> { return save(aState.Draft, aExpectedRevision, aStop); }
		/// \brief Bounded exact target discovery under the same admitted profile owner.
		virtual auto targets(std::stop_token) -> tour_store_result<std::vector<tour_saved_target>> { return std::unexpected(tour_store_error{ }); }
		virtual auto remember_target(const tour_saved_target&, std::stop_token) -> tour_store_result<void> { return std::unexpected(tour_store_error{ }); }
	};
}
#endif
