#include "bpr/core/review_cycle_policy.hpp"

#include <algorithm>

namespace bpr::core {
	namespace {
		auto index(const cycle_disclosure_channel aChannel) noexcept -> std::size_t
		{
			return static_cast<std::size_t>(aChannel);
		}

		/// Local operating-system destinations the reviewer picks are allowed
		/// unless configured off; channels that reach another program need a
		/// configured allowance unless the source is known to be public.
		auto default_permission(const cycle_data_classification aClassification, const cycle_disclosure_channel aChannel) noexcept
			-> cycle_channel_permission
		{
			if (aClassification == cycle_data_classification::public_source) {
				return cycle_channel_permission::allowed;
			}
			switch (aChannel) {
			case cycle_disclosure_channel::clipboard:
			case cycle_disclosure_channel::local_file:
			case cycle_disclosure_channel::terminal_output:
				return cycle_channel_permission::allowed;
			case cycle_disclosure_channel::external_retrieval:
			case cycle_disclosure_channel::agent_adapter:
				return cycle_channel_permission::unconfigured;
			}
			return cycle_channel_permission::unconfigured;
		}

		auto configured(const cycle_disclosure_policy& aPolicy, const cycle_data_classification aClassification) noexcept
			-> const cycle_channel_policy&
		{
			switch (aClassification) {
			case cycle_data_classification::unclassified:
				return aPolicy.Unclassified;
			case cycle_data_classification::public_source:
				return aPolicy.PublicSource;
			case cycle_data_classification::enterprise_restricted:
				return aPolicy.EnterpriseRestricted;
			}
			return aPolicy.EnterpriseRestricted;
		}

		auto channel_decision(const cycle_disclosure_policy& aPolicy, const cycle_data_classification aClassification,
							  const cycle_disclosure_channel aChannel) -> cycle_authorization
		{
			if (index(aChannel) >= cycle_disclosure_channel_count) {
				return cycle_authorization::channel_denied;
			}
			auto permission = configured(aPolicy, aClassification).Channels.at(index(aChannel));
			if (permission == cycle_channel_permission::unconfigured) {
				permission = default_permission(aClassification, aChannel);
			}
			switch (permission) {
			case cycle_channel_permission::allowed:
				return cycle_authorization::allowed;
			case cycle_channel_permission::denied:
				return cycle_authorization::channel_denied;
			case cycle_channel_permission::unconfigured:
				return cycle_authorization::channel_unconfigured;
			}
			return cycle_authorization::channel_denied;
		}

		auto names_round(const cycle_authorization_request& aRequest) -> bool
		{
			return aRequest.Round && aRequest.Digest && belongs_to(*aRequest.Round, aRequest.Subject) && aRequest.Digest->valid();
		}

		auto reviewer_decision(const cycle_disclosure_policy& aPolicy, const cycle_authorization_request& aRequest) -> cycle_authorization
		{
			switch (aRequest.Capability) {
			case cycle_capability::capture_source:
			case cycle_capability::author_feedback:
			case cycle_capability::freeze_round:
			case cycle_capability::read_round:
			case cycle_capability::append_response:
			case cycle_capability::verify_feedback:
				return cycle_authorization::allowed;
			case cycle_capability::instruct_implementation:
				return names_round(aRequest) ? cycle_authorization::allowed : cycle_authorization::wrong_target;
			case cycle_capability::export_round:
				if (!names_round(aRequest)) {
					return cycle_authorization::wrong_target;
				}
				if (!aRequest.Channel) {
					return cycle_authorization::channel_required;
				}
				return channel_decision(aPolicy, aRequest.Origin.Classification, *aRequest.Channel);
			case cycle_capability::publish:
				return aRequest.PublicationConfirmed ? cycle_authorization::allowed : cycle_authorization::publication_unconfirmed;
			}
			return cycle_authorization::reviewer_only;
		}

		auto consumer_decision(const cycle_disclosure_policy& aPolicy, const cycle_authorization_request& aRequest) -> cycle_authorization
		{
			if (!aRequest.Grant) {
				return cycle_authorization::not_granted;
			}
			const auto& grant = *aRequest.Grant;
			if (grant.Subject != aRequest.Subject || !belongs_to(grant.Round, grant.Subject) || !grant.Digest.valid()
				|| aRequest.Round != grant.Round || aRequest.Digest != grant.Digest) {
				return cycle_authorization::wrong_target;
			}
			if (grant.Revoked) {
				return cycle_authorization::revoked;
			}
			switch (aRequest.Capability) {
			case cycle_capability::read_round: {
				if (!grant.ReadRound) {
					return cycle_authorization::not_granted;
				}
				const auto channel = aRequest.Channel.value_or(cycle_disclosure_channel::external_retrieval);
				if (channel != cycle_disclosure_channel::external_retrieval && channel != cycle_disclosure_channel::agent_adapter) {
					return cycle_authorization::reviewer_only;
				}
				return channel_decision(aPolicy, aRequest.Origin.Classification, channel);
			}
			case cycle_capability::append_response:
				return grant.AppendResponse ? cycle_authorization::allowed : cycle_authorization::not_granted;
			case cycle_capability::capture_source:
			case cycle_capability::author_feedback:
			case cycle_capability::freeze_round:
			case cycle_capability::export_round:
			case cycle_capability::instruct_implementation:
			case cycle_capability::verify_feedback:
			case cycle_capability::publish:
				return cycle_authorization::reviewer_only;
			}
			return cycle_authorization::reviewer_only;
		}
	}

	auto retain_origin(const cycle_workspace_origin& aRetained, const cycle_workspace_origin& aObserved) noexcept
		-> cycle_workspace_origin
	{
		const auto cacheCopy = aRetained.Origin == cycle_source_origin::provider_cache_copy
			|| aObserved.Origin == cycle_source_origin::provider_cache_copy;
		return {
			.Origin         = cacheCopy ? cycle_source_origin::provider_cache_copy : aObserved.Origin,
			.Classification = std::max(aRetained.Classification, aObserved.Classification),
		};
	}

	auto response_only_grant(const local_cycle_subject& aSubject, const cycle_round& aRound) -> cycle_round_grant
	{
		return { .Subject = aSubject, .Round = aRound.Id, .Digest = aRound.Digest, .ReadRound = true, .AppendResponse = true };
	}

	auto authorize(const cycle_disclosure_policy& aPolicy, const cycle_authorization_request& aRequest) -> cycle_authorization
	{
		if (aRequest.Actor == cycle_actor::imported_content) {
			return cycle_authorization::untrusted_content;
		}
		if (!valid(aRequest.Subject)) {
			return cycle_authorization::wrong_target;
		}
		switch (aRequest.Actor) {
		case cycle_actor::reviewer:
			return reviewer_decision(aPolicy, aRequest);
		case cycle_actor::granted_consumer:
			return consumer_decision(aPolicy, aRequest);
		case cycle_actor::imported_content:
			break;
		}
		return cycle_authorization::untrusted_content;
	}

	auto name(const cycle_authorization aDecision) noexcept -> std::string_view
	{
		switch (aDecision) {
		case cycle_authorization::allowed:
			return "allowed";
		case cycle_authorization::wrong_target:
			return "wrong_target";
		case cycle_authorization::not_granted:
			return "not_granted";
		case cycle_authorization::reviewer_only:
			return "reviewer_only";
		case cycle_authorization::untrusted_content:
			return "untrusted_content";
		case cycle_authorization::channel_required:
			return "channel_required";
		case cycle_authorization::channel_denied:
			return "channel_denied";
		case cycle_authorization::channel_unconfigured:
			return "channel_unconfigured";
		case cycle_authorization::publication_unconfirmed:
			return "publication_unconfirmed";
		case cycle_authorization::revoked:
			return "revoked";
		}
		return "unknown";
	}

	auto parse_imported_claim(const std::string_view aText) noexcept -> std::optional<cycle_response_claim>
	{
		constexpr auto claims = std::array<std::pair<std::string_view, cycle_response_claim>, 6>{ {
			{ "addressed", cycle_response_claim::addressed },
			{ "partially_addressed", cycle_response_claim::partially_addressed },
			{ "disagreed", cycle_response_claim::disagreed },
			{ "blocked", cycle_response_claim::blocked },
			{ "question", cycle_response_claim::question },
			{ "not_attempted", cycle_response_claim::not_attempted },
		} };
		const auto     found  = std::ranges::find(claims, aText, &std::pair<std::string_view, cycle_response_claim>::first);
		return found == claims.end() ? std::nullopt : std::optional{ found->second };
	}
}
