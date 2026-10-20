/// \file
/// \brief [[BPR-1671]]. The updater panel in each of its states, for the
/// owner's look, through the real updater composition.
///
/// \details `release_update_desktop::start` composes the update window, the
/// Settings "Software Update" group and the Check for Updates menu item exactly
/// as the application does. Only the release access, verification,
/// installation and the kept update settings are fixtures: nothing reaches
/// GitHub, nothing is installed and nothing is written to disk.
///
/// Without arguments the course walks up to date, an offer with Markdown
/// notes, a download and a ready update, and checks each headline; with
/// `BPR_UPDATE_LOOK_RENDER_ROOT` it saves each state's windows there.
/// `--attended` leaves the windows open beside a small look-controls window
/// that chooses what the next check and download meet, and prints
/// `RELEASE_UPDATE_LOOK_SHOWN` once they show.
#include "../support/test_expectation.hpp"
#include "bpr/app/release_update_desktop.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/release_update_panel.hpp"
#include "bpr/qt/settings_window.hpp"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDir>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextStream>
#include <QThread>
#include <QVBoxLayout>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>

namespace {
	using namespace bpr;
	using phase  = core::release_update_phase;
	using reason = core::release_update_reason;
	using bpr::tests::expect;

	constexpr auto installed_version = "0.3.1";
	constexpr auto offered_tag       = "v0.4.0";
	constexpr auto offered_asset     = "BetterPullReview-0.4.0-macos-universal.zip";
	constexpr auto offered_size      = std::uint64_t{ 48'300'000U };
	// Markdown, as GitHub release notes are written: the panel renders it in
	// the pull request overview's reader.
	constexpr auto offered_notes = "## What's new in 0.4.0\n"
								   "\n"
								   "- **Faster first open** of large pull requests.\n"
								   "- Review Cycles can hand a round to your agent.\n"
								   "- See the [full changelog](https://github.com/MikeMarcin/BetterPullReview/releases).\n"
								   "\n"
								   "### Fixed\n"
								   "\n"
								   "- Comments no longer jump when a thread resolves.\n"
								   "- `Check Now` works without a github.com sign-in.\n";

	/// What the next check meets.
	enum class check_outcome : int { offer,
									 current,
									 offline,
									 rate_limited,
									 no_asset };
	/// What the next download meets.
	enum class download_outcome : int { succeeds,
										damaged };

	struct look_state
	{
		std::atomic<int>          Check{ static_cast<int>(check_outcome::current) };
		std::atomic<int>          Download{ static_cast<int>(download_outcome::succeeds) };
		std::chrono::milliseconds CheckDelay{ 0 };
		std::chrono::milliseconds StepDelay{ 0 };
		std::atomic<unsigned>     Checks{ 0U };
		std::atomic<bool>         Unsaved{ false };
		static constexpr unsigned Steps = 75U;
	};

	/// Sleeps in short slices so a stop request is noticed at once.
	auto pause(std::chrono::milliseconds aDelay, const std::stop_token& aStop) -> bool
	{
		const auto until = std::chrono::steady_clock::now() + aDelay;
		while (std::chrono::steady_clock::now() < until) {
			if (aStop.stop_requested()) {
				return false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds{ 20 });
		}
		return !aStop.stop_requested();
	}

	struct fixture_access final : application::release_update_access_port
	{
		fixture_access(std::shared_ptr<look_state> aState, core::release_update_access_id aBinding)
			: State{ std::move(aState) }
			, Binding{ aBinding }
		{ }
		auto binding() const noexcept -> core::release_update_access_id override { return Binding; }
		auto kind() const noexcept -> core::release_update_access_kind override { return core::release_update_access_kind::native_github_com; }
		auto latest(std::stop_token aStop) -> application::release_update_check_result override
		{
			++State->Checks;
			if (!pause(State->CheckDelay, aStop)) {
				return { reason::cancelled, { }, 0U, { } };
			}
			auto release = core::release_update_release{ core::release_update_source::better_pull_review_github_com, core::release_update_release_id{ 40U }, offered_tag, false, false, { { core::release_update_asset_id{ 41U }, offered_asset, offered_size, { }, true } } };
			switch (static_cast<check_outcome>(State->Check.load())) {
			case check_outcome::offer: return { reason::none, std::move(release), 0U, offered_notes };
			case check_outcome::current:
				release.Tag            = std::string{ "v" } + installed_version;
				release.Assets[0].Name = std::string{ "BetterPullReview-" } + installed_version + "-macos-universal.zip";
				return { reason::none, std::move(release), 0U, { } };
			case check_outcome::offline:      return { reason::offline, { }, 0U, { } };
			case check_outcome::rate_limited: return { reason::rate_limited, { }, 60U, { } };
			case check_outcome::no_asset:
				release.Assets[0].Name = "BetterPullReview-0.4.0-windows-x64.zip";
				return { reason::none, std::move(release), 0U, offered_notes };
			}
			return { reason::offline, { }, 0U, { } };
		}
		auto download(const core::release_update_candidate& aCandidate, std::stop_token aStop,
					  const std::function<void(std::uint64_t)>& aProgress) -> application::release_update_download_result override
		{
			for (auto step = 1U; step <= look_state::Steps; ++step) {
				if (!pause(State->StepDelay, aStop)) {
					return { reason::cancelled, { }, 0U, 0U };
				}
				aProgress(offered_size * step / look_state::Steps);
			}
			return { reason::none, std::make_shared<application::release_update_archive>(aCandidate), offered_size, 0U };
		}
		std::shared_ptr<look_state>    State;
		core::release_update_access_id Binding;
	};

	struct fixture_verifier final : application::release_update_verification_port
	{
		explicit fixture_verifier(std::shared_ptr<look_state> aState)
			: State{ std::move(aState) }
		{ }
		auto verify(std::shared_ptr<const application::release_update_archive> aArchive, std::stop_token aStop) -> application::release_update_verification_result override
		{
			if (!pause(State->StepDelay * 8, aStop)) {
				return { reason::cancelled, { } };
			}
			if (static_cast<download_outcome>(State->Download.load()) == download_outcome::damaged) {
				return { reason::digest_mismatch, { } };
			}
			return { reason::none, std::make_shared<application::release_update_verified_bundle>(aArchive->candidate()) };
		}
		std::shared_ptr<look_state> State;
	};

	/// Hands off without installing anything.
	struct fixture_installer final : application::release_update_install_port
	{
		auto install(std::shared_ptr<const application::release_update_verified_bundle>, std::stop_token, const std::function<bool()>&) -> reason override
		{
			return reason::none;
		}
	};

	/// Update settings kept in memory; `Unsaved` makes them fail as storage
	/// that cannot keep them would.
	struct memory_settings final : application::release_update_settings_port
	{
		explicit memory_settings(std::shared_ptr<look_state> aState)
			: State{ std::move(aState) }
		{ }
		auto load() -> std::expected<application::release_update_settings, reason> override
		{
			const std::scoped_lock lock{ Mutex };
			if (State->Unsaved) {
				return std::unexpected(reason::unavailable);
			}
			return Kept;
		}
		auto save(const application::release_update_settings& aSettings) -> bool override
		{
			const std::scoped_lock lock{ Mutex };
			if (State->Unsaved) {
				return false;
			}
			Kept = aSettings;
			return true;
		}
		std::shared_ptr<look_state>          State;
		std::mutex                           Mutex;
		application::release_update_settings Kept;
	};

	void until(const std::function<bool()>& aCondition, const char* aMessage, int aMilliseconds = 20000)
	{
		QElapsedTimer timer;
		timer.start();
		while (!aCondition() && timer.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	/// One composition: the desktop updater, its menu and a Settings window.
	struct look
	{
		std::shared_ptr<look_state>                  State;
		std::unique_ptr<app::release_update_desktop> Desktop;
		std::unique_ptr<qt::settings_window>         Settings = std::make_unique<qt::settings_window>();
		std::unique_ptr<QMenuBar>                    Menu     = std::make_unique<QMenuBar>();

		look(std::shared_ptr<look_state> aState, bool aCanUpdate)
			: State{ std::move(aState) }
		{
			auto       access     = std::make_shared<fixture_access>(State, core::release_update_access_id{ 1U });
			const auto capability = aCanUpdate ? core::release_update_capability::eligible : core::release_update_capability::untrusted_installation;
			auto       components = app::release_update_components{ };
			components.Installed  = installed_version;
			components.Capability = capability;
			components.Settings   = std::make_shared<memory_settings>(State);
			components.Verifier   = std::make_shared<fixture_verifier>(State);
			components.Browser    = [](std::string aAddress) { QTextStream{ stdout } << "RELEASE_PAGE_REQUESTED:" << QString::fromStdString(aAddress) << '\n'; };
			components.Installer  = std::make_shared<fixture_installer>();
			Desktop               = app::release_update_desktop::start(std::move(components));
			Desktop->attach(*Settings);
			Desktop->attach(*Menu);
			// The binding the native composition offers for public releases.
			Desktop->set_accounts({ { "public:github.com", "Public releases (no sign-in)", [access](auto, auto, auto aDone) { aDone(access); } } });
			until([this] { return Desktop->projection().Installed.has_value(); }, "update settings load");
			if (aCanUpdate) {
				until([this] { return Desktop->projection().Commands.front().enabled(); }, "release access is selected without a picker");
			}
		}
		~look()
		{
			Desktop->shutdown();
			until([this] { return Desktop->quiescent(); }, "updater drains");
		}
		look(const look&)                                  = delete;
		auto               operator=(const look&) -> look& = delete;
		void               check() const { Menu->findChild<QAction*>(QStringLiteral("checkForUpdatesAction"))->trigger(); }
		[[nodiscard]] auto headline() const -> QString
		{
			const auto* status = Settings->findChild<QLabel*>(QStringLiteral("releaseUpdateStatus"));
			return status ? status->text() : QString{ };
		}
	};

	void render(const look& aLook, const QString& aName)
	{
		const auto directory = qEnvironmentVariable("BPR_UPDATE_LOOK_RENDER_ROOT");
		if (directory.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(directory), "render root exists");
		QApplication::processEvents();
		if (auto* panel = aLook.Desktop->panel()) {
			expect(panel->window()->grab().save(directory + QStringLiteral("/window-%1.png").arg(aName)), "update window render saved");
		}
		expect(aLook.Settings->grab().save(directory + QStringLiteral("/settings-%1.png").arg(aName)), "Settings render saved");
	}

	void course()
	{
		auto state       = std::make_shared<look_state>();
		state->StepDelay = std::chrono::milliseconds{ 20 };
		look composed{ state, true };
		composed.Settings->present(qt::settings_page::general);
		expect(composed.Desktop->durable(), "update settings are kept");
		expect(composed.Settings->findChild<QGroupBox*>(QStringLiteral("releaseUpdateGroup"))->title() == QStringLiteral("Software Update"), "Settings names the group");
		render(composed, QStringLiteral("never-checked"));

		composed.check();
		until([&] { return composed.Desktop->projection().Reason == reason::current && composed.Desktop->panel(); }, "a check finds nothing newer");
		expect(composed.headline() == QStringLiteral("You’re up to date"), "up-to-date headline");
		render(composed, QStringLiteral("up-to-date"));

		state->Check = static_cast<int>(check_outcome::offer);
		composed.check();
		until([&] { return composed.Desktop->projection().Phase == phase::available; }, "a check finds the offer");
		expect(composed.headline() == QStringLiteral("Version 0.4.0 is available"), "offer headline");
		const auto* notes = dynamic_cast<const qt::markdown_body_view*>(composed.Desktop->panel()->findChild<QWidget*>(QStringLiteral("releaseUpdateNotes")));
		expect(notes && notes->rendered_available() && notes->rendered_view()->document()->begin().blockFormat().headingLevel() == 2, "release notes read as rendered Markdown");
		render(composed, QStringLiteral("available"));

		composed.Desktop->panel()->action(core::release_update_command::download)->trigger();
		until([&] { return composed.Desktop->projection().Phase == phase::downloading && composed.Desktop->projection().Bytes > 0U; }, "download in progress");
		expect(composed.headline() == QStringLiteral("Downloading version 0.4.0…"), "downloading headline");
		render(composed, QStringLiteral("downloading"));

		until([&] { return composed.Desktop->projection().Phase == phase::ready; }, "download verified and ready");
		expect(composed.headline() == QStringLiteral("Ready to install version 0.4.0"), "ready headline");
		render(composed, QStringLiteral("ready"));

		state->Check = static_cast<int>(check_outcome::offline);
		composed.Desktop->panel()->action(core::release_update_command::later)->trigger();
		composed.check();
		until([&] { return composed.Desktop->projection().Reason == reason::offline; }, "an offline check is reported");
		expect(composed.headline() == QStringLiteral("Couldn’t check for updates"), "offline headline");
		render(composed, QStringLiteral("offline"));
	}

	/// The two arrangements the panel only explains: a copy that cannot
	/// update itself, and update settings that cannot be kept.
	void limited()
	{
		auto state = std::make_shared<look_state>();
		{
			look composed{ state, false };
			composed.Settings->present(qt::settings_page::general);
			const auto* page = composed.Settings->findChild<QPushButton*>(QStringLiteral("releaseUpdateViewRelease"));
			expect(page && page->isVisible() && page->isDefault(), "a copy that cannot update itself leads with the release page");
			render(composed, QStringLiteral("cannot-update"));
		}
		state->Unsaved = true;
		look composed{ state, true };
		composed.Settings->present(qt::settings_page::general);
		const auto* notice = composed.Settings->findChild<QLabel*>(QStringLiteral("releaseUpdateNotice"));
		expect(!composed.Desktop->durable() && notice && notice->isVisible(), "settings that cannot be kept are named");
		render(composed, QStringLiteral("unsaved"));
	}

	/// The owner's look: the composition stays open beside its controls.
	auto attended() -> int
	{
		auto state        = std::make_shared<look_state>();
		state->CheckDelay = std::chrono::milliseconds{ 1500 };
		state->StepDelay  = std::chrono::milliseconds{ 200 };
		auto composed     = std::make_unique<look>(state, true);

		QWidget controls{ nullptr, Qt::Window };
		controls.setWindowTitle(QStringLiteral("BPR-1671 look controls"));
		auto* column = new QVBoxLayout{ &controls };
		auto* intro  = new QLabel{ QStringLiteral("Not part of BetterPullReview. Choose what the next check or download meets, "
												  "then use the update window or Settings > General as usual. "
												  "Nothing reaches GitHub and nothing is installed."),
								   &controls };
		intro->setWordWrap(true);
		column->addWidget(intro);

		auto* checks      = new QGroupBox{ QStringLiteral("The next check finds"), &controls };
		auto* checkColumn = new QVBoxLayout{ checks };
		auto* checkGroup  = new QButtonGroup{ &controls };
		for (const auto& [value, label] : { std::pair{ check_outcome::current, "Nothing newer (you have 0.3.1)" }, std::pair{ check_outcome::offer, "Version 0.4.0, with Markdown release notes" },
											std::pair{ check_outcome::offline, "GitHub can't be reached" }, std::pair{ check_outcome::rate_limited, "GitHub is limiting requests" },
											std::pair{ check_outcome::no_asset, "Version 0.4.0 with no download for this platform" } }) {
			auto* button = new QRadioButton{ QString::fromUtf8(label), checks };
			button->setChecked(value == check_outcome::current);
			checkGroup->addButton(button, static_cast<int>(value));
			checkColumn->addWidget(button);
		}
		QObject::connect(checkGroup, &QButtonGroup::idToggled, &controls, [state](int aId, bool aOn) { if (aOn) { state->Check = aId; } });
		column->addWidget(checks);

		auto* downloads      = new QGroupBox{ QStringLiteral("The download"), &controls };
		auto* downloadColumn = new QVBoxLayout{ downloads };
		auto* downloadGroup  = new QButtonGroup{ &controls };
		for (const auto& [value, label] : { std::pair{ download_outcome::succeeds, "Takes about 15 seconds and succeeds" }, std::pair{ download_outcome::damaged, "Arrives damaged" } }) {
			auto* button = new QRadioButton{ QString::fromUtf8(label), downloads };
			button->setChecked(value == download_outcome::succeeds);
			downloadGroup->addButton(button, static_cast<int>(value));
			downloadColumn->addWidget(button);
		}
		QObject::connect(downloadGroup, &QButtonGroup::idToggled, &controls, [state](int aId, bool aOn) { if (aOn) { state->Download = aId; } });
		column->addWidget(downloads);

		auto* checkNow     = new QPushButton{ QStringLiteral("Check for Updates…"), &controls };
		auto* openSettings = new QPushButton{ QStringLiteral("Open Settings > General"), &controls };
		auto* cannotUpdate = new QCheckBox{ QStringLiteral("Start over as a copy that can't update itself"), &controls };
		auto* unsaved      = new QCheckBox{ QStringLiteral("Start over with update settings that can't be saved"), &controls };
		auto* startOver    = new QPushButton{ QStringLiteral("Start Over"), &controls };
		column->addWidget(checkNow);
		column->addWidget(openSettings);
		column->addSpacing(8);
		column->addWidget(cannotUpdate);
		column->addWidget(unsaved);
		column->addWidget(startOver);
		auto* quit = new QPushButton{ QStringLiteral("Quit"), &controls };
		column->addSpacing(8);
		column->addWidget(quit);
		QObject::connect(quit, &QPushButton::clicked, qApp, &QApplication::quit);
		QObject::connect(checkNow, &QPushButton::clicked, &controls, [&composed] { composed->check(); });
		QObject::connect(openSettings, &QPushButton::clicked, &controls, [&composed] { composed->Settings->present(qt::settings_page::general); });
		QObject::connect(startOver, &QPushButton::clicked, &controls, [&composed, state, cannotUpdate, unsaved] {
			const auto place = composed->Settings->pos();
			composed.reset();
			state->Unsaved = unsaved->isChecked();
			composed       = std::make_unique<look>(state, !cannotUpdate->isChecked());
			composed->Settings->move(place);
			composed->Settings->present(qt::settings_page::general);
		});

		const auto area = QApplication::primaryScreen()->availableGeometry();
		controls.setFixedWidth(340);
		controls.move(area.left() + 24, area.top() + 24);
		controls.show();
		composed->Settings->move(area.left() + 390, area.top() + 24);
		composed->Settings->present(qt::settings_page::general);
		composed->check();
		until([&] { return composed->Desktop->panel() && composed->Desktop->panel()->window()->isVisible() && composed->Settings->isVisible(); }, "update window and Settings show");
		QTextStream{ stdout } << "RELEASE_UPDATE_LOOK_SHOWN\n";
		QTextStream{ stdout }.flush();
		const auto result = QApplication::exec();
		composed.reset();
		return result;
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setApplicationName(QStringLiteral("BetterPullReview"));
	QApplication::setApplicationVersion(QString::fromUtf8(installed_version));
	if (aArgc == 2 && std::string_view{ aArgv[1] } == "--attended") {
		return attended();
	}
	expect(aArgc == 1, "usage: test_release_update_look [--attended]");
	course();
	limited();
	std::cout << "release-update-look:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
