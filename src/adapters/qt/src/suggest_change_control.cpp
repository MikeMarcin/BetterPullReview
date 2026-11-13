#include "bpr/qt/suggest_change_control.hpp"

#include "bpr/qt/agent_feature.hpp"

#include <QAbstractButton>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace bpr::qt {
	namespace {
		/// Every label the two buttons take, so they keep one width.
		[[nodiscard]] auto button_words() -> std::array<QString, 7>
		{
			return { QObject::tr("Keep change"), QObject::tr("Discard"), QObject::tr("Undo"), QObject::tr("Done"), QObject::tr("Show reply"), QObject::tr("Hide reply"), QObject::tr("Dismiss") };
		}
	} // namespace

	suggest_change_button::suggest_change_button(QWidget* aParent)
		: QToolButton{ aParent }
	{
		setObjectName(QStringLiteral("threadDraftSuggestChange"));
		setToolButtonStyle(Qt::ToolButtonTextOnly);
		setAutoRaise(true);
		setFocusPolicy(Qt::TabFocus);
		setAccessibleName(tr("Suggest the change"));
		set_running(false);
	}

	void suggest_change_button::set_running(const bool aRunning)
	{
		Running = aRunning;
		setText(aRunning ? tr("Stop") : tr("Suggest change"));
		// One sentence, under sixty characters (owner, 2026-11-10).
		setToolTip(aRunning ? tr("Stop the agent writing this change") : tr("Ask the agent to write this change in the code"));
		restyle();
	}

	auto suggest_change_button::sizeHint() const -> QSize
	{
		// The wider of its two words, so Stop never moves the row.
		const auto metrics = fontMetrics();
		const auto widest  = std::max(metrics.horizontalAdvance(tr("Suggest change")), metrics.horizontalAdvance(tr("Stop")));
		return { widest + 14, std::max(24, metrics.height() + 8) };
	}

	void suggest_change_button::restyle()
	{
		if (Restyling) {
			return;
		}
		Restyling         = true;
		const auto ground = parentWidget() != nullptr ? parentWidget()->palette() : palette();
		// It starts a run, so it wears the Agent accent; Stop wears the
		// ordinary roles, as Enhance does.
		if (Running) {
			wear_ordinary_roles(*this);
		} else {
			wear_agent_accent(*this, ground);
		}
		Restyling = false;
		update();
	}

	void suggest_change_button::changeEvent(QEvent* aEvent)
	{
		QToolButton::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			restyle();
		}
	}

	suggest_change_panel::suggest_change_panel(QWidget* aParent)
		: QFrame{ aParent }
	{
		setObjectName(QStringLiteral("threadDraftSuggestPanel"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(12, 8, 10, 8);
		column->setSpacing(6);
		Line = new QLabel{ this };
		Line->setObjectName(QStringLiteral("suggestChangeLine"));
		Line->setTextFormat(Qt::PlainText);
		Line->setWordWrap(true);
		column->addWidget(Line);
		Reply = new QPlainTextEdit{ this };
		Reply->setObjectName(QStringLiteral("suggestChangeReply"));
		Reply->setReadOnly(true);
		Reply->setAccessibleName(tr("The agent's reply"));
		Reply->setFrameShape(QFrame::NoFrame);
		Reply->setMaximumHeight(140);
		Reply->hide();
		column->addWidget(Reply);
		hide();
	}

	void suggest_change_panel::set_buttons(QAbstractButton* aFirst, QAbstractButton* aSecond)
	{
		First           = aFirst;
		Second          = aSecond;
		auto* const row = new QHBoxLayout{ };
		row->setSpacing(6);
		First->setParent(this);
		Second->setParent(this);
		row->addWidget(First);
		row->addWidget(Second);
		row->addStretch(1);
		static_cast<QVBoxLayout*>(layout())->addLayout(row);
		fit_buttons();
		First->hide();
		Second->hide();
	}

	void suggest_change_panel::fit_buttons()
	{
		auto widest = 0;
		for (auto* const button : { First, Second }) {
			const auto kept = button->text();
			for (const auto& words : button_words()) {
				button->setText(words);
				widest = std::max(widest, button->sizeHint().width());
			}
			button->setText(kept);
		}
		First->setFixedWidth(widest);
		Second->setFixedWidth(widest);
	}

	void suggest_change_panel::present(const agent_suggestion_presentation& aPresentation)
	{
		const auto changed = Phase != aPresentation.Phase;
		Phase              = aPresentation.Phase;
		if (Phase == agent_suggestion_phase::idle) {
			Reply->hide();
			hide();
			return;
		}
		Line->setText(aPresentation.Line);
		setAccessibleName(aPresentation.Line);
		if (Reply->toPlainText() != aPresentation.Reply) {
			Reply->setPlainText(aPresentation.Reply);
		}
		if (changed || Phase != agent_suggestion_phase::refused) {
			Reply->hide();
		}
		if (First != nullptr && Second != nullptr) {
			const auto buttons = Phase != agent_suggestion_phase::running;
			First->setVisible(buttons);
			Second->setVisible(buttons);
			switch (Phase) {
			case agent_suggestion_phase::proposed:
				First->setText(tr("Keep change"));
				First->setToolTip(tr("Put this change in your comment"));
				Second->setText(tr("Discard"));
				Second->setToolTip(tr("Drop the change; your comment stays as it is"));
				break;
			case agent_suggestion_phase::kept:
				First->setText(tr("Undo"));
				First->setToolTip(tr("Put your comment back as it was"));
				Second->setText(tr("Done"));
				Second->setToolTip(tr("Close this panel"));
				break;
			case agent_suggestion_phase::refused:
				First->setText(Reply->isVisible() ? tr("Hide reply") : tr("Show reply"));
				First->setToolTip(tr("Read the agent's reply as text"));
				First->setEnabled(!aPresentation.Reply.isEmpty());
				Second->setText(tr("Dismiss"));
				Second->setToolTip(tr("Close this panel"));
				break;
			case agent_suggestion_phase::running:
			case agent_suggestion_phase::idle:      break;
			}
			if (Phase != agent_suggestion_phase::refused) {
				First->setEnabled(true);
			}
		}
		show();
	}

	void suggest_change_panel::toggle_reply()
	{
		if (Phase != agent_suggestion_phase::refused || Reply->toPlainText().isEmpty()) {
			return;
		}
		Reply->setVisible(!Reply->isVisible());
		if (First != nullptr) {
			First->setText(Reply->isVisible() ? tr("Hide reply") : tr("Show reply"));
		}
	}

	auto suggest_change_panel::line_text() const -> QString { return Line->text(); }
	auto suggest_change_panel::reply_shown() const -> bool { return Reply->isVisible(); }

	void suggest_change_panel::paintEvent(QPaintEvent* /*aEvent*/)
	{
		// A quiet panel under the field, its leading edge in the Agent accent,
		// as Enhance's: what is inside came from the agent.
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto accent = agent_accent(palette());
		const auto mix    = [](const QColor& aBase, const QColor& aTint, const float aAmount) {
            return QColor::fromRgbF((aBase.redF() * (1.0F - aAmount)) + (aTint.redF() * aAmount), (aBase.greenF() * (1.0F - aAmount)) + (aTint.greenF() * aAmount),
                                    (aBase.blueF() * (1.0F - aAmount)) + (aTint.blueF() * aAmount));
		};
		const auto wash   = mix(palette().color(QPalette::Active, QPalette::Base), accent, 0.06F);
		const auto border = mix(palette().color(QPalette::Active, QPalette::Mid), accent, 0.25F);
		painter.setPen(QPen{ border, 1.0 });
		painter.setBrush(wash);
		const auto box = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		painter.drawRoundedRect(box, 6.0, 6.0);
		painter.setPen(Qt::NoPen);
		painter.setBrush(accent);
		painter.drawRoundedRect(QRectF{ box.left() + 3.0, box.top() + 6.0, 2.5, box.height() - 12.0 }, 1.25, 1.25);
	}
} // namespace bpr::qt
