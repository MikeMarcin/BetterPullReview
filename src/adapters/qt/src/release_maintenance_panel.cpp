#include "bpr/qt/release_maintenance_panel.hpp"
#include "bpr/qt/disclosure_button.hpp"

#include <QAccessible>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	namespace {
		using command = core::release_maintenance_command;
		using reason  = core::release_maintenance_reason;
		using phase   = core::release_maintenance_phase;

		auto disclosure_body(QWidget* aParent, QVBoxLayout* aLayout, const QString& aTitle, const QString& aName) -> QWidget*
		{
			// [[BPR-1579]]. A disclosure bar drawn from the theme, not the style.
			auto* toggle = new disclosure_button{ aParent };
			toggle->setObjectName(aName);
			toggle->setText(aTitle);
			aLayout->addWidget(toggle);
			auto* body = new QWidget{ aParent };
			body->hide();
			aLayout->addWidget(body);
			QObject::connect(toggle, &QToolButton::toggled, body, [body](bool aExpanded) { body->setVisible(aExpanded); });
			return body;
		}

		auto index(command aCommand) noexcept -> std::size_t { return static_cast<std::size_t>(aCommand); }

		auto reason_text(reason aReason) -> QString
		{
			switch (aReason) {
			case reason::none:                 return { };
			case reason::unavailable:          return QStringLiteral("This operation is unavailable in this runtime.");
			case reason::busy:                 return QStringLiteral("Wait for the current operation to finish.");
			case reason::no_selection:         return QStringLiteral("Prepare a new preview or inventory first.");
			case reason::selection_changed:
			case reason::invalid_authority:
			case reason::stale_completion:     return QStringLiteral("The profile or prepared selection changed. Prepare it again.");
			case reason::privacy_refusal:      return QStringLiteral("The diagnostic source did not satisfy the privacy checks. Nothing was exported.");
			case reason::limit_exceeded:       return QStringLiteral("The operation exceeds the bounded diagnostic or inventory limit.");
			case reason::incomplete_inventory: return QStringLiteral("Unknown or unresolved local data prevents a complete inventory. Nothing can be deleted until it is reconciled.");
			case reason::cancelled:            return QStringLiteral("Cancelled. No new operation was started.");
			case reason::point_of_no_return:   return QStringLiteral("The operation has committed and can no longer be cancelled.");
			case reason::io_failure:           return QStringLiteral("The local operation failed. An export may already exist at the chosen location; check it before trying again.");
			case reason::recovery_required:    return QStringLiteral("Cleanup may be partial. Recovery is required before this profile can continue; do not remove files by hand.");
			case reason::shutdown:             return QStringLiteral("This profile runtime has stopped.");
			case reason::identity_exhausted:   return QStringLiteral("Restart the application before another maintenance operation.");
			case reason::invalid_selection:
			case reason::invalid_completion:
			case reason::invalid_command:      return QStringLiteral("The operation was refused. Prepare a new selection before trying again.");
			case reason::storage_not_private:
				return QStringLiteral("A file or folder in this profile is not private to your account, so nothing was prepared or deleted.");
			}
			return QStringLiteral("The operation was refused.");
		}

		auto diagnostic_text(const application::release_maintenance_projection& aProjection, reason aReason) -> QString
		{
			if (aProjection.Operation == command::export_diagnostics && aReason == reason::selection_changed) {
				return QStringLiteral("The diagnostic export was refused because the preview or chosen location changed or is unavailable. Preview again and choose a new local file; existing files cannot be overwritten.");
			}
			return reason_text(aReason);
		}

		auto status_text(const application::release_maintenance_projection& aProjection) -> QString
		{
			const auto& state = aProjection.State;
			if (state.RecoveryRequired) {
				return reason_text(reason::recovery_required);
			}
			if (aProjection.Diagnostic != reason::none && (aProjection.Diagnostic != state.Reason || state.Phase == phase::idle)) {
				return diagnostic_text(aProjection, aProjection.Diagnostic);
			}
			switch (state.Phase) {
			case phase::preparing: return QStringLiteral("Preparing the exact preview or inventory…");
			case phase::ready:
				return aProjection.Preview ? QStringLiteral("Ready to save. Originals stay unchanged; nothing is uploaded.")
										   : QStringLiteral("Inventory ready. Nothing has been deleted. Confirm the prepared scope or cancel.");
			case phase::running:
				return state.PointOfNoReturn ? QStringLiteral("Committing the operation. Cancellation is no longer available.") : QStringLiteral("Performing the selected local operation…");
			case phase::cancelling: return QStringLiteral("Cancellation requested. Waiting for the actual result…");
			case phase::completed:
				if (aProjection.Operation == command::purge_profile) {
					return QStringLiteral("The prepared profile data was purged. This runtime has ended; shared data and exported files remain.");
				}
				if (aProjection.Operation == command::export_diagnostics) {
					return QStringLiteral("The diagnostic preview was saved locally. Nothing was uploaded.");
				}
				return QStringLiteral("The prepared diagnostic files were cleared. Reopen the profile to resume work.");
			case phase::stopped: return reason_text(reason::shutdown);
			case phase::cancelled:
				return QStringLiteral("Cancelled. If preparation stopped this profile, reopen it explicitly to resume work.");
			case phase::failed: return diagnostic_text(aProjection, state.Reason) + QStringLiteral(" If preparation stopped this profile, it has not restarted.");
			case phase::idle:   return state.Capabilities.Diagnostics
				? QStringLiteral("Diagnostics are local and available without signing in. Start with a preview.")
				: QStringLiteral("Diagnostics are unavailable in this runtime.");
			}
			return { };
		}

		/// [[BPR-1717]]. Says what the export limits left out; empty when nothing was.
		auto limited(const core::release_diagnostic_snapshot& aSnapshot) -> QString
		{
			if (aSnapshot.LimitOmittedRecords == 0U && aSnapshot.UnreadFiles == 0U) {
				return { };
			}
			if (aSnapshot.UnreadFiles == 0U) {
				return QStringLiteral("\nKept the newest records. %1 older records were left out to fit the export size limit.").arg(aSnapshot.LimitOmittedRecords);
			}
			return QStringLiteral("\nKept the newest records. %1 older records and %2 older log files were left out to fit the export size limit.")
				.arg(aSnapshot.LimitOmittedRecords)
				.arg(aSnapshot.UnreadFiles);
		}
		auto selection_text(const application::release_maintenance_projection& aProjection) -> QString
		{
			if (!aProjection.State.Prepared) {
				return { };
			}
			const auto& selection = aProjection.State.Prepared->Selection;
			if (const auto* snapshot = std::get_if<core::release_diagnostic_snapshot>(&selection)) {
				if (snapshot->Options.Policy != core::diagnostic_export_policy::metadata) {
					QStringList categories;
					if ((snapshot->Categories & 1U) != 0U) {
						categories << QStringLiteral("application");
					}
					if ((snapshot->Categories & 2U) != 0U) {
						categories << QStringLiteral("GitHub");
					}
					if ((snapshot->Categories & 4U) != 0U) {
						categories << QStringLiteral("Git/epochs");
					}
					const auto range = snapshot->Sessions == 0U ? QStringLiteral("No dated records") : QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(snapshot->FirstUtcMs), QTimeZone::UTC).toString(Qt::ISODateWithMs) + QStringLiteral(" to ") + QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(snapshot->LastUtcMs), QTimeZone::UTC).toString(Qt::ISODateWithMs);
					return QStringLiteral("%1 · %2 · %3 sessions · %4 records · %5 bytes\n%8\nCategories: %9\n%6 records omitted · %7 recent-context records · %10 records with capture-time identity omissions.")
							   .arg(snapshot->Options.Policy == core::diagnostic_export_policy::identifying ? QStringLiteral("Identifying details") : QStringLiteral("Anonymized"))
							   .arg(snapshot->Options.Scope == core::diagnostic_export_scope::current_session ? QStringLiteral("current session") : QStringLiteral("retained sessions"))
							   .arg(snapshot->Sessions)
							   .arg(snapshot->Records)
							   .arg(snapshot->Bytes)
							   .arg(snapshot->OmittedRecords)
							   .arg(snapshot->RecentRecords)
							   .arg(range, categories.isEmpty() ? QStringLiteral("none") : categories.join(QStringLiteral(", ")))
							   .arg(snapshot->CaptureRedactedRecords)
						+ limited(*snapshot);
				}
				return QStringLiteral("%1 metadata records · %2 bytes · %3 records omitted. Free text, paths, identities, source, comments, credentials and crash memory are excluded.")
						   .arg(snapshot->Records)
						   .arg(snapshot->Bytes)
						   .arg(snapshot->OmittedRecords)
					+ limited(*snapshot);
			}
			if (const auto* inventory = std::get_if<core::release_maintenance_inventory>(&selection)) {
				return QStringLiteral("Prepared inventory: %1 exact local entries, including any credential bindings and database sidecars. Confirmation applies only to this inventory.").arg(inventory->Entries);
			}
			return { };
		}
	}

	release_maintenance_panel::release_maintenance_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("releaseMaintenancePanel"));
		setAccessibleName(QStringLiteral("Profile diagnostics and removal"));
		auto* column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		Scope = new QLabel{ this };
		Scope->setWordWrap(true);
		column->addWidget(Scope);
		auto* exports = new QHBoxLayout;
		column->addLayout(exports);
		ExportDialog = new QDialog{ this };
		ExportDialog->setObjectName(QStringLiteral("diagnosticExportDialog"));
		ExportDialog->resize(640, 300);
		auto* dialogLayout = new QVBoxLayout{ ExportDialog };
		Disclosure         = new QLabel{ ExportDialog };
		Disclosure->setObjectName(QStringLiteral("diagnosticExportDisclosure"));
		Status = new QLabel{ ExportDialog };
		Status->setObjectName(QStringLiteral("releaseMaintenanceStatus"));
		Summary = new QLabel{ ExportDialog };
		Summary->setObjectName(QStringLiteral("releaseMaintenanceSummary"));
		for (auto* label : { Disclosure, Summary, Status }) {
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			dialogLayout->addWidget(label);
		}
		auto* optionsBody = disclosure_body(ExportDialog, dialogLayout, QStringLiteral("Export options"), QStringLiteral("diagnosticExportOptions"));
		auto* fields      = new QVBoxLayout{ optionsBody };
		fields->setContentsMargins(0, 0, 0, 0);
		ExportScope = new QComboBox{ optionsBody };
		ExportScope->setObjectName(QStringLiteral("diagnosticExportScope"));
		ExportScope->setAccessibleName(QStringLiteral("Diagnostic export session scope"));
		ExportScope->addItem(QStringLiteral("All retained sessions in this profile"));
		ExportScope->addItem(QStringLiteral("Current session only"));
		fields->addWidget(ExportScope);
		IncludeRecent = new QCheckBox{ QStringLiteral("Include recent context (up to 3 minutes)"), optionsBody };
		IncludeRecent->setToolTip(QStringLiteral("Already sanitized detail, limited to 512 events and 2 MiB."));
		IncludeRecent->setObjectName(QStringLiteral("diagnosticExportRecent"));
		fields->addWidget(IncludeRecent);
		auto* refresh = new QPushButton{ QStringLiteral("Update preview"), optionsBody };
		refresh->setObjectName(QStringLiteral("diagnosticExportRefresh"));
		connect(refresh, &QPushButton::clicked, this, [this] { preview_diagnostics(ExportPolicy); });
		fields->addWidget(refresh);
		const auto changed = [this] {
			if (Projection.State.Prepared || Projection.State.Pending) {
				dispatch({ Projection.State.Authority, command::cancel, Projection.State.Pending ? Projection.State.Pending : Projection.State.Prepared });
			}
		};
		connect(ExportScope, &QComboBox::currentIndexChanged, this, changed);
		connect(IncludeRecent, &QCheckBox::toggled, this, changed);
		auto* details       = disclosure_body(ExportDialog, dialogLayout, QStringLiteral("Show records"), QStringLiteral("diagnosticExportRecords"));
		auto* detailsLayout = new QVBoxLayout{ details };
		detailsLayout->setContentsMargins(0, 0, 0, 0);
		Preview = new QPlainTextEdit{ details };
		Preview->setObjectName(QStringLiteral("releaseMaintenancePreview"));
		Preview->setAccessibleName(QStringLiteral("Exact diagnostic export preview"));
		Preview->setReadOnly(true);
		Preview->setMinimumHeight(120);
		Preview->setMaximumHeight(220);
		detailsLayout->addWidget(Preview);
		auto* dialogButtons = new QDialogButtonBox{ QDialogButtonBox::Cancel, ExportDialog };
		dialogButtons->button(QDialogButtonBox::Cancel)->setDefault(true);
		connect(dialogButtons, &QDialogButtonBox::rejected, ExportDialog, &QDialog::reject);
		dialogLayout->addWidget(dialogButtons);
		connect(ExportDialog, &QDialog::finished, this, [this] {
			StartExport = false;
			dismiss_dialog();
			if (action(command::cancel)->isEnabled()) {
				dispatch({ Projection.State.Authority, command::cancel, Projection.State.Pending ? Projection.State.Pending : Projection.State.Prepared });
			}
		});
		auto* maintenanceBody = disclosure_body(this, column, QStringLiteral("Profile maintenance"), QStringLiteral("profileMaintenanceOptions"));
		auto* buttons         = new QGridLayout{ maintenanceBody };
		MaintenanceStatus     = new QLabel{ maintenanceBody };
		MaintenanceStatus->setWordWrap(true);
		buttons->addWidget(MaintenanceStatus, 0, 0, 1, 2);
		const std::array labels{ "Export anonymized diagnostics…", "Save…", "Prepare diagnostic cleanup…", "Clear prepared diagnostics…", "Prepare profile purge…", "Purge prepared profile data…", "Cancel maintenance" };
		for (std::size_t item = 0; item < labels.size(); ++item) {
			auto* action = new QAction{ QString::fromUtf8(labels[item]), this };
			action->setEnabled(false);
			action->setObjectName(QStringLiteral("releaseMaintenanceAction%1").arg(item));
			Actions[item] = action;
			auto* button  = new QPushButton{ action->text(), this };
			button->setEnabled(false);
			button->setAutoDefault(false);
			button->setObjectName(QStringLiteral("releaseMaintenanceCommand%1").arg(item));
			if (item == 0U) {
				exports->addWidget(button);
			} else if (item == 1U) {
				dialogButtons->addButton(button, QDialogButtonBox::ActionRole);
			} else {
				buttons->addWidget(button, static_cast<int>((item - 2U) / 2U) + 1, static_cast<int>(item % 2U));
			}
			connect(action, &QAction::changed, button, [action, button] {
				button->setEnabled(action->isEnabled());
				button->setText(action->text());
				button->setAccessibleName(action->text());
				button->setToolTip(action->toolTip());
			});
			connect(button, &QPushButton::clicked, action, &QAction::trigger);
			connect(action, &QAction::triggered, this, [this, item] { activate(static_cast<command>(item)); });
		}
		IdentifyingPreview = new QPushButton{ QStringLiteral("Export with identifying details…"), this };
		IdentifyingPreview->setObjectName(QStringLiteral("diagnosticIdentifyingPreview"));
		IdentifyingPreview->setEnabled(false);
		connect(IdentifyingPreview, &QPushButton::clicked, this, [this] { open_export(core::diagnostic_export_policy::identifying); });
		exports->addWidget(IdentifyingPreview);
		connect(action(command::preview_diagnostics), &QAction::changed, refresh, [this, refresh] { refresh->setEnabled(action(command::preview_diagnostics)->isEnabled()); });
	}

	void release_maintenance_panel::set_dispatch(application::release_maintenance_dispatch aDispatch) { Dispatch = std::move(aDispatch); }
	void release_maintenance_panel::set_profile_label(const QString& aLabel)
	{
		ProfileLabel = aLabel;
		Scope->setText(QStringLiteral("Export diagnostics for %1").arg(ProfileLabel));
	}
	auto release_maintenance_panel::action(command aCommand) const -> QAction* { return Actions.at(index(aCommand)); }

	void release_maintenance_panel::adopt(const application::release_maintenance_projection& aProjection)
	{
		if (Projection.State.Authority != aProjection.State.Authority || Projection.State.Prepared != aProjection.State.Prepared || Projection.State.Pending != aProjection.State.Pending) {
			dismiss_dialog();
		}
		const auto previousPreview = Projection.Preview;
		Projection                 = aProjection;
		set_profile_label(ProfileLabel);
		for (std::size_t item = 0U; item < Actions.size(); ++item) {
			const auto projected = std::ranges::find(aProjection.Commands, static_cast<command>(item), &core::release_maintenance_command_projection::Command);
			const auto refusal   = projected == aProjection.Commands.end() ? reason::unavailable : projected->DisabledReason;
			Actions[item]->setEnabled(refusal == reason::none);
			Actions[item]->setToolTip(reason_text(refusal));
		}
		IdentifyingPreview->setEnabled(action(command::preview_diagnostics)->isEnabled());
		ExportScope->setEnabled(!Projection.State.Pending);
		IncludeRecent->setEnabled(!Projection.State.Pending);
		const auto* snapshot = Projection.State.Prepared ? std::get_if<core::release_diagnostic_snapshot>(&Projection.State.Prepared->Selection) : nullptr;
		action(command::export_diagnostics)->setText(snapshot && snapshot->Options.Policy == core::diagnostic_export_policy::identifying ? QStringLiteral("Save with identifying details…") : QStringLiteral("Save…"));
		if (previousPreview != aProjection.Preview) {
			Preview->setPlainText(aProjection.Preview ? QString::fromStdString(*aProjection.Preview) : QString{ });
		}

		Summary->setText(selection_text(aProjection));
		const auto text = status_text(aProjection);
		MaintenanceStatus->setText(selection_text(aProjection) + QStringLiteral("\n") + text);
		if (Status->text() != text) {
			Status->setText(text);
			QAccessibleAnnouncementEvent announcement{ Status, text };
			QAccessible::updateAccessibility(&announcement);
		}
		if (StartExport && action(command::preview_diagnostics)->isEnabled()) {
			StartExport = false;
			preview_diagnostics(ExportPolicy);
		}
	}

	void release_maintenance_panel::dispatch(core::release_maintenance_intent aIntent)
	{
		if (Dispatch.Intent) {
			Dispatch.Intent(std::move(aIntent));
		}
	}

	void release_maintenance_panel::activate(command aCommand)
	{
		if (Dialog || !action(aCommand)->isEnabled()) {
			return;
		}
		auto intent = core::release_maintenance_intent{ Projection.State.Authority, aCommand, { } };
		switch (aCommand) {
		case command::prepare_clear:
		case command::prepare_purge:
			confirm(intent, true);
			return;
		case command::clear_diagnostics:
		case command::purge_profile:
			intent.Target = Projection.State.Prepared;
			confirm(intent, false);
			return;
		case command::export_diagnostics:
			if (!ExportDialog->isVisible()) {
				return;
			}
			intent.Target = Projection.State.Prepared;
			if (intent.Target && std::get<core::release_diagnostic_snapshot>(intent.Target->Selection).Options.Policy == core::diagnostic_export_policy::identifying) {
				intent.IdentifyingConfirmed = true;
				choose_export(intent);
			} else {
				choose_export(intent);
			}
			return;
		case command::cancel:
			intent.Target = Projection.State.Pending ? Projection.State.Pending : Projection.State.Prepared;
			break;
		case command::preview_diagnostics: open_export(core::diagnostic_export_policy::anonymized); return;
		}
		dispatch(intent);
	}

	void release_maintenance_panel::preview_diagnostics(core::diagnostic_export_policy aPolicy)
	{
		if (Dialog || !action(command::preview_diagnostics)->isEnabled()) {
			return;
		}
		core::release_maintenance_intent intent{ Projection.State.Authority, command::preview_diagnostics, { } };
		intent.ExportOptions = { aPolicy, ExportScope->currentIndex() == 1 ? core::diagnostic_export_scope::current_session : core::diagnostic_export_scope::retained_sessions, IncludeRecent->isChecked() };
		dispatch(intent);
	}
	void release_maintenance_panel::open_export(core::diagnostic_export_policy aPolicy)
	{
		if (Dialog || Projection.State.Pending) {
			return;
		}
		ExportPolicy           = aPolicy;
		const bool identifying = aPolicy == core::diagnostic_export_policy::identifying;
		ExportDialog->setWindowTitle(identifying ? QStringLiteral("Export with identifying details") : QStringLiteral("Export anonymized diagnostics"));
		Disclosure->setText(identifying
								? QStringLiteral("This file includes identifiers already saved in local logs. Identifiers removed during recording cannot be recovered. Save only if you intend to include them.")
								: QStringLiteral("Identifiers are replaced with fresh labels. Diagnostic explanations may still reveal context."));
		StartExport = true;
		ExportDialog->show();
		ExportDialog->raise();
		ExportDialog->activateWindow();
		if (action(command::preview_diagnostics)->isEnabled()) {
			StartExport = false;
			preview_diagnostics(ExportPolicy);
		}
	}

	void release_maintenance_panel::confirm(core::release_maintenance_intent aIntent, bool aPreparation)
	{
		const bool purge      = aIntent.Command == command::prepare_purge || aIntent.Command == command::purge_profile;
		auto       title      = purge ? QStringLiteral("Purge this prepared profile?") : QStringLiteral("Clear prepared diagnostics?");
		auto       buttonText = purge ? QStringLiteral("Purge prepared data") : QStringLiteral("Clear prepared diagnostics");
		if (aPreparation) {
			title      = QStringLiteral("Prepare local maintenance?");
			buttonText = QStringLiteral("Stop profile and prepare");
		}
		auto text = QStringLiteral("Profile: %1 (profile %2).\n\n").arg(ProfileLabel).arg(aIntent.Authority.Profile.value());
		text += aPreparation
			? QStringLiteral("Preparation stops this profile's review work and closes its data stores. Finish or save any unsaved work first. Nothing is deleted during preparation. Cancelling later does not restart the profile; reopen it explicitly.\n\n")
			: selection_text(Projection) + QStringLiteral("\n\n");
		text += purge
			? QStringLiteral("Purge removes this profile's diagnostic files, private notes, drafts, receipts, settings, retained history, managed agent/MCP data and exact remembered credential bindings.\n\nThe profile root and lock, shared registry and updater state, sibling profiles, exported files and any empty workspace parent remain. GitHub reviews and developer worktrees are unchanged.")
			: QStringLiteral("Clear removes only the prepared local diagnostic files. Reviews, preferences, credentials and retained history remain.");
		auto* box = new QMessageBox{ QMessageBox::Warning, title, text, QMessageBox::Cancel, this };
		box->setObjectName(QStringLiteral("releaseMaintenanceConfirmation"));
		box->setTextFormat(Qt::PlainText);
		box->setAttribute(Qt::WA_DeleteOnClose);
		auto* proceed = box->addButton(buttonText, QMessageBox::DestructiveRole);
		box->setDefaultButton(QMessageBox::Cancel);
		box->setEscapeButton(QMessageBox::Cancel);
		Dialog = box;
		connect(box, &QDialog::finished, this, [this, box, proceed, aIntent, aPreparation] {
			Dialog.clear();
			if (box->clickedButton() == proceed) {
				dispatch(aIntent);
			} else if (!aPreparation) {
				dispatch({ aIntent.Authority, command::cancel, aIntent.Target });
			}
		});
		box->open();
	}

	void release_maintenance_panel::choose_export(core::release_maintenance_intent aIntent)
	{
		if (ChoosingExport) {
			return;
		}
		ChoosingExport = aIntent;
		if (Dispatch.ChooseExportDirectory) {
			Dispatch.ChooseExportDirectory(aIntent);
		} else {
			present_export_directory(aIntent, std::unexpected(reason::unavailable));
		}
	}

	void release_maintenance_panel::present_export_directory(core::release_maintenance_intent aIntent, application::diagnostic_export_directory aDirectory)
	{
		if (!ChoosingExport || ChoosingExport->Authority != aIntent.Authority || ChoosingExport->Target != aIntent.Target) {
			return;
		}
		ChoosingExport.reset();
		if (Projection.State.Authority != aIntent.Authority || Projection.State.Prepared != aIntent.Target || !ExportDialog->isVisible() || Dialog || !action(command::export_diagnostics)->isEnabled()) {
			return;
		}
		if (!aDirectory || aDirectory->empty()) {
			const auto text = QStringLiteral("Downloads, Documents and your home folder are unavailable for export. Make a user folder available, then try Save again. Your preview is unchanged.");
			Status->setText(text);
			QAccessibleAnnouncementEvent announcement{ Status, text };
			QAccessible::updateAccessibility(&announcement);
			return;
		}
		Status->setText(status_text(Projection));
		auto* chooser = new QFileDialog{ this, QStringLiteral("Save diagnostics") };
		chooser->setObjectName(QStringLiteral("releaseMaintenanceSaveDialog"));
		chooser->setAttribute(Qt::WA_DeleteOnClose);
		chooser->setAcceptMode(QFileDialog::AcceptSave);
		chooser->setFileMode(QFileDialog::AnyFile);
		chooser->setSupportedSchemes({ QStringLiteral("file") });
		chooser->setOption(QFileDialog::DontConfirmOverwrite);
		chooser->setNameFilter(QStringLiteral("Diagnostic export (*.txt)"));
		chooser->setDefaultSuffix(QStringLiteral("txt"));
		chooser->setDirectory(QString::fromStdString(*aDirectory));
		// [[BPR-1722]]. Named for when it was taken and by which
		// version, so saved exports need no renaming and sort by time.
		const auto now = QDateTime::currentDateTime();
		chooser->selectFile(QString::fromStdString(core::diagnostic_export_file_name(
			{ .Year = now.date().year(), .Month = static_cast<unsigned>(now.date().month()), .Day = static_cast<unsigned>(now.date().day()), .Hour = static_cast<unsigned>(now.time().hour()), .Minute = static_cast<unsigned>(now.time().minute()) },
			QCoreApplication::applicationVersion().toStdString())));
		Dialog = chooser;
		connect(chooser, &QDialog::finished, this, [this, chooser, aIntent](int aResult) {
			Dialog.clear();
			if (aResult == QDialog::Accepted && Dispatch.Export && chooser->selectedFiles().size() == 1) {
				Dispatch.Export(aIntent, chooser->selectedFiles().front().toStdString());
			}
		});
		chooser->open();
	}

	void release_maintenance_panel::dismiss_dialog()
	{
		ChoosingExport.reset();
		if (Dialog) {
			disconnect(Dialog, nullptr, this, nullptr);
			Dialog->reject();
			Dialog.clear();
		}
	}

	void release_maintenance_panel::hideEvent(QHideEvent* aEvent)
	{
		ExportDialog->reject();
		dismiss_dialog();
		if (action(command::cancel)->isEnabled()) {
			dispatch({ Projection.State.Authority, command::cancel, Projection.State.Pending ? Projection.State.Pending : Projection.State.Prepared });
		}
		QWidget::hideEvent(aEvent);
	}
}
