#include "bpr/core/review_cycle_material.hpp"
#include "bpr/core/sha256.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle material failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto bytes(const std::string_view aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	auto classify(const std::string_view aText) -> material_profile
	{
		return classify_material(bytes(aText));
	}

	/// The same bytes delivered one at a time give the same profile.
	auto classify_bytewise(const std::string_view aText) -> material_profile
	{
		auto classifier = material_classifier{ };
		for (std::size_t index = 0; index < aText.size(); ++index) {
			classifier.add(bytes(aText).subspan(index, 1));
		}
		return classifier.finish();
	}

	void classifies_the_byte_golden_corpus()
	{
		struct row
		{
			std::string_view      Text;
			material_text_kind    Kind;
			material_line_endings Endings;
			bool                  FinalNewline;
		};
		const auto corpus = std::vector<row>{
			{ "", material_text_kind::empty, material_line_endings::none, false },
			{ "a\nb\n", material_text_kind::utf8, material_line_endings::lf, true },
			{ "a\r\nb\r\n", material_text_kind::utf8, material_line_endings::crlf, true },
			{ "a\rb\r", material_text_kind::utf8, material_line_endings::cr, true },
			{ "a\nb\r\nc", material_text_kind::utf8, material_line_endings::mixed, false },
			{ "no final newline", material_text_kind::utf8, material_line_endings::none, false },
			{ "\xEF\xBB\xBFwith bom\n", material_text_kind::utf8_bom, material_line_endings::lf, true },
			{ "caf\xC3\xA9 \xF0\x9F\x98\x80\n", material_text_kind::utf8, material_line_endings::lf, true },
			{ std::string_view{ "\xFF\xFEh\0i\0", 6 }, material_text_kind::unsupported_encoding, material_line_endings::none, false },
			{ std::string_view{ "\xFE\xFF\0h\0i", 6 }, material_text_kind::unsupported_encoding, material_line_endings::none, false },
			{ "overlong \xC0\xAF", material_text_kind::unsupported_encoding, material_line_endings::none, false },
			{ "surrogate \xED\xA0\x80", material_text_kind::unsupported_encoding, material_line_endings::none, false },
			{ "truncated \xE2\x82", material_text_kind::unsupported_encoding, material_line_endings::none, false },
			{ "latin1 caf\xE9\n", material_text_kind::unsupported_encoding, material_line_endings::lf, true },
			{ std::string_view{ "bin\0ary\n", 8 }, material_text_kind::binary, material_line_endings::lf, true },
			{ "trailing cr\r", material_text_kind::utf8, material_line_endings::cr, true },
			{ "lf\ntrailing cr\r", material_text_kind::utf8, material_line_endings::mixed, true },
		};
		for (const auto& item : corpus) {
			const auto profile = classify(item.Text);
			expect(profile.Kind == item.Kind, "text kind");
			expect(profile.LineEndings == item.Endings, "line endings");
			expect(profile.FinalNewline == item.FinalNewline, "final newline");
			expect(profile.Size == item.Text.size(), "size");
			expect(classify_bytewise(item.Text) == profile, "chunking does not change the profile");
		}
		expect(classify("\xEF\xBB\xBF").Kind == material_text_kind::utf8_bom, "BOM alone");
		expect(classify("a\nb").text() && !classify(std::string_view{ "\0", 1 }).text(), "text predicate");
	}

	void recognizes_lfs_pointers_without_running_lfs()
	{
		const auto pointer = std::string_view{ "version https://git-lfs.github.com/spec/v1\noid sha256:4d7a214614ab2935c943f9e0ff69d22eadbb8f32b1258daaa5e2ca24d17e2393\nsize 12345\n" };
		expect(classify(pointer).LfsPointer && classify(pointer).Kind == material_text_kind::utf8, "pointer blob");
		expect(!classify("version https://git-lfs.github.com/spec/v1\nno oid\n").LfsPointer, "incomplete pointer");
		auto large = std::string{ pointer };
		large.append(2048, 'x');
		expect(!classify(large).LfsPointer, "pointers are small");
		expect(!classify("materialized content\n").LfsPointer, "materialized bytes are not a pointer");
	}

	void maps_byte_offsets_and_columns()
	{
		const auto line = std::string_view{ "a\xC3\xA9\xF0\x9F\x98\x80z" }; // a, e-acute, emoji, z
		const auto text = bytes(line);
		expect(utf8_boundary(text, 0) && utf8_boundary(text, 1) && !utf8_boundary(text, 2) && utf8_boundary(text, 3), "boundaries around a two-byte scalar");
		expect(!utf8_boundary(text, 4) && !utf8_boundary(text, 5) && !utf8_boundary(text, 6) && utf8_boundary(text, 7), "inside a four-byte scalar");
		expect(utf8_boundary(text, line.size()) && !utf8_boundary(text, line.size() + 1), "end is a boundary, past it is not");
		expect(utf8_column(text, 0) == 0U && utf8_column(text, 1) == 1U && utf8_column(text, 3) == 2U && utf8_column(text, 7) == 3U && utf8_column(text, 8) == 4U, "columns");
		expect(!utf8_column(text, 2) && !utf8_column(text, 5), "split scalars have no column");
		expect(utf8_offset(text, 0) == 0U && utf8_offset(text, 2) == 3U && utf8_offset(text, 3) == 7U && utf8_offset(text, 4) == 8U, "offsets");
		expect(!utf8_offset(text, 5), "no offset past the end");
		expect(!utf8_column(bytes("\xC0\xAF"), 2), "invalid bytes have no columns");
	}

	void streams_the_same_digest()
	{
		const auto text   = std::string(100'003, 'q') + "tail";
		auto       stream = sha256_stream{ };
		for (std::size_t offset = 0; offset < text.size(); offset += 4093) {
			stream.update(bytes(std::string_view{ text }.substr(offset, 4093)));
		}
		expect(stream.finish() == sha256(bytes(text)), "chunked SHA-256 equals one-shot");
		auto empty = sha256_stream{ };
		expect(empty.finish() == sha256({ }), "empty stream");
	}
}

int main()
{
	classifies_the_byte_golden_corpus();
	recognizes_lfs_pointers_without_running_lfs();
	maps_byte_offsets_and_columns();
	streams_the_same_digest();
	return EXIT_SUCCESS;
}
