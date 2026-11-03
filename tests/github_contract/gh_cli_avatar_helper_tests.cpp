/// \file
/// \brief [[BPR-1846]]. The application side of a GitHub CLI avatar helper
/// launch, over real processes: a pinned gh configuration, the shipped gh
/// process runner, `gh_cli_transport::perform_cli_avatars` and the fixture
/// build of the helper.
///
/// \details Driven by `tests/scripts/verify_gh_cli_avatar_helper.py`, which
/// lays out the scratch root (a fake `gh` that prints a token only for the
/// pinned account, the fixture helper and its loopback configuration), runs
/// the fake private-mode GHES, and judges the line this prints. Every launch
/// this process makes is written to `<root>/app-side.txt`: its program,
/// arguments, environment, standard input and raw standard output. The script
/// checks the token is in none of it.

#include "gh_cli_transport.hpp"
#include "github_cli_process_runner.hpp"
#include "github_host_profile.hpp"

#include "bpr/core/gh_cli_avatar.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	[[noreturn]] void fail(const std::string_view aMessage)
	{
		std::cerr << "gh-cli-avatar:fail:" << aMessage << '\n';
		std::exit(EXIT_FAILURE);
	}

	/// Records every launch whole, on the application's side of the pipe.
	class recording_process final : public github_cli_process_port
	{
	public:
		explicit recording_process(std::unique_ptr<github_cli_process_port> aInner)
			: Inner{ std::move(aInner) }
		{
		}

		auto run(const github_cli_process_request& aRequest, std::stop_token aStop) -> github_cli_process_result override
		{
			auto launched = aRequest;
#if defined(__APPLE__)
			// macOS refuses a nested sandbox, and this course already runs in
			// the test runner's: the helper is launched as the recorded,
			// write-constrained command names it, without the wrapper.
			if (launched.Executable == "/usr/bin/sandbox-exec" && launched.Arguments.size() >= 3U && launched.Arguments[0] == "-p") {
				launched.Executable = launched.Arguments[2];
				launched.Arguments.erase(launched.Arguments.begin(), launched.Arguments.begin() + 3);
			}
#endif
			auto result = Inner->run(launched, std::move(aStop));
			Text += "program=" + aRequest.Executable + '\n';
			for (const auto& argument : aRequest.Arguments) {
				Text += "argument=" + argument + '\n';
			}
			for (const auto& entry : aRequest.Environment) {
				Text += "environment=" + entry.Name + '=' + entry.Value + '\n';
			}
			Text += "input=" + aRequest.StandardInput + '\n';
			Text += result ? "output=" + result->StandardOutput + '\n' : "failure=" + std::to_string(static_cast<int>(result.error())) + '\n';
			return result;
		}

		std::string Text;

	private:
		std::unique_ptr<github_cli_process_port> Inner;
	};

	[[nodiscard]] auto platform_facts(const std::filesystem::path& aRoot) -> gh_cli_platform_environment_facts
	{
#if defined(__APPLE__)
		return gh_cli_macos_environment_facts{ .HomeDirectory = aRoot / "home" };
#elif defined(__linux__)
		return gh_cli_linux_environment_facts{ .HomeDirectory = aRoot / "home", .RuntimeDirectory = aRoot / "runtime", .SessionBusAddress = "unix:path=" + (aRoot / "runtime" / "bus").string() };
#else
	#error The avatar helper course runs on macOS and Linux
#endif
	}

	[[nodiscard]] auto state_name(const core::markdown_image_state aState) -> std::string_view
	{
		switch (aState) {
		case core::markdown_image_state::loaded:      return "loaded";
		case core::markdown_image_state::failed:      return "failed";
		case core::markdown_image_state::too_large:   return "too_large";
		case core::markdown_image_state::blocked:     return "blocked";
		case core::markdown_image_state::unsupported: return "unsupported";
		case core::markdown_image_state::loading:
		case core::markdown_image_state::over_limit:  break;
		}
		return "other";
	}

	[[nodiscard]] auto batch_name(const core::gh_cli_avatar_batch_state aState) -> std::string_view
	{
		switch (aState) {
		case core::gh_cli_avatar_batch_state::answered:        return "answered";
		case core::gh_cli_avatar_batch_state::cli_unavailable: return "cli_unavailable";
		case core::gh_cli_avatar_batch_state::cli_signed_out:  return "cli_signed_out";
		case core::gh_cli_avatar_batch_state::refused:         return "refused";
		}
		return "other";
	}

	[[nodiscard]] auto failure_name(const gh_cli_avatar_failure aFailure) -> std::string_view
	{
		switch (aFailure) {
		case gh_cli_avatar_failure::unsupported:           return "unsupported";
		case gh_cli_avatar_failure::configuration_changed: return "configuration_changed";
		case gh_cli_avatar_failure::launch_failed:         return "launch_failed";
		case gh_cli_avatar_failure::malformed_output:      return "malformed_output";
		case gh_cli_avatar_failure::cancelled:             return "cancelled";
		}
		return "other";
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
{
	std::optional<std::filesystem::path> root;
	std::string                          course;
	std::vector<std::string>             addresses;
	auto                                 removeGh = false;
	for (int index = 1; index < aArgc; ++index) {
		const auto argument = std::string_view{ aArgv[index] };
		if (argument == "--remove-gh") {
			removeGh = true;
			continue;
		}
		if (index + 1 >= aArgc) {
			fail("usage");
		}
		const auto value = std::string{ aArgv[++index] };
		if (argument == "--root") {
			root = value;
		} else if (argument == "--course") {
			course = value;
		} else if (argument == "--address") {
			addresses.push_back(value);
		} else {
			fail("usage");
		}
	}
	if (!root || course.empty() || addresses.empty()) {
		fail("usage");
	}
	const auto canonical = std::filesystem::canonical(*root);

	auto profileCandidate            = github_host_profile_candidate{ };
	profileCandidate.Profile         = core::host_profile_id{ 12U };
	profileCandidate.Host            = core::git_host_id{ 22U };
	profileCandidate.Revision        = application::github_profile_revision{ 32U };
	profileCandidate.Server          = application::github_server_contract::ghes_3_18_2022_11_28;
	profileCandidate.Authentication  = application::github_authentication_transport::github_cli;
	profileCandidate.Provenance      = github_profile_provenance::explicit_user;
	profileCandidate.Hostname        = "ghe.bpr.invalid";
	profileCandidate.RestRoot        = "https://ghe.bpr.invalid/api/v3";
	profileCandidate.GraphqlRoot     = "https://ghe.bpr.invalid/api/graphql";
	profileCandidate.CliAccountLogin = "octo";
	const auto profile               = validate_github_host_profile(profileCandidate);
	if (!profile) {
		fail("profile");
	}
	const auto configuration = validate_gh_cli_configuration({
		.Executable              = canonical / "gh" / "gh",
		.NeutralWorkingDirectory = canonical / "neutral",
		.Platform                = platform_facts(canonical),
		.Https                   = { },
	});
	if (!configuration) {
		fail("configuration");
	}
	const auto pinned = pin_gh_cli_configuration(*configuration, profile->hostname(), profile->cli_account_login());
	if (!pinned) {
		fail("pin");
	}
	auto runner = make_github_cli_process_runner();
	if (!runner) {
		fail("runner");
	}
	auto process   = recording_process{ std::move(runner) };
	auto transport = gh_cli_transport{ *profile, *pinned, process };
	if (removeGh) {
		// gh removed after the profile was chosen: nothing is launched.
		std::filesystem::remove(canonical / "gh" / "gh");
	}
	const auto helper = std::filesystem::canonical(canonical / "helper" / "better_pull_review_gh_avatar_helper_fixture");
	const auto result = transport.perform_cli_avatars(helper, addresses, { });

	std::ofstream{ canonical / "app-side.txt", std::ios::binary } << process.Text;
	std::string line = "gh-cli-avatar:" + course + ':';
	if (!result) {
		line += "failure:" + std::string{ failure_name(result.error()) };
	} else {
		line += std::string{ batch_name(result->State) };
		for (const auto& answer : result->Answers) {
			line += ':' + std::string{ state_name(answer.State) } + '/' + std::to_string(answer.HttpStatus) + '/' + (answer.ContentType.empty() ? std::string{ "-" } : answer.ContentType) + '/'
				+ std::to_string(answer.Bytes.size());
		}
	}
	std::cout << line << '\n';
	return EXIT_SUCCESS;
}
