#include "bpr/core/file_progress_mark.hpp"
#include "bpr/core/review_comment_range.hpp"
#include "bpr/core/suggestion_draft.hpp"
#include "bpr/core/thread_reply.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/compose_review_sheet.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/home_filter_field.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/overview_tour_summary.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "conversation_glyphs.hpp"
#include "drawer_tab_bar.hpp"
#include "workbench_window_support.hpp"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAction>
#include <QAnyStringView>
#include <QApplication>
#include <QBoxLayout>
#include <QBrush>
#include <QChar>
#include <QClipboard>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QEvent>
#include <QFlags>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLatin1Char>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QListSpecialMethods>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMargins>
#include <QMenu>
#include <QMetaObject>
#include <QModelIndex>
#include <QNonConstOverload>
#include <QObject>
#include <QOverload>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVariant>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	namespace {
		/// \brief [[BPR-0775]]. Home's bar: its label, the field and the shortcut
		/// inside one bordered field with one 8-point radius.
		///
		/// \details Painted from the palette rather than a stylesheet. The rule
		/// takes Highlight while the field has focus, so the focus ring is the
		/// frame's rather than a second ring inside it.
		class home_bar_frame final : public QWidget
		{
		public:
			explicit home_bar_frame(QWidget* const aParent)
				: QWidget{ aParent }
			{
			}

			void watch(QWidget* const aField)
			{
				Field = aField;
				aField->installEventFilter(this);
			}

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				static_cast<void>(aEvent);
				auto       painter = QPainter{ this };
				const auto focused = Field != nullptr && Field->hasFocus();
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(QPen{ palette().color(QPalette::Active, focused ? QPalette::Highlight : QPalette::Mid), focused ? 2.0 : 1.0 });
				painter.setBrush(palette().color(QPalette::Active, QPalette::Base));
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0), 8.0, 8.0);
			}

			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aWatched == Field && aEvent != nullptr
					&& (aEvent->type() == QEvent::FocusIn || aEvent->type() == QEvent::FocusOut)) {
					update();
				}
				return QWidget::eventFilter(aWatched, aEvent);
			}

		private:
			QPointer<QWidget> Field;
		};

		/// \brief [[BPR-1381]]. The bar's own word that what it was given is
		/// opening: the rows' turning mark and "Opening…" where the shortcut
		/// stood, in the field's quiet ink.
		///
		/// \details What was asked is answered where it was asked, as a row
		/// answers on its row; the status band at Home's foot says the rest.
		class home_bar_activity final : public QWidget
		{
		public:
			explicit home_bar_activity(QWidget* const aParent)
				: QWidget{ aParent }
			{
				setObjectName(QStringLiteral("homeOpenActivity"));
				setAccessibleName(QStringLiteral("Opening"));
				setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
				Turn = new QTimer{ this };
				Turn->setInterval(80);
				connect(Turn, &QTimer::timeout, this, [this] {
					Frame = (Frame + 1) % home_open_frames;
					update();
				});
				hide();
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto metrics = QFontMetrics{ font() };
				return { metrics.height() + gap + metrics.horizontalAdvance(text()), metrics.height() };
			}

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				static_cast<void>(aEvent);
				auto       painter = QPainter{ this };
				const auto extent  = QFontMetrics{ font() }.height();
				const auto ink     = palette().color(QPalette::Active, QPalette::PlaceholderText);
				auto       faint   = ink;
				faint.setAlphaF(0.25F);
				painter.setRenderHint(QPainter::Antialiasing, true);
				draw_home_open_mark(painter, QRect{ 0, (height() - extent) / 2, extent, extent }, Frame, ink, faint);
				painter.setPen(ink);
				painter.drawText(QRect{ extent + gap, 0, width() - extent - gap, height() }, Qt::AlignVCenter | Qt::AlignLeft, text());
			}

			void showEvent(QShowEvent* const aEvent) override
			{
				QWidget::showEvent(aEvent);
				Turn->start();
			}

			void hideEvent(QHideEvent* const aEvent) override
			{
				QWidget::hideEvent(aEvent);
				Turn->stop();
			}

		private:
			static constexpr int gap = 6;

			[[nodiscard]] static auto text() -> QString
			{
				return QStringLiteral("Opening…");
			}

			QTimer* Turn  = nullptr;
			int     Frame = 0;
		};
	}

	void workbench_window::build_central_surface()
	{
		CentralSplitter = new workbench_central_splitter{ this };
		CentralSplitter->setObjectName(QStringLiteral("workbenchCentralSplitter"));
		CentralSplitter->setAccessibleName(QStringLiteral("Review workbench"));
		CentralSplitter->setAccessibleDescription(QStringLiteral(
			"Resizable file drawer, comparison centre, and comments drawer"));
		// A drawer leaves through the core's drawer state, never through a drag
		// to zero width that presentation decided by itself.
		CentralSplitter->setChildrenCollapsible(false);

		CentreSurface = new QWidget;
		CentreSurface->setObjectName(QStringLiteral("workbenchCentralSurface"));
		CentreSurface->setAccessibleName(QStringLiteral("Review comparison"));
		CentreSurface->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

		// RFC-BPR-0003's review-history control is composed unchanged above the
		// panes. It already owns the independent left and right selectors over
		// arbitrary retained epochs, the complete copyable commit OIDs, and the
		// review-history loading, failure, cancellation, and empty wording, so
		// the shell adds no second selector, OID presentation, or vocabulary.
		// [[BPR-0737]] (owner, 2026-09-11): the epoch line and the "Comparing"
		// menu are one full-width row between the strip and the drawers, not
		// the centre pane's first widget, so its labels no longer move with
		// the drawer widths and the modality. The review surface is that row
		// over the central splitter; Home replaces the whole surface.
		ReviewSurface = new QWidget;
		ReviewSurface->setObjectName(QStringLiteral("workbenchReviewSurface"));
		EpochSelectors = new epoch_comparison_control{ { }, ReviewSurface };
		// An ignored horizontal hint, exactly as the drawer heading uses. The
		// row must never impose a minimum width that the drawer widths the
		// host persists cannot coexist with, and the window must stay usable at
		// the supported compact icon-row width.
		EpochSelectors->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		auto* const review = new QVBoxLayout{ ReviewSurface };
		review->setContentsMargins(0, 0, 0, 0);
		review->setSpacing(0);
		// Readiness uses the Updates track or the folded comparison summary,
		// so loading never inserts a row above the comparison.
		review->addWidget(ReviewBar);
		review->addWidget(EpochSelectors);
		review->addWidget(CentralSplitter, 1);
		if (auto* const comparing = EpochSelectors->release_comparing_button(); comparing != nullptr) {
			comparing->setAutoRaise(true);
			comparing->setParent(ReviewBar);
			ReviewBar->insertWidget(FilePositionAction, comparing)->setObjectName(QStringLiteral("reviewBarComparingAction"));
			ReviewBar->insertWidget(FilePositionAction, new strip_divider{ ReviewBar })
				->setObjectName(QStringLiteral("reviewBarComparisonDividerAction"));
			if (auto* const menu = comparing->menu(); menu != nullptr) {
				menu->addSeparator();
				menu->addAction(ReviewDetailsAction);
			}
		}
		// RFC-BPR-0004's workbench is composed unchanged. It keeps the single
		// shared vertical scrollbar, the single link map, and the natural-layout
		// documents, so the centre gains no second mapping, no regrouping, no
		// follower-scroll coupling, and no synthetic alignment rows.
		ComparisonSurface = new diff_workbench{ CentreSurface };
		ComparisonSurface->set_source_find_changed([this] { update_source_find_actions(); });
		// [[BPR-1724]]. Comment names the lines a drag or a Shift-click
		// selects, which change no selected line; once a turn, not per move.
		auto* const commentTarget = new QTimer{ this };
		commentTarget->setSingleShot(true);
		commentTarget->setInterval(0);
		connect(commentTarget, &QTimer::timeout, this, [this] { update_comment_target(); });
		// [[BPR-1724]]. The one-pane comparison has the same menu.
		for (auto* pane : { ComparisonSurface->left_pane(), ComparisonSurface->right_pane(), ComparisonSurface->unified_pane() }) {
			connect(pane, &QPlainTextEdit::selectionChanged, commentTarget, qOverload<>(&QTimer::start));
			pane->setContextMenuPolicy(Qt::CustomContextMenu);
			connect(pane, &QWidget::customContextMenuRequested, this, [this, pane](const QPoint& aPosition) {
				auto* const menu = code_context_menu(pane, aPosition);
				menu->setAttribute(Qt::WA_DeleteOnClose);
				menu->popup(pane->viewport()->mapToGlobal(aPosition));
			});
		}
		auto* const centre = new QVBoxLayout{ CentreSurface };
		centre->setContentsMargins(0, 0, 0, 0);
		// [[BPR-0646]]. Home's landing, above the epoch line while there is
		// nothing to compare and no pull request: it funnels a reviewer to
		// the account or to a pull request rather than showing empty panes.
		// [[BPR-0646]] [[BPR-0650]]. Home is the whole centre while nothing
		// is open: the landing card and the review queue, and none of the
		// review chrome that means nothing before a pull request is chosen.
		HomePage = new QWidget;
		HomePage->setObjectName(QStringLiteral("workbenchHome"));
		HomePage->setAccessibleName(QStringLiteral("Home"));
		HomePage->setAccessibleDescription(QStringLiteral(
			"The bar, then the first-launch card or the way back to an open "
			"review, and the local review queue"));
		auto* const home = new QVBoxLayout{ HomePage };
		// [[BPR-0775]]. The bottom inset keeps Home's rows clear of Dif resting
		// in the corner beneath them.
		home->setContentsMargins(0, 0, 0, 16);
		home->setSpacing(0);
		HomeColumn  = home;
		HomeLanding = new workbench_landing{ HomePage };
		// [[BPR-0775]], [[BPR-0909]]. The card's three explicit ways in and the
		// row's way back are all it offers.
		HomeLanding->set_primary_dispatch([this] {
			if (HomeLanding->state() == landing_state::back_to_review) {
				choose_home(false);
				return;
			}
			if (HomeLanding->state() == landing_state::sign_in) {
				begin_selected_sign_in();
			}
		});
		HomeLanding->set_secondary_dispatch([this] {
			if (HomeLanding->state() == landing_state::sign_in) {
				begin_github_cli_from_home();
			}
		});
		HomeLanding->set_tertiary_dispatch([this] {
			if (HomeLanding->state() == landing_state::sign_in) {
				begin_enterprise_server_from_home();
			}
		});
		HomeLanding->set_close_dispatch([this] { close_review(); });
		home->addWidget(HomeLanding);
		// [[BPR-0647]]. The centre is a stack: the
		// overview for the Overview modality, the comparison for the rest.
		// The comparison keeps a page of its own with no margins, so every
		// geometry the probes pin on it is what it was.
		CentreStack = new QStackedWidget{ CentreSurface };
		CentreStack->setObjectName(QStringLiteral("workbenchCentreStack"));
		ComparisonPage = new QWidget{ CentreStack };
		ComparisonPage->setObjectName(QStringLiteral("workbenchComparisonPage"));
		auto* const comparisonColumn = new QVBoxLayout{ ComparisonPage };
		comparisonColumn->setContentsMargins(0, 0, 0, 0);
		comparisonColumn->setSpacing(0);
		// [[BPR-1132]]. RFC-BPR-0052: the comparison's header is the document
		// tab strip. It takes the diff status row's place, and says the status
		// at its right while the status has more to say than "ready".
		DocumentTabs = new document_tab_strip{ ComparisonPage };
		DocumentTabs->set_dispatch([this](const core::document_tab_intent& aIntent) {
			if (DocumentTabDispatch) {
				DocumentTabDispatch(aIntent);
			}
		});
		comparisonColumn->addWidget(DocumentTabs);
		// RFC-BPR-0089 M3. Over a split file's diff, its per-commit diffs; it
		// takes no room while hidden, so every other diff keeps its geometry.
		SplitStrip = new split_diff_strip{ ComparisonPage };
		comparisonColumn->addWidget(SplitStrip);
		comparisonColumn->addWidget(ComparisonSurface, 1);
		update_idle_direction();
		ComparisonSurface->set_status_observer([this](const QString& aText, const bool aNotable) {
			if (DocumentTabs != nullptr) {
				// [[BPR-1287]]. With nothing chosen the tab says so in the
				// empty state's words, not "Select a file comparison".
				DocumentTabs->set_status(ComparisonSurface->idle() ? ComparisonSurface->idle_page()->accessibleName() : aText, aNotable);
			}
			// [[BPR-1197]]. With no source text to read, the empty comparison
			// keeps the complete, wrapping and copyable explanation. The compact
			// tab status can elide; text comparisons keep their full reading area.
			const auto& result = ComparisonSurface->current_result();
			ComparisonSurface->set_status_shown(aNotable
												&& (!result || result->Alignment.Content != core::diff_content_state::text));
		});
		OverviewPage = new pull_request_overview_page{ CentreStack };
		// [[BPR-1588]]. Status's tab row carries Open on GitHub too.
		if (OpenOnGithubAction != nullptr) {
			OverviewPage->set_open_on_github_action(OpenOnGithubAction);
		}
		// [[BPR-1588]]. Alt+Shift+6 collapses or expands Status while the
		// Overview shows, as the drawers' keys do elsewhere.
		ToggleStatusDrawerAction = new QAction{ tr("Status drawer"), this };
		ToggleStatusDrawerAction->setObjectName(QStringLiteral("toggleOverviewStatusAction"));
		ToggleStatusDrawerAction->setShortcuts(shortcut_keys(shortcut_command::status_drawer));
		ToggleStatusDrawerAction->setShortcutContext(Qt::WindowShortcut);
		ToggleStatusDrawerAction->setToolTip(tr("Hide or show the Status drawer (%1)").arg(shortcut_key(shortcut_command::status_drawer).toString(QKeySequence::NativeText)));
		ToggleStatusDrawerAction->setEnabled(false);
		addAction(ToggleStatusDrawerAction);
		connect(ToggleStatusDrawerAction, &QAction::triggered, this, [this] {
			if (OverviewPage != nullptr && CentreStack != nullptr && CentreStack->currentWidget() == OverviewPage) {
				OverviewPage->status_drawer()->toggle();
			}
		});
		OverviewPage->set_file_dispatch([this](const QString& aPath) { enter_review_at(aPath); });
		// [[BPR-1589]]. My review's Go to them: Threads, with Only new on.
		OverviewPage->set_new_threads_dispatch([this] { show_new_threads(); });
		// Its conversation line opens Discuss on the pull request.
		OverviewPage->set_new_conversation_dispatch([this] {
			if (Modality != nullptr && HasGithubPullRequest) {
				choose_home(false);
				choose_modality(review_modality::discuss);
			}
		});
		CentreStack->addWidget(ComparisonPage);
		CentreStack->addWidget(OverviewPage);
		CentreStack->setCurrentWidget(ComparisonPage);
		centre->addWidget(CentreStack, 1);

		Drawers[0].Panel = new workbench_drawer_panel{ core::workbench_drawer::files };
		Drawers[1].Panel = new workbench_drawer_panel{
			core::workbench_drawer::conversations
		};
		// [[BPR-1290]]. The inline drawer drags from 280 to 760 pixels, the
		// owner's accepted prototype's range; it opens at 375.
		Drawers[1].Panel->set_expanded_width_bounds(conversation_drawer_minimum_width, conversation_drawer_maximum_width);
		// Its header is its tabs: the drawer's name is its accessible name only.
		Drawers[1].Panel->set_heading_shown(false);
		// [[BPR-0651]]. The repositories tree: each repository the queue names
		// with its pull requests of interest; choosing one opens it on its
		// overview through the same intent the locator sends.
		Drawers[2].Panel = new workbench_drawer_panel{
			core::workbench_drawer::repositories
		};
		// [[BPR-0981]]. Its filter above it, resident over the rows the queue
		// projection already gave the tree; nothing here reaches a provider.
		auto* const repositoriesSurface = new QWidget;
		repositoriesSurface->setObjectName(QStringLiteral("workbenchRepositoriesSurface"));
		auto* const repositoriesColumn = new QVBoxLayout{ repositoriesSurface };
		repositoriesColumn->setContentsMargins(0, 0, 0, 0);
		repositoriesColumn->setSpacing(2);
		RepositoriesFilter = new QLineEdit{ repositoriesSurface };
		RepositoriesFilter->setObjectName(QStringLiteral("repositoriesFilter"));
		RepositoriesFilter->setAccessibleName(QStringLiteral("Filter pull requests"));
		RepositoriesFilter->setAccessibleDescription(QStringLiteral(
			"Show only repositories and pull requests whose name, title or number "
			"contains these letters in order; a matching pull request keeps its repository"));
		RepositoriesFilter->setPlaceholderText(QStringLiteral("Filter"));
		RepositoriesFilter->setClearButtonEnabled(true);
		repositoriesColumn->addWidget(RepositoriesFilter);
		RepositoriesFilterNotice = new filter_empty_notice{
			QStringLiteral("pull requests"), QStringLiteral("repositories"), repositoriesSurface
		};
		RepositoriesFilterNotice->hide();
		repositoriesColumn->addWidget(RepositoriesFilterNotice);
		RepositoriesTree = new repositories_tree_view{ repositoriesSurface };
		repositoriesColumn->addWidget(RepositoriesTree, 1);
		RepositoriesTree->set_saved_repository_dispatch(
			[this](application::saved_repository_intent aIntent) {
				if (SavedRepositoryDispatch && QueueReady) {
					SavedRepositoryDispatch(std::move(aIntent));
				}
			});
		RepositoriesTree->set_open_dispatch([this](const core::queue_item& aItem) {
			open_pull_request_item(aItem);
		});
		connect(RepositoriesFilter, &QLineEdit::textChanged, this, [this](const QString& aText) {
			RepositoriesTree->set_filter(aText);
			refresh_filter_notices();
		});
		connect(RepositoriesFilterNotice->clear_button(), &QAbstractButton::clicked, this, [this] {
			RepositoriesFilter->clear();
		});
		Drawers[2].Panel->set_content_widget(repositoriesSurface, RepositoriesTree);
		// [[BPR-0646]]. A rail is the collapsed drawer's one control, and
		// pressing it is the same gesture as the strip's toggle: the action
		// is triggered, and the core decides.
		for (std::size_t index = 0U; index < Drawers.size(); ++index) {
			connect(Drawers[index].Panel->rail(),
					&QAbstractButton::clicked,
					this,
					[this, index] { Drawers[index].Toggle->trigger(); });
			// [[BPR-0675]]. And the same gesture from the drawer's own
			// control, which is where a reviewer looks for it.
			connect(Drawers[index].Panel->collapse_button(),
					&QAbstractButton::clicked,
					this,
					[this, index] { Drawers[index].Toggle->trigger(); });
		}

		// Route 2 of BPR-0191. The conversations drawer shows RFC-BPR-0002's
		// threaded review conversation, which is a forest and cannot be a list,
		// so the drawer keeps its collapse, width and toggle behaviour and hosts
		// the tree instead of its own list model. The files drawer is unchanged:
		// a file list is a list.
		ConversationPanel = new github_conversation_panel{ Drawers[1].Panel };
		ConversationPanel->set_current_endpoint_placement(false, false);
		// [[BPR-0648]]. The cards read one geometry from the comparison: where
		// a line is. The window relays it, and relays every applied scroll
		// frame, so the cards sit at their lines and follow the shared scroll
		// without the drawer ever reading the panes itself.
		ConversationPanel->cards()->set_line_locator(
			[this](const core::diff_side aSide, const std::uint32_t aOneBasedLine)
				-> std::optional<int> {
				if (ComparisonSurface == nullptr || aOneBasedLine == 0U
					|| ConversationPanel == nullptr) {
					return std::nullopt;
				}
				const auto rect = ComparisonSurface->line_rect(aSide, aOneBasedLine - 1U);
				if (rect.isEmpty()) {
					return std::nullopt;
				}
				const auto inWindow = ComparisonSurface->mapTo(this, rect.topLeft());
				return ConversationPanel->cards()->viewport()->mapFrom(this, inWindow).y();
			});
		ComparisonSurface->set_frame_observer([this] { refresh_cards(); });
		// [[BPR-1290]]. The gutter's markers and the scroll bar's ticks follow
		// the open thread and the note under the pointer; a marker opens its
		// thread, and Shift-click on a tick goes to its thread. A wheel over
		// the notes scrolls the code.
		ComparisonSurface->set_thread_markers(true);
		ComparisonSurface->set_line_mark_dispatch([this](const core::diff_side aSide, const std::uint32_t aLine) { toggle_thread_at(aSide, aLine); });
		// [[BPR-1212]], [[BPR-1335]]. The "+" button selects its line, or the
		// lines a drag on it covered, as a click or a drag in the code would,
		// and starts a comment there. The core decides whether the lines can
		// take one; the status bar says why not.
		ComparisonSurface->set_line_add_dispatch([this](const std::uint32_t aFirst, const std::uint32_t aLast) {
			const auto& result = ComparisonSurface->current_result();
			const auto  head   = [&result](const std::uint32_t aLine) {
				return core::source_line_ref{ .Comparison = result->Alignment.Digest, .Side = core::diff_side::right, .Line = aLine };
			};
			const auto selected = aFirst == aLast
				? ComparisonSurface->select_conversation_line(core::diff_side::right, aLast)
				: result != nullptr && ComparisonSurface->restore_source_range(head(aFirst), head(aLast));
			if (selected) {
				static_cast<void>(comment_on_selected_lines(false));
			}
		});
		// [[BPR-1212]]. Return and Shift+Return in the code (owner, Q1): the
		// pane takes them only while it has focus, and they start the same
		// comment the right-click items do.
		ComparisonSurface->set_line_draft_dispatch([this](const bool aSuggest) { static_cast<void>(comment_on_selected_lines(aSuggest)); });
		ComparisonSurface->set_thread_tick_dispatch([this](const core::diff_side aSide, const std::uint32_t aLine) { go_to_thread_at(aSide, aLine); });
		// [[BPR-1336]]. ⌘-scroll and pinch take the same path as the menu.
		ComparisonSurface->set_code_zoom_dispatch([this](const core::code_zoom_step aStep) { dispatch_code_zoom(aStep); });
		ConversationPanel->cards()->set_marks_observer([this] {
			apply_thread_marks();
			present_reply_placement();
		});
		ConversationPanel->cards()->set_scroll_forward([this](QWheelEvent& aEvent) {
			return ComparisonSurface != nullptr && ComparisonSurface->scroll_by_wheel(aEvent);
		});
		ConversationPanel->cards()->set_reply_text_dispatch([this](const std::string& aThread, const QString& aText) {
			if (!dispatch_conversation_reply(core::conversation_reply_action::edit, aThread, aText)) {
				write_inline_reply(aThread, aText);
			}
		});
		ConversationPanel->cards()->set_draft_text_dispatch([this](const QString& aText) { write_inline_draft(aText); });
		// [[BPR-0661]]. A file marks itself read once fully seen only when the
		// preference says so; the shared scrollbar is what says "fully".
		if (auto* const scrollbar = ComparisonSurface->shared_scrollbar(); scrollbar != nullptr) {
			connect(scrollbar, &QScrollBar::valueChanged, this, [this](int) { consider_mark_read_when_fully_seen(); });
			connect(scrollbar, &QScrollBar::rangeChanged, this, [this](int, int) { consider_mark_read_when_fully_seen(); });
		}
		// [[BPR-1699]]. A thread's or a post's Mark all from its author
		// as read goes where Mark All Comments as Read does, with the login.
		ConversationPanel->cards()->set_author_read_dispatch([this](const std::string& aLogin) {
			const auto dispatch = ConversationsReadDispatch;
			if (dispatch) {
				dispatch(aLogin);
			}
		});
		// [[BPR-1741]]. A post's reaction travels with what the window
		// shows of it: added when the viewer has not made it, removed when
		// they have; the composition decides.
		ConversationPanel->cards()->set_reaction_dispatch([this](const std::string& aPost, const core::reaction_content aContent) {
			const auto dispatch = ReactionDispatch;
			if (!dispatch || ConversationPanel == nullptr) {
				return;
			}
			const auto shown = ConversationPanel->cards()->reactions_of(aPost);
			auto       node  = aPost.substr(aPost.find(':') == std::string::npos ? 0U : aPost.find(':') + 1U);
			dispatch(reaction_gesture{
				.Post           = aPost,
				.Subject        = std::move(node),
				.Content        = aContent,
				.Add            = core::reaction_toggle(shown, aContent),
				.Observed       = shown.has_value(),
				.ViewerCanReact = shown.has_value() && shown->ViewerCanReact,
			});
		});
		ConversationPanel->cards()->set_action_dispatch(
			[this](const card_action aAction, const std::string& aIdentity) {
				// [[BPR-1046]]. A reply still waiting for its comparison is
				// given up by any later card gesture.
				DeferredReply.reset();
				// [[BPR-1326]]. Publish, Edit and Delete act on the editor's
				// draft. On another line's saved draft they first choose that
				// line, which brings its draft into the editor; nothing else
				// is published, opened or deleted.
				if ((aAction == card_action::publish_draft || aAction == card_action::edit_draft || aAction == card_action::delete_draft)
					&& aIdentity != "draft:unpublished") {
					const auto& cards = ConversationPanel->cards()->cards();
					const auto  card  = std::ranges::find(cards, aIdentity, &thread_card::Id);
					if (card != cards.end() && card->Draft && card->Side.has_value() && card->Line.value_or(0U) > 0U && ComparisonSurface != nullptr) {
						const auto side = *card->Side;
						const auto line = *card->Line - 1U;
						static_cast<void>(ComparisonSurface->select_conversation_line(side, line));
						// [[BPR-1212]]. Opening it writes it in the drawer.
						if (aAction == card_action::edit_draft && ReviewEditor != nullptr && ReviewEditor->shown_draft().has_value()
							&& ReviewEditor->shown_draft()->Anchor.Side == side && ReviewEditor->shown_draft()->Anchor.Line == line) {
							open_review_composition(true);
						}
					}
					return;
				}
				switch (aAction) {
				case card_action::keep_reply_as_comment:
				case card_action::discard_reply:
					act_on_kept_reply(aAction, aIdentity);
					break;
				case card_action::copy_reply:
					// [[BPR-1374]]. The notes copied the words themselves.
					break;
				case card_action::defect:
					ConversationPanel->cards()->show_thread_menu(aIdentity);
					break;
				case card_action::convert_draft:
					// [[BPR-1735]]. On a comment GitHub cannot place in the
					// review, the held action is Publish as file comment.
					if (const auto& shown = ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::nullopt;
						shown.has_value() && UnplaceableComments.contains(shown->Id) && !HeldSuggestions.contains(shown->Id)) {
						if (ReviewEditor->publish_action()->isEnabled()) {
							ReviewEditor->publish_action()->trigger();
						}
						break;
					}
					convert_pane_suggestion();
					break;
				case card_action::carry_suggestion:
					carry_moved_suggestion(aIdentity);
					break;
				case card_action::copy_change:
					// [[BPR-1733]]. The note copied the change itself.
					statusBar()->showMessage(tr("Copied the suggested lines."), 4000);
					break;
				case card_action::publish_draft:
					// [[RFC-BPR-0090]]. A held suggestion goes nowhere until it
					// is converted or deleted.
					if (const auto& shown = ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::nullopt; shown.has_value() && HeldSuggestions.contains(shown->Id)) {
						statusBar()->showMessage(HeldSuggestions.at(shown->Id).Reason, 10000);
						break;
					}
					// [[BPR-1212]]. Publish posts this one comment now; the
					// drawer closes the composer once the draft is published.
					if (ReviewEditor != nullptr && ReviewEditor->publish_action()->isEnabled()) {
						ReviewEditor->publish_action()->trigger();
					}
					break;
				case card_action::edit_draft:
					// [[BPR-1212]]. Opening a kept draft makes it editable again,
					// in the drawer.
					if (ReviewEditor != nullptr) {
						open_review_composition(true);
					}
					break;
				case card_action::delete_draft:
					// [[BPR-1345]] v002. Delete asks first when there are
					// words to lose.
					confirm_delete_drawer_draft();
					break;
				case card_action::finish_review:
					if (FinishReviewAction != nullptr && FinishReviewAction->isEnabled()) {
						FinishReviewAction->trigger();
					}
					break;
				case card_action::maximize_draft:
					maximize_drawer_draft();
					break;
				case card_action::close_draft:
					leave_drawer_draft(false);
					break;
				case card_action::review_draft:
					if (const auto& shown = ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::nullopt; shown.has_value() && HeldSuggestions.contains(shown->Id)) {
						statusBar()->showMessage(HeldSuggestions.at(shown->Id).Reason, 10000);
						break;
					}
					// [[BPR-1345]]. Start a review, or Add review comment: the
					// draft stays as written, pending in the review, and the
					// composer closes on it. Nothing is published.
					if (StartReviewDispatch && StartReviewDispatch()) {
						leave_drawer_draft(false);
					}
					break;
				case card_action::mark_read:
					// [[BPR-0656]]. The ledger host advances the cursor and
					// the read state comes back through present_read_state.
					if (ConversationReadDispatch) {
						ConversationReadDispatch(aIdentity);
					}
					break;
				case card_action::mark_unread:
					if (ConversationUnreadDispatch) {
						ConversationUnreadDispatch(aIdentity);
					}
					break;
				case card_action::resolve:
				case card_action::unresolve: {
					// [[BPR-0657]]. What the window knows travels with the
					// gesture: the thread's state as projected and the server's
					// capability for the direction asked; the composition adds
					// what only it knows and decides.
					if (!ThreadResolutionDispatch) {
						break;
					}
					const auto resolve  = aAction == card_action::resolve;
					auto       resolved = false;
					if (const auto index = ConversationPanel->model()->index_for_id(aIdentity); index.isValid()) {
						resolved = index.data(github_conversation_model::resolved_role).toBool();
					}
					ThreadResolutionDispatch(thread_resolution_gesture{
						.Identity   = aIdentity,
						.Resolve    = resolve,
						.Resolved   = resolved,
						.Capability = resolve ? Capabilities.ResolveThread : Capabilities.UnresolveThread,
					});
					break;
				}
				case card_action::reply:
					if (dispatch_conversation_reply(core::conversation_reply_action::open, aIdentity)) {
						break;
					}
					// [[BPR-1133]]. Beside the code as in Discuss: every card
					// answers its own thread, through the one Reply path;
					// [[BPR-1290]] written in place, in the open thread.
					reply_to_thread(aIdentity, true);
					break;
				case card_action::ask_agent:
					if (ConversationAskDispatch) {
						ConversationAskDispatch(aIdentity);
					}
					break;
				case card_action::show_line:
					reveal_thread_line(aIdentity);
					break;
				case card_action::publish_reply:
					if (dispatch_conversation_reply(core::conversation_reply_action::send, aIdentity)) {
						break;
					}
					// [[BPR-1290]]. The reply in place is the draft editor's
					// draft; nothing publishes but its own Publish.
					if (ReviewEditor != nullptr && ReviewEditor->publish_action()->isEnabled()) {
						ReviewEditor->publish_action()->trigger();
					}
					break;
				case card_action::cancel_reply:
					if (dispatch_conversation_reply(core::conversation_reply_action::close, aIdentity)) {
						ConversationPanel->cards()->close_thread();
						break;
					}
					// Cancel gives up what was typed, through the editor's own Discard.
					if (ReviewEditor != nullptr && ReviewEditor->shown_draft().has_value()
						&& ReviewEditor->shown_draft()->ReplyToThread.has_value()
						&& !ReviewEditor->body_editor()->toPlainText().isEmpty()
						&& ReviewEditor->discard_action()->isEnabled()) {
						ReviewEditor->discard_action()->trigger();
					}
					break;
				case card_action::open_in_code:
					// [[BPR-1383]]. The same exact pick as Threads: open
					// Comments and retain the thread until its file loads.
					// [[BPR-1581]]: through the one route, so an outdated
					// thread opens among its file's unplaced ones.
					static_cast<void>(move_thread(aIdentity, core::workbench_thread_origin::discuss, core::workbench_thread_destination::review));
					break;
				case card_action::show_in_discuss:
					// [[BPR-1581]]. The thread, or the post its menu was on.
					static_cast<void>(move_thread(aIdentity, core::workbench_thread_origin::review, core::workbench_thread_destination::discuss));
					break;
				case card_action::show_in_threads:
					// [[BPR-1586]]. Its row in Threads, at that post.
					static_cast<void>(move_thread(aIdentity, core::workbench_thread_origin::review, core::workbench_thread_destination::threads));
					break;
				}
			});
		ConversationPanel->cards()->set_excerpt_locator(
			[this](const QString& aPath, const core::diff_side aSide, const std::uint32_t aLine)
				-> std::optional<QString> {
				// Only the compared file's text is on hand: one line each
				// side of the thread's, from the pane on the thread's side.
				if (ComparisonSurface == nullptr || ComparisonSurface->current_result() == nullptr
					|| !ComparisonSurface->current_context().has_value()
					|| ComparisonSurface->current_context()->Key.File != core::file_identity_of(aPath.toStdString())) {
					return std::nullopt;
				}
				auto* const pane = aSide == core::diff_side::left ? ComparisonSurface->left_pane()
																  : ComparisonSurface->right_pane();
				if (pane == nullptr || pane->document() == nullptr) {
					return std::nullopt;
				}
				auto lines = QStringList{ };
				for (auto number = static_cast<int>(aLine) - 2; number <= static_cast<int>(aLine); ++number) {
					if (number < 0) {
						continue;
					}
					const auto block = pane->document()->findBlockByNumber(number);
					if (block.isValid()) {
						lines.push_back(block.text());
					}
				}
				return lines.isEmpty() ? std::nullopt : std::optional{ lines.join(QLatin1Char{ '\n' }) };
			});
		// [[BPR-1564]]. A suggestion's commented lines, exactly as the compared
		// document holds them, when the thread's file is the one compared.
		ConversationPanel->cards()->set_suggestion_locator(
			[this](const QString& aPath, const core::diff_side aSide, const std::uint32_t aFirst, const std::uint32_t aLast)
				-> std::optional<std::vector<std::string>> {
				if (ComparisonSurface == nullptr || ComparisonSurface->current_result() == nullptr || !ComparisonSurface->current_context().has_value()
					|| ComparisonSurface->current_context()->Key.File != core::file_identity_of(aPath.toStdString()) || aFirst == 0U || aLast < aFirst) {
					return std::nullopt;
				}
				const auto& result   = *ComparisonSurface->current_result();
				const auto& document = aSide == core::diff_side::right ? result.RightDocument : result.LeftDocument;
				if (!document.has_value()) {
					return std::nullopt;
				}
				return core::suggestion_source_lines(*document, aFirst - 1U, aLast - 1U);
			});
		ComparisonSurface->set_selection_observer(
			[this](const std::optional<core::semantic_selection>& aSelection,
				   const bool                                     aInteractive) {
				// [[BPR-1753]], owner decision (a). The Agent tab's
				// suggestions name the selected lines; read once the gesture
				// has settled (owner look v006).
				schedule_agent_suggestions();
				if (ConversationPanel == nullptr || ComparisonSurface == nullptr) {
					return;
				}
				// [[BPR-0929]]. Discuss is a PR-wide reading surface. The change
				// controls may continue to move the retained comparison underneath
				// it, but that code navigation must not replace or clear the
				// conversation identity the reviewer is reading.
				// [[BPR-1293]]. So is Threads: the arrows move the
				// comparison underneath, not the finder's cursor.
				if (Modality != nullptr
					&& (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads)) {
					update_strip_positions();
					update_home_landing();
					return;
				}
				const auto endpointIsCurrent = aSelection.has_value()
					&& (aSelection->PreferredLine.Side == core::diff_side::left
							? LeftConversationEndpointCurrent
							: RightConversationEndpointCurrent);
				const auto line            = aInteractive && aSelection.has_value()
						&& aSelection->Kind == core::semantic_selection_kind::line
						&& (endpointIsCurrent || ConversationPanel->has_semantic_anchor(aSelection->PreferredLine))
					? std::optional<core::source_line_ref>{ aSelection->PreferredLine }
					: std::nullopt;
				ApplyingConversationAnchor = true;
				ConversationPanel->set_anchor_file(conversation_anchor_file());
				ConversationPanel->set_anchor_line(line);
				refresh_cards();
				if (aInteractive && !line.has_value()) {
					ConversationPanel->view()->setCurrentIndex({ });
					ConversationPanel->view()->selectionModel()->clearSelection();
				}
				ApplyingConversationAnchor = false;
				if (aInteractive && !line.has_value()) {
					update_defect_surface();
				}
				update_strip_positions();
				update_home_landing();
				update_comment_target();
			});
		ReviewSubmissionEditor = new review_submission_editor{
			[this](review_submission_editor_intent aIntent) {
				if (ReviewSubmissionDispatch) {
					ReviewSubmissionDispatch(std::move(aIntent));
				}
			},
			Drawers[1].Panel
		};
		ReviewEditor = new review_draft_editor{
			[this](core::review_draft_command aCommand) {
				if (ReviewDraftDispatch) {
					ReviewDraftDispatch(std::move(aCommand));
				}
			},
			[this](core::file_receipt_command aCommand) {
				if (ReceiptDispatch) {
					ReceiptDispatch(aCommand);
				}
			},
			Drawers[1].Panel,
		};
		ReviewEditor->setObjectName(QStringLiteral("workbenchReviewDraftEditor"));
		ReviewEditor->set_presentation_observer([this] {
			update_comment_target();
			present_inline_reply();
			present_inline_draft();
			// [[BPR-1212]]. A comment on the selected lines waiting for its
			// line's draft opens once it is here.
			static_cast<void>(resume_line_comment());
			// [[BPR-1590]] v003. So does a pending comment opened from
			// Finish your review.
			resume_pending_comment_open();
		});
		connect(ReviewEditor->body_editor(), &QPlainTextEdit::textChanged, this, [this] {
			present_inline_reply();
			present_inline_draft();
		});
		connect(ReviewEditor->publish_action(), &QAction::changed, this, [this] {
			present_inline_reply();
			present_inline_draft();
		});
		// [[BPR-1083]]. Marks are drawn from the start, in the built-in tints
		// until the composition supplies the theme's.
		apply_label_marks();
		DefectDialog = new QDialog{ this };
		DefectDialog->setObjectName(QStringLiteral("threadDefectDialog"));
		DefectDialog->setWindowTitle(tr("Thread defect"));
		DefectDialog->setWindowModality(Qt::WindowModal);
		auto* defectLayout = new QVBoxLayout{ DefectDialog };
		DefectCard         = new defect_card_view{
			[this](core::defect_command aCommand) {
				++Statistics.DefectIntents;
				if (DefectDispatch) {
					DefectDispatch(std::move(aCommand));
				}
			},
			DefectDialog,
		};
		DefectCard->setObjectName(QStringLiteral("workbenchDefectCard"));
		defectLayout->addWidget(DefectCard);
		auto* applyClassification = new QToolButton{ DefectDialog };
		applyClassification->setDefaultAction(DefectCard->reclassify_action());
		defectLayout->addWidget(applyClassification);
		auto* defectClose = new QDialogButtonBox{ QDialogButtonBox::Close, DefectDialog };
		connect(defectClose, &QDialogButtonBox::rejected, DefectDialog, &QDialog::hide);
		defectLayout->addWidget(defectClose);
		ConversationPanel->cards()->set_context_menu_builder([this](QMenu& aMenu, const std::string& aThread) { append_thread_defect_menu(aMenu, aThread); });
		ClassifyDefect = new QAction{
			tr("Classify selected thread as defect"), this
		};
		ClassifyDefect->setObjectName(QStringLiteral("classifySelectedDefect"));
		PreviousDefect = new QAction{ tr("Previous defect"), this };
		PreviousDefect->setObjectName(QStringLiteral("previousDefect"));
		NextDefect = new QAction{ tr("Next defect"), this };
		NextDefect->setObjectName(QStringLiteral("nextDefect"));
		connect(ClassifyDefect, &QAction::triggered, this, [this] {
			classify_selected_conversation();
		});
		connect(PreviousDefect, &QAction::triggered, this, [this] {
			navigate_defect(-1);
		});
		connect(NextDefect, &QAction::triggered, this, [this] {
			navigate_defect(1);
		});
		// [[BPR-1134]]. RFC-BPR-0052's right tray: tabs in the drawer's header,
		// each tab's tools beneath them. The Comments tab holds the conversation
		// with its scope and filters, and Next new comment; its host keeps the
		// panel's place while Discuss borrows it for the centre (the drawers
		// are hidden then, so the tab needs no notice of its own).
		auto* const commentsContents = new QWidget{ Drawers[1].Panel };
		commentsContents->setObjectName(QStringLiteral("workbenchCommentsTab"));
		auto* const commentsLayout = new QVBoxLayout{ commentsContents };
		commentsLayout->setContentsMargins(0, 0, 0, 0);
		// [[BPR-1290]]. Next new reply sits on the tab row with the filters;
		// the defect queue's row shows only while there are defects.
		NextNewCommentButton = new QToolButton{ commentsContents };
		NextNewCommentButton->setObjectName(QStringLiteral("workbenchNextNewCommentButton"));
		NextNewCommentButton->setDefaultAction(NextNewCommentAction);
		NextNewCommentButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		NextNewCommentButton->setAutoRaise(true);
		NextNewCommentButton->setIconSize(QSize{ 16, 16 });
		NextNewCommentButton->setFocusPolicy(Qt::StrongFocus);
		NextNewCommentButton->setAccessibleName(QStringLiteral("Next new comment"));
		NextNewCommentButton->setAccessibleDescription(QStringLiteral(
			"Reveal the next thread with a post newer than what you have read"));
		DefectRow = new QWidget{ commentsContents };
		DefectRow->setObjectName(QStringLiteral("workbenchDefectRow"));
		auto* const commentsTools = new QHBoxLayout{ DefectRow };
		commentsTools->setContentsMargins(4, 2, 4, 2);
		commentsTools->addStretch(1);
		DefectCountLabel = new QLabel{ QStringLiteral("Defects 0/0"), commentsContents };
		DefectCountLabel->setObjectName(QStringLiteral("defectCount"));
		DefectCountLabel->setAccessibleName(QStringLiteral("Defect queue position"));
		commentsTools->addWidget(DefectCountLabel);
		PreviousDefectButton = new QToolButton{ commentsContents };
		NextDefectButton     = new QToolButton{ commentsContents };
		PreviousDefect->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
		NextDefect->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
		for (const auto& entry : { std::pair{ PreviousDefectButton, PreviousDefect }, std::pair{ NextDefectButton, NextDefect } }) {
			entry.first->setObjectName(entry.second->objectName() + QStringLiteral("Button"));
			entry.first->setDefaultAction(entry.second);
			entry.first->setToolButtonStyle(Qt::ToolButtonIconOnly);
			entry.first->setAccessibleName(entry.second->text());
			entry.first->setAccessibleDescription(tr("Select the %1 thread in Comments").arg(entry.second == PreviousDefect ? tr("previous defect") : tr("next defect")));
			entry.first->setToolTip(entry.second->text());
			entry.first->setFocusPolicy(Qt::StrongFocus);
			commentsTools->addWidget(entry.first);
		}
		commentsLayout->addWidget(DefectRow);
		DefectRow->hide();
		// [[BPR-1132]]. A tab over an older pair places only its own
		// revisions' threads; this says how many of the file's are on later
		// updates and routes to them.
		OlderPairNotice = new QWidget{ commentsContents };
		OlderPairNotice->setObjectName(QStringLiteral("olderPairNotice"));
		auto* const olderPair = new QVBoxLayout{ OlderPairNotice };
		olderPair->setContentsMargins(8, 6, 8, 6);
		olderPair->setSpacing(4);
		OlderPairText = new QLabel{ OlderPairNotice };
		OlderPairText->setObjectName(QStringLiteral("olderPairNoticeText"));
		OlderPairText->setAccessibleName(QStringLiteral("Threads on later updates"));
		OlderPairText->setTextFormat(Qt::PlainText);
		OlderPairText->setWordWrap(true);
		ShowLaterThreads = new quiet_push_button{ tr("Show them"), OlderPairNotice };
		ShowLaterThreads->setObjectName(QStringLiteral("showLaterThreads"));
		ShowLaterThreads->setAccessibleDescription(QStringLiteral(
			"Shows this file's threads on later updates"));
		olderPair->addWidget(OlderPairText);
		olderPair->addWidget(ShowLaterThreads, 0, Qt::AlignLeft);
		OlderPairNotice->hide();
		commentsLayout->addWidget(OlderPairNotice);
		connect(ShowLaterThreads, &QPushButton::clicked, this, [this] { show_later_threads(); });
		auto* const conversationHost   = new QWidget{ commentsContents };
		auto* const conversationLayout = new QVBoxLayout{ conversationHost };
		ConversationLayout             = conversationLayout;
		ConversationHome               = conversationHost;
		conversationLayout->setContentsMargins(0, 0, 0, 0);
		conversationLayout->addWidget(ConversationPanel, 1);
		commentsLayout->addWidget(conversationHost, 1);
		// The private tab holds AI tools and findings; thread controls belong
		// to the thread menus and Comments navigation.
		auto* const conversationContents = new QWidget{ Drawers[1].Panel };
		conversationContents->setObjectName(QStringLiteral("workbenchNotesTab"));
		auto* const findingsLayout = new QVBoxLayout{ conversationContents };
		findingsLayout->setContentsMargins(0, 0, 0, 0);
		DiscussPage = new QWidget{ CentreStack };
		DiscussPage->setObjectName(QStringLiteral("workbenchDiscussPage"));
		auto* discussLayout = new QHBoxLayout{ DiscussPage };
		discussLayout->setContentsMargins(0, 0, 0, 0);
		discussLayout->setSpacing(0);
		// [[BPR-1212]]. Discuss is the home of the full composer: the
		// conversations at the left, the draft or the review summary with
		// room at the right (owner, 2026-10-06: "Fold add comment window
		// into discuss").
		DiscussComposer = new QWidget{ DiscussPage };
		DiscussComposer->setObjectName(QStringLiteral("discussComposer"));
		DiscussComposer->setAccessibleName(tr("Composer"));
		auto* const composerColumn = new QVBoxLayout{ DiscussComposer };
		composerColumn->setContentsMargins(0, 0, 0, 0);
		composerColumn->setSpacing(0);
		DiscussComposer->hide();
		discussLayout->addWidget(DiscussComposer, 1);
		// [[BPR-1584]]. Discuss's notes keep the page's left edge: past the
		// reading width the rest of the page is this stretch's.
		discussLayout->addStretch(0);
		CentreStack->addWidget(DiscussPage);
		// [[BPR-1293]]. Threads: the finder fills the centre, as
		// Discuss's page does, and the drawers are hidden while it shows.
		ThreadsPage = new QWidget{ CentreStack };
		ThreadsPage->setObjectName(QStringLiteral("workbenchThreadsPage"));
		auto* const conversationsLayout = new QVBoxLayout{ ThreadsPage };
		conversationsLayout->setContentsMargins(0, 0, 0, 0);
		Finder = new conversation_finder{ ThreadsPage };
		conversationsLayout->addWidget(Finder);
		// The drawers are hidden while it shows, so the page always has the
		// window's whole width; as a page of the stack it must not widen the
		// centre's floor while Review shows its drawers.
		ThreadsPage->setMinimumSize(1, 1);
		CentreStack->addWidget(ThreadsPage);
		Finder->set_pick_dispatch([this](const finder_location& aLocation, const core::workbench_thread_destination aDestination) {
			static_cast<void>(open_thread_from_threads(aLocation.Thread, aLocation.Post, aDestination));
		});
		Finder->set_leave_dispatch([this] { leave_threads(); });
		// [[BPR-1582]]. The row menu's Mark as read is the card's Mark read.
		Finder->set_read_dispatch([this](const std::string& aThread, const bool aRead) {
			if (aRead && ConversationReadDispatch) {
				ConversationReadDispatch(aThread);
			} else if (!aRead && ConversationUnreadDispatch) {
				ConversationUnreadDispatch(aThread);
			}
		});
		// [[BPR-1699]]. Mark All from an author as Read, from a row.
		Finder->set_author_read_dispatch(
			[this](const std::string& aLogin) {
				const auto dispatch = ConversationsReadDispatch;
				if (dispatch) {
					dispatch(aLogin);
				}
			},
			[this](const QString& aLogin) { return ConversationPanel->cards()->unread_post_count_from(aLogin); });
		// [[BPR-1356]]. The notice that says which filters a pick cleared
		// goes once the reviewer changes one.
		ConversationPanel->set_discuss_filters_changed([this] {
			if (ThreadsRevealNotice) {
				ThreadsRevealNotice = false;
				ConversationPanel->present_notice(QString{ });
			}
		});
		// [[BPR-1844]]. A reviewer's own filter choice: this window's, and
		// remembered for the next pull request any window opens.
		ConversationPanel->set_filter_choice_dispatch([this](const core::filter_view aView, const core::view_filter_choice aChoice) { choose_view_filter(aView, aChoice); });
		Finder->set_filter_choice_dispatch([this](const core::view_filter_choice aChoice) { choose_view_filter(core::filter_view::threads, aChoice); });
		const auto makeCompositionPage = [this](QWidget* aEditor, const QString& aName, const QString& aBack, QWidget* aExcerpt) {
			// [[BPR-1212]]. A composition page is a column of Discuss, beside
			// the conversations, not a page of its own.
			auto* scroll = new QScrollArea{ DiscussComposer };
			scroll->setObjectName(aName);
			scroll->setWidgetResizable(true);
			scroll->setFrameShape(QFrame::NoFrame);
			scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			auto* page   = new QWidget{ scroll };
			auto* column = new QVBoxLayout{ page };
			column->setContentsMargins(24, 12, 24, 24);
			column->setSpacing(10);
			auto* const bar  = new QHBoxLayout{ };
			auto*       back = new quiet_push_button{ aBack, page };
			back->setObjectName(aName + QStringLiteral("Back"));
			back->setIcon(conversation_icon(conversation_glyph::minimize, palette()));
			// [[BPR-1345]] v002. ⌘⇧M, as Maximize took the draft here; ⌘⇧↩
			// is Add single comment now.
			const auto key = shortcut_key(shortcut_command::draft_to_discuss).toString(QKeySequence::NativeText);
			back->setToolTip(tr("%1, without publishing  %2 or Esc").arg(aBack, key));
			back->setAccessibleDescription(tr("Return to the previous surface without submitting"));
			connect(back, &QPushButton::clicked, this, [this] { close_review_composition(); });
			// [[BPR-1129]]. Esc is Back, from anywhere on the page; so is
			// ⌘⇧M, the key that brought the draft here ([[BPR-1212]];
			// ⌘⇧↩ until [[BPR-1345]] v002).
			auto* const escape = new QShortcut{ shortcut_key(shortcut_command::leave_composer), scroll };
			escape->setObjectName(aName + QStringLiteral("Escape"));
			escape->setContext(Qt::WidgetWithChildrenShortcut);
			connect(escape, &QShortcut::activated, this, [this] { close_review_composition(); });
			auto* const restore = new QShortcut{ shortcut_key(shortcut_command::draft_to_discuss), scroll };
			restore->setObjectName(aName + QStringLiteral("Restore"));
			restore->setContext(Qt::WidgetWithChildrenShortcut);
			connect(restore, &QShortcut::activated, this, [this] { close_review_composition(); });
			auto* const title = new QLabel{ page };
			title->setObjectName(aName + QStringLiteral("Title"));
			title->setTextFormat(Qt::PlainText);
			// [[BPR-1212]] v002. The restore sits at the top right, where the
			// drawer draft's Maximize was (owner, maximize-to-discuss-v001:
			// "the restore (back to code) should still be in the top right").
			back->setFlat(true);
			back->setLayoutDirection(Qt::RightToLeft);
			bar->addWidget(title);
			bar->addStretch(1);
			bar->addWidget(back);
			column->addLayout(bar);
			if (aExcerpt != nullptr) {
				aExcerpt->setParent(page);
				column->addWidget(aExcerpt);
			}
			column->addWidget(aEditor, 1);
			scroll->setWidget(page);
			scroll->hide();
			DiscussComposer->layout()->addWidget(scroll);
			return scroll;
		};
		DraftExcerpt      = new draft_excerpt_view{ DiscussComposer };
		ComposeReviewPage = makeCompositionPage(ReviewSubmissionEditor, QStringLiteral("composeReviewPage"), tr("Back"), nullptr);
		ComposeInlinePage = makeCompositionPage(ReviewEditor, QStringLiteral("composeInlineDraftPage"), tr("Back to the code"), DraftExcerpt);
		// [[BPR-1590]]. RFC-BPR-0085 D6, the owner's "C with an expand to A":
		// Finish your review is a sheet at the review area's right edge, over
		// whatever the modality shows; Expand makes it the focused page in
		// Discuss, whose Restore brings the sheet back. The composer is one
		// widget, lent to whichever is open.
		ReviewSheet = new compose_review_sheet{ ReviewSurface };
		ReviewSheet->scroll()->viewport()->setBackgroundRole(QPalette::Base);
		{
			const auto expandKey = shortcut_key(shortcut_command::draft_to_discuss).toString(QKeySequence::NativeText);
			const auto closeKey  = shortcut_key(shortcut_command::leave_composer).toString(QKeySequence::NativeText);
			ReviewSheet->set_tools(conversation_icon(conversation_glyph::maximize, palette()), tr("Expand to a page  %1").arg(expandKey), [this] { expand_review_sheet(); }, drawer_icon(drawer_glyph::close, palette()), tr("Close  %1").arg(closeKey), [this] { close_review_sheet(); });
			auto* const sheetEscape = new QShortcut{ shortcut_key(shortcut_command::leave_composer), ReviewSheet };
			sheetEscape->setObjectName(QStringLiteral("composeReviewSheetEscape"));
			sheetEscape->setContext(Qt::WidgetWithChildrenShortcut);
			connect(sheetEscape, &QShortcut::activated, this, [this] { close_review_sheet(); });
			auto* const sheetExpand = new QShortcut{ shortcut_key(shortcut_command::draft_to_discuss), ReviewSheet };
			sheetExpand->setObjectName(QStringLiteral("composeReviewSheetExpandKey"));
			sheetExpand->setContext(Qt::WidgetWithChildrenShortcut);
			connect(sheetExpand, &QShortcut::activated, this, [this] { expand_review_sheet(); });
		}
		if (auto* const restore = ComposeReviewPage->findChild<QPushButton*>(QStringLiteral("composeReviewPageBack")); restore != nullptr) {
			const auto key = shortcut_key(shortcut_command::draft_to_discuss).toString(QKeySequence::NativeText);
			restore->setText(tr("Restore"));
			restore->setToolTip(tr("Back to the sheet beside the code  %1 or Esc").arg(key));
			restore->setAccessibleDescription(tr("Return to the review sheet without submitting"));
		}
		CentralSplitter->installEventFilter(this);
		ComposeReviewPage->viewport()->installEventFilter(this);
		// Keep the decision and its illustration visible while the before/after
		// scrolls. Mascot preferences must never hide these semantic controls.
		WorkshopHeader = new QWidget{ CentreSurface };
		WorkshopHeader->setObjectName(QStringLiteral("workbenchWorkshopHeader"));
		auto* workshopEscape = new QShortcut{ shortcut_key(shortcut_command::leave_composer), WorkshopHeader };
		workshopEscape->setObjectName(QStringLiteral("workbenchWorkshopHeaderEscape"));
		workshopEscape->setContext(Qt::WidgetWithChildrenShortcut);
		connect(workshopEscape, &QShortcut::activated, this, [this] { close_review_composition(); });
		auto* workshopHeader = new QHBoxLayout{ WorkshopHeader };
		// [[BPR-1347]]. One compact row: Dif first, beside what he
		// illustrates, then the workshop's name and its decision. The strut
		// keeps the row's height when Dif is hidden, and nothing is left where
		// he was.
		auto margins = workshopHeader->contentsMargins();
		margins.setTop(4);
		margins.setBottom(4);
		workshopHeader->setContentsMargins(margins);
		workshopHeader->addStrut(dif_workshop_size);
		DifWorkshopSlot = new QWidget{ WorkshopHeader };
		DifWorkshopSlot->setObjectName(QStringLiteral("workbenchDifWorkshopSlot"));
		DifWorkshopSlot->setAccessibleName(tr("Decorative Dif workshop illustration"));
		DifWorkshopSlot->setFixedSize(dif_workshop_size, dif_workshop_size);
		auto* workshopIllustration = new QHBoxLayout{ DifWorkshopSlot };
		workshopIllustration->setContentsMargins(0, 0, 0, 0);
		workshopHeader->addWidget(DifWorkshopSlot);
		auto* const workshopTitle = new QLabel{ tr("Comment workshop"), WorkshopHeader };
		workshopTitle->setObjectName(QStringLiteral("workbenchWorkshopTitle"));
		workshopHeader->addWidget(workshopTitle);
		for (auto* action : { ReviewEditor->accept_revision_action(), ReviewEditor->reject_revision_action() }) {
			auto* button = new QToolButton{ WorkshopHeader };
			button->setObjectName(action->objectName() + QStringLiteral("Button"));
			button->setDefaultAction(action);
			button->setToolButtonStyle(Qt::ToolButtonTextOnly);
			button->setFocusPolicy(Qt::StrongFocus);
			workshopHeader->addWidget(button);
			connect(action, &QAction::changed, this, [this] { update_dif_presentation(); });
		}
		workshopHeader->addStretch();
		WorkshopHeader->hide();
		centre->insertWidget(centre->indexOf(CentreStack), WorkshopHeader);
		// [[BPR-1129]]. The kept-draft mark follows what the workflow presents.
		ReviewSubmissionEditor->set_presentation_observer([this] { update_compose_mark(); });
		// [[BPR-1085]]. A preflight row links back to its exact item: my unsent
		// draft opens its page; a conversation is revealed in Discuss.
		// [[BPR-1345]] v002. A pending row goes to its comment; its Delete,
		// once confirmed, goes to the composition; a confirmed Discard review
		// closes the page.
		ReviewSubmissionEditor->set_pending_reveal([this](const core::review_submission_comment& aComment) {
			open_pending_comment(aComment, core::workbench_thread_destination::review);
		});
		// [[BPR-1590]] v003. Threads' gestures on a pending row.
		ReviewSubmissionEditor->set_pending_open([this](const core::review_submission_comment& aComment, const core::workbench_thread_destination aDestination) {
			open_pending_comment(aComment, aDestination);
		});
		ReviewSubmissionEditor->set_pending_delete([this](const core::review_submission_comment& aComment) {
			if (PendingCommentDeleteDispatch) {
				PendingCommentDeleteDispatch(aComment.Draft, aComment.DraftRevision);
			}
		});
		// [[RFC-BPR-0090]]. Convert on a held suggestion (owner, Q3): the
		// comment is opened at its line, and converted there once it shows.
		ReviewSubmissionEditor->set_pending_convert([this](const core::review_submission_comment& aComment) {
			// [[BPR-1735]]. Publish as file comment: the comment is opened at
			// its line and published there on its own once the core places it.
			if (UnplaceableComments.contains(aComment.Draft) && !HeldSuggestions.contains(aComment.Draft)) {
				PendingFilePublish = aComment.Draft;
				if (ReviewEditor != nullptr && ReviewEditor->shown_draft().has_value() && ReviewEditor->shown_draft()->Id == aComment.Draft) {
					publish_pending_file_comment();
					return;
				}
				open_pending_comment(aComment, core::workbench_thread_destination::review);
				return;
			}
			if (!HeldSuggestions.contains(aComment.Draft)) {
				return;
			}
			PendingConvert = aComment.Draft;
			if (ReviewEditor != nullptr && ReviewEditor->shown_draft().has_value() && ReviewEditor->shown_draft()->Id == aComment.Draft) {
				present_inline_draft();
				return;
			}
			open_pending_comment(aComment, core::workbench_thread_destination::review);
		});
		ReviewSubmissionEditor->set_discard_observer([this] {
			if (CompositionPage != nullptr && CompositionPage == ComposeReviewPage) {
				close_review_composition(false);
			}
			close_review_sheet();
		});
		// [[BPR-1345]] v003. A review GitHub confirmed closes the page and
		// returns to the view the reviewer came from, Review or Discuss,
		// with a brief line saying so (owner, finish-page v002). A refused or
		// unknown answer keeps the page and its recovery.
		// [[BPR-1816]]. Or, as the core decided from that verdict's setting,
		// the review closes too and Home shows (owner, 2026-10-29).
		ReviewSubmissionEditor->set_submitted_observer([this](const std::size_t aComments, const core::review_finish_transition aTransition) {
			if (CompositionPage != nullptr && CompositionPage == ComposeReviewPage) {
				close_review_composition(false);
			}
			close_review_sheet();
			const auto said = aComments == 0U ? tr("Review submitted")
				: aComments == 1U             ? tr("Review submitted with 1 comment")
											  : tr("Review submitted with %1 comments").arg(aComments);
			statusBar()->showMessage(said, 8000);
			if (aTransition == core::review_finish_transition::close_review_and_go_home && CloseReviewDispatch) {
				// The workflow that confirmed the review is still on the
				// stack; closing the review retires its host, so the close
				// waits for the next turn and is said there.
				FinishingAtHome = IdentityText;
				QTimer::singleShot(0, this, [this, said] { finish_review_at_home(said); });
				return;
			}
			QAccessibleAnnouncementEvent event{ this, said };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		});
		ReviewSubmissionEditor->set_item_reveal([this](const std::string& aIdentity) {
			if (aIdentity.starts_with("draft:")) {
				open_review_composition(true);
				return;
			}
			close_review_composition();
			static_cast<void>(reveal_conversation(aIdentity));
		});
		// [[BPR-1592]]. RFC-BPR-0085 D3: Notes is the private notes at their
		// lines, and nothing else. The agent's panels move, unchanged, to the
		// provisional Agent tab with the private history and its purpose.
		auto* const notesPage       = new QWidget{ Drawers[1].Panel };
		auto* const notesPageLayout = new QVBoxLayout{ notesPage };
		notesPageLayout->setContentsMargins(0, 0, 0, 0);
		NotesHome    = notesPage;
		PrivateNotes = new private_notes_panel{ notesPage };
		notesPageLayout->addWidget(PrivateNotes, 1);
		wire_private_notes();
		// [[BPR-1781]]. The agent's page is a body in a home: Overview's
		// drawer borrows the body, whole, while Overview shows.
		AgentHome = new QWidget{ Drawers[1].Panel };
		AgentHome->setObjectName(QStringLiteral("workbenchAgentHome"));
		auto* const agentHomeLayout = new QVBoxLayout{ AgentHome };
		agentHomeLayout->setContentsMargins(0, 0, 0, 0);
		auto* const agentPage = new QWidget{ AgentHome };
		agentPage->setObjectName(QStringLiteral("workbenchAgentBody"));
		AgentBody = agentPage;
		agentHomeLayout->addWidget(agentPage, 1);
		auto* const agentPageLayout = new QVBoxLayout{ agentPage };
		agentPageLayout->setContentsMargins(0, 0, 0, 0);
		auto* const notesPurpose = new QLabel{ tr("Private to you and never published. Ask an agent about this pull request, then promote what you want to say to a draft in Comments."), agentPage };
		notesPurpose->setObjectName(QStringLiteral("workbenchNotesPurpose"));
		notesPurpose->setTextFormat(Qt::PlainText);
		notesPurpose->setWordWrap(true);
		notesPurpose->setAccessibleName(tr("About Agent"));
		notesPurpose->setAccessibleDescription(notesPurpose->text());
		notesPurpose->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		agentPageLayout->addWidget(notesPurpose);
		AgentPanel = new agent_launch_panel{ { }, conversationContents };
		AgentPanel->setMaximumHeight(300);
		findingsLayout->addWidget(AgentPanel);
		// The agent surfaces remain beside their launcher. The same retained
		// draft editor and its workshop now have their own composition page.
		SideChat = new side_chat_panel{ { }, conversationContents };
		findingsLayout->addWidget(SideChat);
		// Stacked, as the finding queue's actions are: the drawer never scrolls
		// sideways, so a row wider than the drawer loses its last actions rather
		// than pushing them offscreen (BPR-0506).
		auto* const chatActions = new QVBoxLayout{ };
		for (auto* const action : { SideChat->send_action(),
									SideChat->add_context_action(),
									SideChat->new_session_action(),
									SideChat->continue_old_revision_action() }) {
			QToolButton* const button = action == SideChat->send_action() ? new QToolButton{ conversationContents } : static_cast<QToolButton*>(new quiet_tool_button{ conversationContents });
			button->setDefaultAction(action);
			chatActions->addWidget(button);
		}
		findingsLayout->addLayout(chatActions);
		FindingQueue = new finding_queue_view{ { }, conversationContents };
		findingsLayout->addWidget(FindingQueue);
		// One button per declared action, indexed by the enumerator, so a
		// seventh finding action cannot be declared and then never offered.
		//
		// BPR-0506. Stacked rather than in a row. Six buttons whose longest
		// label is *Convert to local draft comment* need more width than any
		// drawer a reviewer is likely to give this column, and the drawer
		// deliberately does not scroll sideways, so a row put the last two
		// actions somewhere no gesture reaches. A column cannot clip: each
		// button is as wide as its own label and the drawer already scrolls
		// down. The six actions, their order, their labels, and the single
		// intent each emits are untouched - this is where they are drawn, not
		// what they are.
		auto* const findingActions = new QVBoxLayout{ };
		for (auto index = std::size_t{ 0U }; index < finding_queue_action_count;
			 ++index) {
			auto* const action = FindingQueue->action(
				static_cast<finding_queue_action>(index));
			if (action == nullptr) {
				continue;
			}
			const auto         kind   = static_cast<finding_queue_action>(index);
			QToolButton* const button = kind == finding_queue_action::open_evidence || kind == finding_queue_action::ask_follow_up || kind == finding_queue_action::add_to_private_notes
				? static_cast<QToolButton*>(new quiet_tool_button{ conversationContents })
				: new QToolButton{ conversationContents };
			button->setDefaultAction(action);
			findingActions->addWidget(button);
		}
		findingsLayout->addLayout(findingActions);
		// Each tab's column scrolls on its own, so switching tabs keeps each
		// one's scroll position. A column holds more than a short drawer can
		// show at once, and a column that simply squeezed would show each panel
		// with its own fields overlapping — which is showing a reviewer
		// nothing. Collapse, width reporting, and the toggle are untouched:
		// this is inside the drawer's content, not around it.
		// BPR-0527. The drawer opens at the width stated above rather than at
		// whatever the widest row asked for. [[BPR-0728]]: that width is the
		// default, not a floor, so neither column claims a minimum of its own
		// and the handle drags the drawer down to the panel floor the other
		// drawers share. Beneath a column's own minimum its content scrolls
		// sideways rather than clipping, so every control stays reachable.
		const auto makeTabColumn = [this](QWidget* const aContents, const QString& aName) {
			auto* const scroll = new QScrollArea{ Drawers[1].Panel };
			scroll->setObjectName(aName);
			scroll->setWidgetResizable(true);
			scroll->setFrameShape(QFrame::NoFrame);
			scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
			scroll->setWidget(aContents);
			return scroll;
		};
		CommentsScroll     = makeTabColumn(commentsContents, QStringLiteral("workbenchCommentsScroll"));
		ConversationScroll = makeTabColumn(conversationContents, QStringLiteral("workbenchNotesScroll"));
		// Keep the purpose within the visible page even when tools scroll sideways.
		auto* privateTabs = new QTabWidget{ agentPage };
		privateTabs->setObjectName(QStringLiteral("privateConversationTabs"));
		privateTabs->setAccessibleName(tr("Private history and agent tools"));
		PrivateReview = new private_review_panel{ agentPage };
		ReviewSubmissionEditor->set_saved_comments_reveal([this] { PrivateReview->show_saved_comments(); });
		privateTabs->addTab(makeTabColumn(PrivateReview, QStringLiteral("privateHistoryScroll")), tr("Private history"));
		privateTabs->addTab(ConversationScroll, tr("Agent tools"));
		agentPageLayout->addWidget(privateTabs, 1);
		// [[BPR-1594]]. RFC-BPR-0085 D5: the Agent tab presents the agent's
		// panels, moved into one layout; the private history is its History.
		// The panels' own column stays, unseen, for every control not moved.
		AgentTab = new agent_tab_view{ *AgentPanel, *SideChat, *FindingQueue, *privateTabs->widget(0), agentPage };
		// Owner look v006 ("I selected a block and it still said the line
		// suggestion and then when i clicked away ... the suggestion text
		// updates to block"): the selected lines are the panes' own text
		// selection, which a drag changes without a new selected line, and a
		// press reports its line before the pane's selection moves. The
		// suggestions are read from the panes' selections as they change,
		// once the event that changed them has finished.
		AgentSuggestionsTimer = new QTimer{ this };
		AgentSuggestionsTimer->setSingleShot(true);
		AgentSuggestionsTimer->setInterval(0);
		connect(AgentSuggestionsTimer, &QTimer::timeout, this, [this] {
			if (AgentTab != nullptr) {
				AgentTab->refresh_suggestions();
			}
		});
		if (ComparisonSurface != nullptr) {
			for (auto* const pane : { ComparisonSurface->left_pane(), ComparisonSurface->right_pane(), ComparisonSurface->unified_pane() }) {
				if (pane != nullptr) {
					connect(pane, &QPlainTextEdit::selectionChanged, this, [this] { schedule_agent_suggestions(); });
					connect(pane, &QPlainTextEdit::cursorPositionChanged, this, [this] { schedule_agent_suggestions(); });
				}
			}
		}
		AgentTab->set_setup_dispatch([this] { open_settings(settings_page::agents_and_context); });
		AgentSetup = new agent_setup_view{ agentPage };
		AgentSetup->hide();
		agentPageLayout->addWidget(AgentTab, 1);
		notesPurpose->hide();
		privateTabs->hide();

		// [[BPR-1134]]. The tray's header is its tabs: Comments, then Notes
		// and Tour. The drawer still
		// opens on Comments. Choosing Notes is RFC-BPR-0031's explicit move
		// into the AI tools, the action that cues compare_glance in the corner; it
		// dispatches no agent or review action and never opens the drawer.
		auto* const conversationShell       = new QWidget{ Drawers[1].Panel };
		auto* const conversationShellLayout = new QVBoxLayout{ conversationShell };
		conversationShellLayout->setContentsMargins(0, 0, 0, 0);
		conversationShellLayout->setSpacing(0);
		TrayTabs = new tray_tab_bar{ conversationShell };
		TrayTabs->setObjectName(QStringLiteral("workbenchTrayTabs"));
		TrayTabs->setAccessibleName(QStringLiteral("Right tray"));
		TrayTabs->setAccessibleDescription(QStringLiteral(
			"Comments, Notes and Tour. Notes and tour preparation are private until explicitly published."));
		configure_drawer_tabs(*TrayTabs);
		// [[BPR-1594]]. The Agent keeps its spark beside its name.
		// [[BPR-1684]]: fit_tray_tabs gives each tab its face, glyph
		// and tooltip for the width the row has.
		TrayTabs->addTab(tr("Comments"));
		TrayTabs->addTab(tr("Notes"));
		TrayTabs->addTab(tr("Tour"));
		TrayTabs->addTab(drawer_icon(drawer_glyph::agent, palette()), tr("Agent"));
		TrayTabs->setAccessibleTabName(tray_comments_tab, tr("Comments"));
		TrayTabs->setAccessibleTabName(tray_notes_tab, tr("Notes"));
		TrayTabs->setAccessibleTabName(tray_tour_tab, tr("Tour"));
		TrayTabs->setAccessibleTabName(tray_findings_tab, tr("Agent"));
		// [[BPR-1684]]. The count is the tab's button only while it
		// shows, so a tab without new comments keeps no room for it.
		TrayCommentsCount = new drawer_count_pill{ TrayTabs };
		TrayCommentsCount->setObjectName(QStringLiteral("workbenchTrayCommentsCount"));
		TrayCommentsCount->hide();
		TrayPages = new QStackedWidget{ conversationShell };
		TrayPages->setObjectName(QStringLiteral("workbenchTrayPages"));
		TrayPages->addWidget(CommentsScroll);
		TrayPages->addWidget(notesPage);
		// [[BPR-1593]]. The Tour tab is the route; its editor moves to the tour
		// page on Expand and back on Back to the code, so it sits in a holder.
		TourHolder = new QWidget{ conversationShell };
		TourHolder->setObjectName(QStringLiteral("workbenchTourHolder"));
		auto* const tourHolding = new QVBoxLayout{ TourHolder };
		tourHolding->setContentsMargins(0, 0, 0, 0);
		TourEditor = new review_tour_editor{ TourHolder };
		tourHolding->addWidget(TourEditor);
		// [[BPR-1914]] v003, owner 2026-11-06: "something more like a side
		// chat". The tour's side chats dock at the window's right edge, past
		// the drawers, so the code, the comments and the tour keep their
		// room; closed, they take none. The dock has no title bar: the side
		// chat's own tabs, + and close are its title.
		TourSideChatDock = new QDockWidget{ this };
		TourSideChatDock->setObjectName(QStringLiteral("workbenchTourSideChat"));
		TourSideChatDock->setWindowTitle(tr("Side chat"));
		TourSideChatDock->setFeatures(QDockWidget::DockWidgetClosable);
		TourSideChatDock->setAllowedAreas(Qt::RightDockWidgetArea);
		TourSideChatDock->setTitleBarWidget(new QWidget{ TourSideChatDock });
		TourSideChatDock->setWidget(TourEditor->side_chat());
		addDockWidget(Qt::RightDockWidgetArea, TourSideChatDock);
		resizeDocks({ TourSideChatDock }, { 380 }, Qt::Horizontal);
		TourSideChatDock->hide();
		TourEditor->set_side_chat_visibility_dispatch([this](const bool aShown) {
			TourSideChatDock->setVisible(aShown);
			if (aShown) {
				resizeDocks({ TourSideChatDock }, { std::max(360, TourSideChatDock->width()) }, Qt::Horizontal);
			}
		});
		TrayPages->addWidget(makeTabColumn(TourHolder, QStringLiteral("workbenchTourScroll")));
		wire_tour_page();
		TrayPages->addWidget(AgentHome);
		// [[BPR-1290]]. The tab row carries the Comments tab's tools at its
		// right: Only new, Hide resolved and Next new reply.
		auto* const trayHeader = new drawer_header{ conversationShell };
		trayHeader->setObjectName(QStringLiteral("workbenchTrayHeader"));
		CommentsTools = new QWidget{ trayHeader };
		CommentsTools->setObjectName(QStringLiteral("workbenchCommentsTools"));
		auto* const commentsToolsRow = new QHBoxLayout{ CommentsTools };
		commentsToolsRow->setContentsMargins(0, 0, 0, 0);
		commentsToolsRow->setSpacing(1);
		// [[BPR-1887]]. Navigation belongs with the tools, outside the
		// viewport where a note can hang from any code line.
		commentsToolsRow->addWidget(ConversationPanel->cards()->pull_request_comments_chip(), 0, Qt::AlignVCenter);
		commentsToolsRow->addSpacing(4);
		commentsToolsRow->addWidget(ConversationPanel->read_tools());
		NextNewCommentButton->setParent(CommentsTools);
		commentsToolsRow->addWidget(NextNewCommentButton);
		// [[BPR-1592]]. The Notes tab's tools: a new note on the selected line
		// (N), and the notes on the pull request.
		NotesTools = new QWidget{ CommentsTools->parentWidget() };
		NotesTools->setObjectName(QStringLiteral("workbenchNotesTools"));
		auto* const notesToolsRow = new QHBoxLayout{ NotesTools };
		notesToolsRow->setContentsMargins(0, 0, 0, 0);
		notesToolsRow->setSpacing(1);
		PrivateNotes->new_line_action()->setIcon(conversation_icon(conversation_glyph::comment, palette()));
		PrivateNotes->elsewhere_action()->setIcon(drawer_icon(drawer_glyph::pull_request, palette()));
		auto* const newNote = make_drawer_tool(NotesTools, PrivateNotes->new_line_action()->icon(), PrivateNotes->new_line_action()->toolTip(), QStringLiteral("workbenchNewNoteTool"));
		newNote->setDefaultAction(PrivateNotes->new_line_action());
		newNote->setToolButtonStyle(Qt::ToolButtonIconOnly);
		notesToolsRow->addWidget(newNote);
		auto* const elsewhere = make_drawer_tool(NotesTools, drawer_icon(drawer_glyph::pull_request, palette()), PrivateNotes->elsewhere_action()->text(), QStringLiteral("workbenchNotesElsewhereTool"));
		elsewhere->setDefaultAction(PrivateNotes->elsewhere_action());
		elsewhere->setToolButtonStyle(Qt::ToolButtonIconOnly);
		notesToolsRow->addWidget(elsewhere);
		NotesTools->hide();
		// [[BPR-1593]]. The Tour tab's tools: Add stop, and Expand to the
		// tour page.
		TourTools = new QWidget{ CommentsTools->parentWidget() };
		TourTools->setObjectName(QStringLiteral("workbenchTourTools"));
		auto* const tourToolsRow = new QHBoxLayout{ TourTools };
		tourToolsRow->setContentsMargins(0, 0, 0, 0);
		tourToolsRow->setSpacing(1);
		auto* const addStop = make_drawer_tool(TourTools, conversation_icon(conversation_glyph::comment, palette()), tr("Add a stop on the selected lines"), QStringLiteral("workbenchAddTourStopTool"));
		connect(addStop, &QToolButton::clicked, this, [this] { TourEditor->trigger(tour_editor_action::add_stop); });
		tourToolsRow->addWidget(addStop);
		auto* const expandTour = make_drawer_tool(TourTools, conversation_icon(conversation_glyph::maximize, palette()), tr("Expand to the tour page"), QStringLiteral("workbenchExpandTourTool"));
		connect(expandTour, &QToolButton::clicked, this, [this] { open_tour_page(false); });
		tourToolsRow->addWidget(expandTour);
		TourTools->hide();
		auto* const trayTools    = new QWidget{ trayHeader };
		auto* const trayToolsRow = new QHBoxLayout{ trayTools };
		trayToolsRow->setContentsMargins(0, 0, 0, 0);
		trayToolsRow->setSpacing(0);
		CommentsTools->setParent(trayTools);
		NotesTools->setParent(trayTools);
		TourTools->setParent(trayTools);
		trayToolsRow->addWidget(CommentsTools);
		trayToolsRow->addWidget(NotesTools);
		trayToolsRow->addWidget(TourTools);
		trayHeader->set_parts(TrayTabs, trayTools);
		// [[BPR-1592]] [[BPR-1684]]. RFC-BPR-0085 D5: where the names do
		// not fit the tabs go compact, so four tabs never scroll.
		trayHeader->installEventFilter(this);
		TrayHeader = trayHeader;
		fit_tray_tabs(true);
		conversationShellLayout->addWidget(trayHeader);
		conversationShellLayout->addWidget(TrayPages, 1);
		AiToolsAction = new QAction{ tr("Agent"), this };
		AiToolsAction->setObjectName(QStringLiteral("showWorkbenchAiToolsAction"));
		AiToolsAction->setCheckable(true);
		AiToolsAction->setToolTip(
			tr("Agent conversations, findings and private history. Nothing here is published; promote an item to a draft in Comments to publish it."));
		connect(TrayTabs, &QTabBar::currentChanged, this, [this](const int aIndex) {
			TrayPages->setCurrentIndex(aIndex);
			// [[BPR-1684]]. A compact row names the open tab.
			fit_tray_tabs();
			CommentsTools->setVisible(aIndex == tray_comments_tab);
			NotesTools->setVisible(aIndex == tray_notes_tab);
			TourTools->setVisible(aIndex == tray_tour_tab);
			if (aIndex == tray_notes_tab && PrivateNotes != nullptr) {
				PrivateNotes->refresh_geometry();
			}
			apply_thread_marks();
			const auto blocker = QSignalBlocker{ AiToolsAction };
			AiToolsSelected    = aIndex == tray_findings_tab;
			AiToolsAction->setChecked(AiToolsSelected);
			update_dif_presentation();
			if (AiToolsSelected && showing_ai_tools()) {
				play_dif_cue(DifCues->reveal_findings(DifCueClock->elapsed()));
			}
		});
		connect(AiToolsAction, &QAction::toggled, this, [this](bool aSelected) { TrayTabs->setCurrentIndex(aSelected ? tray_findings_tab : tray_comments_tab); });
		// [[BPR-1781]]. Overview's drawer: the conversation tabs Review's
		// tray has, borrowed while Overview shows, and the tour's summary.
		if (auto* const drawer = OverviewPage->status_drawer(); drawer != nullptr) {
			drawer->tour_summary()->set_dispatch([this](const overview_tour_gesture aGesture) { follow_overview_tour(aGesture); });
			drawer->set_tab_observer([this](const overview_drawer_tab aTab) {
				if (!ConversationTabsLent) {
					return;
				}
				if (aTab == overview_drawer_tab::notes && PrivateNotes != nullptr) {
					PrivateNotes->refresh_geometry();
				}
				if (aTab == overview_drawer_tab::agent && AgentTab != nullptr) {
					AgentTab->refresh();
					schedule_agent_suggestions();
				}
			});
		}
		ConversationPanel->cards()->set_pull_request_link_dispatch([this](const std::string& aIdentity) { open_pull_request_conversation(aIdentity); });
		// [[BPR-1781]]. The new comment goes the way a reply does: a private
		// draft first, posted only by its Send, by the composition.
		ConversationPanel->set_new_comment_dispatch([this](const core::conversation_reply_action aAction, const QString& aText) {
			if (ConversationReplyDispatch) {
				ConversationReplyDispatch(aAction, { std::string{ core::new_pull_request_comment }, std::string{ }, std::string{ }, aText.toStdString() });
			}
		});
		TourEditor->set_presented_observer([this] {
			refresh_overview_tour();
			refresh_tour_open_offer();
		});
		// [[BPR-1806]]. A published tour stop's Open in tour.
		ConversationPanel->cards()->set_tour_open_dispatch([this](const std::string& aPost, const core::published_tour_stop& aStop) {
			if (TourStopDispatch) {
				TourStopDispatch(aPost, aStop);
			} else {
				start_tour_at_published_stop(aPost, aStop);
			}
		});
		// [[BPR-1810]]. A stop's chevrons: the neighbouring stop opens where
		// the reader is, in place among these notes or at its own line.
		ConversationPanel->cards()->set_tour_step_dispatch([this](const std::string& aThread) {
			auto* const cards = ConversationPanel->cards();
			if (std::ranges::any_of(cards->cards(), [&aThread](const thread_card& aCard) { return aCard.Id == aThread; })) {
				cards->open_thread(aThread);
				return;
			}
			go_to_conversation(aThread);
		});
		Drawers[1].Panel->set_content_widget(
			conversationShell, ConversationPanel->cards());
		CentralSplitter->addWidget(Drawers[2].Panel);
		CentralSplitter->addWidget(Drawers[0].Panel);
		CentralSplitter->addWidget(CentreSurface);
		CentralSplitter->addWidget(Drawers[1].Panel);
		// [[BPR-0646]]. The queue landing leaves the review header, which is no
		// longer a scroll area, and sits under the file list until the
		// repositories drawer of RFC-BPR-0028 takes it over ([[BPR-0651]]).
		// The review queue is how a pull request is chosen, so it is Home's
		// list, under the landing card, with the room the drawer could not
		// give it. The files drawer holds files.
		if (auto* const homeColumn = qobject_cast<QVBoxLayout*>(HomePage->layout());
			homeColumn != nullptr) {
			// [[BPR-0650]]. Open by link or owner/name#number, in the bar
			// above the list: the same intent the locator sends, and the
			// pull request arrives on Overview.
			// [[BPR-0669]]. The bar is the way a pull request is named: it
			// takes what a reviewer has on the clipboard, whole. It had no
			// label and no button, so the only affordance was a placeholder
			// and the Return key, and the owner went looking in Settings.
			// [[BPR-0775]]. RFC-BPR-0032's address field, alone in Home's first
			// row: the label "Open", the placeholder and Command-L inside one
			// bordered field, and Return to open. The heading over it and the
			// Open button beside it are gone; the action stays for Return and
			// the menu.
			auto* const openFrame = new home_bar_frame{ HomePage };
			openFrame->setObjectName(QStringLiteral("homeOpenFrame"));
			HomeOpenFrame = openFrame;
			HomeOpenLabel = new QLabel{ QStringLiteral("Open"), openFrame };
			HomeOpenLabel->setObjectName(QStringLiteral("homeOpenLabel"));
			auto openLabelFont = HomeOpenLabel->font();
			openLabelFont.setWeight(QFont::DemiBold);
			HomeOpenLabel->setFont(openLabelFont);
			HomeOpenBar = new QLineEdit{ openFrame };
			HomeOpenBar->setObjectName(QStringLiteral("homeOpenBar"));
			HomeOpenBar->setAccessibleName(QStringLiteral("Open a pull request"));
			HomeOpenBar->setAccessibleDescription(QStringLiteral(
				"Paste a pull request link, or owner/name#number, and press Return"));
			HomeOpenBar->setPlaceholderText(
				QStringLiteral("Paste a link, or write owner/name#123"));
			HomeOpenBar->setClearButtonEnabled(true);
			HomeOpenBar->setFrame(false);
			HomeOpenLabel->setBuddy(HomeOpenBar);
			openFrame->watch(HomeOpenBar);
			HomeOpenAction = new QAction{ tr("Open"), this };
			HomeOpenAction->setObjectName(QStringLiteral("homeOpenAction"));
			HomeFocusBarAction = new QAction{ tr("Go to the Open bar"), this };
			HomeFocusBarAction->setObjectName(QStringLiteral("homeFocusBarAction"));
			HomeFocusBarAction->setShortcuts(shortcut_keys(shortcut_command::home_open_bar));
			HomeFocusBarAction->setShortcutContext(Qt::WindowShortcut);
			addAction(HomeFocusBarAction);
			connect(HomeFocusBarAction, &QAction::triggered, this, [this] { focus_home_bar(); });
			auto* const openShortcut = new QLabel{
				HomeFocusBarAction->shortcut().toString(QKeySequence::NativeText), openFrame
			};
			openShortcut->setObjectName(QStringLiteral("homeOpenShortcut"));
			openShortcut->setForegroundRole(QPalette::PlaceholderText);
			openShortcut->setAccessibleName(QStringLiteral("Shortcut to the Open bar"));
			HomeOpenShortcut     = openShortcut;
			HomeOpenActivity     = new home_bar_activity{ openFrame };
			auto* const frameRow = new QHBoxLayout{ openFrame };
			frameRow->setContentsMargins(12, 8, 12, 8);
			frameRow->setSpacing(8);
			frameRow->addWidget(HomeOpenLabel);
			frameRow->addWidget(HomeOpenBar, 1);
			frameRow->addWidget(HomeOpenActivity);
			frameRow->addWidget(openShortcut);
			// [[BPR-0779]]. RFC-BPR-0032 puts the bar "alone in the first row",
			// and the accepted board H1 of `design/home-v2.html` puts first
			// launch's slack in a fill *below* the sign-in card. Signed in, the
			// list takes the column's spare height. Signed out the list is
			// hidden, and this frame's default Preferred policy let it absorb
			// that height instead, drawing its rounded border around some 530
			// points of nothing — the empty box A6 forbids. The frame is
			// bordered, so its height is the box: it keeps the height it asks
			// for in every session state.
			openFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
			// [[BPR-1381]]. The owner, 2026-10-10: a row above the bar "causes
			// all the layout of the page to shift and that section goes away
			// when you open a new pr which shifts the screen under your
			// cursor". [[BPR-1843]]: the owner, 2026-11-03, asked for the open
			// review as a card of its own above the lists. It stands under the
			// bar (see place_home_landing), so the bar never moves, and its
			// place is held while another pull request opens from Home.
			auto* const openRow = new QHBoxLayout{ };
			openRow->setContentsMargins(24, 16, 24, 0);
			openRow->setSpacing(12);
			openRow->addWidget(openFrame, 1);
			homeColumn->addLayout(openRow);
			HomeOpenRow = openRow;
			// A refusal belongs beside what was refused. Silence on Return was
			// indistinguishable from a product that does nothing.
			HomeOpenNotice = new QLabel{ HomePage };
			HomeOpenNotice->setObjectName(QStringLiteral("homeOpenNotice"));
			HomeOpenNotice->setWordWrap(true);
			HomeOpenNotice->hide();
			HomeOpenNotice->setTextFormat(Qt::PlainText);
			// [[BPR-0771]]. A pull request that could not be loaded is said
			// here, once, in one sentence, with its details a press away. The
			// banner on Accounts & hosts that used to say it is gone.
			HomeOpenDetails = new quiet_push_button{ tr("Copy details"), HomePage };
			HomeOpenDetails->setObjectName(QStringLiteral("homeOpenCopyDetails"));
			HomeOpenDetails->setAccessibleName(QStringLiteral("Copy the details of the refusal"));
			HomeOpenDetails->setFlat(true);
			HomeOpenDetails->setAutoDefault(false);
			HomeOpenDetails->setFocusPolicy(Qt::StrongFocus);
			HomeOpenDetails->hide();
			// [[BPR-1357]]. Both now speak in Home's status band, at body size
			// behind a mark that says their tone. The band also carries Home's
			// general status, and Cancel or Retry for what the bar opened.
			// [[BPR-1381]]. It is docked at Home's foot, under the list, rather
			// than under the bar: shown and hidden there it moves nothing
			// above it, where a band under the bar pushed the list down under
			// the pointer. What the bar is opening is also said in the bar.
			HomeStatus     = new qt::home_status_line{ HomePage };
			HomeStatusText = new QLabel{ HomeStatus };
			HomeStatusText->setObjectName(QStringLiteral("homeStatusText"));
			HomeStatusText->setAccessibleName(QStringLiteral("Home status"));
			HomeStatusText->setWordWrap(true);
			HomeStatusText->setTextFormat(Qt::PlainText);
			HomeStatusText->setTextInteractionFlags(Qt::TextSelectableByMouse);
			HomeStatusText->hide();
			HomeOpenCancel = new quiet_push_button{ tr("Cancel"), HomeStatus };
			HomeOpenCancel->setObjectName(QStringLiteral("homeOpenCancel"));
			HomeOpenCancel->setAutoDefault(false);
			HomeOpenCancel->setFocusPolicy(Qt::StrongFocus);
			HomeOpenCancel->hide();
			HomeOpenRetry = new quiet_push_button{ tr("Retry"), HomeStatus };
			HomeOpenRetry->setObjectName(QStringLiteral("homeOpenRetry"));
			HomeOpenRetry->setAutoDefault(false);
			HomeOpenRetry->setFocusPolicy(Qt::StrongFocus);
			HomeOpenRetry->hide();
			connect(HomeOpenCancel, &QPushButton::clicked, this, [this] { dispatch_home_open_action(); });
			connect(HomeOpenRetry, &QPushButton::clicked, this, [this] { dispatch_home_open_action(); });
			HomeStatus->adopt_text(*HomeOpenNotice);
			HomeStatus->adopt_text(*HomeStatusText);
			HomeStatus->adopt_button(*HomeOpenCancel);
			HomeStatus->adopt_button(*HomeOpenRetry);
			HomeStatus->adopt_button(*HomeOpenDetails);
			auto* const noticeRow = new QHBoxLayout{ };
			noticeRow->setContentsMargins(24, 0, 24, 0);
			noticeRow->addWidget(HomeStatus, 1);
			connect(HomeOpenDetails, &QPushButton::clicked, this, [this] {
				if (auto* const clipboard = QApplication::clipboard(); clipboard != nullptr) {
					clipboard->setText(HomeRefusalDetails);
				}
			});
			// [[BPR-1200]]. A pull request this connection cannot read is refused
			// in a card of its own under the bar, not in the line above: the
			// owner missed that line. It names the pull request, says why in
			// terms of the connection and offers what could open it.
			HomeRefusal = new home_refusal{ HomePage };
			homeColumn->addWidget(HomeRefusal);
			HomeRefusal->set_action_dispatch([this](const core::pull_request_refusal_action aAction) {
				take_refusal_action(aAction);
			});
			HomeRefusal->set_copy_dispatch([this] {
				if (auto* const clipboard = QApplication::clipboard(); clipboard != nullptr) {
					clipboard->setText(HomeRefusalDetails);
				}
			});
			auto* setupRow = new QHBoxLayout{ };
			setupRow->setContentsMargins(24, 2, 24, 4);
			SetupContinueButton = new QPushButton{ HomePage };
			SetupContinueButton->setObjectName(QStringLiteral("homeSetupContinue"));
			SetupCancelButton = new quiet_push_button{ tr("Cancel setup"), HomePage };
			SetupCancelButton->setObjectName(QStringLiteral("homeSetupCancel"));
			setupRow->addWidget(SetupContinueButton);
			setupRow->addWidget(SetupCancelButton);
			setupRow->addStretch();
			homeColumn->addLayout(setupRow);
			// The first-launch card sits under this row (see place_home_landing).
			HomeSetupRow = setupRow;
			// [[BPR-1865]]. The owner, 2026-11-04: "I want to be able to fuzzy
			// filter the home screen to quickly find a PR." One quiet line
			// under the bar, above everything it filters: the Active review
			// card, Review Cycles and every panel of the list. Command-F (the
			// key moves here while Home shows, as Refresh does), "/" or a
			// character typed in a list takes the keyboard to it.
			HomeFilter = new home_filter_field{ HomePage };
			HomeFilter->hide();
			homeColumn->addWidget(HomeFilter);
			HomeFilterAction = new QAction{ tr("Filter Home"), this };
			HomeFilterAction->setObjectName(QStringLiteral("homeFilterAction"));
			HomeFilterAction->setShortcutContext(Qt::WindowShortcut);
			addAction(HomeFilterAction);
			HomeFilter->set_shortcut(shortcut_key(shortcut_command::home_filter));
			connect(HomeFilterAction, &QAction::triggered, this, [this] {
				if (showing_home() && HomeFilter != nullptr && HomeFilter->isVisible()) {
					HomeFilter->focus_field();
				}
			});
			SetupContinueButton->hide();
			SetupCancelButton->hide();
			connect(SetupContinueButton, &QPushButton::clicked, this, [this] {
				if (SetupDestinationDispatch && SetupDestinationRequest) {
					SetupDestinationDispatch(application::continue_setup_destination{ *SetupDestinationRequest });
				}
			});
			connect(SetupCancelButton, &QPushButton::clicked, this, [this] {
				if (SetupDestinationDispatch) {
					SetupDestinationDispatch(application::cancel_setup_destination{ });
				}
			});
			connect(HomeOpenBar, &QLineEdit::returnPressed, this, [this] { open_from_home_bar(); });
			connect(HomeOpenAction, &QAction::triggered, this, [this] { open_from_home_bar(); });
			connect(HomeOpenBar, &QLineEdit::textChanged, this, [this] {
				if (!ApplyingSetupProjection && SetupDestinationRequest && SetupDestinationDispatch) {
					SetupDestinationDispatch(application::cancel_setup_destination{ });
				}
				if (HomeOpenNotice != nullptr && HomeOpenNotice->isVisible()) {
					// A refusal the reviewer is already rewriting stays dismissed
					// until the next load rather than returning on the next projection.
					if (!ApplyingSetupProjection && HomeSetupNotice.isEmpty()) {
						DismissedHomeRefusal = HomeOpenNotice->text();
					}
					HomeOpenNotice->hide();
				}
				if (HomeOpenDetails != nullptr) {
					HomeOpenDetails->hide();
				}
				if (HomeRefusal != nullptr && HomeRefusal->isVisible()) {
					if (!ApplyingSetupProjection && HomeSetupNotice.isEmpty()) {
						DismissedHomeRefusal = HomeRefusal->text();
					}
					HomeRefusal->hide();
				}
				if (HomeOpenAction != nullptr) {
					HomeOpenAction->setEnabled(
						!HomeOpenBar->text().trimmed().isEmpty()
						&& (SessionPanel == nullptr || SessionPanel->status() != core::github_status::establishing_session));
				}
			});
			HomeOpenAction->setEnabled(false);
			// [[BPR-0733]] [[BPR-0776]]. What this window read last, most recent
			// first. The app no longer opens the last pull request by itself at
			// startup, so this list is how a reviewer goes back to one, by a
			// gesture. It is governed by the entries it was given rather than by
			// the account: the composition has already filtered them for this
			// window's session, so a signed-out Home lists its public reading.
			// It is the last panel of Home's list, and Resume is its rows'
			// action: on hover, on focus and in the row's menu.
			HomeRecentResumeAction = new QAction{ tr("Resume"), this };
			HomeRecentResumeAction->setObjectName(QStringLiteral("homeRecentResumeAction"));
			HomeRecentResumeAction->setToolTip(QStringLiteral(
				"Opens the selected recently viewed pull request where its reading was left"));
			HomeRecentResumeAction->setEnabled(false);
			// Recently viewed keeps the last ten pull requests read, so the
			// reviewer can take one off it, from the row's menu or by Delete.
			// Delete is the tour editor's Remove in this window, so the list
			// reads the key itself, as Settings › Repositories' list does, and
			// the action claims no sequence.
			HomeRecentRemoveAction = new QAction{ tr("Remove from Recently Viewed"), this };
			HomeRecentRemoveAction->setObjectName(QStringLiteral("homeRecentRemoveAction"));
			HomeRecentRemoveAction->setStatusTip(QStringLiteral(
				"Takes the selected pull request off Recently viewed; nothing changes on GitHub"));
			HomeRecentRemoveAction->setEnabled(false);
			auto* const recentList = new home_recent_view{ HomePage };
			recentList->hide();
			HomeRecentList = recentList;
			HomeRecentList->setObjectName(QStringLiteral("homeRecentList"));
			HomeRecentList->setAccessibleName(QStringLiteral("Recently viewed pull requests"));
			// [[BPR-1364]]. A single click only selects the row.
			HomeRecentList->setAccessibleDescription(
				home_open_description() + QStringLiteral("; Shift and Return resume its saved reading"));
			// The action is the row's menu as well as its hover and focus
			// button, so the gesture is where the row is. [[BPR-1342]]: the
			// row's menu is Home's one row menu, opened on the row the right
			// click made current, with Open Review and Open Review in New
			// Window before Resume and Open on GitHub ([[BPR-1338]]). Its
			// key, ⇧Return, is shown there; it is read by the list, below.
			HomeRecentResumeAction->setShortcuts(shortcut_keys(shortcut_command::home_resume));
			HomeRecentResumeAction->setShortcutContext(Qt::WidgetShortcut);
			HomeRecentResumeAction->setShortcutVisibleInContextMenu(true);
			HomeRecentList->setContextMenuPolicy(Qt::CustomContextMenu);
			connect(HomeRecentList, &QWidget::customContextMenuRequested, this, [this](const QPoint& aPosition) {
				auto* const item = HomeRecentList->itemAt(aPosition);
				if (item == nullptr) {
					return;
				}
				claim_home_selection(home_selection_list::recent);
				HomeRecentList->setCurrentItem(item);
				popup_home_row_menu(HomeRecentList->viewport()->mapToGlobal(aPosition));
			});
			HomeRecentList->installEventFilter(this);
			// [[BPR-1364]]. The pointer opens a row by a double-click, never a
			// single click: `itemActivated` is a single click wherever the style
			// asks for single-click activation (KDE). Return is read below.
			connect(HomeRecentList, &QListWidget::itemDoubleClicked, this,
					[this](QListWidgetItem* const aItem) {
						dispatch_recent_review(
							aItem == nullptr ? -1 : HomeRecentList->row(aItem), false);
					});
			// [[BPR-1365]]. A row made current while another list has Home's
			// keyboard is not shown selected.
			connect(HomeRecentList->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
				if (HomeSelection != home_selection_list::recent) {
					home_hide_selection(*HomeRecentList);
				}
			});
			connect(HomeRecentList, &QListWidget::currentRowChanged, this,
					[this](int) {
						update_recent_resume_action();
						update_open_on_github_action();
						update_home_review_actions();
					});
			connect(HomeRecentResumeAction, &QAction::triggered, this, [this] {
				dispatch_recent_review(HomeRecentList->currentRow(), true);
			});
			connect(HomeRecentRemoveAction, &QAction::triggered, this, [this] {
				remove_recent_review(HomeRecentList->currentRow());
			});
			// [[BPR-1157]]. Review Cycles beside the pull request ways in: Open
			// Project and the cycles this profile keeps, most recent first. It
			// needs no account, so it stays whether or not one is signed in.
			HomeCycles = new QWidget{ HomePage };
			HomeCycles->setObjectName(QStringLiteral("homeReviewCycles"));
			HomeCycles->setAccessibleName(QStringLiteral("Review Cycles"));
			HomeCycles->setContentsMargins(24, 4, 24, 4);
			auto* const cyclesColumn = new QVBoxLayout{ HomeCycles };
			cyclesColumn->setContentsMargins(0, 0, 0, 0);
			auto* const cyclesHeader = new QHBoxLayout{ };
			auto* const cyclesTitle  = new QLabel{ QStringLiteral("Review Cycles"), HomeCycles };
			cyclesTitle->setObjectName(QStringLiteral("homeReviewCyclesTitle"));
			auto cyclesFont = cyclesTitle->font();
			cyclesFont.setWeight(QFont::DemiBold);
			cyclesTitle->setFont(cyclesFont);
			cyclesHeader->addWidget(cyclesTitle);
			cyclesHeader->addStretch(1);
			auto* const openProject = new quiet_tool_button{ HomeCycles };
			openProject->setObjectName(QStringLiteral("homeOpenProjectButton"));
			openProject->setText(QStringLiteral("Open Project…"));
			openProject->setToolTip(QStringLiteral("Review a local folder or file: no account, remote or agent needed"));
			cyclesHeader->addWidget(openProject);
			cyclesColumn->addLayout(cyclesHeader);
			HomeCyclesList = new QListWidget{ HomeCycles };
			HomeCyclesList->setObjectName(QStringLiteral("homeReviewCyclesList"));
			HomeCyclesList->setAccessibleName(QStringLiteral("Recent review cycles"));
			// [[BPR-1364]]. As every Home list: a single click selects, a
			// double-click or Return opens. Return is read in `eventFilter`,
			// because on macOS it is an item view's editing key.
			HomeCyclesList->setAccessibleDescription(home_open_description());
			HomeCyclesList->installEventFilter(this);
			HomeCyclesList->setMaximumHeight(HomeCyclesList->fontMetrics().lineSpacing() * 5 + 8);
			HomeCyclesList->hide();
			cyclesColumn->addWidget(HomeCyclesList);
			homeColumn->addWidget(HomeCycles);
			connect(openProject, &QToolButton::clicked, this, [this] {
				if (OpenProjectAction != nullptr) {
					OpenProjectAction->trigger();
				}
			});
			connect(HomeCyclesList->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
				if (HomeSelection != home_selection_list::cycles) {
					home_hide_selection(*HomeCyclesList);
				}
			});
			connect(HomeCyclesList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* const aItem) {
				resume_home_cycle(aItem == nullptr ? -1 : HomeCyclesList->row(aItem));
			});
			build_queue_landing(HomePage, *homeColumn);
			if (auto* const landing = HomePage->findChild<QWidget*>(
					QStringLiteral("reviewQueueLanding"), Qt::FindDirectChildrenOnly);
				landing != nullptr) {
				QueueSurface = landing;
				landing->setMaximumHeight(QWIDGETSIZE_MAX);
				landing->setContentsMargins(24, 0, 24, 12);
				homeColumn->setStretchFactor(landing, 1);
			}
			// [[BPR-1200]]. With no list to fill it, Home's slack is a fill below
			// the last thing shown, as the accepted board puts it under the
			// first-launch card, so a refusal or a load in progress is said
			// directly under the bar rather than wherever the spare height
			// spread the rows. The list's stretch still takes it when shown.
			homeColumn->addStretch(0);
			homeColumn->addLayout(noticeRow);
			if (HomeList != nullptr) {
				HomeList->adopt_recent(*recentList);
				// [[BPR-1843]]. A Recently viewed row's Open on GitHub, from its
				// icon button: the queue row's chip, naming this row's page.
				HomeList->set_recent_page_dispatch([this](const int aRow) {
					if (aRow >= 0 && static_cast<std::size_t>(aRow) < RecentReviews.Entries.size()) {
						open_pull_request_page(application::recent_review_row{ static_cast<std::size_t>(aRow) });
					}
				});
				// [[BPR-1342]]. ⌘-click or middle-click on a Recently viewed row.
				HomeList->set_recent_new_window_dispatch([this](const int aRow) {
					if (aRow >= 0 && static_cast<std::size_t>(aRow) < RecentReviews.Entries.size()) {
						request_review_window(application::recent_review_row{ static_cast<std::size_t>(aRow) });
					}
				});
			}
			// Nothing was published yet, so there is no panel to show.
			apply_recent_reviews();
		}
		// Only the centre absorbs a window resize, so a drawer keeps the width the
		// host persisted until the user moves the handle.
		CentralSplitter->setStretchFactor(0, 0);
		CentralSplitter->setStretchFactor(1, 0);
		CentralSplitter->setStretchFactor(2, 1);
		CentralSplitter->setStretchFactor(3, 0);

		// [[BPR-0662]]. A width leaves for the host only from a handle drag.
		// Whatever the splitter is given by a resize -- the window, the
		// stack showing it, a hidden page laid out at its default size -- is
		// a squeeze the splitter itself undoes once there is room, and it is
		// never written back.
		connect(CentralSplitter, &QSplitter::splitterMoved, this,
				[this](int, int) { observe_handle_drag(); });
		connect(
			Drawers[0].Panel->contents_view()->selectionModel(),
			&QItemSelectionModel::currentChanged,
			this,
			[this](const QModelIndex& aCurrent) {
				dispatch_file_selection(aCurrent);
			});
		connect(
			ConversationPanel->view()->selectionModel(),
			&QItemSelectionModel::currentChanged,
			this,
			[this](const QModelIndex& aCurrent) {
				if (!ApplyingConversationAnchor) {
					synchronize_selected_conversation(aCurrent);
				}
				update_defect_surface();
				// [[BPR-1133]]. A thread chosen is what Comment now answers.
				update_comment_target();
			});
		update_defect_surface();
	}
}
