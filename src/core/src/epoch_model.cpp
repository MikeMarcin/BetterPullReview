#include "bpr/core/epoch_model.hpp"
#include "bpr/core/sha256.hpp"

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

	auto make_epoch_occurrence_id(const std::string_view aCanonicalHostname,
								  const pull_request_id aPullRequest, const epoch_occurrence_evidence& aEvidence) noexcept
		-> std::optional<epoch_occurrence_id>
	{
		if (aCanonicalHostname.empty() || aCanonicalHostname.size() > 253U
			|| aPullRequest.value() == 0U || !aEvidence.Head.valid()
			|| (aEvidence.Kind != epoch_occurrence_kind::force_push_event
				&& aEvidence.Kind != epoch_occurrence_kind::observed_head)
			|| ((aEvidence.Kind == epoch_occurrence_kind::force_push_event) != (aEvidence.EventId != 0U))) {
			return std::nullopt;
		}
		std::size_t labelLength = 0U;
		char        previous    = '.';
		for (const char character : aCanonicalHostname) {
			if (character == '.') {
				if (labelLength == 0U || previous == '-') {
					return std::nullopt;
				}
				labelLength = 0U;
			} else {
				if (!((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9')
					  || (character == '-' && labelLength != 0U))
					|| ++labelLength > 63U) {
					return std::nullopt;
				}
			}
			previous = character;
		}
		if (labelLength == 0U || previous == '-') {
			return std::nullopt;
		}
		std::array<std::uint8_t, 320U> bytes{ };
		std::size_t                    size = 0U;
		// Fixed domain/version, length-prefixed canonical host, big-endian numeric id.
		for (const char value : std::string_view{ "BPR-occurrence-v1" }) {
			bytes[size++] = static_cast<std::uint8_t>(value);
		}
		bytes[size++] = static_cast<std::uint8_t>(aCanonicalHostname.size());
		for (const char value : aCanonicalHostname) {
			bytes[size++] = static_cast<std::uint8_t>(value);
		}
		const auto appendNumber = [&](const std::uint64_t aValue) {
			for (unsigned shift = 64U; shift != 0U; shift -= 8U) {
				bytes[size++] = static_cast<std::uint8_t>(aValue >> (shift - 8U));
			}
		};
		appendNumber(aPullRequest.value());
		bytes[size++] = static_cast<std::uint8_t>(aEvidence.Kind);
		if (aEvidence.Kind == epoch_occurrence_kind::force_push_event) {
			// The after-head is a consistency check, never a way to mint another event identity.
			appendNumber(aEvidence.EventId);
		} else {
			bytes[size++] = static_cast<std::uint8_t>(aEvidence.Head.algorithm());
			for (const auto value : aEvidence.Head.bytes()) {
				bytes[size++] = value;
			}
		}
		return epoch_occurrence_id{ sha256(std::span{ bytes }.first(size)) };
	}

	auto valid(const epoch_occurrence& aOccurrence) noexcept -> bool
	{
		const auto& evidence  = aOccurrence.Evidence;
		const auto  validBase = [&](const std::optional<git_commit_oid>& aBase) {
			return !aBase || (aBase->valid() && aBase->algorithm() == evidence.Head.algorithm());
		};
		return aOccurrence.Id.valid() && evidence.Head.valid()
			&& core::valid(aOccurrence.FirstSeen)
			&& aOccurrence.DateUtcMilliseconds >= -62'135'596'800'000LL
			&& aOccurrence.DateUtcMilliseconds <= maximum_observed_utc_milliseconds
			&& (evidence.Kind == epoch_occurrence_kind::force_push_event || evidence.Kind == epoch_occurrence_kind::observed_head)
			&& ((evidence.Kind == epoch_occurrence_kind::force_push_event) == (evidence.EventId != 0U))
			&& (!aOccurrence.LocalEpoch || (aOccurrence.LocalEpoch->Authority.valid() && aOccurrence.LocalEpoch->Incarnation.valid() && core::valid(aOccurrence.LocalEpoch->Ordinal)))
			&& validBase(aOccurrence.HistoricalBase) && validBase(aOccurrence.DerivedComparisonBase)
			&& (aOccurrence.DateSource == epoch_occurrence_date_source::commit_date
				|| (aOccurrence.DateSource == epoch_occurrence_date_source::github_event && evidence.Kind == epoch_occurrence_kind::force_push_event)
				|| (aOccurrence.DateSource == epoch_occurrence_date_source::unavailable && aOccurrence.DateUtcMilliseconds == 0));
	}

	auto epoch_occurrence_less(const epoch_occurrence& aLeft, const epoch_occurrence& aRight) noexcept -> bool
	{
		const auto leftUndated  = aLeft.DateSource == epoch_occurrence_date_source::unavailable;
		const auto rightUndated = aRight.DateSource == epoch_occurrence_date_source::unavailable;
		if (leftUndated != rightUndated) {
			return !leftUndated;
		}
		if (!leftUndated && aLeft.DateUtcMilliseconds != aRight.DateUtcMilliseconds) {
			return aLeft.DateUtcMilliseconds < aRight.DateUtcMilliseconds;
		}
		return aLeft.Id < aRight.Id;
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
		case epoch_failure::unsupported_object:
			return epoch_reason_code::unsupported_object;
		case epoch_failure::git_tool_unsupported:
			return epoch_reason_code::git_tool_unsupported;
		case epoch_failure::git_tool_probe_failed:
			return epoch_reason_code::git_tool_probe_failed;
		case epoch_failure::gh_tool_unsupported:
			return epoch_reason_code::gh_tool_unsupported;
		case epoch_failure::gh_tool_probe_failed:
			return epoch_reason_code::gh_tool_probe_failed;
		}
		return epoch_reason_code::invalid_observation;
	}
} // namespace bpr::core
