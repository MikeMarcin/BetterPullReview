#include "wrapping_row.hpp"

#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace bpr::qt::detail {
	namespace {
		struct placed
		{
			QLayoutItem* Item = nullptr;
			QRect        Box;
			int          Row = 0;
		};

		[[nodiscard]] auto expands(const QLayoutItem& aItem) -> bool { return (aItem.expandingDirections() & Qt::Horizontal) != 0; }

		[[nodiscard]] auto height_at(const QLayoutItem& aItem, const int aWidth) -> int
		{
			return aItem.hasHeightForWidth() ? aItem.heightForWidth(aWidth) : aItem.sizeHint().height();
		}
	} // namespace

	wrapping_row::wrapping_row(QWidget* const aParent, const int aHorizontalSpacing, const int aVerticalSpacing)
		: QLayout{ aParent }
		, HorizontalSpacing{ aHorizontalSpacing }
		, VerticalSpacing{ aVerticalSpacing }
	{
		setContentsMargins(0, 0, 0, 0);
	}

	wrapping_row::~wrapping_row()
	{
		while (auto* const item = takeAt(0)) {
			delete item;
		}
	}

	void wrapping_row::set_last_trails(const bool aTrails)
	{
		if (LastTrails != aTrails) {
			LastTrails = aTrails;
			invalidate();
		}
	}

	void wrapping_row::addItem(QLayoutItem* const aItem) { Items.push_back(aItem); }

	auto wrapping_row::count() const -> int { return static_cast<int>(Items.size()); }

	auto wrapping_row::itemAt(const int aIndex) const -> QLayoutItem* { return aIndex >= 0 && aIndex < Items.size() ? Items.at(aIndex) : nullptr; }

	auto wrapping_row::takeAt(const int aIndex) -> QLayoutItem* { return aIndex >= 0 && aIndex < Items.size() ? Items.takeAt(aIndex) : nullptr; }

	auto wrapping_row::expandingDirections() const -> Qt::Orientations { return { }; }

	auto wrapping_row::hasHeightForWidth() const -> bool { return true; }

	auto wrapping_row::heightForWidth(const int aWidth) const -> int { return arrange(QRect{ 0, 0, aWidth, 0 }, false); }

	auto wrapping_row::minimumSize() const -> QSize
	{
		auto size = QSize{ };
		for (const auto* const item : Items) {
			if (!item->isEmpty()) {
				size = size.expandedTo(item->minimumSize());
			}
		}
		const auto margins = contentsMargins();
		return size + QSize{ margins.left() + margins.right(), margins.top() + margins.bottom() };
	}

	auto wrapping_row::sizeHint() const -> QSize
	{
		auto width  = 0;
		auto height = 0;
		auto shown  = 0;
		for (const auto* const item : Items) {
			if (!item->isEmpty()) {
				width += item->sizeHint().width();
				height = std::max(height, item->sizeHint().height());
				++shown;
			}
		}
		const auto margins = contentsMargins();
		return QSize{ width + (std::max(shown, 1) - 1) * HorizontalSpacing + margins.left() + margins.right(), height + margins.top() + margins.bottom() };
	}

	void wrapping_row::setGeometry(const QRect& aRect)
	{
		QLayout::setGeometry(aRect);
		arrange(aRect, true);
	}

	auto wrapping_row::arrange(const QRect& aRect, const bool aPlace) const -> int
	{
		const auto margins = contentsMargins();
		const auto area    = aRect.marginsRemoved(margins);
		const auto width   = std::max(area.width(), 0);
		auto       items   = std::vector<placed>{ };
		auto       natural = 0;
		auto       grow    = 0;
		for (auto* const item : Items) {
			if (!item->isEmpty()) {
				natural += (items.empty() ? 0 : HorizontalSpacing) + item->sizeHint().width();
				grow += expands(*item) ? 1 : 0;
				items.push_back({ .Item = item });
			}
		}
		if (items.empty()) {
			return margins.top() + margins.bottom();
		}

		if (natural <= width) {
			// One row: the space left over goes to the items that expand.
			auto spare = width - natural;
			auto x     = area.x();
			for (auto& entry : items) {
				auto itemWidth = entry.Item->sizeHint().width();
				if (expands(*entry.Item) && grow > 0) {
					const auto share = spare / grow;
					itemWidth += share;
					spare -= share;
					--grow;
				}
				entry.Box = QRect{ x, 0, itemWidth, height_at(*entry.Item, itemWidth) };
				x += itemWidth + HorizontalSpacing;
			}
			// Nothing took the space left over: it stands before the last.
			if (LastTrails && spare > 0) {
				items.back().Box.moveLeft(items.back().Box.x() + spare);
			}
		} else {
			// Wrapped: an item that expands takes a row of its own, and none
			// is wider than the row unless its own minimum is.
			auto x        = area.x();
			auto row      = 0;
			auto ownsNext = false;
			for (auto& entry : items) {
				const auto own       = expands(*entry.Item);
				const auto itemWidth = std::max(own ? width : std::min(entry.Item->sizeHint().width(), width), entry.Item->minimumSize().width());
				if (x > area.x() && (own || ownsNext || x + itemWidth > area.x() + width)) {
					++row;
					x = area.x();
				}
				entry.Box = QRect{ x, 0, itemWidth, height_at(*entry.Item, itemWidth) };
				entry.Row = row;
				x += itemWidth + HorizontalSpacing;
				ownsNext = own;
			}
			// The last ends its row, whether it shares it or has it alone.
			if (auto& last = items.back().Box; LastTrails && last.width() < width) {
				last.moveLeft(area.x() + width - last.width());
			}
		}

		// Each row is as tall as its tallest item; the others are centred in it.
		auto heights = std::vector<int>(static_cast<std::size_t>(items.back().Row) + 1U, 0);
		for (const auto& entry : items) {
			auto& height = heights[static_cast<std::size_t>(entry.Row)];
			height       = std::max(height, entry.Box.height());
		}
		auto tops = std::vector<int>(heights.size(), area.y());
		for (std::size_t index = 1U; index < heights.size(); ++index) {
			tops[index] = tops[index - 1U] + heights[index - 1U] + VerticalSpacing;
		}
		if (aPlace) {
			for (const auto& entry : items) {
				const auto row = static_cast<std::size_t>(entry.Row);
				entry.Item->setGeometry(QRect{ entry.Box.x(), tops[row] + (heights[row] - entry.Box.height()) / 2, entry.Box.width(), entry.Box.height() });
			}
		}
		return tops.back() + heights.back() - area.y() + margins.top() + margins.bottom();
	}
} // namespace bpr::qt::detail
