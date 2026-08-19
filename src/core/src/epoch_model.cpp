#include "bpr/core/epoch_model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace bpr::core {
	namespace {
		[[nodiscard]] constexpr auto hex_nibble(const char aCharacter) noexcept
			-> std::optional<std::uint8_t>
		{
			if (aCharacter >= '0' && aCharacter <= '9') {
				return static_cast<std::uint8_t>(aCharacter - '0');
			}
			if (aCharacter >= 'a' && aCharacter <= 'f') {
				return static_cast<std::uint8_t>(aCharacter - 'a' + 10);
			}
			if (aCharacter >= 'A' && aCharacter <= 'F') {
				return static_cast<std::uint8_t>(aCharacter - 'A' + 10);
			}
			return std::nullopt;
		}

		template <typename Oid>
		[[nodiscard]] auto parse_oid(const std::string_view aText) noexcept
			-> std::optional<Oid>
		{
			git_hash_algorithm algorithm = git_hash_algorithm::invalid;
			if (aText.size() == 40U) {
				algorithm = git_hash_algorithm::sha1;
			} else if (aText.size() == 64U) {
				algorithm = git_hash_algorithm::sha256;
			} else {
				return std::nullopt;
			}

			std::array<std::uint8_t, 32> bytes{ };
			for (std::size_t index = 0; index < aText.size(); index += 2U) {
				const auto high = hex_nibble(aText[index]);
				const auto low  = hex_nibble(aText[index + 1U]);
				if (!high.has_value() || !low.has_value()) {
					return std::nullopt;
				}
				bytes[index / 2U] = static_cast<std::uint8_t>(
					static_cast<std::uint8_t>(*high << 4U) | *low);
			}

			const Oid result{ algorithm, bytes };
			if (!result.valid()) {
				return std::nullopt;
			}
			return result;
		}

		template <typename Oid>
		[[nodiscard]] auto render_oid(const Oid& aOid) noexcept -> canonical_git_oid
		{
			constexpr std::array digits{
				'0',
				'1',
				'2',
				'3',
				'4',
				'5',
				'6',
				'7',
				'8',
				'9',
				'a',
				'b',
				'c',
				'd',
				'e',
				'f',
			};

			if (!aOid.valid()) {
				return { };
			}

			std::array<char, 64> characters{ };
			const auto           count = aOid.byte_count();
			for (std::size_t index = 0; index < count; ++index) {
				const auto byte             = aOid.bytes()[index];
				characters[index * 2U]      = digits[static_cast<std::size_t>(byte >> 4U)];
				characters[index * 2U + 1U] = digits[static_cast<std::size_t>(byte & 0x0FU)];
			}
			return canonical_git_oid{
				characters,
				static_cast<std::uint8_t>(count * 2U),
			};
		}
	} // namespace

	auto parse_git_commit_oid(const std::string_view aText) noexcept
		-> std::optional<git_commit_oid>
	{
		return parse_oid<git_commit_oid>(aText);
	}

	auto parse_git_tree_oid(const std::string_view aText) noexcept
		-> std::optional<git_tree_oid>
	{
		return parse_oid<git_tree_oid>(aText);
	}

	auto parse_git_blob_oid(const std::string_view aText) noexcept
		-> std::optional<git_blob_oid>
	{
		return parse_oid<git_blob_oid>(aText);
	}

	auto canonical_hex(const git_commit_oid& aOid) noexcept -> canonical_git_oid
	{
		return render_oid(aOid);
	}

	auto canonical_hex(const git_tree_oid& aOid) noexcept -> canonical_git_oid
	{
		return render_oid(aOid);
	}

	auto canonical_hex(const git_blob_oid& aOid) noexcept -> canonical_git_oid
	{
		return render_oid(aOid);
	}

	auto reason_for(const epoch_failure aFailure) noexcept -> epoch_reason_code
	{
		switch (aFailure) {
		case epoch_failure::unavailable:
			return epoch_reason_code::unavailable;
		case epoch_failure::malformed_observation:
			return epoch_reason_code::invalid_observation;
		case epoch_failure::catalog_conflict:
			return epoch_reason_code::catalog_conflict;
		case epoch_failure::quota_exceeded:
			return epoch_reason_code::quota_exceeded;
		case epoch_failure::ordinal_exhausted:
			return epoch_reason_code::epoch_ordinal_exhausted;
		case epoch_failure::repair_required:
			return epoch_reason_code::repair_required;
		case epoch_failure::process_failure:
			return epoch_reason_code::process_failure;
		}
		return epoch_reason_code::invalid_observation;
	}
} // namespace bpr::core
