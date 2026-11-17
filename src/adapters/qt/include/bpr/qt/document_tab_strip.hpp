#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DOCUMENT_TAB_STRIP_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DOCUMENT_TAB_STRIP_HPP

#include "bpr/core/document_tabs.hpp"

#include <QString>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

class QLabel;
class QMenu;
class QResizeEvent;
class QScrollArea;
class QTabBar;
class QToolButton;

namespace bpr::qt {
	/// \brief [[BPR-1132]]. One document tab as the strip shows it.
	///
	/// \details The window supplies the words: the strip decides nothing and
	/// knows no path, pair or history of its own.
	struct document_tab_presentation
	{
		core::document_tab_id Id;
		/// The file's name, with as much of its path as tells it apart from
		/// the other tabs' files.
		QString Name;
		/// The whole path, for the tooltip and accessibility.
		QString Path;
		/// "Feb 4 → 9" on a tab with its own pair; empty on a tab that has the
		/// window's comparison.
		QString OwnPair;
		/// The tab's pair in words, "Feb 4 → Feb 9", for the tooltip.
		QString                       Pair;
		core::document_tab_view_state State         = core::document_tab_view_state::idle;
		bool                          Active        = false;
		bool                          OwnComparison = false;
		/// RFC-BPR-0128 ([[BPR-2166]]). An agent's report: the agent glyph,
		/// and Close alone in its menu.
		bool Report = false;

		auto operator==(const document_tab_presentation&) const -> bool = default;
	};

	/// \brief A tab gesture, as the core names it.
	using document_tab_dispatch = std::function<void(const core::document_tab_intent&)>;

	/// \brief [[BPR-1132]]. RFC-BPR-0052's comparison header: the document
	/// tabs.
	///
	/// \details One tab per exact view (a file, a comparison and a reading
	/// position), in the core's order. Names elide from the left, where file
	/// names differ; a tab with its own pair carries it in a pill. When the
	/// tabs do not fit at their readable minimum, they scroll sideways. Each
	/// tab's menu offers Duplicate Tab, Close Tab, Close Other Tabs, returning
	/// the tab to the window's comparison and making its pair the one new tabs
	/// take. The comparison's status (loading, a refusal, an approximate
	/// alignment) sits at the strip's right while it has something to say.
	/// Every gesture leaves as a core intent; what is shown is what the core
	/// published.
	class document_tab_strip final : public QWidget
	{
	public:
		/// \brief The design's row: 28 pixels.
		static constexpr int height_pixels     = 28;
		static constexpr int minimum_tab_width = 220;

		explicit document_tab_strip(QWidget* aParent = nullptr);

		void set_dispatch(document_tab_dispatch aDispatch);
		/// \brief [[BPR-1914]] v004. The narrowest a tab gets before the tabs
		/// scroll sideways; `minimum_tab_width` unless a narrower holder (the
		/// tour's side chats) sets its own.
		void               set_minimum_tab_width(int aWidth);
		[[nodiscard]] auto minimum_width_of_tab() const noexcept -> int { return MinimumTab; }
		/// \brief Shows the tabs the core published, in order.
		void present(std::vector<document_tab_presentation> aTabs);
		/// \brief The comparison's status; shown only while `aNotable`.
		void set_status(const QString& aText, bool aNotable);

		[[nodiscard]] auto tabs() const noexcept -> const std::vector<document_tab_presentation>&;
		[[nodiscard]] auto tab_bar() const noexcept -> QTabBar*;
		[[nodiscard]] auto scroll_area() const noexcept -> QScrollArea*;
		/// \brief "+": a new tab of the active tab's exact view.
		[[nodiscard]] auto new_tab_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto status_label() const noexcept -> QLabel*;
		/// \brief The tab the bar shows at `aIndex`.
		[[nodiscard]] auto tab_at(int aIndex) const -> std::optional<core::document_tab_id>;
		/// \brief The bar index of a shown tab.
		[[nodiscard]] auto index_of(core::document_tab_id aTab) const -> std::optional<int>;
		/// \brief A tab's menu, built as a right-click builds it; the caller
		/// owns it.
		[[nodiscard]] auto make_tab_menu(core::document_tab_id aTab, QWidget* aParent) -> QMenu*;
		/// \brief RFC-BPR-0117 ([[BPR-2076]]). What the holder adds to a tab's
		/// menu, after the strip's own items (Explain This File).
		void set_menu_extension(std::function<void(const document_tab_presentation& aTab, QMenu& aMenu)> aExtension) { MenuExtension = std::move(aExtension); }
		/// \brief RFC-BPR-0117. A control of the holder's at the strip's right,
		/// before the plus (Explain this file); the strip takes ownership.
		void add_trailing_control(QWidget* aControl);
		/// \brief Look ask-v005 (owner, 2026-11-14: "Filetab ask should be on
		/// the tab and smaller than the tab so it doesnt read like another tab.
		/// Should only be shown for the active tab. Shouldn't be shown for
		/// report card tabs."). A control of the holder's drawn on the active
		/// tab itself, while offered, never on a report's tab; the strip owns
		/// it.
		void set_active_tab_control(QWidget* aControl);
		void offer_active_tab_control(bool aOffered);

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		bool eventFilter(QObject*, QEvent*) override;

	private:
		void               rebuild();
		void               layout_tabs();
		void               repaint_glyphs();
		void               reveal_active();
		[[nodiscard]] auto active() const -> const document_tab_presentation*;
		void               send(const core::document_tab_intent& aIntent) const;
		void               open_tab_menu(const QPoint& aAt);

		document_tab_dispatch                                         Dispatch;
		std::function<void(const document_tab_presentation&, QMenu&)> MenuExtension;
		std::vector<document_tab_presentation>                        Tabs;
		/// The tabs the bar shows, in its order.
		std::vector<core::document_tab_id> Shown;
		QTabBar*                           Bar           = nullptr;
		QScrollArea*                       Scroll        = nullptr;
		QToolButton*                       NewTab        = nullptr;
		QLabel*                            Status        = nullptr;
		QWidget*                           ActiveControl = nullptr;
		bool                               ActiveOffered = false;
		bool                               Rebuilding    = false;
		int                                MinimumTab    = minimum_tab_width;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DOCUMENT_TAB_STRIP_HPP
