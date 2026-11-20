#include "bpr/qt/argument_list_editor.hpp"

#include "list_row_handles.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief The rows shown before the list scrolls.
		constexpr int shown_rows = 5;

		[[nodiscard]] auto text_of(const std::string& aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		/// \brief One argument between its grip and its trash. Native selection
		/// and focus are drawn first, across the whole row; the words and the
		/// editor sit between the handles.
		class argument_delegate final : public QStyledItemDelegate
		{
		public:
			using QStyledItemDelegate::QStyledItemDelegate;

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				auto size = QStyledItemDelegate::sizeHint(aOption, aIndex);
				size.setHeight(std::max(size.height(), QFontMetrics{ aOption.font }.height() + 12));
				return size;
			}

			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				auto option = aOption;
				initStyleOption(&option, aIndex);
				const auto words = option.text;
				option.text.clear();
				const auto* const widget = option.widget;
				auto* const       style  = widget != nullptr ? widget->style() : QApplication::style();
				style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, aPainter, widget);
				if ((option.state & QStyle::State_HasFocus) != 0) {
					auto focus = QStyleOptionFocusRect{ };
					focus.QStyleOption::operator=(option);
					focus.backgroundColor = option.palette.color(QPalette::Base);
					style->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, aPainter, widget);
				}
				const auto selected = (option.state & QStyle::State_Selected) != 0;
				const auto room     = text_room(option.rect);
				aPainter->save();
				aPainter->setFont(option.font);
				aPainter->setPen(option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text));
				aPainter->drawText(room, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics{ option.font }.elidedText(words, Qt::ElideMiddle, room.width()));
				aPainter->restore();
			}

			void updateEditorGeometry(QWidget* aEditor, const QStyleOptionViewItem& aOption, const QModelIndex& /*aIndex*/) const override { aEditor->setGeometry(text_room(aOption.rect).adjusted(-3, 2, 0, -2)); }

		private:
			[[nodiscard]] static auto text_room(const QRect& aRow) -> QRect { return aRow.adjusted(detail::row_grip_gutter + 2, 0, -detail::row_trash_room, 0); }
		};
	} // namespace

	argument_list_editor::argument_list_editor(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("argumentListEditor"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(4);
		List = new QListWidget{ this };
		List->setObjectName(QStringLiteral("agentEntryArguments"));
		List->setAccessibleName(tr("Arguments"));
		List->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		List->setItemDelegate(new argument_delegate{ List });
		List->setSelectionMode(QAbstractItemView::SingleSelection);
		List->setDragDropMode(QAbstractItemView::InternalMove);
		List->setDefaultDropAction(Qt::MoveAction);
		List->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
		List->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		List->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		List->setUniformItemSizes(true);
		List->setTabKeyNavigation(false);
		// Five rows show; more scroll.
		const auto row = std::max(List->fontMetrics().height() + 12, 22);
		List->setFixedHeight(shown_rows * row + 2 * List->frameWidth());
		column->addWidget(List);
		Handles          = new detail::list_row_handles{ *List, { .Name = [this](const int aRow) {
																 const auto* const item = List->item(aRow);
																 return item == nullptr || item->text().trimmed().isEmpty() ? tr("argument %1").arg(aRow + 1) : item->text().trimmed(); }, .Move = [this](const int aRow, const int aDelta) { move(aRow, aDelta); }, .Remove = [this](const int aRow) {
															 delete List->takeItem(aRow);
															 changed(); }, .TrashName = QStringLiteral("agentEntryRemoveArgument") } };
		auto* const foot = new QHBoxLayout{ };
		foot->setSpacing(8);
		Add = new quiet_push_button{ tr("Add argument"), this };
		Add->setObjectName(QStringLiteral("agentEntryAddArgument"));
		foot->addWidget(Add);
		auto* const hint = new QLabel{ tr("Double-click to edit. Drag a grip, or press Alt+Up or Alt+Down, to reorder."), this };
		hint->setObjectName(QStringLiteral("agentEntryArgumentsHint"));
		hint->setWordWrap(true);
		hint->setForegroundRole(QPalette::PlaceholderText);
		foot->addWidget(hint, 1);
		column->addLayout(foot);
		connect(Add, &QPushButton::clicked, this, [this] { add_row(); });
		connect(List->model(), &QAbstractItemModel::rowsMoved, this, [this] { changed(); });
		connect(List, &QListWidget::itemChanged, this, [this] { changed(); });
		// An edit closed on an empty row, typed or not, removes the row once
		// the view is done with its editor.
		connect(List->itemDelegate(), &QAbstractItemDelegate::closeEditor, this, [this] { QTimer::singleShot(0, this, [this] { drop_empty(); }); });
	}

	void argument_list_editor::set_arguments(const std::vector<std::string>& aArguments)
	{
		const auto block = QSignalBlocker{ List };
		List->clear();
		for (const auto& argument : aArguments) {
			auto* const item = new QListWidgetItem{ text_of(argument), List };
			item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);
			item->setToolTip(item->text());
		}
		Handles->sync();
	}

	auto argument_list_editor::arguments() const -> std::vector<std::string>
	{
		auto result = std::vector<std::string>{ };
		for (auto row = 0; row < List->count(); ++row) {
			const auto words = List->item(row)->text().trimmed();
			if (!words.isEmpty()) {
				result.push_back(words.toStdString());
			}
		}
		return result;
	}

	void argument_list_editor::set_change(std::function<void()> aChange) { Change = std::move(aChange); }

	void argument_list_editor::add_row()
	{
		auto* const item = new QListWidgetItem{ List };
		item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);
		List->setCurrentItem(item);
		List->scrollToItem(item);
		List->setFocus(Qt::OtherFocusReason);
		List->editItem(item);
	}

	auto argument_list_editor::grip(const int aRow) const -> QWidget* { return Handles->grip(aRow); }

	auto argument_list_editor::trash(const int aRow) const -> QToolButton* { return Handles->trash(aRow); }

	void argument_list_editor::changed()
	{
		// A tooltip is item data too: set only when it differs, or the item's
		// change would come back here.
		for (auto row = 0; row < List->count(); ++row) {
			auto* const item = List->item(row);
			if (item->toolTip() != item->text()) {
				item->setToolTip(item->text());
			}
		}
		if (Change) {
			Change();
		}
	}

	void argument_list_editor::drop_empty()
	{
		// The row an editor is open on now, after Add argument, stays.
		const auto* const focus   = QApplication::focusWidget();
		const auto*       editing = focus != nullptr && focus->parent() == List->viewport() && qobject_cast<const QLineEdit*>(focus) != nullptr ? List->currentItem() : nullptr;
		auto              dropped = false;
		for (auto row = List->count() - 1; row >= 0; --row) {
			if (List->item(row) != editing && List->item(row)->text().trimmed().isEmpty()) {
				delete List->takeItem(row);
				dropped = true;
			}
		}
		if (dropped) {
			changed();
		}
	}

	void argument_list_editor::move(const int aRow, const int aDelta)
	{
		const auto to = aRow + aDelta;
		if (aRow < 0 || aRow >= List->count() || to < 0 || to >= List->count()) {
			return;
		}
		auto* const item = List->takeItem(aRow);
		List->insertItem(to, item);
		List->setCurrentRow(to);
		changed();
	}
} // namespace bpr::qt
