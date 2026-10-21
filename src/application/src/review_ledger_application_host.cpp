#include "bpr/application/review_ledger_application_host.hpp"

#include <algorithm>
#include <array>
#include <deque>
#include <limits>
#include <map>
#include <type_traits>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] constexpr auto durable_integer(const std::uint64_t aValue) noexcept -> bool
		{
			return aValue > 0U
				&& aValue
				<= static_cast<std::uint64_t>(
					   std::numeric_limits<std::int64_t>::max());
		}

		/// \brief Finds a conversation's receipt in receipts sorted by conversation.
		[[nodiscard]] auto find_conversation_receipt(
			const std::vector<core::conversation_receipt>& aReceipts,
			const core::review_conversation_id             aConversation) noexcept
			-> const core::conversation_receipt*
		{
			const auto found = std::ranges::lower_bound(
				aReceipts, aConversation, { }, &core::conversation_receipt::Conversation);
			return found != aReceipts.end() && found->Conversation == aConversation ? &*found : nullptr;
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

	auto project_file_receipt_actions(
		const core::file_receipt& aReceipt) noexcept
		-> file_receipt_action_projection
	{
		const auto command = [&aReceipt](
								 const file_receipt_action_id          aId,
								 const core::file_receipt_command_kind aKind) {
			const auto value = core::file_receipt_command{
				.File     = aReceipt.File,
				.Material = aReceipt.Material,
				.Kind     = aKind,
			};
			const auto outcome = core::apply(aReceipt, value);
			return file_receipt_action_command_projection{
				.Id      = aId,
				.Command = value,
				.Enabled = outcome.Rejection
						== core::file_receipt_rejection::none
					&& outcome.Changed,
			};
		};

		// [[BPR-PENDING-P]]. Seen and Read carry their inverse once reached, as
		// Accept and Exclude do: Seen's is Mark unseen, which clears Read
		// with it; Read's is Mark unread, which leaves the file seen.
		return file_receipt_action_projection{
			.Receipt = aReceipt,
			.Actions = {
				command(file_receipt_action_id::seen,
						aReceipt.Read == core::file_read_state::unread
							? core::file_receipt_command_kind::mark_seen
							: core::file_receipt_command_kind::mark_unread),
				command(file_receipt_action_id::read,
						aReceipt.Read == core::file_read_state::read
							? core::file_receipt_command_kind::unmark_read
							: core::file_receipt_command_kind::mark_read),
				command(file_receipt_action_id::accepted,
						aReceipt.Accepted
							? core::file_receipt_command_kind::revoke_acceptance
							: core::file_receipt_command_kind::accept),
				command(file_receipt_action_id::excluded,
						aReceipt.Excluded
							? core::file_receipt_command_kind::include
							: core::file_receipt_command_kind::exclude),
			},
		};
	}

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

	auto valid(const core::conversation_receipt& aReceipt) noexcept -> bool
	{
		return aReceipt.Conversation.value() != 0U;
	}

	auto valid(const local_note_record& aNote) noexcept -> bool
	{
		if (!durable_integer(aNote.Id.value())
			|| (aNote.Revision.value() != 0U && !durable_integer(aNote.Revision.value())) || aNote.Note.Text.empty()
			|| aNote.Note.Text.size() > core::maximum_note_bytes) {
			return false;
		}
		if (aNote.FilePath
			&& (!aNote.Attachment || aNote.FilePath->empty() || aNote.FilePath->size() > maximum_note_path_bytes
				|| aNote.FilePath->find('\0') != std::string::npos)) {
			return false;
		}
		return !aNote.Attachment
			|| (aNote.Attachment->File.valid()
				&& aNote.Attachment->Comparison.valid()
				// [[BPR-1591]]. A kept blob is a real one.
				&& (!aNote.Attachment->Line || !aNote.Attachment->Line->Blob || aNote.Attachment->Line->Blob->valid()));
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

	auto valid(const agent_session_admission& aAdmission) noexcept -> bool
	{
		const auto& binding = aAdmission.Binding;
		return durable_integer(aAdmission.Profile.value())
			&& durable_integer(binding.Host.value())
			&& durable_integer(binding.Repository.value())
			&& durable_integer(binding.PullRequest.value())
			&& binding.BaseOid.valid() && binding.HeadOid.valid()
			&& core::valid(binding.LeftEpoch) && core::valid(binding.RightEpoch);
	}

	auto valid(const core::agent_session& aSession) noexcept -> bool
	{
		return durable_integer(aSession.Id.value())
			&& valid(agent_session_admission{
				.Profile = aSession.Profile,
				.Binding = aSession.Binding,
			});
	}

	// ---- the ledger actor --------------------------------------------------
	//
	// [[BPR-1211]]. The queue, the retained state and the statistics belong to
	// the decision lane. The store belongs to the effect lane, where exactly one
	// operation touches it at a time. An effect captures the values its storage
	// work needs, and returns a continuation holding the typed answer; that
	// continuation runs on the decision lane, where the core decides, state is
	// retained, and then the terminal is delivered.

	struct review_ledger_application_host::implementation final
		: std::enable_shared_from_this<implementation>
	{
		/// Touched only on the effect lane, one operation at a time.
		struct store_cell
		{
			std::unique_ptr<review_ledger_store_port> Store;
			ledger_store_failure                      Failure = ledger_store_failure::unavailable;
		};

		struct outcome;
		using continuation = std::function<void(outcome&)>;
		using effect       = std::function<continuation(store_cell&)>;

		/// What one step of an operation leaves: more storage work, or its terminal.
		struct outcome
		{
			effect                Next;
			std::function<void()> Terminal;
		};

		struct operation
		{
			continuation                              Begin;
			std::function<void(ledger_store_failure)> Fail;
		};

		implementation(receipt_projection_port& aProjection,
					   effect_executor&         aEffects,
					   decision_executor&       aDecisions,
					   const std::size_t        aCapacity)
			: Projection{ aProjection }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
			, Capacity{ std::max<std::size_t>(aCapacity, 1U) }
			, Cell{ std::make_shared<store_cell>() }
		{
		}

		receipt_projection_port&    Projection;
		effect_executor&            Effects;
		decision_executor&          Decisions;
		std::size_t                 Capacity;
		std::shared_ptr<store_cell> Cell;
		std::deque<operation>       Queue;
		bool                        Running          = false;
		bool                        Pumping          = false;
		bool                        Retired          = false;
		bool                        Released         = false;
		std::size_t                 PendingTerminals = 0U;
		std::optional<bool>         Available;

		insights_review_capture                                                         Insights;
		std::function<void(const core::file_receipt&, core::file_receipt_command_kind)> FileStatistics;
		core::file_receipt                                                              Retained;
		/// Advances on every material observation and `retain`; an older
		/// observation neither retains nor publishes.
		std::uint64_t MaterialGeneration = 0U;
		/// [[BPR-0972]]. The durable receipts read or written through this host.
		std::map<core::file_identity, std::optional<core::file_receipt>> DurableReceipts;
		std::uint64_t                                                    DurableRevision = 0U;
		/// [[BPR-0972]]. The files each recent revision moved, oldest first,
		/// and the revision before which the record no longer answers.
		std::deque<std::pair<std::uint64_t, core::file_identity>> DurableChanges;
		std::uint64_t                                             DurableChangesFloor = 0U;
		/// Advances on every receipt invalidation. A read that began before
		/// one may predate the change it announced, so it retains nothing.
		std::uint64_t ReceiptInvalidationSerial = 0U;
		/// Sorted by conversation, so each thread's receipt is a binary search.
		std::optional<std::vector<core::conversation_receipt>> ConversationReceipts;
		std::uint64_t                                          ConversationSerial = 0U;
		std::vector<ledger_completion<void>>                   ConversationWaiters;
		bool                                                   ConversationLoadQueued = false;
		std::optional<private_ledger_observation>              Private;
		std::optional<ledger_store_failure>                    PrivateFailure;
		std::vector<ledger_completion<void>>                   PrivateWaiters;
		bool                                                   PrivateLoadQueued = false;
		std::optional<rejected_local_note_mutation>            RejectedNote;
		review_ledger_host_statistics                          Statistics;

		// ---- queue mechanics ----------------------------------------------

		static void deliver(const std::function<void()>& aTerminal) noexcept
		{
			if (!aTerminal) {
				return;
			}
			try {
				aTerminal();
			} catch (...) {
				// A terminal observer cannot strand the operations behind it.
			}
		}

		template <typename Completion, typename Value>
		static void answer(const Completion& aDone, Value&& aValue) noexcept
		{
			if (!aDone) {
				return;
			}
			try {
				aDone(std::forward<Value>(aValue));
			} catch (...) {
			}
		}

		void post_terminal(std::function<void()> aTerminal) noexcept
		{
			++PendingTerminals;
			try {
				Decisions.post([self = shared_from_this(), terminal = std::move(aTerminal)] {
					--self->PendingTerminals;
					deliver(terminal);
				});
			} catch (...) {
				// The decision lane is a required live service; without it
				// there is nowhere left to answer.
				--PendingTerminals;
			}
		}

		void enqueue(operation aOperation)
		{
			if (Retired) {
				++Statistics.CancelledOperations;
				post_terminal([fail = std::move(aOperation.Fail)] {
					if (fail) {
						fail(ledger_store_failure::cancelled);
					}
				});
				return;
			}
			if (Queue.size() >= Capacity) {
				++Statistics.AdmissionRefusals;
				post_terminal([fail = std::move(aOperation.Fail)] {
					if (fail) {
						fail(ledger_store_failure::unavailable);
					}
				});
				return;
			}
			Queue.push_back(std::move(aOperation));
			Statistics.PeakOutstanding = std::max<std::uint64_t>(Statistics.PeakOutstanding, Queue.size());
			pump();
		}

		/// Runs one lane step of the head operation; a throwing step fails it.
		void step(continuation& aStep, outcome& aOutcome)
		{
			try {
				aStep(aOutcome);
			} catch (...) {
				aOutcome = outcome{ .Terminal = [fail = Queue.front().Fail] {
					if (fail) {
						fail(ledger_store_failure::unavailable);
					}
				} };
			}
		}

		void pump()
		{
			if (Pumping) {
				return;
			}
			Pumping = true;
			while (!Running && !Retired && !Queue.empty()) {
				auto result = outcome{ };
				step(Queue.front().Begin, result);
				if (result.Next) {
					start(std::move(result.Next));
					continue;
				}
				auto terminal = std::move(result.Terminal);
				Queue.pop_front();
				deliver(terminal);
			}
			Pumping = false;
		}

		void start(effect aEffect)
		{
			Running = true;
			++Statistics.Operations;
			try {
				Effects.post([self = shared_from_this(), cell = Cell, work = std::move(aEffect)]() mutable {
					auto next = continuation{ };
					try {
						next = work(*cell);
					} catch (...) {
						next = { };
					}
					try {
						self->Decisions.post([self, next]() mutable {
							self->resume(std::move(next));
						});
					} catch (...) {
					}
				});
			} catch (...) {
				// Neither the effect nor its completion was posted. This is the
				// lane already, so the operation's terminal is delivered here.
				Running = false;
				fail_head(ledger_store_failure::unavailable);
				pump();
			}
		}

		void fail_head(const ledger_store_failure aFailure)
		{
			auto fail = std::move(Queue.front().Fail);
			Queue.pop_front();
			if (aFailure == ledger_store_failure::cancelled) {
				++Statistics.CancelledOperations;
			}
			if (fail) {
				try {
					fail(aFailure);
				} catch (...) {
				}
			}
		}

		void resume(continuation aNext)
		{
			Running = false;
			if (Retired) {
				// The storage may have answered, even committed, but this host
				// was retired while it ran: nothing is retained or published.
				++Statistics.LateCompletions;
				fail_head(ledger_store_failure::cancelled);
				release();
				return;
			}
			if (!aNext) {
				fail_head(ledger_store_failure::unavailable);
				pump();
				return;
			}
			auto result = outcome{ };
			step(aNext, result);
			if (result.Next) {
				start(std::move(result.Next));
				return;
			}
			auto terminal = std::move(result.Terminal);
			Queue.pop_front();
			deliver(terminal);
			pump();
		}

		void retire() noexcept
		{
			if (Retired) {
				return;
			}
			Retired         = true;
			const auto keep = Running ? std::size_t{ 1U } : std::size_t{ 0U };
			while (Queue.size() > keep) {
				auto fail = std::move(Queue.back().Fail);
				Queue.pop_back();
				++Statistics.CancelledOperations;
				post_terminal([fail] {
					if (fail) {
						fail(ledger_store_failure::cancelled);
					}
				});
			}
			if (!Running) {
				release();
			}
		}

		/// Closes the store on the effect lane, after anything running on it.
		void release() noexcept
		{
			if (Released) {
				return;
			}
			Released = true;
			try {
				Effects.post([cell = Cell] { cell->Store.reset(); });
			} catch (...) {
				// A closed effect lane has already drained; nothing else can
				// be touching the store.
				Cell->Store.reset();
			}
		}

		// ---- storage work -------------------------------------------------

		template <typename Error>
		[[nodiscard]] static auto failure_as(const ledger_store_failure aFailure) -> Error
		{
			if constexpr (std::is_same_v<Error, local_note_failure>) {
				return local_note_failure{ .Reason = aFailure };
			} else {
				return aFailure;
			}
		}

		/// Runs `aWork` against the open store; a missing store or a thrown
		/// store error is a typed failure.
		template <typename Result, typename Work>
		[[nodiscard]] static auto guarded(store_cell& aCell, Work& aWork) -> Result
		{
			using error = typename Result::error_type;
			if (aCell.Store == nullptr) {
				return std::unexpected(failure_as<error>(aCell.Failure));
			}
			try {
				return aWork(*aCell.Store);
			} catch (...) {
				return std::unexpected(failure_as<error>(ledger_store_failure::unavailable));
			}
		}

		/// One storage call on the effect lane, then `aApply` on the decision lane.
		template <typename Result, typename Work, typename Apply>
		[[nodiscard]] static auto storage(Work aWork, Apply aApply) -> effect
		{
			return [work = std::move(aWork), apply = std::move(aApply)](store_cell& aCell) mutable -> continuation {
				auto result = guarded<Result>(aCell, work);
				return [apply, result](outcome& aOutcome) mutable {
					apply(std::move(result), aOutcome);
				};
			};
		}

		// ---- durable receipts --------------------------------------------

		void record_durable_change(const core::file_identity& aFile)
		{
			// A bounded record: the oldest change leaving it raises the floor, so a
			// reader that last looked before it is told to reread everything.
			constexpr auto recorded = std::size_t{ 256U };
			if (DurableChanges.size() == recorded) {
				DurableChangesFloor = DurableChanges.front().first;
				DurableChanges.pop_front();
			}
			DurableChanges.emplace_back(DurableRevision, aFile);
		}

		void remember_durable(const core::file_identity&               aFile,
							  const std::optional<core::file_receipt>& aReceipt,
							  const bool                               aCommitted)
		{
			const auto [retained, inserted] = DurableReceipts.try_emplace(aFile, aReceipt);
			if (!inserted && retained->second != aReceipt) {
				retained->second = aReceipt;
				++DurableRevision;
				record_durable_change(aFile);
			} else if (aCommitted) {
				++DurableRevision;
				record_durable_change(aFile);
			}
		}

		/// Retains a read, unless an invalidation arrived after it began.
		void remember_read(const core::file_identity&               aFile,
						   const std::optional<core::file_receipt>& aReceipt,
						   const std::uint64_t                      aSerial)
		{
			if (aSerial == ReceiptInvalidationSerial) {
				remember_durable(aFile, aReceipt, false);
			}
		}

		void publish(const core::file_receipt& aReceipt)
		{
			++Statistics.Publications;
			Projection.publish(receipt_projection{ .Receipt = aReceipt });
		}

		void publish_failure(receipt_failure_publication aFailure)
		{
			++Statistics.FailurePublications;
			Projection.fail(aFailure);
		}

		// ---- conversation receipts ---------------------------------------

		/// Loads the conversation receipts as the first step of `aThen`'s operation.
		void with_conversation_receipts(outcome& aOutcome, std::function<void(outcome&)> aThen,
										std::function<void(ledger_store_failure)> aFail)
		{
			if (ConversationReceipts.has_value()) {
				aThen(aOutcome);
				return;
			}
			++Statistics.ConversationLoads;
			aOutcome.Next = storage<std::expected<std::vector<core::conversation_receipt>, ledger_store_failure>>(
				[](review_ledger_store_port& aStore) { return aStore.load_conversation_receipts(); },
				[this, serial = ConversationSerial, then = std::move(aThen), fail = std::move(aFail)](
					std::expected<std::vector<core::conversation_receipt>, ledger_store_failure> aLoaded,
					outcome&                                                                     aNext) mutable {
					if (!aLoaded) {
						++Statistics.StoreFailures;
						++Statistics.ConversationFailures;
						aNext.Terminal = [fail, failure = aLoaded.error()] { fail(failure); };
						return;
					}
					std::ranges::sort(*aLoaded, { }, &core::conversation_receipt::Conversation);
					if (serial != ConversationSerial) {
						// A sibling changed them while they were read; decide
						// against the answer, but keep nothing that may predate it.
						auto transient       = std::move(*aLoaded);
						ConversationReceipts = std::move(transient);
						then(aNext);
						ConversationReceipts.reset();
						return;
					}
					ConversationReceipts = std::move(*aLoaded);
					then(aNext);
				});
		}

		// ---- private records -----------------------------------------------

		void adopt_note(const local_note_record& aNote)
		{
			if (!Private) {
				return;
			}
			const auto found = std::ranges::find(Private->Notes, aNote.Id, &local_note_record::Id);
			if (found == Private->Notes.end()) {
				Private->Notes.push_back(aNote);
			} else {
				*found = aNote;
			}
		}

		void adopt_defect(const durable_defect_record& aDefect)
		{
			if (!Private) {
				return;
			}
			const auto found = std::ranges::find(Private->Defects, aDefect.History.Defect.Id,
												 [](const durable_defect_record& aRecord) { return aRecord.History.Defect.Id; });
			if (found == Private->Defects.end()) {
				Private->Defects.push_back(aDefect);
			} else {
				*found = aDefect;
			}
		}

		static void flush(std::vector<ledger_completion<void>>& aWaiters, const std::expected<void, ledger_store_failure>& aResult)
		{
			auto waiters = std::exchange(aWaiters, { });
			for (const auto& waiter : waiters) {
				answer(waiter, aResult);
			}
		}
	};

	// ---- host -------------------------------------------------------------

	review_ledger_application_host::review_ledger_application_host(
		review_ledger_store_opener aOpen,
		receipt_projection_port&   aProjection,
		effect_executor&           aEffects,
		decision_executor&         aDecisions,
		ledger_completion<void>    aOpened,
		const std::size_t aCapacity, insights_review_capture aInsights, std::function<void(const core::file_receipt&, core::file_receipt_command_kind)> aFileStatistics)
		: Implementation{ std::make_shared<implementation>(aProjection, aEffects, aDecisions, aCapacity) }
	{
		using impl           = implementation;
		auto* const self     = Implementation.get();
		self->Insights       = std::move(aInsights);
		self->FileStatistics = std::move(aFileStatistics);
		self->enqueue(impl::operation{
			.Begin = [self, open = std::move(aOpen), opened = aOpened](impl::outcome& aOutcome) mutable { aOutcome.Next = [self, open, opened](impl::store_cell& aCell) mutable -> impl::continuation {
																											  auto failure = std::optional<ledger_store_failure>{ };
																											  try {
																												  auto store = open ? open() : std::unexpected(ledger_store_failure::unavailable);
																												  if (store.has_value() && *store != nullptr) {
																													  aCell.Store = std::move(*store);
																												  } else {
																													  failure = store.has_value() ? ledger_store_failure::unavailable : store.error();
																												  }
																											  } catch (...) {
																												  failure = ledger_store_failure::unavailable;
																											  }
																											  if (failure.has_value()) {
																												  aCell.Failure = *failure;
																											  }
																											  return [self, failure, opened](impl::outcome& aNext) mutable {
																												  self->Available = !failure.has_value();
																												  aNext.Terminal  = [failure, opened] {
																													  impl::answer(opened, failure.has_value() ? std::expected<void, ledger_store_failure>{ std::unexpected(*failure) } : std::expected<void, ledger_store_failure>{ });
																												  };
																											  };
																										  }; },
			.Fail  = [opened = std::move(aOpened)](const ledger_store_failure aFailure) { impl::answer(opened, std::expected<void, ledger_store_failure>{ std::unexpected(aFailure) }); },
		});
	}

	review_ledger_application_host::~review_ledger_application_host()
	{
		retire();
	}

	void review_ledger_application_host::retire() noexcept
	{
		Implementation->retire();
	}

	auto review_ledger_application_host::available() const noexcept -> std::optional<bool>
	{
		return Implementation->Available;
	}

	auto review_ledger_application_host::outstanding() const noexcept -> std::size_t
	{
		return Implementation->Queue.size();
	}

	auto review_ledger_application_host::is_quiescent() const noexcept -> bool
	{
		return !Implementation->Running && Implementation->Queue.empty() && Implementation->PendingTerminals == 0U;
	}

	void review_ledger_application_host::retain(const core::file_receipt& aReceipt)
	{
		++Implementation->MaterialGeneration;
		Implementation->Retained = aReceipt;
	}

	void review_ledger_application_host::observe_material(
		const core::file_material_observation& aObservation,
		ledger_completion<core::file_receipt>  aDone)
	{
		using impl         = implementation;
		auto* const self   = Implementation.get();
		const auto  latest = ++self->MaterialGeneration;
		auto        fail   = [self, done = aDone](const ledger_store_failure aFailure) {
			if (aFailure != ledger_store_failure::cancelled) {
				self->publish_failure({ .Retained = self->Retained, .Store = aFailure });
			}
			impl::answer(done, std::expected<core::file_receipt, ledger_store_failure>{ std::unexpected(aFailure) });
		};
		self->enqueue(impl::operation{
			.Begin = [self, aObservation, latest, done = std::move(aDone)](impl::outcome& aOutcome) mutable {
				++self->Statistics.MaterialObservations;
				++self->Statistics.ReceiptLoads;
				aOutcome.Next = impl::storage<std::expected<std::optional<core::file_receipt>, ledger_store_failure>>(
					[file = aObservation.File](review_ledger_store_port& aStore) { return aStore.load(file); },
					[self, aObservation, latest, serial = self->ReceiptInvalidationSerial, done](
						std::expected<std::optional<core::file_receipt>, ledger_store_failure> aLoaded,
						impl::outcome&                                                         aNext) mutable {
						const auto current = latest == self->MaterialGeneration;
						if (!aLoaded) {
							++self->Statistics.StoreFailures;
							aNext.Terminal = [self, current, failure = aLoaded.error(), done] {
								if (current) {
									self->publish_failure({ .Retained = self->Retained, .Store = failure });
								}
								impl::answer(done, std::expected<core::file_receipt, ledger_store_failure>{ std::unexpected(failure) });
							};
							return;
						}
						if (aLoaded->has_value()) {
							++self->Statistics.ReceiptLoadHits;
							++self->Statistics.MaterialReceiptHits;
						}
						self->remember_read(aObservation.File, *aLoaded, serial);
						auto       observed = aLoaded->has_value()
							? core::carry_over(**aLoaded, aObservation).Receipt
							: core::file_receipt{ .File = aObservation.File, .Material = aObservation.Material };
						const auto finish   = [self, latest](const core::file_receipt& aReceipt, ledger_completion<core::file_receipt> aReply) {
							const auto still = latest == self->MaterialGeneration;
							if (still) {
								self->Retained = aReceipt;
							}
							return [self, still, aReceipt, callback = std::move(aReply)] {
								if (still) {
									self->publish(aReceipt);
								}
								impl::answer(callback, std::expected<core::file_receipt, ledger_store_failure>{ aReceipt });
							};
						};
						if (!aLoaded->has_value() || observed == **aLoaded) {
							aNext.Terminal = finish(observed, std::move(done));
							return;
						}
						// The core carried an old receipt to new material: that carry
						// is durable before anything is retained or shown.
						++self->Statistics.PersistenceRequests;
						aNext.Next = impl::storage<std::expected<void, ledger_store_failure>>(
							[observed](review_ledger_store_port& aStore) { return aStore.commit(receipt_persistence_request{ .Receipt = observed }); },
							[self, observed, latest, finish, done](std::expected<void, ledger_store_failure> aCommitted, impl::outcome& aLast) mutable {
								++self->Statistics.Completions;
								if (!aCommitted) {
									++self->Statistics.StoreFailures;
									const auto stillCurrent = latest == self->MaterialGeneration;
									aLast.Terminal          = [self, stillCurrent, failure = aCommitted.error(), done] {
										if (stillCurrent) {
											self->publish_failure({ .Retained = self->Retained, .Store = failure });
										}
										impl::answer(done, std::expected<core::file_receipt, ledger_store_failure>{ std::unexpected(failure) });
									};
									return;
								}
								self->remember_durable(observed.File, observed, true);
								aLast.Terminal = finish(observed, std::move(done));
							});
					});
			},
			.Fail = std::move(fail),
		});
	}

	void review_ledger_application_host::refresh_receipt(ledger_completion<core::file_receipt> aDone)
	{
		using impl       = implementation;
		auto* const self = Implementation.get();
		auto        fail = [done = aDone](const ledger_store_failure aFailure) {
			impl::answer(done, std::expected<core::file_receipt, ledger_store_failure>{ std::unexpected(aFailure) });
		};
		self->enqueue(impl::operation{
			.Begin = [self, done = std::move(aDone)](impl::outcome& aOutcome) mutable {
				++self->Statistics.ReceiptLoads;
				const auto observation = core::file_material_observation{ .File = self->Retained.File, .Material = self->Retained.Material };
				aOutcome.Next          = impl::storage<std::expected<std::optional<core::file_receipt>, ledger_store_failure>>(
					[file = observation.File](review_ledger_store_port& aStore) { return aStore.load(file); },
					[self, observation, generation = self->MaterialGeneration, serial = self->ReceiptInvalidationSerial, done](
						std::expected<std::optional<core::file_receipt>, ledger_store_failure> aLoaded,
						impl::outcome&                                                         aNext) mutable {
						const auto refuse = [&aNext, &done](const ledger_store_failure aFailure) {
							aNext.Terminal = [aFailure, done] {
								impl::answer(done, std::expected<core::file_receipt, ledger_store_failure>{ std::unexpected(aFailure) });
							};
						};
						if (!aLoaded) {
							++self->Statistics.StoreFailures;
							refuse(aLoaded.error());
							return;
						}
						if (*aLoaded && (**aLoaded).File != observation.File) {
							++self->Statistics.StoreFailures;
							refuse(ledger_store_failure::integrity_failure);
							return;
						}
						self->remember_read(observation.File, *aLoaded, serial);
						if (generation != self->MaterialGeneration) {
							// Another file or material is retained now.
							refuse(ledger_store_failure::cancelled);
							return;
						}
						if (*aLoaded) {
							++self->Statistics.ReceiptLoadHits;
							self->Retained = core::carry_over(**aLoaded, observation).Receipt;
						} else {
							self->Retained = core::file_receipt{ .File = observation.File, .Material = observation.Material };
						}
						aNext.Terminal = [self, receipt = self->Retained, done] {
							self->publish(receipt);
							impl::answer(done, std::expected<core::file_receipt, ledger_store_failure>{ receipt });
						};
					});
			},
			.Fail = std::move(fail),
		});
	}

	void review_ledger_application_host::submit(
		const core::file_receipt_command&             aCommand,
		ledger_completion<core::file_receipt_outcome> aDone)
	{
		using impl       = implementation;
		auto* const self = Implementation.get();
		auto        fail = [self, done = aDone](const ledger_store_failure aFailure) {
			if (aFailure != ledger_store_failure::cancelled) {
				self->publish_failure({ .Retained = self->Retained, .Store = aFailure });
			}
			impl::answer(done, std::expected<core::file_receipt_outcome, ledger_store_failure>{ std::unexpected(aFailure) });
		};
		self->enqueue(impl::operation{
			.Begin = [self, aCommand, done = std::move(aDone)](impl::outcome& aOutcome) mutable {
				++self->Statistics.Commands;
				// Decided now, at the head of the queue, against what every
				// operation before this one left durable.
				const auto outcome = core::apply(self->Retained, aCommand);
				if (outcome.Rejection != core::file_receipt_rejection::none) {
					++self->Statistics.Rejected;
					aOutcome.Terminal = [self, outcome, retained = self->Retained, done] {
						self->publish_failure({ .Retained = retained, .Rejection = outcome.Rejection });
						impl::answer(done, std::expected<core::file_receipt_outcome, ledger_store_failure>{ outcome });
					};
					return;
				}
				if (!outcome.Changed) {
					// A legal command that moved nothing is not an error and is not a
					// write. Persisting it would turn every repeated click into a commit,
					// and publishing it would announce a change that did not happen.
					++self->Statistics.Unchanged;
					aOutcome.Terminal = [self, outcome, aCommand, done] {
						try {
							if (self->FileStatistics) {
								self->FileStatistics(outcome.Receipt, aCommand.Kind);
							}
						} catch (...) { }
						impl::answer(done, std::expected<core::file_receipt_outcome, ledger_store_failure>{ outcome });
					};
					return;
				}
				++self->Statistics.PersistenceRequests;
				aOutcome.Next = impl::storage<std::expected<void, ledger_store_failure>>(
					[receipt = outcome.Receipt](review_ledger_store_port& aStore) {
						return aStore.commit(receipt_persistence_request{ .Receipt = receipt });
					},
					[self, outcome, aCommand, generation = self->MaterialGeneration, done](
						std::expected<void, ledger_store_failure> aCommitted, impl::outcome& aNext) mutable {
						++self->Statistics.Completions;
						if (!aCommitted) {
							// The retained receipt keeps its previous value. Adopting the
							// rejected one would leave the host disagreeing with the database
							// until something else overwrote it.
							++self->Statistics.StoreFailures;
							const auto kept = core::file_receipt_outcome{
								.Receipt   = self->Retained,
								.Rejection = core::file_receipt_rejection::none,
								.Changed   = false,
							};
							aNext.Terminal = [self, kept, failure = aCommitted.error(), done] {
								self->publish_failure({ .Retained = kept.Receipt, .Store = failure });
								impl::answer(done, std::expected<core::file_receipt_outcome, ledger_store_failure>{ kept });
							};
							return;
						}
						try {
							if (self->FileStatistics) {
								self->FileStatistics(outcome.Receipt, aCommand.Kind);
							}
						} catch (...) { }
						self->remember_durable(outcome.Receipt.File, outcome.Receipt, true);
						// A `retain` while the write ran selected something else;
						// the commit is durable, and it is no longer this view's.
						const auto still = generation == self->MaterialGeneration;
						if (still) {
							self->Retained = outcome.Receipt;
						}
						aNext.Terminal = [self, outcome, still, done] {
							if (still) {
								self->publish(outcome.Receipt);
							}
							impl::answer(done, std::expected<core::file_receipt_outcome, ledger_store_failure>{ outcome });
						};
					});
			},
			.Fail = std::move(fail),
		});
	}

	auto review_ledger_application_host::cached_receipt(const core::file_identity& aFile)
		-> std::optional<std::optional<core::file_receipt>>
	{
		const auto found = Implementation->DurableReceipts.find(aFile);
		if (found == Implementation->DurableReceipts.end()) {
			return std::nullopt;
		}
		++Implementation->Statistics.ReceiptCacheHits;
		return found->second;
	}

	void review_ledger_application_host::load_receipts(std::vector<core::file_identity> aFiles, ledger_completion<void> aDone)
	{
		using impl       = implementation;
		auto* const self = Implementation.get();
		using loaded     = std::vector<std::pair<core::file_identity, std::optional<core::file_receipt>>>;
		auto fail        = [done = aDone](const ledger_store_failure aFailure) {
			impl::answer(done, std::expected<void, ledger_store_failure>{ std::unexpected(aFailure) });
		};
		self->enqueue(impl::operation{
			.Begin = [self, files = std::move(aFiles), done = std::move(aDone)](impl::outcome& aOutcome) mutable {
				std::ranges::sort(files);
				files.erase(std::ranges::unique(files).begin(), files.end());
				std::erase_if(files, [self](const core::file_identity& aFile) { return self->DurableReceipts.contains(aFile); });
				if (files.empty()) {
					aOutcome.Terminal = [done] { impl::answer(done, std::expected<void, ledger_store_failure>{ }); };
					return;
				}
				self->Statistics.ReceiptLoads += files.size();
				aOutcome.Next = impl::storage<std::expected<loaded, ledger_store_failure>>(
					[files](review_ledger_store_port& aStore) -> std::expected<loaded, ledger_store_failure> {
						auto answers = loaded{ };
						answers.reserve(files.size());
						for (const auto& file : files) {
							auto receipt = aStore.load(file);
							if (!receipt) {
								return std::unexpected(receipt.error());
							}
							answers.emplace_back(file, std::move(*receipt));
						}
						return answers;
					},
					[self, serial = self->ReceiptInvalidationSerial, done](
						std::expected<loaded, ledger_store_failure> aLoaded, impl::outcome& aNext) mutable {
						if (!aLoaded) {
							++self->Statistics.StoreFailures;
							aNext.Terminal = [failure = aLoaded.error(), done] {
								impl::answer(done, std::expected<void, ledger_store_failure>{ std::unexpected(failure) });
							};
							return;
						}
						for (const auto& [file, receipt] : *aLoaded) {
							if (receipt.has_value()) {
								++self->Statistics.ReceiptLoadHits;
							}
							self->remember_read(file, receipt, serial);
						}
						aNext.Terminal = [done] { impl::answer(done, std::expected<void, ledger_store_failure>{ }); };
					});
			},
			.Fail = std::move(fail),
		});
	}

	void review_ledger_application_host::invalidate_receipts(const std::span<const core::file_identity> aFiles)
	{
		auto& self = *Implementation;
		if (aFiles.empty()) {
			return;
		}
		++self.ReceiptInvalidationSerial;
		++self.DurableRevision;
		for (const auto& file : aFiles) {
			self.Statistics.ReceiptInvalidations += self.DurableReceipts.erase(file);
			self.record_durable_change(file);
		}
	}

	void review_ledger_application_host::invalidate_all_receipts()
	{
		auto& self = *Implementation;
		++self.ReceiptInvalidationSerial;
		self.Statistics.ReceiptInvalidations += self.DurableReceipts.size();
		self.DurableReceipts.clear();
		++self.DurableRevision;
		self.DurableChanges.clear();
		self.DurableChangesFloor = self.DurableRevision;
	}

	auto review_ledger_application_host::durable_receipt_revision() const noexcept -> std::uint64_t
	{
		return Implementation->DurableRevision;
	}

	auto review_ledger_application_host::durable_changes_since(const std::uint64_t aRevision) const
		-> std::optional<std::vector<core::file_identity>>
	{
		const auto& self = *Implementation;
		if (aRevision < self.DurableChangesFloor || aRevision > self.DurableRevision) {
			return std::nullopt;
		}
		auto files = std::vector<core::file_identity>{ };
		for (auto change = self.DurableChanges.rbegin(); change != self.DurableChanges.rend() && change->first > aRevision; ++change) {
			files.push_back(change->second);
		}
		std::ranges::sort(files);
		files.erase(std::ranges::unique(files).begin(), files.end());
		return files;
	}

	auto review_ledger_application_host::unread_conversations(
		const std::span<const conversation_thread_observation> aThreads) const
		-> std::optional<conversation_read_state>
	{
		const auto& receipts = Implementation->ConversationReceipts;
		if (!receipts.has_value()) {
			return std::nullopt;
		}
		auto state = application::conversation_read_state{ };
		for (const auto& thread : aThreads) {
			const auto receipt = find_conversation_receipt(*receipts, thread.Conversation);
			const auto unread  = core::unread_posts(
				receipt != nullptr
					? *receipt
					: core::conversation_receipt{ .Conversation = thread.Conversation },
				thread.Posts);
			if (unread.empty()) {
				continue;
			}
			auto projection = conversation_read_projection{
				.Conversation = thread.Conversation,
				.SeenBefore   = receipt != nullptr,
			};
			for (const auto index : unread) {
				projection.UnreadPosts.push_back(thread.Posts[index].Comment);
			}
			state.Threads.push_back(std::move(projection));
		}
		state.UnreadThreads = static_cast<std::uint32_t>(state.Threads.size());
		return state;
	}

	void review_ledger_application_host::load_conversation_receipts(ledger_completion<void> aDone)
	{
		using impl       = implementation;
		auto* const self = Implementation.get();
		if (self->ConversationReceipts.has_value() && !self->Retired) {
			self->post_terminal([done = std::move(aDone)] { impl::answer(done, std::expected<void, ledger_store_failure>{ }); });
			return;
		}
		self->ConversationWaiters.push_back(std::move(aDone));
		if (self->ConversationLoadQueued) {
			return;
		}
		self->ConversationLoadQueued = true;
		const auto finish            = [self](const std::expected<void, ledger_store_failure>& aResult) {
			self->ConversationLoadQueued = false;
			impl::flush(self->ConversationWaiters, aResult);
		};
		self->enqueue(impl::operation{
			.Begin = [self, finish](impl::outcome& aOutcome) { self->with_conversation_receipts(
																   aOutcome,
																   [finish](impl::outcome& aNext) { aNext.Terminal = [finish] { finish({ }); }; },
																   [finish](const ledger_store_failure aFailure) { finish(std::unexpected(aFailure)); }); },
			.Fail  = [finish](const ledger_store_failure aFailure) { finish(std::unexpected(aFailure)); },
		});
	}

	void review_ledger_application_host::invalidate_conversation_receipts()
	{
		++Implementation->ConversationSerial;
		Implementation->ConversationReceipts.reset();
	}

	void review_ledger_application_host::mark_conversation_read(const core::conversation_observation& aLatest, ledger_completion<void> aDone)
	{
		store_conversation_receipt(
			aLatest.Conversation,
			[aLatest](const core::conversation_receipt& aReceipt) -> std::optional<core::conversation_receipt> {
				return core::mark_conversation_read(aReceipt, aLatest);
			},
			std::move(aDone));
	}

	void review_ledger_application_host::mark_conversation_unread(const conversation_thread_observation& aThread, ledger_completion<void> aDone)
	{
		// [[BPR-1582]]. The core decides which post is new again; a thread of
		// only my posts has none, and the store is not written.
		store_conversation_receipt(
			aThread.Conversation,
			[posts = aThread.Posts](const core::conversation_receipt& aReceipt) {
				return core::mark_conversation_unread(aReceipt, posts);
			},
			std::move(aDone));
	}

	void review_ledger_application_host::mark_conversations_read(
		std::vector<conversation_thread_observation>    aThreads,
		const core::conversation_read_scope             aScope,
		ledger_completion<core::conversation_bulk_read> aDone)
	{
		// [[BPR-1699]]. One decision over the held receipts and one
		// commit of every receipt it changes: either all are read or none.
		using impl       = implementation;
		using answered   = std::expected<core::conversation_bulk_read, ledger_store_failure>;
		auto* const self = Implementation.get();
		auto        fail = [done = aDone](const ledger_store_failure aFailure) {
			impl::answer(done, answered{ std::unexpected(aFailure) });
		};
		self->enqueue(impl::operation{
			.Begin = [self, observed = std::move(aThreads), aScope, done = std::move(aDone), fail](impl::outcome& aOutcome) {
				self->with_conversation_receipts(
					aOutcome,
					[self, observed, aScope, done](impl::outcome& aNext) {
						auto decision = core::mark_conversations_read(*self->ConversationReceipts, observed, aScope);
						if (decision.Receipts.empty()) {
							++self->Statistics.Unchanged;
							aNext.Terminal = [done, decision] { impl::answer(done, answered{ decision }); };
							return;
						}
						if (decision.Receipts.size() > maximum_conversation_receipts
							|| !std::ranges::all_of(decision.Receipts, [](const core::conversation_receipt& aReceipt) { return valid(aReceipt); })) {
							aNext.Terminal = [done] { impl::answer(done, answered{ std::unexpected(ledger_store_failure::invalid_record) }); };
							return;
						}
						++self->Statistics.ConversationWrites;
						aNext.Next = impl::storage<std::expected<void, ledger_store_failure>>(
							[receipts = decision.Receipts](review_ledger_store_port& aStore) { return aStore.store_conversation_receipts(receipts); },
							[self, decision, done](std::expected<void, ledger_store_failure> aStored, impl::outcome& aLast) {
								if (!aStored) {
									++self->Statistics.StoreFailures;
									++self->Statistics.ConversationFailures;
									aLast.Terminal = [failure = aStored.error(), done] { impl::answer(done, answered{ std::unexpected(failure) }); };
									return;
								}
								if (self->ConversationReceipts.has_value()) {
									auto& kept = *self->ConversationReceipts;
									for (const auto& read : decision.Receipts) {
										const auto at = std::ranges::lower_bound(kept, read.Conversation, { }, &core::conversation_receipt::Conversation);
										if (at != kept.end() && at->Conversation == read.Conversation) {
											*at = read;
										} else {
											kept.insert(at, read);
										}
									}
								}
								aLast.Terminal = [done, decision] { impl::answer(done, answered{ decision }); };
							});
					},
					fail);
			},
			.Fail = std::move(fail),
		});
	}

	void review_ledger_application_host::store_conversation_receipt(
		const core::review_conversation_id                                                          aConversation,
		std::function<std::optional<core::conversation_receipt>(const core::conversation_receipt&)> aDecide,
		ledger_completion<void>                                                                     aDone)
	{
		using impl       = implementation;
		auto* const self = Implementation.get();
		auto        fail = [done = aDone](const ledger_store_failure aFailure) {
			impl::answer(done, std::expected<void, ledger_store_failure>{ std::unexpected(aFailure) });
		};
		self->enqueue(impl::operation{
			.Begin = [self, aConversation, decide = std::move(aDecide), done = std::move(aDone), fail](impl::outcome& aOutcome) mutable {
				self->with_conversation_receipts(
					aOutcome,
					[self, aConversation, decide, done](impl::outcome& aNext) mutable {
						auto&      receipts = *self->ConversationReceipts;
						const auto existing = std::ranges::lower_bound(receipts, aConversation, { }, &core::conversation_receipt::Conversation);
						const auto found    = existing != receipts.end() && existing->Conversation == aConversation;
						const auto decided  = decide(found ? *existing : core::conversation_receipt{ .Conversation = aConversation });
						if (!decided.has_value() || decided->Conversation != aConversation || !valid(*decided)) {
							aNext.Terminal = [done] {
								impl::answer(done, std::expected<void, ledger_store_failure>{ std::unexpected(ledger_store_failure::invalid_record) });
							};
							return;
						}
						const auto read = *decided;
						if (found && *existing == read) {
							++self->Statistics.Unchanged;
							aNext.Terminal = [done] { impl::answer(done, std::expected<void, ledger_store_failure>{ }); };
							return;
						}
						++self->Statistics.ConversationWrites;
						aNext.Next = impl::storage<std::expected<void, ledger_store_failure>>(
							[read](review_ledger_store_port& aStore) { return aStore.store_conversation_receipt(read); },
							[self, read, done](std::expected<void, ledger_store_failure> aStored, impl::outcome& aLast) mutable {
								if (!aStored) {
									++self->Statistics.StoreFailures;
									++self->Statistics.ConversationFailures;
									aLast.Terminal = [failure = aStored.error(), done] {
										impl::answer(done, std::expected<void, ledger_store_failure>{ std::unexpected(failure) });
									};
									return;
								}
								if (self->ConversationReceipts.has_value()) {
									auto&      kept = *self->ConversationReceipts;
									const auto at   = std::ranges::lower_bound(kept, read.Conversation, { }, &core::conversation_receipt::Conversation);
									if (at != kept.end() && at->Conversation == read.Conversation) {
										*at = read;
									} else {
										kept.insert(at, read);
									}
								}
								aLast.Terminal = [done] { impl::answer(done, std::expected<void, ledger_store_failure>{ }); };
							});
					},
					fail);
			},
			.Fail = std::move(fail),
		});
	}

	auto review_ledger_application_host::private_observation() const
		-> std::expected<private_ledger_observation, ledger_store_failure>
	{
		const auto& self = *Implementation;
		if (!self.Private.has_value()) {
			return std::unexpected(self.PrivateFailure.value_or(ledger_store_failure::unavailable));
		}
		return *self.Private;
	}

	void review_ledger_application_host::load_private(ledger_completion<void> aDone)
	{
		using impl       = implementation;
		auto* const self = Implementation.get();
		self->PrivateWaiters.push_back(std::move(aDone));
		if (self->PrivateLoadQueued) {
			return;
		}
		self->PrivateLoadQueued = true;
		// The requests one load answers: those made before it began. A request
		// made while it runs queues another, which reads what this one may
		// have missed.
		struct batch
		{
			bool                                 Begun = false;
			std::vector<ledger_completion<void>> Waiters;
		};
		const auto answered = std::make_shared<batch>();
		self->enqueue(impl::operation{
			.Begin = [self, answered](impl::outcome& aOutcome) {
				++self->Statistics.PrivateLoads;
				self->PrivateLoadQueued = false;
				answered->Begun         = true;
				answered->Waiters       = std::exchange(self->PrivateWaiters, { });
				aOutcome.Next           = impl::storage<std::expected<private_ledger_observation, ledger_store_failure>>(
                    [](review_ledger_store_port& aStore) { return aStore.load_private(); },
                    [self, answered](std::expected<private_ledger_observation, ledger_store_failure> aLoaded, impl::outcome& aNext) {
                        auto result = std::expected<void, ledger_store_failure>{ };
                        if (!aLoaded) {
                            ++self->Statistics.PrivateFailures;
                            self->PrivateFailure = aLoaded.error();
                            result               = std::unexpected(aLoaded.error());
                        } else {
                            self->Private = std::move(*aLoaded);
                            self->PrivateFailure.reset();
                        }
                        aNext.Terminal = [answered, result] { impl::flush(answered->Waiters, result); };
                    }); },
			.Fail  = [self, answered](const ledger_store_failure aFailure) {
				if (!answered->Begun) {
					self->PrivateLoadQueued = false;
					answered->Waiters       = std::exchange(self->PrivateWaiters, { });
				}
				impl::flush(answered->Waiters, std::unexpected(aFailure)); },
		});
	}

	void review_ledger_application_host::persist(
		const local_note_record&                 aNote,
		const std::optional<local_note_revision> aExpectedRevision,
		local_note_completion<local_note_record> aDone)
	{
		using impl       = implementation;
		auto* const self = Implementation.get();
		auto        fail = [done = aDone](const ledger_store_failure aFailure) {
			impl::answer(done, std::expected<local_note_record, local_note_failure>{ std::unexpected(local_note_failure{ .Reason = aFailure }) });
		};
		self->enqueue(impl::operation{
			.Begin = [self, aNote, aExpectedRevision, done = std::move(aDone)](impl::outcome& aOutcome) mutable {
				if (!valid(aNote) || (aExpectedRevision && !durable_integer(aExpectedRevision->value()))) {
					++self->Statistics.PrivateFailures;
					aOutcome.Terminal = [done] {
						impl::answer(done, std::expected<local_note_record, local_note_failure>{ std::unexpected(local_note_failure{ .Reason = ledger_store_failure::invalid_record }) });
					};
					return;
				}
				++self->Statistics.PrivateWrites;
				aOutcome.Next = impl::storage<std::expected<local_note_record, local_note_failure>>(
					[aNote, aExpectedRevision, insights = self->Insights](review_ledger_store_port& aStore) {
						auto result = aStore.store_note(aNote, aExpectedRevision);
						if (result && !aExpectedRevision) {
							insights.record_new(core::insights_review_kind::note);
						}
						return result;
					},
					[self, aNote, aExpectedRevision, done](std::expected<local_note_record, local_note_failure> aStored, impl::outcome& aNext) mutable {
						if (!aStored) {
							++self->Statistics.PrivateFailures;
							const auto& current = aStored.error().Current;
							if (current && (current->Id != aNote.Id || !valid(*current) || !durable_integer(current->Revision.value()))) {
								aStored = std::unexpected(local_note_failure{ .Reason = ledger_store_failure::integrity_failure });
							}
							if (!self->RejectedNote) {
								self->RejectedNote = rejected_local_note_mutation{ .Id = aNote.Id, .Candidate = aNote, .ExpectedRevision = aExpectedRevision, .Failure = aStored.error() };
							}
						} else {
							self->adopt_note(*aStored);
						}
						aNext.Terminal = [stored = std::move(aStored), done] { impl::answer(done, stored); };
					});
			},
			.Fail = std::move(fail),
		});
	}

	void review_ledger_application_host::erase(const local_note_id aNote, const local_note_revision aExpectedRevision, local_note_completion<void> aDone)
	{
		using impl       = implementation;
		auto* const self = Implementation.get();
		auto        fail = [done = aDone](const ledger_store_failure aFailure) {
			impl::answer(done, std::expected<void, local_note_failure>{ std::unexpected(local_note_failure{ .Reason = aFailure }) });
		};
		self->enqueue(impl::operation{
			.Begin = [self, aNote, aExpectedRevision, done = std::move(aDone)](impl::outcome& aOutcome) mutable {
				if (!durable_integer(aNote.value()) || !durable_integer(aExpectedRevision.value())) {
					++self->Statistics.PrivateFailures;
					aOutcome.Terminal = [done] {
						impl::answer(done, std::expected<void, local_note_failure>{ std::unexpected(local_note_failure{ .Reason = ledger_store_failure::invalid_record }) });
					};
					return;
				}
				++self->Statistics.PrivateDeletes;
				aOutcome.Next = impl::storage<std::expected<void, local_note_failure>>(
					[aNote, aExpectedRevision](review_ledger_store_port& aStore) { return aStore.erase_note(aNote, aExpectedRevision); },
					[self, aNote, aExpectedRevision, done](std::expected<void, local_note_failure> aErased, impl::outcome& aNext) mutable {
						if (!aErased) {
							++self->Statistics.PrivateFailures;
							const auto& current = aErased.error().Current;
							if (current && (current->Id != aNote || !valid(*current) || !durable_integer(current->Revision.value()))) {
								aErased = std::unexpected(local_note_failure{ .Reason = ledger_store_failure::integrity_failure });
							}
							if (!self->RejectedNote) {
								self->RejectedNote = rejected_local_note_mutation{ .Id = aNote, .ExpectedRevision = aExpectedRevision, .Failure = aErased.error() };
							}
						} else if (self->Private) {
							std::erase_if(self->Private->Notes, [aNote](const local_note_record& aRecord) { return aRecord.Id == aNote; });
						}
						aNext.Terminal = [erased = std::move(aErased), done] { impl::answer(done, erased); };
					});
			},
			.Fail = std::move(fail),
		});
	}

	auto review_ledger_application_host::rejected_note() const
		-> std::optional<rejected_local_note_mutation>
	{
		return Implementation->RejectedNote;
	}

	void review_ledger_application_host::dismiss_rejected_note()
	{
		Implementation->RejectedNote.reset();
	}

	namespace {
		/// One validated private write whose success is adopted by `aAdopt`.
		template <typename Implementation, typename Work, typename Adopt>
		void private_write(Implementation& aSelf, const bool aValid, const bool aDelete, Work aWork, Adopt aAdopt, ledger_completion<void> aDone)
		{
			using impl = Implementation;
			auto fail  = [done = aDone](const ledger_store_failure aFailure) {
				impl::answer(done, std::expected<void, ledger_store_failure>{ std::unexpected(aFailure) });
			};
			aSelf.enqueue(typename impl::operation{
				.Begin = [self = &aSelf, aValid, aDelete, work = std::move(aWork), adopt = std::move(aAdopt), done = std::move(aDone)](typename impl::outcome& aOutcome) mutable {
					if (!aValid) {
						++self->Statistics.PrivateFailures;
						aOutcome.Terminal = [done] {
							impl::answer(done, std::expected<void, ledger_store_failure>{ std::unexpected(ledger_store_failure::invalid_record) });
						};
						return;
					}
					if (aDelete) {
						++self->Statistics.PrivateDeletes;
					} else {
						++self->Statistics.PrivateWrites;
					}
					aOutcome.Next = impl::template storage<std::expected<void, ledger_store_failure>>(
						std::move(work),
						[self, adopt, done](std::expected<void, ledger_store_failure> aWritten, typename impl::outcome& aNext) mutable {
							if (!aWritten) {
								++self->Statistics.PrivateFailures;
							} else {
								adopt(*self);
							}
							aNext.Terminal = [written = aWritten, done] { impl::answer(done, written); };
						});
				},
				.Fail = std::move(fail),
			});
		}
	} // namespace

	void review_ledger_application_host::persist_last_reviewed_head(const core::git_commit_oid& aHead, ledger_completion<void> aDone)
	{
		private_write(
			*Implementation, aHead.valid(), false,
			[aHead](review_ledger_store_port& aStore) { return aStore.store_last_reviewed_head(aHead); },
			[aHead](implementation& aSelf) {
				if (aSelf.Private) {
					aSelf.Private->LastReviewedHead = aHead;
				}
			},
			std::move(aDone));
	}

	void review_ledger_application_host::erase_last_reviewed_head(ledger_completion<void> aDone)
	{
		private_write(
			*Implementation, true, true,
			[](review_ledger_store_port& aStore) { return aStore.erase_last_reviewed_head(); },
			[](implementation& aSelf) {
				if (aSelf.Private) {
					aSelf.Private->LastReviewedHead.reset();
				}
			},
			std::move(aDone));
	}

	void review_ledger_application_host::persist_generated_mark(const core::generated_file_mark& aMark, ledger_completion<void> aDone)
	{
		const auto remove = aMark.Mark == core::generated_mark::none && !aMark.ReviewAnyway;
		const auto valid  = aMark.File.valid() && aMark.Mark <= core::generated_mark::treat_as_generated;
		private_write(
			*Implementation, valid, remove,
			[aMark, remove](review_ledger_store_port& aStore) {
				return remove ? aStore.erase_generated_mark(aMark.File) : aStore.store_generated_mark(aMark);
			},
			[aMark, remove](implementation& aSelf) {
				if (!aSelf.Private) {
					return;
				}
				auto&      marks = aSelf.Private->GeneratedMarks.Marks;
				const auto at    = std::ranges::lower_bound(marks, aMark.File, { }, &core::generated_file_mark::File);
				const auto found = at != marks.end() && at->File == aMark.File;
				if (remove) {
					if (found) {
						marks.erase(at);
					}
				} else if (found) {
					*at = aMark;
				} else {
					marks.insert(at, aMark);
				}
			},
			std::move(aDone));
	}

	void review_ledger_application_host::persist(const durable_defect_record& aDefect, ledger_completion<void> aDone)
	{
		private_write(
			*Implementation, valid(aDefect), false,
			[aDefect, insights = Implementation->Insights](review_ledger_store_port& aStore) {
				auto before = aStore.load_private();
				auto result = aStore.store_defect(aDefect);
				if (result && before && std::ranges::none_of(before->Defects, [&](const auto& aValue) { return aValue.History.Defect.Id == aDefect.History.Defect.Id; })) {
					insights.record_new(core::insights_review_kind::defect);
				}
				return result;
			},
			[aDefect](implementation& aSelf) { aSelf.adopt_defect(aDefect); },
			std::move(aDone));
	}

	void review_ledger_application_host::erase(const core::defect_id aDefect, ledger_completion<void> aDone)
	{
		private_write(
			*Implementation, durable_integer(aDefect.value()), true,
			[aDefect](review_ledger_store_port& aStore) { return aStore.erase_defect(aDefect); },
			[aDefect](implementation& aSelf) {
				if (aSelf.Private) {
					std::erase_if(aSelf.Private->Defects, [aDefect](const durable_defect_record& aRecord) { return aRecord.History.Defect.Id == aDefect; });
				}
			},
			std::move(aDone));
	}

	void review_ledger_application_host::admit(const agent_session_admission& aAdmission, ledger_completion<core::agent_session> aDone)
	{
		using impl       = implementation;
		auto* const self = Implementation.get();
		auto        fail = [done = aDone](const ledger_store_failure aFailure) {
			impl::answer(done, std::expected<core::agent_session, ledger_store_failure>{ std::unexpected(aFailure) });
		};
		self->enqueue(impl::operation{
			.Begin = [self, aAdmission, done = std::move(aDone)](impl::outcome& aOutcome) mutable {
				if (!valid(aAdmission)) {
					++self->Statistics.SessionFailures;
					aOutcome.Terminal = [done] {
						impl::answer(done, std::expected<core::agent_session, ledger_store_failure>{ std::unexpected(ledger_store_failure::invalid_record) });
					};
					return;
				}
				++self->Statistics.SessionAdmissions;
				aOutcome.Next = impl::storage<std::expected<core::agent_session, ledger_store_failure>>(
					[aAdmission](review_ledger_store_port& aStore) { return aStore.admit_agent_session(aAdmission); },
					[self, done](std::expected<core::agent_session, ledger_store_failure> aAdmitted, impl::outcome& aNext) mutable {
						if (!aAdmitted) {
							++self->Statistics.SessionFailures;
						}
						aNext.Terminal = [admitted = std::move(aAdmitted), done] { impl::answer(done, admitted); };
					});
			},
			.Fail = std::move(fail),
		});
	}

	void review_ledger_application_host::session_catalog(ledger_completion<agent_session_catalog_state> aDone)
	{
		using impl       = implementation;
		auto* const self = Implementation.get();
		auto        fail = [done = aDone](const ledger_store_failure aFailure) {
			impl::answer(done, std::expected<agent_session_catalog_state, ledger_store_failure>{ std::unexpected(aFailure) });
		};
		self->enqueue(impl::operation{
			.Begin = [self, done = std::move(aDone)](impl::outcome& aOutcome) mutable {
				++self->Statistics.SessionLoads;
				aOutcome.Next = impl::storage<std::expected<agent_session_catalog_state, ledger_store_failure>>(
					[](review_ledger_store_port& aStore) { return aStore.load_agent_session_catalog(); },
					[self, done](std::expected<agent_session_catalog_state, ledger_store_failure> aLoaded, impl::outcome& aNext) mutable {
						if (!aLoaded) {
							++self->Statistics.SessionFailures;
						}
						aNext.Terminal = [loaded = std::move(aLoaded), done] { impl::answer(done, loaded); };
					});
			},
			.Fail = std::move(fail),
		});
	}

	auto review_ledger_application_host::retained() const noexcept
		-> core::file_receipt
	{
		return Implementation->Retained;
	}

	auto review_ledger_application_host::statistics() const noexcept
		-> review_ledger_host_statistics
	{
		return Implementation->Statistics;
	}
} // namespace bpr::application
