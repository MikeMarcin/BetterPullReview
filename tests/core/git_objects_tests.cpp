#include "bpr/core/git_objects.hpp"
#include "bpr/core/review_cycle_material.hpp"
#include "bpr/core/sha1.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief Canonical Git blob, tree and commit identities in both object
/// formats (RFC-BPR-0059, BPR-1172). Every expected identity was produced by
/// installed Git (`hash-object`, `update-index`/`write-tree`, `commit-tree`).

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

	auto entry_oid(const std::string_view aHex) -> git_entry_oid
	{
		const auto parsed = parse_git_entry_oid(aHex);
		expect(parsed.has_value(), "fixture identity parses");
		return *parsed;
	}

	void sha1_vectors()
	{
		expect(hex20(sha1(bytes(""))) == "da39a3ee5e6b4b0d3255bfef95601890afd80709", "SHA-1 of nothing");
		expect(hex20(sha1(bytes("abc"))) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1 of abc");
		expect(hex20(sha1(bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))) == "84983e441c3bd26ebaae4aa1f95129e5e54670f1", "SHA-1 of the two-block vector");
		auto stream = sha1_stream{ };
		for (const auto character : std::string_view{ "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" }) {
			stream.update(bytes(std::string_view{ &character, 1 }));
		}
		expect(hex20(stream.finish()) == "84983e441c3bd26ebaae4aa1f95129e5e54670f1", "chunking does not change SHA-1");
	}

	void blobs()
	{
		expect(canonical_hex(git_blob_identity(bytes("hello\n"), git_hash_algorithm::sha1)).view() == "ce013625030ba8dba906f756967f9e9ca394464a", "SHA-1 blob");
		expect(canonical_hex(git_blob_identity(bytes("hello\n"), git_hash_algorithm::sha256)).view() == "2cf8d83d9ee29543b34a87727421fdecb7e3f3a183d337639025de576db9ebb4", "SHA-256 blob");
		expect(!git_blob_identity(bytes("hello\n"), git_hash_algorithm::invalid).valid(), "no identity without an object format");
		auto hasher = git_object_hasher{ git_hash_algorithm::sha1, git_object_type::blob, 7U };
		hasher.update(bytes("hello\n"));
		expect(!hasher.finish().has_value(), "a declared size that does not match is refused");
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
}

auto main() -> int
{
	sha1_vectors();
	blobs();
	trees();
	commits();
	std::cout << "git-objects-tests:pass\n";
	return EXIT_SUCCESS;
}
