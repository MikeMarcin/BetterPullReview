/// \file
/// \brief Candidate V of RFC-BPR-0083, measured as a test-only oracle
/// ([[BPR-1459]]): libgit2's `wildmatch` against `core::wildcard_match`, with
/// and without case folding, over generated patterns and paths.
/// \details libgit2's `wildmatch.c` reaches this executable only from the
/// evaluation build's static libgit2 archive; nothing here is linked into
/// the product or core. Usage: `wildmatch_differential [cases] [seed]
/// [--locale]`. With `--locale` the process adopts the environment's locale
/// first, as a Qt application does, because libgit2's matcher classifies
/// bytes with `<ctype.h>`. Each disagreement is printed with its class.

#include "bpr/core/review_cycle_ignore.hpp"

#include <array>
#include <clocale>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

extern "C" int wildmatch(const char* aPattern, const char* aText, unsigned int aFlags);

namespace {
	constexpr unsigned WildmatchCaseFold = 1U;
	constexpr unsigned WildmatchPathname = 2U;
	constexpr int      WildmatchMatch    = 0;

	/// Pattern pieces, weighted toward the constructs where two matchers can
	/// differ: classes, ranges, escapes, stars, `**` and case.
	constexpr std::array<std::string_view, 40> PatternPieces{
		"a",
		"b",
		"A",
		"B",
		"z",
		"Z",
		"0",
		"/",
		".",
		"-",
		" ",
		"*",
		"*",
		"?",
		"**",
		"**/",
		"/**",
		"\\",
		"\\*",
		"\\[",
		"[",
		"]",
		"[a-c]",
		"[A-C]",
		"[!a]",
		"[^b]",
		"[]a]",
		"[a-]",
		"[[:alpha:]]",
		"[[:upper:]]",
		"[[:lower:]]",
		"[[:space:]]",
		"[![:digit:]]",
		"[[:punct:]]",
		"[[:xdigit:]]",
		"[[:bogus:]]",
		"[\\]]",
		"[a",
		"\xC9",
		"\xE9",
	};
	constexpr std::array<std::string_view, 16> PathPieces{
		"a",
		"b",
		"A",
		"B",
		"z",
		"Z",
		"0",
		"/",
		".",
		"-",
		" ",
		"\t",
		"]",
		"\\",
		"\xC9",
		"\xE9",
	};

	template <std::size_t TCount>
	auto generated(std::mt19937_64& aRandom, const std::array<std::string_view, TCount>& aPieces, const std::size_t aMaximum) -> std::string
	{
		std::uniform_int_distribution<std::size_t> length(1, aMaximum);
		std::uniform_int_distribution<std::size_t> pick(0, TCount - 1);
		std::string                                text;
		for (std::size_t count = length(aRandom); count > 0; --count) {
			text += aPieces.at(pick(aRandom));
		}
		return text;
	}

	auto has_high_byte(const std::string_view aText) -> bool
	{
		return aText.find_first_of("\xC9\xE9") != std::string_view::npos;
	}

	/// The construct a disagreement most plausibly turns on, checked from
	/// most to least specific; the printed examples confirm or refine it.
	auto disagreement_class(const std::string_view aPattern, const std::string_view aPath, const bool aCaseFold) -> std::string
	{
		std::string name = aCaseFold ? "casefold:" : "exact:";
		if (has_high_byte(aPattern) || has_high_byte(aPath)) {
			return name + "byte >= 0x80";
		}
		if (aPattern.find("[:") != std::string_view::npos) {
			return name + "POSIX class";
		}
		if (aPattern.find('[') != std::string_view::npos) {
			return name + "bracket";
		}
		if (aPattern.find('\\') != std::string_view::npos) {
			return name + "escape";
		}
		if (aPattern.find("**") != std::string_view::npos) {
			return name + "double star";
		}
		return name + "other";
	}
}

auto main(int aCount, char** aArguments) -> int
{
	const std::span<char*>   arguments(aArguments, static_cast<std::size_t>(aCount));
	std::size_t              cases     = 200'000;
	std::uint64_t            seed      = 83;
	bool                     useLocale = false;
	std::vector<std::string> positional;
	for (const char* argument : arguments.subspan(1)) {
		if (std::string_view(argument) == "--locale") {
			useLocale = true;
		} else {
			positional.emplace_back(argument);
		}
	}
	if (!positional.empty()) {
		cases = std::stoull(positional[0]);
	}
	if (positional.size() > 1) {
		seed = std::stoull(positional[1]);
	}
	if (useLocale) {
		const char* adopted = std::setlocale(LC_ALL, "");
		std::cout << "locale " << (adopted == nullptr ? "(refused)" : adopted) << "\n";
	} else {
		std::cout << "locale C\n";
	}

	std::mt19937_64                                                         random(seed);
	std::map<std::string, std::pair<std::size_t, std::vector<std::string>>> classes;
	std::array<std::size_t, 2>                                              agreed{ };
	std::array<std::size_t, 2>                                              matched{ };
	for (std::size_t index = 0; index < cases; ++index) {
		const std::string pattern = generated(random, PatternPieces, 6);
		const std::string path    = generated(random, PathPieces, 8);
		for (const bool caseFold : { false, true }) {
			const bool     first     = bpr::core::wildcard_match(pattern, path, caseFold);
			const unsigned flags     = WildmatchPathname | (caseFold ? WildmatchCaseFold : 0U);
			const bool     reference = wildmatch(pattern.c_str(), path.c_str(), flags) == WildmatchMatch;
			matched.at(caseFold ? 1 : 0) += reference ? 1U : 0U;
			if (first == reference) {
				++agreed.at(caseFold ? 1 : 0);
				continue;
			}
			auto& entry = classes[disagreement_class(pattern, path, caseFold)];
			++entry.first;
			if (entry.second.size() < 6) {
				std::string example = "pattern=";
				example.append(pattern).append(" path=").append(path);
				example.append(" core=").append(first ? "match" : "no").append(" libgit2=").append(reference ? "match" : "no");
				entry.second.push_back(std::move(example));
			}
		}
	}
	std::cout << "cases " << cases << " seed " << seed << "\n";
	std::cout << "exact agreed " << agreed[0] << " (libgit2 matched " << matched[0] << ")\n";
	std::cout << "casefold agreed " << agreed[1] << " (libgit2 matched " << matched[1] << ")\n";
	for (const auto& [name, entry] : classes) {
		std::cout << "class " << name << " count " << entry.first << "\n";
		for (const std::string& example : entry.second) {
			std::cout << "  " << example << "\n";
		}
	}
	return 0;
}
