#ifndef BPR_APP_GITHUB_APPLICATION_TOUR_OWNER_REPLIES_HPP
#define BPR_APP_GITHUB_APPLICATION_TOUR_OWNER_REPLIES_HPP
#include "bpr/application/tour_owner_channel.hpp"

#include <string>
#include <string_view>

// The error replies the tour owner channel sends a bpr tour command.
namespace bpr::app {
	auto tour_owner_error(std::string aCode, std::string aMessage = { }) -> application::tour_channel_bytes;

	auto assist_refusal_code(std::string_view aReason) -> std::string;
}
#endif
