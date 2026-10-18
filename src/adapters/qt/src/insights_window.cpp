#include "bpr/qt/insights_window.hpp"

#include <QApplication>
#include <QDateTime>
#include <QEvent>
#include <QGridLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr auto content_width  = 900;
		constexpr auto content_height = 542;

		auto outline(const QPalette& aPalette) -> QColor
		{
			auto result = aPalette.color(QPalette::WindowText);
			result.setAlphaF(0.16F);
			return result;
		}

		class divider final : public QWidget
		{
		public:
			explicit divider(QWidget* aParent)
				: QWidget{ aParent }
			{
				setFixedHeight(1);
			}

		protected:
			void paintEvent(QPaintEvent*) override
			{
				auto painter = QPainter{ this };
				painter.setPen(outline(palette()));
				painter.drawLine(0, 0, width(), 0);
			}
		};

		/// \brief Compact cards use the profile's surface and ink, including after a theme change.
		class card final : public QWidget
		{
		public:
			using QWidget::QWidget;

		protected:
			void paintEvent(QPaintEvent*) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing);
				painter.setPen(QPen{ outline(palette()), 1.0 });
				painter.setBrush(palette().color(QPalette::Base));
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);
			}
		};

		/// \brief QTabBar keeps native keyboard/accessibility behavior with the reference's quiet selected pill.
		class segment_bar final : public QTabBar
		{
		public:
			explicit segment_bar(QWidget* aParent)
				: QTabBar{ aParent }
			{
				setExpanding(false);
				setDrawBase(false);
				setFocusPolicy(Qt::StrongFocus);
			}

		protected:
			auto tabSizeHint(int aIndex) const -> QSize override
			{
				return { fontMetrics().horizontalAdvance(tabText(aIndex)) + 24, 32 };
			}
			auto minimumTabSizeHint(int aIndex) const -> QSize override { return tabSizeHint(aIndex); }
			void paintEvent(QPaintEvent*) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing);
				for (int index = 0; index < count(); ++index) {
					const auto box      = QRectF{ tabRect(index) }.adjusted(1.0, 1.0, -1.0, -1.0);
					const bool selected = index == currentIndex();
					if (selected) {
						painter.setPen(QPen{ outline(palette()), 1.0 });
						painter.setBrush(palette().color(QPalette::Base));
						painter.drawRoundedRect(box, 7.0, 7.0);
					}
					painter.setPen(palette().color(QPalette::WindowText));
					painter.drawText(box, Qt::AlignCenter, tabText(index));
					if (selected && hasFocus()) {
						painter.setPen(QPen{ palette().color(QPalette::Highlight), 1.5 });
						painter.setBrush(Qt::NoBrush);
						painter.drawRoundedRect(box.adjusted(2, 2, -2, -2), 5.0, 5.0);
					}
				}
			}
		};

		/// \brief Header actions retain native tool-button feedback; their small line icons follow the palette.
		class header_button final : public QToolButton
		{
		public:
			header_button(const QString& aText, bool aFolder, QWidget* aParent)
				: QToolButton{ aParent }
				, Folder{ aFolder }
			{
				setText(aText);
				setAccessibleName(aText);
				setToolTip(aText);
				setToolButtonStyle(Qt::ToolButtonIconOnly);
				setAutoRaise(true);
				setFixedSize(30, 30);
			}

		protected:
			void paintEvent(QPaintEvent*) override
			{
				auto painter = QStylePainter{ this };
				auto option  = QStyleOptionToolButton{ };
				initStyleOption(&option);
				option.text.clear();
				painter.drawComplexControl(QStyle::CC_ToolButton, option);
				painter.setRenderHint(QPainter::Antialiasing);
				painter.setPen(QPen{ palette().color(QPalette::WindowText), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				painter.setBrush(Qt::NoBrush);
				painter.translate(7, 7);
				if (Folder) {
					auto path = QPainterPath{ };
					path.moveTo(1, 13);
					path.lineTo(1, 3);
					path.lineTo(6, 3);
					path.lineTo(8, 5);
					path.lineTo(15, 5);
					path.lineTo(15, 13);
					path.closeSubpath();
					painter.drawPath(path);
				} else {
					painter.drawRoundedRect(QRectF{ 3, 1, 10, 14 }, 1.5, 1.5);
					painter.drawLine(6, 5, 10, 5);
					painter.drawLine(6, 8, 10, 8);
					painter.drawLine(6, 11, 9, 11);
				}
			}

		private:
			bool Folder;
		};

		auto label(const QString& aText, QWidget* aParent, const char* aType = "body") -> QLabel*
		{
			auto* result = new QLabel{ aText, aParent };
			result->setTextFormat(Qt::PlainText);
			result->setProperty("insightsType", aType);
			result->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
			if (QByteArray{ aType } == "secondary" || QByteArray{ aType } == "small") {
				result->setForegroundRole(QPalette::PlaceholderText);
			}
			return result;
		}

		struct tile_labels
		{
			QLabel* Value;
			QLabel* Detail;
		};
		auto tile(QGridLayout& aGrid, int aIndex, int aColumns, const QString& aTitle, const QString& aEmpty) -> tile_labels
		{
			auto* box = new card;
			box->setObjectName(QStringLiteral("insightsTile"));
			box->setFixedHeight(78);
			auto* column = new QVBoxLayout{ box };
			column->setContentsMargins(12, 10, 12, 10);
			column->setSpacing(2);
			column->addWidget(label(aTitle, box, "small"));
			auto* value = label(aEmpty, box, "empty");
			value->setWordWrap(true);
			column->addWidget(value);
			auto* detail = label({ }, box, "small");
			column->addWidget(detail);
			column->addStretch();
			aGrid.addWidget(box, aIndex / aColumns, aIndex % aColumns);
			return { value, detail };
		}

		auto grid(QVBoxLayout& aColumn, int aColumns) -> QGridLayout*
		{
			auto* result = new QGridLayout;
			result->setSpacing(8);
			for (int index = 0; index < aColumns; ++index) {
				result->setColumnStretch(index, 1);
			}
			aColumn.addLayout(result);
			return result;
		}

		void empty_section(QVBoxLayout& aColumn, const QString& aTitle, const QString& aText, int aHeight = 70)
		{
			auto* box = new card;
			box->setFixedHeight(aHeight);
			auto* column = new QVBoxLayout{ box };
			column->setContentsMargins(12, 10, 12, 10);
			column->setSpacing(6);
			column->addWidget(label(aTitle, box, "heading"));
			column->addWidget(label(aText, box, "secondary"));
			column->addStretch();
			aColumn.addWidget(box);
		}

		void ranges(QVBoxLayout& aColumn, const QStringList& aNames, const QString& aName, int aSelected = 0)
		{
			auto* range = new segment_bar{ aColumn.parentWidget() };
			range->setObjectName(QStringLiteral("insightsRange"));
			range->setAccessibleName(aName);
			for (const auto& name : aNames) {
				range->addTab(name);
			}
			range->setCurrentIndex(aSelected);
			aColumn.addWidget(range, 0, Qt::AlignRight);
		}

		auto bytes(const std::optional<std::uint64_t> aBytes) -> QString
		{
			return aBytes ? QLocale{ }.formattedDataSize(static_cast<qint64>(*aBytes), 2) : QStringLiteral("Not measured yet");
		}

		/// \brief The bounded storage ledger has thin rows and a shared budget meter, rather than boxed cells.
		class storage_delegate final : public QStyledItemDelegate
		{
		public:
			explicit storage_delegate(QTableWidget& aTable)
				: QStyledItemDelegate{ &aTable }
				, Table{ aTable }
			{ }
			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				auto       textOption = aOption;
				const auto ratio      = aIndex.data(Qt::UserRole);
				if (ratio.isValid()) {
					textOption.rect.setBottom(textOption.rect.bottom() - 10);
				}
				QStyledItemDelegate::paint(aPainter, textOption, aIndex);
				aPainter->save();
				aPainter->setClipRect(aOption.rect);
				aPainter->setPen(QPen{ outline(aOption.palette), 1.0 });
				aPainter->drawLine(aOption.rect.bottomLeft(), aOption.rect.bottomRight());
				if (ratio.isValid()) {
					aPainter->setRenderHint(QPainter::Antialiasing);
					aPainter->setPen(Qt::NoPen);
					aPainter->setBrush(outline(aOption.palette));
					const auto meter = QRectF{ 0, static_cast<qreal>(aOption.rect.bottom() - 8), static_cast<qreal>(Table.viewport()->width()), 5 };
					aPainter->drawRoundedRect(meter, 2.5, 2.5);
					aPainter->setBrush(aOption.palette.color(QPalette::Highlight));
					auto fill = meter;
					fill.setWidth(meter.width() * std::clamp(ratio.toDouble(), 0.0, 1.0));
					aPainter->drawRoundedRect(fill, 2.5, 2.5);
				}
				aPainter->restore();
			}

		private:
			QTableWidget& Table;
		};

		void cell(QTableWidget& aTable, int aRow, int aColumn, const QString& aText, const application::insights_storage_row& aStorage)
		{
			auto* item = aTable.item(aRow, aColumn);
			if (!item) {
				item = new QTableWidgetItem;
				aTable.setItem(aRow, aColumn, item);
			}
			item->setText(aText);
			item->setToolTip(aText);
			item->setData(Qt::UserRole, aStorage.Bytes && aStorage.Budget && *aStorage.Budget != 0 ? QVariant{ static_cast<double>(*aStorage.Bytes) / static_cast<double>(*aStorage.Budget) } : QVariant{ });
			item->setTextAlignment((aColumn == 0 ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
		}
	}

	insights_window::insights_window(QWidget* aParent)
		: QWidget{ aParent, Qt::Window }
	{
		setObjectName(QStringLiteral("insightsWindow"));
		setWindowTitle(tr("Insights"));
		setAccessibleName(tr("Insights"));
		auto* column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 16);
		column->setSpacing(0);
		auto* toolbar = new QWidget{ this };
		toolbar->setObjectName(QStringLiteral("insightsToolbar"));
		toolbar->setFixedHeight(52);
		auto* bar = new QHBoxLayout{ toolbar };
		bar->setContentsMargins(16, 8, 16, 8);
		bar->setSpacing(4);
		bar->addSpacing(64);
		bar->addStretch();
		Tabs = new segment_bar{ toolbar };
		Tabs->setObjectName(QStringLiteral("insightsTabs"));
		Tabs->setAccessibleName(tr("Insights views"));
		for (const auto& name : { tr("Summary"), tr("Reviews"), tr("GitHub"), tr("Storage"), tr("Work"), tr("Energy") }) {
			Tabs->addTab(name);
		}
		bar->addWidget(Tabs);
		bar->addStretch();
		auto* reveal = new header_button{ tr("Reveal data folder"), true, toolbar };
		reveal->setObjectName(QStringLiteral("insightsRevealData"));
		auto* exportButton = new header_button{ tr("Export anonymized diagnostics…"), false, toolbar };
		exportButton->setObjectName(QStringLiteral("insightsExport"));
		bar->addWidget(reveal);
		bar->addWidget(exportButton);
		column->addWidget(toolbar);
		column->addWidget(new divider{ this });
		auto* header = new QWidget{ this };
		header->setObjectName(QStringLiteral("insightsHeader"));
		header->setFixedHeight(62);
		auto* heading = new QVBoxLayout{ header };
		heading->setContentsMargins(16, 12, 16, 10);
		heading->setSpacing(0);
		ProfileLabel = label(tr("Insights"), header, "title");
		Status       = label(tr("Waiting for a local snapshot"), header, "secondary");
		heading->addWidget(ProfileLabel);
		heading->addWidget(Status);
		column->addWidget(header);
		Pages = new QStackedWidget{ this };
		Pages->setObjectName(QStringLiteral("insightsPages"));
		column->addWidget(Pages, 1);
		build_pages();
		connect(reveal, &QToolButton::clicked, this, [this] { if (Dispatch.RevealData) { Dispatch.RevealData(); } });
		connect(exportButton, &QToolButton::clicked, this, [this] { if (Dispatch.Export) { Dispatch.Export(); } });
		connect(Tabs, &QTabBar::currentChanged, this, [this](int aIndex) { Pages->setCurrentIndex(aIndex); refresh(); });
		RefreshTimer = new QTimer{ this };
		AgeTimer     = new QTimer{ this };
		AgeTimer->setInterval(1000);
		connect(RefreshTimer, &QTimer::timeout, this, [this] { refresh(); });
		connect(AgeTimer, &QTimer::timeout, this, [this] {
			if (BuiltUnixSeconds != 0) {
				Status->setText(tr("Updated %1 sec. ago").arg(std::max<qint64>(0, QDateTime::currentSecsSinceEpoch() - BuiltUnixSeconds)));
			}
		});
		connect(qApp, &QGuiApplication::applicationStateChanged, this, [this] { set_refresh_interval(); });
		refresh_typography();
		setFixedSize(content_width, content_height);
	}

	void insights_window::build_pages()
	{
		for (int index = 0; index < 6; ++index) {
			auto* page   = new QWidget{ Pages };
			auto* column = new QVBoxLayout{ page };
			column->setContentsMargins(16, 0, 16, 0);
			column->setSpacing(8);
			Pages->addWidget(page);
			if (index == 0) {
				empty_section(*column, tr("GitHub budget this hour"), tr("No reading in the current reset window"), 90);
				auto* tiles = grid(*column, 3);
				tile(*tiles, 0, 3, tr("Reviews submitted, 30 days"), tr("No review statistics yet"));
				tile(*tiles, 1, 3, tr("Comments posted, 30 days"), tr("No comment statistics yet"));
				tile(*tiles, 2, 3, tr("Cost while idle"), tr("No background samples yet"));
				const auto local = tile(*tiles, 3, 3, tr("Local data"), tr("Not measured yet"));
				LocalData        = local.Value;
				LocalDataDetail  = local.Detail;
				LocalData->setObjectName(QStringLiteral("insightsLocalData"));
				const auto work      = tile(*tiles, 4, 3, tr("Background work"), tr("Waiting for a snapshot"));
				BackgroundWork       = work.Value;
				BackgroundWorkDetail = work.Detail;
				tile(*tiles, 5, 3, tr("Problems, 24h"), tr("No problem tallies yet"));
			} else if (index == 3) {
				StorageStatus = label(tr("On disk · not measured yet"), page, "heading");
				column->addWidget(StorageStatus);
				Stores = new QTableWidget{ 0, 2, page };
				Stores->setObjectName(QStringLiteral("insightsStores"));
				Stores->setAccessibleName(tr("Local storage, bytes on disk and budget"));
				Stores->setHorizontalHeaderLabels({ tr("Store"), tr("On disk and budget") });
				Stores->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
				Stores->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
				Stores->horizontalHeader()->hide();
				Stores->verticalHeader()->hide();
				Stores->setFrameShape(QFrame::NoFrame);
				Stores->setShowGrid(false);
				Stores->setEditTriggers(QAbstractItemView::NoEditTriggers);
				Stores->setSelectionBehavior(QAbstractItemView::SelectRows);
				Stores->setItemDelegate(new storage_delegate{ *Stores });
				column->addWidget(Stores, 1);
				auto* actions = new QHBoxLayout;
				auto* manage  = new QPushButton{ tr("Manage cached repositories…"), page };
				manage->setObjectName(QStringLiteral("insightsManageCaches"));
				auto* reveal = new QPushButton{ tr("Reveal data folder"), page };
				actions->addWidget(manage);
				actions->addWidget(reveal);
				actions->addStretch();
				column->addLayout(actions);
				connect(manage, &QPushButton::clicked, this, [this] { if (Dispatch.ManageCaches) { Dispatch.ManageCaches(); } });
				connect(reveal, &QPushButton::clicked, this, [this] { if (Dispatch.RevealData) { Dispatch.RevealData(); } });
			} else {
				build_empty_page(index, *column);
			}
			if (index != 3) {
				column->addStretch();
			}
		}
	}

	void insights_window::build_empty_page(int aIndex, QVBoxLayout& aColumn)
	{
		if (aIndex == 1) {
			ranges(aColumn, { tr("This week"), tr("30 days"), tr("All time") }, tr("Reviews range"), 1);
			auto* tiles = grid(aColumn, 4);
			int   index = 0;
			for (const auto& title : { tr("Reviews submitted"), tr("Files accepted"), tr("Comments posted"), tr("Private notes") }) {
				tile(*tiles, index++, 4, title, tr("No review statistics yet"));
			}
			empty_section(aColumn, tr("Reviews submitted by day"), tr("Reviews submitted on this computer will fill the calendar."));
			auto* facts = new QHBoxLayout;
			for (const auto& title : { tr("How reviews ended"), tr("Records") }) {
				auto* section = new QVBoxLayout;
				section->addWidget(label(title, aColumn.parentWidget(), "heading"));
				section->addWidget(label(tr("No submitted reviews yet"), aColumn.parentWidget(), "secondary"));
				facts->addLayout(section, 1);
			}
			aColumn.addLayout(facts);
		} else if (aIndex == 2) {
			auto* tiles = grid(aColumn, 3);
			int   index = 0;
			for (const auto& title : { tr("Core REST"), tr("GraphQL"), tr("Search") }) {
				tile(*tiles, index++, 3, title, tr("No rate-limit reading yet"));
			}
			ranges(aColumn, { tr("1h"), tr("1d") }, tr("GitHub range"));
			empty_section(aColumn, tr("Where BPR’s requests went"), tr("Ordinary GitHub requests will fill this view."));
			empty_section(aColumn, tr("gh processes"), tr("No process observations yet"));
		} else if (aIndex == 4) {
			auto* tiles = grid(aColumn, 3);
			tile(*tiles, 0, 3, tr("Input response, p95"), tr("No event-loop timings yet"));
			tile(*tiles, 1, 3, tr("Files diffed"), tr("No diff timings yet"));
			tile(*tiles, 2, 3, tr("Syntax cache"), tr("No syntax observations yet"));
			ranges(aColumn, { tr("1h"), tr("1d") }, tr("Work range"));
			empty_section(aColumn, tr("Slowest files to diff"), tr("Diff work on this computer will fill this view."));
		} else {
			auto* tiles = grid(aColumn, 3);
			tile(*tiles, 0, 3, tr("Background CPU"), tr("No energy samples yet"));
			tile(*tiles, 1, 3, tr("Background wakeups"), tr("No wakeup samples yet"));
			tile(*tiles, 2, 3, tr("Background children"), tr("No child CPU samples yet"));
			ranges(aColumn, { tr("1h"), tr("1d"), tr("3m") }, tr("Energy range"));
			auto* pair = new QHBoxLayout;
			pair->setSpacing(14);
			auto* chart = new QVBoxLayout;
			empty_section(*chart, tr("CPU over the range"), tr("Foreground and background samples will fill this chart."), 136);
			pair->addLayout(chart, 2);
			auto* facts = new QVBoxLayout;
			facts->setSpacing(6);
			facts->addWidget(label(tr("Foreground and background"), aColumn.parentWidget(), "heading"));
			for (const auto& title : { tr("CPU"), tr("Wakeups"), tr("Child CPU"), tr("Samples") }) {
				facts->addWidget(label(title + tr(": no samples yet"), aColumn.parentWidget(), "secondary"));
			}
			facts->addStretch();
			pair->addLayout(facts, 1);
			aColumn.addLayout(pair);
		}
	}

	void insights_window::refresh_typography()
	{
		for (auto* item : findChildren<QLabel*>()) {
			const auto type = item->property("insightsType").toByteArray();
			if (type.isEmpty()) {
				continue;
			}
			auto textFont = font();
			if (type == "title") {
				textFont.setPointSizeF(textFont.pointSizeF() * 18.0 / 13.0);
				textFont.setWeight(QFont::DemiBold);
			} else if (type == "value") {
				textFont.setPointSizeF(textFont.pointSizeF() * 20.0 / 13.0);
				textFont.setWeight(QFont::Medium);
			} else if (type == "small") {
				textFont.setPointSizeF(textFont.pointSizeF() * 12.0 / 13.0);
			} else if (type == "heading") {
				textFont.setWeight(QFont::DemiBold);
			}
			item->setFont(textFont);
		}
	}

	void insights_window::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange) {
			refresh_typography();
		}
	}

	void insights_window::set_dispatch(application::insights_dispatch aDispatch) { Dispatch = std::move(aDispatch); }
	void insights_window::set_profile_label(const QString& aLabel)
	{
		setWindowTitle(tr("Insights — %1").arg(aLabel));
		ProfileLabel->setText(tr("Insights"));
	}
	auto insights_window::content_size(const QSize& aAvailable) -> QSize { return { std::max(1, std::min(content_width, aAvailable.width() - 36)), std::max(1, std::min(content_height, aAvailable.height() - 36)) }; }
	void insights_window::present(QWidget* aOpener)
	{
		if (!isVisible()) {
			auto*      targetScreen = aOpener ? aOpener->screen() : screen();
			const auto available    = targetScreen->availableGeometry();
			setFixedSize(content_size(available.size()));
			move(available.center() - QPoint{ width() / 2, height() / 2 });
		}
		show();
		raise();
		activateWindow();
	}
	void insights_window::set_refresh_interval() { RefreshTimer->setInterval(QGuiApplication::applicationState() == Qt::ApplicationActive ? 5000 : 30000); }
	void insights_window::showEvent(QShowEvent* aEvent)
	{
		QWidget::showEvent(aEvent);
		set_refresh_interval();
		RefreshTimer->start();
		AgeTimer->start();
		refresh();
	}
	void insights_window::hideEvent(QHideEvent* aEvent)
	{
		RefreshTimer->stop();
		AgeTimer->stop();
		QWidget::hideEvent(aEvent);
	}
	void insights_window::keyPressEvent(QKeyEvent* aEvent)
	{
		if (aEvent->key() == Qt::Key_Escape) {
			close();
			aEvent->accept();
		} else {
			QWidget::keyPressEvent(aEvent);
		}
	}
	void insights_window::refresh()
	{
		if (isVisible() && Dispatch.Refresh) {
			Dispatch.Refresh(Tabs->currentIndex() == 0 || Tabs->currentIndex() == 3);
		}
	}
	void insights_window::apply(const application::insights_snapshot& aSnapshot)
	{
		BuiltUnixSeconds = aSnapshot.BuiltUnixSeconds;
		Status->setText(tr("Updated 0 sec. ago"));
		LocalData->setText(aSnapshot.TotalBytes ? bytes(aSnapshot.TotalBytes) : tr("Measurement unavailable"));
		LocalData->setProperty("insightsType", aSnapshot.TotalBytes ? "value" : "empty");
		LocalDataDetail->setText(aSnapshot.TotalBytes ? tr("Git cache %1").arg(bytes(aSnapshot.GitBytes)) : QString{ });
		const auto work = QString::fromStdString(aSnapshot.BackgroundWork).split(QLatin1Char{ '\n' });
		BackgroundWork->setText(work.value(0));
		BackgroundWork->setProperty("insightsType", "value");
		BackgroundWorkDetail->setText(work.value(1));
		refresh_typography();
		StorageStatus->setText(aSnapshot.TotalBytes ? tr("On disk, %1 in all").arg(bytes(aSnapshot.TotalBytes)) : tr("Storage measurement unavailable. A local file could not be measured."));
		StorageStatus->setToolTip(tr("SQLite sizes include WAL and SHM files. Measured %1.").arg(QLocale{ }.toString(QDateTime::fromSecsSinceEpoch(aSnapshot.StorageUnixSeconds).time(), QLocale::ShortFormat)));
		Stores->setRowCount(static_cast<int>(aSnapshot.Storage.size()));
		for (std::size_t index = 0; index < aSnapshot.Storage.size(); ++index) {
			const auto& row = aSnapshot.Storage[index];
			cell(*Stores, static_cast<int>(index), 0, QString::fromStdString(row.Name), row);
			cell(*Stores, static_cast<int>(index), 1, row.Bytes ? (row.Budget ? tr("%1 of %2").arg(bytes(row.Bytes), bytes(row.Budget)) : bytes(row.Bytes)) : tr("Unavailable"), row);
			Stores->setRowHeight(static_cast<int>(index), row.Budget ? 38 : 28);
		}
	}
}
