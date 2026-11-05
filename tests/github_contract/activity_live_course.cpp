// [[BPR-1850]]. The live course of activity notifications on the GitHub CLI
// connection: never a CTest row and not built by default
// (BPR_BUILD_LIVE_ACTIVITY_COURSE).
//
// tests/scripts/activity_live_course.py opens a pull request on
// MikeMarcin/BPR-Test as the signed-in account and runs this tool with a JSON
// description of it. The tool composes what the app composes for a GitHub CLI
// profile's activity notifications: `gh_cli_transport` over the shipped
// process runner, pinned to the account, one operation per poll whose account
// is read once (`/user`) before `list_notifications`, the shipped answer
// reader, the application host with its Qt timer, the SQLite store, and the Qt
// sink (its display captured: offscreen has no tray). It asks the script to
// comment as the second identity when the baseline is taken and again when
// the pull request is focused, and prints one `ACTIVITY_LIVE:` line per
// request with its status and `X-RateLimit-Remaining`, per diagnostics line,
// per post, and a summary. It stops below the rate-limit floor.
//
// [[BPR-1867]]. It also prints each hand-off of active pull requests the host
// makes for a window showing one to read it again, and requires one naming the
// pull request after each comment: the focused one's too, which is not posted.
#include "gh_cli_transport.hpp"
#include "github_cli_process_runner.hpp"
#include "github_host_profile.hpp"
#include "github_notifications_read.hpp"

#include "bpr/adapters/sqlite/pull_request_activity_store.hpp"
#include "bpr/application/pull_request_activity_host.hpp"
#include "bpr/qt/pull_request_activity_notifications.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	constexpr auto profile_id = std::uint64_t{ 33U };
	constexpr auto host_id    = std::uint64_t{ 11U };
	constexpr auto revision   = std::uint64_t{ 34U };

	[[nodiscard]] auto course() -> QJsonObject&
	{
		static auto value = QJsonObject{ };
		return value;
	}
	int Floor = 500;

	[[noreturn]] void fail(const std::string_view aWhy)
	{
		std::cout << "ACTIVITY_LIVE:FAILED:" << aWhy << '\n'
				  << std::flush;
		std::exit(EXIT_FAILURE);
	}

	[[nodiscard]] auto text(const char* aKey) -> std::string
	{
		return course().value(QString::fromLatin1(aKey)).toString().toStdString();
	}

	[[nodiscard]] auto number(const char* aKey) -> std::uint64_t
	{
		return static_cast<std::uint64_t>(course().value(QString::fromLatin1(aKey)).toInteger());
	}

	[[nodiscard]] auto header(const wire_response& aResponse, const std::string_view aName) -> std::string
	{
		for (const auto& entry : aResponse.Headers) {
			auto name = entry.Name;
			std::ranges::transform(name, name.begin(), [](const unsigned char aByte) { return static_cast<char>(std::tolower(aByte)); });
			if (name == aName) {
				return entry.Value;
			}
		}
		return "none";
	}

	/// The app's gh path for the signed-in account, as the app composes it.
	struct gh_path
	{
		std::optional<validated_github_host_profile>  Profile;
		std::optional<validated_gh_cli_configuration> Configuration;
		std::unique_ptr<github_cli_process_port>      Process;
		std::unique_ptr<gh_cli_transport>             Transport;
	};

	[[nodiscard]] auto open_gh_path() -> gh_path
	{
		auto       path       = gh_path{ };
		const auto executable = QFileInfo{ QStandardPaths::findExecutable(QStringLiteral("gh")) }.canonicalFilePath();
		if (executable.isEmpty()) {
			fail("gh-not-found");
		}
		const auto live = validate_gh_cli_configuration({
			.Executable              = executable.toStdString(),
			.NeutralWorkingDirectory = QDir::tempPath().toStdString(),
			.Platform                = gh_cli_macos_environment_facts{ .HomeDirectory = QDir::homePath().toStdString() },
			.Https                   = { },
		});
		if (!live) {
			fail("gh-configuration");
		}
		const auto pinned = pin_gh_cli_configuration(*live, "github.com", text("login"));
		if (!pinned) {
			fail("gh-pin");
		}
		path.Configuration.emplace(*pinned);
		const auto profile = validate_github_host_profile({
			.Profile         = core::host_profile_id{ profile_id },
			.Host            = core::git_host_id{ host_id },
			.Revision        = application::github_profile_revision{ revision },
			.Server          = application::github_server_contract::github_com_2022_11_28,
			.Authentication  = application::github_authentication_transport::github_cli,
			.Proxy           = application::github_proxy_mode::system,
			.Trust           = application::github_trust_mode::system,
			.Provenance      = github_profile_provenance::builtin,
			.Hostname        = "github.com",
			.RestRoot        = "https://api.github.com",
			.GraphqlRoot     = "https://api.github.com/graphql",
			.CliAccountLogin = text("login"),
		});
		if (!profile) {
			fail("profile");
		}
		path.Profile.emplace(*profile);
		path.Process = make_github_cli_process_runner();
		if (!path.Process) {
			fail("runner");
		}
		path.Transport = std::make_unique<gh_cli_transport>(*path.Profile, *path.Configuration, *path.Process);
		return path;
	}

	/// Posts onto the Qt main thread, as the app's decision lane does.
	class qt_lane final : public application::decision_executor, public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			QTimer::singleShot(0, qApp, [task = std::move(aTask)] { task(); });
		}
	};

	/// The provider's poll over the app's gh path: one operation, its
	/// account read once, then the notification list.
	class live_port final : public application::pull_request_activity_port
	{
	public:
		explicit live_port(gh_path& aPath)
			: Path{ &aPath }
		{
		}

		void poll_activity(application::pull_request_activity_request aRequest, application::pull_request_activity_completion aCompletion) override
		{
			auto answer         = application::pull_request_activity_answer{ };
			answer.Poll.Outcome = core::activity_poll_outcome::failed;
			auto operation      = Path->Transport->begin_operation({ });
			if (!operation || !*operation) {
				std::cout << "ACTIVITY_LIVE:operation:failed\n"
						  << std::flush;
				aCompletion(answer);
				return;
			}
			const auto& scope   = *operation;
			const auto  request = [&](const read_operation aOperation, std::string aSince) -> std::optional<wire_response> {
				auto wire = compile_read_request({ .Server = server_contract::github_com, .Operation = aOperation, .Page = 1U, .IfModifiedSince = std::move(aSince) });
				if (!wire) {
					fail("compile");
				}
				const auto performed = scope->perform({ .Correlation = { .Parent = aRequest.Context, .ProfileRevision = Path->Profile->revision(), .Host = Path->Profile->host(), .Operation = aOperation, .Page = 1U },
														.Wire        = std::move(*wire) },
													  { });
				++answer.Requests;
				const auto* success = std::get_if<github_wire_success>(&performed);
				if (success == nullptr) {
					std::cout << "ACTIVITY_LIVE:request:" << operation_name(aOperation) << ":no-answer\n"
							  << std::flush;
					return std::nullopt;
				}
				const auto remaining = header(success->Response, "x-ratelimit-remaining");
				std::cout << "ACTIVITY_LIVE:request:" << operation_name(aOperation) << ":" << success->Response.Status << ":remaining=" << remaining
						  << (aOperation == read_operation::list_notifications ? ":last-modified=" + std::string{ header(success->Response, "last-modified") != "none" ? "present" : "none" } + ":poll-interval=" + header(success->Response, "x-poll-interval") : std::string{ })
						  << '\n'
						  << std::flush;
				if (remaining != "none" && std::stoi(remaining) < Floor) {
					fail("rate-limit-below-floor");
				}
				return success->Response;
			};
			// The account read the lease makes as an operation begins.
			const auto viewer = request(read_operation::get_viewer_identity, { });
			if (!viewer || viewer->Status != 200 || viewer->Body.find("\"login\":\"" + text("login") + "\"") == std::string::npos) {
				aCompletion(answer);
				return;
			}
			const auto listed = request(read_operation::list_notifications, aRequest.IfModifiedSince);
			if (!listed) {
				aCompletion(answer);
				return;
			}
			auto read           = read_notifications_response(*listed);
			answer.Poll         = read.Poll;
			answer.LastModified = std::move(read.LastModified);
			answer.Threads      = std::move(read.Threads);
			answer.Complete     = read.Complete;
			aCompletion(std::move(answer));
		}

	private:
		gh_path* Path = nullptr;
	};

	class memory_store final : public application::pull_request_activity_store_port
	{
	public:
		explicit memory_store(std::filesystem::path aRoot)
			: Root{ std::move(aRoot) }
		{
		}
		auto                  store(const core::activity_memory& aMemory) -> bool override { return adapters::sqlite::store_pull_request_activity(Root, aMemory); }
		std::filesystem::path Root;
	};

	class printed_log final : public application::diagnostic_log_port
	{
	public:
		auto record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			std::cout << "ACTIVITY_LIVE:log:" << aEvent.Summary.text() << '\n'
					  << std::flush;
			return application::diagnostic_log_status::available;
		}
		auto status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }
	};
} // namespace

int main(int aArgc, char** aArgv)
{
	QApplication application{ aArgc, aArgv };
	if (aArgc != 4) {
		fail("usage: activity_live_course COURSE.json STATE-ROOT COMMENT-COMMAND");
	}
	auto file = QFile{ QString::fromLocal8Bit(aArgv[1]) };
	if (!file.open(QIODevice::ReadOnly)) {
		fail("course-json");
	}
	course()               = QJsonDocument::fromJson(file.readAll()).object();
	const auto root        = std::filesystem::path{ aArgv[2] };
	const auto commentTool = QString::fromLocal8Bit(aArgv[3]);
	const auto target      = core::pull_request_ref{ .Host = "github.com", .Account = text("login"), .Repository = text("repository"), .Number = static_cast<std::int64_t>(number("number")) };
	const auto title       = text("repository") + " #" + std::to_string(number("number"));

	auto path    = open_gh_path();
	auto port    = live_port{ path };
	auto lane    = qt_lane{ };
	auto store   = memory_store{ root };
	auto focused = std::vector<core::pull_request_ref>{ };
	auto posted  = std::vector<std::string>{ };
	auto sink    = qt::pull_request_activity_notification_sink{ [&](QSystemTrayIcon&, const QString& aTitle, const QString& aBody) {
		const auto ours = aTitle.toStdString() == title;
		posted.push_back(ours ? aTitle.toStdString() : std::string{ "other" });
		std::cout << "ACTIVITY_LIVE:posted:" << (ours ? aTitle.toStdString() + "|" + aBody.toStdString() : std::string{ "another pull request" }) << '\n'
				  << std::flush;
	} };
	auto host  = std::unique_ptr<application::pull_request_activity_host>{ };
	auto timer = qt::pull_request_activity_timer{ [&] { host->tick(); } };
	host       = std::make_unique<application::pull_request_activity_host>(
		port, store, lane, lane, timer, sink,
		[] { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); },
		[&] { return focused; }, adapters::sqlite::load_pull_request_activity(root), std::make_shared<printed_log>());
	sink.set_activation([&] {
		const auto chosen = host->activate();
		std::cout << "ACTIVITY_LIVE:click:" << (chosen.PullRequest == target ? "opens-the-pull-request" : "other") << '\n'
				  << std::flush;
	});

	// Phases, advanced after each answered poll.
	enum class phase : std::uint8_t { baseline,
									  first_comment,
									  focused_comment,
									  done };
	auto handed = std::size_t{ 0U };
	host->set_refresh([&](const std::vector<core::pull_request_ref>& aActive) {
		const auto ours = std::ranges::find(aActive, target) != aActive.end();
		handed += ours ? 1U : 0U;
		std::cout << "ACTIVITY_LIVE:refresh:" << (ours ? "the-pull-request" : "other") << ":count=" << aActive.size() << '\n'
				  << std::flush;
	});
	auto       stage    = phase::baseline;
	auto       waited   = 0;
	auto       before   = std::size_t{ 0U };
	auto       suppress = std::uint64_t{ 0U };
	const auto comment  = [&](const char* aWhich) {
		const auto code = QProcess::execute(commentTool, { QString::fromLatin1(aWhich) });
		std::cout << "ACTIVITY_LIVE:comment:" << aWhich << ":" << (code == 0 ? "posted" : "failed") << '\n'
				  << std::flush;
		if (code != 0) {
			fail("comment");
		}
	};
	const auto summary = [&] {
		const auto stats = host->statistics();
		std::cout << "ACTIVITY_LIVE:summary:polls=" << stats.Polls << ":requests=" << stats.Requests << ":unchanged=" << stats.Unchanged << ":changed=" << stats.Changed
				  << ":failed=" << stats.Failed << ":rate-limited=" << stats.RateLimited << ":posted=" << stats.Posted << ":suppressed=" << stats.Suppressed << ":active=" << stats.Active << '\n'
				  << std::flush;
	};
	host->set_observer([&] {
		std::cout << "ACTIVITY_LIVE:next-poll-seconds:" << host->scheduled().value_or(0U) << '\n'
				  << std::flush;
		const auto ours = static_cast<std::size_t>(std::ranges::count(posted, title));
		switch (stage) {
		case phase::baseline:
			if (host->memory().Baselined) {
				stage  = phase::first_comment;
				before = ours;
				waited = 0;
				comment("first");
			}
			return;
		case phase::first_comment:
			++waited;
			if (ours == before + 1U) {
				std::cout << "ACTIVITY_LIVE:first-comment-posted-after-polls:" << waited << '\n'
						  << std::flush;
				if (handed != 1U) {
					summary();
					fail("first-comment-not-handed-on-to-read-again");
				}
				emit sink.tray_icon()->messageClicked();
				focused  = { target };
				suppress = host->statistics().Suppressed;
				before   = ours;
				stage    = phase::focused_comment;
				waited   = 0;
				comment("second");
			} else if (waited >= 5) {
				summary();
				fail("first-comment-not-posted");
			}
			return;
		case phase::focused_comment:
			++waited;
			if (ours != before) {
				summary();
				fail("focused-pull-request-was-posted");
			}
			if (host->statistics().Suppressed > suppress) {
				std::cout << "ACTIVITY_LIVE:focused-comment-suppressed-after-polls:" << waited << '\n'
						  << std::flush;
				if (handed != 2U) {
					summary();
					fail("focused-comment-not-handed-on-to-read-again");
				}
				stage = phase::done;
				summary();
				// Two more polls show the 304 path again before it stops.
				QTimer::singleShot(std::chrono::seconds{ 130 }, qApp, [&] {
					summary();
					std::cout << "ACTIVITY_LIVE:PASS\n"
							  << std::flush;
					host->shutdown();
					QApplication::quit();
				});
			} else if (waited >= 5) {
				summary();
				fail("focused-comment-not-seen");
			}
			return;
		case phase::done:
			return;
		}
	});

	auto session                            = application::personal_discovery_session{ };
	session.Queue                           = { .Host = "github.com", .Account = text("login"), .Generation = 35U, .ViewerAccount = core::github_account_id{ number("id") } };
	session.Context.ApplicationSession      = core::application_session_id{ 32U };
	session.Context.Profile                 = core::host_profile_id{ profile_id };
	session.Context.Host                    = core::git_host_id{ host_id };
	session.Context.Account                 = core::github_account_id{ number("id") };
	session.Context.IdentityGeneration      = core::github_identity_generation{ 35U };
	session.ProfileAuthority.Profile        = core::host_profile_id{ profile_id };
	session.ProfileAuthority.Authentication = application::github_authentication_transport::github_cli;
	host->adopt(session);
	return QApplication::exec();
}
