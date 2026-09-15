#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/event_loop_device_timer.hpp"

#include <QAbstractButton>
#include <QAccessible>
#include <QAccessibleEvent>
#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;
	using application::device_authorization_gesture;
	using application::device_authorization_handoff;
	using application::device_authorization_presentation;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	// The address a real GitHub.com device grant returns, already validated
	// twice before it could reach a panel. It is a fixture string here and
	// nothing in the adapter ever constructs one.
	[[nodiscard]] auto verification_address() -> std::string
	{
		return std::string{ "https://" } + "github.com/login/device";
	}

	// Exactly what a host publishes: the core is asked which commands are legal
	// in the stage, and the answer travels in the projection. A fixture that
	// hand-wrote the flags would be testing the fixture's opinion of the rule
	// rather than the panel's obedience to it.
	[[nodiscard]] auto commands(const core::device_authorization_stage aStage)
		-> std::vector<core::device_authorization_command_projection>
	{
		return core::device_authorization_commands(
			core::device_authorization_state{ .Stage = aStage });
	}

	[[nodiscard]] auto awaiting(
		const device_authorization_handoff aHandoff
		= device_authorization_handoff::not_attempted,
		const std::uint32_t aRemaining = 900U) -> device_authorization_presentation
	{
		return {
			.Stage                    = core::device_authorization_stage::awaiting_authorization,
			.UserCode                 = "WDJB-MJHT",
			.VerificationUri          = verification_address(),
			.RemainingValiditySeconds = aRemaining,
			.Handoff                  = aHandoff,
			.Commands                 = commands(
				core::device_authorization_stage::awaiting_authorization),
		};
	}

	[[nodiscard]] auto settled(const core::device_authorization_stage aStage)
		-> device_authorization_presentation
	{
		return {
			.Stage                    = aStage,
			.UserCode                 = { },
			.VerificationUri          = { },
			.RemainingValiditySeconds = 0U,
			.Handoff                  = device_authorization_handoff::not_attempted,
			.Commands                 = commands(aStage),
		};
	}

	// -- accessibility capture ---------------------------------------------

	std::vector<QString>                Announcements;
	QAccessible::AnnouncementPoliteness LastPoliteness
		= QAccessible::AnnouncementPoliteness::Assertive;
	QAccessible::UpdateHandler Previous = nullptr;

	void capture_accessibility_update(QAccessibleEvent* const aEvent)
	{
		if (aEvent == nullptr) {
			return;
		}
		if (aEvent->type() == QAccessible::Announcement) {
			const auto* const announcement
				= static_cast<const QAccessibleAnnouncementEvent*>(aEvent);
			Announcements.push_back(announcement->message());
			LastPoliteness = announcement->politeness();
		}
		if (Previous != nullptr) {
			Previous(aEvent);
		}
	}

	// -- gesture recording -------------------------------------------------

	struct recorded_gestures
	{
		std::size_t Begins  = 0U;
		std::size_t Cancels = 0U;
		std::size_t Opens   = 0U;

		[[nodiscard]] auto total() const noexcept -> std::size_t
		{
			return Begins + Cancels + Opens;
		}
	};

	[[nodiscard]] auto make_panel(
		recorded_gestures&     aRecord,
		core::host_profile_id& aObservedProfile) -> qt::device_authorization_panel*
	{
		return new qt::device_authorization_panel{
			core::host_profile_id{ 7U },
			[&aRecord, &aObservedProfile](device_authorization_gesture aGesture) {
				std::visit(
					[&aRecord, &aObservedProfile](const auto& aValue) {
						using type = std::decay_t<decltype(aValue)>;
						if constexpr (
							std::is_same_v<type, core::begin_device_authorization_intent>) {
							++aRecord.Begins;
							aObservedProfile = aValue.Profile;
						} else if constexpr (
							std::is_same_v<type, core::cancel_device_authorization_intent>) {
							++aRecord.Cancels;
						} else {
							++aRecord.Opens;
						}
					},
					aGesture);
			},
		};
	}

	// -- courses -----------------------------------------------------------

	void test_the_panel_dispatches_typed_gestures_and_performs_no_effect(
		const double aScale)
	{
		static_cast<void>(aScale);
		recorded_gestures record;
		auto              observed = core::host_profile_id{ };
		auto* const       panel    = make_panel(record, observed);
		panel->show();
		QApplication::processEvents();

		expect(
			panel->begin_button()->isEnabled(),
			"an idle panel offers the device path");
		expect(
			!panel->cancel_button()->isEnabled(),
			"and nothing to cancel");
		expect(
			!panel->open_button()->isEnabled()
				&& !panel->copy_button()->isEnabled(),
			"with no address to open and no code to copy");

		panel->begin_button()->click();
		expect(record.Begins == 1U, "the begin gesture leaves through dispatch");
		expect(
			observed == core::host_profile_id{ 7U },
			"naming the profile the panel was composed for");
		expect(
			record.total() == 1U,
			"and the panel dispatched nothing else on its own");

		panel->apply(awaiting());
		expect(
			panel->user_code_field()->text() == QStringLiteral("WDJB-MJHT"),
			"the user code is presented");
		expect(
			panel->address_field()->text()
				== QString::fromStdString(verification_address()),
			"and so is the address it goes to");
		expect(
			panel->user_code_field()->isReadOnly()
				&& panel->address_field()->isReadOnly(),
			"both are read-only: the panel presents, it does not collect");
		expect(
			panel->open_button()->isEnabled(),
			"a live address can be handed to a browser");
		expect(
			panel->cancel_button()->isEnabled()
				&& !panel->begin_button()->isEnabled(),
			"and a live flow is cancellable rather than restartable");

		panel->open_button()->click();
		expect(record.Opens == 1U, "the open gesture leaves through dispatch");
		panel->cancel_button()->click();
		expect(record.Cancels == 1U, "so does cancellation");
		expect(
			record.total() == 3U,
			"three gestures, and the panel reached no provider and no vault");

		panel->deleteLater();
		QApplication::processEvents();
	}

	void test_the_panel_applies_incrementally(const double aScale)
	{
		static_cast<void>(aScale);
		recorded_gestures record;
		auto              observed = core::host_profile_id{ };
		auto* const       panel    = make_panel(record, observed);
		panel->show();
		QApplication::processEvents();

		panel->apply(awaiting());
		const auto first = panel->statistics();
		expect(first.Applications == 1U && first.Rebuilds == 1U, "the first "
																 "projection is applied");

		panel->apply(awaiting());
		panel->apply(awaiting());
		const auto repeated = panel->statistics();
		expect(
			repeated.Applications == 3U,
			"every republication is seen");
		expect(
			repeated.Rebuilds == 1U,
			"but an unchanged projection rebuilds nothing");

		panel->apply(awaiting(device_authorization_handoff::not_attempted, 880U));
		expect(
			panel->statistics().Rebuilds == 2U,
			"a changed field rebuilds exactly once");
		expect(
			panel->user_code_field()->text() == QStringLiteral("WDJB-MJHT"),
			"and leaves the fields that did not change alone");

		panel->deleteLater();
		QApplication::processEvents();
	}

	void test_focus_order_follows_reading_order(const double aScale)
	{
		static_cast<void>(aScale);
		recorded_gestures record;
		auto              observed = core::host_profile_id{ };
		auto* const       panel    = make_panel(record, observed);
		panel->apply(awaiting());
		panel->show();
		QApplication::processEvents();

		panel->user_code_field()->setFocus();
		QApplication::processEvents();
		expect(
			panel->user_code_field()->hasFocus(),
			"the code field takes real keyboard focus");

		expect(
			panel->user_code_field()->nextInFocusChain() != nullptr,
			"the code field has a successor in the focus chain");
		panel->address_field()->setFocus();
		QApplication::processEvents();
		expect(
			panel->address_field()->hasFocus(),
			"the address field takes real keyboard focus");
		panel->open_button()->setFocus();
		QApplication::processEvents();
		expect(
			panel->open_button()->hasFocus(),
			"and so does the open action");

		panel->deleteLater();
		QApplication::processEvents();
	}

	void test_one_polite_combined_status_is_announced(const double aScale)
	{
		static_cast<void>(aScale);
		recorded_gestures record;
		auto              observed = core::host_profile_id{ };
		auto* const       panel    = make_panel(record, observed);
		panel->show();
		QApplication::processEvents();

		Announcements.clear();
		panel->apply(awaiting());
		expect(
			Announcements.size() == 1U,
			"one presentation change announces exactly once");
		expect(
			LastPoliteness == QAccessible::AnnouncementPoliteness::Polite,
			"politely");
		expect(
			Announcements.back().contains(QStringLiteral("WDJB-MJHT")),
			"and the announcement carries the code the reviewer must type");

		panel->apply(awaiting());
		expect(
			Announcements.size() == 1U,
			"an unchanged republication announces nothing");

		panel->apply(awaiting(device_authorization_handoff::failed));
		expect(
			Announcements.size() == 2U,
			"a handoff failure announces once");
		expect(
			Announcements.back().contains(QStringLiteral("browser"))
				&& Announcements.back().contains(QStringLiteral("WDJB-MJHT")),
			"combining the stage and the handoff into one utterance");

		panel->deleteLater();
		QApplication::processEvents();
	}

	void test_a_failing_opener_is_not_an_authorization_failure(const double aScale)
	{
		static_cast<void>(aScale);
		recorded_gestures record;
		auto              observed = core::host_profile_id{ };
		auto* const       panel    = make_panel(record, observed);
		panel->show();
		QApplication::processEvents();

		panel->apply(awaiting(device_authorization_handoff::failed));
		expect(
			panel->user_code_field()->text() == QStringLiteral("WDJB-MJHT"),
			"a failed open leaves the code on screen");
		expect(
			panel->address_field()->text()
				== QString::fromStdString(verification_address()),
			"and the address with it");
		expect(
			panel->cancel_button()->isEnabled(),
			"the flow is still live and still cancellable");
		expect(
			panel->open_button()->isEnabled(),
			"and the reviewer may try the browser again");
		expect(
			!panel->handoff_label()->text().isEmpty(),
			"the failure is stated rather than swallowed");
		expect(
			panel->handoff_label()->text().contains(QStringLiteral("any device")),
			"and it says the flow can be finished elsewhere");

		// The whole claim: authorization arrives from somewhere else.
		panel->apply(settled(core::device_authorization_stage::authorized));
		expect(
			panel->user_code_field()->text().isEmpty()
				&& panel->address_field()->text().isEmpty(),
			"and a terminal erases the code and the address together");
		expect(
			panel->handoff_label()->text().isEmpty(),
			"and the stale handoff result with them");

		panel->deleteLater();
		QApplication::processEvents();
	}

	void test_an_off_authority_refusal_is_distinct_from_a_failure(
		const double aScale)
	{
		static_cast<void>(aScale);
		recorded_gestures record;
		auto              observed = core::host_profile_id{ };
		auto* const       panel    = make_panel(record, observed);
		panel->show();
		QApplication::processEvents();

		panel->apply(awaiting(device_authorization_handoff::refused));
		const auto refused = panel->handoff_label()->text();
		expect(
			!refused.isEmpty(),
			"an off-authority refusal is presented");
		expect(
			refused.contains(QStringLiteral("not this profile's sign-in address")),
			"and says why nothing opened");
		expect(
			panel->cancel_button()->isEnabled(),
			"the flow survives the refusal");

		panel->apply(awaiting(device_authorization_handoff::failed));
		expect(
			panel->handoff_label()->text() != refused,
			"a launch failure reads differently from a refused address");

		panel->apply(awaiting(device_authorization_handoff::unavailable));
		expect(
			panel->handoff_label()->text() != refused
				&& panel->handoff_label()->text().contains(
					QStringLiteral("cannot open a browser")),
			"and a build with no browser at all reads differently again");

		panel->deleteLater();
		QApplication::processEvents();
	}

	void test_the_code_can_be_copied_and_the_address_cannot_be_copied_implicitly(
		const double aScale)
	{
		static_cast<void>(aScale);
		recorded_gestures record;
		auto              observed = core::host_profile_id{ };
		auto* const       panel    = make_panel(record, observed);
		panel->show();
		QApplication::processEvents();

		auto* const clipboard = QApplication::clipboard();
		expect(clipboard != nullptr, "the offscreen platform offers a clipboard");
		clipboard->setText(QStringLiteral("untouched"));

		expect(!panel->copy_user_code(), "an idle panel has no code to copy");
		expect(
			clipboard->text() == QStringLiteral("untouched"),
			"and copies nothing");

		panel->apply(awaiting());
		expect(panel->copy_user_code(), "a live code is copyable");
		expect(
			clipboard->text() == QStringLiteral("WDJB-MJHT"),
			"the clipboard holds exactly the user code");
		expect(
			!clipboard->text().contains(QStringLiteral("github")),
			"and never the address, which is read rather than pasted");

		panel->deleteLater();
		QApplication::processEvents();
	}

	void test_a_terminal_flow_offers_a_fresh_code(const double aScale)
	{
		static_cast<void>(aScale);
		recorded_gestures record;
		auto              observed = core::host_profile_id{ };
		auto* const       panel    = make_panel(record, observed);
		panel->show();
		QApplication::processEvents();

		panel->apply(awaiting());
		panel->apply(settled(core::device_authorization_stage::denied));
		expect(
			panel->begin_button()->isEnabled(),
			"a denied flow can be started again");
		expect(
			!panel->cancel_button()->isEnabled(),
			"and has nothing left to cancel");
		expect(
			panel->begin_button()->text() == QStringLiteral("Sign in again"),
			"the retry says it is a retry");
		expect(
			!panel->open_button()->isEnabled()
				&& !panel->copy_button()->isEnabled(),
			"with no stale address to open and no stale code to copy");

		panel->begin_button()->click();
		expect(record.Begins == 1U, "and retrying is the same typed gesture");

		panel->deleteLater();
		QApplication::processEvents();
	}

	// The panel renders command legality; it no longer decides it. The stage
	// that proves the difference is `cancelling`: it is neither `requested` nor
	// `awaiting_authorization`, so a panel reasoning from the stage offers "Get
	// a code" during it, and the core refuses that command with
	// `cancellation_in_progress`. Reading the projection is what makes the two
	// agree.
	void test_the_panel_obeys_the_core_command_legality(const double aScale)
	{
		static_cast<void>(aScale);
		recorded_gestures record;
		auto              observed = core::host_profile_id{ };
		auto* const       panel    = make_panel(record, observed);
		panel->show();
		QApplication::processEvents();

		panel->apply(awaiting());
		panel->apply(settled(core::device_authorization_stage::cancelling));
		expect(
			!panel->begin_button()->isEnabled(),
			"a flow being cancelled offers no new one, because the core forbids "
			"one");
		expect(
			!panel->cancel_button()->isEnabled(),
			"and cancelling again is refused for the same reason");

		// And the projection is the only thing that says so: the same stage
		// with the core's idle answer attached enables what that answer allows.
		auto disagreeing     = settled(core::device_authorization_stage::cancelling);
		disagreeing.Commands = commands(core::device_authorization_stage::idle);
		panel->apply(disagreeing);
		expect(
			panel->begin_button()->isEnabled(),
			"the panel follows the published commands rather than the stage");

		// A projection that carries no command at all disables both rather than
		// falling back to a rule of its own.
		auto silent     = settled(core::device_authorization_stage::idle);
		silent.Commands = { };
		panel->apply(silent);
		expect(
			!panel->begin_button()->isEnabled()
				&& !panel->cancel_button()->isEnabled(),
			"an absent command is a disabled command");

		panel->deleteLater();
		QApplication::processEvents();
	}

	// What this adapter can prove about manual entry, and no more. Whether the
	// token prompt still enrolls a profile is a composition claim that
	// [[BPR-0282]]'s public-path course owns; what belongs here is that the
	// device path takes nothing away from it and cannot be mistaken for it.
	void test_the_device_path_does_not_displace_manual_token_entry(
		const double aScale)
	{
		static_cast<void>(aScale);
		recorded_gestures record;
		auto              observed = core::host_profile_id{ };
		auto* const       panel    = make_panel(record, observed);
		panel->apply(awaiting());
		panel->show();
		QApplication::processEvents();

		// The token prompt is a separate modal owned by composition and reached
		// through a different gesture. The panel neither builds one nor names
		// one, so it cannot consume the Connect path that opens it.
		expect(
			panel->findChild<QWidget*>(
				QStringLiteral("bpr-sensitive-text-prompt"))
				== nullptr,
			"the device panel embeds no credential prompt");
		expect(
			panel->findChild<QLineEdit*>(
				QStringLiteral("bpr-sensitive-text-editor"))
				== nullptr,
			"and no credential editor");

		const auto editors = panel->findChildren<QLineEdit*>();
		expect(!editors.isEmpty(), "the panel does present fields");
		for (const auto* const editor : editors) {
			expect(
				editor->isReadOnly(),
				"but every one of them is read-only: the device path collects "
				"nothing from the reviewer");
			expect(
				editor->echoMode() == QLineEdit::Normal,
				"and none is masked, because a device code is read aloud rather "
				"than kept secret");
		}

		panel->deleteLater();
		QApplication::processEvents();
	}

	// -- the production poll timer ----------------------------------------

	// Runs the event loop until the predicate holds or the budget expires, so
	// a timer that never fires fails the assertion rather than hanging.
	void pump_until(
		const std::function<bool()>& aReady,
		const int                    aBudgetMilliseconds)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aReady() && elapsed.elapsed() < aBudgetMilliseconds) {
			QCoreApplication::processEvents(
				QEventLoop::AllEvents,
				20);
		}
	}

	void test_the_production_timer_arms_fires_once_and_clears(
		const double aScale)
	{
		static_cast<void>(aScale);
		qt::event_loop_device_timer timer;
		expect(!timer.armed(), "a fresh timer is idle");

		auto fires = 0;
		expect(
			timer.schedule_after(1U, [&fires] { ++fires; }),
			"a one-second delay arms");
		expect(timer.armed(), "and reports itself armed");
		expect(fires == 0, "without firing inline");

		pump_until([&fires] { return fires > 0; }, 5000);
		expect(fires == 1, "the delay fires exactly once");
		expect(!timer.armed(), "and the timer is idle again");

		// A single-shot timer that re-armed itself would poll forever.
		pump_until([] { return false; }, 1200);
		expect(fires == 1, "and does not fire again on its own");
	}

	void test_the_production_timer_cancels_without_firing(const double aScale)
	{
		static_cast<void>(aScale);
		qt::event_loop_device_timer timer;
		auto                        fires = 0;
		expect(
			timer.schedule_after(1U, [&fires] { ++fires; }),
			"the delay arms");
		timer.cancel();
		expect(!timer.armed(), "cancellation disarms immediately");

		pump_until([] { return false; }, 1500);
		expect(
			fires == 0,
			"a cancelled delay never fires, so a cancelled flow stops polling "
			"at once rather than at the next interval");
	}

	void test_arming_replaces_rather_than_accumulates(const double aScale)
	{
		static_cast<void>(aScale);
		qt::event_loop_device_timer timer;
		auto                        first  = 0;
		auto                        second = 0;
		expect(timer.schedule_after(1U, [&first] { ++first; }), "first arms");
		expect(
			timer.schedule_after(1U, [&second] { ++second; }),
			"and the second replaces it");

		pump_until([&second] { return second > 0; }, 5000);
		expect(second == 1, "the replacement fires");
		expect(
			first == 0,
			"and the delay it replaced never does, so two callbacks cannot "
			"race for the same poll");
	}

	void test_the_production_timer_refuses_what_it_cannot_honour(
		const double aScale)
	{
		static_cast<void>(aScale);
		qt::event_loop_device_timer timer;
		expect(
			!timer.schedule_after(1U, { }),
			"an absent expiry is refused rather than armed and dropped");
		expect(!timer.armed(), "and arms nothing");
		expect(
			!timer.schedule_after(
				std::numeric_limits<std::uint32_t>::max(),
				[] { }),
			"a delay Qt cannot represent is refused, which the host reads as a "
			"timer that could not be armed");
		expect(!timer.armed(), "and arms nothing either");
	}

	void test_the_panel_renders_at_the_requested_scale(const double aScale)
	{
		recorded_gestures record;
		auto              observed = core::host_profile_id{ };
		auto* const       panel    = make_panel(record, observed);
		panel->apply(awaiting());
		panel->show();
		QApplication::processEvents();

		expect(
			std::abs(panel->devicePixelRatioF() - aScale) < 0.02,
			"the panel reports the requested device-pixel ratio");
		const auto render = panel->grab();
		expect(!render.isNull(), "and renders to a nonempty raster at it");
		expect(
			panel->sizeHint().width() > 0 && panel->sizeHint().height() > 0,
			"with a usable size hint");
		expect(
			panel->user_code_field()->isVisible()
				&& panel->address_field()->isVisible()
				&& panel->open_button()->isVisible()
				&& panel->cancel_button()->isVisible(),
			"and every element the reviewer needs is laid out, not clipped away");

		panel->deleteLater();
		QApplication::processEvents();
	}

	// [[BPR-0773]]. RFC-BPR-0032 A2: the panel is drawn as a server card's
	// account row. A command the core does not allow is hidden rather than
	// shown disabled, a terminal keeps its sentence and offers one Sign in
	// again, and where the device flow is unavailable or turned off the token
	// way in the composing window adopts sits in the same row.
	void test_the_row_hides_what_it_cannot_do_and_offers_the_token_way_in(
		const double aScale)
	{
		static_cast<void>(aScale);
		recorded_gestures record;
		auto              observed = core::host_profile_id{ };
		auto* const       panel    = make_panel(record, observed);
		panel->show();
		QApplication::processEvents();

		// Every button a reviewer can see can be pressed.
		const auto noDisabledButton = [panel] {
			const auto buttons = panel->findChildren<QAbstractButton*>();
			return std::ranges::none_of(buttons, [panel](const QAbstractButton* const aButton) {
				return aButton->isVisibleTo(panel) && !aButton->isEnabled();
			});
		};
		const auto visible = [panel](const QWidget* const aWidget) {
			return aWidget->isVisibleTo(panel);
		};

		expect(noDisabledButton() && visible(panel->begin_button())
				   && panel->begin_button()->text() == QStringLiteral("Sign in")
				   && !visible(panel->cancel_button()) && !visible(panel->open_button())
				   && !visible(panel->copy_button()) && !visible(panel->token_button())
				   && !visible(panel->status_label()),
			   "idle, the row offers Sign in and shows no button it cannot press");

		panel->apply(awaiting());
		QApplication::processEvents();
		expect(noDisabledButton()
				   && visible(panel->address_sentence_label())
				   && panel->address_sentence_label()->text() == QStringLiteral("Enter this code at")
				   && visible(panel->address_field()) && visible(panel->user_code_field())
				   && visible(panel->copy_button()) && visible(panel->open_button())
				   && visible(panel->validity_label()) && visible(panel->cancel_button())
				   && visible(panel->status_label()) && !panel->status_label()->text().isEmpty()
				   && panel->open_button()->isDefault()
				   && !visible(panel->begin_button()) && !visible(panel->token_button()),
			   "awaiting, the row shows the address, the code with Copy, Open browser filled, the validity, one status sentence and Cancel, and hides the Sign in the core forbids");
		expect(panel->copy_button()->accessibleName() == QStringLiteral("Copy code")
				   && panel->copy_button()->toolTip() == QStringLiteral("Copy code"),
			   "the copy glyph is named Copy code and says so in its tooltip");
		auto* const clipboard = QApplication::clipboard();
		expect(clipboard != nullptr, "the offscreen platform offers a clipboard");
		clipboard->setText(QStringLiteral("untouched"));
		panel->copy_button()->click();
		expect(clipboard->text() == QStringLiteral("WDJB-MJHT")
				   && panel->copy_button()->text() == QStringLiteral("Copied"),
			   "copying puts exactly the code on the clipboard and says Copied in place");

		panel->apply(settled(core::device_authorization_stage::denied));
		QApplication::processEvents();
		expect(noDisabledButton() && visible(panel->begin_button())
				   && panel->begin_button()->text() == QStringLiteral("Sign in again")
				   && visible(panel->status_label())
				   && panel->status_label()->text() == QStringLiteral("Sign-in was denied.")
				   && !visible(panel->cancel_button()) && !visible(panel->open_button())
				   && !visible(panel->copy_button()) && !visible(panel->token_button()),
			   "a terminal keeps its sentence and offers one Sign in again, with no Cancel, Open browser or Copy");

		panel->apply(settled(core::device_authorization_stage::unavailable));
		QApplication::processEvents();
		expect(noDisabledButton() && !visible(panel->token_button()) && visible(panel->begin_button()),
			   "with no token way in adopted, an unavailable device flow offers none");

		auto tokens = 0;
		panel->set_manual_token_dispatch([&tokens] { ++tokens; });
		QApplication::processEvents();
		expect(noDisabledButton() && visible(panel->token_button())
				   && panel->token_button()->text() == QStringLiteral("Sign in with a token")
				   && visible(panel->begin_button())
				   && panel->status_label()->text() == QStringLiteral("Device sign-in is unavailable here."),
			   "where the device flow is unavailable, the row offers Sign in with a token beside Sign in again");
		panel->token_button()->click();
		expect(tokens == 1 && record.total() == 0U,
			   "the token way in calls the adopted dispatch once and dispatches no device gesture");

		panel->apply(settled(core::device_authorization_stage::disabled));
		QApplication::processEvents();
		expect(noDisabledButton() && visible(panel->token_button())
				   && panel->status_label()->text() == QStringLiteral("Device sign-in is turned off for this account."),
			   "a device flow turned off for the account offers the token way in as well");
		panel->apply(settled(core::device_authorization_stage::expired));
		QApplication::processEvents();
		expect(noDisabledButton() && !visible(panel->token_button()) && visible(panel->begin_button()),
			   "an expired code is retried with a new code, not with a token");

		panel->deleteLater();
		QApplication::processEvents();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	Previous = QAccessible::installUpdateHandler(capture_accessibility_update);

	const auto scaleText = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
	expect(!scaleText.isEmpty(), "the course names the scale it runs at");
	bool       parsed = false;
	const auto scale  = scaleText.toDouble(&parsed);
	expect(parsed, "expected Qt scale is numeric");

	test_the_panel_dispatches_typed_gestures_and_performs_no_effect(scale);
	test_the_panel_applies_incrementally(scale);
	test_focus_order_follows_reading_order(scale);
	test_one_polite_combined_status_is_announced(scale);
	test_a_failing_opener_is_not_an_authorization_failure(scale);
	test_an_off_authority_refusal_is_distinct_from_a_failure(scale);
	test_the_code_can_be_copied_and_the_address_cannot_be_copied_implicitly(scale);
	test_a_terminal_flow_offers_a_fresh_code(scale);
	test_the_panel_obeys_the_core_command_legality(scale);
	test_the_device_path_does_not_displace_manual_token_entry(scale);
	test_the_panel_renders_at_the_requested_scale(scale);
	test_the_row_hides_what_it_cannot_do_and_offers_the_token_way_in(scale);
	test_the_production_timer_arms_fires_once_and_clears(scale);
	test_the_production_timer_cancels_without_firing(scale);
	test_arming_replaces_rather_than_accumulates(scale);
	test_the_production_timer_refuses_what_it_cannot_honour(scale);

	static_cast<void>(QAccessible::installUpdateHandler(Previous));
	std::cout << "device-authorization-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
