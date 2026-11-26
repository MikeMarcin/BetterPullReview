#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CHAT_NOTICE_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CHAT_NOTICE_VIEW_HPP

#include "bpr/core/chat_notice.hpp"

#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <functional>

class QLabel;
class QToolButton;
class QVBoxLayout;

/// \file
/// \brief The one notice of every chat with an agent (RFC-BPR-0130 stage 6,
/// AC08): draws a `core::chat_notice` and reports the choice pressed.
/// \details Three forms, each the look its notice had before it was shared:
/// a quiet line over the question field (a session carried over, one the
/// agent cannot pick up, a chat's own line), a strip in a card with a close
/// (a send refused), and a card with a title, its words, up to two choices
/// and, where it closes, a close (the pinned-revision card, a stopped run, a
/// run's report). What the notice says and offers is core's; the view draws
/// it and decides nothing. A chat may hang its own parts in the card, above
/// or below the choices (the Agent tab's ways to carry work, its stopped
/// run's log).
namespace bpr::qt {
	class quiet_push_button;

	class chat_notice_view final : public QWidget
	{
	public:
		/// \brief The object names of the notice's parts, each chat's own.
		struct names
		{
			QString Notice;
			QString Title;
			QString Words;
			QString Dismiss;
			QString First;
			QString Second;
		};
		/// \brief The gestures, each chat's to perform.
		struct dispatch
		{
			std::function<void(core::chat_notice_action)> Chose;
			std::function<void()>                         Dismissed;
		};

		chat_notice_view(core::chat_notice_form aForm, const names& aNames, QWidget* aParent);

		void set_dispatch(dispatch aDispatch);
		/// \brief Draws `aNotice`, and shows the view while it says anything.
		void present(const core::chat_notice& aNotice);
		/// \brief Says nothing, and hides.
		void               clear();
		[[nodiscard]] auto notice() const noexcept -> const core::chat_notice& { return Shown; }

		[[nodiscard]] auto form() const noexcept -> core::chat_notice_form { return Form; }
		[[nodiscard]] auto title_label() const noexcept -> QLabel* { return Title; }
		[[nodiscard]] auto words_label() const noexcept -> QLabel* { return Words; }
		[[nodiscard]] auto dismiss_button() const noexcept -> QToolButton* { return Dismiss; }
		/// \brief The card's first and second choice; null in the other forms.
		[[nodiscard]] auto choice_button(std::size_t aSlot) const noexcept -> quiet_push_button* { return aSlot < Choices.size() ? Choices[aSlot] : nullptr; }
		/// \brief The button the choice `aAction` stands on, if it shows.
		[[nodiscard]] auto button_for(core::chat_notice_action aAction) const -> quiet_push_button*;

		/// \brief A chat's own part of the card, above or below its choices.
		void add_above_choices(QWidget* aPart);
		void add_below_choices(QWidget* aPart);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		core::chat_notice_form                   Form;
		core::chat_notice                        Shown;
		dispatch                                 Dispatch;
		QLabel*                                  Title   = nullptr;
		QLabel*                                  Words   = nullptr;
		QToolButton*                             Dismiss = nullptr;
		std::array<quiet_push_button*, 2U>       Choices{ };
		std::array<core::chat_notice_action, 2U> Actions{ };
		QVBoxLayout*                             Column    = nullptr;
		int                                      ChoiceRow = -1;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CHAT_NOTICE_VIEW_HPP
