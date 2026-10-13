#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/utf8.hpp"

#include <algorithm>
#include <utility>

namespace bpr::core {
	namespace {
		constexpr auto base64_alphabet = std::string_view{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };
		constexpr auto hex_digits      = std::string_view{ "0123456789abcdef" };

		void append_utf8(std::string& aOut, const std::uint32_t aPoint)
		{
			if (aPoint < 0x80U) {
				aOut.push_back(static_cast<char>(aPoint));
			} else if (aPoint < 0x800U) {
				aOut.push_back(static_cast<char>(0xC0U | (aPoint >> 6U)));
				aOut.push_back(static_cast<char>(0x80U | (aPoint & 0x3FU)));
			} else if (aPoint < 0x10000U) {
				aOut.push_back(static_cast<char>(0xE0U | (aPoint >> 12U)));
				aOut.push_back(static_cast<char>(0x80U | ((aPoint >> 6U) & 0x3FU)));
				aOut.push_back(static_cast<char>(0x80U | (aPoint & 0x3FU)));
			} else {
				aOut.push_back(static_cast<char>(0xF0U | (aPoint >> 18U)));
				aOut.push_back(static_cast<char>(0x80U | ((aPoint >> 12U) & 0x3FU)));
				aOut.push_back(static_cast<char>(0x80U | ((aPoint >> 6U) & 0x3FU)));
				aOut.push_back(static_cast<char>(0x80U | (aPoint & 0x3FU)));
			}
		}

		/// Length of the well-formed scalar at `aIndex`, or zero.
		/// Length of the well-formed scalar at `aIndex`, or zero ([[BPR-1434]]: utf8proc).
		auto scalar_length(const std::string_view aText, const std::size_t aIndex) noexcept -> std::size_t
		{
			auto next = aIndex;
			return utf8::decode(aText, next) ? next - aIndex : 0U;
		}

		class reader
		{
		public:
			explicit reader(const std::string_view aText) noexcept
				: Text{ aText }
			{
			}

			auto document() -> std::expected<cycle_json_value, cycle_json_error>
			{
				skip_space();
				auto value = parse_value(0U);
				if (!value) {
					return value;
				}
				skip_space();
				if (Index != Text.size()) {
					return std::unexpected(cycle_json_error::syntax);
				}
				return value;
			}

		private:
			void skip_space() noexcept
			{
				while (Index < Text.size() && (Text[Index] == ' ' || Text[Index] == '\t' || Text[Index] == '\n' || Text[Index] == '\r')) {
					++Index;
				}
			}

			auto literal(const std::string_view aWord) noexcept -> bool
			{
				if (Text.substr(Index, aWord.size()) != aWord) {
					return false;
				}
				Index += aWord.size();
				return true;
			}

			auto parse_value(const std::size_t aDepth) -> std::expected<cycle_json_value, cycle_json_error>
			{
				if (Index >= Text.size()) {
					return std::unexpected(cycle_json_error::syntax);
				}
				switch (Text[Index]) {
				case '{': return parse_object(aDepth + 1U);
				case '[': return parse_array(aDepth + 1U);
				case '"': {
					auto text = parse_string();
					if (!text) {
						return std::unexpected(text.error());
					}
					return cycle_json_value{ std::move(*text) };
				}
				case 't': return literal("true") ? std::expected<cycle_json_value, cycle_json_error>{ cycle_json_value{ true } } : std::unexpected(cycle_json_error::syntax);
				case 'f': return literal("false") ? std::expected<cycle_json_value, cycle_json_error>{ cycle_json_value{ false } } : std::unexpected(cycle_json_error::syntax);
				case 'n': return literal("null") ? std::expected<cycle_json_value, cycle_json_error>{ cycle_json_value{ } } : std::unexpected(cycle_json_error::syntax);
				default:  return parse_number();
				}
			}

			auto parse_number() -> std::expected<cycle_json_value, cycle_json_error>
			{
				const auto negative = Text[Index] == '-';
				if (negative) {
					++Index;
				}
				const auto first = Index;
				while (Index < Text.size() && Text[Index] >= '0' && Text[Index] <= '9') {
					++Index;
				}
				const auto digits = Text.substr(first, Index - first);
				if (digits.empty() || (digits.size() > 1U && digits.front() == '0')) {
					return std::unexpected(cycle_json_error::syntax);
				}
				if (Index < Text.size() && (Text[Index] == '.' || Text[Index] == 'e' || Text[Index] == 'E')) {
					return std::unexpected(cycle_json_error::number);
				}
				if (digits.size() > 16U) {
					return std::unexpected(cycle_json_error::number);
				}
				auto value = std::int64_t{ 0 };
				for (const auto digit : digits) {
					value = value * 10 + (digit - '0');
				}
				if (value > maximum_cycle_json_integer) {
					return std::unexpected(cycle_json_error::number);
				}
				return cycle_json_value{ negative ? -value : value };
			}

			auto hex4() noexcept -> std::optional<std::uint32_t>
			{
				if (Text.size() - Index < 4U) {
					return std::nullopt;
				}
				auto value = std::uint32_t{ 0 };
				for (std::size_t offset = 0; offset < 4U; ++offset) {
					const auto symbol = Text[Index + offset];
					auto       digit  = std::uint32_t{ 0 };
					if (symbol >= '0' && symbol <= '9') {
						digit = static_cast<std::uint32_t>(symbol - '0');
					} else if (symbol >= 'a' && symbol <= 'f') {
						digit = static_cast<std::uint32_t>(symbol - 'a' + 10);
					} else if (symbol >= 'A' && symbol <= 'F') {
						digit = static_cast<std::uint32_t>(symbol - 'A' + 10);
					} else {
						return std::nullopt;
					}
					value = (value << 4U) | digit;
				}
				Index += 4U;
				return value;
			}

			auto escaped_scalar() -> std::expected<std::uint32_t, cycle_json_error>
			{
				const auto unit = hex4();
				if (!unit) {
					return std::unexpected(cycle_json_error::syntax);
				}
				if (*unit >= 0xDC00U && *unit <= 0xDFFFU) {
					return std::unexpected(cycle_json_error::encoding);
				}
				if (*unit < 0xD800U || *unit > 0xDBFFU) {
					return *unit;
				}
				if (!literal("\\u")) {
					return std::unexpected(cycle_json_error::encoding);
				}
				const auto low = hex4();
				if (!low) {
					return std::unexpected(cycle_json_error::syntax);
				}
				if (*low < 0xDC00U || *low > 0xDFFFU) {
					return std::unexpected(cycle_json_error::encoding);
				}
				return 0x10000U + ((*unit - 0xD800U) << 10U) + (*low - 0xDC00U);
			}

			auto parse_string() -> std::expected<std::string, cycle_json_error>
			{
				++Index;
				auto result = std::string{ };
				while (Index < Text.size()) {
					const auto symbol = static_cast<unsigned char>(Text[Index]);
					if (symbol == '"') {
						++Index;
						return result;
					}
					if (symbol < 0x20U) {
						return std::unexpected(cycle_json_error::syntax);
					}
					if (symbol != '\\') {
						const auto length = scalar_length(Text, Index);
						if (length == 0U) {
							return std::unexpected(cycle_json_error::encoding);
						}
						result.append(Text.substr(Index, length));
						Index += length;
						continue;
					}
					if (++Index >= Text.size()) {
						break;
					}
					const auto escape = Text[Index++];
					switch (escape) {
					case '"':  result.push_back('"'); break;
					case '\\': result.push_back('\\'); break;
					case '/':  result.push_back('/'); break;
					case 'b':  result.push_back('\b'); break;
					case 'f':  result.push_back('\f'); break;
					case 'n':  result.push_back('\n'); break;
					case 'r':  result.push_back('\r'); break;
					case 't':  result.push_back('\t'); break;
					case 'u':  {
						const auto point = escaped_scalar();
						if (!point) {
							return std::unexpected(point.error());
						}
						append_utf8(result, *point);
						break;
					}
					default: return std::unexpected(cycle_json_error::syntax);
					}
				}
				return std::unexpected(cycle_json_error::syntax);
			}

			auto parse_array(const std::size_t aDepth) -> std::expected<cycle_json_value, cycle_json_error>
			{
				if (aDepth > maximum_cycle_json_depth) {
					return std::unexpected(cycle_json_error::depth);
				}
				++Index;
				auto items = cycle_json_value::array{ };
				skip_space();
				if (literal("]")) {
					return cycle_json_value{ std::move(items) };
				}
				while (true) {
					skip_space();
					auto item = parse_value(aDepth);
					if (!item) {
						return item;
					}
					items.push_back(std::move(*item));
					skip_space();
					if (literal("]")) {
						return cycle_json_value{ std::move(items) };
					}
					if (!literal(",")) {
						return std::unexpected(cycle_json_error::syntax);
					}
				}
			}

			auto parse_object(const std::size_t aDepth) -> std::expected<cycle_json_value, cycle_json_error>
			{
				if (aDepth > maximum_cycle_json_depth) {
					return std::unexpected(cycle_json_error::depth);
				}
				++Index;
				auto members = cycle_json_value::object{ };
				skip_space();
				if (literal("}")) {
					return cycle_json_value{ std::move(members) };
				}
				while (true) {
					skip_space();
					if (Index >= Text.size() || Text[Index] != '"') {
						return std::unexpected(cycle_json_error::syntax);
					}
					auto key = parse_string();
					if (!key) {
						return std::unexpected(key.error());
					}
					skip_space();
					if (!literal(":")) {
						return std::unexpected(cycle_json_error::syntax);
					}
					skip_space();
					auto value = parse_value(aDepth);
					if (!value) {
						return value;
					}
					members.push_back({ .Key = std::move(*key), .Value = std::move(*value) });
					skip_space();
					if (literal("}")) {
						break;
					}
					if (!literal(",")) {
						return std::unexpected(cycle_json_error::syntax);
					}
				}
				std::ranges::sort(members, { }, &cycle_json_member::Key);
				const auto repeated = std::ranges::adjacent_find(members, { }, &cycle_json_member::Key);
				if (repeated != members.end()) {
					return std::unexpected(cycle_json_error::duplicate_key);
				}
				return cycle_json_value{ std::move(members) };
			}

			std::string_view Text;
			std::size_t      Index = 0;
		};

		auto write_string(std::string& aOut, const std::string_view aText) -> bool
		{
			if (!valid_cycle_utf8(aText)) {
				return false;
			}
			aOut.push_back('"');
			for (const auto symbol : aText) {
				const auto byte = static_cast<unsigned char>(symbol);
				switch (symbol) {
				case '"':  aOut.append("\\\""); break;
				case '\\': aOut.append("\\\\"); break;
				case '\b': aOut.append("\\b"); break;
				case '\f': aOut.append("\\f"); break;
				case '\n': aOut.append("\\n"); break;
				case '\r': aOut.append("\\r"); break;
				case '\t': aOut.append("\\t"); break;
				default:
					if (byte < 0x20U) {
						aOut.append("\\u00");
						aOut.push_back(hex_digits[byte >> 4U]);
						aOut.push_back(hex_digits[byte & 0x0FU]);
					} else {
						aOut.push_back(symbol);
					}
				}
			}
			aOut.push_back('"');
			return true;
		}

		auto write_value(std::string& aOut, const cycle_json_value& aValue, const std::size_t aDepth) -> bool
		{
			if (std::holds_alternative<std::monostate>(aValue.Value)) {
				aOut.append("null");
				return true;
			}
			if (const auto* flag = std::get_if<bool>(&aValue.Value)) {
				aOut.append(*flag ? "true" : "false");
				return true;
			}
			if (const auto* number = std::get_if<std::int64_t>(&aValue.Value)) {
				if (*number > maximum_cycle_json_integer || *number < -maximum_cycle_json_integer) {
					return false;
				}
				aOut.append(std::to_string(*number));
				return true;
			}
			if (const auto* text = std::get_if<std::string>(&aValue.Value)) {
				return write_string(aOut, *text);
			}
			if (aDepth >= maximum_cycle_json_depth) {
				return false;
			}
			if (const auto* items = std::get_if<cycle_json_value::array>(&aValue.Value)) {
				aOut.push_back('[');
				for (std::size_t index = 0; index < items->size(); ++index) {
					if (index != 0U) {
						aOut.push_back(',');
					}
					if (!write_value(aOut, (*items)[index], aDepth + 1U)) {
						return false;
					}
				}
				aOut.push_back(']');
				return true;
			}
			const auto& members = std::get<cycle_json_value::object>(aValue.Value);
			auto        order   = std::vector<const cycle_json_member*>{ };
			order.reserve(members.size());
			for (const auto& member : members) {
				order.push_back(&member);
			}
			std::ranges::sort(order, [](const auto* aLeft, const auto* aRight) { return aLeft->Key < aRight->Key; });
			aOut.push_back('{');
			for (std::size_t index = 0; index < order.size(); ++index) {
				if (index != 0U) {
					if (order[index - 1U]->Key == order[index]->Key) {
						return false;
					}
					aOut.push_back(',');
				}
				if (!write_string(aOut, order[index]->Key)) {
					return false;
				}
				aOut.push_back(':');
				if (!write_value(aOut, order[index]->Value, aDepth + 1U)) {
					return false;
				}
			}
			aOut.push_back('}');
			return true;
		}
	}

	auto stable_name(const cycle_json_error aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_json_error::syntax:        return "syntax";
		case cycle_json_error::encoding:      return "encoding";
		case cycle_json_error::duplicate_key: return "duplicate_key";
		case cycle_json_error::depth:         return "depth";
		case cycle_json_error::number:        return "number";
		case cycle_json_error::too_large:     return "too_large";
		}
		return "syntax";
	}

	auto parse_cycle_json(const std::string_view aText, const std::size_t aMaximumBytes) -> std::expected<cycle_json_value, cycle_json_error>
	{
		if (aText.size() > aMaximumBytes) {
			return std::unexpected(cycle_json_error::too_large);
		}
		return reader{ aText }.document();
	}

	auto canonical_cycle_json(const cycle_json_value& aValue) -> std::optional<std::string>
	{
		auto out = std::string{ };
		if (!write_value(out, aValue, 0U)) {
			return std::nullopt;
		}
		return out;
	}

	auto find_member(const cycle_json_value::object& aObject, const std::string_view aKey) noexcept -> const cycle_json_value*
	{
		const auto found = std::ranges::find(aObject, aKey, &cycle_json_member::Key);
		return found == aObject.end() ? nullptr : &found->Value;
	}

	auto has_exactly(const cycle_json_value::object& aObject, const std::span<const std::string_view> aKeys) noexcept -> bool
	{
		return aObject.size() == aKeys.size()
			&& std::ranges::all_of(aKeys, [&](const std::string_view aKey) { return std::ranges::count(aObject, aKey, &cycle_json_member::Key) == 1; });
	}

	auto valid_cycle_utf8(const std::string_view aText) noexcept -> bool
	{
		for (std::size_t index = 0; index < aText.size();) {
			const auto length = scalar_length(aText, index);
			if (length == 0U) {
				return false;
			}
			index += length;
		}
		return true;
	}

	auto encode_cycle_base64(const std::string_view aBytes) -> std::string
	{
		auto result = std::string{ };
		result.reserve((aBytes.size() + 2U) / 3U * 4U);
		for (std::size_t index = 0; index < aBytes.size(); index += 3U) {
			const auto remaining = aBytes.size() - index;
			auto       group     = static_cast<std::uint32_t>(static_cast<unsigned char>(aBytes[index])) << 16U;
			if (remaining > 1U) {
				group |= static_cast<std::uint32_t>(static_cast<unsigned char>(aBytes[index + 1U])) << 8U;
			}
			if (remaining > 2U) {
				group |= static_cast<std::uint32_t>(static_cast<unsigned char>(aBytes[index + 2U]));
			}
			result.push_back(base64_alphabet[(group >> 18U) & 0x3FU]);
			result.push_back(base64_alphabet[(group >> 12U) & 0x3FU]);
			result.push_back(remaining > 1U ? base64_alphabet[(group >> 6U) & 0x3FU] : '=');
			result.push_back(remaining > 2U ? base64_alphabet[group & 0x3FU] : '=');
		}
		return result;
	}

	auto decode_cycle_base64(const std::string_view aText) -> std::optional<std::string>
	{
		if (aText.size() % 4U != 0U) {
			return std::nullopt;
		}
		auto result = std::string{ };
		result.reserve(aText.size() / 4U * 3U);
		for (std::size_t index = 0; index < aText.size(); index += 4U) {
			auto group   = std::uint32_t{ 0 };
			auto padding = 0U;
			for (std::size_t offset = 0; offset < 4U; ++offset) {
				const auto symbol = aText[index + offset];
				if (symbol == '=' && index + 4U == aText.size() && offset >= 2U) {
					++padding;
					group <<= 6U;
					continue;
				}
				const auto value = base64_alphabet.find(symbol);
				if (value == std::string_view::npos || padding != 0U) {
					return std::nullopt;
				}
				group = (group << 6U) | static_cast<std::uint32_t>(value);
			}
			// Canonical form: the bits a padding symbol stands for are zero.
			if ((padding == 1U && (group & 0xFFU) != 0U) || (padding == 2U && (group & 0xFFFFU) != 0U)) {
				return std::nullopt;
			}
			result.push_back(static_cast<char>((group >> 16U) & 0xFFU));
			if (padding < 2U) {
				result.push_back(static_cast<char>((group >> 8U) & 0xFFU));
			}
			if (padding < 1U) {
				result.push_back(static_cast<char>(group & 0xFFU));
			}
		}
		return result;
	}

	auto cycle_hex(const std::span<const std::uint8_t> aBytes) -> std::string
	{
		auto result = std::string{ };
		result.reserve(aBytes.size() * 2U);
		for (const auto byte : aBytes) {
			result.push_back(hex_digits[byte >> 4U]);
			result.push_back(hex_digits[byte & 0x0FU]);
		}
		return result;
	}

	auto parse_cycle_sha256(const std::string_view aText) -> std::optional<std::array<std::uint8_t, 32>>
	{
		if (aText.size() != 64U) {
			return std::nullopt;
		}
		auto result = std::array<std::uint8_t, 32>{ };
		for (std::size_t index = 0; index < result.size(); ++index) {
			const auto high = hex_digits.find(aText[index * 2U]);
			const auto low  = hex_digits.find(aText[index * 2U + 1U]);
			if (high == std::string_view::npos || low == std::string_view::npos) {
				return std::nullopt;
			}
			result[index] = static_cast<std::uint8_t>((high << 4U) | low);
		}
		return result;
	}
}
