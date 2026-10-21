#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CONVERSATION_FINDER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CONVERSATION_FINDER_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/overview_feedback.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/workbench_interaction.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QColor>
#include <QFont>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

class QAbstractButton;
class QLineEdit;
class QListView;
class QMenu;
class QStackedWidget;

/// \file
/// \brief [[BPR-1293]] [[BPR-1341]]. Threads: every thread in the pull request
/// that is on a file, grouped by file, for finding one and going to it.
/// \details Presentation only, drawn as the owner's accepted prototype v2
/// (`docs/design/BPR-1290-inline-drawer-v2`) draws its finder. It reads the
/// conversation projection the Comments drawer reads, the ledger's read state
/// and [[BPR-1087]]'s label lists, and decides nothing about any of them: a
/// pick is reported, and the window turns it into the file selection and the
/// thread opening that already exist. [[BPR-1356]]: a pick names the
/// destination its gesture asked for, Review or Discuss, and the core
/// decides where it opens.
namespace bpr::qt {
	/// \brief The outline beside a branch row: one cell per level.
	enum class finder_connector : std::uint8_t {
		blank,
		/// The line continues past this row to a later sibling.
		pipe,
		/// This row is a branch and a later sibling follows.
		tee,
		/// This row is the last branch of its parent.
		end,
	};

	/// \brief One post as the finder shows it.
	struct finder_post
	{
		std::string Id;
		QString     Author;
		/// The body as written, for search and the preview.
		QString Body;
		/// One line of the body after its recognized header.
		QString                             Words;
		std::optional<core::feedback_label> Label;
		feedback_mark_emphasis              Emphasis;
		/// The post it answers, by index in its conversation; none for the
		/// first post.
		std::optional<std::size_t> Parent;
		bool                       Unread = false;
		bool                       Mine   = false;
	};

	/// \brief One row of a conversation: a line of replies.
	struct finder_row
	{
		std::size_t                   Level = 0U;
		std::vector<finder_connector> Connectors;
		/// The row's posts, by index, in posting order. The first is the one
		/// the row starts with.
		std::vector<std::size_t> Posts;
		/// Rows start beneath this one.
		bool Forks = false;
	};

	/// \brief One conversation on a file.
	struct finder_conversation
	{
		/// The thread root's identity; `draft:unpublished` for my draft.
		std::string              Id;
		QString                  Path;
		std::uint32_t            Line     = 0U;
		bool                     Resolved = false;
		bool                     Draft    = false;
		std::vector<finder_post> Posts;
		std::vector<finder_row>  Rows;
		std::size_t              Unread = 0U;
	};

	/// \brief The rows a conversation draws as.
	/// \details A conversation is one row while it is a line of replies. A
	/// reply nobody answered stays on its parent's row; where a post has two
	/// or more replies that have replies of their own, each of those starts a
	/// row one step in, under it. So a GitHub thread, whose replies all answer
	/// its first comment, is always one row. Each row's posts are in posting
	/// order. A post whose parent is not in the conversation answers the first.
	[[nodiscard]] auto finder_rows_of(const std::vector<finder_post>& aPosts) -> std::vector<finder_row>;

	/// \brief The conversations on a file, from the drawer's forest and the
	/// ledger's read state, in path order and then line order.
	/// \details Pull request comments and review summaries, which are not on
	/// a file, are not listed: Discuss reads them. Until the read state is
	/// known nothing is new.
	[[nodiscard]] auto finder_conversations_of(const std::vector<application::github_conversation_projection>& aForest,
											   const std::vector<thread_read_state>& aReadState, bool aReadStateKnown,
											   const QString& aViewer) -> std::vector<finder_conversation>;

	/// \brief What the finder shows: the search and its three filters.
	struct finder_filters
	{
		/// Words, a person, a file or a line number.
		QString Search;
		/// Only conversations with a post newer than what I have read.
		bool OnlyNew = false;
		/// Hide resolved conversations.
		bool HideResolved = false;
		/// Only conversations whose label this is ([[BPR-1087]]'s counting).
		std::optional<core::feedback_label> Label;

		auto operator==(const finder_filters&) const -> bool = default;
	};

	/// \brief Whether one post is found by a search, lowercase and trimmed.
	[[nodiscard]] auto finder_post_found(const finder_post& aPost, const QString& aSearch) -> bool;

	/// \brief My unpublished comment, shown with a lock on its file's group.
	struct finder_draft
	{
		core::file_identity File;
		/// One-based.
		std::uint32_t Line = 0U;
		QString       Body;
		QString       Author;

		auto operator==(const finder_draft&) const -> bool = default;
	};

	/// \brief Where Review is: the file it shows, the thread open in its
	/// drawer and the post a pick named in it.
	struct finder_reading
	{
		std::optional<core::file_identity> File;
		std::string                        Thread;
		std::string                        Post;

		auto operator==(const finder_reading&) const -> bool = default;
	};

	/// \brief Where a conversation is: its root, one of its posts, its file.
	struct finder_location
	{
		std::string Thread;
		std::string Post;
		QString     Path;

		auto operator==(const finder_location&) const -> bool = default;
	};

	/// \brief A pick: the conversation, the post to open it at, and where the
	/// gesture asked for it ([[BPR-1356]]): a click, Return and Open in
	/// Review ask for Review; the row's Discuss button, Shift+Return and Open
	/// in Discuss ask for Discuss.
	using finder_pick_dispatch = std::function<void(const finder_location&, core::workbench_thread_destination)>;

	/// \brief [[BPR-1582]]. The row menu's Mark as read (true) or Mark as
	/// unread (false) for one thread, by its root's identity.
	using finder_read_dispatch = std::function<void(const std::string&, bool)>;

	class finder_model;
	class finder_list;
	class finder_chip;
	class finder_preview;

	/// \brief The Threads page.
	class conversation_finder final : public QWidget
	{
	public:
		explicit conversation_finder(QWidget* aParent = nullptr);
		~conversation_finder() override;

		conversation_finder(const conversation_finder&)                    = delete;
		auto operator=(const conversation_finder&) -> conversation_finder& = delete;
		conversation_finder(conversation_finder&&)                         = delete;
		auto operator=(conversation_finder&&) -> conversation_finder&      = delete;

		/// \brief Adopts the drawer's forest. Rebuilt when next shown.
		void adopt(const std::vector<application::github_conversation_projection>& aForest);
		void clear();
		/// \brief The ledger's read state, as the drawer adopts it.
		void set_read_state(std::vector<thread_read_state> aState);
		void set_thread_read_state(const std::string& aThread, std::vector<std::string> aUnreadPosts);
		void set_viewer_login(const QString& aLogin);
		/// \brief [[BPR-1087]]'s conversations per label; none before counts.
		void set_labels(std::optional<application::conversation_labels_projection> aLabels);
		void set_label_marks(const feedback_mark_theme& aTheme);
		/// \brief The blue of what is new and the amber of where you are.
		void set_colours(const QColor& aNew, const QColor& aRoute);
		void set_code_font(const QFont& aFont);
		void set_draft(std::optional<finder_draft> aDraft);
		void set_reading(finder_reading aReading);
		/// \brief Shown as the page: the search has focus and the cursor is on
		/// the conversation open in Review, or kept, or on the first row.
		void enter();
		/// \brief [[BPR-1586]]. Puts the cursor on a thread's row, at \p aPost
		/// when the row holds it, scrolled into view with the keyboard on the
		/// list. A filter or a folded file that hides the row is cleared, and
		/// \p aCleared says so. False when the finder has no such thread.
		auto reveal(const std::string& aThread, const std::string& aPost, bool* aCleared = nullptr) -> bool;
		void set_pick_dispatch(finder_pick_dispatch aDispatch);
		/// \brief [[BPR-1582]]. Where the row menu's Mark as read and Mark as
		/// unread go.
		void set_read_dispatch(finder_read_dispatch aDispatch);
		/// \brief [[BPR-1699]]. Where the row menu's Mark All from an
		/// author as Read goes, with the GitHub login, and how many of that
		/// author's posts on the pull request are new; without a count the
		/// rows' own posts are counted.
		void set_author_read_dispatch(std::function<void(const std::string&)> aDispatch, std::function<std::size_t(const QString&)> aUnreadCount = { });
		/// \brief [[BPR-1584]]. How wide the rows grow on a wide page.
		void               set_reading_width(core::reading_width aWidth);
		[[nodiscard]] auto reading_width() const noexcept -> core::reading_width;
		/// \brief Esc on an empty search.
		void set_leave_dispatch(std::function<void()> aDispatch);

		[[nodiscard]] auto filters() const noexcept -> const finder_filters&;
		void               set_filters(finder_filters aFilters);
		[[nodiscard]] auto conversations() const -> const std::vector<finder_conversation>&;
		/// \brief The shown rows' keys, in order: `file:<path>` for a file's
		/// header, `row:<thread>/<first post>` for a conversation's row.
		[[nodiscard]] auto shown_keys() const -> std::vector<std::string>;
		/// \brief The row under the keyboard cursor, by key.
		[[nodiscard]] auto cursor_key() const -> std::string;
		/// \brief The post under the keyboard cursor on its row, or empty.
		[[nodiscard]] auto cursor_post() const -> std::string;
		/// \brief The beads a row shows, by post identity; `+n` is not a bead.
		[[nodiscard]] auto shown_beads(const std::string& aRowKey) const -> std::vector<std::string>;
		/// \brief How many read posts a row folds into its `+n`.
		[[nodiscard]] auto folded_posts(const std::string& aRowKey) const -> std::size_t;
		/// \brief A row's rectangle and one bead's, in the list's viewport.
		[[nodiscard]] auto row_rect(const std::string& aRowKey) const -> QRect;
		[[nodiscard]] auto bead_rect(const std::string& aRowKey, const std::string& aPost) const -> QRect;
		/// \brief [[BPR-1356]]. The row's Discuss button, in the list's
		/// viewport, while it shows: on the row under the pointer and on the
		/// cursor's row, never on a file's header or my draft. Empty otherwise.
		[[nodiscard]] auto discuss_button_rect(const std::string& aRowKey) const -> QRect;
		/// \brief [[BPR-1582]]. The row's Review button, beside its Discuss
		/// button and shown with it; on my draft, alone. Empty otherwise.
		[[nodiscard]] auto review_button_rect(const std::string& aRowKey) const -> QRect;
		/// \brief [[BPR-1582]]. A row's people, counts and state: the
		/// rectangle from its first bead to its right end.
		[[nodiscard]] auto trailing_rect(const std::string& aRowKey) const -> QRect;
		/// \brief [[BPR-1356]]. The row menu while it is open, or null: Open in
		/// Review, the default, and Open in Discuss, each showing its key. It
		/// and its items are made for each opening and go when it closes, as
		/// the code's menu does, so its keys are never a window's.
		[[nodiscard]] auto row_menu() const noexcept -> QMenu*;
		[[nodiscard]] auto open_in_review_action() const noexcept -> QAction*;
		[[nodiscard]] auto open_in_discuss_action() const noexcept -> QAction*;
		/// \brief [[BPR-1582]]. Mark as read or Mark as unread, as the
		/// thread's read state decides, while the menu is open.
		[[nodiscard]] auto read_action() const noexcept -> QAction*;
		/// \brief [[BPR-1699]]. Mark All from the row's or the picked
		/// post's author as Read, while the menu is open; none for my post.
		[[nodiscard]] auto author_read_action() const noexcept -> QAction*;
		/// \brief Opens the row menu for one row at a point of the list's
		/// viewport, the post under the point picked when it is a bead.
		void popup_row_menu(const std::string& aRowKey, QPoint aPosition);
		/// \brief How many conversations each label counts among those shown.
		[[nodiscard]] auto label_counts() const -> std::map<core::feedback_label, std::size_t>;
		/// \brief The accessible text of one row.
		[[nodiscard]] auto row_text(const std::string& aRowKey) const -> QString;
		[[nodiscard]] auto search_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto only_new_button() const noexcept -> QAbstractButton*;
		[[nodiscard]] auto hide_resolved_button() const noexcept -> QAbstractButton*;
		[[nodiscard]] auto label_button() const noexcept -> QAbstractButton*;
		[[nodiscard]] auto list() const noexcept -> QListView*;
		/// \brief The preview of one post, while it is shown.
		[[nodiscard]] auto preview() const noexcept -> QWidget*;
		/// \brief Where a conversation on a file is, named by its root or by
		/// one of its posts; the root names its first post. Nothing for a
		/// conversation that is absent. Pathless comments have an empty Path.
		[[nodiscard]] auto locate(const std::string& aIdentity) -> std::optional<finder_location>;
		/// \brief Rebuilds now if anything changed, shown or not.
		void refresh();
		/// \brief Times the conversations were rebuilt from a forest.
		[[nodiscard]] auto rebuilds() const noexcept -> std::uint64_t;

	protected:
		void changeEvent(QEvent* aEvent) override;
		void showEvent(QShowEvent* aEvent) override;
		void hideEvent(QHideEvent* aEvent) override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		friend class finder_model;
		friend class finder_list;
		friend class finder_delegate;

		struct item;
		struct row_geometry;
		struct bead;

		void               rebuild();
		void               apply_filters(bool aKeepCursor = true);
		[[nodiscard]] auto matches(const finder_conversation& aConversation) const -> bool;
		[[nodiscard]] auto label_members() const -> const std::set<std::string>*;
		void               refresh_bar();
		void               refresh_colours();
		void               open_label_menu();
		void               pick(std::size_t aItem, std::optional<std::size_t> aBead, core::workbench_thread_destination aDestination);
		[[nodiscard]] auto discuss_shown(std::size_t aItem) const -> bool;
		[[nodiscard]] auto review_shown(std::size_t aItem) const -> bool;
		void               popup_menu(std::size_t aItem, std::optional<std::size_t> aBead, QPoint aGlobal);
		void               toggle_fold(const QString& aPath);
		void               move_bead(int aStep);
		void               show_preview(std::size_t aItem, std::size_t aPost, const QRect& aBead);
		void               hide_preview();
		[[nodiscard]] auto handle_key(QKeyEvent& aEvent, bool aInSearch) -> bool;
		[[nodiscard]] auto item_index(const std::string& aKey) const -> std::optional<std::size_t>;
		[[nodiscard]] auto item_key(std::size_t aItem) const -> std::string;
		[[nodiscard]] auto beads_of(std::size_t aItem) const -> std::vector<bead>;
		[[nodiscard]] auto geometry_of(std::size_t aItem, const QRect& aRect) const -> row_geometry;
		[[nodiscard]] auto accessible_text(std::size_t aItem) const -> QString;
		[[nodiscard]] auto path_posts() const -> std::set<std::size_t>;
		[[nodiscard]] auto open_conversation() const -> std::optional<std::size_t>;
		void               paint_item(QPainter& aPainter, std::size_t aItem, const QRect& aRect, bool aHovered, bool aFocused) const;
		void               paint_header(QPainter& aPainter, const QString& aPath, const QRect& aRect, bool aHovered, bool aFocused) const;

		std::vector<application::github_conversation_projection>   Forest;
		std::vector<thread_read_state>                             ReadState;
		bool                                                       ReadStateKnown = false;
		QString                                                    Viewer;
		std::optional<application::conversation_labels_projection> Labels;
		std::map<core::feedback_label, std::set<std::string>>      LabelMembers;
		feedback_mark_theme                                        MarkTheme;
		std::optional<finder_draft>                                Draft;
		finder_reading                                             Reading;
		QColor                                                     New{ 0x2F, 0x6F, 0xD6 };
		QColor                                                     Route{ 0xC9, 0x8F, 0x12 };
		QFont                                                      CodeFont;
		bool                                                       Dirty    = true;
		std::uint64_t                                              Rebuilds = 0U;

		std::vector<finder_conversation> Conversations;
		/// Every item the conversations could show, in order; the model holds
		/// the shown subsequence.
		std::vector<std::unique_ptr<item>> Items;
		finder_filters                     Filters;
		std::set<QString>                  Folded;
		std::set<std::string>              Unfolded;
		std::string                        Cursor;
		int                                Bead = -1;
		std::optional<std::size_t>         HoveredItem;
		std::optional<std::size_t>         HoveredBead;
		/// [[BPR-1356]]. The pointer is on the hovered row's Discuss button.
		bool HoveredDiscuss = false;
		/// [[BPR-1582]]. The pointer is on the hovered row's Review button.
		bool HoveredReview = false;
		/// [[BPR-1356]]. The row menu, and the row and post it was opened on.
		QPointer<QMenu>            RowMenu;
		QPointer<QAction>          OpenReview;
		QPointer<QAction>          OpenDiscuss;
		QPointer<QAction>          ReadAction;
		QPointer<QAction>          AuthorReadAction;
		std::string                MenuRow;
		std::optional<std::size_t> MenuBead;

		finder_pick_dispatch                       PickDispatch;
		finder_read_dispatch                       ReadDispatch;
		std::function<void(const std::string&)>    AuthorReadDispatch;
		std::function<std::size_t(const QString&)> AuthorUnreadCount;
		core::reading_width                        ReadingWidth = core::default_reading_width;
		std::function<void()>                      LeaveDispatch;

		QWidget*        Bar          = nullptr;
		QLineEdit*      Search       = nullptr;
		finder_chip*    OnlyNew      = nullptr;
		finder_chip*    HideResolved = nullptr;
		finder_chip*    LabelChip    = nullptr;
		QStackedWidget* Pages        = nullptr;
		finder_list*    List         = nullptr;
		finder_model*   Model        = nullptr;
		QWidget*        Empty        = nullptr;
		QWidget*        NoneOnFile   = nullptr;
		finder_preview* Preview      = nullptr;
	};
} // namespace bpr::qt

#endif
