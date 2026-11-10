#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REFERENCE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REFERENCE_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-1998]], RFC-BPR-0101. One place a text names a file and
	/// its lines, as written.
	///
	/// \details Offsets are UTF-8 bytes into the text it was found in. `Path`
	/// is the path as written (empty for a bare "line 12", which names the
	/// question's or the stop's one file). Lines are one-based, first and
	/// last; `Before` names the change's base ("before the change").
	struct agent_reference
	{
		std::size_t   Begin  = 0U;
		std::size_t   Length = 0U;
		std::string   Path;
		std::uint32_t First  = 0U;
		std::uint32_t Last   = 0U;
		bool          Before = false;

		auto operator<=>(const agent_reference&) const = default;
	};

	/// \brief Every reference the RFC's forms make in `aText`, in order.
	///
	/// \details The forms: `path:12`, `path:12-34`, `path:12:5` (the column is
	/// ignored), `path#L12`, `path#L12-L34`, `path line 12`, `path lines
	/// 12–34`, `path (lines 12–34)`, each optionally followed by "before the
	/// change", with a hyphen, an en dash or "to" between the numbers; and,
	/// with `aBare`, `line 12` and `lines 12–34` alone. A path is words of
	/// letters, digits, `_`, `.`, `-` and `+` between slashes, the last with
	/// an extension. This is a closed recogniser for those forms and nothing
	/// else: it is not a general pattern engine.
	[[nodiscard]] auto find_agent_references(std::string_view aText, bool aBare) -> std::vector<agent_reference>;

	/// \brief The files a reference may name.
	///
	/// \details `Changed` are the pull request's changed files; `Head` any
	/// file of the repository at the head, when known. `UnchangedOpenable`
	/// says whether the application can show an unchanged file at a line;
	/// when it cannot, a reference that resolves only to one is not a link.
	struct agent_reference_files
	{
		std::vector<std::string> Changed;
		std::vector<std::string> Head;
		bool                     UnchangedOpenable = false;

		auto operator<=>(const agent_reference_files&) const = default;
	};

	/// \brief What a written path resolves to: the files it may open (one
	/// opens; several are offered to choose from; none is not a link).
	///
	/// \details A changed file first, by exact path and then by unique suffix
	/// (several changed files sharing the suffix are offered); then a file of
	/// the head, the same way, when an unchanged file can be shown.
	[[nodiscard]] auto resolve_agent_reference(std::string_view aPath, const agent_reference_files& aFiles) -> std::vector<std::string>;

	/// \brief One reference of a Markdown text that becomes a link.
	struct agent_reference_link
	{
		std::size_t Begin  = 0U;
		std::size_t Length = 0U;
		/// One file opens; several are offered to choose from.
		std::vector<std::string> Paths;
		std::uint32_t            First  = 0U;
		std::uint32_t            Last   = 0U;
		bool                     Before = false;

		auto operator<=>(const agent_reference_link&) const = default;
	};

	/// \brief Every reference of `aMarkdown` that will open, and where.
	///
	/// \details In prose and emphasis, and an inline code span whose whole
	/// content is one reference (the link then covers the whole span).
	/// Fenced code, other code spans and existing links are left as written.
	/// A bare "line 12" names `aContextFile` when it is not empty. A reference
	/// that resolves to nothing openable is not returned: a link is drawn only
	/// for a reference that will open.
	[[nodiscard]] auto agent_markdown_reference_links(std::string_view aMarkdown, const agent_reference_files& aFiles, std::string_view aContextFile) -> std::vector<agent_reference_link>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_REFERENCE_HPP
