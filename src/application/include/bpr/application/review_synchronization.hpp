#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SYNCHRONIZATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SYNCHRONIZATION_HPP

#include <compare>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
	/// \brief One comment as the local projection holds it.
	///
	/// \details `ServerId` is canonical. BetterPullReview keeps a projection
	/// for its own use and re-reads server state after mutations; it never
	/// invents an identity, because an identity this tool made up would be one
	/// GitHub could not confirm.
	///
	/// `EditVersion` distinguishes an edit from a new comment. GitHub keeps the
	/// identity across an edit, so identity alone cannot tell a reader that the
	/// text changed under them.
	struct review_comment_projection
	{
		std::string   ServerId;
		std::string   Body;
		std::uint64_t EditVersion = 0U;
		bool          Resolved    = false;

		auto operator<=>(const review_comment_projection&) const = default;
	};

	/// \brief Names what a refresh did to one comment.
	enum class reconciliation_kind : std::uint8_t {
		unchanged,
		added,
		updated,
		removed,
		conflicted,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(reconciliation_kind aKind) noexcept -> std::string_view;

	/// \brief Reports what happened to one comment during a refresh.
	struct reconciliation_entry
	{
		std::string         ServerId;
		reconciliation_kind Kind = reconciliation_kind::unchanged;

		auto operator<=>(const reconciliation_entry&) const = default;
	};

	/// \brief Reports the whole outcome of one refresh.
	struct reconciliation_result
	{
		std::vector<review_comment_projection> Projection;
		std::vector<reconciliation_entry>      Entries;

		[[nodiscard]] auto conflicts() const -> std::vector<std::string>;

		auto operator<=>(const reconciliation_result&) const = default;
	};

	/// \brief Applies an observed server snapshot to the local projection.
	///
	/// \details GitHub is the authority. Where the server and the projection
	/// disagree about text the server wins, because the projection exists to
	/// mirror the server rather than to argue with it.
	///
	/// The one exception is not an exception to that rule but a refusal to
	/// silently resolve it: when a comment carries an **unpublished local
	/// edit** and the server has also changed it, the entry is reported
	/// `conflicted`. The server's text still becomes the projection — a local
	/// assumption never overwrites an external change — and the conflict is
	/// surfaced so a person can decide what to do with the edit they were
	/// holding. Discarding either side quietly would lose someone's words.
	///
	/// \param aLocallyEdited Server ids carrying an unpublished local edit.
	[[nodiscard]] auto reconcile(
		std::span<const review_comment_projection> aLocal,
		std::span<const review_comment_projection> aObserved,
		std::span<const std::string>               aLocallyEdited)
		-> reconciliation_result;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_SYNCHRONIZATION_HPP
