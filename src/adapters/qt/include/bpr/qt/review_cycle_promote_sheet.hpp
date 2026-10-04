#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_CYCLE_PROMOTE_SHEET_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_CYCLE_PROMOTE_SHEET_HPP

#include "bpr/application/review_cycle_promotion.hpp"

#include <QDialog>

#include <functional>
#include <optional>

class QLabel;
class QListWidget;
class QPushButton;

namespace bpr::qt {
	/// \brief The gestures Add Notes to Review Tour sends to its host ([[BPR-1273]]).
	struct review_cycle_promote_dispatch
	{
		/// Done or Escape: the sheet closes; what was added stays in the tour.
		std::function<void()>                                   Back;
		std::function<void(core::cycle_feedback_item_id, bool)> Choose;
		std::function<void()>                                   Add;
		/// Open Pull Request: the composition hands the linked pull request to
		/// the existing pull request workflow, where its tour is completed.
		std::function<void()> OpenPullRequest;
	};

	/// \brief Add Notes to Review Tour (RFC-BPR-0059, RC59-09, [[BPR-1273]]):
	/// one sheet over the code, as Publish as Pull Request is.
	///
	/// \details It lists the cycle's notes, each checkable, each in two lines:
	/// the note's location and first words, then where its stop would point,
	/// in plain words: the exact lines the pull request's commit holds, or no
	/// lines yet and why (written on an earlier capture, other bytes in the
	/// commit, a folder, the whole change), and what the tour's own policy
	/// makes of it on GitHub when the tour is published. A ledger in the foot
	/// says that adding sends nothing to GitHub and what stays on this
	/// computer. One default button adds the chosen notes; Open Pull Request
	/// takes the reviewer to the tour. It decides and performs nothing.
	class review_cycle_promote_sheet final : public QDialog
	{
	public:
		explicit review_cycle_promote_sheet(QWidget* aParent);

		void set_dispatch(review_cycle_promote_dispatch aDispatch);
		void present(const application::review_cycle_promotion_view& aView);
		void set_anchor(QPoint aGlobal);
		void focus_first();

		[[nodiscard]] auto title() const noexcept -> QLabel*;
		[[nodiscard]] auto intro() const noexcept -> QLabel*;
		/// \brief Every note, two lines each; a note that can be added is checkable.
		[[nodiscard]] auto notes() const noexcept -> QListWidget*;
		[[nodiscard]] auto outgoing_words() const noexcept -> QLabel*;
		[[nodiscard]] auto private_words() const noexcept -> QLabel*;
		[[nodiscard]] auto status() const noexcept -> QLabel*;
		[[nodiscard]] auto add_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto open_pull_request_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto close_button() const noexcept -> QPushButton*;
		/// \brief A row's two lines as one text, for a reader or a course.
		[[nodiscard]] static auto row_text(const application::review_cycle_promotion_row& aRow) -> QString;

	protected:
		void done(int aResult) override;
		void showEvent(QShowEvent* aEvent) override;

	private:
		void place();

		review_cycle_promote_dispatch            Dispatch;
		application::review_cycle_promotion_view Shown;
		QLabel*                                  Title     = nullptr;
		QLabel*                                  Intro     = nullptr;
		QListWidget*                             Notes     = nullptr;
		QLabel*                                  Outgoing  = nullptr;
		QLabel*                                  Private   = nullptr;
		QLabel*                                  Status    = nullptr;
		QPushButton*                             Add       = nullptr;
		QPushButton*                             OpenPull  = nullptr;
		QPushButton*                             Close     = nullptr;
		std::optional<QPoint>                    Anchor;
		/// Rows are being filled from a projection: their signals are not gestures.
		bool Presenting = false;
	};
}

#endif
