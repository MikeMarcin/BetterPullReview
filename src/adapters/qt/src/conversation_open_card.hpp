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

class QLabel;
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

	/// \brief An author's muted hue: background and initials.
	struct avatar_colours
	{
		QColor Background;
		QColor Ink;
	};
	[[nodiscard]] auto avatar_colours_of(const QString& aLogin, bool aDark) -> avatar_colours;
	/// \brief "tbutler" is T; "reviewer-four" is RF.
	[[nodiscard]] auto avatar_initials(const QString& aLogin) -> QString;
	/// \brief An 18 px avatar (14 px small), ringed when it is mine.
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
		/// [[BPR-1293]]. The post a pick in Conversations named: washed amber.
		std::string Picked;
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
		};
		note_button(kind aKind, const QString& aText, QWidget* aParent);
		void               set_style(const open_card_style& aStyle);
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		kind            Kind;
		open_card_style Style;
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
		/// \brief [[BPR-1212]]. What the draft's suggestion block changes, as
		/// drawn under its field; hidden without a block.
		[[nodiscard]] auto change_view() const noexcept -> QWidget*;
		[[nodiscard]] auto reply_editor() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto reply_field() const noexcept -> QAbstractButton*;
		/// \brief The reply's label chip, menu and shortcodes: the composer's
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
		std::function<void(const QString&)>                     ReplyText;
		/// [[BPR-1212]]. Each edit of my draft written in the composer.
		std::function<void(const QString&)> DraftText;
		std::function<void()>               HeightChanged;
		std::function<void()>               CloseRequested;
		/// \brief The thread's actions, as a menu at a global point.
		std::function<void(QPoint)> MoreRequested;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void build(const thread_card& aCard);
		void build_composer(const thread_card& aCard, QVBoxLayout& aFoot);
		void refresh_reply_controls();

		std::string              Id;
		open_card_style          Style;
		bool                     Draft       = false;
		bool                     Writing     = false;
		bool                     Replying    = false;
		bool                     Applying    = false;
		bool                     Publishable = false;
		int                      Notch       = 14;
		QWidget*                 Header      = nullptr;
		QWidget*                 Foot        = nullptr;
		note_button*             ReplyField  = nullptr;
		QWidget*                 Editor      = nullptr;
		QPlainTextEdit*          ReplyBox    = nullptr;
		feedback_label_controls* Labels      = nullptr;
		QLabel*                  ReplyStatus = nullptr;
		note_button*             Publish     = nullptr;
		suggestion_mark*         Suggestion  = nullptr;
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
