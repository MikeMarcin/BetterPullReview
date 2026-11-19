#ifndef BPR_CORE_TOUR_PUBLICATION_HEAD_HPP
#define BPR_CORE_TOUR_PUBLICATION_HEAD_HPP

#include "bpr/core/review_tour.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1991]]. RFC-BPR-0099, three bands in the Tour tab: where the
/// tour stands and the one publication action, decided once so the Tour tab's
/// head and Overview's tour summary say the same thing.
namespace bpr::core {
	/// \brief Whether a stop can go into a first publication as it stands:
	/// exact source, no attention, an accepted suggestion and an explanation
	/// current at the observed head. `assess_tour_publication` applies the same
	/// rule to every stop; a stop that fails it is still to accept.
	[[nodiscard]] auto tour_stop_publishable(const review_tour& aTour, const tour_stop& aStop) noexcept -> bool;

	/// \brief How many parts of a published tour differ from what GitHub has:
	/// each stop that is new or moved to other lines, each stop whose title or
	/// explanation changed, and each published stop removed from the route.
	/// Zero for a tour that is not published.
	[[nodiscard]] auto tour_unpublished_changes(const review_tour& aTour) -> std::size_t;

	/// \brief Where a publication of the tour stands, as the host reports it.
	enum class tour_publication_progress : std::uint8_t {
		/// Nothing under way and nothing left part-sent.
		idle,
		/// Steps are being sent now.
		publishing,
		/// A publication was left part-sent: it failed or was stopped after
		/// some steps, or a restart found its record.
		stopped,
	};

	struct tour_head_progress
	{
		tour_publication_progress Phase = tour_publication_progress::idle;
		/// One-based step being sent, or the step that stopped.
		std::size_t Step                                                = 0U;
		std::size_t Steps                                               = 0U;
		auto        operator==(const tour_head_progress&) const -> bool = default;
	};

	/// \brief The rows of RFC-BPR-0099's head table.
	enum class tour_head_state : std::uint8_t {
		/// No tour the viewer may publish: no head.
		none,
		not_publishable,
		publishable,
		publishing,
		stopped,
		published,
		published_changed,
	};

	/// \brief What keeps a private draft from publishing, first need first.
	enum class tour_head_missing : std::uint8_t {
		nothing,
		/// The draft is busy with a saved-copy or website conflict.
		conflict,
		stops,
		/// One or more stops are still to accept.
		acceptance,
	};

	/// \brief The head's actions, in the order they are drawn; the first is primary.
	enum class tour_head_action : std::uint8_t {
		/// Publish…: the tour page with its preview computed.
		publish,
		/// Details: the tour page, as it is.
		details,
		/// Resume the part-sent publication.
		resume,
		/// The published tour on GitHub.
		on_github,
		/// Update on GitHub…: the tour page with the update's preview computed.
		update,
	};

	struct tour_head
	{
		tour_head_state               State    = tour_head_state::none;
		std::size_t                   Stops    = 0U;
		std::size_t                   ToAccept = 0U;
		std::size_t                   Changes  = 0U;
		std::size_t                   Step     = 0U;
		std::size_t                   Steps    = 0U;
		tour_head_missing             Missing  = tour_head_missing::nothing;
		std::vector<tour_head_action> Actions;
		/// Publish… is drawn but cannot be used (`not_publishable`).
		bool PublishDisabled                            = false;
		auto operator==(const tour_head&) const -> bool = default;
	};

	/// \brief Decides the head for the window's draft and its publication.
	///
	/// \param aDraft The draft, or null while none is loaded.
	/// \param aProgress What the host reports of a publication.
	///
	/// \details A read-only draft has no head. A publication under way or
	/// part-sent outranks the draft's own state; otherwise a published tour
	/// reports its unpublished changes, and a private draft whether it is
	/// complete enough to publish.
	[[nodiscard]] auto decide_tour_head(const review_tour* aDraft, const tour_head_progress& aProgress) -> tour_head;

	/// \brief The first sentence of a stop's explanation, for a one-line row:
	/// up to the first sentence end or blank line, without `**`, `__` and
	/// backtick marks, its lines joined and trimmed, at most
	/// `aLimit` bytes cut on a code point and closed with an ellipsis.
	[[nodiscard]] auto tour_lead_sentence(std::string_view aBody, std::size_t aLimit = 200U) -> std::string;
} // namespace bpr::core

#endif
