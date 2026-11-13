#include "bpr/qt/explain_file_card.hpp"

#include "bpr/qt/quiet_tool_button.hpp"
#include "comment_state_toggle.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/quiet_push_button.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace bpr::qt {
	namespace {
		/// The accent rule at the leading edge; the card is drawn inset from
		/// the widget by `inset` at the sides, and its content sits `padding`
		/// inside it.
		constexpr int rule_width = 3;
		constexpr int inset      = 6;
		constexpr int padding    = 10;
		/// The answer shows at most this many lines before it scrolls, so
		/// the file keeps most of the comparison.
		constexpr int answer_lines = 14;
	} // namespace

	explain_file_card::explain_file_card(QWidget* const aParent)
		: QFrame{ aParent }
	{
		setObjectName(QStringLiteral("explainFileCard"));
		setAccessibleName(tr("Explain this file"));
		setFrameShape(QFrame::NoFrame);
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(inset + rule_width + padding, 4 + padding - 2, inset + padding, 2 + padding - 2);
		column->setSpacing(6);

		ActivityView = new agent_activity_view{ this };
		ActivityView->setObjectName(QStringLiteral("explainFileActivity"));
		column->addWidget(ActivityView);

		Scroll = new QScrollArea{ this };
		Scroll->setObjectName(QStringLiteral("explainFileAnswerScroll"));
		Scroll->setFrameShape(QFrame::NoFrame);
		Scroll->setWidgetResizable(true);
		Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Scroll->viewport()->setAutoFillBackground(false);
		Answer = new markdown_body_view{ Scroll };
		Answer->setObjectName(QStringLiteral("explainFileAnswer"));
		Answer->set_link_dispatch([this](const std::string& aLink) {
			if (Link) {
				Link(QString::fromStdString(aLink));
			}
		});
		Scroll->setWidget(Answer);
		Scroll->hide();
		column->addWidget(Scroll);

		// One row: the run's line, then the three actions at the right, each
		// the same size from the press to the end.
		auto* const row = new QHBoxLayout{ };
		row->setContentsMargins(0, 0, 0, 0);
		row->setSpacing(6);
		Status = new agent_run_status_line{ this };
		Status->setObjectName(QStringLiteral("explainFileStatus"));
		row->addWidget(Status, 1);
		// Owner look v005: kept through the state control every comment of
		// mine has. A whole file takes no draft, so Draft is the struck
		// pencil; Keep local stands only as Go to comment once kept.
		KeepState = new comment_state_toggle{ QPalette::Base, this };
		KeepState->setObjectName(QStringLiteral("explainFileKeepState"));
		KeepState->local_button()->setObjectName(QStringLiteral("explainFileKeepLocal"));
		KeepState->draft_button()->setObjectName(QStringLiteral("explainFileKeepDraft"));
		KeepState->set_names(tr("Keep local"), tr("Draft comment"));
		KeepState->set_local(true, tr("Keep it as a local comment on this file"));
		KeepState->set_draft(false, tr("A whole file takes no draft"));
		KeepState->set_state(comment_state_toggle::state::none);
		KeepState->LocalChosen = [this] {
			if (KeepButton != nullptr && KeepButton->isEnabled()) {
				KeepButton->click();
			}
		};
		row->addWidget(KeepState, 0, Qt::AlignVCenter);
		KeepButton = new quiet_push_button{ tr("Keep local"), this };
		KeepButton->setObjectName(QStringLiteral("explainFileKeep"));
		ContinueButton = new quiet_push_button{ tr("Continue in Agent tab"), this };
		ContinueButton->setObjectName(QStringLiteral("explainFileContinue"));
		DismissButton = new quiet_push_button{ tr("Dismiss"), this };
		DismissButton->setObjectName(QStringLiteral("explainFileDismiss"));
		for (auto* const button : { KeepButton.data(), ContinueButton.data(), DismissButton.data() }) {
			button->setFlat(true);
			row->addWidget(button);
		}
		// Continue in Agent tab continues an agent's run: the accent says so.
		wear_agent_accent(*ContinueButton, palette());
		column->addLayout(row);
		fit_buttons();

		connect(KeepButton, &QPushButton::clicked, this, [this] {
			if (Note != 0U) {
				if (GoToNote) {
					GoToNote(Note);
				}
				return;
			}
			if (Keep && Shown.Answered && !Keeping) {
				Keeping = true;
				KeepButton->setEnabled(false);
				Keep(Shown.Path, Shown.AnswerText);
			}
		});
		connect(ContinueButton, &QPushButton::clicked, this, [this] {
			if (Continue) {
				Continue(Shown.Turn);
			}
		});
		connect(DismissButton, &QPushButton::clicked, this, [this] {
			if (Dismiss) {
				Dismiss();
			}
		});
		hide();
	}

	auto explain_file_card::keep_state() const noexcept -> QWidget*
	{
		return KeepState;
	}

	void explain_file_card::set_dresser(std::function<void(markdown_body_view&)> aDresser)
	{
		if (aDresser && Answer != nullptr) {
			aDresser(*Answer);
		}
	}

	void explain_file_card::present(const explain_file_progress& aProgress)
	{
		const auto newTurn = aProgress.Path != Shown.Path || aProgress.Turn != Shown.Turn;
		if (newTurn) {
			Note    = 0U;
			Keeping = false;
			Refusal.clear();
			++Serial;
		}
		Shown = aProgress;
		if (Shown.Running) {
			Status->present(*Shown.Running, Shown.Started);
			if (Shown.Activity) {
				ActivityView->present(agent_short_name(Shown.Running->Agent), *Shown.Activity, Shown.Running->ActivityStartedMs);
			}
		} else {
			Status->settle(Refusal.isEmpty() ? Shown.Ending : Refusal, Shown.Answered);
			if (Shown.Activity) {
				ActivityView->fold(*Shown.Activity, Shown.Seconds);
			}
		}
		ActivityView->setVisible(Shown.Activity.has_value());
		if (Answer != nullptr) {
			Answer->set_body(Shown.Answer.toStdString(), "explain-file-" + std::to_string(Serial));
		}
		Scroll->setVisible(!Shown.Answer.isEmpty());
		KeepButton->setText(Note != 0U ? tr("Go to comment") : tr("Keep local"));
		KeepButton->setEnabled(Note != 0U || (Shown.Answered && !Shown.Running && !Keeping));
		KeepButton->setVisible(Note != 0U);
		KeepState->setVisible(Note == 0U);
		KeepState->local_button()->setEnabled(Shown.Answered && !Shown.Running && !Keeping);
		fit_answer();
		show();
		updateGeometry();
	}

	void explain_file_card::present_note(const bool aKept, const QString& aWhy, const std::uint64_t aNote)
	{
		Keeping = false;
		if (aKept) {
			Note = aNote;
			Refusal.clear();
		} else {
			Refusal = tr("Not kept: %1.").arg(aWhy);
		}
		present(Shown);
	}

	void explain_file_card::clear()
	{
		Shown   = { };
		Note    = 0U;
		Keeping = false;
		Refusal.clear();
		if (Answer != nullptr) {
			Answer->set_body({ }, "explain-file-cleared");
		}
		hide();
	}

	void explain_file_card::fit_answer()
	{
		if (Answer == nullptr || Scroll == nullptr) {
			return;
		}
		const auto width   = std::max(1, Scroll->viewport()->width());
		const auto natural = Answer->heightForWidth(width);
		const auto ceiling = answer_lines * fontMetrics().lineSpacing();
		Scroll->setFixedHeight(Shown.Answer.isEmpty() ? 0 : std::min(natural, ceiling));
		Scroll->setVerticalScrollBarPolicy(natural > ceiling ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
	}

	/// The three actions take one width, the widest of every word any of them
	/// says, so none moves or resizes as the run goes on.
	void explain_file_card::fit_buttons()
	{
		auto widest  = 0;
		auto tallest = 0;
		for (const auto& words : std::array{ tr("Keep local"), tr("Go to comment"), tr("Continue in Agent tab"), tr("Dismiss") }) {
			auto probe = quiet_push_button{ words };
			probe.setFont(font());
			widest  = std::max(widest, probe.sizeHint().width());
			tallest = std::max(tallest, probe.sizeHint().height());
		}
		for (auto* const button : { KeepButton.data(), ContinueButton.data(), DismissButton.data() }) {
			button->setFixedSize(widest, tallest);
		}
	}

	auto explain_file_card::sizeHint() const -> QSize
	{
		auto hint = QFrame::sizeHint();
		hint.setWidth(std::max(hint.width(), 320));
		return hint;
	}

	void explain_file_card::paintEvent(QPaintEvent* const aEvent)
	{
		QFrame::paintEvent(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing);
		const auto bounds = QRectF{ rect() }.adjusted(6.5, 4.5, -6.5, -2.5);
		auto       card   = QPainterPath{ };
		card.addRoundedRect(bounds, 6.0, 6.0);
		painter.fillPath(card, palette().color(QPalette::Base));
		painter.setPen(QPen{ palette().color(QPalette::Mid), 1.0 });
		painter.drawPath(card);
		// The Agent accent at the leading edge, inside the card's corner.
		painter.save();
		painter.setClipPath(card);
		painter.fillRect(QRectF{ bounds.left(), bounds.top(), static_cast<qreal>(rule_width), bounds.height() }, agent_accent(palette()));
		painter.restore();
	}

	void explain_file_card::resizeEvent(QResizeEvent* const aEvent)
	{
		QFrame::resizeEvent(aEvent);
		fit_answer();
	}

	void explain_file_card::changeEvent(QEvent* const aEvent)
	{
		QFrame::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange && ContinueButton != nullptr) {
			wear_agent_accent(*ContinueButton, palette());
		}
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange) {
			fit_buttons();
			fit_answer();
		}
	}
} // namespace bpr::qt
