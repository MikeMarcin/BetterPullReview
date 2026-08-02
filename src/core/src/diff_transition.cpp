#include "bpr/core/diff_transition.hpp"

#include <limits>
#include <type_traits>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] constexpr auto valid_failure(const diff_reason_code aReason) noexcept -> bool
		{
			return aReason != diff_reason_code::none
				&& aReason != diff_reason_code::stale;
		}

		[[nodiscard]] auto request(diff_state&                     aState,
								   const request_file_diff_intent& aIntent)
			-> diff_transition_output
		{
			auto output = diff_transition_output{ };
			if (aState.Pending.has_value()) {
				output.Requests.emplace_back(cancel_file_diff_request{ *aState.Pending });
				aState.Pending.reset();
			}
			const auto digest = options_digest(aIntent.Options);
			if (!valid(aIntent.Key) || !digest.has_value()
				|| *digest != aIntent.Key.Options) {
				output.Reason     = diff_transition_reason::invalid_request;
				aState.LastReason = diff_reason_code::invalid_alignment;
				return output;
			}
			if (aIntent.Key.Algorithm != supported_diff_algorithm_revision) {
				output.Reason     = diff_transition_reason::unsupported_algorithm;
				aState.LastReason = diff_reason_code::host_failure;
				return output;
			}
			if (aState.ApplicationSession.value() == 0U
				|| aState.NextRequest.value() == 0U
				|| aState.NextRequest.value()
					== std::numeric_limits<std::uint64_t>::max()
				|| aState.Generation.value()
					== std::numeric_limits<std::uint64_t>::max()) {
				output.Reason     = diff_transition_reason::request_identity_exhausted;
				aState.LastReason = diff_reason_code::host_failure;
				return output;
			}

			aState.Generation  = request_generation{ aState.Generation.value() + 1U };
			const auto context = diff_request_context{
				.Request            = aState.NextRequest,
				.ApplicationSession = aState.ApplicationSession,
				.Key                = aIntent.Key,
				.Generation         = aState.Generation,
			};
			aState.NextRequest = request_id{ aState.NextRequest.value() + 1U };
			aState.Pending     = context;
			aState.LastReason  = diff_reason_code::none;
			output.Requests.emplace_back(compute_file_diff_request{ .Context = context, .Options = aIntent.Options });
			return output;
		}

		[[nodiscard]] auto cancel(diff_state& aState) -> diff_transition_output
		{
			auto output = diff_transition_output{ };
			if (!aState.Pending.has_value()) {
				return output;
			}
			const auto context = *aState.Pending;
			output.Requests.emplace_back(cancel_file_diff_request{ context });
			output.Terminal = diff_terminal_delta{
				.Context = context,
				.Reason  = diff_reason_code::cancelled,
			};
			aState.Pending.reset();
			aState.LastReason = diff_reason_code::cancelled;
			return output;
		}

		[[nodiscard]] auto invalidate(
			diff_state& aState, const invalidate_diff_authority_intent& aIntent)
			-> diff_transition_output
		{
			auto output = diff_transition_output{ };
			if (aState.Pending.has_value()
				&& aState.Pending->Key.Authority == aIntent.Authority) {
				output.Requests.emplace_back(cancel_file_diff_request{ *aState.Pending });
				aState.Pending.reset();
			}
			if (aState.Current.has_value()
				&& aState.Current->Key.Authority == aIntent.Authority) {
				aState.Current.reset();
			}
			return output;
		}

		[[nodiscard]] auto complete(diff_state&                     aState,
									const file_diff_computed_event& aEvent)
			-> diff_transition_output
		{
			auto output = diff_transition_output{ };
			if (!aState.Pending.has_value() || *aState.Pending != aEvent.Context) {
				output.Reason = diff_transition_reason::stale_completion;
				return output;
			}
			aState.Pending.reset();
			aState.Current    = aEvent.Context;
			aState.LastReason = diff_reason_code::none;
			output.Accepted   = accepted_file_diff_delta{ aEvent.Context };
			output.Terminal   = diff_terminal_delta{
				.Context = aEvent.Context,
				.Reason  = diff_reason_code::none,
			};
			return output;
		}

		[[nodiscard]] auto fail(diff_state&                   aState,
								const file_diff_failed_event& aEvent)
			-> diff_transition_output
		{
			auto output = diff_transition_output{ };
			if (!valid_failure(aEvent.Reason)) {
				output.Reason = diff_transition_reason::invalid_failure;
				return output;
			}
			if (!aState.Pending.has_value() || *aState.Pending != aEvent.Context) {
				output.Reason = diff_transition_reason::stale_completion;
				return output;
			}
			aState.Pending.reset();
			aState.LastReason = aEvent.Reason;
			output.Terminal   = diff_terminal_delta{
				.Context = aEvent.Context,
				.Reason  = aEvent.Reason,
			};
			return output;
		}
	} // namespace

	auto valid(const diff_request_context& aContext) noexcept -> bool
	{
		return aContext.Request.value() != 0U
			&& aContext.ApplicationSession.value() != 0U
			&& aContext.Generation.value() != 0U && valid(aContext.Key);
	}

	auto transition(diff_state& aState, const diff_event& aEvent)
		-> diff_transition_output
	{
		return std::visit(
			[&aState](const auto& aTypedEvent) -> diff_transition_output {
				using event_type = std::decay_t<decltype(aTypedEvent)>;
				if constexpr (std::is_same_v<event_type,
											 request_file_diff_intent>) {
					return request(aState, aTypedEvent);
				} else if constexpr (std::is_same_v<event_type,
													cancel_file_diff_intent>) {
					return cancel(aState);
				} else if constexpr (std::is_same_v<
										 event_type,
										 invalidate_diff_authority_intent>) {
					return invalidate(aState, aTypedEvent);
				} else if constexpr (std::is_same_v<event_type,
													file_diff_computed_event>) {
					return complete(aState, aTypedEvent);
				} else {
					return fail(aState, aTypedEvent);
				}
			},
			aEvent);
	}
} // namespace bpr::core
