#include "bpr/logging/quill_log.hpp"
#include "../../native/windows_private_storage.hpp"
#include "bpr/application/epoch_catalog_port.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/core/github_model.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <expected>
#include <limits>
#include <mutex>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/Logger.h>
#include <quill/sinks/Sink.h>
#include <thread>
#if !defined(_WIN32)
	#include <fcntl.h>
	#include <sys/file.h>
	#include <sys/stat.h>
	#include <unistd.h>
#else
	#include <fcntl.h>
	#include <io.h>
#endif
namespace bpr::adapters::logging {
	namespace {
		auto failure_name(std::monostate) noexcept -> std::string_view { return "none"; }
		auto failure_name(application::epoch_catalog_failure aFailure) noexcept -> std::string_view
		{
			switch (aFailure) {
			case application::epoch_catalog_failure::invalid_request:             return "invalid_request";
			case application::epoch_catalog_failure::invalid_path:                return "invalid_path";
			case application::epoch_catalog_failure::invalid_permissions:         return "invalid_permissions";
			case application::epoch_catalog_failure::unavailable:                 return "unavailable";
			case application::epoch_catalog_failure::schema_mismatch:             return "schema_mismatch";
			case application::epoch_catalog_failure::integrity_failure:           return "integrity_failure";
			case application::epoch_catalog_failure::authority_mismatch:          return "authority_mismatch";
			case application::epoch_catalog_failure::catalog_mismatch:            return "catalog_mismatch";
			case application::epoch_catalog_failure::revision_conflict:           return "revision_conflict";
			case application::epoch_catalog_failure::operation_identity_conflict: return "operation_identity_conflict";
			case application::epoch_catalog_failure::source_observation_conflict: return "source_observation_conflict";
			case application::epoch_catalog_failure::operation_conflict:          return "operation_conflict";
			case application::epoch_catalog_failure::phase_conflict:              return "phase_conflict";
			case application::epoch_catalog_failure::quota_exceeded:              return "quota_exceeded";
			case application::epoch_catalog_failure::ordinal_exhausted:           return "ordinal_exhausted";
			case application::epoch_catalog_failure::busy:                        return "busy";
			case application::epoch_catalog_failure::repair_required:             return "repair_required";
			case application::epoch_catalog_failure::cancelled:                   return "cancelled";
			case application::epoch_catalog_failure::platform_unsupported:        return "platform_unsupported";
			}
			return "unknown";
		}
		auto failure_name(application::git_cache_failure aFailure) noexcept -> std::string_view
		{
			switch (aFailure) {
			case application::git_cache_failure::invalid_request:        return "invalid_request";
			case application::git_cache_failure::invalid_authority:      return "invalid_authority";
			case application::git_cache_failure::invalid_path:           return "invalid_path";
			case application::git_cache_failure::invalid_manifest:       return "invalid_manifest";
			case application::git_cache_failure::invalid_repository:     return "invalid_repository";
			case application::git_cache_failure::lock_unavailable:       return "lock_unavailable";
			case application::git_cache_failure::quota_exceeded:         return "quota_exceeded";
			case application::git_cache_failure::spool_failure:          return "spool_failure";
			case application::git_cache_failure::inventory_mismatch:     return "inventory_mismatch";
			case application::git_cache_failure::authorization_mismatch: return "authorization_mismatch";
			case application::git_cache_failure::process_failure:        return "process_failure";
			case application::git_cache_failure::object_mismatch:        return "object_mismatch";
			case application::git_cache_failure::ref_conflict:           return "ref_conflict";
			case application::git_cache_failure::repair_required:        return "repair_required";
			case application::git_cache_failure::cancelled:              return "cancelled";
			case application::git_cache_failure::unsupported_object:     return "unsupported_object";
			}
			return "unknown";
		}
		auto failure_name(application::git_smart_transport_failure aFailure) noexcept -> std::string_view
		{
			switch (aFailure) {
			case application::git_smart_transport_failure::invalid_request:    return "invalid_request";
			case application::git_smart_transport_failure::unavailable:        return "unavailable";
			case application::git_smart_transport_failure::authentication:     return "authentication";
			case application::git_smart_transport_failure::permission:         return "permission";
			case application::git_smart_transport_failure::network:            return "network";
			case application::git_smart_transport_failure::certificate:        return "certificate";
			case application::git_smart_transport_failure::proxy:              return "proxy";
			case application::git_smart_transport_failure::redirect:           return "redirect";
			case application::git_smart_transport_failure::timeout:            return "timeout";
			case application::git_smart_transport_failure::cancelled:          return "cancelled";
			case application::git_smart_transport_failure::request_limit:      return "request_limit";
			case application::git_smart_transport_failure::response_limit:     return "response_limit";
			case application::git_smart_transport_failure::malformed_response: return "malformed_response";
			case application::git_smart_transport_failure::consumer_rejected:  return "consumer_rejected";
			}
			return "unknown";
		}
		auto failure_name(application::git_protocol_v2_failure aFailure) noexcept -> std::string_view
		{
			switch (aFailure) {
			case application::git_protocol_v2_failure::invalid_request:        return "invalid_request";
			case application::git_protocol_v2_failure::request_limit:          return "request_limit";
			case application::git_protocol_v2_failure::response_limit:         return "response_limit";
			case application::git_protocol_v2_failure::malformed_packet:       return "malformed_packet";
			case application::git_protocol_v2_failure::unsupported_version:    return "unsupported_version";
			case application::git_protocol_v2_failure::missing_capability:     return "missing_capability";
			case application::git_protocol_v2_failure::object_format_mismatch: return "object_format_mismatch";
			case application::git_protocol_v2_failure::unexpected_ref:         return "unexpected_ref";
			case application::git_protocol_v2_failure::duplicate_ref:          return "duplicate_ref";
			case application::git_protocol_v2_failure::oid_mismatch:           return "oid_mismatch";
			case application::git_protocol_v2_failure::unexpected_section:     return "unexpected_section";
			case application::git_protocol_v2_failure::sideband_error:         return "sideband_error";
			case application::git_protocol_v2_failure::progress_limit:         return "progress_limit";
			case application::git_protocol_v2_failure::pack_limit:             return "pack_limit";
			case application::git_protocol_v2_failure::incomplete_response:    return "incomplete_response";
			case application::git_protocol_v2_failure::pack_sink:              return "pack_sink";
			case application::git_protocol_v2_failure::allocation:             return "allocation";
			}
			return "unknown";
		}
		auto failure_name(application::git_pack_stream_failure aFailure) noexcept -> std::string_view
		{
			switch (aFailure) {
			case application::git_pack_stream_failure::invalid_request: return "invalid_request";
			case application::git_pack_stream_failure::invalid_state:   return "invalid_state";
			case application::git_pack_stream_failure::input_limit:     return "input_limit";
			case application::git_pack_stream_failure::storage:         return "storage";
			case application::git_pack_stream_failure::malformed_pack:  return "malformed_pack";
			case application::git_pack_stream_failure::cancelled:       return "cancelled";
			}
			return "unknown";
		}
		auto failure_name(core::epoch_failure aFailure) noexcept -> std::string_view
		{
			switch (aFailure) {
			case core::epoch_failure::unavailable:           return "unavailable";
			case core::epoch_failure::malformed_observation: return "malformed_observation";
			case core::epoch_failure::catalog_conflict:      return "catalog_conflict";
			case core::epoch_failure::quota_exceeded:        return "quota_exceeded";
			case core::epoch_failure::ordinal_exhausted:     return "ordinal_exhausted";
			case core::epoch_failure::repair_required:       return "repair_required";
			case core::epoch_failure::process_failure:       return "process_failure";
			case core::epoch_failure::unsupported_object:    return "unsupported_object";
			}
			return "unknown";
		}
		auto failure_name(core::github_failure aFailure) noexcept -> std::string_view
		{
			constexpr std::array names{ "authentication", "permission", "network", "api_version", "capability", "rate_limit", "malformed_response", "response_too_large", "vault", "certificate", "redirect", "timeout", "cli_unavailable" };
			const auto           index = static_cast<std::size_t>(aFailure);
			return index < names.size() ? names[index] : "unknown";
		}
		auto failure_name(application::github_read_validation_error aFailure) noexcept -> std::string_view
		{
			constexpr std::array names{ "none", "invalid_context", "invalid_limits", "missing_identity", "identity_mismatch", "missing_required_text", "invalid_text", "text_too_long", "invalid_repository", "invalid_pull_request_number", "invalid_commit_oid", "invalid_line", "invalid_enum", "invalid_capability_revision", "invalid_rate_limit", "collection_too_large", "duplicate_identity", "invalid_relationship", "allocation_limit", "total_text_too_large", "relationship_file_previous_path", "relationship_thread_comment_path", "relationship_review_group_root", "relationship_review_group_reply", "relationship_actor_login", "relationship_thread_reply", "relationship_search_repository", "relationship_search_target_identity", "relationship_search_target_metadata", "relationship_timeline_availability", "relationship_timeline_event", "relationship_review_comments_availability", "relationship_review_threads_capability", "relationship_file_viewed_capability", "relationship_thread_permission_capability", "relationship_search_availability" };
			const auto           index = static_cast<std::size_t>(aFailure);
			return index < names.size() ? names[index] : "unknown";
		}

		using status_type = application::diagnostic_log_status;
		struct backend_status
		{
			std::atomic<bool> Failed{ false };
		};
		struct backend_lifetime
		{
			std::mutex                      Mutex;
			std::size_t                     Users      = 0;
			std::uint64_t                   LastLogger = 0;
			std::shared_ptr<backend_status> Status;
		};
		auto shared_backend() -> backend_lifetime&
		{
			static backend_lifetime lifetime;
			return lifetime;
		}
		// Quill has one process backend. Its lifetime is shared, while each
		// admitted log retains a separate sink and backend logger identity.
		class backend_lease final
		{
		public:
			backend_lease()
			{
				auto&                 lifetime = shared_backend();
				const std::lock_guard lock{ lifetime.Mutex };
				if (lifetime.Users == 64U || lifetime.LastLogger == std::numeric_limits<std::uint64_t>::max()) {
					return;
				}
				auto name = std::string{ "bpr.diagnostics." } + std::to_string(++lifetime.LastLogger);
				if (lifetime.Users == 0U) {
					auto                  status = std::make_shared<backend_status>();
					quill::BackendOptions options;
					options.transit_event_buffer_initial_capacity = 32;
					options.transit_events_soft_limit             = 128;
					options.transit_events_hard_limit             = 128;
					// [[BPR-1051]]. Quill's singleton check detects Quill compiled
					// into two binary modules of one process. Here Quill is compiled
					// into this one translation unit of a static library, and the
					// project builds no shared or module library, so the check
					// cannot fire. On Linux and macOS its only effect was
					// `/tmp/QuillBackendLock<pid>`, which only a clean backend stop
					// removes, so every process that was killed or crashed left one.
					options.check_backend_singleton_instance = false;
					// Backend failures cannot be attributed to one sink. File/cap
					// failures remain in capped_sink's own status instead.
					options.error_notifier = [status](const std::string& aMessage) {
						// Quill also reports ordinary dropping/backpressure and
						// nonfatal thread setup warnings through this callback.
						if (aMessage.find(" Quill INFO: ") == std::string::npos
							&& aMessage.find(" Quill WARNING: ") == std::string::npos) {
							status->Failed.store(true);
						}
					};
					quill::Backend::start(options);
					lifetime.Status = std::move(status);
				}
				Status = lifetime.Status;
				Name   = std::move(name);
				++lifetime.Users;
				Active = true;
			}
			~backend_lease() { release(); }
			backend_lease(const backend_lease&)                    = delete;
			auto operator=(const backend_lease&) -> backend_lease& = delete;
			void release() noexcept
			{
				if (!Active) {
					return;
				}
				try {
					auto&                 lifetime = shared_backend();
					const std::lock_guard lock{ lifetime.Mutex };
					Active = false;
					if (--lifetime.Users == 0U) {
						quill::Backend::stop();
						lifetime.Status.reset();
					}
				} catch (...) {
					Status->Failed.store(true);
				}
			}
			std::shared_ptr<backend_status> Status;
			std::string                     Name;

		private:
			bool Active = false;
		};
		struct frontend_options : quill::FrontendOptions
		{
			static constexpr auto        queue_type             = quill::QueueType::BoundedDropping;
			static constexpr std::size_t initial_queue_capacity = 64U * 1024U;
		};
		using frontend = quill::FrontendImpl<frontend_options>;
		struct sink_state
		{
			std::atomic<status_type>             Status{ status_type::unavailable };
			std::atomic<diagnostic_file_failure> OpenFailure{ diagnostic_file_failure::none };
			std::atomic<std::uint64_t>           Dropped{ 0 };
			void                                 drop() noexcept
			{
				auto count = Dropped.load(std::memory_order_relaxed);
				while (count != std::numeric_limits<std::uint64_t>::max()
					   && !Dropped.compare_exchange_weak(count, count + 1U, std::memory_order_relaxed)) { }
			}

			void fail(const status_type aStatus) noexcept
			{
				Status.store(aStatus);
			}
		};
#if !defined(_WIN32)
		struct owned_fd
		{
			int Value = -1;
			~owned_fd()
			{
				if (Value >= 0) {
					::close(Value);
				}
			}
		};
		struct opened_log
		{
			std::FILE* File = nullptr;
			~opened_log()
			{
				if (File) {
					std::fclose(File);
				}
			}
		};
		auto open_log(const std::filesystem::path& aRoot) -> std::expected<std::unique_ptr<opened_log>, diagnostic_file_failure>
		{
			if (!aRoot.is_absolute() || aRoot.lexically_normal() != aRoot) {
				return std::unexpected(diagnostic_file_failure::invalid_root);
			}
			struct stat named{ };
			if (::lstat(aRoot.c_str(), &named) != 0 || !S_ISDIR(named.st_mode)) {
				return std::unexpected(diagnostic_file_failure::invalid_root);
			}
			// [[BPR-0885]]. The root is opened by the name it was given: its
			// ancestors resolve the way the stores' do, so a root under a
			// symlinked `/var` or `/tmp` opens, and O_NOFOLLOW refuses a root
			// that is itself a link. Nothing here can throw, so a root that
			// changes after the lstat is refused by its identity below and is
			// named, never an unnamed exception.
			owned_fd    root{ ::open(aRoot.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
			struct stat admitted{ };
			if (root.Value < 0 || ::fstat(root.Value, &admitted) != 0 || admitted.st_dev != named.st_dev || admitted.st_ino != named.st_ino) {
				return std::unexpected(diagnostic_file_failure::invalid_root);
			}
			if (admitted.st_uid != ::geteuid() || (admitted.st_mode & 0077U) != 0) {
				return std::unexpected(diagnostic_file_failure::invalid_permissions);
			}
			// The storage owner creates/adopts the root. Logging creates only its one child.
			if (::mkdirat(root.Value, "logs", 0700) != 0 && errno != EEXIST) {
				return std::unexpected(diagnostic_file_failure::io_failure);
			}
			owned_fd    directory{ ::openat(root.Value, "logs", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
			struct stat directoryStat{ };
			if (directory.Value < 0 || ::fstat(directory.Value, &directoryStat) != 0 || directoryStat.st_uid != ::geteuid() || (directoryStat.st_mode & 0077U) != 0) {
				return std::unexpected(diagnostic_file_failure::invalid_permissions);
			}
			owned_fd    file{ ::openat(directory.Value, "diagnostics.log", O_WRONLY | O_APPEND | O_CREAT | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600) };
			struct stat fileStat{ };
			if (file.Value < 0 || ::fstat(file.Value, &fileStat) != 0 || !S_ISREG(fileStat.st_mode) || fileStat.st_uid != ::geteuid() || fileStat.st_nlink != 1 || (fileStat.st_mode & 0077U) != 0) {
				return std::unexpected(diagnostic_file_failure::invalid_permissions);
			}
			if (::flock(file.Value, LOCK_EX | LOCK_NB) != 0) {
				return std::unexpected(diagnostic_file_failure::busy);
			}
			auto result  = std::make_unique<opened_log>();
			result->File = ::fdopen(file.Value, "a");
			if (!result->File) {
				return std::unexpected(diagnostic_file_failure::io_failure);
			}
			file.Value = -1;
			return result;
		}
#else
		namespace win = native_storage;
		struct opened_log
		{
			std::FILE*                               File = nullptr;
			std::vector<win::private_windows_object> Parents;
			~opened_log()
			{
				if (File) {
					std::fclose(File);
				}
			}
		};
		auto open_log(const std::filesystem::path& aRoot) -> std::expected<std::unique_ptr<opened_log>, diagnostic_file_failure>
		{
			const auto canonical = win::canonical_windows_root(aRoot);
			const auto security  = win::private_security::load();
			if (!canonical || !security) {
				return std::unexpected(diagnostic_file_failure::invalid_root);
			}
			auto ancestors = win::validate_ancestor_chain(*canonical);
			auto root      = win::open_private_directory(*canonical, *security, false);
			if (!ancestors || !root || !win::reliable_local_filesystem(*root)) {
				return std::unexpected(diagnostic_file_failure::invalid_permissions);
			}
			auto directory = win::ensure_private_child(*root, L"logs", *security);
			if (!directory) {
				return std::unexpected(diagnostic_file_failure::invalid_permissions);
			}
			win::private_security_attributes attributes;
			if (!attributes.initialize(*security, win::windows_object_kind::regular_file)) {
				return std::unexpected(diagnostic_file_failure::invalid_permissions);
			}
			const auto      path   = directory->Path / L"diagnostics.log";
			constexpr DWORD access = GENERIC_READ | GENERIC_WRITE | READ_CONTROL;
			// A second writer or a replace/delete opener cannot coexist with this handle.
			win::windows_handle file{ ::CreateFileW(path.c_str(), access, FILE_SHARE_READ, attributes.attributes(), CREATE_NEW,
													FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr) };
			if (!file.valid() && (::GetLastError() == ERROR_FILE_EXISTS || ::GetLastError() == ERROR_ALREADY_EXISTS)) {
				file = win::windows_handle{ ::CreateFileW(path.c_str(), access, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
														  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr) };
			}
			if (!file.valid()) {
				return std::unexpected(::GetLastError() == ERROR_SHARING_VIOLATION ? diagnostic_file_failure::busy : diagnostic_file_failure::io_failure);
			}
			if (!win::identity_of(file.Value, win::windows_object_kind::regular_file)
				|| !win::validate_private_security(file.Value, *security, win::windows_object_kind::regular_file)
				|| !win::path_matches_handle(file.Value, path)) {
				return std::unexpected(diagnostic_file_failure::invalid_permissions);
			}
			auto       result = std::make_unique<opened_log>();
			const auto fd     = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(file.Value), _O_APPEND | _O_BINARY | _O_NOINHERIT);
			if (fd < 0) {
				return std::unexpected(diagnostic_file_failure::io_failure);
			}
			file.Value   = INVALID_HANDLE_VALUE;
			result->File = ::_fdopen(fd, "ab");
			if (!result->File) {
				::_close(fd);
				return std::unexpected(diagnostic_file_failure::io_failure);
			}
			result->Parents = std::move(*ancestors);
			result->Parents.push_back(std::move(*root));
			result->Parents.push_back(std::move(*directory));
			return result;
		}
#endif
		class capped_sink final : public quill::Sink
		{
		public:
			capped_sink(const std::filesystem::path& aRoot, const std::uint64_t aMaximumBytes, std::shared_ptr<sink_state> aState)
				: State{ std::move(aState) }
				, MaximumBytes{ aMaximumBytes }
			{
				auto opened = open_log(aRoot);
				if (!opened) {
					State->OpenFailure = opened.error();
					return;
				}
				Native = std::move(*opened);
				File   = Native->File;
				if (std::fseek(File, 0, SEEK_END) != 0) {
					State->OpenFailure = diagnostic_file_failure::io_failure;
					return;
				}
				const auto size = std::ftell(File);
				if (size < 0) {
					State->OpenFailure = diagnostic_file_failure::io_failure;
					return;
				}
				Bytes         = static_cast<std::uint64_t>(size);
				State->Status = status_type::available;
				if (Bytes >= MaximumBytes) {
					State->fail(status_type::full);
				}
			}
			~capped_sink() override = default;

		protected:
			void write_log(const quill::MacroMetadata*, std::uint64_t, std::string_view, std::string_view, const std::string&, std::string_view, quill::LogLevel, std::string_view, std::string_view, const std::vector<std::pair<std::string, std::string>>*, std::string_view, const std::string_view aStatement) override
			{
				if (State->Status != status_type::available) {
					return;
				}
				if (aStatement.size() > MaximumBytes - Bytes) {
					State->fail(status_type::full);
					return;
				}
				if (std::fwrite(aStatement.data(), 1, aStatement.size(), File) != aStatement.size()) {
					State->fail(status_type::unavailable);
					return;
				}
				Bytes += aStatement.size();
			}
			void flush_sink() override
			{
				if (File != nullptr && std::fflush(File) != 0) {
					State->fail(status_type::unavailable);
				}
			}

		private:
			std::shared_ptr<sink_state> State;
			std::FILE*                  File = nullptr;
			std::unique_ptr<opened_log> Native;
			std::uint64_t               MaximumBytes;
			std::uint64_t               Bytes = 0;
		};
		constexpr std::array<std::string_view, 5>  operations{ "application", "github.viewer", "github.capability", "github.pull_request", "epoch" };
		constexpr std::array<std::string_view, 13> paged_operations{ "none", "GetViewerIdentity", "GetRepository", "GetPullRequest", "ListPullRequestFiles", "ListPullRequestCommits", "ListReviews", "ListReviewThreads", "ListIssueComments", "ListChecks", "SearchPullRequests", "ListReviewComments", "ListPullRequestTimeline" };
		constexpr std::array<std::string_view, 6>  outcomes{ "started", "succeeded", "failed", "cancelled", "response_received", "shutdown" };
		constexpr std::array<std::string_view, 5>  summaries{ "absent", "sanitized", "omitted_invalid", "omitted_limit", "omitted_failure" };
	}
	class quill_log::implementation
	{
	public:
		explicit implementation(const std::filesystem::path& aRoot, const std::uint64_t aMaximumBytes)
			: State{ std::make_shared<sink_state>() }
		{
			try {
				Sink = std::make_shared<capped_sink>(aRoot, aMaximumBytes, State);
				if (State->Status != status_type::available) {
					return;
				}
				Backend       = std::make_unique<backend_lease>();
				BackendStatus = Backend->Status;
				if (!BackendStatus) {
					State->fail(status_type::unavailable);
					return;
				}
				Logger = frontend::create_or_get_logger(Backend->Name, Sink, quill::PatternFormatterOptions{ "%(time) %(message)", "%Y-%m-%dT%H:%M:%S.%QnsZ", quill::Timezone::GmtTime });
			} catch (...) {
				// [[BPR-0885]]. A throw while admitting the file still names a
				// cause. Once the sink exists the file was admitted, and a later
				// backend failure is reported through status() alone.
				if (!Sink && State->OpenFailure.load() == diagnostic_file_failure::none) {
					State->OpenFailure.store(diagnostic_file_failure::io_failure);
				}
				State->fail(status_type::unavailable);
			}
		}
		~implementation() { close(); }
		void close() noexcept
		{
			std::lock_guard lock{ Mutex };
			if (Closed) {
				return;
			}
			Closed = true;
			try {
				if (Logger != nullptr) {
					Logger->flush_log();
					frontend::remove_logger_blocking(Logger);
					Logger = nullptr;
				}
			} catch (...) {
				State->fail(status_type::unavailable);
			}
			Backend.reset();
			Sink.reset();
		}
		auto record(const application::diagnostic_event& aEvent) noexcept -> status_type
		{
			std::unique_lock lock{ Mutex, std::try_to_lock };
			if (!lock.owns_lock()) {
				State->drop();
				return status_type::dropped;
			}
			if (Closed) {
				return status_type::closed;
			}
			if (status() != status_type::available || Logger == nullptr) {
				return status();
			}
			const auto                                        operation       = static_cast<std::size_t>(aEvent.Operation);
			const auto                                        outcome         = static_cast<std::size_t>(aEvent.Outcome);
			const auto                                        paged           = static_cast<std::size_t>(aEvent.PagedOperation);
			const auto                                        transportSource = static_cast<std::size_t>(aEvent.TransportFailureSource);
			static constexpr std::array<std::string_view, 4>  transportSources{ "none", "native_transport", "http_response", "network_reply" };
			const auto                                        stage = static_cast<std::size_t>(aEvent.EpochStage);
			static constexpr std::array<std::string_view, 10> stages{ "none", "catalog_open", "catalog_recovery", "catalog_prepare", "cache_open", "pack_sink", "smart_acquire", "protocol_acquire", "cache_inventory", "revision_prepare" };
			static constexpr std::array<std::string_view, 6>  domains{ "none", "catalog", "cache", "transport", "protocol", "epoch" };
			const auto                                        summary = static_cast<std::size_t>(aEvent.Summary.State);
			if (transportSource >= transportSources.size() || (aEvent.HttpStatus != 0U && (aEvent.HttpStatus < 100U || aEvent.HttpStatus > 599U)) || stage >= stages.size() || aEvent.EpochFailure.valueless_by_exception() || aEvent.GithubFailure.valueless_by_exception() || operation >= operations.size() || outcome >= outcomes.size() || paged >= paged_operations.size() || summary >= summaries.size() || aEvent.Summary.Size > aEvent.Summary.Text.size()
				|| std::ranges::any_of(aEvent.Summary.text(), [](const unsigned char aCharacter) { return aCharacter < 32U || aCharacter == 127U; })) {
				State->drop();
				return status_type::dropped;
			}
			const auto thread = std::this_thread::get_id();
			if (std::find(Threads.begin(), Threads.begin() + static_cast<std::ptrdiff_t>(ThreadCount), thread) == Threads.begin() + static_cast<std::ptrdiff_t>(ThreadCount)) {
				if (ThreadCount == Threads.size()) {
					State->drop();
					return status_type::dropped;
				}
				Threads[ThreadCount++] = thread;
			}
			try {
				static constexpr quill::MacroMetadata metadata{ "", "", "v=1 op={} outcome={} request={} http_status={} summary={} dropped={} detail={} epoch_stage={} failure_domain={} failure={} pack_failure={} page_operation={} window_session={} relationship_detail={} transport_source={}", nullptr, quill::LogLevel::Info, quill::MacroMetadata::Event::Log };
				const auto                            githubFailure = aEvent.GithubFailure.index() != 0U;
				const auto                            failureDomain = githubFailure ? (aEvent.GithubFailure.index() == 1U ? "github" : "validation") : domains[aEvent.EpochFailure.index()];
				const auto                            failure       = githubFailure
					? std::visit([](auto aFailure) { return failure_name(aFailure); }, aEvent.GithubFailure)
					: std::visit([](auto aFailure) { return failure_name(aFailure); }, aEvent.EpochFailure);
				if (!Logger->log_statement<false>(&metadata, operations[operation], outcomes[outcome], aEvent.Request, aEvent.HttpStatus, summaries[summary], State->Dropped.load(), aEvent.Summary.text(), stages[stage], failureDomain, failure, aEvent.EpochPackFailure ? failure_name(*aEvent.EpochPackFailure) : std::string_view{ "none" }, paged_operations[paged], aEvent.WindowSession, aEvent.RelationshipDetail ? failure_name(*aEvent.RelationshipDetail) : std::string_view{ "none" }, transportSources[transportSource])) {
					State->drop();
					return status_type::dropped;
				}
				return status_type::available;
			} catch (...) {
				State->drop();
				return status_type::dropped;
			}
		}
		auto status() const noexcept -> status_type
		{
			const auto result = State->Status.load();
			return result == status_type::available && BackendStatus && BackendStatus->Failed.load()
				? status_type::unavailable
				: result;
		}
		std::shared_ptr<sink_state> State;

	private:
		std::mutex                      Mutex;
		std::array<std::thread::id, 16> Threads{ };
		std::size_t                     ThreadCount = 0;
		std::shared_ptr<capped_sink>    Sink;
		frontend::logger_t*             Logger = nullptr;
		std::unique_ptr<backend_lease>  Backend;
		// Retained after close so concurrent status reads never race pointer reset.
		std::shared_ptr<backend_status> BackendStatus;
		bool                            Closed = false;
	};
	quill_log::quill_log(const std::filesystem::path& aRoot, const std::uint64_t aMaximumBytes) noexcept
	{
		try {
			Implementation = std::make_unique<implementation>(aRoot, aMaximumBytes);
		} catch (...) { }
	}
	quill_log::~quill_log() = default;
	auto quill_log::record(const application::diagnostic_event& aEvent) noexcept -> status_type { return Implementation ? Implementation->record(aEvent) : status_type::unavailable; }
	auto quill_log::status() const noexcept -> status_type { return Implementation ? Implementation->status() : status_type::unavailable; }
	auto quill_log::open_failure() const noexcept -> diagnostic_file_failure { return Implementation ? Implementation->State->OpenFailure.load() : diagnostic_file_failure::io_failure; }
	void quill_log::close() noexcept
	{
		if (Implementation) {
			Implementation->close();
		}
	}
}
