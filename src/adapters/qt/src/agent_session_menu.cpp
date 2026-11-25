#include "bpr/qt/agent_session_menu.hpp"

#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/quiet_tool_button.hpp"

#include "bpr/core/agent_cost.hpp"
#include "bpr/core/hex.hpp"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMouseEvent>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// A session's title: a click or Return opens it.
		class session_title final : public QLabel
		{
		public:
			session_title(const QString& aText, std::function<void()> aOpen, QWidget* aParent)
				: QLabel{ aText, aParent }
				, Open{ std::move(aOpen) }
			{
				setFocusPolicy(Qt::StrongFocus);
				setCursor(Qt::PointingHandCursor);
				setTextFormat(Qt::PlainText);
			}

		protected:
			void mouseReleaseEvent(QMouseEvent* aEvent) override
			{
				if (aEvent->button() == Qt::LeftButton && Open) {
					Open();
					return;
				}
				QLabel::mouseReleaseEvent(aEvent);
			}
			void keyPressEvent(QKeyEvent* aEvent) override
			{
				if ((aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter || aEvent->key() == Qt::Key_Space) && Open) {
					Open();
					return;
				}
				QLabel::keyPressEvent(aEvent);
			}

		private:
			std::function<void()> Open;
		};

		/// The rename field: Return keeps the name, Esc puts the title back.
		class rename_field final : public QLineEdit
		{
		public:
			rename_field(std::function<void(const QString&)> aKeep, std::function<void()> aCancel, QWidget* aParent)
				: QLineEdit{ aParent }
				, Keep{ std::move(aKeep) }
				, Cancel{ std::move(aCancel) }
			{ }

		protected:
			void keyPressEvent(QKeyEvent* aEvent) override
			{
				if (aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter) {
					Keep(text());
					return;
				}
				if (aEvent->key() == Qt::Key_Escape) {
					Cancel();
					return;
				}
				QLineEdit::keyPressEvent(aEvent);
			}

		private:
			std::function<void(const QString&)> Keep;
			std::function<void()>               Cancel;
		};

		[[nodiscard]] auto row_tool(QWidget* aParent, const drawer_glyph aGlyph, const QString& aName, const QString& aObjectName) -> quiet_tool_button*
		{
			auto* const tool = make_drawer_tool(aParent, drawer_icon(aGlyph, aParent->palette()), aName, aObjectName);
			tool->setIconSize(QSize{ 14, 14 });
			return tool;
		}

		/// What the i says: turns, revision, mode and, once known, the cost.
		[[nodiscard]] auto info_text(const application::native_agent_history_entry& aEntry, const bool aOpen) -> QString
		{
			const auto head  = QString::fromStdString(std::string{ core::canonical_hex(aEntry.Binding.HeadOid).view() }).left(7);
			auto       words = aEntry.Turns == 1U ? QObject::tr("1 turn, at %1.").arg(head) : QObject::tr("%1 turns, at %2.").arg(aEntry.Turns).arg(head);
			if (aEntry.Cost) {
				words += QObject::tr(" %1 so far.").arg(QString::fromStdString(core::agent_cost_words(*aEntry.Cost)));
			}
			if (aEntry.Replay) {
				words += QObject::tr(" Transcript replay.");
			}
			if (aOpen) {
				words = QObject::tr("The open session. ") + words;
			}
			return words;
		}
	}

	auto agent_session_edit_words(const application::agent_session_edit_receipt& aReceipt, const bool aRenaming) -> QString
	{
		using result = application::agent_session_edit_result;
		switch (aReceipt.Result) {
		case result::done:
			if (aReceipt.Deleted + aReceipt.Kept > 1U || aReceipt.Kept != 0U) {
				return aReceipt.Kept == 0U ? QObject::tr("Deleted %1 sessions.").arg(aReceipt.Deleted)
										   : QObject::tr("Deleted %1 sessions; %2 kept because a turn runs on them or their workspace could not be removed safely.").arg(aReceipt.Deleted).arg(aReceipt.Kept);
			}
			return aRenaming                ? QObject::tr("Renamed.")
				: aReceipt.WorkspaceRemoved ? QObject::tr("Deleted, with its agent workspace.")
				: aReceipt.WorkspaceShared  ? QObject::tr("Deleted. Its workspace stays: another session of this pull request uses it.")
											: QObject::tr("Deleted.");
		case result::not_found:
			return QObject::tr("That session is no longer here.");
		case result::running:
			return QObject::tr("A turn is running on this session or its workspace. Stop it, wait for it to finish, then try again.");
		case result::name_too_long:
			return QObject::tr("A name can be at most 120 bytes long.");
		case result::name_not_text:
			return QObject::tr("That name has characters a session name can’t hold.");
		case result::workspace_refused:
			return QObject::tr("Its agent workspace could not be removed safely, so the session was kept.");
		case result::unavailable:
			break;
		}
		return QObject::tr("The sessions could not be read or saved. Nothing changed.");
	}

	agent_session_menu::agent_session_menu(QWidget& aOwner, QObject* const aParent)
		: QWidgetAction{ aParent }
		, Owner{ aOwner }
	{
		setObjectName(QStringLiteral("agentSessionRows"));
		DeleteAll = new QAction{ tr("Delete all sessions…"), this };
		DeleteAll->setObjectName(QStringLiteral("agentSessionDeleteAll"));
		DeleteAll->setToolTip(tr("Delete every session listed here: their transcripts, names and costs, and their workspaces where no other session uses them"));
		connect(DeleteAll, &QAction::triggered, this, [this] { ask_delete_all(); });
		refresh();
	}

	void agent_session_menu::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void agent_session_menu::present(const application::native_agent_history_projection& aHistory)
	{
		if (aHistory.Loading || aHistory.ReadOnly) {
			return;
		}
		History = aHistory;
		refresh();
	}

	void agent_session_menu::set_running(const bool aRunning)
	{
		if (Running != aRunning) {
			Running = aRunning;
			refresh();
		}
	}

	void agent_session_menu::present_status(const QString& aStatus)
	{
		Status = aStatus;
		refresh();
	}

	auto agent_session_menu::delete_all_action() const noexcept -> QAction* { return DeleteAll; }
	auto agent_session_menu::question() const noexcept -> destructive_question* { return Question.data(); }

	auto agent_session_menu::ids() const -> std::vector<std::string>
	{
		auto found = std::vector<std::string>{ };
		for (const auto& each : History.Entries) {
			found.push_back(each.Id);
		}
		return found;
	}

	auto agent_session_menu::selected() const -> std::string { return History.Selected; }

	auto agent_session_menu::entry(const std::string& aId) const -> const application::native_agent_history_entry*
	{
		const auto found = std::ranges::find(History.Entries, aId, &application::native_agent_history_entry::Id);
		return found == History.Entries.end() ? nullptr : &*found;
	}

	auto agent_session_menu::turns(const std::string& aId) const -> std::size_t
	{
		const auto* const found = entry(aId);
		return found != nullptr ? found->Turns : 0U;
	}

	auto agent_session_menu::replay(const std::string& aId) const -> bool
	{
		const auto* const found = entry(aId);
		return found != nullptr && found->Replay;
	}

	void agent_session_menu::close_popups()
	{
		for (auto* popup = QApplication::activePopupWidget(); popup != nullptr; popup = QApplication::activePopupWidget()) {
			if (!popup->close()) {
				break;
			}
		}
	}

	void agent_session_menu::open(const std::string& aId)
	{
		if (entry(aId) == nullptr || Running) {
			return;
		}
		close_popups();
		if (Dispatch.Open) {
			Dispatch.Open(aId);
		}
	}

	void agent_session_menu::rename(const std::string& aId, const QString& aTyped)
	{
		const auto* const found = entry(aId);
		if (found == nullptr) {
			return;
		}
		const auto typed = aTyped.simplified();
		if (typed == QString::fromStdString(found->Title).simplified()) {
			refresh();
			return;
		}
		Status.clear();
		if (Dispatch.Rename) {
			Dispatch.Rename(aId, typed.toStdString());
		}
	}

	void agent_session_menu::ask_delete(const std::string& aId)
	{
		const auto* const found = entry(aId);
		if (found == nullptr || Running || (Question && Question->isVisible())) {
			return;
		}
		close_popups();
		const auto title = QString::fromStdString(found->Title).simplified();
		auto*      ask   = new destructive_question(Owner, QStringLiteral("deleteAgentSession"), tr("Delete “%1”?").arg(title.left(60)),
													tr("BetterPullReview forgets this session: its transcript, name and cost. Its agent workspace is removed too, unless another session of this pull request uses it. "
													   "The agent’s own session files, such as Claude Code’s or Codex’s, are left alone. This cannot be undone."),
													tr("Delete session"));
		connect(ask, &QDialog::finished, ask, &QObject::deleteLater);
		connect(ask, &QDialog::accepted, this, [this, aId] {
			Status.clear();
			if (Dispatch.Delete) {
				Dispatch.Delete(aId);
			}
		});
		Question = ask;
		ask->open();
	}

	void agent_session_menu::ask_delete_all()
	{
		if (History.Entries.empty() || Running || (Question && Question->isVisible())) {
			return;
		}
		close_popups();
		const auto count = History.Entries.size();
		auto*      ask   = new destructive_question(Owner, QStringLiteral("deleteAllAgentSessions"), count == 1U ? tr("Delete the 1 session?") : tr("Delete all %1 sessions?").arg(count),
													tr("BetterPullReview forgets every session listed: their transcripts, names and costs. Their agent workspaces are removed too, unless another session of this pull request uses them. "
													   "A session a turn is running on is kept. The agents’ own session files are left alone. This cannot be undone."),
													tr("Delete all"));
		connect(ask, &QDialog::finished, ask, &QObject::deleteLater);
		connect(ask, &QDialog::accepted, this, [this] {
			Status.clear();
			if (Dispatch.DeleteAll) {
				Dispatch.DeleteAll();
			}
		});
		Question = ask;
		ask->open();
	}

	auto agent_session_menu::createWidget(QWidget* const aParent) -> QWidget*
	{
		auto* const rows = new QWidget{ aParent };
		rows->setObjectName(QStringLiteral("agentSessionList"));
		auto* const column = new QVBoxLayout{ rows };
		column->setContentsMargins(6, 2, 6, 2);
		column->setSpacing(0);
		fill(*rows);
		return rows;
	}

	void agent_session_menu::refresh()
	{
		DeleteAll->setEnabled(!History.Entries.empty() && !Running);
		DeleteAll->setToolTip(Running ? tr("Stop the running turn first") : tr("Delete every session listed here: their transcripts, names and costs, and their workspaces where no other session uses them"));
		for (auto* const rows : createdWidgets()) {
			fill(*rows);
		}
	}

	void agent_session_menu::fill(QWidget& aRows)
	{
		auto* const column = static_cast<QVBoxLayout*>(aRows.layout());
		while (auto* const item = column->takeAt(0)) {
			if (auto* const widget = item->widget()) {
				// Out of the list at once; gone once any event it is handling ends.
				widget->hide();
				widget->setParent(nullptr);
				widget->deleteLater();
			}
			delete item;
		}
		if (History.Entries.empty()) {
			auto* const none = new QLabel{ tr("No sessions yet"), &aRows };
			none->setForegroundRole(QPalette::PlaceholderText);
			column->addWidget(none);
		}
		for (const auto& each : History.Entries) {
			const auto  id     = each.Id;
			const auto  isOpen = id == History.Selected;
			const auto  title  = QString::fromStdString(each.Title).simplified();
			auto* const row    = new QWidget{ &aRows };
			row->setObjectName(QStringLiteral("agentSessionRow"));
			row->setProperty("sessionId", QString::fromStdString(id));
			auto* const line = new QHBoxLayout{ row };
			line->setContentsMargins(4, 1, 0, 1);
			line->setSpacing(2);
			auto* const name = new session_title{ aRows.fontMetrics().elidedText(title, Qt::ElideRight, 260), [this, id] { open(id); }, row };
			name->setObjectName(QStringLiteral("agentSessionTitle"));
			name->setToolTip(title);
			name->setAccessibleName(isOpen ? tr("%1, the open session").arg(title) : title);
			name->setEnabled(!Running || isOpen);
			if (isOpen) {
				auto font = name->font();
				font.setBold(true);
				name->setFont(font);
			}
			auto* const field = new rename_field{ [this, id](const QString& aTyped) { rename(id, aTyped); }, [this] { refresh(); }, row };
			field->setObjectName(QStringLiteral("agentSessionRenameField"));
			field->setPlaceholderText(tr("Name; empty for the first question"));
			field->setAccessibleName(tr("Session name"));
			field->hide();
			line->addWidget(name, 1);
			line->addWidget(field, 1);
			auto* const pencil = row_tool(row, drawer_glyph::rename, tr("Rename"), QStringLiteral("agentSessionRename"));
			pencil->setToolTip(tr("Rename. Clear the name to show the first question again."));
			connect(pencil, &QToolButton::clicked, row, [name, field, each] {
				name->hide();
				field->setText(QString::fromStdString(each.Name.empty() ? each.Title : each.Name));
				field->show();
				field->selectAll();
				field->setFocus();
			});
			auto* const trash = row_tool(row, drawer_glyph::trash, tr("Delete"), QStringLiteral("agentSessionDelete"));
			trash->setEnabled(!Running);
			trash->setToolTip(Running ? tr("Stop the running turn first") : tr("Delete this session"));
			connect(trash, &QToolButton::clicked, this, [this, id] { ask_delete(id); });
			auto* const info = row_tool(row, drawer_glyph::info, tr("About this session"), QStringLiteral("agentSessionInfo"));
			const auto  said = info_text(each, isOpen);
			info->setToolTip(said);
			info->setAccessibleDescription(said);
			connect(info, &QToolButton::clicked, info, [info] { show_anchored_tooltip(*info, info->rect(), info->toolTip(), anchored_tooltip_hold::focus); });
			line->addWidget(pencil);
			line->addWidget(trash);
			line->addWidget(info);
			column->addWidget(row);
		}
		if (!Status.isEmpty()) {
			auto* const status = new QLabel{ Status, &aRows };
			status->setObjectName(QStringLiteral("agentSessionStatus"));
			status->setWordWrap(true);
			status->setForegroundRole(QPalette::PlaceholderText);
			column->addWidget(status);
		}
	}
}
