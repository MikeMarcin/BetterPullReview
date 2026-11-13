#include "local_draft_search.hpp"

#include "bpr/core/git_objects.hpp"
#include "bpr/core/local_presentation.hpp"

#include <algorithm>
#include <utility>

namespace bpr::app {
	auto local_candidate_subject(const core::local_draft& aDraft, const std::uint64_t aCandidate) -> std::string
	{
		return aDraft.Id.Value + "#candidate-" + std::to_string(aCandidate);
	}

	auto local_search_scope(const core::local_draft& aDraft, const core::local_draft_candidate& aCandidate) -> core::source_search_scope
	{
		const auto identity = core::local_presentation_identity_of(local_candidate_subject(aDraft, aCandidate.Ordinal));
		auto       scope    = core::source_search_scope{ };
		scope.Session       = core::application_session_id{ 1U };
		scope.Authority     = identity.Authority;
		scope.Comparison    = {
			   .Left  = { .Identity = identity.Left, .Commit = aCandidate.Base, .Availability = core::anchor_availability::available },
			   .Right = { .Identity = identity.Right, .Commit = aCandidate.Head, .Availability = core::anchor_availability::available },
		};
		scope.CatalogueGeneration = aCandidate.Ordinal;
		scope.Tab                 = local_search_tab;
		scope.Target              = core::source_search_target::files;
		return scope;
	}

	local_draft_corpus::local_draft_corpus(store_factory aStore)
		: Store{ std::move(aStore) }
	{
	}

	void local_draft_corpus::set_candidate(const core::source_search_scope& aScope, std::string aSubject, std::vector<core::draft_path_change> aChanges)
	{
		const auto guard = std::lock_guard{ Mutex };
		if (Scope == aScope) {
			return;
		}
		Scope   = aScope;
		Subject = std::move(aSubject);
		Changes = std::move(aChanges);
		Bytes.clear();
	}

	auto local_draft_corpus::catalogue(const core::source_search_scope& aScope, const std::stop_token aStop) -> application::source_corpus_observation
	{
		auto changes = std::vector<core::draft_path_change>{ };
		auto subject = std::string{ };
		{
			const auto guard = std::lock_guard{ Mutex };
			if (Scope != aScope) {
				return { .Status = core::source_search_status::unavailable };
			}
			changes = Changes;
			subject = Subject;
		}
		auto store = Store ? Store() : nullptr;
		if (store == nullptr) {
			return { .Status = core::source_search_status::unavailable };
		}
		auto corpus     = std::make_shared<application::source_corpus>();
		corpus->Scope   = aScope;
		auto read = std::map<core::git_blob_oid, std::shared_ptr<const std::vector<std::uint8_t>>>{ };
		const auto side = [&](const std::optional<core::draft_side>& aSide, const std::string& aPath) -> std::optional<application::git_cache_changed_side> {
			if (!aSide || aStop.stop_requested()) {
				return std::nullopt;
			}
			auto bytes = store->object(aSide->Object);
			if (!bytes || !*bytes) {
				return std::nullopt;
			}
			auto blob = core::git_blob_identity(std::span<const std::uint8_t>{ **bytes }, core::git_hash_algorithm::sha1);
			if (!blob) {
				return std::nullopt;
			}
			corpus->OwnedBytes += (*bytes)->size();
			read.emplace(*blob, std::make_shared<const std::vector<std::uint8_t>>(std::move(**bytes)));
			return application::git_cache_changed_side{ .Path = aPath, .Object = *blob, .Mode = aSide->Mode };
		};
		for (const auto& change : changes) {
			auto entry  = application::source_corpus_entry{ };
			entry.Left  = side(change.Left, change.Path);
			entry.Right = side(change.Right, change.Path);
			if (!entry.Left && !entry.Right) {
				continue;
			}
			entry.Change = change.Change == core::draft_change::added ? 'A' : change.Change == core::draft_change::deleted ? 'D' : 'M';
			// The identity the comparison of that path carries, so a result reveals in it.
			entry.File = core::local_presentation_file(subject, change.Path);
			corpus->Entries.push_back(std::move(entry));
		}
		if (aStop.stop_requested()) {
			return { .Status = core::source_search_status::cancelled };
		}
		{
			const auto guard = std::lock_guard{ Mutex };
			Bytes            = std::move(read);
		}
		return { .Status = core::source_search_status::complete, .Corpus = std::move(corpus) };
	}

	auto local_draft_corpus::read(const std::shared_ptr<const application::source_corpus>& aCorpus, const std::size_t aEntry, const core::diff_side aSide, std::stop_token /*aStop*/,
								  application::source_corpus_read_needed /*aStillNeeded*/) -> application::source_corpus_material
	{
		if (!aCorpus || aEntry >= aCorpus->Entries.size()) {
			return { .Status = application::diff_blob_status::failure };
		}
		const auto& entry = aCorpus->Entries[aEntry];
		const auto& side  = aSide == core::diff_side::left ? entry.Left : entry.Right;
		if (!side) {
			return { .Status = application::diff_blob_status::missing };
		}
		const auto guard = std::lock_guard{ Mutex };
		const auto found = Bytes.find(side->Object);
		if (found == Bytes.end()) {
			return { .Status = application::diff_blob_status::missing };
		}
		return { .Status = application::diff_blob_status::available, .Bytes = found->second, .Cached = true };
	}

	auto local_draft_corpus::scan_stop_token(const core::source_search_scope& /*aScope*/) -> std::stop_token
	{
		return { };
	}
}
