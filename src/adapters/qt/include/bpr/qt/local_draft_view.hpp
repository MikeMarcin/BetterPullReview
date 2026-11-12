#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_VIEW_HPP

#include "bpr/application/local_draft_workbench.hpp"

#include <QWidget>

#include <cstdint>
#include <functional>
#include <string>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTabBar;
class QWidget;

namespace bpr::qt {
	class local_draft_evidence_area;

	/// \brief The gestures the local draft page sends to its host.
	struct local_draft_dispatch
	{
		std::function<void(const std::string&)>                     OpenFile;
		std::function<void(const std::string&, bool)>               Include;
		std::function<void()>                                       Refresh;
		std::function<void(std::uint64_t)>                          ShowCandidate;
		std::function<void(const std::string&, const std::string&)> AddNote;
		std::function<void(std::uint64_t)>                          RemoveNote;
		std::function<void(const std::string&)>                     Rename;
		std::function<void(core::local_draft_target)>               ChooseTarget;
		std::function<void()>                                       Rebind;
		/// The page has asked; the host removes the private preparation.
		std::function<void()> Discard;
		std::function<void()> Close;
	};

	/// \brief A local PR draft's page (RFC-BPR-0108, [[BPR-2021]]): the
	/// draft's title, its source and target identity, the Changes of the
	/// shown candidate with the exact diff, working edits to include, and
	/// notes; and Evidence (RFC-BPR-0109). Description and Tour come with
	/// later RFCs and are not shown. Each draft opens on Changes with no chat
	/// (X23).
	/// \details It decides nothing and performs no effect: gestures leave
	/// through `local_draft_dispatch`. Secondary identity (exact commits, the
	/// remote chain, qualifications) is in tooltips, not on the page.
	class local_draft_view final : public QWidget
	{
	public:
		explicit local_draft_view(QWidget* aParent = nullptr);

		void set_dispatch(local_draft_dispatch aDispatch);
		void present(const application::local_draft_workbench_projection& aProjection);
		void set_code_font(const QFont& aFont);

		/// \brief The window title row's text for the page.
		[[nodiscard]] auto title_text() const -> QString;
		[[nodiscard]] auto title_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto identity() const noexcept -> QLabel*;
		[[nodiscard]] auto notice() const noexcept -> QLabel*;
		[[nodiscard]] auto changes() const noexcept -> QListWidget*;
		[[nodiscard]] auto working() const noexcept -> QListWidget*;
		[[nodiscard]] auto notes() const noexcept -> QListWidget*;
		[[nodiscard]] auto note_editor() const noexcept -> QLineEdit*;
		[[nodiscard]] auto diff() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto refresh_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto discard_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto close_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto candidates() const noexcept -> QComboBox*;
		[[nodiscard]] auto target_repository() const noexcept -> QComboBox*;
		[[nodiscard]] auto target_branch() const noexcept -> QComboBox*;
		[[nodiscard]] auto use_target_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto rebind_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto projection() const noexcept -> const application::local_draft_workbench_projection&;
		/// \brief Changes and Evidence.
		[[nodiscard]] auto areas() const noexcept -> QTabBar*;
		[[nodiscard]] auto evidence() const noexcept -> local_draft_evidence_area*;
		/// \brief Replaces the discard question, for a course that cannot answer one.
		void set_discard_question(std::function<bool()> aQuestion);

	private:
		void present_header();
		void present_changes();
		void present_working();
		void present_notes();
		void present_diff();
		void present_recovery();
		void ask_discard();

		local_draft_dispatch                          Dispatch;
		application::local_draft_workbench_projection Projection;
		std::function<bool()>                         DiscardQuestion;
		bool                                          Presenting = false;

		QLineEdit*                 Title            = nullptr;
		QLabel*                    Badge            = nullptr;
		QLabel*                    Identity         = nullptr;
		QLabel*                    Notice           = nullptr;
		QWidget*                   TargetRow        = nullptr;
		QComboBox*                 TargetRepository = nullptr;
		QComboBox*                 TargetBranch     = nullptr;
		QPushButton*               UseTarget        = nullptr;
		QPushButton*               Rebind           = nullptr;
		QComboBox*                 Candidates       = nullptr;
		QPushButton*               Refresh          = nullptr;
		QPushButton*               Discard          = nullptr;
		QPushButton*               Close            = nullptr;
		QListWidget*               Changes          = nullptr;
		QLabel*                    WorkingTitle     = nullptr;
		QListWidget*               Working          = nullptr;
		QListWidget*               Notes            = nullptr;
		QLineEdit*                 NoteEditor       = nullptr;
		QPlainTextEdit*            Diff             = nullptr;
		QTabBar*                   Areas            = nullptr;
		QStackedWidget*            AreaStack        = nullptr;
		local_draft_evidence_area* Evidence         = nullptr;
		/// The draft last presented; another draft opens on Changes.
		std::string PresentedDraft;
	};
}

#endif
