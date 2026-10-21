#include "bpr/core/git_objects.hpp"
#include "bpr/core/review_cycle_candidate.hpp"
#include "bpr/core/review_cycle_material.hpp"
#include "bpr/core/review_cycle_outgoing.hpp"
#include "bpr/core/sha1.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief Canonical Git blob, tree and commit identities in both object
/// formats (RFC-BPR-0059, BPR-1172). Every expected identity was produced by
/// installed Git (`hash-object`, `update-index`/`write-tree`, `commit-tree`).
/// [[BPR-1450]]: the SHA-1 is sha1collisiondetection, which refuses the
/// public collision pairs (tests/fixtures/sha1-collisions) as raw streams.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "git objects failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto bytes(const std::string_view aText) -> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	auto hex20(const std::array<std::uint8_t, 20>& aDigest) -> std::string
	{
		constexpr auto digits = std::string_view{ "0123456789abcdef" };
		auto           text   = std::string{ };
		for (const auto byte : aDigest) {
			text += digits[byte >> 4U];
			text += digits[byte & 15U];
		}
		return text;
	}

	auto digest_hex(const sha1_result& aDigest) -> std::string
	{
		expect(aDigest.has_value(), "an ordinary input has a SHA-1 digest");
		return hex20(*aDigest);
	}

	auto read_file(const std::filesystem::path& aPath) -> std::vector<std::uint8_t>
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		expect(static_cast<bool>(stream), "collision fixture readable");
		const auto text = std::string{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
		return { text.begin(), text.end() };
	}

	auto entry_oid(const std::string_view aHex) -> git_entry_oid
	{
		const auto parsed = parse_git_entry_oid(aHex);
		expect(parsed.has_value(), "fixture identity parses");
		return *parsed;
	}

	void sha1_vectors()
	{
		expect(digest_hex(sha1(bytes(""))) == "da39a3ee5e6b4b0d3255bfef95601890afd80709", "SHA-1 of nothing");
		expect(digest_hex(sha1(bytes("abc"))) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1 of abc");
		expect(digest_hex(sha1(bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))) == "84983e441c3bd26ebaae4aa1f95129e5e54670f1", "SHA-1 of the two-block vector");
		auto stream = sha1_stream{ };
		for (const auto character : std::string_view{ "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" }) {
			stream.update(bytes(std::string_view{ &character, 1 }));
		}
		expect(digest_hex(stream.finish()) == "84983e441c3bd26ebaae4aa1f95129e5e54670f1", "chunking does not change SHA-1");
		auto copied = sha1_stream{ };
		copied.update(bytes("ab"));
		auto copy = copied;
		copy.update(bytes("c"));
		expect(digest_hex(copy.finish()) == "a9993e364706816aba3e25717850c26c9cd0d89d", "a copied stream carries its state");
		const auto million = std::vector<std::uint8_t>(1'000'000U, static_cast<std::uint8_t>('a'));
		expect(digest_hex(sha1(million)) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f", "SHA-1 of a million a (FIPS 180 vector)");
	}

	/// [[BPR-1450]], RFC-BPR-0080 AC03. Each public pair collides under plain
	/// SHA-1 (the fixture README's control: both members share the digest
	/// recorded here, from a SHA-1 without detection). The core SHA-1 refuses
	/// every member as a raw byte stream, whole or in pieces, and never gives
	/// a digest for it.
	void collisions(const std::filesystem::path& aFixtures)
	{
		struct pair_case
		{
			std::string_view First;
			std::string_view Second;
			std::string_view PlainSha1;
		};
		constexpr auto pairs = std::array{
			pair_case{ "shattered-1-prefix-320.bin", "shattered-2-prefix-320.bin", "f92d74e3874587aaf443d1db961d4e26dde13e9c" },
			pair_case{ "sha-mbles-1.bin", "sha-mbles-2.bin", "8ac60ba76f1999a1ab70223f225aefdc78d4ddc0" },
		};
		for (const auto& entry : pairs) {
			const auto first  = read_file(aFixtures / entry.First);
			const auto second = read_file(aFixtures / entry.Second);
			expect(first.size() == second.size() && first != second, "the two members of a pair differ");
			expect(entry.PlainSha1.size() == 40U, "the control digest is a whole SHA-1");
			for (const auto* member : { &first, &second }) {
				const auto whole = sha1(*member);
				expect(!whole && whole.error() == sha1_refusal::collision_detected, "the one-shot SHA-1 refuses a collision member");
				for (const auto piece : { std::size_t{ 1 }, std::size_t{ 7 }, std::size_t{ 64 }, std::size_t{ 100 } }) {
					auto stream = sha1_stream{ };
					for (std::size_t offset = 0U; offset < member->size(); offset += piece) {
						stream.update(std::span{ *member }.subspan(offset, std::min(piece, member->size() - offset)));
					}
					const auto streamed = stream.finish();
					expect(!streamed && streamed.error() == sha1_refusal::collision_detected, "the stream refuses a collision member in any pieces");
				}
			}
		}
		expect(name(sha1_refusal::collision_detected) == "collision_detected", "the refusal names collision detection");
	}

	/// Git (sha1collisiondetection too) gives these blobs identities: framed
	/// by the blob header the collision blocks no longer line up, so neither
	/// Git nor the core detects anything and both agree.
	void framed_collision_inputs(const std::filesystem::path& aFixtures)
	{
		constexpr auto expected = std::array{
			std::pair{ std::string_view{ "shattered-1-prefix-320.bin" }, std::string_view{ "ef380704685cc8e54de9bc13556d1ff7026ec0cc" } },
			std::pair{ std::string_view{ "shattered-2-prefix-320.bin" }, std::string_view{ "6e98aef8bba6ff517f5b164d7418c5e2a6cf90c9" } },
			std::pair{ std::string_view{ "sha-mbles-1.bin" }, std::string_view{ "5a7c30e97646c66422abe0a9793a5fcb9f1cf8d6" } },
			std::pair{ std::string_view{ "sha-mbles-2.bin" }, std::string_view{ "fe39178400a7ebeedca8ccfd0f3a64ceecdb9cda" } },
		};
		for (const auto& [file, identity] : expected) {
			const auto blob = git_blob_identity(read_file(aFixtures / file), git_hash_algorithm::sha1);
			expect(blob && canonical_hex(*blob).view() == identity, "a framed collision member's blob identity equals hash-object's");
		}
	}

	/// Every identity API reports a detection as its own refusal value, named
	/// for collision detection, never as a digest or an invalid identity.
	void refusal_reasons()
	{
		expect(name(git_object_refusal::collision_detected) == "collision_detected", "the object refusal names collision detection");
		expect(name(git_object_refusal::algorithm) == "algorithm" && name(git_object_refusal::size) == "size", "the other object refusals keep their names");
		expect(git_object_digest(git_hash_algorithm::invalid, git_object_type::blob, bytes("x")).error() == git_object_refusal::algorithm, "no identity without a format");
		expect(git_blob_identity(bytes("x"), git_hash_algorithm::invalid).error() == git_object_refusal::algorithm, "no blob identity without a format");
		expect(git_tree_error::collision_detected != git_tree_error::algorithm, "a tree refusal of its own");
		expect(git_commit_error::collision_detected != git_commit_error::algorithm, "a commit refusal of its own");
		expect(verified_commit_error::collision_detected != verified_commit_error::identity, "a verification refusal of its own");
		expect(name(candidate_limit::collision_detected) == "collision_detected", "a candidate refusal names collision detection");
		expect(name(outgoing_refusal::collision_detected) == "collision_detected", "a plan refusal names collision detection");
	}

	void blobs()
	{
		expect(canonical_hex(git_blob_identity(bytes("hello\n"), git_hash_algorithm::sha1).value()).view() == "ce013625030ba8dba906f756967f9e9ca394464a", "SHA-1 blob");
		expect(canonical_hex(git_blob_identity(bytes("hello\n"), git_hash_algorithm::sha256).value()).view() == "2cf8d83d9ee29543b34a87727421fdecb7e3f3a183d337639025de576db9ebb4", "SHA-256 blob");
		expect(!git_blob_identity(bytes("hello\n"), git_hash_algorithm::invalid).has_value(), "no identity without an object format");
		auto hasher = git_object_hasher{ git_hash_algorithm::sha1, git_object_type::blob, 7U };
		hasher.update(bytes("hello\n"));
		expect(hasher.finish().error() == git_object_refusal::size, "a declared size that does not match is refused");
	}

	auto listing(const git_hash_algorithm aAlgorithm) -> std::vector<git_tree_entry>
	{
		const auto sha1  = aAlgorithm == git_hash_algorithm::sha1;
		const auto hello = entry_oid(sha1 ? "ce013625030ba8dba906f756967f9e9ca394464a" : "2cf8d83d9ee29543b34a87727421fdecb7e3f3a183d337639025de576db9ebb4");
		const auto run   = entry_oid(sha1 ? "1a2485251c33a70432394c93fb89330ef214bfc9" : "1249034e3cf9007362d695b09b1fbdb4c578903bf10b665749b94743f8177ce1");
		const auto link  = entry_oid(sha1 ? "1de565933b05f74c75ff9a6520af5f9f8a5a2f1d" : "fed183069fe19669f65384b7a5d87b447abf0a29591abf6800df208a55af5b02");
		// Path-byte order: "a.b" < "a/x" < "a0", which is Git's tree order.
		return {
			{ .Path = "README", .Mode = git_mode_regular, .Object = hello },
			{ .Path = "a.b", .Mode = git_mode_regular, .Object = hello },
			{ .Path = "a/x", .Mode = git_mode_regular, .Object = hello },
			{ .Path = "a0", .Mode = git_mode_regular, .Object = hello },
			{ .Path = "bin/link", .Mode = git_mode_symlink, .Object = link },
			{ .Path = "bin/run.sh", .Mode = git_mode_executable, .Object = run },
		};
	}

	void trees()
	{
		const auto empty = build_git_trees({ }, git_hash_algorithm::sha1);
		expect(empty && canonical_hex(empty->Root).view() == "4b825dc642cb6eb9a060e54bf8d69288fbee4904", "the SHA-1 empty tree");
		const auto empty256 = build_git_trees({ }, git_hash_algorithm::sha256);
		expect(empty256 && canonical_hex(empty256->Root).view() == "6ef19b41225c5369f1c104d45d8d85efa9b057b53b14b4b9b939dd74decc5321", "the SHA-256 empty tree");

		const auto built = build_git_trees(listing(git_hash_algorithm::sha1), git_hash_algorithm::sha1);
		expect(built && canonical_hex(built->Root).view() == "b5358577b95747ac0e3ba9a7cc4612da70ebf3bc", "nested SHA-1 tree equals write-tree");
		expect(built->Trees.size() == 3U && built->Trees.back().Path.empty() && built->Trees.back().Id == built->Root, "children first, root last");
		const auto built256 = build_git_trees(listing(git_hash_algorithm::sha256), git_hash_algorithm::sha256);
		expect(built256 && canonical_hex(built256->Root).view() == "25face35eaaa9b2be325829807b04f7a472ce66ba9577389ac1fddf700a840fb", "nested SHA-256 tree equals write-tree");

		auto unsorted = listing(git_hash_algorithm::sha1);
		std::swap(unsorted[1], unsorted[2]);
		expect(build_git_trees(unsorted, git_hash_algorithm::sha1).error() == git_tree_error::order, "unsorted listings are refused");
		auto duplicate = listing(git_hash_algorithm::sha1);
		duplicate[1]   = duplicate[0];
		expect(!build_git_trees(duplicate, git_hash_algorithm::sha1).has_value(), "duplicates are refused");
		auto conflict = listing(git_hash_algorithm::sha1);
		conflict.insert(conflict.begin() + 1, { .Path = "a", .Mode = git_mode_regular, .Object = conflict[0].Object });
		expect(build_git_trees(conflict, git_hash_algorithm::sha1).error() == git_tree_error::file_directory_conflict, "a file and a folder at one path");
		auto mode    = listing(git_hash_algorithm::sha1);
		mode[0].Mode = 0100664U;
		expect(build_git_trees(mode, git_hash_algorithm::sha1).error() == git_tree_error::mode, "legacy modes are not written");
		expect(build_git_trees(listing(git_hash_algorithm::sha1), git_hash_algorithm::sha256).error() == git_tree_error::algorithm, "identities of another format");
		auto dots    = listing(git_hash_algorithm::sha1);
		dots[0].Path = "../README";
		expect(build_git_trees(dots, git_hash_algorithm::sha1).error() == git_tree_error::path, "dot segments");
	}

	void commits()
	{
		const auto tree   = build_git_trees(listing(git_hash_algorithm::sha1), git_hash_algorithm::sha1)->Root;
		auto       commit = git_commit_content{
			.Tree      = tree,
			.Author    = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000000, .OffsetMinutes = 90 },
			.Committer = { .Name = "BPR Publisher", .Email = "bpr@example.invalid", .Seconds = 1700000100, .OffsetMinutes = -480 },
			.Message   = "Improve parsing\n",
		};
		const auto first = git_commit_identity(commit);
		expect(first && canonical_hex(*first).view() == "a45fbcb979e80a11e91371a41a86ea707b552ee4", "root commit equals commit-tree");
		const auto second = git_commit_identity({
			.Tree      = tree,
			.Parents   = { *first },
			.Author    = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000000 },
			.Committer = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000000 },
			.Message   = "Second\n",
		});
		expect(second && canonical_hex(*second).view() == "a18180d187116b193478f0fdf147af7d95bc1a32", "one-parent commit equals commit-tree");

		const auto tree256   = build_git_trees(listing(git_hash_algorithm::sha256), git_hash_algorithm::sha256)->Root;
		auto       commit256 = commit;
		commit256.Tree       = tree256;
		const auto first256  = git_commit_identity(commit256);
		expect(first256 && canonical_hex(*first256).view() == "4634483cc0d446fc84fb81dd14595ef081edd24cea3c5b7836fc8a752cdd9610", "SHA-256 commit equals commit-tree");
		auto mixed    = commit256;
		mixed.Parents = { *first };
		expect(git_commit_identity(mixed).error() == git_commit_error::algorithm, "a SHA-1 parent under a SHA-256 tree");

		auto bad        = commit;
		bad.Author.Name = "Ada <x>";
		expect(git_commit_identity(bad).error() == git_commit_error::signature, "angle brackets in a name");
		bad              = commit;
		bad.Author.Email = " ada@example.invalid";
		expect(git_commit_identity(bad).error() == git_commit_error::signature, "surrounding space");
		bad                         = commit;
		bad.Committer.OffsetMinutes = 24 * 60;
		expect(git_commit_identity(bad).error() == git_commit_error::signature, "an offset of a day");
		bad         = std::move(commit);
		bad.Message = "no final newline";
		expect(git_commit_identity(bad).error() == git_commit_error::message, "the message ends with a line feed");
		bad.Message = std::string{ "nul\0inside\n", 11 };
		expect(git_commit_identity(bad).error() == git_commit_error::message, "NUL in a message");
		bad.Message = "\xff\xfe\n";
		expect(git_commit_identity(bad).error() == git_commit_error::message, "a message that is not UTF-8");
	}

	void verified_commits()
	{
		const auto tree    = build_git_trees(listing(git_hash_algorithm::sha1), git_hash_algorithm::sha1)->Root;
		const auto content = git_commit_content{
			.Tree      = tree,
			.Author    = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000000, .OffsetMinutes = 90 },
			.Committer = { .Name = "BPR Publisher", .Email = "bpr@example.invalid", .Seconds = 1700000100, .OffsetMinutes = -480 },
			.Message   = "Improve parsing\n",
		};
		const auto object = encode_git_commit(content).value();
		const auto id     = git_commit_identity(content).value();
		expect(verified_commit_tree(object, id) == tree, "a commit object's own tree");
		auto changed = object;
		if (changed.empty()) {
			expect(false, "the encoded fixture commit contains bytes");
			return;
		}
		changed.back() = '!';
		expect(verified_commit_tree(changed, id).error() == verified_commit_error::identity, "bytes that do not hash to the commit");
		expect(verified_commit_tree(object, git_commit_oid{ }).error() == verified_commit_error::identity, "no verification against an invalid identity");
		const auto headless   = std::vector<std::uint8_t>{ 'x', '\n' };
		const auto headlessId = git_commit_oid{ git_hash_algorithm::sha1, git_object_digest(git_hash_algorithm::sha1, git_object_type::commit, headless).value() };
		expect(verified_commit_tree(headless, headlessId).error() == verified_commit_error::malformed, "an object without a tree line");
	}
}

auto main(const int aCount, char** aArguments) -> int
{
	if (aCount != 2) {
		std::cerr << "usage: git_objects_tests SHA1_COLLISION_FIXTURES\n";
		return EXIT_FAILURE;
	}
	const auto fixtures = std::filesystem::path{ std::span{ aArguments, static_cast<std::size_t>(aCount) }[1] };
	sha1_vectors();
	collisions(fixtures);
	framed_collision_inputs(fixtures);
	refusal_reasons();
	blobs();
	trees();
	commits();
	verified_commits();
	std::cout << "git-objects-tests:pass\n";
	return EXIT_SUCCESS;
}
