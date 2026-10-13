#include "bpr/core/diff_compute.hpp"

#include "diff_step_meter.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::core {
	namespace {
		using detail::diff_step_meter;

		/// [[BPR-1366]] Table cells filled, or elements zeroed, between two
		/// step increments: the most a matrix loop runs without counting. A
		/// table no larger is one increment; a larger one is counted every
		/// this many cells, a long row in pieces. Pairing fills a great many
		/// small tables, which are counted in bulk by their caller.
		inline constexpr std::size_t table_tick_cells  = 64U * 1024U;
		inline constexpr std::size_t small_table_cells = table_tick_cells;

		/// [[BPR-1366]] Bytes scanned between two step increments.
		inline constexpr std::size_t scan_chunk_bytes = 64U * 1024U;

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

		/// [[BPR-1366]] Where the scan piece that starts at aBegin ends: at
		/// least scan_chunk_bytes on, before the next ASCII byte. An ASCII byte
		/// always starts a scalar, so the pieces of valid UTF-8 are valid and a
		/// concatenation of valid pieces is valid.
		[[nodiscard]] auto utf8_piece_end(const std::span<const std::uint8_t> aBytes,
										  const std::size_t                   aBegin) noexcept -> std::size_t
		{
			auto end = aBegin + std::min(scan_chunk_bytes, aBytes.size() - aBegin);
			while (end < aBytes.size() && aBytes[end] >= 0x80U) {
				++end;
			}
			return end;
		}

		/// `valid_utf8(aBytes)`, counted in pieces when a meter is counting.
		template <typename TMeter>
		[[nodiscard]] auto metered_valid_utf8(const std::span<const std::uint8_t> aBytes, TMeter& aMeter) -> bool
		{
			if constexpr (std::is_same_v<TMeter, detail::unmetered>) {
				return valid_utf8(aBytes);
			} else {
				auto position = std::size_t{ 0U };
				while (position < aBytes.size()) {
					const auto end = utf8_piece_end(aBytes, position);
					if (!valid_utf8(aBytes.subspan(position, end - position))) {
						return false;
					}
					aMeter.tick(end - position);
					position = end;
				}
				return true;
			}
		}

		/// Whether aBytes holds a zero byte, counted in pieces when a meter is
		/// counting.
		template <typename TMeter>
		[[nodiscard]] auto metered_has_zero(const std::span<const std::uint8_t> aBytes, TMeter& aMeter) -> bool
		{
			if constexpr (std::is_same_v<TMeter, detail::unmetered>) {
				return std::ranges::find(aBytes, std::uint8_t{ 0U }) != aBytes.end();
			} else {
				auto position = std::size_t{ 0U };
				while (position < aBytes.size()) {
					const auto piece = aBytes.subspan(position, std::min(scan_chunk_bytes, aBytes.size() - position));
					if (std::ranges::find(piece, std::uint8_t{ 0U }) != piece.end()) {
						return true;
					}
					aMeter.tick(piece.size() / 16U);
					position += piece.size();
				}
				return false;
			}
		}

		template <typename TMeter>
		[[nodiscard]] auto valid_document(const decoded_text_document& aDocument, TMeter& aMeter) -> bool
		{
			if (!metered_valid_utf8(aDocument.Bytes, aMeter) || metered_has_zero(aDocument.Bytes, aMeter)) {
				return false;
			}
			if (aDocument.Lines.empty()) {
				return aDocument.Bytes.empty() && !aDocument.HasFinalNewline;
			}

			auto cursor = std::uint64_t{ 0U };
			for (std::size_t index = 0U; index < aDocument.Lines.size(); ++index) {
				aMeter.tick(4U);
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

		template <typename Value, typename TMeter>
		[[nodiscard]] auto zeroed_table(const std::size_t aCount, TMeter& aMeter) -> std::vector<Value>
		{
			if (aCount <= small_table_cells) {
				aMeter.tick(aCount / 16U);
				return std::vector<Value>(aCount, Value{ 0U });
			}
			auto table = std::vector<Value>{ };
			table.reserve(aCount);
			while (table.size() < aCount) {
				const auto piece = std::min(aCount - table.size(), table_tick_cells * 4U);
				table.resize(table.size() + piece);
				aMeter.tick(piece / 16U);
			}
			return table;
		}

		/// [[BPR-1366]] TMeter is the engine's meter, or detail::unmetered for
		/// a small table whose caller counts it.
		template <typename Left, typename Right, typename Equal, typename TMeter>
		[[nodiscard]] auto lcs_matches(const Left&    aLeft,
									   const Right&   aRight,
									   Equal          aEqual,
									   matrix_budget& aBudget,
									   TMeter&        aMeter)
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
			auto       table = zeroed_table<std::uint32_t>(rows * columns, aMeter);
			const auto at    = [&table,
								columns](const std::size_t aRow,
										 const std::size_t aColumn) -> std::uint32_t& { return table[aRow * columns + aColumn]; };
			// [[BPR-1366]] Each row is filled from its right end down to
			// aEnd. A small table is one step increment; a large one is
			// counted every table_tick_cells cells, a long row in pieces.
			const auto fill = [&](const std::size_t aRow, const std::size_t aBegin, const std::size_t aEnd) {
				for (auto column = aBegin; column != aEnd; --column) {
					if (aEqual(aLeft[aRow - 1U], aRight[column - 1U])) {
						at(aRow - 1U, column - 1U) = static_cast<std::uint32_t>(1U + at(aRow, column));
					} else {
						at(aRow - 1U, column - 1U) = std::max(at(aRow, column - 1U), at(aRow - 1U, column));
					}
				}
			};
			if (rows * columns <= small_table_cells) {
				for (auto row = aLeft.size(); row != 0U; --row) {
					fill(row, aRight.size(), 0U);
				}
				aMeter.tick(rows * columns);
			} else if (aRight.size() <= table_tick_cells) {
				auto uncounted = std::size_t{ 0U };
				for (auto row = aLeft.size(); row != 0U; --row) {
					fill(row, aRight.size(), 0U);
					uncounted += aRight.size();
					if (uncounted >= table_tick_cells) {
						aMeter.tick(uncounted);
						uncounted = 0U;
					}
				}
				aMeter.tick(uncounted + aLeft.size() + aRight.size());
			} else {
				for (auto row = aLeft.size(); row != 0U; --row) {
					for (auto column = aRight.size(); column != 0U;) {
						const auto pieceEnd = column > table_tick_cells ? column - table_tick_cells : std::size_t{ 0U };
						fill(row, column, pieceEnd);
						aMeter.tick(column - pieceEnd);
						column = pieceEnd;
					}
				}
				aMeter.tick(aLeft.size() + aRight.size());
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

		/// [[BPR-1366]] Pairing compares a great many short lines: a small
		/// table is added to aUncounted for the caller to count in bulk, and
		/// only a large one counts its own steps.
		[[nodiscard]] auto similarity(const normalized_line& aLeft,
									  const normalized_line& aRight,
									  matrix_budget&         aBudget,
									  diff_step_meter&       aMeter,
									  std::uint64_t&         aUncounted)
			-> std::optional<std::uint16_t>
		{
			if (aLeft == aRight) {
				return maximum_similarity_threshold_permille;
			}
			if (aLeft.Values.empty() || aRight.Values.empty()) {
				return std::uint16_t{ 0U };
			}
			const auto equalValues = [](const std::uint32_t aLhs, const std::uint32_t aRhs) { return aLhs == aRhs; };
			const auto cells       = (aLeft.Values.size() + 1U) * (aRight.Values.size() + 1U);
			auto       uncounted   = detail::unmetered{ };
			const auto matches     = cells <= small_table_cells
				? lcs_matches(aLeft.Values, aRight.Values, equalValues, aBudget, uncounted)
				: lcs_matches(aLeft.Values, aRight.Values, equalValues, aBudget, aMeter);
			if (cells <= small_table_cells) {
				aUncounted += cells;
			}
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
										   matrix_budget&           aBudget,
										   diff_step_meter&         aMeter)
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
			auto scores              = zeroed_table<std::uint16_t>(cells, aMeter);
			auto table               = zeroed_table<std::uint64_t>(rows * columns, aMeter);
			auto uncountedSimilarity = std::uint64_t{ 0U };
			for (std::size_t leftIndex = 0U; leftIndex < leftCount; ++leftIndex) {
				for (std::size_t rightIndex = 0U; rightIndex < rightCount;
					 ++rightIndex) {
					const auto score = similarity(
						aLeft.Normalized[static_cast<std::size_t>(aLeftSpan.Begin) + leftIndex],
						aRight.Normalized[static_cast<std::size_t>(aRightSpan.Begin) + rightIndex],
						aBudget,
						aMeter,
						uncountedSimilarity);
					if (!score.has_value()) {
						return std::nullopt;
					}
					scores[leftIndex * rightCount + rightIndex] = *score;
					if (uncountedSimilarity >= small_table_cells) {
						aMeter.tick(uncountedSimilarity);
						uncountedSimilarity = 0U;
					}
				}
			}
			aMeter.tick(uncountedSimilarity);
			const auto at        = [&table,
									columns](const std::size_t aRow,
											 const std::size_t aColumn) -> std::uint64_t& { return table[aRow * columns + aColumn]; };
			auto       uncounted = std::size_t{ 0U };
			for (auto row = leftCount; row != 0U; --row) {
				auto column = rightCount;
				while (column != 0U) {
					const auto pieceEnd = column > table_tick_cells ? column - table_tick_cells : std::size_t{ 0U };
					uncounted += column - pieceEnd;
					for (; column != pieceEnd; --column) {
						const auto score = scores[(row - 1U) * rightCount + column - 1U];
						auto       best  = std::max(at(row, column - 1U), at(row - 1U, column));
						if (score >= aOptions.SimilarityThresholdPermille) {
							best = std::max(
								best, static_cast<std::uint64_t>(score) + at(row, column));
						}
						at(row - 1U, column - 1U) = best;
					}
					if (uncounted >= table_tick_cells) {
						aMeter.tick(uncounted);
						uncounted = 0U;
					}
				}
			}
			aMeter.tick(uncounted + leftCount + rightCount);
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
		[[nodiscard]] auto ordered_unique_matches(const line_view  aLeft,
												  const line_view  aRight,
												  work_budget&     aBudget,
												  diff_step_meter& aMeter)
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
				// [[BPR-1366]] Each probe counts the index work it charged.
				auto       counted = aBudget.Consumed;
				const auto count   = [&aBudget, &aMeter, &counted] {
					aMeter.tick(aBudget.Consumed - counted);
					counted = aBudget.Consumed;
				};
				for (std::size_t position = 0U; position < aRight.size(); ++position) {
					if (!aBudget.consume()) {
						return std::nullopt;
					}
					const auto [entry, inserted] = index.try_emplace(&aRight[position], occurrence{ .Position = position });
					if (!inserted) {
						entry->second.Repeated = true;
					}
					count();
				}
				match_list matches;
				for (std::size_t position = 0U; position < aLeft.size(); ++position) {
					if (!aBudget.consume()) {
						return std::nullopt;
					}
					const auto entry = index.find(&aLeft[position]);
					count();
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
		[[nodiscard]] auto patience_anchors(const line_view  aLeft,
											const line_view  aRight,
											work_budget&     aBudget,
											diff_step_meter& aMeter)
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
				// [[BPR-1366]] Each probe counts the index work it charged.
				auto       counted = aBudget.Consumed;
				const auto count   = [&aBudget, &aMeter, &counted] {
					aMeter.tick(aBudget.Consumed - counted);
					counted = aBudget.Consumed;
				};
				for (std::size_t position = 0U; position < aRight.size(); ++position) {
					if (!aBudget.consume()) {
						return std::nullopt;
					}
					const auto [entry, inserted] = index.try_emplace(&aRight[position], occurrence{ .Right = position });
					if (!inserted) {
						entry->second.RightRepeated = true;
					}
					count();
				}
				match_list candidates;
				for (std::size_t position = 0U; position < aLeft.size(); ++position) {
					if (!aBudget.consume()) {
						return std::nullopt;
					}
					const auto entry = index.find(&aLeft[position]);
					count();
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
				std::erase_if(candidates, [&index, &aLeft, &count](const std::pair<std::size_t, std::size_t>& aCandidate) {
					const auto repeated = index.find(&aLeft[aCandidate.first])->second.LeftRepeated;
					count();
					return repeated;
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
					count();
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
			const std::uint32_t                 aMaximumSegments,
			diff_step_meter&                    aMeter) -> moved_refinement
		{
			auto budget = work_budget{ .Limit = aMaximumWork };
			// [[BPR-1366]] The phase restarts on a stop; its steps are its work.
			auto       counted   = std::uint64_t{ 0U };
			const auto countWork = [&budget, &aMeter, &counted] {
				aMeter.tick(budget.Consumed - counted);
				counted = budget.Consumed;
			};
			auto index = std::map<
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
					countWork();
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
				countWork();
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
				countWork();
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
				countWork();
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
			countWork();
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
											matrix_budget&               aBudget,
											diff_step_meter&             aMeter)
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
				aBudget,
				aMeter);
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
		[[nodiscard]] constexpr auto line_extent(const decoded_text_line& aLine) noexcept -> std::uint64_t
		{
			return aLine.Content.End - aLine.Content.Begin;
		}

		/// [[BPR-1366]] Steps of normalizing one line: decoding and copying its
		/// scalars costs several times a matrix cell per byte.
		[[nodiscard]] constexpr auto normalize_steps(const decoded_text_line& aLine) noexcept -> std::uint64_t
		{
			return 16U * (1U + line_extent(aLine));
		}

		template <typename TAlignment, typename TMeter>
		[[nodiscard]] auto metered_validate_alignment(const TAlignment& aAlignment, TMeter& aMeter)
			-> alignment_validation_result
		{
			if constexpr (std::is_same_v<TMeter, detail::unmetered>) {
				return validate_alignment(aAlignment);
			} else {
				return detail::validate_alignment(aAlignment, aMeter);
			}
		}

		/// The computed-diff rules, shared by both comparison kinds.
		///
		/// [[BPR-1366]] The checks run in stages, and aCursor records each stage
		/// once it has passed, so a stopped validation resumes at its stage; the
		/// equal-content stage resumes at its line. The stages keep the checks'
		/// order, so a failure is reported exactly as one pass reports it.
		template <typename TComputed, typename TMeter>
		auto validate_computed_records(const TComputed& aDiff, detail::validation_cursor& aCursor, TMeter& aMeter)
			-> diff_compute_validation_result
		{
			enum validation_stage : std::uint8_t {
				alignment_stage,
				left_document_stage,
				right_document_stage,
				moves_stage,
				intraline_stage,
				equal_stage,
				pairs_stage,
				passed_stage,
			};
			if (aCursor.Stage == alignment_stage) {
				if (!metered_validate_alignment(aDiff.Alignment, aMeter).valid()) {
					return compute_failure(
						diff_compute_validation_error::invalid_alignment);
				}
				aCursor.Stage = left_document_stage;
				aMeter.commit();
			}
			if (aDiff.Alignment.Content != diff_content_state::text) {
				return !aDiff.LeftDocument.has_value() && !aDiff.RightDocument.has_value() && aDiff.Intraline.empty() && aDiff.MatrixCellsConsumed == 0U && aDiff.MoveDetectionWorkConsumed == 0U && !aDiff.MoveDetectionWorkLimitReached && !aDiff.MoveDetectionSegmentLimitReached && !aDiff.AlignmentApproximate
					? diff_compute_validation_result{ }
					: compute_failure(
						  diff_compute_validation_error::records_for_non_text);
			}

			if (aCursor.Stage == left_document_stage) {
				if (aDiff.LeftDocument.has_value() != left_present(aDiff.Alignment.Key) || aDiff.RightDocument.has_value() != right_present(aDiff.Alignment.Key)) {
					return compute_failure(
						diff_compute_validation_error::document_presence_mismatch);
				}
				if ((aDiff.LeftDocument.has_value() && aDiff.LeftDocument->Lines.size() != aDiff.Alignment.LeftLineCount) || (aDiff.RightDocument.has_value() && aDiff.RightDocument->Lines.size() != aDiff.Alignment.RightLineCount)) {
					return compute_failure(
						diff_compute_validation_error::document_line_count_mismatch);
				}
				if (aDiff.LeftDocument.has_value() && !valid_document(*aDiff.LeftDocument, aMeter)) {
					return compute_failure(diff_compute_validation_error::invalid_document);
				}
				aCursor.Stage = right_document_stage;
				aMeter.commit();
			}
			if (aCursor.Stage == right_document_stage) {
				if (aDiff.RightDocument.has_value() && !valid_document(*aDiff.RightDocument, aMeter)) {
					return compute_failure(diff_compute_validation_error::invalid_document);
				}
				aCursor.Stage = moves_stage;
				aMeter.commit();
			}
			if (aCursor.Stage == moves_stage) {
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
					aMeter.tick(2U * aDiff.Alignment.Segments.size());
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
						aMeter.tick(normalize_steps(aDiff.LeftDocument->Lines[leftLine]) + normalize_steps(aDiff.RightDocument->Lines[rightLine]));
					}
				}
				aCursor.Stage = intraline_stage;
				aMeter.commit();
			}

			if (aCursor.Stage == intraline_stage) {
				auto pairIndex      = std::size_t{ 0U };
				auto priorPairIndex = diff_compute_validation_result::no_record;
				auto priorLeftEnd   = std::uint64_t{ 0U };
				auto priorRightEnd  = std::uint64_t{ 0U };
				for (std::size_t index = 0U; index < aDiff.Intraline.size(); ++index) {
					aMeter.tick(8U);
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
				aCursor.Stage   = equal_stage;
				aCursor.Segment = 0U;
				aCursor.Offset  = 0U;
				aMeter.commit();
			}

			if (aCursor.Stage == equal_stage) {
				while (aCursor.Segment < aDiff.Alignment.Segments.size()) {
					const auto& segment = aDiff.Alignment.Segments[aCursor.Segment];
					if (segment.Kind != alignment_kind::equal || aCursor.Offset >= segment.Left.extent()) {
						++aCursor.Segment;
						aCursor.Offset = 0U;
						aMeter.tick_kept(1U);
						continue;
					}
					const auto leftLine = static_cast<std::uint32_t>(
						static_cast<std::uint64_t>(segment.Left.Begin) + aCursor.Offset);
					const auto rightLine = static_cast<std::uint32_t>(
						static_cast<std::uint64_t>(segment.Right.Begin) + aCursor.Offset);
					if (!normalized_lines_equal(*aDiff.LeftDocument,
												leftLine,
												*aDiff.RightDocument,
												rightLine,
												aDiff.Alignment.Options)) {
						return compute_failure(
							diff_compute_validation_error::invalid_exact_content,
							aCursor.Segment);
					}
					++aCursor.Offset;
					aMeter.tick_kept(normalize_steps(aDiff.LeftDocument->Lines[leftLine]) + normalize_steps(aDiff.RightDocument->Lines[rightLine]));
				}
				aCursor.Stage = pairs_stage;
				aMeter.commit();
			}
			if (aCursor.Stage == pairs_stage) {
				for (std::size_t index = 0U; index < aDiff.Alignment.Pairs.size(); ++index) {
					const auto& pair = aDiff.Alignment.Pairs[index];
					if (pair.Kind == line_pair_kind::exact && !normalized_lines_equal(*aDiff.LeftDocument, pair.LeftLine, *aDiff.RightDocument, pair.RightLine, aDiff.Alignment.Options)) {
						return compute_failure(
							diff_compute_validation_error::invalid_exact_content, index);
					}
					aMeter.tick(pair.Kind == line_pair_kind::exact
									? normalize_steps(aDiff.LeftDocument->Lines[pair.LeftLine]) + normalize_steps(aDiff.RightDocument->Lines[pair.RightLine])
									: 1U);
				}
				aCursor.Stage = passed_stage;
				aMeter.commit();
			}
			return { };
		}
	} // namespace

	void detail::diff_step_meter::check()
	{
		LargestPollDelay = std::max(LargestPollDelay, Steps - NextCheck);
		NextCheck        = advance(Steps);
		if (Stop->stop_requested(Steps)) {
			throw diff_compute_paused{ };
		}
	}

	auto validate_computed_file_diff(const computed_file_diff& aDiff)
		-> diff_compute_validation_result
	{
		auto cursor = detail::validation_cursor{ };
		auto meter  = detail::unmetered{ };
		return validate_computed_records(aDiff, cursor, meter);
	}

	auto validate_computed_material_diff(const computed_material_diff& aDiff)
		-> diff_compute_validation_result
	{
		auto cursor = detail::validation_cursor{ };
		auto meter  = detail::unmetered{ };
		return validate_computed_records(aDiff, cursor, meter);
	}

	auto detail::validate_computed_file_diff(const computed_file_diff& aDiff, validation_cursor& aCursor,
											 diff_step_meter& aMeter) -> diff_compute_validation_result
	{
		return validate_computed_records(aDiff, aCursor, aMeter);
	}

	namespace {
		/// [[BPR-1366]] The engine's steps, in order. Each names where a paused
		/// computation continues.
		enum class engine_step : std::uint8_t {
			scan_zero,
			scan_utf8,
			split,
			normalize,
			primary,
			sort_matches,
			primary_shapes,
			refine,
			moves,
			groups,
			segments,
			pairs,
			move_relations,
			validate_alignment,
			validate_result,
		};

		/// Where the region on top of the primary work list continues.
		enum class region_stage : std::uint8_t {
			strip,
			exact,
			ordered,
			anchors,
		};

		struct region
		{
			std::size_t LeftBegin  = 0U;
			std::size_t LeftEnd    = 0U;
			std::size_t RightBegin = 0U;
			std::size_t RightEnd   = 0U;
		};

		[[nodiscard]] constexpr auto phase_of(const engine_step aStep) noexcept -> diff_compute_phase
		{
			switch (aStep) {
			case engine_step::scan_zero:
			case engine_step::scan_utf8:
			case engine_step::split:
			case engine_step::normalize:
				return diff_compute_phase::prepare;
			case engine_step::primary:
			case engine_step::sort_matches:
			case engine_step::primary_shapes:
				return diff_compute_phase::primary;
			case engine_step::refine:
				return diff_compute_phase::refine;
			case engine_step::moves:
				return diff_compute_phase::moves;
			case engine_step::groups:
			case engine_step::segments:
			case engine_step::pairs:
			case engine_step::move_relations:
				return diff_compute_phase::assemble;
			case engine_step::validate_alignment:
			case engine_step::validate_result:
				return diff_compute_phase::validate;
			}
			return diff_compute_phase::validate;
		}
	} // namespace

	/// [[BPR-1366]] Everything a computation has kept at its last resume point.
	struct diff_compute_progress
	{
		// The identity a resume must match.
		std::variant<file_comparison_key, material_comparison_key> Key;
		diff_options                                               Options;
		diff_compute_limits                                        Limits;

		engine_step                                                                       Step             = engine_step::scan_zero;
		std::uint64_t                                                                     Steps            = 0U;
		std::uint64_t                                                                     WorkSteps        = 0U;
		std::uint64_t                                                                     LargestUnit      = 0U;
		std::uint64_t                                                                     LargestPollDelay = 0U;
		std::uint64_t                                                                     RetainedBytes    = 0U;
		std::uint32_t                                                                     Slices           = 0U;
		std::array<std::uint64_t, static_cast<std::size_t>(diff_compute_phase::finished)> PhaseStarts{ };

		// Prepare: both sides, their copies of the bytes, and the cursor.
		std::array<prepared_document, 2U> Sides;
		std::size_t                       Side      = 0U;
		std::size_t                       Position  = 0U;
		std::size_t                       LineStart = 0U;

		// Primary: the work list, its matches and the budgets.
		std::vector<region> Pending;
		match_list          Matches;
		region_stage        Stage = region_stage::strip;
		matrix_budget       Budget;
		work_budget         IndexBudget;
		bool                Approximate = false;

		// Refine and moves.
		std::vector<pending_segment> Primary;
		std::vector<pending_segment> Refined;
		std::size_t                  Cursor = 0U;
		moved_refinement             Moved;

		// Assemble.
		file_comparison_digest                      Digest;
		std::vector<std::optional<change_group_id>> SegmentGroups;
		std::uint32_t                               GroupOrdinal = 0U;
		std::vector<change_group>                   Groups;
		std::vector<alignment_segment>              Segments;
		std::vector<line_pair>                      Pairs;
		std::vector<move_relation>                  Moves;
		std::vector<intraline_edit>                 Intraline;

		// Validate.
		std::variant<std::monostate, computed_file_diff, computed_material_diff> Result;
		detail::validation_cursor                                                Validation;
	};

	/// The engine's access to a paused state's progress.
	struct diff_compute_progress_access
	{
		[[nodiscard]] static auto take(paused_diff_compute& aPaused) noexcept -> std::unique_ptr<diff_compute_progress>
		{
			return std::move(aPaused.Progress);
		}

		[[nodiscard]] static auto make(std::unique_ptr<diff_compute_progress> aProgress) noexcept -> paused_diff_compute
		{
			auto paused     = paused_diff_compute{ };
			paused.Progress = std::move(aProgress);
			return paused;
		}
	};

	namespace {
		template <typename Value>
		auto add_capacity(std::uint64_t& aTotal, const std::vector<Value>& aValues) noexcept -> void
		{
			aTotal += static_cast<std::uint64_t>(aValues.capacity()) * sizeof(Value);
		}

		auto add_document(std::uint64_t& aTotal, const decoded_text_document& aDocument) noexcept -> void
		{
			add_capacity(aTotal, aDocument.Bytes);
			add_capacity(aTotal, aDocument.Lines);
		}

		[[nodiscard]] auto progress_bytes(const diff_compute_progress& aProgress) noexcept -> std::uint64_t
		{
			auto total = static_cast<std::uint64_t>(sizeof(aProgress));
			for (const auto& side : aProgress.Sides) {
				add_document(total, side.Document);
				add_capacity(total, side.Normalized);
				for (const auto& line : side.Normalized) {
					add_capacity(total, line.Values);
				}
			}
			add_capacity(total, aProgress.Pending);
			add_capacity(total, aProgress.Matches);
			add_capacity(total, aProgress.Primary);
			add_capacity(total, aProgress.Refined);
			add_capacity(total, aProgress.Moved.Segments);
			add_capacity(total, aProgress.Moved.Moves);
			add_capacity(total, aProgress.SegmentGroups);
			add_capacity(total, aProgress.Groups);
			add_capacity(total, aProgress.Segments);
			add_capacity(total, aProgress.Pairs);
			add_capacity(total, aProgress.Moves);
			add_capacity(total, aProgress.Intraline);
			std::visit(
				[&total](const auto& aResult) {
					if constexpr (!std::is_same_v<std::decay_t<decltype(aResult)>, std::monostate>) {
						add_capacity(total, aResult.Alignment.Segments);
						add_capacity(total, aResult.Alignment.Pairs);
						add_capacity(total, aResult.Alignment.Groups);
						add_capacity(total, aResult.Alignment.Moves);
						add_capacity(total, aResult.Intraline);
						if (aResult.LeftDocument.has_value()) {
							add_document(total, *aResult.LeftDocument);
						}
						if (aResult.RightDocument.has_value()) {
							add_document(total, *aResult.RightDocument);
						}
					}
				},
				aProgress.Result);
			return total;
		}

		/// The bytes of one side a paused computation holds.
		[[nodiscard]] auto held_bytes(const diff_compute_progress& aProgress, const std::size_t aSide)
			-> std::span<const std::uint8_t>
		{
			const auto* document = &aProgress.Sides[aSide].Document;
			std::visit(
				[&document, aSide](const auto& aResult) {
					if constexpr (!std::is_same_v<std::decay_t<decltype(aResult)>, std::monostate>) {
						const auto& side = aSide == 0U ? aResult.LeftDocument : aResult.RightDocument;
						if (side.has_value()) {
							document = &*side;
						}
					}
				},
				aProgress.Result);
			return document->Bytes;
		}

		/// [[BPR-1366]] Whether aProgress continues exactly this computation:
		/// the same comparison key (blobs, options digest and algorithm
		/// revision), options and limits, and the same bytes on both sides.
		template <typename TComparison>
		[[nodiscard]] auto resumes(const diff_compute_progress& aProgress, const typename TComparison::input& aInput) -> bool
		{
			using key_type  = std::remove_cvref_t<decltype(aInput.Key)>;
			const auto* key = std::get_if<key_type>(&aProgress.Key);
			if (key == nullptr || *key != aInput.Key || aProgress.Options != aInput.Options || aProgress.Limits != aInput.Limits) {
				return false;
			}
			const auto sides = std::array{ aInput.LeftBytes, aInput.RightBytes };
			const auto keyed = std::array{ left_present(aInput.Key), right_present(aInput.Key) };
			for (std::size_t side = 0U; side < sides.size(); ++side) {
				if (sides[side].has_value() != keyed[side]
					|| (sides[side].has_value() && !std::ranges::equal(*sides[side], held_bytes(aProgress, side)))) {
					return false;
				}
			}
			return true;
		}

		/// The checks that need no text, then a fresh progress holding copies
		/// of both sides. A comparison the checks finish yields its outcome.
		template <typename TComparison>
		[[nodiscard]] auto start(const typename TComparison::input& aInput)
			-> std::variant<typename TComparison::outcome, std::unique_ptr<diff_compute_progress>>
		{
			const auto digest = options_digest(aInput.Options);
			if (!valid(aInput.Key) || !digest.has_value() || aInput.Key.Options != *digest || (!left_present(aInput.Key) && aInput.LeftBytes.has_value()) || (!right_present(aInput.Key) && aInput.RightBytes.has_value())) {
				return typename TComparison::outcome{ .Reason = diff_reason_code::invalid_alignment };
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
			auto progress     = std::make_unique<diff_compute_progress>();
			progress->Key     = aInput.Key;
			progress->Options = aInput.Options;
			progress->Limits  = aInput.Limits;
			progress->Sides[0U].Document.Bytes.assign(leftBytes.begin(), leftBytes.end());
			progress->Sides[1U].Document.Bytes.assign(rightBytes.begin(), rightBytes.end());
			return progress;
		}

		/// The primary segments of a sorted match list: equal runs and the
		/// changes between them.
		[[nodiscard]] auto primary_shapes(const match_list& aMatches, const std::uint32_t aLeftLines,
										  const std::uint32_t aRightLines, diff_step_meter& aMeter)
			-> std::vector<pending_segment>
		{
			auto primary     = std::vector<pending_segment>{ };
			auto leftCursor  = std::uint32_t{ 0U };
			auto rightCursor = std::uint32_t{ 0U };
			auto matchIndex  = std::size_t{ 0U };
			while (matchIndex < aMatches.size()) {
				const auto runBegin   = matchIndex;
				const auto matchLeft  = static_cast<std::uint32_t>(aMatches[matchIndex].first);
				const auto matchRight = static_cast<std::uint32_t>(aMatches[matchIndex].second);
				append_shape(
					primary, { leftCursor, matchLeft }, { rightCursor, matchRight });
				auto equalLeftEnd  = matchLeft + 1U;
				auto equalRightEnd = matchRight + 1U;
				++matchIndex;
				while (matchIndex < aMatches.size() && aMatches[matchIndex].first == equalLeftEnd && aMatches[matchIndex].second == equalRightEnd) {
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
				aMeter.tick(1U + matchIndex - runBegin);
			}
			append_shape(
				primary,
				{ leftCursor, aLeftLines },
				{ rightCursor, aRightLines });
			return primary;
		}

		/// [[BPR-0719]] One region of the primary work list, from the stage it
		/// reached. Each region first sheds its common prefix and suffix, so
		/// the exact matrix is charged for the changed lines only. A region the
		/// matrix budget cannot cover takes the ordered-unique alignment when
		/// that is exact, otherwise patience anchors split it and the gaps
		/// recurse; a gap with nothing to anchor stays a plain replacement.
		/// Every non-exact step sets Approximate.
		///
		/// [[BPR-1366]] The prefix and suffix resume at the line; each later
		/// attempt is a restart unit whose budgets are kept only once it ends.
		auto advance_region(diff_compute_progress& aProgress, const line_view aLeft, const line_view aRight,
							diff_step_meter& aMeter) -> void
		{
			const auto equalLines = [](const normalized_line& aLhs, const normalized_line& aRhs) { return aLhs == aRhs; };
			if (aProgress.Stage == region_stage::strip) {
				auto& current = aProgress.Pending.back();
				while (current.LeftBegin < current.LeftEnd && current.RightBegin < current.RightEnd
					   && equalLines(aLeft[current.LeftBegin], aRight[current.RightBegin])) {
					aProgress.Matches.emplace_back(current.LeftBegin, current.RightBegin);
					const auto work = line_comparison_work(aLeft[current.LeftBegin], aRight[current.RightBegin]);
					++current.LeftBegin;
					++current.RightBegin;
					aMeter.tick_kept(work);
				}
				while (current.LeftBegin < current.LeftEnd && current.RightBegin < current.RightEnd
					   && equalLines(aLeft[current.LeftEnd - 1U], aRight[current.RightEnd - 1U])) {
					--current.LeftEnd;
					--current.RightEnd;
					aProgress.Matches.emplace_back(current.LeftEnd, current.RightEnd);
					aMeter.tick_kept(line_comparison_work(aLeft[current.LeftEnd], aRight[current.RightEnd]));
				}
				if (current.LeftBegin == current.LeftEnd || current.RightBegin == current.RightEnd) {
					aProgress.Pending.pop_back();
					aMeter.commit();
					return;
				}
				aProgress.Stage = region_stage::exact;
				aMeter.commit();
			}
			const auto current     = aProgress.Pending.back();
			const auto leftRegion  = aLeft.subspan(current.LeftBegin, current.LeftEnd - current.LeftBegin);
			const auto rightRegion = aRight.subspan(current.RightBegin, current.RightEnd - current.RightBegin);
			const auto finish      = [&aProgress, &aMeter, &current](const match_list& aFound) {
				for (const auto& [left, right] : aFound) {
					aProgress.Matches.emplace_back(current.LeftBegin + left, current.RightBegin + right);
				}
				aProgress.Pending.pop_back();
				aProgress.Stage = region_stage::strip;
				aMeter.commit();
			};
			if (aProgress.Stage == region_stage::exact) {
				auto       budget = aProgress.Budget;
				const auto exact  = lcs_matches(leftRegion, rightRegion, equalLines, budget, aMeter);
				aProgress.Budget  = budget;
				if (exact.has_value()) {
					finish(*exact);
					return;
				}
				aProgress.Stage = region_stage::ordered;
				aMeter.commit();
			}
			if (aProgress.Stage == region_stage::ordered) {
				auto       index      = aProgress.IndexBudget;
				const auto ordered    = ordered_unique_matches(leftRegion, rightRegion, index, aMeter);
				aProgress.IndexBudget = index;
				if (ordered.has_value()) {
					finish(*ordered);
					return;
				}
				aProgress.Approximate = true;
				aProgress.Stage       = region_stage::anchors;
				aMeter.commit();
			}
			auto       index      = aProgress.IndexBudget;
			const auto anchors    = patience_anchors(leftRegion, rightRegion, index, aMeter);
			aProgress.IndexBudget = index;
			if (!anchors.has_value() || anchors->empty()) {
				finish({ });
				return;
			}
			finish(*anchors);
			auto gapLeft  = current.LeftBegin;
			auto gapRight = current.RightBegin;
			for (const auto& [left, right] : *anchors) {
				aProgress.Pending.push_back(region{ .LeftBegin = gapLeft, .LeftEnd = current.LeftBegin + left, .RightBegin = gapRight, .RightEnd = current.RightBegin + right });
				gapLeft  = current.LeftBegin + left + 1U;
				gapRight = current.RightBegin + right + 1U;
			}
			aProgress.Pending.push_back(region{ .LeftBegin = gapLeft, .LeftEnd = current.LeftEnd, .RightBegin = gapRight, .RightEnd = current.RightEnd });
		}

		/// The engine, shared by both comparison kinds, continued from
		/// aProgress. Returns the finished outcome; a stop throws
		/// detail::diff_compute_paused and leaves aProgress at its last resume
		/// point.
		template <typename TComparison>
		auto run_engine(const typename TComparison::input& aInput, diff_compute_progress& aProgress,
						diff_step_meter& aMeter) -> typename TComparison::outcome
		{
			auto& progress = aProgress;
			auto& left     = progress.Sides[0U];
			auto& right    = progress.Sides[1U];
			for (;;) {
				switch (progress.Step) {
				case engine_step::scan_zero:
					for (; progress.Side < progress.Sides.size(); ++progress.Side, progress.Position = 0U) {
						const auto bytes = std::span<const std::uint8_t>{ progress.Sides[progress.Side].Document.Bytes };
						while (progress.Position < bytes.size()) {
							const auto piece = bytes.subspan(progress.Position, std::min(scan_chunk_bytes, bytes.size() - progress.Position));
							if (std::ranges::find(piece, std::uint8_t{ 0U }) != piece.end()) {
								return nontext_result<TComparison>(
									aInput, diff_content_state::binary, diff_reason_code::binary_file);
							}
							progress.Position += piece.size();
							aMeter.tick_kept(piece.size() / 16U);
						}
					}
					progress.Side = 0U;
					progress.Step = engine_step::scan_utf8;
					break;
				case engine_step::scan_utf8:
					for (; progress.Side < progress.Sides.size(); ++progress.Side, progress.Position = 0U) {
						const auto bytes = std::span<const std::uint8_t>{ progress.Sides[progress.Side].Document.Bytes };
						while (progress.Position < bytes.size()) {
							const auto end = utf8_piece_end(bytes, progress.Position);
							if (!valid_utf8(bytes.subspan(progress.Position, end - progress.Position))) {
								return nontext_result<TComparison>(aInput,
																   diff_content_state::unavailable,
																   diff_reason_code::decode_failure);
							}
							const auto scanned = end - progress.Position;
							progress.Position  = end;
							aMeter.tick_kept(scanned);
						}
					}
					progress.Side = 0U;
					progress.Step = engine_step::split;
					break;
				case engine_step::split:
					for (; progress.Side < progress.Sides.size(); ++progress.Side, progress.Position = 0U, progress.LineStart = 0U) {
						auto&      document  = progress.Sides[progress.Side].Document;
						const auto bytes     = std::span<const std::uint8_t>{ document.Bytes };
						const auto maximum   = static_cast<std::size_t>(aInput.Limits.MaximumLinesPerSide);
						auto       index     = progress.Position;
						auto       lineStart = progress.LineStart;
						while (index < bytes.size()) {
							const auto pieceBegin = index;
							const auto pieceEnd   = index + std::min(scan_chunk_bytes, bytes.size() - index);
							for (; index < pieceEnd; ++index) {
								// [[BPR-1432]]. Only LF ends a line, as Git and GitHub
								// count them; a CR before it is part of a CRLF ending and
								// a lone CR is content. Anchors send these line numbers.
								if (bytes[index] != static_cast<std::uint8_t>('\n')) {
									continue;
								}
								const auto crlf       = index > lineStart && bytes[index - 1U] == static_cast<std::uint8_t>('\r');
								const auto contentEnd = crlf ? index - 1U : index;
								document.Lines.push_back(decoded_text_line{
									.Content    = { static_cast<std::uint64_t>(lineStart),
													static_cast<std::uint64_t>(contentEnd) },
									.Terminator = { static_cast<std::uint64_t>(contentEnd),
													static_cast<std::uint64_t>(index + 1U) },
									.Ending     = crlf ? text_line_ending::crlf : text_line_ending::lf,
								});
								if (document.Lines.size() > maximum) {
									return nontext_result<TComparison>(aInput,
																	   diff_content_state::unavailable,
																	   diff_reason_code::materialization_limit);
								}
								lineStart = index + 1U;
							}
							progress.Position  = index;
							progress.LineStart = lineStart;
							aMeter.tick_kept(index - pieceBegin);
						}
						if (lineStart < bytes.size()) {
							document.Lines.push_back(decoded_text_line{
								.Content    = { static_cast<std::uint64_t>(lineStart),
												static_cast<std::uint64_t>(bytes.size()) },
								.Terminator = { static_cast<std::uint64_t>(bytes.size()),
												static_cast<std::uint64_t>(bytes.size()) },
								.Ending     = text_line_ending::none,
							});
						}
						if (document.Lines.size() > maximum) {
							return nontext_result<TComparison>(aInput,
															   diff_content_state::unavailable,
															   diff_reason_code::materialization_limit);
						}
						document.HasFinalNewline = !bytes.empty() && bytes.back() == static_cast<std::uint8_t>('\n');
					}
					progress.Side = 0U;
					progress.Step = engine_step::normalize;
					break;
				case engine_step::normalize:
					for (; progress.Side < progress.Sides.size(); ++progress.Side) {
						auto& side = progress.Sides[progress.Side];
						if (side.Normalized.capacity() < side.Document.Lines.size()) {
							side.Normalized.reserve(side.Document.Lines.size());
						}
						while (side.Normalized.size() < side.Document.Lines.size()) {
							const auto& line = side.Document.Lines[side.Normalized.size()];
							side.Normalized.push_back(normalize_line(side.Document, line, aInput.Options));
							aMeter.tick_kept(normalize_steps(line));
						}
					}
					progress.Side                                                               = 0U;
					progress.Budget                                                             = matrix_budget{ .Limit = aInput.Limits.MaximumMatrixCells };
					progress.IndexBudget                                                        = work_budget{ .Limit = aInput.Limits.MaximumPrimaryIndexWork };
					progress.Pending                                                            = { region{ .LeftEnd = left.Normalized.size(), .RightEnd = right.Normalized.size() } };
					progress.Stage                                                              = region_stage::strip;
					progress.PhaseStarts[static_cast<std::size_t>(diff_compute_phase::primary)] = aMeter.steps();
					progress.Step                                                               = engine_step::primary;
					break;
				case engine_step::primary:
					while (!progress.Pending.empty()) {
						advance_region(progress, line_view{ left.Normalized }, line_view{ right.Normalized }, aMeter);
					}
					progress.Step = engine_step::sort_matches;
					break;
				case engine_step::sort_matches:
					std::ranges::sort(progress.Matches);
					progress.Step = engine_step::primary_shapes;
					aMeter.tick_kept(progress.Matches.size());
					break;
				case engine_step::primary_shapes:
					progress.Primary                                                           = primary_shapes(progress.Matches,
																												static_cast<std::uint32_t>(left.Normalized.size()),
																												static_cast<std::uint32_t>(right.Normalized.size()),
																												aMeter);
					progress.Matches                                                           = { };
					progress.Cursor                                                            = 0U;
					progress.PhaseStarts[static_cast<std::size_t>(diff_compute_phase::refine)] = aMeter.steps();
					progress.Step                                                              = engine_step::refine;
					aMeter.commit();
					break;
				case engine_step::refine:
					while (progress.Cursor < progress.Primary.size()) {
						const auto& segment = progress.Primary[progress.Cursor];
						if (segment.Kind != alignment_kind::replacement) {
							progress.Refined.push_back(segment);
							++progress.Cursor;
							aMeter.tick_kept(1U);
							continue;
						}
						auto       budget = progress.Budget;
						const auto pairs  = secondary_pairs(
							left, right, segment.Left, segment.Right, aInput.Options, budget, aMeter);
						progress.Budget = budget;
						if (!pairs.has_value()) {
							// [[BPR-0719]]: an unpairable replacement stays a plain
							// replacement instead of refusing the file.
							progress.Approximate = true;
							progress.Refined.push_back(segment);
						} else {
							auto leftPosition  = segment.Left.Begin;
							auto rightPosition = segment.Right.Begin;
							for (const auto& pair : *pairs) {
								append_shape(progress.Refined,
											 { leftPosition, pair.Left },
											 { rightPosition, pair.Right });
								append_shape(
									progress.Refined,
									{ pair.Left, static_cast<std::uint32_t>(pair.Left + 1U) },
									{ pair.Right, static_cast<std::uint32_t>(pair.Right + 1U) },
									pair.Kind);
								leftPosition  = pair.Left + 1U;
								rightPosition = pair.Right + 1U;
							}
							append_shape(progress.Refined,
										 { leftPosition, segment.Left.End },
										 { rightPosition, segment.Right.End });
						}
						++progress.Cursor;
						aMeter.commit();
					}
					if (progress.Refined.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) || progress.Refined.size() > static_cast<std::size_t>(aInput.Limits.MaximumSegments)) {
						return nontext_result<TComparison>(aInput,
														   diff_content_state::unavailable,
														   diff_reason_code::compute_limit);
					}
					progress.Primary                                                          = { };
					progress.PhaseStarts[static_cast<std::size_t>(diff_compute_phase::moves)] = aMeter.steps();
					progress.Step                                                             = engine_step::moves;
					break;
				case engine_step::moves:
					if (aInput.Options.Moves == move_detection_policy::deterministic_exact) {
						progress.Moved = refine_moved_subblocks(
							progress.Refined,
							left,
							right,
							aInput.Limits.MaximumMoveDetectionWork,
							aInput.Limits.MaximumSegments,
							aMeter);
						progress.Refined = { };
					} else {
						progress.Moved = moved_refinement{ .Segments = std::move(progress.Refined) };
					}
					progress.Digest = comparison_digest(aInput.Key);
					progress.SegmentGroups.assign(progress.Moved.Segments.size(), std::nullopt);
					progress.GroupOrdinal                                                        = 0U;
					progress.Cursor                                                              = 0U;
					progress.PhaseStarts[static_cast<std::size_t>(diff_compute_phase::assemble)] = aMeter.steps();
					progress.Step                                                                = engine_step::groups;
					aMeter.commit();
					break;
				case engine_step::groups: {
					const auto& pending = progress.Moved.Segments;
					while (progress.Cursor < pending.size()) {
						if (pending[progress.Cursor].Kind == alignment_kind::equal) {
							++progress.Cursor;
							aMeter.tick_kept(1U);
							continue;
						}
						const auto begin = progress.Cursor;
						auto       end   = begin;
						while (end < pending.size() && pending[end].Kind != alignment_kind::equal) {
							++end;
						}
						const auto& first         = pending[begin];
						const auto& last          = pending[end - 1U];
						const auto  leftSpan      = line_span{ first.Left.Begin, last.Left.End };
						const auto  rightSpan     = line_span{ first.Right.Begin, last.Right.End };
						const auto  id            = group_identity(progress.Digest,
																   leftSpan,
																   rightSpan,
																   progress.GroupOrdinal++);
						auto        added         = std::uint64_t{ 0U };
						auto        deleted       = std::uint64_t{ 0U };
						auto        replacedLeft  = std::uint64_t{ 0U };
						auto        replacedRight = std::uint64_t{ 0U };
						for (auto member = begin; member < end; ++member) {
							const auto& segment = pending[member];
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
						progress.Groups.push_back(change_group{
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
							progress.SegmentGroups[member] = id;
						}
						progress.Cursor = end;
						aMeter.tick_kept(detail::validation_record_steps + (end - begin));
					}
					progress.Cursor = 0U;
					progress.Segments.reserve(pending.size());
					progress.Step = engine_step::segments;
					break;
				}
				case engine_step::segments: {
					const auto& pending = progress.Moved.Segments;
					while (progress.Cursor < pending.size()) {
						const auto& source = pending[progress.Cursor];
						progress.Segments.push_back(alignment_segment{
							.Id    = segment_identity(progress.Digest,
													  source.Kind,
													  source.Left,
													  source.Right,
													  static_cast<std::uint32_t>(progress.Cursor)),
							.Kind  = source.Kind,
							.Left  = source.Left,
							.Right = source.Right,
							.Group = progress.SegmentGroups[progress.Cursor],
						});
						++progress.Cursor;
						aMeter.tick_kept(detail::validation_record_steps);
					}
					progress.Cursor = 0U;
					progress.Step   = engine_step::pairs;
					break;
				}
				case engine_step::pairs: {
					const auto& pending = progress.Moved.Segments;
					while (progress.Cursor < pending.size()) {
						const auto& source = pending[progress.Cursor];
						if (!source.PairKind.has_value()) {
							++progress.Cursor;
							aMeter.tick_kept(1U);
							continue;
						}
						const auto ordinal = static_cast<std::uint32_t>(progress.Pairs.size());
						const auto pair    = line_pair{
							.Id        = pair_identity(progress.Digest,
													   progress.Segments[progress.Cursor].Id,
													   source.Left.Begin,
													   source.Right.Begin,
													   *source.PairKind,
													   ordinal),
							.Segment   = progress.Segments[progress.Cursor].Id,
							.LeftLine  = source.Left.Begin,
							.RightLine = source.Right.Begin,
							.Kind      = *source.PairKind,
						};
						if (pair.Kind == line_pair_kind::similar) {
							auto       budget = progress.Budget;
							const auto ranges = intraline_ranges(left.Document,
																 right.Document,
																 pair.LeftLine,
																 pair.RightLine,
																 budget,
																 aMeter);
							progress.Budget   = budget;
							progress.Pairs.push_back(pair);
							if (!ranges.has_value()) {
								// [[BPR-0719]]: the pair stays similar without intraline
								// edits instead of refusing the file.
								progress.Approximate = true;
							} else {
								for (const auto& [leftRange, rightRange] : *ranges) {
									progress.Intraline.push_back(intraline_edit{
										.Pair  = pair.Id,
										.Left  = leftRange,
										.Right = rightRange,
									});
								}
							}
						} else {
							progress.Pairs.push_back(pair);
						}
						++progress.Cursor;
						aMeter.tick_kept(detail::validation_record_steps);
					}
					progress.Cursor = 0U;
					progress.Step   = engine_step::move_relations;
					break;
				}
				case engine_step::move_relations: {
					while (progress.Cursor < progress.Moved.Moves.size()) {
						const auto [source, destination] = progress.Moved.Moves[progress.Cursor];
						const auto ordinal               = static_cast<std::uint32_t>(progress.Moves.size());
						progress.Moves.push_back(move_relation{
							.Id = move_identity(
								progress.Digest,
								progress.Segments[source].Id,
								progress.Segments[destination].Id,
								ordinal),
							.SourceDeletion       = progress.Segments[source].Id,
							.DestinationInsertion = progress.Segments[destination].Id,
						});
						++progress.Cursor;
						aMeter.tick_kept(detail::validation_record_steps);
					}
					auto alignment = typename TComparison::alignment{
						.Key            = aInput.Key,
						.Options        = aInput.Options,
						.Digest         = progress.Digest,
						.LeftLineCount  = static_cast<std::uint32_t>(left.Document.Lines.size()),
						.RightLineCount = static_cast<std::uint32_t>(right.Document.Lines.size()),
					};
					alignment.Segments = std::move(progress.Segments);
					alignment.Pairs    = std::move(progress.Pairs);
					alignment.Groups   = std::move(progress.Groups);
					alignment.Moves    = std::move(progress.Moves);
					auto result        = typename TComparison::computed{ };
					result.Alignment   = std::move(alignment);
					if (left_present(aInput.Key)) {
						result.LeftDocument = std::move(left.Document);
					}
					if (right_present(aInput.Key)) {
						result.RightDocument = std::move(right.Document);
					}
					result.Intraline                                                             = std::move(progress.Intraline);
					result.MatrixCellsConsumed                                                   = progress.Budget.Consumed;
					result.MoveDetectionWorkConsumed                                             = progress.Moved.WorkConsumed;
					result.MoveDetectionWorkLimitReached                                         = progress.Moved.WorkLimitReached;
					result.MoveDetectionSegmentLimitReached                                      = progress.Moved.SegmentLimitReached;
					result.PrimaryIndexWorkConsumed                                              = progress.IndexBudget.Consumed;
					result.AlignmentApproximate                                                  = progress.Approximate;
					progress.Result                                                              = std::move(result);
					progress.Sides                                                               = { };
					progress.Moved                                                               = { };
					progress.SegmentGroups                                                       = { };
					progress.Validation                                                          = { };
					progress.PhaseStarts[static_cast<std::size_t>(diff_compute_phase::validate)] = aMeter.steps();
					progress.Step                                                                = engine_step::validate_alignment;
					break;
				}
				case engine_step::validate_alignment:
					if (!detail::validate_alignment(std::get<typename TComparison::computed>(progress.Result).Alignment, aMeter).valid()) {
						return { .Reason = diff_reason_code::invalid_alignment };
					}
					progress.Step = engine_step::validate_result;
					aMeter.commit();
					break;
				case engine_step::validate_result: {
					auto& result = std::get<typename TComparison::computed>(progress.Result);
					if (!validate_computed_records(result, progress.Validation, aMeter).valid()) {
						return { .Reason = diff_reason_code::invalid_alignment };
					}
					return { .Reason = diff_reason_code::none, .Result = std::move(result) };
				}
				}
			}
		}

		/// One slice of a computation: starts it, or continues the paused state
		/// it was given when that state continues exactly this computation.
		template <typename TComparison, typename TSlice>
		auto compute_slice(const typename TComparison::input& aInput, const diff_compute_control& aControl,
						   paused_diff_compute aResume) -> TSlice
		{
			auto slice    = TSlice{ };
			auto progress = diff_compute_progress_access::take(aResume);
			if (progress && !resumes<TComparison>(*progress, aInput)) {
				progress.reset();
			}
			slice.Statistics.Resumed = progress != nullptr;
			if (!progress) {
				auto started = start<TComparison>(aInput);
				if (auto* finished = std::get_if<typename TComparison::outcome>(&started)) {
					slice.Outcome           = std::move(*finished);
					slice.Statistics.Slices = 1U;
					slice.Statistics.Phase  = diff_compute_phase::finished;
					return slice;
				}
				progress = std::move(std::get<std::unique_ptr<diff_compute_progress>>(started));
			}
			auto meter = diff_step_meter{ aControl.Stop, aControl.CheckInterval, progress->Steps };
			try {
				slice.Outcome = run_engine<TComparison>(aInput, *progress, meter);
			} catch (const detail::diff_compute_paused&) {
				slice.Outcome.reset();
			}
			progress->WorkSteps += meter.slice_steps();
			progress->LargestUnit      = std::max(progress->LargestUnit, meter.largest_unit());
			progress->LargestPollDelay = std::max(progress->LargestPollDelay, meter.largest_poll_delay());
			++progress->Slices;
			slice.Statistics.Steps              = slice.Outcome.has_value() ? meter.steps() : meter.committed();
			slice.Statistics.WorkSteps          = progress->WorkSteps;
			slice.Statistics.LargestRestartUnit = progress->LargestUnit;
			slice.Statistics.LargestPollDelay   = progress->LargestPollDelay;
			slice.Statistics.Slices             = progress->Slices;
			slice.Statistics.Phase              = slice.Outcome.has_value() ? diff_compute_phase::finished : phase_of(progress->Step);
			slice.Statistics.PhaseStarts        = progress->PhaseStarts;
			if (!slice.Outcome.has_value()) {
				progress->Steps         = meter.committed();
				progress->RetainedBytes = progress_bytes(*progress);
				slice.Paused            = diff_compute_progress_access::make(std::move(progress));
			}
			return slice;
		}
	} // namespace

	paused_diff_compute::paused_diff_compute() noexcept = default;

	paused_diff_compute::~paused_diff_compute() = default;

	paused_diff_compute::paused_diff_compute(paused_diff_compute&& aOther) noexcept = default;

	auto paused_diff_compute::operator=(paused_diff_compute&& aOther) noexcept -> paused_diff_compute& = default;

	auto paused_diff_compute::empty() const noexcept -> bool
	{
		return Progress == nullptr;
	}

	auto paused_diff_compute::retained_bytes() const noexcept -> std::uint64_t
	{
		return Progress ? Progress->RetainedBytes : 0U;
	}

	auto paused_diff_compute::statistics() const noexcept -> diff_compute_statistics
	{
		if (!Progress) {
			return { };
		}
		return {
			.Steps              = Progress->Steps,
			.WorkSteps          = Progress->WorkSteps,
			.LargestRestartUnit = Progress->LargestUnit,
			.LargestPollDelay   = Progress->LargestPollDelay,
			.Slices             = Progress->Slices,
			.Phase              = phase_of(Progress->Step),
			.PhaseStarts        = Progress->PhaseStarts,
		};
	}

	auto compute_file_diff(const diff_compute_input& aInput) -> diff_compute_outcome
	{
		return *compute_slice<pull_request_comparison, file_diff_compute_slice>(aInput, { }, { }).Outcome;
	}

	auto compute_file_diff(const diff_compute_input& aInput, const diff_compute_control& aControl,
						   paused_diff_compute aResume) -> file_diff_compute_slice
	{
		return compute_slice<pull_request_comparison, file_diff_compute_slice>(aInput, aControl, std::move(aResume));
	}

	auto compute_material_diff(const material_diff_input& aInput) -> material_diff_outcome
	{
		return *compute_slice<material_comparison, material_diff_compute_slice>(aInput, { }, { }).Outcome;
	}

	auto compute_material_diff(const material_diff_input& aInput, const diff_compute_control& aControl,
							   paused_diff_compute aResume) -> material_diff_compute_slice
	{
		return compute_slice<material_comparison, material_diff_compute_slice>(aInput, aControl, std::move(aResume));
	}

} // namespace bpr::core
