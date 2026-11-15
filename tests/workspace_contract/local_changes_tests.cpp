#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/git/installed_author_git.hpp"
#include "bpr/application/local_changes_host.hpp"
#include "bpr/core/diff_line_matcher.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

/// \file
/// \brief RFC-BPR-0113 and RFC-BPR-0114 as amended 2026-11-12 ([[BPR-2062]],
/// [[BPR-2065]], [[BPR-2066]]) on real repositories made by the build's Git,
/// through the author-Git runner and `local_changes_host`: the status lists
/// and the three comparisons; Stage Change and Unstage Change writing exactly
/// one change to the index; the one-time question and its declined path; a
/// commit with the repository's hooks, a refusing hook that leaves no
/// commit, Amend while GitHub lacks the commit; a push to a local bare
/// "hosted" repository, never forced; a push refused before anything runs
/// when the hosted branch has commits the branch lacks; Save's changed-on-disk
/// rule and exact bytes; the agent's commit message from the staged diff; and
/// (RFC-BPR-0114 AC06) a push whose pre-push hook hangs, stopped by the author
/// and timed out by its bound: each kills the hook, says why, reads GitHub
/// again and repeats nothing. [[BPR-2182]]: a URL and branch containing
/// "force" push while an added option or `+` refspec launches nothing, and
/// Stage and Unstage take bracketed, starred, questioned and `:`-led paths
/// literally.

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::write_file;

	class queue final : public application::effect_executor, public application::decision_executor
	{
	public:
		void                              post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		std::deque<std::function<void()>> Tasks;
	};

	class recorder final : public application::local_changes_port
	{
	public:
		void                                  publish(const application::local_changes_projection& aProjection) override { Last = aProjection; }
		application::local_changes_projection Last;
	};

	struct course
	{
		const git_runner&                                Git;
		std::filesystem::path                            Root;
		std::filesystem::path                            Hosted;
		std::filesystem::path                            Work;
		std::unique_ptr<application::author_git_port>    Author;
		std::unique_ptr<application::worktree_file_port> Files;
		queue                                            Effects;
		queue                                            Decisions;
		recorder                                         Port;
		std::vector<std::string>                         Allowed;
		std::string                                      Prompt;
		std::unique_ptr<application::local_changes_host> Host;

		void drain()
		{
			while (!Effects.Tasks.empty() || !Decisions.Tasks.empty()) {
				while (!Effects.Tasks.empty()) {
					auto task = std::move(Effects.Tasks.front());
					Effects.Tasks.pop_front();
					task();
				}
				while (!Decisions.Tasks.empty()) {
					auto task = std::move(Decisions.Tasks.front());
					Decisions.Tasks.pop_front();
					task();
				}
			}
		}

		[[nodiscard]] auto out(const std::filesystem::path& aDirectory, std::initializer_list<std::string_view> aArguments) const -> std::string
		{
			return Git.output(aDirectory, aArguments, Root / "out.txt");
		}

		[[nodiscard]] auto view() const -> const application::local_changes_projection& { return Port.Last; }
	};

	void commit_file(const git_runner& aGit, const std::filesystem::path& aDirectory, const std::string& aPath, const std::string& aText, const std::string& aMessage)
	{
		write_file(aDirectory / aPath, aText);
		aGit(aDirectory, { "add", aPath });
		aGit(aDirectory, { "commit", "--quiet", "-m", aMessage });
	}

	[[nodiscard]] auto paths(const std::vector<core::local_status_entry>& aEntries) -> std::string
	{
		auto text = std::string{ };
		for (const auto& entry : aEntries) {
			text += (text.empty() ? "" : ",") + entry.Path + ":" + core::badge(entry.Status);
		}
		return text;
	}

	void status_and_comparisons(course& aCourse)
	{
		const auto& view = aCourse.view();
		expect(view.Observed && view.Status.Branch == "fix/ports", "the branch is read");
		expect(paths(view.Status.Staged) == "docs/ports.md:A", "Staged: the added doc");
		expect(paths(view.Status.Unstaged) == "notes.txt:A,src/port.cpp:M", "Unstaged: the edit and the untracked file");
		const auto edit = std::ranges::find_if(view.Status.Unstaged, [](const auto& aEntry) { return aEntry.Path == "src/port.cpp"; });
		expect(edit->Added == 2U && edit->Removed == 2U, "line counts against the index");
		expect(view.Hosted.Observed && view.Hosted.Tip && view.Hosted.TipKnown && view.Hosted.Ahead == 1U && view.Hosted.Behind == 0U, "1 ahead of GitHub");
		expect(view.Push.State == core::local_push_state::ahead && view.Push.Offered && !view.Push.Default, "Push offered; Commit is the next step");
		expect(view.Command && view.Command->Arguments.back() == "refs/heads/fix/ports:refs/heads/fix/ports", "the exact push");
		expect(view.AmendOffered && !view.HeadOnHost, "Amend offered while GitHub lacks the last commit");
		expect(view.NotPushedAvailable && paths(view.NotPushed) == "docs/ports.md:A,notes.txt:A,src/limits.cpp:A,src/port.cpp:M", "Not pushed: the unpushed commit and every uncommitted file");
		expect(paths(view.WholePullRequest) == "docs/ports.md:A,notes.txt:A,src/limits.cpp:A,src/port.cpp:M,src/strict.cpp:A", "Whole PR: everything since the merge base");
	}

	void stage_one_change(course& aCourse)
	{
		aCourse.Host->open_file("src/port.cpp");
		aCourse.drain();
		const auto& file = aCourse.view().File;
		expect(file && file->Path == "src/port.cpp" && file->Diff, "the file's Uncommitted comparison");
		expect(file->Bands.size() == 2U && file->Bands[0].Stage && file->Bands[1].Stage && !file->Bands[0].Staged, "two unstaged changes");
		expect(!file->EditRefusal && !file->Disk.empty(), "the file is editable");
		const auto second = *file->Bands[1].Stage;
		aCourse.Host->stage_change("src/port.cpp", second);
		aCourse.drain();
		const auto cached = aCourse.out(aCourse.Work, { "diff", "--cached", "--no-color", "-U0", "--", "src/port.cpp" });
		expect(cached.find("+return value;") != std::string::npos && cached.find("+int parse") == std::string::npos, "only the second change is staged");
		expect(paths(aCourse.view().Status.Staged) == "docs/ports.md:A,src/port.cpp:M" && paths(aCourse.view().Status.Unstaged) == "notes.txt:A,src/port.cpp:M",
			   "a partly staged file is in both lists");
		const auto& bands = aCourse.view().File->Bands;
		expect(bands.size() == 2U && !bands[0].Staged && bands[1].Staged && bands[1].Unstage && !bands[1].Stage, "the band says Staged and offers Unstage Change");

		aCourse.Host->unstage_change("src/port.cpp", *bands[1].Unstage);
		aCourse.drain();
		expect(aCourse.out(aCourse.Work, { "diff", "--cached", "--name-only" }) == "docs/ports.md", "Unstage Change returns it");
		expect(read_file(aCourse.Work / "src" / "port.cpp").find("return value;") != std::string::npos, "the worktree never changes");

		aCourse.Host->stage({ "src/port.cpp", "notes.txt" });
		aCourse.drain();
		expect(paths(aCourse.view().Status.Staged) == "docs/ports.md:A,notes.txt:A,src/port.cpp:M" && aCourse.view().Status.Unstaged.empty(), "Stage moves files");
		aCourse.Host->unstage({ "notes.txt" });
		aCourse.drain();
		expect(paths(aCourse.view().Status.Unstaged) == "notes.txt:A", "Unstage moves one back");
	}

	void commit_with_hooks(course& aCourse)
	{
		const auto hooks = aCourse.Work / ".git" / "hooks";
		write_file(hooks / "pre-commit", "#!/bin/sh\necho ran > \"$(git rev-parse --git-dir)/pre-commit-ran\"\n");
		write_file(hooks / "commit-msg", "#!/bin/sh\nif grep -q REFUSE \"$1\"; then echo 'commit-msg: refused by policy' >&2; exit 1; fi\n");
		std::filesystem::permissions(hooks / "pre-commit", std::filesystem::perms::owner_all);
		std::filesystem::permissions(hooks / "commit-msg", std::filesystem::perms::owner_all);

		const auto before = aCourse.out(aCourse.Work, { "rev-parse", "HEAD" });
		aCourse.Host->commit({ .Subject = "REFUSE this", .Description = "" });
		aCourse.drain();
		expect(aCourse.view().ConsentNeeded, "the first Commit asks the one-time question");
		expect(aCourse.out(aCourse.Work, { "rev-parse", "HEAD" }) == before, "nothing ran before the answer");
		aCourse.Host->answer_consent(true);
		aCourse.drain();
		expect(aCourse.Allowed.size() == 1U, "the answer is remembered for the draft");
		expect(aCourse.view().Failure && aCourse.view().Failure->Operation == application::local_changes_operation::committing
				   && aCourse.view().Failure->Message.find("commit-msg: refused by policy") != std::string::npos,
			   "a refusing hook's output is shown");
		expect(aCourse.out(aCourse.Work, { "rev-parse", "HEAD" }) == before, "a refusing hook leaves no commit");

		aCourse.Host->commit({ .Subject = "Parse ports strictly", .Description = "The value is bounded." });
		aCourse.drain();
		expect(!aCourse.view().Failure && !aCourse.view().ConsentNeeded, "committed without asking again");
		expect(aCourse.out(aCourse.Work, { "log", "-1", "--format=%s%n%n%b" }) == "Parse ports strictly\n\nThe value is bounded.", "the exact message");
		expect(std::filesystem::exists(aCourse.Work / ".git" / "pre-commit-ran"), "the repository's pre-commit hook ran");
		expect(aCourse.out(aCourse.Work, { "show", "--name-only", "--format=", "HEAD" }) == "docs/ports.md\nsrc/port.cpp", "only what was staged is committed");
		expect(paths(aCourse.view().Status.Unstaged) == "notes.txt:A" && aCourse.view().Status.Staged.empty(), "the unstaged file is as it was");
		expect(aCourse.view().Hosted.Ahead == 2U && aCourse.view().AmendOffered, "2 ahead; Amend still offered");

		aCourse.Host->commit({ .Subject = "Parse ports strictly, bounded", .Description = "", .Amend = true });
		aCourse.drain();
		expect(aCourse.out(aCourse.Work, { "log", "-1", "--format=%s" }) == "Parse ports strictly, bounded" && aCourse.view().Hosted.Ahead == 2U, "Amend rewords the unpushed commit");
	}

	void push_and_refusal(course& aCourse)
	{
		const auto head = aCourse.out(aCourse.Work, { "rev-parse", "HEAD" });
		aCourse.Host->push();
		aCourse.drain();
		expect(!aCourse.view().Failure, "the push ran");
		expect(aCourse.out(aCourse.Hosted, { "rev-parse", "refs/heads/fix/ports" }) == head, "GitHub has the branch's head");
		expect(aCourse.view().Push.State == core::local_push_state::up_to_date && !aCourse.view().Push.Offered, "up to date after the push");
		expect(aCourse.view().HeadOnHost && !aCourse.view().AmendOffered, "Amend is withdrawn once GitHub has the commit");
		expect(aCourse.out(aCourse.Work, { "config", "--list", "--local" }).find("branch.fix/ports") == std::string::npos, "no upstream written");
		expect(aCourse.out(aCourse.Work, { "for-each-ref", "--format=%(refname)", "refs/remotes" }).find("fix/ports") == std::string::npos, "no remote-tracking ref moved by BPR");

		// Someone else pushes to the hosted branch.
		const auto other = aCourse.Root / "other";
		aCourse.Git(aCourse.Root, { "clone", "--quiet", "-b", "fix/ports", aCourse.Hosted.string(), other.string() });
		aCourse.Git(other, { "config", "user.name", "Other" });
		aCourse.Git(other, { "config", "user.email", "other@example.invalid" });
		commit_file(aCourse.Git, other, "src/other.cpp", "int other;\n", "Another author's commit");
		aCourse.Git(other, { "push", "--quiet", "origin", "fix/ports" });
		const auto hostedTip = aCourse.out(aCourse.Hosted, { "rev-parse", "refs/heads/fix/ports" });

		commit_file(aCourse.Git, aCourse.Work, "notes.txt", "notes\n", "Add notes");
		aCourse.Host->check_again();
		aCourse.drain();
		expect(aCourse.view().Push.State == core::local_push_state::unknown_tip && !aCourse.view().Push.Offered, "GitHub has a commit the branch lacks: Push refused");
		expect(aCourse.view().HeadOnHost && !aCourse.view().AmendOffered, "Amend refused while GitHub's tip is unknown");
		aCourse.Host->push();
		aCourse.drain();
		expect(aCourse.view().Failure && aCourse.view().Failure->RefusedBeforeRunning, "the refusal is said; nothing was sent");
		expect(aCourse.out(aCourse.Hosted, { "rev-parse", "refs/heads/fix/ports" }) == hostedTip, "GitHub's branch is unchanged");
	}

	/// An effect lane of its own thread, as the composition gives the author's
	/// Git, so Stop can be pressed while a push runs.
	class thread_lane final : public application::effect_executor
	{
	public:
		thread_lane()
			: Worker{ [this](const std::stop_token& aStop) { work(aStop); } }
		{
		}
		~thread_lane() override
		{
			Worker.request_stop();
			Changed.notify_all();
		}
		thread_lane(const thread_lane&)                    = delete;
		auto operator=(const thread_lane&) -> thread_lane& = delete;
		thread_lane(thread_lane&&)                         = delete;
		auto operator=(thread_lane&&) -> thread_lane&      = delete;
		void post(std::function<void()> aTask) override
		{
			{
				const auto lock = std::scoped_lock{ Mutex };
				Tasks.push_back(std::move(aTask));
			}
			Changed.notify_all();
		}

	private:
		void work(const std::stop_token& aStop)
		{
			while (!aStop.stop_requested()) {
				auto task = std::function<void()>{ };
				{
					auto lock = std::unique_lock{ Mutex };
					Changed.wait(lock, [&] { return aStop.stop_requested() || !Tasks.empty(); });
					if (Tasks.empty()) {
						continue;
					}
					task = std::move(Tasks.front());
					Tasks.pop_front();
				}
				task();
			}
		}
		std::mutex                        Mutex;
		std::condition_variable           Changed;
		std::deque<std::function<void()>> Tasks;
		std::jthread                      Worker;
	};

	/// A worktree whose pre-push hook marks that it ran and then hangs.
	auto hanging_push(const git_runner& aGit, const std::filesystem::path& aRoot, const std::string& aName) -> std::pair<std::filesystem::path, std::filesystem::path>
	{
		const auto hosted = aRoot / (aName + "-hosted.git");
		const auto work   = aRoot / aName;
		aGit(aRoot, { "init", "--quiet", "--bare", "-b", "main", hosted.string() });
		std::filesystem::create_directories(work);
		aGit(work, { "init", "--quiet", "-b", "main" });
		aGit(work, { "config", "user.name", "Author" });
		aGit(work, { "config", "user.email", "author@example.invalid" });
		commit_file(aGit, work, "a.txt", "a\n", "Initial");
		aGit(work, { "push", "--quiet", hosted.string(), "main" });
		aGit(work, { "checkout", "--quiet", "-b", "fix/slow" });
		commit_file(aGit, work, "b.txt", "b\n", "Slow");
		const auto hook = work / ".git" / "hooks" / "pre-push";
		write_file(hook, "#!/bin/sh\necho ran >> \"$(git rev-parse --git-dir)/pre-push-ran\"\nsleep 30\n");
		std::filesystem::permissions(hook, std::filesystem::perms::owner_all);
		return { work, hosted };
	}

	[[nodiscard]] auto hook_runs(const std::filesystem::path& aWork) -> std::size_t
	{
		const auto text = std::filesystem::exists(aWork / ".git" / "pre-push-ran") ? read_file(aWork / ".git" / "pre-push-ran") : std::string{ };
		return static_cast<std::size_t>(std::ranges::count(text, '\n'));
	}

	/// RFC-BPR-0114 AC06: a push its bound stops, and one the author stops.
	void stopped_and_timed_out_pushes(const git_runner& aGit, const std::filesystem::path& aRoot, const std::filesystem::path& aGitExecutable)
	{
		const auto allowed = [](const std::string&) { return true; };
		const auto subject = [](const std::filesystem::path& aWork, const std::filesystem::path& aHosted) {
			return application::local_changes_subject{ .Worktree = aWork.string(), .Branch = "fix/slow", .FallbackUrl = aHosted.string(), .Target = "main", .ConsentKey = "slow" };
		};
		auto matcher = std::shared_ptr<application::diff_line_matcher_port>{ core::make_xdiff_line_matcher() };
		auto files   = adapters::git::make_worktree_files();
		{
			// Timed out: the hooks' bound is a second and a half here.
			const auto [work, hosted] = hanging_push(aGit, aRoot, "timeout");
			auto       author         = adapters::git::make_installed_author_git(aGitExecutable, { .Hooks = std::chrono::milliseconds{ 1500 } });
			auto       effects        = queue{ };
			auto       decisions      = queue{ };
			auto       port           = recorder{ };
			auto       host           = application::local_changes_host{ *author, *files, matcher, effects, decisions, port, { .Allowed = allowed } };
			const auto drain          = [&] {
				while (!effects.Tasks.empty() || !decisions.Tasks.empty()) {
					for (auto* lane : { &effects, &decisions }) {
						while (!lane->Tasks.empty()) {
							auto task = std::move(lane->Tasks.front());
							lane->Tasks.pop_front();
							task();
						}
					}
				}
			};
			host.open(subject(work, hosted));
			host.check_again();
			drain();
			expect(port.Last.Push.Offered, "the slow branch can be pushed");
			const auto started = std::chrono::steady_clock::now();
			host.push();
			drain();
			const auto took = std::chrono::steady_clock::now() - started;
			std::cout << "TIMEOUT took=" << std::chrono::duration_cast<std::chrono::milliseconds>(took).count() << "ms failure="
					  << (port.Last.Failure ? application::name(port.Last.Failure->Failure) : std::string_view{ "none" }) << '\n';
			expect(port.Last.Failure && port.Last.Failure->Failure == application::author_git_failure::timeout && took < std::chrono::seconds{ 15 },
				   "AC06: the push's bound kills the hanging hook and says it timed out");
			expect(port.Last.Hosted.Observed && !port.Last.Hosted.Tip && port.Last.Push.Offered, "AC06: GitHub is read again: the branch is not there, Push is offered again");
			expect(hook_runs(work) == 1U, "AC06: nothing was repeated");
			host.shutdown();
		}
		{
			// Stopped by the author while the hook hangs.
			const auto [work, hosted] = hanging_push(aGit, aRoot, "stopped");
			auto       author         = adapters::git::make_installed_author_git(aGitExecutable);
			auto       effects        = thread_lane{ };
			auto       decisions      = queue{ };
			auto       port           = recorder{ };
			auto       host           = application::local_changes_host{ *author, *files, matcher, effects, decisions, port, { .Allowed = allowed } };
			const auto settle         = [&](const std::function<bool()>& aDone) {
				const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 20 };
				while (!aDone() && std::chrono::steady_clock::now() < deadline) {
					while (!decisions.Tasks.empty()) {
						auto task = std::move(decisions.Tasks.front());
						decisions.Tasks.pop_front();
						task();
					}
					std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
				}
			};
			host.open(subject(work, hosted));
			host.check_again();
			settle([&] { return port.Last.Hosted.Observed && port.Last.Running == application::local_changes_operation::none; });
			expect(port.Last.Push.Offered, "the slow branch can be pushed");
			host.push();
			settle([&] { return hook_runs(work) == 1U; });
			expect(hook_runs(work) == 1U && port.Last.Running == application::local_changes_operation::pushing, "the push is in flight, its hook running");
			const auto started = std::chrono::steady_clock::now();
			host.stop();
			settle([&] { return port.Last.Running == application::local_changes_operation::none; });
			const auto took = std::chrono::steady_clock::now() - started;
			std::cout << "STOPPED took=" << std::chrono::duration_cast<std::chrono::milliseconds>(took).count() << "ms failure="
					  << (port.Last.Failure ? application::name(port.Last.Failure->Failure) : std::string_view{ "none" }) << '\n';
			expect(port.Last.Failure && port.Last.Failure->Failure == application::author_git_failure::cancelled && took < std::chrono::seconds{ 10 },
				   "AC06: Stop kills the push and its hook at once and says it was stopped");
			expect(port.Last.Hosted.Observed && !port.Last.Hosted.Tip, "AC06: GitHub is read again after the stop: the branch is not there");
			expect(aGit.output(hosted, { "for-each-ref", "--format=%(refname)", "refs/heads/fix/slow" }, aRoot / "refs.txt").empty(), "nothing reached the hosted repository");
			// Long enough for a repeated push to have started its hook again.
			const auto quiet = std::chrono::steady_clock::now() + std::chrono::milliseconds{ 500 };
			settle([&] { return std::chrono::steady_clock::now() > quiet; });
			expect(hook_runs(work) == 1U, "AC06: nothing was repeated");
			host.shutdown();
		}
		std::cout << "stopped-and-timed-out-pushes:pass\n";
	}

	/// [[BPR-2182]]. A URL and a branch containing "force" push: the runner
	/// refuses only a command the core would not build, such as one with
	/// `--force` or a `+` refspec added, and those launch nothing.
	void force_words_push(const git_runner& aGit, const std::filesystem::path& aRoot, const std::filesystem::path& aGitExecutable)
	{
		const auto hosted = aRoot / "salesforce-enforce.git";
		const auto work   = aRoot / "enforce-work";
		aGit(aRoot, { "init", "--quiet", "--bare", "-b", "main", hosted.string() });
		std::filesystem::create_directories(work);
		aGit(work, { "init", "--quiet", "-b", "fix/enforce-limits" });
		commit_file(aGit, work, "limits.txt", "enforce\n", "Enforce limits");
		const auto head    = aGit.output(work, { "rev-parse", "HEAD" }, aRoot / "enforce-head.txt");
		auto       author  = adapters::git::make_installed_author_git(aGitExecutable);
		const auto command = core::make_push_command(work.string(), hosted.string(), "fix/enforce-limits");
		expect(command.has_value(), "the core builds the push for a URL and branch containing \"force\"");
		const auto branch = [&] { return aGit.output(hosted, { "for-each-ref", "--format=%(objectname)", "refs/heads/fix/enforce-limits" }, aRoot / "enforce-ref.txt"); };

		auto forced = *command;
		forced.Arguments.insert(forced.Arguments.begin() + 1, "--force");
		const auto refusedForce = author->push(forced, { });
		expect(!refusedForce && refusedForce.error().Failure == application::author_git_failure::malformed, "an added --force is refused");
		auto plus              = *command;
		plus.Arguments.back()  = "+" + plus.Arguments.back();
		const auto refusedPlus = author->push(plus, { });
		expect(!refusedPlus && refusedPlus.error().Failure == application::author_git_failure::malformed, "a + refspec is refused");
		expect(branch().empty(), "a refused push launches nothing");

		const auto pushed = author->push(*command, { });
		expect(pushed.has_value(), "the push to salesforce-enforce.git on fix/enforce-limits ran");
		expect(branch() == head, "the hosted branch is the head");
		std::cout << "force-words-push:pass\n";
	}

	/// [[BPR-2182]]. Stage and Unstage take a path as itself: brackets, `*`,
	/// `?` and a leading `:` are never a pattern or pathspec magic, on a
	/// branch with commits and on an unborn one.
	void literal_paths(const git_runner& aGit, const std::filesystem::path& aRoot, const std::filesystem::path& aGitExecutable)
	{
		const auto special = std::vector<std::string>{ "pages/[id].tsx", "a*.txt", "q?.txt", ":top.txt" };
		const auto plain   = std::vector<std::string>{ "pages/i.tsx", "pages/d.tsx", "ab.txt", "qz.txt", "top.txt" };
		auto       author  = adapters::git::make_installed_author_git(aGitExecutable);
		for (const auto born : { true, false }) {
			const auto work = aRoot / (born ? "literal-born" : "literal-unborn");
			std::filesystem::create_directories(work);
			aGit(work, { "init", "--quiet", "-b", "main" });
			if (born) {
				commit_file(aGit, work, "README", "readme\n", "Initial");
			}
			for (const auto& path : special) {
				write_file(work / path, path + "\n");
			}
			for (const auto& path : plain) {
				write_file(work / path, path + "\n");
			}
			const auto staged = [&] {
				auto text = aGit.output(work, { "-c", "core.quotepath=false", "diff", "--cached", "--name-only" }, aRoot / "literal-staged.txt");
				std::ranges::replace(text, '\n', ',');
				return text;
			};
			expect(author->stage_paths(work.string(), special, { }).has_value(), "Stage runs");
			expect(staged() == ":top.txt,a*.txt,pages/[id].tsx,q?.txt", born ? "only the named paths are staged" : "only the named paths are staged on an unborn branch");
			expect(author->stage_paths(work.string(), plain, { }).has_value(), "the others stage too");
			expect(author->unstage_paths(work.string(), special, { }).has_value(), "Unstage runs");
			expect(staged() == "ab.txt,pages/d.tsx,pages/i.tsx,qz.txt,top.txt", born ? "only the named paths are unstaged" : "only the named paths are unstaged on an unborn branch");
		}
		std::cout << "literal-paths:pass\n";
	}

	void save_and_agent(course& aCourse)
	{
		const auto file = aCourse.Work / "src" / "crlf.txt";
		write_file(file, "a\r\nb\r\nc");
		const auto read   = std::vector<std::uint8_t>{ 'a', '\r', '\n', 'b', '\r', '\n', 'c' };
		const auto edited = std::vector<std::uint8_t>{ 'a', '\r', '\n', 'B', '\r', '\n', 'c' };
		aCourse.Host->save("src/crlf.txt", read, edited);
		aCourse.drain();
		expect(read_file(file) == "a\r\nB\r\nc" && aCourse.view().Saved == "src/crlf.txt", "Save writes the exact bytes, CRLF and no final newline");
		write_file(file, "changed elsewhere\n");
		aCourse.Host->save("src/crlf.txt", edited, read);
		aCourse.drain();
		expect(read_file(file) == "changed elsewhere\n" && aCourse.view().ChangedOnDisk == "src/crlf.txt", "a file changed on disk is not overwritten");
		expect(!aCourse.Files->write(aCourse.Work.string(), "../outside.txt", { }, { 'x' }), "a path outside the worktree is refused");

		aCourse.Host->stage({ "src/crlf.txt" });
		aCourse.drain();
		aCourse.Host->set_agent(
			[&aCourse](const application::local_commit_message_agent& aAgent, const std::string& aPrompt, std::stop_token) -> std::expected<std::string, std::string> {
				expect(aAgent.Agent == "claude" && aAgent.Instructions == "Write it.", "the footer's agent and the purpose's text");
				aCourse.Prompt = aPrompt;
				return "Subject: Add the CRLF sample\n\nIt keeps Windows line endings.";
			},
			&aCourse.Effects);
		aCourse.Host->suggest_message({ .Agent = "claude", .AgentName = "Claude Code", .Profile = 1U, .Instructions = "Write it." });
		aCourse.drain();
		expect(aCourse.view().Suggestion && aCourse.view().Suggestion->Subject == "Add the CRLF sample" && aCourse.view().Suggestion->Description == "It keeps Windows line endings.",
			   "the suggestion fills subject and description");
		expect(aCourse.Prompt.find("src/crlf.txt") != std::string::npos && aCourse.Prompt.find("Add notes") != std::string::npos
				   && aCourse.Prompt.find("Write it.") == std::string::npos,
			   "the prompt has the staged diff and recent subjects; [[BPR-2140]] the purpose's instructions travel apart");
		expect(aCourse.out(aCourse.Work, { "diff", "--cached", "--name-only" }) == "src/crlf.txt", "a suggestion commits nothing");
	}

	void declined(course& aCourse)
	{
		aCourse.Allowed.clear();
		aCourse.Host->commit({ .Subject = "Add the CRLF sample" });
		aCourse.drain();
		expect(aCourse.view().ConsentNeeded, "asked again for another draft");
		const auto before = aCourse.out(aCourse.Work, { "rev-parse", "HEAD" });
		aCourse.Host->answer_consent(false);
		aCourse.drain();
		expect(aCourse.view().Declined && !aCourse.view().Commands.empty() && aCourse.view().Commands.front().find(" commit") != std::string::npos, "declining shows the commands");
		expect(aCourse.out(aCourse.Work, { "rev-parse", "HEAD" }) == before, "declining runs nothing");
	}
	/// Opt-in, never registered: the same host on a real clone of the owner's
	/// designated BPR-Test, on a disposable branch the caller made. Stage,
	/// commit, Check Again and Push through the author's own Git and
	/// credentials; the caller deletes the branch afterwards.
	auto live(const std::string& aGit, const std::string& aClone, const std::string& aBranch) -> int
	{
		auto       author  = adapters::git::make_installed_author_git(std::filesystem::path{ aGit });
		auto       files   = adapters::git::make_worktree_files();
		auto       effects = queue{ };
		auto       lanes   = queue{ };
		auto       port    = recorder{ };
		auto       matcher = std::shared_ptr<application::diff_line_matcher_port>{ core::make_xdiff_line_matcher() };
		auto       host    = application::local_changes_host{ *author, *files, matcher, effects, lanes, port, { .Allowed = [](const std::string&) { return true; }, .Remember = [](const std::string&) { } } };
		const auto drain   = [&] {
			while (!effects.Tasks.empty() || !lanes.Tasks.empty()) {
				for (auto* lane : { &effects, &lanes }) {
					while (!lane->Tasks.empty()) {
						auto task = std::move(lane->Tasks.front());
						lane->Tasks.pop_front();
						task();
					}
				}
			}
		};
		host.open({ .Worktree = aClone, .Branch = aBranch, .Clone = aClone, .Remote = "origin", .FallbackUrl = "https://github.com/MikeMarcin/BPR-Test.git", .Target = "main", .ConsentKey = "live" });
		drain();
		expect(port.Last.Status.Branch == aBranch && !port.Last.Status.Unstaged.empty(), "live: the clone's change is unstaged");
		host.stage({ port.Last.Status.Unstaged.front().Path });
		drain();
		host.commit({ .Subject = "BPR local-changes live course", .Description = "Disposable; deleted after the course." });
		drain();
		expect(!port.Last.Failure && port.Last.Status.Staged.empty(), "live: committed with the author's Git");
		host.check_again();
		drain();
		std::cout << "LIVE_BEFORE_PUSH:" << core::name(port.Last.Push.State) << ":" << (port.Last.Command ? port.Last.Command->Text : std::string{ }) << '\n';
		expect(port.Last.Push.State == core::local_push_state::missing && port.Last.Push.Offered, "live: the branch is not on GitHub; Push creates it");
		host.push();
		drain();
		expect(!port.Last.Failure, "live: the push ran");
		std::cout << "LIVE_AFTER_PUSH:" << core::name(port.Last.Push.State) << ":" << (port.Last.Hosted.Tip ? std::string{ core::canonical_hex(*port.Last.Hosted.Tip).view() } : std::string{ }) << '\n';
		expect(port.Last.Push.State == core::local_push_state::up_to_date, "live: up to date with GitHub after the push");
		host.shutdown();
		std::cout << "local-changes-live:pass\n";
		return EXIT_SUCCESS;
	}
}

auto main(int argc, char** argv) -> int
{
	if (argc == 5 && std::string_view{ argv[1] } == "--live") {
		return live(argv[2], argv[3], argv[4]);
	}
	const auto git = tests::workspace_contract::git_from(argc, argv);
	if (!git) {
		std::cout << "local-changes:skipped (no Git)\n";
		return EXIT_SUCCESS;
	}
#if !defined(_WIN32)
	auto scratch = tests::sqlite_contract::scratch_root{ "local-changes" };
	auto root    = std::filesystem::canonical(scratch.path());
	auto hosted  = root / "hosted.git";
	(*git)(root, { "init", "--quiet", "--bare", "-b", "main", hosted.string() });
	auto work = root / "work";
	std::filesystem::create_directories(work);
	(*git)(work, { "init", "--quiet", "-b", "main" });
	(*git)(work, { "config", "user.name", "Author" });
	(*git)(work, { "config", "user.email", "author@example.invalid" });
	commit_file(*git, work, "src/port.cpp", "int parse()\n{\nreturn atoi;\n}\n// one\n// two\n// three\n// four\nint end;\n", "Initial");
	(*git)(work, { "push", "--quiet", hosted.string(), "main" });
	const auto base = git->output(work, { "rev-parse", "HEAD" }, root / "base.txt");
	(*git)(work, { "checkout", "--quiet", "-b", "fix/ports" });
	commit_file(*git, work, "src/strict.cpp", "bool strict;\n", "Strict parsing");
	(*git)(work, { "push", "--quiet", hosted.string(), "fix/ports" });
	commit_file(*git, work, "src/limits.cpp", "int limit = 65535;\n", "Limits");
	write_file(work / "docs" / "ports.md", "# Ports\n");
	(*git)(work, { "add", "docs/ports.md" });
	write_file(work / "src" / "port.cpp", "int parse(int value)\n{\nreturn atoi;\n}\n// one\n// two\n// three\n// four\nreturn value;\n");
	write_file(work / "notes.txt", "draft\n");

	auto author  = adapters::git::make_installed_author_git(std::filesystem::path{ argv[2] });
	auto files   = adapters::git::make_worktree_files();
	auto c       = course{ .Git = *git, .Root = root, .Hosted = hosted, .Work = work, .Author = std::move(author), .Files = std::move(files) };
	auto matcher = std::shared_ptr<application::diff_line_matcher_port>{ core::make_xdiff_line_matcher() };
	c.Host       = std::make_unique<application::local_changes_host>(*c.Author, *c.Files, matcher, c.Effects, c.Decisions, c.Port,
																	 application::local_changes_consent{
																		 .Allowed  = [&c](const std::string& aKey) { return std::ranges::find(c.Allowed, aKey) != c.Allowed.end(); },
																		 .Remember = [&c](const std::string& aKey) { c.Allowed.push_back(aKey); },
																	 });
	c.Host->open({ .Worktree = work.string(), .Branch = "fix/ports", .FallbackUrl = hosted.string(), .Target = "main", .Base = core::parse_git_commit_oid(base), .ConsentKey = "draft-1" });
	c.drain();
	expect(!c.view().Hosted.Observed && c.view().Push.State == core::local_push_state::unobserved && !c.view().Push.Offered, "opening reads no network: GitHub is not checked yet");
	c.Host->check_again();
	c.drain();
	status_and_comparisons(c);
	stage_one_change(c);
	commit_with_hooks(c);
	push_and_refusal(c);
	save_and_agent(c);
	declined(c);
	c.Host->shutdown();
	stopped_and_timed_out_pushes(*git, root, std::filesystem::path{ argv[2] });
	force_words_push(*git, root, std::filesystem::path{ argv[2] });
	literal_paths(*git, root, std::filesystem::path{ argv[2] });
	std::cout << "local-changes:pass\n";
#endif
	return EXIT_SUCCESS;
}
