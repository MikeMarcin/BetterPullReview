#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_GENERATED_FILES_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_GENERATED_FILES_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/core/review_cycle_ignore.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// RFC-BPR-0089 M2. Which files of a pull request are generated, and so are
/// set aside behind their own row.
///
/// Generated status belongs to the path, not to a commit: it holds under
/// every commit filter and in Whole PR. The first of these rules that says yes
/// or no decides it:
///
/// 1. The reviewer's mark for this review, kept in the ledger: Show as normal
///    or Treat as generated.
/// 2. `linguist-generated` in the `.gitattributes` files of the pull
///    request's head, as GitHub's Files changed reads them (measured on
///    BPR-Test, 2026-10-21): set, unset, `=false`, or unspecified.
///
/// Nothing else marks a file generated: no size, content or Linguist built-in
/// rule (owner, Q4). Review anyway returns a generated file to the review
/// without changing what it is. None of this is the ledger's Excluded.
///
/// The core decides as pure functions. Pattern matching sits behind
/// `generated_attribute_rules`; the host reads the attribute files at head.
namespace bpr::core {
	/// Marks one review may keep: one per file the review lists, at most.
	inline constexpr std::size_t maximum_generated_file_marks = 262'144U;
	/// `.gitattributes` files read at head for one review.
	inline constexpr std::size_t maximum_generated_attribute_files = 64U;
	/// Bytes of one `.gitattributes` file read at head.
	inline constexpr std::size_t maximum_generated_attribute_file_bytes = 512U * 1024U;

	/// \brief The reviewer's statement about one file.
	enum class generated_mark : std::uint8_t {
		/// The attributes decide.
		none,
		/// Show as normal: not generated, whatever the attributes say.
		show_as_normal,
		/// Treat as generated: generated, whatever the attributes say.
		treat_as_generated,
	};

	/// \brief One file's mark in one review.
	///
	/// \details `ReviewAnyway` returns a generated file to the review: it is
	/// still generated, and says so, but it is listed, walked and counted.
	struct generated_file_mark
	{
		file_identity  File;
		generated_mark Mark         = generated_mark::none;
		bool           ReviewAnyway = false;

		auto operator<=>(const generated_file_mark&) const = default;
	};

	/// \brief Every mark of one review, sorted by file and unique.
	///
	/// \details A mark that says nothing (no mark and not reviewed anyway) is
	/// never kept: it is the same as no entry.
	struct generated_file_marks
	{
		std::vector<generated_file_mark> Marks;

		auto operator<=>(const generated_file_marks&) const = default;
	};

	/// \brief Checks the marks' shape: valid files, sorted, unique, bounded,
	/// and each entry says something.
	[[nodiscard]] auto valid(const generated_file_marks& aMarks) noexcept -> bool;

	/// \brief The mark for one file, or none.
	[[nodiscard]] auto find_generated_mark(const generated_file_marks& aMarks, const file_identity& aFile) noexcept
		-> const generated_file_mark*;

	/// \brief A gesture from a file's context menu.
	enum class generated_gesture : std::uint8_t {
		/// Show as normal: the file is not generated in this review.
		show_as_normal,
		/// Treat as generated: the file is generated in this review.
		treat_as_generated,
		/// Review anyway: a generated file returns to the review.
		review_anyway,
		/// Set aside again: a file reviewed anyway goes back behind the row.
		set_aside_again,
		/// Follow .gitattributes: forgets the reviewer's mark and Review anyway.
		follow_attributes,
	};

	enum class generated_mark_refusal : std::uint8_t {
		/// The file's identity is not valid.
		invalid_file,
		/// The marks are malformed.
		invalid_state,
		/// The review already keeps the most marks it may.
		too_many_marks,
	};

	/// \brief Applies one gesture to one file's mark.
	///
	/// \details Show as normal and Treat as generated replace the mark and
	/// end Review anyway, since the reviewer has said what the file is. The
	/// result is normalized: an entry that says nothing is removed, and a
	/// gesture that changes nothing returns the marks unchanged.
	[[nodiscard]] auto apply_generated_gesture(const generated_file_marks& aMarks, const file_identity& aFile,
											   generated_gesture aGesture) -> std::expected<generated_file_marks, generated_mark_refusal>;

	/// \brief Why a file is, or is not, generated.
	enum class generated_reason : std::uint8_t {
		/// No mark and no attribute: an ordinary file.
		unspecified,
		/// `linguist-generated` is set, or set to a value other than `false`
		/// (Linguist treats any such value as true).
		attribute_set,
		/// `-linguist-generated`.
		attribute_unset,
		/// `linguist-generated=false`.
		attribute_false,
		/// The reviewer chose Show as normal.
		marked_normal,
		/// The reviewer chose Treat as generated.
		marked_generated,
	};

	/// \brief One file's generated decision.
	///
	/// \details `SetAside` is what the review does with it: a generated file
	/// leaves the file set, walk, counts and preparation, unless the reviewer
	/// chose Review anyway.
	struct generated_decision
	{
		bool             Generated    = false;
		bool             ReviewAnyway = false;
		bool             SetAside     = false;
		generated_reason Reason       = generated_reason::unspecified;

		auto operator<=>(const generated_decision&) const = default;
	};

	/// \brief Decides one file from its mark and its `linguist-generated`
	/// attribute at head.
	[[nodiscard]] auto decide_generated(const generated_file_mark* aMark, const git_attribute_state& aAttribute) noexcept
		-> generated_decision;

	/// \brief Whether a gesture would change the file, for menu enablement.
	[[nodiscard]] auto generated_gesture_available(const generated_decision& aDecision, const generated_file_mark* aMark,
												   generated_gesture aGesture) noexcept -> bool;

	/// \brief The matcher boundary: `linguist-generated` for one path, from
	/// the `.gitattributes` files of the pull request's head.
	///
	/// \details Implementations apply Git's attribute precedence (a deeper
	/// file outranks its parents, a later line outranks an earlier one) and
	/// its pattern rules. They are pure: equal attribute files and paths give
	/// equal answers.
	class generated_attribute_rules
	{
	public:
		generated_attribute_rules()                                                    = default;
		generated_attribute_rules(const generated_attribute_rules&)                    = delete;
		auto operator=(const generated_attribute_rules&) -> generated_attribute_rules& = delete;
		generated_attribute_rules(generated_attribute_rules&&)                         = delete;
		auto operator=(generated_attribute_rules&&) -> generated_attribute_rules&      = delete;
		virtual ~generated_attribute_rules()                                           = default;

		/// \brief The attribute's state for one repository path.
		[[nodiscard]] virtual auto linguist_generated(std::string_view aPath) const -> git_attribute_state = 0;
	};

	/// \brief Decides every file of a review, in `aPaths` order.
	///
	/// \details `aRules` is null while the head's attributes are not read, or
	/// could not be: every attribute is then unspecified and only the
	/// reviewer's marks decide, so nothing is set aside before a rule or a
	/// gesture says so.
	[[nodiscard]] auto decide_generated_files(std::span<const std::string> aPaths, const generated_file_marks& aMarks,
											  const generated_attribute_rules* aRules) -> std::vector<generated_decision>;

	/// \brief The directories whose `.gitattributes` can apply to `aPaths`: the
	/// root (empty) and every ancestor directory, parents before children.
	///
	/// \details The host looks for a `.gitattributes` file in each one at head
	/// and reads at most `maximum_generated_attribute_files` of them.
	[[nodiscard]] auto generated_attribute_directories(std::span<const std::string> aPaths) -> std::vector<std::string>;

	/// \brief One `.gitattributes` file the head holds: the root-relative
	/// directory it sits in (empty for the root) and its bytes.
	struct generated_attribute_file
	{
		std::string Directory;
		std::string Contents;

		auto operator<=>(const generated_attribute_file&) const = default;
	};

	/// \brief Why the head's `.gitattributes` files give no rules.
	enum class generated_attributes_refusal : std::uint8_t {
		/// More than `maximum_generated_attribute_files` files.
		too_many_files,
		/// One file is larger than `maximum_generated_attribute_file_bytes`.
		file_too_large,
		/// A directory is not a normalized repository path, or the files are
		/// not sorted by directory and unique.
		invalid_files,
		/// The files hold more rules than the reader keeps.
		too_many_rules,
	};

	/// \brief The head's `linguist-generated` rules, read with the core's
	/// first-party attribute reader (`workspace_attribute_rules`).
	///
	/// \details Owner, 2026-10-21 (attended session `e081a94e`, Accept): the
	/// existing wildmatch and attribute reader, kept by RFC-BPR-0083's decision
	/// record, extended by this one attribute; no library. Git's precedence and
	/// pattern rules apply: a deeper file outranks its parents, a later line an
	/// earlier one, only the root file defines macros, and a directory pattern
	/// never marks a file. Matching is case-sensitive, as on GitHub's servers.
	/// There is no `info/attributes`: GitHub reads only the committed files.
	[[nodiscard]] auto read_generated_attribute_rules(std::span<const generated_attribute_file> aFiles)
		-> std::expected<std::shared_ptr<const generated_attribute_rules>, generated_attributes_refusal>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_GENERATED_FILES_HPP
