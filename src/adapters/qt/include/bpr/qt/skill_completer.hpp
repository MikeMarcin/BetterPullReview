#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SKILL_COMPLETER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SKILL_COMPLETER_HPP

#include "bpr/core/skill_references.hpp"

#include <QObject>
#include <QStringList>

#include <optional>
#include <vector>

class QListWidget;
class QPlainTextEdit;

namespace bpr::qt {
	/// \brief [[BPR-1916]]. `/skill` completion in a prompt field.
	///
	/// \details Typing `/` at the start of a word lists the skills the host
	/// presented, each with where it came from (personal, or the repository at
	/// the pull request's base); Up and Down choose, Return or Tab inserts,
	/// Escape closes. A repository skill a personal one of the same name
	/// hides is inserted as `/repo:NAME`. The list never takes focus from the
	/// field, and the field's text is the only thing it changes.
	class skill_completer final : public QObject
	{
	public:
		explicit skill_completer(QPlainTextEdit* aEditor);
		~skill_completer() override;
		skill_completer(const skill_completer&)                    = delete;
		auto operator=(const skill_completer&) -> skill_completer& = delete;

		void present(std::vector<core::skill_entry> aCatalog);
		/// \brief The rows shown, as `NAME (source, origin)`; empty when closed.
		[[nodiscard]] auto rows() const -> QStringList;
		[[nodiscard]] auto popup() const noexcept -> QListWidget* { return Popup; }
		/// \brief Inserts the chosen row, as Return does.
		void accept_current();

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void                                          refresh();
		void                                          close_popup();
		void                                          accept_row(int aRow);
		QPlainTextEdit*                               Editor = nullptr;
		QListWidget*                                  Popup  = nullptr;
		std::vector<core::skill_entry>                Catalog;
		std::vector<core::skill_completion>           Rows;
		std::optional<core::skill_completion_context> Context;
		bool                                          Inserting = false;
	};
}

#endif
