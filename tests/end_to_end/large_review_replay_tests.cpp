#include "../../src/app/src/large_review_fixture.hpp"

#include <cstdlib>
#include <iostream>

namespace {
	using namespace bpr::adapters::github::detail;
	namespace fixture = bpr::app::detail::large_review_fixture;

	void require(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
}

auto main() -> int
{
	const auto exchanges = fixture::load(BPR_LARGE_REVIEW_FIXTURE_ROOT);
	require(exchanges.size() == 29U, "exact recorded course size");
	fixture::sequential_replay replay{ exchanges };
	for (std::size_t index = 0; index < exchanges.size(); ++index) {
		const auto& exchange = exchanges[index];
		require(validate_outbound_read_request(exchange.Request).has_value(), "recorded request must be canonical");
		if (index == 3U) {
			require(exchange.Response.Completeness == page_completeness::has_next_page, "first files page must require continuation");
			strict_recorded_transport legacy{ { exchange } };
			require(!legacy.perform(exchange.Request), "legacy validator must still reject a partial page");
			require(!replay.perform(exchanges[index + 1U].Request), "skipping REST page must refuse without advancing");
		}
		if (index == 12U) {
			auto semantic = exchange.Request.Semantic;
			require(semantic.GraphqlPage.has_value(), "continuation is GraphQL");
			semantic.GraphqlPage->AfterCursor = "files-wrong";
			const auto other                  = compile_read_request(semantic);
			require(other.has_value() && !replay.perform(*other), "another canonical cursor must refuse without advancing");
			auto damaged = exchange.Request;
			damaged.Body += " ";
			require(!replay.perform(damaged), "noncanonical body must refuse without advancing");
		}
		const auto response = replay.perform(exchange.Request);
		require(response.has_value() && *response == exchange.Response, "exact request returns its exact recorded response");
	}
	require(replay.consumed_all() && !replay.perform(exchanges.front().Request), "exhaustion cannot serve another course");
	std::cout << "LARGE_REVIEW_REPLAY_CONTRACT_OK\n";
	return EXIT_SUCCESS;
}
