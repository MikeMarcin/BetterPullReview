// [[RFC-BPR-0082]] AC07, [[BPR-1465]] AC2. Writes every record of the ordinary
// entry points' outcomes over a deterministic set of comparisons, one line per
// record, so the output of two builds can be compared byte for byte. It uses
// only the public API that predates the evaluation seam, so the same source
// builds against the core at RFC_BASE_SHA.
//
// Usage: tool_diff_baseline_digest [corpus-manifest.json] > digest.txt
//
// The optional manifest is the evaluation corpus manifest; each pair's two
// files are compared as well, with default options.

#include "bpr/core/diff_compute.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr::core;

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed) -> git_blob_oid
	{
		std::array<std::uint8_t, 32U> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
		}
		return git_blob_oid{ git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison_key(const diff_options& aOptions) -> file_comparison_key
	{
		const auto scope       = opaque<pull_request_authority_scope_id>(11U);
		const auto incarnation = opaque<epoch_catalog_incarnation_id>(12U);
		const auto authority   = pull_request_authority{
			.Profile         = host_profile_id{ 10U },
			.ProfileRevision = github_profile_revision{ 20U },
			.Host            = git_host_id{ 30U },
			.HostAuthority   = opaque<host_authority_fingerprint>(13U),
			.Account         = github_account_id{ 40U },
			.Viewer          = github_viewer_actor_id{ 50U },
			.Repository      = repository_id{ 101U },
			.Cache           = opaque<repository_cache_scope_id>(14U),
			.PullRequest     = pull_request_id{ 202U },
			.Number          = pull_request_number{ 17U },
			.Scope           = scope,
		};
		return file_comparison_key{
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = comparison_anchor_identity{ .Authority = scope, .Incarnation = incarnation, .Kind = comparison_anchor_kind::epoch_base, .Epoch = epoch_ordinal{ 1 } },
			.RightAnchor = comparison_anchor_identity{ .Authority = scope, .Incarnation = incarnation, .Kind = comparison_anchor_kind::epoch_head, .Epoch = epoch_ordinal{ 2 } },
			.LeftBlob    = blob(21U),
			.RightBlob   = blob(51U),
			.File        = opaque<file_identity>(31U),
			.Options     = *options_digest(aOptions),
			.Algorithm   = supported_diff_algorithm_revision,
		};
	}

	/// Appends records as text to one output line per record.
	class writer
	{
	public:
		auto line(const std::string_view aText) -> void
		{
			std::fwrite(aText.data(), 1U, aText.size(), stdout);
			std::fputc('\n', stdout);
		}

		template <typename Identity>
		[[nodiscard]] static auto hex(const Identity& aIdentity) -> std::string
		{
			static constexpr auto digits = std::string_view{ "0123456789abcdef" };
			auto                  result = std::string{ };
			for (const auto byte : aIdentity.bytes()) {
				result.push_back(digits[byte >> 4U]);
				result.push_back(digits[byte & 0x0FU]);
			}
			return result;
		}
	};

	[[nodiscard]] auto span_text(const line_span aSpan) -> std::string
	{
		return std::to_string(aSpan.Begin) + "-" + std::to_string(aSpan.End);
	}

	template <typename TComputed>
	auto write_result(writer& aOut, const std::string& aName, const diff_reason_code aReason, const std::optional<TComputed>& aResult) -> void
	{
		aOut.line("case " + aName + " reason " + std::to_string(static_cast<int>(aReason)));
		if (!aResult.has_value()) {
			return;
		}
		const auto& diff = *aResult;
		aOut.line("counters " + std::to_string(diff.MatrixCellsConsumed) + " " + std::to_string(diff.MoveDetectionWorkConsumed) + " "
				  + std::to_string(static_cast<int>(diff.MoveDetectionWorkLimitReached)) + " " + std::to_string(static_cast<int>(diff.MoveDetectionSegmentLimitReached)) + " "
				  + std::to_string(diff.PrimaryIndexWorkConsumed) + " " + std::to_string(static_cast<int>(diff.AlignmentApproximate)));
		const auto& alignment = diff.Alignment;
		aOut.line("alignment " + writer::hex(alignment.Digest) + " " + std::to_string(alignment.LeftLineCount) + " " + std::to_string(alignment.RightLineCount) + " "
				  + std::to_string(static_cast<int>(alignment.Content)) + " " + std::to_string(static_cast<int>(alignment.Diagnostic.Code)));
		for (const auto& segment : alignment.Segments) {
			aOut.line("segment " + writer::hex(segment.Id) + " " + std::to_string(static_cast<int>(segment.Kind)) + " " + span_text(segment.Left) + " " + span_text(segment.Right) + " "
					  + (segment.Group.has_value() ? writer::hex(*segment.Group) : std::string{ "-" }));
		}
		for (const auto& pair : alignment.Pairs) {
			aOut.line("pair " + writer::hex(pair.Id) + " " + writer::hex(pair.Segment) + " " + std::to_string(pair.LeftLine) + " " + std::to_string(pair.RightLine) + " "
					  + std::to_string(static_cast<int>(pair.Kind)));
		}
		for (const auto& group : alignment.Groups) {
			aOut.line("group " + writer::hex(group.Id) + " " + std::to_string(group.Members.Begin) + "-" + std::to_string(group.Members.End) + " " + span_text(group.Left) + " "
					  + span_text(group.Right) + " " + std::to_string(static_cast<int>(group.Kind)) + " " + std::to_string(group.AddedLines) + " "
					  + std::to_string(group.DeletedLines) + " " + std::to_string(group.ReplacedLeftLines) + " " + std::to_string(group.ReplacedRightLines));
		}
		for (const auto& move : alignment.Moves) {
			aOut.line("move " + writer::hex(move.Id) + " " + writer::hex(move.SourceDeletion) + " " + writer::hex(move.DestinationInsertion));
		}
		for (const auto& edit : diff.Intraline) {
			aOut.line("intraline " + writer::hex(edit.Pair) + " " + std::to_string(edit.Left.Begin) + "-" + std::to_string(edit.Left.End) + " " + std::to_string(edit.Right.Begin) + "-"
					  + std::to_string(edit.Right.End));
		}
		for (const auto* document : { &diff.LeftDocument, &diff.RightDocument }) {
			if (!document->has_value()) {
				aOut.line("document -");
				continue;
			}
			auto text = "document " + std::to_string((*document)->Bytes.size()) + " " + std::to_string((*document)->Lines.size()) + " "
				+ std::to_string(static_cast<int>((*document)->HasFinalNewline));
			aOut.line(text);
		}
	}

	struct pair_text
	{
		std::vector<std::uint8_t> Left;
		std::vector<std::uint8_t> Right;
	};

	/// A seeded source-like pair: edits, deletions, insertions and moved blocks.
	[[nodiscard]] auto synthetic_pair(const std::uint64_t aSeed, const std::uint64_t aLines, const std::uint32_t aEditPermille, const bool aRepetitive) -> pair_text
	{
		auto       state = aSeed * 0x9E37'79B9'7F4A'7C15ULL + 1U;
		const auto below = [&state](const std::uint64_t aBound) {
			state ^= state << 13U;
			state ^= state >> 7U;
			state ^= state << 17U;
			return state % aBound;
		};
		const auto line = [&](const std::uint64_t aOrdinal) -> std::string {
			if (aRepetitive) {
				switch (below(4U)) {
				case 0U:
					return "";
				case 1U:
					return "    }";
				case 2U:
					return "    return value;";
				default:
					return "    value += step;";
				}
			}
			switch (below(12U)) {
			case 0U:
				return "";
			case 1U:
				return "    }";
			case 2U:
				return "\tif (ready) {";
			default:
				return "    auto value_" + std::to_string(aOrdinal) + " = compute(" + std::to_string(below(100'000U)) + ", item" + std::to_string(below(1'000U)) + ");";
			}
		};
		auto left = std::vector<std::string>{ };
		for (std::uint64_t ordinal = 0U; ordinal < aLines; ++ordinal) {
			left.push_back(line(ordinal));
		}
		auto right = std::vector<std::string>{ };
		auto moved = std::vector<std::string>{ };
		for (std::uint64_t ordinal = 0U; ordinal < aLines; ++ordinal) {
			const auto roll = below(1'000U);
			if (roll < 2U && ordinal + 30U < aLines) {
				moved.insert(moved.end(), left.begin() + static_cast<std::ptrdiff_t>(ordinal), left.begin() + static_cast<std::ptrdiff_t>(ordinal + 20U));
				ordinal += 19U;
				continue;
			}
			if (roll < 2U + aEditPermille / 4U) {
				continue;
			}
			if (roll < 2U + aEditPermille / 2U) {
				right.push_back(line(aLines + ordinal));
			}
			if (roll >= 1'000U - aEditPermille) {
				auto edited = left[ordinal];
				edited.insert(edited.size() / 2U, "x_" + std::to_string(below(9U)) + (below(3U) == 0U ? " " : ""));
				right.push_back(edited);
			} else {
				right.push_back(left[ordinal]);
			}
			if (!moved.empty() && below(1'000U) < 5U) {
				right.insert(right.end(), moved.begin(), moved.end());
				moved.clear();
			}
		}
		right.insert(right.end(), moved.begin(), moved.end());
		const auto join = [&](const std::vector<std::string>& aText) {
			auto bytes = std::vector<std::uint8_t>{ };
			for (const auto& text : aText) {
				bytes.insert(bytes.end(), text.begin(), text.end());
				if (below(97U) == 0U) {
					bytes.push_back('\r');
				}
				if (below(211U) == 0U) {
					bytes.push_back(' ');
				}
				bytes.push_back('\n');
			}
			return bytes;
		};
		return { .Left = join(left), .Right = join(right) };
	}

	[[nodiscard]] auto bytes_of(const std::string_view aText) -> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	[[nodiscard]] auto read_file(const std::string& aPath) -> std::optional<std::vector<std::uint8_t>>
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		if (!stream) {
			return std::nullopt;
		}
		return std::vector<std::uint8_t>{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	auto compare(writer& aOut, const std::string& aName, const pair_text& aPair, const diff_options& aOptions, const diff_compute_limits& aLimits) -> void
	{
		const auto input = diff_compute_input{
			.Key        = comparison_key(aOptions),
			.Options    = aOptions,
			.LeftBytes  = std::span<const std::uint8_t>{ aPair.Left },
			.RightBytes = std::span<const std::uint8_t>{ aPair.Right },
			.Limits     = aLimits,
		};
		const auto outcome = compute_file_diff(input);
		write_result(aOut, aName, outcome.Reason, outcome.Result);
		const auto material = compute_material_diff(material_diff_input{
			.Key        = material_comparison_key{ .Comparison = opaque<file_comparison_digest>(77U), .LeftPresent = true, .RightPresent = true, .Options = *options_digest(aOptions), .Algorithm = supported_diff_algorithm_revision },
			.Options    = aOptions,
			.LeftBytes  = std::span<const std::uint8_t>{ aPair.Left },
			.RightBytes = std::span<const std::uint8_t>{ aPair.Right },
			.Limits     = aLimits,
		});
		write_result(aOut, aName + "/material", material.Reason, material.Result);
	}

	/// The corpus manifest's pair paths, read without a JSON library: each
	/// `"left": "..."` and `"right": "..."` value in order.
	[[nodiscard]] auto manifest_pairs(const std::string& aManifest) -> std::vector<std::pair<std::string, std::string>>
	{
		const auto text = read_file(aManifest);
		auto       result = std::vector<std::pair<std::string, std::string>>{ };
		if (!text.has_value()) {
			return result;
		}
		const auto content = std::string{ text->begin(), text->end() };
		const auto base    = aManifest.substr(0U, aManifest.find_last_of('/') + 1U);
		const auto value   = [&content](const std::string_view aKey, const std::size_t aFrom) -> std::pair<std::string, std::size_t> {
			const auto key = content.find(aKey, aFrom);
			if (key == std::string::npos) {
				return { { }, std::string::npos };
			}
			const auto open  = content.find('"', key + aKey.size());
			const auto close = content.find('"', open + 1U);
			return { content.substr(open + 1U, close - open - 1U), close };
		};
		auto cursor = std::size_t{ 0U };
		for (;;) {
			const auto [left, afterLeft] = value("\"left\":", cursor);
			if (afterLeft == std::string::npos) {
				break;
			}
			const auto [right, afterRight] = value("\"right\":", afterLeft);
			if (afterRight == std::string::npos) {
				break;
			}
			result.emplace_back(base + left, base + right);
			cursor = afterRight;
		}
		return result;
	}
} // namespace

auto main(const int aArgc, char** aArgv) -> int
{
	auto out     = writer{ };
	auto options = std::vector<std::pair<std::string, diff_options>>{ };
	for (const auto whitespace : { whitespace_policy::exact, whitespace_policy::ignore_trailing, whitespace_policy::ignore_all }) {
		for (const auto endings : { line_ending_policy::exact, line_ending_policy::normalize }) {
			for (const auto moves : { move_detection_policy::disabled, move_detection_policy::deterministic_exact }) {
				for (const auto pairing : { pairing_policy::deterministic_similarity, pairing_policy::disabled }) {
					auto value        = diff_options{ };
					value.Whitespace  = whitespace;
					value.LineEndings = endings;
					value.Moves       = moves;
					value.Pairing     = pairing;
					options.emplace_back("w" + std::to_string(static_cast<int>(whitespace)) + "e" + std::to_string(static_cast<int>(endings)) + "m" + std::to_string(static_cast<int>(moves))
											 + "p" + std::to_string(static_cast<int>(pairing)),
										 value);
				}
			}
		}
	}
	auto lowThreshold                        = diff_options{ };
	lowThreshold.SimilarityThresholdPermille = 300U;
	options.emplace_back("t300", lowThreshold);

	auto pairs = std::vector<std::pair<std::string, pair_text>>{
		{ "empty", { } },
		{ "identical", { bytes_of("a\nb\n"), bytes_of("a\nb\n") } },
		{ "lone-cr", { bytes_of("a\rb\nc\n"), bytes_of("a\rB\nc\r\n") } },
		{ "no-final-newline", { bytes_of("a\nb"), bytes_of("a\nb\n") } },
		{ "unicode", { bytes_of("caf\xC3\xA9 \xF0\x9F\x98\x80 tail\n"), bytes_of("cafe \xF0\x9F\x98\x81 tail\n") } },
		{ "binary", { bytes_of(std::string_view{ "a\0b\n", 4U }), bytes_of("ab\n") } },
		{ "invalid-utf8", { bytes_of("a\xFF\n"), bytes_of("a\n") } },
	};
	for (const auto& [lines, permille] : { std::pair{ 40U, 300U }, std::pair{ 600U, 60U }, std::pair{ 3'000U, 20U }, std::pair{ 12'000U, 5U } }) {
		pairs.emplace_back("synthetic-" + std::to_string(lines), synthetic_pair(lines, lines, permille, false));
		pairs.emplace_back("repetitive-" + std::to_string(lines), synthetic_pair(lines + 1U, lines, permille, true));
	}
	for (const auto& [name, pair] : pairs) {
		for (const auto& [optionName, value] : options) {
			compare(out, name + "/" + optionName, pair, value, { });
		}
	}
	// Small budgets force the ordered, patience, unpaired and intraline
	// degradations.
	for (const auto cells : { std::uint64_t{ 64U }, std::uint64_t{ 4'096U }, std::uint64_t{ 1U } << 18U, std::uint64_t{ 1U } << 22U }) {
		for (const auto& [name, pair] : pairs) {
			compare(out, name + "/cells" + std::to_string(cells), pair, { }, { .MaximumMatrixCells = cells });
			compare(out, name + "/index" + std::to_string(cells), pair, { }, { .MaximumMatrixCells = cells, .MaximumPrimaryIndexWork = cells * 4U });
		}
	}
	if (aArgc > 1) {
		for (const auto& [left, right] : manifest_pairs(aArgv[1])) {
			const auto leftBytes  = read_file(left);
			const auto rightBytes = read_file(right);
			if (!leftBytes.has_value() || !rightBytes.has_value()) {
				out.line("missing " + left + " " + right);
				continue;
			}
			compare(out, left, pair_text{ .Left = *leftBytes, .Right = *rightBytes }, { }, { });
		}
	}
	return 0;
}
