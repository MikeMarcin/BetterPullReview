#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SOURCE_CORPUS_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SOURCE_CORPUS_PORT_HPP

#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/core/source_search.hpp"

#include <functional>

namespace bpr::application {
	/// \brief Exact, immutable catalogue, independent of the Files view and receipts.
	struct source_corpus_entry
	{
		core::file_identity                   File;
		std::optional<git_cache_changed_side> Left;
		std::optional<git_cache_changed_side> Right;
		char                                  Change = 'M';
		[[nodiscard]] auto                    path() const noexcept -> std::string_view { return Right ? Right->Path : Left->Path; }
	};
	struct source_corpus
	{
		core::source_search_scope        Scope;
		std::vector<source_corpus_entry> Entries;
		std::size_t                      OwnedBytes = 0U;
	};
	struct source_corpus_observation
	{
		core::source_search_status           Status = core::source_search_status::unavailable;
		std::shared_ptr<const source_corpus> Corpus;
	};
	struct source_corpus_material
	{
		diff_blob_status                                 Status  = diff_blob_status::failure;
		bool                                             Yielded = false;
		std::shared_ptr<const std::vector<std::uint8_t>> Bytes;
		bool                                             Cached = false;
	};
	/// \brief A query-independent exact read remains needed only by this replacement.
	using source_corpus_read_needed = std::function<bool()>;
	/// \brief Epoch-owned corpus and exact reads; implementations never open a competing cache.
	class source_corpus_port
	{
	public:
		virtual ~source_corpus_port()                                                                                                                                                                                            = default;
		[[nodiscard]] virtual auto catalogue(const core::source_search_scope& aScope, std::stop_token aStop) -> source_corpus_observation                                                                                        = 0;
		[[nodiscard]] virtual auto read(const std::shared_ptr<const source_corpus>& aCorpus, std::size_t aEntry, core::diff_side aSide, std::stop_token aStop, source_corpus_read_needed aStillNeeded) -> source_corpus_material = 0;
		/// \brief Interrupts a background read when demand no longer needs it.
		virtual void demand_changed() = 0;
		/// \brief Stops background reads/slices for selected-source work, which outranks PR search.
		virtual void               yield_to_source_navigation()                                                = 0;
		[[nodiscard]] virtual auto scan_stop_token(const core::source_search_scope& aScope) -> std::stop_token = 0;
	};
}
#endif
