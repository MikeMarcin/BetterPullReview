/// \file
/// \brief [[BPR-1358]]. SHA-256 known answers and stream splits.
///
/// \details The block-wise update hashes whole blocks straight from the input.
/// These tests pin it to FIPS 180-4's examples, every length around the
/// padding and block boundaries, a million-byte input, and every way to split
/// one input across `update` calls.
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto hex(const std::array<std::uint8_t, 32>& aDigest) -> std::string
	{
		static constexpr std::string_view digits = "0123456789abcdef";
		auto                              text   = std::string{ };
		for (const auto byte : aDigest) {
			text.push_back(digits[byte >> 4U]);
			text.push_back(digits[byte & 0x0fU]);
		}
		return text;
	}

	[[nodiscard]] auto bytes(const std::string_view aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	/// The same deterministic pattern the expected digests were made from.
	[[nodiscard]] auto pattern(const std::size_t aSize) -> std::vector<std::uint8_t>
	{
		auto data = std::vector<std::uint8_t>(aSize);
		for (std::size_t index = 0U; index < aSize; ++index) {
			data[index] = static_cast<std::uint8_t>(index * 131U + (index >> 7U));
		}
		return data;
	}

	struct known_length
	{
		std::size_t      Size;
		std::string_view Digest;
	};
}

int main()
{
	using bpr::core::sha256;
	using bpr::core::sha256_stream;
	static_assert(std::is_nothrow_copy_constructible_v<sha256_stream>);
	static_assert(std::is_nothrow_copy_assignable_v<sha256_stream>);
	static_assert(std::is_nothrow_move_constructible_v<sha256_stream>);

	// FIPS 180-4 / NIST examples.
	expect(hex(sha256(bytes(""))) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "the empty input");
	expect(hex(sha256(bytes("abc"))) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "\"abc\"");
	expect(hex(sha256(bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")))
			   == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
		   "the 448-bit message");
	expect(hex(sha256(bytes("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu")))
			   == "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1",
		   "the 896-bit message");
	const auto million = std::string(1000000U, 'a');
	expect(hex(sha256(bytes(million))) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "one million 'a'");

	// Every length around the padding (55, 56) and block (64, 128) boundaries.
	static constexpr std::array<known_length, 14> known{ {
		{ 0U, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
		{ 1U, "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d" },
		{ 55U, "5d05a4435b96f43e53e5a582bbb0b8d840a71c023e7468d6604d1c9ae6511c4c" },
		{ 56U, "0851bc0b733318bd14db8098dec9a591c02ee1e72295d3ba54560e56342430fb" },
		{ 57U, "164027362ea2bb80c2a078b3e3b6d98ec04ea963cf48bc39a619e10fd6877f09" },
		{ 63U, "3d825c09f5fe7358fe955248d4bfc207298e5e74c426ccba7c93ac3992d70143" },
		{ 64U, "949f78c7321c5fa8a90f3d236c471950df72d869abc1d36e985cfce9a3ac98b9" },
		{ 65U, "7978b7d18ecfdd9652aba9457bdd4f51dca470d6edfdd08055e2c244b5ae00cf" },
		{ 119U, "f1ae96dacae36aee55d875c7d523334473a78b3e2381a3e7cc7fde2bef3f1938" },
		{ 120U, "7f8c9999be4063260ab4477a8fb27a81bb475cdd1c8e1f3b301e928ecbc272fd" },
		{ 127U, "fe65c2f6bd8f9c9e3dd366f136a8c84ccb4727b9e1e0604ed5d95a5d47099bce" },
		{ 128U, "ad10550a200b2dd5eef8de5edb1fbb508ecdd446aa02d574f73aa2e2c6188c7e" },
		{ 129U, "5717b50849f9a9647998c1e514434f4b9fc9325457b262d2579338dcd8238cb7" },
		{ 1000U, "16808e380aa2627834b5e72f1fd536ec90dc77be33b814d8f1dae903f8ceaebd" },
	} };
	for (const auto& entry : known) {
		expect(hex(sha256(pattern(entry.Size))) == entry.Digest, "the " + std::to_string(entry.Size) + "-byte pattern");
	}
	const auto large = pattern(std::size_t{ 1U } << 20U);
	expect(hex(sha256(large)) == "0e8293bb75f6c74e5a38f7aaea7df966fd447b5c48c4d0a10ff46d4e194ff965", "the 1 MiB pattern");

	// Splitting one input across update calls never changes the digest:
	// every single split point of 200 bytes, then uneven chunks of 1 MiB.
	const auto whole = pattern(200U);
	const auto once  = sha256(whole);
	for (std::size_t split = 0U; split <= whole.size(); ++split) {
		auto stream = sha256_stream{ };
		stream.update(std::span{ whole }.first(split));
		stream.update(std::span{ whole }.subspan(split));
		expect(stream.finish() == once, "a split at byte " + std::to_string(split));
	}
	auto stream = sha256_stream{ };
	auto rest   = std::span{ large };
	auto chunk  = std::size_t{ 1U };
	while (!rest.empty()) {
		const auto take = std::min(chunk, rest.size());
		stream.update(rest.first(take));
		rest  = rest.subspan(take);
		chunk = chunk * 7U % 211U + 1U;
	}
	expect(hex(stream.finish()) == "0e8293bb75f6c74e5a38f7aaea7df966fd447b5c48c4d0a10ff46d4e194ff965", "1 MiB in uneven chunks");
	auto empty = sha256_stream{ };
	empty.update({ });
	expect(hex(empty.finish()) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "an empty update");

	// Copy an unfinished prefix, then diverge independently. Assignment must
	// release its previous native state; destroying a copy must not alias it.
	auto prefix = sha256_stream{ };
	prefix.update(std::span{ large }.first(67U));
	auto copy     = prefix;
	auto assigned = sha256_stream{ };
	assigned.update(bytes("discarded prefix"));
	assigned          = prefix;
	const auto& alias = prefix;
	prefix            = alias;
	copy.update(bytes("copy"));
	assigned.update(bytes("assigned"));
	prefix.update(bytes("original"));
	const auto with_tail = [&](const std::string_view aTail) {
		auto material = std::vector<std::uint8_t>{ large.begin(), large.begin() + 67 };
		material.insert(material.end(), aTail.begin(), aTail.end());
		return sha256(material);
	};
	expect(copy.finish() == with_tail("copy"), "copy retains an independent prefix");
	expect(assigned.finish() == with_tail("assigned"), "assignment replaces an independent prefix");
	expect(prefix.finish() == with_tail("original"), "copy operations preserve the original");
	auto moveSource = sha256_stream{ };
	moveSource.update(bytes("a"));
	auto moved        = std::move(moveSource);
	auto moveAssigned = sha256_stream{ };
	moveAssigned      = std::move(moved);
	moveAssigned.update(bytes("bc"));
	expect(moveAssigned.finish() == sha256(bytes("abc")), "moves transfer an unfinished stream");

	std::cout << "core-sha256-tests:pass\n";
	return EXIT_SUCCESS;
}
