#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_PROJECTION_HPP

#include "bpr/application/github_capability_diagnostic.hpp"
#include "bpr/application/github_read_progress.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/cached_review_overview.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/home_open.hpp"
#include "bpr/core/markdown_image.hpp"
#include "bpr/core/markdown_navigation.hpp"
#include "bpr/core/review_queue.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace bpr::application {
	enum class github_profile_product : std::uint8_t {
		github_com,
		github_enterprise_server,
	};

	enum class github_profile_transport : std::uint8_t {
		github_cli,
		native_https,
	};

	/// \brief Describes the github profile option record.
	struct github_profile_option
	{
		core::host_profile_id    Profile{ };
		std::string              Label{ };
		std::string              Hostname{ };
		github_profile_product   Product   = github_profile_product::github_com;
		github_profile_transport Transport = github_profile_transport::native_https;

		auto operator<=>(const github_profile_option&) const = default;
	};

	using github_intent = std::variant<
		core::deactivate_github_identity_intent,
		core::activate_host_profile_intent,
		core::open_pull_request_intent,
		core::refresh_pull_request_intent>;
	using github_intent_dispatch = std::function<void(github_intent)>;

	enum class github_rate_limit_resource_projection : std::uint8_t {
		core,
		search,
		graphql,
	};

	/// \brief Describes the github rate limit projection record.
	struct github_rate_limit_projection
	{
		github_rate_limit_resource_projection Resource         = github_rate_limit_resource_projection::core;
		std::uint64_t                         Limit            = 0;
		std::uint64_t                         Remaining        = 0;
		std::uint64_t                         ResetUnixSeconds = 0;
		bool                                  SecondaryLimited = false;

		auto operator<=>(const github_rate_limit_projection&) const = default;
	};

	/// \brief Describes the github rate limits projection record.
	struct github_rate_limits_projection
	{
		std::optional<github_rate_limit_projection> Core    = std::nullopt;
		std::optional<github_rate_limit_projection> Search  = std::nullopt;
		std::optional<github_rate_limit_projection> Graphql = std::nullopt;

		auto operator<=>(const github_rate_limits_projection&) const = default;
	};

	/// \brief Describes the github transition projection record.
	struct github_transition_projection
	{
		core::github_changed_domain                     Changed = core::github_changed_domain::none;
		std::vector<core::github_command_projection>    Commands{ };
		std::optional<core::github_status_delta>        Status       = std::nullopt;
		std::optional<core::github_identity_delta>      Identity     = std::nullopt;
		std::optional<core::github_capability_delta>    Capabilities = std::nullopt;
		std::optional<core::pull_request_summary_delta> PullRequest  = std::nullopt;
		std::vector<core::github_diagnostic>            Diagnostics{ };
		/// [[BPR-1200]]. The first read the core refused, and on which connection.
		std::optional<core::pull_request_refusal> Refusal = std::nullopt;
		/// [[BPR-1575]]. The read this transition started or ended, as the core
		/// named it: a refresh's own lifecycle, which its control presents.
		std::optional<core::pull_request_read_delta> Read = std::nullopt;

		auto operator<=>(const github_transition_projection&) const = default;
	};

	/// \brief What kind of credential the accepted session acts under ([[BPR-0660]]).
	enum class github_grant_kind_projection : std::uint8_t {
		/// A device grant this host enrolled, or a binding restored from one.
		device_grant,
		/// A token the reviewer supplied; nothing renews it and nothing remembers it.
		token,
	};

	/// \brief The accepted session's grant facts, with no credential material.
	///
	/// \details [[BPR-0660]]. What RFC-BPR-0028's account card states: the
	/// kind, when a device grant was issued (seconds since the epoch), whether
	/// it renews itself, and whether it is remembered on this Mac.
	struct github_grant_projection
	{
		github_grant_kind_projection Kind = github_grant_kind_projection::token;
		std::optional<std::int64_t>  SignedInAt;
		bool                         Renews     = false;
		bool                         Remembered = false;

		auto operator<=>(const github_grant_projection&) const = default;
	};

	/// \brief Describes the github session projection record.
	struct github_session_projection
	{
		core::host_profile_id         Profile{ };
		core::git_host_id             Host{ };
		core::github_account_id       Account{ };
		std::string                   Login{ };
		std::string                   DisplayName{ };
		core::github_capabilities     Capabilities{ };
		core::capability_revision     CapabilityRevision{ };
		github_rate_limits_projection RateLimits{ };
		/// [[BPR-0660]]. Absent while the host has not decided the grant.
		std::optional<github_grant_projection> Grant;

		auto operator<=>(const github_session_projection&) const = default;
	};

	enum class github_pull_request_state_projection : std::uint8_t {
		open,
		closed,
		merged,
	};

	enum class github_review_state_projection : std::uint8_t {
		none,
		pending,
		commented,
		approved,
		changes_requested,
		dismissed,
	};

	/// Read-only metadata while the complete snapshot is still being acquired.
	/// Deliberately contains no snapshot, file, epoch or mutation authority.
	struct github_loading_overview_projection
	{
		std::string                          Title;
		std::string                          Body;
		std::string                          AuthorLogin;
		std::string                          BaseRef;
		std::string                          HeadRef;
		std::size_t                          ReportedFiles                                                = 0U;
		std::size_t                          ReportedCommits                                              = 0U;
		github_pull_request_state_projection State                                                        = github_pull_request_state_projection::open;
		bool                                 Draft                                                        = false;
		auto                                 operator<=>(const github_loading_overview_projection&) const = default;
	};

	enum class cached_source_status : std::uint8_t { loading,
													 available,
													 unavailable,
													 binary,
													 too_large };
	struct cached_review_source_projection
	{
		std::string          Path, BaseOid, HeadOid;
		cached_source_status Status = cached_source_status::unavailable;
		/// Missing path is distinct from an observed empty blob.
		std::optional<std::string> BaseText, HeadText;
		auto                       operator<=>(const cached_review_source_projection&) const = default;
	};

	/// \brief Previously accepted overview, optional file catalogue and inert thread history; freshness is unknown.
	struct github_cached_overview_projection
	{
		github_loading_overview_projection                          Overview;
		std::string                                                 BaseOid, HeadOid;
		std::optional<std::vector<core::cached_review_file>>        FileCatalogue;
		std::optional<std::vector<core::cached_review_thread>>      ReviewThreads;
		std::optional<std::vector<core::cached_discussion_message>> GeneralComments, ReviewSummaries;
		auto                                                        operator<=>(const github_cached_overview_projection&) const = default;
	};

	enum class github_conversation_kind : std::uint8_t {
		review,
		review_thread,
		review_comment,
		issue_comment,
	};

	enum class github_conversation_diff_side : std::uint8_t {
		left,
		right,
	};
	enum class github_conversation_remote_subject : std::uint8_t { line,
																   file };

	enum class github_conversation_anchor_availability : std::uint8_t {
		current,
		selected_comparison_mismatch,
		selected_line_out_of_bounds,
		outdated_revision,
		file_level,
		missing_side,
		missing_current_line,
	};

	/// \brief Describes the github conversation projection record.
	struct github_conversation_projection
	{
		std::string                  Id{ };
		std::optional<std::string>   ParentId = std::nullopt;
		github_conversation_kind     Kind     = github_conversation_kind::issue_comment;
		std::string                  AuthorLogin{ };
		std::string                  Body{ };
		std::string                  Path{ };
		std::optional<std::uint32_t> Line         = std::nullopt;
		std::optional<std::uint32_t> OriginalLine = std::nullopt;
		/// [[BPR-1564]]. GitHub's `start_line` of a comment on several lines:
		/// its first line, on the same side as `Line`; none on one line.
		std::optional<std::uint32_t>                 StartLine = std::nullopt;
		std::optional<github_conversation_diff_side> DiffSide  = std::nullopt;
		github_conversation_anchor_availability      AnchorAvailability
			= github_conversation_anchor_availability::missing_current_line;
		github_review_state_projection ReviewState = github_review_state_projection::none;
		bool                           Resolved    = false;
		bool                           Outdated    = false;
		bool                           Deleted     = false;
		/// Remote subject stays honest; Line is the displayed location when SemanticAnchor is present.
		github_conversation_remote_subject   RemoteSubject = github_conversation_remote_subject::line;
		std::optional<core::source_line_ref> SemanticAnchor;
		/// Body remains authoritative server content. Only a validated root has a prose projection.
		std::optional<std::string> ReviewerProse;
		/// Server-observed creation/submission time; empty for legacy observations.
		std::string CreatedAt;
		/// Review owning a file comment, in the same identity space as review summaries.
		std::string ReviewId;
		/// [[BPR-1746]]. On a review thread: GitHub's `viewerCanReply`,
		/// and the pull request's lock, when the conversation read carries
		/// them; absent otherwise, and then nothing is refused on them.
		std::optional<bool> ViewerCanReply;
		std::optional<bool> ConversationLocked;

		auto operator<=>(const github_conversation_projection&) const = default;
	};

	/// \brief RFC-BPR-0084. One decoded frame: premultiplied ARGB, 32 bits per
	/// pixel in native byte order, rows packed (`Width * Scale * 4` bytes each).
	struct markdown_image_frame
	{
		std::vector<std::uint8_t> Pixels;
		/// Display time of an animation frame; zero for a still image.
		std::uint32_t DelayMs = 0U;

		auto operator==(const markdown_image_frame&) const -> bool = default;
	};

	/// \brief RFC-BPR-0084. An image decoded within the bounds, ready to draw.
	struct markdown_decoded_image
	{
		/// The natural size in logical pixels, as a browser lays it out.
		std::uint32_t Width  = 0U;
		std::uint32_t Height = 0U;
		/// Pixels per logical pixel in each frame: 1 for a raster image, 2 for
		/// an SVG rasterized for high-density screens.
		std::uint32_t                     Scale = 1U;
		std::vector<markdown_image_frame> Frames;

		[[nodiscard]] auto bytes() const noexcept -> std::size_t
		{
			std::size_t total = 0U;
			for (const auto& frame : Frames) {
				total += frame.Pixels.size();
			}
			return total;
		}

		auto operator==(const markdown_decoded_image&) const -> bool = default;
	};

	/// \brief RFC-BPR-0084. The state of one image of one rendered body.
	/// \details Carries no address, header or credential: the reader matches it
	/// to its own reference by body identity, ordinal and the source text it
	/// sent, and draws the shared pixels when loaded.
	struct markdown_image_projection
	{
		core::markdown_context                        Context;
		std::string                                   Body;
		std::uint32_t                                 Ordinal = 0U;
		std::string                                   Source;
		core::markdown_image_state                    State = core::markdown_image_state::loading;
		std::shared_ptr<const markdown_decoded_image> Image;

		auto operator<=>(const markdown_image_projection& aOther) const
		{
			return std::tie(Context, Body, Ordinal, Source, State) <=> std::tie(aOther.Context, aOther.Body, aOther.Ordinal, aOther.Source, aOther.State);
		}
		auto operator==(const markdown_image_projection& aOther) const -> bool
		{
			return (*this <=> aOther) == 0 && Image == aOther.Image;
		}
	};

	/// \brief Describes the github pull request projection record.
	struct github_pull_request_projection
	{
		core::host_profile_id                       Profile{ };
		core::git_host_id                           Host{ };
		core::github_account_id                     Account{ };
		core::provider_snapshot_id                  Snapshot{ };
		core::repository_id                         Repository{ };
		core::pull_request_id                       PullRequest{ };
		std::string                                 Owner{ };
		std::string                                 RepositoryName{ };
		core::pull_request_number                   Number{ };
		std::string                                 Title{ };
		std::string                                 Body{ };
		github_pull_request_state_projection        State = github_pull_request_state_projection::open;
		bool                                        Draft = false;
		std::string                                 AuthorLogin{ };
		std::string                                 BaseRef{ };
		std::string                                 HeadRef{ };
		std::string                                 BaseOid{ };
		std::string                                 HeadOid{ };
		std::size_t                                 FileCount              = 0;
		std::size_t                                 CommitCount            = 0;
		std::size_t                                 ReturnedCommitCount    = 0;
		std::size_t                                 ReviewCount            = 0;
		std::size_t                                 ReviewThreadCount      = 0;
		std::size_t                                 IssueCommentCount      = 0;
		bool                                        ReviewThreadsAvailable = false;
		core::github_capabilities                   Capabilities{ };
		core::capability_revision                   CapabilityRevision{ };
		github_rate_limits_projection               RateLimits{ };
		std::vector<github_conversation_projection> Conversations{ };
		/// Exact window publication admitted for explicit body-link gestures.
		std::optional<core::markdown_context> MarkdownContext;
		bool                                  PublicPreview           = false;
		bool                                  ReviewCommentsAvailable = false;

		auto operator<=>(const github_pull_request_projection&) const = default;
	};

	/// \brief Identifies the bounded sign-in step that refused a session.
	/// No request, credential, or provider response crosses this projection.
	enum class github_bootstrap_failure_stage : std::uint8_t {
		none,
		local_enrollment,
		account_identity,
		capability_discovery,
		session_acceptance,
	};

	/// \brief Describes the github host diagnostic projection record.
	struct github_host_diagnostic_projection
	{
		core::github_reason_code                    Reason            = core::github_reason_code::none;
		std::optional<std::uint32_t>                RetryAfterSeconds = std::nullopt;
		github_rate_limits_projection               RateLimits{ };
		std::optional<github_rate_limit_projection> RateLimitCause   = std::nullopt;
		github_bootstrap_failure_stage              BootstrapStage   = github_bootstrap_failure_stage::none;
		github_capability_diagnostic                CapabilityDetail = github_capability_diagnostic::none;
		/// [[BPR-0644]]. The failed session's remembered device grant is still on
		/// this machine: nothing erased it, and the next launch tries it again.
		/// Only the person's Sign out, or a new sign-in, removes it.
		bool GrantKept = false;
		/// [[BPR-0644]]. The session failed because its credential signed in as
		/// a GitHub account other than the one its binding named. Under the
		/// owner's decision of 2026-09-24 a remembered device grant is kept
		/// through this like every other failure (`GrantKept`), and the Accounts
		/// page says it as an error until the person signs out or signs in again.
		bool AccountMismatch = false;

		auto operator<=>(const github_host_diagnostic_projection&) const = default;
	};

	/// \brief Describes the github presentation update record.
	struct github_presentation_update
	{
		std::optional<github_transition_projection>       Transition  = std::nullopt;
		std::optional<github_session_projection>          Session     = std::nullopt;
		std::optional<github_pull_request_projection>     PullRequest = std::nullopt;
		std::vector<github_host_diagnostic_projection>    HostDiagnostics{ };
		std::optional<github_read_progress>               ReadProgress    = std::nullopt;
		std::optional<github_loading_overview_projection> LoadingOverview = std::nullopt;
		/// A sibling's server observation updates content, not Home/modality,
		/// first-open filters or the window's chosen comparison.
		bool                                             PreserveWindowNavigation = false;
		std::optional<cached_review_source_projection>   CachedSource             = std::nullopt;
		std::optional<github_cached_overview_projection> CachedOverview           = std::nullopt;
		/// RFC-BPR-0084. Images of this window's rendered bodies that changed state.
		std::vector<markdown_image_projection> Images{ };

		auto operator<=>(const github_presentation_update&) const = default;
	};

	/// \brief [[BPR-1357]]. What Home shows of the pull request it asked to open.
	///
	/// \details Empty when nothing is being opened and the last open did not
	/// fail. Published in the same turn as the gesture that asked, so the row or
	/// the bar that asked can show it before anything reaches GitHub.
	struct home_open_projection
	{
		std::optional<core::home_open_state> State;
		/// The core's decisions on what the state offers.
		bool Cancellable = false;
		bool Retryable   = false;

		auto operator<=>(const home_open_projection&) const = default;
	};

	/// \brief [[BPR-1357]]. Cancel or Retry on what Home shows, by its read.
	struct cancel_home_open
	{
		core::request_id Request;

		auto operator<=>(const cancel_home_open&) const = default;
	};

	struct retry_home_open
	{
		core::request_id Request;

		auto operator<=>(const retry_home_open&) const = default;
	};

	using home_open_intent   = std::variant<cancel_home_open, retry_home_open>;
	using home_open_dispatch = std::function<void(home_open_intent)>;

	/// \brief Provides the github projection port contract.
	class github_projection_port
	{
	public:
		virtual ~github_projection_port() = default;

		// Delivery is synchronous but observational: exceptions are contained by
		// the host and cannot veto accepted state or cleanup. These records contain
		// no provider requests, credentials, transports, or provider read models.
		virtual void publish(const github_presentation_update& aUpdate) = 0;
	};

	/// \brief [[BPR-1338]]. A Recently viewed row, by its place in the window's published list.
	struct recent_review_row
	{
		std::size_t Index = 0;

		auto operator<=>(const recent_review_row&) const = default;
	};

	/// \brief [[BPR-1338]]. Which pull request's own GitHub page a gesture asks to open.
	///
	/// \details Names a pull request, never an address: the window's open pull
	/// request by its current publication, a Home queue row by its queue
	/// identity, or a Recently viewed row by its place. The host admits it
	/// against what it accepted and builds the address itself.
	using pull_request_page_intent = std::variant<core::markdown_context, core::pull_request_ref, recent_review_row>;

	/// \brief [[BPR-1342]]. Which pull request a Home gesture asks a new window
	/// of this profile to open.
	///
	/// \details Names a row, never an address or a destination: a queue row by
	/// its queue identity, or a Recently viewed row by its place. The host
	/// admits it against what it accepted before any window is asked for.
	using review_window_intent = std::variant<core::pull_request_ref, recent_review_row>;
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_PROJECTION_HPP
