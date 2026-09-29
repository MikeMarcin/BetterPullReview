#include "bpr/application/github_window_read_host.hpp"
#include "bpr/application/manual_executor.hpp"
#include "diff_publication_fixture.hpp"

#include "bpr/core/diff_compute.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/workbench_window.hpp"

#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/github_cli_consent.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/side_chat_panel.hpp"

#include <QAbstractButton>
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
#include <QDeadlineTimer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileInfo>
#include <QFontInfo>
#include <QFontMetrics>
#include <QImage>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QStyleOptionViewItem>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeView>
#include <QWidget>

#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/github_conversation_panel.hpp"

#include <QKeyEvent>
#include <QMouseEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
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

	// [[BPR-0771]] D2, [[BPR-0772]]. RFC-BPR-0015's trust warning belongs to a
	// server with a GitHub CLI profile, on that server's card where the opt-in
	// is made, whichever host is chosen. The picker it once followed is gone.
	// The card's header names the host, so the warning says "this server" and
	// the host is named once on the card (RFC-BPR-0032 A4).
	void test_cli_warning_lives_on_its_servers_card()
	{
		qt::workbench_window window{ qt::workbench_intent_dispatch{ }, qt::workbench_change_dispatch{ }, qt::workbench_drawer_width_dispatch{ }, profiles(), [](application::github_intent) { } };
		auto* const          warning    = required_child<QLabel>(window, "githubCliTrustWarning");
		auto* const          cliCard    = window.server_card_for(core::host_profile_id{ 11U });
		auto* const          nativeCard = window.server_card_for(core::host_profile_id{ 12U });
		expect(cliCard != nullptr && nativeCard != nullptr && cliCard != nativeCard && cliCard->cli_trust_warning() == warning
				   && cliCard->isAncestorOf(warning) && nativeCard->cli_trust_warning() == nullptr
				   && nativeCard->findChild<QLabel*>(QStringLiteral("githubCliTrustWarning")) == nullptr
				   && window.findChildren<QLabel*>(QStringLiteral("githubCliTrustWarning")).size() == 1,
			   "the CLI trust warning is on the card of the server with a GitHub CLI profile only, and the native server's card carries none");
		expect(cliCard->cli_offer_button() != nullptr && nativeCard->cli_offer_button() == nullptr
				   && !cliCard->cli_offer_button()->isVisibleTo(cliCard),
			   "only the server with a GitHub CLI profile has the GitHub CLI offer, and it is not offered while GitHub CLI is already the card's way in");
		expect(!warning->isHidden() && warning->wordWrap() && warning->textFormat() == Qt::PlainText,
			   "the CLI card shows a wrapped literal warning before Connect");
		for (const auto* phrase : { "this server", "existing gh sign-in", "observed after Connect", "native profile", "executable", "configuration", "socket", "CA", "redirects", "child-process", "account switching", "no exact account guarantee", "older gh configuration", "shell credential helper", "gh 2.100.0 or 2.101.0", "32 MiB", "does not authorize publishing" }) {
			expect(warning->text().contains(QString::fromLatin1(phrase)), "CLI warning states each residual trust risk and read boundary");
		}
		const auto host = QString::fromStdString(cliCard->hostname());
		expect(!host.isEmpty() && cliCard->host_label()->text() == host
				   && !warning->text().contains(host, Qt::CaseInsensitive),
			   "the warning says this server rather than naming the host its card's header already names");
		// Profile projection is independent of the shared sign-in dialog,
		// whose native setup hides cards for unrelated hosts.
		window.connect_github_profile(core::host_profile_id{ 12U });
		expect(window.session_panel()->selected_profile() == core::host_profile_id{ 12U } && !warning->isHidden()
				   && warning->text().contains(QStringLiteral("this server"))
				   && !warning->text().contains(host, Qt::CaseInsensitive),
			   "choosing the native host leaves the warning on the CLI card, where the opt-in is made, still without the host");
		window.connect_github_profile(core::host_profile_id{ 11U });
		expect(window.session_panel()->selected_profile() == core::host_profile_id{ 11U } && !warning->isHidden(),
			   "choosing the CLI host again keeps its exact host warning");
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status   = core::github_status_delta{ .Status = core::github_status::establishing_session },
				.Identity = core::github_identity_delta{ .Profile = core::host_profile_id{ 12U } },
			},
		});
		expect(!warning->isHidden() && nativeCard->cli_trust_warning() == nullptr
				   && cliCard->connect_button()->isVisibleTo(cliCard),
			   "a projected native identity leaves the warning beside the CLI card's Connect, before it replaces that session");
	}

	// [[BPR-0933]]. The owner found the read opt-in "too long and visually
	// unwieldy": one bold paragraph of every residual risk. The decision is now
	// a headline and its consequences, and the risk list sits behind the box's
	// own Show Details disclosure. None of the trust it asks for is dropped,
	// Connect is the only way to accept, and Return and Escape both decline.
	void test_cli_read_consent_is_a_short_decision()
	{
		QWidget parent;
		parent.resize(1100, 760);
		const auto host     = QStringLiteral("ghe.example.invalid");
		const auto question = qt::make_github_cli_read_consent(&parent, QStringLiteral("Use configured GitHub CLI"), host);
		const auto decision = question->text() + QLatin1Char(' ') + question->informativeText();
		const auto words    = decision.split(QRegularExpression{ QStringLiteral("\\s+") }, Qt::SkipEmptyParts).size();
		expect(question->icon() == QMessageBox::Warning && question->textFormat() == Qt::PlainText
				   && question->text().contains(host) && question->text().contains(QStringLiteral("installed gh"))
				   && question->text().endsWith(QLatin1Char('?')),
			   "the read opt-in's headline is a plain-text question naming the exact host and the installed gh");
		for (const auto* phrase : { "account gh is signed in to", "can change outside this app", "executable", "configuration", "certificates", "does not authorize publishing", "asks separately" }) {
			expect(decision.contains(QString::fromLatin1(phrase)),
				   std::string{ "the short decision still states the trust it asks for and the read boundary: " } + phrase);
		}
		expect(words <= 70, "the decision before the disclosure reads in one breath, at most 70 words, not the former paragraph of every risk");
		const auto details = question->detailedText();
		for (const auto* phrase : { "no per-request account selector", "no exact account guarantee", "redirects", "socket routing", "CA certificates", "child-process", "environment overrides", "needs migration", "temporary object store", "shell credential helper", "gh 2.100.0 or 2.101.0", "Apple Git 2.50.1", "32 MiB", "supervised temporary-store limit", "never reads, stores or passes your gh token", "own confirmation" }) {
			expect(details.contains(QString::fromLatin1(phrase)),
				   std::string{ "every residual risk the long warning named is behind the disclosure: " } + phrase);
		}

		auto* const connect = question->button(QMessageBox::Yes);
		auto* const cancel  = question->button(QMessageBox::Cancel);
		expect(connect != nullptr && cancel != nullptr && connect->text() == QStringLiteral("Connect")
				   && question->defaultButton() == cancel && question->escapeButton() == cancel,
			   "Connect is the only accepting button, and Cancel is both the default and the escape button");
		const auto pushButtons = question->findChildren<QPushButton*>();
		const auto disclosure  = std::ranges::find_if(pushButtons, [connect, cancel](const QPushButton* aButton) {
			return aButton != connect && aButton != cancel && aButton->text().contains(QStringLiteral("Details"));
		});
		expect(disclosure != pushButtons.end() && ((*disclosure)->focusPolicy() & Qt::TabFocus) != 0
				   && pushButtons.size() == 3,
			   "the risk list has one keyboard-reachable Show Details disclosure beside Connect and Cancel");
		auto* const accessibleDisclosure = QAccessible::queryAccessibleInterface(*disclosure);
		expect(accessibleDisclosure != nullptr && accessibleDisclosure->role() == QAccessible::Button
				   && accessibleDisclosure->text(QAccessible::Name).contains(QStringLiteral("Details")),
			   "the disclosure is announced as a button that names what it shows");

		parent.show();
		question->show();
		QApplication::processEvents();
		const auto detailText = [&question] {
			const auto edits = question->findChildren<QTextEdit*>();
			return edits.size() == 1 ? edits.front() : nullptr;
		};
		expect(detailText() == nullptr || !detailText()->isVisible(), "the residual-risk list is closed until it is asked for");
		const auto closedHeight = question->height();
		const auto closed       = question->grab();
		(*disclosure)->click();
		QApplication::processEvents();
		expect(detailText() != nullptr && detailText()->isVisible() && detailText()->isReadOnly()
				   && detailText()->toPlainText() == details && question->height() > closedHeight,
			   "Show Details opens the complete read-only risk list inside the same question");
		expect(!closed.isNull() && closed.width() > 0 && closed.height() > 0, "the closed question renders");
		if (const auto renderPath = qEnvironmentVariable("BPR_QT_RENDER_PATH"); !renderPath.isEmpty()) {
			const QFileInfo configured{ renderPath };
			const auto      prefix = QStringLiteral("%1/%2-cli-read-consent").arg(configured.path(), configured.completeBaseName());
			expect(closed.save(prefix + QStringLiteral("-closed.png"), "PNG") && question->grab().save(prefix + QStringLiteral("-details.png"), "PNG"),
				   "the closed and disclosed CLI read question renders are saved in the ignored build tree");
		}

		// On macOS the box animates its escape button's click, which lands
		// on a later timer rather than inside this event.
		QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(question.get(), &escape);
		QDeadlineTimer settled{ 2000 };
		while (question->clickedButton() == nullptr && !settled.hasExpired()) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		expect(question->clickedButton() == cancel && question->isHidden(), "Escape declines the read");

		const auto again = qt::make_github_cli_read_consent(&parent, QStringLiteral("Use configured GitHub CLI"), host);
		again->show();
		QApplication::processEvents();
		QKeyEvent enter{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(again.get(), &enter);
		QApplication::processEvents();
		expect(again->clickedButton() == again->button(QMessageBox::Cancel) && again->isHidden(),
			   "Return presses the default, so it declines too; only Connect accepts");
		parent.hide();
	}

	/// \brief [[BPR-1140]]. The GitHub CLI read opt-in names the machine the
	/// way the rest of the product does ([[BPR-0773]]): "this Mac" on macOS,
	/// "this computer" on Windows and Linux. The sentence is otherwise
	/// [[BPR-0933]]'s, word for word. Each platform's build checks its own
	/// sentence, so the Windows and Linux wording is proved on those platforms.
	auto test_cli_read_consent_names_this_machine() -> void
	{
		QWidget    parent;
		const auto question = qt::make_github_cli_read_consent(&parent, QStringLiteral("Use configured GitHub CLI"), QStringLiteral("ghe.example.invalid"));
#if defined(__APPLE__)
		const auto expected = QStringLiteral(
			"BetterPullReview will read as the account gh is signed in to, and that account can change "
			"outside this app. You trust this Mac's gh executable, configuration and certificates as they "
			"are set up. Connecting for reads does not authorize publishing; each review or comment asks "
			"separately.");
#else
		const auto expected = QStringLiteral(
			"BetterPullReview will read as the account gh is signed in to, and that account can change "
			"outside this app. You trust this computer's gh executable, configuration and certificates as "
			"they are set up. Connecting for reads does not authorize publishing; each review or comment "
			"asks separately.");
#endif
		const auto said = question->informativeText();
		if (said != expected) {
			std::cerr << "The GitHub CLI read consent says: " << said.toStdString() << '\n';
		}
		expect(said == expected, "the GitHub CLI read consent names this platform's machine in the accepted sentence");
#if !defined(__APPLE__)
		// A word, so that "Machine configuration" in the details is not one.
		const QRegularExpression mac{ QStringLiteral("\\bMac\\b") };
		expect(!question->text().contains(mac) && !said.contains(mac) && !question->detailedText().contains(mac),
			   "off macOS the GitHub CLI read consent does not say Mac");
#endif
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

	[[nodiscard]] auto session_projection(
		const core::host_profile_id aProfile = core::host_profile_id{ 12U })
		-> application::github_session_projection
	{
		return {
			.Profile            = aProfile,
			.Host               = core::git_host_id{ 22U },
			.Account            = core::github_account_id{ 52U },
			.Login              = "octo-reviewer",
			.DisplayName        = "Octo Reviewer",
			.Capabilities       = capabilities(),
			.CapabilityRevision = core::capability_revision{ 7U },
			.RateLimits         = rate_limits(),
			.Grant              = application::github_grant_projection{
				.Kind       = application::github_grant_kind_projection::device_grant,
				.SignedInAt = 1'757'000'000,
				.Renews     = true,
				.Remembered = true,
			},
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

		// [[BPR-0771]]. One card per configured host, in profile order; the
		// picker these replaced is gone, and choosing a host is a model fact.
		expect(window.server_cards().size() == 2U
				   && window.server_cards().front()->profile() == core::host_profile_id{ 11U }
				   && window.server_cards().back()->profile() == core::host_profile_id{ 12U },
			   "two profiles are presented as two cards in profile order");
		window.session_panel()->select_profile(core::host_profile_id{ 12U });
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

		// [[BPR-0771]]. Open names what the Home bar names. The bar's text
		// leaves as the setup destination, and the application decides what it
		// names; the locator that once split a pasted link into three fields is
		// gone ([[BPR-0689]] made the bar the one place a pull request is named).
		auto submitted = std::vector<std::string>{ };
		window.set_setup_destination_dispatch([&submitted](application::setup_destination_intent aIntent) {
			if (const auto* const submit = std::get_if<application::submit_setup_destination>(&aIntent);
				submit != nullptr) {
				submitted.push_back(submit->Text);
			}
		});
		window.github_open_action()->trigger();
		expect(submitted.empty() && intents.size() == 1U && window.showing_home(),
			   "Open with nothing in the bar submits nothing and shows Home");
		const auto link = std::string{ "https://github.com/better-pull-review/desktop/pull/73" };
		window.home_open_bar()->setText(QString::fromStdString(link));
		window.github_open_action()->trigger();
		expect(submitted.size() == 1U && submitted.back() == link && intents.size() == 1U,
			   "Open submits exactly the bar's text once and emits no GitHub intent of its own");

		window.github_refresh_action()->trigger();
		expect(
			intents.size() == 2U
				&& std::holds_alternative<core::refresh_pull_request_intent>(
					intents.back()),
			"Refresh emits only refresh_pull_request_intent");

		expect(
			intents.size() == 2U && !window.show_review_threads_action()
				&& !window.findChild<QAction*>(QStringLiteral("showReviewThreadsAction")),
			"the removed focus-only review-thread action emits nothing and leaves no silent control");

		expect(
			!window.github_connect_action()->shortcut().isEmpty()
				&& !window.github_open_action()->shortcut().isEmpty()
				&& !window.github_refresh_action()->shortcut().isEmpty(),
			"primary actions expose keyboard shortcuts");
		expect(
			window.active_account_text() == "Not authenticated",
			"account identity is initially read-only authenticated output");

		// The card's Connect is how a keyboard or an assistive reader chooses
		// a host now.
		auto* const cliCard    = window.server_card_for(core::host_profile_id{ 11U });
		auto* const accessible = cliCard == nullptr
			? nullptr
			: QAccessible::queryAccessibleInterface(cliCard->connect_button());
		expect(
			accessible != nullptr
				&& cliCard->connect_button()->isVisibleTo(cliCard)
				&& accessible->text(QAccessible::Name) == QStringLiteral("Connect github.com")
				&& !accessible->text(QAccessible::Description).isEmpty()
				&& accessible->state().focusable,
			"a card's Connect exposes an accessible name, a description and focusable state");
	}

	auto publish_session_ready(
		qt::workbench_window&       aWindow,
		const core::host_profile_id aProfile = core::host_profile_id{ 12U }) -> void
	{
		const auto session = session_projection(aProfile);
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

	struct loading_transport final : application::github_account_transport_lease
	{ };
	auto loading_session() -> application::github_profile_session
	{
		application::github_profile_session result{
			.BprProfile = core::default_bpr_profile_id, .ApplicationSession = core::application_session_id{ 1 }, .ProfileAuthority = { .Profile = core::host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = core::git_host_id{ 1 }, .Hostname = application::github_canonical_hostname{ "github.com" }, .ProbeContractRevision = application::github_probe_contract_revision{ 1 } }, .Session = { .Profile = core::host_profile_id{ 1 }, .Host = core::git_host_id{ 1 }, .Viewer = { .Account = core::github_account_id{ 1 }, .Node = application::github_actor_id{ "actor" }, .Login = "reader" }, .CapabilityEvidence = { .Revision = core::capability_revision{ 1 }, .ContractVersion = 1 } }, .Generation = core::github_identity_generation{ 1 }, .CapabilityEvidence = { .Authority = { .Profile = core::host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = core::git_host_id{ 1 }, .Account = core::github_account_id{ 1 }, .ProbeContractRevision = application::github_probe_contract_revision{ 1 } }, .Revision = core::capability_revision{ 1 } }, .AccountTransport = std::make_shared<loading_transport>()
		};
		return result;
	}

	void test_held_read_paints_loading()
	{
		struct held_read final : application::github_pull_request_port
		{
			std::optional<application::github_pull_request_provider_request> Request;
			application::github_pull_request_completion_handler              Complete;
			application::github_pull_request_progress_handler                Progress;
			application::github_pull_request_overview_handler                Overview;
			std::function<void()>                                            OnSubmit;
			void                                                             fetch_pull_request(application::github_pull_request_provider_request   aRequest,
																								application::github_pull_request_completion_handler aComplete) override
			{
				OnSubmit();
				Request  = std::move(aRequest);
				Complete = std::move(aComplete);
			}
			void cancel_pull_request(application::github_pull_request_provider_request) override { }
			void fetch_pull_request_with_progress(application::github_pull_request_provider_request   aRequest,
												  application::github_pull_request_completion_handler aComplete,
												  application::github_pull_request_progress_handler   aProgress) override
			{
				Progress = std::move(aProgress);
				fetch_pull_request(std::move(aRequest), std::move(aComplete));
			}
			void fetch_pull_request_with_observations(application::github_pull_request_provider_request   aRequest,
													  application::github_pull_request_completion_handler aComplete,
													  application::github_pull_request_progress_handler   aProgress,
													  application::github_pull_request_overview_handler   aOverview) override
			{
				Overview = std::move(aOverview);
				fetch_pull_request_with_progress(std::move(aRequest), std::move(aComplete), std::move(aProgress));
			}
		} provider;
		struct paint_observer final : QObject
		{
			std::size_t Paints = 0U;
			bool        eventFilter(QObject*, QEvent* aEvent) override
			{
				if (aEvent->type() == QEvent::Paint) {
					++Paints;
				}
				return false;
			}
		} observer;
		application::manual_executor          lane;
		application::github_window_read_host* readHost = nullptr;
		qt::workbench_window                  window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[&](application::github_intent aIntent) {
				if (const auto* open = std::get_if<core::open_pull_request_intent>(&aIntent)) {
					readHost->dispatch(*open);
				}
			},
		};
		application::github_window_read_host host{
			{ core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } },
			core::application_session_id{ 2 },
			provider,
			lane,
			[&](const application::github_window_read_update& aUpdate) { window.publish(aUpdate.Presentation); }
		};
		readHost = &host;
		expect(host.adopt(loading_session()), "held read adopts synthetic session");
		// The shipped workbench publishes drawer commands even with no review.
		window.publish(application::workbench_projection{
			.Commands = core::project_workbench_commands({ }) });
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{ .Commands = enabled_commands() } });
		window.resize(900, 720);
		window.show();
		QApplication::processEvents();
		auto* title = window.epoch_selectors()->comparing_button();
		title->installEventFilter(&observer);
		provider.OnSubmit = [&] {
			expect(presentation_state(window) == QStringLiteral("loading"),
				   "loading is projected before provider submission");
			// [[BPR-1200]]. Nothing yet shows that this connection can read the
			// pull request, so the read is said on Home rather than on an
			// empty Overview a refusal would have to take away again.
			expect(window.showing_home() && !window.overview_page()->isVisible(),
				   "before the read has shown anything the window stays on Home");
		};
		// [[BPR-0771]]. The pull request is named in the Home bar. What the bar
		// names is the application's decision, so this fixture stands in for it
		// and resolves exactly the one text written here.
		window.set_setup_destination_dispatch([&readHost](application::setup_destination_intent aIntent) {
			const auto* const submit = std::get_if<application::submit_setup_destination>(&aIntent);
			if (submit != nullptr && submit->Text == "owner/repo#1") {
				readHost->dispatch(core::open_pull_request_intent{
					.Repository = core::repository_locator{ .Owner = "owner", .Name = "repo" },
					.Number     = core::pull_request_number{ 1U },
				});
			}
		});
		window.home_open_bar()->setText(QStringLiteral("owner/repo#1"));
		window.github_open_action()->trigger();
		expect(provider.Request.has_value(), "public Open action reaches held provider");
		QApplication::processEvents();
		expect(presentation_state(window) == QStringLiteral("loading") && !host.accepted_snapshot()
				   && window.showing_home() && !window.overview_page()->isVisible()
				   && window.home_open_notice()->isVisible()
				   && window.home_open_notice()->text() == QStringLiteral("Loading the pull request."),
			   "[[BPR-1200]] the held read is said under the Home bar until it shows what it names");
		expect(title->text().contains(QStringLiteral("Loading"))
				   && !window.overview_page()->presented() && window.overview_page()->file_model()->file_count() == 0U,
			   "the prepared loading overview does not manufacture accepted content");
		expect(window.home_open_details_button()->isHidden(),
			   "the loading sentence has no Copy details");
		auto* const stack = required_child<QStackedWidget>(window, "githubViewState");
		expect(stack->isHidden() && !stack->isVisible() && !window.home_page()->isAncestorOf(stack),
			   "the stack's page is not shown and is not part of Home");
		QApplication::processEvents();
		const auto overview = application::github_pull_request_overview_observation{
			.Context       = provider.Request->Context,
			.Repository    = { .Repository = core::repository_id{ 1U }, .Node = application::github_node_id{ "repository" }, .Owner = "owner", .Name = "repo", .DefaultBranch = "main" },
			.PullRequest   = { .PullRequest = core::pull_request_id{ 2U }, .Repository = core::repository_id{ 1U }, .Number = core::pull_request_number{ 1U }, .Node = application::github_node_id{ "pr" }, .Title = "<b>Literal provisional title</b>", .Body = "Description before file acquisition.", .BaseOid = application::commit_oid{ std::string(40U, '1') }, .HeadOid = application::commit_oid{ std::string(40U, '2') }, .BaseRef = "main", .HeadRef = "topic", .ReportedCommitCount = 3U },
			.ReportedFiles = 1523U
		};
		provider.Overview(overview);
		lane.drain();
		QApplication::processEvents();
		expect(!window.showing_home() && window.overview_page()->isVisible() && title->isVisible()
				   && title->text().contains(QStringLiteral("Loading")),
			   "[[BPR-1200]] the read's overview presents the workbench while provider completion remains held");
		expect(observer.Paints > 0U, "the loading title paints while provider completion remains held");
		auto* observedTitle = required_child<QLabel>(window, "overviewTitle");
		expect(observedTitle->text() == QStringLiteral("<b>Literal provisional title</b>")
				   && observedTitle->textFormat() == Qt::PlainText
				   && !window.overview_page()->entry_button()->isEnabled() && !window.overview_page()->presented(),
			   "validated metadata paints literal useful content but grants no review authority");
		expect(required_child<QLabel>(window, "overviewSummary")->text().contains(QStringLiteral("3 commits"))
				   && required_child<QLabel>(window, "overviewSummary")->text().contains(QStringLiteral("Open. Provisional metadata")),
			   "provisional metadata names its observed state and reported commit count");
		for (std::uint32_t page = 1U; page <= 100U; ++page) {
			provider.Progress(provider.Request->Context, { .Phase = application::github_read_phase::files, .PagesReceived = page, .Sequence = page });
		}
		expect(lane.pending() == 1U, "progress coalesces into one decision task");
		lane.drain();
		expect(title->text().contains(QStringLiteral("changed files")) && title->text().contains(QStringLiteral("100 pages")),
			   "latest actual stage and received-page count are visible");
		capture_layout_state(window, QStringLiteral("loading-overview-progress"));
		const auto currentProgress = title->text();
		provider.Progress(provider.Request->Context, { .Phase = application::github_read_phase::metadata, .PagesReceived = 1U, .Sequence = 1U });
		auto wrongContext       = provider.Request->Context;
		wrongContext.Generation = core::request_generation{ 999 };
		provider.Progress(wrongContext, { .Phase = application::github_read_phase::comments, .PagesReceived = 101U, .Sequence = 101U });
		lane.drain();
		expect(title->text() == currentProgress, "older and foreign progress cannot replace the current read");
		host.cancel();
		provider.Progress(provider.Request->Context, { .Phase = application::github_read_phase::commits, .PagesReceived = 101U, .Sequence = 102U });
		provider.Complete(application::github_pull_request_cancelled{ *provider.Request });
		lane.drain();
		expect(host.is_quiescent(), "held read cancellation drains its terminal");
		expect(!title->isVisible() && window.showing_home(), "cancelled progress cannot keep a loading overview alive");
		expect(observedTitle->text() == QStringLiteral("No pull request"), "cancel clears provisional metadata");
		provider.Overview(overview);
		lane.drain();
		expect(observedTitle->text() == QStringLiteral("No pull request"), "late metadata cannot resurrect the cancelled preview");
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

		auto* const viewState       = required_child<QStackedWidget>(window, "githubViewState");
		auto* const emptyState      = required_child<QWidget>(window, "githubEmptyState");
		auto* const emptyTitle      = required_label(window, "githubEmptyStateTitle");
		auto* const emptyBody       = required_label(window, "githubEmptyStateBody");
		auto* const technicalButton = required_child<QToolButton>(
			window, "githubTechnicalDetailsButton");
		auto* const technicalDetails  = required_child<QWidget>(window, "githubTechnicalDetails");
		auto* const conversationState = required_child<QStackedWidget>(window, "githubConversationState");
		auto* const conversationEmpty = required_child<QWidget>(window, "githubConversationEmptyState");
		auto* const addRuleButton     = required_child<QToolButton>(window, "addObserverRuleButton");
		auto* const removeRuleButton  = required_child<QToolButton>(window, "removeObserverRuleButton");
		// [[BPR-0771]]. The status banner, the connection disclosure and the
		// Connect and Refresh buttons left Accounts & hosts. A session's state
		// and Details are its host's card, and Refresh is the strip's.
		auto* const cliCard        = window.server_card_for(core::host_profile_id{ 11U });
		auto* const enterpriseCard = window.server_card_for(core::host_profile_id{ 12U });
		expect(cliCard != nullptr && enterpriseCard != nullptr
				   && window.findChild<QWidget*>(QStringLiteral("githubStatusBanner")) == nullptr
				   && window.findChild<QWidget*>(QStringLiteral("githubConnectionDetailsButton")) == nullptr
				   && window.findChild<QWidget*>(QStringLiteral("connectGithubButton")) == nullptr
				   && window.findChild<QWidget*>(QStringLiteral("refreshPullRequestButton")) == nullptr,
			   "the window composes a card per host and none of the retired session chrome");

		// A session's state is its card's, on the Settings window's accounts
		// page, so that page is presented for the states it shows.
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		expect(
			cliCard->isVisible()
				&& cliCard->state_label()->text() == QStringLiteral("Signed out")
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
		// [[BPR-0671]]. The owner met "the entire set of information for this
		// PR crammed into a tiny box in files browser". The summary is the
		// overview's; the files drawer holds files, and the box that carried
		// it is not in the drawer and is not shown over a review.
		expect(!window.files_drawer()->isAncestorOf(viewState) && !viewState->isVisible()
				   && window.files_drawer()->isAncestorOf(window.files_tree()),
			   "the pull request summary is not in the files drawer while a review is open");

		// The session's card is on the Settings window's accounts page.
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		// [[BPR-0671]]. The two disclosures no longer share a surface: the
		// connection is its host's card's Details, on Settings ([[BPR-0771]]),
		// and the commit oids are the overview's, beside the branches they
		// identify.
		auto* const connectionButton  = enterpriseCard->details_button();
		auto* const connectionDetails = enterpriseCard->details();
		expect(
			enterpriseCard->presentation().Current
				&& connectionButton->isVisible() && !connectionDetails->isVisible()
				&& (connectionButton->focusPolicy() & Qt::TabFocus) != 0,
			"the connection disclosure is focusable and collapsed on the session's card");
		expect(
			window.overview_page()->isAncestorOf(technicalButton)
				&& window.overview_page()->isAncestorOf(technicalDetails)
				&& !technicalDetails->isVisibleTo(window.overview_page())
				&& (technicalButton->focusPolicy() & Qt::TabFocus) != 0,
			"the commit-oid disclosure is on the overview, focusable and collapsed");

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
		// The queue is Home's, hidden while a pull request is open, so its
		// order is a claim on the full declaration rather than on what is
		// reachable now.
		// [[BPR-0771]]. The cards' controls are the Settings window's stops,
		// and Refresh is the strip's, declared with the strip.
		auto* const refreshPresenter = window.primary_toolbar()->widgetForAction(window.github_refresh_action());
		expect(
			std::ranges::none_of(
				window.server_cards(),
				[&declaredTabStops](const qt::server_card* const aCard) {
					return declaredTabStops.contains(aCard->connect_button())
						|| declaredTabStops.contains(aCard->sign_in_button())
						|| declaredTabStops.contains(aCard->details_button());
				})
				&& refreshPresenter != nullptr
				&& window.focus_order().contains(refreshPresenter)
				// [[BPR-0783]]. Home's three controls are the strip's stops, in
				// the strip's reading order before the refresh glyph; the six
				// typed queue controls are Settings' Queue defaults and are no
				// longer declared here at all.
				&& containsInOrder(
					window.focus_order(),
					{
						window.home_group_button(),
						window.primary_toolbar()->widgetForAction(window.home_state_headers_action()),
						window.home_filter_button(),
						refreshPresenter,
						technicalButton,
					})
				&& !window.focus_order().contains(window.queue_role_filter())
				&& !window.focus_order().contains(window.queue_sort_selector())
				// [[BPR-0652]]. The rule editor is Reviews and queue's now, in the
				// Settings window; the workbench declares no stops across windows.
				&& !window.focus_order().contains(window.queue_rule_kind_selector())
				&& !window.focus_order().contains(addRuleButton)
				&& window.rules_editor()->isAncestorOf(addRuleButton)
				&& window.rules_editor()->isAncestorOf(removeRuleButton),
			"session controls and available queue stops keep their order, and the rule editor lives on Reviews and queue");
		expect(
			window.focus_order().count(addRuleButton) == 0
				&& window.focus_order().count(removeRuleButton) == 0
				&& addRuleButton->isEnabled() == window.add_queue_rule_action()->isEnabled(),
			"the observer-rule presenters are another window's stops and follow their actions");

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

		// [[BPR-0671]]. The technical details are the overview's; focus
		// returns to that window and the overview is shown before the
		// disclosure is walked.
		window.activateWindow();
		window.choose_modality(qt::review_modality::overview);
		QApplication::processEvents();
		technicalButton->setFocus(Qt::TabFocusReason);
		technicalButton->click();
		QApplication::processEvents();
		expect(
			technicalDetails->isVisibleTo(window.overview_page())
				&& technicalButton->arrowType() == Qt::DownArrow
				&& technicalButton->accessibleName().contains(
					QStringLiteral("Hide"))
				&& QApplication::focusWidget() == technicalButton,
			"technical disclosure opens in place without moving focus");
		technicalButton->click();
		QApplication::processEvents();
		expect(
			!technicalDetails->isVisibleTo(window.overview_page())
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
				&& cliCard->presentation().Current
				&& cliCard->state_label()->text() == QStringLiteral("Could not connect")
				&& cliCard->last_problem_label()->text().contains(
					QStringLiteral("Authentication failed")),
			"identity connection failure presents a focused empty error state, and its host's card says so");
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
		for (const auto* name : { "githubEmptyStateTitle", "githubEmptyStateBody" }) {
			auto* const label      = required_label(window, name);
			auto* const accessible = QAccessible::queryAccessibleInterface(label);
			expect(label->textFormat() == Qt::PlainText
					   && label->textInteractionFlags().testFlag(Qt::TextSelectableByMouse)
					   && label->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard)
					   && accessible != nullptr && accessible->text(QAccessible::Name) == label->text(),
				   "the empty page exposes current literal text and supports mouse/keyboard selection");
		}
		// [[BPR-0771]]. The status banner that repeated the failure is gone. A
		// session failure is its host's card's Last problem, with Copy details.
		auto* const card              = window.server_card_for(core::host_profile_id{ 11U });
		auto* const problem           = card == nullptr ? nullptr : card->last_problem_label();
		auto* const problemAccessible = problem == nullptr ? nullptr : QAccessible::queryAccessibleInterface(problem);
		expect(problem != nullptr && card->presentation().Current
				   && problem->text() == diagnostic
				   && problem->textFormat() == Qt::PlainText
				   && problem->textInteractionFlags().testFlag(Qt::TextSelectableByMouse)
				   && problem->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard)
				   && problemAccessible != nullptr && !problemAccessible->text(QAccessible::Name).isEmpty(),
			   "the card's last problem is the literal diagnostic, named, and selectable by mouse and keyboard");
		auto* const clipboard = QApplication::clipboard();
		expect(clipboard != nullptr, "the Qt platform provides a clipboard");
		// [[BPR-0775]]. The stack's empty page is not Home's, so the card is
		// where a session failure is said and copied.
		expect(!required_child<QStackedWidget>(window, "githubViewState")->isVisible(),
			   "the stack's empty page does not carry the session failure on Home");
		card->copy_details_button()->click();
		expect(clipboard->text() == diagnostic,
			   "the card's Copy details copies its last problem without other fields");
		capture_layout_state(window, QStringLiteral("copyable-connection-error"));
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{ .Status = core::github_status::no_profile },
			},
		});
		card->copy_details_button()->click();
		expect(!clipboard->text().contains(diagnostic)
				   && problem->text().isEmpty() && problem->isHidden()
				   && card->copy_details_button()->isHidden(),
			   "clearing the diagnostic also removes stale text from the card and from what it copies");
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
			// [[BPR-0771]], [[BPR-0775]]. The diagnostic the banner carried is the
			// host card's Last problem; the stack's empty page is not Home's.
			auto* const card = window.server_card_for(core::host_profile_id{ 11U });
			expect(card != nullptr && card->last_problem_label()->text() == message
					   && !card->last_problem_label()->isHidden() && !card->copy_details_button()->isHidden()
					   && !required_child<QStackedWidget>(window, "githubViewState")->isVisible(),
				   "bootstrap phase is rendered with the safe existing diagnostic in the host's card, not on a stack page");
			card->copy_details_button()->click();
			expect(QApplication::clipboard()->text() == message,
				   "the card's Copy details copies the exact bootstrap phase");
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
			card->copy_details_button()->click();
			expect(!QApplication::clipboard()->text().contains(QString::fromLatin1(prefix))
					   && card->copy_details_button()->isHidden(),
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

			const auto* const card = window.server_card_for(core::host_profile_id{ 11U });
			// [[BPR-0775]]. The stack's empty page is not Home's; the host card's
			// Last problem says the refusal and its Copy details copies it.
			expect(message.startsWith(QStringLiteral("Capability discovery: ")) && message.endsWith(expected)
					   && card != nullptr && card->last_problem_label()->text() == message
					   && !card->last_problem_label()->isHidden()
					   && !required_child<QStackedWidget>(window, "githubViewState")->isVisible(),
				   "fixed capability refusal detail appears in the host card's Last problem, not on a stack page");
			card->copy_details_button()->click();
			expect(QApplication::clipboard()->text() == message,
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
		auto* const sessionStack = required_child<QStackedWidget>(sessionWindow, "githubViewState");
		auto* const sessionEmpty = required_child<QWidget>(sessionWindow, "githubEmptyState");
		// [[BPR-0771]]. A session's problem is its host's card's, not a banner's.
		auto* const sessionCard = sessionWindow.server_card_for(core::host_profile_id{ 11U });
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
				&& sessionCard != nullptr
				&& sessionCard->state_label()->text() == QStringLiteral("Signed out")
				&& sessionCard->last_problem_label()->isHidden()
				&& sessionCard->copy_details_button()->isHidden()
				&& sessionWindow.home_open_notice()->isHidden()
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
		// [[BPR-0771]]. A pull request's load refusal is said under the Home
		// bar, where the pull request was named, rather than on Accounts & hosts.
		QApplication::processEvents();
		publish_session_ready(firstLoadWindow);
		firstLoadWindow.publish(application::workbench_projection{
			.Commands = core::project_workbench_commands({ }) });
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
		auto* const firstLoadStack   = required_child<QStackedWidget>(firstLoadWindow, "githubViewState");
		auto* const firstLoadEmpty   = required_child<QWidget>(firstLoadWindow, "githubEmptyState");
		auto* const firstLoadNotice  = firstLoadWindow.home_open_notice();
		auto* const firstLoadDetails = firstLoadWindow.home_open_details_button();
		expect(
			presentation_state(firstLoadWindow) == QStringLiteral("cancelled")
				&& firstLoadWindow.showing_home()
				&& !firstLoadWindow.comparison_surface()->isVisible()
				&& firstLoadStack->currentWidget() == firstLoadEmpty
				&& required_label(firstLoadWindow, "githubEmptyStateTitle")
						->text()
					== QStringLiteral("Pull request loading canceled")
				&& required_label(firstLoadWindow, "githubEmptyStateBody")
					   ->text()
					   .contains(QStringLiteral("link in the bar is unchanged"))
				&& firstLoadWindow.github_status_text()
					== "Pull request loading was canceled."
				&& firstLoadWindow.github_diagnostic_text().empty()
				&& firstLoadNotice != nullptr && firstLoadNotice->isHidden()
				&& firstLoadDetails != nullptr && firstLoadDetails->isHidden()
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
				&& firstLoadWindow.showing_home()
				&& firstLoadNotice->isVisible()
				&& !firstLoadWindow.comparison_surface()->isVisible()
				&& firstLoadStack->currentWidget() == firstLoadEmpty
				&& required_label(firstLoadWindow, "githubEmptyStateTitle")
						->text()
					== QStringLiteral("Pull request unavailable")
				&& firstLoadWindow.github_status_text()
					== "The pull request could not be loaded."
				&& !firstLoadWindow.github_diagnostic_text().empty()
				&& required_label(firstLoadWindow, "githubEmptyStateBody")->text()
					== QString::fromStdString(firstLoadWindow.github_diagnostic_text())
				&& !firstLoadNotice->isHidden()
				&& firstLoadNotice->text()
					== QString::fromStdString(firstLoadWindow.github_diagnostic_text())
				&& !firstLoadDetails->isHidden()
				&& firstLoadStack->isHidden()
				&& firstLoadWindow.conversation_model()->rowCount() == 0,
			"first-load failure says the diagnostic once, under the Home bar with "
			"Copy details, and the empty page that carries it is not shown too");
		capture_layout_state(
			firstLoadWindow, QStringLiteral("first-load-error"));
		firstLoadWindow.publish(application::workbench_projection{
			.Commands = core::project_workbench_commands({ }) });
		expect(firstLoadWindow.showing_home() && firstLoadNotice->isVisible(),
			   "later drawer commands cannot hide an initial read failure");

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
		auto* const                 refreshStack     = required_child<QStackedWidget>(refreshWindow, "githubViewState");
		auto* const                 refreshEmpty     = required_child<QWidget>(refreshWindow, "githubEmptyState");
		auto* const                 refreshNotice    = refreshWindow.home_open_notice();
		const auto                  retainedTitle    = required_label(refreshWindow, "pullRequestTitle")->text();
		const auto                  retainedIdentity = required_label(refreshWindow, "pullRequestIdentity")->text();
		const auto                  retainedBody     = required_child<QPlainTextEdit>(
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
				&& !refreshWindow.showing_home()
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
				&& refreshNotice != nullptr && refreshNotice->isHidden()
				&& refreshWindow.home_open_details_button()->isHidden(),
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
		// [[BPR-0771]]. The host, its kind and transport, and the connection
		// state are the host card's header; the "Active host" and transport
		// lines under the picker are gone.
		auto* const enterpriseCard = window.server_card_for(core::host_profile_id{ 12U });
		expect(
			window.active_host_text() == "ghe.example.invalid"
				&& enterpriseCard != nullptr && enterpriseCard->presentation().Current
				&& enterpriseCard->host_label()->text() == QStringLiteral("ghe.example.invalid")
				&& enterpriseCard->kind_label()->text()
					== QStringLiteral("GitHub Enterprise Server, native HTTPS")
				&& enterpriseCard->state_label()->text() == QStringLiteral("Connecting")
				&& window.findChild<QWidget*>(QStringLiteral("activeGithubTransport")) == nullptr,
			"establishing identity shows the exact selected host, its kind and transport, and Connecting on its card");
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
			window.active_host_text() == "ghe.example.invalid"
				&& enterpriseCard->state_label()->text() == QStringLiteral("Connected")
				&& enterpriseCard->login_label()->text() == QStringLiteral("octo-reviewer")
				&& window.github_open_action()->isEnabled(),
			"accepted identity marks the configured host's card Connected with its account");

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
		// [[BPR-0771]]. The capability and rate-limit lines are the session
		// card's Details now, one value to a row.
		const auto detailTexts = [](const QWidget* const aDetails, const QString& aObjectName) {
			auto texts = QStringList{ };
			for (const auto* const label : aDetails->findChildren<QLabel*>(aObjectName)) {
				texts.push_back(label->text());
			}
			return texts;
		};
		const auto capabilityTexts = detailTexts(enterpriseCard->details(), QStringLiteral("serverCardCapability"));
		const auto rateTexts       = detailTexts(enterpriseCard->details(), QStringLiteral("serverCardRateLimitResource"));
		expect(
			capabilityTexts
					== QStringList{ QStringLiteral("Supported"), QStringLiteral("Supported"),
									QStringLiteral("Unsupported"), QStringLiteral("Not yet known") }
				&& rateTexts.size() == 3
				&& rateTexts[0].startsWith(QLocale{ }.toString(qulonglong{ 4'999U }))
				&& rateTexts[1].isEmpty()
				&& rateTexts[2].startsWith(QLocale{ }.toString(qulonglong{ 4'998U })),
			"capability tri-state and numeric rate observations remain distinct");

		window.show();
		QApplication::processEvents();
		window.choose_modality(qt::review_modality::discuss);
		QApplication::processEvents();
		const auto focusedThread = window.conversation_model()->index_for_id("review-thread:t1");
		expect(
			focusedThread.isValid()
				&& window.conversation_panel()->discuss()
				&& window.findChild<QWidget*>(QStringLiteral("workbenchDiscussPage"))
					   ->isAncestorOf(window.conversation_panel())
				&& window.conversation_panel()->cards()->thread_count() > 0U,
			"Discuss automatically reveals accepted PR-wide conversations without a focus-only action");

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
		expect(!window.show_review_threads_action(),
			   "legacy command projection cannot recreate the removed focus-only action");
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
		expect(!window.show_review_threads_action(),
			   "an enabled legacy command projection still cannot recreate the removed action");

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
		// [[BPR-0771]]. The switched-to host's card is the current one, and with
		// no rate limits reported it says so rather than repeating the old ones.
		auto* const cliCard    = window.server_card_for(core::host_profile_id{ 11U });
		auto* const unreported = cliCard == nullptr
			? nullptr
			: cliCard->details()->findChild<QLabel*>(QStringLiteral("serverCardRateLimitsUnreported"));
		expect(
			window.active_host_text() == "github.com"
				&& window.active_account_text() == "Connecting…"
				&& window.conversation_model()->rowCount() == 0
				&& required_label(window, "pullRequestTitle")->text()
					== QStringLiteral("Not available")
				&& cliCard->presentation().Current
				&& cliCard->state_label()->text() == QStringLiteral("Connecting")
				&& unreported != nullptr && !unreported->isHidden()
				&& std::ranges::all_of(
					detailTexts(cliCard->details(), QStringLiteral("serverCardRateLimitResource")),
					[](const QString& aText) { return aText.isEmpty(); })
				&& !enterpriseCard->presentation().Current
				&& !enterpriseCard->presentation().Session.has_value()
				&& enterpriseCard->login_label()->text() == QStringLiteral("No account"),
			"identity switch removes old account, PR, and rate-limit content");

		// [[BPR-0771]]. A pull request is named in the Home bar; the locator
		// that once held focus on Accounts & hosts is gone.
		window.show();
		window.activateWindow();
		QApplication::processEvents();
		expect(window.showing_home(), "with no pull request open the window shows Home and its bar");
		window.home_open_bar()->setFocus(Qt::OtherFocusReason);
		QApplication::processEvents();
		expect(
			QApplication::focusWidget() == window.home_open_bar(),
			"the Home bar receives focus before asynchronous projection");
		publish_session_ready(window);
		QApplication::processEvents();
		expect(
			QApplication::focusWidget() == window.home_open_bar(),
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
			{ reason::permission_denied,
			  "This app's GitHub connection cannot read this pull request. Your browser "
			  "sign-in may have different access." },
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
				"review-thread:long",
				application::github_conversation_kind::review_thread,
				longBody));
		// The reader intentionally displays only the selected root. Measure
		// adjacent rows within that root, not a different hidden conversation.
		pullRequest.Conversations.insert(pullRequest.Conversations.begin() + 1,
										 conversation("review-comment:short", application::github_conversation_kind::review_comment,
													  "Short reply", "review-thread:long"));
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

		// [[BPR-0771]]. The status banner is gone; the session's host card is the
		// accounts surface that inherits the application's appearance now.
		auto* const card       = window.server_card_for(core::host_profile_id{ 12U });
		auto* const viewState  = required_child<QStackedWidget>(window, "githubViewState");
		auto* const title      = required_label(window, "pullRequestTitle");
		auto* const view       = window.conversation_view();
		auto* const model      = window.conversation_model();
		const auto  longIndex  = model->index_for_id("review-thread:long");
		const auto  shortIndex = model->index_for_id("review-comment:short");
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
			card,
			viewState,
			view,
			card->details_button(),
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
		view->expand(longIndex);
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
			card->palette().color(QPalette::Window) == controlledWindow
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
				&& card->palette().color(QPalette::Window)
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
		QString Message;
		QString ObjectName;
		/// [[BPR-0771]]. Several cards share one object name, so the source is
		/// also kept by identity.
		const QObject*                      Object     = nullptr;
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
				.Object     = aEvent->object(),
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
		// [[BPR-0771]]. The banner that carried the announcement is gone. A
		// session's change is attributed to its host's card, and a pull
		// request's to Home, whose bar carries its refusal.
		auto* const card = window.server_card_for(core::host_profile_id{ 11U });
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
				card != nullptr && card->presentation().Current
					&& announcements.size() == 1U
					&& announcements.front().Object == card
					&& announcements.front().ObjectName
						== QStringLiteral("serverCard")
					&& announcements.front().Message.contains(
						QStringLiteral("connection failed"))
					&& announcements.front().Message.contains(
						QStringLiteral("Authentication failed"))
					&& announcements.front().Message
						== window.session_panel()->summary_text()
					&& announcements.front().Politeness
						== QAccessible::AnnouncementPoliteness::Polite,
				"one polite accessibility announcement from the host's card "
				"combines status and diagnostic context");

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
					&& announcements.back().Object == card
					&& announcements.back().Message.contains(
						QStringLiteral("This app's GitHub connection cannot read"))
					&& announcements.back().Message
						== window.session_panel()->summary_text(),
				"a changed diagnostic produces one newly combined announcement");

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
				announcements.size() == 3U
					&& announcements.back().Object == window.home_page()
					&& announcements.back().Message.contains(
						QStringLiteral("could not be loaded"))
					&& announcements.back().Message
						== window.session_panel()->summary_text(),
				"a pull request's refusal is announced from Home, whose bar says it");
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

		// [[BPR-0771]]. The host picker, its label and the locator are gone: a
		// host is chosen by its card's Connect, and a pull request is named in
		// the Home bar.
		auto* const otherCard     = window.server_card_for(core::host_profile_id{ 11U });
		auto* const connectButton = otherCard == nullptr ? nullptr : otherCard->connect_button();
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
			window.findChild<QLabel*>(QStringLiteral("githubProfileLabel")) == nullptr
				&& window.findChild<QWidget*>(QStringLiteral("githubProfileSelector")) == nullptr,
			"no host-profile picker or picker label remains to be a buddy");
		const auto labels   = window.findChildren<QLabel*>();
		const auto hasBuddy = [&labels](const QWidget* const aWidget) {
			return std::ranges::any_of(
				labels,
				[aWidget](const QLabel* const aLabel) {
					return aLabel->buddy() == aWidget;
				});
		};
		expect(
			hasBuddy(window.home_open_bar())
				&& window.findChild<QWidget*>(QStringLiteral("repositoryOwnerEditor")) == nullptr
				&& window.findChild<QWidget*>(QStringLiteral("repositoryNameEditor")) == nullptr
				&& window.findChild<QWidget*>(QStringLiteral("pullRequestNumberEditor")) == nullptr,
			"the Home bar, where a pull request is named, exposes its label relationship, and no locator editor remains");
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		expect(
			connectButton != nullptr && connectButton->isVisible()
				&& !connectButton->accessibleName().isEmpty()
				&& !connectButton->accessibleDescription().isEmpty(),
			"a card's Connect has visible named keyboard-accessible presentation");
		expect(
			window.conversations_drawer() != nullptr
				&& window.conversations_drawer()->isVisible()
				&& window.conversation_view()->isVisible()
				&& window.conversation_view()->width() > 0
				&& window.conversation_view()->height() > 0,
			"metadata and conversations survive minimum size");

		auto* const conversationAccessible = QAccessible::queryAccessibleInterface(window.conversation_view());
		expect(
			conversationAccessible != nullptr
				&& !conversationAccessible->text(QAccessible::Name).isEmpty()
				&& conversationAccessible->state().focusable,
			"conversation tree exposes an accessible focusable interface");

		connectButton->setFocus(Qt::TabFocusReason);
		QApplication::processEvents();
		expect(
			QApplication::focusWidget() == connectButton
				&& QAccessible::queryAccessibleInterface(connectButton)
					   ->state()
					   .focused,
			"a card's Connect reports focused accessible state");

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
	auto test_read_drawer_shows_threads_as_cards_with_scope_and_actions() -> void
	{
		// [[BPR-0648]]. The comments drawer's cards: this file's threads, the
		// whole-PR scope, and no inline action without an exact code anchor.
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		window.resize(1'280, 820);
		window.show();
		QApplication::processEvents();
		publish_session_ready(window);
		auto pullRequest = pull_request_projection();
		auto placed      = conversation("review-thread:t-placed",
										application::github_conversation_kind::review_thread);
		placed.AuthorLogin.clear();
		placed.DiffSide           = application::github_conversation_diff_side::right;
		placed.AnchorAvailability = application::github_conversation_anchor_availability::current;
		auto placedReply          = conversation("review-comment:c-placed",
												 application::github_conversation_kind::review_comment,
												 "A reply on the placed thread.",
												 "review-thread:t-placed");
		auto elsewhere            = conversation("review-thread:t-elsewhere",
												 application::github_conversation_kind::review_thread);
		elsewhere.AuthorLogin.clear();
		elsewhere.Path               = "src/other.cpp";
		elsewhere.Line               = 7U;
		elsewhere.DiffSide           = application::github_conversation_diff_side::right;
		elsewhere.AnchorAvailability = application::github_conversation_anchor_availability::current;
		pullRequest.Conversations    = { placed, placedReply, elsewhere };
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = enabled_commands(),
				.Status   = core::github_status_delta{
					.Status = core::github_status::pull_request_ready,
				},
			},
			.PullRequest = pullRequest,
		});
		QApplication::processEvents();

		auto* const panel = window.conversation_panel();
		auto* const cards = panel->cards();
		expect(cards != nullptr && cards->isVisible()
				   && panel->scope_this_file_button()->isChecked()
				   && window.conversations_drawer()->isAncestorOf(cards),
			   "the comments drawer shows the cards with This file chosen");
		expect(cards->cards().size() == 2U && cards->post_count() == 1U,
			   "with no file selected every thread of the pull request is a card");
		expect(!cards->cards().front().Placed,
			   "without a comparison no card is placed at a line");

		panel->scope_whole_pull_request_button()->click();
		QApplication::processEvents();
		expect(cards->scope() == qt::cards_scope::whole_pull_request
				   && cards->cards().size() == 2U
				   && cards->cards().front().Path == QStringLiteral("src/other.cpp"),
			   "Whole PR lists every thread in file order");
		panel->scope_this_file_button()->click();
		QApplication::processEvents();

		window.activateWindow();
		const auto placedCard = std::ranges::find_if(cards->cards(), [](const qt::thread_card& aCard) {
			return aCard.Id == "review-thread:t-placed";
		});
		expect(placedCard != cards->cards().end() && !placedCard->Placed && placedCard->Actions.empty(),
			   "the compact reader offers no inline action before an exact comparison anchors the thread");
		window.hide();
	}

	/// \brief Exercises one-pane thread placement through the composed GitHub projection.
	auto test_one_pane_places_github_threads_on_their_own_rows() -> void
	{
		const auto opaque = []<typename Identity>(const std::uint8_t aSeed) {
			auto bytes    = std::array<std::uint8_t, 32U>{ };
			bytes.front() = aSeed;
			bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
			return Identity{ bytes };
		};
		const auto blob = [](const std::uint8_t aSeed) {
			auto bytes = std::array<std::uint8_t, 32U>{ };
			for (std::size_t index = 0U; index < 20U; ++index) {
				bytes[index] = static_cast<std::uint8_t>(aSeed + index);
			}
			return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
		};
		const auto options = core::diff_options{ };
		const auto digest  = core::options_digest(options);
		expect(digest.has_value(), "one-pane GitHub fixture options normalize");
		const auto scope                                   = opaque.template operator()<core::pull_request_authority_scope_id>(11U);
		const auto incarnation                             = opaque.template operator()<core::epoch_catalog_incarnation_id>(12U);
		const auto                               authority = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority   = opaque.template operator()<core::host_authority_fingerprint>(13U),
			.Account         = core::github_account_id{ 40U },
			.Viewer          = core::github_viewer_actor_id{ 50U },
			.Repository      = core::repository_id{ 101U },
			.Cache           = opaque.template operator()<core::repository_cache_scope_id>(14U),
			.PullRequest     = core::pull_request_id{ 202U },
			.Number          = core::pull_request_number{ 17U },
			.Scope           = scope,
		};
		const auto key = core::file_comparison_key{
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = { .Authority = scope, .Incarnation = incarnation, .Kind = core::comparison_anchor_kind::epoch_base, .Epoch = core::epoch_ordinal{ 1 } },
			.RightAnchor = { .Authority = scope, .Incarnation = incarnation, .Kind = core::comparison_anchor_kind::epoch_head, .Epoch = core::epoch_ordinal{ 2 } },
			.LeftBlob    = blob(21U),
			.RightBlob   = blob(51U),
			.File        = core::file_identity_of("src/review.cpp"),
			.Options     = *digest,
			.Algorithm   = core::supported_diff_algorithm_revision,
		};
		auto leftText  = std::string{ };
		auto rightText = std::string{ };
		for (std::size_t line = 0U; line < 12U; ++line) {
			const auto common = "leading-" + std::to_string(line) + '\n';
			leftText += common;
			rightText += common;
		}
		leftText += "old-a\nold-b\nold-c\ncommon-tail\n";
		rightText += "new-a\nnew-b\ncommon-tail\n";
		const auto left   = std::vector<std::uint8_t>{ leftText.begin(), leftText.end() };
		const auto right  = std::vector<std::uint8_t>{ rightText.begin(), rightText.end() };
		const auto result = core::compute_file_diff({
			.Key        = key,
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ left },
			.RightBytes = std::span<const std::uint8_t>{ right },
		});
		expect(result.Reason == core::diff_reason_code::none && result.Result.has_value(),
			   "one-pane GitHub fixture computes a replacement");

		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		window.resize(1'280, 1'000);
		window.show();
		QApplication::processEvents();
		publish_session_ready(window);
		auto       pullRequest = pull_request_projection();
		const auto makeThread  = [](std::string                                      aId,
									const application::github_conversation_diff_side aSide) {
			auto thread               = conversation(std::move(aId),
													 application::github_conversation_kind::review_thread);
			thread.Line               = 13U;
			thread.DiffSide           = aSide;
			thread.AnchorAvailability = application::github_conversation_anchor_availability::current;
			return thread;
		};
		pullRequest.Conversations = {
			makeThread("review-thread:old-a", application::github_conversation_diff_side::left),
			makeThread("review-thread:old-b", application::github_conversation_diff_side::left),
			makeThread("review-thread:new", application::github_conversation_diff_side::right),
		};
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = enabled_commands(),
				.Status   = core::github_status_delta{ .Status = core::github_status::pull_request_ready },
			},
			.PullRequest = std::move(pullRequest),
		});
		auto* const surface = window.comparison_surface();
		const auto  context = core::diff_request_context{
			.Request            = core::request_id{ 1U },
			.ApplicationSession = core::application_session_id{ 1U },
			.Key                = key,
			.Generation         = core::request_generation{ 1U },
		};
		surface->set_loading(context);
		surface->publish(qt::test::prepare_publication({
			.Context = context,
			.Result  = std::make_shared<const core::computed_file_diff>(*result.Result),
		}));
		auto onePane    = application::workbench_projection{ };
		onePane.OnePane = core::workbench_one_pane_delta{ .State = core::workbench_one_pane::enabled };
		window.publish(onePane);
		window.set_conversation_endpoint_placement(true, true);
		QApplication::processEvents();

		const auto  oldRow = surface->line_rect(core::diff_side::left, 12U);
		const auto  newRow = surface->line_rect(core::diff_side::right, 12U);
		const auto& cards  = window.conversation_panel()->cards()->cards();
		expect(window.one_pane_action()->isChecked() && surface->one_pane_enabled()
				   && !oldRow.isEmpty() && !newRow.isEmpty() && oldRow.y() < newRow.y(),
			   "composed GitHub one-pane Review shows side-specific replacement rows");
		expect(cards.size() == 3U && std::ranges::all_of(cards, [](const qt::thread_card& aCard) {
				   return aCard.Placed && aCard.Rect.top() >= aCard.TargetY;
			   }) && cards[0].TargetY == cards[1].TargetY
				   && cards[0].Rect.bottom() < cards[1].Rect.top() && cards[1].TargetY < cards[2].TargetY,
			   "GitHub-projected old/new threads anchor and colliding cards push down in document order");
		capture_layout_state(window, QStringLiteral("one-pane-threads"));
		if (const auto path = qEnvironmentVariable("BPR_QT_RENDER_PATH"); !path.isEmpty()) {
			const QFileInfo file{ path };
			const auto      origin = surface->mapTo(&window, QPoint{ 0, 0 });
			const auto      crop   = window.grab(QRect{
				origin.x(),
				origin.y(),
				window.width() - origin.x(),
				std::min(window.height() - origin.y(), 480),
			});
			expect(!crop.isNull()
					   && crop.save(QStringLiteral("%1/%2-one-pane-threads-crop.png")
										.arg(file.path(), file.completeBaseName()),
									"PNG"),
				   "composed GitHub one-pane thread crop is retained for three-scale inspection");
		}
		// [[BPR-0570]]. A compatibility root uses the same selected post and
		// code gesture as a native root. Only the collapsed support detail
		// names its honest remote representation.
		auto semantic              = makeThread("review-thread:semantic", application::github_conversation_diff_side::right);
		semantic.Line              = 1U;
		semantic.RemoteSubject     = application::github_conversation_remote_subject::file;
		semantic.SemanticAnchor    = core::source_line_ref{ .Comparison = core::comparison_digest(key), .Side = core::diff_side::right, .Line = 0U };
		auto semanticPost          = semantic;
		semanticPost.Id            = "review-comment:semantic";
		semanticPost.ParentId      = semantic.Id;
		semanticPost.Kind          = application::github_conversation_kind::review_comment;
		semanticPost.AuthorLogin   = "reviewer";
		semanticPost.Body          = "Raw canonical code link and prose";
		semanticPost.ReviewerProse = "Please retain this unchanged source line.";
		auto semanticPr            = pull_request_projection();
		semanticPr.Conversations   = { semantic, semanticPost };
		window.publish(application::github_presentation_update{ .PullRequest = semanticPr, .PreserveWindowNavigation = true });
		window.set_conversation_endpoint_placement(false, false);
		auto* const panel = window.conversation_panel();
		panel->choose(panel->model()->index_for_id(semanticPost.Id));
		QApplication::processEvents();
		expect(surface->selection() && surface->selection()->PreferredLine == semantic.SemanticAnchor,
			   "choosing a file-subject conversation reveals its exact retained semantic source line");
		auto* const support    = required_child<QToolButton>(window, "conversationSupportDetails");
		auto* const diagnostic = required_child<QLabel>(window, "conversationSupportDescription");
		expect(support->isVisible() && !diagnostic->isVisible(), "the shipped workbench keeps representation collapsed");
		const auto original = QApplication::palette();
		for (const auto& theme : { QStringLiteral("light"), QStringLiteral("dark"), QStringLiteral("high-contrast") }) {
			auto       palette    = original;
			const auto dark       = theme != QStringLiteral("light");
			const auto background = dark ? (theme == QStringLiteral("dark") ? QColor{ 30, 34, 40 } : QColor{ 0, 0, 0 }) : QColor{ 250, 250, 250 };
			const auto foreground = dark ? QColor{ 255, 255, 255 } : QColor{ 0, 0, 0 };
			for (const auto role : { QPalette::Window, QPalette::Base, QPalette::Button }) {
				palette.setColor(role, background);
			}
			for (const auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText }) {
				palette.setColor(role, foreground);
			}
			QApplication::setPalette(palette);
			window.activateWindow();
			support->setFocus(Qt::TabFocusReason);
			auto press   = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
			auto release = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier };
			QApplication::sendEvent(support, &press);
			QApplication::sendEvent(support, &release);
			QApplication::processEvents();
			expect(diagnostic->isVisible() && support->hasFocus()
					   && diagnostic->palette().color(QPalette::WindowText) == foreground,
				   "keyboard support disclosure retains focus and follows light, dark and high-contrast palettes");
			auto tab = QKeyEvent{ QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier };
			QApplication::sendEvent(support, &tab);
			QApplication::processEvents();
			expect(diagnostic->hasFocus(), "Tab reaches selectable support text");
			window.comments_tab_scroll()->ensureWidgetVisible(diagnostic);
			QApplication::processEvents();
			window.comments_tab_scroll()->verticalScrollBar()->setValue(window.comments_tab_scroll()->verticalScrollBar()->maximum());
			QApplication::processEvents();
			const QRect extent{ diagnostic->mapTo(window.comments_tab_scroll()->viewport(), QPoint{ }), diagnostic->size() };
			expect(window.comments_tab_scroll()->viewport()->rect().contains(extent), "complete support text is reachable inside the shipped drawer at each scale");
			capture_layout_state(window, QStringLiteral("semantic-") + theme);
			support->setChecked(false);
		}
		QApplication::setPalette(original);
		const auto resets = panel->model()->statistics().ModelResets;
		window.publish(application::github_presentation_update{ .PullRequest = semanticPr, .PreserveWindowNavigation = true });
		expect(panel->model()->statistics().ModelResets == resets
				   && panel->model()->id_for_index(panel->view()->currentIndex()) == semanticPost.Id,
			   "unchanged semantic refresh retains exact selected post without a model reset");

		window.hide();
	}

	// [[BPR-0650]]. Home's action list: rows grouped under their headings
	// with the columns the design names, a click opening the pull request on
	// Overview, and the open bar sending the locator's intent.
	// [[BPR-0670]]. The reviewer with no account is offered the signed-out
	// read where they are standing, named for what it does, and it enters the
	// mode for the selected host and leaves them at the bar that names a pull
	// request. Signed in, it is not offered at all.
	// [[BPR-0668]]. A comparison that could not be prepared used to leave the
	// reviewer with two empty panes, an empty file tree and a status line
	// claiming no pull request was open while the strip named one. The files
	// come from the pull request's own file set, and the line says what is
	// true and what to do.
	auto test_a_pull_request_without_a_comparison_still_lists_its_files() -> void
	{
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		QApplication::processEvents();
		// [[BPR-0775]]. With nothing open the window is Home, and Home's line is
		// the session's rather than a request for a pull request.
		expect(window.showing_home()
				   && window.status_text() == QStringLiteral("Not signed in. Public pull requests open read-only.")
				   && !window.status_text().contains(QStringLiteral("No pull request is open")),
			   "with nothing open, Home's line is the session line");
		publish_session_ready(window);
		publish_pull_request(window);
		QApplication::processEvents();

		auto overview = application::pull_request_overview_projection{ };
		overview.Files.push_back(application::overview_file_row{
			.Path      = "src/host/ConptyConnection.cpp",
			.Name      = "ConptyConnection.cpp",
			.Operation = application::overview_file_operation::modified,
			.Additions = 12U,
			.Deletions = 3U,
		});
		overview.Files.push_back(application::overview_file_row{
			.Path      = "src/terminal/adapter/adaptDispatch.cpp",
			.Name      = "adaptDispatch.cpp",
			.Operation = application::overview_file_operation::modified,
			.Additions = 4U,
			.Deletions = 0U,
		});
		window.publish(overview);
		QApplication::processEvents();
		auto* const tree = window.files_tree();
		expect(tree != nullptr && tree->tree_model()->file_count() == 2U
				   && tree->tree_model()
						  ->index_for_file(core::file_identity_of("src/host/ConptyConnection.cpp"))
						  .isValid(),
			   "the files the pull request names are listed without a comparison");
		expect(!window.status_text().contains(QStringLiteral("No pull request is open")),
			   "the line never says nothing is open while a pull request is");
		expect(window.status_text().contains(QStringLiteral("This pull request is open"))
				   && window.status_text().contains(QStringLiteral("Refresh")),
			   "it says what is true and names the way to try again");
		// [[BPR-0677]]. Work in progress is not failure. The owner's comparison
		// spent ninety-five seconds being prepared while the line told them to
		// refresh, which would have thrown that work away.
		window.publish(application::epoch_presentation_update{
			.Transition = application::epoch_transition_projection{
				.Status = core::epoch_status_delta{ .Status = core::epoch_status::preparing },
			},
		});
		QApplication::processEvents();
		expect(window.status_text().contains(QStringLiteral("is being prepared"))
				   && !window.status_text().contains(QStringLiteral("Refresh to try again")),
			   "while the comparison is being prepared the line says so and asks for nothing");
		window.publish(application::epoch_presentation_update{
			.Transition = application::epoch_transition_projection{
				.Status = core::epoch_status_delta{ .Status = core::epoch_status::ready },
			},
		});
		QApplication::processEvents();
		expect(window.status_text().contains(QStringLiteral("Refresh to try again")),
			   "and once the history has settled with no comparison, Refresh is the advice");

		// [[BPR-0675]]. The owner met three full drawers and two strip toggles
		// that did nothing, because the core refused every drawer gesture
		// without a comparison. The room a drawer takes is theirs to give back
		// whenever they like, and the control is on the drawer.
		auto* const files = window.files_drawer();
		// [[BPR-0721]]. The drawer's own control is there only when the
		// preference shows the drawer controls; the owner has it off by
		// default, and the strip's toggle is the control then.
		expect(files->collapse_button() != nullptr && !files->collapse_button()->isVisible()
				   && window.files_drawer_toggle_action()->isEnabled(),
			   "by default the drawer has no Hide button and the strip's toggle is the control");
		auto controls      = application::workbench_projection{ };
		controls.Behaviour = application::review_behaviour_settings{
			.DrawerControls = application::review_drawer_controls::shown,
		};
		window.publish(controls);
		QApplication::processEvents();
		expect(files->collapse_button()->isVisible()
				   && files->collapse_button()->accessibleName().contains(QStringLiteral("Files")),
			   "the files drawer offers its own collapse, named");
		expect(window.files_drawer_toggle_action()->isEnabled(),
			   "and the strip's toggle is offered with no comparison open");
		// The core owns the state, so what the window owes is the gesture. It
		// leaves once from the drawer's control and once from the strip, for
		// the drawer the reviewer pressed.
		auto drawerGestures = std::vector<core::workbench_drawer>{ };
		window.set_dispatch([&drawerGestures](core::workbench_interaction_intent aIntent) {
			if (const auto* const toggle = std::get_if<core::toggle_workbench_drawer_intent>(&aIntent);
				toggle != nullptr) {
				drawerGestures.push_back(toggle->Drawer);
			}
		});
		files->collapse_button()->click();
		window.files_drawer_toggle_action()->trigger();
		QApplication::processEvents();
		expect(drawerGestures
				   == std::vector{ core::workbench_drawer::files, core::workbench_drawer::files },
			   "the drawer's own control and the strip send the same gesture for that drawer");
		window.hide();
	}

	/// \brief [[BPR-0732]]. Signed out, Home is the card, one bar and nothing
	/// that needs an account.
	auto test_home_signed_out_is_one_paste_field() -> void
	{
		std::vector<application::github_intent> intents;
		qt::workbench_window                    window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[&intents](application::github_intent aIntent) {
				intents.push_back(aIntent);
			},
		};
		window.resize(1'280, 820);
		window.show();
		window.activateWindow();
		QApplication::processEvents();
		auto* const landing = window.home_landing();
		auto* const surface = window.findChild<QWidget*>(QStringLiteral("reviewQueueLanding"));
		auto* const stack   = window.findChild<QWidget*>(QStringLiteral("githubViewState"));
		// [[BPR-0775]]. RFC-BPR-0032 A5, first launch and signed out: the strip
		// with Home, account and settings; the bar relabelled for public
		// reading; one card with Sign in filled and Add enterprise server
		// bordered; the status bar line; and nothing else. The CLI way is
		// capability-driven, so this presentation-only window does not invent it.
		const auto top = [&window](const QWidget* const aWidget) {
			return aWidget->mapTo(window.home_page(), QPoint{ }).y();
		};
		expect(landing->state() == qt::landing_state::sign_in && landing->isVisible()
				   && landing->title_label()->text() == QStringLiteral("Sign in to GitHub")
				   && landing->body_label()->text().contains(QStringLiteral("device code"))
				   && landing->primary_button()->isVisible() && landing->primary_button()->text() == QStringLiteral("Sign in")
				   && landing->primary_button()->accessibleName() == QStringLiteral("Sign in to GitHub")
				   && landing->primary_button()->isDefault()
				   && !landing->secondary_button()->isVisible()
				   && landing->tertiary_button()->isVisible()
				   && landing->tertiary_button()->text() == QStringLiteral("Add enterprise server")
				   && !landing->tertiary_button()->isFlat() && !landing->tertiary_button()->isDefault()
				   && !landing->shortcut_label()->isVisible()
				   && window.findChild<QPushButton*>(QStringLiteral("workbenchHomeReadSignedOutButton")) == nullptr,
			   "signed out, Home's card is Sign in to GitHub with Sign in filled and Add enterprise server bordered");
		expect(window.home_open_frame()->isVisible() && window.home_open_label()->text() == QStringLiteral("Open")
				   && window.home_open_bar()->placeholderText() == QStringLiteral("Paste a public pull request link to read it without an account")
				   && window.findChild<QWidget*>(QStringLiteral("homeOpenButton")) == nullptr
				   && window.findChild<QWidget*>(QStringLiteral("workbenchHomeOpenLabel")) == nullptr
				   && top(window.home_open_frame()) < top(landing),
			   "the bar comes first, labelled Open and relabelled for public reading, with no heading or button of its own, and the card follows it");
		expect(window.status_text() == QStringLiteral("Not signed in. Public pull requests open read-only."),
			   "the status bar says the signed-out session line");
		auto strayStripControls = QStringList{ };
		for (auto* const action : window.primary_toolbar()->actions()) {
			auto* const widget = window.primary_toolbar()->widgetForAction(action);
			const auto  name   = action->objectName();
			const auto  kept   = action == window.home_action() || action == window.account_action()
				|| action == window.settings_action() || name == QStringLiteral("workbenchStripInsetAction")
				|| name == QStringLiteral("workbenchNavigationSpacerAction")
#if defined(_WIN32)
				// [[BPR-0653]]. On Windows the strip is the title row in every
				// state: its menu control and caption buttons are the window's,
				// not a pull request's.
				|| name == QStringLiteral("workbenchStripMenuAction")
				|| name == QStringLiteral("workbenchCaptionButtonsAction")
#endif
				;
			if (widget != nullptr && widget->isVisible() && !kept) {
				strayStripControls.append(name.isEmpty() ? action->text() : name);
			}
		}
		expect(strayStripControls.isEmpty(),
			   "the strip shows only Home, account and settings while signed out");
		expect(!window.review_bar()->isVisible(), "[[BPR-1129]] Home has no review bar");
		expect(surface != nullptr && !surface->isVisible()
				   && !window.home_list()->queue_shown() && !window.queue_role_filter()->isVisible()
				   && !window.queue_observer_filter()->isVisible() && !window.queue_bucket_filter()->isVisible()
				   && !window.queue_repository_filter()->isVisible() && !window.queue_quiet_filter()->isVisible()
				   && !window.queue_sort_selector()->isVisible(),
			   "no queue panel, list or filter combo is shown without an account");
		expect(window.home_open_bar()->isVisible() && QApplication::focusWidget() == window.home_open_bar(),
			   "the bar is the way in, so it has focus");
		expect(stack != nullptr && !stack->isVisible() && window.home_open_notice()->isHidden()
				   && window.findChild<QPushButton*>(QStringLiteral("homeSetupContinue"))->isHidden()
				   && window.findChild<QPushButton*>(QStringLiteral("homeSetupCancel"))->isHidden()
				   && !window.home_recent_list()->isVisible(),
			   "nothing else is on Home: no stack page, no notice, no setup row and no recently viewed list");
		auto cliDiscoveries = std::size_t{ 0U };
		window.set_github_cli_discovery_dispatch([&cliDiscoveries] { ++cliDiscoveries; });
		QApplication::processEvents();
		expect(landing->secondary_button()->isVisible()
				   && landing->secondary_button()->text() == QStringLiteral("Use GitHub CLI"),
			   "the explicit GitHub CLI capability adds its own first-launch path");
		landing->secondary_button()->click();
		QApplication::processEvents();
		expect(window.sign_in_dialog() != nullptr && window.sign_in_dialog()->isVisible()
				   && !window.settings()->isVisible()
				   && cliDiscoveries == 1U,
			   "Use GitHub CLI opens the shared sign-in dialog and dispatches discovery once");
		window.sign_in_dialog()->reject();
		QApplication::processEvents();
		landing->tertiary_button()->click();
		QApplication::processEvents();
		expect(window.sign_in_dialog()->isVisible() && !window.settings()->isVisible(),
			   "Add enterprise server on Home opens the shared sign-in dialog");
		window.sign_in_dialog()->reject();
		QApplication::processEvents();
		// Review of BPR-0775, finding 2: once the queue that keeps servers is
		// ready, the card's second way in opens the new card in its edit state.
		window.set_saved_server_dispatch([](application::saved_server_intent) { });
		window.publish(application::queue_landing_projection{ .Status = application::queue_landing_status::ready });
		QApplication::processEvents();
		landing->tertiary_button()->click();
		QApplication::processEvents();
		expect(window.sign_in_dialog()->isVisible() && !window.settings()->isVisible()
				   && window.enterprise_server_draft() != nullptr && window.enterprise_server_draft()->isVisible()
				   && QApplication::focusWidget() == window.enterprise_server_hostname(),
			   "with the queue ready, Add enterprise server opens the new card in its edit state with the hostname focused");
		window.enterprise_server_cancel()->click();
		window.sign_in_dialog()->reject();
		QApplication::processEvents();
		// A public-preview session is a session with no account: the same
		// Home, with the queue still absent.
		publish_session_ready(window);
		auto preview = application::public_preview_presentation{ };
		preview.Mode = application::public_preview_mode::signed_out;
		window.publish(preview);
		QApplication::processEvents();
		expect(landing->state() == qt::landing_state::sign_in && !surface->isVisible()
				   && !stack->isVisible(),
			   "a public-preview session keeps the signed-out Home: no picker, no queue, no empty page");
		expect(intents.empty(), "nothing was dispatched on the way");
		// Once a session with an account exists the queue appears.
		preview.Mode = application::public_preview_mode::signed_in;
		window.publish(preview);
		publish_session_ready(window);
		QApplication::processEvents();
		// [[BPR-0783]]. With an account the list appears and the strip gains
		// Home's own controls; the six typed combos are Settings' now, so what
		// says "the queue is here" is the list and those controls.
		expect(landing->state() == qt::landing_state::choose_pull_request && !landing->isVisible()
				   && surface->isVisible()
				   && window.home_list()->queue_shown()
				   && window.home_group_button()->isVisible()
				   && window.home_state_headers_action()->isVisible()
				   && window.home_filter_button()->isVisible()
				   && window.home_open_bar()->placeholderText() == QStringLiteral("Paste a link, or write owner/name#123"),
			   "a session with an account shows the queue, the bar names any pull request, and there is no card");
		window.hide();
	}

	/// \brief [[BPR-1137]]. Signed out, Home's first-launch card names the
	/// machine the way the rest of the product does ([[BPR-0773]]): "this Mac"
	/// on macOS, "this computer" on Windows and Linux. The sentence is
	/// otherwise [[BPR-0775]]'s, word for word. Each platform's build checks
	/// its own sentence, so the Windows and Linux wording is proved on those
	/// platforms.
	auto test_home_sign_in_sentence_names_this_machine() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		window.resize(1'280, 820);
		window.show();
		QApplication::processEvents();
		auto* const landing = window.home_landing();
#if defined(__APPLE__)
		const auto expected = QStringLiteral(
			"A device code in the browser signs this Mac in once; the session is "
			"remembered here. Paste a pull request link to start a review.");
#else
		const auto expected = QStringLiteral(
			"A device code in the browser signs this computer in once; the session is "
			"remembered here. Paste a pull request link to start a review.");
#endif
		const auto said = landing->body_label()->text();
		if (said != expected) {
			std::cerr << "Home's first-launch card says: " << said.toStdString() << '\n';
		}
		expect(landing->state() == qt::landing_state::sign_in && landing->body_label()->isVisible()
				   && said == expected,
			   "signed out, Home's first-launch card names this platform's machine in the accepted sentence");
#if !defined(__APPLE__)
		expect(!said.contains(QStringLiteral("Mac")), "off macOS the card does not say Mac");
#endif
		window.hide();
	}

	/// \brief [[BPR-0775]]. Signed in, Home's first row is the bar: labelled,
	/// focused on arrival and reachable by Command-L from anywhere. A first
	/// load enters the explicit loading Overview ([[BPR-0795]]).
	auto test_home_leads_with_the_bar_signed_in() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		window.resize(1'280, 820);
		window.show();
		window.activateWindow();
		QApplication::processEvents();
		publish_session_ready(window);
		QApplication::processEvents();
		auto* const landing = window.home_landing();
		auto* const bar     = window.home_open_bar();
		auto* const frame   = window.home_open_frame();
		const auto  top     = [&window](const QWidget* const aWidget) {
			return aWidget->mapTo(window.home_page(), QPoint{ }).y();
		};
		auto aboveTheBar = QStringList{ };
		for (auto* const child : window.home_page()->findChildren<QWidget*>(QString{ }, Qt::FindDirectChildrenOnly)) {
			if (child != frame && child->isVisible() && top(child) <= top(frame)) {
				aboveTheBar.append(child->objectName());
			}
		}
		expect(window.showing_home() && !landing->isVisible() && frame->isVisible() && aboveTheBar.isEmpty()
				   && frame->isAncestorOf(bar) && window.home_open_label()->text() == QStringLiteral("Open")
				   && window.home_open_label()->buddy() == bar
				   && bar->placeholderText() == QStringLiteral("Paste a link, or write owner/name#123")
				   && QApplication::focusWidget() == bar,
			   "signed in, the bar is Home's first row, labelled Open with its placeholder, and has focus on arrival");
		expect(window.status_text() == QStringLiteral("Signed in as octo-reviewer on ghe.example.invalid.")
				   && !window.status_text().contains(QStringLiteral("No pull request is open")),
			   "the status bar carries the session line and never says no pull request is open; it says: "
				   + window.status_text().toStdString());

		auto* const focusBar = window.home_focus_bar_action();
		auto* const shortcut = window.findChild<QLabel*>(QStringLiteral("homeOpenShortcut"));
		expect(focusBar != nullptr && focusBar->shortcut() == QKeySequence{ Qt::CTRL | Qt::Key_L }
				   && focusBar->shortcutContext() == Qt::WindowShortcut && window.actions().contains(focusBar)
				   && shortcut != nullptr && frame->isAncestorOf(shortcut)
				   && shortcut->text() == focusBar->shortcut().toString(QKeySequence::NativeText),
			   "Command-L is the bar's window shortcut, and the bar shows it");
		bar->setText(QStringLiteral("octo/app#7"));
		window.queue_role_filter()->setFocus(Qt::OtherFocusReason);
		QApplication::processEvents();
		focusBar->trigger();
		QApplication::processEvents();
		expect(QApplication::focusWidget() == bar && bar->selectedText() == QStringLiteral("octo/app#7"),
			   "Command-L focuses the bar with its text selected");
		bar->clear();

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status      = core::github_status_delta{ .Status = core::github_status::loading_pull_request },
				.PullRequest = core::pull_request_summary_delta{ .Summary = std::nullopt },
			},
		});
		QApplication::processEvents();
		expect(!window.showing_home() && window.overview_page()->isVisible()
				   && !window.overview_page()->presented()
				   && required_child<QLabel>(window, "overviewTitle")->text().contains(QStringLiteral("Loading"))
				   && window.home_open_details_button()->isHidden()
				   && required_child<QStackedWidget>(window, "githubViewState")->isHidden()
				   && window.epoch_selectors()->comparing_button()->text().contains(QStringLiteral("Loading")),
			   "a first load opens the provisional Overview immediately, with explicit loading and no accepted content");
		publish_session_ready(window);
		QApplication::processEvents();
		expect(window.home_open_notice()->isHidden(),
			   "with no load in progress nothing is said under the bar");

		publish_pull_request(window);
		QApplication::processEvents();
		expect(!window.showing_home(), "a pull request brings the review");
		focusBar->trigger();
		QApplication::processEvents();
		expect(window.showing_home() && QApplication::focusWidget() == bar
				   && landing->isVisible() && landing->state() == qt::landing_state::back_to_review,
			   "Command-L from the review shows Home over it, with the return row and the bar focused");
		landing->primary_button()->click();
		QApplication::processEvents();
		expect(!window.showing_home(), "the return row goes back to the review");
		window.hide();
	}

	// [[BPR-0733]]. One row of the recently viewed list, as the composition
	// hands it over: already filtered for this window's session.
	[[nodiscard]] auto recent_entry(
		std::string         aOwner,
		std::string         aName,
		const std::uint32_t aNumber,
		std::string         aTitle,
		const std::int64_t  aViewedAt,
		const bool          aResumable) -> application::recent_review_entry
	{
		return application::recent_review_entry{
			.Destination = core::window_review_destination{
				.Mode       = core::github_access_mode::authenticated,
				.Hostname   = "github.com",
				.Repository = core::repository_locator{
					.Owner = std::move(aOwner),
					.Name  = std::move(aName),
				},
				.Number = core::pull_request_number{ aNumber },
			},
			.Title     = std::move(aTitle),
			.ViewedAt  = aViewedAt,
			.Resumable = aResumable,
		};
	}

	/// \brief [[BPR-0733]]. The owner met a Home that opened the last pull
	/// request by itself, slowly and without being asked. Home now lists what
	/// was read recently and waits: a row is opened by a gesture, its saved
	/// reading is resumed by a named action, and with nothing to list there is
	/// no section at all.
	auto test_home_lists_recently_viewed_pull_requests() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		window.resize(1'280, 820);
		window.show();
		window.activateWindow();
		QApplication::processEvents();

		std::vector<application::recent_review_intent> gestures;
		window.set_recent_review_dispatch(
			[&gestures](application::recent_review_intent aIntent) {
				gestures.push_back(aIntent);
			});
		// [[BPR-0776]]. Recently viewed is the last panel of Home's list, and
		// Resume is a row action: on hover, on focus and in the row's menu. The
		// button beside a heading is gone.
		auto* const panel        = window.home_recent_panel();
		auto* const list         = window.home_recent_list();
		auto* const resume       = window.home_recent_resume_action();
		auto* const resumeButton = window.home_recent_row_action();
		expect(panel != nullptr && list != nullptr && resume != nullptr
				   && resumeButton != nullptr
				   && window.findChild<QToolButton*>(QStringLiteral("homeRecentResumeButton")) == nullptr
				   && window.findChild<QLabel*>(QStringLiteral("workbenchHomeRecentHeading")) == nullptr
				   && panel->objectName() == QStringLiteral("homeRecentPanel")
				   && panel->title() == QStringLiteral("Recently viewed") && panel->isAncestorOf(list)
				   && list->objectName() == QStringLiteral("homeRecentList")
				   && resume->objectName() == QStringLiteral("homeRecentResumeAction")
				   && list->actions().contains(resume) && list->contextMenuPolicy() == Qt::ActionsContextMenu
				   && list->accessibleName()
					   == QStringLiteral("Recently viewed pull requests"),
			   "Home carries a named Recently viewed panel and list, with Resume in the row's menu and no heading button");
		expect(!panel->isVisible() && !list->isVisible() && !resumeButton->isVisible(),
			   "with nothing published there is no panel");

		auto projection = application::recent_reviews_projection{ };
		projection.Entries.push_back(recent_entry(
			"microsoft", "terminal", 42U, "Fix the conpty resize race",
			1'772'000'000, false));
		projection.Entries.push_back(recent_entry(
			"microsoft", "terminal", 77U, "Teach the pane splitter about tabs",
			1'771'000'000, true));
		projection.Entries.push_back(recent_entry("qt", "qtbase", 9'001U, { }, 0, false));
		window.publish(projection);
		QApplication::processEvents();
		expect(list->count() == 3
				   && list->item(0)->text()
					   == QStringLiteral("microsoft/terminal#42 — Fix the conpty resize race")
				   && list->item(1)->text()
					   == QStringLiteral(
						   "microsoft/terminal#77 — Teach the pane splitter about tabs · saved reading")
				   && list->item(2)->text() == QStringLiteral("qt/qtbase#9001"),
			   "the rows are the entries in the order they were given, named and without colour");
		expect(list->item(0)->toolTip().startsWith(QStringLiteral("github.com"))
				   && list->item(0)->toolTip().contains(QStringLiteral("viewed"))
				   && list->item(2)->toolTip() == QStringLiteral("github.com"),
			   "each row says its host, and the instant only where one was recorded");
		expect(panel->isVisible() && list->isVisible() && panel->count() == 3
				   && panel->summary() == QStringLiteral("3 viewed; 1 saved reading"),
			   "with entries the panel is shown, with no account signed in, and counts them");
		expect(QApplication::focusWidget() == window.home_open_bar() && !resumeButton->isVisible(),
			   "publishing the list took no focus from where the reviewer was, and no row offers Resume yet");
		const auto listed = window.focus_order();
		expect(listed.contains(list) && listed.contains(panel->header()) && !listed.contains(resumeButton),
			   "the panel's header and its list are keyboard stops; the row action is not a stop of its own");

		expect(list->currentRow() == 0 && !resume->isEnabled(),
			   "the newest row is current, and it kept no reading to resume");
		list->setCurrentRow(1);
		QApplication::processEvents();
		expect(resume->isEnabled(), "the row with a saved reading offers Resume reading");
		resume->trigger();
		QApplication::processEvents();
		expect(gestures.size() == 1U && gestures.front().Index == 1U
				   && gestures.front().Resume,
			   "Resume reading names that row and asks for its reading");
		// The same gesture from the keyboard, on the row itself.
		list->setFocus(Qt::OtherFocusReason);
		auto shiftReturn = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::ShiftModifier };
		QApplication::sendEvent(list, &shiftReturn);
		QApplication::processEvents();
		expect(gestures.size() == 2U && gestures.back().Index == 1U && gestures.back().Resume,
			   "Shift and Return resume the current row rather than opening it");
		// With the list focused, Resume is the current row's button.
		expect(list->hasFocus() && resumeButton->isVisible()
				   && list->visualItemRect(list->item(1)).contains(resumeButton->geometry()),
			   "with the list focused, Resume sits on the current row that kept a reading");
		{
			const auto at      = QPointF{ resumeButton->rect().center() };
			const auto global  = resumeButton->mapToGlobal(at);
			auto       press   = QMouseEvent{ QEvent::MouseButtonPress, at, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			auto       release = QMouseEvent{ QEvent::MouseButtonRelease, at, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(resumeButton, &press);
			QApplication::sendEvent(resumeButton, &release);
			QApplication::processEvents();
		}
		expect(gestures.size() == 3U && gestures.back().Index == 1U && gestures.back().Resume,
			   "the row's Resume button resumes that row");
		list->setCurrentRow(2);
		QApplication::processEvents();
		expect(!resume->isEnabled() && !resumeButton->isVisible(),
			   "a row that kept no reading offers no resume");
		auto activate = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(list, &activate);
		QApplication::processEvents();
		expect(gestures.size() == 4U && gestures.back().Index == 2U
				   && !gestures.back().Resume,
			   "Return opens the current row, on every platform");
		// The pointer's own activation arrives the way any list's does.
		const auto row     = list->visualItemRect(list->item(0));
		const auto local   = QPointF{ row.center() };
		const auto global  = list->viewport()->mapToGlobal(local);
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, local, global,
										  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       doubled = QMouseEvent{ QEvent::MouseButtonDblClick, local, global,
										  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(list->viewport(), &press);
		QApplication::sendEvent(list->viewport(), &doubled);
		QApplication::processEvents();
		expect(gestures.size() == 5U && gestures.back().Index == 0U
				   && !gestures.back().Resume,
			   "a double-click opens the row it was made on");

		window.publish(application::recent_reviews_projection{ });
		QApplication::processEvents();
		const auto empty = window.focus_order();
		expect(list->count() == 0 && !panel->isVisible() && !list->isVisible()
				   && !resumeButton->isVisible() && !empty.contains(list)
				   && !empty.contains(panel->header()),
			   "an empty list is no panel, no rows, no control and no keyboard stop");
		window.hide();

		// [[BPR-0732]] still decides where a signed-out Home starts: at the
		// bar, with the list beside it rather than in front of it.
		qt::workbench_window arriving{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		arriving.resize(1'280, 820);
		arriving.publish(projection);
		arriving.show();
		arriving.activateWindow();
		QApplication::processEvents();
		expect(arriving.home_recent_list()->count() == 3
				   && arriving.home_recent_list()->isVisible()
				   && QApplication::focusWidget() == arriving.home_open_bar(),
			   "a signed-out arrival lists what was read and still starts at the bar");
		arriving.hide();
	}

	auto test_home_action_list_opens_rows_and_the_bar_on_overview() -> void
	{
		std::vector<application::github_intent> intents;
		qt::workbench_window                    window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[&intents](application::github_intent aIntent) {
				intents.push_back(aIntent);
			},
		};
		// [[BPR-0689]]. The bar no longer parses: Return and Open submit the
		// text as the setup destination, and the application answers with a
		// projection that either opens or explains beside the bar.
		std::vector<std::string> submitted;
		window.set_setup_destination_dispatch([&](application::setup_destination_intent aIntent) {
			const auto* const submit = std::get_if<application::submit_setup_destination>(&aIntent);
			if (submit == nullptr) {
				return;
			}
			submitted.push_back(submit->Text);
			const auto names = submit->Text.find("/pull/") != std::string::npos || submit->Text.find('#') != std::string::npos;
			window.present_setup_destination(names ? application::setup_destination_projection{ .Pending = std::nullopt, .Notice = { }, .Opened = true }
												   : application::setup_destination_projection{ .Pending = std::nullopt, .Notice = "Paste a pull request link or owner/name#number.", .Opened = false });
		});
		window.resize(1'280, 820);
		window.show();
		QApplication::processEvents();
		publish_session_ready(window);
		QApplication::processEvents();
		expect(window.showing_home() && window.home_open_bar() != nullptr
				   && window.home_page()->isAncestorOf(window.home_open_bar())
				   && window.home_page()->isAncestorOf(window.home_list())
				   && window.home_list()->maximumHeight() == QWIDGETSIZE_MAX,
			   "Home carries the open bar and the list, with the list given room");
		auto needsMe = core::queue_item{
			.Ref        = { .Host = "github.com", .Account = "octo-reviewer", .Repository = "better-pull-review/desktop", .Number = 73 },
			.Membership = { .Role = core::github_role::requested_reviewer },
			.Bucket     = core::attention_bucket::initial_review_requested,
		};
		needsMe.Facts    = core::queue_item_facts{ .Title = "Preserve the DRCS font", .Author = "octocat", .UnseenComments = 1U };
		const auto quiet = core::queue_item{
			.Ref        = { .Host = "github.com", .Account = "octo-reviewer", .Repository = "better-pull-review/desktop", .Number = 74 },
			.Membership = { .Role = core::github_role::participant },
			.Bucket     = core::attention_bucket::quiet,
		};
		window.publish(application::queue_landing_projection{
			.Status = application::queue_landing_status::ready,
			.Items  = { needsMe, quiet },
		});
		QApplication::processEvents();
		// [[BPR-0776]]. Each state is its own panel over the one decided queue,
		// drawn through the one row delegate; an empty state is not shown.
		auto* const needsView     = window.home_list()->state_view(qt::home_group::needs_you);
		auto* const followingView = window.home_list()->state_view(qt::home_group::following);
		auto* const delegate      = window.home_list()->row_delegate();
		expect(delegate != nullptr && needsView->itemDelegate() == delegate && followingView->itemDelegate() == delegate
				   && needsView->model()->rowCount() == 1 && followingView->model()->rowCount() == 1
				   && window.home_list()->state_panel(qt::home_group::needs_you)->isVisible()
				   && !window.home_list()->state_panel(qt::home_group::waiting_on_others)->isVisible(),
			   "each state's rows draw through the one row delegate in their own panel, and an empty state is not shown");
		const auto first  = needsView->model()->index(0, 0);
		const auto second = followingView->model()->index(0, 0);
		expect(first.data(qt::home_row_role::reason_word).toString() == QStringLiteral("Requested")
				   && second.data(qt::home_row_role::reason_word).toString() == QStringLiteral("Quiet")
				   && delegate->sizeHint({ }, first).height() >= qt::home_row_delegate::minimum_row_height,
			   "each row ends in its state's word and is at least 44 points tall");
		expect(first.data(qt::home_row_role::title).toString() == QStringLiteral("Preserve the DRCS font")
				   && first.data(qt::home_row_role::why).toString().endsWith(QStringLiteral("1 new comment.")),
			   "the title and what I have not read reach the row");

		// [[BPR-0736]]. Send real keys: emitting activated would hide macOS's
		// edit-key handling, which never emits that signal for Return or Enter.
		auto* const view = followingView;
		view->setCurrentIndex(second);
		view->setFocus();
		for (const auto key : { Qt::Key_Return, Qt::Key_Enter }) {
			intents.clear();
			auto pressed = QKeyEvent{ QEvent::KeyPress, key, Qt::NoModifier };
			QApplication::sendEvent(view, &pressed);
			QApplication::processEvents();
			const auto* const opened = intents.empty() ? nullptr : std::get_if<core::open_pull_request_intent>(&intents.back());
			expect(intents.size() == 1U && opened != nullptr
					   && opened->Repository == core::repository_locator{ .Owner = "better-pull-review", .Name = "desktop" }
					   && opened->Number == core::pull_request_number{ 74U },
				   "Return and Enter each open exactly the current queue row through the GitHub intent");
		}
		view->setCurrentIndex({ });
		intents.clear();
		auto noSelection = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(view, &noSelection);
		expect(intents.empty(), "Return without a current queue row opens nothing");

		intents.clear();
		const auto rowRect = needsView->visualRect(first);
		expect(!rowRect.isEmpty(), "the first row is on screen");
		{
			const auto at      = QPointF{ rowRect.center() };
			auto       press   = QMouseEvent{ QEvent::MouseButtonPress, at, at, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			auto       release = QMouseEvent{ QEvent::MouseButtonRelease, at, at, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(needsView->viewport(), &press);
			QApplication::sendEvent(needsView->viewport(), &release);
		}
		QApplication::processEvents();
		const auto* const open = intents.empty() ? nullptr : std::get_if<core::open_pull_request_intent>(&intents.back());
		expect(intents.size() == 1U && open != nullptr
				   && open->Repository == core::repository_locator{ .Owner = "better-pull-review", .Name = "desktop" }
				   && open->Number == core::pull_request_number{ 73U },
			   "clicking a row opens its pull request through the GitHub intent");
		window.choose_modality(qt::review_modality::read);
		publish_pull_request(window);
		QApplication::processEvents();
		expect(window.current_modality() == qt::review_modality::overview,
			   "the pull request chosen from Home arrives on Overview");

		window.choose_home(true);
		intents.clear();
		window.home_open_bar()->setText(QStringLiteral("https://github.com/octo/app/pull/9"));
		{
			auto pressed = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
			QApplication::sendEvent(window.home_open_bar(), &pressed);
		}
		QApplication::processEvents();
		expect(submitted.size() == 1U && submitted.back() == "https://github.com/octo/app/pull/9" && intents.empty()
				   && window.home_open_bar()->text().isEmpty(),
			   "Return in the open bar submits the text as the setup destination and the accepted projection clears the bar");
		intents.clear();
		window.home_open_bar()->setText(QStringLiteral("not a pull request"));
		{
			auto pressed = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
			QApplication::sendEvent(window.home_open_bar(), &pressed);
		}
		QApplication::processEvents();
		expect(submitted.size() == 2U && intents.empty() && window.home_open_bar()->text() == QStringLiteral("not a pull request"),
			   "text that names no pull request is still submitted, opens nothing and stays for correction");
		// [[BPR-0669]]. Silence was indistinguishable from a product that does
		// nothing, and Return was the only affordance the bar had.
		expect(window.home_open_notice() != nullptr && window.home_open_notice()->isVisible()
				   && window.home_open_notice()->text().contains(QStringLiteral("owner/name#number")),
			   "a paste that names no pull request is answered beside the bar");
		expect(window.home_open_action() != nullptr && window.home_open_action()->isEnabled(),
			   "Open stays offered for any text, because the application, not the bar, decides what it names");
		intents.clear();
		window.home_open_bar()->setText(QStringLiteral("  https://github.example.com/octo/app/pull/42  "));
		expect(window.home_open_action()->isEnabled() && !window.home_open_notice()->isVisible(),
			   "a pasted link on any host offers Open and clears the refusal");
		window.home_open_action()->trigger();
		QApplication::processEvents();
		expect(submitted.size() == 3U && submitted.back().find("github.example.com/octo/app/pull/42") != std::string::npos && intents.empty()
				   && window.home_open_bar()->text().isEmpty(),
			   "Open submits the same text Return does, from a link that was pasted whole, and the accepted projection clears the bar");
		window.hide();
	}

	auto test_repositories_drawer_lists_the_queue_and_opens_on_overview() -> void
	{
		// [[BPR-0651]]. The repositories drawer, left of the files: the queue's
		// pull requests under their repositories, a toggle, a rail, and a row
		// that opens its pull request on Overview.
		std::vector<application::github_intent> intents;
		qt::workbench_window                    window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[&intents](application::github_intent aIntent) {
				intents.push_back(aIntent);
			},
		};
		window.resize(1'280, 820);
		window.show();
		QApplication::processEvents();
		publish_session_ready(window);
		publish_pull_request(window);
		QApplication::processEvents();

		auto* const drawer   = window.repositories_drawer();
		auto* const tree     = window.repositories_tree();
		auto* const splitter = window.findChild<QSplitter*>(QStringLiteral("workbenchCentralSplitter"));
		expect(drawer != nullptr && tree != nullptr && drawer->isVisible() && splitter != nullptr
				   && splitter->indexOf(drawer) == 0
				   && splitter->indexOf(window.files_drawer()) == 1
				   && splitter->indexOf(window.conversations_drawer()) == 3
				   && drawer->isAncestorOf(tree),
			   "the repositories drawer sits left of the files drawer with the tree inside");
		expect(window.repositories_drawer_toggle_action()->shortcut()
					   == QKeySequence{ QStringLiteral("Alt+Shift+R") }
				   && window.primary_toolbar()->actions().indexOf(window.repositories_drawer_toggle_action())
					   < window.primary_toolbar()->actions().indexOf(window.files_drawer_toggle_action()),
			   "its toggle precedes Files in the strip with its own binding");

		window.publish(application::queue_landing_projection{
			.Status = application::queue_landing_status::ready,
			.Items  = {
				core::queue_item{
					.Ref        = { .Host = "github.com", .Account = "octo-reviewer", .Repository = "better-pull-review/desktop", .Number = 73 },
					.Membership = { .Role = core::github_role::requested_reviewer },
					.Bucket     = core::attention_bucket::initial_review_requested,
				},
				core::queue_item{
					.Ref        = { .Host = "github.com", .Account = "octo-reviewer", .Repository = "better-pull-review/desktop", .Number = 74 },
					.Membership = { .Role = core::github_role::participant },
					.Bucket     = core::attention_bucket::quiet,
				},
			},
		});
		QApplication::processEvents();
		auto* const model = tree->tree_model();
		expect(model->rowCount() == 1 && model->item_count() == 2U
				   && model->index(0, 0).data().toString() == QStringLiteral("better-pull-review/desktop")
				   && drawer->rail()->summary() == QStringLiteral("1"),
			   "the queue's pull requests hang under their repository and the rail counts the ones that need me");

		// Choosing a row opens its pull request, which arrives on Overview.
		intents.clear();
		const auto row = model->index(0, 0, model->index(0, 0));
		tree->setCurrentIndex(row);
		QApplication::processEvents();
		const auto* const open = intents.empty() ? nullptr : std::get_if<core::open_pull_request_intent>(&intents.back());
		expect(intents.size() == 1U && open != nullptr
				   && open->Repository == core::repository_locator{ .Owner = "better-pull-review", .Name = "desktop" }
				   && open->Number == core::pull_request_number{ 73U },
			   "choosing a pull request row opens it through the GitHub intent");
		window.choose_modality(qt::review_modality::read);
		publish_pull_request(window);
		QApplication::processEvents();
		expect(window.current_modality() == qt::review_modality::overview,
			   "the pull request chosen from the tree arrives on Overview");
		window.hide();
	}

	auto test_settings_pages_carry_the_server_cards_and_the_rule_editor() -> void
	{
		// [[BPR-0652]] [[BPR-0771]]. Accounts & hosts holds one card per host,
		// which replaced the single account card; Reviews & queue holds the
		// observer-rule editor that used to sit on Home.
		std::vector<application::github_intent> intents;
		qt::workbench_window                    window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[&intents](application::github_intent aIntent) { intents.push_back(std::move(aIntent)); },
		};
		window.resize(1'280, 820);
		window.show();
		QApplication::processEvents();
		auto* const card = window.server_card_for(core::host_profile_id{ 12U });
		expect(card != nullptr && !card->signed_in()
				   && card->login_label()->text() == QStringLiteral("No account")
				   && card->state_label()->text() == QStringLiteral("Signed out")
				   && window.settings()->page_widget(qt::settings_page::accounts_and_hosts)->isAncestorOf(card)
				   && window.findChild<QWidget*>(QStringLiteral("accountCard")) == nullptr,
			   "signed out, the host's card on Accounts and hosts says so, and no separate account card remains");
		publish_session_ready(window);
		QApplication::processEvents();
		expect(card->signed_in() && card->login_label()->text() == QStringLiteral("octo-reviewer")
				   && card->state_label()->text() == QStringLiteral("Connected")
				   && card->rate_limit_label()->isVisibleTo(card)
				   && card->rate_limit_label()->text().contains(QStringLiteral("requests remaining")),
			   "signed in, the card names the account, reads Connected and states the rate limit");
		// [[BPR-0660]]. The grant sentence as RFC-BPR-0032 states it. The date is
		// local and the machine is named by platform, so neither is pinned.
		expect(card->consequence_label()->isVisibleTo(card)
				   && card->consequence_label()->text().startsWith(QStringLiteral("Signed in with a device grant on 2025-09-0"))
				   && card->consequence_label()->text().contains(QStringLiteral("; renews itself and is remembered on this ")),
			   "a durable device grant reads as signed in on its date, renewing, remembered on this machine");
		// [[BPR-0660]]. Sign out on the card leaves as the deactivation the host
		// forgets the grant on; signed out it is gone.
		expect(card->sign_out_button()->isVisibleTo(card), "signed in, the card offers Sign out");
		card->sign_out_button()->click();
		expect(intents.size() == 1U
				   && std::holds_alternative<core::deactivate_github_identity_intent>(intents.back()),
			   "Sign out on the card dispatches the deactivation intent");
		// The host answers the deactivation by retiring the session, which is
		// what the card follows; it holds no account state of its own to clear.
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{ .Status = core::github_status::no_profile },
			},
		});
		QApplication::processEvents();
		expect(!card->sign_out_button()->isVisibleTo(card) && !card->signed_in()
				   && !card->consequence_label()->text().startsWith(QStringLiteral("Signed in")),
			   "signed out, neither Sign out nor the grant sentence remains");
		auto* const rules = window.rules_editor();
		expect(rules != nullptr
				   && window.settings()->page_widget(qt::settings_page::reviews_and_queue)->isAncestorOf(rules)
				   && !window.home_page()->isAncestorOf(rules)
				   && rules->isAncestorOf(window.queue_rule_kind_selector())
				   && rules->isAncestorOf(window.queue_rule_pattern_editor())
				   && !window.focus_order().contains(window.queue_rule_kind_selector())
				   // [[BPR-0783]]. The queue's six typed controls moved to the
				   // same page, so they are off Home and off this order too.
				   && !window.focus_order().contains(window.queue_role_filter())
				   && window.settings()->page_widget(qt::settings_page::reviews_and_queue)->isAncestorOf(window.queue_role_filter()),
			   "the observer-rule editor and the queue defaults live on Reviews and queue, off Home and out of the workbench's focus order");
		window.hide();
	}

	// [[BPR-0981]]. RFC-BPR-0025 AC-02 through the composed window: both
	// drawer filters are resident, keep parent context, say so when nothing
	// matches and clear from that notice, send nothing to a provider, and
	// leave the open pull request and the selected file exactly where they were.
	auto test_drawer_filters_are_resident_and_keep_the_open_views() -> void
	{
		std::vector<application::github_intent>         intents;
		std::vector<core::workbench_interaction_intent> workbench;
		qt::workbench_window                            window{
			[&workbench](core::workbench_interaction_intent aIntent) { workbench.push_back(aIntent); },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[&intents](application::github_intent aIntent) { intents.push_back(aIntent); },
		};
		window.resize(1'280, 820);
		window.show();
		QApplication::processEvents();
		publish_session_ready(window);
		publish_pull_request(window);
		window.publish(application::queue_landing_projection{
			.Status = application::queue_landing_status::ready,
			.Items  = {
				core::queue_item{
					.Ref        = { .Host = "github.com", .Account = "octo-reviewer", .Repository = "better-pull-review/desktop", .Number = 73 },
					.Membership = { .Role = core::github_role::requested_reviewer },
					.Bucket     = core::attention_bucket::initial_review_requested,
					.Facts      = { .Title = "Resident drawer filters" },
				},
				core::queue_item{
					.Ref        = { .Host = "github.com", .Account = "octo-reviewer", .Repository = "better-pull-review/desktop", .Number = 74 },
					.Membership = { .Role = core::github_role::participant },
					.Bucket     = core::attention_bucket::quiet,
					.Facts      = { .Title = "Epoch line dates" },
				},
				// The pull request `publish_pull_request` opened.
				core::queue_item{
					.Ref        = { .Host = "github.com", .Account = "octo-reviewer", .Repository = "openai/better-pull-review", .Number = 27 },
					.Membership = { .Role = core::github_role::participant },
					.Bucket     = core::attention_bucket::quiet,
					.Facts      = { .Title = "Literal title" },
				},
			},
		});
		auto overview = application::pull_request_overview_projection{ };
		overview.Files.push_back(application::overview_file_row{
			.Path = "src/host/ConptyConnection.cpp", .Name = "ConptyConnection.cpp", .Operation = application::overview_file_operation::modified, .Additions = 12U, .Deletions = 3U });
		overview.Files.push_back(application::overview_file_row{
			.Path = "src/terminal/adapter/adaptDispatch.cpp", .Name = "adaptDispatch.cpp", .Operation = application::overview_file_operation::modified, .Additions = 4U, .Deletions = 0U });
		window.publish(overview);
		window.choose_modality(qt::review_modality::review);
		QApplication::processEvents();

		auto* const files        = window.files_tree();
		auto* const filesFilter  = window.files_filter();
		auto* const filesNotice  = window.files_filter_notice();
		auto* const repositories = window.repositories_tree();
		auto* const reposFilter  = window.repositories_filter();
		auto* const reposNotice  = window.repositories_filter_notice();
		expect(files != nullptr && filesFilter != nullptr && filesNotice != nullptr && repositories != nullptr
				   && reposFilter != nullptr && reposNotice != nullptr
				   && window.repositories_drawer()->isAncestorOf(reposFilter)
				   && window.repositories_drawer()->isAncestorOf(reposNotice)
				   && window.files_drawer()->isAncestorOf(filesNotice)
				   && reposFilter->accessibleName() == QStringLiteral("Filter pull requests")
				   && filesFilter->accessibleName() == QStringLiteral("Filter files")
				   && !reposFilter->accessibleDescription().isEmpty()
				   && filesNotice->isHidden() && reposNotice->isHidden(),
			   "each drawer carries its own named filter and a hidden empty-state notice");
		const auto adapt = core::file_identity_of("src/terminal/adapter/adaptDispatch.cpp");
		files->setCurrentIndex(files->tree_model()->index_for_file(adapt));
		QApplication::processEvents();
		const auto selected = files->tree_model()->file_at(files->currentIndex());
		expect(selected == std::optional{ adapt } && !window.showing_home(),
			   "a file is selected and the pull request is open before filtering");
		auto* const model   = repositories->tree_model();
		const auto  openRow = [model, repositories] {
			const auto item = model->item_at(repositories->currentIndex());
			return item.has_value() ? item->Ref.Number : std::int64_t{ 0 };
		};
		expect(openRow() == 27, "the open pull request's row is current in the repositories tree");
		const auto workbenchBefore = workbench.size();
		const auto githubBefore    = intents.size();

		filesFilter->setText(QStringLiteral("cncon"));
		QApplication::processEvents();
		expect(files->tree_model()->file_count() == 1U
				   && files->tree_model()->index_for_file(core::file_identity_of("src/host/ConptyConnection.cpp")).isValid()
				   && files->tree_model()->rowCount() == 1
				   && files->tree_model()->index(0, 0).data().toString() == QStringLiteral("src/host")
				   && filesNotice->isHidden(),
			   "letters in order find the file and keep its folders above it");
		filesFilter->setText(QStringLiteral("nothing like this"));
		QApplication::processEvents();
		expect(files->tree_model()->file_count() == 0U && !filesNotice->isHidden()
				   && filesNotice->message().contains(QStringLiteral("No files match"))
				   && filesNotice->message().contains(QStringLiteral("nothing like this"))
				   && !files->currentIndex().isValid(),
			   "a files filter nothing matches says so, names the filter, and offers Clear filter");
		filesNotice->clear_button()->click();
		QApplication::processEvents();
		expect(filesFilter->text().isEmpty() && files->tree_model()->file_count() == 2U && filesNotice->isHidden()
				   && files->tree_model()->file_at(files->currentIndex()) == std::optional{ adapt },
			   "Clear filter empties the field, lists every file again and the selected file is current again");

		reposFilter->setText(QStringLiteral("epoch"));
		QApplication::processEvents();
		expect(model->rowCount() == 1 && model->item_count() == 1U
				   && model->index(0, 0).data().toString() == QStringLiteral("better-pull-review/desktop")
				   && model->index(0, 0, model->index(0, 0)).data(qt::repositories_tree_model::number_role).toLongLong() == 74
				   && reposNotice->isHidden() && !repositories->currentIndex().isValid(),
			   "a title match keeps its repository row and shows that pull request alone; the hidden open row is not current");
		reposFilter->setText(QStringLiteral("#73"));
		QApplication::processEvents();
		expect(model->rowCount() == 1 && model->item_count() == 1U
				   && model->index(0, 0, model->index(0, 0)).data(qt::repositories_tree_model::number_role).toLongLong() == 73,
			   "a number match finds the pull request by its digits");
		reposFilter->setText(QStringLiteral("zzz"));
		QApplication::processEvents();
		expect(model->rowCount() == 0 && !reposNotice->isHidden()
				   && reposNotice->message().contains(QStringLiteral("No pull requests match")),
			   "a repositories filter nothing matches says so and offers Clear filter");
		reposNotice->clear_button()->click();
		QApplication::processEvents();
		expect(reposFilter->text().isEmpty() && model->rowCount() == 2 && model->item_count() == 3U && reposNotice->isHidden()
				   && openRow() == 27,
			   "Clear filter lists every repository and pull request again and the open one is current again");

		expect(intents.size() == githubBefore && workbench.size() == workbenchBefore && !window.showing_home()
				   && files->tree_model()->file_at(files->currentIndex()) == std::optional{ adapt },
			   "filtering and clearing sent nothing to a provider or the core and left the open pull request and selected file in place");

		// Both empty states at once, rendered for inspection when a path is given.
		if (const auto path = qEnvironmentVariable("BPR_QT_FILTER_RENDER_PATH"); !path.isEmpty()) {
			reposFilter->setText(QStringLiteral("zzz"));
			filesFilter->setText(QStringLiteral("nothing like this"));
			QApplication::processEvents();
			expect(!reposNotice->isHidden() && !filesNotice->isHidden() && window.grab().save(path),
				   "both drawers' empty states render with their Clear filter");
			reposNotice->clear_button()->click();
			filesNotice->clear_button()->click();
			QApplication::processEvents();
			expect(openRow() == 27 && files->tree_model()->file_at(files->currentIndex()) == std::optional{ adapt }
					   && intents.size() == githubBefore,
				   "clearing both empty states restores both current rows without an effect");
		}

		// A row chosen in the tree opens once; when the window then opens
		// another pull request, that row is a fresh gesture again.
		const auto rowOf = [model](const std::int64_t aNumber) {
			return model->index_for(std::string_view{ "better-pull-review/desktop" }, aNumber);
		};
		repositories->setCurrentIndex(rowOf(74));
		QApplication::processEvents();
		expect(intents.size() == githubBefore + 1U, "choosing a row in the tree opens its pull request once");
		publish_pull_request(window);
		QApplication::processEvents();
		expect(openRow() == 27, "the pull request the window opened takes the current row");
		repositories->setCurrentIndex(rowOf(74));
		QApplication::processEvents();
		const auto* const reopened = intents.size() == githubBefore + 2U
			? std::get_if<core::open_pull_request_intent>(&intents.back())
			: nullptr;
		expect(reopened != nullptr && reopened->Number == core::pull_request_number{ 74U },
			   "choosing the earlier row again opens it rather than being swallowed as already current");
		window.hide();
	}

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
		// [[BPR-1134]]. The agent surfaces are the Findings tab's.
		window.ai_tools_action()->setChecked(true);
		QApplication::processEvents();

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

		// The workshop a promotion opens is the retained draft editor's own, on
		// its dedicated in-window page. There is no second editor or top-level window.
		auto* const composePage = window.findChild<QScrollArea*>(
			QStringLiteral("composeInlineDraftPage"));
		expect(
			composePage != nullptr && composePage->isAncestorOf(editor)
				&& composePage->isAncestorOf(editor->workshop_original_editor())
				&& composePage->isAncestorOf(editor->workshop_proposal_editor()),
			"the comment workshop a finding promotion reaches is the retained editor on its dedicated page");

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
		auto* const column = window.findings_tab_scroll();
		expect(
			column != nullptr && drawer->isAncestorOf(column) && column->isVisible(),
			"the Findings tab's column scrolls rather than squeezing its panels");
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
		// [[BPR-0721]]. With the drawer controls hidden, the default, the
		// collapsed drawer is no column at all: not in the splitter's width,
		// no rail, no label, no count. The crop is the window's right edge,
		// where the drawer's column would be.
		const auto rightEdge = [&window, &scaled, &renderPath](const QString& aSuffix) {
			QApplication::processEvents();
			const auto  raster = window.grab();
			const auto  edge   = std::max(0, window.width() - 160);
			const QRect edgeRegion{
				QPoint{ scaled(edge), 0 },
				QPoint{ scaled(window.width()) - 1, scaled(window.height()) - 1 },
			};
			const auto edgeCrop = raster.copy(edgeRegion);
			expect(raster.rect().contains(edgeRegion) && edgeCrop.size() == edgeRegion.size(),
				   "the right-edge crop has exact device dimensions");
			if (!renderPath.isEmpty()) {
				const QFileInfo configured{ renderPath };
				expect(edgeCrop.save(QStringLiteral("%1/%2-%3.png")
										 .arg(configured.path(), configured.completeBaseName(), aSuffix),
									 "PNG"),
					   "the right-edge crop is saved in the ignored build tree");
			}
		};
		const auto sizes = window.central_splitter()->sizes();
		expect(!drawer->isVisible() && !drawer->rail()->isVisible() && !drawer->controls_shown()
				   && sizes.size() == 4 && sizes[3] == 0
				   && !sideChat->isVisible() && !findingQueue->isVisible(),
			   "with the drawer controls hidden a collapsed drawer occupies zero "
			   "width in the splitter and both placed agent surfaces go with it");
		rightEdge(QStringLiteral("drawer-controls-hidden"));
		auto controlsShown      = application::workbench_projection{ };
		controlsShown.Behaviour = application::review_behaviour_settings{
			.DrawerControls = application::review_drawer_controls::shown,
		};
		window.publish(controlsShown);
		QApplication::processEvents();
		auto* const heading = drawer->findChild<QLabel*>(QStringLiteral("workbenchConversationsDrawerHeading"));
		expect(
			drawer->rail()->isVisible()
				&& drawer->width() == qt::workbench_drawer_rail::width_pixels
				&& heading != nullptr && !heading->isVisible()
				&& !drawer->collapse_button()->isVisible()
				&& !sideChat->isVisible() && !findingQueue->isVisible(),
			"collapsing the drawer to its rail hides both placed agent surfaces "
			"with it, and nothing of the heading row stays above the rail");
		rightEdge(QStringLiteral("drawer-controls-rail"));
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
		// Comments navigation and its thread cards can be shown together.
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
		// [[BPR-1204]], [[BPR-1208]]. Notes holds private tools; defect
		// navigation is in Comments and verdicts belong to the thread menu.
		window.ai_tools_action()->setChecked(true);
		QApplication::processEvents();
		auto* const column = window.findings_tab_scroll();
		expect(
			column != nullptr && column->isVisible() && column->viewport() != nullptr
				&& column->viewport()->width() > 0,
			"the drawer's column has a visible width to be inside of");
		// The premise everything below rests on. At its default width the
		// column does not scroll sideways, so the viewport's width is the whole
		// of what a reviewer can reach, at every scroll position the column
		// has. ([[BPR-0728]] lets it scroll sideways only beneath the column's
		// own minimum, which a drag below the default reaches; not this width.)
		expect(
			column->horizontalScrollBar()->maximum() == 0
				&& !column->horizontalScrollBar()->isVisible(),
			"the drawer's column does not scroll sideways at its default width");

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
			 { // The agent launcher's run controls (RFC-BPR-0011).
			   "agentExistingCheckoutAction", "startAgentAction",
			   "cancelAgentAction",
			   // The side chat's row (RFC-BPR-0012, placed by BPR-0513).
			   "sideChatSend", "sideChatAddContext", "sideChatStartNewSession",
			   "sideChatContinueOldRevision",
			   // The six the finding queue declares (BPR-0506).
			   "findingOpenEvidence", "findingAskFollowUp",
			   "findingConvertToDraftComment", "findingConvertToDraftDefect",
			   "findingAddToPrivateNotes", "findingDismiss",
			   // The private note editor and one-item promotion (BPR-1210).
			   "newReviewNoteAction", "newFileNoteAction", "savePrivateNoteAction",
			   "promotePrivateNoteAction", "deletePrivateNoteAction",
			   "discardPrivateNoteChangesAction", "promoteChatReplyAction" }) {
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
		// [[BPR-1134]]. The availability sentence is the Comments tab's.
		window.tray_tabs()->setCurrentIndex(0);
		QApplication::processEvents();
		auto* const availability = drawer->findChild<QLabel*>(
			QStringLiteral("reviewThreadAvailability"));
		expect(
			availability != nullptr && availability->isVisible(),
			"the open drawer's Comments tab shows the review-thread availability sentence");
		window.tray_tabs()->setCurrentIndex(1);
		QApplication::processEvents();
		expect(!offered(QStringLiteral("showReviewThreadsAction")),
			   "the conversation drawer has no focus-only reveal action below its availability sentence");
		expect(
			stackedUnder(
				offered(QStringLiteral("agentExistingCheckoutAction")),
				offered(QStringLiteral("startAgentAction")))
				&& stackedUnder(
					offered(QStringLiteral("startAgentAction")),
					offered(QStringLiteral("cancelAgentAction"))),
			"the agent launcher's run controls are stacked rather than laid in a row: startAgentAction, cancelAgentAction");

		// The moved controls must be absent from Notes, not silently omitted
		// from its width sweep. Comments keeps its own measured navigation.
		for (const auto* name : { "classifySelectedDefect", "previousDefect", "nextDefect",
								  "defectVerify", "defectReopen", "defectWithdraw", "defectAcceptAsIs", "defectReclassify" }) {
			const auto* button = offered(QString::fromLatin1(name));
			expect(button == nullptr || !column->isAncestorOf(button),
				   std::string{ "Notes contains no thread control: " } + name);
		}
		expect(!column->isAncestorOf(window.defect_card()) && !column->isAncestorOf(window.defect_count_label()),
			   "Notes contains neither the defect editor nor the queue position");
		window.tray_tabs()->setCurrentIndex(0);
		QApplication::processEvents();
		auto* const comments        = window.comments_tab_scroll();
		auto        commentControls = std::vector<QWidget*>{ };
		collect_drawer_controls(comments->widget(), commentControls);
		for (auto* control : { static_cast<QWidget*>(window.defect_count_label()),
							   static_cast<QWidget*>(offered(QStringLiteral("previousDefect"))),
							   static_cast<QWidget*>(offered(QStringLiteral("nextDefect"))) }) {
			expect(control && control->isVisible() && comments->isAncestorOf(control)
					   && laid_into(control->parentWidget()->layout(), control)
					   && std::ranges::find(commentControls, control) != commentControls.end(),
				   "Comments visibly lays out and sweeps every defect navigation control");
			const QRect placed{ control->mapTo(comments->viewport(), QPoint{ }), control->size() };
			expect(comments->viewport()->rect().contains(placed),
				   std::string{ "Comments defect navigation is wholly reachable: " } + control_name(control));
		}

		auto* const cards = window.conversation_panel()->cards();
		{
			const auto  menu     = std::unique_ptr<QMenu>{ cards->thread_menu("review-thread:t1") };
			const auto* classify = menu ? menu->findChild<QAction*>(QStringLiteral("threadClassifyDefect")) : nullptr;
			expect(classify && classify->isVisible() && !classify->isEnabled() && !classify->toolTip().isEmpty(),
				   "the thread menu explains why classification needs a current code comparison");
		}
		window.present_defects({ core::defect_history{ .Defect = { .Id = core::defect_id{ 1U }, .State = core::defect_state::needs_verification } } },
							   { { core::defect_id{ 1U }, "review-thread:t1" } });
		QApplication::processEvents();
		const auto badges = cards->findChildren<QToolButton*>(QStringLiteral("threadDefectBadge"));
		expect(badges.size() == 1 && badges.front()->isVisible()
				   && badges.front()->property("threadIdentity") == QStringLiteral("review-thread:t1")
				   && !badges.front()->text().isEmpty(),
			   "the composed Comments thread owns one visible text defect badge");
		cards->set_current_thread("review-thread:t1");
		QKeyEvent menuKey{ QEvent::KeyPress, Qt::Key_F10, Qt::ShiftModifier };
		QApplication::sendEvent(cards, &menuKey);
		QApplication::processEvents();
		auto* const menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
		expect(menu && menu->isVisible(), "Shift+F10 reaches the composed thread's defect menu");
		for (const auto* name : { "defectVerify", "defectReopen", "defectWithdraw", "defectAcceptAsIs", "threadReclassifyDefect" }) {
			auto* action = menu->findChild<QAction*>(QString::fromLatin1(name));
			expect(action && action->isVisible() && !menu->actionGeometry(action).isEmpty()
					   && menu->rect().contains(menu->actionGeometry(action)) && !action->toolTip().isEmpty(),
				   std::string{ "the reachable thread menu retains the explained defect action: " } + name);
		}
		menu->close();
		QApplication::processEvents();

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
	/// \brief Home's landing funnels a reviewer with nothing open.
	auto test_home_landing_funnels_to_an_account_and_then_a_pull_request() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		window.resize(1'280, 820);
		window.show();
		QApplication::processEvents();
		auto* const landing = window.home_landing();
		expect(
			window.showing_home() && window.home_page()->isVisible()
				&& !window.files_drawer()->isVisible()
				&& !window.conversations_drawer()->isVisible()
				&& !window.epoch_selectors()->isVisible()
				&& !window.comparison_surface()->isVisible()
				&& !window.home_list()->queue_shown()
				&& window.home_page()->isAncestorOf(window.home_list()),
			"with nothing open the centre is Home -- the landing, and the review "
			"queue once there is an account -- and none of the review chrome is shown");
		expect(
			landing != nullptr && landing->isVisible()
				&& landing->state() == qt::landing_state::sign_in
				&& landing->title_label()->text()
					== QStringLiteral("Sign in to GitHub")
				&& landing->primary_button()->isVisible()
				&& landing->primary_button()->text().startsWith(
					QStringLiteral("Sign in"))
				&& window.focus_order().contains(landing->primary_button()),
			"signed out with nothing open, Home offers sign-in and its buttons "
			"are focus stops");

		auto signInGestures = std::vector<application::device_authorization_gesture>{ };
		window.set_device_authorization_dispatch([&signInGestures](application::device_authorization_gesture aGesture) {
			signInGestures.push_back(std::move(aGesture));
		});
		landing->primary_button()->click();
		QApplication::processEvents();
		expect(
			window.sign_in_dialog() != nullptr && window.sign_in_dialog()->isVisible()
				&& !window.settings()->isVisible(),
			"the sign-in button opens the shared sign-in dialog");
		expect(signInGestures.size() == 1U
				   && std::holds_alternative<core::begin_device_authorization_intent>(signInGestures.front())
				   && std::get<core::begin_device_authorization_intent>(signInGestures.front()).Profile == *window.session_panel()->selected_profile(),
			   "Home Sign in begins once for the selected host without a second gesture");
		auto waiting = application::device_authorization_presentation{
			.Stage                    = core::device_authorization_stage::awaiting_authorization,
			.UserCode                 = "TEST-CODE",
			.VerificationUri          = "fixture-sign-in",
			.RemainingValiditySeconds = 300U,
		};
		waiting.Commands = core::device_authorization_commands(core::device_authorization_state{
			.Stage = core::device_authorization_stage::awaiting_authorization,
		});
		window.device_authorization()->apply(waiting);
		QApplication::processEvents();
		// [[BPR-0773]]. The code is not a panel of its own on the page: it is
		// the account row of the card whose host the sign-in was begun for.
		const auto  signingIn     = window.session_panel()->selected_profile();
		auto* const signingInCard = signingIn.has_value() ? window.server_card_for(*signingIn) : nullptr;
		expect(signingInCard != nullptr && signingInCard->device_sign_in() == window.device_authorization()
				   && window.device_authorization()->isVisible()
				   && window.device_authorization()->isVisibleTo(signingInCard)
				   && signingInCard->state_label()->text() == QStringLiteral("Signing in"),
			   "the live code appears as the account row of the chosen host's card, which says Signing in");
		landing->primary_button()->click();
		expect(signInGestures.size() == 1U, "another Home gesture cannot restart a running sign-in");
		window.device_authorization()->cancel_button()->click();
		expect(signInGestures.size() == 2U && std::holds_alternative<core::cancel_device_authorization_intent>(signInGestures.back()),
			   "cancellation remains reachable through the existing device intent");
		auto expired = application::device_authorization_presentation{
			.Stage = core::device_authorization_stage::expired,
		};
		expired.Commands = core::device_authorization_commands(core::device_authorization_state{
			.Stage = core::device_authorization_stage::expired,
		});
		window.device_authorization()->apply(expired);
		window.sign_in_dialog()->reject();
		window.settings()->present(qt::settings_page::accounts_and_hosts);
		// [[BPR-0771]]. Another host is chosen by its card's Connect, which
		// rebinds the device sign-in to it.
		auto* const enterpriseCard = window.server_card_for(core::host_profile_id{ 12U });
		expect(enterpriseCard != nullptr && enterpriseCard->connect_button()->isVisibleTo(enterpriseCard),
			   "the host that is not chosen offers Connect on its card");
		enterpriseCard->connect_button()->click();
		QApplication::processEvents();
		// [[BPR-0774]] review correction. A settled flow's sign-in follows the
		// chosen host, but its expiry is said only on the host the code was
		// asked for, so the enterprise card offers its own ways in instead.
		expect(signInGestures.size() == 2U
				   && window.session_panel()->selected_profile() == core::host_profile_id{ 12U }
				   && window.device_authorization()->bound_profile() == core::host_profile_id{ 12U }
				   && enterpriseCard->device_sign_in() == window.device_authorization()
				   && !window.device_authorization()->isVisible()
				   && enterpriseCard->sign_in_button()->isVisible(),
			   "a card's Connect chooses its host and binds the device sign-in to it without a sign-in gesture, and the expired row is not carried to that card");
		landing->primary_button()->click();
		expect(signInGestures.size() == 3U
				   && std::get<core::begin_device_authorization_intent>(signInGestures.back()).Profile == core::host_profile_id{ 12U },
			   "retry after expiry names the newly selected host rather than the old grant");
		window.set_device_authorization_dispatch({ });
		window.sign_in_dialog()->reject();
		window.settings()->hide();

		publish_session_ready(window);
		QApplication::processEvents();
		// [[BPR-0775]]. Signed in with nothing open the bar leads Home: the card
		// and its focus-only buttons are gone, and naming a pull request is the
		// bar's.
		expect(
			!landing->isVisible()
				&& landing->state() == qt::landing_state::choose_pull_request
				&& !landing->primary_button()->isVisible()
				&& !landing->secondary_button()->isVisible()
				&& window.home_list()->queue_shown()
				&& window.findChild<QWidget*>(QStringLiteral("repositoryOwnerEditor")) == nullptr
				&& window.home_page()->isAncestorOf(window.home_open_bar()),
			"signed in with nothing open, Home has no card and no focus-only buttons, and the bar names a pull request");
		window.settings()->hide();
		window.activateWindow();

		// On Home the strip keeps only Home, account and settings.
		auto* const homeButton = qobject_cast<QToolButton*>(
			window.primary_toolbar()->widgetForAction(window.home_action()));
		expect(
			window.home_action() != nullptr && window.home_action()->isChecked()
				&& window.home_action()->shortcut()
					== QKeySequence{ QStringLiteral("Ctrl+0") }
				&& homeButton != nullptr && homeButton->isVisible()
				&& homeButton->accessibleName() == QStringLiteral("Home")
				&& !window.primary_toolbar()
						->widgetForAction(window.files_drawer_toggle_action())
						->isVisible()
				&& !window.modality()->isVisible()
				// [[BPR-0783]]. Home's strip carries Group, State headers, Filter
				// and the refresh glyph before the account and settings, because
				// an account can fill the list here. [[BPR-0990]]: on Home that
				// glyph is the profile's discovery, not the pull request's refresh.
				&& window.home_group_button()->isVisible()
				&& window.home_group_button()->text() == QStringLiteral("Group: None")
				&& window.home_state_headers_action()->isChecked()
				&& window.home_filter_button()->isVisible()
				&& window.primary_toolbar()
					   ->widgetForAction(window.queue_refresh_action())
					   ->isVisible()
				&& !window.primary_toolbar()
						->widgetForAction(window.github_refresh_action())
						->isVisible()
				&& window.primary_toolbar()
					   ->widgetForAction(window.account_action())
					   ->isVisible()
				&& window.primary_toolbar()
					   ->widgetForAction(window.settings_action())
					   ->isVisible(),
			"on Home the strip shows Home checked, account and settings, and "
			"none of a pull request's controls");

		publish_pull_request(window);
		QApplication::processEvents();
		expect(
			!window.showing_home() && !landing->isVisible()
				&& !window.home_action()->isChecked()
				&& window.files_drawer()->isVisible()
				// [[BPR-1130]]. The review bar, with the Updates line folded
				// as every new window starts.
				&& window.review_bar()->isVisible()
				&& !window.updates_line_open() && !window.epoch_selectors()->isVisible()
				&& window.comparison_surface()->isVisible()
				&& window.primary_toolbar()
					   ->widgetForAction(window.files_drawer_toggle_action())
					   ->isVisible()
				&& window.modality()->isVisible(),
			"with a pull request open the centre is the review workbench, Home "
			"is gone and the strip is whole again");

		// Home on demand over an open review, and the modality brings the
		// review back.
		window.activateWindow();
		// A review control outside Home that is enabled with this pull request:
		// the strip's files toggle, hidden on Home and shown with the review.
		auto* const reviewControl = window.primary_toolbar()->widgetForAction(window.files_drawer_toggle_action());
		reviewControl->setFocus(Qt::OtherFocusReason);
		QApplication::processEvents();
		// The window's own focus child: what it keeps whether or not it is active.
		auto* const reviewFocus = window.focusWidget();
		window.home_action()->trigger();
		QApplication::processEvents();
		// [[BPR-0775]]. One shaded return row above the bar, the bar focused on
		// arrival, and the session line in the status bar.
		expect(reviewFocus != nullptr && reviewFocus == reviewControl,
			   std::string{ "a review control had focus before Home; focus is on " }
				   + (reviewFocus == nullptr ? std::string{ "nothing" } : reviewFocus->objectName().toStdString())
				   + (reviewControl->isEnabled() ? " (control enabled)" : " (control disabled)")
				   + (reviewControl->isVisible() ? " (control visible)" : " (control hidden)"));
		expect(landing->mapTo(window.home_page(), QPoint{ }).y()
				   < window.home_open_frame()->mapTo(window.home_page(), QPoint{ }).y(),
			   "over a review the return row is above the bar");
		expect(!landing->secondary_button()->isVisible() && !landing->body_label()->isVisible()
				   && landing->shortcut_label()->isVisible()
				   && landing->shortcut_label()->text() == window.home_action()->shortcut().toString(QKeySequence::NativeText),
			   "the return row is one button with its shortcut and no second button or body");
		expect(landing->height() < 80,
			   "the return row is one row high; it is " + std::to_string(landing->height()));
		expect(window.focusWidget() == window.home_open_bar(),
			   "the bar has focus when Home appears over the review");
		expect(window.status_text().startsWith(QStringLiteral("Signed in as "))
				   && !window.status_text().contains(QStringLiteral("No pull request is open")),
			   "over a review Home's line is the session line; it says: " + window.status_text().toStdString());
		expect(
			window.showing_home() && window.home_action()->isChecked()
				&& window.modality()->isVisible()
				&& !window.primary_toolbar()
						->widgetForAction(window.files_drawer_toggle_action())
						->isVisible()
				&& landing->isVisible()
				&& landing->state() == qt::landing_state::back_to_review
				&& landing->title_label()->text().startsWith(
					QStringLiteral("Reviewing "))
				&& landing->title_label()->text().contains(QStringLiteral("#"))
				&& landing->primary_button()->text()
					== QStringLiteral("Back to the review"),
			"the Home button shows Home over an open review, names the review "
			"at the top with the way back, and keeps the modalities");
		landing->primary_button()->click();
		QApplication::processEvents();
		expect(
			!window.showing_home() && !window.home_action()->isChecked()
				&& window.focusWidget() == reviewFocus,
			"Back to the review returns to the review with its focus where it was");
		// A strip control that had focus is hidden on Home and shown again with
		// the review, and takes focus back once it is.
		auto* const stripControl = window.primary_toolbar()->widgetForAction(window.files_drawer_toggle_action());
		stripControl->setFocus(Qt::OtherFocusReason);
		QApplication::processEvents();
		expect(window.focusWidget() == stripControl, "a strip control had focus before Home");
		window.home_action()->trigger();
		QApplication::processEvents();
		expect(!stripControl->isVisible() && window.focusWidget() == window.home_open_bar(),
			   "on Home the strip control is hidden and the bar has focus");
		window.modality()->action(qt::review_modality::review)->trigger();
		QApplication::processEvents();
		expect(
			!window.showing_home() && !window.home_action()->isChecked()
				&& stripControl->isVisible() && window.focusWidget() == stripControl,
			"choosing a modality returns to the review too, with focus back on the strip control");
		window.hide();
	}

	/// \brief [[BPR-0646]], [[BPR-0773]]. Device sign-in is the account row of
	/// the card whose host it is bound to: hidden while no flow runs, begun by
	/// the strip's account button, drawn in place with Signing in while a code
	/// is live, and moved with the host a card's Connect chooses.
	auto test_device_sign_in_is_mounted_in_the_chosen_cards_account_row() -> void
	{
		auto nativeProfiles              = profiles();
		nativeProfiles.front().Transport = application::github_profile_transport::native_https;
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			std::move(nativeProfiles),
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
		auto* const panel    = window.device_authorization();
		auto* const session  = window.session_panel();
		const auto  cards    = window.server_cards();
		const auto  selected = session == nullptr ? std::optional<core::host_profile_id>{ } : session->selected_profile();
		auto* const chosen   = selected.has_value() ? window.server_card_for(*selected) : nullptr;
		auto* const account  = chosen == nullptr ? nullptr : chosen->findChild<QWidget*>(QStringLiteral("serverCardAccount"));
		// [[BPR-0773]]. The page is its sentence, then one card per host in
		// profile order. The device sign-in is no longer a panel of its own in
		// the page's column: it is the chosen native card's account row.
		auto* const pageLayout = header->layout();
		auto* const lede       = required_child<QLabel>(window, "accountsLede");
		auto* const cardColumn = pageLayout != nullptr && pageLayout->count() > 1
			? pageLayout->itemAt(1)->layout()
			: nullptr;
		expect(
			session != nullptr && pageLayout != nullptr
				&& pageLayout->indexOf(lede) == 0
				&& pageLayout->indexOf(panel) < 0
				&& cardColumn != nullptr && cards.size() == 2U
				&& cardColumn->indexOf(cards.front()) == 0
				&& cardColumn->indexOf(cards.back()) == 1,
			"the page is its sentence, then the host cards, and the device sign-in is not a panel in the page's own column");
		expect(
			selected.has_value() && chosen != nullptr && account != nullptr
				&& chosen->offers_device_sign_in() && chosen->device_sign_in() == panel
				&& account->isAncestorOf(panel)
				&& std::ranges::none_of(cards, [chosen, panel](const qt::server_card* const aCard) {
					   return aCard != chosen && (aCard->device_sign_in() == panel || aCard->isAncestorOf(panel));
				   })
				&& window.settings()->page_widget(qt::settings_page::accounts_and_hosts)->isAncestorOf(panel),
			"the device sign-in is laid into the account row of the chosen native card, and no other card holds it");
		expect(
			panel->isHidden() && !panel->isVisibleTo(chosen)
				&& chosen->sign_in_button()->isVisibleTo(chosen),
			"while no flow runs the sign-in is hidden, and the card offers its own Sign in");
		expect(
			!window.files_drawer()->isAncestorOf(panel)
				&& std::ranges::none_of(cards, [&window](const qt::server_card* const aCard) {
					   return window.files_drawer()->isAncestorOf(aCard);
				   }),
			"neither sign-in nor a host card is in the files drawer");

		// The window sets no dispatch of its own, so this one is the test's,
		// exactly as it is for the two agent surfaces above.
		auto gestures = std::vector<application::device_authorization_gesture>{ };
		window.set_device_authorization_dispatch(
			[&gestures](application::device_authorization_gesture aGesture) {
				gestures.push_back(std::move(aGesture));
			});
		window.account_action()->trigger();
		QApplication::processEvents();
		expect(
			window.sign_in_dialog() != nullptr && window.sign_in_dialog()->isVisible()
				&& !window.settings()->isVisible()
				&& gestures.size() == 1U
				&& std::holds_alternative<core::begin_device_authorization_intent>(gestures.front())
				&& std::get<core::begin_device_authorization_intent>(gestures.front()).Profile == *selected,
			"the strip's account button opens shared sign-in and begins exactly one device flow for the chosen host");
		expect(
			!window.focus_order().contains(panel->begin_button())
				&& !window.focus_order().contains(panel->open_button())
				&& std::ranges::none_of(cards, [&window](const qt::server_card* const aCard) {
					   return window.focus_order().contains(aCard->connect_button())
						   || window.focus_order().contains(aCard->sign_in_button());
				   }),
			"the workbench declares no focus stops across windows");

		// The core answers a begin with a live code, applied to the panel the
		// way the composition's fan-out applies it. It is what turns the two
		// gestures below into legal ones, and what the capture is a picture of.
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
			panel->isVisible() && panel->isVisibleTo(chosen) && chosen->device_sign_in() == panel
				&& chosen->state_label()->text() == QStringLiteral("Signing in")
				&& !chosen->sign_in_button()->isVisibleTo(chosen)
				&& !chosen->login_label()->isVisibleTo(chosen)
				&& panel->user_code_field()->text() == QStringLiteral("WDJB-MJHT")
				&& panel->address_field()->text() == QStringLiteral("https://github.com/login/device"),
			"a live code is the chosen card's account row, in place of its ways in, and its header says Signing in");
		expect(
			std::ranges::none_of(cards, [chosen](const qt::server_card* const aCard) {
				return aCard != chosen && aCard->state_label()->text() == QStringLiteral("Signing in");
			}),
			"no other card says Signing in");

		panel->open_button()->click();
		panel->cancel_button()->click();
		QApplication::processEvents();
		expect(
			gestures.size() == 3U
				&& std::holds_alternative<application::open_verification_address_gesture>(gestures[1])
				&& std::holds_alternative<core::cancel_device_authorization_intent>(gestures[2]),
			"opening a browser and cancelling both leave through the same sink "
			"and neither is performed here");

		// The sign-in is near the top of the accounts page, so the capture is a
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
		// [[BPR-0771]]. The page scrolls: with one card per host it is taller
		// than the window, so the crop is the part of the page the window shows,
		// which is still the page and only the page.
		const auto visible = headerScroll->visibleRegion().boundingRect();
		expect(!visible.isEmpty() && visible.contains(placed.topLeft()),
			   "the visible part of the accounts page includes the device panel's corner");
		const auto origin = headerScroll->mapTo(settingsWindow, visible.topLeft());
		// Two rounded edges rather than a rounded origin plus a rounded size,
		// for the reason BPR-0520 recorded one crop below.
		const QRect region{
			QPoint{ scaled(origin.x()), scaled(origin.y()) },
			QPoint{ scaled(origin.x() + visible.width()) - 1,
					scaled(origin.y() + visible.height()) - 1 },
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

		// [[BPR-0771]], [[BPR-0773]]. A card's Connect chooses the host a
		// Connect or a new sign-in means, and the sign-in moves into that host's
		// card. The flow is settled back to idle first, so what moves is the
		// mount alone.
		panel->apply(application::device_authorization_presentation{ });
		QApplication::processEvents();
		expect(
			panel->isHidden() && chosen->state_label()->text() == QStringLiteral("Signed out")
				&& chosen->sign_in_button()->isVisibleTo(chosen),
			"a flow settled back to idle hands the row back to the card's own Sign in");
		const auto  other     = *selected == core::host_profile_id{ 11U } ? core::host_profile_id{ 12U } : core::host_profile_id{ 11U };
		auto* const otherCard = window.server_card_for(other);
		expect(
			otherCard != nullptr && otherCard != chosen && otherCard->connect_button()->isVisibleTo(otherCard),
			"the host that is not chosen offers Connect on its card");
		otherCard->connect_button()->click();
		QApplication::processEvents();
		const auto changed = session->selected_profile();
		expect(
			changed.has_value() && *changed == other
				&& panel->bound_profile() == other
				&& otherCard->device_sign_in() == panel && otherCard->isAncestorOf(panel)
				&& chosen->device_sign_in() == nullptr && !chosen->isAncestorOf(panel)
				&& gestures.size() == 3U,
			"a card's Connect moves the device sign-in into its own account row, bound to its host, without a sign-in gesture");
		panel->begin_button()->click();
		QApplication::processEvents();
		expect(
			gestures.size() == 4U
				&& std::holds_alternative<core::begin_device_authorization_intent>(gestures.back())
				&& std::get<core::begin_device_authorization_intent>(gestures.back()).Profile == other,
			"the next request names the host the card's Connect chose");
		window.set_device_authorization_dispatch({ });
	}

	/// \brief [[BPR-0771]]. What Accounts & hosts carried before RFC-BPR-0032.
	///
	/// \details The host picker row and its buttons, the host, transport and
	/// login lines beneath it, the locator with Open and Refresh, the status
	/// banner and its copy, the public-preview strip and the old account card.
	/// None may return to the page under its object name or its accessible name.
	constexpr std::array retired_accounts_object_names{
		"githubProfileSelector",
		"githubProfileLabel",
		"connectGithubButton",
		"signOutGithubButton",
		"publicPreviewButton",
		"githubConnectionDetailsButton",
		"githubConnectionDetails",
		"activeGithubHost",
		"activeGithubTransport",
		"activeGithubAccount",
		"pullRequestLocatorGroup",
		"repositoryOwnerEditor",
		"repositoryNameEditor",
		"pullRequestNumberEditor",
		"pullRequestLocatorActions",
		"openPullRequestButton",
		"refreshPullRequestButton",
		"githubStatusBanner",
		"githubStatus",
		"githubDiagnostic",
		"copyGithubDiagnosticButton",
		"githubPublicPreviewStrip",
		"githubAccessMode",
		"githubPublicPreviewRefusal",
		"githubSessionPanel",
		"githubIdentityGroup",
		"accountCard",
	};

	constexpr std::array retired_accounts_accessible_names{
		"GitHub host profile",
		"Pull request locator",
		"GitHub activity",
		"Read without signing in",
		"Access mode",
		"GitHub connection",
	};

	[[nodiscard]] auto names_retired(
		const QString&                     aName,
		const std::span<const char* const> aRetired) -> bool
	{
		return !aName.isEmpty()
			&& std::ranges::any_of(aRetired, [&aName](const char* const aCandidate) {
				   return aName == QLatin1String{ aCandidate };
			   });
	}

	/// \brief Fails when the page, or any object under it, carries a retired
	/// object name or a retired accessible name.
	void expect_no_retired_widgets(
		QWidget* const         aPage,
		const std::string_view aState)
	{
		auto objects = aPage->findChildren<QObject*>();
		objects.push_front(aPage);
		for (const auto* const object : std::as_const(objects)) {
			const auto* const widget         = qobject_cast<const QWidget*>(object);
			const auto        accessibleName = widget == nullptr ? QString{ } : widget->accessibleName();
			expect(!names_retired(object->objectName(), retired_accounts_object_names)
					   && !names_retired(accessibleName, retired_accounts_accessible_names),
				   std::string{ "Accounts & hosts carries no retired object while " } + std::string{ aState }
					   + ": " + object->objectName().toStdString() + " / " + accessibleName.toStdString());
		}
	}

	/// \brief One node an assistive reader reaches: its accessible name and,
	/// where it stands for an object, that object's name.
	struct accessible_node
	{
		QString Name;
		QString ObjectName;
	};

	/// \brief Walks an accessibility subtree the way an assistive reader does,
	/// through each interface's children, spending one unit of `aBudget` per node.
	void collect_accessible_nodes(
		QAccessibleInterface* const   aInterface,
		std::vector<accessible_node>& aNodes,
		std::size_t&                  aBudget)
	{
		if (aInterface == nullptr || !aInterface->isValid() || aBudget == 0U) {
			return;
		}
		--aBudget;
		const auto* const object = aInterface->object();
		aNodes.push_back({
			.Name       = aInterface->text(QAccessible::Name),
			.ObjectName = object == nullptr ? QString{ } : object->objectName(),
		});
		for (auto index = 0; index < aInterface->childCount(); ++index) {
			collect_accessible_nodes(aInterface->child(index), aNodes, aBudget);
		}
	}

	/// \brief Fails when the page's accessibility tree names a retired object.
	///
	/// \details The walk must also finish inside its budget, so a tree too
	/// large to walk fails visibly rather than passing on the part it reached.
	void expect_no_retired_accessible_nodes(
		QWidget* const         aPage,
		const std::string_view aState)
	{
		auto nodes  = std::vector<accessible_node>{ };
		auto budget = std::size_t{ 20'000U };
		collect_accessible_nodes(QAccessible::queryAccessibleInterface(aPage), nodes, budget);
		expect(!nodes.empty() && budget > 0U,
			   std::string{ "the accessibility walk of Accounts & hosts reaches the page and finishes while " }
				   + std::string{ aState });
		const auto retired = std::ranges::find_if(nodes, [](const accessible_node& aNode) {
			return names_retired(aNode.Name, retired_accounts_accessible_names)
				|| names_retired(aNode.ObjectName, retired_accounts_object_names);
		});
		expect(retired == nodes.end(),
			   std::string{ "the accessibility tree of Accounts & hosts carries no retired name while " }
				   + std::string{ aState }
				   + (retired == nodes.end()
						  ? std::string{ }
						  : ": " + retired->Name.toStdString() + " / " + retired->ObjectName.toStdString()));
	}

	/// \brief [[BPR-0772]]. Fails unless the page holds exactly one card per
	/// configured server (hostname), in the order the servers were first
	/// configured, every configured host profile is a way in to exactly one
	/// of them, native HTTPS first, and the window hands out those same cards.
	void expect_one_card_per_server(
		const qt::workbench_window&                            aWindow,
		QWidget* const                                         aPage,
		const std::vector<application::github_profile_option>& aConfigured,
		const std::string_view                                 aState)
	{
		auto hostnames = std::vector<std::string>{ };
		for (const auto& option : aConfigured) {
			if (std::ranges::find(hostnames, option.Hostname) == hostnames.end()) {
				hostnames.push_back(option.Hostname);
			}
		}
		const auto cards   = aWindow.server_cards();
		const auto shown   = aPage->findChildren<QWidget*>(QStringLiteral("serverCard"));
		auto       ordered = cards.size() == hostnames.size()
			&& static_cast<std::size_t>(shown.size()) == hostnames.size();
		for (std::size_t index = 0U; ordered && index < hostnames.size(); ++index) {
			const auto* const card = cards[index];
			ordered                = shown.at(static_cast<qsizetype>(index)) == card
				&& card->hostname() == hostnames[index]
				&& card->property("hostname").toString() == QString::fromStdString(hostnames[index])
				&& card->holds(card->profile())
				&& card->property("hostProfile").toULongLong() == card->profile().value();
		}
		expect(ordered,
			   std::string{ "Accounts & hosts holds exactly one card per configured server, in the order the servers were first configured, while " }
				   + std::string{ aState });
		for (const auto& option : aConfigured) {
			const auto        holders = std::ranges::count_if(cards, [&option](const qt::server_card* const aCard) {
				return aCard->holds(option.Profile);
			});
			const auto* const card    = aWindow.server_card_for(option.Profile);
			expect(holders == 1 && card != nullptr && card->hostname() == option.Hostname,
				   std::string{ "host profile " } + std::to_string(option.Profile.value())
					   + " is a way in to exactly one card, its server's, and the window names that card, while "
					   + std::string{ aState });
		}
		for (const auto* const card : cards) {
			const auto& held        = card->options();
			const auto  configured  = std::ranges::count_if(aConfigured, [card](const application::github_profile_option& aOption) {
				return aOption.Hostname == card->hostname();
			});
			const auto  firstCli    = std::ranges::find_if(held, [](const application::github_profile_option& aHeld) {
				return aHeld.Transport == application::github_profile_transport::github_cli;
			});
			const auto  nativeFirst = std::ranges::none_of(firstCli, held.end(), [](const application::github_profile_option& aHeld) {
				return aHeld.Transport == application::github_profile_transport::native_https;
			});
			expect(static_cast<std::size_t>(configured) == held.size() && nativeFirst,
				   std::string{ "the card for " } + card->hostname()
					   + " holds every profile configured for its server and no other, native HTTPS first, while "
					   + std::string{ aState });
		}
	}

	/// \brief [[BPR-0771]] acceptance 1. The Accounts page's widget and
	/// accessibility trees contain none of the retired objects in any
	/// connection state.
	///
	/// \details Each state is driven through the window's own projection entry
	/// points and is first checked for having reached its host's card, so a
	/// sweep of a page that never changed cannot pass for one that did.
	auto test_accounts_page_carries_none_of_the_retired_chrome() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		window.resize(1'280, 820);
		window.show();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		auto* const page = window.settings()->page_widget(qt::settings_page::accounts_and_hosts);
		expect(page != nullptr, "the Settings window has an Accounts and hosts page");
		const auto inspect = [&window, page](
								 const std::string_view      aState,
								 const core::host_profile_id aCurrent,
								 const QString&              aStateWord) {
			QApplication::processEvents();
			const auto* const current = window.server_card_for(aCurrent);
			expect(current != nullptr && current->presentation().Current
					   && current->state_label()->text() == aStateWord,
				   std::string{ "the state under inspection has reached its host's card: " } + std::string{ aState });
			expect_one_card_per_server(window, page, profiles(), aState);
			expect_no_retired_widgets(page, aState);
			expect_no_retired_accessible_nodes(page, aState);
		};

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{ .Status = core::github_status::no_profile },
			},
		});
		inspect("no profile", core::host_profile_id{ 11U }, QStringLiteral("Signed out"));

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status   = core::github_status_delta{ .Status = core::github_status::establishing_session },
				.Identity = core::github_identity_delta{ .Profile = core::host_profile_id{ 12U } },
			},
		});
		inspect("establishing a session", core::host_profile_id{ 12U }, QStringLiteral("Connecting"));

		publish_session_ready(window);
		inspect("session ready", core::host_profile_id{ 12U }, QStringLiteral("Connected"));

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status      = core::github_status_delta{ .Status = core::github_status::session_failed },
				.Diagnostics = { { .Code = core::github_reason_code::authentication_failed } },
			},
		});
		inspect("session failed", core::host_profile_id{ 12U }, QStringLiteral("Could not connect"));

		publish_session_ready(window);
		auto preview = application::public_preview_presentation{ };
		preview.Mode = application::public_preview_mode::signed_out;
		window.publish(preview);
		inspect("reading public pull requests signed out", core::host_profile_id{ 12U }, QStringLiteral("Signed out"));
		expect(window.server_card_for(core::host_profile_id{ 12U })->consequence_label()->text()
				   == QStringLiteral("Reading public pull requests without an account."),
			   "signed out, the reading host's card says it reads public pull requests without an account");
		window.settings()->hide();
		window.hide();
	}

	/// \brief [[BPR-0771]] acceptance 2, the session half. A failed session is
	/// said in its host's card: the short refusal, the last problem and Copy
	/// details, and nowhere else.
	auto test_a_session_failure_is_said_in_its_card() -> void
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
				.Status   = core::github_status_delta{ .Status = core::github_status::establishing_session },
				.Identity = core::github_identity_delta{ .Profile = core::host_profile_id{ 12U } },
			},
		});
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{ .Status = core::github_status::session_failed },
			},
		});
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Diagnostics = { { .Code = core::github_reason_code::authentication_failed } },
			},
		});
		QApplication::processEvents();

		auto* const card       = window.server_card_for(core::host_profile_id{ 12U });
		auto* const other      = window.server_card_for(core::host_profile_id{ 11U });
		const auto  diagnostic = QString::fromStdString(window.github_diagnostic_text());
		expect(card != nullptr && other != nullptr && card->presentation().Current && !diagnostic.isEmpty(),
			   "the failed host's card is the current one, and the failure has a diagnostic");
		expect(card->state_label()->text() == QStringLiteral("Could not connect"),
			   "the card's state is the short refusal");
		expect(card->last_problem_label()->text() == diagnostic && card->details_button()->isVisibleTo(card),
			   "its Details carries the diagnostic as the last problem");
		card->details_button()->click();
		QApplication::processEvents();
		expect(card->details()->isVisibleTo(card) && card->last_problem_label()->isVisibleTo(card)
				   && card->copy_details_button()->isVisibleTo(card),
			   "opening Details shows the last problem with Copy details");
		auto* const clipboard = QApplication::clipboard();
		expect(clipboard != nullptr, "the Qt platform provides a clipboard");
		clipboard->setText(QStringLiteral("clipboard before Copy details"));
		card->copy_details_button()->click();
		expect(clipboard->text() == diagnostic, "Copy details copies exactly the last problem");
		expect(other->state_label()->text() == QStringLiteral("Signed out")
				   && other->last_problem_label()->text().isEmpty()
				   && !other->details_button()->isVisibleTo(other),
			   "the other host's card says nothing of a failure that was not its own");
		expect(window.home_open_notice()->isHidden() && window.home_open_details_button()->isHidden(),
			   "a session failure is not said under the Home bar");
	}

	/// \brief [[BPR-0771]] acceptance 2, the pull-request half. A load that
	/// failed with nothing open is one sentence and Copy details under the Home
	/// bar, and the stack's empty page does not say it again.
	auto test_a_load_refusal_is_said_under_the_home_bar() -> void
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[](application::github_intent) { },
		};
		window.resize(1'280, 820);
		window.show();
		window.activateWindow();
		QApplication::processEvents();
		publish_session_ready(window);
		const auto load = [&window] {
			window.publish(application::github_presentation_update{
				.Transition = application::github_transition_projection{
					.Status      = core::github_status_delta{ .Status = core::github_status::loading_pull_request },
					.PullRequest = core::pull_request_summary_delta{ .Summary = std::nullopt },
				},
			});
		};
		const auto refuse = [&window] {
			window.publish(application::github_presentation_update{
				.Transition = application::github_transition_projection{
					.Status      = core::github_status_delta{ .Status = core::github_status::pull_request_failed },
					.Diagnostics = { { .Code = core::github_reason_code::network_unavailable } },
				},
			});
			QApplication::processEvents();
		};
		load();
		refuse();

		auto* const notice     = window.home_open_notice();
		auto* const details    = window.home_open_details_button();
		auto* const stack      = required_child<QStackedWidget>(window, "githubViewState");
		const auto  diagnostic = QString::fromStdString(window.github_diagnostic_text());
		expect(window.showing_home() && !diagnostic.isEmpty() && notice != nullptr && details != nullptr,
			   "with no pull request open, a failed load leaves the window on Home with a diagnostic");
		expect(!notice->isHidden() && notice->text() == diagnostic,
			   "the refusal is said under the Home bar, as the diagnostic");
		expect(!details->isHidden() && details->objectName() == QStringLiteral("homeOpenCopyDetails")
				   && details->text() == QStringLiteral("Copy details"),
			   "with Copy details beside it");
		expect(stack->isHidden(), "the pull request stack's empty page does not say it again");
		auto* const clipboard = QApplication::clipboard();
		expect(clipboard != nullptr, "the Qt platform provides a clipboard");
		clipboard->setText(QStringLiteral("clipboard before Copy details"));
		details->click();
		expect(clipboard->text().contains(diagnostic)
				   && clipboard->text()
					   == QString::fromStdString(window.github_status_text()) + QStringLiteral("\n") + diagnostic,
			   "Copy details copies the status, then the refusal");

		auto typed = QKeyEvent{ QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("x") };
		QApplication::sendEvent(window.home_open_bar(), &typed);
		QApplication::processEvents();
		expect(window.home_open_bar()->text() == QStringLiteral("x") && notice->isHidden() && details->isHidden(),
			   "typing in the bar dismisses the refusal and its Copy details");
		refuse();
		expect(notice->isHidden() && details->isHidden(),
			   "the same refusal projected again stays dismissed until the next load");
		load();
		refuse();
		expect(!notice->isHidden() && notice->text() == diagnostic && !details->isHidden(),
			   "the next load's refusal is said again");

		// The negative control: the same window, still with nothing open, and
		// a status that is not a refusal.
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{ .Status = core::github_status::pull_request_ready },
			},
		});
		QApplication::processEvents();
		expect(notice->isHidden() && details->isHidden(),
			   "with pull_request_ready there is no notice and no Copy details under the bar");
		window.hide();
	}

	/// \brief [[BPR-1200]]. The central page a window shows, recorded at every
	/// change from the moment it is watched.
	///
	/// \details The course's predicate: a refused first read never passes
	/// through the review surface. Recording every change, rather than looking
	/// once at the end, is what lets a workbench shown and taken away again
	/// before the refusal fail it.
	class central_passage final
	{
	public:
		explicit central_passage(qt::workbench_window& aWindow)
			: Stack{ required_child<QStackedWidget>(aWindow, "workbenchCentralStack") }
		{
			Pages.push_back(Stack->currentWidget()->objectName());
			Watch = QObject::connect(Stack, &QStackedWidget::currentChanged, Stack, [this](int) {
				Pages.push_back(Stack->currentWidget()->objectName());
			});
		}

		~central_passage()
		{
			QObject::disconnect(Watch);
		}

		central_passage(const central_passage&)                    = delete;
		auto operator=(const central_passage&) -> central_passage& = delete;
		central_passage(central_passage&&)                         = delete;
		auto operator=(central_passage&&) -> central_passage&      = delete;

		[[nodiscard]] auto stayed_home() const -> bool
		{
			return std::ranges::all_of(Pages, [](const QString& aPage) {
				return aPage == QStringLiteral("workbenchHome");
			});
		}

		[[nodiscard]] auto pages() const -> const QStringList&
		{
			return Pages;
		}

	private:
		QStackedWidget*         Stack = nullptr;
		QStringList             Pages;
		QMetaObject::Connection Watch;
	};

	[[nodiscard]] auto refused_pull_request(
		const core::refused_connection aConnection,
		const std::string&             aName,
		const core::github_reason_code aReason = core::github_reason_code::permission_denied)
		-> core::pull_request_refusal
	{
		return core::pull_request_refusal{
			.Reason     = aReason,
			.Connection = aConnection,
			.Repository = core::repository_locator{ .Owner = "octo-org", .Name = aName },
			.Number     = core::pull_request_number{ 17U },
		};
	}

	void publish_first_read(qt::workbench_window& aWindow)
	{
		aWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status      = core::github_status_delta{ .Status = core::github_status::loading_pull_request },
				.PullRequest = core::pull_request_summary_delta{ .Summary = std::nullopt },
			},
		});
		// The shipped workbench publishes its drawer commands again while a
		// read runs; each publication decides the page afresh.
		aWindow.publish(application::workbench_projection{ .Commands = core::project_workbench_commands({ }) });
		QApplication::processEvents();
	}

	void publish_refusal(qt::workbench_window& aWindow, const core::pull_request_refusal& aRefusal)
	{
		aWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status      = core::github_status_delta{ .Status = core::github_status::pull_request_failed },
				.Diagnostics = { { .Code = aRefusal.Reason } },
				.Refusal     = aRefusal,
			},
		});
		QApplication::processEvents();
	}

	/// \brief [[BPR-1200]]. Saves the window as it is, beside the build, when asked.
	void save_refusal_course_render(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto prefix = qEnvironmentVariable("BPR_QT_REFUSAL_RENDER_PREFIX");
		if (!prefix.isEmpty()) {
			QApplication::processEvents();
			expect(aWindow.grab().save(QStringLiteral("%1-%2.png").arg(prefix, aName), "PNG"),
				   "the course render is saved in the ignored build tree");
		}
	}

	/// \brief [[BPR-1200]]. The refusal card, rendered: every word and control
	/// inside the window and nothing cut short, saved beside the build.
	void check_refusal_render(qt::workbench_window& aWindow, const QString& aName)
	{
		QApplication::processEvents();
		auto* const card = aWindow.home_refusal_card();
		const auto  area = QRect{ card->mapTo(&aWindow, QPoint{ }), card->size() };
		expect(aWindow.rect().contains(area), "the refusal card lies wholly inside the window");
		for (auto* const label : { card->heading_label(), card->explanation_label() }) {
			expect(label->height() >= label->heightForWidth(label->width()),
				   "the heading and the explanation wrap in full rather than being cut short");
		}
		for (auto* const button : card->offered_buttons()) {
			expect(card->rect().contains(QRect{ button->mapTo(card, QPoint{ }), button->size() })
					   && button->width() >= button->sizeHint().width(),
				   "every offered step fits the card at its full width");
		}
		const auto rendered    = aWindow.grab();
		const auto deviceRatio = aWindow.devicePixelRatioF();
		expect(!rendered.isNull()
				   && rendered.width() == static_cast<int>(std::lround(static_cast<double>(aWindow.width()) * deviceRatio))
				   && rendered.height() == static_cast<int>(std::lround(static_cast<double>(aWindow.height()) * deviceRatio)),
			   "the refusal render has the window's exact device dimensions");
		const auto expectedScaleText = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
		if (!expectedScaleText.isEmpty()) {
			auto       parsed = false;
			const auto scale  = expectedScaleText.toDouble(&parsed);
			expect(parsed && std::abs(deviceRatio - scale) < 0.02, "the refusal renders at the requested scale");
		}
		const auto prefix = qEnvironmentVariable("BPR_QT_REFUSAL_RENDER_PREFIX");
		if (!prefix.isEmpty()) {
			expect(rendered.save(QStringLiteral("%1-%2.png").arg(prefix, aName), "PNG"),
				   "the refusal render is saved in the ignored build tree");
		}
	}

	/// \brief [[BPR-1200]]. A pull request this connection cannot read: Home
	/// throughout, then a card that names it, says why in terms of the
	/// connection, offers what could open it and is announced.
	///
	/// \details The course runs with the shipped workbench's drawer commands
	/// published, which is what used to keep an empty Overview up while the
	/// read ran. Its control presents the workbench before the refusal, the
	/// way a readable pull request's overview does, and the same predicate
	/// must fail it.
	auto test_a_refused_first_read_stays_on_home_and_says_why() -> void
	{
		std::vector<application::github_intent> intents;
		qt::workbench_window                    window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			[&intents](application::github_intent aIntent) { intents.push_back(std::move(aIntent)); },
		};
		auto cliDiscoveries = 0;
		window.set_github_cli_discovery_dispatch([&cliDiscoveries] { ++cliDiscoveries; });
		window.resize(1'280, 820);
		window.show();
		window.activateWindow();
		QApplication::processEvents();
		window.publish(application::workbench_projection{ .Commands = core::project_workbench_commands({ }) });
		publish_session_ready(window, core::host_profile_id{ 11U });
		auto preview = application::public_preview_presentation{ };
		preview.Mode = application::public_preview_mode::signed_out;
		window.publish(preview);
		QApplication::processEvents();
		// With drawer commands published nothing but Home's own request keeps
		// the window there, as it is when a reviewer pastes into the bar.
		if (!window.showing_home()) {
			window.home_action()->trigger();
			QApplication::processEvents();
		}
		expect(window.showing_home() && window.home_landing()->state() == qt::landing_state::sign_in,
			   "the course begins signed out on Home");

		auto* const card    = window.home_refusal_card();
		auto* const notice  = window.home_open_notice();
		auto* const details = window.home_open_details_button();
		expect(card != nullptr && card->isHidden(), "Home composes a refusal card, hidden while nothing is refused");

		// Signed out, a private pull request.
		std::vector<accessible_announcement> announcements;
		const auto                           signedOut = refused_pull_request(core::refused_connection::signed_out, "private-repository");
		{
			const scoped_accessibility_capture capture{ announcements };
			const central_passage              passage{ window };
			publish_first_read(window);
			expect(window.showing_home() && !notice->isHidden()
					   && notice->text() == QStringLiteral("Loading the pull request."),
				   "while the first read runs the window stays on Home and says so under the bar");
			save_refusal_course_render(window, QStringLiteral("loading"));
			publish_refusal(window, signedOut);
			expect(passage.stayed_home(), "a refused first read never passes through the workbench");
		}
		const auto expected = core::word_pull_request_refusal(signedOut, core::pull_request_refusal_context{ .GithubCliOffered = true });
		expect(expected.has_value(), "the core words a signed-out permission refusal");
		expect(window.showing_home() && card->isVisible()
				   && card->heading_label()->text() == QString::fromStdString(expected->Heading)
				   && card->explanation_label()->text() == QString::fromStdString(expected->Explanation),
			   "Home presents the core's words for the refusal, unchanged");
		expect(card->heading_label()->text() == QStringLiteral("Sign in to open octo-org/private-repository#17")
				   && !card->explanation_label()->text().contains(QStringLiteral("browser")),
			   "signed out, the card names the pull request and says to sign in, and speaks of no browser");
		expect(notice->isHidden() && details->isHidden(),
			   "the line under the bar and its Copy details give way to the card");
		const auto top = [&window](const QWidget* const aWidget) {
			return aWidget->mapTo(window.home_page(), QPoint{ }).y();
		};
		expect(top(window.home_open_frame()) < top(card) && top(card) < top(window.home_landing()),
			   "the card sits beside what was refused: under the bar, above the first-launch card");
		expect(card->heading_label()->font().bold() && card->heading_label()->font().pointSizeF() > notice->font().pointSizeF()
				   && card->icon_label()->pixmap().isNull() == false,
			   "the card is set apart from helper text: an icon and a heading larger and heavier than the line");
		auto offered = card->offered_buttons();
		expect(offered.size() == 2U && offered[0]->text() == QStringLiteral("Sign in") && offered[1]->text() == QStringLiteral("Use GitHub CLI")
				   && std::ranges::all_of(offered, [](QPushButton* aButton) { return aButton->isVisible() && !aButton->isDefault() && !aButton->accessibleDescription().isEmpty(); })
				   && !card->action_button(core::pull_request_refusal_action::use_another_account)->isVisible()
				   && card->copy_details_button()->isVisible(),
			   "signed out it offers Sign in, then Use GitHub CLI, then Copy details, with Home's filled control left to the page");
		expect(card->accessibleName() == card->heading_label()->text()
				   && card->accessibleDescription() == card->explanation_label()->text(),
			   "the card's accessible name and description are its heading and explanation");
		const auto spoken = card->heading_label()->text() + QLatin1Char{ ' ' } + card->explanation_label()->text();
		expect(std::ranges::count_if(announcements, [&](const accessible_announcement& aAnnouncement) {
				   return aAnnouncement.Object == window.home_page() && aAnnouncement.Message == spoken
					   && aAnnouncement.Politeness == QAccessible::AnnouncementPoliteness::Polite;
			   }) == 1,
			   "the refusal is announced once, politely, from Home, in the card's words");
		window.home_open_bar()->setFocus(Qt::TabFocusReason);
		QApplication::processEvents();
		auto tab = QKeyEvent{ QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier };
		QApplication::sendEvent(window.home_open_bar(), &tab);
		QApplication::processEvents();
		expect(QApplication::focusWidget() == offered[0], "Tab from the bar reaches the card's first step");
		check_refusal_render(window, QStringLiteral("signed-out"));

		auto* const clipboard = QApplication::clipboard();
		clipboard->setText(QStringLiteral("before"));
		card->copy_details_button()->click();
		expect(clipboard->text().contains(card->heading_label()->text()) && clipboard->text().contains(card->explanation_label()->text()),
			   "Copy details copies the status, then the refusal as the card says it");
		offered[0]->click();
		QApplication::processEvents();
		expect(window.sign_in_dialog() != nullptr && window.sign_in_dialog()->isVisible(),
			   "Sign in opens the shared sign-in dialog");
		window.sign_in_dialog()->reject();
		QApplication::processEvents();
		offered[1]->click();
		QApplication::processEvents();
		expect(cliDiscoveries == 1 && window.sign_in_dialog()->isVisible(),
			   "Use GitHub CLI starts discovery in the shared sign-in dialog");
		window.sign_in_dialog()->reject();
		QApplication::processEvents();

		auto typed = QKeyEvent{ QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("x") };
		QApplication::sendEvent(window.home_open_bar(), &typed);
		QApplication::processEvents();
		expect(card->isHidden(), "rewriting the link dismisses the card");
		publish_refusal(window, signedOut);
		expect(card->isHidden(), "the same refusal projected again stays dismissed until the next read");
		window.home_open_bar()->clear();

		// Signed in, an account the repository does not admit.
		preview.Mode = application::public_preview_mode::signed_in;
		window.publish(preview);
		publish_session_ready(window, core::host_profile_id{ 12U });
		const auto account = refused_pull_request(core::refused_connection::account, "inaccessible-repository");
		// The link is pasted into the bar, so the read starts on Home.
		if (!window.showing_home()) {
			window.home_action()->trigger();
			QApplication::processEvents();
		}
		{
			const central_passage passage{ window };
			publish_first_read(window);
			publish_refusal(window, account);
			expect(passage.stayed_home(), "an account's refused first read never passes through the workbench either");
		}
		offered = card->offered_buttons();
		expect(card->isVisible() && card->heading_label()->text() == QStringLiteral("octo-reviewer cannot open octo-org/inaccessible-repository#17")
				   && card->explanation_label()->text().contains(QStringLiteral("this app's sign-in"))
				   && !card->explanation_label()->text().contains(QStringLiteral("not signed in")),
			   "signed in, the card names the account and says why in terms of the account");
		expect(offered.size() == 2U && offered[0]->text() == QStringLiteral("Use another account")
				   && offered[1]->text() == QStringLiteral("Use GitHub CLI")
				   && !card->action_button(core::pull_request_refusal_action::sign_in)->isVisible(),
			   "signed in it offers another account, then GitHub CLI");
		check_refusal_render(window, QStringLiteral("account"));
		offered[0]->click();
		QApplication::processEvents();
		expect(window.sign_in_dialog()->isVisible(), "Use another account opens the shared sign-in dialog");
		window.sign_in_dialog()->reject();
		QApplication::processEvents();

		// A refusal another connection would not change keeps its line.
		publish_first_read(window);
		publish_refusal(window, refused_pull_request(core::refused_connection::account, "review-repository", core::github_reason_code::network_unavailable));
		expect(card->isHidden() && !notice->isHidden() && notice->text() == QString::fromStdString(window.github_diagnostic_text())
				   && !details->isHidden(),
			   "an unreachable host is still said in the line, with its Copy details");

		// The control: the workbench presented before the refusal, as a
		// readable pull request's overview presents it. The course's
		// predicate must fail it, or it could not tell the defect apart.
		{
			const central_passage passage{ window };
			publish_first_read(window);
			expect(window.showing_home(), "the control's read starts on Home too");
			window.publish(application::github_presentation_update{
				.LoadingOverview = application::github_loading_overview_projection{
					.Title = "A readable pull request", .Body = "", .AuthorLogin = "octocat", .BaseRef = "main", .HeadRef = "topic", .ReportedFiles = 1U, .ReportedCommits = 1U },
			});
			QApplication::processEvents();
			expect(!window.showing_home(), "an overview shown by the read presents the workbench");
			publish_refusal(window, account);
			expect(window.showing_home() && card->isVisible(), "the control's refusal still returns to Home with its card");
			expect(!passage.stayed_home() && passage.pages().contains(QStringLiteral("workbenchReviewSurface")),
				   "the control, which presented the workbench before the refusal, fails the course's predicate");
		}
		window.hide();
		expect(intents.empty(), "nothing in the course dispatched a GitHub intent");
	}

	/// \brief [[BPR-0771]]. The picker's job, now a card's. Connect on a card
	/// that is not the chosen one chooses its host, rebinds the device sign-in
	/// and dispatches exactly one activation for it through the existing intent.
	auto test_a_card_connect_chooses_and_connects_its_host() -> void
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
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = enabled_commands(),
			},
		});
		QApplication::processEvents();
		auto* const chosen  = window.server_card_for(core::host_profile_id{ 12U });
		auto* const current = window.server_card_for(core::host_profile_id{ 11U });
		expect(chosen != nullptr && current != nullptr && !chosen->presentation().Current
				   && current->presentation().Current
				   && window.session_panel()->selected_profile() == core::host_profile_id{ 11U }
				   && window.device_authorization()->bound_profile() == core::host_profile_id{ 11U },
			   "before any gesture the first configured host is the chosen one");
		expect(chosen->connect_button()->isVisibleTo(chosen) && intents.empty(),
			   "a host that is not the chosen one offers Connect on its card, and nothing has been dispatched");
		chosen->connect_button()->click();
		QApplication::processEvents();
		const auto* const activation = intents.size() == 1U
			? std::get_if<core::activate_host_profile_intent>(&intents.front())
			: nullptr;
		expect(activation != nullptr && activation->Profile == core::host_profile_id{ 12U }
				   && activation->Mode == core::github_access_mode::authenticated,
			   "Connect dispatches exactly one activation, for that card's host");
		expect(window.session_panel()->selected_profile() == core::host_profile_id{ 12U }
				   && window.device_authorization()->bound_profile() == core::host_profile_id{ 12U },
			   "and chooses that host for the next Connect and sign-in");
		expect(chosen->presentation().Current && !chosen->connect_button()->isVisibleTo(chosen)
				   && current->connect_button()->isVisibleTo(current),
			   "the chosen card no longer offers Connect, and the other card now does");
	}

	/// \brief [[BPR-0772]]. The shipped composition's shape: each server is
	/// configured with native HTTPS and with GitHub CLI, labelled the way the
	/// composition labels them.
	[[nodiscard]] auto grouped_profiles()
		-> std::vector<application::github_profile_option>
	{
		using product   = application::github_profile_product;
		using transport = application::github_profile_transport;
		return {
			{
				.Profile   = core::host_profile_id{ 21U },
				.Label     = "GitHub.com — Native HTTPS",
				.Hostname  = "github.com",
				.Product   = product::github_com,
				.Transport = transport::native_https,
			},
			{
				.Profile   = core::host_profile_id{ 22U },
				.Label     = "GitHub.com — GitHub CLI (uses your gh sign-in)",
				.Hostname  = "github.com",
				.Product   = product::github_com,
				.Transport = transport::github_cli,
			},
			{
				.Profile   = core::host_profile_id{ 23U },
				.Label     = "GHES 3.18 — Native HTTPS",
				.Hostname  = "ghe.example.invalid",
				.Product   = product::github_enterprise_server,
				.Transport = transport::native_https,
			},
			{
				.Profile   = core::host_profile_id{ 24U },
				.Label     = "GHES 3.18 — GitHub CLI (unavailable)",
				.Hostname  = "ghe.example.invalid",
				.Product   = product::github_enterprise_server,
				.Transport = transport::github_cli,
			},
		};
	}

	/// \brief The host profiles a card holds, in the order it holds them.
	[[nodiscard]] auto held_profiles(const qt::server_card& aCard)
		-> std::vector<core::host_profile_id>
	{
		auto held = std::vector<core::host_profile_id>{ };
		for (const auto& option : aCard.options()) {
			held.push_back(option.Profile);
		}
		return held;
	}

	/// \brief [[BPR-0772]] acceptance 1 and 3. One card per configured server
	/// holds every way in to that server, native HTTPS first. The card's
	/// Connect and its GitHub CLI offer choose, then connect, exactly the host
	/// profile they name through the existing intents, and no widget gains an
	/// effect of its own.
	auto test_one_card_per_server_holds_its_ways_in() -> void
	{
		// Each GitHub intent, with the host chosen and the profile sign-in was
		// bound to at the instant it left, so that choosing before connecting
		// is observed rather than inferred.
		struct observed_intent
		{
			application::github_intent           Intent;
			std::optional<core::host_profile_id> Selected;
			core::host_profile_id                Bound;
		};
		auto                 observed     = std::vector<observed_intent>{ };
		auto                 otherEffects = std::size_t{ 0U };
		auto*                observing    = static_cast<qt::workbench_window*>(nullptr);
		qt::workbench_window window{
			[&otherEffects](core::workbench_interaction_intent) { ++otherEffects; },
			[&otherEffects](core::navigate_diff_change_intent) { ++otherEffects; },
			[&otherEffects](core::workbench_drawer, std::uint32_t) { ++otherEffects; },
			grouped_profiles(),
			[&observed, &observing](application::github_intent aIntent) {
				observed.push_back({
					.Intent   = std::move(aIntent),
					.Selected = observing == nullptr ? std::optional<core::host_profile_id>{ }
													 : observing->session_panel()->selected_profile(),
					.Bound    = observing == nullptr ? core::host_profile_id{ }
													 : observing->device_authorization()->bound_profile(),
				});
			},
		};
		observing = &window;
		window.set_device_authorization_dispatch([&otherEffects](application::device_authorization_gesture) { ++otherEffects; });
		window.set_public_preview_dispatch([&otherEffects](application::public_preview_gesture) { ++otherEffects; });
		window.set_setup_destination_dispatch([&otherEffects](application::setup_destination_intent) { ++otherEffects; });
		window.set_saved_server_dispatch([&otherEffects](application::saved_server_intent) { ++otherEffects; });
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = enabled_commands(),
			},
		});
		QApplication::processEvents();

		auto* const page = window.settings()->page_widget(qt::settings_page::accounts_and_hosts);
		expect(page != nullptr, "the Settings window has an Accounts and hosts page");
		expect_one_card_per_server(window, page, grouped_profiles(), "two servers are configured with two profiles each");
		const auto  cards      = window.server_cards();
		auto* const github     = window.server_card_for(core::host_profile_id{ 21U });
		auto* const enterprise = window.server_card_for(core::host_profile_id{ 23U });
		expect(cards.size() == 2U && github != nullptr && enterprise != nullptr
				   && cards.front() == github && cards.back() == enterprise
				   && window.server_card_for(core::host_profile_id{ 22U }) == github
				   && window.server_card_for(core::host_profile_id{ 24U }) == enterprise
				   && !github->holds(core::host_profile_id{ 23U }) && !enterprise->holds(core::host_profile_id{ 22U }),
			   "GitHub.com's two profiles are one card and the enterprise server's two are another");
		expect(held_profiles(*github) == std::vector{ core::host_profile_id{ 21U }, core::host_profile_id{ 22U } }
				   && held_profiles(*enterprise) == std::vector{ core::host_profile_id{ 23U }, core::host_profile_id{ 24U } }
				   && github->native_option() != nullptr && github->native_option()->Profile == core::host_profile_id{ 21U }
				   && github->cli_option() != nullptr && github->cli_option()->Profile == core::host_profile_id{ 22U }
				   && enterprise->native_option() != nullptr && enterprise->native_option()->Profile == core::host_profile_id{ 23U }
				   && enterprise->cli_option() != nullptr && enterprise->cli_option()->Profile == core::host_profile_id{ 24U },
			   "each card holds its server's native HTTPS profile first and its GitHub CLI profile second");

		// Configured the other way round, GitHub CLI before native HTTPS and the
		// enterprise server first, the servers keep the order they were first
		// configured in and each card still holds native HTTPS first.
		{
			auto reversed = grouped_profiles();
			std::ranges::reverse(reversed);
			qt::workbench_window reversedWindow{
				qt::workbench_intent_dispatch{ },
				qt::workbench_change_dispatch{ },
				qt::workbench_drawer_width_dispatch{ },
				reversed,
				[](application::github_intent) { },
			};
			auto* const reversedPage = reversedWindow.settings()->page_widget(qt::settings_page::accounts_and_hosts);
			expect(reversedPage != nullptr, "the reversed window has an Accounts and hosts page");
			expect_one_card_per_server(reversedWindow, reversedPage, reversed, "the same profiles are configured in reverse");
			const auto reversedCards = reversedWindow.server_cards();
			expect(reversedCards.size() == 2U && reversedCards.front()->hostname() == "ghe.example.invalid"
					   && held_profiles(*reversedCards.front()) == std::vector{ core::host_profile_id{ 23U }, core::host_profile_id{ 24U } }
					   && held_profiles(*reversedCards.back()) == std::vector{ core::host_profile_id{ 21U }, core::host_profile_id{ 22U } },
				   "configured GitHub CLI first, each card still holds native HTTPS first, and the server configured first leads");
		}

		expect(github->presentation().Current && !enterprise->presentation().Current
				   && github->profile() == core::host_profile_id{ 21U }
				   && enterprise->profile() == core::host_profile_id{ 23U }
				   && github->property("hostProfile").toULongLong() == 21U
				   && enterprise->property("hostProfile").toULongLong() == 23U,
			   "before any gesture GitHub.com is chosen, and each card speaks for its native HTTPS profile");
		expect(github->kind_label()->text() == QStringLiteral("GitHub.com, native HTTPS")
				   && enterprise->kind_label()->text() == QStringLiteral("GitHub Enterprise Server, native HTTPS"),
			   "each header's kind line names the product and the transport of the profile its card speaks for");
		expect(github->cli_offer_button() != nullptr && enterprise->cli_offer_button() != nullptr
				   && github->cli_offer_button()->text() == QStringLiteral("GitHub CLI (uses your gh sign-in)")
				   && enterprise->cli_offer_button()->text() == QStringLiteral("GitHub CLI (unavailable)")
				   && github->cli_offer_button()->isVisibleTo(github)
				   && enterprise->cli_offer_button()->isVisibleTo(enterprise),
			   "each server offers GitHub CLI in its own composition's words");
		expect(window.findChildren<QLabel*>(QStringLiteral("githubCliTrustWarning")).size() == 2
				   && std::ranges::all_of(cards, [](const qt::server_card* const aCard) {
						  auto* const warning = aCard->cli_trust_warning();
						  return warning != nullptr && aCard->isAncestorOf(warning)
							  && aCard->findChildren<QLabel*>(QStringLiteral("githubCliTrustWarning")) == QList<QLabel*>{ warning }
						  && warning->isVisibleTo(aCard)
							  && warning->text().contains(QStringLiteral("this server"))
							  && !warning->text().contains(QString::fromStdString(aCard->hostname()), Qt::CaseInsensitive);
					  }),
			   "exactly one trust warning per server, in that server's card, saying this server rather than its host");
		expect(github->sign_in_button()->isVisibleTo(github) && github->sign_in_button()->isDefault()
				   && !github->connect_button()->isVisibleTo(github)
				   && enterprise->connect_button()->isVisibleTo(enterprise)
				   && !enterprise->sign_in_button()->isVisibleTo(enterprise),
			   "the chosen server offers Sign in, filled, and the other offers Connect");

		observed.clear();
		otherEffects = 0U;
		// [[BPR-0915]]. A card's Connect and its GitHub CLI offer open the
		// shared sign-in dialog, which holds the same cards in their setup
		// presentation. Closing it is the reviewer's Cancel, returning the
		// cards to Settings.
		auto       cancels           = std::size_t{ 0U };
		const auto closeSharedSignIn = [&window] {
			window.sign_in_dialog()->reject();
			QApplication::processEvents();
		};
		window.set_sign_in_cancel_dispatch([&cancels] { ++cancels; });
		const auto inSharedSignIn = [&window](const qt::server_card* const aCard) {
			return window.sign_in_dialog() != nullptr && window.sign_in_dialog()->isVisible()
				&& window.sign_in_dialog()->isAncestorOf(aCard) && aCard->isVisibleTo(window.sign_in_dialog())
				&& !window.settings()->isVisible();
		};
		// The intent at `aIndex` activates exactly this host profile with an
		// account, and left after that profile was chosen and sign-in bound to it.
		const auto activated = [&observed](const std::size_t aIndex, const core::host_profile_id aProfile) {
			const auto* const activation = aIndex < observed.size()
				? std::get_if<core::activate_host_profile_intent>(&observed[aIndex].Intent)
				: nullptr;
			return activation != nullptr && activation->Profile == aProfile
				&& activation->Mode == core::github_access_mode::authenticated
				&& observed[aIndex].Selected == aProfile && observed[aIndex].Bound == aProfile;
		};

		enterprise->connect_button()->click();
		QApplication::processEvents();
		expect(observed.size() == 1U && activated(0U, core::host_profile_id{ 23U }),
			   "Connect on the card not chosen emits exactly one activation, for its native HTTPS profile, once that profile is chosen and sign-in is bound to it");
		expect(inSharedSignIn(enterprise) && !github->isVisibleTo(window.sign_in_dialog()),
			   "that Connect opens the shared sign-in dialog for the enterprise server alone");
		expect(enterprise->presentation().Current && enterprise->profile() == core::host_profile_id{ 23U }
				   && !enterprise->connect_button()->isVisibleTo(enterprise)
				   && enterprise->sign_in_button()->isVisibleTo(enterprise)
				   && !enterprise->cli_offer_button()->isVisibleTo(enterprise)
				   && !github->presentation().Current && github->profile() == core::host_profile_id{ 21U }
				   && github->connect_button()->isVisibleTo(github),
			   "the enterprise server is chosen and signs in natively, its card offers no GitHub CLI inside native sign-in, and GitHub.com offers Connect for its native HTTPS profile");
		closeSharedSignIn();
		expect(cancels == 1U && page->isAncestorOf(enterprise) && !window.sign_in_dialog()->isVisible()
				   && enterprise->presentation().Current && enterprise->cli_offer_button()->isVisibleTo(enterprise)
				   && enterprise->cli_trust_warning()->isVisibleTo(enterprise),
			   "closing sign-in cancels it once and returns the cards to Settings, where the chosen server offers GitHub CLI beside its trust warning");

		enterprise->cli_offer_button()->click();
		QApplication::processEvents();
		expect(observed.size() == 2U && activated(1U, core::host_profile_id{ 24U }),
			   "the chosen server's GitHub CLI offer emits exactly one activation, for its GitHub CLI profile");
		const auto speaksForCli = [enterprise] {
			return enterprise->presentation().Current && enterprise->profile() == core::host_profile_id{ 24U }
			&& enterprise->property("hostProfile").toULongLong() == 24U
				&& enterprise->kind_label()->text() == QStringLiteral("GitHub Enterprise Server, GitHub CLI")
				&& !enterprise->cli_offer_button()->isVisibleTo(enterprise)
				&& enterprise->connect_button()->isVisibleTo(enterprise)
				&& !enterprise->sign_in_button()->isDefault();
		};
		expect(inSharedSignIn(enterprise) && speaksForCli()
				   && enterprise->login_label()->text() == enterprise->cli_offer_button()->text()
				   && !enterprise->sign_in_button()->isVisibleTo(enterprise),
			   "the offer opens GitHub CLI sign-in, where the card speaks for its GitHub CLI profile and Connect is its only way in");
		closeSharedSignIn();
		expect(cancels == 2U && page->isAncestorOf(enterprise) && speaksForCli()
				   && enterprise->sign_in_button()->isVisibleTo(enterprise)
				   && enterprise->cli_trust_warning()->isVisibleTo(enterprise),
			   "with GitHub CLI chosen the card speaks for it: the kind line says so, Connect is its retry, the warning stays and Sign in is not filled");

		github->cli_offer_button()->click();
		QApplication::processEvents();
		expect(observed.size() == 3U && activated(2U, core::host_profile_id{ 22U }),
			   "the GitHub CLI offer on the card not chosen emits exactly one activation, for its own GitHub CLI profile");
		expect(inSharedSignIn(github) && github->presentation().Current && github->profile() == core::host_profile_id{ 22U }
				   && github->kind_label()->text() == QStringLiteral("GitHub.com, GitHub CLI")
				   && !enterprise->presentation().Current && enterprise->profile() == core::host_profile_id{ 23U }
				   && enterprise->kind_label()->text() == QStringLiteral("GitHub Enterprise Server, native HTTPS"),
			   "GitHub.com now speaks for GitHub CLI, and the enterprise card, no longer chosen, for its native HTTPS profile again");
		closeSharedSignIn();
		expect(cancels == 3U && otherEffects == 0U,
			   "no card gesture leaves through any sink but the GitHub intents and sign-in's own Cancel: no workbench intent, device sign-in, public preview, setup destination or saved server");

		// A profile added from setup joins its server's card when that server
		// is already configured; a new server appends one card.
		auto       configured = grouped_profiles();
		const auto add        = [&window, &configured](application::github_profile_option aOption) {
			configured.push_back(aOption);
			window.add_setup_profile(std::move(aOption));
			QApplication::processEvents();
		};
		add({
			.Profile   = core::host_profile_id{ 25U },
			.Label     = "GitHub.com — Native HTTPS (second account)",
			.Hostname  = "github.com",
			.Product   = application::github_profile_product::github_com,
			.Transport = application::github_profile_transport::native_https,
		});
		expect(window.server_cards().size() == 2U && window.server_card_for(core::host_profile_id{ 25U }) == github
				   && held_profiles(*github) == std::vector{ core::host_profile_id{ 21U }, core::host_profile_id{ 25U }, core::host_profile_id{ 22U } }
				   && window.session_panel()->profile_option(core::host_profile_id{ 25U }) != nullptr,
			   "a setup profile on a server already configured joins that server's card, native HTTPS still first, and adds no card");
		add({
			.Profile   = core::host_profile_id{ 26U },
			.Label     = "Review GHES — Native HTTPS",
			.Hostname  = "review.example.invalid",
			.Product   = application::github_profile_product::github_enterprise_server,
			.Transport = application::github_profile_transport::native_https,
		});
		const auto  grown  = window.server_cards();
		auto* const review = window.server_card_for(core::host_profile_id{ 26U });
		expect(grown.size() == 3U && review != nullptr && grown.back() == review
				   && review->hostname() == "review.example.invalid"
				   && review->property("hostname").toString() == QStringLiteral("review.example.invalid")
				   && review->cli_offer_button() == nullptr && review->cli_trust_warning() == nullptr,
			   "a setup profile on a new server appends one card for it, with no GitHub CLI way in to offer or warn about");
		// RFC-BPR-0015's offer carries the composition's words only when they
		// name GitHub CLI after the product; otherwise it says GitHub CLI.
		add({
			.Profile   = core::host_profile_id{ 27U },
			.Label     = "Enterprise through gh",
			.Hostname  = "cli.example.invalid",
			.Product   = application::github_profile_product::github_enterprise_server,
			.Transport = application::github_profile_transport::github_cli,
		});
		add({
			.Profile   = core::host_profile_id{ 28U },
			.Label     = "Build farm — gh sign-in",
			.Hostname  = "build.example.invalid",
			.Product   = application::github_profile_product::github_enterprise_server,
			.Transport = application::github_profile_transport::github_cli,
		});
		auto* const bare    = window.server_card_for(core::host_profile_id{ 27U });
		auto* const unnamed = window.server_card_for(core::host_profile_id{ 28U });
		expect(window.server_cards().size() == 5U && bare != nullptr && unnamed != nullptr
				   && bare->cli_offer_button() != nullptr && unnamed->cli_offer_button() != nullptr
				   && bare->cli_offer_button()->text() == QStringLiteral("GitHub CLI")
				   && unnamed->cli_offer_button()->text() == QStringLiteral("GitHub CLI"),
			   "a GitHub CLI label with no separator, or naming something else after it, is offered as GitHub CLI");
		expect_one_card_per_server(window, page, configured, "setup has added a profile to one server and three new servers");
		expect(observed.size() == 3U && otherEffects == 0U, "adding setup profiles dispatches nothing");
	}

	/// \brief One text a reviewer can read on a page, and the widget showing it.
	struct shown_text
	{
		const QWidget* Widget = nullptr;
		QString        Text;
	};

	/// \brief [[BPR-0772]]. Every text a page shows now: the text of each label
	/// and button, and the current text of each field and combo box, whose
	/// widget is visible to the page. Nothing is left out for being outside a
	/// card.
	[[nodiscard]] auto shown_texts(QWidget* const aPage) -> std::vector<shown_text>
	{
		auto texts = std::vector<shown_text>{ };
		for (const auto* const widget : aPage->findChildren<QWidget*>()) {
			if (!widget->isVisibleTo(aPage)) {
				continue;
			}
			if (const auto* const label = qobject_cast<const QLabel*>(widget); label != nullptr) {
				texts.push_back({ .Widget = widget, .Text = label->text() });
			} else if (const auto* const button = qobject_cast<const QAbstractButton*>(widget); button != nullptr) {
				texts.push_back({ .Widget = widget, .Text = button->text() });
			} else if (const auto* const field = qobject_cast<const QLineEdit*>(widget); field != nullptr) {
				texts.push_back({ .Widget = widget, .Text = field->text() });
			} else if (const auto* const combo = qobject_cast<const QComboBox*>(widget); combo != nullptr) {
				texts.push_back({ .Widget = widget, .Text = combo->currentText() });
			}
		}
		return texts;
	}

	/// \brief How a fact is recognised in a text.
	enum class fact_kind : std::uint8_t {
		words,
		/// A hostname followed by "/" is part of an address, such as the device
		/// flow's github.com/login/device, and does not name the host.
		host,
	};

	/// \brief How many times one text names a fact, case-sensitively.
	[[nodiscard]] auto mentions(const QString& aText, const QString& aFact, const fact_kind aKind)
		-> std::size_t
	{
		if (aFact.isEmpty()) {
			return 0U;
		}
		auto count = std::size_t{ 0U };
		for (auto at = aText.indexOf(aFact); at >= 0; at = aText.indexOf(aFact, at + aFact.size())) {
			const auto end = at + aFact.size();
			if (aKind == fact_kind::host && end < aText.size() && aText.at(end) == QLatin1Char('/')) {
				continue;
			}
			++count;
		}
		return count;
	}

	/// \brief How many times the texts of the widgets `aWhere` admits name a fact.
	template <typename Where>
	[[nodiscard]] auto mentions(
		const std::vector<shown_text>& aTexts,
		const QString&                 aFact,
		const fact_kind                aKind,
		const Where&                   aWhere) -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (const auto& shown : aTexts) {
			if (aWhere(shown.Widget)) {
				count += mentions(shown.Text, aFact, aKind);
			}
		}
		return count;
	}

	/// \brief Fails unless the widgets `aWhere` admits name a fact between
	/// `aLeast` and `aMost` times, and names every text that does.
	template <typename Where>
	void expect_mentions(
		const std::vector<shown_text>& aTexts,
		const QString&                 aFact,
		const fact_kind                aKind,
		const Where&                   aWhere,
		const std::size_t              aLeast,
		const std::size_t              aMost,
		const std::string&             aClaim)
	{
		const auto count = mentions(aTexts, aFact, aKind, aWhere);
		if (count >= aLeast && count <= aMost) {
			return;
		}
		auto named = std::string{ };
		for (const auto& shown : aTexts) {
			if (aWhere(shown.Widget) && mentions(shown.Text, aFact, aKind) > 0U) {
				named += " [" + shown.Widget->objectName().toStdString() + ": \"" + shown.Text.toStdString() + "\"]";
			}
		}
		expect(false,
			   aClaim + " (\"" + aFact.toStdString() + "\" is named " + std::to_string(count) + " times:"
				   + (named.empty() ? std::string{ " nowhere" } : named) + ")");
	}

	/// \brief [[BPR-0772]]. One state of the grouped Accounts page, and what
	/// each of its two cards should say in it.
	struct accounts_page_state
	{
		std::string_view Name;
		/// The host profile the session is on, or the one chosen while none is.
		core::host_profile_id Current;
		/// The profile GitHub.com's card speaks for, and its state in one word.
		core::host_profile_id GithubPresents;
		QString               GithubState;
		/// The profile the enterprise server's card speaks for, and its state.
		core::host_profile_id EnterprisePresents;
		QString               EnterpriseState;
		/// Whether the chosen card shows the fixture's signed-in account.
		bool SignedIn = false;
		/// Whether the chosen card's Sign in is the page's one filled control.
		/// A live code is the shared sign-in dialog's, never the page's
		/// ([[BPR-0915]]); `expect_signing_in_dialog` says what it shows.
		bool Filled = false;
	};

	/// \brief [[BPR-0772]], RFC-BPR-0032 A4. Fails unless each card on the page
	/// names its host, its account, the transport it speaks for and its state
	/// once, the page names each host and the account once, nothing outside the
	/// cards names any of them, and at most one control on the page is filled.
	void expect_each_fact_once(
		const qt::workbench_window& aWindow,
		QWidget* const              aPage,
		const accounts_page_state&  aState,
		const std::string_view      aDisclosure)
	{
		const auto during = [&aState, aDisclosure](const std::string& aClaim) {
			return aClaim + " while " + std::string{ aState.Name } + ", " + std::string{ aDisclosure };
		};
		const auto texts = shown_texts(aPage);
		const auto cards = aWindow.server_cards();
		expect(cards.size() == 2U && cards.front()->holds(core::host_profile_id{ 21U })
				   && cards.back()->holds(core::host_profile_id{ 23U }),
			   during("the page holds GitHub.com's card, then the enterprise server's"));
		const auto       login  = QString::fromStdString(session_projection().Login);
		const auto       native = QStringLiteral("native HTTPS");
		const auto       cli    = QStringLiteral("GitHub CLI");
		const std::array stateWords{
			QStringLiteral("Signed out"),
			QStringLiteral("Connecting"),
			QStringLiteral("Signing in"),
			QStringLiteral("Connected"),
			QStringLiteral("Could not connect"),
		};
		const auto anywhere = [](const QWidget* const) { return true; };
		const auto outside  = [&cards](const QWidget* const aWidget) {
			return std::ranges::none_of(cards, [aWidget](const qt::server_card* const aCard) {
				return aCard->isAncestorOf(aWidget);
			});
		};

		// Outside the cards: the page's title and sentence, and the enterprise
		// servers editor until BPR-0774 replaces it. The device sign-in is a
		// card's account row since BPR-0773, so its address and code are
		// counted as that card's. Nothing outside may say a fact again.
		for (const auto* const card : cards) {
			expect_mentions(texts, QString::fromStdString(card->hostname()), fact_kind::host, outside, 0U, 0U,
							during("nothing outside the server cards names " + card->hostname()));
		}
		expect_mentions(texts, login, fact_kind::words, outside, 0U, 0U,
						during("nothing outside the server cards names the account"));
		for (const auto& transport : { native, cli }) {
			expect_mentions(texts, transport, fact_kind::words, outside, 0U, 0U,
							during("nothing outside the server cards names a transport"));
		}
		for (const auto& word : stateWords) {
			expect_mentions(texts, word, fact_kind::words, outside, 0U, 0U,
							during("nothing outside the server cards names a connection state"));
		}

		const auto       fixture = grouped_profiles();
		const std::array presented{ aState.GithubPresents, aState.EnterprisePresents };
		const std::array words{ aState.GithubState, aState.EnterpriseState };
		for (std::size_t index = 0U; index < cards.size(); ++index) {
			const auto* const card   = cards[index];
			const auto        name   = "the card for " + card->hostname();
			const auto        within = [card](const QWidget* const aWidget) { return card->isAncestorOf(aWidget); };
			const auto        option = std::ranges::find(fixture, presented[index], &application::github_profile_option::Profile);
			expect(option != fixture.end() && card->profile() == presented[index],
				   during(name + " speaks for the profile the state chose for it"));
			const auto speaks = option->Transport == application::github_profile_transport::github_cli ? cli : native;
			const auto other  = speaks == cli ? native : cli;
			expect_mentions(texts, QString::fromStdString(card->hostname()), fact_kind::host, within, 1U, 1U,
							during(name + " names its host once"));
			const auto logins = aState.SignedIn && card->holds(aState.Current) ? 1U : 0U;
			expect_mentions(texts, login, fact_kind::words, within, logins, logins,
							during(name + (logins == 1U ? " names its signed-in account once" : " names no account")));
			expect_mentions(texts, speaks, fact_kind::words, within, 1U, 1U,
							during(name + " names the transport it speaks for once"));
			expect_mentions(
				texts, speaks, fact_kind::words, [card](const QWidget* const aWidget) { return aWidget == card->kind_label(); }, 1U, 1U,
				during(name + " names the transport it speaks for in its kind line"));
			expect_mentions(texts, other, fact_kind::words, within, 0U, 1U,
							during(name + " names the other transport at most once"));
			expect_mentions(
				texts, cli, fact_kind::words,
				[card](const QWidget* const aWidget) {
					return card->isAncestorOf(aWidget) && aWidget != card->kind_label() && aWidget != card->cli_offer_button();
				},
				0U, 0U, during("only the kind line and the GitHub CLI offer name GitHub CLI on " + name));
			expect_mentions(
				texts, native, fact_kind::words,
				[card](const QWidget* const aWidget) { return card->isAncestorOf(aWidget) && aWidget != card->kind_label(); },
				0U, 0U, during("only the kind line names native HTTPS on " + name));
			for (const auto& word : stateWords) {
				const auto expected = word == words[index] ? 1U : 0U;
				expect_mentions(texts, word, fact_kind::words, within, expected, expected,
								during(name + (expected == 1U ? " says its own state once" : " says no other state")));
			}
			expect_mentions(
				texts, words[index], fact_kind::words, [card](const QWidget* const aWidget) { return aWidget == card->state_label(); }, 1U, 1U,
				during(name + " says its state in its header"));
		}

		for (const auto* const card : cards) {
			expect_mentions(texts, QString::fromStdString(card->hostname()), fact_kind::host, anywhere, 1U, 1U,
							during("the page names " + card->hostname() + " once"));
		}
		const auto logins = aState.SignedIn ? 1U : 0U;
		expect_mentions(texts, login, fact_kind::words, anywhere, logins, logins,
						during(logins == 1U ? "the page names the signed-in account once" : "the page names no account"));

		auto filled = std::vector<const QPushButton*>{ };
		for (const auto* const button : aPage->findChildren<QPushButton*>()) {
			if (button->isVisibleTo(aPage) && button->isDefault()) {
				filled.push_back(button);
			}
		}
		// The one filled control is the page's job: the chosen card's Sign in
		// while it is signed out with native HTTPS presented.
		const auto* const chosen   = aWindow.server_card_for(aState.Current);
		const auto* const expected = aState.Filled && chosen != nullptr
			? static_cast<const QPushButton*>(chosen->sign_in_button())
			: nullptr;
		expect(filled.size() <= 1U, during("at most one control on the page is filled"));
		expect(expected == nullptr
				   ? filled.empty()
				   : filled.size() == 1U && filled.front() == expected && chosen != nullptr && chosen->isAncestorOf(expected),
			   during(aState.Filled ? "the chosen card's Sign in is the page's one filled control"
									: "no control on the page is filled"));
	}

	/// \brief [[BPR-0915]]. A live code is shown in the shared sign-in dialog,
	/// which holds the Accounts page's cards while Settings is closed. Fails
	/// unless the dialog shows only the signing-in server's card, which names
	/// its host, the transport it speaks for and Signing in once each, the
	/// other server appears nowhere in it, and its one filled control is the
	/// code's Open browser.
	void expect_signing_in_dialog(
		const qt::workbench_window& aWindow,
		const qt::server_card&      aSigning,
		const qt::server_card&      aOther,
		const QString&              aTransport,
		const std::string_view      aState)
	{
		const auto during = [aState](const std::string& aClaim) {
			return aClaim + " while " + std::string{ aState };
		};
		auto* const dialog = aWindow.sign_in_dialog();
		expect(dialog != nullptr && dialog->isVisible() && !aWindow.settings()->isVisible()
				   && dialog->isAncestorOf(&aSigning) && aSigning.isVisibleTo(dialog) && !aOther.isVisibleTo(dialog)
				   && aWindow.device_authorization() != nullptr && aSigning.isAncestorOf(aWindow.device_authorization()),
			   during("the shared sign-in dialog shows the signing-in server's card alone, holding the code"));
		const auto texts    = shown_texts(dialog);
		const auto anywhere = [](const QWidget* const) { return true; };
		expect_mentions(texts, QString::fromStdString(aSigning.hostname()), fact_kind::host, anywhere, 1U, 1U,
						during("the dialog names the signing-in host once"));
		expect_mentions(texts, QString::fromStdString(aOther.hostname()), fact_kind::host, anywhere, 0U, 0U,
						during("the dialog names no other server"));
		expect_mentions(
			texts, aTransport, fact_kind::words,
			[&aSigning](const QWidget* const aWidget) { return aWidget == aSigning.kind_label(); }, 1U, 1U,
			during("the card names the transport it speaks for in its kind line"));
		expect_mentions(texts, aTransport, fact_kind::words, anywhere, 1U, 1U,
						during("the dialog names that transport once"));
		expect_mentions(
			texts, QStringLiteral("Signing in"), fact_kind::words,
			[&aSigning](const QWidget* const aWidget) { return aWidget == aSigning.state_label(); }, 1U, 1U,
			during("the card says Signing in in its header"));
		expect_mentions(texts, QStringLiteral("Signing in"), fact_kind::words, anywhere, 1U, 1U,
						during("the dialog says Signing in once"));
		auto filled = std::vector<const QPushButton*>{ };
		for (const auto* const button : dialog->findChildren<QPushButton*>()) {
			if (button->isVisibleTo(dialog) && button->isDefault()) {
				filled.push_back(button);
			}
		}
		expect(filled.size() == 1U && filled.front() == aWindow.device_authorization()->open_button(),
			   during("the code's Open browser is the dialog's one filled control"));
	}

	/// \brief [[BPR-0772]] acceptance 2, RFC-BPR-0032 A4. On every state of the
	/// Accounts page, with each server configured for native HTTPS and GitHub
	/// CLI, host, login, transport and connection state each appear once,
	/// checked by an inventory of the page's visible text.
	///
	/// \details Each state is driven through the window's own entry points and
	/// first checked for having reached its card, so an inventory of a page that
	/// never changed cannot pass for one that did. Each is inventoried with
	/// Details closed and again with the chosen card's Details open.
	auto test_accounts_page_says_each_fact_once() -> void
	{
		auto                 intents = std::vector<application::github_intent>{ };
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			grouped_profiles(),
			[&intents](application::github_intent aIntent) {
				intents.push_back(std::move(aIntent));
			},
		};
		window.resize(1'280, 820);
		window.show();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		auto* const page = window.settings()->page_widget(qt::settings_page::accounts_and_hosts);
		expect(page != nullptr && window.settings()->isVisible()
				   && window.settings()->current_page() == qt::settings_page::accounts_and_hosts,
			   "Settings is open on Accounts and hosts");
		const auto inspect = [&window, page](const accounts_page_state& aState) {
			QApplication::processEvents();
			auto* const current = window.server_card_for(aState.Current);
			const auto  word    = current != nullptr && current->holds(core::host_profile_id{ 21U })
				? aState.GithubState
				: aState.EnterpriseState;
			expect(current != nullptr && current->presentation().Current && current->state_label()->text() == word,
				   std::string{ "the state under inspection has reached its card: " } + std::string{ aState.Name });
			expect_one_card_per_server(window, page, grouped_profiles(), aState.Name);
			expect_each_fact_once(window, page, aState, "Details closed");
			current->details_button()->click();
			QApplication::processEvents();
			expect(current->details_button()->isChecked() && current->details()->isVisibleTo(page)
					   && (current->presentation().LastProblem.isEmpty() || current->last_problem_label()->isVisibleTo(page)),
				   std::string{ "the chosen card's Details opens, with any last problem in it: " } + std::string{ aState.Name });
			expect_each_fact_once(window, page, aState, "the chosen card's Details open");
			current->details_button()->click();
			QApplication::processEvents();
			expect(!current->details_button()->isChecked() && !current->details()->isVisibleTo(page),
				   std::string{ "the chosen card's Details closes again: " } + std::string{ aState.Name });
		};

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = enabled_commands(),
				.Status   = core::github_status_delta{ .Status = core::github_status::no_profile },
			},
		});
		inspect({
			.Name               = "no profile",
			.Current            = core::host_profile_id{ 21U },
			.GithubPresents     = core::host_profile_id{ 21U },
			.GithubState        = QStringLiteral("Signed out"),
			.EnterprisePresents = core::host_profile_id{ 23U },
			.EnterpriseState    = QStringLiteral("Signed out"),
			.SignedIn           = false,
			.Filled             = true,
		});

		// [[BPR-0774]]. The queue is ready and keeps the enterprise server: its
		// card carries Remember this server and Forget server, and Add
		// enterprise server sits under the cards. Every later state keeps them,
		// so each is inventoried with the enterprise rows on the page.
		auto keptServerIntents = std::vector<application::saved_server_intent>{ };
		window.set_saved_server_dispatch([&keptServerIntents](application::saved_server_intent aIntent) {
			keptServerIntents.push_back(std::move(aIntent));
		});
		window.publish(application::queue_landing_projection{
			.Status       = application::queue_landing_status::ready,
			.SavedServers = { application::saved_server{ .Hostname = "ghe.example.invalid", .Account = "reviewer" } },
		});
		QApplication::processEvents();
		{
			const auto  cards      = window.server_cards();
			auto* const github     = window.server_card_for(core::host_profile_id{ 21U });
			auto* const enterprise = window.server_card_for(core::host_profile_id{ 23U });
			auto* const add        = window.add_enterprise_server_button();
			expect(github != nullptr && enterprise != nullptr && add != nullptr && !cards.empty()
					   && enterprise->remember_switch()->isVisibleTo(page) && enterprise->remember_switch()->isChecked()
					   && enterprise->forget_button()->isVisibleTo(page)
					   && !github->remember_switch()->isVisibleTo(page) && !github->forget_button()->isVisibleTo(page),
				   "with the queue keeping ghe.example.invalid, its card remembers it and offers Forget server, and GitHub.com's card offers neither");
			expect(add->isVisibleTo(page) && !add->isDefault()
					   && add->mapTo(page, QPoint{ 0, 0 }).y()
						   >= cards.back()->mapTo(page, QPoint{ 0, 0 }).y() + cards.back()->height(),
				   "Add enterprise server is under the cards and is not the page's filled control");
		}
		inspect({
			.Name               = "the queue keeps the enterprise server",
			.Current            = core::host_profile_id{ 21U },
			.GithubPresents     = core::host_profile_id{ 21U },
			.GithubState        = QStringLiteral("Signed out"),
			.EnterprisePresents = core::host_profile_id{ 23U },
			.EnterpriseState    = QStringLiteral("Signed out"),
			.SignedIn           = false,
			.Filled             = true,
		});
		expect(keptServerIntents.empty(), "inventorying the page with the enterprise rows on it dispatches nothing");

		// [[BPR-0773]], [[BPR-0915]]. Signing in on GitHub.com: the chosen
		// card's Sign in is pressed, which opens the shared sign-in dialog with
		// the page's cards in it, and the code the core answers with is that
		// card's account row there. Its address names github.com as part of an
		// address, which is not the host said a second time. Closing the
		// dialog is the reviewer's Cancel and returns the cards to Settings.
		auto cancels = std::size_t{ 0U };
		window.set_sign_in_cancel_dispatch([&cancels] { ++cancels; });
		const auto returnToSettings = [&window, page] {
			window.sign_in_dialog()->reject();
			window.open_settings(qt::settings_page::accounts_and_hosts);
			QApplication::processEvents();
			return window.settings()->isVisible() && page->isAncestorOf(window.server_card_for(core::host_profile_id{ 21U }))
				&& page->isAncestorOf(window.server_card_for(core::host_profile_id{ 23U }));
		};
		auto* const panel = window.device_authorization();
		window.server_card_for(core::host_profile_id{ 21U })->sign_in_button()->click();
		auto awaiting = application::device_authorization_presentation{
			.Stage                    = core::device_authorization_stage::awaiting_authorization,
			.UserCode                 = "WDJB-MJHT",
			.VerificationUri          = "https://github.com/login/device",
			.RemainingValiditySeconds = 900U,
		};
		awaiting.Commands = core::device_authorization_commands(core::device_authorization_state{
			.Stage = core::device_authorization_stage::awaiting_authorization,
		});
		panel->apply(awaiting);
		QApplication::processEvents();
		expect(window.server_card_for(core::host_profile_id{ 21U })->device_sign_in() == panel
				   && window.sign_in_dialog() != nullptr
				   && panel->address_field()->isVisibleTo(window.sign_in_dialog())
				   && panel->address_field()->text().contains(QStringLiteral("github.com/")),
			   "signing in on GitHub.com, the code and its github.com address are that card's account row in the shared sign-in dialog");
		expect_signing_in_dialog(window, *window.server_card_for(core::host_profile_id{ 21U }),
								 *window.server_card_for(core::host_profile_id{ 23U }), QStringLiteral("native HTTPS"),
								 "signing in on GitHub.com");
		panel->apply(application::device_authorization_presentation{ });
		QApplication::processEvents();
		expect(returnToSettings() && cancels == 1U && !panel->isVisibleTo(page)
				   && window.server_card_for(core::host_profile_id{ 21U })->state_label()->text() == QStringLiteral("Signed out"),
			   "settled back to idle and closed, the sign-in cancels once and returns the cards to Settings, where GitHub.com's card is signed out again and the sign-in is hidden");

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status   = core::github_status_delta{ .Status = core::github_status::establishing_session },
				.Identity = core::github_identity_delta{ .Profile = core::host_profile_id{ 23U } },
			},
		});
		inspect({
			.Name               = "connecting to the enterprise server",
			.Current            = core::host_profile_id{ 23U },
			.GithubPresents     = core::host_profile_id{ 21U },
			.GithubState        = QStringLiteral("Signed out"),
			.EnterprisePresents = core::host_profile_id{ 23U },
			.EnterpriseState    = QStringLiteral("Connecting"),
			.SignedIn           = false,
			.Filled             = false,
		});

		publish_session_ready(window, core::host_profile_id{ 21U });
		inspect({
			.Name               = "signed in on GitHub.com",
			.Current            = core::host_profile_id{ 21U },
			.GithubPresents     = core::host_profile_id{ 21U },
			.GithubState        = QStringLiteral("Connected"),
			.EnterprisePresents = core::host_profile_id{ 23U },
			.EnterpriseState    = QStringLiteral("Signed out"),
			.SignedIn           = true,
			.Filled             = false,
		});

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status      = core::github_status_delta{ .Status = core::github_status::session_failed },
				.Diagnostics = { { .Code = core::github_reason_code::authentication_failed } },
			},
		});
		inspect({
			.Name               = "the session on GitHub.com failed",
			.Current            = core::host_profile_id{ 21U },
			.GithubPresents     = core::host_profile_id{ 21U },
			.GithubState        = QStringLiteral("Could not connect"),
			.EnterprisePresents = core::host_profile_id{ 23U },
			.EnterpriseState    = QStringLiteral("Signed out"),
			.SignedIn           = false,
			.Filled             = true,
		});

		publish_session_ready(window, core::host_profile_id{ 21U });
		auto preview = application::public_preview_presentation{ };
		preview.Mode = application::public_preview_mode::signed_out;
		window.publish(preview);
		inspect({
			.Name               = "reading public pull requests signed out on GitHub.com",
			.Current            = core::host_profile_id{ 21U },
			.GithubPresents     = core::host_profile_id{ 21U },
			.GithubState        = QStringLiteral("Signed out"),
			.EnterprisePresents = core::host_profile_id{ 23U },
			.EnterpriseState    = QStringLiteral("Signed out"),
			.SignedIn           = false,
			.Filled             = true,
		});
		expect(window.server_card_for(core::host_profile_id{ 21U })->consequence_label()->text()
				   == QStringLiteral("Reading public pull requests without an account."),
			   "signed out, GitHub.com's card says it reads public pull requests without an account");

		// With the mode left and the identity retired, GitHub.com is chosen for
		// native HTTPS again, and its GitHub CLI offer chooses GitHub CLI.
		preview.Mode = application::public_preview_mode::signed_in;
		window.publish(preview);
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status   = core::github_status_delta{ .Status = core::github_status::no_profile },
				.Identity = core::github_identity_delta{ },
			},
		});
		QApplication::processEvents();
		auto* const github = window.server_card_for(core::host_profile_id{ 22U });
		expect(github != nullptr && github->presentation().Current && github->profile() == core::host_profile_id{ 21U }
				   && github->cli_offer_button() != nullptr && github->cli_offer_button()->isVisibleTo(page),
			   "with no identity left GitHub.com is chosen for native HTTPS again and offers GitHub CLI");
		intents.clear();
		github->cli_offer_button()->click();
		QApplication::processEvents();
		const auto* const chosen = intents.size() == 1U
			? std::get_if<core::activate_host_profile_intent>(&intents.front())
			: nullptr;
		expect(chosen != nullptr && chosen->Profile == core::host_profile_id{ 22U },
			   "GitHub CLI is chosen on GitHub.com through its offer, which activates exactly that profile");
		// [[BPR-0915]]. The offer opens GitHub CLI sign-in for GitHub.com in the
		// shared dialog; the Settings inventory resumes once it is closed.
		expect(window.sign_in_dialog()->isVisible() && github->isVisibleTo(window.sign_in_dialog())
				   && !window.server_card_for(core::host_profile_id{ 23U })->isVisibleTo(window.sign_in_dialog())
				   && github->login_label()->text() == github->cli_offer_button()->text(),
			   "the GitHub CLI offer opens GitHub CLI sign-in for GitHub.com alone in the shared dialog");
		expect(returnToSettings() && cancels == 2U && intents.size() == 1U,
			   "closing GitHub CLI sign-in cancels it once, dispatches nothing else and returns the cards to Settings");
		inspect({
			.Name               = "GitHub CLI chosen on GitHub.com, signed out",
			.Current            = core::host_profile_id{ 22U },
			.GithubPresents     = core::host_profile_id{ 22U },
			.GithubState        = QStringLiteral("Signed out"),
			.EnterprisePresents = core::host_profile_id{ 23U },
			.EnterpriseState    = QStringLiteral("Signed out"),
			.SignedIn           = false,
			.Filled             = false,
		});

		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status   = core::github_status_delta{ .Status = core::github_status::establishing_session },
				.Identity = core::github_identity_delta{ .Profile = core::host_profile_id{ 22U } },
			},
		});
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status      = core::github_status_delta{ .Status = core::github_status::session_failed },
				.Diagnostics = { { .Code = core::github_reason_code::authentication_failed } },
			},
		});
		inspect({
			.Name               = "GitHub CLI on GitHub.com could not connect",
			.Current            = core::host_profile_id{ 22U },
			.GithubPresents     = core::host_profile_id{ 22U },
			.GithubState        = QStringLiteral("Could not connect"),
			.EnterprisePresents = core::host_profile_id{ 23U },
			.EnterpriseState    = QStringLiteral("Signed out"),
			.SignedIn           = false,
			.Filled             = false,
		});
		window.settings()->hide();
		window.hide();
	}

	/// \brief [[BPR-0773]]. A composition with one server: GitHub.com over
	/// native HTTPS, which has a device flow.
	[[nodiscard]] auto device_profiles()
		-> std::vector<application::github_profile_option>
	{
		return {
			{
				.Profile   = core::host_profile_id{ 31U },
				.Label     = "GitHub.com — Native HTTPS",
				.Hostname  = "github.com",
				.Product   = application::github_profile_product::github_com,
				.Transport = application::github_profile_transport::native_https,
			},
		};
	}

	/// \brief What the composition applies to the device sign-in in one stage:
	/// a live code while awaiting authorization, and the commands the core
	/// allows in that stage.
	[[nodiscard]] auto device_presentation(const core::device_authorization_stage aStage)
		-> application::device_authorization_presentation
	{
		auto presentation = application::device_authorization_presentation{
			.Stage = aStage,
		};
		if (aStage == core::device_authorization_stage::awaiting_authorization) {
			presentation.UserCode                 = "WDJB-MJHT";
			presentation.VerificationUri          = "https://github.com/login/device";
			presentation.RemainingValiditySeconds = 900U;
		}
		presentation.Commands = core::device_authorization_commands(
			core::device_authorization_state{ .Stage = aStage });
		return presentation;
	}

	/// \brief Names every button shown in a card that cannot be pressed; empty when none.
	[[nodiscard]] auto disabled_buttons(const qt::server_card& aCard) -> std::string
	{
		auto named = std::string{ };
		for (const auto* const button : aCard.findChildren<QAbstractButton*>()) {
			if (button->isVisibleTo(&aCard) && !button->isEnabled()) {
				named += " [" + button->objectName().toStdString() + ": \"" + button->text().toStdString() + "\"]";
			}
		}
		return named;
	}

	/// \brief Names every field shown under a widget that is narrower than its
	/// own text in its own font; empty when none.
	[[nodiscard]] auto truncated_fields(const QWidget& aRoot) -> std::string
	{
		auto named = std::string{ };
		for (const auto* const field : aRoot.findChildren<QLineEdit*>()) {
			const auto advance = QFontMetrics{ field->font() }.horizontalAdvance(field->text());
			if (field->isVisibleTo(&aRoot) && field->width() < advance) {
				named += " [" + field->objectName().toStdString() + ": " + std::to_string(field->width())
					+ " px for " + std::to_string(advance) + " px of \"" + field->text().toStdString() + "\"]";
			}
		}
		return named;
	}

	/// \brief [[BPR-0773]] acceptance 1, RFC-BPR-0032 A2. One Sign in gesture on
	/// the chosen card begins the device flow, and the code the core answers
	/// with is shown inside that card: the address sentence and the address,
	/// the code with Copy code, Open browser filled, the validity, one status
	/// sentence and Cancel. Nothing is behind a disclosure or below the fold,
	/// no button shown is disabled and no field is truncated.
	///
	/// \details [[BPR-0915]]. The gesture opens the shared sign-in dialog,
	/// which holds the card while Settings is closed, so the claims are made
	/// there, at the size the dialog opens at. Its own Cancel is the flow's
	/// Cancel; the row's duplicate is hidden.
	auto test_one_sign_in_gesture_shows_the_code_in_the_card() -> void
	{
		auto                 intents = std::vector<application::github_intent>{ };
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			device_profiles(),
			[&intents](application::github_intent aIntent) {
				intents.push_back(std::move(aIntent));
			},
		};
		auto gestures = std::vector<application::device_authorization_gesture>{ };
		window.set_device_authorization_dispatch([&gestures](application::device_authorization_gesture aGesture) {
			gestures.push_back(std::move(aGesture));
		});
		window.resize(1'280, 820);
		window.show();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();

		// Settings keeps the size it opens at; the claim below is made there.
		auto* const settings = window.settings();
		auto* const page     = settings->page_widget(qt::settings_page::accounts_and_hosts);
		auto* const scroll   = qobject_cast<QScrollArea*>(settings->pages()->currentWidget());
		auto* const card     = window.server_card_for(core::host_profile_id{ 31U });
		auto* const panel    = window.device_authorization();
		expect(settings->isVisible() && settings->width() <= 960 && settings->height() <= 640
				   && page != nullptr && scroll != nullptr && scroll->widget() == page
				   && card != nullptr && card->presentation().Current && card->sign_in_button()->isVisible(),
			   "Settings is open on Accounts and hosts at no more than its own size, and the chosen GitHub.com card offers Sign in");

		auto cancels = std::size_t{ 0U };
		window.set_sign_in_cancel_dispatch([&cancels] { ++cancels; });
		card->sign_in_button()->click();
		QApplication::processEvents();
		expect(gestures.size() == 1U
				   && std::holds_alternative<core::begin_device_authorization_intent>(gestures.front())
				   && std::get<core::begin_device_authorization_intent>(gestures.front()).Profile == core::host_profile_id{ 31U }
				   && intents.empty(),
			   "the card's Sign in emits exactly one device begin, for its native HTTPS profile, and no GitHub intent");
		auto* const dialog       = window.sign_in_dialog();
		auto* const signInScroll = dialog != nullptr ? dialog->findChild<QScrollArea*>() : nullptr;
		auto* const buttons      = dialog != nullptr ? dialog->findChild<QDialogButtonBox*>() : nullptr;
		auto* const dialogCancel = buttons != nullptr ? buttons->button(QDialogButtonBox::Cancel) : nullptr;
		expect(dialog != nullptr && dialog->isVisible() && !settings->isVisible() && signInScroll != nullptr
				   && signInScroll->widget() != nullptr && signInScroll->widget()->isAncestorOf(card)
				   && dialogCancel != nullptr && dialogCancel->isVisible(),
			   "the Sign in opens the shared sign-in dialog, which holds the card and offers Cancel, and Settings closes");

		// The core answers with a live code, applied the way the composition's
		// fan-out applies it.
		panel->apply(device_presentation(core::device_authorization_stage::awaiting_authorization));
		QApplication::processEvents();
		auto* const                   account = card->findChild<QWidget*>(QStringLiteral("serverCardAccount"));
		const std::array<QWidget*, 7> row{
			panel->address_sentence_label(),
			panel->address_field(),
			panel->user_code_field(),
			panel->copy_button(),
			panel->open_button(),
			panel->validity_label(),
			panel->status_label(),
		};
		expect(card->device_sign_in() == panel && account != nullptr && account->isAncestorOf(panel)
				   && std::ranges::all_of(row, [card, account](const QWidget* const aWidget) {
						  return card->isAncestorOf(aWidget) && account->isAncestorOf(aWidget) && aWidget->isVisible()
							  && !card->details()->isAncestorOf(aWidget);
					  })
				   && !panel->cancel_button()->isVisible() && dialogCancel->isVisible(),
			   "the address sentence, the address, the code, Copy code, Open browser, the validity and the status sentence are all shown in the card's account row, in no disclosure, and the dialog's own Cancel stands for the row's");
		expect(panel->address_sentence_label()->text() == QStringLiteral("Enter this code at")
				   && panel->address_field()->text() == QStringLiteral("https://github.com/login/device")
				   && panel->user_code_field()->text() == QStringLiteral("WDJB-MJHT")
				   && panel->user_code_field()->isReadOnly()
				   && panel->open_button()->text() == QStringLiteral("Open browser")
				   && panel->open_button()->isDefault()
				   && panel->cancel_button()->text() == QStringLiteral("Cancel")
				   && !panel->validity_label()->text().isEmpty()
				   && panel->status_label()->text().startsWith(QStringLiteral("Waiting for the browser to authorize this ")),
			   "the row reads Enter this code at and the address, the selectable code, Open browser filled, the validity, one status sentence and Cancel");
		expect(card->state_label()->text() == QStringLiteral("Signing in")
				   && !card->sign_in_button()->isVisible() && !card->login_label()->isVisible(),
			   "the header says Signing in, and the row is the sign-in rather than the card's ways in");

		const QRect placed{ panel->mapTo(signInScroll->viewport(), QPoint{ 0, 0 }), panel->size() };
		expect(!placed.isEmpty() && signInScroll->viewport()->rect().contains(placed),
			   "the whole sign-in lies inside the shared sign-in dialog's viewport at the size it opens at, without scrolling");
		const auto disabled = disabled_buttons(*card);
		expect(disabled.empty(), "no button shown in the card is disabled:" + disabled);
		const auto truncated = truncated_fields(*panel);
		expect(truncated.empty(), "no field in the sign-in is narrower than its text:" + truncated);
		const auto code = panel->user_code_field()->font();
		expect(QFontInfo{ code }.fixedPitch() && panel->font().pointSizeF() > 0.0
				   && std::abs(code.pointSizeF() - panel->font().pointSizeF() * 1.6) <= panel->font().pointSizeF() * 0.02,
			   "the code is in a fixed-pitch font at 1.6 times the row's size");
		expect(!card->details_button()->isChecked() && !card->details()->isVisible(),
			   "the card's Details stays collapsed");

		auto* const clipboard = QApplication::clipboard();
		expect(clipboard != nullptr, "the Qt platform provides a clipboard");
		clipboard->setText(QStringLiteral("clipboard before Copy code"));
		panel->copy_button()->click();
		expect(clipboard->text() == QStringLiteral("WDJB-MJHT")
				   && panel->copy_button()->text() == QStringLiteral("Copied")
				   && panel->copy_button()->accessibleName() == QStringLiteral("Copy code")
				   && panel->copy_button()->toolTip() == QStringLiteral("Copy code"),
			   "Copy code puts exactly the code on the clipboard and says Copied in place");
		expect(gestures.size() == 1U && intents.empty() && cancels == 0U, "showing and copying the code dispatches nothing more");
		dialogCancel->click();
		QApplication::processEvents();
		expect(cancels == 1U && !dialog->isVisible() && page->isAncestorOf(card) && gestures.size() == 1U,
			   "the dialog's Cancel cancels sign-in exactly once and returns the card to Settings");
		window.set_device_authorization_dispatch({ });
		settings->hide();
		window.hide();
	}

	/// \brief [[BPR-0803]]. Choosing GitHub CLI while a device flow awaits its
	/// code keeps the code beside the CLI choice until the reviewer ends one.
	///
	/// \details The owner's decision of 2026-09-24 (`dbae079d`). Both endings
	/// run: the CLI way in finishing first, which ends the device flow through
	/// its own Cancel, and the device authorization finishing first, which
	/// closes sign-in and so withdraws the CLI choice. Each says which account
	/// the host ends signed in to, and neither leaves a flow running.
	auto test_the_code_stays_beside_the_github_cli_choice() -> void
	{
		enum class ending { cli_first,
							device_first };
		for (const auto end : { ending::cli_first, ending::device_first }) {
			auto                 intents = std::vector<application::github_intent>{ };
			qt::workbench_window window{
				qt::workbench_intent_dispatch{ },
				qt::workbench_change_dispatch{ },
				qt::workbench_drawer_width_dispatch{ },
				device_profiles(),
				[&intents](application::github_intent aIntent) {
					intents.push_back(std::move(aIntent));
				},
			};
			auto gestures = std::vector<application::device_authorization_gesture>{ };
			window.set_device_authorization_dispatch([&gestures](application::device_authorization_gesture aGesture) {
				gestures.push_back(std::move(aGesture));
			});
			auto discoveries = std::size_t{ 0U };
			window.set_github_cli_discovery_dispatch([&discoveries] { ++discoveries; });
			auto cancels = std::size_t{ 0U };
			window.set_sign_in_cancel_dispatch([&cancels] { ++cancels; });
			window.resize(1'280, 820);
			window.show();
			window.open_settings(qt::settings_page::accounts_and_hosts);
			QApplication::processEvents();
			auto* const card  = window.server_card_for(core::host_profile_id{ 31U });
			auto* const panel = window.device_authorization();
			card->sign_in_button()->click();
			QApplication::processEvents();
			panel->apply(device_presentation(core::device_authorization_stage::awaiting_authorization));
			QApplication::processEvents();
			auto* const dialog = window.sign_in_dialog();
			expect(dialog != nullptr && dialog->isVisible() && card->isVisibleTo(dialog) && panel->user_code_field()->isVisible()
					   && gestures.size() == 1U && std::holds_alternative<core::begin_device_authorization_intent>(gestures.front()),
				   "the card's Sign in begins one device flow, whose code is shown in the shared sign-in dialog");

			window.use_github_cli_button()->click();
			QApplication::processEvents();
			const auto codeInView = [&] {
				return card->isVisibleTo(dialog) && panel->isVisibleTo(dialog) && panel->user_code_field()->isVisible()
					&& panel->user_code_field()->text() == QStringLiteral("WDJB-MJHT") && panel->address_field()->isVisible();
			};
			expect(discoveries == 1U && dialog->isVisible() && codeInView(),
				   "Use GitHub CLI during the flow runs one discovery and keeps the GitHub.com card, its code and its address in view");

			// The discovery inventory finds a GitHub CLI account on github.com.
			window.add_setup_profile({
				.Profile   = core::host_profile_id{ 32U },
				.Label     = "GitHub.com — GitHub CLI (octo-cli, active in gh)",
				.Hostname  = "github.com",
				.Product   = application::github_profile_product::github_com,
				.Transport = application::github_profile_transport::github_cli,
			});
			window.present_cli_account_choices({ core::host_profile_id{ 32U } });
			window.present_github_cli_discovery(qt::github_cli_discovery_state::ready, 1U);
			QApplication::processEvents();
			expect(codeInView() && card->cli_offer_button() != nullptr && card->cli_offer_button()->isVisibleTo(dialog)
					   && card->cli_offer_button()->text().contains(QStringLiteral("octo-cli")),
				   "the GitHub CLI choice stands beside the running code, on the same card");
			expect(gestures.size() == 1U && intents.empty(), "choosing GitHub CLI ends nothing by itself");

			if (end == ending::cli_first) {
				card->cli_offer_button()->click();
				QApplication::processEvents();
				const auto* const activation = intents.size() == 1U ? std::get_if<core::activate_host_profile_intent>(&intents.front()) : nullptr;
				expect(activation != nullptr && activation->Profile == core::host_profile_id{ 32U }
						   && activation->Mode == core::github_access_mode::authenticated && gestures.size() == 1U && codeInView(),
					   "the GitHub CLI choice activates exactly the discovered GitHub CLI profile, and the code stays until that way in finishes");
				publish_session_ready(window, core::host_profile_id{ 32U });
				QApplication::processEvents();
				expect(gestures.size() == 2U && std::holds_alternative<core::cancel_device_authorization_intent>(gestures.back()),
					   "once the GitHub CLI way in has signed in, the device flow is ended through its own Cancel, once");
				panel->apply(device_presentation(core::device_authorization_stage::cancelled));
				QApplication::processEvents();
				expect(!dialog->isVisible() && cancels == 0U && !panel->signing_in(),
					   "sign-in closes as finished, not cancelled, and no device flow is left running");
				expect(card->signed_in() && card->profile() == core::host_profile_id{ 32U }
						   && card->login_label()->text() == QStringLiteral("octo-reviewer"),
					   "and GitHub.com ends signed in through its GitHub CLI profile, naming the account");
			} else {
				panel->apply(device_presentation(core::device_authorization_stage::authorized));
				publish_session_ready(window, core::host_profile_id{ 31U });
				QApplication::processEvents();
				expect(gestures.size() == 1U && intents.empty() && cancels == 0U,
					   "the device authorization finishing first cancels nothing and activates no GitHub CLI profile");
				expect(!dialog->isVisible() && !panel->signing_in(),
					   "sign-in closes, which withdraws the GitHub CLI choice, and no flow is left running");
				expect(card->signed_in() && card->profile() == core::host_profile_id{ 31U }
						   && card->login_label()->text() == QStringLiteral("octo-reviewer"),
					   "and GitHub.com ends signed in through its native HTTPS profile, naming the account");
			}
			window.set_device_authorization_dispatch({ });
			window.settings()->hide();
			window.hide();
		}
	}

	/// \brief [[BPR-0773]] acceptance 2, RFC-BPR-0032 A2. Cancelled, expired and
	/// denied flows stay in the card's account row with their sentence and one
	/// Sign in again. An unavailable device flow adds Sign in with a token,
	/// which is the window's own Connect. No button shown is disabled and no
	/// field is truncated, and an authorized flow hands the row back to the
	/// signed-in account.
	auto test_terminal_and_token_rows_stay_in_the_card() -> void
	{
		auto                 intents = std::vector<application::github_intent>{ };
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			device_profiles(),
			[&intents](application::github_intent aIntent) {
				intents.push_back(std::move(aIntent));
			},
		};
		auto gestures = std::vector<application::device_authorization_gesture>{ };
		window.set_device_authorization_dispatch([&gestures](application::device_authorization_gesture aGesture) {
			gestures.push_back(std::move(aGesture));
		});
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = enabled_commands(),
				.Status   = core::github_status_delta{ .Status = core::github_status::no_profile },
			},
		});
		window.resize(1'280, 820);
		window.show();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		auto* const card  = window.server_card_for(core::host_profile_id{ 31U });
		auto* const panel = window.device_authorization();
		expect(card != nullptr && card->presentation().Current && card->device_sign_in() == panel,
			   "the chosen GitHub.com card holds the device sign-in");

		// [[BPR-0915]]. Sign in opens the shared sign-in dialog, whose own
		// Cancel is the live code's Cancel. It leaves through the sign-in
		// cancel sink, which the composition turns into the device flow's
		// cancel (github_application.cpp, set_sign_in_cancel_dispatch).
		auto cancels = std::size_t{ 0U };
		window.set_sign_in_cancel_dispatch([&cancels] { ++cancels; });
		card->sign_in_button()->click();
		panel->apply(device_presentation(core::device_authorization_stage::awaiting_authorization));
		QApplication::processEvents();
		auto* const buttons      = window.sign_in_dialog() != nullptr ? window.sign_in_dialog()->findChild<QDialogButtonBox*>() : nullptr;
		auto* const dialogCancel = buttons != nullptr ? buttons->button(QDialogButtonBox::Cancel) : nullptr;
		expect(gestures.size() == 1U && std::holds_alternative<core::begin_device_authorization_intent>(gestures.back())
				   && window.sign_in_dialog()->isVisible() && dialogCancel != nullptr && dialogCancel->isVisible()
				   && !panel->cancel_button()->isVisible(),
			   "Sign in begins the flow in the shared sign-in dialog, whose Cancel is its live code's Cancel");
		dialogCancel->click();
		QApplication::processEvents();
		expect(cancels == 1U && gestures.size() == 1U && !window.sign_in_dialog()->isVisible(),
			   "Cancel leaves exactly once, through sign-in's own cancel, and closes the dialog");
		// The cards are back in Settings, which the reviewer reopens to see how
		// the flow ended.
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();

		// A terminal row keeps its sentence in the card, offers one Sign in
		// again, and shows nothing it cannot do.
		const auto expectTerminalRow = [card, panel](const std::string_view aStage, const QString& aSentence, const bool aToken) {
			QApplication::processEvents();
			const auto name    = std::string{ aStage };
			auto       retries = 0;
			for (const auto* const button : card->findChildren<QAbstractButton*>()) {
				if (button->isVisible() && button->text() == QStringLiteral("Sign in again")) {
					++retries;
				}
			}
			expect(card->device_sign_in() == panel && card->isAncestorOf(panel) && panel->isVisible()
					   && panel->status_label()->isVisible() && panel->status_label()->text() == aSentence,
				   name + ": the card's account row keeps the stage's sentence");
			expect(retries == 1 && panel->begin_button()->isVisible()
					   && panel->begin_button()->text() == QStringLiteral("Sign in again"),
				   name + ": the row offers exactly one Sign in again");
			expect(!panel->cancel_button()->isVisible() && !panel->open_button()->isVisible()
					   && !panel->copy_button()->isVisible(),
				   name + ": the row shows no Cancel, Open browser or Copy code");
			expect(panel->token_button()->isVisible() == aToken
					   && (!aToken || panel->token_button()->text() == QStringLiteral("Sign in with a token")),
				   name + (aToken ? ": the row offers Sign in with a token" : ": the row offers no token way in"));
			expect(card->state_label()->text() == QStringLiteral("Signed out") && !card->sign_in_button()->isVisible(),
				   name + ": the header says Signed out, and the row's retry stands in for the card's own Sign in");
			const auto disabled  = disabled_buttons(*card);
			const auto truncated = truncated_fields(*card);
			expect(disabled.empty(), name + ": no button shown in the card is disabled:" + disabled);
			expect(truncated.empty(), name + ": no field in the card is narrower than its text:" + truncated);
		};
		panel->apply(device_presentation(core::device_authorization_stage::cancelled));
		expectTerminalRow("cancelled", QStringLiteral("Sign-in was cancelled."), false);
		panel->apply(device_presentation(core::device_authorization_stage::expired));
		expectTerminalRow("expired", QStringLiteral("The code expired before it was used."), false);
		panel->apply(device_presentation(core::device_authorization_stage::denied));
		expectTerminalRow("denied", QStringLiteral("Sign-in was denied."), false);
		panel->apply(device_presentation(core::device_authorization_stage::unavailable));
		expectTerminalRow("unavailable", QStringLiteral("Device sign-in is unavailable here."), true);

		// Where the device flow is unavailable, the token way in is the window's
		// own Connect, which opens the composition's existing token prompt.
		intents.clear();
		const auto deviceGestures = gestures.size();
		panel->token_button()->click();
		QApplication::processEvents();
		const auto* const activation = intents.size() == 1U
			? std::get_if<core::activate_host_profile_intent>(&intents.front())
			: nullptr;
		expect(activation != nullptr && activation->Profile == core::host_profile_id{ 31U }
				   && activation->Mode == core::github_access_mode::authenticated
				   && window.session_panel()->selected_profile() == core::host_profile_id{ 31U }
				   && gestures.size() == deviceGestures,
			   "Sign in with a token triggers the window's Connect: exactly one activation for the chosen profile, and no device gesture");

		panel->apply(device_presentation(core::device_authorization_stage::authorized));
		publish_session_ready(window, core::host_profile_id{ 31U });
		QApplication::processEvents();
		expect(!panel->isVisible() && card->signed_in()
				   && card->login_label()->isVisible()
				   && card->login_label()->text() == QStringLiteral("octo-reviewer")
				   && card->state_label()->text() == QStringLiteral("Connected")
				   && card->sign_out_button()->isVisible(),
			   "an authorized flow and the session it opens hand the row back to the signed-in account");
		window.set_device_authorization_dispatch({ });
		window.settings()->hide();
		window.hide();
	}

	/// \brief [[BPR-0773]] D5. Signed out on GitHub.com, the strip's account
	/// button presses the chosen card's own Sign in. With an account signed in,
	/// or with a chosen host that has no device flow, it only opens the page.
	auto test_the_account_action_presses_the_chosen_cards_sign_in() -> void
	{
		{
			auto                 intents = std::vector<application::github_intent>{ };
			qt::workbench_window window{
				qt::workbench_intent_dispatch{ },
				qt::workbench_change_dispatch{ },
				qt::workbench_drawer_width_dispatch{ },
				device_profiles(),
				[&intents](application::github_intent aIntent) {
					intents.push_back(std::move(aIntent));
				},
			};
			auto gestures = std::vector<application::device_authorization_gesture>{ };
			window.set_device_authorization_dispatch([&gestures](application::device_authorization_gesture aGesture) {
				gestures.push_back(std::move(aGesture));
			});
			window.resize(1'280, 820);
			window.show();
			QApplication::processEvents();
			window.account_action()->trigger();
			QApplication::processEvents();
			expect(window.sign_in_dialog() != nullptr && window.sign_in_dialog()->isVisible()
					   && !window.settings()->isVisible()
					   && gestures.size() == 1U
					   && std::holds_alternative<core::begin_device_authorization_intent>(gestures.front())
					   && std::get<core::begin_device_authorization_intent>(gestures.front()).Profile == core::host_profile_id{ 31U }
					   && intents.empty(),
				   "signed out on GitHub.com, the account button opens shared sign-in and begins one device flow for the chosen host");

			window.sign_in_dialog()->reject();
			publish_session_ready(window, core::host_profile_id{ 31U });
			QApplication::processEvents();
			auto* const card = window.server_card_for(core::host_profile_id{ 31U });
			expect(card != nullptr && card->signed_in() && !card->sign_in_button()->isVisibleTo(card),
				   "with an account signed in, the chosen card offers no Sign in");
			gestures.clear();
			intents.clear();
			window.account_action()->trigger();
			QApplication::processEvents();
			expect(window.settings()->isVisible()
					   && window.settings()->current_page() == qt::settings_page::accounts_and_hosts
					   && gestures.empty() && intents.empty(),
				   "signed in, the account button only opens Accounts and hosts: no device begin and no intent");
			window.set_device_authorization_dispatch({ });
			window.settings()->hide();
			window.hide();
		}

		auto                 intents = std::vector<application::github_intent>{ };
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			std::vector<application::github_profile_option>{
				{
					.Profile   = core::host_profile_id{ 41U },
					.Label     = "GHES 3.18 — Native HTTPS",
					.Hostname  = "ghe.example.invalid",
					.Product   = application::github_profile_product::github_enterprise_server,
					.Transport = application::github_profile_transport::native_https,
				},
			},
			[&intents](application::github_intent aIntent) {
				intents.push_back(std::move(aIntent));
			},
		};
		auto gestures = std::vector<application::device_authorization_gesture>{ };
		window.set_device_authorization_dispatch([&gestures](application::device_authorization_gesture aGesture) {
			gestures.push_back(std::move(aGesture));
		});
		window.resize(1'280, 820);
		window.show();
		QApplication::processEvents();
		auto* const card = window.server_card_for(core::host_profile_id{ 41U });
		expect(card != nullptr && card->presentation().Current && !card->offers_device_sign_in()
				   && card->sign_in_button()->isVisibleTo(card),
			   "a chosen enterprise host offers Sign in, but it has no device flow");
		window.account_action()->trigger();
		QApplication::processEvents();
		expect(window.settings()->isVisible()
				   && window.settings()->current_page() == qt::settings_page::accounts_and_hosts
				   && gestures.empty() && intents.empty(),
			   "with a chosen host that has no device flow, the account button only opens Accounts and hosts: no device begin and no intent");
		window.set_device_authorization_dispatch({ });
		window.settings()->hide();
		window.hide();
	}

	/// \brief [[BPR-0774]] review correction. One enterprise host, over native
	/// HTTPS, which has no device flow.
	[[nodiscard]] auto enterprise_profile() -> application::github_profile_option
	{
		return {
			.Profile   = core::host_profile_id{ 41U },
			.Label     = "GHES 3.18 — Native HTTPS",
			.Hostname  = "ghe.example.invalid",
			.Product   = application::github_profile_product::github_enterprise_server,
			.Transport = application::github_profile_transport::native_https,
		};
	}

	/// \brief [[BPR-0773]], [[BPR-0774]] review correction. A code belongs to the
	/// host it was asked for. While it is live its row stays on that host's
	/// card whichever host is chosen. Once it has finished, its sentence is said
	/// only on that host's card, and another host's card offers its own ways in.
	auto test_a_finished_sign_in_is_said_only_on_its_host() -> void
	{
		auto                 intents = std::vector<application::github_intent>{ };
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			std::vector<application::github_profile_option>{ device_profiles().front(), enterprise_profile() },
			[&intents](application::github_intent aIntent) {
				intents.push_back(std::move(aIntent));
			},
		};
		auto gestures = std::vector<application::device_authorization_gesture>{ };
		window.set_device_authorization_dispatch([&gestures](application::device_authorization_gesture aGesture) {
			gestures.push_back(std::move(aGesture));
		});
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = enabled_commands(),
				.Status   = core::github_status_delta{ .Status = core::github_status::no_profile },
			},
		});
		window.resize(1'280, 820);
		window.show();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		auto* const github     = window.server_card_for(core::host_profile_id{ 31U });
		auto* const enterprise = window.server_card_for(core::host_profile_id{ 41U });
		auto* const panel      = window.device_authorization();
		expect(github != nullptr && enterprise != nullptr && github != enterprise && github->presentation().Current
				   && github->sign_in_button()->isVisible() && enterprise->connect_button()->isVisible(),
			   "GitHub.com is chosen and offers Sign in, and the enterprise card offers Connect");
		// The device gesture at `aIndex` begins a sign-in for GitHub.com's profile.
		const auto begun = [&gestures](const std::size_t aIndex) {
			return aIndex < gestures.size()
				&& std::holds_alternative<core::begin_device_authorization_intent>(gestures[aIndex])
				&& std::get<core::begin_device_authorization_intent>(gestures[aIndex]).Profile == core::host_profile_id{ 31U };
		};
		// The GitHub intent at `aIndex` activates exactly this profile with an account.
		const auto activated = [&intents](const std::size_t aIndex, const core::host_profile_id aProfile) {
			const auto* const activation = aIndex < intents.size()
				? std::get_if<core::activate_host_profile_intent>(&intents[aIndex])
				: nullptr;
			return activation != nullptr && activation->Profile == aProfile
				&& activation->Mode == core::github_access_mode::authenticated;
		};

		// [[BPR-0915]]. Sign in opens the shared sign-in dialog for GitHub.com,
		// with the code in that card's row there.
		auto cancels = std::size_t{ 0U };
		window.set_sign_in_cancel_dispatch([&cancels] { ++cancels; });
		github->sign_in_button()->click();
		panel->apply(device_presentation(core::device_authorization_stage::awaiting_authorization));
		QApplication::processEvents();
		auto* const dialog = window.sign_in_dialog();
		expect(gestures.size() == 1U && begun(0U) && github->device_sign_in() == panel && github->isAncestorOf(panel)
				   && dialog != nullptr && dialog->isVisible() && panel->isVisible()
				   && github->state_label()->text() == QStringLiteral("Signing in")
				   && !enterprise->isVisibleTo(dialog),
			   "GitHub.com's Sign in begins once in the shared sign-in dialog, and the live code is that card's row, saying Signing in");

		// While the code is live another host is chosen the way the composition
		// chooses one for a pull request link on it: the dialog is retargeted to
		// that host, whose card then offers its own way in.
		window.open_sign_in("ghe.example.invalid");
		QApplication::processEvents();
		expect(dialog->isVisible() && enterprise->isVisibleTo(dialog) && !github->isVisibleTo(dialog)
				   && enterprise->connect_button()->isVisible(),
			   "retargeted to the enterprise server, the dialog shows its card alone, offering Connect");
		enterprise->connect_button()->click();
		QApplication::processEvents();
		expect(intents.size() == 1U && activated(0U, core::host_profile_id{ 41U }) && gestures.size() == 1U,
			   "Connect on the enterprise card while the code is live dispatches exactly one activation, for the enterprise profile");
		expect(github->device_sign_in() == panel && github->isAncestorOf(panel) && panel->isVisibleTo(github)
				   && github->state_label()->text() == QStringLiteral("Signing in")
				   && enterprise->device_sign_in() == nullptr && !enterprise->isAncestorOf(panel)
				   && enterprise->state_label()->text() != QStringLiteral("Signing in"),
			   "the live code stays on GitHub.com's card, and the enterprise card shows no device row");

		panel->apply(device_presentation(core::device_authorization_stage::denied));
		QApplication::processEvents();
		expect(!panel->isVisible() && github->device_sign_in() == nullptr && !github->isAncestorOf(panel)
				   && github->state_label()->text() == QStringLiteral("Signed out")
				   && enterprise->state_label()->text() == QStringLiteral("Signed out"),
			   "a denied code is said on no card once another host is chosen, and GitHub.com's card no longer holds the sign-in");
		expect(enterprise->presentation().Current && enterprise->sign_in_button()->isVisible()
				   && enterprise->login_label()->isVisible() && github->connect_button()->isVisibleTo(github),
			   "the enterprise card offers its own Sign in, and GitHub.com's card offers Connect");
		const auto disabledWhileDenied = disabled_buttons(*github) + disabled_buttons(*enterprise);
		expect(disabledWhileDenied.empty(), "no button shown in either card is disabled:" + disabledWhileDenied);

		// Back in Settings, GitHub.com's Connect chooses it again and, since
		// the host has a device flow, is that flow's retry at once.
		dialog->reject();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		expect(cancels == 1U && !dialog->isVisible() && github->connect_button()->isVisible(),
			   "closing the dialog cancels sign-in once, and in Settings GitHub.com's card offers Connect");
		github->connect_button()->click();
		QApplication::processEvents();
		expect(intents.size() == 1U && gestures.size() == 2U && begun(1U),
			   "Connect on GitHub.com's card begins exactly one device sign-in, for its profile, and dispatches no GitHub intent");
		auto retries = 0;
		for (const auto* const button : github->findChildren<QAbstractButton*>()) {
			if (button->isVisible() && button->text() == QStringLiteral("Sign in again")) {
				++retries;
			}
		}
		expect(dialog->isVisible() && github->isVisibleTo(dialog) && !enterprise->isVisibleTo(dialog)
				   && github->presentation().Current && github->device_sign_in() == panel && github->isAncestorOf(panel)
				   && panel->isVisible() && panel->status_label()->isVisible()
				   && panel->status_label()->text() == QStringLiteral("Sign-in was denied.")
				   && retries == 1 && panel->begin_button()->isVisible(),
			   "chosen again in the shared sign-in dialog, GitHub.com's card holds the sign-in and says its denial, with one Sign in again, until the core answers");
		const auto disabledOnItsHost = disabled_buttons(*github) + disabled_buttons(*enterprise);
		expect(disabledOnItsHost.empty(), "no button shown in either card is disabled:" + disabledOnItsHost);
		dialog->reject();
		QApplication::processEvents();
		expect(cancels == 2U && gestures.size() == 2U && intents.size() == 1U,
			   "closing it again cancels once more, and no other device gesture or GitHub intent was dispatched");
		window.set_device_authorization_dispatch({ });
		window.settings()->hide();
		window.hide();
	}

	/// \brief [[BPR-0774]] review correction. A card's Sign in shows only while
	/// its way in is admitted: the device flow's begin for a host that has one,
	/// Connect for a host that does not. A refused way in hides it rather than
	/// leaving a button that does nothing, and no disabled button shows.
	auto test_sign_in_waits_for_its_way_in_to_be_admitted() -> void
	{
		{
			auto                 intents = std::vector<application::github_intent>{ };
			qt::workbench_window window{
				qt::workbench_intent_dispatch{ },
				qt::workbench_change_dispatch{ },
				qt::workbench_drawer_width_dispatch{ },
				std::vector<application::github_profile_option>{ enterprise_profile() },
				[&intents](application::github_intent aIntent) {
					intents.push_back(std::move(aIntent));
				},
			};
			window.publish(application::github_presentation_update{
				.Transition = application::github_transition_projection{
					.Commands = enabled_commands(),
					.Status   = core::github_status_delta{ .Status = core::github_status::no_profile },
				},
			});
			window.resize(1'280, 820);
			window.show();
			window.open_settings(qt::settings_page::accounts_and_hosts);
			QApplication::processEvents();
			auto* const card = window.server_card_for(core::host_profile_id{ 41U });
			expect(card != nullptr && card->presentation().Current && !card->offers_device_sign_in()
					   && card->sign_in_button()->isVisible() && !card->connect_button()->isVisible(),
				   "a chosen enterprise host offers Sign in while Connect is admitted");
			// The core's answer arrives on its own, as a command projection.
			const auto admit = [&window](const bool aEnabled) {
				window.publish(application::github_presentation_update{
					.Transition = application::github_transition_projection{
						.Commands = {
							{
								.Id             = core::github_command_id::activate_host_profile,
								.Enabled        = aEnabled,
								.DisabledReason = aEnabled ? core::github_reason_code::none : core::github_reason_code::invalid_profile,
							},
						},
					},
				});
				QApplication::processEvents();
			};
			admit(false);
			const auto refused = disabled_buttons(*card);
			expect(!card->sign_in_button()->isVisible() && !card->connect_button()->isVisible()
					   && refused.empty() && intents.empty(),
				   "with Connect refused, the enterprise card shows neither Sign in nor Connect, and no disabled button:" + refused);
			admit(true);
			expect(card->sign_in_button()->isVisible() && intents.empty(), "admitted again, Sign in returns");
			window.settings()->hide();
			window.hide();
		}

		auto                 gestures = std::vector<application::device_authorization_gesture>{ };
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			device_profiles(),
			[](application::github_intent) { },
		};
		window.set_device_authorization_dispatch([&gestures](application::device_authorization_gesture aGesture) {
			gestures.push_back(std::move(aGesture));
		});
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = enabled_commands(),
				.Status   = core::github_status_delta{ .Status = core::github_status::no_profile },
			},
		});
		window.resize(1'280, 820);
		window.show();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		auto* const card  = window.server_card_for(core::host_profile_id{ 31U });
		auto* const panel = window.device_authorization();
		expect(card != nullptr && card->presentation().Current && card->offers_device_sign_in()
				   && panel->begin_button()->isEnabled() && card->sign_in_button()->isVisible(),
			   "a chosen GitHub.com card offers Sign in while the device flow's begin is admitted");
		// The core refuses a begin, as it does while a flow is being cancelled,
		// while nothing is on screen for the flow itself.
		auto refusedBegin     = application::device_authorization_presentation{ };
		refusedBegin.Commands = core::device_authorization_commands(
			core::device_authorization_state{ .Stage = core::device_authorization_stage::cancelling });
		panel->apply(refusedBegin);
		QApplication::processEvents();
		const auto refused = disabled_buttons(*card);
		expect(!panel->begin_button()->isEnabled() && !card->sign_in_button()->isVisible() && !panel->isVisible()
				   && refused.empty(),
			   "with the device flow's begin refused, GitHub.com's card shows no Sign in and no disabled button:" + refused);
		panel->apply(application::device_authorization_presentation{ });
		QApplication::processEvents();
		expect(panel->begin_button()->isEnabled() && card->sign_in_button()->isVisible() && gestures.empty(),
			   "once a begin is admitted again Sign in returns, and nothing was dispatched");
		window.set_device_authorization_dispatch({ });
		window.settings()->hide();
		window.hide();
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewGitHubQtTests"));

	if (QCoreApplication::arguments().contains(QStringLiteral("--loading-only"))) {
		test_held_read_paints_loading();
		test_cancellation_states_and_failure_layouts();
		test_a_load_refusal_is_said_under_the_home_bar();
		std::cout << "GitHub Qt loading and refusal tests passed\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-1200]]. The refused first read's course, alone, at each scale.
	if (QCoreApplication::arguments().contains(QStringLiteral("--home-refusal"))) {
		test_a_refused_first_read_stays_on_home_and_says_why();
		std::cout << "home-refusal:pass\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-1137]]. The first-launch card's sentence on this platform, alone,
	// so each platform confirms its own wording without the full run.
	if (QCoreApplication::arguments().contains(QStringLiteral("--home-sign-in"))) {
		test_home_sign_in_sentence_names_this_machine();
		std::cout << "home-sign-in:pass\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-1140]]. The GitHub CLI read consent's sentence on this platform,
	// alone, as the first-launch card's is above.
	if (QCoreApplication::arguments().contains(QStringLiteral("--cli-read-consent"))) {
		test_cli_read_consent_names_this_machine();
		std::cout << "cli-read-consent:pass\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-0981]]. The resident drawer filters' composed gesture path alone;
	// the full run stops later at the card assertion [[BPR-0940]] owns.
	if (QCoreApplication::arguments().contains(QStringLiteral("--resident-drawer-filters"))) {
		test_drawer_filters_are_resident_and_keep_the_open_views();
		std::cout << "resident-drawer-filters:pass\n";
		return EXIT_SUCCESS;
	}

	test_cli_warning_lives_on_its_servers_card();
	test_cli_read_consent_is_a_short_decision();
	test_cli_read_consent_names_this_machine();
	test_conversation_model_is_stable_and_incremental();
	test_intent_translation_is_narrow_and_account_is_output_only();
	test_held_read_paints_loading();
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
	test_settings_pages_carry_the_server_cards_and_the_rule_editor();
	test_repositories_drawer_lists_the_queue_and_opens_on_overview();
	test_drawer_filters_are_resident_and_keep_the_open_views();
	test_home_action_list_opens_rows_and_the_bar_on_overview();
	test_home_signed_out_is_one_paste_field();
	test_home_sign_in_sentence_names_this_machine();
	test_home_leads_with_the_bar_signed_in();
	test_home_lists_recently_viewed_pull_requests();
	test_a_pull_request_without_a_comparison_still_lists_its_files();
	test_read_drawer_shows_threads_as_cards_with_scope_and_actions();
	test_one_pane_places_github_threads_on_their_own_rows();
	test_drawer_controls_are_reachable_in_the_conversations_drawer();
	test_home_landing_funnels_to_an_account_and_then_a_pull_request();
	test_device_sign_in_is_mounted_in_the_chosen_cards_account_row();
	test_accounts_page_carries_none_of_the_retired_chrome();
	test_a_session_failure_is_said_in_its_card();
	test_a_load_refusal_is_said_under_the_home_bar();
	test_a_refused_first_read_stays_on_home_and_says_why();
	test_a_card_connect_chooses_and_connects_its_host();
	test_one_card_per_server_holds_its_ways_in();
	test_accounts_page_says_each_fact_once();
	test_one_sign_in_gesture_shows_the_code_in_the_card();
	test_the_code_stays_beside_the_github_cli_choice();
	test_terminal_and_token_rows_stay_in_the_card();
	test_the_account_action_presses_the_chosen_cards_sign_in();
	test_a_finished_sign_in_is_said_only_on_its_host();
	test_sign_in_waits_for_its_way_in_to_be_admitted();

	std::cout << "All GitHub Qt presentation tests passed\n";
	return EXIT_SUCCESS;
}
