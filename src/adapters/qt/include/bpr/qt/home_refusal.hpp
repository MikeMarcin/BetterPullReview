#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_REFUSAL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_REFUSAL_HPP

#include "bpr/core/pull_request_refusal.hpp"

#include <QSize>
#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <functional>
#include <vector>

class QEvent;
class QHBoxLayout;
class QLabel;
class QPaintEvent;
class QPushButton;
class QResizeEvent;

namespace bpr::qt {
	/// \brief [[BPR-1200]]. A refused pull request, under the Home bar that named it.
	///
	/// \details The owner found the refusal of a private pull request "too
	/// subtle and easy to miss": one line of helper text. This is a card of
	/// its own between the bar and whatever Home shows next: the style's
	/// warning icon, a heading that names the pull request, a sentence that
	/// says why in terms of the connection, and the steps that could open it,
	/// with Copy details last. The core decides every word and which steps are
	/// offered; this widget lays them out, paints the card from the palette and
	/// reports a pressed step through its dispatch. It performs nothing itself.
	class home_refusal final : public QWidget
	{
	public:
		using action_dispatch = std::function<void(core::pull_request_refusal_action)>;

		explicit home_refusal(QWidget* aParent = nullptr);

		/// \brief Shows the core's words and steps, in its order.
		void present(const core::pull_request_refusal_wording& aWording);
		void set_action_dispatch(action_dispatch aDispatch);
		void set_copy_dispatch(std::function<void()> aDispatch);

		/// \brief The heading, then the explanation: what is announced and copied.
		[[nodiscard]] auto text() const -> QString;
		[[nodiscard]] auto heading_label() const noexcept -> QLabel*;
		[[nodiscard]] auto explanation_label() const noexcept -> QLabel*;
		[[nodiscard]] auto icon_label() const noexcept -> QLabel*;
		/// \brief The control for a step, whether or not it is offered now.
		[[nodiscard]] auto action_button(core::pull_request_refusal_action aAction) const noexcept -> QPushButton*;
		/// \brief The offered steps' controls, in the order the core gave.
		[[nodiscard]] auto offered_buttons() const -> std::vector<QPushButton*>;
		[[nodiscard]] auto copy_details_button() const noexcept -> QPushButton*;
		/// \brief The height its words take wrapped at the card's own width.
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		void refresh_presentation();

		static constexpr std::size_t action_count = 3U;

		QLabel*                                Icon        = nullptr;
		QLabel*                                Heading     = nullptr;
		QLabel*                                Explanation = nullptr;
		QHBoxLayout*                           Buttons     = nullptr;
		std::array<QPushButton*, action_count> Actions{ };
		QPushButton*                           CopyDetails = nullptr;
		core::pull_request_refusal_wording     Wording;
		action_dispatch                        ActionDispatch;
		std::function<void()>                  CopyDispatch;
	};
}

#endif
