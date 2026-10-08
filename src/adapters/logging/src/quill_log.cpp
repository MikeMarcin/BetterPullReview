#include "bpr/logging/quill_log.hpp"
#include "bpr/application/epoch_catalog_port.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/core/github_model.hpp"
#include "session_files.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <deque>
#include <functional>
#include <limits>
#include <mutex>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/Logger.h>
#include <quill/sinks/Sink.h>
#include <random>
#include <thread>
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
			case core::epoch_failure::git_tool_unsupported:  return "git_tool_unsupported";
			case core::epoch_failure::git_tool_probe_failed: return "git_tool_probe_failed";
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
		struct priority_frontend_options : frontend_options
		{ };
		using frontend          = quill::FrontendImpl<frontend_options>;
		using priority_frontend = quill::FrontendImpl<priority_frontend_options>;
		struct sink_state
		{
			std::atomic<status_type>             Status{ status_type::unavailable };
			std::atomic<diagnostic_file_failure> OpenFailure{ diagnostic_file_failure::none };
			std::atomic<std::uint64_t>           Dropped{ 0 };
			std::atomic<std::uint64_t>           Generation{ 1 };
			std::atomic<std::size_t>             PendingEvents{ 0 };
			std::atomic<std::size_t>             PendingBytes{ 0 };
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
		class capped_sink final : public quill::Sink
		{
		public:
			capped_sink(const std::filesystem::path& aRoot, const std::string& aSession, diagnostic_file_limits aLimits, std::shared_ptr<sink_state> aState, std::function<void()> aMaintainContext)
				: State{ std::move(aState) }
				, Files{ aRoot, aSession, aLimits }
				, MaintainContext{ std::move(aMaintainContext) }
			{
				State->OpenFailure = Files.failure();
				State->Status      = Files.failure() == diagnostic_file_failure::none ? status_type::available : status_type::unavailable;
			}

			auto clear() -> application::diagnostic_clear_result { return Files.clear(); }
			void stop_context_maintenance()
			{
				const std::lock_guard lock{ MaintenanceMutex };
				MaintainContext = { };
			}

		protected:
			void write_log(const quill::MacroMetadata*, std::uint64_t, std::string_view, std::string_view, const std::string&, std::string_view, quill::LogLevel, std::string_view, std::string_view, const std::vector<std::pair<std::string, std::string>>*, std::string_view, const std::string_view aStatement) override
			{
				State->PendingEvents.fetch_sub(1U);
				State->PendingBytes.fetch_sub(aStatement.size() - 1U);
				const auto    separator  = aStatement.find('|');
				std::uint64_t generation = 0U;
				if (separator == std::string_view::npos) {
					State->drop();
					return;
				}
				const auto prefix = aStatement.substr(0U, separator);
				const auto parsed = std::from_chars(prefix.data(), prefix.data() + prefix.size(), generation);
				if (parsed.ec != std::errc{ } || parsed.ptr != aStatement.data() + separator || generation != State->Generation.load()) {
					State->drop();
					return;
				}
				if (State->Status != status_type::available) {
					State->drop();
					return;
				}
				const auto result = Files.write(aStatement.substr(separator + 1U));
				if (result != status_type::available) {
					State->drop();
					if (result != status_type::dropped) {
						State->fail(result);
					}
				}
			}
			// Native writes reach the kernel before the directory lock is released.
			// There is no userspace file buffer and no crash-durability promise.
			void flush_sink() override { }
			void run_periodic_tasks() override
			{
				const auto now = std::chrono::steady_clock::now();
				if (now < NextMaintenance) {
					return;
				}
				NextMaintenance = now + std::chrono::milliseconds{ 100 };
				const std::lock_guard lock{ MaintenanceMutex };
				if (MaintainContext) {
					MaintainContext();
				}
			}

		private:
			std::shared_ptr<sink_state>           State;
			session_files                         Files;
			std::mutex                            MaintenanceMutex;
			std::function<void()>                 MaintainContext;
			std::chrono::steady_clock::time_point NextMaintenance = std::chrono::steady_clock::now() + std::chrono::milliseconds{ 100 };
		};
		// Only already-classified fields enter this fixed-capacity encoder. Escaping
		// protects JSON framing; it does not classify or sanitize provider content.
		class json_record
		{
		public:
			json_record() { append("{"); }
			void text(std::string_view aKey, std::string_view aValue)
			{
				key(aKey);
				append("\"");
				append_escaped(aValue);
				append("\"");
			}
			void number(std::string_view aKey, std::uint64_t aValue)
			{
				key(aKey);
				std::array<char, 20> number{ };
				const auto           converted = std::to_chars(number.data(), number.data() + number.size(), aValue);
				append({ number.data(), static_cast<std::size_t>(converted.ptr - number.data()) });
			}
			auto finish() -> std::string_view
			{
				append("}");
				return Valid ? std::string_view{ Bytes.data(), Size } : std::string_view{ };
			}

		private:
			void append_escaped(std::string_view aValue)
			{
				for (const unsigned char character : aValue) {
					if (character == '"' || character == '\\') {
						append("\\");
					}
					const char value = static_cast<char>(character);
					append({ &value, 1U });
				}
			}
			void key(std::string_view aKey)
			{
				append(Size == 1U ? "\"" : ",\"");
				append(aKey);
				append("\":");
			}
			void append(std::string_view aValue)
			{
				if (aValue.size() > Bytes.size() - Size) {
					Valid = false;
					return;
				}
				std::copy(aValue.begin(), aValue.end(), Bytes.begin() + static_cast<std::ptrdiff_t>(Size));
				Size += aValue.size();
			}
			// Includes space for Quill's single newline; nothing grows on overflow.
			std::array<char, 16U * 1024U - 1U> Bytes{ };
			std::size_t                        Size  = 0U;
			bool                               Valid = true;
		};
		auto session_identity() -> std::string
		{
			std::random_device random;
			std::string        identity;
			identity.reserve(32U);
			constexpr std::string_view hex = "0123456789abcdef";
			for (unsigned index = 0U; index < 4U; ++index) {
				const auto value = static_cast<std::uint32_t>(random());
				for (unsigned shift = 0U; shift < 32U; shift += 4U) {
					identity.push_back(hex[(value >> shift) & 15U]);
				}
			}
			return identity;
		}

		constexpr std::array<std::string_view, 6>  operations{ "application", "github.viewer", "github.capability", "github.pull_request", "epoch", "github.review_comment" };
		constexpr std::array<std::string_view, 13> paged_operations{ "none", "GetViewerIdentity", "GetRepository", "GetPullRequest", "ListPullRequestFiles", "ListPullRequestCommits", "ListReviews", "ListReviewThreads", "ListIssueComments", "ListChecks", "SearchPullRequests", "ListReviewComments", "ListPullRequestTimeline" };
		constexpr std::array<std::string_view, 6>  outcomes{ "started", "succeeded", "failed", "cancelled", "response_received", "shutdown" };
		constexpr std::array<std::string_view, 5>  summaries{ "absent", "sanitized", "omitted_invalid", "omitted_limit", "omitted_failure" };
	}
	class quill_log::implementation
	{
	public:
		explicit implementation(const std::filesystem::path& aRoot, const diagnostic_file_limits aLimits, application::diagnostic_preferences aPreferences)
			: State{ std::make_shared<sink_state>() }
		{
			try {
				Preferences = application::valid_diagnostic_preferences(aPreferences) ? aPreferences : application::diagnostic_preferences{ };
				Session     = session_identity();
				Sink        = std::make_shared<capped_sink>(aRoot, Session, aLimits, State, [this] {
					const std::unique_lock lock{ Mutex, std::try_to_lock };
					if (lock.owns_lock()) {
						refresh_context();
					}
				});
				if (State->Status != status_type::available) {
					return;
				}
				Backend       = std::make_unique<backend_lease>();
				BackendStatus = Backend->Status;
				if (!BackendStatus) {
					State->fail(status_type::unavailable);
					return;
				}
				Logger         = frontend::create_or_get_logger(Backend->Name, Sink, quill::PatternFormatterOptions{ "%(message)" });
				PriorityLogger = priority_frontend::create_or_get_logger(Backend->Name + ".priority", Sink, quill::PatternFormatterOptions{ "%(message)" });
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
			if (Sink) {
				// Retire the callback before the implementation can die, even if
				// Quill temporarily retains the sink in its backend cache.
				Sink->stop_context_maintenance();
			}
			Recent.clear();
			RecentBytes = 0U;
			Identities.clear();
			try {
				if (Logger != nullptr) {
					flush();
					priority_frontend::remove_logger_blocking(PriorityLogger);
					PriorityLogger = nullptr;
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
			refresh_context();
			if (aEvent.ContextGeneration != 0U && aEvent.ContextGeneration != ContextGeneration) {
				State->drop();
				return status_type::dropped;
			}
			if (!Preferences.Enabled) {
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
			static constexpr std::array<std::string_view, 16> gitSteps{ "none", "admission", "temporary_store", "process_runner", "identity_before", "cli_version", "git_version", "cli_qualification", "git_qualification", "clone", "object_check", "identity_after", "local_store", "local_pack", "pack_read", "git_capability_probe" };
			const auto                                        gitStep = static_cast<std::size_t>(aEvent.GitStep);
			const auto                                        summary = static_cast<std::size_t>(aEvent.Summary.State);
			if ((aEvent.Detail != application::diagnostic_detail::standard && aEvent.Detail != application::diagnostic_detail::verbose) || gitStep >= gitSteps.size() || transportSource >= transportSources.size() || (aEvent.HttpStatus != 0U && (aEvent.HttpStatus < 100U || aEvent.HttpStatus > 599U)) || stage >= stages.size() || aEvent.EpochFailure.valueless_by_exception() || aEvent.GithubFailure.valueless_by_exception() || operation >= operations.size() || outcome >= outcomes.size() || paged >= paged_operations.size() || summary >= summaries.size() || aEvent.Summary.Size > aEvent.Summary.Text.size() || (aEvent.Summary.Size != 0U && aEvent.Summary.State != application::diagnostic_summary_state::sanitized)
				|| std::ranges::any_of(aEvent.Summary.text(), [](const unsigned char aCharacter) { return aCharacter < 32U || aCharacter >= 127U; })) {
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

				static constexpr quill::MacroMetadata metadata{ "", "", "{}", nullptr, quill::LogLevel::Info, quill::MacroMetadata::Event::Log };
				const auto                            githubFailure = aEvent.GithubFailure.index() != 0U;
				const auto                            failureDomain = githubFailure ? (aEvent.GithubFailure.index() == 1U ? "github" : "validation") : domains[aEvent.EpochFailure.index()];
				const auto                            failure       = githubFailure
					? std::visit([](auto aFailure) { return failure_name(aFailure); }, aEvent.GithubFailure)
					: std::visit([](auto aFailure) { return failure_name(aFailure); }, aEvent.EpochFailure);
				const auto                            utc           = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				if (utc < 0 || Sequence == std::numeric_limits<std::uint64_t>::max()) {
					State->drop();
					return status_type::dropped;
				}
				json_record record;
				record.number("v", 3U);
				record.text("event", std::string{ operations[operation] } + "." + std::string{ outcomes[outcome] });
				record.text("category", operation == 0U ? "application" : operation == 4U ? "epoch"
																						  : "github");
				record.text("severity", aEvent.Outcome == application::diagnostic_outcome::failed ? "error" : aEvent.HttpStatus >= 400U ? "warning"
																																		: "info");
				record.number("utc_ms", static_cast<std::uint64_t>(utc));
				record.number("elapsed_us", static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - Started).count()));
				record.text("session", Session);
				record.number("sequence", ++Sequence);
				record.text("build", BPR_DIAGNOSTIC_BUILD);
				record.text("op", operations[operation]);
				record.text("outcome", outcomes[outcome]);
				record.number("request", Preferences.Privacy == application::diagnostic_privacy::include_locally ? aEvent.Request : 0U);
				record.number("http_status", aEvent.HttpStatus);
				record.text("summary", summaries[summary]);
				record.number("dropped", State->Dropped.load());
				record.text("detail", aEvent.Summary.text());
				record.text("epoch_stage", stages[stage]);
				record.text("failure_domain", failureDomain);
				record.text("failure", failure);
				record.text("pack_failure", aEvent.EpochPackFailure ? failure_name(*aEvent.EpochPackFailure) : std::string_view{ "none" });
				record.text("page_operation", paged_operations[paged]);
				record.number("window_session", Preferences.Privacy == application::diagnostic_privacy::include_locally ? aEvent.WindowSession : 0U);
				record.text("relationship_detail", aEvent.RelationshipDetail ? failure_name(*aEvent.RelationshipDetail) : std::string_view{ "none" });
				record.text("transport_source", transportSources[transportSource]);
				record.text("git_step", gitSteps[gitStep]);
				record.text("request_label", identity_label(aEvent.Request, false));
				record.text("window_label", identity_label(aEvent.WindowSession, true));
				record.text("capture_privacy", Preferences.Privacy == application::diagnostic_privacy::include_locally ? "include_locally" : "anonymized");
				const bool priority = aEvent.Outcome == application::diagnostic_outcome::failed || aEvent.HttpStatus >= 400U;
				const bool verbose  = !priority && (aEvent.Detail == application::diagnostic_detail::verbose || (aEvent.GitStep != application::git_smart_transport_step{ } && aEvent.Outcome == application::diagnostic_outcome::started));
				record.text("capture_detail", Preferences.Detail == application::diagnostic_detail::verbose ? "verbose" : "standard");
				record.text("event_detail", verbose ? "verbose" : "standard");
				const auto bytes = record.finish();
				if (bytes.empty()) {
					State->drop();
					return status_type::dropped;
				}
				if (verbose) {
					retain_recent(bytes);
				}
				if (verbose && Preferences.Detail == application::diagnostic_detail::standard) {
					return status_type::available;
				}
				// The generation prefix is transport-only and is stripped before file I/O.
				const auto transport  = std::to_string(ContextGeneration) + "|" + std::string{ bytes };
				const auto eventLimit = priority ? 4096U : 3584U;
				const auto byteLimit  = (priority ? 8U : 7U) * 1024U * 1024U;
				if (State->PendingEvents.load() >= eventLimit || State->PendingBytes.load() > byteLimit - transport.size()) {
					State->drop();
					return status_type::dropped;
				}
				State->PendingEvents.fetch_add(1U);
				State->PendingBytes.fetch_add(transport.size());
				const auto submitted = priority
					? PriorityLogger->log_statement<false>(&metadata, std::string_view{ transport })
					: Logger->log_statement<false>(&metadata, std::string_view{ transport });
				if (!submitted) {
					State->PendingEvents.fetch_sub(1U);
					State->PendingBytes.fetch_sub(transport.size());
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
		void discard_context() noexcept
		{
			State->Generation.fetch_add(1U);
			const std::unique_lock lock{ Mutex, std::try_to_lock };
			if (lock.owns_lock()) {
				refresh_context();
			}
		}
		auto configure(application::diagnostic_preferences aPreferences) noexcept -> bool
		{
			if (!application::valid_diagnostic_preferences(aPreferences)) {
				return false;
			}
			std::lock_guard lock{ Mutex };
			if (Closed) {
				return false;
			}
			try {
				State->Generation.fetch_add(1U);
				refresh_context();
				flush();
				Preferences = aPreferences;
				return true;
			} catch (...) {
				State->fail(status_type::unavailable);
				return false;
			}
		}
		auto clear() noexcept -> application::diagnostic_clear_result
		{
			using result = application::diagnostic_clear_result;
			std::lock_guard lock{ Mutex };
			if (Closed) {
				return result::unavailable;
			}
			try {
				State->Generation.fetch_add(1U);
				refresh_context();
				flush();
				const auto cleared = Sink ? Sink->clear() : result::unavailable;
				if (cleared == result::cleared) {
					State->Status = status_type::available;
				}
				if (cleared == result::partial) {
					State->fail(status_type::unavailable);
				}
				return cleared;
			} catch (...) {
				State->fail(status_type::unavailable);
				return result::unavailable;
			}
		}

		auto capture_state() -> application::diagnostic_capture_state
		{
			std::lock_guard lock{ Mutex };
			refresh_context();
			return { Preferences, status(), State->Dropped.load(), Recent.size(), RecentBytes };
		}
		auto recent_context() -> std::vector<std::string>
		{
			std::lock_guard lock{ Mutex };
			refresh_context();
			std::vector<std::string> result;
			result.reserve(Recent.size());
			for (const auto& item : Recent) {
				result.push_back(item.Bytes);
			}
			if (ContextGeneration != State->Generation.load()) {
				refresh_context();
				result.clear();
			}
			return result;
		}
		auto flush_for_export(bool aIncludeRecent) -> std::optional<application::diagnostic_export_capture>
		{
			std::lock_guard lock{ Mutex };
			refresh_context();
			flush();
			application::diagnostic_export_capture result{ Session, Sequence, { } };
			if (aIncludeRecent) {
				result.Recent.reserve(Recent.size());
				for (const auto& item : Recent) {
					result.Recent.push_back(item.Bytes);
				}
			}
			if (ContextGeneration != State->Generation.load()) {
				refresh_context();
				return std::nullopt;
			}
			return result;
		}

		std::shared_ptr<sink_state> State;

	private:
		void flush()
		{
			if (Logger) {
				Logger->flush_log();
			}
			if (PriorityLogger) {
				PriorityLogger->flush_log();
			}
		}
		void refresh_context()
		{
			const auto generation = State->Generation.load();
			if (ContextGeneration != generation) {
				Recent.clear();
				RecentBytes = 0;
				Identities.clear();
				ContextGeneration = generation;
			}
			const auto cutoff = std::chrono::steady_clock::now() - std::chrono::minutes{ 3 };
			while (!Recent.empty() && Recent.front().Time <= cutoff) {
				RecentBytes -= Recent.front().Bytes.size();
				Recent.pop_front();
			}
		}
		void retain_recent(std::string_view aBytes)
		{
			while (!Recent.empty() && (Recent.size() >= 512U || RecentBytes + aBytes.size() > 2U * 1024U * 1024U)) {
				RecentBytes -= Recent.front().Bytes.size();
				Recent.pop_front();
			}
			Recent.push_back({ std::chrono::steady_clock::now(), std::string{ aBytes } });
			RecentBytes += aBytes.size();
		}
		auto identity_label(std::uint64_t aValue, bool aWindow) -> std::string
		{
			if (aValue == 0U) {
				return "none";
			}
			const auto found = std::ranges::find_if(Identities, [&](const auto& aIdentity) { return aIdentity.Value == aValue && aIdentity.Window == aWindow; });
			if (found != Identities.end()) {
				return found->Label;
			}
			if (Identities.size() == 4096U) {
				return "omitted_limit";
			}
			auto label = std::string{ aWindow ? "window-" : "request-" } + Session + "-" + std::to_string(++NextIdentity);
			Identities.push_back({ aValue, aWindow, label });
			return label;
		}
		struct recent_record
		{
			std::chrono::steady_clock::time_point Time;
			std::string                           Bytes;
		};
		struct identity_record
		{
			std::uint64_t Value;
			bool          Window;
			std::string   Label;
		};
		application::diagnostic_preferences Preferences;
		std::deque<recent_record>           Recent;
		std::vector<identity_record>        Identities;
		std::size_t                         RecentBytes       = 0;
		std::uint64_t                       ContextGeneration = 1;
		std::uint64_t                       NextIdentity      = 0;
		priority_frontend::logger_t*        PriorityLogger    = nullptr;
		std::mutex                          Mutex;
		std::array<std::thread::id, 16>     Threads{ };
		std::size_t                         ThreadCount = 0;
		std::shared_ptr<capped_sink>        Sink;
		frontend::logger_t*                 Logger = nullptr;
		std::unique_ptr<backend_lease>      Backend;
		// Retained after close so concurrent status reads never race pointer reset.
		std::shared_ptr<backend_status>             BackendStatus;
		const std::chrono::steady_clock::time_point Started = std::chrono::steady_clock::now();
		std::string                                 Session;
		std::uint64_t                               Sequence = 0U;
		bool                                        Closed   = false;
	};
	quill_log::quill_log(const std::filesystem::path& aRoot, const diagnostic_file_limits aLimits, application::diagnostic_preferences aPreferences) noexcept
	{
		try {
			Implementation = std::make_unique<implementation>(aRoot, aLimits, aPreferences);
		} catch (...) { }
	}
	quill_log::~quill_log() = default;
	void quill_log::discard_context() noexcept
	{
		if (Implementation) {
			Implementation->discard_context();
		}
	}
	auto quill_log::context_generation() const noexcept -> std::uint64_t { return Implementation ? Implementation->State->Generation.load() : 0U; }
	auto quill_log::configure(application::diagnostic_preferences aPreferences) noexcept -> bool { return Implementation && Implementation->configure(aPreferences); }
	auto quill_log::clear() noexcept -> application::diagnostic_clear_result { return Implementation ? Implementation->clear() : application::diagnostic_clear_result::unavailable; }
	auto quill_log::capture_state() -> application::diagnostic_capture_state { return Implementation ? Implementation->capture_state() : application::diagnostic_capture_state{ }; }
	auto quill_log::flush_for_export(bool aIncludeRecent) -> std::optional<application::diagnostic_export_capture> { return Implementation ? Implementation->flush_for_export(aIncludeRecent) : std::nullopt; }
	auto quill_log::recent_context() -> std::vector<std::string> { return Implementation ? Implementation->recent_context() : std::vector<std::string>{ }; }
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
