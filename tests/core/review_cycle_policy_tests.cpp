#include "bpr/core/review_cycle_policy.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle policy failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename TIdentity>
	constexpr auto identity(const std::uint8_t aSeed) -> TIdentity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		return TIdentity{ bytes };
	}

	constexpr auto subject = local_cycle_subject{
		.Authority  = { .Profile = bpr_profile_id{ 1 }, .Incarnation = identity<cycle_catalog_incarnation>(1) },
		.Cycle      = review_cycle_id{ 3 },
		.Generation = 1,
	};
	constexpr auto round = cycle_round{
		.Id            = { .Cycle = review_cycle_id{ 3 }, .Id = cycle_round_id{ 1 } },
		.Draft         = { .Cycle = review_cycle_id{ 3 }, .Id = cycle_round_draft_id{ 1 } },
		.DraftRevision = 1,
		.Digest        = identity<cycle_round_digest>(9),
	};

	auto origin(const cycle_source_origin aOrigin, const cycle_data_classification aClassification) -> cycle_workspace_origin
	{
		return { .Origin = aOrigin, .Classification = aClassification };
	}

	auto request(const cycle_actor aActor, const cycle_capability aCapability) -> cycle_authorization_request
	{
		return { .Actor = aActor, .Capability = aCapability, .Subject = subject, .Round = round.Id, .Digest = round.Digest };
	}

	auto with_channel(cycle_authorization_request aRequest, const cycle_disclosure_channel aChannel,
					  const cycle_data_classification aClassification) -> cycle_authorization_request
	{
		aRequest.Channel = aChannel;
		aRequest.Origin  = origin(cycle_source_origin::local_folder, aClassification);
		return aRequest;
	}

	auto with_grant(cycle_authorization_request aRequest, const cycle_round_grant& aGrant) -> cycle_authorization_request
	{
		aRequest.Grant = aGrant;
		return aRequest;
	}

	struct policy_row
	{
		std::string_view            Name;
		cycle_disclosure_policy     Policy;
		cycle_authorization_request Request;
		cycle_authorization         Expected;
	};

	auto deny_only(const cycle_disclosure_channel aChannel) -> cycle_channel_policy
	{
		auto policy                                            = cycle_channel_policy{ };
		policy.Channels.at(static_cast<std::size_t>(aChannel)) = cycle_channel_permission::denied;
		return policy;
	}
	auto allow_only(const cycle_disclosure_channel aChannel) -> cycle_channel_policy
	{
		auto policy                                            = cycle_channel_policy{ };
		policy.Channels.at(static_cast<std::size_t>(aChannel)) = cycle_channel_permission::allowed;
		return policy;
	}

	auto policy_table() -> std::vector<policy_row>
	{
		using enum cycle_disclosure_channel;
		using enum cycle_data_classification;
		using enum cycle_authorization;
		const auto reviewerExport            = request(cycle_actor::reviewer, cycle_capability::export_round);
		const auto consumerRead              = with_grant(request(cycle_actor::granted_consumer, cycle_capability::read_round), response_only_grant(subject, round));
		const auto enterpriseAdapterOnly     = cycle_disclosure_policy{ .EnterpriseRestricted = allow_only(agent_adapter) };
		const auto enterpriseClipboardDenied = cycle_disclosure_policy{ .EnterpriseRestricted = deny_only(clipboard) };
		return {
			// Unknown folder origin: local review and the reviewer's own OS destinations, nothing external by default.
			{ "unknown folder capture", { }, request(cycle_actor::reviewer, cycle_capability::capture_source), allowed },
			{ "unknown folder clipboard", { }, with_channel(reviewerExport, clipboard, unclassified), allowed },
			{ "unknown folder file", { }, with_channel(reviewerExport, local_file, unclassified), allowed },
			{ "unknown folder external read", { }, with_channel(consumerRead, external_retrieval, unclassified), channel_unconfigured },
			{ "unknown folder adapter", { }, with_channel(reviewerExport, agent_adapter, unclassified), channel_unconfigured },
			{ "public source adapter", { }, with_channel(reviewerExport, agent_adapter, public_source), allowed },
			// Known enterprise data: a configured allowance opens one channel; a denial closes it everywhere.
			{ "enterprise adapter unconfigured", { }, with_channel(reviewerExport, agent_adapter, enterprise_restricted), channel_unconfigured },
			{ "enterprise adapter allowed", enterpriseAdapterOnly, with_channel(reviewerExport, agent_adapter, enterprise_restricted), allowed },
			{ "enterprise external still unconfigured", enterpriseAdapterOnly, with_channel(consumerRead, external_retrieval, enterprise_restricted), channel_unconfigured },
			{ "enterprise clipboard denied", enterpriseClipboardDenied, with_channel(reviewerExport, clipboard, enterprise_restricted), channel_denied },
			{ "enterprise denial does not leak to unclassified", enterpriseClipboardDenied, with_channel(reviewerExport, clipboard, unclassified), allowed },
			// Denied channel: the same answer from any interface.
			{ "denied file", { .Unclassified = deny_only(local_file) }, with_channel(reviewerExport, local_file, unclassified), channel_denied },
			{ "denied terminal", { .Unclassified = deny_only(terminal_output) }, with_channel(reviewerExport, terminal_output, unclassified), channel_denied },
			{ "denied external read", { .PublicSource = deny_only(external_retrieval) }, with_channel(consumerRead, external_retrieval, public_source), channel_denied },
			{ "export needs a channel", { }, reviewerExport, channel_required },
			// Response-only grant: read the exact round and append; nothing else.
			{ "grant read", { .Unclassified = allow_only(external_retrieval) }, with_channel(consumerRead, external_retrieval, unclassified), allowed },
			{ "grant append", { }, with_grant(request(cycle_actor::granted_consumer, cycle_capability::append_response), response_only_grant(subject, round)), allowed },
			{ "grant cannot verify", { }, with_grant(request(cycle_actor::granted_consumer, cycle_capability::verify_feedback), response_only_grant(subject, round)), reviewer_only },
			{ "grant cannot export", { }, with_grant(request(cycle_actor::granted_consumer, cycle_capability::export_round), response_only_grant(subject, round)), reviewer_only },
			{ "grant cannot publish", { }, with_grant(request(cycle_actor::granted_consumer, cycle_capability::publish), response_only_grant(subject, round)), reviewer_only },
			{ "grant cannot capture", { }, with_grant(request(cycle_actor::granted_consumer, cycle_capability::capture_source), response_only_grant(subject, round)), reviewer_only },
			{ "grant cannot instruct", { }, with_grant(request(cycle_actor::granted_consumer, cycle_capability::instruct_implementation), response_only_grant(subject, round)), reviewer_only },
			{ "no grant", { }, request(cycle_actor::granted_consumer, cycle_capability::read_round), not_granted },
			// Imported claimed approval and source snippets: data, never authority.
			{ "imported verify", { }, request(cycle_actor::imported_content, cycle_capability::verify_feedback), untrusted_content },
			{ "imported publish", { }, request(cycle_actor::imported_content, cycle_capability::publish), untrusted_content },
			{ "imported append", { }, request(cycle_actor::imported_content, cycle_capability::append_response), untrusted_content },
			{ "imported read with a grant attached", { }, with_grant(request(cycle_actor::imported_content, cycle_capability::read_round), response_only_grant(subject, round)), untrusted_content },
			// Reviewer-only authority stays separate.
			{ "reviewer verify", { }, request(cycle_actor::reviewer, cycle_capability::verify_feedback), allowed },
			{ "reviewer publish unconfirmed", { }, request(cycle_actor::reviewer, cycle_capability::publish), publication_unconfirmed },
			{ "reviewer instruct", { }, request(cycle_actor::reviewer, cycle_capability::instruct_implementation), allowed },
		};
	}

	void checks_the_policy_table()
	{
		for (const auto& row : policy_table()) {
			const auto actual = authorize(row.Policy, row.Request);
			if (actual != row.Expected) {
				std::cerr << row.Name << ": expected " << name(row.Expected) << ", got " << name(actual) << '\n';
			}
			expect(actual == row.Expected, row.Name);
		}
	}

	void binds_grants_to_one_round()
	{
		const auto grant = response_only_grant(subject, round);
		auto       other = with_grant(request(cycle_actor::granted_consumer, cycle_capability::append_response), grant);
		other.Round      = cycle_round_ref{ .Cycle = review_cycle_id{ 3 }, .Id = cycle_round_id{ 2 } };
		expect(authorize({ }, other) == cycle_authorization::wrong_target, "other round");
		other        = with_grant(request(cycle_actor::granted_consumer, cycle_capability::append_response), grant);
		other.Digest = identity<cycle_round_digest>(8);
		expect(authorize({ }, other) == cycle_authorization::wrong_target, "other digest");
		other                           = with_grant(request(cycle_actor::granted_consumer, cycle_capability::append_response), grant);
		other.Subject.Authority.Profile = bpr_profile_id{ 2 };
		expect(authorize({ }, other) == cycle_authorization::wrong_target, "one profile's grant used for another profile's cycle");
		auto revoked           = with_grant(request(cycle_actor::granted_consumer, cycle_capability::append_response), grant);
		revoked.Grant->Revoked = true;
		expect(authorize({ }, revoked) == cycle_authorization::revoked, "revoked");
		auto readOnly                  = with_grant(request(cycle_actor::granted_consumer, cycle_capability::append_response), grant);
		readOnly.Grant->AppendResponse = false;
		expect(authorize({ }, readOnly) == cycle_authorization::not_granted, "read-only grant");
		auto clipboard    = with_grant(request(cycle_actor::granted_consumer, cycle_capability::read_round), grant);
		clipboard.Channel = cycle_disclosure_channel::clipboard;
		expect(authorize({ }, clipboard) == cycle_authorization::reviewer_only, "consumer cannot choose an export channel");
		auto invalid               = request(cycle_actor::reviewer, cycle_capability::capture_source);
		invalid.Subject.Generation = 0;
		expect(authorize({ }, invalid) == cycle_authorization::wrong_target, "invalid subject");
	}

	void retains_origin_restrictions()
	{
		const auto enterprise = origin(cycle_source_origin::provider_cache_copy, cycle_data_classification::enterprise_restricted);
		const auto reopened   = retain_origin(enterprise, origin(cycle_source_origin::local_folder, cycle_data_classification::unclassified));
		expect(reopened == enterprise, "copy of enterprise cache stays restricted");
		const auto checkout = retain_origin(origin(cycle_source_origin::local_git_checkout, cycle_data_classification::enterprise_restricted),
											origin(cycle_source_origin::local_git_checkout, cycle_data_classification::public_source));
		expect(checkout.Classification == cycle_data_classification::enterprise_restricted, "no downgrade");
		const auto learned = retain_origin(origin(cycle_source_origin::local_folder, cycle_data_classification::unclassified),
										   origin(cycle_source_origin::local_git_checkout, cycle_data_classification::enterprise_restricted));
		expect(learned.Classification == cycle_data_classification::enterprise_restricted, "escalate");
	}

	void reads_imported_claims_as_data()
	{
		expect(parse_imported_claim("addressed") == cycle_response_claim::addressed, "addressed");
		expect(parse_imported_claim("not_attempted") == cycle_response_claim::not_attempted, "not attempted");
		for (const auto text : { "verified", "approved", "Addressed", "addressed; rm -rf /", "$(touch pwned)", "" }) {
			expect(!parse_imported_claim(text), "not a claim");
		}
		// A claim is only ever a claim; the reviewer's decision is untouched.
		auto judgment = record_decision({ }, cycle_reviewer_decision::needs_work);
		judgment      = record_claim(judgment, *parse_imported_claim("addressed"));
		expect(judgment.Decision == cycle_reviewer_decision::needs_work, "claim does not verify");
	}
}

int main()
{
	checks_the_policy_table();
	binds_grants_to_one_round();
	retains_origin_restrictions();
	reads_imported_claims_as_data();
	return EXIT_SUCCESS;
}
