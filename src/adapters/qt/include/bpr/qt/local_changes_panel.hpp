#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_CHANGES_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_CHANGES_PANEL_HPP

#include "bpr/application/local_changes_host.hpp"

#include <QWidget>

#include <array>
#include <functional>
#include <string>
#include <vector>

class QAbstractButton;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QTreeWidget;

namespace bpr::qt {
	/// \brief The gestures Local Changes sends to its host.
	struct local_changes_dispatch
	{
		std::function<void(core::local_comparison)>     Choose;
		std::function<void(const std::string&)>         OpenFile;
		std::function<void(std::vector<std::string>)>   Stage;
		std::function<void(std::vector<std::string>)>   Unstage;
		std::function<void(core::local_commit_request)> Commit;
		std::function<void()>                           Push;
		std::function<void()>                           CheckAgain;
		std::function<void()>                           Stop;
		/// Asks the footer's agent for the commit's subject and description.
		std::function<void()>     Suggest;
		std::function<void(bool)> Consent;
	};

	/// \brief Author state's Local Changes drawer tab (owner, 2026-11-12,
	/// variant a; RFC-BPR-0114 as amended): the three-way pick, Unstaged over
	/// Staged with Stage and Unstage, the pinned commit box with Amend, the
	/// agent's message and Commit N Files, and the push line.
	/// \details Decides nothing and performs no effect. Buttons whose words
	/// change keep their width and place; there is no explainer text, only
	/// tooltips.
	class local_changes_panel final : public QWidget
	{
	public:
		explicit local_changes_panel(QWidget* aParent = nullptr);

		void set_dispatch(local_changes_dispatch aDispatch);
		void present(const application::local_changes_projection& aProjection);
		/// \brief The agent's name, for the message control's tooltip.
		void set_agent_name(const QString& aName);
		/// \brief Replaces the one-time question, for a course.
		void set_consent_question(std::function<bool()> aQuestion);

		[[nodiscard]] auto pick(core::local_comparison aComparison) const -> QAbstractButton*;
		[[nodiscard]] auto unstaged() const noexcept -> QTreeWidget*;
		[[nodiscard]] auto staged() const noexcept -> QTreeWidget*;
		[[nodiscard]] auto listed() const noexcept -> QTreeWidget*;
		[[nodiscard]] auto stage_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto unstage_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto subject() const noexcept -> QLineEdit*;
		[[nodiscard]] auto description() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto amend() const noexcept -> QCheckBox*;
		[[nodiscard]] auto suggest_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto commit_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto push_state() const noexcept -> QLabel*;
		[[nodiscard]] auto push_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto command() const noexcept -> QLabel*;
		[[nodiscard]] auto banner() const noexcept -> QLabel*;
		[[nodiscard]] auto projection() const noexcept -> const application::local_changes_projection&;

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		void               present_lists();
		void               present_commit();
		void               present_push();
		void               fill(QTreeWidget& aTree, const std::vector<core::local_status_entry>& aEntries, const std::string& aCurrent);
		[[nodiscard]] auto selected(const QTreeWidget& aTree) const -> std::vector<std::string>;
		void               restyle();

		local_changes_dispatch                Dispatch;
		application::local_changes_projection Projection;
		std::function<bool()>                 ConsentQuestion;
		std::uint64_t                         SuggestionShown = 0U;
		bool                                  Presenting      = false;
		QString                               AgentName;

		std::array<QAbstractButton*, 3> Picks{ };
		QStackedWidget*                 Pages         = nullptr;
		QLabel*                         UnstagedTitle = nullptr;
		QLabel*                         StagedTitle   = nullptr;
		QTreeWidget*                    Unstaged      = nullptr;
		QTreeWidget*                    Staged        = nullptr;
		QTreeWidget*                    Listed        = nullptr;
		QPushButton*                    Stage         = nullptr;
		QPushButton*                    Unstage       = nullptr;
		QLineEdit*                      Subject       = nullptr;
		QPlainTextEdit*                 Description   = nullptr;
		QCheckBox*                      Amend         = nullptr;
		QToolButton*                    Suggest       = nullptr;
		QPushButton*                    Commit        = nullptr;
		QLabel*                         PushDot       = nullptr;
		QLabel*                         PushState     = nullptr;
		QPushButton*                    Push          = nullptr;
		QLabel*                         Command       = nullptr;
		QLabel*                         Banner        = nullptr;
	};
}

#endif
