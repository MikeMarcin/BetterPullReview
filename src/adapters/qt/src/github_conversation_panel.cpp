#include "bpr/qt/github_conversation_panel.hpp"

#include "bpr/qt/github_conversation_delegate.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_thread_map.hpp"

#include "github_presentation_text.hpp"

#include <QAction>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto endpoint_is_current(
			const application::github_conversation_projection& aNode,
			const bool                                         aLeftHasCurrentDiffRevision,
			const bool                                         aRightHasCurrentDiffRevision) noexcept -> bool
		{
			if (!aNode.DiffSide.has_value()) {
				return false;
			}
			const auto side = github_conversation_source_side(
				static_cast<int>(*aNode.DiffSide));
			return side.has_value()
				&& (*side == core::diff_side::left
						? aLeftHasCurrentDiffRevision
						: aRightHasCurrentDiffRevision);
		}

		[[nodiscard]] auto with_endpoint_placement(
			std::vector<application::github_conversation_projection> aConversations,
			const bool                                               aLeftHasCurrentDiffRevision,
			const bool                                               aRightHasCurrentDiffRevision)
			-> std::vector<application::github_conversation_projection>
		{
			for (auto& conversation : aConversations) {
				if (conversation.AnchorAvailability
					!= application::github_conversation_anchor_availability::current) {
					continue;
				}
				const auto side = conversation.DiffSide.has_value()
					? github_conversation_source_side(
						  static_cast<int>(*conversation.DiffSide))
					: std::nullopt;
				if (!side.has_value()) {
					conversation.AnchorAvailability = application::
						github_conversation_anchor_availability::missing_side;
				} else if (!endpoint_is_current(
							   conversation,
							   aLeftHasCurrentDiffRevision,
							   aRightHasCurrentDiffRevision)) {
					conversation.AnchorAvailability = application::
						github_conversation_anchor_availability::selected_comparison_mismatch;
				}
			}
			return aConversations;
		}

		/// \brief Keeps the threads whose root is anchored in one file.
		///
		/// \details A reply carries no anchor — only a thread's root names a
		/// path — so membership is decided at the root and inherited. Filtering
		/// node by node would keep a root without its replies, or keep a reply
		/// whose root was dropped and present it as a top-level remark.
		///
		/// Roots are found by following `ParentId`, which is single and optional,
		/// so every chain terminates. A parent naming a node that is not in the
		/// projection is treated as a root: the alternative is discarding a
		/// comment because its ancestor is missing, and a visible orphan is
		/// better than a silent omission.
		[[nodiscard]] auto anchored_to(
			const std::vector<application::github_conversation_projection>&
														aConversations,
			const std::optional<core::file_identity>&   aFile,
			const std::optional<core::source_line_ref>& aLine,
			const bool                                  aLeftHasCurrentDiffRevision,
			const bool                                  aRightHasCurrentDiffRevision)
			-> std::vector<application::github_conversation_projection>
		{
			auto byId = std::unordered_map<std::string, std::size_t>{ };
			byId.reserve(aConversations.size());
			for (auto index = std::size_t{ 0U }; index < aConversations.size();
				 ++index) {
				byId.emplace(aConversations[index].Id, index);
			}

			const auto rootMatches = [&](std::size_t aIndex) {
				const auto anchorMatches = [&](const auto& aNode) {
					if (aFile.has_value()
						&& (aNode.Path.empty()
							|| core::file_identity_of(aNode.Path) != *aFile)) {
						return false;
					}
					if (!aLine.has_value()) {
						return true;
					}
					if (aNode.AnchorAvailability
							!= application::github_conversation_anchor_availability::current
						|| !aNode.Line.has_value() || *aNode.Line == 0U
						|| !endpoint_is_current(
							aNode,
							aLeftHasCurrentDiffRevision,
							aRightHasCurrentDiffRevision)) {
						return false;
					}
					const auto side = github_conversation_source_side(
						static_cast<int>(*aNode.DiffSide));
					return side.has_value() && *side == aLine->Side
						&& *aNode.Line - 1U == aLine->Line;
				};
				// Bounded by the node count, so a projection that somehow
				// contained a cycle cannot spin here.
				for (auto step = std::size_t{ 0U }; step <= aConversations.size();
					 ++step) {
					const auto& node = aConversations[aIndex];
					if (!node.ParentId.has_value()) {
						return anchorMatches(node);
					}
					const auto parent = byId.find(*node.ParentId);
					if (parent == byId.end()) {
						return anchorMatches(node);
					}
					aIndex = parent->second;
				}
				return false;
			};

			auto kept = std::vector<application::github_conversation_projection>{ };
			kept.reserve(aConversations.size());
			for (auto index = std::size_t{ 0U }; index < aConversations.size();
				 ++index) {
				if (rootMatches(index)) {
					kept.push_back(aConversations[index]);
				}
			}
			return kept;
		}
	} // namespace

	github_conversation_panel::github_conversation_panel(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("pullRequestConversationGroup"));
		setAccessibleName(QStringLiteral("Read-only GitHub conversations"));

		auto* const layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);

		AvailabilityLabel = new QLabel{ this };
		AvailabilityLabel->setObjectName(
			QStringLiteral("reviewThreadAvailability"));
		AvailabilityLabel->setAccessibleName(
			QStringLiteral("Review-thread availability"));
		AvailabilityLabel->setWordWrap(true);
		detail::set_plain_text_label(AvailabilityLabel);

		auto* const header = new QHBoxLayout;
		header->addWidget(AvailabilityLabel, 1);
		RevealButton = new QToolButton{ this };
		RevealButton->setObjectName(QStringLiteral("showReviewThreadsButton"));
		RevealButton->setAccessibleName(QStringLiteral("Show review threads"));
		RevealButton->setFocusPolicy(Qt::StrongFocus);
		RevealButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		header->addWidget(RevealButton);
		layout->addLayout(header);

		Stack = new QStackedWidget{ this };
		Stack->setObjectName(QStringLiteral("githubConversationState"));
		auto* const empty = new QWidget{ Stack };
		empty->setObjectName(QStringLiteral("githubConversationEmptyState"));
		Stack->addWidget(empty);

		Model = new github_conversation_model{ this };

		// The loaded state shows the same forest twice. The map answers "where
		// am I in this thread" - the question BPR-0211 recorded a reviewer
		// losing the answer to at depth - and the tree below it answers "what
		// does this comment say". A splitter, because which of those a reviewer
		// wants more is not a decision this panel gets to make for them.
		auto* const loaded = new QSplitter{ Qt::Vertical, Stack };
		loaded->setObjectName(QStringLiteral("githubConversationLoaded"));
		loaded->setChildrenCollapsible(true);

		auto* const mapPane   = new QWidget{ loaded };
		auto* const mapLayout = new QVBoxLayout{ mapPane };
		mapLayout->setContentsMargins(0, 0, 0, 0);
		ThreadMapSummary = new QLabel{ mapPane };
		ThreadMapSummary->setObjectName(QStringLiteral("threadMapSummary"));
		ThreadMapSummary->setAccessibleName(
			QStringLiteral("Thread map summary"));
		detail::set_plain_text_label(ThreadMapSummary);
		mapLayout->addWidget(ThreadMapSummary);
		ThreadMap = new github_thread_map{ mapPane };
		mapLayout->addWidget(ThreadMap, 1);
		ThreadMapBranch = new QLabel{ mapPane };
		ThreadMapBranch->setObjectName(QStringLiteral("threadMapBranch"));
		ThreadMapBranch->setAccessibleName(
			QStringLiteral("Path to the selected reply"));
		ThreadMapBranch->setWordWrap(true);
		detail::set_plain_text_label(ThreadMapBranch);
		mapLayout->addWidget(ThreadMapBranch);
		loaded->addWidget(mapPane);

		View = new QTreeView{ loaded };
		View->setObjectName(QStringLiteral("githubConversationView"));
		View->setAccessibleName(QStringLiteral("Read-only GitHub conversations"));
		View->setAccessibleDescription(
			QStringLiteral("Reviews, review threads, and pull request comments"));
		View->setModel(Model);
		View->setSelectionBehavior(QAbstractItemView::SelectRows);
		View->setSelectionMode(QAbstractItemView::SingleSelection);
		View->setEditTriggers(QAbstractItemView::NoEditTriggers);
		// A reply feed, not a staircase. BPR-0456: the owner asked for a
		// linear feed grouped by root, because a per-depth indent spends the
		// width the comment text needs, and gave the reason plainly - the
		// thread map is where depth is read now. So the view stops indenting
		// by depth and the delegate gives every reply the same single step.
		View->setRootIsDecorated(false);
		View->setIndentation(0);
		View->setItemsExpandable(true);
		View->setWordWrap(true);
		// Rows are as tall as their comment, so no two are alike and none of
		// them can be assumed uniform.
		View->setUniformRowHeights(false);
		View->setTextElideMode(Qt::ElideNone);
		View->header()->hide();
		View->setItemDelegate(new github_conversation_delegate{ View });
		loaded->addWidget(View);
		// The map asks for the band its own sizeHint names and the tree takes
		// the rest. Left to stretch factors alone the map collapsed to its
		// minimum in an ordinary window, which showed a highlighted branch
		// whose end was off-screen - the exact failure the owner's
		// "show the full branch" direction is about.
		loaded->setStretchFactor(0, 0);
		loaded->setStretchFactor(1, 1);
		Stack->addWidget(loaded);
		layout->addWidget(Stack, 1);

		// Selection has one owner. A map node press moves the tree's current
		// index, and everything that already listened to the tree - the code
		// reveal, the defect surface, the anchor marker - keeps hearing about
		// it from exactly where it did before.
		ThreadMap->set_selection_sink([this](const std::string& aIdentity) {
			const auto target = Model->index_for_id(aIdentity);
			if (target.isValid()) {
				View->setCurrentIndex(target);
			}
		});
		connect(
			View->selectionModel(),
			&QItemSelectionModel::currentChanged,
			this,
			[this](const QModelIndex&) { synchronize_map_focus(); });

		setTabOrder(RevealButton, ThreadMap);
		setTabOrder(ThreadMap, View);
	}

	void github_conversation_panel::synchronize_map_focus()
	{
		// Not announced: the tree the reviewer moved in has already said what
		// is now selected, and the map echoing it makes one gesture speak twice.
		ThreadMap->set_focus_identity(
			Model->id_for_index(View->currentIndex()), false);
		ThreadMapBranch->setText(ThreadMap->branch_text());
	}

	void github_conversation_panel::adopt(
		const std::vector<application::github_conversation_projection>&
				aConversations,
		QString aAvailabilityText)
	{
		// Captured before the model changes. Restoring by identity rather than
		// by row is what keeps a reviewer on the comment they were reading when
		// a reply lands above it.
		const auto selected = Model->id_for_index(View->currentIndex());

		Retained             = aConversations;
		RetainedAvailability = aAvailabilityText;
		AvailabilityLabel->setText(std::move(aAvailabilityText));
		auto shown = AnchorFile.has_value() || AnchorLine.has_value()
			? anchored_to(
				  aConversations,
				  AnchorFile,
				  AnchorLine,
				  LeftHasCurrentDiffRevision,
				  RightHasCurrentDiffRevision)
			: aConversations;
		shown      = with_endpoint_placement(
			std::move(shown),
			LeftHasCurrentDiffRevision,
			RightHasCurrentDiffRevision);
		Model->apply(shown);
		ThreadMap->adopt(shown);
		ThreadMapSummary->setText(ThreadMap->summary_text());
		Stack->setCurrentIndex(shown.empty() ? 0 : 1);
		View->doItemsLayout();
		if (ExpandOnNextProjection && !shown.empty()) {
			View->expandAll();
			ExpandOnNextProjection = false;
		}
		if (!selected.empty()) {
			const auto restored = Model->index_for_id(selected);
			if (restored.isValid()) {
				// Blocked, because restoring a selection is not the reviewer
				// making one, and anything listening should not be told it was.
				const QSignalBlocker blocker{ View->selectionModel() };
				View->setCurrentIndex(restored);
			}
		}
		// Set explicitly rather than through the signal, which the restore
		// above deliberately blocks: the map has to end an adopt showing the
		// branch the reviewer was reading, not the one before the refresh.
		synchronize_map_focus();
	}

	void github_conversation_panel::clear(QString aAvailabilityText)
	{
		ExpandOnNextProjection = true;
		Retained.clear();
		RetainedAvailability.clear();
		AvailabilityLabel->setText(std::move(aAvailabilityText));
		Model->clear();
		ThreadMap->clear();
		ThreadMapSummary->setText(ThreadMap->summary_text());
		ThreadMapBranch->setText(ThreadMap->branch_text());
		Stack->setCurrentIndex(0);
	}

	void github_conversation_panel::set_anchor_file(
		std::optional<core::file_identity> aFile)
	{
		if (AnchorFile == aFile) {
			return;
		}
		AnchorFile = std::move(aFile);
		// Re-filtered from what was retained rather than waiting for the next
		// projection. Moving between files is a reviewer gesture and the
		// drawer has to follow it, not follow the network.
		if (!Retained.empty()) {
			// Copied deliberately. Passing `Retained` straight back in would
			// make `adopt`'s first act a self-assignment, which is legal and
			// still the kind of thing that becomes a defect the moment someone
			// adds a clear() to the top of it.
			const auto retained     = Retained;
			const auto availability = RetainedAvailability;
			adopt(retained, availability);
		}
	}

	void github_conversation_panel::set_anchor_line(
		std::optional<core::source_line_ref> aLine)
	{
		if (AnchorLine == aLine) {
			return;
		}
		AnchorLine = std::move(aLine);
		if (!Retained.empty()) {
			const auto retained     = Retained;
			const auto availability = RetainedAvailability;
			adopt(retained, availability);
		}
	}

	void github_conversation_panel::set_current_endpoint_placement(
		const bool aLeftHasCurrentDiffRevision,
		const bool aRightHasCurrentDiffRevision)
	{
		if (LeftHasCurrentDiffRevision == aLeftHasCurrentDiffRevision
			&& RightHasCurrentDiffRevision == aRightHasCurrentDiffRevision) {
			return;
		}
		LeftHasCurrentDiffRevision  = aLeftHasCurrentDiffRevision;
		RightHasCurrentDiffRevision = aRightHasCurrentDiffRevision;
		if (!Retained.empty()) {
			const auto retained     = Retained;
			const auto availability = RetainedAvailability;
			adopt(retained, availability);
		}
	}

	auto github_conversation_panel::anchor_file() const noexcept
		-> const std::optional<core::file_identity>&
	{
		return AnchorFile;
	}

	auto github_conversation_panel::anchor_line() const noexcept
		-> const std::optional<core::source_line_ref>&
	{
		return AnchorLine;
	}

	void github_conversation_panel::set_reveal_action(QAction* const aAction)
	{
		RevealButton->setDefaultAction(aAction);
		// setDefaultAction adopts the action's name, and the action names the
		// gesture while the button names the control. Restored so the two do
		// not answer to one accessible name between them.
		RevealButton->setAccessibleName(QStringLiteral("Show review threads"));
	}

	void github_conversation_panel::relayout()
	{
		View->doItemsLayout();
	}

	auto github_conversation_panel::view() const noexcept -> QTreeView*
	{
		return View;
	}

	auto github_conversation_panel::thread_map() const noexcept
		-> github_thread_map*
	{
		return ThreadMap;
	}

	auto github_conversation_panel::thread_map_summary() const noexcept
		-> QLabel*
	{
		return ThreadMapSummary;
	}

	auto github_conversation_panel::thread_map_branch() const noexcept -> QLabel*
	{
		return ThreadMapBranch;
	}

	auto github_conversation_panel::model() const noexcept
		-> github_conversation_model*
	{
		return Model;
	}

	auto github_conversation_panel::availability_label() const noexcept -> QLabel*
	{
		return AvailabilityLabel;
	}

	auto github_conversation_panel::reveal_button() const noexcept -> QToolButton*
	{
		return RevealButton;
	}
} // namespace bpr::qt
