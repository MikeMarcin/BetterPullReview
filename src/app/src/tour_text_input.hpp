#ifndef BPR_APP_TOUR_TEXT_INPUT_HPP
#define BPR_APP_TOUR_TEXT_INPUT_HPP
#include "tour_runtime.hpp"
#include <QString>
namespace bpr::app {
	[[nodiscard]] auto read_tour_terminal_line() -> tour_cli_result<std::string>;
	/// Import raw UTF-8 from one explicitly selected file or standard input.
	[[nodiscard]] auto read_tour_text_input(const QString& aPath) -> tour_cli_result<std::string>;
}
#endif
