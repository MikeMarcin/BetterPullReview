#include "../sqlite_contract/scratch_root.hpp"
#include "../support/git_metadata_writers.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/core/git_config.hpp"
#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_git.hpp"
#include "bpr/core/review_cycle_ignore.hpp"
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
/// \brief [[BPR-1457]]. Measures the Git metadata readers RFC-BPR-0083
/// compares. Reports numbers; asserts only correctness and bounds, never
/// wall-clock thresholds. [[BPR-2308]]: the Review Cycle capture, comparison
/// and diff half retired with Review Cycles.
/// \details `--git-index-entries N` (with `--git`) measures the
/// Git metadata readers RFC-BPR-0083 compares: the median of five index parses
/// of an N-entry index Git accepts, the median of five ignore walks over its
/// paths, and peak memory. `--adversarial` times the H6 cases.
namespace {
	using namespace bpr;
	using clock_type = std::chrono::steady_clock;
	using tests::expect;
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
	auto gitEntries = std::size_t{ 0 };
	for (int index = 1; index < aArgc; ++index) {
		if (std::string_view{ aArgv[index] } == "--adversarial") {
			measure_adversarial();
		}
	}
	for (int index = 1; index + 1 < aArgc; ++index) {
		if (std::string_view{ aArgv[index] } == "--git-index-entries") {
			gitEntries = static_cast<std::size_t>(std::stoull(aArgv[index + 1]));
		}
	}
	tests::sqlite_contract::scratch_root scratch{ "git-metadata-scale" };
	if (gitEntries != 0U) {
		const auto git = tests::workspace_contract::git_from(aArgc, aArgv);
		if (git) {
			measure_git_readers(scratch.path(), *git, gitEntries);
		} else {
			std::cout << "SKIPPED: --git-index-entries needs --git\n";
		}
	}
	std::cout << "git-metadata-scale-bench:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return tests::failing_exit();
}
