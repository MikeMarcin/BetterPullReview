#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/qt/conversation_cards_view.hpp"

#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <QWidget>

#include <cstdint>
#include <utility>
#include <vector>

class QAction;
class QComboBox;
class QLabel;
class QModelIndex;
class QSplitter;
class QStackedWidget;
class QToolButton;
class QTreeView;

namespace bpr::qt {
	class github_conversation_model;
	class github_thread_map;
	class markdown_body_view;
	class appearance_profile;

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
	/// A compact index selects one conversation. Its structural map selects
	/// an exact post in the single full-body detail. Discuss uses the complete
	/// PR independently of code anchors, with an optional local file scope.
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
		void               set_anchor_line(std::optional<core::source_line_ref> aLine);
		[[nodiscard]] auto has_semantic_anchor(const core::source_line_ref& aLine) const -> bool;
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
		/// \brief [[BPR-1132]]. How many review threads of one file are
		/// anchored on the pull request's current head, placed or not: what a
		/// tab over an older pair says are on later updates.
		[[nodiscard]] auto current_head_thread_count(const core::file_identity& aFile) const -> std::size_t;

		/// \brief [[BPR-0985]]. Whether the current row is the one the panel
		/// made current to show a detail, rather than one the reviewer chose.
		///
		/// \details A default row selects a detail without moving code, so the
		/// workbench reveals again only a thread the reviewer chose when a
		/// comparison or the conversations are published. Any change of the
		/// tree's current row that is heard, and `choose`, ends the default.
		[[nodiscard]] auto current_is_default() const noexcept -> bool;
		/// \brief Makes one thread current as the reviewer's choice.
		///
		/// \details Choosing the row that is current only by default is still
		/// a choice, so it is announced like any other.
		void choose(const QModelIndex& aIndex);

		/// \brief Compatibility no-op; accepted conversations reveal automatically.
		void set_reveal_action(QAction* aAction);

		/// \brief Re-lays the rows after a palette or font change.
		void relayout();
		/// \brief Complete selected public post; the tree and cards remain navigation summaries.
		[[nodiscard]] auto body_view() const noexcept -> markdown_body_view*;
		void               set_link_dispatch(std::function<void(const std::string&, const std::string&)> aDispatch);
		void               adopt_appearance(const appearance_profile& aProfile);

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
		/// \brief Presents the PR-wide index beside one selected detail, or as
		/// switchable index/detail pages at narrow widths.
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

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		/// \brief Puts the map and the drawer's tree on the same node.
		void synchronize_map_focus();
		/// \brief Shows the loaded page while the snapshot holds any
		/// conversation, and the map and detail while the anchor shows one.
		void show_loaded_page();
		void refresh_summary();
		void refresh_projection();
		void refresh_detail();
		void refresh_body(bool aAdmitted);
		void arrange_discuss();

		QLabel*                                                     AvailabilityLabel  = nullptr;
		QToolButton*                                                RevealButton       = nullptr;
		QStackedWidget*                                             Stack              = nullptr;
		QTreeView*                                                  View               = nullptr;
		QWidget*                                                    DetailPane         = nullptr;
		markdown_body_view*                                         Body               = nullptr;
		QToolButton*                                                SupportDetails     = nullptr;
		QLabel*                                                     SupportDescription = nullptr;
		std::string                                                 SupportPost;
		std::function<void(const std::string&, const std::string&)> LinkDispatch;
		github_conversation_model*                                  Model = nullptr;
		/// The Treeverse-style reply-structure map [[BPR-0418]] adds beside the
		/// thread itself. It is a second *view* of the drawer's own forest, not
		/// a second source of it, so it holds no selection authority: a click
		/// on the map moves the tree's current index and the tree's selection
		/// is what everything downstream still reads.
		github_thread_map*                           ThreadMap          = nullptr;
		conversation_cards_view*                     Cards              = nullptr;
		QToolButton*                                 ScopeThisFile      = nullptr;
		QToolButton*                                 ScopeWholePr       = nullptr;
		QComboBox*                                   DiscussFileChoice  = nullptr;
		QLabel*                                      ThreadMapSummary   = nullptr;
		QLabel*                                      ThreadMapBranch    = nullptr;
		QSplitter*                                   Loaded             = nullptr;
		QSplitter*                                   DiscussSplitter    = nullptr;
		QWidget*                                     DiscussColumn      = nullptr;
		QWidget*                                     CardsPane          = nullptr;
		QWidget*                                     MapPane            = nullptr;
		QToolButton*                                 FilterUnread       = nullptr;
		QToolButton*                                 FilterUnresolved   = nullptr;
		QLabel*                                      DiscussSummary     = nullptr;
		QLabel*                                      Notice             = nullptr;
		QToolButton*                                 BackToIndex        = nullptr;
		bool                                         DetailOpened       = false;
		bool                                         Discuss            = false;
		bool                                         HasThreads         = false;
		bool                                         DiscussInitialized = false;
		std::string                                  DetailRoot;
		bool                                         DetailDirty    = true;
		bool                                         DetailAdmitted = false;
		std::unordered_map<std::string, std::size_t> ShownPositions;
		std::unordered_set<std::string>              VisibleRoots;
		std::set<core::source_line_ref>              SemanticAnchors;
		std::string                                  DiscussSelection;
		int                                          DiscussReadingOffset = 0;
		cards_scope                                  DiscussScope         = cards_scope::whole_pull_request;
		discuss_filters                              DiscussFilters;
		/// Expanded once, on the first projection that has anything to expand.
		/// A reviewer who then collapses a thread keeps it collapsed across the
		/// next refresh, which re-expanding on every projection would undo.
		bool ExpandOnNextProjection = true;
		/// [[BPR-0985]]. The panel made the current row current to show a
		/// detail; the reviewer did not choose it.
		bool DefaultCurrent = false;
		/// The file whose threads are shown, or nothing for all of them.
		std::optional<core::file_identity>   AnchorFile;
		std::optional<core::file_identity>   DiscussFile;
		std::optional<core::source_line_ref> AnchorLine;
		bool                                 LeftHasCurrentDiffRevision  = true;
		bool                                 RightHasCurrentDiffRevision = true;
		/// Retained so a change of anchor file re-filters without waiting for
		/// the next projection. A reviewer moving between files expects the
		/// drawer to follow immediately, and the projection does not arrive
		/// again just because they scrolled.
		std::vector<application::github_conversation_projection> Retained;
		/// [[BPR-0673]]. The file identity of each retained node's path, index
		/// for index. Derived once per adopted forest: re-filtering a
		/// thousand-thread review hashed every path on every anchor change.
		std::vector<core::file_identity>                         RetainedFiles;
		std::vector<application::github_conversation_projection> Shown;
		/// [[BPR-0972]]. Per adopted forest: every node that has a root, by its
		/// root's file, so anchoring to a file visits that file's nodes only.
		std::vector<std::pair<core::file_identity, std::size_t>> RetainedByRootFile;
		std::vector<std::optional<std::size_t>>                  RetainedRoot;
		/// [[BPR-0972]]. The retained forest with the current endpoint
		/// placement, handed to the cards only when either changes.
		std::vector<application::github_conversation_projection> PlacedRetained;
		std::vector<application::github_conversation_projection> EarlierRetained;
		std::map<core::file_identity, std::size_t>               CurrentThreadCounts;
		bool                                                     PlacedStale            = true;
		std::uint64_t                                            ProjectionNodesVisited = 0U;

		void               index_retained();
		[[nodiscard]] auto anchored() -> std::vector<application::github_conversation_projection>;

	public:
		/// \brief [[BPR-0972]]. Retained nodes visited while re-filtering.
		[[nodiscard]] auto projection_nodes_visited() const noexcept -> std::uint64_t;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP
