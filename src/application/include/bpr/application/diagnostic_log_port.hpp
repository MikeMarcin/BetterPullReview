#ifndef BPR_APPLICATION_DIAGNOSTIC_LOG_PORT_HPP
#define BPR_APPLICATION_DIAGNOSTIC_LOG_PORT_HPP
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
namespace bpr::core {
	enum class epoch_failure : std::uint8_t;
	enum class github_failure : std::uint8_t;
}
namespace bpr::application {
	enum class github_read_validation_error : std::uint8_t;
	enum class epoch_catalog_failure : std::uint8_t;
	enum class git_cache_failure : std::uint8_t;
	enum class git_smart_transport_failure : std::uint8_t;
	enum class git_protocol_v2_failure : std::uint8_t;
	enum class git_pack_stream_failure : std::uint8_t;
	enum class diagnostic_epoch_stage : std::uint8_t {
		none,
		catalog_open,
		catalog_recovery,
		catalog_prepare,
		cache_open,
		pack_sink,
		smart_acquire,
		protocol_acquire,
		cache_inventory,
		revision_prepare
	};
	using diagnostic_epoch_failure  = std::variant<std::monostate, epoch_catalog_failure,
												   git_cache_failure, git_smart_transport_failure, git_protocol_v2_failure, core::epoch_failure>;
	using diagnostic_github_failure = std::variant<std::monostate, core::github_failure, github_read_validation_error>;

	enum class diagnostic_operation : std::uint8_t { application,
													 viewer,
													 capability,
													 pull_request,
													 epoch };
	enum class diagnostic_paged_operation : std::uint8_t {
		none,
		get_viewer_identity,
		get_repository,
		get_pull_request,
		list_pull_request_files,
		list_pull_request_commits,
		list_reviews,
		list_review_threads,
		list_issue_comments,
		list_checks,
		search_pull_requests,
		list_review_comments,
		list_pull_request_timeline,
	};
	enum class diagnostic_outcome : std::uint8_t { started,
												   succeeded,
												   failed,
												   cancelled,
												   response_received,
												   shutdown };
	enum class diagnostic_summary_state : std::uint8_t { absent,
														 sanitized,
														 omitted_invalid,
														 omitted_limit,
														 omitted_failure };
	/// \brief Fixed-capacity prose sanitized by the credential-owning adapter before logging.
	struct diagnostic_summary
	{
		std::array<char, 2048>   Text{ };
		std::uint16_t            Size  = 0;
		diagnostic_summary_state State = diagnostic_summary_state::absent;
		[[nodiscard]] auto       text() const noexcept -> std::string_view { return { Text.data(), Size <= Text.size() ? Size : 0U }; }
		auto                     operator==(const diagnostic_summary&) const -> bool = default;
	};
	struct diagnostic_event
	{
		diagnostic_operation                   Operation      = diagnostic_operation::application;
		diagnostic_outcome                     Outcome        = diagnostic_outcome::started;
		std::uint64_t                          Request        = 0;
		std::uint64_t                          WindowSession  = 0;
		std::uint16_t                          HttpStatus     = 0;
		diagnostic_paged_operation             PagedOperation = diagnostic_paged_operation::none;
		diagnostic_summary                     Summary{ };
		diagnostic_github_failure              GithubFailure;
		diagnostic_epoch_stage                 EpochStage = diagnostic_epoch_stage::none;
		diagnostic_epoch_failure               EpochFailure;
		std::optional<git_pack_stream_failure> EpochPackFailure;
	};
	enum class diagnostic_log_status : std::uint8_t { available,
													  unavailable,
													  full,
													  dropped,
													  closed };
	/// \brief Nonblocking bounded diagnostic submission; failure never changes domain behavior.
	class diagnostic_log_port
	{
	public:
		virtual ~diagnostic_log_port()                                                                      = default;
		[[nodiscard]] virtual auto record(const diagnostic_event& aEvent) noexcept -> diagnostic_log_status = 0;
		[[nodiscard]] virtual auto status() const noexcept -> diagnostic_log_status                         = 0;
	};
}
#endif
