#include "bpr/core/source_matcher.hpp"

#include <re2/re2.h>

#include <new>

namespace bpr::core {
	namespace {
		class re2_source_regex final : public source_regex
		{
		public:
			re2_source_regex(const std::string_view aPattern, const RE2::Options& aOptions)
				: Program{ aPattern, aOptions }
			{ }
			[[nodiscard]] auto find(const std::string_view aText, const std::size_t aBegin, const std::size_t aEnd, const bool aAnchored) const -> std::optional<byte_span> override
			{
				if (aBegin > aEnd || aEnd > aText.size()) {
					return std::nullopt;
				}
				auto match = std::string_view{ };
				if (!Program.Match(aText, aBegin, aEnd, aAnchored ? RE2::ANCHOR_START : RE2::UNANCHORED, &match, 1)) {
					return std::nullopt;
				}
				const auto begin = static_cast<std::uint64_t>(match.data() - aText.data());
				return byte_span{ begin, begin + match.size() };
			}
			[[nodiscard]] auto program() const noexcept -> const RE2& { return Program; }

		private:
			RE2 Program;
		};
	}

	namespace {
		class re2_source_regex_engine final : public source_regex_engine
		{
		public:
			[[nodiscard]] auto compile(std::string_view aPattern, source_regex_syntax aSyntax) const -> source_regex_compile override;
		};
	}

	auto re2_source_regex_engine::compile(const std::string_view aPattern, const source_regex_syntax aSyntax) const -> source_regex_compile
	{
		auto result = source_regex_compile{ };
		try {
			auto options = RE2::Options{ };
			options.set_encoding(RE2::Options::EncodingUTF8);
			options.set_log_errors(false);
			options.set_never_capture(true);
			options.set_literal(aSyntax.Literal);
			options.set_case_sensitive(aSyntax.CaseSensitive);
			options.set_max_mem(static_cast<std::int64_t>(aSyntax.MaximumMemory));
			auto regex = std::make_shared<const re2_source_regex>(aPattern, options);
			if (!regex->program().ok()) {
				result.Status = regex->program().error_code() == RE2::ErrorPatternTooLarge ? source_search_status::resource_limit : source_search_status::invalid_pattern;
				// RE2 names the offending fragment rather than its offset; the last
				// occurrence is where parsing stopped.
				const auto fragment = regex->program().error_arg();
				const auto at       = fragment.empty() ? std::string_view::npos : aPattern.rfind(fragment);
				result.ErrorOffset  = at == std::string_view::npos ? 0U : at;
				return result;
			}
			result.Status        = source_search_status::complete;
			result.RetainedBytes = aSyntax.MaximumMemory + aPattern.size();
			result.Regex         = std::move(regex);
		} catch (const std::bad_alloc&) {
			result.Status = source_search_status::resource_limit;
		}
		return result;
	}

	auto default_source_regex_engine() -> const source_regex_engine&
	{
		static const auto engine = re2_source_regex_engine{ };
		return engine;
	}
}
