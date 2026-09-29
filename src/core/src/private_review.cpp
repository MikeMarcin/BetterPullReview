#include "bpr/core/private_review.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <type_traits>

namespace bpr::core {
	namespace {
		auto valid_anchor(const comparison_anchor_identity& aAnchor, epoch_ordinal aEpoch) -> bool
		{
			return aAnchor.Authority.valid() && aAnchor.Incarnation.valid() && aAnchor.Epoch == aEpoch
				&& aEpoch.value() > 0 && aAnchor.Kind <= comparison_anchor_kind::review_commit
				&& (aAnchor.Kind != comparison_anchor_kind::review_commit || aAnchor.Review.valid());
		}
		auto valid_actor(const private_review_actor& aActor, const private_review_scope& aScope) -> bool
		{
			return std::visit([&](const auto& aValue) {
				using value_type = std::decay_t<decltype(aValue)>;
				if constexpr (std::is_same_v<value_type, private_reviewer_actor>) {
					return !aValue.Account || (valid_private_id(*aValue.Account) && *aValue.Account == aScope.Account);
				} else if constexpr (std::is_same_v<value_type, private_agent_actor>) {
					return valid_private_id(aValue.Run);
				} else {
					return true;
				}
			},
							  aActor);
		}
		auto valid_reference(const private_revision_reference& aRef) -> bool
		{
			return valid_private_scope(aRef.Scope) && valid_private_id(aRef.Thread) && valid_private_id(aRef.Turn)
				&& valid_private_id(aRef.Revision) && valid_private_id(aRef.Order) && aRef.Digest.valid();
		}
		auto valid_prior(const private_revision_reference& aNew, const private_revision_reference& aPrior) -> bool
		{
			return valid_reference(aPrior) && aNew.Scope == aPrior.Scope && aPrior.Order < aNew.Order
				&& aPrior.Revision != aNew.Revision;
		}
		auto valid_thread(const private_review_thread& aThread) -> bool
		{
			if (!valid_private_id(aThread.Id) || !valid_private_scope(scope_of(aThread.Binding))
				|| !valid_private_id(aThread.LastOrder) || aThread.Version == 0 || aThread.Version > maximum_private_identity
				|| aThread.Lifecycle > private_thread_lifecycle::archived) {
				return false;
			}
			if (const auto* exact = std::get_if<exact_private_subject>(&aThread.Binding)) {
				return valid_private_binding(exact->Binding);
			}
			const auto& legacy = std::get<legacy_retained_subject>(aThread.Binding);
			return !legacy.Attachment || (legacy.Attachment->File.valid() && legacy.Attachment->Comparison.valid());
		}
		auto writable(const private_review_thread& aThread, std::uint64_t aExpectedVersion) -> std::optional<private_review_refusal>
		{
			if (!valid_thread(aThread)) {
				return private_review_refusal::invalid_identity;
			}
			if (aExpectedVersion != aThread.Version) {
				return private_review_refusal::conflict;
			}
			if (aThread.Version == maximum_private_identity) {
				return private_review_refusal::bounds;
			}
			if (aThread.Lifecycle == private_thread_lifecycle::archived) {
				return private_review_refusal::archived;
			}
			return std::nullopt;
		}
		auto make_revision(const private_review_thread& aThread, const private_revision_input& aInput)
			-> std::expected<private_review_revision, private_review_refusal>
		{
			if (!valid_private_id(aInput.Turn) || !valid_private_id(aInput.Revision) || !valid_private_id(aInput.Order)) {
				return std::unexpected(private_review_refusal::invalid_identity);
			}
			if (!valid_actor(aInput.Actor, scope_of(aThread.Binding))) {
				return std::unexpected(private_review_refusal::invalid_actor);
			}
			if (aInput.Completion > private_revision_completion::cancelled
				|| (!std::holds_alternative<private_agent_actor>(aInput.Actor) && aInput.Completion != private_revision_completion::complete)) {
				return std::unexpected(private_review_refusal::invalid_completion);
			}
			const auto limit = std::holds_alternative<private_agent_actor>(aInput.Actor) ? maximum_private_assistant_body_bytes : maximum_private_reviewer_body_bytes;
			if (aInput.Body.size() > limit) {
				return std::unexpected(private_review_refusal::bounds);
			}
			return private_review_revision{
				.Identity       = { scope_of(aThread.Binding), aThread.Id, aInput.Turn, aInput.Revision, aInput.Order, private_digest(aInput.Body) },
				.Actor          = aInput.Actor,
				.Body           = aInput.Body,
				.Completion     = aInput.Completion,
				.Predecessor    = { },
				.ReplyTo        = { },
				.EditDerivation = { }
			};
		}
		auto valid_agent_authorization(const private_review_thread& aThread, const private_revision_input& aInput, const private_run_authorization* aRun) -> bool
		{
			const auto* actor = std::get_if<private_agent_actor>(&aInput.Actor);
			const auto* exact = std::get_if<exact_private_subject>(&aThread.Binding);
			return actor && exact && aRun && aRun->Scope == exact->Binding.Scope && aRun->Run == actor->Run
				&& aRun->Binding == exact->Binding && valid_private_id(aRun->Order) && aRun->Order < aInput.Order
				&& aRun->Completion == aInput.Completion
				&& (aRun->OutputDigest ? *aRun->OutputDigest == private_digest(aInput.Body)
									   : aInput.Body.empty() && aInput.Completion != private_revision_completion::complete);
		}
		auto diff_subject_valid(const private_thread_subject& aSubject, const diff_semantic_index& aIndex) -> bool
		{
			const auto& alignment = aIndex.diff().Alignment;
			return std::visit([&](const auto& aValue) -> bool {
				using value_type = std::decay_t<decltype(aValue)>;
				if constexpr (requires { aValue.Comparison; }) {
					if (aValue.Comparison != alignment.Digest) {
						return false;
					}
					if constexpr (std::is_same_v<value_type, private_file_subject>) {
						return aValue.File == alignment.Key.File;
					} else if constexpr (std::is_same_v<value_type, source_line_ref> || std::is_same_v<value_type, source_boundary_ref>) {
						return valid(aValue, alignment.LeftLineCount, alignment.RightLineCount);
					} else if constexpr (std::is_same_v<value_type, source_span_ref>) {
						return aValue.Side <= diff_side::right && !aValue.Lines.empty() && valid(aValue.Lines, aValue.Side == diff_side::left ? alignment.LeftLineCount : alignment.RightLineCount);
					} else if constexpr (std::is_same_v<value_type, private_group_subject>) {
						return aIndex.group_index(aValue.Group).has_value();
					} else if constexpr (std::is_same_v<value_type, private_segment_subject>) {
						return aIndex.segment_index(aValue.Segment).has_value();
					} else if constexpr (std::is_same_v<value_type, private_move_subject>) {
						return std::ranges::any_of(alignment.Moves, [&](const auto& aMove) { return aMove.Id == aValue.Move; });
					} else {
						return false;
					}
				} else {
					// A statement after a chain every branch of which returns is C4702 on MSVC.
					return false;
				}
			},
							  aSubject);
		}
	}
	auto valid_private_scope(const private_review_scope& aScope) noexcept -> bool
	{
		return valid_private_id(aScope.Profile) && aScope.Incarnation.valid() && valid_private_id(aScope.Host)
			&& aScope.Account.value() <= maximum_private_identity && valid_private_id(aScope.Repository) && valid_private_id(aScope.PullRequest);
	}
	auto valid_private_binding(const private_review_binding& aBinding) noexcept -> bool
	{
		const auto& scope    = aBinding.Scope;
		const auto& revision = aBinding.Revision;
		return valid_private_scope(scope) && aBinding.HostAuthority.valid()
			&& ((aBinding.Access == github_access_mode::authenticated && valid_private_id(scope.Account))
				|| (aBinding.Access == github_access_mode::public_preview && scope.Account.value() == 0))
			&& revision.Host == scope.Host && revision.Repository == scope.Repository && revision.PullRequest == scope.PullRequest
			&& revision.BaseOid.valid() && revision.HeadOid.valid()
			&& valid_anchor(aBinding.LeftAnchor, revision.LeftEpoch) && valid_anchor(aBinding.RightAnchor, revision.RightEpoch)
			&& aBinding.LeftAnchor.Authority == aBinding.RightAnchor.Authority && aBinding.LeftAnchor.Incarnation == aBinding.RightAnchor.Incarnation
			&& (!aBinding.Comparison || aBinding.Comparison->valid());
	}
	auto scope_of(const private_thread_binding& aBinding) noexcept -> const private_review_scope&
	{
		if (const auto* exact = std::get_if<exact_private_subject>(&aBinding)) {
			return exact->Binding.Scope;
		}
		return std::get<legacy_retained_subject>(aBinding).Scope;
	}
	auto private_digest(std::string_view aBody) noexcept -> private_body_digest
	{
		return private_body_digest{ sha256({ reinterpret_cast<const std::uint8_t*>(aBody.data()), aBody.size() }) };
	}
	auto valid_private_subject(const exact_private_subject& aSubject, const diff_semantic_index* aIndex) noexcept -> bool
	{
		if (!valid_private_binding(aSubject.Binding)) {
			return false;
		}
		if (std::holds_alternative<private_pr_subject>(aSubject.Subject)) {
			return true;
		}
		if (const auto* conversation = std::get_if<review_conversation_id>(&aSubject.Subject)) {
			return valid_private_id(*conversation);
		}
		if (const auto* finding = std::get_if<private_finding_reference>(&aSubject.Subject)) {
			return valid_private_id(finding->Run) && finding->Digest.valid();
		}
		if (!aIndex || !aSubject.Binding.Comparison) {
			return false;
		}
		const auto& key     = aIndex->diff().Alignment.Key;
		const auto& binding = aSubject.Binding;
		return aIndex->diff().Alignment.Digest == *binding.Comparison
			&& key.Authority.Host == binding.Scope.Host && key.Authority.Account == binding.Scope.Account
			&& key.Authority.HostAuthority == binding.HostAuthority && key.Authority.Mode == binding.Access
			&& key.Repository == binding.Scope.Repository && key.PullRequest == binding.Scope.PullRequest
			&& key.LeftAnchor == binding.LeftAnchor && key.RightAnchor == binding.RightAnchor
			&& diff_subject_valid(aSubject.Subject, *aIndex);
	}
	auto observe_private_subject(const private_thread_binding& aOriginal, const private_review_binding& aCurrent, bool aExactSubjectAvailable) noexcept -> private_subject_availability
	{
		const auto* exact = std::get_if<exact_private_subject>(&aOriginal);
		if (!exact || !valid_private_binding(aCurrent) || !valid_private_binding(exact->Binding)
			|| exact->Binding.Scope != aCurrent.Scope || !aExactSubjectAvailable) {
			return private_subject_availability::unplaceable;
		}
		return exact->Binding == aCurrent ? private_subject_availability::current : private_subject_availability::stale;
	}
	auto create_private_thread(private_review_thread_id aId, const exact_private_subject& aSubject,
							   const private_revision_input& aInput, const diff_semantic_index* aIndex) -> std::expected<private_revision_commit, private_review_refusal>
	{
		if (!valid_private_id(aId)) {
			return std::unexpected(private_review_refusal::invalid_identity);
		}
		if (!valid_private_subject(aSubject, aIndex)) {
			return std::unexpected(private_review_refusal::invalid_subject);
		}
		if (!std::holds_alternative<private_reviewer_actor>(aInput.Actor)) {
			return std::unexpected(private_review_refusal::invalid_actor);
		}
		const auto thread   = private_review_thread{ aId, aSubject, 1, aInput.Order, private_thread_lifecycle::open };
		auto       revision = make_revision(thread, aInput);
		if (!revision) {
			return std::unexpected(revision.error());
		}
		return private_revision_commit{ thread, std::move(*revision) };
	}
	auto migrate_private_thread(private_review_thread_id aId, const legacy_retained_subject& aSubject,
								const private_revision_input& aInput) -> std::expected<private_revision_commit, private_review_refusal>
	{
		const auto thread = private_review_thread{ aId, aSubject, 1, aInput.Order, private_thread_lifecycle::open };
		if (!valid_thread(thread)) {
			return std::unexpected(private_review_refusal::invalid_identity);
		}
		if (!std::holds_alternative<private_legacy_unknown_actor>(aInput.Actor)) {
			return std::unexpected(private_review_refusal::invalid_actor);
		}
		auto revision = make_revision(thread, aInput);
		if (!revision) {
			return std::unexpected(revision.error());
		}
		return private_revision_commit{ thread, std::move(*revision) };
	}
	auto append_private_turn(const private_review_thread& aThread, std::uint64_t aExpectedVersion,
							 const private_revision_input& aInput, const private_review_revision* aReplyTo, const private_run_authorization* aRun)
		-> std::expected<private_revision_commit, private_review_refusal>
	{
		if (auto refusal = writable(aThread, aExpectedVersion)) {
			return std::unexpected(*refusal);
		}
		if (std::holds_alternative<private_legacy_unknown_actor>(aInput.Actor)
			|| (std::holds_alternative<private_agent_actor>(aInput.Actor) && !valid_agent_authorization(aThread, aInput, aRun))) {
			return std::unexpected(private_review_refusal::invalid_actor);
		}
		auto revision = make_revision(aThread, aInput);
		if (!revision) {
			return std::unexpected(revision.error());
		}
		if (aReplyTo) {
			if (!valid_private_revision(*aReplyTo) || !valid_prior(revision->Identity, aReplyTo->Identity)) {
				return std::unexpected(private_review_refusal::missing_source);
			}
			if (aReplyTo->Identity.Turn == aInput.Turn) {
				return std::unexpected(private_review_refusal::conflict);
			}
			revision->ReplyTo = aReplyTo->Identity;
		}
		if (aInput.Order <= aThread.LastOrder) {
			return std::unexpected(private_review_refusal::cycle);
		}
		auto thread      = aThread;
		thread.LastOrder = aInput.Order;
		++thread.Version;
		return private_revision_commit{ std::move(thread), std::move(*revision) };
	}
	auto edit_private_revision(const private_review_thread& aThread, std::uint64_t aExpectedVersion,
							   const private_review_revision& aLatest, private_review_turn_revision_id aExpectedLatest,
							   const private_revision_input& aInput, bool aAdoptIncomplete) -> std::expected<private_revision_commit, private_review_refusal>
	{
		if (auto refusal = writable(aThread, aExpectedVersion)) {
			return std::unexpected(*refusal);
		}
		if (!valid_private_revision(aLatest) || aLatest.Identity.Scope != scope_of(aThread.Binding) || aLatest.Identity.Thread != aThread.Id) {
			return std::unexpected(private_review_refusal::missing_source);
		}
		if (aLatest.Identity.Revision != aExpectedLatest || aInput.Turn != aLatest.Identity.Turn) {
			return std::unexpected(private_review_refusal::conflict);
		}
		if (!std::holds_alternative<private_reviewer_actor>(aInput.Actor)) {
			return std::unexpected(private_review_refusal::invalid_actor);
		}
		if (aLatest.Completion != private_revision_completion::complete && !aAdoptIncomplete) {
			return std::unexpected(private_review_refusal::incomplete_source);
		}
		auto revision = make_revision(aThread, aInput);
		if (!revision) {
			return std::unexpected(revision.error());
		}
		if (!valid_prior(revision->Identity, aLatest.Identity)) {
			return std::unexpected(private_review_refusal::cycle);
		}
		revision->Predecessor    = aLatest.Identity;
		revision->ReplyTo        = aLatest.ReplyTo;
		revision->EditDerivation = aLatest.Completion == private_revision_completion::complete
			? private_derivation_kind::edited_from
			: private_derivation_kind::adopted_from_incomplete_run;
		if (aInput.Order <= aThread.LastOrder) {
			return std::unexpected(private_review_refusal::cycle);
		}
		auto thread      = aThread;
		thread.LastOrder = aInput.Order;
		++thread.Version;
		return private_revision_commit{ std::move(thread), std::move(*revision) };
	}
	auto set_private_lifecycle(const private_review_thread& aThread, std::uint64_t aExpectedVersion, private_thread_lifecycle aLifecycle)
		-> std::expected<private_review_thread, private_review_refusal>
	{
		if (!valid_thread(aThread)) {
			return std::unexpected(private_review_refusal::invalid_identity);
		}
		if (aThread.Version != aExpectedVersion) {
			return std::unexpected(private_review_refusal::conflict);
		}
		if (aLifecycle > private_thread_lifecycle::archived) {
			return std::unexpected(private_review_refusal::invalid_transition);
		}
		if (aThread.Lifecycle == aLifecycle) {
			return aThread;
		}
		if (aThread.Version == maximum_private_identity) {
			return std::unexpected(private_review_refusal::bounds);
		}
		auto thread      = aThread;
		thread.Lifecycle = aLifecycle;
		++thread.Version;
		return thread;
	}
	auto valid_private_revision(const private_review_revision& aRevision) noexcept -> bool
	{
		const auto& id    = aRevision.Identity;
		const auto  limit = std::holds_alternative<private_agent_actor>(aRevision.Actor) ? maximum_private_assistant_body_bytes : maximum_private_reviewer_body_bytes;
		return valid_reference(id) && valid_actor(aRevision.Actor, id.Scope) && aRevision.Body.size() <= limit
			&& aRevision.Completion <= private_revision_completion::cancelled
			&& (std::holds_alternative<private_agent_actor>(aRevision.Actor) || aRevision.Completion == private_revision_completion::complete)
			&& id.Digest == private_digest(aRevision.Body)
			&& (!aRevision.ReplyTo || valid_prior(id, *aRevision.ReplyTo))
			&& (aRevision.Predecessor.has_value() == aRevision.EditDerivation.has_value())
			&& (!aRevision.Predecessor || (valid_prior(id, *aRevision.Predecessor) && aRevision.Predecessor->Thread == id.Thread && aRevision.Predecessor->Turn == id.Turn && std::holds_alternative<private_reviewer_actor>(aRevision.Actor) && (*aRevision.EditDerivation == private_derivation_kind::edited_from || *aRevision.EditDerivation == private_derivation_kind::adopted_from_incomplete_run)));
	}
	auto eligible_private_revision(const private_review_revision& aRevision) noexcept -> bool
	{
		return valid_private_revision(aRevision) && aRevision.Completion == private_revision_completion::complete;
	}
	auto begin_private_request(private_review_state& aState, private_request_lane aLane, private_request_context aContext)
		-> std::expected<private_request_context, private_review_refusal>
	{
		const auto lane = static_cast<std::size_t>(aLane);
		if (lane >= aState.Pending.size() || !valid_private_scope(aState.Scope) || aContext.Scope != aState.Scope
			|| !valid_private_id(aContext.Session) || !valid_private_id(aContext.Request) || !valid_private_id(aContext.IdentityGeneration)
			|| aContext.Window.Profile != aState.Scope.Profile || !valid_private_id(aContext.Window.Window)
			|| !valid_private_id(aContext.Window.Generation)) {
			return std::unexpected(private_review_refusal::invalid_identity);
		}
		if (aState.Generations[lane] >= maximum_private_identity) {
			return std::unexpected(private_review_refusal::bounds);
		}
		aContext.Generation  = request_generation{ ++aState.Generations[lane] };
		aState.Pending[lane] = aContext;
		return aContext;
	}
	auto finish_private_request(private_review_state& aState, private_request_lane aLane, const private_request_context& aContext) noexcept -> bool
	{
		const auto lane = static_cast<std::size_t>(aLane);
		if (lane >= aState.Pending.size() || aContext.Scope != aState.Scope || !aState.Pending[lane] || *aState.Pending[lane] != aContext) {
			return false;
		}
		aState.Pending[lane].reset();
		return true;
	}
	auto stable_name(private_thread_lifecycle aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case private_thread_lifecycle::open:             return "open";
		case private_thread_lifecycle::resolved_locally: return "resolved_locally";
		case private_thread_lifecycle::archived:         return "archived";
		}
		return "invalid";
	}
	auto stable_name(private_revision_completion aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case private_revision_completion::complete:   return "complete";
		case private_revision_completion::incomplete: return "incomplete";
		case private_revision_completion::failed:     return "failed";
		case private_revision_completion::cancelled:  return "cancelled";
		}
		return "invalid";
	}
	auto stable_name(private_subject_availability aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case private_subject_availability::current:     return "current";
		case private_subject_availability::stale:       return "stale";
		case private_subject_availability::unplaceable: return "unplaceable";
		}
		return "invalid";
	}
	auto stable_name(private_review_refusal aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case private_review_refusal::invalid_identity:   return "invalid_identity";
		case private_review_refusal::invalid_binding:    return "invalid_binding";
		case private_review_refusal::invalid_subject:    return "invalid_subject";
		case private_review_refusal::invalid_actor:      return "invalid_actor";
		case private_review_refusal::invalid_completion: return "invalid_completion";
		case private_review_refusal::bounds:             return "bounds";
		case private_review_refusal::conflict:           return "conflict";
		case private_review_refusal::archived:           return "archived";
		case private_review_refusal::invalid_transition: return "invalid_transition";
		case private_review_refusal::missing_source:     return "missing_source";
		case private_review_refusal::wrong_scope:        return "wrong_scope";
		case private_review_refusal::digest_mismatch:    return "digest_mismatch";
		case private_review_refusal::incomplete_source:  return "incomplete_source";
		case private_review_refusal::duplicate_source:   return "duplicate_source";
		case private_review_refusal::cycle:              return "cycle";
		case private_review_refusal::ambiguous_target:   return "ambiguous_target";
		case private_review_refusal::invalid_target:     return "invalid_target";
		case private_review_refusal::source_changed:     return "source_changed";
		case private_review_refusal::invalid_provenance: return "invalid_provenance";
		case private_review_refusal::invalid_mode:       return "invalid_mode";
		}
		return "invalid";
	}
}
