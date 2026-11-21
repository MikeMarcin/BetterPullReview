#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_ENTRY_DIALOG_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_ENTRY_DIALOG_HPP

#include "bpr/core/agent_entry.hpp"
#include "bpr/core/agent_program_bundle.hpp"
#include "bpr/qt/argument_list_editor.hpp"

#include <QDialog>
#include <QString>

#include <expected>
#include <functional>
#include <optional>
#include <string>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QResizeEvent;

namespace bpr::qt {
	/// \brief [[BPR-2303]]. What the program field's Choose… asks of the host.
	struct agent_program_host
	{
		/// Shows a chooser over `aParent` starting at `aStart` and answers the
		/// path chosen, or empty. Empty: the platform's own chooser, which on
		/// macOS lets an app bundle be chosen or browsed into.
		std::function<QString(QWidget* aParent, const QString& aTitle, const QString& aStart)> Choose;
		/// Reads the app bundle at a path that `core::names_app_bundle`
		/// accepts, or none when it is not a folder. Empty: a `.app` path is
		/// kept as written, as on Windows and Linux.
		std::function<std::optional<core::app_bundle_listing>(const std::string& aBundle)> ReadBundle;
	};

	/// \brief [[BPR-1753]], owner amendment 2026-10-23. Adds or edits
	/// one agent: its name, the protocol it speaks, its program, its fixed
	/// arguments one per row ([[BPR-2318]]), and its default model and effort.
	///
	/// \details Presentation only: `core::inspect_agent_entry` says whether
	/// the entry may be kept, and OK waits until it may. The arguments are
	/// taken as written, one per row, and are never split or quoted further;
	/// nothing runs them through a shell.
	class agent_entry_dialog final : public QDialog
	{
	public:
		explicit agent_entry_dialog(const core::agent_entry& aEntry, QWidget* aParent = nullptr, agent_program_host aHost = { });

		[[nodiscard]] auto entry() const -> core::agent_entry;
		[[nodiscard]] auto name() const noexcept -> QLineEdit* { return Name; }
		[[nodiscard]] auto protocol() const noexcept -> QComboBox* { return Protocol; }
		[[nodiscard]] auto program() const noexcept -> QLineEdit* { return Program; }
		/// [[BPR-2318]]. The arguments, one per row.
		[[nodiscard]] auto arguments() const noexcept -> argument_list_editor* { return Arguments; }
		[[nodiscard]] auto model() const noexcept -> QLineEdit* { return Model; }
		[[nodiscard]] auto check() const noexcept -> QPlainTextEdit* { return Check; }
		[[nodiscard]] auto effort() const noexcept -> QComboBox* { return Effort; }
		/// [[BPR-1918]]. Strict run checks; none for a popular agent.
		[[nodiscard]] auto strict_checks() const noexcept -> QCheckBox* { return StrictChecks; }
		/// [[BPR-2307]]. Rely on the launcher's sandbox; none for a popular
		/// agent, hidden unless the protocol is a Codex one.
		[[nodiscard]] auto launcher_sandbox() const noexcept -> QCheckBox* { return LauncherSandbox; }
		/// [[BPR-2007]], [[BPR-2008]]. Scratch folder starts; Build and run tests.
		[[nodiscard]] auto scratch_starts() const noexcept -> QComboBox* { return ScratchStarts; }
		[[nodiscard]] auto mcp_servers() const noexcept -> QComboBox* { return McpServers; }
		[[nodiscard]] auto instructions() const noexcept -> QComboBox* { return Instructions; }
		[[nodiscard]] auto builds() const noexcept -> QCheckBox* { return Builds; }
		[[nodiscard]] auto problem() const noexcept -> QLabel* { return Problem; }
		[[nodiscard]] auto buttons() const noexcept -> QDialogButtonBox* { return Buttons; }
		[[nodiscard]] auto choose_button() const noexcept -> QPushButton* { return ChooseButton; }

	protected:
		auto event(QEvent* aEvent) -> bool override;
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		/// Owner, 0.5.0 build 75: Add argument drew over the second argument
		/// row. The wrapped words need more height than the dialog's own
		/// minimum says, and a window opens at most two thirds of the screen
		/// tall, so the rows were squeezed. The dialog is kept at least as
		/// tall as its contents need at its width, within the screen.
		void fit_height();
		void settle();
		void offer_efforts();
		/// [[BPR-2303]]. Reads the program field as an app bundle when it
		/// names one, through the host.
		void read_bundle();
		/// Puts the bundle's program in the field once it is known.
		void show_bundle_program();

		agent_program_host Host;
		/// The program a `.app` in the field runs, or why it runs none; none
		/// while the field names no bundle the host could read.
		std::optional<std::expected<std::string, core::app_bundle_refusal>> Bundle;

		core::agent_entry     Base;
		QLineEdit*            Name                   = nullptr;
		QComboBox*            Protocol               = nullptr;
		QLineEdit*            Program                = nullptr;
		argument_list_editor* Arguments              = nullptr;
		QPlainTextEdit*       Check                  = nullptr;
		QLineEdit*            Model                  = nullptr;
		QComboBox*            Effort                 = nullptr;
		QCheckBox*            StrictChecks           = nullptr;
		QCheckBox*            LauncherSandbox        = nullptr;
		QLabel*               LauncherSandboxMeaning = nullptr;
		QComboBox*            ScratchStarts          = nullptr;
		/// [[BPR-2139]], [[BPR-2140]] (owner, 2026-11-12).
		QComboBox*        McpServers    = nullptr;
		QComboBox*        Instructions  = nullptr;
		QCheckBox*        Builds        = nullptr;
		QLabel*           BuildsMeaning = nullptr;
		QLabel*           Problem       = nullptr;
		QDialogButtonBox* Buttons       = nullptr;
		QPushButton*      ChooseButton  = nullptr;
	};

	/// \brief How a view asks for an entry to be added or edited: the entry
	/// to start from, and the one the reviewer accepted, or none. The default
	/// opens `agent_entry_dialog`; a test answers without a window.
	using agent_entry_editor = std::function<std::optional<core::agent_entry>(const core::agent_entry& aStart, QWidget* aParent)>;
	[[nodiscard]] auto default_agent_entry_editor(agent_program_host aHost = { }) -> agent_entry_editor;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_ENTRY_DIALOG_HPP
