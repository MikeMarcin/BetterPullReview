#ifndef BPR_CORE_LINE_ENDINGS_HPP
#define BPR_CORE_LINE_ENDINGS_HPP
#include <string>
#include <string_view>

/// \file
/// \brief [[BPR-1298]]. A body's own line endings, kept through an editor that
/// reads them back as LF.
///
/// \details A Qt plain-text editor stores each line break as a paragraph
/// separator and reads every break back as LF, so a CRLF body written by the
/// command line or an agent lost its CRs at its first desktop edit, a label
/// chip's included (RFC-BPR-0048 R12). An editor keeps the body it loaded and
/// reads its text back through `restore_line_endings`.
namespace bpr::core {
	/// \brief The edited text with the original's line endings restored.
	///
	/// \details `aEdited` is the editor's text, whose breaks are LF; `aOriginal`
	/// is the body the editor loaded. An unedited text is the original byte for
	/// byte, mixed endings included. Otherwise every line the edit left alone at
	/// the start or the end keeps its own ending, and every other break takes
	/// the original's dominant ending: CRLF when CRLF breaks are at least as
	/// many as lone LF ones, else LF. An original without breaks gives LF.
	[[nodiscard]] auto restore_line_endings(std::string_view aEdited, std::string_view aOriginal) -> std::string;
} // namespace bpr::core

#endif // BPR_CORE_LINE_ENDINGS_HPP
