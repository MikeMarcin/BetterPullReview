#include "bpr/core/line_endings.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace bpr::core {
	namespace {
		/// \brief One line of a body and the break that ends it ("" for the last).
		struct original_line
		{
			std::string_view Text;
			std::string_view Ending;
		};

		[[nodiscard]] auto original_lines(const std::string_view aBody) -> std::vector<original_line>
		{
			auto lines = std::vector<original_line>{ };
			auto start = std::size_t{ 0U };
			while (true) {
				const auto newline = aBody.find('\n', start);
				if (newline == std::string_view::npos) {
					lines.push_back({ aBody.substr(start), { } });
					return lines;
				}
				const auto crlf = newline > start && aBody[newline - 1U] == '\r';
				const auto end  = crlf ? newline - 1U : newline;
				lines.push_back({ aBody.substr(start, end - start), aBody.substr(end, newline + 1U - end) });
				start = newline + 1U;
			}
		}

		[[nodiscard]] auto edited_lines(const std::string_view aText) -> std::vector<std::string_view>
		{
			auto lines = std::vector<std::string_view>{ };
			auto start = std::size_t{ 0U };
			while (true) {
				const auto newline = aText.find('\n', start);
				if (newline == std::string_view::npos) {
					lines.push_back(aText.substr(start));
					return lines;
				}
				lines.push_back(aText.substr(start, newline - start));
				start = newline + 1U;
			}
		}
	} // namespace

	auto restore_line_endings(const std::string_view aEdited, const std::string_view aOriginal) -> std::string
	{
		const auto original = original_lines(aOriginal);
		const auto edited   = edited_lines(aEdited);
		auto       crlf     = std::size_t{ 0U };
		auto       lf       = std::size_t{ 0U };
		for (const auto& line : original) {
			crlf += line.Ending == "\r\n" ? 1U : 0U;
			lf += line.Ending == "\n" ? 1U : 0U;
		}
		const auto dominant = crlf > 0U && crlf >= lf ? std::string_view{ "\r\n" } : std::string_view{ "\n" };
		// The lines the edit left alone, from each end. The suffix never
		// overlaps the prefix, so an inserted line between equal neighbours
		// is attributed to the middle.
		const auto common = std::min(original.size(), edited.size());
		auto       prefix = std::size_t{ 0U };
		while (prefix < common && edited[prefix] == original[prefix].Text) {
			++prefix;
		}
		auto suffix = std::size_t{ 0U };
		while (suffix < common - prefix && edited[edited.size() - 1U - suffix] == original[original.size() - 1U - suffix].Text) {
			++suffix;
		}
		auto result = std::string{ };
		result.reserve(aEdited.size() + edited.size());
		for (std::size_t index = 0U; index < edited.size(); ++index) {
			result.append(edited[index]);
			if (index + 1U == edited.size()) {
				break;
			}
			auto ending = std::string_view{ };
			if (index < prefix) {
				ending = original[index].Ending;
			} else if (index >= edited.size() - suffix) {
				ending = original[original.size() - (edited.size() - index)].Ending;
			}
			result.append(ending.empty() ? dominant : ending);
		}
		return result;
	}
} // namespace bpr::core
