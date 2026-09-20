#include "bpr/qt/release_update_recovery_dialog.hpp"
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace bpr::qt {
	release_update_recovery_dialog::release_update_recovery_dialog(std::vector<application::release_update_saved_draft> aDrafts, QWidget* aParent)
		: QDialog{ aParent }
	{
		setWindowTitle(QStringLiteral("Saved update drafts"));
		resize(700, 550);
		auto* column = new QVBoxLayout{ this };
		auto* notice = new QLabel{ QStringLiteral("Private text saved before an update. Copy what you need. Nothing here is submitted automatically; check GitHub before retrying an uncertain publication."), this };
		notice->setWordWrap(true);
		column->addWidget(notice);
		auto* list = new QListWidget{ this };
		list->setObjectName(QStringLiteral("updateRecoveryDrafts"));
		auto* body = new QPlainTextEdit{ this };
		body->setObjectName(QStringLiteral("updateRecoveryText"));
		body->setReadOnly(true);
		for (const auto& draft : aDrafts) {
			list->addItem(QString::fromStdString(draft.Description) + (draft.Indeterminate ? QStringLiteral(" — publication may have completed") : QString{ }));
		}
		QObject::connect(list, &QListWidget::currentRowChanged, body, [body, drafts = std::move(aDrafts)](int aRow) {
			body->setPlainText(aRow >= 0 && static_cast<std::size_t>(aRow) < drafts.size() ? QString::fromStdString(drafts[static_cast<std::size_t>(aRow)].Body) : QString{ });
		});
		column->addWidget(list);
		column->addWidget(body, 1);
		auto* buttons = new QDialogButtonBox{ QDialogButtonBox::Close, this };
		QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		column->addWidget(buttons);
		if (list->count() != 0) {
			list->setCurrentRow(0);
		}
	}
}
