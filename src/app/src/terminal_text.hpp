#ifndef BPR_APP_TERMINAL_TEXT_HPP
#define BPR_APP_TERMINAL_TEXT_HPP
#include <QString>
namespace bpr::app {
	/// \brief Text a terminal shows as text: controls and bidirectional overrides
	/// become visible `\uXXXX` escapes; tabs and line breaks stay.
	[[nodiscard]] auto neutral_terminal_text(const QString& aText) -> QString;
	/// \brief `neutral_terminal_text`, wrapped to `COLUMNS` (20..240; default 80) by display cells.
	[[nodiscard]] auto wrapped_terminal_text(const QString& aText) -> QString;
	/// \brief Both standard input and standard output are terminals.
	[[nodiscard]] auto interactive_terminal() -> bool;
}
#endif
