#include "bpr/core/review_comment_range.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/menu_tool_button.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QBrush>
#include <QByteArrayView>
#include <QChar>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLatin1Char>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QListSpecialMethods>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QObject>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QSizePolicy>
#include <QSortFilterProxyModel>
#include <QString>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	namespace {
		/// \brief Shows a literal ampersand where Qt would read a mnemonic.
		///
		/// \details [[BPR-0783]]. A menu or a button reads "&" in an action's text
		/// as the mnemonic marker and draws the next character underlined instead
		/// of the ampersand, so "Reviews & queue" arrives as "Reviews _queue".
		/// Doubling it is the escape. It belongs only in text that is drawn:
		/// a tooltip, a status tip and an accessible name are read rather than
		/// parsed, and Qt strips the pair back to one ampersand for assistive
		/// technology, so what is read aloud stays the plain sentence.
		[[nodiscard]] auto menu_literal(const QString& aText) -> QString
		{
			auto escaped = aText;
			escaped.replace(QStringLiteral("&"), QStringLiteral("&&"));
			return escaped;
		}

		// [[BPR-0774]]. The card in its edit state is drawn the way a server
		// card is: the base colour, one Mid hairline and the card radius.
		class card_surface final : public QWidget
		{
		public:
			using QWidget::QWidget;

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				static_cast<void>(aEvent);
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Mid), 1.0 });
				painter.setBrush(palette().color(QPalette::Active, QPalette::Base));
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);
			}
		};
	}

	void workbench_window::build_home_strip_controls()
	{
		// [[BPR-0783]]. RFC-BPR-0032's amendment to RFC-BPR-0028's strip: while
		// Home is shown and an account can fill its list, Home's own control and
		// the refresh glyph stand before the right divider, the account and
		// settings. Signed out they act on nothing, so they are absent rather
		// than disabled, and the review strip is unchanged.
		//
		// [[BPR-1580]]. One View menu in place of Group, State headers and
		// Filter (the owner, 2026-10-17: "I wonder if we should collapse these 3
		// into one"). Filter's four groups were section titles, which the native
		// macOS menu draws as bare separators, so its choices arrived
		// unlabelled. Here every group is a submenu whose own item carries the
		// group's name and current value: an ordinary item's text, which every
		// platform's menu draws. No section title is used anywhere in it.
		HomeViewMenu = new QMenu{ this };
		HomeViewMenu->setObjectName(QStringLiteral("homeViewMenu"));
		HomeViewMenu->setAccessibleName(QStringLiteral("View options for Home's pull requests"));
		HomeViewMenu->setAccessibleDescription(QStringLiteral(
			"Group by, Sort by, State headers, My role, Added by, Quiet items and Reset to defaults; nothing here changes GitHub"));
		HomeViewMenu->setToolTipsVisible(true);

		auto* const group = add_home_view_submenu(
			home_view_choice::group, QStringLiteral("homeGroupMenu"),
			QStringLiteral("How Home gathers its rows into panels. The state order stays inside every grouping."));
		auto* const groupings = new QActionGroup{ group };
		groupings->setExclusive(true);
		for (const auto& [value, title] : std::array{
				 std::pair{ application::home_grouping::none, QStringLiteral("None") },
				 std::pair{ application::home_grouping::organization, QStringLiteral("Organization") },
				 std::pair{ application::home_grouping::repository, QStringLiteral("Repository") },
				 std::pair{ application::home_grouping::account, QStringLiteral("Account") } }) {
			auto* const action = group->addAction(title);
			action->setObjectName(QStringLiteral("homeGroupBy%1Action").arg(title));
			action->setCheckable(true);
			action->setData(static_cast<int>(value));
			groupings->addAction(action);
			const auto purpose = value == application::home_grouping::none
				? QStringLiteral("One list in state order, with no grouping.")
				: QStringLiteral("One panel per %1, with the state order kept inside it.").arg(title.toLower());
			action->setToolTip(purpose);
			action->setStatusTip(purpose);
			HomeGroupActions[static_cast<std::size_t>(value)] = action;
			connect(action, &QAction::triggered, this, [this, value] {
				if (ApplyingHomeView) {
					return;
				}
				HomeView.Grouping = value;
				apply_home_view();
				dispatch_home_view();
			});
		}

		// Sort sits with Group by: both arrange the rows, neither hides any.
		static_cast<void>(add_home_view_submenu(
			home_view_choice::sort, QStringLiteral("homeSortMenu"),
			QStringLiteral("The order of the rows inside each state. The states themselves keep their order.")));

		HomeStateHeaders = HomeViewMenu->addAction(QStringLiteral("State headers"));
		HomeStateHeaders->setObjectName(QStringLiteral("homeStateHeadersAction"));
		HomeStateHeaders->setCheckable(true);
		HomeStateHeaders->setChecked(true);
		HomeStateHeaders->setToolTip(QStringLiteral(
			"Show each state as its own panel. With it off the rows sit in state order with a state mark."));
		HomeStateHeaders->setStatusTip(HomeStateHeaders->toolTip());
		connect(HomeStateHeaders, &QAction::toggled, this, [this](const bool aChecked) {
			if (ApplyingHomeView) {
				return;
			}
			HomeView.StateHeaders = aChecked;
			apply_home_view();
			dispatch_home_view();
		});

		HomeViewMenu->addSeparator();
		static_cast<void>(add_home_view_submenu(
			home_view_choice::role, QStringLiteral("homeRoleMenu"),
			QStringLiteral("Show pull requests by my part in them on GitHub.")));
		static_cast<void>(add_home_view_submenu(
			home_view_choice::added_by, QStringLiteral("homeAddedByMenu"),
			QStringLiteral("Show pull requests by the observer rule that added them. Observer rules are mine, kept in Settings › Home & queue, and never change GitHub.")));
		static_cast<void>(add_home_view_submenu(
			home_view_choice::quiet, QStringLiteral("homeQuietMenu"),
			QStringLiteral("A quiet pull request has nothing new for me and nothing waiting on anyone.")));
		build_home_filter_menus();

		HomeViewMenu->addSeparator();
		HomeViewReset = HomeViewMenu->addAction(QStringLiteral("Reset to defaults"));
		HomeViewReset->setObjectName(QStringLiteral("homeViewResetAction"));
		HomeViewReset->setToolTip(QStringLiteral(
			"Group by None, Sort by Priority, State headers on, any role, added any way and quiet items shown."));
		HomeViewReset->setStatusTip(HomeViewReset->toolTip());
		connect(HomeViewReset, &QAction::triggered, this, [this] { restore_home_view_defaults(true); });

		HomeViewMenu->addSeparator();
		// [[BPR-1580]]. Where observer rules are kept is Added by's own first
		// line now, so the footer says only what holds for the whole menu.
		auto* const footer = HomeViewMenu->addAction(QStringLiteral("Nothing here changes GitHub."));
		footer->setObjectName(QStringLiteral("homeFilterFooter"));
		footer->setEnabled(false);
		// The footer says where they are; this is the way there, by keyboard as
		// well as by pointer.
		// The name carries an ampersand, which a menu would otherwise eat as a
		// mnemonic (`menu_literal`).
		HomeFilterSettings = HomeViewMenu->addAction(menu_literal(QStringLiteral("Open Home & queue")));
		HomeFilterSettings->setObjectName(QStringLiteral("homeFilterSettingsAction"));
		HomeFilterSettings->setToolTip(QStringLiteral(
			"Opens Settings, Home & queue, where observer rules and queue defaults are kept."));
		HomeFilterSettings->setStatusTip(HomeFilterSettings->toolTip());
		connect(HomeFilterSettings, &QAction::triggered, this, [this] {
			open_settings(settings_page::home_and_queue);
		});

		HomeViewButton = new menu_tool_button{ this };
		HomeViewButton->setObjectName(QStringLiteral("homeViewButton"));
		HomeViewButton->setText(QStringLiteral("View"));
		HomeViewButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		HomeViewButton->setPopupMode(QToolButton::InstantPopup);
		HomeViewButton->setMenu(HomeViewMenu);
		HomeViewButton->setAccessibleName(QStringLiteral("View"));
		Strip->adopt_button(HomeViewButton);
		HomeViewItem = PrimaryToolbar->insertWidget(GithubRefreshAction, HomeViewButton);
		HomeViewItem->setObjectName(QStringLiteral("homeViewAction"));
		apply_home_view();
	}

	auto workbench_window::add_home_view_submenu(const home_view_choice aChoice, const QString& aName, const QString& aPurpose) -> QMenu*
	{
		auto* const menu = new QMenu{ HomeViewMenu };
		menu->setObjectName(aName);
		menu->setToolTipsVisible(true);
		menu->setAccessibleDescription(aPurpose);
		menu->menuAction()->setObjectName(aName + QStringLiteral("Action"));
		menu->menuAction()->setToolTip(aPurpose);
		menu->menuAction()->setStatusTip(aPurpose);
		HomeViewMenu->addMenu(menu);
		HomeViewSubmenus[static_cast<std::size_t>(aChoice)] = menu;
		return menu;
	}

	void workbench_window::build_home_filter_menus()
	{
		// [[BPR-0783]] Decisions (c). Exactly the four typed preferences that
		// survive as Home filters, each listing every value of its vocabulary.
		// Repository and Bucket are not here: Group by and the state order do
		// their job.
		//
		// [[BPR-0783]] review finding. Each is the one exclusive choice the
		// board draws, not independent check boxes: an item cannot be unticked
		// into nothing, and choosing one unticks its siblings.
		//
		// [[BPR-1580]]. A native macOS menu draws no tooltip, so a choice whose
		// meaning is not obvious says it in its own text; the submenu's own item
		// shows the short name, and the tooltip and status tip say it again
		// where the platform draws them.
		struct choice
		{
			QString Text;
			QString Short;
			QString Purpose;
		};
		auto sections = std::array<QActionGroup*, 4U>{ };
		for (auto& section : sections) {
			section = new QActionGroup{ this };
			section->setExclusive(true);
		}
		const auto item = [this, &sections](const std::size_t aSection,
											QMenu&            aMenu,
											const choice&     aChoice,
											const QString&    aName,
											const int         aValue,
											auto              aGesture) {
			auto* const action = aMenu.addAction(aChoice.Text);
			action->setObjectName(QStringLiteral("homeFilter%1Action").arg(aName));
			action->setCheckable(true);
			action->setData(aValue);
			action->setToolTip(aChoice.Purpose);
			action->setStatusTip(aChoice.Purpose);
			sections[aSection]->addAction(action);
			HomeFilterSections[aSection].push_back(action);
			HomeFilterShortNames[action] = aChoice.Short;
			connect(action, &QAction::triggered, this, [this, aGesture] {
				if (ApplyingHomeView) {
					return;
				}
				aGesture();
			});
		};
		const auto named = [](const QString& aShort) { return QString{ aShort }.remove(QLatin1Char{ ' ' }); };

		auto& role = *HomeViewSubmenus[static_cast<std::size_t>(home_view_choice::role)];
		for (const auto& [value, words] : std::array{
				 std::pair{ std::optional<core::github_role>{ },
							choice{ QStringLiteral("Any role"), QStringLiteral("Any"), QStringLiteral("Every pull request, whatever my part in it.") } },
				 std::pair{ std::optional{ core::github_role::author },
							choice{ QStringLiteral("I opened it"), QStringLiteral("Author"), QStringLiteral("Only pull requests I opened.") } },
				 std::pair{ std::optional{ core::github_role::reviewer },
							choice{ QStringLiteral("I reviewed it"), QStringLiteral("Reviewer"), QStringLiteral("Only pull requests I have already reviewed.") } },
				 std::pair{ std::optional{ core::github_role::requested_reviewer },
							choice{ QStringLiteral("My review is requested"), QStringLiteral("Requested"),
									QStringLiteral("Only pull requests that ask for my review and do not have it yet.") } },
				 std::pair{ std::optional{ core::github_role::participant },
							choice{ QStringLiteral("I took part in it"), QStringLiteral("Participant"),
									QStringLiteral("Only pull requests I took part in on GitHub without being their author or a reviewer.") } },
				 std::pair{ std::optional{ core::github_role::none },
							choice{ QStringLiteral("None of these (a rule or a follow added it)"), QStringLiteral("No role"),
									QStringLiteral("Only pull requests I have no part in on GitHub, which an observer rule or a follow brought to Home.") } } }) {
			item(0U, role, words, named(words.Short), value.has_value() ? static_cast<int>(*value) : -1, [this, value] {
				auto filter = home_filter();
				filter.Role = value;
				choose_home_filter(application::queue_preference_field::role, filter);
			});
		}

		auto& addedBy = *HomeViewSubmenus[static_cast<std::size_t>(home_view_choice::added_by)];
		// What an observer rule is, and where it is kept, as the first line.
		auto* const rules = addedBy.addAction(menu_literal(QStringLiteral("Observer rules are mine, kept in Settings › Home & queue")));
		rules->setObjectName(QStringLiteral("homeAddedByMeaning"));
		rules->setEnabled(false);
		for (const auto& [value, words] : std::array{
				 std::pair{ std::optional<core::observer_rule_kind>{ },
							choice{ QStringLiteral("Any way"), QStringLiteral("Any"), QStringLiteral("Every pull request, whether or not an observer rule added it.") } },
				 std::pair{ std::optional{ core::observer_rule_kind::repository },
							choice{ QStringLiteral("A rule on a repository"), QStringLiteral("Repository rule"),
									QStringLiteral("Only pull requests an observer rule watching a whole repository added.") } },
				 std::pair{ std::optional{ core::observer_rule_kind::author },
							choice{ QStringLiteral("A rule on an author"), QStringLiteral("Author rule"),
									QStringLiteral("Only pull requests an observer rule watching one author's pull requests added.") } },
				 std::pair{ std::optional{ core::observer_rule_kind::path },
							choice{ QStringLiteral("A rule on a path"), QStringLiteral("Path rule"),
									QStringLiteral("Only pull requests an observer rule watching files under a path added.") } } }) {
			// Any is a word My role uses too, so this one is named for its group.
			const auto name = value.has_value() ? named(words.Short) : QStringLiteral("AddedByAny");
			item(1U, addedBy, words, name, value.has_value() ? static_cast<int>(*value) : -1, [this, value] {
				auto filter    = home_filter();
				filter.AddedBy = value;
				choose_home_filter(application::queue_preference_field::observer, filter);
			});
		}

		auto& quiet = *HomeViewSubmenus[static_cast<std::size_t>(home_view_choice::quiet)];
		// What quiet means, as the submenu's first line: the one word here a
		// reviewer cannot be expected to know.
		auto* const meaning = quiet.addAction(QStringLiteral("Quiet: nothing new for me and nothing waiting on anyone"));
		meaning->setObjectName(QStringLiteral("homeQuietMeaning"));
		meaning->setEnabled(false);
		for (const auto& [value, words] : std::array{
				 std::pair{ application::queue_quiet_filter::all,
							choice{ QStringLiteral("Show"), QStringLiteral("Show"), QStringLiteral("Quiet pull requests are listed with the rest.") } },
				 std::pair{ application::queue_quiet_filter::attention,
							choice{ QStringLiteral("Hide"), QStringLiteral("Hide"),
									QStringLiteral("Quiet pull requests are hidden. One a failed check or merge block holds up still shows.") } },
				 std::pair{ application::queue_quiet_filter::quiet,
							choice{ QStringLiteral("Only quiet"), QStringLiteral("Only quiet"), QStringLiteral("Only quiet pull requests are listed.") } } }) {
			item(2U, quiet, words, named(words.Short), static_cast<int>(value), [this, value] {
				auto filter  = home_filter();
				filter.Quiet = value;
				choose_home_filter(application::queue_preference_field::quiet, filter);
			});
		}

		auto& sort = *HomeViewSubmenus[static_cast<std::size_t>(home_view_choice::sort)];
		for (const auto& [value, words] : std::array{
				 std::pair{ application::queue_sort_order::core_priority,
							choice{ QStringLiteral("Priority"), QStringLiteral("Priority"), QStringLiteral("Home's own order: what most needs me comes first.") } },
				 std::pair{ application::queue_sort_order::repository,
							choice{ QStringLiteral("Repository"), QStringLiteral("Repository"), QStringLiteral("By repository name, then pull request number.") } },
				 std::pair{ application::queue_sort_order::pull_request_number,
							choice{ QStringLiteral("Pull request number"), QStringLiteral("Pull request number"),
									QStringLiteral("By pull request number, then repository.") } } }) {
			item(3U, sort, words, named(words.Short), static_cast<int>(value), [this, value] {
				auto filter = home_filter();
				filter.Sort = value;
				choose_home_filter(application::queue_preference_field::sort, filter);
			});
		}
	}

	auto workbench_window::home_filter() const -> application::home_filter
	{
		return HomeView.FilterOwned
			? HomeView.Filter
			: application::home_filter_of(QueueProjection.Preferences);
	}

	void workbench_window::choose_home_filter(
		const application::queue_preference_field aField, const application::home_filter& aFilter)
	{
		if (!QueueReady || !QueuePreferencesDispatch) {
			// Nothing to filter, and nowhere for the choice to be kept. The menu
			// goes back to showing what is in force.
			apply_home_view();
			return;
		}
		// [[BPR-0783]] Decisions (e). One gesture, two destinations. The profile's
		// store hears the same typed intent the six combos sent, so the last
		// choice is what a new window starts with; and from this gesture on the
		// window keeps its own four values, so two windows of one profile filter
		// independently.
		auto next           = QueueProjection.Preferences;
		next.Role           = aFilter.Role;
		next.ObserverReason = aFilter.AddedBy;
		next.Quiet          = aFilter.Quiet;
		next.Sort           = aFilter.Sort;
		QueuePreferencesDispatch(application::queue_preferences_intent{
			.Preferences = std::move(next),
			.Field       = aField,
		});
		HomeView.FilterOwned = true;
		HomeView.Filter      = aFilter;
		apply_home_view();
		dispatch_home_view();
	}

	void workbench_window::restore_home_view_defaults(const bool aWholeView)
	{
		if (ApplyingHomeView) {
			return;
		}
		const auto before  = HomeView;
		const auto current = home_filter();
		auto       target  = current;
		target.Role.reset();
		target.AddedBy.reset();
		target.Quiet = application::queue_quiet_filter::all;
		if (aWholeView) {
			target.Sort           = application::queue_sort_order::core_priority;
			HomeView.Grouping     = application::home_grouping::none;
			HomeView.StateHeaders = true;
		}
		if (target != current && QueueReady && QueuePreferencesDispatch) {
			// The profile hears this as it hears a filter choice: one typed
			// intent per field that changes, each naming only its own field, so
			// the store merges them exactly as it merges single choices.
			auto next           = QueueProjection.Preferences;
			next.Role           = target.Role;
			next.ObserverReason = target.AddedBy;
			next.Quiet          = target.Quiet;
			next.Sort           = target.Sort;
			const auto send     = [this, &next](const application::queue_preference_field aField) {
				QueuePreferencesDispatch(application::queue_preferences_intent{ .Preferences = next, .Field = aField });
			};
			if (current.Role != target.Role) {
				send(application::queue_preference_field::role);
			}
			if (current.AddedBy != target.AddedBy) {
				send(application::queue_preference_field::observer);
			}
			if (current.Quiet != target.Quiet) {
				send(application::queue_preference_field::quiet);
			}
			if (current.Sort != target.Sort) {
				send(application::queue_preference_field::sort);
			}
			HomeView.FilterOwned = true;
			HomeView.Filter      = target;
		}
		if (HomeView == before) {
			return;
		}
		apply_home_view();
		dispatch_home_view();
	}

	void workbench_window::apply_home_view()
	{
		ApplyingHomeView = true;
		for (auto index = std::size_t{ 0U }; index < HomeGroupActions.size(); ++index) {
			if (HomeGroupActions[index] != nullptr) {
				HomeGroupActions[index]->setChecked(index == static_cast<std::size_t>(HomeView.Grouping));
			}
		}
		if (HomeStateHeaders != nullptr) {
			HomeStateHeaders->setChecked(HomeView.StateHeaders);
		}
		apply_home_filter_controls();
		if (HomeList != nullptr) {
			HomeList->set_grouping(HomeView.Grouping);
			HomeList->set_state_headers(HomeView.StateHeaders);
			HomeList->set_collapsed_panels(HomeView.Collapsed);
		}
		if (QueueFilterModel != nullptr) {
			QueueFilterModel->set_home_view(HomeView);
			if (HomeList != nullptr) {
				HomeList->refresh();
			}
		}
		ApplyingHomeView = false;
		if (QueueReady && QueueFilterModel != nullptr) {
			// The queue's own sentence explains the rows Home is hiding, which
			// the view may just have changed.
			update_queue_status(QueueProjection);
		}
		apply_focus_order();
	}

	void workbench_window::apply_home_filter_controls()
	{
		// Adopting what is in force is not a gesture, so nothing here reports back
		// out; a menu's own check change must not read as the reviewer's choice.
		const auto applying = ApplyingHomeView;
		ApplyingHomeView    = true;
		const auto filter   = home_filter();
		const auto shown    = [this](const std::size_t aSection, const int aValue) {
			auto chosen = QString{ };
			for (auto* const action : HomeFilterSections[aSection]) {
				action->setChecked(action->data().toInt() == aValue);
				if (action->isChecked()) {
					chosen = HomeFilterShortNames[action];
				}
			}
			return chosen;
		};
		const auto        role    = shown(0U, filter.Role.has_value() ? static_cast<int>(*filter.Role) : -1);
		const auto        addedBy = shown(1U, filter.AddedBy.has_value() ? static_cast<int>(*filter.AddedBy) : -1);
		const auto        quiet   = shown(2U, static_cast<int>(filter.Quiet));
		const auto        sort    = shown(3U, static_cast<int>(filter.Sort));
		const auto* const grouped = HomeGroupActions[static_cast<std::size_t>(HomeView.Grouping)];
		const auto        group   = grouped != nullptr ? grouped->text() : QStringLiteral("None");

		// [[BPR-1580]]. Each submenu's own item names its group and what is in
		// force, so the menu reads as a summary of the view before it is opened.
		const auto title = [this](const home_view_choice aChoice, const QString& aGroup, const QString& aValue) {
			if (auto* const menu = HomeViewSubmenus[static_cast<std::size_t>(aChoice)]; menu != nullptr) {
				menu->setTitle(QStringLiteral("%1: %2").arg(aGroup, aValue));
				menu->setAccessibleName(aGroup);
			}
		};
		title(home_view_choice::group, QStringLiteral("Group by"), group);
		title(home_view_choice::sort, QStringLiteral("Sort by"), sort);
		title(home_view_choice::role, QStringLiteral("My role"), role);
		title(home_view_choice::added_by, QStringLiteral("Added by"), addedBy);
		title(home_view_choice::quiet, QStringLiteral("Quiet items"), quiet);
		if (auto* const menu = HomeViewSubmenus[static_cast<std::size_t>(home_view_choice::added_by)]; menu != nullptr) {
			// With no observer rule, nothing was added by one, so the choice is
			// offered only once there is a rule, or while this window is
			// already filtered by one, which must stay within reach to undo.
			menu->menuAction()->setVisible(!QueueRules.empty() || filter.AddedBy.has_value());
		}

		// What differs from the default, said in the button's tooltip.
		auto changed = QStringList{ };
		if (HomeView.Grouping != application::home_grouping::none) {
			changed.push_back(QStringLiteral("Group by %1").arg(group));
		}
		if (filter.Sort != application::queue_sort_order::core_priority) {
			changed.push_back(QStringLiteral("Sort by %1").arg(sort));
		}
		if (!HomeView.StateHeaders) {
			changed.push_back(QStringLiteral("State headers off"));
		}
		if (filter.Role.has_value()) {
			changed.push_back(QStringLiteral("My role %1").arg(role));
		}
		if (filter.AddedBy.has_value()) {
			changed.push_back(QStringLiteral("Added by %1").arg(addedBy));
		}
		if (filter.Quiet != application::queue_quiet_filter::all) {
			changed.push_back(QStringLiteral("Quiet items %1").arg(quiet));
		}
		if (HomeViewReset != nullptr) {
			HomeViewReset->setEnabled(!changed.isEmpty());
		}
		if (HomeViewButton != nullptr) {
			// [[BPR-1580]] owner's note on v001: "I'm not sure why View carries a
			// count". The button stays "View" whatever is chosen; what differs is
			// said in its tooltip and description, the menu's own items name
			// every value, and a view that hides every row says so in the list.
			const auto purpose = changed.isEmpty()
				? QStringLiteral("Arrange and filter Home's pull requests. The default view: nothing is grouped or hidden.")
				: QStringLiteral("Arrange and filter Home's pull requests. %1 changed: %2.")
					  .arg(QString::number(changed.size()), changed.join(QStringLiteral("; ")));
			HomeViewButton->setAccessibleDescription(purpose);
			HomeViewButton->setToolTip(purpose);
		}
		ApplyingHomeView = applying;
	}

	void workbench_window::dispatch_home_view()
	{
		if (ApplyingHomeView || !HomeViewDispatch || !application::valid(HomeView)) {
			return;
		}
		HomeViewDispatch(HomeView);
	}

	void workbench_window::adopt_home_view(const application::home_view_state& aView)
	{
		if (!application::valid(aView) || aView == HomeView) {
			// Equal to what Home already shows, so nothing is re-laid out and
			// focus, selection and scroll stay where the reviewer left them. A
			// view the host refused arrives the same way and puts Home back.
			return;
		}
		HomeView = aView;
		apply_home_view();
	}

	void workbench_window::set_home_view_dispatch(std::function<void(application::home_view_state)> aDispatch)
	{
		HomeViewDispatch = std::move(aDispatch);
	}

	auto workbench_window::home_view() const noexcept -> const application::home_view_state&
	{
		return HomeView;
	}

	auto workbench_window::home_view_button() const noexcept -> QToolButton*
	{
		return HomeViewButton;
	}

	auto workbench_window::home_view_menu() const noexcept -> QMenu*
	{
		return HomeViewMenu;
	}

	auto workbench_window::home_view_submenu(const home_view_choice aChoice) const noexcept -> QMenu*
	{
		return HomeViewSubmenus[static_cast<std::size_t>(aChoice)];
	}

	auto workbench_window::home_view_reset_action() const noexcept -> QAction*
	{
		return HomeViewReset;
	}

	auto workbench_window::home_group_action(const application::home_grouping aGrouping) const noexcept -> QAction*
	{
		return HomeGroupActions[static_cast<std::size_t>(aGrouping)];
	}

	auto workbench_window::home_state_headers_action() const noexcept -> QAction*
	{
		return HomeStateHeaders;
	}

	auto workbench_window::home_filter_settings_action() const noexcept -> QAction*
	{
		return HomeFilterSettings;
	}

	void workbench_window::rebuild_queue_repositories(
		const std::span<const core::queue_item> aItems,
		const std::string&                      aSelectedRepository)
	{
		auto repositories = std::vector<std::string>{ };
		repositories.reserve(aItems.size());
		for (const auto& item : aItems) {
			repositories.push_back(item.Ref.Repository);
		}
		std::ranges::sort(repositories);
		const auto unique = std::ranges::unique(repositories);
		repositories.erase(unique.begin(), unique.end());
		if (!aSelectedRepository.empty()
			&& !std::ranges::contains(repositories, aSelectedRepository)) {
			repositories.push_back(aSelectedRepository);
			std::ranges::sort(repositories);
		}
		ApplyingQueuePreferences = true;
		QueueRepositoryFilter->clear();
		QueueRepositoryFilter->addItem(tr("Repository: any"), QString{ });
		for (const auto& repository : repositories) {
			const auto text = QString::fromStdString(repository);
			QueueRepositoryFilter->addItem(
				tr("Repository: %1").arg(text), text);
		}
		ApplyingQueuePreferences = false;
	}

	void workbench_window::update_queue_status(
		const application::queue_landing_projection& aProjection)
	{
		// [[BPR-0776]]. On Home the status sentence speaks only when it says
		// something the panels do not: restoring, unavailable, a limit. A filter
		// that hides every row is the list's own sentence ([[BPR-1580]]). The count is the panels' headers and an
		// empty queue is Needs me's sentence. Settings keeps the whole text.
		auto notable = true;
		switch (aProjection.Status) {
		case application::queue_landing_status::restoring:
			QueueStatus->setText(tr("Restoring local observer rules and queue preferences…"));
			break;
		case application::queue_landing_status::unavailable:
			QueueStatus->setText(tr("Local observer rules and queue preferences are unavailable."));
			break;
		case application::queue_landing_status::ready:
			if (QueueLanding->rowCount() == 0) {
				QueueStatus->setText(tr(
					"Review queue is empty. Local rules never change GitHub assignments."));
				notable = false;
			} else if (QueueFilterModel->rowCount() == 0
					   && QueueFilterModel->has_active_filters()) {
				QueueStatus->setText(tr(
										 "No reviews match the active queue filters. %1 review(s) remain in the queue.")
										 .arg(QueueLanding->rowCount()));
				// [[BPR-1580]]. Home's list says this itself, with the way out.
				notable = false;
			} else {
				QueueStatus->setText(
					tr("%1 of %2 review(s) shown. %3")
						.arg(QueueFilterModel->rowCount())
						.arg(QueueLanding->rowCount())
						.arg(aProjection.Online
								 ? tr("Refresh is online.")
								 : tr("Refresh is offline.")));
				notable = false;
			}
			break;
		}
		if (aProjection.Failure == application::queue_store_failure::collection_limit) {
			QueueStatus->setText(aProjection.Status == application::queue_landing_status::ready
									 ? tr("The local queue configuration limit was reached. Existing settings are unchanged; remove an entry before adding another.")
									 : tr("Stored queue configuration exceeds this build's supported limits. It was not loaded or changed."));
			notable = true;
		}
		if (SettingsQueueStatus != nullptr) {
			SettingsQueueStatus->setText(QueueStatus->text());
		}
		QueueStatusNotable = notable;
		update_home_surface();
		// [[BPR-1357]]. A queue row that appeared or went may carry the opening.
		if (HomeOpen.has_value()) {
			refresh_home_open();
		}
	}

	void workbench_window::dispatch_queue_rule(
		const application::queue_rule_action aAction)
	{
		if (!QueueRuleDispatch || !QueueReady) {
			return;
		}
		auto rule = core::observer_rule{ };
		if (aAction == application::queue_rule_action::add) {
			rule = {
				.Kind = static_cast<core::observer_rule_kind>(
					QueueRuleKind->currentData().toInt()),
				.Pattern   = QueueRulePattern->text().trimmed().toStdString(),
				.Excluding = QueueRuleExcluding->isChecked(),
			};
		} else {
			const auto index = QueueRuleSelector->currentIndex();
			if (index < 0 || static_cast<std::size_t>(index) >= QueueRules.size()) {
				return;
			}
			rule = QueueRules[static_cast<std::size_t>(index)];
		}
		if (!core::valid(rule)) {
			return;
		}
		++Statistics.QueueRuleIntents;
		QueueRuleDispatch(application::queue_rule_intent{
			.Action = aAction,
			.Rule   = std::move(rule),
		});
	}

	void workbench_window::update_queue_rule_actions()
	{
		if (AddQueueRule == nullptr || RemoveQueueRule == nullptr) {
			return;
		}
		const auto pattern = QueueRulePattern != nullptr
			? QueueRulePattern->text().trimmed().toStdString()
			: std::string{ };
		AddQueueRule->setEnabled(
			QueueReady && static_cast<bool>(QueueRuleDispatch)
			&& core::valid(core::observer_rule{ .Pattern = pattern }));
		RemoveQueueRule->setEnabled(
			QueueReady && static_cast<bool>(QueueRuleDispatch)
			&& QueueRuleSelector != nullptr
			&& QueueRuleSelector->currentIndex() >= 0);
		AddQueueRule->setToolTip(QStringLiteral(
			"Persist a local observer rule; this never changes GitHub."));
		RemoveQueueRule->setToolTip(QStringLiteral(
			"Delete the selected local observer rule."));
	}

	void workbench_window::publish(
		const application::queue_landing_projection& aProjection)
	{
		auto selectedReview = std::optional<core::pull_request_ref>{ };
		if (const auto item = selected_queue_item(); item.has_value()) {
			selectedReview = item->Ref;
		}
		auto       selectedRule  = std::optional<core::observer_rule>{ };
		const auto selectedIndex = QueueRuleSelector->currentIndex();
		if (selectedIndex >= 0
			&& static_cast<std::size_t>(selectedIndex) < QueueRules.size()) {
			selectedRule = QueueRules[static_cast<std::size_t>(selectedIndex)];
		}
		const auto rulesChanged   = QueueRules != aProjection.Rules;
		const auto serversChanged = QueueProjection.SavedServers != aProjection.SavedServers;
		const auto discovered     = QueueProjection.Discovery;

		QueueReady                = aProjection.Status == application::queue_landing_status::ready;
		const auto filtersEnabled = QueueReady
			&& static_cast<bool>(QueuePreferencesDispatch);
		for (auto* const selector : { QueueRoleFilter,
									  QueueObserverFilter,
									  QueueBucketFilter,
									  QueueRepositoryFilter,
									  QueueQuietFilter,
									  QueueSort }) {
			selector->setEnabled(filtersEnabled);
		}
		// The models retain item identity. Keep only the queue's metadata here.
		QueueProjection = application::queue_landing_projection{
			.Status = aProjection.Status, .Rules = aProjection.Rules, .Preferences = aProjection.Preferences, .Session = aProjection.Session, .Online = aProjection.Online, .BackoffSeconds = aProjection.BackoffSeconds, .SavedRepositories = aProjection.SavedRepositories, .SavedServers = aProjection.SavedServers, .Failure = aProjection.Failure, .Discovery = aProjection.Discovery, .Addition = aProjection.Addition
		};
		QueueRules = aProjection.Rules;
		// [[BPR-1580]]. Whether View offers Added by follows the rules.
		apply_home_filter_controls();
		// [[BPR-1407]]. Ages are measured to the host's clock as the queue arrives.
		if (HomeClock) {
			QueueLanding->set_now(HomeClock());
		}
		if (aProjection.UpdatedItem) {
			QueueLanding->apply_item(*aProjection.UpdatedItem);
		} else {
			QueueLanding->apply(aProjection.Items);
		}
		if (RepositoriesTree != nullptr) {
			if (aProjection.UpdatedItem) {
				RepositoriesTree->apply_item(*aProjection.UpdatedItem);
			} else {
				RepositoriesTree->apply(aProjection.Items, aProjection.SavedRepositories);
			}
			update_rail_summaries();
			refresh_filter_notices();
		}
		// [[BPR-0660]], [[BPR-0774]]. A remembered server is its host's card,
		// remembering exactly the record the queue keeps.
		if (serversChanged) {
			update_server_cards();
		}
		update_saved_repository_list();
		if (!aProjection.UpdatedItem) {
			rebuild_queue_repositories(aProjection.Items, aProjection.Preferences.Repository);
		}
		apply_queue_preferences(aProjection.Preferences);
		if (HomeList != nullptr) {
			// The panels are recounted now rather than after the model's own
			// signals settle, so the row the reviewer had is found again in
			// whichever state holds it. Restoring it neither expands a panel
			// the reviewer collapsed nor scrolls the page.
			HomeList->set_queue_status(QueueReady, HomeAccount);
			HomeList->refresh();
			if (selectedReview.has_value()) {
				static_cast<void>(HomeList->select(*selectedReview, false));
			}
		}
		if (rulesChanged) {
			QueueRuleSelector->clear();
			auto restoredIndex = -1;
			for (std::size_t index = 0U; index < QueueRules.size(); ++index) {
				const auto& rule = QueueRules[index];
				QueueRuleSelector->addItem(
					QStringLiteral("%1 %2%3")
						.arg(QString::fromUtf8(core::name(rule.Kind).data()))
						.arg(rule.Excluding ? QStringLiteral("not ") : QString{ })
						.arg(QString::fromStdString(rule.Pattern)));
				if (selectedRule == std::optional{ rule }) {
					restoredIndex = static_cast<int>(index);
				}
			}
			if (restoredIndex >= 0) {
				QueueRuleSelector->setCurrentIndex(restoredIndex);
			}
		}
		update_queue_status(aProjection);
		++Statistics.QueueUpdates;
		update_queue_rule_actions();
		update_saved_repository_actions();
		if (aProjection.Addition) {
			present_repository_addition(*aProjection.Addition);
		}
		update_enterprise_server_actions();
		update_discovery_controls();
		update_pull_request_queue_actions();
		update_pending_panes();
		observe_home_refresh(discovered, aProjection);
		apply_focus_order();
	}

	void workbench_window::build_repositories_page()
	{
		// RFC-BPR-0062, owner verdicts of 2026-10-07 and 2026-10-08: the saved
		// repositories have a page of their own, laid out the way Accounts &
		// hosts is. The account's repositories are one card; a new repository is
		// a card in its edit state above the bordered Add repository button that
		// opens it; there is no dialog. The page keeps no record of its own: the
		// rows are the queue host's accepted records, Remove sends the typed
		// intent for the exact record a row stands for, and Add sends the same
		// repository-addition gesture the dialog did.
		SavedRepositoriesEditor = new QWidget;
		SavedRepositoriesEditor->setObjectName(QStringLiteral("savedRepositoriesEditor"));
		SavedRepositoriesEditor->setAccessibleName(QStringLiteral("Repositories"));
		SavedRepositoriesEditor->setAccessibleDescription(QStringLiteral(
			"The repositories saved for the displayed account, where my pull requests and review requests are found"));
		auto* const column = new QVBoxLayout{ SavedRepositoriesEditor };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(16);
		auto* const lede = new QLabel{
			tr("My pull requests and review requests are found in these repositories. Nothing here changes GitHub."),
			SavedRepositoriesEditor
		};
		lede->setObjectName(QStringLiteral("repositoriesLede"));
		lede->setWordWrap(true);
		lede->setTextFormat(Qt::PlainText);
		lede->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(lede);

		// The account's card: whose repositories they are, then the list.
		auto* const card = new card_surface{ SavedRepositoriesEditor };
		card->setObjectName(QStringLiteral("savedRepositoriesCard"));
		card->setAccessibleName(QStringLiteral("Saved repositories"));
		auto* const cardColumn = new QVBoxLayout{ card };
		cardColumn->setContentsMargins(16, 12, 16, 12);
		cardColumn->setSpacing(8);
		SavedRepositoriesDestination = new QLabel{ card };
		SavedRepositoriesDestination->setObjectName(QStringLiteral("repositoriesDestination"));
		SavedRepositoriesDestination->setAccessibleName(tr("Account and host"));
		SavedRepositoriesDestination->setTextFormat(Qt::PlainText);
		SavedRepositoriesDestination->setWordWrap(true);
		SavedRepositoriesDestination->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		auto destinationFont = SavedRepositoriesDestination->font();
		destinationFont.setWeight(QFont::DemiBold);
		SavedRepositoriesDestination->setFont(destinationFont);
		cardColumn->addWidget(SavedRepositoriesDestination);
		SavedRepositoryList = new QListWidget{ card };
		SavedRepositoryList->setObjectName(QStringLiteral("savedRepositoryList"));
		SavedRepositoryList->setAccessibleName(QStringLiteral("Saved repositories"));
		SavedRepositoryList->setAccessibleDescription(QStringLiteral(
			"Select a repository; Delete or Remove stops finding my pull requests in it"));
		SavedRepositoryList->setSelectionMode(QAbstractItemView::SingleSelection);
		SavedRepositoryList->setFrameShape(QFrame::NoFrame);
		SavedRepositoryList->setWordWrap(true);
		SavedRepositoryList->setTextElideMode(Qt::ElideNone);
		SavedRepositoryList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		SavedRepositoryList->setUniformItemSizes(false);
		SavedRepositoryList->setSpacing(2);
		// The card is as tall as its rows, up to ten, and scrolls beyond: a
		// card never draws an empty box under a short list.
		SavedRepositoryList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		SavedRepositoryList->installEventFilter(this);
		cardColumn->addWidget(SavedRepositoryList);
		// Empty is a sentence in the card, never an empty box.
		SavedRepositoriesEmpty = new QLabel{ tr("No repositories yet."), card };
		SavedRepositoriesEmpty->setObjectName(QStringLiteral("savedRepositoriesEmpty"));
		static_cast<QLabel*>(SavedRepositoriesEmpty)->setTextFormat(Qt::PlainText);
		SavedRepositoriesEmpty->setForegroundRole(QPalette::PlaceholderText);
		SavedRepositoriesEmpty->hide();
		cardColumn->addWidget(SavedRepositoriesEmpty);
		RemoveSavedRepository = new QAction{ tr("Remove"), this };
		RemoveSavedRepository->setObjectName(QStringLiteral("removeSavedRepository"));
		RemoveSavedRepositoryButton = new QPushButton{ tr("Remove"), card };
		RemoveSavedRepositoryButton->setObjectName(QStringLiteral("removeSavedRepositoryButton"));
		RemoveSavedRepositoryButton->setAccessibleName(QStringLiteral("Remove saved repository"));
		RemoveSavedRepositoryButton->setAccessibleDescription(QStringLiteral(
			"Stop finding my pull requests in the selected repository; nothing changes on GitHub"));
		RemoveSavedRepositoryButton->setAutoDefault(false);
		connect(RemoveSavedRepositoryButton, &QPushButton::clicked, RemoveSavedRepository, &QAction::trigger);
		auto* const cardActions = new QHBoxLayout{ };
		cardActions->setContentsMargins(0, 0, 0, 0);
		cardActions->addStretch(1);
		cardActions->addWidget(RemoveSavedRepositoryButton);
		cardColumn->addLayout(cardActions);
		column->addWidget(card);

		// The new repository, as Accounts & hosts draws a new server: a card in
		// its edit state with a labelled field, what was refused beside it, and
		// Add and Cancel at its trailing edge.
		auto* const draft = new card_surface{ SavedRepositoriesEditor };
		RepositoryDraft   = draft;
		draft->setObjectName(QStringLiteral("repositoryAdditionDraft"));
		draft->setAccessibleName(tr("New repository"));
		draft->setAccessibleDescription(tr("Find my pull requests and review requests in a repository saved for the displayed account"));
		auto* const grid = new QGridLayout{ draft };
		grid->setContentsMargins(16, 12, 16, 12);
		grid->setHorizontalSpacing(16);
		grid->setVerticalSpacing(8);
		grid->setColumnStretch(1, 1);
		auto* const draftTitle = new QLabel{ tr("New repository"), draft };
		draftTitle->setObjectName(QStringLiteral("repositoryAdditionTitle"));
		auto draftTitleFont = draftTitle->font();
		draftTitleFont.setWeight(QFont::DemiBold);
		draftTitle->setFont(draftTitleFont);
		RepositoryDestination = new QLabel{ draft };
		RepositoryDestination->setObjectName(QStringLiteral("repositoryAdditionDestination"));
		RepositoryDestination->setTextFormat(Qt::PlainText);
		RepositoryDestination->setWordWrap(true);
		RepositoryDestination->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		RepositoryDestination->setForegroundRole(QPalette::PlaceholderText);
		RepositoryDestination->setAccessibleName(tr("Destination account and host"));
		auto* const nameLabel = new QLabel{ tr("&Repository"), draft };
		SavedRepositoryName   = new QLineEdit{ draft };
		SavedRepositoryName->setObjectName(QStringLiteral("savedRepositoryName"));
		SavedRepositoryName->setAccessibleName(tr("Repository link or owner/name"));
		SavedRepositoryName->setAccessibleDescription(tr("Enter owner/name for the displayed host, or a complete HTTPS repository link"));
		SavedRepositoryName->setPlaceholderText(tr("owner/name or repository link"));
		SavedRepositoryName->setMaxLength(4096);
		SavedRepositoryName->installEventFilter(this);
		nameLabel->setBuddy(SavedRepositoryName);
		RepositoryAdditionStatus = new QLabel{ draft };
		RepositoryAdditionStatus->setObjectName(QStringLiteral("repositoryAdditionStatus"));
		RepositoryAdditionStatus->setAccessibleName(tr("Repository addition status"));
		RepositoryAdditionStatus->setTextFormat(Qt::PlainText);
		RepositoryAdditionStatus->setWordWrap(true);
		RepositoryAdditionStatus->hide();
		RepositoryPrepareButton = new quiet_push_button{ tr("Choose account or set up server…"), draft };
		RepositoryPrepareButton->setObjectName(QStringLiteral("repositoryAdditionPrepare"));
		RepositoryPrepareButton->setAccessibleDescription(tr("Keep this draft while signing in or explicitly adding its server; return here before saving"));
		RepositoryPrepareButton->setAutoDefault(false);
		RepositoryPrepareButton->hide();
		RepositoryAddButton = new QPushButton{ tr("Add"), draft };
		RepositoryAddButton->setObjectName(QStringLiteral("repositoryAdditionAdd"));
		RepositoryAddButton->setAccessibleName(tr("Add repository"));
		RepositoryAddButton->setAccessibleDescription(tr("Save this repository locally for the displayed account; nothing changes on GitHub"));
		RepositoryAddButton->setAutoDefault(false);
		RepositoryAddButton->setDefault(true);
		RepositoryCancelButton = new quiet_push_button{ tr("Cancel"), draft };
		RepositoryCancelButton->setObjectName(QStringLiteral("repositoryAdditionCancel"));
		RepositoryCancelButton->setAccessibleName(tr("Cancel new repository"));
		RepositoryCancelButton->setAccessibleDescription(tr("Discard this repository draft without saving"));
		RepositoryCancelButton->setAutoDefault(false);
		RepositoryCancelButton->setFlat(true);
		auto* const draftActions = new QHBoxLayout{ };
		draftActions->setContentsMargins(0, 0, 0, 0);
		draftActions->addWidget(RepositoryPrepareButton);
		draftActions->addStretch(1);
		draftActions->addWidget(RepositoryAddButton);
		draftActions->addWidget(RepositoryCancelButton);
		grid->addWidget(draftTitle, 0, 0, 1, 2);
		grid->addWidget(RepositoryDestination, 1, 0, 1, 2);
		grid->addWidget(nameLabel, 2, 0);
		grid->addWidget(SavedRepositoryName, 2, 1);
		grid->addWidget(RepositoryAdditionStatus, 3, 0, 1, 2);
		grid->addLayout(draftActions, 4, 0, 1, 2);
		draft->hide();
		column->addWidget(draft);

		AddSavedRepositoryButton = new QPushButton{ tr("Add repository"), SavedRepositoriesEditor };
		AddSavedRepositoryButton->setObjectName(QStringLiteral("addSavedRepositoryButton"));
		AddSavedRepositoryButton->setAccessibleName(QStringLiteral("Add repository"));
		AddSavedRepositoryButton->setAccessibleDescription(QStringLiteral(
			"Adds a repository for the displayed account as a new card on this page; nothing changes on GitHub"));
		AddSavedRepositoryButton->setAutoDefault(false);
		connect(AddSavedRepositoryButton, &QPushButton::clicked, AddSavedRepository, &QAction::trigger);
		auto* const addRow = new QHBoxLayout{ };
		addRow->setContentsMargins(0, 0, 0, 0);
		addRow->addWidget(AddSavedRepositoryButton);
		addRow->addStretch(1);
		column->addLayout(addRow);
		// A save is confirmed on the page as well as in the window's status bar.
		RepositoriesConfirmation = new QLabel{ SavedRepositoriesEditor };
		RepositoriesConfirmation->setObjectName(QStringLiteral("repositoriesConfirmation"));
		RepositoriesConfirmation->setAccessibleName(tr("Repository addition result"));
		RepositoriesConfirmation->setTextFormat(Qt::PlainText);
		RepositoriesConfirmation->setWordWrap(true);
		RepositoriesConfirmation->setForegroundRole(QPalette::PlaceholderText);
		RepositoriesConfirmation->hide();
		column->addWidget(RepositoriesConfirmation);
		column->addStretch(1);

		connect(RemoveSavedRepository, &QAction::triggered, this, [this] {
			dispatch_saved_repository(application::saved_repository_action::remove);
		});
		connect(SavedRepositoryList, &QListWidget::currentRowChanged, this,
				[this](int) { update_saved_repository_actions(); });
		connect(RepositoryAddButton, &QPushButton::clicked, this, [this] { submit_repository_addition(); });
		connect(SavedRepositoryName, &QLineEdit::returnPressed, this, [this] { submit_repository_addition(); });
		connect(RepositoryCancelButton, &QPushButton::clicked, this, [this] { close_repository_draft(); });
		connect(RepositoryPrepareButton, &QPushButton::clicked, this, [this] {
			if (!SetupDestinationDispatch || RepositorySaving) {
				return;
			}
			RepositoryPreparing      = true;
			RepositoryPendingGesture = 0U;
			update_repository_destination();
			SetupDestinationDispatch(application::submit_repository_setup{ .Text = SavedRepositoryName->text().toStdString() });
		});
		connect(SavedRepositoryName, &QLineEdit::textChanged, this, [this] {
			if (!RepositorySaving) {
				repository_addition_notice({ });
				RepositoryPrepareButton->hide();
			}
		});
		SettingsWindow->adopt_panel(settings_page::repositories, SavedRepositoriesEditor);
		update_saved_repository_list();
		update_saved_repository_actions();
	}

	void workbench_window::update_saved_repository_list()
	{
		if (SavedRepositoryList == nullptr) {
			return;
		}
		// Signed in, the displayed account's records. Signed out there is no
		// displayed account, so every kept record is listed with the exact
		// account and host it is saved for, rather than an empty page.
		const auto& session  = QueueProjection.Session;
		const auto  signedIn = !session.Host.empty() && !session.Account.empty();
		auto        rows     = QStringList{ };
		auto        records  = std::vector<const application::saved_repository*>{ };
		for (const auto& saved : QueueProjection.SavedRepositories) {
			if (!signedIn) {
				rows.push_back(tr("%1 (@%2 on %3)").arg(QString::fromStdString(saved.Repository), QString::fromStdString(saved.Account), QString::fromStdString(saved.Host)));
				records.push_back(&saved);
			} else if (saved.Host == session.Host && saved.Account == session.Account) {
				rows.push_back(QString::fromStdString(saved.Repository));
				records.push_back(&saved);
			}
		}
		auto current = QStringList{ };
		for (auto row = 0; row < SavedRepositoryList->count(); ++row) {
			current.push_back(SavedRepositoryList->item(row)->text());
		}
		SavedRepositoriesDestination->setText(signedIn ? tr("Saved for @%1 on %2").arg(QString::fromStdString(session.Account), QString::fromStdString(session.Host))
													   : tr("Signed out. Sign in to add or remove repositories for an account."));
		if (rows != current) {
			// The removed row's neighbour keeps the keyboard's place; a new list
			// starts on its first row so the keyboard lands on a repository.
			const auto selected = SavedRepositoryList->currentItem() != nullptr ? SavedRepositoryList->currentItem()->text() : QString{ };
			const auto previous = SavedRepositoryList->currentRow();
			const auto focused  = SavedRepositoryList->hasFocus();
			SavedRepositoryList->clear();
			for (std::size_t index = 0U; index < records.size(); ++index) {
				const auto& saved = *records[index];
				auto* const item  = new QListWidgetItem{ rows[static_cast<qsizetype>(index)], SavedRepositoryList };
				const auto  name  = QString::fromStdString(saved.Repository);
				const auto  host  = QString::fromStdString(saved.Host);
				const auto  login = QString::fromStdString(saved.Account);
				item->setData(Qt::UserRole, name);
				item->setData(Qt::UserRole + 1, host);
				item->setData(Qt::UserRole + 2, login);
				item->setToolTip(tr("%1 on %2, saved for @%3").arg(name, host, login));
				item->setData(Qt::AccessibleDescriptionRole, tr("On %1, saved for @%2").arg(host, login));
			}
			if (const auto restored = rows.indexOf(selected); restored >= 0) {
				SavedRepositoryList->setCurrentRow(static_cast<int>(restored));
			} else if (!rows.isEmpty()) {
				SavedRepositoryList->setCurrentRow(std::clamp(previous, 0, static_cast<int>(rows.size()) - 1));
			}
			if (focused && rows.isEmpty() && AddSavedRepositoryButton != nullptr) {
				AddSavedRepositoryButton->setFocus(Qt::OtherFocusReason);
			}
		}
		SavedRepositoryList->setVisible(!rows.isEmpty());
		if (!rows.isEmpty()) {
			const auto shown  = std::min(SavedRepositoryList->count(), 10);
			auto       height = 2 * SavedRepositoryList->frameWidth();
			for (auto row = 0; row < shown; ++row) {
				height += SavedRepositoryList->sizeHintForRow(row) + 2 * SavedRepositoryList->spacing();
			}
			SavedRepositoryList->setFixedHeight(height);
		}
		SavedRepositoriesEmpty->setVisible(rows.isEmpty());
		if (RemoveSavedRepositoryButton != nullptr) {
			RemoveSavedRepositoryButton->setVisible(!rows.isEmpty());
		}
	}

	void workbench_window::build_enterprise_server_draft(QWidget* const aParent, QVBoxLayout& aLayout)
	{
		auto* const draft     = new card_surface{ aParent };
		EnterpriseServerDraft = draft;
		draft->setObjectName(QStringLiteral("enterpriseServerDraft"));
		draft->setAccessibleName(QStringLiteral("New enterprise server"));
		auto* const grid = new QGridLayout{ draft };
		grid->setContentsMargins(16, 12, 16, 12);
		grid->setHorizontalSpacing(16);
		grid->setVerticalSpacing(8);
		grid->setColumnMinimumWidth(0, 150);
		grid->setColumnStretch(1, 1);
		auto* const title = new QLabel{ QStringLiteral("Enterprise server"), draft };
		title->setObjectName(QStringLiteral("enterpriseServerDraftTitle"));
		auto titleFont = title->font();
		titleFont.setWeight(QFont::DemiBold);
		title->setFont(titleFont);
		auto* const hostnameName = new QLabel{ QStringLiteral("Hostname"), draft };
		EnterpriseServerHostname = new QLineEdit{ draft };
		EnterpriseServerHostname->setObjectName(QStringLiteral("enterpriseServerHostname"));
		EnterpriseServerHostname->setAccessibleName(QStringLiteral("Hostname"));
		EnterpriseServerHostname->setAccessibleDescription(QStringLiteral("The hostname of a GitHub Enterprise server, without a scheme"));
		EnterpriseServerHostname->setPlaceholderText(QStringLiteral("github.example.com"));
		EnterpriseServerHostname->setMaxLength(253);
		hostnameName->setBuddy(EnterpriseServerHostname);
		auto* const accountName = new QLabel{ QStringLiteral("Account"), draft };
		EnterpriseServerAccount = new QLineEdit{ draft };
		EnterpriseServerAccount->setObjectName(QStringLiteral("enterpriseServerAccount"));
		EnterpriseServerAccount->setAccessibleName(QStringLiteral("Account"));
		EnterpriseServerAccount->setAccessibleDescription(QStringLiteral("The account you sign in with there, if you know it"));
		EnterpriseServerAccount->setPlaceholderText(QStringLiteral("optional"));
		EnterpriseServerAccount->setMaxLength(128);
		accountName->setBuddy(EnterpriseServerAccount);
		EnterpriseServerRefusal = new QLabel{ draft };
		EnterpriseServerRefusal->setObjectName(QStringLiteral("enterpriseServerRefusal"));
		EnterpriseServerRefusal->setTextFormat(Qt::PlainText);
		EnterpriseServerRefusal->setWordWrap(true);
		EnterpriseServerRefusal->hide();
		EnterpriseServerAdd = new QPushButton{ QStringLiteral("Add"), draft };
		EnterpriseServerAdd->setObjectName(QStringLiteral("enterpriseServerAdd"));
		EnterpriseServerAdd->setAutoDefault(false);
		EnterpriseServerAdd->setAccessibleName(QStringLiteral("Add enterprise server"));
		EnterpriseServerAdd->setAccessibleDescription(QStringLiteral("Keeps this server so it is offered as a host. No sign-in is stored."));
		EnterpriseServerCancel = new quiet_push_button{ QStringLiteral("Cancel"), draft };
		EnterpriseServerCancel->setObjectName(QStringLiteral("enterpriseServerCancel"));
		EnterpriseServerCancel->setAutoDefault(false);
		EnterpriseServerCancel->setFlat(true);
		auto* const actions = new QHBoxLayout{ };
		actions->setContentsMargins(0, 0, 0, 0);
		actions->addStretch(1);
		actions->addWidget(EnterpriseServerAdd);
		actions->addWidget(EnterpriseServerCancel);
		grid->addWidget(title, 0, 0, 1, 2);
		grid->addWidget(hostnameName, 1, 0);
		grid->addWidget(EnterpriseServerHostname, 1, 1);
		grid->addWidget(accountName, 2, 0);
		grid->addWidget(EnterpriseServerAccount, 2, 1);
		grid->addWidget(EnterpriseServerRefusal, 3, 0, 1, 2);
		grid->addLayout(actions, 4, 0, 1, 2);
		draft->hide();
		aLayout.addWidget(draft);

		UseGithubCliButton = new QPushButton{ QStringLiteral("Use GitHub CLI"), aParent };
		UseGithubCliButton->setObjectName(QStringLiteral("accountsUseGithubCli"));
		UseGithubCliButton->setAutoDefault(false);
		UseGithubCliButton->setAccessibleDescription(QStringLiteral(
			"Checks active authenticated hosts with the installed gh. Tokens are never requested or shown."));
		UseGithubCliButton->hide();

		AddEnterpriseServerButton = new QPushButton{ QStringLiteral("Add enterprise server"), aParent };
		AddEnterpriseServerButton->setObjectName(QStringLiteral("accountsAddEnterpriseServer"));
		AddEnterpriseServerButton->setAutoDefault(false);
		AddEnterpriseServerButton->setAccessibleDescription(QStringLiteral("Adds a GitHub Enterprise server as a card, offered again at the next start. No sign-in is stored."));
		auto* const addRow = new QHBoxLayout{ };
		addRow->setContentsMargins(0, 0, 0, 0);
		addRow->addWidget(UseGithubCliButton);
		addRow->addWidget(AddEnterpriseServerButton);
		addRow->addStretch(1);
		aLayout.addLayout(addRow);
		GithubCliDiscoveryStatus = new QLabel{ aParent };
		GithubCliDiscoveryStatus->setObjectName(QStringLiteral("githubCliDiscoveryStatus"));
		GithubCliDiscoveryStatus->setTextFormat(Qt::PlainText);
		GithubCliDiscoveryStatus->setWordWrap(true);
		GithubCliDiscoveryStatus->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		GithubCliDiscoveryStatus->setForegroundRole(QPalette::PlaceholderText);
		GithubCliDiscoveryStatus->hide();
		aLayout.addWidget(GithubCliDiscoveryStatus);

		connect(UseGithubCliButton, &QPushButton::clicked, this, [this] {
			if (GithubCliDiscoveryDispatch
				&& GithubCliDiscoveryState != github_cli_discovery_state::discovering) {
				open_sign_in(SignInHostname, true);
				GithubCliDiscoveryDispatch();
			}
		});
		connect(AddEnterpriseServerButton, &QPushButton::clicked, this, [this] { open_enterprise_server_draft(); });
		connect(EnterpriseServerAdd, &QPushButton::clicked, this, [this] { add_enterprise_server(); });
		connect(EnterpriseServerHostname, &QLineEdit::returnPressed, this, [this] { add_enterprise_server(); });
		connect(EnterpriseServerAccount, &QLineEdit::returnPressed, this, [this] { add_enterprise_server(); });
		connect(EnterpriseServerCancel, &QPushButton::clicked, this, [this] { close_enterprise_server_draft(); });
		connect(EnterpriseServerHostname, &QLineEdit::textEdited, this, [this] { EnterpriseServerRefusal->hide(); });
		update_enterprise_server_actions();
	}

	void workbench_window::set_saved_server_dispatch(workbench_saved_server_dispatch aDispatch)
	{
		SavedServerDispatch = std::move(aDispatch);
		update_enterprise_server_actions();
	}

	void workbench_window::set_github_cli_discovery_dispatch(github_cli_discovery_dispatch aDispatch)
	{
		GithubCliDiscoveryDispatch = std::move(aDispatch);
		if (HomeLanding != nullptr) {
			HomeLanding->set_github_cli_available(
				static_cast<bool>(GithubCliDiscoveryDispatch));
		}
		if (UseGithubCliButton != nullptr) {
			UseGithubCliButton->setVisible(static_cast<bool>(GithubCliDiscoveryDispatch));
			UseGithubCliButton->setEnabled(
				GithubCliDiscoveryState != github_cli_discovery_state::discovering);
		}
	}

	void workbench_window::set_github_cli_check_dispatch(github_cli_discovery_dispatch aDispatch)
	{
		GithubCliCheckDispatch = std::move(aDispatch);
		update_server_cards();
	}

	void workbench_window::present_cli_accounts(std::map<std::string, std::vector<server_card_cli_account>, std::less<>> aAccounts)
	{
		CliAccountsByHost = std::move(aAccounts);
		update_server_cards();
	}

	void workbench_window::present_github_cli_discovery(
		const github_cli_discovery_state aState, const std::size_t aAccounts)
	{
		GithubCliDiscoveryState = aState;
		if (UseGithubCliButton == nullptr || GithubCliDiscoveryStatus == nullptr) {
			return;
		}
		UseGithubCliButton->setEnabled(
			static_cast<bool>(GithubCliDiscoveryDispatch)
			&& aState != github_cli_discovery_state::discovering);
		switch (aState) {
		case github_cli_discovery_state::idle:
			GithubCliDiscoveryStatus->clear();
			GithubCliDiscoveryStatus->hide();
			break;
		case github_cli_discovery_state::discovering:
			GithubCliDiscoveryStatus->setText(QStringLiteral("Checking GitHub CLI accounts…"));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::ready:
			GithubCliDiscoveryStatus->setText(
				QStringLiteral("Found %1 GitHub CLI account%2.")
					.arg(aAccounts)
					.arg(aAccounts == 1U ? QString{ } : QStringLiteral("s")));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::empty:
			GithubCliDiscoveryStatus->setText(QStringLiteral(
				"GitHub CLI reported no signed-in accounts. Run gh auth login in Terminal, then retry."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::unsupported_version:
			GithubCliDiscoveryStatus->setText(QStringLiteral(
				"This GitHub CLI version is too old. Upgrade to gh 2.96 or newer, then try again."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::process_failed:
			GithubCliDiscoveryStatus->setText(QStringLiteral(
				"GitHub CLI could not be run successfully. Verify gh auth status in Terminal, then try again."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::ineligible_accounts:
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI accounts were found, but none is both signed in and available from secure storage. Check gh auth status in Terminal. If it works there, check for operating-system updates and retry, or use Sign in."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::missing_executable:
			// [[BPR-1791]]. Each platform names where it looks besides PATH.
#if defined(_WIN32)
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI was not found. Install gh 2.96 or newer (for example, winget install GitHub.cli), then retry."));
#elif defined(__APPLE__)
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI was not found. Install gh 2.96 or newer in PATH or /usr/local/bin, then retry."));
#else
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI was not found. Install gh 2.96 or newer in PATH or /usr/bin, then retry."));
#endif
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::launch_failed:
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI could not start. Check that gh can run on this system, then retry. On macOS, /usr/bin/sandbox-exec must also be available."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::timed_out:
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI account discovery timed out. Check gh auth status in Terminal and network access, then retry."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::exited_unsuccessfully:
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI exited unsuccessfully during discovery. Check gh --version and gh auth status in Terminal, then retry."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::output_rejected:
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI output exceeded a limit or wrote unexpected diagnostic output. Check gh --version and gh auth status in Terminal; share the safe BPR diagnostic category if this persists."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::malformed_response:
			GithubCliDiscoveryStatus->setText(QStringLiteral(
				"GitHub CLI returned an account status this version of BetterPullReview could not safely read."));
			GithubCliDiscoveryStatus->show();
			break;
		}
		update_server_cards();
		update_sign_in_surface();
	}

	auto workbench_window::use_github_cli_button() const noexcept -> QPushButton*
	{
		return UseGithubCliButton;
	}

	auto workbench_window::github_cli_discovery_status() const noexcept -> QLabel*
	{
		return GithubCliDiscoveryStatus;
	}

	void workbench_window::open_enterprise_server_draft()
	{
		if (EnterpriseServerDraft == nullptr || !QueueReady || !SavedServerDispatch) {
			return;
		}
		EnterpriseDraftOpen = true;
		EnterpriseServerRefusal->hide();
		update_enterprise_server_actions();
		EnterpriseServerHostname->setFocus(Qt::OtherFocusReason);
	}

	void workbench_window::close_enterprise_server_draft()
	{
		if (EnterpriseServerDraft == nullptr) {
			return;
		}
		const auto hadFocus = EnterpriseServerDraft->isAncestorOf(QApplication::focusWidget());
		EnterpriseDraftOpen = false;
		EnterpriseServerHostname->clear();
		EnterpriseServerAccount->clear();
		EnterpriseServerRefusal->hide();
		update_enterprise_server_actions();
		if (hadFocus && AddEnterpriseServerButton->isVisible()) {
			AddEnterpriseServerButton->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::add_enterprise_server()
	{
		if (!EnterpriseDraftOpen) {
			return;
		}
		// A refusal sits beside what was refused, and the fields stay as typed.
		if (!QueueReady || !SavedServerDispatch) {
			EnterpriseServerRefusal->setText(QStringLiteral("Servers can be kept once the review queue has loaded."));
			EnterpriseServerRefusal->show();
			return;
		}
		auto server = application::saved_server{
			.Hostname = EnterpriseServerHostname->text().trimmed().toLower().toStdString(),
			.Account  = EnterpriseServerAccount->text().trimmed().toStdString(),
		};
		if (!application::valid(server)) {
			EnterpriseServerRefusal->setText(QStringLiteral("Enter a hostname such as github.example.com, without a scheme or a path."));
			EnterpriseServerRefusal->show();
			return;
		}
		SavedServerDispatch(application::saved_server_intent{
			.Action = application::saved_server_action::add,
			.Server = std::move(server),
		});
		close_enterprise_server_draft();
	}

	void workbench_window::update_enterprise_server_actions()
	{
		if (EnterpriseServerDraft == nullptr || AddEnterpriseServerButton == nullptr) {
			return;
		}
		// A server is kept without an account and without signing in: only the
		// queue's own durable state is needed, so adding waits for it.
		const auto admitted = QueueReady && static_cast<bool>(SavedServerDispatch);
		if (!admitted && EnterpriseDraftOpen) {
			EnterpriseDraftOpen = false;
			EnterpriseServerHostname->clear();
			EnterpriseServerAccount->clear();
			EnterpriseServerRefusal->hide();
		}
		EnterpriseServerDraft->setVisible(admitted && EnterpriseDraftOpen);
		AddEnterpriseServerButton->setVisible(admitted && !EnterpriseDraftOpen);
		update_server_cards();
	}

	void workbench_window::remember_enterprise_server(const std::string& aHostname)
	{
		if (!QueueReady || !SavedServerDispatch || SessionPanel == nullptr) {
			return;
		}
		// The account name kept with the hostname is the one signed in on that
		// server's card, when one is. The token is still asked for.
		auto        server  = application::saved_server{ .Hostname = aHostname };
		const auto& session = SessionPanel->session();
		const auto  card    = std::ranges::find_if(ServerCards, [&aHostname](const server_card* const aCard) {
			return aCard->hostname() == aHostname;
		});
		if (card != ServerCards.end() && session.has_value() && (*card)->holds(session->Profile)
			&& !SessionPanel->public_preview_signed_out()) {
			server.Account = session->Login;
		}
		if (!application::valid(server)) {
			return;
		}
		SavedServerDispatch(application::saved_server_intent{
			.Action = application::saved_server_action::add,
			.Server = std::move(server),
		});
	}

	void workbench_window::confirm_forget_server(const application::saved_server& aServer)
	{
		if (!QueueReady || !SavedServerDispatch) {
			return;
		}
		if (ForgetServerConfirmation != nullptr) {
			ForgetServerConfirmation->close();
		}
		// Forgetting is confirmed first, and the confirmation names exactly
		// the record it removes and what it does not touch.
		// Owned by the Settings window the reviewer is looking at: the profile's
		// shell when one is mounted, so the confirmation is modal to that page.
		auto* const owner = settings() != nullptr ? static_cast<QWidget*>(settings()) : static_cast<QWidget*>(this);
		auto* const box   = new QMessageBox{ owner };
		box->setObjectName(QStringLiteral("forgetServerConfirmation"));
		box->setAttribute(Qt::WA_DeleteOnClose);
		box->setWindowModality(Qt::WindowModal);
		box->setIcon(QMessageBox::Question);
		const auto host    = QString::fromStdString(aServer.Hostname);
		const auto account = QString::fromStdString(aServer.Account);
		box->setWindowTitle(QStringLiteral("Forget %1?").arg(host));
		box->setText(QStringLiteral("Forget %1?").arg(host));
		box->setInformativeText(
			account.isEmpty()
				? QStringLiteral("BetterPullReview stops offering %1 at the next start. Nothing on the server changes, and no sign-in is removed.").arg(host)
				: QStringLiteral("BetterPullReview forgets %1 with the account %2 and stops offering it at the next start. Nothing on the server changes, and no sign-in is removed.").arg(host, account));
		auto* const forget = box->addButton(QStringLiteral("Forget server"), QMessageBox::DestructiveRole);
		forget->setObjectName(QStringLiteral("forgetServerConfirm"));
		auto* const cancel = box->addButton(QMessageBox::Cancel);
		cancel->setObjectName(QStringLiteral("forgetServerCancel"));
		box->setDefaultButton(cancel);
		box->setEscapeButton(cancel);
		ForgetServerConfirmation = box;
		connect(box, &QMessageBox::buttonClicked, this, [this, server = aServer, forget](QAbstractButton* const aButton) {
			if (aButton != forget || !QueueReady || !SavedServerDispatch) {
				return;
			}
			SavedServerDispatch(application::saved_server_intent{
				.Action = application::saved_server_action::remove,
				.Server = server,
			});
		});
		box->open();
	}

	auto workbench_window::add_enterprise_server_button() const noexcept -> QPushButton*
	{
		return AddEnterpriseServerButton;
	}

	auto workbench_window::enterprise_server_draft() const noexcept -> QWidget*
	{
		return EnterpriseServerDraft;
	}

	auto workbench_window::enterprise_server_hostname() const noexcept -> QLineEdit*
	{
		return EnterpriseServerHostname;
	}

	auto workbench_window::enterprise_server_account() const noexcept -> QLineEdit*
	{
		return EnterpriseServerAccount;
	}

	auto workbench_window::enterprise_server_add() const noexcept -> QPushButton*
	{
		return EnterpriseServerAdd;
	}

	auto workbench_window::enterprise_server_cancel() const noexcept -> QPushButton*
	{
		return EnterpriseServerCancel;
	}

	auto workbench_window::enterprise_server_refusal() const noexcept -> QLabel*
	{
		return EnterpriseServerRefusal;
	}

	auto workbench_window::forget_server_confirmation() const noexcept -> QMessageBox*
	{
		return ForgetServerConfirmation.data();
	}

	auto workbench_window::kept_servers() const noexcept -> const std::vector<application::saved_server>&
	{
		return QueueProjection.SavedServers;
	}

	void workbench_window::set_saved_repository_dispatch(workbench_saved_repository_dispatch aDispatch)
	{
		SavedRepositoryDispatch = std::move(aDispatch);
		update_saved_repository_actions();
		update_pull_request_queue_actions();
	}

	void workbench_window::set_repository_addition_dispatch(std::function<void(application::repository_addition_intent)> aDispatch)
	{
		RepositoryAdditionDispatch = std::move(aDispatch);
		update_saved_repository_actions();
	}

	auto workbench_window::repository_addition_draft() const noexcept -> QWidget* { return RepositoryDraft; }

	void workbench_window::repository_addition_notice(const QString& aText)
	{
		if (!RepositoryAdditionStatus || RepositoryAdditionStatus->text() == aText) {
			return;
		}
		RepositoryAdditionStatus->setText(aText);
		RepositoryAdditionStatus->setVisible(!aText.isEmpty());
		if (!aText.isEmpty()) {
			QAccessibleAnnouncementEvent event{ RepositoryAdditionStatus, aText };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
	}

	void workbench_window::open_repository_addition()
	{
		if (!RepositoryAdditionDispatch || RepositoryDraft == nullptr) {
			return;
		}
		// Every Add repository is this page with the field ready. A draft kept
		// through explicit setup returns as it was; a new one starts clean.
		if (!RepositoryPreparing && !RepositoryDraftOpen) {
			repository_addition_notice({ });
			RepositoryPrepareButton->hide();
		}
		RepositoryPreparing = false;
		RepositoryDraftOpen = true;
		if (RepositoriesConfirmation != nullptr) {
			RepositoriesConfirmation->hide();
		}
		update_repository_destination();
		open_settings(settings_page::repositories);
		if (auto* const shell = settings(); shell != nullptr) {
			shell->activateWindow();
		}
		SavedRepositoryName->setFocus(Qt::OtherFocusReason);
	}

	void workbench_window::close_repository_draft()
	{
		if (RepositoryDraft == nullptr) {
			return;
		}
		const auto hadFocus      = RepositoryDraft->isAncestorOf(QApplication::focusWidget());
		const auto preparing     = RepositoryPreparing;
		RepositoryDraftOpen      = false;
		RepositoryPreparing      = false;
		RepositorySaving         = false;
		RepositoryPendingGesture = 0U;
		SavedRepositoryName->clear();
		repository_addition_notice({ });
		RepositoryPrepareButton->hide();
		if (preparing && SetupDestinationDispatch) {
			SetupDestinationDispatch(application::cancel_setup_destination{ });
		}
		update_repository_destination();
		if (hadFocus && AddSavedRepositoryButton->isVisibleTo(SavedRepositoriesEditor)) {
			AddSavedRepositoryButton->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::submit_repository_addition()
	{
		if (!RepositoryDraftOpen || RepositorySaving || RepositoryPreparing || !RepositoryAdditionDispatch
			|| RepositoryGesture == std::numeric_limits<std::uint64_t>::max()) {
			return;
		}
		RepositoryPendingGesture = ++RepositoryGesture;
		RepositoryAdditionDispatch({ .Gesture = RepositoryPendingGesture, .Destination = RepositoryDisplayedDestination, .Text = SavedRepositoryName->text().toStdString() });
	}

	void workbench_window::update_repository_destination()
	{
		if (RepositoryDraft == nullptr) {
			return;
		}
		if (RepositoryDisplayedDestination != QueueProjection.Session) {
			RepositoryDisplayedDestination = QueueProjection.Session;
			RepositoryPendingGesture       = 0U;
			RepositorySaving               = false;
			if (!SavedRepositoryName->text().isEmpty()) {
				repository_addition_notice(tr("The destination changed. Check the account and host, then choose Add."));
			}
		}
		const auto& destination = RepositoryDisplayedDestination;
		RepositoryDestination->setText(destination.Account.empty()
										   ? tr("Sign in to choose the account for this repository.")
										   : tr("Save for @%1 on %2").arg(QString::fromStdString(destination.Account), QString::fromStdString(destination.Host)));
		RepositoryAddButton->setEnabled(static_cast<bool>(RepositoryAdditionDispatch) && !RepositorySaving && !RepositoryPreparing);
		SavedRepositoryName->setEnabled(!RepositorySaving);
		RepositoryPrepareButton->setEnabled(!RepositorySaving && static_cast<bool>(SetupDestinationDispatch));
		RepositoryDraft->setVisible(RepositoryDraftOpen);
		AddSavedRepositoryButton->setVisible(!RepositoryDraftOpen);
	}
}
