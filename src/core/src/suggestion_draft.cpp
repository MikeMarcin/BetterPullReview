#include "bpr/core/suggestion_draft.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>

namespace bpr::core {
	namespace {
		/// \brief The body's lines, split on `\n`, each without a trailing `\r`.
		[[nodiscard]] auto lines_of(const std::string_view aText) -> std::vector<std::string_view>
		{
			auto lines = std::vector<std::string_view>{ };
			auto start = std::size_t{ 0U };
			while (true) {
				const auto end  = aText.find('\n', start);
				auto       line = aText.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
				if (line.ends_with('\r')) {
					line.remove_suffix(1U);
				}
				lines.push_back(line);
				if (end == std::string_view::npos) {
					return lines;
				}
				start = end + 1U;
			}
		}

		/// \brief How many of `aMarker` a line starts with, after up to three spaces.
		[[nodiscard]] auto fence_run(const std::string_view aLine, const char aMarker) -> std::size_t
		{
			auto at = std::size_t{ 0U };
			while (at < aLine.size() && at < 3U && aLine[at] == ' ') {
				++at;
			}
			auto run = std::size_t{ 0U };
			while (at + run < aLine.size() && aLine[at + run] == aMarker) {
				++run;
			}
			return run;
		}

		struct fence_opening
		{
			char        Marker = '`';
			std::size_t Run    = 0U;
		};

		[[nodiscard]] auto suggestion_opening(const std::string_view aLine) -> std::optional<fence_opening>
		{
			for (const auto marker : { '`', '~' }) {
				const auto run = fence_run(aLine, marker);
				if (run < 3U) {
					continue;
				}
				auto info = aLine.substr(aLine.find(marker) + run);
				while (!info.empty() && (info.front() == ' ' || info.front() == '\t')) {
					info.remove_prefix(1U);
				}
				if (info.starts_with("suggestion")) {
					const auto rest = info.substr(std::string_view{ "suggestion" }.size());
					if (rest.empty() || rest.front() == ' ' || rest.front() == '\t') {
						return fence_opening{ .Marker = marker, .Run = run };
					}
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto closes(const std::string_view aLine, const fence_opening& aOpening) -> bool
		{
			const auto run = fence_run(aLine, aOpening.Marker);
			if (run < aOpening.Run) {
				return false;
			}
			const auto start = aLine.find(aOpening.Marker);
			return aLine.find_first_not_of(" \t", start + run) == std::string_view::npos;
		}
	} // namespace

	auto suggestion_source_lines(const decoded_text_document& aDocument, const std::uint32_t aFirst, const std::uint32_t aLast)
		-> std::optional<std::vector<std::string>>
	{
		if (aFirst > aLast || aLast >= aDocument.Lines.size()) {
			return std::nullopt;
		}
		auto lines = std::vector<std::string>{ };
		lines.reserve(static_cast<std::size_t>(aLast - aFirst) + 1U);
		for (auto index = aFirst; index <= aLast; ++index) {
			const auto& span = aDocument.Lines[index].Content;
			if (span.Begin > span.End || span.End > aDocument.Bytes.size()) {
				return std::nullopt;
			}
			const auto begin = aDocument.Bytes.begin() + static_cast<std::ptrdiff_t>(span.Begin);
			const auto end   = aDocument.Bytes.begin() + static_cast<std::ptrdiff_t>(span.End);
			auto&      line  = lines.emplace_back();
			line.reserve(static_cast<std::size_t>(span.End - span.Begin));
			std::ranges::transform(begin, end, std::back_inserter(line), [](const std::uint8_t aByte) { return static_cast<char>(aByte); });
		}
		return lines;
	}

	auto suggestion_fence_body(const std::span<const std::string> aLines) -> std::string
	{
		auto longest = std::size_t{ 2U };
		for (const auto& line : aLines) {
			longest = std::max(longest, fence_run(line, '`'));
		}
		const auto fence = std::string(longest + 1U, '`');
		auto       body  = fence + "suggestion\n";
		for (const auto& line : aLines) {
			body += line;
			body += '\n';
		}
		body += fence;
		return body;
	}

	auto suggested_change_of(const std::string_view aBody, const std::span<const std::string> aOriginal) -> suggested_change
	{
		const auto lines   = lines_of(aBody);
		auto       opening = std::optional<fence_opening>{ };
		auto       first   = lines.size();
		for (auto index = std::size_t{ 0U }; index < lines.size(); ++index) {
			opening = suggestion_opening(lines[index]);
			if (opening.has_value()) {
				first = index + 1U;
				break;
			}
		}
		if (!opening.has_value()) {
			return { };
		}
		auto last = first;
		while (last < lines.size() && !closes(lines[last], *opening)) {
			++last;
		}
		const auto block = std::span{ lines }.subspan(first, last - first);
		auto       head  = std::size_t{ 0U };
		while (head < block.size() && head < aOriginal.size() && block[head] == aOriginal[head]) {
			++head;
		}
		auto tail = std::size_t{ 0U };
		while (tail < block.size() - head && tail < aOriginal.size() - head
			   && block[block.size() - 1U - tail] == aOriginal[aOriginal.size() - 1U - tail]) {
			++tail;
		}
		auto change = suggested_change{ .Fenced = true };
		change.Removed.assign(aOriginal.begin() + static_cast<std::ptrdiff_t>(head), aOriginal.end() - static_cast<std::ptrdiff_t>(tail));
		for (auto index = head; index < block.size() - tail; ++index) {
			change.Added.emplace_back(block[index]);
		}
		return change;
	}
} // namespace bpr::core
