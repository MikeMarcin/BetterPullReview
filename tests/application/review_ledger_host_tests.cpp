#include "bpr/application/review_ledger_application_host.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	/// One log shared by the store and the projection, so "published after
	/// committed" is an observation rather than a belief about the code.
	using event_log = std::vector<std::string>;

	class recording_store final : public application::review_ledger_store_port
	{
	public:
		explicit recording_store(event_log& aLog) noexcept
			: Log{ &aLog }
		{
		}

		auto commit(const application::receipt_persistence_request& aRequest)
			-> std::expected<void, application::ledger_store_failure> override
		{
			Log->emplace_back("request");
			Requests.push_back(aRequest);
			if (NextFailure) {
				const auto failure = *NextFailure;
				NextFailure.reset();
				Log->emplace_back("commit-refused");
				return std::unexpected(failure);
			}
			Committed = aRequest.Receipt;
			Log->emplace_back("committed");
			return { };
		}

		auto load(const core::file_identity& /*aFile*/)
			-> std::expected<
				std::optional<core::file_receipt>,
				application::ledger_store_failure> override
		{
			return Committed;
		}

		void fail_next(const application::ledger_store_failure aFailure)
		{
			NextFailure = aFailure;
		}

		std::vector<application::receipt_persistence_request> Requests;
		std::optional<core::file_receipt>                     Committed;

	private:
		event_log*                                       Log = nullptr;
		std::optional<application::ledger_store_failure> NextFailure;
	};

	class recording_projection final : public application::receipt_projection_port
	{
	public:
		explicit recording_projection(event_log& aLog) noexcept
			: Log{ &aLog }
		{
		}

		void publish(const application::receipt_projection& aProjection) override
		{
			Log->emplace_back("published");
			Published.push_back(aProjection);
		}

		void fail(const application::receipt_failure_publication& aFailure) override
		{
			Log->emplace_back("failed");
			Failures.push_back(aFailure);
		}

		std::vector<application::receipt_projection>          Published;
		std::vector<application::receipt_failure_publication> Failures;

	private:
		event_log* Log = nullptr;
	};

	[[nodiscard]] auto seeded_receipt() -> core::file_receipt
	{
		return core::file_receipt{
			.File     = opaque<core::file_identity>(1U),
			.Material = opaque<core::file_material_id>(2U),
		};
	}

	[[nodiscard]] auto command_for(
		const core::file_receipt&             aReceipt,
		const core::file_receipt_command_kind aKind) -> core::file_receipt_command
	{
		return core::file_receipt_command{
			.File     = aReceipt.File,
			.Material = aReceipt.Material,
			.Kind     = aKind,
		};
	}

	// The ordering is the contract. A projection published before the commit
	// would show a reader a state the database does not hold, and would survive
	// exactly until the next restart contradicted it.
	void test_the_projection_follows_the_commit()
	{
		auto       log        = event_log{ };
		auto       store      = recording_store{ log };
		auto       projection = recording_projection{ log };
		auto       host       = application::review_ledger_application_host{ store, projection };
		const auto seed       = seeded_receipt();
		host.retain(seed);

		const auto outcome = host.submit(
			command_for(seed, core::file_receipt_command_kind::mark_read));

		expect(
			outcome.Rejection == core::file_receipt_rejection::none
				&& outcome.Changed,
			"a legal command that moves the receipt is accepted");
		expect(
			log == event_log{ "request", "committed", "published" },
			"the request precedes the commit and the commit precedes the "
			"publication");
		expect(
			store.Requests.size() == 1U
				&& store.Requests.front().Receipt.Read == core::file_read_state::read,
			"the request carries the decided receipt");
		expect(
			projection.Published.size() == 1U
				&& projection.Published.front().Receipt == store.Committed,
			"the published projection is exactly what was committed");
		expect(
			host.retained().Read == core::file_read_state::read,
			"the host adopts the committed receipt");
	}

	void test_a_refused_command_never_reaches_the_store()
	{
		auto log        = event_log{ };
		auto store      = recording_store{ log };
		auto projection = recording_projection{ log };
		auto host       = application::review_ledger_application_host{ store, projection };
		auto seed       = seeded_receipt();
		seed.Excluded   = true;
		host.retain(seed);

		const auto outcome = host.submit(
			command_for(seed, core::file_receipt_command_kind::accept));

		expect(
			outcome.Rejection
				== core::file_receipt_rejection::excluded_file_cannot_be_accepted,
			"the core refuses to accept an excluded file");
		expect(
			log == event_log{ "failed" },
			"nothing was requested or committed");
		expect(
			store.Requests.empty() && !store.Committed.has_value(),
			"a refused command writes nothing");
		expect(
			projection.Failures.size() == 1U
				&& projection.Failures.front().Rejection == outcome.Rejection
				&& !projection.Failures.front().Store.has_value(),
			"the refusal is published with the core's reason and no store reason");
		expect(
			host.retained() == seed,
			"the retained receipt is untouched by a refused command");
	}

	// A failed commit must leave the host agreeing with the database. Adopting
	// the rejected receipt would leave the two disagreeing until something else
	// happened to overwrite it.
	void test_a_failed_commit_changes_nothing()
	{
		auto       log        = event_log{ };
		auto       store      = recording_store{ log };
		auto       projection = recording_projection{ log };
		auto       host       = application::review_ledger_application_host{ store, projection };
		const auto seed       = seeded_receipt();
		host.retain(seed);
		store.fail_next(application::ledger_store_failure::conflict);

		const auto outcome = host.submit(
			command_for(seed, core::file_receipt_command_kind::accept));

		expect(
			log == event_log{ "request", "commit-refused", "failed" },
			"the failure is published after the refused commit, not before it");
		expect(
			projection.Published.empty(),
			"a failed commit publishes no accepted projection");
		expect(
			projection.Failures.size() == 1U
				&& projection.Failures.front().Store
					== application::ledger_store_failure::conflict
				&& projection.Failures.front().Rejection
					== core::file_receipt_rejection::none,
			"the failure carries the store's reason and no core rejection");
		expect(
			host.retained() == seed && !outcome.Changed,
			"the retained receipt keeps the value the database still holds");

		// The next command must still work: a failure is not a poisoned host.
		const auto recovered = host.submit(
			command_for(seed, core::file_receipt_command_kind::accept));
		expect(
			recovered.Changed && host.retained().Accepted,
			"a later command succeeds after a store failure");
	}

	void test_an_unchanged_command_is_not_a_write()
	{
		auto       log        = event_log{ };
		auto       store      = recording_store{ log };
		auto       projection = recording_projection{ log };
		auto       host       = application::review_ledger_application_host{ store, projection };
		const auto seed       = seeded_receipt();
		host.retain(seed);

		expect(
			host.submit(command_for(seed, core::file_receipt_command_kind::accept))
				.Changed,
			"the first acceptance changes the receipt");
		const auto before = log.size();

		const auto again = host.submit(
			command_for(seed, core::file_receipt_command_kind::accept));

		expect(
			again.Rejection == core::file_receipt_rejection::none && !again.Changed,
			"re-issuing the command is legal and changes nothing");
		expect(
			log.size() == before,
			"a command that moves nothing produces no request, commit, or "
			"publication");
		expect(
			host.statistics().Unchanged == 1U
				&& host.statistics().PersistenceRequests == 1U,
			"the unchanged command is counted without being persisted");
	}

	void test_statistics_account_for_every_command()
	{
		auto       log        = event_log{ };
		auto       store      = recording_store{ log };
		auto       projection = recording_projection{ log };
		auto       host       = application::review_ledger_application_host{ store, projection };
		const auto seed       = seeded_receipt();
		host.retain(seed);

		(void)host.submit(command_for(seed, core::file_receipt_command_kind::mark_read));
		(void)host.submit(command_for(seed, core::file_receipt_command_kind::mark_read));
		store.fail_next(application::ledger_store_failure::unavailable);
		(void)host.submit(command_for(seed, core::file_receipt_command_kind::accept));
		auto elsewhere = command_for(seed, core::file_receipt_command_kind::accept);
		elsewhere.File = opaque<core::file_identity>(9U);
		(void)host.submit(elsewhere);

		const auto statistics = host.statistics();
		expect(statistics.Commands == 4U, "every command is counted");
		expect(
			statistics.Rejected == 1U && statistics.Unchanged == 1U,
			"the refused and the unchanged commands are counted apart");
		expect(
			statistics.PersistenceRequests == 2U && statistics.Completions == 2U,
			"only the two changing commands reached the store");
		expect(
			statistics.StoreFailures == 1U && statistics.Publications == 1U,
			"one commit failed and only the successful one published");
		expect(
			statistics.FailurePublications == 2U,
			"both the refusal and the store failure were published");
	}
} // namespace

auto main() -> int
{
	test_the_projection_follows_the_commit();
	test_a_refused_command_never_reaches_the_store();
	test_a_failed_commit_changes_nothing();
	test_an_unchanged_command_is_not_a_write();
	test_statistics_account_for_every_command();

	std::cout << "review-ledger-host-tests:pass\n";
	return EXIT_SUCCESS;
}
