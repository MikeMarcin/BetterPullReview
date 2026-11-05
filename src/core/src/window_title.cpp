#include "bpr/core/window_title.hpp"

namespace bpr::core {
	namespace {
		constexpr std::string_view ellipsis  = "\xE2\x80\xA6";
		constexpr std::string_view separator = " \xE2\x80\x94 ";
		/// How many characters back from the cut a space may be and still be
		/// cut at, so a single long word is not lost whole.
		constexpr std::size_t word_reach = 16U;

		[[nodiscard]] auto continuation(const char aByte) noexcept -> bool
		{
			return (static_cast<unsigned char>(aByte) & 0xC0U) == 0x80U;
		}

		[[nodiscard]] auto control(const char aByte) noexcept -> bool
		{
			const auto byte = static_cast<unsigned char>(aByte);
			return byte < 0x20U || byte == 0x7FU;
		}

		/// One line, its spaces collapsed and trimmed.
		[[nodiscard]] auto one_line(const std::string_view aText) -> std::string
		{
			auto line  = std::string{ };
			auto space = false;
			for (const auto byte : aText) {
				if (byte == ' ' || control(byte)) {
					space = !line.empty();
					continue;
				}
				if (space) {
					line += ' ';
					space = false;
				}
				line += byte;
			}
			return line;
		}

		[[nodiscard]] auto profile_words(const std::string_view aProfile) -> std::string
		{
			return std::string{ aProfile } + " profile";
		}
	} // namespace

	auto shorten_window_title_text(const std::string_view aText, const std::size_t aCharacters) -> std::string
	{
		if (aCharacters == 0U) {
			return { };
		}
		auto line = one_line(aText);
		// Counts the code points and finds where the last one that leaves
		// room for the ellipsis ends.
		auto characters = std::size_t{ 0U };
		auto cut        = line.size();
		for (std::size_t index = 0U; index < line.size(); ++index) {
			if (continuation(line[index])) {
				continue;
			}
			if (characters + 1U == aCharacters) {
				cut = index;
			}
			++characters;
		}
		if (characters <= aCharacters) {
			return line;
		}
		auto kept = std::string_view{ line }.substr(0U, cut);
		// Cut at the last word boundary when it is within reach, unless the
		// cut already falls between words.
		const auto between = line[cut] == ' ';
		if (const auto space = kept.rfind(' '); !between && space != std::string_view::npos && space > 0U) {
			auto reach = std::size_t{ 0U };
			for (auto index = space + 1U; index < kept.size(); ++index) {
				reach += continuation(kept[index]) ? 0U : 1U;
			}
			if (reach <= word_reach) {
				kept = kept.substr(0U, space);
			}
		}
		while (!kept.empty() && kept.back() == ' ') {
			kept.remove_suffix(1U);
		}
		return std::string{ kept } + std::string{ ellipsis };
	}

	auto window_title(const window_title_input& aInput) -> std::string
	{
		if (aInput.PullRequest.has_value() && aInput.PullRequest->Number != 0U) {
			const auto& pull  = *aInput.PullRequest;
			auto        title = "#" + std::to_string(pull.Number);
			if (const auto text = shorten_window_title_text(pull.Title, maximum_window_title_pull_request_characters); !text.empty()) {
				title += " " + text;
			}
			if (!pull.Owner.empty() && !pull.Repository.empty()) {
				title += std::string{ separator } + std::string{ pull.Owner } + "/" + std::string{ pull.Repository };
			}
			if (aInput.Profiles > 1U && !aInput.Profile.empty()) {
				title += std::string{ separator } + profile_words(aInput.Profile);
			}
			return title;
		}
		auto title = std::string{ window_title_application };
		if (!aInput.Mode.empty()) {
			title += std::string{ separator } + std::string{ aInput.Mode };
		}
		if (!aInput.Profile.empty()) {
			title += std::string{ separator } + profile_words(aInput.Profile);
		}
		return title;
	}
} // namespace bpr::core
