#include "bpr/application/review_ledger_application_host.hpp"

namespace bpr::application {
	auto name(const ledger_store_failure aFailure) noexcept -> std::string_view
	{
		switch (aFailure) {
		case ledger_store_failure::unavailable:
			return "unavailable";
		case ledger_store_failure::conflict:
			return "conflict";
		case ledger_store_failure::integrity_failure:
			return "integrity_failure";
		case ledger_store_failure::cancelled:
			return "cancelled";
		}
		return "unavailable";
	}

	review_ledger_application_host::review_ledger_application_host(
		review_ledger_store_port& aStore,
		receipt_projection_port&  aProjection) noexcept
		: Store{ &aStore }
		, Projection{ &aProjection }
	{
	}

	void review_ledger_application_host::retain(const core::file_receipt& aReceipt)
	{
		Retained = aReceipt;
	}

	auto review_ledger_application_host::retained() const noexcept
		-> const core::file_receipt&
	{
		return Retained;
	}

	auto review_ledger_application_host::statistics() const noexcept
		-> review_ledger_host_statistics
	{
		return Statistics;
	}

	auto review_ledger_application_host::submit(
		const core::file_receipt_command& aCommand) -> core::file_receipt_outcome
	{
		++Statistics.Commands;

		const auto outcome = core::apply(Retained, aCommand);
		if (outcome.Rejection != core::file_receipt_rejection::none) {
			++Statistics.Rejected;
			++Statistics.FailurePublications;
			Projection->fail(receipt_failure_publication{
				.Retained  = Retained,
				.Rejection = outcome.Rejection,
			});
			return outcome;
		}

		// A legal command that moved nothing is not an error and is not a
		// write. Persisting it would turn every repeated click into a commit,
		// and publishing it would announce a change that did not happen.
		if (!outcome.Changed) {
			++Statistics.Unchanged;
			return outcome;
		}

		++Statistics.PersistenceRequests;
		const auto completion
			= Store->commit(receipt_persistence_request{ .Receipt = outcome.Receipt });
		++Statistics.Completions;

		if (!completion) {
			// The retained receipt keeps its previous value. Adopting the
			// rejected one would leave the host disagreeing with the database
			// until something else overwrote it.
			++Statistics.StoreFailures;
			++Statistics.FailurePublications;
			Projection->fail(receipt_failure_publication{
				.Retained = Retained,
				.Store    = completion.error(),
			});
			return core::file_receipt_outcome{
				.Receipt   = Retained,
				.Rejection = core::file_receipt_rejection::none,
				.Changed   = false,
			};
		}

		Retained = outcome.Receipt;
		++Statistics.Publications;
		Projection->publish(receipt_projection{ .Receipt = Retained });
		return outcome;
	}
} // namespace bpr::application
