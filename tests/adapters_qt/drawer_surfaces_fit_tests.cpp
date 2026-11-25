/// \file
/// \brief [[BPR-1595]]. RFC-BPR-0085 AC08: every rebuilt surface renders
/// without clipping in the four themes, at its narrowest and widest width.
///
/// \details The milestone courses each proved their own surface's behaviour,
/// mostly in Light and Dark at one size. This course is the closeout's sweep:
/// the Overview's Status drawer at 260 and 560 pixels; the Comments drawer
/// with the pull request's comments open from their edge chip, its Local comment chips, the
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
/// No drawer ever pans sideways (owner, look workflow-v002: "It's panning
/// horizontally again. I don't like my drawers doing that. This should be a
/// rule by now."). Every drawer tab of every surface is held to it at the
/// drawer's narrowest width and a wide one: no scroll area in the drawer
/// shows a horizontal scroll bar, has sideways range (a trackpad pans a
/// scroll area whose bar is turned off), or holds content wider than its
/// viewport. The surfaces: Overview's Status, Commits, Comments, Tour and
/// Agent; Review's Comments, Tour and Agent; Author's Workflow, Tour and
/// Agent on the owner's pull request and on a local draft; and the local
/// draft's Review and Overview tabs.
///
/// The Agent tab's card for a session that read an earlier head (owner,
/// attended review 2026-11-24, of "Continue against the old revision" cut off
/// in a 300 px drawer) is held to the same at 280, 300, 340 and 760 pixels in
/// its three states: offered before a question, a refused question, and work
/// in the way of the move. Its buttons wrap; none is narrower than its words,
/// none lies over another and none leaves the card.
///
/// `BPR_DRAWER_FIT_RENDER_ROOT`, when set, receives a render of each state.
/// `BPR_PINNED_CARD_RENDER_ROOT`, when set, makes the run a look of that card
/// alone: Light and Dark at 280, 300 and 340 pixels, saved under a folder
/// named for the platform and style that drew them as
/// `<tag>-<theme>-<scale>-<state>-<width>.png` (`BPR_PINNED_CARD_RENDER_TAG`,
/// else "card"), then checked.

#include "../support/review_tour_fixture.hpp"

#include "bpr/application/local_draft_workbench.hpp"
#include "bpr/application/review_tour_projection.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/agent_entry.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/author_mode.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/local_draft.hpp"
#include "bpr/core/review_preflight.hpp"
#include "bpr/core/review_submission.hpp"
#include "bpr/core/workflow_action.hpp"
#include "bpr/core/workflow_configuration.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/compose_review_sheet.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/local_draft_workflow_area.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/overview_tour_summary.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionButton>
#include <QTabBar>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
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

	/// \brief Every way \p aSurface (a drawer, or a tab's page in one) pans
	/// sideways: a horizontal scroll bar shown, sideways range a trackpad can
	/// pan even with the bar turned off, or scrolled content wider than its
	/// viewport.
	[[nodiscard]] auto pans_sideways(const QWidget& aSurface) -> QStringList
	{
		auto found = QStringList{ };
		auto areas = aSurface.findChildren<QAbstractScrollArea*>();
		if (auto* const self = qobject_cast<QAbstractScrollArea*>(const_cast<QWidget*>(&aSurface)); self != nullptr) {
			areas.push_front(self);
		}
		for (const auto* const area : areas) {
			if (!area->isVisible() || area->viewport()->width() <= 0) {
				continue;
			}
			const auto* const bar = area->horizontalScrollBar();
			if (bar->isVisible()) {
				found.push_back(QStringLiteral("%1 shows a horizontal scroll bar").arg(describe(*area)));
			}
			if (bar->maximum() > bar->minimum()) {
				found.push_back(QStringLiteral("%1 pans %2 px sideways").arg(describe(*area)).arg(bar->maximum() - bar->minimum()));
			}
			if (const auto* const scroll = qobject_cast<const QScrollArea*>(area); scroll != nullptr && scroll->widget() != nullptr && scroll->widget()->isVisible()
				&& scroll->widget()->width() > scroll->viewport()->width()) {
				// Name what forces the width, innermost first, so the failure
				// says what to wrap or elide: a visible part whose own minimum,
				// where it stands, reaches past the viewport.
				const auto* const content = scroll->widget();
				const auto        reach   = [content](const QWidget& aPart) {
					return aPart.mapTo(content, QPoint{ 0, 0 }).x() + std::max(aPart.minimumWidth(), aPart.minimumSizeHint().width());
				};
				const auto limit = scroll->viewport()->width();
				auto       past  = QStringList{ };
				for (const auto* const child : content->findChildren<QWidget*>()) {
					if (!child->isVisible() || reach(*child) <= limit || past.size() >= 6) {
						continue;
					}
					const auto inner = std::ranges::any_of(child->findChildren<QWidget*>(), [&](const QWidget* aInner) { return aInner->isVisible() && reach(*aInner) > limit; });
					if (!inner) {
						past.push_back(QStringLiteral("%1 reaches %2").arg(describe(*child)).arg(reach(*child)));
					}
				}
				found.push_back(QStringLiteral("%1's content is %2 wide in a %3 px viewport (%4)").arg(describe(*area)).arg(scroll->widget()->width()).arg(scroll->viewport()->width()).arg(past.join(QStringLiteral(", "))));
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

	/// \brief A drawer never pans sideways.
	void require_still(const QWidget& aSurface, const std::string& aWhere)
	{
		if (const auto panning = pans_sideways(aSurface); !panning.isEmpty()) {
			failures.push_back(aWhere + ": pans sideways: " + panning.join(QStringLiteral("; ")).toStdString());
		}
	}

	/// \brief A drawer, or a tab's page in one: whole, and it never pans sideways.
	void require_drawer(const QWidget& aSurface, const std::string& aWhere, const qt::drawer_principles* aPrinciples = nullptr)
	{
		require_whole(aSurface, aWhere, aPrinciples);
		require_still(aSurface, aWhere);
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
		for (const auto* name : { "Comments", "Tour", "Agent", "Another tab" }) {
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

		// A drawer that pans: its bar turned off still pans by trackpad.
		QScrollArea scroll;
		scroll.resize(200, 120);
		scroll.setWidgetResizable(true);
		scroll.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		auto* const content = new QWidget{ };
		content->setMinimumWidth(320);
		scroll.setWidget(content);
		scroll.show();
		settle();
		expect(!pans_sideways(scroll).isEmpty(), "negative control: content wider than the viewport pans though the bar is off");
		scroll.setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		settle();
		expect(pans_sideways(scroll).size() >= 2, "negative control: a horizontal scroll bar is caught");
		content->setMinimumWidth(0);
		settle();
		expect(pans_sideways(scroll).isEmpty(), "and content that fits does not pan");
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
					require_drawer(*drawer, where, &principles);
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

	/// \brief RFC-BPR-0127. My Local comments where Notes kept its notes:
	/// one on the pull request, one on the whole file shown and one on
	/// another file.
	[[nodiscard]] auto local_comments() -> std::vector<qt::local_comment_card>
	{
		return {
			{ .Id = 3, .Revision = 1, .Body = QStringLiteral("Before approving: run the soak test with the retry budget at 1."), .Place = qt::local_comment_place::pull_request },
			{ .Id = 4, .Revision = 1, .Body = QStringLiteral("The new clamp at 4.0 changes behaviour on 5K displays. Worth a test?"), .Place = qt::local_comment_place::whole_file },
			{ .Id = 5, .Revision = 1, .Body = QStringLiteral("Should Renderer be non-copyable now?"), .Place = qt::local_comment_place::other_file, .Path = QStringLiteral("src/engine/renderer.h"), .PathLine = 12U },
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
				require_drawer(*drawer, "Comments with the edge link" + at);
				save(window, QStringLiteral("comments-chip-%1-%2").arg(QString::fromLatin1(name)).arg(width));
				expect(!cards->pull_request_comments_shown(), "Review's Comments has no pull request mode" + at);

				// RFC-BPR-0127. Local comments, where Notes was: the chips for
				// this whole file and other files beside the link.
				window.present_local_comments(local_comments());
				settle();
				expect(cards->local_file_chip()->isVisible() && cards->local_elsewhere_chip()->isVisible(), "the Local comment chips show" + at);
				require_drawer(*drawer, "Comments with the Local comment chips" + at);
				save(window, QStringLiteral("local-chips-%1-%2").arg(QString::fromLatin1(name)).arg(width));
				const auto principles = qt::drawer_principles{ };

				// The Tour route.
				present_tour(window);
				window.open_review_tour();
				window.tour_editor()->show_stops();
				settle();
				expect(window.tray_tabs()->currentIndex() == 1 && window.tour_editor()->mode() == qt::tour_editor_mode::route, "the Tour tab shows the route" + at);
				require_drawer(*drawer, "the Tour route" + at);
				require_whole(*window.tour_editor(), "the Tour route's editor" + at, &principles);
				save(window, QStringLiteral("tour-route-%1-%2").arg(QString::fromLatin1(name)).arg(width));

				// The Agent tab.
				window.tray_tabs()->setCurrentIndex(2);
				settle();
				expect(window.agent_tab() != nullptr && window.agent_tab()->isVisible(), "the Agent tab shows" + at);
				require_drawer(*drawer, "the Agent tab" + at);
				save(window, QStringLiteral("agent-%1-%2").arg(QString::fromLatin1(name)).arg(width));
				// Look prompts-v002 (owner: "the drawer is back to having a
				// horizontal panning/scrolling"): the pending list, with long
				// messages, an edit in place and a long state, never makes the
				// drawer wider than it is.
				{
					const auto words = QStringLiteral("Explain why the renderer clamps the resize before the next frame and whether the shaders under src/engine/shaders/post still compile");
					auto       rows  = std::vector<qt::agent_tab_view::queue_row>{
						{ .Id = 1U, .Label = words, .Sending = true },
						{ .Id = 2U, .Label = words, .Text = words },
						{ .Id = 3U, .Label = QStringLiteral("Explain this file") },
						// RFC-BPR-0126 AC04: sent before a crash, not known to
						// have arrived; it offers Discard.
						{ .Id = 4U, .Label = words, .Sending = true, .Uncertain = true },
					};
					auto discarded = std::optional<std::uint64_t>{ };
					window.agent_tab()->set_queue_dispatch(qt::agent_tab_view::queue_dispatch{ .Discard = [&discarded](const std::uint64_t aId) { discarded = aId; } });
					window.agent_tab()->present_queue_rows(rows, QStringLiteral("Paused: the queue waits for the agent's current answer and for you to resume it"), true);
					settle();
					// The window has no agent set up here, so the tab shows its
					// setup; the conversation page, where the list lives, is put
					// in front as a set-up agent's would be.
					QStackedWidget* pages      = nullptr;
					QWidget*        conversing = nullptr;
					for (QWidget* w = window.agent_tab()->queue_list(); w != nullptr && w != window.agent_tab(); w = w->parentWidget()) {
						if (auto* const stack = qobject_cast<QStackedWidget*>(w->parentWidget()); stack != nullptr) {
							pages      = stack;
							conversing = w;
							break;
						}
					}
					auto* const shown = pages != nullptr ? pages->currentWidget() : nullptr;
					if (pages != nullptr) {
						pages->setCurrentWidget(conversing);
					}
					settle();
					expect(window.agent_tab()->queue_list()->isVisible(), "the pending list shows" + at);
					require_drawer(*drawer, "the Agent tab with its pending list" + at);
					// Look prompts-v004: the card's controls are icons alone,
					// each with a tooltip, so the message keeps the width.
					for (const auto* const controlName : { "agentQueueEdit", "agentQueueSteer", "agentQueueRemove" }) {
						auto* const control = window.agent_tab()->queue_list()->findChild<QAbstractButton*>(QString::fromLatin1(controlName));
						expect(control != nullptr && control->text().isEmpty() && !control->toolTip().isEmpty() && control->width() <= 24, std::string{ controlName } + " is an icon with a tooltip" + at);
					}
					for (auto* const scroll : window.agent_tab()->findChildren<QScrollArea*>()) {
						if (scroll->isVisible() && scroll->widget() != nullptr) {
							expect(scroll->widget()->width() <= scroll->viewport()->width(), "the Agent tab never pans sideways" + at);
						}
					}
					save(window, QStringLiteral("agent-queue-%1-%2").arg(QString::fromLatin1(name)).arg(width));
					// Only the uncertain message offers Discard, and the press
					// only asks the host (the tab decides nothing).
					const auto discards = window.agent_tab()->queue_list()->findChildren<QAbstractButton*>(QStringLiteral("agentQueueDiscard"));
					expect(discards.size() == 1 && discards.front()->isVisibleTo(window.agent_tab()->queue_list()), "the uncertain message alone offers Discard" + at);
					discards.front()->click();
					expect(discarded == std::optional<std::uint64_t>{ 4U }, "Discard asks to discard that message" + at);
					window.agent_tab()->set_queue_dispatch({ });
					window.agent_tab()->present_queue_rows({ }, { }, false);
					if (pages != nullptr && shown != nullptr) {
						pages->setCurrentWidget(shown);
					}
					settle();
				}
				window.hide();
			}
		}
	}

	/// \brief [[BPR-1781]]. Overview's drawer with each tab: Status,
	/// Commits, the pull request's conversation in Comments, the tour as an
	/// author with a draft, as an author with none and as a reader of a
	/// published tour, and the Agent tab; whole and still in every theme at
	/// the narrowest, default and widest widths.
	void test_the_overview_conversation_tabs_fit()
	{
		for (const auto& [mode, name] : themes) {
			auto profile = profile_for(mode);
			for (const auto width : { application::minimum_status_drawer_width, application::default_status_drawer_width, application::maximum_status_drawer_width }) {
				for (const auto* const tour : { "draft", "none", "reader" }) {
					qt::workbench_window window;
					open_review(window, profile, 1'480, 400U);
					window.conversation_panel()->adopt(pull_request_comments(), QString{ });
					window.present_local_comments(local_comments());
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
						? std::vector{ qt::overview_drawer_tab::status, qt::overview_drawer_tab::commits, qt::overview_drawer_tab::comments, qt::overview_drawer_tab::tour, qt::overview_drawer_tab::agent }
						: std::vector{ qt::overview_drawer_tab::tour };
					for (const auto tab : tabs) {
						drawer->show_tab(tab);
						settle();
						const auto which = tab == qt::overview_drawer_tab::status ? QStringLiteral("status")
							: tab == qt::overview_drawer_tab::commits             ? QStringLiteral("commits")
							: tab == qt::overview_drawer_tab::comments            ? QStringLiteral("comments")
							: tab == qt::overview_drawer_tab::tour                ? QStringLiteral("tour-%1").arg(QString::fromLatin1(tour))
																				  : QStringLiteral("agent");
						const auto where = "Overview's " + which.toStdString() + at;
						require_whole(*drawer->header(), where + " header", &principles);
						if (tab != qt::overview_drawer_tab::agent) {
							require_drawer(*drawer, where);
						} else {
							require_still(*drawer, where);
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

	// ---- Author's drawer, and a local draft's tabs ---------------------------

	template <typename T>
	[[nodiscard]] auto made(T aResult, const std::string& aWhat)
	{
		expect(aResult.has_value(), aWhat);
		return std::move(*aResult);
	}

	[[nodiscard]] auto oid(const char aDigit) -> core::git_commit_oid { return core::parse_git_commit_oid(std::string(40U, aDigit)).value(); }

	/// \brief An organization's workflow with long names everywhere a name
	/// can be long: the workflow, its origin, its tasks and a program path.
	[[nodiscard]] auto long_workflow() -> core::workflow_configuration
	{
		const auto command = [](std::string aId, const core::workflow_slot aSlot, std::string aTask, const bool aRequired = false) {
			auto action        = core::workflow_action{ };
			action.Id          = std::move(aId);
			action.Slot        = aSlot;
			action.Task        = std::move(aTask);
			action.Binding     = core::workflow_binding::command;
			action.ProgramName = "synthetic-organization-pull-request-companion";
			action.Arguments   = { "--request", "{request}", "--response", "{response}" };
			action.Effects     = { core::workflow_effect::read_context, core::workflow_effect::run_check };
			action.Required    = aRequired;
			return action;
		};
		auto describe    = core::workflow_action{ };
		describe.Id      = "describe";
		describe.Slot    = core::workflow_slot::propose_description;
		describe.Task    = "Draft the description from the ticket, the design notes and the tour";
		describe.Binding = core::workflow_binding::agent;
		describe.Agent   = "writer";
		describe.Prompt  = "Draft a description.";
		describe.Effects = { core::workflow_effect::write_proposal };
		auto made        = core::workflow_configuration{ };
		made.PortableId  = "synthetic-organization-platform";
		made.Name        = "Synthetic Organization platform team pull request preparation";
		made.Origin      = "Synthetic Organization Platform Engineering Productivity";
		made.Actions     = { command("context", core::workflow_slot::gather_context, "Gather the ticket's context, linked designs and previous review threads"), describe,
							 command("template", core::workflow_slot::check_preparation, "Check the description against the organization's pull request template", true),
							 command("publish", core::workflow_slot::publish_draft, "Create the pull request with the organization's publisher"),
							 command("follow", core::workflow_slot::finish_publication, "Link the ticket and upload the screenshots to the review system") };
		return made;
	}

	/// \brief A local draft whose workflow holds a handed-off command, an
	/// agent row, a required check reported not met with a long finding, the
	/// organization's publisher chosen and a follow-up that waits.
	[[nodiscard]] auto workflow_draft() -> application::local_draft_workbench_projection
	{
		auto change     = core::draft_path_change{ };
		change.Path     = "src/engine/renderer/a-directory-name-long-enough-to-matter/frame_resize_budget.cpp";
		change.Change   = core::draft_change::modified;
		change.Left     = core::draft_side{ .Mode = 0100644, .Object = core::parse_git_entry_oid(std::string(40U, '2')).value() };
		change.Right    = core::draft_side{ .Mode = 0100644, .Object = core::parse_git_entry_oid(std::string(40U, '3')).value() };
		auto first      = core::local_draft_candidate{ };
		first.Head      = oid('a');
		first.TargetTip = oid('b');
		first.Base      = oid('c');
		first.Changes.push_back(std::move(change));
		auto worktree                  = core::local_draft_worktree{ };
		worktree.Location              = "/work/engine";
		worktree.Repository            = { .Volume = 1, .ObjectLow = 10 };
		worktree.Worktree              = { .Volume = 1, .ObjectLow = 11 };
		auto target                    = core::chosen_target{ };
		target.Target.Remote           = "origin";
		target.Target.Repository.Host  = "github.com";
		target.Target.Repository.Owner = "synthetic-owner";
		target.Target.Repository.Name  = "synthetic-repository";
		target.Target.Branch           = "main";
		auto draft                     = made(core::create_local_draft({ }, { std::string(32U, '1') }, worktree, "feature/renderer-resize-budget-with-a-long-branch-name", target, { }, first, 100),
											  "the draft is created");
		draft                          = made(core::select_workflow(draft, draft.Version, long_workflow(), 101), "the workflow is selected");
		draft                          = made(core::begin_action(draft, draft.Version, "template", { }, 102), "the check is handed off");
		auto unmet                     = core::workflow_response{ };
		unmet.Invocation               = core::find_record(*draft.Workflow, "template")->Invocation;
		unmet.Draft                    = draft.Id.Value;
		unmet.Action                   = "template";
		unmet.Outcome                  = core::workflow_outcome::findings;
		auto finding                   = core::workflow_finding{ };
		finding.Check                  = "template";
		finding.State                  = core::workflow_finding_state::unmet;
		finding.Detail                 = "The Testing section is missing; see docs/contributing/pull-request-template-for-the-platform-team.md";
		unmet.Findings.push_back(std::move(finding));
		draft                     = made(core::admit_workflow_response(draft, draft.Version, unmet, 103), "the check reports not met");
		draft                     = made(core::begin_action(draft, draft.Version, "context", { }, 104), "the context is handed off");
		draft.Workflow->Publisher = core::workflow_publisher::external;

		auto projected           = application::local_draft_workbench_projection{ };
		projected.Stage          = application::local_draft_stage::open;
		projected.Outcome        = application::local_draft_open_outcome::resumed;
		projected.Location       = draft.Worktree.Location;
		projected.Shown          = 1U;
		projected.Branch         = draft.Branch;
		projected.Source         = core::source_state::branch;
		projected.Draft          = draft;
		projected.AgentAvailable = true;
		auto& workflow           = projected.Workflow;
		auto  option             = application::local_draft_workflow_option{ };
		option.Id                = "synthetic-organization-platform";
		option.Name              = long_workflow().Name;
		option.Origin            = long_workflow().Origin;
		workflow.Available.push_back(std::move(option));
		workflow.UpdateAvailable = true;
		for (const auto& action : draft.Workflow->Workflow.Actions) {
			auto view   = application::local_draft_workflow_action_view{ };
			view.Action = action;
			if (const auto* const record = core::find_record(*draft.Workflow, action.Id); record != nullptr) {
				view.Record = *record;
			}
			if (action.Binding == core::workflow_binding::agent) {
				view.AgentName = "Writer agent with a long name";
			} else {
				view.Command      = { action.ProgramName, "--request", "/Users/someone/Library/Application Support/BetterPullReview/workflow/requests/0001/request.json" };
				view.CommandLine  = action.ProgramName + " --request '/Users/someone/Library/Application Support/BetterPullReview/workflow/requests/0001/request.json' --response "
														 "'/Users/someone/Library/Application Support/BetterPullReview/workflow/requests/0001/response.json'";
				view.RequestPath  = "/Users/someone/Library/Application Support/BetterPullReview/workflow/requests/0001/request.json";
				view.ResponsePath = "/Users/someone/Library/Application Support/BetterPullReview/workflow/requests/0001/response.json";
			}
			workflow.Actions.push_back(std::move(view));
		}
		return projected;
	}

	/// \brief Each of Author's drawer tabs, then (on a local draft) the
	/// draft's Review and Overview tabs, at the narrowest and a wide width.
	void test_the_author_drawer_fits()
	{
		for (const auto& [mode, name] : themes) {
			auto profile = profile_for(mode);
			for (const auto width : { 280U, 760U }) {
				for (const auto local : { false, true }) {
					qt::workbench_window window;
					open_review(window, profile, width == 280U ? 1'280 : 1'920, width);
					const auto at = std::string{ " " } + name + " at " + std::to_string(width) + (local ? " (local draft)" : " (my pull request)");
					if (local) {
						window.show_local_draft(QStringLiteral("Renderer resize budget"));
						window.present_local_draft(workflow_draft());
					} else {
						window.set_pull_request_author_case(core::author_case::open_pull_request);
					}
					settle();
					present_tour(window);
					window.choose_modality(qt::review_modality::author);
					settle();
					auto* const drawer = window.conversations_drawer();
					expect(window.current_modality() == qt::review_modality::author && drawer->isVisible(), "Author shows its drawer" + at);
					auto* const tabs = window.author_tabs();
					expect(tabs != nullptr && tabs->isVisible() && tabs->count() == 3, "Author's tabs show" + at);
					if (local) {
						expect(window.local_draft()->workflow()->external_publisher()->isVisible(), "the publisher choice shows on the local draft" + at);
					}
					for (int index = 0; index < tabs->count(); ++index) {
						tabs->setCurrentIndex(index);
						settle();
						const auto where = "Author's " + tabs->tabText(index).toStdString() + at;
						require_drawer(*drawer, where);
						save(window, QStringLiteral("author-%1-%2-%3-%4").arg(tabs->tabText(index).toLower(), QString::fromLatin1(name), local ? QStringLiteral("draft") : QStringLiteral("pull")).arg(width));
					}
					if (local) {
						// The local draft in Review: Comments (its notes), Tour and Agent.
						window.choose_modality(qt::review_modality::review);
						settle();
						for (int index = 0; index < window.tray_tabs()->count(); ++index) {
							if (!window.tray_tabs()->isTabVisible(index)) {
								continue;
							}
							window.tray_tabs()->setCurrentIndex(index);
							settle();
							require_drawer(*drawer, "the local draft's Review " + window.tray_tabs()->tabText(index).toStdString() + at);
						}
						// And on Overview: no Status until its pull request exists.
						window.choose_modality(qt::review_modality::overview);
						settle();
						auto* const overview = window.overview_page()->status_drawer();
						overview->adopt({ .Width = std::min(width, application::maximum_status_drawer_width), .State = core::workbench_drawer_state::expanded });
						settle();
						for (const auto tab : { qt::overview_drawer_tab::comments, qt::overview_drawer_tab::tour, qt::overview_drawer_tab::agent }) {
							overview->show_tab(tab);
							settle();
							if (overview->isVisible() && overview->current_tab() == tab) {
								require_still(*overview, "the local draft's Overview tab " + std::to_string(static_cast<int>(tab)) + at);
							}
						}
					}
					window.hide();
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
	/// \brief The Agent tab's card for a session that read an earlier head.
	enum class pinned_card : std::uint8_t {
		/// Offered as the session opens, before a question.
		offered,
		/// A question was refused: the two ways back, each resends it.
		refused,
		/// Work is in the way of the move: the reconcile choices first.
		reconcile,
	};

	/// \brief Where a push button is drawn in \p aCard. A native button's
	/// widget is larger than its face, so two widgets may overlap where their
	/// faces do not.
	[[nodiscard]] auto face_in(const QPushButton& aButton, const QWidget& aCard) -> QRect
	{
		auto face   = aButton.rect();
		auto option = QStyleOptionButton{ };
		option.initFrom(&aButton);
		if (const auto item = aButton.style()->subElementRect(QStyle::SE_PushButtonLayoutItem, &option, &aButton); item.isValid()) {
			face = item;
		}
		return face.translated(aButton.mapTo(&aCard, QPoint{ 0, 0 }));
	}

	/// \brief Every way the card's buttons fail to fit it, and the tab pans.
	[[nodiscard]] auto card_faults(const qt::agent_tab_view& aTab) -> QStringList
	{
		auto              found = QStringList{ };
		const auto* const card  = aTab.banner();
		auto              shown = std::vector<const QPushButton*>{ };
		for (const auto* const button : card->findChildren<QPushButton*>()) {
			if (button->isVisibleTo(card)) {
				shown.push_back(button);
			}
		}
		for (const auto* const button : shown) {
			const auto place = QRect{ button->mapTo(card, QPoint{ 0, 0 }), button->size() };
			if (place.left() < 0 || place.right() >= card->width()) {
				found.push_back(QStringLiteral("%1 spans %2..%3 of the card's %4").arg(describe(*button)).arg(place.left()).arg(place.right() + 1).arg(card->width()));
			}
			// A button that wraps its words needs its longest word and the
			// lines they take at its width; one with shorter words to show,
			// those; any other, its one line.
			const auto wraps = button->hasHeightForWidth();
			const auto needs = wraps ? QSize{ button->minimumSizeHint().width(), button->heightForWidth(button->width()) } : QSize{ button->minimumSizeHint().width(), button->sizeHint().height() };
			if (button->width() < needs.width() || button->height() < needs.height()) {
				found.push_back(QStringLiteral("%1 is %2x%3, its words need %4x%5").arg(describe(*button)).arg(button->width()).arg(button->height()).arg(needs.width()).arg(needs.height()));
			}
		}
		for (std::size_t first = 0U; first < shown.size(); ++first) {
			for (std::size_t second = first + 1U; second < shown.size(); ++second) {
				if (face_in(*shown[first], *card).intersects(face_in(*shown[second], *card))) {
					found.push_back(QStringLiteral("%1 lies over %2").arg(describe(*shown[first]), describe(*shown[second])));
				}
			}
		}
		for (const auto* const scroll : aTab.findChildren<QScrollArea*>()) {
			if (scroll->isVisible() && scroll->widget() != nullptr && scroll->widget()->width() > scroll->viewport()->width()) {
				found.push_back(QStringLiteral("the conversation is %1 wide in a %2 px drawer column").arg(scroll->widget()->width()).arg(scroll->viewport()->width()));
			}
		}
		return found;
	}

	/// \brief Every way the card fails to stand on the question field, in
	/// view wherever the thread is scrolled, with its two choices on one
	/// line. Owner, attended review 2026-11-25: "surely we can find a way to
	/// fit both options on one line at the narrowest drawer. Also it being at
	/// the top of the transcript is weird since we auto scroll to the
	/// bottom."
	[[nodiscard]] auto placement_faults(const qt::agent_tab_view& aTab, const QWidget& aComposer) -> QStringList
	{
		auto              found  = QStringList{ };
		const auto* const card   = aTab.banner();
		auto* const       thread = aTab.findChild<QScrollArea*>(QStringLiteral("agentConversationScroll"));
		expect(thread != nullptr, "the Agent tab's thread is a scroll area");
		const auto in    = [&aTab](const QWidget& aPart) { return QRect{ aPart.mapTo(&aTab, QPoint{ 0, 0 }), aPart.size() }; };
		const auto place = in(*card);
		if (!aTab.banner_docked()) {
			found.push_back(QStringLiteral("the card is not on the question field"));
		}
		for (const auto* parent = card->parentWidget(); parent != nullptr && parent != &aTab; parent = parent->parentWidget()) {
			if (qobject_cast<const QAbstractScrollArea*>(parent) != nullptr) {
				found.push_back(QStringLiteral("the card is inside %1, which scrolls it away").arg(describe(*parent)));
			}
		}
		if (thread->isAncestorOf(card) || in(*thread).bottom() >= place.top()) {
			found.push_back(QStringLiteral("the card (top %1) is not under the thread (bottom %2)").arg(place.top()).arg(in(*thread).bottom()));
		}
		if (const auto field = in(aComposer); place.bottom() >= field.top() || field.top() - place.bottom() > 40) {
			found.push_back(QStringLiteral("the card (bottom %1) does not stand on the question field (top %2)").arg(place.bottom()).arg(field.top()));
		}
		// In view, whole, with the thread at its start and at its end.
		auto* const bar = thread->verticalScrollBar();
		for (const auto value : { bar->minimum(), bar->maximum() }) {
			bar->setValue(value);
			settle();
			const auto now = in(*card);
			if (now != place || !aTab.rect().contains(now) || card->visibleRegion().boundingRect() != card->rect()) {
				found.push_back(QStringLiteral("the card is not whole in view with the thread scrolled to %1 of %2..%3 (%4,%5 %6x%7 in a tab of %8x%9)")
									.arg(value)
									.arg(bar->minimum())
									.arg(bar->maximum())
									.arg(now.left())
									.arg(now.top())
									.arg(now.width())
									.arg(now.height())
									.arg(aTab.width())
									.arg(aTab.height()));
			}
		}
		// The thread keeps room to be read above it.
		if (thread->viewport()->height() < 120) {
			found.push_back(QStringLiteral("the card leaves the thread %1 px").arg(thread->viewport()->height()));
		}
		// Its two choices: one line, the first then the second, each one
		// line tall.
		const auto* const first  = aTab.banner_first_button();
		const auto* const second = aTab.banner_second_button();
		if (!first->isVisibleTo(card) || !second->isVisibleTo(card)) {
			found.push_back(QStringLiteral("the card does not show its two choices"));
			return found;
		}
		const auto one = in(*first);
		const auto two = in(*second);
		if (one.top() != two.top() || one.height() != two.height() || one.right() >= two.left()) {
			found.push_back(QStringLiteral("the two choices are not on one line: \"%1\" at %2,%3 %4x%5 and \"%6\" at %7,%8 %9x%10")
								.arg(first->text())
								.arg(one.left())
								.arg(one.top())
								.arg(one.width())
								.arg(one.height())
								.arg(second->text())
								.arg(two.left())
								.arg(two.top())
								.arg(two.width())
								.arg(two.height()));
		}
		for (const auto* const choice : { first, second }) {
			if (choice->hasHeightForWidth() || choice->height() != choice->sizeHint().height()) {
				found.push_back(QStringLiteral("%1 is not one line tall (%2, one line is %3)").arg(describe(*choice)).arg(choice->height()).arg(choice->sizeHint().height()));
			}
		}
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
		// In the typefaces these platforms draw the drawer in, the whole
		// words fit the narrowest drawer; the shorter words are for a
		// typeface that is wider.
		if (first->shortened() || second->shortened()) {
			found.push_back(QStringLiteral("a choice draws its shorter words: \"%1\" is %2 wide and needs %3").arg(first->text()).arg(first->width()).arg(first->sizeHint().width()));
		}
#endif
		return found;
	}

	/// \brief The card in each state at each width. As a look (\p aLookRoot
	/// set) it draws Light and Dark under the system appearance each is used
	/// in and checks the card alone; otherwise every theme, and the whole
	/// drawer with it.
	void test_the_pinned_revision_card_fits(const QString& aLookRoot = { })
	{
		const auto look   = !aLookRoot.isEmpty();
		const auto folder = look ? QStringLiteral("%1/%2-%3").arg(aLookRoot, QGuiApplication::platformName(), QApplication::style()->name().toLower()) : QString{ };
		expect(!look || QDir{ }.mkpath(folder), "the look's directory exists");
		const auto tag      = qEnvironmentVariable("BPR_PINNED_CARD_RENDER_TAG", QStringLiteral("card"));
		const auto scale    = QString::number(qRound(qApp->devicePixelRatio() * 100.0));
		const auto earlier  = *core::parse_git_commit_oid("3333333333333333333333333333333333333333");
		const auto binding  = core::agent_revision_binding{ .Host = core::git_host_id{ 1 }, .Repository = core::repository_id{ 2 }, .PullRequest = core::pull_request_id{ 3 }, .BaseOid = *core::parse_git_commit_oid("1111111111111111111111111111111111111111"), .HeadOid = *core::parse_git_commit_oid("2222222222222222222222222222222222222222"), .LeftEpoch = core::epoch_ordinal{ 1 }, .RightEpoch = core::epoch_ordinal{ 2 } };
		auto       pinned   = binding;
		pinned.HeadOid      = earlier;
		auto profile        = core::agent_profile{ };
		profile.Id          = core::entry_profile_id(core::agent_protocol::codex_jsonl_exec, 1U);
		profile.Protocol    = core::agent_protocol::codex_jsonl_exec;
		profile.DisplayName = "Fixture agent";
		profile.Executable  = "/usr/bin/true";
		const auto widths   = look ? std::vector<std::uint32_t>{ 280U, 300U, 340U } : std::vector<std::uint32_t>{ 280U, 300U, 340U, 760U };
		for (const auto& [mode, name] : themes) {
			const auto dark = mode == core::appearance_mode::dark || mode == core::appearance_mode::high_contrast_dark;
			if (look) {
				if (mode != core::appearance_mode::light && mode != core::appearance_mode::dark) {
					continue;
				}
				QGuiApplication::styleHints()->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
			}
			auto appearance = profile_for(mode);
			for (const auto width : widths) {
				qt::workbench_window window;
				open_review(window, appearance, width == 760U ? 1'920 : 1'280, width);
				auto* const drawer = window.conversations_drawer();
				auto* const tab    = window.agent_tab();
				auto* const chat   = window.side_chat();
				window.tray_tabs()->setCurrentIndex(2);
				window.agent_panel()->offer_profiles(std::vector{ profile });
				settle();
				const auto at = std::string{ " " } + name + " at " + std::to_string(width);
				expect(drawer->isVisible() && drawer->width() == static_cast<int>(width) && tab->isVisible(), "the Agent tab shows in a drawer of that width" + at);
				chat->set_dispatch([](const qt::side_chat_intent&) { });
				tab->set_pinned_conversation_dispatch([](core::agent_continuation) { return false; });
				tab->set_move_reconcile_dispatch([](core::conversation_reconcile) { return false; });
				const auto present = [&](const core::agent_revision_binding& aRead) {
					const auto session = core::agent_session{ .Id = core::agent_session_id{ 1U }, .Profile = profile.Id, .Binding = aRead };
					chat->present(qt::side_chat_projection{
						.Session        = session,
						.Continuations  = core::offer_agent_continuations(session, binding),
						.Chat           = { .Scope = { .Kind = core::agent_chat_scope_kind::pull_request } },
						.PrivateContext = { .Session = session.Id },
					});
					settle();
				};
				auto       generation = std::uint64_t{ 0U };
				const auto stop       = [&](const bool aWorkInTheWay) {
					chat->composer()->setPlainText(QStringLiteral("Review this pull request"));
					tab->ask_button()->click();
					settle();
					auto identity                 = application::agent_invocation_projection_identity{ };
					identity.ApplicationSession   = core::application_session_id{ 1U };
					identity.Session.Id           = core::agent_session_id{ 9U };
					identity.InvocationGeneration = ++generation;
					window.agent_panel()->admit(identity);
					settle();
					auto result               = application::agent_run_result{ .Session = identity.Session.Id, .Outcome = application::agent_run_outcome::private_storage_failure };
					result.RefusalReason      = aWorkInTheWay ? "This conversation's workspace at 3333333 has uncommitted edits, so it was not moved to 2222222. No question was sent."
															  : "This conversation is pinned to an earlier revision, 3333333. Continue at 3333333 to keep asking about that revision, or start a new session at 2222222. No question was sent.";
					result.StandardError      = result.RefusalReason;
					result.PinnedConversation = core::offer_pinned_conversation({ .Pinned = pinned, .Observed = binding, .Remembers = true });
					if (aWorkInTheWay) {
						result.MoveRefusal = core::decide_conversation_move({ .Pinned = pinned, .Observed = binding, .Remembers = true, .Resumable = true, .Workspace = core::conversation_workspace_state::local_edits });
					}
					window.agent_panel()->finish(identity, result);
					settle();
				};
				const auto judge = [&](const pinned_card aState, const char* const aName) {
					settle();
					const auto where   = std::string{ "the Agent tab's pinned-revision card, " } + aName + at;
					const auto choices = tab->banner()->findChildren<QPushButton*>();
					const auto buttons = std::ranges::count_if(choices, [&](const QPushButton* aButton) { return aButton->isVisibleTo(tab->banner()); });
					expect(tab->banner()->isVisible() && tab->state() == (aState == pinned_card::offered ? qt::agent_tab_state::head_moved : qt::agent_tab_state::stopped)
							   && buttons == (aState == pinned_card::reconcile ? 5 : 2),
						   where + " shows its " + (aState == pinned_card::reconcile ? "five" : "two") + " choices (" + std::to_string(buttons) + ")");
					if (look) {
						expect(drawer->grab().save(QStringLiteral("%1/%2-%3-%4-%5-%6.png").arg(folder, tag, QString::fromLatin1(name), scale, QString::fromLatin1(aName)).arg(width), "PNG"), "the look is saved");
					} else {
						save(window, QStringLiteral("agent-pinned-card-%1-%2-%3").arg(QString::fromLatin1(aName), QString::fromLatin1(name)).arg(width));
						// The whole drawer, the refused turn under the card
						// with it: the refusal is said once, in words that
						// wrap, and never again in a code block that pans.
						require_drawer(*drawer, where);
					}
					if (const auto faults = card_faults(*tab) + placement_faults(*tab, *chat->composer()); !faults.isEmpty()) {
						failures.push_back(where + ": " + faults.join(QStringLiteral("; ")).toStdString());
					}
					// The same card in its three states: one title, the
					// choices in one order, in the words RFC-BPR-0130 gives
					// the card and History.
					if (tab->findChild<QLabel*>(QStringLiteral("agentStateTitle"))->text() != QStringLiteral("This session read 3333333") || tab->banner_first_button()->text() != QStringLiteral("Continue at 3333333")
						|| tab->banner_second_button()->text() != QStringLiteral("New session") || tab->banner_first_button()->toolTip().isEmpty() || tab->banner_second_button()->toolTip().isEmpty()) {
						failures.push_back(where + ": the card says which head the session read, then Continue at it and New session: " + tab->findChild<QLabel*>(QStringLiteral("agentStateTitle"))->text().toStdString() + " / "
										   + tab->banner_first_button()->text().toStdString() + " / " + tab->banner_second_button()->text().toStdString());
					}
				};
				// A session with a turn, opened after the head moved.
				tab->set_session_title(QStringLiteral("#17 Keep review snapshots deterministic"));
				tab->present_thread({ qt::agent_thread_turn{ .Question = QStringLiteral("What does this change do?"),
															 .Answer   = QStringLiteral("It sorts the snapshot's files before hashing them, so two reads of one head give the same digest."),
															 .Footer   = QStringLiteral("Answered") } });
				present(pinned);
				judge(pinned_card::offered, "offered");
				present(binding);
				stop(false);
				judge(pinned_card::refused, "refused");
				// The refused turn is the thread's last, just above the
				// card: it says why, and the card does not say it again.
				if (tab->banner_text() != QStringLiteral("Your message is kept and is sent when you choose.")) {
					failures.push_back("after a refused question the card says only that the message is kept" + at + ": " + tab->banner_text().toStdString());
				}
				// Owner, attended review 2026-11-25: "This made the drawer
				// have a horizontal scroll which should have been
				// disallowed by our tests." The application refused the
				// question before any agent ran: the turn says so once and
				// does not show the same words again as what the agent wrote.
				if (const auto said = tab->turn_answer(tab->turn_count() - 1U); said.contains(QStringLiteral("It wrote")) || !said.contains(QStringLiteral("No question was sent."))) {
					failures.push_back("the refused turn says the refusal once, and not as what the agent wrote" + at + ": " + said.toStdString());
				}
				stop(true);
				judge(pinned_card::reconcile, "reconcile");
				// Offered again after a refused question: neither choice
				// starts a run, so neither keeps the refused card's accent.
				present(pinned);
				if (tab->state() != qt::agent_tab_state::head_moved || qt::agent_run_control(*tab->banner_first_button()) || qt::agent_run_control(*tab->banner_second_button())) {
					failures.push_back("offered after a refused question, the card's choices wear the ordinary roles" + at);
				}
			}
		}
		if (look) {
			std::cout << "pinned-card-look: " << folder.toStdString() << ", device pixel ratio " << qApp->devicePixelRatio() << '\n';
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	// A look: the pinned-revision card's renders and its checks, nothing else,
	// so the same binary draws and measures it under the native look too. The
	// scale in each file's name is `QT_SCALE_FACTOR` times the screen's own ratio.
	if (const auto root = qEnvironmentVariable("BPR_PINNED_CARD_RENDER_ROOT"); !root.isEmpty()) {
		test_the_pinned_revision_card_fits(root);
		for (const auto& failure : failures) {
			std::cerr << "CLIPPED: " << failure << '\n';
		}
		expect(failures.empty(), std::to_string(failures.size()) + " states of the pinned-revision card do not fit");
		std::cout << "pinned-card-look:pass dpr=" << application.devicePixelRatio() << '\n';
		return EXIT_SUCCESS;
	}
	const auto scale = qEnvironmentVariable("QT_SCALE_FACTOR").toDouble();
	if (scale > 0) {
		expect(qFuzzyCompare(application.devicePixelRatio(), scale), "the requested display scale is effective");
	}
	test_the_checks_catch_clipping();
	test_the_status_drawer_fits();
	test_the_drawer_tabs_fit();
	test_the_pinned_revision_card_fits();
	test_the_overview_conversation_tabs_fit();
	test_the_author_drawer_fits();
	test_the_sheet_and_pages_fit();
	for (const auto& failure : failures) {
		std::cerr << "CLIPPED: " << failure << '\n';
	}
	expect(failures.empty(), std::to_string(failures.size()) + " surface states are clipped or break the principles");
	std::cout << "drawer-surfaces-fit:pass dpr=" << application.devicePixelRatio() << '\n';
	return EXIT_SUCCESS;
}
