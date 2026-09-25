#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_SUBMISSION_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_SUBMISSION_EDITOR_HPP

#include "bpr/core/review_submission.hpp"

#include <QString>
#include <QWidget>
#include <functional>

class QAction;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QTextEdit;

namespace bpr::qt {
	enum class review_submission_editor_operation { command,
													reconcile,
													new_review };
	struct review_submission_editor_intent
	{
		review_submission_editor_operation Operation = review_submission_editor_operation::command;
		core::review_submission_command    Command;
	};
	using review_submission_editor_dispatch = std::function<void(review_submission_editor_intent)>;
	/// \brief Safe, decided presentation; no provider, request or execution authority.
	struct review_submission_editor_projection
	{
		std::optional<core::review_submission_draft> Draft;
		core::review_submission_controls             Controls;
		QString                                      TargetDescription;
		QString                                      Status;
	};
	/// \brief Presents a PR-level review independently of selected source lines.
	class review_submission_editor final : public QWidget
	{
	public:
		explicit review_submission_editor(review_submission_editor_dispatch aDispatch, QWidget* aParent = nullptr);
		void               present(const review_submission_editor_projection& aProjection);
		[[nodiscard]] auto body_editor() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto disposition_editor() const noexcept -> QComboBox*;
		[[nodiscard]] auto submit_action() const noexcept -> QAction*;
		[[nodiscard]] auto discard_action() const noexcept -> QAction*;
		[[nodiscard]] auto reconcile_action() const noexcept -> QAction*;
		[[nodiscard]] auto new_review_action() const noexcept -> QAction*;
		[[nodiscard]] auto status_label() const noexcept -> QLabel*;
		[[nodiscard]] auto target_editor() const noexcept -> QTextEdit*;
		[[nodiscard]] auto shown_draft() const noexcept -> const std::optional<core::review_submission_draft>&;
		/// \brief [[BPR-1129]]. Called after every presentation, so the window can
		/// mark Compose review while the presented draft is kept.
		void set_presentation_observer(std::function<void()> aObserver);

	private:
		void                                         emit_command(core::review_submission_command_kind aKind);
		void                                         emit_operation(review_submission_editor_operation aOperation);
		QString                                      LastAnnouncement;
		review_submission_editor_dispatch            Dispatch;
		std::optional<core::review_submission_draft> Draft;
		std::function<void()>                        PresentationObserver;
		QPlainTextEdit*                              Body        = nullptr;
		QComboBox*                                   Disposition = nullptr;
		QTextEdit*                                   Target      = nullptr;
		QLabel*                                      Status      = nullptr;
		QAction*                                     Submit      = nullptr;
		QAction*                                     Discard     = nullptr;
		QAction*                                     Reconcile   = nullptr;
		QAction*                                     NewReview   = nullptr;
	};
}
#endif
