#include "external_link_confirmation.hpp"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QTextOption>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

namespace bpr::qt::detail {
	auto show_external_link_confirmation(QWidget& aOwner, const QString& aAddress, std::function<void()> aOpen) -> QDialog*
	{
		auto* const dialog = new QDialog{ &aOwner };
		dialog->setObjectName(QStringLiteral("externalLinkConfirmation"));
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->setWindowModality(Qt::WindowModal);
		dialog->setWindowTitle(QObject::tr("Open Link"));
		dialog->setAccessibleName(QObject::tr("Open link in your browser?"));
		dialog->setAccessibleDescription(aAddress);

		auto* const column   = new QVBoxLayout{ dialog };
		auto* const question = new QLabel{ QObject::tr("Open this link in your browser?"), dialog };
		question->setObjectName(QStringLiteral("externalLinkQuestion"));
		question->setTextFormat(Qt::PlainText);
		auto heading = question->font();
		heading.setBold(true);
		question->setFont(heading);
		column->addWidget(question);

		// The whole address, never elided: wrapping anywhere keeps a long one
		// readable without widening the dialog past its window.
		auto* const address = new QPlainTextEdit{ dialog };
		address->setObjectName(QStringLiteral("externalLinkAddress"));
		address->setAccessibleName(QObject::tr("Link address"));
		address->setPlainText(aAddress);
		address->setReadOnly(true);
		address->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		address->setWordWrapMode(QTextOption::WrapAnywhere);
		address->setTabChangesFocus(true);
		const auto lines = QFontMetrics{ address->font() }.lineSpacing();
		address->setMinimumWidth(QFontMetrics{ address->font() }.averageCharWidth() * 56);
		address->setFixedHeight(lines * 4 + 2 * static_cast<int>(address->document()->documentMargin()) + 2 * address->frameWidth());
		column->addWidget(address);

		auto* const buttons = new QDialogButtonBox{ dialog };
		auto* const open    = buttons->addButton(QObject::tr("Open in Browser"), QDialogButtonBox::AcceptRole);
		auto* const cancel  = buttons->addButton(QDialogButtonBox::Cancel);
		open->setObjectName(QStringLiteral("externalLinkOpen"));
		cancel->setObjectName(QStringLiteral("externalLinkCancel"));
		open->setDefault(true);
		column->addWidget(buttons);
		QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
		QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
		// Either answer gives the keyboard back to the link that asked, in its
		// window, so Tab and Return go on from where the reviewer was.
		const auto asker = QPointer<QWidget>{ QApplication::focusWidget() };
		QObject::connect(dialog, &QDialog::finished, dialog, [owner = QPointer<QWidget>{ &aOwner }, asker, confirmed = std::move(aOpen)](const int aResult) {
			if (owner != nullptr) {
				owner->activateWindow();
			}
			if (asker != nullptr) {
				asker->setFocus(Qt::OtherFocusReason);
			}
			if (aResult == QDialog::Accepted && confirmed) {
				confirmed();
			}
		});
		dialog->open();
		open->setFocus(Qt::OtherFocusReason);
		return dialog;
	}
} // namespace bpr::qt::detail
