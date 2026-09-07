#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CONVERSATION_CARDS_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CONVERSATION_CARDS_VIEW_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/review_round_trip.hpp"

#include <QAbstractScrollArea>
#include <QColor>
#include <QPoint>
#include <QRect>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

namespace bpr::qt {
	/// \brief Which threads the cards show.
	enum class cards_scope : std::uint8_t {
		/// The threads of the file the comparison shows, aligned to their lines.
		this_file,
		/// Every thread of the pull request, in file order.
		whole_pull_request,
	};

	/// \brief What a press on a card asks for.
	enum class card_action : std::uint8_t {
		/// Start a draft on this thread's line.
		comment_here,
		publish_draft,
		edit_draft,
		delete_draft,
	};

	/// \brief One post as the cards draw it.
	struct card_post
	{
		std::string Id;
		QString     Author;
		QString     Body;
		/// The post's depth beneath the root, one for the first reply.
		std::size_t Depth = 0U;
		bool        Mine  = false;
		/// Where the post was drawn, in content coordinates.
		QRect Rect;
	};

	/// \brief One card: a thread's root and its replies, or my draft.
	struct thread_card
	{
		/// The thread root's identity; empty for the draft card.
		std::string                    Id;
		QString                        Path;
		std::optional<core::diff_side> Side;
		/// The one-based line the thread sits on, when it has one.
		std::optional<std::uint32_t> Line;
		bool                         Resolved = false;
		bool                         Outdated = false;
		bool                         Draft    = false;
		/// True while the thread's line is on the comparison and locatable.
		bool Placed = false;
		/// Where the line is, in content coordinates, when placed.
		int                    TargetY = 0;
		QRect                  Rect;
		std::vector<card_post> Posts;
		/// The action buttons' rects, in content coordinates.
		std::vector<std::pair<card_action, QRect>> Actions;
	};

	/// \brief Answers where a one-based line is, in this view's viewport.
	using card_line_locator    = std::function<std::optional<int>(core::diff_side, std::uint32_t)>;
	using card_thread_dispatch = std::function<void(const std::string&)>;
	using card_action_dispatch = std::function<void(card_action, const std::string&)>;

	/// \brief RFC-BPR-0028's Read drawer: threads as cards at their lines.
	///
	/// \details [[BPR-0648]]. A drawn view of the drawer's own forest, as the
	/// thread map is: it holds no authority over the conversation and
	/// decides nothing about it. Each review thread is one card, the root
	/// anchored to its line, the replies beneath it on an avatar rail with
	/// a thread line. Cards never overlap: each sits at its line or beneath
	/// the card above, whichever is lower, and the whole column follows the
	/// comparison's scroll because the window relays it on every frame.
	/// Whole PR lists every thread in file order under its path.
	///
	/// A press on a card makes it current and reports its root, which the
	/// window turns into the reveal of its line; a press on an action
	/// reports the action. Read state, Mark read and Resolve wait on the
	/// authorities BPR-0656 and BPR-0657 add; until then no post is drawn as
	/// read or unread, and the only action on a thread is a comment on its
	/// line.
	class conversation_cards_view final : public QAbstractScrollArea
	{
	public:
		explicit conversation_cards_view(QWidget* aParent = nullptr);

		void               adopt(const std::vector<application::github_conversation_projection>& aConversations);
		void               clear();
		void               set_scope(cards_scope aScope);
		[[nodiscard]] auto scope() const noexcept -> cards_scope;
		void               set_anchor_file(std::optional<core::file_identity> aFile);
		void               set_line_locator(card_line_locator aLocator);
		/// \brief My unpublished draft, drawn as a dashed card at its line.
		void set_draft(
			std::optional<core::review_comment_draft>   aDraft,
			std::optional<core::file_comparison_digest> aCurrentComparison);
		void set_viewer_login(const QString& aLogin);
		void set_thread_dispatch(card_thread_dispatch aDispatch);
		void set_action_dispatch(card_action_dispatch aDispatch);
		/// \brief Makes a thread current without reporting it.
		void set_current_thread(const std::string& aId);
		/// \brief Lays the cards out again against the locator.
		void relayout();

		[[nodiscard]] auto cards() const noexcept -> const std::vector<thread_card>&;
		[[nodiscard]] auto current_thread() const noexcept -> const std::string&;
		[[nodiscard]] auto layouts() const noexcept -> std::uint64_t;
		/// \brief The card under a viewport point, and the action if any.
		[[nodiscard]] auto hit_test(QPoint aViewportPoint) const
			-> std::optional<std::pair<std::size_t, std::optional<card_action>>>;
		/// \brief Lines that carry a thread on a side, zero-based, for the gutter.
		[[nodiscard]] auto marked_lines(core::diff_side aSide) const -> std::vector<std::uint32_t>;
		[[nodiscard]] auto post_count() const noexcept -> std::size_t;
		void               set_colours(const QColor& aAccent, const QColor& aMarker);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;

	private:
		[[nodiscard]] auto card_height(const thread_card& aCard, int aWidth) const -> int;
		void               measure(thread_card& aCard, int aWidth) const;
		[[nodiscard]] auto to_content(QPoint aViewportPoint) const -> QPoint;
		void               choose(std::size_t aCard);

		std::vector<application::github_conversation_projection> Retained;
		std::vector<thread_card>                                 Cards;
		std::optional<core::file_identity>                       AnchorFile;
		std::optional<core::review_comment_draft>                Draft;
		std::optional<core::file_comparison_digest>              CurrentComparison;
		card_line_locator                                        Locator;
		card_thread_dispatch                                     ThreadDispatch;
		card_action_dispatch                                     ActionDispatch;
		QString                                                  Viewer;
		std::string                                              Current;
		cards_scope                                              Scope = cards_scope::this_file;
		QColor                                                   Accent{ 0x09, 0x69, 0xDA };
		QColor                                                   Marker{ 0xD2, 0x99, 0x22 };
		int                                                      ContentHeight = 0;
		std::uint64_t                                            Layouts       = 0U;
	};
} // namespace bpr::qt

#endif
