#include "bpr/application/account_setup_host.hpp"

#include <utility>

namespace bpr::application {
	account_setup_host::account_setup_host(account_setup_effect_port& aEffects, decision_executor& aLane, account_setup_sink aSink, const bool aGithubCliOffered)
		: Effects{ aEffects }
		, Lane{ aLane }
		, Sink{ std::move(aSink) }
		, Self{ std::make_shared<account_setup_host*>(this) }
	{
		Projection.GithubCliOffered = aGithubCliOffered;
	}

	account_setup_host::~account_setup_host() { shutdown(); }

	void account_setup_host::shutdown()
	{
		Stopped = true;
		Pending.clear();
		if (Self) {
			*Self = nullptr;
		}
	}

	void account_setup_host::dispatch(core::account_setup_event aEvent)
	{
		if (Stopped) {
			return;
		}
		// The CLI way is not offered where gh cannot be inspected, so a
		// request for it is refused here rather than started and failed.
		const auto* const begin = std::get_if<core::begin_account_setup>(&aEvent);
		const auto* const choose = std::get_if<core::choose_account_setup_way>(&aEvent);
		if (!Projection.GithubCliOffered
			&& ((begin != nullptr && begin->Way == core::account_setup_way::github_cli) || (choose != nullptr && choose->Way == core::account_setup_way::github_cli))) {
			return;
		}
		Pending.push_back(std::move(aEvent));
		apply_pending();
	}

	void account_setup_host::observe_device(const device_authorization_presentation& aPresentation)
	{
		if (Stopped) {
			return;
		}
		const auto stageChanged = aPresentation.Stage != Projection.Device.Stage;
		const auto shownChanged = aPresentation != Projection.Device;
		Projection.Device       = aPresentation;
		if (stageChanged) {
			Pending.emplace_back(core::account_setup_device_observed{ .Stage = aPresentation.Stage });
			apply_pending();
		} else if (shownChanged && !Applying && active()) {
			// The code's remaining time and the browser handoff are shown,
			// without a transition.
			publish();
		}
	}

	void account_setup_host::observe_session(core::account_setup_session_observed aSession)
	{
		if (Stopped) {
			return;
		}
		Pending.emplace_back(std::move(aSession));
		apply_pending();
	}

	void account_setup_host::open_verification_address()
	{
		if (!Stopped && Projection.State.Phase == core::account_setup_phase::awaiting_browser) {
			Effects.open_verification_address();
		}
	}

	void account_setup_host::complete_github_cli(core::github_cli_inspected aAnswer) { marshal(std::move(aAnswer)); }

	void account_setup_host::complete_enterprise_server(core::enterprise_server_prepared aAnswer) { marshal(std::move(aAnswer)); }

	void account_setup_host::marshal(core::account_setup_event aAnswer)
	{
		auto weak = std::weak_ptr<account_setup_host*>{ Self };
		Lane.post([weak, answer = std::move(aAnswer)]() mutable {
			const auto self = weak.lock();
			if (!self || *self == nullptr) {
				return;
			}
			(*self)->dispatch(std::move(answer));
		});
	}

	void account_setup_host::observe_signed_in(const bool aSignedIn)
	{
		if (Stopped || Projection.SignedIn == aSignedIn) {
			return;
		}
		Projection.SignedIn = aSignedIn;
		if (!Applying && active()) {
			publish();
		}
	}

	void account_setup_host::apply_pending()
	{
		if (Applying) {
			return;
		}
		Applying = true;
		while (!Pending.empty() && !Stopped) {
			const auto event = std::move(Pending.front());
			Pending.pop_front();
			auto result = core::transition_account_setup(Projection.State, event);
			if (result.Refusal != core::account_setup_refusal::none) {
				continue;
			}
			Projection.State = std::move(result.State);
			Projection.Step  = core::account_setup_step_of(Projection.State);
			publish();
			// Effects run after the state that asked for them is published, so a
			// port that answers at once answers the published state.
			for (const auto& effect : result.Effects) {
				if (Stopped) {
					break;
				}
				perform(effect);
			}
		}
		Applying = false;
	}

	void account_setup_host::publish()
	{
		++Projection.Generation;
		if (Sink) {
			Sink(Projection);
		}
	}

	void account_setup_host::perform(const core::account_setup_effect& aEffect)
	{
		switch (aEffect.Kind) {
		case core::account_setup_effect_kind::prepare_enterprise_server: Effects.prepare_enterprise_server(aEffect.Request, aEffect.Hostname); break;
		case core::account_setup_effect_kind::inspect_github_cli:        Effects.inspect_github_cli(aEffect.Request, aEffect.Hostname); break;
		case core::account_setup_effect_kind::enroll_token:
			if (aEffect.Profile.has_value()) {
				Effects.enroll_token(*aEffect.Profile);
			}
			break;
		case core::account_setup_effect_kind::begin_browser_sign_in:  Effects.begin_browser_sign_in(); break;
		case core::account_setup_effect_kind::cancel_browser_sign_in: Effects.cancel_browser_sign_in(); break;
		case core::account_setup_effect_kind::connect_github_cli:
			if (aEffect.Profile.has_value()) {
				Effects.connect_github_cli(*aEffect.Profile);
			}
			break;
		}
	}
} // namespace bpr::application
