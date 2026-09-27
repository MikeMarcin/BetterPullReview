#include "bpr/application/manual_executor.hpp"
#include "bpr/application/workbench_application_host.hpp"
#include <cstdlib>
#include <iostream>
#include <map>

namespace {
	using namespace bpr;
	void expect(const bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	struct state
	{
		application::workbench_profile_preferences                          Preferences;
		std::map<core::bpr_window_id, application::workbench_window_layout> Windows;
		bool                                                                Fail   = false;
		std::size_t                                                         Writes = 0;
	};
	struct fake_settings_store final : application::workbench_settings_port
	{
		fake_settings_store(state& aState, core::bpr_window_id aWindow)
			: State{ aState }
			, Window{ aWindow }
		{ }
		auto window() const noexcept -> core::bpr_window_id override { return Window; }
		auto profile_scoped() const noexcept -> bool override { return true; }
		auto load(std::stop_token) -> application::workbench_settings_result<application::workbench_layout_settings> override
		{
			const auto found = State.Windows.find(Window);
			return application::combined_layout(found == State.Windows.end() ? State.Preferences.NewWindowDefaults : found->second, State.Preferences.Behaviour);
		}
		auto store(const application::workbench_layout_settings& aLayout, std::stop_token aStop) -> application::workbench_settings_result<void> override
		{
			if (aStop.stop_requested()) {
				return std::unexpected(application::workbench_settings_error{ });
			}
			State.Windows[Window] = application::window_layout(aLayout);
			return { };
		}
		auto load_preferences(std::stop_token) -> application::workbench_settings_result<application::workbench_profile_preferences> override { return State.Preferences; }
		auto store_preferences(const application::workbench_profile_preferences& aPreferences, std::stop_token aStop) -> application::workbench_settings_result<void> override
		{
			if (State.Fail || aStop.stop_requested()) {
				return std::unexpected(application::workbench_settings_error{ });
			}
			++State.Writes;
			State.Preferences = aPreferences;
			return { };
		}
		state&              State;
		core::bpr_window_id Window;
	};
	struct observations final : application::workbench_observation_port
	{
		auto assemble(const application::workbench_observation_request& aRequest, std::stop_token)
			-> application::workbench_file_observation override { return { .Context = aRequest.Context, .Authority = aRequest.Authority }; }
	};
	struct projection final : application::workbench_projection_port
	{
		void                                           fail(const application::workbench_failure_publication&) override { ++Failures; }
		std::size_t                                    Failures = 0;
		void                                           publish(const application::workbench_projection& aProjection) override { Published.push_back(aProjection); }
		std::vector<application::workbench_projection> Published;
	};
	void test_refused_subscription_has_no_effect_authority()
	{
		for (int refusal = 0; refusal < 3; ++refusal) {
			state                                   durable;
			fake_settings_store                     port{ durable, { } };
			application::manual_executor            effects, decision;
			application::workbench_preferences_host owner{ port, effects, decision, core::bpr_profile_id{ 1 } };
			owner.adopt_loaded(durable.Preferences);
			if (refusal == 1) {
				for (std::size_t index = 0; index < application::maximum_workbench_window_layouts; ++index) {
					expect(owner.subscribe(core::bpr_profile_id{ 1 }, [](const auto&) { }) != 0, "owner consumer capacity fills");
				}
			}
			if (refusal == 2) {
				owner.shutdown();
			}
			observations                            observer;
			projection                              projected;
			application::workbench_application_host rejected{ { }, observer, port, effects, decision, projected, core::application_session_id{ 1 }, { }, nullptr, nullptr, &owner, core::bpr_profile_id{ refusal == 0 ? 2U : 1U } };
			rejected.restore();
			auto changed         = application::review_behaviour_settings{ };
			changed.OpenModality = application::review_open_modality::read;
			rejected.apply_behaviour(changed);
			rejected.resize(core::workbench_drawer::files, 555);
			effects.drain();
			decision.drain();
			expect(durable.Writes == 0 && durable.Windows.empty() && effects.pending() == 0,
				   "foreign full or closing owner cannot grant rejected workbench write authority");
			expect(projected.Failures != 0, "refused preference attachment reports scoped failure");
			rejected.shutdown();
			owner.shutdown();
		}
	}

	struct inline_executor final : application::effect_executor, application::decision_executor
	{
		void post(std::function<void()> aTask) override { aTask(); }
	};
	void test_reentrant_preference_fanout()
	{
		state                                          durable;
		fake_settings_store                            port{ durable, { } };
		inline_executor                                executor;
		application::workbench_preferences_host        owner{ port, executor, executor };
		bool                                           changed = false;
		std::vector<application::review_open_modality> received;
		const auto                                     first  = owner.subscribe(core::default_bpr_profile_id, [&](const auto&) {
			if (!changed) {
				changed           = true;
				auto next         = application::review_behaviour_settings{ };
				next.OpenModality = application::review_open_modality::read;
				expect(owner.apply_behaviour(next), "reentrant subscriber durably changes preferences");
			}
		});
		const auto                                     second = owner.subscribe(core::default_bpr_profile_id, [&](const auto& aPreferences) {
			received.push_back(aPreferences.Behaviour.OpenModality);
		});
		expect(first != 0 && second != 0, "reentrant test attaches two consumers");
		owner.restore();
		expect(received == std::vector{ application::review_open_modality::read }, "older fanout cannot follow reentrant accepted preferences");
		owner.shutdown();
		expect(owner.is_quiescent(), "inline reentrant effects finish exactly once");
	}

}
int main()
{
	test_refused_subscription_has_no_effect_authority();
	test_reentrant_preference_fanout();
	{
		state                                   firstState, otherState;
		fake_settings_store                     port{ firstState, { } }, otherPort{ otherState, { } };
		application::manual_executor            effects, decision;
		application::workbench_preferences_host owner{ port, effects, decision }, other{ otherPort, effects, decision, core::bpr_profile_id{ 2 } };
		owner.restore();
		other.restore();
		effects.drain();
		decision.drain();
		expect(owner.apply_writing_hook(core::writing_hook::problem, "keep precise"), "hook override admitted");
		expect(owner.apply_writing_hook(core::writing_hook::solution, "keep qualified"), "pending hook edits coalesce");
		effects.drain();
		decision.drain();
		effects.drain();
		decision.drain();
		expect(firstState.Preferences.WritingHooks.Revision == 3 && firstState.Preferences.WritingHooks.Overrides[0] == "keep precise" && firstState.Preferences.WritingHooks.Overrides[1] == "keep qualified", "distinct hook overrides survive coalesced store");
		expect(otherState.Preferences.WritingHooks == core::writing_hook_settings{ }, "sibling profile unaffected");
		expect(!owner.apply_writing_hook(core::writing_hook::outline, std::string(8193, 'x')) && !owner.apply_writing_hook(core::writing_hook::comment, std::string(1, static_cast<char>(0xff))), "oversize and malformed instructions refused");
		expect(owner.apply_writing_hook(core::writing_hook::problem, { }), "reset selected hook");
		effects.drain();
		decision.drain();
		expect(firstState.Preferences.WritingHooks.Overrides[0].empty() && firstState.Preferences.WritingHooks.Overrides[1] == "keep qualified", "reset preserves other hooks");
		owner.shutdown();
		other.shutdown();
		expect(owner.is_quiescent() && other.is_quiescent(), "settings drained");
	}

	state                                   durable;
	fake_settings_store                     preferencesPort{ durable, { } }, firstPort{ durable, core::bpr_window_id{ 7 } }, secondPort{ durable, core::bpr_window_id{ 9 } };
	application::manual_executor            effects, decision;
	application::workbench_preferences_host preferences{ preferencesPort, effects, decision };
	observations                            observer;
	projection                              firstProjection, secondProjection;
	application::workbench_application_host first{ { }, observer, firstPort, effects, decision, firstProjection, core::application_session_id{ 1 }, { }, nullptr, nullptr, &preferences };
	application::workbench_application_host second{ { }, observer, secondPort, effects, decision, secondProjection, core::application_session_id{ 2 }, { }, nullptr, nullptr, &preferences };
	const auto                              settle = [&] { for (int count=0;count<4;++count) { effects.drain(); decision.drain(); } };
	first.restore();
	second.restore();
	settle();
	expect(durable.Windows.empty(), "missing windows use defaults without persisting a row");
	first.resize(core::workbench_drawer::files, 444);
	second.resize(core::workbench_drawer::files, 777);
	settle();
	expect(durable.Windows.size() == 2 && durable.Windows.at(core::bpr_window_id{ 7 }).FilesWidth == 444
			   && durable.Windows.at(core::bpr_window_id{ 9 }).FilesWidth == 777,
		   "two window gestures preserve independent persisted geometry");
	const auto windows  = durable.Windows;
	auto       chosen   = application::review_behaviour_settings{ };
	chosen.OpenModality = application::review_open_modality::read;
	first.apply_behaviour(chosen);
	expect(first.layout().Behaviour != chosen && second.layout().Behaviour != chosen, "behaviour changes remain unaccepted before durable completion");
	settle();
	expect(first.layout().Behaviour == chosen && second.layout().Behaviour == chosen && durable.Writes == 1, "one accepted profile preference write fans out to both windows");
	expect(durable.Windows == windows && first.layout().FilesWidth == 444 && second.layout().FilesWidth == 777,
		   "shared preference fanout cannot replace either window layout");
	durable.Fail           = true;
	auto refused           = chosen;
	refused.CommentsFilter = application::review_comments_filter::unresolved;
	second.apply_behaviour(refused);
	settle();
	expect(first.layout().Behaviour == chosen && second.layout().Behaviour == chosen && durable.Writes == 1, "failed durable preferences never become accepted state");
	first.shutdown();
	durable.Fail = false;
	second.apply_behaviour(refused);
	settle();
	expect(second.layout().Behaviour == refused && first.layout().Behaviour == chosen, "closed consumer is detached while sibling preferences remain live");
	auto defaults       = durable.Preferences.NewWindowDefaults;
	defaults.FilesWidth = 555;
	defaults.OnePane    = core::workbench_one_pane::enabled;
	expect(preferences.apply_defaults(defaults), "a defaults edit begins durable work");
	auto latest         = refused;
	latest.OpenModality = application::review_open_modality::overview;
	expect(preferences.apply_behaviour(latest), "a behaviour edit coalesces behind defaults");
	defaults.FilesWidth = 666;
	expect(preferences.apply_defaults(defaults), "latest defaults coalesce without losing behaviour");
	settle();
	expect(durable.Preferences.NewWindowDefaults.FilesWidth == 666 && durable.Preferences.NewWindowDefaults.OnePane == core::workbench_one_pane::enabled && durable.Preferences.Behaviour == latest,
		   "bounded pending edits retain both preference domains");
	expect(second.layout().FilesWidth == 777 && second.layout().OnePane == core::workbench_one_pane::disabled && durable.Windows == windows, "default changes never overwrite admitted window geometry");
	expect(preferences.subscribe(core::bpr_profile_id{ 99 }, [](const auto&) { }) == 0, "foreign profile subscriptions are refused");
	expect(preferences.apply_defaults(application::workbench_window_layout{ }), "cancellable write starts");
	preferences.cancel();
	settle();
	expect(durable.Preferences.NewWindowDefaults.FilesWidth == 666 && !preferences.accepted(), "cancelled writes cannot publish and require fresh durable observation");
	preferences.restore();
	settle();
	expect(preferences.accepted() && preferences.accepted()->NewWindowDefaults.FilesWidth == 666, "cancelled owner reloads accepted durable state");
	second.shutdown();
	preferences.shutdown();
	settle();
	expect(first.is_quiescent() && second.is_quiescent() && preferences.is_quiescent(), "all consumers and profile effects drain independently");
	return EXIT_SUCCESS;
}
