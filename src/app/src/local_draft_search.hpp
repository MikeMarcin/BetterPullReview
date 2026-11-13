#ifndef BPR_SRC_APP_SRC_LOCAL_DRAFT_SEARCH_HPP
#define BPR_SRC_APP_SRC_LOCAL_DRAFT_SEARCH_HPP

#include "bpr/application/local_draft_store.hpp"
#include "bpr/application/source_corpus_port.hpp"
#include "bpr/core/local_draft.hpp"
#include "bpr/core/source_search.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief Find in files over a local draft's shown candidate (RFC-BPR-0108,
/// owner 2026-11-13: the draft has the pull request window's own Search tab).
/// The same search host, panel and reveal as a pull request's; only the
/// corpus differs: the candidate's two sides, read from the draft's own store.
namespace bpr::app {
	/// \brief The subject a candidate's comparisons are identified by, shared
	/// with the presentations the window's diff viewer shows.
	[[nodiscard]] auto local_candidate_subject(const core::local_draft& aDraft, std::uint64_t aCandidate) -> std::string;

	/// \brief The search scope of a draft's candidate: its comparisons'
	/// authority and anchors, the candidate's base and head.
	[[nodiscard]] auto local_search_scope(const core::local_draft& aDraft, const core::local_draft_candidate& aCandidate) -> core::source_search_scope;

	/// \brief The one tab a local draft's search results open in.
	inline constexpr auto local_search_tab = core::document_tab_id{ 1U };

	/// \brief The candidate's sides as a search corpus.
	/// \details The changes are set on the decision lane; the catalogue and the
	/// reads run on the search lane, each through a store of their own.
	class local_draft_corpus final : public application::source_corpus_port
	{
	public:
		using store_factory = std::function<std::unique_ptr<application::local_draft_store>()>;
		explicit local_draft_corpus(store_factory aStore);

		/// \brief The candidate a scope's catalogue lists.
		void set_candidate(const core::source_search_scope& aScope, std::string aSubject, std::vector<core::draft_path_change> aChanges);

		[[nodiscard]] auto catalogue(const core::source_search_scope& aScope, std::stop_token aStop) -> application::source_corpus_observation override;
		[[nodiscard]] auto read(const std::shared_ptr<const application::source_corpus>& aCorpus, std::size_t aEntry, core::diff_side aSide, std::stop_token aStop,
								application::source_corpus_read_needed aStillNeeded) -> application::source_corpus_material override;
		void               demand_changed() override { }
		void               yield_to_source_navigation() override { }
		[[nodiscard]] auto scan_stop_token(const core::source_search_scope& aScope) -> std::stop_token override;

	private:
		store_factory                                                        Store;
		std::mutex                                                           Mutex;
		std::optional<core::source_search_scope>                             Scope;
		std::string                                                          Subject;
		std::vector<core::draft_path_change>                                 Changes;
		std::map<core::git_blob_oid, std::shared_ptr<const std::vector<std::uint8_t>>> Bytes;
	};
}

#endif
