#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CHAT_HEADER_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CHAT_HEADER_VIEW_HPP

#include "bpr/core/chat_header.hpp"

#include <QString>
#include <QWidget>

#include <functional>

class QHBoxLayout;
class QLabel;

/// \file
/// \brief The one header row of a chat with an agent (RFC-BPR-0130, header A
/// as the owner amended it).
/// \details The Agent tab, the tour's side chat, a local draft's discussion
/// and the side chat about one review item wear this row and no other: what
/// the chat is about on one line that ends in an ellipsis where it does not
/// fit, its place under it, then Show, the one conversation tool (History
/// where the chat keeps sessions, Copy conversation where it keeps none) and
/// Close. `core::plan_chat_header` decides which of them a chat has; this
/// widget draws that value and sends gestures. It holds no New session
/// button: New session is History's first item.
namespace bpr::qt {
	class quiet_push_button;
	class quiet_tool_button;

	class chat_header_view final : public QWidget
	{
	public:
		/// \brief The object names of the row's parts, each chat's own.
		struct names
		{
			QString Row;
			QString Subject;
			QString Place;
			QString Show;
			QString History;
			QString Copy;
			QString Close;
		};
		/// \brief What a chat's own tooltips say; the names and the glyphs
		/// are the row's.
		struct tips
		{
			/// What Show does here.
			QString Show;
			/// What closing keeps.
			QString Close;
		};
		struct dispatch
		{
			std::function<void()> Show;
			std::function<void()> CopyConversation;
			std::function<void()> Close;
		};

		chat_header_view(const names& aNames, QWidget* aParent);

		void set_dispatch(dispatch aDispatch);
		void set_tips(const tips& aTips);
		/// \brief Draws `aHeader`. An equal header changes nothing.
		void               present(const core::chat_header& aHeader);
		[[nodiscard]] auto header() const noexcept -> const core::chat_header& { return Header; }

		/// \brief The row's layout, for the margins the chat's own column
		/// gives it.
		[[nodiscard]] auto row() const noexcept -> QHBoxLayout* { return Row; }
		[[nodiscard]] auto subject() const noexcept -> QLabel* { return Subject; }
		[[nodiscard]] auto place() const noexcept -> QLabel* { return Place; }
		[[nodiscard]] auto show_button() const noexcept -> quiet_push_button* { return Show; }
		/// \brief History: the chat hangs its `chat_history_menu` on it.
		/// Hidden in a chat that keeps no sessions.
		[[nodiscard]] auto history_tool() const noexcept -> quiet_tool_button* { return History; }
		[[nodiscard]] auto copy_tool() const noexcept -> quiet_tool_button* { return Copy; }
		[[nodiscard]] auto close_tool() const noexcept -> quiet_tool_button* { return Close; }
		/// \brief The title as drawn: with its ellipsis where it is cut.
		[[nodiscard]] auto shown_title() const -> QString;
		[[nodiscard]] auto title_cut() const -> bool;
		/// \brief A glyph is painted in the ink of the palette it was made
		/// with: paints each again in this widget's palette.
		void repaint_glyphs();

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void fit();

		core::chat_header  Header;
		dispatch           Dispatch;
		tips               Tips;
		QString            ShowWords;
		bool               Presented = false;
		QHBoxLayout*       Row       = nullptr;
		QLabel*            Subject   = nullptr;
		QLabel*            Place     = nullptr;
		quiet_push_button* Show      = nullptr;
		quiet_tool_button* History   = nullptr;
		quiet_tool_button* Copy      = nullptr;
		quiet_tool_button* Close     = nullptr;
	};
}

#endif
