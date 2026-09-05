#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/usage_ring.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

	void test_the_mode_is_named_in_all_three_places(const double aScale)
	{
		static_cast<void>(aScale);
		auto window = make_window();
		window->publish(signed_out_preview());
		QApplication::processEvents();

		const auto mode = qt::public_preview_mode_text();
		expect(
			window->windowTitle().contains(mode),
			"the window title names the mode");
		auto* const selector = window->profile_selector();
		expect(selector != nullptr, "the host picker is composed");
		const auto entry = selector->itemText(selector->currentIndex());
		expect(entry.contains(mode), "the host picker entry names the mode");
		expect(
			entry.contains(QStringLiteral("github.com")),
			"and carries the profile's host beside it");
		auto* const label = window->session_panel()->access_mode_label();
		expect(label != nullptr && label->text() == mode,
			   "the review header names the mode exactly");
		expect(
			window->session_panel()->public_preview_signed_out(),
			"and the mode strip is on screen rather than blank");
		expect(
			window->active_account_text().find("Public preview")
				== std::string::npos,
			"the mode is not presented as an authenticated account");
	}

	void test_the_ring_reports_the_budget_and_the_way_out(const double aScale)
	{
		auto window = make_window();
		window->publish(signed_out_preview());
		QApplication::processEvents();

		auto* const ring = window->usage_indicator();
		expect(ring != nullptr, "the ring is composed beside the host picker");
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
			refused(window->show_review_threads_action()),
			"review-thread resolution is refused");
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
		expect(
			window->focus_order().contains(ring),
			"and the declared focus order walks it");
		expect(
			window->focus_order().indexOf(ring)
				< window->focus_order().indexOf(window->profile_selector()),
			"reading order and focus order agree: the mode is read first");
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
		auto* const selector = window->profile_selector();
		expect(
			selector->itemText(selector->currentIndex())
				== QStringLiteral("Work GitHub"),
			"and the host picker still names the profile");
		expect(
			!window->session_panel()->public_preview_signed_out(),
			"the mode strip is absent rather than blank");
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

	test_the_mode_is_named_in_all_three_places(scale);
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
