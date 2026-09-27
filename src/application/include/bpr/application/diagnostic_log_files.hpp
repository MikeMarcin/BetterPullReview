#ifndef BPR_APPLICATION_DIAGNOSTIC_LOG_FILES_HPP
#define BPR_APPLICATION_DIAGNOSTIC_LOG_FILES_HPP
#include <algorithm>
#include <string_view>
namespace bpr::application {
	/// \brief Closed leaf names shared by diagnostic storage and explicit maintenance.
	inline auto diagnostic_log_file(std::string_view aName) noexcept -> bool
	{
		if (aName == "diagnostics.log") {
			return true;
		}
		if (aName.size() != 55U || !aName.starts_with("session-") || aName[40] != '-' || !aName.ends_with(".jsonl")) {
			return false;
		}
		const auto hex = [](char aCharacter) { return (aCharacter >= '0' && aCharacter <= '9') || (aCharacter >= 'a' && aCharacter <= 'f'); };
		return std::ranges::all_of(aName.substr(8U, 32U), hex) && std::ranges::all_of(aName.substr(41U, 8U), hex);
	}
}
#endif
