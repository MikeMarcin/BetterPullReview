#include "bpr/qt/release_update_recovery_dialog.hpp"
#include <QClipboard>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	release_update_recovery_dialog::release_update_recovery_dialog(std::vector<application::release_update_recovery_row> aRows,
																   discard_dispatch aDiscard, QWidget* aParent)
		: QDialog{ aParent }
		, Rows{ std::move(aRows) }
		, Discard{ std::move(aDiscard) }
		, List{ new QListWidget{ this } }
		, Body{ new QPlainTextEdit{ this } }
		, Status{ new QLabel{ this } }
	{
		setObjectName(QStringLiteral("releaseUpdateRecoveryDialog"));
		setWindowTitle(tr("Saved update drafts"));
		resize(700, 550);
		auto* column = new QVBoxLayout{ this };
		auto* notice = new QLabel{ tr("Private text saved before an update. Check GitHub before retrying an uncertain publication."), this };
		notice->setWordWrap(true);
		column->addWidget(notice);
		List->setObjectName(QStringLiteral("updateRecoveryDrafts"));
		List->setAccessibleName(tr("Saved drafts"));
		List->setWordWrap(true);
		List->setTextElideMode(Qt::ElideNone);
		Body->setObjectName(QStringLiteral("updateRecoveryText"));
		Body->setAccessibleName(tr("Saved draft text"));
		Body->setReadOnly(true);
		connect(List, &QListWidget::currentRowChanged, this, [this] { present_selection(); });
		column->addWidget(List);
		column->addWidget(Body, 1);

		auto* retention = new QLabel{ tr("Saved drafts stay until you discard them."), this };
		retention->setWordWrap(true);
		column->addWidget(retention);
		Status->setObjectName(QStringLiteral("updateRecoveryStatus"));
		Status->setWordWrap(true);
		Status->hide();
		column->addWidget(Status);
		auto* buttons   = new QDialogButtonBox{ QDialogButtonBox::Close, this };
		Copy            = buttons->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
		DiscardSelected = buttons->addButton(tr("Discard…"), QDialogButtonBox::ActionRole);
		DiscardAll      = buttons->addButton(tr("Discard all…"), QDialogButtonBox::ActionRole);
		Copy->setObjectName(QStringLiteral("copyUpdateDraft"));
		DiscardSelected->setObjectName(QStringLiteral("discardUpdateDraft"));
		DiscardSelected->setAccessibleName(tr("Discard selected saved draft"));
		DiscardAll->setObjectName(QStringLiteral("discardAllUpdateDrafts"));
		connect(Copy, &QPushButton::clicked, this, [this] {
			if (List->currentRow() >= 0) {
				QGuiApplication::clipboard()->setText(QString::fromStdString(Rows[static_cast<std::size_t>(List->currentRow())].Draft.Body));
			}
		});
		connect(DiscardSelected, &QPushButton::clicked, this, [this] { confirm_discard(false); });
		connect(DiscardAll, &QPushButton::clicked, this, [this] { confirm_discard(true); });
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		buttons->button(QDialogButtonBox::Close)->setDefault(true);
		column->addWidget(buttons);
		for (const auto& row : Rows) {
			const auto& draft = row.Draft;
			List->addItem(QString::fromStdString(draft.Description)
						  + (draft.Indeterminate ? tr(" — publication may have completed") : QString{ }));
		}
		if (!Rows.empty()) {
			List->setCurrentRow(0);
		}
		present_selection();
		List->setFocus();
	}

	auto release_update_recovery_dialog::action_label(std::size_t aCount) -> QString
	{
		return tr("Saved update drafts (%1)").arg(aCount);
	}

	void release_update_recovery_dialog::remove_rows(std::span<const application::release_update_recovery_id> aIds)
	{
		const auto           oldRow = List->currentRow();
		const QSignalBlocker silent{ List };
		for (int row = List->count() - 1; row >= 0; --row) {
			if (std::ranges::find(aIds, Rows[static_cast<std::size_t>(row)].Id) != aIds.end()) {
				delete List->takeItem(row);
				Rows.erase(Rows.begin() + row);
			}
		}
		if (!Rows.empty() && List->currentRow() < 0) {
			List->setCurrentRow(std::clamp(oldRow, 0, List->count() - 1));
		}
		Busy = false;
		Status->setText(Rows.empty() ? tr("No saved update drafts.") : QString{ });
		Status->setVisible(Rows.empty());
		present_selection();
	}

	void release_update_recovery_dialog::present_selection()
	{
		const int selected = List->currentRow();
		Body->setPlainText(selected >= 0 ? QString::fromStdString(Rows[static_cast<std::size_t>(selected)].Draft.Body) : QString{ });
		Copy->setEnabled(selected >= 0);
		DiscardSelected->setEnabled(selected >= 0 && Discard && !Busy);
		DiscardAll->setEnabled(!Rows.empty() && Discard && !Busy);
	}

	void release_update_recovery_dialog::confirm_discard(bool aAll)
	{
		if (Busy || !Discard || Rows.empty() || (!aAll && List->currentRow() < 0)) {
			return;
		}
		std::vector<application::release_update_recovery_id> ids;
		if (aAll) {
			for (const auto& row : Rows) {
				ids.push_back(row.Id);
			}
		} else {
			ids.push_back(Rows[static_cast<std::size_t>(List->currentRow())].Id);
		}
		auto* question = new QMessageBox{ QMessageBox::Warning, windowTitle(),
										  aAll ? tr("Discard all saved text? It can’t be brought back.") : tr("Discard this saved text? It can’t be brought back."),
										  QMessageBox::Cancel, this };
		question->setObjectName(QStringLiteral("confirmDiscardUpdateDrafts"));
		question->setAttribute(Qt::WA_DeleteOnClose);
		auto* confirm = question->addButton(aAll ? tr("Discard all") : tr("Discard"), QMessageBox::DestructiveRole);
		question->setDefaultButton(QMessageBox::Cancel);
		question->setEscapeButton(QMessageBox::Cancel);
		connect(question, &QMessageBox::finished, this, [this, question, confirm, selectedIds = std::move(ids)](int) mutable {
			if (question->clickedButton() == confirm) {
				Busy = true;
				Status->hide();
				DiscardSelected->setEnabled(false);
				DiscardAll->setEnabled(false);
				Discard(std::move(selectedIds));
			}
		});
		question->open();
	}

	void release_update_recovery_dialog::discard_failed()
	{
		Busy = false;
		Status->setText(tr("Couldn’t discard the saved drafts. Try again."));
		Status->show();
		present_selection();
	}
}
