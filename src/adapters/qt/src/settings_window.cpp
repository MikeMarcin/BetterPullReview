#include "bpr/qt/settings_window.hpp"

#include "bpr/core/fuzzy_match.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/resident_filter.hpp"

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <tuple>

namespace bpr::qt {
	namespace {
		constexpr auto page_order = std::array{
			settings_page::general,
			settings_page::accounts_and_hosts,
			settings_page::reviews_and_queue,
			settings_page::appearance,
			settings_page::notifications,
			settings_page::agents_and_context,
			settings_page::storage_and_privacy,
		};
		static_assert(page_order.size() == settings_page_count);

		[[nodiscard]] auto page_index(const settings_page aPage) noexcept -> int
		{
			return static_cast<int>(aPage);
		}

		/// Says where a setting lives until its page hosts a surface. Written
		/// for the reviewer looking for the setting, not for the roadmap.
		[[nodiscard]] auto page_notice(const settings_page aPage) -> QString
		{
			switch (aPage) {
			case settings_page::general:
				return QStringLiteral(
					"New windows open Home. Window restoration is not available in this build. "
					"Review navigation defaults are in Reviews & Queue; colours and fonts are in Appearance.");
			case settings_page::accounts_and_hosts:
				return QStringLiteral(
					"Sign in, sign out and enterprise servers are in the review window's "
					"header for now. Remembered servers will move here.");
			case settings_page::reviews_and_queue:
				return QStringLiteral(
					"Observer rules and queue ordering are in the review window's queue "
					"for now.");
			case settings_page::appearance:
				return QStringLiteral("Appearance settings are unavailable in this build.");
			case settings_page::notifications:
				return QStringLiteral("Local notification preferences will live here.");
			case settings_page::agents_and_context:
				return QStringLiteral("Agent profiles and context supplements will live here.");
			case settings_page::storage_and_privacy:
				return QStringLiteral(
					"Storage location, cache limits and diagnostics privacy will live "
					"here.");
			}
			return { };
		}

		/// \brief A label's words without its mnemonic or trailing colon.
		[[nodiscard]] auto plain_label(QString aText) -> QString
		{
			aText.remove(QLatin1Char('&'));
			aText = aText.simplified();
			while (aText.endsWith(QLatin1Char(':'))) {
				aText.chop(1);
			}
			return aText.trimmed();
		}

		/// \brief One searchable thing on a page, as the page shows it.
		struct search_entry
		{
			settings_page     Page = settings_page::general;
			QString           Name;
			QString           Section;
			QString           Description;
			QPointer<QWidget> Target;
			bool              PageEntry    = false;
			bool              SectionEntry = false;
			int               Order        = 0;
		};

		/// \brief Whether a widget is a setting's own control.
		///
		/// \details Pages carry no registry of their settings, so the index reads
		/// the controls they show: check boxes, choices, fields, spinners,
		/// sliders and buttons with words. An information glyph explains a
		/// setting and is not one; the editors inside a combo box or spin box
		/// belong to it.
		[[nodiscard]] auto setting_control(QWidget* const aWidget) -> bool
		{
			if (dynamic_cast<info_glyph*>(aWidget) != nullptr) {
				return false;
			}
			if (auto* const parent = aWidget->parentWidget();
				parent != nullptr && (qobject_cast<QComboBox*>(parent) != nullptr || qobject_cast<QAbstractSpinBox*>(parent) != nullptr)) {
				return false;
			}
			return qobject_cast<QAbstractButton*>(aWidget) != nullptr || qobject_cast<QComboBox*>(aWidget) != nullptr
				|| qobject_cast<QLineEdit*>(aWidget) != nullptr || qobject_cast<QAbstractSpinBox*>(aWidget) != nullptr
				|| qobject_cast<QAbstractSlider*>(aWidget) != nullptr;
		}

		/// \brief The section a control belongs to, and the widget that heads it.
		///
		/// \details The nearest titled group box, then an appearance card (a
		/// frame named `…Card` and named for its title), then the last heading
		/// label (`…Heading`) met before it on the page.
		[[nodiscard]] auto section_of(QWidget* const aWidget, QWidget* const aSurface, const std::pair<QString, QWidget*>& aHeading)
			-> std::pair<QString, QWidget*>
		{
			for (auto* ancestor = aWidget->parentWidget(); ancestor != nullptr && ancestor != aSurface; ancestor = ancestor->parentWidget()) {
				if (auto* const group = qobject_cast<QGroupBox*>(ancestor); group != nullptr && !plain_label(group->title()).isEmpty()) {
					return { plain_label(group->title()), group };
				}
				if (ancestor->objectName().endsWith(QStringLiteral("Card")) && !ancestor->accessibleName().isEmpty()) {
					return { plain_label(ancestor->accessibleName()), ancestor };
				}
			}
			return aHeading;
		}

		/// \brief The name a page shows for a control.
		[[nodiscard]] auto control_name(QWidget* const aWidget, const std::map<QWidget*, QString>& aLabels) -> QString
		{
			if (const auto labelled = aLabels.find(aWidget); labelled != aLabels.end() && !labelled->second.isEmpty()) {
				return labelled->second;
			}
			// An appearance row: a frame `…Row` holding a `…RowName` label.
			for (auto* ancestor = aWidget->parentWidget(); ancestor != nullptr; ancestor = ancestor->parentWidget()) {
				if (!ancestor->objectName().endsWith(QStringLiteral("Row"))) {
					continue;
				}
				if (auto* const name = ancestor->findChild<QLabel*>(ancestor->objectName() + QStringLiteral("Name")); name != nullptr) {
					return plain_label(name->text());
				}
				break;
			}
			if (auto* const button = qobject_cast<QAbstractButton*>(aWidget); button != nullptr && !plain_label(button->text()).isEmpty()) {
				return plain_label(button->text());
			}
			return plain_label(aWidget->accessibleName());
		}

		/// \brief Every searchable entry of one page, in the page's order.
		void index_page(const settings_page aPage, QWidget* const aSurface, std::vector<search_entry>& aOut)
		{
			const auto pageTitle = settings_page_title(aPage);
			aOut.push_back({ .Page = aPage, .Name = pageTitle, .PageEntry = true, .Order = static_cast<int>(aOut.size()) });
			auto labels = std::map<QWidget*, QString>{ };
			for (auto* const form : aSurface->findChildren<QFormLayout*>()) {
				for (auto row = 0; row < form->rowCount(); ++row) {
					auto* const field = form->itemAt(row, QFormLayout::FieldRole);
					auto* const label = form->itemAt(row, QFormLayout::LabelRole);
					if (field == nullptr || label == nullptr || field->widget() == nullptr) {
						continue;
					}
					if (auto* const text = qobject_cast<QLabel*>(label->widget()); text != nullptr) {
						labels.emplace(field->widget(), plain_label(text->text()));
					}
				}
			}
			for (auto* const label : aSurface->findChildren<QLabel*>()) {
				if (label->buddy() != nullptr) {
					labels.emplace(label->buddy(), plain_label(label->text()));
				}
			}
			auto heading  = std::pair<QString, QWidget*>{ };
			auto sections = std::set<QString>{ };
			auto rows     = std::set<std::pair<QString, QString>>{ };
			for (auto* const widget : aSurface->findChildren<QWidget*>()) {
				if (!widget->isVisibleTo(aSurface)) {
					continue;
				}
				if (auto* const label = qobject_cast<QLabel*>(widget);
					label != nullptr && label->objectName().endsWith(QStringLiteral("Heading")) && !plain_label(label->text()).isEmpty()) {
					heading = { plain_label(label->text()), label };
				}
				if (!setting_control(widget)) {
					continue;
				}
				const auto [section, sectionWidget] = section_of(widget, aSurface, heading);
				auto name                           = control_name(widget, labels);
				// A segmented choice names each option "Setting: option"; the
				// setting is one result, focused on its chosen option.
				auto* const button = qobject_cast<QAbstractButton*>(widget);
				if (button != nullptr && button->isCheckable()) {
					const auto accessible = widget->accessibleName();
					if (const auto split = accessible.indexOf(QStringLiteral(": ")); split > 0) {
						name = plain_label(accessible.left(split));
					}
				}
				if (name.isEmpty()) {
					continue;
				}
				// Several controls of one row (a choice's options, a size's field
				// and its buttons) are one setting: the first is kept, focused on
				// a chosen option when there is one.
				if (!rows.emplace(section, name).second) {
					if (button != nullptr && button->isCheckable() && button->isChecked()) {
						for (auto& entry : aOut) {
							if (entry.Page == aPage && entry.Section == section && entry.Name == name && !entry.SectionEntry) {
								entry.Target = widget;
							}
						}
					}
					continue;
				}
				if (!section.isEmpty() && sections.insert(section).second) {
					aOut.push_back({ .Page = aPage, .Name = section, .Target = sectionWidget, .SectionEntry = true, .Order = static_cast<int>(aOut.size()) });
				}
				aOut.push_back({ .Page        = aPage,
								 .Name        = name,
								 .Section     = section,
								 .Description = widget->accessibleDescription(),
								 .Target      = widget,
								 .Order       = static_cast<int>(aOut.size()) });
			}
		}

		/// Ranking offsets: a setting found by its name ranks above one found
		/// by its section or its description.
		inline constexpr std::int32_t section_field_offset     = 20'000;
		inline constexpr std::int32_t description_field_offset = 40'000;

		/// \brief An entry's best match over its fields.
		[[nodiscard]] auto match_entry(const std::string& aQuery, const search_entry& aEntry)
			-> std::optional<core::fuzzy_match_result>
		{
			auto       best     = std::optional<core::fuzzy_match_result>{ };
			const auto consider = [&](const QString& aText, const std::int32_t aOffset, const bool aWordsOnly) {
				if (aText.isEmpty()) {
					return;
				}
				// A section or a description is a sentence, which scattered
				// letters of almost any query fall into; it counts only where
				// the query starts one of its words.
				if (auto match = core::fuzzy_match(aQuery, aText.toStdString());
					match.has_value() && (!aWordsOnly || match->Kind <= core::fuzzy_match_kind::word_prefix)) {
					match->Score -= aOffset;
					if (!best.has_value() || match->Score > best->Score) {
						best = match;
					}
				}
			};
			consider(aEntry.Name, 0, false);
			// A page's own result stands for its page; its settings are found by
			// their names, sections and descriptions, not listed wholesale.
			if (!aEntry.PageEntry && !aEntry.SectionEntry) {
				consider(aEntry.Section, section_field_offset, true);
				consider(aEntry.Description, description_field_offset, true);
			}
			return best;
		}
	} // namespace

	auto settings_page_title(const settings_page aPage) -> QString
	{
		switch (aPage) {
		case settings_page::general:
			return QStringLiteral("General");
		case settings_page::accounts_and_hosts:
			return QStringLiteral("Accounts & hosts");
		case settings_page::reviews_and_queue:
			return QStringLiteral("Reviews & queue");
		case settings_page::appearance:
			return QStringLiteral("Appearance");
		case settings_page::notifications:
			return QStringLiteral("Notifications");
		case settings_page::agents_and_context:
			return QStringLiteral("Agents & context");
		case settings_page::storage_and_privacy:
			return QStringLiteral("Storage & privacy");
		}
		return { };
	}

	void settings_window::set_profile_label(const QString& aLabel)
	{
		const auto title = QStringLiteral("Settings — %1 profile").arg(aLabel);
		setWindowTitle(title);
		setAccessibleName(title);
	}

	settings_window::settings_window(QWidget* const aParent)
		: QWidget{ aParent, Qt::Window }
	{
		setObjectName(QStringLiteral("settingsWindow"));
		setWindowTitle(QStringLiteral("Settings — Default profile"));
		setAccessibleName(QStringLiteral("Settings — Default profile"));
		setAccessibleDescription(QStringLiteral(
			"Application settings grouped by page; changes apply as they are made"));
		// A modeless window that a reviewer opens beside the comparison, so
		// closing it must not destroy the panels it adopted: hide, never delete.
		setAttribute(Qt::WA_DeleteOnClose, false);
		setMinimumSize(760, 480);
		resize(960, 640);

		// [[BPR-1289]]. The sidebar is a column: the search field, then the
		// page list, which a search's results or its empty notice replace.
		SidebarColumn = new QWidget{ this };
		SidebarColumn->setObjectName(QStringLiteral("settingsSidebarColumn"));
		SidebarColumn->setFixedWidth(196);
		auto* const sidebarLayout = new QVBoxLayout{ SidebarColumn };
		sidebarLayout->setContentsMargins(6, 8, 6, 0);
		sidebarLayout->setSpacing(6);
		Search = new QLineEdit{ SidebarColumn };
		Search->setObjectName(QStringLiteral("settingsSearch"));
		Search->setAccessibleName(QStringLiteral("Search settings"));
		Search->setAccessibleDescription(QStringLiteral(
			"Find a setting by its name, section, page or description; Down moves to the results, "
			"Return opens the first, Escape clears"));
		Search->setPlaceholderText(QStringLiteral("Search settings"));
		Search->setClearButtonEnabled(true);
		Search->installEventFilter(this);
		sidebarLayout->addWidget(Search);

		Sidebar = new QListWidget{ SidebarColumn };
		Sidebar->setObjectName(QStringLiteral("settingsSidebar"));
		Sidebar->setAccessibleName(QStringLiteral("Settings pages"));
		Sidebar->setFrameShape(QFrame::NoFrame);
		Sidebar->setSelectionMode(QAbstractItemView::SingleSelection);
		sidebarLayout->addWidget(Sidebar, 1);

		Results = new QListWidget{ SidebarColumn };
		Results->setObjectName(QStringLiteral("settingsSearchResults"));
		Results->setAccessibleName(QStringLiteral("Matching settings"));
		Results->setAccessibleDescription(QStringLiteral(
			"Each result names a setting, then its page and section; Return opens it"));
		Results->setFrameShape(QFrame::NoFrame);
		Results->setSelectionMode(QAbstractItemView::SingleSelection);
		Results->setWordWrap(true);
		Results->setTextElideMode(Qt::ElideRight);
		Results->hide();
		sidebarLayout->addWidget(Results, 1);

		SearchNotice = new filter_empty_notice{ QStringLiteral("settings"), QStringLiteral("settingsSearch"), SidebarColumn };
		SearchNotice->hide();
		sidebarLayout->addWidget(SearchNotice);
		sidebarLayout->addStretch(0);

		Pages = new QStackedWidget{ this };
		Pages->setObjectName(QStringLiteral("settingsPages"));
		Pages->setAccessibleName(QStringLiteral("Settings page"));

		for (const auto page : page_order) {
			Sidebar->addItem(settings_page_title(page));
			auto* const scroll = new QScrollArea{ Pages };
			scroll->setObjectName(QStringLiteral("settingsPageScroll-%1").arg(page_index(page)));
			scroll->setAccessibleName(settings_page_title(page));
			scroll->setWidgetResizable(true);
			scroll->setFrameShape(QFrame::NoFrame);
			auto* const surface = new QWidget;
			surface->setObjectName(
				QStringLiteral("settingsPage-%1").arg(page_index(page)));
			auto* const column = new QVBoxLayout{ surface };
			column->setContentsMargins(20, 16, 20, 16);
			auto* const title = new QLabel{ settings_page_title(page), surface };
			title->setObjectName(QStringLiteral("settingsPageTitle"));
			auto titleFont = title->font();
			titleFont.setPointSizeF(titleFont.pointSizeF() * 1.25);
			titleFont.setWeight(QFont::DemiBold);
			title->setFont(titleFont);
			column->addWidget(title);
			auto* const notice = new QLabel{ page_notice(page), surface };
			notice->setObjectName(QStringLiteral("settingsPageNotice"));
			notice->setWordWrap(true);
			column->addWidget(notice);
			column->addStretch(1);
			scroll->setWidget(surface);
			Pages->addWidget(scroll);
		}

		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(0, 0, 0, 0);
		row->setSpacing(0);
		row->addWidget(SidebarColumn);
		row->addWidget(Pages, 1);

		connect(Sidebar, &QListWidget::currentRowChanged, this, [this](const int aRow) {
			select_page(aRow);
		});
		connect(Search, &QLineEdit::textChanged, this, [this] { refresh_search(); });
		connect(Results, &QListWidget::itemActivated, this, [this](QListWidgetItem* const aItem) {
			go_to_search_result(Results->row(aItem));
		});
		connect(Results, &QListWidget::itemClicked, this, [this](QListWidgetItem* const aItem) {
			go_to_search_result(Results->row(aItem));
		});
		connect(SearchNotice->clear_button(), &QPushButton::clicked, this, [this] {
			Search->clear();
			Search->setFocus(Qt::OtherFocusReason);
		});
		// The platform's find key: Command-F on macOS, Ctrl+F elsewhere.
		auto* const find = new QShortcut{ QKeySequence::Find, this };
		find->setContext(Qt::WindowShortcut);
		connect(find, &QShortcut::activated, this, [this] {
			Search->setFocus(Qt::ShortcutFocusReason);
			Search->selectAll();
		});
		Sidebar->setCurrentRow(page_index(settings_page::appearance));
	}

	settings_window::~settings_window() = default;

	void settings_window::adopt_panel(const settings_page aPage, QWidget* const aPanel)
	{
		auto* const surface = page_widget(aPage);
		if (surface == nullptr || aPanel == nullptr) {
			return;
		}
		auto* const column = qobject_cast<QVBoxLayout*>(surface->layout());
		if (column == nullptr) {
			return;
		}
		if (auto* const notice = surface->findChild<QLabel*>(
				QStringLiteral("settingsPageNotice"), Qt::FindDirectChildrenOnly);
			notice != nullptr) {
			notice->hide();
		}
		aPanel->setParent(surface);
		// Between the title and the trailing stretch.
		column->insertWidget(1, aPanel);
		aPanel->show();
		refresh_search();
	}

	void settings_window::withdraw_page(const settings_page aPage)
	{
		auto* const item = Sidebar->item(page_index(aPage));
		if (item == nullptr) {
			return;
		}
		item->setHidden(true);
		if (Sidebar->currentRow() == page_index(aPage)) {
			Sidebar->setCurrentRow(page_index(settings_page::general));
		}
		refresh_search();
	}

	void settings_window::present(const settings_page aPage)
	{
		if (!isVisible()) {
			ReturnFocus = QApplication::focusWidget();
		}
		const auto* const item = Sidebar->item(page_index(aPage));
		Sidebar->setCurrentRow(page_index(item != nullptr && item->isHidden() ? settings_page::general : aPage));
		show();
		raise();
		activateWindow();
	}

	auto settings_window::current_page() const noexcept -> settings_page
	{
		const auto row = Pages->currentIndex();
		if (row < 0 || row >= static_cast<int>(settings_page_count)) {
			return settings_page::general;
		}
		return page_order[static_cast<std::size_t>(row)];
	}

	auto settings_window::sidebar() const noexcept -> QListWidget*
	{
		return Sidebar;
	}

	auto settings_window::pages() const noexcept -> QStackedWidget*
	{
		return Pages;
	}

	auto settings_window::page_widget(const settings_page aPage) const noexcept -> QWidget*
	{
		auto* const scroll = qobject_cast<QScrollArea*>(Pages->widget(page_index(aPage)));
		return scroll != nullptr ? scroll->widget() : nullptr;
	}

	auto settings_window::search_field() const noexcept -> QLineEdit*
	{
		return Search;
	}

	auto settings_window::search_results() const noexcept -> QListWidget*
	{
		return Results;
	}

	auto settings_window::search_notice() const noexcept -> QWidget*
	{
		return SearchNotice;
	}

	void settings_window::refresh_search()
	{
		Results->clear();
		Hits.clear();
		const auto query = Search->text().simplified();
		if (query.isEmpty()) {
			Results->hide();
			SearchNotice->hide();
			Sidebar->show();
			return;
		}
		// The index is read from the pages as they are now, so a new setting
		// is found without a second list, and a withdrawn page never is.
		auto entries = std::vector<search_entry>{ };
		for (const auto page : page_order) {
			const auto* const item    = Sidebar->item(page_index(page));
			auto* const       surface = page_widget(page);
			if (item != nullptr && !item->isHidden() && surface != nullptr) {
				index_page(page, surface, entries);
			}
		}
		struct scored
		{
			const search_entry*      Entry = nullptr;
			core::fuzzy_match_result Match;
		};
		const auto utf8    = query.toStdString();
		auto       matches = std::vector<scored>{ };
		for (const auto& entry : entries) {
			if (const auto match = match_entry(utf8, entry); match.has_value()) {
				matches.push_back({ &entry, *match });
			}
		}
		// Typo tolerance is the core's rescue: it answers only when nothing
		// matched directly.
		if (std::ranges::any_of(matches, [](const scored& aMatch) { return aMatch.Match.Kind != core::fuzzy_match_kind::typo; })) {
			std::erase_if(matches, [](const scored& aMatch) { return aMatch.Match.Kind == core::fuzzy_match_kind::typo; });
		}
		// Grouped by page in sidebar order; within a page, the page itself,
		// then by score, then in the page's own order.
		std::ranges::stable_sort(matches, [](const scored& aLeft, const scored& aRight) {
			return std::tuple{ page_index(aLeft.Entry->Page), !aLeft.Entry->PageEntry, -aLeft.Match.Score, aLeft.Entry->Order }
			< std::tuple{ page_index(aRight.Entry->Page), !aRight.Entry->PageEntry, -aRight.Match.Score, aRight.Entry->Order };
		});
		for (const auto& match : matches) {
			const auto& entry    = *match.Entry;
			const auto  page     = settings_page_title(entry.Page);
			const auto  location = entry.PageEntry ? QStringLiteral("Settings page")
				: entry.SectionEntry || entry.Section.isEmpty()
				? page
				: QStringLiteral("%1 · %2").arg(page, entry.Section);
			auto* const item     = new QListWidgetItem{ QStringLiteral("%1\n%2").arg(entry.Name, location), Results };
			item->setData(Qt::AccessibleTextRole, QStringLiteral("%1, %2").arg(entry.Name, location));
			item->setToolTip(entry.Description.isEmpty() ? location : entry.Description);
			Hits.push_back({ .Page = entry.Page, .Name = entry.Name, .Location = location, .Target = entry.Target });
		}
		Sidebar->hide();
		Results->setVisible(!Hits.empty());
		SearchNotice->setVisible(Hits.empty());
		if (Hits.empty()) {
			SearchNotice->set_filter(query);
		} else {
			Results->setCurrentRow(0);
		}
	}

	void settings_window::go_to_search_result(const int aRow)
	{
		if (aRow < 0 || static_cast<std::size_t>(aRow) >= Hits.size()) {
			return;
		}
		const auto hit = Hits[static_cast<std::size_t>(aRow)];
		Sidebar->setCurrentRow(page_index(hit.Page));
		auto* const scroll = qobject_cast<QScrollArea*>(Pages->widget(page_index(hit.Page)));
		if (scroll == nullptr) {
			return;
		}
		if (auto* const layout = scroll->widget()->layout(); layout != nullptr) {
			layout->activate();
		}
		// Bring it into view and give it focus; choosing changes no setting.
		if (hit.Target != nullptr && hit.Target->isVisibleTo(scroll->widget())) {
			scroll->ensureWidgetVisible(hit.Target, 24, 24);
			hit.Target->setFocus(Qt::OtherFocusReason);
			return;
		}
		scroll->verticalScrollBar()->setValue(0);
		scroll->setFocus(Qt::OtherFocusReason);
	}

	auto settings_window::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aWatched == Search && aEvent->type() == QEvent::KeyPress) {
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			if (key->key() == Qt::Key_Escape && !Search->text().isEmpty()) {
				// Escape clears a search; an empty field lets it close the window.
				Search->clear();
				return true;
			}
			if (key->key() == Qt::Key_Down && Results->isVisible() && Results->count() > 0) {
				Results->setFocus(Qt::TabFocusReason);
				Results->setCurrentRow(std::max(0, Results->currentRow()));
				return true;
			}
			if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && Results->isVisible() && Results->count() > 0) {
				go_to_search_result(std::max(0, Results->currentRow()));
				return true;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void settings_window::set_close_handler(std::function<void()> aHandler)
	{
		CloseHandler = std::move(aHandler);
	}

	void settings_window::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		auto labelWidth = 0;
		for (auto index = 0; index < Sidebar->count(); ++index) {
			labelWidth = std::max(labelWidth, Sidebar->fontMetrics().horizontalAdvance(Sidebar->item(index)->text()));
		}
		// Keep category names visible while returning unused sidebar width to
		// the page in a small window. Native font metrics own the minimum.
		SidebarColumn->setFixedWidth(std::max(labelWidth + 32, width() < 900 ? 0 : 196));
	}

	void settings_window::closeEvent(QCloseEvent* const aEvent)
	{
		QWidget::closeEvent(aEvent);
		if (aEvent->isAccepted() && CloseHandler) {
			CloseHandler();
		}
		if (aEvent->isAccepted() && ReturnFocus != nullptr && ReturnFocus->isVisible()) {
			ReturnFocus->window()->activateWindow();
			ReturnFocus->setFocus(Qt::OtherFocusReason);
		}
	}

	void settings_window::keyPressEvent(QKeyEvent* const aEvent)
	{
		// Child pickers and editors receive Escape first. Only an unhandled
		// key reaches the window, so closing cannot steal their cancellation.
		if (aEvent->key() == Qt::Key_Escape && aEvent->modifiers() == Qt::NoModifier) {
			close();
			aEvent->accept();
			return;
		}
		QWidget::keyPressEvent(aEvent);
	}

	void settings_window::select_page(const int aRow)
	{
		if (aRow < 0 || aRow >= Pages->count()) {
			return;
		}
		Pages->setCurrentIndex(aRow);
	}
} // namespace bpr::qt
