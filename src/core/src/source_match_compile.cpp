#include "source_matcher_internal.hpp"

#include <new>

namespace bpr::core::source_matching {
	namespace {
		/// Engine memory for each Whole Word helper; both are one character class.
		constexpr std::size_t word_class_memory = 1024U * 1024U;
		/// Line anchors are on by default, as the RFC defines; `(?-m)` turns them off.
		constexpr std::string_view regex_prefix = "(?m)";

		/// \brief Refuses constructs RE2 lacks or that would split a scalar.
		/// \details Lookaround and backreferences are unsupported by design (no
		/// backtracking); `\C` matches one byte and could end a match inside a
		/// UTF-8 scalar. Character classes and `\Q...\E` quote their contents.
		void refuse_unsupported(std::string_view aPattern, budget& aWork)
		{
			auto inClass  = false;
			auto inQuote  = false;
			auto previous = std::size_t{ 0U };
			for (auto offset = std::size_t{ 0U }; offset < aPattern.size(); ++offset) {
				aWork.tick();
				const auto value = aPattern[offset];
				if (inQuote) {
					if (value == '\\' && offset + 1U < aPattern.size() && aPattern[offset + 1U] == 'E') {
						inQuote = false;
						++offset;
					}
					continue;
				}
				if (value == '\\') {
					if (offset + 1U == aPattern.size()) {
						return;
					}
					const auto escaped = aPattern[++offset];
					if (escaped == 'C' || (!inClass && escaped >= '1' && escaped <= '9')) {
						throw stopped{ source_search_status::unsupported_pattern, offset - 1U };
					}
					inQuote = !inClass && escaped == 'Q';
					continue;
				}
				if (inClass) {
					// A ']' first in the class, or after '^', is a literal member.
					if (value == ']' && offset > previous + 1U && !(offset == previous + 2U && aPattern[previous + 1U] == '^')) {
						inClass = false;
					}
					continue;
				}
				if (value == '[') {
					inClass  = true;
					previous = offset;
					continue;
				}
				const auto rest = aPattern.substr(offset);
				if (rest.starts_with("(?=") || rest.starts_with("(?!") || rest.starts_with("(?<=") || rest.starts_with("(?<!")) {
					throw stopped{ source_search_status::unsupported_pattern, offset };
				}
			}
		}
		auto compile_class(const source_regex_engine& aEngine, std::string_view aPattern) -> source_regex_compile
		{
			auto compiled = aEngine.compile(aPattern, { .Literal = false, .CaseSensitive = true, .MaximumMemory = word_class_memory });
			if (compiled.Status != source_search_status::complete || !compiled.Regex) {
				throw stopped{ source_search_status::resource_limit };
			}
			return compiled;
		}
	}
}
namespace bpr::core {
	source_match_program::source_match_program(std::shared_ptr<const implementation> aProgram)
		: Program(std::move(aProgram))
	{ }
	auto source_match_program::retained_bytes() const noexcept -> std::size_t { return Program->OwnedBytes; }
	auto compile_source_match(const source_regex_engine& aEngine, std::string_view aQuery, source_search_options aOptions, source_match_limits aLimits, diff_compute_stop* aStop) -> source_match_compile
	{
		using namespace source_matching;
		auto result = source_match_compile{ };
		try {
			auto work = budget{ aLimits.MaximumCompileSteps, aStop };
			try {
				if (!limits_valid(aLimits)) {
					throw stopped{ source_search_status::resource_limit };
				}
				if (aQuery.empty()) {
					result.Status = source_search_status::idle;
					return result;
				}
				if (aQuery.size() > maximum_source_search_query_bytes) {
					throw stopped{ source_search_status::invalid_input };
				}
				for (auto offset = std::size_t{ 0U }; offset < aQuery.size();) {
					work.tick();
					(void)scalar(aQuery, offset);
				}
				// [[BPR-1432]]. CRLF reads as LF; a lone CR stays literal.
				auto normalized = std::string{ };
				normalized.reserve(aQuery.size());
				for (auto offset = std::size_t{ 0U }; offset < aQuery.size(); ++offset) {
					work.tick();
					if (aQuery[offset] == '\r' && offset + 1U < aQuery.size() && aQuery[offset + 1U] == '\n') {
						continue;
					}
					normalized += aQuery[offset];
				}
				auto pattern = normalized;
				if (aOptions.RegularExpression) {
					refuse_unsupported(normalized, work);
					pattern = std::string{ regex_prefix } + normalized;
				}
				const auto memory = aLimits.MaximumProgramBytes + (aOptions.WholeWord ? 2U * word_class_memory : 0U) + pattern.size();
				if (memory > aLimits.MaximumOwnedBytes) {
					throw stopped{ source_search_status::resource_limit };
				}
				work.poll();
				auto compiled = aEngine.compile(pattern, { .Literal = !aOptions.RegularExpression, .CaseSensitive = aOptions.MatchCase, .MaximumMemory = aLimits.MaximumProgramBytes });
				if (compiled.Status != source_search_status::complete || !compiled.Regex) {
					const auto prefix = aOptions.RegularExpression ? regex_prefix.size() : 0U;
					throw stopped{ compiled.Status == source_search_status::complete ? source_search_status::failure : compiled.Status, compiled.ErrorOffset > prefix ? compiled.ErrorOffset - prefix : 0U };
				}
				auto program        = std::make_shared<source_match_program::implementation>();
				program->Limits     = aLimits;
				program->Options    = aOptions;
				program->Query      = std::move(compiled.Regex);
				program->OwnedBytes = std::max(compiled.RetainedBytes, aLimits.MaximumProgramBytes);
				if (aOptions.WholeWord) {
					auto word        = compile_class(aEngine, R"([\p{L}\p{M}\p{N}_])");
					auto other       = compile_class(aEngine, R"([^\p{L}\p{M}\p{N}_])");
					program->Word    = std::move(word.Regex);
					program->NonWord = std::move(other.Regex);
					program->OwnedBytes += std::max(word.RetainedBytes, word_class_memory) + std::max(other.RetainedBytes, word_class_memory);
				}
				result.Program = source_match_access::make(std::move(program));
				result.Status  = source_search_status::complete;
			} catch (const stopped& stop) {
				result.Status      = stop.Status;
				result.ErrorOffset = stop.Offset;
			}
			result.Steps = work.Steps;
		} catch (const stopped& stop) {
			result.Status = stop.Status;
		} catch (const std::bad_alloc&) {
			result.Status = source_search_status::resource_limit;
		}
		return result;
	}
}
