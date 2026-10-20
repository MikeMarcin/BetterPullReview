#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/release_update_recovery_store.hpp"
#include "bpr/adapters/sqlite/release_update_store.hpp"
#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#if defined(__APPLE__)
	#include "bpr/adapters/update/release_update_installation.hpp"
	#include "bpr/adapters/update/release_update_staging.hpp"
#endif
#include "bpr/app/profile_desktop.hpp"
#include "bpr/app/release_update_desktop.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/qt/release_update_panel.hpp"
#include "bpr/qt/release_update_recovery_dialog.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/settings_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFont>
#include <QLabel>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextList>
#include <QThread>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
	using namespace bpr;
	using phase   = core::release_update_phase;
	using command = core::release_update_command;
	using reason  = core::release_update_reason;
	using bpr::tests::expect;
	// Untrusted remote text: Markdown with an HTML tag, a link and images
	// whose sources, remote and local, must never load.
	constexpr auto fixture_notes = "# Fixture release\n"
								   "\n"
								   "- <b>Fixture private release notes</b>\n"
								   "- See [the changelog](https://example.invalid/changes) ![tracker](https://example.invalid/pixel.png) ![local](pixel.png)\n";
	void           until(const std::function<bool()>& aCondition, const char* aMessage, int aMilliseconds = 12000)
	{
		QElapsedTimer timer;
		timer.start();
		while (!aCondition() && timer.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	struct clock final : application::release_update_clock_port
	{
		core::release_update_time Time{ 100000, 100U };
		auto                      now() noexcept -> core::release_update_time override { return Time; }
	};
	struct access final : application::release_update_access_port
	{
		std::atomic<unsigned> Checks = 0U, Downloads = 0U;
		std::atomic<bool>     Revoked = false;
		reason                Failure = reason::none;
		auto                  binding() const noexcept -> core::release_update_access_id override { return core::release_update_access_id{ Revoked ? 0U : 7U }; }
		auto                  kind() const noexcept -> core::release_update_access_kind override { return core::release_update_access_kind::native_github_com; }
		auto                  latest(std::stop_token) -> application::release_update_check_result override
		{
			++Checks;
			return { Failure, { core::release_update_source::better_pull_review_github_com, core::release_update_release_id{ 10U }, "v2.3.4", false, false, { { core::release_update_asset_id{ 20U }, "BetterPullReview-2.3.4-macos-universal.zip", 100U, { }, true } } }, 0U, std::string{ fixture_notes } };
		}
		auto download(const core::release_update_candidate&     aCandidate, std::stop_token,
					  const std::function<void(std::uint64_t)>& aProgress) -> application::release_update_download_result override
		{
			++Downloads;
			aProgress(50U);
			aProgress(100U);
			return { reason::none, std::make_shared<application::release_update_archive>(aCandidate), 100U, 0U };
		}
	};
	struct verifier final : application::release_update_verification_port
	{
		std::atomic<unsigned> Calls = 0U;
		auto                  verify(std::shared_ptr<const application::release_update_archive> aArchive, std::stop_token) -> application::release_update_verification_result override
		{
			++Calls;
			return { reason::none, std::make_shared<application::release_update_verified_bundle>(aArchive->candidate()) };
		}
	};
	struct installer final : application::release_update_install_port
	{
		std::atomic<unsigned> Calls = 0U;
		auto                  install(std::shared_ptr<const application::release_update_verified_bundle> aBundle, std::stop_token aStop, const std::function<bool()>& aAuthorized) -> reason override
		{
			expect(aBundle && !aStop.stop_requested() && aAuthorized(), "public Install gesture retains verified capability and authority");
			++Calls;
			return reason::none;
		}
	};
	auto root(QTemporaryDir& aTemporary) -> std::filesystem::path
	{
		expect(aTemporary.isValid(), "temporary root exists");
		// [[BPR-1031]]. The course reopens the recovery store itself after the
		// desktop is gone, so it names the root as the adapters admit it: Qt's
		// '/' spelling is refused on Windows as an invalid path.
		const auto path = e2e::private_data_root(aTemporary);
		expect(path.has_value(), "private fixture state root");
		return *path;
	}
	void root_wiring()
	{
		QTemporaryDir                                   temporary;
		app::detail::github_public_path_fixture_options options;
		options.ApplicationDataRoot      = root(temporary);
		app::github_application* owner   = nullptr;
		auto                     desktop = app::detail::github_application_fixture_access::start_desktop(
			app::detail::github_public_path_fixture::profile_window_restoration_v1, options,
			[&](auto aResult) { if (aResult) { owner = *aResult; } });
		until([&] { return owner && !owner->windows().empty(); }, "production desktop starts");
		auto* window = owner->window(owner->windows().front());
		auto* action = window->findChild<QAction*>(QStringLiteral("checkForUpdatesAction"));
		expect(action, "production root composes the menu action");
		action->trigger();
		window->open_settings(qt::settings_page::general);
		until([&] { return window->settings() && window->settings()->findChild<QWidget*>(QStringLiteral("releaseUpdatePanel")); }, "production root composes Settings");
		auto* status = window->settings()->findChild<QLabel*>(QStringLiteral("releaseUpdateStatus"));
		until([&] { return status && !status->text().isEmpty(); }, "native unsupported/development projection arrives");
		auto* newWindow = window->findChild<QAction*>(QStringLiteral("newBprWindowAction"));
		expect(newWindow, "public new window gesture exists");
		newWindow->trigger();
		until([&] { return owner->windows().size() == 2U; }, "update barrier has two real windows");
		unsigned editor = 0U;
		for (const auto context : owner->windows()) {
			owner->window(context)->review_editor()->body_editor()->setPlainText(QStringLiteral("Retain update text %1").arg(++editor));
		}
		const auto recoveryPath = *options.ApplicationDataRoot / "state/v1/update-draft-recovery.sqlite3";
		const auto retainedPath = *options.ApplicationDataRoot / "retained-recovery.sqlite3";
		std::filesystem::rename(recoveryPath, retainedPath);
		std::filesystem::create_directory(recoveryPath);
		bool failedSave = false, faultPending = false, faultStarted = false;
		until([&] {
			if (!failedSave && !faultPending) {
				faultPending = true;
				faultStarted = false;
				desktop->quit_for_update([&](bool aAccepted) {
					// [[BPR-1121]]. The event loop calls this, and a throw may not cross it, so a failure here exits.
					const bpr::tests::exit_on_failure inTheEventLoop;
					expect(!aAccepted, "failed draft save refuses Quit");
					failedSave   = faultStarted;
					faultPending = false;
				});
				// A synchronous busy refusal is not the injected save-failure course.
				faultStarted = !window->isEnabled();
			}
			return failedSave;
		},
			  "injected local persistence failure returns without quitting");
		expect(!desktop->is_quiescent() && owner->windows().size() == 2U && window->isEnabled(), "failed update leaves both windows usable");
		expect(std::filesystem::remove(recoveryPath), "remove only the task-owned empty obstruction");
		std::filesystem::rename(retainedPath, recoveryPath);
		bool accepted = false, pending = false;
		until([&] {
			if (!accepted && !pending) {
				pending = true;
				desktop->quit_for_update([&](bool aAccepted) { accepted = aAccepted; pending = false; });
			}
			return accepted;
		},
			  "update barrier saves text before requesting orderly Quit");
		until([&] { return desktop->is_quiescent(); }, "root updater participates in orderly desktop drain");
		expect(desktop->exit_code() == 0, "all update shutdown flushes succeed");
		desktop.reset();
		const auto recovered = adapters::sqlite::make_release_update_recovery_store(*options.ApplicationDataRoot)->load();
		expect(recovered && recovered->size() == 2U && (*recovered)[0].Body != (*recovered)[1].Body, "both windows retain distinct text durably");
		qt::release_update_recovery_dialog recovery{ *recovered };
		const auto*                        text = recovery.findChild<QPlainTextEdit*>(QStringLiteral("updateRecoveryText"));
		expect(text && text->isReadOnly() && text->toPlainText().startsWith(QStringLiteral("Retain update text")), "recovery presents inert copyable text");
	}
	void gestures_restart_and_staging()
	{
		QTemporaryDir                        temporary;
		const auto                           stateRoot = root(temporary);
		const auto                           store     = adapters::sqlite::make_release_update_store(stateRoot);
		auto                                 time      = std::make_shared<clock>();
		auto                                 wire      = std::make_shared<access>();
		auto                                 validator = std::make_shared<verifier>();
		auto                                 install   = std::make_shared<installer>();
		application::release_update_settings initial;
		initial.Preferences.AutomaticChecks = false;
		expect(store->save(initial), "initial app-wide preferences saved");
		std::string          browser;
		auto                 start   = [&](bool aInstall = false) { return app::release_update_desktop::start({ "2.3.3", core::release_update_capability::eligible, time, store, validator, { }, [&](std::string aAddress) { browser = std::move(aAddress); }, aInstall ? install : nullptr }); };
		auto                 desktop = start();
		qt::workbench_window window;
		qt::settings_window  settings;
		desktop->attach(window);
		desktop->attach(window);
		expect(window.findChildren<QAction*>(QStringLiteral("checkForUpdatesAction")).size() == 1, "menu attachment is idempotent");
		desktop->attach(settings);
		const auto                        binding = std::string(64U, 'a');
		const app::release_update_account account{ binding, "Fixture account", [wire](auto, auto, auto aDone) { aDone(wire); } };
		desktop->set_accounts({ account });
		until([&] { return desktop->projection().Installed.has_value(); }, "settings loaded before gestures");
		auto* panel = dynamic_cast<qt::release_update_panel*>(settings.findChild<QWidget*>(QStringLiteral("releaseUpdatePanel")));
		expect(panel, "Settings owns live update panel");
		expect(!panel->findChild<QComboBox*>(), "no release-account picker");
		until([&] { return desktop->projection().Commands.front().enabled(); }, "the offered release access is selected without a picker");
		auto* menu = window.findChild<QAction*>(QStringLiteral("checkForUpdatesAction"));
		expect(menu, "public menu entry exists");
		menu->trigger();
		until([&] { return desktop->projection().Phase == phase::available; }, "menu reaches host and accepted offer");
		expect(wire->Checks == 1U && desktop->panel(), "one manual request and app-wide modeless offer");
		const auto screenshot = qEnvironmentVariable("BPR_UPDATE_SCREENSHOT_PATH");
		if (!screenshot.isEmpty()) {
			expect(desktop->panel()->grab().save(screenshot), "fixture-only offer capture");
		}
		expect(desktop->projection().Preferences.LastSuccess == time->Time.WallSeconds, "successful timestamp projected");
		const auto* notes = panel->findChild<QTextEdit*>(QStringLiteral("releaseUpdateNotes"));
		expect(notes && notes->isReadOnly() && !notes->isHidden(), "release notes show read-only beside the offer");
		expect(!(notes->textInteractionFlags() & (Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard)), "release notes follow no link");
		auto* document = notes->document();
		expect(document->begin().blockFormat().headingLevel() == 1 && document->begin().text() == QStringLiteral("Fixture release"), "a Markdown heading renders as a heading");
		expect(document->begin().begin().fragment().charFormat().fontWeight() >= QFont::DemiBold, "the heading is set in semibold");
		auto items = 0;
		auto tag   = false;
		for (auto block = document->begin(); block.isValid(); block = block.next()) {
			items += block.textList() != nullptr ? 1 : 0;
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				const auto fragment = it.fragment();
				expect(!fragment.charFormat().isImageFormat(), "no image object is laid out");
				expect(!fragment.charFormat().isAnchor() && !fragment.charFormat().hasProperty(QTextFormat::AnchorHref), "no link destination remains");
				if (fragment.text().contains(QStringLiteral("<b>Fixture private release notes</b>"))) {
					tag = block.textList() != nullptr;
					expect(fragment.charFormat().fontWeight() < QFont::DemiBold, "an HTML tag is not interpreted as bold");
				}
			}
		}
		expect(items == 2, "a Markdown list renders as two list items");
		expect(tag, "an HTML tag reads as its literal text inside the list item");
		const auto text = notes->toPlainText();
		expect(text.contains(QStringLiteral("See the changelog tracker local")) && !text.contains(QStringLiteral("example.invalid")), "a link reads as its text and an image as its alternative text");
		expect(document->resource(QTextDocument::ImageResource, QUrl{ QStringLiteral("https://example.invalid/pixel.png") }).toByteArray().isEmpty()
				   && document->resource(QTextDocument::ImageResource, QUrl{ QStringLiteral("pixel.png") }).toByteArray().isEmpty(),
			   "release notes load no remote or local resource");
		panel->action(command::later)->trigger();
		expect(!desktop->projection().OfferVisible, "Later hides current offer");
		panel->action(command::check)->trigger();
		until([&] { return desktop->projection().OfferVisible; }, "manual check reoffers deferred version");
		panel->action(command::skip)->trigger();
		auto* automatic = panel->findChild<QCheckBox*>(QStringLiteral("releaseUpdateAutomatic"));
		automatic->click();
		desktop->shutdown();
		until([&] { return desktop->quiescent(); }, "preferences flush before shutdown");
		desktop.reset();
		const auto persisted = store->load();
		expect(persisted && persisted->AccountBinding == binding && persisted->Preferences.SkippedVersion && persisted->Preferences.AutomaticChecks, "opaque account, skip and automatic preference persist");
		desktop = start();
		QMenuBar menuBar;
		desktop->attach(menuBar);
		desktop->set_accounts({ account });
		until([&] { return desktop->projection().Installed && desktop->quiescent(); }, "restart preferences restored");
		const auto previous = wire->Checks.load();
		time->Time.WallSeconds += 90000;
		time->Time.MonotonicSeconds += 90000U;
		until([&] { return wire->Checks > previous && desktop->quiescent(); }, "daily timer checks once after restart");
		expect(!desktop->projection().OfferVisible && desktop->projection().Reason == reason::skipped, "automatic check respects persisted skip");
		menuBar.findChild<QAction*>(QStringLiteral("checkForUpdatesAction"))->trigger();
		until([&] { return desktop->projection().OfferVisible; }, "manual override ignores skipped version");
		auto* offer = desktop->panel();
		offer->action(command::download)->trigger();
		until([&] { return desktop->projection().Phase == phase::ready; }, "download reaches staged verification and projection");
		expect(wire->Downloads == 1U && validator->Calls == 1U && !offer->action(command::install)->isEnabled(), "verified candidate retained without enabling unimplemented installer");
		wire->Revoked = true;
		until([&] { return !desktop->projection().Candidate; }, "revocation discards staged candidate");
		desktop->shutdown();
		until([&] { return desktop->quiescent(); }, "all fixture effects drained");
		desktop.reset();
		wire->Revoked = false;
		desktop       = start(true);
		desktop->set_accounts({ account });
		QMenuBar installMenu;
		desktop->attach(installMenu);
		until([&] { return desktop->projection().Installed.has_value(); }, "install composition ready");
		until([&] { return desktop->projection().Commands.front().enabled(); }, "install release authority restored");
		installMenu.findChild<QAction*>(QStringLiteral("checkForUpdatesAction"))->trigger();
		until([&] { return desktop->projection().Phase == phase::available; }, "install public offer");
		desktop->panel()->action(command::download)->trigger();
		until([&] { return desktop->projection().Phase == phase::ready; }, "install staged public offer");
		expect(desktop->panel()->action(command::install)->isEnabled(), "only supplied installer capability enables gesture");
		const auto checks = wire->Checks.load();
		desktop->panel()->action(command::install)->trigger();
		until([&] { return desktop->projection().Phase == phase::handed_off; }, "gesture refreshes and transfers to installer");
		expect(wire->Checks == checks + 1U && install->Calls == 1U, "one refreshed authority and exact handoff");
		expect(desktop->observe_installation_result(false) && desktop->projection().Phase == phase::recovered, "journal-proven abort clears handoff and reports recovery");
		desktop->shutdown();
		until([&] { return desktop->quiescent(); }, "install public course drains");
	}
	/// [[BPR-1672]]. A self-updating build offers the public releases first,
	/// and its update settings must save with that access selected. The
	/// readable "public:github.com" binding was refused by the store, so
	/// every save failed and the panel said settings reset on quit.
	void public_release_access_persists()
	{
		QTemporaryDir                        temporary;
		const auto                           store = adapters::sqlite::make_release_update_store(root(temporary));
		auto                                 time  = std::make_shared<clock>();
		auto                                 wire  = std::make_shared<access>();
		application::release_update_settings initial;
		initial.Preferences.AutomaticChecks = false;
		expect(store->save(initial), "initial preferences saved without access");
		const auto publicAccess = app::release_update_public_account();
		expect(publicAccess.Binding.size() == 64U && std::ranges::all_of(publicAccess.Binding, [](char aValue) { return (aValue >= '0' && aValue <= '9') || (aValue >= 'a' && aValue <= 'f'); }),
			   "the public binding has an account binding's form");
		// The native binding and label; only the lease is the fixture's, so no
		// check reaches GitHub.
		auto                desktop = app::release_update_desktop::start({ "2.3.3", core::release_update_capability::eligible, time, store, { }, { }, [](std::string) { } });
		qt::settings_window settings;
		desktop->attach(settings);
		desktop->set_accounts({ { publicAccess.Binding, publicAccess.Label, [wire](auto, auto, auto aDone) { aDone(wire); } } });
		until([&] { return desktop->projection().Installed && desktop->projection().Commands.front().enabled() && desktop->quiescent(); }, "the public releases are selected");
		expect(desktop->durable(), "update settings save with the public releases selected");
		auto* automatic = settings.findChild<QCheckBox*>(QStringLiteral("releaseUpdateAutomatic"));
		expect(automatic && !automatic->isChecked(), "automatic checks start off");
		automatic->click();
		until([&] { return desktop->quiescent(); }, "the preference flushes");
		expect(desktop->durable() && settings.findChild<QLabel*>(QStringLiteral("releaseUpdateNotice"))->isHidden(), "no unsaved-settings notice");
		desktop->shutdown();
		until([&] { return desktop->quiescent(); }, "public course drains");
		desktop.reset();
		const auto persisted = store->load();
		expect(persisted && persisted->AccountBinding == publicAccess.Binding && persisted->Preferences.AutomaticChecks, "the public binding and the preference persist");
	}
	void unavailable_installations()
	{
		const auto binding = std::string(64U, 'a');
		for (const auto capability : { core::release_update_capability::managed, core::release_update_capability::unsupported_platform,
									   core::release_update_capability::untrusted_installation, core::release_update_capability::read_only, core::release_update_capability::translocated, core::release_update_capability::mounted_image }) {
			QTemporaryDir                        temporary;
			const auto                           store = adapters::sqlite::make_release_update_store(root(temporary));
			application::release_update_settings initial;
			initial.AccountBinding = binding;
			expect(store->save(initial), "fallback settings fixture");
			auto                time     = std::make_shared<clock>();
			unsigned            acquired = 0U;
			std::string         browser;
			auto                desktop = app::release_update_desktop::start({ "2.3.3", capability, time, store, { }, { }, [&](std::string aAddress) { browser = std::move(aAddress); } });
			qt::settings_window settings;
			desktop->attach(settings);
			desktop->set_accounts({ { binding, "Fixture account", [&](auto, auto, auto aDone) { ++acquired; aDone({}); } } });
			until([&] { return desktop->projection().Installed.has_value(); }, "fallback projection settles");
			auto* panel = dynamic_cast<qt::release_update_panel*>(settings.findChild<QWidget*>(QStringLiteral("releaseUpdatePanel")));
			expect(panel && !panel->action(command::check)->isEnabled() && !desktop->projection().AutomaticAvailable && desktop->projection().Reason != reason::none, "truthful unavailable controls and reason");
			expect(acquired == 0U, "managed and unsupported installs never acquire credentials");
			panel->findChild<QPushButton*>(QStringLiteral("releaseUpdateViewRelease"))->click();
			expect(browser == "https://github.com/MikeMarcin/BetterPullReview/releases", "manual fallback uses only fixed release browser address");
			desktop->shutdown();
			until([&] { return desktop->quiescent(); }, "fallback service drains");
		}
	}
#if defined(__APPLE__)
	/// The one disposable root a native qualification course may touch. The
	/// code that reads it is macOS-only, but the MSVC gate reads text rather
	/// than branches, and the suppression is what the tree spells everywhere
	/// `std::getenv` is read (BPR-0519, [[BPR-0854]]).
	auto qualification_target_root() -> const char*
	{
	#if defined(_MSC_VER)
		#pragma warning(push)
		#pragma warning(disable : 4996)
	#endif
		return std::getenv("QUALIFICATION_TARGET_ROOT");
	#if defined(_MSC_VER)
		#pragma warning(pop)
	#endif
	}
	void native_state(char** aArguments)
	{
		namespace fs = std::filesystem;
		const fs::path root{ aArguments[2] }, state{ aArguments[3] };
		const auto*    permitted = qualification_target_root();
		expect(permitted && fs::canonical(root) == fs::path{ permitted } && fs::canonical(state) == state
				   && state.string().starts_with(root.string() + "/"),
			   "native state requires exact disposable scope");
		const bool                                                 seed = std::string_view{ aArguments[4] } == "seed";
		const std::vector<application::release_update_saved_draft> drafts{
			{ "native-draft-1", "First window draft", "Retained signed update draft", false },
			{ "native-draft-2", "Second window indeterminate publication", "Check server before publishing again", true }
		};
		auto recovery = adapters::sqlite::make_release_update_recovery_store(state);
		expect(!seed || recovery->save(drafts), "seed real private recovery store");
		const auto retained = recovery->load();
		expect(retained && *retained == drafts, "native draft text and indeterminate marker retained exactly");
		auto store = adapters::sqlite::make_review_mutation_recovery_store({ state, "github.com", "21", "fixture/update-qualification", 1 });
		expect(store.has_value(), "real publication recovery store opens");
		application::review_mutation_recovery_record record;
		record.IdempotencyKey = std::string(64U, 'a');
		record.RequestFingerprint.fill(0x11U);
		record.AuthorityFingerprint.fill(0x22U);
		record.Body = "Unresolved signed update qualification write";
		std::array<std::uint8_t, 32> comparison{ };
		comparison.fill(0x33U);
		std::array<std::uint8_t, 32> commit{ };
		std::fill_n(commit.begin(), 20U, 0x44U);
		record.Anchor                  = { { core::file_comparison_digest{ comparison }, core::diff_side::right, 12U }, "fixture/review.txt", core::git_commit_oid{ core::git_hash_algorithm::sha1, commit } };
		record.BaselineExactCommentIds = std::vector<std::string>{ "PRRC_fixture_40" };
		record.Transmission            = application::review_mutation_recovery_transmission::prepared;
		if (seed) {
			expect((*store)->admit(record).has_value() && (*store)->mark_possibly_transmitted(record.IdempotencyKey, record.RequestFingerprint).has_value(), "seed durable unresolved publication without a network write");
		}
		record.Transmission = application::review_mutation_recovery_transmission::possibly_transmitted;
		const auto pending  = (*store)->load(record.IdempotencyKey);
		expect(pending && *pending && **pending == record, "native restart preserves exact unresolved publication and baseline");
		std::cout << "native-retained-state:pass\n";
	}
	struct signed_access final : application::release_update_access_port
	{
		std::filesystem::path                                      Archive;
		std::string                                                Version;
		std::shared_ptr<application::release_update_archive_store> Store;
		std::atomic<unsigned>                                      Checks = 0U;
		auto                                                       binding() const noexcept -> core::release_update_access_id override { return core::release_update_access_id{ 350U }; }
		auto                                                       kind() const noexcept -> core::release_update_access_kind override { return core::release_update_access_kind::native_github_com; }
		auto                                                       latest(std::stop_token) -> application::release_update_check_result override
		{
			++Checks;
			return { reason::none, { core::release_update_source::better_pull_review_github_com, core::release_update_release_id{ 350U }, "v" + Version, false, false, { { core::release_update_asset_id{ 351U }, "BetterPullReview-" + Version + "-macos-universal.zip", std::filesystem::file_size(Archive), { }, true } } }, 0U, "Disposable signed qualification" };
		}
		auto download(const core::release_update_candidate& aCandidate, std::stop_token aStop, const std::function<void(std::uint64_t)>& aProgress) -> application::release_update_download_result override
		{
			auto  writer = Store->begin(aCandidate);
			QFile input{ QString::fromStdString(Archive.string()) };
			if (!writer || !input.open(QIODevice::ReadOnly)) {
				return { reason::unavailable, { }, 0U, 0U };
			}
			std::uint64_t count = 0U;
			while (!input.atEnd() && !aStop.stop_requested()) {
				const auto bytes = input.read(65536);
				if (bytes.isEmpty() || !writer->append(std::as_bytes(std::span{ bytes.constData(), static_cast<std::size_t>(bytes.size()) }))) {
					return { reason::unavailable, { }, count, 0U };
				}
				count += static_cast<std::uint64_t>(bytes.size());
				aProgress(count);
			}
			return { aStop.stop_requested() ? reason::cancelled : reason::none, writer->finish(), count, 0U };
		}
	};
	void native_handoff(char** aArguments)
	{
		namespace fs = std::filesystem;
		const fs::path root{ aArguments[2] }, archive{ aArguments[3] }, stateRoot{ aArguments[6] };
		const auto*    permitted = qualification_target_root();
		expect(permitted && fs::canonical(root) == fs::path{ permitted }, "native public course requires disposable guard");
		expect(fs::canonical(stateRoot) == stateRoot && stateRoot.string().starts_with(root.string() + "/"), "native restart state stays in disposable root");
		const auto installed = root / "installed/BetterPullReview.app";
		expect(adapters::update::release_update_installation_capability(installed, { }) == core::release_update_capability::eligible, "actual signed baseline is eligible");
		const auto driverRoot = root / "driver";
		fs::create_directory(driverRoot);
		expect(adapters::sqlite::make_storage_root_owner_private(driverRoot), "private driver state");
		app::worker_executor                 cleanup;
		auto                                 staging = adapters::update::make_release_update_staging(driverRoot, installed, cleanup);
		auto                                 store   = adapters::sqlite::make_release_update_store(driverRoot);
		application::release_update_settings initial;
		initial.Preferences.AutomaticChecks = false;
		expect(store->save(initial), "native course requires explicit manual gesture");
		auto wire                    = std::make_shared<signed_access>();
		wire->Archive                = archive;
		wire->Version                = aArguments[5];
		wire->Store                  = staging;
		auto                 desktop = app::release_update_desktop::start({ aArguments[4], core::release_update_capability::eligible, std::make_shared<clock>(), store, staging, staging, { }, adapters::update::make_release_update_installer(installed, stateRoot) });
		qt::workbench_window window;
		qt::settings_window  settings;
		desktop->attach(window);
		desktop->attach(settings);
		desktop->set_accounts({ { std::string(64U, 'b'), "Local fixture authority", [wire](auto, auto, auto aDone) { aDone(wire); } } });
		until([&] { return desktop->projection().Installed.has_value(); }, "native settings loaded");
		auto* panel = dynamic_cast<qt::release_update_panel*>(settings.findChild<QWidget*>(QStringLiteral("releaseUpdatePanel")));
		expect(panel, "public Settings update panel");
		until([&] { return desktop->projection().Commands.front().enabled(); }, "native release access selected without a picker");
		window.findChild<QAction*>(QStringLiteral("checkForUpdatesAction"))->trigger();
		until([&] { return desktop->projection().Phase == phase::available; }, "native menu offer");
		panel->action(command::download)->trigger();
		until([&] { return desktop->projection().Phase == phase::ready || desktop->projection().Phase == phase::failed; }, "native archive staging terminal", 300000);
		expect(desktop->projection().Phase == phase::ready, "actual signed target passes native staging");
		expect(panel->action(command::install)->isEnabled(), "verified candidate permits exact Install and Restart gesture");
		panel->action(command::install)->trigger();
		until([&] { return desktop->projection().Phase == phase::handed_off || desktop->projection().Phase == phase::failed; }, "native helper handoff terminal", 300000);
		expect(desktop->projection().Phase == phase::handed_off && wire->Checks == 2U, "native handoff follows exact release refresh");
		std::cout << "native-public-handoff:accepted\n"
				  << std::flush;
		std::optional<adapters::update::release_update_installation_result> result;
		until([&] {
			if (std::ranges::any_of(fs::directory_iterator{ installed.parent_path() }, [](const auto& aEntry) { return aEntry.path().filename().string().starts_with(".bpr-update-") && fs::exists(aEntry.path() / "active.json"); })) {
				return false;
			}
			auto observer = adapters::update::release_update_installation::join(installed);
			if (observer) {
				result = (*observer)->last_result();
			}
			return result.has_value();
		},
			  "native helper reaches durable terminal", 150000);
		const bool expected = std::string_view{ aArguments[7] } == "installed";
		expect(result->Installed == expected && desktop->observe_installation_result(result->Installed), "native result reaches public projection");
		std::cout << "native-public-result:" << (result->Installed ? "installed" : "recovered") << ":" << result->Transaction << '\n';
		desktop->shutdown();
		until([&] { return desktop->quiescent(); }, "native public service drains");
		desktop.reset();
		wire.reset();
		staging.reset();
		cleanup.close_and_drain();
	}
#endif
}
auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QApplication::setApplicationVersion(QStringLiteral("2.3.3"));
#if defined(__APPLE__)
	if (aArgc == 5 && std::string_view{ aArgv[1] } == "--native-state") {
		native_state(aArgv);
		return 0;
	}
	if (aArgc == 8 && std::string_view{ aArgv[1] } == "--native-handoff") {
		native_handoff(aArgv);
		return 0;
	}
#endif
	root_wiring();
	gestures_restart_and_staging();
	public_release_access_persists();
	unavailable_installations();
	std::cout << "release-update-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
