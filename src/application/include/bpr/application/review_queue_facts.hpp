#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_FACTS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_QUEUE_FACTS_HPP
#include "bpr/application/github_read_model.hpp"
#include "bpr/core/review_queue_attention.hpp"
namespace bpr::application {
	/// \brief Projects bounded provider observations to inert Home metadata.
	[[nodiscard]] auto queue_waiting_for(const github_pull_request_snapshot& aSnapshot) -> core::queue_waiting_facts;
	/// \brief Merges only observed fields, preserving last-known facts on unavailable reads.
	void merge_queue_waiting(core::queue_waiting_facts& aRetained, const core::queue_waiting_facts& aRead);
	/// \brief Versioned bounded private-store representation; malformed input is refused.
	[[nodiscard]] auto encode_queue_facts(const core::queue_item_facts& aFacts) -> std::optional<std::string>;
	[[nodiscard]] auto decode_queue_facts(std::string_view aBytes) -> std::optional<core::queue_item_facts>;
}
#endif
