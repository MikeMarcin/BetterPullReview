#ifndef BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_OPEN_CARD_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_OPEN_CARD_HPP

#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QAbstractButton>
#include <QColor>
#include <QFont>
#include <QString>
#include <QTextEdit>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QLabel;
class QPalette;
class QPlainTextEdit;
class QToolButton;

namespace bpr::qt {
	class feedback_label_controls;

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
	};

	/// \brief One post's body as written: the recognized header in semibold,
	/// inline code in the code font, web links that open through the
	/// application's admission. Read-only and selectable.
	class thread_post_body final : public QTextEdit
	{
	public:
		thread_post_body(QWidget* aParent);
		void                                set_post(const card_post& aPost, const open_card_style& aStyle);
		[[nodiscard]] auto                  height_for(int aWidth) const -> int;
		std::function<void(const QString&)> LinkDispatch;

	protected:
		void mousePressEvent(QMouseEvent* aEvent) override;
		void mouseReleaseEvent(QMouseEvent* aEvent) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;

	private:
		QString Pressed;
	};

	/// \brief One post of the open thread: rail, avatar, mark, name row and body.
	class thread_post_view final : public QWidget
	{
	public:
		thread_post_view(const card_post& aPost, const open_card_style& aStyle, bool aRuleAbove, QWidget* aParent);
		[[nodiscard]] auto hasHeightForWidth() const -> bool override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto body() const noexcept -> thread_post_body*;
		[[nodiscard]] auto post() const noexcept -> const card_post&;
		[[nodiscard]] auto mark_rect() const -> QRect;
		[[nodiscard]] auto rail_rect() const -> QRectF;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		auto event(QEvent* aEvent) -> bool override;

	private:
		card_post         Post;
		open_card_style   Style;
		bool              RuleAbove = false;
		thread_post_body* Body      = nullptr;
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
		void               present_reply(const std::optional<thread_reply_presentation>& aReply);
		[[nodiscard]] auto reply_editor() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto reply_field() const noexcept -> QAbstractButton*;
		/// \brief The reply's label chip, menu and shortcodes: the composer's
		/// own [[BPR-1083]] controls on the reply's text. Null on a card that
		/// takes no reply.
		[[nodiscard]] auto label_controls() const noexcept -> feedback_label_controls*;
		[[nodiscard]] auto posts() const -> std::vector<thread_post_view*>;
		/// \brief The vertical centre of the notch, from the card's top.
		void               set_notch(int aY);
		[[nodiscard]] auto notch() const noexcept -> int;

		std::function<void(card_action)>                        Action;
		std::function<void(const std::string&, const QString&)> Link;
		std::function<void(const QString&)>                     ReplyText;
		std::function<void()>                                   HeightChanged;
		std::function<void()>                                   CloseRequested;
		/// \brief The thread's actions, as a menu at a global point.
		std::function<void(QPoint)> MoreRequested;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void build(const thread_card& aCard);
		void refresh_reply_controls();

		std::string                    Id;
		open_card_style                Style;
		bool                           Draft       = false;
		bool                           Replying    = false;
		bool                           Applying    = false;
		bool                           Publishable = false;
		int                            Notch       = 14;
		QWidget*                       Header      = nullptr;
		QWidget*                       Foot        = nullptr;
		note_button*                   ReplyField  = nullptr;
		QWidget*                       Editor      = nullptr;
		QPlainTextEdit*                ReplyBox    = nullptr;
		feedback_label_controls*       Labels      = nullptr;
		QLabel*                        ReplyStatus = nullptr;
		note_button*                   Publish     = nullptr;
		std::vector<thread_post_view*> Posts;
	};
} // namespace bpr::qt

#endif
