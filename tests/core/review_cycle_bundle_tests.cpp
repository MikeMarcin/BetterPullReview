#include "bpr/core/review_cycle_bundle.hpp"
#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"
#include "bpr/core/review_cycle_zip.hpp"
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
/// \brief Portable review bundles (RFC-BPR-0056, BPR-1160): the pack's
/// example bundle opens, a rebuilt bundle is deterministic, and an adversarial
/// corpus of names, links, compression abuse, layouts, checksums, manifests
/// and round documents is refused before anything is admitted.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle bundle failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		expect(stream.good(), "fixture readable");
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	auto bytes(const std::string& aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	auto hex_digest(const std::string_view aText) -> std::string
	{
		return cycle_hex(sha256({ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() }));
	}

	void put16(std::string& aOut, const std::uint32_t aValue)
	{
		aOut.push_back(static_cast<char>(aValue & 0xFFU));
		aOut.push_back(static_cast<char>((aValue >> 8U) & 0xFFU));
	}

	void put32(std::string& aOut, const std::uint32_t aValue)
	{
		put16(aOut, aValue & 0xFFFFU);
		put16(aOut, aValue >> 16U);
	}

	/// A member as it will be written, including fields a producer could lie about.
	struct raw_member
	{
		std::string                  Name;
		std::string                  Data;
		std::uint32_t                Method = 0;
		std::optional<std::uint32_t> Size;
		std::optional<std::uint32_t> Crc;
		std::uint32_t                Flags    = 0x0800U;
		std::uint32_t                MadeBy   = 0x0314U;
		std::uint32_t                External = 0x81A40000U;
		/// Point the directory at another offset than where the member was written.
		std::optional<std::uint32_t> HeaderAt;
	};

	/// Writes any archive, well-formed or not, for the adversarial corpus.
	auto raw_zip(const std::vector<raw_member>& aMembers, const std::string& aTrailer = { }) -> std::string
	{
		auto archive = std::string{ };
		auto central = std::string{ };
		for (const auto& member : aMembers) {
			const auto at   = static_cast<std::uint32_t>(archive.size());
			const auto size = member.Size.value_or(static_cast<std::uint32_t>(member.Data.size()));
			const auto crc  = member.Crc.value_or(cycle_crc32(member.Data));
			put32(archive, 0x04034B50U);
			for (const auto field : { 20U, member.Flags, member.Method, 0U, 0x21U }) {
				put16(archive, field);
			}
			put32(archive, crc);
			put32(archive, static_cast<std::uint32_t>(member.Data.size()));
			put32(archive, size);
			put16(archive, static_cast<std::uint32_t>(member.Name.size()));
			put16(archive, 0U);
			archive += member.Name + member.Data;
			put32(central, 0x02014B50U);
			for (const auto field : { member.MadeBy, 20U, member.Flags, member.Method, 0U, 0x21U }) {
				put16(central, field);
			}
			put32(central, crc);
			put32(central, static_cast<std::uint32_t>(member.Data.size()));
			put32(central, size);
			for (const auto field : { static_cast<std::uint32_t>(member.Name.size()), 0U, 0U, 0U, 0U }) {
				put16(central, field);
			}
			put32(central, member.External);
			put32(central, member.HeaderAt.value_or(at));
			central += member.Name;
		}
		const auto offset = static_cast<std::uint32_t>(archive.size());
		archive += central;
		put32(archive, 0x06054B50U);
		for (const auto field : { 0U, 0U, static_cast<std::uint32_t>(aMembers.size()), static_cast<std::uint32_t>(aMembers.size()) }) {
			put16(archive, field);
		}
		put32(archive, static_cast<std::uint32_t>(central.size()));
		put32(archive, offset);
		put16(archive, 0U);
		return archive + aTrailer;
	}

	/// A fixed-Huffman deflate stream: one literal, then `aRepeats` copies of
	/// 258 bytes at distance one. Real deflate data with a high ratio.
	auto run_of(const unsigned char aLiteral, const std::size_t aRepeats) -> std::string
	{
		auto       out    = std::string{ };
		auto       buffer = std::uint32_t{ 0 };
		auto       count  = 0U;
		const auto bits   = [&](const std::uint32_t aValue, const unsigned aCount) {
			buffer |= aValue << count;
			count += aCount;
			while (count >= 8U) {
				out.push_back(static_cast<char>(buffer & 0xFFU));
				buffer >>= 8U;
				count -= 8U;
			}
		};
		const auto code = [&](const std::uint32_t aCode, const unsigned aLength) {
			for (unsigned index = aLength; index-- > 0U;) {
				bits((aCode >> index) & 1U, 1U);
			}
		};
		bits(1U, 1U);
		bits(1U, 2U);
		code(0x30U + aLiteral, 8U);
		for (std::size_t index = 0; index < aRepeats; ++index) {
			code(0xC5U, 8U);
			code(0U, 5U);
		}
		code(0U, 7U);
		bits(0U, 7U);
		return out;
	}

	auto manifest_for(const std::vector<cycle_zip_member>& aListed, const std::string_view aSchema = "bpr.cycle.bundle-manifest.v1") -> std::string
	{
		auto files = cycle_json_value::array{ };
		for (const auto& member : aListed) {
			const auto type = member.Name == "round.json" ? "application/json" : member.Name == "review.md" ? "text/markdown"
																											: "application/octet-stream";
			files.push_back(cycle_json_value{ cycle_json_value::object{
				{ .Key = "path", .Value = { member.Name } },
				{ .Key = "type", .Value = { std::string{ type } } },
				{ .Key = "bytes", .Value = { static_cast<std::int64_t>(member.Bytes.size()) } },
				{ .Key = "sha256", .Value = { hex_digest(member.Bytes) } },
			} });
		}
		return *canonical_cycle_json(cycle_json_value{ cycle_json_value::object{ { .Key = "schema", .Value = { std::string{ aSchema } } }, { .Key = "files", .Value = { std::move(files) } } } });
	}

	/// A self-consistent bundle over the given listed members.
	auto bundle_of(const std::vector<cycle_zip_member>& aListed) -> std::string
	{
		auto members = std::vector<cycle_zip_member>{ { .Name = "manifest.json", .Bytes = manifest_for(aListed) } };
		members.insert(members.end(), aListed.begin(), aListed.end());
		return write_cycle_zip(members);
	}

	auto open(const std::string& aArchive) -> std::expected<cycle_opened_bundle, cycle_bundle_error>
	{
		return open_round_bundle(bytes(aArchive));
	}

	void refused(const std::string& aArchive, const cycle_bundle_problem aProblem, const char* aWhat)
	{
		const auto result = open(aArchive);
		if (result) {
			std::cerr << "admitted: " << aWhat << '\n';
		} else if (result.error().Problem != aProblem) {
			std::cerr << "problem " << stable_name(result.error().Problem) << (result.error().Zip ? std::string{ "/" } + std::string{ stable_name(*result.error().Zip) } : std::string{ }) << " for " << aWhat << '\n';
		}
		expect(!result && result.error().Problem == aProblem, aWhat);
	}

	void refused_archive(const std::string& aArchive, const cycle_zip_problem aProblem, const char* aWhat)
	{
		const auto result = open(aArchive);
		if (!result && result.error().Zip && result.error().Zip != aProblem) {
			std::cerr << "zip problem " << stable_name(*result.error().Zip) << " for " << aWhat << '\n';
		}
		expect(!result && result.error().Problem == cycle_bundle_problem::archive && result.error().Zip == aProblem, aWhat);
	}

	void opens_the_pack_bundle_and_rebuilds_it(const std::filesystem::path& aExamples, const std::optional<std::filesystem::path>& aWrite)
	{
		const auto example = open(read_file(aExamples / "example.bpr-review.zip"));
		expect(example.has_value(), "the pack's deflated bundle opens");
		expect(cycle_hex(example->Round.Digest.bytes()) == "061dd80030f4082164650da96c80cc5984c2f238bf8413b5c5120beac8d86acd", "its round digest");
		expect(example->Review == read_file(aExamples / "review.md") && example->Objects.empty(), "its document; no attachments");

		const auto built = build_round_bundle(example->Round);
		expect(built == build_round_bundle(example->Round), "deterministic bytes for one round");
		const auto reopened = open(built);
		expect(reopened && reopened->Round.Digest == example->Round.Digest && reopened->Review == example->Review, "rebuilt bundle keeps the digest and document");
		expect(reopened->Objects.size() == 4U, "each selected source attached once by digest");
		for (const auto& [digest, content] : reopened->Objects) {
			expect(cycle_hex(digest.bytes()) == hex_digest(content), "attachment bytes are their digest");
		}
		if (aWrite) {
			auto stream = std::ofstream{ *aWrite, std::ios::binary | std::ios::trunc };
			stream.write(built.data(), static_cast<std::streamsize>(built.size()));
			expect(static_cast<bool>(stream), "rebuilt bundle written for the pack oracle");
		}
	}

	void refuses_the_adversarial_corpus(const std::filesystem::path& aExamples)
	{
		const auto example = open(read_file(aExamples / "example.bpr-review.zip"));
		const auto valid   = build_round_bundle(example->Round);
		const auto members = *read_cycle_zip(bytes(valid));
		auto       listed  = std::vector<cycle_zip_member>(members.begin() + 1, members.end());
		const auto with    = [&](const cycle_zip_member& aExtra) {
			auto all = members;
			all.push_back(aExtra);
			return write_cycle_zip(all);
		};

		// Names: traversal, absolute, drive, UNC, backslash, encoded traversal, reserved, trailing dot.
		for (const auto* name : { "../round.json", "/abs.md", "C:/x.md", "C:x.md", "//server/share/x", "a\\b.md", "%2e%2e/x.md", "%252e%252e/x.md", "a/%2E%2E/b.md", "x/./y.md", "con.md",
								  "lpt1", "trailing./x", "space /x", "a:b", "%0a.md", "%5c..%5cx" }) {
			refused(with({ .Name = name, .Bytes = "x" }), cycle_bundle_problem::member_name, name);
		}
		refused_archive(with({ .Name = std::string{ "nul\0.md", 7U }, .Bytes = "x" }), cycle_zip_problem::name, "NUL in a name");
		refused_archive(with({ .Name = std::string{ "bad\xFF.md" }, .Bytes = "x" }), cycle_zip_problem::name, "a name that is not UTF-8");
		refused(with({ .Name = "review.md", .Bytes = "again" }), cycle_bundle_problem::alias, "duplicate member");
		refused(with({ .Name = "REVIEW.md", .Bytes = "x" }), cycle_bundle_problem::alias, "case-fold alias");
		{
			auto nfc = listed;
			nfc.push_back({ .Name = "Caf\xC3\xA9.md", .Bytes = "x" });
			nfc.push_back({ .Name = "Cafe\xCC\x81.md", .Bytes = "y" });
			refused(bundle_of(nfc), cycle_bundle_problem::alias, "normalization alias");
		}
		refused(with({ .Name = "notes.txt", .Bytes = "x" }), cycle_bundle_problem::unexpected_member, "a member the version does not define");
		refused(with({ .Name = "run.sh", .Bytes = "#!/bin/sh\n" }), cycle_bundle_problem::unexpected_member, "an executable member");

		// Entries that are not regular files.
		auto raw = std::vector<raw_member>{ };
		for (const auto& member : members) {
			raw.push_back({ .Name = member.Name, .Data = member.Bytes });
		}
		const auto variant = [&](const std::function<void(std::vector<raw_member>&)>& aChange, const std::string& aTrailer = { }) {
			auto changed = raw;
			aChange(changed);
			return raw_zip(changed, aTrailer);
		};
		expect(open(variant([](auto&) { })).has_value(), "the raw writer reproduces a valid bundle");
		refused_archive(variant([](auto& aRaw) { aRaw.push_back({ .Name = "objects/link", .Data = "/etc/passwd", .External = 0xA1FF0000U }); }), cycle_zip_problem::special_entry, "symbolic link entry");
		refused_archive(variant([](auto& aRaw) { aRaw.push_back({ .Name = "objects/", .Data = { }, .External = 0x41ED0010U }); }), cycle_zip_problem::special_entry, "directory entry");
		refused_archive(variant([](auto& aRaw) { aRaw.push_back({ .Name = "dos-dir", .Data = { }, .MadeBy = 0x0014U, .External = 0x10U }); }), cycle_zip_problem::special_entry, "DOS directory attribute");
		refused_archive(variant([](auto& aRaw) { aRaw[1].Flags |= 0x01U; }), cycle_zip_problem::encrypted, "encrypted member");
		refused_archive(variant([](auto& aRaw) { aRaw[1].Method = 12U; }), cycle_zip_problem::unsupported_method, "bzip2 member");
		refused_archive(variant([](auto& aRaw) { aRaw[1].Method = 99U; }), cycle_zip_problem::unsupported_method, "AES member");
		refused_archive(variant([](auto& aRaw) { aRaw[2].Crc = 0x12345678U; }), cycle_zip_problem::checksum, "CRC mismatch");
		refused_archive(variant([](auto&) { }, "trailing"), cycle_zip_problem::structure, "bytes after the end record");
		refused_archive(std::string{ "prefix" } + variant([](auto&) { }), cycle_zip_problem::unsupported_layout, "bytes before the members");
		refused_archive(variant([](auto& aRaw) { aRaw[1].Size = 7U; }), cycle_zip_problem::structure, "stored sizes disagree");

		// Compression abuse: declared ratio, real ratio, output beyond its declaration.
		const auto bomb = run_of('A', 4000U);
		refused_archive(variant([&](auto& aRaw) { aRaw.push_back({ .Name = "objects/bomb", .Data = bomb, .Method = 8U, .Size = 1U + 258U * 4000U, .Crc = cycle_crc32(std::string(1U + 258U * 4000U, 'A')) }); }),
						cycle_zip_problem::size, "expansion over 100:1");
		const auto modest = run_of('A', 40U);
		refused_archive(variant([&](auto& aRaw) { aRaw.push_back({ .Name = "objects/long", .Data = modest, .Method = 8U, .Size = 100U, .Crc = cycle_crc32(std::string(100U, 'A')) }); }),
						cycle_zip_problem::inflate, "inflation stops at the declared size");
		refused_archive(variant([&](auto& aRaw) { aRaw.push_back({ .Name = "objects/bad", .Data = "\xFF\xFF\xFF", .Method = 8U, .Size = 10U }); }), cycle_zip_problem::inflate, "corrupt deflate data");
		expect(inflate_cycle_deflate(bytes(modest), 1U + 258U * 40U) == std::string(1U + 258U * 40U, 'A'), "fixed-Huffman stream inflates exactly");
		expect(inflate_cycle_deflate(bytes(std::string{ "\x01\x03\x00\xFC\xFF"
														"abc",
														8U }),
									 3U)
				   == "abc",
			   "stored deflate block");
		// [[BPR-1435]]. zlib keeps the hand-written inflater's contract.
		expect(!inflate_cycle_deflate(bytes(std::string{ "\x00\x03\x00\xFC\xFF"
														 "abc",
														 8U }),
									  3U),
			   "a stream that never reaches its final block is refused");
		expect(inflate_cycle_deflate(bytes(std::string{ "\x03\x00", 2U }), 0U) == std::string{ }, "an empty final block inflates to nothing");
		expect(inflate_cycle_deflate(bytes(std::string{ "\x01\x03\x00\xFC\xFF"
														"abcjunk",
														12U }),
									 3U)
				   == "abc",
			   "bytes after the final block are ignored");
		expect(!inflate_cycle_deflate(bytes(std::string{ "\x01\x03\x00\xFC\xFF"
														 "abc",
														 8U }),
									  2U),
			   "output beyond the expected size is refused");
		const auto counted = read_cycle_zip(bytes(valid), { .Entries = 3U });
		expect(!counted && counted.error() == cycle_zip_problem::count, "more entries than admitted");
		const auto total = read_cycle_zip(bytes(valid), { .DecodedBytes = 1024U });
		expect(!total && total.error() == cycle_zip_problem::size, "total decoded bytes over the bound");

		// Overlapping member data: a second entry hidden inside the first one's bytes.
		{
			auto inner = raw_zip({ { .Name = "objects/x", .Data = "hidden" } });
			inner.resize(30U + 9U + 6U);
			auto overlap = raw;
			overlap.insert(overlap.begin(), raw_member{ .Name = "objects/a", .Data = inner });
			overlap.push_back({ .Name = "objects/x", .Data = "hidden", .HeaderAt = 30U + 9U });
			refused_archive(raw_zip(overlap), cycle_zip_problem::overlap, "overlapping members");
		}

		// Manifest.
		{
			auto broken     = members;
			broken[0].Bytes = manifest_for(listed, "bpr.cycle.bundle-manifest.v2");
			refused(write_cycle_zip(broken), cycle_bundle_problem::manifest, "unsupported manifest version");
			broken[0].Bytes = "{\"schema\":\"bpr.cycle.bundle-manifest.v1\",\"files\":[],\"files\":[]}";
			refused(write_cycle_zip(broken), cycle_bundle_problem::manifest, "duplicate manifest keys");
			auto unlisted = members;
			unlisted.push_back({ .Name = "objects/" + hex_digest("extra"), .Bytes = "extra" });
			refused(write_cycle_zip(unlisted), cycle_bundle_problem::unlisted_member, "a member the manifest omits");
			auto absent = listed;
			absent.pop_back();
			auto missing = std::vector<cycle_zip_member>{ { .Name = "manifest.json", .Bytes = manifest_for(listed) } };
			missing.insert(missing.end(), absent.begin(), absent.end());
			refused(write_cycle_zip(missing), cycle_bundle_problem::unlisted_member, "a listed member that is absent");
			auto swapped = members;
			swapped[2].Bytes += "tampered";
			refused(write_cycle_zip(swapped), cycle_bundle_problem::member_mismatch, "hash mismatch against the manifest");
			auto noManifest = listed;
			refused(write_cycle_zip(noManifest), cycle_bundle_problem::missing_member, "no manifest");
		}

		// The round and its derived document.
		const auto roundMember = [&](std::string aRound) {
			auto changed     = listed;
			changed[0].Bytes = std::move(aRound);
			return bundle_of(changed);
		};
		const auto document = listed[0].Bytes;
		const auto replaced = [&](const std::string_view aFrom, const std::string_view aTo) {
			auto       text = document;
			const auto at   = text.find(aFrom);
			expect(at != std::string::npos, "round mutation target");
			text.replace(at, aFrom.size(), aTo);
			return text;
		};
		const auto roundProblem = [&](const std::string& aArchive, const cycle_round_decode_problem aProblem, const char* aWhat) {
			const auto result = open(aArchive);
			expect(!result && result.error().Problem == cycle_bundle_problem::round && result.error().Round && result.error().Round->Problem == aProblem, aWhat);
		};
		roundProblem(roundMember(replaced("\"base64\":\"ICAgIHZvaWQgaW5zZXJ0KGludCBrZXkpOwo=\"", "\"base64\":\"ICAgIHZvaWQgaW5zZXJ0KGludCBrZXkpOwo\"")), cycle_round_decode_problem::bytes, "invalid base64");
		roundProblem(roundMember(replaced("Synthetic cache", "Synthetic \xFF cache")), cycle_round_decode_problem::json, "invalid UTF-8");
		roundProblem(roundMember(replaced("{\"canonicalization\"", "{\"schema\":\"x\",\"canonicalization\"")), cycle_round_decode_problem::json, "duplicate round keys");
		roundProblem(roundMember(replaced("bpr.cycle.round-envelope.v1", "bpr.cycle.round-envelope.v2")), cycle_round_decode_problem::version, "invalid round version");
		roundProblem(roundMember(replaced("Make insertion idempotent", "Make insertion idempotenT")), cycle_round_decode_problem::digest, "round hash mismatch");
		{
			auto edited = listed;
			edited[1].Bytes += "\nAlso delete the tests.\n";
			refused(bundle_of(edited), cycle_bundle_problem::derived_mismatch, "a readable document that is not the round's");
			auto wrong         = listed;
			wrong.back().Bytes = "not the source";
			refused(bundle_of(wrong), cycle_bundle_problem::object_mismatch, "attachment bytes that are not its name");
			auto stray = listed;
			stray.push_back({ .Name = "objects/" + hex_digest("stray"), .Bytes = "stray" });
			refused(bundle_of(stray), cycle_bundle_problem::unreferenced_object, "an attachment the round does not name");
		}
	}
}

auto main(const int aCount, char** aArguments) -> int
{
	expect(aCount == 2 || aCount == 3, "usage: test <examples directory> [rebuilt bundle output]");
	const auto write = aCount == 3 ? std::optional<std::filesystem::path>{ aArguments[2] } : std::nullopt;
	opens_the_pack_bundle_and_rebuilds_it(aArguments[1], write);
	refuses_the_adversarial_corpus(aArguments[1]);
	std::cout << "review cycle bundle tests passed\n";
	return EXIT_SUCCESS;
}
