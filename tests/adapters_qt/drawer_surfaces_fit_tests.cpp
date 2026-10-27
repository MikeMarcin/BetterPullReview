/// \file
/// \brief [[BPR-1595]]. RFC-BPR-0085 AC08: every rebuilt surface renders
/// without clipping in the four themes, at its narrowest and widest width.
///
/// \details The milestone courses each proved their own surface's behaviour,
/// mostly in Light and Dark at one size. This course is the closeout's sweep:
/// the Overview's Status drawer at 260 and 560 pixels; the Comments drawer
/// with the pull request's comments open from their edge chip, Notes, the
/// Tour route and the Agent tab at 280 and 760 pixels; and the Compose review
/// sheet, its expanded page and the tour page in the narrowest window the
/// workbench allows and a wide one. Each runs in Light, Dark and both
/// high-contrast themes, and CTest runs the binary at 100, 125 and 200
/// percent.
///
/// "Without clipping" is checked three ways, each with a negative control:
/// every visible control lies inside its surface (or inside scrolled content
/// whose whole width the scroll bar reaches); no visible button or unwrapped
/// label is narrower than its own minimum size hint, which is where Qt cuts
/// words off; and every tab of a visible tab bar fits the bar. The surfaces
/// that keep RFC-BPR-0085's principles are audited for them as well.
///
/// `BPR_DRAWER_FIT_RENDER_ROOT`, when set, receives a render of each state.

#include "../support/review_tour_fixture.hpp"

#include "bpr/application/review_tour_projection.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/review_preflight.hpp"
#include "bpr/core/review_submission.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/compose_review_sheet.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/overview_tour_summary.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabBar>
#include <QToolButton>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (int pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	struct theme
	{
		core::appearance_mode Mode;
		const char*           Name;
	};

	constexpr theme themes[] = {
		{ core::appearance_mode::light, "light" },
		{ core::appearance_mode::dark, "dark" },
		{ core::appearance_mode::high_contrast_light, "high-contrast-light" },
		{ core::appearance_mode::high_contrast_dark, "high-contrast-dark" },
	};

	[[nodiscard]] auto profile_for(const core::appearance_mode aMode) -> qt::appearance_profile
	{
		auto settings = qt::default_appearance_settings();
		settings.Mode = aMode;
		return qt::appearance_profile{ settings };
	}

	[[nodiscard]] auto describe(const QWidget& aWidget) -> QString
	{
		auto words = QString::fromLatin1(aWidget.metaObject()->className());
		if (!aWidget.objectName().isEmpty()) {
			words += QStringLiteral(" '%1'").arg(aWidget.objectName());
		}
		if (const auto* const button = qobject_cast<const QAbstractButton*>(&aWidget); button != nullptr && !button->text().isEmpty()) {
			words += QStringLiteral(" \"%1\"").arg(button->text());
		}
		if (const auto* const label = qobject_cast<const QLabel*>(&aWidget); label != nullptr && !label->text().isEmpty()) {
			words += QStringLiteral(" \"%1\"").arg(label->text().left(40));
		}
		return words;
	}

	/// \brief The widget a child's horizontal extent is bounded by: the
	/// content of the nearest scroll area between it and the surface, or the
	/// surface itself.
	[[nodiscard]] auto bound_of(const QWidget& aChild, const QWidget& aSurface) -> const QWidget*
	{
		for (const auto* parent = aChild.parentWidget(); parent != nullptr && parent != &aSurface; parent = parent->parentWidget()) {
			const auto* const owner = parent->parentWidget();
			if (const auto* const scroll = qobject_cast<const QScrollArea*>(owner != nullptr ? owner->parentWidget() : nullptr);
				scroll != nullptr && scroll->widget() == parent) {
				return parent;
			}
			if (const auto* const area = qobject_cast<const QAbstractScrollArea*>(owner); area != nullptr && area->viewport() == parent) {
				return parent;
			}
		}
		return &aSurface;
	}

	/// \brief Every way a visible control on \p aSurface is clipped.
	[[nodiscard]] auto clipped(const QWidget& aSurface) -> QStringList
	{
		auto found = QStringList{ };
		// Scrolled content must be wholly reachable sideways.
		for (const auto* const scroll : aSurface.findChildren<QScrollArea*>()) {
			if (scroll->isVisible() && scroll->widget() != nullptr
				&& scroll->horizontalScrollBar()->maximum() + scroll->viewport()->width() < scroll->widget()->width()) {
				found.push_back(QStringLiteral("%1 scrolls short of its content").arg(describe(*scroll)));
			}
		}
		for (const auto* const child : aSurface.findChildren<QWidget*>()) {
			if (!child->isVisible() || child->width() <= 0 || child->isWindow()) {
				continue;
			}
			// Scrolled content is judged by its reach, above.
			if (const auto* const scroll = qobject_cast<const QScrollArea*>(child->parentWidget() != nullptr ? child->parentWidget()->parentWidget() : nullptr);
				scroll != nullptr && scroll->widget() == child) {
				continue;
			}
			const auto* const bound = bound_of(*child, aSurface);
			const auto        left  = child->mapTo(bound, QPoint{ 0, 0 }).x();
			const auto        right = left + child->width();
			// A scroll bar and a viewport are the scroll area's own parts.
			if (qobject_cast<const QScrollBar*>(child) == nullptr && (left < 0 || right > bound->width())) {
				found.push_back(QStringLiteral("%1 spans %2..%3 of %4's %5").arg(describe(*child)).arg(left).arg(right).arg(describe(*bound)).arg(bound->width()));
			}
			if (const auto* const button = qobject_cast<const QAbstractButton*>(child);
				button != nullptr && !button->text().isEmpty() && button->width() < button->minimumSizeHint().width()) {
				found.push_back(QStringLiteral("%1 is %2 wide, needs %3").arg(describe(*button)).arg(button->width()).arg(button->minimumSizeHint().width()));
			}
			if (const auto* const label = qobject_cast<const QLabel*>(child);
				label != nullptr && !label->wordWrap() && !label->text().isEmpty() && label->pixmap().isNull()
				&& label->width() < label->minimumSizeHint().width()) {
				found.push_back(QStringLiteral("%1 is %2 wide, needs %3").arg(describe(*label)).arg(label->width()).arg(label->minimumSizeHint().width()));
			}
			if (const auto* const bar = qobject_cast<const QTabBar*>(child); bar != nullptr) {
				for (int index = 0; index < bar->count(); ++index) {
					if (bar->isTabVisible(index) && bar->tabRect(index).right() >= bar->width()) {
						found.push_back(QStringLiteral("%1's tab %2 \"%3\" ends at %4 in a %5 px bar (hint %6, header %7)").arg(describe(*bar)).arg(index).arg(bar->tabText(index)).arg(bar->tabRect(index).right() + 1).arg(bar->width()).arg(bar->sizeHint().width()).arg(bar->parentWidget() != nullptr ? bar->parentWidget()->width() : -1));
					}
				}
			}
		}
		return found;
	}

	/// Every surface state that failed, so one run names them all.
	std::vector<std::string> failures;

	void require_whole(const QWidget& aSurface, const std::string& aWhere, const qt::drawer_principles* aPrinciples = nullptr)
	{
		if (const auto problems = clipped(aSurface); !problems.isEmpty()) {
			failures.push_back(aWhere + ": clipped: " + problems.join(QStringLiteral("; ")).toStdString());
		}
		if (aPrinciples != nullptr) {
			if (const auto violations = qt::drawer_principle_violations(aSurface, *aPrinciples); !violations.isEmpty()) {
				failures.push_back(aWhere + ": RFC-BPR-0085's principles broken: " + violations.join(QStringLiteral("; ")).toStdString());
			}
		}
	}

	void save(const QWidget& aWidget, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_DRAWER_FIT_RENDER_ROOT");
		if (!root.isEmpty()) {
			expect(const_cast<QWidget&>(aWidget).grab().save(QStringLiteral("%1/%2.png").arg(root, aName), "PNG"), "the render is saved: " + aName.toStdString());
		}
	}

	/// \brief The checks themselves fail on what they claim to catch.
	void test_the_checks_catch_clipping()
	{
		QWidget surface;
		surface.resize(200, 120);
		auto* const past = new QPushButton{ QStringLiteral("Past the edge"), &surface };
		past->setGeometry(150, 10, 100, 30);
		surface.show();
		settle();
		expect(!clipped(surface).isEmpty(), "negative control: a control past the surface's edge is caught");
		past->setGeometry(10, 10, 20, 30);
		settle();
		expect(!clipped(surface).isEmpty(), "negative control: a button narrower than its words is caught");
		delete past;
		auto* const label = new QLabel{ QStringLiteral("A label much wider than the room it was given"), &surface };
		label->setGeometry(10, 10, 60, 20);
		label->show();
		settle();
		expect(!clipped(surface).isEmpty(), "negative control: an unwrapped label narrower than its words is caught");
		delete label;
		auto* const bar = new QTabBar{ &surface };
		bar->setUsesScrollButtons(true);
		for (const auto* name : { "Comments", "Notes", "Tour", "Agent" }) {
			bar->addTab(QString::fromLatin1(name));
		}
		bar->setGeometry(10, 10, 80, 30);
		bar->show();
		settle();
		expect(!clipped(surface).isEmpty(), "negative control: a tab bar too narrow for its tabs is caught");
		surface.resize(480, 120);
		bar->setGeometry(0, 0, 480, 30);
		settle();
		expect(clipped(surface).isEmpty(), "and the same parts with room are whole");
	}

	// ---- The Overview's Status drawer ---------------------------------------

	[[nodiscard]] auto status_projection() -> application::pull_request_overview_projection
	{
		auto projected                = application::pull_request_overview_projection{ };
		projected.Owner               = "EpicGames";
		projected.RepositoryName      = "UnrealEngine";
		projected.Number              = core::pull_request_number{ 14366U };
		projected.Title               = "Hair strand lighting fixes";
		projected.Body                = "Fixes three crashes.";
		projected.AuthorLogin         = "jiayin";
		projected.BaseRef             = "release";
		projected.HeadRef             = "hair-strand";
		projected.CommitCount         = 4U;
		projected.ReturnedCommitCount = 4U;
		projected.Participants        = {
			{ .Login = "jiayin", .Role = application::overview_participant_role::author, .Signoff = application::overview_signoff::uploaded },
			{ .Login = "mike", .Role = application::overview_participant_role::reviewer, .Signoff = application::overview_signoff::changes_requested, .Viewer = true },
			{ .Login = "rcaloca", .Role = application::overview_participant_role::reviewer, .Signoff = application::overview_signoff::approved },
		};
		projected.SinceLastReview = application::overview_since_last_review{
			.Known               = true,
			.ReviewedAt          = core::observed_utc_milliseconds{ 1'770'000'000'000 },
			.Uploads             = 2U,
			.FilesChanged        = 2U,
			.RepliesToMe         = 1U,
			.MyUnresolvedThreads = 2U,
		};
		projected.Progress        = { .FilesRead = 1U, .FilesTotal = 3U, .PrivateNotes = 1U, .DraftedComments = 1U };
		projected.MergeState      = "Conflicts";
		projected.MergeTone       = application::overview_merge_tone::blocked;
		projected.MergeConditions = { "Review required", "build-linux failed", "lint failed" };
		projected.ReviewRequests  = { { .Name = "EpicGames/render", .DisplayName = "Render", .Team = true, .CodeOwner = true }, { .Name = "tbutler" } };
		projected.Checks          = { .Passed = 33U, .Failed = 2U, .Skipped = 2U };
		for (const auto& [name, failed] : { std::pair{ "build-linux-with-a-long-matrix-name", true }, std::pair{ "lint", true }, std::pair{ "test-mac", false } }) {
			projected.CheckRows.push_back({ .Identity = std::string{ "check:" } + name, .Name = name, .Kind = "Check run", .Outcome = failed ? "Failed" : "Passed", .Failed = failed });
		}
		auto& status          = projected.Status;
		status.Viewer         = "mike";
		status.Roles          = { .ViewerKnown = true,
								  .Roles       = { { .Kind = core::review_role_kind::team_requested, .CodeOwner = true, .Teams = { "EpicGames/render" }, .Reason = core::team_membership_reason::needs_read_org },
												   { .Kind = core::review_role_kind::reviewed, .Review = core::my_review_state::changes_requested },
												   { .Kind = core::review_role_kind::mentioned, .Mention = core::mention_place::comment } } };
		status.Attention      = { { .Kind = core::status_attention_kind::conflicts, .Count = 1U },
								  { .Kind = core::status_attention_kind::failed_checks, .Count = 2U, .Names = { "build-linux-with-a-long-matrix-name", "lint" } },
								  { .Kind = core::status_attention_kind::waiting_on, .Count = 2U, .Names = { "EpicGames/render", "tbutler" } },
								  { .Kind = core::status_attention_kind::replies_to_me, .Count = 1U } };
		status.OpenConditions = { "Review required" };
		return projected;
	}

	void test_the_status_drawer_fits()
	{
		for (const auto& [mode, name] : themes) {
			auto profile = profile_for(mode);
			for (const auto width : { application::minimum_status_drawer_width, application::maximum_status_drawer_width }) {
				for (const auto open : { false, true }) {
					qt::pull_request_overview_page page;
					profile.apply_to(page);
					page.adopt_appearance(profile);
					page.resize(1'240, open ? 1'500 : 900);
					page.present(status_projection());
					page.set_new_threads(3U);
					page.set_new_conversation_comments(2U);
					page.show();
					page.status_drawer()->adopt({ .Width = width, .State = core::workbench_drawer_state::expanded });
					for (std::size_t index = 0U; index < qt::overview_status_section_count; ++index) {
						page.status_section_header(static_cast<qt::overview_status_section>(index))->setChecked(open);
					}
					settle();
					auto* const drawer = page.status_drawer();
					const auto  where  = std::string{ "Status " } + name + " at " + std::to_string(width) + (open ? ", every section open" : ", sections closed");
					expect(drawer->isVisible() && drawer->width() >= static_cast<int>(width), where + ": the drawer is shown at its width (" + std::to_string(drawer->width()) + ")");
					const auto principles = qt::drawer_principles{ };
					require_whole(*drawer, where, &principles);
					save(*drawer, QStringLiteral("status-%1-%2-%3").arg(QString::fromLatin1(name)).arg(width).arg(open ? QStringLiteral("open") : QStringLiteral("closed")));
				}
			}
		}
	}

	// ---- The workbench's drawer tabs, sheet and pages ------------------------

	[[nodiscard]] auto pull_request_comments() -> std::vector<application::github_conversation_projection>
	{
		auto summary        = application::github_conversation_projection{ };
		summary.Id          = "review:one";
		summary.Kind        = application::github_conversation_kind::review;
		summary.AuthorLogin = "reviewer-with-a-long-login";
		summary.Body        = "Please check both files. The retry budget on the second view needs fixing before this lands; everything else is small and can follow.";
		summary.ReviewState = application::github_review_state_projection::changes_requested;
		summary.CreatedAt   = "2026-10-18T01:00:00Z";
		auto old            = application::github_conversation_projection{ };
		old.Id              = "issue-comment:old";
		old.AuthorLogin     = "author";
		old.Body            = "First comment on the pull request, long enough to wrap in the narrowest drawer the reviewer can drag it to.";
		old.CreatedAt       = "2026-10-17T00:00:00Z";
		auto fresh          = old;
		fresh.Id            = "issue-comment:new";
		fresh.Body          = "New comment";
		fresh.CreatedAt     = "2026-10-18T02:00:00Z";
		return { summary, fresh, old };
	}

	[[nodiscard]] auto sheet_review() -> qt::review_submission_editor_projection
	{
		const auto head  = core::parse_git_commit_oid("abcdef0123456789abcdef0123456789abcdef01").value();
		auto       draft = core::review_submission_draft{
			.Id            = core::review_submission_draft_id{ 1U },
			.Target        = { .Host = core::git_host_id{ 1U }, .Repository = core::repository_id{ 2U }, .PullRequest = core::pull_request_id{ 3U }, .ExpectedHead = head },
			.ActingAccount = core::github_account_id{ 4U },
			.Body          = "Nice cleanup overall. The retry budget on the second view needs fixing before this lands; everything else is small and can follow.",
		};
		const auto comment = [](const std::uint64_t aId, const char* aPath, const std::uint32_t aLine, const char* aWords) {
			auto made          = core::review_submission_comment{ };
			made.Draft         = core::review_draft_id{ aId };
			made.DraftRevision = 1U;
			made.Path          = aPath;
			made.Line          = aLine;
			made.Body          = aWords;
			return made;
		};
		draft.Comments = {
			comment(7U, "src/engine/renderer.cpp", 439U, "issue (blocking): The retry budget resets every frame, so a view that keeps failing never stops retrying. Move the counter onto the view."),
			comment(8U, "src/engine/shaders/post/a-rather-long-directory-name/bloom.frag", 2U, "thought: The bloom threshold is hard-coded."),
			comment(9U, "README.md", 5U, "praise: Thanks for documenting the new folder layout."),
		};
		return { .Draft = draft, .Controls = core::controls(draft), .TargetDescription = QStringLiteral("github.example.test · synthetic-owner/synthetic-repository#17"), .Prepared = core::prepare_review_decision(draft, { }) };
	}

	[[nodiscard]] auto notes_projection() -> qt::private_notes_projection
	{
		const auto file      = qt::private_note_target{ core::file_identity_of("src/engine/renderer.cpp"), core::file_comparison_digest{ } };
		auto       elsewhere = qt::private_note_target{ core::file_identity_of("src/engine/renderer.h"), core::file_comparison_digest{ } };
		return {
			.Scope            = 7,
			.Available        = true,
			.CurrentFile      = file,
			.CurrentFileLabel = "src/engine/renderer.cpp",
			.Notes            = {
				{ .Id = 3, .Revision = 1, .Attachment = std::nullopt, .Label = "Pull request", .Text = "Before approving: run the soak test with the retry budget at 1." },
				{ .Id = 4, .Revision = 1, .Attachment = file, .Label = "src/engine/renderer.cpp", .Text = "The new clamp at 4.0 changes behaviour on 5K displays. Worth a test?" },
				{ .Id = 5, .Revision = 1, .Attachment = elsewhere, .Label = "src/engine/renderer.h", .Text = "Should Renderer be non-copyable now?" },
			},
		};
	}

	/// \brief How the Comments drawer reaches its width: restored from the
	/// window's remembered layout, as at startup, or dragged by its seam.
	enum class reached : std::uint8_t { restored,
										dragged };

	/// \brief A window in Review with both drawers open, themed, its
	/// Comments drawer at \p aDrawerWidth.
	void open_review(qt::workbench_window& aWindow, qt::appearance_profile& aProfile, const int aWindowWidth, const std::uint32_t aDrawerWidth, const reached aHow = reached::restored)
	{
		aWindow.resize(aWindowWidth, 900);
		aProfile.apply_to(aWindow);
		aWindow.show();
		auto opened   = application::workbench_projection{ };
		auto commands = core::workbench_command_delta{ };
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			commands.Commands[index] = { .Id = static_cast<core::workbench_command_id>(index), .Enabled = true, .DisabledReason = core::workbench_reason_code::none };
		}
		opened.Commands = commands;
		opened.Drawers  = core::workbench_drawer_delta{ .Files = core::workbench_drawer_state::expanded, .Conversations = core::workbench_drawer_state::expanded };
		opened.Layout   = application::workbench_layout_delta{ .FilesWidth = 260U, .ConversationsWidth = aHow == reached::restored ? aDrawerWidth : 400U };
		aWindow.publish(opened);
		settle();
		aWindow.choose_modality(qt::review_modality::review);
		settle();
		if (aHow == reached::dragged) {
			// The conversations drawer is the splitter's last widget.
			auto* const splitter = aWindow.central_splitter();
			splitter->move_handle(3, splitter->width() - static_cast<int>(aDrawerWidth) - splitter->handleWidth());
			settle();
		}
	}

	void present_tour(qt::workbench_window& aWindow)
	{
		auto* const editor = aWindow.tour_editor();
		editor->set_dispatch([](core::tour_command) { }, [](qt::tour_editor_action) { });
		auto tour       = tests::review_tour_fixture();
		auto second     = tour.Stops.front();
		second.Id       = core::tour_stop_id{ 2 };
		second.Title    = "Zero width is a real state, and this stop's title is long enough to wrap";
		second.Body     = "During an interactive resize Qt reports a width of 0 for one frame.";
		tour.LastStopId = 2;
		tour.Stops.push_back(second);
		application::tour_presentation presentation;
		presentation.Draft    = std::make_shared<const core::review_tour>(tour);
		presentation.Selected = core::tour_stop_id{ 2 };
		editor->present(presentation);
	}

	void test_the_drawer_tabs_fit()
	{
		for (const auto& [mode, name] : themes) {
			auto profile = profile_for(mode);
			for (const auto& [width, how] : { std::pair{ 280U, reached::restored }, std::pair{ 280U, reached::dragged }, std::pair{ 760U, reached::restored }, std::pair{ 760U, reached::dragged } }) {
				qt::workbench_window window;
				open_review(window, profile, width == 280U ? 1'280 : 1'920, width, how);
				auto* const drawer = window.conversations_drawer();
				const auto  at     = std::string{ " " } + name + " at " + std::to_string(width) + (how == reached::restored ? " (restored)" : " (dragged)");
				expect(drawer->isVisible() && drawer->width() == static_cast<int>(width), "the Comments drawer is " + std::to_string(width) + " pixels" + at + " (" + std::to_string(drawer->width()) + ")");

				// Comments, with the quiet link to the pull request's
				// conversation on Overview ([[BPR-1781]]).
				window.conversation_panel()->adopt(pull_request_comments(), QString{ });
				window.tray_tabs()->setCurrentIndex(0);
				settle();
				auto* const cards = window.conversation_panel()->cards();
				auto* const chip  = cards->pull_request_comments_chip();
				expect(chip != nullptr && chip->isVisible() && chip->text() == QStringLiteral("Pull request conversation"), "the link to the pull request's conversation waits at the edge" + at);
				expect(chip->geometry().right() < cards->viewport()->width(), "the link fits the drawer" + at);
				require_whole(*drawer, "Comments with the edge link" + at);
				save(window, QStringLiteral("comments-chip-%1-%2").arg(QString::fromLatin1(name)).arg(width));
				expect(!cards->pull_request_comments_shown(), "Review's Comments has no pull request mode" + at);

				// Notes.
				window.private_notes()->present(notes_projection());
				window.tray_tabs()->setCurrentIndex(1);
				settle();
				expect(window.private_notes()->isVisible(), "Notes shows" + at);
				const auto principles = qt::drawer_principles{ };
				require_whole(*drawer, "Notes" + at);
				require_whole(*window.private_notes(), "the Notes panel" + at, &principles);
				save(window, QStringLiteral("notes-%1-%2").arg(QString::fromLatin1(name)).arg(width));

				// The Tour route.
				present_tour(window);
				window.open_review_tour();
				window.tour_editor()->show_stops();
				settle();
				expect(window.tray_tabs()->currentIndex() == 2 && window.tour_editor()->mode() == qt::tour_editor_mode::route, "the Tour tab shows the route" + at);
				require_whole(*drawer, "the Tour route" + at);
				require_whole(*window.tour_editor(), "the Tour route's editor" + at, &principles);
				save(window, QStringLiteral("tour-route-%1-%2").arg(QString::fromLatin1(name)).arg(width));

				// The Agent tab.
				window.tray_tabs()->setCurrentIndex(3);
				settle();
				expect(window.agent_tab() != nullptr && window.agent_tab()->isVisible(), "the Agent tab shows" + at);
				require_whole(*drawer, "the Agent tab" + at);
				save(window, QStringLiteral("agent-%1-%2").arg(QString::fromLatin1(name)).arg(width));
				window.hide();
			}
		}
	}

	/// \brief [[BPR-1781]]. Overview's drawer with each conversation tab:
	/// the pull request's conversation in Comments, the pull request's
	/// notes, the tour as an author with a draft, as an author with none and
	/// as a reader of a published tour, and the Agent tab; whole in every
	/// theme at the default and widest widths.
	void test_the_overview_conversation_tabs_fit()
	{
		for (const auto& [mode, name] : themes) {
			auto profile = profile_for(mode);
			for (const auto width : { application::default_status_drawer_width, application::maximum_status_drawer_width }) {
				for (const auto* const tour : { "draft", "none", "reader" }) {
					qt::workbench_window window;
					open_review(window, profile, 1'480, 400U);
					window.conversation_panel()->adopt(pull_request_comments(), QString{ });
					window.private_notes()->present(notes_projection());
					auto       projected = status_projection();
					const auto author    = std::string_view{ tour } != "reader";
					if (author) {
						projected.Status.Roles.Roles.insert(projected.Status.Roles.Roles.begin(), core::review_role{ .Kind = core::review_role_kind::author });
					}
					window.choose_modality(qt::review_modality::overview);
					// The core collapses every drawer as Overview is chosen
					// ([[BPR-0674]]); this window has no core, so it is told.
					auto collapsed    = application::workbench_projection{ };
					collapsed.Drawers = core::workbench_drawer_delta{ .Files = core::workbench_drawer_state::collapsed, .Conversations = core::workbench_drawer_state::collapsed, .Repositories = core::workbench_drawer_state::collapsed };
					window.publish(collapsed);
					window.publish(projected);
					auto* const editor = window.tour_editor();
					editor->set_dispatch([](core::tour_command) { }, [](qt::tour_editor_action) { });
					auto fixture       = tests::review_tour_fixture();
					auto second        = fixture.Stops.front();
					second.Id          = core::tour_stop_id{ 2 };
					second.Title       = "Zero width is a real state, and this stop's title is long enough to wrap";
					second.Preparation = core::tour_preparation::needs_attention;
					fixture.LastStopId = 2;
					fixture.Stops.push_back(second);
					application::tour_presentation presentation;
					if (std::string_view{ tour } == "draft") {
						presentation.Draft = std::make_shared<const core::review_tour>(fixture);
					} else if (std::string_view{ tour } == "reader") {
						auto published       = fixture;
						published.Remote     = core::tour_remote_binding{ .Entity = "IC_root", .Actor = "jiayin", .Own = false };
						presentation.Draft   = std::make_shared<const core::review_tour>(core::review_tour{ });
						presentation.Offered = std::make_shared<const core::review_tour>(published);
					} else {
						presentation.Draft = std::make_shared<const core::review_tour>(core::review_tour{ });
					}
					editor->present(presentation);
					settle();
					auto* const drawer = window.overview_page()->status_drawer();
					drawer->adopt({ .Width = width, .State = core::workbench_drawer_state::expanded });
					settle();
					const auto at = std::string{ " " } + name + " at " + std::to_string(width) + " (tour: " + tour + ")";
					expect(window.showing_overview() && drawer->isVisible(), "Overview shows its drawer" + at);
					expect(drawer->tour_tab_shown(), "the Tour tab shows for an author and for a published tour" + at);
					const auto principles = qt::drawer_principles{ .NestedTabs = false };
					const auto tabs       = std::string_view{ tour } == "draft"
						? std::vector{ qt::overview_drawer_tab::comments, qt::overview_drawer_tab::notes, qt::overview_drawer_tab::tour, qt::overview_drawer_tab::agent }
						: std::vector{ qt::overview_drawer_tab::tour };
					for (const auto tab : tabs) {
						drawer->show_tab(tab);
						settle();
						const auto which = tab == qt::overview_drawer_tab::comments ? QStringLiteral("comments")
							: tab == qt::overview_drawer_tab::notes                 ? QStringLiteral("notes")
							: tab == qt::overview_drawer_tab::tour                  ? QStringLiteral("tour-%1").arg(QString::fromLatin1(tour))
																					: QStringLiteral("agent");
						const auto where = "Overview's " + which.toStdString() + at;
						require_whole(*drawer->header(), where + " header", &principles);
						if (tab != qt::overview_drawer_tab::agent) {
							require_whole(*drawer, where);
						}
						if (tab == qt::overview_drawer_tab::tour) {
							require_whole(*drawer->tour_summary(), where + " summary", &principles);
						}
						save(window, QStringLiteral("overview-%1-%2-%3").arg(which, QString::fromLatin1(name)).arg(width));
					}
				}
			}
		}
	}

	void test_the_sheet_and_pages_fit()
	{
		for (const auto& [mode, name] : themes) {
			auto profile = profile_for(mode);
			for (const auto wide : { false, true }) {
				qt::workbench_window window;
				open_review(window, profile, 1'920, 375U);
				if (!wide) {
					window.resize(window.minimumSizeHint().width(), 900);
					window.resize(std::max(window.minimumWidth(), window.minimumSizeHint().width()), 900);
					settle();
				}
				const auto at = std::string{ " " } + name + (wide ? " in a wide window (" : " in the narrowest window (") + std::to_string(window.width()) + ")";

				// Compose review: the sheet, then its expanded page.
				window.review_submission_editor_view()->present(sheet_review());
				window.compose_review_action()->trigger();
				settle();
				auto* const sheet = window.review_sheet();
				expect(window.review_sheet_open() && sheet->isVisible(), "Finish review opens the sheet" + at);
				expect(sheet->geometry().left() >= 0 && sheet->geometry().right() < sheet->parentWidget()->width(), "the sheet lies inside the window" + at);
				const auto principles = qt::drawer_principles{ };
				require_whole(*sheet, "the Compose review sheet" + at, &principles);
				save(window, QStringLiteral("sheet-%1-%2").arg(QString::fromLatin1(name), wide ? QStringLiteral("wide") : QStringLiteral("narrowest")));
				sheet->expand_button()->click();
				settle();
				auto* const page = window.findChild<QScrollArea*>(QStringLiteral("composeReviewPage"));
				expect(page != nullptr && page->isVisible(), "Expand opens the focused page" + at);
				require_whole(*page, "the Compose review page" + at);
				save(window, QStringLiteral("compose-page-%1-%2").arg(QString::fromLatin1(name), wide ? QStringLiteral("wide") : QStringLiteral("narrowest")));
				window.findChild<QPushButton*>(QStringLiteral("composeReviewPageBack"))->click();
				settle();
				window.close_review_sheet();
				settle();

				// The tour page.
				window.choose_modality(qt::review_modality::review);
				present_tour(window);
				window.open_review_tour();
				window.tour_editor()->show_stops();
				settle();
				window.findChild<QToolButton*>(QStringLiteral("workbenchExpandTourTool"))->click();
				settle();
				auto* const tourPage = window.findChild<QWidget*>(QStringLiteral("workbenchTourPage"));
				expect(window.tour_page_open() && tourPage != nullptr && tourPage->isVisible(), "Expand opens the tour page" + at);
				require_whole(*tourPage, "the tour page" + at);
				save(window, QStringLiteral("tour-page-%1-%2").arg(QString::fromLatin1(name), wide ? QStringLiteral("wide") : QStringLiteral("narrowest")));
				window.findChild<QPushButton*>(QStringLiteral("workbenchTourPageBack"))->click();
				settle();
				window.hide();
			}
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   scale = qEnvironmentVariable("QT_SCALE_FACTOR").toDouble();
	if (scale > 0) {
		expect(qFuzzyCompare(application.devicePixelRatio(), scale), "the requested display scale is effective");
	}
	test_the_checks_catch_clipping();
	test_the_status_drawer_fits();
	test_the_drawer_tabs_fit();
	test_the_overview_conversation_tabs_fit();
	test_the_sheet_and_pages_fit();
	for (const auto& failure : failures) {
		std::cerr << "CLIPPED: " << failure << '\n';
	}
	expect(failures.empty(), std::to_string(failures.size()) + " surface states are clipped or break the principles");
	std::cout << "drawer-surfaces-fit:pass dpr=" << application.devicePixelRatio() << '\n';
	return EXIT_SUCCESS;
}
