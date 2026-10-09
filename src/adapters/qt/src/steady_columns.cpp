#include "bpr/qt/steady_columns.hpp"

#include <QAbstractItemModel>
#include <QEvent>
#include <QFontMetrics>
#include <QHeaderView>
#include <QModelIndex>
#include <QScrollBar>
#include <QStyle>
#include <QTreeView>

#include <algorithm>
#include <cstddef>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The cells per column and kind whose delegate measure is taken: the
		/// widest by their text. Bold and regular text order strings by width
		/// almost alike, so a few candidates find the widest of either.
		constexpr std::size_t candidates_per_kind = 8U;

		struct candidate final
		{
			int         Estimate = 0;
			int         Depth    = 0;
			QModelIndex Index;
		};

		void keep_widest(std::vector<candidate>& aWidest, candidate aCandidate)
		{
			if (aWidest.size() == candidates_per_kind && aWidest.back().Estimate >= aCandidate.Estimate) {
				return;
			}
			const auto at = std::ranges::upper_bound(aWidest, aCandidate.Estimate, std::greater{ }, &candidate::Estimate);
			aWidest.insert(at, std::move(aCandidate));
			if (aWidest.size() > candidates_per_kind) {
				aWidest.pop_back();
			}
		}
	} // namespace

	steady_columns::steady_columns(QTreeView& aView, const int aFillColumn, cell_kind aKind)
		: QObject{ &aView }
		, View{ aView }
		, Fill{ aFillColumn }
		, Kind{ std::move(aKind) }
	{
		auto* const header = View.header();
		header->setStretchLastSection(false);
		const auto* const model = View.model();
		Q_ASSERT(model != nullptr);
		const auto changed = [this] { invalidate(); };
		connect(model, &QAbstractItemModel::rowsInserted, this, changed);
		connect(model, &QAbstractItemModel::rowsRemoved, this, changed);
		connect(model, &QAbstractItemModel::rowsMoved, this, changed);
		connect(model, &QAbstractItemModel::modelReset, this, changed);
		connect(model, &QAbstractItemModel::layoutChanged, this, changed);
		connect(model, &QAbstractItemModel::columnsInserted, this, changed);
		connect(model, &QAbstractItemModel::columnsRemoved, this, changed);
		connect(model, &QAbstractItemModel::headerDataChanged, this, changed);
		connect(model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& aFirst, const QModelIndex& aLast) {
			// A change only in columns nobody measures (the fill, a fixed
			// width) leaves every width as it is.
			for (auto column = aFirst.column(); column <= aLast.column(); ++column) {
				if (column != Fill && !Fixed.contains(column)) {
					invalidate();
					return;
				}
			}
		});
		connect(header, &QHeaderView::sectionResized, this, [this](const int aColumn, int, const int aWidth) { user_resized(aColumn, aWidth); });
		// The view sizes the column to its visible rows first; this gives back
		// the measured width of the whole tree.
		connect(header, &QHeaderView::sectionHandleDoubleClicked, this, [this](const int aColumn) {
			User.erase(aColumn);
			lay_out();
		});
		View.installEventFilter(this);
		invalidate();
	}

	void steady_columns::set_fixed_width(const int aColumn, const std::optional<int> aWidth)
	{
		if (aWidth) {
			Fixed[aColumn] = *aWidth;
		} else if (Fixed.erase(aColumn) != 0U) {
			Dirty = true;
		}
		apply();
	}

	void steady_columns::apply()
	{
		if (Dirty) {
			measure();
		}
		lay_out();
	}

	auto steady_columns::shared_width() const -> int
	{
		const auto* const bar       = View.verticalScrollBar();
		const auto        transient = View.style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, bar) != 0;
		const auto        reserved  = transient || View.verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff ? 0 : bar->sizeHint().width();
		return std::max(0, View.contentsRect().width() - reserved);
	}

	auto steady_columns::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aWatched == &View) {
			switch (aEvent->type()) {
			case QEvent::Resize:
				lay_out();
				break;
			case QEvent::Show:
				apply();
				break;
			case QEvent::FontChange:
			case QEvent::StyleChange:
				Dirty = true;
				apply();
				break;
			default:
				break;
			}
		}
		return QObject::eventFilter(aWatched, aEvent);
	}

	void steady_columns::invalidate()
	{
		Dirty = true;
		if (Queued) {
			return;
		}
		Queued = true;
		QMetaObject::invokeMethod(this, [this] {
			Queued = false;
			if (Dirty) {
				apply();
			} }, Qt::QueuedConnection);
	}

	void steady_columns::measure()
	{
		Dirty                     = false;
		const auto* const model   = View.model();
		auto* const       header  = View.header();
		const auto        columns = model->columnCount();
		Natural.assign(static_cast<std::size_t>(columns), 0);
		auto measured = std::vector<int>{ };
		for (auto column = 0; column < columns; ++column) {
			Natural[static_cast<std::size_t>(column)] = std::max(header->minimumSectionSize(), header->isHidden() ? 0 : header->sectionSizeHint(column));
			if (column != Fill && !Fixed.contains(column)) {
				measured.push_back(column);
			}
		}
		if (measured.empty()) {
			return;
		}
		// Every row of the whole tree, open or not: estimate each cell by its
		// text, then take the delegate's own measure of the widest few.
		const auto metrics = QFontMetrics{ View.font() };
		auto       widest  = std::map<std::pair<int, int>, std::vector<candidate>>{ };
		auto       pending = std::vector<std::pair<QModelIndex, int>>{ { QModelIndex{ }, -1 } };
		while (!pending.empty()) {
			const auto [parent, depth] = pending.back();
			pending.pop_back();
			const auto rows = model->rowCount(parent);
			for (auto row = 0; row < rows; ++row) {
				const auto first = model->index(row, 0, parent);
				const auto kind  = Kind ? Kind(first) : 0;
				for (const auto column : measured) {
					const auto cell = column == 0 ? first : model->index(row, column, parent);
					keep_widest(widest[{ column, kind }], candidate{ .Estimate = metrics.horizontalAdvance(cell.data(Qt::DisplayRole).toString()), .Depth = depth + 1, .Index = cell });
				}
				if (model->hasChildren(first)) {
					pending.emplace_back(first, depth + 1);
				}
			}
		}
		// As QTreeView measures its tree column: each row's indentation.
		const auto tree   = View.treePosition() < 0 ? 0 : View.treePosition();
		const auto indent = View.indentation();
		const auto rooted = View.rootIsDecorated() ? 1 : 0;
		for (const auto& [key, cells] : widest) {
			auto& width = Natural[static_cast<std::size_t>(key.first)];
			for (const auto& cell : cells) {
				const auto inset = key.first == tree ? indent * (cell.Depth - 1 + rooted) : 0;
				width            = std::max(width, View.sizeHintForIndex(cell.Index).width() + inset);
			}
		}
	}

	void steady_columns::lay_out()
	{
		auto* const header  = View.header();
		const auto  columns = std::min(header->count(), static_cast<int>(Natural.size()));
		// With the fill column hidden, the last shown column fills instead.
		auto fill = Fill;
		if (fill >= 0 && fill < columns && header->isSectionHidden(fill)) {
			fill = -1;
			for (auto visual = header->count() - 1; visual >= 0 && fill < 0; --visual) {
				const auto logical = header->logicalIndex(visual);
				fill               = logical < columns && !header->isSectionHidden(logical) ? logical : -1;
			}
		}
		Applying   = true;
		auto taken = 0;
		for (auto column = 0; column < columns; ++column) {
			if (column == fill) {
				continue;
			}
			const auto fixed = Fixed.find(column);
			const auto user  = User.find(column);
			const auto mode  = fixed != Fixed.end() ? QHeaderView::Fixed : QHeaderView::Interactive;
			const auto width = fixed != Fixed.end() ? fixed->second : user != User.end() ? user->second
																						 : Natural[static_cast<std::size_t>(column)];
			if (header->sectionResizeMode(column) != mode) {
				header->setSectionResizeMode(column, mode);
			}
			if (header->sectionSize(column) != width) {
				header->resizeSection(column, width);
			}
			taken += header->isSectionHidden(column) ? 0 : width;
		}
		if (fill >= 0 && fill < columns) {
			if (header->sectionResizeMode(fill) != QHeaderView::Fixed) {
				header->setSectionResizeMode(fill, QHeaderView::Fixed);
			}
			const auto width = std::max(Natural[static_cast<std::size_t>(fill)], shared_width() - taken);
			if (header->sectionSize(fill) != width) {
				header->resizeSection(fill, width);
			}
		}
		Applying = false;
	}

	void steady_columns::user_resized(const int aColumn, const int aWidth)
	{
		if (Applying || aColumn == Fill || Fixed.contains(aColumn)) {
			return;
		}
		User[aColumn] = aWidth;
		lay_out();
	}
} // namespace bpr::qt
