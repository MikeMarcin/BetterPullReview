#include "list_row_handles.hpp"

#include "conversation_glyphs.hpp"

#include "bpr/qt/quiet_tool_button.hpp"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>

#include <array>
#include <utility>

namespace bpr::qt::detail {
	namespace {
		/// \brief Two columns of three dots, as the prompts page's and the
		/// labels' grips are drawn, in the row's quiet ink.
		class row_grip final : public QWidget
		{
		public:
			row_grip(QListWidget& aList, QWidget* aParent)
				: QWidget{ aParent }
				, List{ aList }
			{
				setObjectName(QStringLiteral("rowGrip"));
				// The press reaches the list under it, which starts its drag.
				setAttribute(Qt::WA_TransparentForMouseEvents, true);
				setFocusPolicy(Qt::NoFocus);
				setAccessibleDescription(QCoreApplication::translate("bpr::qt::list_row_handles", "Drag to reorder, or select the row and press Alt+Up or Alt+Down."));
			}

			int Row = -1;

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				const auto* const item     = List.item(Row);
				const auto        selected = item != nullptr && item->isSelected();
				auto              painter  = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(Qt::NoPen);
				painter.setBrush(List.palette().color(selected ? QPalette::HighlightedText : QPalette::PlaceholderText));
				const auto centre = QPointF{ width() / 2.0, height() / 2.0 };
				for (const auto dx : std::array{ -2.0, 2.0 }) {
					for (const auto dy : std::array{ -4.0, 0.0, 4.0 }) {
						painter.drawEllipse(centre + QPointF{ dx, dy }, 1.1, 1.1);
					}
				}
			}

		private:
			QListWidget& List;
		};

		[[nodiscard]] auto row_of(const QObject* aHandle) -> int { return aHandle == nullptr ? -1 : aHandle->property("bprRow").toInt(); }
	} // namespace

	list_row_handles::list_row_handles(QListWidget& aList, hooks aHooks)
		: QObject{ &aList }
		, List{ aList }
		, Hooks{ std::move(aHooks) }
		, Viewport{ aList.viewport() }
	{
		List.installEventFilter(this);
		List.viewport()->installEventFilter(this);
		List.viewport()->setMouseTracking(true);
		auto* const model = List.model();
		connect(model, &QAbstractItemModel::rowsInserted, this, [this] { sync(); });
		connect(model, &QAbstractItemModel::rowsRemoved, this, [this] { sync(); });
		connect(model, &QAbstractItemModel::rowsMoved, this, [this] { sync(); });
		connect(model, &QAbstractItemModel::modelReset, this, [this] { sync(); });
		connect(model, &QAbstractItemModel::layoutChanged, this, [this] { sync(); });
		connect(model, &QAbstractItemModel::dataChanged, this, [this] { sync(); });
		connect(List.verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { place(); });
		connect(&List, &QListWidget::itemSelectionChanged, this, [this] { shade(); });
		sync();
	}

	void list_row_handles::sync()
	{
		// The viewport goes before the model while the list is destroyed, and
		// the model's last reset must find nothing to do.
		if (Viewport.isNull()) {
			return;
		}
		const auto rows = static_cast<std::size_t>(std::max(0, List.count()));
		while (Grips.size() > rows) {
			delete Grips.back();
			Grips.pop_back();
		}
		while (Trash.size() > rows) {
			delete Trash.back();
			Trash.pop_back();
		}
		while (Grips.size() < rows) {
			auto* const grip = new row_grip{ List, List.viewport() };
			grip->show();
			Grips.push_back(grip);
		}
		while (Hooks.Remove && Trash.size() < rows) {
			auto* const trash = new quiet_tool_button{ List.viewport() };
			trash->setObjectName(Hooks.TrashName);
			trash->setIcon(conversation_icon(conversation_glyph::trash, List.palette()));
			trash->setAutoRaise(true);
			trash->set_surface(QPalette::Base);
			trash->set_tinted(true);
			trash->setFocusPolicy(Qt::TabFocus);
			connect(trash, &QToolButton::clicked, this, [this, trash] {
				const auto row = row_of(trash);
				if (row >= 0 && row < List.count() && Hooks.Remove) {
					List.setCurrentRow(row);
					Hooks.Remove(row);
				}
			});
			trash->show();
			Trash.push_back(trash);
		}
		for (std::size_t row = 0; row < rows; ++row) {
			const auto at   = static_cast<int>(row);
			const auto name = Hooks.Name ? Hooks.Name(at) : QString{ };
			auto*      grip = static_cast<row_grip*>(Grips[row]);
			grip->Row       = at;
			grip->setProperty("bprRow", at);
			grip->setAccessibleName(QCoreApplication::translate("bpr::qt::list_row_handles", "Reorder %1").arg(name));
			if (row < Trash.size()) {
				Trash[row]->setProperty("bprRow", at);
				Trash[row]->setAccessibleName(QCoreApplication::translate("bpr::qt::list_row_handles", "Remove %1").arg(name));
				Trash[row]->setToolTip(Trash[row]->accessibleName());
			}
		}
		place();
		shade();
	}

	void list_row_handles::place()
	{
		if (Viewport.isNull()) {
			return;
		}
		for (std::size_t row = 0; row < Grips.size(); ++row) {
			const auto rect = List.visualItemRect(List.item(static_cast<int>(row)));
			Grips[row]->setGeometry(rect.left(), rect.top(), row_grip_gutter, rect.height());
			if (row < Trash.size()) {
				// Rows span the viewport; a row's own rectangle is laid out
				// again only after a resize, so the viewport gives the edge.
				const auto side = std::min(rect.height() - 4, Trash[row]->sizeHint().height());
				Trash[row]->setGeometry(Viewport->width() - 7 - side, rect.center().y() - side / 2, side, side);
			}
		}
	}

	void list_row_handles::shade()
	{
		if (Viewport.isNull()) {
			return;
		}
		// A selected row's trash is read in the selection's ink on its ground.
		for (std::size_t row = 0; row < Trash.size(); ++row) {
			auto* const button   = static_cast<quiet_tool_button*>(Trash[row]);
			const auto* item     = List.item(static_cast<int>(row));
			const auto  selected = item != nullptr && item->isSelected();
			button->set_surface_colours(selected ? List.palette().color(QPalette::Highlight) : QColor{ }, selected ? List.palette().color(QPalette::HighlightedText) : QColor{ });
		}
		for (auto* const grip : Grips) {
			grip->update();
		}
	}

	auto list_row_handles::grip(const int aRow) const -> QWidget* { return aRow < 0 || static_cast<std::size_t>(aRow) >= Grips.size() ? nullptr : Grips[static_cast<std::size_t>(aRow)]; }

	auto list_row_handles::trash(const int aRow) const -> QToolButton* { return aRow < 0 || static_cast<std::size_t>(aRow) >= Trash.size() ? nullptr : Trash[static_cast<std::size_t>(aRow)]; }

	auto list_row_handles::eventFilter(QObject* const aObject, QEvent* const aEvent) -> bool
	{
		if (aObject == &List && aEvent->type() == QEvent::KeyPress) {
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			const auto        row = List.currentRow();
			const auto        alt = (key->modifiers() & Qt::AltModifier) != 0;
			if (alt && (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)) {
				const auto delta = key->key() == Qt::Key_Up ? -1 : 1;
				if (row >= 0 && row + delta >= 0 && row + delta < List.count() && Hooks.Move) {
					Hooks.Move(row, delta);
				}
				return true;
			}
			if ((key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace) && key->modifiers() == Qt::NoModifier && Hooks.Remove && row >= 0) {
				Hooks.Remove(row);
				return true;
			}
		}
		if (!Viewport.isNull() && aObject == Viewport.data()) {
			if (aEvent->type() == QEvent::Resize) {
				place();
				// And again once the list has laid its rows out for the new size.
				QTimer::singleShot(0, this, [this] { place(); });
			} else if (aEvent->type() == QEvent::MouseMove) {
				// The open hand over a grip says a drag starts there.
				const auto* const mouse = static_cast<QMouseEvent*>(aEvent);
				const auto        point = mouse->position().toPoint();
				const auto        over  = List.itemAt(point) != nullptr && point.x() - List.visualItemRect(List.itemAt(point)).left() < row_grip_gutter;
				if (over) {
					List.viewport()->setCursor(Qt::OpenHandCursor);
				} else {
					List.viewport()->unsetCursor();
				}
			}
		}
		return QObject::eventFilter(aObject, aEvent);
	}
} // namespace bpr::qt::detail
