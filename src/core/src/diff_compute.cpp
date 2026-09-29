#include "bpr/core/diff_compute.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace bpr::core {
	namespace {
		struct unicode_scalar
		{
			std::uint32_t Value = 0U;
			std::uint64_t Begin = 0U;
			std::uint64_t End   = 0U;
		};

		struct normalized_line
		{
			std::vector<std::uint32_t> Values;

			auto operator<=>(const normalized_line&) const = default;
		};

		struct normalized_line_pointer_less
		{
			[[nodiscard]] auto operator()(
				const normalized_line* const aLeft,
				const normalized_line* const aRight) const noexcept -> bool
			{
				return *aLeft < *aRight;
			}
		};

		struct prepared_document
		{
			decoded_text_document        Document;
			std::vector<normalized_line> Normalized;
		};

		struct pending_segment
		{
			alignment_kind                Kind = alignment_kind::equal;
			line_span                     Left;
			line_span                     Right;
			std::optional<line_pair_kind> PairKind;
		};

		struct accepted_pair
		{
			std::uint32_t  Left  = 0U;
			std::uint32_t  Right = 0U;
			line_pair_kind Kind  = line_pair_kind::similar;
		};

		struct work_budget
		{
			std::uint64_t Limit    = 0U;
			std::uint64_t Consumed = 0U;

			[[nodiscard]] auto consume(
				const std::uint64_t aAmount = 1U) noexcept -> bool
			{
				if (aAmount > Limit || Consumed > Limit - aAmount) {
					return false;
				}
				Consumed += aAmount;
				return true;
			}
		};

		[[nodiscard]] constexpr auto indexed_probe_work(
			const std::size_t aEntries,
			const std::size_t aLineValues) noexcept -> std::uint64_t
		{
			const auto comparisons = 1U
				+ 2U * static_cast<std::uint64_t>(std::bit_width(aEntries));
			return comparisons * (1U + static_cast<std::uint64_t>(aLineValues));
		}

		[[nodiscard]] constexpr auto line_comparison_work(
			const normalized_line& aLeft,
			const normalized_line& aRight) noexcept -> std::uint64_t
		{
			return 1U + static_cast<std::uint64_t>(std::min(aLeft.Values.size(), aRight.Values.size()));
		}

		[[nodiscard]] constexpr auto sort_work(
			const std::size_t aEntries) noexcept -> std::uint64_t
		{
			return static_cast<std::uint64_t>(aEntries)
				* static_cast<std::uint64_t>(std::bit_width(aEntries));
		}

		struct move_line_location
		{
			std::size_t   Segment = 0U;
			std::uint32_t Line    = 0U;
		};

		struct move_line_occurrences
		{
			std::optional<move_line_location> Left;
			std::optional<move_line_location> Right;
			std::uint32_t                     LeftCount  = 0U;
			std::uint32_t                     RightCount = 0U;
		};

		struct detected_move
		{
			std::size_t SourceOriginal = 0U;
			line_span   SourceLines;
			std::size_t DestinationOriginal = 0U;
			line_span   DestinationLines;
			std::size_t SourceSegment      = 0U;
			std::size_t DestinationSegment = 0U;
		};

		struct moved_refinement
		{
			std::vector<pending_segment>                     Segments;
			std::vector<std::pair<std::size_t, std::size_t>> Moves;
			std::uint64_t                                    WorkConsumed        = 0U;
			bool                                             WorkLimitReached    = false;
			bool                                             SegmentLimitReached = false;
		};

		struct matrix_budget
		{
			std::uint64_t Limit    = 0U;
			std::uint64_t Consumed = 0U;

			[[nodiscard]] auto consume(const std::size_t aLeft,
									   const std::size_t aRight) noexcept -> bool
			{
				if (aLeft == 0U || aRight == 0U) {
					return true;
				}
				const auto leftValue  = static_cast<std::uint64_t>(aLeft);
				const auto rightValue = static_cast<std::uint64_t>(aRight);
				if (leftValue > std::numeric_limits<std::uint64_t>::max() / rightValue) {
					return false;
				}
				const auto cells = leftValue * rightValue;
				if (cells > Limit || Consumed > Limit - cells) {
					return false;
				}
				Consumed += cells;
				return true;
			}
		};

		[[nodiscard]] auto valid_utf8(const std::span<const std::uint8_t> aBytes,
									  std::vector<unicode_scalar>*        aOutput = nullptr)
			-> bool
		{
			auto index = std::size_t{ 0U };
			while (index < aBytes.size()) {
				const auto first   = aBytes[index];
				auto       length  = std::size_t{ 0U };
				auto       value   = std::uint32_t{ 0U };
				auto       minimum = std::uint32_t{ 0U };
				if (first <= 0x7FU) {
					length = 1U;
					value  = first;
				} else if (first >= 0xC2U && first <= 0xDFU) {
					length  = 2U;
					value   = static_cast<std::uint32_t>(first & 0x1FU);
					minimum = 0x80U;
				} else if (first >= 0xE0U && first <= 0xEFU) {
					length  = 3U;
					value   = static_cast<std::uint32_t>(first & 0x0FU);
					minimum = 0x800U;
				} else if (first >= 0xF0U && first <= 0xF4U) {
					length  = 4U;
					value   = static_cast<std::uint32_t>(first & 0x07U);
					minimum = 0x10000U;
				} else {
					return false;
				}
				if (length > aBytes.size() - index) {
					return false;
				}
				for (std::size_t offset = 1U; offset < length; ++offset) {
					const auto continuation = aBytes[index + offset];
					if ((continuation & 0xC0U) != 0x80U) {
						return false;
					}
					value = static_cast<std::uint32_t>((value << 6U) | (continuation & 0x3FU));
				}
				if (value < minimum || value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU)) {
					return false;
				}
				if (aOutput != nullptr) {
					aOutput->push_back(unicode_scalar{
						.Value = value,
						.Begin = static_cast<std::uint64_t>(index),
						.End   = static_cast<std::uint64_t>(index + length),
					});
				}
				index += length;
			}
			return true;
		}

		[[nodiscard]] auto line_bytes(const decoded_text_document& aDocument,
									  const std::uint32_t          aLine)
			-> std::span<const std::uint8_t>
		{
			const auto& record = aDocument.Lines[static_cast<std::size_t>(aLine)];
			const auto  begin  = static_cast<std::size_t>(record.Content.Begin);
			const auto  end    = static_cast<std::size_t>(record.Content.End);
			return std::span<const std::uint8_t>{ aDocument.Bytes }.subspan(begin,
																			end - begin);
		}

		[[nodiscard]] auto byte_boundary(const decoded_text_document& aDocument,
										 const decoded_text_line&     aLine,
										 const std::uint64_t          aOffset) -> bool
		{
			const auto extent = aLine.Content.End - aLine.Content.Begin;
			if (aOffset > extent) {
				return false;
			}
			if (aOffset == 0U || aOffset == extent) {
				return true;
			}
			const auto byte = aDocument.Bytes[static_cast<std::size_t>(aLine.Content.Begin + aOffset)];
			return (byte & 0xC0U) != 0x80U;
		}

		[[nodiscard]] auto valid_document(const decoded_text_document& aDocument) -> bool
		{
			if (!valid_utf8(aDocument.Bytes) || std::ranges::find(aDocument.Bytes, std::uint8_t{ 0U }) != aDocument.Bytes.end()) {
				return false;
			}
			if (aDocument.Lines.empty()) {
				return aDocument.Bytes.empty() && !aDocument.HasFinalNewline;
			}

			auto cursor = std::uint64_t{ 0U };
			for (std::size_t index = 0U; index < aDocument.Lines.size(); ++index) {
				const auto& line = aDocument.Lines[index];
				if (line.Content.Begin != cursor || line.Content.Begin > line.Content.End || line.Content.End != line.Terminator.Begin || line.Terminator.Begin > line.Terminator.End || line.Terminator.End > aDocument.Bytes.size()) {
					return false;
				}
				const auto begin  = static_cast<std::size_t>(line.Terminator.Begin);
				const auto extent = line.Terminator.End - line.Terminator.Begin;
				switch (line.Ending) {
				case text_line_ending::none:
					if (extent != 0U || index + 1U != aDocument.Lines.size()) {
						return false;
					}
					break;
				case text_line_ending::lf:
					if (extent != 1U || aDocument.Bytes[begin] != '\n') {
						return false;
					}
					break;
				case text_line_ending::cr:
					if (extent != 1U || aDocument.Bytes[begin] != '\r') {
						return false;
					}
					break;
				case text_line_ending::crlf:
					if (extent != 2U || aDocument.Bytes[begin] != '\r' || aDocument.Bytes[begin + 1U] != '\n') {
						return false;
					}
					break;
				}
				cursor = line.Terminator.End;
			}
			return cursor == aDocument.Bytes.size() && aDocument.HasFinalNewline == (aDocument.Lines.back().Ending != text_line_ending::none);
		}

		[[nodiscard]] constexpr auto compute_failure(
			const diff_compute_validation_error aError,
			const std::size_t                   aRecord = diff_compute_validation_result::no_record)
			-> diff_compute_validation_result
		{
			return { .Error = aError, .Record = aRecord };
		}

		[[nodiscard]] auto normalize_line(const decoded_text_document& aDocument,
										  const decoded_text_line&     aLine,
										  const diff_options&          aOptions)
			-> normalized_line
		{
			const auto begin   = static_cast<std::size_t>(aLine.Content.Begin);
			const auto end     = static_cast<std::size_t>(aLine.Content.End);
			auto       scalars = std::vector<unicode_scalar>{ };
			static_cast<void>(
				valid_utf8(std::span<const std::uint8_t>{ aDocument.Bytes }.subspan(
							   begin, end - begin),
						   &scalars));
			auto result = normalized_line{ };
			result.Values.reserve(scalars.size() + 1U);
			for (const auto& scalar : scalars) {
				if (aOptions.Whitespace == whitespace_policy::ignore_all && (scalar.Value == 0x20U || scalar.Value == 0x09U)) {
					continue;
				}
				result.Values.push_back(scalar.Value);
			}
			if (aOptions.Whitespace == whitespace_policy::ignore_trailing) {
				while (!result.Values.empty() && (result.Values.back() == 0x20U || result.Values.back() == 0x09U)) {
					result.Values.pop_back();
				}
			}
			if (aLine.Ending != text_line_ending::none) {
				const auto marker = aOptions.LineEndings == line_ending_policy::normalize
					? 0x110000U
					: aLine.Ending == text_line_ending::lf ? 0x110000U
					: aLine.Ending == text_line_ending::cr ? 0x110001U
														   : 0x110002U;
				result.Values.push_back(marker);
			}
			return result;
		}

		[[nodiscard]] auto normalized_lines_equal(const decoded_text_document& aLeft,
												  const std::uint32_t          aLeftLine,
												  const decoded_text_document& aRight,
												  const std::uint32_t          aRightLine,
												  const diff_options&          aOptions) -> bool
		{
			return normalize_line(aLeft,
								  aLeft.Lines[static_cast<std::size_t>(aLeftLine)],
								  aOptions)
				== normalize_line(aRight,
								  aRight.Lines[static_cast<std::size_t>(aRightLine)],
								  aOptions);
		}

		[[nodiscard]] auto prepare_document(const std::span<const std::uint8_t> aBytes,
											const diff_options&                 aOptions,
											const std::uint32_t                 aMaximumLines)
			-> std::optional<prepared_document>
		{
			if (!valid_utf8(aBytes)) {
				return std::nullopt;
			}
			auto result = prepared_document{ };
			result.Document.Bytes.assign(aBytes.begin(), aBytes.end());
			auto lineStart = std::size_t{ 0U };
			for (std::size_t index = 0U; index < aBytes.size(); ++index) {
				const auto current = aBytes[index];
				if (current != static_cast<std::uint8_t>('\n') && current != static_cast<std::uint8_t>('\r')) {
					continue;
				}
				const auto crlf   = current == static_cast<std::uint8_t>('\r') && index + 1U < aBytes.size() && aBytes[index + 1U] == static_cast<std::uint8_t>('\n');
				const auto ending = crlf ? text_line_ending::crlf
					: current == static_cast<std::uint8_t>('\r')
					? text_line_ending::cr
					: text_line_ending::lf;
				const auto end    = crlf ? index + 2U : index + 1U;
				result.Document.Lines.push_back(decoded_text_line{
					.Content    = { static_cast<std::uint64_t>(lineStart),
									static_cast<std::uint64_t>(index) },
					.Terminator = { static_cast<std::uint64_t>(index),
									static_cast<std::uint64_t>(end) },
					.Ending     = ending,
				});
				if (result.Document.Lines.size() > static_cast<std::size_t>(aMaximumLines)) {
					return std::nullopt;
				}
				lineStart = end;
				if (crlf) {
					++index;
				}
			}
			if (lineStart < aBytes.size()) {
				result.Document.Lines.push_back(decoded_text_line{
					.Content    = { static_cast<std::uint64_t>(lineStart),
									static_cast<std::uint64_t>(aBytes.size()) },
					.Terminator = { static_cast<std::uint64_t>(aBytes.size()),
									static_cast<std::uint64_t>(aBytes.size()) },
					.Ending     = text_line_ending::none,
				});
			}
			if (result.Document.Lines.size() > static_cast<std::size_t>(aMaximumLines)) {
				return std::nullopt;
			}
			result.Document.HasFinalNewline = !aBytes.empty() && (aBytes.back() == static_cast<std::uint8_t>('\n') || aBytes.back() == static_cast<std::uint8_t>('\r'));
			result.Normalized.reserve(result.Document.Lines.size());
			for (const auto& line : result.Document.Lines) {
				result.Normalized.push_back(
					normalize_line(result.Document, line, aOptions));
			}
			return result;
		}

		template <typename Left, typename Right, typename Equal>
		[[nodiscard]] auto lcs_matches(const Left&    aLeft,
									   const Right&   aRight,
									   Equal          aEqual,
									   matrix_budget& aBudget)
			-> std::optional<std::vector<std::pair<std::size_t, std::size_t>>>
		{
			if (aLeft.empty() || aRight.empty()) {
				return std::vector<std::pair<std::size_t, std::size_t>>{ };
			}
			if (aLeft.size() == std::numeric_limits<std::size_t>::max() || aRight.size() == std::numeric_limits<std::size_t>::max()) {
				return std::nullopt;
			}
			const auto rows    = aLeft.size() + 1U;
			const auto columns = aRight.size() + 1U;
			if (rows > std::numeric_limits<std::size_t>::max() / columns) {
				return std::nullopt;
			}
			if (!aBudget.consume(rows, columns)) {
				return std::nullopt;
			}
			auto       table = std::vector<std::uint32_t>(rows * columns, 0U);
			const auto at    = [&table,
								columns](const std::size_t aRow,
										 const std::size_t aColumn) -> std::uint32_t& { return table[aRow * columns + aColumn]; };
			for (auto row = aLeft.size(); row != 0U; --row) {
				for (auto column = aRight.size(); column != 0U; --column) {
					if (aEqual(aLeft[row - 1U], aRight[column - 1U])) {
						at(row - 1U, column - 1U) = static_cast<std::uint32_t>(1U + at(row, column));
					} else {
						at(row - 1U, column - 1U) = std::max(at(row, column - 1U), at(row - 1U, column));
					}
				}
			}
			auto result = std::vector<std::pair<std::size_t, std::size_t>>{ };
			auto row    = std::size_t{ 0U };
			auto column = std::size_t{ 0U };
			while (row < aLeft.size() && column < aRight.size()) {
				if (aEqual(aLeft[row], aRight[column])) {
					result.emplace_back(row, column);
					++row;
					++column;
				} else if (at(row + 1U, column) >= at(row, column + 1U)) {
					++row;
				} else {
					++column;
				}
			}
			return result;
		}

		[[nodiscard]] auto similarity(const normalized_line& aLeft,
									  const normalized_line& aRight,
									  matrix_budget&         aBudget)
			-> std::optional<std::uint16_t>
		{
			if (aLeft == aRight) {
				return maximum_similarity_threshold_permille;
			}
			if (aLeft.Values.empty() || aRight.Values.empty()) {
				return std::uint16_t{ 0U };
			}
			const auto matches = lcs_matches(
				aLeft.Values,
				aRight.Values,
				[](const std::uint32_t aLhs, const std::uint32_t aRhs) { return aLhs == aRhs; },
				aBudget);
			if (!matches.has_value()) {
				return std::nullopt;
			}
			const auto numerator   = static_cast<std::uint64_t>(matches->size()) * 2U * maximum_similarity_threshold_permille;
			const auto denominator = static_cast<std::uint64_t>(aLeft.Values.size()) + static_cast<std::uint64_t>(aRight.Values.size());
			return static_cast<std::uint16_t>(numerator / denominator);
		}

		[[nodiscard]] auto secondary_pairs(const prepared_document& aLeft,
										   const prepared_document& aRight,
										   const line_span          aLeftSpan,
										   const line_span          aRightSpan,
										   const diff_options&      aOptions,
										   matrix_budget&           aBudget)
			-> std::optional<std::vector<accepted_pair>>
		{
			if (aOptions.Pairing == pairing_policy::disabled) {
				return std::vector<accepted_pair>{ };
			}
			const auto leftCount  = static_cast<std::size_t>(aLeftSpan.extent());
			const auto rightCount = static_cast<std::size_t>(aRightSpan.extent());
			if ((leftCount != 0U && rightCount > std::numeric_limits<std::size_t>::max() / leftCount) || leftCount == std::numeric_limits<std::size_t>::max() || rightCount == std::numeric_limits<std::size_t>::max()) {
				return std::nullopt;
			}
			const auto cells   = leftCount * rightCount;
			const auto rows    = leftCount + 1U;
			const auto columns = rightCount + 1U;
			if (rows > std::numeric_limits<std::size_t>::max() / columns) {
				return std::nullopt;
			}
			if (!aBudget.consume(leftCount, rightCount) || !aBudget.consume(rows, columns)) {
				return std::nullopt;
			}
			auto scores = std::vector<std::uint16_t>(cells, 0U);
			auto table  = std::vector<std::uint64_t>(rows * columns, 0U);
			for (std::size_t leftIndex = 0U; leftIndex < leftCount; ++leftIndex) {
				for (std::size_t rightIndex = 0U; rightIndex < rightCount;
					 ++rightIndex) {
					const auto score = similarity(
						aLeft.Normalized[static_cast<std::size_t>(aLeftSpan.Begin) + leftIndex],
						aRight.Normalized[static_cast<std::size_t>(aRightSpan.Begin) + rightIndex],
						aBudget);
					if (!score.has_value()) {
						return std::nullopt;
					}
					scores[leftIndex * rightCount + rightIndex] = *score;
				}
			}
			const auto at = [&table,
							 columns](const std::size_t aRow,
									  const std::size_t aColumn) -> std::uint64_t& { return table[aRow * columns + aColumn]; };
			for (auto row = leftCount; row != 0U; --row) {
				for (auto column = rightCount; column != 0U; --column) {
					const auto score = scores[(row - 1U) * rightCount + column - 1U];
					auto       best  = std::max(at(row, column - 1U), at(row - 1U, column));
					if (score >= aOptions.SimilarityThresholdPermille) {
						best = std::max(
							best, static_cast<std::uint64_t>(score) + at(row, column));
					}
					at(row - 1U, column - 1U) = best;
				}
			}
			auto result = std::vector<accepted_pair>{ };
			auto row    = std::size_t{ 0U };
			auto column = std::size_t{ 0U };
			while (row < leftCount && column < rightCount) {
				const auto score  = scores[row * rightCount + column];
				const auto paired = score >= aOptions.SimilarityThresholdPermille && at(row, column) == static_cast<std::uint64_t>(score) + at(row + 1U, column + 1U);
				if (paired) {
					result.push_back(accepted_pair{
						.Left = static_cast<std::uint32_t>(
							static_cast<std::uint64_t>(aLeftSpan.Begin) + row),
						.Right = static_cast<std::uint32_t>(
							static_cast<std::uint64_t>(aRightSpan.Begin) + column),
						.Kind = score == maximum_similarity_threshold_permille
							? line_pair_kind::exact
							: line_pair_kind::similar,
					});
					++row;
					++column;
				} else if (at(row + 1U, column) >= at(row, column + 1U)) {
					++row;
				} else {
					++column;
				}
			}
			return result;
		}

		auto append_shape(std::vector<pending_segment>&       aSegments,
						  const line_span                     aLeft,
						  const line_span                     aRight,
						  const std::optional<line_pair_kind> aPair = std::nullopt)
			-> void
		{
			if (aLeft.empty() && aRight.empty()) {
				return;
			}
			const auto kind = aLeft.empty() ? alignment_kind::right_only
				: aRight.empty()            ? alignment_kind::left_only
											: alignment_kind::replacement;
			aSegments.push_back(pending_segment{
				.Kind     = kind,
				.Left     = aLeft,
				.Right    = aRight,
				.PairKind = aPair,
			});
		}

		struct primary_index_limit
		{ };

		struct budgeted_line_less
		{
			work_budget* Budget = nullptr;

			[[nodiscard]] auto operator()(const normalized_line* aLeft, const normalized_line* aRight) const -> bool
			{
				if (!Budget->consume()) {
					throw primary_index_limit{ };
				}
				const auto extent = std::min(aLeft->Values.size(), aRight->Values.size());
				for (std::size_t index = 0U; index < extent; ++index) {
					if (!Budget->consume()) {
						throw primary_index_limit{ };
					}
					if (aLeft->Values[index] != aRight->Values[index]) {
						return aLeft->Values[index] < aRight->Values[index];
					}
				}
				return aLeft->Values.size() < aRight->Values.size();
			}
		};

		using line_view  = std::span<const normalized_line>;
		using match_list = std::vector<std::pair<std::size_t, std::size_t>>;

		/// All common lines are unique and ordered, so taking every one is the
		/// unique LCS. Ambiguous duplicates or crossing matches decline; this
		/// path never substitutes a different tie-breaking policy.
		[[nodiscard]] auto ordered_unique_matches(const line_view aLeft,
												  const line_view aRight,
												  work_budget&    aBudget)
			-> std::optional<match_list>
		{
			struct occurrence
			{
				std::size_t Position = 0U;
				bool        Repeated = false;
			};
			// Charge actual comparisons, including every normalized value. Tree
			// implementations may call a bool comparator twice at a visited node;
			// an estimated depth is not a bound on that work. The private exception
			// stops the container operation immediately and cannot escape here.
			try {
				std::map<const normalized_line*, occurrence, budgeted_line_less> index{ budgeted_line_less{ &aBudget } };
				for (std::size_t position = 0U; position < aRight.size(); ++position) {
					if (!aBudget.consume()) {
						return std::nullopt;
					}
					const auto [entry, inserted] = index.try_emplace(&aRight[position], occurrence{ .Position = position });
					if (!inserted) {
						entry->second.Repeated = true;
					}
				}
				match_list matches;
				for (std::size_t position = 0U; position < aLeft.size(); ++position) {
					if (!aBudget.consume()) {
						return std::nullopt;
					}
					const auto entry = index.find(&aLeft[position]);
					if (entry == index.end()) {
						continue;
					}
					const auto& found = entry->second;
					if (found.Repeated || (!matches.empty() && found.Position <= matches.back().second)) {
						return std::nullopt;
					}
					matches.emplace_back(position, found.Position);
				}
				return matches;
			} catch (const primary_index_limit&) {
				return std::nullopt;
			}
		}

		/// [[BPR-0719]]: patience anchors. Lines that occur exactly once on each
		/// side, kept in the longest increasing order of their right positions.
		/// The anchors are a valid monotonic alignment, not necessarily the
		/// shortest one, so a caller that relies on them reports approximate.
		[[nodiscard]] auto patience_anchors(const line_view aLeft,
											const line_view aRight,
											work_budget&    aBudget)
			-> std::optional<match_list>
		{
			constexpr auto unseen = std::numeric_limits<std::size_t>::max();
			struct occurrence
			{
				std::size_t Left          = unseen;
				std::size_t Right         = unseen;
				bool        LeftRepeated  = false;
				bool        RightRepeated = false;
			};
			try {
				std::map<const normalized_line*, occurrence, budgeted_line_less> index{ budgeted_line_less{ &aBudget } };
				for (std::size_t position = 0U; position < aRight.size(); ++position) {
					if (!aBudget.consume()) {
						return std::nullopt;
					}
					const auto [entry, inserted] = index.try_emplace(&aRight[position], occurrence{ .Right = position });
					if (!inserted) {
						entry->second.RightRepeated = true;
					}
				}
				match_list candidates;
				for (std::size_t position = 0U; position < aLeft.size(); ++position) {
					if (!aBudget.consume()) {
						return std::nullopt;
					}
					const auto entry = index.find(&aLeft[position]);
					if (entry == index.end()) {
						continue;
					}
					auto& found = entry->second;
					if (found.Left != unseen) {
						found.LeftRepeated = true;
						continue;
					}
					found.Left = position;
					if (!found.RightRepeated) {
						candidates.emplace_back(position, found.Right);
					}
				}
				// A left repeat is discovered after its first occurrence was
				// already recorded; drop those candidates now.
				std::erase_if(candidates, [&index, &aLeft](const std::pair<std::size_t, std::size_t>& aCandidate) {
					return index.find(&aLeft[aCandidate.first])->second.LeftRepeated;
				});
				// Longest strictly increasing subsequence of right positions
				// (candidates are already in left order).
				std::vector<std::size_t> tails;
				std::vector<std::size_t> tailIndex;
				std::vector<std::size_t> previous(candidates.size(), unseen);
				for (std::size_t position = 0U; position < candidates.size(); ++position) {
					if (!aBudget.consume()) {
						return std::nullopt;
					}
					const auto right = candidates[position].second;
					const auto slot  = static_cast<std::size_t>(std::distance(tails.begin(), std::ranges::lower_bound(tails, right)));
					if (slot == tails.size()) {
						tails.push_back(right);
						tailIndex.push_back(position);
					} else {
						tails[slot]     = right;
						tailIndex[slot] = position;
					}
					if (slot != 0U) {
						previous[position] = tailIndex[slot - 1U];
					}
				}
				match_list anchors(tails.size());
				auto       cursor = tails.empty() ? unseen : tailIndex.back();
				for (auto slot = tails.size(); slot != 0U; --slot) {
					anchors[slot - 1U] = candidates[cursor];
					cursor             = previous[cursor];
				}
				return anchors;
			} catch (const primary_index_limit&) {
				return std::nullopt;
			}
		}

		/// [[BPR-0719]]: aligns the whole file without ever refusing it. Each
		/// region first sheds its common prefix and suffix, so the exact matrix
		/// is charged for the changed lines only. A region the matrix budget
		/// cannot cover takes the ordered-unique alignment when that is exact,
		/// otherwise patience anchors split it and the gaps recurse; a gap with
		/// nothing to anchor stays a plain replacement. Every non-exact step
		/// sets aApproximate.
		[[nodiscard]] auto primary_matches(const line_view aLeft,
										   const line_view aRight,
										   matrix_budget&  aBudget,
										   work_budget&    aIndexBudget,
										   bool&           aApproximate)
			-> match_list
		{
			struct region
			{
				std::size_t LeftBegin  = 0U;
				std::size_t LeftEnd    = 0U;
				std::size_t RightBegin = 0U;
				std::size_t RightEnd   = 0U;
			};
			const auto          equalLines = [](const normalized_line& aLhs, const normalized_line& aRhs) { return aLhs == aRhs; };
			match_list          matches;
			std::vector<region> pending{ region{ .LeftEnd = aLeft.size(), .RightEnd = aRight.size() } };
			while (!pending.empty()) {
				auto current = pending.back();
				pending.pop_back();
				while (current.LeftBegin < current.LeftEnd && current.RightBegin < current.RightEnd
					   && equalLines(aLeft[current.LeftBegin], aRight[current.RightBegin])) {
					matches.emplace_back(current.LeftBegin, current.RightBegin);
					++current.LeftBegin;
					++current.RightBegin;
				}
				while (current.LeftBegin < current.LeftEnd && current.RightBegin < current.RightEnd
					   && equalLines(aLeft[current.LeftEnd - 1U], aRight[current.RightEnd - 1U])) {
					--current.LeftEnd;
					--current.RightEnd;
					matches.emplace_back(current.LeftEnd, current.RightEnd);
				}
				if (current.LeftBegin == current.LeftEnd || current.RightBegin == current.RightEnd) {
					continue;
				}
				const auto leftRegion  = aLeft.subspan(current.LeftBegin, current.LeftEnd - current.LeftBegin);
				const auto rightRegion = aRight.subspan(current.RightBegin, current.RightEnd - current.RightBegin);
				const auto offset      = [&matches, &current](const match_list& aFound) {
					for (const auto& [left, right] : aFound) {
						matches.emplace_back(current.LeftBegin + left, current.RightBegin + right);
					}
				};
				if (const auto exact = lcs_matches(leftRegion, rightRegion, equalLines, aBudget); exact.has_value()) {
					offset(*exact);
					continue;
				}
				if (const auto ordered = ordered_unique_matches(leftRegion, rightRegion, aIndexBudget); ordered.has_value()) {
					offset(*ordered);
					continue;
				}
				aApproximate       = true;
				const auto anchors = patience_anchors(leftRegion, rightRegion, aIndexBudget);
				if (!anchors.has_value() || anchors->empty()) {
					continue;
				}
				offset(*anchors);
				auto gapLeft  = current.LeftBegin;
				auto gapRight = current.RightBegin;
				for (const auto& [left, right] : *anchors) {
					pending.push_back(region{ .LeftBegin = gapLeft, .LeftEnd = current.LeftBegin + left, .RightBegin = gapRight, .RightEnd = current.RightBegin + right });
					gapLeft  = current.LeftBegin + left + 1U;
					gapRight = current.RightBegin + right + 1U;
				}
				pending.push_back(region{ .LeftBegin = gapLeft, .LeftEnd = current.LeftEnd, .RightBegin = gapRight, .RightEnd = current.RightEnd });
			}
			std::ranges::sort(matches);
			return matches;
		}

		[[nodiscard]] auto primary_segments(const prepared_document& aLeft,
											const prepared_document& aRight,
											const diff_options&      aOptions,
											matrix_budget&           aBudget,
											work_budget&             aIndexBudget,
											bool&                    aApproximate)
			-> std::optional<std::vector<pending_segment>>
		{
			const auto matches     = std::optional{ primary_matches(
				line_view{ aLeft.Normalized }, line_view{ aRight.Normalized }, aBudget, aIndexBudget, aApproximate) };
			auto       primary     = std::vector<pending_segment>{ };
			auto       leftCursor  = std::uint32_t{ 0U };
			auto       rightCursor = std::uint32_t{ 0U };
			auto       matchIndex  = std::size_t{ 0U };
			while (matchIndex < matches->size()) {
				const auto matchLeft  = static_cast<std::uint32_t>((*matches)[matchIndex].first);
				const auto matchRight = static_cast<std::uint32_t>((*matches)[matchIndex].second);
				append_shape(
					primary, { leftCursor, matchLeft }, { rightCursor, matchRight });
				auto equalLeftEnd  = matchLeft + 1U;
				auto equalRightEnd = matchRight + 1U;
				++matchIndex;
				while (matchIndex < matches->size() && (*matches)[matchIndex].first == equalLeftEnd && (*matches)[matchIndex].second == equalRightEnd) {
					++equalLeftEnd;
					++equalRightEnd;
					++matchIndex;
				}
				primary.push_back(pending_segment{
					.Kind  = alignment_kind::equal,
					.Left  = { matchLeft, equalLeftEnd },
					.Right = { matchRight, equalRightEnd },
				});
				leftCursor  = equalLeftEnd;
				rightCursor = equalRightEnd;
			}
			append_shape(
				primary,
				{ leftCursor, static_cast<std::uint32_t>(aLeft.Normalized.size()) },
				{ rightCursor, static_cast<std::uint32_t>(aRight.Normalized.size()) });

			auto refined = std::vector<pending_segment>{ };
			for (const auto& segment : primary) {
				if (segment.Kind != alignment_kind::replacement) {
					refined.push_back(segment);
					continue;
				}
				const auto pairs = secondary_pairs(
					aLeft, aRight, segment.Left, segment.Right, aOptions, aBudget);
				if (!pairs.has_value()) {
					// [[BPR-0719]]: an unpairable replacement stays a plain
					// replacement instead of refusing the file.
					aApproximate = true;
					refined.push_back(segment);
					continue;
				}
				auto leftPosition  = segment.Left.Begin;
				auto rightPosition = segment.Right.Begin;
				for (const auto& pair : *pairs) {
					append_shape(refined,
								 { leftPosition, pair.Left },
								 { rightPosition, pair.Right });
					append_shape(
						refined,
						{ pair.Left, static_cast<std::uint32_t>(pair.Left + 1U) },
						{ pair.Right, static_cast<std::uint32_t>(pair.Right + 1U) },
						pair.Kind);
					leftPosition  = pair.Left + 1U;
					rightPosition = pair.Right + 1U;
				}
				append_shape(refined,
							 { leftPosition, segment.Left.End },
							 { rightPosition, segment.Right.End });
			}
			return refined;
		}

		[[nodiscard]] constexpr auto
		summary_kind(const std::uint64_t aAdded,
					 const std::uint64_t aDeleted,
					 const std::uint64_t aReplacedLeft,
					 const std::uint64_t aReplacedRight) noexcept -> change_summary_kind
		{
			const auto addition    = aAdded != 0U;
			const auto deletion    = aDeleted != 0U;
			const auto replacement = aReplacedLeft != 0U || aReplacedRight != 0U;
			if (addition && !deletion && !replacement) {
				return change_summary_kind::addition;
			}
			if (!addition && deletion && !replacement) {
				return change_summary_kind::deletion;
			}
			if (!addition && !deletion && replacement) {
				return change_summary_kind::replacement;
			}
			return change_summary_kind::mixed;
		}

		auto left_present(const file_comparison_key& aKey) noexcept -> bool
		{
			return aKey.LeftBlob.has_value();
		}
		auto right_present(const file_comparison_key& aKey) noexcept -> bool
		{
			return aKey.RightBlob.has_value();
		}
		auto left_present(const material_comparison_key& aKey) noexcept -> bool
		{
			return aKey.LeftPresent;
		}
		auto right_present(const material_comparison_key& aKey) noexcept -> bool
		{
			return aKey.RightPresent;
		}

		/// The record types of one comparison kind.
		struct pull_request_comparison
		{
			using input     = diff_compute_input;
			using alignment = file_alignment;
			using computed  = computed_file_diff;
			using outcome   = diff_compute_outcome;
		};
		struct material_comparison
		{
			using input     = material_diff_input;
			using alignment = material_alignment;
			using computed  = computed_material_diff;
			using outcome   = material_diff_outcome;
		};

		auto validate_result(const computed_file_diff& aDiff) -> diff_compute_validation_result
		{
			return validate_computed_file_diff(aDiff);
		}
		auto validate_result(const computed_material_diff& aDiff) -> diff_compute_validation_result
		{
			return validate_computed_material_diff(aDiff);
		}

		template <typename TComparison>
		[[nodiscard]] auto nontext_result(const typename TComparison::input& aInput,
										  const diff_content_state           aContent,
										  const diff_reason_code             aReason)
			-> typename TComparison::outcome
		{
			auto alignment = typename TComparison::alignment{
				.Key        = aInput.Key,
				.Options    = aInput.Options,
				.Digest     = comparison_digest(aInput.Key),
				.Content    = aContent,
				.Diagnostic = { .Code = aReason },
			};
			if (!validate_alignment(alignment).valid()) {
				return { .Reason = diff_reason_code::invalid_alignment };
			}
			auto result = typename TComparison::computed{ .Alignment = std::move(alignment) };
			if (!validate_result(result).valid()) {
				return { .Reason = diff_reason_code::invalid_alignment };
			}
			return { .Reason = aReason, .Result = std::move(result) };
		}

		[[nodiscard]] auto refine_moved_subblocks(
			const std::vector<pending_segment>& aSegments,
			const prepared_document&            aLeft,
			const prepared_document&            aRight,
			const std::uint64_t                 aMaximumWork,
			const std::uint32_t                 aMaximumSegments) -> moved_refinement
		{
			auto budget = work_budget{ .Limit = aMaximumWork };
			auto index  = std::map<
				const normalized_line*,
				move_line_occurrences,
				normalized_line_pointer_less>{ };
			const auto workLimited = [&] {
				return moved_refinement{
					.Segments         = aSegments,
					.WorkConsumed     = budget.Consumed,
					.WorkLimitReached = true,
				};
			};
			const auto indexSide
				= [&](const std::size_t aSegmentIndex,
					  const line_span   aLines,
					  const bool        aLeftSide) -> bool {
				const auto& document = aLeftSide ? aLeft : aRight;
				for (auto line = aLines.Begin; line < aLines.End; ++line) {
					const auto& normalized = document.Normalized[line];
					if (!budget.consume(
							indexed_probe_work(index.size(), normalized.Values.size()))) {
						return false;
					}
					auto& occurrences = index[&normalized];
					auto& count       = aLeftSide ? occurrences.LeftCount
												  : occurrences.RightCount;
					auto& location    = aLeftSide ? occurrences.Left
												  : occurrences.Right;
					++count;
					if (count == 1U) {
						location = move_line_location{
							.Segment = aSegmentIndex,
							.Line    = line,
						};
					} else {
						location.reset();
					}
				}
				return true;
			};

			for (std::size_t segment = 0U; segment < aSegments.size(); ++segment) {
				const auto& candidate = aSegments[segment];
				if (
					(candidate.Kind == alignment_kind::left_only
					 && !indexSide(segment, candidate.Left, true))
					|| (candidate.Kind == alignment_kind::right_only
						&& !indexSide(segment, candidate.Right, false))) {
					return workLimited();
				}
			}

			auto anchors = std::vector<std::pair<move_line_location, move_line_location>>{ };
			anchors.reserve(index.size());
			for (const auto& [normalized, occurrences] : index) {
				static_cast<void>(normalized);
				if (!budget.consume()) {
					return workLimited();
				}
				if (
					occurrences.LeftCount == 1U && occurrences.RightCount == 1U
					&& occurrences.Left.has_value()
					&& occurrences.Right.has_value()) {
					anchors.emplace_back(*occurrences.Left, *occurrences.Right);
				}
			}
			if (anchors.empty()) {
				return {
					.Segments     = aSegments,
					.WorkConsumed = budget.Consumed,
				};
			}
			if (!budget.consume(sort_work(anchors.size()))) {
				return workLimited();
			}
			std::ranges::sort(
				anchors,
				[](const auto& aLeftAnchor, const auto& aRightAnchor) {
					return std::tie(
							   aLeftAnchor.first.Line,
							   aLeftAnchor.first.Segment,
							   aLeftAnchor.second.Line,
							   aLeftAnchor.second.Segment)
						< std::tie(
							   aRightAnchor.first.Line,
							   aRightAnchor.first.Segment,
							   aRightAnchor.second.Line,
							   aRightAnchor.second.Segment);
				});

			const auto claimInitializationWork
				= static_cast<std::uint64_t>(aLeft.Normalized.size())
				+ static_cast<std::uint64_t>(aRight.Normalized.size());
			if (!budget.consume(claimInitializationWork)) {
				return workLimited();
			}
			auto leftClaimed  = std::vector<bool>(aLeft.Normalized.size(), false);
			auto rightClaimed = std::vector<bool>(aRight.Normalized.size(), false);
			auto detected     = std::vector<detected_move>{ };
			for (const auto& [source, destination] : anchors) {
				if (leftClaimed[source.Line] || rightClaimed[destination.Line]) {
					continue;
				}
				const auto sourceLimit      = aSegments[source.Segment].Left;
				const auto destinationLimit = aSegments[destination.Segment].Right;
				auto       sourceBegin      = source.Line;
				auto       destinationBegin = destination.Line;
				while (
					sourceBegin > sourceLimit.Begin
					&& destinationBegin > destinationLimit.Begin
					&& !leftClaimed[sourceBegin - 1U]
					&& !rightClaimed[destinationBegin - 1U]) {
					if (!budget.consume(line_comparison_work(
							aLeft.Normalized[sourceBegin - 1U],
							aRight.Normalized[destinationBegin - 1U]))) {
						return workLimited();
					}
					if (aLeft.Normalized[sourceBegin - 1U]
						!= aRight.Normalized[destinationBegin - 1U]) {
						break;
					}
					--sourceBegin;
					--destinationBegin;
				}
				auto sourceEnd      = source.Line + 1U;
				auto destinationEnd = destination.Line + 1U;
				while (
					sourceEnd < sourceLimit.End
					&& destinationEnd < destinationLimit.End
					&& !leftClaimed[sourceEnd]
					&& !rightClaimed[destinationEnd]) {
					if (!budget.consume(line_comparison_work(
							aLeft.Normalized[sourceEnd],
							aRight.Normalized[destinationEnd]))) {
						return workLimited();
					}
					if (aLeft.Normalized[sourceEnd]
						!= aRight.Normalized[destinationEnd]) {
						break;
					}
					++sourceEnd;
					++destinationEnd;
				}
				const auto retainedLines = static_cast<std::uint64_t>(
											   sourceEnd - sourceBegin)
					+ static_cast<std::uint64_t>(
											   destinationEnd - destinationBegin);
				if (!budget.consume(retainedLines)) {
					return workLimited();
				}
				std::fill(
					leftClaimed.begin() + sourceBegin,
					leftClaimed.begin() + sourceEnd,
					true);
				std::fill(
					rightClaimed.begin() + destinationBegin,
					rightClaimed.begin() + destinationEnd,
					true);
				detected.push_back({
					.SourceOriginal      = source.Segment,
					.SourceLines         = { sourceBegin, sourceEnd },
					.DestinationOriginal = destination.Segment,
					.DestinationLines    = { destinationBegin, destinationEnd },
				});
			}

			struct move_piece
			{
				line_span   Lines;
				std::size_t Move   = 0U;
				bool        Source = false;
			};
			const auto refinementWork
				= static_cast<std::uint64_t>(aSegments.size())
				+ static_cast<std::uint64_t>(detected.size()) * 6U;
			if (!budget.consume(refinementWork)) {
				return workLimited();
			}
			auto pieces = std::vector<std::vector<move_piece>>(aSegments.size());
			for (std::size_t move = 0U; move < detected.size(); ++move) {
				pieces[detected[move].SourceOriginal].push_back({
					.Lines  = detected[move].SourceLines,
					.Move   = move,
					.Source = true,
				});
				pieces[detected[move].DestinationOriginal].push_back({
					.Lines = detected[move].DestinationLines,
					.Move  = move,
				});
			}
			auto refined = std::vector<pending_segment>{ };
			refined.reserve(aSegments.size() + detected.size() * 4U);
			for (std::size_t segment = 0U; segment < aSegments.size(); ++segment) {
				const auto& original = aSegments[segment];
				if (pieces[segment].empty()) {
					refined.push_back(original);
					continue;
				}
				if (!budget.consume(sort_work(pieces[segment].size()))) {
					return workLimited();
				}
				std::ranges::sort(
					pieces[segment],
					{ },
					[](const move_piece& aPiece) { return aPiece.Lines.Begin; });
				const auto full   = original.Kind == alignment_kind::left_only
					? original.Left
					: original.Right;
				auto       cursor = full.Begin;
				const auto append = [&](const line_span aLines) {
					refined.push_back({
						.Kind  = original.Kind,
						.Left  = original.Kind == alignment_kind::left_only
							? aLines
							: original.Left,
						.Right = original.Kind == alignment_kind::right_only
							? aLines
							: original.Right,
					});
				};
				for (const auto& piece : pieces[segment]) {
					if (cursor < piece.Lines.Begin) {
						append({ cursor, piece.Lines.Begin });
					}
					const auto refinedIndex = refined.size();
					append(piece.Lines);
					if (piece.Source) {
						detected[piece.Move].SourceSegment = refinedIndex;
					} else {
						detected[piece.Move].DestinationSegment = refinedIndex;
					}
					cursor = piece.Lines.End;
				}
				if (cursor < full.End) {
					append({ cursor, full.End });
				}
			}
			if (refined.size() > static_cast<std::size_t>(aMaximumSegments)) {
				return {
					.Segments            = aSegments,
					.WorkConsumed        = budget.Consumed,
					.SegmentLimitReached = true,
				};
			}
			if (!budget.consume(sort_work(detected.size()))) {
				return workLimited();
			}
			std::ranges::sort(
				detected,
				{ },
				[](const detected_move& aMove) {
					return std::pair{ aMove.SourceSegment,
									  aMove.DestinationSegment };
				});
			auto moves = std::vector<std::pair<std::size_t, std::size_t>>{ };
			moves.reserve(detected.size());
			for (const auto& move : detected) {
				moves.emplace_back(move.SourceSegment, move.DestinationSegment);
			}
			return {
				.Segments         = std::move(refined),
				.Moves            = std::move(moves),
				.WorkConsumed     = budget.Consumed,
				.WorkLimitReached = false,
			};
		}

		[[nodiscard]] auto intraline_ranges(const decoded_text_document& aLeft,
											const decoded_text_document& aRight,
											const std::uint32_t          aLeftLine,
											const std::uint32_t          aRightLine,
											matrix_budget&               aBudget)
			-> std::optional<std::vector<std::pair<byte_span, byte_span>>>
		{
			auto leftScalars  = std::vector<unicode_scalar>{ };
			auto rightScalars = std::vector<unicode_scalar>{ };
			static_cast<void>(valid_utf8(line_bytes(aLeft, aLeftLine), &leftScalars));
			static_cast<void>(
				valid_utf8(line_bytes(aRight, aRightLine), &rightScalars));
			const auto matches = lcs_matches(
				leftScalars,
				rightScalars,
				[](const unicode_scalar& aLhs, const unicode_scalar& aRhs) { return aLhs.Value == aRhs.Value; },
				aBudget);
			if (!matches.has_value()) {
				return std::nullopt;
			}
			const auto leftSize   = static_cast<std::uint64_t>(line_bytes(aLeft, aLeftLine).size());
			const auto rightSize  = static_cast<std::uint64_t>(line_bytes(aRight, aRightLine).size());
			auto       result     = std::vector<std::pair<byte_span, byte_span>>{ };
			auto       priorLeft  = std::uint64_t{ 0U };
			auto       priorRight = std::uint64_t{ 0U };
			for (const auto& [leftIndex, rightIndex] : *matches) {
				const auto leftBegin  = leftScalars[leftIndex].Begin;
				const auto rightBegin = rightScalars[rightIndex].Begin;
				if (priorLeft != leftBegin || priorRight != rightBegin) {
					result.emplace_back(byte_span{ priorLeft, leftBegin },
										byte_span{ priorRight, rightBegin });
				}
				priorLeft  = leftScalars[leftIndex].End;
				priorRight = rightScalars[rightIndex].End;
			}
			if (priorLeft != leftSize || priorRight != rightSize) {
				result.emplace_back(byte_span{ priorLeft, leftSize },
									byte_span{ priorRight, rightSize });
			}
			return result;
		}
	} // namespace

	namespace {
		/// The computed-diff rules, shared by both comparison kinds.
		template <typename TComputed>
		auto validate_computed_records(const TComputed& aDiff)
			-> diff_compute_validation_result
		{
			if (!validate_alignment(aDiff.Alignment).valid()) {
				return compute_failure(
					diff_compute_validation_error::invalid_alignment);
			}
			if (aDiff.Alignment.Content != diff_content_state::text) {
				return !aDiff.LeftDocument.has_value() && !aDiff.RightDocument.has_value() && aDiff.Intraline.empty() && aDiff.MatrixCellsConsumed == 0U && aDiff.MoveDetectionWorkConsumed == 0U && !aDiff.MoveDetectionWorkLimitReached && !aDiff.MoveDetectionSegmentLimitReached && !aDiff.AlignmentApproximate
					? diff_compute_validation_result{ }
					: compute_failure(
						  diff_compute_validation_error::records_for_non_text);
			}

			if (aDiff.LeftDocument.has_value() != left_present(aDiff.Alignment.Key) || aDiff.RightDocument.has_value() != right_present(aDiff.Alignment.Key)) {
				return compute_failure(
					diff_compute_validation_error::document_presence_mismatch);
			}
			if ((aDiff.LeftDocument.has_value() && aDiff.LeftDocument->Lines.size() != aDiff.Alignment.LeftLineCount) || (aDiff.RightDocument.has_value() && aDiff.RightDocument->Lines.size() != aDiff.Alignment.RightLineCount)) {
				return compute_failure(
					diff_compute_validation_error::document_line_count_mismatch);
			}
			if ((aDiff.LeftDocument.has_value() && !valid_document(*aDiff.LeftDocument)) || (aDiff.RightDocument.has_value() && !valid_document(*aDiff.RightDocument))) {
				return compute_failure(diff_compute_validation_error::invalid_document);
			}
			if (
				(aDiff.Alignment.Options.Moves == move_detection_policy::disabled
				 && (aDiff.MoveDetectionWorkConsumed != 0U
					 || aDiff.MoveDetectionWorkLimitReached
					 || aDiff.MoveDetectionSegmentLimitReached))
				|| ((aDiff.MoveDetectionWorkLimitReached
					 || aDiff.MoveDetectionSegmentLimitReached)
					&& !aDiff.Alignment.Moves.empty())
				|| (aDiff.MoveDetectionWorkLimitReached
					&& aDiff.MoveDetectionSegmentLimitReached)) {
				return compute_failure(
					diff_compute_validation_error::invalid_move_statistics);
			}

			for (std::size_t index = 0U; index < aDiff.Alignment.Moves.size(); ++index) {
				const auto& move        = aDiff.Alignment.Moves[index];
				const auto  source      = std::ranges::find(aDiff.Alignment.Segments,
															move.SourceDeletion,
															&alignment_segment::Id);
				const auto  destination = std::ranges::find(aDiff.Alignment.Segments,
															move.DestinationInsertion,
															&alignment_segment::Id);
				if (!aDiff.LeftDocument.has_value() || !aDiff.RightDocument.has_value() || source == aDiff.Alignment.Segments.end() || destination == aDiff.Alignment.Segments.end() || source->Left.extent() != destination->Right.extent()) {
					return compute_failure(
						diff_compute_validation_error::invalid_move_content, index);
				}
				for (std::uint64_t offset = 0U; offset < source->Left.extent(); ++offset) {
					const auto leftLine = static_cast<std::uint32_t>(
						static_cast<std::uint64_t>(source->Left.Begin) + offset);
					const auto rightLine = static_cast<std::uint32_t>(
						static_cast<std::uint64_t>(destination->Right.Begin) + offset);
					if (!normalized_lines_equal(*aDiff.LeftDocument,
												leftLine,
												*aDiff.RightDocument,
												rightLine,
												aDiff.Alignment.Options)) {
						return compute_failure(
							diff_compute_validation_error::invalid_move_content, index);
					}
				}
			}

			auto pairIndex      = std::size_t{ 0U };
			auto priorPairIndex = diff_compute_validation_result::no_record;
			auto priorLeftEnd   = std::uint64_t{ 0U };
			auto priorRightEnd  = std::uint64_t{ 0U };
			for (std::size_t index = 0U; index < aDiff.Intraline.size(); ++index) {
				const auto& edit = aDiff.Intraline[index];
				while (pairIndex < aDiff.Alignment.Pairs.size() && aDiff.Alignment.Pairs[pairIndex].Id != edit.Pair) {
					++pairIndex;
				}
				if (pairIndex == aDiff.Alignment.Pairs.size()) {
					return compute_failure(
						diff_compute_validation_error::unknown_intraline_pair, index);
				}
				const auto& pair = aDiff.Alignment.Pairs[pairIndex];
				if (pair.Kind != line_pair_kind::similar) {
					return compute_failure(
						diff_compute_validation_error::intraline_for_exact_pair, index);
				}
				const auto& leftLine  = aDiff.LeftDocument->Lines[static_cast<std::size_t>(pair.LeftLine)];
				const auto& rightLine = aDiff.RightDocument->Lines[static_cast<std::size_t>(pair.RightLine)];
				if (edit.Left.Begin > edit.Left.End || edit.Right.Begin > edit.Right.End || (edit.Left.empty() && edit.Right.empty()) || !byte_boundary(*aDiff.LeftDocument, leftLine, edit.Left.Begin) || !byte_boundary(*aDiff.LeftDocument, leftLine, edit.Left.End) || !byte_boundary(*aDiff.RightDocument, rightLine, edit.Right.Begin) || !byte_boundary(*aDiff.RightDocument, rightLine, edit.Right.End)) {
					return compute_failure(
						diff_compute_validation_error::invalid_intraline_span, index);
				}
				if (priorPairIndex != pairIndex) {
					priorPairIndex = pairIndex;
					priorLeftEnd   = 0U;
					priorRightEnd  = 0U;
				}
				if (edit.Left.Begin < priorLeftEnd || edit.Right.Begin < priorRightEnd) {
					return compute_failure(
						diff_compute_validation_error::nonmonotonic_intraline, index);
				}
				priorLeftEnd  = edit.Left.End;
				priorRightEnd = edit.Right.End;
			}

			for (std::size_t index = 0U; index < aDiff.Alignment.Segments.size(); ++index) {
				const auto& segment = aDiff.Alignment.Segments[index];
				if (segment.Kind != alignment_kind::equal) {
					continue;
				}
				for (std::uint64_t offset = 0U; offset < segment.Left.extent(); ++offset) {
					const auto leftLine = static_cast<std::uint32_t>(
						static_cast<std::uint64_t>(segment.Left.Begin) + offset);
					const auto rightLine = static_cast<std::uint32_t>(
						static_cast<std::uint64_t>(segment.Right.Begin) + offset);
					if (!normalized_lines_equal(*aDiff.LeftDocument,
												leftLine,
												*aDiff.RightDocument,
												rightLine,
												aDiff.Alignment.Options)) {
						return compute_failure(
							diff_compute_validation_error::invalid_exact_content,
							index);
					}
				}
			}
			for (std::size_t index = 0U; index < aDiff.Alignment.Pairs.size(); ++index) {
				const auto& pair = aDiff.Alignment.Pairs[index];
				if (pair.Kind == line_pair_kind::exact && !normalized_lines_equal(*aDiff.LeftDocument, pair.LeftLine, *aDiff.RightDocument, pair.RightLine, aDiff.Alignment.Options)) {
					return compute_failure(
						diff_compute_validation_error::invalid_exact_content, index);
				}
			}
			return { };
		}
	}

	auto validate_computed_file_diff(const computed_file_diff& aDiff)
		-> diff_compute_validation_result
	{
		return validate_computed_records(aDiff);
	}

	auto validate_computed_material_diff(const computed_material_diff& aDiff)
		-> diff_compute_validation_result
	{
		return validate_computed_records(aDiff);
	}

	namespace {
		/// The engine, shared by both comparison kinds.
		template <typename TComparison>
		auto compute_diff(const typename TComparison::input& aInput) -> typename TComparison::outcome
		{
			const auto digest = options_digest(aInput.Options);
			if (!valid(aInput.Key) || !digest.has_value() || aInput.Key.Options != *digest || (!left_present(aInput.Key) && aInput.LeftBytes.has_value()) || (!right_present(aInput.Key) && aInput.RightBytes.has_value())) {
				return { .Reason = diff_reason_code::invalid_alignment };
			}
			if (left_present(aInput.Key) && !aInput.LeftBytes.has_value()) {
				return nontext_result<TComparison>(aInput,
												   diff_content_state::unavailable,
												   diff_reason_code::missing_left_blob);
			}
			if (right_present(aInput.Key) && !aInput.RightBytes.has_value()) {
				return nontext_result<TComparison>(aInput,
												   diff_content_state::unavailable,
												   diff_reason_code::missing_right_blob);
			}
			const auto leftBytes  = aInput.LeftBytes.value_or(std::span<const std::uint8_t>{ });
			const auto rightBytes = aInput.RightBytes.value_or(std::span<const std::uint8_t>{ });
			if (static_cast<std::uint64_t>(leftBytes.size()) > aInput.Limits.MaximumBytesPerSide || static_cast<std::uint64_t>(rightBytes.size()) > aInput.Limits.MaximumBytesPerSide) {
				return nontext_result<TComparison>(aInput,
												   diff_content_state::unavailable,
												   diff_reason_code::materialization_limit);
			}
			if (std::ranges::find(leftBytes, std::uint8_t{ 0U }) != leftBytes.end() || std::ranges::find(rightBytes, std::uint8_t{ 0U }) != rightBytes.end()) {
				return nontext_result<TComparison>(
					aInput, diff_content_state::binary, diff_reason_code::binary_file);
			}
			auto left = prepare_document(
				leftBytes, aInput.Options, aInput.Limits.MaximumLinesPerSide);
			auto right = prepare_document(
				rightBytes, aInput.Options, aInput.Limits.MaximumLinesPerSide);
			if (!left.has_value() || !right.has_value()) {
				const auto decodeFailed = !valid_utf8(leftBytes) || !valid_utf8(rightBytes);
				return nontext_result<TComparison>(aInput,
												   diff_content_state::unavailable,
												   decodeFailed
													   ? diff_reason_code::decode_failure
													   : diff_reason_code::materialization_limit);
			}

			auto budget      = matrix_budget{ .Limit = aInput.Limits.MaximumMatrixCells };
			auto indexBudget = work_budget{ .Limit = aInput.Limits.MaximumPrimaryIndexWork };
			auto approximate = false;
			auto pending     = primary_segments(*left, *right, aInput.Options, budget, indexBudget, approximate);
			if (!pending.has_value() || pending->size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) || pending->size() > static_cast<std::size_t>(aInput.Limits.MaximumSegments)) {
				return nontext_result<TComparison>(aInput,
												   diff_content_state::unavailable,
												   diff_reason_code::compute_limit);
			}
			auto moved = moved_refinement{ .Segments = *pending };
			if (aInput.Options.Moves == move_detection_policy::deterministic_exact) {
				moved = refine_moved_subblocks(
					*pending,
					*left,
					*right,
					aInput.Limits.MaximumMoveDetectionWork,
					aInput.Limits.MaximumSegments);
			}
			pending = std::move(moved.Segments);

			auto alignment = typename TComparison::alignment{
				.Key            = aInput.Key,
				.Options        = aInput.Options,
				.Digest         = comparison_digest(aInput.Key),
				.LeftLineCount  = static_cast<std::uint32_t>(left->Document.Lines.size()),
				.RightLineCount = static_cast<std::uint32_t>(right->Document.Lines.size()),
			};
			auto segmentGroups = std::vector<std::optional<change_group_id>>(pending->size());
			auto groupOrdinal  = std::uint32_t{ 0U };
			for (std::size_t index = 0U; index < pending->size();) {
				if ((*pending)[index].Kind == alignment_kind::equal) {
					++index;
					continue;
				}
				const auto begin = index;
				while (index < pending->size() && (*pending)[index].Kind != alignment_kind::equal) {
					++index;
				}
				const auto  end           = index;
				const auto& first         = (*pending)[begin];
				const auto& last          = (*pending)[end - 1U];
				const auto  leftSpan      = line_span{ first.Left.Begin, last.Left.End };
				const auto  rightSpan     = line_span{ first.Right.Begin, last.Right.End };
				const auto  id            = group_identity(alignment.Digest,
														   leftSpan,
														   rightSpan,
														   groupOrdinal++);
				auto        added         = std::uint64_t{ 0U };
				auto        deleted       = std::uint64_t{ 0U };
				auto        replacedLeft  = std::uint64_t{ 0U };
				auto        replacedRight = std::uint64_t{ 0U };
				for (auto member = begin; member < end; ++member) {
					const auto& segment = (*pending)[member];
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
				alignment.Groups.push_back(change_group{
					.Id      = id,
					.Members = {
						static_cast<std::uint32_t>(begin),
						static_cast<std::uint32_t>(end),
					},
					.Left               = leftSpan,
					.Right              = rightSpan,
					.First              = { .Left = first.Left.Begin, .Right = first.Right.Begin },
					.Last               = { .Left = last.Left.End, .Right = last.Right.End },
					.Kind               = summary_kind(added, deleted, replacedLeft, replacedRight),
					.AddedLines         = added,
					.DeletedLines       = deleted,
					.ReplacedLeftLines  = replacedLeft,
					.ReplacedRightLines = replacedRight,
				});
				for (auto member = begin; member < end; ++member) {
					segmentGroups[static_cast<std::size_t>(member)] = id;
				}
			}

			alignment.Segments.reserve(pending->size());
			for (std::size_t index = 0U; index < pending->size(); ++index) {
				const auto& source = (*pending)[index];
				alignment.Segments.push_back(alignment_segment{
					.Id    = segment_identity(alignment.Digest,
											  source.Kind,
											  source.Left,
											  source.Right,
											  static_cast<std::uint32_t>(index)),
					.Kind  = source.Kind,
					.Left  = source.Left,
					.Right = source.Right,
					.Group = segmentGroups[index],
				});
			}

			auto intraline = std::vector<intraline_edit>{ };
			for (std::size_t index = 0U; index < pending->size(); ++index) {
				const auto& source = (*pending)[index];
				if (!source.PairKind.has_value()) {
					continue;
				}
				const auto ordinal = static_cast<std::uint32_t>(alignment.Pairs.size());
				const auto pair    = line_pair{
					.Id        = pair_identity(alignment.Digest,
											   alignment.Segments[index].Id,
											   source.Left.Begin,
											   source.Right.Begin,
											   *source.PairKind,
											   ordinal),
					.Segment   = alignment.Segments[index].Id,
					.LeftLine  = source.Left.Begin,
					.RightLine = source.Right.Begin,
					.Kind      = *source.PairKind,
				};
				alignment.Pairs.push_back(pair);
				if (pair.Kind == line_pair_kind::similar) {
					const auto ranges = intraline_ranges(left->Document,
														 right->Document,
														 pair.LeftLine,
														 pair.RightLine,
														 budget);
					if (!ranges.has_value()) {
						// [[BPR-0719]]: the pair stays similar without intraline
						// edits instead of refusing the file.
						approximate = true;
						continue;
					}
					for (const auto& [leftRange, rightRange] : *ranges) {
						intraline.push_back(intraline_edit{
							.Pair  = pair.Id,
							.Left  = leftRange,
							.Right = rightRange,
						});
					}
				}
			}

			for (const auto& [source, destination] : moved.Moves) {
				const auto ordinal = static_cast<std::uint32_t>(alignment.Moves.size());
				alignment.Moves.push_back(move_relation{
					.Id = move_identity(
						alignment.Digest,
						alignment.Segments[source].Id,
						alignment.Segments[destination].Id,
						ordinal),
					.SourceDeletion       = alignment.Segments[source].Id,
					.DestinationInsertion = alignment.Segments[destination].Id,
				});
			}
			if (!validate_alignment(alignment).valid()) {
				return { .Reason = diff_reason_code::invalid_alignment };
			}
			auto result = typename TComparison::computed{
				.Alignment                        = std::move(alignment),
				.LeftDocument                     = left_present(aInput.Key)
					? std::optional<decoded_text_document>{ std::move(
						  left->Document) }
					: std::nullopt,
				.RightDocument                    = right_present(aInput.Key)
					? std::optional<decoded_text_document>{ std::move(
						  right->Document) }
					: std::nullopt,
				.Intraline                        = std::move(intraline),
				.MatrixCellsConsumed              = budget.Consumed,
				.MoveDetectionWorkConsumed        = moved.WorkConsumed,
				.MoveDetectionWorkLimitReached    = moved.WorkLimitReached,
				.MoveDetectionSegmentLimitReached = moved.SegmentLimitReached,
				.PrimaryIndexWorkConsumed         = indexBudget.Consumed,
				.AlignmentApproximate             = approximate,
			};
			if (!validate_result(result).valid()) {
				return { .Reason = diff_reason_code::invalid_alignment };
			}
			return { .Reason = diff_reason_code::none, .Result = std::move(result) };
		}
	}

	auto compute_file_diff(const diff_compute_input& aInput) -> diff_compute_outcome
	{
		return compute_diff<pull_request_comparison>(aInput);
	}

	auto compute_material_diff(const material_diff_input& aInput) -> material_diff_outcome
	{
		return compute_diff<material_comparison>(aInput);
	}

} // namespace bpr::core
