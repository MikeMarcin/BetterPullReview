#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REMEMBERED_CONNECTION_STORE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REMEMBERED_CONNECTION_STORE_HPP

#include "bpr/core/remembered_connection.hpp"
#include <expected>
#include <stop_token>

namespace bpr::application {
	enum class remembered_connection_store_failure : std::uint8_t { unavailable,
																	corrupt,
																	newer_schema,
																	conflict,
																	cancelled };
	template <typename T>
	using remembered_connection_store_result = std::expected<T, remembered_connection_store_failure>;
	/// \brief Profile-owned token-free metadata effects; invoke on a serialized effect lane.
	/// \details Loads grant no session authority. Save returns only after full transactional
	/// commit. The host keeps session-only state separately after a refused save and must
	/// report disable-save failures. It drains admitted saves before orderly shutdown.
	class remembered_connection_store_port
	{
	public:
		virtual ~remembered_connection_store_port()                                                                                                                                = default;
		virtual auto load(std::stop_token aStop) -> remembered_connection_store_result<core::remembered_connection_state>                                                          = 0;
		virtual auto save(const core::remembered_connection_state& aCandidate, std::uint64_t aExpectedRevision, std::stop_token aStop) -> remembered_connection_store_result<void> = 0;
	};
}
#endif
