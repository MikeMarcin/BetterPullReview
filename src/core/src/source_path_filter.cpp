#include "source_matcher_internal.hpp"

#include <new>

namespace bpr::core {
	struct source_path_filter::implementation
	{
		std::shared_ptr<const source_regex> Include;
		std::shared_ptr<const source_regex> Exclude;
		std::uint64_t                       MaximumSteps = 0U;
	};
	namespace {
		using namespace source_matching;
		void append_literal(std::string& aOutput, char aValue)
		{
			if (std::string_view{ ".^$|()[]{}*+?\\" }.find(aValue) != std::string_view::npos) {
				aOutput.push_back('\\');
			}
			aOutput.push_back(aValue);
		}
		auto split_globs(std::string_view aFilter, budget& aWork) -> std::vector<std::string_view>
		{
			auto       result  = std::vector<std::string_view>{ };
			auto       depth   = 0U;
			auto       inClass = false;
			auto       escaped = false;
			auto       begin   = std::size_t{ 0U };
			const auto append  = [&](std::size_t aEnd) {
				auto text = aFilter.substr(begin, aEnd - begin);
				while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
					text.remove_prefix(1U);
				}
				while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
					text.remove_suffix(1U);
				}
				if (text.empty()) {
					throw stopped{ source_search_status::invalid_pattern, aEnd };
				}
				result.push_back(text);
			};
			for (auto offset = std::size_t{ 0U }; offset < aFilter.size(); ++offset) {
				aWork.tick();
				const auto value = aFilter[offset];
				if (escaped) {
					escaped = false;
					continue;
				}
				if (value == '\\') {
					escaped = true;
					continue;
				}
				if (inClass) {
					if (value == ']') {
						inClass = false;
					}
					continue;
				}
				if (value == '[') {
					inClass = true;
				} else if (value == '{') {
					if (++depth > 64U) {
						throw stopped{ source_search_status::resource_limit, offset };
					}
				} else if (value == '}') {
					if (depth == 0U) {
						throw stopped{ source_search_status::invalid_pattern, offset };
					}
					--depth;
				} else if (value == ',' && depth == 0U) {
					append(offset);
					begin = offset + 1U;
				}
			}
			if (escaped || inClass || depth != 0U) {
				throw stopped{ source_search_status::invalid_pattern, aFilter.size() };
			}
			if (!aFilter.empty()) {
				append(aFilter.size());
			}
			return result;
		}
		auto glob_regex(std::string_view aGlob, budget& aWork) -> std::string
		{
			auto       output = std::string{ "(?:" };
			const auto rooted = aGlob.starts_with("./");
			if (rooted) {
				aGlob.remove_prefix(2U);
			}
			if (aGlob.empty() || aGlob.front() == '/') {
				throw stopped{ source_search_status::invalid_pattern };
			}
			if (!rooted && aGlob.find('/') == std::string_view::npos) {
				output += "(?:.*/)?";
			}
			auto braceItems = std::vector<bool>{ };
			for (auto offset = std::size_t{ 0U }; offset < aGlob.size(); ++offset) {
				aWork.tick();
				const auto value = aGlob[offset];
				if (value == '\\') {
					if (++offset == aGlob.size()) {
						throw stopped{ source_search_status::invalid_pattern };
					}
					append_literal(output, aGlob[offset]);
				} else if (value == '*') {
					if (offset + 1U < aGlob.size() && aGlob[offset + 1U] == '*') {
						++offset;
						if (offset + 1U < aGlob.size() && aGlob[offset + 1U] == '/') {
							++offset;
							output += "(?:.*/)?";
						} else {
							output += ".*";
						}
					} else {
						output += "[^/]*";
					}
				} else if (value == '?') {
					output += "[^/]";
				} else if (value == '{') {
					braceItems.push_back(false);
					output += "(?:";
					continue;
				} else if (value == '}' || (value == ',' && !braceItems.empty())) {
					if (braceItems.empty() || !braceItems.back()) {
						throw stopped{ source_search_status::invalid_pattern, offset };
					}
					if (value == '}') {
						braceItems.pop_back();
						output += ')';
					} else {
						braceItems.back() = false;
						output += '|';
						continue;
					}
				} else if (value == '[') {
					output += '[';
					++offset;
					if (offset < aGlob.size() && (aGlob[offset] == '!' || aGlob[offset] == '^')) {
						output += '^';
						++offset;
					}
					auto count = 0U;
					while (offset < aGlob.size() && aGlob[offset] != ']') {
						aWork.tick();
						auto item = aGlob[offset++];
						if (item == '\\') {
							if (offset == aGlob.size()) {
								throw stopped{ source_search_status::invalid_pattern };
							}
							item = aGlob[offset++];
							output += '\\';
						} else if (item == '[') {
							output += '\\';
						}
						output += item;
						++count;
					}
					if (offset == aGlob.size() || count == 0U) {
						throw stopped{ source_search_status::invalid_pattern, offset };
					}
					output += ']';
				} else {
					append_literal(output, value);
				}
				if (!braceItems.empty()) {
					braceItems.back() = true;
				}
				if (output.size() > 32760U) {
					throw stopped{ source_search_status::resource_limit };
				}
			}
			output += ')';
			return output;
		}
		/// Engine memory for one compiled include or exclude list.
		constexpr std::size_t path_program_memory = 2U * 1024U * 1024U;
		auto                  compile_filter(const source_regex_engine& aEngine, std::string_view aFilter, budget& aWork) -> source_regex_compile
		{
			if (aFilter.empty()) {
				return { .Status = source_search_status::idle };
			}
			const auto alternatives = split_globs(aFilter, aWork);
			auto       pattern      = std::string{ "\\A(?:" };
			for (const auto& alternative : alternatives) {
				if (pattern.size() > 5U) {
					pattern += '|';
				}
				pattern += glob_regex(alternative, aWork);
				if (pattern.size() > 32760U) {
					throw stopped{ source_search_status::resource_limit };
				}
			}
			pattern += ")\\z";
			aWork.poll();
			auto compiled = aEngine.compile(pattern, { .Literal = false, .CaseSensitive = true, .MaximumMemory = path_program_memory });
			if (compiled.Status != source_search_status::complete || !compiled.Regex) {
				// Translation already validated the glob, so a refusal here is a resource one.
				throw stopped{ compiled.Status == source_search_status::resource_limit ? source_search_status::resource_limit : source_search_status::invalid_pattern };
			}
			return compiled;
		}
		auto path_matches(const std::shared_ptr<const source_regex>& aProgram, std::uint64_t aMaximumSteps, std::optional<std::string_view> aPath) -> source_path_decision
		{
			if (!aPath) {
				return { };
			}
			if (aPath->empty() || aPath->size() > 4096U || aPath->front() == '/' || aPath->starts_with("./") || aPath->find('\0') != std::string_view::npos) {
				return { source_search_status::invalid_input, false };
			}
			if (!aProgram) {
				return { source_search_status::complete, true };
			}
			try {
				// One counted step per scalar validates the path and bounds its work.
				auto work = budget{ aMaximumSteps, nullptr };
				for (auto offset = std::size_t{ 0U }; offset < aPath->size();) {
					work.tick();
					(void)scalar(*aPath, offset);
				}
			} catch (const stopped& stop) {
				return { stop.Status, false };
			}
			return { source_search_status::complete, aProgram->find(*aPath, 0U, aPath->size(), true).has_value() };
		}
		auto either_path(const std::shared_ptr<const source_regex>& aProgram, std::uint64_t aMaximumSteps, std::optional<std::string_view> aLeft, std::optional<std::string_view> aRight) -> source_path_decision
		{
			const auto left = path_matches(aProgram, aMaximumSteps, aLeft);
			if (left.Status != source_search_status::complete) {
				return left;
			}
			const auto right = path_matches(aProgram, aMaximumSteps, aRight);
			if (right.Status != source_search_status::complete) {
				return right;
			}
			return { source_search_status::complete, left.Included || right.Included };
		}
	}
	source_path_filter::source_path_filter(std::shared_ptr<const implementation> aFilter)
		: Filter(std::move(aFilter))
	{ }
	auto source_path_filter::evaluate(std::optional<std::string_view> aLeft, std::optional<std::string_view> aRight) const -> source_path_decision
	{
		if (!Filter) {
			return { source_search_status::unavailable, false };
		}
		if (Filter->Exclude) {
			const auto excluded = either_path(Filter->Exclude, Filter->MaximumSteps, aLeft, aRight);
			if (excluded.Status != source_search_status::complete) {
				return excluded;
			}
			if (excluded.Included) {
				return { };
			}
		}
		return either_path(Filter->Include, Filter->MaximumSteps, aLeft, aRight);
	}
	auto source_path_filter::includes(std::optional<std::string_view> aLeft, std::optional<std::string_view> aRight) const -> bool
	{
		const auto decision = evaluate(aLeft, aRight);
		return decision.Status == source_search_status::complete && decision.Included;
	}
	auto compile_source_paths(const source_regex_engine& aEngine, std::string_view aInclude, std::string_view aExclude, source_match_limits aLimits, diff_compute_stop* aStop) -> source_path_compile
	{
		auto result = source_path_compile{ };
		try {
			if (!limits_valid(aLimits) || aInclude.size() + aExclude.size() > maximum_source_search_query_bytes) {
				throw stopped{ source_search_status::resource_limit };
			}
			if (2U * path_program_memory > aLimits.MaximumOwnedBytes) {
				throw stopped{ source_search_status::resource_limit };
			}
			auto work            = budget{ aLimits.MaximumCompileSteps, aStop };
			auto included        = compile_filter(aEngine, aInclude, work);
			auto excluded        = compile_filter(aEngine, aExclude, work);
			auto filter          = std::make_shared<source_path_filter::implementation>();
			filter->Include      = std::move(included.Regex);
			filter->Exclude      = std::move(excluded.Regex);
			filter->MaximumSteps = aLimits.MaximumScanSteps;
			result.Filter.emplace(std::move(filter));
			result.Status = source_search_status::complete;
		} catch (const stopped& stop) {
			result.Status      = stop.Status;
			result.ErrorOffset = stop.Offset;
		} catch (const std::bad_alloc&) {
			result.Status = source_search_status::resource_limit;
		}
		return result;
	}
}
