#include "bpr/qt/agent_suggestion_list.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

namespace bpr::qt {
	agent_suggestion_list::agent_suggestion_list(const QString& aLead, QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("agentSuggestions"));
		Column = new QVBoxLayout{ this };
		Column->setContentsMargins(0, 2, 0, 0);
		Column->setSpacing(6);
		Lead = new QLabel{ aLead, this };
		Lead->setObjectName(QStringLiteral("agentSuggestionsLine"));
		Lead->setForegroundRole(QPalette::PlaceholderText);
		Lead->setWordWrap(true);
		Lead->setVisible(!aLead.isEmpty());
		Column->addWidget(Lead);
	}

	void agent_suggestion_list::present(const QStringList& aPrompts)
	{
		if (aPrompts == Prompts) {
			return;
		}
		Prompts = aPrompts;
		for (auto& button : Buttons) {
			if (button != nullptr) {
				button->hide();
				Column->removeWidget(button);
				button->deleteLater();
			}
		}
		Buttons.clear();
		for (const auto& prompt : Prompts) {
			auto* const button = new quiet_push_button{ this };
			button->setObjectName(QStringLiteral("agentSuggestion"));
			// A long path is shortened in its middle; the prompt sent is whole.
			button->setText(button->fontMetrics().elidedText(prompt, Qt::ElideMiddle, 320));
			// [[BPR-1872]]. A click sends; Shift edits first. Said where the
			// prompt is pointed at, and to a screen reader.
			button->setToolTip(tr("%1\nClick to send. Shift-click to edit before sending.").arg(prompt));
			button->setAccessibleName(prompt);
			button->setAccessibleDescription(tr("Sends this prompt to the agent. Shift-click or %1 puts it in the question field to edit before sending.")
												 .arg(QKeySequence{ Qt::SHIFT | Qt::Key_Return }.toString(QKeySequence::NativeText)));
			button->setProperty("agentPrompt", prompt);
			button->setEnabled(Usable);
			button->installEventFilter(this);
			connect(button, &QPushButton::clicked, this, [this, prompt] { choose(prompt, std::exchange(EditFirst, false)); });
			Column->addWidget(button, 0, Qt::AlignLeft);
			Buttons.emplace_back(button);
		}
	}

	void agent_suggestion_list::set_usable(const bool aUsable)
	{
		Usable = aUsable;
		for (const auto& button : Buttons) {
			if (button != nullptr) {
				button->setEnabled(aUsable);
			}
		}
	}

	auto agent_suggestion_list::buttons() const -> std::vector<QPushButton*>
	{
		auto result = std::vector<QPushButton*>{ };
		for (const auto& button : Buttons) {
			if (button != nullptr) {
				result.push_back(button);
			}
		}
		return result;
	}

	void agent_suggestion_list::choose(const QString& aPrompt, const bool aEditFirst)
	{
		if (Choose) {
			Choose(aPrompt, aEditFirst);
		}
	}

	bool agent_suggestion_list::eventFilter(QObject* aWatched, QEvent* aEvent)
	{
		// [[BPR-1872]]. A suggested prompt reads Shift on the press that
		// clicks it; Return and Enter choose it too, as Space does.
		if (aWatched->objectName() == QStringLiteral("agentSuggestion")) {
			if (aEvent->type() == QEvent::MouseButtonRelease) {
				EditFirst = (static_cast<QMouseEvent*>(aEvent)->modifiers() & Qt::ShiftModifier) != 0;
			} else if (aEvent->type() == QEvent::KeyRelease || aEvent->type() == QEvent::KeyPress) {
				const auto* const key   = static_cast<QKeyEvent*>(aEvent);
				const auto        shift = (key->modifiers() & Qt::ShiftModifier) != 0;
				if (key->key() == Qt::Key_Space) {
					EditFirst = shift;
				} else if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && aEvent->type() == QEvent::KeyPress) {
					auto* const button = static_cast<QPushButton*>(aWatched);
					if (button->isEnabled() && !key->isAutoRepeat()) {
						choose(button->property("agentPrompt").toString(), shift);
					}
					return true;
				}
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
}
