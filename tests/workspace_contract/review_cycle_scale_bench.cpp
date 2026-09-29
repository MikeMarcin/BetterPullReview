#include "../support/test_expectation.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_comparison.hpp"
#include "cycle_course.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if !defined(_WIN32)
	#include <sys/resource.h>
#endif

/// \file
/// \brief Measures capture, recapture, comparison paging and file diffs over
/// a synthetic tree (RFC-BPR-0054 LIMITS). Reports numbers; asserts only
/// correctness and bounds, never wall-clock thresholds.
namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using clock_type = std::chrono::steady_clock;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::write_file;

	auto milliseconds(const clock_type::duration aDuration) -> double
	{
		return std::chrono::duration<double, std::milli>(aDuration).count();
	}

	auto percentile(std::vector<double> aValues, const double aFraction) -> double
	{
		if (aValues.empty()) {
			return 0.0;
		}
		std::ranges::sort(aValues);
		const auto index = static_cast<std::size_t>(aFraction * static_cast<double>(aValues.size() - 1U));
		return aValues[index];
	}

	auto peak_resident_megabytes() -> double
	{
#if defined(_WIN32)
		return 0.0;
#else
		auto usage = rusage{ };
		getrusage(RUSAGE_SELF, &usage);
	#if defined(__APPLE__)
		return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
	#else
		return static_cast<double>(usage.ru_maxrss) / 1024.0;
	#endif
#endif
	}

	auto file_text(const std::size_t aFile, const std::size_t aLines, const std::string_view aTag) -> std::string
	{
		auto text = std::string{ };
		for (std::size_t line = 0; line < aLines; ++line) {
			text += "line " + std::to_string(line) + " of file " + std::to_string(aFile) + " " + std::string{ aTag } + "\n";
		}
		return text;
	}

	auto path_of(const std::size_t aFile) -> std::filesystem::path
	{
		return std::filesystem::path{ "d" + std::to_string(aFile / 1000U) } / ("s" + std::to_string((aFile / 100U) % 10U)) / ("f" + std::to_string(aFile) + ".txt");
	}
}

auto main(const int aArgc, char** aArgv) -> int
try {
	auto files = std::size_t{ 2000 };
	auto lines = std::size_t{ 10 };
	for (int index = 1; index + 1 < aArgc; ++index) {
		if (std::string_view{ aArgv[index] } == "--files") {
			files = static_cast<std::size_t>(std::stoull(aArgv[index + 1]));
		}
		if (std::string_view{ aArgv[index] } == "--lines") {
			lines = static_cast<std::size_t>(std::stoull(aArgv[index + 1]));
		}
	}
	tests::sqlite_contract::scratch_root scratch{ "cycle-scale" };
	const auto                           root = scratch.path() / "tree";
	for (std::size_t file = 0; file < files; ++file) {
		write_file(root / path_of(file), file_text(file, lines, "base"));
	}

	auto source = adapters::workspace::make_local_workspace_source();
	auto cycle  = course{ *source, root };

	const auto measure = [&](const char* aName) {
		const auto start   = clock_type::now();
		const auto outcome = cycle.capture(*source);
		const auto elapsed = milliseconds(clock_type::now() - start);
		expect(outcome.Status == cycle_capture_status::captured, "capture completes");
		std::cout << "MEASURE " << aName << "_ms=" << elapsed << " files_read=" << outcome.Diagnostics.FilesRead << " bytes_read=" << outcome.Diagnostics.BytesRead
				  << " attempts=" << outcome.Diagnostics.Attempts << '\n';
		return outcome;
	};
	const auto cold = measure("initial_capture");
	expect(cold.Capture->Entries == files, "every file captured");
	const auto warm = measure("unchanged_recapture");
	expect(warm.Receipt && !warm.Receipt->Changed, "unchanged recapture records nothing new");
	const auto changedCount = std::max<std::size_t>(1U, files / 100U);
	for (std::size_t file = 0; file < changedCount; ++file) {
		write_file(root / path_of(file * 97U % files), file_text(file * 97U % files, lines, "edited"));
	}
	const auto changed = measure("changed_recapture");
	expect(changed.Receipt && changed.Receipt->Changed, "changed recapture records a revision");

	auto       host  = application::review_cycle_comparison_host{ *cycle.Store, cycle.Catalog.Authority };
	const auto state = cycle.state();
	const auto pair  = *core::resolve_comparison(state, core::all_cycle_view{ });
	auto       pages = std::vector<double>{ };
	auto       after = std::optional<std::string>{ };
	auto       seen  = std::size_t{ 0 };
	for (;;) {
		const auto start = clock_type::now();
		const auto page  = host.page(cycle.Subject, pair, after, core::maximum_cycle_comparison_page, false, { });
		pages.push_back(milliseconds(clock_type::now() - start));
		expect(page.Page && page.Page->Entries.size() <= core::maximum_cycle_comparison_page, "bounded page");
		seen += page.Page->Entries.size();
		if (!page.Page->Next) {
			break;
		}
		after = page.Page->Next;
	}
	expect(seen == files, "pages cover every path");
	auto diffs = std::vector<double>{ };
	for (std::size_t file = 0; file < std::min<std::size_t>(changedCount, 100U); ++file) {
		const auto path  = path_of(file * 97U % files).generic_string();
		const auto start = clock_type::now();
		const auto diff  = host.file(cycle.Subject, pair, path, { }, { });
		diffs.push_back(milliseconds(clock_type::now() - start));
		expect(diff.Diff && core::validate_computed_material_diff(*diff.Diff).valid(), "valid diff");
	}
	std::cout << "MEASURE comparison_page_ms p50=" << percentile(pages, 0.5) << " p95=" << percentile(pages, 0.95) << " pages=" << pages.size() << '\n';
	std::cout << "MEASURE file_diff_ms p50=" << percentile(diffs, 0.5) << " p95=" << percentile(diffs, 0.95) << " diffs=" << diffs.size() << '\n';
	std::cout << "MEASURE peak_resident_mb=" << peak_resident_megabytes() << " files=" << files << " lines=" << files * lines << '\n';
	std::cout << "review-cycle-scale-bench:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return tests::failing_exit();
}
