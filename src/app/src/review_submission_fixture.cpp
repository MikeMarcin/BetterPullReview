#include "review_submission_fixture.hpp"

#include <algorithm>

namespace bpr::app::detail {
	namespace {
		class fixture_provider final : public application::review_submission_port
		{
		public:
			fixture_provider(std::shared_ptr<review_submission_fixture_state>              aState,
							 std::unique_ptr<application::review_submission_recovery_port> aStore,
							 application::review_submission_live_binding                   aBinding)
				: State{ std::move(aState) }
				, Store{ std::move(aStore) }
				, Binding{ std::move(aBinding) }
			{ }
			auto pending(std::stop_token aStop) -> application::review_submission_discovery override
			{
				if (aStop.stop_requested()) {
					return std::unexpected(application::review_mutation_failure::cancelled);
				}
				auto records = Store->pending();
				if (!records) {
					return std::unexpected(application::review_mutation_failure::unavailable);
				}
				std::vector<application::review_submission_request> result;
				for (const auto& record : *records) {
					result.push_back(record.Request);
				}
				return result;
			}
			auto submit_and_reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result override
			{
				if (aStop.stop_requested() || !aRequest.LiveBinding || *aRequest.LiveBinding != Binding) {
					return { .Request = aRequest, .Publication = application::review_submission_publication::definitely_not_published };
				}
				std::scoped_lock lock{ State->Mutex };
				if (State->RefuseBeforeSend) {
					return { .Request = aRequest, .Publication = application::review_submission_publication::definitely_not_published };
				}
				auto known = Store->load(aRequest.IdempotencyKey);
				if (!known) {
					return { .Request = aRequest };
				}
				if (*known) {
					return reconcile_locked(aRequest);
				}
				std::vector<application::review_database_id> baseline;
				for (std::size_t index = 0; index < State->Reviews.size(); ++index) {
					baseline.emplace_back(index + 1U);
				}
				auto record = application::review_submission_recovery_record{
					.Request              = aRequest,
					.RequestFingerprint   = application::review_submission_request_fingerprint(aRequest),
					.AuthorityFingerprint = application::review_submission_authority_fingerprint(*aRequest.LiveBinding),
					.BaselineReviewIds    = std::move(baseline),
					.Transmission         = application::review_mutation_recovery_transmission::prepared
				};
				auto admitted = Store->admit(record);
				if (!admitted) {
					return { .Request = aRequest };
				}
				if (!admitted->Inserted) {
					return reconcile_locked(aRequest);
				}
				if (!Store->mark_possibly_transmitted(aRequest.IdempotencyKey, record.RequestFingerprint)) {
					return { .Request = aRequest };
				}
				State->Reviews.push_back(aRequest);
				++State->Submissions;
				if (State->LoseNextResponse) {
					State->LoseNextResponse = false;
					return { .Request = aRequest };
				}
				auto result = reconcile_locked(aRequest);
				if (State->StaleNextResult) {
					State->StaleNextResult          = false;
					result.Request.Proposal.Attempt = core::review_submission_attempt_id{ aRequest.Proposal.Attempt.value() + 1U };
				}
				return result;
			}
			auto reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result override
			{
				if (aStop.stop_requested()) {
					return { .Request = aRequest };
				}
				std::scoped_lock lock{ State->Mutex };
				++State->Reconciliations;
				return reconcile_locked(aRequest);
			}

		private:
			auto reconcile_locked(const application::review_submission_request& aRequest) -> application::review_submission_result
			{
				auto loaded = Store->load(aRequest.IdempotencyKey);
				if (!loaded || !*loaded || (**loaded).Request != aRequest || !aRequest.LiveBinding || !application::same_review_submission_identity(*aRequest.LiveBinding, Binding)) {
					return { .Request = aRequest };
				}
				const auto& record = **loaded;
				if (record.Receipt) {
					return { .Request = aRequest, .Publication = application::review_submission_publication::published, .Receipt = record.Receipt };
				}
				if (record.Transmission != application::review_mutation_recovery_transmission::possibly_transmitted) {
					return { .Request = aRequest };
				}
				const auto found = std::ranges::find(State->Reviews, aRequest);
				if (found == State->Reviews.end()) {
					return { .Request = aRequest };
				}
				const auto id      = static_cast<std::uint64_t>(std::distance(State->Reviews.begin(), found)) + 1U;
				auto       receipt = application::review_submission_receipt{
					.ReviewDatabaseId = application::review_database_id{ id }, .ReviewNodeId = "PRR_fixture_" + std::to_string(id), .Target = aRequest.Proposal.Target, .ActingAccount = aRequest.Proposal.ActingAccount, .Disposition = aRequest.Proposal.Disposition
				};
				auto resolved = Store->resolve(aRequest.IdempotencyKey, record.RequestFingerprint, receipt);
				return resolved ? application::review_submission_result{ .Request = aRequest, .Publication = application::review_submission_publication::published, .Receipt = receipt } : application::review_submission_result{ .Request = aRequest };
			}
			std::shared_ptr<review_submission_fixture_state>              State;
			std::unique_ptr<application::review_submission_recovery_port> Store;
			application::review_submission_live_binding                   Binding;
		};
	}
	auto make_review_submission_fixture_provider(std::shared_ptr<review_submission_fixture_state>              aState,
												 std::unique_ptr<application::review_submission_recovery_port> aStore,
												 application::review_submission_live_binding                   aBinding) -> std::unique_ptr<application::review_submission_port>
	{
		return std::make_unique<fixture_provider>(std::move(aState), std::move(aStore), std::move(aBinding));
	}
}
