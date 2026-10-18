#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_SUBMISSION_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_SUBMISSION_EDITOR_HPP

#include "bpr/core/review_preflight.hpp"
#include "bpr/core/review_submission.hpp"
#include "bpr/core/review_submission_unknown.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QColor>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>

class QAction;
class QLabel;
class QMenu;
class QPlainTextEdit;
class QTextEdit;
class QToolButton;
class QVBoxLayout;

namespace bpr::qt {
	class danger_tool_button;
	class destructive_question;
	enum class review_submission_editor_operation { command,
													reconcile };
	struct review_submission_editor_intent
	{
		review_submission_editor_operation Operation = review_submission_editor_operation::command;
		core::review_submission_command    Command;
		/// [[BPR-1085]]. On Submit, the decision the editor showed when it was
		/// pressed. The workflow refuses it if anything it bound has changed.
		std::optional<core::review_prepared_decision> Prepared;
	};
	using review_submission_editor_dispatch = std::function<void(review_submission_editor_intent)>;
	/// \brief [[BPR-1085]]. Where one preflight item is and what it says.
	struct review_submission_preflight_reference
	{
		QString Where;
		QString Subject;
		auto    operator==(const review_submission_preflight_reference&) const -> bool = default;
	};
	/// \brief Safe, decided presentation; no provider, request or execution authority.
	struct review_submission_editor_projection
	{
		std::optional<core::review_submission_draft> Draft;
		core::review_submission_controls             Controls;
		QString                                      TargetDescription;
		QString                                      Status;
		/// [[BPR-1085]]. My own feedback for this draft's account and revision.
		/// Absent when it has not been read: unknown, never "none".
		std::optional<core::review_preflight> Preflight;
		/// Each preflight item's place and subject, by conversation or draft identity.
		std::map<std::string, review_submission_preflight_reference> References;
		/// What a Submit pressed now would prepare.
		std::optional<core::review_prepared_decision> Prepared;
		/// [[BPR-1541]]. Why a submitted review is still unknown, as the last
		/// Check status or the attempt's record left it; none otherwise.
		std::optional<core::review_submission_unknown_notice> Unknown;
	};
	/// \brief [[BPR-1541]]. The line the page shows under "Review status is
	/// unknown": what happened and what to do, in the reviewer's words, with
	/// only the counts the notice carries.
	[[nodiscard]] auto review_submission_unknown_text(const core::review_submission_unknown_notice& aNotice) -> QString;
	/// \brief [[BPR-1085]]. The words the preflight chip says, one item a line.
	[[nodiscard]] auto review_preflight_summary(const std::optional<core::review_preflight>& aPreflight) -> QString;
	/// \brief Presents a PR-level review independently of selected source lines.
	class review_submission_editor final : public QWidget
	{
	public:
		explicit review_submission_editor(review_submission_editor_dispatch aDispatch, QWidget* aParent = nullptr);
		void               present(const review_submission_editor_projection& aProjection);
		[[nodiscard]] auto body_editor() const noexcept -> QPlainTextEdit*;
		/// \brief [[BPR-1085]], [[BPR-1345]] v002. Comment, Approve and Request
		/// changes: the three buttons that finish the review, each submitting
		/// it at once with its own native verdict (owner, finish-review v001:
		/// "these should be the action buttons that finish the review").
		/// Each is enabled only when the review can be submitted as it; a
		/// disabled one says why in its tooltip.
		[[nodiscard]] auto disposition_button(core::review_disposition aDisposition) const noexcept -> QToolButton*;
		/// \brief Chooses a verdict without submitting: the draft keeps it.
		void choose_disposition(core::review_disposition aDisposition);
		/// \brief [[BPR-1085]]. The chip that counts my own blocking feedback and
		/// opens its list.
		[[nodiscard]] auto preflight_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto preflight_menu() const noexcept -> QMenu*;
		/// \brief Submits with the verdict the draft holds. No button shows it
		/// since [[BPR-1345]] v002: the verdict buttons submit.
		[[nodiscard]] auto submit_action() const noexcept -> QAction*;
		/// \brief Discard, or Discard review. A review with a summary or
		/// pending comments asks first, naming what it deletes.
		[[nodiscard]] auto discard_action() const noexcept -> QAction*;
		/// \brief [[BPR-1345]] v002. Discard review's question while it is
		/// open, or null. v003: the shared destructive question.
		[[nodiscard]] auto discard_confirmation() const noexcept -> destructive_question*;
		/// \brief [[BPR-1345]] v002. A pending row's Delete question while it
		/// is open, or null. v003: the shared destructive question, showing
		/// the comment in its box.
		[[nodiscard]] auto delete_confirmation() const noexcept -> destructive_question*;
		/// \brief [[BPR-1345]] v003. Discard review's button: red words, the
		/// deleting style.
		[[nodiscard]] auto discard_button() const noexcept -> danger_tool_button*;
		[[nodiscard]] auto reconcile_action() const noexcept -> QAction*;
		[[nodiscard]] auto status_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1541]]. Why the review is still unknown, under the
		/// status; hidden when there is no reason.
		[[nodiscard]] auto unknown_reason_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1541]] v002. The panel a review awaiting GitHub's
		/// confirmation is shown in, under the title: its mark, its title,
		/// why, what is safe, and Check status. Hidden otherwise.
		[[nodiscard]] auto waiting_panel() const noexcept -> QWidget*;
		[[nodiscard]] auto waiting_title_label() const noexcept -> QLabel*;
		[[nodiscard]] auto waiting_assurance_label() const noexcept -> QLabel*;
		/// \brief Check status's button, in the panel.
		[[nodiscard]] auto reconcile_button() const noexcept -> QToolButton*;
		/// \brief [[BPR-1345]]. The page's title: "Finish your review" while
		/// the review carries line comments, else "Compose review".
		[[nodiscard]] auto title_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1590]] v002. The pull request the review is for, as one
		/// quiet line under the title ("owner/repository #17 · its title");
		/// empty hides it.
		void set_context(const QString& aContext);
		/// \brief [[BPR-1590]] v002. In the sheet beside the code, one faint
		/// line under the box says a row shows its line behind the sheet.
		void               set_reveal_hint(bool aShown);
		[[nodiscard]] auto context_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1345]]. One row per carried comment, hidden with none.
		/// \details v002: a row is a button that goes to its comment, beside
		/// a Delete button.
		[[nodiscard]] auto pending_list() const noexcept -> QWidget*;
		/// \brief [[BPR-1345]]. The rows' words, in order: "renderer.cpp,
		/// line 431: question: Where does 0.01 come from?".
		[[nodiscard]] auto pending_rows() const -> QStringList;
		/// \brief [[BPR-1345]]. The list's heading: "2 pending comments".
		[[nodiscard]] auto carries_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1345]] v002. "Nothing is published until you
		/// submit.", said once, while a review is being written.
		[[nodiscard]] auto note_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1345]] v002. The disclosure that shows the host, the
		/// head and the account; closed by default.
		[[nodiscard]] auto details_toggle() const noexcept -> QToolButton*;
		/// \brief [[BPR-1345]] v002. Where a pending row goes: the window
		/// shows the comment at its line. Navigation only.
		void set_pending_reveal(std::function<void(const core::review_submission_comment&)> aReveal);
		/// \brief [[BPR-1345]] v002. A pending row's Delete, once confirmed.
		void set_pending_delete(std::function<void(const core::review_submission_comment&)> aDelete);
		/// \brief [[BPR-1345]] v002. Called once a confirmed Discard review
		/// has discarded the review, so the window can close the page.
		void set_discard_observer(std::function<void()> aObserver);
		/// \brief [[BPR-1345]] v003. GitHub confirmed this review: the workflow
		/// says so once, after the next review has started. The observer
		/// hears how many line comments it carried, so the window can close
		/// the page and say so (owner, finish-page v002: "once the review is
		/// submitted I just want it to close and return to the PR instead").
		void confirm_submitted(const core::review_submission_draft& aSubmitted);
		void set_submitted_observer(std::function<void(std::size_t)> aObserver);
		/// \brief The last review GitHub confirmed, and how many were.
		[[nodiscard]] auto last_submitted() const noexcept -> const std::optional<core::review_submission_draft>&;
		[[nodiscard]] auto confirmed_submissions() const noexcept -> std::size_t;
		[[nodiscard]] auto target_editor() const noexcept -> QTextEdit*;
		[[nodiscard]] auto shown_draft() const noexcept -> const std::optional<core::review_submission_draft>&;
		/// \brief [[BPR-1085]]. The whole presentation last shown, its preflight
		/// and prepared decision included.
		[[nodiscard]] auto shown_projection() const noexcept -> const review_submission_editor_projection&;
		/// \brief [[BPR-1129]]. Called after every presentation, so the window can
		/// mark Compose review while the presented draft is kept.
		void set_presentation_observer(std::function<void()> aObserver);
		/// \brief [[BPR-1085]]. Where a preflight item's row goes: the window
		/// reveals the conversation, or opens the draft. Navigation only.
		void set_item_reveal(std::function<void(const std::string&)> aReveal);
		/// \brief The tints and mark style the window draws marks with.
		void set_label_marks(const feedback_mark_theme& aTheme);
		/// \brief [[BPR-1085]]. The amber of Approve's dot: the theme's
		/// `state_attention` (Appearance > Colours). Until the window supplies
		/// one, the built-in value of the variant the palette's base implies.
		void               set_attention_colour(const QColor& aColour);
		[[nodiscard]] auto attention_colour() const -> QColor;

		/// Inspect all retained inline comments, including earlier revisions.
		void set_saved_comments_reveal(std::function<void()>);

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		/// [[BPR-1541]] v002. Draws the waiting panel's mark and quieter words
		/// in the current theme and attention colour.
		void restyle_waiting();

	protected:
	private:
		void                                                        present_preflight(const review_submission_editor_projection& aProjection);
		void                                                        present_pending();
		void                                                        present_verdicts();
		void                                                        submit_as(core::review_disposition aDisposition);
		void                                                        confirm_discard();
		void                                                        discard_now();
		void                                                        confirm_delete(const core::review_submission_comment& aComment);
		void                                                        emit_command(core::review_submission_command_kind aKind, std::optional<core::review_disposition> aDisposition = std::nullopt);
		void                                                        emit_operation(review_submission_editor_operation aOperation);
		std::function<void()>                                       SavedCommentsReveal;
		QString                                                     LastAnnouncement;
		review_submission_editor_dispatch                           Dispatch;
		std::optional<core::review_submission_draft>                Draft;
		std::function<void()>                                       PresentationObserver;
		std::function<void(const std::string&)>                     Reveal;
		std::function<void(const core::review_submission_comment&)> PendingReveal;
		std::function<void(const core::review_submission_comment&)> PendingDelete;
		std::function<void()>                                       DiscardObserver;
		std::function<void(std::size_t)>                            SubmittedObserver;
		std::optional<core::review_submission_draft>                LastSubmitted;
		std::size_t                                                 Confirmed = 0U;
		std::optional<feedback_mark_theme>                          MarkTheme;
		std::optional<QColor>                                       Attention;
		review_submission_editor_projection                         Shown;
		QPlainTextEdit*                                             Body = nullptr;
		std::array<QToolButton*, 3>                                 Decisions{ };
		QToolButton*                                                Preflight       = nullptr;
		QMenu*                                                      PreflightMenu   = nullptr;
		QTextEdit*                                                  Target          = nullptr;
		QLabel*                                                     Status          = nullptr;
		QLabel*                                                     UnknownReason   = nullptr;
		QWidget*                                                    Waiting         = nullptr;
		QLabel*                                                     WaitingMark     = nullptr;
		QLabel*                                                     WaitingTitle    = nullptr;
		QLabel*                                                     WaitingFallback = nullptr;
		QLabel*                                                     WaitingAssure   = nullptr;
		QLabel*                                                     DraftState      = nullptr;
		QLabel*                                                     Title           = nullptr;
		QLabel*                                                     Purpose         = nullptr;
		QWidget*                                                    Pending         = nullptr;
		QVBoxLayout*                                                PendingRows     = nullptr;
		QLabel*                                                     Carries         = nullptr;
		/// [[BPR-1590]] v002. The pull request line, the box's heading and its rules.
		QLabel*                                      Context         = nullptr;
		QWidget*                                     Heading         = nullptr;
		QWidget*                                     HeadingRule     = nullptr;
		QWidget*                                     SummaryRule     = nullptr;
		QLabel*                                      RevealHint      = nullptr;
		bool                                         HintWanted      = false;
		QToolButton*                                 Details         = nullptr;
		QWidget*                                     Recovery        = nullptr;
		danger_tool_button*                          DiscardButton   = nullptr;
		QToolButton*                                 ReconcileButton = nullptr;
		QPointer<destructive_question>               DiscardQuestion;
		QPointer<destructive_question>               DeleteQuestion;
		std::vector<core::review_submission_comment> Listed;
		QStringList                                  ListedRows;
		QAction*                                     Submit    = nullptr;
		QAction*                                     Discard   = nullptr;
		QAction*                                     Reconcile = nullptr;
	};
}
#endif
