#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_CYCLE_PUBLISH_SHEET_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_CYCLE_PUBLISH_SHEET_HPP

#include "bpr/application/review_cycle_projection.hpp"

#include <QDialog>

#include <functional>
#include <optional>
#include <string>
#include <vector>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QScrollArea;
class QWidget;

namespace bpr::qt {
	/// \brief The gestures Publish as Pull Request sends to its host ([[BPR-1177]]).
	struct review_cycle_publish_dispatch
	{
		/// Cancel, Done or Escape: the sheet closes; a recorded plan or journal stays.
		std::function<void()>                                           Back;
		std::function<void(application::review_cycle_publication_form)> Edit;
		/// Review Plan, Change (back to choosing, the plan dropped), Publish Pull
		/// Request, Check GitHub and Cancel Publishing.
		std::function<void()> Prepare;
		std::function<void()> Revise;
		std::function<void()> Publish;
		std::function<void()> Recover;
		std::function<void()> CancelPublication;
		/// The composition chooses the file Save Patch writes.
		std::function<void()>                   SavePatch;
		std::function<void(const std::string&)> Link;
		/// Open Pull Request: the composition hands the linked pull request to
		/// the existing pull request workflow; the sheet names no address.
		std::function<void()> OpenPullRequest;
	};

	/// \brief Publish as Pull Request (RFC-BPR-0059 and RFC-BPR-0061,
	/// [[BPR-1177]]): one sheet over the code, as Share notes is.
	///
	/// \details Window-modal, attached as a sheet on macOS and hung from the
	/// review bar elsewhere, so the cycle stays behind it. It reads top down
	/// as the decision does. The baselines first, each named for what it is:
	/// the commit the new one starts from (P), the target branch as this
	/// checkout last saw it (T) and the cycle's first and latest captures (R0
	/// and Rn), so the cycle's own changes are never mistaken for the pull
	/// request's. Then what goes out: the two starting choices and every
	/// changed path in its own words (starting work, the cycle's improvement,
	/// both, added, deleted, needs a choice, stays out). Then where and as
	/// whom, the title, the description's Problem and Solution and the draft
	/// choice; and a ledger of what goes to GitHub against what stays on this
	/// computer. One default button moves the decision on: Review Plan,
	/// Publish Pull Request (which names exactly the plan read), Check GitHub
	/// after an unanswered step, Open Pull Request once it is linked. Save
	/// Patch and Link Existing Pull Request sit in its foot for publishing
	/// with one's own Git tools. Any edit after a plan is another plan: the
	/// host drops the old one and says so. It decides and performs nothing:
	/// gestures leave through `review_cycle_publish_dispatch`.
	class review_cycle_publish_sheet final : public QDialog
	{
	public:
		explicit review_cycle_publish_sheet(QWidget* aParent);

		void set_dispatch(review_cycle_publish_dispatch aDispatch);
		/// \brief Shows the host's view; the projection supplies what stays private.
		void present(const application::review_cycle_publication_view& aView, const application::review_cycle_workbench_projection& aProjection);
		/// \brief Where the sheet hangs: the global point under the middle of
		/// the review bar, used where the window system does not attach it.
		void set_anchor(QPoint aGlobal);
		/// \brief Gives focus to the default button.
		void focus_first();
		/// \brief The form as the reviewer has it now, including text not yet sent.
		[[nodiscard]] auto current_form() const -> application::review_cycle_publication_form;

		[[nodiscard]] auto title() const noexcept -> QLabel*;
		[[nodiscard]] auto intro() const noexcept -> QLabel*;
		/// \brief The baselines: P, T and the cycle's captures, each in words.
		[[nodiscard]] auto base_words() const noexcept -> QLabel*;
		[[nodiscard]] auto target_words() const noexcept -> QLabel*;
		[[nodiscard]] auto cycle_words() const noexcept -> QLabel*;
		[[nodiscard]] auto base_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto target_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto starting_and_improvements() const noexcept -> QRadioButton*;
		[[nodiscard]] auto improvements_only() const noexcept -> QRadioButton*;
		/// \brief Every changed path, two lines each; a path that needs or allows
		/// a choice is checkable.
		[[nodiscard]] auto paths() const noexcept -> QListWidget*;
		[[nodiscard]] auto owner_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto repository_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto base_branch_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto new_branch_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto account_words() const noexcept -> QLabel*;
		[[nodiscard]] auto title_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto problem_field() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto solution_field() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto draft_box() const noexcept -> QCheckBox*;
		[[nodiscard]] auto author_name_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto author_email_field() const noexcept -> QLineEdit*;
		/// \brief What goes to GitHub, and what stays on this computer.
		[[nodiscard]] auto outgoing_words() const noexcept -> QLabel*;
		[[nodiscard]] auto private_words() const noexcept -> QLabel*;
		[[nodiscard]] auto status() const noexcept -> QLabel*;
		/// \brief The one default button, whatever it says now.
		[[nodiscard]] auto primary_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto exact_plan_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto cancel_publication_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto save_patch_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto link_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto link_field() const noexcept -> QLineEdit*;
		[[nodiscard]] auto close_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto change_button() const noexcept -> QPushButton*;
		/// \brief The short account of a plan or a pull request, shown in place
		/// of the form once there is one.
		[[nodiscard]] auto summary() const noexcept -> QLabel*;
		/// \brief The exact plan, as a confirmation authorizes it.
		[[nodiscard]] auto plan_text() const noexcept -> QPlainTextEdit*;

	protected:
		void done(int aResult) override;
		void showEvent(QShowEvent* aEvent) override;

	private:
		void send_form() const;
		void primary();
		void place();
		void fill_paths(const application::review_cycle_publication_view& aView);

		review_cycle_publish_dispatch              Dispatch;
		application::review_cycle_publication_view Shown;
		QLabel*                                    Title            = nullptr;
		QLabel*                                    Intro            = nullptr;
		QLabel*                                    BaseWords        = nullptr;
		QLabel*                                    TargetWords      = nullptr;
		QLabel*                                    CycleWords       = nullptr;
		QLineEdit*                                 Base             = nullptr;
		QLabel*                                    BaseLabel        = nullptr;
		QLabel*                                    TargetLabel      = nullptr;
		QLineEdit*                                 Target           = nullptr;
		QButtonGroup*                              Modes            = nullptr;
		QRadioButton*                              BothMode         = nullptr;
		QRadioButton*                              ImprovementsMode = nullptr;
		QListWidget*                               Paths            = nullptr;
		QLabel*                                    PathsProblem     = nullptr;
		QLineEdit*                                 Owner            = nullptr;
		QLineEdit*                                 Repository       = nullptr;
		QLineEdit*                                 BaseBranch       = nullptr;
		QLineEdit*                                 NewBranch        = nullptr;
		QLabel*                                    AccountWords     = nullptr;
		QLineEdit*                                 PullTitle        = nullptr;
		QPlainTextEdit*                            Problem          = nullptr;
		QPlainTextEdit*                            Solution         = nullptr;
		QCheckBox*                                 Draft            = nullptr;
		QLineEdit*                                 AuthorName       = nullptr;
		QLineEdit*                                 AuthorEmail      = nullptr;
		QLabel*                                    OutgoingWords    = nullptr;
		QLabel*                                    PrivateWords     = nullptr;
		QLabel*                                    Status           = nullptr;
		QPlainTextEdit*                            PlanText         = nullptr;
		QScrollArea*                               Body             = nullptr;
		QWidget*                                   Fields           = nullptr;
		QPushButton*                               Primary          = nullptr;
		QPushButton*                               ExactPlan        = nullptr;
		QPushButton*                               CancelPublishing = nullptr;
		QPushButton*                               SavePatch        = nullptr;
		QPushButton*                               Link             = nullptr;
		QLineEdit*                                 LinkAddress      = nullptr;
		QPushButton*                               LinkConfirm      = nullptr;
		QPushButton*                               Close            = nullptr;
		QPushButton*                               Change           = nullptr;
		QWidget*                                   Form             = nullptr;
		QLabel*                                    Summary          = nullptr;
		std::optional<QPoint>                      Anchor;
		/// Widgets are being filled from a projection: their signals are not gestures.
		bool Presenting = false;
	};
}

#endif
