#include "../support/test_expectation.hpp"
#include "product_diff.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_comparison.hpp"
#include "cycle_course.hpp"
#include "workspace_fixture.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if !defined(_WIN32)
	#include <csignal>
	#include <sys/resource.h>
	#include <sys/wait.h>
	#include <unistd.h>
#endif

/// \file
/// \brief Capture recovery and retained-material budgets (RFC-BPR-0053,
/// RFC-BPR-0054, BPR-1152): kills at every store boundary of a capture, a full
/// disk, corrupted and repaired blobs, quota refusal without eviction, retry
/// exhaustion and missing scope material. After every failure the existing
/// cycle and its revisions are exactly as they were.
namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using application::review_material_status;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::digest_of;
	using tests::workspace_contract::operation;
	using tests::workspace_contract::profile;
	using tests::workspace_contract::write_file;

	auto database(const course& aCourse) -> std::filesystem::path
	{
		return aCourse.StoreRoot.path() / "state" / "v1" / "review-cycles.sqlite3";
	}

	auto hex(const core::cycle_material_digest& aDigest) -> std::string
	{
		constexpr auto digits = std::string_view{ "0123456789ABCDEF" };
		auto           text   = std::string{ };
		for (const auto byte : aDigest.bytes()) {
			text += digits[byte >> 4U];
			text += digits[byte & 0x0FU];
		}
		return text;
	}

	void sql(const std::filesystem::path& aDatabase, const std::string& aStatement)
	{
		sqlite3* handle = nullptr;
		expect(sqlite3_open(aDatabase.string().c_str(), &handle) == SQLITE_OK, "open store for fault injection");
		expect(sqlite3_exec(handle, aStatement.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "fault injection statement");
		sqlite3_close(handle);
	}

	auto full_source(course& aCourse, const std::uint64_t aRevision, const std::string& aPath) -> application::cycle_file_observation
	{
		const auto state = aCourse.state();
		auto       host  = application::review_cycle_comparison_host{ *aCourse.Store, aCourse.Catalog.Authority, bpr::tests::product_line_matcher() };
		return host.file(aCourse.Subject, *core::resolve_comparison(state, core::full_source_view{ core::cycle_source_revision_id{ aRevision } }), aPath, { }, { });
	}

	// -------------------------------------------------------- full disk

#if !defined(_WIN32)
	/// Runs one capture in a child whose file-size limit is the store's
	/// current size: the device is full as far as SQLite can tell.
	auto capture_on_a_full_disk(course& aCourse, application::workspace_source_port& aSource) -> int
	{
		const auto state   = aCourse.state();
		auto       largest = std::uintmax_t{ 0 };
		for (const auto& file : std::filesystem::directory_iterator{ database(aCourse).parent_path() }) {
			largest = std::max(largest, file.file_size());
		}
		aCourse.Store.reset();
		const auto child = fork();
		expect(child >= 0, "fork full-disk writer");
		if (child == 0) {
			std::signal(SIGXFSZ, SIG_IGN);
			auto limit = rlimit{ .rlim_cur = static_cast<rlim_t>(largest + 4096U), .rlim_max = static_cast<rlim_t>(largest + 4096U) };
			if (setrlimit(RLIMIT_FSIZE, &limit) != 0) {
				_exit(97);
			}
			auto store   = adapters::sqlite::make_review_cycle_store(aCourse.StoreRoot.path(), profile);
			auto host    = application::review_cycle_capture_host{ aSource, *store };
			auto outcome = host.capture(state, { .Operation = operation(150), .ExpectedRevision = state.Revision }, { });
			_exit(outcome.Status == cycle_capture_status::store ? 10 : outcome.Status == cycle_capture_status::captured ? 11
																														: 12);
		}
		auto status = 0;
		expect(waitpid(child, &status, 0) == child && WIFEXITED(status), "full-disk writer exits");
		aCourse.Store = adapters::sqlite::make_review_cycle_store(aCourse.StoreRoot.path(), profile);
		return WEXITSTATUS(status);
	}

	void refuses_a_full_disk_without_losing_the_cycle(const std::filesystem::path& aScratch)
	{
		const auto root = aScratch / "disk-full";
		write_file(root / "a.txt", "before the disk filled\n");
		auto source = adapters::workspace::make_local_workspace_source();
		auto cycle  = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1 before the disk fills");
		const auto before = cycle.state();

		write_file(root / "big.bin", std::string(2U * 1024U * 1024U, 'x'));
		expect(capture_on_a_full_disk(cycle, *source) == 10, "a full disk refuses the capture as a store failure");
		expect(cycle.state() == before && cycle.revisions() == 1, "the existing cycle is unchanged");
		expect(full_source(cycle, 1, "a.txt").Status == review_material_status::available, "earlier material still reads");
		expect(cycle.capture(*source).Status == cycle_capture_status::captured && cycle.revisions() == 2, "capture succeeds once space returns");
		std::cout << "NATIVE: a file-size limit at the store's size refused the capture and lost nothing\n";
	}
#endif

	// ------------------------------------------------ corruption, quota

	void repairs_damaged_copies_and_reports_the_rest(const std::filesystem::path& aScratch)
	{
		const auto root = aScratch / "corrupt";
		write_file(root / "kept.txt", "bytes that will be captured again\n");
		write_file(root / "changed.txt", "bytes only R1 has\n");
		auto source = adapters::workspace::make_local_workspace_source();
		auto cycle  = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
		for (const auto* text : { "bytes that will be captured again\n", "bytes only R1 has\n" }) {
			sql(database(cycle), "UPDATE object_chunks SET bytes=X'0000' WHERE digest=X'" + hex(digest_of(text)) + "'");
		}
		expect(full_source(cycle, 1, "kept.txt").Status == review_material_status::corrupt, "damage is reported");
		expect(full_source(cycle, 1, "kept.txt").Source.empty(), "and never replaced by the live file");

		// Capturing the same bytes again replaces the damaged copy with bytes
		// that hash to its identity; other damage stays reported.
		write_file(root / "changed.txt", "the file moved on\n");
		const auto again = cycle.capture(*source);
		expect(again.Status == cycle_capture_status::captured, "R2");
		const auto repaired = full_source(cycle, 1, "kept.txt");
		expect(repaired.Status == review_material_status::available && std::string(repaired.Source.begin(), repaired.Source.end()) == "bytes that will be captured again\n", "a verified recapture repairs the pin");
		expect(full_source(cycle, 1, "changed.txt").Status == review_material_status::corrupt, "damage nobody can verify stays reported");
		std::cout << "NATIVE: corrupted blobs reported; a verified recapture repaired one\n";
	}

	void refuses_quota_without_evicting_pins(const std::filesystem::path& aScratch)
	{
		const auto root = aScratch / "quota";
		write_file(root / "small.txt", "small\n");
		auto source = adapters::workspace::make_local_workspace_source();
		auto cycle  = course{ *source, root, { }, 64U * 1024U };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1 inside the budget");
		const auto before = cycle.state();
		write_file(root / "large.bin", std::string(128U * 1024U, 'q'));
		const auto refused = cycle.capture(*source);
		expect(refused.Status == cycle_capture_status::store && refused.Store && refused.Store->Failure == application::cycle_store_failure::quota && refused.Store->Inventory, "quota refusal with the inventory");
		expect(refused.Store->Inventory->PinnedObjects > 0 && refused.Store->Inventory->Budget == 64U * 1024U, "pinned material counted, never evicted");
		expect(cycle.state() == before && full_source(cycle, 1, "small.txt").Status == review_material_status::available, "the existing revision is intact");
		const auto reclaimed = cycle.Store->reclaim({ });
		expect(reclaimed && reclaimed->PinnedObjects == refused.Store->Inventory->PinnedObjects, "reclaim keeps every pin");
		expect(cycle.capture(*source).Status == cycle_capture_status::store, "still refused: pins are not evicted to make room");

		// An explicit, larger budget admits the capture.
		cycle.Store = adapters::sqlite::make_review_cycle_store(cycle.StoreRoot.path(), profile, { .RetainedBudgetBytes = 1024U * 1024U });
		expect(cycle.capture(*source).Status == cycle_capture_status::captured && cycle.revisions() == 2, "admitted under an adjusted budget");
	}

	// ---------------------------------------- explanations and missing scope

	void explains_quality_without_warnings(const std::filesystem::path& aScratch)
	{
		const auto root = aScratch / "explain";
		write_file(root / "src" / "a.txt", "a\n");
		auto       source  = adapters::workspace::make_local_workspace_source();
		auto       cycle   = course{ *source, root, { "nope", "src" } };
		const auto missing = cycle.capture(*source);
		expect(missing.Status == cycle_capture_status::captured, "captured with a missing scope path");
		const auto& explained = *missing.Diagnostics.Explanation;
		expect(!explained.AtomicClaim && explained.Quality == core::cycle_capture_quality::observed_stable, "no atomicity claimed");
		expect(explained.Unavailable == 1 && explained.ExplicitDeletions == 0 && explained.Attention, "missing scope material is unavailable, not deleted, and noted");

		cycle.rescope({ "src" });
		const auto quiet = cycle.capture(*source);
		expect(quiet.Diagnostics.Explanation && !quiet.Diagnostics.Explanation->Attention && quiet.Diagnostics.Explanation->Complete, "an ordinary stable capture raises nothing");
		expect(core::explain_capture({ .Coverage = core::cycle_capture_coverage::complete }, 3).Retries == 2, "retries counted from attempts");
		expect(core::explain_capture({ .Coverage = core::cycle_capture_coverage::complete, .Quality = core::cycle_capture_quality::immutable_source }, 1).AtomicClaim, "only an immutable source claims one instant");
	}

#if !defined(_WIN32)
	// ------------------------------------------------------------ kills

	std::string KillPrefix;
	int         KillOccurrence = 1;
	int         KillSeen       = 0;
	bool        KillArmed      = false;
	bool        KillAfter      = false;
	auto        kill_trace(const unsigned aEvent, void*, void* aStatement, void*) -> int
	{
		const auto text = std::string_view{ sqlite3_sql(static_cast<sqlite3_stmt*>(aStatement)) };
		if (aEvent == SQLITE_TRACE_STMT && text.starts_with(KillPrefix)) {
			KillArmed = true;
		}
		if (KillArmed && text == "COMMIT" && aEvent == (KillAfter ? SQLITE_TRACE_PROFILE : SQLITE_TRACE_STMT)) {
			KillArmed = false;
			if (++KillSeen == KillOccurrence) {
				kill(getpid(), SIGKILL);
				_exit(99);
			}
		}
		return 0;
	}
	auto install_kill(sqlite3* aDatabase, char**, const sqlite3_api_routines*) -> int
	{
		return sqlite3_trace_v2(aDatabase, SQLITE_TRACE_STMT | SQLITE_TRACE_PROFILE, kill_trace, nullptr);
	}

	struct boundary
	{
		const char* Name;
		const char* Prefix;
		int         Occurrence;
	};

	void survives_kills_at_every_capture_boundary(const std::filesystem::path& aScratch)
	{
		// Content install, manifest install, and the revision record: before
		// and after each commit.
		constexpr auto boundaries = std::array{
			boundary{ "content install", "INSERT INTO objects", 1 },
			boundary{ "manifest install", "INSERT INTO objects", 2 },
			boundary{ "revision record", "INSERT INTO revisions", 1 },
		};
		auto source = adapters::workspace::make_local_workspace_source();
		auto index  = 0;
		for (const auto& point : boundaries) {
			for (const bool after : { false, true }) {
				const auto root = aScratch / ("kill-" + std::to_string(index++));
				write_file(root / "first.txt", "R1\n");
				auto cycle = course{ *source, root };
				expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");
				write_file(root / "second.txt", "new in R2\n");
				const auto before  = cycle.state();
				auto       request = application::cycle_capture_request{ .Operation = operation(200), .ExpectedRevision = before.Revision };
				cycle.Store.reset();

				const auto child = fork();
				expect(child >= 0, "fork capture writer");
				if (child == 0) {
					KillPrefix     = point.Prefix;
					KillOccurrence = point.Occurrence;
					KillSeen       = 0;
					KillArmed      = false;
					KillAfter      = after;
					if (sqlite3_auto_extension(reinterpret_cast<void (*)()>(install_kill)) != SQLITE_OK) {
						_exit(98);
					}
					auto store = adapters::sqlite::make_review_cycle_store(cycle.StoreRoot.path(), profile);
					auto host  = application::review_cycle_capture_host{ *source, *store };
					static_cast<void>(host.capture(before, request, { }));
					_exit(90);
				}
				auto status = 0;
				expect(waitpid(child, &status, 0) == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL, "SIGKILL at the selected boundary");

				cycle.Store         = adapters::sqlite::make_review_cycle_store(cycle.StoreRoot.path(), profile);
				const auto seen     = cycle.state();
				const auto recorded = std::string_view{ point.Prefix } == "INSERT INTO revisions" && after;
				expect(recorded ? seen.Next.SourceRevision == 3 : seen == before, "all or nothing: the cycle is as before or has the whole revision");
				expect(full_source(cycle, 1, "first.txt").Status == review_material_status::available, "the existing revision survives");

				// The same request completes exactly once.
				auto host                = application::review_cycle_capture_host{ *source, *cycle.Store };
				request.ExpectedRevision = before.Revision;
				const auto retried       = host.capture(cycle.state(), request, { });
				expect(retried.Status == cycle_capture_status::captured && retried.Receipt && retried.Receipt->Replayed == recorded, "retry completes or replays");
				expect(cycle.revisions() == 2 && full_source(cycle, 2, "second.txt").Status == review_material_status::available, "exactly one new revision, fully readable");
				const auto inventory = cycle.Store->reclaim({ });
				expect(inventory && inventory->UnreferencedObjects == 0, "only unreferenced candidates were left, and reclaim removes them");
				std::cout << "NATIVE: kill " << (after ? "after " : "before ") << point.Name << " commit recovered\n";
			}
		}
	}
#endif
}

auto main() -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-recovery" };
	repairs_damaged_copies_and_reports_the_rest(scratch.path());
	refuses_quota_without_evicting_pins(scratch.path());
	explains_quality_without_warnings(scratch.path());
#if !defined(_WIN32)
	refuses_a_full_disk_without_losing_the_cycle(scratch.path());
	survives_kills_at_every_capture_boundary(scratch.path());
#else
	std::cout << "NATIVE: full-disk and kill courses are POSIX-only\n";
#endif
	std::cout << "review-cycle-recovery-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return tests::failing_exit();
}
