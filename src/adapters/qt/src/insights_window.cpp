#include "bpr/qt/insights_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace bpr::qt {
	namespace {
		auto tile(QGridLayout& aGrid, const int aIndex, const QString& aTitle, const QString& aText) -> QLabel*
		{
			auto* box    = new QGroupBox{ aTitle };
			auto* column = new QVBoxLayout{ box };
			auto* value  = new QLabel{ aText, box };
			value->setWordWrap(true);
			value->setTextFormat(Qt::PlainText);
			value->setAlignment(Qt::AlignTop | Qt::AlignLeft);
			column->addWidget(value);
			column->addStretch();
			aGrid.addWidget(box, aIndex / 3, aIndex % 3);
			return value;
		}

		auto bytes(const std::optional<std::uint64_t> aBytes) -> QString
		{
			return aBytes ? QLocale{ }.formattedDataSize(static_cast<qint64>(*aBytes), 2) : QStringLiteral("Not measured yet");
		}

		void cell(QTableWidget& aTable, const int aRow, const int aColumn, const QString& aText)
		{
			auto* item = aTable.item(aRow, aColumn);
			if (!item) {
				item = new QTableWidgetItem;
				aTable.setItem(aRow, aColumn, item);
			}
			item->setText(aText);
			if (aColumn != 0) {
				item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
			}
		}
	}

	insights_window::insights_window(QWidget* aParent)
		: QWidget{ aParent, Qt::Window }
	{
		setObjectName(QStringLiteral("insightsWindow"));
		setWindowTitle(tr("Insights"));
		setAccessibleName(tr("Insights"));
		auto* column = new QVBoxLayout{ this };
		column->setContentsMargins(20, 18, 20, 18);
		column->setSpacing(14);
		auto* header     = new QHBoxLayout;
		auto* heading    = new QVBoxLayout;
		ProfileLabel     = new QLabel{ tr("Insights"), this };
		auto headingFont = ProfileLabel->font();
		headingFont.setPointSizeF(headingFont.pointSizeF() * 1.45);
		headingFont.setBold(true);
		ProfileLabel->setFont(headingFont);
		ProfileLabel->setTextFormat(Qt::PlainText);
		Status = new QLabel{ tr("Waiting for a local snapshot"), this };
		heading->addWidget(ProfileLabel);
		heading->addWidget(Status);
		header->addLayout(heading);
		header->addStretch();
		auto* reveal = new QPushButton{ tr("Reveal data folder"), this };
		reveal->setObjectName(QStringLiteral("insightsRevealData"));
		auto* exportButton = new QPushButton{ tr("Export anonymized diagnostics…"), this };
		exportButton->setObjectName(QStringLiteral("insightsExport"));
		header->addWidget(reveal);
		header->addWidget(exportButton);
		column->addLayout(header);
		connect(reveal, &QPushButton::clicked, this, [this] { if (Dispatch.RevealData) { Dispatch.RevealData(); } });
		connect(exportButton, &QPushButton::clicked, this, [this] { if (Dispatch.Export) { Dispatch.Export(); } });

		Tabs = new QTabWidget{ this };
		Tabs->setObjectName(QStringLiteral("insightsTabs"));
		Tabs->setAccessibleName(tr("Insights views"));
		Tabs->setDocumentMode(true);
		column->addWidget(Tabs, 1);
		const std::array names{ tr("Summary"), tr("Reviews"), tr("GitHub"), tr("Storage"), tr("Work"), tr("Energy") };
		const std::array empty{ QString{ }, tr("No review statistics yet. Reviews you open and submit will appear here."),
								tr("No rate-limit readings yet. Ordinary GitHub responses will fill this view."), QString{ },
								tr("No work timings yet. Diff, syntax and search work will appear here."),
								tr("No energy samples yet. Foreground and background samples will appear here.") };
		for (int index = 0; index < static_cast<int>(names.size()); ++index) {
			auto* page       = new QWidget{ Tabs };
			auto* pageColumn = new QVBoxLayout{ page };
			pageColumn->setContentsMargins(0, 16, 0, 0);
			Tabs->addTab(page, names[static_cast<std::size_t>(index)]);
			if (index == 0) {
				auto* budget       = new QGroupBox{ tr("GitHub budget this hour"), page };
				auto* budgetColumn = new QVBoxLayout{ budget };
				budgetColumn->addWidget(new QLabel{ tr("No reading in the current reset window"), budget });
				pageColumn->addWidget(budget);
				auto* grid = new QGridLayout;
				grid->setSpacing(12);
				for (int axis = 0; axis < 3; ++axis) {
					grid->setColumnStretch(axis, 1);
				}
				tile(*grid, 0, tr("Reviews submitted · 30 days"), tr("No review statistics yet"));
				tile(*grid, 1, tr("Comments posted · 30 days"), tr("No comment statistics yet"));
				tile(*grid, 2, tr("Cost while idle"), tr("No background energy samples yet"));
				LocalData = tile(*grid, 3, tr("Local data"), tr("Not measured yet"));
				LocalData->setObjectName(QStringLiteral("insightsLocalData"));
				BackgroundWork = tile(*grid, 4, tr("Background work"), tr("Waiting for a local snapshot"));
				tile(*grid, 5, tr("Problems · 24 hours"), tr("No problem tallies yet"));
				pageColumn->addLayout(grid, 1);
			} else if (index == 3) {
				StorageStatus = new QLabel{ tr("Not measured yet. Store sizes include their SQLite sidecars."), page };
				StorageStatus->setWordWrap(true);
				pageColumn->addWidget(StorageStatus);
				Stores = new QTableWidget{ 0, 3, page };
				Stores->setObjectName(QStringLiteral("insightsStores"));
				Stores->setAccessibleName(tr("Local storage, bytes on disk and budget"));
				Stores->setHorizontalHeaderLabels({ tr("Store"), tr("On disk"), tr("Budget") });
				Stores->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
				Stores->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
				Stores->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
				Stores->verticalHeader()->hide();
				Stores->setEditTriggers(QAbstractItemView::NoEditTriggers);
				Stores->setSelectionBehavior(QAbstractItemView::SelectRows);
				Stores->setAlternatingRowColors(true);
				pageColumn->addWidget(Stores, 1);
				auto* manage = new QPushButton{ tr("Manage cached repositories…"), page };
				manage->setObjectName(QStringLiteral("insightsManageCaches"));
				pageColumn->addWidget(manage, 0, Qt::AlignLeft);
				connect(manage, &QPushButton::clicked, this, [this] { if (Dispatch.ManageCaches) { Dispatch.ManageCaches(); } });
			} else {
				auto* range = new QComboBox{ page };
				range->setObjectName(QStringLiteral("insightsRange%1").arg(index));
				range->setAccessibleName(names[static_cast<std::size_t>(index)] + tr(" range"));
				if (index == 1) {
					range->addItems({ tr("This week"), tr("30 days"), tr("All time") });
				} else {
					range->addItems(index == 5 ? QStringList{ tr("1 hour"), tr("1 day"), tr("3 months") } : QStringList{ tr("1 hour"), tr("1 day") });
				}
				pageColumn->addWidget(range, 0, Qt::AlignRight);
				auto* notice = new QLabel{ empty[static_cast<std::size_t>(index)], page };
				notice->setWordWrap(true);
				notice->setAlignment(Qt::AlignCenter);
				pageColumn->addWidget(notice, 1);
			}
		}
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
		connect(Tabs, &QTabWidget::currentChanged, this, [this] { refresh(); });
		setFixedSize(900, 640);
	}

	void insights_window::set_dispatch(application::insights_dispatch aDispatch) { Dispatch = std::move(aDispatch); }
	void insights_window::set_profile_label(const QString& aLabel)
	{
		setWindowTitle(tr("Insights — %1").arg(aLabel));
		ProfileLabel->setText(tr("Insights"));
	}
	auto insights_window::content_size(const QSize& aAvailable) -> QSize { return { std::max(1, std::min(900, aAvailable.width() - 36)), std::max(1, std::min(640, aAvailable.height() - 36)) }; }
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
		LocalData->setText(aSnapshot.TotalBytes ? tr("%1 in all\n%2 in the Git cache").arg(bytes(aSnapshot.TotalBytes), bytes(aSnapshot.GitBytes)) : tr("Storage measurement unavailable"));
		BackgroundWork->setText(QString::fromStdString(aSnapshot.BackgroundWork));
		StorageStatus->setText(aSnapshot.TotalBytes ? tr("%1 in all. SQLite sizes include WAL and SHM files. Measured %2.").arg(bytes(aSnapshot.TotalBytes), QLocale{ }.toString(QDateTime::fromSecsSinceEpoch(aSnapshot.StorageUnixSeconds).time(), QLocale::ShortFormat)) : tr("Storage measurement unavailable. A local file could not be measured."));
		Stores->setRowCount(static_cast<int>(aSnapshot.Storage.size()));
		for (std::size_t index = 0; index < aSnapshot.Storage.size(); ++index) {
			const auto& row = aSnapshot.Storage[index];
			cell(*Stores, static_cast<int>(index), 0, QString::fromStdString(row.Name));
			cell(*Stores, static_cast<int>(index), 1, row.Bytes ? bytes(row.Bytes) : tr("Unavailable"));
			cell(*Stores, static_cast<int>(index), 2, row.Budget ? bytes(row.Budget) : tr("No fixed budget"));
		}
	}
}
