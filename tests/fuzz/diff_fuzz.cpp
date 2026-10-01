// [[BPR-0970]]. Diff parsing and mapping: the text decoding, alignment and
// limits of compute_file_diff and compute_material_diff, and the line, move,
// group and selection mapping built over a published file diff.
//
// Input: [split] [options] [policies] [mode] then the two sides' bytes, the
// left side being the first `split / 255` of them.

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "fuzz_support.hpp"

#include <memory>
#include <optional>
#include <span>

namespace {
	using namespace bpr;

	auto authority() -> core::pull_request_authority
	{
		return {
			.Profile         = core::host_profile_id{ 1 },
			.ProfileRevision = core::github_profile_revision{ 2 },
			.Host            = core::git_host_id{ 3 },
			.HostAuthority   = fuzz::opaque<core::host_authority_fingerprint>(4),
			.Account         = core::github_account_id{ 5 },
			.Viewer          = core::github_viewer_actor_id{ 6 },
			.Repository      = core::repository_id{ 7 },
			.Cache           = fuzz::opaque<core::repository_cache_scope_id>(8),
			.PullRequest     = core::pull_request_id{ 9 },
			.Number          = core::pull_request_number{ 10 },
			.Scope           = fuzz::opaque<core::pull_request_authority_scope_id>(11),
		};
	}

	auto comparison(const core::diff_options& aOptions, const bool aLeft, const bool aRight)
		-> std::optional<core::file_comparison_key>
	{
		const auto auth        = authority();
		const auto incarnation = fuzz::opaque<core::epoch_catalog_incarnation_id>(12);
		const auto digest      = core::options_digest(aOptions);
		if (!digest) {
			return std::nullopt;
		}
		return core::file_comparison_key{
			.Authority   = auth,
			.Repository  = auth.Repository,
			.PullRequest = auth.PullRequest,
			.LeftAnchor  = { auth.Scope, incarnation, core::comparison_anchor_kind::epoch_base, core::epoch_ordinal{ 1 }, { } },
			.RightAnchor = { auth.Scope, incarnation, core::comparison_anchor_kind::epoch_head, core::epoch_ordinal{ 2 }, { } },
			.LeftBlob    = aLeft ? core::parse_git_blob_oid("1111111111111111111111111111111111111111") : std::nullopt,
			.RightBlob   = aRight ? core::parse_git_blob_oid("2222222222222222222222222222222222222222") : std::nullopt,
			.File        = fuzz::opaque<core::file_identity>(13),
			.Options     = *digest,
			.Algorithm   = core::supported_diff_algorithm_revision,
		};
	}

	// Small limits so a campaign reaches the degraded and refused paths.
	constexpr core::diff_compute_limits limits{
		.MaximumBytesPerSide      = 4096,
		.MaximumLinesPerSide      = 512,
		.MaximumMatrixCells       = 32768,
		.MaximumMoveDetectionWork = 8192,
		.MaximumSegments          = 1024,
		.MaximumPrimaryIndexWork  = 32768,
	};

	auto map_every_line(const core::diff_semantic_index& aIndex, const core::computed_file_diff& aDiff, const std::uint8_t aExtra) -> void
	{
		for (const auto side : { core::diff_side::left, core::diff_side::right }) {
			const auto count = side == core::diff_side::left ? aDiff.Alignment.LeftLineCount : aDiff.Alignment.RightLineCount;
			for (const auto line : { 0U, 1U, count, count + 1U, static_cast<std::uint32_t>(aExtra) }) {
				const core::source_line_ref ref{ aDiff.Alignment.Digest, side, line };
				const auto                  mapped = core::map_line(aIndex, ref);
				fuzz::require(mapped.Source == ref);
				if (mapped.Counterpart) {
					// A mapping is symmetric: the counterpart maps back here.
					const auto back = core::map_line(aIndex, *mapped.Counterpart);
					fuzz::require(back.Counterpart == ref);
				}
				static_cast<void>(core::move_for_line(aIndex, ref));
				static_cast<void>(core::detail_for_mapping(aIndex, mapped));
				static_cast<void>(core::detail_for_line(aIndex, ref));
				static_cast<void>(core::select_line(aIndex, ref));
			}
		}
		for (const auto& group : aDiff.Alignment.Groups) {
			static_cast<void>(core::select_group(aIndex, group.Id));
			static_cast<void>(core::preferred_group_line(aIndex, group.Id));
			for (const auto direction : { core::change_navigation_direction::next, core::change_navigation_direction::previous }) {
				static_cast<void>(core::navigate_change(aIndex, group.Id, direction, core::change_navigation_policy::wrap));
			}
		}
	}

	auto file_diff(const core::diff_options& aOptions, const std::span<const std::uint8_t> aLeft, const std::span<const std::uint8_t> aRight,
				   const bool aLeftPresent, const bool aRightPresent, const std::uint8_t aExtra) -> void
	{
		const auto key = comparison(aOptions, aLeftPresent, aRightPresent);
		if (!key) {
			return;
		}
		const core::diff_compute_input input{
			.Key        = *key,
			.Options    = aOptions,
			.LeftBytes  = aLeftPresent ? std::optional{ aLeft } : std::nullopt,
			.RightBytes = aRightPresent ? std::optional{ aRight } : std::nullopt,
			.Limits     = limits,
		};
		const auto computed = core::compute_file_diff(input);
		if (!computed.Result) {
			return;
		}
		const auto& diff = *computed.Result;
		// Whatever the engine publishes satisfies its own validator and stays
		// inside the work it was given.
		fuzz::require(core::validate_computed_file_diff(diff).valid());
		fuzz::require(diff.MatrixCellsConsumed <= limits.MaximumMatrixCells);
		fuzz::require(diff.MoveDetectionWorkConsumed <= limits.MaximumMoveDetectionWork);
		fuzz::require(diff.PrimaryIndexWorkConsumed <= limits.MaximumPrimaryIndexWork);
		const auto shared = std::make_shared<const core::computed_file_diff>(diff);
		const auto index  = core::build_diff_semantic_index(shared);
		fuzz::require(index.has_value());
		static_cast<void>(core::project_changes(*index));
		map_every_line(*index, diff, aExtra);
		// A result whose comparison identity is forged never gains an index.
		auto forged              = std::make_shared<core::computed_file_diff>(diff);
		forged->Alignment.Digest = { };
		fuzz::require(!core::validate_computed_file_diff(*forged).valid());
		fuzz::require(!core::build_diff_semantic_index(forged));
	}

	auto material_diff(const core::diff_options& aOptions, const std::span<const std::uint8_t> aLeft, const std::span<const std::uint8_t> aRight,
					   const bool aLeftPresent, const bool aRightPresent) -> void
	{
		const auto digest = core::options_digest(aOptions);
		if (!digest) {
			return;
		}
		const core::material_diff_input input{
			.Key = {
				.Comparison   = fuzz::opaque<core::file_comparison_digest>(14),
				.LeftPresent  = aLeftPresent,
				.RightPresent = aRightPresent,
				.Options      = *digest,
				.Algorithm    = core::supported_diff_algorithm_revision,
			},
			.Options    = aOptions,
			.LeftBytes  = aLeftPresent ? std::optional{ aLeft } : std::nullopt,
			.RightBytes = aRightPresent ? std::optional{ aRight } : std::nullopt,
			.Limits     = limits,
		};
		const auto computed = core::compute_material_diff(input);
		if (computed.Result) {
			fuzz::require(core::validate_computed_material_diff(*computed.Result).valid());
			fuzz::require(computed.Result->MatrixCellsConsumed <= limits.MaximumMatrixCells);
		}
	}
}

// libFuzzer calls the entry point by this exact name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* aData, const std::size_t aSize) -> int
{
	fuzz::fuzz_input input{ aData, aSize };
	const auto       split    = input.byte();
	const auto       options  = input.byte();
	const auto       policies = input.byte();
	const auto       mode     = input.byte();
	const auto       bytes    = input.rest();
	if (bytes.size() > 8192U) {
		return 0;
	}

	core::diff_options requested{
		.Whitespace                  = static_cast<core::whitespace_policy>(policies % 4U),
		.LineEndings                 = static_cast<core::line_ending_policy>((policies >> 2U) % 3U),
		.ContextLines                = options % 8U,
		.Pairing                     = static_cast<core::pairing_policy>((policies >> 4U) % 3U),
		.SimilarityThresholdPermille = static_cast<std::uint16_t>(((options >> 3U) % 12U) * 100U),
		.Moves                       = static_cast<core::move_detection_policy>((policies >> 6U) % 3U),
	};
	// Out-of-range enumerators and thresholds are refused. What normalisation
	// returns is valid and is its own normal form.
	const auto normalized = core::normalize_diff_options(requested);
	if (!normalized) {
		fuzz::require(!core::valid(requested));
		return 0;
	}
	fuzz::require(core::valid(*normalized) && core::normalize_diff_options(*normalized) == normalized);

	const auto left         = bytes.first((bytes.size() * split) / 255U);
	const auto right        = bytes.subspan(left.size());
	const bool leftPresent  = (mode & 1U) == 0U || (mode & 2U) != 0U;
	const bool rightPresent = (mode & 1U) == 0U || (mode & 2U) == 0U;
	if ((mode & 4U) == 0U) {
		file_diff(*normalized, left, right, leftPresent, rightPresent, options);
	} else {
		material_diff(*normalized, left, right, leftPresent, rightPresent);
	}
	return 0;
}
