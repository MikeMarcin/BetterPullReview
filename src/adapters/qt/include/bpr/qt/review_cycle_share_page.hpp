#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_CYCLE_SHARE_PAGE_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_CYCLE_SHARE_PAGE_HPP

#include "bpr/application/review_cycle_projection.hpp"

#include <QWidget>

#include <array>
#include <functional>
#include <utility>
#include <vector>

class QComboBox;
class QFont;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace bpr::qt {
	/// \brief The gestures Compose feedback sends to its host.
	struct review_cycle_share_dispatch
	{
		std::function<void()>                         Back;
		std::function<void(core::cycle_compose_edit)> Edit;
		/// The composition chooses the file for Save to File and Save as Bundle.
		std::function<void(core::cycle_share_route)> Share;
		std::function<void(bool)>                    Replace;
	};

	/// \brief Compose feedback and Share feedback for a local cycle
	/// (RFC-BPR-0061, BPR-1161), in the centre of the cycle page.
	///
	/// \details As a pull request's Compose review page does: Back at the top,
	/// then the round. The share row offers Copy to Clipboard and Save to
	/// File first, then Save as Bundle, then any optional route this
	/// installation reaches; a route policy refuses is shown refused, never
	/// hidden or replaced. The assignment (action and optional goal) and the
	/// ordered notes, each included or not by its check box, sit beside the
	/// preview, which is exactly the readable document Copy and Save send.
	/// Below them every delivery observation of the cycle, each in its own
	/// words, with what none of them means. It decides nothing and performs
	/// nothing: gestures leave through `review_cycle_share_dispatch`.
	class review_cycle_share_page final : public QWidget
	{
	public:
		explicit review_cycle_share_page(QWidget* aParent = nullptr);

		void set_dispatch(review_cycle_share_dispatch aDispatch);
		/// \brief Shows the host's share view; `aNotes` names the notes.
		void present(const application::review_cycle_share_view& aShare, const std::vector<application::review_cycle_note_entry>& aNotes, bool aIdle);
		void set_code_font(const QFont& aFont);
		/// \brief Gives focus to the first share route offered, else Back.
		void focus_first();

		[[nodiscard]] auto back_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto title() const noexcept -> QLabel*;
		[[nodiscard]] auto facts() const noexcept -> QLabel*;
		[[nodiscard]] auto status() const noexcept -> QLabel*;
		/// \brief The button for a route, when the page offers or refuses it.
		[[nodiscard]] auto route_button(core::cycle_share_route aRoute) const noexcept -> QPushButton*;
		[[nodiscard]] auto action_box() const noexcept -> QComboBox*;
		[[nodiscard]] auto goal() const noexcept -> QLineEdit*;
		[[nodiscard]] auto items() const noexcept -> QListWidget*;
		[[nodiscard]] auto move_up_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto move_down_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto preview() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto deliveries() const noexcept -> QListWidget*;
		[[nodiscard]] auto invocation() const noexcept -> QLineEdit*;
		[[nodiscard]] auto replace_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto keep_button() const noexcept -> QPushButton*;

	private:
		void rebuild_items(const application::review_cycle_share_view& aShare, const std::vector<application::review_cycle_note_entry>& aNotes);
		void rebuild_deliveries(const application::review_cycle_share_view& aShare);
		void refresh_moves();
		void move_current(bool aEarlier);

		review_cycle_share_dispatch Dispatch;
		QPushButton*                Back        = nullptr;
		QLabel*                     Title       = nullptr;
		QLabel*                     Facts       = nullptr;
		QLabel*                     Status      = nullptr;
		QWidget*                    ReplaceRow  = nullptr;
		QLabel*                     ReplaceText = nullptr;
		QPushButton*                ReplaceIt   = nullptr;
		QPushButton*                KeepIt      = nullptr;
		QLineEdit*                  Invocation  = nullptr;
		/// Use in External Agent and Send to Agent, below Copy and Save.
		QWidget*                                                OptionalRoutes = nullptr;
		std::array<QPushButton*, core::cycle_share_route_count> Routes{ };
		QComboBox*                                              Action     = nullptr;
		QLineEdit*                                              Goal       = nullptr;
		QListWidget*                                            Items      = nullptr;
		QPushButton*                                            MoveUp     = nullptr;
		QPushButton*                                            MoveDown   = nullptr;
		QPlainTextEdit*                                         Preview    = nullptr;
		QListWidget*                                            Deliveries = nullptr;
		/// What the item list and the delivery list were built from.
		std::vector<std::pair<core::cycle_feedback_item_id, bool>> ShownItems;
		std::vector<QString>                                       ShownDeliveries;
		bool                                                       Idle       = true;
		bool                                                       Presenting = false;
	};
}

#endif
