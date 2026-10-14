#include "../support/git_metadata_writers.hpp"
#include "product_diff.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/core/git_config.hpp"
#include "bpr/core/review_cycle_git.hpp"
#include "bpr/core/review_cycle_ignore.hpp"
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
/// \details [[BPR-1457]]. `--git-index-entries N` (with `--git`) measures the
/// Git metadata readers RFC-BPR-0083 compares: the median of five index parses
/// of an N-entry index Git accepts, the median of five ignore walks over its
/// paths, and peak memory. `--adversarial` times the H6 cases.
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

	template <typename TRun>
	auto median_of_five(TRun aRun) -> double
	{
		auto runs = std::vector<double>{ };
		for (int run = 0; run < 5; ++run) {
			const auto start = clock_type::now();
			aRun();
			runs.push_back(milliseconds(clock_type::now() - start));
		}
		return percentile(runs, 0.5);
	}

	/// A representative checkout ignore file: names, extensions, directories,
	/// anchored paths, double stars, classes and negations.
	constexpr auto scale_ignore_rules = std::string_view{
		"*.log\n*.tmp\n*.o\nbuild/\nout/\n**/node_modules/\n.cache/\n/d0/s1/\nd1*/s[0-4]/f1*.txt\n!d1/s0/f1000.txt\n"
		"**/coverage/**\n*.swp\n*~\n.DS_Store\nThumbs.db\nd2/**/f2[[:digit:]]9*.txt\n!keep.log\n*.pyc\n__pycache__/\ndist/\n"
	};

	void measure_git_readers(const std::filesystem::path& aScratch, const tests::workspace_contract::git_runner& aGit, const std::size_t aEntries)
	{
		const auto repo = aScratch / "git-scale";
		std::filesystem::create_directories(repo);
		aGit(repo, { "init", "--quiet" });
		write_file(aScratch / "empty", "");
		const auto blob  = aGit.output(repo, { "hash-object", "-w", (aScratch / "empty").string() }, aScratch / "blob.out");
		auto       index = core::git_index{ .Version = 2 };
		auto       paths = std::vector<std::string>{ };
		for (std::size_t file = 0; file < aEntries; ++file) {
			paths.push_back(path_of(file).generic_string());
		}
		std::ranges::sort(paths);
		const auto oid = core::parse_git_blob_oid(blob);
		expect(oid.has_value(), "the empty blob's name");
		for (const auto& path : paths) {
			index.Entries.push_back({ .Path = path, .Mode = 0100644U, .Object = *oid });
		}
		const auto bytes = tests::write_git_index(index, core::git_hash_algorithm::sha1, false);
		write_file(repo / ".git" / "index", std::string{ bytes.begin(), bytes.end() });
		// Git itself accepts the index: it lists every entry.
		const auto listed = aGit.output(repo, { "ls-files", "--stage", "--", "d0/s0/f0.txt" }, aScratch / "listed.out");
		expect(listed.find("d0/s0/f0.txt") != std::string::npos, "Git reads the written index");

		auto       parsedEntries = std::size_t{ 0 };
		const auto parse         = median_of_five([&] {
			const auto parsed = core::parse_git_index(bytes, core::git_hash_algorithm::sha1, core::maximum_cycle_capture_entries);
			expect(parsed.has_value(), "the index parses");
			parsedEntries = parsed->Entries.size();
		});
		expect(parsedEntries == aEntries, "every entry parsed");
		// The same index under index.skipHash: the parse without the checksum.
		const auto unhashed      = tests::write_git_index(index, core::git_hash_algorithm::sha1, true);
		const auto parseUnhashed = median_of_five([&] { expect(core::parse_git_index(unhashed, core::git_hash_algorithm::sha1, core::maximum_cycle_capture_entries).has_value(), "the skipHash index parses"); });

		auto rules = core::workspace_ignore_rules{ };
		expect(rules.add("", scale_ignore_rules), "rules load");
		auto       ignored = std::size_t{ 0 };
		const auto walk    = median_of_five([&] {
			// Each directory once, as a walk meets it, then each file.
			ignored       = 0;
			auto lastDir  = std::string{ };
			auto excluded = std::string{ "\x01" };
			for (const auto& path : paths) {
				const auto slash = path.find_last_of('/');
				const auto dir   = path.substr(0, slash);
				if (dir != lastDir) {
					lastDir  = dir;
					excluded = "\x01";
					for (auto at = dir.find('/');; at = dir.find('/', at + 1)) {
						const auto prefix = dir.substr(0, at);
						if (rules.ignored(prefix, true)) {
							excluded = prefix;
							break;
						}
						if (at == std::string::npos) {
							break;
						}
					}
				}
				if (path.starts_with(excluded) || rules.ignored(path, false)) {
					++ignored;
				}
			}
		});
		std::cout << "MEASURE git_index_parse_ms median_of_5=" << parse << " entries=" << aEntries << " index_bytes=" << bytes.size() << " skip_hash_median_of_5=" << parseUnhashed << '\n';
		std::cout << "MEASURE git_ignore_walk_ms median_of_5=" << walk << " paths=" << paths.size() << " ignored=" << ignored << " rules=" << rules.size() << '\n';
		std::cout << "MEASURE git_readers_peak_resident_mb=" << peak_resident_megabytes() << '\n';
	}

	/// RFC-BPR-0083 H6: inputs built to make a reader work hard. Each line
	/// reports one case's time; the decision record compares it with 1 s.
	void measure_adversarial()
	{
		const auto time = [](const char* aName, auto aRun) {
			const auto start  = clock_type::now();
			const auto result = aRun();
			std::cout << "MEASURE h6_" << aName << "_ms=" << milliseconds(clock_type::now() - start) << " result=" << result << std::endl;
		};
		auto stars = std::string{ };
		while (stars.size() < 6000U) {
			stars += "*a";
		}
		stars += "b";
		const auto longName = std::string(4095U, 'a');
		// BPR-0970's shape: one 6 KiB line of stars against a long name.
		time("bpr0970_stars", [&] { return core::wildcard_match(stars, longName); });
		time("bpr0970_stars_folded", [&] { return core::wildcard_match(stars, longName, true); });
		auto deep = std::string{ };
		while (deep.size() + 2U < 4096U) {
			deep += deep.empty() ? "a" : "/a";
		}
		auto doubleStars = std::string{ };
		while (doubleStars.size() < 6000U) {
			doubleStars += "**/";
		}
		time("double_star_chain", [&] { return core::wildcard_match(doubleStars + "x", deep); });
		auto mixed = std::string{ };
		while (mixed.size() < 6000U) {
			mixed += "**/a*";
		}
		time("double_star_and_star", [&] { return core::wildcard_match(mixed + "b", deep); });
		auto segments = std::string{ };
		while (segments.size() < 4000U) {
			segments += "*/";
		}
		time("star_segments", [&] { return core::wildcard_match(segments + "*b", deep); });
		auto brackets = std::string{ };
		while (brackets.size() < 6000U) {
			brackets += "[[:alpha:]a-z]*";
		}
		time("bracket_stars", [&] { return core::wildcard_match(brackets + "!", longName, true); });

		auto ignoreFile = std::string{ };
		for (std::size_t rule = 0; ignoreFile.size() < 1000U * 1024U; ++rule) {
			ignoreFile += "*a*a*a" + std::to_string(rule) + "*b\n";
		}
		auto big = core::workspace_ignore_rules{ };
		time("one_mib_ignore_file_load", [&] { return big.add("", ignoreFile); });
		// One path against every rule of the largest file capture reads.
		time("one_mib_ignore_file_one_path", [&] { return big.ignored("d0/" + std::string(200U, 'a'), false); });
		// The same bound with every rule's bytes present in the path, so no
		// rule is skipped before the matcher runs: 100,000 rules that each scan
		// the whole name and fail at its end.
		auto present = std::string{ };
		for (std::size_t rule = 0; rule < core::maximum_ignore_rules; ++rule) {
			present += "*c*a*a\n";
		}
		auto dense = core::workspace_ignore_rules{ };
		time("dense_ignore_file_load", [&] { return dense.add("", present); });
		time("dense_ignore_file_one_path", [&] { return dense.ignored("d0/" + std::string(199U, 'a') + "c", false); });

		auto chain = std::string{ };
		for (int link = 0; link < 50'000; ++link) {
			chain += "[attr]m" + std::to_string(link) + " m" + std::to_string(link + 1) + "\n";
		}
		chain += "[attr]m50000 filter=end\n*.c m0\n";
		auto attributes = core::workspace_attribute_rules{ };
		time("macro_chain_50000", [&] { return attributes.add("", chain) && attributes.filter("x.c").Value == "end"; });

		auto config = std::string{ "[core]\n" };
		for (std::size_t entry = 0; entry + 1U < core::maximum_git_config_entries; ++entry) {
			config += "\tk" + std::to_string(entry) + " = \"v\\\"x\" ; c\n";
		}
		time("config_65535_entries", [&] { return core::parse_git_config(config).has_value(); });
	}
}

auto main(const int aArgc, char** aArgv) -> int
try {
	auto files      = std::size_t{ 2000 };
	auto lines      = std::size_t{ 10 };
	auto gitEntries = std::size_t{ 0 };
	auto onlyGit    = false;
	for (int index = 1; index < aArgc; ++index) {
		if (std::string_view{ aArgv[index] } == "--adversarial") {
			measure_adversarial();
			onlyGit = true;
		}
	}
	for (int index = 1; index + 1 < aArgc; ++index) {
		if (std::string_view{ aArgv[index] } == "--git-index-entries") {
			gitEntries = static_cast<std::size_t>(std::stoull(aArgv[index + 1]));
		}
		if (std::string_view{ aArgv[index] } == "--files") {
			files = static_cast<std::size_t>(std::stoull(aArgv[index + 1]));
		}
		if (std::string_view{ aArgv[index] } == "--lines") {
			lines = static_cast<std::size_t>(std::stoull(aArgv[index + 1]));
		}
	}
	tests::sqlite_contract::scratch_root scratch{ "cycle-scale" };
	if (gitEntries != 0U) {
		const auto git = tests::workspace_contract::git_from(aArgc, aArgv);
		if (git) {
			measure_git_readers(scratch.path(), *git, gitEntries);
		} else {
			std::cout << "SKIPPED: --git-index-entries needs --git\n";
		}
		onlyGit = true;
	}
	if (onlyGit) {
		std::cout << "review-cycle-scale-bench:pass\n";
		return EXIT_SUCCESS;
	}
	const auto root = scratch.path() / "tree";
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

	auto       host  = application::review_cycle_comparison_host{ *cycle.Store, cycle.Catalog.Authority, bpr::tests::product_line_matcher() };
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
