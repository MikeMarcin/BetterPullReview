#include "hash_arms.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
	#include <fcntl.h>
	#include <io.h>
#endif

/// \file
/// \brief [[BPR-1446]]. RFC-BPR-0080's evaluation harness: known answers,
/// chunking, seeded differential inputs, Git object identities, a repository
/// object sweep, the SHA-1 collision inputs and the W1 to W3 throughput
/// workloads, each over every implementation `make_arms` returns.
/// \details Every command writes one JSON object per line on standard output
/// and exits nonzero when a correctness expectation fails. The driver
/// `run_hash_evaluation.py` runs Git, collects the lines and writes the raw
/// results the report cites.
namespace {
	using namespace bpr::hash_evaluation;
	using bytes = std::vector<std::uint8_t>;

	constexpr std::size_t mebibyte = std::size_t{ 1 } << 20U;

	/// xoshiro256** seeded through splitmix64: a fixed stream for a seed.
	class seeded_bytes
	{
	public:
		explicit seeded_bytes(std::uint64_t aSeed)
		{
			for (auto& word : State) {
				aSeed += 0x9E3779B97F4A7C15ULL;
				auto mixed = aSeed;
				mixed      = (mixed ^ (mixed >> 30U)) * 0xBF58476D1CE4E5B9ULL;
				mixed      = (mixed ^ (mixed >> 27U)) * 0x94D049BB133111EBULL;
				word       = mixed ^ (mixed >> 31U);
			}
		}
		auto next() noexcept -> std::uint64_t
		{
			const auto result = std::rotl(State[1] * 5U, 7) * 9U;
			const auto shift  = State[1] << 17U;
			State[2] ^= State[0];
			State[3] ^= State[1];
			State[1] ^= State[2];
			State[0] ^= State[3];
			State[2] ^= shift;
			State[3] = std::rotl(State[3], 45);
			return result;
		}
		auto below(std::uint64_t aBound) noexcept -> std::uint64_t { return aBound == 0U ? 0U : next() % aBound; }
		void fill(std::span<std::uint8_t> aBytes) noexcept
		{
			std::ranges::generate(aBytes, [this] { return static_cast<std::uint8_t>(next() >> 56U); });
		}
		auto make(std::size_t aSize) -> bytes
		{
			bytes result(aSize);
			fill(result);
			return result;
		}

	private:
		std::array<std::uint64_t, 4> State{ };
	};

	auto as_bytes(std::string_view aText) -> std::span<const std::uint8_t>
	{
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): text viewed as its bytes.
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	auto json_text(std::string_view aText) -> std::string
	{
		std::string result = "\"";
		for (const auto character : aText) {
			if (character == '"' || character == '\\') {
				result.push_back('\\');
			}
			result.push_back(character);
		}
		result.push_back('"');
		return result;
	}

	auto algorithm_name(hash_algorithm aAlgorithm) -> std::string_view
	{
		return aAlgorithm == hash_algorithm::sha1 ? "sha1" : "sha256";
	}

	auto from_hex(std::string_view aText) -> std::optional<bytes>
	{
		const auto value = [](char aDigit) -> int {
			if (aDigit >= '0' && aDigit <= '9') {
				return aDigit - '0';
			}
			if (aDigit >= 'a' && aDigit <= 'f') {
				return aDigit - 'a' + 10;
			}
			if (aDigit >= 'A' && aDigit <= 'F') {
				return aDigit - 'A' + 10;
			}
			return -1;
		};
		if (aText.size() % 2U != 0U) {
			return std::nullopt;
		}
		bytes result;
		for (std::size_t at = 0U; at < aText.size(); at += 2U) {
			const auto high = value(aText[at]);
			const auto low  = value(aText[at + 1U]);
			if (high < 0 || low < 0) {
				return std::nullopt;
			}
			result.push_back(static_cast<std::uint8_t>(high * 16 + low));
		}
		return result;
	}

	auto read_file(const std::string& aPath) -> std::optional<bytes>
	{
		std::ifstream file(aPath, std::ios::binary);
		if (!file) {
			return std::nullopt;
		}
		return bytes{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{ } };
	}

	/// Per-arm tally of one correctness command.
	struct tally
	{
		std::string   Arm;
		std::uint64_t Cases      = 0U;
		std::uint64_t Mismatches = 0U;
		std::string   FirstMismatch;
	};

	class tallies
	{
	public:
		auto of(std::string_view aArm) -> tally&
		{
			const auto found = std::ranges::find(Entries, aArm, &tally::Arm);
			if (found != Entries.end()) {
				return *found;
			}
			// A named local, not a designated temporary: GCC 15 at -O3 warns
			// on the defaulted strings of a moved temporary (tests/AGENTS.md).
			tally entry;
			entry.Arm = aArm;
			Entries.push_back(std::move(entry));
			return Entries.back();
		}
		void check(std::string_view aArm, bool aMatched, std::string_view aCase)
		{
			auto& entry = of(aArm);
			++entry.Cases;
			if (!aMatched) {
				++entry.Mismatches;
				if (entry.FirstMismatch.empty()) {
					entry.FirstMismatch = aCase;
				}
			}
		}
		/// Prints one line per arm; true when nothing mismatched.
		auto report(std::string_view aCommand, std::string_view aGroup) const -> bool
		{
			bool clean = true;
			for (const auto& entry : Entries) {
				std::printf("{\"command\":%s,\"group\":%s,\"arm\":%s,\"cases\":%llu,\"mismatches\":%llu,\"first_mismatch\":%s}\n", json_text(aCommand).c_str(),
							json_text(aGroup).c_str(), json_text(entry.Arm).c_str(), static_cast<unsigned long long>(entry.Cases),
							static_cast<unsigned long long>(entry.Mismatches), json_text(entry.FirstMismatch).c_str());
				clean = clean && entry.Mismatches == 0U;
			}
			return clean;
		}

	private:
		std::vector<tally> Entries;
	};

	// ---- Known answers -----------------------------------------------------

	struct message_vector
	{
		bytes       Message;
		bytes       Expected;
		std::string Name;
	};

	/// The Len/Msg/MD records of a CAVP ShortMsg or LongMsg response file.
	auto read_message_vectors(const std::string& aPath) -> std::optional<std::vector<message_vector>>
	{
		std::ifstream file(aPath);
		if (!file) {
			return std::nullopt;
		}
		std::vector<message_vector> result;
		std::optional<std::size_t>  bits;
		bytes                       message;
		for (std::string line; std::getline(file, line);) {
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			const auto field = [&](std::string_view aKey) -> std::optional<std::string_view> {
				const std::string_view text{ line };
				return text.starts_with(aKey) ? std::optional{ text.substr(aKey.size()) } : std::nullopt;
			};
			if (const auto value = field("Len = ")) {
				bits = std::stoull(std::string{ *value });
			} else if (const auto text = field("Msg = ")) {
				auto decoded = from_hex(*text);
				if (!decoded || !bits) {
					return std::nullopt;
				}
				decoded->resize(*bits / 8U);
				message = std::move(*decoded);
			} else if (const auto digestText = field("MD = ")) {
				auto expected = from_hex(*digestText);
				if (!expected || !bits) {
					return std::nullopt;
				}
				message_vector vector;
				vector.Message  = message;
				vector.Expected = std::move(*expected);
				vector.Name     = aPath + " Len=" + std::to_string(*bits);
				result.push_back(std::move(vector));
				bits.reset();
			}
		}
		return result;
	}

	struct monte_vectors
	{
		bytes              Seed;
		std::vector<bytes> Checkpoints;
	};

	auto read_monte_vectors(const std::string& aPath) -> std::optional<monte_vectors>
	{
		std::ifstream file(aPath);
		if (!file) {
			return std::nullopt;
		}
		monte_vectors result;
		for (std::string line; std::getline(file, line);) {
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			const std::string_view text{ line };
			if (text.starts_with("Seed = ")) {
				auto seed = from_hex(text.substr(7U));
				if (!seed) {
					return std::nullopt;
				}
				result.Seed = std::move(*seed);
			} else if (text.starts_with("MD = ")) {
				auto checkpoint = from_hex(text.substr(5U));
				if (!checkpoint) {
					return std::nullopt;
				}
				result.Checkpoints.push_back(std::move(*checkpoint));
			}
		}
		return result;
	}

	/// SHAVS's Monte Carlo test: 100 checkpoints of 1000 chained digests.
	auto run_monte(hash_arm& aArm, const monte_vectors& aVectors, tallies& aTallies, std::string_view aName) -> void
	{
		auto seed = aVectors.Seed;
		for (std::size_t checkpoint = 0U; checkpoint < aVectors.Checkpoints.size(); ++checkpoint) {
			std::array<bytes, 3> previous{ seed, seed, seed };
			bytes                current;
			for (std::size_t round = 3U; round < 1003U; ++round) {
				bytes message;
				for (const auto& part : previous) {
					message.insert(message.end(), part.begin(), part.end());
				}
				const auto result = aArm.hash(message);
				current.assign(result.view().begin(), result.view().end());
				previous[0] = std::move(previous[1]);
				previous[1] = std::move(previous[2]);
				previous[2] = current;
			}
			aTallies.check(aArm.name(), current == aVectors.Checkpoints[checkpoint], std::string{ aName } + " COUNT=" + std::to_string(checkpoint));
			seed = current;
		}
	}

	struct fixed_answer
	{
		hash_algorithm   Algorithm;
		std::string_view Message;
		std::size_t      Repeat = 1U;
		std::string_view Expected;
	};

	/// FIPS 180-4 example messages and their published digests.
	constexpr std::array<fixed_answer, 8> fips_examples{ {
		{ hash_algorithm::sha1, "abc", 1U, "a9993e364706816aba3e25717850c26c9cd0d89d" },
		{ hash_algorithm::sha1, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1U, "84983e441c3bd26ebaae4aa1f95129e5e54670f1" },
		{ hash_algorithm::sha1, "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 1U, "a49b2446a02c645bf419f995b67091253a04a259" },
		{ hash_algorithm::sha1, "a", 1000000U, "34aa973cd4c4daa4f61eeb2bdbad27316534016f" },
		{ hash_algorithm::sha256, "abc", 1U, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
		{ hash_algorithm::sha256, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1U, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
		{ hash_algorithm::sha256, "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 1U, "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1" },
		{ hash_algorithm::sha256, "a", 1000000U, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0" },
	} };

	auto run_known_answers(const std::string& aCavp) -> int
	{
		bool clean = true;
		for (const auto algorithm : { hash_algorithm::sha1, hash_algorithm::sha256 }) {
			auto       arms   = make_arms(algorithm);
			const auto prefix = aCavp + (algorithm == hash_algorithm::sha1 ? "/SHA1" : "/SHA256");
			tallies    results;
			for (const auto* file : { "ShortMsg.rsp", "LongMsg.rsp" }) {
				const auto vectors = read_message_vectors(prefix + file);
				if (!vectors || vectors->empty()) {
					std::printf("{\"command\":\"kat\",\"error\":%s}\n", json_text("unreadable " + prefix + file).c_str());
					return 2;
				}
				for (const auto& vector : *vectors) {
					for (auto& arm : arms) {
						const auto result = arm->hash(vector.Message);
						results.check(arm->name(), std::ranges::equal(result.view(), vector.Expected), vector.Name);
					}
				}
			}
			const auto monte = read_monte_vectors(prefix + "Monte.rsp");
			if (!monte || monte->Checkpoints.size() != 100U) {
				std::printf("{\"command\":\"kat\",\"error\":%s}\n", json_text("unreadable " + prefix + "Monte.rsp").c_str());
				return 2;
			}
			for (auto& arm : arms) {
				run_monte(*arm, *monte, results, prefix + "Monte.rsp");
			}
			for (const auto& example : fips_examples) {
				if (example.Algorithm != algorithm) {
					continue;
				}
				for (auto& arm : arms) {
					arm->reset();
					for (std::size_t repeat = 0U; repeat < example.Repeat; ++repeat) {
						arm->update(as_bytes(example.Message));
					}
					results.check(arm->name(), hex(arm->finish()) == example.Expected, std::string{ "FIPS 180-4 " } + std::string{ example.Message.substr(0U, 8U) });
				}
			}
			clean = results.report("kat", algorithm_name(algorithm)) && clean;
		}
		return clean ? 0 : 1;
	}

	// ---- Chunking ------------------------------------------------------------

	auto pattern(std::size_t aSize) -> bytes
	{
		bytes result(aSize);
		for (std::size_t at = 0U; at < aSize; ++at) {
			result[at] = static_cast<std::uint8_t>((at * 131U + 7U) & 0xFFU);
		}
		return result;
	}

	/// Boundary lengths, every two-update split of 200 bytes, uneven chunks
	/// over 1 MiB and empty updates, each against the baseline's one call.
	auto run_chunking() -> int
	{
		bool clean = true;
		for (const auto algorithm : { hash_algorithm::sha1, hash_algorithm::sha256 }) {
			auto&&     arms     = make_arms(algorithm);
			auto&      baseline = *arms.front();
			auto       platform = make_platform_reference(algorithm);
			tallies    results;
			const auto compare = [&](const bytes& aMessage, const digest& aExpected, std::string_view aCase, auto&& aFeed) {
				for (auto& arm : arms) {
					arm->reset();
					aFeed(*arm, aMessage);
					results.check(arm->name(), arm->finish() == aExpected, aCase);
				}
			};
			const auto               whole = [](hash_arm& aArm, const bytes& aMessage) { aArm.update(aMessage); };
			std::vector<std::size_t> lengths;
			for (std::size_t length = 0U; length <= 130U; ++length) {
				lengths.push_back(length);
			}
			lengths.push_back(1000U);
			lengths.push_back(mebibyte);
			for (const auto length : lengths) {
				const auto message  = pattern(length);
				const auto expected = baseline.hash(message);
				if (platform) {
					results.check("baseline-vs-platform", platform->hash(message) == expected, "length " + std::to_string(length));
				}
				compare(message, expected, "length " + std::to_string(length), whole);
			}
			const auto twoHundred = pattern(200U);
			const auto expected   = baseline.hash(twoHundred);
			for (std::size_t split = 0U; split <= twoHundred.size(); ++split) {
				compare(twoHundred, expected, "split " + std::to_string(split), [split](hash_arm& aArm, const bytes& aMessage) {
					const std::span all{ aMessage };
					aArm.update(all.first(split));
					aArm.update({ });
					aArm.update(all.subspan(split));
				});
			}
			const auto large      = pattern(mebibyte + 17U);
			const auto largeWhole = baseline.hash(large);
			compare(large, largeWhole, "uneven chunks", [](hash_arm& aArm, const bytes& aMessage) {
				constexpr std::array<std::size_t, 12> sizes{ 1U, 2U, 3U, 5U, 7U, 63U, 64U, 65U, 127U, 128U, 4096U, 65537U };
				std::span<const std::uint8_t>         rest{ aMessage };
				for (std::size_t turn = 0U; !rest.empty(); ++turn) {
					const auto take = std::min(rest.size(), sizes[turn % sizes.size()]);
					aArm.update(rest.first(take));
					rest = rest.subspan(take);
				}
			});
			clean = results.report("chunking", algorithm_name(algorithm)) && clean;
		}
		return clean ? 0 : 1;
	}

	// ---- Seeded differential ---------------------------------------------------

	auto run_differential(std::uint64_t aSeed, bool aLarge) -> int
	{
		bool clean = true;
		for (const auto algorithm : { hash_algorithm::sha1, hash_algorithm::sha256 }) {
			seeded_bytes             random{ aSeed + (algorithm == hash_algorithm::sha1 ? 0U : 1U) };
			auto                     arms     = make_arms(algorithm);
			auto&                    baseline = *arms.front();
			auto                     platform = make_platform_reference(algorithm);
			tallies                  results;
			std::vector<std::size_t> lengths;
			for (std::size_t length = 0U; length <= 300U; ++length) {
				lengths.push_back(length);
			}
			for (std::size_t count = 0U; count < 2000U; ++count) {
				lengths.push_back(static_cast<std::size_t>(random.below(mebibyte + 1U)));
			}
			if (aLarge) {
				lengths.insert(lengths.end(), { 16U * mebibyte, 64U * mebibyte, 256U * mebibyte });
			}
			std::uint64_t totalBytes = 0U;
			for (const auto length : lengths) {
				const auto message  = random.make(length);
				const auto expected = baseline.hash(message);
				const auto name     = "length " + std::to_string(length);
				totalBytes += length;
				if (platform) {
					results.check("baseline-vs-platform", platform->hash(message) == expected, name);
				}
				const auto chunkSeed = random.next();
				for (auto& arm : arms) {
					results.check(arm->name(), arm->hash(message) == expected, name + " one update");
					seeded_bytes                  chunks{ chunkSeed };
					std::span<const std::uint8_t> rest{ message };
					arm->reset();
					while (!rest.empty()) {
						const auto take = std::min<std::size_t>(rest.size(), chunks.below(2U) == 0U ? chunks.below(130U) : chunks.below(131072U));
						arm->update(rest.first(take));
						rest = rest.subspan(take);
					}
					results.check(arm->name(), arm->finish() == expected, name + " random chunks");
				}
			}
			std::printf("{\"command\":\"differential\",\"group\":%s,\"seed\":%llu,\"inputs\":%zu,\"bytes\":%llu}\n", json_text(algorithm_name(algorithm)).c_str(),
						static_cast<unsigned long long>(aSeed), lengths.size(), static_cast<unsigned long long>(totalBytes));
			clean = results.report("differential", algorithm_name(algorithm)) && clean;
		}
		return clean ? 0 : 1;
	}

	// ---- Git object identities ---------------------------------------------------

	auto object_header(std::string_view aType, std::size_t aSize) -> std::string
	{
		std::string header{ aType };
		header += ' ';
		header += std::to_string(aSize);
		header.push_back('\0');
		return header;
	}

	auto object_id(hash_arm& aArm, std::string_view aType, std::span<const std::uint8_t> aContent) -> digest
	{
		aArm.reset();
		aArm.update(as_bytes(object_header(aType, aContent.size())));
		aArm.update(aContent);
		return aArm.finish();
	}

	/// The blob identity of each file in both object formats, from every arm.
	auto run_object_ids(std::span<const std::string> aFiles) -> int
	{
		auto sha1Arms   = make_arms(hash_algorithm::sha1);
		auto sha256Arms = make_arms(hash_algorithm::sha256);
		for (const auto& path : aFiles) {
			const auto content = read_file(path);
			if (!content) {
				std::printf("{\"command\":\"object-ids\",\"error\":%s}\n", json_text("unreadable " + path).c_str());
				return 2;
			}
			std::string line = "{\"command\":\"object-ids\",\"file\":" + json_text(path) + ",\"size\":" + std::to_string(content->size());
			for (auto* arms : { &sha1Arms, &sha256Arms }) {
				line += ",\"" + std::string{ algorithm_name(arms->front()->algorithm()) } + "\":{";
				bool first = true;
				for (auto& arm : *arms) {
					const auto id = object_id(*arm, "blob", *content);
					line += (first ? "" : ",") + json_text(arm->name()) + ":" + json_text(hex(id));
					first = false;
					if (arm->detected()) {
						line += ",\"" + std::string{ arm->name() } + "-detected\":true";
					}
				}
				line += "}";
			}
			line += "}\n";
			std::fputs(line.c_str(), stdout);
		}
		return 0;
	}

	// ---- Repository object sweep -------------------------------------------------

	auto read_line(std::FILE* aInput, std::string& aLine) -> bool
	{
		aLine.clear();
		for (int character = std::fgetc(aInput); character != EOF; character = std::fgetc(aInput)) {
			if (character == '\n') {
				return true;
			}
			aLine.push_back(static_cast<char>(character));
		}
		return !aLine.empty();
	}

	/// Reads `git cat-file --batch-all-objects --batch` and recomputes every
	/// object's identity with every arm of the repository's format.
	auto run_sweep(hash_algorithm aAlgorithm) -> int
	{
#if defined(_WIN32)
		static_cast<void>(_setmode(_fileno(stdin), _O_BINARY));
#endif
		auto                         arms = make_arms(aAlgorithm);
		tallies                      results;
		std::uint64_t                objects    = 0U;
		std::uint64_t                totalBytes = 0U;
		std::uint64_t                detections = 0U;
		std::array<std::uint64_t, 4> byType{ };
		std::string                  header;
		bytes                        content;
		while (read_line(stdin, header)) {
			const auto firstSpace  = header.find(' ');
			const auto secondSpace = header.find(' ', firstSpace + 1U);
			if (firstSpace == std::string::npos || secondSpace == std::string::npos) {
				std::printf("{\"command\":\"sweep\",\"error\":%s}\n", json_text("unexpected header " + header).c_str());
				return 2;
			}
			const auto oid  = header.substr(0U, firstSpace);
			const auto type = header.substr(firstSpace + 1U, secondSpace - firstSpace - 1U);
			const auto size = static_cast<std::size_t>(std::stoull(header.substr(secondSpace + 1U)));
			content.resize(size);
			if (std::fread(content.data(), 1U, size, stdin) != size || std::fgetc(stdin) != '\n') {
				std::printf("{\"command\":\"sweep\",\"error\":%s}\n", json_text("truncated object " + oid).c_str());
				return 2;
			}
			++objects;
			totalBytes += size;
			constexpr std::array<std::string_view, 4> types{ "blob", "tree", "commit", "tag" };
			const auto                                kind = std::ranges::find(types, type);
			if (kind != types.end()) {
				++byType[static_cast<std::size_t>(kind - types.begin())];
			}
			for (auto& arm : arms) {
				const auto id = object_id(*arm, type, content);
				results.check(arm->name(), hex(id) == oid, oid);
				if (arm->detected()) {
					++detections;
				}
			}
		}
		std::printf("{\"command\":\"sweep\",\"group\":%s,\"objects\":%llu,\"bytes\":%llu,\"blobs\":%llu,\"trees\":%llu,\"commits\":%llu,\"tags\":%llu,\"detections\":%llu}\n",
					json_text(algorithm_name(aAlgorithm)).c_str(), static_cast<unsigned long long>(objects), static_cast<unsigned long long>(totalBytes),
					static_cast<unsigned long long>(byType[0]), static_cast<unsigned long long>(byType[1]), static_cast<unsigned long long>(byType[2]),
					static_cast<unsigned long long>(byType[3]), static_cast<unsigned long long>(detections));
		const auto clean = results.report("sweep", algorithm_name(aAlgorithm));
		return clean && detections == 0U && objects != 0U ? 0 : 1;
	}

	// ---- Collision inputs ------------------------------------------------------

	/// The SHAttered and SHA-mbles pairs and sha1dc's reduced-round vector:
	/// plain SHA-1 collides, sha1dc detects, and what a Git blob header does
	/// to both.
	auto run_collisions(const std::string& aDirectory) -> int
	{
		struct pair_case
		{
			std::string_view Name;
			std::string_view First;
			std::string_view Second;
		};
		constexpr std::array<pair_case, 2> pairs{ {
			{ "SHAttered", "shattered-1.pdf", "shattered-2.pdf" },
			{ "SHA-mbles", "sha-mbles-1.bin", "sha-mbles-2.bin" },
		} };
		auto                               core      = std::move(make_arms(hash_algorithm::sha1).front());
		auto                               detector  = make_sha1dc(false);
		auto                               reduced   = make_sha1dc(true);
		bool                               satisfied = true;
		for (const auto& entry : pairs) {
			const auto first  = read_file(aDirectory + "/" + std::string{ entry.First });
			const auto second = read_file(aDirectory + "/" + std::string{ entry.Second });
			if (!first || !second) {
				std::printf("{\"command\":\"collision\",\"error\":%s}\n", json_text("unreadable " + std::string{ entry.Name }).c_str());
				return 2;
			}
			const auto plainFirst     = core->hash(*first);
			const auto plainSecond    = core->hash(*second);
			const auto dcFirst        = detector->hash(*first);
			const auto detectedFirst  = detector->detected();
			const auto dcSecond       = detector->hash(*second);
			const auto detectedSecond = detector->detected();
			const auto blobFirst      = object_id(*core, "blob", *first);
			const auto blobSecond     = object_id(*core, "blob", *second);
			static_cast<void>(object_id(*detector, "blob", *first));
			const auto blobDetectedFirst = detector->detected();
			static_cast<void>(object_id(*detector, "blob", *second));
			const auto blobDetectedSecond = detector->detected();
			const bool contentsDiffer     = *first != *second;
			const bool plainCollides      = plainFirst == plainSecond;
			const bool sameDigest         = dcFirst == plainFirst && dcSecond == plainSecond;
			std::printf("{\"command\":\"collision\",\"input\":%s,\"bytes\":[%zu,%zu],\"contents_differ\":%s,\"plain_sha1\":[%s,%s],\"plain_collides\":%s,"
						"\"sha1dc_detected\":[%s,%s],\"sha1dc_digest_equals_plain\":%s,\"blob_ids\":[%s,%s],\"blob_sha1dc_detected\":[%s,%s]}\n",
						json_text(entry.Name).c_str(), first->size(), second->size(), contentsDiffer ? "true" : "false", json_text(hex(plainFirst)).c_str(),
						json_text(hex(plainSecond)).c_str(), plainCollides ? "true" : "false", detectedFirst ? "true" : "false", detectedSecond ? "true" : "false",
						sameDigest ? "true" : "false", json_text(hex(blobFirst)).c_str(), json_text(hex(blobSecond)).c_str(), blobDetectedFirst ? "true" : "false",
						blobDetectedSecond ? "true" : "false");
			satisfied = satisfied && contentsDiffer && plainCollides && detectedFirst && detectedSecond && sameDigest;
		}
		const auto reducedInput = read_file(aDirectory + "/sha1_reducedsha_coll.bin");
		if (!reducedInput) {
			std::printf("{\"command\":\"collision\",\"error\":\"unreadable sha1_reducedsha_coll.bin\"}\n");
			return 2;
		}
		static_cast<void>(detector->hash(*reducedInput));
		const auto fullRoundDetected = detector->detected();
		static_cast<void>(reduced->hash(*reducedInput));
		const auto reducedDetected = reduced->detected();
		std::printf("{\"command\":\"collision\",\"input\":\"sha1_reducedsha_coll.bin\",\"bytes\":%zu,\"sha1dc_detected\":%s,\"sha1dc_reduced_round_detected\":%s}\n",
					reducedInput->size(), fullRoundDetected ? "true" : "false", reducedDetected ? "true" : "false");
		satisfied = satisfied && reducedDetected;
		std::printf("{\"command\":\"collision\",\"satisfied\":%s}\n", satisfied ? "true" : "false");
		return satisfied ? 0 : 1;
	}

	// ---- Throughput --------------------------------------------------------------

	struct workload
	{
		std::string_view Name;
		std::size_t      Bytes  = 0U;
		std::size_t      Update = 0U;
		/// W2: one whole digest per body of `Update` bytes.
		bool Bodies = false;
	};

	auto elapsed_nanoseconds(auto&& aWork) -> std::uint64_t
	{
		const auto start = std::chrono::steady_clock::now();
		aWork();
		const auto stop = std::chrono::steady_clock::now();
		return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count());
	}

	/// W1, W2 and W3 for every arm of both algorithms: \p aRuns timed runs
	/// after one untimed warm-up, arms interleaved within each run.
	auto run_throughput(std::size_t aRuns, std::uint64_t aSeed) -> int
	{
		constexpr std::size_t         bodyCount = 20015U;
		constexpr std::size_t         bodySize  = 400U;
		const std::array<workload, 5> workloads{ {
			{ "W1-64MiB-one-update", 64U * mebibyte, 64U * mebibyte, false },
			{ "W2-20015x400B-bodies", bodyCount * bodySize, bodySize, true },
			{ "W3-1MiB-64KiB-updates", mebibyte, 64U * 1024U, false },
			{ "W3-16MiB-64KiB-updates", 16U * mebibyte, 64U * 1024U, false },
			{ "W3-256MiB-64KiB-updates", 256U * mebibyte, 64U * 1024U, false },
		} };
		seeded_bytes                  random{ aSeed };
		const auto                    input = random.make(256U * mebibyte);
		volatile std::uint8_t         sink  = 0U;
		for (const auto algorithm : { hash_algorithm::sha1, hash_algorithm::sha256 }) {
			auto arms = make_arms(algorithm);
			for (const auto& work : workloads) {
				const std::span<const std::uint8_t> data{ input.data(), work.Bytes };
				const auto                          once = [&](hash_arm& aArm) {
					if (work.Bodies) {
						for (std::size_t body = 0U; body < bodyCount; ++body) {
							sink = sink ^ aArm.hash(data.subspan(body * bodySize, bodySize)).Bytes[0];
						}
						return;
					}
					aArm.reset();
					for (auto rest = data; !rest.empty();) {
						const auto take = std::min(rest.size(), work.Update);
						aArm.update(rest.first(take));
						rest = rest.subspan(take);
					}
					sink = sink ^ aArm.finish().Bytes[0];
				};
				std::vector<std::vector<std::uint64_t>> runs(arms.size());
				for (std::size_t run = 0U; run <= aRuns; ++run) {
					for (std::size_t index = 0U; index < arms.size(); ++index) {
						const auto nanoseconds = elapsed_nanoseconds([&] { once(*arms[index]); });
						if (run != 0U) {
							runs[index].push_back(nanoseconds);
						}
					}
				}
				for (std::size_t index = 0U; index < arms.size(); ++index) {
					auto sorted = runs[index];
					std::ranges::sort(sorted);
					const auto  median = sorted[sorted.size() / 2U];
					std::string list;
					for (const auto value : runs[index]) {
						list += (list.empty() ? "" : ",") + std::to_string(value);
					}
					std::printf("{\"command\":\"throughput\",\"group\":%s,\"workload\":%s,\"arm\":%s,\"bytes\":%zu,\"runs_ns\":[%s],\"median_ns\":%llu,\"min_ns\":%llu,\"max_ns\":%llu,\"median_mb_per_s\":%.1f}\n",
								json_text(algorithm_name(algorithm)).c_str(), json_text(work.Name).c_str(), json_text(arms[index]->name()).c_str(), work.Bytes, list.c_str(),
								static_cast<unsigned long long>(median), static_cast<unsigned long long>(sorted.front()), static_cast<unsigned long long>(sorted.back()),
								static_cast<double>(work.Bytes) * 1000.0 / static_cast<double>(median));
					std::fflush(stdout);
				}
			}
		}
		static_cast<void>(sink);
		return 0;
	}

	auto usage() -> int
	{
		std::fputs("usage: hash_evaluation kat CAVP_DIR | chunking | differential SEED [--large] | object-ids FILE... | sweep sha1|sha256 | collision DIR | throughput RUNS SEED\n",
				   stderr);
		return 2;
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	const std::vector<std::string> arguments(aArgv + 1, aArgv + aArgc);
	if (arguments.empty()) {
		return usage();
	}
	const auto& command = arguments.front();
	if (command == "kat" && arguments.size() == 2U) {
		return run_known_answers(arguments[1]);
	}
	if (command == "chunking" && arguments.size() == 1U) {
		return run_chunking();
	}
	if (command == "differential" && (arguments.size() == 2U || (arguments.size() == 3U && arguments[2] == "--large"))) {
		return run_differential(std::stoull(arguments[1]), arguments.size() == 3U);
	}
	if (command == "object-ids" && arguments.size() >= 2U) {
		return run_object_ids(std::span{ arguments }.subspan(1U));
	}
	if (command == "sweep" && arguments.size() == 2U && (arguments[1] == "sha1" || arguments[1] == "sha256")) {
		return run_sweep(arguments[1] == "sha1" ? hash_algorithm::sha1 : hash_algorithm::sha256);
	}
	if (command == "collision" && arguments.size() == 2U) {
		return run_collisions(arguments[1]);
	}
	if (command == "throughput" && arguments.size() == 3U) {
		return run_throughput(std::stoull(arguments[1]), std::stoull(arguments[2]));
	}
	return usage();
}
