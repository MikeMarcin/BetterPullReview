#ifndef BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_OPEN_CARD_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_OPEN_CARD_HPP

#include "conversation_glyphs.hpp"

#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QAbstractButton>
#include <QColor>
#include <QFont>
#include <QString>
#include <QWidget>

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
		[[nodiscard]] auto mark_rect() const -> QRect;
		[[nodiscard]] auto rail_rect() const -> QRectF;
		/// \brief [[BPR-1581]]. How strongly the post's arrival is marked,
		/// from 1 to 0.
		void               set_arrival(qreal aStrength);
		[[nodiscard]] auto arrival() const noexcept -> qreal;
		/// \brief The body's height changed: a source switch, fonts.
		std::function<void()> HeightChanged;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void enterEvent(QEnterEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;
		auto event(QEvent* aEvent) -> bool override;

	private:
		[[nodiscard]] auto name_height() const -> int;
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
		bool                     Draft       = false;
		bool                     Writing     = false;
		bool                     Replying    = false;
		bool                     Applying    = false;
		bool                     Publishable = false;
		bool                     ImagesShown = true;
		int                      Notch       = 14;
		QWidget*                 Header      = nullptr;
		QWidget*                 Foot        = nullptr;
		note_button*             ReplyField  = nullptr;
		QWidget*                 Editor      = nullptr;
		QPlainTextEdit*          ReplyBox    = nullptr;
		feedback_label_controls* Labels      = nullptr;
		QLabel*                  ReplyStatus = nullptr;
		note_button*             Publish     = nullptr;
		/// [[BPR-1345]]. Start a review, Add review comment, or on a pending
		/// draft Update review comment; and where the draft stands.
		note_button*       ReviewAction = nullptr;
		note_button*       SplitAction  = nullptr;
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
	};
} // namespace bpr::qt

#endif
