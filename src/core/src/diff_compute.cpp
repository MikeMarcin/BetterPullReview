#include "bpr/core/diff_compute.hpp"

#include "bpr/core/diff_compute_components.hpp"
#include "diff_intraline.hpp"
#include "diff_step_meter.hpp"

#include <rapidfuzz/distance/Indel.hpp>

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
#include <unordered_map>
#include <utility>
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

		/// [[BPR-1465]] A component's steps, counted by the engine's meter.
		class component_meter final : public diff_component_meter
		{
		public:
			explicit component_meter(diff_step_meter& aMeter) noexcept
				: Meter{ &aMeter }
			{
			}

			void tick(const std::uint64_t aSteps) override
			{
				Meter->tick(aSteps);
			}

			void poll() override
			{
				Meter->poll();
			}

		private:
			diff_step_meter* Meter = nullptr;
		};

		/// [[BPR-1465]] Thrown when a component supplies a value the engine's
		/// rules refuse; the computation ends refused.
		struct invalid_component_result
		{ };

		/// [[BPR-1465]] Whether supplied matches are strictly increasing on both
		/// sides, in range, and join equal values.
		[[nodiscard]] auto valid_supplied_matches(const std::span<const diff_match>    aMatches,
												  const std::span<const std::uint32_t> aLeft,
												  const std::span<const std::uint32_t> aRight) noexcept -> bool
		{
			for (std::size_t index = 0U; index < aMatches.size(); ++index) {
				const auto& match = aMatches[index];
				if (match.Left >= aLeft.size() || match.Right >= aRight.size() || aLeft[match.Left] != aRight[match.Right]) {
					return false;
				}
				if (index != 0U && (match.Left <= aMatches[index - 1U].Left || match.Right <= aMatches[index - 1U].Right)) {
					return false;
				}
			}
			return true;
		}

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

		/// [[BPR-1480]] The steps `lcs_matches` counted for a character table
		/// too large to count in bulk, (aLeft + 1) x (aRight + 1) cells over
		/// `small_table_cells`: its zeroing, then its fill, in the same
		/// increments. Pairing no longer fills that table, but it keeps the
		/// engine's step accounting, so every poll and phase start
		/// falls where it did.
		auto charge_large_similarity_table(const std::size_t aLeft,
										   const std::size_t aRight,
										   diff_step_meter&  aMeter) -> void
		{
			const auto count = (aLeft + 1U) * (aRight + 1U);
			for (auto zeroed = std::size_t{ 0U }; zeroed < count;) {
				const auto piece = std::min(count - zeroed, table_tick_cells * 4U);
				zeroed += piece;
				aMeter.tick(piece / 16U);
			}
			if (aRight <= table_tick_cells) {
				auto uncounted = std::size_t{ 0U };
				for (auto row = aLeft; row != 0U; --row) {
					uncounted += aRight;
					if (uncounted >= table_tick_cells) {
						aMeter.tick(uncounted);
						uncounted = 0U;
					}
				}
				aMeter.tick(uncounted + aLeft + aRight);
				return;
			}
			for (auto row = aLeft; row != 0U; --row) {
				for (auto column = aRight; column != 0U;) {
					const auto pieceEnd = column > table_tick_cells ? column - table_tick_cells : std::size_t{ 0U };
					aMeter.tick(column - pieceEnd);
					column = pieceEnd;
				}
			}
			aMeter.tick(aLeft + aRight);
		}

		/// [[BPR-1480]] Scores one left line of a replacement against right
		/// lines with rapidfuzz-cpp's Indel distance, which counts the
		/// insertions and deletions between two lines, so the longest common
		/// subsequence is (L + R - distance) / 2 and the score is the engine's
		/// 2 x LCS x 1000 / (L + R), floored, in integers. The left line's
		/// pattern is built once, on its first scored pair.
		///
		/// The matrix budget is charged, and the steps counted, exactly as the
		/// character LCS table that this replaces charged them: an
		/// (L + 1) x (R + 1) table per scored pair, a small one added to
		/// aUncounted for the caller to count in bulk. A limit therefore stops
		/// at the same pair and leaves the same replacements unpaired.
		///
		/// A pair below the threshold scores 0 instead of its exact score: the
		/// distance is only computed up to the threshold's cutoff, and pairing
		/// reads no score under the threshold.
		class line_pair_scorer
		{
		public:
			line_pair_scorer(const normalized_line& aLeft, const std::uint16_t aThresholdPermille) noexcept
				: Left{ &aLeft }
				, Threshold{ std::min(aThresholdPermille, maximum_similarity_threshold_permille) }
			{
			}

			[[nodiscard]] auto score(const normalized_line& aRight,
									 matrix_budget&         aBudget,
									 diff_step_meter&       aMeter,
									 std::uint64_t&         aUncounted) -> std::optional<std::uint16_t>
			{
				if (*Left == aRight) {
					return maximum_similarity_threshold_permille;
				}
				const auto leftSize  = Left->Values.size();
				const auto rightSize = aRight.Values.size();
				if (leftSize == 0U || rightSize == 0U) {
					return std::uint16_t{ 0U };
				}
				constexpr auto most    = std::numeric_limits<std::size_t>::max();
				const auto     cells   = (leftSize + 1U) * (rightSize + 1U);
				const auto     small   = cells <= small_table_cells;
				const auto     charged = leftSize != most && rightSize != most
					&& leftSize + 1U <= most / (rightSize + 1U)
					&& aBudget.consume(leftSize + 1U, rightSize + 1U);
				if (small) {
					aUncounted += cells;
				}
				if (!charged) {
					return std::nullopt;
				}
				if (!small) {
					charge_large_similarity_table(leftSize, rightSize, aMeter);
				}
				if (!Cached.has_value()) {
					Cached.emplace(Left->Values.begin(), Left->Values.end());
				}
				constexpr auto maximum = std::uint64_t{ maximum_similarity_threshold_permille };
				const auto     total   = static_cast<std::uint64_t>(leftSize) + static_cast<std::uint64_t>(rightSize);
				// The largest distance whose score can still reach the
				// threshold, plus one: Indel stops counting past it.
				const auto cutoff   = total * (maximum - Threshold) / maximum + 1U;
				const auto distance = static_cast<std::uint64_t>(
					Cached->distance(aRight.Values.begin(), aRight.Values.end(), static_cast<std::size_t>(cutoff)));
				if (distance > cutoff) {
					return std::uint16_t{ 0U };
				}
				return static_cast<std::uint16_t>((total - distance) * maximum / total);
			}

		private:
			const normalized_line*                               Left      = nullptr;
			std::uint16_t                                        Threshold = 0U;
			std::optional<rapidfuzz::CachedIndel<std::uint32_t>> Cached;
		};

		/// [[BPR-1465]] A component's scores for one replacement. The budget is
		/// charged pair by pair exactly as `line_pair_scorer` charges it, so a limit
		/// stops at the same pair and leaves the same consumption behind.
		[[nodiscard]] auto component_scores(const prepared_document& aLeft,
											const prepared_document& aRight,
											const line_span          aLeftSpan,
											const line_span          aRightSpan,
											const diff_options&      aOptions,
											matrix_budget&           aBudget,
											diff_step_meter&         aMeter,
											diff_components&         aComponents)
			-> std::optional<std::optional<std::vector<std::uint16_t>>>
		{
			const auto leftCount  = static_cast<std::size_t>(aLeftSpan.extent());
			const auto rightCount = static_cast<std::size_t>(aRightSpan.extent());
			for (std::size_t leftIndex = 0U; leftIndex < leftCount; ++leftIndex) {
				const auto& left = aLeft.Normalized[static_cast<std::size_t>(aLeftSpan.Begin) + leftIndex];
				for (std::size_t rightIndex = 0U; rightIndex < rightCount; ++rightIndex) {
					const auto& right = aRight.Normalized[static_cast<std::size_t>(aRightSpan.Begin) + rightIndex];
					aMeter.tick(1U);
					if (left.Values.empty() || right.Values.empty() || left == right) {
						continue;
					}
					if (!aBudget.consume(left.Values.size() + 1U, right.Values.size() + 1U)) {
						return std::nullopt;
					}
				}
			}
			auto leftLines  = std::vector<std::span<const std::uint32_t>>{ };
			auto rightLines = std::vector<std::span<const std::uint32_t>>{ };
			leftLines.reserve(leftCount);
			rightLines.reserve(rightCount);
			for (std::size_t index = 0U; index < leftCount; ++index) {
				leftLines.emplace_back(aLeft.Normalized[static_cast<std::size_t>(aLeftSpan.Begin) + index].Values);
			}
			for (std::size_t index = 0U; index < rightCount; ++index) {
				rightLines.emplace_back(aRight.Normalized[static_cast<std::size_t>(aRightSpan.Begin) + index].Values);
			}
			auto meter  = component_meter{ aMeter };
			auto scores = aComponents.pair_scores(
				diff_pairing_request{ .Left = leftLines, .Right = rightLines, .ThresholdPermille = aOptions.SimilarityThresholdPermille }, meter);
			if (scores.has_value()
				&& (scores->size() != leftCount * rightCount
					|| std::ranges::any_of(*scores, [](const std::uint16_t aScore) { return aScore > maximum_similarity_threshold_permille; }))) {
				throw invalid_component_result{ };
			}
			return scores;
		}

		[[nodiscard]] auto secondary_pairs(const prepared_document& aLeft,
										   const prepared_document& aRight,
										   const line_span          aLeftSpan,
										   const line_span          aRightSpan,
										   const diff_options&      aOptions,
										   matrix_budget&           aBudget,
										   diff_step_meter&         aMeter,
										   diff_components* const   aComponents = nullptr)
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
			auto supplied            = false;
			if (aComponents != nullptr) {
				auto budget = aBudget;
				auto found  = component_scores(aLeft, aRight, aLeftSpan, aRightSpan, aOptions, budget, aMeter, *aComponents);
				if (!found.has_value()) {
					aBudget = budget;
					return std::nullopt;
				}
				if (found->has_value()) {
					aBudget  = budget;
					scores   = std::move(**found);
					supplied = true;
				}
			}
			for (std::size_t leftIndex = 0U; !supplied && leftIndex < leftCount; ++leftIndex) {
				auto scorer = line_pair_scorer{ aLeft.Normalized[static_cast<std::size_t>(aLeftSpan.Begin) + leftIndex], aOptions.SimilarityThresholdPermille };
				for (std::size_t rightIndex = 0U; rightIndex < rightCount;
					 ++rightIndex) {
					const auto score = scorer.score(
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

		using match_list = std::vector<std::pair<std::size_t, std::size_t>>;

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
											diff_step_meter&             aMeter,
											diff_components* const       aComponents = nullptr)
			-> std::optional<std::vector<std::pair<byte_span, byte_span>>>
		{
			auto leftScalars  = std::vector<unicode_scalar>{ };
			auto rightScalars = std::vector<unicode_scalar>{ };
			static_cast<void>(valid_utf8(line_bytes(aLeft, aLeftLine), &leftScalars));
			static_cast<void>(
				valid_utf8(line_bytes(aRight, aRightLine), &rightScalars));
			// The scalar values that a component and dtl compare.
			const auto values = [](const std::vector<unicode_scalar>& aScalars) {
				auto result = std::vector<std::uint32_t>{ };
				result.reserve(aScalars.size());
				for (const auto& scalar : aScalars) {
					result.push_back(scalar.Value);
				}
				return result;
			};
			const auto leftValues  = values(leftScalars);
			const auto rightValues = values(rightScalars);
			aMeter.tick(leftValues.size() + rightValues.size());
			auto matches = std::optional<std::vector<std::pair<std::size_t, std::size_t>>>{ };
			if (aComponents != nullptr) {
				// [[BPR-1465]] A component's matched scalars, checked before use.
				auto       meter    = component_meter{ aMeter };
				const auto supplied = aComponents->intraline_matches(leftValues, rightValues, meter);
				if (supplied.has_value()) {
					if (!valid_supplied_matches(*supplied, leftValues, rightValues)) {
						throw invalid_component_result{ };
					}
					matches.emplace();
					matches->reserve(supplied->size());
					for (const auto& match : *supplied) {
						matches->emplace_back(match.Left, match.Right);
					}
				}
			}
			if (!matches.has_value()) {
				// [[BPR-1481]] dtl's common scalars, charged to the step meter but
				// not to the matrix budget, which pairing then has to itself. A
				// pair too large for dtl keeps the budgeted table below.
				matches = detail::intraline_common_scalars(leftValues, rightValues, aMeter);
			}
			if (!matches.has_value()) {
				matches = lcs_matches(
					leftScalars,
					rightScalars,
					[](const unicode_scalar& aLhs, const unicode_scalar& aRhs) { return aLhs.Value == aRhs.Value; },
					aBudget,
					aMeter);
			}
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

		/// The computed-diff rules, shared by both comparison kinds. [[BPR-1366]]
		/// aMeter counts each check; a stop abandons the validation.
		template <typename TComputed, typename TMeter>
		auto validate_computed_records(const TComputed& aDiff, TMeter& aMeter)
			-> diff_compute_validation_result
		{
			if (!metered_validate_alignment(aDiff.Alignment, aMeter).valid()) {
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
			if (aDiff.LeftDocument.has_value() && !valid_document(*aDiff.LeftDocument, aMeter)) {
				return compute_failure(diff_compute_validation_error::invalid_document);
			}
			if (aDiff.RightDocument.has_value() && !valid_document(*aDiff.RightDocument, aMeter)) {
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

			for (std::size_t index = 0U; index < aDiff.Alignment.Segments.size(); ++index) {
				const auto& segment = aDiff.Alignment.Segments[index];
				aMeter.tick(1U);
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
					aMeter.tick(normalize_steps(aDiff.LeftDocument->Lines[leftLine]) + normalize_steps(aDiff.RightDocument->Lines[rightLine]));
				}
			}
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
			return { };
		}
	} // namespace

	void detail::diff_step_meter::check()
	{
		LargestPollDelay = std::max(LargestPollDelay, Steps - NextCheck);
		NextCheck        = advance(Steps);
		if (Stop->stop_requested(Steps)) {
			throw diff_compute_stopped{ };
		}
	}

	void detail::diff_step_meter::poll()
	{
		if (Stop != nullptr && Stop->stop_requested(Steps)) {
			throw diff_compute_stopped{ };
		}
	}

	auto validate_computed_file_diff(const computed_file_diff& aDiff)
		-> diff_compute_validation_result
	{
		auto meter = detail::unmetered{ };
		return validate_computed_records(aDiff, meter);
	}

	auto validate_computed_material_diff(const computed_material_diff& aDiff)
		-> diff_compute_validation_result
	{
		auto meter = detail::unmetered{ };
		return validate_computed_records(aDiff, meter);
	}

	auto detail::validate_computed_file_diff(const computed_file_diff& aDiff, diff_step_meter& aMeter)
		-> diff_compute_validation_result
	{
		return validate_computed_records(aDiff, aMeter);
	}

	namespace {
		/// [[BPR-1485]] Thrown when the line matcher supplies no primary
		/// matches: the comparison is refused as a host failure.
		struct declined_primary
		{ };

		/// Where one computation has reached, for its statistics.
		struct engine_progress
		{
			diff_compute_phase                                                                Phase = diff_compute_phase::prepare;
			std::array<std::uint64_t, static_cast<std::size_t>(diff_compute_phase::finished)> PhaseStarts{ };

			auto begin(const diff_compute_phase aPhase, const detail::diff_step_meter& aMeter) noexcept -> void
			{
				Phase                                         = aPhase;
				PhaseStarts[static_cast<std::size_t>(aPhase)] = aMeter.steps();
			}
		};

		/// The checks that need no text. A comparison they finish yields its
		/// outcome.
		template <typename TComparison>
		[[nodiscard]] auto checked_start(const typename TComparison::input& aInput)
			-> std::optional<typename TComparison::outcome>
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
			return std::nullopt;
		}

		/// [[BPR-1465]] One class per line of both sides: equal normalized lines
		/// share a class. Classes are numbered in order of first appearance,
		/// left side first, so they are the same in every run.
		struct normalized_line_pointer_hash
		{
			[[nodiscard]] auto operator()(const normalized_line* const aLine) const noexcept -> std::size_t
			{
				auto value = std::uint64_t{ 0xCBF2'9CE4'8422'2325ULL };
				for (const auto scalar : aLine->Values) {
					value = (value ^ scalar) * 0x0000'0100'0000'01B3ULL;
				}
				return static_cast<std::size_t>(value);
			}
		};

		struct normalized_line_pointer_equal
		{
			[[nodiscard]] auto operator()(const normalized_line* const aLeft, const normalized_line* const aRight) const noexcept -> bool
			{
				return *aLeft == *aRight;
			}
		};

		struct line_classes
		{
			std::vector<std::uint32_t> Left;
			std::vector<std::uint32_t> Right;
		};

		[[nodiscard]] auto assign_line_classes(const std::array<prepared_document, 2U>& aSides, detail::diff_step_meter& aMeter)
			-> line_classes
		{
			auto classes = std::unordered_map<const normalized_line*, std::uint32_t, normalized_line_pointer_hash, normalized_line_pointer_equal>{ };
			classes.reserve(aSides[0U].Normalized.size() + aSides[1U].Normalized.size());
			auto result = line_classes{ };
			for (std::size_t side = 0U; side < aSides.size(); ++side) {
				auto& target = side == 0U ? result.Left : result.Right;
				target.reserve(aSides[side].Normalized.size());
				for (const auto& line : aSides[side].Normalized) {
					aMeter.tick(2U + 2U * static_cast<std::uint64_t>(line.Values.size()));
					const auto [entry, inserted] = classes.try_emplace(&line, static_cast<std::uint32_t>(classes.size()));
					static_cast<void>(inserted);
					target.push_back(entry->second);
				}
			}
			return result;
		}

		/// [[BPR-1465]] Supplied matches as the engine's match list, refused
		/// unless they keep the primary rules.
		[[nodiscard]] auto checked_matches(const line_classes& aClasses, const std::vector<diff_match>& aMatches)
			-> match_list
		{
			if (!valid_supplied_matches(aMatches, aClasses.Left, aClasses.Right)) {
				throw invalid_component_result{ };
			}
			auto result = match_list{ };
			result.reserve(aMatches.size());
			for (const auto& match : aMatches) {
				result.emplace_back(match.Left, match.Right);
			}
			return result;
		}

		/// The primary segments of a sorted match list: equal runs and the
		/// changes between them.
		[[nodiscard]] auto primary_shapes(const match_list& aMatches, const std::uint32_t aLeftLines,
										  const std::uint32_t aRightLines, detail::diff_step_meter& aMeter)
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

		/// Splits one side into LF-ended lines. Returns false past the line limit.
		[[nodiscard]] auto split_lines(decoded_text_document& aDocument, const std::size_t aMaximum, detail::diff_step_meter& aMeter)
			-> bool
		{
			const auto bytes     = std::span<const std::uint8_t>{ aDocument.Bytes };
			auto       index     = std::size_t{ 0U };
			auto       lineStart = std::size_t{ 0U };
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
					aDocument.Lines.push_back(decoded_text_line{
						.Content    = { static_cast<std::uint64_t>(lineStart),
										static_cast<std::uint64_t>(contentEnd) },
						.Terminator = { static_cast<std::uint64_t>(contentEnd),
										static_cast<std::uint64_t>(index + 1U) },
						.Ending     = crlf ? text_line_ending::crlf : text_line_ending::lf,
					});
					if (aDocument.Lines.size() > aMaximum) {
						return false;
					}
					lineStart = index + 1U;
				}
				aMeter.tick(index - pieceBegin);
			}
			if (lineStart < bytes.size()) {
				aDocument.Lines.push_back(decoded_text_line{
					.Content    = { static_cast<std::uint64_t>(lineStart),
									static_cast<std::uint64_t>(bytes.size()) },
					.Terminator = { static_cast<std::uint64_t>(bytes.size()),
									static_cast<std::uint64_t>(bytes.size()) },
					.Ending     = text_line_ending::none,
				});
			}
			aDocument.HasFinalNewline = !bytes.empty() && bytes.back() == static_cast<std::uint8_t>('\n');
			return aDocument.Lines.size() <= aMaximum;
		}

		/// The engine, shared by both comparison kinds. Returns the finished
		/// outcome; a stop throws detail::diff_compute_stopped and the work so
		/// far is abandoned.
		template <typename TComparison>
		auto run_engine(const typename TComparison::input& aInput, diff_components& aComponents,
						detail::diff_step_meter& aMeter, engine_progress& aProgress) -> typename TComparison::outcome
		{
			auto sides = std::array<prepared_document, 2U>{ };
			sides[0U].Document.Bytes.assign(aInput.LeftBytes.value_or(std::span<const std::uint8_t>{ }).begin(),
											aInput.LeftBytes.value_or(std::span<const std::uint8_t>{ }).end());
			sides[1U].Document.Bytes.assign(aInput.RightBytes.value_or(std::span<const std::uint8_t>{ }).begin(),
											aInput.RightBytes.value_or(std::span<const std::uint8_t>{ }).end());
			auto& left  = sides[0U];
			auto& right = sides[1U];

			// Prepare: the binary and UTF-8 scans, the line split and the
			// normalization of each side.
			for (const auto& side : sides) {
				const auto bytes = std::span<const std::uint8_t>{ side.Document.Bytes };
				for (std::size_t position = 0U; position < bytes.size();) {
					const auto piece = bytes.subspan(position, std::min(scan_chunk_bytes, bytes.size() - position));
					if (std::ranges::find(piece, std::uint8_t{ 0U }) != piece.end()) {
						return nontext_result<TComparison>(
							aInput, diff_content_state::binary, diff_reason_code::binary_file);
					}
					position += piece.size();
					aMeter.tick(piece.size() / 16U);
				}
			}
			for (const auto& side : sides) {
				const auto bytes = std::span<const std::uint8_t>{ side.Document.Bytes };
				for (std::size_t position = 0U; position < bytes.size();) {
					const auto end = utf8_piece_end(bytes, position);
					if (!valid_utf8(bytes.subspan(position, end - position))) {
						return nontext_result<TComparison>(aInput,
														   diff_content_state::unavailable,
														   diff_reason_code::decode_failure);
					}
					aMeter.tick(end - position);
					position = end;
				}
			}
			for (auto& side : sides) {
				if (!split_lines(side.Document, static_cast<std::size_t>(aInput.Limits.MaximumLinesPerSide), aMeter)) {
					return nontext_result<TComparison>(aInput,
													   diff_content_state::unavailable,
													   diff_reason_code::materialization_limit);
				}
			}
			for (auto& side : sides) {
				side.Normalized.reserve(side.Document.Lines.size());
				for (const auto& line : side.Document.Lines) {
					side.Normalized.push_back(normalize_line(side.Document, line, aInput.Options));
					aMeter.tick(normalize_steps(line));
				}
			}

			// Primary: [[BPR-1485]] the line matcher's matches, checked against
			// the engine's line classes, then any post-pass.
			aProgress.begin(diff_compute_phase::primary, aMeter);
			const auto parts = aComponents.parts();
			if (!parts.Primary) {
				throw declined_primary{ };
			}
			const auto classes = assign_line_classes(sides, aMeter);
			const auto request = diff_primary_request{
				.Left         = &left.Document,
				.Right        = &right.Document,
				.LeftClasses  = classes.Left,
				.RightClasses = classes.Right,
				.Options      = aInput.Options,
			};
			auto       componentMeter = component_meter{ aMeter };
			const auto supplied       = aComponents.primary_matches(request, componentMeter);
			if (!supplied.has_value()) {
				throw declined_primary{ };
			}
			auto matches = checked_matches(classes, *supplied);
			if (parts.Slide) {
				auto sorted = std::vector<diff_match>{ };
				sorted.reserve(matches.size());
				for (const auto& [leftLine, rightLine] : matches) {
					sorted.push_back({ .Left = static_cast<std::uint32_t>(leftLine), .Right = static_cast<std::uint32_t>(rightLine) });
				}
				const auto slid = aComponents.slide_matches(request, sorted, componentMeter);
				if (slid.has_value()) {
					matches = checked_matches(classes, *slid);
				}
			}
			aMeter.tick(matches.size());
			const auto primary = primary_shapes(matches,
												static_cast<std::uint32_t>(left.Normalized.size()),
												static_cast<std::uint32_t>(right.Normalized.size()),
												aMeter);
			matches            = { };

			// Refine: secondary pairing of each replacement.
			aProgress.begin(diff_compute_phase::refine, aMeter);
			auto        budget      = matrix_budget{ .Limit = aInput.Limits.MaximumMatrixCells };
			auto        approximate = false;
			auto        refined     = std::vector<pending_segment>{ };
			auto* const pairing     = parts.Pairing ? &aComponents : nullptr;
			for (const auto& segment : primary) {
				if (segment.Kind != alignment_kind::replacement) {
					refined.push_back(segment);
					aMeter.tick(1U);
					continue;
				}
				auto       attempt = budget;
				const auto pairs   = secondary_pairs(left, right, segment.Left, segment.Right, aInput.Options, attempt, aMeter, pairing);
				budget             = attempt;
				if (!pairs.has_value()) {
					// [[BPR-0719]]: an unpairable replacement stays a plain
					// replacement instead of refusing the file.
					approximate = true;
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
			if (refined.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) || refined.size() > static_cast<std::size_t>(aInput.Limits.MaximumSegments)) {
				return nontext_result<TComparison>(aInput,
												   diff_content_state::unavailable,
												   diff_reason_code::compute_limit);
			}

			// Moves.
			aProgress.begin(diff_compute_phase::moves, aMeter);
			auto moved = aInput.Options.Moves == move_detection_policy::deterministic_exact
				? refine_moved_subblocks(refined,
										 left,
										 right,
										 aInput.Limits.MaximumMoveDetectionWork,
										 aInput.Limits.MaximumSegments,
										 aMeter)
				: moved_refinement{ .Segments = std::move(refined) };
			refined    = { };

			// Assemble: groups, segments, pairs and their intraline edits, and
			// move relations.
			aProgress.begin(diff_compute_phase::assemble, aMeter);
			const auto  digest        = comparison_digest(aInput.Key);
			const auto& pending       = moved.Segments;
			auto        segmentGroups = std::vector<std::optional<change_group_id>>(pending.size(), std::nullopt);
			auto        groupOrdinal  = std::uint32_t{ 0U };
			auto        groups        = std::vector<change_group>{ };
			for (std::size_t cursor = 0U; cursor < pending.size();) {
				if (pending[cursor].Kind == alignment_kind::equal) {
					++cursor;
					aMeter.tick(1U);
					continue;
				}
				const auto begin = cursor;
				auto       end   = begin;
				while (end < pending.size() && pending[end].Kind != alignment_kind::equal) {
					++end;
				}
				const auto& first         = pending[begin];
				const auto& last          = pending[end - 1U];
				const auto  leftSpan      = line_span{ first.Left.Begin, last.Left.End };
				const auto  rightSpan     = line_span{ first.Right.Begin, last.Right.End };
				const auto  id            = group_identity(digest, leftSpan, rightSpan, groupOrdinal++);
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
				groups.push_back(change_group{
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
					segmentGroups[member] = id;
				}
				cursor = end;
				aMeter.tick(detail::validation_record_steps + (end - begin));
			}
			auto segments = std::vector<alignment_segment>{ };
			segments.reserve(pending.size());
			for (std::size_t cursor = 0U; cursor < pending.size(); ++cursor) {
				const auto& source = pending[cursor];
				segments.push_back(alignment_segment{
					.Id    = segment_identity(digest,
											  source.Kind,
											  source.Left,
											  source.Right,
											  static_cast<std::uint32_t>(cursor)),
					.Kind  = source.Kind,
					.Left  = source.Left,
					.Right = source.Right,
					.Group = segmentGroups[cursor],
				});
				aMeter.tick(detail::validation_record_steps);
			}
			auto        pairs     = std::vector<line_pair>{ };
			auto        intraline = std::vector<intraline_edit>{ };
			auto* const scalars   = parts.Intraline ? &aComponents : nullptr;
			for (std::size_t cursor = 0U; cursor < pending.size(); ++cursor) {
				const auto& source = pending[cursor];
				if (!source.PairKind.has_value()) {
					aMeter.tick(1U);
					continue;
				}
				const auto ordinal = static_cast<std::uint32_t>(pairs.size());
				const auto pair    = line_pair{
					.Id        = pair_identity(digest,
											   segments[cursor].Id,
											   source.Left.Begin,
											   source.Right.Begin,
											   *source.PairKind,
											   ordinal),
					.Segment   = segments[cursor].Id,
					.LeftLine  = source.Left.Begin,
					.RightLine = source.Right.Begin,
					.Kind      = *source.PairKind,
				};
				pairs.push_back(pair);
				if (pair.Kind == line_pair_kind::similar) {
					auto       attempt = budget;
					const auto ranges  = intraline_ranges(left.Document, right.Document, pair.LeftLine, pair.RightLine, attempt, aMeter, scalars);
					budget             = attempt;
					if (!ranges.has_value()) {
						// [[BPR-0719]]: the pair stays similar without intraline
						// edits instead of refusing the file.
						approximate = true;
					} else {
						for (const auto& [leftRange, rightRange] : *ranges) {
							intraline.push_back(intraline_edit{
								.Pair  = pair.Id,
								.Left  = leftRange,
								.Right = rightRange,
							});
						}
					}
				}
				aMeter.tick(detail::validation_record_steps);
			}
			auto relations = std::vector<move_relation>{ };
			for (const auto& [source, destination] : moved.Moves) {
				const auto ordinal = static_cast<std::uint32_t>(relations.size());
				relations.push_back(move_relation{
					.Id                   = move_identity(digest, segments[source].Id, segments[destination].Id, ordinal),
					.SourceDeletion       = segments[source].Id,
					.DestinationInsertion = segments[destination].Id,
				});
				aMeter.tick(detail::validation_record_steps);
			}
			auto alignment = typename TComparison::alignment{
				.Key            = aInput.Key,
				.Options        = aInput.Options,
				.Digest         = digest,
				.LeftLineCount  = static_cast<std::uint32_t>(left.Document.Lines.size()),
				.RightLineCount = static_cast<std::uint32_t>(right.Document.Lines.size()),
			};
			alignment.Segments = std::move(segments);
			alignment.Pairs    = std::move(pairs);
			alignment.Groups   = std::move(groups);
			alignment.Moves    = std::move(relations);
			auto result        = typename TComparison::computed{ };
			result.Alignment   = std::move(alignment);
			if (left_present(aInput.Key)) {
				result.LeftDocument = std::move(left.Document);
			}
			if (right_present(aInput.Key)) {
				result.RightDocument = std::move(right.Document);
			}
			result.Intraline                        = std::move(intraline);
			result.MatrixCellsConsumed              = budget.Consumed;
			result.MoveDetectionWorkConsumed        = moved.WorkConsumed;
			result.MoveDetectionWorkLimitReached    = moved.WorkLimitReached;
			result.MoveDetectionSegmentLimitReached = moved.SegmentLimitReached;
			result.AlignmentApproximate             = approximate;
			sides                                   = { };
			moved                                   = { };

			// Validate.
			aProgress.begin(diff_compute_phase::validate, aMeter);
			if (!detail::validate_alignment(result.Alignment, aMeter).valid()) {
				return { .Reason = diff_reason_code::invalid_alignment };
			}
			if (!validate_computed_records(result, aMeter).valid()) {
				return { .Reason = diff_reason_code::invalid_alignment };
			}
			return { .Reason = diff_reason_code::none, .Result = std::move(result) };
		}

		/// One computation, which aControl may stop.
		template <typename TComparison, typename TRun>
		auto compute_run(const typename TComparison::input& aInput, const diff_compute_control& aControl,
						 diff_components& aComponents) -> TRun
		{
			auto run = TRun{ };
			if (auto finished = checked_start<TComparison>(aInput); finished.has_value()) {
				run.Outcome          = std::move(*finished);
				run.Statistics.Phase = diff_compute_phase::finished;
				return run;
			}
			auto meter    = detail::diff_step_meter{ aControl.Stop, aControl.CheckInterval };
			auto progress = engine_progress{ };
			try {
				run.Outcome = run_engine<TComparison>(aInput, aComponents, meter, progress);
			} catch (const detail::diff_compute_stopped&) {
				// [[BPR-1485]] A stop abandons the computation.
				run.Outcome.reset();
			} catch (const invalid_component_result&) {
				// [[BPR-1465]] A component supplied what the engine's rules
				// refuse: the comparison is refused, never published.
				run.Outcome = typename TComparison::outcome{ .Reason = diff_reason_code::invalid_alignment };
			} catch (const declined_primary&) {
				run.Outcome = typename TComparison::outcome{ .Reason = diff_reason_code::host_failure };
			}
			run.Statistics.Steps            = meter.steps();
			run.Statistics.LargestPollDelay = meter.largest_poll_delay();
			run.Statistics.Phase            = run.Outcome.has_value() ? diff_compute_phase::finished : progress.Phase;
			run.Statistics.PhaseStarts      = progress.PhaseStarts;
			return run;
		}
	} // namespace

	auto compute_file_diff(const diff_compute_input& aInput, diff_components& aComponents) -> diff_compute_outcome
	{
		return *compute_run<pull_request_comparison, file_diff_compute_run>(aInput, { }, aComponents).Outcome;
	}

	auto compute_file_diff(const diff_compute_input& aInput, const diff_compute_control& aControl,
						   diff_components& aComponents) -> file_diff_compute_run
	{
		return compute_run<pull_request_comparison, file_diff_compute_run>(aInput, aControl, aComponents);
	}

	auto compute_material_diff(const material_diff_input& aInput, diff_components& aComponents) -> material_diff_outcome
	{
		return *compute_run<material_comparison, material_diff_compute_run>(aInput, { }, aComponents).Outcome;
	}

	auto compute_material_diff(const material_diff_input& aInput, const diff_compute_control& aControl,
							   diff_components& aComponents) -> material_diff_compute_run
	{
		return compute_run<material_comparison, material_diff_compute_run>(aInput, aControl, aComponents);
	}

	// [[BPR-1465]] The component seam.

	auto diff_components::parts() const noexcept -> diff_component_parts
	{
		return { };
	}

	auto diff_components::primary_matches(const diff_primary_request& /*aRequest*/, diff_component_meter& /*aMeter*/)
		-> std::optional<std::vector<diff_match>>
	{
		return std::nullopt;
	}

	auto diff_components::slide_matches(const diff_primary_request& /*aRequest*/, std::span<const diff_match> /*aMatches*/,
										diff_component_meter& /*aMeter*/) -> std::optional<std::vector<diff_match>>
	{
		return std::nullopt;
	}

	auto diff_components::pair_scores(const diff_pairing_request& /*aRequest*/, diff_component_meter& /*aMeter*/)
		-> std::optional<std::vector<std::uint16_t>>
	{
		return std::nullopt;
	}

	auto diff_components::intraline_matches(std::span<const std::uint32_t> /*aLeft*/, std::span<const std::uint32_t> /*aRight*/,
											diff_component_meter& /*aMeter*/) -> std::optional<std::vector<diff_match>>
	{
		return std::nullopt;
	}

	namespace {
		/// [[BPR-1465]] Observed primary matches, offered as a component.
		class observed_primary final : public diff_components
		{
		public:
			explicit observed_primary(const diff_primary_observation& aObservation) noexcept
				: Observation{ &aObservation }
			{
			}

			[[nodiscard]] auto parts() const noexcept -> diff_component_parts override
			{
				return { .Primary = true };
			}

			[[nodiscard]] auto primary_matches(const diff_primary_request& /*aRequest*/, diff_component_meter& aMeter)
				-> std::optional<std::vector<diff_match>> override
			{
				aMeter.tick(Observation->Matches.size());
				return Observation->Matches;
			}

		private:
			const diff_primary_observation* Observation = nullptr;
		};
	} // namespace

	auto compute_file_diff(const diff_compute_input& aInput, const diff_primary_observation& aObservation)
		-> diff_compute_outcome
	{
		auto components = observed_primary{ aObservation };
		return compute_file_diff(aInput, components);
	}

	auto compute_material_diff(const material_diff_input& aInput, const diff_primary_observation& aObservation)
		-> material_diff_outcome
	{
		auto components = observed_primary{ aObservation };
		return compute_material_diff(aInput, components);
	}

} // namespace bpr::core
