// RFC-BPR-0104, third review round (F1, 2026-11-11). A pull request's own
// agent settings never run in the checkout BetterPullReview made from it, and
// no launch writes the agent's configuration.
//
// Opt in on macOS: configure with -DBPR_LIVE_AGENT_COURSES=ON and run
// `ctest -L live-agent` (row `live_agent_checkout_settings`), or run it by
// hand; never a default row, since it starts the installed Codex and Claude
// Code. Every run has its own home (`HOME` only, as the
// application passes it), whose configuration points every model provider at
// http://127.0.0.1:9, so no run reaches a model; a run is ended after
// `--seconds` (12 by default).
//
//   live_agent_checkout_settings_course --root DIR [--replace] [--codex PROGRAM] [--claude PROGRAM] [--seconds N]
//
// DIR must not exist, unless `--replace` removes it first. The fourth review
// (2026-11-11) adds Codex runs whose home trusts the checkout, whose
// checkout is a linked worktree of a trusted repository, and that run in the
// checkout through the `/System/Volumes/Data` firmlink; guarded runs name the
// checkout by `application::agent_checkout_spellings`, as the host does. For each run the course makes a Git checkout holding
// the hostile settings in `tests/fixtures/agent-checkout-settings` and builds
// the arguments with `application::build_agent_launch`, exactly as a turn
// would: Codex (`exec` and the App Server) and Claude Code, each with no
// grant, the scratch folder, and builds. Guarded runs name the checkout
// (`PullRequestCheckout`) and must leave no beacon and the home's
// configuration byte for byte as it was. Control runs leave it out, as
// launches did before this round, and must leave a beacon (Codex `exec` and
// Claude Code): without them the fixture would prove nothing. Prints one line
// per run and `live-agent-checkout-settings-course:pass` at the end.

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_scratch_folder.hpp"

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern char** environ;

namespace {
	using namespace bpr;
	namespace fs = std::filesystem;

	[[nodiscard]] auto read_file(const fs::path& aPath) -> std::optional<std::string>
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		if (!stream) {
			return std::nullopt;
		}
		return std::string{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	void write_file(const fs::path& aPath, const std::string_view aText)
	{
		fs::create_directories(aPath.parent_path());
		auto stream = std::ofstream{ aPath, std::ios::binary };
		stream << aText;
	}

	[[nodiscard]] auto substituted(std::string aText, const std::string_view aName, const std::string& aValue) -> std::string
	{
		for (auto at = aText.find(aName); at != std::string::npos; at = aText.find(aName, at + aValue.size())) {
			aText.replace(at, aName.size(), aValue);
		}
		return aText;
	}

	[[nodiscard]] auto fixture(const std::string_view aName, const fs::path& aBeacons, const std::string& aLabel) -> std::string
	{
		const auto text = read_file(fs::path{ BPR_CHECKOUT_SETTINGS_FIXTURE } / aName);
		if (!text) {
			std::cerr << "missing fixture " << aName << '\n';
			std::exit(EXIT_FAILURE);
		}
		return substituted(substituted(*text, "@BEACONS@", aBeacons.string()), "@NAME@", aLabel);
	}

	/// Runs a program with its own environment and stdin, for at most
	/// `aSeconds`. The App Server's pipe stays open, as the application keeps
	/// it; `exec` and Claude Code read to the end of their input first.
	[[nodiscard]] auto run(const std::vector<std::string>& aArgv, const std::vector<std::string>& aEnvironment, const fs::path& aWorkingDirectory, const std::string& aInput,
						   const fs::path& aOutput, const int aSeconds, const bool aKeepOpen) -> bool
	{
		auto input = std::array<int, 2>{ };
		if (pipe(input.data()) != 0) {
			return false;
		}
		auto actions = posix_spawn_file_actions_t{ };
		posix_spawn_file_actions_init(&actions);
		posix_spawn_file_actions_adddup2(&actions, input[0], STDIN_FILENO);
		posix_spawn_file_actions_addclose(&actions, input[1]);
		posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, aOutput.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
		const auto errors = aOutput.string() + ".err";
		posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, errors.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
		auto attributes = posix_spawnattr_t{ };
		posix_spawnattr_init(&attributes);
		posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
		posix_spawnattr_setpgroup(&attributes, 0);
		auto argv = std::vector<char*>{ };
		for (const auto& argument : aArgv) {
			argv.push_back(const_cast<char*>(argument.c_str()));
		}
		argv.push_back(nullptr);
		auto environment = std::vector<char*>{ };
		for (const auto& entry : aEnvironment) {
			environment.push_back(const_cast<char*>(entry.c_str()));
		}
		environment.push_back(nullptr);
		// The course is one thread: the child starts in the working folder.
		const auto previous = fs::current_path();
		fs::current_path(aWorkingDirectory);
		auto       child   = pid_t{ };
		const auto spawned = posix_spawn(&child, aArgv.front().c_str(), &actions, &attributes, argv.data(), environment.data());
		fs::current_path(previous);
		posix_spawn_file_actions_destroy(&actions);
		posix_spawnattr_destroy(&attributes);
		close(input[0]);
		if (spawned != 0) {
			close(input[1]);
			return false;
		}
		const auto written = write(input[1], aInput.data(), aInput.size());
		static_cast<void>(written);
		if (!aKeepOpen) {
			close(input[1]);
			input[1] = -1;
		}
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ aSeconds };
		auto       status   = 0;
		while (waitpid(child, &status, WNOHANG) == 0) {
			if (std::chrono::steady_clock::now() >= deadline) {
				kill(-child, SIGKILL);
				static_cast<void>(waitpid(child, &status, 0));
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
		}
		if (input[1] >= 0) {
			close(input[1]);
		}
		// A server the agent started may outlive it by its own `sleep`.
		std::this_thread::sleep_for(std::chrono::seconds{ 2 });
		kill(-child, SIGKILL);
		return true;
	}

	[[nodiscard]] auto beacons_in(const fs::path& aBeacons) -> std::string
	{
		auto names = std::string{ };
		for (const auto& entry : fs::directory_iterator{ aBeacons }) {
			names += (names.empty() ? "" : " ") + entry.path().filename().string();
		}
		return names;
	}

	/// Runs `git -C CHECKOUT ARGUMENTS…` from `PATH`, with no shell.
	[[nodiscard]] auto git(const fs::path& aCheckout, std::vector<std::string> aArguments) -> bool
	{
		aArguments.insert(aArguments.begin(), { "git", "-C", aCheckout.string() });
		auto argv = std::vector<char*>{ };
		for (auto& argument : aArguments) {
			argv.push_back(argument.data());
		}
		argv.push_back(nullptr);
		auto child  = pid_t{ };
		auto status = 0;
		return posix_spawnp(&child, "git", nullptr, nullptr, argv.data(), environ) == 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
	}

	[[nodiscard]] auto git_checkout(const fs::path& aCheckout) -> bool
	{
		return git(aCheckout, { "init", "-q" }) && git(aCheckout, { "add", "-A" })
			&& git(aCheckout, { "-c", "user.email=course@example.invalid", "-c", "user.name=course", "commit", "-qm", "fixture" });
	}

	[[nodiscard]] auto profile(const core::agent_protocol aProtocol, const std::string& aProgram) -> core::agent_profile
	{
		return { .Id = core::agent_profile_id{ 1U }, .DisplayName = "Course", .Executable = aProgram, .Protocol = aProtocol, .Workspace = core::agent_workspace_policy::disposable_review_workspace };
	}

	[[nodiscard]] auto supplement() -> core::agent_context_supplement
	{
		const auto oid = [](const std::uint8_t aSeed) {
			// SHA-1: twenty bytes, the rest zero.
			auto value = std::array<std::uint8_t, 32U>{ };
			std::fill_n(value.begin(), 20U, aSeed);
			return core::git_commit_oid{ core::git_hash_algorithm::sha1, value };
		};
		const auto session = core::agent_session{
			.Id      = core::agent_session_id{ 1U },
			.Profile = core::agent_profile_id{ 1U },
			.Binding = { .Host = core::git_host_id{ 1U }, .Repository = core::repository_id{ 1U }, .PullRequest = core::pull_request_id{ 1U }, .BaseOid = oid(1U), .HeadOid = oid(2U), .LeftEpoch = core::epoch_ordinal{ 1U }, .RightEpoch = core::epoch_ordinal{ 2U } },
		};
		const auto observation = application::agent_context_observation{ .ApplicationSession = core::application_session_id{ 1U }, .Binding = session.Binding, .PullRequestUrl = "https://github.example/o/r/pull/1" };
		const auto built       = application::build_agent_context_supplement(observation, session);
		if (!built) {
			std::cerr << "the course's context did not build\n";
			std::exit(EXIT_FAILURE);
		}
		return *built;
	}

	struct course_case
	{
		std::string          Label;
		core::agent_protocol Protocol;
		bool                 Scratch = false;
		bool                 Builds  = false;
		bool                 Guarded = true;
		/// A control run must fire: the fixture is hostile.
		bool MustFire = false;
		/// The turn runs in the checkout through a symbolic link, as a
		/// workspace under `/var` or `/tmp` does on macOS; the launch names
		/// both paths, as the host does.
		bool ViaLink = false;
		/// Fourth review: the home trusts the checkout (or, for a worktree,
		/// the repository it was added from).
		bool HomeTrusts = false;
		/// Fourth review: the checkout is a linked worktree.
		bool Worktree = false;
		/// Fourth review: the turn runs in the checkout through the
		/// `/System/Volumes/Data` firmlink spelling of its path.
		bool ViaFirmlink = false;
	};
} // namespace

auto main(int aCount, char** aArguments) -> int
{
	auto root    = std::optional<fs::path>{ };
	auto codex   = std::string{ };
	auto claude  = std::string{ };
	auto seconds = 12;
	auto replace = false;
	for (auto index = 1; index < aCount; index += 2) {
		const auto name = std::string_view{ aArguments[index] };
		if (name == "--replace") {
			replace = true;
			--index;
			continue;
		}
		if (index + 1 >= aCount) {
			break;
		}
		const auto value = std::string{ aArguments[index + 1] };
		if (name == "--root") {
			root = fs::path{ value };
		} else if (name == "--codex") {
			codex = value;
		} else if (name == "--claude") {
			claude = value;
		} else if (name == "--seconds") {
			seconds = std::stoi(value);
		}
	}
	if (root && replace) {
		fs::remove_all(*root);
	}
	if (!root || fs::exists(*root) || (codex.empty() && claude.empty())) {
		std::cerr << "usage: live_agent_checkout_settings_course --root NEW-DIR [--replace] [--codex PROGRAM] [--claude PROGRAM] [--seconds N]\n";
		return EXIT_FAILURE;
	}
	fs::create_directories(*root);
	root = fs::canonical(*root);

	auto cases = std::vector<course_case>{ };
	if (!codex.empty()) {
		cases.push_back({ .Label = "codex-exec-control", .Protocol = core::agent_protocol::codex_jsonl_exec, .Guarded = false, .MustFire = true });
		for (const auto protocol : { core::agent_protocol::codex_jsonl_exec, core::agent_protocol::codex_app_server }) {
			const auto name = std::string{ protocol == core::agent_protocol::codex_jsonl_exec ? "codex-exec" : "codex-app-server" };
			cases.push_back({ .Label = name + "-plain", .Protocol = protocol });
			cases.push_back({ .Label = name + "-scratch", .Protocol = protocol, .Scratch = true });
			cases.push_back({ .Label = name + "-builds", .Protocol = protocol, .Scratch = true, .Builds = true });
			cases.push_back({ .Label = name + "-builds-via-link", .Protocol = protocol, .Scratch = true, .Builds = true, .ViaLink = true });
			cases.push_back({ .Label = name + "-builds-home-trusts", .Protocol = protocol, .Scratch = true, .Builds = true, .HomeTrusts = true });
		}
		cases.push_back({ .Label = "codex-exec-control-home-trusts", .Protocol = core::agent_protocol::codex_jsonl_exec, .Guarded = false, .MustFire = true, .HomeTrusts = true });
		cases.push_back({ .Label = "codex-exec-worktree-home-trusts", .Protocol = core::agent_protocol::codex_jsonl_exec, .HomeTrusts = true, .Worktree = true });
		cases.push_back({ .Label = "codex-exec-control-worktree-home-trusts", .Protocol = core::agent_protocol::codex_jsonl_exec, .Guarded = false, .MustFire = true, .HomeTrusts = true, .Worktree = true });
		if (fs::exists("/System/Volumes/Data" + root->string())) {
			cases.push_back({ .Label = "codex-exec-builds-via-firmlink", .Protocol = core::agent_protocol::codex_jsonl_exec, .Scratch = true, .Builds = true, .ViaFirmlink = true });
		}
	}
	if (!claude.empty()) {
		cases.push_back({ .Label = "claude-control", .Protocol = core::agent_protocol::claude_stream_json, .Guarded = false, .MustFire = true });
		cases.push_back({ .Label = "claude-plain", .Protocol = core::agent_protocol::claude_stream_json });
		cases.push_back({ .Label = "claude-scratch", .Protocol = core::agent_protocol::claude_stream_json, .Scratch = true });
		cases.push_back({ .Label = "claude-builds", .Protocol = core::agent_protocol::claude_stream_json, .Scratch = true, .Builds = true });
	}

	const auto context = supplement();
	auto       failed  = false;
	for (const auto& entry : cases) {
		const auto base     = *root / entry.Label;
		const auto home     = base / "home";
		const auto checkout = base / "checkout";
		const auto scratch  = base / "scratch";
		const auto beacons  = base / "beacons";
		// A worktree's files are committed in its repository and checked out.
		const auto source = entry.Worktree ? base / "repository" : checkout;
		for (const auto& folder : { home, source, scratch, beacons }) {
			fs::create_directories(folder);
		}
		const auto isCodex = entry.Protocol != core::agent_protocol::claude_stream_json;
		write_file(source / ".codex/config.toml", fixture("codex-project-config.toml.in", beacons, "codex"));
		write_file(source / ".claude/settings.json", fixture("claude-project-settings.json.in", beacons, "claude-settings"));
		write_file(source / ".claude/settings.local.json", fixture("claude-project-settings.json.in", beacons, "claude-settings-local"));
		write_file(source / ".mcp.json", fixture("mcp.json.in", beacons, "claude"));
		write_file(source / ".claude/skills/beacon/SKILL.md", fixture("claude-skill.md.in", beacons, "claude"));
		write_file(source / "README.md", "A pull request.\n");
		if (!git_checkout(source) || (entry.Worktree && !git(source, { "worktree", "add", "-q", "--detach", checkout.string() }))) {
			std::cerr << entry.Label << ": the checkout could not be made\n";
			return EXIT_FAILURE;
		}
		const auto homeConfig = isCodex ? home / ".codex/config.toml" : home / ".claude/settings.json";
		auto       homeText   = isCodex ? fixture("codex-home-config.toml.in", beacons, "home") : std::string{ "{}\n" };
		if (entry.HomeTrusts) {
			// The reviewer's own trust, as `codex` writes it.
			homeText += "\n[projects.\"" + fs::canonical(source).string() + "\"]\ntrust_level = \"trusted\"\n";
		}
		write_file(homeConfig, homeText);
		const auto before = read_file(homeConfig);

		auto runsIn = checkout;
		if (entry.ViaLink) {
			fs::create_directory_symlink(base, *root / (entry.Label + "-link"));
			runsIn = *root / (entry.Label + "-link") / "checkout";
		}
		if (entry.ViaFirmlink) {
			runsIn = fs::path{ "/System/Volumes/Data" + checkout.string() };
		}
		auto options = application::agent_launch_options{ };
		if (entry.Guarded) {
			auto spellings = application::agent_checkout_spellings(runsIn.string());
			if (!spellings) {
				std::cerr << entry.Label << ": the checkout could not be resolved\n";
				return EXIT_FAILURE;
			}
			options.PullRequestCheckout = std::move(*spellings);
		}
		if (entry.Scratch || entry.Builds) {
			options.Grant = application::agent_launch_grant{ .ScratchFolder = entry.Scratch ? scratch.string() : std::string{ }, .BuildWorkspace = entry.Builds ? runsIn.string() : std::string{ } };
		}
		const auto program = isCodex ? codex : claude;
		const auto launch  = application::build_agent_launch(profile(entry.Protocol, program), context, application::environment_reader{ }, options);
		if (!launch) {
			std::cerr << entry.Label << ": the launch was refused\n";
			return EXIT_FAILURE;
		}
		auto argv = std::vector<std::string>{ program };
		argv.insert(argv.end(), launch->Arguments.begin(), launch->Arguments.end());
		auto input = launch->Input;
		if (entry.Protocol == core::agent_protocol::codex_app_server) {
			// What the application sends once Codex answers `initialize`.
			input += "{\"method\":\"initialized\"}\n{\"method\":\"thread/start\",\"id\":1,\"params\":{}}\n";
		}
		auto environment = std::vector<std::string>{ "HOME=" + home.string(), "PATH=/usr/bin:/bin:/usr/sbin:/sbin:" + fs::path{ program }.parent_path().string(), "TMPDIR=" + (base / "tmp").string() };
		fs::create_directories(base / "tmp");
		if (!isCodex) {
			environment.insert(environment.end(), { "ANTHROPIC_API_KEY=sk-course-dead", "ANTHROPIC_BASE_URL=http://127.0.0.1:9", "CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC=1", "DISABLE_TELEMETRY=1", "DISABLE_AUTOUPDATER=1" });
		}
		if (!run(argv, environment, runsIn, input, base / "output.jsonl", seconds, entry.Protocol == core::agent_protocol::codex_app_server)) {
			std::cerr << entry.Label << ": could not start " << program << '\n';
			return EXIT_FAILURE;
		}
		const auto fired     = beacons_in(beacons);
		const auto after     = read_file(homeConfig);
		const auto unchanged = before == after;
		// A run that never started its session would prove nothing.
		const auto output  = read_file(base / "output.jsonl").value_or(std::string{ });
		const auto marker  = entry.Protocol == core::agent_protocol::codex_jsonl_exec ? std::string_view{ "\"turn.started\"" }
			: entry.Protocol == core::agent_protocol::codex_app_server                ? std::string_view{ "\"thread/started\"" }
																					  : std::string_view{ "\"subtype\":\"init\"" };
		const auto reached = output.find(marker) != std::string::npos;
		const auto passed  = reached && (entry.MustFire ? !fired.empty() : fired.empty() && unchanged);
		std::cout << entry.Label << ": " << (reached ? "session started" : "SESSION NOT STARTED") << ", beacons [" << fired << "], home configuration " << (unchanged ? "unchanged" : "CHANGED")
				  << (entry.MustFire ? " (control: must fire)" : "") << ' ' << (passed ? "ok" : "FAILED") << '\n';
		std::ostringstream arguments;
		for (const auto& argument : launch->Arguments) {
			arguments << " [" << argument << ']';
		}
		std::cout << "  arguments:" << arguments.str() << '\n';
		failed = failed || !passed;
	}
	if (failed) {
		std::cout << "live-agent-checkout-settings-course:FAILED\n";
		return EXIT_FAILURE;
	}
	std::cout << "live-agent-checkout-settings-course:pass\n";
	return EXIT_SUCCESS;
}
