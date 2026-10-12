#include "bpr/qt/home_list.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QHoverEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPalette>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr auto state_count = std::size_t{ 4U };
		/// The all-empty sentence sits at a row's inset inside its panel.
		constexpr auto sentence_inset          = 14;
		constexpr auto sentence_inset_vertical = 12;
		/// [[BPR-1357]]. One step of the opening mark: a turn in about a second.
		constexpr auto open_turn_interval = 80;

		constexpr auto groups = std::array{
			home_group::needs_you,
			home_group::my_pull_requests,
			home_group::waiting_on_others,
			home_group::following,
		};

		[[nodiscard]] auto text(const std::string& aValue) -> QString
		{
			return QString::fromStdString(aValue);
		}

		[[nodiscard]] auto panel_name(const home_group aGroup) -> QString
		{
			switch (aGroup) {
			case home_group::needs_you:
				return QStringLiteral("homeNeedsMePanel");
			case home_group::my_pull_requests:
				return QStringLiteral("homeAuthoredPanel");
			case home_group::waiting_on_others:
				return QStringLiteral("homeWaitingOnOthersPanel");
			case home_group::following:
				break;
			}
			return QStringLiteral("homeFollowingPanel");
		}

		[[nodiscard]] auto rows_name(const home_group aGroup) -> QString
		{
			switch (aGroup) {
			case home_group::needs_you:
				return QStringLiteral("homeNeedsMeRows");
			case home_group::my_pull_requests:
				return QStringLiteral("homeAuthoredRows");
			case home_group::waiting_on_others:
				return QStringLiteral("homeWaitingOnOthersRows");
			case home_group::following:
				break;
			}
			return QStringLiteral("homeFollowingRows");
		}

		/// \brief [[BPR-0783]]. The bit one nameless panel's collapse is kept in.
		[[nodiscard]] auto collapsed_bit(const std::optional<home_group> aState) -> std::uint8_t
		{
			if (!aState.has_value()) {
				return application::home_collapsed_panel::recently_viewed;
			}
			switch (*aState) {
			case home_group::needs_you:
				return application::home_collapsed_panel::needs_me;
			case home_group::my_pull_requests:
				return application::home_collapsed_panel::my_pull_requests;
			case home_group::waiting_on_others:
				return application::home_collapsed_panel::waiting_on_others;
			case home_group::following:
				break;
			}
			return application::home_collapsed_panel::following;
		}

		/// \brief The group one row belongs to, read from the row itself.
		///
		/// \details [[BPR-0783]]. Deliberately the row's own roles rather than the
		/// whole decided item: this is asked once per row while a slice builds its
		/// mapping, and reaching back through the queue proxy's mapping while that
		/// is happening is neither cheap nor safe.
		[[nodiscard]] auto key_of_row(const application::home_grouping aGrouping, const QModelIndex& aRow)
			-> home_group_key
		{
			const auto host = aRow.data(review_landing_model::host_role).toString();
			switch (aGrouping) {
			case application::home_grouping::organization: {
				const auto repository = aRow.data(review_landing_model::repository_role).toString();
				const auto slash      = repository.indexOf(QLatin1Char{ '/' });
				return { .Host = host, .Value = slash > 0 ? repository.left(slash) : repository };
			}
			case application::home_grouping::repository:
				return { .Host = host, .Value = aRow.data(review_landing_model::repository_role).toString() };
			case application::home_grouping::account:
				return { .Host = host, .Value = aRow.data(review_landing_model::account_role).toString() };
			case application::home_grouping::none:
				break;
			}
			return { };
		}

		/// \brief The state one row is in, read from the row itself.
		[[nodiscard]] auto state_of_row(const QModelIndex& aRow) -> std::optional<home_group>
		{
			const auto state = aRow.data(review_landing_model::group_role);
			if (!state.isValid()) {
				return std::nullopt;
			}
			return static_cast<home_group>(state.toInt());
		}

		void connect_model_signals(QAbstractItemModel& aModel, QObject& aContext, const std::function<void()>& aChanged)
		{
			QObject::connect(&aModel, &QAbstractItemModel::rowsInserted, &aContext, [aChanged](const QModelIndex&, int, int) { aChanged(); });
			QObject::connect(&aModel, &QAbstractItemModel::rowsRemoved, &aContext, [aChanged](const QModelIndex&, int, int) { aChanged(); });
			QObject::connect(&aModel, &QAbstractItemModel::rowsMoved, &aContext,
							 [aChanged](const QModelIndex&, int, int, const QModelIndex&, int) { aChanged(); });
			QObject::connect(&aModel, &QAbstractItemModel::modelReset, &aContext, [aChanged] { aChanged(); });
			QObject::connect(&aModel, &QAbstractItemModel::layoutChanged, &aContext, [aChanged] { aChanged(); });
			QObject::connect(&aModel, &QAbstractItemModel::dataChanged, &aContext,
							 [aChanged](const QModelIndex&, const QModelIndex&, const QList<int>&) { aChanged(); });
		}
	} // namespace

	// ---- grouping keys ------------------------------------------------------

	auto home_group_key_of(const application::home_grouping aGrouping, const core::queue_item& aItem) -> home_group_key
	{
		const auto host       = text(aItem.Ref.Host);
		const auto repository = text(aItem.Ref.Repository);
		switch (aGrouping) {
		case application::home_grouping::organization: {
			// The owner of "owner/name". A repository the provider gave in
			// another shape is its own owner rather than a group of nothing.
			const auto slash = repository.indexOf(QLatin1Char{ '/' });
			return { .Host = host, .Value = slash > 0 ? repository.left(slash) : repository };
		}
		case application::home_grouping::repository:
			return { .Host = host, .Value = repository };
		case application::home_grouping::account:
			return { .Host = host, .Value = text(aItem.Ref.Account) };
		case application::home_grouping::none:
			break;
		}
		return { };
	}

	auto home_group_name(const application::home_grouping aGrouping, const home_group_key& aKey) -> QString
	{
		// Every grouping names its group by its own value: an owner, "owner/name",
		// or a login. The host is never part of the name.
		static_cast<void>(aGrouping);
		return aKey.Value;
	}

	// ---- home_slice_model ---------------------------------------------------

	home_slice_model::home_slice_model(review_landing_filter_model& aQueue, QObject* const aParent)
		: QSortFilterProxyModel{ aParent }
		, Queue{ aQueue }
	{
		setDynamicSortFilter(true);
		setSourceModel(&aQueue);
		sort(-1);
	}

	void home_slice_model::set_key(const application::home_grouping aGrouping, const home_group_key& aKey)
	{
		if (Grouping == aGrouping && Key == aKey) {
			return;
		}
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
		beginFilterChange();
		Grouping = aGrouping;
		Key      = aKey;
		endFilterChange(Direction::Rows);
#else
		Grouping = aGrouping;
		Key      = aKey;
		invalidateFilter();
#endif
	}

	void home_slice_model::set_state(const std::optional<home_group> aState)
	{
		if (State == aState) {
			return;
		}
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
		beginFilterChange();
		State = aState;
		endFilterChange(Direction::Rows);
#else
		State = aState;
		invalidateFilter();
#endif
	}

	void home_slice_model::set_state_ordered(const bool aOrdered)
	{
		if (StateOrdered == aOrdered) {
			return;
		}
		StateOrdered = aOrdered;
		// Qt returns early when the requested column and order are unchanged, so
		// the proxy sort is disabled first for the transition to be observable.
		sort(-1);
		if (StateOrdered) {
			sort(0, Qt::AscendingOrder);
		}
	}

	void home_slice_model::set_chips(const bool aChips)
	{
		if (Chips == aChips) {
			return;
		}
		Chips = aChips;
		// A chip appearing or going is a data change over the rows that are
		// already there, never a reset of them.
		if (const auto rows = rowCount(); rows > 0) {
			emit dataChanged(index(0, 0), index(rows - 1, 0), QList<int>{ home_row_role::account_chip });
		}
	}

	auto home_slice_model::state() const noexcept -> std::optional<home_group>
	{
		return State;
	}

	auto home_slice_model::key() const -> home_group_key
	{
		return Key;
	}

	auto home_slice_model::chips() const noexcept -> bool
	{
		return Chips;
	}

	auto home_slice_model::data(const QModelIndex& aIndex, const int aRole) const -> QVariant
	{
		if (aRole == home_row_role::account_chip && !Chips) {
			// [[BPR-0783]]. The group already says the account, so its rows do not
			// repeat it: under Account grouping, and in a single-account group.
			return QString{ };
		}
		return QSortFilterProxyModel::data(aIndex, aRole);
	}

	auto home_slice_model::filterAcceptsRow(const int aSourceRow, const QModelIndex& aSourceParent) const -> bool
	{
		const auto row = Queue.index(aSourceRow, 0, aSourceParent);
		if (!row.isValid()) {
			return false;
		}
		if (State.has_value() && state_of_row(row) != State) {
			return false;
		}
		return Key.empty() || key_of_row(Grouping, row) == Key;
	}

	auto home_slice_model::lessThan(const QModelIndex& aLeft, const QModelIndex& aRight) const -> bool
	{
		if (!StateOrdered) {
			return aLeft.row() < aRight.row();
		}
		// [[BPR-0783]]. State is always Home's first key; within a state the rows
		// keep the order of the queue beneath, which is the reviewer's chosen
		// sort. Grouping changes the panels around the rows, never their order.
		const auto left  = state_of_row(aLeft);
		const auto right = state_of_row(aRight);
		return left != right ? left < right : aLeft.row() < aRight.row();
	}

	void home_hide_selection(QAbstractItemView& aView)
	{
		if (auto* const selection = aView.selectionModel(); selection != nullptr && selection->hasSelection()) {
			selection->clearSelection();
		}
	}

	void home_show_selection(QAbstractItemView& aView)
	{
		auto* const selection = aView.selectionModel();
		if (selection == nullptr) {
			return;
		}
		if (const auto current = selection->currentIndex(); current.isValid() && !selection->isSelected(current)) {
			selection->select(current, QItemSelectionModel::ClearAndSelect);
		}
	}

	auto home_open_description() -> QString
	{
		return QStringLiteral("Double-click or press Return to open");
	}

	// ---- home_row_action ----------------------------------------------------

	home_row_action::home_row_action(QAbstractItemView& aView, const QString& aText, const int aRole)
		: QToolButton{ aView.viewport() }
		, View{ aView }
		, Role{ aRole }
	{
		setText(aText);
		setToolButtonStyle(aRole == home_row_role::icon_action ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextOnly);
		setFocusPolicy(Qt::NoFocus);
		setAutoRaise(false);
		if (aRole == home_row_role::icon_action) {
			// The size the delegate keeps for it: the style's small icon.
			const auto side = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, &aView);
			setIconSize(QSize{ side, side });
		}
		hide();
		aView.viewport()->setMouseTracking(true);
		aView.viewport()->installEventFilter(this);
		aView.installEventFilter(this);
		connect(this, &QToolButton::clicked, this, [this] {
			if (Row >= 0 && Trigger) {
				Trigger(Row);
			}
		});
		if (auto* const model = aView.model(); model != nullptr) {
			connect_model_signals(*model, *this, [this] { refresh(); });
		}
		if (auto* const selection = aView.selectionModel(); selection != nullptr) {
			connect(selection, &QItemSelectionModel::currentChanged, this, [this] { refresh(); });
		}
	}

	void home_row_action::set_trigger(std::function<void(int aRow)> aTrigger)
	{
		Trigger = std::move(aTrigger);
	}

	auto home_row_action::row() const noexcept -> int
	{
		return Row;
	}

	void home_row_action::refresh()
	{
		auto* const model  = View.model();
		const auto  offers = [this, model](const int aRow) {
			if (aRow < 0 || model == nullptr || aRow >= model->rowCount(View.rootIndex())) {
				return false;
			}
			const auto index = model->index(aRow, 0, View.rootIndex());
			// [[BPR-1357]]. The row being opened holds Cancel or Retry there.
			if (const auto* const delegate = dynamic_cast<const home_row_delegate*>(View.itemDelegate());
				delegate != nullptr && delegate->opens(index)) {
				return false;
			}
			// [[BPR-1338]]. The icon action takes the place only where no
			// labelled action holds it.
			return Role == home_row_role::row_action
				? !index.data(home_row_role::row_action).toString().isEmpty()
				: index.data(home_row_role::row_action).toString().isEmpty() && !index.data(Role).toString().isEmpty();
		};
		auto row = offers(HoverRow) ? HoverRow : -1;
		if (row < 0 && View.hasFocus() && View.currentIndex().isValid() && offers(View.currentIndex().row())) {
			row = View.currentIndex().row();
		}
		Row = row;
		if (row < 0) {
			hide();
			return;
		}
		const auto index  = model->index(row, 0, View.rootIndex());
		auto       option = QStyleOptionViewItem{ };
		option.initFrom(&View);
		option.rect   = View.visualRect(index);
		option.font   = View.font();
		option.widget = &View;
		setText(index.data(Role).toString());
		setAccessibleName(QStringLiteral("%1 %2").arg(text(), index.data(home_row_role::identity).toString()));
		setGeometry(home_row_delegate::action_rect(option, index));
		show();
		raise();
	}

	auto home_row_action::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		// [[BPR-1364]]. A list being destroyed with the keyboard still in it
		// sends its own focus-out from `~QWidget`, after its item-view part is
		// gone; its rows can no longer be asked anything.
		if (qobject_cast<const QAbstractItemView*>(&View) == nullptr) {
			return QToolButton::eventFilter(aWatched, aEvent);
		}
		if (aEvent != nullptr && aWatched == View.viewport()) {
			switch (aEvent->type()) {
			case QEvent::MouseMove:
				HoverRow = View.indexAt(static_cast<QMouseEvent*>(aEvent)->position().toPoint()).row();
				refresh();
				break;
			case QEvent::HoverMove:
				HoverRow = View.indexAt(static_cast<QHoverEvent*>(aEvent)->position().toPoint()).row();
				refresh();
				break;
			case QEvent::Leave:
			case QEvent::HoverLeave:
				HoverRow = -1;
				refresh();
				break;
			case QEvent::Resize:
				refresh();
				break;
			default:
				break;
			}
		} else if (aEvent != nullptr && aWatched == &View) {
			switch (aEvent->type()) {
			case QEvent::FocusIn:
			case QEvent::FocusOut:
			case QEvent::Resize:
			case QEvent::Show:
				refresh();
				break;
			default:
				break;
			}
		}
		return QToolButton::eventFilter(aWatched, aEvent);
	}

	// ---- home_list ----------------------------------------------------------

	home_list::home_list(review_landing_filter_model& aQueue, QWidget* const aParent)
		: QScrollArea{ aParent }
		, Queue{ aQueue }
	{
		setObjectName(QStringLiteral("homeList"));
		setAccessibleName(QStringLiteral("Pull requests"));
		setAccessibleDescription(QStringLiteral(
			"Needs me, then Waiting on others, then Following, then Recently viewed"));
		setFrameShape(QFrame::NoFrame);
		setWidgetResizable(true);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		setFocusPolicy(Qt::NoFocus);
		setBackgroundRole(QPalette::Window);
		viewport()->setAutoFillBackground(false);

		Column = new QWidget{ };
		Column->setObjectName(QStringLiteral("homeListColumn"));
		Column->setAutoFillBackground(false);
		Layout = new QVBoxLayout{ Column };
		Layout->setContentsMargins(0, 12, 0, 12);
		Layout->setSpacing(panel_gap);

		Delegate = new home_row_delegate{ this };
		for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
			const auto  group   = groups[index];
			auto* const model   = new home_slice_model{ Queue, this };
			auto* const panel   = new home_panel{ group, home_panel_style::panel, Column };
			auto* const content = new QWidget{ panel };
			auto* const column  = new QVBoxLayout{ content };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(0);
			model->set_state(group);
			auto* const view = new home_row_view{ content };
			adopt_rows(*view, *model, rows_name(group), home_group_label(group));
			column->addWidget(view);
			panel->setObjectName(panel_name(group));
			panel->header()->setObjectName(panel_name(group) + QStringLiteral("Header"));
			panel->set_title(home_group_label(group));
			panel->set_content(content);
			observe_nameless_collapse(*panel, collapsed_bit(group));
			panel->set_header_navigator([this, panel](const int aKey) { return navigate_from_header(*panel, aKey); });
			panel->installEventFilter(this);
			panel->hide();
			if (group == home_group::needs_you) {
				// RFC-BPR-0032: empty is a sentence, never a box. The link is the
				// page the saved repositories are kept on (RFC-BPR-0062, owner
				// verdict 2026-10-08); observer rules are reached from Filter.
				EmptySentence = new QLabel{ content };
				EmptySentence->setObjectName(QStringLiteral("homeEmptySentence"));
				EmptySentence->setTextFormat(Qt::RichText);
				EmptySentence->setText(QStringLiteral(
					"Nothing needs me. Pull requests from <a href=\"repositories\">saved repositories</a> and "
					"observer rules appear here."));
				EmptySentence->setWordWrap(true);
				EmptySentence->setOpenExternalLinks(false);
				EmptySentence->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
				EmptySentence->setFocusPolicy(Qt::StrongFocus);
				EmptySentence->setContentsMargins(sentence_inset, sentence_inset_vertical, sentence_inset, sentence_inset_vertical);
				EmptySentence->setAccessibleName(home_all_empty_sentence());
				EmptySentence->setAccessibleDescription(QStringLiteral("The link opens Settings, Repositories"));
				EmptySentence->installEventFilter(this);
				EmptySentence->hide();
				connect(EmptySentence, &QLabel::linkActivated, this, [this](const QString&) {
					if (SettingsDispatch) {
						SettingsDispatch();
					}
				});
				column->addWidget(EmptySentence);
				derive_link_colour();
			}
			StateModels[index] = model;
			StatePanels[index] = panel;
			StateViews[index]  = view;
			Layout->addWidget(panel);
		}

		// [[BPR-0783]]. Group None with State headers off: one neutral card of
		// rows in state order, each carrying its state's mark. It has no name, so
		// it has no header and no toggle either.
		FlatModel = new home_slice_model{ Queue, this };
		FlatModel->set_state_ordered(true);
		FlatPanel = new home_panel{ std::nullopt, home_panel_style::panel, Column };
		FlatPanel->setObjectName(QStringLiteral("homeFlatPanel"));
		FlatPanel->set_header_shown(false);
		auto* const flatContent = new QWidget{ FlatPanel };
		auto* const flatColumn  = new QVBoxLayout{ flatContent };
		flatColumn->setContentsMargins(0, 0, 0, 0);
		flatColumn->setSpacing(0);
		FlatView = new home_row_view{ flatContent };
		adopt_rows(*FlatView, *FlatModel, QStringLiteral("homeFlatRows"), QStringLiteral("Pull requests in state order"));
		flatColumn->addWidget(FlatView);
		FlatPanel->set_content(flatContent);
		FlatPanel->installEventFilter(this);
		FlatPanel->hide();
		Layout->addWidget(FlatPanel);

		RecentPanel = new home_panel{ std::nullopt, home_panel_style::panel, Column };
		RecentPanel->setObjectName(QStringLiteral("homeRecentPanel"));
		RecentPanel->header()->setObjectName(QStringLiteral("homeRecentPanelHeader"));
		RecentPanel->set_title(QStringLiteral("Recently viewed"));
		observe_nameless_collapse(*RecentPanel, application::home_collapsed_panel::recently_viewed);
		RecentPanel->set_header_navigator([this](const int aKey) { return navigate_from_header(*RecentPanel, aKey); });
		RecentPanel->installEventFilter(this);
		RecentPanel->hide();
		Layout->addWidget(RecentPanel);
		Layout->addStretch(1);
		setWidget(Column);

		// [[BPR-1357]]. The open row's Cancel or Retry: a real button on the row
		// that asked, so it paints natively and assistive technology reaches it.
		// It moves to the list that shows the row; Escape on that row presses it.
		OpenAction = new QToolButton{ Column };
		OpenAction->setObjectName(QStringLiteral("homeRowOpenActionButton"));
		OpenAction->setToolButtonStyle(Qt::ToolButtonTextOnly);
		OpenAction->setFocusPolicy(Qt::NoFocus);
		OpenAction->hide();
		connect(OpenAction, &QToolButton::clicked, this, [this] {
			if (OpenActionDispatch) {
				OpenActionDispatch();
			}
		});
		// The mark's turn is animation only: what is shown, and for how long, is
		// the core's opening and the application's wait.
		OpenTurn = new QTimer{ this };
		OpenTurn->setObjectName(QStringLiteral("homeOpenMarkTurn"));
		OpenTurn->setInterval(open_turn_interval);
		connect(OpenTurn, &QTimer::timeout, this, [this] { turn_open_mark(); });

		connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) { update_sticky_headers(); });
		connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int) { update_sticky_headers(); });
		refresh();
	}

	void home_list::adopt_rows(
		home_row_view&    aView,
		home_slice_model& aModel,
		const QString&    aName,
		const QString&    aAccessible)
	{
		aView.setObjectName(aName);
		aView.setAccessibleName(aAccessible);
		aView.setAccessibleDescription(home_open_description());
		aView.setModel(&aModel);
		aView.setItemDelegate(Delegate);
		aView.installEventFilter(this);
		aView.viewport()->installEventFilter(this);
		// [[BPR-1364]]. A single click only selects: the row becomes current,
		// takes focus and is highlighted. A double-click opens it, as Return
		// does (read in `eventFilter`) and the row menu's Open Review does.
		// Neither `clicked` nor `activated` is taken: `activated` is a single
		// click wherever the style asks for single-click activation (KDE), and
		// Return is already the list's own.
		connect(&aView, &QAbstractItemView::doubleClicked, this, [this, view = &aView](const QModelIndex& aIndex) { open_row(*view, aIndex); });
		connect(aView.selectionModel(), &QItemSelectionModel::currentChanged, this,
				[this, view = &aView](const QModelIndex& aCurrent) { adopt_current(view, aCurrent); });
		// [[BPR-1365]]. While another Home list has the keyboard, a row made
		// current here (a refresh restoring it, a right click) is not shown
		// selected: Home shows one selection.
		connect(aView.selectionModel(), &QItemSelectionModel::selectionChanged, this, [this, view = &aView] {
			if (!SelectionShown) {
				home_hide_selection(*view);
			}
		});
		connect_model_signals(aModel, *this, [this] { schedule_refresh(); });
		// [[BPR-1338]]. Open on GitHub: an icon on the hovered row and on the
		// current row while its list has focus, and the row's menu.
		auto* const page = new home_row_action{ aView, QStringLiteral("Open on GitHub"), home_row_role::icon_action };
		page->setObjectName(QStringLiteral("homeRowOpenOnGithubButton"));
		page->setIcon(PageIcon);
		page->setToolTip(PageToolTip);
		page->set_trigger([this, view = &aView](const int aRow) {
			if (!PageDispatch || view->model() == nullptr) {
				return;
			}
			if (const auto item = item_of(*view, view->model()->index(aRow, 0, view->rootIndex())); item.has_value()) {
				PageDispatch(*item);
			}
		});
		aView.setContextMenuPolicy(Qt::CustomContextMenu);
		connect(&aView, &QWidget::customContextMenuRequested, this, [this, view = &aView](const QPoint& aPosition) {
			const auto index = view->indexAt(aPosition);
			if (!index.isValid() || !RowMenuDispatch) {
				return;
			}
			view->setCurrentIndex(index);
			RowMenuDispatch(view->viewport()->mapToGlobal(aPosition));
		});
	}

	void home_list::set_selection_shown(const bool aShown)
	{
		SelectionShown = aShown;
		if (!aShown) {
			for (auto* const view : row_views()) {
				home_hide_selection(*view);
			}
		} else if (CurrentRows != nullptr) {
			home_show_selection(*CurrentRows);
		}
	}

	auto home_list::selection_shown() const noexcept -> bool
	{
		return SelectionShown;
	}

	void home_list::set_page_dispatch(std::function<void(const core::queue_item&)> aDispatch)
	{
		PageDispatch = std::move(aDispatch);
	}

	void home_list::set_page_action(const QIcon& aIcon, const QString& aToolTip)
	{
		PageIcon    = aIcon;
		PageToolTip = aToolTip;
		for (auto* const button : Column->findChildren<QToolButton*>(QStringLiteral("homeRowOpenOnGithubButton"))) {
			button->setIcon(PageIcon);
			button->setToolTip(PageToolTip);
		}
	}

	void home_list::set_row_menu_dispatch(std::function<void(const QPoint& aGlobal)> aDispatch)
	{
		RowMenuDispatch = std::move(aDispatch);
	}

	void home_list::set_current_observer(std::function<void()> aObserver)
	{
		CurrentObserver = std::move(aObserver);
	}

	void home_list::set_new_window_dispatch(std::function<void(const core::queue_item&)> aDispatch)
	{
		NewWindowDispatch = std::move(aDispatch);
	}

	void home_list::set_recent_new_window_dispatch(std::function<void(int aRow)> aDispatch)
	{
		RecentNewWindowDispatch = std::move(aDispatch);
	}

	void home_list::adopt_recent(home_recent_view& aRecent)
	{
		RecentView = &aRecent;
		RecentPanel->set_content(&aRecent);
		aRecent.setItemDelegate(Delegate);
		aRecent.installEventFilter(this);
		aRecent.viewport()->installEventFilter(this);
		RecentAction = new home_row_action{ aRecent, QStringLiteral("Resume") };
		RecentAction->setObjectName(QStringLiteral("homeRecentResumeRowButton"));
		RecentAction->setToolTip(QStringLiteral("Opens this pull request where its reading was left"));
		RecentAction->set_trigger([this](const int aRow) {
			if (RecentActionDispatch) {
				RecentActionDispatch(aRow);
			}
		});
		refresh();
	}

	void home_list::set_open_dispatch(std::function<void(const core::queue_item&)> aDispatch)
	{
		OpenDispatch = std::move(aDispatch);
	}

	void home_list::set_settings_dispatch(std::function<void()> aDispatch)
	{
		SettingsDispatch = std::move(aDispatch);
	}

	void home_list::set_recent_action_dispatch(std::function<void(int aRow)> aDispatch)
	{
		RecentActionDispatch = std::move(aDispatch);
	}

	void home_list::set_onboarding(const bool aOnboarding)
	{
		Onboarding = aOnboarding;
	}

	auto home_list::onboarding() const noexcept -> bool
	{
		return Onboarding;
	}

	void home_list::set_queue_status(const bool aReady, const bool aAccount)
	{
		if (QueueReady == aReady && QueueAccount == aAccount) {
			return;
		}
		QueueReady   = aReady;
		QueueAccount = aAccount;
		refresh();
	}

	void home_list::set_recent(const int aCount, const QString& aSummary)
	{
		RecentCount = aCount;
		RecentPanel->set_count(aCount);
		RecentPanel->set_summary(aSummary);
		refresh();
	}

	void home_list::set_state_colours(const home_state_colours& aColours)
	{
		Delegate->set_state_colours(aColours);
		for (auto* const panel : StatePanels) {
			panel->set_state_colours(aColours);
		}
		FlatPanel->set_state_colours(aColours);
		RecentPanel->set_state_colours(aColours);
		for (const auto& group : Groups) {
			group->Panel->set_state_colours(aColours);
			for (auto* const band : group->Bands) {
				if (band != nullptr) {
					band->set_state_colours(aColours);
				}
			}
		}
		for (auto* const view : row_views()) {
			view->viewport()->update();
		}
		if (RecentView != nullptr) {
			RecentView->viewport()->update();
		}
	}

	void home_list::set_grouping(const application::home_grouping aGrouping)
	{
		if (Grouping == aGrouping) {
			return;
		}
		// The row the reviewer had is kept: it is the same pull request, in a
		// different arrangement of the same queue.
		const auto current = current_queue_item();
		const auto focused = current.has_value() && CurrentRows != nullptr && CurrentRows->hasFocus();
		Grouping           = aGrouping;
		// [[BPR-0783]] review finding. Every group panel is retired first. A key
		// is a host and a value, so one word can be an owner here and an account
		// there: "octo-reviewer" is both. Reusing the widget would carry one
		// arrangement's collapse into the other, and only a panel built afresh
		// reads this grouping's own memory.
		retire_groups({ });
		refresh();
		if (current.has_value()) {
			// [[BPR-1364]]. With the keyboard on the row, it stays on that row.
			static_cast<void>(select(current->Ref, focused));
		}
	}

	void home_list::set_state_headers(const bool aHeaders)
	{
		if (StateHeaders == aHeaders) {
			return;
		}
		const auto current = current_queue_item();
		const auto focused = current.has_value() && CurrentRows != nullptr && CurrentRows->hasFocus();
		StateHeaders       = aHeaders;
		refresh();
		if (current.has_value()) {
			static_cast<void>(select(current->Ref, focused));
		}
	}

	void home_list::set_collapsed_panels(const std::uint8_t aCollapsed)
	{
		if (CollapsedPanels == aCollapsed) {
			return;
		}
		CollapsedPanels = aCollapsed;
		apply_collapsed_panels();
	}

	auto home_list::grouping() const noexcept -> application::home_grouping
	{
		return Grouping;
	}

	auto home_list::state_headers() const noexcept -> bool
	{
		return StateHeaders;
	}

	auto home_list::collapsed_panels() const noexcept -> std::uint8_t
	{
		return CollapsedPanels;
	}

	void home_list::set_view_observer(std::function<void()> aObserver)
	{
		ViewObserver = std::move(aObserver);
	}

	void home_list::apply_collapsed_panels()
	{
		// Adopting a view is not a gesture: it must not be reported back out as
		// one, or two windows of one profile would dispatch each other's views.
		ApplyingCollapse = true;
		for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
			StatePanels[index]->set_collapsed((CollapsedPanels & collapsed_bit(groups[index])) != 0U);
		}
		RecentPanel->set_collapsed((CollapsedPanels & application::home_collapsed_panel::recently_viewed) != 0U);
		ApplyingCollapse = false;
		update_sticky_headers();
	}

	void home_list::observe_nameless_collapse(home_panel& aPanel, const std::uint8_t aBit)
	{
		aPanel.set_collapse_observer([this, aBit](const bool aCollapsed) {
			const auto next    = aCollapsed
				? static_cast<std::uint8_t>(CollapsedPanels | aBit)
				: static_cast<std::uint8_t>(CollapsedPanels & static_cast<std::uint8_t>(~aBit));
			const auto changed = next != CollapsedPanels;
			CollapsedPanels    = next;
			update_sticky_headers();
			if (changed && !ApplyingCollapse && ViewObserver) {
				ViewObserver();
			}
		});
	}

	void home_list::observe_named_collapse(
		home_panel& aPanel, const home_group_key& aKey, const std::optional<home_group> aState)
	{
		const auto key = collapse_key(Grouping, aKey, aState);
		aPanel.set_collapse_observer([this, key](const bool aCollapsed) {
			// [[BPR-0783]] Decisions (a). A group panel's key names an owner, a
			// repository or an account. The window remembers its collapse while it
			// lives and never dispatches it, so nothing writes those names to the
			// settings store.
			GroupCollapse[key] = aCollapsed;
			update_sticky_headers();
		});
	}

	auto home_list::collapse_key(
		const application::home_grouping aGrouping,
		const home_group_key&            aKey,
		const std::optional<home_group>  aState) -> QString
	{
		// [[BPR-0783]] review finding. The grouping names which kind of thing the
		// value is: "octo-reviewer" can be an owner and a login on one host, and
		// collapsing the owner must not collapse the account.
		return QStringLiteral("%1\n%2\n%3\n%4")
			.arg(QString::number(static_cast<int>(aGrouping)),
				 aKey.Host,
				 aKey.Value,
				 aState.has_value() ? QString::number(static_cast<int>(*aState)) : QString{ });
	}

	void home_list::schedule_refresh()
	{
		if (RefreshPending) {
			return;
		}
		RefreshPending = true;
		// A publication moves many rows at once; the panels are recounted once,
		// after it, rather than once per row.
		QMetaObject::invokeMethod(
			this, [this] {
				if (RefreshPending) {
					refresh();
				}
			},
			Qt::QueuedConnection);
	}

	void home_list::refresh()
	{
		RefreshPending             = false;
		const auto* const source   = Queue.sourceModel();
		const auto        allEmpty = QueueReady && QueueAccount && source != nullptr && source->rowCount() == 0;
		const auto        shown    = QueueReady && QueueAccount;
		// [[BPR-0783]]. An empty queue has nothing to group: whatever the view
		// asks for, it is Needs me holding one sentence and no box at all.
		const auto stated  = shown && (allEmpty || (Grouping == application::home_grouping::none && StateHeaders));
		const auto flat    = shown && !allEmpty && Grouping == application::home_grouping::none && !StateHeaders;
		const auto grouped = shown && !allEmpty && Grouping != application::home_grouping::none;
		// With a state's own panel or band the colour is the header and the rail;
		// without one each row carries its state's 3-point mark instead.
		Delegate->set_marks(!StateHeaders);

		for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
			const auto  group = groups[index];
			auto* const model = StateModels[index];
			auto* const panel = StatePanels[index];
			model->set_chips(true);
			const auto rows  = model->rowCount();
			auto       items = std::vector<core::queue_item>{ };
			items.reserve(static_cast<std::size_t>(rows));
			for (auto row = 0; row < rows; ++row) {
				if (auto item = Queue.item_at(model->mapToSource(model->index(row, 0))); item.has_value()) {
					items.push_back(std::move(*item));
				}
			}
			panel->set_count(rows);
			panel->set_summary(home_state_summary(group, items));
			const auto sentence = group == home_group::needs_you && allEmpty && !Onboarding;
			// A list is shown by what it holds; whether its state belongs to this
			// arrangement at all is the panel's to say. Hiding the list itself
			// would take the keyboard from a row an activation had just made
			// current, which is what a notification's activation does.
			StateViews[index]->setVisible(rows > 0);
			if (group == home_group::needs_you) {
				EmptySentence->setVisible(sentence);
			}
			panel->setVisible(stated && (rows > 0 || sentence));
		}

		FlatModel->set_chips(true);
		const auto flatRows = FlatModel->rowCount();
		FlatView->setVisible(flatRows > 0);
		FlatPanel->setVisible(flat && flatRows > 0);

		if (grouped) {
			// One pass for every group's accounts, before the groups ask.
			const auto census = group_accounts();
			const auto keys   = ordered_keys();
			retire_groups(keys);
			// Placed in the order their first row in state order gives them, with
			// only a group that actually moved taken out of the column.
			auto target = Layout->indexOf(FlatPanel) + 1;
			for (const auto& key : keys) {
				auto& group = group_for(key, home_group_name(Grouping, key));
				refresh_group(group, census);
				if (Layout->indexOf(group.Panel) != target) {
					Layout->removeWidget(group.Panel);
					Layout->insertWidget(target, group.Panel);
				}
				++target;
			}
		} else {
			retire_groups({ });
		}

		RecentPanel->setVisible(RecentView != nullptr && RecentCount > 0);
		if (CurrentRows != nullptr) {
			// A list the arrangement no longer shows holds no current row.
			const auto live = row_views();
			if (std::ranges::find(live, CurrentRows) == live.end()) {
				CurrentRows = nullptr;
			}
		}
		update_sticky_headers();
		place_open_action();
	}

	auto home_list::ordered_keys() const -> std::vector<home_group_key>
	{
		// The queue in state order: state first, then its own order within a
		// state, which is the reviewer's chosen sort. A group follows its first
		// row in that order, so the order is read off it directly.
		auto ordered = std::vector<std::pair<int, int>>{ };
		ordered.reserve(static_cast<std::size_t>(Queue.rowCount()));
		for (auto row = 0; row < Queue.rowCount(); ++row) {
			if (const auto state = state_of_row(Queue.index(row, 0)); state.has_value()) {
				ordered.emplace_back(static_cast<int>(*state), row);
			}
		}
		std::ranges::stable_sort(ordered, [](const auto& aLeft, const auto& aRight) {
			return aLeft.first != aRight.first ? aLeft.first < aRight.first : aLeft.second < aRight.second;
		});
		auto keys = std::vector<home_group_key>{ };
		for (const auto& [state, row] : ordered) {
			static_cast<void>(state);
			auto key = key_of_row(Grouping, Queue.index(row, 0));
			if (!std::ranges::contains(keys, key)) {
				keys.push_back(std::move(key));
			}
		}
		return keys;
	}

	auto home_list::group_accounts() const -> std::map<home_group_key, std::vector<home_account>>
	{
		auto census = std::map<home_group_key, std::vector<home_account>>{ };
		for (auto row = 0; row < Queue.rowCount(); ++row) {
			const auto index    = Queue.index(row, 0);
			auto&      accounts = census[key_of_row(Grouping, index)];
			auto       account  = home_account{ .Host  = index.data(review_landing_model::host_role).toString(),
												.Login = index.data(review_landing_model::account_role).toString() };
			if (!std::ranges::contains(accounts, account)) {
				accounts.push_back(std::move(account));
			}
		}
		return census;
	}

	auto home_list::group_for(const home_group_key& aKey, const QString& aName) -> group_entry&
	{
		const auto found = std::ranges::find_if(
			Groups, [&aKey](const std::unique_ptr<group_entry>& aGroup) { return aGroup->Key == aKey; });
		if (found != Groups.end()) {
			(*found)->Name = aName;
			return **found;
		}
		auto entry  = std::make_unique<group_entry>();
		entry->Key  = aKey;
		entry->Name = aName;
		// A neutral panel: a group is not a state, so it carries no rail and no
		// tinted header of its own.
		entry->Panel = new home_panel{ std::nullopt, home_panel_style::panel, Column };
		entry->Panel->setObjectName(QStringLiteral("homeGroupPanel"));
		entry->Panel->header()->setObjectName(QStringLiteral("homeGroupPanelHeader"));
		entry->Panel->set_title(aName);
		entry->Content = new QWidget{ entry->Panel };
		entry->Column  = new QVBoxLayout{ entry->Content };
		entry->Column->setContentsMargins(0, 0, 0, 0);
		entry->Column->setSpacing(0);
		entry->Panel->set_content(entry->Content);
		entry->Panel->set_state_colours(Delegate->state_colours());
		observe_named_collapse(*entry->Panel, aKey, std::nullopt);
		auto* const panel = entry->Panel;
		panel->set_header_navigator([this, panel](const int aPressed) { return navigate_from_header(*panel, aPressed); });
		panel->installEventFilter(this);
		if (const auto remembered = GroupCollapse.find(collapse_key(Grouping, aKey, std::nullopt));
			remembered != GroupCollapse.end()) {
			ApplyingCollapse = true;
			panel->set_collapsed(remembered->second);
			ApplyingCollapse = false;
		}
		Layout->insertWidget(Layout->indexOf(RecentPanel), panel);
		Groups.push_back(std::move(entry));
		return *Groups.back();
	}

	void home_list::retire_groups(const std::vector<home_group_key>& aKept)
	{
		for (auto index = Groups.size(); index > 0U; --index) {
			auto& group = Groups[index - 1U];
			if (std::ranges::contains(aKept, group->Key)) {
				continue;
			}
			// A group whose key is gone from the queue is gone from the page. Its
			// collapse stays in the window's map, so the same owner collapsed
			// before is still collapsed if it comes back.
			if (CurrentRows != nullptr && group->Panel->isAncestorOf(CurrentRows)) {
				CurrentRows = nullptr;
			}
			// [[BPR-1364]]. A single click leaves the keyboard in the row it
			// selected, so the panel going may hold it: the list keeps it until
			// a row is chosen again, rather than a destroyed list handing it on.
			if (auto* const focused = QApplication::focusWidget();
				focused != nullptr && group->Panel->isAncestorOf(focused)) {
				setFocus(Qt::OtherFocusReason);
			}
			// The open row's Cancel or Retry lives in the list that shows the
			// row; it outlives that list and is placed again once rows exist.
			if (OpenAction != nullptr && group->Panel->isAncestorOf(OpenAction)) {
				OpenAction->hide();
				OpenAction->setParent(Column);
				QMetaObject::invokeMethod(this, [this] { place_open_action(); }, Qt::QueuedConnection);
			}
			delete group->Panel;
			Groups.erase(Groups.begin() + static_cast<std::ptrdiff_t>(index - 1U));
		}
	}

	void home_list::refresh_group(
		group_entry& aGroup, const std::map<home_group_key, std::vector<home_account>>& aAccounts)
	{
		aGroup.Panel->set_title(aGroup.Name);
		const auto counted  = aAccounts.find(aGroup.Key);
		const auto accounts = counted != aAccounts.end() ? counted->second : std::vector<home_account>{ };
		const auto many     = account_chips();
		// [[BPR-0783]]. A row drops its chip where the group already says the
		// account: under Account grouping, and in a single-account group whose
		// header carries it instead.
		const auto chips = many && Grouping != application::home_grouping::account && accounts.size() > 1U;
		auto       count = 0;
		if (StateHeaders) {
			if (aGroup.Bands.front() == nullptr) {
				for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
					const auto  state = groups[index];
					auto* const model = new home_slice_model{ Queue, aGroup.Panel };
					auto* const band  = new home_panel{ state, home_panel_style::band, aGroup.Content };
					auto* const view  = new home_row_view{ band };
					model->set_state(state);
					model->set_key(Grouping, aGroup.Key);
					band->setObjectName(QStringLiteral("homeGroupBand"));
					band->header()->setObjectName(QStringLiteral("homeGroupBandHeader"));
					band->set_title(home_group_label(state));
					band->set_state_colours(Delegate->state_colours());
					adopt_rows(*view, *model, QStringLiteral("homeGroupBandRows"), home_group_label(state));
					band->set_content(view);
					observe_named_collapse(*band, aGroup.Key, state);
					band->set_header_navigator([this, band](const int aKey) { return navigate_from_header(*band, aKey); });
					band->installEventFilter(this);
					if (const auto remembered = GroupCollapse.find(collapse_key(Grouping, aGroup.Key, state));
						remembered != GroupCollapse.end()) {
						ApplyingCollapse = true;
						band->set_collapsed(remembered->second);
						ApplyingCollapse = false;
					}
					aGroup.Column->addWidget(band);
					aGroup.Bands[index]      = band;
					aGroup.BandModels[index] = model;
					aGroup.BandViews[index]  = view;
				}
			}
			for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
				auto* const model = aGroup.BandModels[index];
				model->set_key(Grouping, aGroup.Key);
				model->set_chips(chips);
				const auto rows  = model->rowCount();
				auto       items = std::vector<core::queue_item>{ };
				items.reserve(static_cast<std::size_t>(rows));
				for (auto row = 0; row < rows; ++row) {
					if (auto item = Queue.item_at(model->mapToSource(model->index(row, 0))); item.has_value()) {
						items.push_back(std::move(*item));
					}
				}
				aGroup.Bands[index]->set_count(rows);
				aGroup.Bands[index]->set_summary(home_state_summary(groups[index], items));
				// A state with nothing in this group is not shown.
				aGroup.BandViews[index]->setVisible(rows > 0);
				aGroup.Bands[index]->setVisible(rows > 0);
				count += rows;
			}
			if (aGroup.RowsView != nullptr) {
				aGroup.RowsView->hide();
			}
		} else {
			if (aGroup.RowsModel == nullptr) {
				aGroup.RowsModel = new home_slice_model{ Queue, aGroup.Panel };
				aGroup.RowsModel->set_state_ordered(true);
				aGroup.RowsView = new home_row_view{ aGroup.Content };
				adopt_rows(*aGroup.RowsView, *aGroup.RowsModel, QStringLiteral("homeGroupRows"), aGroup.Name);
				aGroup.Column->insertWidget(0, aGroup.RowsView);
			}
			aGroup.RowsModel->set_key(Grouping, aGroup.Key);
			aGroup.RowsModel->set_chips(chips);
			aGroup.RowsView->setAccessibleName(aGroup.Name);
			count = aGroup.RowsModel->rowCount();
			aGroup.RowsView->setVisible(count > 0);
			for (auto* const band : aGroup.Bands) {
				if (band != nullptr) {
					band->hide();
				}
			}
		}
		aGroup.Panel->set_count(count);
		// [[BPR-0783]] Decisions (d). No avatar image is cached, so every avatar
		// is an initial tile. Owner kind is not projected: an owner's tile is
		// round when the owner is one of the queue's own logins, square otherwise,
		// and an account's tile is always round.
		const auto account = Grouping == application::home_grouping::account;
		const auto round   = account
			|| std::ranges::any_of(Queue.accounts(), [&aGroup](const home_account& aCandidate) {
							   return aCandidate.Login.compare(aGroup.Key.Value, Qt::CaseInsensitive) == 0;
						   });
		aGroup.Panel->set_avatar(aGroup.Name, round);
		if (account) {
			// Board H8: the account's own host at the header's right. A group's
			// name never carries a host itself.
			aGroup.Panel->set_trailing(aGroup.Key.Host, false);
		} else if (many && accounts.size() == 1U) {
			aGroup.Panel->set_trailing(home_account_chip_text(accounts.front().Login, accounts.front().Host), true);
		} else {
			aGroup.Panel->set_trailing(QString{ }, false);
		}
		aGroup.Panel->setVisible(count > 0);
	}

	void home_list::update_sticky_headers()
	{
		// A panel's own Show or Hide reaches this through the list's event filter,
		// and the first of them arrives while the list is still building itself.
		// Every panel is therefore checked: half of them do not exist yet.
		const auto stick = [this](home_panel* const aPanel) {
			if (aPanel != nullptr) {
				stick_home_header(*aPanel, *viewport(), 0);
			}
		};
		for (auto* const panel : StatePanels) {
			stick(panel);
		}
		stick(FlatPanel);
		for (const auto& group : Groups) {
			stick(group->Panel);
		}
		stick(RecentPanel);
	}

	void home_list::adopt_current(QAbstractItemView* const aView, const QModelIndex& aCurrent)
	{
		if (!aCurrent.isValid()) {
			if (CurrentRows == aView) {
				CurrentRows = nullptr;
			}
			return;
		}
		CurrentRows = aView;
		if (CurrentObserver) {
			CurrentObserver();
		}
		// One current row on the page: making a row current in one list clears
		// the others, whichever arrangement they belong to.
		for (auto* const view : row_views()) {
			if (view == aView) {
				continue;
			}
			auto* const selection = view->selectionModel();
			if (selection != nullptr && (selection->currentIndex().isValid() || selection->hasSelection())) {
				const QSignalBlocker blocked{ selection };
				selection->clear();
				view->viewport()->update();
			}
		}
		if (!RestoringSelection) {
			if (auto* const panel = panel_of(*aView); panel != nullptr) {
				ensure_row_visible(*aView, aCurrent, *panel);
			}
		}
	}

	void home_list::open_row(const QAbstractItemView& aView, const QModelIndex& aIndex)
	{
		// [[BPR-1357]]. A row already being opened asks for nothing again: a
		// double click or a second Return is the same request.
		if (!OpenDispatch || opening(aIndex)) {
			return;
		}
		if (const auto item = item_of(aView, aIndex); item.has_value()) {
			OpenDispatch(*item);
		}
	}

	void home_list::open_row_in_new_window(const QAbstractItemView& aView, const QModelIndex& aIndex)
	{
		if (&aView == RecentView) {
			if (RecentNewWindowDispatch && aIndex.isValid()) {
				RecentNewWindowDispatch(aIndex.row());
			}
			return;
		}
		if (!NewWindowDispatch) {
			return;
		}
		if (const auto item = item_of(aView, aIndex); item.has_value()) {
			NewWindowDispatch(*item);
		}
	}

	auto home_list::read_new_window_click(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		const auto type = aEvent->type();
		if (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonRelease && type != QEvent::MouseButtonDblClick) {
			return false;
		}
		auto* view = static_cast<QAbstractItemView*>(nullptr);
		if (RecentView != nullptr && aWatched == RecentView->viewport()) {
			view = RecentView;
		} else {
			for (auto* const candidate : row_views()) {
				if (aWatched == candidate->viewport()) {
					view = candidate;
					break;
				}
			}
		}
		if (view == nullptr) {
			return false;
		}
		// [[BPR-1342]]. A browser's gestures for "open this elsewhere": the
		// middle button, or the primary one with ⌘ (Ctrl off macOS). Taken
		// whole, so the click does not also open the row here and ⌘ does not
		// toggle the row's selection away.
		const auto* const mouse     = static_cast<QMouseEvent*>(aEvent);
		const auto        elsewhere = mouse->button() == Qt::MiddleButton
			|| (mouse->button() == Qt::LeftButton && mouse->modifiers().testFlag(Qt::ControlModifier));
		if (!elsewhere) {
			return false;
		}
		const auto index = view->indexAt(mouse->position().toPoint());
		if (type == QEvent::MouseButtonPress) {
			NewWindowPress = index;
			if (index.isValid()) {
				view->setFocus(Qt::MouseFocusReason);
				view->setCurrentIndex(index);
			}
		} else if (type == QEvent::MouseButtonRelease) {
			const auto pressed = std::exchange(NewWindowPress, QPersistentModelIndex{ });
			if (index.isValid() && pressed == index) {
				open_row_in_new_window(*view, index);
			}
		}
		return true;
	}

	auto home_list::item_of(const QAbstractItemView& aView, const QModelIndex& aIndex) const
		-> std::optional<core::queue_item>
	{
		const auto* const proxy = qobject_cast<const QSortFilterProxyModel*>(aView.model());
		if (proxy == nullptr) {
			return std::nullopt;
		}
		return Queue.item_at(proxy->mapToSource(aIndex));
	}

	void home_list::ensure_row_visible(QAbstractItemView& aView, const QModelIndex& aIndex, const home_panel& aPanel)
	{
		const auto rect = aView.visualRect(aIndex);
		if (!rect.isValid() || !aView.isVisibleTo(Column)) {
			return;
		}
		const auto top = aView.viewport()->mapTo(Column, rect.topLeft());
		// Clear of the header that sticks over the top of the panel.
		ensureVisible(top.x(), top.y() + (rect.height() / 2), 0, (rect.height() / 2) + aPanel.header_height());
	}

	auto home_list::select(const core::pull_request_ref& aRef, const bool aFocus) -> bool
	{
		for (auto* const view : row_views()) {
			auto* const model = view->model();
			if (model == nullptr) {
				continue;
			}
			for (auto row = 0; row < model->rowCount(); ++row) {
				const auto index = model->index(row, 0);
				const auto item  = item_of(*view, index);
				if (!item.has_value() || item->Ref != aRef) {
					continue;
				}
				if (aFocus) {
					// The band and the group that hold the row are expanded, so
					// focusing it never lands on a row nobody can see.
					for (auto* widget = static_cast<QWidget*>(view); widget != nullptr && widget != Column;
						 widget       = widget->parentWidget()) {
						if (auto* const panel = dynamic_cast<home_panel*>(widget); panel != nullptr) {
							panel->set_collapsed(false);
						}
					}
				}
				RestoringSelection = !aFocus;
				view->setCurrentIndex(index);
				adopt_current(view, index);
				RestoringSelection = false;
				if (aFocus) {
					view->setFocus(Qt::OtherFocusReason);
					if (auto* const panel = panel_of(*view); panel != nullptr) {
						ensure_row_visible(*view, index, *panel);
					}
				}
				return true;
			}
		}
		return false;
	}

	void home_list::focus_first_row()
	{
		for (const auto& stop : reading_order()) {
			if (!stop.Panel->collapsed() && stop.Rows != nullptr && focus_row(*stop.Rows, 0)) {
				return;
			}
			if (stop.Panel->header_shown()) {
				stop.Panel->header()->setFocus(Qt::OtherFocusReason);
				return;
			}
		}
	}

	auto home_list::current_queue_item() const -> std::optional<core::queue_item>
	{
		if (CurrentRows == nullptr) {
			return std::nullopt;
		}
		const auto index = CurrentRows->currentIndex();
		if (!index.isValid()) {
			return std::nullopt;
		}
		return item_of(*CurrentRows, index);
	}

	auto home_list::current_view() const -> QListView*
	{
		if (CurrentRows == nullptr || !CurrentRows->currentIndex().isValid()) {
			return nullptr;
		}
		return qobject_cast<QListView*>(CurrentRows);
	}

	auto home_list::state_panel(const home_group aGroup) const noexcept -> home_panel*
	{
		return StatePanels[static_cast<std::size_t>(aGroup)];
	}

	auto home_list::state_view(const home_group aGroup) const noexcept -> QListView*
	{
		return StateViews[static_cast<std::size_t>(aGroup)];
	}

	auto home_list::state_model(const home_group aGroup) const noexcept -> home_slice_model*
	{
		return StateModels[static_cast<std::size_t>(aGroup)];
	}

	auto home_list::flat_panel() const noexcept -> home_panel*
	{
		return FlatPanel;
	}

	auto home_list::flat_view() const noexcept -> QListView*
	{
		return FlatView;
	}

	auto home_list::group_panels() const -> std::vector<home_panel*>
	{
		// In the column's own order, which is the order the groups are shown in.
		auto panels = std::vector<home_panel*>{ };
		for (auto index = 0; index < Layout->count(); ++index) {
			auto* const widget = Layout->itemAt(index)->widget();
			for (const auto& group : Groups) {
				if (group->Panel == widget && !group->Panel->isHidden()) {
					panels.push_back(group->Panel);
				}
			}
		}
		return panels;
	}

	auto home_list::group_panel(const QString& aName) const -> home_panel*
	{
		for (auto* const panel : group_panels()) {
			if (panel->title() == aName) {
				return panel;
			}
		}
		return nullptr;
	}

	auto home_list::band_panel(const home_panel& aGroup, const home_group aState) const -> home_panel*
	{
		for (const auto& group : Groups) {
			if (group->Panel != &aGroup) {
				continue;
			}
			auto* const band = group->Bands[static_cast<std::size_t>(aState)];
			return band != nullptr && !band->isHidden() ? band : nullptr;
		}
		return nullptr;
	}

	auto home_list::rows_view(const home_panel& aPanel) const -> QAbstractItemView*
	{
		const auto shown = [](home_row_view* const aView) -> QAbstractItemView* {
			return aView != nullptr && !aView->isHidden() ? aView : nullptr;
		};
		if (&aPanel == RecentPanel) {
			return RecentView;
		}
		if (&aPanel == FlatPanel) {
			return shown(FlatView);
		}
		for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
			if (StatePanels[index] == &aPanel) {
				return shown(StateViews[index]);
			}
		}
		for (const auto& group : Groups) {
			if (group->Panel == &aPanel) {
				return shown(group->RowsView);
			}
			for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
				if (group->Bands[index] == &aPanel) {
					return shown(group->BandViews[index]);
				}
			}
		}
		return nullptr;
	}

	auto home_list::account_chips() const -> bool
	{
		return Queue.accounts().size() > 1U;
	}

	auto home_list::panel_of(const QAbstractItemView& aView) const -> home_panel*
	{
		for (auto* widget = aView.parentWidget(); widget != nullptr && widget != Column; widget = widget->parentWidget()) {
			if (auto* const panel = dynamic_cast<home_panel*>(widget); panel != nullptr) {
				return panel;
			}
		}
		return nullptr;
	}

	auto home_list::row_views() const -> std::vector<QAbstractItemView*>
	{
		// The lists of the arrangement that is on screen, and only those: the
		// other arrangements keep their models, so a row exists in several of
		// them at once and the page would have more than one current row.
		// As `update_sticky_headers`: an event can reach the list before every
		// view it will own exists, so a view that is not there yet is not one.
		auto       views = std::vector<QAbstractItemView*>{ };
		const auto keep  = [&views](QAbstractItemView* const aView) {
			if (aView != nullptr) {
				views.push_back(aView);
			}
		};
		if (Grouping == application::home_grouping::none) {
			if (StateHeaders) {
				for (auto* const view : StateViews) {
					keep(view);
				}
			} else {
				keep(FlatView);
			}
			return views;
		}
		for (const auto& group : Groups) {
			if (StateHeaders) {
				for (auto* const band : group->BandViews) {
					keep(band);
				}
			} else {
				keep(group->RowsView);
			}
		}
		// An empty ready queue is Needs me holding one sentence whatever the
		// grouping, so its own list is the one that is on screen.
		if (Groups.empty()) {
			for (auto* const view : StateViews) {
				keep(view);
			}
		}
		return views;
	}

	auto home_list::reading_order() const -> std::vector<panel_stop>
	{
		auto       stops   = std::vector<panel_stop>{ };
		const auto collect = [this, &stops](home_panel& aPanel, const auto& aSelf) -> void {
			if (aPanel.isHidden()) {
				return;
			}
			stops.push_back({ .Panel = &aPanel, .Rows = rows_view(aPanel) });
			if (aPanel.collapsed() || aPanel.content() == nullptr) {
				return;
			}
			// The bands are the content's direct children, created in state order.
			for (auto* const child : aPanel.content()->children()) {
				if (auto* const band = dynamic_cast<home_panel*>(child); band != nullptr) {
					aSelf(*band, aSelf);
				}
			}
		};
		for (auto index = 0; index < Layout->count(); ++index) {
			if (auto* const panel = dynamic_cast<home_panel*>(Layout->itemAt(index)->widget()); panel != nullptr) {
				collect(*panel, collect);
			}
		}
		return stops;
	}

	auto home_list::empty_sentence() const noexcept -> QLabel*
	{
		return EmptySentence;
	}

	auto home_list::recent_panel() const noexcept -> home_panel*
	{
		return RecentPanel;
	}

	auto home_list::recent_action() const noexcept -> home_row_action*
	{
		return RecentAction;
	}

	auto home_list::row_delegate() const noexcept -> home_row_delegate*
	{
		return Delegate;
	}

	auto home_list::queue_shown() const -> bool
	{
		return QueueAccount && isVisible();
	}

	auto home_list::focus_chain() const -> std::vector<QWidget*>
	{
		auto chain = std::vector<QWidget*>{ };
		for (const auto& stop : reading_order()) {
			if (stop.Panel->header_shown()) {
				chain.push_back(stop.Panel->header());
			}
			if (stop.Panel->collapsed()) {
				continue;
			}
			if (stop.Rows != nullptr) {
				chain.push_back(stop.Rows);
			}
			if (stop.Panel == StatePanels[0U] && !EmptySentence->isHidden()) {
				chain.push_back(EmptySentence);
			}
		}
		return chain;
	}

	auto home_list::focus_row(QAbstractItemView& aView, const int aRow) -> bool
	{
		auto* const model = aView.model();
		if (model == nullptr || aRow < 0 || aRow >= model->rowCount(aView.rootIndex())) {
			return false;
		}
		aView.setFocus(Qt::OtherFocusReason);
		aView.setCurrentIndex(model->index(aRow, 0, aView.rootIndex()));
		return true;
	}

	auto home_list::navigate_from_header(const home_panel& aPanel, const int aKey) -> bool
	{
		const auto stops = reading_order();
		const auto found = std::ranges::find_if(stops, [&aPanel](const panel_stop& aStop) { return aStop.Panel == &aPanel; });
		if (found == stops.end()) {
			return false;
		}
		const auto at = static_cast<std::size_t>(found - stops.begin());
		if (aKey == Qt::Key_Down) {
			if (!aPanel.collapsed()) {
				if (found->Rows != nullptr && focus_row(*found->Rows, 0)) {
					return true;
				}
				if (&aPanel == StatePanels[0U] && !EmptySentence->isHidden()) {
					EmptySentence->setFocus(Qt::TabFocusReason);
					return true;
				}
			}
			if (at + 1U < stops.size()) {
				// The next stop in reading order: a band inside this group, or
				// whatever panel follows it.
				const auto& next = stops[at + 1U];
				if (next.Panel->header_shown()) {
					next.Panel->header()->setFocus(Qt::OtherFocusReason);
					return true;
				}
				return next.Rows != nullptr && focus_row(*next.Rows, 0);
			}
			return false;
		}
		if (aKey == Qt::Key_Up && at > 0U) {
			const auto& previous = stops[at - 1U];
			if (!previous.Panel->collapsed() && previous.Rows != nullptr && previous.Rows->model() != nullptr
				&& focus_row(*previous.Rows, previous.Rows->model()->rowCount(previous.Rows->rootIndex()) - 1)) {
				return true;
			}
			if (previous.Panel->header_shown()) {
				previous.Panel->header()->setFocus(Qt::OtherFocusReason);
				return true;
			}
			return false;
		}
		return false;
	}

	void home_list::derive_link_colour()
	{
		if (EmptySentence == nullptr) {
			return;
		}
		// The link reads in the text's own colour, underlined: blue on Home is a
		// state, and a link is not one. Only the two link roles are set, so every
		// other role keeps following the palette the label inherits, and a
		// theme change re-derives them from the new text colour.
		const auto current = EmptySentence->palette();
		auto       links   = QPalette{ };
		auto       changed = false;
		for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
			const auto text = current.color(group, QPalette::Text);
			for (const auto role : { QPalette::Link, QPalette::LinkVisited }) {
				links.setColor(group, role, text);
				changed = changed || current.color(group, role) != text;
			}
		}
		if (changed) {
			EmptySentence->setPalette(links);
		}
	}

	void home_list::set_open(home_row_open aOpen)
	{
		const auto& previous = Delegate->open();
		aOpen.Frame          = previous.Frame;
		if (aOpen == previous) {
			return;
		}
		const auto relaid = aOpen.Identity != previous.Identity || aOpen.Phase != previous.Phase || aOpen.Line != previous.Line
			|| aOpen.Action != previous.Action;
		Delegate->set_open(std::move(aOpen));
		const auto turning = Delegate->open().Phase == home_row_open_phase::opening || Delegate->open().Phase == home_row_open_phase::waiting;
		if (turning && !OpenTurn->isActive()) {
			OpenTurn->start();
		} else if (!turning) {
			OpenTurn->stop();
		}
		if (relaid) {
			// The open row's line is set larger than a reason, so its height may
			// change: every list lays out again, which refits each panel.
			auto views = row_views();
			if (RecentView != nullptr) {
				views.push_back(RecentView);
			}
			for (auto* const view : views) {
				view->doItemsLayout();
				view->viewport()->update();
			}
		}
		place_open_action();
	}

	auto home_list::open_index(const QAbstractItemView& aView) const -> QModelIndex
	{
		auto* const model = aView.model();
		if (model == nullptr || Delegate->open().Phase == home_row_open_phase::none) {
			return { };
		}
		if (Delegate->opens(aView.currentIndex())) {
			return aView.currentIndex();
		}
		for (auto row = 0; row < model->rowCount(aView.rootIndex()); ++row) {
			if (const auto index = model->index(row, 0, aView.rootIndex()); Delegate->opens(index)) {
				return index;
			}
		}
		return { };
	}

	auto home_list::open_view() const -> QAbstractItemView*
	{
		if (Delegate->open().Phase == home_row_open_phase::none) {
			return nullptr;
		}
		// Shown lists only, in reading order, with Recently viewed last; the
		// list whose current row is the open row first, since that is the
		// one the reviewer used.
		auto views = row_views();
		if (RecentView != nullptr) {
			views.push_back(RecentView);
		}
		const auto shown = [this](const QAbstractItemView* const aView) {
			auto* const panel = aView == RecentView ? RecentPanel : panel_of(*aView);
			return aView->isVisibleTo(Column) && (panel == nullptr || (panel->isVisibleTo(Column) && !panel->collapsed()));
		};
		for (auto* const view : views) {
			if (shown(view) && Delegate->opens(view->currentIndex())) {
				return view;
			}
		}
		for (auto* const view : views) {
			if (shown(view) && open_index(*view).isValid()) {
				return view;
			}
		}
		return nullptr;
	}

	auto home_list::open_action() const noexcept -> QToolButton*
	{
		return OpenAction;
	}

	void home_list::set_open_action_dispatch(std::function<void()> aDispatch)
	{
		OpenActionDispatch = std::move(aDispatch);
	}

	auto home_list::opening(const QModelIndex& aIndex) const -> bool
	{
		const auto phase = Delegate->open().Phase;
		return (phase == home_row_open_phase::opening || phase == home_row_open_phase::waiting) && Delegate->opens(aIndex);
	}

	void home_list::place_open_action()
	{
		if (OpenAction == nullptr) {
			return;
		}
		const auto& open = Delegate->open();
		auto* const view = open.Action.isEmpty() ? nullptr : open_view();
		if (view == nullptr) {
			OpenAction->hide();
			return;
		}
		const auto index = open_index(*view);
		if (OpenAction->parentWidget() != view->viewport()) {
			OpenAction->setParent(view->viewport());
		}
		auto option = QStyleOptionViewItem{ };
		option.initFrom(view);
		option.rect   = view->visualRect(index);
		option.font   = view->font();
		option.widget = view;
		OpenAction->setText(open.Action);
		const auto identity = index.data(home_row_role::identity).toString();
		OpenAction->setAccessibleName(open.Phase == home_row_open_phase::failed
										  ? QStringLiteral("Retry opening %1").arg(identity)
										  : QStringLiteral("Cancel opening %1").arg(identity));
		OpenAction->setToolTip(open.Phase == home_row_open_phase::failed
								   ? QStringLiteral("Open %1 again").arg(identity)
								   : QStringLiteral("Stop opening %1 (Esc)").arg(identity));
		OpenAction->setGeometry(Delegate->open_action_rect(option, index));
		OpenAction->show();
		OpenAction->raise();
	}

	void home_list::turn_open_mark()
	{
		auto open = Delegate->open();
		if (open.Phase != home_row_open_phase::opening && open.Phase != home_row_open_phase::waiting) {
			OpenTurn->stop();
			return;
		}
		open.Frame = (open.Frame + 1) % home_open_frames;
		Delegate->set_open(open);
		auto views = row_views();
		if (RecentView != nullptr) {
			views.push_back(RecentView);
		}
		for (auto* const view : views) {
			if (const auto index = open_index(*view); index.isValid() && view->isVisibleTo(Column)) {
				view->viewport()->update(view->visualRect(index));
			}
		}
	}

	auto home_list::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent == nullptr) {
			return QScrollArea::eventFilter(aWatched, aEvent);
		}
		if (aWatched == EmptySentence && aEvent->type() == QEvent::PaletteChange) {
			derive_link_colour();
		}
		const auto views = row_views();
		if (aEvent->type() == QEvent::Wheel) {
			// A panel's rows are exactly as tall as they are and never scroll on
			// their own, so a wheel over them is the page's. Qt does not pass a
			// wheel a scroll area could not use on to its parents when the event
			// was synthesized, so the page is handed it directly rather than
			// relying on propagation.
			const auto rows = std::ranges::any_of(views, [aWatched](const QAbstractItemView* const aView) {
								  return aWatched == aView->viewport();
							  })
				|| (RecentView != nullptr && aWatched == RecentView->viewport());
			if (rows) {
				QCoreApplication::sendEvent(verticalScrollBar(), aEvent);
				return true;
			}
		}
		if (aWatched == EmptySentence && aEvent->type() == QEvent::KeyPress) {
			// The sentence holds one link, so the focused sentence is that link:
			// Return, Enter or Space follows it. A label otherwise reaches its
			// link only by a Tab inside it, which a reviewer arriving by the
			// list's own order never makes.
			const auto key = static_cast<QKeyEvent*>(aEvent)->key();
			if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space) {
				if (SettingsDispatch) {
					SettingsDispatch();
				}
				return true;
			}
		}
		if (aEvent->type() == QEvent::Resize || aEvent->type() == QEvent::Move || aEvent->type() == QEvent::Show
			|| aEvent->type() == QEvent::Hide) {
			if (dynamic_cast<home_panel*>(aWatched) != nullptr) {
				update_sticky_headers();
			}
		}
		if (read_new_window_click(aWatched, aEvent)) {
			return true;
		}
		// [[BPR-1357]]. A list that changed its size moved its rows.
		if ((aEvent->type() == QEvent::Resize || aEvent->type() == QEvent::LayoutRequest) && OpenAction != nullptr
			&& OpenAction->parentWidget() == aWatched) {
			QMetaObject::invokeMethod(this, [this] { place_open_action(); }, Qt::QueuedConnection);
		}
		if (aEvent->type() != QEvent::KeyPress) {
			return QScrollArea::eventFilter(aWatched, aEvent);
		}
		const auto* const key = static_cast<QKeyEvent*>(aEvent);
		// [[BPR-1357]]. Escape on the row being opened presses its Cancel.
		if (key->key() == Qt::Key_Escape && OpenAction != nullptr && OpenAction->isVisible()
			&& Delegate->open().Phase == home_row_open_phase::waiting) {
			if (auto* const view = qobject_cast<QAbstractItemView*>(aWatched);
				view != nullptr && view == open_view() && Delegate->opens(view->currentIndex())) {
				OpenAction->click();
				return true;
			}
		}
		const auto unshifted = key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::KeypadModifier;
		for (auto* const view : views) {
			if (aWatched != view) {
				continue;
			}
			if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
				// [[BPR-0736]]. macOS treats these as editing keys instead of
				// emitting activated. Consume them here on every platform so a
				// key opens once; the pointer keeps its own path. [[BPR-1342]]:
				// with ⌘ (Ctrl off macOS) the row opens in a new window.
				if (view->currentIndex().isValid()) {
					if (key->modifiers().testFlag(Qt::ControlModifier)) {
						open_row_in_new_window(*view, view->currentIndex());
					} else {
						open_row(*view, view->currentIndex());
					}
				}
				return true;
			}
			const auto  current = view->currentIndex();
			const auto  stops   = reading_order();
			auto* const panel   = panel_of(*view);
			const auto  found   = std::ranges::find_if(stops, [panel](const panel_stop& aStop) { return aStop.Panel == panel; });
			if (key->key() == Qt::Key_Up && unshifted && (!current.isValid() || current.row() == 0)) {
				if (panel != nullptr && panel->header_shown()) {
					panel->header()->setFocus(Qt::OtherFocusReason);
					return true;
				}
				if (found != stops.end() && found != stops.begin()) {
					const auto& previous = *(found - 1);
					if (previous.Panel->header_shown()) {
						previous.Panel->header()->setFocus(Qt::OtherFocusReason);
						return true;
					}
				}
				return QScrollArea::eventFilter(aWatched, aEvent);
			}
			if (key->key() == Qt::Key_Down && unshifted && current.isValid() && view->model() != nullptr
				&& current.row() == view->model()->rowCount(view->rootIndex()) - 1) {
				if (found != stops.end() && found + 1 != stops.end()) {
					const auto& next = *(found + 1);
					if (next.Panel->header_shown()) {
						next.Panel->header()->setFocus(Qt::OtherFocusReason);
						return true;
					}
					if (next.Rows != nullptr && focus_row(*next.Rows, 0)) {
						return true;
					}
				}
			}
			return QScrollArea::eventFilter(aWatched, aEvent);
		}
		if (aWatched == RecentView && key->key() == Qt::Key_Up && unshifted
			&& (!RecentView->currentIndex().isValid() || RecentView->currentIndex().row() == 0)) {
			RecentPanel->header()->setFocus(Qt::OtherFocusReason);
			return true;
		}
		return QScrollArea::eventFilter(aWatched, aEvent);
	}
} // namespace bpr::qt
