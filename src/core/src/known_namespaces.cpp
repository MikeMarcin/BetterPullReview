#include "bpr/core/known_namespaces.hpp"

#include <algorithm>
#include <array>

namespace bpr::core {
	namespace {
		/// \brief ISO C++23's keywords and alternative tokens.
		/// \details None can name a namespace. Contextual identifiers such as
		/// `final`, `override` and `module` can, so they are absent.
		constexpr auto Keywords = std::to_array<std::string_view>({ "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break",
																	"case", "catch", "char", "char16_t", "char32_t", "char8_t", "class", "co_await", "co_return", "co_yield",
																	"compl", "concept", "const", "const_cast", "consteval", "constexpr", "constinit", "continue", "decltype", "default",
																	"delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false",
																	"float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace",
																	"new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected",
																	"public", "register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert",
																	"static_cast", "struct", "switch", "template", "this", "thread_local", "throw", "true", "try", "typedef",
																	"typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while",
																	"xor", "xor_eq" });

		[[nodiscard]] constexpr auto letter(const char aByte) noexcept -> bool
		{
			return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || aByte == '_';
		}

		[[nodiscard]] constexpr auto digit(const char aByte) noexcept -> bool
		{
			return aByte >= '0' && aByte <= '9';
		}

		[[nodiscard]] constexpr auto separator(const char aByte) noexcept -> bool
		{
			return aByte == ',' || aByte == ' ' || aByte == '\t' || aByte == '\n' || aByte == '\r' || aByte == '\f' || aByte == '\v';
		}

		[[nodiscard]] auto keyword(const std::string_view aPart) -> bool
		{
			return std::ranges::find(Keywords, aPart) != Keywords.end();
		}

		/// \brief Checks one `::`-separated part.
		[[nodiscard]] auto check_part(const std::string_view aPart) -> std::optional<known_namespace_problem>
		{
			if (aPart.empty()) {
				return known_namespace_problem::empty_part;
			}
			if (!std::ranges::all_of(aPart, [](const char aByte) { return letter(aByte) || digit(aByte); })) {
				return known_namespace_problem::invalid_character;
			}
			if (digit(aPart.front())) {
				return known_namespace_problem::leading_digit;
			}
			if (keyword(aPart)) {
				return known_namespace_problem::keyword;
			}
			return std::nullopt;
		}
	}

	auto check_known_namespace(const std::string_view aName) -> std::optional<known_namespace_problem>
	{
		if (aName.size() > maximum_known_namespace_bytes) {
			return known_namespace_problem::too_long;
		}
		// A lone `:` is a character no part may hold, not half a separator.
		auto rest    = aName;
		auto problem = std::optional<known_namespace_problem>{ };
		while (!problem) {
			const auto split = rest.find("::");
			problem          = check_part(rest.substr(0U, split));
			if (split == std::string_view::npos) {
				break;
			}
			rest.remove_prefix(split + 2U);
		}
		return problem;
	}

	auto validate_known_namespaces(const known_namespaces& aNamespaces) -> std::vector<known_namespace_rejection>
	{
		auto rejections = std::vector<known_namespace_rejection>{ };
		for (auto index = std::size_t{ 0U }; index < aNamespaces.Names.size(); ++index) {
			const auto& name    = aNamespaces.Names[index];
			auto        problem = check_known_namespace(name);
			if (!problem && std::ranges::find(aNamespaces.Names.begin(), aNamespaces.Names.begin() + static_cast<std::ptrdiff_t>(index), name) != aNamespaces.Names.begin() + static_cast<std::ptrdiff_t>(index)) {
				problem = known_namespace_problem::duplicate;
			}
			if (!problem && index >= maximum_known_namespaces) {
				problem = known_namespace_problem::too_many;
			}
			if (problem) {
				rejections.push_back({ .Index = index, .Problem = *problem });
			}
		}
		return rejections;
	}

	auto split_known_namespaces(const std::string_view aText) -> known_namespaces
	{
		auto result = known_namespaces{ .Names = { } };
		for (auto at = std::size_t{ 0U }; at < aText.size();) {
			if (separator(aText[at])) {
				++at;
				continue;
			}
			auto end = at;
			while (end < aText.size() && !separator(aText[end])) {
				++end;
			}
			result.Names.emplace_back(aText.substr(at, end - at));
			at = end;
		}
		return result;
	}

	auto join_known_namespaces(const known_namespaces& aNamespaces) -> std::string
	{
		auto text = std::string{ };
		for (const auto& name : aNamespaces.Names) {
			if (!text.empty()) {
				text += ", ";
			}
			text += name;
		}
		return text;
	}

	auto canonical_known_namespaces(const known_namespaces& aNamespaces) -> known_namespaces
	{
		auto result = known_namespaces{ .Names = { } };
		for (const auto& name : aNamespaces.Names) {
			if (result.Names.size() == maximum_known_namespaces) {
				break;
			}
			if (!check_known_namespace(name) && std::ranges::find(result.Names, name) == result.Names.end()) {
				result.Names.push_back(name);
			}
		}
		std::ranges::sort(result.Names);
		return result;
	}
}
