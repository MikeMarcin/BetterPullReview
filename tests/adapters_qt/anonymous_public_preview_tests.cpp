#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/usage_ring.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;
	using application::public_preview_budget;
	using application::public_preview_mode;
	using application::public_preview_observation;
	using application::public_preview_presentation;
	using application::public_preview_refusal_presentation;
	using application::public_preview_refusal_reason;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr std::uint64_t fixture_now = 1'000'000U;
	// Three hours and four minutes past the fixture instant, so the rendered
	// reset is the exact sentence the approved decision names.
	constexpr std::uint64_t fixture_reset = fixture_now + 11'040U;

	[[nodiscard]] auto profiles()
		-> std::vector<application::github_profile_option>
	{
		return {
			application::github_profile_option{
				.Profile   = core::host_profile_id{ 1U },
				.Label     = "Work GitHub",
				.Hostname  = "github.com",
				.Product   = application::github_profile_product::github_com,
				.Transport = application::github_profile_transport::native_https,
			},
		};
	}

	[[nodiscard]] auto make_window() -> std::unique_ptr<qt::workbench_window>
	{
		auto window = std::make_unique<qt::workbench_window>(
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			profiles(),
			application::github_intent_dispatch{ },
			nullptr);
		window->resize(1'280, 820);
		window->show();
		QApplication::processEvents();
		return window;
	}

	// Exactly what a host publishes under the signed-out mode: a spent
	// fraction of the shared hourly core budget, the instant it returns, and
	// the three sentences the mode cannot get past.
	[[nodiscard]] auto signed_out_preview() -> public_preview_presentation
	{
		public_preview_presentation preview{ };
		preview.Mode   = public_preview_mode::signed_out;
		preview.Budget = public_preview_budget{
			.Limit            = 60U,
			.Remaining        = 41U,
			.ResetUnixSeconds = fixture_reset,
		};
		preview.NowUnixSeconds = fixture_now;
		return preview;
	}

	void activate_ring(qt::usage_ring* const aRing)
	{
		aRing->setFocus();
		QKeyEvent press{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(aRing, &press);
		QApplication::processEvents();
	}

	// Counts the device-authorization gestures that left the window. The ring
	// performs no effect of its own, so this is the only evidence that
	// activating it reaches the mounted "Get a code".
	struct device_sink
	{
		std::size_t Begins = 0;

		void adopt(qt::workbench_window& aWindow)
		{
			aWindow.set_device_authorization_dispatch(
				[this](application::device_authorization_gesture aGesture) {
					if (std::holds_alternative<
							core::begin_device_authorization_intent>(aGesture)) {
						++Begins;
					}
				});
		}
	};

	// [[BPR-0771]]. RFC-BPR-0032: the mode is named in the window title and
	// on the account row of the card whose host is being read. The host picker
	// entry and the access-mode label that once named it are gone.
	void test_the_mode_is_named_in_the_title_and_on_its_card(const double aScale)
	{
		static_cast<void>(aScale);
		auto window = make_window();
		window->publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Status   = core::github_status_delta{ .Status = core::github_status::session_ready },
				.Identity = core::github_identity_delta{ .Profile = core::host_profile_id{ 1U } },
			},
		});
		window->publish(signed_out_preview());
		QApplication::processEvents();

		const auto mode = qt::public_preview_mode_text();
		expect(
			window->windowTitle().contains(mode),
			"the window title names the mode");
		auto* const card = window->server_card_for(core::host_profile_id{ 1U });
		expect(card != nullptr && card->presentation().Current && card->presentation().ReadingPublic,
			   "the card of the host being read is the one reading");
		expect(card->host_label()->text() == QStringLiteral("github.com"),
			   "and it carries the profile's host");
		expect(card->state_label()->text() == QStringLiteral("Signed out")
				   && card->login_label()->text() == QStringLiteral("No account")
				   && card->consequence_label()->text()
					   == QStringLiteral("Reading public pull requests without an account."),
			   "its account row says it reads public pull requests without an account");
		expect(card->sign_in_button()->isVisibleTo(card) && !card->sign_out_button()->isVisibleTo(card),
			   "and offers Sign in rather than Sign out");
		expect(window->findChild<QWidget*>(QStringLiteral("githubAccessMode")) == nullptr
				   && window->findChild<QWidget*>(QStringLiteral("githubProfileSelector")) == nullptr,
			   "no access-mode label or host picker entry names the mode a second time");
		expect(
			window->session_panel()->public_preview_signed_out(),
			"the session presentation is in the mode rather than blank");
		expect(
			window->active_account_text().find("Reading without signing in")
					== std::string::npos
				&& !card->signed_in(),
			"the mode is not presented as an authenticated account");
	}

	// [[BPR-0771]], [[BPR-0772]]. RFC-BPR-0032 took "Read without signing in"
	// off Accounts & hosts: reading without an account is the Home bar's. What
	// the page still owes is that nothing on it enters the mode, and that each
	// card's ways in reach exactly the host profile they name, as the core
	// admits. GitHub.com's native HTTPS and GitHub CLI profiles are one card.
	//
	// [[BPR-1023]]. Since [[BPR-0915]] GitHub.com's Connect chooses its native
	// profile and begins device sign-in in the shared sign-in dialog instead of
	// dispatching an activation. The course follows that entry path, keeps the
	// GitHub CLI offer's exact activation wherever it is admitted, and ends on
	// the decision recorded on BPR-1023: every way in on a card, device sign-in
	// included, obeys the host-activation command it is shown by.
	void test_accounts_offer_no_public_preview_activation()
	{
		auto options = profiles();
		options.push_back(application::github_profile_option{
			.Profile   = core::host_profile_id{ 7U },
			.Label     = "Enterprise native",
			.Hostname  = "ghe.example.invalid",
			.Product   = application::github_profile_product::github_enterprise_server,
			.Transport = application::github_profile_transport::native_https,
		});
		options.push_back(application::github_profile_option{
			.Profile   = core::host_profile_id{ 9U },
			.Label     = "GitHub CLI",
			.Hostname  = "github.com",
			.Product   = application::github_profile_product::github_com,
			.Transport = application::github_profile_transport::github_cli,
		});
		std::vector<application::github_intent> intents;
		qt::workbench_window                    window{
			qt::workbench_intent_dispatch{ },
			qt::workbench_change_dispatch{ },
			qt::workbench_drawer_width_dispatch{ },
			options,
			[&intents](application::github_intent aIntent) {
				intents.push_back(std::move(aIntent));
			},
			nullptr,
		};
		device_sink device;
		device.adopt(window);
		window.resize(1'280, 820);
		window.show();
		window.open_settings(qt::settings_page::accounts_and_hosts);
		QApplication::processEvents();
		auto* const page = window.settings()->page_widget(qt::settings_page::accounts_and_hosts);
		expect(page != nullptr
				   && page->findChild<QWidget*>(QStringLiteral("publicPreviewButton")) == nullptr,
			   "Accounts and hosts carries no public-preview button");
		const auto pageWidgets = page->findChildren<QWidget*>();
		expect(std::ranges::none_of(pageWidgets,
									[](const QWidget* const aWidget) {
										return aWidget->accessibleName()
											== QStringLiteral("Read without signing in");
									}),
			   "and nothing on it is named for reading without signing in");
		// [[BPR-0772]]. One card per configured server: GitHub.com's native
		// HTTPS and GitHub CLI profiles are ways in to one card.
		const auto  cards      = window.server_cards();
		auto* const github     = window.server_card_for(core::host_profile_id{ 1U });
		auto* const enterprise = window.server_card_for(core::host_profile_id{ 7U });
		expect(cards.size() == 2U && github != nullptr && enterprise != nullptr
				   && cards.front() == github && cards.back() == enterprise
				   && window.server_card_for(core::host_profile_id{ 9U }) == github,
			   "one card per configured server, in the order the servers were first configured");
		expect(github->cli_offer_button() != nullptr && enterprise->cli_offer_button() == nullptr
				   && github->cli_offer_button()->text() == QStringLiteral("GitHub CLI"),
			   "only GitHub.com offers GitHub CLI, and a label that says nothing more is offered as GitHub CLI");
		// Exactly one intent since the last gesture, and it activates exactly
		// this host profile with an account, never in public preview.
		const auto activated = [&intents](const core::host_profile_id aProfile) {
			const auto* const activation = intents.size() == 1U
				? std::get_if<core::activate_host_profile_intent>(&intents.front())
				: nullptr;
			return activation != nullptr && activation->Profile == aProfile
				&& activation->Mode == core::github_access_mode::authenticated;
		};

		// The first configured profile is chosen before any gesture.
		expect(github->presentation().Current && !enterprise->presentation().Current
				   && enterprise->connect_button()->isVisibleTo(enterprise) && intents.empty(),
			   "before any gesture GitHub.com is chosen, and the enterprise card offers Connect");
		enterprise->connect_button()->click();
		expect(activated(core::host_profile_id{ 7U }),
			   "the enterprise card's Connect activates exactly its host");
		intents.clear();

		// The shared sign-in dialog each way in opens holds the cards in their
		// setup presentation; closing it returns them to Settings.
		const auto closeSharedSignIn = [&window] {
			if (window.sign_in_dialog() != nullptr && window.sign_in_dialog()->isVisible()) {
				window.sign_in_dialog()->reject();
			}
			QApplication::processEvents();
		};
		closeSharedSignIn();

		expect(!github->presentation().Current && github->profile() == core::host_profile_id{ 1U }
				   && github->connect_button()->isVisibleTo(github),
			   "GitHub.com, no longer chosen, offers Connect for its native HTTPS profile");
		github->connect_button()->click();
		QApplication::processEvents();
		expect(intents.empty() && device.Begins == 1U
				   && window.session_panel()->selected_profile() == std::optional{ core::host_profile_id{ 1U } }
				   && github->presentation().Current && github->profile() == core::host_profile_id{ 1U },
			   "GitHub.com's Connect, while it is not chosen, chooses exactly its native HTTPS profile and begins device sign-in, activating nothing");
		expect(window.sign_in_dialog() != nullptr && window.sign_in_dialog()->isVisible()
				   && window.sign_in_dialog()->isAncestorOf(github) && github->isVisibleTo(window.sign_in_dialog())
				   && !enterprise->isVisibleTo(window.sign_in_dialog()),
			   "and it does so in the shared sign-in dialog, for GitHub.com alone");
		closeSharedSignIn();

		expect(github->presentation().Current && github->profile() == core::host_profile_id{ 1U }
				   && !github->connect_button()->isVisibleTo(github)
				   && github->sign_in_button()->isVisibleTo(github)
				   && github->cli_offer_button()->isVisibleTo(github),
			   "chosen with native HTTPS presented, GitHub.com offers Sign in rather than Connect, beside the GitHub CLI offer");
		github->cli_offer_button()->click();
		expect(activated(core::host_profile_id{ 9U }),
			   "the GitHub CLI offer activates exactly the GitHub CLI profile");
		intents.clear();
		closeSharedSignIn();
		expect(github->presentation().Current && github->profile() == core::host_profile_id{ 9U }
				   && !github->cli_offer_button()->isVisibleTo(github)
				   && github->connect_button()->isVisibleTo(github)
				   && github->sign_in_button()->isVisibleTo(github)
				   && !github->sign_in_button()->isDefault(),
			   "with GitHub CLI presented the offer gives way to Connect as its retry, and Sign in stays without being filled");
		expect(device.Begins == 1U, "the GitHub CLI offer and the enterprise Connect emit no device sign-in; GitHub.com's Connect began the one");

		// Choosing the enterprise host again leaves GitHub.com offering both
		// Connect and GitHub CLI, so the refusal below has both to take away.
		enterprise->connect_button()->click();
		expect(activated(core::host_profile_id{ 7U }),
			   "choosing the enterprise host again activates exactly it");
		intents.clear();
		closeSharedSignIn();
		expect(!github->presentation().Current && github->connect_button()->isVisibleTo(github)
				   && github->cli_offer_button()->isVisibleTo(github),
			   "GitHub.com, no longer chosen, offers Connect and GitHub CLI before the refusal");
		// The refusal arrives the way the core sends one: a command projection
		// on its own, through the window.
		window.publish(application::github_presentation_update{
			.Transition = application::github_transition_projection{
				.Commands = {
					{
						.Id             = core::github_command_id::activate_host_profile,
						.Enabled        = false,
						.DisabledReason = core::github_reason_code::invalid_profile,
					},
				},
			},
		});
		QApplication::processEvents();
		expect(!github->connect_button()->isVisibleTo(github)
				   && !github->cli_offer_button()->isVisibleTo(github)
				   && !enterprise->connect_button()->isVisibleTo(enterprise),
			   "the refused activation command hides every card's Connect and the GitHub CLI offer");
		// Reached anyway, each is refused before it chooses, opens or begins
		// anything: GitHub.com's Connect begins no device sign-in either.
		github->connect_button()->click();
		github->cli_offer_button()->click();
		enterprise->connect_button()->click();
		QApplication::processEvents();
		expect(intents.empty() && device.Begins == 1U
				   && (window.sign_in_dialog() == nullptr || !window.sign_in_dialog()->isVisible())
				   && enterprise->presentation().Current,
			   "every way in on a card, device sign-in included, obeys the host activation command state");
	}

	void test_the_ring_reports_the_budget_and_the_way_out(const double aScale)
	{
		auto window = make_window();
		window->publish(signed_out_preview());
		QApplication::processEvents();

		auto* const ring = window->usage_indicator();
		auto* const card = window->server_card_for(core::host_profile_id{ 1U });
		expect(ring != nullptr && card != nullptr && card->usage_indicator() == ring,
			   "the ring is composed on the card of the host being read");
		expect(ring->has_budget(), "and it has an observed budget to draw");
		expect(
			ring->consumed_fraction() > 0.31 && ring->consumed_fraction() < 0.32,
			"the arc fills with the consumed fraction, not the remaining one");

		QStringList expected;
		expected.append(
			QStringLiteral("32% of the hourly limit · Resets in 3 hr 4 min"));
		expected.append(QStringLiteral("GitHub.com only."));
		expected.append(QStringLiteral("No review threads."));
		expected.append(QStringLiteral("No triage or publishing."));
		expected.append(qt::public_preview_sign_in_text());
		expect(
			ring->tooltip_text() == expected.join(QStringLiteral("\n")),
			"the tooltip reads the consumed fraction, the reset, what the "
			"preview cannot do, and the way out of it");
		expect(
			ring->toolTip() == ring->tooltip_text(),
			"and the widget carries that text as its tooltip");
		expect(
			ring->accessibleName() == QStringLiteral("Public preview hourly limit"),
			"the ring has an accessible name");
		expect(
			ring->accessibleDescription() == ring->tooltip_text(),
			"and an accessible description carrying the same facts");
		expect(
			ring->size().width() > 0 && ring->size().height() > 0 && aScale > 0.0,
			"the ring occupies real space at the scale under test");
	}

	void test_the_reset_wording_degrades_by_the_supplied_instant(
		const double aScale)
	{
		static_cast<void>(aScale);
		auto window            = make_window();
		auto preview           = signed_out_preview();
		preview.NowUnixSeconds = fixture_reset - 2'400U;
		window->publish(preview);
		QApplication::processEvents();
		expect(
			window->usage_indicator()->tooltip_text().startsWith(
				QStringLiteral("32% of the hourly limit · Resets in 40 min")),
			"under an hour the hours are dropped rather than printed as zero");

		preview.NowUnixSeconds = fixture_reset + 1U;
		window->publish(preview);
		QApplication::processEvents();
		expect(
			window->usage_indicator()->tooltip_text().startsWith(
				QStringLiteral("32% of the hourly limit · Resets now")),
			"a reset that has passed is stated rather than counted backwards");
	}

	void test_sign_in_is_offered_from_the_ring_and_from_every_refusal(
		const double aScale)
	{
		static_cast<void>(aScale);
		auto        window = make_window();
		device_sink sink;
		sink.adopt(*window);
		window->publish(signed_out_preview());
		QApplication::processEvents();

		activate_ring(window->usage_indicator());
		expect(
			sink.Begins == 1U,
			"activating the ring reaches the mounted device sign-in");

		auto preview    = signed_out_preview();
		preview.Refusal = public_preview_refusal_presentation{
			.Reason = public_preview_refusal_reason::budget_exhausted,
			.Budget = public_preview_budget{
				.Limit            = 60U,
				.Remaining        = 0U,
				.ResetUnixSeconds = fixture_reset,
			},
		};
		window->publish(preview);
		QApplication::processEvents();

		const auto refusal = window->public_preview_refusal_text();
		expect(
			refusal.contains(QStringLiteral("60")),
			"a spent budget is refused by naming its limit");
		expect(
			refusal.contains(QStringLiteral("Resets in 3 hr 4 min")),
			"and the instant it comes back");
		expect(
			refusal.contains(qt::public_preview_sign_in_text()),
			"and every refusal offers the way out of the mode");
		activate_ring(window->usage_indicator());
		expect(
			sink.Begins == 2U,
			"the offer is live while the refusal is on screen");

		for (const auto reason : { public_preview_refusal_reason::private_or_missing,
								   public_preview_refusal_reason::
									   enterprise_host_unavailable,
								   public_preview_refusal_reason::
									   review_threads_unavailable,
								   public_preview_refusal_reason::
									   viewer_identity_unavailable,
								   public_preview_refusal_reason::
									   triage_or_publishing_refused,
								   public_preview_refusal_reason::
									   mode_unavailable }) {
			auto each    = signed_out_preview();
			each.Refusal = public_preview_refusal_presentation{
				.Reason = reason,
				.Budget = std::nullopt,
			};
			window->publish(each);
			QApplication::processEvents();
			expect(
				window->public_preview_refusal_text().contains(
					qt::public_preview_sign_in_text()),
				"every typed refusal offers sign-in, not only the budget one");
		}
	}

	void test_what_the_mode_cannot_do_is_disabled_and_explained(
		const double aScale)
	{
		static_cast<void>(aScale);
		auto window = make_window();
		window->publish(signed_out_preview());
		QApplication::processEvents();

		const auto refused = [](QAction* const aAction) {
			return aAction != nullptr && !aAction->isEnabled()
				&& aAction->isVisible()
				&& aAction->toolTip().contains(
					QStringLiteral("unavailable in public preview"));
		};
		expect(refused(window->seen_action()), "viewed state is refused");
		expect(refused(window->read_action()), "reading progress is refused");
		expect(refused(window->accept_action()), "triage accept is refused");
		expect(refused(window->exclude_action()), "triage exclude is refused");
		expect(
			!window->show_review_threads_action()
				&& !window->findChild<QAction*>(QStringLiteral("showReviewThreadsAction")),
			"the removed focus-only review-thread action is absent in public preview too");
		auto* const role = window->queue_role_filter();
		expect(
			role != nullptr && !role->isEnabled()
				&& role->toolTip().contains(
					QStringLiteral("unavailable in public preview")),
			"the queue role is refused");
		expect(
			window->review_editor() != nullptr
				&& !window->review_editor()->isEnabled(),
			"publishing a review is refused");
	}

	void test_review_comments_are_read_only_or_named_unobservable(
		const double aScale)
	{
		static_cast<void>(aScale);
		auto window           = make_window();
		auto absent           = signed_out_preview();
		absent.ReviewComments = public_preview_observation::unavailable;
		window->publish(absent);
		QApplication::processEvents();

		auto* const availability = window->findChild<QLabel*>(
			QStringLiteral("reviewThreadAvailability"));
		expect(
			availability != nullptr
				&& availability->text()
					== QStringLiteral("Review comments were not observable."),
			"an unobserved conversation says so rather than showing nothing");
		expect(
			window->conversation_model()->rowCount() == 0,
			"and no empty conversation is presented as an answer");

		auto observed           = signed_out_preview();
		observed.ReviewComments = public_preview_observation::observed;
		observed.ReviewCommentGroups.push_back(
			application::public_preview_review_comment_group{
				.Root = application::public_preview_review_comment{
					.Id          = "1",
					.AuthorLogin = "octocat",
					.Body        = "This branch drops the guard.",
					.Path        = "src/core/model.cpp",
					.Line        = 42U,
				},
				.Replies = {
					application::public_preview_review_comment{
						.Id          = "2",
						.AuthorLogin = "hubot",
						.Body        = "Agreed.",
						.Path        = "src/core/model.cpp",
						.Line        = 42U,
					},
				},
			});
		window->publish(observed);
		QApplication::processEvents();
		expect(window->conversation_model()->rowCount() == 0,
			   "profile-wide preview metadata cannot lend a sibling window's conversation");
		// The window's accepted read publication owns its own complete bodies.
		window->publish(application::github_presentation_update{
			.PullRequest = application::github_pull_request_projection{
				.Profile       = core::host_profile_id{ 1U },
				.Host          = core::git_host_id{ 1U },
				.Repository    = core::repository_id{ 1U },
				.PullRequest   = core::pull_request_id{ 1U },
				.Conversations = {
					{ .Id = "public-preview-comment:1", .Kind = application::github_conversation_kind::review_comment, .AuthorLogin = "octocat", .Body = observed.ReviewCommentGroups[0].Root.Body, .Path = "src/core/model.cpp", .Line = 42U },
					{ .Id = "public-preview-comment:2", .ParentId = "public-preview-comment:1", .Kind = application::github_conversation_kind::review_comment, .AuthorLogin = "reviewer", .Body = observed.ReviewCommentGroups[0].Replies[0].Body, .Path = "src/core/model.cpp", .Line = 42U },
				},
				.PublicPreview           = true,
				.ReviewCommentsAvailable = true,
			},
		});
		QApplication::processEvents();
		expect(
			window->conversation_model()->rowCount() == 1,
			"the observed conversation reaches the reviewer");
		expect(
			availability->text().contains(QStringLiteral("read-only")),
			"named read-only rather than offered as a thread to resolve");
		expect(
			!availability->text().contains(QStringLiteral("Resolved")),
			"and no resolution is inferred for it");
	}

	void test_the_ring_is_a_keyboard_stop(const double aScale)
	{
		static_cast<void>(aScale);
		auto window = make_window();
		window->publish(signed_out_preview());
		QApplication::processEvents();
		auto* const ring = window->usage_indicator();
		expect(
			ring->focusPolicy() != Qt::NoFocus,
			"the ring can take keyboard focus");
		// [[BPR-0771]]. The ring is on the card of the host being read, on
		// Settings > Accounts and hosts, so it is walked in that card's reading
		// order rather than in the workbench's declared order: after the
		// account row's way in, on the Rate limit row, before Details.
		auto* const page = window->settings()->page_widget(qt::settings_page::accounts_and_hosts);
		auto* const card = window->server_card_for(core::host_profile_id{ 1U });
		expect(page != nullptr && card != nullptr && page->isAncestorOf(ring) && card->isAncestorOf(ring)
				   && !window->focus_order().contains(ring),
			   "and it is walked on its card on the Accounts and hosts page, not in the workbench's order");
		const auto firstReached = [](QWidget* const aFrom, QWidget* const aOne, QWidget* const aOther) -> QWidget* {
			for (auto* candidate = aFrom->nextInFocusChain(); candidate != nullptr && candidate != aFrom;
				 candidate       = candidate->nextInFocusChain()) {
				if (candidate == aOne || candidate == aOther) {
					return candidate;
				}
			}
			return nullptr;
		};
		expect(firstReached(card->sign_in_button(), ring, card->details_button()) == ring,
			   "reading order and focus order agree: the limit is read before its details");
	}

	void test_an_unpublished_window_shows_nothing_of_the_mode(
		const double aScale)
	{
		static_cast<void>(aScale);
		auto        window = make_window();
		device_sink sink;
		sink.adopt(*window);

		expect(
			!window->public_preview_signed_out(),
			"a window nobody published to is not in the signed-out mode");
		expect(
			window->windowTitle() == QStringLiteral("BetterPullReview"),
			"the title says nothing about a mode nothing described");
		auto* const card = window->server_card_for(core::host_profile_id{ 1U });
		expect(
			card != nullptr
				&& card->host_label()->text() == QStringLiteral("github.com")
				&& card->state_label()->text() == QStringLiteral("Signed out")
				&& card->login_label()->text() == QStringLiteral("No account")
				&& !card->presentation().ReadingPublic
				&& card->consequence_label()->text()
					!= QStringLiteral("Reading public pull requests without an account."),
			"and the host's card still names its host without claiming to read public pull requests");
		expect(
			!window->session_panel()->public_preview_signed_out(),
			"the session presentation is not in the mode");
		expect(
			window->public_preview_refusal_text().isEmpty(),
			"there is no refusal to state");
		auto* const ring = window->usage_indicator();
		expect(!ring->has_budget(), "and no budget to draw");
		activate_ring(ring);
		expect(
			sink.Begins == 0U,
			"a ring no presentation described offers no way into sign-in");
		expect(
			window->seen_action()->toolTip().contains(
				QStringLiteral("unavailable in public preview"))
				== false,
			"and nothing is refused for a mode nobody is in");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };

	const auto scaleText = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
	expect(!scaleText.isEmpty(), "the course names the scale it runs at");
	bool       parsed = false;
	const auto scale  = scaleText.toDouble(&parsed);
	expect(parsed, "expected Qt scale is numeric");
	if (argc == 2 && std::string_view{ argv[1] } == "--conversation-only") {
		test_review_comments_are_read_only_or_named_unobservable(scale);
		std::cout << "public-preview-conversation-tests:pass\n";
		return EXIT_SUCCESS;
	}

	test_the_mode_is_named_in_the_title_and_on_its_card(scale);
	test_accounts_offer_no_public_preview_activation();
	test_the_ring_reports_the_budget_and_the_way_out(scale);
	test_the_reset_wording_degrades_by_the_supplied_instant(scale);
	test_sign_in_is_offered_from_the_ring_and_from_every_refusal(scale);
	test_what_the_mode_cannot_do_is_disabled_and_explained(scale);
	test_review_comments_are_read_only_or_named_unobservable(scale);
	test_the_ring_is_a_keyboard_stop(scale);
	test_an_unpublished_window_shows_nothing_of_the_mode(scale);

	std::cout << "anonymous-public-preview-tests:pass\n";
	return EXIT_SUCCESS;
}
