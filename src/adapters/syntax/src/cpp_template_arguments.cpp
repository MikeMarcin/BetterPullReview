#include "cpp_template_arguments.hpp"

#include <array>
#include <string_view>

namespace bpr::adapters::syntax {
	namespace {
		using role = core::appearance_role;

		/// Facts kept per unit of a line that holds at least one list.
		constexpr auto ListOpen  = 1U;
		constexpr auto ListClose = 2U;
		constexpr auto ListComma = 4U;
		constexpr auto NoClose   = std::int32_t{ -1 };

		/// Words a backward or forward walk around one argument may cross. A
		/// malformed line cannot make a walk longer than this.
		constexpr auto MaximumWalk = 64;

		[[nodiscard]] constexpr auto identifier_start(const char16_t aUnit) noexcept -> bool
		{
			return (aUnit >= u'a' && aUnit <= u'z') || (aUnit >= u'A' && aUnit <= u'Z') || aUnit == u'_';
		}

		[[nodiscard]] constexpr auto identifier_unit(const char16_t aUnit) noexcept -> bool
		{
			return identifier_start(aUnit) || (aUnit >= u'0' && aUnit <= u'9');
		}

		[[nodiscard]] constexpr auto blank(const char16_t aUnit) noexcept -> bool
		{
			return aUnit == u' ' || aUnit == u'\t' || aUnit == u'\r';
		}

		/// Qualifiers that may stand before the type an argument names.
		[[nodiscard]] auto leading_qualifier(const QStringView aWord) noexcept -> bool
		{
			constexpr auto words = std::array<std::u16string_view, 7U>{
				u"typename",
				u"const",
				u"volatile",
				u"struct",
				u"class",
				u"enum",
				u"union",
			};
			for (const auto qualifier : words) {
				if (aWord == QStringView{ qualifier.data(), static_cast<qsizetype>(qualifier.size()) }) {
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] auto trailing_qualifier(const QStringView aWord) noexcept -> bool
		{
			return aWord == u"const" || aWord == u"volatile";
		}
	}

	auto cpp_template_arguments::unit(const qsizetype aAt) const noexcept -> char16_t
	{
		return aAt >= 0 && aAt < Line.size() ? Line[aAt].unicode() : u'\0';
	}

	auto cpp_template_arguments::carries(const qsizetype aAt, const unsigned aFacts) const noexcept -> bool
	{
		return aAt >= 0 && aAt < Line.size() && (static_cast<unsigned>(Facts[static_cast<std::size_t>(aAt)]) & aFacts) != 0U;
	}

	void cpp_template_arguments::mark(const qsizetype aAt, const unsigned aFacts)
	{
		auto& facts = Facts[static_cast<std::size_t>(aAt)];
		facts       = static_cast<std::uint8_t>(facts | aFacts);
	}

	auto cpp_template_arguments::opens(const qsizetype aAt) const noexcept -> bool
	{
		const auto next = unit(aAt + 1);
		if (next == u'<' || next == u'=') {
			return false;
		}
		auto begin = aAt;
		while (begin > 0 && identifier_unit(unit(begin - 1))) {
			--begin;
		}
		return begin < aAt && identifier_start(unit(begin)) && Line.mid(begin, aAt - begin) != u"operator";
	}

	void cpp_template_arguments::abandon(const int aDepth)
	{
		while (!Open.empty() && Open.back().Depth >= aDepth) {
			Commas.resize(Open.back().FirstComma);
			Open.pop_back();
		}
	}

	void cpp_template_arguments::close(const qsizetype aAt)
	{
		const auto list = Open.back();
		Open.pop_back();
		Close[static_cast<std::size_t>(list.At)] = static_cast<std::int32_t>(aAt);
		mark(list.At, ListOpen);
		mark(aAt, ListClose);
		for (auto comma = list.FirstComma; comma < Commas.size(); ++comma) {
			mark(Commas[comma], ListComma);
		}
		Commas.resize(list.FirstComma);
		Found = true;
	}

	void cpp_template_arguments::scan(const QStringView aLine)
	{
		Line  = aLine;
		Found = false;
		Open.clear();
		Commas.clear();
		if (!aLine.contains(u'<')) {
			return;
		}
		const auto size = static_cast<std::size_t>(aLine.size());
		Close.assign(size, NoClose);
		Facts.assign(size, 0U);
		auto depth = 0;
		for (auto at = qsizetype{ 0 }; at < aLine.size(); ++at) {
			const auto next = unit(at + 1);
			switch (unit(at)) {
			case u'<':
				if (opens(at)) {
					Open.push_back({ .At = at, .Depth = depth, .FirstComma = Commas.size() });
				} else {
					// A comparison or shift at a list's own depth cannot be inside it.
					abandon(depth);
					at += next == u'<' || next == u'=' ? 1 : 0;
				}
				break;
			case u'>':
				if (next == u'=') {
					abandon(depth);
					++at;
				} else if (!Open.empty() && Open.back().Depth == depth) {
					close(at);
				}
				break;
			case u'-':
				if (next == u'>') {
					abandon(depth);
					++at;
				}
				break;
			case u'(':
			case u'[':
				++depth;
				break;
			case u')':
			case u']':
				// A list cannot outlive the parenthesis it was opened inside, and
				// one closing a parenthesis opened before the list ends it.
				if (depth == 0) {
					abandon(0);
				} else {
					--depth;
					abandon(depth + 1);
				}
				break;
			case u',':
				if (!Open.empty() && Open.back().Depth == depth) {
					Commas.push_back(at);
				}
				break;
			case u'=':
			case u'?':
			case u'|':
				abandon(depth);
				break;
			case u'&':
				if (next == u'&') {
					auto operand = at + 2;
					while (blank(unit(operand))) {
						++operand;
					}
					const auto following = unit(operand);
					// `T&&` ends an argument; `a && b` is a logical operator.
					if (identifier_unit(following) || following == u'(' || following == u'!') {
						abandon(depth);
					}
					++at;
				}
				break;
			case u'/':
				if (next == u'/') {
					abandon(0);
					at = aLine.size();
				} else if (next == u'*') {
					abandon(0);
					const auto end = aLine.indexOf(u"*/", at + 2);
					at             = end < 0 ? aLine.size() : end + 1;
				}
				break;
			case u'\'':
				if (at > 0 && unit(at - 1) >= u'0' && unit(at - 1) <= u'9') {
					break; // A digit separator.
				}
				[[fallthrough]];
			case u'"': {
				abandon(0);
				const auto quote = unit(at);
				for (++at; at < aLine.size() && unit(at) != quote; ++at) {
					at += unit(at) == u'\\' ? 1 : 0;
				}
				break;
			}
			case u';':
			case u'{':
			case u'}':
				abandon(0);
				break;
			default:
				break;
			}
		}
		Open.clear();
		Commas.clear();
	}

	auto cpp_template_arguments::argument(const qsizetype aBegin, const qsizetype aAfter) const noexcept -> bool
	{
		const auto back = [this](qsizetype aAt) {
			while (aAt >= 0 && blank(unit(aAt))) {
				--aAt;
			}
			return aAt;
		};
		const auto wordBefore = [this](const qsizetype aLast) {
			auto begin = aLast + 1;
			while (begin > 0 && identifier_unit(unit(begin - 1))) {
				--begin;
			}
			return begin;
		};
		// Before: a `::` path and leading qualifiers, then the list's `<` or `,`.
		auto at = back(aBegin - 1);
		for (auto walked = 0; walked < MaximumWalk && at >= 0; ++walked) {
			if (unit(at) == u':' && unit(at - 1) == u':') {
				const auto last  = back(at - 2);
				const auto first = wordBefore(last);
				at               = first <= last ? back(first - 1) : last;
			} else if (identifier_unit(unit(at)) && leading_qualifier(Line.mid(wordBefore(at), at + 1 - wordBefore(at)))) {
				at = back(wordBefore(at) - 1);
			} else {
				break;
			}
		}
		if (!carries(at, ListOpen | ListComma)) {
			return false;
		}
		// After: a function type, an array, a pack, or declarator punctuation and
		// trailing qualifiers before the list's `,` or `>`.
		auto       next = aAfter;
		const auto skip = [this, &next] {
			while (blank(unit(next))) {
				++next;
			}
		};
		skip();
		if (unit(next) == u'(' || unit(next) == u'[' || (unit(next) == u'.' && unit(next + 1) == u'.' && unit(next + 2) == u'.')) {
			return true;
		}
		for (auto walked = 0; walked < MaximumWalk && next < Line.size(); ++walked) {
			if (unit(next) == u'*' || unit(next) == u'&') {
				++next;
			} else if (identifier_start(unit(next))) {
				auto end = next;
				while (identifier_unit(unit(end))) {
					++end;
				}
				if (!trailing_qualifier(Line.mid(next, end - next))) {
					return false;
				}
				next = end;
			} else {
				break;
			}
			skip();
		}
		return carries(next, ListClose | ListComma);
	}

	auto cpp_template_arguments::role_of(const qsizetype aBegin, const qsizetype aEnd) const noexcept
		-> std::optional<core::appearance_role>
	{
		if (!Found || aBegin < 0 || aBegin >= aEnd || aEnd > Line.size()) {
			return std::nullopt;
		}
		if (aEnd < Line.size() && Close[static_cast<std::size_t>(aEnd)] != NoClose) {
			// The name opens a list: a call or declaration when `(` follows it,
			// otherwise a type where a type is the only reading.
			auto after = static_cast<qsizetype>(Close[static_cast<std::size_t>(aEnd)]) + 1;
			while (blank(unit(after))) {
				++after;
			}
			const auto following = unit(after);
			if (following == u'(') {
				return role::syntax_function;
			}
			if (identifier_start(following) || following == u'&' || following == u'*' || following == u'{'
				|| (following == u':' && unit(after + 1) == u':') || argument(aBegin, after)) {
				return role::syntax_type;
			}
			return std::nullopt;
		}
		return argument(aBegin, aEnd) ? std::optional{ role::syntax_type } : std::nullopt;
	}
}
