#include "bpr/qt/private_review_panel.hpp"
#include <QCheckBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
namespace bpr::qt {
	namespace {
		auto text(std::string_view value) -> QString { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
		auto actor(const core::private_review_actor& value) -> QString
		{
			if (const auto* agent = std::get_if<core::private_agent_actor>(&value)) {
				return QStringLiteral("Agent · run %1 · model/effort in provenance").arg(agent->Run.value());
			}
			return std::holds_alternative<core::private_reviewer_actor>(value) ? QStringLiteral("Reviewer") : QStringLiteral("Legacy · author unknown");
		}
	}
	auto private_thread_model::rowCount(const QModelIndex& parent) const -> int { return parent.isValid() ? 0 : static_cast<int>(Rows.size()); }
	auto private_thread_model::data(const QModelIndex& index, int role) const -> QVariant
	{
		if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
			return { };
		}
		const auto& row = Rows[static_cast<std::size_t>(index.row())];
		if (role == Qt::UserRole) {
			return QVariant::fromValue<qulonglong>(row.Thread.Id.value());
		}
		if (role == Qt::DisplayRole || role == Qt::AccessibleTextRole || role == Qt::ToolTipRole) {
			return QStringLiteral("Private thread %1 · %2 · %3").arg(row.Thread.Id.value()).arg(text(core::stable_name(row.Thread.Lifecycle)), text(core::stable_name(row.Availability)));
		}
		return { };
	}
	void private_thread_model::present(const std::vector<application::private_thread_row>& rows)
	{
		if (rows.size() > 64) {
			return;
		}
		for (int i = rowCount() - 1; i >= 0; --i) {
			if (std::ranges::none_of(rows, [&](const auto& row) { return row.Thread.Id == Rows[static_cast<std::size_t>(i)].Thread.Id; })) {
				beginRemoveRows({ }, i, i);
				Rows.erase(Rows.begin() + i);
				endRemoveRows();
			}
		}
		for (std::size_t i = 0; i < rows.size(); ++i) {
			auto found = std::ranges::find_if(Rows, [&](const auto& row) { return row.Thread.Id == rows[i].Thread.Id; });
			if (found == Rows.end()) {
				beginInsertRows({ }, static_cast<int>(i), static_cast<int>(i));
				Rows.insert(Rows.begin() + static_cast<std::ptrdiff_t>(i), rows[i]);
				endInsertRows();
			} else {
				const auto at = static_cast<std::size_t>(found - Rows.begin());
				if (at != i) {
					beginMoveRows({ }, static_cast<int>(at), static_cast<int>(at), { }, static_cast<int>(i));
					auto row = std::move(*found);
					Rows.erase(found);
					Rows.insert(Rows.begin() + static_cast<std::ptrdiff_t>(i), std::move(row));
					endMoveRows();
				}
				if (Rows[i] != rows[i]) {
					Rows[i] = rows[i];
					emit dataChanged(index(static_cast<int>(i)), index(static_cast<int>(i)));
				}
			}
		}
	}
	auto private_turn_model::rowCount(const QModelIndex& parent) const -> int { return parent.isValid() ? 0 : static_cast<int>(Rows.size()); }
	auto private_turn_model::data(const QModelIndex& index, int role) const -> QVariant
	{
		if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
			return { };
		}
		const auto& row = Rows[static_cast<std::size_t>(index.row())];
		if (role == Qt::UserRole) {
			return QVariant::fromValue<qulonglong>(row.Revision.Identity.Revision.value());
		}
		if (role == Qt::DisplayRole || role == Qt::AccessibleTextRole || role == Qt::ToolTipRole) {
			return QStringLiteral("Turn %1 · revision %2\n%3 · %4 · %5%6%7").arg(row.Revision.Identity.Turn.value()).arg(row.Revision.Identity.Revision.value()).arg(actor(row.Revision.Actor), text(core::stable_name(row.Revision.Completion)), text(core::stable_name(row.Availability)), row.Selection.Historical ? QStringLiteral(" · historical") : QString{ }, row.Lineage.empty() ? QString{ } : QStringLiteral("\n") + text(row.Lineage));
		}
		return { };
	}
	void private_turn_model::present(const std::vector<application::private_turn_row>& rows)
	{
		if (rows.size() > 64) {
			return;
		}
		for (int i = rowCount() - 1; i >= 0; --i) {
			if (std::ranges::none_of(rows, [&](const auto& row) { return row.Revision.Identity == Rows[static_cast<std::size_t>(i)].Revision.Identity; })) {
				beginRemoveRows({ }, i, i);
				Rows.erase(Rows.begin() + i);
				endRemoveRows();
			}
		}
		for (std::size_t i = 0; i < rows.size(); ++i) {
			auto found = std::ranges::find_if(Rows, [&](const auto& row) { return row.Revision.Identity == rows[i].Revision.Identity; });
			if (found == Rows.end()) {
				beginInsertRows({ }, static_cast<int>(i), static_cast<int>(i));
				Rows.insert(Rows.begin() + static_cast<std::ptrdiff_t>(i), rows[i]);
				endInsertRows();
			} else {
				const auto at = static_cast<std::size_t>(found - Rows.begin());
				if (at != i) {
					beginMoveRows({ }, static_cast<int>(at), static_cast<int>(at), { }, static_cast<int>(i));
					auto row = std::move(*found);
					Rows.erase(found);
					Rows.insert(Rows.begin() + static_cast<std::ptrdiff_t>(i), std::move(row));
					endMoveRows();
				}
				if (Rows[i] != rows[i]) {
					Rows[i] = rows[i];
					emit dataChanged(index(static_cast<int>(i)), index(static_cast<int>(i)));
				}
			}
		}
	}
	private_review_panel::private_review_panel(QWidget* parent)
		: QWidget(parent)
	{
		setObjectName(QStringLiteral("privateReviewPanel"));
		auto* layout = new QVBoxLayout(this);
		auto* badge  = new QLabel(tr("Private · stored on this device · not visible on GitHub"), this);
		badge->setWordWrap(true);
		badge->setTextFormat(Qt::PlainText);
		badge->setAccessibleName(badge->text());
		layout->addWidget(badge);
		Status = new QLabel(this);
		Status->setWordWrap(true);
		Status->setTextFormat(Qt::PlainText);
		Status->setObjectName(QStringLiteral("privateReviewStatus"));
		layout->addWidget(Status);
		Threads    = new private_thread_model(this);
		ThreadList = new QListView(this);
		ThreadList->setObjectName(QStringLiteral("privateThreadList"));
		ThreadList->setAccessibleName(tr("Private threads, one page"));
		ThreadList->setModel(Threads);
		ThreadList->setMinimumHeight(100);
		layout->addWidget(ThreadList);
		auto button = [&](const QString& label, const QString& name, application::private_conversation_action action) { auto* b = new QPushButton(label, this); b->setObjectName(name); b->setAccessibleName(label); layout->addWidget(b); connect(b, &QPushButton::clicked, this, [this, action] { emit_action(action); }); return b; };
		button(tr("Refresh private history"), QStringLiteral("privateRefresh"), application::private_conversation_action::refresh);
		MoreThreads = button(tr("Next thread page"), QStringLiteral("privateMoreThreads"), application::private_conversation_action::next_threads);
		Turns       = new private_turn_model(this);
		TurnList    = new QListView(this);
		TurnList->setObjectName(QStringLiteral("privateTurnList"));
		TurnList->setAccessibleName(tr("Private turns and immutable revision history"));
		TurnList->setWordWrap(true);
		TurnList->setModel(Turns);
		TurnList->setMinimumHeight(130);
		layout->addWidget(TurnList);
		MoreTurns = button(tr("Next revision page"), QStringLiteral("privateMoreTurns"), application::private_conversation_action::next_turns);
		Body      = new QPlainTextEdit(this);
		Body->setReadOnly(true);
		Body->setObjectName(QStringLiteral("privateTurnBody"));
		Body->setAccessibleName(tr("Exact private revision body, current chunk"));
		Body->setMinimumHeight(100);
		layout->addWidget(Body);
		MoreBody = button(tr("Next body chunk"), QStringLiteral("privateMoreBody"), application::private_conversation_action::read_body);
		button(tr("Inspect provenance"), QStringLiteral("privateProvenance"), application::private_conversation_action::provenance);
		Provenance = new QPlainTextEdit(this);
		Provenance->setReadOnly(true);
		Provenance->setObjectName(QStringLiteral("privateProvenanceDetails"));
		Provenance->setAccessibleName(tr("Requested, configured, reported and self-reported provenance"));
		Provenance->setMaximumHeight(160);
		layout->addWidget(Provenance);
		Historical = new QCheckBox(tr("Explicitly select this historical revision"), this);
		Historical->setObjectName(QStringLiteral("privateHistorical"));
		layout->addWidget(Historical);
		Selection = new QLabel(tr("0 exact sources selected"), this);
		Selection->setWordWrap(true);
		layout->addWidget(Selection);
		auto* source = new QPushButton(tr("Select exact revision as source"), this);
		source->setObjectName(QStringLiteral("privateSelectSource"));
		layout->addWidget(source);
		connect(source, &QPushButton::clicked, this, [this] { select_source(); });
		auto* clear = new QPushButton(tr("Clear selected sources"), this);
		layout->addWidget(clear);
		connect(clear, &QPushButton::clicked, this, [this] { Sources.clear(); Selection->setText(tr("0 exact sources selected")); });
		Composer = new QPlainTextEdit(this);
		Composer->setObjectName(QStringLiteral("privateReviewerText"));
		Composer->setAccessibleName(tr("Private reviewer text or explicit agent question"));
		Composer->setMinimumHeight(100);
		layout->addWidget(Composer);
		button(tr("Create pull request thread"), QStringLiteral("privateCreatePr"), application::private_conversation_action::create_pr);
		button(tr("Create thread at selected code"), QStringLiteral("privateCreateSelected"), application::private_conversation_action::create_selected);
		button(tr("Add reviewer turn"), QStringLiteral("privateReply"), application::private_conversation_action::reply);
		button(tr("Edit as reviewer successor"), QStringLiteral("privateEdit"), application::private_conversation_action::edit);
		button(tr("Ask agent using only selected sources"), QStringLiteral("privateAskAgent"), application::private_conversation_action::ask_agent);
		button(tr("Squash selected sources to draft…"), QStringLiteral("privateCompose"), application::private_conversation_action::compose);
		button(tr("Archive private thread"), QStringLiteral("privateArchive"), application::private_conversation_action::archive);
		button(tr("Delete private thread"), QStringLiteral("privateDelete"), application::private_conversation_action::erase);
		connect(ThreadList->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { if (!Applying) { emit_action(application::private_conversation_action::select_thread); } });
		connect(TurnList->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { if (!Applying) { select_turn(); } });
		present({ });
	}
	void private_review_panel::set_dispatch(application::private_conversation_dispatch dispatch) { Dispatch = std::move(dispatch); }
	void private_review_panel::present(const application::private_conversation_projection& projection)
	{
		Applying = true;
		if (Shown.Generation != projection.Generation) {
			Sources.clear();
			Selection->setText(tr("0 exact sources selected"));
			Composer->clear();
			Provenance->clear();
		}
		Shown = projection;
		Threads->present(projection.Threads);
		Turns->present(projection.Turns);
		for (int i = 0; i < Threads->rowCount(); ++i) {
			if (projection.SelectedThread == Threads->rows()[static_cast<std::size_t>(i)].Thread.Id) {
				ThreadList->setCurrentIndex(Threads->index(i));
				break;
			}
		}
		MoreThreads->setEnabled(projection.MoreThreads);
		MoreTurns->setEnabled(projection.MoreTurns);
		setEnabled(projection.Available && !projection.Busy);
		show_status(text(projection.Status));
		select_turn();
		Applying = false;
	}
	void private_review_panel::show_status(const QString& status)
	{
		if (Status->text() != status) {
			Status->setText(status);
			Status->setAccessibleDescription(status);
		}
	}
	void private_review_panel::select_turn()
	{
		const auto row = TurnList->currentIndex().row();
		Historical->setChecked(false);
		if (row < 0 || row >= Turns->rowCount()) {
			Body->clear();
			MoreBody->setEnabled(false);
			return;
		}
		const auto& turn = Turns->rows()[static_cast<std::size_t>(row)];
		Body->setPlainText(text(turn.Revision.Body));
		MoreBody->setEnabled(turn.BodyContinues);
		Historical->setEnabled(turn.Selection.Historical);
	}
	void private_review_panel::select_source()
	{
		const auto row = TurnList->currentIndex().row();
		if (row < 0 || row >= Turns->rowCount()) {
			return;
		}
		const auto& turn = Turns->rows()[static_cast<std::size_t>(row)];
		if (turn.Selection.Historical && !Historical->isChecked()) {
			show_status(tr("Select the historical revision explicitly before adding it."));
			return;
		}
		if (Sources.size() >= core::maximum_private_sources) {
			show_status(tr("The selection limit is 256 sources."));
			return;
		}
		if (std::ranges::none_of(Sources, [&](const auto& source) { return source.Source == turn.Selection.Source; })) {
			Sources.push_back(turn.Selection);
		}
		Selection->setText(tr("%1 exact sources selected; content is sent only by Ask agent or agent-assisted composition.").arg(Sources.size()));
	}
	void private_review_panel::emit_action(application::private_conversation_action action)
	{
		if (Applying || !Dispatch || !Shown.Available || Shown.Busy) {
			return;
		}
		auto       intent = application::private_conversation_intent{ .Action = action, .Generation = Shown.Generation, .Thread = Shown.SelectedThread, .Sources = Sources, .Body = Composer->toPlainText().toStdString() };
		const auto thread = ThreadList->currentIndex().row();
		if (thread >= 0 && thread < Threads->rowCount()) {
			intent.Thread          = Threads->rows()[static_cast<std::size_t>(thread)].Thread.Id;
			intent.ExpectedVersion = Threads->rows()[static_cast<std::size_t>(thread)].Thread.Version;
		}
		const auto turn = TurnList->currentIndex().row();
		if (turn >= 0 && turn < Turns->rowCount()) {
			intent.Revision = Turns->rows()[static_cast<std::size_t>(turn)].Selection;
		}
		Dispatch(std::move(intent));
	}
	void private_review_panel::present_body(const core::private_source_reference& source, std::string_view body, bool more)
	{
		const auto row = TurnList->currentIndex().row();
		if (row >= 0 && row < Turns->rowCount() && Turns->rows()[static_cast<std::size_t>(row)].Selection.Source == source) {
			Body->setPlainText(text(body));
			MoreBody->setEnabled(more);
		}
	}
	void private_review_panel::present_provenance(const core::agent_run_provenance& run)
	{
		auto result = tr("Run %1 · profile %2\nModel and effort remain unknown unless a fact below supplies them.\n").arg(run.Run.value()).arg(text(run.ProfileDisplayName));
		auto facts  = [&](const QString& basis, const core::agent_setting_facts& value) { result += basis + tr(": model=%1; effort=%2\n").arg(value.Model ? text(*value.Model) : tr("unknown"), value.Effort ? text(*value.Effort) : tr("unknown")); };
		facts(tr("Requested"), run.Metadata.Requested);
		facts(tr("Configured"), run.Metadata.Configured);
		for (const auto& report : run.Metadata.ProtocolReported) {
			facts(tr("Protocol reported (%1)").arg(text(report.Basis)), report.Settings);
			result += tr("Tool=%1; session=%2; thread=%3; turn=%4; run=%5\n").arg(report.ToolVersion ? text(*report.ToolVersion) : tr("unknown"), report.RemoteSession ? text(*report.RemoteSession) : tr("unknown"), report.RemoteThread ? text(*report.RemoteThread) : tr("unknown"), report.RemoteTurn ? text(*report.RemoteTurn) : tr("unknown"), report.RemoteRun ? text(*report.RemoteRun) : tr("unknown"));
		}
		facts(tr("Content self-reported (unverified)"), run.Metadata.ContentSelfReported);
		const auto conflicts = core::inspect_runtime_metadata(run.Metadata);
		result += conflicts && conflicts->empty() ? tr("No conflicting facts reported.") : tr("Conflicting facts retained; no model or effort winner is inferred.");
		Provenance->setPlainText(result);
	}
}
