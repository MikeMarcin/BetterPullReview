#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WINDOW_REVIEW_RESTORATION_PORTS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WINDOW_REVIEW_RESTORATION_PORTS_HPP
#include "bpr/core/window_review_restoration.hpp"
#include <stop_token>

namespace bpr::application {
	struct window_review_snapshot
	{
		std::uint64_t                           Revision = 1;
		std::vector<core::window_review_record> Records;
		auto                                    operator==(const window_review_snapshot&) const -> bool = default;
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
	};
}
#endif
