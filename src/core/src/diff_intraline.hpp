#ifndef BPR_SRC_CORE_SRC_DIFF_INTRALINE_HPP
#define BPR_SRC_CORE_SRC_DIFF_INTRALINE_HPP

#include "diff_step_meter.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

// [[BPR-1481]] Private to the core's diff sources: the intraline ranges of one
// similar line pair, from dtl's O(NP) comparison (RFC-BPR-0082). dtl is used
// for this per-line comparison only, never as a whole-file line matcher.
//
// [[BPR-1537]] dtl compares the pair's tokens, not its scalars: a word part
// (a lowercase run, a capitalised word, an uppercase acronym, a digit run), a
// connector such as `_`, a run of spaces and tabs, and any other character on
// its own, a wide (East Asian) character or a punctuation or symbol cluster.
// The edit script is then cleaned up so that a reader sees changed words, not
// changed letters: a short equality between two changes joins them, a
// change inside one identifier covers the identifier, and a pair that keeps
// little in common is emphasised from its first change to its last.
namespace bpr::core::detail {
	/// \brief The largest (left + 1) x (right + 1) token grid dtl compares.
	/// dtl records one path point per snake, at most (left + 1) x (right + 1)
	/// for a whole comparison, and past 2,000,000 points it restarts on a path
	/// that can join unequal tokens. A grid below that bound never restarts.
	inline constexpr std::uint64_t intraline_dtl_cells_limit = 1'999'999U;

	/// \brief An emphasis whose unchanged words keep less than this share, in
	/// permille, of the shorter line's non-space scalars is drawn as one
	/// change from the first changed token to the last.
	inline constexpr std::uint64_t intraline_rewrite_permille = 400U;

	/// \brief How a token was formed.
	enum class intraline_token_kind : std::uint8_t {
		/// A run of spaces and tabs (and other space separators).
		space,
		/// A word part: a lowercase run, a capitalised word, an uppercase run
		/// or a digit run. Non-ASCII letters, marks and numbers are lowercase.
		word,
		/// `_` or another connector punctuation, one scalar a token.
		connector,
		/// One wide (East Asian) character, or one punctuation, symbol or
		/// control cluster.
		other,
	};

	/// \brief One token of a line: bytes [Begin, End) of the line's content,
	/// `Scalars` scalars long.
	struct intraline_token
	{
		std::uint64_t        Begin   = 0U;
		std::uint64_t        End     = 0U;
		std::uint64_t        Scalars = 0U;
		intraline_token_kind Kind    = intraline_token_kind::other;
	};

	/// \brief The tokens of one line's content, which must be well-formed
	/// UTF-8. Every token boundary is a scalar boundary, and a grapheme
	/// cluster is never split except inside a word.
	[[nodiscard]] auto intraline_tokens(std::string_view aLine) -> std::vector<intraline_token>;

	/// \brief One similar pair prepared for comparison: both lines' tokens,
	/// each token's value (equal tokens have equal values), and the common
	/// prefix and suffix, which are matched without dtl.
	struct intraline_pair
	{
		std::string_view             Left;
		std::string_view             Right;
		std::vector<intraline_token> LeftTokens;
		std::vector<intraline_token> RightTokens;
		std::vector<std::uint32_t>   LeftValues;
		std::vector<std::uint32_t>   RightValues;
		std::size_t                  Prefix = 0U;
		std::size_t                  Suffix = 0U;

		/// \brief The values between the common prefix and suffix.
		[[nodiscard]] auto left_middle() const noexcept -> std::span<const std::uint32_t>
		{
			return std::span<const std::uint32_t>{ LeftValues }.subspan(Prefix, LeftValues.size() - Prefix - Suffix);
		}

		[[nodiscard]] auto right_middle() const noexcept -> std::span<const std::uint32_t>
		{
			return std::span<const std::uint32_t>{ RightValues }.subspan(Prefix, RightValues.size() - Prefix - Suffix);
		}
	};

	/// \brief Tokenises both lines and numbers their tokens. Each byte and
	/// token is a step of `aMeter`.
	[[nodiscard]] auto prepare_intraline_pair(std::string_view aLeft, std::string_view aRight, diff_step_meter& aMeter)
		-> intraline_pair;

	/// \brief The strictly increasing (left, right) indices of the elements
	/// dtl finds common to `aLeft` and `aRight`. Nothing when the pair's grid
	/// is larger than `intraline_dtl_cells_limit`, or when dtl's script breaks
	/// the engine's match rules; the caller then compares the pair with the
	/// matrix budget's table.
	///
	/// Each element comparison is a step of `aMeter`, and the elements
	/// recorded after it are steps too. A stop is honoured within 4,096
	/// comparisons: dtl is finished quickly, its result dropped, and the stop
	/// thrown again, so no stop unwinds through dtl's own allocations.
	[[nodiscard]] auto intraline_common_elements(std::span<const std::uint32_t> aLeft,
												 std::span<const std::uint32_t> aRight,
												 diff_step_meter&               aMeter)
		-> std::optional<std::vector<std::pair<std::size_t, std::size_t>>>;

	/// \brief One changed region of a pair: left bytes [LeftBegin, LeftEnd)
	/// became right bytes [RightBegin, RightEnd). Either side may be empty.
	struct intraline_region
	{
		std::uint64_t LeftBegin  = 0U;
		std::uint64_t LeftEnd    = 0U;
		std::uint64_t RightBegin = 0U;
		std::uint64_t RightEnd   = 0U;

		auto operator==(const intraline_region&) const -> bool = default;
	};

	/// \brief The emphasis of a prepared pair, given the strictly increasing
	/// common token indices of its middle (relative to the middle, as
	/// `intraline_common_elements` returns them), in line order, after the
	/// cleanup described above. Linear in the tokens; each is a step.
	[[nodiscard]] auto intraline_regions(const intraline_pair&                                aPair,
										 std::span<const std::pair<std::size_t, std::size_t>> aMiddleMatches,
										 diff_step_meter&                                     aMeter)
		-> std::vector<intraline_region>;
} // namespace bpr::core::detail

#endif // BPR_SRC_CORE_SRC_DIFF_INTRALINE_HPP
