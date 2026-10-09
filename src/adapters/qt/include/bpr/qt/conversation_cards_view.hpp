#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CONVERSATION_CARDS_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CONVERSATION_CARDS_VIEW_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/review_round_trip.hpp"
#include "bpr/core/suggestion_applicability.hpp"
#include "bpr/core/suggestion_draft.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

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
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

class QAbstractButton;
class QContextMenuEvent;
class QMenu;
class QPlainTextEdit;
class QToolButton;
class QTimer;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;

namespace bpr::qt {
	class feedback_label_controls;

	/// \brief Which threads the notes show.
	enum class cards_scope : std::uint8_t {
		/// The threads of the file the comparison shows, at their lines.
		this_file,
		/// Every thread of the pull request, in file order (Discuss).
		whole_pull_request,
	};

	/// \brief What a gesture on a thread asks for.
	enum class card_action : std::uint8_t {
		publish_draft,
		edit_draft,
		delete_draft,
		/// Read this thread up to its latest post ([[BPR-0656]]).
		mark_read,
		/// Start a reply on this thread ([[BPR-0649]]). [[BPR-1290]]: the
		/// reply is written in place, at the foot of the open thread, through
		/// the one Reply path; no thread starts a second thread.
		reply,
		/// Return to the code at this thread's line from Discuss ([[BPR-0649]]).
		open_in_code,
		/// Resolve or reopen this thread through the mutation port ([[BPR-0657]]).
		resolve,
		unresolve,
		defect,
		/// [[BPR-1290]]. Reveal the open thread's line in the code, without
		/// changing the modality.
		show_line,
		/// [[BPR-1290]]. Publish the reply written in place.
		publish_reply,
		/// [[BPR-1290]]. Give up the reply written in place.
		cancel_reply,
		/// [[BPR-1212]]. Open the draft written in the drawer in Discuss,
		/// where the full composer is (⌘⇧↩).
		maximize_draft,
		/// [[BPR-1212]]. Leave the draft written in the drawer (Esc, Close):
		/// the window keeps a written draft and drops an empty one.
		close_draft,
	};

	/// \brief The notes' filters ([[BPR-0649]], [[BPR-1290]]).
	struct discuss_filters
	{
		/// Only threads with a post newer than what I have read.
		bool UnreadOnly = false;
		/// Hide resolved threads.
		bool UnresolvedOnly = false;
		/// [[BPR-1089]]. Exactly these conversations, by root identity: an
		/// Overview count's drill-down. None lists every conversation.
		std::optional<std::set<std::string>> Listed;

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

	/// \brief One post as the notes draw it.
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
		/// "author" for the pull request's author, "reviewer" otherwise.
		QString Role;
		/// Whom the post answers when it answers anything but the thread's
		/// first post ([[BPR-1290]]: deep threads are read in place).
		QString ReplyingTo;
		/// [[BPR-1083]]. The post's label mark, when its body begins with a
		/// recognized Conventional Comments prefix. `Body` is still the whole
		/// body, its header and decorations included, as written.
		std::optional<core::feedback_label> Label;
		/// The label's written decorations, as its mark and rail show them.
		feedback_mark_emphasis Emphasis;
		/// "tbutler, issue (blocking)": the author, then the mark's name.
		QString Name;
		/// [[BPR-1290]]. How many characters of `Body` the recognized header
		/// takes after `Lead`, which the notes set in semibold; zero for a
		/// plain post.
		qsizetype HeaderLength = 0;
		/// [[BPR-1086]]. Where a closed note's words start: a published tour
		/// stop's narrative, after its pinned source and generated heading, so
		/// its label reads beside its mark; zero for any other post.
		qsizetype Lead = 0;
	};

	/// \brief One thread's note: a review thread's root and its replies, or my draft.
	struct thread_card
	{
		/// The thread root's identity; empty for the draft note.
		std::string                    Id;
		QString                        Kind;
		QString                        DefectBadge;
		QString                        DefectDescription;
		QString                        Path;
		std::optional<core::diff_side> Side;
		/// The one-based line the thread sits on, when it has one.
		std::optional<std::uint32_t> Line;
		/// [[BPR-1212]] slice 1. My draft on several lines: its one-based
		/// first line; `Line` is its last, where it is anchored.
		std::optional<std::uint32_t> FirstLine;
		bool                         Resolved          = false;
		bool                         Outdated          = false;
		bool                         AnchorUnavailable = false;
		bool                         Draft             = false;
		/// [[BPR-1212]]. My draft, open and being written in the drawer: the
		/// composer, not the note. It carries no posts; the composer shows
		/// what the draft editor holds.
		bool Writing = false;
		/// [[BPR-1290]]. GitHub stores the thread as a file comment that its
		/// code link places at a line (RFC-BPR-0022).
		bool FileLevel = false;
		/// How many of the posts are unread; the first post unread makes the
		/// thread new.
		std::size_t UnreadCount = 0U;
		bool        RootUnread  = false;
		/// The code at the thread's line, when Discuss can read it
		/// ([[BPR-0649]]); empty otherwise.
		QString Excerpt;
		/// [[BPR-1084]]. What a reply in this thread is attached to on
		/// GitHub, which decides whether a suggested change in it is native.
		core::suggestion_placement_observation Suggestion;
		/// True while the thread's line is on the comparison and locatable.
		bool Placed = false;
		/// Stable projection order breaks geometry ties across comparison frames.
		std::size_t ProjectionOrder = 0U;
		/// Where the line is, in viewport coordinates, when placed.
		int TargetY = 0;
		/// Where the note is drawn, in content coordinates.
		QRect                  Rect;
		std::vector<card_post> Posts;
	};

	/// \brief [[BPR-1084]]. What a reply in this conversation would be attached
	/// to on GitHub, from its accepted projection: a file comment (also one its
	/// code link places at a line), an outdated line, no current line, the pull
	/// request's own conversation, or a current line on its side.
	[[nodiscard]] auto suggestion_placement_of(const application::github_conversation_projection& aNode) -> core::suggestion_placement_observation;

	/// \brief [[BPR-1290]]. One thread's marker in the code's gutter and its
	/// tick on the code's scroll bar.
	struct thread_line_mark
	{
		std::uint32_t                       Line = 0U;
		std::optional<core::feedback_label> Label;
		feedback_mark_emphasis              Emphasis;
		bool                                Unread   = false;
		bool                                Resolved = false;
		bool                                Draft    = false;
		/// The open thread: its line is washed and its marker ringed.
		bool Open = false;
		/// [[BPR-1212]]. My draft, being written in the drawer: a blue
		/// bracket in place of the amber wash.
		bool Writing = false;
		/// [[BPR-1212]] slice 1. My draft on several lines: its zero-based
		/// first line, where the bracket starts.
		std::optional<std::uint32_t> FirstLine;
		/// The note under the pointer: its line is washed lightly.
		bool Hovered = false;
		/// "Line 656: 4 posts, 2 new, resolved".
		QString Description;

		auto operator==(const thread_line_mark&) const -> bool = default;
	};

	/// \brief Answers where a one-based line is, in this view's viewport.
	using card_line_locator = std::function<std::optional<int>(core::diff_side, std::uint32_t)>;
	/// \brief Reports a thread made current and whether the gesture opens it.
	using card_thread_dispatch = std::function<void(const std::string&, bool)>;
	using card_action_dispatch = std::function<void(card_action, const std::string&)>;
	/// \brief Answers the code around a one-based line of a path, for Discuss.
	using card_excerpt_locator
		= std::function<std::optional<QString>(const QString&, core::diff_side, std::uint32_t)>;

	/// \brief [[BPR-1290]]. The reply written in place, as the draft editor holds it.
	struct thread_reply_presentation
	{
		/// The thread the draft answers, by root identity.
		std::string Thread;
		QString     Body;
		/// The draft editor's Publish is enabled.
		bool Publishable = false;
		/// [[BPR-1290]]. The draft editor's label helper is on, so the reply
		/// completes and replaces `:xx:` codes as the composer does
		/// ([[BPR-1083]]); off, a label is typed in full.
		bool LabelHelper = false;

		auto operator==(const thread_reply_presentation&) const -> bool = default;
	};

	/// \brief [[BPR-1212]]. The new comment written in the drawer, as the
	/// draft editor holds it.
	struct draft_composer_presentation
	{
		QString Body;
		/// The draft editor's Publish is enabled.
		bool Publishable = false;
		/// The label helper is on: `:xx:` codes complete and replace.
		bool LabelHelper = false;
		/// The draft can be written here. A draft whose retained text is in
		/// conflict, or that cannot be edited, is read but not written.
		bool Editable = true;
		/// The lines the draft is on, in words: "Line 12", "Lines 10–12".
		QString Lines;
		/// What the draft's suggestion block changes, when it has one.
		std::optional<core::suggested_change> Change;
		/// What the last Publish came to ([[BPR-1329]]), else why Publish is
		/// unavailable, or empty.
		QString Notice;
		/// [[BPR-1329]]. The draft editor offers Publish again as a retry of
		/// the same publication.
		bool Retry = false;

		auto operator==(const draft_composer_presentation&) const -> bool = default;
	};

	class open_thread_card;

	/// \brief RFC-BPR-0028's Comments tab: this file's threads as notes in a
	/// margin beside the code ([[BPR-0648]], [[BPR-1290]]).
	///
	/// \details A drawn view of the drawer's own forest: it holds no authority
	/// over the conversation and decides nothing about it. Each review thread
	/// is one note hanging from its line: the first post's author, avatar,
	/// label mark and rail ([[BPR-1083]]), two lines of its body as written,
	/// the replies and what is new. One thread at a time opens in place, pinned
	/// exactly at its line, to the whole conversation with Show line, Resolve,
	/// Mark read, More and a reply written at its foot; the notes around it
	/// make room above and below. The drawer moves with the code: the window
	/// relays every applied scroll frame, a wheel over the notes scrolls the
	/// code, and only when the code can go no further do the notes take the
	/// overflow themselves. Threads above or below what the notes show are
	/// chips on the top and bottom edges, nearest first.
	///
	/// In Discuss (`set_feed`) the same notes are a list in file order under
	/// their paths, with their own scrolling, each opening in place.
	///
	/// Opening a note makes it current and reports its root, which the window
	/// turns into the reveal of its line; a gesture on an action reports the
	/// action.
	class conversation_cards_view final : public QAbstractScrollArea
	{
	public:
		explicit conversation_cards_view(QWidget* aParent = nullptr);
		~conversation_cards_view() override;

		conversation_cards_view(const conversation_cards_view&)                    = delete;
		auto operator=(const conversation_cards_view&) -> conversation_cards_view& = delete;
		conversation_cards_view(conversation_cards_view&&)                         = delete;
		auto operator=(conversation_cards_view&&) -> conversation_cards_view&      = delete;

		void adopt(const std::vector<application::github_conversation_projection>& aConversations);
		/// \brief Adopts the forest and file anchor in one layout.
		void               adopt(const std::vector<application::github_conversation_projection>& aConversations,
								 std::optional<core::file_identity>                              aFile);
		void               clear();
		void               set_scope(cards_scope aScope);
		[[nodiscard]] auto scope() const noexcept -> cards_scope;
		void               set_anchor_file(std::optional<core::file_identity> aFile);
		void               set_line_locator(card_line_locator aLocator);
		/// \brief My unpublished draft, drawn as a dashed note at its line.
		///
		/// \details A draft that answers a thread is that thread's reply,
		/// written in place, and draws no note of its own.
		void set_draft(
			std::optional<core::review_comment_draft>   aDraft,
			std::optional<core::file_comparison_digest> aCurrentComparison);
		/// \brief [[BPR-1326]]. My other unpublished drafts, each drawn as a
		/// dashed note at its line while it is on this comparison, as the
		/// editor's draft is. The editor's draft is drawn from the editor.
		/// \details The editor holds only the selected line's draft, so a draft
		/// on any other line showed nothing until its line was chosen again.
		void set_saved_drafts(std::vector<core::review_comment_draft> aDrafts);
		void set_viewer_login(const QString& aLogin);
		void set_thread_dispatch(card_thread_dispatch aDispatch);
		void set_action_dispatch(card_action_dispatch aDispatch);
		using defect_badges = std::map<std::string, std::pair<QString, QString>>;
		void               set_defect_badges(defect_badges aBadges);
		void               set_context_menu_builder(std::function<void(QMenu&, const std::string&)> aBuilder);
		[[nodiscard]] auto thread_menu(const std::string& aIdentity) -> QMenu*;
		void               show_thread_menu(const std::string& aIdentity, QPoint aGlobalPosition = { });
		/// \brief Discuss's list ([[BPR-0649]]): every thread in file order
		/// under its path, each opening in place, with its code excerpt.
		void               set_feed(bool aFeed);
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
		/// \brief [[BPR-1270]]. The excerpt's and inline code's font: the
		/// appearance profile's code font. Until one is supplied, the default
		/// typography's.
		void               set_code_font(const QFont& aFont);
		[[nodiscard]] auto code_font() const -> const QFont&;
		/// \brief Adopts the read state the ledger host decided.
		///
		/// \details [[BPR-0656]]. Until a state arrives every post is drawn
		/// plainly: the notes do not guess what is unread.
		void set_read_state(std::vector<thread_read_state> aState);
		/// \brief [[BPR-0972]]. Adopts one thread's unread posts; empty means read.
		/// \returns The thread's previous unread posts.
		/// \details Lays out again only when that thread's note is shown.
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
		/// \brief The zero-based lines of the threads whose first post is unread.
		[[nodiscard]] auto unread_marked_lines(core::diff_side aSide) const -> std::vector<std::uint32_t>;
		[[nodiscard]] auto unread_post_count() const noexcept -> std::size_t;
		/// \brief [[BPR-1290]]. Every shown thread's gutter marker and scroll-bar
		/// tick on one side, by zero-based line, sorted and one per line.
		[[nodiscard]] auto line_marks(core::diff_side aSide) const -> std::vector<thread_line_mark>;
		/// \brief Makes a thread current without reporting it or opening it.
		void set_current_thread(const std::string& aId);
		/// \brief Lays the notes out again against the locator.
		void relayout();
		/// \brief Follows the code: places the notes at their lines again,
		/// retaining measured notes.
		void refresh_geometry();

		/// \brief [[BPR-1290]]. Opens one thread in place, closing any other,
		/// makes it current and reports it as the reviewer's choice.
		void open_thread(const std::string& aId);
		/// \brief [[BPR-1293]]. Opens one thread in place, as `open_thread`
		/// does, with one of its posts picked: washed amber and brought into
		/// view. A post the thread does not hold picks nothing.
		void open_thread_at(const std::string& aId, const std::string& aPost);
		/// \brief The picked post of the open thread, or empty. Opening
		/// another thread, or closing this one, drops it.
		[[nodiscard]] auto picked_post() const noexcept -> const std::string&;
		/// \brief Opens a closed thread or closes the open one.
		void toggle_thread(const std::string& aId);
		/// \brief Closes the open thread, keeping it current.
		void               close_thread();
		[[nodiscard]] auto open_thread_id() const noexcept -> const std::string&;
		/// \brief Told whenever the open thread or the note under the pointer
		/// changes, so the gutter and scroll bar can follow.
		void set_marks_observer(std::function<void()> aObserver);
		/// \brief A wheel over the notes scrolls the code: the sink reports
		/// whether the code moved. While the code cannot move, the notes take
		/// the overflow themselves.
		void set_scroll_forward(std::function<bool(QWheelEvent&)> aForward);
		/// \brief Link destinations in a post, with the post's identity, for
		/// the application's admission ([[BPR-0456]]).
		void set_link_dispatch(std::function<void(const std::string&, const std::string&)> aDispatch);
		/// \brief [[BPR-1290]]. Shows the reply editor at the foot of the open
		/// thread and gives it focus.
		void begin_reply(const std::string& aThread);
		/// \brief [[BPR-1290]]. The draft editor's reply, so the editor in
		/// place shows and offers what the draft holds.
		void present_reply(std::optional<thread_reply_presentation> aReply);
		/// \brief [[BPR-1290]]. Each edit of the reply written in place.
		void set_reply_text_dispatch(std::function<void(const std::string&, const QString&)> aDispatch);
		/// \brief The editor of the reply in place, while one is shown.
		[[nodiscard]] auto reply_editor() const -> QPlainTextEdit*;
		/// \brief [[BPR-1212]]. Opens my draft in the drawer as a draft thread
		/// at its line, writing, and gives its field focus. The draft is
		/// shown even while it is empty.
		void begin_draft();
		/// \brief [[BPR-1212]]. Stops writing: the draft closes to its note,
		/// or to nothing while it is empty.
		void end_draft();
		/// \brief [[BPR-1212]]. Whether my draft is open and being written.
		[[nodiscard]] auto writing_draft() const noexcept -> bool;
		/// \brief [[BPR-1212]]. What the draft editor holds for the draft.
		void present_draft(std::optional<draft_composer_presentation> aDraft);
		/// \brief [[BPR-1212]]. Each edit of the draft written in the drawer.
		void set_draft_text_dispatch(std::function<void(const QString&)> aDispatch);
		/// \brief [[BPR-1212]]. The draft's field, while it is being written.
		[[nodiscard]] auto draft_editor() const -> QPlainTextEdit*;
		/// \brief [[BPR-1212]]. The identity the draft's note and composer carry.
		[[nodiscard]] static auto draft_identity() -> const std::string&;
		/// \brief The reply's label chip, menu and shortcodes ([[BPR-1083]]'s
		/// controls, as the composer has them), while a reply is shown.
		[[nodiscard]] auto reply_label_controls() const -> feedback_label_controls*;
		/// \brief The open thread's card, while one is open.
		[[nodiscard]] auto open_card() const -> QWidget*;
		/// \brief The out-of-view chips shown now, top first then bottom.
		[[nodiscard]] auto edge_chips() const -> std::vector<QAbstractButton*>;
		/// \brief What the notes scroll by themselves beyond the code.
		[[nodiscard]] auto overflow() const -> int;
		/// \brief One note's rectangle in viewport coordinates, or empty.
		[[nodiscard]] auto note_rect(const std::string& aId) const -> QRect;
		/// \brief The thread whose note is under the pointer, if any.
		[[nodiscard]] auto hovered_thread() const noexcept -> const std::string&;

		[[nodiscard]] auto cards() const noexcept -> const std::vector<thread_card>&;
		[[nodiscard]] auto current_thread() const noexcept -> const std::string&;
		[[nodiscard]] auto layouts() const noexcept -> std::uint64_t;
		/// \brief [[BPR-0972]]. Retained conversation nodes visited by layouts.
		[[nodiscard]] auto layout_nodes_visited() const noexcept -> std::uint64_t;
		/// \brief Number of note text measurements, excluding retained-cache hits.
		[[nodiscard]] auto measurements() const noexcept -> std::uint64_t;
		/// \brief The note under a viewport point.
		[[nodiscard]] auto hit_test(QPoint aViewportPoint) const -> std::optional<std::size_t>;
		/// \brief Lines that carry a thread on a side, zero-based, for the gutter.
		[[nodiscard]] auto marked_lines(core::diff_side aSide) const -> std::vector<std::uint32_t>;
		[[nodiscard]] auto post_count() const noexcept -> std::size_t;
		/// \brief The colour of what is new and the amber of the open thread.
		void               set_colours(const QColor& aAccent, const QColor& aMarker);
		[[nodiscard]] auto accent() const noexcept -> const QColor&;
		[[nodiscard]] auto marker() const noexcept -> const QColor&;
		/// \brief [[BPR-1083]]. The label tints of the theme on screen and the
		/// mark style.
		void               set_label_marks(const feedback_mark_theme& aTheme);
		[[nodiscard]] auto label_marks() const noexcept -> const feedback_mark_theme&;
		/// \brief Where a closed note draws its first post's 16 px mark, in
		/// viewport coordinates: centred under its 18 px avatar, 3 px below
		/// it. Empty for a plain first post.
		[[nodiscard]] auto mark_rect(const thread_card& aCard) const -> QRect;
		/// \brief Where a closed note's rail is drawn, in viewport coordinates:
		/// down its left edge, 2 px wide or 4 px when blocking, growing outward.
		[[nodiscard]] auto rail_rect(const thread_card& aCard) const -> QRectF;

	protected:
		void changeEvent(QEvent* aEvent) override;
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void showEvent(QShowEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void wheelEvent(QWheelEvent* aEvent) override;
		void contextMenuEvent(QContextMenuEvent* aEvent) override;
		void focusInEvent(QFocusEvent* aEvent) override;
		void focusOutEvent(QFocusEvent* aEvent) override;
		auto viewportEvent(QEvent* aEvent) -> bool override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		/// \brief [[BPR-1290]]. Tab from the notes with a thread open enters
		/// its first post's text, where the conversation is read.
		auto focusNextPrevChild(bool aNext) -> bool override;

	private:
		struct closed_measure
		{
			QString Body;
			bool    Resolved = false;
			bool    Draft    = false;
			int     Height   = 0;
			/// How many lines of the body the note shows: one when it fits,
			/// otherwise two (one when resolved), the last elided.
			int Lines = 0;
		};
		struct measurement_generation
		{
			int                                   Width = -1;
			std::map<std::string, closed_measure> Notes;
		};

		void               index_retained();
		void               measure_closed(thread_card& aCard, int aWidth);
		void               clear_measurements();
		void               forget_measurements_if(const std::function<bool(const std::string&)>& aForget);
		[[nodiscard]] auto aligned() const noexcept -> bool;
		[[nodiscard]] auto closed_measure_of(const thread_card& aCard) const -> const closed_measure*;
		void               place_cards(bool aForce);
		void               place_open_card();
		void               sync_edges();
		void               rebuild_open_card();
		void               measure_cards();
		bool               GeometryPending = false;
		void               choose(std::size_t aCard, bool aOpen);
		void               set_hovered(const std::string& aId);
		void               notify_marks();
		void               step_new(int aDirection);
		void               ensure_visible(const QRect& aViewportRect);
		/// \brief Brings what is being read in an open thread into view: the
		/// notes' own overflow in Discuss, the code (which the drawer follows)
		/// in Review, as a wheel over the notes would.
		void               reveal_reading(const QRect& aViewportRect);
		[[nodiscard]] auto card_index(const std::string& aId) const -> std::optional<std::size_t>;
		[[nodiscard]] auto to_content(QPoint aViewportPoint) const -> QPoint;
		[[nodiscard]] auto to_viewport(QRect aContentRect) const -> QRect;
		[[nodiscard]] auto handle_thread_key(QKeyEvent& aEvent, bool aTyping) -> bool;
		void               paint_closed(QPainter& aPainter, const thread_card& aCard, bool aHovered, bool aFocused) const;

		std::vector<application::github_conversation_projection> Retained;
		/// The file identity of each retained node's path, index for index,
		/// derived once per adopted forest rather than once per relayout.
		std::vector<core::file_identity> RetainedFiles;
		/// [[BPR-0972]]. Per adopted forest: the first node for each identity,
		/// each node's root and depth, and every node by its root's file, so a
		/// layout scoped to one file visits that file's conversations only.
		/// [[BPR-1362]]. Keyed by value: a view into `Retained` dangled once
		/// `Retained` was replaced, and MSVC's clear() re-hashes its keys.
		std::unordered_map<std::string, std::size_t>             RetainedById;
		std::vector<std::size_t>                                 RetainedRoot;
		std::vector<std::size_t>                                 RetainedDepth;
		std::vector<std::pair<core::file_identity, std::size_t>> RetainedByFile;
		/// [[BPR-1083]]. Each retained node's mark, header length and
		/// ([[BPR-1086]]) lead, read once per adopted forest from the accepted
		/// body.
		std::vector<feedback_body_mark> RetainedMarks;
		std::vector<qsizetype>          RetainedHeaders;
		std::vector<qsizetype>          RetainedLeads;
		feedback_mark_theme             MarkTheme;
		/// [[BPR-0972]]. The first read state of each thread.
		std::unordered_map<std::string, std::size_t>                ReadStateByThread;
		mutable std::optional<std::vector<std::string>>             UnreadThreads;
		std::uint64_t                                               LayoutNodesVisited = 0U;
		std::vector<thread_card>                                    Cards;
		std::optional<core::file_identity>                          AnchorFile;
		std::optional<core::review_comment_draft>                   Draft;
		std::vector<core::review_comment_draft>                     SavedDrafts;
		std::optional<core::file_comparison_digest>                 CurrentComparison;
		card_line_locator                                           Locator;
		card_thread_dispatch                                        ThreadDispatch;
		card_action_dispatch                                        ActionDispatch;
		defect_badges                                               DefectBadges;
		std::function<void(QMenu&, const std::string&)>             ContextMenuBuilder;
		std::function<void()>                                       MarksObserver;
		std::function<bool(QWheelEvent&)>                           ScrollForward;
		std::function<void(const std::string&, const std::string&)> LinkDispatch;
		std::function<void(const std::string&, const QString&)>     ReplyTextDispatch;
		std::optional<thread_reply_presentation>                    Reply;
		/// [[BPR-1212]]. My draft is open and being written in the drawer.
		bool                                       Writing = false;
		std::optional<draft_composer_presentation> Composer;
		std::function<void(const QString&)>        DraftTextDispatch;
		QString                                    Viewer;
		std::string                                Current;
		std::string                                Open;
		/// [[BPR-1293]]. The open thread's picked post, and the one a pick
		/// is about to open with.
		std::string Picked;
		std::string PickedNext;
		bool        RevealPicked      = false;
		QTimer*     PickedReveal      = nullptr;
		int         PickedRevealSteps = 0;
		int         PickedRevealLast  = 0;
		std::string Hovered;
		/// The thread whose reply editor is shown in its open card.
		std::string                    Replying;
		std::vector<thread_read_state> ReadState;
		bool                           ReadStateKnown    = false;
		bool                           Feed              = false;
		bool                           ResolutionOffered = false;
		bool                           HasFocus          = false;
		discuss_filters                Filters;
		QString                        PullRequestAuthor;
		card_excerpt_locator           ExcerptLocator;
		std::function<void()>          LayoutObserver;
		QFont                          CodeFont;
		QColor                         CodeBackground{ 0xF6, 0xF8, 0xFA };
		QColor                         CodeInk{ 0x1F, 0x23, 0x28 };
		cards_scope                    Scope = cards_scope::this_file;
		QColor                         Accent{ 0x09, 0x69, 0xDA };
		QColor                         Marker{ 0xD2, 0x99, 0x22 };
		int                            ContentHeight = 0;
		std::uint64_t                  Layouts       = 0U;
		/// [[BPR-0673]]. Two widths, because a scroll bar that comes and goes
		/// alternates the content width; keeping one width re-measured every
		/// note of a large review each time it did.
		measurement_generation        Measurements;
		measurement_generation        PreviousMeasurements;
		QFont                         MeasurementFont;
		std::uint64_t                 MeasurementCount = 0U;
		open_thread_card*             OpenCard         = nullptr;
		std::vector<QAbstractButton*> Chips;
		std::size_t                   ChipsShown = 0U;
		QWidget*                      BandAbove  = nullptr;
		QWidget*                      BandBelow  = nullptr;
	};
} // namespace bpr::qt

#endif
