#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SOURCE_SEARCH_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SOURCE_SEARCH_PANEL_HPP
#include "bpr/qt/source_find_bar.hpp"
#include <QFont>
class QTreeView;
class QLineEdit;
class QLabel;
class QToolButton;
namespace bpr::qt {
	class source_search_result_model;
	/// \brief Native PR search projection, preserving result indexes between ordered batches.
	class source_search_panel final : public QWidget
	{
	public:
		explicit source_search_panel(QWidget* aParent);
		std::function<void(source_find_gesture)> Dispatch;
		std::function<void(std::uint32_t)>       Activate;
		std::function<void(std::uint32_t)>       Select;
		std::function<void()>                    Changed;
		void                                     present(const application::source_search_projection& aProjection);
		void                                     set_scope_labels(QString aLabel, QString aIdentity);
		void                                     set_snippet_font(const QFont& aFont);
		void                                     reset();
		void                                     cancel_pending();
		[[nodiscard]] auto                       bar() const -> source_find_bar*;
		[[nodiscard]] auto                       include() const -> std::string;
		[[nodiscard]] auto                       exclude() const -> std::string;
		[[nodiscard]] auto                       projection() const -> const application::source_search_projection&;
		[[nodiscard]] auto                       tree() const -> QTreeView*;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		source_find_bar*                      Bar;
		QLabel*                               Scope;
		QLabel*                               Identity;
		QLabel*                               Coverage;
		QLabel*                               Skipped;
		QToolButton*                          Retry;
		QLineEdit*                            Include;
		QLineEdit*                            Exclude;
		QTreeView*                            Tree;
		source_search_result_model*           Model;
		QTimer*                               Debounce;
		application::source_search_projection Projection;
		void                                  append_results(const application::source_search_projection& aProjection);
	};
}
#endif
