// [[BPR-2171]]. Who Discuss treats as a bot, the per-repository marks, and
// the column's width, with their stored form.
#include "bpr/core/discuss_preferences.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr::core;

	int Failures = 0;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			++Failures;
		}
	}

	void detection()
	{
		expect(github_bot_login("github-actions[bot]") && github_bot_login("Dependabot[BOT]"), "GitHub's App logins are bots, in any case");
		expect(!github_bot_login("[bot]") && !github_bot_login("botanist") && !github_bot_login("jiayin"), "anything else is not");
		const auto repository = repository_mark_key("github.com", "BPR-Fixtures", "Conversations");
		expect(repository == "github.com/bpr-fixtures/conversations", "a repository is keyed by host, owner and name, folded");
		expect(repository_mark_key("github.com", "", "x").empty(), "a repository without an owner has no key");
		auto preferences = mark_author({ }, repository, "Renovate-Helper", true);
		expect(author_is_bot("renovate-helper", marks_of(preferences, repository)), "Mark as bot makes an author a bot in that repository");
		expect(!author_is_bot("renovate-helper", marks_of(preferences, "github.com/other/repo")), "and nowhere else");
		preferences = mark_author(std::move(preferences), repository, "codecov[bot]", false);
		expect(!author_is_bot("Codecov[bot]", marks_of(preferences, repository)), "Not a bot overrides GitHub");
		const auto undone = mark_author(mark_author(preferences, repository, "codecov[bot]", true), repository, "renovate-helper", false);
		expect(undone.Marks.empty(), "a mark that says what GitHub says is no mark, and a repository with none is gone");
		expect(mark_author({ }, repository, "jiayin", false).Marks.empty(), "Not a bot on a person changes nothing");
		expect(mark_author({ }, "", "jiayin", true).Marks.empty() && mark_author({ }, repository, "", true).Marks.empty(), "no repository or login changes nothing");
	}

	void column()
	{
		expect(default_discuss_column == discuss_column::column && discuss_column_width(discuss_column::column) == 820U, "the column is 820 points by default");
		expect(!discuss_column_width(discuss_column::full).has_value(), "Full width has no limit");
	}

	void stored()
	{
		auto preferences   = mark_author({ }, "github.com/a/b", "Person", true);
		preferences        = mark_author(std::move(preferences), "github.com/a/b", "app[bot]", false);
		preferences.Column = discuss_column::wide;
		const auto row     = stored_discuss_preferences_of(preferences);
		expect(row.Column == 2 && row.Marks.size() == 2U && row.Marks.front().Login == "app[bot]" && row.Marks.front().Bot == 0, "stored in key order, logins folded");
		const auto read = read_discuss_preferences(row);
		expect(read.Preferences == preferences && !read.Recovered && read.Writable, "what is stored reads back");
		expect(read_discuss_preferences(std::nullopt) == discuss_preferences_read{ }, "nothing stored is nothing chosen");
		auto damaged = row;
		damaged.Marks.push_back(stored_author_mark{ .Repository = "github.com/a/b", .Login = "x", .Bot = 7 });
		damaged.Column = 9;
		const auto recovered = read_discuss_preferences(damaged);
		expect(recovered.Recovered && recovered.Preferences.Column == default_discuss_column && recovered.Preferences.Marks == preferences.Marks, "a damaged value is left out, the rest kept");
		auto newer    = row;
		newer.Version = discuss_preferences_version + 1;
		expect(!read_discuss_preferences(newer).Writable && read_discuss_preferences(newer).Preferences == discuss_preferences{ }, "a newer build's store is read as nothing and kept");
	}
}

auto main() -> int
{
	detection();
	column();
	stored();
	if (Failures != 0) {
		std::cerr << Failures << " expectation(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "DISCUSS_PREFERENCES_OK\n";
	return EXIT_SUCCESS;
}
