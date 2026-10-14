// [[RFC-BPR-0082]] [[BPR-1466]]. The evaluation build's hook: the components
// the ordinary entry points use, chosen once per process by the
// BPR_DIFF_EVALUATION_CANDIDATE environment variable or, when a test runner
// clears the environment, by the first line of the build tree's selection
// file. An unknown name stops the process, so a typo cannot silently measure
// the baseline.

#include "bpr/core/diff_compute_components.hpp"
#include "candidates/candidates.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

namespace {
	[[nodiscard]] auto selected_name() -> std::string
	{
#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable : 4996)
#endif
		const auto* value = std::getenv("BPR_DIFF_EVALUATION_CANDIDATE");
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif
		if (value != nullptr && *value != '\0') {
			return value;
		}
		auto stream = std::ifstream{ BPR_EVAL_SELECTION_FILE };
		auto line   = std::string{ };
		if (stream && std::getline(stream, line)) {
			return line;
		}
		return "baseline";
	}

	[[nodiscard]] auto selected() -> bpr::evaluation::candidate_components*
	{
		static const auto components = [] {
			const auto name = selected_name();
			if (!bpr::evaluation::find_candidate(name).has_value()) {
				std::fprintf(stderr, "unknown diff evaluation candidate: %s\n", name.c_str());
				std::abort();
			}
			return bpr::evaluation::make_candidate(name);
		}();
		return components.get();
	}
} // namespace

auto bpr::core::detail::evaluation_components() noexcept -> diff_components*
{
	return selected();
}
