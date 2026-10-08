#ifndef BPR_QT_RELEASE_MAINTENANCE_PANEL_HPP
#define BPR_QT_RELEASE_MAINTENANCE_PANEL_HPP

#include "bpr/application/release_maintenance_projection.hpp"

#include <QPointer>
#include <QWidget>
#include <array>

class QAction;
class QDialog;
class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace bpr::qt {
	/// \brief Profile maintenance gestures and exact preview presentation, with no effect capability.
	class release_maintenance_panel final : public QWidget
	{
	public:
		explicit release_maintenance_panel(QWidget* aParent = nullptr);
		void               set_dispatch(application::release_maintenance_dispatch aDispatch);
		void               set_profile_label(const QString& aLabel);
		void               adopt(const application::release_maintenance_projection& aProjection);
		[[nodiscard]] auto action(core::release_maintenance_command aCommand) const -> QAction*;

		/// \brief Opens the local export preview and requests its frozen selection.
		void open_export(core::diagnostic_export_policy aPolicy);
		/// \brief Presents the native chooser only for the still-current requested preview.
		void present_export_directory(core::release_maintenance_intent aIntent, application::diagnostic_export_directory aDirectory);

	protected:
		void hideEvent(QHideEvent* aEvent) override;

	private:
		void                                            activate(core::release_maintenance_command aCommand);
		void                                            confirm(core::release_maintenance_intent aIntent, bool aPreparation);
		void                                            preview_diagnostics(core::diagnostic_export_policy aPolicy);
		void                                            choose_export(core::release_maintenance_intent aIntent);
		void                                            dismiss_dialog();
		void                                            dispatch(core::release_maintenance_intent aIntent);
		application::release_maintenance_dispatch       Dispatch;
		application::release_maintenance_projection     Projection;
		std::array<QAction*, 7>                         Actions{ };
		QCheckBox*                                      IncludeRecent      = nullptr;
		QComboBox*                                      ExportScope        = nullptr;
		QPushButton*                                    IdentifyingPreview = nullptr;
		QLabel*                                         Scope              = nullptr;
		QLabel*                                         Status             = nullptr;
		QLabel*                                         Summary            = nullptr;
		QPlainTextEdit*                                 Preview            = nullptr;
		QPointer<QDialog>                               Dialog;
		std::optional<core::release_maintenance_intent> ChoosingExport;
		QDialog*                                        ExportDialog      = nullptr;
		QLabel*                                         Disclosure        = nullptr;
		QLabel*                                         MaintenanceStatus = nullptr;
		core::diagnostic_export_policy                  ExportPolicy      = core::diagnostic_export_policy::anonymized;
		bool                                            StartExport       = false;
		QString                                         ProfileLabel;
	};
}
#endif
