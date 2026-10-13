#include "../support/source_search_fixture.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/source_pr_search_execution.hpp"

#include <algorithm>
#include <map>

namespace {
	using namespace bpr;
	using namespace bpr::application;
	using namespace bpr::tests;

	struct corpus_fixture final : source_corpus_port
	{
		std::shared_ptr<source_corpus>                       Corpus = std::make_shared<source_corpus>();
		std::map<core::git_blob_oid, std::string>            Contents;
		std::vector<std::pair<std::size_t, core::diff_side>> Reads;
		std::function<void()>                                OnRead;
		std::size_t                                          Catalogues = 0U;
		std::stop_source                                     Priority;
		corpus_fixture() { Corpus->Scope = search_scope(core::source_search_target::files); }
		auto catalogue(const core::source_search_scope& aScope, std::stop_token) -> source_corpus_observation override
		{
			++Catalogues;
			return aScope == Corpus->Scope ? source_corpus_observation{ core::source_search_status::complete, Corpus } : source_corpus_observation{ };
		}
		auto read(const std::shared_ptr<const source_corpus>& aCorpus, std::size_t aEntry, core::diff_side aSide, std::stop_token aStop, source_corpus_read_needed) -> source_corpus_material override
		{
			search_expect(aCorpus == Corpus, "worker only reads exact catalogue");
			Reads.emplace_back(aEntry, aSide);
			if (OnRead) {
				OnRead();
			}
			if (aStop.stop_requested()) {
				return { diff_blob_status::cancelled, false, { } };
			}
			const auto& side = aSide == core::diff_side::left ? Corpus->Entries[aEntry].Left : Corpus->Entries[aEntry].Right;
			if (side->Mode != 0100644U && side->Mode != 0100755U) {
				return { diff_blob_status::generated_file_unsupported, false, { } };
			}
			const auto found = Contents.find(side->Object);
			if (found == Contents.end()) {
				return { diff_blob_status::missing, false, { } };
			}
			return { diff_blob_status::available, false, std::make_shared<const std::vector<std::uint8_t>>(found->second.begin(), found->second.end()) };
		}
		void demand_changed() override { }
		void yield_to_source_navigation() override
		{
			Priority.request_stop();
			Priority = std::stop_source{ };
		}
		auto scan_stop_token(const core::source_search_scope&) -> std::stop_token override { return Priority.get_token(); }
		auto add_side(std::string aPath, std::string aContent, std::uint8_t aSeed, std::uint32_t aMode = 0100644U) -> git_cache_changed_side
		{
			const auto blob = *core::parse_git_blob_oid(std::string(40U, static_cast<char>('a' + aSeed)));
			Contents[blob]  = std::move(aContent);
			return { std::move(aPath), blob, aMode };
		}
		void add(std::optional<git_cache_changed_side> aLeft, std::optional<git_cache_changed_side> aRight, char aChange = 'M')
		{
			auto entry   = source_corpus_entry{ };
			entry.Left   = std::move(aLeft);
			entry.Right  = std::move(aRight);
			entry.Change = aChange;
			entry.File   = core::file_identity_of(entry.path());
			Corpus->Entries.push_back(std::move(entry));
		}
		auto work(std::string aQuery = "needle") -> source_search_work
		{
			auto output               = source_search_work{ };
			output.Request.Scope      = Corpus->Scope;
			output.Request.Generation = 1U;
			output.Query              = std::move(aQuery);
			return output;
		}
	};
	struct projections final : source_search_projection_port
	{
		source_search_projection Last;
		std::size_t              SearchingUpdates = 0U;
		void                     publish(const source_search_projection& aProjection) override
		{
			Last = aProjection;
			if (Last.Status == core::source_search_status::searching && Last.Summary.Matches != 0U) {
				++SearchingUpdates;
			}
		}
	};
	void full_documents_and_side_qualification()
	{
		corpus_fixture fixture;
		const auto     one = fixture.add_side("a-added", "unchanged\nneedle\n", 0U);
		const auto     two = fixture.add_side("b-removed", "needle\n", 1U);
		fixture.add({ }, one, 'A');
		fixture.add(two, { }, 'D');
		auto modifiedLeft  = one;
		modifiedLeft.Path  = "c-modified";
		auto modifiedRight = two;
		modifiedRight.Path = "c-modified";
		fixture.add(modifiedLeft, modifiedRight);
		auto renamedLeft  = one;
		renamedLeft.Path  = "old-name";
		auto renamedRight = one;
		renamedRight.Path = "d-renamed";
		fixture.add(renamedLeft, renamedRight, 'R');
		auto copy = one;
		copy.Path = "e-copied";
		fixture.add(one, copy, 'C');
		const auto binary    = fixture.add_side("f-binary", std::string("\0needle", 7U), 2U);
		auto       textRight = one;
		textRight.Path       = "f-binary";
		fixture.add(binary, textRight);
		const auto link = fixture.add_side("g-link", "needle", 3U, 0120000U);
		fixture.add({ }, link, 'A');
		const auto invalid = fixture.add_side("h-invalid", std::string("\xffneedle", 7U), 4U);
		fixture.add({ }, invalid, 'A');
		auto missing   = one;
		missing.Path   = "i-missing";
		missing.Object = *core::parse_git_blob_oid(std::string(40U, 'f'));
		fixture.add({ }, missing, 'A');
		source_pr_search_execution execution{ fixture };
		const auto                 work = fixture.work();
		execution.update_demand(work);
		auto       batches = std::vector<source_search_batch>{ };
		const auto result  = execution.execute(work, { }, [&](auto aBatch) { batches.push_back(std::move(aBatch)); });
		search_expect(result.Terminal.Status == core::source_search_status::partial && result.Terminal.Summary.EligibleFiles == 9U && result.Terminal.Summary.SearchedFiles == 6U && result.Terminal.Summary.Matches == 7U && result.Terminal.Summary.SkippedSides == 4U, "all entries full both-side documents and precise partial coverage");
		search_expect(result.Skipped && result.Skipped->size() == 4U && result.RetryAvailable, "skipped reasons and exact-authority retry retained");
		search_expect(result.Matches->at(2U).Match.Locations[0] && !result.Matches->at(2U).Match.Locations[1] && result.Matches->at(3U).Match.Locations[1], "different blobs remain left-before-right records even with identical previews");
		search_expect(result.Matches->at(4U).Match.Locations[0] && result.Matches->at(4U).Match.Locations[1], "same-entry identical rename blob scanned once with both exact locations");
		search_expect(fixture.Reads.size() == 11U, "identical rename and copy reads coalesce, different entries retain their own occurrence");
		search_expect(std::ranges::all_of(batches, [](const auto& aBatch) { return aBatch.Matches.size() <= 128U; }) && result.PeakOwnedBound <= maximum_source_pr_search_owned_bytes, "bounded ordered publications and aggregate admission envelope");
	}
	void filters_and_invalid_patterns_before_material()
	{
		corpus_fixture fixture;
		auto           left  = fixture.add_side("old/name.cpp", "needle", 0U);
		auto           right = left;
		right.Path           = "new/name.cpp";
		fixture.add(left, right, 'R');
		source_pr_search_execution execution{ fixture };
		auto                       work = fixture.work();
		work.Include                    = "old/**";
		execution.update_demand(work);
		auto result = execution.execute(work, { });
		search_expect(result.Terminal.Summary.Matches == 1U && result.Matches->at(0U).Match.Locations[1], "either rename path includes full entry");
		fixture.Reads.clear();
		work.Exclude = "old/**";
		execution.update_demand(work);
		result = execution.execute(work, { });
		search_expect(result.Terminal.Status == core::source_search_status::complete && result.Terminal.Summary.EligibleFiles == 0U && fixture.Reads.empty(), "exclude wins before acquisition");
		const auto catalogues = fixture.Catalogues;
		work.Include          = "{";
		result                = execution.execute(work, { });
		search_expect(result.Terminal.Status == core::source_search_status::invalid_pattern && fixture.Catalogues == catalogues, "invalid filter refused before catalogue or blobs");
		work.Include.clear();
		work.Exclude.clear();
		work.Query                             = "(?=needle)";
		work.Request.Options.RegularExpression = true;
		result                                 = execution.execute(work, { });
		search_expect(result.Terminal.Status == core::source_search_status::unsupported_pattern && fixture.Catalogues == catalogues, "unsupported regex refused before reads");
		work.Query.clear();
		result = execution.execute(work, { });
		search_expect(fixture.Catalogues == catalogues, "empty search reads no source");
	}
	void cap_batches_public_host_and_reordered_progress()
	{
		for (const auto occurrences : { 10000U, 10001U }) {
			corpus_fixture fixture;
			fixture.add({ }, fixture.add_side("a", std::string(occurrences, 'x'), 0U), 'A');
			source_pr_search_execution execution{ fixture };
			manual_executor            effects;
			manual_executor            decisions;
			projections                projection;
			source_search_host         host{ decisions, effects, execution, projection };
			search_expect(host.set_scope(fixture.Corpus->Scope) && host.set_query("x"), "large exact query admitted");
			effects.drain();
			search_expect(decisions.pending() == 2U, "eighty bounded batches coalesce into one progress closure plus terminal");
			decisions.run_next();
			search_expect(host.state().Status == core::source_search_status::searching && projection.Last.Matches->size() == 10000U && !projection.Last.NoResults, "usable current prefix before terminal");
			static_cast<void>(host.navigate(core::change_navigation_direction::next));
			decisions.drain();
			search_expect(host.state().Status == core::source_search_status::result_limit && host.state().Summary.AdditionalMatchObserved == (occurrences == 10001U) && host.state().CurrentMatch == 1U, "truthful cap and current prefix cursor preserved by terminal");
			const auto reads = fixture.Reads.size();
			for (auto i = 0U; i < 1000U; ++i) {
				static_cast<void>(host.navigate(core::change_navigation_direction::next));
			}
			search_expect(fixture.Reads.size() == reads, "cap traversal does no acquisition");
			static_cast<void>(host.retry());
			effects.drain();
			decisions.run(1U);
			search_expect(!host.is_quiescent(), "late progress closure is accounted after terminal reordered first");
			decisions.drain();
			search_expect(host.is_quiescent() && host.state().Status == core::source_search_status::result_limit, "late progress cannot restore searching or obsolete counts");
			host.shutdown();
		}
	}
	void retry_uses_same_scope_and_keeps_partial_hits()
	{
		corpus_fixture fixture;
		fixture.add({ }, fixture.add_side("a", "needle", 0U), 'A');
		auto unavailable = fixture.add_side("b", "needle", 1U);
		fixture.Contents.erase(unavailable.Object);
		fixture.add({ }, unavailable, 'A');
		source_pr_search_execution execution{ fixture };
		manual_executor            effects;
		manual_executor            decisions;
		projections                projection;
		source_search_host         host{ decisions, effects, execution, projection };
		static_cast<void>(host.set_scope(fixture.Corpus->Scope));
		static_cast<void>(host.set_query("needle"));
		effects.drain();
		decisions.drain();
		search_expect(host.state().Status == core::source_search_status::partial && projection.Last.RetryAvailable && projection.Last.Matches->size() == 1U && !projection.Last.NoResults, "transport-unavailable side keeps usable partial prefix and retry qualification");
		const auto scope                     = *host.state().Scope;
		fixture.Contents[unavailable.Object] = "needle";
		static_cast<void>(host.retry());
		effects.drain();
		decisions.drain();
		search_expect(host.state().Status == core::source_search_status::complete && host.state().Scope == scope && projection.Last.Corpus->Scope == scope && projection.Last.Matches->size() == 2U, "retry restores exact original authority and comparison, never a newer head");
		static_cast<void>(host.set_paths(std::string(4097U, 'x'), { }));
		search_expect(host.state().Input == core::source_search_input::input_limit && !projection.Last.Matches && effects.pending() == 0U, "oversized filter clears current hits before copying or source work");
		host.shutdown();
	}
	void cancellation_and_resource_prefix()
	{
		corpus_fixture fixture;
		fixture.add({ }, fixture.add_side("a", "needle", 0U), 'A');
		source_pr_search_execution execution{ fixture };
		auto                       work = fixture.work();
		execution.update_demand(work);
		std::stop_source stop;
		fixture.OnRead     = [&] { stop.request_stop(); };
		const auto stopped = execution.execute(work, stop.get_token());
		search_expect(stopped.Terminal.Status == core::source_search_status::cancelled && stopped.Terminal.Summary.SearchedFiles == 0U, "cancellation keeps unsearched coverage qualified");
		fixture.OnRead = { };
		core::source_match_limits limits;
		limits.MaximumOwnedBytes = 1024U;
		source_pr_search_execution bounded{ fixture, limits };
		const auto                 refused = bounded.execute(work, { });
		search_expect(refused.Terminal.Status == core::source_search_status::resource_limit, "matcher resource refusal stays distinct");
		corpus_fixture lines;
		lines.add(lines.add_side("a", "needle\rneedle", 0U), lines.add_side("a", "needle", 1U));
		core::source_match_limits lineLimits;
		lineLimits.MaximumLines = 1U;
		source_pr_search_execution lineBounded{ lines, lineLimits };
		const auto                 qualified = lineBounded.execute(lines.work(), { });
		search_expect(qualified.Terminal.Status == core::source_search_status::partial && qualified.Terminal.Summary.Matches == 1U && qualified.Terminal.Summary.SkippedSides == 1U && qualified.Skipped->at(0U).Reason == diff_blob_status::materialization_limit, "CR-only line ceiling qualifies one side and preserves searchable opposite side");
	}
}
int main()
{
	full_documents_and_side_qualification();
	filters_and_invalid_patterns_before_material();
	cap_batches_public_host_and_reordered_progress();
	retry_uses_same_scope_and_keeps_partial_hits();
	cancellation_and_resource_prefix();
	std::cout << "source-pr-search-tests:pass\n";
}
