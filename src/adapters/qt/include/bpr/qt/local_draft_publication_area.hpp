#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_PUBLICATION_AREA_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_PUBLICATION_AREA_HPP

#include "bpr/application/local_draft_workbench.hpp"

#include <QWidget>

#include <cstdint>
#include <functional>
#include <string>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace bpr::qt {
	class tour_step_list;
	class tour_publication_banner_view;

	/// \brief The gestures the Pull request area sends to its host.
	struct local_draft_publication_dispatch
	{
		/// Reads GitHub for the plan; with the tour as a follow-up or not.
		std::function<void(bool)> Check;
		/// The author's confirmation of the plan shown, by its digest.
		std::function<void(const std::string&)> Create;
		std::function<void()>                   Recover;
		std::function<void(std::uint64_t)>      Associate;
		/// Copies the named push for the author's terminal.
		std::function<void(const std::string&)> Copy;
	};

	/// \brief RFC-BPR-0111 ([[BPR-2029]], [[BPR-2030]]). A local draft's Pull
	/// request area: whether the pushed branch is the candidate, the push to
	/// run when it is not, the exact plan Create draft PR confirms, its steps
	/// and a failure in the error role (RFC-BPR-0099's presentation, X18).
	/// \details It decides nothing and performs no effect; BPR pushes nothing.
	/// Secondary detail is in tooltips. Controls keep their place: a press
	/// disables what cannot run again until GitHub answers.
	class local_draft_publication_area final : public QWidget
	{
	public:
		explicit local_draft_publication_area(QWidget* aParent = nullptr);

		void set_dispatch(local_draft_publication_dispatch aDispatch);
		void present(const application::local_draft_workbench_projection& aProjection);

		[[nodiscard]] auto state() const noexcept -> QLabel* { return State; }
		[[nodiscard]] auto check_button() const noexcept -> QPushButton* { return Check; }
		[[nodiscard]] auto create_button() const noexcept -> QPushButton* { return Create; }
		[[nodiscard]] auto push_command() const noexcept -> QLineEdit* { return Push; }
		[[nodiscard]] auto copy_button() const noexcept -> QPushButton* { return Copy; }
		[[nodiscard]] auto associate_button() const noexcept -> QPushButton* { return Associate; }
		[[nodiscard]] auto tour_follow_up() const noexcept -> QCheckBox* { return Tour; }
		[[nodiscard]] auto preview() const noexcept -> QPlainTextEdit* { return Preview; }
		[[nodiscard]] auto steps() const noexcept -> tour_step_list* { return Steps; }
		[[nodiscard]] auto banner() const noexcept -> tour_publication_banner_view* { return Banner; }
		[[nodiscard]] auto link() const noexcept -> QLabel* { return Link; }

	private:
		local_draft_publication_dispatch              Dispatch;
		application::local_draft_workbench_projection Projection;
		QLabel*                                       State     = nullptr;
		QPushButton*                                  Check     = nullptr;
		QPushButton*                                  Create    = nullptr;
		QWidget*                                      PushRow   = nullptr;
		QLineEdit*                                    Push      = nullptr;
		QPushButton*                                  Copy      = nullptr;
		QWidget*                                      Existing  = nullptr;
		QLabel*                                       ExistingText = nullptr;
		QPushButton*                                  Associate = nullptr;
		QCheckBox*                                    Tour      = nullptr;
		QPlainTextEdit*                               Preview   = nullptr;
		tour_step_list*                               Steps     = nullptr;
		tour_publication_banner_view*                 Banner    = nullptr;
		QLabel*                                       Link      = nullptr;
	};

	/// \brief The area's state line, for the page and a course.
	[[nodiscard]] auto local_publication_state_text(const application::local_draft_workbench_projection& aProjection) -> QString;
}

#endif
