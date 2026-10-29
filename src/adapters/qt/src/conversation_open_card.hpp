#ifndef BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_OPEN_CARD_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_OPEN_CARD_HPP

#include "conversation_glyphs.hpp"

#include "bpr/core/conversation_reactions.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QAbstractButton>
#include <QColor>
#include <QFont>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QBoxLayout;
class QLabel;
class QResizeEvent;
class QVBoxLayout;
class QPalette;
class QPlainTextEdit;
class QToolButton;

namespace bpr::qt {
	class feedback_label_controls;
	class markdown_body_view;
	class reaction_bar;
	class reaction_picker;
	class suggestion_mark;

	/// \brief [[BPR-1290]]. The colours the notes are drawn in, derived from
	/// the palette on screen and the appearance profile's unread and marker
	/// colours.
	struct note_colours
	{
		QColor Ink;
		QColor Ink2;
		QColor Faint;
		QColor Rule;
		QColor Rule2;
		QColor Panel;
		QColor Drawer;
		QColor Hover;
		QColor New;
		QColor OnNew;
		QColor Route;
		QColor RouteSoft;
		QColor RouteLine;
		QColor Ok;
		QColor Signal;
		QColor CodeBackground;
		QColor CodeInk;
		bool   Dark = false;
	};
	[[nodiscard]] auto note_colours_of(const QPalette& aPalette, const QColor& aNew, const QColor& aRoute) -> note_colours;

	/// \brief An 18 px avatar (14 px small), ringed when it is mine: the shared
	/// `paint_identity_avatar` ([[BPR-1583]]) over the notes' grounds.
	void paint_avatar(QPainter& aPainter, const QRectF& aBox, const QString& aLogin, bool aMine, const note_colours& aColours, const QFont& aFont);

	/// \brief [[BPR-1803]]. The name a post's author is shown by: the GitHub
	/// login, mine as anyone else's, never "you"; "Unknown author", as the
	/// other surfaces say, while no login is known.
	[[nodiscard]] auto shown_author(const QString& aLogin) -> QString;

	/// \brief What the open card draws with.
	struct open_card_style
	{
		feedback_mark_theme Marks;
		note_colours        Colours;
		QFont               CodeFont;
		QString             Viewer;
		bool                ResolutionOffered = false;
		bool                Feed              = false;
		bool                HasCodeAnchor     = false;
		/// [[BPR-1581]]. The thread is on a file, so Review has a place for it.
		bool OnFile = false;
		/// [[BPR-1293]]. The post a pick in Threads named: washed amber.
		std::string Picked;
		/// [[BPR-1562]]. The review panes' colours for fenced code.
		std::optional<markdown_code_theme> CodeTheme;
		/// [[BPR-1564]]. A comment on lines: how its suggestions show.
		std::optional<markdown_suggestion_presentation> Suggestions;
		/// [[BPR-1775]]. Edit is offered on my own published comments.
		bool CommentEditOffered = false;
		/// [[BPR-1806]]. Open in tour is offered on a published tour stop.
		bool TourOpenOffered = false;
		/// [[BPR-1802]]. The pull request's repository, whose permalinks show
		/// as snippet boxes.
		std::optional<core::markdown_repository> PermalinkRepository;
	};

	/// \brief [[BPR-1806]]. A published tour stop's plate, over its
	/// narrative: the tour's numbered route marker, the stop's title, "Stop 6
	/// of 9", and Open in tour when the window has the tour to follow. The
	/// route marker's line runs up when a stop comes before and down when one
	/// follows, as the stops sit on the tour's route.
	///
	/// \details [[BPR-1810]]. A stop of a longer tour steps along it: "Stop 6
	/// of 9" sits in a small pill between a previous and a next chevron, each
	/// named for the stop it reaches and unavailable at the tour's end or
	/// while that stop is not among the comments at hand.
	class tour_stop_plate final : public QWidget
	{
	public:
		tour_stop_plate(const core::published_tour_stop& aStop, std::optional<std::size_t> aCount, const std::optional<tour_stop_neighbour>& aPrevious,
						const std::optional<tour_stop_neighbour>& aNext, const note_colours& aColours, bool aOpenOffered, QWidget* aParent);
		[[nodiscard]] auto hasHeightForWidth() const -> bool override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		/// \brief The marker's box, for tests.
		[[nodiscard]] auto marker_rect() const -> QRectF;
		[[nodiscard]] auto open_button() const noexcept -> QAbstractButton*;
		/// \brief [[BPR-1810]]. The chevrons; null on a stop with no tour to
		/// step along (a tour of one).
		[[nodiscard]] auto previous_button() const noexcept -> QAbstractButton*;
		[[nodiscard]] auto next_button() const noexcept -> QAbstractButton*;
		/// \brief Open in tour pressed.
		std::function<void()> OpenRequested;
		/// \brief [[BPR-1810]]. A chevron pressed: true for Next.
		std::function<void(bool)> StepRequested;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void               place_children();
		[[nodiscard]] auto title_font() const -> QFont;
		[[nodiscard]] auto badge_font() const -> QFont;
		/// The badge row's top and the stepper's box, at the plate's width.
		[[nodiscard]] auto badge_top() const -> int;
		[[nodiscard]] auto stepper_rect() const -> QRect;

		core::published_tour_stop  Stop;
		std::optional<std::size_t> Count;
		note_colours               Colours;
		QAbstractButton*           Open     = nullptr;
		QAbstractButton*           Previous = nullptr;
		QAbstractButton*           Next     = nullptr;
	};

	/// \brief [[BPR-1290]]. A post's quiet glyph button in its name row:
	/// shown while the post is hovered or holds focus, or while it is on
	/// (the source being read). Focusable and named either way.
	class post_tool final : public QAbstractButton
	{
	public:
		post_tool(conversation_glyph aGlyph, const note_colours& aColours, QWidget* aParent);
		void               set_glyph(conversation_glyph aGlyph);
		void               set_revealed(bool aRevealed);
		void               set_on(bool aOn);
		[[nodiscard]] auto revealed() const noexcept -> bool;
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		conversation_glyph Glyph;
		note_colours       Colours;
		bool               Revealed = false;
		bool               On       = false;
	};

	/// \brief One post of the open thread: rail, avatar, mark, name row and
	/// its body, read through the shared Markdown reader in place
	/// (RFC-BPR-0036), with View source and Copy source in the name row.
	class thread_post_view final : public QWidget
	{
	public:
		thread_post_view(const card_post& aPost, const open_card_style& aStyle, bool aRuleAbove, QWidget* aParent);
		[[nodiscard]] auto hasHeightForWidth() const -> bool override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto reader() const noexcept -> markdown_body_view*;
		[[nodiscard]] auto source_button() const noexcept -> post_tool*;
		[[nodiscard]] auto copy_button() const noexcept -> post_tool*;
		[[nodiscard]] auto post() const noexcept -> const card_post&;
		void               present_read_state(bool aUnread, bool aRuleAbove);
		[[nodiscard]] auto mark_rect() const -> QRect;
		[[nodiscard]] auto rail_rect() const -> QRectF;
		/// \brief [[BPR-1757]]. The room the author's name is painted
		/// in, before the post's tools.
		[[nodiscard]] auto name_rect() const -> QRect;
		/// \brief [[BPR-1581]]. How strongly the post's arrival is marked,
		/// from 1 to 0.
		void               set_arrival(qreal aStrength);
		[[nodiscard]] auto arrival() const noexcept -> qreal;
		/// \brief The body's height changed: a source switch, fonts.
		std::function<void()> HeightChanged;
		/// \brief [[BPR-1741]]. The post's reactions as the viewer
		/// should see them; none when they were not read. Under the body,
		/// with Add reaction among the post's tools when GitHub lets the
		/// viewer react.
		void               set_reactions(const std::optional<core::reaction_summary>& aSummary);
		[[nodiscard]] auto reactions() const noexcept -> const std::optional<core::reaction_summary>&;
		[[nodiscard]] auto react_button() const noexcept -> post_tool*;
		[[nodiscard]] auto reaction_chips() const noexcept -> reaction_bar*;
		/// \brief Opens the picker under Add reaction; null when the viewer
		/// may not react.
		auto open_reaction_picker() -> reaction_picker*;
		/// \brief A reaction chosen in the picker or pressed under the body.
		std::function<void(core::reaction_content)> ReactionRequested;
		/// \brief [[BPR-1775]]. Offers Edit among the post's tools.
		void               set_editable(bool aEditable);
		[[nodiscard]] auto edit_button() const noexcept -> post_tool*;
		/// \brief [[BPR-1775]]. Edit pressed.
		std::function<void()> EditRequested;
		/// \brief [[BPR-1775]]. Puts \p aPanel (the editor) in the body's
		/// place, or the body back with none. The post keeps its name row.
		void               set_edit_panel(QWidget* aPanel);
		[[nodiscard]] auto edit_panel() const noexcept -> QWidget*;
		/// \brief [[BPR-1775]]. The editor's height changed.
		void refresh_edit_geometry();
		/// \brief [[BPR-1806]]. A published tour stop's plate; null otherwise.
		[[nodiscard]] auto tour_plate() const noexcept -> tour_stop_plate*;
		/// \brief [[BPR-1806]]. Open in tour pressed on the stop's plate.
		std::function<void()> TourOpenRequested;
		/// \brief [[BPR-1810]]. A chevron pressed on the stop's plate: true
		/// for Next.
		std::function<void(bool)> TourStepRequested;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void enterEvent(QEnterEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;
		auto event(QEvent* aEvent) -> bool override;

	private:
		[[nodiscard]] auto name_height() const -> int;
		/// \brief [[BPR-1806]]. The tour stop plate's height and its gaps at
		/// a text width; zero for any other post or while editing.
		[[nodiscard]] auto plate_height(int aWidth) const -> int;
		void               place_children();
		void               refresh_tools();
		void               refresh_source_button();

		card_post           Post;
		open_card_style     Style;
		bool                RuleAbove    = false;
		bool                Hovered      = false;
		qreal               Arrival      = 0.0;
		markdown_body_view* Reader       = nullptr;
		post_tool*          SourceButton = nullptr;
		post_tool*          CopyButton   = nullptr;
		/// [[BPR-1741]].
		post_tool*                            ReactButton = nullptr;
		reaction_bar*                         Reactions   = nullptr;
		std::optional<core::reaction_summary> Summary;
		/// [[BPR-1775]].
		post_tool* EditButton = nullptr;
		QWidget*   EditPanel  = nullptr;
		bool       Editable   = false;
		/// [[BPR-1806]].
		tour_stop_plate* TourPlate = nullptr;
	};

	/// \brief A button drawn as the prototype draws it: a filled signal pill
	/// for Publish, a quiet field for Reply.
	class note_button final : public QAbstractButton
	{
	public:
		enum class kind : std::uint8_t {
			primary,
			reply_field,
			/// [[BPR-1345]]. An outlined pill beside the primary one: Add
			/// single comment while a review is the draft's main action.
			secondary,
			/// [[BPR-1345]] v002. The primary pill's arrow, joined to its right
			/// edge, that opens the actions the narrow drawer folds away.
			split,
		};
		note_button(kind aKind, const QString& aText, QWidget* aParent);
		void set_style(const open_card_style& aStyle);
		/// \brief [[BPR-1345]] v002. A primary pill with its split arrow
		/// joined to its right edge squares that edge.
		void               set_joined(bool aJoined);
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		kind            Kind;
		open_card_style Style;
		bool            Joined = false;
	};

	/// \brief [[BPR-1290]]. The open thread, in place at its line: the header
	/// row, every post with its complete body, and the reply at its foot.
	class open_thread_card final : public QWidget
	{
	public:
		open_thread_card(const thread_card& aCard, const open_card_style& aStyle, QWidget* aParent);

		[[nodiscard]] auto id() const noexcept -> const std::string&;
		[[nodiscard]] auto hasHeightForWidth() const -> bool override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;
		/// \brief Shows the reply editor (true) or the Reply field (false).
		void               set_replying(bool aReplying);
		[[nodiscard]] auto replying() const noexcept -> bool;
		/// \brief The draft editor's reply for this thread.
		void present_reply(const std::optional<thread_reply_presentation>& aReply);
		/// \brief [[BPR-1212]]. What the draft editor holds for my draft, on
		/// the composer of a draft being written in the drawer.
		void present_draft(const std::optional<draft_composer_presentation>& aDraft);
		/// \brief [[BPR-1212]]. Whether this card is my draft's composer.
		[[nodiscard]] auto writing() const noexcept -> bool;
		/// \brief [[BPR-1345]]. What ⌘↩ does in the composer. v002 (owner,
		/// finish-review v001: "Start a review, add to review should be the
		/// default for cmd+enter"): Start a review with no review in
		/// progress, Add review comment while one is, and on a pending draft
		/// the close that keeps it pending. ⌘⇧↩ is Add single comment.
		[[nodiscard]] auto primary_action() const noexcept -> card_action;
		/// \brief [[BPR-1212]]. What the draft's suggestion block changes, as
		/// drawn under its field; hidden without a block.
		[[nodiscard]] auto change_view() const noexcept -> QWidget*;
		[[nodiscard]] auto reply_editor() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto reply_field() const noexcept -> QAbstractButton*;
		/// \brief The reply's label chip and menu: the composer's
		/// own [[BPR-1083]] controls on the reply's text. Null on a card that
		/// takes no reply.
		[[nodiscard]] auto label_controls() const noexcept -> feedback_label_controls*;
		/// \brief [[BPR-1084]]. Beside the reply's label chip: whether a
		/// suggested change in the reply is native in this thread or text only.
		[[nodiscard]] auto suggestion_mark_widget() const noexcept -> suggestion_mark*;
		[[nodiscard]] auto posts() const -> std::vector<thread_post_view*>;
		/// \brief [[BPR-1741]]. Shows each post's reactions as \p aLookup
		/// gives them for the post's identity, in place.
		void present_reactions(const std::function<std::optional<core::reaction_summary>(const std::string&)>& aLookup);
		/// \brief [[BPR-1741]]. Resolve conversation at the thread's
		/// end; null where the thread offers none.
		[[nodiscard]] auto resolve_conversation() const noexcept -> QAbstractButton*;
		/// \brief [[BPR-1741]]. A reaction asked for on one post.
		std::function<void(const std::string&, core::reaction_content)> Reaction;
		/// \brief [[BPR-1775]]. Shows the editor in place of the named
		/// post's body, or closes it with none. The editor's text is put only
		/// when it differs from what it holds, so typing is never disturbed.
		void present_comment_edit(const std::optional<comment_edit_presentation>& aEdit);
		/// \brief [[BPR-1775]]. The editor's parts, while it is shown.
		[[nodiscard]] auto comment_editor() const noexcept -> scoped_prose_editor*;
		[[nodiscard]] auto comment_edit_save_button() const noexcept -> QAbstractButton*;
		[[nodiscard]] auto comment_edit_cancel_button() const noexcept -> QAbstractButton*;
		[[nodiscard]] auto comment_edit_newer_button() const noexcept -> QAbstractButton*;
		[[nodiscard]] auto comment_edit_status() const noexcept -> QLabel*;
		/// \brief [[BPR-1775]]. Edit asked for on one post, from its tool or
		/// its body menu.
		std::function<void(const std::string&)> EditRequested;
		/// \brief [[BPR-1806]]. Open in tour pressed on a published tour
		/// stop: its post and the stop it was read as.
		std::function<void(const std::string&, const core::published_tour_stop&)> TourOpenRequested;
		/// \brief [[BPR-1810]]. A chevron pressed on the stop's plate: true
		/// for Next.
		std::function<void(bool)> TourStepRequested;
		/// \brief [[BPR-1775]]. The editor's raw text after each change.
		std::function<void(const std::string&)> EditText;
		/// \brief [[BPR-1775]]. Save (⌘↩), Cancel (Esc) and Edit GitHub's
		/// newer text, pressed in the editor.
		std::function<void()> EditSave;
		std::function<void()> EditCancel;
		std::function<void()> EditNewer;
		/// \brief Updates durable read marks without replacing readers or their menus.
		void present_read_state(const thread_card& aCard);
		/// \brief Whether \p aWatched is a post's rendered or source text,
		/// where the arrow keys move the reading caret.
		[[nodiscard]] auto is_reading(const QObject* aWatched) const -> bool;
		/// \brief Where Tab from the notes enters the thread: its first post's
		/// text. Null with no post.
		[[nodiscard]] auto reading_entry() const -> QWidget*;
		/// \brief The vertical centre of the notch, from the card's top.
		void               set_notch(int aY);
		[[nodiscard]] auto notch() const noexcept -> int;

		std::function<void(card_action)>                        Action;
		std::function<void(const std::string&, const QString&)> Link;
		/// RFC-BPR-0084. A published post's image references, by post identity.
		std::function<void(const std::string&, std::vector<core::markdown_image_reference>)> ImageRequest;
		/// \brief Shows one image answer in the post it names.
		void apply_image(const application::markdown_image_projection& aImage);
		/// \brief Asks again for every published post's images still loading.
		void request_images();
		/// \brief RFC-BPR-0084 Q3. Shows or hides every post's images, now and
		/// for posts built later.
		void set_images_shown(bool aShown);
		/// [[BPR-1562]]. Published posts' fenced code to classify.
		std::function<void(std::vector<application::markdown_code_syntax_request>)> CodeRequest;
		/// [[BPR-1802]]. Published posts' standing permalinks to read.
		std::function<void(std::vector<application::markdown_blob_snippet_request>)> PermalinkRequest;
		/// \brief Shows a permalink's lines in every post that holds it.
		void apply_permalink_snippet(const application::markdown_blob_snippet_publication& aSnippet);
		/// \brief Asks again for every published post's permalinks.
		void request_permalink_snippets();
		void set_permalink_repository(const std::optional<core::markdown_repository>& aRepository);
		/// \brief Colours every post showing the answer's code.
		void apply_code_syntax(const application::markdown_code_syntax_publication& aSpans);
		/// \brief Asks again for every published post's code.
		void                                request_code_syntax();
		void                                set_code_theme(const markdown_code_theme& aTheme);
		std::function<void(const QString&)> ReplyText;
		/// [[BPR-1212]]. Each edit of my draft written in the composer.
		std::function<void(const QString&)> DraftText;
		std::function<void()>               HeightChanged;
		std::function<void()>               CloseRequested;
		/// \brief The thread's actions, as a menu at a global point.
		std::function<void(QPoint)> MoreRequested;
		/// \brief [[BPR-1384]]. The exact comment or reply named by its body menu.
		std::function<void(const std::string&)> OpenInReviewRequested;
		/// \brief [[BPR-1581]]. The exact comment or reply named by its body
		/// menu in Review, to show in Discuss.
		std::function<void(const std::string&)> ShowInDiscussRequested;
		/// \brief [[BPR-1586]]. The same post, to show in Threads.
		std::function<void(const std::string&)> ShowInThreadsRequested;
		/// \brief [[BPR-1699]]. How many posts a login wrote that are
		/// new, over the pull request; none while the read state is unknown.
		std::function<std::optional<std::size_t>(const QString&)> AuthorUnread;
		/// \brief [[BPR-1699]]. A post's body menu's Mark all from its
		/// author as read, with the login.
		std::function<void(const QString&)> AuthorReadRequested;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		auto event(QEvent* aEvent) -> bool override;

	private:
		void build(const thread_card& aCard);
		void build_composer(const thread_card& aCard, QVBoxLayout& aFoot);
		void refresh_reply_controls();
		/// [[BPR-1345]] v002. One row: the privacy note, Delete and the two
		/// ways out side by side, or, where they do not fit, the main action
		/// with a split arrow that carries Add single comment.
		void fit_draft_actions();
		void present_review_state();
		void resizeEvent(QResizeEvent* aEvent) override;

		std::string              Id;
		open_card_style          Style;
		bool                     Draft               = false;
		bool                     Writing             = false;
		bool                     Replying            = false;
		bool                     Applying            = false;
		bool                     Publishable         = false;
		bool                     ImagesShown         = true;
		int                      Notch               = 14;
		QWidget*                 Header              = nullptr;
		QBoxLayout*              HeaderRow           = nullptr;
		QWidget*                 NewPill             = nullptr;
		QWidget*                 NewRule             = nullptr;
		QToolButton*             MarkRead            = nullptr;
		int                      NewPillPosition     = 0;
		std::size_t              UnreadCount         = 0U;
		QWidget*                 Foot                = nullptr;
		note_button*             ReplyField          = nullptr;
		note_button*             ResolveConversation = nullptr;
		QWidget*                 Editor              = nullptr;
		QPlainTextEdit*          ReplyBox            = nullptr;
		feedback_label_controls* Labels              = nullptr;
		QLabel*                  ReplyStatus         = nullptr;
		note_button*             Publish             = nullptr;
		/// [[BPR-1345]]. Start a review, Add review comment, or on a pending
		/// draft Update review comment; and where the draft stands.
		note_button* ReviewAction = nullptr;
		note_button* SplitAction  = nullptr;
		/// [[RFC-BPR-0090]]. Convert, shown in the sending buttons' place
		/// while GitHub cannot apply the suggestion; and whether the change
		/// written in the code is content even when the field is empty.
		note_button*       ConvertAction = nullptr;
		QString            ConvertWords;
		bool               CodeChange   = false;
		QBoxLayout*        DraftActions = nullptr;
		QWidget*           Privacy      = nullptr;
		draft_review_state Review       = draft_review_state::none;
		suggestion_mark*   Suggestion   = nullptr;
		/// [[BPR-1212]]. The composer's line button, state and change view.
		QToolButton* Where      = nullptr;
		QWidget*     DraftState = nullptr;
		QWidget*     Change     = nullptr;
		QString      LastNotice;
		/// What a reply here is attached to on GitHub ([[BPR-1084]]).
		core::suggestion_placement_observation Placement;
		std::vector<thread_post_view*>         Posts;
		/// [[BPR-1775]]. The comment editor and the post it is in.
		QWidget*             EditPanel = nullptr;
		std::string          EditPost;
		scoped_prose_editor* EditEditor       = nullptr;
		QLabel*              EditStatus       = nullptr;
		QAbstractButton*     EditSaveButton   = nullptr;
		QAbstractButton*     EditCancelButton = nullptr;
		QAbstractButton*     EditNewerButton  = nullptr;
		bool                 EditApplying     = false;
	};
} // namespace bpr::qt

#endif
