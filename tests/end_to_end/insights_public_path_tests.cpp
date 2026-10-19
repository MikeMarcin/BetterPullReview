#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/insights_storage.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/insights_calendar.hpp"
#include "bpr/qt/insights_chart.hpp"
#include "bpr/qt/insights_window.hpp"
#include "bpr/qt/settings_window.hpp"
#include "private_data_root.hpp"
#include <QDateTime>
#include <QEventLoop>
#include <QTimeZone>
#include <sqlite3.h>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>

namespace {
	using namespace bpr;
	using tests::expect;
	void until(const std::function<bool()>& aCondition, const char* aMessage)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(2);
		}
		expect(aCondition(), aMessage);
	}

	auto insights() -> qt::insights_window*
	{
		for (auto* widget : QApplication::topLevelWidgets()) {
			if (auto* shell = dynamic_cast<qt::insights_window*>(widget)) {
				return shell;
			}
		}
		return nullptr;
	}

	void seed_work(const std::filesystem::path& aRoot)
	{
		auto                                    store = adapters::sqlite::make_insights_storage(aRoot, [] { return std::vector<application::insights_storage_row>{ }; });
		std::vector<core::insights_work_sample> samples;
		const auto                              add = [&](core::insights_work_kind aKind, std::uint64_t aDuration, std::uint64_t aCount = 1) {core::insights_work_sample sample; sample.At=QDateTime::currentSecsSinceEpoch()-60; sample.Kind=aKind; sample.DurationMicroseconds=aDuration; sample.Count=aCount; samples.push_back(sample); };
		for (std::uint64_t index = 0; index < 7; ++index) {
			add(core::insights_work_kind::diff, 10000 * (index + 1));
			samples.back().Path         = "src/work/file-" + std::to_string(index) + ".cpp";
			samples.back().WorkLimit    = index == 0;
			samples.back().SegmentLimit = index == 1;
			samples.back().Approximate  = index == 2;
		}
		add(core::insights_work_kind::syntax_hit, 0, 9);
		add(core::insights_work_kind::syntax_miss, 0, 3);
		add(core::insights_work_kind::syntax_eviction, 0, 2);
		add(core::insights_work_kind::search, 10000);
		add(core::insights_work_kind::search, 20000);
		add(core::insights_work_kind::search, 30000);
		store->record_work(samples);
	}

	void seed_reviews(const std::filesystem::path& aRoot)
	{
		auto                                     store = adapters::sqlite::make_insights_storage(aRoot, [] { return std::vector<application::insights_storage_row>{ }; });
		std::vector<core::insights_review_event> events;
		const auto                               today = QDate::currentDate();
		const auto                               add   = [&](core::insights_review_kind aKind, int aOffset, const std::string& aIdentity, std::uint64_t aPull = 7, std::uint64_t aFiles = 125, std::uint8_t aDisposition = 0, bool aSuggestion = false, std::uint64_t aLines = 0) {
			core::insights_review_event event;
			event.Kind         = aKind;
			event.Host         = "github.com";
			event.Repository   = aPull == 9 ? "owner/largest" : aPull == 10 ? "owner/older"
																			: "owner/repo";
			event.PullRequest  = aPull;
			event.FilesChanged = aFiles;
			event.Identity     = aIdentity;
			event.Disposition  = aDisposition;
			event.Suggestion   = aSuggestion;
			event.Lines        = aLines;
			event.UnixSeconds  = QDateTime{ today.addDays(aOffset), QTime{ 12, 0 }, QTimeZone::systemTimeZone() }.toSecsSinceEpoch();
			event.LocalDay     = QDate{ 1970, 1, 1 }.daysTo(today.addDays(aOffset));
			if (aKind == core::insights_review_kind::seen || aKind == core::insights_review_kind::accepted) {
				event.Head = "fixture-head";
			}
			events.push_back(std::move(event));
		};
		using enum core::insights_review_kind;
		add(submitted, 0, "5001");
		add(submitted, 0, "5002");
		add(submitted, 0, "5003");
		add(submitted, -1, "5004", 8, 10, 1);
		add(submitted, -7, "5005", 7, 125, 2);
		add(submitted, -29, "5006", 9, 1700);
		add(submitted, -30, "5007", 9, 1700, 1);
		add(submitted, -100, "5008", 10, 50, 2);
		add(submitted, -170, "5009", 10, 50);
		add(opened, 0, "opened");
		add(opened, -1, "opened");
		add(accepted, 0, "src/one.cpp", 7, 125, 0, false, 55);
		add(accepted, -29, "src/two.cpp", 7, 125, 0, false, 120);
		add(accepted, -30, "src/three.cpp", 7, 125, 0, false, 30);
		add(seen, 0, "src/one.cpp");
		add(seen, -1, "src/extra.cpp");
		add(seen, -29, "src/two.cpp");
		add(seen, -30, "src/three.cpp");
		add(comment, 0, "comment1", 7, 125, 0, true);
		add(comment, 0, "comment2");
		add(comment, 0, "comment3");
		add(comment, -30, "comment4");
		add(reply, -1, "reply1", 7, 125, 0, true);
		add(reply, -1, "reply2");
		add(note, 0, "note1");
		add(defect, -1, "defect1");
		add(round, 0, "round1");
		add(resolved, -1, "resolve1");
		store->record_reviews(events);
		store->record_reviews(events);
		store.reset();
		sqlite3* database = nullptr;
		expect(sqlite3_open((aRoot / "state/v1/insights.sqlite3").string().c_str(), &database) == SQLITE_OK, "seeded store reopens for its historical start date");
		const auto started = QDateTime{ today.addDays(-180), QTime{ 0, 0 }, QTimeZone::systemTimeZone() }.toSecsSinceEpoch();
		const auto sql     = "UPDATE insights_metadata SET started_unix_seconds=" + std::to_string(started);
		expect(sqlite3_exec(database, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "fixture start date matches its historical activity");
		sqlite3_close(database);
	}
	class recorded_energy_metrics final : public application::process_metrics_port
	{
	public:
		auto read() -> std::optional<core::insights_process_counters> override
		{
			// The fixture's Energy history is seeded below. It has no unrecorded native interval.
			return std::nullopt;
		}
	};

	void seed_energy(const std::filesystem::path& aRoot)
	{
		auto                                      store = adapters::sqlite::make_insights_storage(aRoot, [] { return std::vector<application::insights_storage_row>{ }; });
		const auto                                now   = QDateTime::currentSecsSinceEpoch();
		std::vector<core::insights_energy_sample> samples;
		for (int minute = 1; minute < 59; ++minute) {
			if (minute >= 20 && minute < 25) {
				continue;
			}
			for (const bool foreground : { false, true }) {
				core::insights_energy_sample sample;
				sample.AtMicroseconds         = (now - minute * 60 - (foreground ? 0 : 30)) * 1000000;
				sample.LocalDay               = application::local_insights_day(sample.AtMicroseconds / 1000000);
				sample.Foreground             = foreground;
				sample.Samples                = 1;
				sample.WallMicroseconds       = 30000000;
				sample.CpuMicroseconds        = foreground ? 1000000 : 33334;
				sample.Wakeups                = foreground ? 60 : 6;
				sample.WakeupWallMicroseconds = 30000000;
				sample.PeakLoad               = static_cast<double>(sample.CpuMicroseconds) / 30000000.0;
				if (minute == 1) {
					const auto kind                   = foreground ? 1U : 0U;
					sample.Children[kind]             = 1;
					sample.ChildCpuMicroseconds[kind] = foreground ? 1000000 : 250000;
				}
				samples.push_back(sample);
			}
		}
		for (int day = 1; day < 75; ++day) {
			core::insights_energy_sample sample;
			sample.AtMicroseconds         = (now - day * 86400LL) * 1000000;
			sample.LocalDay               = application::local_insights_day(sample.AtMicroseconds / 1000000);
			sample.Samples                = 1;
			sample.WallMicroseconds       = 1800000000;
			sample.CpuMicroseconds        = static_cast<std::uint64_t>(day % 9 + 1) * 250000;
			sample.Wakeups                = static_cast<std::uint64_t>(day * 5);
			sample.WakeupWallMicroseconds = sample.WallMicroseconds;
			samples.push_back(sample);
		}
		store->record_energy(samples);
	}
	void energy_contract(qt::insights_window& aShell)
	{
		auto* tabs = aShell.findChild<QTabBar*>(QStringLiteral("insightsTabs"));
		tabs->setCurrentIndex(5);
		auto* range = aShell.findChild<QTabBar*>(QStringLiteral("insightsEnergyRange"));
		range->setCurrentIndex(0);
		auto* cpu = aShell.findChild<QLabel*>(QStringLiteral("insightsEnergy0"));
		until([&] { return cpu->text().startsWith(QStringLiteral("4.00")); }, "real retained background cost reaches the public Energy page");
		expect(cpu->property("overBudget").toBool() && aShell.findChild<QLabel*>(QStringLiteral("insightsEnergy1"))->text().startsWith(QStringLiteral("12.00")), "background-only CPU and wakeup tiles mark the seeded over-budget range");
		expect(aShell.findChild<QLabel*>(QStringLiteral("insightsEnergy2"))->text().contains(QStringLiteral("0.25")), "background child tile excludes the foreground Git CPU");
		auto* facts = aShell.findChild<QTableWidget*>(QStringLiteral("insightsEnergyFacts"));
		expect(facts->columnCount() == 3 && facts->horizontalHeaderItem(1)->text() == QStringLiteral("Foreground") && facts->horizontalHeaderItem(2)->text() == QStringLiteral("Background"), "every Energy fact has separate foreground and background columns");
		for (int row = 0; row < facts->rowCount(); ++row) {
			expect(facts->item(row, 1) && facts->item(row, 2), "each paired fact supplies both states");
		}
		expect(facts->item(1, 1)->text() != facts->item(1, 2)->text() && facts->item(3, 1)->text() == QStringLiteral("No children") && facts->item(4, 2)->text() == QStringLiteral("No children"), "foreground activity cannot leak into background CPU or either child fact");
		auto* chart = dynamic_cast<qt::insights_chart*>(aShell.findChild<QWidget*>(QStringLiteral("insightsEnergyChart")));
		expect(chart->accessibleDescription().contains(QStringLiteral("over")) && chart->accessibleDescription().contains(QStringLiteral("seconds per background hour")), "Energy chart describes measured idle cost against the budget");
		range->setCurrentIndex(1);
		expect(chart->isVisible(), "one day uses the shared lane chart");
		range->setCurrentIndex(2);
		auto* calendar = dynamic_cast<qt::insights_calendar*>(aShell.findChild<QWidget*>(QStringLiteral("insightsEnergyCalendar")));
		expect(calendar->isVisible() && calendar->accessibleDescription().contains(QStringLiteral("background CPU")), "three months uses the local-day CPU-size/background-intensity calendar");
		range->setCurrentIndex(0);
		tabs->setCurrentIndex(0);
		expect(aShell.findChild<QLabel*>(QStringLiteral("insightsSummaryIdle"))->text() == cpu->text() && aShell.findChild<QLabel*>(QStringLiteral("insightsSummaryIdle"))->property("overBudget").toBool(), "Summary idle cost uses the last-hour background measurement");
		application::insights_snapshot unsupported;
		core::insights_energy_sample   zero;
		zero.AtMicroseconds       = QDateTime::currentSecsSinceEpoch() * 1000000;
		zero.LocalDay             = application::local_insights_day(zero.AtMicroseconds / 1000000);
		zero.Samples              = 1;
		zero.WallMicroseconds     = 60000000;
		zero.Children             = { 2, 2 };
		zero.UnmeasuredChildren   = { 1, 1 };
		zero.ChildCpuMicroseconds = { 123000, 456000 };
		unsupported.Energy        = core::build_insights_energy({ zero }, zero.AtMicroseconds / 1000000, zero.LocalDay);
		qt::insights_window unsupportedWindow;
		unsupportedWindow.apply(unsupported);
		expect(unsupportedWindow.findChild<QLabel*>(QStringLiteral("insightsEnergy1"))->text() == QStringLiteral("Not reported on this platform"), "an absent wakeup observation never becomes a measured zero in Qt");
		expect(unsupportedWindow.findChild<QLabel*>(QStringLiteral("insightsEnergy0"))->text() == QStringLiteral("0.00 s / idle h"), "an actual zero-CPU interval remains a measured value");
		expect(unsupportedWindow.findChild<QLabel*>(QStringLiteral("insightsEnergy2"))->text() == QStringLiteral("Not reported"), "an unsupported child counter cannot display a partial sum or measured zero");
		auto* unsupportedFacts = unsupportedWindow.findChild<QTableWidget*>(QStringLiteral("insightsEnergyFacts"));
		expect(unsupportedFacts->item(3, 2)->text() == QStringLiteral("Not reported") && unsupportedFacts->item(4, 2)->text() == QStringLiteral("Not reported") && unsupportedFacts->item(3, 1)->text() == QStringLiteral("No children"), "gh and Git facts preserve unsupported counters separately from no children");
		const auto folder = qEnvironmentVariable("BPR_INSIGHTS_RENDER_ROOT");
		if (!folder.isEmpty()) {
			unsupportedWindow.findChild<QTabBar*>(QStringLiteral("insightsTabs"))->setCurrentIndex(5);
			unsupportedWindow.present(nullptr);
			QApplication::processEvents();
			expect(unsupportedWindow.grab().save(folder + QStringLiteral("/insights-energy-not-reported.png")), "unsupported child wording renders on the actual Energy surface");
		}
	}

	void reviews_contract(qt::insights_window& aShell, const std::filesystem::path& aRoot)
	{
		auto* tabs = aShell.findChild<QTabBar*>(QStringLiteral("insightsTabs"));
		tabs->setCurrentIndex(1);
		auto* range     = aShell.findChild<QTabBar*>(QStringLiteral("insightsReviewsRange"));
		auto* submitted = aShell.findChild<QLabel*>(QStringLiteral("insightsReviewCount0"));
		range->setCurrentIndex(1);
		until([&] { return submitted->text() == QStringLiteral("6"); }, "Reviews30 days reads six deduplicated submissions from the seeded real store");
		expect(aShell.findChild<QLabel*>(QStringLiteral("insightsReviewCount1"))->text() == QStringLiteral("2") && aShell.findChild<QLabel*>(QStringLiteral("insightsReviewCount2"))->text() == QStringLiteral("3") && aShell.findChild<QLabel*>(QStringLiteral("insightsReviewCount3"))->text() == QStringLiteral("1"), "accepted files, comments and notes match the seeded range");
		expect(aShell.findChild<QLabel*>(QStringLiteral("insightsCountedSince"))->text().contains(QStringLiteral("Counted on this computer since")), "since-date is visible");
		auto* endings = aShell.findChild<QTableWidget*>(QStringLiteral("insightsReviewEndings"));
		expect(endings->item(0, 1)->text() == QStringLiteral("4") && endings->item(1, 1)->text() == QStringLiteral("1") && endings->item(2, 1)->text() == QStringLiteral("1") && endings->item(3, 1)->text() == QStringLiteral("1") && endings->item(4, 1)->text() == QStringLiteral("1"), "How reviews ended uses the selected range");
		range->setCurrentIndex(2);
		expect(submitted->text() == QStringLiteral("9"), "all-time includes retained activity before30 days");
		auto* records = aShell.findChild<QTableWidget*>(QStringLiteral("insightsReviewRecords"));
		expect(records->item(0, 1)->text().contains(QStringLiteral("1,700")) && records->item(0, 1)->text().contains(QStringLiteral("owner/largest #9")) && records->item(1, 1)->text().contains(QStringLiteral("4")) && records->item(2, 1)->text().contains(QStringLiteral("3 reviews")) && records->item(3, 1)->text() == QStringLiteral("205") && records->item(4, 1)->text() == QStringLiteral("owner/repo"), "all five lifetime records match submitted-PR and file facts");
		range->setCurrentIndex(0);
		const auto week = QDate::currentDate().dayOfWeek() > 1 ? 4 : 3;
		expect(submitted->text() == QString::number(week), "This week starts on local Monday");
		auto* calendar = dynamic_cast<qt::insights_calendar*>(aShell.findChild<QWidget*>(QStringLiteral("insightsReviewCalendar")));
		expect(calendar && calendar->accessibleDescription().contains(QStringLiteral("9 reviews")), "the26-week calendar exposes its actual activity description");

		const auto raster = calendar->grab().toImage();
		const auto day    = QDate::currentDate().dayOfWeek() - 1;
		const auto sample = QPoint{ static_cast<int>(34.0 + 25.5 * static_cast<double>(calendar->width() - 44) / 26.0), static_cast<int>(24.0 + (static_cast<double>(day) + 0.5) * static_cast<double>(calendar->height() - 44) / 7.0) };
		const auto pixel  = [](const QImage& aImage, QPoint aPoint) { return aImage.pixelColor(static_cast<int>(aPoint.x() * aImage.devicePixelRatio()), static_cast<int>(aPoint.y() * aImage.devicePixelRatio())); };
		calendar->apply({ });
		const auto absent = calendar->grab().toImage();
		expect(pixel(raster, sample) != pixel(absent, sample), "negative control: removing calendar activity removes today's plotted dot");
		auto store = adapters::sqlite::make_insights_storage(aRoot, [] { return std::vector<application::insights_storage_row>{ }; });
		calendar->apply(store->reviews(QDateTime::currentSecsSinceEpoch()));
		const auto  local = QPointF{ sample };
		QMouseEvent hover{ QEvent::MouseMove, local, calendar->mapToGlobal(sample), Qt::NoButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(calendar, &hover);
		const auto folder = qEnvironmentVariable("BPR_INSIGHTS_RENDER_ROOT");
		if (!folder.isEmpty()) {
			expect(aShell.grab().save(folder + QStringLiteral("/insights-reviews-hover.png")), "current-day calendar hover is captured from the real window");
		}
		QEvent leave{ QEvent::Leave };
		QApplication::sendEvent(calendar, &leave);
		for (auto* item : aShell.findChildren<QLabel*>()) {
			expect(!item->text().contains(QStringLiteral("streak"), Qt::CaseInsensitive), "no streak is shown");
		}
		range->setCurrentIndex(1);
		tabs->setCurrentIndex(0);
		expect(aShell.findChild<QLabel*>(QStringLiteral("insightsSummaryReviews"))->text() == QStringLiteral("6") && aShell.findChild<QLabel*>(QStringLiteral("insightsSummaryComments"))->text() == QStringLiteral("3"), "both Summary review tiles use30-day statistics");
	}

	void shared_chart_contract(QWidget& aParent)
	{
		qt::insights_chart chart{ &aParent };
		chart.resize(600, 160);
		chart.set_colors(QColor{ 20, 150, 95 }, QColor{ 40, 100, 220 });
		qt::insights_chart_data facts;
		facts.Name             = QStringLiteral("Stacked foreground and background");
		facts.Description      = QStringLiteral("Background under foreground with gaps.");
		facts.Quantity         = QStringLiteral("System capacity, %");
		facts.BackgroundName   = QStringLiteral("Background");
		facts.ForegroundName   = QStringLiteral("Foreground");
		facts.EventsName       = QStringLiteral("Events/min");
		facts.StartUnixSeconds = 0;
		facts.EndUnixSeconds   = 300;
		facts.GapSeconds       = 90;
		facts.Capacity         = 100.0;
		facts.Points           = { { 0, 10.0, 20.0, 2.0, false }, { 60, 10.0, 20.0, 3.0, false }, { 300, 10.0, 20.0, 1.0, true } };
		chart.apply(facts);
		expect(chart.quantity_maximum() == 100.0, "capacity lane stays at 100 until exceeded");
		const auto move = [&](const double aX) { const auto local = QPointF{aX,90.0}; QMouseEvent event{QEvent::MouseMove,local,chart.mapToGlobal(local.toPoint()),Qt::NoButton,Qt::NoButton,Qt::NoModifier}; QApplication::sendEvent(&chart,&event); };
		move(67.0);
		expect(chart.hovered_index() == 0, "hover stays with the old sample before its midpoint");
		move(69.0);
		expect(chart.hovered_index() == 1, "hover advances after crossing the midpoint");
		move(590.0);
		expect(chart.hovered_index() == 2 && chart.hover_card_rect().left() >= 8.0 && chart.hover_card_rect().right() <= 592.0 && chart.hover_card_rect().bottom() <= 152.0, "hover flips at the trailing and bottom edges with an eight-pixel inset");
		move(10.0);
		expect(chart.hover_card_rect().left() >= 8.0, "leading hover respects the inset");
		QEvent leave{ QEvent::Leave };
		QApplication::sendEvent(&chart, &leave);
		chart.hide();
		const auto raster = chart.grab().toImage();
		const auto pixel  = [](const QImage& aImage, const int aX, const int aY) { return aImage.pixelColor(static_cast<int>(aX * aImage.devicePixelRatio()), static_cast<int>(aY * aImage.devicePixelRatio())); };
		expect(pixel(raster, 115, 40) != pixel(raster, 300, 40), "the events lane draws interval bars and leaves unsampled time empty");
		expect(pixel(raster, 60, 115) != pixel(raster, 300, 115), "foreground is filled above background, and the unsampled interval is a gap");
		auto noEvents = facts;
		for (auto& point : noEvents.Points) {
			point.Events.reset();
		}
		chart.apply(noEvents);
		const auto noEventsRaster = chart.grab().toImage();
		expect(pixel(noEventsRaster, 115, 40) == pixel(noEventsRaster, 300, 40), "negative control: removing events removes the bars");
		auto empty = facts;
		for (auto& point : empty.Points) {
			point.Background.reset();
		}
		chart.apply(empty);
		const auto emptyRaster = chart.grab().toImage();
		expect(pixel(emptyRaster, 60, 115) == pixel(emptyRaster, 300, 115), "negative control: without quantity observations the area disappears");
		auto segmented = facts;
		segmented.ForegroundName.clear();
		segmented.Points = { { 0, 10.0, 0.0, std::nullopt, false }, { 180, 10.0, 0.0, std::nullopt, false }, { 240, 30.0, 0.0, std::nullopt, false }, { 300, 10.0, 0.0, std::nullopt, false } };
		chart.apply(segmented);
		const auto afterIsolated = chart.grab().toImage();
		segmented.Points.erase(segmented.Points.begin());
		chart.apply(segmented);
		const auto withoutIsolated = chart.grab().toImage();
		expect(pixel(afterIsolated, 474, 118) == pixel(withoutIsolated, 474, 118), "an isolated sample must not change the fill of a later continuous run");
		const auto renderRoot = qEnvironmentVariable("BPR_INSIGHTS_RENDER_ROOT");
		if (!renderRoot.isEmpty()) {
			expect(QDir{ }.mkpath(renderRoot), "chart render folder exists");
			expect(afterIsolated.save(renderRoot + QStringLiteral("/chart-isolated-then-continuous.png")) && withoutIsolated.save(renderRoot + QStringLiteral("/chart-continuous-only.png")), "painter-state regression renders saved");
		}
		facts.Points.back().Foreground = 120.0;
		facts.EndUnixSeconds           = 600;
		chart.apply(facts);
		expect(chart.quantity_maximum() == 130.0 && !chart.hovered_index(), "exceeded capacity expands, and changing the range clears hover");
		expect(!chart.accessibleName().isEmpty() && !chart.accessibleDescription().isEmpty(), "shared chart has a range description");
	}

	void render(qt::insights_window& aShell, app::github_application& aRuntime)
	{
		const auto folder = qEnvironmentVariable("BPR_INSIGHTS_RENDER_ROOT");
		if (folder.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(folder), "render folder exists");
		const auto original = aShell.size();
		aShell.setFixedSize(qt::insights_window::content_size({ 1280, 1000 }));
		for (const auto& [mode, suffix] : { std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
			static_cast<void>(app::detail::github_application_fixture_access::select_appearance_mode(aRuntime, mode));
			QApplication::processEvents();
			auto* tabs = aShell.findChild<QTabBar*>(QStringLiteral("insightsTabs"));
			for (int page = 0; page < tabs->count(); ++page) {
				tabs->setCurrentIndex(page);
				QApplication::processEvents();
				expect(aShell.grab().save(folder + QStringLiteral("/insights-%1-%2.png").arg(page).arg(suffix)), "composed Insights render saved");
				if (page == 1) {
					auto* range = aShell.findChild<QTabBar*>(QStringLiteral("insightsReviewsRange"));
					for (int selected = 0; selected < 3; ++selected) {
						range->setCurrentIndex(selected);
						QApplication::processEvents();
						expect(aShell.grab().save(folder + QStringLiteral("/insights-reviews-%1-%2.png").arg(selected).arg(suffix)), "all real Reviews ranges render");
					}
					range->setCurrentIndex(1);
				}
				if (page == 5) {
					auto* range = aShell.findChild<QTabBar*>(QStringLiteral("insightsEnergyRange"));
					for (int selected = 0; selected < 3; ++selected) {
						range->setCurrentIndex(selected);
						QApplication::processEvents();
						expect(aShell.grab().save(folder + QStringLiteral("/insights-energy-%1-%2.png").arg(selected).arg(suffix)), "all Energy ranges render through the real window");
					}
					range->setCurrentIndex(0);
				}

				if (page == 2) {
					auto* range = aShell.findChild<QTabBar*>(QStringLiteral("insightsRange"));
					for (auto* candidate : aShell.findChildren<QTabBar*>()) {
						if (candidate->accessibleName() == QStringLiteral("GitHub range")) {
							range = candidate;
						}
					}
					for (int selected = 0; selected < 2; ++selected) {
						range->setCurrentIndex(selected);
						QApplication::processEvents();
						expect(aShell.grab().save(folder + QStringLiteral("/insights-github-%1-%2.png").arg(selected).arg(suffix)), "both real GitHub ranges render");
						auto*       chart = dynamic_cast<qt::insights_chart*>(aShell.findChild<QWidget*>(QStringLiteral("insightsGithubChart")));
						const auto  local = QPointF{ static_cast<double>(chart->width()) * 0.8, static_cast<double>(chart->height()) * 0.65 };
						QMouseEvent hover{ QEvent::MouseMove, local, chart->mapToGlobal(local.toPoint()), Qt::NoButton, Qt::NoButton, Qt::NoModifier };
						QApplication::sendEvent(chart, &hover);
						expect(aShell.grab().save(folder + QStringLiteral("/insights-github-hover-%1-%2.png").arg(selected).arg(suffix)), "ATT-style hover renders on the real GitHub chart");
						QEvent leave{ QEvent::Leave };
						QApplication::sendEvent(chart, &leave);
					}
				}
			}
		}
		aShell.setFixedSize(original);
	}
}

int main(int aArgc, char** aArgv)
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary{ qEnvironmentVariable("QUALIFICATION_TARGET_ROOT", QDir::tempPath()) + QStringLiteral("/insights-XXXXXX") };
	expect(temporary.isValid(), "private course parent created");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private course profile root admitted");
	seed_reviews(root);
	seed_energy(root);
	seed_work(root);
	app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot      = root;
	options.InsightsMetrics          = std::make_shared<recorded_energy_metrics>();
	options.RegisteredDeviceCourse   = true;
	auto                     desktop = app::detail::github_application_fixture_access::start_desktop(app::detail::github_public_path_fixture::insights_budget_v1, options, [](auto) { });
	app::github_application* runtime = nullptr;
	until([&] { runtime = desktop->runtime(core::default_bpr_profile_id); return runtime && !runtime->windows().empty(); }, "profile runtime and window composed");

	QEventLoop latencyCourse;
	QTimer::singleShot(20, &latencyCourse, [] { QThread::msleep(250); });
	QTimer::singleShot(400, &latencyCourse, &QEventLoop::quit);
	latencyCourse.exec();
	auto* window = runtime->window(runtime->windows().front());
	window->show();
	auto* action = window->findChild<QAction*>(QStringLiteral("openInsightsAction"));
	expect(action && action->shortcut() == QKeySequence{ Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_I }, "the public menu entry carries the agreed shortcut");
#if defined(Q_OS_MACOS)
	std::cout << "insights-menu platform=" << QApplication::platformName().toStdString() << " role=" << static_cast<int>(action->menuRole()) << " associations=" << action->associatedObjects().size() << '\n';
	if (QApplication::platformName() == QStringLiteral("cocoa")) {
		const auto associations = action->associatedObjects();
		expect(action->menuRole() == QAction::NoRole && std::ranges::none_of(associations, [](const QObject* aObject) { return qobject_cast<const QMenu*>(aObject) != nullptr; }),
			   "Cocoa keeps the Insights shortcut donor outside per-window native menu merging");
	} else {
		const auto associations = action->associatedObjects();
		const auto inHelp       = std::ranges::any_of(associations, [](const QObject* aObject) {
			const auto* menu = qobject_cast<const QMenu*>(aObject);
			return menu != nullptr && QString{ menu->title() }.remove(QLatin1Char{ '&' }) == QStringLiteral("Help");
		});
		if (!inHelp) {
			std::cerr << "insights-menu platform=" << QApplication::platformName().toStdString() << " role=" << static_cast<int>(action->menuRole()) << " associations=" << associations.size() << '\n';
		}
		expect(inHelp, "offscreen macOS retains Insights in the ordinary Help menu");
	}
#endif
	action->trigger();
	until([] { return insights() && insights()->isVisible(); }, "menu entry opens the real Insights window");
	auto* shell = insights();
	action->trigger();
	expect(insights() == shell, "repeated open reuses one per-profile instance");

	until([&] { return shell->findChild<QLabel*>(QStringLiteral("insightsWork1"))->text() == QStringLiteral("7"); }, "Work fixture snapshot is ready");
	const auto debugWork = adapters::sqlite::make_insights_storage(root, [] { return std::vector<application::insights_storage_row>{ }; })->work(QDateTime::currentSecsSinceEpoch()).Ranges[0];
	std::cout << "work-latency:count=" << debugWork.Latencies << " stalls=" << debugWork.Stalls << " longest=" << debugWork.LongestMicroseconds << "\n";
	until([&] { return shell->findChild<QLabel*>(QStringLiteral("insightsWork0"))->text() == QStringLiteral("1"); }, "real250ms main-thread block becomes exactly one stall through the public snapshot path");
	const auto observedWork = adapters::sqlite::make_insights_storage(root, [] { return std::vector<application::insights_storage_row>{ }; })->work(QDateTime::currentSecsSinceEpoch()).Ranges[0];
	expect(observedWork.Stalls == 1 && observedWork.LongestMicroseconds >= 240000 && observedWork.LongestMicroseconds < 330000, "stall reports the actual250ms busy turn and excludes idle wait");
	auto* workFacts = shell->findChild<QTableWidget*>(QStringLiteral("insightsWorkFacts"));
	auto* slowFiles = shell->findChild<QTableWidget*>(QStringLiteral("insightsSlowFiles"));
	expect(workFacts->item(2, 1)->text() == QStringLiteral("1") && workFacts->item(3, 1)->text() == QStringLiteral("1") && workFacts->item(4, 1)->text() == QStringLiteral("1"), "three diff limits appear under their own rows");
	expect(workFacts->item(5, 1)->text() == QStringLiteral("75.0% (9 / 12)") && workFacts->item(6, 1)->text() == QStringLiteral("2"), "syntax cache figures match retained observations");
	expect(slowFiles->rowCount() == 5 && slowFiles->item(0, 0)->text() == QStringLiteral("src/work/file-6.cpp") && shell->findChild<QLabel*>(QStringLiteral("insightsWork2"))->text() == QStringLiteral("20.0 ms"), "five slowest paths and search median match host facts");
	auto* table = shell->findChild<QTableWidget*>(QStringLiteral("insightsStores"));
	until([&] { return table->rowCount() > 0; }, "the composed snapshot reaches a populated storage ledger");
	expect(std::filesystem::exists(root / "state/v1/insights.sqlite3"), "first launch composed its real Insights store");
	bool foundInsights = false;
	for (int row = 0; row < table->rowCount(); ++row) {
		foundInsights = foundInsights || table->item(row, 0)->text() == QStringLiteral("insights.sqlite3");
	}
	expect(foundInsights && !shell->findChild<QLabel*>(QStringLiteral("insightsLocalData"))->text().contains(QStringLiteral("unavailable")), "Summary and Storage display real data, including Insights itself");
	auto* budget = shell->findChild<QWidget*>(QStringLiteral("insightsBudget"));
	until([&] { return budget->property("ownRequests").toULongLong() == 412; }, "seeded real store reaches the Summary budget");
	const auto expectedOther     = QStringLiteral("other clients about %1;").arg(QLocale{ }.toString(1240));
	const auto expectedRemaining = QStringLiteral("%1 left of").arg(QLocale{ }.toString(3348));
	const auto budgetMatches     = budget->accessibleDescription().contains(expectedOther) && budget->accessibleDescription().contains(expectedRemaining);
	if (!budgetMatches) {
		std::cerr << "insights-budget locale=" << QLocale{ }.name().toStdString() << " own_requests=" << budget->property("ownRequests").toULongLong()
				  << " description=" << budget->accessibleDescription().toStdString() << '\n';
	}
	expect(budgetMatches, "two-client split uses persisted BPR count and the lowest remaining response");
	expect(shell->findChild<QLabel*>(QStringLiteral("insightsProblems"))->text() == QStringLiteral("2"), "Problems arrives from the same local snapshot");
	shared_chart_contract(*window);
	reviews_contract(*shell, root);
	energy_contract(*shell);
	const auto beforeRefresh = app::detail::github_application_fixture_access::evidence(*runtime);

	const auto initialContent   = shell->size();
	auto*      tabs             = shell->findChild<QTabBar*>(QStringLiteral("insightsTabs"));
	auto*      pages            = shell->findChild<QStackedWidget*>(QStringLiteral("insightsPages"));
	auto*      toolbar          = shell->findChild<QWidget*>(QStringLiteral("insightsToolbar"));
	auto*      header           = shell->findChild<QWidget*>(QStringLiteral("insightsHeader"));
	const auto initialHeaderBox = header->geometry();
	expect(toolbar->geometry().top() == 0 && toolbar->height() == 40 && shell->windowFlags().testFlag(Qt::ExpandedClientAreaHint) && shell->windowFlags().testFlag(Qt::NoTitleBarBackgroundHint) && !shell->testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea), "tabs and actions occupy the expanded native title row without an extra safe-area row");
	expect(toolbar->geometry().bottom() < initialHeaderBox.top() && initialHeaderBox.bottom() < pages->geometry().top(), "the unified title row precedes a fixed title/status header and page body");
	expect(qt::insights_window::content_size({ 1280, 1000 }) == QSize{ 900, 542 } && initialContent == qt::insights_window::content_size(shell->screen()->availableGeometry().size()), "the mockup dimensions clamp to the opening screen");
	expect(tabs->count() == 6 && shell->minimumSize() == shell->maximumSize(), "six pages share one non-resizable size");
	// The offscreen screen can be only 400 logical pixels at 2x. First prove
	// real-screen clamping above, then judge the preferred canvas at each DPI.
	shell->setFixedSize(qt::insights_window::content_size({ 1280, 1000 }));
	QApplication::processEvents();
	const auto frame     = shell->frameGeometry();
	const auto content   = shell->size();
	const auto headerBox = header->geometry();
	for (int page = 0; page < tabs->count(); ++page) {
		tabs->setCurrentIndex(page);
		if (page == 2) {
			auto* chart      = dynamic_cast<qt::insights_chart*>(shell->findChild<QWidget*>(QStringLiteral("insightsGithubChart")));
			auto* categories = shell->findChild<QTableWidget*>(QStringLiteral("insightsRequestCategories"));
			expect(chart->height() > 200 && categories->mapTo(shell, QPoint{ }).x() > chart->mapTo(shell, QPoint{ }).x() + chart->width(), "ATT composition gives the chart height and keeps bounded facts beside it");
		}
		for (auto* range : pages->currentWidget()->findChildren<QTabBar*>()) {
			for (int index = 0; index < range->count(); ++index) {
				range->setCurrentIndex(index);
				QApplication::processEvents();
				expect(shell->frameGeometry() == frame, "every range keeps the frame");
			}
		}
		for (int refresh = 0; refresh < 3; ++refresh) {
			shell->refresh();
			QApplication::processEvents();
			expect(shell->frameGeometry() == frame && shell->size() == content && header->geometry() == headerBox, "three refreshes on each page preserve frame and content");
		}
	}
	const auto afterRefresh = app::detail::github_application_fixture_access::evidence(*runtime);
	expect(beforeRefresh.AccountReadCount == afterRefresh.AccountReadCount && beforeRefresh.SmartAcquireCount == afterRefresh.SmartAcquireCount && beforeRefresh.BootstrapFactoryAttempts == afterRefresh.BootstrapFactoryAttempts, "all local refreshes issue no GitHub, Git or credential acquisition");
	window->github_connect_action()->trigger();
	until([&] { return window->session_panel()->status() == core::github_status::session_ready; }, "normal fixture account connects");
	until([&] { shell->refresh(); return budget->property("ownRequests").toULongLong() > 412; }, "real provider transport observations join the persisted current-window count");
	expect(qt::insights_window::content_size({ 800, 500 }) == QSize{ 764, 464 }, "smaller screen clamps both content dimensions with an 18 px margin");
	shell->setFixedSize(qt::insights_window::content_size({ 800, 500 }));
	for (int page = 0; page < tabs->count(); ++page) {
		tabs->setCurrentIndex(page);
		QApplication::processEvents();
		if (page == 1) {
			const auto folder = qEnvironmentVariable("BPR_INSIGHTS_RENDER_ROOT");
			if (!folder.isEmpty()) {
				expect(shell->grab().save(folder + QStringLiteral("/insights-reviews-clamped.png")), "clamped Reviews surface renders through the public window");
			}
		}
		for (auto* item : pages->currentWidget()->findChildren<QWidget*>(QString{ }, Qt::FindDirectChildrenOnly)) {
			expect(pages->currentWidget()->rect().contains(item->geometry()), "every page fits the clamped content without page scrolling");
		}
	}
	shell->setFixedSize(content);
	render(*shell, *runtime);
	shell->findChild<QAbstractButton*>(QStringLiteral("insightsExport"))->click();
	QDialog* exportDialog = nullptr;
	until([&] {
		for (auto* widget : QApplication::topLevelWidgets()) {
			if (widget->objectName() == QStringLiteral("diagnosticExportDialog") && widget->isVisible()) {
				exportDialog = qobject_cast<QDialog*>(widget);
			}
		}
		return exportDialog && exportDialog->findChild<QPlainTextEdit*>(QStringLiteral("releaseMaintenancePreview"))->toPlainText().contains(QStringLiteral("\"section\":\"insights\""));
	},
		  "the header action opens the existing frozen export preview with byte-only Insights data");
	exportDialog->reject();

	tabs->setCurrentIndex(3);
	shell->findChild<QPushButton*>(QStringLiteral("insightsManageCaches"))->click();
	expect(window->settings() && window->settings()->isVisible() && window->settings()->current_page() == qt::settings_page::storage_and_privacy, "Manage cached repositories opens existing cache controls");
	window->settings()->hide();
	QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
	QApplication::sendEvent(shell, &escape);
	expect(!shell->isVisible(), "Escape closes Insights");
	for (auto* timer : shell->findChildren<QTimer*>()) {
		expect(!timer->isActive(), "hidden window has no running refresh or status timer");
	}
	shell->refresh();
	expect(!shell->isVisible(), "hidden refresh cannot reopen the window");
	desktop->quit();
	until([&] { return desktop->is_quiescent(); }, "profile drains local Insights effects and completions before retiring");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "course profile removed");
	std::cout << "insights-public-path:pass locale=" << QLocale{ }.name().toStdString() << " menu singleton real-storage all-tabs all-ranges three-refreshes fixed-frame manage-existing-controls escape hidden-timers shutdown\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
