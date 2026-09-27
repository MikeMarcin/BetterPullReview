#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_TOUR_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_TOUR_EDITOR_HPP
#include "bpr/application/review_tour_projection.hpp"
#include <QWidget>
#include <functional>
class QPlainTextEdit;
class QTabWidget;
class QLabel;
class QPushButton;
namespace bpr::qt {
	class markdown_body_view;
	enum class tour_editor_action : std::uint8_t { retry_save,
												   use_saved_copy,
												   map_problem,
												   map_solution };
	/// \brief Native Markdown interaction and inert projections for private tour preparation.
	class review_tour_editor final : public QWidget
	{
	public:
		explicit review_tour_editor(QWidget* aParent = nullptr);
		void               set_dispatch(std::function<void(core::tour_command)> aDispatch, std::function<void(tour_editor_action)> aAction);
		void               present(const application::tour_presentation& aProjection);
		void               show_problem();
		void               show_notice(const QString& aText);
		[[nodiscard]] auto problem_editor() const -> QPlainTextEdit* { return Problem; }
		[[nodiscard]] auto solution_editor() const -> QPlainTextEdit* { return Solution; }
		[[nodiscard]] auto roadmap() const -> QTabWidget* { return Roadmap; }
		[[nodiscard]] auto preview() const -> markdown_body_view* { return Preview; }
		[[nodiscard]] auto shown() const -> const application::tour_presentation& { return Shown; }
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	private:
		void                                    edit(core::tour_command_kind aKind, const QString& aText);
		void                                    refresh_preview();
		application::tour_presentation          Shown;
		std::function<void(core::tour_command)> Dispatch;
		std::function<void(tour_editor_action)> Action;
		QPlainTextEdit*                         Problem  = nullptr;
		QPlainTextEdit*                         Solution = nullptr;
		QPlainTextEdit*                         Conflict = nullptr;
		QTabWidget*                             Roadmap  = nullptr;
		markdown_body_view*                     Preview  = nullptr;
		QLabel*                                 Status   = nullptr;
		QPushButton*                            Retry    = nullptr;
		QPushButton*                            UseSaved = nullptr;
		bool                                    Applying = false;
	};
}
#endif
