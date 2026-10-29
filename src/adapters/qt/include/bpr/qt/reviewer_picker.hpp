#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEWER_PICKER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEWER_PICKER_HPP

#include "bpr/core/reviewer_request.hpp"

#include <QFrame>

#include <cstdint>
#include <functional>
#include <string>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace bpr::qt {
	/// \brief [[BPR-1804]]. What the reviewer picker asks of the composition.
	/// The picker performs nothing: every gesture goes out as one of these.
	struct reviewer_picker_gesture
	{
		enum class kind : std::uint8_t {
			/// Request review was pressed.
			open,
			/// The search field's text changed to `Text`.
			query,
			/// The person `Text` (a login) was chosen.
			choose,
			/// The picker was dismissed.
			close,
		};

		kind        Kind = kind::open;
		std::string Text;

		auto operator==(const reviewer_picker_gesture&) const -> bool = default;
	};

	using reviewer_picker_dispatch = std::function<void(const reviewer_picker_gesture&)>;

	/// \brief [[BPR-1804]]. The people picker People opens: a search field,
	/// GitHub's suggestions and everyone else who can review, each row with
	/// the roster's avatar and a word for where they stand, and one line that
	/// says what is happening.
	///
	/// \details Keyboard: Up and Down move through the people from the search
	/// field, Return chooses, Escape closes. Each row's accessible text names
	/// the person and their standing; the line's accessible name follows it.
	class reviewer_picker final : public QFrame
	{
	public:
		explicit reviewer_picker(QWidget* aParent = nullptr);

		void set_dispatch(reviewer_picker_dispatch aDispatch);
		/// \brief Shows the core's state. Opening places the picker under
		/// `aAnchor`; a closed state hides it without reporting a close.
		void present(const core::reviewer_picker_state& aState, QWidget* aAnchor);

		[[nodiscard]] auto search_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto people() const noexcept -> QListWidget*;
		[[nodiscard]] auto status() const noexcept -> QLabel*;
		/// \brief The logins listed, in order, headings left out.
		[[nodiscard]] auto listed_logins() const -> QStringList;
		/// \brief Moves the highlight `aStep` people, skipping headings.
		void move_highlight(int aStep);
		/// \brief Chooses the highlighted person, as Return does.
		void choose_highlighted();

		/// \brief The words the status line shows for `aState`.
		[[nodiscard]] static auto status_text(const core::reviewer_picker_state& aState) -> QString;

	protected:
		bool eventFilter(QObject* aObject, QEvent* aEvent) override;
		void hideEvent(QHideEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;

	private:
		void dispatch(reviewer_picker_gesture::kind aKind, std::string aText = { });
		void rebuild(const core::reviewer_picker_state& aState);

		QLineEdit*               Search = nullptr;
		QListWidget*             List   = nullptr;
		QLabel*                  Status = nullptr;
		reviewer_picker_dispatch Dispatch;
		/// A hide the core asked for is not reported back as a close.
		bool Presenting = false;
	};
} // namespace bpr::qt

#endif
