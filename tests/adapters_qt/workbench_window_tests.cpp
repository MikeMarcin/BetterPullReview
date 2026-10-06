#include "../support/review_tour_fixture.hpp"
#include "diff_publication_fixture.hpp"

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/epoch_history_model.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_thread_map.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAbstractTextDocumentLayout>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCursor>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileInfo>
#include <QGroupBox>
#include <QHelpEvent>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStatusTipEvent>
#include <QStringList>
#include <QStyle>
#include <QStyleFactory>
#include <QSysInfo>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QWidget>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>

#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {
#if defined(_WIN32)
	// [[BPR-0653]]. On Windows the compact menu control stands between the
	// platform inset and Home, so every left-region index moves by one.
	constexpr auto strip_lead_actions = qsizetype{ 1 };
#else
	constexpr auto strip_lead_actions = qsizetype{ 0 };
#endif

	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// [[BPR-1242]]. RFC-BPR-0023 gave the Notes tab two pages, Private
	/// history first and Agent tools second; the agent surfaces are on the
	/// second, so a course that inspects them opens it as a reviewer would.
	void show_agent_tools(qt::workbench_window& aWindow)
	{
		auto* const pages = aWindow.findChild<QTabWidget*>(QStringLiteral("privateConversationTabs"));
		expect(pages != nullptr && pages->indexOf(aWindow.findings_tab_scroll()) >= 0,
			   "the Notes tab offers its Agent tools page");
		pages->setCurrentWidget(aWindow.findings_tab_scroll());
		QApplication::processEvents();
	}

	void test_settings_collection_limit_keeps_accepted_models_and_recovery_actions()
	{
		auto window = qt::workbench_window{ };
		window.set_queue_rule_dispatch([](application::queue_rule_intent) { });
		window.set_saved_repository_dispatch([](application::saved_repository_intent) { });
		window.set_saved_server_dispatch([](application::saved_server_intent) { });
		auto projection = application::queue_landing_projection{
			.Status  = application::queue_landing_status::ready,
			.Session = application::queue_session_id{ .Host = "github.com", .Account = "reviewer", .Generation = 1U }
		};
		for (auto i = std::size_t{ 0U }; i < application::maximum_queue_rules; ++i) {
			projection.Rules.push_back(core::observer_rule{ .Pattern = "fixture-" + std::to_string(i) });
		}
		for (auto i = std::size_t{ 0U }; i < application::maximum_saved_repositories; ++i) {
			projection.SavedRepositories.push_back(application::saved_repository{ .Host = "github.com", .Account = "reviewer", .Repository = "fixture/repo-" + std::to_string(i) });
		}
		for (auto i = std::size_t{ 0U }; i < application::maximum_saved_servers; ++i) {
			projection.SavedServers.push_back(application::saved_server{ .Hostname = "fixture-" + std::to_string(i) + ".example.com" });
		}
		const auto widgets = window.findChildren<QWidget*>().size();
		window.publish(projection);
		auto* rules        = window.findChild<QComboBox*>(QStringLiteral("observerRuleSelector"));
		auto* repositories = window.findChild<QComboBox*>(QStringLiteral("savedRepositorySelector"));
		// [[BPR-0774]]. A kept server has no list of its own: it is its host's
		// card, and a kept server with no configured host profile adds nothing.
		expect(rules->count() == static_cast<int>(application::maximum_queue_rules)
				   && repositories->count() == static_cast<int>(application::maximum_saved_repositories)
				   && window.kept_servers() == projection.SavedServers
				   && window.server_cards().empty()
				   && window.findChildren<QWidget*>().size() <= widgets + 4,
			   "maximum admitted Settings collections use bounded models, and kept servers add no widgets or cards of their own");
		auto* const accounts = window.settings()->page_widget(qt::settings_page::accounts_and_hosts);
		auto* const add      = window.add_enterprise_server_button();
		expect(accounts != nullptr && add != nullptr && accounts->isAncestorOf(add) && add->isVisibleTo(accounts),
			   "with the queue ready, Accounts and hosts offers Add enterprise server");
		const auto ruleIndex       = QPersistentModelIndex{ rules->model()->index(0, 0) };
		const auto repositoryIndex = QPersistentModelIndex{ repositories->model()->index(0, 0) };
		projection.Failure         = application::queue_store_failure::collection_limit;
		window.publish(projection);
		expect(ruleIndex.isValid() && repositoryIndex.isValid()
				   && window.remove_saved_repository_action()->isEnabled()
				   && window.kept_servers() == projection.SavedServers
				   && add->isVisibleTo(accounts),
			   "limit refusal preserves accepted model identities, the remove action, the kept servers and Add enterprise server");
		auto* status = window.settings()->findChild<QLabel*>(QStringLiteral("settingsQueueStatus"));
		expect(status != nullptr && status->text().contains(QStringLiteral("limit")),
			   "the Settings page explains the typed collection refusal");
		projection.Failure.reset();
		window.publish(projection);
		expect(!status->text().contains(QStringLiteral("limit")) && ruleIndex.isValid(),
			   "recovery clears the refusal without rebuilding unchanged models");
	}

	void test_settings_share_only_offered_agent_context_and_maintenance_actions()
	{
		auto  window  = qt::workbench_window{ };
		auto* panel   = window.agent_panel();
		auto  intents = std::vector<qt::agent_intent>{ };
		panel->set_dispatch([&](qt::agent_intent aIntent) { intents.push_back(std::move(aIntent)); });
		const auto profiles = std::array{
			core::agent_profile{ .Id = core::agent_profile_id{ 1U }, .DisplayName = "First" },
			core::agent_profile{ .Id = core::agent_profile_id{ 2U }, .DisplayName = "Second" }
		};
		panel->offer_profiles(profiles);
		panel->disclose_scope(QStringLiteral("Current review metadata"));
		const auto notes = std::array{ application::local_note_id{ 7U }, application::local_note_id{ 9U } };
		const auto draft = application::agent_draft_choice{ .Id = core::review_draft_id{ 11U }, .Revision = 3U };
		panel->offer_private_context(notes, draft);
		auto* page     = window.settings()->page_widget(qt::settings_page::agents_and_context);
		auto* selector = page->findChild<QComboBox*>(QStringLiteral("settingsAgentProfile"));
		auto* noteList = page->findChild<QListView*>(QStringLiteral("settingsAgentNotes"));
		auto* draftBox = page->findChild<QCheckBox*>(QStringLiteral("settingsAgentDraft"));
		expect(selector != nullptr && noteList != nullptr && draftBox != nullptr,
			   "Settings mounts the agent choices");
		expect(selector->model() == panel->profile_selector()->model()
				   && noteList->model() == panel->note_selector()->model()
				   && !draftBox->isChecked() && panel->note_selector()->item(0)->checkState() == Qt::Unchecked,
			   "Settings shares only host-offered identities with private context unchecked");
		selector->setCurrentIndex(1);
		noteList->model()->setData(noteList->model()->index(1, 0), Qt::Checked, Qt::CheckStateRole);
		draftBox->click();
		expect(intents.empty(), "editing Settings never starts an agent");
		panel->start_action()->trigger();
		expect(intents.size() == 1U, "the existing explicit start emits one intent");
		const auto& start = std::get<qt::start_agent_intent>(intents.front());
		expect(start.Profile == profiles[1].Id
				   && start.IncludedNotes == std::vector{ notes[1] } && start.IncludedDraft == draft,
			   "the request carries exactly the profile and private identities selected in Settings");
		panel->offer_profiles(std::span{ profiles }.first(1));
		panel->offer_private_context(std::span{ notes }.first(1), std::nullopt);
		expect(selector->count() == 1 && noteList->model()->rowCount() == 1
				   && !draftBox->isChecked() && !draftBox->isEnabled()
				   && panel->note_selector()->item(0)->checkState() == Qt::Unchecked,
			   "a changed offer removes old choices and clears private opt-ins");
		panel->set_running(true);
		expect(!noteList->isEnabled(), "private context is disabled during a request");
		window.set_storage_location(QStringLiteral("/fixture/local-data"));
		auto* storage  = window.settings()->page_widget(qt::settings_page::storage_and_privacy);
		auto* location = storage->findChild<QLineEdit*>(QStringLiteral("settingsStorageLocation"));
		expect(location != nullptr && location->isReadOnly() && location->text() == QStringLiteral("/fixture/local-data"),
			   "storage location is a read-only composition fact");
		for (auto* action : { window.epoch_selectors()->prune_action(), window.epoch_selectors()->delete_cache_action(), window.epoch_selectors()->cancel_maintenance_action() }) {
			const auto buttons = storage->findChildren<QToolButton*>();
			expect(std::ranges::any_of(buttons, [action](auto* aButton) { return aButton->defaultAction() == action; }),
				   "Storage uses the existing command-owned maintenance action");
		}
	}

	void send_click(QWidget& aSurface, const QPointF aPosition)
	{
		auto press = QMouseEvent{
			QEvent::MouseButtonPress,
			aPosition,
			aPosition,
			aPosition,
			Qt::LeftButton,
			Qt::LeftButton,
			Qt::NoModifier,
		};
		auto release = QMouseEvent{
			QEvent::MouseButtonRelease,
			aPosition,
			aPosition,
			aPosition,
			Qt::LeftButton,
			Qt::NoButton,
			Qt::NoModifier,
		};
		QCoreApplication::sendEvent(&aSurface, &press);
		QCoreApplication::sendEvent(&aSurface, &release);
	}

	/// \brief Records one reported host-owned drawer width.
	struct width_report
	{
		core::workbench_drawer Drawer = core::workbench_drawer::files;
		std::uint32_t          Width  = 0U;
	};

	/// \brief Records the semantic intents one window emitted.
	struct intent_log
	{
		std::vector<core::workbench_interaction_intent> Workbench;
		std::vector<core::navigate_diff_change_intent>  Change;
		std::vector<core::file_receipt_command>         Receipt;
		std::vector<width_report>                       Width;
	};

	/// \brief Counts the model signals one drawer model actually emitted.
	struct model_signal_log
	{
		std::size_t Inserted = 0U;
		std::size_t Removed  = 0U;
		std::size_t Moved    = 0U;
		std::size_t Changed  = 0U;
		std::size_t Layout   = 0U;
		std::size_t Reset    = 0U;
	};

	void observe_model_signals(
		const QAbstractItemModel& aModel,
		model_signal_log&         aLog)
	{
		QObject::connect(&aModel,
						 &QAbstractItemModel::rowsInserted,
						 &aModel,
						 [&aLog](const QModelIndex&, int, int) { ++aLog.Inserted; });
		QObject::connect(&aModel,
						 &QAbstractItemModel::rowsRemoved,
						 &aModel,
						 [&aLog](const QModelIndex&, int, int) { ++aLog.Removed; });
		QObject::connect(
			&aModel,
			&QAbstractItemModel::rowsMoved,
			&aModel,
			[&aLog](const QModelIndex&, int, int, const QModelIndex&, int) {
				++aLog.Moved;
			});
		QObject::connect(
			&aModel,
			&QAbstractItemModel::dataChanged,
			&aModel,
			[&aLog](const QModelIndex&, const QModelIndex&, const QList<int>&) {
				++aLog.Changed;
			});
		QObject::connect(&aModel,
						 &QAbstractItemModel::modelAboutToBeReset,
						 &aModel,
						 [&aLog] { ++aLog.Reset; });
		QObject::connect(
			&aModel,
			&QAbstractItemModel::layoutChanged,
			&aModel,
			[&aLog](
				const QList<QPersistentModelIndex>&,
				QAbstractItemModel::LayoutChangeHint) { ++aLog.Layout; });
	}

	[[nodiscard]] auto drawer_row(
		const char* const aIdentity,
		const char* const aTitle,
		const char* const aDetail) -> qt::workbench_drawer_row
	{
		return {
			.Identity = QString::fromLatin1(aIdentity),
			.Title    = QString::fromLatin1(aTitle),
			.Detail   = QString::fromLatin1(aDetail),
		};
	}

	[[nodiscard]] constexpr auto workbench_file(const std::uint8_t aSeed)
		-> core::workbench_file_id
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0x5AU);
		return core::workbench_file_id{ bytes };
	}

	[[nodiscard]] constexpr auto file_material(const std::uint8_t aSeed)
		-> core::file_material_id
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return core::file_material_id{ bytes };
	}

	[[nodiscard]] constexpr auto comparison_digest(const std::uint8_t aSeed)
		-> core::file_comparison_digest
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0x3CU);
		return core::file_comparison_digest{ bytes };
	}

	[[nodiscard]] constexpr auto defect_history(
		const std::uint64_t                    aId,
		const core::defect_anchor_availability aAvailability) -> core::defect_history
	{
		const auto anchor = core::source_line_ref{
			.Comparison = comparison_digest(static_cast<std::uint8_t>(aId)),
			.Side       = core::diff_side::right,
			.Line       = static_cast<std::uint32_t>(aId + 10U),
		};
		return {
			.Defect = core::defect_record{
				.Id     = core::defect_id{ aId },
				.Anchor = anchor,
				.State  = core::defect_state::needs_verification,
			},
			.OriginalAnchor = anchor,
			.Availability   = aAvailability,
		};
	}

	[[nodiscard]] auto drawer_states(
		const core::workbench_drawer_state aFiles,
		const core::workbench_drawer_state aConversations)
		-> core::workbench_drawer_delta
	{
		return { .Files = aFiles, .Conversations = aConversations };
	}

	[[nodiscard]] auto command_state(
		const core::workbench_command_id  aId,
		const bool                        aEnabled,
		const core::workbench_reason_code aReason)
		-> core::workbench_command_projection
	{
		return { .Id = aId, .Enabled = aEnabled, .DisabledReason = aReason };
	}

	[[nodiscard]] auto offered(const core::workbench_command_id aId)
		-> core::workbench_command_projection
	{
		return command_state(aId, true, core::workbench_reason_code::none);
	}

	[[nodiscard]] auto refused(const core::workbench_command_id  aId,
							   const core::workbench_reason_code aReason)
		-> core::workbench_command_projection
	{
		return command_state(aId, false, aReason);
	}

	/// \brief Builds one core command delta with every entry in projection order.
	///
	/// \details Entry `i` projects `core::workbench_command_id{ i }`, exactly as
	/// the core publishes it, so a window that read the wrong entry would answer
	/// with the wrong command's identity rather than silently agreeing.
	[[nodiscard]] auto command_delta(
		const core::workbench_command_projection& aPreviousFile,
		const core::workbench_command_projection& aNextFile,
		const core::workbench_command_projection& aPreviousChange,
		const core::workbench_command_projection& aNextChange,
		const core::change_navigation_policy      aPolicy
		= core::change_navigation_policy::stop) -> core::workbench_command_delta
	{
		return {
			.Commands = {
				aPreviousFile,
				aNextFile,
				aPreviousChange,
				aNextChange,
				offered(core::workbench_command_id::toggle_files_drawer),
				offered(core::workbench_command_id::toggle_conversations_drawer),
				offered(core::workbench_command_id::toggle_line_numbers),
				offered(core::workbench_command_id::toggle_repositories_drawer),
				offered(core::workbench_command_id::toggle_one_pane),
			},
			.Policy = aPolicy,
		};
	}

	/// \brief Projects file legality while no comparison is open.
	[[nodiscard]] auto file_commands(
		const core::workbench_command_projection& aPrevious,
		const core::workbench_command_projection& aNext)
		-> application::workbench_projection
	{
		return {
			.Commands = command_delta(
				aPrevious,
				aNext,
				refused(core::workbench_command_id::previous_change,
						core::workbench_reason_code::no_open_comparison),
				refused(core::workbench_command_id::next_change,
						core::workbench_reason_code::no_open_comparison)),
		};
	}

	/// \brief [[BPR-0721]]. The preference that gives a drawer its rail and
	/// Hide button back; off by default, so the tests that pin the rail ask.
	[[nodiscard]] auto drawer_controls_shown() -> application::review_behaviour_settings
	{
		return application::review_behaviour_settings{
			.DrawerControls = application::review_drawer_controls::shown,
		};
	}

	[[nodiscard]] auto both_files_enabled() -> application::workbench_projection
	{
		return file_commands(offered(core::workbench_command_id::previous_file),
							 offered(core::workbench_command_id::next_file));
	}

	/// \brief Projects every primary command as offered under one policy.
	/// \brief [[BPR-1130]]. Publishes the fold the core decided.
	void publish_updates_line(qt::workbench_window& aWindow, const core::workbench_updates_line aState)
	{
		auto fold        = application::workbench_projection{ };
		fold.UpdatesLine = core::workbench_updates_line_delta{ .State = aState };
		aWindow.publish(fold);
		QApplication::processEvents();
		QApplication::processEvents();
	}

	void open_updates_line(qt::workbench_window& aWindow)
	{
		publish_updates_line(aWindow, core::workbench_updates_line::open);
	}

	void fold_updates_line(qt::workbench_window& aWindow)
	{
		publish_updates_line(aWindow, core::workbench_updates_line::folded);
	}

	/// \brief Saves a window render beside the suite's other renders, when the
	/// run names a render path.
	void save_render(qt::workbench_window& aWindow, const QString& aSuffix)
	{
		const auto rendered = aWindow.grab();
		expect(!rendered.isNull(), "the window renders");
		if (const auto path = qEnvironmentVariable("BPR_QT_RENDER_PATH"); !path.isEmpty()) {
			const QFileInfo file{ path };
			expect(rendered.save(QStringLiteral("%1/%2-%3.png").arg(file.path(), file.completeBaseName(), aSuffix), "PNG"),
				   "the render is saved for inspection");
		}
	}

	[[nodiscard]] auto every_primary_command_offered(
		const core::change_navigation_policy aPolicy
		= core::change_navigation_policy::stop)
		-> application::workbench_projection
	{
		return {
			.Commands = command_delta(
				offered(core::workbench_command_id::previous_file),
				offered(core::workbench_command_id::next_file),
				offered(core::workbench_command_id::previous_change),
				offered(core::workbench_command_id::next_change),
				aPolicy),
		};
	}

	[[nodiscard]] auto navigation_actions(const qt::workbench_window& aWindow)
		-> std::array<QAction*, 4>
	{
		return {
			aWindow.previous_file_action(),
			aWindow.next_file_action(),
			aWindow.previous_change_action(),
			aWindow.next_change_action(),
		};
	}

	[[nodiscard]] auto file_direction(
		const core::workbench_interaction_intent& aIntent)
		-> std::optional<core::change_navigation_direction>
	{
		const auto* const navigate = std::get_if<
			core::navigate_workbench_file_intent>(&aIntent);
		return navigate == nullptr
			? std::nullopt
			: std::optional{ navigate->Direction };
	}

	void test_all_four_primary_actions_follow_one_core_projection()
	{
		qt::workbench_window window;

		// Four different decisions in one delta. An action that read any entry
		// but its own, or that kept a second carrier for change legality, cannot
		// reproduce this pattern.
		const auto mixed = command_delta(
			refused(core::workbench_command_id::previous_file,
					core::workbench_reason_code::at_first_file),
			offered(core::workbench_command_id::next_file),
			offered(core::workbench_command_id::previous_change),
			refused(core::workbench_command_id::next_change,
					core::workbench_reason_code::at_last_change));
		window.publish({ .Commands = mixed });

		const auto actions = navigation_actions(window);
		for (std::size_t index = 0U; index < actions.size(); ++index) {
			expect(mixed.Commands[index].Id
					   == static_cast<core::workbench_command_id>(index),
				   "the published delta projects one entry per command in order");
			expect(actions[index]->isEnabled() == mixed.Commands[index].Enabled,
				   "every primary action follows its own entry of the one core "
				   "command projection");
		}
		expect(window.previous_file_action()->toolTip()
					   == QStringLiteral("This is the first actionable file.")
				   && window.next_change_action()->toolTip()
					   == QStringLiteral(
						   "This is the last change in this comparison."),
			   "each disabled primary action carries its own exact core reason");

		// The same delta shape decides the reverse pair, so no action can be
		// following a constant that merely agreed with the first publication.
		const auto reversed = command_delta(
			offered(core::workbench_command_id::previous_file),
			refused(core::workbench_command_id::next_file,
					core::workbench_reason_code::at_last_file),
			refused(core::workbench_command_id::previous_change,
					core::workbench_reason_code::at_first_change),
			offered(core::workbench_command_id::next_change));
		window.publish({ .Commands = reversed });
		for (std::size_t index = 0U; index < actions.size(); ++index) {
			expect(actions[index]->isEnabled() == reversed.Commands[index].Enabled,
				   "a replacement projection moves every primary action with it");
		}
		expect(window.previous_change_action()->toolTip()
					   == QStringLiteral(
						   "This is the first change in this comparison.")
				   && window.previous_change_action()->statusTip()
					   == window.previous_change_action()->toolTip(),
			   "the change reason reaches the tooltip and the status tip");
	}

	void test_every_primary_action_translates_to_a_semantic_intent()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			[&log](core::navigate_diff_change_intent aIntent) {
				log.Change.push_back(aIntent);
			}
		};

		window.publish(every_primary_command_offered());
		expect(window.previous_file_action()->isEnabled()
				   && window.next_file_action()->isEnabled()
				   && window.previous_change_action()->isEnabled()
				   && window.next_change_action()->isEnabled(),
			   "an enabling projection enables every primary action");

		window.previous_file_action()->trigger();
		window.next_file_action()->trigger();
		window.previous_change_action()->trigger();
		window.next_change_action()->trigger();

		expect(log.Workbench.size() == 2U && log.Change.size() == 2U,
			   "each primary action emits exactly one semantic intent");
		expect(file_direction(log.Workbench[0])
					   == core::change_navigation_direction::previous
				   && file_direction(log.Workbench[1])
					   == core::change_navigation_direction::next,
			   "file actions emit navigate-workbench-file intents by direction");
		expect(log.Change[0].Direction
					   == core::change_navigation_direction::previous
				   && log.Change[1].Direction
					   == core::change_navigation_direction::next,
			   "change actions emit navigate-diff-change intents by direction");
		expect(log.Change[0].Policy == core::change_navigation_policy::stop
				   && log.Change[1].Policy == core::change_navigation_policy::stop,
			   "the emitted change intent repeats the decided navigation policy");

		window.publish(
			every_primary_command_offered(core::change_navigation_policy::wrap));
		window.next_change_action()->trigger();
		expect(log.Change.size() == 3U
				   && log.Change[2].Policy == core::change_navigation_policy::wrap,
			   "a changed decided policy travels with the next emitted gesture");
	}

	void test_action_state_follows_core_command_projections()
	{
		qt::workbench_window window;

		expect(!window.previous_file_action()->isEnabled()
				   && !window.next_file_action()->isEnabled()
				   && !window.previous_change_action()->isEnabled()
				   && !window.next_change_action()->isEnabled(),
			   "no published projection offers no navigation gesture");
		expect(window.previous_file_action()->toolTip()
				   == QStringLiteral("Open a pull request to review its files."),
			   "an unprojected file action carries the inactive-workbench reason");
		expect(window.previous_change_action()->toolTip()
				   == QStringLiteral("Open a pull request to review its changes."),
			   "an unprojected change action carries the inactive-workbench reason "
			   "in its own words");

		window.publish(file_commands(
			refused(core::workbench_command_id::previous_file,
					core::workbench_reason_code::at_first_file),
			offered(core::workbench_command_id::next_file)));
		expect(window.previous_change_action()->toolTip()
				   == QStringLiteral("No file comparison is open."),
			   "an active workbench with no open comparison says so exactly");
		expect(!window.previous_file_action()->isEnabled()
				   && window.next_file_action()->isEnabled(),
			   "QAction enabled state follows the core command projection");
		expect(window.previous_file_action()->toolTip()
				   == QStringLiteral("This is the first actionable file."),
			   "a disabled file action's tooltip carries its exact core reason");
		expect(window.previous_file_action()->statusTip()
				   == window.previous_file_action()->toolTip(),
			   "the disabled file reason also reaches the status tip");
		expect(window.next_file_action()->toolTip()
				   == QStringLiteral("Select the next actionable file."),
			   "an enabled file action describes the gesture it offers");

		window.publish(file_commands(
			refused(core::workbench_command_id::previous_file,
					core::workbench_reason_code::no_actionable_files),
			refused(core::workbench_command_id::next_file,
					core::workbench_reason_code::at_last_file)));
		expect(window.previous_file_action()->toolTip()
				   == QStringLiteral("This pull request has no actionable files."),
			   "a different core reason produces a different exact file reason");
		expect(window.next_file_action()->toolTip()
					   == QStringLiteral("This is the last actionable file.")
				   && window.next_file_action()->statusTip()
					   == window.next_file_action()->toolTip(),
			   "each disabled file action carries its own exact core reason");

		window.publish({
			.Commands = command_delta(
				offered(core::workbench_command_id::previous_file),
				offered(core::workbench_command_id::next_file),
				refused(core::workbench_command_id::previous_change,
						core::workbench_reason_code::at_first_change),
				offered(core::workbench_command_id::next_change)),
		});
		expect(!window.previous_change_action()->isEnabled()
				   && window.next_change_action()->isEnabled(),
			   "QAction enabled state follows the same core command projection "
			   "for change navigation");
		expect(window.previous_change_action()->toolTip()
					   == QStringLiteral("This is the first change in this comparison.")
				   && window.previous_change_action()->statusTip()
					   == window.previous_change_action()->toolTip(),
			   "a disabled change action's tooltip carries its exact core reason");

		window.publish({
			.Commands = command_delta(
				offered(core::workbench_command_id::previous_file),
				refused(core::workbench_command_id::next_file,
						core::workbench_reason_code::at_last_file),
				refused(core::workbench_command_id::previous_change,
						core::workbench_reason_code::no_changes),
				refused(core::workbench_command_id::next_change,
						core::workbench_reason_code::no_changes)),
		});
		expect(window.previous_change_action()->toolTip()
				   == QStringLiteral(
					   "This comparison has no changes to navigate."),
			   "a different core reason produces a different exact change reason");

		const auto before = window.statistics();
		window.fail({ .Failure
					  = application::workbench_host_failure::observation_limit });
		expect(window.statistics().Failures == before.Failures + 1U,
			   "a host failure is observed by the window");
		expect(window.next_file_action()->toolTip()
					   == QStringLiteral("This is the last actionable file.")
				   && !window.next_file_action()->isEnabled(),
			   "a host failure does not let the window re-decide command legality");
		expect(window.status_text().contains(QStringLiteral(
				   "more actionable files than the workbench retains")),
			   "the host failure is explicit in status text");
	}

	/// \brief [[BPR-1129]]. RFC-BPR-0052's chrome: the title row carries the
	/// window, the pull request and the modality, and the review bar under it
	/// carries what acts on the review, each in its exact persistent order.
	void test_compact_icon_row_preserves_exact_order_and_edges()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			[&log](core::navigate_diff_change_intent aIntent) {
				log.Change.push_back(aIntent);
			}
		};
		window.publish(every_primary_command_offered());
		window.resize(1'280, 720);
		window.show();
		QApplication::processEvents();

		auto* const toolbar       = window.primary_toolbar();
		auto* const bar           = window.review_bar();
		auto* const files         = window.files_drawer_toggle_action();
		auto* const conversations = window.conversations_drawer_toggle_action();
		const auto  navigation    = navigation_actions(window);
		const auto  actions       = toolbar->actions();
		const auto  rows          = window.findChildren<QToolBar*>();
		expect(rows.size() == 2 && rows.contains(toolbar) && rows.contains(bar) && bar != toolbar,
			   "the review surface has exactly two rows of controls: the title row and the review bar");
		const auto named = [](const QList<QAction*>& aActions, const qsizetype aIndex, const char* const aName) {
			return aIndex >= 0 && aIndex < aActions.size() && aActions[aIndex]->objectName() == QLatin1String{ aName };
		};
		// [[BPR-1129]]. The title row, left to right: the platform inset, Home,
		// Repositories, Files, the name and a spacer; then the modality, Home's
		// own three controls (hidden outside Home), refresh, Home's discovery
		// refresh (hidden outside Home), comments, a rule, account, settings.
#if defined(_WIN32)
		// [[BPR-0653]]. On Windows the title row carries the compact menu
		// control after the inset, and the caption buttons end it.
		const auto lead  = qsizetype{ 1 };
		const auto trail = qsizetype{ 1 };
		expect(named(actions, 1, "workbenchStripMenuAction") && named(actions, actions.size() - 1, "workbenchCaptionButtonsAction"),
			   "the Windows title row opens with its menu control and ends with the caption buttons");
#else
		const auto lead  = qsizetype{ 0 };
		const auto trail = qsizetype{ 0 };
#endif
		const auto titleRow = actions.size() == 16 + lead + trail
			&& named(actions, 0, "workbenchStripInsetAction")
			&& actions[1 + lead] == window.home_action()
			&& actions[2 + lead] == window.repositories_drawer_toggle_action()
			&& actions[3 + lead] == files
			&& named(actions, 4 + lead, "workbenchStripIdentityAction")
			&& named(actions, 5 + lead, "workbenchNavigationSpacerAction")
			&& named(actions, 6 + lead, "workbenchModalityAction")
			&& named(actions, 7 + lead, "homeGroupAction")
			&& named(actions, 8 + lead, "homeStateHeadersAction")
			&& named(actions, 9 + lead, "homeFilterAction")
			&& actions[10 + lead] == window.github_refresh_action()
			&& actions[11 + lead] == window.queue_refresh_action()
			&& actions[12 + lead] == conversations
			&& named(actions, 13 + lead, "workbenchStripRightDividerAction")
			&& actions[14 + lead] == window.account_action()
			&& actions[15 + lead] == window.settings_action();
		if (!titleRow) {
			for (auto index = qsizetype{ 0 }; index < actions.size(); ++index) {
				std::cerr << "title row " << index << ": " << actions[index]->objectName().toStdString() << '\n';
			}
		}
		expect(titleRow, "the title row holds exactly the window, the pull request and the modality, in order");
		for (auto* const action : { navigation[0], navigation[1], navigation[2], navigation[3], window.review_details_toggle_action(),
									window.compose_review_action(), window.comment_action(), window.next_new_comment_action() }) {
			expect(!actions.contains(action), "no navigation, progress or composition control sits in the title row");
		}
		expect(!toolbar->isAncestorOf(window.review_controls_button()) && bar->isAncestorOf(window.review_controls_button()),
			   "private progress is the review bar's, not the title row's");
		expect(toolbar->height() == qt::workbench_strip::height_pixels,
			   "the title row is the design's 30-pixel row");

		// [[BPR-1129]]. The review bar, left to right: an inset, Updates, the
		// comparison's menu and a rule, the file pair and its position, the
		// change pair and its position, private progress, a spacer, Compose
		// review and Comment.
		const auto barActions         = bar->actions();
		const auto matchesReviewOrder = [&](const QList<QAction*>& aActions) {
			const auto& ordered = aActions;
			return ordered.size() == 14
				&& named(ordered, 0, "reviewBarInsetAction")
				&& named(ordered, 1, "reviewBarUpdatesAction")
				&& named(ordered, 2, "reviewBarComparingAction")
				&& named(ordered, 3, "reviewBarComparisonDividerAction")
				&& named(ordered, 4, "workbenchStripFilePositionAction")
				&& ordered[5] == navigation[0]
				&& ordered[6] == navigation[1]
				&& named(ordered, 7, "workbenchStripChangePositionAction")
				&& ordered[8] == navigation[2]
				&& ordered[9] == navigation[3]
				&& named(ordered, 10, "workbenchReviewControlsAction")
				&& named(ordered, 11, "reviewBarSpacerAction")
				&& ordered[12] == window.compose_review_action()
				&& ordered[13] == window.comment_action();
		};
		const auto reviewRow   = matchesReviewOrder(barActions);
		auto       labelsAfter = barActions;
		std::rotate(labelsAfter.begin() + 4, labelsAfter.begin() + 5, labelsAfter.begin() + 7);
		std::rotate(labelsAfter.begin() + 7, labelsAfter.begin() + 8, labelsAfter.begin() + 10);
		expect(!matchesReviewOrder(labelsAfter), "negative control: labels after the arrow pairs fail the review-bar order");
		const auto rails = bar->grab().toImage();
		const auto scale = rails.devicePixelRatio();
		for (const auto start : { 4, 7 }) {
			const auto first = bar->actionGeometry(barActions[start]);
			const auto last  = bar->actionGeometry(barActions[start + 2]);
			const auto ink   = bar->palette().color(QPalette::Active, QPalette::Highlight);
			for (const auto x : { first.left() + 3, (first.left() + last.right()) / 2, last.right() - 3 }) {
				expect(rails.pixelColor(qRound(x * scale), qRound((bar->height() - 2) * scale)) == ink,
					   "a continuous palette-derived rail joins each label and both arrows");
			}
		}

		auto oldOrder = barActions;
		std::rotate(oldOrder.begin() + 4, oldOrder.begin() + 7, oldOrder.begin() + 10);
		expect(!matchesReviewOrder(oldOrder), "negative control: the old change-before-file order fails the exact row contract");
		if (!reviewRow) {
			for (auto index = qsizetype{ 0 }; index < barActions.size(); ++index) {
				std::cerr << "review bar " << index << ": " << barActions[index]->objectName().toStdString() << '\n';
			}
		}
		expect(reviewRow, "the review bar reads Updates and the comparison, then the file and change navigation and progress, with Compose review and Comment at its right");
		expect(bar->height() == qt::workbench_strip::review_bar_height_pixels,
			   "the review bar is the design's 28-pixel row");
		expect(window.updates_disclosure_button()->text() == QStringLiteral("Updates")
				   && bar->widgetForAction(barActions[1]) == window.updates_disclosure_button()
				   && bar->widgetForAction(barActions[2]) == window.epoch_selectors()->comparing_button()
				   && window.epoch_selectors()->comparing_button()->text().startsWith(QStringLiteral("Comparing")),
			   "Updates names the line, and the comparison's own menu follows it");
		expect(window.epoch_selectors()->comparing_button()->menu()->actions().contains(window.review_details_toggle_action()),
			   "Review details is reached from the comparison's menu");

		const auto required = std::array{
			std::pair{ files, toolbar },
			std::pair{ navigation[0], bar },
			std::pair{ navigation[1], bar },
			std::pair{ navigation[2], bar },
			std::pair{ navigation[3], bar },
			std::pair{ conversations, toolbar },
			std::pair{ window.compose_review_action(), bar },
			std::pair{ window.comment_action(), bar },
		};
		for (const auto& [action, row] : required) {
			auto* const button = qobject_cast<QToolButton*>(row->widgetForAction(action));
			expect(button != nullptr && button->isVisible()
					   && button->toolButtonStyle() == Qt::ToolButtonIconOnly
					   && !button->icon().isNull(),
				   "every drawer, navigation and composition presenter is visible, icon-only and has its icon");
			expect(button->focusPolicy() == Qt::StrongFocus
					   && !button->accessibleName().isEmpty()
					   && !button->accessibleDescription().isEmpty(),
				   "every required presenter is focusable, named, and described");
			expect(window.actions().contains(action) && !action->shortcut().isEmpty()
					   && action->shortcutContext() == Qt::WindowShortcut,
				   "every required action retains its stable window shortcut");
			expect(action->toolTip().contains(action->shortcut().toString(QKeySequence::NativeText))
					   || action == files || action == conversations || std::ranges::find(navigation, action) != navigation.end(),
				   "Compose review's and Comment's tooltips name the command and its key");
		}
		expect(window.compose_review_action()->shortcut() == QKeySequence{ QStringLiteral("Alt+R") }
				   && window.comment_action()->shortcut() == QKeySequence{ QStringLiteral("Alt+C") }
				   && window.compose_review_action()->toolTip().startsWith(QStringLiteral("Compose review"))
				   && window.comment_action()->toolTip().startsWith(QStringLiteral("Comment")),
			   "Option-R (Alt+R) composes the review and Option-C (Alt+C) comments");
		expect(window.pull_request_menu()->actions().contains(window.compose_review_action())
				   && window.pull_request_menu()->actions().contains(window.comment_action()),
			   "both composition commands are in the Pull Request menu, with their keys");

		auto* const spacer = toolbar->findChild<QWidget*>(QStringLiteral("workbenchNavigationSpacer"));
		expect(spacer != nullptr && spacer->width() > 0
				   && spacer->x() > window.identity_label()->x()
				   && window.modality()->x() > spacer->x()
				   && toolbar->widgetForAction(window.github_refresh_action())->x() > window.modality()->x()
				   && toolbar->widgetForAction(conversations)->x()
					   > toolbar->widgetForAction(window.github_refresh_action())->x(),
			   "the modality sits at the title row's right, beside refresh and the comments toggle");
		auto* const barSpacer = bar->findChild<QWidget*>(QStringLiteral("reviewBarSpacer"));
		auto* const compose   = bar->widgetForAction(window.compose_review_action());
		auto* const comment   = bar->widgetForAction(window.comment_action());
		expect(barSpacer != nullptr && barSpacer->width() > 0
				   && barSpacer->x() > window.review_controls_button()->x()
				   && compose->x() > barSpacer->x() && comment->x() > compose->x()
				   && comment->geometry().right() > bar->width() - 12,
			   "Compose review and Comment sit at the review bar's right, after its spacer");

		auto* const reviewButton = window.review_controls_button();
		auto* const reviewMenu   = window.review_controls_menu();
		expect(reviewButton != nullptr && reviewButton->menu() == reviewMenu
				   && reviewButton->toolButtonStyle() == Qt::ToolButtonIconOnly
				   && reviewButton->focusPolicy() == Qt::StrongFocus
				   && !reviewButton->shortcut().isEmpty()
				   && reviewButton->toolTip().contains(reviewButton->shortcut().toString(QKeySequence::NativeText))
				   && reviewButton->toolTip().contains(QStringLiteral("Seen, Read, Accept or Exclude")),
			   "private progress is one keyboard-accessible icon popup whose tooltip names its key and its four actions");
		expect(reviewMenu->actions()
				   == QList<QAction*>{ window.seen_action(),
									   window.read_action(),
									   window.accept_action(),
									   window.exclude_action() },
			   "the popup reuses the four existing receipt actions without duplicating policy");
		const auto secondaryIcons = std::array{
			files->icon().pixmap(QSize{ 24, 24 }).toImage(),
			conversations->icon().pixmap(QSize{ 24, 24 }).toImage(),
			window.review_details_toggle_action()->icon().pixmap(QSize{ 24, 24 }).toImage(),
			reviewButton->icon().pixmap(QSize{ 24, 24 }).toImage(),
			window.compose_review_action()->icon().pixmap(QSize{ 24, 24 }).toImage(),
			window.comment_action()->icon().pixmap(QSize{ 24, 24 }).toImage(),
		};
		for (std::size_t first = 0U; first < secondaryIcons.size(); ++first) {
			for (std::size_t second = first + 1U; second < secondaryIcons.size(); ++second) {
				expect(!secondaryIcons[first].isNull() && secondaryIcons[first] != secondaryIcons[second],
					   "drawer, details, progress, compose and comment symbols remain visually distinct");
			}
		}

		for (const auto type : { QEvent::PaletteChange,
								 QEvent::FontChange,
								 QEvent::StyleChange,
								 QEvent::ThemeChange }) {
			for (const auto& [action, row] : required) {
				static_cast<void>(row);
				action->setIcon(QIcon{ });
			}
			reviewButton->setIcon(QIcon{ });
			auto appearanceChange = QEvent{ type };
			QApplication::sendEvent(&window, &appearanceChange);
			expect(std::ranges::all_of(required, [](const auto& aEntry) {
					   return !aEntry.first->icon().isNull();
				   }) && !reviewButton->icon().isNull(),
				   "every native appearance change refreshes every derived row icon");
		}

		window.resize(window.minimumWidth(), 720);
		QApplication::processEvents();
		for (const auto& [action, row] : required) {
			expect(row->actions().contains(action)
					   && row->widgetForAction(action)->isVisible()
					   && (action->isEnabled() || (action == window.comment_action() && action->toolTip().contains(QStringLiteral("Select a current-head")))),
				   "every drawer, navigation and composition control stays top level at the width floor with target-dependent enablement");
		}
		for (auto* const row : { static_cast<QToolBar*>(toolbar), static_cast<QToolBar*>(bar) }) {
			auto* const extension = row->findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button"));
			expect(extension == nullptr || !extension->isVisible(), "no control folds into an overflow menu at the width floor");
		}

		log.Workbench.clear();
		log.Change.clear();
		window.previous_file_action()->trigger();
		window.next_change_action()->trigger();
		expect(log.Workbench.size() == 1U && log.Change.size() == 1U,
			   "icon-only primary actions still translate to their semantic intents");
		window.hide();
	}

	void test_shell_accessibility_status_and_scale()
	{
		qt::workbench_window window;
		expect(!window.primary_toolbar()->accessibleName().isEmpty()
				   && !window.primary_toolbar()->accessibleDescription().isEmpty(),
			   "the primary toolbar carries an accessible name and description");
		expect(!window.review_bar()->accessibleName().isEmpty()
				   && !window.review_bar()->accessibleDescription().isEmpty(),
			   "the review bar carries an accessible name and description");
		for (auto* const action : navigation_actions(window)) {
			auto* const button = qobject_cast<QToolButton*>(
				window.review_bar()->widgetForAction(action));
			expect(button != nullptr && !button->accessibleName().isEmpty()
					   && !button->accessibleDescription().isEmpty(),
				   "every primary action carries an accessible name and "
				   "description");
			expect(button->focusPolicy() == Qt::StrongFocus,
				   "every primary action is keyboard focusable");
		}
		expect(!window.review_controls_button()->accessibleName().isEmpty()
				   && !window.review_controls_menu()
						   ->accessibleName()
						   .isEmpty(),
			   "the bounded secondary review popup carries accessible names");

		expect(window.files_drawer() != nullptr
				   && window.conversations_drawer() != nullptr
				   && window.epoch_selectors() != nullptr
				   && window.comparison_surface() != nullptr
				   && window.detail_pane() != nullptr,
			   "the shell names its composed epoch selectors, its composed "
			   "comparison surface, and its detail pane");

		// [[BPR-0775]]. An inactive workbench is Home, and Home's line is the
		// session's; the navigation it cannot offer is not on Home's strip.
		expect(window.showing_home()
				   && window.status_text()
					   == QStringLiteral("Not signed in. Public pull requests open read-only."),
			   "an inactive workbench is Home and its status line states the session");
		auto selection = both_files_enabled();
		selection.Selection
			= core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{
												   .Ordinal           = 3U,
												   .ActionableCount   = 12U,
												   .PreviousAvailable = true,
												   .NextAvailable     = true,
											   } };
		const auto before = window.statistics();
		window.publish(selection);
		expect(window.status_text()
				   == QStringLiteral("Actionable file 3 of 12."),
			   "the retained cursor is explicit in text as well as enabled state");
		expect(window.statistics().Announcements == before.Announcements + 1U,
			   "a published presentation change announces once");
		window.publish(selection);
		expect(window.statistics().Announcements == before.Announcements + 1U,
			   "an unchanged status is not announced again");

		window.resize(1'280, 720);
		window.show();
		QApplication::processEvents();
		const auto expected = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
		if (!expected.isEmpty()) {
			expect(std::abs(window.devicePixelRatioF() - expected.toDouble())
					   < 0.001,
				   "the window runs at the exact expected device scale");
		}
		expect(window.findChildren<QToolBar*>().size() == 2,
			   "every tested scale keeps the title row and the review bar, and no other row of controls");
		window.hide();
	}

	void test_toolbar_status_tips_do_not_resize_the_workbench()
	{
		qt::workbench_window window;
		window.resize(1'280, 720);
		window.show();
		QApplication::processEvents();

		auto* const statusBar       = window.statusBar();
		auto* const central         = window.centralWidget();
		const auto  statusBarHeight = statusBar->height();
		const auto  centralGeometry = central->geometry();
		const auto  retainedStatus  = window.status_text();
		auto        hover           = QStatusTipEvent{
			window.files_drawer_toggle_action()->statusTip()
		};
		QApplication::sendEvent(&window, &hover);
		QApplication::processEvents();

		expect(statusBar->currentMessage()
				   == window.files_drawer_toggle_action()->statusTip(),
			   "toolbar hover presents the action's temporary status message");
		expect(statusBar->height() == statusBarHeight
				   && central->geometry() == centralGeometry,
			   "toolbar hover leaves status-bar and central-surface geometry unchanged");

		auto leave = QStatusTipEvent{ QString{ } };
		QApplication::sendEvent(&window, &leave);
		QApplication::processEvents();
		expect(statusBar->currentMessage().isEmpty()
				   && window.status_text() == retainedStatus,
			   "leaving the toolbar restores the unchanged retained status");
		expect(statusBar->height() == statusBarHeight
				   && central->geometry() == centralGeometry,
			   "leaving the toolbar preserves the same stable geometry");
		window.hide();
	}

	void test_drawer_toggles_follow_core_command_projections()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			{ },
			[&log](core::workbench_drawer aDrawer, std::uint32_t aWidth) {
				log.Width.push_back({ .Drawer = aDrawer, .Width = aWidth });
			}
		};

		auto* const files         = window.files_drawer_toggle_action();
		auto* const conversations = window.conversations_drawer_toggle_action();
		expect(files->isCheckable() && conversations->isCheckable()
				   && !files->shortcut().isEmpty()
				   && !conversations->shortcut().isEmpty()
				   && window.actions().contains(files)
				   && window.actions().contains(conversations),
			   "each drawer toggle is a checkable window action with a shortcut");
		expect(window.primary_toolbar()->actions().indexOf(files) == 3 + strip_lead_actions
				   && window.primary_toolbar()->actions().indexOf(conversations)
					   > window.primary_toolbar()->actions().indexOf(
						   window.github_refresh_action()),
			   "Files opens the title row after the platform inset, Home and Repositories, and Conversations sits at its right after refresh");
		for (auto* const toggle : { files, conversations }) {
			auto* const button = qobject_cast<QToolButton*>(
				window.primary_toolbar()->widgetForAction(toggle));
			expect(button != nullptr && !button->accessibleName().isEmpty()
					   && !button->accessibleDescription().isEmpty()
					   && button->focusPolicy() == Qt::StrongFocus,
				   "each drawer toggle is named, described, and focusable");
		}

		// [[BPR-0675]]. With nothing open the window is Home, which has no
		// drawers, so the strip does not show their toggles. The state that
		// mattered to the owner, a pull request open whose comparison failed,
		// is pinned in the composed suite: there the toggles are offered and
		// each drawer carries its own collapse.
		expect(!files->isVisible() && !conversations->isVisible(),
			   "Home shows no drawer toggles, because Home has no drawers");
		expect(files->toolTip()
				   == QStringLiteral(
					   "Hide the file drawer and give its width to the comparison."),
			   "and the toggle is described by what it does, not by a refusal");

		auto       projection = both_files_enabled();
		auto&      commands   = *projection.Commands;
		const auto filesEntry = static_cast<std::size_t>(
			core::workbench_command_id::toggle_files_drawer);
		commands.Commands[filesEntry]
			= refused(core::workbench_command_id::toggle_files_drawer,
					  core::workbench_reason_code::no_active_workbench);
		projection.Drawers = drawer_states(core::workbench_drawer_state::expanded,
										   core::workbench_drawer_state::expanded);
		window.publish(projection);
		expect(!files->isEnabled() && conversations->isEnabled(),
			   "drawer QAction enabled state follows the core command projection");
		expect(files->toolTip()
					   == QStringLiteral(
						   "Open a pull request to use its review drawers.")
				   && files->statusTip() == files->toolTip(),
			   "a disabled drawer toggle carries its exact core reason");
		expect(conversations->isChecked()
				   && conversations->toolTip()
					   == QStringLiteral("Hide the conversation drawer and give "
										 "its width to the comparison."),
			   "an expanded drawer offers the gesture that collapses it");

		conversations->trigger();
		expect(log.Workbench.size() == 1U,
			   "a drawer toggle emits exactly one semantic intent");
		const auto* const toggle = std::get_if<core::toggle_workbench_drawer_intent>(
			&log.Workbench[0]);
		expect(toggle != nullptr
				   && toggle->Drawer == core::workbench_drawer::conversations,
			   "the emitted intent names the toggled drawer");
		expect(conversations->isChecked()
				   && window.conversations_drawer()->state()
					   == core::workbench_drawer_state::expanded,
			   "triggering a toggle changes no drawer state locally; the core "
			   "decides");
		expect(log.Width.empty(),
			   "a collapse gesture never travels the host width path");

		projection         = both_files_enabled();
		projection.Drawers = drawer_states(
			core::workbench_drawer_state::expanded,
			core::workbench_drawer_state::collapsed);
		window.publish(projection);
		expect(!conversations->isChecked()
				   && window.conversations_drawer()->state()
					   == core::workbench_drawer_state::collapsed,
			   "the decided drawer state reaches the toggle and the drawer");
		expect(conversations->toolTip()
				   == QStringLiteral(
					   "Show the conversation drawer at its saved width."),
			   "a collapsed drawer offers the gesture that reopens it");
		expect(window.statistics().DrawerUpdates == 2U,
			   "each published drawer delta is observed exactly once");
	}

	/// \brief Proves the View menu exists and the toggle is core-decided.
	void test_the_view_menu_offers_a_core_decided_line_number_toggle()
	{
		intent_log           log;
		qt::workbench_window window{ [&log](core::workbench_interaction_intent aIntent) {
			log.Workbench.push_back(std::move(aIntent));
		} };
		window.show();
		QApplication::processEvents();

		expect(window.menu_bar() != nullptr && window.view_menu() != nullptr,
			   "the window has a menu bar with a View menu");
		expect(window.view_menu()->menuAction()->text()
				   == QStringLiteral("&View"),
			   "the menu the owner asked for is named View");
		auto* const action = window.line_numbers_action();
		expect(action != nullptr && action->isCheckable(),
			   "the View menu carries a checkable line-numbers entry");
		expect(window.view_menu()->actions().contains(action),
			   "the entry is in the View menu rather than only on the window");

		// Unprojected, the command is refused for the same reason every other
		// workbench command is.
		expect(!action->isEnabled(),
			   "an unprojected toggle is disabled, like every workbench "
			   "command with no workbench open");

		auto projection        = both_files_enabled();
		projection.LineNumbers = core::workbench_line_numbers_delta{
			.State = core::workbench_line_numbers::shown,
		};
		window.publish(projection);
		expect(action->isEnabled() && action->isChecked(),
			   "a projected toggle is enabled and shows the projected state");

		action->trigger();
		expect(log.Workbench.size() == 1U,
			   "the entry emits exactly one semantic intent");
		expect(std::get_if<core::toggle_workbench_line_numbers_intent>(
				   &log.Workbench[0])
				   != nullptr,
			   "the emitted intent is the line-numbers toggle");
		// The assertion the card asks for. Qt flips a checkable action's mark
		// on click before any handler runs, so an implementation that let the
		// widget decide would leave it unchecked here and look correct until
		// the core refused the gesture.
		expect(action->isChecked(),
			   "triggering changes no state locally; the mark still follows the "
			   "projection and the core decides");

		projection.LineNumbers = core::workbench_line_numbers_delta{
			.State = core::workbench_line_numbers::hidden,
		};
		window.publish(projection);
		expect(!action->isChecked(),
			   "the decided state reaches the entry");

		auto  refusedProjection = both_files_enabled();
		auto& commands          = *refusedProjection.Commands;
		commands.Commands[static_cast<std::size_t>(
			core::workbench_command_id::toggle_line_numbers)]
			= refused(core::workbench_command_id::toggle_line_numbers,
					  core::workbench_reason_code::no_active_workbench);
		window.publish(refusedProjection);
		expect(!action->isEnabled(),
			   "the entry's enabled state follows the core command projection");
	}

	void test_the_view_menu_projects_one_pane_without_changing_modality()
	{
		intent_log           log;
		qt::workbench_window window{ [&log](core::workbench_interaction_intent aIntent) {
			log.Workbench.push_back(std::move(aIntent));
		} };
		window.show();
		QApplication::processEvents();
		auto* const action = window.one_pane_action();
		expect(action != nullptr && action->isCheckable()
				   && action->shortcut().isEmpty()
				   && window.view_menu()->actions().contains(action),
			   "One pane is a checkable View item with no shortcut");
		expect(!action->isEnabled() && !action->isChecked(),
			   "One pane starts disabled and unchecked without a comparison");

		auto projection    = both_files_enabled();
		projection.OnePane = core::workbench_one_pane_delta{
			.State = core::workbench_one_pane::disabled,
		};
		window.publish(projection);
		expect(action->isEnabled() && !action->isChecked(),
			   "the View action takes legality and state from the projection");
		action->trigger();
		expect(log.Workbench.size() == 1U
				   && std::get_if<core::toggle_workbench_one_pane_intent>(
						  &log.Workbench.front())
					   != nullptr
				   && !action->isChecked(),
			   "the View item dispatches one intent and reasserts the projected mark");

		projection.OnePane = core::workbench_one_pane_delta{
			.State = core::workbench_one_pane::enabled,
		};
		window.publish(projection);
		expect(action->isChecked()
				   && window.comparison_surface()->one_pane_enabled(),
			   "the published choice reaches the composed comparison surface");
		window.choose_modality(qt::review_modality::read);
		window.choose_modality(qt::review_modality::review);
		expect(action->isChecked()
				   && window.comparison_surface()->one_pane_enabled(),
			   "switching Read to Review retains the one-pane presentation");

		auto refusedProjection = both_files_enabled();
		refusedProjection.Commands->Commands[static_cast<std::size_t>(
			core::workbench_command_id::toggle_one_pane)]
			= refused(core::workbench_command_id::toggle_one_pane,
					  core::workbench_reason_code::no_active_workbench);
		window.publish(refusedProjection);
		expect(!action->isEnabled() && action->isChecked(),
			   "a refused command disables the item without changing the choice");
	}

	void process_events();

	/// \brief Drags the handle between the files drawer and the centre.
	///
	/// \details [[BPR-0662]]. The only way a reviewer changes a width is a
	/// handle drag, which ends in the splitter's own move; a direct resize of a
	/// panel is a squeeze the window answers with the persisted widths.
	void drag_files_handle(qt::workbench_window& aWindow, const int aFilesWidth)
	{
		auto* const splitter = aWindow.central_splitter();
		const auto  sizes    = splitter->sizes();
		splitter->move_handle(2, sizes[0] + splitter->handleWidth() + aFilesWidth);
	}

	void test_collapsing_a_drawer_returns_space_to_the_centre()
	{
		intent_log           log;
		qt::workbench_window window{
			{ },
			{ },
			[&log](core::workbench_drawer aDrawer, std::uint32_t aWidth) {
				log.Width.push_back({ .Drawer = aDrawer, .Width = aWidth });
			}
		};

		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 260U,
		};
		opened.Behaviour = drawer_controls_shown();
		// [[BPR-0651]]. Wide enough for three open drawers and a centre above
		// its minimum, so collapsing one returns exactly its width.
		window.resize(1'600, 720);
		window.show();
		QApplication::processEvents();
		window.publish(opened);
		QApplication::processEvents();

		auto* const centre = window.comparison_surface();
		expect(window.files_drawer()->width() == 320
				   && window.conversations_drawer()->width() == 260,
			   "each drawer takes exactly the width the host published");
		const auto openCentre = centre->width();
		expect(openCentre > 0, "the centre has a measurable width when open");

		auto collapsed    = application::workbench_projection{ };
		collapsed.Drawers = drawer_states(core::workbench_drawer_state::collapsed,
										  core::workbench_drawer_state::expanded);
		window.publish(collapsed);
		QApplication::processEvents();
		expect(centre->width()
				   >= openCentre + 320 - qt::workbench_drawer_rail::width_pixels,
			   "collapsing a drawer returns all but its rail's width to the centre");
		expect(window.files_drawer()->isVisible()
				   && window.files_drawer()->width()
					   == qt::workbench_drawer_rail::width_pixels
				   && window.files_drawer()->rail()->isVisible()
				   && !window.files_drawer()->contents_view()->isVisible(),
			   "a collapsed drawer narrows to its rail rather than leaving the "
			   "central surface");
		expect(window.files_drawer()->rail()->accessibleName()
					   == QStringLiteral("Files rail")
				   && window.files_drawer()->rail()->focusPolicy() == Qt::StrongFocus
				   && window.focus_order().contains(window.files_drawer()->rail()),
			   "the rail is a named, focusable stop");
		expect(window.conversations_drawer()->width() == 260,
			   "collapsing one drawer leaves the other drawer's width alone");

		auto reopened    = application::workbench_projection{ };
		reopened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
										 core::workbench_drawer_state::expanded);
		window.publish(reopened);
		QApplication::processEvents();
		expect(window.files_drawer()->isVisible()
				   && window.files_drawer()->width() == 320,
			   "reopening a drawer restores its persisted width");
		expect(centre->width() == openCentre,
			   "reopening a drawer returns the borrowed width to the drawer");

		// A width persisted while the drawer was closed is the width it reopens
		// at, which a cached widget geometry could not produce.
		window.publish(collapsed);
		QApplication::processEvents();
		auto widened   = application::workbench_projection{ };
		widened.Layout = application::workbench_layout_delta{
			.FilesWidth         = 420U,
			.ConversationsWidth = 260U,
		};
		window.publish(widened);
		window.publish(reopened);
		QApplication::processEvents();
		expect(window.files_drawer()->width() == 420,
			   "reopening restores the width persisted while it was collapsed");

		expect(log.Width.empty(),
			   "applying a published layout reports no width back to the host");
		window.hide();
	}

	void test_strip_modalities_positions_and_account()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			[&log](core::navigate_diff_change_intent aIntent) {
				log.Change.push_back(aIntent);
			}
		};
		window.resize(1'280, 820);
		window.show();
		QApplication::processEvents();

		auto* const modality = window.modality();
		expect(modality != nullptr
				   && window.current_modality() == qt::review_modality::review,
			   "the strip opens on Review, the modality with both drawers open");
		for (std::size_t index = 0U; index < qt::review_modality_count; ++index) {
			const auto  which  = static_cast<qt::review_modality>(index);
			auto* const action = modality->action(which);
			auto* const button = modality->button(which);
			expect(action != nullptr && action->isCheckable()
					   && action->shortcut()
						   == QKeySequence{ QStringLiteral("Ctrl+%1").arg(index + 1U) }
					   && action->shortcutContext() == Qt::WindowShortcut
					   && button != nullptr && button->isVisibleTo(window.modality())
					   && button->focusPolicy() == Qt::StrongFocus
					   && !button->accessibleName().isEmpty()
					   && !button->accessibleDescription().isEmpty()
					   && window.focus_order().contains(button),
				   "each modality is a checkable, shortcut-bearing, focusable stop");
		}
		expect(modality->action(qt::review_modality::review)->isChecked()
				   && !modality->action(qt::review_modality::read)->isChecked()
				   && modality->button(qt::review_modality::review)->isChecked()
				   && !modality->button(qt::review_modality::read)->isChecked(),
			   "exactly the current modality reads checked, on the action and "
			   "on its segment");
		// The host's first projection carries the drawers; the segment must
		// still read checked after the window infers the modality from them.
		window.publish(application::workbench_projection{
			.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									 core::workbench_drawer_state::expanded) });
		QApplication::processEvents();
		expect(window.current_modality() == qt::review_modality::review
				   && modality->button(qt::review_modality::review)->isChecked(),
			   "a restored layout with both drawers open keeps Review checked on "
			   "its segment");

		// [[BPR-0675]] [[BPR-0674]]. A drawer'''s room does not depend on a
		// comparison, so a modality asks for its preset whether or not one has
		// resolved. That is the case that matters: a reviewer whose comparison
		// is still coming is exactly who wants the room arranged.
		log.Workbench.clear();
		modality->action(qt::review_modality::read)->trigger();
		QApplication::processEvents();
		const auto drawerRequests = [&log] {
			auto asked = std::vector<std::pair<core::workbench_drawer, core::workbench_drawer_state>>{ };
			for (const auto& intent : log.Workbench) {
				if (const auto* const set = std::get_if<core::set_workbench_drawer_intent>(&intent);
					set != nullptr) {
					asked.emplace_back(set->Drawer, set->State);
				}
			}
			return asked;
		};
		expect(window.current_modality() == qt::review_modality::read
				   && modality->action(qt::review_modality::read)->isChecked()
				   && !modality->action(qt::review_modality::review)->isChecked()
				   && drawerRequests().size() == 3U,
			   "a modality asks for its three drawers with no comparison open");

		// [[BPR-0674]]. Overview is the centre'''s page, so it asks for every
		// drawer to be collapsed. The owner met it 150 pixels wide between
		// three of them.
		log.Workbench.clear();
		modality->action(qt::review_modality::overview)->trigger();
		QApplication::processEvents();
		expect(drawerRequests()
				   == std::vector<std::pair<core::workbench_drawer, core::workbench_drawer_state>>{
					   { core::workbench_drawer::files, core::workbench_drawer_state::collapsed },
					   { core::workbench_drawer::conversations, core::workbench_drawer_state::collapsed },
					   { core::workbench_drawer::repositories, core::workbench_drawer_state::collapsed } },
			   "Overview asks for all three drawers collapsed");

		window.publish(every_primary_command_offered());
		QApplication::processEvents();
		const auto drawerIntents = [&log] {
			auto states = std::vector<core::set_workbench_drawer_intent>{ };
			for (const auto& intent : log.Workbench) {
				if (const auto* const set
					= std::get_if<core::set_workbench_drawer_intent>(&intent);
					set != nullptr) {
					states.push_back(*set);
				}
			}
			return states;
		};
		// [[BPR-0674]]. Three drawers, three exact set intents, in that order.
		const auto expectPreset = [&](const qt::review_modality          aModality,
									  const core::workbench_drawer_state aFiles,
									  const core::workbench_drawer_state aConversations,
									  const core::workbench_drawer_state aRepositories,
									  const char* const                  aMessage) {
			log.Workbench.clear();
			modality->action(aModality)->trigger();
			QApplication::processEvents();
			const auto states = drawerIntents();
			expect(states.size() == 3U
					   && states[0]
						   == core::set_workbench_drawer_intent{
							   .Drawer = core::workbench_drawer::files,
							   .State  = aFiles }
					   && states[1] == core::set_workbench_drawer_intent{ .Drawer = core::workbench_drawer::conversations, .State = aConversations } && states[2] == core::set_workbench_drawer_intent{ .Drawer = core::workbench_drawer::repositories, .State = aRepositories } && window.current_modality() == aModality,
				   aMessage);
		};
		// Overview is the centre's own page and takes the whole centre.
		expectPreset(qt::review_modality::overview,
					 core::workbench_drawer_state::collapsed,
					 core::workbench_drawer_state::collapsed,
					 core::workbench_drawer_state::collapsed,
					 "Overview closes every drawer through exact set intents");
		// [[BPR-0724]]: the repositories tree is no part of reading a file.
		expectPreset(qt::review_modality::read,
					 core::workbench_drawer_state::collapsed,
					 core::workbench_drawer_state::expanded,
					 core::workbench_drawer_state::collapsed,
					 "Read collapses files and repositories and keeps comments beside the code");
		expectPreset(qt::review_modality::review,
					 core::workbench_drawer_state::expanded,
					 core::workbench_drawer_state::expanded,
					 window.repositories_drawer()->state(),
					 "Review opens both drawers");
		expectPreset(qt::review_modality::discuss,
					 core::workbench_drawer_state::collapsed,
					 core::workbench_drawer_state::expanded,
					 window.repositories_drawer()->state(),
					 "Discuss collapses files and gives the conversation the room");

		// A manual toggle never moves the segment: the preset stands until
		// the next modality is chosen.
		log.Workbench.clear();
		window.files_drawer_toggle_action()->trigger();
		QApplication::processEvents();
		expect(window.current_modality() == qt::review_modality::discuss,
			   "a manual drawer toggle leaves the modality where it was");

		// The file position comes from the core's navigation cursor and
		// reads as the strip labels it.
		window.publish(application::workbench_projection{
			.Selection = core::workbench_selection_delta{
				.Cursor = core::workbench_navigation_cursor{
					.Ordinal           = 3U,
					.ActionableCount   = 40U,
					.PreviousAvailable = true,
					.NextAvailable     = true,
				} } });
		QApplication::processEvents();
		expect(window.file_position_label() != nullptr
				   && window.file_position_label()->text()
					   == QStringLiteral("File 3 of 40"),
			   "the strip states the selected file's position among the actionable files");
		expect(window.change_position_label() != nullptr
				   && window.change_position_label()->text() == QStringLiteral("Changes"),
			   "with no comparison held the Changes label still names its arrow pair");

		// Signed out, the account button is the way to the sign-in the
		// header shows; its presenter is named and never reads disabled.
		auto* const account       = window.account_action();
		auto* const accountButton = qobject_cast<QToolButton*>(
			window.primary_toolbar()->widgetForAction(account));
		expect(account != nullptr && account->isEnabled()
				   && account->text() == QStringLiteral("Sign in")
				   && accountButton != nullptr && accountButton->isVisible()
				   && accountButton->accessibleName() == QStringLiteral("Account")
				   && !accountButton->accessibleDescription().isEmpty(),
			   "signed out, the account button offers sign-in and stays enabled");
		auto* const settingsButton = qobject_cast<QToolButton*>(
			window.primary_toolbar()->widgetForAction(window.settings_action()));
		expect(settingsButton != nullptr && settingsButton->isVisible()
				   && !settingsButton->accessibleName().isEmpty(),
			   "settings closes the strip's right region");

		// The identity and the positions yield width before the floor; the
		// controls never do.
		window.resize(window.minimumWidth(), 720);
		QApplication::processEvents();
		expect(window.primary_toolbar()->findChild<QToolButton*>(
				   QStringLiteral("qt_toolbar_ext_button"))
					   == nullptr
				   || !window.primary_toolbar()
						   ->findChild<QToolButton*>(
							   QStringLiteral("qt_toolbar_ext_button"))
						   ->isVisible(),
			   "no strip control folds into an overflow menu at the width floor");
		for (std::size_t index = 0U; index < qt::review_modality_count; ++index) {
			expect(modality->button(static_cast<qt::review_modality>(index))
					   ->isVisible(),
				   "every modality segment stays visible at the width floor");
		}
		// [[BPR-1129]]. The positions are the review bar's and stay at the
		// floor, keeping their numbers where they drop their words. No pull
		// request is open here, so the name has nothing to say and takes no
		// width; with one open it stays at every width
		// (test_strip_identity_stays_at_every_width).
		expect(!window.identity_label()->isVisible()
				   && window.file_position_label()->isVisible()
				   && window.file_position_label()->text().startsWith(QStringLiteral("File"))
				   && window.file_position_label()->text().contains(QStringLiteral("40"))
				   && window.file_position_label()->toolTip() == QStringLiteral("File 3 of 40"),
			   "the file position stays on the review bar at the width floor, its words in its tooltip");
		window.hide();
	}

	/// [[BPR-0722]], [[BPR-1129]]. With a long name published, nothing enters
	/// either row's overflow at any width, and each row yields text in order.
	/// The title row's name has the room the navigation took: it drops its
	/// title, then its owner, then elides its repository, and keeps its number
	/// at every width down to the floor; the modality drops its shortcuts on
	/// the way. The review bar drops the comparison's dates, then the
	/// positions' words, and never a control.
	///
	/// [[BPR-0824]]. The widths at which each step happens are measured, not
	/// pinned: the rows' laid-out widths differ by platform (the macOS inset
	/// and command-glyph shortcuts, the Windows menu control and caption
	/// buttons, fontconfig's metrics on Linux). What is pinned is the order,
	/// at every width.
	void test_strip_identity_stays_at_every_width()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			[&log](core::navigate_diff_change_intent aIntent) {
				log.Change.push_back(aIntent);
			}
		};
		window.resize(1'920, 1'080);
		window.show();
		QApplication::processEvents();
		window.publish(application::github_presentation_update{
			.PullRequest = application::github_pull_request_projection{
				.Owner          = "microsoft",
				.RepositoryName = "terminal",
				.Number         = core::pull_request_number{ 20607U },
				.Title          = "Fix the ScreenBufferTests suite so that every scrolling case restores the viewport it started with",
			},
		});
		auto positioned      = every_primary_command_offered();
		positioned.Selection = core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{
																	.Ordinal           = 3U,
																	.ActionableCount   = 40U,
																	.PreviousAvailable = true,
																	.NextAvailable     = true,
																} };
		window.publish(positioned);
		QApplication::processEvents();

		const auto overflowShown = [&window]() {
			for (auto* const row : { window.primary_toolbar(), window.review_bar() }) {
				auto* const extension = row->findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button"));
				if (extension != nullptr && extension->isVisible()) {
					return true;
				}
			}
			return false;
		};
		// The design's widths: the whole name, title included, at 1920 and at
		// 1280 on every platform, now that the title row carries no navigation.
		for (const auto width : { 1'920, 1'280 }) {
			window.resize(width, 820);
			QApplication::processEvents();
			const auto text = window.identity_label()->text();
			expect(!overflowShown() && window.identity_label()->isVisible()
					   && text.contains(QStringLiteral("microsoft/terminal  #20607"))
					   && text.contains(QStringLiteral("Fix the")),
				   "at 1920 and 1280 the title row shows the whole name with its title");
			expect(window.identity_label()->toolTip().contains(QStringLiteral("microsoft/terminal  #20607 Fix the ScreenBufferTests")),
				   "the full name stays in the tooltip");
			expect(window.file_position_label()->text() == QStringLiteral("File 3 of 40"),
				   "the review bar's positions keep their words at 1280");
		}
		// Every width down to the floor: 20 pixels at a time to 1300, then
		// pixel by pixel.
		auto titleGone    = false;
		auto ownerGone    = false;
		auto wordsGone    = false;
		auto compactFirst = 0; // the widest width with the modality's shortcuts gone
		for (auto width = 1'920; width >= window.minimumWidth(); width -= width > 1'300 ? 20 : 1) {
			window.resize(width, 820);
			QApplication::processEvents();
			expect(!overflowShown(), "no control folds into an overflow menu at any width");
			expect(!window.modality()->abbreviated(),
				   "the title row fits at every width down to its floor, so the modality never shortens to prefixes");
			const auto text  = window.identity_label()->text();
			const auto title = text.contains(QStringLiteral("Fix the"));
			const auto owner = text.contains(QStringLiteral("microsoft/"));
			expect(window.identity_label()->isVisible() && text.contains(QStringLiteral("#20607")),
				   "the name keeps its number at every width down to the floor");
			expect(!(titleGone && title), "the title yields once and does not return at a narrower width");
			expect(!(ownerGone && owner), "the owner yields once and does not return at a narrower width");
			expect(owner || !title, "the title yields before the owner");
			titleGone = titleGone || !title;
			ownerGone = ownerGone || !owner;
			if (window.modality()->compact() && compactFirst == 0) {
				compactFirst = width;
			}
			const auto words = window.file_position_label()->text().startsWith(QStringLiteral("File"));
			expect(window.file_position_label()->isVisible() && window.change_position_label()->isVisible(),
				   "the review bar's named groups stay at every width");
			expect(!(wordsGone && words), "the positions' words yield once and do not return at a narrower width");
			wordsGone = wordsGone || !words;
		}
		expect(compactFirst > 0, "the modality's shortcuts yield on the way down to the floor");
#if defined(_WIN32)
		// [[BPR-0653]], [[BPR-1129]]. The owner's floor on Windows is 902. With the
		// navigation on the review bar, the title row there holds the name and
		// the whole modality words, with the menu control and caption buttons.
		expect(window.minimumWidth() == 902, "the Windows title row's width floor is 902");
		expect(!window.modality()->abbreviated() && window.identity_label()->isVisible()
				   && !window.caption_buttons()->isHidden() && !window.strip_menu_button()->isHidden(),
			   "at the Windows floor the name, the whole modality words and every title-row control show");
		for (std::size_t index = 0U; index < qt::review_modality_count; ++index) {
			const auto modality = static_cast<qt::review_modality>(index);
			expect(window.modality()->button(modality)->text() == qt::review_modality_title(modality),
				   "at the Windows floor each segment shows its whole word");
		}
#else
		expect(window.minimumWidth() == 900, "the width floor stays 900 off Windows");
#endif
		// Straight from the default width to the narrowest that keeps the
		// shortcuts: the rows measure their items in the form this resize
		// gives them, not the form the last one left.
		window.resize(1'920, 820);
		QApplication::processEvents();
		window.resize(compactFirst + 1, 820);
		QApplication::processEvents();
		expect(!window.modality()->compact() && window.identity_label()->text().contains(QStringLiteral("#20607")),
			   "one large resize lays out the title row as a gradual one does");
		window.hide();
	}

	void test_rails_reopen_their_drawers()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			{ },
			{ },
		};
		auto collapsed    = both_files_enabled();
		collapsed.Drawers = drawer_states(core::workbench_drawer_state::collapsed,
										  core::workbench_drawer_state::collapsed);
		collapsed.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 260U,
		};
		collapsed.Behaviour = drawer_controls_shown();
		window.resize(1'280, 720);
		window.show();
		QApplication::processEvents();
		window.publish(collapsed);
		QApplication::processEvents();

		for (auto* const drawer : { window.files_drawer(), window.conversations_drawer() }) {
			expect(drawer->width() == qt::workbench_drawer_rail::width_pixels
					   && drawer->rail()->isVisible()
					   && drawer->rail()->width()
						   == qt::workbench_drawer_rail::width_pixels,
				   "each collapsed drawer is its 18-pixel rail");
		}
		expect(window.files_drawer()->rail()->label() == QStringLiteral("Files")
				   && window.conversations_drawer()->rail()->label()
					   == QStringLiteral("Comments"),
			   "the rails carry the drawers' names vertically");

		log.Workbench.clear();
		window.files_drawer()->rail()->click();
		QApplication::processEvents();
		expect(log.Workbench.size() == 1U
				   && std::holds_alternative<core::toggle_workbench_drawer_intent>(
					   log.Workbench.front())
				   && std::get<core::toggle_workbench_drawer_intent>(
						  log.Workbench.front())
						   .Drawer
					   == core::workbench_drawer::files,
			   "pressing a rail asks the core to toggle that drawer and nothing "
			   "else");
		expect(window.files_drawer()->width()
				   == qt::workbench_drawer_rail::width_pixels,
			   "the rail does not reopen the drawer by itself; the core decides");

		// The file position reaches the files rail from the same cursor the
		// strip reads.
		auto positioned      = both_files_enabled();
		positioned.Selection = core::workbench_selection_delta{
			.Cursor = core::workbench_navigation_cursor{
				.Ordinal           = 12U,
				.ActionableCount   = 40U,
				.PreviousAvailable = true,
				.NextAvailable     = true,
			}
		};
		window.publish(positioned);
		QApplication::processEvents();
		expect(window.files_drawer()->rail()->summary() == QStringLiteral("12 of 40"),
			   "the files rail states the selected file's position");
		window.hide();
	}

	void test_strip_is_the_title_row()
	{
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.move(200, 150);
		window.show();
		// [[BPR-0650]]. A control to press on: the strip is whole once a
		// workbench is active; on Home it keeps only Home, account, settings.
		window.publish(every_primary_command_offered());
		QApplication::processEvents();

		auto* const strip = window.primary_toolbar();
		// A point on the expanding spacer before the modality control: strip
		// surface with no control under it.
		auto* const spacer = strip->findChild<QWidget*>(
			QStringLiteral("workbenchNavigationSpacer"));
		expect(spacer != nullptr && spacer->width() > 8,
			   "the strip has bare surface to take hold of");
		const auto hold       = spacer->mapTo(strip, QPoint{ spacer->width() / 2, 11 });
		const auto before     = window.frameGeometry().topLeft();
		const auto globalHold = strip->mapToGlobal(hold);
		auto       press      = QMouseEvent{
			QEvent::MouseButtonPress,
			QPointF{ hold },
			QPointF{ globalHold },
			Qt::LeftButton,
			Qt::LeftButton,
			Qt::NoModifier,
		};
		QApplication::sendEvent(strip, &press);
		const auto moved = hold + QPoint{ 60, 80 };
		auto       move  = QMouseEvent{
			QEvent::MouseMove,
			QPointF{ moved },
			QPointF{ globalHold + QPoint{ 60, 80 } },
			Qt::NoButton,
			Qt::LeftButton,
			Qt::NoModifier,
		};
		QApplication::sendEvent(strip, &move);
		auto release = QMouseEvent{
			QEvent::MouseButtonRelease,
			QPointF{ moved },
			QPointF{ globalHold + QPoint{ 60, 80 } },
			Qt::LeftButton,
			Qt::NoButton,
			Qt::NoModifier,
		};
		QApplication::sendEvent(strip, &release);
		QApplication::processEvents();
		expect(press.isAccepted() && move.isAccepted() && release.isAccepted(),
			   "the strip takes a drag that starts on its bare surface");
		expect(window.frameGeometry().topLeft() == before + QPoint{ 60, 80 },
			   "dragging bare strip surface moves the window by the drag, as a "
			   "title bar would");

		// A press on a control is that control's, never a drag.
		auto* const files = qobject_cast<QToolButton*>(
			strip->widgetForAction(window.files_drawer_toggle_action()));
		const auto onFiles      = files->mapTo(strip, files->rect().center());
		auto       controlPress = QMouseEvent{
			QEvent::MouseButtonPress,
			QPointF{ onFiles },
			QPointF{ strip->mapToGlobal(onFiles) },
			Qt::LeftButton,
			Qt::LeftButton,
			Qt::NoModifier,
		};
		const auto beforeControl = window.frameGeometry().topLeft();
		QApplication::sendEvent(strip, &controlPress);
		auto controlMove = QMouseEvent{
			QEvent::MouseMove,
			QPointF{ onFiles + QPoint{ 30, 30 } },
			QPointF{ strip->mapToGlobal(onFiles + QPoint{ 30, 30 }) },
			Qt::NoButton,
			Qt::LeftButton,
			Qt::NoModifier,
		};
		QApplication::sendEvent(strip, &controlMove);
		expect(window.frameGeometry().topLeft() == beforeControl,
			   "a drag that starts on a strip control never moves the window");

		// Double-clicking bare surface zooms the window and back.
		auto zoom = QMouseEvent{
			QEvent::MouseButtonDblClick,
			QPointF{ hold },
			QPointF{ globalHold },
			Qt::LeftButton,
			Qt::LeftButton,
			Qt::NoModifier,
		};
		QApplication::sendEvent(strip, &zoom);
		QApplication::processEvents();
		expect(zoom.isAccepted() && window.isMaximized(),
			   "double-clicking bare strip surface zooms the window");
		// Zoomed, the strip has relaid its items, so the bare surface is
		// found again rather than assumed where it was.
		//
		// [[BPR-0653]]. The point is now proved to be inside the strip before
		// it is used. `empty_surface_at` refuses a point outside the strip's
		// own rectangle, which the Windows hit test needs and which is plainly
		// right -- a point the strip does not contain is not its title-bar
		// surface. Before that guard existed an out-of-rect point read as bare
		// surface, and this row was passing by double-clicking outside the
		// strip entirely.
		//
		// The zoomed geometry has to be able to hold the strip for there to be
		// any bare surface to aim at. The offscreen screen is 800x600 device
		// pixels, so at 200% it is 400x300 logical: maximising there gives a
		// window narrower than the strip's own width floor (measured: toolbar
		// 398 wide, spacer left at x=426 from before the maximise), the strip
		// necessarily overflows, and it offers no bare surface at all. Where
		// that is the case the restore is exercised through the same public
		// window state instead, and the sub-floor geometry is asserted rather
		// than assumed, so this never silently skips on a screen that could
		// have hosted the gesture.
		strip->layout()->activate();
		QApplication::processEvents();
		const auto zoomedHold
			= spacer->mapTo(strip, QPoint{ spacer->width() / 2, 11 });
		const auto zoomedGeometry = std::string{ "point " }
			+ std::to_string(zoomedHold.x()) + "," + std::to_string(zoomedHold.y())
			+ " strip=" + std::to_string(strip->width()) + "x"
			+ std::to_string(strip->height()) + " spacer=" + std::to_string(spacer->x())
			+ "," + std::to_string(spacer->y()) + " " + std::to_string(spacer->width())
			+ "x" + std::to_string(spacer->height())
			+ " window=" + std::to_string(window.width()) + " floor="
			+ std::to_string(window.minimumWidth());
		if (strip->rect().contains(zoomedHold)) {
			auto unzoom = QMouseEvent{
				QEvent::MouseButtonDblClick,
				QPointF{ zoomedHold },
				QPointF{ strip->mapToGlobal(zoomedHold) },
				Qt::LeftButton,
				Qt::LeftButton,
				Qt::NoModifier,
			};
			QApplication::sendEvent(strip, &unzoom);
			QApplication::processEvents();
			expect(unzoom.isAccepted() && !window.isMaximized(),
				   std::string{ "double-clicking again restores the window: " }
					   + zoomedGeometry + " accepted="
					   + (unzoom.isAccepted() ? "1" : "0") + " maximized="
					   + (window.isMaximized() ? "1" : "0"));
		} else {
			expect(window.width() < window.minimumWidth(),
				   std::string{ "only a zoom narrower than the strip's floor may leave no bare "
								"surface to double-click: " }
					   + zoomedGeometry);
			window.showNormal();
			QApplication::processEvents();
			expect(!window.isMaximized(),
				   "a window zoomed below the strip's floor still restores");
		}

		expect(strip->height() == qt::workbench_strip::height_pixels
				   && strip->actions().front()->objectName()
					   == QStringLiteral("workbenchStripInsetAction"),
			   "the strip keeps its height and opens with the platform inset");
		window.hide();
	}

#if defined(_WIN32)
	/// Presses the strip's menu control and runs aStep once, from the event
	/// loop, once the menu shows. It does not matter whether the menu waits in
	/// a nested loop of its own, so the same course runs against the stack
	/// `exec()` form [[BPR-1028]] replaced and fails there. aWindow must not be
	/// used after aStep, which may destroy it.
	void with_open_strip_menu(qt::workbench_window& aWindow, const std::function<void(QMenu&)>& aStep)
	{
		auto context  = QObject{ };
		auto ran      = false;
		auto attempts = 0;
		auto poll     = std::function<void()>{ };
		poll          = [&] {
			if (auto* const popup = qobject_cast<QMenu*>(QApplication::activePopupWidget()); popup != nullptr) {
				ran = true;
				aStep(*popup);
			} else if (++attempts < 250) {
				QTimer::singleShot(20, &context, poll);
			}
		};
		QTimer::singleShot(0, &context, poll);
		aWindow.strip_menu_button()->click();
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!ran && attempts < 250 && elapsed.elapsed() < 10'000) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		expect(ran, "the strip's menu control opens a menu");
	}

	/// The accessible child of aMenu that presents aAction.
	auto accessible_item(QMenu& aMenu, QAction* const aAction) -> QAccessibleInterface*
	{
		auto* const menu  = QAccessible::queryAccessibleInterface(&aMenu);
		const auto  index = aMenu.actions().indexOf(aAction);
		expect(menu != nullptr && index >= 0 && index < menu->childCount(),
			   "the menu presents the action as an accessible item");
		auto* const item = menu->child(static_cast<int>(index));
		expect(item != nullptr && item->actionInterface() != nullptr,
			   "the accessible menu item offers its actions");
		return item;
	}

	// [[BPR-1028]]. The strip's application menu and the window's lifetime. An
	// accessible invoke runs a menu action without closing the menu, so File >
	// Quit can destroy the window while the menu is open. The menu has to go
	// with the window, and none of the window's frames may be left on the stack
	// waiting for the menu. With the old stack menu run by `exec()`, the
	// window's destruction below deleted a stack object and the Debug CRT
	// asserted.
	void test_strip_menu_goes_with_its_window()
	{
		auto window = std::make_unique<qt::workbench_window>();
		window->resize(1'280, 820);
		window->show();
		window->publish(every_primary_command_offered());
		QApplication::processEvents();

		auto expected = QList<QAction*>{ };
		for (auto* const action : window->menuBar()->actions()) {
			if (action->isVisible()) {
				expected.push_back(action);
			}
		}
		auto closed = QPointer<QMenu>{ };
		auto shown  = QList<QAction*>{ };
		with_open_strip_menu(*window, [&](QMenu& aMenu) {
			closed = &aMenu;
			shown  = aMenu.actions();
			expect(aMenu.objectName() == QStringLiteral("workbenchStripMenu") && aMenu.parent() == window.get(),
				   "the strip menu is the window's own child");
			aMenu.close();
		});
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		QApplication::processEvents();
		expect(!expected.isEmpty() && shown == expected,
			   "the strip menu offers the menu bar's visible menus, in the bar's order");
		expect(closed.isNull() && window->findChildren<QMenu*>(QStringLiteral("workbenchStripMenu")).isEmpty(),
			   "a closed strip menu deletes itself, so opening it again leaves nothing behind");

		// [[BPR-1136]]. Each opening presents its items to accessibility, the
		// third as much as the second. The items' interfaces were cached by
		// action, so a menu opened after one that had been read found its
		// predecessor's, invalid and parented to nothing: UI Automation saw an
		// empty menu on the cleared Windows image.
		for (auto opening = 0; opening < 2; ++opening) {
			with_open_strip_menu(*window, [&](QMenu& aMenu) {
				auto* const menu = QAccessible::queryAccessibleInterface(&aMenu);
				expect(menu != nullptr && menu->childCount() == aMenu.actions().size(), "the reopened strip menu has an accessible item per entry");
				for (auto i = 0; menu != nullptr && i < menu->childCount(); ++i) {
					auto* const item = menu->child(i);
					expect(item != nullptr && item->isValid() && item->parent() == menu && !item->text(QAccessible::Name).isEmpty(),
						   "each item of a reopened strip menu is valid, named and the open menu's own");
				}
				aMenu.close();
			});
			QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::processEvents();
		}

		// Quit posts the window's destruction, as the desktop's own quit does,
		// and is pressed the way UI Automation presses it: File is opened
		// through its accessible item, and Quit's accessible press triggers the
		// action without closing either menu.
		auto* const quit = window->findChild<QAction*>(QStringLiteral("quitProfilesAction"));
		expect(quit != nullptr, "the window has its Quit action");
		auto* const fileMenu = qobject_cast<QMenu*>(quit->parent());
		expect(fileMenu != nullptr && fileMenu->menuAction() != nullptr && expected.contains(fileMenu->menuAction()),
			   "Quit is in a menu the strip menu offers");
		auto quitContext = QObject{ };
		window->set_profile_management_dispatch({ }, { }, [&window, &quitContext] {
			QTimer::singleShot(0, &quitContext, [&window] { window.reset(); });
		});
		auto open    = QPointer<QMenu>{ };
		auto subMenu = QPointer<QMenu>{ fileMenu };
		with_open_strip_menu(*window, [&](QMenu& aMenu) {
			open = &aMenu;
			accessible_item(aMenu, fileMenu->menuAction())->actionInterface()->doAction(QAccessibleActionInterface::showMenuAction());
			accessible_item(*fileMenu, quit)->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
			expect(!open.isNull() && open->isVisible(), "an accessible press leaves the strip menu open");
		});
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (window != nullptr && elapsed.elapsed() < 10'000) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		expect(window == nullptr, "Quit destroyed the window while its strip menu was open");
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		QApplication::processEvents();
		expect(open.isNull() && subMenu.isNull(), "the open strip menu went with its window");
		expect(QApplication::activePopupWidget() == nullptr, "no popup is left behind");
	}

	/// [[BPR-1022]]. One key, delivered as the Windows platform plugin delivers
	/// it: through `QWindowSystemInterface`, which offers it as a shortcut first
	/// and then hands it to the window, before any widget. The window it goes
	/// to is the one the platform picks: an open popup holds the keyboard grab.
	void platform_key(qt::workbench_window&       aWindow,
					  const QEvent::Type          aType,
					  const int                   aKey,
					  const Qt::KeyboardModifiers aModifiers,
					  const QString&              aText = { })
	{
		auto* const popup  = QApplication::activePopupWidget();
		auto* const target = popup != nullptr ? popup->windowHandle() : aWindow.windowHandle();
		expect(target != nullptr, "a key has a window to go to");
		QWindowSystemInterface::handleKeyEvent<QWindowSystemInterface::SynchronousDelivery>(
			target, aType, aKey, aModifiers, aText);
		QApplication::processEvents();
	}

	/// Alt pressed and released alone, with the modifiers Qt's Windows key
	/// mapper reports: Alt on the press, none on the release.
	void press_alt_alone(qt::workbench_window& aWindow)
	{
		platform_key(aWindow, QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
		platform_key(aWindow, QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
	}

	void press_escape(qt::workbench_window& aWindow)
	{
		platform_key(aWindow, QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
		platform_key(aWindow, QEvent::KeyRelease, Qt::Key_Escape, Qt::NoModifier);
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		QApplication::processEvents();
	}

	/// A left click on aTarget, delivered to the window as the platform does.
	void platform_click(qt::workbench_window& aWindow, QWidget* const aTarget, const Qt::KeyboardModifiers aModifiers)
	{
		const auto local  = QPointF{ aTarget->mapTo(&aWindow, aTarget->rect().center()) };
		const auto global = QPointF{ aTarget->mapToGlobal(aTarget->rect().center()) };
		QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(
			aWindow.windowHandle(), local, global, Qt::LeftButton, Qt::LeftButton, QEvent::MouseButtonPress, aModifiers);
		QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(
			aWindow.windowHandle(), local, global, Qt::NoButton, Qt::LeftButton, QEvent::MouseButtonRelease, aModifiers);
		QApplication::processEvents();
	}

	[[nodiscard]] auto open_strip_menu_popup() -> QMenu*
	{
		auto* const menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
		return menu != nullptr && menu->objectName() == QStringLiteral("workbenchStripMenu") ? menu : nullptr;
	}

	// [[BPR-1022]]. Alt pressed and released alone, and F10, open the strip's
	// application menu with its first menu highlighted; Esc closes it and the
	// keyboard goes back where it was. Alt as a modifier opens nothing and keeps
	// its own effect. Offscreen, input enters where the Windows platform plugin
	// hands it to Qt (QWindowSystemInterface), so everything from the shortcut
	// map onwards is the real path. That the plugin itself passes Alt and F10 on
	// as these events, and never enters the system's own menu mode, was measured
	// on the real `windows` platform and is recorded on the card.
	void test_strip_menu_opens_from_alt_and_f10()
	{
		auto window = qt::workbench_window{ };
		window.resize(1'280, 820);
		window.show();
		window.publish(every_primary_command_offered());
		window.activateWindow();
		QApplication::processEvents();
		expect(QGuiApplication::focusWindow() == window.windowHandle(), "the window has the keyboard");

		auto* const bar   = window.menuBar();
		auto        menus = QList<QAction*>{ };
		for (auto* const action : bar->actions()) {
			if (action->isVisible()) {
				menus.push_back(action);
			}
		}
		expect(!bar->isVisible() && menus.size() >= 2 && menus.front()->menu() != nullptr,
			   "the menu bar is hidden and holds the menus");
		auto* const fileMenu = menus.front()->menu();

		// The keyboard starts in an editor, which is where AltGr text goes.
		auto* editor = static_cast<QLineEdit*>(nullptr);
		for (auto* const candidate : window.findChildren<QLineEdit*>()) {
			if (candidate->isVisible() && candidate->isEnabled() && !candidate->isReadOnly()) {
				editor = candidate;
				break;
			}
		}
		expect(editor != nullptr, "the workbench has a visible editor to type into");
		editor->setFocus(Qt::OtherFocusReason);
		QApplication::processEvents();
		expect(QApplication::focusWidget() == editor, "the editor has the keyboard");

		const auto opensHighlighted = [&](const std::string_view aGesture) {
			auto* const menu = window.findChild<QMenu*>(QStringLiteral("workbenchStripMenu"));
			expect(menu != nullptr && menu->isVisible(), std::string{ aGesture } + " opens the strip's application menu");
			expect(menu->actions() == menus, std::string{ aGesture } + " offers exactly the bar's visible menus");
			expect(menu->activeAction() == menus.front(), std::string{ aGesture } + " highlights the first menu");
			expect(!fileMenu->isVisible() && QApplication::activePopupWidget() == menu,
				   std::string{ aGesture } + " highlights the first menu without opening it");
			press_escape(window);
			expect(QApplication::activePopupWidget() == nullptr, std::string{ "Esc closes the menu " } + std::string{ aGesture } + " opened");
			expect(QApplication::focusWidget() == editor, std::string{ "after " } + std::string{ aGesture } + ", Esc returns the keyboard to the editor");
			expect(window.findChildren<QMenu*>(QStringLiteral("workbenchStripMenu")).isEmpty(),
				   std::string{ "the menu " } + std::string{ aGesture } + " opened deletes itself once closed");
		};

		// AC1.
		press_alt_alone(window);
		opensHighlighted("Alt pressed and released alone");
		platform_key(window, QEvent::KeyPress, Qt::Key_F10, Qt::NoModifier);
		platform_key(window, QEvent::KeyRelease, Qt::Key_F10, Qt::NoModifier);
		opensHighlighted("F10");

		// Inside the menu, the arrows and the menus' mnemonics work as they do
		// from the mouse path, and Alt closes it again rather than reopening it.
		press_alt_alone(window);
		platform_key(window, QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
		platform_key(window, QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier);
		expect(QApplication::activePopupWidget() == fileMenu, "Right opens the highlighted first menu");
		press_escape(window);
		expect(open_strip_menu_popup() != nullptr, "Esc from the first menu returns to the application menu");
		press_escape(window);
		auto* const lastMenu = menus.back()->menu();
		const auto  mnemonic = QKeySequence::mnemonic(menus.back()->text());
		expect(lastMenu != nullptr && !mnemonic.isEmpty(), "the last menu has a mnemonic");
		press_alt_alone(window);
		platform_key(window, QEvent::KeyPress, static_cast<int>(mnemonic[0].key()), Qt::NoModifier, menus.back()->text().at(menus.back()->text().indexOf(u'&') + 1).toLower());
		expect(QApplication::activePopupWidget() == lastMenu, "the last menu's mnemonic opens it from the application menu");
		press_escape(window);
		press_escape(window);
		expect(QApplication::activePopupWidget() == nullptr && QApplication::focusWidget() == editor,
			   "leaving the menus returns the keyboard to the editor");
		press_alt_alone(window);
		expect(open_strip_menu_popup() != nullptr, "Alt opens the menu before it closes it");
		press_alt_alone(window);
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		QApplication::processEvents();
		expect(QApplication::activePopupWidget() == nullptr, "Alt pressed and released alone again closes the open menu and does not reopen it");
		expect(QApplication::focusWidget() == editor, "closing the menu with Alt returns the keyboard to the editor");

		// AC2. Every Alt chord keeps exactly the effect it has as a bare
		// shortcut, and opens no menu. Its actions are counted both ways.
		struct chord
		{
			const char*           Name;
			int                   Key;
			Qt::KeyboardModifiers Modifiers;
		};
		const auto chords = std::array{
			chord{ "Alt+N", Qt::Key_N, Qt::AltModifier },
			chord{ "Alt+1", Qt::Key_1, Qt::AltModifier },
			chord{ "Alt+2", Qt::Key_2, Qt::AltModifier },
			chord{ "Alt+3", Qt::Key_3, Qt::AltModifier },
			chord{ "Alt+Up", Qt::Key_Up, Qt::AltModifier },
			chord{ "Alt+Down", Qt::Key_Down, Qt::AltModifier },
			chord{ "Alt+PgUp", Qt::Key_PageUp, Qt::AltModifier },
			chord{ "Alt+PgDown", Qt::Key_PageDown, Qt::AltModifier },
			chord{ "Alt+Shift+1", Qt::Key_1, Qt::AltModifier | Qt::ShiftModifier },
			chord{ "Alt+Shift+2", Qt::Key_2, Qt::AltModifier | Qt::ShiftModifier },
			chord{ "Alt+Shift+3", Qt::Key_3, Qt::AltModifier | Qt::ShiftModifier },
			chord{ "Alt+Shift+4", Qt::Key_4, Qt::AltModifier | Qt::ShiftModifier },
			chord{ "Alt+Shift+5", Qt::Key_5, Qt::AltModifier | Qt::ShiftModifier },
			chord{ "Alt+Shift+R", Qt::Key_R, Qt::AltModifier | Qt::ShiftModifier },
		};
		auto triggered = 0;
		auto counted   = QObject{ };
		for (auto* const action : window.findChildren<QAction*>()) {
			QObject::connect(action, &QAction::triggered, &counted, [&triggered] { ++triggered; });
		}
		// A chord whose control opens a menu of its own (Alt+Shift+4, the
		// private progress control) runs that menu in a nested loop; it is
		// counted and closed from there.
		auto popups     = 0;
		auto closer     = QTimer{ };
		auto stripMenus = 0;
		QObject::connect(&closer, &QTimer::timeout, &counted, [&] {
			// Closing hides the popup at once, so each showing is counted once.
			if (auto* const popup = QApplication::activePopupWidget(); popup != nullptr) {
				++popups;
				if (popup->objectName() == QStringLiteral("workbenchStripMenu")) {
					++stripMenus;
				}
				popup->close();
			}
		});
		closer.start(10);
		const auto settle = [] {
			auto elapsed = QElapsedTimer{ };
			elapsed.start();
			while (elapsed.elapsed() < 400) {
				QApplication::processEvents(QEventLoop::AllEvents, 20);
			}
		};
		// Offscreen, a closed popup can leave no window active, where the
		// Windows platform never activates a popup at all. Each chord starts
		// where a reviewer's would: in the window, in the editor.
		const auto keepActive = [&] {
			if (QGuiApplication::focusWindow() != window.windowHandle() || QApplication::focusWidget() != editor) {
				window.activateWindow();
				editor->setFocus(Qt::OtherFocusReason);
				QApplication::processEvents();
			}
			expect(QGuiApplication::focusWindow() == window.windowHandle() && QApplication::focusWidget() == editor,
				   "each chord starts in the active window's editor");
		};
		auto effective = 0;
		for (const auto& gesture : chords) {
			const auto shifted = gesture.Modifiers.testFlag(Qt::ShiftModifier);
			triggered          = 0;
			popups             = 0;
			stripMenus         = 0;
			keepActive();
			platform_key(window, QEvent::KeyPress, gesture.Key, gesture.Modifiers);
			platform_key(window, QEvent::KeyRelease, gesture.Key, gesture.Modifiers);
			settle();
			const auto bare = std::pair{ triggered, popups };
			triggered       = 0;
			popups          = 0;
			keepActive();
			platform_key(window, QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
			if (shifted) {
				platform_key(window, QEvent::KeyPress, Qt::Key_Shift, Qt::AltModifier | Qt::ShiftModifier);
			}
			platform_key(window, QEvent::KeyPress, gesture.Key, gesture.Modifiers);
			platform_key(window, QEvent::KeyRelease, gesture.Key, gesture.Modifiers);
			if (shifted) {
				platform_key(window, QEvent::KeyRelease, Qt::Key_Shift, Qt::AltModifier);
			}
			platform_key(window, QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
			settle();
			std::cout << "BPR-1022 chord " << gesture.Name << ": bare triggered=" << bare.first << " popups=" << bare.second
					  << "; with Alt pressed and released triggered=" << triggered << " popups=" << popups
					  << " strip menus=" << stripMenus << '\n';
			expect(stripMenus == 0 && QApplication::activePopupWidget() == nullptr,
				   std::string{ gesture.Name } + " opens no application menu when Alt is released");
			expect(std::pair{ triggered, popups } == bare, std::string{ gesture.Name } + " keeps exactly the effect it has as a bare shortcut");
			effective += bare.first + bare.second > 0 ? 1 : 0;
		}
		expect(effective >= 8, "most Alt chords do something here, so keeping their effect is not vacuous");

		// Alt+Space (the system menu on the real platform) and Alt+F4 (close,
		// the platform's) pass through as chords too.
		for (const auto& [name, key] : std::array{ std::pair{ "Alt+Space", int{ Qt::Key_Space } }, std::pair{ "Alt+F4", int{ Qt::Key_F4 } } }) {
			stripMenus = 0;
			keepActive();
			platform_key(window, QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
			platform_key(window, QEvent::KeyPress, key, Qt::AltModifier, key == Qt::Key_Space ? QStringLiteral(" ") : QString{ });
			platform_key(window, QEvent::KeyRelease, key, Qt::AltModifier);
			platform_key(window, QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
			settle();
			expect(stripMenus == 0 && QApplication::activePopupWidget() == nullptr,
				   std::string{ name } + " opens no application menu");
		}

		// AltGr is Ctrl with Alt: its text reaches the editor and no menu opens.
		stripMenus = 0;
		keepActive();
		const auto before = editor->text();
		platform_key(window, QEvent::KeyPress, Qt::Key_Control, Qt::ControlModifier);
		platform_key(window, QEvent::KeyPress, Qt::Key_Alt, Qt::ControlModifier | Qt::AltModifier);
		platform_key(window, QEvent::KeyPress, Qt::Key_Q, Qt::ControlModifier | Qt::AltModifier, QStringLiteral("@"));
		platform_key(window, QEvent::KeyRelease, Qt::Key_Q, Qt::ControlModifier | Qt::AltModifier, QStringLiteral("@"));
		platform_key(window, QEvent::KeyRelease, Qt::Key_Alt, Qt::ControlModifier);
		platform_key(window, QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier);
		settle();
		expect(stripMenus == 0 && QApplication::activePopupWidget() == nullptr, "AltGr text entry opens no application menu");
		expect(editor->text() == before + QStringLiteral("@"), "AltGr text entry types its character into the editor");

		// An Alt-click is the click's, and opens no menu when Alt is released.
		auto* const files = window.primary_toolbar()->widgetForAction(window.files_drawer_toggle_action());
		expect(files != nullptr && files->isVisible() && window.files_drawer_toggle_action()->isEnabled(),
			   "the files drawer toggle is there to Alt-click");
		triggered  = 0;
		stripMenus = 0;
		keepActive();
		platform_key(window, QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
		platform_click(window, files, Qt::AltModifier);
		platform_key(window, QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
		settle();
		expect(triggered == 1, "an Alt-click presses the control under it");
		expect(stripMenus == 0 && QApplication::activePopupWidget() == nullptr, "an Alt-click opens no application menu");
		closer.stop();

		// Alt alone still opens it after all of that.
		keepActive();
		press_alt_alone(window);
		expect(open_strip_menu_popup() != nullptr, "Alt pressed and released alone opens the menu after the chords");
		press_escape(window);
		window.hide();
	}
#endif

	void test_review_details_return_height_to_the_comparison()
	{
		qt::workbench_window window;
		window.resize(1'280, 1'000);
		window.show();
		// [[BPR-0650]]. Home is the centre until a workbench is active; the
		// review details are the workbench's. [[BPR-1130]]: with the Updates
		// line open, as the reviewer left it; the fold course covers folded.
		window.publish(every_primary_command_offered());
		open_updates_line(window);

		auto* const action     = window.review_details_toggle_action();
		auto* const comparison = window.comparison_surface();
		auto* const epochs     = window.epoch_selectors();
		auto* const detail     = window.detail_pane();
		// [[BPR-1129]]. Review details moved into the comparison's menu, since
		// what they disclose are the comparison's anchors; the menu's button
		// is a declared focus stop and the action keeps its window key.
		auto* const comparing = window.epoch_selectors()->comparing_button();
		expect(action != nullptr && action->isCheckable() && !action->isChecked()
				   && !action->shortcut().isEmpty() && window.actions().contains(action)
				   && !window.primary_toolbar()->actions().contains(action)
				   && comparing != nullptr && comparing->menu() != nullptr
				   && comparing->menu()->actions().contains(action)
				   && comparing->focusPolicy() == Qt::StrongFocus
				   && window.review_bar()->isAncestorOf(comparing)
				   && window.focus_order().contains(comparing),
			   "review details are a checkable, keyboard-reachable action in the comparison's menu on the review bar");
		// [[BPR-0646]]. Off by default: the open epoch line is chrome; the
		// exact anchors and the selected-change details are the review
		// details, shown on request.
		expect(window.findChild<QScrollArea*>(
				   QStringLiteral("workbenchReviewHeaderScroll"))
					   == nullptr
				   && comparison != nullptr && epochs != nullptr
				   && epochs->isVisible() && epochs->line() != nullptr
				   && epochs->line()->isVisible() && !epochs->details_visible()
				   && detail != nullptr && !detail->isVisible(),
			   "the epoch line shows above the comparison with the review details "
			   "hidden, and no review header above either");
		const auto focusedHeight = comparison->height();
		expect(action->toolTip().contains(
				   QStringLiteral("Show the exact review anchors")),
			   "the retained action explains what it reveals");

		action->trigger();
		QApplication::processEvents();
		expect(action->isChecked() && epochs->details_visible()
				   && epochs->line()->isVisible() && detail->isVisible(),
			   "the review details gesture shows the exact anchors and the detail "
			   "pane and keeps the epoch line");
		expect(comparison->height() < focusedHeight,
			   "showing review details takes their height from the comparison");

		action->trigger();
		QApplication::processEvents();
		expect(!action->isChecked() && !epochs->details_visible()
				   && !detail->isVisible() && comparison->height() == focusedHeight,
			   "the same shipped action hides the details and returns the height");
		window.hide();
	}

	void test_drawer_resize_emits_the_host_layout_path()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			{ },
			[&log](core::workbench_drawer aDrawer, std::uint32_t aWidth) {
				log.Width.push_back({ .Drawer = aDrawer, .Width = aWidth });
			}
		};

		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 260U,
		};
		window.resize(1'280, 720);
		window.show();
		QApplication::processEvents();
		window.publish(opened);
		QApplication::processEvents();

		auto* const splitter = window.findChild<QSplitter*>(
			QStringLiteral("workbenchCentralSplitter"));
		expect(splitter != nullptr && splitter->count() == 4,
			   "the centre and the three drawers share one resizable splitter");
		expect(!splitter->childrenCollapsible(),
			   "a drag can never collapse a drawer behind the core's back");

		log.Width.clear();
		log.Workbench.clear();
		drag_files_handle(window, 400);
		QApplication::processEvents();
		expect(window.files_drawer()->width() == 400,
			   "a splitter drag resizes the drawer");
		expect(log.Width.size() == 1U
				   && log.Width[0].Drawer == core::workbench_drawer::files
				   && log.Width[0].Width == 400U,
			   "a width change leaves once through the host layout path");
		expect(log.Workbench.empty(),
			   "a width change never becomes a semantic drawer intent");

		auto collapsed    = application::workbench_projection{ };
		collapsed.Drawers = drawer_states(core::workbench_drawer_state::collapsed,
										  core::workbench_drawer_state::expanded);
		auto reopened     = application::workbench_projection{ };
		reopened.Drawers  = drawer_states(core::workbench_drawer_state::expanded,
										  core::workbench_drawer_state::expanded);
		log.Width.clear();
		window.publish(collapsed);
		QApplication::processEvents();
		window.publish(reopened);
		QApplication::processEvents();
		expect(window.files_drawer()->width() == 400,
			   "the resized width is the width the drawer reopens at");
		expect(log.Width.empty(),
			   "collapsing and reopening reports no new width to the host");
		window.hide();
	}

	void test_a_squeezed_splitter_never_reports_a_width()
	{
		// [[BPR-0662]]. A hidden page laid out at its default size, a window
		// narrower than the persisted widths: the splitter squeezes the
		// drawers, and none of it is a width the reviewer chose.
		intent_log           log;
		qt::workbench_window window{
			{ },
			{ },
			[&log](core::workbench_drawer aDrawer, std::uint32_t aWidth) {
				log.Width.push_back({ .Drawer = aDrawer, .Width = aWidth });
			}
		};
		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 375U,
		};
		// Published before the window is shown, as the composed host restores
		// the layout before presentation: the splitter has only its default
		// size to lay out in.
		window.publish(opened);
		window.resize(1'600, 720);
		window.show();
		process_events();
		expect(log.Width.empty(),
			   "laying out a hidden splitter reports no width to the host");
		expect(window.files_drawer()->width() == 320
				   && window.conversations_drawer()->width() == 375,
			   "shown, the drawers take the widths the host persisted");

		// Narrower than the drawers and the centre's minimum together: the
		// drawers give way for as long as the window is that narrow.
		window.resize(900, 720);
		process_events();
		expect(log.Width.empty(),
			   "a squeeze reports no width to the host");
		window.resize(1'600, 720);
		process_events();
		expect(window.files_drawer()->width() == 320
				   && window.conversations_drawer()->width() == 375,
			   "the drawers return to the persisted widths once there is room");
		expect(log.Width.empty(),
			   "returning to the persisted widths reports nothing either");

		// The reviewer's own drag is what the host hears.
		drag_files_handle(window, 360);
		process_events();
		expect(log.Width.size() == 1U
				   && log.Width[0].Drawer == core::workbench_drawer::files
				   && log.Width[0].Width == 360U,
			   "a handle drag reports exactly the dragged drawer's width");
		window.hide();
	}

	void drag_conversations_handle(qt::workbench_window& aWindow, const int aWidth)
	{
		// The conversations drawer is the splitter's last widget, so its
		// handle's position is measured back from the splitter's far edge.
		auto* const splitter = aWindow.central_splitter();
		splitter->move_handle(3, splitter->width() - aWidth - splitter->handleWidth());
	}

	/// \brief Reports whether every visible control in the conversations drawer
	/// is reachable: inside the drawer, or inside the scrolled content whose
	/// whole width the horizontal scroll bar can bring into view.
	[[nodiscard]] auto every_control_is_reachable(const qt::workbench_window& aWindow) -> bool
	{
		auto* const drawer = aWindow.conversations_drawer();
		// [[BPR-1134]]. The column of the tray's tab that is showing. [[BPR-1242]]:
		// RFC-BPR-0023 gave Notes two pages, Private history and Agent tools, each
		// its own column; the one showing is the Notes tab's column.
		auto* const notesPages = aWindow.findChild<QTabWidget*>(QStringLiteral("privateConversationTabs"));
		auto* const scroll     = aWindow.tray_tabs()->currentIndex() == 0
			? aWindow.comments_tab_scroll()
			: (notesPages == nullptr ? nullptr : qobject_cast<QScrollArea*>(notesPages->currentWidget()));
		if (scroll == nullptr || scroll->widget() == nullptr) {
			std::cerr << "the conversations drawer has no content scroll area\n";
			return false;
		}
		auto* const contents = scroll->widget();
		if (scroll->horizontalScrollBar()->maximum() + scroll->viewport()->width()
			< contents->width()) {
			std::cerr << "the content is wider than the scroll range reaches\n";
			return false;
		}
		for (auto* const child : drawer->findChildren<QWidget*>()) {
			if (!child->isVisible() || child->width() == 0) {
				continue;
			}
			const auto  inContents = contents->isAncestorOf(child);
			auto* const frame      = inContents ? contents : drawer;
			const auto  right      = child->mapTo(frame, child->rect().topRight()).x();
			if (right >= frame->width()) {
				std::cerr << "unreachable control: " << child->metaObject()->className()
						  << " '" << child->objectName().toStdString() << "' right edge "
						  << right << " in " << frame->width() << "\n";
				return false;
			}
		}
		return true;
	}

	void test_the_conversations_drawer_narrows_below_its_default_in_read()
	{
		// [[BPR-0728]]. 375 is the width the drawer opens at, not a floor: the
		// owner found Read short of horizontal room for the code, so the
		// handle drags the drawer down to the presented floor the other
		// drawers stop at, the host hears the width, and Home and back keep
		// it rather than lifting it to 375.
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			{ },
			[&log](core::workbench_drawer aDrawer, std::uint32_t aWidth) {
				log.Width.push_back({ .Drawer = aDrawer, .Width = aWidth });
			}
		};
		window.resize(1'600, 820);
		window.show();
		process_events();
		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::collapsed,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 375U,
		};
		window.publish(opened);
		process_events();
		window.choose_modality(qt::review_modality::read);
		process_events();
		expect(window.conversations_drawer()->width() == 375,
			   "Read opens the conversations drawer at its default width");

		drag_conversations_handle(window, 200);
		process_events();
		expect(window.conversations_drawer()->width() == 200,
			   "the handle drags the conversations drawer below 375");
		expect(!log.Width.empty()
				   && log.Width.back().Drawer == core::workbench_drawer::conversations
				   && log.Width.back().Width == 200U,
			   "the host hears the dragged conversations width");
		expect(every_control_is_reachable(window),
			   "no Comments tab control is clipped at 200 pixels; the column scrolls");
		window.tray_tabs()->setCurrentIndex(1);
		process_events();
		expect(every_control_is_reachable(window),
			   "no Notes tab control is clipped at 200 pixels; the column scrolls");
		auto* const notesPages = window.findChild<QTabWidget*>(QStringLiteral("privateConversationTabs"));
		expect(notesPages != nullptr && notesPages->count() == 2,
			   "the Notes tab has its Private history and Agent tools pages");
		notesPages->setCurrentWidget(window.findings_tab_scroll());
		process_events();
		expect(every_control_is_reachable(window),
			   "no Agent tools control is clipped at 200 pixels; the column scrolls");
		notesPages->setCurrentIndex(0);
		process_events();
		window.tray_tabs()->setCurrentIndex(0);
		process_events();

		drag_conversations_handle(window, 100);
		process_events();
		expect(window.conversations_drawer()->width() == 100
				   && window.conversations_drawer()->minimumWidth()
					   == static_cast<int>(application::minimum_workbench_drawer_width)
				   && log.Width.back().Width == 100U,
			   "beneath the presented floor the drawer stops where the other drawers do, "
			   "and the host hears that too; the next launch lifts it to the design width");

		// The host persists 200 and republishes it; Home and back present it.
		auto persisted   = application::workbench_projection{ };
		persisted.Layout = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 200U,
		};
		window.publish(persisted);
		process_events();
		window.choose_home(true);
		process_events();
		expect(window.showing_home(), "Home shows over the open review");
		window.choose_modality(qt::review_modality::read);
		process_events();
		expect(!window.showing_home() && window.conversations_drawer()->width() == 200,
			   "back in Read the drawer keeps the persisted 200 rather than lifting to 375");
		window.hide();
	}

	void test_the_seam_is_centred_for_native_and_even_handle_widths()
	{
		for (const auto& styleName : QStyleFactory::keys()) {
			auto style    = std::unique_ptr<QStyle>{ QStyleFactory::create(styleName) };
			auto splitter = qt::workbench_central_splitter{ nullptr };
			splitter.setStyle(style.get());
			for (auto index = 0; index < 4; ++index) {
				splitter.addWidget(new QWidget{ });
			}
			splitter.resize(600, 120);
			splitter.show();
			const auto nativeWidth = style->pixelMetric(QStyle::PM_SplitterWidth, nullptr, &splitter);
			expect(splitter.handleWidth() == nativeWidth,
				   "the workbench preserves the selected style's native splitter width");
			std::cout << "seam geometry style=" << styleName.toStdString()
					  << " nativeWidth=" << nativeWidth << '\n';
			// Both parity cases run on every platform, even without the macOS style.
			for (const auto width : { nativeWidth, 4, 7 }) {
				splitter.setHandleWidth(width);
				process_events();
				for (auto index = 1; index < splitter.count(); ++index) {
					auto* const handle = dynamic_cast<qt::workbench_splitter_handle*>(splitter.handle(index));
					expect(handle != nullptr && handle->width() == width,
						   "the rendered handle has the requested style or parity width");
					const auto column = (width - 1) / 2;
					expect(handle->rule_edge() == column && column > 0 && column < width - 1,
						   "the seam has aimable space on both sides at native, four and seven pixel widths");
					// Render at logical resolution to pin exactly one column independently
					// of device rounding; the composed window checks the scaled raster too.
					auto image = QImage{ handle->size(), QImage::Format_ARGB32 };
					handle->render(&image);
					const auto mid        = handle->palette().color(QPalette::Active, QPalette::Mid);
					const auto background = handle->palette().color(QPalette::Active, QPalette::Window);
					expect(mid != background, "the seam raster distinguishes rule from background");
					for (auto x = 0; x < width; ++x) {
						expect(image.pixelColor(x, image.height() / 2).rgba() == (x == column ? mid : background).rgba(),
							   "exactly the middle logical column paints the one-pixel rule");
					}
				}
			}
		}
	}

	/// \brief Rests the pointer on a central splitter handle and reports what
	/// the platform would show: the widget under the pointer and its cursor.
	[[nodiscard]] auto handle_under_resting_pointer(
		qt::workbench_window& aWindow,
		const int             aHandle,
		const char* const     aMoment) -> bool
	{
		// The platform shows the cursor of the widget Qt's enter/leave tracking
		// holds under the pointer; a handle the tracking has left shows the
		// arrow of whatever took its place. The offscreen platform answers no
		// top-level for widgetAt, so the tracking's own record is what is read.
		auto* const splitter = aWindow.central_splitter();
		auto* const handle   = splitter->handle(aHandle);
		const auto  local    = handle->mapFromGlobal(QCursor::pos());
		if (handle->underMouse() && handle->rect().contains(local)
			&& handle->cursor().shape() == Qt::SplitHCursor) {
			return true;
		}
		auto* const under = aWindow.childAt(aWindow.mapFromGlobal(QCursor::pos()));
		std::cerr << aMoment << ": handle " << aHandle << " geometry x=" << handle->geometry().x()
				  << " width=" << handle->geometry().width() << " handleWidth="
				  << splitter->handleWidth() << "; pointer at local x=" << local.x()
				  << ", child at the pointer: "
				  << (under != nullptr ? under->metaObject()->className() : "nothing") << " '"
				  << (under != nullptr ? under->objectName().toStdString() : std::string{ })
				  << "', handle cursor " << static_cast<int>(handle->cursor().shape())
				  << ", handle underMouse " << handle->underMouse() << "\n";
		return false;
	}

	void test_the_split_cursor_stays_over_a_resting_handle()
	{
		// [[BPR-0739]]. The owner, resizing the conversations drawer, saw the
		// horizontal-resize cursor for a moment and then the arrow again with
		// the pointer still. The card's candidate causes are the window's own
		// relayouts: a modality change and a host-republished layout both set
		// the splitter's sizes again, and a handle that moves out from under a
		// resting pointer leaves the arrow until the pointer moves. Here the
		// pointer rests on the conversations handle through both, and the
		// widget under it must still be that handle with the split cursor.
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			{ },
			[&log](core::workbench_drawer aDrawer, std::uint32_t aWidth) {
				log.Width.push_back({ .Drawer = aDrawer, .Width = aWidth });
			}
		};
		window.resize(1'600, 820);
		window.show();
		process_events();
		auto opened      = both_files_enabled();
		opened.Drawers   = drawer_states(core::workbench_drawer_state::expanded,
										 core::workbench_drawer_state::expanded);
		opened.Behaviour = drawer_controls_shown();
		opened.Layout    = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 375U,
		};
		window.publish(opened);
		process_events();
		window.choose_modality(qt::review_modality::review);
		process_events();

		auto* const splitter = window.central_splitter();
		auto* const handle   = splitter->handle(3);
		// The platform cursor is placed so Qt's later relayout checks read the
		// pointer where it rests; the move itself is delivered to the window
		// as the platform would, because the offscreen platform's own delivery
		// at a fractional scale lands beside the handle.
		const auto restOn = [&](QSplitterHandle* const aHandle) {
			const auto global = aHandle->mapToGlobal(aHandle->rect().center());
			QCursor::setPos(global);
			const auto  local = QPointF{ window.mapFromGlobal(global) };
			QMouseEvent move{ QEvent::MouseMove, local, local, QPointF{ global },
							  Qt::NoButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(window.windowHandle(), &move);
			process_events();
		};
		const auto rest = [&] { restOn(handle); };
		// Qt's under-mouse tracking is re-evaluated from the integer global
		// pointer position when a widget moves; at a fractional scale the
		// offscreen round trip of that position lands a device pixel off, and
		// the relayout steps read a pointer beside the handle. The platforms
		// the defect was seen on scale by whole numbers; the fractional case is
		// the Windows lane's to observe on a real display.
		const auto scale      = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE", QStringLiteral("1.0")).toDouble();
		const auto fractional = std::abs(scale - std::round(scale)) > 0.001;
		if (fractional) {
			std::cout << "the resting-pointer sequence is read at whole-number scales; scale "
					  << scale << " pins the seam only\n";
		}
		const auto pointerHolds = [&](const char* const aMoment) {
			return fractional || handle_under_resting_pointer(window, 3, aMoment);
		};
		rest();
		expect(pointerHolds("at rest"),
			   "the conversations handle under a resting pointer shows the split cursor");

		// A modality change relays the drawers ([[BPR-0649]] Discuss widens the
		// conversation; Read returns it), and the pointer has not moved.
		window.choose_modality(qt::review_modality::read);
		process_events();
		expect(pointerHolds("after a modality change"),
			   "after a modality change the resting pointer is still on the handle "
			   "with the split cursor");

		// The host republishes the layout it persisted, the same widths.
		auto persisted   = application::workbench_projection{ };
		persisted.Layout = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 375U,
		};
		window.publish(persisted);
		process_events();
		expect(pointerHolds("after a republished layout"),
			   "after the host republishes the layout the resting pointer is still on "
			   "the handle with the split cursor");

		// A drag the host answers with its clamped width, then the pointer rests
		// where the drag left it: on the handle.
		drag_conversations_handle(window, 300);
		process_events();
		auto answered   = application::workbench_projection{ };
		answered.Layout = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 300U,
		};
		window.publish(answered);
		process_events();
		rest();
		window.choose_modality(qt::review_modality::discuss);
		process_events();
		expect(!handle->isVisible(), "Discuss's main page has no conversation-drawer resize seam");
		window.choose_modality(qt::review_modality::read);
		process_events();
		rest();
		expect(pointerHolds("after a drag, Discuss and Read"),
			   "after a Discuss round trip the restored drawer seam handles hover with the split cursor");

		// The files handle is the same seam on the other side.
		restOn(splitter->handle(2));
		window.publish(answered);
		process_events();
		expect(fractional || handle_under_resting_pointer(window, 2, "files handle"),
			   "the files handle behaves the same under a resting pointer");

		// The seam is visible: the handle draws the drawer's edge rule at one
		// constant weight, the weight the accepted asset draws it at, whether
		// or not the pointer is over it. [[BPR-0739]], the owner in the v009
		// attended session: "Drop it: draw the rule at one constant weight, as
		// the asset does".
		const auto ruleShows = [](QSplitterHandle* const aHandle, const QRgb aColour) {
			auto* const own  = dynamic_cast<qt::workbench_splitter_handle*>(aHandle);
			const auto  edge = own != nullptr ? own->rule_edge() : -1;
			if (edge < 0) {
				return false;
			}
			// The rendering is in device pixels: the logical rule column is the
			// band of device columns it scales to, and one of them carries the
			// one device pixel wide cosmetic line.
			const auto image = aHandle->grab().toImage();
			const auto ratio = image.devicePixelRatio();
			const auto first = std::max(0, static_cast<int>(std::floor(edge * ratio)) - 1);
			const auto last  = std::min(image.width() - 1,
										static_cast<int>(std::ceil((edge + 1) * ratio)));
			for (auto x = first; x <= last; ++x) {
				if (image.pixelColor(x, image.height() / 2).rgb() == aColour) {
					return true;
				}
			}
			return false;
		};
		const auto& palette   = window.palette();
		const auto  mid       = palette.color(QPalette::Active, QPalette::Mid).rgb();
		const auto  highlight = palette.color(QPalette::Active, QPalette::Highlight).rgb();
		const auto  windowBg  = palette.color(QPalette::Active, QPalette::Window).rgb();
		expect(ruleShows(splitter->handle(2), mid)
				   && !ruleShows(splitter->handle(2), highlight),
			   "the handle under the pointer draws its rule at the same constant "
			   "weight as one the pointer has left, never the highlight colour");
		expect(ruleShows(handle, mid) && mid != windowBg,
			   "a handle the pointer is not over draws the drawer's edge rule, "
			   "distinct from the window colour");
		expect(dynamic_cast<qt::workbench_splitter_handle*>(splitter->handle(2))->rule_edge()
					   == (splitter->handle(2)->width() - 1) / 2
				   && dynamic_cast<qt::workbench_splitter_handle*>(handle)->rule_edge()
					   == (handle->width() - 1) / 2,
			   "each drawer rule occupies the middle column of its hot zone");

		// [[BPR-0746]]. Whether the seam's paint can witness where the pointer
		// is, measured here rather than reasoned from Qt's sources, which are
		// not on disk. BPR-0746 admits the owner's v008 screenshot — a lit rule
		// with an arrow cursor — as proof that the handle held Qt's pointer
		// state while the effective cursor was still the arrow, and refutes the
		// hit-zone cause on that reading. That reading is admissible only if the
		// pointer's arrival and departure actually repaint the handle, since a
		// pen that reads underMouse() otherwise keeps whatever state the last
		// unrelated repaint recorded. They do repaint it: Qt::WA_Hover is set on
		// the handle, arrival and departure each cost one paint, and
		// underMouse() follows them. So a lit rule places the pointer on the
		// handle at the moment the rule was drawn, and BPR-0746's diagnosis
		// keeps its foundation. If this ever goes red the screenshot stops being
		// evidence of pointer position and that diagnosis has to be reopened.
		// Read only at whole-number scales, for the same reason the
		// resting-pointer sequence above is.
		//
		// [[BPR-0824]]. Qt::WA_Hover is the style's to set, not the handle's:
		// the application's own style sets it when it polishes the handle, and
		// Fusion does too, but the basic Windows style that the headless Linux
		// lane forces through QT_STYLE_OVERRIDE does not, and without it Qt
		// sends the handle no hover event and nothing repaints it. The
		// screenshot is of the application under its own style, so a forced
		// style cannot speak to whether the attribute is set. Under one, the
		// handle is given the attribute its own style would give it, and what
		// the reading rests on -- a repaint on arrival and another on
		// departure, with underMouse() following both -- is still measured.
		const auto forcedStyle = !qEnvironmentVariableIsEmpty("QT_STYLE_OVERRIDE");
		const auto styleHover  = handle->testAttribute(Qt::WA_Hover);
		if (forcedStyle) {
			handle->setAttribute(Qt::WA_Hover, true);
		}
		struct paint_counter final : QObject
		{
			int Painted = 0;

			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aEvent != nullptr && aEvent->type() == QEvent::Paint) {
					++Painted;
				}
				return QObject::eventFilter(aWatched, aEvent);
			}
		};
		auto counter = paint_counter{ };
		handle->installEventFilter(&counter);
		// The pointer is on the files handle, away from this one.
		process_events();
		const auto settled = counter.Painted;
		process_events();
		const auto idle = counter.Painted;
		// It arrives.
		rest();
		const auto arrived    = counter.Painted;
		const auto heldOnRest = handle->underMouse();
		// And it leaves, onto the conversations drawer this handle borders —
		// one widget over, which is where a pointer aimed at the rule lands
		// when it misses, the rule being drawn in the handle's boundary column.
		auto* const drawer = window.conversations_drawer();
		restOn(splitter->handle(2));
		const auto left      = counter.Painted;
		const auto heldAfter = handle->underMouse();
		handle->removeEventFilter(&counter);
		std::cout << "seam repaint on pointer movement: style "
				  << QApplication::style()->name().toStdString()
				  << (forcedStyle ? " (forced)" : "") << " sets WA_Hover=" << styleHover
				  << " settled=" << settled
				  << " idle=" << idle << " on arrival=" << arrived << " on departure="
				  << left << "; underMouse on arrival=" << heldOnRest << " after departure="
				  << heldAfter << "; handleWidth=" << splitter->handleWidth()
				  << " rule column=" << dynamic_cast<qt::workbench_splitter_handle*>(handle)->rule_edge()
				  << " drawer x=" << (drawer != nullptr ? drawer->geometry().x() : -1) << "\n";
		expect(idle == settled,
			   "an idle event loop does not repaint the seam, so a repaint counted "
			   "below is the pointer's doing and nothing else");
		expect(fractional
				   || ((styleHover || forcedStyle) && arrived > idle && left > arrived
					   && heldOnRest && !heldAfter),
			   "the seam is repainted when the pointer arrives on it and again when the "
			   "pointer leaves, so a rule drawn from the pointer's state witnesses where "
			   "the pointer was when it was drawn");

		window.hide();
	}

	void test_a_clobbered_persisted_width_opens_at_the_design_width()
	{
		// [[BPR-0662]]. The host's floor is 64; a layout persisted at the
		// floor for every drawer was written by a squeeze, and opening at 64
		// would leave three unreadable slivers. The window grants the
		// design widths and reports nothing until the reviewer drags.
		intent_log           log;
		qt::workbench_window window{
			{ },
			{ },
			[&log](core::workbench_drawer aDrawer, std::uint32_t aWidth) {
				log.Width.push_back({ .Drawer = aDrawer, .Width = aWidth });
			}
		};
		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 64U,
			.ConversationsWidth = 64U,
			.RepositoriesWidth  = 64U,
		};
		window.resize(1'600, 720);
		window.show();
		process_events();
		window.publish(opened);
		process_events();
		expect(window.files_drawer()->width() == 320
				   && window.conversations_drawer()->width() == 375
				   && window.central_splitter()->sizes()[0] == 300,
			   "a width at the host's floor opens at the drawer's design width");
		expect(log.Width.empty(),
			   "granting the design width reports nothing to the host");
		window.hide();
	}

	void test_drawer_contents_use_incremental_model_signals()
	{
		qt::workbench_window window;
		auto* const          contents = window.files_drawer()->contents();
		expect(contents != nullptr
				   && window.files_drawer()->contents_view()->model() == contents,
			   "the drawer view is bound to its stable-identity model");

		model_signal_log emitted;
		observe_model_signals(*contents, emitted);

		contents->apply({ drawer_row("a", "alpha.cpp", "Modified"),
						  drawer_row("b", "beta.cpp", "Added"),
						  drawer_row("c", "gamma.cpp", "Deleted") });
		expect(contents->rowCount() == 3 && emitted.Inserted == 3U
				   && emitted.Reset == 0U,
			   "first contents arrive as row insertions rather than a reset");

		const QPersistentModelIndex retained{ contents->index_for_identity(
			QStringLiteral("b")) };
		expect(retained.isValid(), "a stable identity resolves to a live index");

		contents->apply({ drawer_row("a", "alpha.cpp", "Modified"),
						  drawer_row("b", "beta.cpp", "Modified"),
						  drawer_row("c", "gamma.cpp", "Deleted") });
		expect(emitted.Changed == 1U && emitted.Inserted == 3U
				   && emitted.Removed == 0U && emitted.Moved == 0U
				   && emitted.Reset == 0U,
			   "a bounded state change is one data change, not a reset");
		expect(retained.isValid()
				   && contents->identity_at(retained) == QStringLiteral("b"),
			   "a persistent index survives a bounded state change");
		expect(contents->data(retained, Qt::AccessibleDescriptionRole).toString()
				   == QStringLiteral("Modified"),
			   "the changed row carries its new accessible description");

		contents->apply({ drawer_row("b", "beta.cpp", "Modified"),
						  drawer_row("a", "alpha.cpp", "Modified"),
						  drawer_row("c", "gamma.cpp", "Deleted") });
		expect(emitted.Moved == 1U && emitted.Inserted == 3U
				   && emitted.Removed == 0U && emitted.Reset == 0U,
			   "a bounded reorder is one row move, not a reset");
		expect(retained.isValid() && retained.row() == 0
				   && contents->identity_at(retained) == QStringLiteral("b"),
			   "a moved row keeps its identity and its persistent index");

		contents->apply({ drawer_row("b", "beta.cpp", "Modified"),
						  drawer_row("c", "gamma.cpp", "Deleted") });
		expect(contents->rowCount() == 2 && emitted.Removed == 1U
				   && emitted.Inserted == 3U && emitted.Reset == 0U,
			   "a bounded removal is one row removal, not a reset");
		expect(contents->index_for_identity(QStringLiteral("a")).isValid() == false,
			   "a retired identity no longer resolves");

		contents->apply({ drawer_row("b", "beta.cpp", "Modified"),
						  drawer_row("d", "delta.cpp", "Renamed"),
						  drawer_row("c", "gamma.cpp", "Deleted") });
		expect(contents->rowCount() == 3 && emitted.Inserted == 4U
				   && emitted.Reset == 0U,
			   "a bounded insertion is one row insertion, not a reset");

		const auto statistics = contents->statistics();
		expect(statistics.ModelResets == 0U
				   && statistics.RowsInserted == emitted.Inserted
				   && statistics.RowsRemoved == emitted.Removed
				   && statistics.RowsMoved == emitted.Moved
				   && statistics.RowsChanged == emitted.Changed,
			   "the model's own record agrees with the signals it emitted");

		// The conversations drawer no longer has a list model of its own: it
		// hosts RFC-BPR-0002's threaded conversation, which is a forest and
		// cannot be a QAbstractListModel. So the old "each drawer owns its own
		// independent contents" reading is restated rather than kept -- the two
		// drawers stopped being the same kind of thing, which is exactly what
		// BPR-0191's route 2 decided.
		expect(window.conversations_drawer()->contents() == nullptr
				   && window.conversations_drawer()->contents_view() == nullptr,
			   "a drawer that hosts a tree reports no list, rather than a hidden "
			   "one a caller could mistake for its content");
		expect(window.files_drawer()->contents() == contents,
			   "the files drawer is untouched by that: a file list is a list");
	}

	void test_file_rows_emit_only_semantic_user_selection()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(std::move(aIntent));
			},
			{ },
			{ }
		};
		const auto first  = workbench_file(1U);
		const auto second = workbench_file(2U);
		auto       rows   = application::workbench_projection{ };
		rows.Files        = std::vector<application::workbench_file_projection>{
			{ .File = first, .Path = "first.cpp" },
			{ .File = second, .Path = "second.cpp" },
		};
		window.publish(rows);
		expect(
			log.Workbench.empty()
				&& window.files_drawer()->contents()->file_at(
					   window.files_drawer()->contents()->index(1, 0))
					== std::optional{ second },
			"projection inserts retain opaque file identity without emitting intent");

		auto* const view = window.files_drawer()->contents_view();
		view->setCurrentIndex(view->model()->index(1, 0));
		expect(
			log.Workbench.size() == 1U
				&& std::get<core::select_workbench_file_intent>(log.Workbench.back()).File
					== second,
			"list selection emits the retained semantic identity without parsing text");

		auto reordered  = application::workbench_projection{ };
		reordered.Files = std::vector<application::workbench_file_projection>{
			{ .File = second, .Path = "renamed-second.cpp" },
			{ .File = first, .Path = "first.cpp" },
		};
		reordered.Selection = core::workbench_selection_delta{
			.Cursor = core::workbench_navigation_cursor{
				.File              = first,
				.Ordinal           = 2U,
				.ActionableCount   = 2U,
				.PreviousAvailable = true,
				.NextAvailable     = false,
			},
		};
		window.publish(reordered);
		expect(
			log.Workbench.size() == 1U
				&& view->currentIndex()
					== window.files_drawer()->contents()->index_for_file(first),
			"incremental reorder and host-selected row emit no false user selection");
	}

	// [[BPR-0658]]. The files drawer shows the tree over its list: the same
	// rows, a row chosen in the tree selecting in the list (which is what
	// dispatches), the core's cursor reaching the tree, the overview's facts
	// reaching the rows, and the order and filter controls beside it.
	void test_the_files_drawer_shows_the_tree_over_its_list()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			}
		};
		window.resize(1'280, 820);
		window.show();
		process_events();
		const auto first  = core::file_identity_of("src/app/first.cpp");
		const auto second = core::file_identity_of("src/app/second.cpp");
		auto       opened = both_files_enabled();
		opened.Files      = std::vector<application::workbench_file_projection>{
			{ .File = first, .Path = "src/app/first.cpp" },
			{ .File = second, .Path = "src/app/second.cpp" },
		};
		window.publish(opened);
		process_events();
		auto* const tree = window.files_tree();
		expect(tree != nullptr && window.files_drawer()->isAncestorOf(tree)
				   && window.files_order_selector() != nullptr && window.files_filter() != nullptr
				   && tree->tree_model()->file_count() == 2U
				   && window.files_drawer()->contents()->rowCount() == 2,
			   "the tree lists the drawer's files and the list keeps them too");
		expect(tree->tree_model()->rowCount() == 1
				   && tree->tree_model()->index(0, 0).data().toString() == QStringLiteral("src/app"),
			   "the two files share one compacted folder row");
		log.Workbench.clear();
		tree->setCurrentIndex(tree->tree_model()->index_for_file(second));
		process_events();
		expect(log.Workbench.size() == 1U
				   && std::get<core::select_workbench_file_intent>(log.Workbench.back()).File == second
				   && window.files_drawer()->contents()->file_at(window.files_drawer()->contents_view()->currentIndex())
					   == std::optional{ second },
			   "choosing a row in the tree selects it in the list, which dispatches the intent once");

		auto selection      = application::workbench_projection{ };
		selection.Selection = core::workbench_selection_delta{
			.Cursor = core::workbench_navigation_cursor{
				.File              = first,
				.Ordinal           = 1U,
				.ActionableCount   = 2U,
				.PreviousAvailable = false,
				.NextAvailable     = true,
			},
		};
		log.Workbench.clear();
		window.publish(selection);
		process_events();
		expect(log.Workbench.empty()
				   && tree->tree_model()->file_at(tree->currentIndex()) == std::optional{ first },
			   "the core's cursor reaches the tree without a second intent");

		auto overview = application::pull_request_overview_projection{ };
		overview.Files.push_back(application::overview_file_row{
			.Path      = "src/app/first.cpp",
			.Name      = "first.cpp",
			.Operation = application::overview_file_operation::modified,
			.Additions = 5U,
			.Deletions = 1U,
			.Changed   = application::overview_change_since_review::changed,
		});
		window.publish(overview);
		process_events();
		const auto row = tree->tree_model()->index_for_file(first);
		expect(row.isValid() && row.data(qt::files_tree_model::additions_role).toULongLong() == 5U
				   && row.data(qt::files_tree_model::changed_role).toBool()
				   && tree->tree_model()->file_at(tree->currentIndex()) == std::optional{ first },
			   "the overview's facts reach the tree's rows and the selection survives");
		window.files_filter()->setText(QStringLiteral("second"));
		process_events();
		expect(tree->tree_model()->file_count() == 1U && !tree->tree_model()->index_for_file(first).isValid(),
			   "the filter narrows the tree");
		window.files_filter()->clear();
		window.hide();
	}

	void test_the_conversations_drawer_hosts_the_threaded_conversation()
	{
		qt::workbench_window window;

		auto* const panel = window.conversation_panel();
		expect(panel != nullptr, "the shell composes the conversation");
		expect(panel != nullptr && window.review_editor() != nullptr
				   && window.conversations_drawer()->isAncestorOf(panel)
				   && !window.conversations_drawer()->isAncestorOf(window.review_editor()),
			   "the conversation lives in the drawer while inline drafting has a dedicated page");

		// The drawer keeps every behaviour that makes it a drawer. Swapping what
		// it shows must not have cost it collapse, which is core-owned state.
		window.conversations_drawer()->adopt(
			core::workbench_drawer_state::collapsed);
		expect(window.conversations_drawer()->state()
				   == core::workbench_drawer_state::collapsed,
			   "a hosted drawer still adopts the collapse the core decided");
		window.conversations_drawer()->adopt(
			core::workbench_drawer_state::expanded);

		// The declared focus order names one stop per drawer. For the files
		// drawer that is still its list; for this one it is the tree, and the
		// window must not be reaching past the drawer to find it.
		expect(window.conversations_drawer()->focus_stop() == panel->view(),
			   "the drawer's one interactive stop is the conversation tree");
		// [[BPR-0658]]. The files drawer's stop is its tree; the list it
		// keeps as its authority is hidden.
		expect(window.files_drawer()->focus_stop() == window.files_tree()
				   && !window.files_drawer()->contents_view()->isVisibleTo(window.files_drawer()),
			   "the files drawer's stop is its tree, over the retained list");

		const auto order = window.focus_order();
		expect(std::find(order.begin(), order.end(),
						 static_cast<QWidget*>(panel->view()))
				   != order.end(),
			   "and the declared focus order names it");
		expect(std::find(order.begin(), order.end(), nullptr) == order.end(),
			   "no drawer contributes a null stop");
	}

	[[nodiscard]] auto close(
		const qreal aLeft,
		const qreal aRight,
		const qreal aTolerance = 0.001) noexcept -> bool
	{
		return std::abs(aLeft - aRight) <= aTolerance;
	}

	void process_events()
	{
		for (auto pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	[[nodiscard]] auto message_for(
		const std::string_view aMessage,
		const std::string_view aStage) -> std::string
	{
		return std::string{ aMessage } + " (" + std::string{ aStage } + ')';
	}

	template <typename TIdentity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> TIdentity
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return TIdentity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed)
		-> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	/// \brief Builds one comparison key, optionally for a distinct file.
	///
	/// \details The seed only moves the file identity and its two blobs, so a
	/// harness that publishes several different files into one window keeps a
	/// distinct immutable comparison for each. Seed zero is the original key.
	[[nodiscard]] auto comparison_key(
		const core::diff_options&                 aOptions,
		const std::uint8_t                        aSeed = 0U,
		const std::optional<core::file_identity>& aFile = std::nullopt)
		-> core::file_comparison_key
	{
		const auto digest = core::options_digest(aOptions);
		expect(digest.has_value(), "the comparison fixture options normalize");
		const auto scope     = opaque<core::pull_request_authority_scope_id>(11U);
		const auto authority = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(13U),
			.Account         = core::github_account_id{ 40U },
			.Viewer          = core::github_viewer_actor_id{ 50U },
			.Repository      = core::repository_id{ 101U },
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = core::pull_request_id{ 202U },
			.Number          = core::pull_request_number{ 17U },
			.Scope           = scope,
		};
		return {
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = {
				.Authority   = scope,
				.Incarnation = opaque<core::epoch_catalog_incarnation_id>(12U),
				.Kind        = core::comparison_anchor_kind::epoch_base,
				.Epoch       = core::epoch_ordinal{ 1 },
			},
			.RightAnchor = {
				.Authority   = scope,
				.Incarnation = opaque<core::epoch_catalog_incarnation_id>(12U),
				.Kind        = core::comparison_anchor_kind::epoch_head,
				.Epoch       = core::epoch_ordinal{ 2 },
			},
			.LeftBlob  = blob(static_cast<std::uint8_t>(21U + aSeed)),
			.RightBlob = blob(static_cast<std::uint8_t>(51U + aSeed)),
			.File      = aFile.value_or(opaque<core::file_identity>(static_cast<std::uint8_t>(31U + aSeed))),
			.Options   = *digest,
			.Algorithm = core::supported_diff_algorithm_revision,
		};
	}

	[[nodiscard]] auto source_bytes(const std::string_view aText)
		-> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	/// \brief Computes one immutable RFC-BPR-0004 comparison fixture.
	[[nodiscard]] auto computed_comparison(
		const std::string_view                    aLeft,
		const std::string_view                    aRight,
		const std::uint8_t                        aSeed = 0U,
		const std::optional<core::file_identity>& aFile = std::nullopt)
		-> core::computed_file_diff
	{
		const auto options = core::diff_options{ };
		auto       left    = source_bytes(aLeft);
		auto       right   = source_bytes(aRight);
		const auto outcome = core::compute_file_diff({
			.Key        = comparison_key(options, aSeed, aFile),
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ left },
			.RightBytes = std::span<const std::uint8_t>{ right },
		});
		expect(outcome.Reason == core::diff_reason_code::none
				   && outcome.Result.has_value(),
			   "the comparison fixture computes and publishes");
		return *outcome.Result;
	}

	[[nodiscard]] auto request_context(const core::computed_file_diff& aDiff)
		-> core::diff_request_context
	{
		return {
			.Request            = core::request_id{ 1U },
			.ApplicationSession = core::application_session_id{ 1U },
			.Key                = aDiff.Alignment.Key,
			.Generation         = core::request_generation{ 1U },
		};
	}

	/// \brief Builds two sources whose changes are spread through the file.
	///
	/// \details The two sides deliberately end up different lengths and carry
	/// empty-span insertions and deletions, so a surface that invented alignment
	/// rows to make both documents the same height could not pass.
	[[nodiscard]] auto comparison_fixture_text()
		-> std::pair<std::string, std::string>
	{
		auto       left   = std::string{ };
		auto       right  = std::string{ };
		const auto common = [&left, &right](
								const std::string_view aPrefix,
								const std::size_t      aCount) {
			for (std::size_t line = 0U; line < aCount; ++line) {
				const auto text = std::string{ aPrefix } + '-'
					+ std::to_string(line) + '\n';
				left += text;
				right += text;
			}
		};

		common("leading", 12U);
		left += "replaced-old-a\nreplaced-old-b\nreplaced-old-c\n";
		right += "replaced-new-a\nreplaced-new-b\n";
		common("alpha", 18U);
		right += "inserted-a\ninserted-b\ninserted-c\ninserted-d\n";
		common("beta", 18U);
		left += "deleted-a\ndeleted-b\n";
		common("gamma", 18U);
		left += "second-old-a\nsecond-old-b\n";
		right += "second-new-a\nsecond-new-b\nsecond-new-c\n";
		common("delta", 18U);
		right += "appended-a\nappended-b\nappended-c\n";
		common("tail", 16U);
		return { std::move(left), std::move(right) };
	}

	/// \brief Counts the source rows one composed pane document actually holds.
	///
	/// \details Qt leaves one trailing empty block for a document whose last
	/// source line ends in a newline. That block is a final-newline artifact,
	/// never a source row and never an alignment row.
	[[nodiscard]] auto document_rows(const QPlainTextEdit& aPane) -> std::uint32_t
	{
		const auto* const document = aPane.document();
		const auto        blocks   = static_cast<std::uint32_t>(document->blockCount());
		return blocks != 0U && document->lastBlock().text().isEmpty()
			? blocks - 1U
			: blocks;
	}

	/// \brief Measures live line boundaries from one composed natural pane.
	[[nodiscard]] auto measured_line_boundaries(
		const QPlainTextEdit& aPane,
		const std::uint32_t   aLines) -> std::vector<qreal>
	{
		const auto* const document   = aPane.document();
		const auto* const layout     = document->documentLayout();
		auto              extent     = document->documentMargin();
		auto              boundaries = std::vector<qreal>{ };
		boundaries.reserve(static_cast<std::size_t>(aLines) + 1U);
		boundaries.push_back(extent);
		auto block = document->begin();
		for (auto line = 0U; line < aLines && block.isValid(); ++line) {
			extent += layout->blockBoundingRect(block).height();
			boundaries.push_back(extent);
			block = block.next();
		}
		return boundaries;
	}

	[[nodiscard]] auto segment_index(
		const core::computed_file_diff&  aDiff,
		const core::alignment_segment_id aSegment) -> std::optional<std::size_t>
	{
		const auto found = std::ranges::find(
			aDiff.Alignment.Segments, aSegment, &core::alignment_segment::Id);
		return found == aDiff.Alignment.Segments.end()
			? std::nullopt
			: std::optional{ static_cast<std::size_t>(
				  std::distance(aDiff.Alignment.Segments.begin(), found)) };
	}

	[[nodiscard]] auto canonical_group(
		const core::computed_file_diff& aDiff,
		const core::change_group_id     aGroup) -> bool
	{
		return std::ranges::any_of(
			aDiff.Alignment.Groups,
			[aGroup](const core::change_group& aCandidate) {
				return aCandidate.Id == aGroup;
			});
	}

	/// \brief Records what one composed-surface observation actually painted.
	struct composed_observation
	{
		std::size_t Ribbons = 0U;
		std::size_t Wedges  = 0U;
		std::size_t Grouped = 0U;
	};

	/// \brief Re-checks every composed comparison rule at one interaction stage.
	///
	/// \details The stage name travels into each message so a failure names the
	/// exact scroll or resize state that produced it.
	[[nodiscard]] auto observe_composed_comparison(
		const qt::workbench_window&     aWindow,
		const core::computed_file_diff& aDiff,
		const QString&                  aLeftText,
		const QString&                  aRightText,
		const std::string_view          aStage) -> composed_observation
	{
		auto* const surface = aWindow.comparison_surface();
		auto* const left    = surface->left_pane();
		auto* const right   = surface->right_pane();

		const auto bars  = surface->comparison_splitter()
							   ->parentWidget()
							   ->findChildren<QScrollBar*>();
		const auto shown = std::ranges::count_if(
			bars, [](const QScrollBar* const aBar) {
				return aBar->orientation() == Qt::Vertical && aBar->isVisible();
			});
		expect(shown == 1,
			   message_for("the comparison region shows exactly one vertical "
						   "scrollbar",
						   aStage));
		expect(surface->shared_scrollbar()->isVisible()
				   && surface->shared_scrollbar()->isEnabled(),
			   message_for("the one visible vertical scrollbar is the shared "
						   "comparison scrollbar",
						   aStage));
		expect(left->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff
				   && right->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff,
			   message_for("neither top pane owns vertical scroll authority",
						   aStage));

		expect(left->toPlainText() == aLeftText
				   && right->toPlainText() == aRightText,
			   message_for("each top document is exactly its own source text",
						   aStage));
		expect(document_rows(*left) == aDiff.Alignment.LeftLineCount
				   && document_rows(*right) == aDiff.Alignment.RightLineCount,
			   message_for("neither top document contains a synthetic alignment "
						   "row",
						   aStage));

		const auto leftBoundaries = measured_line_boundaries(
			*left, aDiff.Alignment.LeftLineCount);
		const auto rightBoundaries = measured_line_boundaries(
			*right, aDiff.Alignment.RightLineCount);
		expect(leftBoundaries.size()
					   == static_cast<std::size_t>(aDiff.Alignment.LeftLineCount)
						   + 1U
				   && rightBoundaries.size()
					   == static_cast<std::size_t>(
							  aDiff.Alignment.RightLineCount)
						   + 1U,
			   message_for("each composed pane measures one boundary per source "
						   "line",
						   aStage));

		auto observation       = composed_observation{ };
		auto previous          = std::optional<std::size_t>{ };
		auto previousLeft      = qreal{ 0.0 };
		auto previousRight     = qreal{ 0.0 };
		auto previousLeftLine  = std::uint32_t{ 0U };
		auto previousRightLine = std::uint32_t{ 0U };
		expect(!surface->link_map()->visible_links().empty(),
			   message_for("the composed link map paints the visible "
						   "correspondences",
						   aStage));
		for (const auto& link : surface->link_map()->visible_links()) {
			const auto index = segment_index(aDiff, link.Identity.Segment);
			expect(index.has_value(),
				   message_for("every painted ribbon names a canonical "
							   "RFC-BPR-0004 segment",
							   aStage));
			const auto& segment = aDiff.Alignment.Segments[*index];
			expect(link.Kind == segment.Kind
					   && link.Identity.Group == segment.Group,
				   message_for("every ribbon carries its canonical segment kind "
							   "and group rather than a presentation-side "
							   "regrouping",
							   aStage));
			expect(segment.Group.has_value()
					   && canonical_group(aDiff, *segment.Group),
				   message_for("every painted correspondence belongs to a "
							   "canonical change group",
							   aStage));
			expect(close(link.LeftBottom - link.LeftTop,
						 leftBoundaries[segment.Left.End]
							 - leftBoundaries[segment.Left.Begin])
					   && close(link.RightBottom - link.RightTop,
								rightBoundaries[segment.Right.End]
									- rightBoundaries[segment.Right.Begin]),
				   message_for("each ribbon spans the measured pane interval of "
							   "its segment",
							   aStage));
			if (previous.has_value()) {
				expect(*index > *previous,
					   message_for("visible ribbons keep canonical segment "
								   "order",
								   aStage));
				expect(link.LeftTop >= previousLeft
						   && link.RightTop >= previousRight,
					   message_for("ordinary correspondence stays monotonic and "
								   "noncrossing",
								   aStage));
				expect(close(link.LeftTop - previousLeft,
							 leftBoundaries[segment.Left.Begin]
								 - leftBoundaries[previousLeftLine])
						   && close(link.RightTop - previousRight,
									rightBoundaries[segment.Right.Begin]
										- rightBoundaries[previousRightLine]),
					   message_for("ribbon spacing follows the measured pane "
								   "geometry between segments",
								   aStage));
			}
			previous          = index;
			previousLeft      = link.LeftTop;
			previousRight     = link.RightTop;
			previousLeftLine  = segment.Left.Begin;
			previousRightLine = segment.Right.Begin;
			++observation.Ribbons;
			observation.Grouped += link.Identity.Group.has_value() ? 1U : 0U;
			observation.Wedges += segment.Left.Begin == segment.Left.End
					|| segment.Right.Begin == segment.Right.End
				? 1U
				: 0U;
		}
		return observation;
	}

	/// \brief Waits, bounded, until the composed panes have measured their lines.
	///
	/// \details [[BPR-1073]]. The panes measure their lines in batches of about
	/// 4 ms, and the link map's links, the scroll controller's axis and the
	/// shared scrollbar's enabled state all come from the geometry that
	/// measurement completes. How many event passes that takes depends on the
	/// machine: four were enough on macOS and Linux, and on the Windows station
	/// a Debug build needed five. So a read of any of them waits for
	/// `geometry_pending()` to clear, as [[BPR-1033]]'s capture and
	/// `diff_workbench_tests`' `settle_geometry` do, and then drains as before.
	/// Where the measurement is already done it returns at once. The bound is a
	/// deadline, not a budget: the largest sized publication, 60,000 lines,
	/// measured for 12.6 s at 2.0 in a Windows Debug build.
	void settle_geometry(const qt::diff_workbench& aSurface, const std::string_view aStage)
	{
		auto measuring = QElapsedTimer{ };
		measuring.start();
		while (aSurface.geometry_pending() && measuring.elapsed() < 30'000) {
			QApplication::processEvents();
		}
		expect(!aSurface.geometry_pending(),
			   message_for("the composed panes finish measuring within the local test deadline", aStage));
		process_events();
	}

	/// \brief Publishes the fixture comparison into the composed workbench centre.
	[[nodiscard]] auto publish_fixture(
		qt::workbench_window&           aWindow,
		const core::computed_file_diff& aDiff,
		const std::source_location      aCaller = std::source_location::current()) -> qt::diff_workbench*
	{
		auto* const surface = aWindow.comparison_surface();
		const auto  result  = std::make_shared<const core::computed_file_diff>(aDiff);
		surface->set_loading(request_context(aDiff));
		surface->publish(bpr::qt::test::prepare_publication({
			.Context = request_context(aDiff),
			.Result  = result,
		}));
		process_events();
		// [[BPR-1286]]. A publication of more than 4,096 lines fills detached
		// documents over several turns and is adopted only then
		// ([[BPR-1264]]), with geometry_pending() true until it is. So the
		// surface's result is read once the geometry settles, not straight
		// after the publish.
		settle_geometry(*surface, "published at line " + std::to_string(aCaller.line()));
		expect(surface->current_result() == result,
			   "the composed surface accepts its exact publication");
		return surface;
	}

	void test_one_pane_places_threads_on_their_own_changed_rows()
	{
		const auto [leftText, rightText] = comparison_fixture_text();
		const auto diff                  = computed_comparison(
			leftText, rightText, 0U, core::file_identity_of("src/file.cpp"));
		qt::workbench_window window;
		window.resize(1'280, 1'000);
		window.show();
		process_events();
		auto* const surface = publish_fixture(window, diff);
		auto        onePane = both_files_enabled();
		onePane.OnePane     = core::workbench_one_pane_delta{
			.State = core::workbench_one_pane::enabled,
		};
		window.publish(onePane);
		process_events();
		window.set_conversation_endpoint_placement(true, true);
		window.conversation_panel()->adopt({
											   application::github_conversation_projection{
												   .Id                 = "old-thread-a",
												   .Kind               = application::github_conversation_kind::review_thread,
												   .Path               = "src/file.cpp",
												   .Line               = 13U,
												   .DiffSide           = application::github_conversation_diff_side::left,
												   .AnchorAvailability = application::github_conversation_anchor_availability::current,
											   },
											   application::github_conversation_projection{
												   .Id                 = "old-thread-b",
												   .Kind               = application::github_conversation_kind::review_thread,
												   .Path               = "src/file.cpp",
												   .Line               = 13U,
												   .DiffSide           = application::github_conversation_diff_side::left,
												   .AnchorAvailability = application::github_conversation_anchor_availability::current,
											   },
											   application::github_conversation_projection{
												   .Id                 = "new-thread",
												   .Kind               = application::github_conversation_kind::review_thread,
												   .Path               = "src/file.cpp",
												   .Line               = 13U,
												   .DiffSide           = application::github_conversation_diff_side::right,
												   .AnchorAvailability = application::github_conversation_anchor_availability::current,
											   },
										   },
										   QStringLiteral("three conversations"));
		process_events();
		const auto oldRow = surface->line_rect(core::diff_side::left, 12U);
		const auto newRow = surface->line_rect(core::diff_side::right, 12U);
		expect(!oldRow.isEmpty() && !newRow.isEmpty()
				   && oldRow.y() < newRow.y(),
			   "removed and added threads have distinct unified rows in source order");
		const auto& cards = window.conversation_panel()->cards()->cards();
		expect(cards.size() == 3U
				   && cards[0].Rect.bottom() < cards[1].Rect.top() && cards[1].Rect.bottom() < cards[2].Rect.top()
				   && window.conversation_panel()->cards()->marked_lines(core::diff_side::left) == std::vector<std::uint32_t>{ 12U }
				   && window.conversation_panel()->cards()->marked_lines(core::diff_side::right) == std::vector<std::uint32_t>{ 12U },
			   "compact index entries do not overlap and preserve both exact old/new-side line marks");
		const auto selected = window.conversation_model()->index_for_id("old-thread-b");
		window.conversation_view()->setCurrentIndex(selected);
		process_events();
		const auto hiddenDetail = [&](const std::string& aIdentity) {
			const auto index = window.conversation_model()->index_for_id(aIdentity);
			return !index.isValid() || window.conversation_view()->isRowHidden(index.row(), { });
		};
		expect(window.conversation_panel()->cards()->current_thread() == "old-thread-b"
				   && !hiddenDetail("old-thread-b") && hiddenDetail("old-thread-a") && hiddenDetail("new-thread"),
			   "the exact selected root owns the single detail while other roots have no duplicate detail");
		const auto rendered = window.grab();
		expect(!rendered.isNull(), "the one-pane thread state renders in the composed window");
		if (const auto path = qEnvironmentVariable("BPR_QT_RENDER_PATH"); !path.isEmpty()) {
			const QFileInfo file{ path };
			expect(rendered.save(
					   QStringLiteral("%1/%2-one-pane-threads.png").arg(file.path(), file.completeBaseName()), "PNG"),
				   "the one-pane thread state is saved for inspection");
		}
	}

	/// \brief Proves the View menu's decision actually reaches the source panes.
	///
	/// \details This is the seam the rest of the line-number evidence does not
	/// cross. The core test proves the state changes, the window test proves the
	/// action follows the projection, and the diff-workbench test proves a bare
	/// surface honours the flag — and all three pass with the one line that
	/// connects the window to its surface deleted. Only this test fails then.
	void test_the_view_menu_reaches_the_composed_source_panes()
	{
		const auto [leftText, rightText] = comparison_fixture_text();
		const auto diff                  = computed_comparison(leftText, rightText);

		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		process_events();
		auto* const surface = publish_fixture(window, diff);
		expect(surface->left_pane()->blockCount() > 1
				   && surface->right_pane()->blockCount() > 1,
			   "the composed panes hold the fixture's source");

		const auto gutterOf = [](QPlainTextEdit* const aPane) -> QWidget* {
			auto* const found = aPane->findChild<QWidget*>(
				QStringLiteral("diffSourceLineNumbers"));
			expect(found != nullptr, "each composed pane has its gutter");
			return found;
		};

		auto shownWidth  = std::vector<int>{ };
		auto shownOrigin = std::vector<int>{ };
		for (auto* const source : { surface->left_pane(), surface->right_pane() }) {
			shownWidth.push_back(gutterOf(source)->width());
			shownOrigin.push_back(source->viewport()->x());
			expect(shownWidth.back() > 0,
				   "the composed panes start with numbers drawn");
		}
		const auto ribbonEndpoints = [surface] {
			auto endpoints = std::vector<std::array<qreal, 4>>{ };
			for (const auto& link : surface->link_map()->visible_links()) {
				endpoints.push_back({ link.LeftTop,
									  link.LeftBottom,
									  link.RightTop,
									  link.RightBottom });
			}
			return endpoints;
		};
		const auto shownRibbons = ribbonEndpoints();
		const auto shownDetail  = surface->detail_pane()->width();
		expect(!shownRibbons.empty(),
			   "the composed link map paints ribbons to begin with");

		auto hidden        = both_files_enabled();
		hidden.LineNumbers = core::workbench_line_numbers_delta{
			.State = core::workbench_line_numbers::hidden,
		};
		window.publish(hidden);
		process_events();
		// Hiding the gutter changes the panes' viewports, so they measure again.
		settle_geometry(*surface, "numbers hidden");

		for (auto        index = std::size_t{ 0U };
			 auto* const source : { surface->left_pane(), surface->right_pane() }) {
			auto* const gutter = gutterOf(source);
			// The published decision has to travel window -> surface -> pane.
			// Cutting that wire leaves every other line-number test green.
			expect(!gutter->isVisible() && gutter->width() == 0,
				   "a published hidden state reaches the composed panes");
			expect(shownOrigin[index] - source->viewport()->x()
					   == shownWidth[index],
				   "the composed pane gave the gutter's exact width back to the "
				   "code");
			++index;
		}

		// BPR-0208's class of defect, in the state no other suite reaches: the
		// text viewport just moved, and the band is measured from a live
		// viewport rather than inferred, so a stale band would put the ribbons
		// somewhere the code no longer is.
		expect(ribbonEndpoints() == shownRibbons,
			   "every ribbon endpoint is exactly where it was, because hiding "
			   "the gutter moved the text horizontally and a ribbon's vertical "
			   "attachment is measured from the live viewport rather than "
			   "inferred");
		expect(surface->detail_pane()->width() == shownDetail,
			   "the lower detail pane keeps the centre's width in both states");

		auto shown        = both_files_enabled();
		shown.LineNumbers = core::workbench_line_numbers_delta{
			.State = core::workbench_line_numbers::shown,
		};
		window.publish(shown);
		process_events();
		settle_geometry(*surface, "numbers shown");
		for (auto        index = std::size_t{ 0U };
			 auto* const source : { surface->left_pane(), surface->right_pane() }) {
			expect(gutterOf(source)->width() == shownWidth[index]
					   && source->viewport()->x() == shownOrigin[index],
				   "publishing shown restores the exact composed geometry");
			++index;
		}
		expect(ribbonEndpoints() == shownRibbons
				   && surface->detail_pane()->width() == shownDetail,
			   "the ribbons and the detail pane are unmoved across the round "
			   "trip");
		window.hide();
	}

	void test_selected_file_filter_survives_diff_loading()
	{
		qt::workbench_window window;
		const auto           first  = core::file_identity_of("src/first.cpp");
		const auto           second = core::file_identity_of("src/second.cpp");
		const auto           select = [&](core::file_identity aFile) {
			auto projection      = both_files_enabled();
			projection.Selection = core::workbench_selection_delta{
				.Cursor = core::workbench_navigation_cursor{ .File = aFile, .Ordinal = 1U, .ActionableCount = 2U }
			};
			window.publish(projection);
		};
		select(first);
		window.conversation_panel()->adopt({ { .Id = "first", .Kind = application::github_conversation_kind::review_thread, .Path = "src/first.cpp" },
											 { .Id = "second", .Kind = application::github_conversation_kind::review_thread, .Path = "src/second.cpp" },
											 { .Id = "unrelated", .Kind = application::github_conversation_kind::review_thread, .Path = "src/unrelated.cpp" } },
										   QStringLiteral("three threads"));
		expect(window.conversation_model()->rowCount() == 1 && window.conversation_model()->index_for_id("first").isValid(), "core file selection filters before a diff arrives");
		select(second);
		window.comparison_surface()->clear();
		window.set_conversation_endpoint_placement(false, false);
		expect(window.conversation_panel()->anchor_file() == second
				   && window.conversation_model()->rowCount() == 1 && window.conversation_model()->index_for_id("second").isValid(),
			   "clearing the previous diff cannot widen the selected file to the whole review");
		const auto diff = computed_comparison("old\n", "new\n", 0U, second);
		static_cast<void>(publish_fixture(window, diff));
		expect(window.conversation_panel()->anchor_file() == second && window.conversation_model()->rowCount() == 1, "accepted diff keeps the same selected file filter");
		window.comparison_surface()->clear();
		auto cleared      = both_files_enabled();
		cleared.Selection = core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{ } };
		window.publish(cleared);
		expect(!window.conversation_panel()->anchor_file() && window.conversation_model()->rowCount() == 3, "an explicit absent core file selection restores the whole-review view");
	}

	void test_same_automatically_selected_line_click_filters_conversations()
	{
		const auto diff = computed_comparison(
			"shared-0\nshared-1\nshared-2\n",
			"shared-0\nshared-1\nshared-2\n",
			0U,
			core::file_identity_of("src/file.cpp"));
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		process_events();
		auto* const surface = publish_fixture(window, diff);
		expect(surface->selection().has_value()
				   && surface->selection()->Kind
					   == core::semantic_selection_kind::line,
			   "the fixture publication begins with an automatic semantic line selection");
		const auto automaticallySelected = surface->selection()->PreferredLine;
		window.set_conversation_endpoint_placement(true, true);
		window.conversation_panel()->adopt({
											   application::github_conversation_projection{
												   .Id       = "automatic-line-thread",
												   .Kind     = application::github_conversation_kind::review_thread,
												   .Path     = "src/file.cpp",
												   .Line     = automaticallySelected.Line + 1U,
												   .DiffSide = automaticallySelected.Side == core::diff_side::left
													   ? application::github_conversation_diff_side::left
													   : application::github_conversation_diff_side::right,
												   .AnchorAvailability
												   = application::github_conversation_anchor_availability::current,
											   },
										   },
										   QStringLiteral("one conversation"));
		expect(!window.conversation_panel()->anchor_line().has_value(),
			   "an automatic publication selection leaves conversations at file scope");

		auto applicationSelections = 0U;
		surface->set_selection_sink([&applicationSelections](const auto&) {
			++applicationSelections;
		});
		const auto  before = applicationSelections;
		auto* const pane   = automaticallySelected.Side == core::diff_side::left
			? surface->left_pane()
			: surface->right_pane();
		const auto  block  = pane->document()->findBlockByNumber(
			static_cast<int>(automaticallySelected.Line));
		send_click(
			*pane->viewport(),
			pane->cursorRect(QTextCursor{ block }).center());
		process_events();
		expect(
			window.conversation_panel()->anchor_line()
					== std::optional{ automaticallySelected }
				&& window.conversation_model()->rowCount() == 1
				&& window.conversation_model()
					   ->index_for_id("automatic-line-thread")
					   .isValid(),
			"clicking the already-selected line still reports an interactive gesture "
			"and filters to its exact conversation");
		expect(applicationSelections == before,
			   "an unchanged same-line click does not duplicate the application selection sink");
	}

	void test_conversation_selection_reveals_exact_diff_anchor()
	{
		const auto diff = computed_comparison(
			"left-0\nleft-1\nleft-2\nleft-3\n",
			"right-0\nright-1\nright-2\nright-3\n",
			0U,
			core::file_identity_of("src/file.cpp"));
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		process_events();
		auto* const surface = publish_fixture(window, diff);
		window.set_conversation_endpoint_placement(true, true);
		window.conversation_panel()->set_anchor_line(std::nullopt);
		window.conversation_panel()->adopt({
											   application::github_conversation_projection{
												   .Id       = "right-thread",
												   .Kind     = application::github_conversation_kind::review_thread,
												   .Path     = "src/file.cpp",
												   .Line     = 3U,
												   .DiffSide = application::github_conversation_diff_side::right,
												   .AnchorAvailability
												   = application::github_conversation_anchor_availability::current,
											   },
											   application::github_conversation_projection{
												   .Id       = "right-reply",
												   .ParentId = "right-thread",
												   .Kind     = application::github_conversation_kind::review_comment,
												   .Path     = "src/file.cpp",
												   .Line     = 3U,
												   .DiffSide = application::github_conversation_diff_side::right,
												   .AnchorAvailability
												   = application::github_conversation_anchor_availability::current,
											   },
											   application::github_conversation_projection{
												   .Id       = "right-deep-reply",
												   .ParentId = "right-reply",
												   .Kind     = application::github_conversation_kind::review_comment,
												   .Path     = "src/file.cpp",
												   .Line     = 3U,
												   .DiffSide = application::github_conversation_diff_side::right,
												   .AnchorAvailability
												   = application::github_conversation_anchor_availability::current,
											   },
											   application::github_conversation_projection{
												   .Id       = "left-thread",
												   .Kind     = application::github_conversation_kind::review_thread,
												   .Path     = "src/file.cpp",
												   .Line     = 4U,
												   .DiffSide = application::github_conversation_diff_side::left,
												   .AnchorAvailability
												   = application::github_conversation_anchor_availability::current,
											   },
											   application::github_conversation_projection{
												   .Id   = "file-thread",
												   .Kind = application::github_conversation_kind::review_thread,
												   .Path = "src/file.cpp",
												   .AnchorAvailability
												   = application::github_conversation_anchor_availability::file_level,
											   },
										   },
										   QStringLiteral("three conversations"));
		expect(window.conversation_model()->rowCount() == 3
				   && window.conversation_model()->index_for_id("right-thread").isValid(),
			   "the unanchored real panel path initially exposes all fixture threads");
		// The compact index initially selects a detail without moving code.
		// Exercise an actual selection change through the existing anchor route.
		window.conversation_view()->setCurrentIndex({ });
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id("right-thread"));
		process_events();
		expect(surface->conversation_anchor().has_value()
				   && surface->conversation_anchor()->Side == core::diff_side::right
				   && surface->conversation_anchor()->Line == 2U,
			   "right drawer anchor converts one-based GitHub line to the exact zero-based later pane line");
		expect(window.conversation_model()->rowCount() == 1
				   && window.conversation_model()->index_for_id("right-thread").isValid()
				   && window.conversation_model()->index_for_id("right-reply").parent()
					   == window.conversation_model()->index_for_id("right-thread")
				   && window.conversation_model()->index_for_id("right-deep-reply").parent()
					   == window.conversation_model()->index_for_id("right-reply"),
			   "the real panel path retains the selected thread while reverse-filtering to its exact source line");
		auto applicationSelections = 0U;
		surface->set_selection_sink([&applicationSelections](const auto&) {
			++applicationSelections;
		});
		const auto selectionsBeforeSameLine = applicationSelections;
		const auto rightBlock               = surface->right_pane()->document()->findBlockByNumber(2);
		send_click(
			*surface->right_pane()->viewport(),
			surface->right_pane()->cursorRect(QTextCursor{ rightBlock }).center());
		process_events();
		expect(window.conversation_view()->currentIndex()
					   == window.conversation_model()->index_for_id("right-thread")
				   && surface->conversation_anchor().has_value()
				   && surface->conversation_anchor()->Side == core::diff_side::right
				   && surface->conversation_anchor()->Line == 2U
				   && !surface->conversation_anchor_marker_rect(core::diff_side::right)
						   .isEmpty()
				   && applicationSelections == selectionsBeforeSameLine,
			   "a physical click on the selected root's exact line retains the root and marker without duplicating the application sink");
		const auto visibleConnectors = surface->link_map()->visible_links();
		const auto connector         = std::ranges::find_if(
			visibleConnectors,
			[](const qt::visible_diff_link& aLink) {
				return aLink.Identity.Group.has_value();
			});
		expect(connector != visibleConnectors.end(),
			   "the fixture exposes a semantic connector for the selection-lifetime course");
		const auto connectorPoint = QPointF{
			connector->Path.boundingRect().center().x(),
			(connector->LeftTop + connector->LeftBottom + connector->RightTop
			 + connector->RightBottom)
				/ 4.0,
		};
		send_click(*surface->link_map(), connectorPoint);
		process_events();
		expect(!surface->conversation_anchor().has_value()
				   && !window.conversation_view()->currentIndex().isValid()
				   && !window.conversation_panel()->anchor_line().has_value()
				   && !window.classify_defect_action()->isEnabled(),
			   "a connector gesture clears the drawer selection and defect action with its conversation marker");
		window.conversation_panel()->set_anchor_line(std::nullopt);
		surface->clear_conversation_anchor();
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id("right-deep-reply"));
		process_events();
		expect(surface->conversation_anchor().has_value()
				   && surface->conversation_anchor()->Side == core::diff_side::right
				   && surface->conversation_anchor()->Line == 2U
				   && window.conversation_model()->rowCount() == 1
				   && window.conversation_model()->index_for_id("right-thread").isValid()
				   && window.conversation_model()->index_for_id("right-reply").isValid()
				   && window.conversation_model()
						  ->index_for_id("right-deep-reply")
						  .isValid(),
			   "selecting a deep reply resolves its root and reveals the same exact line while retaining the full thread forest");
		window.conversation_panel()->set_anchor_line(std::nullopt);
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id("left-thread"));
		process_events();
		expect(surface->conversation_anchor().has_value()
				   && surface->conversation_anchor()->Side == core::diff_side::left
				   && surface->conversation_anchor()->Line == 3U,
			   "left drawer anchor converts one-based GitHub line to the exact zero-based earlier pane line");
		window.conversation_panel()->set_anchor_line(std::nullopt);
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id("file-thread"));
		process_events();
		expect(!surface->conversation_anchor().has_value(),
			   "an explicitly unplaced file-level thread clears the source marker without guessing");

		window.set_conversation_endpoint_placement(false, false);
		window.conversation_panel()->set_anchor_line(std::nullopt);
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id("right-thread"));
		process_events();
		const auto mismatched = window.conversation_model()->index_for_id(
			"right-thread");
		expect(!surface->conversation_anchor().has_value()
				   && !mismatched.isValid(),
			   "a later current-head thread is hidden against an older comparison");
		const auto anchoredRightBlock = surface->right_pane()->document()->findBlockByNumber(2);
		send_click(
			*surface->right_pane()->viewport(),
			surface->right_pane()
				->cursorRect(QTextCursor{ anchoredRightBlock })
				.center());
		process_events();
		expect(!window.conversation_panel()->anchor_line().has_value()
				   && window.conversation_model()->rowCount() == 1,
			   "a non-current line gesture keeps unplaced file-level scope without relisting later threads");

		window.set_conversation_endpoint_placement(true, true);
		window.conversation_model()->apply({
			application::github_conversation_projection{
				.Id       = "invalid-side-thread",
				.Kind     = application::github_conversation_kind::review_thread,
				.Path     = "src/file.cpp",
				.Line     = 3U,
				.DiffSide = static_cast<application::github_conversation_diff_side>(
					255U),
				.AnchorAvailability
				= application::github_conversation_anchor_availability::current,
			},
		});
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id("invalid-side-thread"));
		process_events();
		const auto invalidSide = window.conversation_model()->index_for_id("invalid-side-thread");
		expect(!surface->conversation_anchor().has_value()
				   && invalidSide
						   .data(qt::github_conversation_model::anchor_availability_role)
						   .toInt()
					   == static_cast<int>(application::
											   github_conversation_anchor_availability::missing_side)
				   && !window.classify_defect_action()->isEnabled(),
			   "the composed Qt boundary refuses an invalid projected side instead of revealing or classifying it as right");

		window.set_conversation_endpoint_placement(false, true);
		auto pullRequest = application::github_pull_request_projection{
			.ReviewThreadCount      = 1U,
			.ReviewThreadsAvailable = true,
			.Conversations          = {
				application::github_conversation_projection{
					.Id       = "refresh-thread",
					.Kind     = application::github_conversation_kind::review_thread,
					.Path     = "src/file.cpp",
					.Line     = 3U,
					.DiffSide = application::github_conversation_diff_side::right,
					.AnchorAvailability
					= application::github_conversation_anchor_availability::current,
				},
			},
		};
		window.publish(application::github_presentation_update{
			.PullRequest = pullRequest,
		});
		// [[BPR-0985]]. The sole thread is the drawer's default row, which
		// moves no code until the reviewer chooses it.
		window.conversation_panel()->choose(
			window.conversation_model()->index_for_id("refresh-thread"));
		process_events();
		expect(surface->conversation_anchor().has_value()
				   && surface->conversation_anchor()->Line == 2U,
			   "the refresh fixture begins with its current right-side line anchored");

		window.conversation_panel()->set_anchor_line(std::nullopt);
		pullRequest.Conversations.front().Line.reset();
		pullRequest.Conversations.front().OriginalLine       = 3U;
		pullRequest.Conversations.front().AnchorAvailability = application::
			github_conversation_anchor_availability::outdated_revision;
		window.publish(application::github_presentation_update{
			.PullRequest = pullRequest,
		});
		process_events();
		expect(!surface->conversation_anchor().has_value()
				   && window.conversation_model()
						   ->index_for_id("refresh-thread")
						   .data(qt::github_conversation_model::anchor_availability_role)
						   .toInt()
					   == static_cast<int>(application::
											   github_conversation_anchor_availability::outdated_revision),
			   "same-identity current-to-outdated refresh clears the stale source marker");

		pullRequest.Conversations.front().Line = 4U;
		pullRequest.Conversations.front().OriginalLine.reset();
		pullRequest.Conversations.front().AnchorAvailability = application::
			github_conversation_anchor_availability::current;
		window.publish(application::github_presentation_update{
			.PullRequest = pullRequest,
		});
		process_events();
		expect(surface->conversation_anchor().has_value()
				   && surface->conversation_anchor()->Side == core::diff_side::right
				   && surface->conversation_anchor()->Line == 3U,
			   "same-identity line-change refresh moves the marker to the new exact line");
	}

	// [[BPR-0985]]. The drawer makes its first thread current to show a
	// detail. That is not the reviewer's choice: the application places the
	// conversation endpoints again after every published file diff, and doing
	// so must neither move the reader's line nor report a reading gesture, which
	// superseded the saved reading a restart was restoring. Choosing the same
	// thread is the reviewer's, and does both.
	void test_a_default_conversation_row_never_moves_code()
	{
		const auto diff = computed_comparison(
			"left-0\nleft-1\nleft-2\nleft-3\n",
			"right-0\nright-1\nright-2\nright-3\n",
			0U,
			core::file_identity_of("src/file.cpp"));
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		process_events();
		auto* const surface  = publish_fixture(window, diff);
		auto        gestures = 0U;
		surface->set_reading_gesture_sink([&gestures] { ++gestures; });
		window.set_conversation_endpoint_placement(true, true);
		window.conversation_panel()->adopt({
											   application::github_conversation_projection{
												   .Id       = "right-thread",
												   .Kind     = application::github_conversation_kind::review_thread,
												   .Path     = "src/file.cpp",
												   .Line     = 3U,
												   .DiffSide = application::github_conversation_diff_side::right,
												   .AnchorAvailability
												   = application::github_conversation_anchor_availability::current,
											   },
											   application::github_conversation_projection{
												   .Id       = "left-thread",
												   .Kind     = application::github_conversation_kind::review_thread,
												   .Path     = "src/file.cpp",
												   .Line     = 4U,
												   .DiffSide = application::github_conversation_diff_side::left,
												   .AnchorAvailability
												   = application::github_conversation_anchor_availability::current,
											   },
										   },
										   QStringLiteral("two conversations"));
		process_events();
		const auto reading = surface->selection();
		expect(reading.has_value()
				   && window.conversation_model()->id_for_index(window.conversation_view()->currentIndex()) == "right-thread"
				   && window.conversation_panel()->current_is_default(),
			   "the drawer makes its first thread current by default beside a published reading");
		window.set_conversation_endpoint_placement(true, true);
		process_events();
		expect(!surface->conversation_anchor().has_value() && surface->selection() == reading && gestures == 0U,
			   "placing the endpoints again after a published diff leaves a default thread's line unrevealed and reports no reading gesture");

		auto* const cards = window.conversation_panel()->cards();
		cards->setFocus(Qt::OtherFocusReason);
		auto choose = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(cards, &choose);
		process_events();
		expect(!window.conversation_panel()->current_is_default()
				   && window.conversation_model()->id_for_index(window.conversation_view()->currentIndex()) == "right-thread"
				   && surface->conversation_anchor().has_value()
				   && surface->conversation_anchor()->Side == core::diff_side::right
				   && surface->conversation_anchor()->Line == 2U
				   && surface->selection() != reading && gestures == 1U,
			   "choosing the default thread's card reveals its exact line as one reading gesture");
		const auto chosen = surface->selection();
		window.set_conversation_endpoint_placement(true, true);
		process_events();
		expect(surface->conversation_anchor().has_value() && surface->conversation_anchor()->Line == 2U
				   && surface->selection() == chosen,
			   "a chosen thread stays revealed when the endpoints are placed again");
	}

	void test_conversation_line_outside_selected_comparison_is_explicit()
	{
		const auto diff = computed_comparison(
			"left-0\nleft-1\nleft-2\nleft-3\n",
			"right-0\nright-1\nright-2\nright-3\n",
			0U,
			core::file_identity_of("src/file.cpp"));
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		process_events();
		auto* const surface = publish_fixture(window, diff);
		window.set_conversation_endpoint_placement(false, true);
		window.conversation_panel()->adopt(
			{
				application::github_conversation_projection{
					.Id       = "outside-thread",
					.Kind     = application::github_conversation_kind::review_thread,
					.Path     = "src/file.cpp",
					.Line     = 999U,
					.DiffSide = application::github_conversation_diff_side::right,
					.AnchorAvailability
					= application::github_conversation_anchor_availability::current,
				},
			},
			QStringLiteral("one conversation"));
		const auto outside = window.conversation_model()->index_for_id("outside-thread");
		window.conversation_view()->setCurrentIndex({ });
		window.conversation_view()->setCurrentIndex(outside);
		process_events();
		const auto refused = window.conversation_model()->index_for_id("outside-thread");
		expect(
			!surface->conversation_anchor().has_value()
				&& refused
						.data(qt::github_conversation_model::anchor_availability_role)
						.toInt()
					== static_cast<int>(application::
											github_conversation_anchor_availability::selected_line_out_of_bounds)
				&& refused.data(Qt::DisplayRole)
					   .toString()
					   .contains(QStringLiteral("outside the selected comparison")),
			"a provider-valid line beyond the selected blob receives an explicit refusal and no marker");
	}

	void test_refresh_gap_retains_only_visual_conversation_scope()
	{
		const auto           file = core::file_identity_of("src/a.cpp");
		qt::workbench_window window;
		auto                 pr = application::github_pull_request_projection{ };
		for (auto index = 0; index < 1000; ++index) {
			pr.Conversations.push_back(application::github_conversation_projection{
				.Id   = "thread-" + std::to_string(index),
				.Kind = application::github_conversation_kind::review_thread,
				.Path = index < 2 ? "src/a.cpp" : "src/other.cpp",
			});
		}
		window.publish(application::github_presentation_update{ .PullRequest = pr });
		auto selected = application::workbench_projection{
			.Context   = { .Session = core::application_session_id{ 1U }, .Generation = core::request_generation{ 1U } },
			.Selection = core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{ .File = file, .Ordinal = 1U, .ActionableCount = 1U } },
			.Files     = std::vector{ application::workbench_file_projection{ .File = file, .Path = "src/a.cpp" } },
		};
		window.publish(selected);
		window.conversation_view()->setCurrentIndex(window.conversation_model()->index_for_id("thread-0"));
		const auto measurements = window.conversation_panel()->cards()->measurements();
		auto       gap          = application::workbench_projection{
			.Context   = { .Session = core::application_session_id{ 1U }, .Generation = core::request_generation{ 2U } },
			.Selection = core::workbench_selection_delta{ },
			.Files     = std::vector<application::workbench_file_projection>{ },
		};
		window.publish(gap);
		process_events();
		expect(window.conversation_panel()->anchor_file() == std::optional{ file }
				   && !window.files_drawer()->contents_view()->currentIndex().isValid(),
			   "new-generation observation gap keeps visual scope without retaining semantic file selection");
		expect(window.conversation_panel()->cards()->thread_count() == 2U,
			   "contextual refresh gap keeps the selected file's two compact index entries");
		expect(window.conversation_panel()->cards()->measurements() == measurements,
			   "refresh gap does not remeasure the thousand-thread forest");
		auto observed              = selected;
		observed.Context           = { .Request = core::request_id{ 4U }, .Session = gap.Context.Session, .Generation = gap.Context.Generation };
		observed.Selection->Cursor = core::workbench_navigation_cursor{ .ActionableCount = 1U, .PreviousAvailable = true, .NextAvailable = true };
		window.publish(observed);
		process_events();
		// [[BPR-0818]]. Nothing selects the file again after the terminal, so
		// the drawer shows the whole pull request the reviewer now has open.
		expect(!window.conversation_panel()->anchor_file().has_value()
				   && window.conversation_panel()->cards()->thread_count() == 1000U
				   && !window.files_drawer()->contents_view()->currentIndex().isValid(),
			   "successful file observation ends the visual scope at its unselected terminal");
		selected.Context.Generation = core::request_generation{ 2U };
		selected.Context.Request    = core::request_id{ 0U };
		window.publish(selected);
		expect(window.conversation_model()->id_for_index(window.conversation_view()->currentIndex()) == "thread-0",
			   "refresh completion preserves the exact selected conversation");
		window.publish(gap);
		expect(!window.conversation_panel()->anchor_file().has_value() && window.conversation_panel()->cards()->thread_count() == 1000U,
			   "same-generation explicit selection clear still widens to the whole pull request");
		window.publish(selected);
		gap.Context.Generation = core::request_generation{ 3U };
		window.publish(gap);
		window.fail(application::workbench_failure_publication{ .Context = selected.Context, .Failure = application::workbench_host_failure::observation_unavailable });
		expect(window.conversation_panel()->anchor_file().has_value(), "an older failure cannot release the new refresh visual scope");
		window.fail(application::workbench_failure_publication{ .Context = gap.Context, .Failure = application::workbench_host_failure::observation_unavailable });
		expect(!window.conversation_panel()->anchor_file().has_value(), "matching observation failure ends retained refresh scope");
		selected.Context.Generation = core::request_generation{ 3U };
		window.publish(selected);
		gap.Context.Generation = core::request_generation{ 4U };
		window.publish(gap);
		window.publish(application::github_presentation_update{ .Transition = application::github_transition_projection{ .PullRequest = core::pull_request_summary_delta{ } } });
		expect(window.conversation_panel()->cards()->cards().empty(), "authority invalidation clears every retained conversation immediately");
		window.publish(application::github_presentation_update{ .PullRequest = pr });
		gap.Context.Generation = core::request_generation{ 5U };
		window.publish(gap);
		expect(!window.conversation_panel()->anchor_file().has_value() && window.conversation_panel()->cards()->thread_count() == 1000U,
			   "a new PR cannot inherit the old refresh scope");
		selected.Context.Generation = core::request_generation{ 5U };
		window.publish(selected);
		gap.Context.Generation = core::request_generation{ 6U };
		window.publish(gap);
		window.publish(application::workbench_projection{
			.Context   = { .Request = core::request_id{ 11U }, .Session = gap.Context.Session, .Generation = gap.Context.Generation },
			.Selection = core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{ } },
		});
		process_events();
		expect(!window.conversation_panel()->anchor_file().has_value() && window.conversation_panel()->cards()->thread_count() == 1000U,
			   "successful empty observation's engaged empty cursor releases visual scope even without a changed Files domain");
		selected.Context.Generation = core::request_generation{ 6U };
		window.publish(selected);
		gap.Context.Generation = core::request_generation{ 7U };
		window.publish(gap);
		observed.Context.Generation = gap.Context.Generation;
		observed.Files              = std::vector{ application::workbench_file_projection{ .File = core::file_identity_of("src/other.cpp"), .Path = "src/other.cpp" } };
		window.publish(observed);
		process_events();
		expect(!window.conversation_panel()->anchor_file().has_value() && window.conversation_panel()->cards()->thread_count() == 1000U,
			   "observation removing the prior file releases its visual anchor on the next event-loop turn");
	}

	/// \brief [[BPR-0818]]. A refresh that leaves no file selected shows the
	/// review summary and the issue comment again once its observation lands.
	///
	/// \details The held scope hid both roots for good: the observation's
	/// terminal kept it, and no later publication selected the file again.
	void test_refresh_without_a_selection_shows_review_wide_roots()
	{
		const auto           file = core::file_identity_of("src/a.cpp");
		qt::workbench_window window;
		auto                 pr = application::github_pull_request_projection{ };
		pr.Conversations        = {
			application::github_conversation_projection{ .Id = "review", .Kind = application::github_conversation_kind::review },
			application::github_conversation_projection{ .Id = "thread", .Kind = application::github_conversation_kind::review_thread, .Path = "src/a.cpp" },
			application::github_conversation_projection{ .Id = "issue-comment", .Kind = application::github_conversation_kind::issue_comment },
		};
		window.publish(application::github_presentation_update{ .PullRequest = pr });
		const auto files   = std::vector{ application::workbench_file_projection{ .File = file, .Path = "src/a.cpp" } };
		const auto session = core::application_session_id{ 1U };
		window.publish(application::workbench_projection{
			.Context   = { .Session = session, .Generation = core::request_generation{ 1U } },
			.Selection = core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{ .File = file, .Ordinal = 1U, .ActionableCount = 1U } },
			.Files     = files,
		});
		const auto shows = [&window](const std::string& aId) {
			return window.conversation_model()->index_for_id(aId).isValid();
		};
		expect(window.conversation_model()->rowCount() == 1 && shows("thread") && !shows("review") && !shows("issue-comment"),
			   "a selected file scopes the drawer to its own thread");
		window.publish(application::workbench_projection{
			.Context   = { .Session = session, .Generation = core::request_generation{ 2U } },
			.Selection = core::workbench_selection_delta{ },
			.Files     = std::vector<application::workbench_file_projection>{ },
		});
		expect(window.conversation_panel()->anchor_file() == std::optional{ file } && window.conversation_model()->rowCount() == 1,
			   "the refresh gap keeps the file's scope while the observation is outstanding");
		window.publish(application::workbench_projection{
			.Context   = { .Request = core::request_id{ 4U }, .Session = session, .Generation = core::request_generation{ 2U } },
			.Selection = core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{ .ActionableCount = 1U, .PreviousAvailable = true, .NextAvailable = true } },
			.Files     = files,
		});
		process_events();
		expect(!window.conversation_panel()->anchor_file().has_value() && window.conversation_model()->rowCount() == 3
				   && shows("review") && shows("thread") && shows("issue-comment"),
			   "the observation's unselected terminal shows the review summary and the issue comment again");
	}

	void test_failed_replacement_keeps_conversations_on_the_requested_file()
	{
		const auto           fileA = core::file_identity_of("src/a.cpp");
		const auto           fileB = core::file_identity_of("src/b.cpp");
		const auto           diffA = computed_comparison("a-left\n", "a-right\n", 0U, fileA);
		const auto           diffB = computed_comparison("b-left\n", "b-right\n", 1U, fileB);
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		process_events();
		auto* const surface = publish_fixture(window, diffA);
		window.set_conversation_endpoint_placement(false, true);
		window.conversation_panel()->adopt(
			{
				application::github_conversation_projection{
					.Id       = "thread-a",
					.Kind     = application::github_conversation_kind::review_thread,
					.Path     = "src/a.cpp",
					.Line     = 1U,
					.DiffSide = application::github_conversation_diff_side::right,
					.AnchorAvailability
					= application::github_conversation_anchor_availability::current,
				},
				application::github_conversation_projection{
					.Id       = "thread-b",
					.Kind     = application::github_conversation_kind::review_thread,
					.Path     = "src/b.cpp",
					.Line     = 1U,
					.DiffSide = application::github_conversation_diff_side::right,
					.AnchorAvailability
					= application::github_conversation_anchor_availability::current,
				},
			},
			QStringLiteral("two conversations"));
		expect(
			window.conversation_panel()->anchor_file() == std::optional{ fileA }
				&& window.conversation_model()->index_for_id("thread-a").isValid(),
			"the published comparison filters conversations to its exact file");

		const auto contextB = request_context(diffB);
		surface->set_loading(contextB);
		process_events();
		expect(
			window.conversation_panel()->anchor_file() == std::optional{ fileB }
				&& window.conversation_model()->index_for_id("thread-b").isValid()
				&& !window.conversation_model()->index_for_id("thread-a").isValid(),
			"loading a replacement synchronizes the drawer to the new request before completion");
		surface->fail({
			.Context = contextB,
			.Reason  = core::diff_reason_code::host_failure,
		});
		process_events();
		expect(
			window.conversation_panel()->anchor_file() == std::optional{ fileB }
				&& window.conversation_model()->index_for_id("thread-b").isValid()
				&& !surface->current_result(),
			"an exact replacement failure keeps conversations on the requested file instead of restoring stale scope");
	}

	void test_the_centre_is_the_composed_rfc_0004_comparison_surface()
	{
		const auto [leftText, rightText] = comparison_fixture_text();
		const auto diff                  = computed_comparison(leftText, rightText);
		expect(diff.Alignment.Groups.size() >= 3U,
			   "the fixture carries several canonical change groups");
		expect(diff.Alignment.LeftLineCount != diff.Alignment.RightLineCount,
			   "the fixture would need invented rows to make both documents the "
			   "same height");

		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		process_events();

		auto* const surface = window.comparison_surface();
		// [[BPR-0647]]. The surface sits on the comparison page of the
		// centre's stack, beneath the epoch line, inside the central surface.
		expect(surface != nullptr
				   && window.findChild<QWidget*>(
								QStringLiteral("workbenchCentralSurface"))
						  ->isAncestorOf(surface)
				   && surface->parentWidget()
					   == window.findChild<QWidget*>(
						   QStringLiteral("workbenchComparisonPage")),
			   "the composed comparison surface is the workbench centre");
		expect(static_cast<application::diff_projection_port*>(surface)
				   != nullptr,
			   "the composed centre is the workbench's diff projection port");
		expect(surface->comparison_splitter() != nullptr
				   && surface->comparison_splitter()->count() == 2
				   && surface->link_map()
					   == surface->comparison_splitter()->link_map(),
			   "the centre holds exactly the two natural panes divided by the "
			   "single link map");
		expect(surface->scroll_controller() != nullptr
				   && surface->shared_scrollbar() != nullptr,
			   "the centre reuses one retained comparison position and its "
			   "shared scrollbar");

		static_cast<void>(publish_fixture(window, diff));
		const auto published = observe_composed_comparison(
			window,
			diff,
			QString::fromStdString(leftText),
			QString::fromStdString(rightText),
			"published");
		expect(published.Ribbons > 0U
				   && published.Grouped == published.Ribbons,
			   "every painted correspondence carries a canonical group identity");

		// The shell publishes command, drawer, and layout meaning only. A
		// workbench projection that also moved comparison state would be the
		// second authority RFC-BPR-0004 forbids.
		process_events();
		const auto before = surface->statistics();
		window.publish(every_primary_command_offered());
		expect(surface->statistics() == before
				   && surface->current_result() != nullptr,
			   "a workbench-shell projection never re-decides composed "
			   "comparison state");
		window.hide();
	}

	void test_composed_ribbons_follow_the_group_model_through_scroll_and_resize()
	{
		const auto [leftText, rightText] = comparison_fixture_text();
		const auto diff                  = computed_comparison(leftText, rightText);
		const auto left                  = QString::fromStdString(leftText);
		const auto right                 = QString::fromStdString(rightText);

		qt::workbench_window window;
		// Re-measured for the composed window rather than kept. 820 was measured
		// on a shell with no review header; the header takes up to 220, and at
		// the bottom of the fixture a shorter viewport shows a trailing page with
		// no correspondence in it at all. The assertion is unchanged -- the
		// window it is asserted on is the composed one now.
		window.resize(1'280, 820 + 220);
		window.show();
		process_events();
		auto* const surface = publish_fixture(window, diff);
		expect(surface->scroll_controller()->maximum_position() > 0.0,
			   "the composed comparison is taller than its viewport, so the "
			   "shared scrollbar has travel");

		auto totals = observe_composed_comparison(
			window, diff, left, right, "published");

		auto* const shared     = surface->shared_scrollbar();
		const auto  leftBefore = surface->left_pane()->verticalScrollBar()->value();
		const auto  rightBefore
			= surface->right_pane()->verticalScrollBar()->value();
		// [[BPR-1291]]. The bar's maximum leaves only the last line at the
		// top, so step back a page to have the file's last page in view.
		shared->setValue(shared->maximum());
		shared->triggerAction(QAbstractSlider::SliderPageStepSub);
		process_events();
		expect(surface->left_pane()->verticalScrollBar()->value() != leftBefore
				   && surface->right_pane()->verticalScrollBar()->value()
					   != rightBefore,
			   "the one shared vertical scrollbar moves both natural panes");
		const auto scrolled = observe_composed_comparison(
			window, diff, left, right, "scrolled");

		window.resize(window.width() - 200, 700 + 220);
		process_events();
		// A resize changes the panes' viewports, so they measure again.
		settle_geometry(*surface, "resized");
		const auto resized = observe_composed_comparison(
			window, diff, left, right, "resized");

		totals.Ribbons += scrolled.Ribbons + resized.Ribbons;
		totals.Wedges += scrolled.Wedges + resized.Wedges;
		totals.Grouped += scrolled.Grouped + resized.Grouped;
		expect(totals.Grouped == totals.Ribbons,
			   "every ribbon agrees with the canonical group model through "
			   "scroll and resize");
		expect(totals.Wedges > 0U,
			   "an empty-span correspondence renders as a wedge in the composed "
			   "surface");
		window.hide();
	}

	/// \brief Reads one canonical source line's exact text out of the result.
	///
	/// \details The line number always arrives from the canonical RFC-BPR-0004
	/// mapping. Nothing here decides which line a selection maps to; this only
	/// resolves the text the immutable decoded document already holds at it.
	[[nodiscard]] auto canonical_line_text(
		const std::optional<core::decoded_text_document>& aDocument,
		const std::uint32_t                               aLine) -> QString
	{
		expect(aDocument.has_value() && aLine < aDocument->Lines.size(),
			   "the canonical mapping names a line the immutable document holds");
		const auto& span = aDocument->Lines[aLine].Content;
		return QString::fromStdString(std::string{
			aDocument->Bytes.begin() + static_cast<std::ptrdiff_t>(span.Begin),
			aDocument->Bytes.begin() + static_cast<std::ptrdiff_t>(span.End) });
	}

	/// \brief Names the rows one side owes for the canonical selected detail.
	///
	/// \details Empty means the canonical mapping gave this side an insertion or
	/// deletion boundary rather than source lines, which is a real shape rather
	/// than a missing answer.
	[[nodiscard]] auto canonical_side_rows(
		const core::computed_file_diff& aDiff,
		const core::line_detail_input&  aDetail,
		const core::diff_side           aSide) -> QStringList
	{
		const auto& document = aSide == core::diff_side::left
			? aDiff.LeftDocument
			: aDiff.RightDocument;
		const auto& mapped   = aSide == core::diff_side::left
			? aDetail.LeftLine
			: aDetail.RightLine;
		if (mapped.has_value()) {
			return { canonical_line_text(document, mapped->Line) };
		}
		if (
			aDetail.OpposingSpan.has_value()
			&& aDetail.OpposingSpan->Side == aSide) {
			auto rows = QStringList{ };
			for (auto line = aDetail.OpposingSpan->Lines.Begin;
				 line < aDetail.OpposingSpan->Lines.End;
				 ++line) {
				rows.push_back(canonical_line_text(document, line));
			}
			return rows;
		}
		return { };
	}

	/// \brief Finds one contiguous run of rows at or below a starting row.
	[[nodiscard]] auto run_index(
		const QStringList& aRows,
		const QStringList& aRun,
		const qsizetype    aFrom) -> std::optional<qsizetype>
	{
		if (aRun.isEmpty()) {
			return std::nullopt;
		}
		for (auto start = std::max<qsizetype>(aFrom, 0);
			 start + aRun.size() <= aRows.size();
			 ++start) {
			auto matched = true;
			for (auto offset = qsizetype{ 0 }; offset < aRun.size(); ++offset) {
				if (aRows[start + offset] != aRun[offset]) {
					matched = false;
					break;
				}
			}
			if (matched) {
				return start;
			}
		}
		return std::nullopt;
	}

	/// \brief Re-checks that the lower detail pane owns the whole central width.
	///
	/// \details "Central" is the region between the two review drawers, which is
	/// the workbench centre itself. Agreeing with the comparison sub-region would
	/// not satisfy the claim, so the sub-region is measured too and must be
	/// strictly narrower.
	[[nodiscard]] auto observe_full_central_width(
		qt::workbench_window&  aWindow,
		const std::string_view aStage) -> int
	{
		auto* const surface = aWindow.comparison_surface();
		auto* const detail  = surface->detail_pane();
		auto* const centre  = aWindow.findChild<QWidget*>(
			QStringLiteral("workbenchCentralSurface"));
		expect(centre != nullptr && detail != nullptr,
			   message_for("the shell names its centre and its lower detail pane",
						   aStage));
		expect(detail->isVisible(),
			   message_for("the lower detail pane is part of the shown workbench",
						   aStage));
		const auto detailLeft = detail->mapTo(&aWindow, QPoint{ 0, 0 }).x();
		const auto centreLeft = centre->mapTo(&aWindow, QPoint{ 0, 0 }).x();
		expect(detailLeft == centreLeft && detail->width() == centre->width(),
			   message_for("the lower detail pane spans the whole central width",
						   aStage));
		expect(detail->width() > surface->comparison_splitter()->width(),
			   message_for("the lower detail pane is wider than the comparison "
						   "sub-region it sits under",
						   aStage));
		expect(detail->width() > surface->left_pane()->width()
				   && detail->width() > surface->right_pane()->width(),
			   message_for("the lower detail pane is wider than either source "
						   "pane",
						   aStage));
		for (auto* const drawer : { static_cast<QWidget*>(aWindow.files_drawer()),
									static_cast<QWidget*>(
										aWindow.conversations_drawer()) }) {
			if (!drawer->isVisible()) {
				continue;
			}
			const auto drawerLeft = drawer->mapTo(&aWindow, QPoint{ 0, 0 }).x();
			expect(drawerLeft >= detailLeft + detail->width()
					   || drawerLeft + drawer->width() <= detailLeft,
				   message_for("the lower detail pane never overlaps an open "
							   "review drawer",
							   aStage));
		}
		return detail->width();
	}

	void test_the_lower_detail_pane_is_full_width_line_over_line()
	{
		const auto [leftText, rightText] = comparison_fixture_text();
		const auto diff                  = computed_comparison(leftText, rightText);
		expect(diff.Alignment.Groups.size() >= 3U,
			   "the fixture carries several canonical change groups");

		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		process_events();

		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 260U,
		};
		opened.Behaviour = drawer_controls_shown();
		window.publish(opened);
		process_events();

		auto* const surface = publish_fixture(window, diff);
		auto* const detail  = surface->detail_pane();
		expect(detail != nullptr && surface->isAncestorOf(detail),
			   "the lower detail pane is RFC-BPR-0004's composed detail surface "
			   "rather than a second pane the shell invented");

		// [[BPR-0646]]. The detail pane is a review detail, shown on request.
		window.review_details_toggle_action()->setChecked(true);
		process_events();
		const auto openWidth = observe_full_central_width(window, "drawers open");

		auto withoutFiles    = application::workbench_projection{ };
		withoutFiles.Drawers = drawer_states(
			core::workbench_drawer_state::collapsed,
			core::workbench_drawer_state::expanded);
		window.publish(withoutFiles);
		process_events();
		expect(observe_full_central_width(window, "file drawer collapsed")
				   >= openWidth + 320 - qt::workbench_drawer_rail::width_pixels,
			   "collapsing the file drawer gives all but its rail's width to the "
			   "lower detail pane");
		window.publish(opened);
		process_events();
		expect(observe_full_central_width(window, "file drawer reopened")
				   == openWidth,
			   "reopening the file drawer returns its width and leaves the lower "
			   "detail pane spanning the centre");

		auto withoutConversations    = application::workbench_projection{ };
		withoutConversations.Drawers = drawer_states(
			core::workbench_drawer_state::expanded,
			core::workbench_drawer_state::collapsed);
		window.publish(withoutConversations);
		process_events();
		expect(observe_full_central_width(window, "conversation drawer collapsed")
				   >= openWidth + 260 - qt::workbench_drawer_rail::width_pixels,
			   "collapsing the conversation drawer gives all but its rail's width "
			   "to the lower detail pane");
		window.publish(opened);
		process_events();
		expect(observe_full_central_width(window, "conversation drawer reopened")
				   == openWidth,
			   "reopening the conversation drawer returns its width and leaves "
			   "the lower detail pane spanning the centre");

		// Every canonical group is walked through RFC-BPR-0004's own change
		// navigation, and every row the pane shows is checked against the mapping
		// the core resolved for that exact selection.
		auto paired      = std::size_t{ 0U };
		auto bounded     = std::size_t{ 0U };
		auto lastEarlier = QStringList{ };
		auto lastLater   = QStringList{ };
		auto previous    = detail->toPlainText();
		for (std::size_t index = 0U; index < diff.Alignment.Groups.size();
			 ++index) {
			if (index != 0U) {
				expect(surface->navigate_change(
						   core::change_navigation_direction::next,
						   core::change_navigation_policy::stop),
					   "the semantic bridge reaches the next canonical group");
				process_events();
			}
			const auto& selection = surface->selection();
			expect(selection.has_value()
					   && selection->Group == diff.Alignment.Groups[index].Id,
				   "change navigation selects each canonical group in canonical "
				   "order");
			const auto mapping = core::detail_for_selection(diff, *selection);
			expect(mapping.Reason == core::semantic_lookup_reason::none
					   && mapping.Group == selection->Group,
				   "the canonical mapping resolves the selected group");
			const auto rows        = detail->toPlainText().split(QLatin1Char('\n'));
			const auto earlierRows = canonical_side_rows(
				diff, mapping, core::diff_side::left);
			const auto laterRows = canonical_side_rows(
				diff, mapping, core::diff_side::right);
			if (index == 1U) {
				expect(detail->toPlainText() != previous,
					   "selecting another change updates the lower detail pane");
			}
			if (earlierRows.isEmpty() || laterRows.isEmpty()) {
				const auto& present = earlierRows.isEmpty() ? laterRows
															: earlierRows;
				expect(run_index(rows, present, 0).has_value(),
					   "a boundary-shaped group still shows its canonically "
					   "mapped source rows");
				expect(mapping.OpposingBoundary.has_value(),
					   "the side without source rows is a canonical insertion or "
					   "deletion boundary");
				++bounded;
				continue;
			}
			const auto earlierAt = run_index(rows, earlierRows, 0);
			expect(earlierAt.has_value(),
				   "the lower pane shows the canonically mapped earlier rows");
			const auto laterAt = run_index(
				rows, laterRows, *earlierAt + earlierRows.size());
			expect(laterAt.has_value(),
				   "the lower pane shows the canonically mapped later rows below "
				   "the earlier rows, line over line");
			lastEarlier = earlierRows;
			lastLater   = laterRows;
			++paired;
		}
		expect(paired >= 2U && bounded >= 1U,
			   "the walk covered paired-line groups and boundary-shaped groups");
		expect(!lastEarlier.isEmpty() && !lastLater.isEmpty(),
			   "the walk ended with retained canonical rows to clear");

		surface->clear();
		process_events();
		const auto cleared = detail->toPlainText().split(QLatin1Char('\n'));
		expect(!surface->selection().has_value(),
			   "clearing the comparison clears the semantic selection");
		expect(!run_index(cleared, lastEarlier, 0).has_value()
				   && !run_index(cleared, lastLater, 0).has_value(),
			   "a cleared selection leaves no selected rows in the lower detail "
			   "pane");
		window.review_details_toggle_action()->setChecked(true);
		process_events();
		static_cast<void>(observe_full_central_width(window, "selection cleared"));
		window.hide();
	}

	/// \brief Builds one comparison whose later side has a chosen line count.
	///
	/// \details The earlier side keeps six retained anchor lines and the later
	/// side repeats a unique body between them, so every non-equal shape is a
	/// pure insertion and the later document reaches `6 * (aBodyLines + 1)`
	/// lines. Growing both sides together is not available here:
	/// `core::diff_compute_limits` caps the primary alignment matrix at sixteen
	/// million cells, so a comparison whose two sides are both enormous is
	/// refused by the bounded compute budget rather than materialized. A very
	/// large added file is the largest source the workbench can ever be asked
	/// to present through its real public path.
	[[nodiscard]] auto scaled_source_pair(const std::uint32_t aBodyLines)
		-> std::pair<std::string, std::string>
	{
		constexpr auto anchors = std::uint32_t{ 6U };
		auto           left    = std::string{ };
		auto           right   = std::string{ };
		right.reserve(static_cast<std::size_t>(anchors)
					  * (static_cast<std::size_t>(aBodyLines) + 1U) * 24U);
		for (std::uint32_t anchor = 0U; anchor < anchors; ++anchor) {
			for (std::uint32_t body = 0U; body < aBodyLines; ++body) {
				right += "inserted-";
				right += std::to_string(anchor);
				right += '-';
				right += std::to_string(body);
				right += '\n';
			}
			const auto retained = "retained-" + std::to_string(anchor) + '\n';
			left += retained;
			right += retained;
		}
		return { std::move(left), std::move(right) };
	}

	/// \brief Counts every live widget in one subtree, at any depth.
	///
	/// \details `findChildren` is recursive and untyped by role, so this is the
	/// complete retained widget tree beneath the subject: toolbars, tool
	/// compact-row buttons, drawer panels, item views, splitter handles,
	/// the source panes, their viewports, and their scrollbars. A per-source-row
	/// widget layer anywhere beneath the subject would be counted here.
	[[nodiscard]] auto widget_count(const QWidget& aSubject) -> std::size_t
	{
		return static_cast<std::size_t>(aSubject.findChildren<QWidget*>().size());
	}

	/// \brief Records what one sized publication actually retained and cost.
	struct virtualized_observation
	{
		std::uint32_t LeftLines           = 0U;
		std::uint32_t RightLines          = 0U;
		std::size_t   WindowWidgets       = 0U;
		std::size_t   SurfaceWidgets      = 0U;
		std::size_t   PaneWidgets         = 0U;
		std::size_t   VisibleLinks        = 0U;
		std::uint64_t ScrollRebuilds      = 0U;
		int           ScrollTravel        = 0;
		qint64        PublishMilliseconds = 0;
		qint64        ScrollMilliseconds  = 0;
	};

	/// \brief Publishes one sized comparison and measures the composed surface.
	///
	/// \details Only the presentation cost is timed. The immutable comparison is
	/// computed first, so the recorded interval is the accepted publication and
	/// the geometry work the composed workbench then does, not the core diff.
	[[nodiscard]] auto observe_sized_publication(
		qt::workbench_window& aWindow,
		const std::uint32_t   aBodyLines,
		const std::uint8_t    aSeed) -> virtualized_observation
	{
		const auto [leftText, rightText] = scaled_source_pair(aBodyLines);
		const auto diff                  = computed_comparison(leftText, rightText, aSeed);
		expect(diff.Alignment.Content == core::diff_content_state::text,
			   "the sized fixture stays inside the bounded compute budget");

		auto* const   surface = aWindow.comparison_surface();
		QElapsedTimer publishTimer;
		publishTimer.start();
		// [[BPR-1073]]. publish_fixture returns once the native measurement is
		// complete, so the interval includes it. It used to be bounded here at
		// 10 s from the publication; the 60,000-line Debug publication took
		// 12.6 s at 2.0 on the Windows station, so the bound is the helper's.
		static_cast<void>(publish_fixture(aWindow, diff));
		const auto published = publishTimer.elapsed();

		auto* const left  = surface->left_pane();
		auto* const right = surface->right_pane();
		expect(left->toPlainText() == QString::fromStdString(leftText)
				   && right->toPlainText() == QString::fromStdString(rightText),
			   "each composed pane holds exactly its own fixture source");
		expect(document_rows(*left) == diff.Alignment.LeftLineCount
				   && document_rows(*right) == diff.Alignment.RightLineCount,
			   "each composed pane really holds every source line of this size");

		auto* const   shared   = surface->shared_scrollbar();
		const auto    rebuilds = surface->statistics().GeometryRebuilds;
		QElapsedTimer scrollTimer;
		scrollTimer.start();
		shared->setValue(shared->maximum());
		process_events();
		const auto scrolled = scrollTimer.elapsed();

		// Counted after the scroll reaches the end of the document, so a row
		// widget created lazily for newly revealed source would be counted.
		return {
			.LeftLines      = diff.Alignment.LeftLineCount,
			.RightLines     = diff.Alignment.RightLineCount,
			.WindowWidgets  = widget_count(aWindow),
			.SurfaceWidgets = widget_count(*surface),
			.PaneWidgets    = widget_count(*right),
			.VisibleLinks   = surface->link_map()->visible_links().size(),
			.ScrollRebuilds = surface->statistics().GeometryRebuilds
				- rebuilds,
			.ScrollTravel        = shared->maximum() - shared->minimum(),
			.PublishMilliseconds = published,
			.ScrollMilliseconds  = scrolled,
		};
	}

	void test_a_large_file_leaves_the_widget_count_bounded()
	{
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		process_events();

		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 260U,
		};
		window.publish(opened);
		process_events();

		// The whole shell is already assembled and shown, so nothing counted
		// below is a widget the window merely had not built yet.
		const auto assembled = widget_count(window);
		expect(assembled > 0U,
			   "the assembled workbench retains a live widget tree to count");

		const auto small  = observe_sized_publication(window, 9U, 1U);
		const auto medium = observe_sized_publication(window, 999U, 2U);
		const auto large  = observe_sized_publication(window, 9'999U, 3U);

		expect(small.RightLines == 60U && medium.RightLines == 6'000U
				   && large.RightLines == 60'000U,
			   "the harness presents sixty, six thousand, and sixty thousand "
			   "source lines through the same composed workbench");
		expect(medium.RightLines >= 100U * small.RightLines
				   && large.RightLines >= 1'000U * small.RightLines,
			   "the reviewed source grows by orders of magnitude, not by a "
			   "constant");
		expect(large.ScrollTravel > 0,
			   "the sixty-thousand-line comparison really is longer than its "
			   "viewport");

		// Factual scale context for this machine and run. Elapsed time is
		// recorded, never asserted: one machine's timing is not a frame-rate
		// guarantee.
		std::cout << "workbench-window-virtualization:"
				  << " platform="
				  << QSysInfo::prettyProductName().toStdString()
				  << " architecture="
				  << QSysInfo::currentCpuArchitecture().toStdString()
				  << " qt=" << qVersion()
				  << " scale=" << window.devicePixelRatioF()
				  << " counted=window/surface/right-pane"
				  << " lines=" << small.RightLines << '/' << medium.RightLines
				  << '/' << large.RightLines
				  << " window-widgets=" << small.WindowWidgets << '/'
				  << medium.WindowWidgets << '/' << large.WindowWidgets
				  << " surface-widgets=" << small.SurfaceWidgets << '/'
				  << medium.SurfaceWidgets << '/' << large.SurfaceWidgets
				  << " pane-widgets=" << small.PaneWidgets << '/'
				  << medium.PaneWidgets << '/' << large.PaneWidgets
				  << " visible-links=" << small.VisibleLinks << '/'
				  << medium.VisibleLinks << '/' << large.VisibleLinks
				  << " publish-ms=" << small.PublishMilliseconds << '/'
				  << medium.PublishMilliseconds << '/'
				  << large.PublishMilliseconds
				  << " scroll-ms=" << small.ScrollMilliseconds << '/'
				  << medium.ScrollMilliseconds << '/'
				  << large.ScrollMilliseconds << '\n';

		// The ratio is the claim. A harness that merely finished rendering a
		// large file would prove nothing about how the surface is built.
		expect(small.WindowWidgets == assembled
				   && medium.WindowWidgets == assembled
				   && large.WindowWidgets == assembled,
			   "a file a thousand times longer creates no additional widget "
			   "anywhere in the workbench window");
		expect(small.SurfaceWidgets == medium.SurfaceWidgets
				   && medium.SurfaceWidgets == large.SurfaceWidgets,
			   "the composed comparison surface retains one widget tree at every "
			   "source length");
		expect(small.PaneWidgets == medium.PaneWidgets
				   && medium.PaneWidgets == large.PaneWidgets,
			   "the source pane presenting sixty thousand lines has exactly the "
			   "child widgets it has presenting sixty");
		// The entire window remains identical above. The size ratio concerns
		// the source-presenting surface: adding fixed shell controls must not
		// masquerade as a per-source-line allocation regression.
		expect(100U * large.SurfaceWidgets < large.RightLines,
			   "the retained diff surface stays two orders of magnitude smaller "
			   "than the source it presents");
		expect(small.VisibleLinks == medium.VisibleLinks
				   && medium.VisibleLinks == large.VisibleLinks,
			   "the painted correspondences follow the canonical change count "
			   "rather than the source length");
		expect(large.ScrollRebuilds == 0U,
			   "scrolling a sixty-thousand-line comparison rebuilds no measured "
			   "geometry");

		window.hide();
	}

	// --- Composed review-history fixtures -----------------------------------
	//
	// RFC-BPR-0003 decides review-history identity, legality, and wording, and
	// its own suite proves those rules. These fixtures build the same
	// projections so this suite can prove the composition instead: that the
	// shell exposes the control, drives it through the window's projection
	// port, states its decided text, and places it in one declared focus order.

	/// \brief Builds one complete forty-character fixture commit OID text.
	///
	/// \details The characters vary along the whole string rather than
	/// repeating, so an abbreviation, a bare prefix, or an elided middle is a
	/// different value instead of a coincidental match.
	[[nodiscard]] auto epoch_commit_text(const std::uint8_t aSeed) -> std::string
	{
		constexpr std::string_view digits = "0123456789abcdef";
		auto                       text   = std::string{ };
		text.reserve(40U);
		for (std::size_t index = 0U; index < 40U; ++index) {
			text.push_back(digits[((index * 7U) + aSeed) % digits.size()]);
		}
		return text;
	}

	[[nodiscard]] auto epoch_commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		const auto parsed = core::parse_git_commit_oid(epoch_commit_text(aSeed));
		expect(parsed.has_value() && parsed->valid(),
			   "the fixture commit OID parses into a live identity");
		return *parsed;
	}

	[[nodiscard]] constexpr auto epoch_base_seed(const std::int64_t aOrdinal)
		-> std::uint8_t
	{
		return static_cast<std::uint8_t>(2 * aOrdinal);
	}

	[[nodiscard]] constexpr auto epoch_head_seed(const std::int64_t aOrdinal)
		-> std::uint8_t
	{
		return static_cast<std::uint8_t>((2 * aOrdinal) + 1);
	}

	[[nodiscard]] auto epoch_scope() -> core::pull_request_authority_scope_id
	{
		return opaque<core::pull_request_authority_scope_id>(61U);
	}

	[[nodiscard]] auto epoch_catalog() -> core::epoch_catalog_identity
	{
		return {
			.Incarnation = opaque<core::epoch_catalog_incarnation_id>(62U),
			.Revision    = core::epoch_catalog_revision{ 7 },
		};
	}

	[[nodiscard]] auto epoch_authority() -> core::pull_request_authority
	{
		return {
			.Profile         = core::host_profile_id{ 12U },
			.ProfileRevision = core::github_profile_revision{ 22U },
			.Host            = core::git_host_id{ 32U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(63U),
			.Account         = core::github_account_id{ 42U },
			.Viewer          = core::github_viewer_actor_id{ 52U },
			.Repository      = core::repository_id{ 62U },
			.Cache           = opaque<core::repository_cache_scope_id>(64U),
			.PullRequest     = core::pull_request_id{ 72U },
			.Number          = core::pull_request_number{ 82U },
			.Scope           = epoch_scope(),
		};
	}

	[[nodiscard]] auto epoch_row(const std::int64_t aOrdinal)
		-> application::epoch_row_projection
	{
		return {
			.Id = {
				.Authority   = epoch_scope(),
				.Incarnation = epoch_catalog().Incarnation,
				.Ordinal     = core::epoch_ordinal{ aOrdinal },
			},
			.Snapshot              = core::provider_snapshot_id{ static_cast<std::uint64_t>(aOrdinal) },
			.IdentityGeneration    = core::github_identity_generation{ 3U },
			.PullRequestGeneration = core::request_generation{ 4U },
			.BaseBranch            = opaque<core::base_branch_id>(65U),
			.Base                  = epoch_commit(epoch_base_seed(aOrdinal)),
			.Head                  = epoch_commit(epoch_head_seed(aOrdinal)),
			.Cause                 = {
				.Origin   = core::epoch_origin::live_observation,
				.Base     = core::epoch_base_transition::unchanged,
				.Head     = core::epoch_head_transition::descendant,
				.Material = core::epoch_material_relation::different,
				.Primary  = core::epoch_primary_cause::author_upload,
			},
			.ObservedAt       = core::observed_utc_milliseconds{ 1'700'000'000'000LL + aOrdinal },
			.BaseAvailability = core::anchor_availability::available,
			.HeadAvailability = core::anchor_availability::available,
		};
	}

	[[nodiscard]] auto epoch_anchor(
		const application::epoch_row_projection& aRow,
		const core::comparison_anchor_kind       aKind) -> core::comparison_anchor
	{
		return {
			.Identity = {
				.Authority   = aRow.Id.Authority,
				.Incarnation = aRow.Id.Incarnation,
				.Kind        = aKind,
				.Epoch       = aRow.Id.Ordinal,
			},
			.Commit       = aKind == core::comparison_anchor_kind::epoch_base ? aRow.Base : aRow.Head,
			.Availability = core::anchor_availability::available,
		};
	}

	/// \brief Builds one bounded review-history page publication.
	[[nodiscard]] auto history_page_update(
		const core::epoch_history_page_phase           aPhase,
		std::vector<application::epoch_row_projection> aRows)
		-> application::epoch_presentation_update
	{
		const auto identity = epoch_catalog();
		return {
			.Catalog = application::epoch_catalog_projection{
				.Catalog     = identity,
				.HistoryPage = application::epoch_history_page_projection{
					.Phase      = aPhase,
					.Catalog    = identity,
					.Navigation = core::epoch_history_navigation::newest,
					.Limit      = core::maximum_epoch_history_page_size,
					.Rows       = std::move(aRows),
				},
			},
		};
	}

	[[nodiscard]] auto retained_epochs(
		std::vector<application::epoch_row_projection> aRows)
		-> application::epoch_presentation_update
	{
		return history_page_update(core::epoch_history_page_phase::loaded,
								   std::move(aRows));
	}

	[[nodiscard]] auto enabled_epoch_commands() -> core::epoch_command_delta
	{
		const auto enabled = [](const core::epoch_command_id aId) {
			return core::epoch_command_projection{
				.Id             = aId,
				.Enabled        = true,
				.DisabledReason = core::epoch_reason_code::none,
			};
		};
		return {
			.Commands = {
				enabled(core::epoch_command_id::refresh_epochs),
				enabled(core::epoch_command_id::select_comparison),
				enabled(core::epoch_command_id::prune_selected_epochs),
				enabled(core::epoch_command_id::delete_repository_cache),
				enabled(core::epoch_command_id::cancel_epoch_refresh),
				enabled(core::epoch_command_id::cancel_epoch_maintenance),
			},
		};
	}

	/// \brief Publishes one exactly resolved retained comparison pair.
	[[nodiscard]] auto resolved_pair_update(const core::comparison_pair& aPair)
		-> application::epoch_presentation_update
	{
		const auto selection = core::explicit_selection(aPair.Left.Identity,
														aPair.Right.Identity);
		return {
			.Transition = application::epoch_transition_projection{
				.Changed = core::epoch_changed_domain::selection
					| core::epoch_changed_domain::comparison
					| core::epoch_changed_domain::authority
					| core::epoch_changed_domain::status
					| core::epoch_changed_domain::commands,
				.Authority = application::epoch_authority_projection{
					.Authority  = epoch_authority(),
					.Generation = core::epoch_generation{ 8U },
				},
				.Selection = core::comparison_selection_delta{
					.Selection = selection,
				},
				.Comparison = application::epoch_comparison_projection{
					.Selection        = selection,
					.Pair             = aPair,
					.Generation       = core::comparison_generation{ 9U },
					.Revision         = core::comparison_revision{ 3 },
					.AvailableForDiff = true,
				},
				.Status = core::epoch_status_delta{
					.Status = core::epoch_status::ready,
				},
				.Commands = enabled_epoch_commands(),
			},
		};
	}

	[[nodiscard]] auto anchor_intent(const application::epoch_intent& aIntent)
		-> const core::select_comparison_anchor_intent*
	{
		return std::get_if<core::select_comparison_anchor_intent>(&aIntent);
	}

	/// \brief [[BPR-1129]]. The review bar keeps every control top level at every
	/// width: it drops the comparison's dates, then connective words and numbers,
	/// and neither returns at a narrower width.
	void test_the_review_bar_yields_dates_then_words()
	{
		qt::workbench_window window;
		window.resize(1'920, 820);
		window.show();
		auto positioned      = every_primary_command_offered();
		positioned.Selection = core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{
																	.Ordinal           = 3U,
																	.ActionableCount   = 40U,
																	.PreviousAvailable = true,
																	.NextAvailable     = true,
																} };
		window.publish(positioned);
		const auto first = epoch_row(1);
		const auto third = epoch_row(3);
		window.publish(retained_epochs({ first, epoch_row(2), third }));
		window.publish(resolved_pair_update(core::comparison_pair{
			.Left  = epoch_anchor(first, core::comparison_anchor_kind::epoch_base),
			.Right = epoch_anchor(third, core::comparison_anchor_kind::epoch_head),
		}));
		process_events();
		auto* const comparing = window.epoch_selectors()->comparing_button();
		const auto  dated     = [comparing] { return comparing->text().contains(QStringLiteral(" → ")); };
		const auto  worded    = [&window] { return window.file_position_label()->text() == QStringLiteral("File 3 of 40"); };
		expect(dated() && worded(), "a wide review bar names the comparison's dates and the positions' words");
		// The bar fits at the floor in the default font. A larger one, as a
		// reviewer's system font can be, makes it yield on the way down.
		auto larger = window.review_bar()->font();
		larger.setPointSizeF(larger.pointSizeF() * 2.0);
		window.review_bar()->setFont(larger);
		process_events();
		auto datesGone = false;
		auto wordsGone = false;
		for (auto width = 1'920; width >= window.minimumWidth(); width -= width > 1'100 ? 20 : 1) {
			window.resize(width, 820);
			process_events();
			auto* const extension = window.review_bar()->findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button"));
			expect(extension == nullptr || !extension->isVisible(), "no review-bar control folds into an overflow menu at any width");
			expect(!(datesGone && dated()) && !(wordsGone && worded()), "what yielded does not return at a narrower width");
			expect(worded() || !dated(), "the dates yield before the positions' words");
			expect(comparing->text().startsWith(QStringLiteral("Comparing")) && window.file_position_label()->text().startsWith(QStringLiteral("File"))
					   && window.file_position_label()->toolTip() == QStringLiteral("File 3 of 40"),
				   "the comparison and navigation groups keep their names and full position tooltips");
			datesGone = datesGone || !dated();
			wordsGone = wordsGone || !worded();
		}
		// Dates yield before positions compact; the group names never yield.
		expect(datesGone, "in a large font the dates yield on the way down to the floor");
		window.hide();
	}

	/// \brief [[BPR-1255]]. The platform's width floor, which the window keeps at
	/// the default font.
	[[nodiscard]] constexpr auto platform_floor_width() noexcept -> int
	{
#if defined(_WIN32)
		return 902;
#else
		return 900;
#endif
	}

	/// \brief A review bar with every control, dates and positions shown.
	void publish_positioned_review_bar(qt::workbench_window& aWindow)
	{
		auto positioned      = every_primary_command_offered();
		positioned.Selection = core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{
																	.Ordinal           = 3U,
																	.ActionableCount   = 40U,
																	.PreviousAvailable = true,
																	.NextAvailable     = true,
																} };
		aWindow.publish(positioned);
		const auto first = epoch_row(1);
		const auto third = epoch_row(3);
		aWindow.publish(retained_epochs({ first, epoch_row(2), third }));
		aWindow.publish(resolved_pair_update(core::comparison_pair{
			.Left  = epoch_anchor(first, core::comparison_anchor_kind::epoch_base),
			.Right = epoch_anchor(third, core::comparison_anchor_kind::epoch_head),
		}));
	}

	[[nodiscard]] auto review_bar_folded(qt::workbench_window& aWindow) -> bool
	{
		auto* const extension = aWindow.review_bar()->findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button"));
		return extension != nullptr && extension->isVisible();
	}

	/// \brief [[BPR-1255]]. The floor follows the bar, the owner's choice of
	/// 2026-10-06: in a font too large for the platform's floor the window's
	/// minimum rises to the fully yielded review bar, and it returns to the
	/// floor at the default font.
	///
	/// \details Three times the bar's font is wider than every platform's floor
	/// once everything has yielded, so the rise is observed on each platform;
	/// the doubled font of the yield course fits the floor on macOS.
	void test_the_floor_follows_the_review_bar()
	{
		qt::workbench_window window;
		window.resize(1'920, 820);
		window.show();
		publish_positioned_review_bar(window);
		process_events();
		expect(window.minimumWidth() == platform_floor_width(), "at the default font the window keeps the platform's floor");
		const auto normal = window.review_bar()->font();
		auto       larger = normal;
		larger.setPointSizeF(normal.pointSizeF() * 3.0);
		window.review_bar()->setFont(larger);
		process_events();
		const auto raised = window.minimumWidth();
		expect(raised > platform_floor_width(), "a font too large for the floor raises the window's minimum");
		window.resize(platform_floor_width(), 820);
		process_events();
		expect(window.width() == raised && !review_bar_folded(window),
			   "the window cannot be made narrower than the raised floor, where nothing folds");
		expect(window.review_bar()->sizeHint().width() <= raised, "the raised floor holds the bar with everything yielded");
		window.review_bar()->setFont(normal);
		process_events();
		expect(window.minimumWidth() == platform_floor_width(), "the floor returns at the default font");
		auto windowFont = window.font();
		windowFont.setPointSizeF(windowFont.pointSizeF() * 3.0);
		window.setFont(windowFont);
		process_events();
		expect(window.minimumWidth() > platform_floor_width() && !review_bar_folded(window),
			   "a larger window font raises the floor the same way");
		window.hide();
	}

	/// \brief [[BPR-1255]]. A geometry saved at the platform's floor is restored
	/// under a doubled font at the floor the bar needs, with nothing folded.
	void test_restored_geometry_accepts_the_raised_floor()
	{
		qt::workbench_window window;
		publish_positioned_review_bar(window);
		auto larger = window.review_bar()->font();
		larger.setPointSizeF(larger.pointSizeF() * 2.0);
		window.review_bar()->setFont(larger);
		window.show();
		process_events();
		const auto floor = window.minimumWidth();
		expect(floor >= platform_floor_width(), "the doubled font keeps at least the platform's floor");
		auto* const screen = QGuiApplication::primaryScreen();
		const auto  origin = screen != nullptr ? screen->availableGeometry().topLeft() + QPoint{ 40, 80 } : QPoint{ 40, 80 };
		window.publish(application::workbench_projection{
			.Geometry = application::workbench_window_geometry{ origin.x(), origin.y(), platform_floor_width(), 700, false } });
		process_events();
		// The height is the screen's to clamp; the width is the floor's.
		expect(window.width() == floor && !review_bar_folded(window),
			   "a geometry saved at the platform's floor is restored at the floor the bar needs");
		window.hide();
	}

	/// \brief [[BPR-1129]]. The title row and the review bar as a reviewer sees
	/// them, at the design's 1280 by 852 and at the platform's width floor, saved
	/// beside the other window renders at each registered scale.
	void test_the_title_row_and_review_bar_render()
	{
		qt::workbench_window window;
		window.resize(1'280, 852);
		window.show();
		window.publish(application::github_presentation_update{
			.PullRequest = application::github_pull_request_projection{
				.Owner          = "EpicGames",
				.RepositoryName = "UnrealEngine",
				.Number         = core::pull_request_number{ 14366U },
				.Title          = "Hair strand lighting fixes for LSS",
			},
		});
		auto positioned      = every_primary_command_offered();
		positioned.Selection = core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{
																	.Ordinal           = 3U,
																	.ActionableCount   = 40U,
																	.PreviousAvailable = true,
																	.NextAvailable     = true,
																} };
		window.publish(positioned);
		const auto first = epoch_row(1);
		const auto third = epoch_row(3);
		window.publish(retained_epochs({ first, epoch_row(2), third }));
		window.publish(resolved_pair_update(core::comparison_pair{
			.Left  = epoch_anchor(first, core::comparison_anchor_kind::epoch_base),
			.Right = epoch_anchor(third, core::comparison_anchor_kind::epoch_head),
		}));
		const auto head = core::parse_git_commit_oid("abcdef0123456789abcdef0123456789abcdef01").value();
		auto       kept = core::review_submission_draft{
			.Id            = core::review_submission_draft_id{ 1U },
			.Target        = { .Host = core::git_host_id{ 1U }, .Repository = core::repository_id{ 2U }, .PullRequest = core::pull_request_id{ 3U }, .ExpectedHead = head },
			.ActingAccount = core::github_account_id{ 4U },
			.Body          = "Kept summary",
		};
		window.review_submission_editor_view()->present(qt::review_submission_editor_projection{ .Draft = kept, .Controls = core::controls(kept), .TargetDescription = QStringLiteral("target"), .Status = QStringLiteral("status") });
		process_events();
		expect(window.identity_label()->text().contains(QStringLiteral("Hair strand lighting fixes for LSS"))
				   && window.epoch_selectors()->comparing_button()->text().contains(QStringLiteral(" → "))
				   && window.file_position_label()->text() == QStringLiteral("File 3 of 40"),
			   "at 1280 the title row names the pull request whole and the review bar names the comparison's dates and the position");
		const auto save = [&window](const QString& aSuffix) {
			const auto rendered = window.grab();
			expect(!rendered.isNull(), "the chrome renders in the composed window");
			if (const auto path = qEnvironmentVariable("BPR_QT_RENDER_PATH"); !path.isEmpty()) {
				const QFileInfo file{ path };
				expect(rendered.save(QStringLiteral("%1/%2-%3.png").arg(file.path(), file.completeBaseName(), aSuffix), "PNG"),
					   "the chrome render is saved for inspection");
			}
		};
		save(QStringLiteral("review-bar-1280x852"));
		window.resize(window.minimumWidth(), 852);
		process_events();
		expect(window.identity_label()->isVisible() && window.identity_label()->text().contains(QStringLiteral("#14366")),
			   "at the width floor the title row keeps the name and its number");
		save(QStringLiteral("review-bar-floor"));
		window.hide();
	}

	/// \brief [[BPR-1130]]. RFC-BPR-0052 AC-01 and AC-04: the Updates line
	/// folds from the review bar's disclosure and by keyboard, a new window
	/// starts folded, the bar never moves, and the line carries no caption:
	/// its statement is its tooltip, and incomplete history is a mark on the
	/// line and on the disclosure. Saved at 1280 by 852, open and folded, and
	/// at the width floor, at each registered scale.
	void test_the_updates_line_folds_from_the_review_bar()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			[&log](core::navigate_diff_change_intent aIntent) {
				log.Change.push_back(aIntent);
			}
		};
		window.resize(1'280, 852);
		window.show();
		window.publish(application::github_presentation_update{
			.PullRequest = application::github_pull_request_projection{
				.Owner          = "EpicGames",
				.RepositoryName = "UnrealEngine",
				.Number         = core::pull_request_number{ 14366U },
				.Title          = "Hair strand lighting fixes for LSS",
			},
		});
		auto positioned      = every_primary_command_offered();
		positioned.Selection = core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{
																	.Ordinal           = 3U,
																	.ActionableCount   = 40U,
																	.PreviousAvailable = true,
																	.NextAvailable     = true,
																} };
		window.publish(positioned);
		auto* const disclosure = window.updates_disclosure_button();
		auto* const epochs     = window.epoch_selectors();
		auto* const line       = epochs->line();
		expect(disclosure != nullptr && !disclosure->history_marked() && !line->history_marked(),
			   "with no updates known there is no history to mark");
		const auto first = epoch_row(1);
		const auto third = epoch_row(3);
		window.publish(retained_epochs({ first, epoch_row(2), third }));
		window.publish(resolved_pair_update(core::comparison_pair{
			.Left  = epoch_anchor(first, core::comparison_anchor_kind::epoch_base),
			.Right = epoch_anchor(third, core::comparison_anchor_kind::epoch_head),
		}));
		process_events();

		auto* const bar      = window.review_bar();
		const auto  inWindow = [&window](const QWidget* aWidget) { return QRect{ aWidget->mapTo(&window, QPoint{ 0, 0 }), aWidget->size() }; };
		const auto  barRect  = inWindow(bar);
		const auto  panesTop = [&window] { return window.central_splitter()->mapTo(&window, QPoint{ 0, 0 }).y(); };
		const auto  toggles  = [&log] {
			return std::ranges::count_if(log.Workbench, [](const core::workbench_interaction_intent& aIntent) {
				return std::holds_alternative<core::toggle_workbench_updates_line_intent>(aIntent);
			});
		};
		const auto folded = [&] {
			return !window.updates_line_open() && !line->isVisible() && !epochs->isVisible()
				&& disclosure->arrowType() == Qt::RightArrow && !disclosure->isChecked()
				&& !window.updates_line_action()->isChecked() && panesTop() == barRect.bottom() + 1;
		};
		expect(folded(), "a new window starts with the Updates line folded, and the panes sit under the review bar");
		expect(epochs->comparing_button()->isVisible() && epochs->comparing_button()->text().contains(QStringLiteral(" → ")),
			   "folded, the review bar still names the comparison and its dates");

		// The gesture leaves; only the core's decision changes what is shown.
		disclosure->click();
		process_events();
		expect(toggles() == 1 && folded(), "a click on the disclosure sends one fold gesture and shows nothing the core did not decide");
		disclosure->setFocus(Qt::TabFocusReason);
		process_events();
		expect(disclosure->hasFocus() && disclosure->focusPolicy() == Qt::StrongFocus, "the disclosure takes keyboard focus");
		QKeyEvent press{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
		QKeyEvent release{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
		QApplication::sendEvent(disclosure, &press);
		QApplication::sendEvent(disclosure, &release);
		process_events();
		expect(toggles() == 2 && folded(), "Space on the focused disclosure sends the same gesture");
		auto* const viewMenu = window.findChild<QMenu*>(QStringLiteral("workbenchViewMenu"));
		expect(viewMenu != nullptr && viewMenu->actions().contains(window.updates_line_action())
				   && window.updates_line_action()->shortcut().isEmpty(),
			   "the View menu offers the fold by keyboard, and the fold takes no key of its own");
		window.updates_line_action()->trigger();
		process_events();
		expect(toggles() == 3 && folded(), "the menu's command sends the same gesture");

		open_updates_line(window);
		const auto lineRect = inWindow(epochs);
		expect(window.updates_line_open() && line->isVisible() && disclosure->arrowType() == Qt::DownArrow
				   && disclosure->isChecked() && window.updates_line_action()->isChecked(),
			   "open, the timeline shows and the disclosure and the command say so");
		expect(inWindow(bar) == barRect && lineRect.top() == barRect.bottom() + 1 && lineRect.left() == barRect.left()
				   && lineRect.width() == barRect.width() && lineRect.height() == qt::epoch_line::height_pixels
				   && panesTop() == lineRect.bottom() + 1,
			   "open, the timeline is one 34-pixel row across the window just below the bar, and the bar has not moved");
		expect(inWindow(line).left() == lineRect.left() + 12 && !epochs->line_summary()->isVisible()
				   && !window.findChild<QLabel*>(QStringLiteral("epochLineName")),
			   "the timeline runs alone on its row: no name and no summary words beside it");
		save_render(window, QStringLiteral("updates-open-1280x852"));

		// No caption: the statement is the tooltip and the accessible description,
		// and incomplete history is a mark on the line and on the disclosure.
		expect(line->statement() == line->history_text() && line->history_text().startsWith(QStringLiteral("Incomplete history"))
				   && line->accessibleDescription() == line->statement(),
			   "the history statement is the line's accessible description");
		const auto empty = QPoint{ line->width() / 2 + 5, qt::epoch_line::height_pixels - 2 };
		QHelpEvent help{ QEvent::ToolTip, empty, line->mapToGlobal(empty) };
		QApplication::sendEvent(line, &help);
		expect(QToolTip::text() == line->statement(), "away from a node the line's tooltip is its history statement");
		QToolTip::hideText();
		expect(line->history_marked() && disclosure->history_marked()
				   && disclosure->accessibleName() == QStringLiteral("Updates, history incomplete")
				   && disclosure->toolTip().contains(line->statement())
				   && epochs->comparing_button()->toolTip().contains(line->statement()),
			   "incomplete history marks the line and the disclosure, and both tooltips carry the statement");
		expect(line->node_centre(0U).x() > 12 + qt::epoch_line::history_mark_size,
			   "the mark sits before the track, which starts after it");

		// Nothing folds or opens on its own.
		for (const auto modality : { qt::review_modality::overview, qt::review_modality::read,
									 qt::review_modality::discuss, qt::review_modality::review }) {
			window.choose_modality(modality);
			process_events();
			expect(window.updates_line_open() && inWindow(bar) == barRect,
				   "a modality neither folds the line nor moves the bar: " + std::to_string(static_cast<int>(modality)));
			expect(line->isVisible() && line->history_marked() && disclosure->isVisible() && disclosure->history_marked(),
				   "the line and the disclosure carry the mark in modality " + std::to_string(static_cast<int>(modality)));
		}
		process_events();

		fold_updates_line(window);
		expect(folded() && inWindow(bar) == barRect, "folded, the line leaves no row and the bar has not moved");
		save_render(window, QStringLiteral("updates-folded-1280x852"));

		// Review details live in the line's control: shown while folded, the
		// control holds them and not the timeline.
		window.review_details_toggle_action()->trigger();
		process_events();
		expect(epochs->isVisible() && epochs->details_visible() && !line->isVisible() && inWindow(epochs).top() == barRect.bottom() + 1,
			   "Review details show under the bar while the line stays folded");
		window.review_details_toggle_action()->trigger();
		process_events();
		expect(folded(), "hiding the details folds the control away again");

		window.resize(window.minimumWidth(), 852);
		process_events();
		expect(disclosure->isVisible() && disclosure->history_marked(), "at the width floor the disclosure keeps its name and its mark");
		save_render(window, QStringLiteral("updates-folded-floor"));
		open_updates_line(window);
		save_render(window, QStringLiteral("updates-open-floor"));
		window.hide();
	}

	/// \brief [[BPR-1133]]. RFC-BPR-0052 AC-08: Comment, the review bar's icon
	/// and Option-C, starts a comment on the current line or a reply to the
	/// selected thread, and its menu names the target; its arrow offers the
	/// plain comment alone until RFC-BPR-0048's labels. Every thread card
	/// beside the code offers Reply, which answers its own thread through the
	/// Discuss path, and nothing publishes without the explicit Publish.
	void test_comment_names_its_target_and_every_card_replies()
	{
		qt::workbench_window window;
		window.resize(1'280, 852);
		window.show();
		process_events();
		auto opened    = every_primary_command_offered(core::change_navigation_policy::wrap);
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		window.publish(opened);
		process_events();
		auto text = std::string{ };
		for (auto line = 0U; line < 30U; ++line) {
			text += "line " + std::to_string(line) + '\n';
		}
		auto laterText = text;
		laterText.replace(laterText.find("line 12"), std::string{ "line 12" }.size(), "changed line 12");
		const auto diff    = computed_comparison(text, laterText, 0U, core::file_identity_of("src/file.cpp"));
		auto       replies = std::vector<std::string>{ };
		window.set_review_reply_dispatch([&replies](const std::string& aIdentity) { replies.push_back(aIdentity); });
		auto drafts = std::vector<core::review_draft_command>{ };
		window.set_review_draft_dispatch([&drafts](core::review_draft_command aCommand) { drafts.push_back(std::move(aCommand)); });
		window.conversation_panel()->adopt(
			{ application::github_conversation_projection{
				  .Id                 = "review-thread:t22",
				  .Kind               = application::github_conversation_kind::review_thread,
				  .AuthorLogin        = "tbutler",
				  .Path               = "src/file.cpp",
				  .Line               = 22U,
				  .DiffSide           = application::github_conversation_diff_side::right,
				  .AnchorAvailability = application::github_conversation_anchor_availability::current },
			  application::github_conversation_projection{
				  .Id          = "review-comment:t22-1",
				  .ParentId    = "review-thread:t22",
				  .Kind        = application::github_conversation_kind::review_comment,
				  .AuthorLogin = "tbutler",
				  .Body        = "Where does 0.01 come from?",
				  .Path        = "src/file.cpp" },
			  application::github_conversation_projection{
				  .Id                 = "review-thread:t5",
				  .Kind               = application::github_conversation_kind::review_thread,
				  .AuthorLogin        = "jiayincaonv",
				  .Path               = "src/file.cpp",
				  .Line               = 5U,
				  .DiffSide           = application::github_conversation_diff_side::right,
				  .AnchorAvailability = application::github_conversation_anchor_availability::current },
			  application::github_conversation_projection{
				  .Id          = "review-comment:t5-1",
				  .ParentId    = "review-thread:t5",
				  .Kind        = application::github_conversation_kind::review_comment,
				  .AuthorLogin = "jiayincaonv",
				  .Body        = "Guarding on view data validity avoids the null deref.",
				  .Path        = "src/file.cpp" } },
			QStringLiteral("two conversations"));
		// Before any comparison there is no line, and no thread chosen.
		emit window.comment_menu()->aboutToShow();
		expect(!window.comment_action()->isEnabled()
				   && window.comment_action()->text().contains(QStringLiteral("Select a current-head"))
				   && window.comment_action()->toolTip().contains(QStringLiteral("Select a current-head")),
			   "without a target Comment is unavailable with its reason in the menu and tooltip");
		static_cast<void>(publish_fixture(window, diff));
		window.set_conversation_endpoint_placement(false, true);
		process_events();

		auto* const comment = window.comment_action();
		auto* const menu    = window.comment_menu();
		auto* const button  = qobject_cast<QToolButton*>(window.review_bar()->widgetForAction(comment));
		auto* const stack   = window.findChild<QStackedWidget*>(QStringLiteral("workbenchCentreStack"));
		auto* const page    = window.findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
		expect(button != nullptr && button->popupMode() == QToolButton::MenuButtonPopup && button->menu() == menu
				   && menu != nullptr && menu->actions() == QList<QAction*>{ comment } && stack != nullptr && page != nullptr,
			   "Comment's arrow opens a menu that offers the plain comment alone, and no label");
		const auto usable = [&] {
			return comment->isEnabled() && window.review_editor()->shown_draft()
				&& window.review_editor()->publish_action()->isEnabled();
		};
		expect(!usable(), "a selected line without an admitted draft does not offer Comment");
		comment->setEnabled(true); // Negative control: activation still rechecks admission.
		comment->trigger();
		expect(stack->currentWidget() != page && !usable(), "an incorrectly enabled action cannot open an unusable page");
		const auto admit = [&window](const std::optional<core::semantic_selection>& aSelection) {
			if (aSelection) {
				window.review_editor()->present(core::review_comment_draft{
					.Id = core::review_draft_id{ 1U }, .Anchor = aSelection->PreferredLine, .Body = { } });
			}
		};
		window.comparison_surface()->set_selection_sink(admit);
		expect(window.comparison_surface()->select_conversation_line(core::diff_side::right, 12U), "select the current-head side of the first change");
		admit(window.comparison_surface()->selection());
		expect(usable(), "the host's exact editable draft makes Comment available");
		const auto key       = comment->shortcut().toString(QKeySequence::NativeText);
		const auto showsMenu = [menu] { emit menu->aboutToShow(); };
		const auto back      = [&window] {
			QKeyEvent press{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
			QApplication::sendEvent(window.focusWidget() != nullptr ? window.focusWidget() : &window, &press);
			process_events();
		};
		const auto publishes = [&drafts] {
			return std::ranges::count_if(drafts, [](const core::review_draft_command& aCommand) {
				return aCommand.Kind == core::review_draft_command_kind::begin_publish;
			});
		};
		showsMenu();
		expect(comment->text() == QStringLiteral("Co&mment on line 13…"),
			   "a comparison opens on its first change, whose line Comment names");

		// A line with no thread: Comment comments on it.
		expect(window.comparison_surface()->select_conversation_line(core::diff_side::right, 9U), "line 10 is selected as a click would");
		process_events();
		showsMenu();
		expect(comment->text() == QStringLiteral("Co&mment on line 10…")
				   && comment->toolTip() == QStringLiteral("Comment on line 10 (%1)").arg(key)
				   && window.pull_request_menu()->actions().contains(comment),
			   "the menu, the tooltip and the Pull Request menu name the line Comment would start on");
		comment->trigger();
		process_events();
		expect(stack->currentWidget() == page && replies.empty(), "Comment opens the inline page on the line's draft, answering no thread");
		back();
		expect(stack->currentWidget() != page, "Esc returns from the page");
		// A conflict still offers an explicit recovery, so Back must not strand
		// the kept text without a route back to that usable recovery page.
		auto*      inlineEditor = window.review_editor();
		const auto acceptedLine = *inlineEditor->shown_draft();
		inlineEditor->set_revisioned_dispatch([](auto) { });
		inlineEditor->present(core::revisioned_review_draft{ acceptedLine, 1U, core::review_draft_scope_id{ 1U } });
		inlineEditor->body_editor()->setPlainText(QStringLiteral("retained conflict"));
		inlineEditor->present(core::revisioned_review_draft{ acceptedLine, 2U, core::review_draft_scope_id{ 1U } });

		expect(!inlineEditor->publish_action()->isEnabled() && inlineEditor->reload_draft_action()->isEnabled()
				   && comment->isEnabled(),
			   "a recoverable conflict keeps its Comment entry without offering Publish");
		comment->trigger();
		process_events();
		expect(stack->currentWidget() == page, "Comment can reopen the retained text's actionable recovery");
		inlineEditor->reload_draft_action()->trigger();
		inlineEditor->set_revisioned_dispatch({ });
		back();

		// The line of a thread: Comment answers it.
		expect(window.comparison_surface()->select_conversation_line(core::diff_side::right, 21U), "line 22 is selected as a click would");
		process_events();
		showsMenu();
		expect(comment->text() == QStringLiteral("&Reply to tbutler's thread…")
				   && comment->toolTip() == QStringLiteral("Reply to tbutler's thread (%1)").arg(key),
			   "on a thread's line the menu and the tooltip name the thread Comment answers");
		comment->trigger();
		process_events();
		expect(replies == std::vector<std::string>{ "review-thread:t22" } && stack->currentWidget() == page,
			   "Comment (Option-C) names the selected thread and opens the inline page on the reply draft");
		back();

		// A card beside the code: Reply, never a second thread. The other
		// thread's line is selected first, so the card's Reply has to find its
		// own; the cards are rebuilt by that, so they are read after it
		// ([[BPR-1029]]).
		window.choose_modality(qt::review_modality::review);
		expect(window.comparison_surface()->select_conversation_line(core::diff_side::right, 4U), "line 5 is selected");
		process_events();
		showsMenu();
		expect(comment->text() == QStringLiteral("&Reply to jiayincaonv's thread…"), "Comment now names the thread on line 5");
		const auto& cards = window.conversation_panel()->cards()->cards();
		const auto  card  = std::ranges::find(cards, std::string{ "review-thread:t22" }, &qt::thread_card::Id);
		expect(window.conversation_panel()->cards()->isVisible() && !window.conversation_panel()->cards()->feed() && card != cards.end(),
			   "both threads' cards sit beside the code");
		save_render(window, QStringLiteral("comment-reply-cards"));
		menu->popup(button->mapToGlobal(QPoint{ 0, button->height() }));
		process_events();
		const auto menuRender = menu->grab();
		expect(!menuRender.isNull() && menu->isVisible(), "Comment's menu opens from its arrow");
		if (const auto path = qEnvironmentVariable("BPR_QT_RENDER_PATH"); !path.isEmpty()) {
			const QFileInfo file{ path };
			expect(menuRender.save(QStringLiteral("%1/%2-comment-menu.png").arg(file.path(), file.completeBaseName()), "PNG"),
				   "the menu render is saved for inspection");
		}
		menu->hide();
		process_events();
		const auto reply = std::ranges::find(card->Actions, qt::card_action::reply, &std::pair<qt::card_action, QRect>::first);
		expect(reply != card->Actions.end()
				   && std::ranges::all_of(card->Actions, [](const auto& aAction) {
						  return aAction.first == qt::card_action::reply || aAction.first == qt::card_action::mark_read
							  || aAction.first == qt::card_action::resolve || aAction.first == qt::card_action::unresolve;
					  }),
			   "the card beside the code offers Reply and nothing that starts a second thread");
		const auto replyPoint = reply->second.center()
			- QPoint{ 0, window.conversation_panel()->cards()->verticalScrollBar()->value() };
		auto press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ replyPoint }, QPointF{ replyPoint }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(window.conversation_panel()->cards()->viewport(), &press);
		process_events();
		expect(replies == std::vector<std::string>{ "review-thread:t22", "review-thread:t22" } && stack->currentWidget() == page
				   && window.comparison_surface()->selection().has_value()
				   && window.comparison_surface()->selection()->PreferredLine.Line == 21U,
			   "the card's Reply selects its own thread's line and answers that thread on the inline page");
		back();
		window.set_conversation_endpoint_placement(true, true);
		expect(window.comparison_surface()->select_conversation_line(core::diff_side::left, 12U), "the changed left-side line is selectable");
		process_events();
		expect(!comment->isEnabled() && comment->toolTip().contains(QStringLiteral("right, current-head")), "a left-side new comment is unavailable with its exact reason");
		window.conversation_panel()->adopt({ application::github_conversation_projection{
											   .Id = "review-thread:left", .Kind = application::github_conversation_kind::review_thread, .AuthorLogin = "left-reviewer", .Path = "src/file.cpp", .Line = 13U, .DiffSide = application::github_conversation_diff_side::left, .AnchorAvailability = application::github_conversation_anchor_availability::current } },
										   QStringLiteral("left thread"));
		window.set_review_reply_dispatch([&window, &replies](const std::string& aIdentity) {
			replies.push_back(aIdentity);
			auto replyDraft          = *window.review_editor()->shown_draft();
			replyDraft.ReplyToThread = aIdentity;
			window.review_editor()->present(replyDraft);
		});
		process_events();
		showsMenu();
		expect(comment->isEnabled() && comment->text().contains(QStringLiteral("left-reviewer's thread")), "the left line's existing thread remains an admitted Reply target");
		comment->trigger();
		process_events();
		expect(replies.back() == "review-thread:left" && stack->currentWidget() == page
				   && window.review_editor()->publish_action()->isEnabled(),
			   "left Reply opens the converted reply draft with explicit Publish available");
		back();
		expect(publishes() == 0, "nothing was published: Comment and Reply only open the page, and Publish stays the page's");
		window.hide();
	}

	/// \brief [[BPR-1214]]. Comment on a line no thread is on, Discard, Back:
	/// the Comments drawer still shows the file's conversations.
	void test_discard_and_back_keep_the_comments_drawer()
	{
		qt::workbench_window window;
		window.resize(1'280, 852);
		window.show();
		process_events();
		auto opened    = every_primary_command_offered(core::change_navigation_policy::wrap);
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		window.publish(opened);
		window.choose_modality(qt::review_modality::review);
		process_events();
		auto text = std::string{ };
		for (auto line = 0U; line < 30U; ++line) {
			text += "line " + std::to_string(line) + '\n';
		}
		auto laterText = text;
		laterText.replace(laterText.find("line 12"), std::string{ "line 12" }.size(), "changed line 12");
		const auto diff   = computed_comparison(text, laterText, 0U, core::file_identity_of("src/file.cpp"));
		auto       drafts = std::vector<core::review_draft_command>{ };
		window.set_review_draft_dispatch([&drafts](core::review_draft_command aCommand) { drafts.push_back(std::move(aCommand)); });
		window.conversation_panel()->adopt(
			{ application::github_conversation_projection{
				  .Id                 = "review-thread:t22",
				  .Kind               = application::github_conversation_kind::review_thread,
				  .AuthorLogin        = "tbutler",
				  .Path               = "src/file.cpp",
				  .Line               = 22U,
				  .DiffSide           = application::github_conversation_diff_side::right,
				  .AnchorAvailability = application::github_conversation_anchor_availability::current },
			  application::github_conversation_projection{
				  .Id          = "review-comment:t22-1",
				  .ParentId    = "review-thread:t22",
				  .Kind        = application::github_conversation_kind::review_comment,
				  .AuthorLogin = "tbutler",
				  .Body        = "Where does 0.01 come from?",
				  .Path        = "src/file.cpp" } },
			QStringLiteral("one conversation"));
		static_cast<void>(publish_fixture(window, diff));
		window.set_conversation_endpoint_placement(false, true);
		process_events();
		auto* const panel = window.conversation_panel();
		auto* const state = panel->findChild<QStackedWidget*>(QStringLiteral("githubConversationState"));
		auto* const cards = panel->cards();
		const auto  shows = [&] {
			return state != nullptr && state->currentWidget()->objectName() == QStringLiteral("githubConversationLoaded")
				&& cards->isVisible()
				&& std::ranges::find(cards->cards(), std::string{ "review-thread:t22" }, &qt::thread_card::Id) != cards->cards().end();
		};
		expect(shows(), "the Comments drawer shows the file's thread card");
		const auto admit = [&window](const std::optional<core::semantic_selection>& aSelection) {
			if (aSelection) {
				window.review_editor()->present(core::review_comment_draft{
					.Id = core::review_draft_id{ 1U }, .Anchor = aSelection->PreferredLine, .Body = { } });
			}
		};
		window.comparison_surface()->set_selection_sink(admit);
		expect(window.comparison_surface()->select_conversation_line(core::diff_side::right, 9U), "line 10, which no thread is on, is selected");
		admit(window.comparison_surface()->selection());
		process_events();
		expect(shows() && panel->anchor_line().has_value() && window.conversation_model()->rowCount() == 0
				   && !panel->thread_map()->isVisible() && !panel->view()->isVisible(),
			   "a line no thread is on keeps the cards; only the empty map and detail step aside");
		auto* const comment = window.comment_action();
		auto* const stack   = window.findChild<QStackedWidget*>(QStringLiteral("workbenchCentreStack"));
		auto* const page    = window.findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
		comment->trigger();
		process_events();
		expect(stack->currentWidget() == page && window.review_editor()->body_editor()->isEnabled(),
			   "Comment opens the editable inline page");
		window.review_editor()->body_editor()->setPlainText(QStringLiteral("local text"));
		window.review_editor()->discard_action()->trigger();
		process_events();
		auto* const back = page->findChild<QPushButton*>(QStringLiteral("composeInlineDraftPageBack"));
		expect(back != nullptr, "the page has Back");
		back->click();
		process_events();
		expect(stack->currentWidget() != page, "Back returns to the comparison");
		expect(shows(), "after Discard and Back the drawer shows the file's thread card without another selection");
		save_render(window, QStringLiteral("discard-back-drawer"));
		const auto discards  = std::ranges::count_if(drafts, [](const core::review_draft_command& aCommand) {
			return aCommand.Kind == core::review_draft_command_kind::discard;
		});
		const auto publishes = std::ranges::count_if(drafts, [](const core::review_draft_command& aCommand) {
			return aCommand.Kind == core::review_draft_command_kind::begin_publish;
		});
		expect(discards == 1 && publishes == 0, "Discard sends one discard and nothing publishes");
		// The thread's own line still filters to it and shows its detail.
		expect(window.comparison_surface()->select_conversation_line(core::diff_side::right, 21U), "line 22, the thread's line, is selected");
		process_events();
		expect(shows() && window.conversation_model()->rowCount() == 1 && panel->thread_map()->isVisible() && panel->view()->isVisible(),
			   "the thread's line filters to it with its map and detail");
		// Negative control: a snapshot with no conversation keeps the empty page.
		panel->adopt({ }, QStringLiteral("no conversations"));
		process_events();
		expect(state->currentWidget()->objectName() == QStringLiteral("githubConversationEmptyState"),
			   "an empty snapshot still shows the empty page");
		window.hide();
	}

	void window_key(qt::workbench_window& aWindow, int aKey, Qt::KeyboardModifiers aModifiers);

	// [[BPR-1132]]. The assembled comparison header, file gestures and older-pair notice.
	void test_document_tabs_present_exact_views_and_translate_gestures()
	{
		qt::workbench_window window;
		window.resize(1280, 852);
		window.show();
		const auto expectedScale = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE", QStringLiteral("1.0")).toDouble();
		expect(std::abs(window.devicePixelRatioF() - expectedScale) < 0.001, "tab course runs at its declared scale");
		window.publish(every_primary_command_offered());
		window.choose_modality(qt::review_modality::review);
		const auto file      = core::file_identity_of("src/file.cpp");
		const auto otherFile = core::file_identity_of("tests/file.cpp");
		window.publish(application::workbench_projection{ .Files = std::vector<application::workbench_file_projection>{
															  { .File = file, .Path = "src/file.cpp" }, { .File = otherFile, .Path = "tests/file.cpp" } } });
		const auto old  = epoch_row(1);
		const auto now  = epoch_row(2);
		const auto pair = [](const auto& aRow) { return core::comparison_pair{
													 epoch_anchor(aRow, core::comparison_anchor_kind::epoch_base), epoch_anchor(aRow, core::comparison_anchor_kind::epoch_head)
												 }; };
		window.publish(retained_epochs({ old, now }));
		window.publish(resolved_pair_update(pair(old)));
		auto intents = std::vector<core::document_tab_intent>{ };
		window.set_document_tab_dispatch([&](const auto& aIntent) { intents.push_back(aIntent); });
		auto projection = application::document_tabs_projection{
			.Tabs             = { { .Id = core::document_tab_id{ 1 }, .View = { pair(old), file, { } }, .State = core::document_tab_view_state::ready, .Active = true, .OwnComparison = true },
								  { .Id = core::document_tab_id{ 2 }, .View = { pair(now), file, { } }, .State = core::document_tab_view_state::ready },
								  { .Id = core::document_tab_id{ 3 }, .View = { pair(now), otherFile, { } }, .State = core::document_tab_view_state::ready } },
			.WindowComparison = pair(now)
		};
		window.publish(projection);
		process_events();
		auto* strip = window.document_tabs();
		auto* bar   = strip->tab_bar();
		expect(strip->isVisible() && bar->elideMode() == Qt::ElideLeft, "comparison header has tabs with left elision");
		expect(strip->tabs()[0].Name == QStringLiteral("src/file.cpp") && strip->tabs()[2].Name == QStringLiteral("tests/file.cpp"), "equal basenames retain distinguishing paths");
		expect(!strip->tabs()[0].OwnPair.isEmpty() && strip->tabs()[1].OwnPair.isEmpty(), "only a distinct pair carries a pill");
		expect(bar->accessibleTabName(0).contains(QStringLiteral("comparing")) && bar->tabToolTip(0).contains(QStringLiteral("src/file.cpp")), "own pair and full path are accessible");
		expect(!bar->findChildren<QWidget*>(QStringLiteral("documentTabPairPill")).isEmpty(), "own pair is rendered as a pill");
		const auto unchanged = bar->tabButton(0, QTabBar::LeftSide);
		window.publish(projection);
		expect(bar->tabButton(0, QTabBar::LeftSide) == unchanged && intents.empty(), "unchanged projection preserves widgets and sends no intent");
		auto       menu        = std::unique_ptr<QMenu>{ strip->make_tab_menu(core::document_tab_id{ 1 }, &window) };
		const auto triggerMenu = [&](const char* aName) { auto* action = menu->findChild<QAction*>(QString::fromLatin1(aName)); expect(action && action->isEnabled(), "tab menu command available"); action->trigger(); };
		triggerMenu("documentTabDuplicate");
		expect(std::get<core::duplicate_tab_intent>(intents.back()).Tab == core::document_tab_id{ 1 }, "Duplicate names its exact tab");
		triggerMenu("documentTabClose");
		expect(std::get<core::close_tab_intent>(intents.back()).Tab == core::document_tab_id{ 1 }, "Close names its exact tab");
		triggerMenu("documentTabCloseOthers");
		expect(std::get<core::close_other_tabs_intent>(intents.back()).Tab == core::document_tab_id{ 1 }, "Close others preserves its exact tab");
		triggerMenu("documentTabReturnToWindow");
		expect(std::holds_alternative<core::return_tab_to_window_comparison_intent>(intents.back()), "return to window is a semantic intent");
		triggerMenu("documentTabMakeWindow");
		expect(std::holds_alternative<core::make_tab_comparison_window_intent>(intents.back()), "make window is a semantic intent");
		strip->new_tab_button()->click();
		expect(std::get<core::duplicate_tab_intent>(intents.back()).Tab == core::document_tab_id{ 1 }, "plus duplicates active exact view");
		window.activateWindow();
		bar->setFocus(Qt::OtherFocusReason);
		process_events();
		const auto pressShortcut = [&](QAction* aAction) {
			const auto combination = aAction->shortcut()[0];
			const auto count       = intents.size();
			window_key(window, combination.key(), combination.keyboardModifiers());
			expect(intents.size() == count + 1U, "tab shortcut dispatches exactly one semantic intent");
		};
		pressShortcut(window.next_tab_action());
		expect(std::get<core::activate_adjacent_tab_intent>(intents.back()).Direction == core::change_navigation_direction::next, "next tab translates");
		pressShortcut(window.previous_tab_action());
		expect(std::get<core::activate_adjacent_tab_intent>(intents.back()).Direction == core::change_navigation_direction::previous, "previous tab translates");
		pressShortcut(window.close_tab_action());
		expect(std::get<core::close_tab_intent>(intents.back()).Tab == core::document_tab_id{ 1 }, "close key names active tab");
#if defined(Q_OS_MACOS)
		expect(window.next_tab_action()->shortcut() == QKeySequence{ QStringLiteral("Meta+Tab") }
				   && window.previous_tab_action()->shortcut() == QKeySequence{ QStringLiteral("Meta+Shift+Tab") },
			   "macOS tab navigation uses physical Control");
#else
		expect(window.next_tab_action()->shortcut() == QKeySequence{ QStringLiteral("Ctrl+Tab") }
				   && window.previous_tab_action()->shortcut() == QKeySequence{ QStringLiteral("Ctrl+Shift+Tab") },
			   "tab navigation uses Control");
#endif
		expect(window.close_tab_action()->shortcut() == QKeySequence{ QStringLiteral("Ctrl+W") }, "platform Close Tab key is bound");
		auto sequences = QList<QKeySequence>{ };
		for (auto* action : window.findChildren<QAction*>()) {
			if (!action->shortcut().isEmpty()) {
				expect(!sequences.contains(action->shortcut()), "assembled keys are unique");
				sequences.push_back(action->shortcut());
			}
		}
		auto* files = window.files_tree();
		files->expandAll();
		process_events();
		const auto index    = files->tree_model()->index_for_file(otherFile);
		auto       fileMenu = std::unique_ptr<QMenu>{ files->make_file_menu(index, &window) };
		expect(fileMenu && fileMenu->actions().size() == 1, "file offers one new-tab menu command");
		fileMenu->actions().front()->trigger();
		expect(std::get<core::open_in_new_tab_intent>(intents.back()).File == otherFile, "file menu carries exact identity");
		const auto  before        = files->currentIndex();
		const auto  centre        = QPointF{ files->visualRect(index).center() };
		const auto  previousCount = intents.size();
		QMouseEvent click{ QEvent::MouseButtonPress, centre, QPointF{ files->viewport()->mapToGlobal(centre.toPoint()) }, Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier };
		QApplication::sendEvent(files->viewport(), &click);
		expect(intents.size() == previousCount + 1U && std::get<core::open_in_new_tab_intent>(intents.back()).File == otherFile && files->currentIndex() == before, "Cmd/Ctrl click opens a background tab without selecting its file");
		auto selectedFiles = std::vector<core::workbench_file_id>{ };
		window.set_dispatch([&](const core::workbench_interaction_intent& aIntent) {
			if (const auto* selected = std::get_if<core::select_workbench_file_intent>(&aIntent)) {
				selectedFiles.push_back(selected->File);
			}
		});
		files->select_file(otherFile);
		QMouseEvent repeat{ QEvent::MouseButtonPress, centre, QPointF{ files->viewport()->mapToGlobal(centre.toPoint()) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(files->viewport(), &repeat);
		QMouseEvent release{ QEvent::MouseButtonRelease, centre, QPointF{ files->viewport()->mapToGlobal(centre.toPoint()) }, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(files->viewport(), &release);
		expect(selectedFiles == std::vector{ otherFile }, "clicking the retained file dispatches exactly once even without a cursor change");
		QApplication::sendEvent(files->viewport(), &repeat);
		QApplication::sendEvent(files->viewport(), &release);
		expect(selectedFiles == std::vector{ otherFile, otherFile }, "repeating the click after both tree and list cursors agree still dispatches once");
		files->select_file(files->tree_model()->file_at(before));
		window.conversation_panel()->adopt({ application::github_conversation_projection{
											   .Id = "review-thread:later", .Kind = application::github_conversation_kind::review_thread, .Path = "src/file.cpp", .Line = 1U, .DiffSide = application::github_conversation_diff_side::right, .AnchorAvailability = application::github_conversation_anchor_availability::current } },
										   QStringLiteral("one later thread"));
		static_cast<void>(publish_fixture(window, computed_comparison("old\n", "new\n", 0U, file)));
		window.set_conversation_endpoint_placement(false, false);
		process_events();
		expect(window.older_pair_notice()->isVisible() && window.older_pair_notice_text().contains(QStringLiteral("One thread is on later updates")), "older-pair notice counts later threads of this file");
		expect(!window.conversation_panel()->model()->index_for_id("review-thread:later").isValid()
				   && window.conversation_panel()->cards()->thread_count() == 0U,
			   "later threads are absent from both older-pair tree and cards");
		expect(!window.comment_action()->isEnabled(), "new Comment is unavailable off current head");
		window.show_later_threads_button()->click();
		expect(std::get<core::activate_tab_intent>(intents.back()).Tab == core::document_tab_id{ 2 }, "Show them routes to same-file window-pair sibling");
		auto single = projection;
		single.Tabs.resize(1U);
		window.publish(single);
		window.show_later_threads_button()->click();
		expect(std::get<core::return_tab_to_window_comparison_intent>(intents.back()).Tab == core::document_tab_id{ 1 }, "without a sibling Show them explicitly returns the active tab to the window pair");
		single.Tabs.front().OwnComparison = false;
		window.publish(single);
		const auto beforeDiscuss = intents.size();
		window.show_later_threads_button()->click();
		expect(window.current_modality() == qt::review_modality::discuss && intents.size() == beforeDiscuss, "when the window pair is also old Show them opens Discuss without retargeting a tab");
		window.choose_modality(qt::review_modality::review);
		window.publish(projection);
		window.set_conversation_endpoint_placement(false, true);
		process_events();
		expect(window.older_pair_notice()->isHidden(), "current-head comparison has no later-pair notice");
		expect(window.conversation_panel()->model()->index_for_id("review-thread:later").isValid()
				   && window.conversation_panel()->cards()->thread_count() == 1U,
			   "current-head comparison restores the complete file conversation");
		window.set_conversation_endpoint_placement(false, false);
		for (const auto count : { 1, 3, 12, 40 }) {
			auto sweep = projection;
			sweep.Tabs.resize(1U);
			for (int id = 2; id <= count; ++id) {
				sweep.Tabs.push_back({ .Id = core::document_tab_id{ static_cast<std::uint64_t>(id) }, .View = { pair(now), otherFile, { } } });
			}
			for (const auto width : { 1280, 1100, window.minimumWidth() }) {
				window.resize(width, 852);
				window.publish(sweep);
				process_events();
				auto*      viewport  = strip->scroll_area()->viewport();
				const auto expected  = std::max(qt::document_tab_strip::minimum_tab_width, viewport->width() / count);
				const auto equalFill = [&](int aCount, int aWidth) {
					return aCount == count && aWidth >= expected && aWidth <= expected + 1;
				};
				expect(bar->count() == count && equalFill(bar->count(), bar->tabRect(0).width()), "every tab fills an equal share or the pinned220pixel minimum");
				expect(!equalFill(1, 100) && !equalFill(count, 100), "negative controls reject collapsed-count and fixed100pixel strips");
				for (int i = 0; i < count; ++i) {
					expect(equalFill(bar->count(), bar->tabRect(i).width()) && !bar->accessibleTabName(i).isEmpty(), "every tab keeps equal width and accessible name");
				}
				expect(strip->findChild<QToolButton*>(QStringLiteral("documentTabOverflow")) == nullptr, "there is no count or overflow menu");
				expect(strip->rect().contains(strip->new_tab_button()->geometry()) && strip->new_tab_button()->isVisible(), "plus stays outside the scrolled tabs and reachable");
				expect(window.focus_order().contains(bar) && window.focus_order().contains(strip->new_tab_button()), "tab bar and plus remain keyboard stops");
				if (count > 3) {
					auto* scroll = strip->scroll_area()->horizontalScrollBar();
					scroll->setValue(0);
					QWheelEvent wheel{ QPointF{ 10, 10 }, QPointF{ 10, 10 }, QPoint{ }, QPoint{ 0, -120 }, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false };
					QApplication::sendEvent(viewport, &wheel);
					expect(scroll->value() > 0, "vertical mouse wheel scrolls horizontal tabs");
				}
				sweep.Tabs.front().Active = false;
				sweep.Tabs.back().Active  = true;
				window.publish(sweep);
				process_events();
				const auto rect = bar->tabRect(count - 1).translated(-strip->scroll_area()->horizontalScrollBar()->value(), 0);
				expect(rect.left() >= 0 && rect.right() < viewport->width(), "activation reveals the entire active tab including after restoration");
				bar->setFocus(Qt::OtherFocusReason);
				process_events();
				expect(bar->hasFocus(), "active strip remains focused and arrow-key reachable");
				save_render(window, QStringLiteral("document-tabs-%1-%2").arg(count).arg(width));
				sweep.Tabs.back().Active  = false;
				sweep.Tabs.front().Active = true;
			}
		}
		// Exercise the actual plus intent through the core instead of faking its active flag.
		auto tabsState = core::document_tabs_state{ };
		static_cast<void>(core::transition(tabsState, core::set_window_comparison_intent{ pair(now) }));
		const auto loaded = core::transition(tabsState, core::open_in_active_tab_intent{ file });
		expect(loaded.Load.has_value(), "plus course starts an exact view");
		const auto source = *tabsState.Active;
		const auto exact  = core::find_tab(tabsState, source)->View;
		auto       shown  = application::document_tabs_projection{ .Tabs = { { .Id = source, .View = exact, .Active = true } }, .WindowComparison = pair(now) };
		window.publish(shown);
		window.set_document_tab_dispatch([&](const auto& aIntent) {
			const auto result = core::transition(tabsState, aIntent);
			expect(result.Load.has_value(), "plus creates an active load, never a background-only duplicate");
			shown.Tabs.clear();
			for (const auto& tab : tabsState.Tabs) {
				shown.Tabs.push_back({ .Id = tab.Id, .View = tab.View, .Active = tabsState.Active == tab.Id });
			}
			window.publish(shown);
		});
		strip->new_tab_button()->click();
		process_events();
		expect(tabsState.Active != source && core::find_tab(tabsState, *tabsState.Active)->View == exact && bar->hasFocus(), "plus activates the exact duplicated view and focuses its tab");
		auto       background       = tabsState;
		const auto activeBefore     = background.Active;
		const auto backgroundResult = core::transition(background, core::open_in_new_tab_intent{ otherFile });
		expect(!backgroundResult.Load && background.Active == activeBefore, "Cmd-click remains background and fails the active-load predicate");
		window.set_document_tab_dispatch([&](const auto& aIntent) { intents.push_back(aIntent); });
		// [[BPR-1197]]. An elided header accompanies a complete explanation
		// in the empty comparison; it does not replace that explanation.
		const auto  explanation = QStringLiteral("Comparison unavailable — the file cannot be read at the earlier selected revision. Select another file or comparison to continue. <literal text>");
		auto* const surface     = window.comparison_surface();
		surface->clear(explanation);
		auto* const status = strip->status_label();
		auto* const full   = surface->findChild<QLabel*>(QStringLiteral("diffWorkbenchStatus"));
		for (const auto width : { 1280, 1100, window.minimumWidth() }) {
			window.resize(width, 852);
			process_events();
			expect(status->isVisible() && status->text() == explanation && status->toolTip() == explanation
					   && status->accessibleDescription() == explanation,
				   "elided tab status preserves full tooltip and description");
			auto* const accessible = QAccessible::queryAccessibleInterface(status);
			expect(accessible && accessible->text(QAccessible::Description) == explanation && !accessible->state().invisible,
				   "elided tab status exposes the entire explanation to accessibility");
			expect(strip->height() == 28 && strip->rect().contains(status->geometry())
					   && status->height() >= status->fontMetrics().height() && status->width() >= status->fontMetrics().horizontalAdvance(QStringLiteral("…")) + 14
					   && status->fontMetrics().horizontalAdvance(explanation) > status->width(),
				   "long status legitimately elides within the fixed-height tab strip");
			expect(full && full->isVisible() && full->text() == explanation && full->textFormat() == Qt::PlainText
					   && full->wordWrap() && full->height() >= full->heightForWidth(full->width())
					   && surface->rect().contains(full->geometry()),
				   "empty comparison shows the whole literal explanation without clipping");
			save_render(window, QStringLiteral("document-tabs-unavailable-%1").arg(width));
		}
		static_cast<void>(publish_fixture(window, computed_comparison("old\n", "new\n", 0U, file)));
		process_events();
		expect(full->isHidden() && status->isHidden() && strip->height() == 28, "text comparison recovery restores the compact header");
		window.hide();
	}

	void test_composed_epoch_selectors_show_exact_retained_epochs()
	{
		std::vector<application::epoch_intent> epochs;
		qt::workbench_window                   window;

		auto* const selectors = window.epoch_selectors();
		// [[BPR-0737]]. The control is the review surface's first row, above
		// the central splitter and outside every pane.
		expect(selectors != nullptr
				   && selectors->parentWidget()
					   == window.findChild<QWidget*>(
						   QStringLiteral("workbenchReviewSurface"))
				   && selectors->parentWidget() == window.central_splitter()->parentWidget(),
			   "the composed epoch selectors sit on the review surface above the "
			   "central splitter, outside the centre pane");
		expect(window.findChild<QWidget*>(
				   QStringLiteral("workbenchEpochSelectorPlaceholder"))
				   == nullptr,
			   "no empty selector placeholder survives beside the real control");
		selectors->set_dispatch(
			[&epochs](application::epoch_intent aIntent) {
				epochs.push_back(std::move(aIntent));
			});

		const auto first  = epoch_row(1);
		const auto second = epoch_row(2);
		const auto third  = epoch_row(3);
		window.publish(retained_epochs({ first, second, third }));
		expect(window.statistics().EpochUpdates == 1U,
			   "a review-history update reaches the shell through its own "
			   "projection port");

		auto* const left  = selectors->left_selector();
		auto* const right = selectors->right_selector();
		expect(left != nullptr && right != nullptr && left != right
				   && left->model() == selectors->history_model()
				   && right->model() == selectors->history_model(),
			   "the shell offers two independent selectors over one retained "
			   "review history");
		expect(left->count() == 6 && right->count() == 6,
			   "every retained epoch offers its base and its head anchor to both "
			   "selectors");
		expect(left->itemText(2)
					   == QStringLiteral(
						   "Epoch 2 · Base — Author upload — Available")
				   && left->itemText(3)
					   == QStringLiteral(
						   "Epoch 2 · Head — Author upload — "
						   "Available"),
			   "each selector entry labels the exact retained epoch and anchor");

		// An arbitrary retained pair: the oldest base against the newest head.
		const auto pair = core::comparison_pair{
			.Left  = epoch_anchor(first, core::comparison_anchor_kind::epoch_base),
			.Right = epoch_anchor(third, core::comparison_anchor_kind::epoch_head),
		};
		window.publish(resolved_pair_update(pair));

		auto* const leftOid  = selectors->left_oid_editor();
		auto* const rightOid = selectors->right_oid_editor();
		const auto  expectedLeft
			= QString::fromStdString(epoch_commit_text(epoch_base_seed(1)));
		const auto expectedRight
			= QString::fromStdString(epoch_commit_text(epoch_head_seed(3)));
		expect(expectedLeft.size() == 40 && expectedRight.size() == 40
				   && expectedLeft != expectedRight,
			   "the fixture carries two different complete forty-character OIDs");
		expect(leftOid->text() == expectedLeft
				   && rightOid->text() == expectedRight,
			   "each selector states the exact complete commit OID of its "
			   "selected anchor");
		expect(leftOid->text() != expectedLeft.left(7)
				   && leftOid->text() != expectedLeft.left(8)
				   && leftOid->text() != expectedLeft.left(12)
				   && !leftOid->text().contains(QStringLiteral("…"))
				   && !leftOid->text().contains(QStringLiteral("...")),
			   "the exact OID is the whole value rather than an abbreviation or "
			   "an elision");

		leftOid->selectAll();
		rightOid->selectAll();
		expect(leftOid->isReadOnly() && rightOid->isReadOnly()
				   && leftOid->selectedText() == expectedLeft
				   && rightOid->selectedText() == expectedRight,
			   "the whole exact OID is selectable and copyable out of the shell "
			   "without being editable");

		// Independence: moving one selector asks for one side and leaves the
		// other side's decided anchor exactly where the core put it.
		epochs.clear();
		const auto rightBefore = right->currentIndex();
		left->setCurrentIndex(3);
		expect(epochs.size() == 1U,
			   "one selector gesture emits exactly one semantic intent");
		const auto* const leftIntent = anchor_intent(epochs.front());
		expect(leftIntent != nullptr
				   && leftIntent->Side == core::comparison_side::left
				   && leftIntent->Anchor.Kind
					   == core::comparison_anchor_kind::epoch_head
				   && leftIntent->Anchor.Epoch == core::epoch_ordinal{ 2 },
			   "the left selector asks for the exact arbitrary retained epoch it "
			   "shows");
		expect(right->currentIndex() == rightBefore
				   && rightOid->text() == expectedRight,
			   "moving the left selector moves neither the right selector nor its "
			   "exact OID");
		expect(left->currentIndex() == 0,
			   "the selector re-asserts the decided anchor rather than keeping a "
			   "selection presentation chose for itself");

		epochs.clear();
		right->setCurrentIndex(0);
		expect(epochs.size() == 1U,
			   "the second selector gesture also emits exactly one intent");
		const auto* const rightIntent = anchor_intent(epochs.front());
		expect(rightIntent != nullptr
				   && rightIntent->Side == core::comparison_side::right
				   && rightIntent->Anchor.Kind
					   == core::comparison_anchor_kind::epoch_base
				   && rightIntent->Anchor.Epoch == core::epoch_ordinal{ 1 },
			   "the right selector independently asks for any retained epoch, "
			   "including one earlier than the left side shows");

		// The reverse pair proves the two sides really are independent rather
		// than one ordered range presented twice.
		const auto reversed = core::comparison_pair{
			.Left  = epoch_anchor(third, core::comparison_anchor_kind::epoch_head),
			.Right = epoch_anchor(first, core::comparison_anchor_kind::epoch_base),
		};
		window.publish(resolved_pair_update(reversed));
		expect(leftOid->text() == expectedRight
				   && rightOid->text() == expectedLeft,
			   "an arbitrary reversed retained pair reaches both selectors "
			   "exactly");
	}

	void test_review_history_state_is_explicit_in_workbench_text()
	{
		{
			// [[BPR-0775]]. Home's line is the session's alone. Review history
			// is the review surface's, and Home shows none of it.
			qt::workbench_window home;
			const auto           session = home.status_text();
			home.publish(retained_epochs({ epoch_row(1), epoch_row(2) }));
			expect(home.showing_home()
					   && session == QStringLiteral("Not signed in. Public pull requests open read-only.")
					   && home.status_text() == session,
				   "on Home the line is the session line and states no review history");
		}
		qt::workbench_window window;
		window.publish(both_files_enabled());
		const auto inactive = window.status_text();
		expect(!window.showing_home()
				   && inactive == QStringLiteral("No pull request is open in the workbench."),
			   "before any review-history update the review surface states only its own "
			   "state");

		// An empty loaded catalog is said to be empty only once review history
		// is ready (eecdda2a); before that it is still setting up, not empty.
		auto emptyCatalog       = retained_epochs({ });
		emptyCatalog.Transition = application::epoch_transition_projection{
			.Status = core::epoch_status_delta{ .Status = core::epoch_status::ready },
		};
		window.publish(emptyCatalog);
		const auto empty = window.status_text();
		window.publish(retained_epochs({ epoch_row(1), epoch_row(2) }));
		const auto retained = window.status_text();
		window.publish(
			history_page_update(core::epoch_history_page_phase::loading, { }));
		const auto loading = window.status_text();
		window.publish(
			history_page_update(core::epoch_history_page_phase::failed, { }));
		const auto failed = window.status_text();
		window.publish(
			history_page_update(core::epoch_history_page_phase::cancelled, { }));
		const auto cancelled = window.status_text();

		const std::array<QString, 5> states{
			empty, retained, loading, failed, cancelled
		};
		for (std::size_t outer = 0U; outer < states.size(); ++outer) {
			expect(!states[outer].isEmpty(),
				   "every review-history state reaches the shell as text");
			for (std::size_t inner = outer + 1U; inner < states.size(); ++inner) {
				expect(states[outer] != states[inner],
					   "loading, failure, cancellation, empty, and retained "
					   "review history each read differently in text rather "
					   "than differing only in colour or enabled state");
			}
		}
		expect(empty.contains(
				   QStringLiteral("No review epochs have been recorded.")),
			   "an empty review history says so in words");
		expect(loading.contains(QStringLiteral("Loading")),
			   "a loading review history says so in words");
		expect(failed.contains(QStringLiteral("could not be loaded")),
			   "a failed review-history load says so in words");
		expect(cancelled.contains(QStringLiteral("canceled")),
			   "a canceled review-history load says so in words");
		for (const auto& state : states) {
			expect(state.startsWith(inactive),
				   "the shell states one combined line: its own state first, "
				   "then the review-history state the composed control decided");
		}

		// One published change, one announcement, made by whichever control
		// decided it.
		const auto before        = window.statistics();
		const auto controlBefore = window.epoch_selectors()->statistics();
		window.publish(retained_epochs({ epoch_row(1) }));
		expect(window.statistics().EpochUpdates == before.EpochUpdates + 1U,
			   "the window observes each review-history update exactly once");
		expect(window.statistics().Announcements == before.Announcements
				   && window.epoch_selectors()->statistics().Announcements
					   > controlBefore.Announcements,
			   "a review-history change is announced once, by the control that "
			   "decided it, rather than twice");

		// The shell's own host failure stays distinguishable inside the same
		// combined line.
		window.fail({ .Failure
					  = application::workbench_host_failure::settings_unavailable });
		expect(window.status_text().contains(
				   QStringLiteral("The saved workbench layout is unavailable."))
				   && window.status_text() != cancelled,
			   "a workbench host failure is explicit in the same combined line "
			   "and does not read like a review-history state");

		window.fail({ .Failure = application::workbench_host_failure::selected_file_comparison_unavailable });
		const auto unavailable = window.status_text();
		window.fail({ .Failure = application::workbench_host_failure::selected_file_stale });
		const auto stale = window.status_text();
		expect(unavailable.contains(QStringLiteral("No exact comparison is ready"))
				   && unavailable.contains(QStringLiteral("Check Review history, then retry"))
				   && stale.contains(QStringLiteral("selected file changed"))
				   && unavailable != stale,
			   "an unavailable comparison gives actionable recovery without claiming the selected file changed");
	}

	/// [[BPR-0742]]. An unavailable saved layout is retained window state, not a
	/// one-shot host failure: it survives the projections that clear `Failure`.
	void test_unavailable_saved_layout_survives_workbench_projections()
	{
		qt::workbench_window window;
		const auto           sentence = QStringLiteral("The saved workbench layout is unavailable.");
		expect(!window.status_text().contains(sentence), "a window starts without a layout-unavailable sentence");

		const auto before = window.statistics();
		window.set_saved_layout_unavailable(true);
		expect(window.status_text().contains(sentence) && window.statistics().Failures == before.Failures,
			   "an unavailable saved layout is stated without delivering a host failure");

		window.publish(application::workbench_projection{
			.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									 core::workbench_drawer_state::expanded) });
		window.publish(application::workbench_projection{
			.Layout = application::workbench_layout_delta{ .FilesWidth = 320U } });
		expect(window.statistics().Publications == before.Publications + 2U
				   && window.status_text().contains(sentence),
			   "the sentence survives workbench projections");

		window.fail({ .Failure = application::workbench_host_failure::settings_unavailable });
		expect(window.status_text().count(sentence) == 1,
			   "a host settings failure and the retained state say the sentence once");
		window.publish(application::workbench_projection{
			.Drawers = drawer_states(core::workbench_drawer_state::collapsed,
									 core::workbench_drawer_state::expanded) });
		expect(window.status_text().count(sentence) == 1,
			   "clearing the host failure keeps the retained sentence");

		window.set_saved_layout_unavailable(false);
		expect(!window.status_text().contains(sentence), "an available saved layout is not reported");
	}

	[[nodiscard]] auto accessible_names(const QList<QWidget*>& aStops)
		-> QStringList
	{
		auto names = QStringList{ };
		for (auto* const stop : aStops) {
			names.push_back(stop->accessibleName());
		}
		return names;
	}

	/// \brief Walks the live focus chain and reports the declared stops it met.
	///
	/// \details The chain is circular, so the walk starts at the first declared
	/// stop and stops when it returns there. Widgets outside the declared order
	/// are skipped, which is what makes this a relative-order check rather than
	/// a claim about every widget Qt happens to keep in the chain.
	[[nodiscard]] auto walked_focus_order(const QList<QWidget*>& aStops)
		-> QList<QWidget*>
	{
		auto walked = QList<QWidget*>{ };
		if (aStops.isEmpty()) {
			return walked;
		}
		auto* const start  = aStops.front();
		auto*       cursor = start;
		for (auto step = 0; step < 8'192; ++step) {
			cursor = cursor->nextInFocusChain();
			if (cursor == nullptr || cursor == start) {
				break;
			}
			if (aStops.contains(cursor)) {
				walked.push_back(cursor);
			}
		}
		return walked;
	}

	void test_focus_order_is_declared_named_and_walkable()
	{
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		// [[BPR-0650]]. The declared order is the workbench's; Home shows
		// until a workbench is active, so one is made active first.
		window.publish(every_primary_command_offered());
		process_events();
		// [[BPR-0981]]. The order is claimed at the scale the row registers.
		if (const auto expected = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE"); !expected.isEmpty()) {
			expect(std::abs(window.devicePixelRatioF() - expected.toDouble()) < 0.001,
				   "the focus order is walked at the exact expected device scale");
		}

		const auto expanded = window.focus_order();
		expect(expanded.size() >= 12,
			   "the shell declares every interactive stop it owns");
		auto declared = QStringList{ };
		for (auto* const stop : expanded) {
			expect(stop != nullptr && stop->focusPolicy() != Qt::NoFocus,
				   "every declared focus stop can actually take keyboard focus");
			expect(!stop->accessibleName().isEmpty()
					   && !stop->accessibleDescription().isEmpty(),
				   "every interactive element carries an accessible name and an "
				   "accessible description");
			expect(!declared.contains(stop->accessibleName()),
				   "no two interactive elements answer to one accessible name");
			declared.push_back(stop->accessibleName());
		}
		expect(declared.contains(QStringLiteral("Left review epoch"))
				   && declared.contains(QStringLiteral("Right review epoch"))
				   && declared.contains(QStringLiteral("Exact left commit OID"))
				   && declared.contains(QStringLiteral("Exact right commit OID")),
			   "both epoch selectors and both exact OIDs are reachable stops in "
			   "the shell's declared focus order");
		// [[BPR-0981]]. RFC-BPR-0025 AC-09: each drawer's filter, its Clear
		// filter and the files order selector are declared stops before their tree.
		expect(declared.indexOf(QStringLiteral("Filter pull requests")) >= 0
				   && declared.indexOf(QStringLiteral("Clear pull requests filter"))
					   == declared.indexOf(QStringLiteral("Filter pull requests")) + 1
				   && declared.indexOf(QStringLiteral("Filter pull requests"))
					   < declared.indexOf(window.repositories_tree()->accessibleName())
				   && declared.indexOf(QStringLiteral("Files order")) >= 0
				   && declared.indexOf(QStringLiteral("Filter files"))
					   == declared.indexOf(QStringLiteral("Files order")) + 1
				   && declared.indexOf(QStringLiteral("Clear files filter"))
					   == declared.indexOf(QStringLiteral("Filter files")) + 1
				   && declared.indexOf(QStringLiteral("Filter files"))
					   < declared.indexOf(window.files_tree()->accessibleName()),
			   "both drawer filters, their Clear filter and the files order selector are "
			   "declared stops ahead of their trees");
		expect(walked_focus_order(expanded) == expanded.mid(1),
			   "the live focus chain walks the declared order exactly");
		// [[BPR-1129]]. The review bar's stops follow the title row's, in the
		// bar's reading order.
		const auto barStop  = [&window](QAction* const aAction) { return window.review_bar()->widgetForAction(aAction); };
		const auto barOrder = QList<QWidget*>{
			window.epoch_selectors()->comparing_button(),
			barStop(window.previous_file_action()),
			barStop(window.next_file_action()),
			barStop(window.previous_change_action()),
			barStop(window.next_change_action()),
			window.review_controls_button(),
			barStop(window.compose_review_action()),
			barStop(window.comment_action()),
		};
		const auto barFirst = expanded.indexOf(barOrder.front());
		expect(barFirst > expanded.indexOf(window.primary_toolbar()->widgetForAction(window.settings_action()))
				   && expanded.mid(barFirst, barOrder.size()) == barOrder,
			   "the review bar's controls are declared stops right after the title row's, in the bar's order");

		// No reachable focusable widget anywhere in the assembled window may
		// answer to a name another reachable focusable widget already uses.
		// Reachable is part of the rule rather than a convenience: Qt keeps a
		// hidden zero-size first splitter handle, which no keyboard or assistive
		// user can ever land on.
		auto seen = QStringList{ };
		for (auto* const widget : window.findChildren<QWidget*>()) {
			if (widget->focusPolicy() == Qt::NoFocus || !widget->isVisible()
				|| widget->accessibleName().isEmpty()) {
				continue;
			}
			expect(!seen.contains(widget->accessibleName()),
				   message_for("no accessible name is shared by two focusable "
							   "widgets in the assembled workbench",
							   (widget->accessibleName() + QStringLiteral(" on ")
								+ widget->metaObject()->className()
								+ QStringLiteral(" ") + widget->objectName())
								   .toStdString()));
			seen.push_back(widget->accessibleName());
		}

		// An ambiguous shortcut overload triggers neither action, so one key
		// sequence may never be offered twice in one window.
		auto sequences = QList<QKeySequence>{ };
		for (auto* const action : window.findChildren<QAction*>()) {
			if (action->shortcut().isEmpty()) {
				continue;
			}
			expect(!sequences.contains(action->shortcut()),
				   "no keyboard command is claimed twice in the assembled "
				   "workbench");
			sequences.push_back(action->shortcut());
		}
		expect(!sequences.contains(
				   window.review_controls_button()->shortcut()),
			   "the secondary review popup's own shortcut is not claimed by an action "
			   "as well");

		window.resize(window.minimumWidth(), 720);
		process_events();
		const auto compact = window.focus_order();
		const auto lead    = strip_lead_actions;
#if defined(_WIN32)
		// [[BPR-0653]]. The compact menu control reads first, as it sits, and
		// the three caption buttons follow Settings as the strip's last stops.
		const auto settings = std::ranges::find(
			compact, window.primary_toolbar()->widgetForAction(window.settings_action()));
		expect(compact.front() == window.strip_menu_button()
				   && window.caption_buttons() != nullptr && settings != compact.end()
				   && std::distance(settings, compact.end()) > 3
				   && *(settings + 1) == window.caption_buttons()->button(qt::title_row_part::minimize)
				   && *(settings + 2) == window.caption_buttons()->button(qt::title_row_part::maximize)
				   && *(settings + 3) == window.caption_buttons()->button(qt::title_row_part::close),
			   "on Windows the menu control is the first stop and minimise, maximise and close follow Settings");
#endif
		expect(compact[lead]
					   == window.primary_toolbar()->widgetForAction(window.home_action())
				   && compact[lead + 1]
					   == window.primary_toolbar()->widgetForAction(
						   window.repositories_drawer_toggle_action())
				   && compact[lead + 2]
					   == window.primary_toolbar()->widgetForAction(
						   window.files_drawer_toggle_action()),
			   "Home, Repositories then Files remain the first interactive stops at the supported width floor");
		for (auto* const stop : compact) {
			expect(!stop->accessibleName().isEmpty()
					   && !stop->accessibleDescription().isEmpty(),
				   "the compact icon row keeps every stop named and described");
		}
		expect(accessible_names(compact) == declared,
			   "narrowing the window never removes or replaces a primary navigation stop");
		expect(walked_focus_order(compact) == compact.mid(1),
			   "the compact focus chain walks its declared order exactly");

		window.resize(1'280, 820);
		process_events();
		const auto restored = window.focus_order();
		expect(accessible_names(restored) == declared,
			   "returning to the wide layout retains the same declared focus order");
		expect(walked_focus_order(restored) == restored.mid(1),
			   "the restored focus chain walks the declared order exactly");
		window.hide();
	}

	void test_receipt_actions_follow_decided_projection_and_clear_while_loading()
	{
		intent_log           log;
		qt::workbench_window window;
		window.set_receipt_dispatch(
			[&log](core::file_receipt_command aCommand) {
				log.Receipt.push_back(std::move(aCommand));
			});
		window.show();
		// [[BPR-0775]]. Receipt progress is the review surface's to state; Home's
		// line is the session's.
		window.publish(both_files_enabled());
		process_events();

		const auto receipt = core::file_receipt{
			.File     = core::file_identity_of("review.cpp"),
			.Material = file_material(73U),
		};
		window.publish(application::project_file_receipt_actions(receipt));
		process_events();
		expect(window.seen_action()->isEnabled()
				   && window.read_action()->isEnabled()
				   && window.accept_action()->isEnabled()
				   && window.exclude_action()->isEnabled(),
			   "the four compact receipt gestures use the application projection's legality");

		window.seen_action()->trigger();
		expect(log.Receipt == std::vector{ core::file_receipt_command{
				   .File     = receipt.File,
				   .Material = receipt.Material,
				   .Kind     = core::file_receipt_command_kind::mark_seen,
			   } },
			   "a receipt action emits the exact selected file, material, and semantic command");

		auto advanced     = receipt;
		advanced.Read     = core::file_read_state::seen;
		advanced.Accepted = true;
		window.publish(application::project_file_receipt_actions(advanced));
		process_events();
		expect(window.seen_action()->isChecked()
				   && !window.seen_action()->isEnabled()
				   && window.accept_action()->text().contains(
					   QStringLiteral("Revoke"))
				   && !window.exclude_action()->isEnabled()
				   && window.status_text().contains(
					   QStringLiteral("Private progress: seen, accepted")),
			   "accepted receipt projection decides checked state, inverse gesture, and status");

		window.clear_review_ledger();
		process_events();
		expect(!window.seen_action()->isEnabled()
				   && !window.read_action()->isEnabled()
				   && !window.accept_action()->isEnabled()
				   && !window.exclude_action()->isEnabled(),
			   "starting another selected diff clears the prior file's gesture surface");

		window.clear_review_ledger(true);
		process_events();
		expect(window.status_text().contains(
				   QStringLiteral("Private review progress is unavailable.")),
			   "a store-open obstruction is visible in text while the workbench remains live");
		window.hide();
	}

	void test_selection_synchronisation_and_restart_persistence()
	{
		const auto [leftText, rightText] = comparison_fixture_text();
		const auto diff                  = computed_comparison(leftText, rightText);

		intent_log log;
		auto       window = std::make_unique<qt::workbench_window>(
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{
				[&log](const core::workbench_drawer aDrawer,
					   const std::uint32_t          aWidth) {
					log.Width.push_back({ .Drawer = aDrawer, .Width = aWidth });
				} });
		window->resize(1'280, 820);
		window->show();
		process_events();

		auto persisted    = both_files_enabled();
		persisted.Drawers = drawer_states(core::workbench_drawer_state::expanded,
										  core::workbench_drawer_state::expanded);
		persisted.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 260U,
		};
		window->publish(persisted);
		process_events();
		auto* const surface = publish_fixture(*window, diff);

		// Selection synchronisation: one gesture, one semantic selection, and
		// every composed region answers with that same selection.
		const auto beforeSelection = surface->selection();
		const auto beforeDetail    = window->detail_pane()->toPlainText();
		expect(surface->navigate_change(core::change_navigation_direction::next,
										core::change_navigation_policy::stop),
			   "the semantic bridge selects the next change");
		process_events();
		const auto& selection = surface->selection();
		expect(selection.has_value() && selection != beforeSelection,
			   "selecting the next change moves the one semantic selection");
		expect(window->detail_pane() == surface->detail_pane(),
			   "the shell's detail pane is the composed surface's own pane, so "
			   "one selection can never produce two answers");
		expect(window->detail_pane()->toPlainText() != beforeDetail,
			   "the lower detail pane follows the selection the composed surface "
			   "made");
		const auto links = surface->link_map()->visible_links();
		expect(std::ranges::any_of(
				   links,
				   [&selection](const qt::visible_diff_link& aLink) {
					   return aLink.Identity.Group == selection->Group;
				   }),
			   "the link map paints the same selected group the lower detail "
			   "pane shows");

		// Restart persistence: a drag reports the host-owned width, and a fresh
		// window built from the settings the host persisted restores it.
		auto* const splitter = window->findChild<QSplitter*>(
			QStringLiteral("workbenchCentralSplitter"));
		expect(splitter != nullptr, "the shell owns one central splitter");
		log.Width.clear();
		drag_files_handle(*window, 380);
		process_events();
		expect(!log.Width.empty()
				   && log.Width.back().Drawer == core::workbench_drawer::files
				   && log.Width.back().Width == 380U,
			   "a resized drawer reports exactly the width the host must "
			   "persist");
		const auto persistedWidth = log.Width.back().Width;

		auto closing    = application::workbench_projection{ };
		closing.Drawers = drawer_states(core::workbench_drawer_state::expanded,
										core::workbench_drawer_state::collapsed);
		window->publish(closing);
		process_events();
		window->hide();
		window.reset();

		qt::workbench_window restarted;
		restarted.resize(1'280, 820);
		restarted.show();
		process_events();
		expect(restarted.files_drawer()->width() != static_cast<int>(persistedWidth)
				   || restarted.conversations_drawer()->isVisible(),
			   "a fresh shell starts without the persisted layout, so restoring "
			   "it is a real transition");

		auto reloaded    = both_files_enabled();
		reloaded.Drawers = drawer_states(core::workbench_drawer_state::expanded,
										 core::workbench_drawer_state::collapsed);
		reloaded.Layout  = application::workbench_layout_delta{
			.FilesWidth         = persistedWidth,
			.ConversationsWidth = 260U,
		};
		reloaded.Behaviour = drawer_controls_shown();
		restarted.publish(reloaded);
		process_events();
		expect(restarted.files_drawer()->width()
				   == static_cast<int>(persistedWidth),
			   "a restarted shell restores the drawer width the host persisted");
		expect(restarted.conversations_drawer()->width()
					   == qt::workbench_drawer_rail::width_pixels
				   && restarted.conversations_drawer()->rail()->isVisible()
				   && !restarted.conversations_drawer_toggle_action()->isChecked(),
			   "a restarted shell restores the persisted drawer state as well as "
			   "its width");
		// [[BPR-0727]]. Overview is chosen, never inferred, and no preset has
		// produced the files-only shape since [[BPR-0674]]; the segment keeps
		// the modality it had rather than reading a rule into the layout.
		expect(restarted.current_modality() == qt::review_modality::review,
			   "a restored layout with files open and comments collapsed infers "
			   "no modality; Overview is chosen, never inferred");
		restarted.hide();
	}

	// [[BPR-0656]]. The read state reaches the gutter and the strip: a new
	// thread's line is marked blue, the group counts the unread threads and
	// steps through them in file order, and a card's Mark read leaves through
	// the conversation read dispatch.
	// [[BPR-0649]]. Discuss widens the conversation over the code and puts
	// its panel in the Discuss arrangement; Open in code on a card returns
	// to Read at that thread's line.
	/// \brief [[BPR-1129]]. One key through the window system, where a platform
	/// hands keys to Qt, so the shortcut map sees it before any widget.
	///
	/// \details On macOS the Cocoa plugin offers a key press to the shortcut map
	/// itself before it delivers the key, and Qt's own key path skips the map
	/// there, so the press is offered the same way here.
	void window_key(qt::workbench_window& aWindow, const int aKey, const Qt::KeyboardModifiers aModifiers)
	{
#if defined(__APPLE__)
		if (QWindowSystemInterface::handleShortcutEvent(aWindow.windowHandle(), 0UL, aKey, aModifiers, 0U, 0U, 0U)) {
			QApplication::processEvents();
			return;
		}
#endif
		for (const auto type : { QEvent::KeyPress, QEvent::KeyRelease }) {
			QWindowSystemInterface::handleKeyEvent<QWindowSystemInterface::SynchronousDelivery>(
				aWindow.windowHandle(), type, aKey, type == QEvent::KeyPress ? aModifiers : Qt::NoModifier);
			QApplication::processEvents();
		}
	}

	/// \brief [[BPR-1129]]. Option-R (Alt+R) composes the review and Option-C
	/// (Alt+C) comments, from anywhere in the window with no visible entry row,
	/// and Esc returns from either page to where it was.
	void test_composition_keys_open_the_pages_and_esc_returns()
	{
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		window.publish(both_files_enabled());
		process_events();
		window.activateWindow();
		process_events();
		auto* const stack = window.findChild<QStackedWidget*>(QStringLiteral("workbenchCentreStack"));
		auto* const start = stack->currentWidget();
		window.modality()->button(qt::review_modality::review)->setFocus(Qt::OtherFocusReason);
		process_events();
		expect(QGuiApplication::focusWindow() == window.windowHandle() && start != nullptr,
			   "the keys start in the active window, on the comparison");
		window_key(window, Qt::Key_R, Qt::AltModifier);
		expect(stack->currentWidget()->objectName() == QStringLiteral("composeReviewPage")
				   && window.findChild<QPushButton*>(QStringLiteral("composeReviewPageBack"))->hasFocus(),
			   "Option-R (Alt+R) opens the no-draft explanation with Back focused");
		window_key(window, Qt::Key_Escape, Qt::NoModifier);
		expect(stack->currentWidget() == start && window.modality()->button(qt::review_modality::review)->hasFocus(),
			   "Esc returns from the review page to the invoking surface and focus");
		window_key(window, Qt::Key_C, Qt::AltModifier);
		expect(stack->currentWidget() == start && !window.comment_action()->isEnabled(),
			   "Option-C (Alt+C) without an admitted target keeps the useful surface open");
		window_key(window, Qt::Key_Escape, Qt::NoModifier);
		expect(stack->currentWidget() == start, "Esc returns from the comment page");
		window_key(window, Qt::Key_Escape, Qt::NoModifier);
		expect(stack->currentWidget() == start, "Esc with no page open leaves the comparison where it is");
		window.hide();
	}

	/// \brief [[BPR-1241]]. In the assembled window, with a stop selected in the
	/// tour outline, Option-Shift-U retargets the stop and Option-R still composes
	/// the review; away from the outline Option-Shift-U retargets nothing.
	void test_tour_retarget_key_reaches_only_the_outline()
	{
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		window.publish(both_files_enabled());
		process_events();
		window.activateWindow();
		process_events();
		auto* const editor = window.tour_editor();
		auto        fired  = std::vector<qt::tour_editor_action>{ };
		editor->set_dispatch([](core::tour_command) { }, [&fired](qt::tour_editor_action aAction) { fired.push_back(aAction); });
		application::tour_presentation tour;
		tour.Draft = std::make_shared<const core::review_tour>(tests::review_tour_fixture());
		editor->present(tour);
		window.open_review_tour();
		editor->show_stops();
		process_events();
		auto* const outline  = editor->outline();
		auto* const retarget = editor->findChild<QAction*>(QStringLiteral("tourRetarget"));
		auto* const menu     = editor->findChild<QMenu*>(QStringLiteral("tourStopMenu"));
		auto* const stack    = window.findChild<QStackedWidget*>(QStringLiteral("workbenchCentreStack"));
		auto* const start    = stack->currentWidget();
		expect(retarget != nullptr && menu != nullptr && outline->count() == 1 && outline->isVisible(),
			   "the open tour shows its one stop and the stop menu holds the retarget");
		const auto keys = retarget->shortcut().toString(QKeySequence::NativeText);
		expect(retarget->shortcut() == QKeySequence{ QStringLiteral("Alt+Shift+U") } && menu->toolTipsVisible()
				   && retarget->toolTip().contains(keys) && retarget->toolTip().contains(QStringLiteral("No code is copied")),
			   "the retarget is Option-Shift-U, and its menu tooltip names that key and says it copies no code");
		outline->setCurrentRow(0);
		outline->setFocus(Qt::OtherFocusReason);
		process_events();
		expect(outline->hasFocus() && editor->selected_stop() != nullptr, "the stop is selected and the outline has focus");
		window_key(window, Qt::Key_U, Qt::AltModifier | Qt::ShiftModifier);
		expect(fired == std::vector{ qt::tour_editor_action::retarget_stop } && stack->currentWidget() == start,
			   "Option-Shift-U on the outline retargets the selected stop once and opens nothing else");
		window_key(window, Qt::Key_R, Qt::AltModifier);
		expect(fired.size() == 1U && stack->currentWidget()->objectName() == QStringLiteral("composeReviewPage"),
			   "Option-R on the outline composes the review and does not retarget");
		window_key(window, Qt::Key_Escape, Qt::NoModifier);
		window.modality()->button(qt::review_modality::review)->setFocus(Qt::OtherFocusReason);
		process_events();
		window_key(window, Qt::Key_U, Qt::AltModifier | Qt::ShiftModifier);
		expect(fired.size() == 1U, "Option-Shift-U away from the outline retargets nothing");
		window.hide();
	}

	/// \brief [[BPR-1129]]. Compose review's mark is the kept-draft rule applied to
	/// what the workflow last presented: none for an empty draft, the amber mark
	/// and its words while a summary is kept, none again once it is discarded.
	void test_compose_review_marks_a_kept_draft()
	{
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		window.publish(both_files_enabled());
		process_events();
		auto* const button = qobject_cast<QToolButton*>(window.review_bar()->widgetForAction(window.compose_review_action()));
		const auto  marked = [button] { return button->property(qt::strip_style::mark_property).toBool(); };
		expect(button != nullptr && !marked() && !window.compose_review_action()->toolTip().contains(QStringLiteral("kept")),
			   "with no draft presented Compose review carries no mark");
		const auto head  = core::parse_git_commit_oid("abcdef0123456789abcdef0123456789abcdef01").value();
		auto       draft = core::review_submission_draft{
			.Id            = core::review_submission_draft_id{ 1U },
			.Target        = { .Host = core::git_host_id{ 1U }, .Repository = core::repository_id{ 2U }, .PullRequest = core::pull_request_id{ 3U }, .ExpectedHead = head },
			.ActingAccount = core::github_account_id{ 4U },
		};
		const auto present = [&window, &draft] {
			window.review_submission_editor_view()->present(qt::review_submission_editor_projection{ .Draft = draft, .Controls = core::controls(draft), .TargetDescription = QStringLiteral("target"), .Status = QStringLiteral("status") });
			process_events();
		};
		present();
		expect(!marked(), "an empty comment draft is not kept work");
		draft.Body = "Kept summary";
		present();
		expect(marked() && window.compose_review_action()->toolTip().contains(QStringLiteral("A review draft is kept"))
				   && button->accessibleDescription().contains(QStringLiteral("kept")),
			   "a kept summary marks Compose review and says so in its tooltip and description");
		draft.State = core::review_submission_state::discarded;
		present();
		expect(!marked() && !window.compose_review_action()->toolTip().contains(QStringLiteral("kept")),
			   "a discarded draft withdraws the mark");
		window.hide();
	}

	void test_composition_pages_retain_editors_and_never_submit_on_navigation()
	{
		qt::workbench_window window;
		window.publish(both_files_enabled());
		window.show();
		process_events();
		auto* whole         = window.review_submission_editor_view();
		auto* inlineEditor  = window.review_editor();
		auto* stack         = window.findChild<QStackedWidget*>(QStringLiteral("workbenchCentreStack"));
		auto* compose       = window.findChild<QAction*>(QStringLiteral("composeReviewAction"));
		auto* inlineCompose = window.findChild<QAction*>(QStringLiteral("composeInlineDraftAction"));
		auto* page          = window.findChild<QScrollArea*>(QStringLiteral("composeReviewPage"));
		auto* back          = window.findChild<QPushButton*>(QStringLiteral("composeReviewPageBack"));
		expect(compose && inlineCompose && page && back && stack, "composition pages have explicit entry and Back controls");
		expect(!window.findChild<QAction*>(QStringLiteral("showReviewThreadsAction")) && !window.show_review_threads_action(), "the focus-only reveal action is removed");
		auto gestures = std::vector<qt::review_submission_editor_intent>{ };
		window.set_review_submission_dispatch([&](auto aIntent) { gestures.push_back(std::move(aIntent)); });
		const auto head  = core::parse_git_commit_oid("abcdef0123456789abcdef0123456789abcdef01").value();
		const auto draft = core::review_submission_draft{
			.Id            = core::review_submission_draft_id{ 1U },
			.Target        = { .Host = core::git_host_id{ 1U }, .Repository = core::repository_id{ 2U }, .PullRequest = core::pull_request_id{ 3U }, .ExpectedHead = head },
			.ActingAccount = core::github_account_id{ 4U },
			.Body          = "Retained review summary"
		};
		const auto projection = qt::review_submission_editor_projection{
			.Draft = draft, .Controls = core::controls(draft), .TargetDescription = QStringLiteral("github.example.test · synthetic-owner/synthetic-repository#17\nHead: abcdef0123456789abcdef0123456789abcdef01\nSubmitting as synthetic-reviewer (account 4)"), .Status = QStringLiteral("Choose a review type and enter a summary.")
		};
		whole->present(projection);
		whole->body_editor()->moveCursor(QTextCursor::End);
		const auto cursor         = whole->body_editor()->textCursor().position();
		auto       inlineGestures = std::vector<core::revisioned_review_draft_command>{ };
		inlineEditor->set_revisioned_dispatch([&](auto aCommand) { inlineGestures.push_back(std::move(aCommand)); });
		auto acceptedInline = core::revisioned_review_draft{
			{ .Id     = core::review_draft_id{ 1U },
			  .Anchor = { .Comparison = opaque<core::file_comparison_digest>(2U), .Side = core::diff_side::right, .Line = 8U },
			  .Body   = "Accepted inline draft" },
			1U,
			core::review_draft_scope_id{ 1U }
		};
		inlineEditor->present(acceptedInline);
		inlineEditor->body_editor()->setPlainText(QStringLiteral("Local candidate"));
		acceptedInline.Revision   = 2U;
		acceptedInline.Draft.Body = "Other window's edit";
		inlineEditor->present_conflict({ acceptedInline, core::revisioned_draft_refusal::conflict });
		const auto candidate = inlineEditor->rejected_candidate();
		expect(candidate.has_value(), "fixture retains an exact rejected inline candidate");
		inlineGestures.clear();
		for (const auto mode : { qt::review_modality::review, qt::review_modality::read, qt::review_modality::discuss }) {
			window.choose_modality(mode);
			process_events();
			auto* invoking = window.modality()->button(mode);
			invoking->setFocus();
			compose->trigger();
			process_events();
			expect(stack->currentWidget() == page && whole->body_editor()->hasFocus(), "explicit entry opens and focuses retained whole-review editor");
			whole->present(projection);
			window.publish(both_files_enabled());
			process_events();
			expect(stack->currentWidget() == page && whole == window.review_submission_editor_view()
					   && whole->shown_draft() == draft && whole->body_editor()->textCursor().position() == cursor,
				   "same-authority projection retains page, editor identity, draft and cursor");
			back->click();
			process_events();
			expect(window.current_modality() == mode && invoking->hasFocus(), "Back restores invoking mode and focus");
			window.open_review_composition(true);
			process_events();
			expect(stack->currentWidget()->objectName() == QStringLiteral("composeInlineDraftPage")
					   && inlineEditor == window.review_editor() && inlineEditor->body_editor()->height() >= 90,
				   "inline drafting opens the same editor with an independently usable body");
			inlineEditor->present(acceptedInline);
			expect(inlineEditor->rejected_candidate()
					   && inlineEditor->rejected_candidate()->Command == candidate->Command
					   && inlineEditor->rejected_candidate()->Scope == candidate->Scope
					   && inlineEditor->rejected_candidate()->ExpectedRevision == candidate->ExpectedRevision
					   && inlineEditor->body_editor()->toPlainText() == QStringLiteral("Local candidate")
					   && !inlineEditor->publish_action()->isEnabled(),
				   "page/mode changes and same-authority refresh retain candidate and conflict refusal");
			window.close_review_composition();
		}
		for (const auto points : { 13, 20 }) {
			auto font = window.font();
			font.setPointSize(points);
			window.setFont(font);
			for (const auto width : { 900, 1400 }) {
				window.resize(width, 900);
				compose->trigger();
				process_events();
				expect(page->horizontalScrollBar()->maximum() == 0 && whole->width() <= page->viewport()->width()
						   && whole->body_editor()->height() >= 160,
					   "narrow/wide composition at normal/large fonts has no horizontal escape or compressed summary");
				for (auto* control : whole->findChildren<QWidget*>(QString{ }, Qt::FindDirectChildrenOnly)) {
					expect(whole->rect().contains(control->geometry()), "target, type, summary, actions and status stay inside the full-width editor");
				}
				window.resize(width, 400);
				process_events();
				expect(page->verticalScrollBar()->maximum() > 0 && whole->body_editor()->height() >= 160,
					   "short-height fallback scrolls the outer page and keeps summary usable");
			}
		}
		expect(gestures.empty(), "opening, Back, mode changes and projections never submit or edit a review");
		expect(inlineGestures.empty(), "inline page navigation never publishes or edits");
		auto foreignInline  = acceptedInline;
		foreignInline.Scope = core::review_draft_scope_id{ 2U };
		inlineEditor->present(foreignInline);
		expect(!inlineEditor->reapply_draft_action()->isEnabled() && !inlineEditor->publish_action()->isEnabled(),
			   "replacement authority cannot apply or publish the retained foreign candidate");
		whole->present({ .Status = QStringLiteral("The review target changed. Open a new review.") });
		expect(!whole->shown_draft() && whole->body_editor()->toPlainText().isEmpty() && !whole->submit_action()->isEnabled(),
			   "authority invalidation still clears and refuses the retained editor");
		window.hide();
	}

	void test_discuss_uses_main_content_and_open_in_code_returns_to_read()
	{
		qt::workbench_window window;
		window.resize(1'600, 820);
		window.show();
		process_events();
		auto opened = every_primary_command_offered(
			core::change_navigation_policy::wrap);
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 375U,
		};
		window.publish(opened);
		process_events();
		auto text = std::string{ };
		for (auto line = 0U; line < 30U; ++line) {
			text += "line " + std::to_string(line) + '\n';
		}
		auto laterText = text;
		laterText.replace(laterText.find("line 5"),
						  std::string{ "line 5" }.size(),
						  "changed line 5");
		laterText.replace(laterText.find("line 12"),
						  std::string{ "line 12" }.size(),
						  "changed line 12");
		const auto diff = computed_comparison(
			text, laterText, 0U, core::file_identity_of("src/file.cpp"));
		auto replies = std::vector<std::string>{ };
		window.set_review_reply_dispatch(
			[&replies](const std::string& aIdentity) { replies.push_back(aIdentity); });
		window.conversation_panel()->adopt(
			{ application::github_conversation_projection{
				  .Id       = "review-thread:late",
				  .Kind     = application::github_conversation_kind::review_thread,
				  .Path     = "src/file.cpp",
				  .Line     = 22U,
				  .DiffSide = application::github_conversation_diff_side::right,
				  .AnchorAvailability
				  = application::github_conversation_anchor_availability::current },
			  application::github_conversation_projection{
				  .Id          = "review-comment:late-1",
				  .ParentId    = "review-thread:late",
				  .Kind        = application::github_conversation_kind::review_comment,
				  .AuthorLogin = "someone",
				  .Body        = "a post",
				  .Path        = "src/file.cpp" },
			  application::github_conversation_projection{
				  .Id       = "review-thread:off-file",
				  .Kind     = application::github_conversation_kind::review_thread,
				  .Path     = "src/other.cpp",
				  .Line     = 22U,
				  .DiffSide = application::github_conversation_diff_side::right,
				  .AnchorAvailability
				  = application::github_conversation_anchor_availability::current },
			  application::github_conversation_projection{
				  .Id          = "review-comment:off-file-1",
				  .ParentId    = "review-thread:off-file",
				  .Kind        = application::github_conversation_kind::review_comment,
				  .AuthorLogin = "another-reviewer",
				  .Body        = "an off-file post",
				  .Path        = "src/other.cpp" } },
			QStringLiteral("two conversations"));
		static_cast<void>(publish_fixture(window, diff));
		process_events();
		expect(window.conversations_drawer()->width() == 375 && !window.conversation_panel()->discuss(),
			   "Review keeps the persisted conversation width and the Read arrangement");
		window.set_conversation_endpoint_placement(false, true);

		window.choose_modality(qt::review_modality::discuss);
		process_events();
		expect(window.current_modality() == qt::review_modality::discuss
				   && window.conversation_panel()->discuss()
				   && window.conversation_panel()->cards()->feed()
				   && window.findChild<QWidget*>(QStringLiteral("workbenchDiscussPage"))->isAncestorOf(window.conversation_panel())
				   && !window.conversations_drawer()->isVisible(),
			   "Discuss hosts the retained conversation panel in the main-content page");
		const auto codePosition = window.comparison_surface()->shared_scrollbar()->value();
		window.conversation_view()->setCurrentIndex(window.conversation_model()->index_for_id("review-thread:late"));
		process_events();
		expect(window.comparison_surface()->shared_scrollbar()->value() == codePosition,
			   "selecting PR-wide discussion does not navigate the retained code comparison");
		auto navigatedChanges = std::vector<core::change_navigation_direction>{ };
		window.set_change_dispatch(
			[&window, &navigatedChanges](const core::navigate_diff_change_intent aIntent) {
				if (window.comparison_surface()->navigate_change(
						aIntent.Direction, aIntent.Policy)) {
					navigatedChanges.push_back(aIntent.Direction);
				}
			});
		window.next_change_action()->trigger();
		window.previous_change_action()->trigger();
		process_events();
		expect(navigatedChanges
					   == std::vector{ core::change_navigation_direction::next,
									   core::change_navigation_direction::previous }
				   && window.current_modality() == qt::review_modality::discuss
				   && window.conversation_model()->id_for_index(
						  window.conversation_view()->currentIndex())
					   == "review-thread:late",
			   "previous and next change navigation beneath Discuss preserve the selected conversation");
		const auto& cards    = window.conversation_panel()->cards()->cards();
		const auto  sameFile = std::ranges::find(cards, std::string{ "review-thread:late" },
												 &qt::thread_card::Id);
		const auto  offFile  = std::ranges::find(cards, std::string{ "review-thread:off-file" },
												 &qt::thread_card::Id);
		expect(cards.size() == 2U && sameFile != cards.end() && offFile != cards.end()
				   && sameFile->Excerpt == QStringLiteral("line 20\nline 21\nline 22")
				   && offFile->Excerpt.isEmpty(),
			   "the PR-wide index carries code only for the currently compared file");
		const auto selectionBeforeReply = window.comparison_surface()->selection();
		const auto reply                = std::ranges::find(offFile->Actions, qt::card_action::reply,
															&std::pair<qt::card_action, QRect>::first);
		expect(reply != offFile->Actions.end(), "the off-file conversation still offers a reply gesture");
		const auto replyPoint = reply->second.center()
			- QPoint{ 0, window.conversation_panel()->cards()->verticalScrollBar()->value() };
		auto replyPress = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ replyPoint },
									   QPointF{ replyPoint }, Qt::LeftButton, Qt::LeftButton,
									   Qt::NoModifier };
		QApplication::sendEvent(window.conversation_panel()->cards()->viewport(), &replyPress);
		process_events();
		expect(replies.empty()
				   && window.comparison_surface()->selection() == selectionBeforeReply
				   && window.current_modality() == qt::review_modality::discuss
				   && window.conversation_panel()->notice_label()->text().contains(
					   QStringLiteral("Open this thread in code")),
			   "Reply refuses an off-file anchor without dispatching or changing the retained code selection");

		// [[BPR-1029]]. The reply's dispatch relays the feed out, which rebuilds
		// its cards, so the iterators taken before the press are spent. MSVC's
		// debug iterators abort on them; elsewhere they silently point at
		// whichever card was rebuilt in that slot.
		const auto sameFileAfterReply = std::ranges::find(cards, std::string{ "review-thread:late" },
														  &qt::thread_card::Id);
		expect(sameFileAfterReply != cards.end(), "the same-file conversation survives the refused reply");
		const auto open = std::ranges::find(sameFileAfterReply->Actions, qt::card_action::open_in_code,
											&std::pair<qt::card_action, QRect>::first);
		expect(open != sameFileAfterReply->Actions.end(), "the feed offers Open in code");
		const auto point = open->second.center()
			- QPoint{ 0, window.conversation_panel()->cards()->verticalScrollBar()->value() };
		auto press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ point }, QPointF{ point },
								  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(window.conversation_panel()->cards()->viewport(), &press);
		process_events();
		expect(window.current_modality() == qt::review_modality::read
				   && !window.conversation_panel()->discuss()
				   && window.conversation_model()->id_for_index(window.conversation_view()->currentIndex())
					   == "review-thread:late"
				   && window.comparison_surface()->conversation_anchor().has_value()
				   && window.comparison_surface()->conversation_anchor()->Side
					   == core::diff_side::right
				   && window.comparison_surface()->conversation_anchor()->Line == 21U,
			   "Open in code returns to Read with the thread current, so its line is revealed");
		expect(window.conversations_drawer()->width() == 375,
			   "leaving Discuss returns the conversation to its persisted width");
		window.choose_modality(qt::review_modality::discuss);
		process_events();
		expect(window.conversation_model()->id_for_index(window.conversation_view()->currentIndex()) == "review-thread:late",
			   "returning from code retains the selected discussion identity");
		window.hide();
	}

	// [[BPR-1046]]. Reply names its thread only once the thread's own line is
	// selected, which is what composes the draft that answers it. While that
	// file's comparison is still loading, the reply waits for it. A comparison
	// of any other file refuses it, and nothing is left waiting after that.
	void test_a_reply_waits_for_its_threads_own_comparison()
	{
		auto text = std::string{ };
		for (auto line = 0U; line < 30U; ++line) {
			text += "line " + std::to_string(line) + '\n';
		}
		auto laterText = text;
		laterText.replace(laterText.find("line 5"), std::string{ "line 5" }.size(), "changed line 5");
		const auto threadFile = computed_comparison(text, laterText, 0U, core::file_identity_of("src/file.cpp"));
		const auto otherFile  = computed_comparison(text, laterText, 0U, core::file_identity_of("src/other.cpp"));
		// As the composition does: it places the endpoints when it asks for a
		// comparison, and again once the comparison arrives.
		const auto request = [](qt::workbench_window& aWindow, const core::computed_file_diff& aDiff) {
			aWindow.set_conversation_endpoint_placement(false, true);
			aWindow.comparison_surface()->set_loading(request_context(aDiff));
			process_events();
		};
		const auto publish = [&request](qt::workbench_window& aWindow, const core::computed_file_diff& aDiff) {
			request(aWindow, aDiff);
			aWindow.comparison_surface()->publish(bpr::qt::test::prepare_publication({
				.Context = request_context(aDiff),
				.Result  = std::make_shared<const core::computed_file_diff>(aDiff),
			}));
			aWindow.set_conversation_endpoint_placement(false, true);
			process_events();
		};
		const auto arrive = [](qt::workbench_window& aWindow, const core::computed_file_diff& aDiff) {
			aWindow.comparison_surface()->publish(bpr::qt::test::prepare_publication({
				.Context = request_context(aDiff),
				.Result  = std::make_shared<const core::computed_file_diff>(aDiff),
			}));
			aWindow.set_conversation_endpoint_placement(false, true);
			process_events();
		};
		struct dispatched
		{
			std::string                             Thread;
			std::optional<core::semantic_selection> Selection;
		};
		const auto course = [&](const bool aThreadFileArrives) {
			qt::workbench_window window;
			window.resize(1'600, 820);
			window.show();
			process_events();
			auto opened    = every_primary_command_offered(core::change_navigation_policy::wrap);
			opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
										   core::workbench_drawer_state::expanded);
			window.publish(opened);
			process_events();
			auto replies = std::vector<dispatched>{ };
			window.set_review_reply_dispatch([&window, &replies](const std::string& aIdentity) {
				replies.push_back({ .Thread = aIdentity, .Selection = window.comparison_surface()->selection() });
			});
			window.conversation_panel()->adopt(
				{ application::github_conversation_projection{
					  .Id                 = "review-thread:late",
					  .Kind               = application::github_conversation_kind::review_thread,
					  .Path               = "src/file.cpp",
					  .Line               = 22U,
					  .DiffSide           = application::github_conversation_diff_side::right,
					  .AnchorAvailability = application::github_conversation_anchor_availability::current },
				  application::github_conversation_projection{
					  .Id          = "review-comment:late-1",
					  .ParentId    = "review-thread:late",
					  .Kind        = application::github_conversation_kind::review_comment,
					  .AuthorLogin = "someone",
					  .Body        = "a post",
					  .Path        = "src/file.cpp" } },
				QStringLiteral("one conversation"));
			// The thread's file has been asked for and has not arrived.
			request(window, threadFile);
			window.choose_modality(qt::review_modality::discuss);
			process_events();
			const auto& cards = window.conversation_panel()->cards()->cards();
			const auto  card  = std::ranges::find(cards, std::string{ "review-thread:late" }, &qt::thread_card::Id);
			expect(card != cards.end(), "the loading thread's card is in the feed");
			const auto reply = std::ranges::find(card->Actions, qt::card_action::reply,
												 &std::pair<qt::card_action, QRect>::first);
			expect(reply != card->Actions.end(), "the card offers Reply while its comparison loads");
			const auto point = reply->second.center()
				- QPoint{ 0, window.conversation_panel()->cards()->verticalScrollBar()->value() };
			auto press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ point }, QPointF{ point },
									  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			QApplication::sendEvent(window.conversation_panel()->cards()->viewport(), &press);
			process_events();
			expect(replies.empty()
					   && !window.conversation_panel()->notice_label()->text().contains(
						   QStringLiteral("Open this thread in code")),
				   "Reply waits while its thread's own comparison loads: nothing is named and nothing refused");
			if (aThreadFileArrives) {
				arrive(window, threadFile);
				expect(replies.size() == 1U && replies.front().Thread == "review-thread:late"
						   && replies.front().Selection.has_value()
						   && replies.front().Selection->PreferredLine.Side == core::diff_side::right
						   && replies.front().Selection->PreferredLine.Line == 21U,
					   "when the thread's comparison arrives its line is selected first, then the thread is named");
				expect(!window.conversation_panel()->notice_label()->text().contains(
						   QStringLiteral("Open this thread in code")),
					   "a reply that completes shows no refusal");
			} else {
				publish(window, otherFile);
				expect(replies.empty()
						   && window.conversation_panel()->notice_label()->text().contains(
							   QStringLiteral("Open this thread in code")),
					   "a comparison of another file refuses the waiting reply with the notice");
				publish(window, threadFile);
				expect(replies.empty(),
					   "a refused reply leaves nothing waiting: the thread's own comparison arriving later names nothing");
			}
			window.hide();
		};
		course(true);
		course(false);
	}

	/// \brief [[BPR-1134]]. RFC-BPR-0052 AC-09: the right tray's header holds
	/// Comments and Notes tabs, each tab's tools sit beneath the tabs, each
	/// tab shows only its own content, switching keeps each tab's scroll and
	/// selection, and keyboard and accessibility reach every tab.
	void test_the_right_tray_holds_comments_and_notes_tabs()
	{
		qt::workbench_window window;
		window.resize(1'280, 560);
		window.show();
		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::collapsed,
									   core::workbench_drawer_state::expanded);
		window.publish(opened);
		process_events();
		auto text = std::string{ };
		for (auto line = 0U; line < 40U; ++line) {
			text += "line " + std::to_string(line) + '\n';
		}
		window.set_conversation_endpoint_placement(false, true);
		const auto thread = [](const std::string& aId, const std::uint32_t aLine) {
			return application::github_conversation_projection{
				.Id                 = aId,
				.Kind               = application::github_conversation_kind::review_thread,
				.Path               = "src/file.cpp",
				.Line               = aLine,
				.DiffSide           = application::github_conversation_diff_side::right,
				.AnchorAvailability = application::github_conversation_anchor_availability::current,
			};
		};
		const auto post = [](const std::string& aId, const std::string& aParent) {
			return application::github_conversation_projection{
				.Id          = aId,
				.ParentId    = aParent,
				.Kind        = application::github_conversation_kind::review_comment,
				.AuthorLogin = "someone",
				.Body        = "a post",
				.Path        = "src/file.cpp",
			};
		};
		window.conversation_panel()->adopt(
			{ thread("review-thread:a", 12U), post("review-comment:a-1", "review-thread:a"),
			  thread("review-thread:b", 22U), post("review-comment:b-1", "review-thread:b") },
			QStringLiteral("two conversations"));
		static_cast<void>(publish_fixture(window, computed_comparison(text, text, 0U, core::file_identity_of("src/file.cpp"))));
		window.present_read_state({
			qt::thread_read_state{ .Thread = "review-thread:a", .UnreadPosts = { "review-comment:a-1" } },
			qt::thread_read_state{ .Thread = "review-thread:b", .UnreadPosts = { "review-comment:b-1" } },
		});
		process_events();

		auto* const tabs     = window.tray_tabs();
		auto* const comments = window.comments_tab_scroll();
		auto* const findings = window.findings_tab_scroll();
		auto* const drawer   = window.conversations_drawer();
		expect(tabs != nullptr && drawer->isAncestorOf(tabs) && tabs->count() == 3
				   && tabs->tabText(0) == QStringLiteral("Comments") && tabs->tabText(1) == QStringLiteral("Notes")
				   && tabs->tabText(2) == QStringLiteral("Tour") && tabs->accessibleTabName(2) == QStringLiteral("Tour")
				   && tabs->currentIndex() == 0 && tabs->isVisible(),
			   "the tray's header is Comments, Notes and Tour, and it opens on Comments");
		const auto privateHeader = [&] {
			return tabs->tabText(1) == QStringLiteral("Notes") && tabs->accessibleTabName(1) == QStringLiteral("Notes")
				&& tabs->tabToolTip(1) == QStringLiteral("Your private notes, AI conversations and review feedback. Nothing here is published; promote an item to a draft in Comments to publish it.")
				&& tabs->accessibleDescription().contains(QStringLiteral("Notes"))
				&& window.ai_tools_action()->text() == QStringLiteral("Notes");
		};
		expect(privateHeader(), "Notes labels, tooltip and accessible name state its private purpose");
		tabs->setTabText(1, QStringLiteral("Findings"));
		expect(!privateHeader(), "negative control: the obsolete Findings heading fails the Notes contract");
		tabs->setTabText(1, QStringLiteral("Notes"));
		auto* purpose = window.findChild<QLabel*>(QStringLiteral("workbenchNotesPurpose"));
		expect(purpose != nullptr && purpose->text() == QStringLiteral("Private to you and never published. Write a note or ask an agent about this pull request, then promote what you want to say to a draft in Comments."), "empty Notes explains privacy and promotion");
		window.agent_panel()->offer_profiles({ });
		window.private_notes()->present({ .Scope = 1U, .Available = true });
		expect(window.agent_panel()->profile_selector()->count() == 0
				   && window.agent_panel()->profile_selector()->placeholderText() == QStringLiteral("No agent configured")
				   && window.private_notes()->new_review_action()->isEnabled(),
			   "private notes stay usable when no agent is configured");
		expect(findings->isAncestorOf(window.private_notes()), "the private note editor belongs to Notes");
		expect(window.comments_count_text() == QStringLiteral("2")
				   && tabs->accessibleTabName(0).contains(QStringLiteral("2 threads with new comments")),
			   "the Comments tab carries the new-comment count and says it");
		const auto below = [&window, tabs](const QWidget* const aWidget) {
			return aWidget->mapTo(&window, QPoint{ 0, 0 }).y() >= tabs->mapTo(&window, QPoint{ 0, tabs->height() }).y();
		};
		expect(comments->isVisible() && !findings->isVisible() && window.conversation_panel()->isVisible()
				   && window.next_new_comment_button()->isVisible() && below(window.next_new_comment_button())
				   && window.next_new_comment_button()->defaultAction() == window.next_new_comment_action()
				   && !window.agent_panel()->isVisible() && !window.side_chat()->isVisible()
				   && !window.finding_queue()->isVisible() && window.defect_count_label()->isVisible(),
			   "the Comments tab shows the conversation and Next new beneath the tabs, and nothing of Notes");
		const auto save = [&window](const QString& aSuffix) {
			const auto rendered = window.grab();
			expect(!rendered.isNull(), "the right tray renders in the composed window");
			if (const auto path = qEnvironmentVariable("BPR_QT_RENDER_PATH"); !path.isEmpty()) {
				const QFileInfo file{ path };
				expect(rendered.save(QStringLiteral("%1/%2-%3.png").arg(file.path(), file.completeBaseName(), aSuffix), "PNG"),
					   "the right tray render is saved for inspection");
			}
		};
		save(QStringLiteral("tray-comments"));
		expect(comments->isAncestorOf(window.conversation_panel()) && findings->isAncestorOf(window.finding_queue())
				   && findings->isAncestorOf(window.side_chat()) && !findings->isAncestorOf(window.defect_card())
				   && comments->isAncestorOf(window.defect_count_label())
				   && findings->isAncestorOf(window.agent_panel()) && findings->findChild<QWidget*>(QStringLiteral("workbenchDifAiSlot")) == nullptr,
			   "the private tools stay in Notes while thread defect controls are outside it");

		auto* const view = window.conversation_view();
		view->setCurrentIndex(window.conversation_model()->index_for_id("review-thread:b"));
		process_events();
		const auto selected = view->currentIndex();
		tabs->setCurrentIndex(1);
		process_events();
		show_agent_tools(window);
		expect(findings->isVisible() && !comments->isVisible() && window.ai_tools_action()->isChecked()
				   && window.finding_queue()->isVisible() && !window.defect_count_label()->isVisible() && below(window.agent_panel())
				   && !window.conversation_panel()->isVisible(),
			   "the Notes tab shows only its own tools, beneath the tabs, and checks the AI tools action");
		// [[BPR-1242]]. The purpose sits above the Notes pages' tab widget, whose
		// frame is wider than the Agent tools column inside it.
		auto* const notesPages = window.findChild<QTabWidget*>(QStringLiteral("privateConversationTabs"));
		expect(purpose->isVisible() && notesPages != nullptr && purpose->width() <= notesPages->width()
				   && purpose->height() >= purpose->heightForWidth(purpose->width()),
			   "the private purpose wraps within the visible Notes page");
		save(QStringLiteral("tray-notes"));
		auto* const findingsBar = findings->verticalScrollBar();
		expect(findingsBar->maximum() > 0, "fixture: the Notes column is taller than the drawer");
		findingsBar->setValue(findingsBar->maximum() / 2);
		const auto findingsScrolled = findingsBar->value();
		tabs->setCurrentIndex(0);
		process_events();
		expect(view->currentIndex() == selected && comments->isVisible(),
			   "returning to Comments keeps the selected conversation");
		tabs->setCurrentIndex(1);
		process_events();
		expect(findingsBar->value() == findingsScrolled && findingsScrolled > 0,
			   "returning to Notes keeps its scroll position");

		// Keyboard: the tabs are a focus stop, and the arrow keys move between them.
		expect(tabs->focusPolicy() == Qt::StrongFocus && window.focus_order().contains(tabs)
				   && window.focus_order().contains(window.next_new_comment_button()),
			   "the tray's tabs and the Comments tab's tool are declared focus stops");
		tabs->setFocus(Qt::OtherFocusReason);
		for (const auto& [key, expected] : { std::pair{ Qt::Key_Left, 0 }, std::pair{ Qt::Key_Right, 1 } }) {
			auto press = QKeyEvent{ QEvent::KeyPress, key, Qt::NoModifier };
			QApplication::sendEvent(tabs, &press);
			process_events();
			expect(tabs->currentIndex() == expected, "the arrow keys move between the tray's tabs");
		}
		auto* const accessible = QAccessible::queryAccessibleInterface(tabs);
		expect(accessible != nullptr && accessible->childCount() >= 2
				   && accessible->child(0)->text(QAccessible::Name).startsWith(QStringLiteral("Comments"))
				   && accessible->child(1)->text(QAccessible::Name) == QStringLiteral("Notes"),
			   "accessibility reaches each tab by name");

		// The AI tools action and the tab are one choice.
		window.ai_tools_action()->setChecked(false);
		process_events();
		expect(tabs->currentIndex() == 0 && comments->isVisible(), "unchecking the AI tools action returns to Comments");
		window.hide();
	}

	void test_read_state_reaches_the_gutter_and_the_strip()
	{
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		process_events();
		auto marked = std::vector<std::string>{ };
		window.set_conversation_read_dispatch(
			[&marked](const std::string& aThread) { marked.push_back(aThread); });
		auto text = std::string{ };
		for (auto line = 0U; line < 30U; ++line) {
			text += "line " + std::to_string(line) + '\n';
		}
		const auto diff = computed_comparison(
			text, text, 0U, core::file_identity_of("src/file.cpp"));
		window.set_conversation_endpoint_placement(false, true);
		auto* const strip = window.next_new_comment_action();
		// [[BPR-1129]]. The count rides on the title row's comments toggle.
		auto* const commentsToggle = qobject_cast<QToolButton*>(
			window.primary_toolbar()->widgetForAction(window.conversations_drawer_toggle_action()));
		const auto badge = [commentsToggle] {
			return commentsToggle->property(qt::strip_style::badge_property).toString();
		};
		expect(strip != nullptr && !strip->isEnabled() && commentsToggle != nullptr && badge().isEmpty()
				   && window.primary_toolbar()->widgetForAction(strip) == nullptr
				   && strip->shortcut() == QKeySequence{ QStringLiteral("Alt+N") },
			   "with nothing unread next new comment is disabled, keeps its key without a title-row presenter, and the comments toggle carries no count");
		const auto thread = [](const std::string& aId, const std::uint32_t aLine) {
			return application::github_conversation_projection{
				.Id       = aId,
				.Kind     = application::github_conversation_kind::review_thread,
				.Path     = "src/file.cpp",
				.Line     = aLine,
				.DiffSide = application::github_conversation_diff_side::right,
				.AnchorAvailability
				= application::github_conversation_anchor_availability::current,
			};
		};
		const auto post = [](const std::string& aId, const std::string& aParent) {
			return application::github_conversation_projection{
				.Id          = aId,
				.ParentId    = aParent,
				.Kind        = application::github_conversation_kind::review_comment,
				.AuthorLogin = "someone",
				.Body        = "a post",
				.Path        = "src/file.cpp",
			};
		};
		window.conversation_panel()->adopt(
			{ thread("review-thread:late", 22U), post("review-comment:late-1", "review-thread:late"),
			  thread("review-thread:early", 12U), post("review-comment:early-1", "review-thread:early"),
			  post("review-comment:early-2", "review-thread:early") },
			QStringLiteral("two conversations"));
		// The comparison's publication is the frame the cards and the gutter
		// marks follow, as it is when a pull request opens.
		static_cast<void>(publish_fixture(window, diff));
		process_events();
		auto* const cards = window.conversation_panel()->cards();
		expect(cards->marked_lines(core::diff_side::right) == std::vector<std::uint32_t>{ 11U, 21U },
			   "both threads mark their lines");
		const auto kinds = [&window] {
			auto found = std::vector<qt::diff_line_mark_kind>{ };
			for (const auto& mark : window.comparison_surface()->line_marks(core::diff_side::right)) {
				found.push_back(mark.Kind);
			}
			return found;
		};
		expect(kinds() == std::vector{ qt::diff_line_mark_kind::thread, qt::diff_line_mark_kind::thread },
			   "before any read state both marks are amber");

		window.present_read_state({
			qt::thread_read_state{ .Thread = "review-thread:late", .UnreadPosts = { "review-comment:late-1" } },
			qt::thread_read_state{ .Thread = "review-thread:early", .UnreadPosts = { "review-comment:early-2" } },
		});
		process_events();
		expect(kinds() == std::vector{ qt::diff_line_mark_kind::thread, qt::diff_line_mark_kind::new_thread },
			   "the thread whose first post is unread is marked blue, the other stays amber");
		const auto countedWidth = commentsToggle->sizeHint().width();
		expect(strip->isEnabled() && badge() == QStringLiteral("2")
				   && commentsToggle->accessibleDescription().contains(QStringLiteral("2 threads with new comments")),
			   "the comments toggle carries the count of unread threads and says it");

		strip->trigger();
		process_events();
		expect(cards->current_thread() == "review-thread:early"
				   && window.conversation_model()->id_for_index(window.conversation_view()->currentIndex())
					   == "review-thread:early",
			   "next new comment steps to the first unread thread in file order and selects it");
		strip->trigger();
		process_events();
		expect(cards->current_thread() == "review-thread:late", "and then to the next");
		strip->trigger();
		process_events();
		expect(cards->current_thread() == "review-thread:early", "and wraps to the first");

		const auto late = std::ranges::find(cards->cards(), "review-thread:late", &qt::thread_card::Id);
		expect(late != cards->cards().end() && late->Actions.size() == 2U
				   && late->Actions.back().first == qt::card_action::mark_read,
			   "the unread card offers Mark read");
		const auto point = late->Actions.back().second.center() - QPoint{ 0, cards->verticalScrollBar()->value() };
		auto       press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ point }, QPointF{ point },
										Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(cards->viewport(), &press);
		expect(marked == std::vector<std::string>{ "review-thread:late" },
			   "Mark read leaves through the conversation read dispatch with the thread");

		window.present_read_state({ });
		process_events();
		expect(!strip->isEnabled() && badge().isEmpty() && commentsToggle->sizeHint().width() < countedWidth
				   && kinds() == std::vector{ qt::diff_line_mark_kind::thread, qt::diff_line_mark_kind::thread },
			   "an empty read state withdraws the count, its room on the toggle, and the blue marks");
		window.hide();
	}

	void test_defect_queue_navigation_and_semantic_dispatch()
	{
		qt::workbench_window window;
		auto                 commands = std::vector<core::defect_command>{ };
		auto                 classifications
			= std::vector<qt::workbench_defect_classification>{ };
		auto text = std::string{ };
		for (auto line = 0U; line < 30U; ++line) {
			text += "line " + std::to_string(line) + '\n';
		}
		const auto diff = computed_comparison(text, text, 0U, core::file_identity_of("review.cpp"));
		static_cast<void>(publish_fixture(window, diff));
		window.set_conversation_endpoint_placement(false, true);
		window.set_defect_dispatch(
			[&commands](core::defect_command aCommand) {
				commands.push_back(std::move(aCommand));
			});
		window.set_defect_classification_dispatch(
			[&classifications](qt::workbench_defect_classification aClassification) {
				classifications.push_back(std::move(aClassification));
			});
		window.conversation_model()->apply({
			application::github_conversation_projection{
				.Id       = "review-thread:thread-2",
				.Kind     = application::github_conversation_kind::review_thread,
				.Path     = "review.cpp",
				.Line     = 22U,
				.DiffSide = application::github_conversation_diff_side::right,
				.AnchorAvailability
				= application::github_conversation_anchor_availability::current,
			},
			application::github_conversation_projection{
				.Id       = "review-thread:left-thread",
				.Kind     = application::github_conversation_kind::review_thread,
				.Path     = "review.cpp",
				.Line     = 12U,
				.DiffSide = application::github_conversation_diff_side::left,
				.AnchorAvailability
				= application::github_conversation_anchor_availability::current,
			},
			application::github_conversation_projection{
				.Id   = "review-thread:file-thread",
				.Kind = application::github_conversation_kind::review_thread,
				.Path = "review.cpp",
			},
			application::github_conversation_projection{
				.Id   = "issue-comment:comment-1",
				.Kind = application::github_conversation_kind::issue_comment,
			},
		});
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id(
				"review-thread:thread-2"));
		window.classify_defect_action()->trigger();
		expect(
			classifications
				== std::vector<qt::workbench_defect_classification>{
					{
						.Conversation = "review-thread:thread-2",
						.Anchor       = {
							.Comparison = diff.Alignment.Digest,
							.Side       = core::diff_side::right,
							.Line       = 21U,
						},
					},
				},
			"classification emits the exact selected identity, side, comparison, "
			"and zero-based line");
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id(
				"review-thread:left-thread"));
		expect(!window.classify_defect_action()->isEnabled(),
			   "a current LEFT thread cannot classify when the selected comparison "
			   "does not expose GitHub's exact merge-base revision");
		window.classify_defect_action()->trigger();
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id(
				"review-thread:file-thread"));
		expect(!window.classify_defect_action()->isEnabled(),
			   "a file-level thread cannot fabricate a source-line defect anchor");
		window.classify_defect_action()->trigger();
		window.conversation_view()->setCurrentIndex(
			window.conversation_model()->index_for_id(
				"issue-comment:comment-1"));
		expect(!window.classify_defect_action()->isEnabled(),
			   "a non-thread conversation cannot enter local defect classification");
		window.classify_defect_action()->trigger();
		expect(classifications.size() == 1U,
			   "unplaceable anchors and unsupported conversation kinds emit no "
			   "classification intent");

		window.present_defects({
			defect_history(2U, core::defect_anchor_availability::outdated),
			defect_history(1U, core::defect_anchor_availability::available),
		});
		expect(
			window.defect_count() == 2U
				&& window.defect_count_label()->text().contains(QStringLiteral("1/2"))
				&& window.defect_card()->shown_defect()->Id == core::defect_id{ 1U },
			"the queue is stable-ID ordered and exposes an exact count");
		window.next_defect_action()->trigger();
		expect(
			window.defect_card()->shown_defect()->Id == core::defect_id{ 2U }
				&& window.defect_card()->context_label()->text().contains(
					QStringLiteral("unavailable")),
			"navigation reaches the next retained history without losing context status");
		window.defect_card()->verify_action()->trigger();
		expect(
			commands.size() == 1U
				&& commands.front().Id == core::defect_id{ 2U }
				&& commands.front().Kind == core::defect_command_kind::verify,
			"the visible card forwards one explicit lifecycle intent");
		expect(
			window.defect_card()->shown_defect()->State
				== core::defect_state::needs_verification,
			"the window does not verify before a new accepted projection");
	}

	void test_thread_defect_menus_badges_and_navigation()
	{
		qt::workbench_window window;
		window.resize(1280, 900);
		window.show();
		auto opened    = both_files_enabled();
		opened.Drawers = drawer_states(core::workbench_drawer_state::collapsed, core::workbench_drawer_state::expanded);
		window.publish(opened);
		auto text = std::string{ };
		for (auto line = 0U; line < 30U; ++line) {
			text += "line " + std::to_string(line) + '\n';
		}
		const auto diff = computed_comparison(text, text, 0U, core::file_identity_of("src/file.cpp"));
		static_cast<void>(publish_fixture(window, diff));
		window.set_conversation_endpoint_placement(false, true);
		auto forest = std::vector<application::github_conversation_projection>{ };
		for (auto index = 1U; index <= 3U; ++index) {
			forest.push_back({ .Id = "review-thread:" + std::to_string(index), .Kind = application::github_conversation_kind::review_thread, .Path = "src/file.cpp", .Line = index * 5U, .DiffSide = application::github_conversation_diff_side::right, .AnchorAvailability = application::github_conversation_anchor_availability::current });
		}
		window.conversation_panel()->adopt(forest, QStringLiteral("three threads"));
		std::vector<core::defect_command>                commands;
		std::vector<qt::workbench_defect_classification> classifications;
		window.set_defect_dispatch([&](auto aCommand) { commands.push_back(aCommand); });
		window.set_defect_classification_dispatch([&](auto aCommand) { classifications.push_back(aCommand); });
		auto first                = core::defect_history{ .Defect = { .Id = core::defect_id{ 1U }, .Anchor = { .Comparison = diff.Alignment.Digest, .Side = core::diff_side::right, .Line = 4U } } };
		first.OriginalAnchor      = first.Defect.Anchor;
		auto second               = first;
		second.Defect.Id          = core::defect_id{ 2U };
		second.Defect.Anchor.Line = 9U;
		second.OriginalAnchor     = second.Defect.Anchor;
		second.Defect.State       = core::defect_state::needs_verification;
		window.present_defects({ first, second }, { { core::defect_id{ 1U }, "review-thread:1" }, { core::defect_id{ 2U }, "review-thread:2" } });
		process_events();
		auto*      cards       = window.conversation_panel()->cards();
		auto*      findings    = window.findings_tab_scroll();
		const auto privateOnly = [&] {
			if (findings->isAncestorOf(window.defect_card()) || findings->isAncestorOf(window.defect_count_label())) {
				return false;
			}
			for (auto* button : findings->findChildren<QToolButton*>()) {
				if (button->defaultAction() == window.classify_defect_action() || button->defaultAction() == window.previous_defect_action() || button->defaultAction() == window.next_defect_action()) {
					return false;
				}
			}
			return true;
		};
		expect(privateOnly(), "the private tab has no thread defect controls");
		auto* wrong = new QToolButton{ findings->widget() };
		wrong->setDefaultAction(window.classify_defect_action());
		expect(!privateOnly(), "negative control detects a thread control returned to the private tab");
		delete wrong;
		auto* menu   = cards->thread_menu("review-thread:1");
		auto* verify = menu->findChild<QAction*>(QStringLiteral("defectVerify"));
		expect(verify && !verify->isEnabled() && !verify->toolTip().isEmpty(), "an open defect refuses Verify with an explanation");
		delete menu;
		menu   = cards->thread_menu("review-thread:2");
		verify = menu->findChild<QAction*>(QStringLiteral("defectVerify"));
		expect(verify && verify->isEnabled(), "the defect needing verification offers its existing verdict");
		window.next_defect_action()->trigger();
		window.previous_defect_action()->trigger();
		expect(cards->current_thread() == "review-thread:1", "a later navigation really changes the selected defect thread");
		verify->trigger();
		expect(commands.size() == 1U && commands.front().Id == core::defect_id{ 2U } && commands.front().Kind == core::defect_command_kind::verify,
			   "a menu keeps its exact defect identity across a later selection change");
		delete menu;
		menu           = cards->thread_menu("review-thread:3");
		auto* classify = menu->findChild<QAction*>(QStringLiteral("threadClassifyDefect"));
		expect(classify && classify->isEnabled(), "the unclassified current thread offers local classification");
		classify->trigger();
		expect(classifications.size() == 1U && classifications.front().Conversation == "review-thread:3" && classifications.front().Anchor.Line == 14U,
			   "classification carries the exact menu thread and semantic line");
		delete menu;
		window.next_defect_action()->trigger();
		expect(cards->current_thread() == "review-thread:2" && window.defect_count_label()->text().contains(QStringLiteral("2/2")), "defect navigation selects its thread and exact queue position");
		expect(window.comments_tab_scroll()->isAncestorOf(window.defect_count_label()) && window.focus_order().contains(window.findChild<QToolButton*>(QStringLiteral("nextDefectButton"))), "defect position and keyboard navigation belong to Comments");
		menu = cards->thread_menu("review-thread:1");
		menu->findChild<QAction*>(QStringLiteral("threadReclassifyDefect"))->trigger();
		process_events();
		expect(window.defect_card()->severity_selector()->isVisible() && window.defect_card()->category_selector()->isVisible(), "Reclassify opens the existing severity and category controls");
		window.findChild<QDialog*>(QStringLiteral("threadDefectDialog"))->hide();
		delete menu;
		process_events();
		save_render(window, QStringLiteral("thread-defect-controls"));
		cards->verticalScrollBar()->setValue(cards->verticalScrollBar()->maximum());
		process_events();
		save_render(window, QStringLiteral("thread-defect-and-unclassified"));
		window.hide();
	}

	void test_queue_rule_gestures_wait_for_the_accepted_projection()
	{
		qt::workbench_window window;
		auto                 intents = std::vector<application::queue_rule_intent>{ };
		window.resize(1'280, 820);
		window.show();
		process_events();

		auto* const addButton = window.findChild<QToolButton*>(
			QStringLiteral("addObserverRuleButton"));
		auto* const removeButton = window.findChild<QToolButton*>(
			QStringLiteral("removeObserverRuleButton"));
		expect(
			addButton != nullptr && removeButton != nullptr
				&& addButton != removeButton
				&& addButton->defaultAction() == window.add_queue_rule_action()
				&& removeButton->defaultAction()
					== window.remove_queue_rule_action(),
			"the named rule presenters retain the exact Add and Remove actions");
		expect(
			!addButton->accessibleName().isEmpty()
				&& !addButton->accessibleDescription().isEmpty()
				&& !removeButton->accessibleName().isEmpty()
				&& !removeButton->accessibleDescription().isEmpty(),
			"both retained rule presenters are explicitly named and described");
		// [[BPR-0652]]. The rule editor is Reviews and queue's: off Home, out
		// of the workbench's declared order, and its own row keeps Exclude,
		// Add, Remove in that order.
		const auto declared = window.focus_order();
		expect(
			!declared.contains(window.queue_rule_exclusion())
				&& !declared.contains(addButton) && !declared.contains(removeButton)
				&& window.rules_editor() != nullptr
				&& window.rules_editor()->isAncestorOf(window.queue_rule_exclusion())
				&& window.rules_editor()->isAncestorOf(addButton)
				&& window.rules_editor()->isAncestorOf(removeButton)
				&& !window.home_page()->isAncestorOf(addButton)
				&& window.rules_editor()->layout()->indexOf(window.queue_rule_exclusion())
					< window.rules_editor()->layout()->indexOf(addButton)
				&& window.rules_editor()->layout()->indexOf(addButton)
					< window.rules_editor()->layout()->indexOf(removeButton),
			"the rule presenters live on Reviews and queue, off the workbench's order, Exclude then Add then Remove");

		window.set_queue_rule_dispatch(
			[&intents](application::queue_rule_intent aIntent) {
				intents.push_back(std::move(aIntent));
			});
		window.publish({ .Status = application::queue_landing_status::ready });
		process_events();
		// [[BPR-0652]]. The presenters are Reviews and queue's; what the
		// workbench decides is the actions' availability.
		expect(
			!window.add_queue_rule_action()->isEnabled() && !window.remove_queue_rule_action()->isEnabled(),
			"blank input and no selected persisted rule leave both presenters unavailable");
		window.queue_rule_kind_selector()->setCurrentIndex(2);
		window.queue_rule_pattern_editor()->setText(QStringLiteral("src/parser"));
		window.queue_rule_exclusion()->setChecked(true);
		process_events();
		expect(
			window.add_queue_rule_action()->isEnabled() && !window.remove_queue_rule_action()->isEnabled(),
			"valid input restores Add while absent persisted selection keeps Remove unavailable");
		window.add_queue_rule_action()->trigger();
		const auto rule = core::observer_rule{
			.Kind      = core::observer_rule_kind::path,
			.Pattern   = "src/parser",
			.Excluding = true,
		};
		expect(
			intents == std::vector{ application::queue_rule_intent{ .Action = application::queue_rule_action::add, .Rule = rule } },
			"the real add action emits one exact semantic local-rule intent");
		expect(window.queue_rule_kind_selector()->count() == 3
				   && window.queue_landing()->rowCount() == 0,
			   "Qt performs no store or enrollment effect before projection");

		const auto membership = core::queue_membership{
			.MatchedRule = true,
			.Rule        = core::observer_rule{
				.Kind = core::observer_rule_kind::path, .Pattern = "src/parser" }
		};
		window.publish({
			.Status = application::queue_landing_status::ready,
			.Rules  = { rule },
			.Items  = { core::queue_item{
				.Ref = core::pull_request_ref{
					.Host = "github.com", .Account = "reviewer", .Repository = "octo/app", .Number = 7 },
				.Membership = membership,
				.Bucket     = core::attention_bucket::quiet,
			} },
		});
		process_events();
		expect(window.queue_landing()->rowCount() == 1,
			   "an accepted projection inserts the matched review");
		expect(
			window.add_queue_rule_action()->isEnabled() && window.remove_queue_rule_action()->isEnabled(),
			"accepted prerequisites restore both presenters");
		window.queue_rule_pattern_editor()->clear();
		process_events();
		expect(
			!window.add_queue_rule_action()->isEnabled() && window.remove_queue_rule_action()->isEnabled(),
			"blank input disables only Add while accepted selection retains Remove");
		window.queue_rule_pattern_editor()->setText(QStringLiteral("src/parser"));
		process_events();
		expect(
			window.add_queue_rule_action()->isEnabled() && window.remove_queue_rule_action()->isEnabled(),
			"restoring valid input restores Add");
		window.remove_queue_rule_action()->trigger();
		expect(intents.size() == 2U
				   && intents.back() == application::queue_rule_intent{ .Action = application::queue_rule_action::remove, .Rule = rule },
			   "the real remove action emits the exact selected persisted rule");
		expect(window.queue_landing()->rowCount() == 1,
			   "the row remains until the host accepts removal");
		window.publish({ .Status = application::queue_landing_status::ready });
		process_events();
		expect(window.queue_landing()->rowCount() == 0,
			   "the accepted cut-match projection removes the row");
		expect(
			window.add_queue_rule_action()->isEnabled() && !window.remove_queue_rule_action()->isEnabled(),
			"removing the accepted selection disables only Remove");
		window.hide();
	}

	void test_queue_rule_selection_survives_unrelated_projection()
	{
		qt::workbench_window window;
		auto                 intents = std::vector<application::queue_rule_intent>{ };
		window.set_queue_rule_dispatch(
			[&intents](application::queue_rule_intent aIntent) {
				intents.push_back(std::move(aIntent));
			});
		const auto first = core::observer_rule{
			.Kind = core::observer_rule_kind::repository, .Pattern = "octo/app"
		};
		const auto second = core::observer_rule{
			.Kind = core::observer_rule_kind::author, .Pattern = "octocat"
		};
		const auto projection = application::queue_landing_projection{
			.Status = application::queue_landing_status::ready,
			.Rules  = { first, second },
		};
		window.publish(projection);
		auto* const selector = window.findChild<QComboBox*>(
			QStringLiteral("observerRuleSelector"));
		expect(selector != nullptr && selector->count() == 2,
			   "two exact saved rules are visible");
		selector->setCurrentIndex(1);
		window.publish(projection);
		expect(selector->currentIndex() == 1,
			   "an unrelated projection preserves the exact selected rule");
		window.remove_queue_rule_action()->trigger();
		expect(
			intents == std::vector{ application::queue_rule_intent{
				.Action = application::queue_rule_action::remove,
				.Rule   = second,
			} },
			"Remove still targets the rule the reviewer selected");
	}

	// [[BPR-0659]]. The saved-repositories editor on Reviews and queue: Save
	// needs an account the queue session names and an owner/name, Remove the
	// selected saved repository; both leave as intents, and the tree lists
	// what the projection carries.
	// [[BPR-0661]]. The Reviews & queue preference rows: a row dispatches the
	// whole record, the page follows what the host publishes (so a refused
	// change reverts), and each group's Reset returns only its own rows.
	void test_review_behaviour_rows_dispatch_follow_the_projection_and_reset_by_group()
	{
		qt::workbench_window window;
		auto                 records = std::vector<application::review_behaviour_settings>{ };
		window.set_behaviour_dispatch([&records](const application::review_behaviour_settings& aSettings) {
			records.push_back(aSettings);
		});
		auto* const editor = window.behaviour_editor();
		expect(editor != nullptr
				   && window.settings()->page_widget(qt::settings_page::reviews_and_queue)->isAncestorOf(editor)
				   && !window.home_page()->isAncestorOf(editor),
			   "the preference rows live on Reviews and queue, off Home");
		const auto button = [&window](const char* const aName) {
			auto* const found = window.findChild<QToolButton*>(QString::fromLatin1(aName));
			expect(found != nullptr, "every preference option is a named button");
			return found;
		};
		expect(button("openModalityOverview")->isChecked() && button("continueModalityRead")->isChecked()
				   && button("markReadByHand")->isChecked() && button("returnComparisonWholePr")->isChecked()
				   && button("commentsFilterAll")->isChecked(),
			   "the rows start at the defaults, the behaviour the window had");
		button("openModalityRead")->click();
		expect(records.size() == 1U && records.back().OpenModality == application::review_open_modality::read
				   && records.back().ContinueModality == application::review_continue_modality::read,
			   "choosing an option dispatches the whole record with that one change");

		auto published             = application::review_behaviour_settings{ };
		published.ContinueModality = application::review_continue_modality::review;
		published.CommentsFilter   = application::review_comments_filter::unread;
		auto projection            = application::workbench_projection{ };
		projection.Behaviour       = published;
		window.publish(projection);
		expect(records.size() == 1U && window.behaviour() == published
				   && button("openModalityOverview")->isChecked() && button("continueModalityReview")->isChecked()
				   && button("commentsFilterUnread")->isChecked(),
			   "the page follows what the host publishes without dispatching again, so a refused change reverts");

		button("resetReadingBehaviour")->click();
		expect(records.size() == 2U
				   && records.back() == application::review_behaviour_settings{ .ContinueModality = application::review_continue_modality::review },
			   "Reset on the reading group returns its rows to the defaults and leaves the modality rules alone");
		button("resetModalityRules")->click();
		expect(records.size() == 3U
				   && records.back() == application::review_behaviour_settings{ .CommentsFilter = application::review_comments_filter::unread },
			   "Reset on the modality group returns its rows and leaves the reading rows as shown");
		// [[BPR-0721]]. The drawers group: one row, off by default, its own
		// Reset.
		expect(button("drawerControlsHidden")->isChecked() && !button("drawerControlsShown")->isChecked(),
			   "the drawer controls start hidden, the owner's direction");
		button("drawerControlsShown")->click();
		expect(records.size() == 4U
				   && records.back().DrawerControls == application::review_drawer_controls::shown
				   && records.back().CommentsFilter == application::review_comments_filter::unread,
			   "choosing Shown dispatches the whole record with the drawer controls shown");
		auto shown           = published;
		shown.DrawerControls = application::review_drawer_controls::shown;
		projection.Behaviour = shown;
		window.publish(projection);
		button("resetDrawers")->click();
		expect(records.size() == 5U && records.back() == published,
			   "Reset on the drawers group hides the controls again and leaves the other groups alone");
	}

	// [[BPR-0721]]. With the drawer controls hidden, the default, a collapsed
	// drawer leaves the splitter entirely and an expanded one has no Hide
	// button; with them shown, the rail is the 18-pixel column and nothing
	// of the heading row stays above it.
	void test_hidden_drawer_controls_leave_no_column_and_no_hide_button()
	{
		qt::workbench_window window;
		auto                 opened = both_files_enabled();
		opened.Drawers              = drawer_states(core::workbench_drawer_state::expanded,
													core::workbench_drawer_state::expanded);
		opened.Layout               = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 260U,
		};
		window.resize(1'280, 720);
		window.show();
		process_events();
		window.publish(opened);
		process_events();
		auto* const files    = window.files_drawer();
		auto* const comments = window.conversations_drawer();
		auto* const heading  = files->findChild<QLabel*>(QStringLiteral("workbenchFilesDrawerHeading"));
		expect(heading != nullptr && heading->isVisible() && !files->collapse_button()->isVisible()
				   && !files->controls_shown(),
			   "an expanded drawer keeps its heading and, by default, has no Hide button");

		auto collapsed    = application::workbench_projection{ };
		collapsed.Drawers = drawer_states(core::workbench_drawer_state::collapsed,
										  core::workbench_drawer_state::collapsed);
		window.publish(collapsed);
		process_events();
		const auto sizes = window.central_splitter()->sizes();
		expect(!files->isVisible() && !comments->isVisible() && sizes.size() == 4 && sizes[1] == 0
				   && sizes[3] == 0,
			   "with the controls hidden a collapsed drawer occupies zero width in the splitter");
		expect(window.comparison_surface()->width()
				   == window.central_splitter()->width()
					   - window.repositories_drawer()->width() - window.central_splitter()->handleWidth(),
			   "and no handle is left for it: the centre reaches the window's edge");
		expect(!window.files_drawer_toggle_action()->isChecked()
				   && window.files_drawer_toggle_action()->isEnabled(),
			   "the strip's toggle remains the way back");

		auto shown      = application::workbench_projection{ };
		shown.Behaviour = drawer_controls_shown();
		window.publish(shown);
		process_events();
		expect(files->isVisible() && files->width() == qt::workbench_drawer_rail::width_pixels
				   && files->rail()->isVisible() && !heading->isVisible()
				   && !files->collapse_button()->isVisible(),
			   "with the controls shown the collapsed drawer is exactly its rail, with nothing above it");
		window.publish(opened);
		process_events();
		expect(files->isVisible() && files->width() == 320 && heading->isVisible()
				   && files->collapse_button()->isVisible() && !files->rail()->isVisible(),
			   "expanded with the controls shown, the Hide button is back beside the heading");

		auto hidden      = application::workbench_projection{ };
		hidden.Behaviour = application::review_behaviour_settings{ };
		window.publish(hidden);
		process_events();
		expect(files->width() == 320 && !files->collapse_button()->isVisible() && heading->isVisible(),
			   "hiding the controls again takes only the Hide button from an expanded drawer");
		window.hide();
	}

	// [[BPR-0661]]. Continue review enters the modality the preference names,
	// and a pull request opened from Home opens in the preferred one: Read, or
	// the modality last used while a pull request was open.
	void test_continue_review_and_opening_honour_the_modality_preferences()
	{
		intent_log           log;
		auto                 opened = std::size_t{ 0U };
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) { log.Workbench.push_back(aIntent); },
			[&log](core::navigate_diff_change_intent aIntent) { log.Change.push_back(aIntent); },
			{ },
			{ },
			[&opened](application::github_intent) { ++opened; }
		};
		window.set_setup_destination_dispatch([&](application::setup_destination_intent aIntent) {
			if (std::holds_alternative<application::submit_setup_destination>(aIntent)) {
				++opened;
				window.present_setup_destination({ .Pending = std::nullopt, .Notice = { }, .Opened = true });
			}
		});

		window.resize(1'280, 820);
		window.show();
		process_events();
		auto files  = application::workbench_projection{ };
		files.Files = std::vector<application::workbench_file_projection>{
			{ .File = workbench_file(1U), .Path = "src/a.cpp" },
		};
		window.publish(files);
		window.publish(every_primary_command_offered());
		auto overview        = application::pull_request_overview_projection{ };
		overview.Title       = "Fix the thing";
		overview.AuthorLogin = "author";
		overview.Files       = { application::overview_file_row{ .Path = "src/a.cpp", .Folder = "src", .Name = "a.cpp" } };
		overview.Progress    = { .FilesTotal = 1U };
		overview.Entry       = application::overview_entry_kind::continue_review;
		overview.EntryPath   = "src/a.cpp";
		window.publish(overview);
		process_events();

		auto behaviour      = application::workbench_projection{ };
		behaviour.Behaviour = application::review_behaviour_settings{
			.ContinueModality = application::review_continue_modality::review,
		};
		window.publish(behaviour);
		window.choose_modality(qt::review_modality::overview);
		process_events();
		window.overview_page()->entry_button()->click();
		process_events();
		expect(window.current_modality() == qt::review_modality::review,
			   "Continue review enters Review when the preference says so");

		const auto openFromHome = [&window, &opened] {
			const auto before = opened;
			window.home_open_bar()->setText(QStringLiteral("octo/app#7"));
			auto pressed = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
			QApplication::sendEvent(window.home_open_bar(), &pressed);
			process_events();
			expect(opened == before + 1U, "the home bar dispatches the open intent");
			window.publish(application::github_presentation_update{
				.PullRequest = application::github_pull_request_projection{
					.ReviewThreadCount      = 0U,
					.ReviewThreadsAvailable = true,
				},
			});
			process_events();
		};
		behaviour.Behaviour = application::review_behaviour_settings{ .OpenModality = application::review_open_modality::read };
		window.publish(behaviour);
		openFromHome();
		expect(window.current_modality() == qt::review_modality::read,
			   "a pull request opened from Home opens in Read when the preference says so");

		behaviour.Behaviour = application::review_behaviour_settings{ .OpenModality = application::review_open_modality::last_used };
		window.publish(behaviour);
		window.choose_modality(qt::review_modality::discuss);
		process_events();
		openFromHome();
		expect(window.current_modality() == qt::review_modality::discuss,
			   "with \"last used\" a pull request opens in the modality last chosen while one was open");
		window.hide();
	}

	// [[BPR-0661]]. On open the comments drawer starts on the preferred filter,
	// and the preferred comparison is asked for through the epoch control's
	// own action once the history offers it.
	void test_comments_filter_and_return_comparison_preferences_apply_on_open()
	{
		qt::workbench_window window;
		auto                 intents = std::vector<application::epoch_intent>{ };
		window.epoch_selectors()->set_dispatch([&intents](application::epoch_intent aIntent) {
			intents.push_back(std::move(aIntent));
		});
		auto behaviour      = application::workbench_projection{ };
		behaviour.Behaviour = application::review_behaviour_settings{
			.ReturnComparison = application::review_return_comparison::since_last_review,
			.CommentsFilter   = application::review_comments_filter::unresolved,
		};
		window.publish(behaviour);
		window.publish(application::github_presentation_update{
			.PullRequest = application::github_pull_request_projection{
				.ReviewThreadCount      = 0U,
				.ReviewThreadsAvailable = true,
			},
		});
		process_events();
		auto* const panel = window.conversation_panel();
		expect(panel->filter_unresolved_button()->isChecked() && !panel->filter_unread_button()->isChecked(),
			   "the comments drawer opens on the preferred filter");
		expect(intents.empty(), "the comparison waits until the history offers a selection");

		auto offered       = retained_epochs({ });
		offered.Transition = application::epoch_transition_projection{ .Commands = enabled_epoch_commands() };
		window.publish(offered);
		process_events();
		const auto named = std::ranges::find_if(intents, [](const application::epoch_intent& aIntent) {
			const auto* const alias = std::get_if<core::select_named_comparison_intent>(&aIntent);
			return alias != nullptr && alias->Alias == core::named_comparison::my_last_reviewed_to_latest;
		});
		expect(named != intents.end(), "once offered, the preferred named comparison is asked for exactly as the action asks");
		const auto asked = intents.size();
		window.publish(offered);
		process_events();
		expect(intents.size() == asked, "a later epoch publish does not ask again");
		panel->filter_unresolved_button()->setChecked(false);
		panel->filter_unread_button()->setChecked(true);
		window.choose_home(true);
		window.publish(application::github_presentation_update{
			.PullRequest              = application::github_pull_request_projection{ .ReviewThreadsAvailable = true },
			.PreserveWindowNavigation = true,
		});
		process_events();
		expect(window.showing_home() && !panel->filter_unresolved_button()->isChecked()
				   && panel->filter_unread_button()->isChecked() && intents.size() == asked,
			   "a sibling server observation preserves Home, user filters and the chosen comparison");
	}

	// [[BPR-0661]]. With "when fully seen", reaching the end of the shared
	// scrollbar dispatches the file's own mark-read command once; by hand,
	// never.
	void test_mark_read_when_fully_seen_dispatches_once_at_the_end_of_the_comparison()
	{
		const auto [leftText, rightText] = comparison_fixture_text();
		const auto           diff        = computed_comparison(leftText, rightText);
		intent_log           log;
		qt::workbench_window window;
		window.set_receipt_dispatch([&log](core::file_receipt_command aCommand) { log.Receipt.push_back(std::move(aCommand)); });
		window.resize(1'280, 1'040);
		window.show();
		process_events();
		auto* const surface = publish_fixture(window, diff);
		const auto  receipt = core::file_receipt{
			.File     = core::file_identity_of("review.cpp"),
			.Material = file_material(73U),
		};
		window.publish(application::project_file_receipt_actions(receipt));
		process_events();
		expect(window.read_action()->isEnabled() && surface->shared_scrollbar()->maximum() > 0,
			   "the file can be marked read and the comparison has travel");
		auto behaviour      = application::workbench_projection{ };
		behaviour.Behaviour = application::review_behaviour_settings{
			.MarkRead = application::review_mark_read_behaviour::when_fully_seen,
		};
		window.publish(behaviour);
		process_events();
		expect(log.Receipt.empty(), "a comparison not yet scrolled to its end marks nothing");
		auto* const shared = surface->shared_scrollbar();
		shared->setValue(shared->maximum());
		process_events();
		expect(log.Receipt == std::vector{ core::file_receipt_command{
				   .File     = receipt.File,
				   .Material = receipt.Material,
				   .Kind     = core::file_receipt_command_kind::mark_read,
			   } },
			   "reaching the end dispatches the file's own mark-read command");
		shared->setValue(0);
		shared->setValue(shared->maximum());
		process_events();
		expect(log.Receipt.size() == 1U, "the same file and material is marked once");
		behaviour.Behaviour = application::review_behaviour_settings{ };
		window.publish(behaviour);
		auto another     = receipt;
		another.Material = file_material(74U);
		window.publish(application::project_file_receipt_actions(another));
		shared->setValue(0);
		shared->setValue(shared->maximum());
		process_events();
		expect(log.Receipt.size() == 1U, "by hand, reaching the end marks nothing");
		window.hide();
	}

	/// \brief [[BPR-0774]]. Names every button shown under a widget that cannot
	/// be pressed; empty when none.
	[[nodiscard]] auto disabled_buttons_under(const QWidget& aRoot) -> std::string
	{
		auto named = std::string{ };
		for (const auto* const button : aRoot.findChildren<QAbstractButton*>()) {
			if (button->isVisibleTo(&aRoot) && !button->isEnabled()) {
				named += " [" + button->objectName().toStdString() + ": \"" + button->text().toStdString() + "\"]";
			}
		}
		return named;
	}

	/// \brief Takes the step an event loop would take after a dialog closed.
	///
	/// \details A course runs outside any event loop, so `processEvents` alone
	/// never deletes a `WA_DeleteOnClose` dialog closed by a button; the
	/// deferred deletion is delivered here explicitly.
	void settle_closed_dialogs()
	{
		QApplication::processEvents();
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	}

	/// \brief [[BPR-0774]]. GitHub.com and one GitHub Enterprise Server host,
	/// both over native HTTPS.
	[[nodiscard]] auto enterprise_profiles() -> std::vector<application::github_profile_option>
	{
		return {
			{
				.Profile   = core::host_profile_id{ 51U },
				.Label     = "GitHub.com — Native HTTPS",
				.Hostname  = "github.com",
				.Product   = application::github_profile_product::github_com,
				.Transport = application::github_profile_transport::native_https,
			},
			{
				.Profile   = core::host_profile_id{ 52U },
				.Label     = "GHE — Native HTTPS",
				.Hostname  = "ghe.example.com",
				.Product   = application::github_profile_product::github_enterprise_server,
				.Transport = application::github_profile_transport::native_https,
			},
		};
	}

	// [[BPR-0660]], [[BPR-0774]]. RFC-BPR-0032 A3: one bordered Add enterprise
	// server under the cards opens a card in its edit state (hostname,
	// account, Add and Cancel), and Add leaves as exactly the saved-server
	// intent the store accepts. It waits for the queue that keeps servers, and
	// a refused hostname is said beside the fields rather than disabling Add.
	void test_add_enterprise_server_opens_a_card_in_its_edit_state()
	{
		{
			qt::workbench_window unwired;
			unwired.publish(application::queue_landing_projection{ .Status = application::queue_landing_status::ready });
			auto* const unwiredPage = unwired.settings()->page_widget(qt::settings_page::accounts_and_hosts);
			expect(unwiredPage != nullptr && !unwired.add_enterprise_server_button()->isVisibleTo(unwiredPage)
					   && !unwired.enterprise_server_draft()->isVisibleTo(unwiredPage),
				   "a ready queue with nowhere to send a saved server offers no Add enterprise server and no draft");
		}

		qt::workbench_window window;
		auto                 intents = std::vector<application::saved_server_intent>{ };
		window.set_saved_server_dispatch([&intents](application::saved_server_intent aIntent) {
			intents.push_back(std::move(aIntent));
		});
		window.resize(1'280, 820);
		window.show();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		auto* const page     = window.settings()->page_widget(qt::settings_page::accounts_and_hosts);
		auto* const add      = window.add_enterprise_server_button();
		auto* const draft    = window.enterprise_server_draft();
		auto* const hostname = window.enterprise_server_hostname();
		auto* const account  = window.enterprise_server_account();
		auto* const confirm  = window.enterprise_server_add();
		auto* const cancel   = window.enterprise_server_cancel();
		auto* const refusal  = window.enterprise_server_refusal();
		expect(page != nullptr && add != nullptr && draft != nullptr && hostname != nullptr && account != nullptr
				   && confirm != nullptr && cancel != nullptr && refusal != nullptr,
			   "the window composes Add enterprise server and its draft card");
		expect(!add->isVisibleTo(page) && !draft->isVisibleTo(page),
			   "before a ready queue projection there is no Add enterprise server and no draft");

		window.publish(application::queue_landing_projection{ .Status = application::queue_landing_status::ready });
		QApplication::processEvents();
		expect(add->isVisible() && add->text() == QStringLiteral("Add enterprise server")
				   && !add->isDefault() && !add->isFlat() && !draft->isVisibleTo(page),
			   "with the queue ready, one bordered Add enterprise server is offered and no draft is open");
		expect(page->isAncestorOf(add) && page->isAncestorOf(draft) && !window.home_page()->isAncestorOf(add)
				   && !window.home_page()->isAncestorOf(draft)
				   && !window.focus_order().contains(add) && !window.focus_order().contains(hostname)
				   && !window.focus_order().contains(account) && !window.focus_order().contains(confirm)
				   && !window.focus_order().contains(cancel),
			   "Add enterprise server and its draft live on Accounts and hosts, off Home and out of the workbench's order");

		const auto open = [add, draft, page] {
			add->click();
			QApplication::processEvents();
			return draft->isVisibleTo(page) && !add->isVisibleTo(page);
		};
		expect(open(), "Add enterprise server opens the draft card in place of the button");
		expect(hostname->isVisibleTo(draft) && account->isVisibleTo(draft) && confirm->isVisibleTo(draft)
				   && cancel->isVisibleTo(draft) && !refusal->isVisibleTo(draft)
				   && confirm->text() == QStringLiteral("Add") && cancel->text() == QStringLiteral("Cancel")
				   && window.settings()->focusWidget() == hostname,
			   "the draft is hostname, account, Add and Cancel with no refusal, and the hostname has focus");

		hostname->setText(QStringLiteral("ghe.cancelled.example"));
		account->setText(QStringLiteral("reviewer"));
		cancel->click();
		QApplication::processEvents();
		expect(!draft->isVisibleTo(page) && add->isVisibleTo(page) && hostname->text().isEmpty()
				   && account->text().isEmpty() && intents.empty(),
			   "Cancel closes the draft, clears its fields and dispatches nothing");

		expect(open(), "Add enterprise server opens the draft again");
		hostname->setText(QStringLiteral("https://ghe.example.com"));
		account->setText(QStringLiteral("reviewer"));
		const auto disabled = disabled_buttons_under(*draft);
		expect(disabled.empty(), "no button shown in the draft is disabled:" + disabled);
		confirm->click();
		QApplication::processEvents();
		expect(intents.empty() && draft->isVisibleTo(page) && refusal->isVisibleTo(draft)
				   && !refusal->text().isEmpty() && confirm->isEnabled()
				   && hostname->text() == QStringLiteral("https://ghe.example.com")
				   && account->text() == QStringLiteral("reviewer"),
			   "Add with a hostname that has a scheme is refused in a sentence beside the fields, dispatches nothing and keeps what was typed");
		auto typed = QKeyEvent{ QEvent::KeyPress, Qt::Key_Slash, Qt::NoModifier, QStringLiteral("/") };
		QApplication::sendEvent(hostname, &typed);
		expect(!refusal->isVisibleTo(draft) && hostname->text() == QStringLiteral("https://ghe.example.com/"),
			   "editing the hostname takes the refusal away");
		auto hostnameReturn = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(hostname, &hostnameReturn);
		QApplication::processEvents();
		expect(intents.empty() && refusal->isVisibleTo(draft) && draft->isVisibleTo(page),
			   "Return in the hostname field is the same Add, and is refused the same way");

		hostname->setText(QStringLiteral(" GHE.Example.Org "));
		account->setText(QStringLiteral(" reviewer "));
		confirm->click();
		QApplication::processEvents();
		expect(intents
				   == std::vector{ application::saved_server_intent{
					   .Action = application::saved_server_action::add,
					   .Server = { .Hostname = "ghe.example.org", .Account = "reviewer" } } },
			   "Add dispatches exactly one add intent, with the hostname trimmed and lowercased and the account trimmed");
		expect(!draft->isVisibleTo(page) && add->isVisibleTo(page) && hostname->text().isEmpty()
				   && account->text().isEmpty() && !refusal->isVisibleTo(draft),
			   "once the add has left, the draft closes and clears");

		expect(open(), "Add enterprise server opens the draft a third time");
		hostname->setText(QStringLiteral("ghe2.example.org"));
		auto accountReturn = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(account, &accountReturn);
		QApplication::processEvents();
		expect(intents.size() == 2U
				   && intents.back()
					   == application::saved_server_intent{
						   .Action = application::saved_server_action::add,
						   .Server = { .Hostname = "ghe2.example.org", .Account = "" } }
				   && !draft->isVisibleTo(page),
			   "Return in the account field adds too, with no account when none was typed");

		expect(open(), "Add enterprise server opens the draft once more");
		hostname->setText(QStringLiteral("ghe3.example.org"));
		window.publish(application::queue_landing_projection{ .Status = application::queue_landing_status::restoring });
		QApplication::processEvents();
		expect(!draft->isVisibleTo(page) && !add->isVisibleTo(page) && hostname->text().isEmpty() && intents.size() == 2U,
			   "a queue that is no longer ready closes and clears the draft and offers no Add enterprise server");
		window.publish(application::queue_landing_projection{ .Status = application::queue_landing_status::ready });
		QApplication::processEvents();
		expect(add->isVisibleTo(page) && !draft->isVisibleTo(page) && intents.size() == 2U,
			   "ready again, Add enterprise server returns with the draft still closed");
		window.settings()->hide();
		window.hide();
	}

	// [[BPR-0909]]. CLI discovery is a capability-owned action, separate from
	// the explicit enterprise-server editor. Presentation reports its bounded
	// asynchronous states and accepts a discovered CLI-only GHES card.
	void test_github_cli_discovery_is_a_separate_accounts_path()
	{
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		auto* const page   = window.settings()->page_widget(qt::settings_page::accounts_and_hosts);
		auto* const useCli = window.use_github_cli_button();
		auto* const status = window.github_cli_discovery_status();
		expect(page != nullptr && useCli != nullptr && status != nullptr
				   && !useCli->isVisibleTo(page)
				   && !window.home_landing()->secondary_button()->isVisible(),
			   "without a composed CLI capability neither Accounts nor Home offers discovery");

		auto discoveries = std::size_t{ 0U };
		window.set_github_cli_discovery_dispatch([&discoveries] { ++discoveries; });
		QApplication::processEvents();
		expect(useCli->isVisibleTo(page) && useCli->isEnabled()
				   && window.home_landing()->secondary_button()->isVisible(),
			   "the composed CLI capability adds one explicit discovery action to Accounts and Home");
		useCli->click();
		expect(discoveries == 1U, "one Use GitHub CLI press dispatches one discovery");

		window.present_github_cli_discovery(qt::github_cli_discovery_state::discovering);
		QApplication::processEvents();
		expect(!useCli->isEnabled() && status->isVisible()
				   && status->text().contains(QStringLiteral("Checking")),
			   "discovery disables re-entry and reports its in-flight state");
		useCli->click();
		expect(discoveries == 1U, "an in-flight discovery cannot be dispatched twice");

		window.add_setup_profile({
			.Profile   = core::host_profile_id{ 91U },
			.Label     = "GHES 3.18 — GitHub CLI (reviewer, active in gh)",
			.Hostname  = "ghe.example.com",
			.Product   = application::github_profile_product::github_enterprise_server,
			.Transport = application::github_profile_transport::github_cli,
		});
		window.present_cli_account_choices({ core::host_profile_id{ 91U } });
		window.present_github_cli_discovery(qt::github_cli_discovery_state::ready, 1U);
		QApplication::processEvents();
		auto* const discovered = window.server_card_for(core::host_profile_id{ 91U });
		expect(useCli->isEnabled() && status->text().contains(QStringLiteral("Found 1"))
				   && discovered != nullptr && discovered->hostname() == "ghe.example.com"
				   && discovered->cli_offer_button() != nullptr
				   && discovered->cli_offer_button()->text().contains(QStringLiteral("reviewer")),
			   "a discovered GHES identity becomes a selectable CLI-only server card naming the observed login");

		window.present_github_cli_discovery(qt::github_cli_discovery_state::empty);
		expect(status->text().contains(QStringLiteral("no active authenticated accounts")),
			   "an empty authenticated inventory is explicit and retryable");
		window.present_github_cli_discovery(
			qt::github_cli_discovery_state::unsupported_version);
		expect(status->text().contains(QStringLiteral("too old"))
				   && status->text().contains(QStringLiteral("gh 2.96 or newer")),
			   "an unsupported version requires an explicit upgrade from the compatibility floor");
		window.present_github_cli_discovery(qt::github_cli_discovery_state::process_failed);
		expect(status->text().contains(QStringLiteral("could not be run successfully")),
			   "a process failure is explicit and retryable");
		window.present_github_cli_discovery(
			qt::github_cli_discovery_state::malformed_response);
		expect(status->text().contains(QStringLiteral("could not safely read")),
			   "a malformed inventory is distinguished from process failure");
		window.settings()->hide();
		window.hide();
	}

	// [[BPR-0774]]. RFC-BPR-0032 A3: a kept enterprise server is its host's
	// card. Remember this server is a switch showing exactly the record the
	// queue keeps; Forget server and the switch turned off confirm first,
	// naming that record, and Cancel does nothing. A token session is labelled
	// session-only.
	void test_a_kept_enterprise_server_remembers_and_forgets_on_its_card()
	{
		auto                 otherEffects = std::size_t{ 0U };
		auto                 intents      = std::vector<application::saved_server_intent>{ };
		qt::workbench_window window{
			[&otherEffects](core::workbench_interaction_intent) { ++otherEffects; },
			[&otherEffects](core::navigate_diff_change_intent) { ++otherEffects; },
			[&otherEffects](core::workbench_drawer, std::uint32_t) { ++otherEffects; },
			enterprise_profiles(),
			[&otherEffects](application::github_intent) { ++otherEffects; },
		};
		window.set_device_authorization_dispatch([&otherEffects](application::device_authorization_gesture) { ++otherEffects; });
		window.set_public_preview_dispatch([&otherEffects](application::public_preview_gesture) { ++otherEffects; });
		window.set_setup_destination_dispatch([&otherEffects](application::setup_destination_intent) { ++otherEffects; });
		window.set_saved_repository_dispatch([&otherEffects](application::saved_repository_intent) { ++otherEffects; });
		window.set_queue_rule_dispatch([&otherEffects](application::queue_rule_intent) { ++otherEffects; });
		window.set_saved_server_dispatch([&intents](application::saved_server_intent aIntent) {
			intents.push_back(std::move(aIntent));
		});
		const auto kept         = application::saved_server{ .Hostname = "ghe.example.com", .Account = "reviewer" };
		auto       projection   = application::queue_landing_projection{ .Status = application::queue_landing_status::ready };
		projection.SavedServers = { kept };
		window.publish(projection);
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		// Only what a gesture sends counts; composing and publishing are not gestures.
		otherEffects = 0U;

		auto* const page       = window.settings()->page_widget(qt::settings_page::accounts_and_hosts);
		auto* const github     = window.server_card_for(core::host_profile_id{ 51U });
		auto* const enterprise = window.server_card_for(core::host_profile_id{ 52U });
		expect(page != nullptr && github != nullptr && enterprise != nullptr && github != enterprise
				   && enterprise->enterprise() && !github->enterprise()
				   && window.kept_servers() == std::vector{ kept },
			   "the window composes GitHub.com's card and the enterprise server's card, and keeps exactly the published record");
		auto* const toggle = enterprise->remember_switch();
		auto* const forget = enterprise->forget_button();
		expect(toggle->isVisibleTo(page) && toggle->isChecked() && toggle->isEnabled()
				   && forget->isVisibleTo(page) && forget->text() == QStringLiteral("Forget server")
				   && enterprise->remember_glyph()->isVisibleTo(page) && enterprise->forget_glyph()->isVisibleTo(page)
				   && enterprise->presentation().Remembered == std::optional{ kept },
			   "the enterprise card remembers the kept server: Remember this server is on and Forget server is shown");
		const auto rememberConsequence = QStringLiteral("Keeps the hostname and account name across launches. The token is still asked for.");
		const auto forgetConsequence   = QStringLiteral("Stops offering this server at the next start. Nothing on the server changes.");
		expect(enterprise->remember_glyph()->toolTip() == rememberConsequence
				   && enterprise->remember_glyph()->accessibleDescription() == rememberConsequence
				   && enterprise->forget_glyph()->toolTip() == forgetConsequence
				   && enterprise->forget_glyph()->accessibleDescription() == forgetConsequence,
			   "each row's info glyph says its consequence in its popover and as its accessible description");
		expect(!github->remember_switch()->isVisibleTo(page) && !github->forget_button()->isVisibleTo(page),
			   "GitHub.com's card has neither row");

		forget->click();
		QApplication::processEvents();
		auto* box = window.forget_server_confirmation();
		expect(box != nullptr && box->isVisible() && box->objectName() == QStringLiteral("forgetServerConfirmation")
				   && box->windowModality() == Qt::WindowModal && box->parentWidget() == window.settings()
				   && box->testAttribute(Qt::WA_DeleteOnClose)
				   && box->text() == QStringLiteral("Forget ghe.example.com?")
				   && box->informativeText().contains(QStringLiteral("ghe.example.com"))
				   && box->informativeText().contains(QStringLiteral("reviewer"))
				   && box->informativeText().contains(QStringLiteral("Nothing on the server changes"))
				   && box->informativeText().contains(QStringLiteral("no sign-in is removed"))
				   && intents.empty(),
			   "Forget server opens a window-modal confirmation naming the host and the account and what it does not touch, and dispatches nothing yet");
		auto* confirmButton = box->findChild<QAbstractButton*>(QStringLiteral("forgetServerConfirm"));
		auto* cancelButton  = box->findChild<QAbstractButton*>(QStringLiteral("forgetServerCancel"));
		expect(confirmButton != nullptr && confirmButton->text() == QStringLiteral("Forget server") && cancelButton != nullptr,
			   "the confirmation offers Forget server and Cancel");
		cancelButton->click();
		QApplication::processEvents();
		expect(intents.empty()
				   && (window.forget_server_confirmation() == nullptr || !window.forget_server_confirmation()->isVisible()),
			   "Cancel closes the confirmation and dispatches nothing");
		settle_closed_dialogs();
		expect(window.forget_server_confirmation() == nullptr && toggle->isChecked() && forget->isVisibleTo(page),
			   "the closed confirmation is gone, and the server is still remembered");

		toggle->click();
		QApplication::processEvents();
		box = window.forget_server_confirmation();
		expect(toggle->isChecked() && box != nullptr && box->isVisible()
				   && box->text() == QStringLiteral("Forget ghe.example.com?")
				   && box->informativeText().contains(QStringLiteral("reviewer")) && intents.empty(),
			   "switching Remember off returns the switch to the kept record at once and opens the same confirmation");
		confirmButton = box->findChild<QAbstractButton*>(QStringLiteral("forgetServerConfirm"));
		expect(confirmButton != nullptr, "the confirmation offers Forget server");
		confirmButton->click();
		QApplication::processEvents();
		expect(intents
				   == std::vector{ application::saved_server_intent{
					   .Action = application::saved_server_action::remove,
					   .Server = kept } },
			   "Forget server dispatches exactly one remove intent, for exactly the kept record");
		settle_closed_dialogs();
		expect(window.forget_server_confirmation() == nullptr && toggle->isChecked(),
			   "the confirmation is gone, and the switch shows the kept record until a projection says otherwise");
		expect(otherEffects == 0U, "Forget server, its confirmation and the switch reach no sink but the saved-server dispatch");

		projection.SavedServers.clear();
		window.publish(projection);
		QApplication::processEvents();
		otherEffects = 0U;
		expect(!toggle->isChecked() && toggle->isVisibleTo(page) && !forget->isVisibleTo(page)
				   && !enterprise->presentation().Remembered.has_value(),
			   "with no record kept, Remember this server is off and Forget server is gone");
		intents.clear();
		toggle->click();
		QApplication::processEvents();
		// Named: GCC 15.2 misreads an intent list temporary inside a && chain as dangling (BPR-0816).
		const auto accountlessAdd = std::vector{ application::saved_server_intent{
			.Action = application::saved_server_action::add,
			.Server = { .Hostname = "ghe.example.com", .Account = "" } } };
		expect(intents == accountlessAdd
				   && !toggle->isChecked() && window.forget_server_confirmation() == nullptr,
			   "switching Remember on dispatches exactly one add, with no account while none is signed in, and the switch waits for the projection");
		expect(otherEffects == 0U, "switching Remember on reaches no sink but the saved-server dispatch");
		projection.SavedServers = { application::saved_server{ .Hostname = "ghe.example.com", .Account = "" } };
		window.publish(projection);
		QApplication::processEvents();
		expect(toggle->isChecked() && forget->isVisibleTo(page),
			   "once a projection keeps the server, the switch is on and Forget server is back");

		// Signed in on the enterprise server with a token.
		projection.SavedServers.clear();
		window.publish(projection);
		const auto session = application::github_session_projection{
			.Profile     = core::host_profile_id{ 52U },
			.Host        = core::git_host_id{ 62U },
			.Account     = core::github_account_id{ 72U },
			.Login       = "enterprise-reviewer",
			.DisplayName = "Enterprise Reviewer",
			.Grant       = application::github_grant_projection{
				.Kind = application::github_grant_kind_projection::token,
			},
		};
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status   = core::github_status_delta{ .Status = core::github_status::session_ready },
				.Identity = core::github_identity_delta{
					.Profile    = session.Profile,
					.Host       = session.Host,
					.Account    = session.Account,
					.Generation = core::github_identity_generation{ 1U },
				},
			},
			.Session = session,
		});
		QApplication::processEvents();
		otherEffects = 0U;
		expect(enterprise->signed_in() && enterprise->login_label()->text() == QStringLiteral("enterprise-reviewer")
				   && enterprise->consequence_label()->isVisibleTo(page)
				   && enterprise->consequence_label()->text()
					   == QStringLiteral("Signed in with a token for this session only; it is asked for again after a restart.")
				   && !toggle->isChecked() && toggle->isVisibleTo(page),
			   "an enterprise server signed in with a token says the session is kept for this session only");
		intents.clear();
		toggle->click();
		QApplication::processEvents();
		// Named: GCC 15.2 misreads an intent list temporary inside a && chain as dangling (BPR-0816).
		const auto signedInAdd = std::vector{ application::saved_server_intent{
			.Action = application::saved_server_action::add,
			.Server = { .Hostname = "ghe.example.com", .Account = "enterprise-reviewer" } } };
		expect(intents == signedInAdd && !toggle->isChecked(),
			   "signed in on the enterprise server, switching Remember on keeps the hostname with the signed-in login");
		expect(otherEffects == 0U, "remembering a signed-in server reaches no sink but the saved-server dispatch");
		window.settings()->hide();
	}

	/// \brief [[BPR-0774]]. What the servers combo editor was, by object name and accessible name.
	constexpr std::array retired_server_editor_object_names{
		"savedServerSelector",
		"savedServerHostname",
		"savedServerAccount",
		"addSavedServerButton",
		"forgetSavedServerButton",
		"enterpriseServersEditor",
		"accountsServersHeading",
	};

	constexpr std::array retired_server_editor_accessible_names{
		"Remembered servers",
		"Server to remember",
		"Enterprise servers",
	};

	[[nodiscard]] auto names_one_of(const QString& aName, const std::span<const char* const> aNames) -> bool
	{
		return !aName.isEmpty() && std::ranges::any_of(aNames, [&aName](const char* const aCandidate) {
			return aName == QLatin1String{ aCandidate };
		});
	}

	/// \brief Walks an accessibility subtree the way an assistive reader does,
	/// keeping each node's accessible name and object name, one unit of `aBudget` per node.
	void collect_accessible_names(
		QAccessibleInterface* const               aInterface,
		std::vector<std::pair<QString, QString>>& aNodes,
		std::size_t&                              aBudget)
	{
		if (aInterface == nullptr || !aInterface->isValid() || aBudget == 0U) {
			return;
		}
		--aBudget;
		const auto* const object = aInterface->object();
		aNodes.emplace_back(aInterface->text(QAccessible::Name), object == nullptr ? QString{ } : object->objectName());
		for (auto index = 0; index < aInterface->childCount(); ++index) {
			collect_accessible_names(aInterface->child(index), aNodes, aBudget);
		}
	}

	/// \brief Fails when Accounts and hosts, its widget tree or its accessibility
	/// tree carries any piece of the retired servers combo editor or any combo box.
	void expect_no_server_combo_editor(
		const qt::workbench_window& aWindow,
		QWidget* const              aPage,
		const std::string_view      aState)
	{
		const auto state   = std::string{ aState };
		auto       objects = aPage->findChildren<QObject*>();
		objects.push_front(aPage);
		for (const auto* const object : std::as_const(objects)) {
			const auto* const widget = qobject_cast<const QWidget*>(object);
			expect(!names_one_of(object->objectName(), retired_server_editor_object_names)
					   && qobject_cast<const QComboBox*>(object) == nullptr
					   && (widget == nullptr || !names_one_of(widget->accessibleName(), retired_server_editor_accessible_names)),
				   "Accounts and hosts carries no piece of the servers combo editor and no combo box while " + state + ": "
					   + object->metaObject()->className() + " " + object->objectName().toStdString());
		}
		expect(aWindow.findChild<QAction*>(QStringLiteral("addSavedServer")) == nullptr
				   && aWindow.findChild<QAction*>(QStringLiteral("forgetSavedServer")) == nullptr,
			   "the window keeps no Remember server or Forget server action of the editor while " + state);
		auto nodes  = std::vector<std::pair<QString, QString>>{ };
		auto budget = std::size_t{ 20'000U };
		collect_accessible_names(QAccessible::queryAccessibleInterface(aPage), nodes, budget);
		expect(!nodes.empty() && budget > 0U,
			   "the accessibility walk of Accounts and hosts reaches the page and finishes while " + state);
		const auto retired = std::ranges::find_if(nodes, [](const std::pair<QString, QString>& aNode) {
			return names_one_of(aNode.first, retired_server_editor_accessible_names)
				|| names_one_of(aNode.second, retired_server_editor_object_names);
		});
		expect(retired == nodes.end(),
			   "the accessibility tree of Accounts and hosts names no piece of the servers combo editor while " + state
				   + (retired == nodes.end() ? std::string{ } : ": " + retired->first.toStdString() + " / " + retired->second.toStdString()));
	}

	// [[BPR-0774]]. RFC-BPR-0032 A3 and A1: the Enterprise servers heading and
	// its combo editor are gone from Accounts and hosts and from its
	// accessibility tree, before a queue projection, with servers kept, and
	// with the Add enterprise server draft open.
	void test_the_servers_combo_editor_is_gone()
	{
		qt::workbench_window window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			enterprise_profiles(),
			[](application::github_intent) { },
		};
		window.resize(1'280, 820);
		window.show();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		auto* const page = window.settings()->page_widget(qt::settings_page::accounts_and_hosts);
		expect(page != nullptr, "the Settings window has an Accounts and hosts page");
		expect_no_server_combo_editor(window, page, "no queue projection has arrived");

		window.set_saved_server_dispatch([](application::saved_server_intent) { });
		auto projection         = application::queue_landing_projection{ .Status = application::queue_landing_status::ready };
		projection.SavedServers = {
			application::saved_server{ .Hostname = "ghe.example.com", .Account = "reviewer" },
			application::saved_server{ .Hostname = "ghe.other.example", .Account = "" },
		};
		window.publish(projection);
		QApplication::processEvents();
		auto* const enterprise = window.server_card_for(core::host_profile_id{ 52U });
		expect(enterprise != nullptr && enterprise->remember_switch()->isVisibleTo(page)
				   && enterprise->remember_switch()->isChecked()
				   && window.add_enterprise_server_button()->isVisibleTo(page),
			   "the state under inspection has reached the page: the kept server's card remembers it and Add enterprise server is offered");
		expect_no_server_combo_editor(window, page, "the queue is ready and keeps two servers");

		window.add_enterprise_server_button()->click();
		QApplication::processEvents();
		expect(window.enterprise_server_draft()->isVisibleTo(page), "Add enterprise server opens its draft card");
		expect_no_server_combo_editor(window, page, "the Add enterprise server draft is open");
		window.settings()->hide();
		window.hide();
	}

	void test_saved_repository_gestures_round_trip_through_the_dispatch()
	{
		qt::workbench_window window;
		auto                 intents = std::vector<application::saved_repository_intent>{ };
		window.set_saved_repository_dispatch(
			[&intents](application::saved_repository_intent aIntent) {
				intents.push_back(std::move(aIntent));
			});
		auto* const editor = window.saved_repositories_editor();
		expect(editor != nullptr
				   && window.settings()->page_widget(qt::settings_page::reviews_and_queue)->isAncestorOf(editor)
				   && !window.home_page()->isAncestorOf(editor)
				   && !window.focus_order().contains(window.saved_repository_name()),
			   "the saved-repositories editor lives on Reviews and queue, off Home and out of the workbench's order");
		expect(!window.add_saved_repository_action()->isEnabled()
				   && !window.remove_saved_repository_action()->isEnabled(),
			   "before a ready projection neither Save nor Remove is offered");

		const auto kept = application::saved_repository{
			.Host = "github.com", .Account = "reviewer", .Repository = "octo/app"
		};
		auto projection = application::queue_landing_projection{
			.Status  = application::queue_landing_status::ready,
			.Session = application::queue_session_id{ .Host = "github.com", .Account = "reviewer", .Generation = 1U },
		};
		projection.SavedRepositories = { kept };
		window.publish(projection);
		auto* const tree = window.repositories_tree()->tree_model();
		expect(tree->rowCount() == 1 && tree->index(0, 0).data(qt::repositories_tree_model::saved_role).toBool(),
			   "the tree lists the saved repository the projection carries");
		auto* const selector = window.findChild<QComboBox*>(QStringLiteral("savedRepositorySelector"));
		expect(selector != nullptr && selector->count() == 1
				   && selector->itemText(0) == QStringLiteral("octo/app"),
			   "the editor lists the saved repository");
		expect(!window.add_saved_repository_action()->isEnabled(),
			   "Save waits for an owner/name");
		window.saved_repository_name()->setText(QStringLiteral("nope"));
		expect(!window.add_saved_repository_action()->isEnabled(),
			   "a name that is not owner/name is not offered");
		window.saved_repository_name()->setText(QStringLiteral(" octo/tools "));
		expect(window.add_saved_repository_action()->isEnabled(), "an owner/name is offered");
		window.add_saved_repository_action()->trigger();
		expect(intents == std::vector{ application::saved_repository_intent{ .Action = application::saved_repository_action::add, .Repository = { .Host = "github.com", .Account = "reviewer", .Repository = "octo/tools" } } },
			   "Save leaves as an add intent for the session's account, trimmed");
		selector->setCurrentIndex(0);
		expect(window.remove_saved_repository_action()->isEnabled(), "a selected saved repository can be removed");
		window.remove_saved_repository_action()->trigger();
		expect(intents.size() == 2U && intents.back().Action == application::saved_repository_action::remove
				   && intents.back().Repository == kept,
			   "Remove leaves as a remove intent for the selected saved repository");

		auto signedOut    = projection;
		signedOut.Session = application::queue_session_id{ };
		window.publish(signedOut);
		expect(!window.add_saved_repository_action()->isEnabled()
				   && !window.remove_saved_repository_action()->isEnabled(),
			   "without an account in the session there is nothing to save for");
	}

	void test_queue_filter_controls_are_accessible_incremental_and_focus_stable()
	{
		qt::workbench_window window;
		auto                 intents = std::vector<application::queue_preferences_intent>{ };
		window.set_queue_preferences_dispatch(
			[&intents](application::queue_preferences_intent aIntent) {
				intents.push_back(std::move(aIntent));
			});
		auto urgent = core::queue_item{
			.Ref = core::pull_request_ref{
				.Host = "github.com", .Account = "reviewer", .Repository = "z/repo", .Number = 9 },
			.Membership = core::queue_membership{ .Role = core::github_role::requested_reviewer },
			.Bucket     = core::attention_bucket::initial_review_requested,
		};
		auto quiet = core::queue_item{
			.Ref = core::pull_request_ref{
				.Host = "github.com", .Account = "reviewer", .Repository = "a/repo", .Number = 2 },
			.Membership = core::queue_membership{ .MatchedRule = true, .Rule = core::observer_rule{ .Kind = core::observer_rule_kind::path, .Pattern = "src" } },
			.Bucket     = core::attention_bucket::quiet,
		};
		window.publish({
			.Status = application::queue_landing_status::ready,
			.Items  = { urgent, quiet },
		});
		static_cast<void>(window.home_list()->select(urgent.Ref, false));
		window.queue_role_filter()->setFocus();
		const auto focused = QApplication::focusWidget();
		window.queue_role_filter()->setCurrentIndex(
			window.queue_role_filter()->findData(
				static_cast<int>(core::github_role::requested_reviewer)));
		expect(
			intents.size() == 1U
				&& intents.back().Preferences.Role
					== core::github_role::requested_reviewer,
			"the real role control emits one typed reviewer-preference intent");
		expect(window.queue_filter_model()->rowCount() == 2, "unaccepted filter intent leaves the projected queue intact");
		window.publish({ .Status = application::queue_landing_status::ready, .Items = { urgent, quiet }, .Preferences = intents.back().Preferences });
		expect(
			window.queue_filter_model()->rowCount() == 1,
			"the presentation filter changes without mutating the source queue");

		// [[BPR-0783]]. Home no longer filters by Bucket, so the filter that can
		// empty it is one of the four that survive: neither row is authored.
		window.queue_role_filter()->setCurrentIndex(
			window.queue_role_filter()->findData(static_cast<int>(core::github_role::author)));
		window.publish({ .Status = application::queue_landing_status::ready, .Items = { urgent, quiet }, .Preferences = intents.back().Preferences });
		expect(
			window.queue_filter_model()->rowCount() == 0
				&& window.queue_status_label()->text().contains(
					QStringLiteral("No reviews match")),
			"an empty filtered result explains active filters instead of claiming the queue is empty");

		window.publish({
			.Status      = application::queue_landing_status::ready,
			.Items       = { urgent, quiet },
			.Preferences = application::queue_presentation_preferences{
				.Sort = application::queue_sort_order::repository },
		});
		expect(
			window.queue_filter_model()->item_at(
										   window.queue_filter_model()->index(0, 0))
					->Ref
				== quiet.Ref,
			"an accepted explicit alternate sort is visible while source order stays core-owned");
		expect(
			QApplication::focusWidget() == focused,
			"an accepted filter/sort projection retains the focused control");

		// [[BPR-0783]]. The six typed controls are Settings' Queue defaults now:
		// still named, still explained, still the same controls, but stops of
		// another top-level window, which the workbench never declares.
		const auto  focusOrder = window.focus_order();
		auto* const defaults   = window.settings()->page_widget(qt::settings_page::reviews_and_queue);
		for (auto* const selector : { window.queue_role_filter(),
									  window.queue_observer_filter(),
									  window.queue_bucket_filter(),
									  window.queue_repository_filter(),
									  window.queue_quiet_filter(),
									  window.queue_sort_selector() }) {
			expect(
				!selector->accessibleName().isEmpty()
					&& !selector->accessibleDescription().isEmpty()
					&& !focusOrder.contains(selector)
					&& defaults != nullptr && defaults->isAncestorOf(selector)
					&& !window.home_page()->isAncestorOf(selector),
				"every queue preference control is named, explained, on Reviews & queue and off the workbench's order");
		}
	}

	void test_notification_activation_selects_only_an_exact_visible_queue_row()
	{
		qt::workbench_window window;
		auto                 intents = std::vector<application::queue_preferences_intent>{ };
		window.set_queue_preferences_dispatch(
			[&intents](application::queue_preferences_intent aIntent) {
				intents.push_back(std::move(aIntent));
			});
		const auto visible = core::queue_item{
			.Ref = core::pull_request_ref{
				.Host = "github.com", .Account = "reviewer", .Repository = "octo/app", .Number = 7 },
			.Membership = core::queue_membership{ .Role = core::github_role::requested_reviewer },
			.Bucket     = core::attention_bucket::initial_review_requested,
		};
		const auto hidden = core::queue_item{
			.Ref = core::pull_request_ref{
				.Host = "ghe.example", .Account = "other", .Repository = "octo/app", .Number = 7 },
			.Membership = core::queue_membership{ .Role = core::github_role::participant },
			.Bucket     = core::attention_bucket::new_comments_or_author_replies,
		};
		window.publish({
			.Status = application::queue_landing_status::ready,
			.Items  = { visible, hidden },
		});
		window.queue_role_filter()->setCurrentIndex(
			window.queue_role_filter()->findData(
				static_cast<int>(core::github_role::requested_reviewer)));
		window.publish({ .Status = application::queue_landing_status::ready, .Items = { visible, hidden }, .Preferences = intents.back().Preferences });
		const auto preferences = window.queue_filter_model()->preferences();
		const auto intentCount = intents.size();

		window.show();
		QApplication::processEvents();
		expect(
			window.select_queue_item(visible.Ref),
			"a notification activation selects an exact visible queue identity");
		const auto selected = window.selected_queue_item();
		expect(selected.has_value(), "activation makes a row current");
		expect(selected->Ref == visible.Ref,
			   message_for("activation selects the exact identity, not one with an equal repository and number",
						   (QString::fromStdString(selected->Ref.Host) + QStringLiteral(" ")
							+ QString::fromStdString(selected->Ref.Account))
							   .toStdString()));
		expect(
			QApplication::focusWidget() == window.home_list()->current_view(),
			message_for("activation focuses the real proxy row",
						QString{ QStringLiteral("focus=%1 current=%2") }
							.arg(QApplication::focusWidget() == nullptr
									 ? QStringLiteral("none")
									 : QApplication::focusWidget()->objectName(),
								 window.home_list()->current_view() == nullptr
									 ? QStringLiteral("none")
									 : window.home_list()->current_view()->objectName())
							.toStdString()));
		expect(
			window.queue_filter_model()->preferences() == preferences
				&& intents.size() == intentCount,
			"visible activation does not mutate queue preferences");

		expect(
			!window.select_queue_item(hidden.Ref)
				&& window.queue_status_label()->text().contains(
					QStringLiteral("hidden")),
			"a filtered target fails visibly instead of clearing the filter");
		expect(
			window.queue_filter_model()->preferences() == preferences
				&& intents.size() == intentCount,
			"hidden activation preserves every preference and emits no preference intent");

		const auto absent = core::pull_request_ref{
			.Host = "github.com", .Account = "reviewer", .Repository = "octo/app", .Number = 99
		};
		expect(
			!window.select_queue_item(absent)
				&& window.queue_status_label()->text().contains(
					QStringLiteral("no longer")),
			"an absent target fails visibly without inventing provider navigation");
		expect(
			window.queue_filter_model()->preferences() == preferences
				&& intents.size() == intentCount,
			"absent activation also leaves preferences untouched");
	}

	void test_overview_refuses_a_hand_opened_drawer_and_says_so()
	{
		// [[BPR-0727]]. Choosing Overview tells the core the modality before
		// its preset; a toggle in Overview still leaves as a gesture, the
		// core's refusal comes back as a published reason, and the window
		// shows it in the status bar while the toggle stays enabled and
		// unchecked. Read, Review and Discuss send their presets after their
		// modality, so their drawers open as before.
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			}
		};
		window.resize(1'280, 820);
		window.show();
		process_events();
		window.publish(every_primary_command_offered());
		process_events();

		log.Workbench.clear();
		window.choose_modality(qt::review_modality::overview);
		process_events();
		expect(log.Workbench.size() == 4U
				   && log.Workbench.front()
					   == core::workbench_interaction_intent{ core::set_workbench_modality_intent{
						   .Modality = core::workbench_modality::overview } }
				   && std::holds_alternative<core::set_workbench_drawer_intent>(log.Workbench[1]),
			   "choosing Overview dispatches the modality before its three drawer presets");
		auto collapsed    = application::workbench_projection{ };
		collapsed.Drawers = drawer_states(core::workbench_drawer_state::collapsed,
										  core::workbench_drawer_state::collapsed);
		window.publish(collapsed);
		process_events();

		auto* const files = window.files_drawer_toggle_action();
		expect(files->isEnabled() && !files->isChecked()
				   && files->toolTip().contains(QStringLiteral("Overview keeps every drawer collapsed")),
			   "in Overview the strip toggle stays enabled and says what the gesture will meet");
		log.Workbench.clear();
		files->trigger();
		process_events();
		expect(log.Workbench.size() == 1U
				   && std::holds_alternative<core::toggle_workbench_drawer_intent>(log.Workbench.front())
				   && !files->isChecked(),
			   "the toggle leaves as a gesture and the window shows the state the core kept");

		auto refused    = application::workbench_projection{ };
		refused.Refusal = core::workbench_reason_code::overview_keeps_drawers_collapsed;
		window.publish(refused);
		process_events();
		expect(window.last_refusal() == core::workbench_reason_code::overview_keeps_drawers_collapsed
				   && window.statusBar()->currentMessage()
					   == QStringLiteral("Overview keeps every drawer collapsed. Choose Read, Review "
										 "or Discuss to open one.")
				   && !files->isChecked()
				   && !window.conversations_drawer_toggle_action()->isChecked(),
			   "the published refusal is visible in the status bar and every drawer stays collapsed");

		log.Workbench.clear();
		window.choose_modality(qt::review_modality::read);
		process_events();
		const auto* const readModality        = log.Workbench.empty()
			? nullptr
			: std::get_if<core::set_workbench_modality_intent>(&log.Workbench.front());
		auto              conversationsOpened = false;
		for (const auto& intent : log.Workbench) {
			if (const auto* const set = std::get_if<core::set_workbench_drawer_intent>(&intent);
				set != nullptr && set->Drawer == core::workbench_drawer::conversations) {
				conversationsOpened = set->State == core::workbench_drawer_state::expanded;
			}
		}
		expect(readModality != nullptr && readModality->Modality == core::workbench_modality::read
				   && conversationsOpened,
			   "Read sends its modality first and then opens the conversations drawer");
		auto opened    = application::workbench_projection{ };
		opened.Drawers = drawer_states(core::workbench_drawer_state::collapsed,
									   core::workbench_drawer_state::expanded);
		window.publish(opened);
		process_events();
		expect(files->isEnabled() && !files->toolTip().contains(QStringLiteral("Overview")),
			   "outside Overview the toggle describes what it does again");
		window.hide();
	}

	void test_the_epoch_row_spans_the_window_in_every_modality()
	{
		// [[BPR-0737]]. The epoch line and the "Comparing" menu are one row
		// between the strip and the drawers, as wide as the window's client
		// area in every modality, and a drawer's state or width moves nothing
		// on it. Before this it was the centre pane's first widget and its
		// labels moved with every drawer.
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			[&log](core::navigate_diff_change_intent aIntent) {
				log.Change.push_back(aIntent);
			}
		};
		window.resize(1'280, 820);
		window.show();
		process_events();
		auto files  = application::workbench_projection{ };
		files.Files = std::vector<application::workbench_file_projection>{
			{ .File = workbench_file(1U), .Path = "src/a.cpp" },
			{ .File = workbench_file(2U), .Path = "src/b.cpp" },
		};
		window.publish(files);
		window.publish(every_primary_command_offered());
		process_events();

		auto* const epochs = window.epoch_selectors();
		auto* const bar    = window.review_bar();
		auto* const client = window.centralWidget();
		// [[BPR-1129]]. RFC-BPR-0052 AC-01: above the Updates line the review
		// surface holds the review bar and nothing else, directly under the
		// title row, including while loading. Readiness occupies the track
		// itself; no separate readiness or composition-entry row exists.
		const auto entriesBottom = [&window, epochs, bar, client] {
			const auto top      = epochs->mapTo(&window, QPoint{ 0, 0 }).y();
			const auto barFrame = QRect{ bar->mapTo(&window, QPoint{ 0, 0 }), bar->size() };
			if (!bar->isVisible() || barFrame.top() != client->mapTo(&window, QPoint{ 0, 0 }).y()) {
				return -1;
			}
			const auto bottom = barFrame.bottom() + 1;
			for (const auto* const row : epochs->parentWidget()->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
				const auto placed = QRect{ row->mapTo(&window, QPoint{ 0, 0 }), row->size() };
				if (row == epochs || row == bar || !row->isVisible() || placed.top() >= top) {
					continue;
				}
				return -1;
			}
			return bottom;
		};
		const auto spans = [&window, epochs, bar, client, &entriesBottom] {
			const auto row   = QRect{ epochs->mapTo(&window, QPoint{ 0, 0 }), epochs->size() };
			const auto area  = QRect{ client->mapTo(&window, QPoint{ 0, 0 }), client->size() };
			const auto top   = QRect{ bar->mapTo(&window, QPoint{ 0, 0 }), bar->size() };
			const auto panes = window.central_splitter()->mapTo(&window, QPoint{ 0, 0 });
			return epochs->isVisible() && row.left() == area.left() && row.width() == area.width()
				&& top.left() == area.left() && top.width() == area.width()
				&& row.top() == entriesBottom() && row.bottom() < panes.y();
		};
		const auto titleRow = window.primary_toolbar();
		expect(QRect{ titleRow->mapTo(&window, QPoint{ 0, 0 }), titleRow->size() }.bottom() + 1
				   == bar->mapTo(&window, QPoint{ 0, 0 }).y(),
			   "the review bar sits directly under the title row");
		expect(window.findChild<QToolButton*>(QStringLiteral("composeReviewActionButton")) == nullptr
				   && window.findChild<QToolButton*>(QStringLiteral("composeInlineDraftActionButton")) == nullptr,
			   "the composition entry row is gone");
		// [[BPR-1130]]. A new window starts with the Updates line folded: no
		// row at all between the bar and the panes but the readiness line.
		const auto barBottom = [&window, bar] { return QRect{ bar->mapTo(&window, QPoint{ 0, 0 }), bar->size() }.bottom() + 1; };
		const auto panesTop  = [&window] { return window.central_splitter()->mapTo(&window, QPoint{ 0, 0 }).y(); };
		const auto barFolded = QRect{ bar->mapTo(&window, QPoint{ 0, 0 }), bar->size() };
		expect(!epochs->isVisible() && !window.updates_line_open() && entriesBottom() == barBottom() && panesTop() == barBottom(),
			   "a new window starts with the Updates line folded, and the panes sit directly under the review bar");
		open_updates_line(window);
		const auto before = QRect{ epochs->mapTo(&window, QPoint{ 0, 0 }), epochs->size() };
		expect(spans() && before.width() == client->width() && before.width() > window.width() / 2,
			   "open, the Updates line is the first row below the review bar, and the bar and the line are as wide as the client area");
		expect(QRect{ bar->mapTo(&window, QPoint{ 0, 0 }), bar->size() } == barFolded
				   && before.height() == qt::epoch_line::height_pixels && panesTop() == before.bottom() + 1,
			   "opening the line moves the bar nowhere; the timeline is one 34-pixel row above the panes");

		for (const auto modality : { qt::review_modality::overview, qt::review_modality::read,
									 qt::review_modality::review, qt::review_modality::discuss }) {
			window.choose_modality(modality);
			process_events();
			const auto row = QRect{ epochs->mapTo(&window, QPoint{ 0, 0 }), epochs->size() };
			expect(spans() && row == before,
				   "the epoch row holds its place and width through modality "
					   + std::to_string(static_cast<int>(modality)));
		}
		window.choose_modality(qt::review_modality::review);

		auto opened    = application::workbench_projection{ };
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		window.publish(opened);
		process_events();
		auto closed            = application::workbench_projection{ };
		closed.Drawers         = drawer_states(core::workbench_drawer_state::collapsed,
											   core::workbench_drawer_state::collapsed);
		const auto withDrawers = QRect{ epochs->mapTo(&window, QPoint{ 0, 0 }), epochs->size() };
		window.publish(closed);
		process_events();
		const auto withoutDrawers = QRect{ epochs->mapTo(&window, QPoint{ 0, 0 }), epochs->size() };
		expect(spans() && withDrawers == before && withoutDrawers == before,
			   "opening and collapsing both drawers moves nothing on the epoch row");

		window.publish(opened);
		process_events();
		auto sizes = window.central_splitter()->sizes();
		expect(sizes.size() == 4 && sizes[1] > 0 && sizes[2] > 200, "the drawers have widths to move");
		sizes[1] += 120;
		sizes[2] -= 120;
		window.central_splitter()->setSizes(sizes);
		process_events();
		const auto resized = QRect{ epochs->mapTo(&window, QPoint{ 0, 0 }), epochs->size() };
		expect(spans() && resized == before,
			   "resizing a drawer moves nothing on the epoch row");
		fold_updates_line(window);
		expect(!epochs->isVisible() && QRect{ bar->mapTo(&window, QPoint{ 0, 0 }), bar->size() } == barFolded && panesTop() == barBottom(),
			   "folded again, the line leaves no row and the bar has not moved");
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{ .Status = core::github_status::loading_pull_request } } });
		// [[BPR-1200]]. A first read shows the workbench once it has shown
		// what it names; until then it is said on Home.
		window.publish(application::github_presentation_update{
			.LoadingOverview = application::github_loading_overview_projection{
				.Title = "Loading readiness", .Body = "", .AuthorLogin = "octocat", .BaseRef = "main", .HeadRef = "topic", .ReportedFiles = 1U, .ReportedCommits = 1U } });
		for (const auto modality : { qt::review_modality::overview, qt::review_modality::read,
									 qt::review_modality::review, qt::review_modality::discuss }) {
			window.choose_modality(modality);
			window.resize(window.minimumWidth(), 820);
			fold_updates_line(window);
			process_events();
			expect(window.findChild<QWidget*>(QStringLiteral("reviewReadiness")) == nullptr
					   && epochs->comparing_button()->text().contains(QStringLiteral("Loading"))
					   && epochs->comparing_button()->toolTip().contains(epochs->readiness_text())
					   && panesTop() == barBottom(),
				   "loading in every folded modality uses the review summary and adds no row");
			save_render(window, QStringLiteral("readiness-folded-floor-%1").arg(static_cast<int>(modality)));
			open_updates_line(window);
			process_events();
			auto* const readiness = window.findChild<QLabel*>(QStringLiteral("epochReadiness"));
			expect(spans() && readiness != nullptr && readiness->isVisible() && !epochs->line()->isVisible()
					   && readiness->text() == epochs->readiness_text()
					   && readiness->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard)
					   && epochs->height() == qt::epoch_line::height_pixels,
				   "loading occupies the existing 34-pixel track with copyable complete words");
			save_render(window, QStringLiteral("readiness-open-%1").arg(static_cast<int>(modality)));
			auto* const oldRow = new QWidget{ epochs->parentWidget() };
			oldRow->setObjectName(QStringLiteral("reviewReadiness"));
			oldRow->setFixedHeight(20);
			auto* const rows = qobject_cast<QVBoxLayout*>(epochs->parentWidget()->layout());
			expect(rows != nullptr, "the real review rows are inspectable");
			rows->insertWidget(1, oldRow);
			oldRow->show();
			process_events();
			expect(!spans(), "negative control: retaining the old readiness row fails the row contract");
			delete oldRow;
			process_events();
			expect(spans(), "removing the control restores the direct bar-to-track geometry");
		}
		window.hide();
	}

	void test_identical_overview_skips_rebuild_but_restores_cleared_files()
	{
		qt::workbench_window window;
		auto                 overview = application::pull_request_overview_projection{ .Title = "Overview" };
		for (auto index = 0; index < 515; ++index) {
			const auto name = "file-" + std::to_string(index) + ".cpp";
			overview.Files.push_back(application::overview_file_row{ .Path = "src/" + name, .Folder = "src", .Name = name });
		}
		// [[BPR-0934]]. The page's folder tree is retained: it projects by
		// inserting and removing rows and never resets, so any structural
		// signal counts as a page rebuild.
		auto        pageBuilds     = 0;
		auto        treeResets     = 0;
		auto* const pageModel      = window.overview_page()->file_model();
		const auto  countPageBuild = [&pageBuilds] { ++pageBuilds; };
		QObject::connect(pageModel, &QAbstractItemModel::modelReset, countPageBuild);
		QObject::connect(pageModel, &QAbstractItemModel::rowsInserted, countPageBuild);
		QObject::connect(pageModel, &QAbstractItemModel::rowsRemoved, countPageBuild);
		QObject::connect(pageModel, &QAbstractItemModel::rowsMoved, countPageBuild);
		QObject::connect(pageModel, &QAbstractItemModel::layoutChanged, countPageBuild);
		// [[BPR-0972]]. The drawer tree is reconciled too, so any structural
		// signal counts as a tree build.
		auto* const treeModel      = window.files_tree()->tree_model();
		const auto  countTreeBuild = [&treeResets] { ++treeResets; };
		QObject::connect(treeModel, &QAbstractItemModel::modelReset, countTreeBuild);
		QObject::connect(treeModel, &QAbstractItemModel::rowsInserted, countTreeBuild);
		QObject::connect(treeModel, &QAbstractItemModel::rowsRemoved, countTreeBuild);
		QObject::connect(treeModel, &QAbstractItemModel::rowsMoved, countTreeBuild);
		window.publish(overview);
		const auto firstPageBuilds = pageBuilds;
		const auto firstTreeResets = treeResets;
		expect(firstPageBuilds > 0 && firstTreeResets > 0, "initial overview projects both real file models");
		for (auto repeat = 0; repeat < 5; ++repeat) {
			window.publish(overview);
		}
		expect(pageBuilds == firstPageBuilds && treeResets == firstTreeResets,
			   "equal 515-file overviews rebuild neither page nor drawer tree");
		overview.Title                     = "Changed overview";
		overview.Files.front().ThreadCount = 7U;
		window.publish(overview);
		const auto firstFile = core::file_identity_of(overview.Files.front().Path);
		expect(window.overview_page()->title_label()->text() == QStringLiteral("Changed overview")
				   && window.files_tree()->tree_model()->index_for_file(firstFile).data(qt::files_tree_model::thread_count_role).toULongLong() == 7U,
			   "changed overview updates visible title and per-file facts");
		window.publish(application::workbench_projection{ .Files = std::vector<application::workbench_file_projection>{ } });
		expect(!window.files_tree()->tree_model()->index_for_file(firstFile).isValid(), "observation gap clears the drawer rows");
		const auto beforeFallbackPageBuilds = pageBuilds;
		const auto beforeFallbackTreeResets = treeResets;
		window.publish(overview);
		expect(pageBuilds == beforeFallbackPageBuilds && treeResets > beforeFallbackTreeResets
				   && window.files_tree()->tree_model()->index_for_file(firstFile).isValid()
				   && window.files_tree()->tree_model()->index_for_file(firstFile).data(qt::files_tree_model::thread_count_role).toULongLong() == 7U,
			   "equal overview restores cleared fallback rows and facts without rebuilding its unchanged page");
		window.clear_overview();
		const auto beforeReopen = pageBuilds;
		window.publish(overview);
		expect(pageBuilds > beforeReopen && window.overview_page()->presented(), "clear then identical reopen projects the overview again");
		window.publish(application::github_presentation_update{ .Transition = application::github_transition_projection{ .PullRequest = core::pull_request_summary_delta{ } } });
		const auto beforeAuthority = pageBuilds;
		window.publish(overview);
		expect(pageBuilds > beforeAuthority, "authority clear does not suppress the next PR's identical visible overview");
	}

	// [[BPR-0972]]. A receipt change reaches the presented overview as its
	// changed rows: one data change on the page table and one on the drawer
	// tree, no structural signal, and the complete overview it amounts to is
	// then recognised as already presented.
	void test_receipt_update_moves_only_its_rows()
	{
		qt::workbench_window window;
		auto                 overview = application::pull_request_overview_projection{ .Title = "Overview" };
		for (auto index = 0; index < 515; ++index) {
			const auto name = "file-" + std::to_string(index) + ".cpp";
			overview.Files.push_back(application::overview_file_row{ .Path = "src/" + name, .Folder = "src", .Name = name });
		}
		overview.Progress.FilesTotal = overview.Files.size();
		overview.EntryPath           = overview.Files.front().Path;
		window.publish(overview);
		auto*      pageModel     = window.overview_page()->file_model();
		auto*      treeModel     = window.files_tree()->tree_model();
		auto       structural    = 0;
		auto       pageChanges   = 0;
		auto       treeChanges   = 0;
		const auto countStructal = [&structural] { ++structural; };
		for (auto* const model : { static_cast<QAbstractItemModel*>(pageModel), static_cast<QAbstractItemModel*>(treeModel) }) {
			QObject::connect(model, &QAbstractItemModel::modelReset, countStructal);
			QObject::connect(model, &QAbstractItemModel::rowsInserted, countStructal);
			QObject::connect(model, &QAbstractItemModel::rowsRemoved, countStructal);
			QObject::connect(model, &QAbstractItemModel::rowsMoved, countStructal);
			QObject::connect(model, &QAbstractItemModel::layoutChanged, countStructal);
		}
		QObject::connect(pageModel, &QAbstractItemModel::dataChanged, [&pageChanges] { ++pageChanges; });
		QObject::connect(treeModel, &QAbstractItemModel::dataChanged, [&treeChanges] { ++treeChanges; });
		auto changed               = overview;
		changed.Files[7].Read      = core::file_read_state::read;
		changed.Progress.FilesRead = 1U;
		changed.Entry              = application::overview_entry_kind::continue_review;
		changed.EntryPath          = changed.Files.front().Path;
		const auto update          = application::overview_receipt_update{ .Rows = { changed.Files[7] }, .Progress = changed.Progress, .Entry = changed.Entry, .EntryPath = changed.EntryPath };
		window.publish(update);
		const auto file = core::file_identity_of(changed.Files[7].Path);
		expect(structural == 0 && pageChanges == 1 && treeChanges == 1, "one receipt signals one page row and one tree row");
		expect(treeModel->index_for_file(file).data(qt::files_tree_model::read_role).toInt() == static_cast<int>(core::file_read_state::read),
			   "the drawer tree presents the committed read state");
		expect(window.overview_page()->entry_button()->text() == QStringLiteral("Continue review")
				   && window.overview_page()->files_heading()->text().contains(QStringLiteral("514 not read")),
			   "the progress and the one action follow the receipt");
		window.publish(changed);
		expect(structural == 0 && pageChanges == 1 && treeChanges == 1, "the equivalent complete overview is already presented");
	}

	void test_overview_entry_preserves_start_and_continue_semantics()
	{
		// [[BPR-0647]]. The Overview modality shows the host-decided overview in
		// place of the comparison. Start review and Continue review keep distinct
		// modality semantics; a file row remains a plain Read gesture.
		intent_log           log;
		auto                 githubIntents = std::size_t{ 0U };
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			[&log](core::navigate_diff_change_intent aIntent) {
				log.Change.push_back(aIntent);
			},
			{ },
			{ },
			[&githubIntents](application::github_intent) { ++githubIntents; }
		};
		window.resize(1'280, 820);
		window.show();
		process_events();
		auto files  = application::workbench_projection{ };
		files.Files = std::vector<application::workbench_file_projection>{
			{ .File = workbench_file(1U), .Path = "src/a.cpp" },
			{ .File = workbench_file(2U), .Path = "src/b.cpp" },
		};
		window.publish(files);
		window.publish(every_primary_command_offered());
		// [[BPR-1130]]. The Updates line open, as the reviewer left it.
		open_updates_line(window);

		window.choose_modality(qt::review_modality::overview);
		process_events();
		expect(!window.showing_overview() && window.comparison_surface()->isVisible()
				   && !window.overview_page()->isVisible(),
			   "Overview before the host decided an overview keeps the comparison");

		auto overview        = application::pull_request_overview_projection{ };
		overview.Title       = "Fix the thing";
		overview.AuthorLogin = "author";
		overview.Files       = {
			application::overview_file_row{ .Path = "src/a.cpp", .Folder = "src", .Name = "a.cpp" },
			application::overview_file_row{ .Path = "src/b.cpp", .Folder = "src", .Name = "b.cpp" },
		};
		overview.Progress  = { .FilesTotal = 2U };
		overview.Entry     = application::overview_entry_kind::start_review;
		overview.EntryPath = "src/b.cpp";
		window.publish(overview);
		process_events();
		expect(window.showing_overview() && window.overview_page()->isVisible()
				   && !window.comparison_surface()->isVisible()
				   && window.epoch_selectors()->isVisible()
				   && window.overview_page()->title_label()->text() == QStringLiteral("Fix the thing"),
			   "once decided, Overview shows the page beneath the epoch line in place "
			   "of the comparison");

		window.choose_modality(qt::review_modality::review);
		process_events();
		expect(!window.showing_overview() && window.comparison_surface()->isVisible(),
			   "every other modality shows the comparison");
		window.choose_modality(qt::review_modality::overview);
		process_events();
		expect(window.showing_overview(), "Overview returns to the page");

		log.Workbench.clear();
		window.overview_page()->entry_button()->click();
		process_events();
		expect(!log.Workbench.empty()
				   && std::get_if<core::select_workbench_file_intent>(&log.Workbench.front()) != nullptr
				   && std::get<core::select_workbench_file_intent>(log.Workbench.front()).File
					   == workbench_file(2U),
			   "Start review selects the entry file through the core");
		expect(window.current_modality() == qt::review_modality::review,
			   "Start review enters Review even while Continue review defaults to Read");
		expect(!window.showing_overview() && window.comparison_surface()->isVisible(),
			   "Start review shows the comparison");

		overview.Entry = application::overview_entry_kind::continue_review;
		window.publish(overview);
		window.choose_modality(qt::review_modality::overview);
		log.Workbench.clear();
		window.overview_page()->entry_button()->click();
		process_events();
		expect(!log.Workbench.empty()
				   && std::get_if<core::select_workbench_file_intent>(&log.Workbench.front()) != nullptr
				   && std::get<core::select_workbench_file_intent>(log.Workbench.front()).File
					   == workbench_file(2U)
				   && window.current_modality() == qt::review_modality::read,
			   "Continue review keeps its intended entry file and default Read destination");

		window.choose_modality(qt::review_modality::overview);
		auto behaviour      = application::workbench_projection{ };
		behaviour.Behaviour = application::review_behaviour_settings{
			.ContinueModality = application::review_continue_modality::review,
		};
		window.publish(behaviour);
		log.Workbench.clear();
		const auto first = window.overview_page()->file_model()->index_for_path(QStringLiteral("src/a.cpp"));
		window.overview_page()->files_view()->activated(first);
		process_events();
		expect(!log.Workbench.empty()
				   && std::get_if<core::select_workbench_file_intent>(&log.Workbench.front()) != nullptr
				   && std::get<core::select_workbench_file_intent>(log.Workbench.front()).File
					   == workbench_file(1U)
				   && window.current_modality() == qt::review_modality::read,
			   "activating a file row selects that file and enters Read independently of Continue preference");
		expect(githubIntents == 0U,
			   "Start review, Continue review, and file-row navigation publish no GitHub intent");

		window.choose_modality(qt::review_modality::overview);
		window.clear_overview();
		process_events();
		expect(!window.showing_overview() && !window.overview_page()->presented(),
			   "clearing the overview returns Overview to the comparison");
		window.hide();
	}

	void test_repositories_drawer_toggles_through_the_core_and_collapses_to_a_rail()
	{
		// [[BPR-0651]]. The third drawer is decided as the other two are: its
		// toggle asks the core, the core's delta sets it, and collapsed it is
		// an 18-pixel rail that reopens it.
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) {
				log.Workbench.push_back(aIntent);
			},
			[&log](core::navigate_diff_change_intent aIntent) {
				log.Change.push_back(aIntent);
			}
		};
		window.resize(1'280, 820);
		window.show();
		process_events();
		window.publish(every_primary_command_offered());
		process_events();
		expect(window.repositories_drawer_toggle_action()->isEnabled()
				   && window.repositories_drawer_toggle_action()->isChecked()
				   && window.repositories_drawer()->state() == core::workbench_drawer_state::expanded,
			   "with the core offering the command the repositories toggle is enabled and checked");
		log.Workbench.clear();
		window.repositories_drawer_toggle_action()->trigger();
		process_events();
		expect(log.Workbench.size() == 1U
				   && std::get_if<core::toggle_workbench_drawer_intent>(&log.Workbench.back()) != nullptr
				   && std::get<core::toggle_workbench_drawer_intent>(log.Workbench.back()).Drawer
					   == core::workbench_drawer::repositories
				   && window.repositories_drawer_toggle_action()->isChecked(),
			   "the toggle asks the core for the repositories drawer and keeps the decided state");
		auto collapsed    = application::workbench_projection{ };
		collapsed.Drawers = core::workbench_drawer_delta{
			.Files         = core::workbench_drawer_state::expanded,
			.Conversations = core::workbench_drawer_state::expanded,
			.Repositories  = core::workbench_drawer_state::collapsed,
		};
		collapsed.Behaviour = drawer_controls_shown();
		window.publish(collapsed);
		process_events();
		expect(window.repositories_drawer()->state() == core::workbench_drawer_state::collapsed
				   && window.repositories_drawer()->width() == qt::workbench_drawer_rail::width_pixels
				   && window.repositories_drawer()->rail()->isVisible()
				   && !window.repositories_drawer_toggle_action()->isChecked()
				   && window.files_drawer()->state() == core::workbench_drawer_state::expanded,
			   "the core's delta collapses only the repositories drawer to its rail");
		log.Workbench.clear();
		window.repositories_drawer()->rail()->click();
		process_events();
		expect(log.Workbench.size() == 1U
				   && std::get<core::toggle_workbench_drawer_intent>(log.Workbench.back()).Drawer
					   == core::workbench_drawer::repositories,
			   "the rail asks the core to reopen its drawer");
		window.hide();
	}

	void test_notification_settings_wait_for_acceptance_and_reset_only_their_domain()
	{
		qt::workbench_window window;
		auto                 intents = std::vector<application::queue_preferences_intent>{ };
		window.set_queue_preferences_dispatch([&intents](application::queue_preferences_intent aIntent) { intents.push_back(std::move(aIntent)); });
		auto projection             = application::queue_landing_projection{ .Status = application::queue_landing_status::ready };
		projection.Preferences.Role = core::github_role::requested_reviewer;
		window.publish(projection);
		auto* const enabled = window.findChild<QCheckBox*>(QStringLiteral("notificationsEnabled"));
		auto* const paused  = window.findChild<QCheckBox*>(QStringLiteral("notificationsPaused"));
		expect(enabled && paused && enabled->isChecked() && !paused->isChecked(), "notification controls show the accepted defaults");
		paused->click();
		expect(intents.size() == 1U && intents.back().Preferences.Notifications.Paused && !paused->isChecked(), "pause leaves as a single typed intent and waits for acceptance");
		projection.Preferences = intents.back().Preferences;
		window.publish(projection);
		expect(paused->isChecked(), "accepted pause is projected");
		enabled->click();
		expect(enabled->isChecked() && !intents.back().Preferences.Notifications.Enabled, "enablement also waits for accepted storage");
		projection.Status = application::queue_landing_status::unavailable;
		window.publish(projection);
		expect(enabled->isChecked() && paused->isChecked(), "storage failure preserves both accepted values");
		projection.Status = application::queue_landing_status::ready;
		window.publish(projection);
		for (auto* button : window.findChildren<QPushButton*>()) {
			if (button->text() == QStringLiteral("Reset notification preferences")) {
				button->click();
			}
		}
		expect(intents.back().Preferences.Notifications == core::notification_preferences{ }
				   && intents.back().Preferences.Role == projection.Preferences.Role,
			   "notification reset leaves queue filters intact");
		for (auto* button : window.findChildren<QPushButton*>()) {
			if (button->text() == QStringLiteral("Reset queue defaults")) {
				button->click();
			}
		}
		expect(!intents.back().Preferences.Role && intents.back().Preferences.Notifications.Paused,
			   "queue reset leaves accepted notification choices intact");
	}

	void test_large_queue_filters_through_the_composed_window()
	{
		qt::workbench_window window;
		auto                 intents = std::vector<application::queue_preferences_intent>{ };
		window.set_queue_preferences_dispatch(
			[&intents](application::queue_preferences_intent aIntent) { intents.push_back(std::move(aIntent)); });
		auto items = std::vector<core::queue_item>{ };
		items.reserve(2'048U);
		for (auto number = std::int64_t{ 1 }; number <= 2'048; ++number) {
			items.push_back(core::queue_item{
				.Ref = core::pull_request_ref{
					.Host       = "github.com",
					.Account    = "reviewer",
					.Repository = number % 4 == 0 ? "z/repo" : "a/repo",
					.Number     = number,
				},
				.Membership = core::queue_membership{
					.Role = number % 2 == 0 ? core::github_role::requested_reviewer : core::github_role::participant,
				},
				.Bucket = core::attention_bucket::quiet,
			});
		}
		items = core::ordered_queue(items);
		window.publish({
			.Status = application::queue_landing_status::ready,
			.Items  = items,
		});
		for (auto row = 0; row < window.queue_filter_model()->rowCount(); ++row) {
			const auto index = window.queue_filter_model()->index(row, 0);
			const auto item  = window.queue_filter_model()->item_at(index);
			if (item.has_value() && item->Ref.Number == 1'000) {
				static_cast<void>(window.home_list()->select(item->Ref, false));
				break;
			}
		}
		expect(
			window.selected_queue_item().has_value(),
			"the large queue selects one stable pull-request identity");
		window.show();
		QApplication::processEvents();
		// [[BPR-0783]]. The six typed controls are Settings' Queue defaults now,
		// in a window of its own that is not open here, so the control whose focus
		// a large publication must not disturb is one of Home's own.
		window.home_open_bar()->setFocus(Qt::OtherFocusReason);
		QApplication::processEvents();
		expect(
			QApplication::focusWidget() == window.home_open_bar(),
			"a real Home control accepts focus before the large-queue gesture");
		const auto focused   = QApplication::focusWidget();
		auto       signalLog = model_signal_log{ };
		observe_model_signals(*window.queue_filter_model(), signalLog);

		window.queue_role_filter()->setCurrentIndex(
			window.queue_role_filter()->findData(
				static_cast<int>(core::github_role::requested_reviewer)));
		window.publish({ .Status = application::queue_landing_status::ready, .Items = items, .Preferences = intents.back().Preferences });
		window.queue_sort_selector()->setCurrentIndex(
			window.queue_sort_selector()->findData(
				static_cast<int>(application::queue_sort_order::pull_request_number)));

		const auto selected = window.selected_queue_item();
		expect(
			window.queue_landing()->rowCount() == 2'048
				&& window.queue_filter_model()->rowCount() == 1'024,
			"the composed non-contiguous filter keeps the exact matching half");
		expect(
			selected.has_value() && selected->Ref.Number == 1'000,
			"the selected pull request survives the large filter and alternate sort");
		window.publish({ .Status = application::queue_landing_status::ready, .Items = items, .Preferences = intents.back().Preferences });
		expect(
			QApplication::focusWidget() == focused,
			"the focused control survives the large filter and alternate sort");
		expect(
			signalLog.Reset == 0 && signalLog.Removed <= 1'024
				&& signalLog.Layout <= 2,
			"the composed large update is linear and incremental without a reset");
	}

	/// \brief [[BPR-0783]]. A session with an account, which is what Home's own
	/// strip controls act on.
	void sign_in(qt::workbench_window& aWindow)
	{
		aWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{ .Status = core::github_status::session_ready },
			},
		});
		process_events();
	}

	[[nodiscard]] auto two_account_projection() -> application::queue_landing_projection
	{
		auto mine = core::queue_item{
			.Ref        = { .Host = "github.com", .Account = "octo-reviewer", .Repository = "octo-org/app", .Number = 101 },
			.Membership = { .Role = core::github_role::requested_reviewer },
			.Bucket     = core::attention_bucket::initial_review_requested,
		};
		mine.Facts  = core::queue_item_facts{ .Title = "A title", .Author = "octocat" };
		auto theirs = core::queue_item{
			.Ref        = { .Host = "ghe.example.invalid", .Account = "octo-enterprise", .Repository = "acme-tools/build", .Number = 201 },
			.Membership = { .Role = core::github_role::reviewer },
			.Bucket     = core::attention_bucket::quiet,
		};
		theirs.Facts = core::queue_item_facts{ .Title = "A title", .Author = "octocat" };
		return {
			.Status = application::queue_landing_status::ready,
			.Items  = core::ordered_queue(std::vector{ mine, theirs }),
		};
	}

	/// \brief Saves one full-window capture, optionally with an open menu over it.
	void save_home_capture(
		qt::workbench_window& aWindow, const QString& aDirectory, const QString& aName, QWidget* const aOverlay)
	{
		auto image = aWindow.grab();
		if (aOverlay != nullptr) {
			auto painter = QPainter{ &image };
			painter.drawPixmap(aWindow.mapFromGlobal(aOverlay->mapToGlobal(QPoint{ })), aOverlay->grab());
		}
		expect(image.save(aDirectory + QLatin1Char{ '/' } + aName), "the capture is written");
	}

	// A7: the strip holds Group, State headers, Filter and the refresh glyph
	// before the divider, the account and settings, and only where they can act.
	void test_home_strip_holds_group_state_headers_filter_and_refresh()
	{
		qt::workbench_window window;
		window.resize(1'280, 820);
		window.show();
		process_events();
		window.publish(two_account_projection());
		sign_in(window);

		auto* const toolbar = window.primary_toolbar();
		const auto  actions = toolbar->actions();
		// The actions themselves where the window hands them out, and the object
		// name only for the two widget items and the rule, which it does not.
		const auto at    = [&actions](QAction* const aAction) { return actions.indexOf(aAction); };
		const auto named = [&actions](const QString& aName) {
			for (auto index = qsizetype{ 0 }; index < actions.size(); ++index) {
				if (actions[index]->objectName() == aName) {
					return index;
				}
			}
			return qsizetype{ -1 };
		};
		const auto groupAt    = named(QStringLiteral("homeGroupAction"));
		const auto headersAt  = at(window.home_state_headers_action());
		const auto filterAt   = named(QStringLiteral("homeFilterAction"));
		const auto refreshAt  = at(window.queue_refresh_action());
		const auto dividerAt  = named(QStringLiteral("workbenchStripRightDividerAction"));
		const auto accountAt  = at(window.account_action());
		const auto settingsAt = at(window.settings_action());
		expect(groupAt >= 0 && groupAt < headersAt && headersAt < filterAt && filterAt < refreshAt
				   && refreshAt < dividerAt && dividerAt < accountAt && accountAt < settingsAt,
			   message_for("the strip's order is Group, State headers, Filter, refresh, the divider, account, settings",
						   QStringLiteral("group=%1 headers=%2 filter=%3 refresh=%4 divider=%5 account=%6 settings=%7")
							   .arg(groupAt)
							   .arg(headersAt)
							   .arg(filterAt)
							   .arg(refreshAt)
							   .arg(dividerAt)
							   .arg(accountAt)
							   .arg(settingsAt)
							   .toStdString()));

		auto* const headers = toolbar->widgetForAction(window.home_state_headers_action());
		expect(window.showing_home() && window.home_group_button()->isVisible()
				   && window.home_group_button()->text() == QStringLiteral("Group: None")
				   && headers != nullptr && headers->isVisible()
				   && window.home_state_headers_action()->isChecked()
				   && window.home_filter_button()->isVisible()
				   && window.home_filter_button()->text() == QStringLiteral("Filter")
				   && toolbar->widgetForAction(window.queue_refresh_action())->isVisible()
				   && !toolbar->widgetForAction(window.github_refresh_action())->isVisible(),
			   "signed in on Home all four are shown, Group says None and State headers is on");
		// [[BPR-0990]]. The glyph is discovery's, and says so while nothing
		// can run it: no discovery dispatch and no account session yet.
		expect(!window.queue_refresh_action()->isEnabled() && !window.queue_refresh_action()->toolTip().isEmpty(),
			   "the refresh glyph is disabled, and says why, until discovery can run");
		for (auto* const stop : { static_cast<QWidget*>(window.home_group_button()), headers,
								  static_cast<QWidget*>(window.home_filter_button()) }) {
			expect(stop->focusPolicy() == Qt::StrongFocus && !stop->accessibleName().isEmpty()
					   && !stop->accessibleDescription().isEmpty() && window.focus_order().contains(stop),
				   "each of Home's controls is focusable, named, described and declared");
		}

		// Group and State headers change Home at once and are remembered.
		auto views = std::vector<application::home_view_state>{ };
		window.set_home_view_dispatch([&views](application::home_view_state aView) { views.push_back(aView); });
		window.home_group_action(application::home_grouping::repository)->trigger();
		process_events();
		expect(window.home_group_button()->text() == QStringLiteral("Group: Repository")
				   && window.home_list()->grouping() == application::home_grouping::repository
				   && views.size() == 1U && views.back().Grouping == application::home_grouping::repository,
			   "choosing a grouping regroups Home and dispatches the whole view");
		window.home_state_headers_action()->trigger();
		process_events();
		expect(!window.home_list()->state_headers() && views.size() == 2U && !views.back().StateHeaders,
			   "turning State headers off does the same");
		// Collapsing one of the panels that carry no name is part of the view.
		window.home_group_action(application::home_grouping::none)->trigger();
		window.home_state_headers_action()->trigger();
		process_events();
		const auto before = views.size();
		window.home_list()->state_panel(qt::home_group::needs_you)->set_collapsed(true);
		process_events();
		expect(views.size() == before + 1U
				   && (views.back().Collapsed & application::home_collapsed_panel::needs_me) != 0U,
			   "collapsing Needs me dispatches the view with that bit set");

		// Signed out they act on nothing, so they are absent rather than disabled.
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{ .Status = core::github_status::no_profile },
			},
		});
		process_events();
		expect(!window.home_group_button()->isVisible() && !headers->isVisible()
				   && !window.home_filter_button()->isVisible()
				   && !toolbar->widgetForAction(window.github_refresh_action())->isVisible()
				   && !toolbar->widgetForAction(window.queue_refresh_action())->isVisible()
				   && toolbar->widgetForAction(window.account_action())->isVisible()
				   && toolbar->widgetForAction(window.settings_action())->isVisible(),
			   "signed out the strip is Home, account and settings, with no disabled ghosts");
		window.hide();
	}

	/// [[BPR-0990]]. The queue as discovery leaves it: a session, a saved
	/// repository, one row found and the status of the last refresh.
	[[nodiscard]] auto discovery_projection(const application::queue_discovery_status& aStatus)
		-> application::queue_landing_projection
	{
		auto projection              = two_account_projection();
		projection.Session           = { .Host = "github.com", .Account = "octo-reviewer", .Generation = 3U };
		projection.SavedRepositories = { { .Host = "github.com", .Account = "octo-reviewer", .Repository = "octo-org/app" } };
		projection.Discovery         = aStatus;
		return projection;
	}

	// [[BPR-0990]], [[BPR-0801]]. On Home the refresh glyph is the profile's
	// discovery: it sends one typed intent, arriving at Home asks discovery to
	// catch up, one sentence says what discovery last did, and the Pull
	// Request menu follows the open pull request or saves its repository
	// without reaching GitHub.
	void test_home_discovery_glyph_status_and_pull_request_menu()
	{
		qt::workbench_window window;
		window.resize(1'280, 820);
		auto discovery = std::vector<application::queue_discovery_intent>{ };
		auto follows   = std::vector<application::queue_follow_intent>{ };
		auto saves     = std::vector<application::saved_repository_intent>{ };
		window.set_queue_discovery_dispatch([&discovery](application::queue_discovery_intent aIntent) { discovery.push_back(aIntent); });
		window.set_queue_follow_dispatch([&follows](application::queue_follow_intent aIntent) { follows.push_back(std::move(aIntent)); });
		window.set_saved_repository_dispatch([&saves](application::saved_repository_intent aIntent) { saves.push_back(std::move(aIntent)); });
		window.show();
		process_events();
		sign_in(window);
		using state   = application::queue_discovery_state;
		using problem = application::queue_discovery_problem;
		auto status   = application::queue_discovery_status{ .Host = "github.com", .Account = "octo-reviewer", .State = state::refreshing, .Repositories = 1U };
		window.publish(discovery_projection(status));
		process_events();
		auto* const toolbar = window.primary_toolbar();
		auto* const glyph   = qobject_cast<QToolButton*>(toolbar->widgetForAction(window.queue_refresh_action()));
		auto* const label   = window.home_discovery_status();
		expect(window.showing_home() && glyph != nullptr && glyph->isVisible() && !window.queue_refresh_action()->isEnabled()
				   && label->isVisible() && label->text() == QStringLiteral("Finding my pull requests in 1 saved repository…"),
			   "while a refresh runs the glyph waits and Home says what is being searched");
		expect(glyph->focusPolicy() == Qt::StrongFocus && !glyph->accessibleName().isEmpty() && !glyph->accessibleDescription().isEmpty()
				   && window.focus_order().contains(glyph)
				   && window.focus_order().indexOf(glyph) > window.focus_order().indexOf(window.home_filter_button()),
			   "the glyph is a named, described stop after Filter in the declared order");

		status.State       = state::current;
		status.LastSuccess = 1'700'000'000;
		window.publish(discovery_projection(status));
		process_events();
		const auto at = qt::home_discovery_time(1'700'000'000);
		expect(window.queue_refresh_action()->isEnabled() && label->text() == QStringLiteral("Found at %1 in 1 saved repository.").arg(at),
			   "a current list says when it was found, from how many repositories, and the glyph can run again");
		window.queue_refresh_action()->trigger();
		expect(discovery == std::vector{ application::queue_discovery_intent::refresh }, "the glyph sends exactly the refresh intent");

		const auto says = [&](const application::queue_discovery_status& aStatus, const QString& aText, const char* aWhat) {
			window.publish(discovery_projection(aStatus));
			process_events();
			expect(label->isVisible() == !aText.isEmpty() && label->text() == aText,
				   message_for(aWhat, label->text().toStdString()));
		};
		says({ .Host = "github.com", .Account = "octo-reviewer", .State = state::partial, .Problem = problem::result_limit, .LastSuccess = 1'700'000'000, .Repositories = 2U },
			 QStringLiteral("Found at %1 in 2 saved repositories, but the list may be missing some: more matched than one search returns.").arg(at),
			 "a partial list says so, and why");
		says({ .Host = "github.com", .Account = "octo-reviewer", .State = state::stale, .Problem = problem::rate_limited, .LastSuccess = 1'700'000'000, .RetryAfterSeconds = 120U, .Repositories = 1U },
			 QStringLiteral("Not refreshed: GitHub's rate limit was reached. Showing what was found at %1. Refresh again in 120 seconds.").arg(at),
			 "a stale list names its failure, keeps what was found and says how long to wait");
		says({ .Host = "github.com", .Account = "octo-reviewer", .State = state::stale, .Problem = problem::offline, .Repositories = 1U },
			 QStringLiteral("Not refreshed: GitHub could not be reached."),
			 "an offline first refresh says so");
		says({ .Host = "github.com", .Account = "octo-reviewer", .State = state::unavailable, .Problem = problem::no_saved_repository },
			 QStringLiteral("Save a repository to find the pull requests in it that need me."),
			 "with nothing saved Home says what to do");
		says({ .State = state::unavailable, .Problem = problem::no_account }, QString{ }, "with no account session there is no sentence");
		expect(!window.queue_refresh_action()->isEnabled(), "and nothing to refresh");

		// Leaving Home and coming back asks discovery to catch up.
		window.publish(application::github_presentation_update{
			.PullRequest = application::github_pull_request_projection{ .Owner = "octo-org", .RepositoryName = "tools", .Number = core::pull_request_number{ 7U }, .Title = "Opened by link" },
		});
		window.publish(discovery_projection(status));
		process_events();
		expect(!window.showing_home() && !glyph->isVisible() && toolbar->widgetForAction(window.github_refresh_action())->isVisible(),
			   "the review strip's refresh is the pull request's");
		discovery.clear();
		window.home_action()->trigger();
		process_events();
		expect(window.showing_home() && discovery == std::vector{ application::queue_discovery_intent::activate },
			   "arriving at Home asks discovery once to catch up");

		// The open pull request's two local gestures.
		auto* const follow = window.follow_pull_request_action();
		auto* const save   = window.save_pull_request_repository_action();
		expect(window.pull_request_menu() != nullptr && window.pull_request_menu()->actions().contains(follow) && window.pull_request_menu()->actions().contains(save)
				   && follow->isEnabled() && !follow->isChecked() && save->isEnabled(),
			   "an unsaved repository's pull request can be followed and its repository saved");
		follow->trigger();
		save->trigger();
		const auto opened = core::pull_request_ref{ .Host = "github.com", .Account = "octo-reviewer", .Repository = "octo-org/tools", .Number = 7 };
		expect(follows.size() == 1U && follows.back().Ref == opened && follows.back().Follow && follows.back().Title == "Opened by link"
				   && saves.size() == 1U && saves.back().Action == application::saved_repository_action::add
				   && saves.back().Repository == application::saved_repository{ .Host = "github.com", .Account = "octo-reviewer", .Repository = "octo-org/tools" },
			   "Follow and Save Repository send the queue's typed intents for exactly the open pull request");
		expect(!follow->isChecked(), "the check mark waits for the queue");
		auto followed = discovery_projection(status);
		followed.SavedRepositories.push_back({ .Host = "github.com", .Account = "octo-reviewer", .Repository = "octo-org/tools" });
		followed.Items.push_back(core::queue_item{ .Ref = opened, .Membership = { .Followed = true }, .Bucket = core::attention_bucket::detail_unknown });
		window.publish(followed);
		process_events();
		expect(follow->isChecked() && !save->isEnabled() && !save->toolTip().isEmpty(), "once the queue keeps both, Follow is checked and Save is done");
		follow->trigger();
		expect(follows.size() == 2U && !follows.back().Follow, "following again unfollows");

		// Signed out there is nothing to follow for.
		window.publish(application::github_presentation_update{
			.PullRequest = application::github_pull_request_projection{ .Owner = "octo-org", .RepositoryName = "tools", .Number = core::pull_request_number{ 8U }, .PublicPreview = true },
		});
		process_events();
		expect(!follow->isEnabled() && !save->isEnabled(), "a pull request read signed out cannot be kept for an account");
		window.hide();
	}

	// A7: Filter holds exactly the four surviving preferences, and one gesture
	// sends the profile's typed intent and takes ownership of this window's filter.
	void test_home_filter_menu_and_its_gesture()
	{
		qt::workbench_window window;
		auto                 intents = std::vector<application::queue_preferences_intent>{ };
		auto                 views   = std::vector<application::home_view_state>{ };
		window.set_queue_preferences_dispatch(
			[&intents](application::queue_preferences_intent aIntent) { intents.push_back(std::move(aIntent)); });
		window.set_home_view_dispatch([&views](application::home_view_state aView) { views.push_back(aView); });
		window.resize(1'280, 820);
		window.show();
		process_events();
		auto projection = two_account_projection();
		// A profile that keeps a Repository and a Bucket: Home ignores both.
		projection.Preferences.Repository = "octo-org/app";
		projection.Preferences.Bucket     = core::attention_bucket::initial_review_requested;
		window.publish(projection);
		sign_in(window);

		// The inventory, exactly, in the board's words.
		auto inventory = QStringList{ };
		for (const auto* const action : window.home_filter_menu()->actions()) {
			if (action->isSeparator() && action->text().isEmpty()) {
				continue;
			}
			inventory.push_back(action->text());
		}
		expect(inventory
				   == QStringList{ QStringLiteral("My role"), QStringLiteral("Any"), QStringLiteral("Author"),
								   QStringLiteral("Reviewer"), QStringLiteral("Requested"), QStringLiteral("Participant"),
								   QStringLiteral("No role"),
								   QStringLiteral("Added by"), QStringLiteral("Any"), QStringLiteral("Repository rule"),
								   QStringLiteral("Author rule"), QStringLiteral("Path rule"),
								   QStringLiteral("Quiet items"), QStringLiteral("Hide"), QStringLiteral("Show"),
								   QStringLiteral("Only quiet"),
								   QStringLiteral("Sort within a group"), QStringLiteral("Priority"),
								   QStringLiteral("Repository"), QStringLiteral("Pull request number"),
								   // [[BPR-0783]]. The text Qt is given: the doubled
								   // ampersand is the mnemonic escape, not what is shown.
								   QStringLiteral("Nothing here changes GitHub. Saved repositories and observer rules are in Settings › Reviews && queue."),
								   QStringLiteral("Open Reviews && queue") },
			   "Filter holds exactly My role, Added by, Quiet items and Sort within a group, with its footer");
		expect(window.home_filter_settings_action()->isEnabled(),
			   "the way to Settings › Reviews & queue is reachable from the menu");

		// [[BPR-0783]]. What a reviewer actually sees, rather than what the source
		// wrote: a menu eats a single "&" as a mnemonic and draws "Reviews _queue".
		// Qt's own mnemonic stripping is what assistive technology is given, so the
		// menu's accessibility tree is the honest oracle for the displayed text.
		auto* const menuAccessible = QAccessible::queryAccessibleInterface(window.home_filter_menu());
		expect(menuAccessible != nullptr && menuAccessible->childCount() > 0,
			   "the Filter menu is in the accessibility tree");
		auto* const footerAction = window.home_filter_menu()->findChild<QAction*>(QStringLiteral("homeFilterFooter"));
		const auto  displayed    = [&window, menuAccessible](QAction* const aAction) {
			const auto  index = window.home_filter_menu()->actions().indexOf(aAction);
			auto* const item  = index >= 0 ? menuAccessible->child(static_cast<int>(index)) : nullptr;
			return item != nullptr ? item->text(QAccessible::Name) : QString{ };
		};
		expect(footerAction != nullptr
				   && footerAction->text().contains(QStringLiteral("Reviews && queue"))
				   && window.home_filter_settings_action()->text().contains(QStringLiteral("Reviews && queue")),
			   "each literal ampersand is escaped, so the menu draws it instead of eating it as a mnemonic");
		expect(displayed(footerAction)
				   == QStringLiteral("Nothing here changes GitHub. Saved repositories and observer rules are in Settings › Reviews & queue."),
			   message_for("the footer a reviewer reads carries one plain ampersand", displayed(footerAction).toStdString()));
		expect(displayed(window.home_filter_settings_action()) == QStringLiteral("Open Reviews & queue"),
			   message_for("and so does the way to Reviews & queue",
						   displayed(window.home_filter_settings_action()).toStdString()));
		for (const auto& shown : { displayed(footerAction), displayed(window.home_filter_settings_action()) }) {
			expect(!shown.contains(QStringLiteral("&&")) && !shown.contains(QLatin1Char{ '_' }),
				   message_for("no escape and no mnemonic artefact reaches what is read aloud", shown.toStdString()));
		}
		window.home_filter_settings_action()->trigger();
		process_events();
		expect(window.settings()->isVisible() && window.settings()->current_page() == qt::settings_page::reviews_and_queue,
			   "and it opens that page");
		window.settings()->hide();
		process_events();

		// Home filters by the profile's four until this window filters for itself.
		expect(window.queue_filter_model()->rowCount() == 2
				   && window.queue_filter_model()->preferences().Repository == "octo-org/app"
				   && window.queue_filter_model()->effective_preferences().Repository.empty()
				   && !window.queue_filter_model()->effective_preferences().Bucket.has_value(),
			   "Repository and Bucket are kept for the profile and filter nothing on Home");

		auto* const requested = window.home_filter_menu()->findChild<QAction*>(QStringLiteral("homeFilterRequestedAction"));
		expect(requested != nullptr, "the menu's Requested item is named");
		requested->trigger();
		process_events();
		expect(intents.size() == 1U && intents.back().Field == application::queue_preference_field::role
				   && intents.back().Preferences.Role == core::github_role::requested_reviewer
				   && intents.back().Preferences.Repository == "octo-org/app"
				   && intents.back().Preferences.Bucket == core::attention_bucket::initial_review_requested
				   && intents.back().Preferences.Notifications == projection.Preferences.Notifications,
			   "one Filter gesture sends the existing typed intent with that one field changed");
		expect(views.size() == 1U && views.back().FilterOwned
				   && views.back().Filter.Role == core::github_role::requested_reviewer,
			   "and the window's view now owns its own four values");
		expect(window.home_view().FilterOwned && window.queue_filter_model()->rowCount() == 1,
			   "Home filters by the window's own filter at once");
		expect(window.home_filter_button()->text() == QStringLiteral("Filter •")
				   && window.home_filter_button()->accessibleDescription().contains(QStringLiteral("Requested"))
				   && window.home_filter_button()->accessibleDescription().startsWith(QStringLiteral("Filtered.")),
			   "a filter that is not the default is said without colour and described");

		// The profile moving on does not move an owning window.
		auto moved             = projection;
		moved.Preferences.Role = core::github_role::author;
		window.publish(moved);
		process_events();
		expect(window.queue_filter_model()->effective_preferences().Role == core::github_role::requested_reviewer
				   && window.queue_filter_model()->rowCount() == 1,
			   "an owning window ignores a later profile change");
		expect(window.queue_role_filter()->currentData().toInt() == static_cast<int>(core::github_role::author),
			   "while Settings still shows the profile's own answer");

		// A window that never filtered follows the profile.
		qt::workbench_window following;
		following.set_queue_preferences_dispatch([](application::queue_preferences_intent) { });
		following.publish(moved);
		process_events();
		expect(!following.home_view().FilterOwned
				   && following.queue_filter_model()->effective_preferences().Role == core::github_role::author
				   && following.queue_filter_model()->rowCount() == 0,
			   "a window that never filtered follows the profile's preferences");
		// [[BPR-0783]] review finding. The control must say what Home is filtered
		// by, not what the window was built with: the six combos it replaced
		// stayed in step with the accepted profile, and so must this.
		auto* const followingAuthor
			= following.home_filter_menu()->findChild<QAction*>(QStringLiteral("homeFilterAuthorAction"));
		expect(followingAuthor != nullptr && followingAuthor->isChecked(),
			   "the profile preference a window follows is ticked in its Filter menu");
		expect(following.home_filter_button()->text() == QStringLiteral("Filter •"),
			   message_for("and the Filter control says it is filtered",
						   following.home_filter_button()->text().toStdString()));
		window.hide();
	}

	// A7: the view the host holds is restored without being dispatched again.
	void test_a_published_home_view_is_restored_without_a_second_dispatch()
	{
		qt::workbench_window window;
		auto                 views = std::vector<application::home_view_state>{ };
		window.set_queue_preferences_dispatch([](application::queue_preferences_intent) { });
		window.set_home_view_dispatch([&views](application::home_view_state aView) { views.push_back(aView); });
		window.resize(1'280, 820);
		window.show();
		process_events();
		window.publish(two_account_projection());
		sign_in(window);
		static_cast<void>(window.home_list()->select(
			core::pull_request_ref{ .Host = "github.com", .Account = "octo-reviewer", .Repository = "octo-org/app", .Number = 101 },
			true));
		process_events();

		auto restored = application::home_view_state{
			.Grouping     = application::home_grouping::account,
			.StateHeaders = false,
			.Collapsed    = application::home_collapsed_panel::recently_viewed,
			.FilterOwned  = true,
			.Filter       = { .Quiet = application::queue_quiet_filter::quiet },
		};
		auto projection     = application::workbench_projection{ };
		projection.HomeView = restored;
		window.publish(projection);
		process_events();
		expect(window.home_view() == restored && window.home_list()->grouping() == application::home_grouping::account
				   && !window.home_list()->state_headers()
				   && window.home_group_button()->text() == QStringLiteral("Group: Account")
				   && !window.home_state_headers_action()->isChecked()
				   && window.queue_filter_model()->effective_preferences().Quiet == application::queue_quiet_filter::quiet,
			   "a published view is what Home shows, in its list and on its strip");
		expect(views.empty(), "restoring a view dispatches nothing back");

		// Restoring a view does rearrange Home, which is what it is for. What must
		// change nothing at all is the same view arriving again.
		const auto selected = window.selected_queue_item();
		const auto focused  = QApplication::focusWidget();
		window.publish(projection);
		process_events();
		expect(views.empty(), "the same view again dispatches nothing");
		expect(QApplication::focusWidget() == focused, "the same view again moves no focus");
		expect(window.selected_queue_item() == selected, "the same view again loses no selection");
		window.hide();
	}

	// A7 and Decisions (e): the six typed controls are Settings' Queue defaults,
	// with their reset unchanged.
	void test_the_six_queue_controls_live_on_settings_queue_defaults()
	{
		qt::workbench_window window;
		auto                 intents = std::vector<application::queue_preferences_intent>{ };
		window.set_queue_preferences_dispatch(
			[&intents](application::queue_preferences_intent aIntent) { intents.push_back(std::move(aIntent)); });
		auto projection             = two_account_projection();
		projection.Preferences.Role = core::github_role::reviewer;
		window.publish(projection);
		process_events();

		auto* const page  = window.settings()->page_widget(qt::settings_page::reviews_and_queue);
		auto* const group = [page] {
			for (auto* const box : page->findChildren<QGroupBox*>()) {
				if (box->title() == QStringLiteral("Queue defaults")) {
					return box;
				}
			}
			return static_cast<QGroupBox*>(nullptr);
		}();
		expect(group != nullptr, "Reviews & queue has a Queue defaults group");
		const auto selectors = std::array{ window.queue_role_filter(), window.queue_observer_filter(),
										   window.queue_bucket_filter(), window.queue_repository_filter(),
										   window.queue_quiet_filter(), window.queue_sort_selector() };
		const auto names     = std::array{ QStringLiteral("queueRoleFilter"), QStringLiteral("queueObserverFilter"),
										   QStringLiteral("queueBucketFilter"), QStringLiteral("queueRepositoryFilter"),
										   QStringLiteral("queueQuietFilter"), QStringLiteral("queueSortSelector") };
		for (auto index = std::size_t{ 0U }; index < selectors.size(); ++index) {
			expect(selectors[index] != nullptr && selectors[index]->objectName() == names[index]
					   && group->isAncestorOf(selectors[index])
					   && !window.home_page()->isAncestorOf(selectors[index]),
				   "each control keeps its name and moved whole onto Queue defaults");
		}
		expect(window.queue_role_filter()->currentData().toInt() == static_cast<int>(core::github_role::reviewer),
			   "and still shows the accepted profile preference");
		// Its own gesture still dispatches the same typed intent.
		window.queue_role_filter()->setCurrentIndex(
			window.queue_role_filter()->findData(static_cast<int>(core::github_role::author)));
		expect(intents.size() == 1U && intents.back().Field == application::queue_preference_field::role
				   && intents.back().Preferences.Role == core::github_role::author,
			   "a Queue defaults gesture is the same typed profile intent it always was");
		for (auto* const button : window.findChildren<QPushButton*>()) {
			if (button->text() == QStringLiteral("Reset queue defaults")) {
				button->click();
			}
		}
		expect(!intents.back().Preferences.Role
				   && intents.back().Field == application::queue_preference_field::queue_filters
				   && intents.back().Preferences.Notifications == projection.Preferences.Notifications,
			   "Reset queue defaults is unchanged: the filters go, the notification choices stay");
	}

	// The accepted Home states, captured for the attended boards when asked for.
	void test_home_captures_every_accepted_view()
	{
		const auto directory = qEnvironmentVariable("BPR_HOME_CAPTURE_DIR");
		if (directory.isEmpty()) {
			return;
		}
		qt::workbench_window window;
		window.set_queue_preferences_dispatch([](application::queue_preferences_intent) { });
		window.resize(1'280, 820);
		window.show();
		process_events();
		window.publish(two_account_projection());
		sign_in(window);
		process_events();
		const auto capture = [&window, &directory](const QString& aName, QWidget* const aOverlay = nullptr) {
			process_events();
			save_home_capture(window, directory, aName, aOverlay);
		};
		capture(QStringLiteral("group-none-headers-on.png"));
		window.home_state_headers_action()->trigger();
		capture(QStringLiteral("group-none-headers-off.png"));
		window.home_group_action(application::home_grouping::organization)->trigger();
		capture(QStringLiteral("organization-headers-off.png"));
		window.home_state_headers_action()->trigger();
		capture(QStringLiteral("organization-headers-on.png"));
		window.home_group_action(application::home_grouping::account)->trigger();
		capture(QStringLiteral("account-headers-on.png"));
		window.home_group_action(application::home_grouping::none)->trigger();
		process_events();
		window.home_filter_menu()->popup(window.home_filter_button()->mapToGlobal(QPoint{ 0, window.home_filter_button()->height() }));
		process_events();
		capture(QStringLiteral("filter-menu-open.png"), window.home_filter_menu());
		window.home_filter_menu()->close();
		window.hide();
	}

	void test_home_composes_the_production_catalog_and_respects_motion_and_width()
	{
		auto provider = std::make_shared<qt::dif_catalog_provider>();
		auto window   = qt::workbench_window{ { }, { }, { }, { }, { }, nullptr, provider };
		expect(provider->load_attempts() == 0U && window.dif() == nullptr,
			   "constructing Home does not decode before a resolved visible presentation needs it");
		window.resize(900, 600);
		window.show();
		window.activateWindow();
		process_events();
		window.adopt_dif(core::resolve_dif(
			{ .Visibility = core::dif_visibility::shown, .Motion = core::motion_mode::full },
			core::motion_observation::reduced));
		process_events();
		expect(provider->load_attempts() == 0U && window.dif() == nullptr,
			   "compact Home does not decode art that has no presentation space");
		window.resize(1'280, 820);
		process_events();
		auto* dif = window.dif();
		expect(dif != nullptr && window.home_landing()->illustration() == dif
				   && provider->load_attempts() == 1U && provider->decoded_count() == 10U,
			   "real Home acquires the accepted ten-strip catalog once");
		expect(dif->focusPolicy() == Qt::NoFocus
				   && dif->testAttribute(Qt::WA_TransparentForMouseEvents),
			   "Dif is decoration outside the focus and pointer paths");

		const auto waitForFrame = [&](const auto& aReady, const int aTimeoutMs) {
			QElapsedTimer elapsed;
			elapsed.start();
			while (!aReady() && elapsed.elapsed() < aTimeoutMs) {
				QApplication::processEvents();
				QThread::msleep(2);
			}
			return aReady();
		};
		expect(waitForFrame([&] {
				   return dif->current_frame().Clip == qt::dif_index(qt::dif_clip_id::greet)
					   && dif->current_frame().Frame > 0;
			   },
							1'000),
			   "full-motion Home paints the greeting");
		const auto greeting = dif->grab().toImage();
		expect(waitForFrame([&] {
				   return dif->current_frame().Clip == qt::dif_index(qt::dif_clip_id::calm_idle)
					   && dif->current_frame().Frame > 0;
			   },
							2'000),
			   "the greeting returns directly to the breathing loop");
		const auto breathing = dif->grab().toImage();
		expect(!greeting.isNull() && !breathing.isNull() && greeting != breathing,
			   "normal Home renders different accepted greeting and breathing frames");
		expect(waitForFrame([&] {
				   return dif->current_frame().Clip == qt::dif_index(qt::dif_clip_id::blink);
			   },
							13'000),
			   "four calm loops reach the bounded blink cadence on Home");

		window.adopt_dif(core::resolve_dif(
			{ .Visibility = core::dif_visibility::shown, .Motion = core::motion_mode::reduced },
			core::motion_observation::full));
		process_events();
		const auto staticFrame   = dif->current_frame();
		const auto staticWakeups = dif->statistics().TimerWakeups;
		QThread::msleep(350);
		process_events();
		expect(staticFrame.Clip == qt::dif_index(qt::dif_clip_id::rest)
				   && staticFrame.Frame == 0 && !staticFrame.NextMs
				   && dif->statistics().TimerWakeups == staticWakeups,
			   "reduced motion rests on one static frame without a timer wakeup");

		window.resize(1'000, 600);
		process_events();
		expect(dif->isVisible() && dif->size() == QSize{ 72, 72 },
			   "Home compacts the illustration before its input controls");
		window.home_open_bar()->setFocus(Qt::OtherFocusReason);
		window.resize(900, 600);
		process_events();
		expect(!dif->isVisible() && window.home_open_bar()->isVisible()
				   && QApplication::focusWidget() == window.home_open_bar(),
			   "compact Home hides art while retaining the input and its focus");

		window.adopt_dif(core::resolve_dif(
			{ .Visibility = core::dif_visibility::hidden, .Motion = core::motion_mode::full },
			core::motion_observation::full));
		window.resize(1'280, 820);
		process_events();
		expect(!dif->isVisible(), "the accepted hidden preference removes Home art");
		window.hide();
	}

	void test_dif_uses_the_corner_except_for_prime_spots()
	{
		auto provider = std::make_shared<qt::dif_catalog_provider>();
		auto window   = qt::workbench_window{ { }, { }, { }, { }, { }, nullptr, provider };
		window.resize(1'600, 820);
		window.show();
		window.activateWindow();
		window.adopt_dif(core::resolve_dif(
			{ .Visibility = core::dif_visibility::shown, .Motion = core::motion_mode::reduced },
			core::motion_observation::full));
		process_events();
		auto* const dif = window.dif();
		expect(dif != nullptr && window.home_landing()->illustration() == dif
				   && dif->size() == QSize{ 96, 96 } && provider->load_attempts() == 1U,
			   "Home owns the one lazily decoded Dif presentation before a review opens");

		window.publish(application::github_presentation_update{
			.PullRequest = application::github_pull_request_projection{
				.Owner                  = "octo",
				.RepositoryName         = "app",
				.Number                 = core::pull_request_number{ 7U },
				.ReviewThreadCount      = 0U,
				.ReviewThreadsAvailable = true,
			},
		});
		auto collapsed    = both_files_enabled();
		collapsed.Drawers = drawer_states(core::workbench_drawer_state::expanded,
										  core::workbench_drawer_state::collapsed);
		window.publish(collapsed);
		process_events();
		auto* const corner   = window.dif_corner_slot();
		const auto  inCorner = [&] {
			return corner != nullptr && corner->isVisible() && dif->isVisible()
				&& dif->parentWidget() == &window && dif->size() == QSize{ 28, 28 }
			&& !window.conversations_drawer()->isAncestorOf(dif)
				&& window.findChild<QWidget*>(QStringLiteral("workbenchDifAiSlot")) == nullptr;
		};
		dif->setFixedSize(40, 40);
		expect(!inCorner(), "negative control: the rejected forty-pixel corner is not accepted");
		dif->setFixedSize(28, 28);
		expect(!window.showing_home() && inCorner(),
			   "opening a review rests the same Dif in the status corner even with the drawer collapsed");

		// [[BPR-1134]]. The explicit named selector is the tray's Notes tab.
		auto* const action = window.ai_tools_action();
		auto* const tabs   = window.tray_tabs();
		expect(action != nullptr && action->isCheckable() && !action->isChecked()
				   && action->text() == QStringLiteral("Notes") && tabs != nullptr
				   && tabs->tabText(1) == QStringLiteral("Notes") && tabs->currentIndex() == 0,
			   "the existing AI area has an explicit named selector");
		window.adopt_dif(core::resolve_dif(
			{ .Visibility = core::dif_visibility::shown, .Motion = core::motion_mode::full },
			core::motion_observation::full));
		tabs->setCurrentIndex(1);
		process_events();
		expect(dif->current_frame().Clip != qt::dif_index(qt::dif_clip_id::compare_glance),
			   "selecting a hidden private tab does not replay the explicit-content cue in the corner");
		window.adopt_dif(core::resolve_dif(
			{ .Visibility = core::dif_visibility::shown, .Motion = core::motion_mode::reduced },
			core::motion_observation::full));
		expect(action->isChecked()
				   && window.conversations_drawer()->state()
					   == core::workbench_drawer_state::collapsed
				   && inCorner(),
			   "the private tab selector leaves its collapsed drawer closed and Dif in the corner");

		auto opened    = collapsed;
		opened.Drawers = drawer_states(core::workbench_drawer_state::expanded,
									   core::workbench_drawer_state::expanded);
		opened.Layout  = application::workbench_layout_delta{
			.FilesWidth         = 320U,
			.ConversationsWidth = 375U,
		};
		window.publish(opened);
		process_events();
		expect(inCorner()
				   && window.home_landing()->illustration() == nullptr
				   && window.findChildren<QWidget*>(QStringLiteral("workbenchDif")).size() == 1
				   && provider->load_attempts() == 1U,
			   "selecting the private tab keeps the same single 28-pixel presentation in the corner");
		expect(dif->current_frame().Clip == qt::dif_index(qt::dif_clip_id::rest)
				   && !dif->current_frame().NextMs,
			   "reduced motion keeps the contextual presentation on a static frame");

		drag_conversations_handle(window, 200);
		process_events();
		expect(inCorner(), "a narrow drawer does not own or hide the corner illustration");
		dif->setParent(window.findings_tab_scroll()->widget());
		dif->show();
		expect(!inCorner(), "negative control: moving Dif into the private drawer fails corner placement");
		window.adopt_dif(core::resolve_dif(
			{ .Visibility = core::dif_visibility::shown, .Motion = core::motion_mode::reduced },
			core::motion_observation::full));
		process_events();
		expect(inCorner(), "the accepted placement repairs the negative control with the same widget");
		for (const auto mode : { qt::review_modality::overview, qt::review_modality::read,
								 qt::review_modality::review, qt::review_modality::discuss }) {
			window.choose_modality(mode);
			process_events();
			expect(inCorner(), "each ordinary review surface uses the corner");
			save_render(window, QStringLiteral("dif-corner-%1").arg(static_cast<int>(mode)));
		}
		window.open_review_composition(false);
		process_events();
		expect(inCorner(), "whole-review Compose has no workshop prime spot");
		save_render(window, QStringLiteral("dif-corner-compose"));

		window.publish(opened);
		window.choose_home(true);
		process_events();
		// [[BPR-0775]] mandate r2. Over a review Home is not the first-launch
		// card, so Dif rests in the window's bottom-left corner, out of the way.
		expect(window.showing_home() && window.home_landing()->illustration() == nullptr
				   && corner != nullptr && corner->isVisible()
				   && dif->parentWidget() == &window && dif->isVisible() && dif->size() == QSize{ 28, 28 }
				   && window.findChildren<QWidget*>(QStringLiteral("workbenchDif")).size() == 1
				   && provider->load_attempts() == 1U,
			   "Home wins and rests the same presentation in the bottom-left corner");
		const auto slotBottomLeft = corner->mapTo(&window, QPoint{ 0, corner->height() });
		const auto statusBar      = window.statusBar()->geometry();
		expect(dif->geometry().left() == slotBottomLeft.x() && dif->geometry().bottom() == slotBottomLeft.y() - 1
				   && dif->geometry().left() < 24
				   && statusBar.contains(QPoint{ dif->geometry().left(), dif->geometry().bottom() })
				   && window.status_label()->geometry().left() >= corner->geometry().right(),
			   "the corner sprite stands on the status line's leading edge and the line begins after it");
		expect(dif->testAttribute(Qt::WA_TransparentForMouseEvents) && dif->focusPolicy() == Qt::NoFocus,
			   "the corner sprite is outside the pointer and focus paths");
		window.statusBar()->showMessage(QStringLiteral("A transient message"));
		process_events();
		expect(!corner->isVisible() && !dif->isVisible(),
			   "a transient status message hides the slot and the sprite rather than being covered by them");
		window.statusBar()->clearMessage();
		process_events();
		expect(corner->isVisible() && dif->isVisible(),
			   "the corner returns when the message clears");
		// Review of BPR-0775, finding 1: a message already showing when Home
		// arrives is not painted over either.
		window.choose_modality(qt::review_modality::read);
		process_events();
		window.statusBar()->showMessage(QStringLiteral("A message before Home"));
		window.choose_home(true);
		process_events();
		expect(window.showing_home() && !corner->isVisible() && !dif->isVisible(),
			   "Home entered under a transient message keeps the corner hidden until it clears");
		window.statusBar()->clearMessage();
		process_events();
		expect(corner->isVisible() && dif->isVisible() && dif->parentWidget() == &window,
			   "the corner appears once that message clears");

		window.choose_modality(qt::review_modality::read);
		process_events();
		expect(!window.showing_home() && inCorner(),
			   "returning to the review preserves the corner independently of private tab selection");
		tabs->setCurrentIndex(0);
		process_events();
		expect(!action->isChecked() && inCorner(),
			   "leaving the private tab preserves the corner without changing its content");
		auto* editor = window.review_editor();
		auto  draft  = core::review_comment_draft{
			.Id     = core::review_draft_id{ 1U },
			.Anchor = { .Comparison = opaque<core::file_comparison_digest>(2U), .Side = core::diff_side::right, .Line = 8U },
			.Body   = "Original comment",
		};
		editor->present(draft);
		editor->present(qt::comment_workshop_proposal{ draft.Id, draft.Body, "Proposed revision", { } });
		window.open_review_composition(true);
		process_events();
		auto* accept = window.findChild<QToolButton*>(QStringLiteral("reviewAcceptRevisionButton"));
		expect(accept != nullptr && accept->isVisible() && accept->isEnabled()
				   && window.dif_workshop_slot()->isVisible() && dif->parentWidget() == window.dif_workshop_slot()
				   && !corner->isVisible()
				   && window.findChildren<QWidget*>(QStringLiteral("workbenchDif")).size() == 1,
			   "Compose offers the exact proposal with the same single Dif beside visible acceptance");
		accept->setFocus();
		window_key(window, Qt::Key_Escape, Qt::NoModifier);
		expect(!window.dif_workshop_slot()->isVisible(), "Escape also returns from the fixed workshop header");
		window.open_review_composition(true);
		process_events();
		draft.Id = core::review_draft_id{ 2U };
		editor->present(draft);
		process_events();
		expect(!window.dif_workshop_slot()->isVisible() && !accept->isEnabled()
				   && editor->reject_revision_action()->isEnabled() && editor->statistics().DraftIntents == 0U,
			   "another draft cannot keep the workshop illustration or accept its proposal");
		expect(inCorner(), "an unavailable workshop acceptance returns the same widget to the corner");
		window.hide();
	}

	/// \brief [[BPR-1074]]. Names every widget shown as a window while it lives.
	class shown_window_recorder final : public QObject
	{
	public:
		shown_window_recorder()
		{
			QCoreApplication::instance()->installEventFilter(this);
		}
		shown_window_recorder(const shown_window_recorder&)                    = delete;
		shown_window_recorder(shown_window_recorder&&)                         = delete;
		auto operator=(const shown_window_recorder&) -> shown_window_recorder& = delete;
		auto operator=(shown_window_recorder&&) -> shown_window_recorder&      = delete;
		~shown_window_recorder() override
		{
			QCoreApplication::instance()->removeEventFilter(this);
		}

		[[nodiscard]] auto shown() const -> const QStringList&
		{
			return Shown;
		}

		[[nodiscard]] auto described() const -> std::string
		{
			return Shown.isEmpty() ? std::string{ "none" } : Shown.join(QStringLiteral(", ")).toStdString();
		}

	protected:
		auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
		{
			if (aEvent->type() == QEvent::Show) {
				if (const auto* const widget = qobject_cast<const QWidget*>(aWatched);
					widget != nullptr && widget->isWindow()) {
					Shown.push_back(widget->objectName());
				}
			}
			return QObject::eventFilter(aWatched, aEvent);
		}

	private:
		QStringList Shown;
	};

	/// \brief [[BPR-1074]]. Building the window puts nothing on the desktop.
	///
	/// \details The three drawers are made without a parent and placed in the
	/// splitter later. A drawer that showed itself in between became a window of
	/// its own, so every launch put three windows titled with the application's
	/// name on the desktop for about a second, and a probe that took the first
	/// window for the application held a handle that was then destroyed.
	void test_building_the_window_shows_no_window_of_its_own()
	{
		{
			const auto           recorder = shown_window_recorder{ };
			qt::workbench_window window;
			process_events();
			expect(recorder.shown().isEmpty(),
				   "building the workbench window shows no window of its own; shown: " + recorder.described());
			for (const auto* const drawer : { window.files_drawer(), window.conversations_drawer(), window.repositories_drawer() }) {
				expect(drawer != nullptr && !drawer->isWindow() && drawer->parentWidget() != nullptr,
					   "each drawer is placed in the window, not a window of its own");
			}
			window.show();
			process_events();
			expect(recorder.shown() == QStringList{ QStringLiteral("workbenchWindow") },
				   "showing the workbench window shows that window and no other; shown: " + recorder.described());
			// Home holds the window at first; the drawers belong to the review.
			window.publish(both_files_enabled());
			process_events();
			expect(!window.showing_home() && window.files_drawer()->isVisible()
					   && window.conversations_drawer()->isVisible() && window.repositories_drawer()->isVisible(),
				   "the expanded drawers show with the review");
			window.hide();
		}

		const auto placed = [](qt::workbench_drawer_panel* const aPanel) {
			auto host = std::make_unique<QWidget>();
			host->setObjectName(QStringLiteral("drawerHost"));
			auto* const column = new QVBoxLayout{ host.get() };
			column->addWidget(aPanel);
			host->resize(400, 300);
			host->show();
			process_events();
			return host;
		};
		const auto recorder = shown_window_recorder{ };
		// A fresh panel adopts every state it can while it has no parent.
		auto* const fresh = new qt::workbench_drawer_panel{ core::workbench_drawer::files };
		fresh->adopt(core::workbench_drawer_state::collapsed);
		fresh->set_controls_shown(true);
		fresh->adopt(core::workbench_drawer_state::expanded);
		fresh->set_controls_shown(false);
		process_events();
		expect(recorder.shown().isEmpty() && !fresh->isVisible() && !fresh->testAttribute(Qt::WA_WState_Created),
			   "a drawer without a parent is never shown as a window; shown: " + recorder.described());
		const auto freshHost = placed(fresh);
		expect(fresh->isVisible(), "an expanded drawer shows once it is placed");

		// Collapsed with the controls hidden is absence, before and after it is
		// placed, and the drawer comes back when it expands.
		auto* const absent = new qt::workbench_drawer_panel{ core::workbench_drawer::repositories };
		absent->adopt(core::workbench_drawer_state::collapsed);
		const auto absentHost = placed(absent);
		expect(!absent->isVisible(), "a collapsed drawer without its controls stays absent once placed");
		absent->adopt(core::workbench_drawer_state::expanded);
		process_events();
		expect(absent->isVisible(), "the absent drawer shows when it expands");

		// Hidden while it had no parent, then expanded before it was placed:
		// the placement shows it.
		auto* const returning = new qt::workbench_drawer_panel{ core::workbench_drawer::conversations };
		returning->adopt(core::workbench_drawer_state::collapsed);
		returning->adopt(core::workbench_drawer_state::expanded);
		process_events();
		expect(!returning->isVisible(), "a drawer expanded without a parent is still not a window");
		const auto returningHost = placed(returning);
		expect(returning->isVisible(), "a drawer hidden and expanded before placement shows once placed");
		expect(recorder.shown() == QStringList{ QStringLiteral("drawerHost"), QStringLiteral("drawerHost"), QStringLiteral("drawerHost") },
			   "only the three hosts were shown as windows; shown: " + recorder.described());
	}

	/// \brief [[BPR-1287]]. The owner, 2026-10-06: "this is what happens when
	/// you go to Read without a file open. We need to not show a blank diff
	/// here and instead have some clear direction to open a file."
	void test_read_without_a_file_gives_direction_not_a_blank_diff()
	{
		intent_log           log;
		qt::workbench_window window{
			[&log](core::workbench_interaction_intent aIntent) { log.Workbench.push_back(aIntent); },
			[&log](core::navigate_diff_change_intent aIntent) { log.Change.push_back(aIntent); }
		};
		window.resize(1'280, 852);
		window.show();
		window.choose_modality(qt::review_modality::read);
		auto projection      = every_primary_command_offered();
		projection.Selection = core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{ .ActionableCount = 17U } };
		window.publish(projection);
		process_events();
		auto* const surface = window.comparison_surface();
		auto* const idle    = surface->idle_page();
		expect(surface->idle() && idle->isVisible() && !surface->left_pane()->isVisible() && !surface->right_pane()->isVisible()
				   && !surface->findChild<QLabel*>(QStringLiteral("diffWorkbenchStatus"))->isVisible(),
			   "with no file chosen Read shows no empty panes, no line numbers and no bare status");
		const auto* const title  = idle->findChild<QLabel*>(QStringLiteral("diffWorkbenchIdleTitle"));
		const auto* const detail = idle->findChild<QLabel*>(QStringLiteral("diffWorkbenchIdleDetail"));
		expect(title != nullptr && title->text() == QStringLiteral("No file open") && detail != nullptr
				   && detail->text().contains(QStringLiteral("17 files in Files")) && idle->accessibleName() == QStringLiteral("No file open"),
			   "it says no file is open and how to open one");
		expect(window.status_text().contains(QStringLiteral("No file is open. Choose one of the 17 actionable file(s).")),
			   "the status bar says nothing is chosen, not that a file is outside the list");
		save_render(window, QStringLiteral("read-no-file"));
		auto* const openFirst = surface->idle_open_first_button();
		auto* const showFiles = surface->idle_show_files_button();
		expect(openFirst->isVisible() && openFirst->isEnabled() && openFirst->focusPolicy() != Qt::NoFocus && showFiles->isVisible()
				   && showFiles->focusPolicy() != Qt::NoFocus,
			   "Open first file and Show files are offered and reachable by keyboard");
		const auto before = log.Workbench.size();
		openFirst->click();
		expect(log.Workbench.size() == before + 1U && file_direction(log.Workbench.back()) == core::change_navigation_direction::next,
			   "Open first file is Next file's own intent, which from no file opens the first");
		const auto noFiles = command_delta(offered(core::workbench_command_id::previous_file),
										   refused(core::workbench_command_id::next_file, core::workbench_reason_code::no_actionable_files),
										   offered(core::workbench_command_id::previous_change), offered(core::workbench_command_id::next_change));
		window.publish({ .Selection = core::workbench_selection_delta{ .Cursor = core::workbench_navigation_cursor{ } }, .Commands = noFiles });
		process_events();
		expect(title->text() == QStringLiteral("No files to read") && openFirst->isHidden(),
			   "a pull request with no actionable file says that instead and offers no Open first file");
		surface->set_loading(request_context(computed_comparison("old\n", "new\n", 0U, core::file_identity_of("src/file.cpp"))));
		process_events();
		expect(!surface->idle() && idle->isHidden() && surface->left_pane()->isVisible(),
			   "a chosen file's comparison takes the area back");
		surface->clear();
		process_events();
		expect(surface->idle() && idle->isVisible(), "clearing the choice brings the direction back");
		surface->clear(QStringLiteral("The comparison could not be read."));
		process_events();
		expect(!surface->idle() && surface->status_text() == QStringLiteral("The comparison could not be read."),
			   "a reason the host gives is shown as before, not hidden behind the direction");
		expect(log.Change.empty() && log.Workbench.size() == before + 1U, "the direction itself dispatches nothing else");
	}
}

void test_native_geometry_fits_current_displays_and_observes_only_after_restore()
{
	using namespace bpr;
	qt::workbench_window                                window;
	std::vector<application::workbench_window_geometry> observations;
	window.set_geometry_dispatch([&](auto aGeometry) { observations.push_back(aGeometry); });
	window.show();
	process_events();
	expect(observations.empty(), "initial native events cannot replace unread geometry");
	window.publish(application::workbench_projection{ .Geometry = application::workbench_window_geometry{ 800000, -800000, 1400, 700, false } });
	process_events();
	expect(!observations.empty(), "restored placement enables bounded native observations");
	const auto available = QGuiApplication::primaryScreen()->availableGeometry();
	expect(available.contains(window.geometry().topLeft()) && window.width() <= std::max(window.minimumWidth(), available.width())
			   && window.height() <= std::max(window.minimumHeight(), available.height()),
		   "obsolete-display placement keeps the top edge and a bounded size reachable");
	window.move(available.left() + 20, available.top() + 55);
	window.resize(window.minimumWidth(), 500);
	process_events();
	const auto latest  = observations.back();
	const auto current = window.geometry();
	window.publish(application::workbench_projection{ .Geometry = application::workbench_window_geometry{ 800000, -800000, 1400, 700, false } });
	process_events();
	expect(window.geometry() == current && observations.back() == latest,
		   "a repeated restore cannot move an already active window");
	window.showMaximized();
	process_events();
	expect(observations.back().Maximized && observations.back().Width == latest.Width
			   && observations.back().Height == latest.Height,
		   "maximization preserves the normal placement");
	window.hide();
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	if (QCoreApplication::arguments().contains(QStringLiteral("--native-window-geometry"))) {
		// [[BPR-1074]]. The one window a launch puts on the desktop.
		test_building_the_window_shows_no_window_of_its_own();
		test_native_geometry_fits_current_displays_and_observes_only_after_restore();
		std::cout << "native-window-geometry:pass\n";
		return EXIT_SUCCESS;
	}
	if (QCoreApplication::arguments().contains(QStringLiteral("--shared-snapshot-navigation"))) {
		test_comments_filter_and_return_comparison_preferences_apply_on_open();
		std::cout << "shared-snapshot-navigation:pass\n";
		return EXIT_SUCCESS;
	}
	if (QCoreApplication::arguments().contains(QStringLiteral("--dif-workshop-placement"))) {
		test_composition_pages_retain_editors_and_never_submit_on_navigation();
		test_composition_keys_open_the_pages_and_esc_returns();
		test_tour_retarget_key_reaches_only_the_outline();
		test_dif_uses_the_corner_except_for_prime_spots();
		std::cout << "dif-workshop-placement:pass\n";
		return EXIT_SUCCESS;
	}
	if (QCoreApplication::arguments().contains(QStringLiteral("--document-tabs"))) {
		test_document_tabs_present_exact_views_and_translate_gestures();
		std::cout << "document-tabs:pass\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-0981]]. The declared focus order alone, at each registered scale:
	// the full run stops earlier at the epoch-row assertion [[BPR-0940]] owns.
	if (QCoreApplication::arguments().contains(QStringLiteral("--focus-order"))) {
		test_focus_order_is_declared_named_and_walkable();
		std::cout << "workbench-focus-order:pass\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-0653]]. The strip as the title row -- its order, its width sweep
	// down to the platform's floor and its bare-surface gestures -- reached
	// directly past the epoch-row assertion [[BPR-0940]] owns.
	if (QCoreApplication::arguments().contains(QStringLiteral("--title-row"))) {
		test_compact_icon_row_preserves_exact_order_and_edges();
		test_strip_identity_stays_at_every_width();
		test_the_review_bar_yields_dates_then_words();
		test_the_floor_follows_the_review_bar();
		test_restored_geometry_accepts_the_raised_floor();
		test_strip_is_the_title_row();
		std::cout << "workbench-title-row:pass\n";
		return EXIT_SUCCESS;
	}
#if defined(_WIN32)
	// [[BPR-1028]]. The Windows strip's application menu against its window's
	// lifetime: closed, it deletes itself; open, it goes with the window.
	if (QCoreApplication::arguments().contains(QStringLiteral("--strip-menu"))) {
		test_strip_menu_goes_with_its_window();
		test_strip_menu_opens_from_alt_and_f10();
		std::cout << "workbench-strip-menu:pass\n";
		return EXIT_SUCCESS;
	}
#endif
	// [[BPR-0990]]. Home's discovery glyph, sentence and the Pull Request menu,
	// reached directly past the epoch-row assertion [[BPR-0940]] owns.
	if (QCoreApplication::arguments().contains(QStringLiteral("--home-discovery"))) {
		test_home_strip_holds_group_state_headers_filter_and_refresh();
		test_home_discovery_glyph_status_and_pull_request_menu();
		// RFC-BPR-0026 AC-04: the window's own filter beside the profile's.
		test_home_filter_menu_and_its_gesture();
		std::cout << "workbench-home-discovery:pass\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-0985]]. The conversation courses, which the full run does not reach
	// past that same assertion: which thread moves code, and when.
	if (QCoreApplication::arguments().contains(QStringLiteral("--conversation-reveal"))) {
		test_discard_and_back_keep_the_comments_drawer();
		test_the_conversations_drawer_hosts_the_threaded_conversation();
		test_same_automatically_selected_line_click_filters_conversations();
		test_selected_file_filter_survives_diff_loading();
		test_conversation_selection_reveals_exact_diff_anchor();
		test_a_default_conversation_row_never_moves_code();
		test_conversation_line_outside_selected_comparison_is_explicit();
		test_failed_replacement_keeps_conversations_on_the_requested_file();
		test_refresh_gap_retains_only_visual_conversation_scope();
		test_refresh_without_a_selection_shows_review_wide_roots();
		test_discuss_uses_main_content_and_open_in_code_returns_to_read();
		test_a_reply_waits_for_its_threads_own_comparison();
		test_defect_queue_navigation_and_semantic_dispatch();
		test_thread_defect_menus_badges_and_navigation();
		std::cout << "workbench-conversation-reveal:pass\n";
		return EXIT_SUCCESS;
	}
	// [[BPR-0934]]. The window's Overview courses, which the full run does not
	// reach past the epoch-row assertion [[BPR-0940]] owns: the retained
	// folder tree must still skip equal overviews and re-project after a clear.
	if (QCoreApplication::arguments().contains(QStringLiteral("--overview"))) {
		test_overview_refuses_a_hand_opened_drawer_and_says_so();
		test_overview_entry_preserves_start_and_continue_semantics();
		test_identical_overview_skips_rebuild_but_restores_cleared_files();
		test_receipt_update_moves_only_its_rows();
		std::cout << "workbench-overview:pass\n";
		return EXIT_SUCCESS;
	}
	test_composition_pages_retain_editors_and_never_submit_on_navigation();
	test_composition_keys_open_the_pages_and_esc_returns();
	test_tour_retarget_key_reaches_only_the_outline();
	test_compose_review_marks_a_kept_draft();
	test_settings_collection_limit_keeps_accepted_models_and_recovery_actions();
	test_settings_share_only_offered_agent_context_and_maintenance_actions();
	test_all_four_primary_actions_follow_one_core_projection();
	test_every_primary_action_translates_to_a_semantic_intent();
	test_read_without_a_file_gives_direction_not_a_blank_diff();
	test_action_state_follows_core_command_projections();
	test_compact_icon_row_preserves_exact_order_and_edges();
	test_shell_accessibility_status_and_scale();
	test_toolbar_status_tips_do_not_resize_the_workbench();
	test_drawer_toggles_follow_core_command_projections();
	test_the_view_menu_offers_a_core_decided_line_number_toggle();
	test_the_view_menu_projects_one_pane_without_changing_modality();
	test_the_view_menu_reaches_the_composed_source_panes();
	test_one_pane_places_threads_on_their_own_changed_rows();
	test_collapsing_a_drawer_returns_space_to_the_centre();
	test_strip_modalities_positions_and_account();
	test_strip_identity_stays_at_every_width();
	test_the_review_bar_yields_dates_then_words();
	test_the_floor_follows_the_review_bar();
	test_restored_geometry_accepts_the_raised_floor();
	test_the_title_row_and_review_bar_render();
	test_the_updates_line_folds_from_the_review_bar();
	test_comment_names_its_target_and_every_card_replies();
	test_strip_is_the_title_row();
	test_the_epoch_row_spans_the_window_in_every_modality();
	test_rails_reopen_their_drawers();
	test_review_details_return_height_to_the_comparison();
	test_drawer_resize_emits_the_host_layout_path();
	test_a_squeezed_splitter_never_reports_a_width();
	test_a_clobbered_persisted_width_opens_at_the_design_width();
	test_the_conversations_drawer_narrows_below_its_default_in_read();
	test_the_seam_is_centred_for_native_and_even_handle_widths();
	test_the_split_cursor_stays_over_a_resting_handle();
	test_drawer_contents_use_incremental_model_signals();
	test_file_rows_emit_only_semantic_user_selection();
	test_the_files_drawer_shows_the_tree_over_its_list();
	test_the_conversations_drawer_hosts_the_threaded_conversation();
	test_the_centre_is_the_composed_rfc_0004_comparison_surface();
	test_same_automatically_selected_line_click_filters_conversations();
	test_selected_file_filter_survives_diff_loading();
	test_conversation_selection_reveals_exact_diff_anchor();
	test_a_default_conversation_row_never_moves_code();
	test_conversation_line_outside_selected_comparison_is_explicit();
	test_failed_replacement_keeps_conversations_on_the_requested_file();
	test_refresh_gap_retains_only_visual_conversation_scope();
	test_refresh_without_a_selection_shows_review_wide_roots();
	test_composed_ribbons_follow_the_group_model_through_scroll_and_resize();
	test_the_lower_detail_pane_is_full_width_line_over_line();
	test_composed_epoch_selectors_show_exact_retained_epochs();
	test_review_history_state_is_explicit_in_workbench_text();
	test_unavailable_saved_layout_survives_workbench_projections();
	test_focus_order_is_declared_named_and_walkable();
	test_receipt_actions_follow_decided_projection_and_clear_while_loading();
	test_selection_synchronisation_and_restart_persistence();
	test_read_state_reaches_the_gutter_and_the_strip();
	test_the_right_tray_holds_comments_and_notes_tabs();
	test_discuss_uses_main_content_and_open_in_code_returns_to_read();
	test_a_reply_waits_for_its_threads_own_comparison();
	test_defect_queue_navigation_and_semantic_dispatch();
	test_thread_defect_menus_badges_and_navigation();
	test_queue_rule_gestures_wait_for_the_accepted_projection();
	test_queue_rule_selection_survives_unrelated_projection();
	test_saved_repository_gestures_round_trip_through_the_dispatch();
	test_add_enterprise_server_opens_a_card_in_its_edit_state();
	test_github_cli_discovery_is_a_separate_accounts_path();
	test_a_kept_enterprise_server_remembers_and_forgets_on_its_card();
	test_the_servers_combo_editor_is_gone();
	test_review_behaviour_rows_dispatch_follow_the_projection_and_reset_by_group();
	test_hidden_drawer_controls_leave_no_column_and_no_hide_button();
	test_continue_review_and_opening_honour_the_modality_preferences();
	test_comments_filter_and_return_comparison_preferences_apply_on_open();
	test_mark_read_when_fully_seen_dispatches_once_at_the_end_of_the_comparison();
	test_queue_filter_controls_are_accessible_incremental_and_focus_stable();
	test_notification_activation_selects_only_an_exact_visible_queue_row();
	test_notification_settings_wait_for_acceptance_and_reset_only_their_domain();
	test_large_queue_filters_through_the_composed_window();
	// [[BPR-0783]]. Home's strip controls, its Filter menu, the window's own view
	// state, and the six typed controls on Settings' Queue defaults.
	test_home_strip_holds_group_state_headers_filter_and_refresh();
	test_home_filter_menu_and_its_gesture();
	test_a_published_home_view_is_restored_without_a_second_dispatch();
	test_the_six_queue_controls_live_on_settings_queue_defaults();
	test_home_captures_every_accepted_view();
	test_home_composes_the_production_catalog_and_respects_motion_and_width();
	test_dif_uses_the_corner_except_for_prime_spots();
	test_repositories_drawer_toggles_through_the_core_and_collapses_to_a_rail();
	test_overview_entry_preserves_start_and_continue_semantics();
	test_identical_overview_skips_rebuild_but_restores_cleared_files();
	test_receipt_update_moves_only_its_rows();
	test_overview_refuses_a_hand_opened_drawer_and_says_so();
	test_a_large_file_leaves_the_widget_count_bounded();
	std::cout << "workbench-window-tests:pass\n";
	return EXIT_SUCCESS;
}
