#include "bpr/qt/workbench_window.hpp"

#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/side_chat_panel.hpp"

#include <QAbstractItemDelegate>
#include <QAbstractItemModel>
#include <QAbstractScrollArea>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QCursor>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QImage>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QPalette>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyleOptionViewItem>
#include <QToolButton>
#include <QTreeView>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Widget>
	[[nodiscard]] auto required_child(
		qt::workbench_window& aWindow,
		const char* const     aObjectName) -> Widget*
	{
		auto* const child = aWindow.findChild<Widget*>(QString::fromLatin1(aObjectName));
		expect(child != nullptr,
			   std::string{ "required GitHub presentation object exists: " }
				   + aObjectName);
		return child;
	}

	[[nodiscard]] auto presentation_state(qt::workbench_window& aWindow) -> QString
	{
		return required_child<QStackedWidget>(aWindow, "githubViewState")
			->property("githubPresentationState")
			.toString();
	}

	auto capture_layout_state(
		qt::workbench_window& aWindow,
		const QString&        aStateName) -> void
	{
		QApplication::processEvents();
		const auto rendered    = aWindow.grab();
		const auto deviceRatio = aWindow.devicePixelRatioF();
		expect(
			!rendered.isNull() && rendered.width() > 0 && rendered.height() > 0
				&& rendered.width()
					== static_cast<int>(std::lround(
						static_cast<double>(aWindow.width()) * deviceRatio))
				&& rendered.height()
					== static_cast<int>(std::lround(
						static_cast<double>(aWindow.height()) * deviceRatio)),
			"state layout capture is nonempty and has exact device dimensions");

		const auto configuredPath = qEnvironmentVariable("BPR_QT_RENDER_PATH");
		if (configuredPath.isEmpty()) {
			return;
		}
		const QFileInfo configured{ configuredPath };
		const auto      statePath = QStringLiteral("%1/%2-%3.png")
										.arg(
											configured.path(),
											configured.completeBaseName(),
											aStateName);
		expect(
			rendered.save(statePath, "PNG"),
			"state layout capture is saved in the ignored build tree");
	}

	[[nodiscard]] auto profiles()
		-> std::vector<application::github_profile_option>
	{
		return {
			{
				.Profile   = core::host_profile_id{ 11U },
				.Label     = "GitHub.com through GitHub CLI",
				.Hostname  = "github.com",
				.Product   = application::github_profile_product::github_com,
				.Transport = application::github_profile_transport::github_cli,
			},
			{
				.Profile   = core::host_profile_id{ 12U },
				.Label     = "Engineering GHES",
				.Hostname  = "ghe.example.invalid",
				.Product   = application::github_profile_product::github_enterprise_server,
				.Transport = application::github_profile_transport::native_https,
			},
		};
	}

	[[nodiscard]] auto conversation(
		std::string                                 aId,
		const application::github_conversation_kind aKind,
		std::string                                 aBody   = { },
		std::optional<std::string>                  aParent = std::nullopt)
		-> application::github_conversation_projection
	{
		return {
			.Id          = std::move(aId),
			.ParentId    = std::move(aParent),
			.Kind        = aKind,
			.AuthorLogin = "reviewer",
			.Body        = std::move(aBody),
			.Path        = "src/review.cpp",
			.Line        = 42U,
			.ReviewState = application::github_review_state_projection::commented,
		};
	}

	[[nodiscard]] auto initial_conversations()
		-> std::vector<application::github_conversation_projection>
	{
		using kind  = application::github_conversation_kind;
		auto thread = conversation("review-thread:t1", kind::review_thread);
		thread.AuthorLogin.clear();
		thread.Resolved = false;
		return {
			conversation("review:r1", kind::review, "Review body"),
			std::move(thread),
			conversation(
				"review-comment:c1",
				kind::review_comment,
				"Thread reply",
				"review-thread:t1"),
			conversation(
				"issue-comment:i1",
				kind::issue_comment,
				"<b>& canary"),
		};
	}

	auto test_conversation_model_is_stable_and_incremental() -> void
	{
		qt::github_conversation_model model;
		std::size_t                   inserted = 0U;
		std::size_t                   removed  = 0U;
		std::size_t                   moved    = 0U;
		std::size_t                   changed  = 0U;
		std::size_t                   reset    = 0U;

		QObject::connect(
			&model,
			&QAbstractItemModel::rowsInserted,
			[&inserted](const QModelIndex&, const int aFirst, const int aLast) {
				inserted += static_cast<std::size_t>(aLast - aFirst + 1);
			});
		QObject::connect(
			&model,
			&QAbstractItemModel::rowsRemoved,
			[&removed](const QModelIndex&, const int aFirst, const int aLast) {
				removed += static_cast<std::size_t>(aLast - aFirst + 1);
			});
		QObject::connect(
			&model,
			&QAbstractItemModel::rowsMoved,
			[&moved](
				const QModelIndex&,
				const int aFirst,
				const int aLast,
				const QModelIndex&,
				const int) {
				moved += static_cast<std::size_t>(aLast - aFirst + 1);
			});
		QObject::connect(
			&model,
			&QAbstractItemModel::dataChanged,
			[&changed](
				const QModelIndex& aTopLeft,
				const QModelIndex& aBottomRight,
				const QList<int>&) {
				changed += static_cast<std::size_t>(
					aBottomRight.row() - aTopLeft.row() + 1);
			});
		QObject::connect(
			&model,
			&QAbstractItemModel::modelReset,
			[&reset] { ++reset; });

		auto values = initial_conversations();
		model.apply(values);
		expect(model.rowCount() == 3, "three root conversations are inserted");
		const auto thread = model.index_for_id("review-thread:t1");
		expect(
			thread.isValid() && model.rowCount(thread) == 1,
			"review comment is a child of its stable thread");
		const auto child = model.index_for_id("review-comment:c1");
		expect(
			child.isValid()
				&& model.id_for_index(model.parent(child)) == "review-thread:t1",
			"nested review comment exposes its stable parent");
		const auto hostile = model.index_for_id("issue-comment:i1");
		expect(
			model.data(hostile, qt::github_conversation_model::body_role)
				== QStringLiteral("<b>& canary"),
			"remote markup-like body remains literal model text");

		QPersistentModelIndex stable{ hostile };
		inserted = 0U;
		removed  = 0U;
		moved    = 0U;
		changed  = 0U;

		values.back().Body = "Changed body";
		model.apply(values);
		expect(
			changed == 1U && inserted == 0U && removed == 0U && moved == 0U,
			"one semantic edit emits only dataChanged");
		expect(
			stable.isValid()
				&& model.id_for_index(QModelIndex{ stable }) == "issue-comment:i1",
			"data change preserves the persistent identity index");

		values.insert(
			values.begin(),
			conversation(
				"issue-comment:i2",
				application::github_conversation_kind::issue_comment,
				"New first comment"));
		inserted = 0U;
		model.apply(values);
		expect(inserted == 1U, "one new conversation emits one row insertion");
		expect(
			stable.isValid()
				&& model.id_for_index(QModelIndex{ stable }) == "issue-comment:i1",
			"unrelated insertion preserves the persistent identity index");

		auto       reordered = values;
		const auto issue     = std::ranges::find_if(
			reordered,
			[](const auto& aValue) { return aValue.Id == "issue-comment:i1"; });
		expect(issue != reordered.end(), "reorder fixture contains stable issue");
		auto issueValue = std::move(*issue);
		reordered.erase(issue);
		reordered.insert(reordered.begin(), std::move(issueValue));
		moved = 0U;
		model.apply(reordered);
		expect(moved == 1U, "stable root reorder emits one row move");
		expect(
			stable.isValid()
				&& stable.row() == 0
				&& QModelIndex{ stable } == model.index(0, 0)
				&& model.id_for_index(model.index(0, 0)) == "issue-comment:i1"
				&& model.id_for_index(model.index(1, 0)) == "issue-comment:i2"
				&& model.id_for_index(model.index(2, 0)) == "review:r1"
				&& model.id_for_index(model.index(3, 0))
					== "review-thread:t1",
			"row move produces the complete requested order and preserves the "
			"persistent index");

		auto reverseReordered = reordered;
		auto first            = std::move(reverseReordered.front());
		reverseReordered.erase(reverseReordered.begin());
		reverseReordered.push_back(std::move(first));
		moved = 0U;
		model.apply(reverseReordered);
		expect(
			moved == 1U,
			"first-to-last root reorder emits one proportional row move");
		expect(
			stable.isValid()
				&& stable.row() == 3
				&& QModelIndex{ stable } == model.index(3, 0)
				&& model.id_for_index(model.index(0, 0)) == "issue-comment:i2"
				&& model.id_for_index(model.index(1, 0)) == "review:r1"
				&& model.id_for_index(model.index(2, 0))
					== "review-thread:t1"
				&& model.id_for_index(model.index(3, 0)) == "issue-comment:i1",
			"reverse move produces the complete requested order and preserves "
			"the persistent index");
		reordered = std::move(reverseReordered);

		const auto review = std::ranges::find_if(
			reordered,
			[](const auto& aValue) { return aValue.Id == "review:r1"; });
		expect(review != reordered.end(), "removal fixture contains review");
		reordered.erase(review);
		removed = 0U;
		model.apply(reordered);
		expect(removed == 1U, "one removed review emits one row removal");

		const auto firstReply = std::ranges::find_if(
			reordered,
			[](const auto& aValue) {
				return aValue.Id == "review-comment:c1";
			});
		expect(firstReply != reordered.end(), "nested insertion has a parent row");
		reordered.insert(
			firstReply + 1,
			conversation(
				"review-comment:c2",
				application::github_conversation_kind::review_comment,
				"Second reply",
				"review-thread:t1"));
		inserted = 0U;
		model.apply(reordered);
		const auto updatedThread = model.index_for_id("review-thread:t1");
		expect(
			inserted == 1U
				&& model.rowCount(updatedThread) == 2
				&& model.id_for_index(model.index(0, 0, updatedThread))
					== "review-comment:c1"
				&& model.id_for_index(model.index(1, 0, updatedThread))
					== "review-comment:c2",
			"one nested reply emits one ordered child-row insertion");
		expect(reset == 0U, "incremental conversation updates never reset model");
		expect(
			model.statistics().ModelResets == 0U,
			"conversation statistics report zero model resets");
	}

	[[nodiscard]] auto enabled_commands()
		-> std::vector<core::github_command_projection>
	{
		return {
			{ .Id = core::github_command_id::activate_host_profile, .Enabled = true },
			{ .Id = core::github_command_id::open_pull_request, .Enabled = true },
			{ .Id = core::github_command_id::refresh_pull_request, .Enabled = true },
			{ .Id = core::github_command_id::show_review_threads, .Enabled = true },
		};
	}

	[[nodiscard]] auto capabilities(
		const core::github_capability_state aReviewThreads = core::github_capability_state::supported)
		-> core::github_capabilities
	{
		return {
			.ReviewThreads       = aReviewThreads,
			.ViewedFileMutations = core::github_capability_state::supported,
			.ResolveThread       = core::github_capability_state::unsupported,
			.UnresolveThread     = core::github_capability_state::unknown,
		};
	}

	[[nodiscard]] auto rate_limits()
		-> application::github_rate_limits_projection
	{
		return {
			.Core = application::github_rate_limit_projection{
				.Resource         = application::github_rate_limit_resource_projection::core,
				.Limit            = 5'000U,
				.Remaining        = 4'999U,
				.ResetUnixSeconds = 2'000'000'000U,
			},
			.Graphql = application::github_rate_limit_projection{
				.Resource         = application::github_rate_limit_resource_projection::graphql,
				.Limit            = 5'000U,
				.Remaining        = 4'998U,
				.ResetUnixSeconds = 2'000'000'000U,
			},
		};
	}

	[[nodiscard]] auto session_projection()
		-> application::github_session_projection
	{
		return {
			.Profile            = core::host_profile_id{ 12U },
			.Host               = core::git_host_id{ 22U },
			.Account            = core::github_account_id{ 52U },
			.Login              = "octo-reviewer",
			.DisplayName        = "Octo Reviewer",
			.Capabilities       = capabilities(),
			.CapabilityRevision = core::capability_revision{ 7U },
			.RateLimits         = rate_limits(),
		};
	}

	[[nodiscard]] auto pull_request_projection(
		const bool aThreadsAvailable = true)
		-> application::github_pull_request_projection
	{
		return {
			.Profile                = core::host_profile_id{ 12U },
			.Host                   = core::git_host_id{ 22U },
			.Account                = core::github_account_id{ 52U },
			.Snapshot               = core::provider_snapshot_id{ 91U },
			.Repository             = core::repository_id{ 71U },
			.PullRequest            = core::pull_request_id{ 81U },
			.Owner                  = "openai",
			.RepositoryName         = "better-pull-review",
			.Number                 = core::pull_request_number{ 27U },
			.Title                  = "<b>& literal pull request title",
			.Body                   = "<script>literal body & review</script>",
			.State                  = application::github_pull_request_state_projection::open,
			.Draft                  = true,
			.AuthorLogin            = "octo-author",
			.BaseRef                = "main",
			.HeadRef                = "github-presentation",
			.BaseOid                = "1111111111111111111111111111111111111111",
			.HeadOid                = "2222222222222222222222222222222222222222",
			.FileCount              = 2U,
			.CommitCount            = 3U,
			.ReviewCount            = aThreadsAvailable ? 1U : 0U,
			.ReviewThreadCount      = aThreadsAvailable ? 1U : 0U,
			.IssueCommentCount      = 1U,
			.ReviewThreadsAvailable = aThreadsAvailable,
			.Capabilities           = capabilities(
				aThreadsAvailable
					? core::github_capability_state::supported
					: core::github_capability_state::unknown),
			.CapabilityRevision = core::capability_revision{ 7U },
			.RateLimits         = rate_limits(),
			.Conversations      = aThreadsAvailable
				? initial_conversations()
				: std::vector<application::github_conversation_projection>{
					  conversation(
						  "issue-comment:i1",
						  application::github_conversation_kind::issue_comment,
						  "<b>& canary"),
				  },
		};
	}

	auto test_intent_translation_is_narrow_and_account_is_output_only() -> void
	{
		std::vector<application::github_intent> intents;
		qt::workbench_window                    window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[&intents](application::github_intent aIntent) {
				intents.push_back(std::move(aIntent));
			},
		};

		application::github_presentation_update commandUpdate{
			.Transition = application::github_transition_projection{
				.Commands = enabled_commands(),
			},
		};
		window.publish(commandUpdate);

		expect(window.profile_selector()->count() == 2, "two profiles are visible");
		window.profile_selector()->setCurrentIndex(1);
		expect(
			intents.empty(),
			"programmatic profile synchronization emits no authority intent");
		window.github_connect_action()->trigger();
		expect(intents.size() == 1U, "Connect emits exactly one semantic intent");
		const auto* const activate = std::get_if<core::activate_host_profile_intent>(&intents.back());
		expect(
			activate != nullptr
				&& activate->Profile == core::host_profile_id{ 12U },
			"Connect emits only the selected opaque profile identity");

		window.session_panel()->owner_editor()->setText(QStringLiteral("better-pull-review"));
		window.session_panel()->repository_editor()->setText(QStringLiteral("desktop"));
		window.session_panel()->pull_request_editor()->setValue(73);
		window.github_open_action()->trigger();
		expect(intents.size() == 2U, "Open emits exactly one semantic intent");
		const auto* const open = std::get_if<core::open_pull_request_intent>(&intents.back());
		expect(
			open != nullptr
				&& open->Repository
					== core::repository_locator{
						.Owner = "better-pull-review",
						.Name  = "desktop",
					}
				&& open->Number == core::pull_request_number{ 73U },
			"Open preserves owner, repository, and pull-request number");

		window.github_refresh_action()->trigger();
		expect(
			intents.size() == 3U
				&& std::holds_alternative<core::refresh_pull_request_intent>(
					intents.back()),
			"Refresh emits only refresh_pull_request_intent");

		window.show_review_threads_action()->trigger();
		expect(
			intents.size() == 3U,
			"Show Review Threads remains a local presentation action");

		expect(
			!window.github_connect_action()->shortcut().isEmpty()
				&& !window.github_open_action()->shortcut().isEmpty()
				&& !window.github_refresh_action()->shortcut().isEmpty(),
			"primary actions expose keyboard shortcuts");
		expect(
			window.active_account_text() == "Not authenticated",
			"account identity is initially read-only authenticated output");

		auto* const accessible = QAccessible::queryAccessibleInterface(window.profile_selector());
		expect(
			accessible != nullptr
				&& !accessible->text(QAccessible::Name).isEmpty()
				&& accessible->state().focusable,
			"profile selector exposes an accessible name and focusable state");
	}

	auto publish_session_ready(qt::workbench_window& aWindow) -> void
	{
		const auto session = session_projection();
		aWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = enabled_commands(),
				.Status   = core::github_status_delta{
					.Status = core::github_status::session_ready,
				},
				.Identity = core::github_identity_delta{
					.Profile    = session.Profile,
					.Host       = session.Host,
					.Account    = session.Account,
					.Generation = core::github_identity_generation{ 2U },
				},
				.Capabilities = core::github_capability_delta{
					.Capabilities = session.Capabilities,
					.Revision     = session.CapabilityRevision,
				},
			},
			.Session = session,
		});
	}

	auto publish_pull_request(
		qt::workbench_window& aWindow,
		const bool            aThreadsAvailable = true) -> void
	{
		auto pullRequest = pull_request_projection(aThreadsAvailable);
		aWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = enabled_commands(),
				.Status   = core::github_status_delta{
					.Status = core::github_status::pull_request_ready,
				},
				.PullRequest = core::pull_request_summary_delta{
					.Summary = core::github_pull_request_summary{
						.Locator = {
							.Owner = pullRequest.Owner,
							.Name  = pullRequest.RepositoryName,
						},
						.Number      = pullRequest.Number,
						.Repository  = pullRequest.Repository,
						.PullRequest = pullRequest.PullRequest,
						.Snapshot    = pullRequest.Snapshot,
					},
				},
			},
			.PullRequest = std::move(pullRequest),
		});
	}

	[[nodiscard]] auto required_label(
		qt::workbench_window& aWindow,
		const char* const     aObjectName) -> QLabel*
	{
		return required_child<QLabel>(aWindow, aObjectName);
	}

	auto test_presentation_states_disclosures_and_responsive_layout() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		window.resize(640, 720);
		window.show();
		QApplication::processEvents();

		auto* const banner           = required_child<QFrame>(window, "githubStatusBanner");
		auto* const viewState        = required_child<QStackedWidget>(window, "githubViewState");
		auto* const emptyState       = required_child<QWidget>(window, "githubEmptyState");
		auto* const emptyTitle       = required_label(window, "githubEmptyStateTitle");
		auto* const emptyBody        = required_label(window, "githubEmptyStateBody");
		auto* const connectionButton = required_child<QToolButton>(
			window, "githubConnectionDetailsButton");
		auto* const connectionDetails = required_child<QWidget>(window, "githubConnectionDetails");
		auto* const technicalButton   = required_child<QToolButton>(
			window, "githubTechnicalDetailsButton");
		auto* const technicalDetails  = required_child<QWidget>(window, "githubTechnicalDetails");
		auto* const conversationState = required_child<QStackedWidget>(window, "githubConversationState");
		auto* const conversationEmpty = required_child<QWidget>(window, "githubConversationEmptyState");
		auto* const connectButton     = required_child<QToolButton>(window, "connectGithubButton");
		auto* const refreshButton     = required_child<QToolButton>(window, "refreshPullRequestButton");
		auto* const addRuleButton     = required_child<QToolButton>(window, "addObserverRuleButton");
		auto* const removeRuleButton  = required_child<QToolButton>(window, "removeObserverRuleButton");

		// The status banner is the session chrome's, on the Settings window's
		// accounts page, so that page is presented for the states it shows.
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		expect(
			banner->isVisible()
				&& viewState->currentWidget() == emptyState
				&& presentation_state(window) == QStringLiteral("disconnected")
				&& emptyTitle->text() == QStringLiteral("Connect to GitHub")
				&& !emptyBody->text().isEmpty(),
			"initial presentation is a visible disconnected empty state");
		capture_layout_state(window, QStringLiteral("disconnected"));
		// Restated for the composed window. `github_window` gave the description
		// and the conversation a splitter of their own and turned it vertical at
		// narrow widths. The shell has no such splitter: the conversation lives
		// in the conversations drawer, whose width is host-persisted and whose
		// collapse is core-owned, which is RFC-BPR-0005's mechanism for exactly
		// this. The guarantee the old assertion bought -- that the conversation
		// still has somewhere to be at the 640-pixel minimum -- is what is
		// checked here.
		expect(
			window.conversations_drawer() != nullptr
				&& window.conversation_panel() != nullptr
				&& window.conversations_drawer()->isAncestorOf(
					window.conversation_panel()),
			"at the 640-pixel minimum the conversation is still hosted by the "
			"conversations drawer");

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::establishing_session,
				},
			},
		});
		expect(
			presentation_state(window) == QStringLiteral("connecting")
				&& viewState->currentWidget() == emptyState
				&& emptyTitle->text().contains(QStringLiteral("Connecting"))
				&& emptyBody->text().contains(
					QStringLiteral("authenticated session")),
			"connecting has explicit activity and guidance");

		publish_session_ready(window);
		expect(
			presentation_state(window) == QStringLiteral("empty")
				&& viewState->currentWidget() == emptyState
				&& emptyTitle->text() == QStringLiteral("Open a pull request")
				&& emptyBody->text().contains(QStringLiteral("owner")),
			"session-ready without a pull request has an actionable empty state");

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::loading_pull_request,
				},
				.PullRequest = core::pull_request_summary_delta{
					.Summary = std::nullopt,
				},
			},
		});
		expect(
			presentation_state(window) == QStringLiteral("loading")
				&& viewState->currentWidget() == emptyState
				&& emptyTitle->text().contains(QStringLiteral("Loading"))
				&& emptyBody->text().contains(QStringLiteral("snapshot")),
			"a new target load clears content into an explicit loading state");
		capture_layout_state(window, QStringLiteral("loading"));

		publish_pull_request(window);
		window.set_queue_preferences_dispatch(
			[](application::queue_preferences_intent) { });
		window.set_queue_rule_dispatch(
			[](application::queue_rule_intent) { });
		const auto savedRule = core::observer_rule{
			.Kind    = core::observer_rule_kind::repository,
			.Pattern = "octo/review-repository",
		};
		window.publish(application::queue_landing_projection{
			.Status = application::queue_landing_status::ready,
			.Rules  = { savedRule },
		});
		window.queue_rule_pattern_editor()->setText(
			QStringLiteral("src/parser"));
		QApplication::processEvents();
		expect(
			presentation_state(window) == QStringLiteral("loaded")
				&& viewState->currentWidget() != emptyState
				&& conversationState->currentWidget() != conversationEmpty
				&& conversationState->currentWidget()->isAncestorOf(
					window.conversation_view()),
			"accepted pull-request content replaces the empty state");

		// The session chrome is on the Settings window's accounts page.
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		expect(
			connectionButton->isVisible()
				&& technicalButton->isVisible()
				&& !connectionDetails->isVisible()
				&& !technicalDetails->isVisible()
				&& (connectionButton->focusPolicy() & Qt::TabFocus) != 0
				&& (technicalButton->focusPolicy() & Qt::TabFocus) != 0,
			"both disclosure controls are naturally focusable and initially "
			"collapsed");

		const auto nextVisibleTabStop = [](QWidget* const aStart) {
			auto* current = aStart->nextInFocusChain();
			while (current != aStart) {
				if (
					current->isVisible() && current->isEnabled()
					&& (current->focusPolicy() & Qt::TabFocus) != 0) {
					return current;
				}
				current = current->nextInFocusChain();
			}
			return aStart;
		};
		auto declaredTabStops = window.focus_order();
		declaredTabStops.removeIf([](const QWidget* const aStop) {
			return !aStop->isVisible() || !aStop->isEnabled()
				|| (aStop->focusPolicy() & Qt::TabFocus) == 0;
		});
		expect(!declaredTabStops.isEmpty(), "the workbench declares available tab stops");

		auto  walkedTabStops = QList<QWidget*>{ };
		auto* currentTabStop = declaredTabStops.front();
		auto  steps          = qsizetype{ 0 };
		do {
			if (declaredTabStops.contains(currentTabStop)) {
				walkedTabStops.push_back(currentTabStop);
			}
			currentTabStop = nextVisibleTabStop(currentTabStop);
			++steps;
		} while (
			currentTabStop != declaredTabStops.front()
			&& steps <= window.findChildren<QWidget*>().size());
		expect(
			currentTabStop == declaredTabStops.front()
				&& walkedTabStops == declaredTabStops,
			"the live Qt tab chain walks every declared available stop in order");

		const auto containsInOrder = [](
										 const QList<QWidget*>& aStops,
										 const QList<QWidget*>& aExpected) {
			auto cursor = aStops.cbegin();
			for (auto* const expected : aExpected) {
				cursor = std::find(cursor, aStops.cend(), expected);
				if (cursor == aStops.cend()) {
					return false;
				}
				++cursor;
			}
			return true;
		};
		// [[BPR-0646]]. The session controls are the Settings window's
		// accounts page, another top-level window with its own tab chain, so
		// the workbench declares none of them; the queue and the pull request
		// details keep their order.
		expect(
			!declaredTabStops.contains(window.profile_selector())
				&& !declaredTabStops.contains(connectButton)
				&& !declaredTabStops.contains(connectionButton)
				&& !declaredTabStops.contains(refreshButton)
				&& containsInOrder(
					declaredTabStops,
					{
						window.queue_view(),
						window.queue_role_filter(),
						window.queue_observer_filter(),
						window.queue_bucket_filter(),
						window.queue_repository_filter(),
						window.queue_quiet_filter(),
						window.queue_sort_selector(),
						required_child<QComboBox>(
							window, "observerRuleSelector"),
						window.queue_rule_kind_selector(),
						window.queue_rule_pattern_editor(),
						window.queue_rule_exclusion(),
						addRuleButton,
						removeRuleButton,
						technicalButton,
					}),
			"session controls and available queue stops retain Exclude, Add, Remove, then technical-details order");
		expect(
			declaredTabStops.count(addRuleButton) == 1
				&& declaredTabStops.count(removeRuleButton) == 1,
			"each enabled observer-rule presenter appears once in the declared and live Qt chain");

		connectionButton->setFocus(Qt::TabFocusReason);
		connectionButton->click();
		QApplication::processEvents();
		expect(
			connectionDetails->isVisible()
				&& connectionButton->arrowType() == Qt::DownArrow
				&& connectionButton->accessibleName().contains(
					QStringLiteral("Hide"))
				&& QApplication::focusWidget() == connectionButton,
			"connection disclosure opens in place without moving focus");
		connectionButton->click();
		QApplication::processEvents();
		expect(
			!connectionDetails->isVisible()
				&& connectionButton->arrowType() == Qt::RightArrow
				&& QApplication::focusWidget() == connectionButton,
			"connection disclosure closes in place without moving focus");

		// The technical details are the workbench's, in the files drawer;
		// focus returns to that window before the disclosure is walked.
		window.activateWindow();
		QApplication::processEvents();
		technicalButton->setFocus(Qt::TabFocusReason);
		technicalButton->click();
		QApplication::processEvents();
		expect(
			technicalDetails->isVisible()
				&& technicalButton->arrowType() == Qt::DownArrow
				&& technicalButton->accessibleName().contains(
					QStringLiteral("Hide"))
				&& QApplication::focusWidget() == technicalButton,
			"technical disclosure opens in place without moving focus");
		technicalButton->click();
		QApplication::processEvents();
		expect(
			!technicalDetails->isVisible()
				&& technicalButton->arrowType() == Qt::RightArrow
				&& QApplication::focusWidget() == technicalButton,
			"technical disclosure closes in place without moving focus");

		window.resize(960, 720);
		QApplication::processEvents();
		// Restated with the drawer that replaced the content splitter: at a
		// comfortable width the conversation is beside the comparison rather
		// than stacked under it, which is what the horizontal orientation used
		// to say.
		expect(
			window.conversations_drawer()->isVisible()
				&& window.conversations_drawer()->width() > 0,
			"at 960 pixels the conversations drawer holds the conversation "
			"beside the comparison");

		const auto retainedTitle    = required_label(window, "pullRequestTitle")->text();
		const auto retainedIdentity = QPersistentModelIndex{
			window.conversation_model()->index_for_id("issue-comment:i1")
		};
		window.conversation_view()->setCurrentIndex(retainedIdentity);
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::loading_pull_request,
				},
			},
		});
		expect(
			presentation_state(window) == QStringLiteral("refreshing")
				&& required_label(window, "pullRequestTitle")->text()
					== retainedTitle
				&& retainedIdentity.isValid()
				&& window.conversation_view()->currentIndex()
					== retainedIdentity,
			"refresh retains accepted content in a distinct refreshing state");

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::pull_request_failed,
				},
				.Diagnostics = {
					{
						.Code = core::github_reason_code::network_unavailable,
					},
				},
			},
		});
		expect(
			presentation_state(window) == QStringLiteral("loaded-error")
				&& required_label(window, "pullRequestTitle")->text()
					== retainedTitle
				&& retainedIdentity.isValid()
				&& !window.github_diagnostic_text().empty(),
			"refresh failure retains accepted content and presents the error");
		capture_layout_state(window, QStringLiteral("retained-refresh-error"));

		auto availableEmpty              = pull_request_projection();
		availableEmpty.ReviewCount       = 0U;
		availableEmpty.ReviewThreadCount = 0U;
		availableEmpty.IssueCommentCount = 0U;
		availableEmpty.Conversations.clear();
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::pull_request_ready,
				},
			},
			.PullRequest = std::move(availableEmpty),
		});
		expect(
			presentation_state(window) == QStringLiteral("loaded")
				&& conversationState->currentWidget() == conversationEmpty
				&& required_label(window, "reviewThreadAvailability")
					   ->text()
					   .contains(QStringLiteral("available;"))
				&& required_label(window, "reviewThreadAvailability")
					   ->text()
					   .contains(QStringLiteral("none")),
			"available-empty conversations are distinct from unavailable data");

		auto unknownThreads = pull_request_projection(false);
		unknownThreads.Conversations.clear();
		window.publish(application::github_presentation_update{
			.PullRequest = std::move(unknownThreads),
		});
		expect(
			presentation_state(window) == QStringLiteral("loaded")
				&& conversationState->currentWidget() == conversationEmpty
				&& required_label(window, "reviewThreadAvailability")
					   ->text()
					   .contains(QStringLiteral("not been confirmed")),
			"unknown review-thread support remains visibly unknown");

		auto unsupportedThreads = pull_request_projection(false);
		unsupportedThreads.Conversations.clear();
		unsupportedThreads.Capabilities.ReviewThreads = core::github_capability_state::unsupported;
		window.publish(application::github_presentation_update{
			.PullRequest = std::move(unsupportedThreads),
		});
		expect(
			required_label(window, "reviewThreadAvailability")
				->text()
				.contains(QStringLiteral("does not support")),
			"unsupported review-thread capability is distinct from unknown");

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::establishing_session,
				},
				.Identity = core::github_identity_delta{
					.Profile    = core::host_profile_id{ 11U },
					.Host       = std::nullopt,
					.Account    = std::nullopt,
					.Generation = core::github_identity_generation{ 3U },
				},
				.PullRequest = core::pull_request_summary_delta{
					.Summary = std::nullopt,
				},
			},
		});
		expect(
			presentation_state(window) == QStringLiteral("connecting")
				&& viewState->currentWidget() == emptyState
				&& window.conversation_model()->rowCount() == 0
				&& window.active_account_text() == "Connecting…",
			"identity switching clears stale content into the connecting state");

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::session_failed,
				},
				.Diagnostics = {
					{
						.Code = core::github_reason_code::authentication_failed,
					},
				},
			},
		});
		expect(
			presentation_state(window) == QStringLiteral("error")
				&& emptyTitle->text()
					== QStringLiteral("Connection unavailable")
				&& emptyBody->text().contains(
					QStringLiteral("Authentication failed"))
				&& banner->isVisible(),
			"identity connection failure presents a focused empty error state");
	}

	auto test_connection_diagnostics_can_be_copied() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		window.resize(960, 640);
		window.show();
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status      = core::github_status_delta{ .Status = core::github_status::session_failed },
				.Diagnostics = { { .Code = core::github_reason_code::malformed_response } },
			},
		});
		QApplication::processEvents();
		const auto diagnostic = QStringLiteral("GitHub returned data that BetterPullReview could not safely use.");
		for (const auto* name : { "githubStatus", "githubDiagnostic", "githubEmptyStateTitle", "githubEmptyStateBody" }) {
			auto* const label      = required_label(window, name);
			auto* const accessible = QAccessible::queryAccessibleInterface(label);
			expect(label->textFormat() == Qt::PlainText
					   && label->textInteractionFlags().testFlag(Qt::TextSelectableByMouse)
					   && label->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard)
					   && accessible != nullptr && accessible->text(QAccessible::Name) == label->text(),
				   "status and diagnostic expose current literal text and support mouse/keyboard selection");
		}
		auto* const clipboard = QApplication::clipboard();
		expect(clipboard != nullptr, "the Qt platform provides a clipboard");
		required_child<QToolButton>(window, "copyGithubMessageButton")->click();
		expect(clipboard->text() == QStringLiteral("Connection unavailable\n") + diagnostic,
			   "the ordinary copy control copies the exact visible failure title and body");
		required_child<QToolButton>(window, "copyGithubDiagnosticButton")->click();
		expect(clipboard->text() == QString::fromStdString(window.github_status_text()) + QStringLiteral("\n") + diagnostic,
			   "the status banner copies its current status and diagnostic without other fields");
		capture_layout_state(window, QStringLiteral("copyable-connection-error"));
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{ .Status = core::github_status::no_profile },
			},
		});
		required_child<QToolButton>(window, "copyGithubDiagnosticButton")->click();
		expect(!clipboard->text().contains(diagnostic)
				   && QAccessible::queryAccessibleInterface(required_label(window, "githubDiagnostic"))->text(QAccessible::Name).isEmpty(),
			   "clearing the diagnostic also removes stale text from copy and accessibility");
		required_child<QToolButton>(window, "copyGithubMessageButton")->click();
		expect(clipboard->text().startsWith(QStringLiteral("Connect to GitHub\n"))
				   && !clipboard->text().contains(diagnostic),
			   "empty-state copy follows the new projection");
	}

	auto test_bootstrap_stage_messages_are_visible_copyable_and_cleared() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ }, qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ }, profiles(), [](application::github_intent) { }
		};
		window.resize(960, 640);
		window.show();
		const std::array cases{
			std::pair{ application::github_bootstrap_failure_stage::account_identity, "Account identity observation: " },
			std::pair{ application::github_bootstrap_failure_stage::capability_discovery, "Capability discovery: " },
			std::pair{ application::github_bootstrap_failure_stage::session_acceptance, "Session acceptance: " },
			std::pair{ application::github_bootstrap_failure_stage::local_enrollment, "Local enrollment: " }
		};
		for (const auto& [stage, prefix] : cases) {
			window.publish(application::github_presentation_update{
				.Transition = application::github_transition_projection{
					.Status = core::github_status_delta{ .Status = core::github_status::establishing_session } } });
			window.publish(application::github_presentation_update{
				.Transition = application::github_transition_projection{
					.Status      = core::github_status_delta{ .Status = core::github_status::session_failed },
					.Diagnostics = { { .Code = core::github_reason_code::malformed_response } } },
				.HostDiagnostics = { { .Reason = core::github_reason_code::malformed_response, .BootstrapStage = stage } } });
			QApplication::processEvents();
			auto message = QString::fromLatin1(prefix) + QStringLiteral("GitHub returned data that BetterPullReview could not safely use.");
			if (stage == application::github_bootstrap_failure_stage::capability_discovery) {
				message.append(QStringLiteral(" No more specific capability refusal detail is available."));
			}
			expect(required_label(window, "githubEmptyStateBody")->isVisible()
					   && required_label(window, "githubEmptyStateBody")->text() == message
					   && required_label(window, "githubDiagnostic")->text() == message,
				   "bootstrap phase is rendered with the safe existing diagnostic");
			required_child<QToolButton>(window, "copyGithubMessageButton")->click();
			expect(QApplication::clipboard()->text() == QStringLiteral("Connection unavailable\n") + message,
				   "copy includes the exact visible bootstrap phase");
			window.publish(application::github_presentation_update{
				.Transition = application::github_transition_projection{
					.Diagnostics = { { .Code = core::github_reason_code::malformed_response } } } });
			expect(QString::fromStdString(window.github_diagnostic_text()) == message,
				   "generic same-reason projection cannot overwrite the stage detail");
			if (stage == application::github_bootstrap_failure_stage::capability_discovery) {
				capture_layout_state(window, QStringLiteral("bootstrap-capability-refusal"));
			}
			window.publish(application::github_presentation_update{
				.Transition = application::github_transition_projection{
					.Status = core::github_status_delta{ .Status = core::github_status::no_profile } } });
			expect(window.github_diagnostic_text().empty(), "identity replacement clears bootstrap phase");
			required_child<QToolButton>(window, "copyGithubMessageButton")->click();
			expect(!QApplication::clipboard()->text().contains(QString::fromLatin1(prefix)),
				   "cleared bootstrap phase cannot survive in copied text");
		}
	}

	auto test_capability_refusal_detail_is_copyable_and_cleared() -> void
	{
		qt::workbench_window window{ qt::workbench_intent_dispatch{ }, qt::workbench_change_dispatch{ }, qt::workbench_drawer_width_dispatch{ }, profiles(), [](application::github_intent) { } };
		window.resize(960, 640);
		window.show();
		using diagnostic = application::github_capability_diagnostic;
		const std::array cases{
			std::pair{ diagnostic::transport_content_encoding, QStringLiteral("The response used an unsupported content encoding.") },
			std::pair{ diagnostic::graphql_errors, QStringLiteral("The capability response contained a GraphQL error envelope.") },
			std::pair{ diagnostic::host_cache_stage, QStringLiteral("The application could not stage the capability observation.") },
			std::pair{ diagnostic::graphql_errors_not_array, QStringLiteral("The capability errors field was not an array.") },
			std::pair{ diagnostic::graphql_errors_empty, QStringLiteral("The capability response contained an empty errors array.") },
			std::pair{ diagnostic::graphql_errors_malformed, QStringLiteral("The capability response contained a malformed error entry.") },
			std::pair{ diagnostic::graphql_errors_mixed, QStringLiteral("The capability response contained mixed or unclassified error codes.") },
			std::pair{ diagnostic::graphql_code_forbidden, QStringLiteral("The capability response reported GraphQL error code FORBIDDEN.") },
			std::pair{ diagnostic::graphql_code_unauthorized, QStringLiteral("The capability response reported GraphQL error code UNAUTHORIZED.") },
			std::pair{ diagnostic::graphql_code_not_found, QStringLiteral("The capability response reported GraphQL error code NOT_FOUND.") },
			std::pair{ diagnostic::graphql_code_max_node_limit_exceeded, QStringLiteral("The capability response reported GraphQL error code MAX_NODE_LIMIT_EXCEEDED.") },
			std::pair{ diagnostic::graphql_code_internal, QStringLiteral("The capability response reported GraphQL error code INTERNAL.") },
			std::pair{ diagnostic::graphql_code_service_unavailable, QStringLiteral("The capability response reported GraphQL error code SERVICE_UNAVAILABLE.") },
			std::pair{ diagnostic::graphql_code_parse_failed, QStringLiteral("The capability response reported GraphQL error code GRAPHQL_PARSE_FAILED.") },
			std::pair{ diagnostic::graphql_code_validation_failed, QStringLiteral("The capability response reported GraphQL error code GRAPHQL_VALIDATION_FAILED.") },
			std::pair{ diagnostic::graphql_code_undefined_field, QStringLiteral("The capability response reported GraphQL error code undefinedField.") },
			std::pair{ diagnostic::graphql_code_argument_not_accepted, QStringLiteral("The capability response reported GraphQL error code argumentNotAccepted.") },
			std::pair{ diagnostic::graphql_code_undefined_type, QStringLiteral("The capability response reported GraphQL error code undefinedType.") },
			std::pair{ diagnostic::graphql_code_selection_mismatch, QStringLiteral("The capability response reported GraphQL error code selectionMismatch.") },
			std::pair{ diagnostic::graphql_code_variable_mismatch, QStringLiteral("The capability response reported GraphQL error code variableMismatch.") },
			std::pair{ diagnostic::graphql_code_variable_not_used, QStringLiteral("The capability response reported GraphQL error code variableNotUsed.") },
			std::pair{ diagnostic::graphql_code_variable_requires_valid_type, QStringLiteral("The capability response reported GraphQL error code variableRequiresValidType.") },
			std::pair{ diagnostic::graphql_code_variable_not_defined, QStringLiteral("The capability response reported GraphQL error code variableNotDefined.") },
			std::pair{ static_cast<diagnostic>(255U), QStringLiteral("No more specific capability refusal detail is available.") }
		};
		for (const auto& [detail, expected] : cases) {
			window.publish(application::github_presentation_update{
				.Transition = application::github_transition_projection{
					.Status      = core::github_status_delta{ .Status = core::github_status::session_failed },
					.Diagnostics = { { .Code = core::github_reason_code::malformed_response } } },
				.HostDiagnostics = { { .Reason = core::github_reason_code::malformed_response, .BootstrapStage = application::github_bootstrap_failure_stage::capability_discovery, .CapabilityDetail = detail } } });
			QApplication::processEvents();
			const auto message = QString::fromStdString(window.github_diagnostic_text());

			expect(message.startsWith(QStringLiteral("Capability discovery: ")) && message.endsWith(expected)
					   && required_label(window, "githubEmptyStateBody")->isVisible()
					   && required_label(window, "githubEmptyStateBody")->text() == message,
				   "fixed capability refusal detail appears in the visible error message");
			required_child<QToolButton>(window, "copyGithubMessageButton")->click();
			expect(QApplication::clipboard()->text() == QStringLiteral("Connection unavailable\n") + message,
				   "copy uses exactly the finite visible detail");
			if (detail == diagnostic::graphql_code_forbidden) {
				capture_layout_state(window, QStringLiteral("capability-graphql-code-refusal"));
			}
			window.publish(application::github_presentation_update{ .Transition = application::github_transition_projection{ .Status = core::github_status_delta{ .Status = core::github_status::no_profile } } });
			expect(window.github_diagnostic_text().empty(), "replacement clears capability detail");
		}
	}

	auto test_cancellation_states_and_failure_layouts() -> void
	{
		qt::workbench_window sessionWindow{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		sessionWindow.resize(960, 640);
		sessionWindow.show();
		QApplication::processEvents();
		auto* const sessionStack      = required_child<QStackedWidget>(sessionWindow, "githubViewState");
		auto* const sessionEmpty      = required_child<QWidget>(sessionWindow, "githubEmptyState");
		auto* const sessionDiagnostic = required_label(sessionWindow, "githubDiagnostic");
		sessionWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::establishing_session,
				},
			},
		});
		sessionWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::session_cancelled,
				},
			},
		});
		expect(
			presentation_state(sessionWindow) == QStringLiteral("cancelled")
				&& sessionStack->currentWidget() == sessionEmpty
				&& required_label(sessionWindow, "githubEmptyStateTitle")->text()
					== QStringLiteral("Connection canceled")
				&& required_label(sessionWindow, "githubEmptyStateBody")
					   ->text()
					   .contains(QStringLiteral("Connect"))
				&& sessionWindow.github_status_text()
					== "The GitHub connection was canceled."
				&& sessionWindow.github_diagnostic_text().empty()
				&& sessionDiagnostic->isHidden()
				&& required_label(sessionWindow, "pullRequestIdentity")->text()
					== QStringLiteral("No pull request is open")
				&& !sessionWindow.conversation_view()
						->currentIndex()
						.isValid(),
			"unloaded session cancellation has an empty cancellation page "
			"without a diagnostic or retained pull request");
		capture_layout_state(
			sessionWindow, QStringLiteral("session-cancelled"));

		qt::workbench_window firstLoadWindow{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		firstLoadWindow.resize(960, 640);
		firstLoadWindow.show();
		// The diagnostic is the session chrome's, on the accounts page.
		firstLoadWindow.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		publish_session_ready(firstLoadWindow);
		firstLoadWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::loading_pull_request,
				},
				.PullRequest = core::pull_request_summary_delta{
					.Summary = std::nullopt,
				},
			},
		});
		firstLoadWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::pull_request_cancelled,
				},
			},
		});
		auto* const firstLoadStack      = required_child<QStackedWidget>(firstLoadWindow, "githubViewState");
		auto* const firstLoadEmpty      = required_child<QWidget>(firstLoadWindow, "githubEmptyState");
		auto* const firstLoadDiagnostic = required_label(firstLoadWindow, "githubDiagnostic");
		expect(
			presentation_state(firstLoadWindow) == QStringLiteral("cancelled")
				&& firstLoadStack->currentWidget() == firstLoadEmpty
				&& required_label(firstLoadWindow, "githubEmptyStateTitle")
						->text()
					== QStringLiteral("Pull request loading canceled")
				&& required_label(firstLoadWindow, "githubEmptyStateBody")
					   ->text()
					   .contains(QStringLiteral("locator is unchanged"))
				&& firstLoadWindow.github_status_text()
					== "Pull request loading was canceled."
				&& firstLoadWindow.github_diagnostic_text().empty()
				&& firstLoadDiagnostic->isHidden()
				&& required_label(firstLoadWindow, "pullRequestIdentity")->text()
					== QStringLiteral("No pull request is open")
				&& required_child<QPlainTextEdit>(
					   firstLoadWindow, "pullRequestBody")
					   ->toPlainText()
					   .isEmpty()
				&& firstLoadWindow.conversation_model()->rowCount() == 0
				&& !firstLoadWindow.conversation_view()
						->currentIndex()
						.isValid(),
			"first-load pull-request cancellation has no retained projection or "
			"failure diagnostic");
		capture_layout_state(
			firstLoadWindow, QStringLiteral("first-load-cancelled"));

		firstLoadWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::loading_pull_request,
				},
				.PullRequest = core::pull_request_summary_delta{
					.Summary = std::nullopt,
				},
			},
		});
		firstLoadWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::pull_request_failed,
				},
				.Diagnostics = {
					{
						.Code = core::github_reason_code::network_unavailable,
					},
				},
			},
		});
		expect(
			presentation_state(firstLoadWindow) == QStringLiteral("error")
				&& firstLoadStack->currentWidget() == firstLoadEmpty
				&& required_label(firstLoadWindow, "githubEmptyStateTitle")
						->text()
					== QStringLiteral("Pull request unavailable")
				&& required_label(firstLoadWindow, "githubEmptyStateBody")->text()
					== firstLoadDiagnostic->text()
				&& firstLoadWindow.github_status_text()
					== "The pull request could not be loaded."
				&& !firstLoadWindow.github_diagnostic_text().empty()
				&& firstLoadDiagnostic->isVisible()
				&& firstLoadWindow.conversation_model()->rowCount() == 0,
			"first-load failure has an empty error page with the visible "
			"diagnostic");
		capture_layout_state(
			firstLoadWindow, QStringLiteral("first-load-error"));

		qt::workbench_window refreshWindow{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		publish_session_ready(refreshWindow);
		publish_pull_request(refreshWindow);
		refreshWindow.resize(960, 640);
		refreshWindow.show();
		QApplication::processEvents();
		auto* const                 refreshStack      = required_child<QStackedWidget>(refreshWindow, "githubViewState");
		auto* const                 refreshEmpty      = required_child<QWidget>(refreshWindow, "githubEmptyState");
		auto* const                 refreshDiagnostic = required_label(refreshWindow, "githubDiagnostic");
		const auto                  retainedTitle     = required_label(refreshWindow, "pullRequestTitle")->text();
		const auto                  retainedIdentity  = required_label(refreshWindow, "pullRequestIdentity")->text();
		const auto                  retainedBody      = required_child<QPlainTextEdit>(
															refreshWindow, "pullRequestBody")
															->toPlainText();
		const QPersistentModelIndex retainedSelection{
			refreshWindow.conversation_model()->index_for_id(
				"issue-comment:i1")
		};
		refreshWindow.conversation_view()->setCurrentIndex(retainedSelection);
		refreshWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::loading_pull_request,
				},
			},
		});
		refreshWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::pull_request_cancelled,
				},
			},
		});
		expect(
			presentation_state(refreshWindow)
					== QStringLiteral("loaded-cancelled")
				&& refreshStack->currentWidget() != refreshEmpty
				&& required_label(refreshWindow, "pullRequestTitle")->text()
					== retainedTitle
				&& required_label(refreshWindow, "pullRequestIdentity")->text()
					== retainedIdentity
				&& required_child<QPlainTextEdit>(
					   refreshWindow, "pullRequestBody")
						->toPlainText()
					== retainedBody
				&& retainedSelection.isValid()
				&& refreshWindow.conversation_view()->currentIndex()
					== retainedSelection
				&& refreshWindow.github_status_text()
					== "Refresh canceled. Showing the last accepted pull "
					   "request."
				&& refreshWindow.github_diagnostic_text().empty()
				&& refreshDiagnostic->isHidden(),
			"loaded refresh cancellation retains exact accepted content and "
			"selection without presenting a failure diagnostic");
		capture_layout_state(
			refreshWindow, QStringLiteral("retained-refresh-cancelled"));
	}

	auto test_identity_metadata_commands_and_refresh_semantics() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = {
					{
						.Id = core::github_command_id::
							activate_host_profile,
						.Enabled = true,
					},
					{
						.Id             = core::github_command_id::open_pull_request,
						.Enabled        = false,
						.DisabledReason = core::github_reason_code::session_in_progress,
					},
					{
						.Id             = core::github_command_id::refresh_pull_request,
						.Enabled        = false,
						.DisabledReason = core::github_reason_code::
							no_active_pull_request,
					},
					{
						.Id             = core::github_command_id::show_review_threads,
						.Enabled        = false,
						.DisabledReason = core::github_reason_code::
							no_active_pull_request,
					},
				},
				.Status = core::github_status_delta{
					.Status = core::github_status::establishing_session,
				},
				.Identity = core::github_identity_delta{
					.Profile    = core::host_profile_id{ 12U },
					.Host       = std::nullopt,
					.Account    = std::nullopt,
					.Generation = core::github_identity_generation{ 1U },
				},
				.Capabilities = core::github_capability_delta{
					.Capabilities = { },
					.Revision     = { },
				},
				.PullRequest = core::pull_request_summary_delta{
					.Summary = std::nullopt,
				},
			},
		});
		expect(
			window.active_host_text().find("ghe.example.invalid")
					!= std::string::npos
				&& required_label(window, "activeGithubTransport")->text()
					== QStringLiteral("Native HTTPS"),
			"establishing identity shows exact selected host and transport");
		expect(
			window.active_account_text() == "Connecting…",
			"establishing identity visibly clears the old account");
		expect(
			!window.github_open_action()->isEnabled()
				&& window.github_open_action()
						->property("githubDisabledReason")
						.toInt()
					== static_cast<int>(
						core::github_reason_code::session_in_progress)
				&& !window.github_open_action()->toolTip().isEmpty(),
			"Open command projection owns disabled state and explanation");

		publish_session_ready(window);
		expect(
			window.active_account_text()
				== "Octo Reviewer (@octo-reviewer)",
			"accepted /user output is visible as a read-only account");
		expect(
			window.active_host_text().find("Active host")
					!= std::string::npos
				&& window.github_open_action()->isEnabled(),
			"accepted identity marks the configured host active");

		publish_pull_request(window);
		expect(
			required_label(window, "pullRequestTitle")->text()
				== QStringLiteral("<b>& literal pull request title"),
			"remote title is rendered as literal plain text");
		auto* const body = window.findChild<QPlainTextEdit*>(QStringLiteral("pullRequestBody"));
		expect(
			body != nullptr && body->isReadOnly()
				&& body->toPlainText()
					== QStringLiteral(
						"<script>literal body & review</script>"),
			"remote pull-request body is literal and read-only");
		expect(
			required_label(window, "pullRequestIdentity")->text()
					== QStringLiteral("openai/better-pull-review #27")
				&& required_label(window, "pullRequestState")->text()
					== QStringLiteral("Open draft")
				&& required_label(window, "pullRequestBranches")
					   ->text()
					   .contains(QStringLiteral("main"))
				&& required_label(window, "pullRequestCommitOids")
					   ->text()
					   .contains(
						   QStringLiteral(
							   "2222222222222222222222222222222222222222")),
			"normalized metadata includes identity, state, branches, and exact OIDs");
		expect(
			window.conversation_model()->rowCount() == 3
				&& window.conversation_model()
					   ->index_for_id("review-comment:c1")
					   .isValid(),
			"accepted pull request renders review and comment identities");
		expect(
			required_label(window, "reviewThreadAvailability")->text()
				== QStringLiteral("1 review thread available."),
			"available review threads have an explicit visible state");
		expect(
			required_label(window, "githubCapabilities")
					->text()
					.contains(QStringLiteral("Unsupported"))
				&& required_label(window, "githubCapabilities")
					   ->text()
					   .contains(QStringLiteral("Not yet known"))
				&& required_label(window, "githubRateLimits")
					   ->text()
					   .contains(QStringLiteral("4999")),
			"capability tri-state and numeric rate observations remain distinct");

		window.show();
		QApplication::processEvents();
		window.show_review_threads_action()->trigger();
		QApplication::processEvents();
		const auto focusedThread = window.conversation_model()->index_for_id("review-thread:t1");
		expect(
			focusedThread.isValid()
				&& window.conversation_view()->currentIndex() == focusedThread
				&& window.conversation_view()->isExpanded(focusedThread)
				&& QApplication::focusWidget() == window.conversation_view(),
			"Show Review Threads focuses, selects, and expands a review thread");

		const auto selectedComment = window.conversation_model()->index_for_id("issue-comment:i1");
		window.conversation_view()->setCurrentIndex(selectedComment);
		auto reorderedProjection = pull_request_projection();
		auto selectedProjection  = std::ranges::find_if(
			reorderedProjection.Conversations,
			[](const auto& aValue) {
				return aValue.Id == "issue-comment:i1";
			});
		expect(
			selectedProjection != reorderedProjection.Conversations.end(),
			"selection fixture contains the issue comment");
		auto movedProjection = std::move(*selectedProjection);
		reorderedProjection.Conversations.erase(selectedProjection);
		reorderedProjection.Conversations.insert(
			reorderedProjection.Conversations.begin(),
			std::move(movedProjection));
		window.publish(application::github_presentation_update{
			.PullRequest = std::move(reorderedProjection),
		});
		expect(
			window.conversation_model()->id_for_index(
				window.conversation_view()->currentIndex())
				== "issue-comment:i1",
			"accepted refresh preserves selection by stable conversation identity");

		auto availableEmpty              = pull_request_projection();
		availableEmpty.ReviewThreadCount = 0U;
		std::erase_if(
			availableEmpty.Conversations,
			[](const auto& aValue) {
				return aValue.Kind
					== application::github_conversation_kind::review_thread
					|| aValue.Kind
					== application::github_conversation_kind::review_comment;
			});
		window.publish(application::github_presentation_update{
			.PullRequest = std::move(availableEmpty),
		});
		expect(
			required_label(window, "reviewThreadAvailability")
					->text()
					.contains(QStringLiteral("available;"))
				&& required_label(window, "reviewThreadAvailability")
					   ->text()
					   .contains(QStringLiteral("none")),
			"available-empty is distinct from unavailable review threads");

		window.publish(application::github_presentation_update{
			.PullRequest = pull_request_projection(false),
		});
		expect(
			required_label(window, "reviewThreadAvailability")
					->text()
					.contains(QStringLiteral("not been confirmed"))
				&& window.conversation_model()->rowCount() == 1,
			"unknown review-thread availability remains visibly unavailable");
		auto unsupportedThreads                       = pull_request_projection(false);
		unsupportedThreads.Capabilities.ReviewThreads = core::github_capability_state::unsupported;
		window.publish(application::github_presentation_update{
			.PullRequest = std::move(unsupportedThreads),
		});
		expect(
			required_label(window, "reviewThreadAvailability")
				->text()
				.contains(QStringLiteral("does not support")),
			"unsupported review threads remain distinct from unknown support");
		publish_pull_request(window);

		const auto priorTitle = required_label(window, "pullRequestTitle")->text();
		const auto priorRows  = window.conversation_model()->rowCount();
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = {
					{
						.Id             = core::github_command_id::refresh_pull_request,
						.Enabled        = false,
						.DisabledReason = core::github_reason_code::
							pull_request_load_in_progress,
					},
				},
				.Status = core::github_status_delta{
					.Status = core::github_status::loading_pull_request,
				},
			},
		});
		expect(
			required_label(window, "pullRequestTitle")->text() == priorTitle
				&& window.conversation_model()->rowCount() == priorRows,
			"refresh loading retains the last accepted projection");

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::pull_request_failed,
				},
				.Diagnostics = {
					{
						.Code = core::github_reason_code::network_unavailable,
					},
				},
			},
		});
		expect(
			required_label(window, "pullRequestTitle")->text() == priorTitle
				&& window.conversation_model()->rowCount() == priorRows
				&& !window.github_diagnostic_text().empty(),
			"refresh failure retains accepted content and shows a fixed error");

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::loading_pull_request,
				},
				.PullRequest = core::pull_request_summary_delta{
					.Summary = std::nullopt,
				},
			},
		});
		expect(
			required_label(window, "pullRequestTitle")->text()
					== QStringLiteral("Not available")
				&& window.conversation_model()->rowCount() == 0,
			"new pull-request open clears old metadata and conversations");

		publish_pull_request(window);
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = {
					{
						.Id             = core::github_command_id::show_review_threads,
						.Enabled        = false,
						.DisabledReason = core::github_reason_code::capability_unknown,
					},
				},
				.Capabilities = core::github_capability_delta{
					.Capabilities = capabilities(),
					.Revision     = core::capability_revision{ 7U },
				},
			},
		});
		expect(
			!window.show_review_threads_action()->isEnabled(),
			"capability display cannot override command-disabled action state");
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = {
					{
						.Id      = core::github_command_id::show_review_threads,
						.Enabled = true,
					},
				},
				.Capabilities = core::github_capability_delta{
					.Capabilities = capabilities(core::github_capability_state::unsupported),
					.Revision     = core::capability_revision{ 7U },
				},
			},
		});
		expect(
			window.show_review_threads_action()->isEnabled(),
			"command projection remains the sole action-legality authority");

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::establishing_session,
				},
				.Identity = core::github_identity_delta{
					.Profile    = core::host_profile_id{ 11U },
					.Host       = std::nullopt,
					.Account    = std::nullopt,
					.Generation = core::github_identity_generation{ 3U },
				},
				.Capabilities = core::github_capability_delta{
					.Capabilities = { },
					.Revision     = { },
				},
				.PullRequest = core::pull_request_summary_delta{
					.Summary = std::nullopt,
				},
			},
		});
		expect(
			window.active_host_text().find("github.com") != std::string::npos
				&& window.active_account_text() == "Connecting…"
				&& window.conversation_model()->rowCount() == 0
				&& required_label(window, "pullRequestTitle")->text()
					== QStringLiteral("Not available")
				&& required_label(window, "githubRateLimits")
					   ->text()
					   .contains(QStringLiteral("Not reported")),
			"identity switch removes old account, PR, and rate-limit content");

		window.show();
		// The locator is on the Settings window's accounts page.
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		window.session_panel()->owner_editor()->setFocus(Qt::OtherFocusReason);
		QApplication::processEvents();
		expect(
			QApplication::focusWidget() == window.session_panel()->owner_editor(),
			"owner editor receives focus before asynchronous projection");
		publish_session_ready(window);
		QApplication::processEvents();
		expect(
			QApplication::focusWidget() == window.session_panel()->owner_editor(),
			"asynchronous projections do not steal keyboard focus");
	}

	struct diagnostic_case
	{
		core::github_reason_code Reason;
		std::string_view         ExpectedText;
	};

	[[nodiscard]] auto diagnostic_cases() -> std::vector<diagnostic_case>
	{
		using reason = core::github_reason_code;
		return {
			{ reason::invalid_profile, "profile is invalid" },
			{ reason::session_in_progress, "connection attempt" },
			{ reason::session_required, "before opening" },
			{ reason::invalid_repository, "repository owner" },
			{ reason::invalid_pull_request_number, "pull request number" },
			{ reason::pull_request_load_in_progress, "current pull request load" },
			{ reason::no_active_pull_request, "before refreshing" },
			{ reason::capability_unknown, "not been confirmed" },
			{ reason::capability_unsupported, "does not support" },
			{ reason::stale_completion, "outdated GitHub result" },
			{ reason::completion_identity_mismatch, "different identity" },
			{ reason::authentication_failed, "Authentication failed" },
			{ reason::permission_denied, "does not have permission" },
			{ reason::network_unavailable, "network connection" },
			{ reason::api_version_unsupported, "GitHub API version" },
			{ reason::capability_unavailable, "capability information" },
			{ reason::rate_limited, "rate limit" },
			{ reason::malformed_response, "could not safely use" },
			{ reason::response_too_large, "safe limits" },
			{ reason::vault_unavailable, "credential store" },
			{ reason::certificate_rejected, "certificate" },
			{ reason::redirect_rejected, "untrusted destination" },
			{ reason::timed_out, "timed out" },
			{ reason::cli_unavailable, "CLI connection" },
			{ reason::cancelled, "was canceled" },
		};
	}

	auto test_status_diagnostics_and_host_detail_are_complete() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		using status = core::github_status;
		const std::vector statuses{
			status::no_profile,
			status::establishing_session,
			status::session_ready,
			status::session_failed,
			status::session_cancelled,
			status::loading_pull_request,
			status::pull_request_ready,
			status::pull_request_failed,
			status::pull_request_cancelled,
		};
		for (const auto value : statuses) {
			window.publish(application::github_presentation_update{
				.Transition = application::github_transition_projection{
					.Status = core::github_status_delta{ .Status = value },
				},
			});
			expect(
				!window.github_status_text().empty(),
				"every GitHub status has presentation-owned wording");
		}

		for (const auto& testCase : diagnostic_cases()) {
			window.publish(application::github_presentation_update{
				.Transition = application::github_transition_projection{
					.Diagnostics = { { .Code = testCase.Reason } },
				},
			});
			expect(
				window.github_diagnostic_text().find(testCase.ExpectedText)
					!= std::string::npos,
				"every GitHub reason has category-specific presentation wording");
		}

		const application::github_rate_limit_projection cause{
			.Resource         = application::github_rate_limit_resource_projection::graphql,
			.Limit            = 5'000U,
			.Remaining        = 100U,
			.ResetUnixSeconds = 2'000'000'000U,
			.SecondaryLimited = true,
		};
		window.publish(application::github_presentation_update{
			.HostDiagnostics = {
				{
					.Reason            = core::github_reason_code::rate_limited,
					.RetryAfterSeconds = 30U,
					.RateLimits        = rate_limits(),
					.RateLimitCause    = cause,
				},
			},
		});
		const auto detailed = window.github_diagnostic_text();
		expect(
			detailed.find("30 seconds") != std::string::npos
				&& detailed.find("temporarily") != std::string::npos,
			"bounded host advisory exposes retry and causal rate facts");
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Diagnostics = { { .Code = core::github_reason_code::rate_limited } },
			},
		});
		expect(
			window.github_diagnostic_text() == detailed,
			"generic core reason cannot overwrite richer same-turn host detail");

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Diagnostics = { { .Code = core::github_reason_code::none } },
			},
		});
		expect(
			window.github_diagnostic_text().empty(),
			"none diagnostic clears stale presentation wording");
	}

	auto test_native_inheritance_and_wrapped_conversation_layout() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		publish_session_ready(window);

		const std::string longBody    = "<b>& literal markup remains text.\n"
										"A deliberately long second paragraph wraps across several visual "
										"lines when the conversation pane narrows, preserving every word, "
										"punctuation mark, and line break without clipping or elision.";
		auto              pullRequest = pull_request_projection();
		pullRequest.Conversations.insert(
			pullRequest.Conversations.begin(),
			conversation(
				"issue-comment:long",
				application::github_conversation_kind::issue_comment,
				longBody));
		auto acceptedRefresh  = pullRequest;
		acceptedRefresh.Title = "Accepted refreshed title";
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::pull_request_ready,
				},
			},
			.PullRequest = std::move(pullRequest),
		});
		window.resize(960, 720);
		window.show();
		QApplication::processEvents();

		auto* const banner     = required_child<QFrame>(window, "githubStatusBanner");
		auto* const viewState  = required_child<QStackedWidget>(window, "githubViewState");
		auto* const title      = required_label(window, "pullRequestTitle");
		auto* const view       = window.conversation_view();
		auto* const model      = window.conversation_model();
		const auto  longIndex  = model->index_for_id("issue-comment:long");
		const auto  shortIndex = model->index_for_id("review:r1");
		expect(
			longIndex.isValid() && shortIndex.isValid(),
			"wrapped-row fixture exposes long and short stable rows");

		const auto literal = QString::fromUtf8(
			longBody.data(), static_cast<qsizetype>(longBody.size()));
		expect(
			longIndex.data(qt::github_conversation_model::body_role).toString()
					== literal
				&& longIndex.data(Qt::DisplayRole).toString().contains(literal)
				&& view->wordWrap()
				&& !view->uniformRowHeights()
				&& view->textElideMode() == Qt::ElideNone,
			"conversation content remains literal and is configured for "
			"nonuniform wrapping without elision");

		const auto measureHeight =
			[view](const QModelIndex& aIndex, const int aWidth) {
				QStyleOptionViewItem option;
				option.initFrom(view);
				option.widget = view;
				option.rect   = QRect{ 0, 0, aWidth, 1 };
				return view->itemDelegate()->sizeHint(option, aIndex).height();
			};
		const auto originalWideHeight  = measureHeight(longIndex, 620);
		const auto originalShortHeight = measureHeight(shortIndex, 620);
		expect(
			originalWideHeight > originalShortHeight,
			"long multiline conversation rows receive a larger delegate height");

		const std::array<QWidget*, 4> inheritedWidgets{
			banner,
			viewState,
			view,
			window.profile_selector(),
		};
		expect(
			std::ranges::all_of(
				inheritedWidgets,
				[&window](const QWidget* const aWidget) {
					return !aWidget->testAttribute(Qt::WA_SetPalette)
						&& !aWidget->testAttribute(Qt::WA_SetStyle)
						&& !aWidget->testAttribute(Qt::WA_SetFont)
						&& aWidget->font() == window.font()
						&& aWidget->styleSheet().isEmpty();
				})
				&& !window.testAttribute(Qt::WA_SetPalette)
				&& !window.testAttribute(Qt::WA_SetStyle)
				&& !window.testAttribute(Qt::WA_SetFont)
				&& window.styleSheet().isEmpty(),
			"representative presentation widgets inherit application palette, "
			"style, and body font without overrides");

		std::size_t modelResets = 0U;
		QObject::connect(
			model,
			&QAbstractItemModel::modelReset,
			[&modelResets] { ++modelResets; });
		const QPersistentModelIndex stableLong{ longIndex };
		const QPersistentModelIndex collapsedThread{
			model->index_for_id("review-thread:t1")
		};
		view->setCurrentIndex(longIndex);
		view->selectionModel()->select(
			longIndex,
			QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		view->setFocus(Qt::OtherFocusReason);
		view->collapse(collapsedThread);
		QApplication::processEvents();
		expect(
			collapsedThread.isValid() && !view->isExpanded(collapsedThread)
				&& QApplication::focusWidget() == view
				&& view->selectionModel()->isSelected(longIndex),
			"conversation fixture starts selected, focused, and explicitly "
			"collapsed");

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::pull_request_ready,
				},
			},
			.PullRequest = std::move(acceptedRefresh),
		});
		QApplication::processEvents();
		expect(
			required_label(window, "pullRequestTitle")->text()
					== QStringLiteral("Accepted refreshed title")
				&& stableLong.isValid() && collapsedThread.isValid()
				&& !view->isExpanded(collapsedThread)
				&& view->currentIndex() == stableLong
				&& view->selectionModel()->isSelected(stableLong)
				&& QApplication::focusWidget() == view
				&& modelResets == 0U,
			"accepted refresh preserves explicit collapse, selection, focus, and "
			"incremental model state");

		const auto originalPalette         = QApplication::palette();
		const auto originalFont            = QApplication::font();
		const auto originalBodyFontHeight  = QFontMetrics{ window.font() }.height();
		const auto originalTitleFontHeight = QFontMetrics{ title->font() }.height();

		auto         controlledPalette = originalPalette;
		const QColor controlledWindow{ 37, 52, 68 };
		const QColor controlledBase{ 25, 34, 43 };
		const QColor controlledText{ 226, 232, 240 };
		const QColor controlledHighlight{ 103, 73, 184 };
		controlledPalette.setColor(QPalette::Window, controlledWindow);
		controlledPalette.setColor(QPalette::Base, controlledBase);
		controlledPalette.setColor(QPalette::Text, controlledText);
		controlledPalette.setColor(
			QPalette::WindowText, controlledText);
		controlledPalette.setColor(
			QPalette::Highlight, controlledHighlight);
		QApplication::setPalette(controlledPalette);
		QApplication::processEvents();
		expect(
			banner->palette().color(QPalette::Window) == controlledWindow
				&& view->palette().color(QPalette::Base) == controlledBase
				&& title->palette().color(QPalette::WindowText)
					== controlledText
				&& std::ranges::none_of(
					inheritedWidgets,
					[](const QWidget* const aWidget) {
						return aWidget->testAttribute(Qt::WA_SetPalette);
					})
				&& !view->isExpanded(collapsedThread)
				&& view->currentIndex() == stableLong
				&& view->selectionModel()->isSelected(stableLong)
				&& QApplication::focusWidget() == view
				&& modelResets == 0U,
			"an application palette change reaches representative widgets "
			"through inheritance without disturbing conversation state");

		auto largerFont = originalFont;
		if (largerFont.pointSizeF() > 0.0) {
			largerFont.setPointSizeF(largerFont.pointSizeF() + 4.0);
		} else {
			largerFont.setPixelSize(
				std::max(1, largerFont.pixelSize()) + 6);
		}
		QApplication::setFont(largerFont);
		QApplication::processEvents();
		view->doItemsLayout();
		QApplication::processEvents();

		const auto largerWideHeight   = measureHeight(longIndex, 620);
		const auto largerNarrowHeight = measureHeight(longIndex, 190);
		expect(
			QFontMetrics{ window.font() }.height() > originalBodyFontHeight
				&& QFontMetrics{ title->font() }.height()
					> originalTitleFontHeight
				&& largerWideHeight > originalWideHeight
				&& largerNarrowHeight > largerWideHeight,
			"application font growth and narrower width increase wrapped row "
			"size hints");

		const auto nextIndex = view->indexBelow(longIndex);
		const auto longRect  = view->visualRect(longIndex);
		const auto nextRect  = view->visualRect(nextIndex);
		expect(
			nextIndex.isValid() && longRect.isValid() && nextRect.isValid()
				&& longRect.height() > nextRect.height()
				&& longRect.bottom() < nextRect.top(),
			"laid-out wrapped rows have nonuniform heights and never overlap");
		expect(
			stableLong.isValid()
				&& view->currentIndex() == stableLong
				&& view->selectionModel()->isSelected(stableLong)
				&& !view->isExpanded(collapsedThread)
				&& QApplication::focusWidget() == view
				&& modelResets == 0U,
			"appearance changes preserve row identity, selection, focus, and "
			"incremental model state");

		QApplication::setFont(originalFont);
		QApplication::setPalette(originalPalette);
		QApplication::processEvents();
		expect(
			QFontMetrics{ window.font() }.height() == originalBodyFontHeight
				&& banner->palette().color(QPalette::Window)
					== originalPalette.color(QPalette::Window)
				&& stableLong.isValid()
				&& view->currentIndex() == stableLong
				&& !view->isExpanded(collapsedThread)
				&& QApplication::focusWidget() == view
				&& modelResets == 0U,
			"application palette and font restoration also preserves UI state");
	}

	struct accessible_announcement
	{
		QString                             Message;
		QString                             ObjectName;
		QAccessible::AnnouncementPoliteness Politeness = QAccessible::AnnouncementPoliteness::Polite;
	};

	std::vector<accessible_announcement>* AnnouncementSink = nullptr;

	void capture_accessibility_update(QAccessibleEvent* const aEvent)
	{
		if (
			AnnouncementSink == nullptr || aEvent == nullptr
			|| aEvent->type() != QAccessible::Announcement) {
			return;
		}
		const auto* const announcement = static_cast<const QAccessibleAnnouncementEvent*>(aEvent);
		AnnouncementSink->push_back(
			{
				.Message    = announcement->message(),
				.ObjectName = aEvent->object() == nullptr ? QString{ }
														  : aEvent->object()->objectName(),
				.Politeness = announcement->politeness(),
			});
	}

	class scoped_accessibility_capture final
	{
	public:
		explicit scoped_accessibility_capture(
			std::vector<accessible_announcement>& aAnnouncements)
			: Previous{
				QAccessible::installUpdateHandler(capture_accessibility_update)
			}
		{
			AnnouncementSink = &aAnnouncements;
		}

		~scoped_accessibility_capture()
		{
			AnnouncementSink = nullptr;
			static_cast<void>(QAccessible::installUpdateHandler(Previous));
		}

		scoped_accessibility_capture(const scoped_accessibility_capture&) = delete;
		auto operator=(const scoped_accessibility_capture&)
			-> scoped_accessibility_capture&                         = delete;
		scoped_accessibility_capture(scoped_accessibility_capture&&) = delete;
		auto operator=(scoped_accessibility_capture&&)
			-> scoped_accessibility_capture& = delete;

	private:
		QAccessible::UpdateHandler Previous = nullptr;
	};

	auto test_accessible_announcements_are_combined_and_deduplicated() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		auto* const banner = required_child<QFrame>(window, "githubStatusBanner");
		window.show();
		QApplication::processEvents();
		std::vector<accessible_announcement> announcements;
		{
			const scoped_accessibility_capture            capture{ announcements };
			const application::github_presentation_update failure{
				.Transition = application::github_transition_projection{
					.Status = core::github_status_delta{
						.Status = core::github_status::session_failed,
					},
					.Diagnostics = {
						{
							.Code = core::github_reason_code::authentication_failed,
						},
					},
				},
			};
			window.publish(failure);
			expect(
				announcements.size() == 1U
					&& announcements.front().ObjectName
						== QStringLiteral("githubStatusBanner")
					&& announcements.front().Message.contains(
						QStringLiteral("connection failed"))
					&& announcements.front().Message.contains(
						QStringLiteral("Authentication failed"))
					&& announcements.front().Message
						== banner->accessibleDescription()
					&& announcements.front().Politeness
						== QAccessible::AnnouncementPoliteness::Polite,
				"one polite accessibility announcement combines status and "
				"diagnostic context");

			window.publish(failure);
			expect(
				announcements.size() == 1U,
				"an identical presentation update suppresses duplicate "
				"announcements");

			window.publish(application::github_presentation_update{
				.Transition = application::github_transition_projection{
					.Status = core::github_status_delta{
						.Status = core::github_status::session_failed,
					},
					.Diagnostics = {
						{
							.Code = core::github_reason_code::permission_denied,
						},
					},
				},
			});
			expect(
				announcements.size() == 2U
					&& announcements.back().Message.contains(
						QStringLiteral("does not have permission"))
					&& announcements.back().Message
						== banner->accessibleDescription(),
				"a changed diagnostic produces one newly combined announcement");
		}
	}

	auto test_accessibility_minimum_geometry_and_render() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		publish_session_ready(window);
		publish_pull_request(window);
		window.resize(640, 420);
		window.show();
		QApplication::processEvents();

		auto* const profileLabel  = window.findChild<QLabel*>(QStringLiteral("githubProfileLabel"));
		auto* const connectButton = window.findChild<QToolButton*>(QStringLiteral("connectGithubButton"));
		// `github_window` scrolled its entire contents, so the lower half of the
		// window was only reachable by driving that scrollbar. The shell does not
		// scroll as a whole -- the workbench fills it and the review header is
		// separately bounded -- so the lower render is simply the window, and
		// what is scrolled here is the header when it has anything to scroll.
		auto* const headerScroll = window.findChild<QScrollArea*>(
			QStringLiteral("workbenchReviewHeaderScroll"));
		const auto scrollHeaderTo = [headerScroll](const int aValue) {
			if (headerScroll != nullptr) {
				headerScroll->verticalScrollBar()->setValue(aValue);
			}
		};
		expect(
			profileLabel != nullptr
				&& profileLabel->buddy() == window.profile_selector(),
			"profile label exposes its accessible buddy relationship");
		const auto labels   = window.findChildren<QLabel*>();
		const auto hasBuddy = [&labels](const QWidget* const aWidget) {
			return std::ranges::any_of(
				labels,
				[aWidget](const QLabel* const aLabel) {
					return aLabel->buddy() == aWidget;
				});
		};
		expect(
			hasBuddy(window.session_panel()->owner_editor())
				&& hasBuddy(window.session_panel()->repository_editor())
				&& hasBuddy(window.session_panel()->pull_request_editor()),
			"all pull-request locator editors expose label relationships");
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		expect(
			connectButton != nullptr && connectButton->isVisible()
				&& !connectButton->accessibleName().isEmpty()
				&& !connectButton->accessibleDescription().isEmpty(),
			"Connect has visible named keyboard-accessible presentation");
		expect(
			window.session_panel()->owner_editor()->isVisible()
				&& window.session_panel()->repository_editor()->isVisible()
				&& window.session_panel()->pull_request_editor()->isVisible()
				&& window.conversations_drawer() != nullptr
				&& window.conversations_drawer()->isVisible()
				&& window.conversation_view()->isVisible()
				&& window.conversation_view()->width() > 0
				&& window.conversation_view()->height() > 0,
			"identity, locator, metadata, and conversations survive minimum size");

		auto* const conversationAccessible = QAccessible::queryAccessibleInterface(window.conversation_view());
		expect(
			conversationAccessible != nullptr
				&& !conversationAccessible->text(QAccessible::Name).isEmpty()
				&& conversationAccessible->state().focusable,
			"conversation tree exposes an accessible focusable interface");

		window.profile_selector()->setFocus(Qt::TabFocusReason);
		QApplication::processEvents();
		expect(
			QApplication::focusWidget() == window.profile_selector()
				&& QAccessible::queryAccessibleInterface(
					   window.profile_selector())
					   ->state()
					   .focused,
			"profile selector reports focused accessible state");

		const auto            expectedScaleText = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
		std::optional<double> expectedScale;
		if (!expectedScaleText.isEmpty()) {
			bool parsed   = false;
			expectedScale = expectedScaleText.toDouble(&parsed);
			expect(parsed, "expected Qt scale is numeric");
			expect(
				std::abs(window.devicePixelRatioF() - *expectedScale) < 0.02,
				"window reports the requested fractional device-pixel ratio");
		}

		QCursor::setPos(
			window.mapToGlobal(
				QPoint{ window.width() - 8, window.height() - 8 }));
		window.statusBar()->clearMessage();
		QApplication::processEvents();
		const QPixmap minimumRender = window.grab();
		expect(
			!minimumRender.isNull(),
			"minimum-size GitHub state renders to a nonempty raster");
		if (expectedScale.has_value()) {
			expect(
				minimumRender.width()
						== static_cast<int>(std::lround(
							static_cast<double>(window.width())
							* *expectedScale))
					&& minimumRender.height()
						== static_cast<int>(std::lround(
							static_cast<double>(window.height())
							* *expectedScale)),
				"minimum raster dimensions match logical size and scale");
		}
		const auto minimumRenderPath = qEnvironmentVariable("BPR_QT_MIN_RENDER_PATH");
		if (!minimumRenderPath.isEmpty()) {
			expect(
				minimumRender.save(minimumRenderPath, "PNG"),
				"minimum-size GitHub render is saved under the build");
		}
		scrollHeaderTo(
			headerScroll != nullptr
				? headerScroll->verticalScrollBar()->maximum()
				: 0);
		window.statusBar()->clearMessage();
		QApplication::processEvents();
		const auto minimumLowerRenderPath = qEnvironmentVariable("BPR_QT_MIN_LOWER_RENDER_PATH");
		if (!minimumLowerRenderPath.isEmpty()) {
			expect(
				window.grab().save(minimumLowerRenderPath, "PNG"),
				"minimum-size lower GitHub render is saved under the build");
		}

		scrollHeaderTo(0);
		window.resize(960, 640);
		QCursor::setPos(
			window.mapToGlobal(
				QPoint{ window.width() - 8, window.height() - 8 }));
		window.statusBar()->clearMessage();
		QApplication::processEvents();
		const QPixmap rendered = window.grab();
		expect(
			!rendered.isNull() && rendered.width() > 0 && rendered.height() > 0,
			"loaded GitHub state renders to a nonempty raster");
		if (expectedScale.has_value()) {
			expect(
				rendered.width()
						== static_cast<int>(std::lround(
							static_cast<double>(window.width())
							* *expectedScale))
					&& rendered.height()
						== static_cast<int>(std::lround(
							static_cast<double>(window.height())
							* *expectedScale)),
				"rendered raster dimensions match logical size and scale");
		}
		const auto renderPath = qEnvironmentVariable("BPR_QT_RENDER_PATH");
		if (!renderPath.isEmpty()) {
			expect(
				rendered.save(renderPath, "PNG"),
				"synthetic GitHub presentation render is saved under the build");
		}
		expect(
			window.statistics().GithubFullResets == 0U
				&& window.conversation_model()->statistics().ModelResets == 0U,
			"rendered projection path performs no full model or window reset");
	}
	/// \brief BPR-0513: both agent surfaces are laid into the review drawer.
	///
	/// \details The claim this case makes is placement and nothing else. It
	/// finds each panel the way a keyboard or assistive user does — by the
	/// accessible name the panel gives itself — and then asks the drawer's own
	/// column whether it manages the widget it found. Both halves are needed:
	/// a panel constructed as a child of the column but never added to it is
	/// still a `findChild` hit, and would still be painted, at the default
	/// geometry, on top of the conversation tree.
	auto test_agent_surfaces_are_placed_in_the_conversations_drawer() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		publish_session_ready(window);
		publish_pull_request(window);
		// Tall enough that the drawer can show the whole agent stack at once,
		// which is what the capture below has to be a picture of.
		window.resize(1'400, 1'500);
		window.show();

		// The drawer opens because the core said so, which is the only way it
		// ever opens; the window decides no drawer state of its own.
		auto opened    = application::workbench_projection{ };
		opened.Drawers = core::workbench_drawer_delta{
			.Files         = core::workbench_drawer_state::expanded,
			.Conversations = core::workbench_drawer_state::expanded,
		};
		window.publish(opened);
		QApplication::processEvents();

		auto* const drawer = window.conversations_drawer();
		expect(
			drawer != nullptr && drawer->isVisible(),
			"the conversations drawer opens on the core's decided state");

		const auto named = [drawer](const QString& aName) -> QWidget* {
			for (auto* const widget : drawer->findChildren<QWidget*>()) {
				if (widget->accessibleName() == aName) {
					return widget;
				}
			}
			return nullptr;
		};
		auto* const sideChat     = named(QStringLiteral("AI side chat"));
		auto* const findingQueue = named(QStringLiteral("AI finding queue"));
		expect(
			sideChat != nullptr && sideChat == window.side_chat(),
			"the open drawer holds the side chat panel under its accessible name");
		expect(
			findingQueue != nullptr && findingQueue == window.finding_queue(),
			"the open drawer holds the finding queue under its accessible name");

		const auto laidOut = [](QWidget* const aWidget) {
			auto* const parent
				= aWidget == nullptr ? nullptr : aWidget->parentWidget();
			return parent != nullptr && parent->layout() != nullptr
				&& parent->layout()->indexOf(aWidget) >= 0;
		};
		expect(
			laidOut(sideChat),
			"the side chat is laid into the drawer's own column, not merely parented to it");
		expect(
			laidOut(findingQueue),
			"the finding queue is laid into the drawer's own column, not merely parented to it");

		auto* const agentPanel = window.agent_panel();
		auto* const editor     = window.review_editor();
		expect(
			agentPanel != nullptr && editor != nullptr
				&& agentPanel->parentWidget() == sideChat->parentWidget()
				&& sideChat->parentWidget() == findingQueue->parentWidget(),
			"both agent surfaces share the drawer column the agent launch panel is in");
		const auto columnTop = [drawer](const QWidget* const aWidget) {
			return aWidget->mapTo(drawer, QPoint{ 0, 0 }).y();
		};
		expect(
			sideChat->isVisible() && findingQueue->isVisible()
				&& sideChat->width() > 0 && sideChat->height() > 0
				&& findingQueue->width() > 0 && findingQueue->height() > 0
				&& columnTop(agentPanel) < columnTop(sideChat)
				&& columnTop(sideChat) < columnTop(findingQueue),
			"the agent launcher, the side chat, and the finding queue stack down one visible column");

		// The workshop a promotion opens is the draft editor's own, and that
		// editor is already in this drawer. There is no second workshop, and
		// no window of its own for one to appear in.
		expect(
			drawer->isAncestorOf(editor)
				&& drawer->isAncestorOf(editor->workshop_original_editor())
				&& drawer->isAncestorOf(editor->workshop_proposal_editor()),
			"the comment workshop a finding promotion reaches is in this drawer already");

		auto* const convert = window.finding_queue()->action(
			qt::finding_queue_action::convert_to_draft_comment);
		expect(convert != nullptr, "the queue declares the promotion action");
		auto* offered = static_cast<QToolButton*>(nullptr);
		for (auto* const button : drawer->findChildren<QToolButton*>()) {
			if (button->defaultAction() == convert) {
				offered = button;
			}
		}
		expect(
			offered != nullptr && offered->isVisible(),
			"the finding action that opens the workshop is offered in the drawer");

		// The drawer button reaches the panel's own way out and nothing else.
		// The window sets no dispatch, so this one is the test's.
		auto intents = std::vector<qt::finding_action_intent>{ };
		window.finding_queue()->set_dispatch(
			[&intents](qt::finding_action_intent aIntent) {
				intents.push_back(std::move(aIntent));
			});
		auto delivery = application::agent_finding_delivery{ };
		delivery.Findings.push_back(application::agent_delivered_finding{ });
		delivery.Findings.front().Finding.Claim = "BPR-0513 placed finding";
		window.finding_queue()->present(delivery);
		window.finding_queue()->select(0U);
		offered->click();
		QApplication::processEvents();
		expect(
			intents.size() == 1U
				&& intents.front().Action
					== qt::finding_queue_action::convert_to_draft_comment,
			"clicking the placed button emits exactly one promotion intent and performs nothing");
		window.finding_queue()->set_dispatch({ });

		// The column is taller than the drawer, so it scrolls. Bringing the
		// two agent surfaces into the drawer's own visible region is what
		// makes the capture below a picture of them rather than of whatever
		// happened to be at the top of the column.
		auto* const column = drawer->findChild<QScrollArea*>(
			QStringLiteral("workbenchConversationDrawerScroll"));
		expect(
			column != nullptr,
			"the drawer's column scrolls rather than squeezing its panels");
		column->verticalScrollBar()->setValue(
			column->verticalScrollBar()->value()
			+ sideChat->mapTo(column->viewport(), QPoint{ 0, 0 }).y());
		QApplication::processEvents();
		const auto mostlyShown = [drawer](const QWidget* const aWidget) {
			const QRect placed{
				aWidget->mapTo(drawer, QPoint{ 0, 0 }),
				aWidget->size(),
			};
			return drawer->rect().intersected(placed).height()
				>= placed.height() / 2;
		};
		expect(
			mostlyShown(sideChat) && mostlyShown(findingQueue),
			"scrolling the drawer's column shows both agent surfaces inside the drawer");

		// The capture the acceptance criterion asks for, cropped to the drawer
		// the panels are in. A whole-window diff would pass on a repaint
		// anywhere; this raster is the drawer and only the drawer.
		QApplication::processEvents();
		const auto rendered = window.grab();
		const auto ratio    = window.devicePixelRatioF();
		const auto scaled   = [ratio](const int aValue) {
			return static_cast<int>(
				std::lround(static_cast<double>(aValue) * ratio));
		};
		const auto origin = drawer->mapTo(&window, QPoint{ 0, 0 });
		// A logical rectangle's device rectangle is its two rounded edges, not
		// a rounded origin plus a rounded size. Those two disagree by a pixel
		// at 1.25x for half the drawer widths there are, and `copy` answers a
		// rectangle that leaves the raster by returning a smaller pixmap
		// rather than refusing, so the disagreement arrived here as a crop of
		// the wrong size (BPR-0520).
		const QRect region{
			QPoint{ scaled(origin.x()), scaled(origin.y()) },
			QPoint{ scaled(origin.x() + drawer->width()) - 1,
					scaled(origin.y() + drawer->height()) - 1 },
		};
		const auto cropped = rendered.copy(region);
		expect(
			rendered.rect().contains(region) && !cropped.isNull()
				&& cropped.size() == region.size(),
			"the drawer crop has the drawer's exact device dimensions");
		const auto renderPath = qEnvironmentVariable("BPR_QT_RENDER_PATH");
		if (!renderPath.isEmpty()) {
			const QFileInfo configured{ renderPath };
			expect(
				cropped.save(
					QStringLiteral("%1/%2-agent-drawer.png")
						.arg(configured.path(), configured.completeBaseName()),
					"PNG"),
				"the drawer crop is saved in the ignored build tree");
		}

		// Collapse is the drawer's, and the placed panels inherit it rather
		// than answering it themselves.
		auto collapsed    = application::workbench_projection{ };
		collapsed.Drawers = core::workbench_drawer_delta{
			.Files         = core::workbench_drawer_state::expanded,
			.Conversations = core::workbench_drawer_state::collapsed,
		};
		window.publish(collapsed);
		QApplication::processEvents();
		expect(
			drawer->rail()->isVisible()
				&& drawer->width() == qt::workbench_drawer_rail::width_pixels
				&& !sideChat->isVisible() && !findingQueue->isVisible(),
			"collapsing the drawer to its rail hides both placed agent surfaces "
			"with it");
		window.publish(opened);
		QApplication::processEvents();
		expect(
			drawer->isVisible() && sideChat->isVisible()
				&& findingQueue->isVisible(),
			"reopening the drawer restores both placed agent surfaces");
	}
	/// \brief Whether a layout, or a row nested inside it, manages a widget.
	///
	/// \details BPR-0520 follows BPR-0513's membership check one level deeper,
	/// because a defect button sits in a row nested in the drawer's column
	/// rather than directly in the column. A button constructed as a child of
	/// that column but never added to any of its rows still answers
	/// `findChildren`, still paints, and would sit at the column's origin -
	/// which is inside the visible width, and so would pass the measurement
	/// below for entirely the wrong reason.
	[[nodiscard]] auto laid_into(
		const QLayout* const aLayout,
		const QWidget* const aWidget) -> bool
	{
		if (aLayout == nullptr) {
			return false;
		}
		for (auto index = 0; index < aLayout->count(); ++index) {
			auto* const item = aLayout->itemAt(index);
			if (item->widget() == aWidget || laid_into(item->layout(), aWidget)) {
				return true;
			}
		}
		return false;
	}

	/// \brief Names a control the way a failure has to name it.
	///
	/// \details BPR-0527 sweeps every control in the drawer rather than a
	/// listed few, so the message cannot be written out beside each name. A
	/// button answers under the object name its action carries, which is the
	/// name the card and the product's own wiring use; anything else answers
	/// under its own object name, then its accessible name, then its class,
	/// so a control nobody named still fails legibly instead of anonymously.
	[[nodiscard]] auto control_name(const QWidget* const aWidget) -> std::string
	{
		const auto* const button = qobject_cast<const QToolButton*>(aWidget);
		const auto* const action = button == nullptr ? nullptr
													 : button->defaultAction();
		auto              name   = action != nullptr && !action->objectName().isEmpty()
			? action->objectName()
			: aWidget->objectName();
		if (name.isEmpty()) {
			name = aWidget->accessibleName();
		}
		if (name.isEmpty()) {
			name = QString::fromLatin1(aWidget->metaObject()->className());
		}
		return name.toStdString();
	}

	/// \brief Every control the drawer's own width has to hold.
	///
	/// \details BPR-0527 widens BPR-0520's per-action measurement from the
	/// defect rows to the whole drawer, because the card it came from was
	/// written about two rows that had never been measured and a listed name
	/// cannot fail for a row nobody thought to list. The walk is over the
	/// column's own widget tree, so a row added later is swept without being
	/// added here.
	///
	/// It stops at a pane that scrolls on its own. A conversation tree, the
	/// agent's output, the finding list: each of those answers for what is
	/// inside it and can be scrolled to reach it. The drawer's width has to
	/// hold the pane, and that is what is measured. Hidden children and
	/// windows of their own - a combo box's popup - are not in the column's
	/// coordinate space and are not controls the drawer's width has to hold.
	void collect_drawer_controls(
		QWidget* const         aRoot,
		std::vector<QWidget*>& aFound)
	{
		for (auto* const child : aRoot->findChildren<QWidget*>(
				 QString{ }, Qt::FindDirectChildrenOnly)) {
			if (child->isWindow() || !child->isVisible()) {
				continue;
			}
			aFound.push_back(child);
			if (qobject_cast<QAbstractScrollArea*>(child) == nullptr) {
				collect_drawer_controls(child, aFound);
			}
		}
	}

	/// \brief Every control in the conversations drawer is inside its width.
	///
	/// \details The drawer's column scrolls down and refuses to scroll
	/// sideways, so a control whose rectangle leaves the visible width is not
	/// offscreen — it is absent, and no gesture in the product reaches it.
	/// BPR-0513's capture showed that happening to the defect rows; BPR-0520
	/// measured those two rows rather than looking at them. BPR-0527 widens
	/// the same measurement to the whole drawer, because two rows it did not
	/// name — the conversation panel's reveal and the agent launcher's run
	/// controls — were still overflowing while this case passed.
	///
	/// So it asks three questions. Whether the column is no wider than the
	/// width a reviewer can reach, which is the one claim that covers a row
	/// added tomorrow. Whether every control the column holds is inside that
	/// width, swept from the widget tree and named in the failure. And
	/// whether each action the drawer offers is still there under the object
	/// name it carried, is laid into a layout rather than merely parented,
	/// and was reached by the sweep — so a rename fails here rather than
	/// silently skipping. One executable is the 1.0x, 1.25x and 2.0x scale
	/// cases, so each is measured at all three scales the card names.
	auto test_drawer_controls_are_reachable_in_the_conversations_drawer() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		publish_session_ready(window);
		publish_pull_request(window);
		// Only the centre absorbs a window resize, so widening the window
		// leaves the drawer at the width the composition gives it - which is
		// the default width the claim is about. The height is generous so the
		// whole defect stack can be shown at once, which is what the window
		// capture below has to be a picture of.
		window.resize(1'400, 1'500);
		window.show();

		auto opened    = application::workbench_projection{ };
		opened.Drawers = core::workbench_drawer_delta{
			.Files         = core::workbench_drawer_state::expanded,
			.Conversations = core::workbench_drawer_state::expanded,
		};
		window.publish(opened);
		QApplication::processEvents();

		auto* const drawer = window.conversations_drawer();
		expect(
			drawer != nullptr && drawer->isVisible(),
			"the conversations drawer opens on the core's decided state");
		auto* const column = drawer->findChild<QScrollArea*>(
			QStringLiteral("workbenchConversationDrawerScroll"));
		expect(
			column != nullptr && column->viewport() != nullptr
				&& column->viewport()->width() > 0,
			"the drawer's column has a visible width to be inside of");
		// The premise everything below rests on. With sideways scrolling
		// refused, the viewport's width is the whole of what a reviewer can
		// reach, at every scroll position the column has.
		expect(
			column->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff
				&& !column->horizontalScrollBar()->isVisible(),
			"the drawer's column still refuses to scroll sideways");

		// Every control the column holds, swept from the widget tree rather
		// than listed, so a row nobody thought to list is measured too.
		expect(
			column->widget() != nullptr,
			"the drawer's column holds a widget to sweep");
		auto controls = std::vector<QWidget*>{ };
		collect_drawer_controls(column->widget(), controls);
		expect(
			!controls.empty(),
			"the sweep reaches the drawer's controls rather than an empty column");
		// BPR-0527's one claim about the drawer rather than about a control in
		// it, and the one that covers a row added tomorrow. A column is
		// resized to its own minimum whenever that minimum is wider than the
		// viewport, and everything past the viewport is then unreachable at
		// every scroll position - so the column fitting is the whole of what
		// makes its contents reachable. It is asked first because it names
		// what made the column too wide, which is not the same widget as the
		// one that then sticks out of it: a row that asks for nine pixels too
		// many stretches every panel beside it, and the sweep below reports
		// whichever of those the walk reaches first.
		auto* widest = static_cast<QWidget*>(nullptr);
		for (auto* const control : controls) {
			if (widest == nullptr
				|| control->minimumSizeHint().width()
					> widest->minimumSizeHint().width()) {
				widest = control;
			}
		}
		expect(
			column->widget()->width() <= column->viewport()->width()
				&& column->horizontalScrollBar()->maximum() == 0,
			std::string{ "the drawer's column is no wider than the width a "
						 "reviewer can reach; the widest thing in it is " }
				+ control_name(widest) + ", asking "
				+ std::to_string(widest->minimumSizeHint().width())
				+ " px of a " + std::to_string(column->viewport()->width())
				+ " px viewport");

		for (auto* const control : controls) {
			const QRect placed{
				control->mapTo(column->viewport(), QPoint{ 0, 0 }),
				control->size(),
			};
			expect(
				placed.left() >= 0
					&& placed.right() < column->viewport()->width(),
				std::string{ "the drawer control is inside the drawer's "
							 "visible width: " }
					+ control_name(control));
		}

		const auto offered = [drawer](const QString& aName) -> QToolButton* {
			auto* found = static_cast<QToolButton*>(nullptr);
			for (auto* const button : drawer->findChildren<QToolButton*>()) {
				auto* const action = button->defaultAction();
				if (action != nullptr && action->objectName() == aName) {
					found = button;
				}
			}
			return found;
		};
		// Every action the drawer offers, row by row, under the object name it
		// already carried. The sweep above measures whatever the column holds;
		// this list is what the drawer is supposed to hold, so a renamed or
		// dropped action fails here rather than passing as an absence.
		for (const auto* const name :
			 { // The conversation panel's own row, at the top of the column
			   // (RFC-BPR-0009, RFC-BPR-0010).
			   "showReviewThreadsAction",
			   // The agent launcher's run controls (RFC-BPR-0011).
			   "agentExistingCheckoutAction", "startAgentAction",
			   "cancelAgentAction",
			   // The side chat's row (RFC-BPR-0012, placed by BPR-0513).
			   "sideChatSend", "sideChatAddContext", "sideChatStartNewSession",
			   "sideChatContinueOldRevision",
			   // The six the finding queue declares (BPR-0506).
			   "findingOpenEvidence", "findingAskFollowUp",
			   "findingConvertToDraftComment", "findingConvertToDraftDefect",
			   "findingAddToPrivateNotes", "findingDismiss",
			   // The three that fill and walk the defect queue, then the five
			   // verdicts RFC-BPR-0009 and RFC-BPR-0010 give the selected
			   // defect (BPR-0520).
			   "classifySelectedDefect", "previousDefect", "nextDefect",
			   "defectVerify", "defectReopen", "defectWithdraw",
			   "defectAcceptAsIs", "defectReclassify" }) {
			auto* const button = offered(QString::fromLatin1(name));
			expect(
				button != nullptr && button->isVisible() && button->width() > 0
					&& button->height() > 0,
				std::string{ "the open drawer offers a visible button for " }
					+ name);
			expect(
				laid_into(button->parentWidget()->layout(), button),
				std::string{ "the drawer action is laid into the layout that "
							 "owns it, not merely parented to it: " }
					+ name);
			// And that the sweep saw it. A sweep that walked past the panels
			// would leave these names measured only by the loop that lists
			// them, which is the sample the card refused.
			expect(
				std::find(controls.begin(), controls.end(), button)
					!= controls.end(),
				std::string{ "the sweep reached the drawer action: " } + name);
			const QRect placed{
				button->mapTo(column->viewport(), QPoint{ 0, 0 }),
				button->size(),
			};
			expect(
				placed.left() >= 0
					&& placed.right() < column->viewport()->width(),
				std::string{ "the drawer action is inside the drawer's "
							 "visible width: " }
					+ name);
		}

		// The two rows BPR-0527 stacked, asserted as stacked rather than as
		// merely fitting. The width above is the width the product opens at,
		// and it is the reviewer's to change: the drawer drags down to
		// `minimum_workbench_drawer_width`, and at those widths what keeps a
		// row's last control reachable is that the row is a column. Measured
		// as the arrangement it is - the second control begins at or below the
		// bottom of the first, so the two do not share a line - because a
		// horizontal row of these three run controls asks for 366 px, nine
		// more than the stacked column asks for, and the difference is small
		// enough to hide inside the column's own margins at the opening
		// width while being lost at every narrower one.
		const auto stackedUnder = [](const QWidget* const aAbove,
									 const QWidget* const aBelow) {
			return aAbove != nullptr && aBelow != nullptr
				&& aBelow->mapTo(aBelow->window(), QPoint{ 0, 0 }).y()
				>= aAbove->mapTo(aAbove->window(), QPoint{ 0, 0 }).y()
					+ aAbove->height();
		};
		auto* const availability = drawer->findChild<QLabel*>(
			QStringLiteral("reviewThreadAvailability"));
		expect(
			availability != nullptr && availability->isVisible(),
			"the open drawer shows the review-thread availability sentence");
		expect(
			stackedUnder(availability, offered(QStringLiteral("showReviewThreadsAction"))),
			"the reveal action is stacked under the availability sentence rather than beside it: showReviewThreadsAction");
		expect(
			stackedUnder(
				offered(QStringLiteral("agentExistingCheckoutAction")),
				offered(QStringLiteral("startAgentAction")))
				&& stackedUnder(
					offered(QStringLiteral("startAgentAction")),
					offered(QStringLiteral("cancelAgentAction"))),
			"the agent launcher's run controls are stacked rather than laid in a row: startAgentAction, cancelAgentAction");

		// The one control in these rows that is not an action. BPR-0527's card
		// measured it 17 px past the viewport with its text still legible,
		// which is the failure that reads as success: the label was inside the
		// drawer, its right edge was not.
		auto* const defectCount = drawer->findChild<QLabel*>(
			QStringLiteral("defectCount"));
		expect(
			defectCount != nullptr && defectCount->isVisible(),
			"the open drawer shows the defect queue position");
		expect(
			std::find(controls.begin(), controls.end(), defectCount)
					!= controls.end()
				&& QRect{ defectCount->mapTo(column->viewport(), QPoint{ 0, 0 }),
						  defectCount->size() }
						.right()
					< column->viewport()->width(),
			"the defect queue position is inside the drawer's visible width");

		// The column is taller than the drawer, so the defect stack has to be
		// brought into the drawer's visible region before a capture is a
		// picture of it. Scrolling moves these rows down the drawer; what it
		// cannot do is move them across it, which is the whole point.
		auto* const first = offered(QStringLiteral("classifySelectedDefect"));
		auto* const last  = offered(QStringLiteral("defectReclassify"));
		column->verticalScrollBar()->setValue(
			column->verticalScrollBar()->value()
			+ first->mapTo(column->viewport(), QPoint{ 0, 0 }).y());
		QApplication::processEvents();
		const auto wholly = [column](const QWidget* const aWidget) {
			const QRect placed{
				aWidget->mapTo(column->viewport(), QPoint{ 0, 0 }),
				aWidget->size(),
			};
			return column->viewport()->rect().contains(placed);
		};
		expect(
			wholly(first) && wholly(last),
			"the defect stack from its first action to its last is inside the drawer's visible region");

		// The capture the card asks for, cropped to the drawer the actions are
		// in. A whole-window raster would differ on a repaint anywhere; this
		// one is the drawer and only the drawer.
		QApplication::processEvents();
		const auto rendered = window.grab();
		const auto ratio    = window.devicePixelRatioF();
		const auto scaled   = [ratio](const int aValue) {
			return static_cast<int>(
				std::lround(static_cast<double>(aValue) * ratio));
		};
		const auto  origin = drawer->mapTo(&window, QPoint{ 0, 0 });
		const QRect region{
			QPoint{ scaled(origin.x()), scaled(origin.y()) },
			QPoint{ scaled(origin.x() + drawer->width()) - 1,
					scaled(origin.y() + drawer->height()) - 1 },
		};
		const auto cropped = rendered.copy(region);
		expect(
			rendered.rect().contains(region) && !cropped.isNull()
				&& cropped.size() == region.size(),
			"the defect drawer crop has the drawer's exact device dimensions");
		const auto renderPath = qEnvironmentVariable("BPR_QT_RENDER_PATH");
		if (!renderPath.isEmpty()) {
			const QFileInfo configured{ renderPath };
			expect(
				cropped.save(
					QStringLiteral("%1/%2-defect-drawer.png")
						.arg(configured.path(), configured.completeBaseName()),
					"PNG"),
				"the defect drawer crop is saved in the ignored build tree");
			// And the column whole. The crop above is what the drawer shows at
			// one scroll position, and BPR-0527's claim is about every row, so
			// looking at it means looking at all of them. The column renders at
			// exactly the width a reviewer can reach, so a control past that
			// width is cut off in this picture as it is in the drawer.
			expect(
				column->widget()->grab().save(
					QStringLiteral("%1/%2-drawer-column.png")
						.arg(configured.path(), configured.completeBaseName()),
					"PNG"),
				"the whole drawer column is saved in the ignored build tree");
		}
	}

	/// \brief BPR-0526: device sign-in is mounted beside the token prompt.
	///
	/// \details Found the way an assistive reader finds it -- by the accessible
	/// name the panel announces -- and then asked the review header's own column
	/// whether it manages what was found, because a panel parented to the header
	/// but never laid into it is still a `findChildren` hit and would still be
	/// painted at the default geometry on top of the session chrome.
	///
	/// The gestures are the second half. The window supplies no dispatch, so the
	/// sink here is the test's; in the shipped graph it is the one the
	/// composition sets, which ends at the device host. What this case proves is
	/// that pressing begin on the mounted panel emits exactly one typed intent
	/// naming exactly the profile the session panel's picker names -- before and
	/// after the reviewer changes that picker -- and that nothing else leaves.
	auto test_device_sign_in_is_mounted_beside_the_session_chrome() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		window.resize(1'100, 900);
		window.show();
		QApplication::processEvents();
		// [[BPR-0646]]. Accounts live in Settings: the sign-in and the session
		// chrome are the Accounts and hosts page, sign-in first while signed
		// out, and the strip's account button opens that page.
		auto* const header
			= required_child<QWidget>(window, "workbenchAccountsPage");
		auto* found = static_cast<QWidget*>(nullptr);
		for (auto* const widget : header->findChildren<QWidget*>()) {
			if (widget->accessibleName()
				== QStringLiteral("Sign in with a device code")) {
				found = widget;
			}
		}
		expect(
			found != nullptr && found == window.device_authorization(),
			"the accounts page holds the device sign-in panel under its "
			"accessible name");
		auto* const panel = window.device_authorization();
		expect(
			panel->parentWidget() == header
				&& header->layout() != nullptr
				&& header->layout()->indexOf(panel) >= 0
				&& window.settings() != nullptr
				&& window.settings()
					   ->page_widget(qt::settings_page::accounts_and_hosts)
					   ->isAncestorOf(panel),
			"the device panel is laid into the accounts page's own column on "
			"the Settings window's Accounts and hosts page");
		auto* const session = window.session_panel();
		expect(
			session != nullptr && session->parentWidget() == header
				&& header->layout()->indexOf(panel) + 1
					== header->layout()->indexOf(session),
			"device sign-in is the page's first panel, directly above the "
			"session chrome that offers the token prompt");
		expect(
			!window.files_drawer()->isAncestorOf(panel)
				&& !window.files_drawer()->isAncestorOf(session),
			"neither sign-in nor the session chrome is in the files drawer");
		window.account_action()->trigger();
		QApplication::processEvents();
		expect(
			window.settings()->isVisible()
				&& window.settings()->current_page()
					== qt::settings_page::accounts_and_hosts
				&& panel->isVisible() && panel->width() > 0 && panel->height() > 0
				&& panel->mapTo(header, QPoint{ 0, 0 }).y()
					< session->mapTo(header, QPoint{ 0, 0 }).y()
				&& panel->mapTo(window.settings(), QPoint{ 0, 0 }).y()
						+ panel->height()
					<= window.settings()->height(),
			"the strip's account button presents Accounts and hosts with the "
			"sign-in panel visible above the Connect control, without "
			"scrolling");
		expect(
			!window.focus_order().contains(panel->begin_button())
				&& !window.focus_order().contains(session->connect_button()),
			"the workbench declares no focus stops across windows");

		publish_session_ready(window);
		publish_pull_request(window);
		QApplication::processEvents();
		expect(
			!panel->isVisible(),
			"once a session is accepted the sign-in panel is hidden rather than "
			"left as noise above the pull request");

		// The window sets no dispatch of its own, so this one is the test's,
		// exactly as it is for the two agent surfaces above.
		auto gestures = std::vector<application::device_authorization_gesture>{ };
		window.set_device_authorization_dispatch(
			[&gestures](application::device_authorization_gesture aGesture) {
				gestures.push_back(std::move(aGesture));
			});

		const auto selected = session->selected_profile();
		expect(
			selected.has_value() && panel->bound_profile() == *selected,
			"the mounted panel is bound to the profile the picker already names");
		expect(
			panel->begin_button()->isEnabled(),
			"asking for a code is the one command the core allows before a flow");
		panel->begin_button()->click();
		QApplication::processEvents();
		expect(
			gestures.size() == 1U
				&& std::holds_alternative<core::begin_device_authorization_intent>(
					gestures.front())
				&& std::get<core::begin_device_authorization_intent>(
					   gestures.front())
						.Profile
					== *selected,
			"pressing begin emits one intent naming the picker's own profile");
		// The core answers a begin by moving the status; the shell shows the
		// panel again for the flow it started, exactly as it does for a
		// signed-out window.
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::establishing_session } } });
		QApplication::processEvents();
		expect(
			panel->isVisible(),
			"a flow the core is establishing shows the sign-in panel again");

		// The picker names the host the Connect button acts on. Sign-in has to
		// mean the same host through both doors, so changing it here changes
		// which profile a new code would be asked for.
		const auto first = session->profile_selector()->currentIndex();
		session->profile_selector()->setCurrentIndex(first == 0 ? 1 : 0);
		QApplication::processEvents();
		const auto changed = session->selected_profile();
		expect(
			changed.has_value() && *changed != *selected
				&& panel->bound_profile() == *changed,
			"changing the host picker rebinds the mounted panel to that profile");
		panel->begin_button()->click();
		QApplication::processEvents();
		expect(
			gestures.size() == 2U
				&& std::get<core::begin_device_authorization_intent>(
					   gestures.back())
						.Profile
					== *changed,
			"the next request names the profile the picker changed to");

		// One published presentation, applied through the same call the
		// composition's fan-out makes. It is what turns the two gestures below
		// into legal ones, and it is what the capture is a picture of.
		auto awaiting = application::device_authorization_presentation{
			.Stage                    = core::device_authorization_stage::awaiting_authorization,
			.UserCode                 = "WDJB-MJHT",
			.VerificationUri          = "https://github.com/login/device",
			.RemainingValiditySeconds = 899U,
		};
		awaiting.Commands = core::device_authorization_commands(
			core::device_authorization_state{
				.Stage = core::device_authorization_stage::awaiting_authorization,
			});
		panel->apply(awaiting);
		QApplication::processEvents();
		expect(
			panel->user_code_field()->text() == QStringLiteral("WDJB-MJHT")
				&& panel->address_field()->text()
					== QStringLiteral("https://github.com/login/device"),
			"a published presentation reaches the mounted panel's own fields");

		panel->open_button()->click();
		panel->cancel_button()->click();
		QApplication::processEvents();
		expect(
			gestures.size() == 4U
				&& std::holds_alternative<
					application::open_verification_address_gesture>(
					gestures[2])
				&& std::holds_alternative<
					core::cancel_device_authorization_intent>(gestures[3]),
			"opening a browser and cancelling both leave through the same sink "
			"and neither is performed here");
		window.set_device_authorization_dispatch({ });

		// The panel is at the top of the accounts page, so the capture is a
		// picture of it without scrolling anything.
		auto* const headerScroll = window.settings()->page_widget(
			qt::settings_page::accounts_and_hosts);
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		const QRect placed{
			panel->mapTo(headerScroll, QPoint{ 0, 0 }),
			panel->size(),
		};
		expect(
			headerScroll->rect().intersected(placed).height()
				>= placed.height() / 2,
			"the accounts page shows the device panel inside it");

		// Cropped to the page the panel is on. A whole-window raster would
		// pass on a repaint anywhere; this one is the page and only the page.
		QApplication::processEvents();
		auto* const settingsWindow = window.settings();
		const auto  rendered       = settingsWindow->grab();
		const auto  ratio          = settingsWindow->devicePixelRatioF();
		const auto  scaled         = [ratio](const int aValue) {
			return static_cast<int>(
				std::lround(static_cast<double>(aValue) * ratio));
		};
		const auto origin = headerScroll->mapTo(settingsWindow, QPoint{ 0, 0 });
		// Two rounded edges rather than a rounded origin plus a rounded size,
		// for the reason BPR-0520 recorded one crop below.
		const QRect region{
			QPoint{ scaled(origin.x()), scaled(origin.y()) },
			QPoint{ scaled(origin.x() + headerScroll->width()) - 1,
					scaled(origin.y() + headerScroll->height()) - 1 },
		};
		const auto cropped = rendered.copy(region);
		expect(
			rendered.rect().contains(region) && !cropped.isNull()
				&& cropped.size() == region.size(),
			"the accounts page crop has the page's exact device dimensions");
		const auto renderPath = qEnvironmentVariable("BPR_QT_RENDER_PATH");
		if (!renderPath.isEmpty()) {
			const QFileInfo configured{ renderPath };
			expect(
				cropped.save(
					QStringLiteral("%1/%2-device-sign-in.png")
						.arg(configured.path(), configured.completeBaseName()),
					"PNG"),
				"the device sign-in crop is saved in the ignored build tree");
		}
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewGitHubQtTests"));

	test_conversation_model_is_stable_and_incremental();
	test_intent_translation_is_narrow_and_account_is_output_only();
	test_presentation_states_disclosures_and_responsive_layout();
	test_connection_diagnostics_can_be_copied();
	test_capability_refusal_detail_is_copyable_and_cleared();
	test_bootstrap_stage_messages_are_visible_copyable_and_cleared();
	test_cancellation_states_and_failure_layouts();
	test_identity_metadata_commands_and_refresh_semantics();
	test_status_diagnostics_and_host_detail_are_complete();
	test_native_inheritance_and_wrapped_conversation_layout();
	test_accessible_announcements_are_combined_and_deduplicated();
	test_accessibility_minimum_geometry_and_render();
	test_agent_surfaces_are_placed_in_the_conversations_drawer();
	test_drawer_controls_are_reachable_in_the_conversations_drawer();
	test_device_sign_in_is_mounted_beside_the_session_chrome();

	std::cout << "All GitHub Qt presentation tests passed\n";
	return EXIT_SUCCESS;
}
