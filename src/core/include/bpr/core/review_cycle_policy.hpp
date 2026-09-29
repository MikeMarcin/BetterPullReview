#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_POLICY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_POLICY_HPP

#include "bpr/core/review_cycle.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

/// \file
/// \brief Review Cycles scope and disclosure policy (RFC-BPR-0053, RFC-BPR-0060).
/// \details Capture, authoring, freezing, export, implementation instruction,
/// response append, reviewer verification and publication are separate
/// capabilities. Every decision comes from the local actor, a locally stored
/// grant, the source's classification and the configured channel policy.
/// Nothing in an imported payload, a skill declaration or a provider's claim
/// is an input to the decision.
namespace bpr::core {
	/// \brief Where captured source came from.
	enum class cycle_source_origin : std::uint8_t {
		/// A plain folder or file the reviewer opened.
		local_folder,
		/// A local Git checkout, with or without a remote.
		local_git_checkout,
		/// Material copied out of a provider-authorized cache.
		provider_cache_copy,
	};

	/// \brief How restricted the source is. Ordered from least to most.
	enum class cycle_data_classification : std::uint8_t {
		/// Nothing is known; local review is allowed, external disclosure is
		/// not assumed to be.
		unclassified,
		/// Known public material.
		public_source,
		/// Known enterprise material, such as a GitHub Enterprise Server origin.
		enterprise_restricted,
	};

	/// \brief A workspace's retained origin facts.
	struct cycle_workspace_origin
	{
		cycle_source_origin       Origin                                           = cycle_source_origin::local_folder;
		cycle_data_classification Classification                                   = cycle_data_classification::unclassified;
		auto                      operator<=>(const cycle_workspace_origin&) const = default;
	};

	/// \brief Combines retained and newly observed facts. Classification only
	/// becomes stricter: opening a checkout never washes away a known
	/// enterprise origin, and a provider cache copy stays one.
	[[nodiscard]] auto retain_origin(const cycle_workspace_origin& aRetained, const cycle_workspace_origin& aObserved) noexcept
		-> cycle_workspace_origin;

	/// \brief Where a frozen round's bytes go when exported.
	enum class cycle_disclosure_channel : std::uint8_t {
		clipboard,
		local_file,
		terminal_output,
		/// A permitted external harness reads the round through the local owner.
		external_retrieval,
		/// An installed agent adapter sends the round.
		agent_adapter,
	};
	inline constexpr std::size_t cycle_disclosure_channel_count = 5U;

	enum class cycle_channel_permission : std::uint8_t {
		/// No configuration: allowed only where the default says so.
		unconfigured,
		allowed,
		denied,
	};

	/// \brief Configured channel permissions for one classification.
	struct cycle_channel_policy
	{
		std::array<cycle_channel_permission, cycle_disclosure_channel_count> Channels{ };
		auto                                                                 operator<=>(const cycle_channel_policy&) const = default;
	};
	/// \brief Configured policy per classification. Unconfigured channels fall
	/// back to the defaults in `authorize`.
	struct cycle_disclosure_policy
	{
		cycle_channel_policy Unclassified;
		cycle_channel_policy PublicSource;
		cycle_channel_policy EnterpriseRestricted;
		auto                 operator<=>(const cycle_disclosure_policy&) const = default;
	};

	/// \brief Separate cycle capabilities. There is no execute capability:
	/// review data is never a command.
	enum class cycle_capability : std::uint8_t {
		capture_source,
		author_feedback,
		freeze_round,
		read_round,
		export_round,
		instruct_implementation,
		append_response,
		verify_feedback,
		publish,
	};

	/// \brief Who is asking, as the local host established it.
	enum class cycle_actor : std::uint8_t {
		/// The reviewer through the desktop or an interactive CLI.
		reviewer,
		/// A harness or tool holding a stored local grant.
		granted_consumer,
		/// Content arriving in an imported bundle, response or report.
		imported_content,
	};

	/// \brief A locally stored grant: a nonsecret reference to exactly one
	/// frozen round and a finite capability set.
	struct cycle_round_grant
	{
		local_cycle_subject Subject;
		cycle_round_ref     Round;
		cycle_round_digest  Digest;
		bool                ReadRound                                   = false;
		bool                AppendResponse                              = false;
		bool                Revoked                                     = false;
		auto                operator<=>(const cycle_round_grant&) const = default;
	};
	/// \brief The first external profile: read the ready round and append a
	/// response to it, nothing else.
	[[nodiscard]] auto response_only_grant(const local_cycle_subject& aSubject, const cycle_round& aRound) -> cycle_round_grant;

	struct cycle_authorization_request
	{
		cycle_actor                             Actor      = cycle_actor::imported_content;
		cycle_capability                        Capability = cycle_capability::read_round;
		local_cycle_subject                     Subject;
		cycle_workspace_origin                  Origin;
		std::optional<cycle_round_ref>          Round;
		std::optional<cycle_round_digest>       Digest;
		std::optional<cycle_disclosure_channel> Channel;
		/// A stored grant the host resolved for a granted consumer.
		std::optional<cycle_round_grant> Grant;
		/// The reviewer confirmed this exact publication plan in the
		/// publication flow. Nothing else sets it.
		bool PublicationConfirmed = false;
	};

	enum class cycle_authorization : std::uint8_t {
		allowed,
		/// Subject, round or digest does not match the grant or request.
		wrong_target,
		/// The actor holds no grant for this capability.
		not_granted,
		/// Only the reviewer may do this.
		reviewer_only,
		/// Imported content is data and holds no capability.
		untrusted_content,
		/// An export needs a channel.
		channel_required,
		/// The channel is configured off for this classification.
		channel_denied,
		/// No configuration permits this channel for this classification.
		channel_unconfigured,
		/// Publication needs its own confirmed plan.
		publication_unconfirmed,
		/// The grant was revoked.
		revoked,
	};
	[[nodiscard]] auto name(cycle_authorization aDecision) noexcept -> std::string_view;

	/// \brief Decides one request. The same inputs give the same answer from
	/// the desktop, the CLI or an adapter, whatever format the export uses.
	[[nodiscard]] auto authorize(const cycle_disclosure_policy& aPolicy, const cycle_authorization_request& aRequest) -> cycle_authorization;

	/// \brief Reads a recipient's reported disposition. Only the six claim
	/// words are accepted; words such as "verified" or "approved" are not a
	/// claim, and nothing here can produce a reviewer decision.
	[[nodiscard]] auto parse_imported_claim(std::string_view aText) noexcept -> std::optional<cycle_response_claim>;
}

#endif
