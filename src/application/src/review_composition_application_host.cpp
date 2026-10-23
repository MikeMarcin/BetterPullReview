#include "bpr/application/review_composition_application_host.hpp"
#include "bpr/core/reply_draft_standing.hpp"
#include "bpr/core/review_submission.hpp"
#include "bpr/core/thread_reply.hpp"
#include "private_review_operations.hpp"
#include <algorithm>
#include <array>
#include <set>
namespace bpr::application {
	namespace {
		using namespace core;
		using namespace private_review_operations;
		auto copy_body(private_review_store_port& aStore, const private_source_reference& aSource) -> std::string
		{
			if (std::holds_alternative<private_finding_source>(aSource.Key)) {
				auto finding = take(aStore.load_finding(aSource));
				require(finding.Finding.SuggestedComment.has_value(), private_review_refusal::invalid_mode);
				return *finding.Finding.SuggestedComment;
			}
			return body(aStore, aSource);
		}
		auto prepare(private_review_store_port& aStore, const review_composition_request& aRequest, std::uint64_t& aNext, private_store_write& aWrite) -> private_composition_record
		{
			require(aNext > 0 && aNext <= maximum_private_identity - 1, private_review_refusal::bounds);
			auto observed = observe(aStore, aRequest.Sources);
			auto state    = decide(create_review_composition(review_composition_id{ aNext++ }, aRequest.Binding, aRequest.Mode, aRequest.Sources, observed, aRequest.Target));
			if (state.Target) {
				require(state.Target->Binding == state.Binding, private_review_refusal::invalid_target);
			}
			if (aRequest.Mode == review_composition_mode::exact_copy || (aRequest.Mode == review_composition_mode::reviewer_authored && !aRequest.Body.empty())) {
				auto text     = aRequest.Mode == review_composition_mode::exact_copy ? copy_body(aStore, aRequest.Sources.front().Source) : aRequest.Body;
				auto proposal = decide(propose_composition_revision(state, review_composition_revision_id{ aNext }, private_vertex_order{ aNext }, text, aRequest.Sources, observed));
				++aNext;
				state = proposal.State;
				aWrite.Candidates.push_back(std::move(proposal.Revision));
			} else {
				require(aRequest.Body.empty(), private_review_refusal::invalid_mode);
			}
			private_composition_record record{ state, 1, aRequest.Sources };
			aWrite.Compositions.push_back({ record, 0 });
			return record;
		}
		auto link(const private_source_reference& aCandidate) -> review_composition_link
		{
			const auto& key = std::get<private_composition_source>(aCandidate.Key);
			return { aCandidate.Scope, key.Composition, key.Revision, aCandidate.Digest };
		}
	}
	auto review_composition_application_host::create(const review_composition_request& aRequest) -> private_run_result<private_composition_record>
	{
		return attempt([&] {
			auto f = take(Store.frontier());
			require(f.Scope == aRequest.Binding.Scope, core::private_review_refusal::invalid_binding);
			auto                next = f.NextIdentity;
			private_store_write write{ .ExpectedNextIdentity = next };
			auto                result = prepare(Store, aRequest, next, write);
			take(Store.commit(write));
			return result;
		});
	}
	auto review_composition_application_host::split(const review_composition_request& aRequest) -> private_run_result<review_composition_split>
	{
		return attempt([&] {
			auto f = take(Store.frontier());
			require(f.Scope == aRequest.Binding.Scope && !aRequest.Target, core::private_review_refusal::invalid_binding);
			auto observed = observe(Store, aRequest.Sources);
			decide(core::admit_composition_sources(f.Scope, aRequest.Sources, observed));
			auto partitions = decide(core::partition_composition_targets(observed));
			require(partitions.Assigned.size() <= maximum_private_drafts, core::private_review_refusal::bounds);
			review_composition_split result;
			for (auto i : partitions.Unassigned) {
				result.Unassigned.push_back(aRequest.Sources[i]);
			}
			auto                next = f.NextIdentity;
			private_store_write write{ .ExpectedNextIdentity = next };
			for (const auto& partition : partitions.Assigned) {
				auto request = aRequest;
				request.Sources.clear();
				request.Target  = partition.Target;
				request.Binding = partition.Target.Binding;
				for (auto i : partition.Sources) {
					request.Sources.push_back(aRequest.Sources[i]);
				}
				result.Compositions.push_back(prepare(Store, request, next, write));
			}
			if (!write.Compositions.empty()) {
				take(Store.commit(write));
			}
			return result;
		});
	}
	auto review_composition_application_host::revise(core::review_composition_id aId, std::uint64_t aExpectedVersion, std::string aBody) -> private_run_result<private_composition_record>
	{
		return attempt([&] {
			auto f      = take(Store.frontier());
			auto record = take(Store.load_composition(aId));
			require(record.Version == aExpectedVersion);
			std::optional<core::review_composition_revision> prior;
			if (record.State.Candidate) {
				prior = take(Store.load_candidate(*record.State.Candidate));
			}
			auto observed = observe(Store, record.Sources);
			auto proposal = decide(core::propose_composition_revision(record.State, core::review_composition_revision_id{ f.NextIdentity }, core::private_vertex_order{ f.NextIdentity }, aBody, record.Sources, observed, nullptr, prior ? &*prior : nullptr));
			record.State  = proposal.State;
			++record.Version;
			take(Store.commit({ .ExpectedNextIdentity = f.NextIdentity, .Compositions = { { record, aExpectedVersion } }, .Candidates = { proposal.Revision }, .SourceGuards = record.Sources }));
			return record;
		});
	}
	auto review_composition_application_host::target(core::review_composition_id aId, std::uint64_t aExpectedVersion, const core::private_composition_target& aTarget, const core::diff_semantic_index& aIndex) -> private_run_result<private_composition_record>
	{
		return attempt([&] {
			auto f      = take(Store.frontier());
			auto record = take(Store.load_composition(aId));
			require(record.Version == aExpectedVersion && record.State.Lifecycle == core::review_composition_lifecycle::editing && aTarget.Binding == record.State.Binding);
			record.State.Target = decide(core::make_composition_target(aTarget.Binding, aTarget.Line, aIndex));
			++record.Version;
			take(Store.commit({ .ExpectedNextIdentity = f.NextIdentity, .Compositions = { { record, aExpectedVersion } }, .SourceGuards = record.Sources }));
			return record;
		});
	}
	auto review_composition_application_host::agent_intent(core::review_composition_id aId, std::uint64_t aExpectedVersion) -> private_run_result<private_run_intent>
	{
		return attempt([&] {
			auto record = take(Store.load_composition(aId));
			require(record.Version == aExpectedVersion && record.State.Mode == core::review_composition_mode::agent_assisted && record.State.Lifecycle == core::review_composition_lifecycle::editing && !record.State.Candidate);
			decide(core::admit_private_run_sources(record.State.Binding, record.Sources, observe(Store, record.Sources)));
			return private_run_intent{ .Binding = record.State.Binding, .Purpose = core::private_run_purpose::composition, .Sources = record.Sources, .Composition = aId, .ExpectedCompositionVersion = aExpectedVersion };
		});
	}
	auto review_composition_application_host::accept(std::span<const review_composition_acceptance> aRequests, const core::private_review_binding& aCurrent, const core::diff_semantic_index& aIndex, core::review_draft_scope_id aRuntimeScope) -> private_run_result<durable_draft_commit>
	{
		return accept(aRequests, std::array{ composition_target_observation{ aCurrent, aIndex } }, aRuntimeScope);
	}
	auto review_composition_application_host::accept(std::span<const review_composition_acceptance> aRequests, std::span<const composition_target_observation> aTargets, core::review_draft_scope_id aRuntimeScope) -> private_run_result<durable_draft_commit>
	{
		return attempt([&] {
			require(!aRequests.empty() && aRequests.size() <= maximum_private_drafts && aRuntimeScope.value() != 0, core::private_review_refusal::bounds);
			auto f = take(Store.frontier());
			require(!aTargets.empty() && aTargets.size() <= maximum_private_drafts, core::private_review_refusal::bounds);
			auto                 next = f.NextIdentity;
			private_store_write  write{ .ExpectedNextIdentity = next };
			durable_draft_commit result;
			result.Scope = f.Scope;
			std::set<core::review_composition_id> compositions;
			std::set<core::review_draft_id>       drafts;
			for (const auto& request : aRequests) {
				auto record = take(Store.load_composition(request.Composition));
				auto target = std::ranges::find_if(aTargets, [&](const auto& aT) { return aT.Binding == record.State.Binding; });
				require(target != aTargets.end() && target->Binding.Scope == f.Scope, core::private_review_refusal::invalid_binding);
				const auto& aCurrent = target->Binding;
				const auto& aIndex   = target->Index;
				require(compositions.insert(request.Composition).second && record.Version == request.ExpectedVersion && record.State.Candidate == request.Candidate && record.State.Binding == aCurrent && record.State.Target && record.State.Target->Binding == aCurrent);
				auto                                         candidate = take(Store.load_candidate(request.Candidate));
				std::optional<core::revisioned_review_draft> existing;
				if (request.Draft) {
					auto draft = take(Store.load_draft(*request.Draft));
					require(draft.Binding == aCurrent && draft.Draft.Revision == request.ExpectedDraftRevision && drafts.insert(*request.Draft).second);
					existing        = draft.Draft;
					existing->Scope = aRuntimeScope;
				} else {
					require(request.ExpectedDraftRevision == 0);
				}
				auto                          accepted = decide(core::accept_composition_as_draft(record.State, candidate, record.Sources, observe(Store, record.Sources), aIndex, existing));
				core::revisioned_review_draft draft;
				if (existing) {
					auto changed = core::apply(*existing, { .Command = { existing->Draft.Id, core::review_draft_command_kind::edit, accepted.Body }, .ExpectedRevision = existing->Revision, .Scope = aRuntimeScope, .Composition = link(candidate.Identity) });
					require(changed.Refusal == core::revisioned_draft_refusal::none && changed.Changed);
					draft = changed.Accepted;
				} else {
					require(next > 0 && next <= core::maximum_private_identity, core::private_review_refusal::bounds);
					draft = { { core::review_draft_id{ next++ }, accepted.Target.Line, accepted.Body, core::review_draft_state::drafting, { } }, 1, { }, link(candidate.Identity) };
				}
				draft.Scope = { };
				result.Records.push_back({ aCurrent, draft });
				record.State = decide(core::acknowledge_composition_draft(record.State, candidate.Identity));
				++record.Version;
				write.Compositions.push_back({ record, request.ExpectedVersion });
				write.Drafts.push_back({ result.Records.back(), request.ExpectedDraftRevision });
			}
			take(Store.commit(write));
			return result;
		});
	}
	auto review_composition_application_host::allocate_draft(const core::private_composition_target& aTarget, const core::diff_semantic_index& aIndex, std::string aBody) -> private_run_result<durable_draft_commit>
	{
		return attempt([&] {
			auto f = take(Store.frontier());
			require(f.Scope == aTarget.Binding.Scope && aBody.size() <= core::maximum_shared_draft_body_bytes && f.NextIdentity != 0, core::private_review_refusal::bounds);
			decide(core::make_composition_target(aTarget.Binding, aTarget.Line, aIndex));
			private_draft_record record{ aTarget.Binding, { { core::review_draft_id{ f.NextIdentity }, aTarget.Line, std::move(aBody), core::review_draft_state::drafting, { } }, 1, { }, { } } };
			take(Store.commit({ .ExpectedNextIdentity = f.NextIdentity, .Drafts = { { record, 0 } } }));
			durable_draft_commit result;
			result.Scope = f.Scope;
			result.Records.push_back(std::move(record));
			return result;
		});
	}
	auto review_composition_application_host::allocate_reply(const core::private_review_binding& aBinding, core::source_line_ref aLine, const core::diff_semantic_index& aIndex, std::string aThread) -> private_run_result<durable_draft_commit>
	{
		return attempt([&] {
			const auto f = take(Store.frontier());
			require(f.Scope == aBinding.Scope && f.NextIdentity != 0 && !aThread.empty() && aThread.size() <= core::maximum_shared_draft_reply_bytes, core::private_review_refusal::bounds);
			require(core::valid_private_subject({ aBinding, aLine }, &aIndex), core::private_review_refusal::invalid_target);
			private_draft_record record{ aBinding, { { core::review_draft_id{ f.NextIdentity }, aLine, { }, core::review_draft_state::drafting, std::move(aThread) }, 1, { }, { } } };
			take(Store.commit({ .ExpectedNextIdentity = f.NextIdentity, .Drafts = { { record, 0 } } }));
			durable_draft_commit result;
			result.Scope = f.Scope;
			result.Records.push_back(std::move(record));
			return result;
		});
	}
	auto review_composition_application_host::select_thread_reply(const core::private_review_binding& aBinding, std::string aThread) -> private_run_result<durable_draft_commit>
	{
		return attempt([&] {
			const auto f = take(Store.frontier());
			require(f.Scope == aBinding.Scope && f.NextIdentity != 0 && !aThread.empty() && aThread.size() <= core::maximum_shared_draft_reply_bytes && aBinding.Comparison.has_value(),
					core::private_review_refusal::bounds);
			auto records = take(Store.list_drafts());
			std::erase_if(records, [&](const auto& aRecord) { return aRecord.Binding.Revision.HeadOid != aBinding.Revision.HeadOid; });
			auto drafts = std::vector<review_comment_draft>{ };
			for (const auto& record : records) {
				drafts.push_back(record.Draft.Draft);
			}
			durable_draft_commit result;
			result.Scope = f.Scope;
			if (const auto saved = core::reopen_thread_reply(drafts, aThread)) {
				const auto found = std::ranges::find_if(records, [&](const auto& aRecord) { return aRecord.Draft.Draft.Id == *saved; });
				result.Records.push_back(*found);
				return result;
			}
			private_draft_record record{ aBinding, { { core::review_draft_id{ f.NextIdentity }, core::thread_reply_anchor(*aBinding.Comparison), { }, core::review_draft_state::drafting, std::move(aThread) }, 1, { }, { } } };
			take(Store.commit({ .ExpectedNextIdentity = f.NextIdentity, .Drafts = { { record, 0 } } }));
			result.Records.push_back(std::move(record));
			return result;
		});
	}
	auto review_composition_application_host::carry_thread_replies(const core::private_review_binding& aBinding) -> private_run_result<durable_draft_commit>
	{
		return attempt([&] {
			const auto f = take(Store.frontier());
			require(f.Scope == aBinding.Scope && f.NextIdentity != 0, core::private_review_refusal::bounds);
			durable_draft_commit result;
			result.Scope = f.Scope;
			if (!aBinding.Comparison.has_value()) {
				return result;
			}
			const auto records = take(Store.list_drafts());
			const auto here    = [&](const std::string& aThread) {
				return std::ranges::any_of(records, [&](const auto& aRecord) {
					return aRecord.Binding.Revision.HeadOid == aBinding.Revision.HeadOid && core::thread_bound_reply(aRecord.Draft.Draft)
						&& *aRecord.Draft.Draft.ReplyToThread == aThread
						&& (aRecord.Draft.Draft.State == core::review_draft_state::drafting || aRecord.Draft.Draft.State == core::review_draft_state::publishing);
				});
			};
			auto write = private_store_write{ .ExpectedNextIdentity = f.NextIdentity };
			auto next  = f.NextIdentity;
			auto taken = std::set<std::string>{ };
			for (const auto& record : records) {
				const auto& draft = record.Draft.Draft;
				if (record.Binding.Revision.HeadOid == aBinding.Revision.HeadOid || !core::thread_bound_reply(draft) || draft.State != core::review_draft_state::drafting
					|| here(*draft.ReplyToThread) || !taken.insert(*draft.ReplyToThread).second) {
					continue;
				}
				require(next < maximum_private_identity, core::private_review_refusal::bounds);
				// The reducer works in a runtime scope; the store keeps none.
				auto held            = record.Draft;
				held.Scope           = core::review_draft_scope_id{ 1 };
				const auto discarded = core::apply(held, core::revisioned_review_draft_command{ .Command = { .Id = draft.Id, .Kind = core::review_draft_command_kind::discard }, .ExpectedRevision = record.Draft.Revision, .Scope = held.Scope });
				require(discarded.Refusal == core::revisioned_draft_refusal::none && discarded.Changed);
				auto earlier        = record;
				earlier.Draft       = discarded.Accepted;
				earlier.Draft.Scope = { };
				write.Drafts.push_back({ std::move(earlier), record.Draft.Revision });
				private_draft_record carried{ aBinding, { { core::review_draft_id{ next++ }, core::thread_reply_anchor(*aBinding.Comparison), draft.Body, core::review_draft_state::drafting, draft.ReplyToThread }, 1, { }, { } } };
				result.Records.push_back(carried);
				write.Drafts.push_back({ std::move(carried), 0 });
			}
			if (!write.Drafts.empty()) {
				take(Store.commit(write));
			}
			return result;
		});
	}
	auto review_composition_application_host::clear_unused_drafts() -> private_run_result<durable_draft_commit>
	{
		return attempt([&] {
			auto                frontier = take(Store.frontier());
			auto                records  = take(Store.list_drafts());
			private_store_write write;
			write.ExpectedNextIdentity = frontier.NextIdentity;
			for (const auto& record : records) {
				if (core::unused_review_draft(record.Draft)) {
					write.RemoveUnusedDrafts.push_back(record.Draft.Draft.Id);
				}
			}
			if (!write.RemoveUnusedDrafts.empty()) {
				take(Store.commit(write));
			}
			durable_draft_commit result;
			result.Scope         = frontier.Scope;
			result.RemovedUnused = std::move(write.RemoveUnusedDrafts);
			result.UnusedBefore  = frontier.NextIdentity ? frontier.NextIdentity : core::maximum_private_identity + 1U;
			return result;
		});
	}
	auto review_composition_application_host::restore(std::optional<core::git_commit_oid> aHead, std::optional<core::review_draft_id> aDraft) -> private_run_result<durable_draft_commit>
	{
		return attempt([&] {
			durable_draft_commit result;
			result.Scope   = take(Store.frontier()).Scope;
			result.Records = take(Store.list_drafts());
			for (const auto& record : result.Records) {
				if (core::held_by_review(record.Draft.Draft)) {
					const auto& key = record.Draft.Draft.InReview->Submission;
					if (std::ranges::find(result.HeldReviews, key) == result.HeldReviews.end()) {
						result.HeldReviews.push_back(key);
					}
				}
			}
			std::erase_if(result.Records, [&](const auto& aRecord) { return (aHead && aRecord.Binding.Revision.HeadOid != *aHead) || (aDraft && aRecord.Draft.Draft.Id != *aDraft); });
			return result;
		});
	}
	auto review_composition_application_host::select_draft(const core::private_review_binding& aBinding, const core::source_line_ref aLine, const core::diff_semantic_index& aIndex, std::optional<std::string> aReplyThread, const bool aAllocate) -> private_run_result<durable_draft_commit>
	{
		auto reopened = attempt([&] {
			auto drafts = std::vector<review_comment_draft>{ };
			for (const auto& record : take(Store.list_drafts())) {
				if (record.Binding == aBinding && (aAllocate || !core::unused_review_draft(record.Draft))) {
					drafts.push_back(record.Draft.Draft);
				}
			}
			return reopen_draft_at(drafts, aLine, aReplyThread);
		});
		if (!reopened) {
			return std::unexpected(reopened.error());
		}
		if (*reopened) {
			return restore(aBinding.Revision.HeadOid, *reopened);
		}
		if (aReplyThread) {
			return allocate_reply(aBinding, aLine, aIndex, std::move(*aReplyThread));
		}
		if (!aAllocate) {
			return restore(aBinding.Revision.HeadOid, core::review_draft_id{ });
		}
		const auto target = make_composition_target(aBinding, aLine, aIndex);
		if (!target) {
			return std::unexpected(private_run_error{ .Policy = target.error() });
		}
		return allocate_draft(*target, aIndex);
	}
	auto review_composition_application_host::materialize_draft(const core::private_review_binding& aBinding, const core::diff_semantic_index& aIndex, const core::revisioned_review_draft& aPlaceholder, const core::revisioned_review_draft_command& aCommand) -> private_run_result<durable_draft_commit>
	{
		return attempt([&] {
			require(aPlaceholder.Draft.Id.value() > core::maximum_private_identity && core::unused_review_draft(aPlaceholder) && aPlaceholder.Scope.value() != 0 && aCommand.Command.Kind == core::review_draft_command_kind::edit && aCommand.Command.Id == aPlaceholder.Draft.Id && aCommand.ExpectedRevision == aPlaceholder.Revision && aCommand.Scope == aPlaceholder.Scope && !aCommand.ChangeReplyTarget && !aCommand.Composition, core::private_review_refusal::invalid_target);
			auto frontier = take(Store.frontier());
			require(frontier.Scope == aBinding.Scope);
			decide(core::make_composition_target(aBinding, aPlaceholder.Draft.Anchor, aIndex));
			auto initial           = private_draft_record{ aBinding, aPlaceholder };
			initial.Draft.Draft.Id = core::review_draft_id{ frontier.NextIdentity };
			initial.Draft.Scope    = { };
			auto current           = initial.Draft;
			current.Scope          = aPlaceholder.Scope;
			auto command           = aCommand;
			command.Command.Id     = current.Draft.Id;
			auto changed           = core::apply(current, command);
			require(changed.Refusal == core::revisioned_draft_refusal::none && changed.Changed);
			auto accepted        = initial;
			accepted.Draft       = changed.Accepted;
			accepted.Draft.Scope = { };
			private_store_write write;
			write.ExpectedNextIdentity = frontier.NextIdentity;
			write.Drafts.push_back({ initial, 0 });
			if (changed.Changed) {
				write.Drafts.push_back({ accepted, 1 });
			}
			take(Store.commit(write));
			durable_draft_commit result;
			result.Scope = frontier.Scope;
			result.Records.push_back(std::move(accepted));
			return result;
		});
	}
	auto review_composition_application_host::change_draft(const core::revisioned_review_draft_command& aCommand) -> private_run_result<durable_draft_commit>
	{
		return attempt([&] {
			auto f      = take(Store.frontier());
			auto record = take(Store.load_draft(aCommand.Command.Id));
			require(record.Draft.Revision == aCommand.ExpectedRevision && aCommand.Scope.value() != 0);
			record.Draft.Scope = aCommand.Scope;
			auto command       = aCommand;
			require(!command.Composition, core::private_review_refusal::invalid_provenance);
			private_store_write write{ .ExpectedNextIdentity = f.NextIdentity };
			if (record.Draft.Composition && command.Command.Kind == core::review_draft_command_kind::edit && command.Command.Body != record.Draft.Draft.Body) {
				require(!command.ChangeReplyTarget);
				const auto& old         = *record.Draft.Composition;
				auto        composition = take(Store.load_composition(old.Composition));
				require(composition.State.Candidate && link(*composition.State.Candidate) == old && composition.State.Binding == record.Binding);
				auto prior          = take(Store.load_candidate(*composition.State.Candidate));
				auto state          = decide(core::reopen_review_composition(composition.State));
				auto proposal       = decide(core::propose_composition_revision(state, core::review_composition_revision_id{ f.NextIdentity }, core::private_vertex_order{ f.NextIdentity }, command.Command.Body, composition.Sources, observe(Store, composition.Sources), nullptr, &prior));
				command.Composition = link(proposal.Revision.Identity);
				composition.State   = decide(core::acknowledge_composition_draft(proposal.State, proposal.Revision.Identity));
				write.Compositions.push_back({ composition, composition.Version });
				++write.Compositions.back().Record.Version;
				write.Candidates.push_back(std::move(proposal.Revision));
			}
			auto changed = core::apply(record.Draft, command);
			require(changed.Refusal == core::revisioned_draft_refusal::none);
			record.Draft       = changed.Accepted;
			record.Draft.Scope = { };
			if (changed.Changed) {
				write.Drafts.push_back({ record, aCommand.ExpectedRevision });
				take(Store.commit(write));
			}
			durable_draft_commit result;
			result.Scope = f.Scope;
			result.Records.push_back(record);
			return result;
		});
	}
	auto review_composition_application_host::settle_review(std::string_view aKey, bool aPublished, const std::optional<core::review_submission_proposal>& aProposal) -> private_run_result<durable_draft_commit>
	{
		return attempt([&] {
			auto frontier = take(Store.frontier());
			require(!aKey.empty() && aKey.size() <= 256 && (!aPublished || aProposal));
			if (aProposal) {
				const auto& target = aProposal->Target;
				require(target.Host == frontier.Scope.Host && target.Repository == frontier.Scope.Repository && target.PullRequest == frontier.Scope.PullRequest && aProposal->ActingAccount == frontier.Scope.Account, core::private_review_refusal::invalid_binding);
			}
			private_store_write  write{ .ExpectedNextIdentity = frontier.NextIdentity };
			durable_draft_commit result;
			result.Scope = frontier.Scope;
			for (auto record : take(Store.list_drafts())) {
				const auto revision = record.Draft.Revision;
				record.Draft.Scope  = core::review_draft_scope_id{ 1 };
				const auto command  = core::settle_held_review_draft(record.Draft, aKey, aPublished, aProposal ? &*aProposal : nullptr);
				if (!command) {
					continue;
				}
				const auto changed = core::apply(record.Draft, *command);
				require(changed.Refusal == core::revisioned_draft_refusal::none && changed.Changed);
				record.Draft       = changed.Accepted;
				record.Draft.Scope = { };
				write.Drafts.push_back({ record, revision });
				result.Records.push_back(std::move(record));
			}
			if (!write.Drafts.empty()) {
				take(Store.commit(write));
			}
			return result;
		});
	}
	auto review_composition_application_host::change_drafts(std::span<const core::revisioned_review_draft_command> aCommands) -> private_run_result<durable_draft_commit>
	{
		return attempt([&] {
			require(!aCommands.empty() && aCommands.size() <= core::maximum_review_submission_comments);
			auto                 f = take(Store.frontier());
			private_store_write  write{ .ExpectedNextIdentity = f.NextIdentity };
			durable_draft_commit result;
			result.Scope = f.Scope;
			for (const auto& command : aCommands) {
				require(command.Scope.value() != 0 && !command.Composition && !command.ChangeReplyTarget);
				require(std::ranges::none_of(aCommands.first(static_cast<std::size_t>(&command - aCommands.data())), [&command](const auto& aEarlier) { return aEarlier.Command.Id == command.Command.Id; }));
				auto record = take(Store.load_draft(command.Command.Id));
				require(record.Draft.Revision == command.ExpectedRevision);
				// A linked draft's prose changes only through change_draft.
				require(!record.Draft.Composition || command.Command.Kind != core::review_draft_command_kind::edit || command.Command.Body == record.Draft.Draft.Body);
				record.Draft.Scope = command.Scope;
				auto changed       = core::apply(record.Draft, command);
				require(changed.Refusal == core::revisioned_draft_refusal::none);
				record.Draft       = changed.Accepted;
				record.Draft.Scope = { };
				if (changed.Changed) {
					write.Drafts.push_back({ record, command.ExpectedRevision });
				}
				result.Records.push_back(record);
			}
			if (!write.Drafts.empty()) {
				take(Store.commit(write));
			}
			return result;
		});
	}
}
