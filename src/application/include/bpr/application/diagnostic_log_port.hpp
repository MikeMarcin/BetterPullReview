#ifndef BPR_APPLICATION_DIAGNOSTIC_LOG_PORT_HPP
#define BPR_APPLICATION_DIAGNOSTIC_LOG_PORT_HPP
#include "bpr/core/review_submission_unknown.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
namespace bpr::core {
	enum class epoch_failure : std::uint8_t;
	enum class github_failure : std::uint8_t;
}
namespace bpr::application {
	enum class github_read_validation_error : std::uint8_t;
	enum class epoch_catalog_failure : std::uint8_t;
	enum class git_cache_failure : std::uint8_t;
	enum class git_smart_transport_failure : std::uint8_t;
	enum class git_smart_transport_step : std::uint8_t;
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
													 epoch,
													 review_comment,
													 /// [[BPR-1540]]. A submitted review left unknown.
													 review_submission,
													 /// [[BPR-1680]]. A Home discovery refresh or one of its reads.
													 discovery };
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
		/// [[BPR-1560]]. The merge base GitHub names for the pull request.
		get_merge_base,
		list_requested_reviewers,
		list_home_pull_requests,
		list_home_review_membership,
		list_review_requests,
	};
	enum class diagnostic_outcome : std::uint8_t { started,
												   succeeded,
												   failed,
												   cancelled,
												   response_received,
												   shutdown };
	enum class diagnostic_transport_failure_source : std::uint8_t {
		none,
		native_transport,
		http_response,
		network_reply,
	};
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
	enum class diagnostic_detail : std::uint8_t { standard,
												  verbose };
	enum class diagnostic_privacy : std::uint8_t { anonymized,
												   include_locally };
	/// \brief Independent capture choices, scoped to one BPR profile.
	struct diagnostic_preferences
	{
		bool               Enabled                                                 = true;
		diagnostic_detail  Detail                                                  = diagnostic_detail::standard;
		diagnostic_privacy Privacy                                                 = diagnostic_privacy::anonymized;
		auto               operator==(const diagnostic_preferences&) const -> bool = default;
	};
	[[nodiscard]] inline auto valid_diagnostic_preferences(const diagnostic_preferences& aValue) noexcept -> bool
	{
		return (aValue.Detail == diagnostic_detail::standard || aValue.Detail == diagnostic_detail::verbose)
			&& (aValue.Privacy == diagnostic_privacy::anonymized || aValue.Privacy == diagnostic_privacy::include_locally);
	}
	struct diagnostic_event
	{
		diagnostic_operation                        Operation      = diagnostic_operation::application;
		diagnostic_outcome                          Outcome        = diagnostic_outcome::started;
		std::uint64_t                               Request        = 0;
		std::uint64_t                               WindowSession  = 0;
		std::uint16_t                               HttpStatus     = 0;
		diagnostic_paged_operation                  PagedOperation = diagnostic_paged_operation::none;
		diagnostic_summary                          Summary{ };
		diagnostic_github_failure                   GithubFailure;
		diagnostic_epoch_stage                      EpochStage = diagnostic_epoch_stage::none;
		diagnostic_epoch_failure                    EpochFailure;
		std::optional<git_pack_stream_failure>      EpochPackFailure;
		std::optional<github_read_validation_error> RelationshipDetail;
		diagnostic_transport_failure_source         TransportFailureSource = diagnostic_transport_failure_source::none;
		git_smart_transport_step                    GitStep{ };
		diagnostic_detail                           Detail = diagnostic_detail::standard;
		/// Capture generation for delayed intermediate producers; zero means current.
		std::uint64_t ContextGeneration = 0;
		/// [[BPR-1540]]. Why a submitted review is still unknown: enums and
		/// counts only, logged as the `review_submission` failure domain.
		std::optional<core::review_submission_unknown_diagnosis> ReviewSubmission;
		/// [[BPR-1722]]. How a submitted review ended when that is known
		/// (published, refused, settled or not sent): enums and counts only,
		/// logged as the `review_submission` failure domain.
		std::optional<core::review_submission_ending_diagnosis> ReviewEnding;
	};
	enum class diagnostic_log_status : std::uint8_t { available,
													  unavailable,
													  full,
													  dropped,
													  closed };
	/// \brief Bounded snapshot of already sanitized context; never a raw-event buffer.
	struct diagnostic_capture_state
	{
		diagnostic_preferences Preferences;
		diagnostic_log_status  Status       = diagnostic_log_status::unavailable;
		std::uint64_t          Dropped      = 0;
		std::size_t            RecentEvents = 0;
		std::size_t            RecentBytes  = 0;
	};
	/// \brief Nonblocking bounded diagnostic submission; failure never changes domain behavior.
	class diagnostic_log_port
	{
	public:
		virtual ~diagnostic_log_port()                                                                      = default;
		[[nodiscard]] virtual auto record(const diagnostic_event& aEvent) noexcept -> diagnostic_log_status = 0;
		[[nodiscard]] virtual auto status() const noexcept -> diagnostic_log_status                         = 0;
		/// \brief Immediately invalidates queued/recent account context without waiting for I/O.
		virtual void               discard_context() noexcept { }
		[[nodiscard]] virtual auto context_generation() const noexcept -> std::uint64_t { return 0; }
	};
	enum class diagnostic_clear_result { cleared,
										 busy,
										 unavailable,
										 partial };
	/// \brief Cutoff after a bounded flush; optional recent records are already sanitized.
	struct diagnostic_export_capture
	{
		std::string              Session;
		std::uint64_t            Sequence = 0;
		std::vector<std::string> Recent;
		/// [[BPR-1722]]. The running build, as its records name it
		/// (`VERSION-DIGEST-CONFIGURATION`); empty when not known.
		std::string Build;
	};
	/// \brief Effect-lane control capability; barriers never run on the decision lane.
	class diagnostic_control_port
	{
	public:
		virtual ~diagnostic_control_port()                                                         = default;
		[[nodiscard]] virtual auto configure(diagnostic_preferences aPreferences) noexcept -> bool = 0;
		[[nodiscard]] virtual auto clear() noexcept -> diagnostic_clear_result                     = 0;
		[[nodiscard]] virtual auto capture_state() -> diagnostic_capture_state                     = 0;
		/// \brief Explicit bounded export selection; reading it expires obsolete context first.
		[[nodiscard]] virtual auto recent_context() -> std::vector<std::string> = 0;
		/// \brief Flushes accepted file events, captures a sequence cutoff and only explicitly selected context.
		[[nodiscard]] virtual auto flush_for_export(bool) -> std::optional<diagnostic_export_capture> { return std::nullopt; }
	};
}
#endif
