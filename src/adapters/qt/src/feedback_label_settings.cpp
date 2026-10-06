#include "bpr/qt/feedback_label_settings.hpp"

#include "bpr/qt/anchored_tooltip.hpp"

#include <QApplication>
#include <QDropEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPainter>
#include <QResizeEvent>
#include <QStringList>
#include <QStyle>
#include <QStyledItemDelegate>

#include <algorithm>
#include <optional>
#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief The gutter left of every row, where a ticked row's grip sits.
		constexpr int GripGutter = 14;
		/// \brief The rule row's height.
		constexpr int SeparatorHeight = 9;

		[[nodiscard]] auto label_of(const QModelIndex& aIndex) -> std::optional<core::feedback_label>
		{
			const auto value = aIndex.data(feedback_label_columns_editor::label_role);
			if (!value.isValid()) {
				return std::nullopt;
			}
			const auto index = value.toInt();
			if (index < 0 || static_cast<std::size_t>(index) >= core::feedback_label_count) {
				return std::nullopt;
			}
			return static_cast<core::feedback_label>(index);
		}

		/// \brief Paints the rule, the grip, and each row's description after
		/// its ID; the check, mark, ID, selection and focus are the style's.
		class columns_delegate final : public QStyledItemDelegate
		{
		public:
			explicit columns_delegate(feedback_label_columns_editor& aEditor)
				: QStyledItemDelegate{ &aEditor }
				, Editor{ aEditor }
			{ }

			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				if (aIndex.data(feedback_label_columns_editor::separator_role).toBool()) {
					aPainter->save();
					aPainter->setPen(QPen{ aOption.palette.color(QPalette::Mid), 1.0 });
					const auto y = aOption.rect.center().y();
					aPainter->drawLine(aOption.rect.left() + 6, y, aOption.rect.right() - 6, y);
					aPainter->restore();
					return;
				}
				auto option = aOption;
				option.rect.setLeft(option.rect.left() + GripGutter);
				QStyledItemDelegate::paint(aPainter, option, aIndex);
				const auto ticked = aIndex.data(Qt::CheckStateRole).toInt() == Qt::Checked;
				aPainter->save();
				aPainter->setRenderHint(QPainter::Antialiasing, true);
				const auto selected = (aOption.state & QStyle::State_Selected) != 0;
				const auto quiet    = selected ? aOption.palette.color(QPalette::HighlightedText) : aOption.palette.color(QPalette::PlaceholderText);
				if (ticked) {
					// Two columns of three dots: the grip a ticked row is dragged by.
					aPainter->setPen(Qt::NoPen);
					aPainter->setBrush(quiet);
					const auto centre = QPointF{ aOption.rect.left() + (GripGutter / 2.0), aOption.rect.center().y() + 0.5 };
					for (const auto dx : { -2.0, 2.0 }) {
						for (const auto dy : { -4.0, 0.0, 4.0 }) {
							aPainter->drawEllipse(centre + QPointF{ dx, dy }, 1.0, 1.0);
						}
					}
				}
				if (const auto label = label_of(aIndex); label.has_value() && Editor.descriptions_shown()) {
					auto textOption = option;
					initStyleOption(&textOption, aIndex);
					const auto* const style    = Editor.style();
					const auto        textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &textOption, &Editor);
					const auto        metrics  = QFontMetrics{ textOption.font };
					const auto        left     = textRect.left() + metrics.horizontalAdvance(textOption.text) + 12;
					const auto        room     = QRect{ left, textRect.top(), std::max(0, aOption.rect.right() - 6 - left), textRect.height() };
					if (room.width() > metrics.averageCharWidth() * 4) {
						aPainter->setPen(quiet);
						aPainter->setFont(textOption.font);
						aPainter->drawText(room, Qt::AlignVCenter | Qt::AlignLeft, metrics.elidedText(feedback_label_description(*label), Qt::ElideRight, room.width()));
					}
				}
				aPainter->restore();
			}

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				if (aIndex.data(feedback_label_columns_editor::separator_role).toBool()) {
					return { aOption.rect.width(), SeparatorHeight };
				}
				auto size = QStyledItemDelegate::sizeHint(aOption, aIndex);
				size.rwidth() += GripGutter;
				size.setHeight(std::max(size.height(), 22));
				return size;
			}

			auto editorEvent(QEvent* aEvent, QAbstractItemModel* aModel, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> bool override
			{
				// The check box sits where `paint` put it, one gutter in.
				auto option = aOption;
				option.rect.setLeft(option.rect.left() + GripGutter);
				return QStyledItemDelegate::editorEvent(aEvent, aModel, option, aIndex);
			}

		private:
			feedback_label_columns_editor& Editor;
		};
	}

	feedback_label_columns_editor::feedback_label_columns_editor(QWidget* const aParent)
		: QListWidget{ aParent }
	{
		setObjectName(QStringLiteral("feedbackLabelColumns"));
		setAccessibleName(QStringLiteral("Overview chips"));
		setAccessibleDescription(QStringLiteral("Ticked labels get their own count in the Overview's Comments column, in this order. Alt+Up and Alt+Down move the current one."));
		setItemDelegate(new columns_delegate{ *this });
		setSelectionMode(QAbstractItemView::SingleSelection);
		setDragDropMode(QAbstractItemView::InternalMove);
		setDefaultDropAction(Qt::MoveAction);
		setDragEnabled(true);
		setUniformItemSizes(false);
		// The list is as tall as its thirteen rows, so the page scrolls and
		// the list never does (no nested scrolling, RFC-BPR-0050).
		setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		Theme = default_feedback_mark_theme(palette());
		for (std::size_t index = 0U; index < Rows.size(); ++index) {
			auto* const row = new QListWidgetItem{ feedback_label_text(static_cast<core::feedback_label>(index)) };
			row->setData(label_role, static_cast<int>(index));
			row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
			row->setCheckState(Qt::Unchecked);
			Rows[index] = row;
		}
		Separator = new QListWidgetItem;
		Separator->setData(separator_role, true);
		Separator->setFlags(Qt::NoItemFlags);
		Separator->setData(Qt::AccessibleTextRole, QString{ });
		arrange(Shown);
		connect(this, &QListWidget::itemChanged, this, [this](QListWidgetItem* const aItem) {
			if (Applying || aItem == nullptr || aItem == Separator) {
				return;
			}
			report();
		});
		anchor_tooltips(*viewport(), [this](const QPoint aPosition) -> std::optional<anchored_tooltip_target> {
			const auto* const item = itemAt(aPosition);
			if (item == nullptr || item == Separator) {
				return std::nullopt;
			}
			return anchored_tooltip_target{ .Anchor = visualItemRect(item), .Text = row_tooltip(*item) };
		});
	}

	void feedback_label_columns_editor::set_columns(const core::feedback_label_columns& aColumns)
	{
		if (!core::check_feedback_label_columns(aColumns).has_value()) {
			return;
		}
		arrange(aColumns);
	}

	auto feedback_label_columns_editor::columns() const -> core::feedback_label_columns
	{
		core::feedback_label_columns chosen;
		for (int position = 0; position < count(); ++position) {
			const auto* const row   = item(position);
			const auto        label = label_of(indexFromItem(row));
			if (label.has_value() && row->checkState() == Qt::Checked && chosen.Count < core::feedback_label_count) {
				chosen.Labels[chosen.Count] = *label;
				++chosen.Count;
			}
		}
		return chosen;
	}

	void feedback_label_columns_editor::set_theme(const feedback_mark_theme& aTheme)
	{
		Theme = aTheme;
		refresh_rows();
	}

	auto feedback_label_columns_editor::theme() const noexcept -> const feedback_mark_theme& { return Theme; }

	void feedback_label_columns_editor::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	auto feedback_label_columns_editor::item_of(const core::feedback_label aLabel) const -> QListWidgetItem*
	{
		const auto index = static_cast<std::size_t>(aLabel);
		return index < Rows.size() ? Rows[index] : nullptr;
	}

	auto feedback_label_columns_editor::separator_item() const noexcept -> QListWidgetItem* { return Separator; }

	auto feedback_label_columns_editor::move_current(const int aStep) -> bool
	{
		const auto* const current = currentItem();
		const auto        label   = current == nullptr ? std::nullopt : label_of(indexFromItem(current));
		if (!label.has_value() || current->checkState() != Qt::Checked || (aStep != -1 && aStep != 1)) {
			return false;
		}
		auto       chosen = columns();
		auto       labels = std::span{ chosen.Labels }.first(chosen.Count);
		const auto found  = std::ranges::find(labels, *label);
		const auto at     = static_cast<int>(std::distance(labels.begin(), found));
		const auto to     = at + aStep;
		if (found == labels.end() || to < 0 || to >= static_cast<int>(labels.size())) {
			return false;
		}
		std::swap(labels[static_cast<std::size_t>(at)], labels[static_cast<std::size_t>(to)]);
		arrange(chosen);
		if (Dispatch) {
			Dispatch(chosen);
		}
		return true;
	}

	auto feedback_label_columns_editor::descriptions_shown() const noexcept -> bool
	{
		return width() >= description_width;
	}

	auto feedback_label_columns_editor::row_tooltip(const QListWidgetItem& aItem) const -> QString
	{
		const auto label = label_of(indexFromItem(&aItem));
		if (!label.has_value()) {
			return { };
		}
		const auto description = feedback_label_description(*label);
		return aItem.checkState() == Qt::Checked ? description + QLatin1Char{ '\n' } + reorder_hint() : description;
	}

	auto feedback_label_columns_editor::reorder_hint() -> QString
	{
		return QStringLiteral("Drag to reorder (%1 %2)")
			.arg(QKeySequence{ Qt::ALT | Qt::Key_Up }.toString(QKeySequence::NativeText),
				 QKeySequence{ Qt::ALT | Qt::Key_Down }.toString(QKeySequence::NativeText));
	}

	auto feedback_label_columns_editor::sizeHint() const -> QSize
	{
		auto height = 2 * frameWidth();
		for (int position = 0; position < count(); ++position) {
			if (!item(position)->isHidden()) {
				height += sizeHintForRow(position);
			}
		}
		return { 380, height };
	}

	auto feedback_label_columns_editor::minimumSizeHint() const -> QSize
	{
		return { 220, sizeHint().height() };
	}

	void feedback_label_columns_editor::keyPressEvent(QKeyEvent* const aEvent)
	{
		if ((aEvent->modifiers() & Qt::AltModifier) != 0 && (aEvent->key() == Qt::Key_Up || aEvent->key() == Qt::Key_Down)) {
			static_cast<void>(move_current(aEvent->key() == Qt::Key_Up ? -1 : 1));
			aEvent->accept();
			return;
		}
		QListWidget::keyPressEvent(aEvent);
	}

	void feedback_label_columns_editor::dropEvent(QDropEvent* const aEvent)
	{
		QListWidget::dropEvent(aEvent);
		// A dragged row lands where it was dropped; the ticked rows top to
		// bottom are the new order, and the rows then return to their
		// arrangement: ticked first, the rule, the rest in catalog order.
		const auto chosen = columns();
		report();
		QMetaObject::invokeMethod(this, [this, chosen] { arrange(chosen); }, Qt::QueuedConnection);
	}

	void feedback_label_columns_editor::changeEvent(QEvent* const aEvent)
	{
		QListWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			refresh_rows();
		}
	}

	void feedback_label_columns_editor::resizeEvent(QResizeEvent* const aEvent)
	{
		QListWidget::resizeEvent(aEvent);
		if ((aEvent->oldSize().width() >= description_width) != (aEvent->size().width() >= description_width)) {
			viewport()->update();
		}
	}

	void feedback_label_columns_editor::arrange(const core::feedback_label_columns& aColumns)
	{
		const auto  chosen  = core::labels_of(aColumns);
		auto        order   = std::vector<QListWidgetItem*>{ };
		auto* const current = currentItem();
		for (const auto label : chosen) {
			order.push_back(Rows[static_cast<std::size_t>(label)]);
		}
		order.push_back(Separator);
		for (std::size_t index = 0U; index < Rows.size(); ++index) {
			if (std::ranges::find(chosen, static_cast<core::feedback_label>(index)) == chosen.end()) {
				order.push_back(Rows[index]);
			}
		}
		Applying       = true;
		auto unchanged = count() == static_cast<int>(order.size());
		for (int position = 0; unchanged && position < count(); ++position) {
			unchanged = item(position) == order[static_cast<std::size_t>(position)];
		}
		if (!unchanged) {
			while (count() > 0) {
				static_cast<void>(takeItem(0));
			}
			for (auto* const row : order) {
				addItem(row);
			}
		}
		for (std::size_t index = 0U; index < Rows.size(); ++index) {
			const auto ticked = std::ranges::find(chosen, static_cast<core::feedback_label>(index)) != chosen.end();
			Rows[index]->setCheckState(ticked ? Qt::Checked : Qt::Unchecked);
		}
		Separator->setHidden(chosen.empty() || chosen.size() == core::feedback_label_count);
		Shown = aColumns;
		if (current != nullptr) {
			setCurrentItem(current, QItemSelectionModel::NoUpdate);
		}
		Applying = false;
		refresh_rows();
		fit_height();
	}

	void feedback_label_columns_editor::refresh_rows()
	{
		const auto ratio  = devicePixelRatioF();
		const auto chosen = core::labels_of(Shown);
		Applying          = true;
		for (std::size_t index = 0U; index < Rows.size(); ++index) {
			const auto label = static_cast<core::feedback_label>(index);
			auto*      row   = Rows[index];
			row->setIcon(QIcon{ feedback_mark_pixmap(label, feedback_mark_emphasis{ }, feedback_mark_colours_for(Theme, label, palette()), ratio) });
			const auto at = std::ranges::find(chosen, label);
			row->setData(Qt::AccessibleDescriptionRole,
						 at == chosen.end()
							 ? QStringLiteral("Not shown. %1").arg(feedback_label_description(label))
							 : QStringLiteral("Chip %1 of %2. %3").arg(std::distance(chosen.begin(), at) + 1).arg(chosen.size()).arg(feedback_label_description(label)));
		}
		Applying = false;
		viewport()->update();
	}

	void feedback_label_columns_editor::report()
	{
		const auto chosen = columns();
		if (chosen == Shown) {
			return;
		}
		Shown = chosen;
		refresh_rows();
		// A newly ticked row moves up beside the others once this gesture is
		// done; arranging now would move rows under the view's own handler.
		QMetaObject::invokeMethod(this, [this, chosen] {
			if (Shown == chosen) {
				arrange(chosen);
			} }, Qt::QueuedConnection);
		if (Dispatch) {
			Dispatch(chosen);
		}
	}

	void feedback_label_columns_editor::fit_height()
	{
		setFixedHeight(sizeHint().height());
	}

	auto feedback_label_columns_recovery_text(const std::uint8_t aUnknown, const std::uint8_t aRepeated) -> QString
	{
		auto parts = QStringList{ };
		if (aUnknown != 0U) {
			parts << (aUnknown == 1U ? QStringLiteral("1 unknown label") : QStringLiteral("%1 unknown labels").arg(aUnknown));
		}
		if (aRepeated != 0U) {
			parts << (aRepeated == 1U ? QStringLiteral("1 repeat") : QStringLiteral("%1 repeats").arg(aRepeated));
		}
		return QStringLiteral("Some saved chips were left out: %1. The rest keep their order. Change a chip or Reset to save this list.")
			.arg(parts.join(QStringLiteral(" and ")));
	}
} // namespace bpr::qt
