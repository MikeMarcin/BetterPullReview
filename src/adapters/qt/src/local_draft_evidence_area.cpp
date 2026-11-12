#include "bpr/qt/local_draft_evidence_area.hpp"

#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <ranges>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr int record_role = Qt::UserRole + 1;
		constexpr int kind_role   = Qt::UserRole + 2;

		enum class row_kind : int {
			group,
			claim,
			check,
			result,
			question,
		};

		constexpr int text_column   = 0;
		constexpr int status_column = 1;
		constexpr int use_column    = 2;

		auto text_of(const std::string_view aBytes) -> QString
		{
			return QString::fromUtf8(aBytes.data(), static_cast<qsizetype>(aBytes.size()));
		}

		auto utf8(const QString& aText) -> std::string
		{
			const auto bytes = aText.toUtf8();
			return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
		}

		auto first_line(const std::string& aText) -> QString
		{
			const auto text = text_of(aText);
			const auto end  = text.indexOf(QLatin1Char{ '\n' });
			return end < 0 ? text : text.left(end) + QStringLiteral("…");
		}

		constexpr auto all_kinds    = std::array{ core::claim_kind::bug_fix, core::claim_kind::preserved_behavior, core::claim_kind::compatibility,
												  core::claim_kind::performance, core::claim_kind::formal_property, core::claim_kind::other };
		constexpr auto all_methods  = std::array{ core::check_method::test, core::check_method::benchmark, core::check_method::fuzz, core::check_method::property_test,
												  core::check_method::inspection, core::check_method::model_check, core::check_method::proof, core::check_method::other };
		constexpr auto all_outcomes = std::array{ core::evidence_outcome::pass, core::evidence_outcome::fail, core::evidence_outcome::inconclusive, core::evidence_outcome::not_run,
												  core::evidence_outcome::interrupted };

		auto kind_word(const core::claim_kind aKind) -> QString
		{
			switch (aKind) {
			case core::claim_kind::bug_fix:            return QStringLiteral("Bug fix");
			case core::claim_kind::preserved_behavior: return QStringLiteral("Preserved behavior");
			case core::claim_kind::compatibility:      return QStringLiteral("Compatibility");
			case core::claim_kind::performance:        return QStringLiteral("Performance");
			case core::claim_kind::formal_property:    return QStringLiteral("Formal property");
			case core::claim_kind::other:              return QStringLiteral("Claim");
			}
			return { };
		}

		auto method_word(const core::check_method aMethod) -> QString
		{
			switch (aMethod) {
			case core::check_method::test:          return QStringLiteral("Test");
			case core::check_method::benchmark:     return QStringLiteral("Benchmark");
			case core::check_method::fuzz:          return QStringLiteral("Fuzzing");
			case core::check_method::property_test: return QStringLiteral("Property test");
			case core::check_method::inspection:    return QStringLiteral("Inspection");
			case core::check_method::model_check:   return QStringLiteral("Model check");
			case core::check_method::proof:         return QStringLiteral("Proof");
			case core::check_method::other:         return QStringLiteral("Other check");
			}
			return { };
		}

		auto outcome_word(const core::evidence_outcome aOutcome) -> QString
		{
			switch (aOutcome) {
			case core::evidence_outcome::pass:         return QStringLiteral("Pass");
			case core::evidence_outcome::fail:         return QStringLiteral("Fail");
			case core::evidence_outcome::inconclusive: return QStringLiteral("Inconclusive");
			case core::evidence_outcome::not_run:      return QStringLiteral("Not run");
			case core::evidence_outcome::interrupted:  return QStringLiteral("Interrupted");
			}
			return { };
		}

		/// What a method's result can speak for: never the whole change.
		auto reach_text(const core::check_method aMethod) -> QString
		{
			switch (core::reach(aMethod)) {
			case core::evidence_reach::material:    return QStringLiteral("Speaks for the material it ran on, in its environment.");
			case core::evidence_reach::environment: return QStringLiteral("Measures the environment it ran in; not the change as a whole.");
			case core::evidence_reach::model:       return QStringLiteral("Concerns the model it analyzed, within its bounds; not the code.");
			case core::evidence_reach::property:    return QStringLiteral("Covers the stated property under its assumptions; not the whole implementation.");
			case core::evidence_reach::reading:     return QStringLiteral("A reading of the code, not a run.");
			}
			return { };
		}

		auto reporter(const core::evidence_origin& aOrigin) -> QString
		{
			if (aOrigin.Import == 0U) {
				return QStringLiteral("you");
			}
			auto name = text_of(aOrigin.Producer);
			if (!aOrigin.Version.empty()) {
				name += QLatin1Char{ ' ' } + text_of(aOrigin.Version);
			}
			return aOrigin.Source == core::evidence_source::agent ? QStringLiteral("the agent %1").arg(name) : name;
		}

		auto currency_word(const core::local_draft& aDraft, const core::evidence_result& aResult, const std::uint64_t aShown) -> QString
		{
			switch (core::currency(aDraft, aResult, aShown)) {
			case core::evidence_currency::current:       return QStringLiteral("This candidate");
			case core::evidence_currency::carried:       return QStringLiteral("Carried from candidate %1").arg(aResult.Candidate);
			case core::evidence_currency::needs_recheck: return QStringLiteral("Recheck");
			case core::evidence_currency::unbound:       return QStringLiteral("Not bound");
			case core::evidence_currency::later:         return QStringLiteral("Candidate %1").arg(aResult.Candidate);
			}
			return { };
		}

		auto disposition_word(const core::evidence_disposition aDisposition) -> QString
		{
			switch (aDisposition) {
			case core::evidence_disposition::proposed: return QStringLiteral("Proposed");
			case core::evidence_disposition::accepted: return QString{ };
			case core::evidence_disposition::rejected: return QStringLiteral("Rejected");
			}
			return { };
		}

		auto claim_status(const core::local_draft& aDraft, const core::evidence_claim& aClaim, const std::uint64_t aShown) -> QString
		{
			if (aClaim.Disposition != core::evidence_disposition::accepted) {
				return disposition_word(aClaim.Disposition);
			}
			const auto counts = core::evidence_for_claim(aDraft, aClaim.Id, aShown);
			auto       parts  = QStringList{ };
			if (counts.Failing > 0U) {
				parts << QStringLiteral("%1 failing").arg(counts.Failing);
			}
			if (counts.Passing > 0U) {
				parts << QStringLiteral("%1 passing").arg(counts.Passing);
			}
			if (counts.Other > 0U) {
				parts << QStringLiteral("%1 unsettled").arg(counts.Other);
			}
			if (counts.NeedsRecheck > 0U) {
				parts << QStringLiteral("%1 to recheck").arg(counts.NeedsRecheck);
			}
			if (counts.Unbound > 0U) {
				parts << QStringLiteral("%1 not bound").arg(counts.Unbound);
			}
			if (counts.Planned > 0U) {
				parts << QStringLiteral("%1 planned").arg(counts.Planned);
			}
			return parts.isEmpty() ? QStringLiteral("No checks") : parts.join(QStringLiteral(", "));
		}

		auto refusal_word(const core::evidence_import_refusal aRefusal) -> QString
		{
			switch (aRefusal) {
			case core::evidence_import_refusal::too_large:     return QStringLiteral("larger than 16 MiB");
			case core::evidence_import_refusal::malformed:     return QStringLiteral("not a readable JSON document");
			case core::evidence_import_refusal::format:        return QStringLiteral("not a BetterPullReview evidence document");
			case core::evidence_import_refusal::version:       return QStringLiteral("a version this build does not read");
			case core::evidence_import_refusal::unknown_field: return QStringLiteral("a field this version does not define");
			case core::evidence_import_refusal::observed:      return QStringLiteral("a result claims to be observed; imported results are reported");
			case core::evidence_import_refusal::invalid_value: return QStringLiteral("a value out of range");
			case core::evidence_import_refusal::reference:     return QStringLiteral("a reference to nothing");
			case core::evidence_import_refusal::bounds:        return QStringLiteral("more records than a draft keeps");
			case core::evidence_import_refusal::duplicate:     return QStringLiteral("already imported");
			case core::evidence_import_refusal::empty:         return QStringLiteral("no records");
			case core::evidence_import_refusal::stale:         return QStringLiteral("the draft changed while it was read; import it again");
			}
			return { };
		}

		auto draft_refusal_word(const core::draft_refusal aRefusal) -> QString
		{
			switch (aRefusal) {
			case core::draft_refusal::stale:          return QStringLiteral("Not saved: the draft changed; it was read again.");
			case core::draft_refusal::evidence_full:  return QStringLiteral("Not saved: the draft holds as many of these as it keeps.");
			case core::draft_refusal::unknown_record: return QStringLiteral("Not saved: that record is gone.");
			case core::draft_refusal::unchanged:      return QString{ };
			default:                                  return QStringLiteral("Not saved: the text is empty or too long.");
			}
		}

		/// A form dialog with OK and Cancel.
		auto form_dialog(QWidget* const aParent, const QString& aTitle) -> std::pair<QDialog*, QFormLayout*>
		{
			auto* const dialog = new QDialog{ aParent };
			dialog->setWindowTitle(aTitle);
			auto* const column  = new QVBoxLayout{ dialog };
			auto* const form    = new QFormLayout{ };
			auto* const buttons = new QDialogButtonBox{ QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog };
			column->addLayout(form);
			column->addWidget(buttons);
			QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
			QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
			dialog->resize(520, dialog->sizeHint().height());
			return { dialog, form };
		}

		auto text_box(QWidget* const aParent, const int aLines) -> QPlainTextEdit*
		{
			auto* const box = new QPlainTextEdit{ aParent };
			box->setTabChangesFocus(true);
			box->setFixedHeight(box->fontMetrics().lineSpacing() * aLines + 12);
			return box;
		}

		auto claim_choices(QComboBox* const aBox, const core::draft_evidence& aEvidence, const bool aNone)
		{
			if (aNone) {
				aBox->addItem(QStringLiteral("None"), QVariant::fromValue<qulonglong>(0U));
			}
			for (const auto& claim : aEvidence.Claims) {
				if (claim.Disposition != core::evidence_disposition::rejected) {
					aBox->addItem(first_line(claim.Text), QVariant::fromValue<qulonglong>(claim.Id));
				}
			}
		}

		auto default_forms(QWidget* const aParent) -> local_draft_evidence_forms
		{
			auto forms  = local_draft_evidence_forms{ };
			forms.Claim = [aParent](const application::local_draft_workbench_projection&) -> std::optional<local_draft_evidence_forms::claim> {
				auto [dialog, form] = form_dialog(aParent, QStringLiteral("Add Claim"));
				auto* const kind    = new QComboBox{ dialog };
				for (const auto value : all_kinds) {
					kind->addItem(kind_word(value), static_cast<int>(value));
				}
				auto* const text = text_box(dialog, 4);
				form->addRow(QStringLiteral("Kind"), kind);
				form->addRow(QStringLiteral("Claim"), text);
				const auto ok     = dialog->exec() == QDialog::Accepted && !text->toPlainText().trimmed().isEmpty();
				auto       answer = ok ? std::optional{ local_draft_evidence_forms::claim{ .Kind = static_cast<core::claim_kind>(kind->currentData().toInt()), .Text = utf8(text->toPlainText().trimmed()) } }
									   : std::nullopt;
				dialog->deleteLater();
				return answer;
			};
			forms.Revision = [aParent](const core::evidence_claim& aClaim) -> std::optional<std::string> {
				auto [dialog, form] = form_dialog(aParent, QStringLiteral("Revise Claim"));
				auto* const text    = text_box(dialog, 4);
				text->setPlainText(text_of(aClaim.Text));
				form->addRow(QStringLiteral("Claim"), text);
				const auto ok     = dialog->exec() == QDialog::Accepted && !text->toPlainText().trimmed().isEmpty();
				auto       answer = ok ? std::optional{ utf8(text->toPlainText().trimmed()) } : std::nullopt;
				dialog->deleteLater();
				return answer;
			};
			forms.Check = [aParent](const application::local_draft_workbench_projection& aView) -> std::optional<local_draft_evidence_forms::check> {
				auto [dialog, form] = form_dialog(aParent, QStringLiteral("Add Check"));
				auto* const method  = new QComboBox{ dialog };
				for (const auto value : all_methods) {
					method->addItem(method_word(value), static_cast<int>(value));
				}
				auto* const title   = new QLineEdit{ dialog };
				auto* const command = new QLineEdit{ dialog };
				command->setToolTip(QStringLiteral("How the check is run, for the record; BetterPullReview does not run it"));
				auto* const claims = new QListWidget{ dialog };
				claims->setFixedHeight(claims->fontMetrics().lineSpacing() * 4 + 8);
				for (const auto& claim : aView.Draft->Evidence.Claims) {
					if (claim.Disposition == core::evidence_disposition::rejected) {
						continue;
					}
					auto* const item = new QListWidgetItem{ first_line(claim.Text), claims };
					item->setData(record_role, QVariant::fromValue<qulonglong>(claim.Id));
					item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
					item->setCheckState(Qt::Unchecked);
				}
				auto* const property    = new QLineEdit{ dialog };
				auto* const subject     = new QLineEdit{ dialog };
				auto* const tool        = new QLineEdit{ dialog };
				auto* const assumptions = new QLineEdit{ dialog };
				auto* const bounds      = new QLineEdit{ dialog };
				subject->setToolTip(QStringLiteral("The model or code the tool analyzed"));
				form->addRow(QStringLiteral("Method"), method);
				form->addRow(QStringLiteral("Title"), title);
				form->addRow(QStringLiteral("Command"), command);
				form->addRow(QStringLiteral("Claims"), claims);
				form->addRow(QStringLiteral("Property"), property);
				form->addRow(QStringLiteral("Analyzed"), subject);
				form->addRow(QStringLiteral("Tool"), tool);
				form->addRow(QStringLiteral("Assumptions"), assumptions);
				form->addRow(QStringLiteral("Bounds"), bounds);
				const auto formal = [&] {
					const auto on = core::formal(static_cast<core::check_method>(method->currentData().toInt()));
					for (auto* const field : { property, subject, tool, assumptions, bounds }) {
						field->setEnabled(on);
					}
				};
				QObject::connect(method, &QComboBox::currentIndexChanged, dialog, formal);
				formal();
				auto answer = std::optional<local_draft_evidence_forms::check>{ };
				if (dialog->exec() == QDialog::Accepted && !title->text().trimmed().isEmpty()) {
					answer = local_draft_evidence_forms::check{ .Method  = static_cast<core::check_method>(method->currentData().toInt()),
																.Title   = utf8(title->text().trimmed()),
																.Command = utf8(command->text().trimmed()),
																.Formal  = { .Property    = utf8(property->text().trimmed()),
																			 .Subject     = utf8(subject->text().trimmed()),
																			 .Tool        = utf8(tool->text().trimmed()),
																			 .Assumptions = utf8(assumptions->text().trimmed()),
																			 .Bounds      = utf8(bounds->text().trimmed()) } };
					for (int row = 0; row < claims->count(); ++row) {
						if (claims->item(row)->checkState() == Qt::Checked) {
							answer->Claims.push_back(claims->item(row)->data(record_role).toULongLong());
						}
					}
				}
				dialog->deleteLater();
				return answer;
			};
			forms.Result = [aParent](const application::local_draft_workbench_projection& aView) -> std::optional<core::manual_result> {
				auto [dialog, form] = form_dialog(aParent, QStringLiteral("Record Result"));
				auto* const check   = new QComboBox{ dialog };
				for (const auto& entry : aView.Draft->Evidence.Checks) {
					if (entry.Disposition != core::evidence_disposition::rejected) {
						check->addItem(text_of(entry.Title), QVariant::fromValue<qulonglong>(entry.Id));
					}
				}
				auto* const outcome = new QComboBox{ dialog };
				for (const auto value : all_outcomes) {
					outcome->addItem(outcome_word(value), static_cast<int>(value));
				}
				auto* const candidate = new QComboBox{ dialog };
				candidate->addItem(QStringLiteral("Candidate %1").arg(aView.Shown), QVariant::fromValue<qulonglong>(aView.Shown));
				candidate->addItem(QStringLiteral("Not one of these candidates"), QVariant::fromValue<qulonglong>(0U));
				candidate->setToolTip(QStringLiteral("The material the check ran on"));
				auto* const inputs = new QLineEdit{ dialog };
				inputs->setToolTip(QStringLiteral("Paths the result depends on, separated by spaces; end a folder with /. A later candidate that leaves them unchanged keeps the result"));
				auto* const environment = new QLineEdit{ dialog };
				auto* const expected    = new QLineEdit{ dialog };
				auto* const actual      = text_box(dialog, 3);
				auto* const limitations = new QLineEdit{ dialog };
				form->addRow(QStringLiteral("Check"), check);
				form->addRow(QStringLiteral("Outcome"), outcome);
				form->addRow(QStringLiteral("Ran on"), candidate);
				form->addRow(QStringLiteral("Inputs"), inputs);
				form->addRow(QStringLiteral("Environment"), environment);
				form->addRow(QStringLiteral("Expected"), expected);
				form->addRow(QStringLiteral("Actual"), actual);
				form->addRow(QStringLiteral("Limitations"), limitations);
				auto answer = std::optional<core::manual_result>{ };
				if (dialog->exec() == QDialog::Accepted && check->count() > 0) {
					answer = core::manual_result{ .Check       = check->currentData().toULongLong(),
												  .Outcome     = static_cast<core::evidence_outcome>(outcome->currentData().toInt()),
												  .Candidate   = candidate->currentData().toULongLong(),
												  .Environment = utf8(environment->text().trimmed()),
												  .Expected    = utf8(expected->text().trimmed()),
												  .Actual      = utf8(actual->toPlainText().trimmed()),
												  .Limitations = utf8(limitations->text().trimmed()) };
					for (const auto& path : inputs->text().split(QLatin1Char{ ' ' }, Qt::SkipEmptyParts)) {
						answer->Inputs.push_back(utf8(path));
					}
				}
				dialog->deleteLater();
				return answer;
			};
			forms.Question = [aParent](const application::local_draft_workbench_projection& aView) -> std::optional<local_draft_evidence_forms::question> {
				auto [dialog, form] = form_dialog(aParent, QStringLiteral("Add Question"));
				auto* const text    = text_box(dialog, 3);
				auto* const claim   = new QComboBox{ dialog };
				claim_choices(claim, aView.Draft->Evidence, true);
				form->addRow(QStringLiteral("Question"), text);
				form->addRow(QStringLiteral("About"), claim);
				const auto ok     = dialog->exec() == QDialog::Accepted && !text->toPlainText().trimmed().isEmpty();
				auto       answer = ok ? std::optional{ local_draft_evidence_forms::question{ .Text = utf8(text->toPlainText().trimmed()), .Claim = claim->currentData().toULongLong() } }
									   : std::nullopt;
				dialog->deleteLater();
				return answer;
			};
			return forms;
		}
	}

	local_draft_evidence_area::local_draft_evidence_area(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("localDraftEvidence"));
		setAccessibleName(QStringLiteral("Evidence"));
		Forms              = default_forms(this);
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(6);

		// One row of commands, always in place: enabled or not, never moved.
		auto* const commands = new QHBoxLayout{ };
		const auto  button   = [this](const QString& aText, const char* aName, const QString& aTip) {
			auto* const made = new QPushButton{ aText, this };
			made->setObjectName(QString::fromLatin1(aName));
			made->setToolTip(aTip);
			return made;
		};
		AddClaim                = button(QStringLiteral("Add Claim…"), "localDraftAddClaim", QStringLiteral("Say what the change does: a fix, preserved behavior, compatibility, performance or a formal property"));
		AddCheck                = button(QStringLiteral("Add Check…"), "localDraftAddCheck", QStringLiteral("Plan a check of one or more claims; BetterPullReview does not run it"));
		RecordResult            = button(QStringLiteral("Record Result…"), "localDraftRecordResult", QStringLiteral("Report a check's result and the candidate it ran on"));
		AddQuestion             = button(QStringLiteral("Add Question…"), "localDraftAddQuestion", QStringLiteral("Keep an open question for the reviewer-facing description"));
		Import                  = button(QStringLiteral("Import…"), "localDraftImportEvidence",
										 QStringLiteral("Import a bpr.evidence-import document from your tools; its claims and checks arrive proposed and its results reported"));
		Accept                  = button(QStringLiteral("Accept"), "localDraftAcceptEvidence", QStringLiteral("Put the selected proposed claim, check or question in the draft"));
		Reject                  = button(QStringLiteral("Reject"), "localDraftRejectEvidence", QStringLiteral("Leave the selected claim, check or question out; it stays in the history"));
		const auto commandWidth = std::ranges::max(std::array{ AddClaim, AddCheck, RecordResult, AddQuestion, Import } | std::views::transform([](QPushButton* aButton) { return aButton->sizeHint().width(); }));
		for (auto* const made : { AddClaim, AddCheck, RecordResult, AddQuestion, Import }) {
			made->setMinimumWidth(commandWidth);
			commands->addWidget(made);
		}
		commands->addStretch(1);
		const auto decisionWidth = std::max(Accept->sizeHint().width(), Reject->sizeHint().width());
		for (auto* const made : { Accept, Reject }) {
			made->setMinimumWidth(decisionWidth);
			commands->addWidget(made);
		}
		column->addLayout(commands);

		auto* const splitter = new QSplitter{ Qt::Horizontal, this };
		splitter->setChildrenCollapsible(false);
		Tree = new QTreeWidget{ splitter };
		Tree->setObjectName(QStringLiteral("localDraftEvidenceTree"));
		Tree->setAccessibleName(QStringLiteral("Claims, checks, results and questions"));
		Tree->setColumnCount(3);
		Tree->setHeaderLabels({ QStringLiteral("Evidence"), QStringLiteral("Status"), QStringLiteral("Use") });
		Tree->headerItem()->setToolTip(use_column, QStringLiteral("Selected for the description and the tour; what is not selected stays private"));
		Tree->header()->setStretchLastSection(false);
		Tree->header()->setSectionResizeMode(text_column, QHeaderView::Stretch);
		Tree->header()->setSectionResizeMode(status_column, QHeaderView::ResizeToContents);
		Tree->header()->setSectionResizeMode(use_column, QHeaderView::ResizeToContents);
		Tree->setContextMenuPolicy(Qt::CustomContextMenu);
		Tree->setUniformRowHeights(true);
		auto* const side       = new QWidget{ splitter };
		auto* const sideColumn = new QVBoxLayout{ side };
		sideColumn->setContentsMargins(0, 0, 0, 0);
		Detail = new QPlainTextEdit{ side };
		Detail->setObjectName(QStringLiteral("localDraftEvidenceDetail"));
		Detail->setAccessibleName(QStringLiteral("Selected evidence"));
		Detail->setReadOnly(true);
		sideColumn->addWidget(Detail, 3);
		Artifacts = new QListWidget{ side };
		Artifacts->setObjectName(QStringLiteral("localDraftEvidenceArtifacts"));
		Artifacts->setAccessibleName(QStringLiteral("Artifacts"));
		Artifacts->setToolTip(QStringLiteral("Double-click to read a kept artifact or open a link"));
		sideColumn->addWidget(Artifacts, 1);
		splitter->addWidget(Tree);
		splitter->addWidget(side);
		splitter->setStretchFactor(0, 3);
		splitter->setStretchFactor(1, 2);
		column->addWidget(splitter, 1);

		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("localDraftEvidenceStatus"));
		Status->setTextInteractionFlags(Qt::TextSelectableByMouse);
		// Always present, so a status never moves what is above it.
		Status->setMinimumHeight(Status->fontMetrics().lineSpacing());
		column->addWidget(Status);

		connect(AddClaim, &QPushButton::clicked, this, [this] {
			if (Projection.Draft && Forms.Claim && Dispatch.AddClaim) {
				if (const auto answer = Forms.Claim(Projection)) {
					Dispatch.AddClaim(answer->Kind, answer->Text);
				}
			}
		});
		connect(AddCheck, &QPushButton::clicked, this, [this] {
			if (Projection.Draft && Forms.Check && Dispatch.AddCheck) {
				if (auto answer = Forms.Check(Projection)) {
					Dispatch.AddCheck(answer->Method, answer->Title, answer->Command, std::move(answer->Claims), std::move(answer->Formal));
				}
			}
		});
		connect(RecordResult, &QPushButton::clicked, this, [this] {
			if (Projection.Draft && Forms.Result && Dispatch.RecordResult) {
				if (auto answer = Forms.Result(Projection)) {
					Dispatch.RecordResult(std::move(*answer));
				}
			}
		});
		connect(AddQuestion, &QPushButton::clicked, this, [this] {
			if (Projection.Draft && Forms.Question && Dispatch.AddQuestion) {
				if (const auto answer = Forms.Question(Projection)) {
					Dispatch.AddQuestion(answer->Text, answer->Claim);
				}
			}
		});
		connect(Import, &QPushButton::clicked, this, [this] {
			if (Dispatch.Import) {
				Dispatch.Import();
			}
		});
		connect(Accept, &QPushButton::clicked, this, [this] {
			if (const auto record = selected_record(); record != 0U && Dispatch.Decide) {
				Dispatch.Decide(record, core::evidence_disposition::accepted);
			}
		});
		connect(Reject, &QPushButton::clicked, this, [this] {
			if (const auto record = selected_record(); record != 0U && Dispatch.Decide) {
				Dispatch.Decide(record, core::evidence_disposition::rejected);
			}
		});
		connect(Tree, &QTreeWidget::currentItemChanged, this, [this] {
			if (!Presenting) {
				present_detail();
				update_buttons();
			}
		});
		connect(Tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* const aItem, const int aColumn) {
			if (!Presenting && aColumn == use_column && Dispatch.Select) {
				Dispatch.Select(aItem->data(text_column, record_role).toULongLong(), aItem->checkState(use_column) == Qt::Checked);
			}
		});
		connect(Tree, &QWidget::customContextMenuRequested, this, [this](const QPoint& aAt) {
			auto* const item = Tree->itemAt(aAt);
			if (item == nullptr || !Projection.Draft) {
				return;
			}
			const auto  id   = item->data(text_column, record_role).toULongLong();
			const auto  kind = static_cast<row_kind>(item->data(text_column, kind_role).toInt());
			auto* const menu = new QMenu{ this };
			menu->setAttribute(Qt::WA_DeleteOnClose);
			const auto& evidence = Projection.Draft->Evidence;
			if (kind == row_kind::claim) {
				connect(menu->addAction(QStringLiteral("Revise Claim…")), &QAction::triggered, this, [this, id] {
					const auto& claims = Projection.Draft->Evidence.Claims;
					const auto  claim  = std::ranges::find(claims, id, &core::evidence_claim::Id);
					if (claim != claims.end() && Forms.Revision && Dispatch.ReviseClaim) {
						if (const auto text = Forms.Revision(*claim)) {
							Dispatch.ReviseClaim(id, *text);
						}
					}
				});
			} else if (kind == row_kind::question) {
				const auto question = std::ranges::find(evidence.Questions, id, &core::evidence_question::Id);
				const auto resolved = question != evidence.Questions.end() && question->Resolved;
				connect(menu->addAction(resolved ? QStringLiteral("Reopen Question") : QStringLiteral("Mark Resolved")), &QAction::triggered, this, [this, id, resolved] {
					if (Dispatch.ResolveQuestion) {
						Dispatch.ResolveQuestion(id, !resolved);
					}
				});
			}
			if (menu->isEmpty()) {
				delete menu;
				return;
			}
			menu->popup(Tree->viewport()->mapToGlobal(aAt));
		});
		connect(Artifacts, &QListWidget::itemActivated, this, [this](QListWidgetItem* const aItem) {
			const auto  result  = aItem->data(record_role).toULongLong();
			const auto  index   = static_cast<std::size_t>(aItem->data(kind_role).toULongLong());
			const auto& results = Projection.Draft->Evidence.Results;
			const auto  found   = std::ranges::find(results, result, &core::evidence_result::Id);
			if (found == results.end() || index >= found->Artifacts.size()) {
				return;
			}
			if (found->Artifacts[index].State == core::artifact_state::link) {
				if (Dispatch.OpenLink) {
					Dispatch.OpenLink(result, index);
				}
			} else if (Dispatch.OpenArtifact) {
				Dispatch.OpenArtifact(result, index);
			}
		});
		update_buttons();
	}

	void local_draft_evidence_area::set_dispatch(local_draft_evidence_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void local_draft_evidence_area::set_forms(local_draft_evidence_forms aForms)
	{
		Forms = std::move(aForms);
	}

	void local_draft_evidence_area::present(const application::local_draft_workbench_projection& aProjection)
	{
		const auto changed = !Projection.Draft || !aProjection.Draft || Projection.Draft->Evidence != aProjection.Draft->Evidence || Projection.Shown != aProjection.Shown
			|| Projection.Draft->Candidates.size() != aProjection.Draft->Candidates.size();
		Projection = aProjection;
		if (changed) {
			rebuild();
		}
		present_detail();
		present_status();
		present_artifact();
		update_buttons();
	}

	void local_draft_evidence_area::rebuild()
	{
		Presenting       = true;
		const auto keep  = selected_record();
		const auto block = QSignalBlocker{ Tree };
		Tree->clear();
		if (!Projection.Draft) {
			Presenting = false;
			return;
		}
		const auto& draft    = *Projection.Draft;
		const auto& evidence = draft.Evidence;
		const auto  shown    = Projection.Shown;
		const auto  row      = [](QTreeWidgetItem* const aItem, const std::uint64_t aId, const row_kind aKind, const QString& aText, const QString& aStatus) {
			aItem->setText(text_column, aText);
			aItem->setText(status_column, aStatus);
			aItem->setData(text_column, record_role, QVariant::fromValue<qulonglong>(aId));
			aItem->setData(text_column, kind_role, static_cast<int>(aKind));
			aItem->setFlags(aItem->flags() & ~Qt::ItemIsUserCheckable);
			return aItem;
		};
		const auto usable = [](QTreeWidgetItem* const aItem, const bool aEligible, const bool aSelected) {
			if (aEligible) {
				aItem->setFlags(aItem->flags() | Qt::ItemIsUserCheckable);
				aItem->setCheckState(use_column, aSelected ? Qt::Checked : Qt::Unchecked);
			} else {
				aItem->setFlags(aItem->flags() & ~Qt::ItemIsUserCheckable);
			}
		};
		const auto dimmed = [this](QTreeWidgetItem* const aItem) {
			for (int column = 0; column < Tree->columnCount(); ++column) {
				aItem->setForeground(column, palette().color(QPalette::Disabled, QPalette::Text));
			}
		};
		const auto add_results = [&](QTreeWidgetItem* const aCheckRow, const core::evidence_check& aCheck) {
			for (const auto& result : evidence.Results) {
				if (result.Check != aCheck.Id) {
					continue;
				}
				auto* const item = row(new QTreeWidgetItem{ aCheckRow }, result.Id, row_kind::result,
									   QStringLiteral("%1, reported by %2").arg(outcome_word(result.Outcome), reporter(result.Origin)), currency_word(draft, result, shown));
				item->setToolTip(text_column, reach_text(aCheck.Method));
				usable(item, aCheck.Disposition == core::evidence_disposition::accepted, result.Selected);
				const auto state = core::currency(draft, result, shown);
				if (state == core::evidence_currency::needs_recheck || state == core::evidence_currency::later) {
					dimmed(item);
				}
			}
		};
		const auto add_check = [&](QTreeWidgetItem* const aParent, const core::evidence_check& aCheck) {
			const auto  planned = std::ranges::none_of(evidence.Results, [&aCheck](const core::evidence_result& aResult) { return aResult.Check == aCheck.Id; });
			const auto  status  = aCheck.Disposition != core::evidence_disposition::accepted ? disposition_word(aCheck.Disposition) : planned ? QStringLiteral("Planned")
																																			  : QString{ };
			auto* const item    = row(aParent == nullptr ? new QTreeWidgetItem{ Tree } : new QTreeWidgetItem{ aParent }, aCheck.Id, row_kind::check,
									  QStringLiteral("%1 (%2)").arg(text_of(aCheck.Title), method_word(aCheck.Method)), status);
			item->setToolTip(text_column, reach_text(aCheck.Method));
			if (aCheck.Disposition == core::evidence_disposition::rejected) {
				dimmed(item);
			}
			add_results(item, aCheck);
			item->setExpanded(aCheck.Disposition != core::evidence_disposition::rejected);
			return item;
		};
		for (const auto& claim : evidence.Claims) {
			auto* const item = row(new QTreeWidgetItem{ Tree }, claim.Id, row_kind::claim, QStringLiteral("%1: %2").arg(kind_word(claim.Kind), first_line(claim.Text)),
								   claim_status(draft, claim, shown));
			item->setToolTip(text_column, text_of(claim.Text));
			usable(item, claim.Disposition == core::evidence_disposition::accepted, claim.Selected);
			if (claim.Disposition == core::evidence_disposition::rejected) {
				dimmed(item);
			}
			for (const auto& check : evidence.Checks) {
				if (std::ranges::find(check.Claims, claim.Id) != check.Claims.end()) {
					add_check(item, check);
				}
			}
			item->setExpanded(claim.Disposition != core::evidence_disposition::rejected);
		}
		auto* other = static_cast<QTreeWidgetItem*>(nullptr);
		for (const auto& check : evidence.Checks) {
			if (check.Claims.empty()) {
				if (other == nullptr) {
					other = row(new QTreeWidgetItem{ Tree }, 0U, row_kind::group, QStringLiteral("Checks of no claim"), QString{ });
					other->setExpanded(true);
				}
				add_check(other, check);
			}
		}
		if (!evidence.Questions.empty()) {
			auto* const group = row(new QTreeWidgetItem{ Tree }, 0U, row_kind::group, QStringLiteral("Questions"), QString{ });
			for (const auto& question : evidence.Questions) {
				const auto  status = question.Disposition != core::evidence_disposition::accepted ? disposition_word(question.Disposition)
					: question.Resolved                                                           ? QStringLiteral("Resolved")
																								  : QStringLiteral("Open");
				auto* const item   = row(new QTreeWidgetItem{ group }, question.Id, row_kind::question, first_line(question.Text), status);
				item->setToolTip(text_column, text_of(question.Text));
				usable(item, question.Disposition == core::evidence_disposition::accepted, question.Selected);
				if (question.Disposition == core::evidence_disposition::rejected || question.Resolved) {
					dimmed(item);
				}
			}
			group->setExpanded(true);
		}
		if (auto* const again = row_of(keep); again != nullptr) {
			Tree->setCurrentItem(again);
		}
		Presenting = false;
	}

	void local_draft_evidence_area::present_detail()
	{
		Artifacts->clear();
		auto*      item = Tree->currentItem();
		const auto id   = selected_record();
		if (item == nullptr || id == 0U || !Projection.Draft) {
			Detail->clear();
			return;
		}
		const auto& draft    = *Projection.Draft;
		const auto& evidence = draft.Evidence;
		auto        lines    = QStringList{ };
		const auto  origin   = [&lines](const core::evidence_origin& aOrigin, const core::evidence_disposition aDisposition) {
			lines << QString{ };
			lines << (aOrigin.Import == 0U ? QStringLiteral("Entered by you.") : QStringLiteral("From import %1, by %2.").arg(aOrigin.Import).arg(reporter(aOrigin)));
			if (aDisposition == core::evidence_disposition::proposed) {
				lines << QStringLiteral("Proposed.");
			} else if (aDisposition == core::evidence_disposition::rejected) {
				lines << QStringLiteral("Rejected.");
			}
		};
		switch (static_cast<row_kind>(item->data(text_column, kind_role).toInt())) {
		case row_kind::group: break;
		case row_kind::claim: {
			const auto claim = std::ranges::find(evidence.Claims, id, &core::evidence_claim::Id);
			lines << kind_word(claim->Kind) << text_of(claim->Text);
			for (const auto& earlier : claim->Earlier | std::views::reverse) {
				lines << QString{ } << QStringLiteral("Earlier: %1").arg(text_of(earlier));
			}
			origin(claim->Origin, claim->Disposition);
			break;
		}
		case row_kind::check: {
			const auto check = std::ranges::find(evidence.Checks, id, &core::evidence_check::Id);
			lines << QStringLiteral("%1: %2").arg(method_word(check->Method), text_of(check->Title));
			if (!check->Command.empty()) {
				lines << QStringLiteral("Command: %1").arg(text_of(check->Command));
			}
			const auto& formal = check->Formal;
			const auto  field  = [&lines](const QString& aName, const std::string& aValue) {
				if (!aValue.empty()) {
					lines << QStringLiteral("%1: %2").arg(aName, text_of(aValue));
				}
			};
			field(QStringLiteral("Property"), formal.Property);
			field(QStringLiteral("Analyzed"), formal.Subject);
			field(QStringLiteral("Tool"), formal.Tool);
			field(QStringLiteral("Assumptions"), formal.Assumptions);
			field(QStringLiteral("Bounds"), formal.Bounds);
			field(QStringLiteral("Correspondence checked"), formal.Correspondence);
			lines << QString{ } << reach_text(check->Method);
			origin(check->Origin, check->Disposition);
			break;
		}
		case row_kind::result: {
			const auto result = std::ranges::find(evidence.Results, id, &core::evidence_result::Id);
			const auto check  = std::ranges::find(evidence.Checks, result->Check, &core::evidence_check::Id);
			lines << QStringLiteral("%1 of %2, reported by %3.").arg(outcome_word(result->Outcome), text_of(check->Title), reporter(result->Origin));
			lines << reach_text(check->Method);
			lines << QString{ };
			if (result->Candidate != 0U) {
				lines << QStringLiteral("Ran on candidate %1.").arg(result->Candidate);
			} else if (result->Material) {
				lines << QStringLiteral("Ran on material that is none of this draft's candidates.");
			} else {
				lines << QStringLiteral("Names no material it ran on.");
			}
			if (result->Material) {
				auto material = QStringLiteral("Commit %1").arg(text_of(core::canonical_hex(result->Material->Head).view()).left(12));
				if (!result->Material->Working.empty()) {
					material += QStringLiteral(" with %1 uncommitted file(s)").arg(result->Material->Working.size());
				}
				lines << material;
			}
			switch (core::currency(draft, *result, Projection.Shown)) {
			case core::evidence_currency::current: break;
			case core::evidence_currency::carried:
				lines << QStringLiteral("Candidate %1 leaves its inputs unchanged.").arg(Projection.Shown);
				break;
			case core::evidence_currency::needs_recheck:
				lines << QStringLiteral("Candidate %1 may change what it tested: recheck it.").arg(Projection.Shown);
				break;
			case core::evidence_currency::unbound:
				lines << QStringLiteral("It does not speak for candidate %1.").arg(Projection.Shown);
				break;
			case core::evidence_currency::later:
				lines << QStringLiteral("It ran on a later candidate than the one shown.");
				break;
			}
			if (!result->Inputs.empty()) {
				auto inputs = QStringList{ };
				for (const auto& input : result->Inputs) {
					inputs << text_of(input);
				}
				lines << QStringLiteral("Inputs: %1").arg(inputs.join(QLatin1Char{ ' ' }));
			}
			const auto field = [&lines](const QString& aName, const std::string& aValue) {
				if (!aValue.empty()) {
					lines << QStringLiteral("%1: %2").arg(aName, text_of(aValue));
				}
			};
			field(QStringLiteral("Environment"), result->Environment);
			field(QStringLiteral("Expected"), result->Expected);
			field(QStringLiteral("Actual"), result->Actual);
			field(QStringLiteral("Limitations"), result->Limitations);
			origin(result->Origin, core::evidence_disposition::accepted);
			for (std::size_t index = 0; index < result->Artifacts.size(); ++index) {
				const auto& artifact = result->Artifacts[index];
				auto        state    = QString{ };
				switch (artifact.State) {
				case core::artifact_state::stored:    state = QStringLiteral("kept, %1 bytes").arg(artifact.Size); break;
				case core::artifact_state::link:      state = QStringLiteral("link"); break;
				case core::artifact_state::missing:   state = QStringLiteral("missing"); break;
				case core::artifact_state::too_large: state = QStringLiteral("too large to keep"); break;
				case core::artifact_state::refused:   state = QStringLiteral("not read"); break;
				}
				auto* const entry = new QListWidgetItem{ QStringLiteral("%1 (%2)").arg(text_of(artifact.Name), state), Artifacts };
				entry->setToolTip(text_of(artifact.Reference));
				entry->setData(record_role, QVariant::fromValue<qulonglong>(result->Id));
				entry->setData(kind_role, QVariant::fromValue<qulonglong>(index));
				if (artifact.State != core::artifact_state::stored && artifact.State != core::artifact_state::link) {
					entry->setFlags(entry->flags() & ~Qt::ItemIsEnabled);
				}
			}
			break;
		}
		case row_kind::question: {
			const auto question = std::ranges::find(evidence.Questions, id, &core::evidence_question::Id);
			lines << text_of(question->Text);
			if (question->Claim != 0U) {
				const auto claim = std::ranges::find(evidence.Claims, question->Claim, &core::evidence_claim::Id);
				lines << QString{ } << QStringLiteral("About: %1").arg(first_line(claim->Text));
			}
			lines << (question->Resolved ? QStringLiteral("Resolved.") : QStringLiteral("Open."));
			origin(question->Origin, question->Disposition);
			break;
		}
		}
		Detail->setPlainText(lines.join(QLatin1Char{ '\n' }));
	}

	void local_draft_evidence_area::present_status()
	{
		auto text = QString{ };
		if (Projection.Importing) {
			text = QStringLiteral("Importing…");
		} else if (Projection.ImportError) {
			const auto& error = *Projection.ImportError;
			switch (error.Failure) {
			case application::evidence_import_failure::refused:
				text = QStringLiteral("Import refused, nothing applied: %1.").arg(refusal_word(error.Refusal->Reason));
				if (!error.Refusal->Detail.empty()) {
					text += QStringLiteral(" (%1)").arg(text_of(error.Refusal->Detail));
				}
				break;
			case application::evidence_import_failure::unreadable: text = QStringLiteral("The document could not be read; nothing was applied."); break;
			case application::evidence_import_failure::store:      text = QStringLiteral("The import could not be saved; nothing was applied."); break;
			case application::evidence_import_failure::cancelled:  text = QStringLiteral("Import cancelled; nothing was applied."); break;
			}
		} else if (Projection.Imported) {
			const auto& report = *Projection.Imported;
			text               = QStringLiteral("Imported %1 claim(s), %2 check(s), %3 reported result(s) (%4 bound to a candidate) and %5 question(s).")
									 .arg(report.Claims)
									 .arg(report.Checks)
									 .arg(report.Results)
									 .arg(report.Bound)
									 .arg(report.Questions);
			if (report.Unstored > 0U) {
				text += QStringLiteral(" %1 artifact(s) not kept.").arg(report.Unstored);
			}
		} else if (Projection.EvidenceRefusal) {
			text = draft_refusal_word(*Projection.EvidenceRefusal);
		}
		Status->setText(text);
	}

	void local_draft_evidence_area::present_artifact()
	{
		if (!Projection.Artifact) {
			if (Reader != nullptr) {
				const auto block = QSignalBlocker{ Reader };
				Reader->hide();
			}
			return;
		}
		if (Reader == nullptr) {
			Reader = new QDialog{ this };
			Reader->setObjectName(QStringLiteral("localDraftArtifactReader"));
			auto* const column = new QVBoxLayout{ Reader };
			ReaderText         = new QPlainTextEdit{ Reader };
			ReaderText->setReadOnly(true);
			ReaderText->setLineWrapMode(QPlainTextEdit::NoWrap);
			column->addWidget(ReaderText);
			Reader->resize(720, 520);
			connect(Reader, &QDialog::finished, this, [this] {
				if (Dispatch.CloseArtifact) {
					Dispatch.CloseArtifact();
				}
			});
		}
		const auto& view = *Projection.Artifact;
		Reader->setWindowTitle(text_of(view.Name));
		auto text = QString{ };
		if (view.Reading) {
			text = QStringLiteral("Reading…");
		} else if (view.Status == application::review_material_status::available) {
			text = view.Binary ? QStringLiteral("A binary file of %1 bytes; it is not shown as text.").arg(view.Size) : text_of(view.Text);
			if (view.Truncated && !view.Binary) {
				text += QStringLiteral("\n\nThe first MiB of %1 bytes is shown.").arg(view.Size);
			}
		} else {
			text = QStringLiteral("This artifact is not in the draft's store.");
		}
		if (ReaderText->toPlainText() != text) {
			ReaderText->setPlainText(text);
		}
		Reader->show();
	}

	void local_draft_evidence_area::update_buttons()
	{
		const auto open = Projection.Draft.has_value();
		for (auto* const made : { AddClaim, AddCheck, AddQuestion }) {
			made->setEnabled(open);
		}
		RecordResult->setEnabled(open && std::ranges::any_of(Projection.Draft ? Projection.Draft->Evidence.Checks : std::vector<core::evidence_check>{ }, [](const core::evidence_check& aCheck) {
									 return aCheck.Disposition != core::evidence_disposition::rejected;
								 }));
		Import->setEnabled(open && !Projection.Importing);
		auto disposition = std::optional<core::evidence_disposition>{ };
		if (const auto id = selected_record(); id != 0U && open) {
			const auto& evidence = Projection.Draft->Evidence;
			if (const auto claim = std::ranges::find(evidence.Claims, id, &core::evidence_claim::Id); claim != evidence.Claims.end()) {
				disposition = claim->Disposition;
			} else if (const auto check = std::ranges::find(evidence.Checks, id, &core::evidence_check::Id); check != evidence.Checks.end()) {
				disposition = check->Disposition;
			} else if (const auto question = std::ranges::find(evidence.Questions, id, &core::evidence_question::Id); question != evidence.Questions.end()) {
				disposition = question->Disposition;
			}
		}
		Accept->setEnabled(disposition && *disposition != core::evidence_disposition::accepted);
		Reject->setEnabled(disposition && *disposition != core::evidence_disposition::rejected);
	}

	auto local_draft_evidence_area::selected_record() const -> std::uint64_t
	{
		const auto* const item = Tree->currentItem();
		return item == nullptr ? 0U : item->data(text_column, record_role).toULongLong();
	}

	auto local_draft_evidence_area::row_of(const std::uint64_t aRecord) const -> QTreeWidgetItem*
	{
		if (aRecord == 0U) {
			return nullptr;
		}
		for (auto it = QTreeWidgetItemIterator{ Tree }; *it != nullptr; ++it) {
			if ((*it)->data(text_column, record_role).toULongLong() == aRecord) {
				return *it;
			}
		}
		return nullptr;
	}

	auto local_draft_evidence_area::tree() const noexcept -> QTreeWidget*
	{
		return Tree;
	}

	auto local_draft_evidence_area::detail() const noexcept -> QPlainTextEdit*
	{
		return Detail;
	}

	auto local_draft_evidence_area::artifacts() const noexcept -> QListWidget*
	{
		return Artifacts;
	}

	auto local_draft_evidence_area::status() const noexcept -> QLabel*
	{
		return Status;
	}

	auto local_draft_evidence_area::add_claim_button() const noexcept -> QPushButton*
	{
		return AddClaim;
	}

	auto local_draft_evidence_area::add_check_button() const noexcept -> QPushButton*
	{
		return AddCheck;
	}

	auto local_draft_evidence_area::record_result_button() const noexcept -> QPushButton*
	{
		return RecordResult;
	}

	auto local_draft_evidence_area::add_question_button() const noexcept -> QPushButton*
	{
		return AddQuestion;
	}

	auto local_draft_evidence_area::import_button() const noexcept -> QPushButton*
	{
		return Import;
	}

	auto local_draft_evidence_area::accept_button() const noexcept -> QPushButton*
	{
		return Accept;
	}

	auto local_draft_evidence_area::reject_button() const noexcept -> QPushButton*
	{
		return Reject;
	}

	auto local_draft_evidence_area::artifact_reader() const noexcept -> QDialog*
	{
		return Reader != nullptr && Reader->isVisible() ? Reader : nullptr;
	}
}
