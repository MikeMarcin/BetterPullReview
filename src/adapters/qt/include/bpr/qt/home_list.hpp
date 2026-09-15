#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_LIST_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_LIST_HPP

#include "bpr/qt/home_panel.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/review_landing_model.hpp"

#include <QAbstractItemView>
#include <QFrame>
#include <QListView>
#include <QListWidget>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QSortFilterProxyModel>
#include <QToolButton>

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

class QLabel;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief [[BPR-0776]]. One of Home's states over the decided, filtered queue.
	///
	/// \details A filter, not a second queue: it holds no item and orders
	/// nothing, so the rows it admits keep the order of the queue beneath it,
	/// which is the core's or the reviewer's chosen sort within a state.
	class home_state_filter_model final : public QSortFilterProxyModel
	{
	public:
		home_state_filter_model(home_group aGroup, QAbstractItemModel& aSource, QObject* aParent = nullptr);

		[[nodiscard]] auto group() const noexcept -> home_group;

	protected:
		[[nodiscard]] auto filterAcceptsRow(int aSourceRow, const QModelIndex& aSourceParent) const
			-> bool override;

	private:
		home_group Group;
	};

	/// \brief [[BPR-0776]]. A list view exactly as tall as its rows, inside a panel.
	///
	/// \details Home scrolls as one page, so a panel's list never scrolls on its
	/// own. Its height is the view's own laid-out content, which the view
	/// recomputes whenever its rows or its width change; a wheel it cannot use
	/// reaches the page.
	template <typename TView>
	class home_fitted_view final : public TView
	{
	public:
		explicit home_fitted_view(QWidget* const aParent = nullptr)
			: TView{ aParent }
		{
			this->setFrameShape(QFrame::NoFrame);
			this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
			this->setResizeMode(QListView::Adjust);
			this->setUniformItemSizes(false);
			this->setWordWrap(true);
			this->setMouseTracking(true);
			this->viewport()->setAttribute(Qt::WA_Hover);
			this->setSelectionMode(QAbstractItemView::SingleSelection);
			this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		}

		[[nodiscard]] auto sizeHint() const -> QSize override
		{
			return { TView::sizeHint().width(), fitted_height() };
		}

		[[nodiscard]] auto minimumSizeHint() const -> QSize override
		{
			return { 0, fitted_height() };
		}

		[[nodiscard]] auto fitted_height() const -> int
		{
			return this->contentsSize().height() + (2 * this->frameWidth());
		}

	protected:
		void updateGeometries() override
		{
			TView::updateGeometries();
			const auto height = fitted_height();
			if (height != Fitted) {
				Fitted = height;
				this->updateGeometry();
			}
		}

	private:
		int Fitted = -1;
	};

	using home_row_view    = home_fitted_view<QListView>;
	using home_recent_view = home_fitted_view<QListWidget>;

	/// \brief [[BPR-0776]]. A row's action, shown on the hovered row and on the
	/// current row while its list has focus.
	///
	/// \details A real button, so it paints natively and is reachable by
	/// assistive technology; it takes no focus, because the keyboard already has
	/// the row (Shift and Return) and the row's menu. It is shown only for a row
	/// whose `home_row_role::row_action` names an action.
	class home_row_action final : public QToolButton
	{
	public:
		home_row_action(QAbstractItemView& aView, const QString& aText);

		void               set_trigger(std::function<void(int aRow)> aTrigger);
		[[nodiscard]] auto row() const noexcept -> int;
		void               refresh();

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		QAbstractItemView&            View;
		int                           Row      = -1;
		int                           HoverRow = -1;
		std::function<void(int aRow)> Trigger;
	};

	/// \brief [[BPR-0776]]. Home's list: one panel per state, in state order, and
	/// Recently viewed last.
	///
	/// \details Rows are always Needs me, then Waiting on others, then Following,
	/// because each panel admits one of the core's groups from the one decided
	/// queue. A state with no row is not shown. When the ready queue of a signed-in
	/// window is empty, Needs me holds one sentence whose link opens Reviews &
	/// queue; no empty box is drawn. The list decides nothing: it reports a row
	/// chosen, a link followed and a row action taken, and the window acts.
	class home_list final : public QScrollArea
	{
	public:
		explicit home_list(review_landing_filter_model& aQueue, QWidget* aParent = nullptr);

		/// \brief Takes Recently viewed as the last panel.
		void adopt_recent(home_recent_view& aRecent);
		void set_open_dispatch(std::function<void(const core::queue_item&)> aDispatch);
		void set_settings_dispatch(std::function<void()> aDispatch);
		void set_recent_action_dispatch(std::function<void(int aRow)> aDispatch);
		/// \brief Whether the queue is ready and the window has an account to list it for.
		void set_queue_status(bool aReady, bool aAccount);
		void set_recent(int aCount, const QString& aSummary);
		void set_state_colours(const home_state_colours& aColours);
		/// \brief Recounts every panel, and shows or hides each by what it holds.
		void refresh();
		/// \brief Makes one queue identity current; a focusing selection expands its panel.
		auto select(const core::pull_request_ref& aRef, bool aFocus) -> bool;
		void focus_first_row();
		void update_sticky_headers();

		[[nodiscard]] auto current_queue_item() const -> std::optional<core::queue_item>;
		/// \brief The state list holding the current queue row, or null.
		[[nodiscard]] auto current_view() const -> QListView*;
		[[nodiscard]] auto state_panel(home_group aGroup) const noexcept -> home_panel*;
		[[nodiscard]] auto state_view(home_group aGroup) const noexcept -> QListView*;
		[[nodiscard]] auto state_model(home_group aGroup) const noexcept -> home_state_filter_model*;
		[[nodiscard]] auto recent_panel() const noexcept -> home_panel*;
		[[nodiscard]] auto recent_action() const noexcept -> home_row_action*;
		[[nodiscard]] auto empty_sentence() const noexcept -> QLabel*;
		[[nodiscard]] auto row_delegate() const noexcept -> home_row_delegate*;
		/// \brief True while state panels may be shown: an account, and the list on screen.
		[[nodiscard]] auto queue_shown() const -> bool;
		/// \brief The keyboard stops of every shown panel, in reading order.
		[[nodiscard]] auto focus_chain() const -> std::vector<QWidget*>;

		static constexpr int panel_gap = 14;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void               schedule_refresh();
		void               adopt_current(std::size_t aGroup, const QModelIndex& aCurrent);
		void               open_row(std::size_t aGroup, const QModelIndex& aIndex);
		void               ensure_row_visible(QAbstractItemView& aView, const QModelIndex& aIndex, const home_panel& aPanel);
		void               derive_link_colour();
		[[nodiscard]] auto shown_panels() const -> std::vector<home_panel*>;
		[[nodiscard]] auto rows_of(const home_panel& aPanel) const -> QAbstractItemView*;
		auto               navigate_from_header(const home_panel& aPanel, int aKey) -> bool;
		auto               focus_row(QAbstractItemView& aView, int aRow) -> bool;

		review_landing_filter_model&                 Queue;
		QWidget*                                     Column   = nullptr;
		QVBoxLayout*                                 Layout   = nullptr;
		home_row_delegate*                           Delegate = nullptr;
		std::array<home_state_filter_model*, 3U>     StateModels{ };
		std::array<home_panel*, 3U>                  StatePanels{ };
		std::array<home_row_view*, 3U>               StateViews{ };
		home_panel*                                  RecentPanel   = nullptr;
		home_recent_view*                            RecentView    = nullptr;
		home_row_action*                             RecentAction  = nullptr;
		QLabel*                                      EmptySentence = nullptr;
		std::optional<std::size_t>                   CurrentGroup;
		std::function<void(const core::queue_item&)> OpenDispatch;
		std::function<void()>                        SettingsDispatch;
		std::function<void(int aRow)>                RecentActionDispatch;
		int                                          RecentCount        = 0;
		bool                                         QueueReady         = false;
		bool                                         QueueAccount       = false;
		bool                                         RefreshPending     = false;
		bool                                         RestoringSelection = false;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_LIST_HPP
