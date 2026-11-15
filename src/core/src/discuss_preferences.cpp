#include "bpr/core/discuss_preferences.hpp"

#include <algorithm>
#include <cstddef>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto folded(const char aCharacter) noexcept -> char
		{
			return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter - 'A' + 'a') : aCharacter;
		}

		[[nodiscard]] auto fold(const std::string_view aText) -> std::string
		{
			auto result = std::string{ aText };
			std::ranges::transform(result, result.begin(), folded);
			return result;
		}

		[[nodiscard]] auto storable(const std::string_view aKey) noexcept -> bool
		{
			return !aKey.empty() && aKey.size() <= maximum_mark_key_bytes
				&& std::ranges::none_of(aKey, [](const char aCharacter) { return static_cast<unsigned char>(aCharacter) < 0x20U; });
		}
	}

	auto github_bot_login(const std::string_view aLogin) noexcept -> bool
	{
		constexpr auto suffix = std::string_view{ "[bot]" };
		if (aLogin.size() <= suffix.size()) {
			return false;
		}
		const auto tail = aLogin.substr(aLogin.size() - suffix.size());
		return std::ranges::equal(tail, suffix, [](const char aLeft, const char aRight) { return folded(aLeft) == aRight; });
	}

	auto author_mark_key(const std::string_view aLogin) -> std::string
	{
		return fold(aLogin);
	}

	auto repository_mark_key(const std::string_view aHost, const std::string_view aOwner, const std::string_view aName) -> std::string
	{
		if (aHost.empty() || aOwner.empty() || aName.empty()) {
			return { };
		}
		return fold(aHost) + '/' + fold(aOwner) + '/' + fold(aName);
	}

	auto author_is_bot(const std::string_view aLogin, const author_bot_marks& aMarks) -> bool
	{
		if (const auto found = aMarks.find(author_mark_key(aLogin)); found != aMarks.end()) {
			return found->second;
		}
		return github_bot_login(aLogin);
	}

	auto marks_of(const discuss_preferences& aPreferences, const std::string_view aRepository) -> const author_bot_marks&
	{
		static const auto none  = author_bot_marks{ };
		const auto        found = aPreferences.Marks.find(aRepository);
		return found == aPreferences.Marks.end() ? none : found->second;
	}

	auto mark_author(discuss_preferences aPreferences, const std::string_view aRepository, const std::string_view aLogin, const bool aBot) -> discuss_preferences
	{
		const auto login = author_mark_key(aLogin);
		if (!storable(aRepository) || !storable(login)) {
			return aPreferences;
		}
		auto repository = aPreferences.Marks.find(aRepository);
		if (aBot == github_bot_login(login)) {
			if (repository != aPreferences.Marks.end()) {
				repository->second.erase(login);
				if (repository->second.empty()) {
					aPreferences.Marks.erase(repository);
				}
			}
			return aPreferences;
		}
		if (repository == aPreferences.Marks.end()) {
			repository = aPreferences.Marks.emplace(std::string{ aRepository }, author_bot_marks{ }).first;
		}
		repository->second.insert_or_assign(login, aBot);
		return aPreferences;
	}

	auto read_discuss_preferences(const std::optional<stored_discuss_preferences>& aStored) -> discuss_preferences_read
	{
		auto result = discuss_preferences_read{ };
		if (!aStored.has_value()) {
			return result;
		}
		if (aStored->Version > discuss_preferences_version) {
			result.Writable = false;
			return result;
		}
		if (aStored->Version < 1) {
			result.Recovered = true;
			return result;
		}
		if (aStored->Column >= 0 && aStored->Column <= static_cast<std::int64_t>(discuss_column::full)) {
			result.Preferences.Column = static_cast<discuss_column>(aStored->Column);
		} else {
			result.Recovered = true;
		}
		for (const auto& mark : aStored->Marks) {
			const auto login = author_mark_key(mark.Login);
			if (!storable(mark.Repository) || !storable(login) || (mark.Bot != 0 && mark.Bot != 1) || mark.Repository != fold(mark.Repository)) {
				result.Recovered = true;
				continue;
			}
			result.Preferences = mark_author(std::move(result.Preferences), mark.Repository, login, mark.Bot == 1);
		}
		return result;
	}

	auto stored_discuss_preferences_of(const discuss_preferences& aPreferences) -> stored_discuss_preferences
	{
		auto stored   = stored_discuss_preferences{ };
		stored.Column = static_cast<std::int64_t>(aPreferences.Column);
		for (const auto& [repository, marks] : aPreferences.Marks) {
			for (const auto& [login, bot] : marks) {
				stored.Marks.push_back(stored_author_mark{ .Repository = repository, .Login = login, .Bot = bot ? 1 : 0 });
			}
		}
		return stored;
	}
}
