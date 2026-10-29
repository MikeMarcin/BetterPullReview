#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_MARKDOWN_BLOB_SNIPPET_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_MARKDOWN_BLOB_SNIPPET_HPP

#include "bpr/core/markdown_navigation.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1802]]. A blob line permalink in the pull request's own
/// repository shown as GitHub shows it: a box headed `repo/path`, "Lines 10
/// to 20 in 1a2b3c4", and the lines themselves, numbered.
/// \details GitHub's rule, read from its Markdown API in `gfm` mode with a
/// repository context on BPR-Test (2026-10-28): a bare or `<...>` permalink
/// with a full commit and a line anchor, in the context repository on the
/// same host, becomes the box; another repository's permalink, an authored
/// `[text](permalink)`, a blob address without a line anchor and a range past
/// the file's end stay plain links. Eighty lines were all shown. This unit
/// decides which permalinks of a body stand where a box can go, which lines a
/// file's bytes give a range, the header's words, and the Markdown a reader
/// imports in their place. Reading the file is the host's.
namespace bpr::core {
	/// The most lines one box shows; a longer range keeps the compact link.
	/// GitHub showed all of an 80-line range; it states no bound of its own.
	inline constexpr std::uint32_t markdown_blob_snippet_maximum_lines = 100U;
	/// The largest file a snippet's lines are read from.
	inline constexpr std::size_t markdown_blob_snippet_file_bytes = 1024U * 1024U;
	/// The most bytes of lines one box shows.
	inline constexpr std::size_t markdown_blob_snippet_bytes = 64U * 1024U;
	/// The most permalinks of one body that become boxes, in document order.
	inline constexpr std::size_t markdown_blob_snippets_per_body = 8U;

	/// \brief The pull request's repository, which a reader's permalinks are
	/// judged by: only its own become boxes and lose their owner.
	struct markdown_repository
	{
		std::string Host;
		std::string Owner;
		std::string Repository;

		auto operator==(const markdown_repository&) const -> bool = default;
	};

	/// \brief Whether `aPermalink` is in `aRepository` on its host.
	[[nodiscard]] auto markdown_blob_permalink_in_repository(const markdown_blob_permalink& aPermalink, const markdown_repository& aRepository) -> bool;

	/// \brief A permalink alone on a line of a body, where a box can stand.
	struct markdown_standalone_permalink
	{
		/// The line's bytes, without its line ending.
		std::size_t Begin = 0U;
		std::size_t End   = 0U;
		/// The address as written, without `<` and `>`.
		std::string             Address;
		markdown_blob_permalink Permalink;

		auto operator==(const markdown_standalone_permalink&) const -> bool = default;
	};

	/// \brief The permalinks of `aBody` that stand alone on a top-level line.
	/// \details A line holding only a bare or `<...>` permalink, at its first
	/// column (trailing spaces allowed), outside fenced code and HTML blocks,
	/// and not underlined as a heading. A permalink inside a sentence, a list
	/// item or a quote keeps the compact link. At most
	/// `markdown_blob_snippets_per_body`; none when the body holds
	/// `markdown_blob_snippet_mark`, so no authored fence can pose as a box.
	[[nodiscard]] auto markdown_standalone_permalinks(std::string_view aBody) -> std::vector<markdown_standalone_permalink>;

	/// \brief Whether a permalink's range is one a box may show.
	[[nodiscard]] auto markdown_blob_snippet_range_shown(const markdown_blob_permalink& aPermalink) -> bool;

	/// \brief The lines `aPermalink` names in a file's bytes.
	/// \details Lines split at LF, a CR before it dropped; a final line without
	/// one counts, a final line ending adds none. Nothing for a file over
	/// `markdown_blob_snippet_file_bytes`, holding a NUL, ending before the
	/// range's last line, a range over the bounds, or lines that are not
	/// well-formed UTF-8.
	[[nodiscard]] auto markdown_blob_snippet_lines(std::string_view aFile, const markdown_blob_permalink& aPermalink) -> std::optional<std::vector<std::string>>;

	/// \brief The key a file's bytes are held under: host, owner and
	/// repository in lower case, the commit and the path.
	[[nodiscard]] auto markdown_blob_permalink_source_key(const markdown_blob_permalink& aPermalink) -> std::string;
	/// \brief Whether two permalinks name the same lines of the same file.
	[[nodiscard]] auto markdown_blob_permalink_same_lines(const markdown_blob_permalink& aLeft, const markdown_blob_permalink& aRight) -> bool;

	/// \brief The box's title, GitHub's header link: `repo/path`.
	[[nodiscard]] auto markdown_blob_snippet_title(const markdown_blob_permalink& aPermalink) -> std::string;
	/// \brief "Line 7" or "Lines 7 to 9".
	[[nodiscard]] auto markdown_blob_snippet_range(const markdown_blob_permalink& aPermalink) -> std::string;
	/// \brief The commit's first seven digits, as GitHub abbreviates it.
	[[nodiscard]] auto markdown_blob_snippet_commit(const markdown_blob_permalink& aPermalink) -> std::string;
	/// \brief The commit's page on the permalink's own scheme and host:
	/// `https://host/owner/repo/commit/<sha>`; empty when `aAddress` is not a
	/// permalink. Opening it still goes through `admit_markdown_link`.
	[[nodiscard]] auto markdown_blob_permalink_commit_address(std::string_view aAddress) -> std::string;

	/// \brief [[BPR-1802]]. The compact label, without the owner when the
	/// permalink is in the reader's own repository, as GitHub's header drops
	/// it: `repo/path#L10-L20 at 1a2b3c4`.
	[[nodiscard]] auto markdown_blob_permalink_label(const markdown_blob_permalink& aPermalink, bool aOwnRepository) -> std::string;

	/// The private-use character (U+E0E1) a box's fence info starts with,
	/// followed by its place in the reader's list. A body that holds it shows
	/// no box.
	inline constexpr std::string_view markdown_blob_snippet_mark = "\xEE\x83\xA1";

	/// \brief One box to put in place of its permalink's line.
	struct markdown_blob_snippet_placement
	{
		/// The line, as `markdown_standalone_permalink` found it.
		std::size_t Begin = 0U;
		std::size_t End   = 0U;
		/// The box's place in the reader's list, written after the mark.
		std::uint32_t                Ordinal = 0U;
		std::span<const std::string> Lines;
	};

	/// \brief `aBody` with each placement's line replaced by a fenced block
	/// of its lines whose info is the mark and its ordinal, set apart by
	/// blank lines, so a paragraph around it is split as GitHub splits it.
	/// \details The fence is longer than any run of backticks in the lines.
	/// Placements must be in order and not overlap; an invalid one is skipped.
	[[nodiscard]] auto markdown_with_blob_snippets(std::string_view aBody, std::span<const markdown_blob_snippet_placement> aPlacements) -> std::string;

	/// \brief The ordinal a fence info names, when it is a box's.
	[[nodiscard]] auto markdown_blob_snippet_ordinal(std::string_view aInfo) -> std::optional<std::uint32_t>;
} // namespace bpr::core

#endif
