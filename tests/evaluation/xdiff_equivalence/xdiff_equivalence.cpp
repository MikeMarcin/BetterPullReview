// [[BPR-1485]]. AC2 equivalence: the product's diff (the core engine with the
// linked xdiff line matcher, exactly as tests/support/product_diff.hpp and the
// application compute it) over RFC-BPR-0082's evaluation corpus. For each pair
// it prints the digests the evaluation harness recorded (segments, pairs,
// intraline, with the harness's FNV-1a) and, for a pair with a recorded GitHub
// patch, whether the hunks agree with GitHub's (joined as GitHub's file list
// joins them). compare_with_l_myers_indent.py compares the lines with the
// evaluation's `l-myers-indent` raw outcomes.
//
// Input, one pair per line on standard input, tab-separated:
//   id  options  left-path  right-path  oracle
// where oracle is "-" or "a,b,c,d;a,b,c,d;..." hunk headers as written.

#include "harness/oracle.hpp"
#include "product_diff.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
	using namespace bpr;

	class fnv
	{
	public:
		auto add(const std::uint64_t aValue) -> void
		{
			for (int shift = 0; shift < 64; shift += 8) {
				Value ^= (aValue >> shift) & 0xFFU;
				Value *= 0x100000001B3ULL;
			}
		}

		[[nodiscard]] auto hex() const -> std::string
		{
			auto text = std::array<char, 17U>{ };
			std::snprintf(text.data(), text.size(), "%016llx", static_cast<unsigned long long>(Value));
			return text.data();
		}

	private:
		std::uint64_t Value = 0xCBF29CE484222325ULL;
	};

	[[nodiscard]] auto read_bytes(const std::string& aPath) -> std::vector<std::uint8_t>
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	[[nodiscard]] auto parse_options(const std::string& aName) -> core::diff_options
	{
		auto options = core::diff_options{ };
		if (aName.find("ignore-trailing") != std::string::npos) {
			options.Whitespace = core::whitespace_policy::ignore_trailing;
		}
		if (aName.find("ignore-all") != std::string::npos) {
			options.Whitespace = core::whitespace_policy::ignore_all;
		}
		if (aName.find("normalize-endings") != std::string::npos) {
			options.LineEndings = core::line_ending_policy::normalize;
		}
		return options;
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] auto comparison_key(const core::diff_options& aOptions) -> core::file_comparison_key
	{
		const auto blob = [](const std::uint8_t aSeed) {
			std::array<std::uint8_t, 32U> bytes{ };
			for (std::size_t index = 0U; index < 20U; ++index) {
				bytes[index] = static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
			}
			return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
		};
		const auto scope       = opaque<core::pull_request_authority_scope_id>(11U);
		const auto incarnation = opaque<core::epoch_catalog_incarnation_id>(12U);
		const auto authority   = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(13U),
			.Account         = core::github_account_id{ 40U },
			.Viewer          = core::github_viewer_actor_id{ 50U },
			.Repository      = core::repository_id{ 101U },
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = core::pull_request_id{ 202U },
			.Number          = core::pull_request_number{ 17U },
			.Scope           = scope,
		};
		return core::file_comparison_key{
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = { .Authority = scope, .Incarnation = incarnation, .Kind = core::comparison_anchor_kind::epoch_base, .Epoch = core::epoch_ordinal{ 1 } },
			.RightAnchor = { .Authority = scope, .Incarnation = incarnation, .Kind = core::comparison_anchor_kind::epoch_head, .Epoch = core::epoch_ordinal{ 2 } },
			.LeftBlob    = blob(21U),
			.RightBlob   = blob(51U),
			.File        = opaque<core::file_identity>(31U),
			.Options     = *core::options_digest(aOptions),
			.Algorithm   = core::supported_diff_algorithm_revision,
		};
	}

	[[nodiscard]] auto parse_oracle(const std::string& aText) -> std::vector<evaluation::oracle_hunk>
	{
		auto result = std::vector<evaluation::oracle_hunk>{ };
		if (aText == "-") {
			return result;
		}
		auto hunks = std::istringstream{ aText };
		auto hunk  = std::string{ };
		while (std::getline(hunks, hunk, ';')) {
			auto values = std::istringstream{ hunk };
			auto parsed = evaluation::oracle_hunk{ };
			auto field  = std::string{ };
			for (auto& value : parsed) {
				std::getline(values, field, ',');
				value = static_cast<std::uint32_t>(std::stoul(field));
			}
			result.push_back(parsed);
		}
		return result;
	}
} // namespace

int main()
{
	auto line = std::string{ };
	while (std::getline(std::cin, line)) {
		auto fields = std::vector<std::string>{ };
		auto stream = std::istringstream{ line };
		auto field  = std::string{ };
		while (std::getline(stream, field, '\t')) {
			fields.push_back(field);
		}
		if (fields.size() != 5U) {
			std::cerr << "malformed input line\n";
			return 2;
		}
		const auto options = parse_options(fields[1]);
		const auto left    = read_bytes(fields[2]);
		const auto right   = read_bytes(fields[3]);
		const auto outcome = bpr::tests::product_file_diff({
			.Key        = comparison_key(options),
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ left },
			.RightBytes = std::span<const std::uint8_t>{ right },
		});
		std::cout << fields[0] << '\t' << static_cast<int>(outcome.Reason) << '\t' << (outcome.Result.has_value() ? 1 : 0);
		if (!outcome.Result.has_value()) {
			std::cout << "\t-\t-\t-\t-\t-\n";
			continue;
		}
		const auto& diff     = *outcome.Result;
		auto        segments = fnv{ };
		for (const auto& segment : diff.Alignment.Segments) {
			segments.add(static_cast<std::uint64_t>(segment.Kind));
			segments.add((static_cast<std::uint64_t>(segment.Left.Begin) << 32U) | segment.Left.End);
			segments.add((static_cast<std::uint64_t>(segment.Right.Begin) << 32U) | segment.Right.End);
		}
		auto pairs     = fnv{ };
		auto intraline = fnv{ };
		auto pairIndex = std::size_t{ 0U };
		for (const auto& edit : diff.Intraline) {
			while (pairIndex < diff.Alignment.Pairs.size() && diff.Alignment.Pairs[pairIndex].Id != edit.Pair) {
				++pairIndex;
			}
			intraline.add(pairIndex);
			intraline.add((edit.Left.Begin << 32U) | edit.Left.End);
			intraline.add((edit.Right.Begin << 32U) | edit.Right.End);
		}
		for (const auto& pair : diff.Alignment.Pairs) {
			pairs.add((static_cast<std::uint64_t>(pair.LeftLine) << 32U) | pair.RightLine);
			pairs.add(static_cast<std::uint64_t>(pair.Kind));
		}
		std::cout << '\t' << segments.hex() << '\t' << pairs.hex() << '\t' << intraline.hex() << '\t' << (diff.AlignmentApproximate ? 1 : 0);
		const auto oracle = parse_oracle(fields[4]);
		if (fields[4] == "-") {
			std::cout << "\t-\n";
		} else {
			const auto agreement = evaluation::compare_with_oracle(diff.Alignment, oracle);
			std::cout << '\t' << (agreement.ExactGithubJoin ? 1 : 0) << '\n';
		}
	}
	return 0;
}
