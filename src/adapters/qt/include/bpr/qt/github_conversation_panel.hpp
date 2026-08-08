#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP

#include "bpr/application/github_projection.hpp"

#include <QWidget>

#include <vector>

class QAction;
class QLabel;
class QStackedWidget;
class QToolButton;
class QTreeView;

namespace bpr::qt {
	class github_conversation_model;

	/// \brief Presents the threaded review conversation, on its own.
	///
	/// \details Extracted from `github_window` so the conversation can be
	/// composed somewhere other than that window. BPR-0191 puts it in the
	/// workbench shell's conversations drawer, which RFC-BPR-0005 says is where
	/// comments belong; the shared `workbench_drawer_panel` could not host it,
	/// because that panel is a list over `QAbstractListModel` and this is a
	/// thread.
	///
	/// The shape it presents is a **forest of trees**, not a general graph.
	/// `github_conversation_projection` carries a single optional parent, so no
	/// node has two parents and no two edges can cross. Anything built on top of
	/// this — lanes, filtering, a graph-style rendering — should take that as
	/// given rather than handling merges that cannot occur.
	///
	/// It owns no wording about GitHub capability. The availability line is
	/// composed by whoever knows whether review threads were retrievable, which
	/// is RFC-BPR-0002's business, and handed in. That keeps this type about
	/// presenting a thread forest and nothing else.
	class github_conversation_panel final : public QWidget
	{
	public:
		explicit github_conversation_panel(QWidget* aParent = nullptr);

		github_conversation_panel(const github_conversation_panel&) = delete;
		auto operator=(const github_conversation_panel&)
			-> github_conversation_panel&                      = delete;
		github_conversation_panel(github_conversation_panel&&) = delete;
		auto operator=(github_conversation_panel&&)
			-> github_conversation_panel& = delete;

		/// \brief Shows one projection's conversations.
		///
		/// \details The current selection is preserved across the update by
		/// identity rather than by row, because a projection can insert a reply
		/// above the selected node and a row-indexed restore would silently move
		/// the reviewer somewhere else.
		void adopt(
			const std::vector<application::github_conversation_projection>&
					aConversations,
			QString aAvailabilityText);

		/// \brief Returns to the empty state, keeping the availability wording.
		void clear(QString aAvailabilityText);

		/// \brief Binds the action that reveals the threads.
		///
		/// \details The action stays owned by the window, which adds it for its
		/// shortcut; this only presents the button for it.
		void set_reveal_action(QAction* aAction);

		/// \brief Re-lays the rows after a palette or font change.
		void relayout();

		[[nodiscard]] auto view() const noexcept -> QTreeView*;
		[[nodiscard]] auto model() const noexcept -> github_conversation_model*;
		[[nodiscard]] auto availability_label() const noexcept -> QLabel*;
		[[nodiscard]] auto reveal_button() const noexcept -> QToolButton*;

	private:
		QLabel*                    AvailabilityLabel = nullptr;
		QToolButton*               RevealButton      = nullptr;
		QStackedWidget*            Stack             = nullptr;
		QTreeView*                 View              = nullptr;
		github_conversation_model* Model             = nullptr;
		/// Expanded once, on the first projection that has anything to expand.
		/// A reviewer who then collapses a thread keeps it collapsed across the
		/// next refresh, which re-expanding on every projection would undo.
		bool ExpandOnNextProjection = true;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP
