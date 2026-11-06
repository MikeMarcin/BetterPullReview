#include "github_projection_mapping.hpp"

#include "../support/test_expectation.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using tests::expect;

	auto comment_id(std::size_t aIndex) -> std::string
	{
		return "comment-" + std::to_string(aIndex);
	}

	auto snapshot_of(const std::vector<std::optional<std::size_t>>& aParents) -> application::github_pull_request_snapshot
	{
		auto snapshot = application::github_pull_request_snapshot{ };
		auto thread   = application::github_review_thread_record{ };
		thread.Thread = application::github_thread_id{ "thread" };
		thread.Comments.reserve(aParents.size());
		for (std::size_t index = 0U; index < aParents.size(); ++index) {
			auto comment    = application::github_review_comment_record{ };
			comment.Comment = application::github_comment_id{ comment_id(index) };
			comment.Body    = "body-" + std::to_string(index);
			if (aParents[index]) {
				comment.ReplyTo = application::github_comment_id{ comment_id(*aParents[index]) };
			}
			thread.Comments.push_back(std::move(comment));
		}
		snapshot.ReviewThreads.push_back(std::move(thread));
		return snapshot;
	}

	/// A small independent reachability oracle: a parent is safe only when its
	/// ancestry eventually exits the graph instead of revisiting a comment.
	auto expected_parent(const std::vector<std::optional<std::size_t>>& aParents, std::size_t aIndex) -> std::string
	{
		const auto parent = aParents[aIndex];
		if (!parent || *parent >= aParents.size()) {
			return "review-thread:thread";
		}
		auto visited = std::vector<bool>(aParents.size());
		for (auto next = parent; next && *next < aParents.size(); next = aParents[*next]) {
			if (visited[*next]) {
				return "review-thread:thread";
			}
			visited[*next] = true;
		}
		return "review-comment:" + comment_id(*parent);
	}

	void verify_graph(const std::vector<std::optional<std::size_t>>& aParents)
	{
		auto snapshot = snapshot_of(aParents);
		for (const bool reversed : { false, true }) {
			if (reversed) {
				std::ranges::reverse(snapshot.ReviewThreads.front().Comments);
			}
			const auto projection = application::project_github_pull_request(snapshot);
			expect(projection.Conversations.size() == aParents.size() + 1U, "projection keeps every comment and its thread");
			for (std::size_t row = 0U; row < aParents.size(); ++row) {
				const auto  index   = reversed ? aParents.size() - row - 1U : row;
				const auto& comment = projection.Conversations[row + 1U];
				expect(comment.Id == "review-comment:" + comment_id(index) && comment.Body == "body-" + std::to_string(index), "projection preserves source order, identity and body");
				expect(comment.ParentId == expected_parent(aParents, index), "immediate parents and missing/cyclic ancestry fallback agree with reachability");
			}
		}
	}

	void test_reply_graphs()
	{
		verify_graph({ });
		verify_graph({ std::nullopt, 0U, 1U, 0U, 8U, 4U, 7U, 6U, 7U, 9U });
		// Every four-comment graph: no parent, a missing parent, or one of
		// four comments. Both source orders exercise forward/backward edges.
		for (std::size_t shape = 0U; shape < 6U * 6U * 6U * 6U; ++shape) {
			auto parents = std::vector<std::optional<std::size_t>>(4U);
			auto choices = shape;
			for (auto& parent : parents) {
				const auto choice = choices % 6U;
				if (choice != 0U) {
					parent = choice - 1U;
				}
				choices /= 6U;
			}
			verify_graph(parents);
		}
	}

	auto large_thread(std::size_t aCount, bool aCycle) -> application::github_pull_request_snapshot
	{
		auto parents = std::vector<std::optional<std::size_t>>(aCount);
		for (std::size_t index = 1U; index < aCount; ++index) {
			parents[index] = index - 1U;
		}
		if (aCycle) {
			parents.front() = aCount - 1U;
		}
		return snapshot_of(parents);
	}

	void test_large_threads()
	{
		for (const bool cycle : { false, true }) {
			auto snapshot = large_thread(10'000U, cycle);
			std::ranges::reverse(snapshot.ReviewThreads.front().Comments);
			const auto projection = application::project_github_pull_request(snapshot);
			expect(projection.Conversations.size() == 10'001U, "maximum-size thread retains every comment");
			for (std::size_t row = 1U; row < projection.Conversations.size(); ++row) {
				const auto index    = 10'000U - row;
				const auto expected = cycle || index == 0U ? std::string{ "review-thread:thread" } : "review-comment:" + comment_id(index - 1U);
				expect(projection.Conversations[row].ParentId == expected, "long chains retain immediate parents and cycles fall back without recursion");
			}
		}
	}

	void measure_projection()
	{
		for (const auto count : { 1'000U, 5'000U, 10'000U }) {
			for (const bool cycle : { false, true }) {
				const auto snapshot = large_thread(count, cycle);
				auto       samples  = std::array<double, 5U>{ };
				for (auto& elapsed : samples) {
					const auto started    = std::chrono::steady_clock::now();
					const auto projection = application::project_github_pull_request(snapshot);
					elapsed               = std::chrono::duration<double, std::milli>{ std::chrono::steady_clock::now() - started }.count();
					expect(projection.Conversations.size() == count + 1U, "measurement observes a complete projection");
				}
				std::ranges::sort(samples);
				std::printf("projection: comments=%u shape=%s median-ms=%.3f min-ms=%.3f max-ms=%.3f\n", count, cycle ? "cycle" : "chain", samples[2], samples.front(), samples.back());
			}
		}
	}
}

auto main(int aArgc, char* aArgv[]) -> int
try {
	if (aArgc == 2 && std::string_view{ aArgv[1] } == "--measure") {
		measure_projection();
		return EXIT_SUCCESS;
	}
	expect(aArgc == 1, "only --measure is accepted");
	test_reply_graphs();
	test_large_threads();
	std::puts("github-projection-mapping-tests:pass");
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
