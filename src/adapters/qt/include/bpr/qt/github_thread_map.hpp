#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_THREAD_MAP_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_THREAD_MAP_HPP

#include "bpr/application/github_projection.hpp"

#include <QAbstractScrollArea>
#include <QPainterPath>
#include <QRectF>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

namespace bpr::qt {
	/// \brief States how one mapped node stands to the node in focus.
	///
	/// \details The owner asked for "the full branch that you clicked on, or at
	/// least up to the node you selected", so the branch is drawn in two
	/// weights rather than one. `ancestor` is the root-to-focus path, which is
	/// the floor that was actually asked for; `descendant` is the subtree
	/// below the focus, which answers where the chain goes next without
	/// competing with the path for attention. `descendant` therefore covers
	/// sibling sub-branches under the focus, deliberately: all of it is
	/// downstream of the comment that was clicked.
	enum class thread_map_relation : std::uint8_t {
		unrelated,
		ancestor,
		focused,
		descendant,
	};

	/// \brief Describes one laid-out node of the mapped reply structure.
	struct thread_map_node
	{
		std::string         Identity;
		std::string         ParentIdentity;
		QString             Label;
		QRectF              Bounds;
		std::size_t         Depth        = 0U;
		std::size_t         Slot         = 0U;
		bool                IsThreadRoot = false;
		thread_map_relation Relation     = thread_map_relation::unrelated;
	};

	/// \brief Describes one painted parent-to-child reply edge.
	struct thread_map_edge
	{
		std::string         ParentIdentity;
		std::string         ChildIdentity;
		QPainterPath        Path;
		thread_map_relation Relation = thread_map_relation::unrelated;
	};

	/// \brief Records bounded layout, paint, and callback work.
	struct thread_map_statistics
	{
		std::uint64_t Layouts            = 0U;
		std::uint64_t PaintEvents        = 0U;
		std::uint64_t SelectionCallbacks = 0U;
		std::uint64_t CallbackFailures   = 0U;
		std::uint64_t PromotedNodes      = 0U;
		std::uint64_t SkippedRebuilds    = 0U;
		std::size_t   LastNodes          = 0U;
		std::size_t   LastEdges          = 0U;
	};

	/// \brief Fixes the map's logical geometry before device scaling.
	struct thread_map_configuration
	{
		int RowPitch    = 30;
		int NodeHeight  = 20;
		int NodePadding = 7;
		int SlotGap     = 14;
		int DepthIndent = 10;
		int Margin      = 8;
	};

	using thread_map_selection_sink = std::function<void(const std::string&)>;

	/// \brief Maps one conversation's reply structure as a drawn tree.
	///
	/// \details The second view [[BPR-0211]] chose and [[BPR-0418]] builds. It
	/// reads the same `github_conversation_projection` forest the drawer's tree
	/// already shows and adds nothing to it: no second fetch, no second parent
	/// rule, and no state the projection does not carry. In particular the
	/// accepted mockup legends a per-node read/unread distinction that
	/// **nothing in this product carries** — the review ledger's unread notion
	/// is a per-conversation cursor, not a per-comment flag — so that legend is
	/// deliberately absent here rather than approximated.
	///
	/// The shape is a forest of trees: `ParentId` is single and optional. A
	/// parent naming a node that is not present, and a parent chain that never
	/// terminates, are both answered the way `github_conversation_model`
	/// answers them — the node is promoted to a root. An unexpected shape costs
	/// nesting, never a row.
	///
	/// Layout is level order with slots assigned in pre-order, which is what
	/// makes the drawing readable: within one depth every node of an earlier
	/// subtree precedes every node of a later one, so no two edges can cross.
	class github_thread_map final : public QAbstractScrollArea
	{
	public:
		explicit github_thread_map(
			QWidget*                 aParent        = nullptr,
			thread_map_configuration aConfiguration = { });

		github_thread_map(const github_thread_map&)                    = delete;
		auto operator=(const github_thread_map&) -> github_thread_map& = delete;
		github_thread_map(github_thread_map&&)                         = delete;
		auto operator=(github_thread_map&&) -> github_thread_map&      = delete;

		/// \brief Replaces the mapped forest.
		///
		/// \details The focused identity survives when it is still present,
		/// because a refresh is not the reviewer moving. A projection equal to
		/// the one already mapped is skipped outright: the drawer re-adopts on
		/// every anchor change, and relaying a forest that did not change would
		/// make an unchanged refresh cost the whole map.
		void adopt(
			const std::vector<application::github_conversation_projection>&
				aConversations);
		void clear();

		/// \brief Puts one node in focus, by identity.
		///
		/// \details An identity the map does not hold clears the focus rather
		/// than keeping a stale one. Returns whether the focus changed.
		///
		/// \param aAnnounce whether to announce the new focus. A composed
		/// parent following its own tree's selection passes `false`, because
		/// that tree has already announced the same move and two announcements
		/// for one gesture is worse than none.
		auto set_focus_identity(std::string_view aIdentity, bool aAnnounce = true)
			-> bool;
		[[nodiscard]] auto focus_identity() const noexcept -> const std::string&;

		/// \brief Returns the root-to-focus path, root first and focus last.
		[[nodiscard]] auto branch_path() const -> std::vector<std::string>;
		/// \brief Returns every identity below the focus on its own branch.
		[[nodiscard]] auto branch_below() const -> std::vector<std::string>;

		[[nodiscard]] auto nodes() const noexcept
			-> std::span<const thread_map_node>;
		[[nodiscard]] auto edges() const noexcept
			-> std::span<const thread_map_edge>;
		[[nodiscard]] auto node_for(std::string_view aIdentity) const noexcept
			-> const thread_map_node*;

		/// \brief Returns "N comments · depth D" for the mapped forest.
		///
		/// \details Depth counts reply edges, so a thread whose replies all
		/// answer its first comment is depth one. The thread root itself is not
		/// a comment and is not counted as one.
		[[nodiscard]] auto summary_text() const -> QString;
		/// \brief Returns the path to the focused reply and what follows it.
		[[nodiscard]] auto branch_text() const -> QString;

		void               set_selection_sink(thread_map_selection_sink aSink);
		[[nodiscard]] auto hit_test(QPointF aPosition) const noexcept
			-> std::string;
		[[nodiscard]] auto statistics() const noexcept -> thread_map_statistics;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;

	private:
		static constexpr auto npos = static_cast<std::size_t>(-1);
		/// The band the map refuses to shrink below, in whole node rows.
		static constexpr auto minimum_visible_rows = 4;

		/// \brief One node before layout, with its resolved place in the forest.
		struct entry
		{
			std::string              Identity;
			std::string              ParentIdentity;
			QString                  Label;
			QString                  Accessible;
			std::size_t              Parent = npos;
			std::vector<std::size_t> Children;
			std::size_t              Depth        = 0U;
			bool                     IsThreadRoot = false;
		};

		void rebuild_layout();
		/// \brief Orders the forest depth first and records each node's depth.
		[[nodiscard]] auto depth_first_order() -> std::vector<std::size_t>;
		/// \brief Places every node in its depth's next free slot.
		void               place_nodes(const std::vector<std::size_t>& aOrder);
		void               build_edges();
		void               refresh_relations();
		void               update_scroll_ranges();
		void               update_accessible_status();
		[[nodiscard]] auto index_of(std::string_view aIdentity) const noexcept
			-> std::size_t;
		[[nodiscard]] auto content_size() const noexcept -> QSize;
		[[nodiscard]] auto scroll_offset() const noexcept -> QPointF;
		void               choose(std::size_t aIndex);
		void               reveal(std::size_t aIndex);
		/// \brief Scrolls so the focused node and as much of its path as fits show.
		void               reveal_branch();
		[[nodiscard]] auto neighbour(std::size_t aIndex, int aKey) const noexcept
			-> std::size_t;

		thread_map_configuration                                 Configuration;
		std::vector<entry>                                       Entries;
		std::vector<std::size_t>                                 Roots;
		std::vector<thread_map_node>                             Nodes;
		std::vector<thread_map_edge>                             Edges;
		std::vector<application::github_conversation_projection> Retained;
		std::string                                              FocusIdentity;
		std::size_t                                              CommentCount = 0U;
		std::size_t                                              ReplyDepth   = 0U;
		QSize                                                    Content;
		thread_map_selection_sink                                SelectionSink;
		thread_map_statistics                                    Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_THREAD_MAP_HPP
