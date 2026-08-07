#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_WINDOW_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_session_panel.hpp"

#include <QMainWindow>
#include <QString>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class QAction;
class QComboBox;
class QEvent;
class QFrame;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QResizeEvent;
class QSpinBox;
class QSplitter;
class QStackedWidget;
class QToolButton;
class QTreeView;
class QWidget;

namespace bpr::qt {
	/// \brief Describes the github view config record.
	struct github_view_config
	{
		std::vector<application::github_profile_option> Profiles;
	};

	/// \brief Describes the github window statistics record.
	struct github_window_statistics
	{
		std::size_t CommandUpdates     = 0;
		std::size_t StatusUpdates      = 0;
		std::size_t IdentityUpdates    = 0;
		std::size_t CapabilityUpdates  = 0;
		std::size_t SessionUpdates     = 0;
		std::size_t PullRequestUpdates = 0;
		std::size_t DiagnosticUpdates  = 0;
		std::size_t FullResets         = 0;
	};

	/// \brief Provides the github window contract.
	class github_window final
		: public QMainWindow,
		  public application::github_projection_port
	{
	public:
		github_window(
			github_view_config                  aConfig,
			application::github_intent_dispatch aDispatch,
			QWidget*                            aParent = nullptr);

		void publish(
			const application::github_presentation_update& aUpdate) override;

		[[nodiscard]] auto connect_action() const noexcept -> QAction*;
		[[nodiscard]] auto open_action() const noexcept -> QAction*;
		[[nodiscard]] auto refresh_action() const noexcept -> QAction*;
		[[nodiscard]] auto show_review_threads_action() const noexcept -> QAction*;
		[[nodiscard]] auto profile_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto owner_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto repository_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto pull_request_editor() const noexcept -> QSpinBox*;
		[[nodiscard]] auto conversation_view() const noexcept -> QTreeView*;
		[[nodiscard]] auto conversation_model() const noexcept
			-> github_conversation_model*;
		[[nodiscard]] auto epoch_control() const noexcept
			-> epoch_comparison_control*;
		[[nodiscard]] auto diff_comparison_workbench() const noexcept
			-> diff_workbench*;
		[[nodiscard]] auto appearance_settings() const noexcept
			-> appearance_settings_panel*;
		[[nodiscard]] auto status_text() const -> std::string;
		[[nodiscard]] auto diagnostic_text() const -> std::string;
		[[nodiscard]] auto active_host_text() const -> std::string;
		[[nodiscard]] auto active_account_text() const -> std::string;
		[[nodiscard]] auto statistics() const noexcept -> github_window_statistics;

	protected:
		void changeEvent(QEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		void apply_transition(
			const application::github_transition_projection& aTransition);
		void apply_content_command(const core::github_command_projection& aCommand);
		void apply_pull_request(
			const application::github_pull_request_projection& aPullRequest);
		void apply_host_diagnostic(
			const application::github_host_diagnostic_projection& aDiagnostic);
		void clear_pull_request();
		void update_presentation_state();
		void update_splitter_orientation();
		void refresh_native_appearance();
		void announce_presentation_update();
		void focus_review_threads();

		[[nodiscard]] auto presentation_summary_text() const -> QString;

		application::github_intent_dispatch             Dispatch;
		std::vector<application::github_profile_option> Profiles;

		QAction*                   ConnectAction                 = nullptr;
		QAction*                   OpenAction                    = nullptr;
		QAction*                   RefreshAction                 = nullptr;
		QAction*                   ShowReviewThreadsAction       = nullptr;
		QToolButton*               ShowReviewThreadsButton       = nullptr;
		QToolButton*               TechnicalDetailsButton        = nullptr;
		QLabel*                    PullRequestLabel              = nullptr;
		QLabel*                    TitleLabel                    = nullptr;
		QLabel*                    StateLabel                    = nullptr;
		QLabel*                    AuthorLabel                   = nullptr;
		QLabel*                    BranchLabel                   = nullptr;
		QLabel*                    OidLabel                      = nullptr;
		QLabel*                    CountLabel                    = nullptr;
		QPlainTextEdit*            BodyEditor                    = nullptr;
		QLabel*                    ConversationAvailabilityLabel = nullptr;
		QLabel*                    EmptyStateTitleLabel          = nullptr;
		QLabel*                    EmptyStateBodyLabel           = nullptr;
		QWidget*                   TechnicalDetails              = nullptr;
		QStackedWidget*            PullRequestStack              = nullptr;
		QStackedWidget*            ConversationStack             = nullptr;
		QSplitter*                 ContentSplitter               = nullptr;
		QTreeView*                 ConversationView              = nullptr;
		github_conversation_model* ConversationModel             = nullptr;
		epoch_comparison_control*  EpochControl                  = nullptr;
		diff_workbench*            DiffWorkbench                 = nullptr;
		github_session_panel*      SessionPanel                  = nullptr;
		appearance_settings_panel* AppearanceSettings            = nullptr;

		QString                  LastAnnouncement;
		bool                     HasPullRequest                      = false;
		bool                     ExpandConversationsOnNextProjection = true;
		github_window_statistics Statistics;
	};
}

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_WINDOW_HPP
