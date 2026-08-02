#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_GRAPHQL_OPERATION_POLICY_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_GRAPHQL_OPERATION_POLICY_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bpr::adapters::github::detail {
	enum class graphql_operation_policy_result : std::uint8_t {
		query_only,
		write_operation,
		multiple_operations,
		malformed,
		response_limit,
	};

	inline constexpr std::size_t maximum_graphql_operation_document_bytes = 16U * 1024U;
	inline constexpr std::size_t maximum_graphql_operation_nesting        = 32U;
	inline constexpr std::size_t maximum_graphql_document_definitions     = 64U;

	class graphql_operation_scanner final
	{
	public:
		explicit constexpr graphql_operation_scanner(
			const std::string_view aDocument) noexcept
			: Document{ aDocument }
		{
		}

		[[nodiscard]] constexpr auto inspect() noexcept
			-> graphql_operation_policy_result
		{
			if (Document.empty()) {
				return graphql_operation_policy_result::malformed;
			}
			if (Document.size() > maximum_graphql_operation_document_bytes) {
				return graphql_operation_policy_result::response_limit;
			}

			skip_ignored();
			while (Position < Document.size()) {
				const auto operation = scan_name();
				if (operation == "mutation" || operation == "subscription") {
					return graphql_operation_policy_result::write_operation;
				}
				if (operation != "query" && operation != "fragment") {
					return graphql_operation_policy_result::malformed;
				}
				if (Definitions == maximum_graphql_document_definitions) {
					return graphql_operation_policy_result::response_limit;
				}
				++Definitions;

				if (operation == "query" && ++Queries != 1U) {
					return graphql_operation_policy_result::multiple_operations;
				}

				const auto definition = scan_definition();
				if (definition != graphql_operation_policy_result::query_only) {
					return definition;
				}
				skip_ignored();
			}
			return Queries == 1U
				? graphql_operation_policy_result::query_only
				: graphql_operation_policy_result::malformed;
		}

	private:
		[[nodiscard]] static constexpr auto name_start(
			const char aValue) noexcept -> bool
		{
			return (aValue >= 'A' && aValue <= 'Z')
				|| (aValue >= 'a' && aValue <= 'z') || aValue == '_';
		}

		[[nodiscard]] static constexpr auto name_continue(
			const char aValue) noexcept -> bool
		{
			return name_start(aValue) || digit(aValue);
		}

		[[nodiscard]] static constexpr auto digit(const char aValue) noexcept
			-> bool
		{
			return aValue >= '0' && aValue <= '9';
		}

		[[nodiscard]] static constexpr auto whitespace(
			const char aValue) noexcept -> bool
		{
			return aValue == ' ' || aValue == '\t' || aValue == '\r'
				|| aValue == '\n' || aValue == ',';
		}

		constexpr void skip_ignored() noexcept
		{
			while (Position < Document.size()) {
				if (whitespace(Document[Position])) {
					++Position;
					continue;
				}
				if (Document[Position] != '#') {
					return;
				}
				while (
					Position < Document.size()
					&& Document[Position] != '\r'
					&& Document[Position] != '\n') {
					++Position;
				}
			}
		}

		[[nodiscard]] constexpr auto scan_name() noexcept -> std::string_view
		{
			if (
				Position >= Document.size()
				|| !name_start(Document[Position])) {
				return { };
			}
			const auto start = Position++;
			while (
				Position < Document.size()
				&& name_continue(Document[Position])) {
				++Position;
			}
			return Document.substr(start, Position - start);
		}

		[[nodiscard]] constexpr auto scan_string() noexcept -> bool
		{
			const auto block = Document.size() - Position >= 3U
				&& Document.substr(Position, 3U) == "\"\"\"";
			Position += block ? 3U : 1U;
			while (Position < Document.size()) {
				if (
					block && Document.size() - Position >= 4U
					&& Document.substr(Position, 4U) == "\\\"\"\"") {
					Position += 4U;
					continue;
				}
				if (
					block && Document.size() - Position >= 3U
					&& Document.substr(Position, 3U) == "\"\"\"") {
					Position += 3U;
					return true;
				}

				const auto value = Document[Position++];
				if (!block && value == '"') {
					return true;
				}
				if (!block && value == '\\') {
					if (Position == Document.size()) {
						return false;
					}
					++Position;
				} else if (!block && (value == '\r' || value == '\n')) {
					return false;
				}
			}
			return false;
		}

		[[nodiscard]] constexpr auto scan_number() noexcept -> bool
		{
			if (Document[Position] == '-') {
				++Position;
				if (Position == Document.size()) {
					return false;
				}
			}

			if (Document[Position] == '0') {
				++Position;
				if (Position < Document.size() && digit(Document[Position])) {
					return false;
				}
			} else if (
				Document[Position] >= '1' && Document[Position] <= '9') {
				do {
					++Position;
				} while (
					Position < Document.size() && digit(Document[Position]));
			} else {
				return false;
			}

			if (Position < Document.size() && Document[Position] == '.') {
				++Position;
				if (Position == Document.size() || !digit(Document[Position])) {
					return false;
				}
				while (
					Position < Document.size() && digit(Document[Position])) {
					++Position;
				}
			}

			if (
				Position < Document.size()
				&& (Document[Position] == 'e' || Document[Position] == 'E')) {
				++Position;
				if (
					Position < Document.size()
					&& (Document[Position] == '+'
						|| Document[Position] == '-')) {
					++Position;
				}
				if (Position == Document.size() || !digit(Document[Position])) {
					return false;
				}
				while (
					Position < Document.size() && digit(Document[Position])) {
					++Position;
				}
			}

			return Position == Document.size()
				|| (Document[Position] != '.'
					&& !name_start(Document[Position]));
		}

		[[nodiscard]] static constexpr auto try_increase_depth(
			std::size_t&      aDepth,
			const std::size_t aBraceDepth,
			const std::size_t aParenthesisDepth,
			const std::size_t aBracketDepth) noexcept -> bool
		{
			if (
				aBraceDepth + aParenthesisDepth + aBracketDepth
				>= maximum_graphql_operation_nesting) {
				return false;
			}
			++aDepth;
			return true;
		}

		[[nodiscard]] constexpr auto scan_definition() noexcept
			-> graphql_operation_policy_result
		{
			std::size_t braceDepth       = 0U;
			std::size_t parenthesisDepth = 0U;
			std::size_t bracketDepth     = 0U;
			bool        selectionStarted = false;
			bool        selectionHasName = false;

			while (Position < Document.size()) {
				skip_ignored();
				if (Position == Document.size()) {
					return graphql_operation_policy_result::malformed;
				}

				const auto value = Document[Position];
				if (value == '"') {
					if (!scan_string()) {
						return graphql_operation_policy_result::malformed;
					}
					continue;
				}
				if (name_start(value)) {
					static_cast<void>(scan_name());
					selectionHasName = selectionHasName
						|| (selectionStarted && braceDepth != 0U);
					continue;
				}
				if (value == '-' || (value >= '0' && value <= '9')) {
					if (!scan_number()) {
						return graphql_operation_policy_result::malformed;
					}
					continue;
				}
				if (value == '.') {
					if (
						Document.size() - Position < 3U
						|| Document.substr(Position, 3U) != "...") {
						return graphql_operation_policy_result::malformed;
					}
					Position += 3U;
					continue;
				}

				++Position;
				switch (value) {
				case '{':
					if (
						!try_increase_depth(
							braceDepth,
							braceDepth,
							parenthesisDepth,
							bracketDepth)) {
						return graphql_operation_policy_result::response_limit;
					}
					if (
						!selectionStarted && braceDepth == 1U
						&& parenthesisDepth == 0U && bracketDepth == 0U) {
						selectionStarted = true;
					}
					break;
				case '}':
					if (braceDepth == 0U) {
						return graphql_operation_policy_result::malformed;
					}
					--braceDepth;
					if (selectionStarted && braceDepth == 0U) {
						return parenthesisDepth == 0U && bracketDepth == 0U
								&& selectionHasName
							? graphql_operation_policy_result::query_only
							: graphql_operation_policy_result::malformed;
					}
					break;
				case '(':
					if (
						!try_increase_depth(
							parenthesisDepth,
							braceDepth,
							parenthesisDepth,
							bracketDepth)) {
						return graphql_operation_policy_result::response_limit;
					}
					break;
				case ')':
					if (parenthesisDepth == 0U) {
						return graphql_operation_policy_result::malformed;
					}
					--parenthesisDepth;
					break;
				case '[':
					if (
						!try_increase_depth(
							bracketDepth,
							braceDepth,
							parenthesisDepth,
							bracketDepth)) {
						return graphql_operation_policy_result::response_limit;
					}
					break;
				case ']':
					if (bracketDepth == 0U) {
						return graphql_operation_policy_result::malformed;
					}
					--bracketDepth;
					break;
				case '!':
				case '$':
				case '&':
				case ':':
				case '=':
				case '@':
				case '|':
					break;
				default:
					return graphql_operation_policy_result::malformed;
				}
			}
			return graphql_operation_policy_result::malformed;
		}

		std::string_view Document;
		std::size_t      Position    = 0U;
		std::size_t      Definitions = 0U;
		std::size_t      Queries     = 0U;
	};

	[[nodiscard]] constexpr auto inspect_graphql_operation_policy(
		const std::string_view aDocument) noexcept
		-> graphql_operation_policy_result
	{
		return graphql_operation_scanner{ aDocument }.inspect();
	}
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_GRAPHQL_OPERATION_POLICY_HPP
