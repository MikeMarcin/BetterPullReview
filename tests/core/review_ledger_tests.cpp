#include "bpr/core/file_progress_mark.hpp"
#include "bpr/core/review_ledger.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>

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

	[[nodiscard]] auto material(const std::uint8_t aSeed) -> core::file_material_id
	{
		return opaque<core::file_material_id>(aSeed);
	}

	[[nodiscard]] auto file(const std::uint8_t aSeed) -> core::file_identity
	{
		return opaque<core::file_identity>(aSeed);
	}

	[[nodiscard]] auto receipt_for(
		const std::uint8_t aFile,
		const std::uint8_t aMaterial) -> core::file_receipt
	{
		return core::file_receipt{
			.File     = file(aFile),
			.Material = material(aMaterial),
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

	using kind = core::file_receipt_command_kind;

	void test_every_state_is_reachable()
	{
		const auto start = receipt_for(1U, 2U);

		const auto seen = core::apply(start, command_for(start, kind::mark_seen));
		expect(
			seen.Rejection == core::file_receipt_rejection::none && seen.Changed
				&& seen.Receipt.Read == core::file_read_state::seen,
			"an unread file becomes seen");

		const auto read
			= core::apply(seen.Receipt, command_for(start, kind::mark_read));
		expect(
			read.Changed && read.Receipt.Read == core::file_read_state::read,
			"a seen file becomes read");

		const auto reopened
			= core::apply(read.Receipt, command_for(start, kind::mark_unread));
		expect(
			reopened.Changed
				&& reopened.Receipt.Read == core::file_read_state::unread,
			"an explicit mark-unread reopens a read file");

		const auto accepted
			= core::apply(start, command_for(start, kind::accept));
		expect(
			accepted.Changed && accepted.Receipt.Accepted,
			"a file is accepted");
		const auto revoked = core::apply(
			accepted.Receipt,
			command_for(start, kind::revoke_acceptance));
		expect(
			revoked.Changed && !revoked.Receipt.Accepted,
			"acceptance is revoked");

		const auto excluded
			= core::apply(start, command_for(start, kind::exclude));
		expect(
			excluded.Changed && excluded.Receipt.Excluded,
			"a file is excluded");
		const auto included = core::apply(
			excluded.Receipt,
			command_for(start, kind::include));
		expect(
			included.Changed && !included.Receipt.Excluded,
			"an excluded file is included again");
	}

	// The two facts are independent. Conflating them is the defect this guards:
	// marking a file read is not approval, and approving it is not a claim to
	// have read it.
	void test_reading_and_accepting_are_independent()
	{
		const auto start = receipt_for(3U, 4U);

		const auto read = core::apply(start, command_for(start, kind::mark_read));
		expect(
			read.Receipt.Read == core::file_read_state::read
				&& !read.Receipt.Accepted,
			"marking a file read does not accept it");

		const auto accepted
			= core::apply(start, command_for(start, kind::accept));
		expect(
			accepted.Receipt.Accepted
				&& accepted.Receipt.Read == core::file_read_state::unread,
			"accepting a file does not mark it read");

		const auto both
			= core::apply(read.Receipt, command_for(start, kind::accept));
		expect(
			both.Receipt.Accepted
				&& both.Receipt.Read == core::file_read_state::read,
			"the two facts coexist without either overwriting the other");

		const auto revoked = core::apply(
			both.Receipt,
			command_for(start, kind::revoke_acceptance));
		expect(
			!revoked.Receipt.Accepted
				&& revoked.Receipt.Read == core::file_read_state::read,
			"revoking acceptance leaves the read progression alone");
	}

	void test_a_weaker_read_claim_never_moves_the_progression_back()
	{
		const auto start = receipt_for(5U, 6U);
		const auto read  = core::apply(start, command_for(start, kind::mark_read));
		const auto seen
			= core::apply(read.Receipt, command_for(start, kind::mark_seen));

		expect(
			seen.Rejection == core::file_receipt_rejection::none,
			"restating a weaker claim is legal rather than an error");
		expect(
			!seen.Changed && seen.Receipt.Read == core::file_read_state::read,
			"marking a read file seen changes nothing and is not a change");
	}

	void test_illegal_commands_are_refused_with_their_own_reason()
	{
		const auto start = receipt_for(7U, 8U);

		auto elsewhere       = command_for(start, kind::mark_read);
		elsewhere.File       = file(9U);
		const auto wrongFile = core::apply(start, elsewhere);
		expect(
			wrongFile.Rejection == core::file_receipt_rejection::unknown_file
				&& !wrongFile.Changed && wrongFile.Receipt == start,
			"a command naming another file is refused and changes nothing");

		auto stale               = command_for(start, kind::mark_read);
		stale.Material           = material(10U);
		const auto wrongMaterial = core::apply(start, stale);
		expect(
			wrongMaterial.Rejection
					== core::file_receipt_rejection::material_mismatch
				&& !wrongMaterial.Changed && wrongMaterial.Receipt == start,
			"a command formed against replaced material is refused");

		const auto excluded
			= core::apply(start, command_for(start, kind::exclude));
		const auto acceptExcluded
			= core::apply(excluded.Receipt, command_for(start, kind::accept));
		expect(
			acceptExcluded.Rejection
					== core::file_receipt_rejection::
						excluded_file_cannot_be_accepted
				&& !acceptExcluded.Changed,
			"an excluded file cannot be accepted");

		const auto accepted
			= core::apply(start, command_for(start, kind::accept));
		const auto excludeAccepted
			= core::apply(accepted.Receipt, command_for(start, kind::exclude));
		expect(
			excludeAccepted.Rejection
					== core::file_receipt_rejection::
						accepted_file_cannot_be_excluded
				&& !excludeAccepted.Changed,
			"an accepted file cannot be excluded");

		// The two refusals are distinct, so a caller can tell them apart.
		expect(
			acceptExcluded.Rejection != excludeAccepted.Rejection,
			"the two exclusion conflicts carry different reasons");
	}

	void test_a_repeated_command_is_legal_and_is_not_a_change()
	{
		const auto start = receipt_for(11U, 12U);
		const auto first = core::apply(start, command_for(start, kind::accept));
		const auto again
			= core::apply(first.Receipt, command_for(start, kind::accept));

		expect(
			again.Rejection == core::file_receipt_rejection::none,
			"re-issuing a command is not an error");
		expect(
			!again.Changed && again.Receipt == first.Receipt,
			"re-issuing a command reports no change, so nothing is persisted");
	}

	// Precision is the claim. A changed file must not disturb its neighbours,
	// and an unchanged file must survive an advance that changed them.
	void test_an_epoch_advance_invalidates_only_changed_material()
	{
		// Three files: one unchanged, one whose material moved, one absent from
		// the advanced comparison.
		const auto steady = core::file_receipt{
			.File     = file(20U),
			.Material = material(30U),
			.Read     = core::file_read_state::read,
			.Accepted = true,
		};
		const auto moved = core::file_receipt{
			.File     = file(21U),
			.Material = material(31U),
			.Read     = core::file_read_state::read,
			.Accepted = true,
		};
		const auto dropped = core::file_receipt{
			.File     = file(22U),
			.Material = material(32U),
			.Read     = core::file_read_state::read,
		};

		const auto steadyOutcome = core::carry_over(
			steady,
			core::file_material_observation{
				.File = steady.File, .Material = steady.Material });
		expect(
			steadyOutcome.Disposition == core::receipt_carry_over::retained
				&& steadyOutcome.Receipt == steady,
			"an unchanged file keeps its receipt byte for byte");

		const auto movedOutcome = core::carry_over(
			moved,
			core::file_material_observation{
				.File = moved.File, .Material = material(41U) });
		expect(
			movedOutcome.Disposition
				== core::receipt_carry_over::reopened_material_changed,
			"a file whose material moved has its receipt reopened");
		expect(
			movedOutcome.Receipt.Read == core::file_read_state::unread
				&& !movedOutcome.Receipt.Accepted,
			"reopening clears both the read progression and the acceptance");
		expect(
			movedOutcome.Receipt.Material == material(41U),
			"the reopened receipt re-attaches to the new material");

		const auto droppedOutcome = core::carry_over_absent(dropped);
		expect(
			droppedOutcome.Disposition == core::receipt_carry_over::file_absent,
			"a file absent from the advance is neither retained nor reopened");

		// The neighbours are untouched: this is the precision claim, asserted
		// per file rather than by counting invalidations.
		expect(
			steadyOutcome.Receipt.Read == core::file_read_state::read
				&& steadyOutcome.Receipt.Accepted,
			"the changed neighbour did not disturb the unchanged file");
	}

	// Exclusion is a standing statement about a file, not about particular
	// bytes. Reopening it on every advance would make the setting useless for
	// exactly the generated artefacts it exists for.
	void test_exclusion_survives_a_material_change()
	{
		const auto excluded = core::file_receipt{
			.File     = file(23U),
			.Material = material(33U),
			.Read     = core::file_read_state::read,
			.Accepted = false,
			.Excluded = true,
		};

		const auto outcome = core::carry_over(
			excluded,
			core::file_material_observation{
				.File = excluded.File, .Material = material(43U) });

		expect(
			outcome.Disposition
				== core::receipt_carry_over::reopened_material_changed,
			"the excluded file's material still moved");
		expect(
			outcome.Receipt.Excluded,
			"exclusion survives a material change");
		expect(
			outcome.Receipt.Read == core::file_read_state::unread,
			"the read progression is still reopened for an excluded file");
	}

	void test_an_observation_for_another_file_is_not_a_carry_over()
	{
		const auto receipt = core::file_receipt{
			.File     = file(24U),
			.Material = material(34U),
			.Read     = core::file_read_state::read,
		};

		const auto outcome = core::carry_over(
			receipt,
			core::file_material_observation{
				.File = file(25U), .Material = material(34U) });

		expect(
			outcome.Disposition == core::receipt_carry_over::file_absent,
			"an observation naming another file cannot retain this receipt");
		expect(
			outcome.Receipt == receipt,
			"a mismatched observation changes nothing");
	}

	// An edit changes no comment identity. A cursor that stored only identity
	// would call this conversation read while its text had been rewritten.
	void test_an_edited_comment_reopens_a_conversation()
	{
		const auto conversation = core::review_conversation_id{ 1U };
		const auto latest       = core::review_comment_id{ 7U };

		const auto caughtUp = core::conversation_receipt{
			.Conversation    = conversation,
			.SeenComment     = latest,
			.SeenEdit        = core::comment_edit_version{ 1U },
			.AcceptedComment = latest,
			.AcceptedEdit    = core::comment_edit_version{ 1U },
		};
		const auto unchanged = core::conversation_observation{
			.Conversation  = conversation,
			.LatestComment = latest,
			.LatestEdit    = core::comment_edit_version{ 1U },
		};
		expect(
			!core::unread(caughtUp, unchanged)
				&& !core::acceptance_outdated(caughtUp, unchanged),
			"a conversation with nothing new is neither unread nor outdated");

		const auto edited = core::conversation_observation{
			.Conversation  = conversation,
			.LatestComment = latest,
			.LatestEdit    = core::comment_edit_version{ 2U },
		};
		expect(
			edited.LatestComment == unchanged.LatestComment,
			"the edit changed no comment identity, which is the whole point");
		expect(
			core::unread(caughtUp, edited),
			"an edit to the seen comment reopens the conversation unread");
		expect(
			core::acceptance_outdated(caughtUp, edited),
			"an edit to the accepted comment outdates the acceptance");

		const auto added = core::conversation_observation{
			.Conversation  = conversation,
			.LatestComment = core::review_comment_id{ 8U },
			.LatestEdit    = core::comment_edit_version{ 1U },
		};
		expect(
			core::unread(caughtUp, added)
				&& core::acceptance_outdated(caughtUp, added),
			"a new comment reopens the conversation and outdates acceptance");
	}

	[[nodiscard]] auto queued(
		const std::uint8_t          aFile,
		const bool                  aDefect,
		const bool                  aVerification,
		const bool                  aOutdated,
		const std::uint32_t         aConversations,
		const core::file_read_state aRead) -> core::actionability_input
	{
		return core::actionability_input{
			.Receipt = core::file_receipt{
				.File     = file(aFile),
				.Material = material(aFile),
				.Read     = aRead,
			},
			.UnresolvedDefect     = aDefect,
			.AwaitingVerification = aVerification,
			.AcceptanceOutdated   = aOutdated,
			.UnreadConversations  = aConversations,
		};
	}

	// [[BPR-0656]]. The per-post form of unread: everything after the seen
	// post, the seen post when edited, everything when nothing was seen, and
	// never a post of my own. Mark read moves only the seen cursor.
	void test_unread_posts_follow_the_seen_cursor()
	{
		const auto conversation = core::review_conversation_id{ 5U };
		const auto posts        = std::array{
			core::conversation_post_observation{ .Comment = core::review_comment_id{ 1U }, .Edit = core::comment_edit_version{ 10U } },
			core::conversation_post_observation{ .Comment = core::review_comment_id{ 2U }, .Edit = core::comment_edit_version{ 20U }, .Mine = true },
			core::conversation_post_observation{ .Comment = core::review_comment_id{ 3U }, .Edit = core::comment_edit_version{ 30U } },
		};
		const auto never = core::conversation_receipt{ .Conversation = conversation };
		expect(core::unread_posts(never, posts) == std::vector<std::size_t>{ 0U, 2U },
			   "nothing seen: every post but my own is unread");

		const auto seenFirst = core::conversation_receipt{
			.Conversation = conversation,
			.SeenComment  = core::review_comment_id{ 1U },
			.SeenEdit     = core::comment_edit_version{ 10U },
		};
		expect(core::unread_posts(seenFirst, posts) == std::vector<std::size_t>{ 2U },
			   "the posts after the seen one are unread, my own reply excepted");

		const auto seenEdited = core::conversation_receipt{
			.Conversation = conversation,
			.SeenComment  = core::review_comment_id{ 1U },
			.SeenEdit     = core::comment_edit_version{ 9U },
		};
		expect(core::unread_posts(seenEdited, posts) == std::vector<std::size_t>{ 0U, 2U },
			   "an edit to the seen post makes it unread again");

		const auto latest = core::conversation_observation{
			.Conversation  = conversation,
			.LatestComment = core::review_comment_id{ 3U },
			.LatestEdit    = core::comment_edit_version{ 30U },
		};
		const auto read = core::mark_conversation_read(seenFirst, latest);
		expect(read.SeenComment == latest.LatestComment && read.SeenEdit == latest.LatestEdit
				   && read.AcceptedComment == seenFirst.AcceptedComment
				   && read.AcceptedEdit == seenFirst.AcceptedEdit,
			   "Mark read moves the seen cursor to the latest post and leaves acceptance");
		expect(core::unread_posts(read, posts).empty() && !core::unread(read, latest),
			   "a conversation read to its latest post has nothing unread");
		expect(core::unread_posts(read, std::span<const core::conversation_post_observation>{ }).empty(),
			   "an empty conversation has nothing unread");
	}

	void test_the_queue_order_is_deterministic_including_ties()
	{
		// Two files share the highest reason. Without an explicit tie-break
		// they would come back in whatever order the caller supplied, which
		// looks stable on one run and is not deterministic.
		const auto inputs = std::array{
			queued(60U, false, false, false, 0U, core::file_read_state::unread),
			queued(50U, true, false, false, 0U, core::file_read_state::read),
			queued(40U, false, true, false, 0U, core::file_read_state::read),
			queued(30U, true, false, false, 0U, core::file_read_state::read),
		};

		const auto ranked = core::rank_actionable(inputs, { });
		expect(ranked.size() == 4U, "every actionable file is queued");

		expect(
			ranked[0].Reason == core::actionability_reason::unresolved_defect
				&& ranked[1].Reason
					== core::actionability_reason::unresolved_defect,
			"the two defects rank above everything else");
		expect(
			ranked[0].File.bytes() < ranked[1].File.bytes(),
			"the tie between two defects is broken on file identity");
		expect(
			ranked[2].Reason
				== core::actionability_reason::awaiting_verification,
			"verification ranks below a defect");
		expect(
			ranked[3].Reason == core::actionability_reason::unread_material,
			"unread material ranks last of the actionable reasons");

		// Supplying the same set in a different order must not change it.
		const auto reversed = std::array{
			inputs[3],
			inputs[2],
			inputs[1],
			inputs[0],
		};
		const auto again = core::rank_actionable(reversed, { });
		expect(
			again == ranked,
			"the order does not depend on the order the caller supplied");
	}

	void test_the_explanation_follows_the_deciding_fact()
	{
		auto input = queued(
			70U,
			false,
			false,
			false,
			0U,
			core::file_read_state::read);
		expect(
			core::reason_for(input) == core::actionability_reason::nothing_to_do
				&& !core::actionable(core::reason_for(input)),
			"a fully read file with nothing outstanding is not actionable");

		input.UnreadConversations = 1U;
		expect(
			core::reason_for(input)
				== core::actionability_reason::unread_conversation,
			"an unread conversation is what makes it actionable");

		input.UnresolvedDefect = true;
		expect(
			core::reason_for(input)
				== core::actionability_reason::unresolved_defect,
			"the defect outranks the conversation and the reason says so");

		input.UnresolvedDefect = false;
		expect(
			core::reason_for(input)
				== core::actionability_reason::unread_conversation,
			"removing the deciding fact restores the previous reason");
	}

	void test_exclusion_leaves_the_queue_rather_than_ranking_in_it()
	{
		auto excluded = queued(
			80U,
			true,
			false,
			false,
			3U,
			core::file_read_state::unread);
		excluded.Receipt.Excluded = true;

		expect(
			core::reason_for(excluded) == core::actionability_reason::excluded,
			"exclusion outranks every actionable reason rather than competing");
		expect(
			!core::actionable(core::actionability_reason::excluded),
			"excluded is not an actionable reason");

		const auto inputs = std::array{ excluded };
		expect(
			core::rank_actionable(inputs, { }).empty(),
			"an excluded file is absent from the default queue");

		const auto shown = core::rank_actionable(inputs, { .IncludeExcluded = true });
		expect(
			shown.size() == 1U
				&& shown[0].Reason == core::actionability_reason::excluded,
			"the filter admits it without changing why it is what it is");
	}

	void test_a_filter_changes_membership_and_not_the_ranking_rule()
	{
		const auto inputs = std::array{
			queued(90U, false, true, false, 0U, core::file_read_state::read),
			queued(91U, true, false, false, 0U, core::file_read_state::read),
			queued(92U, true, false, false, 0U, core::file_read_state::read),
		};

		const auto all = core::rank_actionable(inputs, { });
		const auto defectsOnly
			= core::rank_actionable(inputs, { .OnlyDefects = true });

		expect(
			all.size() == 3U && defectsOnly.size() == 2U,
			"the filter removes the non-defect item");
		expect(
			defectsOnly[0] == all[0] && defectsOnly[1] == all[1],
			"the surviving items keep the order they had before the filter");
	}

	void test_every_enumerator_names_itself()
	{
		expect(
			core::name(core::file_read_state::unread) == "unread"
				&& core::name(core::file_read_state::seen) == "seen"
				&& core::name(core::file_read_state::read) == "read",
			"every read state names itself");
		expect(
			core::name(kind::mark_seen) == "mark_seen"
				&& core::name(kind::revoke_acceptance) == "revoke_acceptance"
				&& core::name(kind::include) == "include",
			"every command names itself");
		expect(
			core::name(core::file_receipt_rejection::material_mismatch)
					== "material_mismatch"
				&& core::name(core::file_receipt_rejection::none) == "none",
			"every rejection names itself");
	}
	/// [[BPR-1331]]. The review bar's progress button shows one mark per file,
	/// and the core decides it from the receipt the transitions produced.
	void test_one_progress_mark_sums_up_a_receipt()
	{
		using mark         = core::file_progress_mark;
		auto       receipt = receipt_for(1U, 1U);
		const auto step    = [&receipt](const kind aKind) {
			const auto outcome = core::apply(receipt, command_for(receipt, aKind));
			expect(outcome.Rejection == core::file_receipt_rejection::none, "each step of the mark walk is legal");
			receipt = outcome.Receipt;
			return core::progress_mark(receipt);
		};
		expect(core::progress_mark(receipt) == mark::unread, "a fresh receipt is unread");
		expect(step(kind::mark_seen) == mark::seen, "a seen file shows seen");
		expect(step(kind::mark_read) == mark::read, "a read file shows read");
		expect(step(kind::accept) == mark::accepted, "acceptance outranks the read progression");
		expect(step(kind::mark_unread) == mark::accepted, "an accepted file stays accepted however far it was read");
		expect(step(kind::revoke_acceptance) == mark::unread, "revoking acceptance shows the read progression again");
		expect(step(kind::exclude) == mark::excluded, "an excluded file shows excluded");
		expect(step(kind::mark_read) == mark::excluded, "reading an excluded file leaves it excluded");
		expect(step(kind::include) == mark::read, "including it again shows how far it was read");

		auto handmade     = receipt_for(2U, 2U);
		handmade.Accepted = true;
		handmade.Excluded = true;
		expect(core::progress_mark(handmade) == mark::excluded, "exclusion wins over acceptance in a receipt the core never builds");
		expect(core::name(mark::unread) == "unread" && core::name(mark::seen) == "seen" && core::name(mark::read) == "read"
				   && core::name(mark::accepted) == "accepted" && core::name(mark::excluded) == "excluded",
			   "every progress mark names itself");
	}

	/// [[BPR-1339]]. The owner, 2026-10-08: "Accepted is stronger than read or
	/// unread". A file is done once read or accepted, and acceptance alone
	/// never makes it read.
	void test_an_accepted_file_is_done_without_being_read()
	{
		auto       receipt = receipt_for(1U, 1U);
		const auto step    = [&receipt](const kind aKind) {
			const auto outcome = core::apply(receipt, command_for(receipt, aKind));
			expect(outcome.Rejection == core::file_receipt_rejection::none, "each step of the done walk is legal");
			receipt = outcome.Receipt;
			return core::progress_done(receipt);
		};
		expect(!core::progress_done(receipt), "a fresh receipt is not done");
		expect(!step(kind::mark_seen), "a seen file is not done");
		expect(step(kind::accept), "an accepted file is done though only seen");
		expect(receipt.Read == core::file_read_state::seen, "accepting a file does not mark it read");
		expect(!step(kind::revoke_acceptance), "revoking acceptance leaves a seen file not done");
		expect(step(kind::mark_read), "a read file is done");
		expect(step(kind::accept) && step(kind::mark_unread), "an accepted file stays done when marked unread");
		expect(!step(kind::revoke_acceptance), "and is not done once acceptance is revoked");
	}
} // namespace

auto main() -> int
{
	test_every_state_is_reachable();
	test_reading_and_accepting_are_independent();
	test_a_weaker_read_claim_never_moves_the_progression_back();
	test_illegal_commands_are_refused_with_their_own_reason();
	test_a_repeated_command_is_legal_and_is_not_a_change();
	test_an_epoch_advance_invalidates_only_changed_material();
	test_exclusion_survives_a_material_change();
	test_an_observation_for_another_file_is_not_a_carry_over();
	test_an_edited_comment_reopens_a_conversation();
	test_unread_posts_follow_the_seen_cursor();
	test_the_queue_order_is_deterministic_including_ties();
	test_the_explanation_follows_the_deciding_fact();
	test_exclusion_leaves_the_queue_rather_than_ranking_in_it();
	test_a_filter_changes_membership_and_not_the_ranking_rule();
	test_every_enumerator_names_itself();
	test_one_progress_mark_sums_up_a_receipt();
	test_an_accepted_file_is_done_without_being_read();

	std::cout << "review-ledger-core-tests:pass\n";
	return EXIT_SUCCESS;
}
