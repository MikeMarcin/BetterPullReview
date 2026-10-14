#include "bpr/core/diff_transition.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <variant>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed) -> git_blob_oid
	{
		std::array<std::uint8_t, 32U> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return git_blob_oid{ git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto key(const std::uint8_t            aFile      = 1U,
						   const diff_algorithm_revision aAlgorithm = supported_diff_algorithm_revision)
		-> file_comparison_key
	{
		const auto options = diff_options{ };
		const auto digest  = options_digest(options);
		expect(digest.has_value(), "default options digest");
		const auto scope       = opaque<pull_request_authority_scope_id>(2U);
		const auto incarnation = opaque<epoch_catalog_incarnation_id>(3U);
		const auto authority   = pull_request_authority{
			.Profile         = host_profile_id{ 1U },
			.ProfileRevision = github_profile_revision{ 1U },
			.Host            = git_host_id{ 1U },
			.HostAuthority   = opaque<host_authority_fingerprint>(4U),
			.Account         = github_account_id{ 1U },
			.Viewer          = github_viewer_actor_id{ 1U },
			.Repository      = repository_id{ 1U },
			.Cache           = opaque<repository_cache_scope_id>(5U),
			.PullRequest     = pull_request_id{ 1U },
			.Number          = pull_request_number{ 1U },
			.Scope           = scope,
		};
		return file_comparison_key{
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = comparison_anchor_identity{
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = comparison_anchor_kind::epoch_base,
				.Epoch       = epoch_ordinal{ 1 },
			},
			.RightAnchor = comparison_anchor_identity{
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = comparison_anchor_kind::epoch_head,
				.Epoch       = epoch_ordinal{ 2 },
			},
			.LeftBlob  = blob(10U),
			.RightBlob = blob(40U),
			.File      = opaque<file_identity>(aFile),
			.Options   = *digest,
			.Algorithm = aAlgorithm,
		};
	}

	[[nodiscard]] auto state() -> diff_state
	{
		return diff_state{ .ApplicationSession = application_session_id{ 7U } };
	}

	auto test_exact_generation_and_acceptance() -> void
	{
		auto       value = state();
		const auto first = transition(
			value, request_file_diff_intent{ .Key = key(), .Options = { } });
		expect(first.Requests.size() == 1U && value.Pending.has_value(),
			   "first request becomes exact pending context");
		const auto firstRequest = std::get<compute_file_diff_request>(first.Requests.front());
		expect(firstRequest.Context.Request == request_id{ 1U }
				   && firstRequest.Context.Generation == request_generation{ 1U }
				   && valid(firstRequest.Context),
			   "request and generation are nonzero and monotonic");

		const auto second = transition(
			value, request_file_diff_intent{ .Key = key(2U), .Options = { } });
		expect(second.Requests.size() == 2U
				   && std::holds_alternative<cancel_file_diff_request>(
					   second.Requests.front()),
			   "supersession cancels the exact earlier context");
		const auto secondContext = std::get<compute_file_diff_request>(second.Requests.back()).Context;
		expect(secondContext.Request == request_id{ 2U }
				   && secondContext.Generation == request_generation{ 2U },
			   "supersession advances both identities");

		const auto stale = transition(
			value, file_diff_computed_event{ firstRequest.Context });
		expect(stale.Reason == diff_transition_reason::stale_completion
				   && !stale.Accepted.has_value() && value.Pending.has_value(),
			   "reordered old completion is rejected without state change");
		const auto accepted = transition(
			value, file_diff_computed_event{ secondContext });
		expect(accepted.Accepted.has_value() && accepted.Terminal.has_value()
				   && value.Current == secondContext && !value.Pending,
			   "only the exact current completion is accepted");
		const auto duplicate = transition(
			value, file_diff_computed_event{ secondContext });
		expect(duplicate.Reason == diff_transition_reason::stale_completion,
			   "duplicate completion is stale");
	}

	auto test_failure_cancel_and_invalidation() -> void
	{
		auto value  = state();
		auto output = transition(
			value, request_file_diff_intent{ .Key = key(), .Options = { } });
		const auto context = std::get<compute_file_diff_request>(output.Requests.back()).Context;
		output             = transition(value,
										file_diff_failed_event{
											.Context = context,
											.Reason  = diff_reason_code::decode_failure,
										});
		expect(output.Terminal.has_value()
				   && output.Terminal->Reason == diff_reason_code::decode_failure
				   && value.LastReason == diff_reason_code::decode_failure,
			   "matching typed failure terminates without replacing current");

		output = transition(
			value, request_file_diff_intent{ .Key = key(), .Options = { } });
		output = transition(value, cancel_file_diff_intent{ });
		expect(output.Requests.size() == 1U && output.Terminal.has_value()
				   && output.Terminal->Reason == diff_reason_code::cancelled
				   && !value.Pending,
			   "explicit cancellation revokes pending context");

		output = transition(
			value, request_file_diff_intent{ .Key = key(), .Options = { } });
		const auto authority = key().Authority;
		output               = transition(
			value, invalidate_diff_authority_intent{ .Authority = authority });
		expect(output.Requests.size() == 1U && !value.Pending,
			   "authority invalidation cancels matching pending work");
	}

	auto test_exact_epoch_invalidation() -> void
	{
		const auto original = key();
		const auto epoch    = review_epoch_id{ original.LeftAnchor.Authority,
											   original.LeftAnchor.Incarnation, original.LeftAnchor.Epoch };
		expect(uses_epoch(original, epoch), "left retained epoch matches");
		const auto right = review_epoch_id{ original.RightAnchor.Authority,
											original.RightAnchor.Incarnation, original.RightAnchor.Epoch };
		expect(uses_epoch(original, right), "right retained epoch matches");
		for (auto changed : { 0, 1, 2 }) {
			auto other = epoch;
			if (changed == 0) {
				other.Authority = opaque<pull_request_authority_scope_id>(90U);
			}
			if (changed == 1) {
				other.Incarnation = opaque<epoch_catalog_incarnation_id>(90U);
			}
			if (changed == 2) {
				other.Ordinal = epoch_ordinal{ 90U };
			}
			expect(!uses_epoch(original, other), "full epoch identity isolates prune");
		}
		auto review            = original;
		review.LeftAnchor.Kind = comparison_anchor_kind::review_commit;
		expect(!uses_epoch(review, epoch), "review anchor is not an epoch anchor");
		auto other              = original;
		other.LeftAnchor.Epoch  = epoch_ordinal{ 3U };
		other.RightAnchor.Epoch = epoch_ordinal{ 4U };
		auto       value        = state();
		auto       output       = transition(value, request_file_diff_intent{ original, { } });
		const auto old          = *value.Pending;
		output                  = transition(value, file_diff_computed_event{ old });
		output                  = transition(value, request_file_diff_intent{ other, { } });
		const auto pending      = *value.Pending;
		output                  = transition(value, invalidate_diff_epoch_intent{ epoch });
		expect(!value.Current && value.Pending == pending && output.Requests.empty(),
			   "pruning current epoch preserves a newer unrelated pending selection");
		output = transition(value, file_diff_computed_event{ pending });
		expect(output.Accepted.has_value(), "unrelated pending result still accepts");
		output               = transition(value, request_file_diff_intent{ original, { } });
		const auto cancelled = *value.Pending;
		output               = transition(value, invalidate_diff_epoch_intent{ epoch });
		expect(!value.Pending && value.Current == pending && output.Requests.size() == 1U
				   && std::get<cancel_file_diff_request>(output.Requests.front()).Context == cancelled,
			   "pruning pending work cancels only its exact request");
		output = transition(value, file_diff_computed_event{ cancelled });
		expect(output.Reason == diff_transition_reason::stale_completion && value.Current == pending,
			   "late pruned completion cannot replace an unrelated current result");
	}

	auto test_invalid_and_unsupported_requests() -> void
	{
		// [[BPR-1497]]. Revision 1 is the first-party engine linked xdiff
		// replaced; its keys must be recomputed, never served.
		auto value       = state();
		auto unsupported = key(1U, diff_algorithm_revision{ 1U });
		auto output      = transition(
			value,
			request_file_diff_intent{ .Key = unsupported, .Options = { } });
		expect(output.Reason == diff_transition_reason::unsupported_algorithm
				   && output.Requests.empty(),
			   "caller cannot select an unstamped algorithm revision");

		auto mismatched      = key();
		auto options         = diff_options{ };
		options.ContextLines = 9U;
		output               = transition(
			value,
			request_file_diff_intent{ .Key = mismatched, .Options = options });
		expect(output.Reason == diff_transition_reason::invalid_request
				   && output.Requests.empty(),
			   "options must match the exact key digest");

		output = transition(value,
							file_diff_failed_event{
								.Context = { },
								.Reason  = diff_reason_code::none,
							});
		expect(output.Reason == diff_transition_reason::invalid_failure,
			   "success cannot enter through the failure path");

		for (const auto unsupportedReplacement : { false, true }) {
			auto       replacementState = state();
			const auto pending          = transition(
				replacementState,
				request_file_diff_intent{ .Key = key(), .Options = { } });
			const auto pendingContext     = std::get<compute_file_diff_request>(pending.Requests.back())
												.Context;
			auto       replacementKey     = key();
			auto       replacementOptions = diff_options{ };
			if (unsupportedReplacement) {
				replacementKey.Algorithm = diff_algorithm_revision{ 1U };
			} else {
				replacementOptions.ContextLines = 9U;
			}
			output = transition(
				replacementState,
				request_file_diff_intent{
					.Key     = replacementKey,
					.Options = replacementOptions,
				});
			expect(output.Requests.size() == 1U
					   && std::get<cancel_file_diff_request>(
							  output.Requests.front())
							   .Context
						   == pendingContext
					   && !replacementState.Pending.has_value(),
				   "a newer invalid request revokes older pending work");
			const auto stale = transition(
				replacementState, file_diff_computed_event{ pendingContext });
			expect(stale.Reason == diff_transition_reason::stale_completion,
				   "revoked work cannot publish after malformed replacement");
		}
	}
} // namespace

int main()
{
	test_exact_generation_and_acceptance();
	test_failure_cancel_and_invalidation();
	test_exact_epoch_invalidation();
	test_invalid_and_unsupported_requests();
	return EXIT_SUCCESS;
}
