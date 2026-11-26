#include "bpr/qt/chat_history_menu.hpp"

#include "menu_row.hpp"

#include "bpr/core/chat_header.hpp"
#include "bpr/core/hex.hpp"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// Why New session is greyed. Reading the sessions takes a moment and
		/// ends by itself, so it is not said.
		[[nodiscard]] auto refusal_words(const core::new_session_refusal aRefusal) -> QString
		{
			using refusal = core::new_session_refusal;
			switch (aRefusal) {
			case refusal::none:
			case refusal::loading:
				break;
			case refusal::no_agent:
				return QObject::tr("Set up an agent first.");
			case refusal::closed:
				return QObject::tr("This part is no longer in the tour.");
			case refusal::running:
				return QObject::tr("Stop the running turn first.");
			case refusal::busy:
				return QObject::tr("The agent is at work on another part. Wait for it to finish.");
			case refusal::already_new:
				return QObject::tr("This session is new: nothing has been asked in it.");
			}
			return { };
		}

		/// The part a chat about another part of the tour belongs to.
		[[nodiscard]] auto other_chat_words(const application::native_agent_history_entry& aEntry) -> QString
		{
			const auto stop = aEntry.Scope.rfind(':');
			const auto kind = stop == std::string::npos || stop == 0U ? std::string::npos : aEntry.Scope.rfind(':', stop - 1U);
			const auto part = kind == std::string::npos ? std::string{ } : aEntry.Scope.substr(kind + 1U, stop - kind - 1U);
			const auto name = part == "1" ? QObject::tr("Problem")
				: part == "2"             ? QObject::tr("Solution")
				: part == "3"             ? QObject::tr("Stop ID %1").arg(QString::fromStdString(aEntry.Scope.substr(stop + 1U)))
										  : QObject::tr("Whole tour");
			return name + QStringLiteral(": ") + QString::fromStdString(aEntry.Title).simplified().left(60);
		}
	}

	/// New session, as a row of the menu: its name, and under it why it
	/// cannot be chosen when it cannot.
	class chat_history_menu::new_session_item final : public QWidgetAction
	{
	public:
		explicit new_session_item(QObject* const aParent)
			: QWidgetAction{ aParent }
		{
			setText(tr("New session"));
			setToolTip(tr("Keep this chat in History and start an empty one. Nothing is sent until you ask."));
		}

		void set_reason(const QString& aReason)
		{
			if (Reason != aReason) {
				Reason = aReason;
				for (auto* const made : createdWidgets()) {
					dress(*made);
				}
			}
		}
		[[nodiscard]] auto reason() const -> const QString& { return Reason; }

	protected:
		auto createWidget(QWidget* const aParent) -> QWidget* override
		{
			auto* const row = new detail::menu_row{ aParent };
			row->setObjectName(objectName() + QStringLiteral("Row"));
			row->set_action(this);
			row->setAccessibleName(text());
			row->set_activation([this] {
				for (auto* popup = QApplication::activePopupWidget(); popup != nullptr; popup = QApplication::activePopupWidget()) {
					if (!popup->close()) {
						break;
					}
				}
				trigger();
			});
			const auto* const menu  = aParent != nullptr ? aParent : row;
			auto* const       lines = new QVBoxLayout{ row };
			lines->setContentsMargins(detail::menu_text_inset(*menu), 0, 10, 0);
			lines->setSpacing(0);
			auto* const name = new QLabel{ text(), row };
			name->setObjectName(QStringLiteral("chatHistoryNewSessionName"));
			name->setTextFormat(Qt::PlainText);
			name->setAttribute(Qt::WA_TransparentForMouseEvents);
			name->setMinimumHeight(detail::menu_item_height(*menu));
			auto* const why = new QLabel{ row };
			why->setObjectName(QStringLiteral("chatHistoryNewSessionReason"));
			why->setTextFormat(Qt::PlainText);
			why->setAttribute(Qt::WA_TransparentForMouseEvents);
			auto small = why->font();
			small.setPointSizeF(small.pointSizeF() * 0.86);
			why->setFont(small);
			why->setContentsMargins(0, 0, 0, 5);
			lines->addWidget(name);
			lines->addWidget(why);
			row->set_highlight_observer([row, name](const bool aLit) {
				auto worn = QPalette{ };
				if (aLit) {
					worn = name->palette();
					worn.setColor(QPalette::WindowText, row->ink());
				}
				name->setPalette(worn);
			});
			row->setEnabled(isEnabled());
			dress(*row);
			return row;
		}

	private:
		/// The reason under the name, read in the quiet ink though the row is
		/// greyed: it is what the reviewer is meant to read.
		void dress(QWidget& aRow) const
		{
			auto* const why = aRow.findChild<QLabel*>(QStringLiteral("chatHistoryNewSessionReason"));
			if (why == nullptr) {
				return;
			}
			why->setText(Reason);
			why->setVisible(!Reason.isEmpty());
			auto quiet = QPalette{ };
			quiet.setColor(QPalette::Disabled, QPalette::WindowText, aRow.palette().color(QPalette::Active, QPalette::PlaceholderText));
			why->setPalette(quiet);
			aRow.setAccessibleDescription(Reason);
			aRow.setToolTip(Reason.isEmpty() ? toolTip() : Reason);
			aRow.updateGeometry();
		}

		QString Reason;
	};

	chat_history_menu::chat_history_menu(const core::chat_history_kind aKind, QWidget& aOwner, QToolButton& aButton, const QString& aPrefix)
		: QObject{ &aButton }
		, Kind{ aKind }
	{
		Menu = new QMenu{ &aButton };
		Menu->setObjectName(aPrefix + QStringLiteral("HistoryMenu"));
		Menu->setToolTipsVisible(true);
		aButton.setMenu(Menu);
		aButton.setPopupMode(QToolButton::InstantPopup);

		NewItem = new new_session_item{ this };
		NewItem->setObjectName(aPrefix + QStringLiteral("NewSession"));
		if (aKind == core::chat_history_kind::tour_side_chat) {
			NewItem->setToolTip(tr("Keep this chat in History and start an empty one. Accepted tour work stays. What was sent earlier stays in the old chat."));
		} else if (aKind == core::chat_history_kind::review_item) {
			NewItem->setToolTip(tr("Keep this chat in History and start an empty one about the same item. What it changed stays changed."));
		}
		connect(NewItem, &QAction::triggered, this, [this] {
			// Core's word, asked again as it is pressed.
			refresh();
			if (Plan.NewSession == core::new_session_refusal::none && Dispatch.NewSession) {
				Dispatch.NewSession();
			}
		});
		Rows = new agent_session_menu{ aOwner, this };
		Rows->set_dispatch({
			.Open      = [this](const std::string& aId) { if (Dispatch.Open) { Dispatch.Open(aId); } },
			.Rename    = [this](const std::string& aId, std::string aName) { if (Dispatch.Edit) { Dispatch.Edit(aId, std::move(aName)); } },
			.Delete    = [this](const std::string& aId) { if (Dispatch.Edit) { Dispatch.Edit(aId, std::nullopt); } },
			.DeleteAll = [this] { if (Dispatch.Edit) { Dispatch.Edit({ }, std::nullopt); } },
		});
		CarryOver = new QAction{ carry_over_words({ }), this };
		CarryOver->setObjectName(aPrefix + QStringLiteral("CarryOver"));
		connect(CarryOver, &QAction::triggered, this, [this] {
			if (Plan.CarryOver && Dispatch.CarryOver) {
				Dispatch.CarryOver(History.Selected);
			}
		});
		Others = new QMenu{ tr("Chats about other parts of the tour"), Menu };
		Others->setObjectName(aPrefix + QStringLiteral("OtherChats"));
		Copy = new QAction{ tr("Copy conversation"), this };
		Copy->setObjectName(aPrefix + QStringLiteral("CopyChat"));
		Copy->setToolTip(tr("Copy this conversation as text"));
		connect(Copy, &QAction::triggered, this, [this] { if (Dispatch.CopyConversation) { Dispatch.CopyConversation(); } });
		EveryRun = new QAction{ tr("Show every run…"), this };
		EveryRun->setObjectName(aPrefix + QStringLiteral("ShowEveryRun"));
		connect(EveryRun, &QAction::triggered, this, [this] { if (Dispatch.ShowEveryRun) { Dispatch.ShowEveryRun(); } });
		Reset = new QAction{ tr("Reset agents…"), this };
		Reset->setObjectName(aPrefix + QStringLiteral("ResetAgentsAction"));
		connect(Reset, &QAction::triggered, this, [this] { if (Dispatch.ResetAgents) { Dispatch.ResetAgents(); } });
		Revision = new QAction{ this };
		Revision->setObjectName(aPrefix + QStringLiteral("RevisionRead"));
		Revision->setEnabled(false);
		connect(Menu, &QMenu::aboutToShow, this, [this] {
			refresh();
			if (Dispatch.AboutToShow) {
				Dispatch.AboutToShow();
			}
		});
		refresh();
	}

	void chat_history_menu::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	auto chat_history_menu::new_session_action() const noexcept -> QAction* { return NewItem; }

	auto chat_history_menu::new_session_reason() const -> QString { return NewItem->reason(); }

	void chat_history_menu::present(const application::native_agent_history_projection& aHistory)
	{
		if (aHistory.ReadOnly) {
			return;
		}
		Loading = aHistory.Loading;
		if (!aHistory.Loading) {
			History = aHistory;
			Rows->present(aHistory);
		}
		refresh();
	}

	void chat_history_menu::set_state(const chat_state& aState)
	{
		if (State == aState && Built) {
			return;
		}
		State = aState;
		refresh();
	}

	void chat_history_menu::set_state_source(std::function<chat_state()> aSource)
	{
		Source = std::move(aSource);
		refresh();
	}

	void chat_history_menu::present_status(const QString& aStatus) { Rows->present_status(aStatus); }

	auto chat_history_menu::facts() const -> core::chat_history_facts
	{
		const auto open  = std::ranges::find(History.Entries, History.Selected, &application::native_agent_history_entry::Id);
		const auto found = !History.Selected.empty() && open != History.Entries.end();
		return {
			.Kind            = Kind,
			.AgentReady      = State.AgentReady,
			.Closed          = State.Closed,
			.Loading         = Loading,
			.Running         = State.Running,
			.Busy            = State.Busy,
			.SavedTurns      = found ? open->Turns : 0U,
			.ShownTurns      = State.ShownTurns,
			.Uncertain       = found && open->Pending,
			.NativeSupported = History.NativeSupported,
			.CarriedOver     = History.Replay,
			.Pinned          = State.Pinned,
			.OtherChats      = History.OtherChats.size(),
			.RevisionKnown   = found && open->Binding.HeadOid != decltype(open->Binding.HeadOid){ },
			.EveryRun        = State.EveryRun,
			.ResetAgents     = State.ResetAgents,
		};
	}

	// The sentences are core's (`chat_header.hpp`), one set for every chat.
	auto chat_history_menu::carried_over_words(const QString& aAgent) -> QString
	{
		return QString::fromStdString(core::chat_session_notice(core::chat_continuation::carried_over, aAgent.toStdString()));
	}

	auto chat_history_menu::cannot_continue_words(const QString& aAgent) -> QString
	{
		return QString::fromStdString(core::chat_session_notice(core::chat_continuation::cannot_continue, aAgent.toStdString()));
	}

	auto chat_history_menu::session_notice_words(const bool aCarriedOver, const bool aNativeSupported, const QString& aAgent) -> QString
	{
		return QString::fromStdString(core::chat_session_notice(core::chat_continuation_of(aCarriedOver, aNativeSupported), aAgent.toStdString()));
	}

	auto chat_history_menu::carry_over_words(const QString& aAgent) -> QString { return QString::fromStdString(core::chat_carry_over_words(aAgent.toStdString())); }

	auto chat_history_menu::carry_over_tip(const std::size_t aTurnsLeftOut, const bool aTextCut) -> QString
	{
		return QString::fromStdString(core::chat_carry_over_tip(aTurnsLeftOut, aTextCut));
	}

	void chat_history_menu::refresh()
	{
		if (Source) {
			State = Source();
		}
		const auto before = Plan.Items;
		Plan              = core::plan_chat_history(facts());
		Rows->set_running(State.Running);
		NewItem->setEnabled(Plan.NewSession == core::new_session_refusal::none);
		NewItem->set_reason(refusal_words(Plan.NewSession));
		CarryOver->setEnabled(Plan.CarryOver);
		CarryOver->setText(carry_over_words(State.Agent));
		CarryOver->setToolTip(carry_over_tip(History.Replay ? History.ReplayTurnsOmitted : 0U, History.Replay && History.ReplayTextCutBytes > 0U));
		Copy->setEnabled(Plan.Copy);
		if (const auto open = std::ranges::find(History.Entries, History.Selected, &application::native_agent_history_entry::Id); open != History.Entries.end()) {
			Revision->setText(QString::fromStdString(core::chat_revision_read_words(core::canonical_hex(open->Binding.HeadOid).view())));
		}
		Others->clear();
		for (const auto& entry : History.OtherChats) {
			auto* const item = Others->addAction(other_chat_words(entry));
			connect(item, &QAction::triggered, this, [this, id = entry.Id] { if (Dispatch.ViewOther) { Dispatch.ViewOther(id); } });
		}
		if (!Built || before != Plan.Items) {
			rebuild();
		}
	}

	void chat_history_menu::rebuild()
	{
		using item = core::chat_history_item;
		Built      = true;
		// The actions are this object's and the submenu the menu's: clearing
		// the menu takes them out of it and deletes none.
		for (auto* const action : Menu->actions()) {
			Menu->removeAction(action);
			if (action->isSeparator()) {
				action->deleteLater();
			}
		}
		for (const auto each : Plan.Items) {
			switch (each) {
			case item::new_session:       Menu->addAction(NewItem); break;
			case item::separator:         Menu->addSeparator(); break;
			case item::sessions:          Menu->addAction(Rows); break;
			case item::carry_over:        Menu->addAction(CarryOver); break;
			case item::delete_all:        Menu->addAction(Rows->delete_all_action()); break;
			case item::other_chats:       Menu->addMenu(Others); break;
			case item::copy_conversation: Menu->addAction(Copy); break;
			case item::show_every_run:    Menu->addAction(EveryRun); break;
			case item::reset_agents:      Menu->addAction(Reset); break;
			case item::revision_read:     Menu->addAction(Revision); break;
			}
		}
	}
}
