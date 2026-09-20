#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_RELEASE_UPDATE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_RELEASE_UPDATE_HPP

#include "bpr/core/model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace bpr::core {

	inline constexpr std::uint64_t release_update_day_seconds           = 86'400U;
	inline constexpr std::size_t   release_update_maximum_assets        = 128U;
	inline constexpr std::size_t   release_update_maximum_tag_bytes     = 128U;
	inline constexpr std::uint64_t release_update_maximum_archive_bytes = 512U * 1024U * 1024U;

	/// \brief Stable-channel precedence; build metadata never changes ordering.
	struct release_update_version
	{
		std::uint64_t Major                                            = 0U;
		std::uint64_t Minor                                            = 0U;
		std::uint64_t Patch                                            = 0U;
		auto          operator<=>(const release_update_version&) const = default;
	};

	/// \brief Parses bounded [v]MAJOR.MINOR.PATCH[+BUILD], refusing prereleases.
	[[nodiscard]] auto parse_release_update_version(std::string_view aTag) noexcept
		-> std::optional<release_update_version>;

	struct release_update_release_id_tag;
	struct release_update_asset_id_tag;
	struct release_update_access_id_tag;
	struct release_update_generation_tag;
	using release_update_release_id = strong_id<release_update_release_id_tag>;
	using release_update_asset_id   = strong_id<release_update_asset_id_tag>;
	using release_update_access_id  = strong_id<release_update_access_id_tag>;
	using release_update_generation = strong_id<release_update_generation_tag>;

	enum class release_update_source : std::uint8_t { unrecognized,
													  better_pull_review_github_com };
	enum class release_update_access_kind : std::uint8_t { unavailable,
														   native_github_com };
	enum class release_update_capability : std::uint8_t {
		eligible,
		managed,
		unsupported_platform,
		untrusted_installation,
		read_only,
		translocated,
		mounted_image,
	};
	enum class release_update_phase : std::uint8_t { idle,
													 checking,
													 available,
													 downloading,
													 verifying,
													 ready,
													 failed };
	enum class release_update_operation : std::uint8_t { check,
														 download,
														 verify };
	enum class release_update_check_mode : std::uint8_t { automatic,
														  manual };
	enum class release_update_reason : std::uint8_t {
		none,
		current,
		skipped,
		disabled,
		managed,
		unsupported_platform,
		untrusted_installation,
		read_only,
		translocated,
		mounted_image,
		access_required,
		unauthenticated,
		forbidden,
		inaccessible,
		offline,
		rate_limited,
		malformed_release,
		invalid_version,
		unstable_release,
		no_compatible_asset,
		ambiguous_asset,
		invalid_digest,
		size_limit,
		certificate,
		redirect,
		timed_out,
		cancelled,
		unavailable,
		unsafe_archive,
		invalid_bundle,
		wrong_architecture,
		wrong_signer,
		digest_mismatch,
		changed_candidate,
		busy,
		not_due,
		no_candidate,
		stale_completion,
		invalid_progress,
		identity_exhausted,
		installer_unavailable,
		shutdown,
	};

	/// \brief Provider-free asset metadata, bounded before policy is invoked.
	struct release_update_asset
	{
		release_update_asset_id    Id;
		std::string                Name;
		std::uint64_t              Size = 0U;
		std::optional<std::string> Digest;
		bool                       Uploaded = true;
	};

	/// \brief A latest-release observation; no URL, note, token or provider type.
	struct release_update_release
	{
		release_update_source             Source = release_update_source::unrecognized;
		release_update_release_id         Id;
		std::string                       Tag;
		bool                              Draft      = false;
		bool                              Prerelease = false;
		std::vector<release_update_asset> Assets;
	};

	/// \brief Complete immutable selection identity, with bounded string fields.
	struct release_update_candidate
	{
		release_update_release_id                   Release;
		release_update_asset_id                     Asset;
		release_update_version                      Version;
		std::string                                 Tag;
		std::string                                 AssetName;
		std::uint64_t                               Size = 0U;
		std::optional<std::array<std::uint8_t, 32>> Sha256;
		auto                                        operator<=>(const release_update_candidate&) const = default;
	};

	/// \brief Selection either offers one exact newer universal ZIP or explains refusal.
	struct release_update_selection
	{
		std::optional<release_update_candidate> Candidate;
		release_update_reason                   Reason = release_update_reason::none;
	};
	/// \brief Applies stable-channel and exact universal-ZIP selection policy.
	[[nodiscard]] auto select_release_update(const release_update_version& aInstalled,
											 const release_update_release& aRelease) -> release_update_selection;

	/// \brief Explicit host clock facts. Monotonic time is never persisted.
	struct release_update_time
	{
		std::int64_t  WallSeconds      = 0;
		std::uint64_t MonotonicSeconds = 0U;
	};

	/// \brief Nonsecret preferences and separate attempted/successful check facts.
	struct release_update_preferences
	{
		bool                                  AutomaticChecks = true;
		std::optional<release_update_version> SkippedVersion;
		std::optional<std::int64_t>           LastAttempt;
		std::optional<std::int64_t>           LastSuccess;
		auto                                  operator<=>(const release_update_preferences&) const = default;
	};

	/// \brief Every completion is sealed to session, request, generation and authority.
	struct release_update_context
	{
		application_session_id                  Session;
		request_id                              Request;
		release_update_generation               Generation;
		release_update_access_id                Access;
		release_update_operation                Operation = release_update_operation::check;
		std::optional<release_update_candidate> Candidate;
		auto                                    operator<=>(const release_update_context&) const = default;
	};

	enum class release_update_command : std::uint8_t { check,
													   download,
													   later,
													   skip,
													   cancel,
													   install };
	/// \brief Candidate-bearing gestures must name the exact accepted selection.
	struct release_update_intent
	{
		release_update_command                  Command = release_update_command::check;
		std::optional<release_update_candidate> Candidate;
	};
	/// \brief A host scheduling opportunity; core still decides whether a check is due.
	struct release_update_tick
	{ };
	/// \brief Changes automatic discovery without disabling explicit manual checks.
	struct release_update_set_automatic
	{
		bool Enabled = true;
	};
	/// \brief Replaces or revokes the separately selected release authority.
	struct release_update_access_changed
	{
		release_update_access_id   Access;
		release_update_access_kind Kind = release_update_access_kind::unavailable;
	};
	/// \brief Closes admission and invalidates all candidate and request authority.
	struct release_update_shutdown
	{ };
	/// \brief One complete bounded metadata observation, never a partial asset page.
	struct release_update_checked
	{
		release_update_context Context;
		release_update_release Release;
	};
	/// \brief Actual received bytes for one exact pending download.
	struct release_update_progress
	{
		release_update_context Context;
		std::uint64_t          Bytes = 0U;
	};
	/// \brief Completion requires actual bytes to equal the selected advertised size.
	struct release_update_downloaded
	{
		release_update_context Context;
		std::uint64_t          Bytes = 0U;
	};
	/// \brief Success supplied only by the platform's complete native validation port.
	struct release_update_verified
	{
		release_update_context Context;
	};
	/// \brief Typed effect failure; a rate limit supplies a relative monotonic delay.
	struct release_update_failed
	{
		release_update_context Context;
		release_update_reason  Reason            = release_update_reason::unavailable;
		std::uint64_t          RetryAfterSeconds = 0U;
	};
	using release_update_event = std::variant<release_update_intent, release_update_tick,
											  release_update_set_automatic, release_update_access_changed, release_update_shutdown,
											  release_update_checked, release_update_progress, release_update_downloaded,
											  release_update_verified, release_update_failed>;

	/// \brief A semantic effect request, or cancellation of the exact previous effect.
	struct release_update_request
	{
		release_update_context Context;
		bool                   Cancel = false;
	};
	/// \brief Command legality and its reason, shared with gesture admission.
	struct release_update_command_projection
	{
		release_update_command Command;
		release_update_reason  DisabledReason = release_update_reason::none;
		[[nodiscard]] auto     enabled() const noexcept -> bool { return DisabledReason == release_update_reason::none; }
	};

	/// \brief Compact deterministic state; retains one candidate and one pending operation.
	struct release_update_state
	{
		application_session_id                  Session;
		request_id                              NextRequest{ 1U };
		release_update_generation               Generation{ 1U };
		release_update_access_id                Access;
		std::optional<release_update_version>   Installed;
		release_update_capability               Capability = release_update_capability::untrusted_installation;
		release_update_preferences              Preferences;
		release_update_phase                    Phase  = release_update_phase::idle;
		release_update_reason                   Reason = release_update_reason::none;
		std::optional<release_update_candidate> Candidate;
		std::optional<release_update_context>   Pending;
		release_update_check_mode               CheckMode        = release_update_check_mode::manual;
		bool                                    OfferVisible     = false;
		bool                                    Stopped          = false;
		bool                                    Exhausted        = false;
		std::uint64_t                           Bytes            = 0U;
		std::uint64_t                           MonotonicSeconds = 0U;
		std::uint64_t                           ScheduleAnchor   = 0U;
		std::uint64_t                           ScheduleDelay    = 0U;
		std::uint64_t                           RateAnchor       = 0U;
		std::uint64_t                           RateDelay        = 0U;
	};

	/// \brief Accepted distinguishes a valid observation from a stale/illegal event.
	struct release_update_output
	{
		bool                                Changed            = false;
		bool                                PreferencesChanged = false;
		bool                                Accepted           = false;
		release_update_reason               Diagnostic         = release_update_reason::none;
		std::vector<release_update_request> Requests;
	};

	/// \brief Normalizes persisted wall-clock facts and starts the monotonic schedule.
	[[nodiscard]] auto make_release_update_state(application_session_id aSession,
												 std::string_view aInstalled, release_update_capability aCapability,
												 release_update_preferences aPreferences, release_update_time aTime) -> release_update_state;
	/// \brief Projects every updater command, including the uncomposed installer refusal.
	[[nodiscard]] auto release_update_commands(const release_update_state& aState)
		-> std::array<release_update_command_projection, 6>;
	/// \brief Applies policy without clocks, credentials, I/O or platform dependencies.
	[[nodiscard]] auto transition(release_update_state&       aState,
								  const release_update_event& aEvent, release_update_time aTime) -> release_update_output;
}

#endif
