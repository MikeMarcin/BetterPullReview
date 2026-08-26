#include "bpr/qt/github_thread_map.hpp"

#include <QAccessible>
#include <QColor>
#include <QEvent>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionFocusRect>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief Keeps a caller-supplied geometry inside drawable bounds.
		[[nodiscard]] auto sanitized(thread_map_configuration aConfiguration) noexcept -> thread_map_configuration
		{
			aConfiguration.RowPitch    = std::clamp(aConfiguration.RowPitch, 16, 160);
			aConfiguration.NodeHeight  = std::clamp(aConfiguration.NodeHeight, 12, 120);
			aConfiguration.NodePadding = std::clamp(aConfiguration.NodePadding, 2, 40);
			aConfiguration.SlotGap     = std::clamp(aConfiguration.SlotGap, 4, 120);
			aConfiguration.DepthIndent = std::clamp(aConfiguration.DepthIndent, 0, 80);
			aConfiguration.Margin      = std::clamp(aConfiguration.Margin, 0, 64);
			// A node has to fit inside its own row, or edges leave from below
			// the node beneath them and the drawing stops meaning anything.
			aConfiguration.NodeHeight
				= std::min(aConfiguration.NodeHeight, aConfiguration.RowPitch - 6);
			return aConfiguration;
		}

		/// \brief Names the file a thread is anchored in, without its directories.
		///
		/// \details The map is about reply structure, and a full repository path
		/// makes a root box wider than the tree below it. The complete path stays
		/// on the accessible name and in the drawer's own row, so nothing is lost
		/// - only the box is short.
		[[nodiscard]] auto anchor_label(
			const application::github_conversation_projection& aNode) -> QString
		{
			const auto separator = aNode.Path.find_last_of('/');
			const auto name      = separator == std::string::npos
				? aNode.Path
				: aNode.Path.substr(separator + 1U);
			auto       label     = QString::fromStdString(name);
			if (aNode.Line.has_value() && *aNode.Line != 0U) {
				label += QStringLiteral(":") + QString::number(*aNode.Line);
			}
			return label;
		}

		[[nodiscard]] auto node_label(
			const application::github_conversation_projection& aNode) -> QString
		{
			if (!aNode.AuthorLogin.empty()) {
				return QStringLiteral("@")
					+ QString::fromStdString(aNode.AuthorLogin);
			}
			if (!aNode.Path.empty()) {
				return anchor_label(aNode);
			}
			return QStringLiteral("Thread");
		}

		[[nodiscard]] auto accessible_label(
			const application::github_conversation_projection& aNode) -> QString
		{
			const auto who = aNode.AuthorLogin.empty()
				? QStringLiteral("Review thread")
				: QStringLiteral("Reply by @")
					+ QString::fromStdString(aNode.AuthorLogin);
			if (aNode.Path.empty()) {
				return who;
			}
			auto where = QStringLiteral(" in ")
				+ QString::fromStdString(aNode.Path);
			if (aNode.Line.has_value() && *aNode.Line != 0U) {
				where += QStringLiteral(" line ") + QString::number(*aNode.Line);
			}
			return who + where;
		}

		[[nodiscard]] auto with_alpha(QColor aColor, const int aAlpha) noexcept
			-> QColor
		{
			aColor.setAlpha(aAlpha);
			return aColor;
		}

		/// \brief States how one node is filled, outlined, and lettered.
		struct node_ink
		{
			QColor Fill;
			QColor Border;
			QColor Label;
			qreal  Stroke = 1.0;
		};

		/// \brief Chooses the pen one reply edge is drawn with.
		///
		/// \details Every colour comes from the live palette, so the map follows
		/// the application and operating-system appearance rather than carrying
		/// its own. Weight is what separates the three readings: the path the
		/// owner asked for is heaviest, the rest of the clicked branch is
		/// present but quieter, and everything else recedes.
		[[nodiscard]] auto edge_pen(
			const thread_map_relation aRelation,
			const QPalette&           aPalette) -> QPen
		{
			const auto highlight = aPalette.color(QPalette::Highlight);
			switch (aRelation) {
			case thread_map_relation::ancestor:
			case thread_map_relation::focused:
				return QPen{ highlight, 2.5 };
			case thread_map_relation::descendant:
				return QPen{ with_alpha(highlight, 150), 1.6 };
			case thread_map_relation::unrelated:
				break;
			}
			return QPen{
				with_alpha(aPalette.color(QPalette::Text), 96), 1.0
			};
		}

		[[nodiscard]] auto node_paint(
			const thread_map_relation aRelation,
			const QPalette&           aPalette) -> node_ink
		{
			const auto text      = aPalette.color(QPalette::Text);
			const auto highlight = aPalette.color(QPalette::Highlight);
			switch (aRelation) {
			case thread_map_relation::focused:
				return node_ink{
					.Fill   = highlight,
					.Border = text,
					.Label  = aPalette.color(QPalette::HighlightedText),
					.Stroke = 1.8,
				};
			case thread_map_relation::ancestor:
				return node_ink{
					.Fill   = with_alpha(highlight, 64),
					.Border = highlight,
					.Label  = text,
					.Stroke = 1.4,
				};
			case thread_map_relation::descendant:
				return node_ink{
					.Fill   = with_alpha(highlight, 30),
					.Border = with_alpha(highlight, 150),
					.Label  = text,
				};
			case thread_map_relation::unrelated:
				break;
			}
			return node_ink{
				.Fill   = aPalette.color(QPalette::Base),
				.Border = with_alpha(text, 120),
				.Label  = text,
			};
		}
	} // namespace

	github_thread_map::github_thread_map(
		QWidget* const           aParent,
		thread_map_configuration aConfiguration)
		: QAbstractScrollArea{ aParent }
		, Configuration{ sanitized(aConfiguration) }
	{
		setObjectName(QStringLiteral("githubThreadMap"));
		setAccessibleName(QStringLiteral("Review thread map"));
		setFocusPolicy(Qt::StrongFocus);
		setFrameShape(QFrame::NoFrame);
		update_accessible_status();
	}

	void github_thread_map::adopt(
		const std::vector<application::github_conversation_projection>&
			aConversations)
	{
		if (!Entries.empty() && Retained == aConversations) {
			++Statistics.SkippedRebuilds;
			return;
		}
		Retained = aConversations;
		Entries.clear();
		Roots.clear();
		Entries.reserve(aConversations.size());

		auto byIdentity = std::unordered_map<std::string, std::size_t>{ };
		byIdentity.reserve(aConversations.size());
		for (const auto& conversation : aConversations) {
			byIdentity.emplace(conversation.Id, Entries.size());
			Entries.push_back(entry{
				.Identity       = conversation.Id,
				.ParentIdentity = conversation.ParentId.value_or(std::string{ }),
				.Label          = node_label(conversation),
				.Accessible     = accessible_label(conversation),
				.IsThreadRoot   = conversation.Kind
					== application::github_conversation_kind::review_thread,
			});
		}

		// Resolved first, judged second, applied third. The judging pass writes
		// nothing, so the result cannot depend on the order it happens to visit
		// nodes in - and every member of a cycle is promoted, not just the one
		// the walk reached first. `github_conversation_model::apply` answers an
		// unexpected parent exactly this way, and the drawer's two views of one
		// forest have to agree on its shape or the map is lying about the tree.
		for (auto& node : Entries) {
			if (node.ParentIdentity.empty()) {
				continue;
			}
			const auto found = byIdentity.find(node.ParentIdentity);
			if (found != byIdentity.end()) {
				node.Parent = found->second;
			} else {
				// A parent naming a node that is not here. The node keeps its
				// recorded parent identity and becomes a root, which is a lost
				// level rather than a lost comment.
				++Statistics.PromotedNodes;
			}
		}
		auto promote = std::vector<bool>(Entries.size(), false);
		for (auto index = std::size_t{ 0U }; index < Entries.size(); ++index) {
			auto walk = Entries[index].Parent;
			auto step = std::size_t{ 0U };
			while (walk != npos && step <= Entries.size()) {
				walk = Entries[walk].Parent;
				++step;
			}
			promote[index] = walk != npos;
		}
		for (auto index = std::size_t{ 0U }; index < Entries.size(); ++index) {
			if (promote[index]) {
				Entries[index].Parent = npos;
				++Statistics.PromotedNodes;
			}
		}
		for (auto index = std::size_t{ 0U }; index < Entries.size(); ++index) {
			if (Entries[index].Parent == npos) {
				Roots.push_back(index);
			} else {
				Entries[Entries[index].Parent].Children.push_back(index);
			}
		}

		if (!FocusIdentity.empty() && index_of(FocusIdentity) == npos) {
			FocusIdentity.clear();
		}
		rebuild_layout();
	}

	void github_thread_map::clear()
	{
		Retained.clear();
		Entries.clear();
		Roots.clear();
		Nodes.clear();
		Edges.clear();
		FocusIdentity.clear();
		CommentCount         = 0U;
		ReplyDepth           = 0U;
		Content              = { };
		Statistics.LastNodes = 0U;
		Statistics.LastEdges = 0U;
		update_scroll_ranges();
		update_accessible_status();
		viewport()->update();
	}

	auto github_thread_map::depth_first_order() -> std::vector<std::size_t>
	{
		// Pre-order, so that within one depth every node of an earlier subtree
		// precedes every node of a later one. That single property is what
		// keeps the drawn edges from crossing.
		auto order = std::vector<std::size_t>{ };
		order.reserve(Entries.size());
		auto pending = std::vector<std::size_t>{ };
		for (auto root = Roots.rbegin(); root != Roots.rend(); ++root) {
			pending.push_back(*root);
		}
		while (!pending.empty()) {
			const auto index = pending.back();
			pending.pop_back();
			order.push_back(index);
			auto& node = Entries[index];
			node.Depth = node.Parent == npos
				? 0U
				: Entries[node.Parent].Depth + 1U;
			for (auto child = node.Children.rbegin();
				 child != node.Children.rend();
				 ++child) {
				pending.push_back(*child);
			}
		}
		return order;
	}

	void github_thread_map::place_nodes(const std::vector<std::size_t>& aOrder)
	{
		const auto metrics = QFontMetricsF{ font() };
		auto       taken   = std::vector<std::size_t>{ };
		// Where the next node of each depth starts. Each depth is indented one
		// step further than the one above it, which is what gives a reply chain
		// its readable diagonal.
		auto cursors = std::vector<qreal>{ };
		for (const auto index : aOrder) {
			const auto& node  = Entries[index];
			const auto  depth = node.Depth;
			if (depth >= taken.size()) {
				taken.resize(depth + 1U, 0U);
				cursors.resize(
					depth + 1U,
					static_cast<qreal>(Configuration.Margin)
						+ static_cast<qreal>(depth)
							* static_cast<qreal>(Configuration.DepthIndent));
			}
			// Sized to its own label rather than to a fixed box, so no author
			// name is ever elided into an ambiguous prefix.
			const auto width = metrics.horizontalAdvance(node.Label)
				+ 2.0 * static_cast<qreal>(Configuration.NodePadding);
			const auto bounds = QRectF{
				cursors[depth],
				static_cast<qreal>(Configuration.Margin)
					+ static_cast<qreal>(depth)
						* static_cast<qreal>(Configuration.RowPitch),
				width,
				static_cast<qreal>(Configuration.NodeHeight),
			};
			cursors[depth] = bounds.right()
				+ static_cast<qreal>(Configuration.SlotGap);
			Nodes[index] = thread_map_node{
				.Identity       = node.Identity,
				.ParentIdentity = node.Parent == npos
					? std::string{ }
					: Entries[node.Parent].Identity,
				.Label          = node.Label,
				.Bounds         = bounds,
				.Depth          = depth,
				.Slot           = taken[depth],
				.IsThreadRoot   = node.IsThreadRoot,
			};
			++taken[depth];
			if (!node.IsThreadRoot) {
				++CommentCount;
				// The thread root is not a reply, so the first comment below it
				// is depth zero and its own answer is depth one.
				ReplyDepth = std::max(ReplyDepth, depth == 0U ? 0U : depth - 1U);
			}
		}
	}

	void github_thread_map::build_edges()
	{
		Edges.reserve(Nodes.size());
		for (auto index = std::size_t{ 0U }; index < Entries.size(); ++index) {
			const auto parent = Entries[index].Parent;
			if (parent == npos) {
				continue;
			}
			const auto& node   = Nodes[index];
			const auto  start  = QPointF{ Nodes[parent].Bounds.center().x(),
										  Nodes[parent].Bounds.bottom() };
			const auto  finish = QPointF{ node.Bounds.center().x(),
										  node.Bounds.top() };
			const auto  middle = 0.5 * (start.y() + finish.y());
			auto        path   = QPainterPath{ start };
			path.cubicTo(
				QPointF{ start.x(), middle },
				QPointF{ finish.x(), middle },
				finish);
			Edges.push_back(thread_map_edge{
				.ParentIdentity = node.ParentIdentity,
				.ChildIdentity  = node.Identity,
				.Path           = std::move(path),
			});
		}
	}

	void github_thread_map::rebuild_layout()
	{
		Edges.clear();
		// Kept parallel to `Entries` on purpose: one index names the same node
		// in both, so every parent, child, and sibling link resolved during
		// intake stays usable for hit testing, keyboard movement, and reveal
		// without a second table to keep in step.
		Nodes.assign(Entries.size(), thread_map_node{ });
		CommentCount = 0U;
		ReplyDepth   = 0U;

		place_nodes(depth_first_order());
		build_edges();

		auto extent = QRectF{ };
		for (const auto& node : Nodes) {
			extent |= node.Bounds;
		}
		Content = extent.isEmpty()
			? QSize{ }
			: QSize{
				  static_cast<int>(std::ceil(extent.right()))
					  + Configuration.Margin,
				  static_cast<int>(std::ceil(extent.bottom()))
					  + Configuration.Margin,
			  };

		++Statistics.Layouts;
		Statistics.LastNodes = Nodes.size();
		Statistics.LastEdges = Edges.size();
		refresh_relations();
		update_scroll_ranges();
		update_accessible_status();
		viewport()->update();
	}

	void github_thread_map::refresh_relations()
	{
		for (auto& node : Nodes) {
			node.Relation = thread_map_relation::unrelated;
		}
		for (auto& edge : Edges) {
			edge.Relation = thread_map_relation::unrelated;
		}
		if (FocusIdentity.empty()) {
			return;
		}

		auto relationOf = std::unordered_map<std::string, thread_map_relation>{ };
		for (const auto& identity : branch_path()) {
			relationOf.emplace(identity, thread_map_relation::ancestor);
		}
		for (const auto& identity : branch_below()) {
			relationOf.insert_or_assign(
				identity, thread_map_relation::descendant);
		}
		relationOf.insert_or_assign(
			FocusIdentity, thread_map_relation::focused);

		for (auto& node : Nodes) {
			const auto found = relationOf.find(node.Identity);
			if (found != relationOf.end()) {
				node.Relation = found->second;
			}
		}
		// An edge belongs to the branch exactly when its child does: every
		// parent of a branch node is itself a branch node.
		for (auto& edge : Edges) {
			const auto found = relationOf.find(edge.ChildIdentity);
			if (found == relationOf.end()) {
				continue;
			}
			edge.Relation = found->second == thread_map_relation::descendant
				? thread_map_relation::descendant
				: thread_map_relation::ancestor;
		}
	}

	auto github_thread_map::set_focus_identity(
		const std::string_view aIdentity,
		const bool             aAnnounce) -> bool
	{
		auto wanted = index_of(aIdentity) == npos
			? std::string{ }
			: std::string{ aIdentity };
		if (wanted == FocusIdentity) {
			return false;
		}
		FocusIdentity = std::move(wanted);
		refresh_relations();
		update_accessible_status();
		if (aAnnounce && !FocusIdentity.empty()) {
			const auto self  = QPointer<github_thread_map>{ this };
			auto       event = QAccessibleAnnouncementEvent{
				this, accessibleDescription()
			};
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
			if (self == nullptr) {
				return true;
			}
		}
		reveal_branch();
		viewport()->update();
		return true;
	}

	auto github_thread_map::focus_identity() const noexcept
		-> const std::string&
	{
		return FocusIdentity;
	}

	auto github_thread_map::branch_path() const -> std::vector<std::string>
	{
		auto path  = std::vector<std::string>{ };
		auto index = index_of(FocusIdentity);
		for (auto step = std::size_t{ 0U };
			 index != npos && step <= Entries.size();
			 ++step) {
			path.push_back(Entries[index].Identity);
			index = Entries[index].Parent;
		}
		std::ranges::reverse(path);
		return path;
	}

	auto github_thread_map::branch_below() const -> std::vector<std::string>
	{
		auto       below = std::vector<std::string>{ };
		const auto focus = index_of(FocusIdentity);
		if (focus == npos) {
			return below;
		}
		auto pending = std::vector<std::size_t>{ };
		pending.push_back(focus);
		while (!pending.empty()) {
			const auto index = pending.back();
			pending.pop_back();
			for (const auto child : Entries[index].Children) {
				below.push_back(Entries[child].Identity);
				pending.push_back(child);
			}
		}
		return below;
	}

	auto github_thread_map::nodes() const noexcept
		-> std::span<const thread_map_node>
	{
		return Nodes;
	}

	auto github_thread_map::edges() const noexcept
		-> std::span<const thread_map_edge>
	{
		return Edges;
	}

	auto github_thread_map::node_for(const std::string_view aIdentity)
		const noexcept -> const thread_map_node*
	{
		const auto found = std::ranges::find(
			Nodes, aIdentity, &thread_map_node::Identity);
		return found == Nodes.end() ? nullptr : &*found;
	}

	auto github_thread_map::summary_text() const -> QString
	{
		if (Nodes.empty()) {
			return QStringLiteral("No conversation to map");
		}
		return QStringLiteral("%1 comments · depth %2")
			.arg(static_cast<qulonglong>(CommentCount))
			.arg(static_cast<qulonglong>(ReplyDepth));
	}

	auto github_thread_map::branch_text() const -> QString
	{
		if (FocusIdentity.empty()) {
			return QStringLiteral(
				"Select a comment to follow its branch from the root.");
		}
		auto labels = QStringList{ };
		for (const auto& identity : branch_path()) {
			const auto index = index_of(identity);
			if (index != npos) {
				labels.append(Entries[index].Label);
			}
		}
		const auto below = branch_below().size();
		auto       text  = QStringLiteral("Path to this reply: ")
			+ labels.join(QStringLiteral(" › "));
		if (below == 0U) {
			return text + QStringLiteral(" · nothing below it");
		}
		return text
			+ QStringLiteral(" · %1 %2 below it")
				  .arg(static_cast<qulonglong>(below))
				  .arg(below == 1U ? QStringLiteral("reply")
								   : QStringLiteral("replies"));
	}

	void github_thread_map::set_selection_sink(thread_map_selection_sink aSink)
	{
		SelectionSink = std::move(aSink);
	}

	auto github_thread_map::hit_test(const QPointF aPosition) const noexcept
		-> std::string
	{
		const auto point = aPosition + scroll_offset();
		const auto found = std::ranges::find_if(
			Nodes,
			[&point](const thread_map_node& aNode) noexcept {
				return aNode.Bounds.contains(point);
			});
		return found == Nodes.end() ? std::string{ } : found->Identity;
	}

	auto github_thread_map::statistics() const noexcept -> thread_map_statistics
	{
		return Statistics;
	}

	auto github_thread_map::sizeHint() const -> QSize
	{
		const auto rows = static_cast<int>(
			std::min<std::size_t>(ReplyDepth + 3U, 8U));
		return QSize{ 240, Configuration.Margin * 2 + rows * Configuration.RowPitch };
	}

	auto github_thread_map::minimumSizeHint() const -> QSize
	{
		// Four rows, not two. A map that can only ever show the node you are on
		// and its parent answers "where am I" with "here", which is the
		// question it exists to answer better than a list already did. Four is
		// the smallest band in which a path reads as a path.
		return QSize{ 120,
					  Configuration.Margin * 2
						  + minimum_visible_rows * Configuration.RowPitch };
	}

	auto github_thread_map::index_of(const std::string_view aIdentity)
		const noexcept -> std::size_t
	{
		if (aIdentity.empty()) {
			return npos;
		}
		const auto found = std::ranges::find(
			Entries, aIdentity, &entry::Identity);
		return found == Entries.end()
			? npos
			: static_cast<std::size_t>(found - Entries.begin());
	}

	auto github_thread_map::content_size() const noexcept -> QSize
	{
		return Content;
	}

	auto github_thread_map::scroll_offset() const noexcept -> QPointF
	{
		return QPointF{ static_cast<qreal>(horizontalScrollBar()->value()),
						static_cast<qreal>(verticalScrollBar()->value()) };
	}

	void github_thread_map::update_scroll_ranges()
	{
		const auto content = content_size();
		const auto visible = viewport()->size();
		horizontalScrollBar()->setRange(
			0, std::max(0, content.width() - visible.width()));
		horizontalScrollBar()->setPageStep(visible.width());
		horizontalScrollBar()->setSingleStep(Configuration.RowPitch);
		verticalScrollBar()->setRange(
			0, std::max(0, content.height() - visible.height()));
		verticalScrollBar()->setPageStep(visible.height());
		verticalScrollBar()->setSingleStep(Configuration.RowPitch);
	}

	void github_thread_map::update_accessible_status()
	{
		auto description = summary_text();
		if (!FocusIdentity.empty()) {
			const auto index = index_of(FocusIdentity);
			if (index != npos) {
				description += QStringLiteral(". ") + Entries[index].Accessible;
			}
		}
		description += QStringLiteral(". ") + branch_text();
		setAccessibleDescription(description);
	}

	void github_thread_map::reveal(const std::size_t aIndex)
	{
		if (aIndex >= Nodes.size()) {
			return;
		}
		const auto& bounds     = Nodes[aIndex].Bounds;
		const auto  visible    = viewport()->size();
		auto* const vertical   = verticalScrollBar();
		auto* const horizontal = horizontalScrollBar();
		if (bounds.top() < static_cast<qreal>(vertical->value())) {
			vertical->setValue(static_cast<int>(std::floor(bounds.top())));
		} else if (bounds.bottom()
				   > static_cast<qreal>(vertical->value() + visible.height())) {
			vertical->setValue(
				static_cast<int>(std::ceil(bounds.bottom()))
				- visible.height());
		}
		if (bounds.left() < static_cast<qreal>(horizontal->value())) {
			horizontal->setValue(static_cast<int>(std::floor(bounds.left())));
		} else if (bounds.right()
				   > static_cast<qreal>(horizontal->value() + visible.width())) {
			horizontal->setValue(
				static_cast<int>(std::ceil(bounds.right())) - visible.width());
		}
	}

	void github_thread_map::reveal_branch()
	{
		const auto focus = index_of(FocusIdentity);
		if (focus >= Nodes.size()) {
			return;
		}
		// The selected node is the one thing that must end up on screen. Beyond
		// that the owner asked for the path to it, so when the whole branch does
		// not fit the view keeps the ancestors above rather than the replies
		// below: "at least up to the node you selected" is which half to keep.
		auto branch = QRectF{ Nodes[focus].Bounds };
		for (const auto& identity : branch_path()) {
			const auto index = index_of(identity);
			if (index < Nodes.size()) {
				branch |= Nodes[index].Bounds;
			}
		}
		auto* const vertical = verticalScrollBar();
		const auto  height   = viewport()->height();
		const auto  wanted   = branch.height() <= static_cast<qreal>(height)
			? static_cast<int>(std::floor(branch.top()))
			: static_cast<int>(std::ceil(Nodes[focus].Bounds.bottom())) - height;
		vertical->setValue(
			std::clamp(wanted, vertical->minimum(), vertical->maximum()));
		reveal(focus);
	}

	void github_thread_map::choose(const std::size_t aIndex)
	{
		if (aIndex >= Nodes.size()) {
			return;
		}
		const auto identity = Nodes[aIndex].Identity;
		// Pressing the node already in focus is not a no-op. It re-reveals and
		// re-reports, so a reviewer who scrolled away and clicked back lands
		// where they clicked instead of nowhere.
		static_cast<void>(set_focus_identity(identity));
		reveal_branch();
		if (!SelectionSink) {
			return;
		}
		try {
			SelectionSink(identity);
			++Statistics.SelectionCallbacks;
		} catch (...) {
			// A presentation surface cannot decide what a failed observer
			// means, and dropping the map's own selection because someone
			// else threw would be worse than counting it.
			++Statistics.CallbackFailures;
		}
	}

	auto github_thread_map::neighbour(
		const std::size_t aIndex,
		const int         aKey) const noexcept -> std::size_t
	{
		if (aIndex >= Nodes.size()) {
			return Nodes.empty() ? npos : 0U;
		}
		const auto& node = Entries[aIndex];
		switch (aKey) {
		case Qt::Key_Up:
			return node.Parent;
		case Qt::Key_Down:
			return node.Children.empty() ? npos : node.Children.front();
		case Qt::Key_Left:
		case Qt::Key_Right: {
			const auto& siblings = node.Parent == npos
				? Roots
				: Entries[node.Parent].Children;
			const auto  found    = std::ranges::find(siblings, aIndex);
			if (found == siblings.end()) {
				return npos;
			}
			const auto position
				= static_cast<std::size_t>(found - siblings.begin());
			if (aKey == Qt::Key_Left) {
				return position == 0U ? npos : siblings[position - 1U];
			}
			return position + 1U == siblings.size()
				? npos
				: siblings[position + 1U];
		}
		case Qt::Key_Home:
			return Roots.empty() ? npos : Roots.front();
		default:
			return npos;
		}
	}

	void github_thread_map::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ viewport() };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.fillRect(viewport()->rect(), palette().color(QPalette::Base));
		painter.translate(-scroll_offset());

		painter.setBrush(Qt::NoBrush);
		for (const auto& edge : Edges) {
			painter.setPen(edge_pen(edge.Relation, palette()));
			painter.drawPath(edge.Path);
		}

		for (const auto& node : Nodes) {
			const auto ink = node_paint(node.Relation, palette());
			painter.setPen(QPen{ ink.Border, ink.Stroke });
			painter.setBrush(ink.Fill);
			painter.drawRoundedRect(node.Bounds, 4.0, 4.0);
			painter.setPen(ink.Label);
			painter.drawText(node.Bounds, Qt::AlignCenter, node.Label);
		}

		if (hasFocus()) {
			painter.resetTransform();
			auto option = QStyleOptionFocusRect{ };
			option.initFrom(this);
			option.rect = viewport()->rect().adjusted(1, 1, -1, -1);
			style()->drawPrimitive(
				QStyle::PE_FrameFocusRect, &option, &painter, this);
		}
		++Statistics.PaintEvents;
	}

	void github_thread_map::resizeEvent(QResizeEvent* const aEvent)
	{
		QAbstractScrollArea::resizeEvent(aEvent);
		update_scroll_ranges();
		// A band that changes height must not scroll the reviewer's own
		// selection out of sight. The drawer settles its layout after the
		// projection lands, so without this the first reveal is computed
		// against a viewport that no longer exists by the time anyone looks.
		reveal_branch();
	}

	void github_thread_map::changeEvent(QEvent* const aEvent)
	{
		QAbstractScrollArea::changeEvent(aEvent);
		if (aEvent == nullptr) {
			return;
		}
		// A font change moves every box, because a box is sized to its label.
		if (aEvent->type() == QEvent::FontChange
			|| aEvent->type() == QEvent::StyleChange) {
			rebuild_layout();
		} else if (aEvent->type() == QEvent::PaletteChange) {
			viewport()->update();
		}
	}

	void github_thread_map::mousePressEvent(QMouseEvent* const aEvent)
	{
		if (aEvent == nullptr || aEvent->button() != Qt::LeftButton) {
			QAbstractScrollArea::mousePressEvent(aEvent);
			return;
		}
		const auto identity = hit_test(aEvent->position());
		if (identity.empty()) {
			QAbstractScrollArea::mousePressEvent(aEvent);
			return;
		}
		setFocus(Qt::MouseFocusReason);
		choose(index_of(identity));
		aEvent->accept();
	}

	void github_thread_map::keyPressEvent(QKeyEvent* const aEvent)
	{
		if (aEvent == nullptr || Nodes.empty()) {
			QAbstractScrollArea::keyPressEvent(aEvent);
			return;
		}
		const auto current = index_of(FocusIdentity);
		const auto next    = current == npos
			? (aEvent->key() == Qt::Key_Home || aEvent->key() == Qt::Key_Down
				   ? (Roots.empty() ? npos : Roots.front())
				   : npos)
			: neighbour(current, aEvent->key());
		if (next == npos) {
			QAbstractScrollArea::keyPressEvent(aEvent);
			return;
		}
		choose(next);
		aEvent->accept();
	}
} // namespace bpr::qt
