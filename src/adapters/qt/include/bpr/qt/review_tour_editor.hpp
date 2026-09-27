#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_TOUR_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_TOUR_EDITOR_HPP
#include "bpr/application/review_tour_projection.hpp"
#include <QWidget>
#include <functional>
#include <map>
class QAction;
class QComboBox;
class QListWidget;
class QLineEdit;
class QTextDocument;
class QPlainTextEdit;
class QTabWidget;
class QLabel;
class QPushButton;
namespace bpr::qt {
	class markdown_body_view;
	enum class tour_editor_action : std::uint8_t { import_selected,
												   retry_save,
												   use_saved_copy,
												   map_problem,
												   map_solution,
												   add_stop,
												   insert_before,
												   add_related,
												   navigate_stop,
												   navigate_related,
												   undo_structure,
												   redo_structure };
	/// \brief Native Markdown interaction and inert projections for private tour preparation.
	class review_tour_editor final : public QWidget
	{
	public:
		explicit review_tour_editor(QWidget* aParent = nullptr);
		void               set_dispatch(std::function<void(core::tour_command)> aDispatch, std::function<void(tour_editor_action)> aAction);
		void               present(const application::tour_presentation& aProjection);
		void               show_problem();
		[[nodiscard]] auto import_choice() const -> std::size_t;
		void               show_notice(const QString& aText);
		void               show_stops();
		void               trigger(tour_editor_action aAction);
		[[nodiscard]] auto selected_stop() const -> const core::tour_stop*;
		[[nodiscard]] auto selected_related() const -> std::optional<core::tour_source_range>;
		[[nodiscard]] auto outline() const -> QListWidget* { return Outline; }
		[[nodiscard]] auto stop_title() const -> QLineEdit* { return Title; }
		[[nodiscard]] auto stop_body() const -> QPlainTextEdit* { return Body; }
		[[nodiscard]] auto problem_editor() const -> QPlainTextEdit* { return Problem; }
		[[nodiscard]] auto solution_editor() const -> QPlainTextEdit* { return Solution; }
		[[nodiscard]] auto roadmap() const -> QTabWidget* { return Roadmap; }
		[[nodiscard]] auto preview() const -> markdown_body_view* { return Preview; }
		[[nodiscard]] auto shown() const -> const application::tour_presentation& { return Shown; }
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	private:
		void                                         edit(core::tour_command_kind aKind, const QString& aText);
		void                                         refresh_preview();
		void                                         build_stops();
		void                                         present_stops(bool aOwnerChanged);
		void                                         edit_stop();
		void                                         structural(core::tour_command_kind aKind, std::size_t aPosition = 0);
		void                                         next_stop(int aOffset);
		QComboBox*                                   Imports       = nullptr;
		QPushButton*                                 Import        = nullptr;
		QListWidget*                                 Outline       = nullptr;
		QListWidget*                                 Related       = nullptr;
		QLineEdit*                                   Title         = nullptr;
		QPlainTextEdit*                              Body          = nullptr;
		QLabel*                                      Source        = nullptr;
		QWidget*                                     StopFields    = nullptr;
		QAction*                                     MoveUp        = nullptr;
		QAction*                                     MoveDown      = nullptr;
		QAction*                                     UndoStructure = nullptr;
		QAction*                                     RedoStructure = nullptr;
		core::tour_stop_id                           Selected;
		core::tour_stop_id                           Editing;
		std::optional<core::tour_command_kind>       EditingKind;
		std::map<core::tour_stop_id, QTextDocument*> BodyDocuments;
		std::map<core::tour_stop_id, int>            BodyPositions;
		application::tour_presentation               Shown;
		std::function<void(core::tour_command)>      Dispatch;
		std::function<void(tour_editor_action)>      Action;
		QPlainTextEdit*                              Problem  = nullptr;
		QPlainTextEdit*                              Solution = nullptr;
		QPlainTextEdit*                              Conflict = nullptr;
		QTabWidget*                                  Roadmap  = nullptr;
		markdown_body_view*                          Preview  = nullptr;
		QLabel*                                      Status   = nullptr;
		QPushButton*                                 Retry    = nullptr;
		QPushButton*                                 UseSaved = nullptr;
		bool                                         Applying = false;
	};
}
#endif
