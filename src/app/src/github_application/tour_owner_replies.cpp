#include "github_application/tour_owner_replies.hpp"

#include "bpr/adapters/sqlite/tour_owner_channel.hpp"

#include <utility>

namespace bpr::app {
	auto tour_owner_error(std::string aCode, std::string aMessage) -> application::tour_channel_bytes
	{
		return adapters::sqlite::encode_tour_response({ .Error = std::move(aCode), .Json = std::move(aMessage) });
	}

	auto assist_refusal_code(std::string_view aReason) -> std::string { return std::string{ aReason }; }
}
