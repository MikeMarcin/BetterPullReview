#include "bpr/qt/settings_window.hpp"

#include "bpr/core/fuzzy_match.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/resident_filter.hpp"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPoint>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <tuple>

namespace bpr::qt {
	namespace {
		constexpr auto page_order = std::array{
			settings_page::general,
			settings_page::accounts_and_hosts,
			settings_page::repositories,
			settings_page::home_and_queue,
			settings_page::review,
			settings_page::comments,
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
					"How a pull request opens and reads is in Review; colours and fonts are in Appearance.");
			case settings_page::accounts_and_hosts:
				return QStringLiteral(
					"Sign in, sign out and enterprise servers are in the review window's "
					"header for now. Remembered servers will move here.");
			case settings_page::repositories:
				return QStringLiteral("Saved repositories for the signed-in account will be listed here.");
			case settings_page::home_and_queue:
				return QStringLiteral(
					"Observer rules and queue ordering are in the review window's queue "
					"for now.");
			case settings_page::review:
				return QStringLiteral("How a pull request opens and reads will be set here.");
			case settings_page::comments:
				return QStringLiteral("Comment labels and the comment workshop will be set here.");
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

		/// \brief Which of an entry's texts a query matched.
		enum class search_field : std::uint8_t {
			name,
			section,
			description,
		};

		/// \brief An entry's best match and the text it was found in.
		struct entry_match
		{
			core::fuzzy_match_result Match;
			search_field             Field = search_field::name;
		};

		/// \brief An entry's best match over its fields.
		[[nodiscard]] auto match_entry(const std::string& aQuery, const search_entry& aEntry) -> std::optional<entry_match>
		{
			auto       best     = std::optional<entry_match>{ };
			const auto consider = [&](const QString& aText, const search_field aField, const std::int32_t aOffset) {
				if (aText.isEmpty()) {
					return;
				}
				// A section or a description is a sentence, which scattered
				// letters of almost any query fall into; it counts only where
				// the query starts one of its words.
				const auto wordsOnly = aField != search_field::name;
				if (auto match = core::fuzzy_match(aQuery, aText.toStdString());
					match.has_value() && (!wordsOnly || match->Kind <= core::fuzzy_match_kind::word_prefix)) {
					match->Score -= aOffset;
					if (!best.has_value() || match->Score > best->Match.Score) {
						best = entry_match{ .Match = std::move(*match), .Field = aField };
					}
				}
			};
			consider(aEntry.Name, search_field::name, 0);
			// A page's own result stands for its page; its settings are found by
			// their names, sections and descriptions, not listed wholesale.
			if (!aEntry.PageEntry && !aEntry.SectionEntry) {
				consider(aEntry.Section, search_field::section, section_field_offset);
				consider(aEntry.Description, search_field::description, description_field_offset);
			}
			return best;
		}

		/// \brief The core's matched byte runs of `aText` as UTF-16 runs,
		/// moved along by `aOffset` characters.
		[[nodiscard]] auto utf16_spans(const QString& aText, const std::vector<core::fuzzy_match_span>& aSpans, const qsizetype aOffset)
			-> QVariantList
		{
			const auto utf8   = aText.toUtf8();
			auto       result = QVariantList{ };
			for (const auto& span : aSpans) {
				const auto begin = QString::fromUtf8(utf8.first(static_cast<qsizetype>(span.Begin))).size();
				const auto end   = QString::fromUtf8(utf8.first(static_cast<qsizetype>(span.Begin) + static_cast<qsizetype>(span.Length))).size();
				result.push_back(QPoint{ static_cast<int>(aOffset + begin), static_cast<int>(end - begin) });
			}
			return result;
		}

		/// [[BPR-1289]]. A result's location is set a little smaller than its
		/// name, as Home's second line is.
		constexpr auto secondary_scale = 0.92;
		/// Above and below a result's lines, and between them.
		constexpr auto result_padding  = 5;
		constexpr auto result_line_gap = 1;

		[[nodiscard]] auto secondary_font(const QFont& aFont) -> QFont
		{
			auto font = aFont;
			if (font.pointSizeF() > 0.0) {
				font.setPointSizeF(font.pointSizeF() * secondary_scale);
			} else if (font.pixelSize() > 0) {
				font.setPixelSize(std::max(1, static_cast<int>(std::lround(font.pixelSize() * secondary_scale))));
			}
			return font;
		}

		/// \brief Draws one line of a result, elided to its width, with its
		/// matched runs set heavier and in `aMatchInk`.
		void draw_result_line(QPainter&           aPainter,
							  const QString&      aText,
							  const QVariantList& aSpans,
							  const QFont&        aFont,
							  const QColor&       aInk,
							  const QColor&       aMatchInk,
							  const QPoint        aTopLeft,
							  const int           aWidth)
		{
			auto heavy = aFont;
			heavy.setWeight(QFont::DemiBold);
			// Elided by the heavier face, so a highlighted line still fits.
			const auto shown = QFontMetrics{ heavy }.elidedText(aText, Qt::ElideRight, aWidth);
			const auto kept  = shown == aText ? shown.size() : shown.size() - 1;
			auto       runs  = QList<QTextLayout::FormatRange>{ };
			for (const auto& value : aSpans) {
				const auto span  = value.toPoint();
				const auto begin = std::min<qsizetype>(span.x(), kept);
				const auto end   = std::min<qsizetype>(span.x() + span.y(), kept);
				if (end <= begin) {
					continue;
				}
				auto format = QTextCharFormat{ };
				format.setFontWeight(QFont::DemiBold);
				format.setForeground(aMatchInk);
				runs.push_back({ .start = static_cast<int>(begin), .length = static_cast<int>(end - begin), .format = format });
			}
			auto layout = QTextLayout{ shown, aFont };
			layout.setFormats(runs);
			layout.beginLayout();
			auto line = layout.createLine();
			line.setLineWidth(aWidth);
			layout.endLayout();
			aPainter.setPen(aInk);
			layout.draw(&aPainter, aTopLeft);
		}

		/// \brief [[BPR-1289]]. Paints a search result: the setting's name, and
		/// under it where it lives, smaller and quieter. Matched letters keep
		/// their line's ink and are set heavier; in the quiet line they rise
		/// to the name's ink. The style paints the row's ground, so hover and
		/// selection stay native.
		class search_result_delegate final : public QStyledItemDelegate
		{
		public:
			using QStyledItemDelegate::QStyledItemDelegate;

			void paint(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				auto option = aOption;
				initStyleOption(&option, aIndex);
				auto* const style  = option.widget != nullptr ? option.widget->style() : QApplication::style();
				const auto  text   = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
				auto        ground = option;
				ground.text.clear();
				ground.features &= ~QStyleOptionViewItem::HasDisplay;
				style->drawControl(QStyle::CE_ItemViewItem, &ground, aPainter, option.widget);

				const auto selected = option.state.testFlag(QStyle::State_Selected);
				const auto group    = !option.state.testFlag(QStyle::State_Enabled) ? QPalette::Disabled
					: option.state.testFlag(QStyle::State_Active)                   ? QPalette::Active
																					: QPalette::Inactive;
				const auto ink      = option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
				const auto quiet    = option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::PlaceholderText);
				const auto left     = text.left();
				const auto width    = std::max(0, option.rect.right() - left - (left - option.rect.left()));
				auto       top      = option.rect.top() + result_padding;
				aPainter->save();
				aPainter->setClipRect(option.rect);
				draw_result_line(*aPainter, aIndex.data(Qt::DisplayRole).toString(), aIndex.data(settings_search_name_spans_role).toList(),
								 option.font, ink, ink, QPoint{ left, top }, width);
				const auto location = aIndex.data(settings_search_location_role).toString();
				if (!location.isEmpty()) {
					top += QFontMetrics{ option.font }.height() + result_line_gap;
					draw_result_line(*aPainter, location, aIndex.data(settings_search_location_spans_role).toList(),
									 secondary_font(option.font), quiet, ink, QPoint{ left, top }, width);
				}
				aPainter->restore();
			}

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				auto option = aOption;
				initStyleOption(&option, aIndex);
				auto height = (2 * result_padding) + QFontMetrics{ option.font }.height();
				if (!aIndex.data(settings_search_location_role).toString().isEmpty()) {
					height += result_line_gap + QFontMetrics{ secondary_font(option.font) }.height();
				}
				return { std::max(0, option.rect.width()), height };
			}
		};

		/// \brief A magnifier for the search field, drawn in `aInk` at the
		/// sizes and scales the field may ask for.
		[[nodiscard]] auto magnifier_icon(const QColor& aInk) -> QIcon
		{
			auto icon = QIcon{ };
			for (const auto logical : { 14, 16 }) {
				for (const auto scale : { 1.0, 1.25, 1.5, 2.0 }) {
					auto pixmap = QPixmap{ QSize{ logical, logical } * scale };
					pixmap.setDevicePixelRatio(scale);
					pixmap.fill(Qt::transparent);
					auto painter = QPainter{ &pixmap };
					painter.setRenderHint(QPainter::Antialiasing, true);
					const auto unit = logical / 16.0;
					auto       pen  = QPen{ aInk, 1.5 * unit };
					pen.setCapStyle(Qt::RoundCap);
					painter.setPen(pen);
					painter.drawEllipse(QPointF{ 6.75 * unit, 6.75 * unit }, 4.5 * unit, 4.5 * unit);
					painter.drawLine(QPointF{ 10.2 * unit, 10.2 * unit }, QPointF{ 13.75 * unit, 13.75 * unit });
					painter.end();
					icon.addPixmap(pixmap);
				}
			}
			return icon;
		}
	} // namespace

	auto settings_page_title(const settings_page aPage) -> QString
	{
		switch (aPage) {
		case settings_page::general:
			return QStringLiteral("General");
		case settings_page::accounts_and_hosts:
			return QStringLiteral("Accounts & hosts");
		case settings_page::repositories:
			return QStringLiteral("Repositories");
		case settings_page::home_and_queue:
			return QStringLiteral("Home & queue");
		case settings_page::review:
			return QStringLiteral("Review");
		case settings_page::comments:
			return QStringLiteral("Comments");
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
			"Find a setting by its name, section, page or description; Up and Down choose a result, "
			"Return opens it, Escape clears"));
		Search->setPlaceholderText(QStringLiteral("Search settings"));
		Search->setClearButtonEnabled(true);
		Search->installEventFilter(this);
		SearchIcon = Search->addAction(QIcon{ }, QLineEdit::LeadingPosition);
		SearchIcon->setObjectName(QStringLiteral("settingsSearchIcon"));
		refresh_search_icon();
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
			"Each result names a setting, then its page and section; Return opens it, Escape returns to the search"));
		Results->setFrameShape(QFrame::NoFrame);
		Results->setSelectionMode(QAbstractItemView::SingleSelection);
		Results->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Results->setItemDelegate(new search_result_delegate{ Results });
		Results->installEventFilter(this);
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
		auto* const find = new QShortcut{ this };
		find->setKeys(shortcut_keys(shortcut_command::settings_find));
		find->setObjectName(QStringLiteral("settingsFindShortcut"));
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
			const search_entry* Entry = nullptr;
			entry_match         Match;
		};
		const auto utf8    = query.toStdString();
		auto       matches = std::vector<scored>{ };
		for (const auto& entry : entries) {
			if (auto match = match_entry(utf8, entry); match.has_value()) {
				matches.push_back({ .Entry = &entry, .Match = std::move(*match) });
			}
		}
		// The core's rescues: scattered letters answer only when no entry
		// holds the query as a run, and typos only when none holds its letters.
		// A description is not shown, so a match found only there answers
		// only when no name, section or page holds the query as near.
		const auto reach = [](const scored& aMatch) {
			return std::pair{ core::fuzzy_reach(aMatch.Match.Match.Kind), aMatch.Match.Field == search_field::description };
		};
		if (!matches.empty()) {
			const auto nearest = reach(std::ranges::min(matches, { }, reach));
			std::erase_if(matches, [&](const scored& aMatch) { return reach(aMatch) != nearest; });
		}
		// Grouped by page in sidebar order, then by section in the page's
		// order, a page or section ahead of its settings; within a section by
		// score, then in the page's own order.
		auto sectionOrder = std::map<std::pair<settings_page, QString>, int>{ };
		for (const auto& entry : entries) {
			if (entry.SectionEntry) {
				sectionOrder.emplace(std::pair{ entry.Page, entry.Name }, entry.Order);
			}
		}
		const auto key = [&sectionOrder](const scored& aMatch) {
			const auto& entry   = *aMatch.Entry;
			const auto  section = sectionOrder.find(std::pair{ entry.Page, entry.SectionEntry ? entry.Name : entry.Section });
			const auto  group   = entry.PageEntry || section == sectionOrder.end() ? -1 : section->second;
			return std::tuple{ page_index(entry.Page), !entry.PageEntry, group, !entry.SectionEntry, -aMatch.Match.Match.Score, entry.Order };
		};
		std::ranges::stable_sort(matches, [&key](const scored& aLeft, const scored& aRight) { return key(aLeft) < key(aRight); });
		auto best = std::optional<std::pair<std::int32_t, int>>{ };
		for (const auto& match : matches) {
			const auto& entry    = *match.Entry;
			const auto  page     = settings_page_title(entry.Page);
			const auto  location = entry.PageEntry ? QString{ }
				: entry.SectionEntry || entry.Section.isEmpty()
				? page
				: QStringLiteral("%1 · %2").arg(page, entry.Section);
			const auto& found    = match.Match;
			auto* const item     = new QListWidgetItem{ entry.Name, Results };
			item->setData(settings_search_location_role, location);
			if (found.Field == search_field::name) {
				item->setData(settings_search_name_spans_role, utf16_spans(entry.Name, found.Match.Spans, 0));
			} else if (found.Field == search_field::section) {
				item->setData(settings_search_location_spans_role, utf16_spans(entry.Section, found.Match.Spans, location.size() - entry.Section.size()));
			}
			item->setData(Qt::AccessibleTextRole, entry.PageEntry ? QStringLiteral("%1, settings page").arg(entry.Name) : QStringLiteral("%1, %2").arg(entry.Name, location));
			auto tip = QStringList{ entry.Name };
			for (const auto& line : { location, entry.Description }) {
				if (!line.isEmpty()) {
					tip.push_back(line);
				}
			}
			item->setToolTip(tip.join(QLatin1Char('\n')));
			const auto row = static_cast<int>(Hits.size());
			if (!best.has_value() || found.Match.Score > best->first) {
				best = std::pair{ found.Match.Score, row };
			}
			Hits.push_back({ .Page = entry.Page, .Name = entry.Name, .Location = location, .Target = entry.Target });
		}
		Sidebar->hide();
		Results->setVisible(!Hits.empty());
		SearchNotice->setVisible(Hits.empty());
		if (!best.has_value()) {
			SearchNotice->set_filter(query);
			return;
		}
		// The list keeps the pages' grouping; Return takes the best match.
		Results->setCurrentRow(best->second);
		Results->scrollToItem(Results->currentItem());
	}

	void settings_window::step_search_result(const int aDelta)
	{
		if (!Results->isVisible() || Results->count() == 0) {
			return;
		}
		Results->setCurrentRow(std::clamp(Results->currentRow() + aDelta, 0, Results->count() - 1));
		Results->scrollToItem(Results->currentItem());
	}

	void settings_window::refresh_search_icon()
	{
		if (SearchIcon != nullptr) {
			SearchIcon->setIcon(magnifier_icon(palette().color(QPalette::Active, QPalette::PlaceholderText)));
		}
	}

	void settings_window::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange) {
			refresh_search_icon();
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
		// Bring it into view and give it focus; choosing changes no setting. A
		// setting that cannot take the keyboard now (unavailable, or a
		// section's heading) is shown, and its page takes the keyboard.
		if (hit.Target != nullptr && hit.Target->isVisibleTo(scroll->widget())) {
			scroll->ensureWidgetVisible(hit.Target, 24, 24);
			if (hit.Target->isEnabled() && hit.Target->focusPolicy() != Qt::NoFocus) {
				hit.Target->setFocus(Qt::OtherFocusReason);
				return;
			}
		} else {
			scroll->verticalScrollBar()->setValue(0);
		}
		scroll->setFocus(Qt::OtherFocusReason);
	}

	auto settings_window::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aEvent->type() != QEvent::KeyPress || (aWatched != Search && aWatched != Results)) {
			return QWidget::eventFilter(aWatched, aEvent);
		}
		const auto* const key = static_cast<QKeyEvent*>(aEvent);
		if (aWatched == Results) {
			// Escape from the results returns to the query, which it keeps.
			if (key->key() == Qt::Key_Escape) {
				Search->setFocus(Qt::OtherFocusReason);
				return true;
			}
			return QWidget::eventFilter(aWatched, aEvent);
		}
		switch (key->key()) {
		case Qt::Key_Escape:
			// Escape clears a search; an empty field lets it close the window.
			if (Search->text().isEmpty()) {
				break;
			}
			Search->clear();
			return true;
		case Qt::Key_Down:
		case Qt::Key_Up:
			// The caret stays in the field while the arrows choose a result.
			if (!Results->isVisible()) {
				break;
			}
			step_search_result(key->key() == Qt::Key_Down ? 1 : -1);
			return true;
		case Qt::Key_Return:
		case Qt::Key_Enter:
			if (!Results->isVisible() || Results->currentRow() < 0) {
				break;
			}
			go_to_search_result(Results->currentRow());
			return true;
		default:
			break;
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
