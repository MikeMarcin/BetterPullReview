#include "bpr/qt/agent_suggestion_list.hpp"
#include "bpr/qt/agent_feature.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include <QAction>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	namespace {
		/// The widest a chip's text is drawn; a longer name is shortened in
		/// its middle on the chip only.
		constexpr int chip_text_width = 320;

		/// The first `agent_suggestion_list::tooltip_lines` lines of what a
		/// prompt sends, and how many more there are.
		auto expansion_shown(const agent_suggestion& aPrompt) -> QString
		{
			auto       text  = aPrompt.Expansion.isEmpty() ? aPrompt.Label : aPrompt.Expansion;
			const auto lines = text.split(QLatin1Char{ '\n' });
			if (lines.size() <= agent_suggestion_list::tooltip_lines) {
				return text;
			}
			const auto rest = lines.size() - agent_suggestion_list::tooltip_lines;
			return lines.mid(0, agent_suggestion_list::tooltip_lines).join(QLatin1Char{ '\n' }) + QLatin1Char{ '\n' }
			+ (rest == 1 ? agent_suggestion_list::tr("and 1 more line") : agent_suggestion_list::tr("and %1 more lines").arg(rest));
		}

		auto how_to_press(const agent_suggestion& aPrompt) -> QString
		{
			// [[BPR-1872]]. A click sends; Shift edits first. A prompt the
			// reviewer finishes is only ever put in the field.
			return aPrompt.Finish ? agent_suggestion_list::tr("Click to put it in the question field.") : agent_suggestion_list::tr("Click to send. Shift-click to edit before sending.");
		}

		auto shortened(const QPushButton& aButton, const QString& aLabel) -> QString { return aButton.fontMetrics().elidedText(aLabel, Qt::ElideMiddle, chip_text_width); }
	} // namespace

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

	auto agent_suggestion_list::tooltip_of(const agent_suggestion& aPrompt, const bool aShortened) -> QString
	{
		auto       parts    = QStringList{ };
		const auto expanded = expansion_shown(aPrompt);
		// The name first only when the chip does not show it whole and what
		// it sends does not already say it.
		if (aShortened && aPrompt.Label != (aPrompt.Expansion.isEmpty() ? aPrompt.Label : aPrompt.Expansion)) {
			parts << aPrompt.Label;
		}
		parts << expanded << how_to_press(aPrompt);
		return parts.join(QLatin1Char{ '\n' });
	}

	auto agent_suggestion_list::description_of(const agent_suggestion& aPrompt) -> QString
	{
		const auto keys = QKeySequence{ Qt::SHIFT | Qt::Key_Return }.toString(QKeySequence::NativeText);
		const auto how  = aPrompt.Finish ? tr("Click or Return puts it in the question field.") : tr("Click to send. Shift-click or %1 puts it in the question field to edit before sending.").arg(keys);
		return expansion_shown(aPrompt) + QLatin1Char{ '\n' } + how;
	}

	void agent_suggestion_list::dress(QPushButton& aButton, const agent_suggestion& aPrompt) const
	{
		const auto shown = shortened(aButton, aPrompt.Label);
		aButton.setText(shown);
		aButton.setToolTip(tooltip_of(aPrompt, shown != aPrompt.Label));
		aButton.setAccessibleName(aPrompt.Label);
		aButton.setAccessibleDescription(description_of(aPrompt));
		aButton.setProperty("agentPrompt", aPrompt.Label);
		aButton.setProperty("agentPromptKey", aPrompt.Key);
		aButton.setProperty("agentPromptExpansion", aPrompt.Expansion);
	}

	void agent_suggestion_list::present(std::vector<agent_suggestion> aChips, std::vector<agent_suggestion> aMore)
	{
		const auto sameNames  = aChips.size() == Chips.size() && std::ranges::equal(aChips, Chips, [](const agent_suggestion& aLeft, const agent_suggestion& aRight) { return aLeft.Key == aRight.Key && aLeft.Label == aRight.Label; });
		const auto moreBefore = !More.empty();
		Chips                 = std::move(aChips);
		More                  = std::move(aMore);
		if (sameNames && moreBefore == !More.empty()) {
			// Same chips: what they send may differ, their place does not.
			for (std::size_t index = 0U; index < Chips.size() && index < Buttons.size(); ++index) {
				if (Buttons[index] != nullptr) {
					dress(*Buttons[index], Chips[index]);
				}
			}
			present_more();
			return;
		}
		for (auto& button : Buttons) {
			if (button != nullptr) {
				button->hide();
				Column->removeWidget(button);
				// Out of the window at once, not only once deleted.
				button->setParent(nullptr);
				button->deleteLater();
			}
		}
		Buttons.clear();
		if (MoreButton != nullptr) {
			MoreButton->hide();
			Column->removeWidget(MoreButton);
			MoreButton->setParent(nullptr);
			MoreButton->deleteLater();
			MoreButton = nullptr;
			MoreMenu   = nullptr;
		}
		for (const auto& prompt : Chips) {
			auto* const button = new quiet_push_button{ this };
			button->setObjectName(QStringLiteral("agentSuggestion"));
			dress(*button, prompt);
			// [[BPR-2000]], RFC-BPR-0101. A suggestion sends: it starts a run.
			wear_agent_accent(*button, palette());
			button->setEnabled(Usable);
			button->installEventFilter(this);
			connect(button, &QPushButton::clicked, this, [this, key = prompt.Key] { choose(key, std::exchange(EditFirst, false)); });
			Column->addWidget(button, 0, Qt::AlignLeft);
			Buttons.emplace_back(button);
		}
		if (!More.empty()) {
			// RFC-BPR-0125 (Q7). One quiet last chip; its menu holds the rest
			// in the same order and with the same press rules.
			auto* const more = new quiet_push_button{ this };
			more->setObjectName(QStringLiteral("agentSuggestionMore"));
			more->setEnabled(Usable);
			MoreMenu = new QMenu{ more };
			MoreMenu->setObjectName(QStringLiteral("agentSuggestionMoreMenu"));
			MoreMenu->setToolTipsVisible(true);
			MoreMenu->installEventFilter(this);
			connect(more, &QPushButton::clicked, this, [this] { open_more(); });
			Column->addWidget(more, 0, Qt::AlignLeft);
			MoreButton = more;
		}
		present_more();
	}

	void agent_suggestion_list::present_more()
	{
		if (MoreButton == nullptr || MoreMenu == nullptr) {
			return;
		}
		const auto count = static_cast<int>(More.size());
		MoreButton->setText(tr("More prompts (%1)…").arg(count));
		MoreButton->setAccessibleName(tr("More prompts, %1 more").arg(count));
		MoreButton->setToolTip(tr("%1 more suggested prompts. Choose one to send it; Shift puts it in the question field.").arg(count));
		MoreMenu->clear();
		for (const auto& prompt : More) {
			auto* const item = MoreMenu->addAction(prompt.Label);
			item->setObjectName(QStringLiteral("agentSuggestionMoreItem"));
			item->setToolTip(tooltip_of(prompt, false));
			item->setData(prompt.Key);
			connect(item, &QAction::triggered, this, [this, key = prompt.Key] { choose(key, std::exchange(MoreEditFirst, false)); });
		}
	}

	void agent_suggestion_list::open_more()
	{
		if (MoreButton == nullptr || MoreMenu == nullptr || !Usable) {
			return;
		}
		MoreEditFirst = false;
		MoreMenu->popup(MoreButton->mapToGlobal(QPoint{ 0, MoreButton->height() }));
	}

	void agent_suggestion_list::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange) {
			for (const auto& button : Buttons) {
				if (button != nullptr) {
					wear_agent_accent(*button, palette());
				}
			}
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
		if (MoreButton != nullptr) {
			MoreButton->setEnabled(aUsable);
		}
	}

	auto agent_suggestion_list::prompts() const -> QStringList
	{
		auto labels = QStringList{ };
		for (const auto& prompt : Chips) {
			labels << prompt.Label;
		}
		return labels;
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

	auto agent_suggestion_list::more_button() const -> QPushButton* { return MoreButton.data(); }

	void agent_suggestion_list::choose(const QString& aKey, const bool aEditFirst)
	{
		if (Choose) {
			Choose(aKey, aEditFirst);
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
						choose(button->property("agentPromptKey").toString(), shift);
					}
					return true;
				}
			}
		} else if (aWatched == MoreMenu) {
			// RFC-BPR-0125. A More item reads Shift on the press that chooses
			// it, as a chip does.
			if (aEvent->type() == QEvent::MouseButtonRelease) {
				MoreEditFirst = (static_cast<QMouseEvent*>(aEvent)->modifiers() & Qt::ShiftModifier) != 0;
			} else if (aEvent->type() == QEvent::KeyPress) {
				const auto* const key = static_cast<QKeyEvent*>(aEvent);
				if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter || key->key() == Qt::Key_Space) {
					MoreEditFirst = (key->modifiers() & Qt::ShiftModifier) != 0;
				}
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
}
