#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_ENTRY_DIALOG_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_ENTRY_DIALOG_HPP

#include "bpr/core/agent_entry.hpp"

#include <QDialog>

#include <functional>
#include <optional>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

namespace bpr::qt {
	/// \brief [[BPR-PENDING-AQ6]], owner amendment 2026-10-23. Adds or edits
	/// one agent: its name, the protocol it speaks, its program, its fixed
	/// arguments one per line, and its default model and effort.
	///
	/// \details Presentation only: `core::inspect_agent_entry` says whether
	/// the entry may be kept, and OK waits until it may. The arguments are
	/// taken as written, one per line, and are never split or quoted further;
	/// nothing runs them through a shell.
	class agent_entry_dialog final : public QDialog
	{
	public:
		explicit agent_entry_dialog(const core::agent_entry& aEntry, QWidget* aParent = nullptr);

		[[nodiscard]] auto entry() const -> core::agent_entry;
		[[nodiscard]] auto name() const noexcept -> QLineEdit* { return Name; }
		[[nodiscard]] auto protocol() const noexcept -> QComboBox* { return Protocol; }
		[[nodiscard]] auto program() const noexcept -> QLineEdit* { return Program; }
		[[nodiscard]] auto arguments() const noexcept -> QPlainTextEdit* { return Arguments; }
		[[nodiscard]] auto model() const noexcept -> QLineEdit* { return Model; }
		[[nodiscard]] auto effort() const noexcept -> QComboBox* { return Effort; }
		[[nodiscard]] auto problem() const noexcept -> QLabel* { return Problem; }
		[[nodiscard]] auto buttons() const noexcept -> QDialogButtonBox* { return Buttons; }

	private:
		void settle();
		void offer_efforts();

		core::agent_entry Base;
		QLineEdit*        Name      = nullptr;
		QComboBox*        Protocol  = nullptr;
		QLineEdit*        Program   = nullptr;
		QPlainTextEdit*   Arguments = nullptr;
		QLineEdit*        Model     = nullptr;
		QComboBox*        Effort    = nullptr;
		QLabel*           Problem   = nullptr;
		QDialogButtonBox* Buttons   = nullptr;
	};

	/// \brief How a view asks for an entry to be added or edited: the entry
	/// to start from, and the one the reviewer accepted, or none. The default
	/// opens `agent_entry_dialog`; a test answers without a window.
	using agent_entry_editor = std::function<std::optional<core::agent_entry>(const core::agent_entry& aStart, QWidget* aParent)>;
	[[nodiscard]] auto default_agent_entry_editor() -> agent_entry_editor;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_ENTRY_DIALOG_HPP
