#include "bpr/core/code_context_roles.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>

namespace bpr::core {
	namespace {
		using role = appearance_role;

		/// The identifier rule of the C++ refinement: every byte of a UTF-8
		/// sequence continues a name.
		[[nodiscard]] auto identifier_start(const char aByte) noexcept -> bool
		{
			return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || aByte == '_' || static_cast<unsigned char>(aByte) >= 0x80U;
		}

		[[nodiscard]] auto identifier_unit(const char aByte) noexcept -> bool { return identifier_start(aByte) || (aByte >= '0' && aByte <= '9'); }

		[[nodiscard]] auto borrowable(const role aRole) noexcept -> bool
		{
			return aRole == role::syntax_namespace || aRole == role::syntax_type || aRole == role::syntax_function || aRole == role::syntax_enum_member || aRole == role::syntax_macro;
		}

		/// A role that says what a name is: borrowable, or evidence against it.
		[[nodiscard]] auto evidence(const role aRole) noexcept -> bool { return borrowable(aRole) || aRole == role::syntax_variable || aRole == role::syntax_constant; }

		/// A block's own role an identifier may give up: plain, or one the
		/// grammar's heuristics guessed. Its own namespaces and enum members
		/// come from what the block declares and stay.
		[[nodiscard]] auto replaceable(const role aRole) noexcept -> bool
		{
			return aRole == role::syntax_normal || aRole == role::syntax_type || aRole == role::syntax_function || aRole == role::syntax_macro;
		}

		/// \brief Two roles of one name: the one it keeps, or none when they conflict.
		/// \details A class is called where it is constructed, and its
		/// constructor is declared by its name, so a type that is also a
		/// function stays a type. Any other pair conflicts.
		[[nodiscard]] auto combined(const role aFirst, const role aSecond) noexcept -> std::optional<role>
		{
			if (aFirst == aSecond) {
				return aFirst;
			}
			if ((aFirst == role::syntax_type && aSecond == role::syntax_function) || (aFirst == role::syntax_function && aSecond == role::syntax_type)) {
				return role::syntax_type;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto line_starts(const std::string_view aText) -> std::vector<std::size_t>
		{
			auto starts = std::vector<std::size_t>{ 0U };
			for (auto at = aText.find('\n'); at != std::string_view::npos; at = aText.find('\n', at + 1U)) {
				starts.push_back(at + 1U);
			}
			return starts;
		}

		/// \brief The byte range `aSpan` covers, when it lies within one line of `aText`.
		[[nodiscard]] auto range_of(const std::string_view aText, const std::vector<std::size_t>& aLines, const token_span& aSpan) -> std::optional<std::pair<std::size_t, std::size_t>>
		{
			if (aSpan.Line >= aLines.size() || aSpan.Length == 0U) {
				return std::nullopt;
			}
			const auto lineEnd = aSpan.Line + 1U < aLines.size() ? aLines[aSpan.Line + 1U] - 1U : aText.size();
			const auto begin   = aLines[aSpan.Line] + aSpan.Column;
			const auto end     = begin + aSpan.Length;
			if (begin >= lineEnd || end > lineEnd) {
				return std::nullopt;
			}
			return std::pair{ begin, end };
		}

		/// \brief Whether `[aBegin, aEnd)` is exactly one whole identifier.
		[[nodiscard]] auto whole_identifier(const std::string_view aText, const std::size_t aBegin, const std::size_t aEnd) noexcept -> bool
		{
			if (!identifier_start(aText[aBegin]) || (aBegin > 0U && identifier_unit(aText[aBegin - 1U])) || (aEnd < aText.size() && identifier_unit(aText[aEnd]))) {
				return false;
			}
			return std::all_of(aText.begin() + static_cast<std::ptrdiff_t>(aBegin), aText.begin() + static_cast<std::ptrdiff_t>(aEnd), identifier_unit);
		}

		struct observed
		{
			role Role     = role::syntax_normal;
			bool Conflict = false;
		};

		/// \brief Every role the file's spans give each whole identifier, combined.
		[[nodiscard]] auto observe_names(const std::string_view aText, const token_span_list& aSpans) -> std::map<std::string_view, observed, std::less<>>
		{
			const auto lines = line_starts(aText);
			auto       names = std::map<std::string_view, observed, std::less<>>{ };
			for (const auto& span : aSpans) {
				if (!evidence(span.Role) || span.Length > maximum_code_context_name_bytes) {
					continue;
				}
				const auto range = range_of(aText, lines, span);
				if (!range || !whole_identifier(aText, range->first, range->second)) {
					continue;
				}
				const auto name  = aText.substr(range->first, range->second - range->first);
				const auto found = names.find(name);
				if (found == names.end()) {
					if (names.size() < maximum_code_context_names) {
						names.emplace(name, observed{ .Role = span.Role });
					}
					continue;
				}
				const auto kept        = combined(found->second.Role, span.Role);
				found->second.Role     = kept.value_or(found->second.Role);
				found->second.Conflict = found->second.Conflict || !kept;
			}
			return names;
		}

		/// \brief The lines of `aText` that are preprocessor directives, and
		/// the lines a trailing `\` continues them onto.
		[[nodiscard]] auto directive_lines(const std::string_view aText, const std::vector<std::size_t>& aLines, const std::vector<role>& aRoles) -> std::vector<bool>
		{
			auto directives = std::vector<bool>(aLines.size(), false);
			auto continued  = false;
			for (auto line = std::size_t{ 0U }; line < aLines.size(); ++line) {
				const auto begin = aLines[line];
				const auto end   = line + 1U < aLines.size() ? aLines[line + 1U] - 1U : aText.size();
				auto       at    = begin;
				while (at < end && (aText[at] == ' ' || aText[at] == '\t')) {
					++at;
				}
				const auto opened = at < end && aText[at] == '#' && aRoles[at] == role::syntax_preprocessor;
				directives[line]  = continued || opened;
				auto last         = end;
				if (last > begin && aText[last - 1U] == '\r') {
					--last;
				}
				continued = directives[line] && last > begin && aText[last - 1U] == '\\';
			}
			return directives;
		}

		[[nodiscard]] auto blank(const char aByte) noexcept -> bool { return aByte == ' ' || aByte == '\t'; }

		/// Member access just before `aAt`, past blanks: `.` (not `..`) or `->`.
		[[nodiscard]] auto after_access(const std::string_view aText, std::size_t aAt) noexcept -> bool
		{
			while (aAt > 0U && blank(aText[aAt - 1U])) {
				--aAt;
			}
			if (aAt >= 1U && aText[aAt - 1U] == '.') {
				return aAt < 2U || aText[aAt - 2U] != '.';
			}
			return aAt >= 2U && aText[aAt - 2U] == '-' && aText[aAt - 1U] == '>';
		}

		/// A scope separator just after `aAt`, past blanks.
		[[nodiscard]] auto before_scope(const std::string_view aText, std::size_t aAt) noexcept -> bool
		{
			while (aAt < aText.size() && blank(aText[aAt])) {
				++aAt;
			}
			return aText.substr(aAt, 2U) == "::";
		}

		/// \brief Whether the place of the identifier at `[aBegin, aEnd)` can hold `aRole`.
		[[nodiscard]] auto fits(const std::string_view aText, const std::size_t aBegin, const std::size_t aEnd, const role aRole) noexcept -> bool
		{
			if (after_access(aText, aBegin)) {
				return aRole == role::syntax_function;
			}
			if (before_scope(aText, aEnd)) {
				return aRole == role::syntax_namespace || aRole == role::syntax_type;
			}
			return true;
		}

		/// \brief Paints every identifier of `aText` the context names; whether any changed.
		[[nodiscard]] auto paint_names(const std::string_view aText, const std::vector<std::size_t>& aLines, std::vector<role>& aRoles, const code_context_roles& aContext) -> bool
		{
			const auto directives = directive_lines(aText, aLines, aRoles);
			auto       painted    = false;
			auto       line       = std::size_t{ 0U };
			for (auto at = std::size_t{ 0U }; at < aText.size();) {
				if (aText[at] == '\n') {
					++line;
					++at;
					continue;
				}
				if (!identifier_start(aText[at]) || (at > 0U && identifier_unit(aText[at - 1U]))) {
					++at;
					continue;
				}
				auto end = at + 1U;
				while (end < aText.size() && identifier_unit(aText[end])) {
					++end;
				}
				const auto own     = aRoles[at];
				const auto uniform = std::all_of(aRoles.begin() + static_cast<std::ptrdiff_t>(at), aRoles.begin() + static_cast<std::ptrdiff_t>(end), [own](const role aRole) { return aRole == own; });
				if (!directives[line] && uniform && replaceable(own)) {
					const auto borrowed = code_context_role(aContext, aText.substr(at, end - at));
					if (borrowed && *borrowed != own && fits(aText, at, end, *borrowed)) {
						std::fill(aRoles.begin() + static_cast<std::ptrdiff_t>(at), aRoles.begin() + static_cast<std::ptrdiff_t>(end), *borrowed);
						painted = true;
					}
				}
				at = end;
			}
			return painted;
		}

		/// \brief The per-byte roles as spans, one per run within a line; none
		/// when there would be more than `aMaximumSpans`.
		[[nodiscard]] auto spans_of(const std::string_view aText, const std::vector<role>& aRoles, const std::size_t aMaximumSpans) -> std::optional<token_span_list>
		{
			auto spans = token_span_list{ };
			auto line  = std::uint32_t{ 0U };
			auto start = std::size_t{ 0U };
			for (auto at = std::size_t{ 0U }; at < aText.size();) {
				if (aText[at] == '\n') {
					++line;
					++at;
					start = at;
					continue;
				}
				const auto selected = aRoles[at];
				auto       end      = at + 1U;
				while (end < aText.size() && aText[end] != '\n' && aRoles[end] == selected) {
					++end;
				}
				if (selected != role::syntax_normal) {
					if (spans.size() >= aMaximumSpans) {
						return std::nullopt;
					}
					spans.push_back({ .Line = line, .Column = static_cast<std::uint32_t>(at - start), .Length = static_cast<std::uint32_t>(end - at), .Role = selected });
				}
				at = end;
			}
			return spans;
		}
	} // namespace

	auto code_context_roles_of(const std::string_view aText, const token_span_list& aSpans) -> code_context_roles
	{
		auto roles = code_context_roles{ };
		for (const auto& [name, seen] : observe_names(aText, aSpans)) {
			if (!seen.Conflict && borrowable(seen.Role)) {
				roles.Names.push_back({ .Name = std::string{ name }, .Role = seen.Role });
			}
		}
		return roles;
	}

	auto code_context_role(const code_context_roles& aRoles, const std::string_view aName) -> std::optional<appearance_role>
	{
		const auto found = std::ranges::lower_bound(aRoles.Names, aName, std::less<>{ }, [](const code_context_name& aEntry) -> std::string_view { return aEntry.Name; });
		if (found == aRoles.Names.end() || found->Name != aName) {
			return std::nullopt;
		}
		return found->Role;
	}

	auto borrow_code_context_roles(const std::string_view aText, const token_span_list& aSpans, const code_context_roles& aContext, const std::size_t aMaximumSpans) -> token_span_list
	{
		if (aContext.Names.empty() || aText.empty()) {
			return aSpans;
		}
		const auto lines = line_starts(aText);
		auto       roles = std::vector<role>(aText.size(), role::syntax_normal);
		for (const auto& span : aSpans) {
			const auto range = range_of(aText, lines, span);
			if (!range) {
				// Spans this text cannot hold are not the block's: keep them as they came.
				return aSpans;
			}
			std::fill(roles.begin() + static_cast<std::ptrdiff_t>(range->first), roles.begin() + static_cast<std::ptrdiff_t>(range->second), span.Role);
		}
		if (!paint_names(aText, lines, roles, aContext)) {
			return aSpans;
		}
		return spans_of(aText, roles, aMaximumSpans).value_or(aSpans);
	}
} // namespace bpr::core
