#include "bpr/qt/device_authorization_panel.hpp"

#include <QAccessible>
#include <QAccessibleEvent>
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QString>

#include <cmath>
#include <cstdlib>
#include <iostream>
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
			panel->begin_button()->text() == QStringLiteral("Get a new code"),
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
	test_the_device_path_does_not_displace_manual_token_entry(scale);
	test_the_panel_renders_at_the_requested_scale(scale);

	static_cast<void>(QAccessible::installUpdateHandler(Previous));
	std::cout << "device-authorization-panel-tests:pass\n";
	return EXIT_SUCCESS;
}
