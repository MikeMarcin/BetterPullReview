/// \file
/// \brief Every quiet button reads on the face it is painted on.
///
/// \details BPR-2056 and BPR-2060 found the open card's tools drawn by the
/// macOS style as native bezels in the system appearance's grey, whatever the
/// profile's palette: a Dark profile on a light system put light marks on
/// pale squares. The same plain auto-raise tool buttons and flat push buttons
/// sat on the code block's header (Wrap, Copy), in the label menu, in the
/// agent tab and on the Overview. This course builds those surfaces in each
/// built-in theme under the default style and the macOS style in both system
/// appearances, finds every quiet button they show outside a toolbar, and
/// measures each in every state with `tests/support/qt_button_faces.hpp`.
///
/// Run under `QT_QPA_PLATFORM=cocoa` to see the native bezels; every surface
/// is drawn with `Qt::WA_DontShowOnScreen`. With `BPR_BUTTON_FACE_RENDER_DIR`
/// set, each surface is saved there in every look and theme.

#include "bpr/qt/about_window.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/conversation_finder.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/folder_list_editor.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_cycle_share_sheet.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/source_search_panel.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include "bpr/qt/tour_side_chat.hpp"
#include "bpr/qt/workbench_window.hpp"

#include "../support/qt_button_faces.hpp"

#include "bpr/application/pull_request_overview.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"

#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	namespace faces = tests::button_faces;

	void settle()
	{
		for (int pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	/// \brief What every surface reported, and the worst per surface.
	struct course
	{
		std::map<std::string, faces::report> Surfaces;
		std::vector<std::string>             Missing;
	};

	/// \brief Asks that the buttons named appear among those swept.
	void require(course& aCourse, const faces::report& aReport, const std::string& aName, const std::vector<std::string>& aNames)
	{
		for (const auto& name : aNames) {
			if (std::ranges::find(aReport.Swept, name) == aReport.Swept.end()) {
				aCourse.Missing.push_back(faces::joined({ aName, ": ", name, " was not swept" }));
			}
		}
	}

	/// \brief A host window for one surface, drawn off screen.
	[[nodiscard]] auto host_window() -> std::unique_ptr<QWidget>
	{
		auto host = std::make_unique<QWidget>();
		host->setAttribute(Qt::WA_DontShowOnScreen, true);
		host->setAutoFillBackground(true);
		auto* const column = new QVBoxLayout{ host.get() };
		column->setContentsMargins(8, 8, 8, 8);
		return host;
	}

	[[nodiscard]] auto overview_projection() -> application::pull_request_overview_projection
	{
		auto projection                = application::pull_request_overview_projection{ };
		projection.Owner               = "octo";
		projection.RepositoryName      = "engine";
		projection.Number              = core::pull_request_number{ 42U };
		projection.Title               = "Hair strand lighting fixes";
		projection.Body                = "Fixes three crashes.\n\n```cpp\nauto lock = std::scoped_lock{ Mutex };\n```\n";
		projection.AuthorLogin         = "mike";
		projection.BaseRef             = "release";
		projection.HeadRef             = "hair-strand";
		projection.CommitCount         = 4U;
		projection.ReturnedCommitCount = 4U;
		projection.Participants        = {
			{ .Login = "mike", .Role = application::overview_participant_role::author, .Signoff = application::overview_signoff::uploaded, .Viewer = true },
			{ .Login = "rcaloca", .Role = application::overview_participant_role::reviewer, .Signoff = application::overview_signoff::approved },
		};
		projection.SinceLastReview = application::overview_since_last_review{ .Known = true, .ReviewedAt = core::observed_utc_milliseconds{ 1'770'000'000'000 }, .Uploads = 2U, .FilesChanged = 2U, .RepliesToMe = 1U, .MyUnresolvedThreads = 2U };
		projection.Checks          = { .Passed = 3U, .Skipped = 1U };
		return projection;
	}

	/// \brief Opens the workbench on a review with the agent tab showing one
	/// finished turn whose answer has a fenced block.
	void open_agent_tab(qt::workbench_window& aWindow, qt::appearance_profile& aProfile)
	{
		aWindow.setAttribute(Qt::WA_DontShowOnScreen, true);
		aWindow.resize(1'440, 900);
		aProfile.apply_to(aWindow);
		aWindow.show();
		auto opened   = application::workbench_projection{ };
		auto commands = core::workbench_command_delta{ };
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			commands.Commands[index] = { .Id = static_cast<core::workbench_command_id>(index), .Enabled = true, .DisabledReason = core::workbench_reason_code::none };
		}
		opened.Commands = commands;
		opened.Drawers  = core::workbench_drawer_delta{ .Files = core::workbench_drawer_state::expanded, .Conversations = core::workbench_drawer_state::expanded };
		opened.Layout   = application::workbench_layout_delta{ .FilesWidth = 260U, .ConversationsWidth = 520U };
		aWindow.publish(opened);
		settle();
		aWindow.choose_modality(qt::review_modality::review);
		settle();
		aWindow.tray_tabs()->setCurrentIndex(3);
		auto profile        = core::agent_profile{ };
		profile.Id          = core::agent_profile_id{ 1U };
		profile.DisplayName = "Claude Code";
		profile.Executable  = "/usr/bin/true";
		aWindow.agent_panel()->offer_profiles(std::vector{ profile });
		settle();
		auto* const agent = aWindow.agent_tab();
		agent->adopt_appearance(aProfile);
		agent->present_thread({ { .Question = QStringLiteral("Where is the lock taken?"),
								  .Answer   = QStringLiteral("Here:\n\n```cpp\nauto lock = std::scoped_lock{ Mutex };\nreturn *Table;\n```\n\nNowhere else."),
								  .Footer   = QStringLiteral("success") } });
		agent->refresh();
		settle();
	}

	using builder = std::function<void(QVBoxLayout&, QWidget&)>;

	/// \brief One small surface in its host: built, themed, shown, swept.
	auto sweep_small(course& aCourse, const std::string& aSurface, const std::string& aKey, qt::appearance_profile& aProfile, const builder& aBuild) -> void
	{
		auto host = host_window();
		aBuild(*static_cast<QVBoxLayout*>(host->layout()), *host);
		host->resize(900, 640);
		aProfile.apply_to(*host);
		host->show();
		settle();
		auto report = faces::sweep(*host, faces::quiet_buttons(*host), aSurface + " " + aKey, faces::floors_of(aProfile));
		aCourse.Surfaces[aSurface].merge(report);
		faces::render(*host, QString::fromStdString(aSurface + "-" + aKey));
	}

	void sweep_look(course& aCourse, const faces::look& aLook)
	{
		if (!faces::use(aLook)) {
			aCourse.Missing.push_back(aLook.Style.toStdString() + ": the style is not in use");
			return;
		}
		QApplication::setPalette(QApplication::style()->standardPalette());
		for (const auto& theme : faces::themes()) {
			auto       profile = faces::profile_for(theme.Mode);
			const auto key     = aLook.Key.toStdString() + "-" + theme.Name;
			const auto floors  = faces::floors_of(profile);

			// The agent tab: its tools, a turn's Copy and its answer's code block.
			{
				qt::workbench_window window;
				open_agent_tab(window, profile);
				auto* const agent = window.agent_tab();
				auto        tab   = faces::sweep(window, faces::quiet_buttons(*agent), "agent-tab " + key, floors);
				require(aCourse, tab, "agent-tab " + key, { "agentHistory", "agentClearSession", "agentAddContext", "agentSuggestionsToggle", "agentTurnCopy" });
				aCourse.Surfaces["agent-tab"].merge(tab);
				faces::render(*agent, QStringLiteral("agent-tab-%1").arg(QString::fromStdString(key)));
				// The answer's code block, its Wrap off and then on.
				auto blocks = agent->findChildren<QWidget*>(QStringLiteral("markdownCodeBlock"));
				if (blocks.isEmpty()) {
					aCourse.Missing.push_back("code-block " + key + ": the answer shows no code block");
				} else {
					auto* const block = blocks.front();
					auto        code  = faces::sweep(window, faces::quiet_buttons(*block), "code-block " + key, floors);
					require(aCourse, code, "code-block " + key, { "markdownCodeBlockWrap", "markdownCodeBlockCopy" });
					aCourse.Surfaces["code-block"].merge(code);
					faces::render(*block, QStringLiteral("code-block-%1").arg(QString::fromStdString(key)));
					if (auto* const wrap = block->findChild<QToolButton*>(QStringLiteral("markdownCodeBlockWrap")); wrap != nullptr) {
						wrap->click();
						settle();
						faces::render(*block, QStringLiteral("code-block-wrap-on-%1").arg(QString::fromStdString(key)));
						wrap->click();
						settle();
					}
				}
				// The rest of the window as it opens on a review, outside its
				// toolbars and the agent tab already swept.
				auto rest = std::vector<QAbstractButton*>{ };
				for (auto* const button : faces::quiet_buttons(window)) {
					if (!agent->isAncestorOf(button)) {
						rest.push_back(button);
					}
				}
				aCourse.Surfaces["workbench"].merge(faces::sweep(window, rest, "workbench " + key, floors));
				window.hide();
			}

			// The label menu, opened from a comment with a label and a decoration.
			{
				auto  host   = host_window();
				auto* body   = new QPlainTextEdit{ host.get() };
				auto* labels = new qt::feedback_label_controls{ body, host.get() };
				labels->set_reference_dispatch([](core::reference_page) { });
				host->layout()->addWidget(labels);
				host->layout()->addWidget(body);
				host->resize(600, 300);
				profile.apply_to(*host);
				host->show();
				body->setPlainText(QStringLiteral("issue (blocking): the lock is taken twice"));
				settle();
				auto* const menu = labels->menu();
				menu->setAttribute(Qt::WA_DontShowOnScreen, true);
				labels->open_menu();
				settle();
				// The profile does not own the link colour (QPalette::Link stays
				// as inherited); `readable_link_colour` holds it to 4.5:1. The
				// macOS system blue reads 5.6 to 6.9:1 in the high-contrast
				// themes: a link-colour shortfall, not a face.
				const auto link = std::map<std::string, faces::known_shortfall>{
					{ "feedbackLabelMenuAbout", { .Floor = 4.5, .Reason = "the inherited link colour is held to 4.5:1, not the high-contrast 7:1" } },
				};
				auto report = faces::sweep(*menu, faces::quiet_buttons(*menu), "label-menu " + key, floors, link);
				require(aCourse, report, "label-menu " + key,
						{ "feedbackLabelMenuPlain", "feedbackLabelMenu_issue", "feedbackLabelMenuBlocking", "feedbackLabelMenuNonBlocking", "feedbackLabelMenuCustomAdd", "feedbackLabelMenuAbout" });
				aCourse.Surfaces["label-menu"].merge(report);
				faces::render(*menu, QStringLiteral("label-menu-%1").arg(QString::fromStdString(key)));
				menu->close();
			}

			// The Overview of a pull request the viewer wrote.
			{
				auto host = host_window();
				auto page = new qt::pull_request_overview_page{ host.get() };
				host->layout()->addWidget(page);
				host->resize(1'100, 800);
				profile.apply_to(*host);
				host->show();
				page->present(overview_projection());
				settle();
				auto report = faces::sweep(*host, faces::quiet_buttons(*host), "overview " + key, floors);
				require(aCourse, report, "overview " + key, { "overviewFullView" });
				aCourse.Surfaces["overview"].merge(report);
				faces::render(*host, QStringLiteral("overview-%1").arg(QString::fromStdString(key)));
			}

			sweep_small(aCourse, "conversation-finder", key, profile, [](QVBoxLayout& aColumn, QWidget& aHost) {
				auto* const finder = new qt::conversation_finder{ &aHost };
				aColumn.addWidget(finder);
				if (auto* const field = finder->findChild<QLineEdit*>(); field != nullptr) {
					field->setText(QStringLiteral("lock"));
				}
			});
			sweep_small(aCourse, "document-tab-strip", key, profile, [](QVBoxLayout& aColumn, QWidget& aHost) { aColumn.addWidget(new qt::document_tab_strip{ &aHost }); });
			sweep_small(aCourse, "folder-list-editor", key, profile, [](QVBoxLayout& aColumn, QWidget& aHost) { aColumn.addWidget(new qt::folder_list_editor{ &aHost }); });
			sweep_small(aCourse, "info-glyph", key, profile, [](QVBoxLayout& aColumn, QWidget& aHost) { aColumn.addWidget(new qt::info_glyph{ QStringLiteral("What it does."), &aHost }); });
			sweep_small(aCourse, "source-search", key, profile, [](QVBoxLayout& aColumn, QWidget& aHost) { aColumn.addWidget(new qt::source_search_panel{ &aHost }); });
			sweep_small(aCourse, "tour-side-chat", key, profile, [](QVBoxLayout& aColumn, QWidget& aHost) { aColumn.addWidget(new qt::tour_side_chat{ &aHost }); });
			sweep_small(aCourse, "tour-agent-panel", key, profile, [](QVBoxLayout& aColumn, QWidget& aHost) { aColumn.addWidget(new qt::tour_agent_panel{ &aHost }); });
			sweep_small(aCourse, "review-cycle", key, profile, [](QVBoxLayout& aColumn, QWidget& aHost) { aColumn.addWidget(new qt::review_cycle_view{ &aHost }); });
			sweep_small(aCourse, "review-submission", key, profile, [](QVBoxLayout& aColumn, QWidget& aHost) { aColumn.addWidget(new qt::review_submission_editor{ [](auto) { }, &aHost }); });
			sweep_small(aCourse, "cycle-share-sheet", key, profile, [](QVBoxLayout& aColumn, QWidget& aHost) { aColumn.addWidget(new qt::review_cycle_share_sheet{ &aHost }); });
			sweep_small(aCourse, "about", key, profile, [](QVBoxLayout& aColumn, QWidget& aHost) {
				auto facts    = qt::about_facts{ };
				facts.Version = QStringLiteral("0.4.2");
				aColumn.addWidget(new qt::about_window{ facts, qt::about_actions{ }, &aHost });
			});
			sweep_small(aCourse, "private-notes", key, profile, [](QVBoxLayout& aColumn, QWidget& aHost) { aColumn.addWidget(new qt::private_notes_panel{ &aHost }); });
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	auto         result = course{ };
	for (const auto& look : faces::looks()) {
		sweep_look(result, look);
	}
	auto failed = !result.Missing.empty();
	for (const auto& missing : result.Missing) {
		std::cerr << "MISSING: " << missing << '\n';
	}
	for (const auto& [surface, report] : result.Surfaces) {
		std::cout << "worst " << surface << ": " << report.Worst << " (" << report.WorstName << "), " << report.Failures.size() << " under the floor, " << report.Known.size()
				  << " known\n";
		for (const auto& known : report.Known) {
			std::cout << "KNOWN: " << known << '\n';
		}
		for (const auto& failure : report.Failures) {
			std::cerr << "FAILED: " << failure << '\n';
		}
		failed = failed || !report.Failures.empty();
	}
	if (failed) {
		return EXIT_FAILURE;
	}
	std::cout << "button-faces-tests:pass\n";
	return EXIT_SUCCESS;
}
