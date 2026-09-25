#include "../../src/adapters/update/src/release_update_transaction.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/update/release_update_installation.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {
	namespace fs = std::filesystem;
	using namespace bpr;
	using reason      = core::release_update_reason;
	using boundary    = adapters::update::detail::install_boundary;
	using transaction = adapters::update::detail::install_transaction;
	using lease       = adapters::update::release_update_installation;
	using bpr::tests::expect;
	void write(const fs::path& aPath, const std::string& aBytes)
	{
		fs::create_directories(aPath.parent_path());
		std::ofstream file{ aPath, std::ios::binary | std::ios::trunc };
		file.write(aBytes.data(), static_cast<std::streamsize>(aBytes.size()));
		expect(file.good(), "fixture written");
	}
	auto read(const fs::path& aPath) -> std::string
	{
		std::ifstream file{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{ } };
	}
	struct effects final : adapters::update::detail::install_effects
	{
		unsigned Launches     = 0;
		bool     Refuse       = false;
		bool     LaunchResult = true;
		auto     validate(const fs::path& aBundle, const fs::path&, const core::release_update_candidate&) -> bool override
		{
			return !Refuse && read(aBundle / "Contents/MacOS/BetterPullReview") == "new-executable";
		}
		auto launch(const fs::path& aExecutable, std::string_view) -> bool override
		{
			++Launches;
			expect(read(aExecutable) == "new-executable", "exact replacement executable launched");
			return LaunchResult;
		}
	};
	struct fixture
	{
		QTemporaryDir                  Temporary;
		fs::path                       Root      = fs::path{ QFileInfo{ Temporary.path() }.canonicalFilePath().toStdString() };
		fs::path                       Installed = Root / "BetterPullReview.app", Staged = Root / "staged/BetterPullReview.app";
		core::release_update_candidate Candidate{ core::release_update_release_id{ 1 }, core::release_update_asset_id{ 2 }, { 1, 2, 3 }, "v1.2.3", "BetterPullReview-1.2.3-macos-universal.zip", 100, { } };
		effects                        Effects;
		fixture()
		{
			expect(Temporary.isValid(), "disposable installation root");
			write(Installed / "Contents/MacOS/BetterPullReview", "old-executable");
			write(Staged / "Contents/MacOS/BetterPullReview", "new-executable");
			write(Root / "private-state/draft", "retained draft and unresolved publication");
		}
		void old_intact()
		{
			expect(read(Installed / "Contents/MacOS/BetterPullReview") == "old-executable", "old application recovered");
			expect(read(Root / "private-state/draft") == "retained draft and unresolved publication", "state outside bundle untouched");
		}
	};
	void cooperative_and_receipt()
	{
		fixture course;
		auto    first = lease::join(course.Installed), second = lease::join(course.Installed);
		expect(first && second, "two installation owners admitted");
		transaction update{ course.Installed, course.Effects };
		expect(update.prepare(course.Staged, course.Candidate) == reason::none, "prepare captures exact participants");
		const auto id = update.identity();
		expect(!lease::join(course.Installed), "pending transaction closes new-process admission");
		expect((*first)->shutdown_requested() == id && (*second)->shutdown_requested() == id, "all owners observe same request");
		expect(update.replace() == reason::busy && course.Effects.Launches == 0, "live owners prevent replacement");
		expect((*first)->acknowledge_shutdown(id, true) && (*second)->acknowledge_shutdown(id, true), "durable shutdown recorded");
		first->reset();
		expect(update.replace() == reason::busy, "one remaining owner blocks replacement");
		second->reset();
		expect(update.replace() == reason::none && course.Effects.Launches == 1, "replace only after both owners exit");
		expect(update.finish() == reason::recovery_required && fs::exists(update.transaction_path() / "backup.app"), "launch is not healthy startup");
		expect(!lease::join(course.Installed, "00000000-0000-0000-0000-000000000000"), "foreign transaction cannot open stores");
		expect(!lease::join(course.Installed, id, "1.2.2"), "old executable version cannot open new state");
		auto replacement = lease::join(course.Installed, id, "1.2.3");
		expect(replacement.has_value(), "exact replacement admitted");
		expect(!(*replacement)->healthy_start("1.2.4"), "wrong version refuses receipt");
		expect(update.recover() == reason::busy, "no rollback while replacement uses state");
		expect((*replacement)->healthy_start("1.2.3"), "ready replacement writes exact receipt");
		expect(update.finish() == reason::none && !fs::exists(update.transaction_path() / "backup.app"), "matching receipt permits backup cleanup");
		expect(fs::exists(update.transaction_path() / "result.json"), "bounded durable result retained");
		expect(read(course.Root / "private-state/draft") == "retained draft and unresolved publication", "successful swap retains private state");
	}
	void interrupted_boundaries()
	{
		for (const auto stop : { boundary::prepared, boundary::before_old_rename, boundary::after_old_rename,
								 boundary::before_new_rename, boundary::after_new_rename, boundary::before_launch, boundary::after_launch }) {
			fixture course;
			{
				transaction update{ course.Installed, course.Effects, [stop](boundary aPoint) { return stop != aPoint; } };
				const auto  prepared = update.prepare(course.Staged, course.Candidate);
				if (prepared == reason::none) {
					static_cast<void>(update.replace());
				}
			}
			transaction restart{ course.Installed, course.Effects };
			expect(restart.recover() == reason::none, "restart reconciles every journal/rename/launch boundary");
			course.old_intact();
			expect(course.Effects.Launches <= 1U, "recovery never retries launch");
		}
	}
	void retained_startup_root()
	{
		fixture    course;
		const auto root = course.Root / "private-state";
		fs::permissions(root, fs::perms::owner_all);
		fs::create_directory_symlink(root, course.Root / "state-alias");
		transaction update{ course.Installed, course.Effects };
		expect(update.prepare(course.Staged, course.Candidate, [] { return true; }, course.Root / "state-alias") == reason::flush_failed, "aliased state root cannot authorize relaunch");
		expect(update.prepare(course.Staged, course.Candidate, [] { return true; }, root) == reason::none && update.replace() == reason::none, "canonical private state root bound before handoff");
		fs::rename(root, course.Root / "retained-state");
		fs::create_directory(root);
		fs::permissions(root, fs::perms::owner_all);
		expect(!lease::join(course.Installed, update.identity(), "1.2.3"), "replaced state directory refuses startup before stores open");
		fs::remove(root);
		fs::rename(course.Root / "retained-state", root);
		auto replacement = lease::join(course.Installed, update.identity(), "1.2.3");
		expect(replacement && (*replacement)->startup_root() == root, "startup receives exact recorded root without environment inheritance");
		expect((*replacement)->healthy_start("1.2.3") && update.finish() == reason::none, "retained stores permit healthy receipt and cleanup");
		expect(read(root / "draft") == "retained draft and unresolved publication", "retained root data survives exact restart");
	}
	void refusal_and_failed_launch()
	{
		fixture     changed;
		transaction mutation{ changed.Installed, changed.Effects };
		expect(mutation.prepare(changed.Staged, changed.Candidate) == reason::none, "changed candidate prepared");
		write(mutation.transaction_path() / "candidate.app/Contents/MacOS/BetterPullReview", "changed");
		expect(mutation.replace() == reason::changed_candidate, "immediate exact-tree validation refuses mutation");
		changed.old_intact();

		fixture failed;
		failed.Effects.LaunchResult = false;
		transaction noLaunch{ failed.Installed, failed.Effects };
		expect(noLaunch.prepare(failed.Staged, failed.Candidate) == reason::none && noLaunch.replace() == reason::recovery_required, "failed relaunch retains recovery");
		expect(noLaunch.recover() == reason::none, "failed relaunch restores old without retry");
		failed.old_intact();

		fixture     flush;
		auto        owner = lease::join(flush.Installed);
		transaction refused{ flush.Installed, flush.Effects };
		expect(owner && refused.prepare(flush.Staged, flush.Candidate) == reason::none, "flush refusal prepared");
		expect((*owner)->acknowledge_shutdown(refused.identity(), false), "failed flush recorded");
		owner->reset();
		expect(refused.replace() == reason::busy && flush.Effects.Launches == 0, "failed flush cannot authorize replacement after exit");
		expect(refused.recover() == reason::none, "failed flush abandons only the prepared transaction");
		flush.old_intact();

		fixture     crashed;
		auto        unclean = lease::join(crashed.Installed);
		transaction abandoned{ crashed.Installed, crashed.Effects };
		expect(unclean && abandoned.prepare(crashed.Staged, crashed.Candidate) == reason::none, "unclean owner prepared");
		unclean->reset();
		expect(abandoned.replace() == reason::busy && crashed.Effects.Launches == 0, "process disappearance is not durable shutdown");
		crashed.old_intact();
	}
	void completed_boundaries()
	{
		for (const auto stop : { boundary::healthy, boundary::before_cleanup, boundary::cleanup_entry, boundary::complete }) {
			fixture course;
			{
				transaction update{ course.Installed, course.Effects, [stop](boundary aPoint) { return stop != aPoint; } };
				expect(update.prepare(course.Staged, course.Candidate) == reason::none && update.replace() == reason::none, "cleanup fault setup");
				auto replacement = lease::join(course.Installed, update.identity(), "1.2.3");
				expect(replacement && (*replacement)->healthy_start("1.2.3"), "exact ready receipt before cleanup interruption");
				expect(update.finish() == reason::recovery_required, "cleanup interruption retained");
			}
			transaction restart{ course.Installed, course.Effects };
			expect(restart.recover() == reason::none && read(course.Installed / "Contents/MacOS/BetterPullReview") == "new-executable", "matching receipt resumes cleanup without rollback or relaunch");
			expect(course.Effects.Launches == 1U, "cleanup never launches another process");
		}
		for (const auto stop : { boundary::before_rollback, boundary::recovered }) {
			fixture course;
			{
				transaction update{ course.Installed, course.Effects, [stop](boundary aPoint) { return stop != aPoint; } };
				expect(update.prepare(course.Staged, course.Candidate) == reason::none && update.replace() == reason::none, "rollback interruption setup");
				expect(update.recover() == reason::recovery_required, "rollback interruption retained");
			}
			transaction restart{ course.Installed, course.Effects };
			expect(restart.recover() == reason::none, "rollback resumes after its journal boundaries");
			course.old_intact();
		}
	}
	void cleanup_preserves_foreign_entries()
	{
		fixture  course;
		fs::path foreign;
		{
			transaction update{ course.Installed, course.Effects, [&](boundary aPoint) {
								   if (aPoint == boundary::cleanup_entry) {
									   write(foreign, "not a transaction-owned file");
									   return false;
								   }
								   return true;
							   } };
			expect(update.prepare(course.Staged, course.Candidate) == reason::none && update.replace() == reason::none, "partial cleanup setup");
			foreign          = update.transaction_path() / "backup.app/foreign.txt";
			auto replacement = lease::join(course.Installed, update.identity(), "1.2.3");
			expect(replacement && (*replacement)->healthy_start("1.2.3"), "partial cleanup has matching receipt");
			expect(update.finish() == reason::recovery_required, "partial cleanup leaves durable entry inventory");
		}
		transaction restarted{ course.Installed, course.Effects };
		expect(restarted.recover() == reason::recovery_required && read(foreign) == "not a transaction-owned file", "retry never recursively removes an unrecorded file");
		expect(read(course.Installed / "Contents/MacOS/BetterPullReview") == "new-executable", "cleanup refusal cannot roll back a healthy installation");
	}
	void separate_process_quiescence()
	{
		for (const bool acknowledge : { false, true }) {
			fixture  course;
			QProcess peer;
			peer.start(QCoreApplication::applicationFilePath(), { "--peer", QString::fromStdString(course.Installed.string()) });
			expect(peer.waitForStarted(5000) && peer.waitForReadyRead(5000) && peer.readAllStandardOutput().contains("joined"), "separate process holds installation lifetime");
			transaction update{ course.Installed, course.Effects };
			expect(update.prepare(course.Staged, course.Candidate) == reason::none && update.replace() == reason::busy, "kernel-held peer lease blocks replacement");
			peer.write(acknowledge ? "ack\n" : "unclean\n");
			peer.closeWriteChannel();
			expect(peer.waitForFinished(5000) && peer.exitCode() == 0, "peer exits cooperatively without signals");
			expect(update.replace() == (acknowledge ? reason::none : reason::busy), "process exit alone is not durable acknowledgement");
			expect(update.recover() == reason::none, "disposable peer course restores old bundle");
			course.old_intact();
		}
		fixture     revoked;
		transaction update{ revoked.Installed, revoked.Effects };
		expect(update.prepare(revoked.Staged, revoked.Candidate, [] { return false; }) == reason::access_required, "revocation before journal publication refuses install");
		revoked.old_intact();

		fixture  lifetime;
		QProcess peer;
		peer.start(QCoreApplication::applicationFilePath(), { "--peer", QString::fromStdString(lifetime.Installed.string()) });
		expect(peer.waitForStarted(5000) && peer.waitForReadyRead(5000) && peer.readAllStandardOutput().contains("joined"), "native-lifetime peer joined");
		transaction held{ lifetime.Installed, lifetime.Effects };
		expect(held.prepare(lifetime.Staged, lifetime.Candidate) == reason::none, "native-lifetime transaction prepared");
		peer.write("held-exit\n");
		expect(peer.waitForReadyRead(5000) && peer.readAllStandardOutput().contains("drained"), "peer destroyed desktop ownership before actual exit");
		expect(held.replace() == reason::busy, "native locks survive desktop destruction until kernel process exit");
		peer.write("exit\n");
		peer.closeWriteChannel();
		expect(peer.waitForFinished(5000) && peer.exitCode() == 0, "native-lifetime peer exits cooperatively");
		expect(held.replace() == reason::none && held.recover() == reason::none, "replacement is admitted only after actual process exit");
		lifetime.old_intact();
	}
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
	auto native_signed_recovery(const fs::path& aRoot, const fs::path& aArchive) -> int
	{
		const auto* permitted = qualification_target_root();
		expect(permitted && fs::canonical(aRoot) == fs::path{ permitted } && aArchive.is_absolute(), "qualification guard names the exact disposable root");
		const auto installed = aRoot / "installed/BetterPullReview.app", staged = aRoot / "staged/BetterPullReview.app";
		expect(!fs::exists(installed) && !fs::exists(staged), "native course never overwrites an existing installation");
		for (const auto& destination : { installed.parent_path(), staged.parent_path() }) {
			fs::create_directory(destination);
			QProcess unpack;
			unpack.start(QStringLiteral("/usr/bin/ditto"), { "-x", "-k", QString::fromStdString(aArchive.string()), QString::fromStdString(destination.string()) });
			expect(unpack.waitForStarted(5000) && unpack.waitForFinished(60000) && unpack.exitCode() == 0, "genuine signed archive expands inside disposable root");
		}
		struct native_effects final : adapters::update::detail::install_effects
		{
			unsigned Launches = 0;
			auto     validate(const fs::path& aBundle, const fs::path& aTrust, const core::release_update_candidate& aCandidate) -> bool override
			{
				return adapters::update::detail::validate_install_tree(aBundle, aTrust, aCandidate, { }) == reason::none;
			}
			auto launch(const fs::path&, std::string_view) -> bool override
			{
				++Launches;
				return false;
			}
		} native;
		const core::release_update_candidate candidate{ core::release_update_release_id{ 1 }, core::release_update_asset_id{ 2 }, { 0, 1, 3 }, "v0.1.3", "BetterPullReview-0.1.3-macos-universal.zip", fs::file_size(aArchive), { } };
		const auto                           original = adapters::update::detail::install_tree_identity(installed);
		expect(original.has_value(), "genuine signed installed tree bound");
		expect(!adapters::update::detail::trusted_install_helper(installed, installed / "Contents/MacOS/bpr_update_helper"), "old release without bundled helper cannot enable installation");
		{
			transaction update{ installed, native, [](boundary aPoint) { return aPoint != boundary::after_old_rename; } };
			expect(update.prepare(staged, candidate) == reason::none, "native trust validates target-volume signed copy");
			expect(update.replace() == reason::recovery_required && fs::exists(update.transaction_path() / "backup.app"), "native rename interruption retains signed old application");
		}
		transaction restarted{ installed, native };
		expect(restarted.recover() == reason::none && adapters::update::detail::install_tree_identity(installed) == original, "journal restart restores exact signed original");
		expect(native.validate(installed, installed, candidate) && native.Launches == 0U, "recovered copy remains natively trusted; no application was launched");
		std::cout << "native-signed-recovery:pass; upgrade-and-new-helper-receipt:not-qualified\n";
		return 0;
	}
	auto native_prepare(char** aArguments) -> int
	{
		const fs::path root{ aArguments[2] }, archive{ aArguments[3] }, stateRoot{ aArguments[5] };
		const auto*    permitted = qualification_target_root();
		expect(permitted && fs::canonical(root) == fs::path{ permitted }
				   && fs::canonical(stateRoot) == stateRoot && stateRoot.string().starts_with(root.string() + "/"),
			   "native fault course requires disposable scope");
		struct native_effects final : adapters::update::detail::install_effects
		{
			auto validate(const fs::path& aBundle, const fs::path& aTrust, const core::release_update_candidate& aCandidate) -> bool override
			{
				return adapters::update::detail::validate_install_tree(aBundle, aTrust, aCandidate, { }) == reason::none;
			}
			auto launch(const fs::path&, std::string_view) -> bool override { return false; }
		} native;
		const std::string version{ aArguments[4] };
		const auto        parsed = core::parse_release_update_version(version);
		expect(parsed.has_value(), "native fault candidate version");
		const core::release_update_candidate candidate{ core::release_update_release_id{ 350U }, core::release_update_asset_id{ 351U }, *parsed, "v" + version, "BetterPullReview-" + version + "-macos-universal.zip", fs::file_size(archive), { } };
		transaction                          update{ root / "installed/BetterPullReview.app", native, [](boundary aPoint) { return aPoint != boundary::after_old_rename; } };
		expect(update.prepare(root / "staged/BetterPullReview.app", candidate, [] { return true; }, stateRoot) == reason::none, "native fault preparation validates exact target");
		if (std::string_view{ aArguments[6] } == "after-old-rename") {
			expect(update.replace() == reason::recovery_required && fs::exists(update.transaction_path() / "backup.app"), "native interrupted rename retains old signed helper");
		}
		std::cout << update.identity() << '\n';
		return 0;
	}
}
auto main(int argc, char** argv) -> int
try {
	QCoreApplication application{ argc, argv };
	if (argc == 7 && std::string_view{ argv[1] } == "--native-prepare") {
		return native_prepare(argv);
	}
	if (argc == 4 && std::string_view{ argv[1] } == "--native-signed-recovery") {
		return native_signed_recovery(fs::path{ argv[2] }, fs::path{ argv[3] });
	}
	if (argc == 3 && std::string_view{ argv[1] } == "--peer") {
		auto owner = lease::join(fs::path{ argv[2] });
		if (!owner) {
			return 1;
		}
		std::cout << "joined\n"
				  << std::flush;
		std::string command;
		std::cin >> command;
		if (command == "unclean") {
			std::_Exit(0);
		}
		const auto request = (*owner)->shutdown_requested();
		if (command == "held-exit" && request && (*owner)->acknowledge_shutdown(*request, true)) {
			(*owner)->keep_until_process_exit();
			owner->reset();
			std::cout << "drained\n"
					  << std::flush;
			std::cin >> command;
			return command == "exit" ? 0 : 1;
		}
		return command == "ack" && request && (*owner)->acknowledge_shutdown(*request, true) ? 0 : 1;
	}
	cooperative_and_receipt();
	interrupted_boundaries();
	retained_startup_root();
	refusal_and_failed_launch();
	completed_boundaries();
	cleanup_preserves_foreign_entries();
	separate_process_quiescence();
	std::cout << "release-update-transaction:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
