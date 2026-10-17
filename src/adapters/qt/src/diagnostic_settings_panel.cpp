#include "bpr/qt/diagnostic_settings_panel.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include <QButtonGroup>
#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
namespace bpr::qt {
	diagnostic_settings_panel::diagnostic_settings_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("diagnosticSettingsPanel"));
		auto* layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		auto*      group = new QGroupBox{ QStringLiteral("Diagnostics"), this };
		auto*      rows  = new QVBoxLayout{ group };
		const auto row   = [group, rows](const QString& aTitle, const QString& aDetail, QWidget* aControl) {
			auto* line  = new QHBoxLayout;
			auto* words = new QVBoxLayout;
			auto* title = new QLabel{ aTitle, group };
			auto  font  = title->font();
			font.setBold(true);
			title->setFont(font);
			words->addWidget(title);
			auto* detail = new QLabel{ aDetail, group };
			detail->setWordWrap(true);
			words->addWidget(detail);
			line->addLayout(words, 1);
			line->addWidget(aControl);
			rows->addLayout(line);
		};
		const auto selector = [this, group](const QString& aName, const QStringList& aChoices, QButtonGroup*& aButtons) {
			auto* control  = new QWidget{ group };
			auto* segments = new QHBoxLayout{ control };
			segments->setContentsMargins(0, 0, 0, 0);
			segments->setSpacing(0);
			aButtons = new QButtonGroup{ this };
			aButtons->setObjectName(aName);
			for (int item = 0; item < aChoices.size(); ++item) {
				auto* button = new QPushButton{ aChoices[item], control };
				button->setCheckable(true);
				button->setAutoDefault(false);
				button->setAccessibleName(aName == QStringLiteral("diagnosticDetail") ? QStringLiteral("Diagnostic detail: %1").arg(aChoices[item]) : QStringLiteral("Identifying details: %1").arg(aChoices[item]));
				aButtons->addButton(button, item);
				segments->addWidget(button);
			}
			return control;
		};
		Enabled = new QCheckBox{ QStringLiteral("Record local diagnostics"), group };
		Enabled->setObjectName(QStringLiteral("diagnosticEnabled"));
		rows->addWidget(Enabled);
		row(QStringLiteral("Diagnostic detail"), QStringLiteral("Choose how much diagnostic detail is saved locally."), selector(QStringLiteral("diagnosticDetail"), { QStringLiteral("Standard"), QStringLiteral("Verbose") }, Detail));
		row(QStringLiteral("Identifying details"), QStringLiteral("Choose whether new records keep identifiers. Existing logs are unchanged."), selector(QStringLiteral("diagnosticPrivacy"), { QStringLiteral("Anonymized"), QStringLiteral("Include locally") }, Privacy));
		layout->addWidget(group);
		// [[BPR-1579]]. A disclosure bar drawn from the theme, not the style.
		auto* more = new disclosure_button{ this };
		more->setText(QStringLiteral("About local diagnostics"));
		layout->addWidget(more);
		auto* context = new QLabel{ QStringLiteral("Credentials and review content are never recorded. Anonymized replaces identifiers with session labels; sanitized explanations can still reveal context. Standard keeps recent sanitized detail for up to 3 minutes (512 events / 2 MiB), included in exports only when selected. Off, privacy changes, Clear and Sign out discard this context. Off keeps older files."), this };
		context->setWordWrap(true);
		context->hide();
		layout->addWidget(context);
		connect(more, &QToolButton::toggled, context, [context](bool aExpanded) { context->setVisible(aExpanded); });
		Open = new QPushButton{ QStringLiteral("Open logs folder"), this };
		Open->setObjectName(QStringLiteral("diagnosticOpen"));
		Clear = new QPushButton{ QStringLiteral("Clear local diagnostics…"), this };
		Clear->setObjectName(QStringLiteral("diagnosticClear"));
		layout->addWidget(Open);
		layout->addWidget(Clear);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("diagnosticStatus"));
		Status->setWordWrap(true);
		layout->addWidget(Status);
		connect(Enabled, &QCheckBox::toggled, this, [this] { apply(); });
		connect(Detail, &QButtonGroup::idClicked, this, [this] { apply(); });
		connect(Privacy, &QButtonGroup::idClicked, this, [this] { apply(); });
		connect(Open, &QPushButton::clicked, this, [this] { if (Dispatch.Open) { Dispatch.Open(); } });
		connect(Clear, &QPushButton::clicked, this, [this] {
			if (Confirming) {
				return;
			}
			Confirming   = true;
			auto* dialog = new QMessageBox{ QMessageBox::Warning, QStringLiteral("Clear this profile's local diagnostics?"), QStringLiteral("This removes owned diagnostic files and recent context for this profile. Exported files remain. Clear is refused while another process has active logs."), QMessageBox::Yes | QMessageBox::Cancel, this };
			dialog->setObjectName(QStringLiteral("diagnosticClearConfirmation"));
			dialog->setAttribute(Qt::WA_DeleteOnClose);
			dialog->setDefaultButton(QMessageBox::Cancel);
			connect(dialog, &QMessageBox::finished, this, [this](int aResult) { Confirming = false; if (aResult == QMessageBox::Yes && Dispatch.Clear) { Dispatch.Clear(); } });
			dialog->open();
		});
		auto* timer = new QTimer{ this };
		timer->setInterval(2000);
		connect(timer, &QTimer::timeout, this, [this] { if (isVisible() && !Projection.Busy && !Confirming && Dispatch.Refresh) { Dispatch.Refresh(); } });
		timer->start();
		adopt({ });
	}
	void diagnostic_settings_panel::set_dispatch(diagnostic_settings_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
		setEnabled(static_cast<bool>(Dispatch.Apply));
	}
	void diagnostic_settings_panel::adopt(const application::diagnostic_settings_projection& aProjection)
	{
		Applying   = true;
		Projection = aProjection;
		Enabled->setChecked(Projection.Capture.Preferences.Enabled);
		Detail->button(static_cast<int>(Projection.Capture.Preferences.Detail))->setChecked(true);
		Privacy->button(static_cast<int>(Projection.Capture.Preferences.Privacy))->setChecked(true);
		Enabled->setEnabled(Projection.Writable && !Projection.Busy);
		for (auto* group : { Detail, Privacy }) {
			for (auto* button : group->buttons()) {
				button->setEnabled(Projection.Writable && !Projection.Busy);
			}
		}
		Clear->setEnabled(!Projection.Busy);
		Open->setEnabled(!Projection.Busy);
		const auto availability = !Projection.Capture.Preferences.Enabled                ? QStringLiteral("Off")
			: Projection.Capture.Status == application::diagnostic_log_status::available ? QStringLiteral("Recording")
																						 : QStringLiteral("Recording unavailable or full");
		auto       text         = QStringLiteral("%1. Dropped: %2. Recent context: %3 events, %4 bytes.")
									  .arg(availability)
									  .arg(Projection.Capture.Dropped)
									  .arg(Projection.Capture.RecentEvents)
									  .arg(Projection.Capture.RecentBytes);
		if (Projection.Busy) {
			text += QStringLiteral(" Applying…");
		}
		if (Projection.Recovered) {
			text += QStringLiteral(" Saved preferences were invalid or unreadable; safe defaults are in use.");
		}
		if (Projection.Result == application::diagnostic_control_result::refused) {
			text += QStringLiteral(" Action refused: storage or folder opening is unavailable.");
		}
		if (Projection.Result == application::diagnostic_control_result::clear_busy) {
			text += QStringLiteral(" Clear refused: another session or retention transaction is active. Files were not deleted; recent context was discarded.");
		}
		if (Projection.Result == application::diagnostic_control_result::clear_unavailable) {
			text += QStringLiteral(" Clear refused: the owned files could not be safely admitted. Recent context was discarded.");
		}
		if (Projection.Result == application::diagnostic_control_result::clear_partial) {
			text += QStringLiteral(" Clear failed during deletion or reopening: some files may be removed. Recording is unavailable; recent context was discarded.");
		}
		if (Projection.Result == application::diagnostic_control_result::cleared) {
			text += QStringLiteral(" Local diagnostics cleared.");
		}
		if (Projection.Result == application::diagnostic_control_result::opened) {
			text += QStringLiteral(" Folder opening requested.");
		}
		Status->setText(text);
		Applying = false;
	}
	void diagnostic_settings_panel::apply()
	{
		if (Applying || Confirming || !Dispatch.Apply) {
			return;
		}
		const application::diagnostic_preferences next{ Enabled->isChecked(), static_cast<application::diagnostic_detail>(Detail->checkedId()), static_cast<application::diagnostic_privacy>(Privacy->checkedId()) };
		const bool                                disclosure = next.Privacy == application::diagnostic_privacy::include_locally && next.Privacy != Projection.Capture.Preferences.Privacy;
		adopt(Projection);
		if (!disclosure) {
			Dispatch.Apply(next);
			return;
		}
		Confirming   = true;
		auto* dialog = new QMessageBox{ QMessageBox::Warning, QStringLiteral("Include identifying details locally?"), QStringLiteral("Future local records keep request and window identifiers. This does not enable credential or review-content logging. Existing anonymized records cannot recover omitted identities. Exports use a separate sharing policy."), QMessageBox::Yes | QMessageBox::Cancel, this };
		dialog->setObjectName(QStringLiteral("diagnosticPrivacyConfirmation"));
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->setDefaultButton(QMessageBox::Cancel);
		connect(dialog, &QMessageBox::finished, this, [this, next](int aResult) { Confirming = false; if (aResult == QMessageBox::Yes && Dispatch.Apply) { Dispatch.Apply(next); } });
		dialog->open();
	}
}
