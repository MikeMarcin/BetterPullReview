#include "../support/insights_observer.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/observing_review_ledger_store.hpp"
#include "bpr/application/review_ledger_application_host.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
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
			if (ThrowOnCommit) {
				throw std::runtime_error{ "store write failure" };
			}
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

		auto load(const core::file_identity& aFile)
			-> std::expected<
				std::optional<core::file_receipt>,
				application::ledger_store_failure> override
		{
			LastLoadedFile = aFile;
			if (ThrowOnLoad) {
				throw std::runtime_error{ "store failure" };
			}
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

		auto store_generated_mark(const core::generated_file_mark& aMark)
			-> std::expected<void, application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(*failure);
			}
			GeneratedWrites.push_back(aMark);
			auto&      marks = Private.GeneratedMarks.Marks;
			const auto at    = std::ranges::lower_bound(marks, aMark.File, { }, &core::generated_file_mark::File);
			if (at != marks.end() && at->File == aMark.File) {
				*at = aMark;
			} else {
				marks.insert(at, aMark);
			}
			return { };
		}

		auto erase_generated_mark(const core::file_identity& aFile)
			-> std::expected<void, application::ledger_store_failure> override
		{
			if (const auto failure = take_failure()) {
				return std::unexpected(*failure);
			}
			GeneratedErases.push_back(aFile);
			std::erase_if(Private.GeneratedMarks.Marks, [&aFile](const core::generated_file_mark& aMark) { return aMark.File == aFile; });
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
		bool                                                  ThrowOnLoad   = false;
		bool                                                  ThrowOnCommit = false;
		core::file_identity                                   LastLoadedFile;
		std::optional<core::file_receipt>                     Committed;
		application::private_ledger_observation               Private;
		std::vector<core::generated_file_mark>                GeneratedWrites;
		std::vector<core::file_identity>                      GeneratedErases;
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

	// ---- [[BPR-1211]] the asynchronous host's test rig ----------------------

	/// Holds one store operation until released, or until its limit passes.
	struct store_gate
	{
		std::mutex                Mutex;
		std::condition_variable   Changed;
		bool                      Held    = false;
		bool                      Entered = false;
		std::chrono::milliseconds Limit{ 2'000 };

		void pass()
		{
			auto lock = std::unique_lock{ Mutex };
			Entered   = true;
			Changed.notify_all();
			static_cast<void>(Changed.wait_for(lock, Limit, [this] { return !Held; }));
		}

		void hold()
		{
			const auto lock = std::scoped_lock{ Mutex };
			Held            = true;
			Entered         = false;
		}

		void release()
		{
			{
				const auto lock = std::scoped_lock{ Mutex };
				Held            = false;
			}
			Changed.notify_all();
		}

		[[nodiscard]] auto wait_entered(const std::chrono::milliseconds aTimeout) -> bool
		{
			auto lock = std::unique_lock{ Mutex };
			return Changed.wait_for(lock, aTimeout, [this] { return Entered; });
		}
	};

	/// Forwards to a store the test keeps, so hosts can open, close and reopen
	/// the same durable root. Every operation may first pass a gate.
	class forwarding_store final : public application::review_ledger_store_port
	{
	public:
		forwarding_store(application::review_ledger_store_port& aTarget, std::shared_ptr<store_gate> aGate, bool* aClosed)
			: Target{ &aTarget }
			, Gate{ std::move(aGate) }
			, Closed{ aClosed }
		{
		}
		forwarding_store(const forwarding_store&)                    = delete;
		auto operator=(const forwarding_store&) -> forwarding_store& = delete;
		forwarding_store(forwarding_store&&)                         = delete;
		auto operator=(forwarding_store&&) -> forwarding_store&      = delete;
		~forwarding_store() override
		{
			if (Closed != nullptr) {
				*Closed = true;
			}
		}

		auto commit(const application::receipt_persistence_request& aRequest) -> std::expected<void, application::ledger_store_failure> override
		{
			pass();
			return Target->commit(aRequest);
		}
		auto load(const core::file_identity& aFile) -> std::expected<std::optional<core::file_receipt>, application::ledger_store_failure> override
		{
			pass();
			return Target->load(aFile);
		}
		auto store_conversation_receipt(const core::conversation_receipt& aReceipt) -> std::expected<void, application::ledger_store_failure> override
		{
			pass();
			return Target->store_conversation_receipt(aReceipt);
		}
		auto load_conversation_receipts() -> std::expected<std::vector<core::conversation_receipt>, application::ledger_store_failure> override
		{
			pass();
			return Target->load_conversation_receipts();
		}
		auto store_note(const application::local_note_record& aNote, const std::optional<application::local_note_revision> aExpected)
			-> std::expected<application::local_note_record, application::local_note_failure> override
		{
			pass();
			return Target->store_note(aNote, aExpected);
		}
		auto erase_note(const application::local_note_id aNote, const application::local_note_revision aExpected)
			-> std::expected<void, application::local_note_failure> override
		{
			pass();
			return Target->erase_note(aNote, aExpected);
		}
		auto store_last_reviewed_head(const core::git_commit_oid& aHead) -> std::expected<void, application::ledger_store_failure> override
		{
			pass();
			return Target->store_last_reviewed_head(aHead);
		}
		auto erase_last_reviewed_head() -> std::expected<void, application::ledger_store_failure> override
		{
			pass();
			return Target->erase_last_reviewed_head();
		}
		auto store_defect(const application::durable_defect_record& aDefect) -> std::expected<void, application::ledger_store_failure> override
		{
			pass();
			return Target->store_defect(aDefect);
		}
		auto erase_defect(const core::defect_id aDefect) -> std::expected<void, application::ledger_store_failure> override
		{
			pass();
			return Target->erase_defect(aDefect);
		}
		auto store_generated_mark(const core::generated_file_mark& aMark) -> std::expected<void, application::ledger_store_failure> override
		{
			pass();
			return Target->store_generated_mark(aMark);
		}
		auto erase_generated_mark(const core::file_identity& aFile) -> std::expected<void, application::ledger_store_failure> override
		{
			pass();
			return Target->erase_generated_mark(aFile);
		}
		auto load_private() -> std::expected<application::private_ledger_observation, application::ledger_store_failure> override
		{
			pass();
			return Target->load_private();
		}
		auto admit_agent_session(const application::agent_session_admission& aAdmission)
			-> std::expected<core::agent_session, application::ledger_store_failure> override
		{
			pass();
			return Target->admit_agent_session(aAdmission);
		}
		auto load_agent_session_catalog() -> std::expected<application::agent_session_catalog_state, application::ledger_store_failure> override
		{
			pass();
			return Target->load_agent_session_catalog();
		}

	private:
		void pass()
		{
			if (Gate) {
				Gate->pass();
			}
		}

		application::review_ledger_store_port* Target = nullptr;
		std::shared_ptr<store_gate>            Gate;
		bool*                                  Closed = nullptr;
	};

	[[nodiscard]] auto opener(application::review_ledger_store_port& aStore, bool* aClosed = nullptr, std::shared_ptr<store_gate> aGate = { })
		-> application::review_ledger_store_opener
	{
		return [&aStore, aClosed, aGate]() -> std::expected<std::unique_ptr<application::review_ledger_store_port>, application::ledger_store_failure> {
			return std::make_unique<forwarding_store>(aStore, aGate, aClosed);
		};
	}

	template <typename Value, typename Error = application::ledger_store_failure>
	using answer = std::optional<std::expected<Value, Error>>;

	/// One host over the manual lanes, with blocking helpers for the tests
	/// that only care about what a completed operation left.
	struct rig
	{
		rig(application::review_ledger_store_port& aStore, event_log& aLog,
			const std::size_t aCapacity = application::maximum_outstanding_ledger_operations)
			: Projection{ aLog }
			, Host{ opener(aStore, &Closed), Projection, Effects, Lane, [this](const std::expected<void, application::ledger_store_failure>& aOpened) { Opened = aOpened.has_value(); }, aCapacity, { Insights, { } }, [this](const auto&, auto) { ++FileStatistics; } }
		{
			settle();
		}

		std::shared_ptr<tests::insights_observer>   Insights       = std::make_shared<tests::insights_observer>();
		std::size_t                                 FileStatistics = 0;
		recording_projection                        Projection;
		application::manual_executor                Lane;
		application::manual_executor                Effects;
		bool                                        Closed = false;
		std::optional<bool>                         Opened;
		application::review_ledger_application_host Host;

		void settle()
		{
			while (Lane.pending() + Effects.pending() > 0U) {
				Effects.drain();
				Lane.drain();
			}
		}

		template <typename Value, typename Error = application::ledger_store_failure>
		[[nodiscard]] static auto into(answer<Value, Error>& aAnswer)
		{
			return [&aAnswer](std::expected<Value, Error> aResult) { aAnswer = std::move(aResult); };
		}

		template <typename Value, typename Error = application::ledger_store_failure>
		[[nodiscard]] auto settled(answer<Value, Error>& aAnswer) -> std::expected<Value, Error>
		{
			settle();
			expect(aAnswer.has_value(), "every operation delivers its terminal");
			return *aAnswer;
		}

		auto submit(const core::file_receipt_command& aCommand) -> core::file_receipt_outcome
		{
			auto got = answer<core::file_receipt_outcome>{ };
			Host.submit(aCommand, into(got));
			const auto result = settled(got);
			expect(result.has_value(), "a submitted command completes");
			return *result;
		}
		auto observe(const core::file_material_observation& aObservation) -> std::expected<core::file_receipt, application::ledger_store_failure>
		{
			auto got = answer<core::file_receipt>{ };
			Host.observe_material(aObservation, into(got));
			return settled(got);
		}
		auto refresh() -> std::expected<core::file_receipt, application::ledger_store_failure>
		{
			auto got = answer<core::file_receipt>{ };
			Host.refresh_receipt(into(got));
			return settled(got);
		}
		auto load_receipt(const core::file_identity& aFile) -> std::expected<std::optional<core::file_receipt>, application::ledger_store_failure>
		{
			if (auto cached = Host.cached_receipt(aFile)) {
				return *cached;
			}
			auto got = answer<void>{ };
			Host.load_receipts({ aFile }, into(got));
			if (const auto loaded = settled(got); !loaded) {
				return std::unexpected(loaded.error());
			}
			const auto cached = Host.cached_receipt(aFile);
			expect(cached.has_value(), "a successful read is retained");
			return *cached;
		}
		auto observe_conversations(const std::vector<application::conversation_thread_observation>& aThreads)
			-> std::expected<application::conversation_read_state, application::ledger_store_failure>
		{
			if (auto state = Host.unread_conversations(aThreads)) {
				return *state;
			}
			auto got = answer<void>{ };
			Host.load_conversation_receipts(into(got));
			if (const auto loaded = settled(got); !loaded) {
				return std::unexpected(loaded.error());
			}
			return *Host.unread_conversations(aThreads);
		}
		auto mark(const core::conversation_observation& aLatest) -> std::expected<void, application::ledger_store_failure>
		{
			auto got = answer<void>{ };
			Host.mark_conversation_read(aLatest, into(got));
			return settled(got);
		}
		auto unmark(const application::conversation_thread_observation& aThread) -> std::expected<void, application::ledger_store_failure>
		{
			auto got = answer<void>{ };
			Host.mark_conversation_unread(aThread, into(got));
			return settled(got);
		}
		/// Loads first, as a caller that has never observed would.
		auto private_observation() -> std::expected<application::private_ledger_observation, application::ledger_store_failure>
		{
			auto got = answer<void>{ };
			Host.load_private(into(got));
			if (const auto loaded = settled(got); !loaded) {
				return std::unexpected(loaded.error());
			}
			return Host.private_observation();
		}
		auto persist(const application::local_note_record& aNote, const std::optional<application::local_note_revision> aExpected = std::nullopt)
			-> std::expected<application::local_note_record, application::local_note_failure>
		{
			auto got = answer<application::local_note_record, application::local_note_failure>{ };
			Host.persist(aNote, aExpected, into<application::local_note_record, application::local_note_failure>(got));
			return settled(got);
		}
		auto erase(const application::local_note_id aNote, const application::local_note_revision aExpected)
			-> std::expected<void, application::local_note_failure>
		{
			auto got = answer<void, application::local_note_failure>{ };
			Host.erase(aNote, aExpected, into<void, application::local_note_failure>(got));
			return settled(got);
		}
		auto persist(const application::durable_defect_record& aDefect) -> std::expected<void, application::ledger_store_failure>
		{
			auto got = answer<void>{ };
			Host.persist(aDefect, into(got));
			return settled(got);
		}
		auto erase(const core::defect_id aDefect) -> std::expected<void, application::ledger_store_failure>
		{
			auto got = answer<void>{ };
			Host.erase(aDefect, into(got));
			return settled(got);
		}
		auto persist_head(const core::git_commit_oid& aHead) -> std::expected<void, application::ledger_store_failure>
		{
			auto got = answer<void>{ };
			Host.persist_last_reviewed_head(aHead, into(got));
			return settled(got);
		}
		auto erase_head() -> std::expected<void, application::ledger_store_failure>
		{
			auto got = answer<void>{ };
			Host.erase_last_reviewed_head(into(got));
			return settled(got);
		}
		auto persist_mark(const core::generated_file_mark& aMark) -> std::expected<void, application::ledger_store_failure>
		{
			auto got = answer<void>{ };
			Host.persist_generated_mark(aMark, into(got));
			return settled(got);
		}
		auto admit(const application::agent_session_admission& aAdmission) -> std::expected<core::agent_session, application::ledger_store_failure>
		{
			auto got = answer<core::agent_session>{ };
			Host.admit(aAdmission, into(got));
			return settled(got);
		}
		auto catalog() -> std::expected<application::agent_session_catalog_state, application::ledger_store_failure>
		{
			auto got = answer<application::agent_session_catalog_state>{ };
			Host.session_catalog(into(got));
			return settled(got);
		}
	};

	// The ordering is the contract. A projection published before the commit
	// would show a reader a state the database does not hold, and would survive
	// exactly until the next restart contradicted it.
	void test_the_projection_follows_the_commit()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		auto       rigged = rig{ store, log };
		const auto seed   = seeded_receipt();
		rigged.Host.retain(seed);

		const auto outcome = rigged.submit(
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
			rigged.Projection.Published.size() == 1U
				&& rigged.Projection.Published.front().Receipt == store.Committed,
			"the published projection is exactly what was committed");
		expect(rigged.FileStatistics == 1, "file statistics follow a successful explicit receipt commit");
		expect(
			rigged.Host.retained().Read == core::file_read_state::read,
			"the host adopts the committed receipt");
	}

	void test_a_refused_command_never_reaches_the_store()
	{
		auto log      = event_log{ };
		auto store    = recording_store{ log };
		auto rigged   = rig{ store, log };
		auto seed     = seeded_receipt();
		seed.Excluded = true;
		rigged.Host.retain(seed);

		const auto outcome = rigged.submit(
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
			rigged.Projection.Failures.size() == 1U
				&& rigged.Projection.Failures.front().Rejection == outcome.Rejection
				&& !rigged.Projection.Failures.front().Store.has_value(),
			"the refusal is published with the core's reason and no store reason");
		expect(
			rigged.Host.retained() == seed,
			"the retained receipt is untouched by a refused command");
		expect(rigged.Effects.pending() == 0U && rigged.Host.statistics().Operations == 1U,
			   "a refused command posts no storage work beyond the store's opening");
	}

	// A failed commit must leave the host agreeing with the database. Adopting
	// the rejected receipt would leave the two disagreeing until something else
	// happened to overwrite it.
	void test_a_failed_commit_changes_nothing()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		auto       rigged = rig{ store, log };
		const auto seed   = seeded_receipt();
		rigged.Host.retain(seed);
		store.fail_next(application::ledger_store_failure::conflict);

		const auto outcome = rigged.submit(
			command_for(seed, core::file_receipt_command_kind::accept));

		expect(
			log == event_log{ "request", "commit-refused", "failed" },
			"the failure is published after the refused commit, not before it");
		expect(
			rigged.Projection.Published.empty(),
			"a failed commit publishes no accepted projection");
		expect(
			rigged.Projection.Failures.size() == 1U
				&& rigged.Projection.Failures.front().Store
					== application::ledger_store_failure::conflict
				&& rigged.Projection.Failures.front().Rejection
					== core::file_receipt_rejection::none,
			"the failure carries the store's reason and no core rejection");
		expect(
			rigged.Host.retained() == seed && !outcome.Changed,
			"the retained receipt keeps the value the database still holds");

		// The next command must still work: a failure is not a poisoned host.
		const auto recovered = rigged.submit(
			command_for(seed, core::file_receipt_command_kind::accept));
		expect(
			recovered.Changed && rigged.Host.retained().Accepted,
			"a later command succeeds after a store failure");

		store.ThrowOnCommit = true;
		const auto thrown   = rigged.submit(command_for(rigged.Host.retained(), core::file_receipt_command_kind::mark_read));
		expect(!thrown.Changed && rigged.Projection.Failures.back().Store == application::ledger_store_failure::unavailable
				   && rigged.Host.retained().Read != core::file_read_state::read,
			   "a store that throws is a typed unavailable failure, not a lost terminal");
	}

	void test_an_unchanged_command_is_not_a_write()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		auto       rigged = rig{ store, log };
		const auto seed   = seeded_receipt();
		rigged.Host.retain(seed);

		expect(
			rigged.submit(command_for(seed, core::file_receipt_command_kind::accept))
				.Changed,
			"the first acceptance changes the receipt");
		const auto before = log.size();

		const auto again = rigged.submit(
			command_for(seed, core::file_receipt_command_kind::accept));

		expect(
			again.Rejection == core::file_receipt_rejection::none && !again.Changed,
			"re-issuing the command is legal and changes nothing");
		expect(
			log.size() == before,
			"a command that moves nothing produces no request, commit, or "
			"publication");
		expect(
			rigged.Host.statistics().Unchanged == 1U
				&& rigged.Host.statistics().PersistenceRequests == 1U,
			"the unchanged command is counted without being persisted");
	}

	void test_statistics_account_for_every_command()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		auto       rigged = rig{ store, log };
		const auto seed   = seeded_receipt();
		rigged.Host.retain(seed);

		static_cast<void>(rigged.submit(command_for(seed, core::file_receipt_command_kind::mark_read)));
		static_cast<void>(rigged.submit(command_for(seed, core::file_receipt_command_kind::mark_read)));
		store.fail_next(application::ledger_store_failure::unavailable);
		static_cast<void>(rigged.submit(command_for(seed, core::file_receipt_command_kind::accept)));
		auto elsewhere = command_for(seed, core::file_receipt_command_kind::accept);
		elsewhere.File = opaque<core::file_identity>(9U);
		static_cast<void>(rigged.submit(elsewhere));

		const auto statistics = rigged.Host.statistics();
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
		expect(statistics.Operations == 3U && statistics.PeakOutstanding == 1U,
			   "the opening and the two commits were the only storage operations");
	}

	void test_private_records_are_validated_before_the_store()
	{
		auto log    = event_log{ };
		auto store  = recording_store{ log };
		auto rigged = rig{ store, log };

		auto note = private_note();
		note.Note.Text.clear();
		const auto badNote = rigged.persist(note);
		const auto badHead = rigged.persist_head(core::git_commit_oid{ });
		auto       defect  = private_defect();
		defect.History.Defect.Anchor.Line += 1U;
		const auto badDefect = rigged.persist(defect);
		const auto badErase  = rigged.erase(application::local_note_id{ }, application::local_note_revision{ 1U });

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
			rigged.Host.statistics().PrivateWrites == 0U
				&& rigged.Host.statistics().PrivateDeletes == 0U
				&& rigged.Host.statistics().PrivateFailures == 4U
				&& rigged.Host.statistics().Operations == 1U,
			"validation failures are counted without false writes, deletes or storage work");
	}

	/// \brief RFC-BPR-0089 M2. Generated marks are stored, replaced and
	/// removed through the host, kept sorted in the private observation, and
	/// a refused or invalid write changes nothing.
	void test_generated_marks_round_trip_through_the_host()
	{
		auto       log     = event_log{ };
		auto       store   = recording_store{ log };
		auto       rigged  = rig{ store, log };
		const auto lock    = core::file_identity_of("package-lock.json");
		const auto gen     = core::file_identity_of("gen/out.c");
		const auto first   = lock < gen ? lock : gen;
		const auto second  = lock < gen ? gen : lock;
		const auto treated = core::generated_file_mark{ .File = second, .Mark = core::generated_mark::treat_as_generated, .ReviewAnyway = false };
		const auto anyway  = core::generated_file_mark{ .File = first, .Mark = core::generated_mark::none, .ReviewAnyway = true };

		expect(rigged.private_observation().has_value(), "the private observation loads");
		expect(rigged.persist_mark(treated).has_value(), "Treat as generated is stored");
		expect(rigged.persist_mark(anyway).has_value(), "Review anyway is stored");
		auto observed = rigged.Host.private_observation();
		expect(observed && observed->GeneratedMarks.Marks == std::vector{ anyway, treated } && core::valid(observed->GeneratedMarks),
			   "the host keeps the marks sorted by file");

		auto normal = treated;
		normal.Mark = core::generated_mark::show_as_normal;
		expect(rigged.persist_mark(normal).has_value(), "Show as normal replaces the mark");
		observed = rigged.Host.private_observation();
		expect(observed && observed->GeneratedMarks.Marks == std::vector{ anyway, normal }, "a replaced mark is kept in place");

		store.fail_next(application::ledger_store_failure::unavailable);
		auto refusedMark   = anyway;
		refusedMark.Mark   = core::generated_mark::treat_as_generated;
		const auto refused = rigged.persist_mark(refusedMark);
		expect(!refused && refused.error() == application::ledger_store_failure::unavailable, "a refused write stays typed");
		expect(rigged.Host.private_observation()->GeneratedMarks.Marks == std::vector{ anyway, normal }, "a refused write changes nothing");

		const auto silent = core::generated_file_mark{ .File = first, .Mark = core::generated_mark::none, .ReviewAnyway = false };
		expect(rigged.persist_mark(silent).has_value(), "a mark that says nothing removes the row");
		expect(store.GeneratedErases == std::vector{ first }, "the store erased it rather than writing an empty mark");
		expect(rigged.Host.private_observation()->GeneratedMarks.Marks == std::vector{ normal }, "the removed mark leaves the observation");

		const auto writes  = store.GeneratedWrites.size();
		const auto invalid = rigged.persist_mark(core::generated_file_mark{ .File = core::file_identity{ }, .Mark = core::generated_mark::treat_as_generated, .ReviewAnyway = false });
		expect(!invalid && invalid.error() == application::ledger_store_failure::invalid_record, "an invalid file is refused before the store");
		expect(store.GeneratedWrites.size() == writes, "nothing reached the store");

		const auto reloaded = rigged.private_observation();
		expect(reloaded && reloaded->GeneratedMarks.Marks == std::vector{ normal }, "the store holds exactly what the host kept");
	}

	void test_private_records_round_trip_and_delete_through_the_host()
	{
		auto log          = event_log{ };
		auto store        = recording_store{ log };
		auto rigged       = rig{ store, log };
		auto note         = private_note();
		note.Revision     = application::local_note_revision{ 1U };
		const auto head   = reviewed_head();
		const auto defect = private_defect();

		expect(!rigged.Host.private_observation().has_value(), "nothing private is answered before it is loaded");
		expect(rigged.persist(note).has_value(), "the note commits");
		expect(
			rigged.persist_head(head).has_value(),
			"the reviewed head commits");
		expect(rigged.persist(defect).has_value(), "the defect commits");

		const auto observed = rigged.private_observation();
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
		const auto refused = rigged.persist(changed);
		expect(
			!refused
				&& refused.error().Reason == application::ledger_store_failure::conflict,
			"a private store failure remains typed");
		expect(
			store.Private.Notes == std::vector{ note } && rigged.Host.private_observation()->Notes == std::vector{ note },
			"a failed private write leaves the durable and the retained observation unchanged");

		expect(rigged.erase(note.Id, note.Revision).has_value(), "the note deletes");
		expect(
			rigged.erase_head().has_value(),
			"the reviewed head deletes");
		expect(
			rigged.erase(defect.History.Defect.Id).has_value(),
			"the defect deletes");
		expect(rigged.Host.private_observation() == application::private_ledger_observation{ },
			   "each committed delete is retained without rereading the store");
		const auto empty = rigged.private_observation();
		expect(
			empty.has_value()
				&& *empty == application::private_ledger_observation{ },
			"deletion returns the exact scope to an empty private observation");

		expect(rigged.Insights->reviews().size() == 2, "one note and one defect creation count; failed writes and deletes add nothing");
		const auto statistics = rigged.Host.statistics();
		expect(
			statistics.PrivateWrites == 4U
				&& statistics.PrivateDeletes == 3U
				&& statistics.PrivateLoads == 2U
				&& statistics.PrivateFailures == 1U,
			"private statistics distinguish writes, deletes, loads, and failures");
	}

	void test_material_observation_loads_carries_and_publishes_in_order()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		auto       rigged = rig{ store, log };
		const auto seed   = seeded_receipt();
		const auto first  = rigged.observe(core::file_material_observation{
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
		const auto carried     = rigged.observe(
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
		expect(rigged.Host.statistics().ReceiptLoads == 2U
				   && rigged.Host.statistics().ReceiptLoadHits == 1U
				   && rigged.Host.statistics().MaterialReceiptHits == 1U
				   && rigged.Host.statistics().PersistenceRequests == 1U,
			   "material observation distinguishes loads, hits, and carry-over writes");
	}

	void test_material_load_failure_is_typed_and_changes_nothing()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		auto       rigged = rig{ store, log };
		const auto seed   = seeded_receipt();
		rigged.Host.retain(seed);
		store.fail_next(application::ledger_store_failure::unavailable);

		const auto observed = rigged.observe(
			core::file_material_observation{
				.File     = seed.File,
				.Material = opaque<core::file_material_id>(33U),
			});

		expect(!observed
				   && observed.error()
					   == application::ledger_store_failure::unavailable,
			   "a load failure stays typed");
		expect(rigged.Host.retained() == seed && store.Requests.empty(),
			   "a failed load neither carries nor persists a receipt");
		expect(log == event_log{ "load-refused", "failed" },
			   "the typed load refusal reaches projection after the store terminal");
	}

	void test_agent_sessions_validate_admit_and_load_through_the_host()
	{
		auto log                  = event_log{ };
		auto store                = recording_store{ log };
		auto rigged               = rig{ store, log };
		auto admission            = session_admission();
		auto invalid              = admission;
		invalid.Binding.LeftEpoch = core::epoch_ordinal{ };

		const auto refused = rigged.admit(invalid);
		expect(
			!refused
				&& refused.error()
					== application::ledger_store_failure::invalid_record
				&& store.Admissions.empty(),
			"an invalid session never reaches durable admission");

		const auto admitted = rigged.admit(admission);
		const auto catalog  = rigged.catalog();
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
			rigged.Host.statistics().SessionAdmissions == 1U
				&& rigged.Host.statistics().SessionLoads == 1U
				&& rigged.Host.statistics().SessionFailures == 1U,
			"session statistics distinguish admission, load, and pre-store refusal");
	}

	// [[BPR-0656]]. The read state of a review's conversations: receipts are
	// loaded once, every post after the cursor is unread, Mark read writes the
	// cursor through the store before the host keeps it, and a refused store
	// leaves the next observation asking again.
	void test_conversation_read_state_loads_once_and_marks_read_through_the_store()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		auto       rigged = rig{ store, log };
		const auto thread = core::review_conversation_id{ 41U };
		const auto posts  = std::vector<core::conversation_post_observation>{
			{ .Comment = core::review_comment_id{ 1U }, .Edit = core::comment_edit_version{ 11U } },
			{ .Comment = core::review_comment_id{ 2U }, .Edit = core::comment_edit_version{ 12U }, .Mine = true },
			{ .Comment = core::review_comment_id{ 3U }, .Edit = core::comment_edit_version{ 13U } },
		};
		const auto threads = std::vector<application::conversation_thread_observation>{
			{ .Conversation = thread, .Posts = posts },
			{ .Conversation = core::review_conversation_id{ 42U },
			  .Posts        = { { .Comment = core::review_comment_id{ 9U }, .Edit = core::comment_edit_version{ 19U }, .Mine = true } } },
		};

		expect(!rigged.Host.unread_conversations(threads).has_value(), "nothing is decided before the receipts are loaded");
		store.fail_next(application::ledger_store_failure::unavailable);
		const auto refused = rigged.observe_conversations(threads);
		expect(!refused.has_value()
				   && refused.error() == application::ledger_store_failure::unavailable
				   && log == event_log{ "conversation-load-refused" },
			   "a refused receipt load is typed and caches nothing");
		log.clear();

		const auto fresh = rigged.observe_conversations(threads);
		expect(fresh.has_value() && fresh->UnreadThreads == 1U
				   && fresh->Threads.size() == 1U
				   && fresh->Threads[0].Conversation == thread
				   && !fresh->Threads[0].SeenBefore
				   && fresh->Threads[0].UnreadPosts
					   == std::vector{ core::review_comment_id{ 1U }, core::review_comment_id{ 3U } },
			   "with no receipt every post of someone else's is unread and my own "
			   "thread is not");
		const auto again = rigged.observe_conversations(threads);
		expect(again == fresh && log == event_log{ "conversation-loaded" },
			   "the receipts are loaded once and reused");

		store.fail_next(application::ledger_store_failure::quota_exceeded);
		const auto refusedWrite = rigged.mark(core::conversation_observation{
			.Conversation  = thread,
			.LatestComment = core::review_comment_id{ 3U },
			.LatestEdit    = core::comment_edit_version{ 13U },
		});
		expect(!refusedWrite.has_value()
				   && refusedWrite.error() == application::ledger_store_failure::quota_exceeded
				   && rigged.observe_conversations(threads) == fresh,
			   "a refused Mark read changes nothing the host reports");

		const auto marked = rigged.mark(core::conversation_observation{
			.Conversation  = thread,
			.LatestComment = core::review_comment_id{ 3U },
			.LatestEdit    = core::comment_edit_version{ 13U },
		});
		expect(marked.has_value() && store.ConversationReceipts.size() == 1U
				   && store.ConversationReceipts[0].SeenComment == core::review_comment_id{ 3U }
				   && store.ConversationReceipts[0].AcceptedComment == core::review_comment_id{ },
			   "Mark read stores the seen cursor at the latest post and leaves acceptance");
		const auto read = rigged.observe_conversations(threads);
		expect(read.has_value() && read->UnreadThreads == 0U && read->Threads.empty(),
			   "a read conversation reports nothing unread");

		auto edited             = threads;
		edited[0].Posts[2].Edit = core::comment_edit_version{ 14U };
		const auto reopened     = rigged.observe_conversations(edited);
		expect(reopened.has_value() && reopened->UnreadThreads == 1U
				   && reopened->Threads[0].UnreadPosts == std::vector{ core::review_comment_id{ 3U } }
				   && reopened->Threads[0].SeenBefore,
			   "an edit to the seen post reopens exactly that post, in a conversation read before");

		const auto repeat = rigged.mark(core::conversation_observation{
			.Conversation  = thread,
			.LatestComment = core::review_comment_id{ 3U },
			.LatestEdit    = core::comment_edit_version{ 13U },
		});
		expect(repeat.has_value() && store.ConversationReceipts.size() == 1U,
			   "an unchanged Mark read is not a write");
		expect(!rigged.mark(core::conversation_observation{ }).has_value(),
			   "a nameless conversation is refused before the store");
		const auto statistics = rigged.Host.statistics();
		expect(statistics.ConversationLoads == 2U && statistics.ConversationWrites == 2U
				   && statistics.ConversationFailures == 2U && statistics.Unchanged == 1U,
			   "statistics account for every conversation load, write and refusal");

		// A cold cache: Mark read loads the receipts first, in one operation.
		auto       cold   = rig{ store, log };
		const auto before = store.ConversationReceipts;
		expect(cold.mark(core::conversation_observation{
							 .Conversation  = core::review_conversation_id{ 42U },
							 .LatestComment = core::review_comment_id{ 9U },
							 .LatestEdit    = core::comment_edit_version{ 19U },
						 })
					   .has_value()
				   && cold.Host.statistics().ConversationLoads == 1U && store.ConversationReceipts.size() == before.size() + 1U
				   && cold.Host.unread_conversations(threads).has_value(),
			   "a cold Mark read loads, decides against the loaded receipts, writes and keeps them");
	}

	/// [[BPR-1582]]. Mark as unread goes through the same store: the latest
	/// post someone else wrote is new again, acceptance stays, a thread of
	/// only my posts is refused before the store, and Mark read undoes it.
	// [[BPR-1601]]. The same seen-before rule applies to singleton posts,
	// including a durable zero cursor after Mark unread and a reconstructed host.
	void test_singleton_conversations_seen_before_and_restart()
	{
		auto log   = event_log{ };
		auto store = recording_store{ log };
		auto host  = rig{ store, log };
		auto posts = std::vector<application::conversation_thread_observation>{
			{ .Conversation = core::review_conversation_id{ 61U }, .Posts = { { .Comment = core::review_comment_id{ 71U }, .Edit = core::comment_edit_version{ 81U } } } },
			{ .Conversation = core::review_conversation_id{ 62U }, .Posts = { { .Comment = core::review_comment_id{ 72U }, .Edit = core::comment_edit_version{ 82U } } } },
			{ .Conversation = core::review_conversation_id{ 63U }, .Posts = { { .Comment = core::review_comment_id{ 73U }, .Edit = core::comment_edit_version{ 83U }, .Mine = true } } },
		};
		const auto fresh = host.observe_conversations(posts);
		expect(fresh && fresh->Threads.size() == 2U && !fresh->Threads[0].SeenBefore && !fresh->Threads[1].SeenBefore, "new PR comment and summary have no seen cursor; my post is excluded");
		for (const auto& post : posts) {
			expect(host.mark({ .Conversation = post.Conversation, .LatestComment = post.Posts[0].Comment, .LatestEdit = post.Posts[0].Edit }).has_value(), "singleton read commits");
		}
		expect(host.observe_conversations(posts)->Threads.empty(), "both singleton kinds read");
		expect(host.unmark(posts[0]).has_value(), "manual unread commits a singleton zero cursor");
		posts[1].Posts[0].Edit = core::comment_edit_version{ 84U };
		auto       restarted   = rig{ store, log };
		const auto state       = restarted.observe_conversations(posts);
		expect(state && state->Threads.size() == 2U && state->Threads[0].SeenBefore && state->Threads[1].SeenBefore, "restart preserves seen-before for manual unread and an edited summary");
		expect(state->Threads[0].UnreadPosts == std::vector{ posts[0].Posts[0].Comment } && state->Threads[1].UnreadPosts == std::vector{ posts[1].Posts[0].Comment }, "restart identifies the exact two unread posts");
	}

	void test_conversation_marks_unread_through_the_store()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		auto       rigged = rig{ store, log };
		const auto thread = core::review_conversation_id{ 51U };
		const auto posts  = std::vector<core::conversation_post_observation>{
			{ .Comment = core::review_comment_id{ 1U }, .Edit = core::comment_edit_version{ 11U } },
			{ .Comment = core::review_comment_id{ 2U }, .Edit = core::comment_edit_version{ 12U } },
			{ .Comment = core::review_comment_id{ 3U }, .Edit = core::comment_edit_version{ 13U }, .Mine = true },
		};
		const auto threads = std::vector<application::conversation_thread_observation>{ { .Conversation = thread, .Posts = posts } };
		expect(rigged.mark(core::conversation_observation{ .Conversation = thread, .LatestComment = core::review_comment_id{ 3U }, .LatestEdit = core::comment_edit_version{ 13U } }).has_value()
				   && rigged.observe_conversations(threads)->Threads.empty(),
			   "the fixture starts read");
		const auto unread = rigged.unmark(threads[0]);
		const auto state  = rigged.observe_conversations(threads);
		expect(unread.has_value() && store.ConversationReceipts.size() == 1U && store.ConversationReceipts[0].SeenComment == core::review_comment_id{ 1U }
				   && state->UnreadThreads == 1U && state->Threads[0].UnreadPosts == std::vector{ core::review_comment_id{ 2U } } && state->Threads[0].SeenBefore,
			   "Mark as unread stores the cursor before the latest other post, which is new again");
		const auto mine    = application::conversation_thread_observation{ .Conversation = core::review_conversation_id{ 52U }, .Posts = { posts[2] } };
		const auto writes  = rigged.Host.statistics().ConversationWrites;
		const auto refused = rigged.unmark(mine);
		expect(!refused.has_value() && refused.error() == application::ledger_store_failure::invalid_record && rigged.Host.statistics().ConversationWrites == writes,
			   "a thread of only my posts is refused before the store");
		expect(rigged.mark(core::conversation_observation{ .Conversation = thread, .LatestComment = core::review_comment_id{ 3U }, .LatestEdit = core::comment_edit_version{ 13U } }).has_value()
				   && rigged.observe_conversations(threads)->Threads.empty(),
			   "Mark read reads it again");
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
		auto       log       = event_log{ };
		auto       store     = recording_store{ log };
		auto       rigged    = rig{ store, log };
		const auto candidate = private_note();
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
			const auto refused   = rigged.persist(candidate);
			const auto retained  = rigged.Host.rejected_note();
			expect(!refused && refused.error().Reason == application::ledger_store_failure::integrity_failure
					   && !refused.error().Current && retained && retained->Candidate == candidate
					   && !retained->Failure.Current,
				   "foreign, unrevisioned and oversized competitors never enter the write projection");
			rigged.Host.dismiss_rejected_note();
			const auto erased = rigged.erase(candidate.Id, application::local_note_revision{ 1U });
			expect(!erased && erased.error().Reason == application::ledger_store_failure::integrity_failure
					   && !erased.error().Current && rigged.Host.rejected_note() && !rigged.Host.rejected_note()->Failure.Current,
				   "delete conflicts apply the same bounded exact-identity observation guard");
			rigged.Host.dismiss_rejected_note();
		}
	}

	void test_note_conflicts_retain_exact_window_candidate()
	{
		auto       log     = event_log{ };
		auto       store   = recording_store{ log };
		auto       first   = rig{ store, log };
		auto       second  = rig{ store, log };
		const auto created = first.persist(private_note());
		expect(created.has_value(), "the initial note is accepted");
		auto winner         = *created;
		winner.Note.Text    = "accepted elsewhere";
		const auto accepted = first.persist(winner, created->Revision);
		expect(accepted.has_value(), "the first window advances the durable note");
		auto candidate      = *created;
		candidate.Note.Text = std::string{ "local\0candidate\n", 16U };
		const auto refused  = second.persist(candidate, created->Revision);
		const auto retained = second.Host.rejected_note();
		const auto durable  = second.private_observation();
		expect(!refused && refused.error().Reason == application::ledger_store_failure::conflict
				   && refused.error().Current == *accepted && retained
				   && retained->Candidate == candidate && retained->Failure == refused.error()
				   && retained->ExpectedRevision == created->Revision,
			   "conflict retains every candidate byte separately from the accepted competitor");
		expect(durable && durable->Notes == std::vector{ *accepted } && !first.Host.rejected_note(),
			   "a rejected window cannot overwrite shared durable state or another window projection");
		expect(second.Host.rejected_note() == retained, "reading durable state does not discard the local candidate");
		const auto laterDelete    = second.erase(created->Id, created->Revision);
		auto       laterCandidate = candidate;
		laterCandidate.Note.Text  = "another refused edit";
		const auto laterEdit      = second.persist(laterCandidate, created->Revision);
		expect(!laterDelete && !laterEdit && second.Host.rejected_note() == retained,
			   "later rejected edits and deletes cannot replace protected candidate bytes");
		auto resolved            = *accepted;
		resolved.Note.Text       = "explicit later accepted edit";
		const auto laterAccepted = second.persist(resolved, accepted->Revision);
		expect(laterAccepted && second.Host.rejected_note() == retained,
			   "even a successful durable write does not implicitly dismiss the protected candidate");
		second.Host.dismiss_rejected_note();
		expect(!second.Host.rejected_note(), "candidate dismissal is explicit");
		const auto deleted = second.erase(created->Id, created->Revision);
		expect(!deleted && second.Host.rejected_note() && !second.Host.rejected_note()->Candidate
				   && second.Host.rejected_note()->Failure.Current == *laterAccepted,
			   "stale deletion has a distinct typed projection with its accepted competitor");
	}

	void test_accepted_write_observer()
	{
		auto       log     = event_log{ };
		auto       owned   = std::make_unique<recording_store>(log);
		auto*      raw     = owned.get();
		auto       changes = std::vector<application::ledger_change>{ };
		auto       store   = application::observing_review_ledger_store{ std::move(owned), [&changes](application::ledger_change aChange) { changes.push_back(aChange); } };
		const auto receipt = seeded_receipt();
		raw->fail_next(application::ledger_store_failure::unavailable);
		expect(!store.commit({ .Receipt = receipt }) && changes.empty(), "failed commit emits no invalidation");
		expect(store.commit({ .Receipt = receipt }).has_value() && changes == std::vector{ application::ledger_change{ .Domain = application::ledger_change_domain::receipt, .File = receipt.File } }, "receipt success carries exact file");
		const auto created = store.store_note(private_note());
		expect(created && created->Revision.value() == 1U, "direct agent-style note write forwards assigned revision");
		const auto count   = changes.size();
		const auto refused = store.store_note(private_note());
		expect(!refused && refused.error().Current == *created && changes.size() == count, "CAS conflict preserves current record without notification");
		const auto edited = store.store_note(private_note(), created->Revision);
		expect(edited && edited->Revision.value() == 2U, "expected revision forwards exactly");
		expect(!store.erase_note(edited->Id, created->Revision), "stale delete refused");
		expect(store.erase_note(edited->Id, edited->Revision).has_value(), "exact delete forwarded");
		expect(store.store_conversation_receipt({ .Conversation = core::review_conversation_id{ 1U } }).has_value() && changes.back().Domain == application::ledger_change_domain::conversation, "conversation success identifies domain");
		expect(store.store_last_reviewed_head(reviewed_head()).has_value(), "head write forwarded");
		expect(store.erase_last_reviewed_head().has_value(), "head delete forwarded");
		expect(store.store_generated_mark({ .File = core::file_identity_of("gen.c"), .Mark = core::generated_mark::treat_as_generated, .ReviewAnyway = false }).has_value(),
			   "generated mark write forwarded");
		expect(store.erase_generated_mark(core::file_identity_of("gen.c")).has_value(), "generated mark delete forwarded");
		expect(store.store_defect(private_defect()).has_value(), "defect write forwarded");
		expect(store.erase_defect(private_defect().History.Defect.Id).has_value(), "defect delete forwarded");
		expect(store.admit_agent_session(session_admission()).has_value(), "session admission forwarded");
		const auto afterWrites = changes.size();
		expect(afterWrites == 12U && changes.back().Domain == application::ledger_change_domain::private_records, "every successful mutation signals exactly once");
		const auto failsSilently = [&raw, &changes, afterWrites](auto aOperation) {
			raw->fail_next(application::ledger_store_failure::unavailable);
			expect(!aOperation() && changes.size() == afterWrites, "failed mutation forwards failure without signal");
		};
		failsSilently([&store] { return store.store_note(private_note()); });
		failsSilently([&store, &edited] { return store.erase_note(edited->Id, edited->Revision); });
		failsSilently([&store] { return store.store_conversation_receipt({ .Conversation = core::review_conversation_id{ 1U } }); });
		failsSilently([&store] { return store.store_last_reviewed_head(reviewed_head()); });
		failsSilently([&store] { return store.erase_last_reviewed_head(); });
		failsSilently([&store] { return store.store_generated_mark({ .File = core::file_identity_of("gen.c"), .Mark = core::generated_mark::treat_as_generated, .ReviewAnyway = false }); });
		failsSilently([&store] { return store.erase_generated_mark(core::file_identity_of("gen.c")); });
		failsSilently([&store] { return store.store_defect(private_defect()); });
		failsSilently([&store] { return store.erase_defect(private_defect().History.Defect.Id); });
		failsSilently([&store] { return store.admit_agent_session(session_admission()); });
		expect(store.load(receipt.File).has_value() && store.load_private().has_value() && store.load_conversation_receipts().has_value() && store.load_agent_session_catalog().has_value() && changes.size() == afterWrites, "all reads remain silent");
		raw->ThrowOnLoad = true;
		auto threw       = false;
		try {
			static_cast<void>(store.load(receipt.File));
		} catch (const std::runtime_error&) {
			threw = true;
		}
		expect(threw && changes.size() == afterWrites, "store exceptions propagate without signal");
		raw->ThrowOnCommit = true;
		threw              = false;
		try {
			static_cast<void>(store.commit({ .Receipt = receipt }));
		} catch (const std::runtime_error&) {
			threw = true;
		}
		expect(threw && changes.size() == afterWrites, "store write exceptions propagate without signal");
		auto throwing = application::observing_review_ledger_store{ std::make_unique<recording_store>(log), [](application::ledger_change) { throw std::runtime_error{ "observer failure" }; } };
		expect(throwing.commit({ .Receipt = receipt }).has_value() && throwing.load(receipt.File).value() == receipt, "observer exception cannot veto durable success");
	}

	void test_sibling_refresh_is_observation_only()
	{
		auto log      = event_log{ };
		auto store    = recording_store{ log };
		auto rigged   = rig{ store, log };
		auto selected = seeded_receipt();
		rigged.Host.retain(selected);
		auto accepted     = selected;
		accepted.Accepted = true;
		accepted.Read     = core::file_read_state::read;
		store.Committed   = accepted;
		expect(rigged.refresh().value() == accepted && rigged.Projection.Published.back().Receipt == accepted, "same material accepts sibling receipt");
		// [[BPR-0824]]. The composing window's own commit echo reaches here too.
		expect(rigged.Host.statistics().ReceiptLoadHits == 1U && rigged.Host.statistics().MaterialReceiptHits == 0U, "a refresh hit is not a material-observation hit");
		selected.Material = opaque<core::file_material_id>(99U);
		rigged.Host.retain(selected);
		const auto refreshed = rigged.refresh();
		expect(refreshed && refreshed->File == selected.File && refreshed->Material == selected.Material && !refreshed->Accepted && refreshed->Read == core::file_read_state::unread, "different selected material carries through core in memory");
		expect(store.Requests.empty() && store.Committed == accepted, "sibling carry never overwrites durable receipt");
		store.Committed->File    = opaque<core::file_identity>(88U);
		const auto beforeFailure = rigged.Host.retained();
		expect(!rigged.refresh() && rigged.Host.retained() == beforeFailure, "wrong file store response cannot replace retained selection");
		store.Committed.reset();
		expect(rigged.refresh().value() == selected, "absent durable receipt resets exact selected material in memory");
		expect(store.LastLoadedFile == selected.File, "refresh loads only currently retained file");
		const auto note = rigged.persist(private_note());
		expect(note.has_value(), "seed accepted note");
		expect(!rigged.persist(private_note()), "retain rejected candidate");
		const auto rejected = rigged.Host.rejected_note();
		expect(rigged.refresh().has_value() && rigged.Host.rejected_note() == rejected, "receipt refresh preserves rejected note candidate");
		expect(rigged.observe_conversations({ }).has_value(), "prime conversation cache");
		const auto loads = rigged.Host.statistics().ConversationLoads;
		rigged.Host.invalidate_conversation_receipts();
		expect(!rigged.Host.unread_conversations({ }).has_value(), "an invalidated cache answers nothing");
		expect(rigged.observe_conversations({ }).has_value() && rigged.Host.statistics().ConversationLoads == loads + 1U && store.Requests.empty(), "sibling conversation invalidation reloads without writes");
	}

	/// [[BPR-0972]]. The overview's per-file reads are answered from durable
	/// receipts the host retains; only another writer's notification may
	/// make one stale, and only a durable change moves the revision.
	void test_durable_receipts_are_retained_until_another_writer_invalidates_them()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		auto       rigged = rig{ store, log };
		const auto seed   = seeded_receipt();
		const auto other  = opaque<core::file_identity>(7U);

		expect(!rigged.Host.cached_receipt(seed.File).has_value(), "nothing is held before it is read");
		expect(rigged.load_receipt(seed.File).has_value() && !*rigged.load_receipt(seed.File),
			   "an absent durable receipt is answered twice");
		expect(rigged.Host.statistics().ReceiptLoads == 1U && rigged.Host.statistics().ReceiptCacheHits == 2U,
			   "the second answer for a file reads no store");
		const auto unread = rigged.Host.durable_receipt_revision();

		// A selection observes unchanged material: nothing durable moves.
		static_cast<void>(rigged.observe(core::file_material_observation{ .File = seed.File, .Material = seed.Material }));
		expect(rigged.Host.durable_receipt_revision() == unread,
			   "an in-memory receipt for exact material leaves the durable revision");

		const auto outcome = rigged.submit(command_for(seed, core::file_receipt_command_kind::mark_read));
		expect(outcome.Changed && rigged.Host.durable_receipt_revision() > unread,
			   "a commit through the host moves the durable revision");
		const auto loadsAfterCommit = rigged.Host.statistics().ReceiptLoads;
		const auto committed        = rigged.load_receipt(seed.File);
		expect(committed.has_value() && committed->has_value() && (*committed)->Read == core::file_read_state::read
				   && rigged.Host.statistics().ReceiptLoads == loadsAfterCommit,
			   "the committed receipt is answered without rereading the store");

		// Another host writes the store; until its notification arrives the
		// retained value stands, and afterwards exactly that file is reread.
		auto sibling          = *store.Committed;
		sibling.Accepted      = true;
		store.Committed       = sibling;
		const auto beforeNote = rigged.Host.durable_receipt_revision();
		expect(rigged.load_receipt(seed.File)->value().Accepted == false, "an unannounced foreign write is not guessed at");
		const auto files = std::array{ seed.File };
		rigged.Host.invalidate_receipts(files);
		expect(rigged.Host.durable_receipt_revision() > beforeNote && rigged.Host.statistics().ReceiptInvalidations == 1U,
			   "a named foreign change drops that file and moves the revision");
		expect(rigged.load_receipt(seed.File)->value().Accepted && rigged.Host.statistics().ReceiptLoads == loadsAfterCommit + 1U,
			   "the invalidated file alone is read again");

		store.fail_next(application::ledger_store_failure::unavailable);
		expect(!rigged.load_receipt(other).has_value(), "a failed read is typed");
		expect(rigged.load_receipt(other).has_value() && rigged.Host.statistics().ReceiptLoads == loadsAfterCommit + 3U,
			   "a failed read retains nothing and is asked again");
		const auto retainedFiles = rigged.Host.statistics().ReceiptInvalidations;
		rigged.Host.invalidate_all_receipts();
		expect(rigged.Host.statistics().ReceiptInvalidations == retainedFiles + 2U,
			   "a change naming no file drops every retained receipt");

		// A read that began before a sibling's invalidation may predate the
		// change the invalidation announced: it answers, and retains nothing.
		auto pending = answer<void>{ };
		rigged.Host.load_receipts({ seed.File }, rig::into(pending));
		rigged.Effects.drain();
		rigged.Host.invalidate_receipts(files);
		rigged.settle();
		expect(pending.has_value() && pending->has_value() && !rigged.Host.cached_receipt(seed.File).has_value(),
			   "a read overtaken by an invalidation of its file retains nothing");
	}

	// ---- [[BPR-1211]] off the decision lane --------------------------------

	/// A decision and effect lane that other threads may post to.
	class locked_lane final : public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			{
				const auto lock = std::scoped_lock{ Mutex };
				Tasks.push_back(std::move(aTask));
			}
			Posted.notify_all();
		}

		/// Runs what is posted, until nothing is.
		auto drain() -> std::size_t
		{
			auto ran = std::size_t{ 0U };
			for (;;) {
				auto task = std::function<void()>{ };
				{
					const auto lock = std::scoped_lock{ Mutex };
					if (Tasks.empty()) {
						return ran;
					}
					task = std::move(Tasks.front());
					Tasks.pop_front();
				}
				task();
				++ran;
			}
		}

		[[nodiscard]] auto wait_for_post(const std::chrono::milliseconds aTimeout) -> bool
		{
			auto lock = std::unique_lock{ Mutex };
			return Posted.wait_for(lock, aTimeout, [this] { return !Tasks.empty(); });
		}

	private:
		std::mutex                        Mutex;
		std::condition_variable           Posted;
		std::deque<std::function<void()>> Tasks;
	};

	/// One effect thread, FIFO, joined after draining.
	class thread_worker final : public application::effect_executor
	{
	public:
		thread_worker()
			: Worker{ [this] { run(); } }
		{
		}
		thread_worker(const thread_worker&)                    = delete;
		auto operator=(const thread_worker&) -> thread_worker& = delete;
		thread_worker(thread_worker&&)                         = delete;
		auto operator=(thread_worker&&) -> thread_worker&      = delete;
		~thread_worker() override
		{
			{
				const auto lock = std::scoped_lock{ Mutex };
				Open            = false;
			}
			Posted.notify_all();
			Worker.join();
		}

		void post(std::function<void()> aTask) override
		{
			{
				const auto lock = std::scoped_lock{ Mutex };
				Tasks.push_back(std::move(aTask));
			}
			Posted.notify_all();
		}

	private:
		void run()
		{
			for (;;) {
				auto task = std::function<void()>{ };
				{
					auto lock = std::unique_lock{ Mutex };
					Posted.wait(lock, [this] { return !Open || !Tasks.empty(); });
					if (Tasks.empty()) {
						return;
					}
					task = std::move(Tasks.front());
					Tasks.pop_front();
				}
				task();
			}
		}

		std::mutex                        Mutex;
		std::condition_variable           Posted;
		std::deque<std::function<void()>> Tasks;
		bool                              Open = true;
		std::thread                       Worker;
	};

	/// The negative control: storage run inline, on the caller's lane.
	class inline_effects final : public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override { aTask(); }
	};

	/// How long `aGesture` held the decision lane while a store operation it
	/// reached was held for `aGate.Limit`.
	[[nodiscard]] auto lane_time_while_held(application::effect_executor& aEffects, locked_lane& aLane,
											application::review_ledger_store_port& aStore, event_log& aLog)
		-> std::chrono::milliseconds
	{
		auto gate       = std::make_shared<store_gate>();
		gate->Limit     = std::chrono::milliseconds{ 1'500 };
		auto projection = recording_projection{ aLog };
		auto opened     = answer<void>{ };
		auto host       = application::review_ledger_application_host{ opener(aStore, nullptr, gate), projection, aEffects, aLane, rig::into(opened) };
		for (auto tries = 0; tries < 200 && !opened.has_value(); ++tries) {
			static_cast<void>(aLane.wait_for_post(std::chrono::milliseconds{ 10 }));
			static_cast<void>(aLane.drain());
		}
		expect(opened.has_value() && opened->has_value(), "the gated store opens");
		const auto seed = seeded_receipt();
		host.retain(seed);
		gate->hold();
		auto       done  = answer<core::file_receipt_outcome>{ };
		const auto began = std::chrono::steady_clock::now();
		host.submit(command_for(seed, core::file_receipt_command_kind::mark_read), rig::into(done));
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - began);
		gate->release();
		for (auto tries = 0; tries < 400 && !done.has_value(); ++tries) {
			static_cast<void>(aLane.wait_for_post(std::chrono::milliseconds{ 10 }));
			static_cast<void>(aLane.drain());
		}
		expect(done.has_value() && done->has_value() && done->value().Changed, "the held commit completes once released");
		return elapsed;
	}

	/// AC3: a store operation is held on the effect lane while the decision
	/// lane keeps deciding, answering reads and running other work; nothing is
	/// published until the store answers. The same gesture with storage run
	/// inline holds the lane for the whole store wait.
	void test_a_held_store_operation_leaves_the_decision_lane_free()
	{
		auto log     = event_log{ };
		auto store   = recording_store{ log };
		auto lane    = locked_lane{ };
		auto gate    = std::make_shared<store_gate>();
		auto opened  = answer<void>{ };
		auto project = recording_projection{ log };
		{
			auto worker = thread_worker{ };
			auto host   = application::review_ledger_application_host{ opener(store, nullptr, gate), project, worker, lane, rig::into(opened) };
			for (auto tries = 0; tries < 200 && !opened.has_value(); ++tries) {
				static_cast<void>(lane.wait_for_post(std::chrono::milliseconds{ 10 }));
				static_cast<void>(lane.drain());
			}
			expect(opened.has_value() && opened->has_value() && host.available() == std::optional{ true }, "the store opens off the decision lane");
			const auto seed = seeded_receipt();
			host.retain(seed);
			gate->hold();
			auto first = answer<core::file_receipt_outcome>{ };
			host.submit(command_for(seed, core::file_receipt_command_kind::mark_read), rig::into(first));
			expect(gate->wait_entered(std::chrono::milliseconds{ 2'000 }), "the commit reaches the store on the effect lane");

			// The store is held. The lane still answers every read and runs
			// other work, and the next gesture is admitted behind the commit.
			const auto began = std::chrono::steady_clock::now();
			auto       ran   = false;
			lane.post([&ran] { ran = true; });
			static_cast<void>(lane.drain());
			auto second = answer<core::file_receipt_outcome>{ };
			host.submit(command_for(seed, core::file_receipt_command_kind::accept), rig::into(second));
			const auto reads = host.retained() == seed && !host.cached_receipt(seed.File).has_value()
				&& !host.private_observation().has_value() && !host.unread_conversations({ }).has_value()
				&& host.durable_receipt_revision() == 0U && host.statistics().PersistenceRequests == 1U && !host.rejected_note().has_value();
			const auto elapsed = std::chrono::steady_clock::now() - began;
			expect(ran && reads && elapsed < std::chrono::milliseconds{ 500 },
				   "while the store is held the decision lane runs, reads answer at once and a gesture is admitted");
			expect(!first.has_value() && !second.has_value() && project.Published.empty() && host.outstanding() == 2U,
				   "nothing is published or answered before the store completes");

			gate->release();
			for (auto tries = 0; tries < 400 && !second.has_value(); ++tries) {
				static_cast<void>(lane.wait_for_post(std::chrono::milliseconds{ 10 }));
				static_cast<void>(lane.drain());
			}
			expect(first.has_value() && second.has_value() && first->value().Changed && second->value().Changed
					   && second->value().Receipt.Read == core::file_read_state::read && second->value().Receipt.Accepted
					   && project.Published.size() == 2U && host.retained().Accepted && host.is_quiescent(),
				   "the queued gesture is decided after the first commit, against what it left durable");
		}

		// Negative control: the same gesture with an inline effect lane waits
		// out the held store on the decision lane.
		auto       inlineLog   = event_log{ };
		auto       inlineStore = recording_store{ inlineLog };
		auto       inlined     = inline_effects{ };
		auto       inlineLane  = locked_lane{ };
		auto       worker      = thread_worker{ };
		auto       threadLog   = event_log{ };
		auto       threadStore = recording_store{ threadLog };
		const auto blocked     = lane_time_while_held(inlined, inlineLane, inlineStore, inlineLog);
		const auto free        = lane_time_while_held(worker, inlineLane, threadStore, threadLog);
		std::cout << "held-store lane time: inline=" << blocked.count() << "ms worker=" << free.count() << "ms\n";
		expect(blocked >= std::chrono::milliseconds{ 1'400 } && free < std::chrono::milliseconds{ 200 },
			   "the control proves the check: storage inline on the lane holds it for the store's wait");
	}

	/// AC3: outstanding work is bounded and the refusal is typed.
	void test_admission_is_bounded()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		auto       rigged = rig{ store, log, 2U };
		const auto seed   = seeded_receipt();
		rigged.Host.retain(seed);
		auto answers = std::array<answer<core::file_receipt_outcome>, 3U>{ };
		for (auto& got : answers) {
			rigged.Host.submit(command_for(seed, core::file_receipt_command_kind::mark_read), rig::into(got));
		}
		expect(rigged.Host.outstanding() == 2U, "two operations are held and the third is not");
		rigged.Lane.drain();
		expect(answers[2].has_value() && !answers[2]->has_value() && answers[2]->error() == application::ledger_store_failure::unavailable
				   && rigged.Projection.Failures.size() == 1U && rigged.Projection.Failures.front().Store == application::ledger_store_failure::unavailable
				   && !answers[0].has_value() && !answers[1].has_value(),
			   "the gesture beyond the bound is refused, typed and presented, before the held ones answer");
		rigged.settle();
		expect(answers[0]->value().Changed && !answers[1]->value().Changed && store.Requests.size() == 1U
				   && rigged.Host.statistics().AdmissionRefusals == 1U && rigged.Host.statistics().PeakOutstanding == 2U,
			   "the admitted gestures complete in order; the refused one never reached the store");
	}

	/// AC2/AC3: rapid gestures are first in, first out, each decided against
	/// the durable result of the one before; a failed write rolls back only
	/// itself.
	void test_rapid_gestures_are_fifo_and_a_failure_rolls_back_only_itself()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		auto       rigged = rig{ store, log };
		const auto seed   = seeded_receipt();
		rigged.Host.retain(seed);
		auto read     = answer<core::file_receipt_outcome>{ };
		auto accept   = answer<core::file_receipt_outcome>{ };
		auto exclude  = answer<core::file_receipt_outcome>{ };
		auto reaccept = answer<core::file_receipt_outcome>{ };
		rigged.Host.submit(command_for(seed, core::file_receipt_command_kind::mark_read), rig::into(read));
		rigged.Host.submit(command_for(seed, core::file_receipt_command_kind::accept), rig::into(accept));
		rigged.Host.submit(command_for(seed, core::file_receipt_command_kind::exclude), rig::into(exclude));
		rigged.Host.submit(command_for(seed, core::file_receipt_command_kind::accept), rig::into(reaccept));
		expect(rigged.Effects.pending() == 1U && store.Requests.empty(), "only the first gesture's write is posted");

		rigged.Effects.run_next();
		rigged.Lane.drain();
		expect(read.has_value() && !accept.has_value() && store.Requests.size() == 1U
				   && log == event_log{ "request", "committed", "published" },
			   "the first write is durable and published before the second is decided");
		store.fail_next(application::ledger_store_failure::conflict);
		rigged.Effects.run_next();
		rigged.Lane.drain();
		expect(accept.has_value() && !accept->value().Changed && rigged.Host.retained().Read == core::file_read_state::read
				   && !rigged.Host.retained().Accepted,
			   "a refused second write rolls back only itself");
		rigged.settle();
		expect(exclude->value().Changed && exclude->value().Receipt.Excluded && !exclude->value().Receipt.Accepted
				   && reaccept.has_value() && reaccept->value().Rejection == core::file_receipt_rejection::excluded_file_cannot_be_accepted,
			   "later gestures are decided against the durable state the failure left, in order");
		expect(store.Committed.has_value() && store.Committed == rigged.Host.retained() && rigged.Projection.Published.size() == 2U,
			   "the host and the store agree after every completion");
	}

	/// AC2/AC3: retirement cancels queued work with a typed terminal; a
	/// completion arriving after it is refused and publishes nothing; the
	/// store closes on the effect lane; a new host on the same root recovers
	/// what the store did commit.
	void test_retirement_cancels_refuses_late_completion_and_recovers_the_same_root()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		const auto seed   = seeded_receipt();
		auto       closed = false;
		{
			auto rigged = rig{ store, log };
			rigged.Host.retain(seed);
			auto running = answer<core::file_receipt_outcome>{ };
			auto queued  = answer<core::file_receipt_outcome>{ };
			rigged.Host.submit(command_for(seed, core::file_receipt_command_kind::mark_read), rig::into(running));
			rigged.Host.submit(command_for(seed, core::file_receipt_command_kind::accept), rig::into(queued));
			rigged.Host.retire();
			rigged.Lane.drain();
			expect(queued.has_value() && queued->error() == application::ledger_store_failure::cancelled && !running.has_value(),
				   "retirement answers queued work cancelled, and leaves the running write to finish");
			rigged.Effects.run_next();
			expect(store.Committed.has_value() && store.Committed->Read == core::file_read_state::read, "the running write reached the store");
			rigged.Lane.drain();
			expect(running.has_value() && running->error() == application::ledger_store_failure::cancelled
					   && rigged.Projection.Published.empty() && rigged.Projection.Failures.empty() && rigged.Host.retained() == seed,
				   "the late completion is refused: nothing retained, published or failed into a successor");
			expect(!rigged.Closed && rigged.Effects.pending() == 1U, "the store is closed on the effect lane, after the write");
			rigged.Effects.drain();
			expect(rigged.Closed && rigged.Host.is_quiescent(), "the retired host closes its store and is quiescent");
			auto after = answer<void>{ };
			rigged.Host.load_private(rig::into(after));
			rigged.settle();
			const auto statistics = rigged.Host.statistics();
			expect(after.has_value() && after->error() == application::ledger_store_failure::cancelled
					   && statistics.LateCompletions == 1U && statistics.CancelledOperations == 3U,
				   "work asked of a retired host is cancelled, never run");
			closed = rigged.Closed;
		}
		expect(closed, "the retired host's store was closed");
		auto       successor = rig{ store, log };
		const auto recovered = successor.observe(core::file_material_observation{ .File = seed.File, .Material = seed.Material });
		expect(recovered.has_value() && recovered->Read == core::file_read_state::read && successor.Host.statistics().MaterialReceiptHits == 1U,
			   "a successor on the same root recovers exactly what the store committed");
	}

	/// AC3: destroying a host with work outstanding drains to exactly one
	/// terminal each, and the store closes once.
	void test_shutdown_drains_every_terminal_once()
	{
		auto log       = event_log{ };
		auto store     = recording_store{ log };
		auto lane      = application::manual_executor{ };
		auto effects   = application::manual_executor{ };
		auto closed    = false;
		auto project   = recording_projection{ log };
		auto terminals = std::vector<std::string>{ };
		{
			auto host = application::review_ledger_application_host{ opener(store, &closed), project, effects, lane };
			host.load_private([&terminals](const std::expected<void, application::ledger_store_failure>& aResult) {
				terminals.push_back(aResult ? "private-ok" : "private-" + std::string{ application::name(aResult.error()) });
			});
			host.session_catalog([&terminals](const auto& aResult) {
				terminals.push_back(aResult ? "catalog-ok" : "catalog-" + std::string{ application::name(aResult.error()) });
			});
			effects.run_next(); // the store opens
			lane.drain();
		}
		// The host is gone; its outstanding work still answers on the lanes.
		effects.drain();
		lane.drain();
		effects.drain();
		lane.drain();
		std::ranges::sort(terminals);
		expect(terminals == std::vector<std::string>{ "catalog-cancelled", "private-cancelled" } && closed,
			   "each outstanding operation answers once, cancelled, and the store closes");
	}

	/// An unopenable store answers every operation with its failure.
	void test_an_unopenable_store_fails_typed()
	{
		auto log     = event_log{ };
		auto lane    = application::manual_executor{ };
		auto effects = application::manual_executor{ };
		auto project = recording_projection{ log };
		auto opened  = answer<void>{ };
		auto host    = application::review_ledger_application_host{
			[] { return std::expected<std::unique_ptr<application::review_ledger_store_port>, application::ledger_store_failure>{ std::unexpected(application::ledger_store_failure::integrity_failure) }; },
			project, effects, lane, rig::into(opened)
		};
		expect(!host.available().has_value(), "availability is unknown until the open answers");
		auto loaded = answer<void>{ };
		host.load_private(rig::into(loaded));
		while (lane.pending() + effects.pending() > 0U) {
			effects.drain();
			lane.drain();
		}
		expect(opened.has_value() && opened->error() == application::ledger_store_failure::integrity_failure
				   && host.available() == std::optional{ false }
				   && loaded.has_value() && loaded->error() == application::ledger_store_failure::integrity_failure
				   && host.private_observation().error() == application::ledger_store_failure::integrity_failure,
			   "the open failure is typed and every later operation answers it");
	}

	/// A newer material observation or `retain` supersedes an older one's
	/// publication; the durable read is still kept.
	void test_a_superseded_observation_neither_retains_nor_publishes()
	{
		auto       log    = event_log{ };
		auto       store  = recording_store{ log };
		auto       rigged = rig{ store, log };
		const auto first  = seeded_receipt();
		auto       second = first;
		second.File       = opaque<core::file_identity>(3U);
		auto older        = answer<core::file_receipt>{ };
		auto newer        = answer<core::file_receipt>{ };
		rigged.Host.observe_material({ .File = first.File, .Material = first.Material }, rig::into(older));
		rigged.Host.observe_material({ .File = second.File, .Material = second.Material }, rig::into(newer));
		rigged.settle();
		expect(older.has_value() && older->has_value() && newer.has_value() && newer->has_value()
				   && rigged.Projection.Published.size() == 1U && rigged.Projection.Published.front().Receipt.File == second.File
				   && rigged.Host.retained().File == second.File && rigged.Host.cached_receipt(first.File).has_value(),
			   "only the latest observation retains and publishes; the older read is kept");
		auto stale = answer<core::file_receipt>{ };
		rigged.Host.observe_material({ .File = first.File, .Material = first.Material }, rig::into(stale));
		rigged.Host.retain({ });
		rigged.settle();
		expect(stale.has_value() && rigged.Projection.Published.size() == 1U && rigged.Host.retained() == core::file_receipt{ },
			   "a cleared selection supersedes an observation still running");
	}
} // namespace

auto main() -> int
{
	test_durable_receipts_are_retained_until_another_writer_invalidates_them();
	test_accepted_write_observer();
	test_sibling_refresh_is_observation_only();
	test_hostile_note_competitors_are_bounded_before_projection();
	test_note_conflicts_retain_exact_window_candidate();
	test_the_projection_follows_the_commit();
	test_a_refused_command_never_reaches_the_store();
	test_a_failed_commit_changes_nothing();
	test_an_unchanged_command_is_not_a_write();
	test_statistics_account_for_every_command();
	test_private_records_are_validated_before_the_store();
	test_private_records_round_trip_and_delete_through_the_host();
	test_generated_marks_round_trip_through_the_host();
	test_material_observation_loads_carries_and_publishes_in_order();
	test_material_load_failure_is_typed_and_changes_nothing();
	test_agent_sessions_validate_admit_and_load_through_the_host();
	test_receipt_action_projection_contains_only_core_legal_changes();
	test_conversation_read_state_loads_once_and_marks_read_through_the_store();
	test_conversation_marks_unread_through_the_store();
	test_singleton_conversations_seen_before_and_restart();
	test_a_held_store_operation_leaves_the_decision_lane_free();
	test_admission_is_bounded();
	test_rapid_gestures_are_fifo_and_a_failure_rolls_back_only_itself();
	test_retirement_cancels_refuses_late_completion_and_recovers_the_same_root();
	test_shutdown_drains_every_terminal_once();
	test_an_unopenable_store_fails_typed();
	test_a_superseded_observation_neither_retains_nor_publishes();

	std::cout << "review-ledger-host-tests:pass\n";
	return EXIT_SUCCESS;
}
