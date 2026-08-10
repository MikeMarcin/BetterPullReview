#include "bpr/application/review_ledger_application_host.hpp"

#include <limits>

namespace bpr::application {
	namespace {
		[[nodiscard]] constexpr auto durable_integer(const std::uint64_t aValue) noexcept -> bool
		{
			return aValue > 0U
				&& aValue
				<= static_cast<std::uint64_t>(
					   std::numeric_limits<std::int64_t>::max());
		}

		template <typename Value>
		[[nodiscard]] constexpr auto enumerator_at_most(
			const Value aValue,
			const Value aMaximum) noexcept -> bool
		{
			return static_cast<std::uint64_t>(aValue)
				<= static_cast<std::uint64_t>(aMaximum);
		}
	} // namespace

	auto name(const ledger_store_failure aFailure) noexcept -> std::string_view
	{
		switch (aFailure) {
		case ledger_store_failure::unavailable:
			return "unavailable";
		case ledger_store_failure::conflict:
			return "conflict";
		case ledger_store_failure::integrity_failure:
			return "integrity_failure";
		case ledger_store_failure::quota_exceeded:
			return "quota_exceeded";
		case ledger_store_failure::cancelled:
			return "cancelled";
		case ledger_store_failure::invalid_record:
			return "invalid_record";
		}
		return "unavailable";
	}

	auto valid(const local_note_record& aNote) noexcept -> bool
	{
		if (!durable_integer(aNote.Id.value()) || aNote.Note.Text.empty()
			|| aNote.Note.Text.size() > core::maximum_note_bytes) {
			return false;
		}
		return !aNote.Attachment
			|| (aNote.Attachment->File.valid()
				&& aNote.Attachment->Comparison.valid());
	}

	auto valid(const durable_defect_record& aDefect) noexcept -> bool
	{
		const auto& history = aDefect.History;
		const auto& defect  = history.Defect;
		return aDefect.File.valid()
			&& durable_integer(aDefect.Conversation.value())
			&& durable_integer(defect.Id.value())
			&& defect.Anchor.Comparison.valid()
			&& history.OriginalAnchor.Comparison.valid()
			// The durable record has no substitute-anchor channel. A caller that
			// offers one is refused instead of quietly rewriting the complaint.
			&& defect.Anchor == history.OriginalAnchor
			&& enumerator_at_most(defect.Anchor.Side, core::diff_side::right)
			&& enumerator_at_most(
				   defect.Severity, core::defect_severity::critical)
			&& enumerator_at_most(
				   defect.Category, core::defect_category::other)
			&& enumerator_at_most(
				   defect.State, core::defect_state::accepted_as_is)
			&& enumerator_at_most(
				   history.Availability,
				   core::defect_anchor_availability::thread_deleted);
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

	auto review_ledger_application_host::persist(
		const local_note_record& aNote)
		-> std::expected<void, ledger_store_failure>
	{
		if (!valid(aNote)) {
			++Statistics.PrivateFailures;
			return std::unexpected(ledger_store_failure::invalid_record);
		}
		++Statistics.PrivateWrites;
		auto stored = Store->store_note(aNote);
		if (!stored) {
			++Statistics.PrivateFailures;
		}
		return stored;
	}

	auto review_ledger_application_host::erase(const local_note_id aNote)
		-> std::expected<void, ledger_store_failure>
	{
		if (!durable_integer(aNote.value())) {
			++Statistics.PrivateFailures;
			return std::unexpected(ledger_store_failure::invalid_record);
		}
		++Statistics.PrivateDeletes;
		auto erased = Store->erase_note(aNote);
		if (!erased) {
			++Statistics.PrivateFailures;
		}
		return erased;
	}

	auto review_ledger_application_host::persist_last_reviewed_head(
		const core::git_commit_oid& aHead)
		-> std::expected<void, ledger_store_failure>
	{
		if (!aHead.valid()) {
			++Statistics.PrivateFailures;
			return std::unexpected(ledger_store_failure::invalid_record);
		}
		++Statistics.PrivateWrites;
		auto stored = Store->store_last_reviewed_head(aHead);
		if (!stored) {
			++Statistics.PrivateFailures;
		}
		return stored;
	}

	auto review_ledger_application_host::erase_last_reviewed_head()
		-> std::expected<void, ledger_store_failure>
	{
		++Statistics.PrivateDeletes;
		auto erased = Store->erase_last_reviewed_head();
		if (!erased) {
			++Statistics.PrivateFailures;
		}
		return erased;
	}

	auto review_ledger_application_host::persist(
		const durable_defect_record& aDefect)
		-> std::expected<void, ledger_store_failure>
	{
		if (!valid(aDefect)) {
			++Statistics.PrivateFailures;
			return std::unexpected(ledger_store_failure::invalid_record);
		}
		++Statistics.PrivateWrites;
		auto stored = Store->store_defect(aDefect);
		if (!stored) {
			++Statistics.PrivateFailures;
		}
		return stored;
	}

	auto review_ledger_application_host::erase(const core::defect_id aDefect)
		-> std::expected<void, ledger_store_failure>
	{
		if (!durable_integer(aDefect.value())) {
			++Statistics.PrivateFailures;
			return std::unexpected(ledger_store_failure::invalid_record);
		}
		++Statistics.PrivateDeletes;
		auto erased = Store->erase_defect(aDefect);
		if (!erased) {
			++Statistics.PrivateFailures;
		}
		return erased;
	}

	auto review_ledger_application_host::private_observation()
		-> std::expected<private_ledger_observation, ledger_store_failure>
	{
		++Statistics.PrivateLoads;
		auto loaded = Store->load_private();
		if (!loaded) {
			++Statistics.PrivateFailures;
		}
		return loaded;
	}
} // namespace bpr::application
