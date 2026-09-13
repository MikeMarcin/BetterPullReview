#include "bpr/logging/quill_log.hpp"
#include "bpr/application/epoch_catalog_port.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/core/github_model.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
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
			constexpr std::array names{ "none", "invalid_context", "invalid_limits", "missing_identity", "identity_mismatch", "missing_required_text", "invalid_text", "text_too_long", "invalid_repository", "invalid_pull_request_number", "invalid_commit_oid", "invalid_line", "invalid_enum", "invalid_capability_revision", "invalid_rate_limit", "collection_too_large", "duplicate_identity", "invalid_relationship", "allocation_limit", "total_text_too_large" };
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
			std::atomic<status_type>   Status{ status_type::unavailable };
			std::atomic<std::uint64_t> Dropped{ 0 };
			void                       fail(const status_type aStatus) noexcept
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
		// Walk every component without following links; the final file is owner-only,
		// single-link, regular, and exclusively locked for this process lifetime.
		auto open_log(const std::filesystem::path& aRoot) -> std::FILE*
		{
			if (!aRoot.is_absolute() || aRoot.lexically_normal() != aRoot) {
				return nullptr;
			}
			owned_fd directory{ ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC) };
			if (directory.Value < 0) {
				return nullptr;
			}
			for (const auto& component : aRoot.relative_path() / "logs") {
				if (component == "." || component == "..") {
					return nullptr;
				}
				if (::mkdirat(directory.Value, component.c_str(), 0700) != 0 && errno != EEXIST) {
					return nullptr;
				}
				const auto next = ::openat(directory.Value, component.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
				if (next < 0) {
					return nullptr;
				}
				::close(directory.Value);
				directory.Value = next;
			}
			struct stat directoryStat{ };
			if (::fstat(directory.Value, &directoryStat) != 0 || directoryStat.st_uid != ::geteuid() || (directoryStat.st_mode & 0077U) != 0) {
				return nullptr;
			}
			owned_fd    file{ ::openat(directory.Value, "diagnostics.log", O_WRONLY | O_APPEND | O_CREAT | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600) };
			struct stat fileStat{ };
			if (file.Value < 0 || ::fstat(file.Value, &fileStat) != 0 || !S_ISREG(fileStat.st_mode) || fileStat.st_uid != ::geteuid() || fileStat.st_nlink != 1 || (fileStat.st_mode & 0077U) != 0 || ::flock(file.Value, LOCK_EX | LOCK_NB) != 0) {
				return nullptr;
			}
			auto* stream = ::fdopen(file.Value, "a");
			if (stream != nullptr) {
				file.Value = -1;
			}
			return stream;
		}
#endif
		class capped_sink final : public quill::Sink
		{
		public:
			capped_sink(const std::filesystem::path& aRoot, const std::uint64_t aMaximumBytes, std::shared_ptr<sink_state> aState)
				: State{ std::move(aState) }
				, MaximumBytes{ aMaximumBytes }
			{
#if !defined(_WIN32)
				File = open_log(aRoot);
#else
				static_cast<void>(aRoot);
#endif
				if (File == nullptr) {
					return;
				}
				if (std::fseek(File, 0, SEEK_END) != 0) {
					return;
				}
				const auto size = std::ftell(File);
				if (size < 0) {
					return;
				}
				Bytes         = static_cast<std::uint64_t>(size);
				State->Status = status_type::available;
				if (Bytes >= MaximumBytes) {
					State->fail(status_type::full);
				}
			}
			~capped_sink() override
			{
				if (File != nullptr) {
					std::fclose(File);
				}
			}

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
				++State->Dropped;
				return status_type::dropped;
			}
			if (Closed) {
				return status_type::closed;
			}
			if (status() != status_type::available || Logger == nullptr) {
				return status();
			}
			const auto                                        operation = static_cast<std::size_t>(aEvent.Operation);
			const auto                                        outcome   = static_cast<std::size_t>(aEvent.Outcome);
			const auto                                        paged     = static_cast<std::size_t>(aEvent.PagedOperation);
			const auto                                        stage     = static_cast<std::size_t>(aEvent.EpochStage);
			static constexpr std::array<std::string_view, 10> stages{ "none", "catalog_open", "catalog_recovery", "catalog_prepare", "cache_open", "pack_sink", "smart_acquire", "protocol_acquire", "cache_inventory", "revision_prepare" };
			static constexpr std::array<std::string_view, 6>  domains{ "none", "catalog", "cache", "transport", "protocol", "epoch" };
			const auto                                        summary = static_cast<std::size_t>(aEvent.Summary.State);
			if (stage >= stages.size() || aEvent.EpochFailure.valueless_by_exception() || aEvent.GithubFailure.valueless_by_exception() || operation >= operations.size() || outcome >= outcomes.size() || paged >= paged_operations.size() || summary >= summaries.size() || aEvent.Summary.Size > aEvent.Summary.Text.size()
				|| std::ranges::any_of(aEvent.Summary.text(), [](const unsigned char aCharacter) { return aCharacter < 32U || aCharacter == 127U; })) {
				++State->Dropped;
				return status_type::dropped;
			}
			const auto thread = std::this_thread::get_id();
			if (std::find(Threads.begin(), Threads.begin() + static_cast<std::ptrdiff_t>(ThreadCount), thread) == Threads.begin() + static_cast<std::ptrdiff_t>(ThreadCount)) {
				if (ThreadCount == Threads.size()) {
					++State->Dropped;
					return status_type::dropped;
				}
				Threads[ThreadCount++] = thread;
			}
			try {
				static constexpr quill::MacroMetadata metadata{ "", "", "v=1 op={} outcome={} request={} http_status={} summary={} dropped={} detail={} epoch_stage={} failure_domain={} failure={} pack_failure={} page_operation={} window_session={}", nullptr, quill::LogLevel::Info, quill::MacroMetadata::Event::Log };
				const auto                            githubFailure = aEvent.GithubFailure.index() != 0U;
				const auto                            failureDomain = githubFailure ? (aEvent.GithubFailure.index() == 1U ? "github" : "validation") : domains[aEvent.EpochFailure.index()];
				const auto                            failure       = githubFailure
					? std::visit([](auto aFailure) { return failure_name(aFailure); }, aEvent.GithubFailure)
					: std::visit([](auto aFailure) { return failure_name(aFailure); }, aEvent.EpochFailure);
				if (!Logger->log_statement<false>(&metadata, operations[operation], outcomes[outcome], aEvent.Request, aEvent.HttpStatus, summaries[summary], State->Dropped.load(), aEvent.Summary.text(), stages[stage], failureDomain, failure, aEvent.EpochPackFailure ? failure_name(*aEvent.EpochPackFailure) : std::string_view{ "none" }, paged_operations[paged], aEvent.WindowSession)) {
					++State->Dropped;
					return status_type::dropped;
				}
				return status_type::available;
			} catch (...) {
				++State->Dropped;
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
	void quill_log::close() noexcept
	{
		if (Implementation) {
			Implementation->close();
		}
	}
}
