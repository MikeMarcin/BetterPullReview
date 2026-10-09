#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_GITHUB_MODEL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_GITHUB_MODEL_HPP

#include "bpr/core/model.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bpr::core {
	struct host_profile_id_tag;
	struct git_host_id_tag;
	struct github_account_id_tag;
	struct repository_id_tag;
	struct pull_request_id_tag;
	struct provider_snapshot_id_tag;
	struct capability_revision_tag;
	struct github_identity_generation_tag;

	using host_profile_id            = strong_id<host_profile_id_tag>;
	using git_host_id                = strong_id<git_host_id_tag>;
	using github_account_id          = strong_id<github_account_id_tag>;
	using repository_id              = strong_id<repository_id_tag>;
	using pull_request_id            = strong_id<pull_request_id_tag>;
	using provider_snapshot_id       = strong_id<provider_snapshot_id_tag>;
	using capability_revision        = strong_id<capability_revision_tag>;
	using github_identity_generation = strong_id<github_identity_generation_tag>;

	// Declared opaquely here so identity, intent, and state records can
	// carry the discriminant without this header depending on the
	// signed-out read policy that defines its enumerators.
	enum class github_access_mode : std::uint8_t;

	inline constexpr std::uint32_t maximum_pull_request_number = 2'147'483'647U;

	/// \brief Provides the pull request number contract.
	class pull_request_number
	{
	public:
		constexpr pull_request_number() = default;
		explicit constexpr pull_request_number(const std::uint32_t aValue) noexcept
			: Value{ aValue }
		{
		}

		[[nodiscard]] constexpr auto value() const noexcept -> std::uint32_t
		{
			return Value;
		}

		[[nodiscard]] constexpr auto valid() const noexcept -> bool
		{
			return Value != 0U && Value <= maximum_pull_request_number;
		}

		auto operator<=>(const pull_request_number&) const = default;

	private:
		std::uint32_t Value = 0;
	};

	/// \brief Describes the repository locator record.
	struct repository_locator
	{
		std::string Owner;
		std::string Name;

		auto operator<=>(const repository_locator&) const = default;
	};

	inline constexpr std::size_t maximum_repository_owner_length = 100U;
	inline constexpr std::size_t maximum_repository_name_length  = 100U;

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const repository_locator& aLocator) noexcept -> bool;

	enum class github_capability_state : std::uint8_t {
		unknown,
		supported,
		unsupported,
	};

	enum class github_capability : std::uint8_t {
		review_threads,
		viewed_file_mutations,
		resolve_thread,
		unresolve_thread,
	};

	/// \brief Describes the github capabilities record.
	struct github_capabilities
	{
		github_capability_state ReviewThreads       = github_capability_state::unknown;
		github_capability_state ViewedFileMutations = github_capability_state::unknown;
		github_capability_state ResolveThread       = github_capability_state::unknown;
		github_capability_state UnresolveThread     = github_capability_state::unknown;

		auto operator<=>(const github_capabilities&) const = default;
	};

	/// \brief Computes the capability state result.
	[[nodiscard]] auto capability_state(
		const github_capabilities& aCapabilities,
		github_capability          aCapability) noexcept -> github_capability_state;

	/// \brief Computes the capability supported result.
	[[nodiscard]] auto capability_supported(
		const github_capabilities& aCapabilities,
		github_capability          aCapability) noexcept -> bool;

	/// \brief Describes the github session request context record.
	struct github_session_request_context
	{
		request_id                 Request;
		application_session_id     ApplicationSession;
		host_profile_id            Profile;
		github_identity_generation IdentityGeneration;

		auto operator<=>(const github_session_request_context&) const = default;
	};

	/// \brief Describes the pull request request context record.
	struct pull_request_request_context
	{
		request_id                     Request;
		application_session_id         ApplicationSession;
		host_profile_id                Profile;
		git_host_id                    Host;
		github_account_id              Account;
		repository_locator             Repository;
		pull_request_number            Number;
		std::optional<repository_id>   ExpectedRepository;
		std::optional<pull_request_id> ExpectedPullRequest;
		capability_revision            Capabilities;
		github_identity_generation     IdentityGeneration;
		request_generation             Generation;
		// The mode this read was opened in. Last and defaulted so every
		// context written before the mode existed still means the
		// authenticated read it meant, and part of the defaulted comparison
		// so a completion issued in one mode can never be admitted in the
		// other. Under public preview the account is zero by rule; the
		// discriminant is what keeps that from reading as a malformed
		// authenticated context.
		github_access_mode Mode{ };

		auto operator<=>(const pull_request_request_context&) const = default;
	};

	/// \brief [[BPR-0988]]. One authenticated read scoped to a repository and
	/// naming no pull request: a personal discovery search.
	///
	/// \details The same session, profile, host, account and generation facts
	/// a pull request read carries, so a transport correlates it against the
	/// same exact account authority; the repository is the only scope. There
	/// is no mode: a discovery read needs a viewer, and a signed-out session
	/// has none.
	struct repository_request_context
	{
		request_id                 Request;
		application_session_id     ApplicationSession;
		host_profile_id            Profile;
		git_host_id                Host;
		github_account_id          Account;
		repository_locator         Repository;
		capability_revision        Capabilities;
		github_identity_generation IdentityGeneration;

		auto operator<=>(const repository_request_context&) const = default;
	};

	/// \brief Describes the activate host profile intent record.
	struct activate_host_profile_intent
	{
		host_profile_id Profile;
		// Last and defaulted so every activation written before the mode
		// existed still asks for the authenticated activation it asked for,
		// and part of the defaulted comparison so no equality can drop it.
		github_access_mode Mode{ };

		auto operator<=>(const activate_host_profile_intent&) const = default;
	};

	/// \brief Describes the open pull request intent record.
	struct open_pull_request_intent
	{
		repository_locator  Repository;
		pull_request_number Number;

		auto operator<=>(const open_pull_request_intent&) const = default;
	};

	/// \brief Describes the refresh pull request intent record.
	struct refresh_pull_request_intent
	{
		auto operator<=>(const refresh_pull_request_intent&) const = default;
	};

	/// \brief Describes the deactivate github identity intent record.
	struct deactivate_github_identity_intent
	{
		auto operator<=>(const deactivate_github_identity_intent&) const = default;
	};

	enum class github_failure : std::uint8_t {
		authentication,
		permission,
		network,
		api_version,
		capability,
		rate_limit,
		malformed_response,
		response_too_large,
		vault,
		certificate,
		redirect,
		timeout,
		cli_unavailable,
	};

	/// \brief Describes the github session ready event record.
	struct github_session_ready_event
	{
		github_session_request_context Context;
		git_host_id                    Host;
		github_account_id              Account;
		github_capabilities            Capabilities;
		capability_revision            CapabilityRevision;

		auto operator<=>(const github_session_ready_event&) const = default;
	};

	/// \brief Describes the github session failed event record.
	struct github_session_failed_event
	{
		github_session_request_context Context;
		github_failure                 Failure = github_failure::network;

		auto operator<=>(const github_session_failed_event&) const = default;
	};

	/// \brief Describes the github session cancelled event record.
	struct github_session_cancelled_event
	{
		github_session_request_context Context;

		auto operator<=>(const github_session_cancelled_event&) const = default;
	};

	/// \brief Describes the pull request loaded event record.
	struct pull_request_loaded_event
	{
		pull_request_request_context Context;
		repository_id                Repository;
		pull_request_id              PullRequest;
		provider_snapshot_id         Snapshot;

		auto operator<=>(const pull_request_loaded_event&) const = default;
	};

	/// \brief Describes the pull request load failed event record.
	struct pull_request_load_failed_event
	{
		pull_request_request_context Context;
		github_failure               Failure = github_failure::network;

		auto operator<=>(const pull_request_load_failed_event&) const = default;
	};

	/// \brief Describes the pull request load cancelled event record.
	struct pull_request_load_cancelled_event
	{
		pull_request_request_context Context;

		auto operator<=>(const pull_request_load_cancelled_event&) const = default;
	};

	using github_event = std::variant<
		activate_host_profile_intent,
		deactivate_github_identity_intent,
		open_pull_request_intent,
		refresh_pull_request_intent,
		github_session_ready_event,
		github_session_failed_event,
		github_session_cancelled_event,
		pull_request_loaded_event,
		pull_request_load_failed_event,
		pull_request_load_cancelled_event>;

	/// \brief Describes the github identity scope record.
	struct github_identity_scope
	{
		std::optional<host_profile_id>      Profile;
		std::optional<git_host_id>          Host;
		std::optional<github_account_id>    Account;
		std::optional<repository_id>        Repository;
		std::optional<pull_request_id>      PullRequest;
		std::optional<provider_snapshot_id> Snapshot;

		auto operator<=>(const github_identity_scope&) const = default;
	};

	/// \brief Describes the invalidate github identity request record.
	struct invalidate_github_identity_request
	{
		/// Core-owned identity for cleanup even when no session was established.
		request_id                 Request;
		application_session_id     ApplicationSession;
		github_identity_generation NewGeneration;
		github_identity_scope      Previous;

		auto operator<=>(const invalidate_github_identity_request&) const = default;
	};

	/// \brief Describes the establish github session request record.
	struct establish_github_session_request
	{
		github_session_request_context Context;

		auto operator<=>(const establish_github_session_request&) const = default;
	};

	/// \brief Describes the fetch pull request request record.
	struct fetch_pull_request_request
	{
		pull_request_request_context Context;

		auto operator<=>(const fetch_pull_request_request&) const = default;
	};

	using github_host_request = std::variant<
		invalidate_github_identity_request,
		establish_github_session_request,
		fetch_pull_request_request>;

	enum class github_command_id : std::uint8_t {
		activate_host_profile,
		open_pull_request,
		refresh_pull_request,
		show_review_threads,
	};

	enum class github_reason_code : std::uint8_t {
		none,
		invalid_profile,
		session_in_progress,
		session_required,
		invalid_repository,
		invalid_pull_request_number,
		pull_request_load_in_progress,
		no_active_pull_request,
		capability_unknown,
		capability_unsupported,
		stale_completion,
		completion_identity_mismatch,
		authentication_failed,
		permission_denied,
		network_unavailable,
		api_version_unsupported,
		capability_unavailable,
		rate_limited,
		malformed_response,
		response_too_large,
		vault_unavailable,
		certificate_rejected,
		redirect_rejected,
		timed_out,
		cli_unavailable,
		cancelled,
	};

	/// \brief Computes the reason for result.
	[[nodiscard]] auto reason_for(github_failure aFailure) noexcept
		-> github_reason_code;

	/// \brief Describes the github command projection record.
	struct github_command_projection
	{
		github_command_id  Id             = github_command_id::activate_host_profile;
		bool               Enabled        = true;
		github_reason_code DisabledReason = github_reason_code::none;

		auto operator<=>(const github_command_projection&) const = default;
	};

	enum class github_status : std::uint8_t {
		no_profile,
		establishing_session,
		session_ready,
		session_failed,
		session_cancelled,
		loading_pull_request,
		pull_request_ready,
		pull_request_failed,
		pull_request_cancelled,
	};

	/// \brief Describes the github status delta record.
	struct github_status_delta
	{
		github_status Status = github_status::no_profile;

		auto operator<=>(const github_status_delta&) const = default;
	};

	/// \brief Describes the github diagnostic record.
	struct github_diagnostic
	{
		github_reason_code Code = github_reason_code::none;

		auto operator<=>(const github_diagnostic&) const = default;
	};

	/// \brief [[BPR-1200]]. The connection a first read was refused on.
	enum class refused_connection : std::uint8_t {
		/// Public preview: there is no account, so only public pull requests
		/// can be read.
		signed_out,
		/// An account's session.
		account,
	};

	/// \brief [[BPR-1200]]. The first read of a named pull request, refused.
	///
	/// \details Decided with the diagnostic when a read that had no accepted
	/// review behind it fails, so what is said can name what was refused and
	/// the connection that refused it. A failed refresh keeps its accepted
	/// review and is not a refusal of this kind.
	struct pull_request_refusal
	{
		github_reason_code  Reason     = github_reason_code::none;
		refused_connection  Connection = refused_connection::account;
		repository_locator  Repository;
		pull_request_number Number;

		auto operator<=>(const pull_request_refusal&) const = default;
	};

	/// \brief Describes the github identity delta record.
	struct github_identity_delta
	{
		std::optional<host_profile_id>   Profile;
		std::optional<git_host_id>       Host;
		std::optional<github_account_id> Account;
		github_identity_generation       Generation;

		auto operator<=>(const github_identity_delta&) const = default;
	};

	/// \brief Describes the github capability delta record.
	struct github_capability_delta
	{
		github_capabilities Capabilities;
		capability_revision Revision;

		auto operator<=>(const github_capability_delta&) const = default;
	};

	/// \brief Describes the github pull request summary record.
	struct github_pull_request_summary
	{
		repository_locator   Locator;
		pull_request_number  Number;
		repository_id        Repository;
		pull_request_id      PullRequest;
		provider_snapshot_id Snapshot;

		auto operator<=>(const github_pull_request_summary&) const = default;
	};

	/// \brief Describes the pull request summary delta record.
	struct pull_request_summary_delta
	{
		std::optional<github_pull_request_summary> Summary;

		auto operator<=>(const pull_request_summary_delta&) const = default;
	};

	/// \brief Describes the accepted snapshot delta record.
	struct accepted_snapshot_delta
	{
		pull_request_request_context Context;
		repository_id                Repository;
		pull_request_id              PullRequest;
		provider_snapshot_id         Snapshot;

		auto operator<=>(const accepted_snapshot_delta&) const = default;
	};

	/// \brief Describes the accepted session delta record.
	struct accepted_session_delta
	{
		github_session_request_context Context;
		git_host_id                    Host;
		github_account_id              Account;
		github_capabilities            Capabilities;
		capability_revision            CapabilityRevision;

		auto operator<=>(const accepted_session_delta&) const = default;
	};

	/// \brief [[BPR-1357]]. Where one pull request read stands.
	enum class pull_request_read_stage : std::uint8_t {
		/// The read was requested and nothing has answered it yet.
		pending,
		/// The read was accepted.
		loaded,
		/// The read failed; `Reason` says why.
		failed,
		/// The read was cancelled before it finished.
		cancelled,
	};

	/// \brief [[BPR-1357]]. One pull request read, named by its request identity.
	///
	/// \details Decided with every transition that starts or ends a read, so a
	/// surface that asked for a pull request can show that it is being opened in
	/// the same turn as the gesture, and can tell its own read's end from any
	/// other. A refresh carries `Refresh`, because it is not an opening.
	struct pull_request_read_delta
	{
		request_id              Request;
		repository_locator      Repository;
		pull_request_number     Number;
		pull_request_read_stage Stage   = pull_request_read_stage::pending;
		bool                    Refresh = false;
		github_reason_code      Reason  = github_reason_code::none;

		auto operator<=>(const pull_request_read_delta&) const = default;
	};

	enum class github_changed_domain : std::uint16_t {
		none         = 0,
		identity     = 1U << 0U,
		capabilities = 1U << 1U,
		pull_request = 1U << 2U,
		commands     = 1U << 3U,
		status       = 1U << 4U,
		diagnostics  = 1U << 5U,
	};

	[[nodiscard]] constexpr auto operator|(
		const github_changed_domain aLeft,
		const github_changed_domain aRight) noexcept -> github_changed_domain
	{
		return static_cast<github_changed_domain>(
			static_cast<std::uint16_t>(aLeft)
			| static_cast<std::uint16_t>(aRight));
	}

	/// \brief Reports whether the has changed condition holds.
	[[nodiscard]] constexpr auto has_changed(
		const github_changed_domain aValue,
		const github_changed_domain aExpected) noexcept -> bool
	{
		return (
				   static_cast<std::uint16_t>(aValue)
				   & static_cast<std::uint16_t>(aExpected))
			!= 0U;
	}

	/// \brief Describes the github transition output record.
	struct github_transition_output
	{
		github_changed_domain                     Changed = github_changed_domain::none;
		std::vector<github_host_request>          Requests;
		std::vector<github_command_projection>    Commands;
		std::optional<github_status_delta>        Status;
		std::optional<github_identity_delta>      Identity;
		std::optional<github_capability_delta>    Capabilities;
		std::optional<pull_request_summary_delta> PullRequest;
		std::optional<accepted_session_delta>     AcceptedSession;
		std::optional<accepted_snapshot_delta>    AcceptedSnapshot;
		std::vector<github_diagnostic>            Diagnostics;
		std::optional<pull_request_refusal>       Refusal;
		/// [[BPR-1357]]. The read this transition started or ended, if any.
		std::optional<pull_request_read_delta> Read;

		auto operator<=>(const github_transition_output&) const = default;
	};

	/// \brief Describes the github state record.
	struct github_state
	{
		application_session_id                        ApplicationSession;
		request_id                                    NextRequest{ 1 };
		std::optional<host_profile_id>                ActiveProfile;
		github_identity_generation                    IdentityGeneration;
		std::optional<github_session_request_context> PendingSession;
		std::optional<git_host_id>                    Host;
		std::optional<github_account_id>              Account;
		github_capabilities                           Capabilities;
		capability_revision                           CapabilityRevision;
		request_generation                            PullRequestGeneration;
		std::optional<pull_request_request_context>   PendingPullRequest;
		std::optional<repository_locator>             ActiveLocator;
		std::optional<pull_request_number>            ActiveNumber;
		std::optional<repository_id>                  Repository;
		std::optional<pull_request_id>                PullRequest;
		std::optional<provider_snapshot_id>           AcceptedSnapshot;
		// The mode the active profile was activated in. Cleared back to the
		// authenticated mode whenever the identity scope is cleared, so a
		// mode never outlives the profile that asked for it.
		github_access_mode AccessMode{ };

		auto operator<=>(const github_state&) const = default;
	};
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_GITHUB_MODEL_HPP
