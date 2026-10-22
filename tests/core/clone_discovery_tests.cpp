/// \file
/// \brief [[BPR-PENDING-AQ3]], RFC-BPR-0092 owner decision D2. Reading a
/// clone's remotes from `.git/config` text, normalizing their URLs without
/// user information, matching a pull request's repository and ordering the
/// matches, without any file.

#include "bpr/core/clone_discovery.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using core::git_remote;
	using core::remote_location;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_config_remotes()
	{
		const auto config  = std::string{ "[core]\n\trepositoryformatversion = 0\n\tbare = false\n"
										  "[remote \"origin\"]\n\turl = git@github.com:bpr-fixtures/engine.git\n\tfetch = +refs/heads/*:refs/remotes/origin/*\n"
										  "[Remote \"up\\\"stream\"]\n\tURL = \"https://github.com/Upstream/Engine\" # the upstream\n"
										  "[include]\n\tpath = ~/.gitconfig-work\n"
										  "[branch \"main\"]\n\tremote = origin\n\turl = https://example.com/not/a-remote\n"
										  "[remote \"broken\"]\n\turl = \"unterminated\n"
										  "; a comment\n[remote \"spaced\"]\n  url   =   https://ghe.example.com/a/b   ; trailing\n" };
		const auto remotes = core::parse_git_config_remotes(config);
		expect(remotes
				   == std::vector<git_remote>{ { .Name = "origin", .Url = "git@github.com:bpr-fixtures/engine.git" },
											   { .Name = "up\"stream", .Url = "https://github.com/Upstream/Engine" },
											   { .Name = "spaced", .Url = "https://ghe.example.com/a/b" } },
			   "remote URLs in order, quoted and commented values read as Git reads them, other sections and an unterminated value skipped");
		constexpr char nulText[] = "[remote \"origin\"]\nurl = https://github.com/a\0b/c\n";
		const auto     withNul   = std::string_view{ nulText, sizeof(nulText) - 1U };
		expect(core::parse_git_config_remotes(withNul).empty(), "a URL with a NUL in it is not kept");
		auto many = std::string{ };
		for (auto index = 0; index < 100; ++index) {
			many += "[remote \"r" + std::to_string(index) + "\"]\nurl = https://github.com/o/n\n";
		}
		expect(core::parse_git_config_remotes(many).size() == core::maximum_clone_remotes, "no more than the remote bound is read");
		auto past = std::string(core::maximum_clone_config_bytes, '\n') + "[remote \"late\"]\nurl = https://github.com/o/n\n";
		expect(core::parse_git_config_remotes(past).empty(), "text past the size bound is not read");
		expect(core::parse_git_config_remotes("[remote \"x\"]\nurl = https://github.com/o/" + std::string(core::maximum_remote_text_bytes, 'n') + "\n").empty(),
			   "an over-long URL is not kept");
	}

	void test_urls()
	{
		const auto at     = [](const std::string_view aUrl) { return core::normalize_remote_url(aUrl); };
		const auto engine = remote_location{ .Host = "github.com", .Owner = "bpr-fixtures", .Name = "engine" };
		expect(at("https://github.com/bpr-fixtures/engine.git") == engine, "HTTPS with .git");
		expect(at("https://github.com/bpr-fixtures/engine/") == engine, "HTTPS with a trailing slash");
		expect(at("git@github.com:bpr-fixtures/engine.git") == engine, "scp-like SSH");
		expect(at("ssh://git@github.com:22/bpr-fixtures/engine") == engine, "SSH URL with user and port");
		expect(at("https://ghp_token123@GitHub.com/bpr-fixtures/engine") == engine, "a token in the URL is dropped and the host lowered");
		// Assembled here so no credential-shaped URL is written in the source.
		const auto withPassword = std::string{ "https://user" } + ":" + "placeholder" + "@github.com/bpr-fixtures/engine";
		expect(at(withPassword) == engine, "user and password are dropped");
		expect(at("git://ghe.example.com/team/tool")->Host == "ghe.example.com", "a git URL");
		expect(!at("/Users/me/src/engine"), "a local path is no location");
		expect(!at("file:///Users/me/src/engine"), "a file URL is no location");
		expect(!at("C:/repos/engine"), "a Windows drive path is no location");
		expect(!at("https://github.com/only-owner"), "a path that is not owner/name");
		expect(!at("https://github.com/a/b/c"), "a deeper path is not a GitHub repository");
		expect(!at("https://github.com/a/../b"), "dot segments are refused");
		expect(!at("https://github.com/a\n/b"), "control characters are refused");
		expect(!at("ftp://github.com/a/b"), "other schemes are refused");
		expect(!at(""), "empty is nothing");
	}

	void test_head()
	{
		expect(core::head_branch("ref: refs/heads/main\n") == "main", "a branch HEAD");
		expect(core::head_branch("ref: refs/heads/spike/retry") == "spike/retry", "a branch with a slash");
		expect(core::head_branch("7f3a9c1a0b9d8e7f6a5b4c3d2e1f0a9b8c7d6e5f\n").empty(), "a detached HEAD names no branch");
		expect(core::head_branch("garbage").empty(), "anything else names no branch");
	}

	void test_matching()
	{
		const auto candidate = [](std::string aPath, std::string aFolder, std::vector<git_remote> aRemotes) {
			auto value    = core::clone_candidate{ };
			value.Path    = std::move(aPath);
			value.Folder  = std::move(aFolder);
			value.Remotes = std::move(aRemotes);
			value.Branch  = "main";
			return value;
		};
		const auto folders    = std::vector<std::string>{ "/Users/me/Documents/GitHub", "/Users/me/src" };
		const auto candidates = std::vector{
			candidate("/Users/me/src/engine-old", "/Users/me/src", { { .Name = "origin", .Url = "https://github.com/someone/fork" }, { .Name = "upstream", .Url = "git@github.com:BPR-Fixtures/Engine.git" } }),
			candidate("/Users/me/Documents/GitHub/engine", "/Users/me/Documents/GitHub", { { .Name = "origin", .Url = "https://token@github.com/bpr-fixtures/engine" } }),
			candidate("/Users/me/Documents/GitHub/other", "/Users/me/Documents/GitHub", { { .Name = "origin", .Url = "https://github.com/bpr-fixtures/other" } }),
			candidate("/Users/me/src/ghes-engine", "/Users/me/src", { { .Name = "origin", .Url = "https://ghe.example.com/bpr-fixtures/engine" } }),
			candidate("/Users/me/Documents/GitHub/engine", "/Users/me/Documents/GitHub", { { .Name = "origin", .Url = "https://github.com/bpr-fixtures/engine" } }),
		};
		const auto target  = core::clone_target{ .Host = "GitHub.com", .Owner = "bpr-fixtures", .Name = "engine" };
		const auto matches = core::match_clones(candidates, target, folders, "");
		expect(matches.size() == 2U, "the two clones that name the repository; another repository and another host do not match; a path is listed once");
		expect(matches[0].Path == "/Users/me/Documents/GitHub/engine" && matches[1].Path == "/Users/me/src/engine-old", "ordered by the listed folders' order");
		expect(matches[0].Location == "github.com/bpr-fixtures/engine" && matches[0].Remote == "origin", "the location shown carries no user information");
		expect(matches[1].Remote == "upstream", "a clone matched by upstream only says so");
		const auto linked = core::match_clones(candidates, target, folders, "/Users/me/src/engine-old");
		expect(linked.front().Path == "/Users/me/src/engine-old", "the previously linked clone comes first");
		const auto ghes = core::match_clones(candidates, core::clone_target{ .Host = "ghe.example.com", .Owner = "bpr-fixtures", .Name = "engine" }, folders, "");
		expect(ghes.size() == 1U && ghes.front().Path == "/Users/me/src/ghes-engine", "a GHES repository matches only its own host");
	}

	void test_folders()
	{
		expect(core::valid_clone_folder("/Users/me/src", false), "an absolute POSIX folder");
		expect(!core::valid_clone_folder("src", false) && !core::valid_clone_folder("~/src", false), "relative and ~ folders are refused");
		expect(!core::valid_clone_folder("/a\nb", false), "a control character is refused");
		expect(core::valid_clone_folder(R"(C:\Users\me\src)", true) && core::valid_clone_folder(R"(\\server\share)", true), "Windows drive and UNC folders");
		expect(!core::valid_clone_folder(R"(C:relative)", true), "a drive-relative folder is refused");
		expect(core::usual_clone_folders("/Users/me", false)
				   == std::vector<std::string>{ "/Users/me/Documents/GitHub", "/Users/me/Developer", "/Users/me/src", "/Users/me/code", "/Users/me/Projects" },
			   "the usual folders under home");
		expect(core::usual_clone_folders(R"(C:\Users\me)", true).front() == R"(C:\Users\me\Documents\GitHub)", "Windows separators");
		expect(core::usual_clone_folders("relative", false).empty(), "no suggestions without an absolute home");
	}
}

auto main() -> int
{
	test_config_remotes();
	test_urls();
	test_head();
	test_matching();
	test_folders();
	std::cout << "clone-discovery-tests:pass\n";
	return EXIT_SUCCESS;
}
