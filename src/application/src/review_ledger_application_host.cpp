#include "bpr/application/review_ledger_application_host.hpp"

#include <algorithm>
#include <array>
#include <limits>
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

		return file_receipt_action_projection{
			.Receipt = aReceipt,
			.Actions = {
				command(file_receipt_action_id::seen,
						core::file_receipt_command_kind::mark_seen),
				command(file_receipt_action_id::read,
						aReceipt.Read == core::file_read_state::read
							? core::file_receipt_command_kind::mark_unread
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

	review_ledger_application_host::review_ledger_application_host(
		review_ledger_store_port& aStore,
		receipt_projection_port&  aProjection) noexcept
		: Store{ &aStore }
		, Projection{ &aProjection }
	{
	}

	void review_ledger_application_host::retain(const core::file_receipt& aReceipt)
	{
		const auto lock = std::scoped_lock{ Mutex };
		Retained        = aReceipt;
	}

	auto review_ledger_application_host::observe_material(
		const core::file_material_observation& aObservation)
		-> std::expected<core::file_receipt, ledger_store_failure>
	{
		auto publication = receipt_projection{ };
		auto failure     = std::optional<receipt_failure_publication>{ };
		{
			const auto lock = std::scoped_lock{ Mutex };
			++Statistics.MaterialObservations;
			++Statistics.ReceiptLoads;
			auto loaded = Store->load(aObservation.File);
			if (!loaded) {
				++Statistics.StoreFailures;
				++Statistics.FailurePublications;
				failure = receipt_failure_publication{
					.Retained = Retained,
					.Store    = loaded.error(),
				};
			} else {
				if (loaded->has_value()) {
					++Statistics.ReceiptLoadHits;
				}
				auto observed = loaded->has_value()
					? core::carry_over(**loaded, aObservation).Receipt
					: core::file_receipt{
						  .File     = aObservation.File,
						  .Material = aObservation.Material,
					  };

				if (loaded->has_value() && observed != **loaded) {
					++Statistics.PersistenceRequests;
					const auto completion = Store->commit(
						receipt_persistence_request{ .Receipt = observed });
					++Statistics.Completions;
					if (!completion) {
						++Statistics.StoreFailures;
						++Statistics.FailurePublications;
						failure = receipt_failure_publication{
							.Retained = Retained,
							.Store    = completion.error(),
						};
					}
				}

				if (!failure.has_value()) {
					Retained    = observed;
					publication = receipt_projection{ .Receipt = Retained };
					++Statistics.Publications;
				}
			}
		}

		if (failure.has_value()) {
			Projection->fail(*failure);
			return std::unexpected(*failure->Store);
		}
		Projection->publish(publication);
		return publication.Receipt;
	}

	auto review_ledger_application_host::load_receipt(const core::file_identity& aFile)
		-> std::expected<std::optional<core::file_receipt>, ledger_store_failure>
	{
		const auto lock = std::scoped_lock{ Mutex };
		++Statistics.ReceiptLoads;
		auto loaded = Store->load(aFile);
		if (!loaded) {
			++Statistics.StoreFailures;
		}
		return loaded;
	}

	auto review_ledger_application_host::conversation_receipts_locked()
		-> std::expected<void, ledger_store_failure>
	{
		if (ConversationReceipts.has_value()) {
			return { };
		}
		++Statistics.ConversationLoads;
		auto loaded = Store->load_conversation_receipts();
		if (!loaded) {
			++Statistics.StoreFailures;
			++Statistics.ConversationFailures;
			return std::unexpected(loaded.error());
		}
		ConversationReceipts = std::move(*loaded);
		return { };
	}

	auto review_ledger_application_host::observe_conversations(
		const std::span<const conversation_thread_observation> aThreads)
		-> std::expected<conversation_read_state, ledger_store_failure>
	{
		const auto lock = std::scoped_lock{ Mutex };
		if (const auto loaded = conversation_receipts_locked(); !loaded) {
			return std::unexpected(loaded.error());
		}
		auto state = conversation_read_state{ };
		for (const auto& thread : aThreads) {
			const auto receipt = std::ranges::find(
				*ConversationReceipts,
				thread.Conversation,
				&core::conversation_receipt::Conversation);
			const auto unread = core::unread_posts(
				receipt != ConversationReceipts->end()
					? *receipt
					: core::conversation_receipt{ .Conversation = thread.Conversation },
				thread.Posts);
			if (unread.empty()) {
				continue;
			}
			auto projection = conversation_read_projection{
				.Conversation = thread.Conversation,
				.SeenBefore   = receipt != ConversationReceipts->end(),
			};
			for (const auto index : unread) {
				projection.UnreadPosts.push_back(thread.Posts[index].Comment);
			}
			state.Threads.push_back(std::move(projection));
		}
		state.UnreadThreads = static_cast<std::uint32_t>(state.Threads.size());
		return state;
	}

	auto review_ledger_application_host::mark_conversation_read(
		const core::conversation_observation& aLatest)
		-> std::expected<void, ledger_store_failure>
	{
		const auto lock = std::scoped_lock{ Mutex };
		if (const auto loaded = conversation_receipts_locked(); !loaded) {
			return std::unexpected(loaded.error());
		}
		const auto existing = std::ranges::find(
			*ConversationReceipts,
			aLatest.Conversation,
			&core::conversation_receipt::Conversation);
		const auto read = core::mark_conversation_read(
			existing != ConversationReceipts->end()
				? *existing
				: core::conversation_receipt{ .Conversation = aLatest.Conversation },
			aLatest);
		if (!valid(read)) {
			return std::unexpected(ledger_store_failure::invalid_record);
		}
		if (existing != ConversationReceipts->end() && *existing == read) {
			++Statistics.Unchanged;
			return { };
		}
		++Statistics.ConversationWrites;
		const auto stored = Store->store_conversation_receipt(read);
		if (!stored) {
			++Statistics.StoreFailures;
			++Statistics.ConversationFailures;
			return std::unexpected(stored.error());
		}
		if (existing != ConversationReceipts->end()) {
			*existing = read;
		} else {
			ConversationReceipts->push_back(read);
		}
		return { };
	}

	auto review_ledger_application_host::retained() const noexcept
		-> core::file_receipt
	{
		const auto lock = std::scoped_lock{ Mutex };
		return Retained;
	}

	auto review_ledger_application_host::statistics() const noexcept
		-> review_ledger_host_statistics
	{
		const auto lock = std::scoped_lock{ Mutex };
		return Statistics;
	}

	auto review_ledger_application_host::submit(
		const core::file_receipt_command& aCommand) -> core::file_receipt_outcome
	{
		auto publication = std::optional<receipt_projection>{ };
		auto failure     = std::optional<receipt_failure_publication>{ };
		auto outcome     = core::file_receipt_outcome{ };
		{
			const auto lock = std::scoped_lock{ Mutex };
			++Statistics.Commands;

			outcome = core::apply(Retained, aCommand);
			if (outcome.Rejection != core::file_receipt_rejection::none) {
				++Statistics.Rejected;
				++Statistics.FailurePublications;
				failure = receipt_failure_publication{
					.Retained  = Retained,
					.Rejection = outcome.Rejection,
				};
			} else if (!outcome.Changed) {
				// A legal command that moved nothing is not an error and is not a
				// write. Persisting it would turn every repeated click into a commit,
				// and publishing it would announce a change that did not happen.
				++Statistics.Unchanged;
			} else {
				++Statistics.PersistenceRequests;
				const auto completion = Store->commit(
					receipt_persistence_request{ .Receipt = outcome.Receipt });
				++Statistics.Completions;

				if (!completion) {
					// The retained receipt keeps its previous value. Adopting the
					// rejected one would leave the host disagreeing with the database
					// until something else overwrote it.
					++Statistics.StoreFailures;
					++Statistics.FailurePublications;
					failure = receipt_failure_publication{
						.Retained = Retained,
						.Store    = completion.error(),
					};
					outcome = core::file_receipt_outcome{
						.Receipt   = Retained,
						.Rejection = core::file_receipt_rejection::none,
						.Changed   = false,
					};
				} else {
					Retained    = outcome.Receipt;
					publication = receipt_projection{ .Receipt = Retained };
					++Statistics.Publications;
				}
			}
		}

		if (failure.has_value()) {
			Projection->fail(*failure);
		} else if (publication.has_value()) {
			Projection->publish(*publication);
		}
		return outcome;
	}

	auto review_ledger_application_host::persist(
		const local_note_record& aNote)
		-> std::expected<void, ledger_store_failure>
	{
		const auto lock = std::scoped_lock{ Mutex };
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
		const auto lock = std::scoped_lock{ Mutex };
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
		const auto lock = std::scoped_lock{ Mutex };
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
		const auto lock = std::scoped_lock{ Mutex };
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
		const auto lock = std::scoped_lock{ Mutex };
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
		const auto lock = std::scoped_lock{ Mutex };
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
		const auto lock = std::scoped_lock{ Mutex };
		++Statistics.PrivateLoads;
		auto loaded = Store->load_private();
		if (!loaded) {
			++Statistics.PrivateFailures;
		}
		return loaded;
	}

	auto review_ledger_application_host::admit(
		const agent_session_admission& aAdmission)
		-> std::expected<core::agent_session, ledger_store_failure>
	{
		const auto lock = std::scoped_lock{ Mutex };
		if (!valid(aAdmission)) {
			++Statistics.SessionFailures;
			return std::unexpected(ledger_store_failure::invalid_record);
		}
		++Statistics.SessionAdmissions;
		auto admitted = Store->admit_agent_session(aAdmission);
		if (!admitted) {
			++Statistics.SessionFailures;
		}
		return admitted;
	}

	auto review_ledger_application_host::session_catalog()
		-> std::expected<agent_session_catalog_state, ledger_store_failure>
	{
		const auto lock = std::scoped_lock{ Mutex };
		++Statistics.SessionLoads;
		auto loaded = Store->load_agent_session_catalog();
		if (!loaded) {
			++Statistics.SessionFailures;
		}
		return loaded;
	}
} // namespace bpr::application
