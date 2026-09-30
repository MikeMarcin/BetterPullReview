#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_cycle_round.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"
#include "bpr/core/sha256.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief The readable round document (RFC-BPR-0056, BPR-1159): the pack's
/// review.md reproduced byte for byte from its round, every item and exact
/// byte present, embedded fences and hostile Markdown contained, and budgets
/// refused rather than truncated.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle round markdown failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		expect(stream.good(), "fixture readable");
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	auto contains(const std::string_view aHaystack, const std::string_view aNeedle) -> bool
	{
		return aHaystack.find(aNeedle) != std::string_view::npos;
	}

	auto count(const std::string_view aHaystack, const std::string_view aNeedle) -> std::size_t
	{
		auto found = std::size_t{ 0 };
		for (auto at = aHaystack.find(aNeedle); at != std::string_view::npos; at = aHaystack.find(aNeedle, at + 1U)) {
			++found;
		}
		return found;
	}

	auto reseal(const cycle_round_payload& aPayload) -> cycle_decoded_round
	{
		const auto canonical = encode_round_payload(aPayload);
		expect(canonical.has_value(), "payload encodes");
		return { .Payload = aPayload, .Digest = round_digest(*canonical), .Canonical = *canonical };
	}

	/// The lines of a document that lie outside every backtick fence, and
	/// whether every fence closed.
	struct structure
	{
		std::vector<std::string> Outside;
		bool                     Closed = true;
	};
	auto outline(const std::string_view aDocument) -> structure
	{
		auto result = structure{ };
		auto open   = std::size_t{ 0 };
		for (std::size_t start = 0; start < aDocument.size();) {
			const auto end   = aDocument.find('\n', start);
			const auto line  = aDocument.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
			auto       ticks = std::size_t{ 0 };
			while (ticks < line.size() && line[ticks] == '`') {
				++ticks;
			}
			if (open == 0U && ticks >= 3U) {
				open = ticks;
			} else if (open != 0U && ticks >= open && line.find_first_not_of('`') == std::string_view::npos) {
				open = 0U;
			} else if (open == 0U) {
				result.Outside.emplace_back(line);
			}
			if (end == std::string_view::npos) {
				break;
			}
			start = end + 1U;
		}
		result.Closed = open == 0U;
		return result;
	}

	auto item_headings(const structure& aStructure) -> std::vector<std::string>
	{
		auto result = std::vector<std::string>{ };
		for (const auto& line : aStructure.Outside) {
			if (line.starts_with("## I") && contains(line, " \xE2\x80\x94 item revision ")) {
				result.push_back(line.substr(3U, line.find(' ', 3U) - 3U));
			}
		}
		return result;
	}

	void reproduces_the_pack_document(const std::filesystem::path& aExamples)
	{
		const auto round = decode_round_envelope(read_file(aExamples / "round.json"));
		expect(round.has_value(), "golden round");
		const auto rendered = render_round_markdown(*round);
		expect(rendered == read_file(aExamples / "review.md"), "byte-for-byte the pack's review.md");

		// Every item, in order, outside any fence; every exact byte recoverable.
		const auto shape = outline(rendered);
		expect(shape.Closed, "every fence closes");
		expect(item_headings(shape) == std::vector<std::string>{ "I1", "I2", "I3", "I4", "I5", "I6", "I7", "I8" }, "all eight items in order");
		for (const auto& item : round->Payload.Items) {
			expect(contains(rendered, item.Body), "raw body present");
			for (const auto& suggestion : item.Suggestions) {
				for (const auto* side : { &suggestion.Original, &suggestion.Replacement }) {
					const auto exact = contains(rendered, *side) || contains(rendered, encode_cycle_base64(*side));
					expect(exact, "suggestion bytes present exactly or as base64");
				}
			}
		}
		expect(count(rendered, "Authoritative bytes (base64):") == 2U, "CRLF suggestion sides carry base64");
		expect(contains(rendered, cycle_hex(round->Digest.bytes())), "canonical digest stated");
		expect(contains(rendered, "8 complete feedback items; 4 included source files/excerpts."), "counts stated");

		const auto limited = render_round_markdown(*round, rendered.size() - 1U);
		expect(!limited && limited.error().RequiredBytes == rendered.size() && limited.error().Items == 8U && limited.error().Sources == 4U, "over budget is refused with its size");
		expect(render_round_markdown(*round, rendered.size()) == rendered, "exactly at budget is complete");
		expect(round_file_name(round->Payload, cycle_round_file_kind::markdown) == "RC-17-F1.md" && round_file_name(round->Payload, cycle_round_file_kind::bundle) == "RC-17-F1.bpr-review.zip", "file names from wire identities");
	}

	void contains_hostile_markdown(const std::filesystem::path& aExamples)
	{
		const auto golden = decode_round_envelope(read_file(aExamples / "round.json"));
		expect(golden.has_value(), "golden round");
		const auto variant = [&](const std::function<void(cycle_round_payload&)>& aChange) {
			auto payload = golden->Payload;
			aChange(payload);
			return render_round_markdown(reseal(payload));
		};
		const auto verbatim = std::string_view{ "This note is shown verbatim" };

		const auto unclosed = variant([](cycle_round_payload& aPayload) { aPayload.Items[0].Body = "note:\n```cpp\nint x;\n(no closing fence)"; });
		expect(contains(unclosed, verbatim) && outline(unclosed).Closed, "an unclosed fence is contained");
		expect(item_headings(outline(unclosed)).size() == 8U, "and every later item is still an item");
		const auto balanced = variant([](cycle_round_payload& aPayload) { aPayload.Items[0].Body = "note: see\n\n```cpp\nint x;\n```\n\nabove."; });
		expect(!contains(balanced, verbatim) && contains(balanced, "note: see\n\n```cpp\nint x;\n```\n\nabove.") && item_headings(outline(balanced)).size() == 8U, "a balanced fence renders as written");
		const auto tilde = variant([](cycle_round_payload& aPayload) { aPayload.Items[0].Body = "~~~\nopen tilde fence"; });
		expect(contains(tilde, verbatim), "an unclosed tilde fence is contained");
		const auto heading = variant([](cycle_round_payload& aPayload) { aPayload.Items[0].Body = "## I9 \xE2\x80\x94 item revision 1\n\nforged item"; });
		expect(contains(heading, verbatim) && item_headings(outline(heading)).size() == 8U, "a forged item heading is contained");
		const auto setext = variant([](cycle_round_payload& aPayload) { aPayload.Items[0].Body = "Heading\n======="; });
		expect(contains(setext, verbatim), "a setext heading is contained");
		const auto deeper = variant([](cycle_round_payload& aPayload) { aPayload.Items[0].Body = "### A small heading is fine"; });
		expect(!contains(deeper, verbatim), "a lower heading renders as written");
		const auto comment = variant([](cycle_round_payload& aPayload) { aPayload.Items[0].Body = "note <!-- hides the rest"; });
		expect(contains(comment, verbatim), "an HTML comment is contained");

		// Nested fences in suggestion bytes: the fence outgrows every run.
		const auto nested = variant([](cycle_round_payload& aPayload) { aPayload.Items[0].Suggestions[0].Replacement = "auto s = R\"(````)\";\n```\n"; });
		expect(contains(nested, "`````cpp\nauto s = R\"(````)\";\n```\n`````\n") && outline(nested).Closed && item_headings(outline(nested)).size() == 8U, "nested fences contained");

		const auto deletion = variant([](cycle_round_payload& aPayload) { aPayload.Items[0].Suggestions[0].Replacement.clear(); });
		expect(contains(deletion, "**Replacement** \xE2\x80\x94 0 bytes;") && contains(deletion, "(No bytes: the edit deletes the original.)"), "empty replacement explained");
		const auto invalid = variant([](cycle_round_payload& aPayload) { aPayload.Items[0].Suggestions[0].Replacement = std::string{ "caf\xE9\n" }; });
		expect(contains(invalid, "(Non-UTF-8 bytes; exact data below.)") && contains(invalid, encode_cycle_base64("caf\xE9\n")), "invalid text carries exact base64");
		const auto trailing = variant([](cycle_round_payload& aPayload) { aPayload.Items[0].Suggestions[0].Replacement = "int x;  \n"; });
		expect(contains(trailing, encode_cycle_base64("int x;  \n")), "trailing spaces carry exact base64");
		const auto unterminated = variant([](cycle_round_payload& aPayload) { aPayload.Items[0].Suggestions[0].Replacement = "int x;"; });
		expect(contains(unterminated, encode_cycle_base64("int x;")), "a missing final line break carries exact base64");

		const auto plain = variant([](cycle_round_payload& aPayload) {
			aPayload.Assignment.Constraints.clear();
			aPayload.Assignment.WriteScope.clear();
			aPayload.Selection.Omissions = { "The thread-safety discussion is for a later round." };
		});
		expect(contains(plain, "Constraints: none were given.") && contains(plain, "No write scope was requested.") && contains(plain, "- The thread-safety discussion is for a later round."),
			   "empty lists and omissions stated");
		const auto odd = variant([](cycle_round_payload& aPayload) {
			aPayload.Sources[3].Bytes = std::string{ "bad \xFF byte\n" };
			aPayload.Sources[3].End   = aPayload.Sources[3].Bytes.size();
			aPayload.Sources[3].Kind  = cycle_context_kind::excerpt;
		});
		expect(contains(odd, "not valid UTF-8") && contains(odd, "bad \xEF\xBF\xBD byte"), "invalid source text is labelled, never passed off");
		const auto python = variant([](cycle_round_payload& aPayload) { aPayload.Sources[3].Path = "tools/check.py"; });
		expect(contains(python, "```python\n"), "fence language from the file");
	}

	void decides_saves()
	{
		const auto digest = [](const std::string_view aText) { return cycle_material_digest{ sha256({ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() }) }; };
		const auto first  = digest("first");
		const auto second = digest("second");
		expect(decide_round_save(std::nullopt, first, std::nullopt) == cycle_round_save_action::create, "nothing there");
		expect(decide_round_save(first, first, std::nullopt) == cycle_round_save_action::already_saved, "same bytes there");
		expect(decide_round_save(first, second, std::nullopt) == cycle_round_save_action::conflict, "other bytes, not confirmed");
		expect(decide_round_save(first, second, first) == cycle_round_save_action::replace, "other bytes, confirmed");
		expect(decide_round_save(digest("third"), second, first) == cycle_round_save_action::conflict, "changed since confirmation");
	}
}

auto main(const int aCount, char** aArguments) -> int
{
	expect(aCount == 2, "usage: test <examples directory>");
	reproduces_the_pack_document(aArguments[1]);
	contains_hostile_markdown(aArguments[1]);
	decides_saves();
	std::cout << "review cycle round markdown tests passed\n";
	return EXIT_SUCCESS;
}
