#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_LIST_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_LIST_HPP

#include "bpr/qt/home_panel.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/review_landing_model.hpp"

#include <QAbstractItemView>
#include <QFrame>
#include <QListView>
#include <QListWidget>
#include <QPersistentModelIndex>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QSortFilterProxyModel>
#include <QString>
#include <QToolButton>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

class QLabel;
class QTimer;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief [[BPR-0783]]. What identifies one of Home's groups.
	///
	/// \details Always a host and one value, because two hosts may name the same
	/// owner, repository or login and they are not the same group. The host is
	/// part of the identity and never part of the name: a chip says "on host",
	/// and under Account grouping the header states it at its right.
	struct home_group_key
	{
		QString Host;
		QString Value;

		auto operator<=>(const home_group_key&) const = default;

		[[nodiscard]] auto empty() const -> bool
		{
			return Host.isEmpty() && Value.isEmpty();
		}
	};

	/// \brief The key one row belongs to under a grouping; empty for no grouping.
	[[nodiscard]] auto home_group_key_of(
		application::home_grouping aGrouping, const core::queue_item& aItem) -> home_group_key;

	/// \brief The group's name: an owner, "owner/name", or a login.
	[[nodiscard]] auto home_group_name(
		application::home_grouping aGrouping, const home_group_key& aKey) -> QString;

	/// \brief [[BPR-0776]] [[BPR-0783]]. One slice of the decided, filtered queue:
	/// a state panel, a group panel, or a state band inside a group.
	///
	/// \details A filter, not a second queue: it holds no item. Left unordered it
	/// keeps the order of the queue beneath it, which is the core's or the
	/// reviewer's chosen sort. Ordered by state it puts the state first and that
	/// same chosen order within each state, because state is always Home's first
	/// key and Group only changes the panels around the rows.
	class home_slice_model final : public QSortFilterProxyModel
	{
	public:
		explicit home_slice_model(review_landing_filter_model& aQueue, QObject* aParent = nullptr);

		/// \brief Admits only the rows of one group; an empty key admits every row.
		void set_key(application::home_grouping aGrouping, const home_group_key& aKey);
		/// \brief Admits only one of Home's states; nothing admits every state.
		void set_state(std::optional<home_group> aState);
		/// \brief Orders by state first, then by the queue's own order within it.
		void set_state_ordered(bool aOrdered);
		/// \brief Whether this slice's rows answer with their account chip.
		///
		/// \details A row drops the chip where its group already says the account:
		/// under Account grouping, and in a group whose rows are all one account.
		void set_chips(bool aChips);

		[[nodiscard]] auto state() const noexcept -> std::optional<home_group>;
		[[nodiscard]] auto key() const -> home_group_key;
		[[nodiscard]] auto chips() const noexcept -> bool;

		[[nodiscard]] auto data(const QModelIndex& aIndex, int aRole) const -> QVariant override;

	protected:
		[[nodiscard]] auto filterAcceptsRow(int aSourceRow, const QModelIndex& aSourceParent) const
			-> bool override;
		[[nodiscard]] auto lessThan(const QModelIndex& aLeft, const QModelIndex& aRight) const
			-> bool override;

	private:
		review_landing_filter_model& Queue;
		application::home_grouping   Grouping = application::home_grouping::none;
		home_group_key               Key;
		std::optional<home_group>    State;
		bool                         StateOrdered = false;
		bool                         Chips        = true;
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
	///
	/// [[BPR-1338]]. Given `home_row_role::icon_action`, it is an icon-only
	/// button for the rows that name one, in the place the delegate keeps for it.
	class home_row_action final : public QToolButton
	{
	public:
		home_row_action(QAbstractItemView& aView, const QString& aText, int aRole = home_row_role::row_action);

		void               set_trigger(std::function<void(int aRow)> aTrigger);
		[[nodiscard]] auto row() const noexcept -> int;
		void               refresh();

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		QAbstractItemView&            View;
		int                           Role     = home_row_role::row_action;
		int                           Row      = -1;
		int                           HoverRow = -1;
		std::function<void(int aRow)> Trigger;
	};

	/// \brief [[BPR-0776]] [[BPR-0783]]. Home's list: the decided queue arranged
	/// by the window's view, with Recently viewed last in every arrangement.
	///
	/// \details Rows are always in state order — Needs me, then Waiting on
	/// others, then Following — because state is Home's first key and Group only
	/// changes the panels around the rows. Four arrangements:
	///
	/// - Group None with State headers on: one panel per state.
	/// - Group None with State headers off: one neutral card of state-ordered
	///   rows, each carrying its state's 3-point mark.
	/// - A grouping with State headers on: one neutral panel per group, with a
	///   slim tinted band per non-empty state nested inside it.
	/// - A grouping with State headers off: the group's rows directly, in state
	///   order, with their marks.
	///
	/// An empty state, band or group is not shown. When the ready queue of a
	/// signed-in window is empty, Needs me holds one sentence whose link opens
	/// Reviews & queue; no empty box is drawn in any arrangement. The list
	/// decides nothing: it reports a row chosen, a link followed, a row action
	/// taken and a collapse changed, and the window acts.
	class home_list final : public QScrollArea
	{
	public:
		explicit home_list(review_landing_filter_model& aQueue, QWidget* aParent = nullptr);

		/// \brief Takes Recently viewed as the last panel.
		void adopt_recent(home_recent_view& aRecent);
		void set_open_dispatch(std::function<void(const core::queue_item&)> aDispatch);
		void set_settings_dispatch(std::function<void()> aDispatch);
		void set_recent_action_dispatch(std::function<void(int aRow)> aDispatch);
		/// \brief [[BPR-1338]]. A queue row's Open on GitHub, from its icon button.
		void set_page_dispatch(std::function<void(const core::queue_item&)> aDispatch);
		/// \brief [[BPR-1338]]. The icon and tooltip every queue row's Open on
		/// GitHub button shows, the window action's.
		void set_page_action(const QIcon& aIcon, const QString& aToolTip);
		/// \brief [[BPR-1338]]. A right click on a queue row: the row is current,
		/// and the window shows its menu at this global point.
		void set_row_menu_dispatch(std::function<void(const QPoint& aGlobal)> aDispatch);
		/// \brief [[BPR-1338]]. Told when a queue row becomes current.
		void set_current_observer(std::function<void()> aObserver);
		/// \brief [[BPR-1342]]. A queue row asked to open in a new window: ⌘Return
		/// (Ctrl+Return off macOS) on the current row, ⌘-click or middle-click.
		void set_new_window_dispatch(std::function<void(const core::queue_item&)> aDispatch);
		/// \brief [[BPR-1342]]. A Recently viewed row asked to open in a new
		/// window by ⌘-click or middle-click; its keys are the window's.
		void set_recent_new_window_dispatch(std::function<void(int aRow)> aDispatch);
		/// \brief Whether the queue is ready and the window has an account to list it for.
		void set_queue_status(bool aReady, bool aAccount);
		/// \brief RFC-BPR-0062. With no saved repository for the account, an
		/// empty queue shows no Needs me lane: its sentence is all it could hold.
		void               set_onboarding(bool aOnboarding);
		[[nodiscard]] auto onboarding() const noexcept -> bool;
		void               set_recent(int aCount, const QString& aSummary);
		void               set_state_colours(const home_state_colours& aColours);

		/// \brief [[BPR-0783]]. What Home groups its rows by; None is the default.
		void set_grouping(application::home_grouping aGrouping);
		/// \brief [[BPR-0783]]. Whether a state is a panel of its own, or a mark on its rows.
		void set_state_headers(bool aHeaders);
		/// \brief [[BPR-0783]]. The collapse of the panels that carry no name, as
		/// bits of `application::home_collapsed_panel`.
		///
		/// \details Only these are the window's to remember: a group panel's key
		/// would name what was reviewed, which the settings store does not hold.
		void               set_collapsed_panels(std::uint8_t aCollapsed);
		[[nodiscard]] auto grouping() const noexcept -> application::home_grouping;
		[[nodiscard]] auto state_headers() const noexcept -> bool;
		[[nodiscard]] auto collapsed_panels() const noexcept -> std::uint8_t;
		/// \brief Told when a nameless panel's collapse changed, so the window
		/// can dispatch its whole view.
		void set_view_observer(std::function<void()> aObserver);

		/// \brief [[BPR-1357]]. The row a pull request is being opened from.
		///
		/// \details Every shown row with that identity says so; the one that
		/// asked (the current row, or the first in reading order) carries the
		/// Cancel or Retry button. The mark turns while the phase is opening
		/// or waiting. The list decides nothing: the phase, line and action are
		/// the window's presentation of the core's state.
		void set_open(home_row_open aOpen);
		/// \brief The shown list that carries the open row's action, or null when
		/// no shown row is the one being opened from.
		[[nodiscard]] auto open_view() const -> QAbstractItemView*;
		/// \brief The open row's Cancel or Retry button.
		[[nodiscard]] auto open_action() const noexcept -> QToolButton*;
		void               set_open_action_dispatch(std::function<void()> aDispatch);

		/// \brief Recounts every panel, and shows or hides each by what it holds.
		void refresh();
		/// \brief Makes one queue identity current; a focusing selection expands
		/// the group and the band that hold it.
		auto select(const core::pull_request_ref& aRef, bool aFocus) -> bool;
		void focus_first_row();
		void update_sticky_headers();

		[[nodiscard]] auto current_queue_item() const -> std::optional<core::queue_item>;
		/// \brief The list holding the current queue row, or null.
		[[nodiscard]] auto current_view() const -> QListView*;
		[[nodiscard]] auto state_panel(home_group aGroup) const noexcept -> home_panel*;
		[[nodiscard]] auto state_view(home_group aGroup) const noexcept -> QListView*;
		[[nodiscard]] auto state_model(home_group aGroup) const noexcept -> home_slice_model*;
		/// \brief [[BPR-0783]]. Group None with State headers off: one card of rows.
		[[nodiscard]] auto flat_panel() const noexcept -> home_panel*;
		[[nodiscard]] auto flat_view() const noexcept -> QListView*;
		/// \brief [[BPR-0783]]. The group panels, in the order they are shown.
		[[nodiscard]] auto group_panels() const -> std::vector<home_panel*>;
		/// \brief The group panel of that name, or null while none is shown.
		[[nodiscard]] auto group_panel(const QString& aName) const -> home_panel*;
		/// \brief One group's state band, or null when that state is not in it.
		[[nodiscard]] auto band_panel(const home_panel& aGroup, home_group aState) const -> home_panel*;
		/// \brief The rows any panel shows, or null when it shows none.
		[[nodiscard]] auto rows_view(const home_panel& aPanel) const -> QAbstractItemView*;
		/// \brief [[BPR-0783]]. Whether the queue names more than one account, so
		/// its rows carry a chip somewhere.
		[[nodiscard]] auto account_chips() const -> bool;
		[[nodiscard]] auto recent_panel() const noexcept -> home_panel*;
		[[nodiscard]] auto recent_action() const noexcept -> home_row_action*;
		[[nodiscard]] auto empty_sentence() const noexcept -> QLabel*;
		[[nodiscard]] auto row_delegate() const noexcept -> home_row_delegate*;
		/// \brief True while the queue's rows may be shown: an account, and the list on screen.
		[[nodiscard]] auto queue_shown() const -> bool;
		/// \brief The keyboard stops of every shown panel, in reading order.
		[[nodiscard]] auto focus_chain() const -> std::vector<QWidget*>;

		static constexpr int panel_gap = 14;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		/// \brief One group panel: its identity, and either its rows or its bands.
		struct group_entry
		{
			home_group_key                    Key;
			QString                           Name;
			home_panel*                       Panel     = nullptr;
			QWidget*                          Content   = nullptr;
			QVBoxLayout*                      Column    = nullptr;
			home_slice_model*                 RowsModel = nullptr;
			home_row_view*                    RowsView  = nullptr;
			std::array<home_panel*, 3U>       Bands{ };
			std::array<home_slice_model*, 3U> BandModels{ };
			std::array<home_row_view*, 3U>    BandViews{ };
		};

		/// \brief One shown panel and the rows it holds, in reading order.
		struct panel_stop
		{
			home_panel*        Panel = nullptr;
			QAbstractItemView* Rows  = nullptr;
		};

		void schedule_refresh();
		/// \brief The keys of every group with a row, in the order of their first
		/// row in state order, ties by name.
		[[nodiscard]] auto ordered_keys() const -> std::vector<home_group_key>;
		auto               group_for(const home_group_key& aKey, const QString& aName) -> group_entry&;
		void               retire_groups(const std::vector<home_group_key>& aKept);
		/// \brief The accounts of every group, in one pass over the queue.
		///
		/// \details [[BPR-0783]]. Once per refresh rather than once per group: a
		/// group asking the whole queue for its own accounts is a scan per group.
		[[nodiscard]] auto group_accounts() const -> std::map<home_group_key, std::vector<home_account>>;
		void               refresh_group(
			group_entry& aGroup, const std::map<home_group_key, std::vector<home_account>>& aAccounts);
		/// \brief What the window remembers a named panel's collapse under, while it lives.
		///
		/// \details The grouping is part of it: an owner and a login can be the
		/// same word on the same host, and they are not the same group.
		[[nodiscard]] static auto collapse_key(
			application::home_grouping aGrouping, const home_group_key& aKey, std::optional<home_group> aState)
			-> QString;
		void observe_named_collapse(
			home_panel& aPanel, const home_group_key& aKey, std::optional<home_group> aState);
		void observe_nameless_collapse(home_panel& aPanel, std::uint8_t aBit);
		void apply_collapsed_panels();
		void adopt_current(QAbstractItemView* aView, const QModelIndex& aCurrent);
		void open_row(const QAbstractItemView& aView, const QModelIndex& aIndex);
		void open_row_in_new_window(const QAbstractItemView& aView, const QModelIndex& aIndex);
		/// \brief [[BPR-1342]]. A ⌘-click or middle-click on a row's viewport:
		/// pressed, it makes the row current; released on the same row, it
		/// opens it in a new window. Any other pointer event is not taken.
		auto               read_new_window_click(QObject* aWatched, QEvent* aEvent) -> bool;
		[[nodiscard]] auto item_of(const QAbstractItemView& aView, const QModelIndex& aIndex) const
			-> std::optional<core::queue_item>;
		void ensure_row_visible(QAbstractItemView& aView, const QModelIndex& aIndex, const home_panel& aPanel);
		void derive_link_colour();
		void adopt_rows(home_row_view& aView, home_slice_model& aModel, const QString& aName, const QString& aAccessible);
		/// \brief Every shown panel in reading order, with the rows it holds.
		[[nodiscard]] auto reading_order() const -> std::vector<panel_stop>;
		/// \brief Every live rows view, whichever arrangement is shown.
		[[nodiscard]] auto row_views() const -> std::vector<QAbstractItemView*>;
		auto               navigate_from_header(const home_panel& aPanel, int aKey) -> bool;
		auto               focus_row(QAbstractItemView& aView, int aRow) -> bool;
		/// \brief The panel that holds one rows view, or null.
		[[nodiscard]] auto panel_of(const QAbstractItemView& aView) const -> home_panel*;
		/// \brief [[BPR-1357]]. The row of a view that is the open row, or invalid.
		[[nodiscard]] auto open_index(const QAbstractItemView& aView) const -> QModelIndex;
		/// \brief [[BPR-1357]]. Puts the open row's button on its row, or hides it.
		void place_open_action();
		/// \brief [[BPR-1357]]. One turn of the opening mark, on the rows that show it.
		void turn_open_mark();
		/// \brief [[BPR-1357]]. Whether a row is being opened, so a second gesture
		/// on it asks for nothing.
		[[nodiscard]] auto opening(const QModelIndex& aIndex) const -> bool;

		review_landing_filter_model&                 Queue;
		QWidget*                                     Column   = nullptr;
		QVBoxLayout*                                 Layout   = nullptr;
		home_row_delegate*                           Delegate = nullptr;
		std::array<home_slice_model*, 3U>            StateModels{ };
		std::array<home_panel*, 3U>                  StatePanels{ };
		std::array<home_row_view*, 3U>               StateViews{ };
		home_panel*                                  FlatPanel = nullptr;
		home_slice_model*                            FlatModel = nullptr;
		home_row_view*                               FlatView  = nullptr;
		std::vector<std::unique_ptr<group_entry>>    Groups;
		home_panel*                                  RecentPanel   = nullptr;
		home_recent_view*                            RecentView    = nullptr;
		home_row_action*                             RecentAction  = nullptr;
		QLabel*                                      EmptySentence = nullptr;
		QAbstractItemView*                           CurrentRows   = nullptr;
		std::function<void(const core::queue_item&)> OpenDispatch;
		std::function<void()>                        SettingsDispatch;
		std::function<void(int aRow)>                RecentActionDispatch;
		std::function<void()>                        ViewObserver;
		std::function<void(const core::queue_item&)> PageDispatch;
		std::function<void(const QPoint&)>           RowMenuDispatch;
		std::function<void()>                        CurrentObserver;
		std::function<void(const core::queue_item&)> NewWindowDispatch;
		std::function<void(int aRow)>                RecentNewWindowDispatch;
		/// [[BPR-1342]]. The row a ⌘-click or middle-click pressed, so only a
		/// release on that same row opens it.
		QPersistentModelIndex NewWindowPress;
		QIcon                 PageIcon;
		QString               PageToolTip;
		/// [[BPR-1357]]. The open row's button, and the timer that turns its mark.
		QToolButton*          OpenAction = nullptr;
		QTimer*               OpenTurn   = nullptr;
		std::function<void()> OpenActionDispatch;
		/// The collapse of a group panel and of the bands inside it, keyed by the
		/// owner, repository or account it names. It lives as long as the window
		/// and is never dispatched, so nothing writes those names to a store.
		std::map<QString, bool>    GroupCollapse;
		application::home_grouping Grouping           = application::home_grouping::none;
		std::uint8_t               CollapsedPanels    = 0U;
		int                        RecentCount        = 0;
		bool                       StateHeaders       = true;
		bool                       QueueReady         = false;
		bool                       QueueAccount       = false;
		bool                       Onboarding         = false;
		bool                       RefreshPending     = false;
		bool                       RestoringSelection = false;
		/// Set while the list itself is moving a collapse, so adopting a view
		/// never reports it back as the reviewer's own gesture.
		bool ApplyingCollapse = false;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_HOME_LIST_HPP
