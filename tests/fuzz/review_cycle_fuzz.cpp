// [[BPR-0970]]. The shared readers the retired Review Cycles left behind
// for local drafts, tours and suggestions: suggestion wire text, the strict
// JSON, Base64 and SHA-256 readers, manifests, publication plans and journals,
// ZIP archives and DEFLATE streams. [[BPR-2308]] retired the cycle's own
// documents (responses, manual replies, rounds, bundles, import claims); their
// selectors read nothing, so the remaining corpus keeps its selector bytes.
//
// Input: [selector] then the payload, read by the decoder the selector names.

#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_cycle_manifest.hpp"
#include "bpr/core/review_cycle_policy.hpp"
#include "bpr/core/review_cycle_publication.hpp"
#include "bpr/core/review_cycle_suggestion.hpp"
#include "bpr/core/review_cycle_zip.hpp"
#include "fuzz_support.hpp"

#include <string>

namespace {
	using namespace bpr;

	auto bytes_of(const std::string_view aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	auto suggestion(const std::string_view aText) -> void
	{
		const auto decoded = core::decode_suggestion_wire(aText);
		if (decoded) {
			fuzz::require(core::decode_suggestion_wire(core::encode_suggestion_wire(*decoded)) == decoded);
		}
	}

	auto json(const std::string_view aText) -> void
	{
		const auto parsed = core::parse_cycle_json(aText, 65536U);
		if (!parsed) {
			return;
		}
		// Canonical form is a fixed point.
		if (const auto canonical = core::canonical_cycle_json(*parsed)) {
			const auto reparsed = core::parse_cycle_json(*canonical, canonical->size());
			fuzz::require(reparsed.has_value());
			fuzz::require(core::canonical_cycle_json(*reparsed) == canonical);
		}
	}

	auto base64_and_digest(const std::string_view aText) -> void
	{
		if (const auto decoded = core::decode_cycle_base64(aText)) {
			fuzz::require(decoded->size() <= (aText.size() / 4U) * 3U);
		}
		if (const auto digest = core::parse_cycle_sha256(aText)) {
			fuzz::require(aText.size() == 64U && core::cycle_hex(*digest) == aText);
		}
	}

	auto manifest(const std::span<const std::uint8_t> aBytes) -> void
	{
		const auto decoded = core::decode_manifest(aBytes);
		if (decoded) {
			fuzz::require(!core::validate_manifest(*decoded).has_value());
			const auto encoded = core::encode_manifest(*decoded);
			fuzz::require(core::decode_manifest(encoded) == decoded);
		}
	}

	auto publication(const std::span<const std::uint8_t> aBytes) -> void
	{
		if (const auto plan = core::decode_publication_plan(aBytes)) {
			fuzz::require(core::decode_publication_plan(core::encode_publication_plan(*plan)) == plan);
		}
		if (const auto journal = core::decode_publication_journal(aBytes)) {
			const auto encoded = core::encode_publication_journal(*journal);
			const auto again   = core::decode_publication_journal(encoded);
			fuzz::require(again.has_value() && core::encode_publication_journal(*again) == encoded);
		}
	}

	auto zip(const std::span<const std::uint8_t> aBytes) -> void
	{
		// Small bounds so a campaign reaches each refusal.
		const core::cycle_zip_limits limits{ .Entries = 64U, .DecodedBytes = 1U << 20U, .Expansion = 100U, .NameBytes = 256U };
		const auto                   members = core::read_cycle_zip(aBytes, limits);
		if (!members) {
			return;
		}
		std::uint64_t total = 0U;
		fuzz::require(members->size() <= limits.Entries);
		for (const auto& member : *members) {
			fuzz::require(member.Name.size() <= limits.NameBytes);
			total += member.Bytes.size();
		}
		fuzz::require(total <= limits.DecodedBytes);
		// What was read writes back to an archive that reads to the same members.
		const auto rewritten = core::write_cycle_zip(*members);
		fuzz::require(core::read_cycle_zip(bytes_of(rewritten), limits) == members);
	}

	auto inflate(fuzz::fuzz_input& aInput) -> void
	{
		const auto expected = aInput.u16();
		const auto inflated = core::inflate_cycle_deflate(aInput.rest(), expected);
		if (inflated) {
			fuzz::require(inflated->size() == expected);
		}
	}

}

// libFuzzer calls the entry point by this exact name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* aData, const std::size_t aSize) -> int
{
	fuzz::fuzz_input input{ aData, aSize };
	const auto       selector = input.byte();
	if (input.rest().size() > 65536U) {
		return 0;
	}
	const auto text  = input.text();
	const auto bytes = input.rest();
	switch (selector % 13U) {
	case 2U:
		suggestion(text);
		break;
	case 6U:
		json(text);
		break;
	case 7U:
		base64_and_digest(text);
		break;
	case 8U:
		manifest(bytes);
		break;
	case 9U:
		publication(bytes);
		break;
	case 11U:
		zip(bytes);
		break;
	case 12U:
		if ((selector & 0x80U) != 0U) {
			inflate(input);
		}
		break;
	default:
		// The retired cycle documents' selectors.
		break;
	}
	return 0;
}
