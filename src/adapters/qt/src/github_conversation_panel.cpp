#include "bpr/qt/github_conversation_panel.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/github_conversation_delegate.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_thread_map.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include "github_presentation_text.hpp"

#include <QApplication>
#include <QComboBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace bpr::qt {
	namespace {
		/// A long post may exceed a page: keyboard navigation must scroll inside it.
		class conversation_detail_view final : public QTreeView
		{
		public:
			using QTreeView::QTreeView;
			std::function<void()> ReadBody;

		protected:
			bool focusNextPrevChild(const bool aNext) override
			{
				if (aNext && ReadBody) {
					ReadBody();
					return true;
				}
				return QTreeView::focusNextPrevChild(aNext);
			}
			void updateGeometries() override
			{
				QTreeView::updateGeometries();
				verticalScrollBar()->setSingleStep(fontMetrics().lineSpacing());
			}
			void keyPressEvent(QKeyEvent* aEvent) override
			{
				auto* const bar = verticalScrollBar();
				switch (aEvent->key()) {
				case Qt::Key_PageDown: bar->setValue(bar->value() + bar->pageStep()); break;
				case Qt::Key_PageUp:   bar->setValue(bar->value() - bar->pageStep()); break;
				case Qt::Key_Home:     bar->setValue(bar->minimum()); break;
				case Qt::Key_End:      bar->setValue(bar->maximum()); break;
				case Qt::Key_Space:
					bar->setValue(bar->value() + (aEvent->modifiers().testFlag(Qt::ShiftModifier) ? -1 : 1) * bar->pageStep());
					break;
				default: QTreeView::keyPressEvent(aEvent); return;
				}
				aEvent->accept();
			}
			void wheelEvent(QWheelEvent* aEvent) override
			{
				const auto delta = !aEvent->pixelDelta().isNull()
					? static_cast<double>(aEvent->pixelDelta().y())
					: static_cast<double>(aEvent->angleDelta().y()) * QApplication::wheelScrollLines() * fontMetrics().lineSpacing() / 120.0;
				WheelRemainder += delta;
				const auto pixels = static_cast<int>(WheelRemainder);
				WheelRemainder -= pixels;
				verticalScrollBar()->setValue(verticalScrollBar()->value() - pixels);
				// This viewport owns reading even at its boundary; a surrounding
				// drawer must not unexpectedly move when the final post is reached.
				aEvent->accept();
			}
			void resizeEvent(QResizeEvent* aEvent) override
			{
				const auto first    = indexAt(QPoint{ 1, 1 });
				const auto before   = visualRect(first);
				const auto fraction = before.height() > 0 ? static_cast<double>(-before.top()) / before.height() : 0.0;
				QTreeView::resizeEvent(aEvent);
				doItemsLayout();
				if (first.isValid()) {
					scrollTo(first, QAbstractItemView::PositionAtTop);
					verticalScrollBar()->setValue(verticalScrollBar()->value() + static_cast<int>(fraction * visualRect(first).height()));
				}
			}

		private:
			double WheelRemainder = 0.0;
		};
		[[nodiscard]] auto endpoint_is_current(
			const application::github_conversation_projection& aNode,
			const bool                                         aLeftHasCurrentDiffRevision,
			const bool                                         aRightHasCurrentDiffRevision) noexcept -> bool
		{
			if (aNode.SemanticAnchor) {
				return true;
			}
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

		/// \brief Whether a root is anchored at exactly this line of the current comparison.
		[[nodiscard]] auto anchored_to_line(
			const application::github_conversation_projection& aNode,
			const core::source_line_ref&                       aLine,
			const bool                                         aLeftHasCurrentDiffRevision,
			const bool                                         aRightHasCurrentDiffRevision) -> bool
		{
			if (aNode.SemanticAnchor) {
				return *aNode.SemanticAnchor == aLine;
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
			return side.has_value() && *side == aLine.Side
				&& *aNode.Line - 1U == aLine.Line;
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
		///
		/// `aFiles` holds the file identity of each node's path, index for index.
		[[nodiscard]] auto anchored_to(
			const std::vector<application::github_conversation_projection>&
														aConversations,
			const std::span<const core::file_identity>  aFiles,
			const std::optional<core::file_identity>&   aFile,
			const std::optional<core::source_line_ref>& aLine,
			const bool                                  aLeftHasCurrentDiffRevision,
			const bool                                  aRightHasCurrentDiffRevision)
			-> std::vector<application::github_conversation_projection>
		{
			auto byId = std::unordered_map<std::string_view, std::size_t>{ };
			byId.reserve(aConversations.size());
			for (auto index = std::size_t{ 0U }; index < aConversations.size();
				 ++index) {
				byId.emplace(aConversations[index].Id, index);
			}

			const auto rootMatches = [&](std::size_t aIndex) {
				const auto anchorMatches = [&](const std::size_t aNodeIndex) {
					const auto& node = aConversations[aNodeIndex];
					if (aFile.has_value()
						&& (node.Path.empty() || aFiles[aNodeIndex] != *aFile)) {
						return false;
					}
					return !aLine.has_value()
						|| anchored_to_line(node, *aLine, aLeftHasCurrentDiffRevision, aRightHasCurrentDiffRevision);
				};
				// Bounded by the node count, so a projection that somehow
				// contained a cycle cannot spin here.
				for (auto step = std::size_t{ 0U }; step <= aConversations.size();
					 ++step) {
					const auto& node = aConversations[aIndex];
					if (!node.ParentId.has_value()) {
						return anchorMatches(aIndex);
					}
					const auto parent = byId.find(*node.ParentId);
					if (parent == byId.end()) {
						return anchorMatches(aIndex);
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

		// Accepted content is shown automatically; there is no focus-only reveal action.
		auto* const header = new QVBoxLayout;
		header->addWidget(AvailabilityLabel);
		layout->addLayout(header);

		Stack = new QStackedWidget{ this };
		Stack->setObjectName(QStringLiteral("githubConversationState"));
		auto* const empty = new QWidget{ Stack };
		empty->setObjectName(QStringLiteral("githubConversationEmptyState"));
		Stack->addWidget(empty);

		Model = new github_conversation_model{ this };

		// The index selects a conversation, the graph locates a post, and
		// only the selected detail paints complete bodies.
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
		auto* const scopeRow = new QGridLayout;
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
		ScopeWholePr->setText(QStringLiteral("All PR conversations"));
		ScopeWholePr->setAccessibleName(QStringLiteral("Whole pull request"));
		ScopeWholePr->setAccessibleDescription(QStringLiteral(
			"Lists every thread of the pull request in file order"));
		ScopeWholePr->setCheckable(true);
		ScopeWholePr->setAutoExclusive(true);
		ScopeWholePr->setFocusPolicy(Qt::StrongFocus);
		scopeRow->addWidget(ScopeThisFile, 0, 0);
		scopeRow->addWidget(ScopeWholePr, 0, 1, 1, 2);
		DiscussFileChoice = new QComboBox{ cardsPane };
		DiscussFileChoice->setObjectName(QStringLiteral("conversationFileScope"));
		DiscussFileChoice->setAccessibleName(QStringLiteral("Conversation file scope"));
		DiscussFileChoice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		DiscussFileChoice->setMinimumContentsLength(12);
		DiscussFileChoice->hide();
		scopeRow->addWidget(DiscussFileChoice, 1, 0, 1, 3);
		connect(DiscussFileChoice, &QComboBox::currentTextChanged, this, [this](const QString& aPath) {
			DiscussFile = aPath.isEmpty() ? std::nullopt : std::optional{ core::file_identity_of(aPath.toStdString()) };
			if (Discuss) {
				refresh_projection();
			}
		});
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
		scopeRow->addWidget(FilterUnread, 2, 0);
		scopeRow->addWidget(FilterUnresolved, 2, 1);
		auto* const clearFilters = new QToolButton{ cardsPane };
		clearFilters->setObjectName(QStringLiteral("conversationClearFilters"));
		clearFilters->setText(QStringLiteral("Clear"));
		clearFilters->setAccessibleName(QStringLiteral("Clear filters"));
		clearFilters->setToolTip(QStringLiteral("Clear filters"));
		clearFilters->setFocusPolicy(Qt::StrongFocus);
		connect(clearFilters, &QToolButton::clicked, this, [this] {
			ScopeWholePr->setChecked(true);
			FilterUnread->setChecked(false);
			FilterUnresolved->setChecked(false);
		});
		scopeRow->addWidget(clearFilters, 2, 2);
		cardsLayout->addLayout(scopeRow);
		Cards = new conversation_cards_view{ cardsPane };
		Cards->set_compact(true);
		Cards->setMinimumHeight(fontMetrics().lineSpacing() * 6);
		Cards->setAccessibleName(QStringLiteral("Conversation index"));
		cardsLayout->addWidget(Cards, 1);
		Notice = new QLabel{ cardsPane };
		Notice->setObjectName(QStringLiteral("conversationNotice"));
		Notice->setAccessibleName(QStringLiteral("Thread notice"));
		Notice->setWordWrap(true);
		Notice->hide();
		DiscussSummary = new QLabel{ cardsPane };
		DiscussSummary->setObjectName(QStringLiteral("conversationDiscussSummary"));
		DiscussSummary->setAccessibleName(QStringLiteral("Threads summary"));
		DiscussSummary->setWordWrap(true);
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
		Cards->set_layout_observer([this] {
			VisibleRoots.clear();
			for (const auto& card : Cards->cards()) {
				VisibleRoots.insert(card.Id);
			}
			refresh_summary();
			if (View != nullptr) {
				refresh_detail();
			}
		});
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
		Cards->set_thread_dispatch([this](const std::string& aIdentity, const bool aOpenDetail) {
			const auto target = Model->index_for_id(aIdentity);
			if (target.isValid()) {
				choose(target);
				if (aOpenDetail) {
					DetailOpened = true;
					arrange_discuss();
					View->setFocus(Qt::OtherFocusReason);
				}
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

		DetailPane         = new QWidget{ loaded };
		auto* detailLayout = new QVBoxLayout{ DetailPane };
		detailLayout->setContentsMargins(0, 0, 0, 0);
		auto* postView = new conversation_detail_view{ DetailPane };
		View           = postView;
		View->setObjectName(QStringLiteral("githubConversationView"));
		View->setAccessibleName(QStringLiteral("Selected conversation detail"));
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
		View->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
		View->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		View->header()->setSectionResizeMode(QHeaderView::Stretch);
		View->setTextElideMode(Qt::ElideNone);
		View->header()->hide();
		View->setItemDelegate(new github_conversation_delegate{ View, true });
		View->setMaximumHeight(fontMetrics().lineSpacing() * 7);
		View->setAccessibleName(QStringLiteral("Conversation posts"));
		View->setAccessibleDescription(QStringLiteral("Select a post or reply to read its complete Markdown body below."));
		detailLayout->addWidget(View);
		Body = new markdown_body_view{ DetailPane };
		Body->setObjectName(QStringLiteral("conversationMarkdownBody"));
		Body->setAccessibleName(QStringLiteral("Selected public conversation body"));
		postView->ReadBody = [this] { Body->setFocus(Qt::TabFocusReason); };
		detailLayout->addWidget(Body, 1);
		SupportDetails = new QToolButton{ DetailPane };
		SupportDetails->setObjectName(QStringLiteral("conversationSupportDetails"));
		SupportDetails->setText(QStringLiteral("Support details"));
		SupportDetails->setAccessibleName(QStringLiteral("Conversation support details"));
		SupportDetails->setCheckable(true);
		SupportDetails->setFocusPolicy(Qt::StrongFocus);
		SupportDetails->hide();
		detailLayout->addWidget(SupportDetails);
		SupportDescription = new QLabel{ DetailPane };
		SupportDescription->setObjectName(QStringLiteral("conversationSupportDescription"));
		detail::set_plain_text_label(SupportDescription);
		SupportDescription->setWordWrap(true);
		SupportDescription->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		SupportDescription->setFocusPolicy(Qt::StrongFocus);
		QWidget::setTabOrder(SupportDetails, SupportDescription);
		SupportDescription->setText(QStringLiteral("GitHub stores this as a file review comment. The code link in its current root post places the conversation at this source line."));
		SupportDescription->hide();
		detailLayout->addWidget(SupportDescription);
		connect(SupportDetails, &QToolButton::toggled, this, [this](const bool aExpanded) {
			SupportDescription->setVisible(aExpanded);
		});
		QWidget::setTabOrder(Body, SupportDetails);
		QWidget::setTabOrder(View, Body);
		loaded->addWidget(DetailPane);
		// The map asks for the band its own sizeHint names; the cards and
		// selected detail share the remaining height. Left to stretch factors
		// alone the map collapsed to its minimum in an ordinary window, which showed a highlighted branch
		// whose end was off-screen - the exact failure the owner's
		// "show the full branch" direction is about.
		loaded->setStretchFactor(0, 1);
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
		BackToIndex = new QToolButton{ DiscussColumn };
		BackToIndex->setObjectName(QStringLiteral("conversationBackToIndex"));
		BackToIndex->setText(QStringLiteral("Back to conversations"));
		BackToIndex->setFocusPolicy(Qt::StrongFocus);
		column->addWidget(BackToIndex);
		connect(BackToIndex, &QToolButton::clicked, this, [this] {
			DetailOpened = false;
			arrange_discuss();
			Cards->setFocus(Qt::OtherFocusReason);
		});
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
				choose(target);
			}
		});
		connect(
			View->selectionModel(),
			&QItemSelectionModel::currentChanged,
			this,
			[this](const QModelIndex& aIndex) {
				// Only the panel's own restores are silent, so a change heard
				// here is never the default row.
				DefaultCurrent = false;
				synchronize_map_focus();
				View->expandAll();
				View->scrollTo(aIndex, QAbstractItemView::PositionAtTop);
			});

		setTabOrder(ScopeThisFile, ScopeWholePr);
		setTabOrder(ScopeWholePr, DiscussFileChoice);
		setTabOrder(DiscussFileChoice, FilterUnread);
		setTabOrder(FilterUnread, FilterUnresolved);
		setTabOrder(FilterUnresolved, clearFilters);
		setTabOrder(clearFilters, Cards);
		setTabOrder(Cards, BackToIndex);
		setTabOrder(BackToIndex, ThreadMap);
		setTabOrder(ThreadMap, View);
	}

	void github_conversation_panel::synchronize_map_focus()
	{
		refresh_detail();
		// Not announced: the tree the reviewer moved in has already said what
		// is now selected, and the map echoing it makes one gesture speak twice.
		ThreadMap->set_focus_identity(
			Model->id_for_index(View->currentIndex()), false);
		ThreadMapBranch->setText(ThreadMap->branch_text());
		Cards->set_current_thread(DetailRoot);
	}

	void github_conversation_panel::arrange_discuss()
	{
		if (!Discuss) {
			CardsPane->show();
			return;
		}
		const auto narrow = width() < fontMetrics().averageCharWidth() * 85;
		BackToIndex->setVisible(narrow);
		CardsPane->setVisible(!narrow || !DetailOpened);
		DiscussColumn->setVisible(!narrow || DetailOpened);
		MapPane->setMaximumHeight(fontMetrics().lineSpacing() * 10);
	}

	void github_conversation_panel::resizeEvent(QResizeEvent* aEvent)
	{
		QWidget::resizeEvent(aEvent);
		arrange_discuss();
	}

	void github_conversation_panel::refresh_detail()
	{
		auto root = View->currentIndex();
		while (root.parent().isValid()) {
			root = root.parent();
		}
		const auto rootId   = Model->id_for_index(root);
		const auto admitted = !Discuss || VisibleRoots.contains(rootId);
		if (Discuss) {
			DiscussSelection = Model->id_for_index(View->currentIndex());
		}
		if (!DetailDirty && DetailRoot == rootId && DetailAdmitted == admitted) {
			refresh_body(admitted);
			return;
		}
		if (DetailDirty) {
			for (auto row = 0; row < Model->rowCount(); ++row) {
				View->setRowHidden(row, { }, !admitted || Model->index(row, 0) != root);
			}
		} else {
			const auto previousRoot = Model->index_for_id(DetailRoot);
			if (previousRoot.isValid()) {
				View->setRowHidden(previousRoot.row(), { }, true);
			}
			if (root.isValid()) {
				View->setRowHidden(root.row(), { }, !admitted);
			}
		}
		DetailDirty    = false;
		DetailAdmitted = admitted;
		DetailRoot     = rootId;
		refresh_body(admitted);
		auto       thread  = std::vector<application::github_conversation_projection>{ };
		const auto collect = [&](const QModelIndex& aIndex, auto& aSelf) -> void {
			const auto found = ShownPositions.find(Model->id_for_index(aIndex));
			if (found != ShownPositions.end()) {
				thread.push_back(Shown[found->second]);
			}
			for (auto row = 0; row < Model->rowCount(aIndex); ++row) {
				aSelf(Model->index(row, 0, aIndex), aSelf);
			}
		};
		if (admitted && root.isValid()) {
			collect(root, collect);
		}
		ThreadMap->adopt(thread);
		ThreadMapSummary->setText(ThreadMap->summary_text());
	}

	void github_conversation_panel::adopt(
		const std::vector<application::github_conversation_projection>&
				aConversations,
		QString aAvailabilityText)
	{
		AvailabilityLabel->setText(std::move(aAvailabilityText));
		if (Retained == aConversations) {
			show_loaded_page();
			synchronize_map_focus();
			return;
		}
		Retained            = aConversations;
		const auto filePath = DiscussFileChoice->currentText();
		{
			const QSignalBlocker blocker{ DiscussFileChoice };
			DiscussFileChoice->clear();
			auto paths = std::set<std::string>{ };
			for (const auto& node : Retained) {
				if (!node.ParentId && !node.Path.empty()) {
					paths.insert(node.Path);
				}
			}
			for (const auto& path : paths) {
				DiscussFileChoice->addItem(QString::fromStdString(path));
			}
			const auto restored = DiscussFileChoice->findText(filePath);
			if (restored >= 0) {
				DiscussFileChoice->setCurrentIndex(restored);
			}
		}
		DiscussFile = DiscussFileChoice->currentText().isEmpty() ? std::nullopt
																 : std::optional{ core::file_identity_of(DiscussFileChoice->currentText().toStdString()) };
		index_retained();
		refresh_projection();
	}

	auto github_conversation_panel::has_semantic_anchor(const core::source_line_ref& aLine) const -> bool
	{
		return SemanticAnchors.contains(aLine);
	}

	void github_conversation_panel::index_retained()
	{
		SemanticAnchors.clear();
		PlacedStale = true;
		EarlierRetained.clear();
		CurrentThreadCounts.clear();
		RetainedFiles.clear();
		RetainedRoot.clear();
		RetainedByRootFile.clear();
		RetainedFiles.reserve(Retained.size());
		auto byId = std::unordered_map<std::string_view, std::size_t>{ };
		byId.reserve(Retained.size());
		for (std::size_t index = 0U; index < Retained.size(); ++index) {
			if (Retained[index].SemanticAnchor) {
				SemanticAnchors.insert(*Retained[index].SemanticAnchor);
			}
			RetainedFiles.push_back(core::file_identity_of(Retained[index].Path));
			byId.emplace(Retained[index].Id, index);
		}
		// Membership is decided at the root, as `anchored_to` decides it: a
		// parent that is not retained makes its child a root, and a chain
		// longer than the forest is a cycle with no root.
		RetainedRoot.reserve(Retained.size());
		for (std::size_t index = 0U; index < Retained.size(); ++index) {
			auto root = std::optional<std::size_t>{ };
			auto node = index;
			for (auto step = std::size_t{ 0U }; step <= Retained.size(); ++step) {
				const auto& current = Retained[node];
				const auto  parent  = current.ParentId.has_value() ? byId.find(*current.ParentId) : byId.end();
				if (parent == byId.end()) {
					root = node;
					break;
				}
				node = parent->second;
			}
			RetainedRoot.push_back(root);
			const auto current = root.has_value() && Retained[*root].Kind == application::github_conversation_kind::review_thread
				&& Retained[*root].AnchorAvailability == application::github_conversation_anchor_availability::current;
			if (!current || Retained[*root].SemanticAnchor) {
				EarlierRetained.push_back(Retained[index]);
			}
			if (current && *root == index && !Retained[*root].SemanticAnchor) {
				++CurrentThreadCounts[RetainedFiles[index]];
			}
			if (root.has_value() && !Retained[*root].Path.empty()) {
				RetainedByRootFile.emplace_back(RetainedFiles[*root], index);
			}
		}
		std::ranges::sort(RetainedByRootFile);
	}

	auto github_conversation_panel::anchored() -> std::vector<application::github_conversation_projection>
	{
		if (!AnchorFile.has_value()) {
			// A line without a file is judged node by node, as before.
			ProjectionNodesVisited += Retained.size();
			return anchored_to(Retained, RetainedFiles, AnchorFile, AnchorLine, LeftHasCurrentDiffRevision, RightHasCurrentDiffRevision);
		}
		auto kept = std::vector<application::github_conversation_projection>{ };
		for (const auto& [file, index] : std::ranges::equal_range(RetainedByRootFile, *AnchorFile, { }, &std::pair<core::file_identity, std::size_t>::first)) {
			++ProjectionNodesVisited;
			const auto root = *RetainedRoot[index];
			if (!LeftHasCurrentDiffRevision && !RightHasCurrentDiffRevision && !Retained[root].SemanticAnchor
				&& Retained[root].Kind == application::github_conversation_kind::review_thread
				&& Retained[root].AnchorAvailability == application::github_conversation_anchor_availability::current) {
				continue;
			}
			if (AnchorLine.has_value() && !anchored_to_line(Retained[root], *AnchorLine, LeftHasCurrentDiffRevision, RightHasCurrentDiffRevision)) {
				continue;
			}
			kept.push_back(Retained[index]);
		}
		return kept;
	}

	auto github_conversation_panel::projection_nodes_visited() const noexcept -> std::uint64_t
	{
		return ProjectionNodesVisited;
	}

	void github_conversation_panel::refresh_body(const bool aAdmitted)
	{
		auto index = View->currentIndex();
		if (index.data(github_conversation_model::kind_role).toInt() == static_cast<int>(application::github_conversation_kind::review_thread)
			&& Model->rowCount(index) > 0) {
			index = Model->index(0, 0, index);
		}
		const auto id         = Model->id_for_index(index);
		const auto found      = ShownPositions.find(id);
		const auto hasSupport = aAdmitted && found != ShownPositions.end()
			&& !Shown[found->second].Deleted && Shown[found->second].SemanticAnchor
			&& Shown[found->second].RemoteSubject == application::github_conversation_remote_subject::file;
		if (SupportPost != id || !hasSupport) {
			SupportDetails->setChecked(false);
			SupportPost = id;
		}
		SupportDetails->setVisible(hasSupport);
		SupportDescription->setVisible(hasSupport && SupportDetails->isChecked());
		if (!aAdmitted || found == ShownPositions.end() || Shown[found->second].Deleted) {
			Body->clear();
			Body->set_link_dispatch({ });
			return;
		}
		const auto& post = Shown[found->second];
		Body->set_body(post.ReviewerProse.value_or(post.Body), post.Id);
		Body->setAccessibleName(QStringLiteral("Public post by %1").arg(QString::fromStdString(post.AuthorLogin)));
		Body->set_link_dispatch([this, id](const std::string& aDestination) {
			if (LinkDispatch) {
				LinkDispatch(id, aDestination);
			}
		});
	}
	auto github_conversation_panel::body_view() const noexcept -> markdown_body_view* { return Body; }
	void github_conversation_panel::set_link_dispatch(std::function<void(const std::string&, const std::string&)> aDispatch)
	{
		LinkDispatch = std::move(aDispatch);
	}
	void github_conversation_panel::adopt_appearance(const appearance_profile& aProfile)
	{
		Body->set_code_font(aProfile.code_font(), aProfile.code_tab_stop_distance());
	}

	void github_conversation_panel::refresh_projection()
	{
		// Restoring by identity keeps the reviewer on the same reply.
		const auto selected      = Model->id_for_index(View->currentIndex());
		const auto readingOffset = View->verticalScrollBar()->value();
		const auto readingIndex  = View->indexAt(QPoint{ 1, 1 });
		const auto readingPost   = Model->id_for_index(readingIndex);
		const auto postOffset    = -View->visualRect(readingIndex).top();
		auto       shown         = !Discuss && (AnchorFile.has_value() || AnchorLine.has_value())
			? anchored()
			: Retained;
		shown                    = with_endpoint_placement(
			std::move(shown),
			LeftHasCurrentDiffRevision,
			RightHasCurrentDiffRevision);
		// [[BPR-0972]]. The cards take the whole placed forest only when it
		// changed; a new anchor alone is a new anchor file.
		if (PlacedStale) {
			PlacedRetained = with_endpoint_placement(!Discuss && AnchorFile && !LeftHasCurrentDiffRevision && !RightHasCurrentDiffRevision ? EarlierRetained : Retained, LeftHasCurrentDiffRevision, RightHasCurrentDiffRevision);
			PlacedStale    = false;
			Cards->adopt(PlacedRetained, Discuss ? DiscussFile : AnchorFile);
		} else {
			Cards->set_anchor_file(Discuss ? DiscussFile : AnchorFile);
		}
		if (Shown == shown) {
			show_loaded_page();
			synchronize_map_focus();
			return;
		}
		Shown = std::move(shown);
		ShownPositions.clear();
		for (std::size_t index = 0; index < Shown.size(); ++index) {
			ShownPositions.emplace(Shown[index].Id, index);
		}
		DetailDirty = true;
		const QSignalBlocker selectionBlocker{ View->selectionModel() };
		Model->apply(Shown);
		HasThreads = !Shown.empty();
		// Select without QAbstractItemView's implicit scroll/layout. The detail
		// must hide unrelated roots before any native layout measures them.
		auto restored = Model->index_for_id(selected);
		if (!restored.isValid()) {
			restored = View->currentIndex();
		}
		if (!restored.isValid() && Model->rowCount() > 0) {
			restored       = Model->index(0, 0);
			DefaultCurrent = true;
		}
		View->selectionModel()->setCurrentIndex(restored, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		// Set explicitly rather than through the signal, which the restore
		// above deliberately blocks: the map has to end an adopt showing the
		// branch the reviewer was reading, not the one before the refresh.
		synchronize_map_focus();
		show_loaded_page();
		if (ExpandOnNextProjection && !Shown.empty()) {
			View->expandAll();
			ExpandOnNextProjection = false;
		}
		View->doItemsLayout();
		const auto restoredReading = Model->index_for_id(readingPost);
		View->verticalScrollBar()->setValue(restoredReading.isValid()
												? View->verticalScrollBar()->value() + View->visualRect(restoredReading).top() + postOffset
												: readingOffset);
	}

	void github_conversation_panel::clear(QString aAvailabilityText)
	{
		ExpandOnNextProjection = true;
		Body->clear();
		Body->set_link_dispatch({ });
		DetailRoot.clear();
		DiscussSelection.clear();
		DefaultCurrent       = false;
		DiscussReadingOffset = 0;
		DiscussScope         = cards_scope::whole_pull_request;
		DiscussFilters       = { };
		DiscussInitialized   = false;
		DetailOpened         = false;
		DiscussFile.reset();
		{
			const QSignalBlocker blocker{ DiscussFileChoice };
			DiscussFileChoice->clear();
		}
		FilterUnread->setChecked(false);
		FilterUnresolved->setChecked(false);
		(Discuss ? ScopeWholePr : ScopeThisFile)->setChecked(true);
		Retained.clear();
		SemanticAnchors.clear();
		index_retained();
		Shown.clear();
		ShownPositions.clear();
		DetailDirty = true;
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
		// [[BPR-1214]]. The page follows the snapshot, not the selected line:
		// a line no thread is on still shows the cards, their scope and
		// filters. Only the map and the detail, which have nothing to show,
		// step aside.
		Stack->setCurrentIndex(Retained.empty() ? 0 : Discuss ? 2
															  : 1);
		MapPane->setVisible(HasThreads);
		DetailPane->setVisible(HasThreads);
	}

	void github_conversation_panel::refresh_summary()
	{
		if (DiscussSummary == nullptr || Cards == nullptr) {
			return;
		}
		DiscussSummary->setText(QStringLiteral("%1 conversations, %2 unresolved, %3 new")
									.arg(Cards->thread_count())
									.arg(Cards->unresolved_count())
									.arg(Cards->unread_thread_count()));
		if (Cards->cards().empty() && !Retained.empty()) {
			DiscussSummary->setText(QStringLiteral("No conversations match these filters. Clear filters to show all PR conversations."));
		}
		if (Cards->scope() == cards_scope::this_file && (Discuss ? DiscussFile : AnchorFile)) {
			DiscussSummary->setText(DiscussSummary->text() + QStringLiteral(" · File scope excludes pathless PR comments and review summaries."));
		}
	}

	void github_conversation_panel::set_discuss(const bool aDiscuss)
	{
		if (Discuss == aDiscuss) {
			return;
		}
		const auto remembered = DiscussSelection;
		if (Discuss) {
			DiscussReadingOffset = View->verticalScrollBar()->value();
			DiscussScope         = Cards->scope();
			DiscussFilters       = Cards->filters();
		}
		Discuss     = aDiscuss;
		PlacedStale = true;
		PlacedStale = true;
		DiscussFileChoice->setVisible(Discuss);
		ScopeThisFile->setText(Discuss ? QStringLiteral("File scope") : QStringLiteral("This file"));
		ScopeThisFile->setAccessibleName(ScopeThisFile->text());
		ScopeThisFile->setAccessibleDescription(Discuss
													? QStringLiteral("Use the selected conversation file; excludes pathless PR comments and review summaries")
													: QStringLiteral("Shows conversations of the file the comparison shows"));
		if (Discuss) {
			if (!DiscussInitialized) {
				ScopeWholePr->setChecked(true);
				FilterUnread->setChecked(false);
				FilterUnresolved->setChecked(false);
				DiscussInitialized = true;
			} else {
				(DiscussScope == cards_scope::whole_pull_request ? ScopeWholePr : ScopeThisFile)->setChecked(true);
				FilterUnread->setChecked(DiscussFilters.UnreadOnly);
				FilterUnresolved->setChecked(DiscussFilters.UnresolvedOnly);
			}
			auto* const column = qobject_cast<QVBoxLayout*>(DiscussColumn->layout());
			column->addWidget(DetailPane, 1);
			column->addWidget(MapPane, 0);
			DiscussSplitter->insertWidget(0, CardsPane);
			DiscussSplitter->setStretchFactor(0, 0);
			DiscussSplitter->setStretchFactor(1, 1);
			DiscussSummary->show();
			Cards->set_feed(true);
		} else {
			ScopeThisFile->setChecked(true);
			Loaded->insertWidget(0, CardsPane);
			Loaded->insertWidget(1, MapPane);
			Loaded->insertWidget(2, DetailPane);
			DiscussSummary->hide();
			Cards->set_feed(false);
		}
		refresh_projection();
		if (Discuss && !remembered.empty()) {
			const auto selected = Model->index_for_id(remembered);
			if (selected.isValid()) {
				View->setCurrentIndex(selected);
			}
		}
		QWidget::setTabOrder(View, Body);
		show_loaded_page();
		arrange_discuss();
		if (Discuss) {
			View->doItemsLayout();
			View->verticalScrollBar()->setValue(DiscussReadingOffset);
		}
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
		// An absent file is the review-wide projection, not an older pair.
		// Changing between files keeps the retained placed forest reusable.
		PlacedStale = PlacedStale || AnchorFile.has_value() != aFile.has_value();
		AnchorFile  = std::move(aFile);
		refresh_projection();
	}

	void github_conversation_panel::set_anchor_line(
		std::optional<core::source_line_ref> aLine)
	{
		if (AnchorLine == aLine) {
			return;
		}
		AnchorLine = std::move(aLine);
		refresh_projection();
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
		PlacedStale                 = true;
		refresh_projection();
	}

	auto github_conversation_panel::anchor_file() const noexcept
		-> const std::optional<core::file_identity>&
	{
		return AnchorFile;
	}

	auto github_conversation_panel::current_head_thread_count(const core::file_identity& aFile) const -> std::size_t
	{
		const auto found = CurrentThreadCounts.find(aFile);
		return found == CurrentThreadCounts.end() ? 0U : found->second;
	}

	auto github_conversation_panel::anchor_line() const noexcept
		-> const std::optional<core::source_line_ref>&
	{
		return AnchorLine;
	}

	void github_conversation_panel::set_reveal_action(QAction* const aAction)
	{
		static_cast<void>(aAction);
	}

	void github_conversation_panel::relayout()
	{
		View->setMaximumHeight(fontMetrics().lineSpacing() * 7);
		View->doItemsLayout();
		arrange_discuss();
	}

	auto github_conversation_panel::current_is_default() const noexcept -> bool
	{
		return DefaultCurrent;
	}

	void github_conversation_panel::choose(const QModelIndex& aIndex)
	{
		if (DefaultCurrent && aIndex.isValid() && View->currentIndex() == aIndex) {
			// Setting the same row again would say nothing.
			const QSignalBlocker blocker{ View->selectionModel() };
			View->setCurrentIndex({ });
		}
		View->setCurrentIndex(aIndex);
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
