#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CONVERSATION_CARDS_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CONVERSATION_CARDS_VIEW_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/review_round_trip.hpp"

#include <QAbstractScrollArea>
#include <QColor>
#include <QFont>
#include <QPoint>
#include <QRect>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

class QContextMenuEvent;
class QMenu;
class QToolButton;
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
		publish_draft,
		edit_draft,
		delete_draft,
		/// Read this thread up to its latest post ([[BPR-0656]]).
		mark_read,
		/// Start a reply on this thread ([[BPR-0649]]). [[BPR-1133]]: every
		/// thread card offers it, beside the code as in Discuss, through the
		/// one path the feed's Reply takes; no card starts a second thread.
		reply,
		/// Return to Read at this thread's line ([[BPR-0649]]).
		open_in_code,
		/// Resolve or reopen this thread through the mutation port ([[BPR-0657]]).
		resolve,
		unresolve,
		defect,
	};

	/// \brief Discuss's filters over the feed ([[BPR-0649]]).
	struct discuss_filters
	{
		bool UnreadOnly     = false;
		bool UnresolvedOnly = false;

		auto operator<=>(const discuss_filters&) const = default;
	};

	/// \brief What the reader has not seen in one thread.
	///
	/// \details [[BPR-0656]]. The thread root's projection identity and the
	/// projection identities of its unread posts, as the ledger host decided
	/// them; a thread absent from the state is read.
	struct thread_read_state
	{
		std::string              Thread;
		std::vector<std::string> UnreadPosts;

		auto operator<=>(const thread_read_state&) const = default;
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
		/// Newer than my cursor, as the ledger decided ([[BPR-0656]]).
		bool Unread = false;
		/// The feed's header ([[BPR-0649]]): "author" for the pull request's
		/// author, "reviewer" otherwise; and whom the post replies to when it
		/// answers another reply rather than the thread.
		QString Role;
		QString ReplyingTo;
		/// Where the post was drawn, in content coordinates.
		QRect Rect;
	};

	/// \brief One card: a thread's root and its replies, or my draft.
	struct thread_card
	{
		/// The thread root's identity; empty for the draft card.
		std::string                    Id;
		QString                        Kind;
		QString                        DefectBadge;
		QString                        DefectDescription;
		QString                        Path;
		std::optional<core::diff_side> Side;
		/// The one-based line the thread sits on, when it has one.
		std::optional<std::uint32_t> Line;
		bool                         Resolved          = false;
		bool                         Outdated          = false;
		bool                         AnchorUnavailable = false;
		bool                         Draft             = false;
		/// How many of the posts are unread; the first post unread makes the
		/// thread new.
		std::size_t UnreadCount = 0U;
		bool        RootUnread  = false;
		/// The code at the thread's line, when the feed can read it
		/// ([[BPR-0649]]); empty otherwise.
		QString Excerpt;
		/// True while the thread's line is on the comparison and locatable.
		bool Placed = false;
		/// Where the line is, in content coordinates, when placed.
		/// Stable projection order breaks geometry ties across comparison frames.
		std::size_t            ProjectionOrder = 0U;
		int                    TargetY         = 0;
		QRect                  Rect;
		std::vector<card_post> Posts;
		/// The action buttons' rects, in content coordinates.
		std::vector<std::pair<card_action, QRect>> Actions;
	};

	/// \brief Answers where a one-based line is, in this view's viewport.
	using card_line_locator = std::function<std::optional<int>(core::diff_side, std::uint32_t)>;
	/// \brief Reports an index selection and whether the gesture explicitly opens detail.
	using card_thread_dispatch = std::function<void(const std::string&, bool)>;
	using card_action_dispatch = std::function<void(card_action, const std::string&)>;
	/// \brief Answers the code around a one-based line of a path, for the feed.
	using card_excerpt_locator
		= std::function<std::optional<QString>(const QString&, core::diff_side, std::uint32_t)>;

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

		void adopt(const std::vector<application::github_conversation_projection>& aConversations);
		/// \brief Adopts the forest and file anchor in one layout.
		void               adopt(const std::vector<application::github_conversation_projection>& aConversations,
								 std::optional<core::file_identity>                              aFile);
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
		using defect_badges = std::map<std::string, std::pair<QString, QString>>;
		void               set_defect_badges(defect_badges aBadges);
		void               set_context_menu_builder(std::function<void(QMenu&, const std::string&)> aBuilder);
		[[nodiscard]] auto thread_menu(const std::string& aIdentity) -> QMenu*;
		void               show_thread_menu(const std::string& aIdentity, QPoint aGlobalPosition = { });
		/// \brief Discuss's feed ([[BPR-0649]]): every thread as a card in
		/// file order with its code excerpt, each post headed by author,
		/// role and "replying to", and Reply, Mark read and Open in code.
		void set_feed(bool aFeed);
		/// Compact navigation entries; complete bodies belong to the selected detail.
		void               set_compact(bool aCompact);
		[[nodiscard]] auto feed() const noexcept -> bool;
		void               set_filters(discuss_filters aFilters);
		[[nodiscard]] auto filters() const noexcept -> discuss_filters;
		void               set_pull_request_author(const QString& aLogin);
		void               set_excerpt_locator(card_excerpt_locator aLocator);
		/// \brief Told after every relayout, so a summary can restate the counts.
		void               set_layout_observer(std::function<void()> aObserver);
		[[nodiscard]] auto thread_count() const noexcept -> std::size_t;
		[[nodiscard]] auto unresolved_count() const noexcept -> std::size_t;
		[[nodiscard]] auto unread_thread_count() const noexcept -> std::size_t;
		void               set_code_colours(const QColor& aBackground, const QColor& aInk);
		/// \brief [[BPR-1270]]. The excerpt's font: the appearance profile's
		/// code font. Until one is supplied, the default typography's.
		void               set_code_font(const QFont& aFont);
		[[nodiscard]] auto code_font() const -> const QFont&;
		/// \brief Adopts the read state the ledger host decided.
		///
		/// \details [[BPR-0656]]. Until a state arrives every post is drawn
		/// plainly: the cards do not guess what is unread.
		void set_read_state(std::vector<thread_read_state> aState);
		/// \brief [[BPR-0972]]. Adopts one thread's unread posts; empty means read.
		/// \returns The thread's previous unread posts.
		/// \details Lays out again only when that thread's card is shown.
		auto set_thread_read_state(const std::string& aThread, std::vector<std::string> aUnreadPosts)
			-> std::vector<std::string>;
		/// \brief [[BPR-0657]]. Offers Resolve and Unresolve on the threads when the
		/// provider and the server both resolve threads.
		void               set_resolution_offered(bool aOffered);
		[[nodiscard]] auto resolution_offered() const noexcept -> bool;
		/// \brief The unread threads of the whole pull request, in file order.
		///
		/// \details [[BPR-0972]]. Decided once per adopted forest or read
		/// state and retained, so a scroll frame that asks again walks nothing.
		[[nodiscard]] auto unread_threads_in_file_order() const -> const std::vector<std::string>&;
		/// \brief The zero-based lines of the cards whose first post is unread.
		[[nodiscard]] auto unread_marked_lines(core::diff_side aSide) const -> std::vector<std::uint32_t>;
		[[nodiscard]] auto unread_post_count() const noexcept -> std::size_t;
		/// \brief Makes a thread current without reporting it.
		void set_current_thread(const std::string& aId);
		/// \brief Lays the cards out again against the locator.
		void relayout();
		/// \brief Refreshes locator geometry, retaining measured posts when unchanged.
		void refresh_geometry();

		[[nodiscard]] auto cards() const noexcept -> const std::vector<thread_card>&;
		[[nodiscard]] auto current_thread() const noexcept -> const std::string&;
		[[nodiscard]] auto layouts() const noexcept -> std::uint64_t;
		/// \brief [[BPR-0972]]. Retained conversation nodes visited by layouts.
		[[nodiscard]] auto layout_nodes_visited() const noexcept -> std::uint64_t;
		/// \brief Number of card text measurements, excluding retained-cache hits.
		[[nodiscard]] auto measurements() const noexcept -> std::uint64_t;
		/// \brief The card under a viewport point, and the action if any.
		[[nodiscard]] auto hit_test(QPoint aViewportPoint) const
			-> std::optional<std::pair<std::size_t, std::optional<card_action>>>;
		/// \brief Lines that carry a thread on a side, zero-based, for the gutter.
		[[nodiscard]] auto marked_lines(core::diff_side aSide) const -> std::vector<std::uint32_t>;
		[[nodiscard]] auto post_count() const noexcept -> std::size_t;
		void               set_colours(const QColor& aAccent, const QColor& aMarker);

	protected:
		void changeEvent(QEvent* aEvent) override;
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void contextMenuEvent(QContextMenuEvent* aEvent) override;

	private:
		void sync_badge_buttons();
		struct measured_card
		{
			QString                                    Label;
			QString                                    Excerpt;
			std::vector<QString>                       Bodies;
			bool                                       Header          = false;
			bool                                       Draft           = false;
			bool                                       Resolved        = false;
			bool                                       Unread          = false;
			bool                                       AnchorAvailable = false;
			int                                        Height          = 0;
			std::vector<QRect>                         Posts;
			std::vector<std::pair<card_action, QRect>> Actions;
		};
		/// \brief The measurements taken at one content width.
		struct measurement_generation
		{
			int                                  Width = -1;
			std::map<std::string, measured_card> Cards;
		};
		struct compact_measurement
		{
			int                                        Height = 0;
			std::vector<std::pair<card_action, QRect>> Actions;
		};
		using compact_measurements = std::map<std::tuple<QString, bool, bool, bool, bool, bool>, compact_measurement>;
		void               measure_cached(thread_card& aCard, int aWidth, compact_measurements* aCompact = nullptr);
		void               clear_measurements();
		void               forget_measurements_if(const std::function<bool(const std::string&)>& aForget);
		[[nodiscard]] auto card_height(const thread_card& aCard, int aWidth) const -> int;
		void               measure(thread_card& aCard, int aWidth) const;
		[[nodiscard]] auto to_content(QPoint aViewportPoint) const -> QPoint;
		void               choose(std::size_t aCard, bool aOpenDetail);
		void               place_cards(bool aForce);

		/// \brief Derives the per-forest indices a layout reads.
		void index_retained();

		std::vector<application::github_conversation_projection> Retained;
		/// The file identity of each retained node's path, index for index,
		/// derived once per adopted forest rather than once per relayout.
		std::vector<core::file_identity> RetainedFiles;
		/// [[BPR-0972]]. Per adopted forest: the first node for each identity,
		/// each node's root and depth, and every node by its root's file, so a
		/// layout scoped to one file visits that file's conversations only.
		std::unordered_map<std::string_view, std::size_t>        RetainedById;
		std::vector<std::size_t>                                 RetainedRoot;
		std::vector<std::size_t>                                 RetainedDepth;
		std::vector<std::pair<core::file_identity, std::size_t>> RetainedByFile;
		/// [[BPR-0972]]. The first read state of each thread.
		std::unordered_map<std::string, std::size_t>    ReadStateByThread;
		mutable std::optional<std::vector<std::string>> UnreadThreads;
		std::uint64_t                                   LayoutNodesVisited = 0U;
		std::vector<thread_card>                        Cards;
		std::optional<core::file_identity>              AnchorFile;
		std::optional<core::review_comment_draft>       Draft;
		std::optional<core::file_comparison_digest>     CurrentComparison;
		card_line_locator                               Locator;
		card_thread_dispatch                            ThreadDispatch;
		card_action_dispatch                            ActionDispatch;
		defect_badges                                   DefectBadges;
		std::function<void(QMenu&, const std::string&)> ContextMenuBuilder;
		std::vector<QToolButton*>                       BadgeButtons;
		QString                                         Viewer;
		std::string                                     Current;
		std::vector<thread_read_state>                  ReadState;
		bool                                            ReadStateKnown    = false;
		bool                                            Feed              = false;
		bool                                            Compact           = false;
		bool                                            ResolutionOffered = false;
		discuss_filters                                 Filters;
		QString                                         PullRequestAuthor;
		card_excerpt_locator                            ExcerptLocator;
		std::function<void()>                           LayoutObserver;
		QFont                                           CodeFont;
		QColor                                          CodeBackground{ 0xF6, 0xF8, 0xFA };
		QColor                                          CodeInk{ 0x1F, 0x23, 0x28 };
		cards_scope                                     Scope = cards_scope::this_file;
		QColor                                          Accent{ 0x09, 0x69, 0xDA };
		QColor                                          Marker{ 0xD2, 0x99, 0x22 };
		int                                             ContentHeight = 0;
		std::uint64_t                                   Layouts       = 0U;
		// Bounded by two widths of the current retained forest plus its one
		// draft. Content, authority and presentation invalidations discard
		// these rectangles.
		//
		// [[BPR-0673]]. Two widths, because the vertical scroll bar alternates
		// the content width: a whole-pull-request layout shows it and a
		// one-file layout hides it. Keeping one width re-measured every card of
		// a large review each time a refresh widened the drawer.
		measurement_generation Measurements;
		measurement_generation PreviousMeasurements;
		QFont                  MeasurementFont;
		bool                   MeasurementFeed              = false;
		bool                   MeasurementResolutionOffered = false;
		std::uint64_t          MeasurementCount             = 0U;
	};
} // namespace bpr::qt

#endif
