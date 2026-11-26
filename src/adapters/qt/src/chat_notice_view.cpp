#include "bpr/qt/chat_notice_view.hpp"

#include "agent_card_frame.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "side_chat_parts.hpp"

#include <QChar>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto look_of(const core::chat_notice_tone aTone) -> agent_card::card_look
		{
			switch (aTone) {
			case core::chat_notice_tone::quiet:   return agent_card::card_look::card;
			case core::chat_notice_tone::callout: return agent_card::card_look::callout;
			case core::chat_notice_tone::ready:   return agent_card::card_look::ready;
			case core::chat_notice_tone::danger:  return agent_card::card_look::danger;
			}
			return agent_card::card_look::card;
		}
	} // namespace

	chat_notice_view::chat_notice_view(const core::chat_notice_form aForm, const names& aNames, QWidget* aParent)
		: QWidget{ aParent }
		, Form{ aForm }
	{
		setObjectName(aNames.Notice);
		Shown.Form = aForm;
		switch (aForm) {
		case core::chat_notice_form::line: {
			// Said only when there is something to say, in the quiet ink.
			auto* const column = new QVBoxLayout{ this };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(0);
			Words = side_chat::quiet(side_chat::small(new QLabel{ this }));
			Words->setObjectName(aNames.Words);
			Words->setWordWrap(true);
			Words->setTextFormat(Qt::PlainText);
			column->addWidget(Words);
			break;
		}
		case core::chat_notice_form::strip: {
			// [[BPR-1817]]. Why the last send produced no turn, said directly
			// above the field it came from, until the reviewer sends again or
			// closes it.
			auto* const row = new QHBoxLayout{ this };
			row->setContentsMargins(10, 6, 2, 6);
			row->setSpacing(4);
			Words = new QLabel{ this };
			Words->setObjectName(aNames.Words);
			Words->setTextFormat(Qt::PlainText);
			Words->setWordWrap(true);
			Words->setTextInteractionFlags(Qt::TextSelectableByMouse);
			row->addWidget(Words, 1);
			Dismiss = make_drawer_tool(this, drawer_icon(drawer_glyph::close, palette()), tr("Dismiss"), aNames.Dismiss);
			connect(Dismiss, &QToolButton::clicked, this, [this] {
				if (Dispatch.Dismissed) {
					Dispatch.Dismissed();
				}
			});
			row->addWidget(Dismiss, 0, Qt::AlignTop);
			break;
		}
		case core::chat_notice_form::card: {
			Column = new QVBoxLayout{ this };
			// Its words start where the question field's do. The trailing
			// inset is narrower, so that two choices fit a 280 px drawer whole
			// whatever seven characters name the revision.
			Column->setContentsMargins(12, 10, 8, 10);
			Column->setSpacing(4);
			Title = new QLabel{ this };
			Title->setObjectName(aNames.Title);
			auto bold = Title->font();
			bold.setWeight(QFont::DemiBold);
			Title->setFont(bold);
			Title->setWordWrap(true);
			// [[BPR-2295]] (owner, 2026-11-19: "The little x in the top right
			// that the agent is ready box has is good enough").
			auto* const top = new QHBoxLayout{ };
			top->addWidget(Title, 1);
			auto* const dismiss = new quiet_tool_button{ this };
			Dismiss             = dismiss;
			dismiss->setObjectName(aNames.Dismiss);
			dismiss->setAutoRaise(true);
			dismiss->setText(QString{ QChar{ 0x2715 } });
			dismiss->setAccessibleName(tr("Dismiss"));
			dismiss->setToolTip(tr("Dismiss"));
			dismiss->setFocusPolicy(Qt::StrongFocus);
			dismiss->hide();
			connect(dismiss, &QToolButton::clicked, this, [this] {
				if (Dispatch.Dismissed) {
					Dispatch.Dismissed();
				}
			});
			top->addWidget(dismiss, 0, Qt::AlignTop);
			Column->addLayout(top);
			Words = new QLabel{ this };
			Words->setObjectName(aNames.Words);
			Words->setWordWrap(true);
			Words->setForegroundRole(QPalette::PlaceholderText);
			Column->addWidget(Words);
			// The card's two choices stay on one line at every drawer width
			// (owner, attended review 2026-11-25: "surely we can find a way to
			// fit both options on one line at the narrowest drawer"). Both are
			// chips, which keep six pixels round their words where the
			// platform's push button keeps a bezel; the row never wraps, and
			// where a typeface is too wide for both the first draws its
			// shorter words (`set_short_text`) instead of being cut.
			auto* const row = new QHBoxLayout{ };
			row->setContentsMargins(0, 2, 0, 0);
			row->setSpacing(6);
			for (auto slot = std::size_t{ 0U }; slot < Choices.size(); ++slot) {
				auto* const choice = new quiet_push_button{ this };
				choice->setObjectName(slot == 0U ? aNames.First : aNames.Second);
				// A chip paints its whole rect: laid out by a bezel it does not
				// have, it is drawn over its neighbour under the macOS style.
				choice->setAttribute(Qt::WA_LayoutUsesWidgetRect);
				choice->hide();
				row->addWidget(choice);
				Choices[slot] = choice;
				connect(choice, &QPushButton::clicked, this, [this, slot] {
					if (Dispatch.Chose) {
						Dispatch.Chose(Actions[slot]);
					}
				});
			}
			row->addStretch(1);
			Column->addLayout(row);
			ChoiceRow = Column->count() - 1;
			break;
		}
		}
		hide();
	}

	void chat_notice_view::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void chat_notice_view::present(const core::chat_notice& aNotice)
	{
		Shown            = aNotice;
		Shown.Form       = Form;
		const auto words = QString::fromStdString(aNotice.Words);
		if (Words->text() != words) {
			Words->setText(words);
		}
		if (Title != nullptr) {
			Title->setText(QString::fromStdString(aNotice.Title));
		}
		if (Form == core::chat_notice_form::line) {
			// The line is hidden with the notice, as it was before it was
			// shared.
			Words->setToolTip(QString::fromStdString(aNotice.Tip));
			Words->setVisible(aNotice.says());
		} else {
			setToolTip(QString::fromStdString(aNotice.Tip));
		}
		if (Form == core::chat_notice_form::card) {
			Dismiss->setVisible(aNotice.Dismissable);
			// An action beside the choice takes the second place; the choices
			// fill from the first.
			auto filled = std::array<const core::chat_notice_choice*, 2U>{ };
			for (const auto& choice : aNotice.Choices) {
				const auto slot = choice.Aside ? std::size_t{ 1U } : filled[0] == nullptr ? std::size_t{ 0U }
																						  : std::size_t{ 1U };
				if (filled[slot] == nullptr) {
					filled[slot] = &choice;
				}
			}
			for (auto slot = std::size_t{ 0U }; slot < Choices.size(); ++slot) {
				auto* const button = Choices[slot];
				const auto* choice = filled[slot];
				if (choice == nullptr) {
					button->hide();
					continue;
				}
				Actions[slot] = choice->Action;
				button->setText(QString::fromStdString(choice->Words));
				button->set_short_text(QString::fromStdString(choice->ShortWords));
				button->setToolTip(QString::fromStdString(choice->Tip));
				button->setEnabled(choice->Enabled);
				// An action beside a choice stays bare at rest; one of two
				// choices is a chip like the other.
				button->setFlat(choice->Aside);
				// [[BPR-2000]]. A choice that starts a run wears the agent's
				// accent.
				if (choice->Starts) {
					wear_agent_accent(*button, palette());
				} else {
					wear_ordinary_roles(*button);
				}
				button->show();
			}
		}
		setVisible(aNotice.says());
		update();
	}

	void chat_notice_view::clear() { present(core::chat_notice{ .Form = Form }); }

	auto chat_notice_view::button_for(const core::chat_notice_action aAction) const -> quiet_push_button*
	{
		for (auto slot = std::size_t{ 0U }; slot < Choices.size(); ++slot) {
			if (Choices[slot] != nullptr && !Choices[slot]->isHidden() && Actions[slot] == aAction) {
				return Choices[slot];
			}
		}
		return nullptr;
	}

	void chat_notice_view::add_above_choices(QWidget* const aPart)
	{
		if (Column != nullptr && aPart != nullptr) {
			Column->insertWidget(ChoiceRow++, aPart);
		}
	}

	void chat_notice_view::add_below_choices(QWidget* const aPart)
	{
		if (Column != nullptr && aPart != nullptr) {
			Column->addWidget(aPart);
		}
	}

	void chat_notice_view::paintEvent(QPaintEvent* /*aEvent*/)
	{
		if (Form != core::chat_notice_form::line) {
			agent_card::paint_card(*this, look_of(Shown.Tone));
		}
	}
} // namespace bpr::qt
