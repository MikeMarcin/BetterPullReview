// [[BPR-1485]]. The only caller of xdiff (RFC-BPR-0082): Git 2.56.0's xdiff,
// built as its own shared library (cmake/BprXdiffProvider.cmake) and called
// through `xdl_diff` with Myers and the indent heuristic. xdiff splits lines
// at LF only, as the engine does.
//
// Whitespace and line endings: xdiff's own flags do not reproduce the engine's
// normalization. `XDF_IGNORE_WHITESPACE_AT_EOL` and `XDF_IGNORE_WHITESPACE`
// treat CR, LF, VT and FF as whitespace, so a CRLF line equals its LF twin and
// a final line without a newline equals one with it, where the engine strips
// only spaces and tabs and keeps the line ending (the evaluation's first macOS
// pass refused those matches). Under any option other than exact comparison,
// xdiff is therefore given each line already normalized as the engine
// normalizes it (`normalize_line` in src/core/src/diff_compute.cpp), with no
// whitespace flag: two lines are byte-equal for xdiff exactly when the engine
// gives them one class. Exact comparison passes the documents' bytes as they
// are. The core checks every match against its own classes, so a mismatch is
// a refused comparison, never a wrong one.

#include "bpr/core/diff_line_matcher.hpp"

extern "C" {
#include "bpr-xdiff-regex.h"
#include "xdiff.h"
}

#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace bpr::core {
	namespace {
		/// One hunk of a zero-context diff, zero-based: left lines
		/// [OldBegin, OldBegin + OldCount) are replaced by right lines
		/// [NewBegin, NewBegin + NewCount).
		struct line_hunk
		{
			long OldBegin = 0;
			long OldCount = 0;
			long NewBegin = 0;
			long NewCount = 0;
		};

		extern "C" auto collect_hunk(const long aOldBegin, const long aOldCount, const long aNewBegin, const long aNewCount, void* aData) -> int
		{
			auto* hunks = static_cast<std::vector<line_hunk>*>(aData);
			try {
				hunks->push_back({ .OldBegin = aOldBegin, .OldCount = aOldCount, .NewBegin = aNewBegin, .NewCount = aNewCount });
			} catch (...) {
				return -1;
			}
			return 0;
		}

		/// The stand-in for a final line, without an ending, that normalizes to
		/// nothing. The engine accepts only valid UTF-8, where 0xFF never
		/// occurs.
		inline constexpr auto empty_final_line = std::uint8_t{ 0xFFU };

		[[nodiscard]] constexpr auto is_blank(const std::uint8_t aByte) noexcept -> bool
		{
			return aByte == 0x20U || aByte == 0x09U;
		}

		[[nodiscard]] auto line_count(const decoded_text_document* aDocument) noexcept -> std::size_t
		{
			return aDocument == nullptr ? 0U : aDocument->Lines.size();
		}

		/// The bytes xdiff compares for one side: the document as it is under
		/// exact comparison, otherwise every line in the engine's normalized
		/// form, one record per engine line. Spaces and tabs are single bytes in UTF-8 and never part of a
		/// longer sequence, so removing them bytewise is removing them as
		/// scalars.
		[[nodiscard]] auto comparison_bytes(const decoded_text_document* aDocument, const diff_options& aOptions,
											std::vector<std::uint8_t>& aStorage) -> std::span<const std::uint8_t>
		{
			if (aDocument == nullptr) {
				return { };
			}
			if (aOptions.Whitespace == whitespace_policy::exact && aOptions.LineEndings == line_ending_policy::exact) {
				return aDocument->Bytes;
			}
			aStorage.clear();
			aStorage.reserve(aDocument->Bytes.size());
			const auto bytes = std::span<const std::uint8_t>{ aDocument->Bytes };
			for (const auto& line : aDocument->Lines) {
				const auto content = bytes.subspan(static_cast<std::size_t>(line.Content.Begin), static_cast<std::size_t>(line.Content.End - line.Content.Begin));
				const auto start   = aStorage.size();
				for (const auto byte : content) {
					if (aOptions.Whitespace == whitespace_policy::ignore_all && is_blank(byte)) {
						continue;
					}
					aStorage.push_back(byte);
				}
				if (aOptions.Whitespace == whitespace_policy::ignore_trailing) {
					while (aStorage.size() > start && is_blank(aStorage.back())) {
						aStorage.pop_back();
					}
				}
				switch (line.Ending) {
				case text_line_ending::none:
					// Only a final line has no ending. Normalized to nothing it
					// would vanish, leaving xdiff one record short of the
					// engine's lines; it is given a byte valid UTF-8 never
					// holds instead, so it equals only another such line.
					if (aStorage.size() == start) {
						aStorage.push_back(empty_final_line);
					}
					break;
				case text_line_ending::lf:
					aStorage.push_back('\n');
					break;
				case text_line_ending::crlf:
					if (aOptions.LineEndings == line_ending_policy::exact) {
						aStorage.push_back('\r');
					}
					aStorage.push_back('\n');
					break;
				}
			}
			return aStorage;
		}

		/// The matched lines between and around the hunks, or nothing when the
		/// hunks do not tile both sides.
		[[nodiscard]] auto matches_from_hunks(const std::vector<line_hunk>& aHunks, const std::size_t aLeftLines, const std::size_t aRightLines)
			-> std::optional<std::vector<diff_match>>
		{
			auto result = std::vector<diff_match>{ };
			auto left   = std::size_t{ 0U };
			auto right  = std::size_t{ 0U };
			for (const auto& hunk : aHunks) {
				if (hunk.OldBegin < 0 || hunk.OldCount < 0 || hunk.NewBegin < 0 || hunk.NewCount < 0) {
					return std::nullopt;
				}
				const auto oldBegin = static_cast<std::size_t>(hunk.OldBegin);
				const auto newBegin = static_cast<std::size_t>(hunk.NewBegin);
				if (oldBegin < left || newBegin < right || oldBegin - left != newBegin - right) {
					return std::nullopt;
				}
				while (left < oldBegin) {
					result.push_back({ .Left = static_cast<std::uint32_t>(left++), .Right = static_cast<std::uint32_t>(right++) });
				}
				left += static_cast<std::size_t>(hunk.OldCount);
				right += static_cast<std::size_t>(hunk.NewCount);
			}
			if (left > aLeftLines || right > aRightLines || aLeftLines - left != aRightLines - right) {
				return std::nullopt;
			}
			while (left < aLeftLines) {
				result.push_back({ .Left = static_cast<std::uint32_t>(left++), .Right = static_cast<std::uint32_t>(right++) });
			}
			return result;
		}

		class xdiff_line_matcher final : public diff_line_matcher_port
		{
		public:
			[[nodiscard]] auto match(const diff_line_match_input& aInput) -> std::optional<std::vector<diff_match>> override
			{
				auto       leftStorage  = std::vector<std::uint8_t>{ };
				auto       rightStorage = std::vector<std::uint8_t>{ };
				const auto leftBytes    = comparison_bytes(aInput.Left, aInput.Options, leftStorage);
				const auto rightBytes   = comparison_bytes(aInput.Right, aInput.Options, rightStorage);
				if (leftBytes.size() > static_cast<std::size_t>(LONG_MAX) || rightBytes.size() > static_cast<std::size_t>(LONG_MAX)) {
					return std::nullopt;
				}
				// Myers, the indent heuristic, and no whitespace flag.
				auto parameters         = xpparam_t{ };
				parameters.flags        = XDF_INDENT_HEURISTIC;
				auto configuration      = xdemitconf_t{ };
				configuration.hunk_func = &collect_hunk;
				auto hunks              = std::vector<line_hunk>{ };
				auto callback           = xdemitcb_t{ };
				callback.priv           = &hunks;
				// xdiff reads the buffers and never writes them.
				auto left  = mmfile_t{ .ptr = const_cast<char*>(reinterpret_cast<const char*>(leftBytes.data())), .size = static_cast<long>(leftBytes.size()) };
				auto right = mmfile_t{ .ptr = const_cast<char*>(reinterpret_cast<const char*>(rightBytes.data())), .size = static_cast<long>(rightBytes.size()) };
				if (xdl_diff(&left, &right, &parameters, &configuration, &callback) < 0) {
					return std::nullopt;
				}
				return matches_from_hunks(hunks, line_count(aInput.Left), line_count(aInput.Right));
			}
		};
	} // namespace

	auto make_xdiff_line_matcher() -> std::shared_ptr<diff_line_matcher_port>
	{
		return std::make_shared<xdiff_line_matcher>();
	}
} // namespace bpr::core
