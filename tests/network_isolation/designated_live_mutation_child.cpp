#include "designated_live_mutation_child.hpp"
#include "designated_live_mutation_canary.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#if !defined(_WIN32)
	#include <csignal>
	#include <fcntl.h>
	#include <sys/wait.h>
	#include <unistd.h>
	#if defined(__APPLE__)
		#include <crt_externs.h>
		#include <libproc.h>
		#include <sys/proc_info.h>
	#elif defined(__linux__)
		#include <dirent.h>
	#endif
#endif

#if !defined(BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256)
	#error "The designated live child requires its configured canary surface digest"
#endif

namespace bpr::tests::network_isolation {
	namespace {

		constexpr std::string_view CanarySurfaceSha256 = BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256;

#if !defined(_WIN32)
		[[nodiscard]] bool write_all(int aDescriptor, std::string_view aBytes)
		{
			std::size_t offset{ };
			while (offset < aBytes.size()) {
				const auto count = write(aDescriptor, aBytes.data() + offset, aBytes.size() - offset);
				if (count < 0) {
					if (errno == EINTR) {
						continue;
					}
					return false;
				}
				offset += static_cast<std::size_t>(count);
			}
			return true;
		}

		void reap_after_kill(pid_t aChild)
		{
			static_cast<void>(kill(aChild, SIGKILL));
			while (waitpid(aChild, nullptr, 0) < 0 && errno == EINTR) {
			}
		}

		[[nodiscard]] bool clear_parent_environment()
		{
	#if defined(__APPLE__)
			char*** environment = _NSGetEnviron();
			if (environment == nullptr) {
				return false;
			}
			*environment = nullptr;
			return true;
	#else
			return clearenv() == 0;
	#endif
		}

		[[nodiscard]] std::optional<unsigned> process_thread_count()
		{
	#if defined(__APPLE__)
			proc_taskinfo taskInfo{ };
			const int     size = proc_pidinfo(
				getpid(),
				PROC_PIDTASKINFO,
				0,
				&taskInfo,
				static_cast<int>(sizeof(taskInfo)));
			if (size != static_cast<int>(sizeof(taskInfo)) || taskInfo.pti_threadnum < 1) {
				return std::nullopt;
			}
			return static_cast<unsigned>(taskInfo.pti_threadnum);
	#elif defined(__linux__)
			DIR* directory = opendir("/proc/self/task");
			if (directory == nullptr) {
				return std::nullopt;
			}
			unsigned threadCount{ };
			errno = 0;
			while (const dirent* entry = readdir(directory)) {
				if (entry->d_name[0] != '.') {
					++threadCount;
				}
			}
			const bool readSucceeded = errno == 0;
			if (closedir(directory) != 0 || !readSucceeded || threadCount == 0) {
				return std::nullopt;
			}
			return threadCount;
	#else
			return std::nullopt;
	#endif
		}

	#if defined(__linux__)
		[[nodiscard]] std::optional<int> decimal_descriptor(const char* aName)
		{
			if (aName == nullptr || *aName == '\0') {
				return std::nullopt;
			}
			int value{ };
			for (const char* cursor = aName; *cursor != '\0'; ++cursor) {
				if (*cursor < '0' || *cursor > '9') {
					return std::nullopt;
				}
				const int digit = *cursor - '0';
				if (value > (std::numeric_limits<int>::max() - digit) / 10) {
					return std::nullopt;
				}
				value = (value * 10) + digit;
			}
			return value;
		}
	#endif

		[[nodiscard]] std::optional<std::vector<int>> open_descriptor_inventory()
		{
			std::vector<int> descriptors;
	#if defined(__APPLE__)
			const int requiredBytes = proc_pidinfo(getpid(), PROC_PIDLISTFDS, 0, nullptr, 0);
			if (requiredBytes <= 0) {
				return std::nullopt;
			}
			constexpr int slackEntries  = 16;
			const int     capacityBytes = requiredBytes
				+ (slackEntries * static_cast<int>(sizeof(proc_fdinfo)));
			std::vector<proc_fdinfo> entries(
				static_cast<std::size_t>(capacityBytes) / sizeof(proc_fdinfo));
			const int usedBytes = proc_pidinfo(
				getpid(),
				PROC_PIDLISTFDS,
				0,
				entries.data(),
				capacityBytes);
			if (usedBytes < 0 || usedBytes >= capacityBytes
				|| usedBytes % static_cast<int>(sizeof(proc_fdinfo)) != 0) {
				return std::nullopt;
			}
			const auto usedEntries = static_cast<std::size_t>(usedBytes) / sizeof(proc_fdinfo);
			descriptors.reserve(usedEntries);
			for (std::size_t index = 0; index < usedEntries; ++index) {
				if (entries[index].proc_fd < 0) {
					return std::nullopt;
				}
				descriptors.push_back(entries[index].proc_fd);
			}
	#elif defined(__linux__)
			DIR* directory = opendir("/proc/self/fd");
			if (directory == nullptr) {
				return std::nullopt;
			}
			const int inventoryDescriptor = dirfd(directory);
			if (inventoryDescriptor < 0) {
				closedir(directory);
				return std::nullopt;
			}
			errno = 0;
			bool invalidEntry{ };
			while (const dirent* entry = readdir(directory)) {
				if (entry->d_name[0] == '.') {
					continue;
				}
				const auto descriptor = decimal_descriptor(entry->d_name);
				if (!descriptor.has_value()) {
					invalidEntry = true;
					break;
				}
				if (*descriptor != inventoryDescriptor) {
					descriptors.push_back(*descriptor);
				}
			}
			const bool readSucceeded = errno == 0;
			if (closedir(directory) != 0 || !readSucceeded || invalidEntry) {
				return std::nullopt;
			}
	#else
			return std::nullopt;
	#endif
			std::ranges::sort(descriptors);
			if (std::ranges::adjacent_find(descriptors) != descriptors.end()) {
				return std::nullopt;
			}
			for (const int descriptor : descriptors) {
				if (fcntl(descriptor, F_GETFD) < 0) {
					return std::nullopt;
				}
			}
			return descriptors;
		}

		enum class inventory_status {
			ready,
			thread_state_failed,
			multithreaded,
			read_failed,
			unstable,
		};

		struct stable_inventory_result
		{
			inventory_status Status{ inventory_status::read_failed };
			std::vector<int> Descriptors;
			unsigned         ObservedThreadCount{ };
			std::size_t      FirstDescriptorCount{ };
			std::size_t      SecondDescriptorCount{ };
		};

		[[nodiscard]] stable_inventory_result stable_descriptor_inventory(
			int aPipeReadDescriptor,
			int aPipeWriteDescriptor)
		{
			const auto firstThreadCount = process_thread_count();
			if (!firstThreadCount.has_value()) {
				return { .Status = inventory_status::thread_state_failed };
			}
			if (*firstThreadCount != 1) {
				return {
					.Status              = inventory_status::multithreaded,
					.ObservedThreadCount = *firstThreadCount,
				};
			}
			const auto first  = open_descriptor_inventory();
			const auto second = open_descriptor_inventory();
			if (!first.has_value() || !second.has_value()) {
				return {
					.Status                = inventory_status::read_failed,
					.ObservedThreadCount   = *firstThreadCount,
					.FirstDescriptorCount  = first.has_value() ? first->size() : 0,
					.SecondDescriptorCount = second.has_value() ? second->size() : 0,
				};
			}
			const auto secondThreadCount = process_thread_count();
			if (!secondThreadCount.has_value()) {
				return {
					.Status                = inventory_status::thread_state_failed,
					.ObservedThreadCount   = *firstThreadCount,
					.FirstDescriptorCount  = first->size(),
					.SecondDescriptorCount = second->size(),
				};
			}
			if (*secondThreadCount != 1) {
				return {
					.Status                = inventory_status::multithreaded,
					.ObservedThreadCount   = *secondThreadCount,
					.FirstDescriptorCount  = first->size(),
					.SecondDescriptorCount = second->size(),
				};
			}
			if (*first != *second
				|| !std::ranges::binary_search(*second, aPipeReadDescriptor)
				|| !std::ranges::binary_search(*second, aPipeWriteDescriptor)) {
				return {
					.Status                = inventory_status::unstable,
					.ObservedThreadCount   = *secondThreadCount,
					.FirstDescriptorCount  = first->size(),
					.SecondDescriptorCount = second->size(),
				};
			}
			return {
				.Status                = inventory_status::ready,
				.Descriptors           = std::move(*second),
				.ObservedThreadCount   = *secondThreadCount,
				.FirstDescriptorCount  = first->size(),
				.SecondDescriptorCount = second->size(),
			};
		}
#endif

	} // namespace

	std::string_view designated_live_canary_surface_sha256()
	{
		return CanarySurfaceSha256;
	}

	child_start_result start_fixed_canary(
		std::string_view            aCapability,
		std::string_view            aExpectedCanarySurfaceSha256,
		const fixed_canary_options& aOptions)
	{
#if defined(_WIN32)
		static_cast<void>(aCapability);
		static_cast<void>(aExpectedCanarySurfaceSha256);
		static_cast<void>(aOptions);
		return { };
#else
		if (aExpectedCanarySurfaceSha256 != CanarySurfaceSha256 || aOptions.TimeoutMilliseconds <= 0) {
			return { };
		}
		if (aOptions.Hook != nullptr) {
			aOptions.Hook->after_surface_binding();
		}
		if (!clear_parent_environment()) {
			return { };
		}
		int capabilityPipe[2]{ -1, -1 };
		if (pipe(capabilityPipe) != 0) {
			return { };
		}
		if (capabilityPipe[0] <= STDERR_FILENO || capabilityPipe[1] <= STDERR_FILENO) {
			close(capabilityPipe[0]);
			close(capabilityPipe[1]);
			return { };
		}
		auto inventory = stable_descriptor_inventory(capabilityPipe[0], capabilityPipe[1]);
		if (inventory.Status != inventory_status::ready) {
			close(capabilityPipe[0]);
			close(capabilityPipe[1]);
			child_run_status status = child_run_status::descriptor_inventory_failed;
			if (inventory.Status == inventory_status::thread_state_failed) {
				status = child_run_status::parent_thread_state_failed;
			} else if (inventory.Status == inventory_status::multithreaded) {
				status = child_run_status::parent_multithreaded;
			} else if (inventory.Status == inventory_status::unstable) {
				status = child_run_status::descriptor_inventory_unstable;
			}
			return {
				.Status                = status,
				.ObservedThreadCount   = inventory.ObservedThreadCount,
				.FirstDescriptorCount  = inventory.FirstDescriptorCount,
				.SecondDescriptorCount = inventory.SecondDescriptorCount,
			};
		}
		const int* descriptorData      = inventory.Descriptors.data();
		const auto descriptorCount     = inventory.Descriptors.size();
		const auto behavior            = aOptions.Behavior;
		const int  forbiddenDescriptor = aOptions.ForbiddenDescriptorForTest;
		const auto child               = fork();
		if (child < 0) {
			close(capabilityPipe[0]);
			close(capabilityPipe[1]);
			return { };
		}
		if (child == 0) {
			close(capabilityPipe[1]);
			if (capabilityPipe[0] != STDIN_FILENO && dup2(capabilityPipe[0], STDIN_FILENO) < 0) {
				_exit(120);
			}
			for (std::size_t index = 0; index < descriptorCount; ++index) {
				const int descriptor = descriptorData[index];
				if (descriptor > STDERR_FILENO) {
					close(descriptor);
				}
			}
			if (forbiddenDescriptor >= 0) {
				errno = 0;
				if (fcntl(forbiddenDescriptor, F_GETFD) != -1 || errno != EBADF) {
					_exit(123);
				}
			}
			if (behavior == fixed_canary_behavior::timeout_after_protocol) {
				char buffer[128];
				while (read(STDIN_FILENO, buffer, sizeof(buffer)) > 0) {
				}
				_exit(121);
			}
			const int canaryResult = run_designated_live_mutation_canary(STDIN_FILENO, STDOUT_FILENO, true);
			if (behavior == fixed_canary_behavior::exit_failure_after_protocol) {
				_exit(122);
			}
			_exit(canaryResult);
		}

		close(capabilityPipe[0]);
		const bool written = write_all(capabilityPipe[1], aCapability);
		if (behavior != fixed_canary_behavior::timeout_after_protocol) {
			close(capabilityPipe[1]);
			capabilityPipe[1] = -1;
		}
		if (!written) {
			if (capabilityPipe[1] >= 0) {
				close(capabilityPipe[1]);
			}
			reap_after_kill(child);
			return { .Status = child_run_status::failed };
		}

		const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(aOptions.TimeoutMilliseconds);
		for (;;) {
			int        status{ };
			const auto waited = waitpid(child, &status, WNOHANG);
			if (waited == child) {
				if (capabilityPipe[1] >= 0) {
					close(capabilityPipe[1]);
				}
				return { .Status = WIFEXITED(status) && WEXITSTATUS(status) == 0 ? child_run_status::succeeded : child_run_status::failed };
			}
			if (waited < 0 && errno != EINTR) {
				if (capabilityPipe[1] >= 0) {
					close(capabilityPipe[1]);
				}
				reap_after_kill(child);
				return { .Status = child_run_status::failed };
			}
			if (std::chrono::steady_clock::now() >= deadline) {
				if (capabilityPipe[1] >= 0) {
					close(capabilityPipe[1]);
				}
				reap_after_kill(child);
				return { .Status = child_run_status::timed_out };
			}
			usleep(1'000);
		}
#endif
	}

} // namespace bpr::tests::network_isolation
