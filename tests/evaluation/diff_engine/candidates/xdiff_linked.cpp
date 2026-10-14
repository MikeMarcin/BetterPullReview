// [[RFC-BPR-0082]] [[BPR-1466]]. The linked shape: Git 2.56.0's xdiff, built
// as its own shared library and called through `xdl_diff`. xdiff splits lines
// at LF only, as the engine does. xdiff has no stop hook: a stop is honoured
// only before and after the call.
//
// Whitespace and line endings: xdiff's own flags do not reproduce the engine's
// normalization. `XDF_IGNORE_WHITESPACE_AT_EOL` and `XDF_IGNORE_WHITESPACE`
// treat CR, LF, VT and FF as whitespace, so a CRLF line equals its LF twin and
// a final line without a newline equals one with it, where the engine strips
// only spaces and tabs and keeps the line ending; the engine's class check
// then refused the matches (the first macOS pass, `patho-crlf` under
// `ignore_trailing` and `ignore_all`). Under any option other than exact
// comparison, xdiff is therefore given each line already normalized as the
// engine normalizes it (`normalize_line` in diff_compute.cpp), with no
// whitespace flag: two lines are byte-equal for xdiff exactly when the engine
// gives them one class. Exact comparison passes the documents' bytes as they
// are.

#include "candidates/candidates.hpp"

#include <regex.h>

extern "C" {
#include "xdiff.h"
}

#include <cstddef>
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

		[[nodiscard]] constexpr auto is_blank(const std::uint8_t aByte) noexcept -> bool
		{
			return aByte == 0x20U || aByte == 0x09U;
		}

		/// The bytes xdiff compares for one side: the document as it is under
		/// exact comparison, otherwise every line in the engine's normalized
		/// form. Spaces and tabs are single bytes in UTF-8 and never part of a
		/// longer sequence, so removing them bytewise is removing them as
		/// scalars.
		[[nodiscard]] auto comparison_bytes(const core::decoded_text_document* aDocument, const core::diff_options& aOptions,
											std::vector<std::uint8_t>& aStorage) -> std::span<const std::uint8_t>
		{
			if (aDocument == nullptr) {
				return { };
			}
			if (aOptions.Whitespace == core::whitespace_policy::exact && aOptions.LineEndings == core::line_ending_policy::exact) {
				return aDocument->Bytes;
			}
			aStorage.clear();
			aStorage.reserve(aDocument->Bytes.size());
			const auto bytes = std::span<const std::uint8_t>{ aDocument->Bytes };
			for (const auto& line : aDocument->Lines) {
				const auto content = bytes.subspan(static_cast<std::size_t>(line.Content.Begin), static_cast<std::size_t>(line.Content.End - line.Content.Begin));
				const auto start   = aStorage.size();
				for (const auto byte : content) {
					if (aOptions.Whitespace == core::whitespace_policy::ignore_all && is_blank(byte)) {
						continue;
					}
					aStorage.push_back(byte);
				}
				if (aOptions.Whitespace == core::whitespace_policy::ignore_trailing) {
					while (aStorage.size() > start && is_blank(aStorage.back())) {
						aStorage.pop_back();
					}
				}
				switch (line.Ending) {
				case core::text_line_ending::none:
					break;
				case core::text_line_ending::lf:
					aStorage.push_back('\n');
					break;
				case core::text_line_ending::crlf:
					if (aOptions.LineEndings == core::line_ending_policy::exact) {
						aStorage.push_back('\r');
					}
					aStorage.push_back('\n');
					break;
				}
			}
			return aStorage;
		}
	} // namespace

	auto xdiff_hunks(const core::decoded_text_document* aLeft, const core::decoded_text_document* aRight, const git_algorithm aAlgorithm,
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
		auto       leftStorage   = std::vector<std::uint8_t>{ };
		auto       rightStorage  = std::vector<std::uint8_t>{ };
		const auto leftBytes     = comparison_bytes(aLeft, aOptions, leftStorage);
		const auto rightBytes    = comparison_bytes(aRight, aOptions, rightStorage);
		auto       configuration = xdemitconf_t{ };
		configuration.hunk_func  = &collect_hunk;
		auto hunks               = std::vector<line_hunk>{ };
		auto callback            = xdemitcb_t{ };
		callback.priv            = &hunks;
		// xdiff reads the buffers and never writes them.
		auto left  = mmfile_t{ .ptr = const_cast<char*>(reinterpret_cast<const char*>(leftBytes.data())), .size = static_cast<long>(leftBytes.size()) };
		auto right = mmfile_t{ .ptr = const_cast<char*>(reinterpret_cast<const char*>(rightBytes.data())), .size = static_cast<long>(rightBytes.size()) };
		if (xdl_diff(&left, &right, &parameters, &configuration, &callback) < 0) {
			return std::nullopt;
		}
		return hunks;
	}
} // namespace bpr::evaluation
