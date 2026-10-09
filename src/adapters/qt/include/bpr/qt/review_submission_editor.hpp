#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_SUBMISSION_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_SUBMISSION_EDITOR_HPP

#include "bpr/core/review_preflight.hpp"
#include "bpr/core/review_submission.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <string>

class QAction;
class QButtonGroup;
class QLabel;
class QMenu;
class QPlainTextEdit;
class QTextEdit;
class QToolButton;
class QVBoxLayout;

namespace bpr::qt {
	enum class review_submission_editor_operation { command,
													reconcile,
													new_review };
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
	};
	/// \brief [[BPR-1085]]. The words the preflight chip says, one item a line.
	[[nodiscard]] auto review_preflight_summary(const std::optional<core::review_preflight>& aPreflight) -> QString;
	/// \brief Presents a PR-level review independently of selected source lines.
	class review_submission_editor final : public QWidget
	{
	public:
		explicit review_submission_editor(review_submission_editor_dispatch aDispatch, QWidget* aParent = nullptr);
		void               present(const review_submission_editor_projection& aProjection);
		[[nodiscard]] auto body_editor() const noexcept -> QPlainTextEdit*;
		/// \brief [[BPR-1085]]. Comment, Approve and Request changes: three
		/// explicit, exclusive buttons carrying the native dispositions.
		[[nodiscard]] auto disposition_button(core::review_disposition aDisposition) const noexcept -> QToolButton*;
		/// \brief [[BPR-1085]]. The chip that counts my own blocking feedback and
		/// opens its list.
		[[nodiscard]] auto preflight_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto preflight_menu() const noexcept -> QMenu*;
		[[nodiscard]] auto submit_action() const noexcept -> QAction*;
		[[nodiscard]] auto discard_action() const noexcept -> QAction*;
		[[nodiscard]] auto reconcile_action() const noexcept -> QAction*;
		[[nodiscard]] auto new_review_action() const noexcept -> QAction*;
		[[nodiscard]] auto status_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1345]]. The page's title: "Finish your review" while
		/// the review carries line comments, else "Compose review".
		[[nodiscard]] auto title_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1345]]. "Pending in this review" and one row per
		/// carried comment, hidden with none.
		[[nodiscard]] auto pending_list() const noexcept -> QWidget*;
		/// \brief [[BPR-1345]]. The rows' words, in order: "renderer.cpp,
		/// line 431: question: Where does 0.01 come from?".
		[[nodiscard]] auto pending_rows() const -> QStringList;
		/// \brief [[BPR-1345]]. "Submits 2 line comments as one review."
		[[nodiscard]] auto carries_label() const noexcept -> QLabel*;
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

	private:
		void                                         present_preflight(const review_submission_editor_projection& aProjection);
		void                                         present_pending();
		void                                         emit_command(core::review_submission_command_kind aKind);
		void                                         emit_operation(review_submission_editor_operation aOperation);
		QString                                      LastAnnouncement;
		review_submission_editor_dispatch            Dispatch;
		std::optional<core::review_submission_draft> Draft;
		std::function<void()>                        PresentationObserver;
		std::function<void(const std::string&)>      Reveal;
		std::optional<feedback_mark_theme>           MarkTheme;
		std::optional<QColor>                        Attention;
		review_submission_editor_projection          Shown;
		QPlainTextEdit*                              Body        = nullptr;
		QButtonGroup*                                Disposition = nullptr;
		std::array<QToolButton*, 3>                  Decisions{ };
		QToolButton*                                 Preflight     = nullptr;
		QMenu*                                       PreflightMenu = nullptr;
		QTextEdit*                                   Target        = nullptr;
		QLabel*                                      Status        = nullptr;
		QLabel*                                      DraftState    = nullptr;
		QLabel*                                      Title         = nullptr;
		QLabel*                                      Purpose       = nullptr;
		QWidget*                                     Pending       = nullptr;
		QVBoxLayout*                                 PendingRows   = nullptr;
		QLabel*                                      Carries       = nullptr;
		std::vector<core::review_submission_comment> Listed;
		QStringList                                  ListedRows;
		QAction*                                     Submit    = nullptr;
		QAction*                                     Discard   = nullptr;
		QAction*                                     Reconcile = nullptr;
		QAction*                                     NewReview = nullptr;
	};
}
#endif
