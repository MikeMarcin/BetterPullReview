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
#include <string>
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

		// BPR-0527. Stacked, for the reason BPR-0506 stacked the finding
		// actions and BPR-0520 the defect rows. This panel is the top of the
		// conversations drawer's column, which scrolls down and never
		// sideways, so a row wider than the drawer loses its last control
		// rather than pushing it offscreen. *Show review threads* was
		// measured 17 px past the right edge at the drawer's default width.
		//
		// Not because of this row, as it turned out. The sentence wraps, so
		// horizontal it asked for 198 px, and what put the button past the
		// edge was the widest row elsewhere in the column stretching every
		// panel beside it. Stacked anyway: the default width is the only
		// width that measurement was about, and the drawer drags down to
		// `minimum_workbench_drawer_width`. A column cannot clip - the
		// sentence wraps to whatever width there is and the button is as wide
		// as its own label - so this row holds at every width, not one.
		//
		// The stretch goes with the row. It gave the sentence the width the
		// button did not take, and a column has nothing to divide. The action,
		// its label, its object name, its accessible name, and the order it is
		// read and tabbed in are untouched.
		auto* const header = new QVBoxLayout;
		header->addWidget(AvailabilityLabel);
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
		Loaded             = loaded;
		loaded->setObjectName(QStringLiteral("githubConversationLoaded"));
		loaded->setChildrenCollapsible(true);

		// [[BPR-0648]]. The cards first: this file's threads at their lines,
		// or every thread of the pull request in file order, with the scope
		// chosen by the two buttons above them. The map and the tree keep
		// their places beneath as the detail of the current thread.
		auto* const cardsPane = new QWidget{ loaded };
		CardsPane             = cardsPane;
		cardsPane->setObjectName(QStringLiteral("conversationCardsPane"));
		auto* const cardsLayout = new QVBoxLayout{ cardsPane };
		cardsLayout->setContentsMargins(0, 0, 0, 0);
		cardsLayout->setSpacing(2);
		auto* const scopeRow = new QHBoxLayout;
		scopeRow->setContentsMargins(4, 0, 4, 0);
		scopeRow->setSpacing(4);
		ScopeThisFile = new QToolButton{ cardsPane };
		ScopeThisFile->setObjectName(QStringLiteral("conversationScopeThisFile"));
		ScopeThisFile->setText(QStringLiteral("This file"));
		ScopeThisFile->setAccessibleName(QStringLiteral("This file"));
		ScopeThisFile->setAccessibleDescription(QStringLiteral(
			"Shows the threads of the file the comparison shows, at their lines"));
		ScopeThisFile->setCheckable(true);
		ScopeThisFile->setChecked(true);
		ScopeThisFile->setAutoExclusive(true);
		ScopeThisFile->setFocusPolicy(Qt::StrongFocus);
		ScopeWholePr = new QToolButton{ cardsPane };
		ScopeWholePr->setObjectName(QStringLiteral("conversationScopeWholePullRequest"));
		ScopeWholePr->setText(QStringLiteral("Whole PR"));
		ScopeWholePr->setAccessibleName(QStringLiteral("Whole pull request"));
		ScopeWholePr->setAccessibleDescription(QStringLiteral(
			"Lists every thread of the pull request in file order"));
		ScopeWholePr->setCheckable(true);
		ScopeWholePr->setAutoExclusive(true);
		ScopeWholePr->setFocusPolicy(Qt::StrongFocus);
		scopeRow->addWidget(ScopeThisFile);
		scopeRow->addWidget(ScopeWholePr);
		// [[BPR-0649]]. Discuss's two other filters, over the same feed.
		FilterUnread = new QToolButton{ cardsPane };
		FilterUnread->setObjectName(QStringLiteral("conversationFilterUnread"));
		FilterUnread->setText(QStringLiteral("Unread"));
		FilterUnread->setAccessibleName(QStringLiteral("Unread only"));
		FilterUnread->setAccessibleDescription(QStringLiteral(
			"Show only threads with a post newer than what you have read"));
		FilterUnread->setCheckable(true);
		FilterUnread->setFocusPolicy(Qt::StrongFocus);
		FilterUnresolved = new QToolButton{ cardsPane };
		FilterUnresolved->setObjectName(QStringLiteral("conversationFilterUnresolved"));
		FilterUnresolved->setText(QStringLiteral("Unresolved"));
		FilterUnresolved->setAccessibleName(QStringLiteral("Unresolved only"));
		FilterUnresolved->setAccessibleDescription(QStringLiteral("Show only threads that are not resolved"));
		FilterUnresolved->setCheckable(true);
		FilterUnresolved->setFocusPolicy(Qt::StrongFocus);
		scopeRow->addWidget(FilterUnread);
		scopeRow->addWidget(FilterUnresolved);
		scopeRow->addStretch(1);
		cardsLayout->addLayout(scopeRow);
		Cards = new conversation_cards_view{ cardsPane };
		cardsLayout->addWidget(Cards, 1);
		Notice = new QLabel{ cardsPane };
		Notice->setObjectName(QStringLiteral("conversationNotice"));
		Notice->setAccessibleName(QStringLiteral("Thread notice"));
		Notice->setWordWrap(true);
		Notice->hide();
		DiscussSummary = new QLabel{ cardsPane };
		DiscussSummary->setObjectName(QStringLiteral("conversationDiscussSummary"));
		DiscussSummary->setAccessibleName(QStringLiteral("Threads summary"));
		detail::set_plain_text_label(DiscussSummary);
		DiscussSummary->hide();
		cardsLayout->addWidget(Notice);
		cardsLayout->addWidget(DiscussSummary);
		const auto applyFilters = [this] {
			Cards->set_filters(discuss_filters{
				.UnreadOnly     = FilterUnread->isChecked(),
				.UnresolvedOnly = FilterUnresolved->isChecked(),
			});
		};
		connect(FilterUnread, &QToolButton::toggled, this, [applyFilters](bool) { applyFilters(); });
		connect(FilterUnresolved, &QToolButton::toggled, this, [applyFilters](bool) { applyFilters(); });
		Cards->set_layout_observer([this] { refresh_summary(); });
		connect(ScopeThisFile, &QToolButton::toggled, this, [this](const bool aChecked) {
			if (aChecked) {
				Cards->set_scope(cards_scope::this_file);
			}
		});
		connect(ScopeWholePr, &QToolButton::toggled, this, [this](const bool aChecked) {
			if (aChecked) {
				Cards->set_scope(cards_scope::whole_pull_request);
			}
		});
		Cards->set_thread_dispatch([this](const std::string& aIdentity) {
			const auto target = Model->index_for_id(aIdentity);
			if (target.isValid()) {
				View->setCurrentIndex(target);
			}
		});
		loaded->addWidget(cardsPane);
		auto* const mapPane   = new QWidget{ loaded };
		MapPane               = mapPane;
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
		loaded->setStretchFactor(0, 2);
		loaded->setStretchFactor(1, 0);
		loaded->setStretchFactor(2, 1);
		Stack->addWidget(loaded);
		// [[BPR-0649]]. Discuss's arrangement, one more page of the stack:
		// the tree above the map at the left, the feed at the right. The
		// widgets are the same ones; `set_discuss` moves them.
		DiscussSplitter = new QSplitter{ Qt::Horizontal, Stack };
		DiscussSplitter->setObjectName(QStringLiteral("conversationDiscussSplitter"));
		DiscussSplitter->setChildrenCollapsible(false);
		DiscussColumn = new QWidget{ DiscussSplitter };
		DiscussColumn->setObjectName(QStringLiteral("conversationDiscussColumn"));
		auto* const column = new QVBoxLayout{ DiscussColumn };
		column->setContentsMargins(0, 0, 0, 0);
		DiscussSplitter->addWidget(DiscussColumn);
		Stack->addWidget(DiscussSplitter);
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

		setTabOrder(RevealButton, ScopeThisFile);
		setTabOrder(ScopeThisFile, ScopeWholePr);
		setTabOrder(ScopeWholePr, Cards);
		setTabOrder(Cards, ThreadMap);
		setTabOrder(ThreadMap, View);
	}

	void github_conversation_panel::synchronize_map_focus()
	{
		// Not announced: the tree the reviewer moved in has already said what
		// is now selected, and the map echoing it makes one gesture speak twice.
		ThreadMap->set_focus_identity(
			Model->id_for_index(View->currentIndex()), false);
		ThreadMapBranch->setText(ThreadMap->branch_text());
		Cards->set_current_thread(Model->id_for_index(View->currentIndex()));
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
		Cards->set_anchor_file(AnchorFile);
		Cards->adopt(Retained);
		ThreadMapSummary->setText(ThreadMap->summary_text());
		HasThreads = !shown.empty();
		show_loaded_page();
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
		Cards->clear();
		ThreadMapSummary->setText(ThreadMap->summary_text());
		ThreadMapBranch->setText(ThreadMap->branch_text());
		HasThreads = false;
		Stack->setCurrentIndex(0);
	}

	void github_conversation_panel::show_loaded_page()
	{
		Stack->setCurrentIndex(!HasThreads ? 0 : Discuss ? 2
														 : 1);
	}

	void github_conversation_panel::refresh_summary()
	{
		if (DiscussSummary == nullptr || Cards == nullptr) {
			return;
		}
		DiscussSummary->setText(QStringLiteral("%1 threads, %2 unresolved, %3 new")
									.arg(Cards->thread_count())
									.arg(Cards->unresolved_count())
									.arg(Cards->unread_thread_count()));
	}

	void github_conversation_panel::set_discuss(const bool aDiscuss)
	{
		if (Discuss == aDiscuss) {
			return;
		}
		Discuss = aDiscuss;
		if (Discuss) {
			auto* const column = qobject_cast<QVBoxLayout*>(DiscussColumn->layout());
			column->addWidget(View, 1);
			column->addWidget(MapPane, 0);
			DiscussSplitter->insertWidget(1, CardsPane);
			DiscussSplitter->setStretchFactor(0, 0);
			DiscussSplitter->setStretchFactor(1, 1);
			DiscussSummary->show();
			Cards->set_feed(true);
		} else {
			Loaded->insertWidget(0, CardsPane);
			Loaded->insertWidget(1, MapPane);
			Loaded->insertWidget(2, View);
			DiscussSummary->hide();
			Cards->set_feed(false);
		}
		show_loaded_page();
	}

	auto github_conversation_panel::discuss() const noexcept -> bool
	{
		return Discuss;
	}

	void github_conversation_panel::apply_first_filter(const application::review_comments_filter aFilter)
	{
		if (FilterUnread == nullptr || FilterUnresolved == nullptr) {
			return;
		}
		FilterUnread->setChecked(aFilter == application::review_comments_filter::unread);
		FilterUnresolved->setChecked(aFilter == application::review_comments_filter::unresolved);
	}

	auto github_conversation_panel::filter_unread_button() const noexcept -> QToolButton*
	{
		return FilterUnread;
	}

	auto github_conversation_panel::filter_unresolved_button() const noexcept -> QToolButton*
	{
		return FilterUnresolved;
	}

	void github_conversation_panel::present_notice(const QString& aNotice)
	{
		Notice->setText(aNotice);
		Notice->setVisible(!aNotice.isEmpty());
	}

	auto github_conversation_panel::notice_label() const noexcept -> QLabel*
	{
		return Notice;
	}

	auto github_conversation_panel::discuss_summary() const noexcept -> QLabel*
	{
		return DiscussSummary;
	}

	auto github_conversation_panel::discuss_column() const noexcept -> QWidget*
	{
		return DiscussColumn;
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
	auto github_conversation_panel::cards() const noexcept -> conversation_cards_view*
	{
		return Cards;
	}

	auto github_conversation_panel::scope_this_file_button() const noexcept -> QToolButton*
	{
		return ScopeThisFile;
	}

	auto github_conversation_panel::scope_whole_pull_request_button() const noexcept -> QToolButton*
	{
		return ScopeWholePr;
	}
} // namespace bpr::qt
