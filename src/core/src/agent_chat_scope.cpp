#include "bpr/core/agent_chat_scope.hpp"

namespace bpr::core {
	namespace {
		[[nodiscard]] auto engaged_identity_count(
			const agent_chat_scope& aScope) noexcept -> std::size_t
		{
			return static_cast<std::size_t>(aScope.Selection.has_value())
				+ static_cast<std::size_t>(aScope.File.has_value())
				+ static_cast<std::size_t>(aScope.Comparison.has_value());
		}
	} // namespace

	auto stable_name(const agent_entry_point aEntryPoint) noexcept
		-> std::string_view
	{
		switch (aEntryPoint) {
		case agent_entry_point::ask_about_selection:
			return "ask_about_selection";
		case agent_entry_point::analyze_current_file:
			return "analyze_current_file";
		case agent_entry_point::analyze_latest_upload:
			return "analyze_latest_upload";
		case agent_entry_point::evaluate_proposed_fix:
			return "evaluate_proposed_fix";
		case agent_entry_point::workshop_comment:
			return "workshop_comment";
		case agent_entry_point::run_ai_review:
			return "run_ai_review";
		case agent_entry_point::open_full_agent_session:
			return "open_full_agent_session";
		case agent_entry_point::ask_about_conversation:
			return "ask_about_conversation";
		}
		return { };
	}

	auto declared(const agent_entry_point aEntryPoint) noexcept -> bool
	{
		return static_cast<std::size_t>(aEntryPoint) < agent_entry_point_count;
	}

	auto acts_on_selection(const agent_entry_point aEntryPoint) noexcept -> bool
	{
		switch (aEntryPoint) {
		case agent_entry_point::ask_about_selection:
		case agent_entry_point::analyze_current_file:
		case agent_entry_point::evaluate_proposed_fix:
			return true;
		case agent_entry_point::analyze_latest_upload:
		case agent_entry_point::workshop_comment:
		case agent_entry_point::run_ai_review:
		case agent_entry_point::open_full_agent_session:
		case agent_entry_point::ask_about_conversation:
			return false;
		}
		// An undeclared entry point acts on nothing, and is refused by name
		// before this is consulted.
		return false;
	}

	auto stable_name(const workshop_operation aOperation) noexcept
		-> std::string_view
	{
		switch (aOperation) {
		case workshop_operation::clarify:
			return "clarify";
		case workshop_operation::shorten:
			return "shorten";
		case workshop_operation::make_actionable:
			return "make_actionable";
		case workshop_operation::phrase_as_a_question:
			return "phrase_as_a_question";
		case workshop_operation::make_more_direct:
			return "make_more_direct";
		case workshop_operation::check_the_premise:
			return "check_the_premise";
		}
		return { };
	}

	auto declared(const workshop_operation aOperation) noexcept -> bool
	{
		return static_cast<std::size_t>(aOperation) < workshop_operation_count;
	}

	auto workshop_operation_for(
		const agent_entry_point                 aEntryPoint,
		const std::optional<workshop_operation> aOperation) noexcept
		-> std::optional<workshop_operation>
	{
		if (aEntryPoint != agent_entry_point::workshop_comment || !aOperation
			|| !declared(*aOperation)) {
			return std::nullopt;
		}
		return aOperation;
	}

	auto stable_name(const agent_observation aObservation) noexcept
		-> std::string_view
	{
		switch (aObservation) {
		case agent_observation::refresh:
			return "refresh";
		case agent_observation::navigation:
			return "navigation";
		case agent_observation::comment_arrival:
			return "comment_arrival";
		case agent_observation::revision_update:
			return "revision_update";
		}
		return { };
	}

	auto note_agent_observation(const agent_observation aObservation) noexcept
		-> agent_observation_record
	{
		return { .Observation = aObservation };
	}

	auto intent_for(const agent_observation_record& aRecord) noexcept
		-> std::optional<agent_action_intent>
	{
		switch (aRecord.Observation) {
		case agent_observation::refresh:
		case agent_observation::navigation:
		case agent_observation::comment_arrival:
		case agent_observation::revision_update:
			// Each named separately rather than swept into one return, so a
			// fifth observation has to arrive at this switch and be argued
			// about, and so that the day somebody wants "refresh re-runs the
			// review" the change is one visible line in a reviewed file.
			return std::nullopt;
		}
		return std::nullopt;
	}

	auto valid(const agent_selection& aSelection) noexcept -> bool
	{
		const auto& selection = aSelection.Selection;
		if (
			!aSelection.File.valid() || !selection.Comparison.valid()
			|| selection.PreferredLine.Comparison != selection.Comparison
			|| selection.PreferredLine.Side > diff_side::right
			|| selection.Kind > semantic_selection_kind::group) {
			return false;
		}
		if (
			selection.Kind == semantic_selection_kind::group
			&& (!selection.Segment.has_value() || !selection.Group.has_value())) {
			return false;
		}
		return (!selection.Segment || selection.Segment->valid())
			&& (!selection.Group || selection.Group->valid());
	}

	auto stable_name(const agent_chat_scope_kind aKind) noexcept
		-> std::string_view
	{
		switch (aKind) {
		case agent_chat_scope_kind::selected_lines:
			return "selected_lines";
		case agent_chat_scope_kind::file:
			return "file";
		case agent_chat_scope_kind::pull_request:
			return "pull_request";
		case agent_chat_scope_kind::repository:
			return "repository";
		}
		return { };
	}

	auto declared(const agent_chat_scope_kind aKind) noexcept -> bool
	{
		return static_cast<std::size_t>(aKind) < agent_chat_scope_kind_count;
	}

	auto wider(
		const agent_chat_scope_kind aLeft,
		const agent_chat_scope_kind aRight) noexcept -> bool
	{
		return declared(aLeft) && declared(aRight)
			&& static_cast<std::size_t>(aLeft) > static_cast<std::size_t>(aRight);
	}

	auto valid(const agent_chat_scope& aScope) noexcept -> bool
	{
		if (!declared(aScope.Kind)) {
			return false;
		}
		switch (aScope.Kind) {
		case agent_chat_scope_kind::selected_lines:
			return engaged_identity_count(aScope) == 1U
				&& aScope.Selection.has_value() && valid(*aScope.Selection);
		case agent_chat_scope_kind::file:
			return engaged_identity_count(aScope) == 2U
				&& aScope.File.has_value() && aScope.File->valid()
				&& aScope.Comparison.has_value() && aScope.Comparison->valid();
		case agent_chat_scope_kind::pull_request:
		case agent_chat_scope_kind::repository:
			// The session's own binding names the host, the repository, and
			// the pull request. A scope carrying its own would be a second
			// answer to a question the session already answered.
			return engaged_identity_count(aScope) == 0U;
		}
		return false;
	}

	auto begin_agent_chat(const agent_selection& aSelection) noexcept
		-> std::optional<agent_chat_state>
	{
		if (!valid(aSelection)) {
			return std::nullopt;
		}
		return agent_chat_state{
			.Scope = {
				.Kind      = agent_chat_scope_kind::selected_lines,
				.Selection = aSelection,
			},
		};
	}

	auto stable_name(const agent_scope_refusal aRefusal) noexcept
		-> std::string_view
	{
		switch (aRefusal) {
		case agent_scope_refusal::none:
			return "none";
		case agent_scope_refusal::state_invalid:
			return "state_invalid";
		case agent_scope_refusal::not_a_widening:
			return "not_a_widening";
		case agent_scope_refusal::unknown_scope:
			return "unknown_scope";
		}
		return { };
	}

	auto next_agent_chat_turn(
		const agent_chat_state&                           aState,
		const std::optional<agent_scope_widening_intent>& aWidening) noexcept
		-> agent_chat_turn
	{
		auto unchanged = agent_chat_turn{
			.Refusal = agent_scope_refusal::state_invalid,
			.State   = aState,
		};
		if (
			!valid(aState.Scope)
			|| (aState.WidenedFrom.has_value() && !valid(*aState.WidenedFrom))) {
			return unchanged;
		}
		if (!aWidening.has_value()) {
			// The whole of the turn that carried no widening intent. There is
			// deliberately nothing else here: no current file, no visible
			// range, no "the reviewer scrolled", because a scope that could
			// follow the viewport would send context nobody chose to send.
			unchanged.Refusal = agent_scope_refusal::none;
			return unchanged;
		}
		const auto target = aWidening->target();
		if (!declared(target)) {
			unchanged.Refusal = agent_scope_refusal::unknown_scope;
			return unchanged;
		}
		if (!wider(target, aState.Scope.Kind)) {
			unchanged.Refusal = agent_scope_refusal::not_a_widening;
			return unchanged;
		}

		auto widened = agent_chat_scope{ .Kind = target };
		if (target == agent_chat_scope_kind::file) {
			// The only scope `file` is wider than is `selected_lines`, whose
			// validity above engaged the selection. The file and the
			// comparison are the selection's own identities: widening states
			// which file in the identities the reviewer was already pointing
			// at, and never re-derives one.
			widened.File       = aState.Scope.Selection->File;
			widened.Comparison = aState.Scope.Selection->Selection.Comparison;
		}
		return {
			.Widened = true,
			.Refusal = agent_scope_refusal::none,
			.State   = {
				.Scope       = widened,
				.WidenedFrom = aState.Scope,
			},
		};
	}

	auto stable_name(const agent_action_refusal aRefusal) noexcept
		-> std::string_view
	{
		switch (aRefusal) {
		case agent_action_refusal::none:
			return "none";
		case agent_action_refusal::profile_refused:
			return "profile_refused";
		case agent_action_refusal::session_invalid:
			return "session_invalid";
		case agent_action_refusal::session_profile_mismatch:
			return "session_profile_mismatch";
		case agent_action_refusal::selection_required:
			return "selection_required";
		case agent_action_refusal::unknown_entry_point:
			return "unknown_entry_point";
		}
		return { };
	}

	auto decide_agent_action(
		const agent_profile&                    aProfile,
		const std::optional<agent_host_policy>& aPolicy,
		const std::optional<agent_selection>&   aSelection,
		const agent_session&                    aSession,
		const agent_action_intent&              aIntent) noexcept
		-> agent_action_verdict
	{
		auto verdict = agent_action_verdict{
			.EntryPoint = aIntent.entry_point(),
		};
		if (!declared(aIntent.entry_point())) {
			// Before the host policy is consulted, so a cast enumerator never
			// reaches a branch that could permit it, and `Invocation` stays
			// absent to say the policy was never asked.
			verdict.Refusal = agent_action_refusal::unknown_entry_point;
			return verdict;
		}
		const auto invocation = decide_agent_invocation(aProfile, aPolicy);
		verdict.Invocation    = invocation;
		if (!invocation.Permitted) {
			verdict.Refusal = agent_action_refusal::profile_refused;
			return verdict;
		}
		if (!valid_agent_session(aSession)) {
			verdict.Refusal = agent_action_refusal::session_invalid;
			return verdict;
		}
		if (aSession.Profile != aProfile.Id) {
			verdict.Refusal = agent_action_refusal::session_profile_mismatch;
			return verdict;
		}
		if (
			acts_on_selection(aIntent.entry_point())
			&& (!aSelection.has_value() || !valid(*aSelection))) {
			verdict.Refusal = agent_action_refusal::selection_required;
			return verdict;
		}
		verdict.Permitted = true;
		verdict.Refusal   = agent_action_refusal::none;
		return verdict;
	}

	auto stable_name(const agent_send_refusal aRefusal) noexcept
		-> std::string_view
	{
		switch (aRefusal) {
		case agent_send_refusal::none:                     return "none";
		case agent_send_refusal::no_review:                return "no_review";
		case agent_send_refusal::no_agent:                 return "no_agent";
		case agent_send_refusal::empty_message:            return "empty_message";
		case agent_send_refusal::request_in_flight:        return "request_in_flight";
		case agent_send_refusal::session_stale:            return "session_stale";
		case agent_send_refusal::agent_unknown:            return "agent_unknown";
		case agent_send_refusal::host_not_mapped:          return "host_not_mapped";
		case agent_send_refusal::agent_forbidden_on_host:  return "agent_forbidden_on_host";
		case agent_send_refusal::agent_not_required_one:   return "agent_not_required_one";
		case agent_send_refusal::session_profile_mismatch: return "session_profile_mismatch";
		case agent_send_refusal::selection_required:       return "selection_required";
		case agent_send_refusal::unknown_action:           return "unknown_action";
		case agent_send_refusal::scope_invalid:            return "scope_invalid";
		case agent_send_refusal::private_context_refused:  return "private_context_refused";
		case agent_send_refusal::launch_refused:           return "launch_refused";
		}
		return { };
	}

	auto agent_send_refusal_of(const agent_action_verdict& aVerdict) noexcept
		-> agent_send_refusal
	{
		if (aVerdict.Permitted) {
			return agent_send_refusal::none;
		}
		switch (aVerdict.Refusal) {
		case agent_action_refusal::none:
			// A verdict that is not permitted and names no refusal is still a
			// refusal; it is never read as a send.
			return agent_send_refusal::unknown_action;
		case agent_action_refusal::profile_refused:
			switch (aVerdict.Invocation ? aVerdict.Invocation->Refusal : agent_invocation_refusal::profile_unknown) {
			case agent_invocation_refusal::host_not_mapped:           return agent_send_refusal::host_not_mapped;
			case agent_invocation_refusal::profile_forbidden_on_host: return agent_send_refusal::agent_forbidden_on_host;
			case agent_invocation_refusal::profile_not_required_one:  return agent_send_refusal::agent_not_required_one;
			case agent_invocation_refusal::none:
			case agent_invocation_refusal::profile_invalid:
			case agent_invocation_refusal::profile_unknown:           return agent_send_refusal::agent_unknown;
			}
			return agent_send_refusal::agent_unknown;
		case agent_action_refusal::session_invalid:          return agent_send_refusal::no_review;
		case agent_action_refusal::session_profile_mismatch: return agent_send_refusal::session_profile_mismatch;
		case agent_action_refusal::selection_required:       return agent_send_refusal::selection_required;
		case agent_action_refusal::unknown_entry_point:      return agent_send_refusal::unknown_action;
		}
		return agent_send_refusal::unknown_action;
	}

	auto decide_agent_send(const agent_send_facts& aFacts) noexcept
		-> agent_send_refusal
	{
		if (!aFacts.ReviewOpen) {
			return agent_send_refusal::no_review;
		}
		if (aFacts.ContinuationRequired) {
			return agent_send_refusal::session_stale;
		}
		if (aFacts.InFlight) {
			return agent_send_refusal::request_in_flight;
		}
		if (!aFacts.AgentChosen) {
			return agent_send_refusal::no_agent;
		}
		if (aFacts.MessageEmpty) {
			return agent_send_refusal::empty_message;
		}
		return agent_send_refusal::none;
	}
} // namespace bpr::core
