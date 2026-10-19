#include "bpr/qt/insights_window.hpp"
#include "bpr/qt/insights_calendar.hpp"
#include "bpr/qt/insights_chart.hpp"
#include "bpr/qt/native_window_chrome.hpp"

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
#include <numeric>
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
				painter.setPen(Qt::NoPen);
				painter.setBrush(palette().color(QPalette::Button));
				painter.drawRoundedRect(QRectF{ rect() }, 7.0, 7.0);
				for (int index = 0; index < count(); ++index) {
					const auto box      = QRectF{ tabRect(index) }.adjusted(1.0, 1.0, -1.0, -1.0);
					const bool selected = index == currentIndex();
					if (selected) {
						painter.setPen(Qt::NoPen);
						painter.setBrush(palette().color(QPalette::Highlight));
						painter.drawRoundedRect(box, 7.0, 7.0);
					}
					painter.setPen(palette().color(selected ? QPalette::HighlightedText : QPalette::ButtonText));
					painter.drawText(box, Qt::AlignCenter, tabText(index));
					if (selected && hasFocus()) {
						painter.setPen(QPen{ palette().color(QPalette::HighlightedText), 1.5 });
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

		class budget_bar final : public QWidget
		{
		public:
			explicit budget_bar(QWidget* aParent)
				: QWidget{ aParent }
			{
				setFixedHeight(90);
				setObjectName(QStringLiteral("insightsBudget"));
				setAccessibleName(tr("GitHub budget this hour"));
			}
			void apply(core::insights_budget aBudget)
			{
				Budget                 = std::move(aBudget);
				const auto description = Budget.Limit ? tr("BetterPullReview about %1 requests; other clients about %2; %3 left of %4. Resets at %5.")
															.arg(QLocale{ }.toString(Budget.OwnRequests), QLocale{ }.toString(Budget.OtherClients.value_or(0)), QLocale{ }.toString(Budget.Remaining.value_or(0)), QLocale{ }.toString(*Budget.Limit), reset_time())
													  : tr("No reading in the current reset window");
				setAccessibleDescription(description);
				setToolTip(description);
				setProperty("ownRequests", QVariant::fromValue<qulonglong>(Budget.OwnRequests));
				update();
			}
			void set_color(QColor aColor)
			{
				Ink = std::move(aColor);
				update();
			}

		protected:
			void paintEvent(QPaintEvent*) override
			{
				QPainter painter{ this };
				painter.setRenderHint(QPainter::Antialiasing);
				painter.setPen(QPen{ outline(palette()), 1.0 });
				painter.setBrush(palette().color(QPalette::Base));
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
				auto titleFont = font();
				titleFont.setWeight(QFont::DemiBold);
				painter.setFont(titleFont);
				painter.setPen(palette().color(QPalette::WindowText));
				painter.drawText(QRect{ 12, 8, width() - 24, 20 }, Qt::AlignLeft | Qt::AlignVCenter, tr("GitHub budget this hour"));
				auto detailFont = font();
				detailFont.setPointSizeF(detailFont.pointSizeF() * 11.0 / 13.0);
				painter.setFont(detailFont);
				painter.setPen(palette().color(QPalette::PlaceholderText));
				if (!Budget.Limit) {
					painter.drawText(QRect{ 12, 39, width() - 24, 22 }, Qt::AlignLeft | Qt::AlignVCenter, tr("No reading in the current reset window"));
					return;
				}
				painter.drawText(QRect{ 12, 8, width() - 24, 20 }, Qt::AlignRight | Qt::AlignVCenter, tr("resets at %1").arg(reset_time()));
				const auto   meter     = QRectF{ 12.0, 34.0, static_cast<double>(width() - 24), 20.0 };
				const auto   own       = static_cast<double>(Budget.OwnRequests);
				const auto   other     = static_cast<double>(Budget.OtherClients.value_or(0));
				const auto   remaining = static_cast<double>(Budget.Remaining.value_or(0));
				const auto   total     = std::max(1.0, std::max(static_cast<double>(*Budget.Limit), own + other + remaining));
				QPainterPath clip;
				clip.addRoundedRect(meter, 3, 3);
				painter.save();
				painter.setClipPath(clip);
				painter.fillRect(meter, outline(palette()));
				const auto ink = Ink.isValid() ? Ink : palette().color(QPalette::Highlight);
				painter.fillRect(QRectF{ meter.left(), meter.top(), meter.width() * own / total, meter.height() }, ink);
				painter.fillRect(QRectF{ meter.left() + meter.width() * own / total, meter.top(), meter.width() * other / total, meter.height() }, palette().color(QPalette::PlaceholderText));
				painter.restore();
				painter.setPen(palette().color(QPalette::PlaceholderText));
				const auto legend = tr("BetterPullReview about %1     Other clients about %2     %3 left of %4")
										.arg(QLocale{ }.toString(Budget.OwnRequests), QLocale{ }.toString(Budget.OtherClients.value_or(0)), QLocale{ }.toString(Budget.Remaining.value_or(0)), QLocale{ }.toString(*Budget.Limit));
				painter.drawText(QRect{ 12, 60, width() - 24, 20 }, Qt::AlignLeft | Qt::AlignVCenter, legend);
			}

		private:
			auto                  reset_time() const -> QString { return QLocale{ }.toString(QDateTime::fromSecsSinceEpoch(Budget.ResetUnixSeconds).time(), QLocale::ShortFormat); }
			core::insights_budget Budget;
			QColor                Ink;
		};
		auto fact_table(QWidget* aParent, const QString& aName) -> QTableWidget*
		{
			auto* table = new QTableWidget{ 0, 2, aParent };
			table->setObjectName(aName);
			table->setAccessibleName(aName);
			table->horizontalHeader()->hide();
			table->verticalHeader()->hide();
			table->setFrameShape(QFrame::NoFrame);
			table->setShowGrid(false);
			table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
			table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
			table->setEditTriggers(QAbstractItemView::NoEditTriggers);
			table->setSelectionBehavior(QAbstractItemView::SelectRows);
			table->setMinimumHeight(48);
			return table;
		}
		void fact_row(QTableWidget& aTable, const int aRow, const QString& aLabel, const QString& aValue)
		{
			for (int column = 0; column < 2; ++column) {
				auto* item = aTable.item(aRow, column);
				if (!item) {
					item = new QTableWidgetItem;
					aTable.setItem(aRow, column, item);
				}
				item->setText(column == 0 ? aLabel : aValue);
				item->setToolTip(item->text());
				item->setTextAlignment((column == 0 ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
				if (column == 0) {
					item->setForeground(aTable.palette().color(QPalette::PlaceholderText));
				}
			}
			aTable.setRowHeight(aRow, 21);
		}
		auto category_names() -> QStringList { return { QObject::tr("Inbox and review queue"), QObject::tr("Opening pull requests"), QObject::tr("Comments and threads"), QObject::tr("Publishing"), QObject::tr("Other") }; }

		auto energy_seconds(const std::uint64_t aMicroseconds) -> QString
		{
			return QObject::tr("%1 s").arg(QLocale{ }.toString(static_cast<double>(aMicroseconds) / 1000000.0, 'f', 2));
		}
		auto energy_child(const core::insights_energy_sample& aSide, const std::size_t aKind) -> QString
		{
			if (!aSide.Children[aKind]) {
				return QObject::tr("No children");
			}
			if (aSide.UnmeasuredChildren[aKind]) {
				return QObject::tr("Unavailable");
			}
			return energy_seconds(aSide.ChildCpuMicroseconds[aKind]);
		}
		auto energy_fact(const core::insights_energy_sample& aSide, const int aRow) -> QString
		{
			if (aRow == 2 || aRow == 3) {
				return energy_child(aSide, static_cast<std::size_t>(aRow - 2));
			}
			if (!aSide.Samples) {
				return QObject::tr("No samples");
			}
			switch (aRow) {
			case 0:  return energy_seconds(aSide.CpuMicroseconds);
			case 1:  return aSide.Wakeups ? QLocale{ }.toString(*aSide.Wakeups) : QObject::tr("Not reported");
			case 4:  return QLocale{ }.toString(aSide.Samples);
			case 5:  return energy_seconds(aSide.WallMicroseconds);
			case 6:  return QObject::tr("%1%").arg(QLocale{ }.toString(aSide.PeakLoad * 100.0, 'f', 2));
			default: return { };
			}
		}

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

		auto ranges(QVBoxLayout& aColumn, const QStringList& aNames, const QString& aName, int aSelected = 0) -> QTabBar*
		{
			auto* range = new segment_bar{ aColumn.parentWidget() };
			range->setObjectName(QStringLiteral("insightsRange"));
			range->setAccessibleName(aName);
			for (const auto& name : aNames) {
				range->addTab(name);
			}
			range->setCurrentIndex(aSelected);
			aColumn.addWidget(range, 0, Qt::AlignRight);
			return range;
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
		// Share the native caption row, as the workbench does. The left inset
		// belongs to macOS traffic lights; Windows keeps its caption controls right.
		setWindowFlag(Qt::ExpandedClientAreaHint, true);
		setWindowFlag(Qt::NoTitleBarBackgroundHint, true);
		setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
		auto* column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 16);
		column->setSpacing(0);
		auto* toolbar = new QWidget{ this };
		toolbar->setObjectName(QStringLiteral("insightsToolbar"));
		toolbar->setFixedHeight(40);
		auto* bar = new QHBoxLayout{ toolbar };
		bar->setContentsMargins(16, 4, 16, 4);
#if defined(Q_OS_WIN)
		bar->setContentsMargins(16, 4, 154, 4);
#endif
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
				BudgetBar = new budget_bar{ page };
				column->addWidget(BudgetBar);
				auto*      tiles     = grid(*column, 3);
				const auto reviews   = tile(*tiles, 0, 3, tr("Reviews submitted, 30 days"), tr("No review statistics yet"));
				const auto comments  = tile(*tiles, 1, 3, tr("Comments posted, 30 days"), tr("No comment statistics yet"));
				SummaryReviewValues  = { reviews.Value, comments.Value };
				SummaryReviewDetails = { reviews.Detail, comments.Detail };
				SummaryReviewValues[0]->setObjectName(QStringLiteral("insightsSummaryReviews"));
				SummaryReviewValues[1]->setObjectName(QStringLiteral("insightsSummaryComments"));
				const auto idle   = tile(*tiles, 2, 3, tr("Cost while idle"), tr("No background samples yet"));
				SummaryIdle       = idle.Value;
				SummaryIdleDetail = idle.Detail;
				SummaryIdle->setObjectName(QStringLiteral("insightsSummaryIdle"));
				const auto local = tile(*tiles, 3, 3, tr("Local data"), tr("Not measured yet"));
				LocalData        = local.Value;
				LocalDataDetail  = local.Detail;
				LocalData->setObjectName(QStringLiteral("insightsLocalData"));
				const auto work      = tile(*tiles, 4, 3, tr("Background work"), tr("Waiting for a snapshot"));
				BackgroundWork       = work.Value;
				BackgroundWorkDetail = work.Detail;
				const auto problems  = tile(*tiles, 5, 3, tr("Problems, 24h"), tr("No problem tallies yet"));
				Problems             = problems.Value;
				ProblemsDetail       = problems.Detail;
				Problems->setObjectName(QStringLiteral("insightsProblems"));
			} else if (index == 1) {
				build_reviews_page(*column);
			} else if (index == 2) {
				build_github_page(*column);
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
			} else if (index == 5) {
				build_energy_page(*column);
			} else {
				build_empty_page(index, *column);
			}
			if (index != 3 && index != 2 && index != 1 && index != 5) {
				column->addStretch();
			}
		}
	}

	void insights_window::build_energy_page(QVBoxLayout& aColumn)
	{
		auto*             tiles = grid(aColumn, 3);
		const QStringList titles{ tr("Background CPU"), tr("Background wakeups"), tr("Background children") };
		for (int index = 0; index < 3; ++index) {
			const auto labels                              = tile(*tiles, index, 3, titles[index], tr("No background samples yet"));
			EnergyValues[static_cast<std::size_t>(index)]  = labels.Value;
			EnergyDetails[static_cast<std::size_t>(index)] = labels.Detail;
			labels.Value->setObjectName(QStringLiteral("insightsEnergy%1").arg(index));
		}
		auto* pair = new QHBoxLayout;
		pair->setSpacing(14);
		auto* column = new QVBoxLayout;
		EnergyRange  = ranges(*column, { tr("1h"), tr("1d"), tr("3m") }, tr("Energy range"));
		EnergyRange->setObjectName(QStringLiteral("insightsEnergyRange"));
		column->setAlignment(EnergyRange, Qt::AlignHCenter);
		EnergyPlots = new QStackedWidget{ aColumn.parentWidget() };
		EnergyChart = new insights_chart{ EnergyPlots };
		EnergyChart->setObjectName(QStringLiteral("insightsEnergyChart"));
		EnergyCalendar = new insights_calendar{ EnergyPlots };
		EnergyCalendar->setObjectName(QStringLiteral("insightsEnergyCalendar"));
		EnergyCalendar->setMinimumHeight(200);
		EnergyCalendar->setMaximumHeight(QWIDGETSIZE_MAX);
		EnergyPlots->addWidget(EnergyChart);
		EnergyPlots->addWidget(EnergyCalendar);
		column->addWidget(EnergyPlots, 1);
		column->addWidget(label(tr("Idle budgets: 2 s CPU / idle h · 6 wakeups / min"), aColumn.parentWidget(), "small"));
		pair->addLayout(column, 3);
		auto* facts = new QVBoxLayout;
		facts->setContentsMargins(0, 40, 0, 0);
		facts->setSpacing(6);
		facts->addWidget(label(tr("During the selected range"), aColumn.parentWidget(), "heading"));
		EnergyFacts = fact_table(aColumn.parentWidget(), QStringLiteral("insightsEnergyFacts"));
		EnergyFacts->setColumnCount(3);
		EnergyFacts->setHorizontalHeaderLabels({ tr("Measured"), tr("Foreground"), tr("Background") });
		EnergyFacts->horizontalHeader()->hide();
		EnergyFacts->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
		EnergyFacts->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
		EnergyFacts->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
		facts->addWidget(EnergyFacts, 1);
		pair->addLayout(facts, 2);
		aColumn.addLayout(pair, 1);
		connect(EnergyRange, &QTabBar::currentChanged, this, [this] { update_energy(); refresh_typography(); });
	}
	void insights_window::update_energy()
	{
		const auto  range   = static_cast<std::size_t>(EnergyRange->currentIndex());
		const auto& bg      = EnergySnapshot.Ranges[range][0];
		const auto  number  = [](double aValue) { return QLocale{ }.toString(aValue, 'f', 2); };
		const auto  seconds = [&](std::uint64_t aValue) { return tr("%1 s").arg(number(static_cast<double>(aValue) / 1000000.0)); };
		const auto  cpu     = core::insights_cpu_per_hour(bg);
		const auto  wakes   = core::insights_wakeups_per_minute(bg);
		EnergyValues[0]->setText(cpu ? tr("%1 s / idle h").arg(number(*cpu)) : tr("No background samples yet"));
		EnergyValues[0]->setProperty("overBudget", cpu && *cpu > 2.0);
		EnergyDetails[0]->setText(cpu ? (*cpu > 2.0 ? tr("Over budget · at most 2 s / idle h") : tr("Within budget · at most 2 s / idle h")) : tr("Background intervals will fill this"));
		EnergyValues[1]->setText(wakes ? tr("%1 / min").arg(number(*wakes)) : bg.Samples ? tr("Not reported on this platform")
																						 : tr("No background samples yet"));
		EnergyValues[1]->setProperty("overBudget", wakes && *wakes > 6.0);
		EnergyDetails[1]->setText(wakes ? (*wakes > 6.0 ? tr("Over budget · at most 6 / min") : tr("Within budget · at most 6 / min")) : QString{ });
		const auto children = bg.Children[0] + bg.Children[1];
		const auto unknown  = bg.UnmeasuredChildren[0] + bg.UnmeasuredChildren[1];
		EnergyValues[2]->setText(!children ? tr("No child CPU samples yet") : unknown ? tr("Child CPU unavailable")
																					  : seconds(bg.ChildCpuMicroseconds[0] + bg.ChildCpuMicroseconds[1]));
		EnergyDetails[2]->setText(!children ? tr("Completed gh and git processes will fill this") : unknown ? tr("%1 children without final counters").arg(unknown)
																											: tr("gh %1 · git %2").arg(energy_child(bg, 0), energy_child(bg, 1)));
		for (std::size_t index = 0; index < 3; ++index) {
			EnergyValues[index]->setProperty("insightsType", index == 0 ? (cpu ? "value" : "empty") : index == 1 ? (wakes ? "value" : "empty")
												 : children && !unknown                                          ? "value"
																												 : "empty");
			EnergyValues[index]->setToolTip(EnergyDetails[index]->text());
		}
		update_energy_facts(range);
		const auto idle = core::insights_cpu_per_hour(EnergySnapshot.Ranges[0][0]);
		SummaryIdle->setText(idle ? tr("%1 s / idle h").arg(number(*idle)) : tr("No background samples yet"));
		SummaryIdle->setProperty("insightsType", idle ? "value" : "empty");
		SummaryIdle->setProperty("overBudget", idle && *idle > 2.0);
		SummaryIdleDetail->setText(idle ? (*idle > 2.0 ? tr("Last hour · over the 2 s budget") : tr("Last hour · within the 2 s budget")) : QString{ });
		EnergyCalendar->apply_energy(EnergySnapshot);
		EnergyPlots->setCurrentIndex(range == 2 ? 1 : 0);
		update_energy_chart();
	}
	void insights_window::update_energy_facts(const std::size_t aRange)
	{
		EnergyFacts->setRowCount(8);
		const QStringList names{ tr("Measured"), tr("CPU"), tr("Wakeups"), tr("gh CPU"), tr("git CPU"), tr("Samples"), tr("Wall time"), tr("Peak CPU") };
		for (int row = 0; row < 8; ++row) {
			for (int column = 0; column < 3; ++column) {
				auto* item = EnergyFacts->item(row, column);
				if (!item) {
					item = new QTableWidgetItem;
					EnergyFacts->setItem(row, column, item);
				}
				if (column == 0) {
					item->setText(names[row]);
				} else if (row == 0) {
					item->setText(column == 1 ? tr("Foreground") : tr("Background"));
				} else {
					item->setText(energy_fact(EnergySnapshot.Ranges[aRange][column == 1 ? 1 : 0], row - 1));
				}
				item->setToolTip(item->text());
				item->setTextAlignment((column == 0 ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
				item->setForeground(palette().color(column == 0 ? QPalette::PlaceholderText : QPalette::Text));
			}
			EnergyFacts->setRowHeight(row, 25);
		}
	}

	void insights_window::update_energy_chart()
	{
		const auto          day = EnergyRange->currentIndex() == 1;
		insights_chart_data chart;
		chart.Name             = tr("Foreground and background CPU");
		chart.Empty            = tr("No energy samples yet");
		chart.Quantity         = tr("CPU seconds / sampled hour");
		chart.BackgroundName   = tr("Background");
		chart.ForegroundName   = tr("Foreground");
		chart.EventsName       = tr("Wakeups / min");
		chart.StartUnixSeconds = EnergySnapshot.Now - (day ? 86400 : 3600);
		chart.EndUnixSeconds   = EnergySnapshot.Now;
		chart.GapSeconds       = day ? 300 : 60;
		chart.Reference        = 2.0;
		chart.EventsReference  = 6.0;
		const auto& sides      = EnergySnapshot.Ranges[day ? 1 : 0];
		const auto  idle       = core::insights_cpu_per_hour(sides[0]);
		chart.Description      = idle ? tr("Background CPU cost %1 seconds per background hour over the selected range, %2 the 2-second budget; foreground is stacked above background, gaps have no samples and wakeups have a separate lane.").arg(QLocale{ }.toString(*idle, 'f', 2), *idle > 2.0 ? tr("over") : tr("within")) : tr("No background CPU samples yet; foreground and background intervals will fill this chart.");
		for (const auto& point : day ? EnergySnapshot.Day : EnergySnapshot.Hour) {
			chart.Points.push_back({ point.UnixSeconds, point.Background, point.Foreground.value_or(0), point.WakeupsPerMinute, false, point.GapBefore, point.BackgroundBudget });
		}
		EnergyChart->apply(std::move(chart));
	}

	void insights_window::build_reviews_page(QVBoxLayout& aColumn)
	{
		auto* header = new QHBoxLayout;
		CountedSince = label(tr("Counted on this computer from the first Insights launch"), aColumn.parentWidget(), "small");
		CountedSince->setObjectName(QStringLiteral("insightsCountedSince"));
		header->addWidget(CountedSince, 1);
		auto* rangeColumn = new QVBoxLayout;
		ReviewsRange      = ranges(*rangeColumn, { tr("This week"), tr("30 days"), tr("All time") }, tr("Reviews range"), 1);
		ReviewsRange->setObjectName(QStringLiteral("insightsReviewsRange"));
		header->addLayout(rangeColumn);
		aColumn.addLayout(header);
		auto*             tiles = grid(aColumn, 4);
		const QStringList titles{ tr("Reviews submitted"), tr("Files accepted"), tr("Comments posted"), tr("Private notes") };
		for (int index = 0; index < 4; ++index) {
			const auto labels                              = tile(*tiles, index, 4, titles[index], tr("No activity yet"));
			ReviewValues[static_cast<std::size_t>(index)]  = labels.Value;
			ReviewDetails[static_cast<std::size_t>(index)] = labels.Detail;
			labels.Value->setObjectName(QStringLiteral("insightsReviewCount%1").arg(index));
		}
		aColumn.addWidget(label(tr("Reviews submitted by day"), aColumn.parentWidget(), "heading"));
		ReviewCalendar = new insights_calendar{ aColumn.parentWidget() };
		aColumn.addWidget(ReviewCalendar);
		auto* sections = new QHBoxLayout;
		sections->setSpacing(24);
		for (int index = 0; index < 2; ++index) {
			auto* column = new QVBoxLayout;
			column->setSpacing(4);
			column->addWidget(label(index == 0 ? tr("How reviews ended") : tr("Records · all time"), aColumn.parentWidget(), "heading"));
			auto* table = fact_table(aColumn.parentWidget(), index == 0 ? QStringLiteral("insightsReviewEndings") : QStringLiteral("insightsReviewRecords"));
			table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			column->addWidget(table, 1);
			sections->addLayout(column, 1);
			if (index == 0) {
				ReviewEndings = table;
			} else {
				ReviewRecords = table;
			}
		}
		aColumn.addLayout(sections, 1);
		connect(ReviewsRange, &QTabBar::currentChanged, this, [this] { update_reviews(); refresh_typography(); });
	}
	void insights_window::update_reviews()
	{
		using metric       = core::insights_review_metric;
		const auto& counts = ReviewsSnapshot.Ranges[static_cast<std::size_t>(ReviewsRange->currentIndex())];
		const auto  value  = [&](metric aMetric) { return counts[static_cast<std::size_t>(aMetric)]; };
		const auto  number = [](std::uint64_t aValue) { return QLocale{ }.toString(aValue); };
		const auto  text   = [&](metric aMetric) { return value(aMetric) ? number(value(aMetric)) : tr("None yet"); };
		const auto  date   = [](std::int64_t aDay) { return QLocale{ }.toString(QDate{ 1970, 1, 1 }.addDays(aDay), QLocale::ShortFormat); };
		CountedSince->setText(ReviewsSnapshot.Available ? tr("Counted on this computer since %1").arg(date(ReviewsSnapshot.StartedDay)) : tr("Local review statistics unavailable"));
		const std::array  metrics{ metric::submitted, metric::accepted, metric::comments, metric::notes };
		const QStringList empty{ tr("No submitted reviews yet"), tr("No accepted files yet"), tr("No posted comments yet"), tr("No private notes yet") };
		for (std::size_t index = 0; index < metrics.size(); ++index) {
			ReviewValues[index]->setText(value(metrics[index]) ? number(value(metrics[index])) : empty[static_cast<int>(index)]);
			ReviewValues[index]->setProperty("insightsType", value(metrics[index]) ? "value" : "empty");
		}
		ReviewDetails[0]->setText(value(metric::opened) ? tr("of %1 opened").arg(number(value(metric::opened))) : tr("No review openings yet"));
		ReviewDetails[1]->setText(value(metric::seen) ? tr("%1 seen").arg(number(value(metric::seen))) : tr("No files seen yet"));
		ReviewDetails[2]->setText(value(metric::replies) || value(metric::suggestions) ? tr("%1 replies · %2 suggestions").arg(number(value(metric::replies)), number(value(metric::suggestions))) : tr("No replies or suggestions yet"));
		ReviewDetails[3]->setText(value(metric::defects) ? tr("%1 defects").arg(number(value(metric::defects))) : tr("No defects yet"));
		ReviewCalendar->apply(ReviewsSnapshot);
		const std::array  endings{ metric::approved, metric::changes, metric::commented, metric::resolved, metric::rounds };
		const QStringList names{ tr("Approved"), tr("Changes requested"), tr("Commented"), tr("Threads resolved"), tr("Review Cycle rounds") };
		ReviewEndings->setRowCount(5);
		for (int row = 0; row < 5; ++row) {
			fact_row(*ReviewEndings, row, names[row], text(endings[static_cast<std::size_t>(row)]));
		}
		update_review_records();
		update_summary_reviews();
	}
	void insights_window::update_review_records()
	{
		using metric      = core::insights_review_metric;
		const auto number = [](std::uint64_t aValue) { return QLocale{ }.toString(aValue); };
		const auto date   = [](std::int64_t aDay) { return QLocale{ }.toString(QDate{ 1970, 1, 1 }.addDays(aDay), QLocale::ShortFormat); };

		const auto pull = [](const core::insights_review_pr& aPull) { return QString::fromStdString(aPull.Repository) + QStringLiteral(" #%1").arg(aPull.Number); };
		ReviewRecords->setRowCount(5);
		fact_row(*ReviewRecords, 0, tr("Biggest pull request"), ReviewsSnapshot.Biggest ? tr("%1 files · %2").arg(number(ReviewsSnapshot.Biggest->Files), pull(*ReviewsSnapshot.Biggest)) : tr("No submitted reviews yet"));
		fact_row(*ReviewRecords, 1, tr("Most reviews on one PR"), ReviewsSnapshot.MostReviews ? tr("%1 · %2").arg(number(ReviewsSnapshot.MostReviews->Reviews), pull(*ReviewsSnapshot.MostReviews)) : tr("No submitted reviews yet"));
		fact_row(*ReviewRecords, 2, tr("Busiest day"), ReviewsSnapshot.Busiest ? tr("%1 · %2 reviews").arg(date(ReviewsSnapshot.Busiest->Day), number(ReviewsSnapshot.Busiest->Counts[static_cast<std::size_t>(metric::submitted)])) : tr("No submitted reviews yet"));
		const auto lines = ReviewsSnapshot.Ranges[2][static_cast<std::size_t>(metric::lines)];
		fact_row(*ReviewRecords, 3, tr("Lines read in all"), lines ? number(lines) : tr("No accepted files yet"));
		fact_row(*ReviewRecords, 4, tr("Most reviewed repository"), ReviewsSnapshot.MostReviewedRepository.empty() ? tr("No submitted reviews yet") : QString::fromStdString(ReviewsSnapshot.MostReviewedRepository));
	}
	void insights_window::update_summary_reviews()
	{
		using metric      = core::insights_review_metric;
		const auto number = [](std::uint64_t aValue) { return QLocale{ }.toString(aValue); };

		const auto& month = ReviewsSnapshot.Ranges[1];
		for (std::size_t index = 0; index < 2; ++index) {
			const auto count = month[static_cast<std::size_t>(index == 0 ? metric::submitted : metric::comments)];
			SummaryReviewValues[index]->setText(count ? number(count) : (index == 0 ? tr("No submitted reviews yet") : tr("No posted comments yet")));
			SummaryReviewValues[index]->setProperty("insightsType", count ? "value" : "empty");
			const auto detail = month[static_cast<std::size_t>(index == 0 ? metric::accepted : metric::replies)];
			SummaryReviewDetails[index]->setText(detail ? (index == 0 ? tr("%1 files accepted") : tr("%1 replies")).arg(number(detail)) : QString{ });
		}
	}

	void insights_window::build_github_page(QVBoxLayout& aColumn)
	{
		auto* tiles = grid(aColumn, 3);
		int   index = 0;
		for (const auto& title : { tr("Core REST"), tr("GraphQL"), tr("Search") }) {
			const auto result                              = tile(*tiles, index, 3, title, tr("No rate-limit reading yet"));
			GithubValues[static_cast<std::size_t>(index)]  = result.Value;
			GithubDetails[static_cast<std::size_t>(index)] = result.Detail;
			result.Value->setObjectName(QStringLiteral("insightsGithubBudget%1").arg(index));
			++index;
		}
		auto* timeline = new QHBoxLayout;
		timeline->setSpacing(14);
		auto* plot        = new QWidget{ aColumn.parentWidget() };
		auto* chartColumn = new QVBoxLayout{ plot };
		chartColumn->setContentsMargins(0, 0, 0, 0);
		chartColumn->setSpacing(8);
		GithubRange = ranges(*chartColumn, { tr("1h"), tr("1d") }, tr("GitHub range"));
		chartColumn->setAlignment(GithubRange, Qt::AlignHCenter);
		GithubChart = new insights_chart{ plot };
		GithubChart->setObjectName(QStringLiteral("insightsGithubChart"));
		chartColumn->addWidget(GithubChart, 1);
		timeline->addWidget(plot, 3);
		auto* facts = new QVBoxLayout;
		facts->setContentsMargins(0, 40, 0, 0);
		facts->setSpacing(10);
		for (int side = 0; side < 2; ++side) {
			auto* column = new QVBoxLayout;
			column->setSpacing(2);
			column->addWidget(label(side == 0 ? tr("Where BPR’s requests went") : tr("gh processes and limits"), aColumn.parentWidget(), "heading"));
			auto* table = fact_table(aColumn.parentWidget(), side == 0 ? QStringLiteral("insightsRequestCategories") : QStringLiteral("insightsProcessFacts"));
			if (side == 0) {
				RequestCategories = table;
			} else {
				ProcessFacts = table;
			}
			column->addWidget(table, 1);
			facts->addLayout(column, 1);
		}
		timeline->addLayout(facts, 2);
		aColumn.addLayout(timeline, 1);
		connect(GithubRange, &QTabBar::currentChanged, this, [this] { update_github(); });
	}
	void insights_window::set_chart_colors(QColor aPrimary, QColor aSecondary)
	{
		PrimaryChartColor   = std::move(aPrimary);
		SecondaryChartColor = std::move(aSecondary);
		static_cast<budget_bar*>(BudgetBar)->set_color(PrimaryChartColor);
		GithubChart->set_colors(PrimaryChartColor, SecondaryChartColor);
		ReviewCalendar->set_color(PrimaryChartColor);
		EnergyCalendar->set_color(PrimaryChartColor);
		EnergyChart->set_colors(PrimaryChartColor, SecondaryChartColor);
	}
	void insights_window::update_github()
	{
		const auto day = GithubRange->currentIndex() == 1;
		update_github_budgets();
		update_github_requests(day);
		update_github_processes(day);
		update_github_chart(day);
		update_problems();
		refresh_typography();
	}

	void insights_window::update_github_budgets()
	{
		static_cast<budget_bar*>(BudgetBar)->apply(GithubSnapshot.Budgets[0]);
		for (std::size_t index = 0; index < GithubValues.size(); ++index) {
			const auto& budget = GithubSnapshot.Budgets[index];
			GithubValues[index]->setText(budget.Remaining ? QLocale{ }.toString(*budget.Remaining) : tr("No current reading"));
			GithubValues[index]->setProperty("insightsType", budget.Remaining ? "value" : "empty");
			GithubDetails[index]->setText(budget.Limit ? tr("left of %1 · BPR %2 · resets %3").arg(QLocale{ }.toString(*budget.Limit), QLocale{ }.toString(budget.OwnRequests), QLocale{ }.toString(QDateTime::fromSecsSinceEpoch(budget.ResetUnixSeconds).time(), QLocale::ShortFormat)) : tr("An ordinary request will fill this"));
			GithubDetails[index]->setToolTip(index == 1 ? tr("GraphQL is charged by query cost. BPR counts requests; no other-clients split is inferred.") : QString{ });
		}
	}

	void insights_window::update_github_requests(const bool aDay)
	{
		const auto  names       = category_names();
		const auto& counts      = aDay ? GithubSnapshot.DayRequestsByCategory : GithubSnapshot.RequestsByCategory;
		const auto  anyRequests = std::accumulate(counts.begin(), counts.end(), std::uint64_t{ 0 }) != 0;
		RequestCategories->setRowCount(anyRequests ? 5 : 1);
		if (anyRequests) {
			for (int index = 0; index < 5; ++index) {
				fact_row(*RequestCategories, index, names[index], QLocale{ }.toString(counts[static_cast<std::size_t>(index)]));
			}
		} else {
			fact_row(*RequestCategories, 0, tr("Ordinary requests will fill this"), { });
		}
	}

	void insights_window::update_github_processes(const bool aDay)
	{
		const auto  processes   = aDay ? GithubSnapshot.DayProcesses : GithubSnapshot.Processes;
		const auto& failures    = aDay ? GithubSnapshot.DayProcessFailures : GithubSnapshot.ProcessFailures;
		const auto  quarantines = aDay ? GithubSnapshot.DayQuarantines : GithubSnapshot.Quarantines;
		const auto  abandoned   = aDay ? GithubSnapshot.DayAbandoned : GithubSnapshot.Abandoned;
		const auto  slowest     = aDay ? GithubSnapshot.DaySlowestMicroseconds : GithubSnapshot.SlowestMicroseconds;
		ProcessFacts->setRowCount(0);
		const auto addFact = [this](const QString& aName, const QString& aValue) { const auto row = ProcessFacts->rowCount(); ProcessFacts->setRowCount(row+1); fact_row(*ProcessFacts,row,aName,aValue); };
		if (processes || slowest || quarantines || abandoned) {
			addFact(tr("Launched"), QLocale{ }.toString(processes));
			const QStringList failureNames{ tr("Succeeded"), tr("Start failed"), tr("Timed out"), tr("Cancelled"), tr("Output limit"), tr("Crashed"), tr("Nonzero exit"), tr("Reap failed"), tr("Other failure") };
			for (std::size_t index = 1; index < failures.size(); ++index) {
				if (failures[index] != 0) {
					addFact(failureNames[static_cast<int>(index)], QLocale{ }.toString(failures[index]));
				}
			}
			addFact(tr("Quarantined lanes"), QLocale{ }.toString(quarantines));
			addFact(tr("Abandoned children"), QLocale{ }.toString(abandoned));
			addFact(tr("Slowest call"), slowest ? tr("%1 s").arg(QLocale{ }.toString(static_cast<double>(*slowest) / 1000000.0, 'f', 2)) : tr("No completed call"));
		} else {
			addFact(tr("No gh process observations yet"), { });
		}
		bool secondary = false;
		for (const auto& event : GithubSnapshot.SecondaryLimits) {
			if (!aDay && event.UnixSeconds < BuiltUnixSeconds - 3600) {
				continue;
			}
			const auto time = QLocale{ }.toString(QDateTime::fromSecsSinceEpoch(event.UnixSeconds).time(), QLocale::ShortFormat);
			addFact(tr("%1 · %2").arg(time, event.SecondaryLimited ? tr("Secondary limit") : tr("Retry-After")), event.RetryAfterSeconds ? tr("%1 s").arg(*event.RetryAfterSeconds) : tr("Reported"));
			secondary = true;
		}
		if (!secondary) {
			addFact(tr("No secondary-limit events"), { });
		}
	}

	void insights_window::update_github_chart(const bool aDay)
	{
		insights_chart_data chart;
		chart.Name             = tr("Core REST budget and BPR requests");
		chart.Quantity         = tr("Core REST remaining");
		chart.BackgroundName   = tr("Remaining");
		chart.EventsName       = tr("BPR requests/min");
		chart.Empty            = tr("Ordinary GitHub requests will fill this chart.");
		chart.StartUnixSeconds = BuiltUnixSeconds - (aDay ? 86400 : 3600);
		chart.EndUnixSeconds   = BuiltUnixSeconds;
		chart.GapSeconds       = aDay ? 450 : 90;
		const auto& points     = aDay ? GithubSnapshot.Day : GithubSnapshot.Hour;
		for (const auto& point : points) {
			chart.Points.push_back({ point.UnixSeconds, point.Remaining, 0.0, point.RequestsPerMinute, point.Reset });
		}
		const auto readings = std::ranges::count_if(points, [](const auto& aPoint) { return aPoint.Remaining.has_value(); });
		chart.Description   = readings ? tr("Core REST remaining budget over the last %1, with BPR requests per minute above; missing observations are gaps and vertical rules mark resets.").arg(aDay ? tr("day") : tr("hour")) : tr("No Core REST budget readings in the selected range; ordinary requests supply them.");
		GithubChart->apply(std::move(chart));
	}

	void insights_window::update_problems()
	{
		const auto problems = std::accumulate(GithubSnapshot.ProblemsByCategory.begin(), GithubSnapshot.ProblemsByCategory.end(), std::uint64_t{ 0 });
		Problems->setText(GithubSnapshot.Available ? QLocale{ }.toString(problems) : tr("No problem tallies yet"));
		Problems->setProperty("insightsType", GithubSnapshot.Available ? "value" : "empty");
		QStringList       details;
		const QStringList shortNames{ tr("inbox"), tr("opening"), tr("conversations"), tr("publishing"), tr("other") };
		for (std::size_t index = 0; index < GithubSnapshot.ProblemsByCategory.size(); ++index) {
			if (GithubSnapshot.ProblemsByCategory[index] != 0) {
				details.push_back(QLocale{ }.toString(GithubSnapshot.ProblemsByCategory[index]) + QLatin1Char{ ' ' } + shortNames[static_cast<int>(index)]);
			}
		}
		ProblemsDetail->setText(problems ? details.join(QStringLiteral(", ")) : (GithubSnapshot.Available ? tr("No failed operations") : QString{ }));
		ProblemsDetail->setToolTip(details.join(QStringLiteral(", ")));
	}

	void insights_window::build_empty_page(int aIndex, QVBoxLayout& aColumn)
	{
		if (aIndex == 4) {
			auto* tiles = grid(aColumn, 3);
			tile(*tiles, 0, 3, tr("Input response, p95"), tr("No event-loop timings yet"));
			tile(*tiles, 1, 3, tr("Files diffed"), tr("No diff timings yet"));
			tile(*tiles, 2, 3, tr("Syntax cache"), tr("No syntax observations yet"));
			ranges(aColumn, { tr("1h"), tr("1d") }, tr("Work range"));
			empty_section(aColumn, tr("Slowest files to diff"), tr("Diff work on this computer will fill this view."));
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
		hide_native_title_text(*this);
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
		GithubSnapshot   = aSnapshot.Github;
		ReviewsSnapshot  = aSnapshot.Reviews;
		EnergySnapshot   = aSnapshot.Energy;
		update_energy();
		update_reviews();
		update_github();
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
