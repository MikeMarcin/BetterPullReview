#include "private_review_fixture_course.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/private_composition_editor.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "github_application_fixture_access.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTabBar>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <chrono>
namespace bpr::app::detail {
	namespace {
		struct course
		{
			std::string                           Phase;
			std::string                           Representation;
			int                                   Stage   = 0;
			std::chrono::steady_clock::time_point Started = std::chrono::steady_clock::now();
			course_modality_step                  Modality{ .Modality = qt::review_modality::review, .Needs = { .Comparison = true, .FilesDrawer = true, .ConversationsDrawer = true } };
			core::private_review_thread_id        Thread;
			int                                   SharedResets = 0;
			int                                   SharedRows   = 0;
			QString                               Theme;
			bool                                  ThemeApplied      = false;
			bool                                  CapturedInput     = false;
			bool                                  CapturedCandidate = false;
		};
		void tick(github_application& aApp, std::shared_ptr<course> aCourse)
		{
			auto& window = aApp.window();
			auto* panel  = window.private_review();
			auto* editor = window.review_editor();
			auto  fail   = [&](const char* aReason) {
				QTextStream{ stdout } << "PRIVATE_REVIEW_COURSE_FAILED phase=" << QString::fromStdString(aCourse->Phase)
									  << " stage=" << aCourse->Stage << " reason=" << aReason
									  << " shared_rows=" << window.conversation_model()->rowCount() << " shared_resets=" << aCourse->SharedResets
									  << " status=" << panel->findChild<QLabel*>(QStringLiteral("privateReviewStatus"))->text()
									  << " agent=" << window.agent_panel()->status_text() << "\n";
				aApp.request_quit(EXIT_FAILURE);
			};
			if (std::chrono::steady_clock::now() - aCourse->Started > std::chrono::seconds{ 90 }) {
				fail("condition_timeout");
				return;
			}
			auto click = [](QWidget* aWidget, const char* aName) {
				auto* button = aWidget->findChild<QPushButton*>(QString::fromLatin1(aName));
				if (!button || !button->isEnabled()) {
					return false;
				}
				button->click();
				return true;
			};
			auto capture = [](QWidget* aWidget, const QString& aName) {
				const auto render = qEnvironmentVariable("BPR_PRIVATE_REVIEW_RENDER");
				if (render.isEmpty()) {
					return true;
				}
				const auto path = QFileInfo{ render };
				return aWidget->grab().save(path.absolutePath() + QLatin1Char('/') + path.completeBaseName() + QLatin1Char('-') + aName + QStringLiteral(".png"));
			};
			switch (aCourse->Stage) {
			case 0:
				if (!aCourse->ThemeApplied) {
					const auto mode = aCourse->Theme == QStringLiteral("light")   ? core::appearance_mode::light
						: aCourse->Theme == QStringLiteral("dark")                ? core::appearance_mode::dark
						: aCourse->Theme == QStringLiteral("high-contrast-light") ? core::appearance_mode::high_contrast_light
						: aCourse->Theme == QStringLiteral("high-contrast-dark")  ? core::appearance_mode::high_contrast_dark
																				  : core::appearance_mode::system;
					if (!github_application_fixture_access::apply_appearance_gesture(aApp, mode, 4)) {
						break;
					}
					aCourse->ThemeApplied = true;
				}
				window.resize(1200, 900);
				window.github_connect_action()->trigger();
				++aCourse->Stage;
				break;
			case 1:
				if (window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled()) {
					window.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
					window.home_open_action()->trigger();
					++aCourse->Stage;
				}
				break;
			case 2:
				if (github_application_fixture_access::enter_course_modality(aApp, aCourse->Modality) == course_modality_result::entered) {
					auto* files = window.files_drawer()->contents_view();
					for (int i = 0; i < files->model()->rowCount(); ++i) {
						auto row = files->model()->index(i, 0);
						if (row.data().toString() == QStringLiteral("review.cpp")) {
							files->selectionModel()->setCurrentIndex(row, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
							++aCourse->Stage;
							break;
						}
					}
				}
				break;
			case 3:
				if (window.comparison_surface()->current_semantic_index() && !window.comparison_surface()->geometry_pending()) {
					if (window.comparison_surface()->select_conversation_line(core::diff_side::right, 1)) {
						window.tray_tabs()->setCurrentIndex(1);
						QObject::connect(window.conversation_model(), &QAbstractItemModel::modelReset, &window, [aCourse] { ++aCourse->SharedResets; });
						++aCourse->Stage;
					}
				}
				break;
			case 4:
				if (!panel->isEnabled() || !editor->shown_draft()) {
					break;
				}
				aCourse->SharedRows = window.conversation_model()->rowCount();

				if (aCourse->Phase == "seed") {
					panel->findChild<QTabWidget*>(QStringLiteral("privateReviewPages"))->setCurrentIndex(2);
					panel->findChild<QPlainTextEdit*>(QStringLiteral("privateReviewerText"))->setPlainText(QStringLiteral("Synthetic private source for RFC-0023."));
					if (click(panel, "privateCreateSelected")) {
						++aCourse->Stage;
					}
				} else if (panel->turn_model()->rowCount() >= 2) {
					++aCourse->Stage;
				}
				break;
			case 5:
				if (aCourse->Phase == "seed") {
					if (panel->turn_model()->rowCount() != 1) {
						break;
					}
					panel->findChild<QListView*>(QStringLiteral("privateTurnList"))->setCurrentIndex(panel->turn_model()->index(0));
					click(panel, "privateSelectSource");
					auto* profile = panel->findChild<QComboBox*>(QStringLiteral("privateAgentProfile"));
					profile->setCurrentIndex(profile->findData(QVariant::fromValue<qulonglong>(core::codex_jsonl_profile_id.value())));
					panel->findChild<QPlainTextEdit*>(QStringLiteral("privateReviewerText"))->setPlainText(QStringLiteral("BPR_CHAT_SEND_0514 what does this retry budget do?"));
					if (click(panel, "privateAskAgent")) {
						++aCourse->Stage;
					}
				} else if (aCourse->Phase == "compose") {
					auto* list = panel->findChild<QListView*>(QStringLiteral("privateTurnList"));
					list->setCurrentIndex(panel->turn_model()->index(0));
					click(panel, "privateSelectSource");
					list->setCurrentIndex(panel->turn_model()->index(panel->turn_model()->rowCount() - 1));
					click(panel, "privateSelectSource");
					panel->findChild<QTabWidget*>(QStringLiteral("privateReviewPages"))->setCurrentIndex(1);
					if (panel->findChild<QComboBox*>(QStringLiteral("privateFindingSources"))->count() != 1) {
						fail("finding_not_restored");
						return;
					}
					click(panel, "privateSelectFinding");
					if (!capture(panel, QStringLiteral("sources"))) {
						fail("render_sources");
						return;
					}
					if (click(panel, "privateCompose")) {
						++aCourse->Stage;
					}
				} else if (aCourse->Phase == "publish") {
					const auto wanted = aCourse->Representation == "file" ? QStringLiteral("BPR-0571 off-hunk publication.") : QStringLiteral("Synthetic reviewer-approved RFC-0023 comment.");
					if (editor->body_editor()->toPlainText() != wanted) {
						break;
					}
					if (editor->publish_action()->isEnabled()) {
						editor->publish_action()->trigger();
						aCourse->Stage = 8;
					}
				} else {
					fail("unknown_phase");
					return;
				}
				break;
			case 6:
				if (aCourse->Phase == "seed") {
					if (panel->turn_model()->rowCount() >= 3 && github_application_fixture_access::agent_invocation_evidence(aApp).Quiescent) {
						panel->findChild<QTabWidget*>(QStringLiteral("privateReviewPages"))->setCurrentIndex(0);
						panel->findChild<QListView*>(QStringLiteral("privateTurnList"))->setCurrentIndex(panel->turn_model()->index(panel->turn_model()->rowCount() - 1));
						click(panel, "privateProvenance");
						aCourse->Stage = 10;
					}
				} else {
					auto* composition = dynamic_cast<qt::private_composition_editor*>(window.findChild<QDialog*>(QStringLiteral("privateCompositionEditor")));
					if (composition && composition->isVisible()) {
						composition->findChild<QPlainTextEdit*>(QStringLiteral("privateCompositionBody"))->setPlainText(aCourse->Representation == "file" ? QStringLiteral("BPR-0571 off-hunk publication.") : QStringLiteral("Synthetic reviewer-approved RFC-0023 comment."));
						if (!aCourse->CapturedInput && !capture(composition, QStringLiteral("composition-input"))) {
							fail("render_composition_input");
							return;
						}
						aCourse->CapturedInput = true;
						if (click(composition, "privateCompositionCreate")) {
							++aCourse->Stage;
						}
					}
				}
				break;
			case 7: {
				auto* composition = dynamic_cast<qt::private_composition_editor*>(window.findChild<QDialog*>(QStringLiteral("privateCompositionEditor")));
				auto* accept      = composition ? composition->findChild<QPushButton*>(QStringLiteral("privateCompositionAccept")) : nullptr;
				if (composition && composition->isVisible() && accept && accept->isEnabled() && !aCourse->CapturedCandidate) {
					if (!capture(composition, QStringLiteral("composition-candidate"))) {
						fail("render_composition_candidate");
						return;
					}
					auto* before = composition->findChild<QPushButton*>(QStringLiteral("privateCompositionAsk"));
					auto* scroll = composition->findChild<QScrollArea*>();
					if (QGuiApplication::platformName() == QStringLiteral("cocoa")) {
						// A background native test process has no key window and cannot own focus.
						scroll->ensureWidgetVisible(accept);
					} else {
						before->setFocus(Qt::TabFocusReason);
						QKeyEvent tab{ QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier };
						QApplication::sendEvent(before, &tab);
					}
					QApplication::processEvents();
					const auto position = accept->mapTo(scroll->viewport(), QPoint{ 0, 0 });
					if ((QGuiApplication::platformName() != QStringLiteral("cocoa") && !accept->hasFocus()) || !scroll->viewport()->rect().intersects(QRect{ position, accept->size() })) {
						QTextStream{ stdout } << "PRIVATE_REVIEW_FOCUS accept=" << accept->hasFocus() << " active="
											  << (QApplication::focusWidget() ? QApplication::focusWidget()->objectName() : QStringLiteral("none"))
											  << " position=" << position.x() << ',' << position.y() << " viewport=" << scroll->viewport()->width() << 'x' << scroll->viewport()->height() << "\n";
						fail("accept_not_keyboard_visible");
						return;
					}
					if (!capture(composition, QStringLiteral("composition-action"))) {
						fail("render_composition_action");
						return;
					}
					aCourse->CapturedCandidate = true;
				}
				if (aCourse->CapturedCandidate && composition && click(composition, "privateCompositionAccept")) {
					++aCourse->Stage;
				}
			} break;
			case 8:
				if (aCourse->Phase == "publish") {
					if (editor->shown_draft() && editor->shown_draft()->State == core::review_draft_state::publishing && editor->publish_action()->isEnabled()) {
						editor->publish_action()->trigger();
					}
					if (github_application_fixture_access::review_mutation_evidence(aApp).StoredComments == 1 && window.conversation_model()->index_for_id("review-comment:PRRC_composed_1").isValid()) {
						aCourse->Stage = 9;
					}
				} else {
					const auto wanted = aCourse->Representation == "file" ? QStringLiteral("BPR-0571 off-hunk publication.") : QStringLiteral("Synthetic reviewer-approved RFC-0023 comment.");
					if (editor->body_editor()->toPlainText() == wanted && editor->shown_draft() && editor->shown_draft()->State == core::review_draft_state::drafting) {
						aCourse->Stage = 9;
					}
				}
				break;
			case 10: {
				const auto provenance = panel->findChild<QPlainTextEdit*>(QStringLiteral("privateProvenanceDetails"))->toPlainText();
				if (provenance.contains(QStringLiteral("Requested")) && provenance.contains(QStringLiteral("Configured")) && provenance.contains(QStringLiteral("unknown"))) {
					if (!capture(panel, QStringLiteral("provenance"))) {
						fail("render_provenance");
						return;
					}
					aCourse->Stage = 9;
				}
				break;
			}
			case 9: {
				const auto evidence = github_application_fixture_access::review_mutation_evidence(aApp);
				if (aCourse->Phase != "publish" && (aCourse->SharedResets != 0 || window.conversation_model()->rowCount() != aCourse->SharedRows)) {
					fail("shared_conversation_changed_by_local_operation");
					return;
				}
				if (aCourse->Phase == "publish") {
					const auto shared   = window.conversation_model()->index_for_id("review-comment:PRRC_composed_1");
					const auto body     = shared.data(qt::github_conversation_model::body_role).toString();
					const auto approved = aCourse->Representation == "file" ? QStringLiteral("BPR-0571 off-hunk publication.") : QStringLiteral("Synthetic reviewer-approved RFC-0023 comment.");
					// The view intentionally shows reviewer prose after parsing a canonical
					// permalink. The accepted server snapshot retains the original wire body.
					bool rawVerified = false;
					if (auto snapshot = github_application_fixture_access::large_review_snapshot(aApp)) {
						for (const auto& thread : snapshot->ReviewThreads) {
							for (const auto& comment : thread.Comments) {
								if (comment.Comment.value() == "PRRC_composed_1" && comment.Author && comment.Author->Login == "octo-reviewer") {
									const auto raw = QString::fromStdString(comment.Body);
									rawVerified    = aCourse->Representation == "native" ? raw == approved
																						 : raw.endsWith(approved) && raw.startsWith(QStringLiteral("https://github.com/")) && raw.contains(QStringLiteral("\n\n"));
								}
							}
						}
					}
					if (!rawVerified || body != approved || shared.data(qt::github_conversation_model::author_login_role).toString() != QStringLiteral("octo-reviewer")) {
						fail("shared_actor_or_body");
						return;
					}
				}
				if (aCourse->Phase == "publish" && (evidence.NativePosts != 1 || evidence.FilePosts != (aCourse->Representation == "file" ? 1U : 0U))) {
					fail("publication_representation");
					return;
				}
				if ((aCourse->Phase != "publish" && (evidence.Submissions || evidence.Resolutions || evidence.Replies)) || (aCourse->Phase == "publish" && evidence.StoredComments != 1)) {
					fail("mutation_canary");
					return;
				}
				const auto render = qEnvironmentVariable("BPR_PRIVATE_REVIEW_RENDER");
				if (!render.isEmpty()) {
					window.resize(1200, 900);
					if (!window.grab().save(render)) {
						fail("render");
						return;
					}
				}
				QTextStream{ stdout } << "PRIVATE_REVIEW_COURSE_OK phase=" << QString::fromStdString(aCourse->Phase) << " representation=" << QString::fromStdString(aCourse->Representation) << " turns=" << panel->turn_model()->rowCount() << " submissions=" << evidence.Submissions << "\n";
				aApp.request_quit(EXIT_SUCCESS);
				return;
			}
			default:
				fail("unknown_stage");
				return;
			}
			QTimer::singleShot(20, &window, [&aApp, aCourse] { tick(aApp, aCourse); });
		}
	}
	void schedule_private_review_course(github_application& aApp, std::string aPhase, std::string aRepresentation)
	{
		auto state            = std::make_shared<course>();
		state->Phase          = std::move(aPhase);
		state->Representation = std::move(aRepresentation);
		state->Theme          = qEnvironmentVariable("BPR_PRIVATE_REVIEW_THEME", "system");
		QTimer::singleShot(0, &aApp.window(), [&aApp, state] { tick(aApp, state); });
	}
}
