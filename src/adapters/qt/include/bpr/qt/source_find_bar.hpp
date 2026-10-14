#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SOURCE_FIND_BAR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SOURCE_FIND_BAR_HPP

#include "bpr/application/source_search_projection.hpp"
#include <QColor>
#include <QWidget>
#include <functional>

class QLabel;
class QPlainTextEdit;
class QToolButton;
class QTimer;
class QKeyEvent;

namespace bpr::qt {
	class appearance_profile;
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
	/// \brief The three query options a toggle inside the field controls.
	enum class source_find_option : std::uint8_t { match_case,
												   whole_word,
												   regular_expression };
	/// \brief Native multiline query controls; effects and session policy stay outside Qt.
	///
	/// \details [[BPR-1503]]. Pane Find is a floating panel modelled on VS
	/// Code's find widget: the query field carries the Aa, ab and .* toggles at
	/// its right end, then the match count, previous, next, find in selection
	/// and close. It is never placed in a layout; its owner positions it over
	/// the focused pane, so opening it moves no code. Files mode is the same
	/// field alone, for the Find in Files panel ([[BPR-1504]]).
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
		/// \brief Takes the error colour from the theme's defect role.
		void adopt_appearance(const appearance_profile& aProfile);
		/// \brief Flips one option as its toggle would; true when it changed.
		auto toggle_option(source_find_option aOption) -> bool;
		/// \brief Option+C, Option+W and Option+R; true when the key was one of them.
		auto handle_option_key(const QKeyEvent& aKey) -> bool;
		/// \brief The panel width that shows every control with a useful query.
		[[nodiscard]] auto preferred_width() const -> int;
		/// \brief Compacts the panel to fit a narrow pane; returns its minimum width.
		/// \details First the spacing and the count's reserve tighten, then the
		/// find-in-selection toggle gives way (Option+L still toggles it).
		auto               fit_to(int aAvailable) -> int;
		[[nodiscard]] auto query() const -> std::string;
		[[nodiscard]] auto options() const -> core::source_search_options;
		[[nodiscard]] auto in_selection() const -> bool;
		[[nodiscard]] auto query_editor() const -> QPlainTextEdit*;
		[[nodiscard]] auto status_text() const -> QString;
		/// \brief The words the count area shows, short where the status is long.
		[[nodiscard]] auto count_text() const -> QString;
		/// \brief True while the query is an invalid or unsupported pattern, or too long.
		[[nodiscard]] auto error_state() const -> bool;
		[[nodiscard]] auto option_button(source_find_option aOption) const -> QToolButton*;
		[[nodiscard]] auto projection() const -> const application::source_search_projection&;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		void changeEvent(QEvent* aEvent) override;
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		QWidget*                              Field     = nullptr;
		QPlainTextEdit*                       Query     = nullptr;
		QToolButton*                          Case      = nullptr;
		QToolButton*                          Word      = nullptr;
		QToolButton*                          Regex     = nullptr;
		QToolButton*                          Selection = nullptr;
		QToolButton*                          Previous  = nullptr;
		QToolButton*                          Next      = nullptr;
		QToolButton*                          Close     = nullptr;
		QLabel*                               Count     = nullptr;
		bool                                  Compact   = false;
		QTimer*                               Debounce  = nullptr;
		bool                                  Composing = false;
		bool                                  Files     = false;
		bool                                  Error     = false;
		QString                               ScopeText;
		QString                               StatusText;
		QString                               SeedError;
		QColor                                ErrorColor;
		application::source_search_projection Projection;
		void                                  schedule();
		void                                  adapt_font();
		void                                  fit_query_height();
		void                                  set_status(QString aWords, QString aShort, bool aError);
	};
}
#endif
