#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CONVERSATION_MUTATION_LIFECYCLE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CONVERSATION_MUTATION_LIFECYCLE_HPP

#include "bpr/core/conversation_reactions.hpp"
#include "bpr/core/epoch_model.hpp"

namespace bpr::core {

	/// \brief [[BPR-1800]]. An admitted change keeps its original request identity
	/// even when a newer read changes the workbench generation.
	struct conversation_mutation_request
	{
		request_id             Request;
		application_session_id Session;
		request_generation     Generation;

		auto operator<=>(const conversation_mutation_request&) const = default;
	};

	/// \brief Stable authority and source of conversation changes. Snapshot and
	/// read/workbench generations deliberately do not participate: an accepted
	/// refresh of this exact context still owes each admitted change its answer.
	struct conversation_mutation_scope
	{
		application_session_id     Session;
		host_profile_id            Profile;
		github_profile_revision    ProfileRevision;
		git_host_id                Host;
		host_authority_fingerprint HostAuthority;
		github_account_id          Account;
		github_viewer_actor_id     Viewer;
		github_identity_generation IdentityGeneration;
		repository_id              Repository;
		pull_request_id            PullRequest;
		pull_request_number        Number;
		git_commit_oid             Base;
		git_commit_oid             Head;
		github_access_mode         Mode{ };

		auto operator<=>(const conversation_mutation_scope&) const = default;
	};

	/// \brief Whether old conversation effects may finish for the adopted context.
	enum class conversation_mutation_retirement : std::uint8_t { cancel,
																 drain };
	/// \brief The conversation state changed by one admitted gesture.
	enum class conversation_mutation_kind : std::uint8_t { resolution,
														   reaction };

	/// \brief Exact gesture bound to a pending request, independent of Qt rows.
	struct pending_conversation_mutation
	{
		conversation_mutation_kind Kind = conversation_mutation_kind::resolution;
		std::string                Subject;
		reaction_content           Content = reaction_content::thumbs_up;
		bool                       Add     = true;

		auto operator<=>(const pending_conversation_mutation&) const = default;
	};

	/// \brief Current context and its admitted, unanswered conversation changes.
	struct conversation_mutation_state
	{
		std::optional<conversation_mutation_scope>                             Scope;
		std::map<conversation_mutation_request, pending_conversation_mutation> Pending;
	};

	/// \brief The host supplies whether its complete provider-free target/actor
	/// binding also agrees. A changed binding or scope retires all old ownership.
	[[nodiscard]] auto adopt_conversation_mutations(conversation_mutation_state& aState, conversation_overlay& aOverlay,
													conversation_mutation_scope aScope, bool aBindingCurrent) -> conversation_mutation_retirement;
	/// \brief Retire request ownership and its presentation overlay together.
	void retire_conversation_mutations(conversation_mutation_state& aState, conversation_overlay& aOverlay);
	/// \brief Register a nonzero request from the adopted application session.
	[[nodiscard]] auto retain_conversation_mutation(conversation_mutation_state& aState, conversation_mutation_request aRequest,
													pending_conversation_mutation aChange) -> bool;
	/// \brief Accept only the exact pending identity and gesture, once. A failed,
	/// cancelled or unconfirmed answer restores the read; it never retries a write.
	[[nodiscard]] auto settle_conversation_mutation(conversation_mutation_state& aState, conversation_overlay& aOverlay,
													conversation_mutation_request aRequest, const pending_conversation_mutation& aChange, bool aSucceeded,
													std::optional<reaction_summary> aAnswered = { }) -> bool;
}

#endif
