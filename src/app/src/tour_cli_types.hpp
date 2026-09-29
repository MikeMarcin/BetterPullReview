#ifndef BPR_APP_TOUR_CLI_TYPES_HPP
#define BPR_APP_TOUR_CLI_TYPES_HPP
#include <expected>
#include <string>
namespace bpr::app {
	struct tour_cli_error
	{
		std::string Code, Message;
		int         Exit = 7;
	};
	template <typename T>
	using tour_cli_result = std::expected<T, tour_cli_error>;
}
#endif
