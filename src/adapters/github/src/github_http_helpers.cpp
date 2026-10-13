#include "github_http_helpers.hpp"

#include <QByteArray>

#include <algorithm>

namespace bpr::adapters::github::detail {
	namespace {
		[[nodiscard]] auto token_character(const char aCharacter) noexcept -> bool
		{
			const auto value = static_cast<unsigned char>(aCharacter);
			return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9')
				|| std::string_view{ "!#$%&'*+-.^_`|~" }.find(aCharacter) != std::string_view::npos;
		}

		[[nodiscard]] auto ascii_lower(const char aCharacter) noexcept -> char
		{
			return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter - 'A' + 'a') : aCharacter;
		}

		[[nodiscard]] auto equal_fold(const std::string_view aText, const std::string_view aLower) noexcept -> bool
		{
			return aText.size() == aLower.size()
				&& std::ranges::equal(aText, aLower, [](const char aLeft, const char aRight) { return ascii_lower(aLeft) == aRight; });
		}

		[[nodiscard]] auto trim_ows(std::string_view aText) noexcept -> std::string_view
		{
			while (!aText.empty() && (aText.front() == ' ' || aText.front() == '\t')) {
				aText.remove_prefix(1U);
			}
			while (!aText.empty() && (aText.back() == ' ' || aText.back() == '\t')) {
				aText.remove_suffix(1U);
			}
			return aText;
		}

		class link_reader
		{
		public:
			explicit link_reader(const std::string_view aText) noexcept
				: Text{ aText }
			{
			}

			[[nodiscard]] auto read() -> std::optional<std::vector<link_value>>
			{
				auto links = std::vector<link_value>{ };
				while (true) {
					if (links.size() == maximum_link_values) {
						return std::nullopt;
					}
					auto link = read_link();
					if (!link) {
						return std::nullopt;
					}
					links.push_back(std::move(*link));
					if (Position == Text.size()) {
						return links;
					}
					// read_link stops only at the end or at a comma.
					++Position;
				}
			}

		private:
			void skip_whitespace() noexcept
			{
				while (Position < Text.size() && (Text[Position] == ' ' || Text[Position] == '\t')) {
					++Position;
				}
			}

			[[nodiscard]] auto read_link() -> std::optional<link_value>
			{
				skip_whitespace();
				if (Position == Text.size() || Text[Position] != '<') {
					return std::nullopt;
				}
				const auto targetEnd = Text.find('>', Position + 1U);
				if (targetEnd == std::string_view::npos || targetEnd == Position + 1U) {
					return std::nullopt;
				}
				auto link   = link_value{ };
				link.Target = Text.substr(Position + 1U, targetEnd - Position - 1U);
				if (!std::ranges::all_of(link.Target, [](const char aCharacter) {
						const auto value = static_cast<unsigned char>(aCharacter);
						return value > 0x20U && value < 0x7fU && aCharacter != '<' && aCharacter != '"';
					})) {
					return std::nullopt;
				}
				Position = targetEnd + 1U;
				while (true) {
					skip_whitespace();
					if (Position == Text.size() || Text[Position] == ',') {
						return link;
					}
					if (Text[Position] != ';') {
						return std::nullopt;
					}
					++Position;
					auto parameter = read_parameter();
					if (!parameter) {
						return std::nullopt;
					}
					link.Parameters.push_back(*parameter);
				}
			}

			[[nodiscard]] auto read_parameter() -> std::optional<link_parameter>
			{
				skip_whitespace();
				const auto nameBegin = Position;
				while (Position < Text.size() && token_character(Text[Position])) {
					++Position;
				}
				if (nameBegin == Position) {
					return std::nullopt;
				}
				auto parameter = link_parameter{ .Name = Text.substr(nameBegin, Position - nameBegin) };
				skip_whitespace();
				if (Position == Text.size() || Text[Position] != '=') {
					return std::nullopt;
				}
				++Position;
				skip_whitespace();
				parameter.Quoted = Position < Text.size() && Text[Position] == '"';
				if (parameter.Quoted) {
					++Position;
				}
				const auto valueBegin = Position;
				if (parameter.Quoted) {
					while (Position < Text.size() && Text[Position] != '"') {
						const auto value = static_cast<unsigned char>(Text[Position]);
						if (value < 0x20U || value == 0x7fU || Text[Position] == '\\') {
							return std::nullopt;
						}
						++Position;
					}
					if (Position == Text.size()) {
						return std::nullopt;
					}
				} else {
					while (Position < Text.size() && token_character(Text[Position])) {
						++Position;
					}
				}
				if (valueBegin == Position) {
					return std::nullopt;
				}
				parameter.Value = Text.substr(valueBegin, Position - valueBegin);
				if (parameter.Quoted) {
					++Position;
				}
				return parameter;
			}

			std::string_view Text;
			std::size_t      Position = 0U;
		};
	}

	auto parse_link_header(const std::string_view aValue) -> std::optional<std::vector<link_value>>
	{
		if (aValue.empty() || aValue.size() > maximum_link_header_bytes) {
			return std::nullopt;
		}
		return link_reader{ aValue }.read();
	}

	auto link_relations(const link_value& aLink) -> std::optional<std::vector<std::string_view>>
	{
		const link_parameter* relation = nullptr;
		for (const auto& parameter : aLink.Parameters) {
			if (parameter.Name != "rel") {
				continue;
			}
			if (relation != nullptr) {
				return std::nullopt;
			}
			relation = &parameter;
		}
		if (relation == nullptr) {
			return std::nullopt;
		}
		auto relations = std::vector<std::string_view>{ };
		auto remaining = relation->Value;
		while (true) {
			const auto separator = remaining.find(' ');
			const auto item      = remaining.substr(0U, separator);
			if (item.empty() || item.find('\t') != std::string_view::npos) {
				return std::nullopt;
			}
			relations.push_back(item);
			if (separator == std::string_view::npos) {
				return relations;
			}
			remaining.remove_prefix(separator + 1U);
		}
	}

	auto pagination_relation(const std::string_view aRelation) noexcept -> bool
	{
		return aRelation == "next" || aRelation == "prev" || aRelation == "first" || aRelation == "last";
	}

	auto link_completeness(const std::optional<std::string>& aLink) -> std::optional<page_completeness>
	{
		if (!aLink.has_value()) {
			return page_completeness::complete;
		}
		const auto links = parse_link_header(*aLink);
		if (!links) {
			return std::nullopt;
		}
		auto hasNext = false;
		for (const auto& link : *links) {
			const auto relations = link_relations(link);
			if (!relations || !std::ranges::all_of(*relations, pagination_relation)) {
				return std::nullopt;
			}
			hasNext = hasNext || std::ranges::find(*relations, "next") != relations->end();
		}
		return hasNext ? page_completeness::has_next_page : page_completeness::complete;
	}

	auto json_media_type(const std::string_view aValue) noexcept -> bool
	{
		if (aValue.size() > 256U) {
			return false;
		}
		const auto separator = aValue.find(';');
		if (!equal_fold(trim_ows(aValue.substr(0U, separator)), "application/json")) {
			return false;
		}
		return separator == std::string_view::npos || equal_fold(trim_ows(aValue.substr(separator + 1U)), "charset=utf-8");
	}

	auto percent_encode(const std::string_view aBytes, const std::string_view aKeep) -> std::string
	{
		// The byte form: QString's would replace invalid UTF-8 before encoding.
		const auto bytes = QByteArray::fromRawData(aBytes.data(), static_cast<qsizetype>(aBytes.size()));
		const auto keep  = QByteArray::fromRawData(aKeep.data(), static_cast<qsizetype>(aKeep.size()));
		return bytes.toPercentEncoding(keep).toStdString();
	}
}
