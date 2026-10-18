#include "bpr/application/private_publication_application_host.hpp"
#include "bpr/application/review_mutation_coordinator.hpp"
#include "private_review_operations.hpp"
#include <algorithm>
namespace bpr::application {
	namespace {
		using namespace private_review_operations;
		auto representation(core::review_publication_representation aValue) -> std::string
		{
			require(aValue <= core::review_publication_representation::file_comment_with_permalink);
			return aValue == core::review_publication_representation::native_line ? "native_line" : "file_comment_with_permalink";
		}
		void validate(const review_mutation_request& aRequest, const private_draft_record& aDraft)
		{
			const auto& scope = aDraft.Binding.Scope;
			require(aRequest.Proposal.Durable && aRequest.LiveBinding && valid(*aRequest.LiveBinding));
			require(aRequest.Proposal.Durable->Incarnation == scope.Incarnation.bytes() && aRequest.Proposal.Durable->DraftRevision == aDraft.Draft.Revision);
			require(aRequest.Proposal.Draft == aDraft.Draft.Draft.Id && aRequest.Proposal.Body == aDraft.Draft.Draft.Body && aRequest.Proposal.Anchor == aDraft.Draft.Draft.Anchor && !aRequest.Proposal.ReplyToThread && !aDraft.Draft.Draft.ReplyToThread && aRequest.Proposal.Disposition == core::review_disposition::comment);
			require(aRequest.Target == aRequest.LiveBinding->Target.Semantic && aRequest.ActingIdentity == scope.Account && aRequest.LiveBinding->ActingIdentity.Account == scope.Account && aRequest.Target.Host == scope.Host && aRequest.Target.Repository == scope.Repository && aRequest.Target.PullRequest == scope.PullRequest && aRequest.Target.ExpectedHead == aDraft.Binding.Revision.HeadOid && aRequest.LiveBinding->Anchor.Commit == aDraft.Binding.Revision.HeadOid && aRequest.LiveBinding->Anchor.Semantic == aDraft.Draft.Draft.Anchor);
			require(aRequest.Source && aRequest.Source->Comparison.Authority.HostAuthority == aDraft.Binding.HostAuthority && aRequest.Source->Comparison.LeftAnchor == aDraft.Binding.LeftAnchor && aRequest.Source->Comparison.RightAnchor == aDraft.Binding.RightAnchor && core::comparison_digest(aRequest.Source->Comparison) == aDraft.Draft.Draft.Anchor.Comparison);
			require(aRequest.IdempotencyKey == idempotency_key_for(aRequest.Proposal, aRequest.Target));
		}
		void match(const private_publication_record& aRecord, const review_mutation_request& aRequest, std::string_view aLogical)
		{
			require(aRequest.Proposal.Durable && aRequest.LiveBinding && valid(*aRequest.LiveBinding));
			require(aRecord.LogicalPublication == aLogical && aRecord.IdempotencyKey == aRequest.IdempotencyKey && aRecord.Draft == aRequest.Proposal.Draft && aRecord.DraftRevision == aRequest.Proposal.Durable->DraftRevision && aRecord.Target.Binding.Scope.Incarnation.bytes() == aRequest.Proposal.Durable->Incarnation && aRecord.ApprovedProseDigest == core::private_digest(aRequest.Proposal.Body));
			require(aRecord.Target.Line == aRequest.Proposal.Anchor && aRecord.Target.Binding.Scope.Host == aRequest.Target.Host && aRecord.Target.Binding.Scope.Repository == aRequest.Target.Repository && aRecord.Target.Binding.Scope.PullRequest == aRequest.Target.PullRequest && aRecord.Target.Binding.Revision.HeadOid == aRequest.Target.ExpectedHead && aRecord.AcceptedHead == aRequest.LiveBinding->Anchor.Commit && aRecord.Path == aRequest.LiveBinding->Anchor.Path);
			require(aRecord.Actor == aRequest.ActingIdentity && aRecord.ActorNode == aRequest.LiveBinding->ActingIdentity.NodeId && aRecord.ActorDatabaseId == aRequest.LiveBinding->ActingIdentity.DatabaseId && aRecord.ActorLogin == aRequest.LiveBinding->ActingIdentity.Login);
		}
		void commit_receipt(private_review_store_port& aStore, private_publication_record aRecord, const review_mutation_receipt& aReceipt)
		{
			auto f = take(aStore.frontier());
			if (aRecord.ServerComment) {
				require(*aRecord.ServerComment == aReceipt.ServerCommentId && aRecord.ReconciledIdentity == aRecord.ServerComment);
				return;
			}
			aRecord.ServerComment      = aReceipt.ServerCommentId;
			aRecord.ReconciledIdentity = aReceipt.ServerCommentId;
			aRecord.ReceiptReserve     = 0;
			private_store_write write{ .ExpectedNextIdentity = f.NextIdentity, .Publications = { aRecord } };
			auto                draft = take(aStore.load_draft(aRecord.Draft));
			if (draft.Draft.Revision == aRecord.DraftRevision && draft.Draft.Draft.State == core::review_draft_state::publishing) {
				draft.Draft.Scope = core::review_draft_scope_id{ 1 };
				auto changed      = core::apply(draft.Draft, { { aRecord.Draft, core::review_draft_command_kind::complete_publish, { } }, aRecord.DraftRevision, false, { }, draft.Draft.Scope });
				require(changed.Refusal == core::revisioned_draft_refusal::none && changed.Changed);
				draft.Draft       = changed.Accepted;
				draft.Draft.Scope = { };
				write.Drafts.push_back({ draft, aRecord.DraftRevision });
			}
			take(aStore.commit(write));
		}
	}
	auto private_publication_application_host::proposal(core::review_draft_id aId, std::uint64_t aExpectedRevision) -> private_run_result<core::review_publication_proposal>
	{
		return attempt([&] {
			auto draft = take(Store.load_draft(aId));
			require(draft.Draft.Revision == aExpectedRevision && !draft.Draft.Draft.ReplyToThread);
			auto result = core::propose_publication(draft.Draft.Draft, core::review_disposition::comment);
			require(result.has_value());
			result->Durable = core::durable_review_publication_identity{ draft.Binding.Scope.Incarnation.bytes(), draft.Draft.Revision };
			return *result;
		});
	}
	auto private_publication_application_host::prepare(const review_mutation_request& aRequest, std::string_view aLogical, std::string_view aWireBody) -> bool
	{
		auto result = attempt([&] {
			auto                       f     = take(Store.frontier());
			auto                       prior = take(Store.lookup_publication(aLogical));
			private_publication_record record;
			if (prior) {
				match(*prior, aRequest, aLogical);
				record = *prior;
				if (record.ServerComment) {
					require(record.Representation == representation(aRequest.Representation) && record.WireBodyDigest == core::private_digest(aWireBody));
					return true;
				}
			} else {
				auto draft = take(Store.load_draft(aRequest.Proposal.Draft));
				validate(aRequest, draft);
				require(draft.Draft.Draft.State == core::review_draft_state::publishing);
				record = { .Id = core::review_publication_lineage_id{ f.NextIdentity }, .Draft = draft.Draft.Draft.Id, .DraftRevision = draft.Draft.Revision, .Composition = draft.Draft.Composition, .Target = { draft.Binding, draft.Draft.Draft.Anchor }, .Actor = aRequest.ActingIdentity, .ApprovedProseDigest = core::private_digest(aRequest.Proposal.Body), .LogicalPublication = std::string{ aLogical }, .IdempotencyKey = aRequest.IdempotencyKey, .ReceiptReserve = 4096, .ActorNode = aRequest.LiveBinding->ActingIdentity.NodeId, .ActorDatabaseId = aRequest.LiveBinding->ActingIdentity.DatabaseId, .ActorLogin = aRequest.LiveBinding->ActingIdentity.Login, .Path = aRequest.LiveBinding->Anchor.Path, .AcceptedHead = aRequest.LiveBinding->Anchor.Commit };
			}
			record.Representation = representation(aRequest.Representation);
			record.WireBodyDigest = core::private_digest(aWireBody);
			take(Store.commit({ .ExpectedNextIdentity = f.NextIdentity, .Publications = { record } }));
			return true;
		});
		Error       = result ? std::nullopt : std::optional{ result.error() };
		return result.has_value();
	}
	auto private_publication_application_host::complete(const review_mutation_request& aRequest, std::string_view aLogical, std::string_view aWireBody, const review_mutation_receipt& aReceipt) -> bool
	{
		auto result = attempt([&] {
			auto prior = take(Store.lookup_publication(aLogical));
			require(prior.has_value());
			match(*prior, aRequest, aLogical);
			require(aReceipt.Representation == aRequest.Representation && prior->Representation == representation(aReceipt.Representation) && prior->WireBodyDigest == core::private_digest(aWireBody) && !aReceipt.ServerCommentId.empty());
			commit_receipt(Store, *prior, aReceipt);
			return true;
		});
		Error       = result ? std::nullopt : std::optional{ result.error() };
		return result.has_value();
	}
	auto private_publication_application_host::complete_observed(const private_publication_record& aRecord, std::string_view aWireBody, const review_mutation_receipt& aReceipt) -> bool
	{
		auto result = attempt([&] {
			auto prior = take(Store.lookup_publication(aRecord.LogicalPublication));
			require(prior && *prior == aRecord && !aReceipt.ServerCommentId.empty()
					&& prior->WireBodyDigest == core::private_digest(aWireBody)
					&& prior->Representation == representation(aReceipt.Representation));
			commit_receipt(Store, *prior, aReceipt);
			return true;
		});
		Error       = result ? std::nullopt : std::optional{ result.error() };
		return result.has_value();
	}
	auto private_publication_application_host::pending(const core::review_mutation_target& aTarget, core::github_account_id aActor) -> std::vector<private_publication_record>
	{
		auto result = attempt([&] {
			std::vector<private_publication_record>   rows;
			std::optional<private_publication_cursor> cursor;
			for (;;) {
				auto page = take(Store.find_publication({ }, cursor));
				for (auto& row : page.Records) {
					const auto& scope = row.Target.Binding.Scope;
					if (!row.ServerComment && scope.Host == aTarget.Host && scope.Repository == aTarget.Repository
						&& scope.PullRequest == aTarget.PullRequest && row.Actor == aActor) {
						rows.push_back(std::move(row));
					}
				}
				if (!page.Next || rows.size() > maximum_private_drafts) {
					return rows;
				}
				cursor = page.Next;
			}
		});
		return result ? std::move(*result) : std::vector<private_publication_record>{ };
	}
	auto private_publication_application_host::for_draft(core::review_draft_id aDraft, std::optional<private_publication_cursor> aCursor) -> private_run_result<private_publication_page>
	{
		return attempt([&] {
			require(core::valid_private_id(aDraft));
			auto page = take(Store.find_publication({ }, aCursor));
			std::erase_if(page.Records, [&](const auto& aRow) { return aRow.Draft != aDraft; });
			return page;
		});
	}
	auto private_publication_application_host::duplicates(core::review_composition_id aId, std::optional<private_publication_cursor> aCursor) -> private_run_result<private_publication_page>
	{
		return attempt([&] {
			auto composition = take(Store.load_composition(aId));
			require(composition.State.Candidate.has_value());
			auto candidate = take(Store.load_candidate(*composition.State.Candidate));
			auto page      = take(Store.find_publication({ }, aCursor));
			std::erase_if(page.Records, [&](const auto& aRow) {
				if (!aRow.ServerComment || !aRow.Composition) {
					return true;
				}
				auto       prior = take(Store.load_composition(aRow.Composition->Composition));
				std::array observed{ core::composition_publication_observation{ aRow.Target.Binding.Scope, prior.State.SourceSetDigest, aRow.ApprovedProseDigest, aRow.Target } };
				return !core::duplicate_composition_publication(composition.State, candidate, observed);
			});
			return page;
		});
	}
}
