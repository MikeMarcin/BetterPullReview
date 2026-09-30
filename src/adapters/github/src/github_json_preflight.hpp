#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_JSON_PREFLIGHT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_JSON_PREFLIGHT_HPP

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <string_view>

namespace bpr::adapters::github::detail {
	/// \brief A bounded structural check of a JSON response before it is
	/// decoded: one value, well-formed strings and numbers, no duplicate or
	/// escaped object key, and fixed depth, member, element and value
	/// ceilings. It proves nothing about meaning; the decoder that follows
	/// requires the exact fields.
	struct json_preflight_limits
	{
		std::size_t Depth    = 24U;
		std::size_t Elements = 128U;
		std::size_t Values   = 4'096U;
	};

	class json_preflight final
	{
	public:
		explicit json_preflight(const std::string_view aInput, const json_preflight_limits aLimits = { }) noexcept
			: Input{ aInput }
			, Limits{ aLimits }
		{
		}

		[[nodiscard]] auto run() noexcept -> bool
		{
			whitespace();
			const auto accepted = value(0U);
			whitespace();
			return accepted && Position == Input.size();
		}

	private:
		static constexpr std::size_t maximum_object_members = 128U;

		[[nodiscard]] auto value(const std::size_t aDepth) noexcept -> bool
		{
			if (Values == Limits.Values) {
				return false;
			}
			++Values;
			whitespace();
			if (Position >= Input.size()) {
				return false;
			}
			switch (Input[Position]) {
			case '{':
				return object(aDepth);
			case '[':
				return array(aDepth);
			case '"':
				return string(nullptr, false);
			case 't':
				return literal("true");
			case 'f':
				return literal("false");
			case 'n':
				return literal("null");
			default:
				return number();
			}
		}

		[[nodiscard]] auto object(const std::size_t aDepth) noexcept -> bool
		{
			if (aDepth >= Limits.Depth || !consume('{')) {
				return false;
			}
			whitespace();
			if (consume('}')) {
				return true;
			}
			std::array<std::string_view, maximum_object_members> keys;
			std::size_t                                          count = 0U;
			while (count < keys.size()) {
				std::string_view key;
				if (!string(&key, true)
					|| std::ranges::find(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(count), key)
						!= keys.begin() + static_cast<std::ptrdiff_t>(count)) {
					return false;
				}
				keys[count++] = key;
				whitespace();
				if (!consume(':') || !value(aDepth + 1U)) {
					return false;
				}
				whitespace();
				if (consume('}')) {
					return true;
				}
				if (!consume(',')) {
					return false;
				}
				whitespace();
			}
			return false;
		}

		[[nodiscard]] auto array(const std::size_t aDepth) noexcept -> bool
		{
			if (aDepth >= Limits.Depth || !consume('[')) {
				return false;
			}
			whitespace();
			if (consume(']')) {
				return true;
			}
			for (std::size_t count = 0U; count < Limits.Elements; ++count) {
				if (!value(aDepth + 1U)) {
					return false;
				}
				whitespace();
				if (consume(']')) {
					return true;
				}
				if (!consume(',')) {
					return false;
				}
				whitespace();
			}
			return false;
		}

		[[nodiscard]] auto string(
			std::string_view* aOutput,
			const bool        aKey) noexcept -> bool
		{
			if (!consume('"')) {
				return false;
			}
			const auto start = Position;
			while (Position < Input.size()) {
				const auto character = static_cast<unsigned char>(Input[Position++]);
				if (character == '"') {
					if (aOutput != nullptr) {
						*aOutput = Input.substr(start, Position - start - 1U);
					}
					return true;
				}
				if (character < 0x20U || (aKey && character == '\\')) {
					return false;
				}
				if (character != '\\') {
					continue;
				}
				if (Position >= Input.size()) {
					return false;
				}
				const auto escaped = Input[Position++];
				if (std::string_view{ "\"\\/bfnrt" }.find(escaped) != std::string_view::npos) {
					continue;
				}
				if (escaped != 'u' || Input.size() - Position < 4U) {
					return false;
				}
				for (std::size_t digit = 0U; digit < 4U; ++digit) {
					if (std::isxdigit(static_cast<unsigned char>(Input[Position++])) == 0) {
						return false;
					}
				}
			}
			return false;
		}

		[[nodiscard]] auto number() noexcept -> bool
		{
			const auto start = Position;
			static_cast<void>(consume('-'));
			if (consume('0')) {
				if (Position < Input.size() && std::isdigit(static_cast<unsigned char>(Input[Position])) != 0) {
					return false;
				}
			} else {
				const auto digits = Position;
				while (Position < Input.size() && std::isdigit(static_cast<unsigned char>(Input[Position])) != 0) {
					++Position;
				}
				if (Position == digits) {
					return false;
				}
			}
			if (consume('.')) {
				const auto digits = Position;
				while (Position < Input.size() && std::isdigit(static_cast<unsigned char>(Input[Position])) != 0) {
					++Position;
				}
				if (Position == digits) {
					return false;
				}
			}
			if (Position < Input.size() && (Input[Position] == 'e' || Input[Position] == 'E')) {
				++Position;
				static_cast<void>(consume('+') || consume('-'));
				const auto digits = Position;
				while (Position < Input.size() && std::isdigit(static_cast<unsigned char>(Input[Position])) != 0) {
					++Position;
				}
				if (Position == digits) {
					return false;
				}
			}
			return Position != start;
		}

		[[nodiscard]] auto literal(const std::string_view aExpected) noexcept -> bool
		{
			if (Input.substr(Position, aExpected.size()) != aExpected) {
				return false;
			}
			Position += aExpected.size();
			return true;
		}

		void whitespace() noexcept
		{
			while (Position < Input.size()
				   && (Input[Position] == ' ' || Input[Position] == '\t'
					   || Input[Position] == '\r' || Input[Position] == '\n')) {
				++Position;
			}
		}

		[[nodiscard]] auto consume(const char aExpected) noexcept -> bool
		{
			if (Position >= Input.size() || Input[Position] != aExpected) {
				return false;
			}
			++Position;
			return true;
		}

		std::string_view      Input;
		json_preflight_limits Limits;
		std::size_t           Position = 0U;
		std::size_t           Values   = 0U;
	};
}

#endif
