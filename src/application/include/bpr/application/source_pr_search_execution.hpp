#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SOURCE_PR_SEARCH_EXECUTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SOURCE_PR_SEARCH_EXECUTION_HPP
#include "bpr/application/source_search_host.hpp"

namespace bpr::application {
	inline constexpr std::size_t maximum_source_pr_search_owned_bytes = 192U * 1024U * 1024U;
	/// \brief Compiles once and streams exact full-file documents through the epoch port.
	/// \details A 192 MiB conservative admission envelope covers matcher, two
	/// documents, existing 16 MiB actor cache, catalogue, records and coalesced
	/// publication copies. No whole corpus, syntax, diff or independent cache is built.
	class source_pr_search_execution final : public source_search_execution_port
	{
	public:
		explicit source_pr_search_execution(source_corpus_port& aCorpus, core::source_match_limits aLimits = { });
		~source_pr_search_execution() override;
		[[nodiscard]] auto execute(const source_search_work& aWork, std::stop_token aStop) -> source_search_execution override;
		[[nodiscard]] auto execute(const source_search_work& aWork, std::stop_token aStop, const source_search_batch_sink& aPublish) -> source_search_execution override;
		void               update_demand(std::optional<source_search_work> aWork) override;
		[[nodiscard]] auto statistics() const -> source_search_statistics;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}
#endif
