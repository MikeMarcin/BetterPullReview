#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <map>
#include <optional>
#include <set>

#include <QWidget>

#include <cstdint>
#include <utility>
#include <vector>

class QAction;
class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QModelIndex;
class QStackedWidget;
class QToolButton;
class QTreeView;

namespace bpr::qt {
	class github_conversation_model;
	class appearance_profile;

	/// \brief Presents the review conversation, on its own.
	///
	/// \details Extracted from `github_window` so the conversation can be
	/// composed somewhere other than that window. BPR-0191 puts it in the
	/// workbench shell's conversations drawer, which RFC-BPR-0005 says is where
	/// comments belong.
	///
	/// The shape it presents is a **forest of trees**, not a general graph.
	/// `github_conversation_projection` carries a single optional parent, so no
	/// node has two parents and no two edges can cross.
	///
	/// [[BPR-1290]]. The drawer is inline only (owner verdict on the inline
	/// drawer prototype, 2026-10-06: "I like it"): the notes beside the code
	/// are everything it shows, and each thread opens in place. The thread map,
	/// the path caption, the post list and the body pane are gone from it.
	/// Selection still has one owner: the conversation model's current index,
	/// held by a tree that is never shown, which the code reveal, the anchor
	/// marker, the reply target and the defect surface read. Discuss lists the
	/// same notes in file order with its scope and filters.
	///
	/// It owns no wording about GitHub capability. The availability line is
	/// composed by whoever knows whether review threads were retrievable, which
	/// is RFC-BPR-0002's business, and handed in.
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
		/// \details Matching is by `core::file_identity_of`, which
		/// `snapshot_workbench_observation` names as the only way to identify a
		/// file. Whole threads survive or none of one does: a reply carries no
		/// anchor of its own, only its root does. Absent means show everything.
		void set_anchor_file(std::optional<core::file_identity> aFile);
		/// \brief Restricts the selection authority to the exact selected diff
		/// side and line; the notes keep showing the whole file.
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
		/// anchored on the pull request's current head, placed or not.
		[[nodiscard]] auto current_head_thread_count(const core::file_identity& aFile) const -> std::size_t;

		/// \brief [[BPR-0985]]. Whether the current row is the one the panel
		/// made current by itself, rather than one the reviewer chose.
		[[nodiscard]] auto current_is_default() const noexcept -> bool;
		/// \brief Makes one thread current as the reviewer's choice.
		void choose(const QModelIndex& aIndex);

		/// \brief Compatibility no-op; accepted conversations reveal automatically.
		void set_reveal_action(QAction* aAction);

		/// \brief Re-lays the notes after a palette or font change.
		void relayout();
		/// \brief Link destinations in a post, with the post's identity.
		void set_link_dispatch(std::function<void(const std::string&, const std::string&)> aDispatch);
		void adopt_appearance(const appearance_profile& aProfile);
		/// \brief [[BPR-1083]]. The label tints and mark style for the notes.
		void set_label_marks(const feedback_mark_theme& aTheme);

		/// \brief The selection authority: the conversation model's current
		/// index. Never shown ([[BPR-1290]]).
		[[nodiscard]] auto view() const noexcept -> QTreeView*;
		[[nodiscard]] auto model() const noexcept -> github_conversation_model*;
		[[nodiscard]] auto availability_label() const noexcept -> QLabel*;
		[[nodiscard]] auto reveal_button() const noexcept -> QToolButton*;
		/// \brief [[BPR-0648]], [[BPR-1290]]. The threads as notes beside the code.
		[[nodiscard]] auto cards() const noexcept -> conversation_cards_view*;
		[[nodiscard]] auto scope_this_file_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto scope_whole_pull_request_button() const noexcept -> QToolButton*;
		/// \brief Discuss: every thread in file order, with its scope and filters.
		void               set_discuss(bool aDiscuss);
		[[nodiscard]] auto discuss() const noexcept -> bool;
		/// \brief [[BPR-0661]]. Sets the drawer's first filter as the preference names it.
		void apply_first_filter(application::review_comments_filter aFilter);
		/// \brief Only new.
		[[nodiscard]] auto filter_unread_button() const noexcept -> QToolButton*;
		/// \brief Hide resolved.
		[[nodiscard]] auto filter_unresolved_button() const noexcept -> QToolButton*;
		/// \brief [[BPR-1290]]. The filters as the tab row shows them beside
		/// the code: two icons. Discuss borrows the buttons for its own row.
		[[nodiscard]] auto read_tools() const noexcept -> QWidget*;
		[[nodiscard]] auto discuss_summary() const noexcept -> QLabel*;
		/// \brief [[BPR-0657]]. Says what a thread gesture came to: a refusal
		/// named before any provider, or the provider's answer. Empty hides it.
		void               present_notice(const QString& aNotice);
		[[nodiscard]] auto notice_label() const noexcept -> QLabel*;
		/// \brief Discuss's scope, file and filter row.
		[[nodiscard]] auto discuss_controls() const noexcept -> QWidget*;

	private:
		/// \brief Tells the notes which thread the selection authority is on.
		void synchronize_current();
		void show_loaded_page();
		void refresh_summary();
		void refresh_projection();
		void arrange_filters();

		QLabel*                         AvailabilityLabel  = nullptr;
		QToolButton*                    RevealButton       = nullptr;
		QStackedWidget*                 Stack              = nullptr;
		QTreeView*                      View               = nullptr;
		github_conversation_model*      Model              = nullptr;
		conversation_cards_view*        Cards              = nullptr;
		QWidget*                        DiscussControls    = nullptr;
		QGridLayout*                    DiscussGrid        = nullptr;
		QWidget*                        ReadTools          = nullptr;
		QHBoxLayout*                    ReadToolsRow       = nullptr;
		QToolButton*                    ScopeThisFile      = nullptr;
		QToolButton*                    ScopeWholePr       = nullptr;
		QComboBox*                      DiscussFileChoice  = nullptr;
		QToolButton*                    FilterUnread       = nullptr;
		QToolButton*                    FilterUnresolved   = nullptr;
		QLabel*                         DiscussSummary     = nullptr;
		QLabel*                         Notice             = nullptr;
		bool                            Discuss            = false;
		bool                            HasThreads         = false;
		bool                            DiscussInitialized = false;
		std::set<core::source_line_ref> SemanticAnchors;
		std::string                     DiscussSelection;
		cards_scope                     DiscussScope = cards_scope::whole_pull_request;
		discuss_filters                 DiscussFilters;
		discuss_filters                 ReadFilters;
		/// [[BPR-0985]]. The panel made the current row current by itself;
		/// the reviewer did not choose it.
		bool DefaultCurrent = false;
		/// The file whose threads are shown, or nothing for all of them.
		std::optional<core::file_identity>   AnchorFile;
		std::optional<core::file_identity>   DiscussFile;
		std::optional<core::source_line_ref> AnchorLine;
		bool                                 LeftHasCurrentDiffRevision  = true;
		bool                                 RightHasCurrentDiffRevision = true;
		/// Retained so a change of anchor file re-filters without waiting for
		/// the next projection.
		std::vector<application::github_conversation_projection> Retained;
		/// [[BPR-0673]]. The file identity of each retained node's path, index
		/// for index. Derived once per adopted forest.
		std::vector<core::file_identity>                         RetainedFiles;
		std::vector<application::github_conversation_projection> Shown;
		/// [[BPR-0972]]. Per adopted forest: every node that has a root, by its
		/// root's file, so anchoring to a file visits that file's nodes only.
		std::vector<std::pair<core::file_identity, std::size_t>> RetainedByRootFile;
		std::vector<std::optional<std::size_t>>                  RetainedRoot;
		/// [[BPR-0972]]. The retained forest with the current endpoint
		/// placement, handed to the notes only when either changes.
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
