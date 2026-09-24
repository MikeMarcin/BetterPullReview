#include "bpr/qt/release_maintenance_panel.hpp"

#include <QAccessible>
#include <QAction>
#include <QFileDialog>
#include <QGridLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	namespace {
		using command = core::release_maintenance_command;
		using reason  = core::release_maintenance_reason;
		using phase   = core::release_maintenance_phase;

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
				return aProjection.Preview ? QStringLiteral("Preview ready. Export saves exactly the text shown below; nothing is uploaded.")
										   : QStringLiteral("Inventory ready. Nothing has been deleted. Confirm the prepared scope or cancel.");
			case phase::running:
				return state.PointOfNoReturn ? QStringLiteral("Committing the operation. Cancellation is no longer available.") : QStringLiteral("Performing the selected local operation…");
			case phase::cancelling: return QStringLiteral("Cancellation requested. Waiting for the actual result…");
			case phase::completed:
				if (aProjection.Operation == command::purge_profile) {
					return QStringLiteral("The prepared profile data was purged. This runtime has ended; shared data and exported files remain.");
				}
				if (aProjection.Operation == command::export_diagnostics) {
					return QStringLiteral("The previewed diagnostic metadata was saved locally. Nothing was uploaded.");
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

		auto selection_text(const application::release_maintenance_projection& aProjection) -> QString
		{
			if (!aProjection.State.Prepared) {
				return { };
			}
			const auto& selection = aProjection.State.Prepared->Selection;
			if (const auto* snapshot = std::get_if<core::release_diagnostic_snapshot>(&selection)) {
				return QStringLiteral("%1 metadata records · %2 bytes · %3 records omitted. Free text, paths, identities, source, comments, credentials and crash memory are excluded.")
					.arg(snapshot->Records)
					.arg(snapshot->Bytes)
					.arg(snapshot->OmittedRecords);
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
		Scope  = new QLabel{ this };
		Status = new QLabel{ QStringLiteral("Maintenance is unavailable until this profile is admitted."), this };
		Status->setObjectName(QStringLiteral("releaseMaintenanceStatus"));
		Summary = new QLabel{ this };
		Summary->setObjectName(QStringLiteral("releaseMaintenanceSummary"));
		for (auto* label : { Scope, Status, Summary }) {
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			column->addWidget(label);
		}
		auto*            buttons = new QGridLayout;
		const std::array labels{ "Preview diagnostics", "Export preview…", "Prepare diagnostic cleanup…", "Clear prepared diagnostics…", "Prepare profile purge…", "Purge prepared profile data…", "Cancel maintenance" };
		for (std::size_t item = 0; item < labels.size(); ++item) {
			auto* action = new QAction{ QString::fromUtf8(labels[item]), this };
			action->setEnabled(false);
			action->setObjectName(QStringLiteral("releaseMaintenanceAction%1").arg(item));
			Actions[item] = action;
			auto* button  = new QPushButton{ action->text(), this };
			button->setEnabled(false);
			button->setAutoDefault(false);
			button->setObjectName(QStringLiteral("releaseMaintenanceCommand%1").arg(item));
			button->setAccessibleName(action->text());
			buttons->addWidget(button, static_cast<int>(item / 2U), static_cast<int>(item % 2U));
			connect(action, &QAction::changed, button, [action, button] {
				button->setEnabled(action->isEnabled());
				button->setToolTip(action->toolTip());
				button->setAccessibleDescription(action->toolTip());
			});
			connect(button, &QPushButton::clicked, action, &QAction::trigger);
			connect(action, &QAction::triggered, this, [this, item] { activate(static_cast<command>(item)); });
		}
		column->addLayout(buttons);
		Preview = new QPlainTextEdit{ this };
		Preview->setObjectName(QStringLiteral("releaseMaintenancePreview"));
		Preview->setAccessibleName(QStringLiteral("Exact diagnostic export preview"));
		Preview->setReadOnly(true);
		Preview->setMinimumHeight(120);
		Preview->setMaximumHeight(220);
		Preview->hide();
		column->addWidget(Preview);
	}

	void release_maintenance_panel::set_dispatch(application::release_maintenance_dispatch aDispatch) { Dispatch = std::move(aDispatch); }
	void release_maintenance_panel::set_profile_label(const QString& aLabel)
	{
		ProfileLabel = aLabel;
		Scope->setText(QStringLiteral("Diagnostics and removal for %1 (profile %2).").arg(ProfileLabel).arg(Projection.State.Authority.Profile.value()));
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
		if (previousPreview != aProjection.Preview) {
			Preview->setPlainText(aProjection.Preview ? QString::fromStdString(*aProjection.Preview) : QString{ });
		}
		Preview->setVisible(aProjection.Preview != nullptr);
		Summary->setText(selection_text(aProjection));
		const auto text = status_text(aProjection);
		if (Status->text() != text) {
			Status->setText(text);
			QAccessibleAnnouncementEvent announcement{ Status, text };
			QAccessible::updateAccessibility(&announcement);
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
			intent.Target = Projection.State.Prepared;
			choose_export(intent);
			return;
		case command::cancel:
			intent.Target = Projection.State.Pending ? Projection.State.Pending : Projection.State.Prepared;
			break;
		case command::preview_diagnostics: break;
		}
		dispatch(intent);
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
		auto* chooser = new QFileDialog{ this, QStringLiteral("Save diagnostic metadata to a new local file") };
		chooser->setObjectName(QStringLiteral("releaseMaintenanceSaveDialog"));
		chooser->setAttribute(Qt::WA_DeleteOnClose);
		chooser->setAcceptMode(QFileDialog::AcceptSave);
		chooser->setFileMode(QFileDialog::AnyFile);
		chooser->setSupportedSchemes({ QStringLiteral("file") });
		chooser->setOption(QFileDialog::DontConfirmOverwrite);
		chooser->setNameFilter(QStringLiteral("Diagnostic metadata (*.txt)"));
		chooser->setDefaultSuffix(QStringLiteral("txt"));
		chooser->selectFile(QStringLiteral("BetterPullReview-diagnostics.txt"));
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
		if (Dialog) {
			disconnect(Dialog, nullptr, this, nullptr);
			Dialog->reject();
			Dialog.clear();
		}
	}

	void release_maintenance_panel::hideEvent(QHideEvent* aEvent)
	{
		dismiss_dialog();
		if (action(command::cancel)->isEnabled()) {
			dispatch({ Projection.State.Authority, command::cancel, Projection.State.Pending ? Projection.State.Pending : Projection.State.Prepared });
		}
		QWidget::hideEvent(aEvent);
	}
}
