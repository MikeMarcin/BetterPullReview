#ifndef BPR_APP_TOUR_CLI_HPP
#define BPR_APP_TOUR_CLI_HPP
#include "tour_runtime.hpp"
#include <QStringList>
namespace bpr::app {
	/// \brief Runs one bounded terminal request on the non-GUI composition.
	[[nodiscard]] auto run_tour_cli(const QStringList& aArguments, tour_reader_factory aReaders = { }) -> int;
}
#endif
