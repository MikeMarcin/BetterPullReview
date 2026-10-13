#include "bpr/core/json_preflight.hpp"

#include <algorithm>
#include <array>

namespace bpr::core {
	namespace {
		/// [[BPR-1433]]. One implementation for every JSON response check; it
		/// replaced three drifted copies in the GitHub adapter and its shared
		/// header.
		class preflight final
		{
		public:
			preflight(const std::string_view aInput, const json_preflight_limits& aLimits) noexcept
				: Input{ aInput }
				, Limits{ aLimits }
				, ObjectMembers{ std::min(aLimits.ObjectMembers, maximum_json_preflight_object_members) }
			{ }

			[[nodiscard]] auto run() noexcept -> json_preflight_report
			{
				whitespace();
				auto result = value(0U);
				if (result == json_preflight_result::valid) {
					whitespace();
					if (Position != Input.size()) {
						result = json_preflight_result::malformed;
					}
				}
				return { .Result = result, .Values = Values };
			}

		private:
			[[nodiscard]] auto value(const std::size_t aDepth) noexcept -> json_preflight_result
			{
				if (Values == Limits.Values) {
					return json_preflight_result::structural_limit;
				}
				++Values;
				whitespace();
				if (Position >= Input.size()) {
					return json_preflight_result::malformed;
				}
				switch (Input[Position]) {
				case '{': return object(aDepth);
				case '[': return array(aDepth);
				case '"': return string(nullptr, false);
				case 't': return literal("true");
				case 'f': return literal("false");
				case 'n': return literal("null");
				default:  return number();
				}
			}

			[[nodiscard]] auto object(const std::size_t aDepth) noexcept -> json_preflight_result
			{
				if (aDepth >= Limits.Depth) {
					return json_preflight_result::structural_limit;
				}
				++Position;
				whitespace();
				if (consume('}')) {
					return json_preflight_result::valid;
				}
				auto keys  = std::array<std::string_view, maximum_json_preflight_object_members>{ };
				auto count = std::size_t{ 0U };
				while (true) {
					if (count == ObjectMembers) {
						return json_preflight_result::structural_limit;
					}
					auto       key  = std::string_view{ };
					const auto read = string(&key, true);
					if (read != json_preflight_result::valid) {
						return read;
					}
					const auto seen = keys.begin() + static_cast<std::ptrdiff_t>(count);
					if (std::ranges::find(keys.begin(), seen, key) != seen) {
						return json_preflight_result::duplicate_key;
					}
					keys[count++] = key;
					whitespace();
					if (!consume(':')) {
						return json_preflight_result::malformed;
					}
					const auto child = value(aDepth + 1U);
					if (child != json_preflight_result::valid) {
						return child;
					}
					whitespace();
					if (consume('}')) {
						return json_preflight_result::valid;
					}
					if (!consume(',')) {
						return json_preflight_result::malformed;
					}
					whitespace();
				}
			}

			[[nodiscard]] auto array(const std::size_t aDepth) noexcept -> json_preflight_result
			{
				if (aDepth >= Limits.Depth) {
					return json_preflight_result::structural_limit;
				}
				++Position;
				whitespace();
				if (consume(']')) {
					return json_preflight_result::valid;
				}
				auto count = std::size_t{ 0U };
				while (true) {
					if (count == Limits.ArrayElements) {
						return json_preflight_result::structural_limit;
					}
					++count;
					const auto child = value(aDepth + 1U);
					if (child != json_preflight_result::valid) {
						return child;
					}
					whitespace();
					if (consume(']')) {
						return json_preflight_result::valid;
					}
					if (!consume(',')) {
						return json_preflight_result::malformed;
					}
					whitespace();
				}
			}

			/// An object key may not contain an escape: keys are compared as written.
			[[nodiscard]] auto string(std::string_view* aOutput, const bool aObjectKey) noexcept -> json_preflight_result
			{
				if (!consume('"')) {
					return json_preflight_result::malformed;
				}
				const auto start = Position;
				while (Position < Input.size()) {
					if (Position - start > Limits.StringBytes) {
						return json_preflight_result::structural_limit;
					}
					const auto character = static_cast<unsigned char>(Input[Position++]);
					if (character == '"') {
						if (aOutput != nullptr) {
							*aOutput = Input.substr(start, Position - start - 1U);
						}
						return json_preflight_result::valid;
					}
					if (character < 0x20U) {
						return json_preflight_result::malformed;
					}
					if (character != '\\') {
						continue;
					}
					if (aObjectKey || Position >= Input.size()) {
						return json_preflight_result::malformed;
					}
					const auto escaped = Input[Position++];
					if (escaped == '"' || escaped == '\\' || escaped == '/' || escaped == 'b' || escaped == 'f' || escaped == 'n' || escaped == 'r' || escaped == 't') {
						continue;
					}
					if (escaped != 'u' || Input.size() - Position < 4U) {
						return json_preflight_result::malformed;
					}
					for (std::size_t index = 0U; index < 4U; ++index) {
						const auto digit = Input[Position++];
						const auto hex   = (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f') || (digit >= 'A' && digit <= 'F');
						if (!hex) {
							return json_preflight_result::malformed;
						}
					}
				}
				return json_preflight_result::malformed;
			}

			[[nodiscard]] auto digits() noexcept -> std::size_t
			{
				const auto begin = Position;
				while (Position < Input.size() && Input[Position] >= '0' && Input[Position] <= '9') {
					++Position;
				}
				return Position - begin;
			}

			/// JSON number grammar. The magnitude test counts the decimal
			/// position of the first significant digit, so 1e308 and above
			/// (including every value a double cannot hold) is refused while
			/// underflow to zero is admitted.
			[[nodiscard]] auto number() noexcept -> json_preflight_result
			{
				static_cast<void>(consume('-'));
				const auto integerBegin = Position;
				if (consume('0')) {
					if (Position < Input.size() && Input[Position] >= '0' && Input[Position] <= '9') {
						return json_preflight_result::malformed;
					}
				} else if (digits() == 0U) {
					return json_preflight_result::malformed;
				}
				const auto integer      = Input.substr(integerBegin, Position - integerBegin);
				auto       fraction     = std::string_view{ };
				auto       exponent     = std::int64_t{ 0 };
				auto       negative     = false;
				auto       exponentLong = false;
				if (consume('.')) {
					const auto begin = Position;
					if (digits() == 0U) {
						return json_preflight_result::malformed;
					}
					fraction = Input.substr(begin, Position - begin);
				}
				if (Position < Input.size() && (Input[Position] == 'e' || Input[Position] == 'E')) {
					++Position;
					if (Position < Input.size() && (Input[Position] == '+' || Input[Position] == '-')) {
						negative = Input[Position] == '-';
						++Position;
					}
					const auto begin = Position;
					if (digits() == 0U) {
						return json_preflight_result::malformed;
					}
					for (const auto digit : Input.substr(begin, Position - begin)) {
						exponent = exponent * 10 + (digit - '0');
						if (exponent > 100'000) {
							exponentLong = true;
							break;
						}
					}
				}
				// Decimal position of the first significant digit (10^position).
				const auto leading  = integer.find_first_not_of('0');
				auto       position = std::int64_t{ 0 };
				if (leading != std::string_view::npos) {
					position = static_cast<std::int64_t>(integer.size() - leading) - 1;
				} else {
					const auto first = fraction.find_first_not_of('0');
					if (first == std::string_view::npos) {
						return json_preflight_result::valid; // zero
					}
					position = -static_cast<std::int64_t>(first) - 1;
				}
				if (exponentLong) {
					return negative ? json_preflight_result::valid : json_preflight_result::malformed;
				}
				position += negative ? -exponent : exponent;
				return position >= 308 ? json_preflight_result::malformed : json_preflight_result::valid;
			}

			[[nodiscard]] auto literal(const std::string_view aExpected) noexcept -> json_preflight_result
			{
				if (Input.substr(Position, aExpected.size()) != aExpected) {
					return json_preflight_result::malformed;
				}
				Position += aExpected.size();
				return json_preflight_result::valid;
			}

			void whitespace() noexcept
			{
				while (Position < Input.size() && (Input[Position] == ' ' || Input[Position] == '\t' || Input[Position] == '\r' || Input[Position] == '\n')) {
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
			std::size_t           ObjectMembers;
			std::size_t           Position = 0U;
			std::size_t           Values   = 0U;
		};
	}

	auto json_preflight(const std::string_view aInput, const json_preflight_limits& aLimits) noexcept -> json_preflight_report
	{
		return preflight{ aInput, aLimits }.run();
	}
}
