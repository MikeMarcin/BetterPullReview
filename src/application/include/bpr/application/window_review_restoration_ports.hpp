#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WINDOW_REVIEW_RESTORATION_PORTS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WINDOW_REVIEW_RESTORATION_PORTS_HPP
#include "bpr/core/window_review_restoration.hpp"
#include <stop_token>

namespace bpr::application {
	struct window_review_snapshot
	{
		std::uint64_t                           Revision = 1;
		std::vector<core::window_review_record> Records;
		/// [[BPR-0733]]. Most recent first; bounded by core::maximum_recent_reviews.
		std::vector<core::window_recent_review> Recent;
		auto                                    operator==(const window_review_snapshot&) const -> bool = default;
	};
	/// \brief [[BPR-0733]]. One row of a window's recently viewed list, already
	/// filtered by the composition for the session the window has.
	struct recent_review_entry
	{
		core::window_review_destination Destination;
		std::string                     Title;
		std::int64_t                    ViewedAt  = 0;
		bool                            Resumable = false;
		/// [[BPR-1365]]. The review this window has open, behind Home or in front.
		bool                Open                                                 = false;
		std::optional<bool> Draft                                                = std::nullopt;
		auto                operator==(const recent_review_entry&) const -> bool = default;
	};
	struct recent_reviews_projection
	{
		std::vector<recent_review_entry> Entries;
		auto                             operator==(const recent_reviews_projection&) const -> bool = default;
	};
	/// \brief A window's gesture on its recently viewed list: open the entry on
	/// Overview, or resume it at its saved reading.
	struct recent_review_intent
	{
		std::size_t Index  = 0;
		bool        Resume = false;
	};
	enum class window_review_store_failure : std::uint8_t { unavailable,
															cancelled,
															schema_mismatch,
															corrupt,
															conflict };
	struct window_review_store_error
	{
		window_review_store_failure           Failure;
		std::optional<window_review_snapshot> Current;
	};
	template <typename T>
	using window_review_store_result = std::expected<T, window_review_store_error>;
	[[nodiscard]] auto valid(const window_review_snapshot& aSnapshot) noexcept -> bool;
	/// Profile-private bounded storage. Save must atomically compare the expected revision.
	class window_review_restoration_port
	{
	public:
		virtual ~window_review_restoration_port()                                                                                                                                               = default;
		[[nodiscard]] virtual auto load(std::stop_token aStop) -> window_review_store_result<window_review_snapshot>                                                                            = 0;
		[[nodiscard]] virtual auto save(const window_review_snapshot& aCandidate, std::uint64_t aExpectedRevision, std::stop_token aStop) -> window_review_store_result<window_review_snapshot> = 0;
	};
	class window_review_restoration_projection_port
	{
	public:
		virtual ~window_review_restoration_projection_port()                             = default;
		virtual void select_startup(const core::window_review_startup_request& aRequest) = 0;
		virtual void open_review(const core::window_review_open_request& aRequest)       = 0;
		virtual void fail(const core::window_review_error& aError)                       = 0;
		/// [[BPR-0733]]. The recently viewed list changed (a load or a view).
		virtual void recent_changed() { }
	};
}
#endif
