#include "bpr/qt/keyboard_shortcuts_window.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace bpr::qt {
	namespace {
		enum role : int {
			kind_role    = Qt::UserRole,
			command_role = Qt::UserRole + 1,
			note_role    = Qt::UserRole + 2,
			keys_role    = Qt::UserRole + 3,
			search_role  = Qt::UserRole + 4,
		};

		enum class row_kind : int {
			group,
			command,
		};

		/// \brief The words a row adds under its title: where the key acts,
		/// and whether a text field takes it while you type.
		[[nodiscard]] auto row_note(const shortcut_entry& aEntry) -> QString
		{
			auto parts = QStringList{ };
			if (!aEntry.Place.empty()) {
				parts.push_back(QString::fromUtf8(aEntry.Place.data(), static_cast<qsizetype>(aEntry.Place.size())));
			}
			if (steps_aside_while_typing(aEntry)) {
				parts.push_back(parts.isEmpty() ? QStringLiteral("Not while typing") : QStringLiteral("not while typing"));
			}
			return parts.join(QStringLiteral(", "));
		}

		/// \brief The keys a row shows: all of them, or the first `Shown`.
		[[nodiscard]] auto shown_keys(const shortcut_entry& aEntry) -> QList<QKeySequence>
		{
			auto keys = shortcut_keys(aEntry);
			if (aEntry.Shown != 0U && keys.size() > aEntry.Shown) {
				keys.resize(aEntry.Shown);
			}
			return keys;
		}

		/// \brief `aTo` over `aFrom` at `aAmount`: a key cap's tints come from
		/// the list's own base and text, so a cap reads as raised in a light,
		/// dark or high-contrast theme alike.
		[[nodiscard]] auto blend(const QColor& aFrom, const QColor& aTo, const float aAmount) -> QColor
		{
			const auto mix = [aAmount](const float aA, const float aB) { return aA + (aB - aA) * aAmount; };
			return QColor::fromRgbF(mix(aFrom.redF(), aTo.redF()), mix(aFrom.greenF(), aTo.greenF()), mix(aFrom.blueF(), aTo.blueF()));
		}

		/// \brief A row's note: the row's font, a step smaller.
		[[nodiscard]] auto note_font(const QFont& aFont) -> QFont
		{
			auto note = aFont;
			if (note.pointSizeF() > 0.0) {
				note.setPointSizeF(note.pointSizeF() * 0.9);
			} else if (note.pixelSize() > 0) {
				note.setPixelSize(std::max(1, static_cast<int>(std::lround(note.pixelSize() * 0.9))));
			}
			return note;
		}

		/// \brief Spacing derived from the font, so every scale and font size
		/// keeps the same proportions.
		struct row_metrics
		{
			explicit row_metrics(const QFont& aFont)
				: Metrics{ aFont }
				, Note{ note_font(aFont) }
				, Line{ Metrics.height() }
				, Margin{ std::max(6, Metrics.horizontalAdvance(QLatin1Char{ ' ' }) * 2) }
				, Padding{ std::max(3, Line / 4) }
				, CapPaddingX{ std::max(5, static_cast<int>(std::lround(Line * 0.45))) }
				, CapPaddingY{ std::max(1, static_cast<int>(std::lround(Line * 0.12))) }
				, CapGap{ std::max(4, Metrics.horizontalAdvance(QLatin1Char{ ' ' })) }
			{
			}

			[[nodiscard]] auto cap_height() const -> int { return Line + 2 * CapPaddingY + 1; }
			[[nodiscard]] auto cap_width(const QString& aText) const -> int { return Metrics.horizontalAdvance(aText) + 2 * CapPaddingX; }

			QFontMetrics Metrics;
			QFontMetrics Note;
			int          Line;
			int          Margin;
			int          Padding;
			int          CapPaddingX;
			int          CapPaddingY;
			int          CapGap;
		};

		/// \brief Draws a group's heading, a command's title and place, and
		/// its keys as key caps, all from the style option's palette and font.
		class shortcut_delegate final : public QStyledItemDelegate
		{
		public:
			explicit shortcut_delegate(QTreeWidget* const aView)
				: QStyledItemDelegate{ aView }
				, View{ aView }
			{
			}

			/// \brief Reserves the title's wrap floor, every native key cap and a scrollbar.
			[[nodiscard]] auto minimum_width() const -> int
			{
				const auto metrics = row_metrics{ View->font() };
				auto       keys    = 0;
				for (int group = 0; group < View->topLevelItemCount(); ++group) {
					const auto* const heading = View->topLevelItem(group);
					for (int row = 0; row < heading->childCount(); ++row) {
						keys = std::max(keys, keys_width(metrics, heading->child(row)->data(1, keys_role).toStringList()));
					}
				}
				return metrics.Line * 8 + keys + 2 * metrics.Margin + 2 * View->frameWidth()
					+ View->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, View->verticalScrollBar());
			}

			void paint(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				auto option = aOption;
				initStyleOption(&option, aIndex);
				const auto metrics = row_metrics{ option.font };
				if (static_cast<row_kind>(aIndex.data(kind_role).toInt()) == row_kind::group) {
					auto bold = option.font;
					bold.setBold(true);
					aPainter->save();
					aPainter->setFont(bold);
					aPainter->setPen(option.palette.color(QPalette::Active, QPalette::WindowText));
					const auto area = option.rect.adjusted(metrics.Margin, 0, -metrics.Margin, -metrics.Padding);
					aPainter->drawText(area, Qt::AlignLeft | Qt::AlignBottom | Qt::TextSingleLine, option.text);
					aPainter->restore();
					return;
				}
				// The style's own row: hover, selection and focus.
				auto panel = option;
				panel.text.clear();
				panel.icon              = { };
				const auto* const style = option.widget != nullptr ? option.widget->style() : QApplication::style();
				style->drawControl(QStyle::CE_ItemViewItem, &panel, aPainter, option.widget);
				const auto selected = option.state.testFlag(QStyle::State_Selected);
				const auto group    = option.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive;
				aPainter->save();
				aPainter->setFont(option.font);
				if (aIndex.column() == 0) {
					paint_title(*aPainter, option, metrics, selected, group, aIndex);
				} else {
					paint_keys(*aPainter, option, metrics, selected, group, aIndex);
				}
				aPainter->restore();
			}

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				auto option = aOption;
				initStyleOption(&option, aIndex);
				const auto metrics = row_metrics{ option.font };
				if (static_cast<row_kind>(aIndex.data(kind_role).toInt()) == row_kind::group) {
					auto bold = option.font;
					bold.setBold(true);
					return { QFontMetrics{ bold }.horizontalAdvance(option.text) + 2 * metrics.Margin, metrics.Line * 2 + metrics.Padding };
				}
				const auto minimum = metrics.cap_height() + 2 * metrics.Padding;
				if (aIndex.column() == 0) {
					// Measured as if the vertical scroll bar were showing, so a
					// scroll bar that appears never leaves a title a line short.
					auto available = View->columnWidth(0) - 2 * metrics.Margin;
					if (!View->verticalScrollBar()->isVisible()) {
						available -= View->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, View->verticalScrollBar());
					}
					const auto width  = std::max(metrics.Line * 6, available);
					const auto title  = metrics.Metrics.boundingRect(QRect{ 0, 0, width, 100'000 }, Qt::TextWordWrap, option.text);
					auto       height = title.height();
					if (const auto note = aIndex.data(note_role).toString(); !note.isEmpty()) {
						height += metrics.Note.boundingRect(QRect{ 0, 0, width, 100'000 }, Qt::TextWordWrap, note).height();
					}
					return { metrics.Line * 8, std::max(minimum, height + 2 * metrics.Padding) };
				}
				return { keys_width(metrics, aIndex.data(keys_role).toStringList()) + 2 * metrics.Margin, minimum };
			}

		private:
			void paint_title(QPainter& aPainter, const QStyleOptionViewItem& aOption, const row_metrics& aMetrics,
							 const bool aSelected, const QPalette::ColorGroup aGroup, const QModelIndex& aIndex) const
			{
				auto       area  = aOption.rect.adjusted(aMetrics.Margin, aMetrics.Padding, -aMetrics.Margin, -aMetrics.Padding);
				const auto note  = aIndex.data(note_role).toString();
				const auto title = aMetrics.Metrics.boundingRect(area, Qt::TextWordWrap, aOption.text);
				const auto total = title.height() + (note.isEmpty() ? 0 : aMetrics.Note.boundingRect(area, Qt::TextWordWrap, note).height());
				area.setTop(area.top() + std::max(0, (area.height() - total) / 2));
				aPainter.setPen(aOption.palette.color(aGroup, aSelected ? QPalette::HighlightedText : QPalette::Text));
				aPainter.drawText(area, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, aOption.text);
				if (!note.isEmpty()) {
					area.setTop(area.top() + title.height());
					auto ink = aOption.palette.color(aGroup, aSelected ? QPalette::HighlightedText : QPalette::PlaceholderText);
					if (aSelected) {
						ink.setAlphaF(0.8F);
					}
					aPainter.setPen(ink);
					aPainter.setFont(note_font(aOption.font));
					aPainter.drawText(area, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, note);
				}
			}

			/// \brief Each key a cap, right-aligned, with "or" between caps.
			void paint_keys(QPainter& aPainter, const QStyleOptionViewItem& aOption, const row_metrics& aMetrics,
							const bool aSelected, const QPalette::ColorGroup aGroup, const QModelIndex& aIndex) const
			{
				const auto keys      = aIndex.data(keys_role).toStringList();
				const auto height    = aMetrics.cap_height();
				auto       x         = aOption.rect.right() - aMetrics.Margin - keys_width(aMetrics, keys) + 1;
				const auto y         = aOption.rect.top() + (aOption.rect.height() - height) / 2;
				const auto radius    = std::max(3.0, aMetrics.Line * 0.22);
				const auto base      = aOption.palette.color(aGroup, QPalette::Base);
				const auto ink       = aOption.palette.color(aGroup, QPalette::Text);
				const auto fill      = blend(base, ink, 0.07F);
				const auto edge      = blend(base, ink, 0.32F);
				const auto shadow    = blend(base, ink, 0.45F);
				const auto separator = QStringLiteral("or");
				aPainter.setRenderHint(QPainter::Antialiasing, true);
				for (qsizetype index = 0; index < keys.size(); ++index) {
					if (index > 0) {
						aPainter.setPen(aOption.palette.color(aGroup, aSelected ? QPalette::HighlightedText : QPalette::PlaceholderText));
						const auto word = aMetrics.Metrics.horizontalAdvance(separator);
						aPainter.drawText(QRect{ x + aMetrics.CapGap, y, word, height - 1 }, Qt::AlignCenter, separator);
						x += word + 2 * aMetrics.CapGap;
					}
					const auto width = aMetrics.cap_width(keys[index]);
					// The cap's shadow edge, one pixel lower, then the cap.
					const auto cap = QRectF{ x + 0.5, y + 0.5, width - 1.0, height - 2.0 };
					aPainter.setPen(Qt::NoPen);
					aPainter.setBrush(shadow);
					aPainter.drawRoundedRect(cap.translated(0.0, 1.0), radius, radius);
					aPainter.setBrush(fill);
					aPainter.setPen(QPen{ edge, 1.0 });
					aPainter.drawRoundedRect(cap, radius, radius);
					aPainter.setPen(ink);
					aPainter.drawText(cap.toRect(), Qt::AlignCenter | Qt::TextSingleLine, keys[index]);
					x += width;
				}
			}

			[[nodiscard]] static auto keys_width(const row_metrics& aMetrics, const QStringList& aKeys) -> int
			{
				auto width = 0;
				for (qsizetype index = 0; index < aKeys.size(); ++index) {
					if (index > 0) {
						width += aMetrics.Metrics.horizontalAdvance(QStringLiteral("or")) + 2 * aMetrics.CapGap;
					}
					width += aMetrics.cap_width(aKeys[index]);
				}
				return width;
			}

			QTreeWidget* View;
		};

		[[nodiscard]] auto footer_text() -> QString
		{
			auto text = QStringLiteral("Keys marked “not while typing” are left to the field or editor you are typing in.");
#if defined(Q_OS_MACOS)
			text += QStringLiteral(" On macOS, Option with a letter, digit or symbol types a character there (⌥R types ®), so those commands wait until you leave the field.");
#endif
			return text;
		}
	}

	keyboard_shortcuts_window::keyboard_shortcuts_window(QWidget* const aParent)
		: QDialog{ aParent }
	{
		setObjectName(QStringLiteral("keyboardShortcutsWindow"));
		setWindowTitle(QStringLiteral("Keyboard Shortcuts"));
		setAccessibleName(QStringLiteral("Keyboard Shortcuts"));
		setAccessibleDescription(QStringLiteral("Every keyboard shortcut, grouped by area. Type to search."));
		setSizeGripEnabled(true);

		auto* const column = new QVBoxLayout{ this };
		Search             = new QLineEdit{ this };
		Search->setObjectName(QStringLiteral("keyboardShortcutsSearch"));
		Search->setPlaceholderText(QStringLiteral("Search by command, place or key"));
		Search->setClearButtonEnabled(true);
		Search->setAccessibleName(QStringLiteral("Search shortcuts"));
		Search->setAccessibleDescription(QStringLiteral("Shows only the shortcuts whose command, place or keys contain every word typed. Down Arrow moves to the list."));
		Search->installEventFilter(this);
		column->addWidget(Search);

		List = new QTreeWidget{ this };
		List->setObjectName(QStringLiteral("keyboardShortcutsList"));
		List->setAccessibleName(QStringLiteral("Keyboard shortcuts"));
		List->setAccessibleDescription(QStringLiteral("Each command and its keys, grouped by area"));
		List->setColumnCount(2);
		List->setHeaderLabels({ QStringLiteral("Command"), QStringLiteral("Keys") });
		List->setHeaderHidden(true);
		List->setRootIsDecorated(false);
		List->setItemsExpandable(false);
		List->setIndentation(0);
		List->setUniformRowHeights(false);
		List->setWordWrap(true);
		List->setSelectionMode(QAbstractItemView::SingleSelection);
		List->setEditTriggers(QAbstractItemView::NoEditTriggers);
		List->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
		List->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		List->setFocusPolicy(Qt::StrongFocus);
		List->setItemDelegate(new shortcut_delegate{ List });
		List->header()->setStretchLastSection(false);
		List->header()->setSectionResizeMode(0, QHeaderView::Stretch);
		List->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
		column->addWidget(List, 1);

		Empty = new QLabel{ this };
		Empty->setObjectName(QStringLiteral("keyboardShortcutsEmpty"));
		Empty->setAlignment(Qt::AlignCenter);
		Empty->setWordWrap(true);
		Empty->setForegroundRole(QPalette::PlaceholderText);
		Empty->setTextFormat(Qt::PlainText);
		Empty->hide();
		column->addWidget(Empty, 1);

		Footer = new QLabel{ footer_text(), this };
		Footer->setObjectName(QStringLiteral("keyboardShortcutsFooter"));
		Footer->setWordWrap(true);
		Footer->setTextFormat(Qt::PlainText);
		Footer->setForegroundRole(QPalette::PlaceholderText);
		Footer->setAccessibleName(QStringLiteral("About keys while typing"));
		column->addWidget(Footer);

		auto* const buttons = new QDialogButtonBox{ QDialogButtonBox::Close, this };
		buttons->setObjectName(QStringLiteral("keyboardShortcutsButtons"));
		buttons->button(QDialogButtonBox::Close)->setAutoDefault(false);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		column->addWidget(buttons);

		setTabOrder(Search, List);
		setTabOrder(List, buttons->button(QDialogButtonBox::Close));
		populate();
		connect(Search, &QLineEdit::textChanged, this, [this](const QString& aText) { apply_search(aText); });

		// Sized from the font, so it opens alike at every scale: about forty
		// rows tall, within the screen it opens on.
		const auto metrics = QFontMetrics{ font() };
		const auto em      = metrics.horizontalAdvance(QLatin1Char{ 'M' });
		update_minimum_size();
		auto wanted = QSize{ em * 42, metrics.height() * 44 };
		if (const auto* const screen = aParent != nullptr ? aParent->screen() : QGuiApplication::primaryScreen(); screen != nullptr) {
			wanted = wanted.boundedTo(screen->availableSize() * 9 / 10).expandedTo(minimumSize());
		}
		resize(wanted);
	}

	auto keyboard_shortcuts_window::search_field() const noexcept -> QLineEdit* { return Search; }
	auto keyboard_shortcuts_window::list() const noexcept -> QTreeWidget* { return List; }
	auto keyboard_shortcuts_window::empty_label() const noexcept -> QLabel* { return Empty; }

	auto keyboard_shortcuts_window::shown_commands() const -> std::vector<shortcut_command>
	{
		auto commands = std::vector<shortcut_command>{ };
		for (int group = 0; group < List->topLevelItemCount(); ++group) {
			const auto* const heading = List->topLevelItem(group);
			for (int row = 0; row < heading->childCount(); ++row) {
				if (!heading->isHidden() && !heading->child(row)->isHidden()) {
					commands.push_back(static_cast<shortcut_command>(heading->child(row)->data(0, command_role).toInt()));
				}
			}
		}
		return commands;
	}

	auto keyboard_shortcuts_window::shown_groups() const -> std::vector<shortcut_group>
	{
		auto groups = std::vector<shortcut_group>{ };
		for (int group = 0; group < List->topLevelItemCount(); ++group) {
			if (!List->topLevelItem(group)->isHidden()) {
				groups.push_back(static_cast<shortcut_group>(List->topLevelItem(group)->data(0, command_role).toInt()));
			}
		}
		return groups;
	}

	void keyboard_shortcuts_window::populate()
	{
		auto* heading = static_cast<QTreeWidgetItem*>(nullptr);
		auto  current = shortcut_group{ };
		for (const auto& entry : shortcut_catalogue()) {
			const auto keys = shown_keys(entry);
			if (keys.isEmpty()) {
				// No key on this platform (Quit on Windows): nothing to list.
				continue;
			}
			if (heading == nullptr || entry.Group != current) {
				current = entry.Group;
				heading = new QTreeWidgetItem{ List };
				heading->setText(0, shortcut_group_title(entry.Group));
				heading->setData(0, kind_role, static_cast<int>(row_kind::group));
				heading->setData(0, command_role, static_cast<int>(entry.Group));
				heading->setData(0, Qt::AccessibleTextRole, shortcut_group_title(entry.Group));
				heading->setFlags(Qt::ItemIsEnabled);
				heading->setFirstColumnSpanned(true);
			}
			auto*      row    = new QTreeWidgetItem{ heading };
			const auto title  = QString::fromUtf8(entry.Title.data(), static_cast<qsizetype>(entry.Title.size()));
			const auto note   = row_note(entry);
			auto       native = QStringList{ };
			auto       spoken = QStringList{ };
			auto       search = QStringList{ shortcut_group_title(entry.Group), title, note };
			for (const auto& key : keys) {
				native.push_back(key.toString(QKeySequence::NativeText));
				spoken.push_back(spoken_key(key));
				search.push_back(native.back());
				search.push_back(spoken.back());
				search.push_back(key.toString(QKeySequence::PortableText));
			}
			row->setText(0, title);
			row->setText(1, native.join(QStringLiteral(" or ")));
			row->setData(0, kind_role, static_cast<int>(row_kind::command));
			row->setData(1, kind_role, static_cast<int>(row_kind::command));
			row->setData(0, command_role, static_cast<int>(entry.Command));
			row->setData(0, note_role, note);
			row->setData(1, keys_role, native);
			row->setData(0, search_role, search.join(QLatin1Char{ '\n' }).toCaseFolded());
			row->setData(0, Qt::AccessibleTextRole, note.isEmpty() ? title : QStringLiteral("%1, %2").arg(title, note));
			row->setData(1, Qt::AccessibleTextRole, spoken.join(QStringLiteral(" or ")));
			row->setToolTip(0, note.isEmpty() ? title : QStringLiteral("%1\n%2").arg(title, note));
			row->setToolTip(1, spoken.join(QStringLiteral(" or ")));
			row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		}
		List->expandAll();
	}

	void keyboard_shortcuts_window::apply_search(const QString& aText)
	{
		const auto words = aText.toCaseFolded().split(QLatin1Char{ ' ' }, Qt::SkipEmptyParts);
		auto       shown = 0;
		for (int group = 0; group < List->topLevelItemCount(); ++group) {
			auto* const heading = List->topLevelItem(group);
			auto        visible = 0;
			for (int row = 0; row < heading->childCount(); ++row) {
				auto* const item     = heading->child(row);
				const auto  haystack = item->data(0, search_role).toString();
				const auto  match    = std::ranges::all_of(words, [&haystack](const QString& aWord) { return haystack.contains(aWord); });
				item->setHidden(!match);
				visible += match ? 1 : 0;
			}
			heading->setHidden(visible == 0);
			shown += visible;
		}
		Empty->setText(QStringLiteral("No shortcut matches “%1”.").arg(aText.trimmed()));
		Empty->setVisible(shown == 0);
		List->setVisible(shown != 0);
		if (shown == 0) {
			Empty->setAccessibleName(Empty->text());
		}
	}

	void keyboard_shortcuts_window::refresh_layout()
	{
		// Coalesced: a resize drag or a theme change measures the rows once.
		if (LayoutPending) {
			return;
		}
		LayoutPending = true;
		QTimer::singleShot(0, this, [this] {
			LayoutPending = false;
			update_minimum_size();
			List->doItemsLayout();
		});
	}

	void keyboard_shortcuts_window::update_minimum_size()
	{
		const auto  metrics  = QFontMetrics{ font() };
		const auto  margins  = layout()->contentsMargins();
		const auto* delegate = static_cast<shortcut_delegate*>(List->itemDelegate());
		setMinimumSize(std::max(metrics.horizontalAdvance(QLatin1Char{ 'M' }) * 24, margins.left() + margins.right() + delegate->minimum_width()), metrics.height() * 16);
	}

	void keyboard_shortcuts_window::resizeEvent(QResizeEvent* const aEvent)
	{
		QDialog::resizeEvent(aEvent);
		// A wrapped title's height follows its column's width.
		refresh_layout();
	}

	void keyboard_shortcuts_window::changeEvent(QEvent* const aEvent)
	{
		QDialog::changeEvent(aEvent);
		switch (aEvent->type()) {
		case QEvent::FontChange:
		case QEvent::PaletteChange:
		case QEvent::StyleChange:
		case QEvent::ApplicationFontChange:
		case QEvent::ApplicationPaletteChange:
			if (List != nullptr) {
				refresh_layout();
			}
			break;
		default:
			break;
		}
	}

	void keyboard_shortcuts_window::showEvent(QShowEvent* const aEvent)
	{
		QDialog::showEvent(aEvent);
		Search->setFocus(Qt::OtherFocusReason);
		Search->selectAll();
	}

	auto keyboard_shortcuts_window::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		// Down Arrow or Return in the search field moves to the first row shown.
		if (aWatched == Search && aEvent->type() == QEvent::KeyPress) {
			const auto key = static_cast<QKeyEvent*>(aEvent)->key();
			if ((key == Qt::Key_Down || key == Qt::Key_Return || key == Qt::Key_Enter) && List->isVisible()) {
				for (int group = 0; group < List->topLevelItemCount(); ++group) {
					const auto* const heading = List->topLevelItem(group);
					for (int row = 0; !heading->isHidden() && row < heading->childCount(); ++row) {
						if (!heading->child(row)->isHidden()) {
							List->setCurrentItem(heading->child(row));
							List->setFocus(Qt::TabFocusReason);
							return true;
						}
					}
				}
			}
		}
		return QDialog::eventFilter(aWatched, aEvent);
	}
}
