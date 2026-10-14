#include "bpr/application/manual_executor.hpp"
#include "bpr/application/workbench_application_host.hpp"
#include <array>
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
			changed.OpenModality = application::review_open_modality::review;
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
				next.OpenModality = application::review_open_modality::review;
				expect(owner.apply_behaviour(next), "reentrant subscriber durably changes preferences");
			}
		});
		const auto                                     second = owner.subscribe(core::default_bpr_profile_id, [&](const auto& aPreferences) {
			received.push_back(aPreferences.Behaviour.OpenModality);
		});
		expect(first != 0 && second != 0, "reentrant test attaches two consumers");
		owner.restore();
		expect(received == std::vector{ application::review_open_modality::review }, "older fanout cannot follow reentrant accepted preferences");
		owner.shutdown();
		expect(owner.is_quiescent(), "inline reentrant effects finish exactly once");
	}

	/// \brief [[BPR-1301]]. A window host that owns its preferences adopts
	/// the stored Claude process wrapper with the rest of the record, so a
	/// later preference write keeps it. An agent-free build never reads it,
	/// and this is what keeps it stored there.
	void test_owned_preferences_keep_the_claude_wrapper()
	{
		struct legacy_port final : application::workbench_settings_port
		{
			auto load(std::stop_token) -> application::workbench_settings_result<application::workbench_layout_settings> override { return Stored; }
			auto store(const application::workbench_layout_settings& aLayout, std::stop_token) -> application::workbench_settings_result<void> override
			{
				Stored = aLayout;
				++Writes;
				return { };
			}
			application::workbench_layout_settings Stored;
			std::size_t                            Writes = 0;
		};
		legacy_port port;
		port.Stored.ClaudeWrapper = "/opt/tools/claude-gateway";
		application::manual_executor            effects, decision;
		observations                            observer;
		projection                              projected;
		application::workbench_application_host host{ { }, observer, port, effects, decision, projected, core::application_session_id{ 1 } };
		host.restore();
		effects.drain();
		decision.drain();
		auto changed     = application::review_behaviour_settings{ };
		changed.MarkRead = application::review_mark_read_behaviour::when_fully_seen;
		host.apply_behaviour(changed);
		for (auto round = 0; round < 4; ++round) {
			effects.drain();
			decision.drain();
		}
		expect(port.Writes != 0 && port.Stored.Behaviour.MarkRead == application::review_mark_read_behaviour::when_fully_seen, "the behaviour change is written");
		expect(port.Stored.ClaudeWrapper == "/opt/tools/claude-gateway", "the adopted record keeps the wrapper through another write");
		host.shutdown();
		effects.drain();
		decision.drain();
	}

	/// \brief [[BPR-1088]]. The label helper, the Overview's starting count and
	/// the label columns follow the profile's existing preference authority:
	/// one durable write fans out to every window of the profile, another
	/// profile is untouched, a failed or cancelled write is never accepted, a
	/// record naming a label twice is refused, and what a load found wrong with
	/// the stored columns reaches the windows until the reviewer's next edit.
	void test_label_preferences_follow_the_profile_authority()
	{
		state durable, otherDurable;
		auto  stored                                     = application::review_behaviour_settings{ };
		stored.OpenModality                              = application::review_open_modality::review;
		stored.LabelColumns                              = *core::make_feedback_label_columns(std::array{ core::feedback_label::issue });
		stored.LabelColumnsRecovery                      = application::review_label_columns_recovery{ .Unknown = 1U, .Repeated = 0U };
		durable.Preferences.Behaviour                    = stored;
		durable.Preferences.NewWindowDefaults.FilesWidth = 400U;
		fake_settings_store                     preferencesPort{ durable, { } }, firstPort{ durable, core::bpr_window_id{ 7 } }, secondPort{ durable, core::bpr_window_id{ 9 } };
		fake_settings_store                     otherPort{ otherDurable, { } }, otherWindowPort{ otherDurable, core::bpr_window_id{ 7 } };
		application::manual_executor            effects, decision;
		application::workbench_preferences_host preferences{ preferencesPort, effects, decision, core::bpr_profile_id{ 1 } };
		application::workbench_preferences_host otherPreferences{ otherPort, effects, decision, core::bpr_profile_id{ 2 } };
		observations                            observer;
		projection                              firstProjection, secondProjection, otherProjection;
		application::workbench_application_host first{ { }, observer, firstPort, effects, decision, firstProjection, core::application_session_id{ 1 }, { }, nullptr, nullptr, &preferences, core::bpr_profile_id{ 1 } };
		application::workbench_application_host second{ { }, observer, secondPort, effects, decision, secondProjection, core::application_session_id{ 2 }, { }, nullptr, nullptr, &preferences, core::bpr_profile_id{ 1 } };
		application::workbench_application_host other{ { }, observer, otherWindowPort, effects, decision, otherProjection, core::application_session_id{ 3 }, { }, nullptr, nullptr, &otherPreferences, core::bpr_profile_id{ 2 } };
		const auto                              settle        = [&] { for (int count = 0; count < 6; ++count) { effects.drain(); decision.drain(); } };
		const auto                              lastBehaviour = [](const projection& aProjection) {
			for (auto it = aProjection.Published.rbegin(); it != aProjection.Published.rend(); ++it) {
				if (it->Behaviour) {
					return *it->Behaviour;
				}
			}
			return application::review_behaviour_settings{ };
		};
		preferences.restore();
		otherPreferences.restore();
		first.restore();
		second.restore();
		other.restore();
		settle();
		expect(first.layout().Behaviour == stored && second.layout().Behaviour == stored && lastBehaviour(firstProjection).LabelColumnsRecovery == stored.LabelColumnsRecovery,
			   "what the load found wrong with the stored columns reaches every window of the profile");
		expect(other.layout().Behaviour == application::review_behaviour_settings{ }, "another profile starts from its own defaults");

		auto chosen = stored;
		chosen.LabelColumnsRecovery.reset();
		chosen.CenterRibbonWidth             = 56U;
		chosen.LabelHelper                   = application::review_label_helper::on;
		chosen.OverviewCounts                = application::review_overview_counts::all;
		chosen.LabelColumns                  = *core::make_feedback_label_columns(std::array{ core::feedback_label::question, core::feedback_label::issue, core::feedback_label::quibble });
		auto withObservation                 = chosen;
		withObservation.LabelColumnsRecovery = stored.LabelColumnsRecovery;
		first.apply_behaviour(withObservation);
		expect(first.layout().Behaviour == stored && durable.Writes == 0U, "an edit is not accepted before its durable write completes");
		settle();
		expect(durable.Writes == 1U && durable.Preferences.Behaviour == chosen && !durable.Preferences.Behaviour.LabelColumnsRecovery.has_value(), "one durable write, with no load observation carried into it");
		expect(first.layout().Behaviour == chosen && second.layout().Behaviour == chosen && lastBehaviour(secondProjection) == chosen,
			   "the accepted edit reaches the other window of the profile, and the report is gone");
		expect(other.layout().Behaviour == application::review_behaviour_settings{ } && otherDurable.Writes == 0U, "the other profile is unaffected");
		expect(durable.Preferences.NewWindowDefaults.FilesWidth == 400U && durable.Preferences.Behaviour.OpenModality == application::review_open_modality::review,
			   "no other preference is reset by the label edit");

		for (const auto invalidWidth : { 15U, 257U }) {
			auto invalid              = chosen;
			invalid.CenterRibbonWidth = invalidWidth;
			expect(!preferences.apply_behaviour(invalid) && durable.Writes == 1U && first.layout().Behaviour == chosen,
				   "a ribbon width outside the supported bounds is refused before any durable write");
		}
		auto twice                    = chosen;
		twice.LabelColumns.Labels[1]  = core::feedback_label::question;
		const auto failuresBefore     = firstProjection.Failures;
		const auto publicationsBefore = firstProjection.Published.size();
		first.apply_behaviour(twice);
		settle();
		expect(!preferences.apply_behaviour(twice) && durable.Writes == 1U && first.layout().Behaviour == chosen, "a record naming a label twice is refused and never written");
		expect(firstProjection.Published.size() > publicationsBefore && lastBehaviour(firstProjection) == chosen && firstProjection.Failures == failuresBefore,
			   "the refused record is answered by republishing the held one, so the page reverts");

		auto there         = chosen;
		there.LabelColumns = *core::make_feedback_label_columns(std::array{ core::feedback_label::note });
		first.apply_behaviour(there);
		first.apply_behaviour(chosen);
		settle();
		expect(durable.Writes == 3U && durable.Preferences.Behaviour == chosen && first.layout().Behaviour == chosen && second.layout().Behaviour == chosen,
			   "going back to the held record while the first edit is in flight is written after it, so the reviewer's last choice wins");

		// Two windows edit at once: the profile's one owner serializes the
		// writes, the later record is the accepted one, and both windows show it.
		auto fromFirst           = chosen;
		fromFirst.OverviewCounts = application::review_overview_counts::open;
		auto fromSecond          = chosen;
		fromSecond.LabelColumns  = *core::make_feedback_label_columns(std::array{ core::feedback_label::typo, core::feedback_label::issue });
		first.apply_behaviour(fromFirst);
		second.apply_behaviour(fromSecond);
		settle();
		expect(durable.Writes == 5U && durable.Preferences.Behaviour == fromSecond && first.layout().Behaviour == fromSecond && second.layout().Behaviour == fromSecond,
			   "concurrent edits from two windows are written in turn and both windows settle on the accepted record");
		second.apply_behaviour(chosen);
		settle();
		expect(durable.Preferences.Behaviour == chosen && first.layout().Behaviour == chosen, "and the profile returns to the earlier record for the checks below");

		durable.Fail        = true;
		auto failed         = chosen;
		failed.LabelHelper  = application::review_label_helper::off;
		failed.LabelColumns = { };
		second.apply_behaviour(failed);
		settle();
		expect(durable.Preferences.Behaviour == chosen && first.layout().Behaviour == chosen && second.layout().Behaviour == chosen,
			   "a failed write is never accepted: both windows keep the held record");
		expect(secondProjection.Failures != 0U && lastBehaviour(secondProjection) == chosen, "the window that asked is told and shown the held record");
		durable.Fail = false;

		auto cancelled           = chosen;
		cancelled.OverviewCounts = application::review_overview_counts::open;
		expect(preferences.apply_behaviour(cancelled), "a write begins");
		preferences.cancel();
		settle();
		expect(durable.Preferences.Behaviour == chosen && !preferences.accepted(), "a cancelled write is not accepted and claims nothing");
		preferences.restore();
		settle();
		expect(preferences.accepted() && preferences.accepted()->Behaviour == chosen, "the profile reloads what is durable");

		auto reset           = chosen;
		reset.FeedbackMarks  = application::review_behaviour_settings{ }.FeedbackMarks;
		reset.LabelHelper    = application::review_behaviour_settings{ }.LabelHelper;
		reset.OverviewCounts = application::review_behaviour_settings{ }.OverviewCounts;
		reset.LabelColumns   = application::review_behaviour_settings{ }.LabelColumns;
		second.apply_behaviour(reset);
		settle();
		expect(durable.Preferences.Behaviour == reset && first.layout().Behaviour == reset && reset.OpenModality == application::review_open_modality::review,
			   "a scoped reset writes the label defaults only and reaches both windows");

		auto otherChoice        = application::review_behaviour_settings{ };
		otherChoice.LabelHelper = application::review_label_helper::on;
		other.apply_behaviour(otherChoice);
		settle();
		expect(otherDurable.Preferences.Behaviour == otherChoice && durable.Preferences.Behaviour == reset && first.layout().Behaviour == reset,
			   "the other profile's edit stays in that profile");
		first.shutdown();
		second.shutdown();
		other.shutdown();
		preferences.shutdown();
		otherPreferences.shutdown();
		settle();
		expect(first.is_quiescent() && second.is_quiescent() && other.is_quiescent() && preferences.is_quiescent() && otherPreferences.is_quiescent(), "label preference work drains");
	}
}
int main()
{
	test_refused_subscription_has_no_effect_authority();
	test_reentrant_preference_fanout();
	test_label_preferences_follow_the_profile_authority();
	test_owned_preferences_keep_the_claude_wrapper();
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
	chosen.OpenModality = application::review_open_modality::review;
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
