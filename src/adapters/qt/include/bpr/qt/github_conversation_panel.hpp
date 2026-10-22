#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CONVERSATION_PANEL_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>

#include <QPixmap>
#include <QWidget>

#include <cstdint>
#include <utility>
#include <vector>

class QAction;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QModelIndex;
class QStackedWidget;
class QToolButton;
class QTreeView;

namespace bpr::qt {
	class discuss_flow_layout;
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
		/// \brief [[BPR-1566]]. Where a reply's label menu sends About Conventional Comments.
		void set_reference_dispatch(std::function<void(core::reference_page)> aDispatch);
		/// \brief RFC-BPR-0084. Image requests and answers for the open thread's posts.
		void set_image_dispatch(std::function<void(const std::string&, std::vector<core::markdown_image_reference>)> aDispatch);
		void apply_markdown_image(const application::markdown_image_projection& aImage);
		void request_images();
		/// \brief RFC-BPR-0084 Q3. Shows or hides images in the conversation's posts.
		void set_images_shown(bool aShown);
		/// \brief [[BPR-1562]]. Code classification for the open thread's posts.
		void set_code_syntax_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch);
		void apply_markdown_code_syntax(const application::markdown_code_syntax_publication& aSpans);
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
		void set_discuss(bool aDiscuss);
		/// \brief [[BPR-1337]]. Whether the availability line says only how many
		/// review threads there are. Discuss's counts then say it, and the line
		/// is shown only when it says something else. Off until told.
		void               set_availability_ordinary(bool aOrdinary);
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
		void present_notice(const QString& aNotice);
		/// \brief [[BPR-PENDING-AS]]. The same, with a button after the words
		/// that does what the refusal needs: one click instead of an
		/// instruction. Another notice, or an empty one, takes it away.
		void               present_notice(const QString& aNotice, const QString& aAction, std::function<void()> aDispatch);
		[[nodiscard]] auto notice_label() const noexcept -> QLabel*;
		[[nodiscard]] auto notice_action() const noexcept -> QToolButton*;
		/// \brief Discuss's scope, file and filter row.
		[[nodiscard]] auto discuss_controls() const noexcept -> QWidget*;
		/// \brief [[BPR-1089]]. Lists exactly these conversations, the ones an
		/// Overview count counted, under a caption that says which count. The
		/// whole pull request is in scope and the other filters are off, so
		/// nothing narrows the list further. Listing again replaces it; the
		/// current thread is kept while it is listed.
		void list_conversations(std::vector<std::string> aConversations, const QString& aCaption, const QString& aDescription, const QPixmap& aMark = { });
		/// \brief Shows every conversation again.
		void               clear_listing();
		[[nodiscard]] auto listing() const noexcept -> const std::optional<std::set<std::string>>&;
		[[nodiscard]] auto listing_caption() const noexcept -> QLabel*;
		[[nodiscard]] auto clear_listing_button() const noexcept -> QToolButton*;
		/// \brief Told when the reviewer clears the listing.
		void set_listing_cleared(std::function<void()> aCleared);
		/// \brief [[BPR-1356]]. Discuss's own scope and filters, as they are
		/// while Discuss shows and as it will restore them while it does not.
		struct discuss_view_state
		{
			cards_scope                          Scope = cards_scope::whole_pull_request;
			std::optional<core::file_identity>   File;
			bool                                 OnlyNew      = false;
			bool                                 HideResolved = false;
			std::optional<std::set<std::string>> Listing;
		};
		[[nodiscard]] auto discuss_view() const -> discuss_view_state;
		/// \brief [[BPR-1356]]. While Discuss shows: the whole pull request
		/// for `aScope`, Only new and Hide resolved off, the listing cleared;
		/// each only when asked.
		void clear_discuss_filters(bool aScope, bool aOnlyNew, bool aHideResolved, bool aListing);
		/// \brief [[BPR-1356]]. Told when the reviewer changes a Discuss scope
		/// or filter by hand.
		void set_discuss_filters_changed(std::function<void()> aChanged);

	private:
		/// \brief Tells the notes which thread the selection authority is on.
		void synchronize_current();
		void show_loaded_page();
		void refresh_summary();
		void refresh_projection();
		void arrange_filters();

		QLabel*                    AvailabilityLabel = nullptr;
		QToolButton*               RevealButton      = nullptr;
		QStackedWidget*            Stack             = nullptr;
		QTreeView*                 View              = nullptr;
		github_conversation_model* Model             = nullptr;
		conversation_cards_view*   Cards             = nullptr;
		QWidget*                   DiscussControls   = nullptr;
		/// [[BPR-1337]]. The band's wrapping row, the filters' place in it and
		/// Clear filters.
		discuss_flow_layout* DiscussFlow          = nullptr;
		QWidget*             DiscussFilterBox     = nullptr;
		QHBoxLayout*         DiscussFiltersRow    = nullptr;
		QToolButton*         ClearFilters         = nullptr;
		bool                 AvailabilityOrdinary = false;
		QWidget*             ReadTools            = nullptr;
		QHBoxLayout*         ReadToolsRow         = nullptr;
		QToolButton*         ScopeThisFile        = nullptr;
		QToolButton*         ScopeWholePr         = nullptr;
		QComboBox*           DiscussFileChoice    = nullptr;
		QToolButton*         FilterUnread         = nullptr;
		QToolButton*         FilterUnresolved     = nullptr;
		QLabel*              DiscussSummary       = nullptr;
		/// [[BPR-1089]]. The drill-down's caption and its clear button.
		QWidget*                             ListingRow     = nullptr;
		QLabel*                              ListingMark    = nullptr;
		QLabel*                              ListingCaption = nullptr;
		QToolButton*                         ClearListing   = nullptr;
		std::optional<std::set<std::string>> Listing;
		std::function<void()>                ListingCleared;
		std::function<void()>                DiscussFiltersChanged;
		void                                 apply_filters();
		QWidget*                             NoticeRow    = nullptr;
		QLabel*                              Notice       = nullptr;
		QToolButton*                         NoticeAction = nullptr;
		std::function<void()>                NoticeDispatch;
		bool                                 Discuss            = false;
		bool                                 HasThreads         = false;
		bool                                 DiscussInitialized = false;
		std::set<core::source_line_ref>      SemanticAnchors;
		std::string                          DiscussSelection;
		cards_scope                          DiscussScope = cards_scope::whole_pull_request;
		discuss_filters                      DiscussFilters;
		discuss_filters                      ReadFilters;
		/// [[BPR-0985]]. The panel made the current row current by itself;
		/// the reviewer did not choose it.
		bool DefaultCurrent = false;
		/// [[BPR-1682]]. A snapshot is adopted, so the notes' page is
		/// the one to show whenever it has a note, my draft included.
		bool Adopted = false;
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
