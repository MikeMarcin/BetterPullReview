#include "bpr/qt/github_thread_map.hpp"

#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QTreeView>

#include <glaze/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using conversation = application::github_conversation_projection;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void process_events()
	{
		for (auto pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	struct fixture_json_options : glz::opts
	{
		bool validate_skipped             = true;
		bool validate_trailing_whitespace = true;
		bool escape_control_characters    = true;
		constexpr fixture_json_options()
		{
			null_terminated       = false;
			error_on_missing_keys = true;
			error_on_unknown_keys = false;
		}
	};
	struct thread_node_json
	{
		std::string                  Id;
		std::optional<std::string>   ParentId;
		std::string                  Kind;
		std::string                  AuthorLogin;
		std::string                  Body;
		std::string                  Path;
		std::optional<std::uint32_t> Line;
		struct glaze
		{
			static constexpr auto value = glz::object("Id", &thread_node_json::Id, "ParentId", &thread_node_json::ParentId, "Kind", &thread_node_json::Kind, "AuthorLogin", &thread_node_json::AuthorLogin, "Body", &thread_node_json::Body, "Path", &thread_node_json::Path, "Line", &thread_node_json::Line);
		};
	};
	struct thread_case_json
	{
		std::string                   Id;
		std::vector<thread_node_json> Projection;
		struct glaze
		{
			static constexpr auto value = glz::object("id", &thread_case_json::Id, "projection", &thread_case_json::Projection);
		};
	};
	struct thread_provenance_json
	{
		bool RecordedEvidence;
		struct glaze
		{
			static constexpr auto value = glz::object("recorded_evidence", &thread_provenance_json::RecordedEvidence);
		};
	};
	struct thread_fixture_json
	{
		thread_provenance_json        Provenance;
		std::vector<thread_case_json> Cases;
		struct glaze
		{
			static constexpr auto value = glz::object("provenance", &thread_fixture_json::Provenance, "cases", &thread_fixture_json::Cases);
		};
	};

	/// \brief Reads one labelled case out of the BPR-0417 constructed fixture.
	///
	/// \details The fixture is the only conversation in this repository with
	/// real reply depth, and it says of itself that it is constructed rather
	/// than recorded. Reading it here rather than restating its nodes means a
	/// change to its topology reaches this test instead of silently passing it.
	[[nodiscard]] auto fixture_case(const std::string_view aCase)
		-> std::vector<conversation>
	{
		auto file = QFile{ QStringLiteral(BPR_DEEP_THREAD_FIXTURE) };
		expect(file.open(QIODevice::ReadOnly),
			   "the constructed deep-thread fixture opens");
		const auto bytes = file.readAll();
		// [[BPR-1262]]. Value-initialized: GCC 15 at -O3 could not see glaze
		// fill every field. A missing key still fails the read.
		thread_fixture_json fixture{ };
		expect(!glz::read<fixture_json_options{ }>(fixture, std::string_view{ bytes.constData(), static_cast<std::size_t>(bytes.size()) }), "the fixture has its required typed JSON fields");
		expect(!fixture.Provenance.RecordedEvidence, "the fixture still declares itself constructed, not recorded");
		auto found = std::vector<conversation>{ };
		for (const auto& entry : fixture.Cases) {
			if (entry.Id != aCase) {
				continue;
			}
			for (const auto& node : entry.Projection) {
				found.push_back(conversation{
					.Id                 = node.Id,
					.ParentId           = node.ParentId,
					.Kind               = node.Kind == "review_thread" ? application::github_conversation_kind::review_thread : application::github_conversation_kind::review_comment,
					.AuthorLogin        = node.AuthorLogin,
					.Body               = node.Body,
					.Path               = node.Path,
					.Line               = node.Line,
					.DiffSide           = application::github_conversation_diff_side::right,
					.AnchorAvailability = application::github_conversation_anchor_availability::current,
				});
			}
		}
		expect(!found.empty(), "the named fixture case exists");
		return found;
	}

	[[nodiscard]] auto identities(const std::vector<std::string>& aValues)
		-> QSet<QString>
	{
		auto found = QSet<QString>{ };
		for (const auto& value : aValues) {
			found.insert(QString::fromStdString(value));
		}
		return found;
	}

	[[nodiscard]] auto relation_of(
		const qt::github_thread_map& aMap,
		const std::string_view       aIdentity) -> qt::thread_map_relation
	{
		const auto* const node = aMap.node_for(aIdentity);
		expect(node != nullptr, "the map holds the identity being asked about");
		return node->Relation;
	}

	/// \brief Builds a shape the recorded fixture cannot supply.
	///
	/// \details `deep-thread-v1` is deep but narrow at the shoulders: every
	/// parent that owns an edge sits at its own depth, so a crossing test over
	/// it compares nothing. Two siblings that both have children is the
	/// smallest shape in which two edges can occupy one band and therefore the
	/// smallest shape in which crossing is possible at all. It is a Qt layout
	/// input and is deliberately not fixture evidence about GitHub.
	[[nodiscard]] auto forked_thread() -> std::vector<conversation>
	{
		const auto node = [](std::string                aId,
							 std::optional<std::string> aParent,
							 std::string                aAuthor) {
			return conversation{
				.Id          = std::move(aId),
				.ParentId    = std::move(aParent),
				.Kind        = aAuthor.empty()
					? application::github_conversation_kind::review_thread
					: application::github_conversation_kind::review_comment,
				.AuthorLogin = std::move(aAuthor),
				.Path        = std::string{ "src/core/fork.cpp" },
				.Line        = 7U,
			};
		};
		return {
			node("t", std::nullopt, ""),
			node("left", "t", "aardvark"),
			node("right", "t", "zebra"),
			node("left-child", "left", "aardvark-reply"),
			node("right-child", "right", "zebra-reply"),
		};
	}

	/// \brief Cuts one node's pixels out of a grabbed raster.
	///
	/// \details Node bounds are logical; a grab is device pixels. Comparing
	/// whole images only proves that something changed somewhere, which is not
	/// the claim - the claim is about which nodes and edges changed.
	[[nodiscard]] auto patch(const QImage& aImage, const QRectF& aBounds)
		-> QImage
	{
		const auto ratio = aImage.devicePixelRatio();
		const auto rect  = QRect{
			static_cast<int>(std::floor(aBounds.left() * ratio)),
			static_cast<int>(std::floor(aBounds.top() * ratio)),
			static_cast<int>(std::ceil(aBounds.width() * ratio)),
			static_cast<int>(std::ceil(aBounds.height() * ratio)),
		};
		expect(aImage.rect().contains(rect),
			   "the node being inspected is inside the grabbed raster");
		return aImage.copy(rect);
	}

	/// \brief Returns the part of the map a reviewer can actually see.
	///
	/// \details The map scrolls, so "the branch is highlighted" and "the branch
	/// is visible" are different claims, and only the second is what the owner
	/// asked for.
	[[nodiscard]] auto visible_content(const qt::github_thread_map& aMap)
		-> QRectF
	{
		return QRectF{
			static_cast<qreal>(aMap.horizontalScrollBar()->value()),
			static_cast<qreal>(aMap.verticalScrollBar()->value()),
			static_cast<qreal>(aMap.viewport()->width()),
			static_cast<qreal>(aMap.viewport()->height()),
		};
	}

	/// \brief Returns the sliver an edge passes through to reach one node.
	///
	/// \details Every edge ends at its child's top centre, so the few pixels
	/// directly above a box belong to that edge and to no other. A wider band
	/// would overlap the siblings leaving the same parent, and then "this edge
	/// did not change" could not be stated about any single one of them.
	[[nodiscard]] auto edge_entry(
		const qt::github_thread_map& aMap,
		const std::string_view       aChild) -> QRectF
	{
		const auto* const child = aMap.node_for(aChild);
		expect(child != nullptr,
			   "the edge being inspected has its child on the map");
		return QRectF{
			QPointF{ child->Bounds.center().x() - 3.0, child->Bounds.top() - 7.0 },
			QPointF{ child->Bounds.center().x() + 3.0, child->Bounds.top() - 1.0 },
		};
	}

	void test_the_whole_forest_is_mapped()
	{
		const auto deep = fixture_case("valid-deep-wide-tree");
		auto       map  = qt::github_thread_map{ };
		map.adopt(deep);

		expect(map.nodes().size() == deep.size(),
			   "every projected node is laid out, none dropped");
		expect(map.edges().size() == deep.size() - 1U,
			   "every node but the single root carries exactly one reply edge");
		// Eight comments below one thread root, and the deepest chain is five
		// reply edges. Both numbers come from the fixture's own contract.
		expect(map.summary_text() == QStringLiteral("8 comments · depth 5"),
			   "the summary states the fixture's exact comment count and depth");

		for (const auto& node : map.nodes()) {
			expect(!node.Bounds.isEmpty(),
				   "a laid-out node occupies real space");
			expect(!node.Label.isEmpty(), "a laid-out node is labelled");
		}
	}

	void test_no_two_edges_cross()
	{
		// Two edges can only cross if they share a band, and they share a band
		// only if their parents share a depth. `deep-thread-v1` never puts two
		// edge-owning parents at one depth, so it is measured here rather than
		// assumed: a run over it alone compares nothing at all.
		const auto shapes = std::vector<std::vector<conversation>>{
			fixture_case("valid-deep-wide-tree"),
			forked_thread(),
		};
		auto compared = std::size_t{ 0U };
		for (const auto& shape : shapes) {
			auto map = qt::github_thread_map{ };
			map.adopt(shape);

			// Slot is what the layout claims; Bounds is what a reader sees. Tie
			// them together, or an implementation could keep slots in pre-order
			// while painting the boxes in any order it liked.
			for (const auto& first : map.nodes()) {
				for (const auto& second : map.nodes()) {
					if (first.Depth != second.Depth
						|| first.Slot >= second.Slot) {
						continue;
					}
					expect(first.Bounds.right() < second.Bounds.left(),
						   "a later slot at one depth is drawn further "
						   "right, and the two boxes never overlap");
				}
			}

			for (const auto& first : map.edges()) {
				for (const auto& second : map.edges()) {
					const auto* const firstParent
						= map.node_for(first.ParentIdentity);
					const auto* const secondParent
						= map.node_for(second.ParentIdentity);
					expect(firstParent != nullptr && secondParent != nullptr,
						   "every edge names a mapped parent");
					if (first.ParentIdentity == second.ParentIdentity
						|| firstParent->Depth != secondParent->Depth
						|| firstParent->Bounds.center().x()
							>= secondParent->Bounds.center().x()) {
						continue;
					}
					const auto* const firstChild
						= map.node_for(first.ChildIdentity);
					const auto* const secondChild
						= map.node_for(second.ChildIdentity);
					expect(firstChild != nullptr && secondChild != nullptr,
						   "every edge names a mapped child");
					expect(firstChild->Bounds.center().x()
							   < secondChild->Bounds.center().x(),
						   "an edge from a parent on the left ends on the "
						   "left, so no two edges in one band cross");
					++compared;
				}
			}
		}
		// The assertion above is only worth having if it ran. This is the check
		// whose absence let a vacuous crossing test pass review.
		expect(compared > 0U,
			   "at least one pair of edges actually shared a band and was "
			   "compared");
	}

	void test_the_branch_to_a_leaf_is_the_whole_path_from_the_root()
	{
		const auto deep = fixture_case("valid-deep-wide-tree");
		auto       map  = qt::github_thread_map{ };
		map.adopt(deep);
		expect(map.set_focus_identity("review-comment:deep-thread-v1-c04-sibling"),
			   "focusing a node the map holds is accepted");

		// The floor the owner set: "it still needs to show the full branch that
		// you clicked on, or at least up to the node you selected."
		const auto path = map.branch_path();
		expect(identities(path)
				   == QSet<QString>{
					   QStringLiteral("review-thread:deep-thread-v1"),
					   QStringLiteral("review-comment:deep-thread-v1-c01"),
					   QStringLiteral("review-comment:deep-thread-v1-c02"),
					   QStringLiteral("review-comment:deep-thread-v1-c03"),
					   QStringLiteral("review-comment:deep-thread-v1-c04-sibling"),
				   },
			   "the branch is the complete chain from the thread root to the "
			   "selected reply");
		expect(path.front() == "review-thread:deep-thread-v1"
				   && path.back()
					   == "review-comment:deep-thread-v1-c04-sibling",
			   "the branch reads root first and selection last");
		expect(map.branch_below().empty(),
			   "a leaf has nothing below it on its own branch");

		expect(relation_of(map, "review-comment:deep-thread-v1-c04-sibling")
				   == qt::thread_map_relation::focused,
			   "the clicked node is the one in focus");
		for (const auto& ancestor : { "review-thread:deep-thread-v1",
									  "review-comment:deep-thread-v1-c01",
									  "review-comment:deep-thread-v1-c02",
									  "review-comment:deep-thread-v1-c03" }) {
			expect(relation_of(map, ancestor)
					   == qt::thread_map_relation::ancestor,
				   "every node between the root and the selection is on the "
				   "branch");
		}
		// The nodes that are emphatically not on it. Without this the claim
		// would pass for an implementation that highlighted the whole tree.
		for (const auto& elsewhere : { "review-comment:deep-thread-v1-c02-sibling",
									   "review-comment:deep-thread-v1-c04",
									   "review-comment:deep-thread-v1-c05",
									   "review-comment:deep-thread-v1-c06" }) {
			expect(relation_of(map, elsewhere)
					   == qt::thread_map_relation::unrelated,
				   "a node on a different branch is not highlighted");
		}
	}

	void test_a_middle_selection_shows_what_follows_it()
	{
		const auto deep = fixture_case("valid-deep-wide-tree");
		auto       map  = qt::github_thread_map{ };
		map.adopt(deep);
		static_cast<void>(
			map.set_focus_identity("review-comment:deep-thread-v1-c03"));

		expect(identities(map.branch_below())
				   == QSet<QString>{
					   QStringLiteral("review-comment:deep-thread-v1-c04"),
					   QStringLiteral("review-comment:deep-thread-v1-c04-sibling"),
					   QStringLiteral("review-comment:deep-thread-v1-c05"),
					   QStringLiteral("review-comment:deep-thread-v1-c06"),
				   },
			   "the rest of the clicked branch is everything below the "
			   "selection");
		expect(relation_of(map, "review-comment:deep-thread-v1-c05")
				   == qt::thread_map_relation::descendant,
			   "what follows the selection is drawn as part of its branch");
		expect(relation_of(map, "review-comment:deep-thread-v1-c02")
				   == qt::thread_map_relation::ancestor,
			   "what precedes the selection stays the path that was asked for");
		expect(relation_of(map, "review-comment:deep-thread-v1-c02-sibling")
				   == qt::thread_map_relation::unrelated,
			   "a sibling branch is still not the branch that was clicked");
		expect(map.branch_text().contains(QStringLiteral("4 replies below it")),
			   "the reviewer is told how much of the thread is below them");
	}

	void test_an_unchanged_projection_costs_nothing()
	{
		// The drawer re-adopts whenever the anchored file or line changes, which
		// a reviewer does by scrolling. Relaying the forest each time would make
		// an unchanged refresh cost the whole map.
		const auto deep = fixture_case("valid-deep-wide-tree");
		auto       map  = qt::github_thread_map{ };
		map.adopt(deep);
		const auto first = map.statistics();
		map.adopt(deep);
		const auto second = map.statistics();
		expect(second.Layouts == first.Layouts
				   && second.SkippedRebuilds == first.SkippedRebuilds + 1U,
			   "re-adopting the same projection lays nothing out again");

		auto changed = deep;
		changed.pop_back();
		map.adopt(changed);
		expect(map.statistics().Layouts == first.Layouts + 1U
				   && map.nodes().size() == changed.size(),
			   "a projection that really changed is laid out again");
	}

	void test_no_read_state_is_invented()
	{
		// The accepted mockup legends per-node read/unread. Nothing in this
		// product carries it - the ledger's unread notion is a per-conversation
		// cursor - so the card's instruction was to build that state or drop
		// the distinction, and never to fake it. This is the guard on dropping
		// it.
		const auto deep = fixture_case("valid-deep-wide-tree");
		auto       map  = qt::github_thread_map{ };
		map.adopt(deep);
		static_cast<void>(
			map.set_focus_identity("review-comment:deep-thread-v1-c03"));
		// Whole words. A thread's own anchored filename contains "read", and a
		// guard that cannot tell `thread_map.cpp` from a read receipt would be
		// removed by the first person it stopped.
		const auto claim = QRegularExpression{
			QStringLiteral("\\b(un)?read\\b"),
			QRegularExpression::CaseInsensitiveOption,
		};
		for (const auto& caption : { map.summary_text(), map.branch_text() }) {
			expect(!claim.match(caption).hasMatch(),
				   "neither caption states a read or unread claim the "
				   "product cannot back");
		}
	}

	void test_an_unexpected_shape_costs_nesting_and_never_a_row()
	{
		for (const auto& name : { "hostile-absent-parent", "hostile-cycle" }) {
			const auto hostile = fixture_case(name);
			auto       map     = qt::github_thread_map{ };
			map.adopt(hostile);
			expect(map.nodes().size() == hostile.size(),
				   "a parent that is absent or circular still leaves every row "
				   "on the map");
			for (const auto& node : map.nodes()) {
				expect(!node.Bounds.isEmpty(),
					   "a promoted node is drawn rather than hidden");
			}

			// Counting rows is not enough, and this is the assertion whose
			// absence let the map and the tree disagree about the same forest.
			// The drawer shows both at once, so a shape they answer differently
			// is a defect the reviewer sees rather than an internal detail.
			auto model = qt::github_conversation_model{ };
			model.apply(hostile);
			auto roots = std::size_t{ 0U };
			for (const auto& node : map.nodes()) {
				if (node.ParentIdentity.empty()) {
					++roots;
				}
			}
			expect(roots == static_cast<std::size_t>(model.rowCount({ })),
				   "the map promotes exactly the nodes the drawer's own tree "
				   "promotes, so one forest is not drawn as two shapes");
		}
	}

	void test_a_press_selects_and_reports_the_node_under_it()
	{
		const auto deep = fixture_case("valid-deep-wide-tree");
		auto       map  = qt::github_thread_map{ };
		map.resize(900, 400);
		map.adopt(deep);
		map.show();
		process_events();

		auto reported = std::vector<std::string>{ };
		map.set_selection_sink([&reported](const std::string& aIdentity) {
			reported.push_back(aIdentity);
		});

		const auto* const target
			= map.node_for("review-comment:deep-thread-v1-c02-sibling");
		expect(target != nullptr, "the node to be pressed is on the map");
		const auto centre = target->Bounds.center();
		auto       press  = QMouseEvent{ QEvent::MouseButtonPress,
										 centre,
										 map.viewport()->mapToGlobal(centre),
										 Qt::LeftButton,
										 Qt::LeftButton,
										 Qt::NoModifier };
		QApplication::sendEvent(map.viewport(), &press);
		process_events();

		expect(reported.size() == 1U
				   && reported.front()
					   == "review-comment:deep-thread-v1-c02-sibling",
			   "a press reports exactly the node under the pointer");
		expect(map.focus_identity() == "review-comment:deep-thread-v1-c02-sibling",
			   "a press puts that node in focus");
		expect(relation_of(map, "review-comment:deep-thread-v1-c01")
				   == qt::thread_map_relation::ancestor,
			   "the branch follows the press rather than the previous "
			   "selection");
	}

	void test_the_highlight_is_actually_painted()
	{
		const auto deep = fixture_case("valid-deep-wide-tree");
		auto       map  = qt::github_thread_map{ };
		// Sized so the whole map fits without scrolling, which keeps logical
		// node bounds and raster coordinates the same measurement.
		map.resize(900, 400);
		map.adopt(deep);
		map.show();
		process_events();

		const auto expectedScale = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
		if (!expectedScale.isEmpty()) {
			expect(std::abs(map.devicePixelRatioF() - expectedScale.toDouble())
					   < 0.05,
				   "the thread map observes the requested Qt scale");
		}

		const auto unfocused = map.grab().toImage();
		expect(!unfocused.isNull() && unfocused.width() > 0,
			   "the map renders to a real raster");

		const auto* const ancestor
			= map.node_for("review-comment:deep-thread-v1-c02");
		const auto* const elsewhere
			= map.node_for("review-comment:deep-thread-v1-c02-sibling");
		expect(ancestor != nullptr && elsewhere != nullptr,
			   "the nodes whose pixels are compared are on the map");
		const auto onPath
			= edge_entry(map, "review-comment:deep-thread-v1-c02");
		const auto offPath
			= edge_entry(map, "review-comment:deep-thread-v1-c02-sibling");

		static_cast<void>(
			map.set_focus_identity("review-comment:deep-thread-v1-c06"));
		process_events();
		const auto focused = map.grab().toImage();
		expect(focused.size() == unfocused.size(),
			   "the highlight changes the drawing rather than the geometry");

		// The claim is visual, and it is about the branch rather than about the
		// one box under the cursor. Comparing whole images would pass for an
		// implementation that highlighted the selected node and nothing else,
		// which is precisely what the owner asked for more than.
		expect(patch(focused, ancestor->Bounds)
				   != patch(unfocused, ancestor->Bounds),
			   "a node between the root and the selection is repainted");
		expect(patch(focused, onPath) != patch(unfocused, onPath),
			   "an edge on the path to the selection is repainted");
		expect(patch(focused, elsewhere->Bounds)
				   == patch(unfocused, elsewhere->Bounds),
			   "a node on another branch is left exactly as it was");
		expect(patch(focused, offPath) == patch(unfocused, offPath),
			   "an edge leaving the path is left exactly as it was");

		const auto renderPath
			= qEnvironmentVariable("BPR_QT_THREAD_MAP_RENDER_PATH");
		if (!renderPath.isEmpty()) {
			expect(focused.save(renderPath), "the mapped branch raster saves");
		}
	}

	// [[BPR-0649]]. The ledger's per-post read state fills nodes; nothing
	// else does, and withdrawing it restores the plain node.
	void test_unread_nodes_are_filled_from_the_ledger_state()
	{
		const auto deep = fixture_case("valid-deep-wide-tree");
		auto       map  = qt::github_thread_map{ };
		map.resize(900, 400);
		map.adopt(deep);
		map.show();
		process_events();
		const auto* const filled = map.node_for("review-comment:deep-thread-v1-c03");
		const auto* const plain  = map.node_for("review-comment:deep-thread-v1-c02-sibling");
		expect(filled != nullptr && plain != nullptr, "both compared nodes are on the map");
		const auto before = map.grab().toImage();
		map.set_unread({ "review-comment:deep-thread-v1-c03" });
		process_events();
		const auto after = map.grab().toImage();
		expect(map.unread("review-comment:deep-thread-v1-c03") && !map.unread("review-comment:deep-thread-v1-c02-sibling"),
			   "the map answers which nodes the ledger called unread");
		expect(patch(after, filled->Bounds) != patch(before, filled->Bounds),
			   "an unread node is repainted");
		expect(patch(after, plain->Bounds) == patch(before, plain->Bounds),
			   "a read node is left exactly as it was");
		map.set_unread({ });
		process_events();
		const auto restored = map.grab().toImage();
		expect(patch(restored, filled->Bounds) == patch(before, filled->Bounds),
			   "withdrawing the read state restores the plain node");
	}

	void test_the_keyboard_walks_the_tree()
	{
		const auto deep = fixture_case("valid-deep-wide-tree");
		auto       map  = qt::github_thread_map{ };
		map.resize(900, 400);
		map.adopt(deep);
		map.show();
		process_events();

		const auto press = [&map](const int aKey) {
			auto event = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
			QApplication::sendEvent(&map, &event);
			process_events();
		};

		press(Qt::Key_Home);
		expect(map.focus_identity() == "review-thread:deep-thread-v1",
			   "Home reaches the thread root");
		press(Qt::Key_Down);
		press(Qt::Key_Down);
		expect(map.focus_identity() == "review-comment:deep-thread-v1-c02",
			   "Down descends to the first reply of the current node");
		press(Qt::Key_Right);
		expect(map.focus_identity()
				   == "review-comment:deep-thread-v1-c02-sibling",
			   "Right moves across to the next reply of the same parent");
		press(Qt::Key_Left);
		expect(map.focus_identity() == "review-comment:deep-thread-v1-c02",
			   "Left moves back");
		press(Qt::Key_Up);
		expect(map.focus_identity() == "review-comment:deep-thread-v1-c01",
			   "Up climbs to the comment that was replied to");

		// A widget in the panel's tab chain that swallowed unhandled keys would
		// silently break scrolling and the drawer's own shortcuts.
		const auto before = map.focus_identity();
		press(Qt::Key_Left);
		expect(map.focus_identity() == before,
			   "a move with nowhere to go leaves the focus where it was");
	}

	void test_the_map_says_where_it_is_to_a_screen_reader()
	{
		const auto deep = fixture_case("valid-deep-wide-tree");
		auto       map  = qt::github_thread_map{ };
		map.adopt(deep);
		expect(map.accessibleName() == QStringLiteral("Review thread map"),
			   "the map is named");
		static_cast<void>(
			map.set_focus_identity("review-comment:deep-thread-v1-c04-sibling"));
		const auto description = map.accessibleDescription();
		expect(description.contains(QStringLiteral("8 comments"))
				   && description.contains(QStringLiteral("@reviewer-three"))
				   && description.contains(QStringLiteral("Path to this reply")),
			   "the description carries the thread size, who is in focus, and "
			   "the path to them, rather than only a node count");
		expect(description.contains(
				   QStringLiteral("src/core/thread_map.cpp")),
			   "the full anchored path is spoken even though the box is short");
	}

	void test_the_shipped_workbench_composes_the_map()
	{
		// BPR-0225's rule: a component test is evidence for the component, not
		// for reachability. This one drives the window the application opens,
		// through the same adopt path the production host uses.
		auto window = qt::workbench_window{ };
		window.resize(1'400, 900);
		window.show();
		process_events();

		// Published through `github_projection_port`, which is the port the
		// application host itself writes to. Nothing here reaches past the
		// public surface to place the content it then measures.
		auto pullRequest                   = application::github_pull_request_projection{ };
		pullRequest.Conversations          = fixture_case("valid-deep-wide-tree");
		pullRequest.ReviewThreadsAvailable = true;
		pullRequest.ReviewThreadCount      = 1U;
		window.publish(application::github_presentation_update{
			.PullRequest = pullRequest,
		});
		process_events();

		auto* const panel = window.conversation_panel();
		expect(panel != nullptr, "the shipped window still owns the drawer");
		auto* const map = panel->thread_map();
		expect(map != nullptr,
			   "the shipped conversation drawer composes the thread map");
		expect(map->nodes().size() == pullRequest.Conversations.size(),
			   "the composed map holds the conversation the window adopted");
		expect(panel->thread_map_summary()->text()
				   == QStringLiteral("8 comments · depth 5"),
			   "the drawer states the mapped thread's real size");

		// A press on the map has to move the drawer's own selection, because
		// that selection is what the code reveal, the anchor marker, and the
		// defect surface already read.
		const auto* const target
			= map->node_for("review-comment:deep-thread-v1-c04");
		expect(target != nullptr, "the composed map holds the deep reply");
		// The drawer's band is short, so a deep node is reached by scrolling to
		// it - which is a gesture, not a workaround. What must not happen is
		// what happened before this was measured: a press delivered at a point
		// outside the viewport, proving the sink is wired and nothing about
		// whether a pointer could ever be there.
		map->verticalScrollBar()->setValue(
			static_cast<int>(std::floor(target->Bounds.top())));
		process_events();
		expect(visible_content(*map).contains(target->Bounds),
			   "the node about to be pressed is inside the visible map");
		const auto centre = target->Bounds.center()
			- QPointF{ static_cast<qreal>(map->horizontalScrollBar()->value()),
					   static_cast<qreal>(map->verticalScrollBar()->value()) };
		auto press = QMouseEvent{ QEvent::MouseButtonPress,
								  centre,
								  map->viewport()->mapToGlobal(centre),
								  Qt::LeftButton,
								  Qt::LeftButton,
								  Qt::NoModifier };
		QApplication::sendEvent(map->viewport(), &press);
		process_events();
		expect(panel->model()->id_for_index(panel->view()->currentIndex())
				   == "review-comment:deep-thread-v1-c04",
			   "pressing a map node selects that comment in the shipped "
			   "drawer");
		expect(panel->thread_map_branch()->text().contains(
				   QStringLiteral("Path to this reply")),
			   "the drawer shows the path to the reply that was pressed");
	}

	void test_selecting_in_the_drawer_moves_the_map()
	{
		auto window = qt::workbench_window{ };
		window.resize(1'400, 900);
		window.show();
		process_events();

		auto pullRequest                   = application::github_pull_request_projection{ };
		pullRequest.Conversations          = fixture_case("valid-deep-wide-tree");
		pullRequest.ReviewThreadsAvailable = true;
		pullRequest.ReviewThreadCount      = 1U;
		window.publish(application::github_presentation_update{
			.PullRequest = pullRequest,
		});
		process_events();

		auto* const panel = window.conversation_panel();
		const auto  deep
			= panel->model()->index_for_id("review-comment:deep-thread-v1-c05");
		expect(deep.isValid(), "the drawer's own tree reaches the deep reply");
		panel->view()->setCurrentIndex(deep);
		process_events();

		auto* const map = panel->thread_map();
		expect(map->focus_identity() == "review-comment:deep-thread-v1-c05",
			   "a drawer selection puts the same node in focus on the map");
		expect(relation_of(*map, "review-comment:deep-thread-v1-c02")
				   == qt::thread_map_relation::ancestor,
			   "the map draws the branch of the comment the reviewer opened");

		// The half that state assertions cannot reach. Before this was measured,
		// the drawer highlighted a branch whose selected end sat below the
		// visible band at every ordinary window height, and every signal test
		// still passed.
		const auto        visible = visible_content(*map);
		const auto* const focused
			= map->node_for("review-comment:deep-thread-v1-c05");
		const auto* const parent
			= map->node_for("review-comment:deep-thread-v1-c04");
		expect(focused != nullptr && visible.contains(focused->Bounds),
			   "the node the reviewer selected is on screen in the map, not "
			   "merely highlighted somewhere inside it");
		expect(parent != nullptr && visible.contains(parent->Bounds),
			   "the comment that was replied to is on screen with it, so the "
			   "drawer's narrow band still reads as a path and not a peephole");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewThreadMapTests"));
	test_the_whole_forest_is_mapped();
	test_no_two_edges_cross();
	test_the_branch_to_a_leaf_is_the_whole_path_from_the_root();
	test_a_middle_selection_shows_what_follows_it();
	test_an_unchanged_projection_costs_nothing();
	test_no_read_state_is_invented();
	test_an_unexpected_shape_costs_nesting_and_never_a_row();
	test_a_press_selects_and_reports_the_node_under_it();
	test_the_highlight_is_actually_painted();
	test_unread_nodes_are_filled_from_the_ledger_state();
	test_the_keyboard_walks_the_tree();
	test_the_map_says_where_it_is_to_a_screen_reader();
	test_the_shipped_workbench_composes_the_map();
	test_selecting_in_the_drawer_moves_the_map();
	std::cout << "thread-map-tests:pass\n";
	return EXIT_SUCCESS;
}
