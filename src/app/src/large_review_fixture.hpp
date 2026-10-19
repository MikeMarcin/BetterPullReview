#ifndef BPR_SRC_APP_SRC_LARGE_REVIEW_FIXTURE_HPP
#define BPR_SRC_APP_SRC_LARGE_REVIEW_FIXTURE_HPP

#if !defined(BPR_BUILD_TESTING)
	#error "The large-review replay is test-build only"
#endif

#include "github_read_contract.hpp"

#include <glaze/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::app::detail::large_review_fixture {
	namespace wire                             = adapters::github::detail;
	inline constexpr std::string_view base_oid = "878e9d7043aa7dcbee8ce1fecd6633e7c3eee4ff";
	inline constexpr std::string_view head_oid = "3a8e16042ce05cae93fa7782aa9d013e49c71cbb";
	inline constexpr std::string_view title    = "Synthetic large review benchmark";

	[[nodiscard]] inline auto read_json(const std::filesystem::path& aPath) -> glz::generic
	{
		std::ifstream input{ aPath, std::ios::binary };
		if (!input) {
			throw std::runtime_error{ "Large-review fixture is unavailable" };
		}
		const std::string bytes{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
		glz::generic      value;
		if (glz::read_json(value, bytes)) {
			throw std::runtime_error{ "Large-review fixture JSON is malformed" };
		}
		return value;
	}

	[[nodiscard]] inline auto request(wire::read_operation aOperation, std::uint32_t aPage = 1U, std::string_view aHead = head_oid) -> wire::semantic_read_request
	{
		wire::semantic_read_request result{ .Operation = aOperation, .Owner = "bpr-fixtures", .Repository = "large-review", .Page = aPage };
		switch (aOperation) {
		case wire::read_operation::get_viewer_identity:
			result.Owner.clear();
			result.Repository.clear();
			break;
		case wire::read_operation::get_repository:
			break;
		case wire::read_operation::list_commit_statuses:
		case wire::read_operation::list_checks:
			result.HeadOid = aHead;
			break;
		case wire::read_operation::search_pull_requests:
			result.SearchTerms = "is:open";
			break;
		default:
			result.PullRequestNumber = 17U;
			break;
		}
		return result;
	}

	inline void append_exchange(std::vector<wire::recorded_exchange>& aExchanges,
								const wire::semantic_read_request&    aRequest,
								const glz::generic&                   aBody,
								std::string                           aNextLink = { })
	{
		auto compiled = wire::compile_read_request(aRequest);
		auto body     = glz::write_json(aBody);
		if (!compiled || !body) {
			throw std::runtime_error{ "Large-review fixture cannot compile its exchange" };
		}
		wire::wire_response response{
			.Status       = 200,
			.Headers      = { { .Name = "Content-Type", .Value = "application/json" } },
			.Body         = std::move(*body),
			.Completeness = aNextLink.empty() ? wire::page_completeness::complete : wire::page_completeness::has_next_page,
		};
		if (!aNextLink.empty()) {
			response.Headers.push_back({ .Name = "Link", .Value = std::move(aNextLink) });
		}
		aExchanges.push_back({ .Request = std::move(*compiled), .Response = std::move(response) });
	}

	[[nodiscard]] inline auto connection(glz::generic::array_t aNodes, std::size_t aTotal,
										 std::size_t aEnd, std::string_view aPrefix) -> glz::generic
	{
		return glz::generic{
			{ "nodes", std::move(aNodes) },
			{ "totalCount", aTotal },
			{ "pageInfo", glz::generic{ { "hasNextPage", aEnd < aTotal }, { "endCursor", std::string{ aPrefix } + std::to_string(aEnd) } } },
		};
	}

	[[nodiscard]] inline auto file_connection(const glz::generic::array_t& aFiles, std::size_t aStart) -> glz::generic
	{
		const auto            end = std::min(aStart + 100U, aFiles.size());
		glz::generic::array_t nodes;
		for (auto index = aStart; index < end; ++index) {
			nodes.emplace_back(glz::generic::object_t{ { "path", aFiles[index]["path"] }, { "viewerViewedState", "UNVIEWED" } });
		}
		return connection(std::move(nodes), aFiles.size(), end, "files-");
	}

	[[nodiscard]] inline auto thread_connection(const std::filesystem::path& aRoot, const glz::generic& aPage,
												std::size_t aPageIndex) -> glz::generic
	{
		const auto comments = read_json(aRoot / aPage["file"].get_string());
		if (comments.get_array().size() != 100U || aPage["count"].get_number() != 100.0) {
			throw std::runtime_error{ "Large-review fixture comment page is incomplete" };
		}
		glz::generic::array_t nodes;
		for (const auto& comment : comments.get_array()) {
			if (comment["commit_id"].get_string() != head_oid
				|| comment["original_commit_id"].get_string() != head_oid) {
				throw std::runtime_error{ "Large-review fixture comment head mismatch" };
			}
			const auto& id         = comment["node_id"].get_string();
			auto        normalized = glz::generic{
				{ "author", glz::generic{ { "id", comment["user"]["node_id"] }, { "login", comment["user"]["login"] } } },
				{ "bodyText", comment["body"] },
				{ "id", id },
				{ "line", comment["line"] },
				{ "originalLine", comment["original_line"] },
				{ "outdated", false },
				{ "path", comment["path"] },
				{ "replyTo", nullptr },
			};
			nodes.emplace_back(glz::generic::object_t{
				{ "comments", connection(glz::generic::array_t{ std::move(normalized) }, 1U, 1U, "comment-") },
				{ "diffSide", comment["side"] },
				{ "id", "SYNTHETIC_THREAD_" + id },
				{ "isOutdated", false },
				{ "isResolved", false },
				{ "line", comment["line"] },
				{ "originalLine", comment["original_line"] },
				{ "path", comment["path"] },
				{ "subjectType", "LINE" },
				{ "viewerCanResolve", true },
				{ "viewerCanUnresolve", false },
			});
		}
		return connection(std::move(nodes), 1000U, (aPageIndex + 1U) * 100U, "threads-");
	}

	inline void append_rest_files(std::vector<wire::recorded_exchange>& aExchanges, const glz::generic::array_t& aFiles)
	{
		for (std::size_t start = 0U; start < aFiles.size(); start += 100U) {
			const auto            end = std::min(start + 100U, aFiles.size());
			glz::generic::array_t nodes;
			for (auto index = start; index < end; ++index) {
				const auto& file = aFiles[index];
				nodes.emplace_back(glz::generic::object_t{
					{ "filename", file["path"] },
					{ "status", "modified" },
					{ "additions", file["additions"].is_null() ? glz::generic(0) : file["additions"] },
					{ "deletions", file["deletions"].is_null() ? glz::generic(0) : file["deletions"] },
					{ "sha", file["head"]["oid"] },
				});
			}
			const auto page = static_cast<std::uint32_t>(start / 100U + 1U);
			const auto link = end < aFiles.size()
				? "<https://api.github.com/repos/bpr-fixtures/large-review/pulls/17/files?page=" + std::to_string(page + 1U) + "&per_page=100>; rel=\"next\""
				: std::string{ };
			append_exchange(aExchanges, request(wire::read_operation::list_pull_request_files, page), std::move(nodes), link);
		}
	}

	inline void append_graphql(std::vector<wire::recorded_exchange>& aExchanges, const std::filesystem::path& aRoot,
							   const glz::generic& aManifest, std::string_view aHead)
	{
		const auto& files = aManifest["files"].get_array();
		const auto& pages = aManifest["comment_pages"].get_array();
		for (std::uint32_t sequence = 1U; sequence <= 15U; ++sequence) {
			const auto initial   = sequence == 1U;
			const auto filePage  = sequence <= 6U;
			const auto pageIndex = static_cast<std::size_t>(filePage ? sequence - 1U : sequence - 6U);
			auto       semantic  = request(wire::read_operation::list_review_threads, sequence);
			semantic.GraphqlPage = wire::github_graphql_page_request{
				.Kind        = initial ? wire::github_graphql_page_kind::initial
					: filePage         ? wire::github_graphql_page_kind::files
									   : wire::github_graphql_page_kind::threads,
				.Shape       = { .ReviewThreads = true, .ViewedFileState = true, .ResolvePermission = true, .UnresolvePermission = true },
				.AfterCursor = initial ? std::string{ } : std::string{ filePage ? "files-" : "threads-" } + std::to_string(pageIndex * 100U),
			};
			auto pull = glz::generic{ { "id", "PR_large_review_17" }, { "headRefOid", aHead } };
			if (filePage) {
				pull["files"] = file_connection(files, pageIndex * 100U);
			}
			if (initial || !filePage) {
				pull["reviewThreads"] = thread_connection(aRoot, pages[pageIndex], pageIndex);
			}
			append_exchange(aExchanges, semantic, glz::generic{
													  { "data", glz::generic{ { "repository", glz::generic{ { "id", "R_large_review" }, { "pullRequest", std::move(pull) } } } } },
												  });
		}
	}

	/// \brief Reconstructs one exact stored head using compact reverse file deltas.
	[[nodiscard]] inline auto manifest_for_head(glz::generic        aManifest,
												const glz::generic& aEpochs, std::string_view aHead) -> glz::generic
	{
		const auto& heads    = aManifest["heads"].get_array();
		const auto  selected = std::ranges::find_if(heads, [aHead](const auto& aValue) {
			return aValue.get_string() == aHead;
		});
		const auto& deltas   = aEpochs["deltas"].get_array();
		if (heads.size() != 32U || selected == heads.end()
			|| aEpochs["schema"].get_string() != "bpr.large-review-epoch-deltas.v1"
			|| aEpochs["base"].get_string() != aManifest["base"].get_string()
			|| deltas.size() != 31U) {
			throw std::runtime_error{ "Large-review epoch identity or count mismatch" };
		}
		const auto selectedIndex = static_cast<std::size_t>(selected - heads.begin());
		for (std::size_t index = 0U; index < deltas.size(); ++index) {
			const auto& delta = deltas[index];
			if (delta["previous_head"].get_string() != heads[index].get_string()
				|| delta["head"].get_string() != heads[index + 1U].get_string()
				|| delta["files"].get_array().size() != 1U) {
				throw std::runtime_error{ "Large-review epoch chain mismatch" };
			}
			for (const auto side : { "before", "after" }) {
				const auto& blob  = delta["files"].get_array().front()[side];
				const auto& oid   = blob["oid"].get_string();
				const auto  bytes = blob["bytes"].get_number();
				if (oid.size() != 40U || !std::ranges::all_of(oid, [](char aCharacter) {
						return (aCharacter >= '0' && aCharacter <= '9') || (aCharacter >= 'a' && aCharacter <= 'f');
					})
					|| bytes <= 0.0 || bytes > 1024.0 || std::floor(bytes) != bytes) {
					throw std::runtime_error{ "Large-review epoch blob metadata mismatch" };
				}
			}
		}
		auto& files = aManifest["files"].get_array();
		for (auto index = deltas.size(); index > selectedIndex; --index) {
			const auto& delta = deltas[index - 1U]["files"].get_array().front();
			const auto  found = std::ranges::find_if(files, [&delta](const auto& aFile) {
				return aFile["path"].get_string() == delta["path"].get_string();
			});
			if (found == files.end()
				|| (*found)["head"]["oid"].get_string() != delta["after"]["oid"].get_string()
				|| (*found)["head"]["bytes"].get_number() != delta["after"]["bytes"].get_number()) {
				throw std::runtime_error{ "Large-review epoch delta does not match its source tree" };
			}
			(*found)["head"] = delta["before"];
		}
		return aManifest;
	}

	/// \brief Compiles the exact local workload's 30 authenticated exchanges.
	[[nodiscard]] inline auto load(const std::filesystem::path& aRoot, std::string_view aHead = head_oid) -> std::vector<wire::recorded_exchange>
	{
		auto manifest = read_json(aRoot / "manifest.json");
		if (manifest["schema"].get_string() != "bpr.large-review-fixture.v1"
			|| manifest["base"].get_string() != base_oid
			|| manifest["heads"].get_array().size() != 32U
			|| manifest["heads"].get_array().back().get_string() != head_oid
			|| manifest["files"].get_array().size() != 515U
			|| manifest["comment_pages"].get_array().size() != 10U) {
			throw std::runtime_error{ "Large-review fixture identity or dimensions mismatch" };
		}
		if (aHead != head_oid) {
			manifest = manifest_for_head(std::move(manifest), read_json(aRoot / "epoch-deltas.json"), aHead);
		}
		auto exchanges = std::vector<wire::recorded_exchange>{ };
		exchanges.reserve(30U);
		append_exchange(exchanges, request(wire::read_operation::get_viewer_identity), glz::generic{
																						   { "id", 800001 },
																						   { "node_id", "SYNTHETIC_USER_800001" },
																						   { "login", "fixture-reviewer" },
																						   { "name", "Fixture Reviewer" },
																					   });
		append_exchange(exchanges, request(wire::read_operation::get_repository), glz::generic{
																					  { "id", 800002 },
																					  { "node_id", "R_large_review" },
																					  { "name", "large-review" },
																					  { "full_name", "bpr-fixtures/large-review" },
																					  { "owner", glz::generic{ { "login", "bpr-fixtures" } } },
																					  { "private", false },
																					  { "default_branch", "main" },
																				  });
		const auto actor      = glz::generic{ { "node_id", "SYNTHETIC_USER_800001" }, { "login", "fixture-reviewer" } };
		const auto repository = glz::generic{ { "id", 800002 }, { "full_name", "bpr-fixtures/large-review" } };
		append_exchange(exchanges, request(wire::read_operation::get_pull_request), glz::generic{
																						{ "id", 800003 },
																						{ "node_id", "PR_large_review_17" },
																						{ "number", 17 },
																						{ "state", "open" },
																						{ "title", title },
																						{ "body", aHead == head_oid ? "Synthetic local benchmark; latest head only." : "Synthetic local benchmark; earlier head projection." },
																						{ "changed_files", 515 },
																						{ "commits", 1 },
																						{ "draft", false },
																						{ "user", actor },
																						{ "base", glz::generic{ { "ref", "main" }, { "sha", base_oid }, { "repo", repository } } },
																						{ "head", glz::generic{ { "ref", "benchmark" }, { "sha", aHead }, { "repo", repository } } },
																						{ "merged", false },
																					});
		append_rest_files(exchanges, manifest["files"].get_array());
		append_exchange(exchanges, request(wire::read_operation::list_pull_request_commits), glz::generic::array_t{
																								 glz::generic{ { "sha", aHead }, { "node_id", aHead == head_oid ? std::string{ "C_large_review_latest" } : "C_large_review_" + std::string{ aHead } }, { "commit", glz::generic{ { "message", aHead == head_oid ? "Synthetic latest head" : "Synthetic earlier head" } } }, { "author", actor } },
																							 });
		append_exchange(exchanges, request(wire::read_operation::list_reviews), glz::generic::array_t{ });
		append_graphql(exchanges, aRoot, manifest, aHead);
		append_exchange(exchanges, request(wire::read_operation::list_issue_comments), glz::generic::array_t{ });
		append_exchange(exchanges, request(wire::read_operation::search_pull_requests), glz::generic{
																							{ "total_count", 0 },
																							{ "incomplete_results", false },
																							{ "items", glz::generic::array_t{ } },
																						});
		append_exchange(exchanges, request(wire::read_operation::list_checks, 1U, aHead), glz::generic{ { "total_count", 0 }, { "check_runs", glz::generic::array_t{ } } });
		append_exchange(exchanges, request(wire::read_operation::list_commit_statuses, 1U, aHead), glz::generic::array_t{ });
		// Stored heads are observed sequentially; this synthetic course has no
		// provider-evidenced force pushes. Exhaust the real timeline read too.
		append_exchange(exchanges, request(wire::read_operation::list_pull_request_timeline), glz::generic::array_t{ });
		return exchanges;
	}

	/// \brief Admits only the acquired historical head and optional exact base want.
	[[nodiscard]] inline auto exact_history_fetch_wants(std::string_view aBody, std::string_view aBase, std::string_view aHead) noexcept -> bool
	{
		bool headSeen = false, baseSeen = false;
		for (auto position = aBody.find("want "); position != std::string_view::npos; position = aBody.find("want ", position)) {
			position += 5U;
			const auto end = aBody.find('\n', position);
			if (end == std::string_view::npos) {
				return false;
			}
			const auto oid = aBody.substr(position, end - position);
			if (oid == aHead && !headSeen) {
				headSeen = true;
			} else if (oid == aBase && !baseSeen) {
				baseSeen = true;
			} else {
				return false;
			}
			position = end + 1U;
		}
		return headSeen;
	}

	/// \brief Replays exact compiled requests while the production pager validates responses.
	class sequential_replay final
	{
	public:
		explicit sequential_replay(std::vector<wire::recorded_exchange> aExchanges)
			: Exchanges{ std::move(aExchanges) }
		{ }
		[[nodiscard]] auto perform(const wire::wire_request& aRequest) -> wire::response_result
		{
			if (const auto valid = wire::validate_outbound_read_request(aRequest); !valid) {
				return std::unexpected{ valid.error() };
			}
			if (Next == Exchanges.size()) {
				return std::unexpected{ wire::contract_failure{ .Code = wire::contract_failure_code::recording_exhausted } };
			}
			if (aRequest != Exchanges[Next].Request) {
				return std::unexpected{ wire::contract_failure{ .Code = wire::contract_failure_code::unexpected_operation } };
			}
			return Exchanges[Next++].Response;
		}
		[[nodiscard]] auto consumed_all() const noexcept -> bool { return Next == Exchanges.size(); }
		void               rewind() noexcept { Next = 0U; }
		/// \brief Restarts an explicitly repeated read at its first exact operation.
		void replay_from(const wire::read_operation aOperation) noexcept
		{
			const auto found = std::ranges::find_if(Exchanges, [aOperation](const auto& aExchange) { return aExchange.Request.Operation == aOperation; });
			Next             = static_cast<std::size_t>(std::distance(Exchanges.begin(), found));
		}

	private:
		std::vector<wire::recorded_exchange> Exchanges;
		std::size_t                          Next = 0U;
	};
}
#endif // BPR_SRC_APP_SRC_LARGE_REVIEW_FIXTURE_HPP
