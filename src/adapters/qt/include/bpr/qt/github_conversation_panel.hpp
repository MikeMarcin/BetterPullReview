#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/qt/conversation_cards_view.hpp"

#include <optional>

#include <QWidget>

#include <vector>

class QAction;
class QLabel;
class QSplitter;
class QStackedWidget;
class QToolButton;
class QTreeView;

namespace bpr::qt {
	class github_conversation_model;
	class github_thread_map;

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
	/// Since [[BPR-0418]] it presents that forest twice: as the anchored tree a
	/// reviewer reads, and above it as a drawn map of the same reply structure
	/// with the branch through the current node highlighted. The map is a
	/// presentation of the tree it sits above and never a second projection.
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

		/// \brief Restricts the drawer to one file's threads.
		///
		/// \details The reviewer usually wants the conversation about the code
		/// in front of them, not the review's entire discussion — a comment
		/// anchored to a line of the file being read, the way SmartBear
		/// Collaborator presents it. Set the file and the panel keeps only the
		/// threads anchored in it.
		///
		/// Matching is by `core::file_identity_of`, which
		/// `snapshot_workbench_observation` names as the only way to identify a
		/// file. A conversation carries a path and the workbench carries an
		/// identity, and deriving one from the other is a pure function, so this
		/// is a comparison rather than a table that could fall out of step.
		///
		/// Whole threads survive or none of one does. A reply carries no anchor
		/// of its own — only its root does — so filtering per node would keep a
		/// root and drop its replies, or worse keep a reply whose root is gone
		/// and present it as a top-level remark.
		///
		/// Absent means show everything, which is what the review-wide view
		/// needs.
		void set_anchor_file(std::optional<core::file_identity> aFile);
		/// \brief Restricts the drawer to the exact selected diff side and line.
		void set_anchor_line(std::optional<core::source_line_ref> aLine);
		/// \brief States which selected endpoints expose current PR-diff coordinates.
		///
		/// \details A GitHub current line is meaningful only against the current
		/// pull-request diff. Retained comparisons may contain the same file and
		/// numeric line at unrelated revisions, so an endpoint mismatch remains
		/// visible but is never treated as a placeable source anchor.
		void set_current_endpoint_placement(
			bool aLeftHasCurrentDiffRevision,
			bool aRightHasCurrentDiffRevision);

		[[nodiscard]] auto anchor_file() const noexcept
			-> const std::optional<core::file_identity>&;
		[[nodiscard]] auto anchor_line() const noexcept
			-> const std::optional<core::source_line_ref>&;

		/// \brief Binds the action that reveals the threads.
		///
		/// \details The action stays owned by the window, which adds it for its
		/// shortcut; this only presents the button for it.
		void set_reveal_action(QAction* aAction);

		/// \brief Re-lays the rows after a palette or font change.
		void relayout();

		[[nodiscard]] auto view() const noexcept -> QTreeView*;
		[[nodiscard]] auto thread_map() const noexcept -> github_thread_map*;
		[[nodiscard]] auto thread_map_summary() const noexcept -> QLabel*;
		[[nodiscard]] auto thread_map_branch() const noexcept -> QLabel*;
		[[nodiscard]] auto model() const noexcept -> github_conversation_model*;
		[[nodiscard]] auto availability_label() const noexcept -> QLabel*;
		[[nodiscard]] auto reveal_button() const noexcept -> QToolButton*;
		/// \brief [[BPR-0648]]. The threads as cards at their lines, above the map.
		[[nodiscard]] auto cards() const noexcept -> conversation_cards_view*;
		[[nodiscard]] auto scope_this_file_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto scope_whole_pull_request_button() const noexcept -> QToolButton*;
		/// \brief [[BPR-0649]]. Discuss: the tree and the map at the left, the
		/// feed at the right, the filters over the feed and a summary beneath.
		void               set_discuss(bool aDiscuss);
		[[nodiscard]] auto discuss() const noexcept -> bool;
		/// \brief [[BPR-0661]]. Sets the drawer's first filter as the preference names it.
		void               apply_first_filter(application::review_comments_filter aFilter);
		[[nodiscard]] auto filter_unread_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto filter_unresolved_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto discuss_summary() const noexcept -> QLabel*;
		/// \brief [[BPR-0657]]. Says what a thread gesture came to: a refusal
		/// named before any provider, or the provider's answer. Empty hides it.
		void               present_notice(const QString& aNotice);
		[[nodiscard]] auto notice_label() const noexcept -> QLabel*;
		[[nodiscard]] auto discuss_column() const noexcept -> QWidget*;

	private:
		/// \brief Puts the map and the drawer's tree on the same node.
		void synchronize_map_focus();
		void show_loaded_page();
		void refresh_summary();
		void refresh_projection();

		QLabel*                    AvailabilityLabel = nullptr;
		QToolButton*               RevealButton      = nullptr;
		QStackedWidget*            Stack             = nullptr;
		QTreeView*                 View              = nullptr;
		github_conversation_model* Model             = nullptr;
		/// The Treeverse-style reply-structure map [[BPR-0418]] adds beside the
		/// thread itself. It is a second *view* of the drawer's own forest, not
		/// a second source of it, so it holds no selection authority: a click
		/// on the map moves the tree's current index and the tree's selection
		/// is what everything downstream still reads.
		github_thread_map*       ThreadMap        = nullptr;
		conversation_cards_view* Cards            = nullptr;
		QToolButton*             ScopeThisFile    = nullptr;
		QToolButton*             ScopeWholePr     = nullptr;
		QLabel*                  ThreadMapSummary = nullptr;
		QLabel*                  ThreadMapBranch  = nullptr;
		QSplitter*               Loaded           = nullptr;
		QSplitter*               DiscussSplitter  = nullptr;
		QWidget*                 DiscussColumn    = nullptr;
		QWidget*                 CardsPane        = nullptr;
		QWidget*                 MapPane          = nullptr;
		QToolButton*             FilterUnread     = nullptr;
		QToolButton*             FilterUnresolved = nullptr;
		QLabel*                  DiscussSummary   = nullptr;
		QLabel*                  Notice           = nullptr;
		bool                     Discuss          = false;
		bool                     HasThreads       = false;
		/// Expanded once, on the first projection that has anything to expand.
		/// A reviewer who then collapses a thread keeps it collapsed across the
		/// next refresh, which re-expanding on every projection would undo.
		bool ExpandOnNextProjection = true;
		/// The file whose threads are shown, or nothing for all of them.
		std::optional<core::file_identity>   AnchorFile;
		std::optional<core::source_line_ref> AnchorLine;
		bool                                 LeftHasCurrentDiffRevision  = true;
		bool                                 RightHasCurrentDiffRevision = true;
		/// Retained so a change of anchor file re-filters without waiting for
		/// the next projection. A reviewer moving between files expects the
		/// drawer to follow immediately, and the projection does not arrive
		/// again just because they scrolled.
		std::vector<application::github_conversation_projection> Retained;
		std::vector<application::github_conversation_projection> Shown;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP
