// [[RFC-BPR-0082]] [[BPR-1466]]. The linked shape: Git 2.56.0's xdiff, called
// directly over the two sides' bytes with the same flags the process shape
// passes. xdiff splits lines at LF only, as the engine does; the engine's
// class check catches any normalization the flags do not reproduce. xdiff has
// no stop hook: a stop is honoured only before and after the call.

#include "candidates/candidates.hpp"

#include <regex.h>

extern "C" {
#include "xdiff.h"
}

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bpr::evaluation {
	namespace {
		extern "C" auto collect_hunk(const long aOldBegin, const long aOldCount, const long aNewBegin, const long aNewCount, void* aData) -> int
		{
			auto* hunks = static_cast<std::vector<line_hunk>*>(aData);
			hunks->push_back({
				.OldBegin = static_cast<std::uint32_t>(aOldBegin),
				.OldCount = static_cast<std::uint32_t>(aOldCount),
				.NewBegin = static_cast<std::uint32_t>(aNewBegin),
				.NewCount = static_cast<std::uint32_t>(aNewCount),
			});
			return 0;
		}
	} // namespace

	auto xdiff_hunks(const std::span<const std::uint8_t> aLeft, const std::span<const std::uint8_t> aRight, const git_algorithm aAlgorithm,
					 const bool aIndentHeuristic, const core::diff_options& aOptions) -> std::optional<std::vector<line_hunk>>
	{
		auto parameters = xpparam_t{ };
		switch (aAlgorithm) {
		case git_algorithm::myers:
			break;
		case git_algorithm::minimal:
			parameters.flags |= XDF_NEED_MINIMAL;
			break;
		case git_algorithm::patience:
			parameters.flags |= XDF_PATIENCE_DIFF;
			break;
		case git_algorithm::histogram:
			parameters.flags |= XDF_HISTOGRAM_DIFF;
			break;
		}
		if (aIndentHeuristic) {
			parameters.flags |= XDF_INDENT_HEURISTIC;
		}
		switch (aOptions.Whitespace) {
		case core::whitespace_policy::exact:
			break;
		case core::whitespace_policy::ignore_trailing:
			parameters.flags |= XDF_IGNORE_WHITESPACE_AT_EOL;
			break;
		case core::whitespace_policy::ignore_all:
			parameters.flags |= XDF_IGNORE_WHITESPACE;
			break;
		}
		if (aOptions.LineEndings == core::line_ending_policy::normalize) {
			parameters.flags |= XDF_IGNORE_CR_AT_EOL;
		}
		auto configuration      = xdemitconf_t{ };
		configuration.hunk_func = &collect_hunk;
		auto hunks              = std::vector<line_hunk>{ };
		auto callback           = xdemitcb_t{ };
		callback.priv           = &hunks;
		// xdiff reads the buffers and never writes them.
		auto left  = mmfile_t{ .ptr = const_cast<char*>(reinterpret_cast<const char*>(aLeft.data())), .size = static_cast<long>(aLeft.size()) };
		auto right = mmfile_t{ .ptr = const_cast<char*>(reinterpret_cast<const char*>(aRight.data())), .size = static_cast<long>(aRight.size()) };
		if (xdl_diff(&left, &right, &parameters, &configuration, &callback) < 0) {
			return std::nullopt;
		}
		return hunks;
	}
} // namespace bpr::evaluation
