#include "bpr/adapters/update/release_update_installation.hpp"
#include "release_update_transaction.hpp"

#include <QCoreApplication>

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>
#include <thread>

namespace bpr::adapters::update {
	namespace {
		namespace fs = std::filesystem;
		using reason = core::release_update_reason;
		auto launch(const fs::path& aExecutable, std::vector<std::string> aArguments, pid_t* aChild = nullptr) -> bool
		{
			auto               name = aExecutable.string();
			std::vector<char*> arguments{ name.data() };
			for (auto& argument : aArguments) {
				arguments.push_back(argument.data());
			}
			arguments.push_back(nullptr);
			// No credential, loader override, profile command or release-supplied
			// argument is inherited. The helper and new app use their installed rpaths.
			std::array<char*, 1>       environment{ nullptr };
			posix_spawn_file_actions_t actions;
			if (posix_spawn_file_actions_init(&actions) != 0) {
				return false;
			}
			posix_spawnattr_t attributes;
			if (posix_spawnattr_init(&attributes) != 0) {
				posix_spawn_file_actions_destroy(&actions);
				return false;
			}
			const auto attributesReady = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_CLOEXEC_DEFAULT) == 0;
			const auto files           = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0) == 0
				&& posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0) == 0
				&& posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0) == 0;
			pid_t      child  = 0;
			const auto result = files && attributesReady ? posix_spawn(&child, name.c_str(), &actions, &attributes, arguments.data(), environment.data()) : -1;
			posix_spawn_file_actions_destroy(&actions);
			posix_spawnattr_destroy(&attributes);
			if (result == 0 && aChild) {
				*aChild = child;
			}
			// Child lifetime is the explicitly transferred application/helper lifetime.
			// No wait or PID lookup grants replacement authority; file leases do.
			return result == 0;
		}
		struct native_effects final : detail::install_effects
		{
			auto validate(const fs::path& aBundle, const fs::path& aTrustSource, const core::release_update_candidate& aCandidate) -> bool override
			{
				return detail::validate_install_tree(aBundle, aTrustSource, aCandidate, { }) == reason::none;
			}
			auto launch(const fs::path& aExecutable, std::string_view aTransaction) -> bool override
			{
				return update::launch(aExecutable, { "--bpr-update-transaction", std::string{ aTransaction } });
			}
		};
		class native_installer final : public application::release_update_install_port
		{
			struct child_registry
			{
				std::mutex         Mutex;
				std::vector<pid_t> Children;
			};

		public:
			native_installer(fs::path aInstalled, fs::path aApplicationRoot)
				: Installed{ std::move(aInstalled) }
				, ApplicationRoot{ std::move(aApplicationRoot) }
			{
				Children->Children.reserve(4U);
				Reaper = std::jthread{ [children = Children](std::stop_token aStop) {
					while (!aStop.stop_requested()) {
						{
							const std::scoped_lock lock{ children->Mutex };
							std::erase_if(children->Children, [](pid_t aChild) {
								const auto result = waitpid(aChild, nullptr, WNOHANG);
								return result == aChild || (result < 0 && errno == ECHILD);
							});
						}
						std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
					}
				} };
			}
			auto install(std::shared_ptr<const application::release_update_verified_bundle> aBundle, std::stop_token aStop, const std::function<bool()>& aAuthorized) -> reason override
			{
				if (ApplicationRoot.empty()) {
					return reason::flush_failed;
				}
				const auto             helper = Installed / "Contents/MacOS/bpr_update_helper";
				const std::unique_lock childLock{ Children->Mutex };
				if (Children->Children.size() >= 4U) {
					return reason::busy;
				}
				if (aStop.stop_requested()) {
					return reason::cancelled;
				}
				if (!aAuthorized || !aAuthorized()) {
					return reason::access_required;
				}
				if (!detail::trusted_install_helper(Installed, helper)) {
					return reason::installer_unavailable;
				}
				const auto material = detail::install_source(aBundle, aStop);
				if (!material) {
					return reason::changed_candidate;
				}
				native_effects effects;
				std::string    id;
				{
					detail::install_transaction transaction{ Installed, effects };
					const auto                  prepared = transaction.prepare(material->Path, aBundle->candidate(), aAuthorized, ApplicationRoot);
					if (prepared != reason::none) {
						return prepared;
					}
					id = transaction.identity();
					if (aStop.stop_requested() || !aAuthorized() || detail::install_tree_identity(material->Path) != std::optional{ material->Identity }) {
						static_cast<void>(transaction.abort_prepared());
						if (aStop.stop_requested()) {
							return reason::cancelled;
						}
						if (!aAuthorized()) {
							return reason::access_required;
						}
						return reason::changed_candidate;
					}
				}
				pid_t child = 0;
				if (!launch(helper, { "--install", Installed.string(), id }, &child)) {
					detail::install_transaction transaction{ Installed, effects };
					if (transaction.resume(id)) {
						static_cast<void>(transaction.abort_prepared());
					}
					return reason::installer_unavailable;
				}
				// A refused quit may leave this app alive. Reap only our helper;
				// stopping the monitor never terminates a helper or application.
				Children->Children.push_back(child);
				return reason::none;
			}

		private:
			fs::path                        Installed;
			fs::path                        ApplicationRoot;
			std::shared_ptr<child_registry> Children = std::make_shared<child_registry>();
			std::jthread                    Reaper;
		};
	}
	auto make_release_update_installer(fs::path aInstalled, fs::path aApplicationRoot) -> std::shared_ptr<application::release_update_install_port>
	{
		return std::make_shared<native_installer>(std::move(aInstalled), std::move(aApplicationRoot));
	}
	auto run_release_update_helper(const fs::path& aInstalled, std::string_view aTransaction, bool aRecover) -> int
	{
		const auto executable = fs::path{ QCoreApplication::applicationFilePath().toStdString() };
		const auto source     = executable.parent_path().parent_path().parent_path();
		if (!detail::trusted_install_helper(source, executable)) {
			return 1;
		}
		native_effects              effects;
		detail::install_transaction transaction{ aInstalled, effects };
		if (!transaction.resume(aTransaction)
			|| (source != aInstalled && source != transaction.transaction_path() / "backup.app")) {
			return 1;
		}
		if (aRecover) {
			return transaction.recover() == reason::none ? 0 : 1;
		}
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 30 };
		auto       outcome  = reason::busy;
		while (outcome == reason::busy && std::chrono::steady_clock::now() < deadline) {
			outcome = transaction.replace();
			if (outcome == reason::busy) {
				std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
			}
		}
		if (outcome == reason::busy) {
			static_cast<void>(transaction.abort_prepared());
			return 1;
		}
		if (outcome != reason::none) {
			static_cast<void>(transaction.recover());
			return 1;
		}
		const auto startupDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 60 };
		while (std::chrono::steady_clock::now() < startupDeadline) {
			if (transaction.finish() == reason::none) {
				return 0;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
		}
		// A live replacement retains its backup and cannot be rolled back. A
		// dead replacement permits recovery; neither path terminates an app.
		static_cast<void>(transaction.recover());
		return 1;
	}
}
