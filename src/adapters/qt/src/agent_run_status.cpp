#include "bpr/qt/agent_run_status.hpp"

#include <QTimer>

namespace bpr::qt {
	auto agent_elapsed_words(const qint64 aSeconds) -> QString
	{
		const auto seconds = aSeconds < 0 ? qint64{ 0 } : aSeconds;
		return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char{ '0' });
	}

	auto agent_run_status_words(const agent_run_status& aStatus, const qint64 aSeconds) -> QString
	{
		const auto elapsed = agent_elapsed_words(aSeconds);
		auto       line    = QString{ };
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

	auto agent_run_stopped_words(const qint64 aSeconds) -> QString { return QObject::tr("Stopped after %1").arg(agent_elapsed_words(aSeconds)); }

	agent_run_status_line::agent_run_status_line(QWidget* aParent)
		: QLabel{ aParent }
	{
		setForegroundRole(QPalette::PlaceholderText);
		setWordWrap(true);
		setTextFormat(Qt::PlainText);
		Ticker = new QTimer{ this };
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
		if (!Ticker->isActive()) {
			Ticker->start();
		}
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
		const auto seconds = Started.isValid() ? Started.elapsed() / 1000 : 0;
		const auto words   = agent_run_status_words(*Status, seconds);
		// Its text is its accessible name; the tick is not announced.
		if (text() != words) {
			setText(words);
		}
	}
}
