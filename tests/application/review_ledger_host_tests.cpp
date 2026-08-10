#include "bpr/application/review_ledger_application_host.hpp"

#include <algorithm>
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

		auto store_note(const application::local_note_record& aNote)
			-> std::expected<void, application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(*failure);
			}
			const auto found = std::ranges::find(
				Private.Notes, aNote.Id, &application::local_note_record::Id);
			if (found == Private.Notes.end()) {
				Private.Notes.push_back(aNote);
			} else {
				*found = aNote;
			}
			return { };
		}

		auto erase_note(const application::local_note_id aNote)
			-> std::expected<void, application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(*failure);
			}
			std::erase_if(
				Private.Notes,
				[aNote](const application::local_note_record& aRecord) {
					return aRecord.Id == aNote;
				});
			return { };
		}

		auto store_last_reviewed_head(const core::git_commit_oid& aHead)
			-> std::expected<void, application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(*failure);
			}
			Private.LastReviewedHead = aHead;
			return { };
		}

		auto erase_last_reviewed_head()
			-> std::expected<void, application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(*failure);
			}
			Private.LastReviewedHead.reset();
			return { };
		}

		auto store_defect(const application::durable_defect_record& aDefect)
			-> std::expected<void, application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(*failure);
			}
			const auto found = std::ranges::find(
				Private.Defects,
				aDefect.History.Defect.Id,
				[](const application::durable_defect_record& aRecord) {
					return aRecord.History.Defect.Id;
				});
			if (found == Private.Defects.end()) {
				Private.Defects.push_back(aDefect);
			} else {
				*found = aDefect;
			}
			return { };
		}

		auto erase_defect(const core::defect_id aDefect)
			-> std::expected<void, application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(*failure);
			}
			std::erase_if(
				Private.Defects,
				[aDefect](const application::durable_defect_record& aRecord) {
					return aRecord.History.Defect.Id == aDefect;
				});
			return { };
		}

		auto load_private() -> std::expected<
			application::private_ledger_observation,
			application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(*failure);
			}
			return Private;
		}

		void fail_next(const application::ledger_store_failure aFailure)
		{
			NextFailure = aFailure;
		}

		std::vector<application::receipt_persistence_request> Requests;
		std::optional<core::file_receipt>                     Committed;
		application::private_ledger_observation               Private;

	private:
		[[nodiscard]] auto take_failure()
			-> std::optional<application::ledger_store_failure>
		{
			const auto failure = NextFailure;
			NextFailure.reset();
			return failure;
		}

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

	[[nodiscard]] auto private_note() -> application::local_note_record
	{
		return application::local_note_record{
			.Id         = application::local_note_id{ 7U },
			.Attachment = application::local_note_attachment{
				.File       = opaque<core::file_identity>(11U),
				.Comparison = opaque<core::file_comparison_digest>(12U),
			},
			.Note = { .Text = "Re-check the ownership boundary." },
		};
	}

	[[nodiscard]] auto reviewed_head() -> core::git_commit_oid
	{
		const auto parsed = core::parse_git_commit_oid(
			"1111111111111111111111111111111111111111");
		return *parsed;
	}

	[[nodiscard]] auto private_defect()
		-> application::durable_defect_record
	{
		const auto anchor = core::source_line_ref{
			.Comparison = opaque<core::file_comparison_digest>(21U),
			.Side       = core::diff_side::right,
			.Line       = 42U,
		};
		return application::durable_defect_record{
			.File         = opaque<core::file_identity>(20U),
			.Conversation = core::review_conversation_id{ 30U },
			.History      = {
				.Defect = {
					.Id       = core::defect_id{ 31U },
					.Anchor   = anchor,
					.Severity = core::defect_severity::major,
					.Category = core::defect_category::security,
					.State    = core::defect_state::needs_verification,
				},
				.OriginalAnchor = anchor,
				.Availability   = core::defect_anchor_availability::outdated,
			},
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

	void test_private_records_are_validated_before_the_store()
	{
		auto log        = event_log{ };
		auto store      = recording_store{ log };
		auto projection = recording_projection{ log };
		auto host       = application::review_ledger_application_host{ store, projection };

		auto note = private_note();
		note.Note.Text.clear();
		const auto badNote = host.persist(note);
		const auto badHead = host.persist_last_reviewed_head(core::git_commit_oid{ });
		auto       defect  = private_defect();
		defect.History.Defect.Anchor.Line += 1U;
		const auto badDefect = host.persist(defect);
		const auto badErase  = host.erase(application::local_note_id{ });

		expect(
			!badNote
				&& badNote.error()
					== application::ledger_store_failure::invalid_record
				&& !badHead
				&& badHead.error()
					== application::ledger_store_failure::invalid_record
				&& !badDefect
				&& badDefect.error()
					== application::ledger_store_failure::invalid_record
				&& !badErase
				&& badErase.error()
					== application::ledger_store_failure::invalid_record,
			"invalid private records are refused with one exact reason");
		expect(
			store.Private == application::private_ledger_observation{ },
			"no invalid record reaches the durable port");
		expect(
			host.statistics().PrivateWrites == 0U
				&& host.statistics().PrivateDeletes == 0U
				&& host.statistics().PrivateFailures == 4U,
			"validation failures are counted without false writes or deletes");
	}

	void test_private_records_round_trip_and_delete_through_the_host()
	{
		auto       log        = event_log{ };
		auto       store      = recording_store{ log };
		auto       projection = recording_projection{ log };
		auto       host       = application::review_ledger_application_host{ store, projection };
		const auto note       = private_note();
		const auto head       = reviewed_head();
		const auto defect     = private_defect();

		expect(host.persist(note).has_value(), "the note commits");
		expect(
			host.persist_last_reviewed_head(head).has_value(),
			"the reviewed head commits");
		expect(host.persist(defect).has_value(), "the defect commits");

		const auto observed = host.private_observation();
		expect(
			observed.has_value()
				&& *observed
					== application::private_ledger_observation{
						.Notes            = { note },
						.LastReviewedHead = head,
						.Defects          = { defect },
					},
			"the host returns exactly the records the durable port accepted");

		store.fail_next(application::ledger_store_failure::conflict);
		auto changed       = note;
		changed.Note.Text  = "This update must not be adopted.";
		const auto refused = host.persist(changed);
		expect(
			!refused
				&& refused.error() == application::ledger_store_failure::conflict,
			"a private store failure remains typed");
		expect(
			store.Private.Notes == std::vector{ note },
			"a failed private write leaves the durable observation unchanged");

		expect(host.erase(note.Id).has_value(), "the note deletes");
		expect(
			host.erase_last_reviewed_head().has_value(),
			"the reviewed head deletes");
		expect(
			host.erase(defect.History.Defect.Id).has_value(),
			"the defect deletes");
		const auto empty = host.private_observation();
		expect(
			empty.has_value()
				&& *empty == application::private_ledger_observation{ },
			"deletion returns the exact scope to an empty private observation");

		const auto statistics = host.statistics();
		expect(
			statistics.PrivateWrites == 4U
				&& statistics.PrivateDeletes == 3U
				&& statistics.PrivateLoads == 2U
				&& statistics.PrivateFailures == 1U,
			"private statistics distinguish writes, deletes, loads, and failures");
	}
} // namespace

auto main() -> int
{
	test_the_projection_follows_the_commit();
	test_a_refused_command_never_reaches_the_store();
	test_a_failed_commit_changes_nothing();
	test_an_unchanged_command_is_not_a_write();
	test_statistics_account_for_every_command();
	test_private_records_are_validated_before_the_store();
	test_private_records_round_trip_and_delete_through_the_host();

	std::cout << "review-ledger-host-tests:pass\n";
	return EXIT_SUCCESS;
}
