#include "bpr/application/source_pr_search_execution.hpp"
#include "bpr/core/review_preparation.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>

namespace bpr::application {
	namespace {
		class search_stop final : public core::diff_compute_stop
		{
		public:
			search_stop(std::stop_token aUser, std::stop_token aPriority = { })
				: User(aUser)
				, Priority(aPriority)
			{ }
			auto stop_requested(std::uint64_t) -> bool override { return User.stop_requested() || Priority.stop_requested(); }

		private:
			std::stop_token User;
			std::stop_token Priority;
		};
		auto text(const source_corpus_material& aMaterial) -> std::string_view
		{
			return { reinterpret_cast<const char*>(aMaterial.Bytes->data()), aMaterial.Bytes->size() };
		}
		auto source_line_count(std::string_view aBytes) -> std::size_t
		{
			auto count = std::size_t{ 0U };
			for (auto i = std::size_t{ 0U }; i < aBytes.size(); ++i) {
				if (aBytes[i] == '\r' || (aBytes[i] == '\n' && (i == 0U || aBytes[i - 1U] != '\r'))) {
					++count;
				}
			}
			return count + (!aBytes.empty() && aBytes.back() != '\n' && aBytes.back() != '\r' ? 1U : 0U);
		}
		auto qualification(const source_corpus_material& aMaterial, const core::source_match_limits& aLimits) -> diff_blob_status
		{
			if (aMaterial.Status != diff_blob_status::available || !aMaterial.Bytes) {
				return aMaterial.Status == diff_blob_status::available ? diff_blob_status::failure : aMaterial.Status;
			}
			const auto bytes = text(aMaterial);
			if (bytes.size() > aLimits.MaximumBytes || source_line_count(bytes) > aLimits.MaximumLines) {
				return diff_blob_status::materialization_limit;
			}
			if (bytes.find('\0') != std::string_view::npos) {
				return diff_blob_status::binary;
			}
			if (!core::valid_writing_utf8(bytes)) {
				return diff_blob_status::decode_failure;
			}
			return diff_blob_status::available;
		}
	}
	struct source_pr_search_execution::implementation : std::enable_shared_from_this<implementation>
	{
		implementation(source_corpus_port& aCorpus, core::source_match_limits aLimits)
			: Corpus(aCorpus)
			, Limits(aLimits)
		{ }
		source_corpus_port&               Corpus;
		core::source_match_limits         Limits;
		std::mutex                        Mutex;
		std::optional<source_search_work> Demand;

		auto needed(const source_search_work& aWork, const source_corpus_entry& aEntry, const core::source_path_filter& aFilter) -> source_corpus_read_needed
		{
			return [self = shared_from_this(), aWork, aEntry, aFilter] {
				std::scoped_lock lock{ self->Mutex };
				const auto&      demand = self->Demand;
				// A changed path policy is compiled off lane before it can demand
				// new material. Until then stop its predecessor's read conservatively.
				if (!demand || demand->Request.Scope != aWork.Request.Scope || demand->Include != aWork.Include || demand->Exclude != aWork.Exclude
					|| core::inspect_source_search_query(demand->Query) != core::source_search_input::ready) {
					return false;
				}
				return aFilter.includes(aEntry.Left ? std::optional<std::string_view>{ aEntry.Left->Path } : std::nullopt, aEntry.Right ? std::optional<std::string_view>{ aEntry.Right->Path } : std::nullopt);
			};
		}
		auto read_side(const source_search_work& aWork, const std::shared_ptr<const source_corpus>& aCorpus, std::size_t aIndex, core::diff_side aSide, std::stop_token aStop, const core::source_path_filter& aFilter) -> source_corpus_material
		{
			auto answer = source_corpus_material{ };
			do {
				answer = Corpus.read(aCorpus, aIndex, aSide, aStop, needed(aWork, aCorpus->Entries[aIndex], aFilter));
				if (answer.Yielded && !aStop.stop_requested()) {
					std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
				}
			} while (answer.Yielded && !aStop.stop_requested());
			return answer;
		}
		static void publish_progress(const source_search_work& aWork, const std::shared_ptr<const source_corpus>& aCorpus, const std::vector<source_search_match>& aRecords, core::source_search_summary aSummary, std::size_t& aPublished, const source_search_batch_sink& aPublish)
		{
			if (!aPublish) {
				return;
			}
			do {
				const auto count                      = std::min(std::size_t{ 128U }, aRecords.size() - aPublished);
				auto       batch                      = source_search_batch{ };
				batch.Request                         = aWork.Request;
				batch.Summary                         = aSummary;
				batch.Summary.Matches                 = static_cast<std::uint32_t>(aPublished + count);
				batch.Summary.AdditionalMatchObserved = false;
				batch.Corpus                          = aCorpus;
				batch.Begin                           = static_cast<std::uint32_t>(aPublished);
				batch.Matches.assign(aRecords.begin() + static_cast<std::ptrdiff_t>(aPublished), aRecords.begin() + static_cast<std::ptrdiff_t>(aPublished + count));
				aPublished += count;
				aPublish(std::move(batch));
			} while (aPublished < aRecords.size());
		}
		auto execute(const source_search_work& aWork, std::stop_token aStop, const source_search_batch_sink& aPublish) -> source_search_execution
		{
			auto output = source_search_execution{ core::source_search_terminal{ aWork.Request, core::source_search_status::unavailable, { } } };
			if (!core::valid(aWork.Request.Scope) || aWork.Request.Scope.Target != core::source_search_target::files || aWork.Query.empty()) {
				return output;
			}
			auto       stop        = search_stop{ aStop };
			const auto compiled    = core::compile_source_match(aWork.Query, aWork.Request.Options, Limits, &stop);
			output.Terminal.Status = compiled.Status;
			if (!compiled.Program) {
				return output;
			}
			const auto paths       = core::compile_source_paths(aWork.Include, aWork.Exclude, Limits, &stop);
			output.Terminal.Status = paths.Status;
			if (!paths.Filter) {
				return output;
			}
			const auto observation = Corpus.catalogue(aWork.Request.Scope, aStop);
			output.Terminal.Status = observation.Status;
			if (!observation.Corpus || observation.Corpus->Scope != aWork.Request.Scope) {
				output.RetryAvailable = observation.Status == core::source_search_status::unavailable || observation.Status == core::source_search_status::failure;
				return output;
			}
			const auto corpus = observation.Corpus;
			output.Corpus     = corpus;
			auto selected     = std::vector<std::size_t>{ };
			selected.reserve(corpus->Entries.size());
			for (auto i = std::size_t{ 0U }; i < corpus->Entries.size(); ++i) {
				if (aStop.stop_requested()) {
					output.Terminal.Status = core::source_search_status::cancelled;
					return output;
				}
				const auto& entry    = corpus->Entries[i];
				const auto  decision = paths.Filter->evaluate(entry.Left ? std::optional<std::string_view>{ entry.Left->Path } : std::nullopt, entry.Right ? std::optional<std::string_view>{ entry.Right->Path } : std::nullopt);
				if (decision.Status != core::source_search_status::complete) {
					output.Terminal.Status = decision.Status;
					return output;
				}
				if (decision.Included) {
					selected.push_back(i);
				}
			}
			auto& summary         = output.Terminal.Summary;
			summary.EligibleFiles = static_cast<std::uint32_t>(selected.size());
			const auto bound      = 16U * 1024U * 1024U + 2U * Limits.MaximumBytes + 2U * maximum_git_changed_catalogue_bytes + Limits.MaximumOwnedBytes + 3U * Limits.MaximumResultBytes + selected.size() * 2U * sizeof(source_search_skipped_side) + selected.capacity() * sizeof(std::size_t);
			output.PeakOwnedBound = bound;
			if (bound > maximum_source_pr_search_owned_bytes) {
				output.Terminal.Status = core::source_search_status::resource_limit;
				return output;
			}
			auto records = std::make_shared<std::vector<source_search_match>>();
			records->reserve(Limits.MaximumMatches);
			auto skipped = std::make_shared<std::vector<source_search_skipped_side>>();
			skipped->reserve(selected.size() * 2U);
			auto retainedBytes     = records->capacity() * sizeof(source_search_match);
			auto published         = std::size_t{ 0U };
			output.Terminal.Status = core::source_search_status::complete;
			publish_progress(aWork, corpus, *records, summary, published, aPublish);
			for (const auto index : selected) {
				if (aStop.stop_requested()) {
					output.Terminal.Status = core::source_search_status::cancelled;
					break;
				}
				const auto& entry    = corpus->Entries[index];
				auto        material = std::array<source_corpus_material, 2U>{ };
				auto        scan     = core::source_match_entry{ };
				scan.File            = entry.File;
				for (auto sideIndex = std::size_t{ 0U }; sideIndex < material.size(); ++sideIndex) {
					const auto& side = sideIndex == 0U ? entry.Left : entry.Right;
					if (!side) {
						continue;
					}
					const auto sideKind = sideIndex == 0U ? core::diff_side::left : core::diff_side::right;
					if (sideIndex == 1U && entry.Left && scan.Left && entry.Left->Object == side->Object && entry.Left->Mode == side->Mode) {
						material[1] = material[0];
					} else {
						material[sideIndex] = read_side(aWork, corpus, index, sideKind, aStop, *paths.Filter);
					}
					const auto reason = qualification(material[sideIndex], Limits);
					if (reason != diff_blob_status::available) {
						skipped->push_back({ entry.File, sideKind, reason });
						++summary.SkippedSides;
						output.RetryAvailable = output.RetryAvailable || reason == diff_blob_status::missing || reason == diff_blob_status::failure;
					} else {
						(sideIndex == 0U ? scan.Left : scan.Right) = core::source_match_side{ side->Object, text(material[sideIndex]) };
					}
				}
				if (aStop.stop_requested()) {
					output.Terminal.Status = core::source_search_status::cancelled;
					break;
				}
				if (!scan.Left && !scan.Right) {
					publish_progress(aWork, corpus, *records, summary, published, aPublish);
					continue;
				}
				auto matched = core::source_match_entry_result{ };
				do {
					const auto priority = Corpus.scan_stop_token(aWork.Request.Scope);
					auto       scanStop = search_stop{ aStop, priority };
					matched             = core::match_source_entry(*compiled.Program, scan, &scanStop);
					if (matched.Status == core::source_search_status::cancelled && !aStop.stop_requested() && priority.stop_requested()) {
						const auto current = Corpus.catalogue(aWork.Request.Scope, aStop);
						if (current.Corpus != corpus) {
							matched.Status = core::source_search_status::unavailable;
							break;
						}
						std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
						continue;
					}
					break;
				} while (!aStop.stop_requested());
				if (matched.ScannedSides != 0U) {
					++summary.SearchedFiles;
				}
				for (auto& match : matched.Matches) {
					if (records->size() == Limits.MaximumMatches) {
						summary.AdditionalMatchObserved = true;
						break;
					}
					const auto bytes = match.Preview.capacity() + 1U;
					if (bytes > Limits.MaximumResultBytes || retainedBytes > Limits.MaximumResultBytes - bytes) {
						output.Terminal.Status = core::source_search_status::resource_limit;
						break;
					}
					retainedBytes += bytes;
					auto record  = source_search_match{ };
					record.File  = entry.File;
					record.Match = std::move(match);
					records->push_back(std::move(record));
				}
				summary.Matches = static_cast<std::uint32_t>(records->size());
				publish_progress(aWork, corpus, *records, summary, published, aPublish);
				if (records->size() == Limits.MaximumMatches) {
					output.Terminal.Status          = Limits.MaximumMatches == core::maximum_source_search_matches ? core::source_search_status::result_limit : core::source_search_status::resource_limit;
					summary.AdditionalMatchObserved = Limits.MaximumMatches == core::maximum_source_search_matches && (summary.AdditionalMatchObserved || matched.AdditionalMatchObserved);
					break;
				}
				if (matched.Status != core::source_search_status::complete) {
					output.Terminal.Status = matched.Status;
					break;
				}
				if (output.Terminal.Status == core::source_search_status::resource_limit) {
					break;
				}
			}
			if (output.Terminal.Status == core::source_search_status::complete && (summary.SkippedSides != 0U || summary.SearchedFiles != summary.EligibleFiles)) {
				output.Terminal.Status = core::source_search_status::partial;
			}
			output.Matches = std::move(records);
			output.Skipped = std::move(skipped);
			return output;
		}
	};
	source_pr_search_execution::source_pr_search_execution(source_corpus_port& aCorpus, core::source_match_limits aLimits)
		: Implementation(std::make_shared<implementation>(aCorpus, aLimits))
	{ }
	source_pr_search_execution::~source_pr_search_execution() { update_demand({ }); }
	auto source_pr_search_execution::execute(const source_search_work& aWork, std::stop_token aStop) -> source_search_execution { return Implementation->execute(aWork, aStop, { }); }
	auto source_pr_search_execution::execute(const source_search_work& aWork, std::stop_token aStop, const source_search_batch_sink& aPublish) -> source_search_execution { return Implementation->execute(aWork, aStop, aPublish); }
	void source_pr_search_execution::update_demand(std::optional<source_search_work> aWork)
	{
		{
			std::scoped_lock lock{ Implementation->Mutex };
			Implementation->Demand = std::move(aWork);
		}
		Implementation->Corpus.demand_changed();
	}
}
