#include "bpr/qt/private_review_panel.hpp"
#include "bpr/qt/destructive_question.hpp"
#include <QAccessible>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
namespace bpr::qt {
	namespace {
		auto text(std::string_view aValue) -> QString { return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size())); }
		auto actor(const core::private_review_actor& aValue) -> QString
		{
			if (const auto* agent = std::get_if<core::private_agent_actor>(&aValue)) {
				return QStringLiteral("Agent · run %1 · model/effort in provenance").arg(agent->Run.value());
			}
			return std::holds_alternative<core::private_reviewer_actor>(aValue) ? QStringLiteral("Reviewer") : QStringLiteral("Legacy · author unknown");
		}
	}
	auto private_thread_model::rowCount(const QModelIndex& aParent) const -> int { return aParent.isValid() ? 0 : static_cast<int>(Rows.size()); }
	auto private_thread_model::data(const QModelIndex& aIndex, int aRole) const -> QVariant
	{
		if (!aIndex.isValid() || aIndex.row() < 0 || aIndex.row() >= rowCount()) {
			return { };
		}
		const auto& row = Rows[static_cast<std::size_t>(aIndex.row())];
		if (aRole == Qt::UserRole) {
			return QVariant::fromValue<qulonglong>(row.Thread.Id.value());
		}
		if (aRole == Qt::DisplayRole || aRole == Qt::AccessibleTextRole || aRole == Qt::ToolTipRole) {
			return QStringLiteral("Private thread %1 · %2 · %3").arg(row.Thread.Id.value()).arg(text(core::stable_name(row.Thread.Lifecycle)), text(core::stable_name(row.Availability)));
		}
		return { };
	}
	void private_thread_model::present(const std::vector<application::private_thread_row>& aRows)
	{
		if (aRows.size() > 64) {
			return;
		}
		for (int i = rowCount() - 1; i >= 0; --i) {
			if (std::ranges::none_of(aRows, [&](const auto& aRow) { return aRow.Thread.Id == Rows[static_cast<std::size_t>(i)].Thread.Id; })) {
				beginRemoveRows({ }, i, i);
				Rows.erase(Rows.begin() + i);
				endRemoveRows();
			}
		}
		for (std::size_t i = 0; i < aRows.size(); ++i) {
			auto found = std::ranges::find_if(Rows, [&](const auto& aRow) { return aRow.Thread.Id == aRows[i].Thread.Id; });
			if (found == Rows.end()) {
				beginInsertRows({ }, static_cast<int>(i), static_cast<int>(i));
				Rows.insert(Rows.begin() + static_cast<std::ptrdiff_t>(i), aRows[i]);
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
				if (Rows[i] != aRows[i]) {
					Rows[i] = aRows[i];
					emit dataChanged(index(static_cast<int>(i)), index(static_cast<int>(i)));
				}
			}
		}
	}
	auto private_turn_model::rowCount(const QModelIndex& aParent) const -> int { return aParent.isValid() ? 0 : static_cast<int>(Rows.size()); }
	auto private_turn_model::data(const QModelIndex& aIndex, int aRole) const -> QVariant
	{
		if (!aIndex.isValid() || aIndex.row() < 0 || aIndex.row() >= rowCount()) {
			return { };
		}
		const auto& row = Rows[static_cast<std::size_t>(aIndex.row())];
		if (aRole == Qt::UserRole) {
			return QVariant::fromValue<qulonglong>(row.Revision.Identity.Revision.value());
		}
		if (aRole == Qt::DisplayRole || aRole == Qt::AccessibleTextRole || aRole == Qt::ToolTipRole) {
			return QStringLiteral("Turn %1 · revision %2\n%3 · %4 · %5%6%7").arg(row.Revision.Identity.Turn.value()).arg(row.Revision.Identity.Revision.value()).arg(actor(row.Revision.Actor), text(core::stable_name(row.Revision.Completion)), text(core::stable_name(row.Availability)), row.Selection.Historical ? QStringLiteral(" · historical") : QString{ }, row.Lineage.empty() ? QString{ } : QStringLiteral("\n") + text(row.Lineage));
		}
		return { };
	}
	void private_turn_model::present(const std::vector<application::private_turn_row>& aRows)
	{
		if (aRows.size() > 64) {
			return;
		}
		for (int i = rowCount() - 1; i >= 0; --i) {
			if (std::ranges::none_of(aRows, [&](const auto& aRow) { return aRow.Revision.Identity == Rows[static_cast<std::size_t>(i)].Revision.Identity; })) {
				beginRemoveRows({ }, i, i);
				Rows.erase(Rows.begin() + i);
				endRemoveRows();
			}
		}
		for (std::size_t i = 0; i < aRows.size(); ++i) {
			auto found = std::ranges::find_if(Rows, [&](const auto& aRow) { return aRow.Revision.Identity == aRows[i].Revision.Identity; });
			if (found == Rows.end()) {
				beginInsertRows({ }, static_cast<int>(i), static_cast<int>(i));
				Rows.insert(Rows.begin() + static_cast<std::ptrdiff_t>(i), aRows[i]);
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
				if (Rows[i] != aRows[i]) {
					Rows[i] = aRows[i];
					emit dataChanged(index(static_cast<int>(i)), index(static_cast<int>(i)));
				}
			}
		}
	}
	private_review_panel::private_review_panel(QWidget* aParent)
		: QWidget(aParent)
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
		auto* tabs = new QTabWidget(this);
		tabs->setObjectName(QStringLiteral("privateReviewPages"));
		auto* history       = new QWidget(tabs);
		auto* sourcesPage   = new QWidget(tabs);
		auto* writePage     = new QWidget(tabs);
		auto* historyLayout = new QVBoxLayout(history);
		auto* sourcesLayout = new QVBoxLayout(sourcesPage);
		auto* writeLayout   = new QVBoxLayout(writePage);
		tabs->addTab(history, tr("History"));
		tabs->addTab(sourcesPage, tr("Sources"));
		tabs->addTab(writePage, tr("Write"));
		layout->addWidget(tabs);
		layout     = historyLayout;
		Threads    = new private_thread_model(this);
		ThreadList = new QListView(this);
		ThreadList->setObjectName(QStringLiteral("privateThreadList"));
		ThreadList->setAccessibleName(tr("Private threads, one page"));
		ThreadList->setModel(Threads);
		ThreadList->setFixedHeight(84);
		layout->addWidget(ThreadList);
		auto button = [&](const QString& aLabel, const QString& aName, application::private_conversation_action aAction) { auto* b = new QPushButton(aLabel, this); b->setObjectName(aName); b->setAccessibleName(aLabel); layout->addWidget(b); connect(b, &QPushButton::clicked, this, [this, aAction] { emit_action(aAction); }); return b; };
		button(tr("Refresh private history"), QStringLiteral("privateRefresh"), application::private_conversation_action::refresh);
		auto* archived = new QCheckBox(tr("Include archived threads"), this);
		archived->setObjectName(QStringLiteral("privateIncludeArchived"));
		layout->addWidget(archived);
		connect(archived, &QCheckBox::clicked, this, [this] { emit_action(application::private_conversation_action::include_archived); });
		MoreThreads = button(tr("Next thread page"), QStringLiteral("privateMoreThreads"), application::private_conversation_action::next_threads);
		Turns       = new private_turn_model(this);
		TurnList    = new QListView(this);
		TurnList->setObjectName(QStringLiteral("privateTurnList"));
		TurnList->setAccessibleName(tr("Private turns and immutable revision history"));
		TurnList->setWordWrap(true);
		TurnList->setModel(Turns);
		TurnList->setFixedHeight(150);
		layout->addWidget(TurnList);
		MoreTurns = button(tr("Next revision page"), QStringLiteral("privateMoreTurns"), application::private_conversation_action::next_turns);
		Body      = new QPlainTextEdit(this);
		Body->setReadOnly(true);
		Body->setObjectName(QStringLiteral("privateTurnBody"));
		Body->setAccessibleName(tr("Exact private revision body, current chunk"));
		Body->setMinimumHeight(100);
		Body->setMaximumHeight(160);
		layout->addWidget(Body);
		MoreBody = button(tr("Next body chunk"), QStringLiteral("privateMoreBody"), application::private_conversation_action::read_body);
		button(tr("Inspect provenance"), QStringLiteral("privateProvenance"), application::private_conversation_action::provenance);
		Provenance = new QPlainTextEdit(this);
		Provenance->setReadOnly(true);
		Provenance->setObjectName(QStringLiteral("privateProvenanceDetails"));
		Provenance->setAccessibleName(tr("Requested, configured, reported and self-reported provenance"));
		Provenance->setMaximumHeight(160);
		layout->addWidget(Provenance);
		layout     = sourcesLayout;
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
		Findings = new QComboBox(this);
		Findings->setObjectName(QStringLiteral("privateFindingSources"));
		Findings->setAccessibleName(tr("Exact retained findings, bounded page"));
		layout->addWidget(Findings);
		button(tr("Next retained findings"), QStringLiteral("privateMoreFindings"), application::private_conversation_action::next_findings);
		auto* findingSource = new QPushButton(tr("Select exact finding as source"), this);
		findingSource->setObjectName(QStringLiteral("privateSelectFinding"));
		layout->addWidget(findingSource);
		connect(findingSource, &QPushButton::clicked, this, [this] {
			const auto row = Findings->currentIndex();
			if (row < 0 || static_cast<std::size_t>(row) >= Shown.Findings.size() || Sources.size() >= core::maximum_private_sources) {
				return;
			}
			const auto& finding = Shown.Findings[static_cast<std::size_t>(row)];
			if (std::ranges::none_of(Sources, [&](const auto& aSelected) { return aSelected.Source == finding.Source; })) {
				Sources.push_back({ finding.Source, finding.Latest, false });
			}
			Selection->setText(tr("%1 exact sources selected").arg(Sources.size()));
		});
		layout        = writeLayout;
		auto* profile = new QComboBox(this);
		profile->setObjectName(QStringLiteral("privateAgentProfile"));
		profile->setAccessibleName(tr("Agent profile for private question"));
		layout->addWidget(profile);
		for (const auto& field : { "Model", "Effort" }) {
			auto* input = new QLineEdit(this);
			input->setObjectName(QStringLiteral("privateRequested") + QString::fromLatin1(field));
			input->setPlaceholderText(tr("Requested %1 (optional)").arg(QString::fromLatin1(field)));
			input->setAccessibleName(input->placeholderText());
			layout->addWidget(input);
		}
		layout = sourcesLayout;
		button(tr("Inspect selected finding"), QStringLiteral("privateInspectFinding"), application::private_conversation_action::inspect_finding);
		auto* findingDetails = new QPlainTextEdit(this);
		findingDetails->setObjectName(QStringLiteral("privateFindingDetails"));
		findingDetails->setAccessibleName(tr("Exact retained finding and verdict"));
		findingDetails->setReadOnly(true);
		findingDetails->setMaximumHeight(200);
		layout->addWidget(findingDetails);
		layout   = writeLayout;
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
		layout       = sourcesLayout;
		auto* drafts = new QComboBox(this);
		drafts->setObjectName(QStringLiteral("privateAcceptedDrafts"));
		drafts->setAccessibleName(tr("Exact retained local drafts"));
		layout->addWidget(drafts);
		button(tr("Open selected local draft"), QStringLiteral("privateOpenDraft"), application::private_conversation_action::open_draft);
		button(tr("Squash selected sources to draft…"), QStringLiteral("privateCompose"), application::private_conversation_action::compose);
		layout = writeLayout;
		button(tr("Archive private thread"), QStringLiteral("privateArchive"), application::private_conversation_action::archive);
		button(tr("Delete private thread"), QStringLiteral("privateDelete"), application::private_conversation_action::erase);
		historyLayout->addStretch();
		sourcesLayout->addStretch();
		writeLayout->addStretch();
		connect(ThreadList->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { if (!Applying) { emit_action(application::private_conversation_action::select_thread); } });
		connect(TurnList->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { if (!Applying) { select_turn(); } });
		present({ });
	}
	void private_review_panel::show_saved_comments()
	{
		if (SavedComments) {
			SavedComments->raise();
			return;
		}
		auto* dialog = new QDialog(this);
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->setObjectName(QStringLiteral("savedComments"));
		dialog->setWindowTitle(tr("Saved comments"));
		dialog->resize(680, 460);
		auto* rows        = new QVBoxLayout(dialog);
		auto* explanation = new QLabel(tr("Comments stay saved under their original pull request, account and revision. Inspect or copy their text here; nothing is published."), dialog);
		explanation->setWordWrap(true);
		rows->addWidget(explanation);
		auto* status = new QLabel(dialog);
		status->setObjectName(QStringLiteral("savedCommentsStatus"));
		status->setWordWrap(true);
		status->setTextFormat(Qt::PlainText);
		rows->addWidget(status);
		auto* choices = new QComboBox(dialog);
		choices->setObjectName(QStringLiteral("savedCommentChoices"));
		choices->setAccessibleName(tr("Retained comments"));
		rows->addWidget(choices);
		auto* authority = new QLabel(dialog);
		authority->setObjectName(QStringLiteral("savedCommentAuthority"));
		authority->setWordWrap(true);
		authority->setTextFormat(Qt::PlainText);
		authority->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		rows->addWidget(authority);
		auto* body = new QPlainTextEdit(dialog);
		body->setObjectName(QStringLiteral("savedCommentBody"));
		body->setAccessibleName(tr("Saved comment text"));
		body->setReadOnly(true);
		rows->addWidget(body, 1);
		auto* discard = new QPushButton(tr("Discard unpublished draft…"), dialog);
		discard->setObjectName(QStringLiteral("savedCommentDiscard"));
		rows->addWidget(discard, 0, Qt::AlignLeft);
		connect(discard, &QPushButton::clicked, this, [this, dialog] {
			const auto selected = dialog->findChild<QComboBox*>(QStringLiteral("savedCommentChoices"))->currentIndex();
			if (selected < 0 || static_cast<std::size_t>(selected) >= Shown.Drafts.size()) {
				return;
			}
			const auto draft      = Shown.Drafts[static_cast<std::size_t>(selected)];
			const auto generation = Shown.Generation;
			const auto scope      = Shown.SavedScope;
			if (!scope || draft.State != core::review_draft_state::drafting || draft.Unused) {
				return;
			}
			auto* question = new destructive_question(*dialog, QStringLiteral("discardSavedDraft"), tr("Discard this unpublished draft?"), tr("Its text stays saved in history. This frees its active slot and does not delete a GitHub comment. This cannot be undone."), tr("Discard draft"));
			connect(question, &QDialog::accepted, this, [this, generation, scope, id = draft.Id, revision = draft.Revision] {
				// The host rechecks the captured account/PR scope and exact revision.
				// A presentation refresh while this question is open must not
				// silently swallow an already confirmed intent.
				if (Dispatch) {
					Dispatch(application::private_conversation_intent{ .Action = application::private_conversation_action::discard_saved_draft, .Generation = generation, .ExpectedVersion = revision, .Draft = id, .SavedScope = scope });
				}
			});
			question->open();
		});
		auto* cleanup = new QPushButton(tr("Free unused empty drafts…"), dialog);
		cleanup->setObjectName(QStringLiteral("savedCommentsFreeUnused"));
		rows->addWidget(cleanup, 0, Qt::AlignLeft);
		auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
		rows->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
		connect(choices, &QComboBox::currentIndexChanged, this, [this] { show_saved_comment(); });
		connect(cleanup, &QPushButton::clicked, this, [this, dialog] {
			const auto generation = Shown.Generation;
			const auto scope      = Shown.SavedScope;
			auto*      question   = new destructive_question(*dialog, QStringLiteral("clearUnusedDrafts"), tr("Free unused empty drafts?"),
															 tr("Only empty placeholders that were never edited will be removed. Written comments and pending publications are kept. This cannot be undone."), tr("Free empty drafts"));
			connect(question, &QDialog::accepted, this, [this, generation, scope] {
				if (Dispatch && scope) {
					Dispatch(application::private_conversation_intent{ .Action = application::private_conversation_action::clear_unused_drafts, .Generation = generation, .SavedScope = scope });
				}
			});
			question->open();
		});
		SavedComments = dialog;
		connect(dialog, &QDialog::finished, this, [this, dialog] { if(SavedComments == dialog) { SavedComments = nullptr; } });
		present_saved_comments();
		dialog->show();
	}
	void private_review_panel::present_saved_comments()
	{
		if (!SavedComments) {
			return;
		}
		auto* choices = SavedComments->findChild<QComboBox*>(QStringLiteral("savedCommentChoices"));
		SavedComments->findChild<QLabel*>(QStringLiteral("savedCommentsStatus"))->clear();
		const auto           selected = choices->currentData();
		const QSignalBlocker blocked(choices);
		choices->clear();
		for (const auto& draft : Shown.Drafts) {
			const auto state = draft.State == core::review_draft_state::publishing ? tr("Publication unconfirmed") : draft.State == core::review_draft_state::published ? tr("Published")
				: draft.State == core::review_draft_state::discarded                                                                                                    ? tr("Discarded")
				: draft.Unused                                                                                                                                          ? tr("Unused empty draft")
																																										: tr("Unpublished");
			choices->addItem(tr("%1 · %2line %3 · %4").arg(state, draft.Anchor.Side == core::diff_side::left ? tr("base ") : QString{ }).arg(draft.Anchor.Line + 1U).arg(text(draft.Head).left(12)), QVariant::fromValue<qulonglong>(draft.Id.value()));
		}
		auto picked = choices->findData(selected);
		if (picked < 0) {
			const auto first = std::ranges::find_if(Shown.Drafts, [](const auto& draft) { return !draft.Body.empty(); });
			picked           = first != Shown.Drafts.end() ? static_cast<int>(first - Shown.Drafts.begin()) : 0;
		}
		choices->setCurrentIndex(picked);
		SavedComments->findChild<QPushButton*>(QStringLiteral("savedCommentsFreeUnused"))->setEnabled(Shown.Available && Shown.SavedScope && !Shown.Busy && std::ranges::any_of(Shown.Drafts, [](const auto& draft) { return draft.Unused; }));
		show_saved_comment();
	}
	void private_review_panel::show_saved_comment()
	{
		if (!SavedComments) {
			return;
		}
		const auto index     = SavedComments->findChild<QComboBox*>(QStringLiteral("savedCommentChoices"))->currentIndex();
		auto*      body      = SavedComments->findChild<QPlainTextEdit*>(QStringLiteral("savedCommentBody"));
		auto*      authority = SavedComments->findChild<QLabel*>(QStringLiteral("savedCommentAuthority"));
		SavedComments->findChild<QPushButton*>(QStringLiteral("savedCommentDiscard"))->setEnabled(false);
		if (index < 0 || static_cast<std::size_t>(index) >= Shown.Drafts.size()) {
			body->clear();
			authority->setText(Shown.Available ? tr("No saved comments in this pull request.") : tr("Open this pull request with its original account and wait for private storage to load."));
			return;
		}
		const auto& draft = Shown.Drafts[static_cast<std::size_t>(index)];
		SavedComments->findChild<QPushButton*>(QStringLiteral("savedCommentDiscard"))->setEnabled(Shown.Available && Shown.SavedScope && !Shown.Busy && draft.State == core::review_draft_state::drafting && !draft.Unused);
		body->setPlainText(text(draft.Body));
		auto details = tr("Original revision: %1").arg(text(draft.Head));
		if (!draft.Path.empty()) {
			details += tr("\nFile: %1").arg(text(draft.Path));
		}
		if (draft.State == core::review_draft_state::publishing) {
			details += tr("\nPublication is unconfirmed. Check publication status before reusing this text; it may already be on GitHub.");
		} else if (!draft.Current && draft.State == core::review_draft_state::drafting) {
			details += tr("\nEarlier comparison. Check current code before copying this text into a new comment. The original stays saved.");
		}
		authority->setText(details);
	}
	void private_review_panel::set_dispatch(application::private_conversation_dispatch aDispatch) { Dispatch = std::move(aDispatch); }
	void private_review_panel::present(const application::private_conversation_projection& aProjection)
	{
		Applying = true;
		if (Shown.Generation != aProjection.Generation) {
			Sources.clear();
			Selection->setText(tr("0 exact sources selected"));
			Composer->clear();
			Provenance->clear();
		}
		Shown = aProjection;
		present_saved_comments();
		findChild<QCheckBox*>(QStringLiteral("privateIncludeArchived"))->setChecked(aProjection.IncludeArchived);
		auto*      drafts      = findChild<QComboBox*>(QStringLiteral("privateAcceptedDrafts"));
		const auto chosenDraft = drafts->currentData();
		drafts->clear();
		for (const auto& draft : aProjection.Drafts) {
			const auto state = draft.State == core::review_draft_state::drafting ? tr("Draft") : draft.State == core::review_draft_state::publishing ? tr("Publishing")
				: draft.State == core::review_draft_state::published                                                                                 ? tr("Published")
																																					 : tr("Discarded");
			drafts->addItem(tr("%1 %2 · line %3").arg(state).arg(draft.Id.value()).arg(draft.Anchor.Line + 1), QVariant::fromValue<qulonglong>(draft.Id.value()));
		}
		if (drafts->findData(chosenDraft) >= 0) {
			drafts->setCurrentIndex(drafts->findData(chosenDraft));
		}
		auto*      profile = findChild<QComboBox*>(QStringLiteral("privateAgentProfile"));
		const auto chosen  = profile->currentData();
		profile->clear();
		for (const auto& item : aProjection.Profiles) {
			profile->addItem(text(item.second), QVariant::fromValue<qulonglong>(item.first.value()));
		}
		if (profile->findData(chosen) >= 0) {
			profile->setCurrentIndex(profile->findData(chosen));
		}
		findChild<QPushButton*>(QStringLiteral("privateMoreFindings"))->setEnabled(aProjection.MoreFindings);
		Findings->clear();
		for (const auto& finding : aProjection.Findings) {
			const auto* key = std::get_if<core::private_finding_source>(&finding.Source.Key);
			if (key) {
				Findings->addItem(tr("Run %1 · finding %2 · %3").arg(key->Run.value()).arg(key->Ordinal).arg(text(core::stable_name(finding.Availability))));
			}
		}

		Threads->present(aProjection.Threads);
		Turns->present(aProjection.Turns);
		for (int i = 0; i < Threads->rowCount(); ++i) {
			if (aProjection.SelectedThread == Threads->rows()[static_cast<std::size_t>(i)].Thread.Id) {
				ThreadList->setCurrentIndex(Threads->index(i));
				break;
			}
		}
		MoreThreads->setEnabled(aProjection.MoreThreads);
		MoreTurns->setEnabled(aProjection.MoreTurns);
		setEnabled(aProjection.Available && !aProjection.Busy);
		show_status(text(aProjection.Status));
		select_turn();
		Applying = false;
	}
	void private_review_panel::show_status(const QString& aStatus)
	{
		if (SavedComments && !Applying) {
			SavedComments->findChild<QLabel*>(QStringLiteral("savedCommentsStatus"))->setText(aStatus);
		}
		if (Status->text() != aStatus) {
			Status->setText(aStatus);
			Status->setAccessibleDescription(aStatus);
			auto event = QAccessibleAnnouncementEvent{ this, aStatus };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
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
		if (std::ranges::none_of(Sources, [&](const auto& aSource) { return aSource.Source == turn.Selection.Source; })) {
			Sources.push_back(turn.Selection);
		}
		Selection->setText(tr("%1 exact sources selected; content is sent only by Ask agent or agent-assisted composition.").arg(Sources.size()));
	}
	void private_review_panel::emit_action(application::private_conversation_action aAction)
	{
		if (Applying || !Dispatch || !Shown.Available || Shown.Busy) {
			return;
		}
		auto       intent = application::private_conversation_intent{ .Action = aAction, .Generation = Shown.Generation, .Thread = Shown.SelectedThread, .ExpectedVersion = Shown.SelectedVersion, .Sources = Sources, .Body = Composer->toPlainText().toStdString() };
		const auto thread = ThreadList->currentIndex().row();
		if (aAction == application::private_conversation_action::select_thread && thread >= 0 && thread < Threads->rowCount()) {
			intent.Thread          = Threads->rows()[static_cast<std::size_t>(thread)].Thread.Id;
			intent.ExpectedVersion = Threads->rows()[static_cast<std::size_t>(thread)].Thread.Version;
		}
		const auto turn = TurnList->currentIndex().row();
		if (turn >= 0 && turn < Turns->rowCount()) {
			intent.Revision = Turns->rows()[static_cast<std::size_t>(turn)].Selection;
		}
		intent.Profile             = core::agent_profile_id{ findChild<QComboBox*>(QStringLiteral("privateAgentProfile"))->currentData().toULongLong() };
		const auto requestedModel  = findChild<QLineEdit*>(QStringLiteral("privateRequestedModel"))->text();
		const auto requestedEffort = findChild<QLineEdit*>(QStringLiteral("privateRequestedEffort"))->text();
		if (!requestedModel.isEmpty()) {
			intent.Requested.Model = requestedModel.toStdString();
		}
		if (!requestedEffort.isEmpty()) {
			intent.Requested.Effort = requestedEffort.toStdString();
		}
		intent.IncludeArchived = findChild<QCheckBox*>(QStringLiteral("privateIncludeArchived"))->isChecked();
		const auto draft       = findChild<QComboBox*>(QStringLiteral("privateAcceptedDrafts"))->currentData().toULongLong();
		if (draft) {
			intent.Draft = core::review_draft_id{ draft };
		}
		if (aAction == application::private_conversation_action::inspect_finding) {
			const auto row = Findings->currentIndex();
			if (row < 0 || static_cast<std::size_t>(row) >= Shown.Findings.size()) {
				return;
			}
			const auto& value = Shown.Findings[static_cast<std::size_t>(row)];
			intent.Revision   = core::private_source_selection{ value.Source, value.Latest, false };
		}
		Dispatch(std::move(intent));
	}
	void private_review_panel::present_body(const core::private_source_reference& aSource, std::string_view aBody, bool aMore)
	{
		const auto row = TurnList->currentIndex().row();
		if (row >= 0 && row < Turns->rowCount() && Turns->rows()[static_cast<std::size_t>(row)].Selection.Source == aSource) {
			Body->setPlainText(text(aBody));
			MoreBody->setEnabled(aMore);
		}
	}
	void private_review_panel::present_finding(std::string_view aDetail) { findChild<QPlainTextEdit*>(QStringLiteral("privateFindingDetails"))->setPlainText(text(aDetail)); }
	void private_review_panel::present_provenance(const core::agent_run_provenance& aRun)
	{
		auto result = tr("Run %1 · profile %2\nModel and effort remain unknown unless a fact below supplies them.\n").arg(aRun.Run.value()).arg(text(aRun.ProfileDisplayName));
		auto facts  = [&](const QString& aBasis, const core::agent_setting_facts& aValue) { result += aBasis + tr(": model=%1; effort=%2\n").arg(aValue.Model ? text(*aValue.Model) : tr("unknown"), aValue.Effort ? text(*aValue.Effort) : tr("unknown")); };
		facts(tr("Requested"), aRun.Metadata.Requested);
		facts(tr("Configured"), aRun.Metadata.Configured);
		for (const auto& report : aRun.Metadata.ProtocolReported) {
			facts(tr("Protocol reported (%1)").arg(text(report.Basis)), report.Settings);
			result += tr("Tool=%1; session=%2; thread=%3; turn=%4; run=%5\n").arg(report.ToolVersion ? text(*report.ToolVersion) : tr("unknown"), report.RemoteSession ? text(*report.RemoteSession) : tr("unknown"), report.RemoteThread ? text(*report.RemoteThread) : tr("unknown"), report.RemoteTurn ? text(*report.RemoteTurn) : tr("unknown"), report.RemoteRun ? text(*report.RemoteRun) : tr("unknown"));
		}
		facts(tr("Content self-reported (unverified)"), aRun.Metadata.ContentSelfReported);
		const auto conflicts = core::inspect_runtime_metadata(aRun.Metadata);
		result += conflicts && conflicts->empty() ? tr("No conflicting facts reported.") : tr("Conflicting facts retained; no model or effort winner is inferred.");
		Provenance->setPlainText(result);
	}
}
