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
			if (const auto failure = take_failure()) {
				Log->emplace_back("load-refused");
				return std::unexpected(*failure);
			}
			return Committed;
		}

		auto store_conversation_receipt(const core::conversation_receipt& aReceipt)
			-> std::expected<void, application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				Log->emplace_back("conversation-store-refused");
				return std::unexpected(*failure);
			}
			Log->emplace_back("conversation-stored");
			std::erase_if(ConversationReceipts, [&aReceipt](const core::conversation_receipt& aStored) {
				return aStored.Conversation == aReceipt.Conversation;
			});
			ConversationReceipts.push_back(aReceipt);
			return { };
		}

		auto load_conversation_receipts() -> std::expected<
			std::vector<core::conversation_receipt>,
			application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				Log->emplace_back("conversation-load-refused");
				return std::unexpected(*failure);
			}
			Log->emplace_back("conversation-loaded");
			return ConversationReceipts;
		}

		std::vector<core::conversation_receipt> ConversationReceipts;

		std::optional<application::local_note_record> HostileCurrent;
		std::uint64_t                                 NextNoteRevision = 1U;
		auto                                          store_note(const application::local_note_record&                 aNote,
																 const std::optional<application::local_note_revision> aExpectedRevision)
			-> std::expected<application::local_note_record, application::local_note_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(application::local_note_failure{ .Reason = *failure });
			}
			if (HostileCurrent) {
				return std::unexpected(application::local_note_failure{ .Reason = application::ledger_store_failure::conflict, .Current = HostileCurrent });
			}
			const auto found   = std::ranges::find(Private.Notes, aNote.Id, &application::local_note_record::Id);
			const auto current = found == Private.Notes.end() ? std::optional<application::local_note_record>{ } : *found;
			if ((!aExpectedRevision && current) || (aExpectedRevision && (!current || current->Revision != *aExpectedRevision))) {
				return std::unexpected(application::local_note_failure{ .Reason = application::ledger_store_failure::conflict, .Current = current });
			}
			auto accepted     = aNote;
			accepted.Revision = application::local_note_revision{ NextNoteRevision++ };
			if (found == Private.Notes.end()) {
				Private.Notes.push_back(accepted);
			} else {
				*found = accepted;
			}
			return accepted;
		}

		auto erase_note(const application::local_note_id       aNote,
						const application::local_note_revision aExpectedRevision)
			-> std::expected<void, application::local_note_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(application::local_note_failure{ .Reason = *failure });
			}
			if (HostileCurrent) {
				return std::unexpected(application::local_note_failure{ .Reason = application::ledger_store_failure::conflict, .Current = HostileCurrent });
			}
			const auto found   = std::ranges::find(Private.Notes, aNote, &application::local_note_record::Id);
			const auto current = found == Private.Notes.end() ? std::optional<application::local_note_record>{ } : *found;
			if (!current || current->Revision != aExpectedRevision) {
				return std::unexpected(application::local_note_failure{ .Reason = application::ledger_store_failure::conflict, .Current = current });
			}
			Private.Notes.erase(found);
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

		auto admit_agent_session(
			const application::agent_session_admission& aAdmission)
			-> std::expected<core::agent_session, application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(*failure);
			}
			if (!Catalog.Next.has_value()) {
				return std::unexpected(
					application::ledger_store_failure::quota_exceeded);
			}
			Admissions.push_back(aAdmission);
			auto admitted = core::agent_session{
				.Id      = *Catalog.Next,
				.Profile = aAdmission.Profile,
				.Binding = aAdmission.Binding,
			};
			Catalog.Latest = admitted;
			Catalog.Next   = core::agent_session_id{ admitted.Id.value() + 1U };
			return admitted;
		}

		auto load_agent_session_catalog() -> std::expected<
			application::agent_session_catalog_state,
			application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(*failure);
			}
			return Catalog;
		}

		void fail_next(const application::ledger_store_failure aFailure)
		{
			NextFailure = aFailure;
		}

		std::vector<application::receipt_persistence_request> Requests;
		std::optional<core::file_receipt>                     Committed;
		application::private_ledger_observation               Private;
		application::agent_session_catalog_state              Catalog{
			.Next = core::agent_session_id{ 1U },
		};
		std::vector<application::agent_session_admission> Admissions;

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

	[[nodiscard]] auto session_admission()
		-> application::agent_session_admission
	{
		return {
			.Profile = core::agent_profile_id{ 4U },
			.Binding = {
				.Host        = core::git_host_id{ 5U },
				.Repository  = core::repository_id{ 6U },
				.PullRequest = core::pull_request_id{ 7U },
				.BaseOid     = reviewed_head(),
				.HeadOid     = reviewed_head(),
				.LeftEpoch   = core::epoch_ordinal{ 8 },
				.RightEpoch  = core::epoch_ordinal{ 9 },
			},
		};
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
		const auto badErase  = host.erase(application::local_note_id{ }, application::local_note_revision{ 1U });

		expect(
			!badNote
				&& badNote.error().Reason
					== application::ledger_store_failure::invalid_record
				&& !badHead
				&& badHead.error()
					== application::ledger_store_failure::invalid_record
				&& !badDefect
				&& badDefect.error()
					== application::ledger_store_failure::invalid_record
				&& !badErase
				&& badErase.error().Reason
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
		auto log          = event_log{ };
		auto store        = recording_store{ log };
		auto projection   = recording_projection{ log };
		auto host         = application::review_ledger_application_host{ store, projection };
		auto note         = private_note();
		note.Revision     = application::local_note_revision{ 1U };
		const auto head   = reviewed_head();
		const auto defect = private_defect();

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
				&& refused.error().Reason == application::ledger_store_failure::conflict,
			"a private store failure remains typed");
		expect(
			store.Private.Notes == std::vector{ note },
			"a failed private write leaves the durable observation unchanged");

		expect(host.erase(note.Id, note.Revision).has_value(), "the note deletes");
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

	void test_material_observation_loads_carries_and_publishes_in_order()
	{
		auto       log        = event_log{ };
		auto       store      = recording_store{ log };
		auto       projection = recording_projection{ log };
		auto       host       = application::review_ledger_application_host{ store, projection };
		const auto seed       = seeded_receipt();
		const auto first      = host.observe_material(core::file_material_observation{
			.File     = seed.File,
			.Material = seed.Material,
		});

		expect(first.has_value() && *first == seed,
			   "an absent durable receipt becomes an unread exact-material receipt");
		expect(store.Requests.empty() && log == event_log{ "published" },
			   "an absent receipt publishes without writing");

		auto old        = seed;
		old.Read        = core::file_read_state::read;
		old.Accepted    = true;
		old.Excluded    = true;
		store.Committed = old;
		log.clear();
		const auto replacement = opaque<core::file_material_id>(19U);
		const auto carried     = host.observe_material(
			core::file_material_observation{
				.File     = old.File,
				.Material = replacement,
			});

		expect(carried.has_value()
				   && carried->Material == replacement
				   && carried->Read == core::file_read_state::unread
				   && !carried->Accepted && carried->Excluded,
			   "changed material reopens read and acceptance while preserving exclusion");
		expect(log == event_log{ "request", "committed", "published" },
			   "carry-over commits before its receipt projection");
		expect(host.statistics().ReceiptLoads == 2U
				   && host.statistics().ReceiptLoadHits == 1U
				   && host.statistics().PersistenceRequests == 1U,
			   "material observation distinguishes loads, hits, and carry-over writes");
	}

	void test_material_load_failure_is_typed_and_changes_nothing()
	{
		auto       log        = event_log{ };
		auto       store      = recording_store{ log };
		auto       projection = recording_projection{ log };
		auto       host       = application::review_ledger_application_host{ store, projection };
		const auto seed       = seeded_receipt();
		host.retain(seed);
		store.fail_next(application::ledger_store_failure::unavailable);

		const auto observed = host.observe_material(
			core::file_material_observation{
				.File     = seed.File,
				.Material = opaque<core::file_material_id>(33U),
			});

		expect(!observed
				   && observed.error()
					   == application::ledger_store_failure::unavailable,
			   "a load failure stays typed");
		expect(host.retained() == seed && store.Requests.empty(),
			   "a failed load neither carries nor persists a receipt");
		expect(log == event_log{ "load-refused", "failed" },
			   "the typed load refusal reaches projection after the store terminal");
	}

	void test_agent_sessions_validate_admit_and_load_through_the_host()
	{
		auto log                  = event_log{ };
		auto store                = recording_store{ log };
		auto projection           = recording_projection{ log };
		auto host                 = application::review_ledger_application_host{ store, projection };
		auto admission            = session_admission();
		auto invalid              = admission;
		invalid.Binding.LeftEpoch = core::epoch_ordinal{ };

		const auto refused = host.admit(invalid);
		expect(
			!refused
				&& refused.error()
					== application::ledger_store_failure::invalid_record
				&& store.Admissions.empty(),
			"an invalid session never reaches durable admission");

		const auto admitted = host.admit(admission);
		const auto catalog  = host.session_catalog();
		expect(
			admitted.has_value() && admitted->Id == core::agent_session_id{ 1U }
				&& admitted->Profile == admission.Profile
				&& admitted->Binding == admission.Binding,
			"the host returns the exact immutable session allocated by storage");
		expect(
			catalog.has_value() && catalog->Latest == admitted
				&& catalog->Next == core::agent_session_id{ 2U },
			"loading returns the latest retained session and monotonic next identity");
		expect(
			host.statistics().SessionAdmissions == 1U
				&& host.statistics().SessionLoads == 1U
				&& host.statistics().SessionFailures == 1U,
			"session statistics distinguish admission, load, and pre-store refusal");
	}

	// [[BPR-0656]]. The read state of a review's conversations: receipts are
	// loaded once, every post after the cursor is unread, Mark read writes the
	// cursor through the store before the host keeps it, and a refused store
	// leaves the next observation asking again.
	void test_conversation_read_state_loads_once_and_marks_read_through_the_store()
	{
		auto       log        = event_log{ };
		auto       store      = recording_store{ log };
		auto       projection = recording_projection{ log };
		auto       host       = application::review_ledger_application_host{ store, projection };
		const auto thread     = core::review_conversation_id{ 41U };
		const auto posts      = std::vector<core::conversation_post_observation>{
			{ .Comment = core::review_comment_id{ 1U }, .Edit = core::comment_edit_version{ 11U } },
			{ .Comment = core::review_comment_id{ 2U }, .Edit = core::comment_edit_version{ 12U }, .Mine = true },
			{ .Comment = core::review_comment_id{ 3U }, .Edit = core::comment_edit_version{ 13U } },
		};
		const auto threads = std::vector<application::conversation_thread_observation>{
			{ .Conversation = thread, .Posts = posts },
			{ .Conversation = core::review_conversation_id{ 42U },
			  .Posts        = { { .Comment = core::review_comment_id{ 9U }, .Edit = core::comment_edit_version{ 19U }, .Mine = true } } },
		};

		store.fail_next(application::ledger_store_failure::unavailable);
		const auto refused = host.observe_conversations(threads);
		expect(!refused.has_value()
				   && refused.error() == application::ledger_store_failure::unavailable
				   && log == event_log{ "conversation-load-refused" },
			   "a refused receipt load is typed and caches nothing");
		log.clear();

		const auto fresh = host.observe_conversations(threads);
		expect(fresh.has_value() && fresh->UnreadThreads == 1U
				   && fresh->Threads.size() == 1U
				   && fresh->Threads[0].Conversation == thread
				   && !fresh->Threads[0].SeenBefore
				   && fresh->Threads[0].UnreadPosts
					   == std::vector{ core::review_comment_id{ 1U }, core::review_comment_id{ 3U } },
			   "with no receipt every post of someone else's is unread and my own "
			   "thread is not");
		const auto again = host.observe_conversations(threads);
		expect(again == fresh && log == event_log{ "conversation-loaded" },
			   "the receipts are loaded once and reused");

		store.fail_next(application::ledger_store_failure::quota_exceeded);
		const auto refusedWrite = host.mark_conversation_read(core::conversation_observation{
			.Conversation  = thread,
			.LatestComment = core::review_comment_id{ 3U },
			.LatestEdit    = core::comment_edit_version{ 13U },
		});
		expect(!refusedWrite.has_value()
				   && refusedWrite.error() == application::ledger_store_failure::quota_exceeded
				   && host.observe_conversations(threads) == fresh,
			   "a refused Mark read changes nothing the host reports");

		const auto marked = host.mark_conversation_read(core::conversation_observation{
			.Conversation  = thread,
			.LatestComment = core::review_comment_id{ 3U },
			.LatestEdit    = core::comment_edit_version{ 13U },
		});
		expect(marked.has_value() && store.ConversationReceipts.size() == 1U
				   && store.ConversationReceipts[0].SeenComment == core::review_comment_id{ 3U }
				   && store.ConversationReceipts[0].AcceptedComment == core::review_comment_id{ },
			   "Mark read stores the seen cursor at the latest post and leaves acceptance");
		const auto read = host.observe_conversations(threads);
		expect(read.has_value() && read->UnreadThreads == 0U && read->Threads.empty(),
			   "a read conversation reports nothing unread");

		auto edited             = threads;
		edited[0].Posts[2].Edit = core::comment_edit_version{ 14U };
		const auto reopened     = host.observe_conversations(edited);
		expect(reopened.has_value() && reopened->UnreadThreads == 1U
				   && reopened->Threads[0].UnreadPosts == std::vector{ core::review_comment_id{ 3U } }
				   && reopened->Threads[0].SeenBefore,
			   "an edit to the seen post reopens exactly that post, in a conversation read before");

		const auto repeat = host.mark_conversation_read(core::conversation_observation{
			.Conversation  = thread,
			.LatestComment = core::review_comment_id{ 3U },
			.LatestEdit    = core::comment_edit_version{ 13U },
		});
		expect(repeat.has_value() && store.ConversationReceipts.size() == 1U,
			   "an unchanged Mark read is not a write");
		expect(!host.mark_conversation_read(core::conversation_observation{ }).has_value(),
			   "a nameless conversation is refused before the store");
		const auto statistics = host.statistics();
		expect(statistics.ConversationLoads == 2U && statistics.ConversationWrites == 2U
				   && statistics.ConversationFailures == 2U && statistics.Unchanged == 1U,
			   "statistics account for every conversation load, write and refusal");
	}

	void test_receipt_action_projection_contains_only_core_legal_changes()
	{
		auto receipt       = seeded_receipt();
		receipt.Read       = core::file_read_state::seen;
		receipt.Accepted   = true;
		const auto actions = application::project_file_receipt_actions(receipt);

		expect(actions.Receipt == receipt
				   && !actions.Actions[0].Enabled
				   && actions.Actions[1].Enabled
				   && actions.Actions[1].Command.Kind
					   == core::file_receipt_command_kind::mark_read
				   && actions.Actions[2].Enabled
				   && actions.Actions[2].Command.Kind
					   == core::file_receipt_command_kind::revoke_acceptance
				   && !actions.Actions[3].Enabled,
			   "the four compact slots carry the inverse gestures and core legality");
	}
	void test_hostile_note_competitors_are_bounded_before_projection()
	{
		auto       log        = event_log{ };
		auto       store      = recording_store{ log };
		auto       projection = recording_projection{ log };
		auto       host       = application::review_ledger_application_host{ store, projection };
		const auto candidate  = private_note();
		for (const auto kind : { 0, 1, 2 }) {
			auto competitor     = candidate;
			competitor.Revision = application::local_note_revision{ 1U };
			if (kind == 0) {
				competitor.Id = application::local_note_id{ candidate.Id.value() + 1U };
			}
			if (kind == 1) {
				competitor.Revision = { };
			}
			if (kind == 2) {
				competitor.Note.Text = std::string(core::maximum_note_bytes + 1U, 'x');
			}
			store.HostileCurrent = competitor;
			const auto refused   = host.persist(candidate);
			const auto retained  = host.rejected_note();
			expect(!refused && refused.error().Reason == application::ledger_store_failure::integrity_failure
					   && !refused.error().Current && retained && retained->Candidate == candidate
					   && !retained->Failure.Current,
				   "foreign, unrevisioned and oversized competitors never enter the write projection");
			host.dismiss_rejected_note();
			const auto erased = host.erase(candidate.Id, application::local_note_revision{ 1U });
			expect(!erased && erased.error().Reason == application::ledger_store_failure::integrity_failure
					   && !erased.error().Current && host.rejected_note() && !host.rejected_note()->Failure.Current,
				   "delete conflicts apply the same bounded exact-identity observation guard");
			host.dismiss_rejected_note();
		}
	}

	void test_note_conflicts_retain_exact_window_candidate()
	{
		auto       log        = event_log{ };
		auto       store      = recording_store{ log };
		auto       projection = recording_projection{ log };
		auto       first      = application::review_ledger_application_host{ store, projection };
		auto       second     = application::review_ledger_application_host{ store, projection };
		const auto created    = first.persist(private_note());
		expect(created.has_value(), "the initial note is accepted");
		auto winner         = *created;
		winner.Note.Text    = "accepted elsewhere";
		const auto accepted = first.persist(winner, created->Revision);
		expect(accepted.has_value(), "the first window advances the durable note");
		auto candidate      = *created;
		candidate.Note.Text = std::string{ "local\0candidate\n", 16U };
		const auto refused  = second.persist(candidate, created->Revision);
		const auto retained = second.rejected_note();
		const auto durable  = second.private_observation();
		expect(!refused && refused.error().Reason == application::ledger_store_failure::conflict
				   && refused.error().Current == *accepted && retained
				   && retained->Candidate == candidate && retained->Failure == refused.error()
				   && retained->ExpectedRevision == created->Revision,
			   "conflict retains every candidate byte separately from the accepted competitor");
		expect(durable && durable->Notes == std::vector{ *accepted } && !first.rejected_note(),
			   "a rejected window cannot overwrite shared durable state or another window projection");
		expect(second.rejected_note() == retained, "reading durable state does not discard the local candidate");
		const auto laterDelete    = second.erase(created->Id, created->Revision);
		auto       laterCandidate = candidate;
		laterCandidate.Note.Text  = "another refused edit";
		const auto laterEdit      = second.persist(laterCandidate, created->Revision);
		expect(!laterDelete && !laterEdit && second.rejected_note() == retained,
			   "later rejected edits and deletes cannot replace protected candidate bytes");
		auto resolved            = *accepted;
		resolved.Note.Text       = "explicit later accepted edit";
		const auto laterAccepted = second.persist(resolved, accepted->Revision);
		expect(laterAccepted && second.rejected_note() == retained,
			   "even a successful durable write does not implicitly dismiss the protected candidate");
		second.dismiss_rejected_note();
		expect(!second.rejected_note(), "candidate dismissal is explicit");
		const auto deleted = second.erase(created->Id, created->Revision);
		expect(!deleted && second.rejected_note() && !second.rejected_note()->Candidate
				   && second.rejected_note()->Failure.Current == *laterAccepted,
			   "stale deletion has a distinct typed projection with its accepted competitor");
	}

} // namespace

auto main() -> int
{
	test_hostile_note_competitors_are_bounded_before_projection();
	test_note_conflicts_retain_exact_window_candidate();
	test_the_projection_follows_the_commit();
	test_a_refused_command_never_reaches_the_store();
	test_a_failed_commit_changes_nothing();
	test_an_unchanged_command_is_not_a_write();
	test_statistics_account_for_every_command();
	test_private_records_are_validated_before_the_store();
	test_private_records_round_trip_and_delete_through_the_host();
	test_material_observation_loads_carries_and_publishes_in_order();
	test_material_load_failure_is_typed_and_changes_nothing();
	test_agent_sessions_validate_admit_and_load_through_the_host();
	test_receipt_action_projection_contains_only_core_legal_changes();
	test_conversation_read_state_loads_once_and_marks_read_through_the_store();

	std::cout << "review-ledger-host-tests:pass\n";
	return EXIT_SUCCESS;
}
