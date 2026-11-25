#include "bpr/qt/agent_session_list.hpp"

#include "bpr/qt/destructive_question.hpp"

#include "bpr/core/hex.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <utility>

namespace bpr::qt {
	namespace {
		constexpr auto id_role    = Qt::UserRole;
		constexpr auto title_role = Qt::UserRole + 1;
	}

	agent_session_list::agent_session_list(QWidget* const aParent)
		: QDialog{ aParent }
	{
		setObjectName(QStringLiteral("agentSessionManager"));
		setWindowTitle(tr("Agent sessions"));
		setAccessibleName(tr("Agent sessions"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(18, 16, 18, 14);
		column->setSpacing(8);
		auto* const lead = new QLabel{ tr("This pull request’s sessions with this agent, newest first. Rename one to find it again; delete one you no longer need."), this };
		lead->setObjectName(QStringLiteral("agentSessionLead"));
		lead->setWordWrap(true);
		lead->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(lead);
		List = new QListWidget{ this };
		List->setObjectName(QStringLiteral("agentSessionList"));
		List->setAccessibleName(tr("Sessions"));
		List->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
		List->setUniformItemSizes(true);
		List->setWordWrap(false);
		List->setTextElideMode(Qt::ElideRight);
		column->addWidget(List, 1);
		auto* const buttons = new QHBoxLayout;
		Rename              = new QPushButton{ tr("Rename…"), this };
		Rename->setObjectName(QStringLiteral("agentSessionRename"));
		Rename->setToolTip(tr("Give the session a name. Clear the name to show its first question again."));
		Rename->setAutoDefault(false);
		Delete = new question_button{ tr("Delete…"), question_button_role::destructive, this };
		Delete->setObjectName(QStringLiteral("agentSessionDelete"));
		Delete->setToolTip(tr("Delete BetterPullReview’s record and transcript of the session"));
		Delete->setAutoDefault(false);
		auto* const close = new QPushButton{ tr("Close"), this };
		close->setObjectName(QStringLiteral("agentSessionClose"));
		close->setDefault(true);
		buttons->addWidget(Rename);
		buttons->addWidget(Delete);
		buttons->addStretch(1);
		buttons->addWidget(close);
		column->addLayout(buttons);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("agentSessionStatus"));
		Status->setWordWrap(true);
		Status->hide();
		column->addWidget(Status);
		setMinimumSize(420, 320);
		connect(close, &QPushButton::clicked, this, &QDialog::close);
		connect(Rename, &QPushButton::clicked, this, [this] {
			if (auto* const item = List->currentItem(); item != nullptr && !Running) {
				List->editItem(item);
			}
		});
		connect(Delete, &QPushButton::clicked, this, [this] { ask_delete(); });
		connect(List, &QListWidget::currentItemChanged, this, [this] { refresh_buttons(); });
		connect(List, &QListWidget::itemChanged, this, [this](QListWidgetItem* const aItem) { edited(aItem); });
		refresh_buttons();
	}

	void agent_session_list::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void agent_session_list::present(const application::native_agent_history_projection& aHistory)
	{
		if (aHistory.Loading) {
			return;
		}
		const auto           kept = List->currentItem() != nullptr ? List->currentItem()->data(id_role).toString() : QString{ };
		const QSignalBlocker quiet{ List };
		List->clear();
		for (const auto& entry : aHistory.Entries) {
			const auto  title = QString::fromStdString(entry.Title).simplified();
			auto* const item  = new QListWidgetItem{ title, List };
			item->setData(id_role, QString::fromStdString(entry.Id));
			item->setData(title_role, title);
			item->setFlags(item->flags() | Qt::ItemIsEditable);
			const auto head  = QString::fromStdString(std::string{ core::canonical_hex(entry.Binding.HeadOid).view() }).left(7);
			const auto turns = entry.Turns == 1U ? tr("1 turn") : tr("%1 turns").arg(entry.Turns);
			item->setToolTip(entry.Id == aHistory.Selected ? tr("The open session. %1, at %2.").arg(turns, head) : tr("%1, at %2.").arg(turns, head));
			if (entry.Id == aHistory.Selected) {
				auto font = item->font();
				font.setBold(true);
				item->setFont(font);
			}
			if (QString::fromStdString(entry.Id) == kept) {
				List->setCurrentItem(item);
			}
		}
		if (List->currentItem() == nullptr && List->count() > 0) {
			List->setCurrentRow(0);
		}
		refresh_buttons();
	}

	void agent_session_list::set_running(const bool aRunning)
	{
		Running = aRunning;
		refresh_buttons();
	}

	void agent_session_list::present_status(const QString& aStatus)
	{
		Status->setText(aStatus);
		Status->setVisible(!aStatus.isEmpty());
	}

	auto agent_session_list::list() const noexcept -> QListWidget* { return List; }
	auto agent_session_list::rename_button() const noexcept -> QPushButton* { return Rename; }
	auto agent_session_list::delete_button() const noexcept -> QPushButton* { return Delete; }
	auto agent_session_list::status_label() const noexcept -> QLabel* { return Status; }
	auto agent_session_list::question() const noexcept -> destructive_question* { return Question.data(); }

	void agent_session_list::refresh_buttons()
	{
		const auto chosen = List->currentItem() != nullptr;
		Rename->setEnabled(chosen && !Running);
		Delete->setEnabled(chosen && !Running);
		Delete->setToolTip(Running ? tr("Stop the running turn first") : tr("Delete BetterPullReview’s record and transcript of the session"));
	}

	void agent_session_list::edited(QListWidgetItem* const aItem)
	{
		const auto typed = aItem->text().simplified();
		const auto was   = aItem->data(title_role).toString();
		if (typed == was || !Dispatch) {
			return;
		}
		present_status({ });
		Dispatch(aItem->data(id_role).toString().toStdString(), typed.toStdString());
	}

	void agent_session_list::ask_delete()
	{
		auto* const item = List->currentItem();
		if (item == nullptr || Running || (Question && Question->isVisible())) {
			return;
		}
		const auto id    = item->data(id_role).toString().toStdString();
		const auto title = item->data(title_role).toString();
		auto*      ask   = new destructive_question(*this, QStringLiteral("deleteAgentSession"), tr("Delete “%1”?").arg(title.left(60)),
													tr("BetterPullReview forgets this session: its transcript, name and cost. Its agent workspace is removed too, unless another session of this pull request uses it. "
													   "The agent’s own session files, such as Claude Code’s or Codex’s, are left alone. This cannot be undone."),
													tr("Delete session"));
		connect(ask, &QDialog::finished, ask, &QObject::deleteLater);
		connect(ask, &QDialog::accepted, this, [this, id] {
			present_status({ });
			if (Dispatch) {
				Dispatch(id, std::nullopt);
			}
		});
		Question = ask;
		ask->open();
	}
}
