#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SOURCE_FIND_BAR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SOURCE_FIND_BAR_HPP

#include "bpr/application/source_search_projection.hpp"
#include <QWidget>
#include <functional>

class QLabel;
class QPlainTextEdit;
class QToolButton;
class QTimer;

namespace bpr::qt {
	enum class source_find_gesture { edit,
									 next,
									 previous,
									 close,
									 selection,
									 query_dirty,
									 document_changed,
									 escape };
	/// \brief Presentation gesture with fixed original source bounds and reading origin.
	struct source_find_intent
	{
		source_find_gesture                      Gesture = source_find_gesture::edit;
		std::string                              Query;
		core::source_search_options              Options;
		core::source_search_target               Target = core::source_search_target::right;
		std::optional<core::source_search_range> Selection;
		core::diff_side                          OriginSide = core::diff_side::right;
		std::size_t                              OriginByte = 0U;
		std::uint32_t                            OriginRow  = 0U;
	};
	/// \brief Native multiline pane query controls; effects and session policy stay outside Qt.
	class source_find_bar final : public QWidget
	{
	public:
		explicit source_find_bar(QWidget* aParent, bool aFiles = false);
		std::function<void(source_find_gesture)> Dispatch;
		void                                     present(const application::source_search_projection& aProjection);
		void                                     seed(const QString& aText, bool aLiteral);
		void                                     set_scope(core::source_search_target aTarget, bool aHasSelection, QString aReason);
		void                                     clear_selection();
		void                                     reset_query();
		void                                     flush();
		void                                     cancel_pending();
		[[nodiscard]] auto                       query() const -> std::string;
		[[nodiscard]] auto                       options() const -> core::source_search_options;
		[[nodiscard]] auto                       in_selection() const -> bool;
		[[nodiscard]] auto                       query_editor() const -> QPlainTextEdit*;
		[[nodiscard]] auto                       status_text() const -> QString;
		[[nodiscard]] auto                       projection() const -> const application::source_search_projection&;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void changeEvent(QEvent* aEvent) override;

	private:
		QPlainTextEdit*                       Query     = nullptr;
		QToolButton*                          Case      = nullptr;
		QToolButton*                          Word      = nullptr;
		QToolButton*                          Regex     = nullptr;
		QToolButton*                          Selection = nullptr;
		QToolButton*                          Previous  = nullptr;
		QToolButton*                          Next      = nullptr;
		QLabel*                               Scope     = nullptr;
		QLabel*                               Status    = nullptr;
		QTimer*                               Debounce  = nullptr;
		bool                                  Composing = false;
		bool                                  Files     = false;
		QString                               SeedError;
		application::source_search_projection Projection;
		void                                  schedule();
		void                                  adapt_font();
	};
}
#endif
