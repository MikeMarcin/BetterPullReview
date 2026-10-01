// [[BPR-0970]]. AI structured output, Review Cycles half: the documents an
// agent or another reviewer hands back (responses, manual replies,
// suggestions, rounds and their ZIP bundles), the strict JSON and DEFLATE
// readers under them, and the cycle's own durable records.
//
// Input: [selector] then the payload, read by the decoder the selector names.

#include "bpr/core/review_cycle_bundle.hpp"
#include "bpr/core/review_cycle_import.hpp"
#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_cycle_manifest.hpp"
#include "bpr/core/review_cycle_policy.hpp"
#include "bpr/core/review_cycle_publication.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "bpr/core/review_cycle_round.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"
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

	auto response(const std::string_view aText) -> void
	{
		const auto decoded = core::decode_response_document(aText);
		if (!decoded) {
			return;
		}
		// The canonical bytes are what the document encodes to, and they read
		// back to the same canonical bytes and digest.
		fuzz::require(core::encode_response_document(decoded->Document) == decoded->Canonical);
		const auto again = core::decode_response_document(decoded->Canonical);
		fuzz::require(again && again->Canonical == decoded->Canonical && again->Digest == decoded->Digest);
	}

	auto manual_reply(const std::string_view aText) -> void
	{
		const auto decoded = core::decode_manual_reply(aText);
		if (decoded) {
			const auto encoded = core::encode_manual_reply(*decoded);
			fuzz::require(encoded.has_value() && *encoded == aText);
		}
	}

	auto suggestion(const std::string_view aText) -> void
	{
		const auto decoded = core::decode_suggestion_wire(aText);
		if (decoded) {
			fuzz::require(core::decode_suggestion_wire(core::encode_suggestion_wire(*decoded)) == decoded);
		}
	}

	auto round_envelope(const std::string_view aText) -> void
	{
		const auto decoded = core::decode_round_envelope(aText);
		if (decoded) {
			const auto payload = core::decode_round_payload(decoded->Canonical);
			fuzz::require(payload && payload->Digest == decoded->Digest && payload->Canonical == decoded->Canonical);
			fuzz::require(core::encode_round_payload(decoded->Payload) == decoded->Canonical);
			// An admitted round renders as the review Markdown it is handed out as.
			static_cast<void>(core::render_round_markdown(*decoded, 1U << 20U));
		}
	}

	auto round_payload(const std::string_view aText) -> void
	{
		const auto decoded = core::decode_round_payload(aText);
		if (decoded) {
			// Only byte-for-byte canonical payloads are read.
			fuzz::require(decoded->Canonical == aText);
			fuzz::require(core::encode_round_payload(decoded->Payload) == decoded->Canonical);
		}
	}

	auto round_selection(const std::string_view aText) -> void
	{
		const auto decoded = core::decode_round_selection(aText);
		if (decoded) {
			const auto encoded = core::encode_round_selection(*decoded);
			fuzz::require(encoded && core::decode_round_selection(*encoded).has_value());
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

	auto import_and_claim(const std::string_view aText) -> void
	{
		const core::local_cycle_authority authority{ core::default_bpr_profile_id, fuzz::opaque<core::cycle_catalog_incarnation>(3) };
		if (const auto association = core::decode_import_association(aText, authority)) {
			const auto encoded = core::encode_import_association(*association);
			fuzz::require(core::decode_import_association(encoded, authority).has_value());
		}
		static_cast<void>(core::parse_imported_claim(aText));
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

	auto bundle(const std::span<const std::uint8_t> aBytes) -> void
	{
		const auto opened = core::open_round_bundle(aBytes);
		if (opened) {
			const auto payload = core::decode_round_payload(opened->Round.Canonical);
			fuzz::require(payload && payload->Digest == opened->Round.Digest);
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
	case 0U:
		response(text);
		break;
	case 1U:
		manual_reply(text);
		break;
	case 2U:
		suggestion(text);
		break;
	case 3U:
		round_envelope(text);
		break;
	case 4U:
		round_payload(text);
		break;
	case 5U:
		round_selection(text);
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
	case 10U:
		import_and_claim(text);
		break;
	case 11U:
		zip(bytes);
		break;
	default:
		if ((selector & 0x80U) != 0U) {
			inflate(input);
		} else {
			bundle(bytes);
		}
		break;
	}
	return 0;
}
