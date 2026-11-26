#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CHAT_COMPOSER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CHAT_COMPOSER_HPP

#include "bpr/core/chat_send.hpp"

#include <QString>
#include <QWidget>

#include <functional>

class QHBoxLayout;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;

/// \file
/// \brief The one composer of every chat with an agent (RFC-BPR-0130 stage
/// 2, AC05): the Agent tab's question field made a widget of its own, which
/// the tour's side chat and the side chat about one review item place too.
/// \details One rounded field: what the chat attaches (its chips) above the
/// words, the words, a thin rule, and one row with the chat's tools on the
/// left and Send at the right end. Return sends, Shift+Return starts a line,
/// Command+Return (Control+Return off macOS) sends the other way a chat
/// offers (steer for queue, queue for steer). Send and Stop follow
/// `core::decide_chat_send`, one rule for every chat: Send becomes Stop while
/// a turn runs and nothing typed could follow it, and Stop stands beside a
/// Send that queues or steers. `/name` completes the reviewer's skills. What
/// a send means stays each chat's: the composer reports gestures and decides
/// nothing.
namespace bpr::qt {
	class quiet_push_button;
	class skill_completer;

	/// \brief [[BPR-1995]]. A quiet pill a chat attaches above the words: the
	/// passage a tour chat quotes, the item an item's chat is about.
	class passage_chip final : public QWidget
	{
	public:
		using QWidget::QWidget;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};

	class chat_composer final : public QWidget
	{
	public:
		/// \brief The object names of the composer's parts, each chat's own.
		struct names
		{
			QString Field;
			/// The words' field, when the composer makes it.
			QString Text;
			QString Send;
			QString Stop;
		};
		/// \brief The gestures, each chat's to perform.
		struct dispatch
		{
			/// Send was pressed while it did not say Stop, or Return was
			/// pressed in the words; `aOpposite` is Command+Return (Control
			/// off macOS), the other follow-up for this one message.
			std::function<void(bool aOpposite)> Send;
			/// Stop, beside Send or in its place.
			std::function<void()> Stop;
		};

		/// \brief A composer with words of its own, or wrapped around
		/// `aText`, a field another widget made (the Agent tab's, until it
		/// holds no borrowed control).
		chat_composer(const names& aNames, QWidget* aParent, QPlainTextEdit* aText = nullptr);

		void set_dispatch(dispatch aDispatch);
		/// \brief Draws Send and Stop as core decided. Each chat enables Send
		/// and says in its tooltip what it does.
		void               present(const core::chat_send_controls& aControls);
		[[nodiscard]] auto controls() const noexcept -> const core::chat_send_controls& { return Controls; }

		[[nodiscard]] auto text() const noexcept -> QPlainTextEdit* { return Text; }
		[[nodiscard]] auto send_button() const noexcept -> QPushButton* { return Send; }
		[[nodiscard]] auto stop_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto skills() const noexcept -> skill_completer* { return Skills; }
		/// \brief Above the words: what the chat attaches.
		[[nodiscard]] auto chips() const noexcept -> QVBoxLayout* { return Chips; }
		/// \brief Under the words, over the rule.
		[[nodiscard]] auto under_text() const noexcept -> QVBoxLayout* { return UnderText; }
		/// \brief The send row's left end: the chat's tools.
		[[nodiscard]] auto tools() const noexcept -> QHBoxLayout* { return Tools; }
		/// \brief The send row itself: its tools, a stretch, Stop and Send.
		[[nodiscard]] auto send_row() const noexcept -> QHBoxLayout* { return Row; }
		/// \brief Under the send row.
		[[nodiscard]] auto under_row() const noexcept -> QVBoxLayout* { return UnderRow; }

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		bool eventFilter(QObject* aWatched, QEvent* aEvent) override;

	private:
		void restyle();

		dispatch                 Dispatch;
		core::chat_send_controls Controls;
		bool                     Focused   = false;
		bool                     Composing = false;
		QPlainTextEdit*          Text      = nullptr;
		QPushButton*             Send      = nullptr;
		quiet_push_button*       Stop      = nullptr;
		skill_completer*         Skills    = nullptr;
		QVBoxLayout*             Chips     = nullptr;
		QVBoxLayout*             UnderText = nullptr;
		QHBoxLayout*             Tools     = nullptr;
		QHBoxLayout*             Row       = nullptr;
		QVBoxLayout*             UnderRow  = nullptr;
	};
}

#endif
