#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_LEDGER_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_LEDGER_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/core/model.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace bpr::core {
	struct review_conversation_id_tag;
	struct review_comment_id_tag;

	using review_conversation_id = strong_id<review_conversation_id_tag>;
	using review_comment_id      = strong_id<review_comment_id_tag>;

	/// \brief Counts in-place edits of one comment.
	///
	/// \details An edit changes no identity, so identity alone cannot tell a
	/// reader that the text they accepted has since been rewritten. The version
	/// is what makes that decidable.
	using comment_edit_version = strong_id<struct comment_edit_version_tag>;

	/// \brief Identifies the material a file receipt is attached to.
	///
	/// \details A receipt attaches to material and never to a path or an epoch.
	/// A path is renamed and an epoch advances while the bytes stay the same;
	/// conversely the same path under a new epoch may be entirely different
	/// content. Only the material answers "is this the thing I already read".
	///
	/// This is RFC-BPR-0004's comparison digest rather than a second identity
	/// invented here, because the compared material is exactly what the reader
	/// inspected.
	using file_material_id = file_comparison_digest;

	/// \brief Names how far a reader has got through one file's material.
	///
	/// \details The three values are a progression and not a set of flags:
	/// a file cannot be read without having been seen. Acceptance and exclusion
	/// are deliberately *not* in this enumeration, because they are independent
	/// facts about the same material — accepting a file says nothing about
	/// whether its text was read, and marking it read is not approval.
	enum class file_read_state : std::uint8_t {
		unread,
		seen,
		read,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(file_read_state aState) noexcept -> std::string_view;

	/// \brief Holds one reader's private progress against one file's material.
	struct file_receipt
	{
		file_identity    File;
		file_material_id Material;
		file_read_state  Read     = file_read_state::unread;
		bool             Accepted = false;
		bool             Excluded = false;

		auto operator<=>(const file_receipt&) const = default;
	};

	/// \brief Names one command a reader can issue against a file receipt.
	enum class file_receipt_command_kind : std::uint8_t {
		mark_seen,
		mark_read,
		mark_unread,
		accept,
		revoke_acceptance,
		exclude,
		include,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(file_receipt_command_kind aKind) noexcept
		-> std::string_view;

	/// \brief Carries one receipt command and the material it was issued against.
	///
	/// \details The material travels with the command so that a command formed
	/// against material the reader was looking at cannot be applied to material
	/// that has since replaced it. Without it a slow command would silently
	/// mark the wrong bytes read.
	struct file_receipt_command
	{
		file_identity             File;
		file_material_id          Material;
		file_receipt_command_kind Kind = file_receipt_command_kind::mark_seen;

		auto operator<=>(const file_receipt_command&) const = default;
	};

	/// \brief Names why a receipt command was refused.
	enum class file_receipt_rejection : std::uint8_t {
		none,
		unknown_file,
		material_mismatch,
		excluded_file_cannot_be_accepted,
		accepted_file_cannot_be_excluded,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(file_receipt_rejection aRejection) noexcept
		-> std::string_view;

	/// \brief Reports what one receipt command did.
	///
	/// \details `Changed` distinguishes a command that moved the receipt from
	/// one that was legal and had nothing to do. Re-issuing a command is not an
	/// error and must not be reported as one, but it must also not be reported
	/// as a change, because a change is what the host persists and publishes.
	struct file_receipt_outcome
	{
		file_receipt           Receipt;
		file_receipt_rejection Rejection = file_receipt_rejection::none;
		bool                   Changed   = false;

		auto operator<=>(const file_receipt_outcome&) const = default;
	};

	/// \brief Applies one command to one receipt.
	///
	/// \details This is the whole of the file-receipt policy. It reads no
	/// storage, holds no ledger, and reaches nothing outside its arguments.
	[[nodiscard]] auto apply(
		const file_receipt&         aReceipt,
		const file_receipt_command& aCommand) noexcept -> file_receipt_outcome;

	/// \brief Describes what the host observed about one file under a new epoch.
	///
	/// \details The host constructs this bounded record from the advanced
	/// comparison. The core never reads the epoch, the repository, or the
	/// ledger to obtain it.
	struct file_material_observation
	{
		file_identity    File;
		file_material_id Material;

		auto operator<=>(const file_material_observation&) const = default;
	};

	/// \brief Names what happened to one receipt when an epoch advanced.
	enum class receipt_carry_over : std::uint8_t {
		retained,
		reopened_material_changed,
		file_absent,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(receipt_carry_over aDisposition) noexcept
		-> std::string_view;

	/// \brief Reports one receipt's fate across an epoch advance.
	struct receipt_carry_over_outcome
	{
		file_receipt       Receipt;
		receipt_carry_over Disposition = receipt_carry_over::retained;

		auto operator<=>(const receipt_carry_over_outcome&) const = default;
	};

	/// \brief Decides whether one receipt survives an epoch advance.
	///
	/// \details A receipt survives exactly when the compared material identity
	/// is unchanged. When the material moved, the reader has not inspected what
	/// is now there, so the read progression and the acceptance are both
	/// reopened and the receipt re-attaches to the new material.
	///
	/// Exclusion deliberately survives. Excluding a file is a standing
	/// statement about that file — a generated artefact stays uninteresting
	/// when it is regenerated — rather than a claim about particular bytes.
	/// Reopening it on every advance would make the setting useless precisely
	/// for the files it exists for.
	[[nodiscard]] auto carry_over(
		const file_receipt&              aReceipt,
		const file_material_observation& aObservation) noexcept
		-> receipt_carry_over_outcome;

	/// \brief Decides one receipt's fate when the file is no longer compared.
	///
	/// \details A file absent from the advanced comparison is not a changed
	/// file, and its receipt is neither retained nor reopened. Reporting it as
	/// retained would leave a receipt attached to material nothing compares.
	[[nodiscard]] auto carry_over_absent(const file_receipt& aReceipt) noexcept
		-> receipt_carry_over_outcome;

	/// \brief Holds one reader's private progress through one conversation.
	///
	/// \details Both cursors record an identity *and* an edit version, because
	/// either can move independently: a new comment changes the identity, and
	/// an edit to the latest comment changes only the version.
	struct conversation_receipt
	{
		review_conversation_id Conversation;
		review_comment_id      SeenComment;
		comment_edit_version   SeenEdit;
		review_comment_id      AcceptedComment;
		comment_edit_version   AcceptedEdit;

		auto operator<=>(const conversation_receipt&) const = default;
	};

	/// \brief Describes what the host observed about one conversation.
	///
	/// \details The host constructs this bounded record; the core never asks
	/// for the conversation itself.
	struct conversation_observation
	{
		review_conversation_id Conversation;
		review_comment_id      LatestComment;
		comment_edit_version   LatestEdit;

		auto operator<=>(const conversation_observation&) const = default;
	};

	/// \brief Reports whether a conversation has unread material.
	///
	/// \details True when the latest comment is not the one seen, or when the
	/// comment seen has been edited since. The second case is the one an
	/// identity-only cursor gets wrong.
	[[nodiscard]] auto unread(
		const conversation_receipt&     aReceipt,
		const conversation_observation& aObservation) noexcept -> bool;

	/// \brief One post of a conversation as the host observed it.
	///
	/// \details [[BPR-0656]]. The host supplies the posts in thread order,
	/// root first; the core never sees the text. `Mine` names a post the
	/// reader wrote, which is never unread to them.
	struct conversation_post_observation
	{
		review_comment_id    Comment;
		comment_edit_version Edit;
		bool                 Mine = false;

		auto operator<=>(const conversation_post_observation&) const = default;
	};

	/// \brief Decides which posts of a conversation are newer than the cursor.
	///
	/// \details [[BPR-0656]]. The indices, in thread order, of the posts the
	/// reader has not seen: every post after the seen one, the seen one
	/// itself when it was edited since, and every post when nothing was
	/// seen. A post the reader wrote is skipped. This is the per-post form
	/// of `unread`: the conversation is unread exactly when this is nonempty
	/// or the seen post is someone else's edited one.
	[[nodiscard]] auto unread_posts(
		const conversation_receipt&                    aReceipt,
		std::span<const conversation_post_observation> aPosts)
		-> std::vector<std::size_t>;

	/// \brief The receipt that has read a conversation up to its latest post.
	///
	/// \details [[BPR-0656]]. Mark read moves the seen cursor to the latest
	/// post and its edit; acceptance is a different statement and is left
	/// where it was.
	[[nodiscard]] auto mark_conversation_read(
		const conversation_receipt&     aReceipt,
		const conversation_observation& aObservation) noexcept
		-> conversation_receipt;

	/// \brief [[BPR-1582]]. What a thread's read command does.
	enum class conversation_read_action : std::uint8_t {
		/// Something in it is new: Mark as read.
		mark_read,
		/// Nothing in it is new, and someone else wrote in it: Mark as unread.
		mark_unread,
		/// Nothing is new and every post is mine: neither.
		none,
	};

	/// \brief [[BPR-1582]]. Decides the read command a thread offers.
	[[nodiscard]] constexpr auto conversation_read_action_for(const bool aUnread, const bool aSomeoneElseWrote) noexcept
		-> conversation_read_action
	{
		if (aUnread) {
			return conversation_read_action::mark_read;
		}
		return aSomeoneElseWrote ? conversation_read_action::mark_unread : conversation_read_action::none;
	}

	/// \brief [[BPR-1582]]. The receipt that makes a conversation new again.
	///
	/// \details The seen cursor moves back to the post before the latest
	/// post someone else wrote, so that post, and anything after it that is
	/// not mine, is new again; with no post before it nothing is seen.
	/// Acceptance is left where it was. None when every post is mine, since
	/// nothing in the conversation can be new to me.
	[[nodiscard]] auto mark_conversation_unread(
		const conversation_receipt&                    aReceipt,
		std::span<const conversation_post_observation> aPosts)
		-> std::optional<conversation_receipt>;

	/// \brief Names the single fact that decided one item's place in the queue.
	///
	/// \details The order of the enumerators *is* the priority order, and the
	/// ranking reads it that way rather than repeating the order in a
	/// comparison function. Two statements of the same order drift apart.
	///
	/// This is the explanation the presentation layer renders. It is a value
	/// and not a formatted string, because an adapter that composed its own
	/// reason text would be deciding policy.
	enum class actionability_reason : std::uint8_t {
		unresolved_defect,
		awaiting_verification,
		acceptance_outdated,
		unread_conversation,
		unread_material,
		nothing_to_do,
		excluded,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(actionability_reason aReason) noexcept
		-> std::string_view;

	/// \brief Reports whether a reason places an item in the actionable queue.
	[[nodiscard]] constexpr auto actionable(
		const actionability_reason aReason) noexcept -> bool
	{
		return aReason < actionability_reason::nothing_to_do;
	}

	/// \brief Carries everything the ranking may consider about one file.
	///
	/// \details The host assembles this from GitHub state, the ledger, and the
	/// defect record. The core sees nothing else, so the ranking cannot depend
	/// on a fact it did not declare.
	struct actionability_input
	{
		file_receipt  Receipt;
		bool          UnresolvedDefect     = false;
		bool          AwaitingVerification = false;
		bool          AcceptanceOutdated   = false;
		std::uint32_t UnreadConversations  = 0U;

		auto operator<=>(const actionability_input&) const = default;
	};

	/// \brief Selects which items the queue presents.
	struct actionability_filters
	{
		bool IncludeExcluded = false;
		bool OnlyDefects     = false;

		auto operator<=>(const actionability_filters&) const = default;
	};

	/// \brief Names one ranked item and why it sits where it does.
	struct actionable_item
	{
		file_identity        File;
		file_material_id     Material;
		actionability_reason Reason = actionability_reason::nothing_to_do;

		auto operator<=>(const actionable_item&) const = default;
	};

	/// \brief Decides why one file is actionable, if it is.
	[[nodiscard]] auto reason_for(const actionability_input& aInput) noexcept
		-> actionability_reason;

	/// \brief Ranks the actionable items of a bounded input set.
	///
	/// \details Ordering is by reason first and by file identity second. The
	/// identity tie-break is not decoration: without it two items sharing a
	/// reason would come back in whatever order the caller happened to supply,
	/// which looks stable on one run and is not deterministic. Filters change
	/// which items appear and never how the remaining ones are ordered.
	[[nodiscard]] auto rank_actionable(
		std::span<const actionability_input> aInputs,
		actionability_filters                aFilters)
		-> std::vector<actionable_item>;

	/// \brief Reports whether a conversation's acceptance has been outrun.
	///
	/// \details Acceptance is a statement about specific text. New or edited
	/// material means the reader has not accepted what is there now.
	[[nodiscard]] auto acceptance_outdated(
		const conversation_receipt&     aReceipt,
		const conversation_observation& aObservation) noexcept -> bool;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_LEDGER_HPP
