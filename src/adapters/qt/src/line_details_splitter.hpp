#ifndef BPR_SRC_ADAPTERS_QT_SRC_LINE_DETAILS_SPLITTER_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_LINE_DETAILS_SPLITTER_HPP

#include "bpr/qt/resize_handle_cursor.hpp"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStyle>
#include <QStyleOptionFocusRect>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>

namespace bpr::qt {
	class line_details_splitter;

	/// \brief [[BPR-1334]]. The seam above Line details: native, focusable,
	/// with the shared resize-cursor hold.
	class line_details_handle final : public QSplitterHandle
	{
	public:
		explicit line_details_handle(line_details_splitter& aSplit);

	protected:
		void keyPressEvent(QKeyEvent* aEvent) override;
		void mouseDoubleClickEvent(QMouseEvent* aEvent) override;
		void contextMenuEvent(QContextMenuEvent* aEvent) override;
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		line_details_splitter& Split;
	};

	/// \brief [[BPR-1334]]. The comparison above, Line details below.
	///
	/// \details Owns only local geometry. The strip keeps its height when the
	/// window grows or shrinks; the comparison takes the difference. Only an
	/// explicit gesture on the seam (a drag, Up/Down, Reset) reports a new
	/// preferred height, in logical pixels; a clamp in a small window does not
	/// overwrite it.
	class line_details_splitter final : public QSplitter
	{
	public:
		explicit line_details_splitter(QWidget* aParent)
			: QSplitter{ Qt::Vertical, aParent }
		{
			setChildrenCollapsible(false);
			setOpaqueResize(true);
			// Wide enough to aim at, from the font so it scales with it.
			setHandleWidth(std::max(6, fontMetrics().lineSpacing() / 3));
			connect(this, &QSplitter::splitterMoved, this, [this] {
				if (!Applying && count() == 2 && !widget(1)->isHidden()) {
					Preferred = static_cast<std::uint32_t>(std::max(1, sizes().back()));
					if (Changed) {
						Changed(Preferred);
					}
				}
			});
		}

		/// Text line height, for Up/Down and the default and minimum heights.
		std::function<int()> LineHeight;
		/// Told the preferred height after an explicit gesture; empty is Reset.
		std::function<void(std::optional<std::uint32_t>)> Changed;

		[[nodiscard]] auto preference() const noexcept -> std::optional<std::uint32_t> { return Preferred; }

		void set_preference(const std::optional<std::uint32_t> aHeight)
		{
			if (aHeight && *aHeight == 0U) {
				return;
			}
			Preferred = aHeight;
			apply_layout();
		}

		void reset()
		{
			Preferred.reset();
			apply_layout();
			if (Changed) {
				Changed(std::nullopt);
			}
		}

		/// \brief The strip's height with no preference: eight text lines, so
		/// a short selection's pairs show without scrolling, and a margin.
		[[nodiscard]] auto default_height() const -> int { return line_height() * 8 + 2 * frame(); }
		[[nodiscard]] auto minimum_strip_height() const -> int { return line_height() * 2 + 2 * frame(); }

		void step(const int aLines)
		{
			if (count() != 2 || widget(1)->isHidden()) {
				return;
			}
			// The seam is the strip's top edge: up makes the strip taller.
			moveSplitter(sizes().front() - aLines * line_height(), 1);
		}

		void apply_layout()
		{
			if (count() != 2 || widget(1)->isHidden()) {
				return;
			}
			Applying            = true;
			const auto shared   = std::max(2, sizes().front() + sizes().back());
			const auto panesMin = std::min(shared / 2, line_height() * 4);
			const auto wanted   = Preferred ? static_cast<int>(*Preferred) : default_height();
			const auto strip    = std::clamp(wanted, std::min(minimum_strip_height(), shared - panesMin), std::max(1, shared - panesMin));
			setSizes({ shared - strip, strip });
			Applying = false;
		}

	protected:
		auto createHandle() -> QSplitterHandle* override { return new line_details_handle{ *this }; }
		/// Keeps the strip at its preferred height whenever the room allows,
		/// so a window that shrank and grew again gets it back.
		void resizeEvent(QResizeEvent* aEvent) override
		{
			QSplitter::resizeEvent(aEvent);
			apply_layout();
		}

	private:
		[[nodiscard]] auto line_height() const -> int
		{
			return std::max(1, LineHeight ? LineHeight() : fontMetrics().lineSpacing());
		}
		[[nodiscard]] auto frame() const -> int
		{
			return count() == 2 ? widget(1)->contentsMargins().top() + 4 : 4;
		}

		std::optional<std::uint32_t> Preferred;
		bool                         Applying = false;
	};

	inline line_details_handle::line_details_handle(line_details_splitter& aSplit)
		: QSplitterHandle{ Qt::Vertical, &aSplit }
		, Split{ aSplit }
	{
		setAccessibleName(QStringLiteral("Resize line details"));
		setAccessibleDescription(QStringLiteral("Up and Down resize by one text line. Double-click or use the context menu to reset its size."));
		setToolTip(QStringLiteral("Resize line details · Up/Down: one line · Double-click: reset"));
		setFocusPolicy(Qt::StrongFocus);
		setCursor(Qt::SplitVCursor);
		// [[BPR-1330]]. The one cursor hold every resize seam shares.
		hold_resize_cursor(*this);
	}

	inline void line_details_handle::keyPressEvent(QKeyEvent* const aEvent)
	{
		if (aEvent->modifiers() == Qt::NoModifier && (aEvent->key() == Qt::Key_Up || aEvent->key() == Qt::Key_Down)) {
			Split.step(aEvent->key() == Qt::Key_Up ? 1 : -1);
			aEvent->accept();
			return;
		}
		QSplitterHandle::keyPressEvent(aEvent);
	}

	inline void line_details_handle::mouseDoubleClickEvent(QMouseEvent* const aEvent)
	{
		if (aEvent->button() == Qt::LeftButton) {
			Split.reset();
			aEvent->accept();
			return;
		}
		QSplitterHandle::mouseDoubleClickEvent(aEvent);
	}

	inline void line_details_handle::contextMenuEvent(QContextMenuEvent* const aEvent)
	{
		auto* const menu = new QMenu{ this };
		menu->setAttribute(Qt::WA_DeleteOnClose);
		connect(menu->addAction(QStringLiteral("Reset Line Details Size")), &QAction::triggered, &Split, [this] { Split.reset(); });
		menu->popup(aEvent->globalPos());
		aEvent->accept();
	}

	inline void line_details_handle::paintEvent(QPaintEvent* const aEvent)
	{
		QSplitterHandle::paintEvent(aEvent);
		if (hasFocus()) {
			auto painter = QPainter{ this };
			auto option  = QStyleOptionFocusRect{ };
			option.initFrom(this);
			option.rect = rect().adjusted(1, 0, -1, 0);
			style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
		}
	}
} // namespace bpr::qt
#endif
