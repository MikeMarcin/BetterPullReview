#include "bpr/qt/workbench_window.hpp"

#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "workbench_window_support.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

namespace bpr::qt {
	void workbench_window::update_go_to_line_action()
	{
		if (RefreshGoToLineDialog) {
			RefreshGoToLineDialog();
		}
		if (!GoToLineAction || !ComparisonSurface) {
			return;
		}
		const auto inReview  = !showing_home() && !LocalCycleShown && !CompositionPage && current_modality() == review_modality::review;
		const auto available = inReview && ComparisonSurface->source_line_destination().has_value();
		GoToLineAction->setEnabled(available && QApplication::activeModalWidget() == nullptr);
		// Cocoa offers native menu equivalents before ShortcutOverride. Lift the
		// binding while editing; the explicit menu action remains available.
		const auto* focused = QApplication::focusWidget();
		const auto  editing = focused && (focused->window() != this || takes_typed_text(focused));
		GoToLineAction->setShortcuts(editing ? QList<QKeySequence>{ } : shortcut_keys(shortcut_command::go_to_line));
		GoToLineAction->setStatusTip(available ? tr("Go to a source line in the displayed comparison") : tr("Open a loaded text comparison in Review to go to a line"));
	}

	void workbench_window::show_go_to_line()
	{
		update_go_to_line_action();
		if (!GoToLineAction->isEnabled() || RefreshGoToLineDialog) {
			return;
		}
		const auto initial = ComparisonSurface->source_line_destination();
		if (!initial) {
			return;
		}
		const auto material  = ComparisonSurface->current_result();
		const auto context   = ComparisonSurface->current_context();
		const auto activeTab = [this]() -> std::optional<core::document_tab_id> {
			if (TabsProjection) {
				const auto found = std::ranges::find_if(TabsProjection->Tabs, [](const auto& aTab) { return aTab.Active; });
				if (found != TabsProjection->Tabs.end()) {
					return found->Id;
				}
			}
			return std::nullopt;
		};
		const auto  tab           = activeTab();
		const auto  invokingFocus = QPointer<QWidget>{ QApplication::focusWidget() };
		auto* const dialog        = new QDialog{ this };
		dialog->setObjectName(QStringLiteral("goToLineDialog"));
		dialog->setWindowTitle(tr("Go to line"));
		dialog->setAccessibleName(tr("Go to line"));
		dialog->setWindowModality(Qt::WindowModal);
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->resize(420, 240);
		auto* const column = new QVBoxLayout{ dialog };
		auto* const file   = new QLabel{ dialog };
		file->setObjectName(QStringLiteral("goToLineFile"));
		file->setTextFormat(Qt::PlainText);
		file->setWordWrap(true);
		file->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		if (DocumentTabs) {
			const auto found = std::ranges::find_if(DocumentTabs->tabs(), [](const auto& aTab) { return aTab.Active; });
			if (found != DocumentTabs->tabs().end()) {
				file->setText(found->Path);
			}
		}
		file->setAccessibleName(tr("File: %1").arg(file->text()));
		column->addWidget(file);
		auto* const form = new QFormLayout;
		column->addLayout(form);
		auto* const side = new QComboBox{ dialog };
		side->setObjectName(QStringLiteral("goToLineSide"));
		side->setAccessibleName(tr("Side"));
		side->addItems({ tr("Base"), tr("Head") });
		const auto counts = std::array{ material->LeftDocument ? material->LeftDocument->Lines.size() : 0U,
										material->RightDocument ? material->RightDocument->Lines.size() : 0U };
		for (int index = 0; index < 2; ++index) {
			if (counts[static_cast<std::size_t>(index)] == 0U) {
				static_cast<QStandardItemModel*>(side->model())->item(index)->setEnabled(false);
			}
		}
		side->setCurrentIndex(initial->Side == core::diff_side::left ? 0 : 1);
		form->addRow(tr("&Side"), side);
		auto* const line = new QLineEdit{ QString::number(initial->Line + 1U), dialog };
		line->setObjectName(QStringLiteral("goToLineNumber"));
		line->setAccessibleName(tr("Line"));
		line->setMaxLength(32);
		form->addRow(tr("&Line"), line);
		auto* const range = new QLabel{ dialog };
		range->setObjectName(QStringLiteral("goToLineRange"));
		form->addRow(QString{ }, range);
		auto* const reason = new QLabel{ dialog };
		reason->setObjectName(QStringLiteral("goToLineValidation"));
		reason->setTextFormat(Qt::PlainText);
		reason->setWordWrap(true);
		reason->setMinimumHeight(reason->fontMetrics().lineSpacing() * 2);
		column->addWidget(reason);
		auto* const buttons = new QDialogButtonBox{ QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog };
		auto* const go      = buttons->button(QDialogButtonBox::Ok);
		go->setText(tr("Go"));
		go->setObjectName(QStringLiteral("goToLineGo"));
		go->setAccessibleName(tr("Go"));
		go->setDefault(true);
		column->addWidget(buttons);
		const auto stale   = std::make_shared<bool>(false);
		const auto refresh = [this, material, context, tab, activeTab, counts, side, line, range, reason, go, stale] {
			*stale = *stale || ComparisonSurface->current_result() != material || ComparisonSurface->current_context() != context
				|| activeTab() != tab || showing_home() || LocalCycleShown || CompositionPage || current_modality() != review_modality::review;
			const auto count = counts[static_cast<std::size_t>(side->currentIndex())];
			range->setText(count == 0U ? tr("No source lines on this side") : tr("Range: 1–%1").arg(count));
			const auto text        = line->text();
			const auto decimal     = !text.isEmpty() && std::ranges::all_of(text, [](const QChar aCharacter) { return aCharacter >= QLatin1Char('0') && aCharacter <= QLatin1Char('9'); });
			bool       converted   = false;
			const auto number      = decimal ? text.toUInt(&converted) : 0U;
			const auto valid       = converted && number > 0U && number <= count;
			const auto explanation = *stale ? tr("The tab, file or comparison changed. Close this dialog and open Go to line again.")
				: count == 0U               ? tr("This side has no navigable source lines.")
				: !valid                    ? tr("Enter a whole line number from 1 to %1.").arg(count)
											: QString{ };
			reason->setText(explanation);
			reason->setAccessibleName(explanation);
			line->setAccessibleDescription(range->text() + QLatin1Char(' ') + explanation);
			go->setEnabled(!*stale && valid);
		};
		RefreshGoToLineDialog = refresh;
		connect(line, &QLineEdit::textChanged, dialog, refresh);
		connect(side, &QComboBox::currentIndexChanged, dialog, refresh);
		connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
		connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, refresh, go, line, side, initial] {
			refresh();
			if (!go->isEnabled()) {
				return;
			}
			auto target = *initial;
			target.Side = side->currentIndex() == 0 ? core::diff_side::left : core::diff_side::right;
			target.Line = line->text().toUInt() - 1U;
			// Close before focusing the source: a modal child still owns focus.
			dialog->accept();
			static_cast<void>(ComparisonSurface->go_to_source_line(target));
		});
		connect(dialog, &QDialog::finished, this, [this, invokingFocus](const int aResult) {
			RefreshGoToLineDialog = { };
			if (aResult != QDialog::Accepted && invokingFocus) {
				invokingFocus->setFocus(Qt::OtherFocusReason);
			}
			update_go_to_line_action();
		});
		refresh();
		dialog->open();
		line->setFocus(Qt::ShortcutFocusReason);
		line->selectAll();
	}
}
