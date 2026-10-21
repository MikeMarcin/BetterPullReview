#include "bpr/core/review_ledger.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>

namespace bpr::core {
	namespace {
		/// \brief Advances a read progression without ever moving it backwards.
		///
		/// \details `seen` after `read` is not a regression to report; it is a
		/// weaker statement about a file already known to be read, so it is
		/// absorbed. Only an explicit `mark_unread` moves the progression back.
		[[nodiscard]] constexpr auto advanced(
			const file_read_state aCurrent,
			const file_read_state aOffered) noexcept -> file_read_state
		{
			return aOffered > aCurrent ? aOffered : aCurrent;
		}

		/// \brief The index of the first post newer than the seen cursor.
		///
		/// \details The post after the seen one, or the seen one itself when
		/// it was edited since; the first post when the cursor names none.
		[[nodiscard]] auto first_unseen(
			const conversation_receipt&                          aReceipt,
			const std::span<const conversation_post_observation> aPosts) noexcept -> std::size_t
		{
			for (std::size_t index = 0U; index < aPosts.size(); ++index) {
				if (aPosts[index].Comment == aReceipt.SeenComment) {
					return aPosts[index].Edit == aReceipt.SeenEdit ? index + 1U : index;
				}
			}
			return 0U;
		}

		/// \brief One conversation's share of a bulk read.
		struct scoped_read
		{
			std::optional<conversation_receipt> Receipt;
			std::uint32_t                       Marked = 0U;
			std::uint32_t                       Kept   = 0U;
		};

		[[nodiscard]] auto in_scope(const conversation_post_observation& aPost, const conversation_read_scope aScope) noexcept -> bool
		{
			return !aPost.Mine && (aScope.Author == review_author_id{ } || aPost.Author == aScope.Author);
		}

		/// \brief Moves one conversation's cursor over the new posts in scope.
		///
		/// \details The cursor passes my posts and the posts in scope, and
		/// stops at the first new post someone else wrote: everything past it
		/// stays new, the scope's own posts included, and is counted kept.
		[[nodiscard]] auto read_in_scope(
			const conversation_receipt&                          aReceipt,
			const std::span<const conversation_post_observation> aPosts,
			const conversation_read_scope                        aScope) noexcept -> scoped_read
		{
			auto result = scoped_read{ };
			auto index  = first_unseen(aReceipt, aPosts);
			auto last   = aPosts.size();
			for (; index < aPosts.size(); ++index) {
				const auto& post = aPosts[index];
				if (!post.Mine && !in_scope(post, aScope)) {
					break;
				}
				result.Marked += post.Mine ? 0U : 1U;
				last = index;
			}
			for (; index < aPosts.size(); ++index) {
				result.Kept += in_scope(aPosts[index], aScope) ? 1U : 0U;
			}
			if (result.Marked == 0U || last == aPosts.size()) {
				return result;
			}
			auto read        = aReceipt;
			read.SeenComment = aPosts[last].Comment;
			read.SeenEdit    = aPosts[last].Edit;
			result.Receipt   = read;
			return result;
		}
	} // namespace

	auto review_author_of(const std::string_view aLogin) noexcept -> review_author_id
	{
		if (aLogin.empty()) {
			return review_author_id{ };
		}
		// A login is at most 39 characters and an app's a little more under
		// its "[bot]"; folding into a bounded buffer keeps this noexcept.
		constexpr auto bound  = std::size_t{ 256U };
		auto           folded = std::array<std::uint8_t, bound>{ };
		const auto     length = std::min(aLogin.size(), bound);
		for (std::size_t index = 0U; index < length; ++index) {
			const auto character = static_cast<std::uint8_t>(aLogin[index]);
			folded.at(index)     = character >= 'A' && character <= 'Z' ? static_cast<std::uint8_t>(character - 'A' + 'a') : character;
		}
		const auto digest = sha256(std::span{ folded.data(), length });
		auto       value  = std::uint64_t{ 0U };
		for (std::size_t index = 0U; index < 8U; ++index) {
			value = (value << 8U) | digest.at(index);
		}
		value &= std::uint64_t{ 0x7fff'ffff'ffff'ffffULL };
		return review_author_id{ value == 0U ? 1U : value };
	}

	auto name(const file_read_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case file_read_state::unread:
			return "unread";
		case file_read_state::seen:
			return "seen";
		case file_read_state::read:
			return "read";
		}
		return "unread";
	}

	auto name(const file_receipt_command_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case file_receipt_command_kind::mark_seen:
			return "mark_seen";
		case file_receipt_command_kind::mark_read:
			return "mark_read";
		case file_receipt_command_kind::mark_unread:
			return "mark_unread";
		case file_receipt_command_kind::accept:
			return "accept";
		case file_receipt_command_kind::revoke_acceptance:
			return "revoke_acceptance";
		case file_receipt_command_kind::exclude:
			return "exclude";
		case file_receipt_command_kind::include:
			return "include";
		}
		return "mark_seen";
	}

	auto name(const file_receipt_rejection aRejection) noexcept -> std::string_view
	{
		switch (aRejection) {
		case file_receipt_rejection::none:
			return "none";
		case file_receipt_rejection::unknown_file:
			return "unknown_file";
		case file_receipt_rejection::material_mismatch:
			return "material_mismatch";
		case file_receipt_rejection::excluded_file_cannot_be_accepted:
			return "excluded_file_cannot_be_accepted";
		case file_receipt_rejection::accepted_file_cannot_be_excluded:
			return "accepted_file_cannot_be_excluded";
		}
		return "none";
	}

	auto apply(
		const file_receipt&         aReceipt,
		const file_receipt_command& aCommand) noexcept -> file_receipt_outcome
	{
		const auto refuse = [&aReceipt](const file_receipt_rejection aRejection) {
			return file_receipt_outcome{
				.Receipt   = aReceipt,
				.Rejection = aRejection,
				.Changed   = false,
			};
		};

		// Identity is checked before material, because a command aimed at
		// another file is a different mistake from one aimed at material that
		// has been replaced, and the two are repaired differently.
		if (aCommand.File != aReceipt.File) {
			return refuse(file_receipt_rejection::unknown_file);
		}
		if (aCommand.Material != aReceipt.Material) {
			return refuse(file_receipt_rejection::material_mismatch);
		}

		auto next = aReceipt;
		switch (aCommand.Kind) {
		case file_receipt_command_kind::mark_seen:
			next.Read = advanced(next.Read, file_read_state::seen);
			break;
		case file_receipt_command_kind::mark_read:
			next.Read = advanced(next.Read, file_read_state::read);
			break;
		case file_receipt_command_kind::mark_unread:
			next.Read = file_read_state::unread;
			break;
		case file_receipt_command_kind::accept:
			if (aReceipt.Excluded) {
				return refuse(
					file_receipt_rejection::excluded_file_cannot_be_accepted);
			}
			next.Accepted = true;
			break;
		case file_receipt_command_kind::revoke_acceptance:
			next.Accepted = false;
			break;
		case file_receipt_command_kind::exclude:
			if (aReceipt.Accepted) {
				return refuse(
					file_receipt_rejection::accepted_file_cannot_be_excluded);
			}
			next.Excluded = true;
			break;
		case file_receipt_command_kind::include:
			next.Excluded = false;
			break;
		}

		return file_receipt_outcome{
			.Receipt   = next,
			.Rejection = file_receipt_rejection::none,
			.Changed   = next != aReceipt,
		};
	}

	auto name(const receipt_carry_over aDisposition) noexcept -> std::string_view
	{
		switch (aDisposition) {
		case receipt_carry_over::retained:
			return "retained";
		case receipt_carry_over::reopened_material_changed:
			return "reopened_material_changed";
		case receipt_carry_over::file_absent:
			return "file_absent";
		}
		return "retained";
	}

	auto carry_over(
		const file_receipt&              aReceipt,
		const file_material_observation& aObservation) noexcept
		-> receipt_carry_over_outcome
	{
		if (aObservation.File != aReceipt.File) {
			return carry_over_absent(aReceipt);
		}
		if (aObservation.Material == aReceipt.Material) {
			return receipt_carry_over_outcome{
				.Receipt     = aReceipt,
				.Disposition = receipt_carry_over::retained,
			};
		}

		return receipt_carry_over_outcome{
			.Receipt = file_receipt{
				.File     = aReceipt.File,
				.Material = aObservation.Material,
				.Read     = file_read_state::unread,
				.Accepted = false,
				.Excluded = aReceipt.Excluded,
			},
			.Disposition = receipt_carry_over::reopened_material_changed,
		};
	}

	auto carry_over_absent(const file_receipt& aReceipt) noexcept
		-> receipt_carry_over_outcome
	{
		return receipt_carry_over_outcome{
			.Receipt     = aReceipt,
			.Disposition = receipt_carry_over::file_absent,
		};
	}

	auto name(const actionability_reason aReason) noexcept -> std::string_view
	{
		switch (aReason) {
		case actionability_reason::unresolved_defect:
			return "unresolved_defect";
		case actionability_reason::awaiting_verification:
			return "awaiting_verification";
		case actionability_reason::acceptance_outdated:
			return "acceptance_outdated";
		case actionability_reason::unread_conversation:
			return "unread_conversation";
		case actionability_reason::unread_material:
			return "unread_material";
		case actionability_reason::nothing_to_do:
			return "nothing_to_do";
		case actionability_reason::excluded:
			return "excluded";
		}
		return "nothing_to_do";
	}

	auto reason_for(const actionability_input& aInput) noexcept
		-> actionability_reason
	{
		// Exclusion is checked first and is not a priority: an excluded file is
		// out of the queue whatever else is true of it. Ordering it among the
		// actionable reasons would put it in the queue it exists to leave.
		if (aInput.Receipt.Excluded) {
			return actionability_reason::excluded;
		}
		if (aInput.UnresolvedDefect) {
			return actionability_reason::unresolved_defect;
		}
		if (aInput.AwaitingVerification) {
			return actionability_reason::awaiting_verification;
		}
		if (aInput.AcceptanceOutdated) {
			return actionability_reason::acceptance_outdated;
		}
		if (aInput.UnreadConversations != 0U) {
			return actionability_reason::unread_conversation;
		}
		if (aInput.Receipt.Read != file_read_state::read) {
			return actionability_reason::unread_material;
		}
		return actionability_reason::nothing_to_do;
	}

	auto rank_actionable(
		const std::span<const actionability_input> aInputs,
		const actionability_filters                aFilters)
		-> std::vector<actionable_item>
	{
		auto ranked = std::vector<actionable_item>{ };
		ranked.reserve(aInputs.size());

		for (const auto& input : aInputs) {
			const auto reason = reason_for(input);
			if (reason == actionability_reason::excluded
				&& !aFilters.IncludeExcluded) {
				continue;
			}
			if (!actionable(reason)
				&& reason != actionability_reason::excluded) {
				continue;
			}
			if (aFilters.OnlyDefects
				&& reason != actionability_reason::unresolved_defect) {
				continue;
			}

			ranked.push_back(actionable_item{
				.File     = input.Receipt.File,
				.Material = input.Receipt.Material,
				.Reason   = reason,
			});
		}

		// Reason first, then file identity. The identity tie-break is what
		// makes the order deterministic rather than merely repeatable on one
		// machine: without it, items sharing a reason keep the caller's order.
		std::ranges::stable_sort(
			ranked,
			[](const actionable_item& aLeft, const actionable_item& aRight) {
				if (aLeft.Reason != aRight.Reason) {
					return aLeft.Reason < aRight.Reason;
				}
				return aLeft.File.bytes() < aRight.File.bytes();
			});

		return ranked;
	}

	auto unread(
		const conversation_receipt&     aReceipt,
		const conversation_observation& aObservation) noexcept -> bool
	{
		return aObservation.LatestComment != aReceipt.SeenComment
			|| aObservation.LatestEdit != aReceipt.SeenEdit;
	}

	auto unread_posts(
		const conversation_receipt&                          aReceipt,
		const std::span<const conversation_post_observation> aPosts)
		-> std::vector<std::size_t>
	{
		auto result = std::vector<std::size_t>{ };
		for (auto index = first_unseen(aReceipt, aPosts); index < aPosts.size(); ++index) {
			if (!aPosts[index].Mine) {
				result.push_back(index);
			}
		}
		return result;
	}

	auto mark_conversation_read(
		const conversation_receipt&     aReceipt,
		const conversation_observation& aObservation) noexcept
		-> conversation_receipt
	{
		auto read         = aReceipt;
		read.Conversation = aObservation.Conversation;
		read.SeenComment  = aObservation.LatestComment;
		read.SeenEdit     = aObservation.LatestEdit;
		return read;
	}

	auto mark_conversations_read(
		const std::span<const conversation_receipt>            aReceipts,
		const std::span<const conversation_thread_observation> aThreads,
		const conversation_read_scope                          aScope)
		-> conversation_bulk_read
	{
		auto result = conversation_bulk_read{ };
		// The decisions so far, by conversation: a conversation observed
		// twice is decided against its first decision, not twice against
		// the stored receipt.
		auto& decided = result.Receipts;
		for (const auto& thread : aThreads) {
			const auto prior   = std::ranges::lower_bound(decided, thread.Conversation, { }, &conversation_receipt::Conversation);
			const auto revisit = prior != decided.end() && prior->Conversation == thread.Conversation;
			auto       receipt = conversation_receipt{ .Conversation = thread.Conversation };
			if (revisit) {
				receipt = *prior;
			} else if (const auto stored = std::ranges::lower_bound(aReceipts, thread.Conversation, { }, &conversation_receipt::Conversation);
					   stored != aReceipts.end() && stored->Conversation == thread.Conversation) {
				receipt = *stored;
			}
			const auto read = read_in_scope(receipt, thread.Posts, aScope);
			result.MarkedPosts += read.Marked;
			result.KeptPosts += read.Kept;
			if (!read.Receipt.has_value() || *read.Receipt == receipt) {
				continue;
			}
			if (revisit) {
				*prior = *read.Receipt;
			} else {
				decided.insert(prior, *read.Receipt);
			}
		}
		return result;
	}

	auto mark_conversation_unread(
		const conversation_receipt&                          aReceipt,
		const std::span<const conversation_post_observation> aPosts)
		-> std::optional<conversation_receipt>
	{
		for (auto index = aPosts.size(); index-- > 0U;) {
			if (aPosts[index].Mine) {
				continue;
			}
			auto unread = aReceipt;
			if (index == 0U) {
				unread.SeenComment = review_comment_id{ };
				unread.SeenEdit    = comment_edit_version{ };
			} else {
				unread.SeenComment = aPosts[index - 1U].Comment;
				unread.SeenEdit    = aPosts[index - 1U].Edit;
			}
			return unread;
		}
		return std::nullopt;
	}

	auto acceptance_outdated(
		const conversation_receipt&     aReceipt,
		const conversation_observation& aObservation) noexcept -> bool
	{
		return aObservation.LatestComment != aReceipt.AcceptedComment
			|| aObservation.LatestEdit != aReceipt.AcceptedEdit;
	}
} // namespace bpr::core
