#ifndef BPR_SRC_ADAPTERS_QT_SRC_OVERVIEW_SUMMARY_SPLITTER_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_OVERVIEW_SUMMARY_SPLITTER_HPP

#include <QAccessibleWidget>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace bpr::qt {
	class overview_summary_splitter;

	/// \brief A native, focusable summary seam; all gestures use the same legal range.
	class overview_summary_handle final : public QSplitterHandle
	{
	public:
		explicit overview_summary_handle(overview_summary_splitter& aSplit);
		[[nodiscard]] auto allocation() const -> int;
		[[nodiscard]] auto shared_height() const -> int;
		void               choose_allocation(int aHeight);

	protected:
		void keyPressEvent(QKeyEvent* aEvent) override;
		void mouseDoubleClickEvent(QMouseEvent* aEvent) override;
		void contextMenuEvent(QContextMenuEvent* aEvent) override;
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		overview_summary_splitter& Split;
	};

	class overview_summary_accessible final : public QAccessibleWidget, public QAccessibleValueInterface
	{
	public:
		explicit overview_summary_accessible(overview_summary_handle* aHandle)
			: QAccessibleWidget{ aHandle, QAccessible::Slider }
		{ }
		auto interface_cast(QAccessible::InterfaceType aType) -> void* override
		{
			return aType == QAccessible::ValueInterface ? static_cast<QAccessibleValueInterface*>(this) : QAccessibleWidget::interface_cast(aType);
		}
		auto currentValue() const -> QVariant override { return handle()->allocation(); }
		auto minimumValue() const -> QVariant override { return 0; }
		auto maximumValue() const -> QVariant override { return handle()->shared_height(); }
		auto minimumStepSize() const -> QVariant override { return 1; }
		void setCurrentValue(const QVariant& aValue) override { handle()->choose_allocation(aValue.toInt()); }

	private:
		auto handle() const -> overview_summary_handle* { return static_cast<overview_summary_handle*>(object()); }
	};

	/// \brief Owns only local geometry. Explicit gestures report a fraction; reflow never overwrites it.
	class overview_summary_splitter final : public QSplitter
	{
	public:
		explicit overview_summary_splitter(QWidget* aParent)
			: QSplitter{ Qt::Vertical, aParent }
		{
			setChildrenCollapsible(false);
			setOpaqueResize(true);
			setHandleWidth(std::max(9, fontMetrics().lineSpacing() / 2));
			static const auto factoryInstalled = [] {
				QAccessible::installFactory([](const QString&, QObject* aObject) -> QAccessibleInterface* {
					if (auto* handle = dynamic_cast<overview_summary_handle*>(aObject)) {
						return new overview_summary_accessible{ handle };
					}
					return nullptr;
				});
				return true;
			}();
			(void)factoryInstalled;
			connect(this, &QSplitter::splitterMoved, this, [this] {
				if (!Applying && !Expanded && shared_height() > 0) {
					Preferred = static_cast<std::uint32_t>(std::clamp(10000 * sizes().front() / shared_height(), 1, 9999));
					if (Changed) {
						Changed(Preferred);
					}
					announce();
				}
			});
		}
		[[nodiscard]] auto                                minimumSizeHint() const -> QSize override { return { 0, 0 }; }
		std::function<int()>                              LineHeight, NaturalHeight, SummaryMinimum, FilesMinimum;
		std::function<void(std::optional<std::uint32_t>)> Changed;
		auto                                              preference() const -> std::optional<std::uint32_t> { return Preferred; }
		void                                              set_preference(std::optional<std::uint32_t> aFraction)
		{
			if (aFraction && (*aFraction == 0 || *aFraction >= 10000)) {
				return;
			}
			Preferred = aFraction;
			queue_layout();
		}
		void set_expanded(bool aExpanded)
		{
			Expanded = aExpanded;
			queue_layout();
		}
		void reset()
		{
			Preferred.reset();
			queue_layout();
			if (Changed) {
				Changed(std::nullopt);
			}
		}
		auto shared_height() const -> int
		{
			const auto allocated = sizes();
			return allocated.size() == 2 ? allocated.front() + allocated.back() : 0;
		}
		void choose_allocation(int aHeight)
		{
			if (Expanded || count() != 2 || widget(0)->isHidden()) {
				return;
			}
			moveSplitter(aHeight, 1);
		}
		void queue_layout()
		{
			if (Queued) {
				return;
			}
			Queued = true;
			QTimer::singleShot(0, this, [this] { Queued = false; apply_layout(); });
		}

	protected:
		auto createHandle() -> QSplitterHandle* override { return new overview_summary_handle{ *this }; }
		void resizeEvent(QResizeEvent* aEvent) override
		{
			if (count() == 2) {
				widget(0)->setMinimumHeight(0);
				widget(1)->setMinimumHeight(0);
			}
			QSplitter::resizeEvent(aEvent);
			queue_layout();
		}
		void changeEvent(QEvent* aEvent) override
		{
			QSplitter::changeEvent(aEvent);
			if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange || aEvent->type() == QEvent::ApplicationFontChange) {
				setHandleWidth(std::max(9, fontMetrics().lineSpacing() / 2));
				queue_layout();
			}
		}

	private:
		void apply_layout()
		{
			if (count() != 2 || !LineHeight || !NaturalHeight || !SummaryMinimum || !FilesMinimum) {
				return;
			}
			Applying = true;
			widget(0)->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
			widget(1)->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
			if (Expanded || widget(0)->isHidden() || widget(1)->isHidden()) {
				widget(0)->setMinimumHeight(0);
				widget(1)->setMinimumHeight(0);
				Applying = false;
				return;
			}
			const auto shared         = std::max(2, height() - handleWidth());
			const auto natural        = NaturalHeight();
			const auto summaryMinimum = Preferred ? SummaryMinimum() : std::min(SummaryMinimum(), natural);
			const auto filesMinimum   = FilesMinimum();
			const auto totalMinimum   = std::max(1, summaryMinimum + filesMinimum);
			const auto summaryFloor   = totalMinimum <= shared ? summaryMinimum : std::max(1, shared * summaryMinimum / totalMinimum);
			const auto filesFloor     = totalMinimum <= shared ? filesMinimum : std::max(1, shared - summaryFloor);
			widget(0)->setMinimumHeight(summaryFloor);
			widget(1)->setMinimumHeight(filesFloor);
			const auto target = Preferred ? static_cast<int>(std::lround(shared * (*Preferred / 10000.0))) : std::min(natural, shared / 2);
			setSizes({ std::clamp(target, summaryFloor, shared - filesFloor), shared - std::clamp(target, summaryFloor, shared - filesFloor) });
			Applying = false;
			announce();
		}
		void announce()
		{
			if (count() == 2) {
				QAccessibleValueChangeEvent event{ handle(1), sizes().front() };
				QAccessible::updateAccessibility(&event);
			}
		}
		std::optional<std::uint32_t> Preferred;
		bool                         Applying = false, Queued = false, Expanded = false;
	};

	inline overview_summary_handle::overview_summary_handle(overview_summary_splitter& aSplit)
		: QSplitterHandle{ Qt::Vertical, &aSplit }
		, Split{ aSplit }
	{
		setObjectName(QStringLiteral("overviewResizeSummary"));
		setAccessibleName(QStringLiteral("Resize summary"));
		setAccessibleDescription(QStringLiteral("Up and Down resize by one text line. Double-click or use the context menu to reset summary size."));
		setToolTip(QStringLiteral("Resize summary · Up/Down: one line · Double-click: reset"));
		setFocusPolicy(Qt::StrongFocus);
		setCursor(Qt::SplitVCursor);
	}
	inline auto overview_summary_handle::allocation() const -> int { return Split.sizes().value(0); }
	inline auto overview_summary_handle::shared_height() const -> int { return Split.shared_height(); }
	inline void overview_summary_handle::choose_allocation(int aHeight) { Split.choose_allocation(aHeight); }
	inline void overview_summary_handle::keyPressEvent(QKeyEvent* aEvent)
	{
		if (aEvent->modifiers() == Qt::NoModifier && (aEvent->key() == Qt::Key_Up || aEvent->key() == Qt::Key_Down)) {
			choose_allocation(allocation() + (aEvent->key() == Qt::Key_Down ? 1 : -1) * (Split.LineHeight ? Split.LineHeight() : fontMetrics().lineSpacing()));
			aEvent->accept();
			return;
		}
		QSplitterHandle::keyPressEvent(aEvent);
	}
	inline void overview_summary_handle::mouseDoubleClickEvent(QMouseEvent* aEvent)
	{
		if (aEvent->button() == Qt::LeftButton) {
			Split.reset();
			aEvent->accept();
			return;
		}
		QSplitterHandle::mouseDoubleClickEvent(aEvent);
	}
	inline void overview_summary_handle::contextMenuEvent(QContextMenuEvent* aEvent)
	{
		auto* menu = new QMenu{ this };
		menu->setAttribute(Qt::WA_DeleteOnClose);
		connect(menu->addAction(QStringLiteral("Reset summary size")), &QAction::triggered, &Split, [this] { Split.reset(); });
		menu->popup(aEvent->globalPos());
		aEvent->accept();
	}
	inline void overview_summary_handle::paintEvent(QPaintEvent* aEvent)
	{
		QSplitterHandle::paintEvent(aEvent);
		auto painter = QPainter{ this };
		painter.setPen(palette().color(QPalette::Mid));
		const auto halfGrip = std::max(12, fontMetrics().lineSpacing());
		painter.drawLine(width() / 2 - halfGrip, height() / 2 - 1, width() / 2 + halfGrip, height() / 2 - 1);
		painter.drawLine(width() / 2 - halfGrip, height() / 2 + 1, width() / 2 + halfGrip, height() / 2 + 1);
		if (hasFocus()) {
			auto option = QStyleOptionFocusRect{ };
			option.initFrom(this);
			option.rect = rect().adjusted(1, 1, -1, -1);
			style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
		}
	}
} // namespace bpr::qt
#endif
