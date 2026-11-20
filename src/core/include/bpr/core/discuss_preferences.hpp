#ifndef BPR_CORE_DISCUSS_PREFERENCES_HPP
#define BPR_CORE_DISCUSS_PREFERENCES_HPP

/// \file
/// \brief [[BPR-2171]]. Who Discuss treats as a bot: the profile's Discuss
/// preferences.
///
/// \details The owner, 2026-11-13, on folding bot comments: "I dunno how you
/// detect bots [...] Do we need the ability to mark a user as a bot and store
/// it as a repo setting?" GitHub decides first: a GitHub App's account is a
/// `Bot`, which the read holds as `<name>[bot]` ([[BPR-1321]]). Any author
/// can then be marked as a bot, or as not one, in one repository; the mark
/// is kept on this computer and is the answer there.
///
/// Discuss's list is a centred column as wide as the Reading width
/// ([[BPR-2299]]); the column setting it had of its own is gone.
///
/// Pure: equal inputs give equal answers.

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief Whether GitHub says the login is an App's: it ends in `[bot]`,
	/// in any ASCII case.
	[[nodiscard]] auto github_bot_login(std::string_view aLogin) noexcept -> bool;

	/// \brief One repository's marks, by login folded to ASCII lower case
	/// (GitHub logins compare so): true is Mark as bot, false is Not a bot.
	using author_bot_marks = std::map<std::string, bool, std::less<>>;

	/// \brief The key a login is marked under: folded to ASCII lower case.
	[[nodiscard]] auto author_mark_key(std::string_view aLogin) -> std::string;

	/// \brief The key a repository's marks are kept under:
	/// `host/owner/name`, folded to ASCII lower case.
	[[nodiscard]] auto repository_mark_key(std::string_view aHost, std::string_view aOwner, std::string_view aName) -> std::string;

	/// \brief Whether Discuss treats the author as a bot: the repository's
	/// mark when there is one, otherwise what GitHub says.
	[[nodiscard]] auto author_is_bot(std::string_view aLogin, const author_bot_marks& aMarks) -> bool;

	/// \brief The profile's Discuss preferences.
	struct discuss_preferences
	{
		/// Each repository's marks, by `repository_mark_key`.
		std::map<std::string, author_bot_marks, std::less<>> Marks;

		auto operator==(const discuss_preferences&) const -> bool = default;
	};

	/// \brief The marks of one repository; empty when it has none.
	[[nodiscard]] auto marks_of(const discuss_preferences& aPreferences, std::string_view aRepository) -> const author_bot_marks&;

	/// \brief The preferences after Mark as bot (\p aBot) or Not a bot.
	/// \details A mark that says what GitHub already says is no mark: it is
	/// removed, so the author follows GitHub again. A repository left with no
	/// mark is removed. An empty login or repository changes nothing.
	[[nodiscard]] auto mark_author(discuss_preferences aPreferences, std::string_view aRepository, std::string_view aLogin, bool aBot) -> discuss_preferences;

	/// \brief The stored form's version this build reads and writes.
	inline constexpr std::int64_t discuss_preferences_version = 1;
	/// \brief The longest repository key and login a store keeps.
	inline constexpr std::size_t maximum_mark_key_bytes = 512U;

	/// \brief One stored mark.
	struct stored_author_mark
	{
		std::string  Repository;
		std::string  Login;
		std::int64_t Bot = 0;

		auto operator==(const stored_author_mark&) const -> bool = default;
	};

	/// \brief The stored preferences as a store reads them.
	struct stored_discuss_preferences
	{
		std::int64_t                    Version = discuss_preferences_version;
		std::vector<stored_author_mark> Marks;

		auto operator==(const stored_discuss_preferences&) const -> bool = default;
	};

	/// \brief What reading the store gave.
	struct discuss_preferences_read
	{
		discuss_preferences Preferences;
		/// Something stored could not be read and was left out.
		bool Recovered = false;
		/// False when a newer build wrote the store: it reads as nothing
		/// chosen and is kept as it is.
		bool Writable = true;

		auto operator==(const discuss_preferences_read&) const -> bool = default;
	};

	/// \brief Reads the stored preferences; none when nothing is stored.
	/// \details Each mark is checked on its own: one that cannot be read is
	/// left out and the read reports it recovered.
	[[nodiscard]] auto read_discuss_preferences(const std::optional<stored_discuss_preferences>& aStored) -> discuss_preferences_read;

	/// \brief What this build stores for the preferences, marks in key order.
	[[nodiscard]] auto stored_discuss_preferences_of(const discuss_preferences& aPreferences) -> stored_discuss_preferences;
}

#endif
