#include "bpr/qt/agent_run_status.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/quiet_push_button.hpp"

#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto activity_clock() -> std::function<qint64()>&
		{
			static auto clock = std::function<qint64()>{ };
			return clock;
		}

		/// A path is said by its file's name; a command by its first words.
		[[nodiscard]] auto subject_words(const core::agent_doing aDoing, const std::string& aSubject) -> QString
		{
			const auto subject = QString::fromStdString(aSubject);
			switch (aDoing) {
			case core::agent_doing::reading:
			case core::agent_doing::editing: {
				const auto name = QFileInfo{ subject }.fileName();
				return name.isEmpty() ? subject : name;
			}
			default: return subject.size() > 60 ? subject.left(59) + QStringLiteral("…") : subject;
			}
		}

		/// The quiet notice's ground: the theme's attention amber, washed.
		class attention_box final : public QWidget
		{
		public:
			using QWidget::QWidget;

		protected:
			void paintEvent(QPaintEvent*) override
			{
				const auto dark  = palette().color(QPalette::Window).lightness() < 128;
				const auto amber = builtin_attention_color(dark ? core::appearance_variant::dark : core::appearance_variant::light, attention_tone::needs_me);
				auto       wash  = amber;
				wash.setAlphaF(dark ? 0.16F : 0.10F);
				QPainter painter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(amber);
				painter.setBrush(wash);
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
			}
		};
	}

	auto agent_activity_clock_ms() -> qint64
	{
		if (const auto& clock = activity_clock(); clock) {
			return clock();
		}
		static const auto started = [] {
			auto timer = QElapsedTimer{ };
			timer.start();
			return timer;
		}();
		return started.elapsed();
	}

	void set_agent_activity_clock(std::function<qint64()> aClock) { activity_clock() = std::move(aClock); }

	auto agent_doing_words(const core::agent_turn_activity& aActivity) -> QString
	{
		const auto subject = subject_words(aActivity.Doing, aActivity.Subject);
		const auto out     = static_cast<int>(core::outstanding_agent_helpers(aActivity));
		switch (aActivity.Doing) {
		case core::agent_doing::starting:   return QObject::tr("is starting");
		case core::agent_doing::thinking:   return QObject::tr("is thinking");
		case core::agent_doing::reading:    return subject.isEmpty() ? QObject::tr("is reading") : QObject::tr("is reading %1").arg(subject);
		case core::agent_doing::searching:  return subject.isEmpty() ? QObject::tr("is searching") : QObject::tr("is searching for %1").arg(subject);
		case core::agent_doing::running:    return subject.isEmpty() ? QObject::tr("is running a command") : QObject::tr("is running %1").arg(subject);
		case core::agent_doing::editing:    return subject.isEmpty() ? QObject::tr("is editing") : QObject::tr("is editing %1").arg(subject);
		case core::agent_doing::using_tool: return subject.isEmpty() ? QObject::tr("is using a tool") : QObject::tr("is using %1").arg(subject);
		case core::agent_doing::writing:    return QObject::tr("is writing");
		case core::agent_doing::waiting:    return out > 0 ? (out == 1 ? QObject::tr("is waiting on 1 helper") : QObject::tr("is waiting on %1 helpers").arg(out)) : QObject::tr("is finishing");
		}
		return QObject::tr("is working");
	}

	auto agent_last_activity_words(const qint64 aSinceMs) -> QString
	{
		const auto seconds = qMax(qint64{ 0 }, aSinceMs) / 1000;
		if (seconds < 60) {
			return QObject::tr("last activity %1 s ago").arg(seconds);
		}
		return QObject::tr("last activity %1 min ago").arg(seconds / 60);
	}

	auto agent_activity_summary_words(const core::agent_turn_activity& aActivity, const qint64 aSeconds) -> QString
	{
		const auto minutes = aSeconds / 60;
		auto       parts   = QStringList{ };
		if (const auto helpers = core::tally_agent_helpers(aActivity).Started; helpers > 0U) {
			parts << (helpers == 1U ? QObject::tr("1 helper") : QObject::tr("%1 helpers").arg(helpers));
		}
		if (aActivity.FilesRead > 0U) {
			parts << (aActivity.FilesRead == 1U ? QObject::tr("1 file read") : QObject::tr("%1 files read").arg(aActivity.FilesRead));
		}
		if (aActivity.Commands > 0U) {
			parts << (aActivity.Commands == 1U ? QObject::tr("1 command") : QObject::tr("%1 commands").arg(aActivity.Commands));
		}
		// [[BPR-2007]], RFC-BPR-0104. What it left in its scratch folder.
		if (const auto written = aActivity.ScratchFiles.size(); written > 0U) {
			parts << (written == 1U ? QObject::tr("1 file written in its scratch folder") : QObject::tr("%1 files written in its scratch folder").arg(written));
		}
		const auto took = minutes > 0 ? QObject::tr("Worked for %1 min").arg(minutes) : QObject::tr("Worked for %1 s").arg(aSeconds);
		return parts.isEmpty() ? took : took + QStringLiteral(": ") + parts.join(QStringLiteral(", "));
	}

	auto agent_elapsed_words(const qint64 aSeconds) -> QString
	{
		const auto seconds = aSeconds < 0 ? qint64{ 0 } : aSeconds;
		return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char{ '0' });
	}

	auto agent_run_status_words(const agent_run_status& aStatus, const qint64 aSeconds) -> QString
	{
		// [[BPR-2252]]. The turn's cost so far follows its time, as the
		// settled line's does: "3:12 · $0.41".
		auto elapsed = agent_elapsed_words(aSeconds);
		if (aStatus.Cost && aStatus.Stage != agent_run_stage::preparing) {
			elapsed += QStringLiteral(" · ") + QString::fromStdString(core::agent_cost_words(*aStatus.Cost));
		}
		auto line = QString{ };
		// [[BPR-2005]]. The runtime's own events say what it is doing, and
		// when it last sent anything.
		if (aStatus.Activity && aStatus.Stage != agent_run_stage::preparing) {
			const auto since = agent_activity_clock_ms() - aStatus.ActivityStartedMs - static_cast<qint64>(aStatus.Activity->LastFrameMs);
			line             = QObject::tr("%1 %2, %3 · %4.").arg(aStatus.Agent, agent_doing_words(*aStatus.Activity), elapsed, agent_last_activity_words(since));
			if (aStatus.FilesOnly) {
				line += QStringLiteral(" ") + QObject::tr("It sees only this pull request's files.");
			}
			return QStringLiteral("● ") + line;
		}
		switch (aStatus.Stage) {
		case agent_run_stage::preparing: line = QObject::tr("%1 is making its workspace at the head, %2.").arg(aStatus.Agent, elapsed); break;
		case agent_run_stage::thinking:
			line = QObject::tr("%1 is thinking, %2.").arg(aStatus.Agent, elapsed);
			if (!aStatus.Latest.isEmpty()) {
				line += QStringLiteral(" ") + aStatus.Latest.left(120);
			}
			break;
		case agent_run_stage::writing: line = QObject::tr("%1 is writing, %2.").arg(aStatus.Agent, elapsed); break;
		}
		// [[BPR-1873]]. A sparse review with no clone: said while it runs.
		if (aStatus.FilesOnly && aStatus.Stage != agent_run_stage::preparing) {
			line += QStringLiteral(" ") + QObject::tr("It sees only this pull request's files.");
		}
		return QStringLiteral("● ") + line;
	}

	auto agent_run_answered_words(const QString& aAgent, const qint64 aSeconds) -> QString { return QObject::tr("%1 answered in %2").arg(aAgent, agent_elapsed_words(aSeconds)); }

	auto agent_short_name(const QString& aAgent) -> QString
	{
		const auto suffix = QStringLiteral(" Code");
		return aAgent.size() > suffix.size() && aAgent.endsWith(suffix) ? aAgent.chopped(suffix.size()) : aAgent;
	}

	auto agent_run_stopped_words(const qint64 aSeconds) -> QString { return QObject::tr("Stopped after %1").arg(agent_elapsed_words(aSeconds)); }

	auto agent_turn_ending_words(const QString& aAgent, const bool aAnswered, const std::optional<qint64> aSeconds) -> QString
	{
		if (aAnswered) {
			return aSeconds ? agent_run_answered_words(agent_short_name(aAgent), *aSeconds) : QObject::tr("%1 answered").arg(agent_short_name(aAgent));
		}
		return aSeconds ? agent_run_stopped_words(*aSeconds) : QObject::tr("Stopped");
	}

	agent_run_status_line::agent_run_status_line(QWidget* aParent)
		: QLabel{ aParent }
	{
		setForegroundRole(QPalette::PlaceholderText);
		setWordWrap(true);
		setTextFormat(Qt::PlainText);
		Ticker = new QTimer{ this };
		// [[BPR-1945]]. An early coarse tick still reads 0:00 and loses
		// the first whole second until the following tick.
		Ticker->setTimerType(Qt::PreciseTimer);
		Ticker->setSingleShot(true);
		Ticker->setInterval(1000);
		connect(Ticker, &QTimer::timeout, this, [this] { tick(); });
	}

	void agent_run_status_line::present(const agent_run_status& aStatus, const QElapsedTimer& aStarted)
	{
		Status  = aStatus;
		Started = aStarted;
		if (font().italic()) {
			auto upright = font();
			upright.setItalic(false);
			setFont(upright);
		}
		tick();
	}

	void agent_run_status_line::settle(const QString& aWords, const bool aAnswered)
	{
		Status.reset();
		Ticker->stop();
		setText(aWords);
		auto shown = font();
		shown.setItalic(!aAnswered);
		setFont(shown);
	}

	void agent_run_status_line::tick()
	{
		if (!Status) {
			Ticker->stop();
			return;
		}
		const auto elapsed = Started.isValid() ? qMax(qint64{ 0 }, Started.elapsed()) : 0;
		const auto seconds = elapsed / 1000;
		const auto words   = agent_run_status_words(*Status, seconds);
		// Its text is its accessible name; the tick is not announced.
		if (text() != words) {
			setText(words);
		}
		// [[BPR-1946]]. Even a native precise callback may arrive before
		// the whole second. Recheck at the actual clock's next boundary,
		// rather than losing that update for another complete period.
		Ticker->start(static_cast<int>(1000 - elapsed % 1000));
	}
}

namespace bpr::qt {
	agent_activity_view::agent_activity_view(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("agentActivity"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(4);
		// After the turn: one quiet line that opens to the rows.
		Summary = new QToolButton{ this };
		Summary->setObjectName(QStringLiteral("agentActivitySummary"));
		Summary->setAutoRaise(true);
		Summary->setCheckable(true);
		Summary->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		Summary->setArrowType(Qt::RightArrow);
		Summary->setForegroundRole(QPalette::PlaceholderText);
		// It never widens the turn: its words are cut to the width there is.
		Summary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		Summary->hide();
		connect(Summary, &QToolButton::toggled, this, [this](const bool aOpen) {
			Summary->setArrowType(aOpen ? Qt::DownArrow : Qt::RightArrow);
			Rows->setVisible(aOpen && RowGrid->count() > 0);
		});
		column->addWidget(Summary, 0, Qt::AlignLeft);
		Rows = new QWidget{ this };
		Rows->setObjectName(QStringLiteral("agentActivityRows"));
		RowGrid = new QGridLayout{ Rows };
		RowGrid->setContentsMargins(14, 0, 0, 0);
		RowGrid->setHorizontalSpacing(10);
		RowGrid->setVerticalSpacing(2);
		RowGrid->setColumnStretch(1, 1);
		Rows->hide();
		column->addWidget(Rows);
		// RFC-BPR-0103: quiet is said out loud, in the attention role.
		Quiet = new attention_box{ this };
		Quiet->setObjectName(QStringLiteral("agentQuietNotice"));
		auto* const notice = new QVBoxLayout{ Quiet };
		notice->setContentsMargins(10, 8, 10, 8);
		notice->setSpacing(4);
		QuietTitle = new QLabel{ Quiet };
		QuietTitle->setObjectName(QStringLiteral("agentQuietTitle"));
		auto strong = QuietTitle->font();
		strong.setWeight(QFont::DemiBold);
		QuietTitle->setFont(strong);
		notice->addWidget(QuietTitle);
		QuietWords = new QLabel{ Quiet };
		QuietWords->setObjectName(QStringLiteral("agentQuietWords"));
		QuietWords->setWordWrap(true);
		QuietWords->setTextFormat(Qt::PlainText);
		notice->addWidget(QuietWords);
		auto* const choices = new QHBoxLayout{ };
		choices->setSpacing(6);
		StopKeeping = new quiet_push_button{ tr("Stop and keep what arrived"), Quiet };
		StopKeeping->setObjectName(QStringLiteral("agentQuietStop"));
		KeepWaiting = new quiet_push_button{ tr("Keep waiting"), Quiet };
		KeepWaiting->setObjectName(QStringLiteral("agentQuietKeepWaiting"));
		choices->addWidget(StopKeeping);
		choices->addWidget(KeepWaiting);
		choices->addStretch(1);
		notice->addLayout(choices);
		Quiet->hide();
		column->addWidget(Quiet);
		connect(StopKeeping, &QPushButton::clicked, this, [this] {
			if (Stop) {
				Stop();
			}
		});
		// Keep waiting leaves the run as it is; the notice comes back after
		// another two minutes of silence.
		connect(KeepWaiting, &QPushButton::clicked, this, [this] {
			KeptWaitingAtMs = agent_activity_clock_ms();
			tick();
		});
		Ticker = new QTimer{ this };
		Ticker->setInterval(1000);
		connect(Ticker, &QTimer::timeout, this, [this] { tick(); });
		hide();
	}

	void agent_activity_view::set_stop(std::function<void()> aStop) { Stop = std::move(aStop); }

	void agent_activity_view::present(const QString& aAgent, const core::agent_turn_activity& aActivity, const qint64 aStartedMs)
	{
		const auto framed = !Live || aActivity.LastFrameMs != Activity.LastFrameMs;
		Agent             = aAgent;
		Activity          = aActivity;
		StartedMs         = aStartedMs;
		Live              = true;
		// A frame puts Keep waiting's pause away with the notice.
		if (framed) {
			KeptWaitingAtMs.reset();
		}
		Summary->hide();
		rebuild_rows();
		Rows->setVisible(RowGrid->count() > 0);
		setVisible(true);
		if (!Ticker->isActive()) {
			Ticker->start();
		}
		tick();
	}

	void agent_activity_view::fold(const core::agent_turn_activity& aActivity, const qint64 aSeconds)
	{
		Live     = false;
		Activity = aActivity;
		Ticker->stop();
		Quiet->hide();
		KeptWaitingAtMs.reset();
		rebuild_rows();
		const auto worked  = !aActivity.Helpers.empty() || aActivity.UnlistedHelpers > 0U || aActivity.FilesRead > 0U || aActivity.Commands > 0U || !aActivity.ScratchFiles.empty();
		const auto blocker = QSignalBlocker{ Summary };
		Summary->setChecked(false);
		Summary->setArrowType(Qt::RightArrow);
		Summary->setProperty("words", agent_activity_summary_words(aActivity, aSeconds));
		Summary->setAccessibleName(Summary->property("words").toString());
		Summary->setToolTip(Summary->property("words").toString());
		fit_summary();
		Summary->setEnabled(RowGrid->count() > 0);
		Summary->setVisible(worked);
		Rows->hide();
		setVisible(worked);
	}

	void agent_activity_view::rebuild_rows()
	{
		while (auto* const item = RowGrid->takeAt(0)) {
			delete item->widget();
			delete item;
		}
		const auto now = agent_activity_clock_ms() - StartedMs;
		auto       row = 0;
		for (const auto& helper : Activity.Helpers) {
			const auto ended   = helper.EndedMs.value_or(static_cast<std::uint64_t>(qMax(qint64{ 0 }, now)));
			const auto seconds = static_cast<qint64>((ended - std::min(ended, helper.StartedMs)) / 1000U);
			auto       state   = QString{ };
			switch (helper.State) {
			case core::agent_helper_state::running:   state = helper.Last.empty() ? tr("running") : QString::fromStdString(helper.Last); break;
			case core::agent_helper_state::finished:  state = helper.Last.empty() ? tr("finished") : tr("finished, %1").arg(QString::fromStdString(helper.Last)); break;
			case core::agent_helper_state::failed:    state = tr("failed"); break;
			case core::agent_helper_state::abandoned: state = tr("never reported back"); break;
			}
			const auto  description = helper.Description.empty() ? tr("Helper %1").arg(row + 1) : QString::fromStdString(helper.Description);
			auto* const name        = new QLabel{ description, Rows };
			name->setTextFormat(Qt::PlainText);
			name->setWordWrap(true);
			auto* const what = new QLabel{ state, Rows };
			what->setTextFormat(Qt::PlainText);
			what->setWordWrap(true);
			what->setForegroundRole(helper.State == core::agent_helper_state::running ? QPalette::PlaceholderText : QPalette::WindowText);
			what->setToolTip(state);
			what->setMinimumWidth(40);
			auto* const time = new QLabel{ agent_elapsed_words(seconds), Rows };
			time->setForegroundRole(QPalette::PlaceholderText);
			for (auto* const label : { name, what, time }) {
				label->setObjectName(QStringLiteral("agentHelperRow"));
				auto small = label->font();
				small.setPointSizeF(small.pointSizeF() * 0.92);
				label->setFont(small);
			}
			if (helper.State == core::agent_helper_state::failed || helper.State == core::agent_helper_state::abandoned) {
				auto italic = what->font();
				italic.setItalic(true);
				what->setFont(italic);
			}
			RowGrid->addWidget(name, row, 0);
			RowGrid->addWidget(what, row, 1);
			RowGrid->addWidget(time, row, 2, Qt::AlignRight);
			++row;
		}
		if (Activity.UnlistedHelpers > 0U) {
			auto* const more = new QLabel{ (Activity.UnlistedHelpers == 1U ? tr("and 1 more helper") : tr("and %1 more helpers").arg(Activity.UnlistedHelpers)), Rows };
			more->setForegroundRole(QPalette::PlaceholderText);
			RowGrid->addWidget(more, row, 0, 1, 3);
			++row;
		}
		// [[BPR-2007]], RFC-BPR-0104. Once folded, the line lists the commands
		// run and the files written in the scratch folder.
		if (Live) {
			return;
		}
		const auto listed = [&](const QString& aKind, const std::vector<std::string>& aItems, const char* aName) {
			for (const auto& item : aItems) {
				auto* const kind = new QLabel{ aKind, Rows };
				kind->setForegroundRole(QPalette::PlaceholderText);
				auto* const what = new QLabel{ QString::fromStdString(item), Rows };
				what->setTextFormat(Qt::PlainText);
				what->setWordWrap(true);
				what->setToolTip(what->text());
				what->setMinimumWidth(40);
				what->setTextInteractionFlags(Qt::TextSelectableByMouse);
				for (auto* const label : { kind, what }) {
					label->setObjectName(QString::fromLatin1(aName));
					auto small = label->font();
					small.setPointSizeF(small.pointSizeF() * 0.92);
					label->setFont(small);
				}
				RowGrid->addWidget(kind, row, 0);
				RowGrid->addWidget(what, row, 1, 1, 2);
				++row;
			}
		};
		listed(tr("Ran"), Activity.CommandsRun, "agentActivityCommand");
		listed(tr("Wrote"), Activity.ScratchFiles, "agentActivityScratchFile");
	}

	void agent_activity_view::tick()
	{
		if (!Live) {
			return;
		}
		const auto now   = agent_activity_clock_ms();
		const auto since = now - StartedMs - static_cast<qint64>(Activity.LastFrameMs);
		const auto kept  = KeptWaitingAtMs ? std::optional<std::uint64_t>{ static_cast<std::uint64_t>(qMax(qint64{ 0 }, now - *KeptWaitingAtMs)) } : std::nullopt;
		const auto quiet = core::agent_run_is_quiet(static_cast<std::uint64_t>(qMax(qint64{ 0 }, since)), kept);
		if (quiet) {
			QuietTitle->setText(tr("No activity for %1 min").arg(since / 60000) + QStringLiteral(" · ") + agent_elapsed_words((now - StartedMs) / 1000));
			auto last = QString{ };
			for (const auto& helper : Activity.Helpers) {
				if (helper.State == core::agent_helper_state::running) {
					last = tr("%1 is still connected but has sent nothing since it started “%2”.").arg(Agent, QString::fromStdString(helper.Description));
				}
			}
			if (last.isEmpty()) {
				last = tr("%1 is still connected but has sent nothing since it %2.").arg(Agent, agent_doing_words(Activity).replace(QStringLiteral("is "), QStringLiteral("was "), Qt::CaseSensitive));
			}
			QuietWords->setText(last + QStringLiteral(" ") + tr("It may be working on something long, or it may be stuck."));
		}
		Quiet->setVisible(quiet);
		// Running rows' times go on.
		if (core::outstanding_agent_helpers(Activity) > 0U) {
			rebuild_rows();
		}
	}

	auto agent_activity_view::quiet_shown() const -> bool { return !Quiet->isHidden(); }

	auto agent_activity_view::helper_rows() const -> int { return static_cast<int>(Activity.Helpers.size()); }

	auto agent_activity_view::helper_row_text(const int aRow) const -> QString
	{
		auto words = QStringList{ };
		for (auto column = 0; column < 3; ++column) {
			const auto* const item  = RowGrid->itemAtPosition(aRow, column);
			const auto* const label = item == nullptr ? nullptr : qobject_cast<QLabel*>(item->widget());
			words << (label == nullptr ? QString{ } : label->text());
		}
		return words.join(QLatin1Char{ '\t' });
	}

	auto agent_activity_view::summary_text() const -> QString { return Summary->isHidden() ? QString{ } : Summary->property("words").toString(); }

	void agent_activity_view::resizeEvent(QResizeEvent* aEvent)
	{
		QWidget::resizeEvent(aEvent);
		fit_summary();
	}

	void agent_activity_view::fit_summary()
	{
		const auto words = Summary->property("words").toString();
		// The arrow and the button's margins take about two lines' height.
		const auto room = std::max(40, width() - Summary->iconSize().width() - 24);
		Summary->setText(Summary->fontMetrics().elidedText(words, Qt::ElideRight, room));
	}

	auto agent_activity_view::rows_open() const -> bool { return !Rows->isHidden(); }

	auto agent_activity_view::quiet_text() const -> QString { return QuietTitle->text() + QLatin1Char{ '\n' } + QuietWords->text(); }
}
