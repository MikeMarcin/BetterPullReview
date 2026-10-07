#include "bpr/qt/github_conversation_panel.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/github_conversation_model.hpp"

#include "conversation_glyphs.hpp"
#include "github_presentation_text.hpp"

#include <QApplication>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace bpr::qt {
	namespace {
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
		layout->setSpacing(0);

		AvailabilityLabel = new QLabel{ this };
		AvailabilityLabel->setObjectName(
			QStringLiteral("reviewThreadAvailability"));
		AvailabilityLabel->setAccessibleName(
			QStringLiteral("Review-thread availability"));
		AvailabilityLabel->setWordWrap(true);
		AvailabilityLabel->setContentsMargins(8, 6, 8, 6);
		detail::set_plain_text_label(AvailabilityLabel);
		layout->addWidget(AvailabilityLabel);

		Stack = new QStackedWidget{ this };
		Stack->setObjectName(QStringLiteral("githubConversationState"));
		auto* const empty = new QWidget{ Stack };
		empty->setObjectName(QStringLiteral("githubConversationEmptyState"));
		Stack->addWidget(empty);

		Model = new github_conversation_model{ this };

		auto* const loaded = new QWidget{ Stack };
		loaded->setObjectName(QStringLiteral("githubConversationLoaded"));
		auto* const column = new QVBoxLayout{ loaded };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);

		// Discuss's row: scope, the file, and its two filters. Beside the code
		// the drawer shows this file only, and its filters sit on the tab row.
		DiscussControls = new QWidget{ loaded };
		DiscussControls->setObjectName(QStringLiteral("conversationDiscussControls"));
		DiscussGrid = new QGridLayout{ DiscussControls };
		DiscussGrid->setContentsMargins(8, 4, 8, 4);
		DiscussGrid->setSpacing(4);
		ScopeThisFile = new QToolButton{ DiscussControls };
		ScopeThisFile->setObjectName(QStringLiteral("conversationScopeThisFile"));
		ScopeThisFile->setText(QStringLiteral("File scope"));
		ScopeThisFile->setAccessibleName(QStringLiteral("File scope"));
		ScopeThisFile->setAccessibleDescription(QStringLiteral(
			"Use the selected conversation file"));
		ScopeThisFile->setCheckable(true);
		ScopeThisFile->setChecked(true);
		ScopeThisFile->setAutoExclusive(true);
		ScopeThisFile->setFocusPolicy(Qt::StrongFocus);
		ScopeWholePr = new QToolButton{ DiscussControls };
		ScopeWholePr->setObjectName(QStringLiteral("conversationScopeWholePullRequest"));
		ScopeWholePr->setText(QStringLiteral("All PR conversations"));
		ScopeWholePr->setAccessibleName(QStringLiteral("Whole pull request"));
		ScopeWholePr->setAccessibleDescription(QStringLiteral(
			"Lists every thread of the pull request in file order"));
		ScopeWholePr->setCheckable(true);
		ScopeWholePr->setAutoExclusive(true);
		ScopeWholePr->setFocusPolicy(Qt::StrongFocus);
		DiscussGrid->addWidget(ScopeThisFile, 0, 0);
		DiscussGrid->addWidget(ScopeWholePr, 0, 1, 1, 2);
		DiscussFileChoice = new QComboBox{ DiscussControls };
		DiscussFileChoice->setObjectName(QStringLiteral("conversationFileScope"));
		DiscussFileChoice->setAccessibleName(QStringLiteral("Conversation file scope"));
		DiscussFileChoice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		DiscussFileChoice->setMinimumContentsLength(12);
		DiscussGrid->addWidget(DiscussFileChoice, 1, 0, 1, 3);
		connect(DiscussFileChoice, &QComboBox::currentTextChanged, this, [this](const QString& aPath) {
			DiscussFile = aPath.isEmpty() ? std::nullopt : std::optional{ core::file_identity_of(aPath.toStdString()) };
			if (Discuss) {
				refresh_projection();
			}
		});
		auto* const clearFilters = new QToolButton{ DiscussControls };
		clearFilters->setObjectName(QStringLiteral("conversationClearFilters"));
		clearFilters->setText(QStringLiteral("Clear"));
		clearFilters->setAccessibleName(QStringLiteral("Clear filters"));
		clearFilters->setToolTip(QStringLiteral("Clear filters"));
		clearFilters->setFocusPolicy(Qt::StrongFocus);
		DiscussGrid->addWidget(clearFilters, 2, 2);
		column->addWidget(DiscussControls);
		DiscussControls->hide();

		// [[BPR-1290]]. Only new and Hide resolved, as two icons on the tab row.
		ReadTools = new QWidget{ this };
		ReadTools->setObjectName(QStringLiteral("conversationReadTools"));
		ReadToolsRow = new QHBoxLayout{ ReadTools };
		ReadToolsRow->setContentsMargins(0, 0, 0, 0);
		ReadToolsRow->setSpacing(1);
		FilterUnread = new QToolButton{ ReadTools };
		FilterUnread->setObjectName(QStringLiteral("conversationFilterUnread"));
		FilterUnread->setCheckable(true);
		FilterUnread->setFocusPolicy(Qt::StrongFocus);
		FilterUnresolved = new QToolButton{ ReadTools };
		FilterUnresolved->setObjectName(QStringLiteral("conversationFilterUnresolved"));
		FilterUnresolved->setCheckable(true);
		FilterUnresolved->setFocusPolicy(Qt::StrongFocus);
		ReadToolsRow->addWidget(FilterUnread);
		ReadToolsRow->addWidget(FilterUnresolved);
		connect(clearFilters, &QToolButton::clicked, this, [this] {
			ScopeWholePr->setChecked(true);
			FilterUnread->setChecked(false);
			FilterUnresolved->setChecked(false);
			clear_listing();
		});
		// [[BPR-1089]]. An Overview count's drill-down: its caption, and a
		// glyph that shows every conversation again.
		ListingRow = new QWidget{ DiscussControls };
		ListingRow->setObjectName(QStringLiteral("conversationListingRow"));
		auto* const listingLayout = new QHBoxLayout{ ListingRow };
		listingLayout->setContentsMargins(0, 0, 0, 0);
		listingLayout->setSpacing(4);
		// The count's own mark leads, as on its chip; × follows the words.
		ListingMark = new QLabel{ ListingRow };
		ListingMark->setObjectName(QStringLiteral("conversationListingMark"));
		ListingCaption = new QLabel{ ListingRow };
		ListingCaption->setObjectName(QStringLiteral("conversationListingCaption"));
		// One line at its own width, so × follows the words; it may shrink
		// in a narrow panel rather than widen it.
		ListingCaption->setMinimumWidth(1);
		detail::set_plain_text_label(ListingCaption);
		ClearListing = new QToolButton{ ListingRow };
		ClearListing->setObjectName(QStringLiteral("conversationClearListing"));
		ClearListing->setText(QStringLiteral("×"));
		ClearListing->setAutoRaise(true);
		ClearListing->setToolTip(QStringLiteral("Show every conversation"));
		ClearListing->setAccessibleName(QStringLiteral("Show every conversation"));
		ClearListing->setFocusPolicy(Qt::StrongFocus);
		listingLayout->addWidget(ListingMark);
		listingLayout->addWidget(ListingCaption);
		listingLayout->addWidget(ClearListing);
		listingLayout->addStretch(1);
		DiscussGrid->addWidget(ListingRow, 3, 0, 1, 3);
		ListingRow->hide();
		connect(ClearListing, &QToolButton::clicked, this, [this] { clear_listing(); });

		Cards = new conversation_cards_view{ loaded };
		Cards->setMinimumHeight(fontMetrics().lineSpacing() * 6);
		column->addWidget(Cards, 1);
		Notice = new QLabel{ loaded };
		Notice->setObjectName(QStringLiteral("conversationNotice"));
		Notice->setAccessibleName(QStringLiteral("Thread notice"));
		Notice->setWordWrap(true);
		Notice->setContentsMargins(8, 4, 8, 4);
		Notice->hide();
		DiscussSummary = new QLabel{ loaded };
		DiscussSummary->setObjectName(QStringLiteral("conversationDiscussSummary"));
		DiscussSummary->setAccessibleName(QStringLiteral("Threads summary"));
		DiscussSummary->setWordWrap(true);
		DiscussSummary->setContentsMargins(8, 4, 8, 4);
		detail::set_plain_text_label(DiscussSummary);
		DiscussSummary->hide();
		column->addWidget(Notice);
		column->addWidget(DiscussSummary);
		Stack->addWidget(loaded);
		layout->addWidget(Stack, 1);

		connect(FilterUnread, &QToolButton::toggled, this, [this](bool) { apply_filters(); });
		connect(FilterUnresolved, &QToolButton::toggled, this, [this](bool) { apply_filters(); });
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
		Cards->set_thread_dispatch([this](const std::string& aIdentity, bool) {
			auto target = Model->index_for_id(aIdentity);
			if (!target.isValid() && AnchorLine.has_value()) {
				// The selection authority holds only the selected line's
				// threads; a thread on another line widens it to the file.
				AnchorLine.reset();
				refresh_projection();
				target = Model->index_for_id(aIdentity);
			}
			if (target.isValid()) {
				choose(target);
			}
		});

		// The selection authority. It is never shown: the notes are what the
		// reviewer reads and chooses with, and they move this current index.
		View = new QTreeView{ this };
		View->setObjectName(QStringLiteral("githubConversationView"));
		View->setAccessibleName(QStringLiteral("Selected conversation"));
		View->setModel(Model);
		View->setSelectionBehavior(QAbstractItemView::SelectRows);
		View->setSelectionMode(QAbstractItemView::SingleSelection);
		View->setEditTriggers(QAbstractItemView::NoEditTriggers);
		View->setHeaderHidden(true);
		View->setFocusPolicy(Qt::NoFocus);
		View->hide();
		connect(
			View->selectionModel(),
			&QItemSelectionModel::currentChanged,
			this,
			[this](const QModelIndex&) {
				// Only the panel's own restores are silent, so a change heard
				// here is never the default row.
				DefaultCurrent = false;
				synchronize_current();
			});
		arrange_filters();
		setTabOrder(ScopeThisFile, ScopeWholePr);
		setTabOrder(ScopeWholePr, DiscussFileChoice);
		setTabOrder(DiscussFileChoice, clearFilters);
		setTabOrder(clearFilters, ClearListing);
		setTabOrder(ClearListing, Cards);
	}

	void github_conversation_panel::apply_filters()
	{
		auto filters           = discuss_filters{ };
		filters.UnreadOnly     = FilterUnread->isChecked();
		filters.UnresolvedOnly = FilterUnresolved->isChecked();
		filters.Listed         = Listing;
		Cards->set_filters(std::move(filters));
	}

	void github_conversation_panel::list_conversations(std::vector<std::string> aConversations, const QString& aCaption, const QString& aDescription, const QPixmap& aMark)
	{
		// The whole pull request, nothing else filtered: exactly the list.
		{
			const QSignalBlocker unread{ FilterUnread };
			const QSignalBlocker unresolved{ FilterUnresolved };
			FilterUnread->setChecked(false);
			FilterUnresolved->setChecked(false);
		}
		ScopeWholePr->setChecked(true);
		Listing = std::set<std::string>{ std::make_move_iterator(aConversations.begin()), std::make_move_iterator(aConversations.end()) };
		ListingMark->setPixmap(aMark);
		ListingMark->setVisible(!aMark.isNull());
		ListingCaption->setText(aCaption);
		ListingCaption->setAccessibleName(aCaption);
		ListingCaption->setAccessibleDescription(aDescription);
		ListingCaption->setToolTip(aDescription);
		ListingRow->show();
		apply_filters();
	}

	void github_conversation_panel::clear_listing()
	{
		if (!Listing) {
			return;
		}
		Listing.reset();
		ListingRow->hide();
		apply_filters();
		if (ListingCleared) {
			ListingCleared();
		}
	}

	auto github_conversation_panel::listing() const noexcept -> const std::optional<std::set<std::string>>&
	{
		return Listing;
	}

	auto github_conversation_panel::listing_caption() const noexcept -> QLabel*
	{
		return ListingCaption;
	}

	auto github_conversation_panel::clear_listing_button() const noexcept -> QToolButton*
	{
		return ClearListing;
	}

	void github_conversation_panel::set_listing_cleared(std::function<void()> aCleared)
	{
		ListingCleared = std::move(aCleared);
	}

	void github_conversation_panel::arrange_filters()
	{
		// Beside the code: two icons with tooltips. In Discuss: words, in its row.
		const auto icons = !Discuss;
		FilterUnread->setToolButtonStyle(icons ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextOnly);
		FilterUnresolved->setToolButtonStyle(icons ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextOnly);
		FilterUnread->setAutoRaise(icons);
		FilterUnresolved->setAutoRaise(icons);
		FilterUnread->setIcon(conversation_icon(conversation_glyph::only_new, palette()));
		FilterUnresolved->setIcon(conversation_icon(conversation_glyph::hide_resolved, palette()));
		FilterUnread->setIconSize(QSize{ 16, 16 });
		FilterUnresolved->setIconSize(QSize{ 16, 16 });
		FilterUnread->setText(icons ? QStringLiteral("Only new") : QStringLiteral("Unread"));
		FilterUnresolved->setText(icons ? QStringLiteral("Hide resolved") : QStringLiteral("Unresolved"));
		FilterUnread->setToolTip(QStringLiteral("Only threads with new replies"));
		FilterUnresolved->setToolTip(QStringLiteral("Hide resolved threads"));
		FilterUnread->setAccessibleName(icons ? QStringLiteral("Only new") : QStringLiteral("Unread only"));
		FilterUnresolved->setAccessibleName(icons ? QStringLiteral("Hide resolved") : QStringLiteral("Unresolved only"));
		FilterUnread->setAccessibleDescription(QStringLiteral("Show only threads with a post newer than what you have read"));
		FilterUnresolved->setAccessibleDescription(QStringLiteral("Show only threads that are not resolved"));
		if (Discuss) {
			DiscussGrid->addWidget(FilterUnread, 2, 0);
			DiscussGrid->addWidget(FilterUnresolved, 2, 1);
		} else {
			ReadToolsRow->addWidget(FilterUnread);
			ReadToolsRow->addWidget(FilterUnresolved);
		}
		FilterUnread->show();
		FilterUnresolved->show();
	}

	void github_conversation_panel::synchronize_current()
	{
		auto root = View->currentIndex();
		while (root.parent().isValid()) {
			root = root.parent();
		}
		const auto id = Model->id_for_index(root);
		if (Discuss) {
			DiscussSelection = Model->id_for_index(View->currentIndex());
		}
		Cards->set_current_thread(id);
	}

	void github_conversation_panel::adopt(
		const std::vector<application::github_conversation_projection>&
				aConversations,
		QString aAvailabilityText)
	{
		AvailabilityLabel->setText(std::move(aAvailabilityText));
		if (Retained == aConversations) {
			show_loaded_page();
			synchronize_current();
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

	void github_conversation_panel::set_link_dispatch(std::function<void(const std::string&, const std::string&)> aDispatch)
	{
		Cards->set_link_dispatch(std::move(aDispatch));
	}

	void github_conversation_panel::set_label_marks(const feedback_mark_theme& aTheme)
	{
		Cards->set_label_marks(aTheme);
	}

	void github_conversation_panel::adopt_appearance(const appearance_profile& aProfile)
	{
		Cards->set_code_font(aProfile.code_font());
	}

	void github_conversation_panel::refresh_projection()
	{
		// Restoring by identity keeps the reviewer on the same thread.
		const auto selected = Model->id_for_index(View->currentIndex());
		auto       shown    = !Discuss && (AnchorFile.has_value() || AnchorLine.has_value())
			? anchored()
			: Retained;
		shown               = with_endpoint_placement(
			std::move(shown),
			LeftHasCurrentDiffRevision,
			RightHasCurrentDiffRevision);
		// [[BPR-0972]]. The notes take the whole placed forest only when it
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
			synchronize_current();
			return;
		}
		Shown = std::move(shown);
		const QSignalBlocker selectionBlocker{ View->selectionModel() };
		Model->apply(Shown);
		HasThreads = !Shown.empty();
		// Select without the view's implicit reveal: a restore is not a choice.
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
		// above deliberately blocks.
		synchronize_current();
		show_loaded_page();
	}

	void github_conversation_panel::clear(QString aAvailabilityText)
	{
		DiscussSelection.clear();
		DefaultCurrent     = false;
		DiscussScope       = cards_scope::whole_pull_request;
		DiscussFilters     = { };
		DiscussInitialized = false;
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
		AvailabilityLabel->setText(std::move(aAvailabilityText));
		Model->clear();
		Cards->clear();
		HasThreads = false;
		Stack->setCurrentIndex(0);
		AvailabilityLabel->show();
	}

	void github_conversation_panel::show_loaded_page()
	{
		// [[BPR-1214]]. The page follows the snapshot, not the selected line:
		// a line no thread is on still shows the notes and their filters.
		Stack->setCurrentIndex(Retained.empty() ? 0 : 1);
		refresh_summary();
	}

	void github_conversation_panel::refresh_summary()
	{
		if (DiscussSummary == nullptr || Cards == nullptr) {
			return;
		}
		// The availability line says what the drawer cannot show; beside a
		// file's threads the notes say it themselves.
		AvailabilityLabel->setVisible(Discuss || Retained.empty() || Cards->cards().empty());
		DiscussSummary->setText(QStringLiteral("%1 conversations, %2 unresolved, %3 new")
									.arg(Cards->thread_count())
									.arg(Cards->unresolved_count())
									.arg(Cards->unread_thread_count()));
		if (Cards->cards().empty() && !Retained.empty()) {
			DiscussSummary->setText(QStringLiteral("No conversations match these filters. Clear filters to show all PR conversations."));
		}
		if (Cards->scope() == cards_scope::this_file && DiscussFile) {
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
			DiscussScope   = Cards->scope();
			DiscussFilters = Cards->filters();
		} else {
			ReadFilters = Cards->filters();
		}
		Discuss     = aDiscuss;
		PlacedStale = true;
		arrange_filters();
		DiscussControls->setVisible(Discuss);
		DiscussSummary->setVisible(Discuss);
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
			Cards->set_feed(true);
		} else {
			ScopeThisFile->setChecked(true);
			FilterUnread->setChecked(ReadFilters.UnreadOnly);
			FilterUnresolved->setChecked(ReadFilters.UnresolvedOnly);
			Cards->set_feed(false);
		}
		refresh_projection();
		if (Discuss && !remembered.empty()) {
			const auto selected = Model->index_for_id(remembered);
			if (selected.isValid()) {
				View->setCurrentIndex(selected);
			}
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

	auto github_conversation_panel::read_tools() const noexcept -> QWidget*
	{
		return ReadTools;
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

	auto github_conversation_panel::discuss_controls() const noexcept -> QWidget*
	{
		return DiscussControls;
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
		arrange_filters();
		Cards->relayout();
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
