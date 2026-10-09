#include "bpr/core/diff_model.hpp"

#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "bpr/core/sha256.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <utility>

namespace bpr::core {
	namespace {
		template <std::size_t Capacity>
		class canonical_encoder final
		{
		public:
			void append_domain(const std::string_view aDomain) noexcept
			{
				for (const auto character : aDomain) {
					append_u8(static_cast<std::uint8_t>(character));
				}
			}

			void append(const std::span<const std::uint8_t> aValues) noexcept
			{
				for (const auto value : aValues) {
					append_u8(value);
				}
			}

			void append_u8(const std::uint8_t aValue) noexcept
			{
				if (Size == Bytes.size()) {
					Overflow = true;
					return;
				}
				Bytes[Size++] = aValue;
			}

			void append_u16(const std::uint16_t aValue) noexcept
			{
				append_u8(static_cast<std::uint8_t>(aValue >> 8U));
				append_u8(static_cast<std::uint8_t>(aValue));
			}

			void append_u32(const std::uint32_t aValue) noexcept
			{
				for (std::size_t shift = 4U; shift != 0U; --shift) {
					append_u8(static_cast<std::uint8_t>(
						aValue >> ((shift - 1U) * 8U)));
				}
			}

			void append_u64(const std::uint64_t aValue) noexcept
			{
				for (std::size_t shift = 8U; shift != 0U; --shift) {
					append_u8(static_cast<std::uint8_t>(
						aValue >> ((shift - 1U) * 8U)));
				}
			}

			[[nodiscard]] auto bytes() const noexcept
				-> std::span<const std::uint8_t>
			{
				return std::span{ Bytes }.first(Size);
			}

			[[nodiscard]] auto overflowed() const noexcept -> bool
			{
				return Overflow;
			}

		private:
			std::array<std::uint8_t, Capacity> Bytes{ };
			std::size_t                        Size     = 0U;
			bool                               Overflow = false;
		};

		template <typename Identity, std::size_t Capacity>
		[[nodiscard]] auto identity_from(
			const canonical_encoder<Capacity>& aEncoder) noexcept -> Identity
		{
			if (aEncoder.overflowed()) {
				return { };
			}
			return Identity{ sha256(aEncoder.bytes()) };
		}

		[[nodiscard]] constexpr auto valid_whitespace_policy(
			const whitespace_policy aPolicy) noexcept -> bool
		{
			switch (aPolicy) {
			case whitespace_policy::exact:
			case whitespace_policy::ignore_trailing:
			case whitespace_policy::ignore_all:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_line_ending_policy(
			const line_ending_policy aPolicy) noexcept -> bool
		{
			switch (aPolicy) {
			case line_ending_policy::exact:
			case line_ending_policy::normalize:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_pairing_policy(
			const pairing_policy aPolicy) noexcept -> bool
		{
			switch (aPolicy) {
			case pairing_policy::disabled:
			case pairing_policy::deterministic_similarity:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_move_detection_policy(
			const move_detection_policy aPolicy) noexcept -> bool
		{
			switch (aPolicy) {
			case move_detection_policy::disabled:
			case move_detection_policy::deterministic_exact:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_side(const diff_side aSide) noexcept -> bool
		{
			switch (aSide) {
			case diff_side::left:
			case diff_side::right:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_alignment_kind(
			const alignment_kind aKind) noexcept -> bool
		{
			switch (aKind) {
			case alignment_kind::equal:
			case alignment_kind::replacement:
			case alignment_kind::left_only:
			case alignment_kind::right_only:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_line_pair_kind(
			const line_pair_kind aKind) noexcept -> bool
		{
			switch (aKind) {
			case line_pair_kind::exact:
			case line_pair_kind::similar:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_content_state(
			const diff_content_state aState) noexcept -> bool
		{
			switch (aState) {
			case diff_content_state::text:
			case diff_content_state::binary:
			case diff_content_state::unavailable:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_reason_code(
			const diff_reason_code aReason) noexcept -> bool
		{
			switch (aReason) {
			case diff_reason_code::none:
			case diff_reason_code::binary_file:
			case diff_reason_code::missing_left_blob:
			case diff_reason_code::missing_right_blob:
			case diff_reason_code::decode_failure:
			case diff_reason_code::materialization_limit:
			case diff_reason_code::compute_limit:
			case diff_reason_code::generated_file_unsupported:
			case diff_reason_code::cancelled:
			case diff_reason_code::stale:
			case diff_reason_code::invalid_alignment:
			case diff_reason_code::host_failure:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_diagnostic(
			const diff_content_state aContent,
			const diff_reason_code   aReason) noexcept -> bool
		{
			if (!valid_content_state(aContent) || !valid_reason_code(aReason)) {
				return false;
			}
			switch (aContent) {
			case diff_content_state::text:
				return aReason == diff_reason_code::none;
			case diff_content_state::binary:
				return aReason == diff_reason_code::binary_file;
			case diff_content_state::unavailable:
				return aReason != diff_reason_code::none
					&& aReason != diff_reason_code::binary_file;
			}
			return false;
		}

		template <std::size_t Capacity>
		void append_authority(canonical_encoder<Capacity>&  aEncoder,
							  const pull_request_authority& aAuthority) noexcept
		{
			aEncoder.append_u64(aAuthority.Profile.value());
			aEncoder.append_u64(aAuthority.ProfileRevision.value());
			aEncoder.append_u64(aAuthority.Host.value());
			aEncoder.append(aAuthority.HostAuthority.bytes());
			aEncoder.append_u64(aAuthority.Account.value());
			aEncoder.append_u64(aAuthority.Viewer.value());
			aEncoder.append_u64(aAuthority.Repository.value());
			aEncoder.append(aAuthority.Cache.bytes());
			aEncoder.append_u64(aAuthority.PullRequest.value());
			aEncoder.append_u32(aAuthority.Number.value());
			aEncoder.append(aAuthority.Scope.bytes());
			// The discriminant is appended only when it is not the
			// authenticated mode, so an authenticated authority encodes
			// byte-for-byte as it did before the mode existed. That matters
			// because `file_comparison_digest` is persisted: the review ledger
			// and the recovery store hold anchors keyed by it, and an
			// unconditional byte would stop every stored anchor matching on
			// upgrade. Discrimination needs less than that: it needs only that
			// no anonymous authority can ever produce an authenticated
			// digest, and a mode byte no authenticated encoding can contain
			// gives exactly that. Making the field unconditional is a
			// re-keying, and it belongs with a domain-string bump and a
			// migration, which is BPR-0317's to make.
			if (aAuthority.Mode != github_access_mode::authenticated) {
				aEncoder.append_u8(static_cast<std::uint8_t>(aAuthority.Mode));
			}
		}

		template <std::size_t Capacity>
		void append_anchor(canonical_encoder<Capacity>&      aEncoder,
						   const comparison_anchor_identity& aAnchor) noexcept
		{
			aEncoder.append(aAnchor.Authority.bytes());
			aEncoder.append(aAnchor.Incarnation.bytes());
			aEncoder.append_u8(static_cast<std::uint8_t>(aAnchor.Kind));
			aEncoder.append_u64(static_cast<std::uint64_t>(aAnchor.Epoch.value()));
			aEncoder.append(aAnchor.Review.bytes());
		}

		template <std::size_t Capacity>
		void append_blob(canonical_encoder<Capacity>&       aEncoder,
						 const std::optional<git_blob_oid>& aBlob) noexcept
		{
			aEncoder.append_u8(aBlob.has_value() ? 1U : 0U);
			if (aBlob.has_value()) {
				aEncoder.append_u8(static_cast<std::uint8_t>(aBlob->algorithm()));
				aEncoder.append(aBlob->bytes());
			}
		}

		template <std::size_t Capacity>
		void append_span(canonical_encoder<Capacity>& aEncoder,
						 const line_span              aSpan) noexcept
		{
			aEncoder.append_u32(aSpan.Begin);
			aEncoder.append_u32(aSpan.End);
		}

		[[nodiscard]] constexpr auto segment_shape_valid(
			const alignment_kind aKind,
			const line_span      aLeft,
			const line_span      aRight) noexcept -> bool
		{
			if (aLeft.Begin > aLeft.End || aRight.Begin > aRight.End
				|| (aLeft.empty() && aRight.empty())) {
				return false;
			}
			switch (aKind) {
			case alignment_kind::equal:
				return !aLeft.empty() && !aRight.empty()
					&& aLeft.extent() == aRight.extent();
			case alignment_kind::replacement:
				return !aLeft.empty() && !aRight.empty();
			case alignment_kind::left_only:
				return !aLeft.empty() && aRight.empty();
			case alignment_kind::right_only:
				return aLeft.empty() && !aRight.empty();
			}
			return false;
		}

		[[nodiscard]] constexpr auto summary_kind(const std::uint64_t aAdded,
												  const std::uint64_t aDeleted,
												  const std::uint64_t aReplacedLeft,
												  const std::uint64_t aReplacedRight) noexcept -> change_summary_kind
		{
			const auto hasAddition    = aAdded != 0U;
			const auto hasDeletion    = aDeleted != 0U;
			const auto hasReplacement = aReplacedLeft != 0U || aReplacedRight != 0U;
			if (hasAddition && !hasDeletion && !hasReplacement) {
				return change_summary_kind::addition;
			}
			if (!hasAddition && hasDeletion && !hasReplacement) {
				return change_summary_kind::deletion;
			}
			if (!hasAddition && !hasDeletion && hasReplacement) {
				return change_summary_kind::replacement;
			}
			return change_summary_kind::mixed;
		}

		[[nodiscard]] constexpr auto failure(const alignment_validation_error aError,
											 const std::size_t                aRecord  = alignment_validation_result::no_record,
											 const std::size_t                aRelated = alignment_validation_result::no_record) noexcept -> alignment_validation_result
		{
			return { .Error = aError, .Record = aRecord, .Related = aRelated };
		}
	} // namespace

	auto normalize_diff_options(diff_options aOptions) noexcept
		-> std::optional<diff_options>
	{
		if (!valid_whitespace_policy(aOptions.Whitespace)
			|| !valid_line_ending_policy(aOptions.LineEndings)
			|| !valid_pairing_policy(aOptions.Pairing)
			|| !valid_move_detection_policy(aOptions.Moves)
			|| aOptions.ContextLines > maximum_diff_context_lines) {
			return std::nullopt;
		}
		if (aOptions.Pairing == pairing_policy::disabled) {
			aOptions.SimilarityThresholdPermille = 0U;
		} else if (aOptions.SimilarityThresholdPermille == 0U
				   || aOptions.SimilarityThresholdPermille
					   > maximum_similarity_threshold_permille) {
			return std::nullopt;
		}
		return aOptions;
	}

	auto valid(const diff_options& aOptions) noexcept -> bool
	{
		const auto normalized = normalize_diff_options(aOptions);
		return normalized.has_value() && *normalized == aOptions;
	}

	auto file_identity_of(const std::string_view aPath) noexcept -> file_identity
	{
		// An empty path names no file, and a path beyond the bound is refused
		// rather than truncated: two paths sharing a prefix would otherwise
		// collapse to one identity, and a receipt for one file would answer for
		// another.
		if (aPath.empty() || aPath.size() > maximum_identified_path_bytes) {
			return { };
		}
		// Domain first, then the length, then the bytes. The domain keeps this
		// identity from colliding with another kind derived from the same
		// bytes; the length keeps two different paths from encoding
		// identically once the domain prefix is fixed.
		auto encoder = canonical_encoder<
			maximum_identified_path_bytes + 64U>{ };
		encoder.append_domain("BPR-file-identity-v1");
		encoder.append_u32(static_cast<std::uint32_t>(aPath.size()));
		for (const auto character : aPath) {
			encoder.append_u8(static_cast<std::uint8_t>(character));
		}
		return identity_from<file_identity>(encoder);
	}

	auto options_digest(const diff_options& aOptions) noexcept
		-> std::optional<diff_options_digest>
	{
		if (!valid(aOptions)) {
			return std::nullopt;
		}
		auto encoder = canonical_encoder<64U>{ };
		encoder.append_domain("BPR-diff-options-v1");
		encoder.append_u8(static_cast<std::uint8_t>(aOptions.Whitespace));
		encoder.append_u8(static_cast<std::uint8_t>(aOptions.LineEndings));
		encoder.append_u32(aOptions.ContextLines);
		encoder.append_u8(static_cast<std::uint8_t>(aOptions.Pairing));
		encoder.append_u16(aOptions.SimilarityThresholdPermille);
		encoder.append_u8(static_cast<std::uint8_t>(aOptions.Moves));
		return identity_from<diff_options_digest>(encoder);
	}

	auto valid(const file_comparison_key& aKey) noexcept -> bool
	{
		const auto blobsValid = (aKey.LeftBlob.has_value() || aKey.RightBlob.has_value())
			&& (!aKey.LeftBlob.has_value() || aKey.LeftBlob->valid())
			&& (!aKey.RightBlob.has_value() || aKey.RightBlob->valid())
			&& (!aKey.LeftBlob.has_value() || !aKey.RightBlob.has_value()
				|| aKey.LeftBlob->algorithm() == aKey.RightBlob->algorithm());
		return bpr::core::valid(aKey.Authority)
			&& aKey.Repository == aKey.Authority.Repository
			&& aKey.PullRequest == aKey.Authority.PullRequest
			&& bpr::core::valid(aKey.LeftAnchor)
			&& bpr::core::valid(aKey.RightAnchor)
			&& aKey.LeftAnchor.Authority == aKey.Authority.Scope
			&& aKey.RightAnchor.Authority == aKey.Authority.Scope
			&& aKey.LeftAnchor.Authority == aKey.RightAnchor.Authority
			&& aKey.LeftAnchor.Incarnation == aKey.RightAnchor.Incarnation
			&& blobsValid && aKey.File.valid() && aKey.Options.valid()
			&& aKey.Algorithm.value() != 0U;
	}

	auto valid(const source_line_ref& aLine,
			   const std::uint32_t    aLeftLineCount,
			   const std::uint32_t    aRightLineCount) noexcept -> bool
	{
		if (!aLine.Comparison.valid() || !valid_side(aLine.Side)) {
			return false;
		}
		return aLine.Side == diff_side::left ? aLine.Line < aLeftLineCount
											 : aLine.Line < aRightLineCount;
	}

	auto valid(const source_boundary_ref& aBoundary,
			   const std::uint32_t        aLeftLineCount,
			   const std::uint32_t        aRightLineCount) noexcept -> bool
	{
		if (!aBoundary.Comparison.valid() || !valid_side(aBoundary.Side)) {
			return false;
		}
		return aBoundary.Side == diff_side::left
			? aBoundary.Boundary <= aLeftLineCount
			: aBoundary.Boundary <= aRightLineCount;
	}

	auto comparison_digest(const file_comparison_key& aKey) noexcept
		-> file_comparison_digest
	{
		if (!valid(aKey)) {
			return { };
		}
		auto encoder = canonical_encoder<640U>{ };
		encoder.append_domain("BPR-file-comparison-v1");
		append_authority(encoder, aKey.Authority);
		encoder.append_u64(aKey.Repository.value());
		encoder.append_u64(aKey.PullRequest.value());
		append_anchor(encoder, aKey.LeftAnchor);
		append_anchor(encoder, aKey.RightAnchor);
		append_blob(encoder, aKey.LeftBlob);
		append_blob(encoder, aKey.RightBlob);
		encoder.append(aKey.File.bytes());
		encoder.append(aKey.Options.bytes());
		encoder.append_u64(aKey.Algorithm.value());
		return identity_from<file_comparison_digest>(encoder);
	}

	auto file_material(const file_comparison_key& aKey) noexcept
		-> file_comparison_digest
	{
		if (!valid(aKey)) {
			return { };
		}
		auto encoder = canonical_encoder<640U>{ };
		encoder.append_domain("BPR-file-material-v1");
		append_authority(encoder, aKey.Authority);
		encoder.append_u64(aKey.Repository.value());
		encoder.append_u64(aKey.PullRequest.value());
		append_blob(encoder, aKey.LeftBlob);
		append_blob(encoder, aKey.RightBlob);
		encoder.append(aKey.File.bytes());
		encoder.append(aKey.Options.bytes());
		encoder.append_u64(aKey.Algorithm.value());
		return identity_from<file_comparison_digest>(encoder);
	}

	auto segment_identity(const file_comparison_digest aComparison,
						  const alignment_kind         aKind,
						  const line_span              aLeft,
						  const line_span              aRight,
						  const std::uint32_t          aOrdinal) noexcept -> alignment_segment_id
	{
		if (!aComparison.valid() || !valid_alignment_kind(aKind)
			|| !segment_shape_valid(aKind, aLeft, aRight)) {
			return { };
		}
		auto encoder = canonical_encoder<96U>{ };
		encoder.append_domain("BPR-alignment-segment-v1");
		encoder.append(aComparison.bytes());
		encoder.append_u8(static_cast<std::uint8_t>(aKind));
		append_span(encoder, aLeft);
		append_span(encoder, aRight);
		encoder.append_u32(aOrdinal);
		return identity_from<alignment_segment_id>(encoder);
	}

	auto pair_identity(const file_comparison_digest aComparison,
					   const alignment_segment_id   aSegment,
					   const std::uint32_t          aLeftLine,
					   const std::uint32_t          aRightLine,
					   const line_pair_kind         aKind,
					   const std::uint32_t          aOrdinal) noexcept -> line_pair_id
	{
		if (!aComparison.valid() || !aSegment.valid() || !valid_line_pair_kind(aKind)) {
			return { };
		}
		auto encoder = canonical_encoder<112U>{ };
		encoder.append_domain("BPR-line-pair-v1");
		encoder.append(aComparison.bytes());
		encoder.append(aSegment.bytes());
		encoder.append_u32(aLeftLine);
		encoder.append_u32(aRightLine);
		encoder.append_u8(static_cast<std::uint8_t>(aKind));
		encoder.append_u32(aOrdinal);
		return identity_from<line_pair_id>(encoder);
	}

	auto group_identity(const file_comparison_digest aComparison,
						const line_span              aLeft,
						const line_span              aRight,
						const std::uint32_t          aOrdinal) noexcept -> change_group_id
	{
		if (!aComparison.valid() || aLeft.Begin > aLeft.End || aRight.Begin > aRight.End
			|| (aLeft.empty() && aRight.empty())) {
			return { };
		}
		auto encoder = canonical_encoder<80U>{ };
		encoder.append_domain("BPR-change-group-v1");
		encoder.append(aComparison.bytes());
		append_span(encoder, aLeft);
		append_span(encoder, aRight);
		encoder.append_u32(aOrdinal);
		return identity_from<change_group_id>(encoder);
	}

	auto move_identity(const file_comparison_digest aComparison,
					   const alignment_segment_id   aSourceDeletion,
					   const alignment_segment_id   aDestinationInsertion,
					   const std::uint32_t          aOrdinal) noexcept -> move_relation_id
	{
		if (!aComparison.valid() || !aSourceDeletion.valid()
			|| !aDestinationInsertion.valid()
			|| aSourceDeletion == aDestinationInsertion) {
			return { };
		}
		auto encoder = canonical_encoder<128U>{ };
		encoder.append_domain("BPR-move-relation-v1");
		encoder.append(aComparison.bytes());
		encoder.append(aSourceDeletion.bytes());
		encoder.append(aDestinationInsertion.bytes());
		encoder.append_u32(aOrdinal);
		return identity_from<move_relation_id>(encoder);
	}

	namespace {
		auto left_present(const file_comparison_key& aKey) noexcept -> bool
		{
			return aKey.LeftBlob.has_value();
		}
		auto right_present(const file_comparison_key& aKey) noexcept -> bool
		{
			return aKey.RightBlob.has_value();
		}
		auto key_digest(const file_comparison_key& aKey) noexcept -> file_comparison_digest
		{
			return comparison_digest(aKey);
		}
		auto left_present(const material_comparison_key& aKey) noexcept -> bool
		{
			return aKey.LeftPresent;
		}
		auto right_present(const material_comparison_key& aKey) noexcept -> bool
		{
			return aKey.RightPresent;
		}
		auto key_digest(const material_comparison_key& aKey) noexcept -> file_comparison_digest
		{
			return valid(aKey) ? aKey.Comparison : file_comparison_digest{ };
		}

		/// The alignment rules, shared by pull-request and material comparisons.
		template <typename TAlignment>
		auto validate_alignment_records(const TAlignment& aAlignment)
			-> alignment_validation_result
		{
			constexpr auto maximumRecords = static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max());
			if (aAlignment.Segments.size() > maximumRecords
				|| aAlignment.Pairs.size() > maximumRecords
				|| aAlignment.Groups.size() > maximumRecords
				|| aAlignment.Moves.size() > maximumRecords) {
				return failure(alignment_validation_error::count_overflow);
			}
			if (!valid(aAlignment.Key)) {
				return failure(alignment_validation_error::invalid_comparison_key);
			}
			const auto normalizedOptionsDigest = options_digest(aAlignment.Options);
			if (!normalizedOptionsDigest.has_value()
				|| *normalizedOptionsDigest != aAlignment.Key.Options) {
				return failure(alignment_validation_error::options_digest_mismatch);
			}
			if (aAlignment.Digest != key_digest(aAlignment.Key)) {
				return failure(
					alignment_validation_error::comparison_digest_mismatch);
			}
			if (!valid_diagnostic(aAlignment.Content, aAlignment.Diagnostic.Code)) {
				return failure(
					alignment_validation_error::invalid_content_diagnostic);
			}
			if (aAlignment.Content == diff_content_state::unavailable
				&& ((aAlignment.Diagnostic.Code == diff_reason_code::missing_left_blob
					 && !left_present(aAlignment.Key))
					|| (aAlignment.Diagnostic.Code
							== diff_reason_code::missing_right_blob
						&& !right_present(aAlignment.Key)))) {
				return failure(
					alignment_validation_error::invalid_content_diagnostic);
			}
			if (aAlignment.Content != diff_content_state::text) {
				if (aAlignment.LeftLineCount != 0U || aAlignment.RightLineCount != 0U
					|| !aAlignment.Segments.empty() || !aAlignment.Pairs.empty()
					|| !aAlignment.Groups.empty() || !aAlignment.Moves.empty()) {
					return failure(alignment_validation_error::records_for_non_text);
				}
				return { };
			}
			if ((aAlignment.Options.Pairing == pairing_policy::disabled
				 && !aAlignment.Pairs.empty())
				|| (aAlignment.Options.Moves == move_detection_policy::disabled
					&& !aAlignment.Moves.empty())) {
				return failure(
					alignment_validation_error::records_disabled_by_options);
			}
			if ((!left_present(aAlignment.Key)
				 && aAlignment.LeftLineCount != 0U)
				|| (!right_present(aAlignment.Key)
					&& aAlignment.RightLineCount != 0U)) {
				return failure(
					alignment_validation_error::invalid_content_diagnostic);
			}
			if (aAlignment.Segments.empty()) {
				if (aAlignment.LeftLineCount != 0U || aAlignment.RightLineCount != 0U) {
					return failure(alignment_validation_error::missing_text_segments);
				}
				return aAlignment.Pairs.empty() && aAlignment.Groups.empty()
						&& aAlignment.Moves.empty()
					? alignment_validation_result{ }
					: failure(alignment_validation_error::records_without_segments);
			}

			auto segmentIndexes = std::map<alignment_segment_id, std::size_t>{ };
			auto expectedLeft   = std::uint32_t{ 0U };
			auto expectedRight  = std::uint32_t{ 0U };
			for (std::size_t index = 0U; index < aAlignment.Segments.size(); ++index) {
				const auto& segment = aAlignment.Segments[index];
				const auto  ordinal = static_cast<std::uint32_t>(index);
				if (!valid(segment.Left, aAlignment.LeftLineCount)
					|| !valid(segment.Right, aAlignment.RightLineCount)) {
					return failure(
						alignment_validation_error::invalid_segment_span, index);
				}
				if (segment.Left.Begin != expectedLeft) {
					return failure(
						alignment_validation_error::left_coverage_discontinuity,
						index);
				}
				if (segment.Right.Begin != expectedRight) {
					return failure(
						alignment_validation_error::right_coverage_discontinuity,
						index);
				}
				if (segment.Left.empty() && segment.Right.empty()) {
					return failure(alignment_validation_error::empty_segment, index);
				}
				if (!segment_shape_valid(segment.Kind, segment.Left, segment.Right)) {
					return failure(
						alignment_validation_error::segment_kind_mismatch, index);
				}
				if (segment.Kind == alignment_kind::equal) {
					if (segment.Group.has_value()) {
						return failure(
							alignment_validation_error::grouped_equal_segment,
							index);
					}
				} else if (!segment.Group.has_value()) {
					return failure(
						alignment_validation_error::ungrouped_change, index);
				}
				if (!segment.Id.valid()
					|| segment.Id
						!= segment_identity(aAlignment.Digest,
											segment.Kind,
											segment.Left,
											segment.Right,
											ordinal)) {
					return failure(
						alignment_validation_error::invalid_segment_id, index);
				}
				const auto [iterator, inserted] = segmentIndexes.emplace(segment.Id, index);
				if (!inserted) {
					return failure(alignment_validation_error::duplicate_segment_id,
								   index,
								   iterator->second);
				}
				expectedLeft  = segment.Left.End;
				expectedRight = segment.Right.End;
			}
			if (expectedLeft != aAlignment.LeftLineCount) {
				return failure(
					alignment_validation_error::incomplete_left_coverage);
			}
			if (expectedRight != aAlignment.RightLineCount) {
				return failure(
					alignment_validation_error::incomplete_right_coverage);
			}

			auto groupIndexes  = std::map<change_group_id, std::size_t>{ };
			auto priorGroupEnd = std::uint32_t{ 0U };
			for (std::size_t index = 0U; index < aAlignment.Groups.size(); ++index) {
				const auto& group        = aAlignment.Groups[index];
				const auto  ordinal      = static_cast<std::uint32_t>(index);
				const auto  segmentCount = static_cast<std::uint32_t>(aAlignment.Segments.size());
				if (group.Members.Begin > group.Members.End
					|| group.Members.End > segmentCount) {
					return failure(
						alignment_validation_error::invalid_group_span, index);
				}
				if (group.Members.empty()) {
					return failure(alignment_validation_error::empty_group, index);
				}
				if (index != 0U && group.Members.Begin < priorGroupEnd) {
					return failure(
						alignment_validation_error::unordered_group, index);
				}
				if (index != 0U && group.Members.Begin == priorGroupEnd) {
					return failure(
						alignment_validation_error::noncanonical_adjacent_group,
						index,
						index - 1U);
				}

				const auto  firstIndex = static_cast<std::size_t>(group.Members.Begin);
				const auto  lastIndex  = static_cast<std::size_t>(group.Members.End - 1U);
				const auto& first      = aAlignment.Segments[firstIndex];
				const auto& last       = aAlignment.Segments[lastIndex];
				if (group.Left
						!= line_span{ .Begin = first.Left.Begin, .End = last.Left.End }
					|| group.Right
						!= line_span{ .Begin = first.Right.Begin, .End = last.Right.End }) {
					return failure(
						alignment_validation_error::group_span_mismatch, index);
				}
				if (group.First
						!= alignment_boundary{
							.Left  = first.Left.Begin,
							.Right = first.Right.Begin,
						}
					|| group.Last != alignment_boundary{
						   .Left  = last.Left.End,
						   .Right = last.Right.End,
					   }) {
					return failure(
						alignment_validation_error::group_boundary_mismatch, index);
				}

				auto added         = std::uint64_t{ 0U };
				auto deleted       = std::uint64_t{ 0U };
				auto replacedLeft  = std::uint64_t{ 0U };
				auto replacedRight = std::uint64_t{ 0U };
				for (auto member = group.Members.Begin; member < group.Members.End;
					 ++member) {
					const auto& segment = aAlignment.Segments[static_cast<std::size_t>(member)];
					if (segment.Kind == alignment_kind::equal
						|| !segment.Group.has_value() || *segment.Group != group.Id) {
						return failure(
							alignment_validation_error::noncontiguous_group,
							index,
							static_cast<std::size_t>(member));
					}
					switch (segment.Kind) {
					case alignment_kind::right_only:
						added += segment.Right.extent();
						break;
					case alignment_kind::left_only:
						deleted += segment.Left.extent();
						break;
					case alignment_kind::replacement:
						replacedLeft += segment.Left.extent();
						replacedRight += segment.Right.extent();
						break;
					case alignment_kind::equal:
						break;
					}
				}
				if (group.AddedLines != added || group.DeletedLines != deleted
					|| group.ReplacedLeftLines != replacedLeft
					|| group.ReplacedRightLines != replacedRight
					|| group.Kind
						!= summary_kind(
							added, deleted, replacedLeft, replacedRight)) {
					return failure(
						alignment_validation_error::group_summary_mismatch, index);
				}
				if (!group.Id.valid()
					|| group.Id
						!= group_identity(
							aAlignment.Digest, group.Left, group.Right, ordinal)) {
					return failure(
						alignment_validation_error::invalid_group_id, index);
				}
				const auto [iterator, inserted] = groupIndexes.emplace(group.Id, index);
				if (!inserted) {
					return failure(alignment_validation_error::duplicate_group_id,
								   index,
								   iterator->second);
				}
				priorGroupEnd = group.Members.End;
			}

			for (std::size_t index = 0U; index < aAlignment.Segments.size(); ++index) {
				const auto& segment = aAlignment.Segments[index];
				if (segment.Kind == alignment_kind::equal) {
					continue;
				}
				const auto group = groupIndexes.find(*segment.Group);
				if (group == groupIndexes.end()) {
					return failure(
						alignment_validation_error::segment_group_mismatch, index);
				}
				const auto& members = aAlignment.Groups[group->second].Members;
				if (index < static_cast<std::size_t>(members.Begin)
					|| index >= static_cast<std::size_t>(members.End)) {
					return failure(alignment_validation_error::segment_group_mismatch,
								   index,
								   group->second);
				}
			}

			auto pairIndexes      = std::map<line_pair_id, std::size_t>{ };
			auto pairedSegments   = std::set<std::size_t>{ };
			auto priorPairSegment = std::optional<std::size_t>{ };
			auto priorLeft        = std::uint32_t{ 0U };
			auto priorRight       = std::uint32_t{ 0U };
			for (std::size_t index = 0U; index < aAlignment.Pairs.size(); ++index) {
				const auto& pair    = aAlignment.Pairs[index];
				const auto  ordinal = static_cast<std::uint32_t>(index);
				if (!valid_line_pair_kind(pair.Kind)) {
					return failure(
						alignment_validation_error::invalid_pair_kind, index);
				}
				const auto foundSegment = segmentIndexes.find(pair.Segment);
				if (foundSegment == segmentIndexes.end()) {
					return failure(
						alignment_validation_error::unknown_pair_segment, index);
				}
				const auto& segment = aAlignment.Segments[foundSegment->second];
				if (segment.Kind != alignment_kind::replacement
					|| pair.LeftLine < segment.Left.Begin
					|| pair.LeftLine >= segment.Left.End
					|| pair.RightLine < segment.Right.Begin
					|| pair.RightLine >= segment.Right.End) {
					return failure(
						alignment_validation_error::pair_out_of_bounds, index);
				}
				if (priorPairSegment.has_value()
					&& (foundSegment->second < *priorPairSegment
						|| (foundSegment->second == *priorPairSegment
							&& (pair.LeftLine <= priorLeft
								|| pair.RightLine <= priorRight)))) {
					return failure(
						alignment_validation_error::nonmonotonic_pair, index);
				}
				if (!pair.Id.valid()
					|| pair.Id
						!= pair_identity(aAlignment.Digest,
										 pair.Segment,
										 pair.LeftLine,
										 pair.RightLine,
										 pair.Kind,
										 ordinal)) {
					return failure(
						alignment_validation_error::invalid_pair_id, index);
				}
				const auto [iterator, inserted] = pairIndexes.emplace(pair.Id, index);
				if (!inserted) {
					return failure(alignment_validation_error::duplicate_pair_id,
								   index,
								   iterator->second);
				}
				pairedSegments.insert(foundSegment->second);
				priorPairSegment = foundSegment->second;
				priorLeft        = pair.LeftLine;
				priorRight       = pair.RightLine;
			}

			auto moveIndexes       = std::map<move_relation_id, std::size_t>{ };
			auto movePairs         = std::map<std::pair<std::size_t, std::size_t>, std::size_t>{ };
			auto movedSources      = std::set<std::size_t>{ };
			auto movedDestinations = std::set<std::size_t>{ };
			auto priorMovePair     = std::optional<std::pair<std::size_t, std::size_t>>{ };
			for (std::size_t index = 0U; index < aAlignment.Moves.size(); ++index) {
				const auto& move        = aAlignment.Moves[index];
				const auto  ordinal     = static_cast<std::uint32_t>(index);
				const auto  source      = segmentIndexes.find(move.SourceDeletion);
				const auto  destination = segmentIndexes.find(move.DestinationInsertion);
				if (source == segmentIndexes.end()
					|| destination == segmentIndexes.end()) {
					return failure(
						alignment_validation_error::unknown_move_segment, index);
				}
				if (aAlignment.Segments[source->second].Kind
						!= alignment_kind::left_only
					|| aAlignment.Segments[destination->second].Kind
						!= alignment_kind::right_only
					|| aAlignment.Segments[source->second].Left.extent()
						!= aAlignment.Segments[destination->second].Right.extent()) {
					return failure(
						alignment_validation_error::invalid_move_topology, index);
				}
				const auto currentMovePair  = std::pair{ source->second, destination->second };
				const auto existingRelation = movePairs.find(currentMovePair);
				if (existingRelation != movePairs.end()) {
					return failure(
						alignment_validation_error::duplicate_move_relation,
						index,
						existingRelation->second);
				}
				if (movedSources.contains(source->second)
					|| movedDestinations.contains(destination->second)) {
					return failure(
						alignment_validation_error::invalid_move_topology, index);
				}
				if (priorMovePair.has_value()
					&& currentMovePair < *priorMovePair) {
					return failure(
						alignment_validation_error::noncanonical_move_order,
						index,
						index - 1U);
				}
				if (!move.Id.valid()
					|| move.Id
						!= move_identity(aAlignment.Digest,
										 move.SourceDeletion,
										 move.DestinationInsertion,
										 ordinal)) {
					return failure(
						alignment_validation_error::invalid_move_id, index);
				}
				const auto [iterator, inserted] = moveIndexes.emplace(move.Id, index);
				if (!inserted) {
					return failure(alignment_validation_error::duplicate_move_id,
								   index,
								   iterator->second);
				}
				movePairs.emplace(currentMovePair, index);
				movedSources.insert(source->second);
				movedDestinations.insert(destination->second);
				priorMovePair = currentMovePair;
			}

			// Same-kind boundaries are canonical only when secondary semantic
			// metadata needs the two stable segment identities to remain distinct.
			for (std::size_t index = 1U; index < aAlignment.Segments.size(); ++index) {
				const auto& prior   = aAlignment.Segments[index - 1U];
				const auto& current = aAlignment.Segments[index];
				if (prior.Kind != current.Kind) {
					continue;
				}
				const auto pairRefinement = current.Kind == alignment_kind::replacement
					&& (pairedSegments.contains(index - 1U)
						|| pairedSegments.contains(index));
				const auto movedDeletionBoundary = current.Kind == alignment_kind::left_only
					&& (movedSources.contains(index - 1U)
						|| movedSources.contains(index));
				const auto movedInsertionBoundary = current.Kind == alignment_kind::right_only
					&& (movedDestinations.contains(index - 1U)
						|| movedDestinations.contains(index));
				if (!pairRefinement && !movedDeletionBoundary
					&& !movedInsertionBoundary) {
					return failure(
						alignment_validation_error::noncanonical_adjacent_segment,
						index,
						index - 1U);
				}
			}
			return { };
		}
	}

	auto valid(const material_comparison_key& aKey) noexcept -> bool
	{
		return aKey.Comparison.valid() && (aKey.LeftPresent || aKey.RightPresent) && aKey.Options.valid() && aKey.Algorithm.value() != 0U;
	}

	auto comparison_digest(const material_comparison_key& aKey) noexcept
		-> file_comparison_digest
	{
		return valid(aKey) ? aKey.Comparison : file_comparison_digest{ };
	}

	auto validate_alignment(const file_alignment& aAlignment)
		-> alignment_validation_result
	{
		return validate_alignment_records(aAlignment);
	}

	auto validate_alignment(const material_alignment& aAlignment)
		-> alignment_validation_result
	{
		return validate_alignment_records(aAlignment);
	}

} // namespace bpr::core
