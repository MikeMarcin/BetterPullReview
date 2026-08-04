#include "bpr/application/review_mutation_coordinator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

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
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		std::fill_n(bytes.begin(), 20U, aSeed);
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto target(const std::uint8_t aHead) -> core::review_mutation_target
	{
		return core::review_mutation_target{
			.Host         = core::git_host_id{ 1U },
			.Repository   = core::repository_id{ 2U },
			.PullRequest  = core::pull_request_id{ 3U },
			.ExpectedHead = commit(aHead),
		};
	}

	[[nodiscard]] auto proposal() -> core::review_publication_proposal
	{
		return core::review_publication_proposal{
			.Draft  = core::review_draft_id{ 7U },
			.Anchor = core::source_line_ref{
				.Comparison = opaque<core::file_comparison_digest>(5U),
				.Side       = core::diff_side::right,
				.Line       = 12U },
			.Body        = "Please rename this.",
			.Disposition = core::review_disposition::comment,
		};
	}

	/// A provider that records what it was asked, recognises repeated
	/// idempotency keys, and can change the target it reports between one
	/// observation and the next.
	class fake_provider final : public application::review_mutation_port
	{
	public:
		explicit fake_provider(core::review_mutation_target aReported)
			: Reported{ aReported }
		{
		}

		auto observe_target() -> std::expected<
			core::review_mutation_target,
			application::review_mutation_failure> override
		{
			++Observations;
			if (Observations == SwapOnObservation) {
				Reported = Swapped;
			}
			if (ObserveFailure) {
				return std::unexpected(*ObserveFailure);
			}
			return Reported;
		}

		auto submit(const application::review_mutation_request& aRequest)
			-> std::expected<
				application::review_mutation_receipt,
				application::review_mutation_failure> override
		{
			Submitted.push_back(aRequest);
			if (SubmitFailure) {
				return std::unexpected(*SubmitFailure);
			}
			const auto known = Seen.find(aRequest.IdempotencyKey);
			if (known != Seen.end()) {
				return application::review_mutation_receipt{
					.ServerCommentId = known->second,
					.Duplicate       = true,
				};
			}
			auto identifier
				= "comment-" + std::to_string(Seen.size() + 1U);
			Seen.emplace(aRequest.IdempotencyKey, identifier);
			return application::review_mutation_receipt{
				.ServerCommentId = std::move(identifier),
				.Duplicate       = false,
			};
		}

		[[nodiscard]] auto capabilities() const
			-> application::review_provider_capabilities override
		{
			return Capabilities;
		}

		/// Reports a different target from the given observation onward.
		void swap_target_at(
			const int                          aObservation,
			const core::review_mutation_target aReplacement)
		{
			SwapOnObservation = aObservation;
			Swapped           = aReplacement;
		}

		core::review_mutation_target                        Reported;
		core::review_mutation_target                        Swapped;
		int                                                 SwapOnObservation = -1;
		int                                                 Observations      = 0;
		std::vector<application::review_mutation_request>   Submitted;
		std::map<std::string, std::string>                  Seen;
		application::review_provider_capabilities           Capabilities;
		std::optional<application::review_mutation_failure> ObserveFailure;
		std::optional<application::review_mutation_failure> SubmitFailure;
	};

	[[nodiscard]] auto coordinator_for(fake_provider& aProvider)
		-> application::review_mutation_coordinator
	{
		return application::review_mutation_coordinator{
			aProvider,
			target(0x11U),
			core::github_account_id{ 4U },
		};
	}

	void test_a_matching_target_publishes_under_the_acting_identity()
	{
		auto provider    = fake_provider{ target(0x11U) };
		auto coordinator = coordinator_for(provider);

		const auto receipt = coordinator.publish(proposal(), false);
		expect(receipt.has_value(), "a matching target publishes");
		expect(
			provider.Submitted.size() == 1U,
			"exactly one submission reached the provider");
		expect(
			provider.Submitted.front().ActingIdentity
				== core::github_account_id{ 4U },
			"the request carries the acting identity");
		expect(
			provider.Submitted.front().Target == coordinator.designated(),
			"the request carries the designated target, not one the caller "
			"supplied");
		expect(
			!provider.Submitted.front().IdempotencyKey.empty(),
			"the request carries an idempotency key");
	}

	// The window this closes: a check performed only at prepare time passes for
	// a target that changes afterwards.
	void test_a_target_altered_after_the_prepare_check_is_refused()
	{
		auto provider = fake_provider{ target(0x11U) };
		// The first observation matches; the second reports another repository.
		auto elsewhere       = target(0x11U);
		elsewhere.Repository = core::repository_id{ 99U };
		provider.swap_target_at(2, elsewhere);

		auto       coordinator = coordinator_for(provider);
		const auto refused     = coordinator.publish(proposal(), false);

		expect(
			!refused.has_value()
				&& refused.error()
					== application::review_mutation_failure::undesignated_target,
			"a target altered after the prepare check is refused");
		expect(
			provider.Submitted.empty(),
			"nothing was submitted");
		expect(
			coordinator.statistics().TargetObservations == 2U
				&& coordinator.statistics().Submissions == 0U,
			"the coordinator observed twice and submitted nothing");
	}

	void test_no_acknowledgement_authorises_another_pull_request()
	{
		auto elsewhere        = target(0x11U);
		elsewhere.PullRequest = core::pull_request_id{ 99U };
		auto provider         = fake_provider{ elsewhere };
		auto coordinator      = coordinator_for(provider);

		for (const auto acknowledged : { false, true }) {
			const auto refused = coordinator.publish(proposal(), acknowledged);
			expect(
				!refused.has_value()
					&& refused.error()
						== application::review_mutation_failure::undesignated_target,
				"acknowledging does not authorise a different pull request");
		}
		expect(provider.Submitted.empty(), "nothing was submitted either way");
	}

	void test_a_moved_head_needs_an_acknowledgement()
	{
		auto provider    = fake_provider{ target(0x22U) };
		auto coordinator = coordinator_for(provider);

		const auto refused = coordinator.publish(proposal(), false);
		expect(
			!refused.has_value()
				&& refused.error() == application::review_mutation_failure::stale_head,
			"a moved head is refused as stale rather than as a wrong target");
		expect(provider.Submitted.empty(), "nothing was submitted");

		const auto accepted = coordinator.publish(proposal(), true);
		expect(
			accepted.has_value() && provider.Submitted.size() == 1U,
			"an acknowledged moved head publishes");
	}

	// A request whose answer was lost is indistinguishable from one that never
	// arrived. The key is what makes retrying safe.
	void test_a_retry_does_not_create_a_second_comment()
	{
		auto provider    = fake_provider{ target(0x11U) };
		auto coordinator = coordinator_for(provider);

		const auto first = coordinator.publish(proposal(), false);
		expect(
			first.has_value() && !first->Duplicate,
			"the first publication is not a duplicate");

		const auto retry = coordinator.publish(proposal(), false);
		expect(retry.has_value(), "the retry succeeds");
		expect(
			retry->Duplicate && retry->ServerCommentId == first->ServerCommentId,
			"the retry is recognised and returns the original comment");
		expect(
			provider.Seen.size() == 1U,
			"the provider holds one comment, not two");
		expect(
			coordinator.statistics().Duplicates == 1U,
			"the duplicate is counted");

		expect(
			provider.Submitted.at(0).IdempotencyKey
				== provider.Submitted.at(1).IdempotencyKey,
			"both attempts carried the same key");
	}

	void test_a_different_comment_gets_a_different_key()
	{
		auto other = proposal();
		other.Body = "Something else entirely.";
		expect(
			application::idempotency_key_for(proposal(), target(0x11U))
				!= application::idempotency_key_for(other, target(0x11U)),
			"a different body produces a different key");

		auto elsewhere        = proposal();
		elsewhere.Anchor.Line = 13U;
		expect(
			application::idempotency_key_for(proposal(), target(0x11U))
				!= application::idempotency_key_for(elsewhere, target(0x11U)),
			"a different anchor produces a different key");

		expect(
			application::idempotency_key_for(proposal(), target(0x11U))
				!= application::idempotency_key_for(proposal(), target(0x22U)),
			"the same comment against a different head produces a different key");

		expect(
			application::idempotency_key_for(proposal(), target(0x11U))
				== application::idempotency_key_for(proposal(), target(0x11U)),
			"the key is a pure function of the proposal and target");
	}

	void test_capability_degradation_is_reported()
	{
		auto provider = fake_provider{ target(0x11U) };
		provider.Capabilities
			= application::review_provider_capabilities{ .ResolvesThreads    = false,
														 .RecordsViewedState = false };
		auto coordinator = coordinator_for(provider);

		expect(
			!coordinator.capabilities().ResolvesThreads
				&& !coordinator.capabilities().RecordsViewedState,
			"a provider without native support reports it rather than "
			"silently skipping");
	}

	void test_a_provider_failure_is_reported_not_swallowed()
	{
		auto provider = fake_provider{ target(0x11U) };
		provider.SubmitFailure
			= application::review_mutation_failure::refused_by_server;
		auto coordinator = coordinator_for(provider);

		const auto refused = coordinator.publish(proposal(), false);
		expect(
			!refused.has_value()
				&& refused.error()
					== application::review_mutation_failure::refused_by_server,
			"the server's refusal reaches the caller unchanged");

		auto unreachable = fake_provider{ target(0x11U) };
		unreachable.ObserveFailure
			= application::review_mutation_failure::unavailable;
		auto       second      = coordinator_for(unreachable);
		const auto unavailable = second.publish(proposal(), false);
		expect(
			!unavailable.has_value()
				&& unavailable.error()
					== application::review_mutation_failure::unavailable,
			"a target that cannot be observed refuses rather than assuming");
		expect(
			unreachable.Submitted.empty(),
			"an unobservable target submits nothing");
	}
} // namespace

auto main() -> int
{
	test_a_matching_target_publishes_under_the_acting_identity();
	test_a_target_altered_after_the_prepare_check_is_refused();
	test_no_acknowledgement_authorises_another_pull_request();
	test_a_moved_head_needs_an_acknowledgement();
	test_a_retry_does_not_create_a_second_comment();
	test_a_different_comment_gets_a_different_key();
	test_capability_degradation_is_reported();
	test_a_provider_failure_is_reported_not_swallowed();

	std::cout << "review-mutation-tests:pass\n";
	return EXIT_SUCCESS;
}
