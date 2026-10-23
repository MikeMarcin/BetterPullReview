#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SETUP_SETTINGS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SETUP_SETTINGS_HPP

#include "bpr/application/agent_setup_preferences.hpp"
#include "bpr/application/clone_link_host.hpp"
#include "bpr/application/review_skill_host.hpp"
#include "bpr/core/agent_setup.hpp"
#include "bpr/core/agent_wrapper.hpp"

#include <QWidget>

#include <functional>
#include <optional>
#include <string>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace bpr::qt {
	/// \brief [[BPR-PENDING-AQ6]], owner look v001 ("add a folder was a text
	/// entry first with a file browser option on the text line"). A folder
	/// path to type, **Browse…** on the same line to fill it, and **Add**.
	///
	/// \details A leading `~` is read as the home folder; anything else that
	/// is not an absolute path is refused here with what to write, and the
	/// field keeps the text. Adding is a request; the host decides.
	class folder_entry final : public QWidget
	{
	public:
		explicit folder_entry(QWidget* aParent = nullptr);
		void               set_add(std::function<void(std::string)> aAdd);
		[[nodiscard]] auto field() const noexcept -> QLineEdit* { return Field; }
		[[nodiscard]] auto add_button() const noexcept -> QPushButton* { return Add; }
		[[nodiscard]] auto status() const noexcept -> QLabel* { return Status; }

	private:
		void add();

		std::function<void(std::string)> Adder;
		QLineEdit*                       Field  = nullptr;
		QPushButton*                     Add    = nullptr;
		QLabel*                          Status = nullptr;
	};

	/// \brief [[BPR-PENDING-AQ6]], RFC-BPR-0092 (owner answer Q5). Settings ›
	/// Agents & context: **Set up agent…**, the Codex program and protocol,
	/// and a harness that reads plain text.
	class agent_programs_panel final : public QWidget
	{
	public:
		struct dispatch
		{
			std::function<void()> SetUp;
			/// An empty path clears the saved program.
			std::function<void(core::setup_agent, std::string)> SetProgram;
			std::function<void(core::codex_protocol_choice)>    SetProtocol;
		};

		explicit agent_programs_panel(QWidget* aParent = nullptr);
		void set_dispatch(dispatch aDispatch);
		void present(const application::agent_setup_preferences& aPreferences);
		/// The answer to a SetProgram: nothing when it was saved.
		void show_answer(core::setup_agent aAgent, const std::optional<core::agent_wrapper_refusal>& aRefusal);

		[[nodiscard]] auto codex_path() const noexcept -> QLineEdit* { return CodexPath; }
		[[nodiscard]] auto harness_path() const noexcept -> QLineEdit* { return HarnessPath; }
		[[nodiscard]] auto protocol() const noexcept -> QComboBox* { return Protocol; }

	private:
		dispatch   Dispatch;
		QLineEdit* CodexPath     = nullptr;
		QLabel*    CodexStatus   = nullptr;
		QComboBox* Protocol      = nullptr;
		QLineEdit* HarnessPath   = nullptr;
		QLabel*    HarnessStatus = nullptr;
	};

	/// \brief [[BPR-PENDING-AQ6]], RFC-BPR-0092 (owner decision D2). Settings ›
	/// Repositories: the clone folders and the linked clones.
	class clone_folders_panel final : public QWidget
	{
	public:
		struct dispatch
		{
			std::function<void(std::string)>                         AddFolder;
			std::function<void(std::string)>                         RemoveFolder;
			std::function<void(const application::repository_link&)> Forget;
		};

		explicit clone_folders_panel(QWidget* aParent = nullptr);
		void set_dispatch(dispatch aDispatch);
		void present(const application::clone_link_projection& aProjection);

		[[nodiscard]] auto folders() const noexcept -> QListWidget* { return Folders; }
		[[nodiscard]] auto links() const noexcept -> QListWidget* { return Links; }

	private:
		dispatch                           Dispatch;
		application::clone_link_projection Shown;
		QListWidget*                       Folders = nullptr;
		QListWidget*                       Links   = nullptr;
		folder_entry*                      Entry   = nullptr;
		QPushButton*                       Remove  = nullptr;
		QPushButton*                       Forget  = nullptr;
	};

	/// \brief [[BPR-PENDING-AQ6]], RFC-BPR-0092 (owner decision D1). Settings ›
	/// Agents & context › Review skill: the skill's text, Save, Restore
	/// default, and a newer default's Compare, Use version N and Keep mine.
	class review_skill_panel final : public QWidget
	{
	public:
		struct dispatch
		{
			std::function<void(std::string)> Save;
			/// Restore default and Use version N; true saves a copy to Downloads first.
			std::function<void(bool)> UseBuiltIn;
			std::function<void()>     KeepMine;
		};

		explicit review_skill_panel(QWidget* aParent = nullptr);
		void set_dispatch(dispatch aDispatch);
		void present(const application::review_skill_projection& aProjection, const std::string& aBuiltIn);
		void show_answer(application::review_skill_answer aAnswer);

		[[nodiscard]] auto editor() const noexcept -> QPlainTextEdit* { return Editor; }
		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto offer() const noexcept -> QWidget* { return Offer; }

	private:
		dispatch        Dispatch;
		std::uint64_t   Generation = 0U;
		QPlainTextEdit* Editor     = nullptr;
		QPlainTextEdit* Default    = nullptr;
		QLabel*         Source     = nullptr;
		QLabel*         Status     = nullptr;
		QCheckBox*      KeepCopy   = nullptr;
		QWidget*        Offer      = nullptr;
		QLabel*         OfferText  = nullptr;
		QPushButton*    UseVersion = nullptr;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SETUP_SETTINGS_HPP
