#include "source_matcher_internal.hpp"

#include <new>

namespace bpr::core {
	struct source_path_filter::implementation
	{
		std::optional<source_match_program> Include;
		std::optional<source_match_program> Exclude;
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
		auto compile_filter(std::string_view aFilter, source_match_limits aLimits, budget& aWork) -> source_match_compile
		{
			if (aFilter.empty()) {
				auto result   = source_match_compile{ };
				result.Status = source_search_status::idle;
				return result;
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
			// Share the total compile budget across glob translation and both programs.
			if (aWork.Steps >= aLimits.MaximumCompileSteps) {
				throw stopped{ source_search_status::resource_limit };
			}
			aLimits.MaximumCompileSteps -= aWork.Steps;
			aLimits.MaximumMatches = 1U;
			aLimits.MaximumLines   = std::min(aLimits.MaximumLines, std::size_t{ 4097U });
			auto result            = compile_pattern(pattern, { .MatchCase = true, .RegularExpression = true }, aLimits, aWork.Stop, true);
			aWork.Steps += result.Steps;
			return result;
		}
		auto path_matches(const std::optional<source_match_program>& aProgram, std::optional<std::string_view> aPath) -> source_path_decision
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
			const auto scanned = scan_source_match(*aProgram, *aPath);
			return { scanned.Status, !scanned.Matches.empty() };
		}
		auto either_path(const std::optional<source_match_program>& aProgram, std::optional<std::string_view> aLeft, std::optional<std::string_view> aRight) -> source_path_decision
		{
			const auto left = path_matches(aProgram, aLeft);
			if (left.Status != source_search_status::complete) {
				return left;
			}
			const auto right = path_matches(aProgram, aRight);
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
			const auto excluded = either_path(Filter->Exclude, aLeft, aRight);
			if (excluded.Status != source_search_status::complete) {
				return excluded;
			}
			if (excluded.Included) {
				return { };
			}
		}
		return either_path(Filter->Include, aLeft, aRight);
	}
	auto source_path_filter::includes(std::optional<std::string_view> aLeft, std::optional<std::string_view> aRight) const -> bool
	{
		const auto decision = evaluate(aLeft, aRight);
		return decision.Status == source_search_status::complete && decision.Included;
	}
	auto compile_source_paths(std::string_view aInclude, std::string_view aExclude, source_match_limits aLimits, diff_compute_stop* aStop) -> source_path_compile
	{
		auto result = source_path_compile{ };
		try {
			if (!limits_valid(aLimits) || aInclude.size() + aExclude.size() > maximum_source_search_query_bytes) {
				throw stopped{ source_search_status::resource_limit };
			}
			auto work     = budget{ aLimits.MaximumCompileSteps, aStop };
			auto included = compile_filter(aInclude, aLimits, work);
			if (included.Status != source_search_status::complete && included.Status != source_search_status::idle) {
				result.Status      = included.Status;
				result.ErrorOffset = included.ErrorOffset;
				return result;
			}
			auto excluded = compile_filter(aExclude, aLimits, work);
			if (excluded.Status != source_search_status::complete && excluded.Status != source_search_status::idle) {
				result.Status      = excluded.Status;
				result.ErrorOffset = excluded.ErrorOffset;
				return result;
			}
			if ((included.Program ? included.Program->retained_bytes() : 0U) + (excluded.Program ? excluded.Program->retained_bytes() : 0U) > aLimits.MaximumOwnedBytes) {
				throw stopped{ source_search_status::resource_limit };
			}
			auto filter     = std::make_shared<source_path_filter::implementation>();
			filter->Include = std::move(included.Program);
			filter->Exclude = std::move(excluded.Program);
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
