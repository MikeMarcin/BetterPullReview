#include "bpr/application/source_search_host.hpp"

namespace bpr::application {
	namespace {
		class matcher_stop final : public core::diff_compute_stop
		{
		public:
			explicit matcher_stop(std::stop_token aStop)
				: Stop(aStop)
			{ }
			auto stop_requested(std::uint64_t) -> bool override { return Stop.stop_requested(); }

		private:
			std::stop_token Stop;
		};
	}
	auto execute_source_pane_match(const source_search_work& aWork, const core::source_match_entry& aEntry, std::stop_token aStop, core::source_match_limits aLimits) -> source_search_execution
	{
		auto        output = source_search_execution{ core::source_search_terminal{ aWork.Request, core::source_search_status::unavailable, { } } };
		const auto& scope  = aWork.Request.Scope;
		if (!core::valid(scope) || scope.Target == core::source_search_target::files || !scope.Document || scope.Document->File != aEntry.File || scope.Target != aEntry.Target) {
			return output;
		}
		if (scope.Target == core::source_search_target::unified) {
			if (!aEntry.Unified || aEntry.Unified->diff().Alignment.Digest != scope.Document->Comparison
				|| aEntry.Unified->diff().Alignment.Key.LeftBlob != scope.Document->LeftBlob || aEntry.Unified->diff().Alignment.Key.RightBlob != scope.Document->RightBlob) {
				return output;
			}
		} else {
			const auto& side = scope.Target == core::source_search_target::left ? aEntry.Left : aEntry.Right;
			const auto& blob = scope.Target == core::source_search_target::left ? scope.Document->LeftBlob : scope.Document->RightBlob;
			if (!side || side->Blob != blob) {
				return output;
			}
		}
		const auto selection = scope.Selection ? std::optional{ std::pair{ scope.Selection->Side, scope.Selection->Span } } : std::nullopt;
		if (selection != aEntry.Selection) {
			return output;
		}
		const auto transferBytes = static_cast<std::size_t>(aLimits.MaximumMatches) * sizeof(source_search_match);
		if (transferBytes >= aLimits.MaximumOwnedBytes) {
			output.Terminal.Status = core::source_search_status::resource_limit;
			return output;
		}
		aLimits.MaximumOwnedBytes -= transferBytes;
		auto       stop        = matcher_stop{ aStop };
		const auto compiled    = core::compile_source_match(aWork.Query, aWork.Request.Options, aLimits, &stop);
		output.Terminal.Status = compiled.Status;
		if (!compiled.Program) {
			return output;
		}
		auto matches                                    = core::match_source_entry(*compiled.Program, aEntry, &stop);
		output.Terminal.Status                          = matches.Status;
		output.Terminal.Summary.EligibleFiles           = 1U;
		output.Terminal.Summary.SearchedFiles           = matches.ScannedSides > 0U ? 1U : 0U;
		output.Terminal.Summary.Matches                 = static_cast<std::uint32_t>(matches.Matches.size());
		output.Terminal.Summary.AdditionalMatchObserved = matches.AdditionalMatchObserved;
		auto records                                    = std::make_shared<std::vector<source_search_match>>();
		records->reserve(matches.Matches.size());
		for (auto& match : matches.Matches) {
			auto record  = source_search_match{ };
			record.File  = aEntry.File;
			record.Match = std::move(match);
			records->push_back(std::move(record));
		}
		output.Matches = std::move(records);
		return output;
	}
}
