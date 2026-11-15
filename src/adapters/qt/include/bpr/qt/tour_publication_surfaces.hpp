#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TOUR_PUBLICATION_SURFACES_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TOUR_PUBLICATION_SURFACES_HPP

#include "bpr/application/review_tour_projection.hpp"
#include "bpr/core/tour_publication_head.hpp"

#include <QWidget>

#include <array>
#include <functional>
#include <vector>

class QLabel;
class QProgressBar;
class QPushButton;
class QVBoxLayout;

/// \file
/// \brief [[BPR-1991]], [[BPR-1992]]. RFC-BPR-0099: the tour's head, its
/// publication steps and the banner a failure is said in. The Tour tab and
/// Overview share the head so both say the same state and offer the same
/// actions.
namespace bpr::qt {
	/// \brief The error role: the red a failure is drawn in, for the window's ground.
	[[nodiscard]] auto tour_error_colour(const QPalette& aPalette) -> QColor;
	/// \brief The head's state line in words.
	[[nodiscard]] auto tour_head_text(const core::tour_head& aHead) -> QString;
	/// \brief What a disabled Publish… is missing, for its tooltip.
	[[nodiscard]] auto tour_head_missing_text(const core::tour_head& aHead) -> QString;
	/// \brief An action's label: Publish…, Details, Resume, On GitHub, Update on GitHub….
	[[nodiscard]] auto tour_head_action_text(core::tour_head_action aAction) -> QString;

	/// \brief The tour's state line and its actions; the first action is primary.
	class tour_head_bar final : public QWidget
	{
	public:
		using action_dispatch = std::function<void(core::tour_head_action)>;
		/// \param aName The object name; its parts are named after it
		/// ("…State", "…Progress", "…Publish", …).
		tour_head_bar(const QString& aName, QWidget* aParent);
		void               present(const core::tour_head& aHead);
		void               set_dispatch(action_dispatch aDispatch);
		[[nodiscard]] auto shown() const noexcept -> const core::tour_head& { return Shown; }
		[[nodiscard]] auto state_label() const noexcept -> QLabel* { return State; }
		[[nodiscard]] auto progress() const noexcept -> QProgressBar* { return Progress; }
		[[nodiscard]] auto button(core::tour_head_action aAction) const -> QPushButton*;
		/// \brief The actions shown, in order.
		[[nodiscard]] auto shown_actions() const -> std::vector<core::tour_head_action>;

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		void                        restyle();
		core::tour_head             Shown;
		action_dispatch             Dispatch;
		QLabel*                     State    = nullptr;
		QProgressBar*               Progress = nullptr;
		std::array<QPushButton*, 5> Primary{ };
		std::array<QPushButton*, 5> Quiet{ };
	};

	/// \brief The publication's steps as a checklist: sent, sending, waiting,
	/// failed with its reason, or not sent.
	class tour_step_list final : public QWidget
	{
	public:
		explicit tour_step_list(QWidget* aParent);
		void               present(const std::vector<application::tour_publication_step_view>& aSteps);
		[[nodiscard]] auto shown() const noexcept -> const std::vector<application::tour_publication_step_view>& { return Shown; }
		/// \brief Each row's accessible line, "Sent: The tour comment".
		[[nodiscard]] auto row_texts() const -> QStringList;

	private:
		std::vector<application::tour_publication_step_view> Shown;
		std::vector<QWidget*>                                Rows;
		QVBoxLayout*                                         Column = nullptr;
	};

	/// \brief A refusal or failure, in the error role: what stopped and where,
	/// why, what is on GitHub and what is not, and the actions that move
	/// forward, the first of them primary.
	class tour_publication_banner_view final : public QWidget
	{
	public:
		using remedy_dispatch = std::function<void(application::tour_publication_remedy, core::tour_stop_id)>;
		explicit tour_publication_banner_view(QWidget* aParent);
		void               present(const std::optional<application::tour_publication_banner>& aBanner);
		void               set_dispatch(remedy_dispatch aDispatch);
		[[nodiscard]] auto title() const noexcept -> QLabel* { return Title; }
		[[nodiscard]] auto why() const noexcept -> QLabel* { return Why; }
		[[nodiscard]] auto on_github() const noexcept -> QLabel* { return OnGitHub; }
		[[nodiscard]] auto buttons() const -> std::vector<QPushButton*>;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		std::optional<application::tour_publication_banner> Shown;
		remedy_dispatch                                     Dispatch;
		QLabel*                                             Title    = nullptr;
		QLabel*                                             Why      = nullptr;
		QLabel*                                             OnGitHub = nullptr;
		QWidget*                                            Actions  = nullptr;
	};
} // namespace bpr::qt

#endif
