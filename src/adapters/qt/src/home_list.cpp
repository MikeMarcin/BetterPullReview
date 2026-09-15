#include "bpr/qt/home_list.hpp"

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
#include <QStyleOptionViewItem>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr auto state_count = std::size_t{ 3U };
		/// The all-empty sentence sits at a row's inset inside its panel.
		constexpr auto sentence_inset          = 14;
		constexpr auto sentence_inset_vertical = 12;

		constexpr auto groups = std::array{
			home_group::needs_you,
			home_group::waiting_on_others,
			home_group::following,
		};

		[[nodiscard]] auto panel_name(const home_group aGroup) -> QString
		{
			switch (aGroup) {
			case home_group::needs_you:
				return QStringLiteral("homeNeedsMePanel");
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
			case home_group::waiting_on_others:
				return QStringLiteral("homeWaitingOnOthersRows");
			case home_group::following:
				break;
			}
			return QStringLiteral("homeFollowingRows");
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

	// ---- home_state_filter_model --------------------------------------------

	home_state_filter_model::home_state_filter_model(
		const home_group    aGroup,
		QAbstractItemModel& aSource,
		QObject* const      aParent)
		: QSortFilterProxyModel{ aParent }
		, Group{ aGroup }
	{
		setDynamicSortFilter(true);
		setSourceModel(&aSource);
		sort(-1);
	}

	auto home_state_filter_model::group() const noexcept -> home_group
	{
		return Group;
	}

	auto home_state_filter_model::filterAcceptsRow(const int aSourceRow, const QModelIndex& aSourceParent) const -> bool
	{
		const auto group = sourceModel()->index(aSourceRow, 0, aSourceParent).data(review_landing_model::group_role);
		return group.isValid() && group.toInt() == static_cast<int>(Group);
	}

	// ---- home_row_action ----------------------------------------------------

	home_row_action::home_row_action(QAbstractItemView& aView, const QString& aText)
		: QToolButton{ aView.viewport() }
		, View{ aView }
	{
		setText(aText);
		setToolButtonStyle(Qt::ToolButtonTextOnly);
		setFocusPolicy(Qt::NoFocus);
		setAutoRaise(false);
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
			return aRow >= 0 && model != nullptr && aRow < model->rowCount(View.rootIndex())
				&& !model->index(aRow, 0, View.rootIndex()).data(home_row_role::row_action).toString().isEmpty();
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
		setText(index.data(home_row_role::row_action).toString());
		setAccessibleName(QStringLiteral("%1 %2").arg(text(), index.data(home_row_role::identity).toString()));
		setGeometry(home_row_delegate::action_rect(option, index));
		show();
		raise();
	}

	auto home_row_action::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
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
			auto* const model   = new home_state_filter_model{ group, Queue, this };
			auto* const panel   = new home_panel{ group, home_panel_style::panel, Column };
			auto* const content = new QWidget{ panel };
			auto* const column  = new QVBoxLayout{ content };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(0);
			auto* const view = new home_row_view{ content };
			view->setObjectName(rows_name(group));
			view->setAccessibleName(home_group_label(group));
			view->setModel(model);
			view->setItemDelegate(Delegate);
			view->installEventFilter(this);
			view->viewport()->installEventFilter(this);
			column->addWidget(view);
			panel->setObjectName(panel_name(group));
			panel->header()->setObjectName(panel_name(group) + QStringLiteral("Header"));
			panel->set_title(home_group_label(group));
			panel->set_content(content);
			panel->set_collapse_observer([this](bool) { update_sticky_headers(); });
			panel->set_header_navigator([this, panel](const int aKey) { return navigate_from_header(*panel, aKey); });
			panel->installEventFilter(this);
			panel->hide();
			if (group == home_group::needs_you) {
				// RFC-BPR-0032: empty is a sentence, never a box. The link is the
				// page the queue's saved repositories and rules are kept on.
				EmptySentence = new QLabel{ content };
				EmptySentence->setObjectName(QStringLiteral("homeEmptySentence"));
				EmptySentence->setTextFormat(Qt::RichText);
				EmptySentence->setText(QStringLiteral(
					"Nothing needs me. Pull requests from <a href=\"reviews-and-queue\">saved repositories and "
					"observer rules</a> appear here."));
				EmptySentence->setWordWrap(true);
				EmptySentence->setOpenExternalLinks(false);
				EmptySentence->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
				EmptySentence->setFocusPolicy(Qt::StrongFocus);
				EmptySentence->setContentsMargins(sentence_inset, sentence_inset_vertical, sentence_inset, sentence_inset_vertical);
				EmptySentence->setAccessibleName(home_all_empty_sentence());
				EmptySentence->setAccessibleDescription(QStringLiteral("The link opens Settings, Reviews & queue"));
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
			connect(view, &QAbstractItemView::clicked, this, [this, index](const QModelIndex& aIndex) { open_row(index, aIndex); });
			connect(view, &QAbstractItemView::activated, this, [this, index](const QModelIndex& aIndex) { open_row(index, aIndex); });
			connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this,
					[this, index](const QModelIndex& aCurrent) { adopt_current(index, aCurrent); });
			connect_model_signals(*model, *this, [this] { schedule_refresh(); });
			StateModels[index] = model;
			StatePanels[index] = panel;
			StateViews[index]  = view;
			Layout->addWidget(panel);
		}

		RecentPanel = new home_panel{ std::nullopt, home_panel_style::panel, Column };
		RecentPanel->setObjectName(QStringLiteral("homeRecentPanel"));
		RecentPanel->header()->setObjectName(QStringLiteral("homeRecentPanelHeader"));
		RecentPanel->set_title(QStringLiteral("Recently viewed"));
		RecentPanel->set_collapse_observer([this](bool) { update_sticky_headers(); });
		RecentPanel->set_header_navigator([this](const int aKey) { return navigate_from_header(*RecentPanel, aKey); });
		RecentPanel->installEventFilter(this);
		RecentPanel->hide();
		Layout->addWidget(RecentPanel);
		Layout->addStretch(1);
		setWidget(Column);

		connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) { update_sticky_headers(); });
		connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int) { update_sticky_headers(); });
		refresh();
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
		RecentPanel->set_state_colours(aColours);
		for (auto* const view : StateViews) {
			view->viewport()->update();
		}
		if (RecentView != nullptr) {
			RecentView->viewport()->update();
		}
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
		for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
			const auto  group = groups[index];
			auto* const model = StateModels[index];
			auto* const panel = StatePanels[index];
			const auto  rows  = model->rowCount();
			auto        items = std::vector<core::queue_item>{ };
			items.reserve(static_cast<std::size_t>(rows));
			for (auto row = 0; row < rows; ++row) {
				if (auto item = Queue.item_at(model->mapToSource(model->index(row, 0))); item.has_value()) {
					items.push_back(std::move(*item));
				}
			}
			panel->set_count(rows);
			panel->set_summary(home_state_summary(group, items));
			const auto sentence = group == home_group::needs_you && allEmpty;
			StateViews[index]->setVisible(rows > 0);
			if (group == home_group::needs_you) {
				EmptySentence->setVisible(sentence);
			}
			panel->setVisible(QueueReady && QueueAccount && (rows > 0 || sentence));
		}
		RecentPanel->setVisible(RecentView != nullptr && RecentCount > 0);
		update_sticky_headers();
	}

	void home_list::update_sticky_headers()
	{
		for (auto* const panel : StatePanels) {
			stick_home_header(*panel, *viewport(), 0);
		}
		stick_home_header(*RecentPanel, *viewport(), 0);
	}

	void home_list::adopt_current(const std::size_t aGroup, const QModelIndex& aCurrent)
	{
		if (!aCurrent.isValid()) {
			if (CurrentGroup == aGroup) {
				CurrentGroup.reset();
			}
			return;
		}
		CurrentGroup = aGroup;
		// One current row on the page: making a row current in one state clears
		// the others.
		for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
			if (index == aGroup) {
				continue;
			}
			auto* const selection = StateViews[index]->selectionModel();
			if (selection->currentIndex().isValid() || selection->hasSelection()) {
				const QSignalBlocker blocked{ selection };
				selection->clear();
				StateViews[index]->viewport()->update();
			}
		}
		if (!RestoringSelection) {
			ensure_row_visible(*StateViews[aGroup], aCurrent, *StatePanels[aGroup]);
		}
	}

	void home_list::open_row(const std::size_t aGroup, const QModelIndex& aIndex)
	{
		if (!OpenDispatch) {
			return;
		}
		if (const auto item = Queue.item_at(StateModels[aGroup]->mapToSource(aIndex)); item.has_value()) {
			OpenDispatch(*item);
		}
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
		for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
			auto* const model = StateModels[index];
			for (auto row = 0; row < model->rowCount(); ++row) {
				const auto proxyIndex = model->index(row, 0);
				const auto item       = Queue.item_at(model->mapToSource(proxyIndex));
				if (!item.has_value() || item->Ref != aRef) {
					continue;
				}
				auto* const view = StateViews[index];
				if (aFocus) {
					StatePanels[index]->set_collapsed(false);
				}
				RestoringSelection = !aFocus;
				view->setCurrentIndex(proxyIndex);
				adopt_current(index, proxyIndex);
				RestoringSelection = false;
				if (aFocus) {
					view->setFocus(Qt::OtherFocusReason);
					ensure_row_visible(*view, proxyIndex, *StatePanels[index]);
				}
				return true;
			}
		}
		return false;
	}

	void home_list::focus_first_row()
	{
		for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
			if (StatePanels[index]->isHidden()) {
				continue;
			}
			if (!StatePanels[index]->collapsed() && StateModels[index]->rowCount() > 0) {
				static_cast<void>(focus_row(*StateViews[index], 0));
				return;
			}
			StatePanels[index]->header()->setFocus(Qt::OtherFocusReason);
			return;
		}
	}

	auto home_list::current_queue_item() const -> std::optional<core::queue_item>
	{
		if (!CurrentGroup.has_value()) {
			return std::nullopt;
		}
		const auto index = StateViews[*CurrentGroup]->currentIndex();
		if (!index.isValid()) {
			return std::nullopt;
		}
		return Queue.item_at(StateModels[*CurrentGroup]->mapToSource(index));
	}

	auto home_list::current_view() const -> QListView*
	{
		if (!CurrentGroup.has_value() || !StateViews[*CurrentGroup]->currentIndex().isValid()) {
			return nullptr;
		}
		return StateViews[*CurrentGroup];
	}

	auto home_list::state_panel(const home_group aGroup) const noexcept -> home_panel*
	{
		return StatePanels[static_cast<std::size_t>(aGroup)];
	}

	auto home_list::state_view(const home_group aGroup) const noexcept -> QListView*
	{
		return StateViews[static_cast<std::size_t>(aGroup)];
	}

	auto home_list::state_model(const home_group aGroup) const noexcept -> home_state_filter_model*
	{
		return StateModels[static_cast<std::size_t>(aGroup)];
	}

	auto home_list::recent_panel() const noexcept -> home_panel*
	{
		return RecentPanel;
	}

	auto home_list::recent_action() const noexcept -> home_row_action*
	{
		return RecentAction;
	}

	auto home_list::empty_sentence() const noexcept -> QLabel*
	{
		return EmptySentence;
	}

	auto home_list::row_delegate() const noexcept -> home_row_delegate*
	{
		return Delegate;
	}

	auto home_list::queue_shown() const -> bool
	{
		return QueueAccount && isVisible();
	}

	auto home_list::shown_panels() const -> std::vector<home_panel*>
	{
		auto panels = std::vector<home_panel*>{ };
		for (auto* const panel : StatePanels) {
			if (!panel->isHidden()) {
				panels.push_back(panel);
			}
		}
		if (!RecentPanel->isHidden()) {
			panels.push_back(RecentPanel);
		}
		return panels;
	}

	auto home_list::rows_of(const home_panel& aPanel) const -> QAbstractItemView*
	{
		if (&aPanel == RecentPanel) {
			return RecentView;
		}
		for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
			if (StatePanels[index] == &aPanel) {
				return StateViews[index]->isHidden() ? nullptr : StateViews[index];
			}
		}
		return nullptr;
	}

	auto home_list::focus_chain() const -> std::vector<QWidget*>
	{
		auto chain = std::vector<QWidget*>{ };
		for (auto* const panel : shown_panels()) {
			chain.push_back(panel->header());
			if (panel->collapsed()) {
				continue;
			}
			if (auto* const rows = rows_of(*panel); rows != nullptr) {
				chain.push_back(rows);
			}
			if (panel == StatePanels[0U] && !EmptySentence->isHidden()) {
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
		const auto panels = shown_panels();
		const auto found  = std::ranges::find(panels, &aPanel);
		if (found == panels.end()) {
			return false;
		}
		const auto at = static_cast<std::size_t>(found - panels.begin());
		if (aKey == Qt::Key_Down) {
			if (!aPanel.collapsed()) {
				if (auto* const rows = rows_of(aPanel); rows != nullptr && focus_row(*rows, 0)) {
					return true;
				}
				if (&aPanel == StatePanels[0U] && !EmptySentence->isHidden()) {
					EmptySentence->setFocus(Qt::TabFocusReason);
					return true;
				}
			}
			if (at + 1U < panels.size()) {
				panels[at + 1U]->header()->setFocus(Qt::OtherFocusReason);
				return true;
			}
			return false;
		}
		if (aKey == Qt::Key_Up && at > 0U) {
			auto* const previous = panels[at - 1U];
			if (!previous->collapsed()) {
				if (auto* const rows = rows_of(*previous); rows != nullptr && rows->model() != nullptr
					&& focus_row(*rows, rows->model()->rowCount(rows->rootIndex()) - 1)) {
					return true;
				}
			}
			previous->header()->setFocus(Qt::OtherFocusReason);
			return true;
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

	auto home_list::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent == nullptr) {
			return QScrollArea::eventFilter(aWatched, aEvent);
		}
		if (aWatched == EmptySentence && aEvent->type() == QEvent::PaletteChange) {
			derive_link_colour();
		}
		if (aEvent->type() == QEvent::Wheel) {
			// A panel's rows are exactly as tall as they are and never scroll on
			// their own, so a wheel over them is the page's. Qt does not pass a
			// wheel a scroll area could not use on to its parents when the event
			// was synthesized, so the page is handed it directly rather than
			// relying on propagation.
			const auto rows = std::ranges::any_of(StateViews, [aWatched](const home_row_view* const aView) {
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
			for (auto* const panel : StatePanels) {
				if (aWatched == panel) {
					update_sticky_headers();
				}
			}
			if (aWatched == RecentPanel) {
				update_sticky_headers();
			}
		}
		if (aEvent->type() != QEvent::KeyPress) {
			return QScrollArea::eventFilter(aWatched, aEvent);
		}
		const auto* const key       = static_cast<QKeyEvent*>(aEvent);
		const auto        unshifted = key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::KeypadModifier;
		for (auto index = std::size_t{ 0U }; index < state_count; ++index) {
			auto* const view = StateViews[index];
			if (aWatched != view) {
				continue;
			}
			if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
				// [[BPR-0736]]. macOS treats these as editing keys instead of
				// emitting activated. Consume them here on every platform so a
				// key opens once; the pointer keeps its own path.
				if (view->currentIndex().isValid()) {
					open_row(index, view->currentIndex());
				}
				return true;
			}
			const auto current = view->currentIndex();
			if (key->key() == Qt::Key_Up && unshifted && (!current.isValid() || current.row() == 0)) {
				StatePanels[index]->header()->setFocus(Qt::OtherFocusReason);
				return true;
			}
			if (key->key() == Qt::Key_Down && unshifted && current.isValid()
				&& current.row() == StateModels[index]->rowCount() - 1) {
				const auto panels = shown_panels();
				const auto found  = std::ranges::find(panels, StatePanels[index]);
				if (found != panels.end() && found + 1 != panels.end()) {
					(*(found + 1))->header()->setFocus(Qt::OtherFocusReason);
					return true;
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
