#include "../support/test_expectation.hpp"
#include "bpr/application/feedback_counters.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/overview_feedback.hpp"
#include "bpr/core/feedback_convention.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <new>
#include <set>
#include <string>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-1094]]. What the label counters cost, measured on a synthetic
/// pull request of 1,000 and of 10,000 conversations, with no chips, three
/// and all twelve (RFC-BPR-0049 R29, R44).
/// \details The 10,000 workload is the read model's own ceiling: it admits
/// at most 10,000 review threads, 3,000 files, 50,000 comments and 16 MiB of
/// text, and the workload is checked against those limits. Each phase prints
/// the work the counter model did (bodies parsed, units compared, scopes and
/// units tallied), the C++ allocations it made and its elapsed time. Only
/// structural facts are asserted: a projection parses no body and tallies no
/// conversation, a root edit parses one body and re-tallies exactly the scopes
/// it reaches, a reply edit and a resolve parse what they must and no more.
/// Times are reported, never compared with a limit.

// Counts unaligned C++ scalar and array allocations. The row is one thread,
// so the counts between two samples are the work's own.
namespace allocation_count {
	std::atomic<std::uint64_t> Calls{ 0U };
	std::atomic<std::uint64_t> Bytes{ 0U };
}
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
auto operator new(const std::size_t aSize) -> void*
{
	if (auto* const block = std::malloc(aSize == 0U ? 1U : aSize); block != nullptr) {
		allocation_count::Calls.fetch_add(1U, std::memory_order_relaxed);
		allocation_count::Bytes.fetch_add(aSize, std::memory_order_relaxed);
		return block;
	}
	throw std::bad_alloc{ };
}
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
auto operator new[](const std::size_t aSize) -> void* { return ::operator new(aSize); }
// GCC at -O3 inlines these into a caller that it can see allocated with
// `operator new`, and then reports the `free` as mismatched; the replaced
// `operator new` above allocates with `malloc`, so the pair is matched.
#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void operator delete(void* aBlock) noexcept { std::free(aBlock); }
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void operator delete(void* aBlock, std::size_t) noexcept { std::free(aBlock); }
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void operator delete[](void* aBlock) noexcept { std::free(aBlock); }
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void operator delete[](void* aBlock, std::size_t) noexcept { std::free(aBlock); }
#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic pop
#endif

namespace {
	using namespace bpr;
	using application::feedback_counter_model;
	using application::feedback_counter_work;
	using application::github_pull_request_snapshot;
	using core::feedback_counter_scope;
	using core::feedback_counter_scope_kind;
	using core::feedback_label;
	using tests::expect;

#ifdef BPR_BENCH_BUILD_CONFIGURATION
	constexpr auto build_configuration = BPR_BENCH_BUILD_CONFIGURATION;
#else
	constexpr auto build_configuration = "unknown";
#endif
#ifdef NDEBUG
	constexpr auto assertions = "off";
#else
	constexpr auto assertions = "on";
#endif

	/// One workload: how many conversations, on how many files.
	struct workload
	{
		const char* Name          = "";
		std::size_t Conversations = 0U;
		std::size_t Files         = 0U;
	};

	auto review_key() -> core::cached_review_key
	{
		core::cached_review_key key;
		key.Authority[0] = 1U;
		key.Profile      = core::host_profile_id{ 1U };
		key.Host         = core::git_host_id{ 1U };
		key.Account      = core::github_account_id{ 1U };
		key.Locator      = { "octo", "repo" };
		key.Number       = core::pull_request_number{ 1094U };
		return key;
	}

	auto actor(const std::string& aLogin) -> application::github_actor_record
	{
		auto record  = application::github_actor_record{ };
		record.Actor = application::github_actor_id{ aLogin };
		record.Login = aLogin;
		return record;
	}

	/// Three folders deep, so a file has three folders above it:
	/// `src/m7/d2/file1234.cpp` is in `src`, `src/m7` and `src/m7/d2`.
	auto file_path(const std::size_t aFile) -> std::string
	{
		return "src/m" + std::to_string(aFile % 25U) + "/d" + std::to_string((aFile / 25U) % 4U) + "/file" + std::to_string(aFile) + ".cpp";
	}

	/// A root body: one of the twelve labels (every seventh blocking), a
	/// plain sentence, an unknown label word or a bold label, then prose.
	/// Every fiftieth root is a long body of 16 KiB.
	auto root_body(const std::size_t aThread) -> std::string
	{
		const auto kind = aThread % 16U;
		auto       body = std::string{ };
		if (kind < core::feedback_label_count) {
			body = std::string{ core::stable_name(core::feedback_label_catalog()[kind].Label) } + (aThread % 7U == 0U ? " (blocking): " : ": ");
		} else if (kind == 13U) {
			body = "concern: ";
		} else if (kind == 14U) {
			body = "**nitpick:** ";
		}
		body += "Synthetic observation " + std::to_string(aThread) + " about the reader's cache. ";
		const auto target = aThread % 50U == 49U ? std::size_t{ 16U * 1024U } : std::size_t{ 400U };
		while (body.size() < target) {
			body += "The handle is kept for the lifetime of the reader and released in its destructor. ";
		}
		return body;
	}

	/// \brief A pull request of native threads, each a root and one reply.
	/// One in a hundred is not on a file; one in three is resolved; one reply
	/// in five carries its own label. Ten top-level comments and five review
	/// summaries are not resolvable.
	auto make_snapshot(const workload& aShape) -> github_pull_request_snapshot
	{
		auto snapshot                      = github_pull_request_snapshot{ };
		snapshot.Viewer                    = actor("reviewer");
		snapshot.ReviewThreadsAvailability = application::github_collection_availability::available;
		snapshot.ReviewThreads.reserve(aShape.Conversations);
		for (std::size_t thread = 0U; thread < aShape.Conversations; ++thread) {
			const auto id     = "t" + std::to_string(thread);
			const auto path   = thread % 100U == 99U ? std::string{ } : file_path(thread % aShape.Files);
			auto       record = application::github_review_thread_record{ };
			record.Thread     = application::github_thread_id{ id };
			record.Path       = path;
			record.Line       = 3U;
			record.Resolved   = thread % 3U == 0U;
			auto root         = application::github_review_comment_record{ };
			root.Comment      = application::github_comment_id{ id + "-root" };
			root.Author       = actor("reviewer");
			root.Body         = root_body(thread);
			root.Path         = path;
			auto reply        = application::github_review_comment_record{ };
			reply.Comment     = application::github_comment_id{ id + "-reply" };
			reply.ReplyTo     = root.Comment;
			reply.Author      = actor("author");
			reply.Body        = thread % 5U == 0U ? std::string{ "question: why not reuse the first one?" } : std::string{ "Thanks, done." };
			reply.Path        = path;
			record.Comments.push_back(std::move(root));
			record.Comments.push_back(std::move(reply));
			snapshot.ReviewThreads.push_back(std::move(record));
		}
		for (std::size_t index = 0U; index < 10U; ++index) {
			auto comment    = application::github_issue_comment_record{ };
			comment.Comment = application::github_comment_id{ "c" + std::to_string(index) };
			comment.Author  = actor("reviewer");
			comment.Body    = index % 2U == 0U ? std::string{ "note: the release branch is cut on Friday." } : std::string{ "Looks good overall." };
			snapshot.IssueComments.push_back(std::move(comment));
		}
		for (std::size_t index = 0U; index < 5U; ++index) {
			auto summary   = application::github_review_record{ };
			summary.Review = application::github_review_id{ "r" + std::to_string(index) };
			summary.Author = actor("reviewer");
			summary.State  = application::github_review_state::commented;
			summary.Body   = "thought: the shape is right.";
			snapshot.Reviews.push_back(std::move(summary));
		}
		return snapshot;
	}

	/// The workload is one the read model admits: within its collection,
	/// comment and text limits.
	void expect_admitted(const workload& aShape, const github_pull_request_snapshot& aSnapshot, const std::vector<std::string>& aFiles)
	{
		const auto limits   = application::github_read_limits{ };
		auto       comments = aSnapshot.IssueComments.size();
		auto       text     = std::size_t{ 0U };
		auto       longest  = std::size_t{ 0U };
		for (const auto& thread : aSnapshot.ReviewThreads) {
			comments += thread.Comments.size();
			for (const auto& comment : thread.Comments) {
				text += comment.Body.size();
				longest = std::max(longest, comment.Body.size());
			}
		}
		for (const auto& comment : aSnapshot.IssueComments) {
			text += comment.Body.size();
		}
		for (const auto& review : aSnapshot.Reviews) {
			text += review.Body.size();
		}
		std::cout << "feedback-cost-workload name=" << aShape.Name << " conversations=" << aSnapshot.ReviewThreads.size() + aSnapshot.IssueComments.size() + aSnapshot.Reviews.size()
				  << " review_threads=" << aSnapshot.ReviewThreads.size() << " posts=" << comments + aSnapshot.Reviews.size() << " files=" << aFiles.size() << " text_bytes=" << text
				  << " longest_body=" << longest << '\n';
		expect(aSnapshot.ReviewThreads.size() <= limits.MaximumReviewThreads && aFiles.size() <= limits.MaximumFiles && comments <= limits.MaximumTotalComments
				   && aSnapshot.IssueComments.size() <= limits.MaximumIssueComments && text <= limits.MaximumTotalTextBytes && longest <= limits.MaximumBodyBytes,
			   std::string{ aShape.Name } + ": the read model admits the workload");
	}

	struct sample
	{
		std::uint64_t                         Calls = allocation_count::Calls.load(std::memory_order_relaxed);
		std::uint64_t                         Bytes = allocation_count::Bytes.load(std::memory_order_relaxed);
		std::chrono::steady_clock::time_point At    = std::chrono::steady_clock::now();
	};

	/// One phase's report: the model's work and the allocations of its first
	/// run, and the elapsed time of every run.
	struct phase_result
	{
		feedback_counter_work Work{ };
		std::uint64_t         Calls = 0U;
		std::uint64_t         Bytes = 0U;
		std::vector<double>   Microseconds;
	};

	auto difference(const feedback_counter_work& aAfter, const feedback_counter_work& aBefore) -> feedback_counter_work
	{
		auto result                   = feedback_counter_work{ };
		result.Observations           = aAfter.Observations - aBefore.Observations;
		result.BodiesParsed           = aAfter.BodiesParsed - aBefore.BodiesParsed;
		result.BodiesReused           = aAfter.BodiesReused - aBefore.BodiesReused;
		result.BodiesHashed           = aAfter.BodiesHashed - aBefore.BodiesHashed;
		result.UnitsCompared          = aAfter.UnitsCompared - aBefore.UnitsCompared;
		result.ScopesRetallied        = aAfter.ScopesRetallied - aBefore.ScopesRetallied;
		result.ScopesTalliedOnRequest = aAfter.ScopesTalliedOnRequest - aBefore.ScopesTalliedOnRequest;
		result.UnitsTallied           = aAfter.UnitsTallied - aBefore.UnitsTallied;
		return result;
	}

	/// Runs \p aRun \p aRepeats times on \p aModel; \p aBetween runs before
	/// each run, outside the measurement.
	auto measure(const std::function<const feedback_counter_model&()>& aModel, const std::size_t aRepeats, const std::function<void()>& aRun,
				 const std::function<void()>& aBetween = { }) -> phase_result
	{
		auto result = phase_result{ };
		for (std::size_t repeat = 0U; repeat < aRepeats; ++repeat) {
			if (aBetween) {
				aBetween();
			}
			const auto workBefore = aModel().work();
			const auto before     = sample{ };
			aRun();
			const auto after = sample{ };
			result.Microseconds.push_back(std::chrono::duration<double, std::micro>(after.At - before.At).count());
			if (repeat == 0U) {
				result.Calls = after.Calls - before.Calls;
				result.Bytes = after.Bytes - before.Bytes;
				result.Work  = difference(aModel().work(), workBefore);
			}
		}
		return result;
	}

	void report(const workload& aShape, const std::string& aPhase, const std::string& aColumns, const char* aMode, const phase_result& aResult)
	{
		auto times = aResult.Microseconds;
		std::ranges::sort(times);
		std::cout << "feedback-cost workload=" << aShape.Name << " phase=" << aPhase << " columns=" << aColumns << " mode=" << aMode << " runs=" << times.size()
				  << " observations=" << aResult.Work.Observations << " bodies_parsed=" << aResult.Work.BodiesParsed << " bodies_reused=" << aResult.Work.BodiesReused
				  << " bodies_hashed=" << aResult.Work.BodiesHashed
				  << " units_compared=" << aResult.Work.UnitsCompared << " scopes_retallied=" << aResult.Work.ScopesRetallied
				  << " scopes_tallied_on_request=" << aResult.Work.ScopesTalliedOnRequest << " units_tallied=" << aResult.Work.UnitsTallied
				  << " alloc_calls=" << aResult.Calls << " alloc_bytes=" << aResult.Bytes << " min_us=" << static_cast<long long>(times.front())
				  << " median_us=" << static_cast<long long>(times[times.size() / 2U]) << " max_us=" << static_cast<long long>(times.back()) << '\n';
	}

	auto columns_of(const std::size_t aCount) -> core::feedback_label_columns
	{
		// Three is the typical subset the pack names: issue, question, nitpick.
		auto labels = std::vector<feedback_label>{ };
		if (aCount == 3U) {
			labels = { feedback_label::issue, feedback_label::question, feedback_label::nitpick };
		} else {
			for (std::size_t index = 0U; index < aCount; ++index) {
				labels.push_back(core::feedback_label_catalog()[index].Label);
			}
		}
		const auto columns = core::make_feedback_label_columns(labels);
		expect(columns.has_value(), "a valid chip selection");
		return *columns;
	}

	auto scope_keys(const std::vector<feedback_counter_scope>& aScopes) -> std::set<std::pair<int, std::string>>
	{
		auto result = std::set<std::pair<int, std::string>>{ };
		for (const auto& scope : aScopes) {
			result.insert({ static_cast<int>(scope.Kind), scope.Path });
		}
		return result;
	}

	void run(const workload& aShape)
	{
		auto snapshot = make_snapshot(aShape);
		auto files    = std::vector<std::string>{ };
		files.reserve(aShape.Files);
		for (std::size_t file = 0U; file < aShape.Files; ++file) {
			files.push_back(file_path(file));
		}
		expect_admitted(aShape, snapshot, files);
		const auto key   = review_key();
		const auto posts = static_cast<std::uint64_t>(2U * aShape.Conversations + 10U + 5U);

		// The first observation: every body parsed, every scope tallied. Each
		// run observes into a model of its own, made before the measurement.
		{
			auto       models  = std::vector<feedback_counter_model>(5U);
			auto       current = models.size();
			auto       first   = application::feedback_counter_update{ };
			const auto result  = measure([&]() -> const feedback_counter_model& { return models[current]; }, models.size(), [&] {
				auto update = models[current].observe(key, snapshot);
				if (current == 0U) {
					first = std::move(update);
				} }, [&] { current = current == models.size() ? 0U : current + 1U; });
			report(aShape, "observe_cold", "-", "-", result);
			expect(first.Admitted && result.Work.BodiesParsed == posts && result.Work.BodiesReused == 0U && first.Retallied == result.Work.ScopesRetallied,
				   std::string{ aShape.Name } + ": the first observation parses every body once");
			expect(result.Work.BodiesHashed == posts, std::string{ aShape.Name } + ": the first observation hashes each body it parses, once");
		}

		auto       model = feedback_counter_model{ };
		const auto first = model.observe(key, snapshot);
		expect(first.Admitted, std::string{ aShape.Name } + ": the snapshot is admitted");
		const auto scopes = model.scopes().size();
		std::cout << "feedback-cost-scopes workload=" << aShape.Name << " kept_scopes=" << scopes << " units=" << model.population().Units.size() << '\n';
		const auto self = [&]() -> const feedback_counter_model& { return model; };

		// An unchanged snapshot: every body reused, only the pull request tallied.
		{
			const auto result = measure(self, 11U, [&] { static_cast<void>(model.observe(key, snapshot)); });
			report(aShape, "observe_unchanged", "-", "-", result);
			expect(result.Work.BodiesParsed == 0U && result.Work.BodiesReused == posts && result.Work.ScopesRetallied == 1U,
				   std::string{ aShape.Name } + ": an unchanged snapshot parses nothing and tallies only the pull request again");
			// [[BPR-1349]]. Every body is proved unchanged by its kept bytes.
			expect(result.Work.BodiesHashed == 0U, std::string{ aShape.Name } + ": an unchanged snapshot hashes no body");
		}

		// The Overview's counts for each chip selection and mode, and a sort:
		// read from the kept tallies, no body parsed, no conversation tallied.
		const auto generation = model.generation();
		for (const auto count : { std::size_t{ 0U }, std::size_t{ 3U }, std::size_t{ 12U } }) {
			for (const auto mode : { core::feedback_resolution_mode::unresolved, core::feedback_resolution_mode::all }) {
				auto request         = application::overview_feedback_request{ };
				request.Columns      = columns_of(count);
				request.Mode         = mode;
				auto        rows     = std::size_t{ 0U };
				const auto  result   = measure(self, 21U, [&] {
					const auto projection = application::project_overview_feedback(model, files, request);
					expect(projection.has_value(), "the Overview is counted");
					rows = projection->Files.size() + projection->Folders.size() + 2U;
				});
				const auto* modeName = mode == core::feedback_resolution_mode::all ? "all" : "open";
				report(aShape, "project rows=" + std::to_string(rows), std::to_string(count), modeName, result);
				expect(result.Work.Observations == 0U && result.Work.BodiesParsed == 0U && result.Work.ScopesRetallied == 0U && result.Work.UnitsTallied == 0U
						   && model.generation() == generation,
					   std::string{ aShape.Name } + ": a chip or mode change parses no body and tallies no conversation");
			}
		}
		{
			auto request      = application::overview_feedback_request{ };
			request.Columns   = columns_of(12U);
			request.Sort      = core::feedback_count_target{ .Kind = core::feedback_count_kind::included };
			const auto result = measure(self, 21U, [&] { static_cast<void>(application::project_overview_feedback(model, files, request)); });
			report(aShape, "project_sorted", "12", "open", result);
			expect(result.Work.BodiesParsed == 0U && result.Work.UnitsTallied == 0U, std::string{ aShape.Name } + ": a sort parses no body and tallies no conversation");
		}
		// One count's drill-down: the exact conversations behind the issue
		// chip of the first file.
		{
			auto request      = application::overview_feedback_request{ };
			request.Columns   = columns_of(3U);
			auto down         = application::overview_drill_down{ };
			down.Row          = application::overview_feedback_row_kind::file;
			down.Path         = files.front();
			down.Target       = core::feedback_count_target{ .Kind = core::feedback_count_kind::dedicated, .Label = feedback_label::issue };
			const auto result = measure(self, 21U, [&] { static_cast<void>(application::list_overview_drill_down(model, files, request, down)); });
			report(aShape, "drill_down", "3", "open", result);
			expect(result.Work.BodiesParsed == 0U, std::string{ aShape.Name } + ": a drill-down parses no body");
		}

		// A website edit to one root's label, the same head: that one body is
		// parsed, and the pull request, its file and the file's three folders
		// are tallied again; nothing else.
		const auto edited = file_path(0U);
		const auto reach  = std::set<std::pair<int, std::string>>{
			{ static_cast<int>(feedback_counter_scope_kind::pull_request), "" },
			{ static_cast<int>(feedback_counter_scope_kind::file), edited },
			{ static_cast<int>(feedback_counter_scope_kind::folder), "src" },
			{ static_cast<int>(feedback_counter_scope_kind::folder), "src/m0" },
			{ static_cast<int>(feedback_counter_scope_kind::folder), "src/m0/d0" },
		};
		{
			auto       flip    = false;
			auto       changed = std::vector<std::set<std::pair<int, std::string>>>{ };
			auto       retally = std::vector<std::size_t>{ };
			auto       parsed  = std::vector<std::size_t>{ };
			const auto result  = measure(self, 11U, [&] {
				const auto update = model.observe(key, snapshot);
				changed.push_back(scope_keys(update.Changed));
				retally.push_back(update.Retallied);
				parsed.push_back(update.Reparsed); }, [&] {
				flip                                       = !flip;
				snapshot.ReviewThreads[0].Comments[0].Body = (flip ? "question: " : "issue (blocking): ") + root_body(0U).substr(root_body(0U).find(": ") + 2U); });
			report(aShape, "root_edit", "-", "-", result);
			expect(std::ranges::all_of(parsed, [](const std::size_t aCount) { return aCount == 1U; }) && result.Work.BodiesReused == posts - 1U,
				   std::string{ aShape.Name } + ": a root edit parses that one body and reuses every other");
			expect(result.Work.BodiesHashed == 1U, std::string{ aShape.Name } + ": a root edit hashes only the edited body");
			expect(std::ranges::all_of(retally, [&](const std::size_t aCount) { return aCount == reach.size(); }) && std::ranges::all_of(changed, [&](const auto& aSet) { return aSet == reach; }),
				   std::string{ aShape.Name } + ": a root edit tallies again exactly the pull request, the file and its three folders, and they are what changed");
		}
		// A reply's label: one body parsed, no count changes, only the pull
		// request tallied again.
		{
			auto       flip    = false;
			auto       changed = std::size_t{ 0U };
			const auto result  = measure(self, 11U, [&] { changed += model.observe(key, snapshot).Changed.size(); }, [&] {
				flip                                       = !flip;
				snapshot.ReviewThreads[1].Comments[1].Body = flip ? "praise: much clearer now." : "Thanks, done."; });
			report(aShape, "reply_edit", "-", "-", result);
			expect(result.Work.BodiesParsed == 1U && result.Work.ScopesRetallied == 1U && changed == 0U,
				   std::string{ aShape.Name } + ": a reply's label parses one body and changes no count");
			expect(result.Work.BodiesHashed == 1U, std::string{ aShape.Name } + ": a reply edit hashes only the edited body");
		}
		// A website resolve or reopen: no body parsed, the same five scopes.
		{
			auto       changed = std::vector<std::set<std::pair<int, std::string>>>{ };
			const auto result  = measure(self, 11U, [&] { changed.push_back(scope_keys(model.observe(key, snapshot).Changed)); }, [&] { snapshot.ReviewThreads[0].Resolved = !snapshot.ReviewThreads[0].Resolved; });
			report(aShape, "resolve", "-", "-", result);
			expect(result.Work.BodiesParsed == 0U && result.Work.ScopesRetallied == reach.size() && std::ranges::all_of(changed, [&](const auto& aSet) { return aSet == reach; }),
				   std::string{ aShape.Name } + ": a resolve parses no body and tallies again exactly the five scopes it reaches");
			expect(result.Work.BodiesHashed == 0U, std::string{ aShape.Name } + ": a resolve hashes no body");
		}
	}
}

auto main() -> int
try {
	const auto before = allocation_count::Calls.load();
	auto*      probe  = ::operator new(37U);
	const auto after  = allocation_count::Calls.load();
	::operator delete(probe);
	expect(after == before + 1U, "the allocation counter sees an explicit allocation");
	std::cout << "feedback-cost-build configuration=" << build_configuration << " assertions=" << assertions << '\n';
	run({ .Name = "1k", .Conversations = 1'000U, .Files = 500U });
	run({ .Name = "10k", .Conversations = 10'000U, .Files = 2'500U });
	std::cout << "feedback-counter-costs:measured\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "FAILED: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
