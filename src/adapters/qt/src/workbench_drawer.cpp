#include "bpr/qt/workbench_drawer.hpp"

#include "bpr/application/workbench_runtime_ports.hpp"

#include <QAbstractItemView>
#include <QByteArray>
#include <QHash>
#include <QLabel>
#include <QList>
#include <QListView>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto changed_roles() -> const QList<int>&
		{
			static const QList<int> roles{
				Qt::DisplayRole,
				Qt::ToolTipRole,
				Qt::AccessibleTextRole,
				Qt::AccessibleDescriptionRole,
				workbench_drawer_model::identity_role,
				workbench_drawer_model::detail_role,
			};
			return roles;
		}

		[[nodiscard]] auto panel_object_name(const core::workbench_drawer aDrawer)
			-> QString
		{
			// [[BPR-0651]]. Named by switch: a drawer this file does not know
			// is a compile error, never the conversations' name twice.
			switch (aDrawer) {
			case core::workbench_drawer::files:
				return QStringLiteral("workbenchFilesDrawer");
			case core::workbench_drawer::conversations:
				return QStringLiteral("workbenchConversationsDrawer");
			case core::workbench_drawer::repositories:
				return QStringLiteral("workbenchRepositoriesDrawer");
			}
			return QStringLiteral("workbenchFilesDrawer");
		}

		[[nodiscard]] auto panel_name(const core::workbench_drawer aDrawer) -> QString
		{
			switch (aDrawer) {
			case core::workbench_drawer::files:
				return QStringLiteral("Files drawer");
			case core::workbench_drawer::conversations:
				return QStringLiteral("Conversations drawer");
			case core::workbench_drawer::repositories:
				return QStringLiteral("Repositories drawer");
			}
			return QStringLiteral("Files drawer");
		}

		[[nodiscard]] auto panel_description(const core::workbench_drawer aDrawer)
			-> QString
		{
			switch (aDrawer) {
			case core::workbench_drawer::files:
				return QStringLiteral(
					"Actionable pull-request files for the current review");
			case core::workbench_drawer::conversations:
				return QStringLiteral(
					"Review conversations anchored to the current comparison");
			case core::workbench_drawer::repositories:
				return QStringLiteral(
					"Repositories with their pull requests of interest");
			}
			return QString{ };
		}
	}

	workbench_drawer_model::workbench_drawer_model(QObject* const aParent)
		: QAbstractListModel{ aParent }
	{
	}

	auto workbench_drawer_model::rowCount(const QModelIndex& aParent) const -> int
	{
		return aParent.isValid() ? 0 : static_cast<int>(Rows.size());
	}

	auto workbench_drawer_model::data(
		const QModelIndex& aIndex,
		const int          aRequestedRole) const -> QVariant
	{
		if (!aIndex.isValid() || aIndex.column() != 0 || aIndex.row() < 0
			|| static_cast<std::size_t>(aIndex.row()) >= Rows.size()) {
			return { };
		}
		const auto& row = Rows[static_cast<std::size_t>(aIndex.row())];
		switch (aRequestedRole) {
		case Qt::DisplayRole:
		case Qt::AccessibleTextRole:
			return row.Title;
		case Qt::ToolTipRole:
		case Qt::AccessibleDescriptionRole:
			return row.Detail;
		case identity_role:
			return row.Identity;
		case detail_role:
			return row.Detail;
		default:
			return { };
		}
	}

	auto workbench_drawer_model::flags(const QModelIndex& aIndex) const
		-> Qt::ItemFlags
	{
		return aIndex.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
								: Qt::NoItemFlags;
	}

	auto workbench_drawer_model::roleNames() const -> QHash<int, QByteArray>
	{
		auto roles = QAbstractListModel::roleNames();
		roles.insert(identity_role, QByteArrayLiteral("identity"));
		roles.insert(detail_role, QByteArrayLiteral("detail"));
		return roles;
	}

	void workbench_drawer_model::apply(
		const std::vector<workbench_drawer_row>& aRows)
	{
		retire_absent(aRows);
		for (std::size_t index = 0U; index < aRows.size(); ++index) {
			insert_or_move(index, aRows[index]);
		}
	}

	auto workbench_drawer_model::index_for_identity(const QString& aIdentity) const
		-> QModelIndex
	{
		const auto found = row_for_identity(aIdentity);
		return found ? index(static_cast<int>(*found), 0) : QModelIndex{ };
	}

	auto workbench_drawer_model::identity_at(const QModelIndex& aIndex) const
		-> QString
	{
		if (!aIndex.isValid() || aIndex.row() < 0
			|| static_cast<std::size_t>(aIndex.row()) >= Rows.size()) {
			return { };
		}
		return Rows[static_cast<std::size_t>(aIndex.row())].Identity;
	}

	auto workbench_drawer_model::file_at(const QModelIndex& aIndex) const
		-> std::optional<core::workbench_file_id>
	{
		if (!aIndex.isValid() || aIndex.row() < 0
			|| static_cast<std::size_t>(aIndex.row()) >= Rows.size()) {
			return std::nullopt;
		}
		return Rows[static_cast<std::size_t>(aIndex.row())].File;
	}

	auto workbench_drawer_model::index_for_file(
		const core::workbench_file_id& aFile) const -> QModelIndex
	{
		const auto found = std::ranges::find(
			Rows, std::optional{ aFile }, &workbench_drawer_row::File);
		return found == Rows.end()
			? QModelIndex{ }
			: index(static_cast<int>(std::distance(Rows.begin(), found)), 0);
	}

	auto workbench_drawer_model::statistics() const noexcept
		-> workbench_drawer_model_statistics
	{
		return Statistics;
	}

	auto workbench_drawer_model::row_for_identity(const QString& aIdentity) const
		-> std::optional<std::size_t>
	{
		const auto found = std::ranges::find(
			Rows, aIdentity, &workbench_drawer_row::Identity);
		if (found == Rows.end()) {
			return std::nullopt;
		}
		return static_cast<std::size_t>(std::distance(Rows.begin(), found));
	}

	void workbench_drawer_model::retire_absent(
		const std::vector<workbench_drawer_row>& aRows)
	{
		for (auto index = Rows.size(); index != 0U; --index) {
			const auto identity = Rows[index - 1U].Identity;
			const auto retained = std::ranges::any_of(
				aRows,
				[&identity](const workbench_drawer_row& aRow) {
					return aRow.Identity == identity;
				});
			if (!retained) {
				remove(identity);
			}
		}
	}

	void workbench_drawer_model::insert_or_move(
		const std::size_t    aRequestedIndex,
		workbench_drawer_row aRow)
	{
		const auto existing = row_for_identity(aRow.Identity);
		const auto target   = std::min(aRequestedIndex, Rows.size());
		if (!existing) {
			const auto position = static_cast<int>(target);
			beginInsertRows({ }, position, position);
			Rows.insert(Rows.begin() + static_cast<std::ptrdiff_t>(target),
						std::move(aRow));
			endInsertRows();
			++Statistics.RowsInserted;
			return;
		}

		auto current = *existing;
		if (current != target && target < Rows.size()) {
			const auto source = static_cast<int>(current);
			// beginMoveRows names the row the moved row lands before, so a forward
			// move counts the vacated row itself.
			const auto destination = static_cast<int>(
				target > current ? target + 1U : target);
			beginMoveRows({ }, source, source, { }, destination);
			auto moved = std::move(Rows[current]);
			Rows.erase(Rows.begin() + static_cast<std::ptrdiff_t>(current));
			Rows.insert(Rows.begin() + static_cast<std::ptrdiff_t>(target),
						std::move(moved));
			endMoveRows();
			current = target;
			++Statistics.RowsMoved;
		}
		if (Rows[current] != aRow) {
			Rows[current]       = std::move(aRow);
			const auto position = index(static_cast<int>(current), 0);
			emit       dataChanged(position, position, changed_roles());
			++Statistics.RowsChanged;
		}
	}

	void workbench_drawer_model::remove(const QString& aIdentity)
	{
		const auto existing = row_for_identity(aIdentity);
		if (!existing) {
			return;
		}
		const auto position = static_cast<int>(*existing);
		beginRemoveRows({ }, position, position);
		Rows.erase(Rows.begin() + static_cast<std::ptrdiff_t>(*existing));
		endRemoveRows();
		++Statistics.RowsRemoved;
	}

	workbench_drawer_rail::workbench_drawer_rail(
		const core::workbench_drawer aDrawer,
		QWidget* const               aParent)
		: QAbstractButton{ aParent }
		, Drawer{ aDrawer }
	{
		setObjectName(panel_object_name(aDrawer) + QStringLiteral("Rail"));
		setAccessibleName(label() + QStringLiteral(" rail"));
		setAccessibleDescription(
			QStringLiteral("Expand the %1 drawer.").arg(label().toLower()));
		setFocusPolicy(Qt::StrongFocus);
		setCursor(Qt::PointingHandCursor);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
		setFixedWidth(width_pixels);
	}

	void workbench_drawer_rail::set_summary(const QString& aSummary)
	{
		if (Summary == aSummary) {
			return;
		}
		Summary = aSummary;
		setToolTip(
			Summary.isEmpty() ? label()
							  : QStringLiteral("%1, %2").arg(label(), Summary));
		update();
	}

	auto workbench_drawer_rail::summary() const -> QString
	{
		return Summary;
	}

	auto workbench_drawer_rail::label() const -> QString
	{
		switch (Drawer) {
		case core::workbench_drawer::files:
			return QStringLiteral("Files");
		case core::workbench_drawer::conversations:
			return QStringLiteral("Comments");
		case core::workbench_drawer::repositories:
			return QStringLiteral("Repositories");
		}
		return QStringLiteral("Files");
	}

	auto workbench_drawer_rail::sizeHint() const -> QSize
	{
		return QSize{ width_pixels, 120 };
	}

	auto workbench_drawer_rail::minimumSizeHint() const -> QSize
	{
		return QSize{ width_pixels, 40 };
	}

	void workbench_drawer_rail::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.fillRect(rect(), palette().color(QPalette::Active, QPalette::Window));
		// The rule sits on the edge the drawer reopens from.
		painter.setPen(palette().color(QPalette::Active, QPalette::Mid));
		// A left drawer draws its rule on its right edge, the comments drawer
		// on its left.
		const auto edge = Drawer != core::workbench_drawer::conversations ? width() - 1 : 0;
		painter.drawLine(edge, 0, edge, height());
		if (hasFocus()) {
			painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Highlight), 1.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(
				QRectF{ rect() }.adjusted(1.5, 1.5, -1.5, -1.5), 3.0, 3.0);
		}
		// The label reads bottom to top, the count in the accent beneath it.
		const auto text = Summary.isEmpty()
			? label()
			: QStringLiteral("%1  %2").arg(label(), Summary);
		painter.setPen(palette().color(QPalette::Active, QPalette::WindowText));
		painter.translate(
			static_cast<qreal>(width()) / 2.0, static_cast<qreal>(height()) - 12.0);
		painter.rotate(-90.0);
		const auto metrics = painter.fontMetrics();
		painter.drawText(
			QRectF{ 0.0,
					-static_cast<qreal>(metrics.height()) / 2.0,
					static_cast<qreal>(height()) - 24.0,
					static_cast<qreal>(metrics.height()) },
			Qt::AlignLeft | Qt::AlignVCenter,
			metrics.elidedText(text, Qt::ElideRight, height() - 24));
	}

	workbench_drawer_panel::workbench_drawer_panel(
		const core::workbench_drawer aDrawer,
		QWidget* const               aParent)
		: QWidget{ aParent }
		, Drawer{ aDrawer }
	{
		setObjectName(panel_object_name(aDrawer));
		setAccessibleName(panel_name(aDrawer));
		setAccessibleDescription(panel_description(aDrawer));
		// The host clamps a persisted width to this floor, so refusing to be
		// dragged below it keeps a reported width inside the range the host will
		// actually retain.
		setMinimumWidth(
			static_cast<int>(application::minimum_workbench_drawer_width));
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

		Heading = new QLabel{ panel_name(aDrawer), this };
		Heading->setObjectName(panel_object_name(aDrawer)
							   + QStringLiteral("Heading"));
		// An ignored horizontal hint keeps the heading from imposing a drawer
		// minimum wider than the width the host is willing to persist.
		Heading->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

		Contents = new workbench_drawer_model{ this };
		View     = new QListView{ this };
		View->setObjectName(panel_object_name(aDrawer) + QStringLiteral("View"));
		View->setAccessibleName(panel_name(aDrawer));
		View->setAccessibleDescription(panel_description(aDrawer));
		View->setModel(Contents);
		View->setUniformItemSizes(true);
		View->setSelectionMode(QAbstractItemView::SingleSelection);
		View->setSelectionBehavior(QAbstractItemView::SelectRows);
		View->setFocusPolicy(Qt::StrongFocus);

		Rail = new workbench_drawer_rail{ aDrawer, this };
		Rail->hide();

		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->addWidget(Heading);
		column->addWidget(View, 1);
		column->addWidget(Rail, 1);
	}

	auto workbench_drawer_panel::drawer() const noexcept -> core::workbench_drawer
	{
		return Drawer;
	}

	auto workbench_drawer_panel::state() const noexcept
		-> core::workbench_drawer_state
	{
		return State;
	}

	auto workbench_drawer_panel::contents() const noexcept
		-> workbench_drawer_model*
	{
		return Contents;
	}

	auto workbench_drawer_panel::contents_view() const noexcept -> QListView*
	{
		return View;
	}

	void workbench_drawer_panel::set_content_widget(
		QWidget* const aWidget,
		QWidget* const aFocusStop)
	{
		auto* const column = qobject_cast<QVBoxLayout*>(layout());
		if (aWidget == nullptr || column == nullptr) {
			return;
		}
		if (View != nullptr) {
			column->removeWidget(View);
			delete View;
			View = nullptr;
			// Parented to the panel rather than to the view, so it outlives the
			// view unless it is deleted here as well.
			delete Contents;
			Contents = nullptr;
		}
		aWidget->setParent(this);
		column->insertWidget(column->indexOf(Rail), aWidget, 1);
		Content      = aWidget;
		ContentFocus = aFocusStop != nullptr ? aFocusStop : aWidget;
		aWidget->setVisible(State == core::workbench_drawer_state::expanded);
	}

	auto workbench_drawer_panel::rail() const noexcept -> workbench_drawer_rail*
	{
		return Rail;
	}

	auto workbench_drawer_panel::focus_stop() const noexcept -> QWidget*
	{
		return ContentFocus != nullptr ? ContentFocus
									   : static_cast<QWidget*>(View);
	}

	void workbench_drawer_panel::set_width_dispatch(
		workbench_drawer_width_dispatch aDispatch)
	{
		WidthDispatch = std::move(aDispatch);
	}

	void workbench_drawer_panel::adopt(const core::workbench_drawer_state aState)
	{
		if (State == aState) {
			return;
		}
		State = aState;
		// [[BPR-0646]]. Collapsed narrows the panel to its rail rather than
		// hiding it; the splitter gives the centre everything beyond the
		// rail's width, and the rail keeps the drawer named and one click
		// away. Expanded restores the width the host persists.
		const auto expanded = State == core::workbench_drawer_state::expanded;
		// Everything the column holds is the drawer's content, including what
		// the window placed there itself; only the rail stays for the rail.
		if (auto* const column = layout(); column != nullptr) {
			for (auto index = 0; index < column->count(); ++index) {
				auto* const item = column->itemAt(index);
				if (item != nullptr && item->widget() != nullptr
					&& item->widget() != Rail) {
					item->widget()->setVisible(expanded);
				}
			}
		}
		Rail->setVisible(!expanded);
		if (expanded) {
			setMinimumWidth(
				static_cast<int>(application::minimum_workbench_drawer_width));
			setMaximumWidth(QWIDGETSIZE_MAX);
		} else {
			setFixedWidth(workbench_drawer_rail::width_pixels);
		}
	}

	void workbench_drawer_panel::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		if (State != core::workbench_drawer_state::expanded
			|| aEvent->oldSize().width() == aEvent->size().width()
			|| aEvent->size().width() <= 0 || !WidthDispatch) {
			return;
		}
		WidthDispatch(Drawer, static_cast<std::uint32_t>(aEvent->size().width()));
	}
}
