#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GENERATED_FILES_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GENERATED_FILES_VIEW_HPP

#include "bpr/application/commit_filter_projection.hpp"

#include <QString>
#include <QWidget>

#include <functional>
#include <optional>
#include <string>
#include <vector>

class QLabel;
class QTreeWidget;
class QMenu;
class QToolButton;

/// \file
/// \brief RFC-BPR-0089 M2. The "N generated files hidden" row that ends the
/// Overview's file tree and the Files drawer's, and the generated-file
/// gestures every file menu offers.
///
/// Presentation only: which files are generated, why, and which gestures a
/// file offers are the core's decisions in the host's projection. A gesture
/// is dispatched by path; nothing here matches a pattern or writes a mark.
namespace bpr::qt {
	using generated_gesture_dispatch = std::function<void(const application::generated_file_gesture&)>;
	/// \brief [[BPR-1693]]. Asks for one set-aside file's diff, by path. It
	/// stays set aside: nothing is marked and the walk is unchanged.
	using generated_open_dispatch = std::function<void(const std::string&)>;

	/// \brief The gesture's menu text.
	[[nodiscard]] auto generated_gesture_text(core::generated_gesture aGesture) -> QString;
	/// \brief Why a file is or is not generated, for its tag and tooltip.
	[[nodiscard]] auto generated_reason_text(core::generated_reason aReason) -> QString;
	/// \brief "321 generated files hidden", or empty with none.
	[[nodiscard]] auto generated_row_text(const application::commit_filter_projection& aProjection) -> QString;

	/// \brief The gestures one listed file offers: its own when the projection
	/// names it, an ordinary file's otherwise.
	[[nodiscard]] auto generated_gestures_for(const application::commit_filter_projection& aProjection, const std::string& aPath)
		-> std::vector<core::generated_gesture>;

	/// \brief Appends the file's generated gestures to a menu, after a
	/// separator when the menu already has actions.
	void add_generated_actions(QMenu& aMenu, const std::string& aPath, const std::vector<core::generated_gesture>& aGestures,
							   const generated_gesture_dispatch& aDispatch);

	/// \brief The row at the end of a file tree: a disclosure naming how many
	/// generated files are set aside and their line totals, which lists them
	/// when opened.
	///
	/// \details Hidden while nothing is set aside. Each listed file shows its
	/// path and why it is generated; its context menu offers Review anyway,
	/// Show as normal and, for a file marked generated, Follow .gitattributes.
	/// [[BPR-1693]]. Activating a listed file, or Open diff in its menu, asks
	/// for its diff on request; with `set_open_on_click` a single click does.
	class generated_files_row final : public QWidget
	{
	public:
		explicit generated_files_row(QWidget* aParent = nullptr);

		void present(const application::commit_filter_projection& aProjection);
		void set_gesture_dispatch(generated_gesture_dispatch aDispatch);
		void set_open_dispatch(generated_open_dispatch aDispatch);
		/// \brief Whether a single click opens a listed file, as a click in
		/// the Files drawer's tree does; otherwise activation does.
		void set_open_on_click(bool aOpenOnClick);

		/// \brief The menu for one listed file; the caller owns it.
		[[nodiscard]] auto make_menu(int aRow, QWidget* aParent) -> QMenu*;

		[[nodiscard]] auto toggle() const noexcept -> QToolButton*;
		[[nodiscard]] auto totals() const noexcept -> QLabel*;
		[[nodiscard]] auto list() const noexcept -> QTreeWidget*;
		[[nodiscard]] auto open() const noexcept -> bool;
		[[nodiscard]] auto rows() const noexcept -> const std::vector<application::generated_file_row>&;

	private:
		void set_open(bool aOpen);
		void open_row(int aRow);

		QToolButton*                                 Toggle = nullptr;
		QLabel*                                      Totals = nullptr;
		QTreeWidget*                                 List   = nullptr;
		bool                                         Open   = false;
		std::vector<application::generated_file_row> Rows;
		generated_gesture_dispatch                   Dispatch;
		generated_open_dispatch                      OpenDispatch;
		bool                                         OpenOnClick = false;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GENERATED_FILES_VIEW_HPP
