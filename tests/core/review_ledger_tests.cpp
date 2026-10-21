#include "bpr/core/file_progress_mark.hpp"
#include "bpr/core/review_ledger.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
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

	// [[BPR-1582]]. Mark unread moves the seen cursor back before the latest
	// post someone else wrote: that post is new again, my own never is, and
	// a conversation of only my posts offers nothing to unread.
	void test_mark_unread_makes_the_latest_other_post_new()
	{
		const auto conversation = core::review_conversation_id{ 6U };
		const auto posts        = std::array{
			core::conversation_post_observation{ .Comment = core::review_comment_id{ 1U }, .Edit = core::comment_edit_version{ 10U } },
			core::conversation_post_observation{ .Comment = core::review_comment_id{ 2U }, .Edit = core::comment_edit_version{ 20U } },
			core::conversation_post_observation{ .Comment = core::review_comment_id{ 3U }, .Edit = core::comment_edit_version{ 30U }, .Mine = true },
		};
		const auto read = core::conversation_receipt{
			.Conversation    = conversation,
			.SeenComment     = core::review_comment_id{ 3U },
			.SeenEdit        = core::comment_edit_version{ 30U },
			.AcceptedComment = core::review_comment_id{ 2U },
			.AcceptedEdit    = core::comment_edit_version{ 20U },
		};
		expect(core::unread_posts(read, posts).empty(), "the fixture starts read");
		const auto unread = core::mark_conversation_unread(read, posts);
		expect(unread.has_value() && unread->SeenComment == core::review_comment_id{ 1U } && unread->SeenEdit == core::comment_edit_version{ 10U }
				   && unread->AcceptedComment == read.AcceptedComment && unread->AcceptedEdit == read.AcceptedEdit && unread->Conversation == conversation,
			   "the seen cursor moves to the post before the latest one by someone else, and acceptance stays");
		expect(core::unread_posts(*unread, posts) == std::vector<std::size_t>{ 1U }, "that post is new again; my own reply after it is not");
		const auto root = std::array{ posts[0], posts[2] };
		const auto none = core::mark_conversation_unread(read, root);
		expect(none.has_value() && none->SeenComment == core::review_comment_id{ } && core::unread_posts(*none, root) == std::vector<std::size_t>{ 0U },
			   "when the latest other post is the first, nothing is seen and it is new");
		const auto mine = std::array{ posts[2] };
		expect(!core::mark_conversation_unread(read, mine).has_value(), "a conversation of only my posts has nothing to make new");
		expect(!core::mark_conversation_unread(read, std::span<const core::conversation_post_observation>{ }).has_value(), "nor has an empty one");
		expect(core::conversation_read_action_for(true, true) == core::conversation_read_action::mark_read
				   && core::conversation_read_action_for(true, false) == core::conversation_read_action::mark_read
				   && core::conversation_read_action_for(false, true) == core::conversation_read_action::mark_unread
				   && core::conversation_read_action_for(false, false) == core::conversation_read_action::none,
			   "a thread with something new offers Mark as read; a read one Mark as unread, unless every post is mine");
		// Negative control: Mark read after Mark unread reads it again.
		const auto latest = core::conversation_observation{ .Conversation = conversation, .LatestComment = core::review_comment_id{ 3U }, .LatestEdit = core::comment_edit_version{ 30U } };
		expect(core::unread_posts(core::mark_conversation_read(*unread, latest), posts).empty(), "Mark read undoes it");
	}

	// [[BPR-1601]]. PR comments and review summaries use singleton conversations.
	void test_single_post_conversation_read_state()
	{
		const auto id      = core::review_conversation_id{ 71U };
		auto       posts   = std::array{ core::conversation_post_observation{ .Comment = core::review_comment_id{ 81U }, .Edit = core::comment_edit_version{ 91U } } };
		const auto initial = core::conversation_receipt{ .Conversation = id };
		expect(core::unread_posts(initial, posts) == std::vector<std::size_t>{ 0U }, "an unseen singleton has one unread post");
		const auto read = core::mark_conversation_read(initial, { .Conversation = id, .LatestComment = posts[0].Comment, .LatestEdit = posts[0].Edit });
		expect(core::unread_posts(read, posts).empty(), "reading a singleton advances its cursor");
		const auto unread = core::mark_conversation_unread(read, posts);
		expect(unread.has_value() && unread->Conversation == id && unread->SeenComment == core::review_comment_id{ } && core::unread_posts(*unread, posts).size() == 1U, "manual unread retains identity while moving before the only post");
		posts[0].Edit = core::comment_edit_version{ 92U };
		expect(core::unread_posts(read, posts).size() == 1U, "editing the same stable comment makes it new again");
		posts[0].Mine = true;
		expect(core::unread_posts(initial, posts).empty() && !core::mark_conversation_unread(read, posts), "my own PR comments and summaries are never new");
	}

	[[nodiscard]] auto post(const std::uint64_t aComment, const core::review_author_id aAuthor, const bool aMine = false)
		-> core::conversation_post_observation
	{
		return { .Comment = core::review_comment_id{ aComment }, .Edit = core::comment_edit_version{ aComment * 10U }, .Mine = aMine, .Author = aAuthor };
	}

	[[nodiscard]] auto unread_after(const core::conversation_bulk_read& aRead, const std::span<const core::conversation_receipt> aBefore,
									const core::conversation_thread_observation& aThread) -> std::vector<std::size_t>
	{
		const auto found = std::ranges::find(aRead.Receipts, aThread.Conversation, &core::conversation_receipt::Conversation);
		if (found != aRead.Receipts.end()) {
			return core::unread_posts(*found, aThread.Posts);
		}
		const auto stored = std::ranges::find(aBefore, aThread.Conversation, &core::conversation_receipt::Conversation);
		return core::unread_posts(stored != aBefore.end() ? *stored : core::conversation_receipt{ .Conversation = aThread.Conversation }, aThread.Posts);
	}

	// [[BPR-PENDING-M]]. A bot's login is one author whatever its case, and
	// is not the same-named person; an empty login is nobody.
	void test_an_author_is_the_login_without_case()
	{
		const auto bot = core::review_author_of("coderabbitai[bot]");
		expect(bot != core::review_author_id{ } && bot == core::review_author_of("CodeRabbitAI[bot]"), "a bot login names one author in any case");
		expect(bot != core::review_author_of("coderabbitai") && bot != core::review_author_of("dependabot[bot]"), "another account is another author");
		expect(core::review_author_of("") == core::review_author_id{ }, "no login names nobody");
		expect(core::review_author_of("octocat") == core::review_author_of("octocat"), "the identity is a pure function of the login");
	}

	// [[BPR-PENDING-M]]. Mark all comments as read: one decision over every
	// conversation; only the receipts that move are written, acceptance
	// stays, and a second mark changes nothing.
	void test_mark_all_read_reads_every_conversation_once()
	{
		const auto bot     = core::review_author_of("coderabbitai[bot]");
		const auto human   = core::review_author_of("tbutler");
		const auto me      = core::review_author_of("reviewer-one");
		const auto threads = std::vector<core::conversation_thread_observation>{
			{ .Conversation = core::review_conversation_id{ 30U }, .Posts = { post(1U, bot), post(2U, human), post(3U, me, true) } },
			{ .Conversation = core::review_conversation_id{ 10U }, .Posts = { post(4U, human) } },
			{ .Conversation = core::review_conversation_id{ 20U }, .Posts = { post(5U, bot), post(6U, bot) } },
			{ .Conversation = core::review_conversation_id{ 40U }, .Posts = { post(7U, me, true) } },
		};
		const auto stored = std::vector<core::conversation_receipt>{
			{ .Conversation = core::review_conversation_id{ 10U }, .SeenComment = core::review_comment_id{ 4U }, .SeenEdit = core::comment_edit_version{ 40U } },
			{ .Conversation = core::review_conversation_id{ 20U }, .SeenComment = core::review_comment_id{ 5U }, .SeenEdit = core::comment_edit_version{ 50U }, .AcceptedComment = core::review_comment_id{ 5U }, .AcceptedEdit = core::comment_edit_version{ 50U } },
		};
		const auto read = core::mark_conversations_read(stored, threads, { });
		expect(read.MarkedPosts == 3U && read.KeptPosts == 0U, "every new post someone else wrote is marked, none kept");
		expect(read.Receipts.size() == 2U && read.Receipts[0].Conversation == core::review_conversation_id{ 20U }
				   && read.Receipts[1].Conversation == core::review_conversation_id{ 30U },
			   "only the conversations with something new change, in conversation order");
		expect(read.Receipts[0].SeenComment == core::review_comment_id{ 6U } && read.Receipts[0].AcceptedComment == core::review_comment_id{ 5U }
				   && read.Receipts[1].SeenComment == core::review_comment_id{ 3U } && read.Receipts[1].SeenEdit == core::comment_edit_version{ 30U },
			   "each is read to its latest post, and acceptance does not move");
		for (const auto& thread : threads) {
			expect(unread_after(read, stored, thread).empty(), "nothing in the pull request is new afterwards");
		}
		auto after = stored;
		for (const auto& receipt : read.Receipts) {
			std::erase_if(after, [&receipt](const core::conversation_receipt& aKept) { return aKept.Conversation == receipt.Conversation; });
			after.push_back(receipt);
		}
		std::ranges::sort(after, { }, &core::conversation_receipt::Conversation);
		const auto again = core::mark_conversations_read(after, threads, { });
		expect(again.Receipts.empty() && again.MarkedPosts == 0U && again.KeptPosts == 0U, "marking again is legal and changes nothing");
		expect(core::mark_conversations_read(after, { }, { }).Receipts.empty(), "a pull request with no conversations has nothing to mark");

		// What arrives later is new: the mark is of the posts observed then.
		auto later = threads;
		later[2].Posts.push_back(post(8U, bot));
		later[1].Posts[0].Edit = core::comment_edit_version{ 41U };
		expect(unread_after(again, after, later[2]) == std::vector<std::size_t>{ 2U }, "a reply posted after the mark is new");
		expect(unread_after(again, after, later[1]) == std::vector<std::size_t>{ 0U }, "a post edited after the mark is new");
	}

	// [[BPR-PENDING-M]]. Mark all from one author: their new posts are read,
	// nobody else's are, and the cursor never passes someone else's new post.
	void test_mark_author_read_reads_only_that_author()
	{
		const auto bot     = core::review_author_of("coderabbitai[bot]");
		const auto human   = core::review_author_of("tbutler");
		const auto me      = core::review_author_of("reviewer-one");
		const auto threads = std::vector<core::conversation_thread_observation>{
			// A bot's PR comment, review summary or thread of its own.
			{ .Conversation = core::review_conversation_id{ 1U }, .Posts = { post(11U, bot) } },
			{ .Conversation = core::review_conversation_id{ 2U }, .Posts = { post(21U, bot), post(22U, me, true), post(23U, bot) } },
			// Someone answered the bot: the answer stays new.
			{ .Conversation = core::review_conversation_id{ 3U }, .Posts = { post(31U, bot), post(32U, human) } },
			// The bot answered someone: their post is first, so nothing moves.
			{ .Conversation = core::review_conversation_id{ 4U }, .Posts = { post(41U, human), post(42U, bot) } },
			// Interleaved: the bot's root is read, the answer and its reply stay new.
			{ .Conversation = core::review_conversation_id{ 5U }, .Posts = { post(51U, bot), post(52U, human), post(53U, bot) } },
			// A person's own thread, and a post with no author.
			{ .Conversation = core::review_conversation_id{ 6U }, .Posts = { post(61U, human) } },
			{ .Conversation = core::review_conversation_id{ 7U }, .Posts = { post(71U, core::review_author_id{ }) } },
		};
		const auto none = std::vector<core::conversation_receipt>{ };
		const auto read = core::mark_conversations_read(none, threads, { .Author = bot });
		expect(read.MarkedPosts == 5U && read.KeptPosts == 2U, "the bot's five reachable posts are read; two behind a new answer stay new");
		expect(read.Receipts.size() == 4U, "the four conversations whose cursor moves are the only ones written");
		const auto unread = [&](const std::size_t aThread) { return unread_after(read, none, threads[aThread]); };
		expect(unread(0U).empty() && unread(1U).empty(), "a conversation of the bot's posts, my replies among them, is read");
		expect(unread(2U) == std::vector<std::size_t>{ 1U }, "a person's answer to the bot stays new");
		expect(unread(3U) == std::vector<std::size_t>{ 0U, 1U }, "a bot reply after a person's new post stays new with it");
		expect(unread(4U) == std::vector<std::size_t>{ 1U, 2U }, "interleaved: the root is read, the rest stays new");
		expect(unread(5U) == std::vector<std::size_t>{ 0U } && unread(6U) == std::vector<std::size_t>{ 0U }, "nobody else's conversation is touched");

		const auto person = core::mark_conversations_read(none, threads, { .Author = human });
		expect(person.MarkedPosts == 2U && person.Receipts.size() == 2U
				   && person.Receipts[0].Conversation == core::review_conversation_id{ 4U }
				   && person.Receipts[0].SeenComment == core::review_comment_id{ 41U },
			   "the same rule for a person: their own thread and their post ahead of the bot's");
		expect(core::mark_conversations_read(none, threads, { .Author = me }).Receipts.empty(), "my own posts are never new, so marking mine changes nothing");

		// Idempotence over the stored result, then a later post by the bot.
		auto       stored = read.Receipts;
		const auto again  = core::mark_conversations_read(stored, threads, { .Author = bot });
		expect(again.Receipts.empty() && again.MarkedPosts == 0U && again.KeptPosts == 2U, "marking the author again changes nothing and still counts the kept posts");
		auto later = threads[0];
		later.Posts.push_back(post(12U, bot));
		expect(unread_after(again, stored, later) == std::vector<std::size_t>{ 1U }, "the author's next post is new: the mark is one-shot, not a mute");

		// A conversation observed twice is decided once.
		const auto twice = std::vector{ threads[0], threads[0] };
		const auto once  = core::mark_conversations_read(none, twice, { .Author = bot });
		expect(once.Receipts.size() == 1U && once.MarkedPosts == 1U, "a repeated observation adds no receipt and no post");
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
	test_mark_unread_makes_the_latest_other_post_new();
	test_single_post_conversation_read_state();
	test_an_author_is_the_login_without_case();
	test_mark_all_read_reads_every_conversation_once();
	test_mark_author_read_reads_only_that_author();
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
