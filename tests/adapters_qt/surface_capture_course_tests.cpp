/// \file
/// \brief Builds the four-appearance, four-scale rendered and reachable course
/// for RFC-BPR-0032's Accounts and Home surfaces, RFC-BPR-0033's Profiles sheet
/// and RFC-BPR-0034's Appearance page.
///
/// \details [[BPR-0779]]. RFC-BPR-0032 A8, RFC-BPR-0033 B5 and RFC-BPR-0034 C6
/// each ask for "the existing four-appearance, four-scale capture course" to be
/// extended. No such shared course existed. The four appearances were iterated
/// inside individual translation units, and the registered scale variants were
/// 100, 125 and 200 for every Qt course except `qt_settings_window`, which alone
/// carried 150. This file is that course, built rather than extended, and it is
/// the first place the four appearances and the four scales meet for these four
/// surfaces.
///
/// Every named state is composed through the shipped public surface, asserted to
/// be the state it claims to be, and only then captured. A capture is written
/// only when `BPR_SURFACE_CAPTURE_DIR` names a directory, which is the gating
/// convention `BPR_SETTINGS_RENDER_ROOT`, `BPR_HOME_CAPTURE_DIR` and
/// `BPR_QT_RENDER_PATH` already use: an ordinary run stays fast and writes
/// nothing, while the assertions run either way. That ordering is the point. A
/// course that emits a file for a state which failed to compose proves nothing,
/// so a state names itself before it is drawn, and a raster must be non-null and
/// exactly the logical size times the scale in force.
///
/// VoiceOver is not automated here and is not claimed anywhere in this file.
/// What is automated is the objective half of A8, B5 and C6: every focusable
/// node of the accessibility tree a screen reader actually walks carries a name,
/// a collapsed Home panel reports its collapsed state, a profile row's state is
/// its accessible description, and the selected theme tile's caption is
/// announced. The attended VoiceOver pass remains human evidence.

#include "bpr/application/device_authorization_projection.hpp"
#include "bpr/application/github_projection.hpp"
#include "bpr/application/home_view_state.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/review_queue_attention.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_panel.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/theme_preview_strip.hpp"
#include "bpr/qt/workbench_strip.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
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

	/// \brief Fails the course, naming what was expected.
	///
	/// \details Two overloads rather than one taking `std::string_view`, because a
	/// string literal converts as readily to `QString` as to `std::string_view`
	/// and every call naming a state would have been ambiguous. `const char*` is
	/// an exact match for a literal, so each call resolves without a cast.
	void expect(const bool aCondition, const char* const aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void expect(const bool aCondition, const QString& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage.toStdString() << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void process_events()
	{
		QApplication::processEvents();
		QApplication::sendPostedEvents();
		QApplication::processEvents();
	}

	/// \brief One key on the theme strip, which is how a tile is both chosen and
	/// scrolled into its well.
	///
	/// \details `appearance_settings_panel::choose_mode` marks a tile through
	/// `theme_preview_strip::set_current`, and that deliberately does not scroll:
	/// it mirrors what the host published, and a published adoption must not move
	/// the well under the reviewer's pointer. Left and Right are the gesture that
	/// chooses and reveals together, so a course that needs every tile drawn
	/// presses them, exactly as `qt_theme_preview_strip` already does.
	void press_strip(qt::theme_preview_strip& aStrip, const Qt::Key aKey)
	{
		auto press = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
		QApplication::sendEvent(&aStrip, &press);
		process_events();
	}

	/// \brief The four appearances A8, B5 and C6 all name, in their stated order.
	constexpr std::array appearance_modes{
		core::appearance_mode::light,
		core::appearance_mode::dark,
		core::appearance_mode::high_contrast_light,
		core::appearance_mode::high_contrast_dark,
	};

	[[nodiscard]] auto appearance_slug(const core::appearance_mode aMode) -> QString
	{
		switch (aMode) {
		case core::appearance_mode::light:
			return QStringLiteral("light");
		case core::appearance_mode::dark:
			return QStringLiteral("dark");
		case core::appearance_mode::high_contrast_light:
			return QStringLiteral("hc-light");
		case core::appearance_mode::high_contrast_dark:
			return QStringLiteral("hc-dark");
		default:
			break;
		}
		expect(false, "only the four named appearances are captured");
		return QString{ };
	}

	/// \brief The variant each captured appearance resolves to.
	///
	/// \details The four captured modes each name their own variant, so the
	/// composition can supply what only it knows without this course observing a
	/// desktop it is not allowed to read.
	[[nodiscard]] auto variant_of(const core::appearance_mode aMode)
		-> core::appearance_variant
	{
		switch (aMode) {
		case core::appearance_mode::light:
			return core::appearance_variant::light;
		case core::appearance_mode::dark:
			return core::appearance_variant::dark;
		case core::appearance_mode::high_contrast_light:
			return core::appearance_variant::high_contrast_light;
		case core::appearance_mode::high_contrast_dark:
			return core::appearance_variant::high_contrast_dark;
		default:
			break;
		}
		expect(false, "only the four named appearances resolve a variant here");
		return core::appearance_variant::light;
	}

	/// \brief The scale this run was registered for, when it was told one.
	[[nodiscard]] auto expected_scale() -> std::optional<double>
	{
		const auto text = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
		if (text.isEmpty()) {
			return std::nullopt;
		}
		auto       parsed = false;
		const auto value  = text.toDouble(&parsed);
		expect(parsed, "the expected Qt scale is numeric");
		return value;
	}

	/// \brief "100", "125", "150", "200": the scale as the file name spells it.
	[[nodiscard]] auto scale_slug() -> QString
	{
		const auto scale = expected_scale();
		return scale.has_value()
			? QString::number(static_cast<int>(std::lround(*scale * 100.0)))
			: QStringLiteral("unset");
	}

	/// \brief Where captures are written, or empty in an ordinary run.
	[[nodiscard]] auto capture_directory() -> QString
	{
		return qEnvironmentVariable("BPR_SURFACE_CAPTURE_DIR");
	}

	std::size_t CapturesWritten = 0U;
	std::size_t StatesCaptured  = 0U;

	/// \brief Draws one composed state and proves the raster is the real thing.
	///
	/// \details The assertions are the course; the file is the attended material.
	/// A grab that is null, empty, or not exactly the widget's logical size times
	/// the device-pixel ratio in force is a failure rather than a thin file, and a
	/// save that does not happen when a directory was named is a failure too. That
	/// is what stops a state which failed to compose from passing quietly.
	void capture(
		QWidget&                    aSurface,
		const QString&              aSurfaceName,
		const QString&              aStateName,
		const core::appearance_mode aMode)
	{
		process_events();
		const auto rendered = aSurface.grab();
		const auto ratio    = aSurface.devicePixelRatioF();
		if (const auto scale = expected_scale(); scale.has_value()) {
			expect(
				std::abs(ratio - *scale) < 0.02,
				QStringLiteral("%1 %2 renders at the scale the course was registered for")
					.arg(aSurfaceName, aStateName));
		}
		expect(
			!rendered.isNull() && rendered.width() > 0 && rendered.height() > 0,
			QStringLiteral("%1 %2 composes to a nonempty raster")
				.arg(aSurfaceName, aStateName));
		expect(
			rendered.width()
					== static_cast<int>(
						std::lround(static_cast<double>(aSurface.width()) * ratio))
				&& rendered.height()
					== static_cast<int>(
						std::lround(static_cast<double>(aSurface.height()) * ratio)),
			QStringLiteral("%1 %2 raster is its logical size times the scale")
				.arg(aSurfaceName, aStateName));
		++StatesCaptured;

		const auto directory = capture_directory();
		if (directory.isEmpty()) {
			return;
		}
		const auto path = QStringLiteral("%1/%2-%3-%4-%5.png")
							  .arg(
								  directory,
								  aSurfaceName,
								  aStateName,
								  appearance_slug(aMode),
								  scale_slug());
		expect(
			rendered.save(path, "PNG"),
			QStringLiteral("the capture for %1 %2 is written").arg(aSurfaceName, aStateName));
		++CapturesWritten;
	}

	/// \brief Applies one of the four appearances to a surface, as Settings does.
	void apply_appearance(qt::appearance_profile& aProfile, QWidget& aSurface)
	{
		aProfile.apply_to(aSurface);
		process_events();
	}

	// ---------------------------------------------------------------- fixtures

	[[nodiscard]] auto host_profiles()
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

	[[nodiscard]] auto session_projection()
		-> application::github_session_projection
	{
		return {
			.Profile     = core::host_profile_id{ 12U },
			.Host        = core::git_host_id{ 22U },
			.Account     = core::github_account_id{ 52U },
			.Login       = "octo-reviewer",
			.DisplayName = "Octo Reviewer",
		};
	}

	/// \brief The Accounts page's connected state, through the shipped publish.
	void publish_session_ready(qt::workbench_window& aWindow)
	{
		const auto session = session_projection();
		aWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
					.Status = core::github_status::session_ready,
				},
				.Identity = core::github_identity_delta{
					.Profile    = session.Profile,
					.Host       = session.Host,
					.Account    = session.Account,
					.Generation = core::github_identity_generation{ 2U },
				},
			},
			.Session = session,
		});
		process_events();
	}

	[[nodiscard]] auto queue_row(
		const std::int64_t           aNumber,
		const core::attention_bucket aBucket,
		const std::string&           aAccount,
		const std::string&           aRepository) -> core::queue_item
	{
		auto row = core::queue_item{
			.Ref        = { .Host = "github.com", .Account = aAccount, .Repository = aRepository, .Number = aNumber },
			.Membership = { .Role = core::github_role::reviewer },
			.Bucket     = aBucket,
		};
		row.Facts = core::queue_item_facts{ .Title = "A title", .Author = "octocat" };
		return row;
	}

	/// \brief Home's default board: two accounts over two owners.
	[[nodiscard]] auto default_queue() -> application::queue_landing_projection
	{
		return {
			.Status = application::queue_landing_status::ready,
			.Items  = core::ordered_queue(std::vector{
				queue_row(101, core::attention_bucket::initial_review_requested, "octo-reviewer", "octo-org/app"),
				queue_row(102, core::attention_bucket::changed_since_my_review, "octo-reviewer", "octo-org/app"),
				queue_row(201, core::attention_bucket::authored_awaiting_reviewer_action, "octo-enterprise", "acme-tools/build"),
				queue_row(202, core::attention_bucket::observed_with_new_activity, "octo-enterprise", "acme-tools/build"),
				queue_row(301, core::attention_bucket::quiet, "octo-reviewer", "octo-reviewer/notes"),
			}),
		};
	}

	/// \brief A ready queue in which nothing is mine to review.
	///
	/// \details Only the buckets `home_group_of` maps away from Needs me, so the
	/// Needs me panel is absent rather than an empty box, which is what A6 asks
	/// the nothing-needs-me board to show.
	[[nodiscard]] auto nothing_needs_me_queue()
		-> application::queue_landing_projection
	{
		return {
			.Status = application::queue_landing_status::ready,
			.Items  = core::ordered_queue(std::vector{
				queue_row(401, core::attention_bucket::authored_awaiting_reviewer_action, "octo-reviewer", "octo-org/app"),
				queue_row(402, core::attention_bucket::observed_with_new_activity, "octo-reviewer", "octo-org/tools"),
				queue_row(403, core::attention_bucket::quiet, "octo-reviewer", "octo-reviewer/notes"),
			}),
		};
	}

	/// \brief A queue long enough that Home has somewhere to scroll to.
	[[nodiscard]] auto long_queue() -> application::queue_landing_projection
	{
		auto rows = std::vector<core::queue_item>{ };
		for (auto number = std::int64_t{ 1 }; number <= 60; ++number) {
			rows.push_back(queue_row(
				number,
				core::attention_bucket::initial_review_requested,
				"octo-reviewer",
				"octo-org/app"));
		}
		return {
			.Status = application::queue_landing_status::ready,
			.Items  = core::ordered_queue(rows),
		};
	}

	/// \brief An open pull request, which is what puts Home over a review.
	///
	/// \details `update_home_landing` reads `HasGithubPullRequest` for the
	/// `back_to_review` landing, and that flag is set by a published pull-request
	/// projection. Driving the landing state directly would be setting the answer
	/// rather than reaching it.
	void publish_open_pull_request(qt::workbench_window& aWindow)
	{
		auto pullRequest = application::github_pull_request_projection{
			.Profile        = core::host_profile_id{ 12U },
			.Host           = core::git_host_id{ 22U },
			.Account        = core::github_account_id{ 52U },
			.Snapshot       = core::provider_snapshot_id{ 91U },
			.Repository     = core::repository_id{ 71U },
			.PullRequest    = core::pull_request_id{ 81U },
			.Owner          = "octo-org",
			.RepositoryName = "app",
			.Number         = core::pull_request_number{ 27U },
			.Title          = "A pull request being read",
			.AuthorLogin    = "octo-author",
			.BaseRef        = "main",
			.HeadRef        = "topic",
			.FileCount      = 2U,
			.CommitCount    = 3U,
		};
		aWindow.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status = core::github_status_delta{
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
		process_events();
	}

	[[nodiscard]] auto profile_projection()
		-> application::profile_management_projection
	{
		return { .Profiles = { { core::default_bpr_profile_id, "Default", application::profile_management_runtime::open },
							   { core::bpr_profile_id{ 2U }, "Work & Research", application::profile_management_runtime::closed } },
				 .Ready    = true };
	}

	// ------------------------------------------------- A8: the Accounts states

	/// \brief RFC-BPR-0032 A8's Accounts page in its three named states.
	///
	/// \details The page is the composed Settings window, which is the direct
	/// surface A1 and A4 are already checked on, so the card states are reached
	/// exactly as a reviewer reaches them: the page opens signed out, a device
	/// flow makes the chosen host's card say Signing in, and an accepted session
	/// makes it say Connected.
	void capture_the_accounts_states(
		qt::workbench_window&       aWindow,
		const core::appearance_mode aMode)
	{
		auto* const settings = aWindow.settings();
		settings->present(qt::settings_page::accounts_and_hosts);
		process_events();

		auto* const enterprise = aWindow.server_card_for(core::host_profile_id{ 12U });
		expect(
			enterprise != nullptr
				&& settings->page_widget(qt::settings_page::accounts_and_hosts)
					   ->isAncestorOf(enterprise),
			"the enterprise server's card is on Accounts and hosts");
		expect(
			enterprise->state_label()->text() == QStringLiteral("Signed out"),
			"the Accounts page opens in its signed-out state");
		capture(*settings, QStringLiteral("accounts"), QStringLiteral("signed-out"), aMode);

		// Signing in: the chosen host's card carries the live device code as its
		// account row, which is where [[BPR-0773]] put it.
		auto* const chosen = aWindow.server_card_for(core::host_profile_id{ 11U });
		expect(chosen != nullptr, "the GitHub.com card is on the page too");
		if (chosen->connect_button()->isVisibleTo(chosen)) {
			chosen->connect_button()->click();
			process_events();
		}
		auto waiting = application::device_authorization_presentation{
			.Stage                    = core::device_authorization_stage::awaiting_authorization,
			.UserCode                 = "TEST-CODE",
			.VerificationUri          = "fixture-sign-in",
			.RemainingValiditySeconds = 300U,
		};
		waiting.Commands = core::device_authorization_commands(
			core::device_authorization_state{
				.Stage = core::device_authorization_stage::awaiting_authorization,
			});
		aWindow.device_authorization()->apply(waiting);
		process_events();
		expect(
			chosen->state_label()->text() == QStringLiteral("Signing in")
				&& chosen->device_sign_in() == aWindow.device_authorization(),
			"the chosen host's card says Signing in and holds the live code");
		capture(*settings, QStringLiteral("accounts"), QStringLiteral("signing-in"), aMode);

		publish_session_ready(aWindow);
		expect(
			enterprise->state_label()->text() == QStringLiteral("Connected")
				&& enterprise->signed_in()
				&& enterprise->login_label()->text() == QStringLiteral("octo-reviewer"),
			"an accepted session makes its host's card say Connected");
		capture(*settings, QStringLiteral("accounts"), QStringLiteral("connected"), aMode);
	}

	// ----------------------------------------- C6: the Appearance page states

	/// \brief RFC-BPR-0034 C6's Appearance page: default, editing, light desktop.
	///
	/// \details Captured on the composed Settings page rather than on a bare
	/// panel, because C6 is about the page. Every tile is shown at least once:
	/// choosing a mode scrolls its tile into the well, and the tile is asserted
	/// visible in the strip before the page is drawn.
	void capture_the_appearance_states(
		qt::workbench_window&       aWindow,
		qt::appearance_profile&     aProfile,
		const core::appearance_mode aMode)
	{
		auto* const settings = aWindow.settings();
		auto* const panel    = aWindow.appearance_settings();
		expect(panel != nullptr, "the Appearance page holds the appearance panel");

		auto adopted = qt::default_appearance_settings();
		adopted.Mode = aMode;
		panel->adopt(adopted, qt::builtin_appearance_palette(variant_of(aMode)));
		panel->set_resolved_variant(variant_of(aMode));
		panel->adopt_appearance(aProfile);
		settings->present(qt::settings_page::appearance);
		process_events();

		auto* const strip = panel->theme_previews();
		expect(
			strip != nullptr && strip->cards().size() == qt::theme_preview_count,
			"the Theme card carries one tile per mode the core declares");
		expect(
			panel->preview_bar() == nullptr,
			"the default Appearance page previews nothing, so it has no preview bar");
		capture(*settings, QStringLiteral("appearance"), QStringLiteral("default"), aMode);

		// C6 asks for the page "with every tile". The well is narrower than the
		// strip, so a tile is drawn only once it has been scrolled into view.
		// Space re-chooses the tile already current, which reveals it without
		// moving the selection; Right then walks the remaining five, revealing
		// each as it goes. Each is asserted whole in the well rather than merely
		// clipped into it, which is the assertion `qt_theme_preview_strip` makes.
		panel->choose_mode(core::appearance_mode::system);
		press_strip(*strip, Qt::Key_Space);
		for (auto index = std::size_t{ 0U }; index < qt::theme_preview_count; ++index) {
			if (index > 0U) {
				press_strip(*strip, Qt::Key_Right);
			}
			const auto tile = static_cast<core::appearance_mode>(
				static_cast<std::uint8_t>(index));
			expect(
				strip->current() == tile,
				"the keyboard walks the theme tiles in their declared order");
			expect(
				strip->tile_rect_in_strip(index).size()
					== strip->cards()[index].Rect.size(),
				QStringLiteral("theme tile %1 is scrolled whole into the well")
					.arg(index));
			capture(
				*settings,
				QStringLiteral("appearance"),
				QStringLiteral("tile-%1").arg(index),
				aMode);
		}
		panel->choose_mode(aMode);
		process_events();

		// Editing: a previewed palette is the state in which the Colours card
		// carries its one bar, and C5 makes that bar's presence the evidence.
		panel->preview_import(qt::builtin_appearance_palette(core::appearance_variant::dark));
		process_events();
		expect(
			panel->preview_bar() != nullptr && panel->preview_palette().has_value()
				&& panel->keep_changes_button() != nullptr
				&& panel->cancel_changes_button() != nullptr,
			"the editing state shows one preview bar with Keep changes and Cancel");
		capture(*settings, QStringLiteral("appearance"), QStringLiteral("editing"), aMode);
		panel->cancel_preview();
		process_events();
		expect(
			panel->preview_bar() == nullptr,
			"cancelling the preview removes the bar again");

		// The light-desktop state: a System selection while the observed desktop
		// is light. The observation is supplied, never read, so the page states
		// what the composition resolved rather than deciding it.
		auto desktop     = qt::appearance_profile{ };
		auto systemLight = qt::default_appearance_settings();
		systemLight.Mode = core::appearance_mode::system;
		static_cast<void>(desktop.select_mode(core::appearance_mode::system));
		static_cast<void>(desktop.observe(core::appearance_observation{
			.Scheme   = core::os_color_scheme::light,
			.Contrast = core::os_contrast_preference::standard,
		}));
		panel->adopt(systemLight, qt::builtin_appearance_palette(core::appearance_variant::light));
		panel->set_resolved_variant(core::appearance_variant::light);
		panel->adopt_appearance(desktop);
		process_events();
		expect(
			strip->current() == core::appearance_mode::system
				&& strip->system_variant() == std::optional{ core::appearance_variant::light },
			"the light-desktop state selects System and says the desktop is light");
		capture(*settings, QStringLiteral("appearance"), QStringLiteral("light-desktop"), aMode);

		// Put the page back to the appearance being captured, so the surfaces
		// after this one are not left describing a desktop they do not have.
		panel->adopt(adopted, qt::builtin_appearance_palette(variant_of(aMode)));
		panel->set_resolved_variant(variant_of(aMode));
		panel->adopt_appearance(aProfile);
		process_events();
		settings->hide();
		process_events();
	}

	// ------------------------------------------------------ A8: the Home boards

	/// \brief Proves Home composes inside the 720-point column A8 constrains it to.
	///
	/// \details The constraint is a property of the column Home is drawn in, not a
	/// clamp the window performs: `workbench_window` places no maximum width on
	/// Home's column. So it is checked the way `qt_settings_window` checks its
	/// bounded page, by narrowing the surface to the stated bound and asserting
	/// nothing has to be reached sideways.
	/// \brief Measures Home's column and asserts the rule RFC-BPR-0032 states.
	///
	/// \details [[BPR-0779]]'s AC1 asks for captures "with the Home column at most
	/// 720 points". RFC-BPR-0032 says the opposite in as many words — "Home is the
	/// window ground edge to edge, not a narrow column" — and states no width
	/// anywhere in the document. The only 720 in the tree is
	/// `home_list_tests.cpp`'s `List.resize(720, 900)`, which is the width that
	/// component fixture happens to give a bare list rather than a bound anything
	/// enforces: `workbench_window` places no maximum width on Home's column, and
	/// at the narrowest width the window itself supports the column measures 852
	/// points.
	///
	/// Asserting the card's figure would therefore assert the opposite of the RFC
	/// and fail, and relaxing it to 852 would dress a measurement up as a rule. So
	/// what is asserted is the RFC's own rule, which is both satisfiable and the
	/// property worth protecting: the list spans the window rather than sitting in
	/// a narrow column, and Home is never read sideways. The measured width is
	/// printed at each width so the attended reviewer has the number AC1 asked
	/// about, and the contradiction is carried back to the card rather than
	/// settled here.
	void report_home_column_and_assert_it_spans_the_window(
		qt::workbench_window& aWindow, const char* const aWhere)
	{
		auto* const list = aWindow.home_list();
		expect(list != nullptr, "Home has its list");
		const auto column = list->viewport()->width();
		std::cout << "home-column: where=" << aWhere << " window=" << aWindow.width()
				  << " column=" << column
				  << " horizontal=" << list->horizontalScrollBar()->maximum() << '\n';
		expect(
			list->horizontalScrollBar()->maximum() == 0,
			"Home is never read sideways");
		expect(
			column * 10 >= aWindow.width() * 9,
			"Home is the window ground edge to edge rather than a narrow column");
	}

	/// \brief RFC-BPR-0032 A8's Home boards, in the order the criterion names them.
	void capture_the_home_boards(
		qt::workbench_window&       aWindow,
		const core::appearance_mode aMode)
	{
		aWindow.set_queue_preferences_dispatch([](application::queue_preferences_intent) { });
		aWindow.set_home_view_dispatch([](application::home_view_state) { });
		aWindow.resize(1'280, 820);
		aWindow.show();
		process_events();

		// First launch: no account at all, so the landing is the sign-in card.
		expect(
			aWindow.showing_home()
				&& aWindow.home_landing()->state() == qt::landing_state::sign_in,
			"a window with no account shows Home's first-launch card");
		// [[BPR-0779]]. The bar is "alone in the first row" (RFC-BPR-0032 A5),
		// and A6 lets no empty box render. Signed out there is no list to take
		// the column's spare height, and the bar's frame used to absorb it and
		// draw its border around roughly 530 points of nothing. The frame is
		// bordered, so its height is the box: hold it to what it asks for.
		auto* const firstLaunchBar = aWindow.home_open_frame();
		std::cout << "home-bar: state=first-launch height=" << firstLaunchBar->height()
				  << " hint=" << firstLaunchBar->sizeHint().height()
				  << " window=" << aWindow.height() << '\n';
		expect(
			firstLaunchBar->height() <= firstLaunchBar->sizeHint().height(),
			"first launch draws the Open bar at its natural height, not as a box of empty space");
		capture(aWindow, QStringLiteral("home"), QStringLiteral("first-launch"), aMode);

		publish_session_ready(aWindow);
		aWindow.publish(default_queue());
		process_events();
		expect(
			aWindow.home_landing()->state() == qt::landing_state::choose_pull_request
				&& aWindow.home_list()->state_panel(qt::home_group::needs_you)->isVisible(),
			"signed in with a ready queue, Home is the bar over its state panels");
		capture(aWindow, QStringLiteral("home"), QStringLiteral("default"), aMode);

		report_home_column_and_assert_it_spans_the_window(aWindow, "capture width");
		const auto captureSize = aWindow.size();
		aWindow.resize(aWindow.minimumWidth(), 820);
		process_events();
		report_home_column_and_assert_it_spans_the_window(aWindow, "narrowest supported");
		aWindow.resize(captureSize);
		process_events();

		// The four grouping boards, each reached by its own strip action.
		const auto groupings = std::array{
			std::pair{ application::home_grouping::none, QStringLiteral("group-none") },
			std::pair{ application::home_grouping::organization, QStringLiteral("group-organization") },
			std::pair{ application::home_grouping::repository, QStringLiteral("group-repository") },
			std::pair{ application::home_grouping::account, QStringLiteral("group-account") },
		};
		for (const auto& [grouping, name] : groupings) {
			aWindow.home_group_action(grouping)->trigger();
			process_events();
			expect(
				aWindow.home_list()->grouping() == grouping,
				QStringLiteral("Home regroups for %1").arg(name));
			capture(aWindow, QStringLiteral("home"), name, aMode);
		}
		aWindow.home_group_action(application::home_grouping::none)->trigger();
		process_events();

		// Nothing needs me: the Needs me panel is absent rather than an empty box.
		aWindow.publish(nothing_needs_me_queue());
		process_events();
		expect(
			!aWindow.home_list()->state_panel(qt::home_group::needs_you)->isVisible()
				&& aWindow.home_list()->state_panel(qt::home_group::following)->isVisible(),
			"with nothing needing me the Needs me panel is absent and the rest remain");
		capture(aWindow, QStringLiteral("home"), QStringLiteral("nothing-needs-me"), aMode);

		// The long list, actually scrolled.
		aWindow.publish(long_queue());
		process_events();
		auto* const bar = aWindow.home_list()->verticalScrollBar();
		expect(bar->maximum() > 0, "a sixty-row queue gives Home somewhere to scroll");
		bar->setValue(bar->maximum());
		process_events();
		expect(
			bar->value() == bar->maximum() && bar->value() > 0,
			"the long-list board is captured scrolled rather than at its top");
		capture(aWindow, QStringLiteral("home"), QStringLiteral("long-list-scrolled"), aMode);
		bar->setValue(0);
		process_events();

		// Over a review: an open pull request behind Home, reached by publishing
		// one and asking for Home, which is the reviewer's own path.
		aWindow.publish(default_queue());
		publish_open_pull_request(aWindow);
		aWindow.choose_home(true);
		process_events();
		expect(
			aWindow.showing_home()
				&& aWindow.home_landing()->state() == qt::landing_state::back_to_review,
			"Home over a review is the one row back to it");
		capture(aWindow, QStringLiteral("home"), QStringLiteral("over-a-review"), aMode);

		aWindow.hide();
		process_events();
	}

	// ------------------------------------------------ B5: the Profiles sheet

	/// \brief RFC-BPR-0033 B5's Profiles sheet in its four named states.
	void capture_the_profiles_sheet(
		qt::appearance_profile&     aProfile,
		const core::appearance_mode aMode)
	{
		auto sheet = qt::profile_management_window{ };
		sheet.set_dispatch([](application::profile_management_intent) { });
		sheet.set_window_counts({ { core::default_bpr_profile_id, 2 } });
		sheet.set_current_profile(core::default_bpr_profile_id);
		auto accepted = profile_projection();
		sheet.adopt(accepted);
		apply_appearance(aProfile, sheet);
		sheet.present();
		process_events();

		expect(
			sheet.profiles()->count() == 2 && !sheet.editing()
				&& sheet.status_text().isEmpty(),
			"the sheet's default state is its rows, with no editor and no notice");
		capture(sheet, QStringLiteral("profiles"), QStringLiteral("default"), aMode);

		sheet.profiles()->setCurrentRow(1);
		sheet.rename_button()->click();
		process_events();
		expect(
			sheet.editing() && sheet.name_editor()->isVisible()
				&& sheet.name_editor()->text() == QStringLiteral("Work & Research"),
			"Rename edits the selected row in place");
		capture(sheet, QStringLiteral("profiles"), QStringLiteral("renaming"), aMode);
		sheet.cancel_button()->click();
		process_events();

		sheet.create_button()->click();
		process_events();
		expect(
			sheet.editing() && sheet.profiles()->count() == 3
				&& sheet.name_editor()->text().isEmpty(),
			"New profile appends a row already in its editor");
		capture(sheet, QStringLiteral("profiles"), QStringLiteral("creating"), aMode);
		sheet.cancel_button()->click();
		process_events();

		accepted.Failure = application::profile_management_error{
			.Operation = application::profile_management_operation::rename,
			.Profile   = core::bpr_profile_id{ 2U },
			.Reason    = application::profile_management_failure::registry_unavailable,
		};
		sheet.adopt(accepted);
		process_events();
		expect(
			!sheet.status_text().isEmpty() && sheet.try_again_button()->isVisible(),
			"a refusal is one notice with Try again");
		capture(sheet, QStringLiteral("profiles"), QStringLiteral("failed"), aMode);
		sheet.close();
		process_events();
	}

	// ------------------------------- A8, B5, C6: reachability and announcement

	/// \brief One focusable node of the tree a screen reader walks.
	struct reachable_node
	{
		QString Name;
		QString Role;
		QString Owner;
	};

	/// \brief Walks the accessibility tree in reading order, gathering focus stops.
	///
	/// \details The accessibility tree rather than `findChildren`, because the
	/// tree a screen reader walks is the one the criterion is about: a control
	/// present as a widget but absent from this tree is not reachable, and a
	/// control named only by its object name is not named at all.
	void walk_accessible_tree(
		QAccessibleInterface* const  aNode,
		const QString&               aOwner,
		std::vector<reachable_node>& aStops)
	{
		if (aNode == nullptr) {
			return;
		}
		const auto  state  = aNode->state();
		auto* const object = aNode->object();
		// An editable combo box owns an internal `QLineEdit` carrying neither an
		// object name nor an accessible name. The control is the combo: it is
		// named, it is itself a stop, and it is what a screen reader announces.
		// The editor is Qt's composite internal rather than a second control, so
		// it is skipped by exactly that shape — a line edit whose parent is a
		// combo — and the combo is required to be named in its place. It is
		// deliberately not excused by a nearest-named-ancestor rule, which would
		// also hide a genuinely unnamed control sitting inside a named card.
		auto* const editor  = qobject_cast<QLineEdit*>(object);
		const auto  inCombo = editor != nullptr
			&& qobject_cast<QComboBox*>(editor->parentWidget()) != nullptr;
		if (inCombo) {
			expect(
				!editor->parentWidget()->accessibleName().isEmpty(),
				"an editable combo box that owns a text editor is itself named");
		}
		if (state.focusable && !state.invisible && !state.disabled && !inCombo) {
			aStops.push_back(reachable_node{
				.Name  = aNode->text(QAccessible::Name),
				.Role  = QString::number(static_cast<int>(aNode->role())),
				.Owner = object != nullptr
					? QString::fromLatin1(object->metaObject()->className())
						+ QStringLiteral("(") + object->objectName() + QStringLiteral(")")
					: aOwner,
			});
		}
		for (auto child = 0; child < aNode->childCount(); ++child) {
			walk_accessible_tree(aNode->child(child), aOwner, aStops);
		}
	}

	/// \brief Every focus stop of a surface, in reading order.
	[[nodiscard]] auto focus_stops(QWidget& aSurface, const QString& aOwner)
		-> std::vector<reachable_node>
	{
		auto stops = std::vector<reachable_node>{ };
		walk_accessible_tree(
			QAccessible::queryAccessibleInterface(&aSurface), aOwner, stops);
		return stops;
	}

	/// \brief Fails on the first reachable control that carries no name, by name.
	void expect_every_stop_is_named(
		QWidget& aSurface, const QString& aSurfaceName)
	{
		const auto stops = focus_stops(aSurface, aSurfaceName);
		expect(
			!stops.empty(),
			QStringLiteral("%1 offers the keyboard something to reach").arg(aSurfaceName));
		// Every unnamed stop is reported rather than only the first. One run then
		// yields the whole inventory of what a screen reader would reach and be
		// unable to announce, which is what makes the result actionable; failing
		// on the first would surface them one rebuild at a time.
		auto unnamed = QStringList{ };
		for (const auto& stop : stops) {
			if (stop.Name.isEmpty()) {
				unnamed.push_back(
					QStringLiteral("%1 (accessible role %2)").arg(stop.Owner, stop.Role));
			}
		}
		std::cout << "reachable: " << aSurfaceName.toStdString()
				  << " stops=" << stops.size() << " unnamed=" << unnamed.size() << '\n';
		for (const auto& said : unnamed) {
			std::cout << "unnamed-stop: " << aSurfaceName.toStdString() << ": "
					  << said.toStdString() << '\n';
		}
		expect(
			unnamed.isEmpty(),
			QStringLiteral("%1: every reachable control has an accessible name; these do not: %2")
				.arg(aSurfaceName, unnamed.join(QStringLiteral(", "))));
	}

	/// \brief A8: every Home control is reachable and named, and a collapsed
	/// panel's state is announced as collapsed.
	void walk_home_and_its_collapsed_panel()
	{
		auto window = qt::workbench_window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			host_profiles(),
			[](application::github_intent) { },
		};
		window.set_queue_preferences_dispatch([](application::queue_preferences_intent) { });
		window.set_home_view_dispatch([](application::home_view_state) { });
		window.resize(1'280, 820);
		window.show();
		publish_session_ready(window);
		window.publish(default_queue());
		process_events();

		expect_every_stop_is_named(*window.home_page(), QStringLiteral("Home"));

		// Home's own strip controls are declared in the window's focus order, so
		// the reading order is the window's rather than a second one invented here.
		for (auto* const stop : { static_cast<QWidget*>(window.home_view_button()) }) {
			expect(
				stop != nullptr && window.focus_order().contains(stop)
					&& !stop->accessibleName().isEmpty(),
				"each of Home's strip controls is declared in the focus order and named");
		}

		// A collapsed panel says it is collapsed. `qt_home_list` already proves
		// this on the bare list; what is added here is that the composed window's
		// Home says it too, which is the surface A8 is about.
		auto* const needs = window.home_list()->state_panel(qt::home_group::needs_you);
		expect(needs != nullptr && needs->header() != nullptr, "Needs me is a panel with a header");
		auto* const header = QAccessible::queryAccessibleInterface(needs->header());
		expect(header != nullptr, "the panel header is in the accessibility tree");
		expect(
			header->state().expanded && !header->state().collapsed,
			"an expanded Home panel announces itself as expanded");
		needs->set_collapsed(true);
		process_events();
		expect(
			header->state().collapsed && !header->state().expanded
				&& !header->text(QAccessible::Name).isEmpty(),
			"a collapsed Home panel announces its collapsed state and keeps its name");
		needs->set_collapsed(false);
		process_events();
		window.hide();
		process_events();
	}

	/// \brief A8: the Accounts page is reachable and named in each of its states.
	void walk_the_accounts_page()
	{
		auto window = qt::workbench_window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			host_profiles(),
			[](application::github_intent) { },
		};
		window.resize(1'280, 820);
		window.show();
		auto* const settings = window.settings();
		settings->present(qt::settings_page::accounts_and_hosts);
		process_events();
		expect_every_stop_is_named(
			*settings->page_widget(qt::settings_page::accounts_and_hosts),
			QStringLiteral("Accounts and hosts, signed out"));
		publish_session_ready(window);
		expect_every_stop_is_named(
			*settings->page_widget(qt::settings_page::accounts_and_hosts),
			QStringLiteral("Accounts and hosts, connected"));
		settings->hide();
		window.hide();
		process_events();
	}

	/// \brief B5: the sheet is reachable and named, and a row's state is its
	/// accessible description.
	void walk_the_profiles_sheet()
	{
		auto sheet = qt::profile_management_window{ };
		sheet.set_dispatch([](application::profile_management_intent) { });
		sheet.set_window_counts({ { core::default_bpr_profile_id, 2 } });
		sheet.set_current_profile(core::default_bpr_profile_id);
		sheet.adopt(profile_projection());
		sheet.present();
		process_events();

		expect_every_stop_is_named(sheet, QStringLiteral("Profiles sheet"));

		// B5 states it exactly: the row's state is its accessible description.
		expect(
			sheet.profiles()->item(0)->data(Qt::AccessibleDescriptionRole).toString()
					== QStringLiteral("Open in 2 windows")
				&& sheet.profiles()->item(1)->data(Qt::AccessibleDescriptionRole).toString()
					== QStringLiteral("Closed"),
			"each profile row's state is that row's accessible description");
		for (auto row = 0; row < sheet.profiles()->count(); ++row) {
			expect(
				!sheet.profiles()->item(row)->data(Qt::AccessibleDescriptionRole).toString().isEmpty()
					&& !sheet.profiles()->item(row)->text().isEmpty(),
				"every profile row carries both a name and a state description");
		}
		sheet.close();
		process_events();
	}

	/// \brief C6: the Appearance page is reachable and named, and the selected
	/// tile's caption is announced.
	///
	/// \details The tiles are painted onto one canvas rather than being six
	/// widgets, so the strip is the single accessible object and it announces the
	/// selected tile's name and caption. Each of the six is therefore selected in
	/// turn and its own caption asserted to be what the strip says.
	void walk_the_appearance_page()
	{
		auto window = qt::workbench_window{ };
		window.resize(1'280, 820);
		window.show();
		auto* const settings = window.settings();
		auto* const panel    = window.appearance_settings();
		auto        profile  = qt::appearance_profile{ };
		panel->adopt(
			qt::default_appearance_settings(),
			qt::builtin_appearance_palette(core::appearance_variant::light));
		panel->set_resolved_variant(core::appearance_variant::light);
		panel->adopt_appearance(profile);
		settings->present(qt::settings_page::appearance);
		process_events();

		expect_every_stop_is_named(
			*settings->page_widget(qt::settings_page::appearance),
			QStringLiteral("Appearance page"));

		auto* const strip = panel->theme_previews();
		expect(strip != nullptr, "the Appearance page carries the theme strip");
		expect(
			!strip->accessibleName().isEmpty(),
			"the theme strip is named");
		panel->choose_mode(core::appearance_mode::system);
		press_strip(*strip, Qt::Key_Space);
		for (auto index = std::size_t{ 0U }; index < qt::theme_preview_count; ++index) {
			if (index > 0U) {
				press_strip(*strip, Qt::Key_Right);
			}
			const auto mode = static_cast<core::appearance_mode>(
				static_cast<std::uint8_t>(index));
			expect(
				strip->current() == mode,
				"the keyboard reaches each theme tile in turn");
			const auto& card      = strip->cards()[index];
			const auto  announced = strip->accessibleName() + QStringLiteral(" ")
				+ strip->accessibleDescription();
			expect(
				!card.Name.isEmpty() && announced.contains(card.Name),
				QStringLiteral("theme tile %1's name is announced").arg(index));
			expect(
				card.Note.isEmpty() || announced.contains(card.Note),
				QStringLiteral("theme tile %1's caption \"%2\" is announced")
					.arg(index)
					.arg(card.Note));
		}
		settings->hide();
		window.hide();
		process_events();
	}

	// -------------------------------------------------------------- the course

	/// \brief One appearance, over all four surfaces.
	void capture_one_appearance(const core::appearance_mode aMode)
	{
		auto appearance = qt::appearance_profile{ };
		static_cast<void>(appearance.select_mode(aMode));

		auto settingsWindow = qt::workbench_window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			host_profiles(),
			[](application::github_intent) { },
		};
		settingsWindow.resize(1'280, 820);
		settingsWindow.show();
		apply_appearance(appearance, settingsWindow);
		apply_appearance(appearance, *settingsWindow.settings());
		capture_the_accounts_states(settingsWindow, aMode);
		capture_the_appearance_states(settingsWindow, appearance, aMode);
		settingsWindow.hide();
		process_events();

		auto homeWindow = qt::workbench_window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			host_profiles(),
			[](application::github_intent) { },
		};
		apply_appearance(appearance, homeWindow);
		capture_the_home_boards(homeWindow, aMode);

		capture_the_profiles_sheet(appearance, aMode);
	}
	// ------------------------------------------ [[BPR-1580]]: Home's View menu

	/// \brief Paints a closed menu where it opens: under its button, held
	/// inside the window, with an open submenu beside the item that opened it.
	///
	/// \details The offscreen platform places a popup against its own small
	/// screen, not against the window, so the course places the grabs itself.
	/// A capture is written only when `BPR_HOME_VIEW_CAPTURE_DIR` names a
	/// directory; the assertions run either way.
	void capture_home_view_state(
		qt::workbench_window&       aWindow,
		const QString&              aState,
		const core::appearance_mode aMode,
		QMenu* const                aMenu    = nullptr,
		QMenu* const                aSubmenu = nullptr)
	{
		process_events();
		auto image = aWindow.grab();
		expect(!image.isNull(), QStringLiteral("View state %1 composes").arg(aState));
		auto bottom = aWindow.home_list()->mapTo(&aWindow, QPoint{ 0, 0 }).y() + 220;
		// A menu is drawn at the size it asks for, whatever the offscreen
		// screen would have allowed it.
		const auto grabbed = [](QMenu& aOpen) {
			aOpen.ensurePolished();
			aOpen.resize(aOpen.sizeHint());
			return aOpen.grab();
		};
		if (aMenu != nullptr) {
			auto* const button = aWindow.home_view_button();
			const auto  menu   = grabbed(*aMenu);
			auto        at     = button->mapTo(&aWindow, QPoint{ 0, button->height() });
			at.setX(std::min(at.x(), aWindow.width() - aMenu->width() - 4));
			auto painter = QPainter{ &image };
			painter.drawPixmap(at, menu);
			bottom = std::max(bottom, at.y() + aMenu->height() + 12);
			if (aSubmenu != nullptr) {
				const auto sub  = grabbed(*aSubmenu);
				const auto row  = aMenu->actionGeometry(aSubmenu->menuAction());
				auto       left = at.x() + aMenu->width() - 2;
				if (left + aSubmenu->width() > aWindow.width()) {
					left = at.x() - aSubmenu->width() + 2;
				}
				const auto top = at.y() + row.top();
				painter.drawPixmap(QPoint{ left, top }, sub);
				bottom = std::max(bottom, top + aSubmenu->height() + 12);
			}
		}
		++StatesCaptured;
		const auto directory = qEnvironmentVariable("BPR_HOME_VIEW_CAPTURE_DIR");
		if (directory.isEmpty()) {
			return;
		}
		const auto ratio = image.devicePixelRatio();
		image            = image.copy(QRect{ 0, 0, image.width(), static_cast<int>(std::lround(std::min(bottom, aWindow.height()) * ratio)) });
		const auto path  = QStringLiteral("%1/view-%2-%3.png").arg(directory, aState, appearance_slug(aMode));
		expect(image.save(path, "PNG"), QStringLiteral("the View capture %1 is written").arg(aState));
		++CapturesWritten;
	}

	/// \brief The View control and its menu in one appearance: closed at the
	/// defaults, open, a submenu open, a changed view and a view that hides
	/// every row.
	void capture_the_home_view(const core::appearance_mode aMode)
	{
		auto appearance = qt::appearance_profile{ };
		static_cast<void>(appearance.select_mode(aMode));
		auto window = qt::workbench_window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			host_profiles(),
			[](application::github_intent) { },
		};
		apply_appearance(appearance, window);
		window.set_queue_preferences_dispatch([](application::queue_preferences_intent) { });
		window.set_home_view_dispatch([](application::home_view_state) { });
		window.resize(1'280, 820);
		window.show();
		process_events();
		publish_session_ready(window);
		auto queue  = default_queue();
		queue.Rules = { core::observer_rule{ .Kind = core::observer_rule_kind::path, .Pattern = "src/" } };
		window.publish(queue);
		process_events();
		// A native menu takes the desktop's appearance; the course gives these
		// the appearance being captured, which is what that desktop would show.
		auto* const menu = window.home_view_menu();
		apply_appearance(appearance, *menu);
		for (const auto choice : { qt::home_view_choice::group, qt::home_view_choice::sort, qt::home_view_choice::role,
								   qt::home_view_choice::added_by, qt::home_view_choice::quiet }) {
			apply_appearance(appearance, *window.home_view_submenu(choice));
		}

		auto* const button = window.home_view_button();
		expect(button->isVisible() && button->text() == QStringLiteral("View")
				   && button->property(qt::strip_style::badge_property).toString().isEmpty(),
			   "View starts at the defaults, plain");
		capture_home_view_state(window, QStringLiteral("closed"), aMode);
		capture_home_view_state(window, QStringLiteral("open"), aMode, menu);
		capture_home_view_state(window, QStringLiteral("my-role"), aMode, menu, window.home_view_submenu(qt::home_view_choice::role));
		capture_home_view_state(window, QStringLiteral("quiet-items"), aMode, menu, window.home_view_submenu(qt::home_view_choice::quiet));

		window.home_group_action(application::home_grouping::repository)->trigger();
		window.home_view_menu()->findChild<QAction*>(QStringLiteral("homeFilterHideAction"))->trigger();
		process_events();
		expect(button->property(qt::strip_style::badge_property).toString().isEmpty() && button->toolTip().contains(QStringLiteral("2 changed")),
			   "a view changed twice keeps View plain and names both changes in its tooltip");
		capture_home_view_state(window, QStringLiteral("changed"), aMode);
		capture_home_view_state(window, QStringLiteral("changed-open"), aMode, menu);

		window.home_view_reset_action()->trigger();
		window.home_view_menu()->findChild<QAction*>(QStringLiteral("homeFilterAuthorAction"))->trigger();
		process_events();
		expect(window.home_list()->filtered_sentence()->isVisible(), "a view that hides every row says so in the list");
		capture_home_view_state(window, QStringLiteral("filtered-empty"), aMode);
		window.hide();
		process_events();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };

	// A capture course that ran at the wrong scale would be four copies of one
	// board, so the scale is proved before anything is drawn.
	if (const auto scale = expected_scale(); scale.has_value()) {
		auto probe = QWidget{ };
		probe.show();
		QApplication::processEvents();
		expect(
			std::abs(probe.devicePixelRatio() - *scale) < 0.01,
			"the course runs at the scale it was registered for");
		probe.hide();
	}

	if (const auto directory = capture_directory(); !directory.isEmpty()) {
		expect(
			QDir{ }.mkpath(directory),
			"the capture directory named by BPR_SURFACE_CAPTURE_DIR exists");
	}

	for (const auto mode : appearance_modes) {
		capture_one_appearance(mode);
		capture_the_home_view(mode);
	}

	// The reachability walks do not vary with the palette, so they run once per
	// scale rather than once per appearance.
	walk_the_accounts_page();
	walk_home_and_its_collapsed_panel();
	walk_the_profiles_sheet();
	walk_the_appearance_page();

	std::cout << "surface-capture-course: states=" << StatesCaptured
			  << " written=" << CapturesWritten << " scale=" << scale_slug().toStdString()
			  << '\n';
	std::cout << "surface-capture-course-tests:pass\n";
	return EXIT_SUCCESS;
}
