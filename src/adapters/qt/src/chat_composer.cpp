#include "bpr/qt/chat_composer.hpp"

#include "composer_parts.hpp"
#include "side_chat_parts.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/skill_completer.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QListWidget>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace bpr::qt {
	/// The attached item: a quiet pill inside the field.
	void passage_chip::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto ground = palette().color(QPalette::Active, QPalette::Base);
		const auto accent = agent_accent(palette());
		const auto box    = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		painter.setPen(QPen{ side_chat::mixed(ground, accent, 0.35), 1.0 });
		painter.setBrush(side_chat::mixed(ground, accent, 0.08));
		painter.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
	}

	chat_composer::chat_composer(const names& aNames, QWidget* const aParent, QPlainTextEdit* const aText)
		: QWidget{ aParent }
	{
		setObjectName(aNames.Field);
		auto* const column = new QVBoxLayout{ this };
		// [[BPR-2337]]. The insets, the rule and the send row every agent
		// composer shares (`composer_parts.hpp`).
		composer::set_field_margins(*column);
		column->setSpacing(4);
		Chips = new QVBoxLayout{ };
		Chips->setContentsMargins(0, 0, 0, 0);
		Chips->setSpacing(4);
		column->addLayout(Chips);

		// The words. Owner look v003 and the Claude Code and Codex composers:
		// no frame of their own inside the field, at most about four lines
		// tall, and Tab leaves for the next control.
		Text = aText != nullptr ? aText : new QPlainTextEdit{ this };
		if (aText == nullptr) {
			Text->setObjectName(aNames.Text);
		}
		Text->setParent(this);
		Text->setTabChangesFocus(true);
		Text->setFrameShape(QFrame::NoFrame);
		Text->viewport()->setAutoFillBackground(false);
		Text->setAttribute(Qt::WA_MacShowFocusRect, false);
		Text->document()->setDocumentMargin(4);
		Text->setMaximumHeight(90);
		// Return's rule is read before the skill list's only while the list
		// is closed: an event filter installed later runs first.
		Text->installEventFilter(this);
		Skills = new skill_completer{ Text };
		column->addWidget(Text);
		UnderText = new QVBoxLayout{ };
		UnderText->setContentsMargins(0, 0, 0, 0);
		UnderText->setSpacing(4);
		column->addLayout(UnderText);

		// The row: the chat's tools, then Stop and Send at the right end.
		Row   = composer::add_send_row(*this, *column);
		Tools = new QHBoxLayout{ };
		Tools->setContentsMargins(0, 0, 0, 0);
		Tools->setSpacing(2);
		Row->addLayout(Tools);
		Row->addStretch(1);
		Stop = new quiet_push_button{ this };
		Stop->setObjectName(aNames.Stop);
		Stop->setFocusPolicy(Qt::TabFocus);
		Stop->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		Stop->setText(tr("Stop"));
		Stop->setAccessibleName(tr("Stop"));
		Stop->setToolTip(tr("Stop the agent. Queued messages wait, paused."));
		Stop->hide();
		Row->addWidget(Stop);
		Send = composer::make_send_button(*this, aNames.Send);
		Send->setFocusPolicy(Qt::TabFocus);
		Row->addWidget(Send);
		UnderRow = new QVBoxLayout{ };
		UnderRow->setContentsMargins(0, 0, 0, 0);
		UnderRow->setSpacing(0);
		column->addLayout(UnderRow);

		connect(Send, &QPushButton::clicked, this, [this] {
			if (Controls.Send == core::chat_send_face::stop) {
				if (Dispatch.Stop) {
					Dispatch.Stop();
				}
				return;
			}
			if (Dispatch.Send) {
				Dispatch.Send(false);
			}
		});
		connect(Stop, &QPushButton::clicked, this, [this] {
			if (Dispatch.Stop) {
				Dispatch.Stop();
			}
		});
		restyle();
	}

	void chat_composer::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	auto chat_composer::stop_button() const noexcept -> QPushButton* { return Stop; }

	void chat_composer::present(const core::chat_send_controls& aControls)
	{
		Controls = aControls;
		restyle();
	}

	void chat_composer::restyle()
	{
		using face = core::chat_send_face;
		// [[BPR-2000]]. Send starts a run, or puts words where they will
		// run, and wears the agent's accent; Stop ends spending and wears the
		// ordinary roles, in Send's place or beside it.
		switch (Controls.Send) {
		case face::send:
			Send->setText({ });
			Send->setAccessibleName(tr("Send"));
			break;
		case face::queue:
			Send->setText(tr("Queue"));
			Send->setAccessibleName(tr("Queue"));
			break;
		case face::steer:
			Send->setText(tr("Steer"));
			Send->setAccessibleName(tr("Steer"));
			break;
		case face::stop:
			Send->setText(tr("Stop"));
			Send->setAccessibleName(tr("Stop"));
			break;
		}
		if (Controls.Send == face::stop) {
			wear_ordinary_roles(*Send);
		} else {
			wear_agent_accent(*Send, palette());
		}
		Send->setIcon(drawer_icon(Controls.Send == face::stop ? drawer_glyph::close : drawer_glyph::send, Send->palette()));
		Stop->setVisible(Controls.StopBeside);
		wear_ordinary_roles(*Stop);
		Stop->setIcon(drawer_icon(drawer_glyph::close, Stop->palette()));
	}

	void chat_composer::paintEvent(QPaintEvent* /*aEvent*/)
	{
		// The Agent tab's field: the base colour in a quiet rounded outline,
		// the outline the keyboard's focus ring while the words have it.
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto base = palette().color(QPalette::Active, QPalette::Base);
		const auto box  = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		painter.setBrush(base);
		painter.setPen(QPen{ Focused ? palette().color(QPalette::Active, QPalette::Highlight) : side_chat::mixed(base, palette().color(QPalette::Active, QPalette::WindowText), 0.16), Focused ? 1.5 : 1.0 });
		painter.drawRoundedRect(box, 8.0, 8.0);
	}

	void chat_composer::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange && Send != nullptr) {
			restyle();
		}
	}

	bool chat_composer::eventFilter(QObject* const aWatched, QEvent* const aEvent)
	{
		if (aWatched != Text) {
			return QWidget::eventFilter(aWatched, aEvent);
		}
		switch (aEvent->type()) {
		case QEvent::FocusIn:
		case QEvent::FocusOut:
			Focused = aEvent->type() == QEvent::FocusIn;
			update();
			break;
		case QEvent::InputMethod:
			// While an input method composes, Return is its own.
			Composing = !static_cast<QInputMethodEvent*>(aEvent)->preeditString().isEmpty();
			break;
		case QEvent::KeyPress: {
			// [[BPR-1999]], RFC-BPR-0101, RFC-BPR-0126. Return sends and
			// Shift+Return starts a line; Command+Return (Control+Return off
			// macOS) sends the other way. Not while an input method
			// composes or the skill list is open, and plain Return in an
			// empty field does nothing.
			const auto* const key       = static_cast<QKeyEvent*>(aEvent);
			const auto        modifiers = key->modifiers() & ~Qt::KeypadModifier;
			if ((key->key() != Qt::Key_Return && key->key() != Qt::Key_Enter) || modifiers.testFlag(Qt::ShiftModifier) || modifiers.testFlag(Qt::AltModifier)) {
				break;
			}
			if (Composing || (Skills != nullptr && Skills->popup() != nullptr && Skills->popup()->isVisible())) {
				return false;
			}
			const auto opposite = modifiers.testAnyFlags(Qt::ControlModifier | Qt::MetaModifier);
			if (Text->toPlainText().trimmed().isEmpty() && !opposite) {
				return true;
			}
			if (Dispatch.Send) {
				Dispatch.Send(opposite);
			}
			return true;
		}
		default: break;
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
}
