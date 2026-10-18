#include "bpr/qt/github_conversation_panel.hpp"

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/github_conversation_model.hpp"

#include "conversation_file_identities.hpp"
#include "conversation_glyphs.hpp"
#include "discuss_header.hpp"
#include "github_presentation_text.hpp"

#include <QApplication>
#include <QComboBox>
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
#include <array>
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
		setAccessibleName(QStringLiteral("Read-only GitHub threads"));

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

		// [[BPR-1337]]. Discuss's top: one band, drawn in the language of
		// Threads' bar, that wraps when the list is narrowed beside the
		// composer. Beside the code the drawer shows this file only, and its
		// filters sit on the tab row.
		DiscussControls = new discuss_band{ loaded };
		DiscussControls->setObjectName(QStringLiteral("conversationDiscussControls"));
		DiscussControls->setAccessibleName(QStringLiteral("Discuss filters"));
		DiscussFlow = new discuss_flow_layout{ DiscussControls };
		DiscussFlow->setContentsMargins(12, 9, 16, 9);
		DiscussFlow->setSpacing(6);

		// The scope: the whole pull request or one file, as one two-way choice,
		// with the file chosen inside it while File is on.
		auto* const scopeBox    = new QWidget{ DiscussControls };
		auto* const scopeLayout = new QHBoxLayout{ scopeBox };
		scopeLayout->setContentsMargins(0, 0, 0, 0);
		scopeLayout->setSpacing(0);
		auto* const wholePr = new discuss_tool{ scopeBox };
		wholePr->set_shape(discuss_tool_shape::segment_first);
		wholePr->setToolButtonStyle(Qt::ToolButtonTextOnly);
		ScopeWholePr = wholePr;
		ScopeWholePr->setObjectName(QStringLiteral("conversationScopeWholePullRequest"));
		ScopeWholePr->setText(QStringLiteral("Pull request"));
		ScopeWholePr->setAccessibleName(QStringLiteral("Whole pull request"));
		ScopeWholePr->setAccessibleDescription(QStringLiteral(
			"Lists every thread of the pull request in file order"));
		ScopeWholePr->setToolTip(QStringLiteral("Every thread in the pull request, in file order"));
		ScopeWholePr->setCheckable(true);
		ScopeWholePr->setAutoExclusive(true);
		ScopeWholePr->setFocusPolicy(Qt::StrongFocus);
		auto* const thisFile = new discuss_tool{ scopeBox };
		thisFile->set_shape(discuss_tool_shape::segment_last);
		thisFile->setToolButtonStyle(Qt::ToolButtonTextOnly);
		ScopeThisFile = thisFile;
		ScopeThisFile->setObjectName(QStringLiteral("conversationScopeThisFile"));
		ScopeThisFile->setText(QStringLiteral("File"));
		ScopeThisFile->setAccessibleName(QStringLiteral("File scope"));
		ScopeThisFile->setAccessibleDescription(QStringLiteral(
			"Use the selected thread's file"));
		ScopeThisFile->setToolTip(QStringLiteral("One file's threads. Pull request comments and review summaries are not on a file, so they are left out."));
		ScopeThisFile->setCheckable(true);
		ScopeThisFile->setChecked(true);
		ScopeThisFile->setAutoExclusive(true);
		ScopeThisFile->setFocusPolicy(Qt::StrongFocus);
		DiscussFileChoice = new QComboBox{ scopeBox };
		DiscussFileChoice->setObjectName(QStringLiteral("conversationFileScope"));
		DiscussFileChoice->setAccessibleName(QStringLiteral("Thread file scope"));
		DiscussFileChoice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		DiscussFileChoice->setMinimumContentsLength(22);
		DiscussFileChoice->setMaximumWidth(360);
		DiscussFileChoice->setToolTip(QStringLiteral("The file whose threads are listed"));
		scopeLayout->addWidget(ScopeWholePr);
		scopeLayout->addWidget(ScopeThisFile);
		scopeLayout->addSpacing(6);
		scopeLayout->addWidget(DiscussFileChoice);
		DiscussFlow->addWidget(scopeBox);
		connect(DiscussFileChoice, &QComboBox::currentTextChanged, this, [this](const QString& aPath) {
			DiscussFile = aPath.isEmpty() ? std::nullopt : std::optional{ core::file_identity_of(aPath.toStdString()) };
			if (Discuss) {
				refresh_projection();
			}
		});

		// Only new and Hide resolved: Threads' two chips.
		DiscussFilterBox  = new QWidget{ DiscussControls };
		DiscussFiltersRow = new QHBoxLayout{ DiscussFilterBox };
		DiscussFiltersRow->setContentsMargins(0, 0, 0, 0);
		DiscussFiltersRow->setSpacing(6);
		DiscussFlow->addWidget(DiscussFilterBox);

		auto* const clearFilters = new discuss_tool{ DiscussControls };
		clearFilters->set_shape(discuss_tool_shape::chip);
		clearFilters->set_glyph(conversation_glyph::clear_filters);
		clearFilters->setToolButtonStyle(Qt::ToolButtonIconOnly);
		ClearFilters = clearFilters;
		ClearFilters->setObjectName(QStringLiteral("conversationClearFilters"));
		ClearFilters->setText(QStringLiteral("Clear"));
		ClearFilters->setAccessibleName(QStringLiteral("Clear filters"));
		ClearFilters->setAccessibleDescription(QStringLiteral("Show every thread of the pull request, with no filter"));
		ClearFilters->setToolTip(QStringLiteral("Clear filters"));
		ClearFilters->setFocusPolicy(Qt::StrongFocus);
		DiscussControls->hide();

		// [[BPR-1290]]. Only new and Hide resolved, as two icons on the tab row.
		ReadTools = new QWidget{ this };
		ReadTools->setObjectName(QStringLiteral("conversationReadTools"));
		ReadToolsRow = new QHBoxLayout{ ReadTools };
		ReadToolsRow->setContentsMargins(0, 0, 0, 0);
		ReadToolsRow->setSpacing(1);
		auto* const unread = new discuss_tool{ ReadTools };
		unread->set_glyph(conversation_glyph::only_new);
		FilterUnread = unread;
		FilterUnread->setObjectName(QStringLiteral("conversationFilterUnread"));
		FilterUnread->setCheckable(true);
		FilterUnread->setFocusPolicy(Qt::StrongFocus);
		auto* const unresolved = new discuss_tool{ ReadTools };
		unresolved->set_glyph(conversation_glyph::hide_resolved);
		FilterUnresolved = unresolved;
		FilterUnresolved->setObjectName(QStringLiteral("conversationFilterUnresolved"));
		FilterUnresolved->setCheckable(true);
		FilterUnresolved->setFocusPolicy(Qt::StrongFocus);
		ReadToolsRow->addWidget(FilterUnread);
		ReadToolsRow->addWidget(FilterUnresolved);
		connect(ClearFilters, &QToolButton::clicked, this, [this] {
			ScopeWholePr->setChecked(true);
			FilterUnread->setChecked(false);
			FilterUnresolved->setChecked(false);
			clear_listing();
		});
		// [[BPR-1089]]. An Overview count's drill-down: a pill with the count's
		// own mark, its caption and a × that shows every conversation again.
		ListingRow = new discuss_pill{ DiscussControls };
		ListingRow->setObjectName(QStringLiteral("conversationListingRow"));
		auto* const listingLayout = new QHBoxLayout{ ListingRow };
		listingLayout->setContentsMargins(8, 0, 4, 0);
		listingLayout->setSpacing(6);
		ListingMark = new QLabel{ ListingRow };
		ListingMark->setObjectName(QStringLiteral("conversationListingMark"));
		ListingCaption = new QLabel{ ListingRow };
		ListingCaption->setObjectName(QStringLiteral("conversationListingCaption"));
		// One line at its own width, so × follows the words; it may shrink
		// in a narrow panel rather than widen it.
		ListingCaption->setMinimumWidth(1);
		detail::set_plain_text_label(ListingCaption);
		auto* const clearListing = new discuss_tool{ ListingRow };
		clearListing->set_shape(discuss_tool_shape::bare);
		clearListing->set_glyph(conversation_glyph::dismiss);
		ClearListing = clearListing;
		ClearListing->setObjectName(QStringLiteral("conversationClearListing"));
		ClearListing->setText(QStringLiteral("×"));
		ClearListing->setToolTip(QStringLiteral("Show every thread"));
		ClearListing->setAccessibleName(QStringLiteral("Show every thread"));
		ClearListing->setFocusPolicy(Qt::StrongFocus);
		listingLayout->addWidget(ListingMark);
		listingLayout->addWidget(ListingCaption);
		listingLayout->addWidget(ClearListing);
		DiscussFlow->addWidget(ListingRow);
		ListingRow->hide();
		connect(ClearListing, &QToolButton::clicked, this, [this] { clear_listing(); });

		// The counts, with Clear filters, at the right end of the band.
		DiscussSummary = new discuss_counts{ DiscussControls };
		DiscussSummary->setObjectName(QStringLiteral("conversationDiscussSummary"));
		DiscussSummary->setAccessibleName(QStringLiteral("Threads summary"));
		auto* const trailing       = new QWidget{ DiscussControls };
		auto* const trailingLayout = new QHBoxLayout{ trailing };
		trailingLayout->setContentsMargins(0, 0, 0, 0);
		trailingLayout->setSpacing(10);
		trailingLayout->addWidget(DiscussSummary);
		trailingLayout->addWidget(ClearFilters);
		DiscussFlow->add_trailing(trailing);
		column->addWidget(DiscussControls);

		Cards = new conversation_cards_view{ loaded };
		Cards->setMinimumHeight(fontMetrics().lineSpacing() * 6);
		column->addWidget(Cards, 1);
		Notice = new QLabel{ loaded };
		Notice->setObjectName(QStringLiteral("conversationNotice"));
		Notice->setAccessibleName(QStringLiteral("Thread notice"));
		Notice->setWordWrap(true);
		Notice->setContentsMargins(8, 4, 8, 4);
		Notice->hide();
		column->addWidget(Notice);
		Stack->addWidget(loaded);
		layout->addWidget(Stack, 1);

		const auto filtersChanged = [this] {
			if (Discuss && DiscussFiltersChanged) {
				DiscussFiltersChanged();
			}
		};
		connect(FilterUnread, &QToolButton::toggled, this, [this, filtersChanged](bool) {
			apply_filters();
			filtersChanged();
		});
		connect(FilterUnresolved, &QToolButton::toggled, this, [this, filtersChanged](bool) {
			apply_filters();
			filtersChanged();
		});
		Cards->set_layout_observer([this] { refresh_summary(); });
		connect(ScopeThisFile, &QToolButton::toggled, this, [this, filtersChanged](const bool aChecked) {
			// The file is chosen inside the scope, while File is on.
			DiscussFileChoice->setVisible(aChecked);
			if (aChecked) {
				Cards->set_scope(cards_scope::this_file);
				filtersChanged();
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
		View->setAccessibleName(QStringLiteral("Selected thread"));
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
		// The band's order is its reading order: scope, file, filters, the
		// drill-down's ×, Clear, then the conversations. Moving the filters
		// between the band and the tab row keeps it.
		const auto order = std::array<QWidget*, 8>{ ScopeWholePr, ScopeThisFile, DiscussFileChoice, FilterUnread, FilterUnresolved, ClearListing, ClearFilters, Cards };
		for (auto index = std::size_t{ 1U }; index < order.size(); ++index) {
			setTabOrder(order[index - 1U], order[index]);
		}
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

	auto github_conversation_panel::discuss_view() const -> discuss_view_state
	{
		auto view    = discuss_view_state{ };
		view.Listing = Listing;
		if (Discuss) {
			view.Scope        = Cards->scope();
			view.File         = DiscussFile;
			view.OnlyNew      = FilterUnread->isChecked();
			view.HideResolved = FilterUnresolved->isChecked();
		} else if (DiscussInitialized) {
			// What `set_discuss` restores on the way back.
			view.Scope        = DiscussScope;
			view.File         = DiscussFile;
			view.OnlyNew      = DiscussFilters.UnreadOnly;
			view.HideResolved = DiscussFilters.UnresolvedOnly;
		}
		return view;
	}

	void github_conversation_panel::clear_discuss_filters(const bool aScope, const bool aOnlyNew, const bool aHideResolved, const bool aListing)
	{
		if (!Discuss) {
			return;
		}
		// Each change is the window's, not the reviewer's.
		auto changed = std::exchange(DiscussFiltersChanged, { });
		if (aScope) {
			ScopeWholePr->setChecked(true);
		}
		if (aOnlyNew) {
			FilterUnread->setChecked(false);
		}
		if (aHideResolved) {
			FilterUnresolved->setChecked(false);
		}
		if (aListing) {
			clear_listing();
		}
		DiscussFiltersChanged = std::move(changed);
	}

	void github_conversation_panel::set_discuss_filters_changed(std::function<void()> aChanged)
	{
		DiscussFiltersChanged = std::move(aChanged);
	}

	void github_conversation_panel::arrange_filters()
	{
		// Beside the code: two icons with tooltips. In Discuss: Threads'
		// two chips, in its band ([[BPR-1337]]).
		const auto icons = !Discuss;
		for (auto* const button : { FilterUnread, FilterUnresolved }) {
			auto* const tool = static_cast<discuss_tool*>(button);
			tool->set_shape(icons ? discuss_tool_shape::native : discuss_tool_shape::chip);
			tool->setToolButtonStyle(Qt::ToolButtonIconOnly);
			tool->setAutoRaise(icons);
			tool->setIconSize(QSize{ 16, 16 });
		}
		FilterUnread->setIcon(icons ? conversation_icon(conversation_glyph::only_new, palette()) : QIcon{ });
		FilterUnresolved->setIcon(icons ? conversation_icon(conversation_glyph::hide_resolved, palette()) : QIcon{ });
		FilterUnread->setText(QStringLiteral("Only new"));
		FilterUnresolved->setText(QStringLiteral("Hide resolved"));
		FilterUnread->setToolTip(QStringLiteral("Only threads with new replies"));
		FilterUnresolved->setToolTip(QStringLiteral("Hide resolved threads"));
		FilterUnread->setAccessibleName(icons ? QStringLiteral("Only new") : QStringLiteral("Unread only"));
		FilterUnresolved->setAccessibleName(icons ? QStringLiteral("Hide resolved") : QStringLiteral("Unresolved only"));
		FilterUnread->setAccessibleDescription(QStringLiteral("Show only threads with a post newer than what you have read"));
		FilterUnresolved->setAccessibleDescription(QStringLiteral("Show only threads that are not resolved"));
		if (Discuss) {
			DiscussFiltersRow->addWidget(FilterUnread);
			DiscussFiltersRow->addWidget(FilterUnresolved);
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
		RetainedFiles = detail::conversation_file_identities(Retained);
		RetainedRoot.clear();
		RetainedByRootFile.clear();
		auto byId = std::unordered_map<std::string_view, std::size_t>{ };
		byId.reserve(Retained.size());
		for (std::size_t index = 0U; index < Retained.size(); ++index) {
			if (Retained[index].SemanticAnchor) {
				SemanticAnchors.insert(*Retained[index].SemanticAnchor);
			}
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

	void github_conversation_panel::set_image_dispatch(std::function<void(const std::string&, std::vector<core::markdown_image_reference>)> aDispatch)
	{
		Cards->set_image_dispatch(std::move(aDispatch));
	}

	void github_conversation_panel::set_images_shown(const bool aShown)
	{
		Cards->set_images_shown(aShown);
	}

	void github_conversation_panel::apply_markdown_image(const application::markdown_image_projection& aImage)
	{
		Cards->apply_markdown_image(aImage);
	}

	void github_conversation_panel::set_code_syntax_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch)
	{
		Cards->set_code_syntax_dispatch(std::move(aDispatch));
	}

	void github_conversation_panel::apply_markdown_code_syntax(const application::markdown_code_syntax_publication& aSpans)
	{
		Cards->apply_markdown_code_syntax(aSpans);
	}

	void github_conversation_panel::request_images()
	{
		Cards->request_images();
	}

	void github_conversation_panel::set_link_dispatch(std::function<void(const std::string&, const std::string&)> aDispatch)
	{
		Cards->set_link_dispatch(std::move(aDispatch));
	}

	void github_conversation_panel::set_reference_dispatch(std::function<void(core::reference_page)> aDispatch)
	{
		Cards->set_reference_dispatch(std::move(aDispatch));
	}

	void github_conversation_panel::set_label_marks(const feedback_mark_theme& aTheme)
	{
		Cards->set_label_marks(aTheme);
	}

	void github_conversation_panel::adopt_appearance(const appearance_profile& aProfile)
	{
		Cards->set_code_font(aProfile.code_font());
		// [[BPR-1562]]. Posts' fenced code in the review panes' colours.
		Cards->set_code_theme({ .Colors = code_role_colors_of(aProfile.palette()), .Variant = aProfile.variant() });
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
			if (!Discuss && AnchorFile && !LeftHasCurrentDiffRevision && !RightHasCurrentDiffRevision) {
				for (const auto& node : Retained) {
					if (!node.ParentId && node.Path.empty() && (node.Kind == application::github_conversation_kind::issue_comment || node.Kind == application::github_conversation_kind::review)) {
						PlacedRetained.push_back(node);
					}
				}
			}
			PlacedStale = false;
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
		// file's threads the notes say it themselves. In Discuss an ordinary
		// count is the summary's to say ([[BPR-1337]]); anything else stays.
		AvailabilityLabel->setVisible(Discuss ? !AvailabilityOrdinary || Retained.empty()
											  : Retained.empty() || Cards->cards().empty());
		const auto conversations = Cards->thread_count();
		const auto unresolved    = Cards->unresolved_count();
		const auto fresh         = Cards->unread_thread_count();
		auto       sentence      = QStringLiteral("%1 %2, %3 unresolved, %4 new")
									   .arg(conversations)
									   .arg(conversations == 1U ? QStringLiteral("thread") : QStringLiteral("threads"))
									   .arg(unresolved)
									   .arg(fresh);
		if (Cards->cards().empty() && !Retained.empty()) {
			sentence = QStringLiteral("No threads match these filters. Clear filters to show all PR threads.");
		}
		auto notes = QStringList{ };
		if (Cards->scope() == cards_scope::this_file && DiscussFile) {
			sentence += QStringLiteral(" File scope excludes pathless PR comments and review summaries.");
			notes.push_back(QStringLiteral("Pull request comments and review summaries are not on a file, so File leaves them out."));
		}
		if (AvailabilityOrdinary && !AvailabilityLabel->text().isEmpty()) {
			notes.push_back(AvailabilityLabel->text());
		}
		DiscussSummary->setText(sentence);
		DiscussSummary->setAccessibleDescription(sentence);
		DiscussSummary->setToolTip(notes.join(QLatin1Char{ '\n' }));
		auto parts      = std::vector<discuss_count_part>(2U);
		parts[0].Number = QString::number(conversations);
		parts[0].Words  = conversations == 1U ? QStringLiteral("thread") : QStringLiteral("threads");
		parts[1].Number = QString::number(unresolved);
		parts[1].Words  = QStringLiteral("unresolved");
		static_cast<discuss_counts*>(DiscussSummary)->set_parts(std::move(parts));
		// Only new carries the count of what is new, as on Threads' bar.
		auto* const unread = static_cast<discuss_tool*>(FilterUnread);
		unread->set_new_ink(Cards->accent());
		unread->set_count(Discuss && fresh > 0U ? QString::number(fresh) : QString{ }, true);
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

	void github_conversation_panel::set_availability_ordinary(const bool aOrdinary)
	{
		AvailabilityOrdinary = aOrdinary;
		refresh_summary();
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
