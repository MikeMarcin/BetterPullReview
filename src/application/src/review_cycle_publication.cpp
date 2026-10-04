#include "bpr/application/review_cycle_publication.hpp"

#include "bpr/core/sha256.hpp"

#include <array>
#include <string_view>
#include <utility>

namespace bpr::application {
	namespace {
		auto failed(const cycle_store_failure aFailure) -> std::unexpected<cycle_store_error>
		{
			return std::unexpected(cycle_store_error{ .Failure = aFailure });
		}

		/// The operation identity of one restart step: the same publication at
		/// the same revision always gets the same one, so a repeated recovery
		/// replays instead of stacking.
		auto restart_operation(const core::local_cycle_subject& aSubject, const core::cycle_publication_id aPublication, const std::uint64_t aRevision) -> cycle_operation_id
		{
			auto bytes = std::vector<std::uint8_t>{ };
			for (const auto character : std::string_view{ "bpr.publication.restart.v1" }) {
				bytes.push_back(static_cast<std::uint8_t>(character));
			}
			const auto append = [&bytes](const std::uint64_t aValue) {
				for (auto shift = 56; shift >= 0; shift -= 8) {
					bytes.push_back(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
				}
			};
			append(aSubject.Authority.Profile.value());
			bytes.insert(bytes.end(), aSubject.Authority.Incarnation.bytes().begin(), aSubject.Authority.Incarnation.bytes().end());
			append(aSubject.Cycle.value());
			append(aSubject.Generation);
			append(aPublication.value());
			append(aRevision);
			return cycle_operation_id{ core::sha256(bytes) };
		}
	}

	review_cycle_publication_host::review_cycle_publication_host(review_cycle_outgoing_host& aOutgoing, review_cycle_store& aStore) noexcept
		: Outgoing{ aOutgoing }
		, Store{ aStore }
	{
	}

	auto review_cycle_publication_host::prepare(const core::review_cycle_state& aCycle, const cycle_operation_id& aOperation, const cycle_publication_request& aRequest, std::stop_token aStop)
		-> cycle_publication_preparation
	{
		auto result = cycle_publication_preparation{ .Built = Outgoing.build(aCycle, aRequest.Candidate, aStop) };
		if (!result.Built.Candidate || !result.Built.Outcome.Plan) {
			return result;
		}
		auto plan = core::make_publication_plan(*result.Built.Outcome.Plan, *result.Built.Candidate, aRequest.Destination, aRequest.PullTitle, aRequest.PullBody, aRequest.Draft);
		if (!plan) {
			result.Refusal = plan.error();
			return result;
		}
		const auto recorded = Store.record_publication(aOperation, aCycle.Subject, *plan, aStop);
		if (!recorded) {
			result.Store = recorded.error();
			return result;
		}
		auto stored = load(aCycle.Subject, core::cycle_publication_id{ recorded->Allocated }, aStop);
		if (!stored) {
			result.Store = stored.error();
			return result;
		}
		result.Preview     = result.Built.Outcome.Preview + "\n" + core::render_publication_preview(stored->Plan);
		result.Publication = std::move(*stored);
		return result;
	}

	auto review_cycle_publication_host::load(const core::local_cycle_subject& aSubject, const core::cycle_publication_id aPublication, std::stop_token aStop)
		-> cycle_store_result<stored_cycle_publication>
	{
		auto loaded = Store.load_publication(aSubject, aPublication, std::move(aStop));
		if (!loaded) {
			return std::unexpected(loaded.error());
		}
		if (!*loaded) {
			return failed(cycle_store_failure::not_found);
		}
		return std::move(**loaded);
	}

	auto review_cycle_publication_host::change(const core::local_cycle_subject& aSubject, const cycle_operation_id& aOperation, const core::cycle_publication_id aPublication,
											   const std::uint64_t aExpectedRevision, const core::publication_command& aCommand, std::stop_token aStop)
		-> cycle_store_result<cycle_publication_change>
	{
		const auto receipt = Store.change_publication(aOperation, aSubject, aPublication, aExpectedRevision, aCommand, aStop);
		if (!receipt) {
			return std::unexpected(receipt.error());
		}
		auto after = load(aSubject, aPublication, aStop);
		if (!after) {
			return std::unexpected(after.error());
		}
		return cycle_publication_change{ .Publication = std::move(*after), .Sequence = receipt->Allocated, .Replayed = receipt->Replayed };
	}

	auto review_cycle_publication_host::authorize(const core::local_cycle_subject& aSubject, const cycle_operation_id& aOperation, const core::cycle_publication_id aPublication,
												  const std::uint64_t aExpectedRevision, const cycle_publication_confirmation& aConfirmation, std::stop_token aStop)
		-> cycle_store_result<cycle_publication_change>
	{
		return change(aSubject, aOperation, aPublication, aExpectedRevision,
					  core::authorize_publication{ .Subject = aSubject, .Actor = aConfirmation.Actor, .PlanDigest = aConfirmation.PlanDigest, .Account = aConfirmation.Account },
					  std::move(aStop));
	}

	auto review_cycle_publication_host::recover_after_restart(const core::local_cycle_subject& aSubject, std::stop_token aStop)
		-> cycle_store_result<std::vector<stored_cycle_publication>>
	{
		return recover_publications_after_restart(Store, aSubject, std::move(aStop));
	}

	auto recover_publications_after_restart(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, std::stop_token aStop)
		-> cycle_store_result<std::vector<stored_cycle_publication>>
	{
		auto recovered = std::vector<stored_cycle_publication>{ };
		auto after     = std::optional<core::cycle_publication_id>{ };
		do {
			auto page = aStore.publications(aSubject, after, aStop);
			if (!page) {
				return std::unexpected(page.error());
			}
			for (auto& publication : page->Publications) {
				const auto restarted = core::apply_publication(publication.Plan, publication.Journal, core::restart_publication{ });
				if (restarted && restarted->Journal != publication.Journal) {
					const auto receipt
						= aStore.change_publication(restart_operation(aSubject, publication.Id.Id, publication.Revision), aSubject, publication.Id.Id, publication.Revision, core::restart_publication{ }, aStop);
					if (!receipt) {
						return std::unexpected(receipt.error());
					}
					auto reloaded = aStore.load_publication(aSubject, publication.Id.Id, aStop);
					if (!reloaded) {
						return std::unexpected(reloaded.error());
					}
					if (!*reloaded) {
						return failed(cycle_store_failure::not_found);
					}
					publication = std::move(**reloaded);
				}
				recovered.push_back(std::move(publication));
			}
			after = page->Next;
		} while (after);
		return recovered;
	}
}
