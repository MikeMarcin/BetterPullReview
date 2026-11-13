#include "bpr/qt/agent_footer.hpp"

#include "bpr/core/agent_activity.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStringList>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto index_of(const agent_footer_place aPlace) -> std::size_t { return static_cast<std::size_t>(aPlace); }

		[[nodiscard]] auto quiet_colour(const QPalette& aPalette) -> QColor
		{
			return aPalette.color(QPalette::Window).lightness() < 128 ? QColor{ 0xD2, 0x99, 0x22 } : QColor{ 0x9A, 0x67, 0x00 };
		}

		/// The Stop square, in the failed check's colour, drawn for the
		/// device's scale.
		[[nodiscard]] auto stop_icon(const QPalette& aPalette, const qreal aScale) -> QIcon
		{
			constexpr auto side   = 12;
			auto           pixmap = QPixmap{ static_cast<int>(side * aScale), static_cast<int>(side * aScale) };
			pixmap.setDevicePixelRatio(aScale);
			pixmap.fill(Qt::transparent);
			auto painter = QPainter{ &pixmap };
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.setPen(Qt::NoPen);
			painter.setBrush(setup_failed_colour(aPalette));
			painter.drawRoundedRect(QRectF{ 2.5, 2.5, 7.0, 7.0 }, 1.5, 1.5);
			return QIcon{ pixmap };
		}

		/// The running dot, in the agent's accent, for the runs count.
		[[nodiscard]] auto dot_icon(const QColor& aColour, const qreal aScale) -> QIcon
		{
			constexpr auto side   = 10;
			auto           pixmap = QPixmap{ static_cast<int>(side * aScale), static_cast<int>(side * aScale) };
			pixmap.setDevicePixelRatio(aScale);
			pixmap.fill(Qt::transparent);
			auto painter = QPainter{ &pixmap };
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.setPen(Qt::NoPen);
			painter.setBrush(aColour);
			painter.drawEllipse(QRectF{ 1.0, 1.0, 8.0, 8.0 });
			return QIcon{ pixmap };
		}

		[[nodiscard]] auto seconds_of(const QElapsedTimer& aStarted) -> qint64 { return aStarted.isValid() ? aStarted.elapsed() / 1000 : 0; }

		[[nodiscard]] auto since_frame_ms(const agent_run_status& aStatus) -> qint64
		{
			if (!aStatus.Activity) {
				return 0;
			}
			return std::max<qint64>(0, agent_activity_clock_ms() - aStatus.ActivityStartedMs - static_cast<qint64>(aStatus.Activity->LastFrameMs));
		}
	} // namespace

	auto agent_footer_place_name(const agent_footer_place aPlace) -> QString
	{
		switch (aPlace) {
		case agent_footer_place::agent_tab:  return QObject::tr("Agent tab");
		case agent_footer_place::tour_chat:  return QObject::tr("Tour chat");
		case agent_footer_place::enhance:    return QObject::tr("Enhance");
		case agent_footer_place::draft_tour: return QObject::tr("Draft the tour");
		case agent_footer_place::tour_tab:   return QObject::tr("Tour tab");
		}
		return { };
	}

	auto agent_footer_run_quiet(const agent_run_status& aStatus) -> bool
	{
		return aStatus.Activity.has_value() && aStatus.Stage != agent_run_stage::preparing && core::agent_run_is_quiet(static_cast<std::uint64_t>(since_frame_ms(aStatus)), std::nullopt);
	}

	auto agent_footer_doing_words(const agent_run_status& aStatus) -> QString
	{
		if (agent_footer_run_quiet(aStatus)) {
			return QObject::tr("no activity for %1 min").arg(std::max<qint64>(2, since_frame_ms(aStatus) / 60000));
		}
		if (aStatus.Activity && aStatus.Stage != agent_run_stage::preparing) {
			auto words = agent_doing_words(*aStatus.Activity);
			if (words.startsWith(QStringLiteral("is "))) {
				words.remove(0, 3);
			}
			return words;
		}
		switch (aStatus.Stage) {
		case agent_run_stage::preparing: return QObject::tr("making its workspace");
		case agent_run_stage::thinking:  return QObject::tr("thinking");
		case agent_run_stage::writing:   return QObject::tr("writing");
		}
		return QObject::tr("working");
	}

	agent_footer::agent_footer(std::shared_ptr<agent_run_settings_source> aSource, QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("agentFooter"));
		setAccessibleName(tr("Agent"));
		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(0, 0, 4, 0);
		row->setSpacing(8);

		// The run: where it started, what it does, how long, and Stop.
		RunArea         = new QWidget{ this };
		auto* const run = new QHBoxLayout{ RunArea };
		run->setContentsMargins(0, 0, 0, 0);
		run->setSpacing(6);
		Dot = new QLabel{ QString{ QChar{ 0x25CF } }, RunArea };
		Dot->setObjectName(QStringLiteral("agentFooterDot"));
		run->addWidget(Dot);
		Where = new QLabel{ RunArea };
		Where->setObjectName(QStringLiteral("agentFooterWhere"));
		auto bold = Where->font();
		bold.setWeight(QFont::DemiBold);
		Where->setFont(bold);
		run->addWidget(Where);
		Doing = new QLabel{ RunArea };
		Doing->setObjectName(QStringLiteral("agentFooterDoing"));
		Doing->setForegroundRole(QPalette::PlaceholderText);
		Doing->setTextFormat(Qt::PlainText);
		// The ending's words may be long; the line keeps one row.
		Doing->setMaximumWidth(280);
		run->addWidget(Doing);
		Elapsed = new QLabel{ RunArea };
		Elapsed->setObjectName(QStringLiteral("agentFooterElapsed"));
		Elapsed->setForegroundRole(QPalette::PlaceholderText);
		// Wide enough for 59:59, so the ticking clock never moves Stop.
		Elapsed->setMinimumWidth(Elapsed->fontMetrics().horizontalAdvance(QStringLiteral("00:00")));
		Elapsed->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		run->addWidget(Elapsed);
		// A quiet chip, the picker's height, so a run never makes the bar
		// taller or louder than the review's own line.
		auto* const stop = new quiet_tool_button{ RunArea };
		StopButton       = stop;
		StopButton->setObjectName(QStringLiteral("agentFooterStop"));
		StopButton->setText(tr("Stop"));
		StopButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		StopButton->setToolTip(tr("Stop this run. What arrived is kept."));
		connect(StopButton, &QToolButton::clicked, this, [this] {
			for (const auto& shown : Runs) {
				if (shown && shown->Stop) {
					shown->Stop();
					return;
				}
			}
		});
		run->addWidget(StopButton);
		row->addWidget(RunArea);

		// Several at once: a count that opens the list, each with its Stop.
		auto* const runs = new quiet_tool_button{ this };
		RunsButton       = runs;
		RunsButton->setObjectName(QStringLiteral("agentFooterRuns"));
		RunsButton->setAutoRaise(true);
		RunsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		RunsButton->setPopupMode(QToolButton::InstantPopup);
		RunsMenu = new QMenu{ RunsButton };
		RunsMenu->setObjectName(QStringLiteral("agentFooterRunsMenu"));
		RunsButton->setMenu(RunsMenu);
		connect(RunsMenu, &QMenu::aboutToShow, this, [this] { rebuild_menu(); });
		row->addWidget(RunsButton);

		Divider = new QLabel{ QStringLiteral("|"), this };
		Divider->setForegroundRole(QPalette::PlaceholderText);
		Divider->setAccessibleName({ });
		row->addWidget(Divider);

		SettingsButton = new quiet_push_button{ tr("Agent settings…"), this };
		SettingsButton->setObjectName(QStringLiteral("agentFooterSettings"));
		SettingsButton->setFlat(true);
		SettingsButton->setToolTip(tr("Open Settings › Agents & context."));
		connect(SettingsButton, &QPushButton::clicked, this, [this] {
			if (SettingsDispatch) {
				SettingsDispatch();
			}
		});
		row->addWidget(SettingsButton);

		Picker = new agent_run_settings_picker{ std::move(aSource), this };
		Picker->setObjectName(QStringLiteral("agentFooterPicker"));
		Picker->installEventFilter(this);
		row->addWidget(Picker);
		Cost = new QLabel{ this };
		Cost->setObjectName(QStringLiteral("agentFooterCost"));
		Cost->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		// Room for "$00.00" whatever the figure, so the picker stays put.
		Cost->setMinimumWidth(Cost->fontMetrics().horizontalAdvance(QStringLiteral("$00.00")));
		row->addWidget(Cost);

		// One height for every state, so the status bar, and the window above
		// it, never moves when a run starts or ends.
		setFixedHeight(std::max({ Picker->sizeHint().height(), StopButton->sizeHint().height(), RunsButton->sizeHint().height(), SettingsButton->sizeHint().height(), Cost->sizeHint().height() }));
		Ticker = new QTimer{ this };
		Ticker->setInterval(1000);
		connect(Ticker, &QTimer::timeout, this, [this] { tick(); });
		FinishedTimer = new QTimer{ this };
		FinishedTimer->setSingleShot(true);
		FinishedTimer->setInterval(6000);
		connect(FinishedTimer, &QTimer::timeout, this, [this] {
			Finished.reset();
			refresh();
		});
		refresh();
	}

	agent_footer::~agent_footer() = default;

	void agent_footer::set_settings_dispatch(std::function<void()> aDispatch) { SettingsDispatch = std::move(aDispatch); }

	void agent_footer::set_finished_hold(const int aMilliseconds) { FinishedTimer->setInterval(aMilliseconds); }

	void agent_footer::present_run(const agent_footer_place aPlace, std::optional<agent_footer_run> aRun)
	{
		auto& slot = Runs[index_of(aPlace)];
		slot       = std::move(aRun);
		if (slot) {
			Finished.reset();
			FinishedTimer->stop();
		}
		refresh();
	}

	void agent_footer::present_finished(const agent_footer_place aPlace, const QString& aEnding)
	{
		Endings[index_of(aPlace)] = aEnding;
		Runs[index_of(aPlace)].reset();
		Finished = aPlace;
		FinishedTimer->start();
		refresh();
	}

	void agent_footer::present_cost(const agent_footer_place aPlace, const std::optional<core::agent_cost> aCost)
	{
		Costs[index_of(aPlace)] = aCost;
		refresh();
	}

	void agent_footer::present_setup(const QString& aNeed, const bool aFailed)
	{
		Need       = aNeed;
		NeedFailed = aFailed;
		refresh();
	}

	auto agent_footer::running() const -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(Runs, [](const auto& aRun) { return aRun.has_value(); }));
	}

	auto agent_footer::total_cost() const -> std::optional<core::agent_cost>
	{
		auto total = std::optional<core::agent_cost>{ };
		for (const auto& cost : Costs) {
			if (cost) {
				total = total.value_or(0U) + *cost;
			}
		}
		return total;
	}

	void agent_footer::tick()
	{
		if (running() == 0U) {
			Ticker->stop();
			return;
		}
		refresh();
	}

	void agent_footer::match_fonts()
	{
		// Every word in the footer at the picker's size, as one line. The
		// style sets the picker's font when it polishes it, so this follows.
		// Resolved whole: the picker's font is the style's class default,
		// which a widget would otherwise take as "inherit".
		auto words = Picker->font();
		words.setResolveMask(QFont::AllPropertiesResolved);
		if (Cost->font() == words && Cost->font().resolveMask() == words.resolveMask()) {
			return;
		}
		for (auto* const label : { Dot, Doing, Elapsed, Divider, Cost }) {
			label->setFont(words);
		}
		auto strong = words;
		strong.setWeight(QFont::DemiBold);
		strong.setResolveMask(QFont::AllPropertiesResolved);
		Where->setFont(strong);
		for (QWidget* const button : { static_cast<QWidget*>(StopButton), static_cast<QWidget*>(RunsButton), static_cast<QWidget*>(SettingsButton) }) {
			button->setFont(words);
		}
		Elapsed->setMinimumWidth(Elapsed->fontMetrics().horizontalAdvance(QStringLiteral("00:00")));
		Cost->setMinimumWidth(Cost->fontMetrics().horizontalAdvance(QStringLiteral("$00.00")));
	}

	void agent_footer::refresh()
	{
		match_fonts();
		const auto count = running();
		const auto first = std::ranges::find_if(Runs, [](const auto& aRun) { return aRun.has_value(); });
		const auto quiet = count == 1U && agent_footer_run_quiet((*first)->Status);
		auto       dot   = Dot->palette();
		dot.setColor(QPalette::WindowText, quiet ? quiet_colour(palette()) : agent_accent(palette()));
		Dot->setPalette(dot);
		auto where = Where->palette();
		where.setColor(QPalette::WindowText, quiet ? quiet_colour(palette()) : palette().color(QPalette::WindowText));
		Where->setPalette(where);
		StopButton->setIcon(stop_icon(palette(), devicePixelRatioF()));

		if (count == 1U) {
			const auto& shown = **first;
			const auto  place = static_cast<agent_footer_place>(first - Runs.begin());
			Dot->setText(QString{ QChar{ 0x25CF } });
			Dot->show();
			Where->setText(agent_footer_place_name(place));
			const auto doing = agent_footer_doing_words(shown.Status);
			Doing->setText(Doing->fontMetrics().elidedText(doing, Qt::ElideRight, Doing->maximumWidth()));
			Doing->setToolTip(doing);
			Elapsed->setText(agent_elapsed_words(seconds_of(shown.Started)));
			Elapsed->show();
			StopButton->setVisible(static_cast<bool>(shown.Stop));
			StopButton->setAccessibleName(tr("Stop %1").arg(agent_footer_place_name(place)));
			RunArea->show();
			RunsButton->hide();
		} else if (count > 1U) {
			RunArea->hide();
			auto names = QStringList{ };
			for (std::size_t index = 0; index < Runs.size(); ++index) {
				if (Runs[index]) {
					names << agent_footer_place_name(static_cast<agent_footer_place>(index));
				}
			}
			RunsButton->setText(QStringLiteral("%1  %2").arg(tr("%1 running").arg(count), names.join(QStringLiteral(", "))));
			RunsButton->setIcon(dot_icon(agent_accent(palette()), devicePixelRatioF()));
			RunsButton->setToolTip(tr("Show each run, with its own Stop"));
			RunsButton->setAccessibleName(tr("%1 agent runs: %2").arg(count).arg(names.join(QStringLiteral(", "))));
			RunsButton->show();
		} else if (Finished) {
			// Just finished: its ending for a few seconds, then idle.
			Dot->setText(QString{ QChar{ 0x2713 } });
			auto done = Dot->palette();
			done.setColor(QPalette::WindowText, setup_passed_colour(palette()));
			Dot->setPalette(done);
			Where->setText(agent_footer_place_name(*Finished));
			const auto ending = Endings[index_of(*Finished)];
			Doing->setText(Doing->fontMetrics().elidedText(ending, Qt::ElideRight, Doing->maximumWidth()));
			Doing->setToolTip(ending);
			Elapsed->hide();
			StopButton->hide();
			RunArea->show();
			RunsButton->hide();
		} else {
			RunArea->hide();
			RunsButton->hide();
		}
		if (count > 0U && !Ticker->isActive()) {
			Ticker->start();
		}

		// The picker, or the way to set one up.
		Picker->set_attention(Need, NeedFailed);
		const auto chosen = !Picker->isHidden();
		SettingsButton->setText(chosen ? tr("Agent settings…") : tr("Set up an agent…"));
		SettingsButton->setVisible(!Need.isEmpty() || Picker->isHidden());
		Divider->setVisible(RunArea->isVisibleTo(this) || RunsButton->isVisibleTo(this));

		const auto total = total_cost();
		Cost->setText(total ? QString::fromStdString(core::agent_cost_words(*total)) : QString{ });
		auto parts = QStringList{ };
		for (std::size_t index = 0; index < Costs.size(); ++index) {
			if (Costs[index]) {
				parts << tr("%1 %2").arg(agent_footer_place_name(static_cast<agent_footer_place>(index)), QString::fromStdString(core::agent_cost_words(*Costs[index])));
			}
		}
		Cost->setToolTip(total ? tr("Agent cost this session: %1").arg(parts.join(QStringLiteral(", "))) : QString{ });
		Cost->setAccessibleName(total ? tr("%1 this session").arg(Cost->text()) : QString{ });
		setAccessibleDescription(text());
		updateGeometry();
		if (RunsMenu->isVisible()) {
			rebuild_menu();
		}
	}

	void agent_footer::rebuild_menu()
	{
		RunsMenu->clear();
		RunsMenu->addSection(tr("Running"));
		for (std::size_t index = 0; index < Runs.size(); ++index) {
			if (!Runs[index]) {
				continue;
			}
			const auto& shown = *Runs[index];
			const auto  name  = agent_footer_place_name(static_cast<agent_footer_place>(index));
			auto* const item  = RunsMenu->addAction(stop_icon(palette(), devicePixelRatioF()), tr("Stop %1: %2, %3").arg(name, agent_footer_doing_words(shown.Status), agent_elapsed_words(seconds_of(shown.Started))));
			item->setObjectName(QStringLiteral("agentFooterStopRun"));
			item->setEnabled(static_cast<bool>(shown.Stop));
			connect(item, &QAction::triggered, this, [stop = shown.Stop] {
				if (stop) {
					stop();
				}
			});
		}
		auto finished = false;
		for (std::size_t index = 0; index < Endings.size(); ++index) {
			if (Endings[index].isEmpty() || Runs[index]) {
				continue;
			}
			if (!finished) {
				RunsMenu->addSection(tr("Finished in this pull request"));
				finished = true;
			}
			RunsMenu->addAction(tr("%1: %2").arg(agent_footer_place_name(static_cast<agent_footer_place>(index)), Endings[index]))->setEnabled(false);
		}
		if (const auto total = total_cost()) {
			RunsMenu->addSeparator();
			RunsMenu->addAction(tr("%1 this session").arg(QString::fromStdString(core::agent_cost_words(*total))))->setEnabled(false);
		}
	}

	auto agent_footer::text() const -> QString
	{
		auto parts = QStringList{ };
		if (RunArea->isVisibleTo(this)) {
			auto run = Where->text() + QLatin1Char{ ' ' } + Doing->toolTip();
			if (Elapsed->isVisibleTo(this)) {
				run += QLatin1Char{ ' ' } + Elapsed->text();
			}
			if (StopButton->isVisibleTo(this)) {
				run += QStringLiteral(" Stop");
			}
			parts << run;
		}
		if (RunsButton->isVisibleTo(this)) {
			parts << RunsButton->text();
		}
		if (SettingsButton->isVisibleTo(this)) {
			parts << SettingsButton->text();
		}
		if (!Picker->isHidden()) {
			parts << Picker->full_text();
		}
		if (!Cost->text().isEmpty()) {
			parts << Cost->text();
		}
		return parts.join(QStringLiteral(" | "));
	}

	auto agent_footer::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		// The style gives the picker its font when it polishes it.
		if (aWatched == Picker && (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::Polish || aEvent->type() == QEvent::Show)) {
			match_fonts();
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void agent_footer::showEvent(QShowEvent* aEvent)
	{
		QWidget::showEvent(aEvent);
		refresh();
	}

	void agent_footer::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange || aEvent->type() == QEvent::FontChange) {
			refresh();
		}
	}
} // namespace bpr::qt
