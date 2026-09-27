#include "bpr/qt/diagnostic_settings_panel.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
namespace bpr::qt {
	diagnostic_settings_panel::diagnostic_settings_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("diagnosticSettingsPanel"));
		auto* layout = new QVBoxLayout{ this };
		layout->addWidget(new QLabel{ QStringLiteral("Local diagnostics"), this });
		auto* explanation = new QLabel{ QStringLiteral("These controls affect future records in this profile. Anonymized replaces request and window identifiers with session labels. Include locally keeps those identifiers; credentials and review content are never recorded. Existing files keep their captured mode. Sanitized explanations can still reveal context."), this };
		explanation->setWordWrap(true);
		layout->addWidget(explanation);
		Enabled = new QCheckBox{ QStringLiteral("Record local diagnostics"), this };
		Enabled->setObjectName(QStringLiteral("diagnosticEnabled"));
		layout->addWidget(Enabled);
		auto* form = new QFormLayout;
		Detail     = new QComboBox{ this };
		Detail->setObjectName(QStringLiteral("diagnosticDetail"));
		Detail->addItems({ QStringLiteral("Standard"), QStringLiteral("Verbose") });
		Privacy = new QComboBox{ this };
		Privacy->setObjectName(QStringLiteral("diagnosticPrivacy"));
		Privacy->addItems({ QStringLiteral("Anonymized"), QStringLiteral("Include locally") });
		form->addRow(QStringLiteral("Diagnostic detail"), Detail);
		form->addRow(QStringLiteral("Identifying details"), Privacy);
		layout->addLayout(form);
		auto* context = new QLabel{ QStringLiteral("Standard keeps sanitized intermediate detail in recent context for up to 3 minutes (512 events / 2 MiB). Recent context is separate from the metadata-only export. Off, privacy changes, Clear and Sign out discard this context. Off keeps older files."), this };
		context->setWordWrap(true);
		layout->addWidget(context);
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
		connect(Detail, &QComboBox::currentIndexChanged, this, [this] { apply(); });
		connect(Privacy, &QComboBox::currentIndexChanged, this, [this] { apply(); });
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
		Detail->setCurrentIndex(static_cast<int>(Projection.Capture.Preferences.Detail));
		Privacy->setCurrentIndex(static_cast<int>(Projection.Capture.Preferences.Privacy));
		Enabled->setEnabled(Projection.Writable && !Projection.Busy);
		Detail->setEnabled(Projection.Writable && !Projection.Busy);
		Privacy->setEnabled(Projection.Writable && !Projection.Busy);
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
		const application::diagnostic_preferences next{ Enabled->isChecked(), static_cast<application::diagnostic_detail>(Detail->currentIndex()), static_cast<application::diagnostic_privacy>(Privacy->currentIndex()) };
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
