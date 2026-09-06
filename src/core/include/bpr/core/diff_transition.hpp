#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_TRANSITION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_TRANSITION_HPP

#include "bpr/core/diff_compute.hpp"

#include <compare>
#include <optional>
#include <variant>
#include <vector>

namespace bpr::core {
	/// \brief Describes the diff request context record.
	struct diff_request_context
	{
		request_id             Request;
		application_session_id ApplicationSession;
		file_comparison_key    Key;
		request_generation     Generation;

		auto operator<=>(const diff_request_context&) const = default;
	};

	/// \brief Describes the request file diff intent record.
	struct request_file_diff_intent
	{
		file_comparison_key Key;
		diff_options        Options;

		auto operator<=>(const request_file_diff_intent&) const = default;
	};

	/// \brief Describes the cancel file diff intent record.
	struct cancel_file_diff_intent
	{
		auto operator<=>(const cancel_file_diff_intent&) const = default;
	};

	/// \brief Describes the invalidate diff authority intent record.
	struct invalidate_diff_authority_intent
	{
		pull_request_authority Authority;

		auto operator<=>(const invalidate_diff_authority_intent&) const = default;
	};

	/// \brief Invalidates comparisons that use one pruned epoch.
	struct invalidate_diff_epoch_intent
	{
		review_epoch_id Epoch;

		auto operator<=>(const invalidate_diff_epoch_intent&) const = default;
	};

	/// \brief Checks either retained epoch anchor using its full catalog identity.
	[[nodiscard]] auto uses_epoch(const file_comparison_key& aKey,
								  const review_epoch_id&     aEpoch) noexcept -> bool;

	/// \brief Describes the file diff computed event record.
	struct file_diff_computed_event
	{
		diff_request_context Context;

		auto operator<=>(const file_diff_computed_event&) const = default;
	};

	/// \brief Describes the file diff failed event record.
	struct file_diff_failed_event
	{
		diff_request_context Context;
		diff_reason_code     Reason = diff_reason_code::host_failure;

		auto operator<=>(const file_diff_failed_event&) const = default;
	};

	using diff_event = std::variant<request_file_diff_intent,
									cancel_file_diff_intent,
									invalidate_diff_authority_intent,
									invalidate_diff_epoch_intent,
									file_diff_computed_event,
									file_diff_failed_event>;

	/// \brief Describes the compute file diff request record.
	struct compute_file_diff_request
	{
		diff_request_context Context;
		diff_options         Options;

		auto operator<=>(const compute_file_diff_request&) const = default;
	};

	/// \brief Describes the cancel file diff request record.
	struct cancel_file_diff_request
	{
		diff_request_context Context;

		auto operator<=>(const cancel_file_diff_request&) const = default;
	};

	using diff_host_request = std::variant<compute_file_diff_request, cancel_file_diff_request>;

	enum class diff_transition_reason : std::uint8_t {
		none,
		invalid_request,
		unsupported_algorithm,
		request_identity_exhausted,
		stale_completion,
		invalid_failure,
	};

	/// \brief Describes the accepted file diff delta record.
	struct accepted_file_diff_delta
	{
		diff_request_context Context;

		auto operator<=>(const accepted_file_diff_delta&) const = default;
	};

	/// \brief Describes the diff terminal delta record.
	struct diff_terminal_delta
	{
		diff_request_context Context;
		diff_reason_code     Reason = diff_reason_code::none;

		auto operator<=>(const diff_terminal_delta&) const = default;
	};

	/// \brief Describes the diff transition output record.
	struct diff_transition_output
	{
		std::vector<diff_host_request>          Requests;
		std::optional<accepted_file_diff_delta> Accepted;
		std::optional<diff_terminal_delta>      Terminal;
		diff_transition_reason                  Reason = diff_transition_reason::none;

		auto operator<=>(const diff_transition_output&) const = default;
	};

	/// \brief Describes the diff state record.
	struct diff_state
	{
		application_session_id              ApplicationSession;
		request_id                          NextRequest{ 1U };
		request_generation                  Generation;
		std::optional<diff_request_context> Pending;
		std::optional<diff_request_context> Current;
		diff_reason_code                    LastReason = diff_reason_code::none;

		auto operator<=>(const diff_state&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const diff_request_context& aContext) noexcept -> bool;
	/// \brief Applies an accepted event to the supplied state.
	[[nodiscard]] auto transition(diff_state& aState, const diff_event& aEvent)
		-> diff_transition_output;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_TRANSITION_HPP
