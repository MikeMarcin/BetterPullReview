#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AUTHOR_DESCRIPTION_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AUTHOR_DESCRIPTION_EDITOR_HPP

#include "bpr/qt/pull_request_overview_page.hpp"

#include <QFrame>
#include <QString>

#include <functional>
#include <optional>
#include <string>

class QLabel;
class QPushButton;

/// \file
/// \brief [[BPR-2280]]. The pull request's description in Author's Workflow
/// tab (owner, 2026-11-19: "seems like we can't edit the PR description from
/// Author tab which seems like a gap").
///
/// \details The author reads the description as GitHub has it, edits its
/// Markdown, and saves. Nothing here writes to GitHub: Save leaves as an
/// `overview_description_save` (the body editing began with and the text to
/// save), the same gesture Overview's editor makes ([[BPR-1774]]), and the
/// composition says what came of it. A draft from the agent (Use as
/// description) lands in the editor as one undoable edit and waits for Save.
namespace bpr::qt {
	class markdown_body_view;
	class scoped_prose_editor;

	class author_description_editor final : public QFrame
	{
	public:
		explicit author_description_editor(QWidget* aParent);

		/// \brief The description the accepted read shows; none before one.
		/// While editing, the editor and the body it began from stay as
		/// they are.
		void               present_body(std::optional<std::string> aBody);
		[[nodiscard]] auto body() const noexcept -> const std::optional<std::string>& { return Body; }
		/// \brief Offers Edit when the connection can write the description
		/// for this viewer; withdrawing it closes an editor not saving.
		void               set_editable(bool aEditable);
		[[nodiscard]] auto editable() const noexcept -> bool { return Editable; }
		void               set_save_dispatch(std::function<void(const overview_description_save&)> aDispatch);

		/// \brief Opens the editor on the description read; false when it
		/// cannot be edited now.
		auto begin_edit() -> bool;
		/// \brief Use as description: the editor holds `aMarkdown` in place of
		/// what it held, as one edit Undo takes back. Nothing is sent.
		auto               fill(const QString& aMarkdown) -> bool;
		[[nodiscard]] auto editing() const noexcept -> bool { return Editing; }
		[[nodiscard]] auto saving() const noexcept -> bool { return Saving; }

		/// \brief One sentence under the editor; `aSaving` holds the editor
		/// while GitHub answers.
		void present_status(const QString& aText, bool aSaving = false);
		/// \brief GitHub's description changed since editing began: nothing
		/// was saved, and Reload offers GitHub's text to go on from.
		void offer_newer(std::string aNewer);
		/// \brief Saved, or given up: the editor closes.
		void close_editor();

		[[nodiscard]] auto original() const noexcept -> const std::string& { return Original; }
		[[nodiscard]] auto reader() const noexcept -> markdown_body_view* { return Reader; }
		[[nodiscard]] auto edit_button() const noexcept -> QPushButton* { return Edit; }
		[[nodiscard]] auto editor() const noexcept -> scoped_prose_editor* { return Text; }
		[[nodiscard]] auto save_button() const noexcept -> QPushButton* { return Save; }
		[[nodiscard]] auto cancel_button() const noexcept -> QPushButton* { return Cancel; }
		[[nodiscard]] auto reload_button() const noexcept -> QPushButton* { return Reload; }
		[[nodiscard]] auto status() const noexcept -> QLabel* { return Status; }

	private:
		void present();
		void save();

		std::optional<std::string>                            Body;
		std::string                                           Original;
		std::optional<std::string>                            Newer;
		bool                                                  Editable = false;
		bool                                                  Editing  = false;
		bool                                                  Saving   = false;
		std::function<void(const overview_description_save&)> SaveDispatch;
		QLabel*                                               Heading = nullptr;
		QPushButton*                                          Edit    = nullptr;
		markdown_body_view*                                   Reader  = nullptr;
		QLabel*                                               Empty   = nullptr;
		QWidget*                                              Panel   = nullptr;
		scoped_prose_editor*                                  Text    = nullptr;
		QLabel*                                               Status  = nullptr;
		QPushButton*                                          Reload  = nullptr;
		QPushButton*                                          Cancel  = nullptr;
		QPushButton*                                          Save    = nullptr;
	};
} // namespace bpr::qt

#endif
