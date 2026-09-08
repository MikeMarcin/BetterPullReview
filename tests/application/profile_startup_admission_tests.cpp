#include "bpr/application/manual_executor.hpp"
#include "bpr/application/profile_startup_admission.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
	using namespace bpr;
	using namespace bpr::application;
	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	struct registry final : profile_registry_port
	{
		profile_registry_snapshot               Snapshot;
		std::optional<profile_registry_failure> Failure;
		std::size_t                             Calls = 0U;
		bool                                    Throw = false;
		registry()
		{
			Snapshot.LastProfileId = 2U;
			Snapshot.Profiles.push_back({ { core::bpr_profile_id{ 2U }, "Work" }, profile_root_mode::isolated, "profile-2" });
		}
		auto load(std::stop_token) -> profile_registry_result<profile_registry_snapshot> override
		{
			++Calls;
			if (Throw) {
				throw std::runtime_error("registry fixture failure");
			}
			return Failure ? profile_registry_result<profile_registry_snapshot>{ std::unexpected(*Failure) } : Snapshot;
		}
		auto save(const profile_registry_snapshot&, std::uint64_t, std::stop_token) -> profile_registry_result<profile_registry_snapshot> override
		{
			expect(false, "startup never writes the registry");
			return std::unexpected(profile_registry_failure::unavailable);
		}
	};
	struct roots final : profile_startup_root_port
	{
		std::vector<core::bpr_profile_id>      Profiles;
		std::optional<profile_startup_failure> Failure;
		bool                                   Throw    = false;
		bool                                   Relative = false;
		auto                                   resolve(const profile_storage_authority& aAuthority, std::stop_token) -> profile_startup_result<std::filesystem::path> override
		{
			Profiles.push_back(aAuthority.profile());
			if (Throw) {
				throw std::runtime_error("root fixture failure");
			}
			if (Failure) {
				return std::unexpected(*Failure);
			}
			if (Relative) {
				return std::filesystem::path{ "relative" };
			}
#if defined(_WIN32)
			const auto base = std::filesystem::path{ "C:/profile-startup-fixture" };
#else
			const auto base = std::filesystem::path{ "/profile-startup-fixture" };
#endif
			return aAuthority.root_mode() == profile_root_mode::legacy ? base : base / "profiles" / aAuthority.root_identity();
		}
	};
	struct projection final : profile_startup_projection_port
	{
		std::vector<profile_startup_terminal> Events;
		bool                                  Throw = false;
		void                                  publish(const profile_startup_terminal& aTerminal) override
		{
			if (Throw) {
				throw std::runtime_error("projection fixture failure");
			}
			Events.push_back(aTerminal);
		}
	};
	struct replay_lane final : decision_executor
	{
		std::vector<std::function<void()>> Tasks;
		void                               post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
	};
	struct reject_effects final : effect_executor
	{
		manual_executor Accepted;
		bool            Reject = true;
		void            post(std::function<void()> aTask) override
		{
			if (Reject) {
				throw std::runtime_error("effect queue rejected");
			}
			Accepted.post(std::move(aTask));
		}
	};

	void test_two_phase_exact_admission()
	{
		registry             store;
		roots                root;
		manual_executor      effects;
		manual_executor      decisions;
		projection           output;
		profile_startup_host host{ store, root, effects, decisions, output, core::application_session_id{ 7U } };
		const auto           requested = core::bpr_profile_id{ 2U };
		const auto           request   = host.start(requested);
		expect(request && store.Calls == 0U && root.Profiles.empty() && output.Events.empty(), "start only queues registry work");
		const auto busy = host.start(core::default_bpr_profile_id);
		expect(!busy && busy.error() == profile_startup_failure::busy && effects.pending() == 1U, "one bounded startup cannot be replaced by another profile");
		effects.drain();
		expect(store.Calls == 1U && root.Profiles.empty() && output.Events.empty(), "registry worker cannot resolve roots or publish");
		decisions.drain();
		expect(root.Profiles.empty() && effects.pending() == 1U && output.Events.empty(), "decision selection submits a separate root effect");
		effects.drain();
		expect(root.Profiles == std::vector{ requested } && output.Events.empty(), "only selected profile reaches root port and worker cannot publish admission");
		decisions.drain();
		expect(host.is_quiescent() && output.Events.size() == 1U && output.Events[0].Result.has_value(), "one decision terminal admits the runtime");
		const auto& terminal = output.Events[0];
		expect(terminal.Context == profile_startup_context{ core::application_session_id{ 7U }, *request, requested }, "terminal retains exact session request and profile identity");
		expect(terminal.Result->Authority.profile() == requested && terminal.Result->Label == "Work"
				   && terminal.Result->StorageRoot.filename() == "profile-2",
			   "admission carries selected authority label and isolated root");
		const auto next = host.start(core::default_bpr_profile_id);
		expect(next && next != request, "next startup receives a fresh request identity");
		effects.drain();
		decisions.drain();
		effects.drain();
		decisions.drain();
		expect(output.Events.size() == 2U && output.Events.back().Result->Authority.profile() == core::default_bpr_profile_id, "Default can be selected explicitly after quiescence");
	}

	void test_failure_isolation()
	{
		registry             store;
		roots                root;
		manual_executor      effects;
		manual_executor      decisions;
		projection           output;
		profile_startup_host host{ store, root, effects, decisions, output, core::application_session_id{ 8U } };
		expect(host.start(core::bpr_profile_id{ 99U }).has_value(), "unknown profile is checked against durable registry");
		effects.drain();
		decisions.drain();
		expect(!output.Events.back().Result && output.Events.back().Result.error() == profile_startup_failure::unknown_profile && root.Profiles.empty(), "unknown profile never falls back or resolves a root");
		store.Snapshot.Profiles.back().RootIdentity = "legacy";
		expect(host.start(core::default_bpr_profile_id).has_value(), "malformed registry startup submits");
		effects.drain();
		decisions.drain();
		expect(output.Events.back().Result.error() == profile_startup_failure::invalid_registry && root.Profiles.empty(), "an invalid sibling blocks Default admission too");
		store.Snapshot.Profiles.back().RootIdentity = "profile-2";
		store.Failure                               = profile_registry_failure::schema_mismatch;
		expect(host.start(core::default_bpr_profile_id).has_value(), "future registry failure submits");
		effects.drain();
		decisions.drain();
		expect(output.Events.back().Result.error() == profile_startup_failure::invalid_registry && root.Profiles.empty(), "future schema never becomes a fresh Default");
		store.Failure.reset();
		store.Throw = true;
		expect(host.start(core::default_bpr_profile_id).has_value(), "throwing registry submits");
		effects.drain();
		decisions.drain();
		expect(output.Events.back().Result.error() == profile_startup_failure::registry_unavailable, "registry exception becomes one typed terminal");
		store.Throw = false;
		root.Throw  = true;
		expect(host.start(core::bpr_profile_id{ 2U }).has_value(), "throwing root submits");
		effects.drain();
		decisions.drain();
		effects.drain();
		decisions.drain();
		expect(output.Events.back().Result.error() == profile_startup_failure::root_unavailable, "root exception cannot admit a fallback environment");
		root.Throw    = false;
		root.Relative = true;
		expect(host.start(core::bpr_profile_id{ 2U }).has_value(), "bad root result submits");
		effects.drain();
		decisions.drain();
		effects.drain();
		decisions.drain();
		expect(output.Events.back().Result.error() == profile_startup_failure::root_unavailable, "relative root data is refused without filesystem effects");
	}

	void test_cancellation_at_each_boundary()
	{
		for (int boundary = 0; boundary < 4; ++boundary) {
			registry             store;
			roots                root;
			manual_executor      effects;
			manual_executor      decisions;
			projection           output;
			profile_startup_host host{ store, root, effects, decisions, output, core::application_session_id{ 9U } };
			expect(host.start(core::bpr_profile_id{ 2U }).has_value(), "cancellation fixture starts");
			if (boundary >= 1) {
				effects.drain();
			}
			if (boundary >= 2) {
				decisions.drain();
			}
			if (boundary >= 3) {
				effects.drain();
			}
			host.cancel();
			expect(!host.is_quiescent() && !host.start(core::default_bpr_profile_id), "cancelled request retains its terminal slot until drained");
			effects.drain();
			decisions.drain();
			effects.drain();
			decisions.drain();
			expect(host.is_quiescent() && output.Events.size() == 1U && !output.Events[0].Result
					   && output.Events[0].Result.error() == profile_startup_failure::cancelled,
				   "every cancelled phase drains exactly one failure and no successful admission");
			expect(root.Profiles.size() == (boundary == 3 ? 1U : 0U), "cancelled queued work never calls root preparation");
		}
	}

	void test_stale_callbacks_shutdown_and_post_failure()
	{
		registry             store;
		roots                root;
		manual_executor      effects;
		replay_lane          lane;
		projection           output;
		profile_startup_host host{ store, root, effects, lane, output, core::application_session_id{ 10U } };
		expect(host.start(core::bpr_profile_id{ 2U }).has_value(), "replay fixture starts");
		effects.drain();
		lane.Tasks[0]();
		lane.Tasks[0]();
		expect(effects.pending() == 1U, "duplicate registry terminal cannot enqueue another root effect");
		effects.drain();
		lane.Tasks[1]();
		expect(host.start(core::default_bpr_profile_id).has_value(), "later startup starts");
		lane.Tasks[1]();
		lane.Tasks[0]();
		expect(output.Events.size() == 1U && !host.is_quiescent(), "old-phase terminals cannot publish or finish a later request");
		host.shutdown();
		effects.drain();
		lane.Tasks[2]();
		expect(host.is_quiescent() && output.Events.size() == 2U && output.Events.back().Result.error() == profile_startup_failure::cancelled && !host.start(core::default_bpr_profile_id), "shutdown emits one cancelled terminal and forbids restart");
		reject_effects       rejected;
		manual_executor      decision;
		projection           failedOutput;
		profile_startup_host rejectedHost{ store, root, rejected, decision, failedOutput, core::application_session_id{ 11U } };
		expect(rejectedHost.start(core::default_bpr_profile_id).has_value() && failedOutput.Events.empty(), "effect post rejection does not publish inline");
		decision.drain();
		expect(rejectedHost.is_quiescent() && failedOutput.Events.size() == 1U && failedOutput.Events[0].Result.error() == profile_startup_failure::registry_unavailable, "rejected submission still gets exactly one decision terminal");
		reject_effects rootRejected;
		rootRejected.Reject = false;
		manual_executor      rootDecision;
		projection           rootOutput;
		profile_startup_host rootRejectedHost{ store, root, rootRejected, rootDecision, rootOutput, core::application_session_id{ 13U } };
		expect(rootRejectedHost.start(core::bpr_profile_id{ 2U }).has_value(), "root-post failure starts with an accepted registry effect");
		rootRejected.Accepted.drain();
		rootRejected.Reject = true;
		rootDecision.run_next();
		expect(rootOutput.Events.empty() && rootDecision.pending() == 1U, "rejected root post queues rather than inlining its terminal");
		rootDecision.drain();
		expect(rootRejectedHost.is_quiescent() && rootOutput.Events.size() == 1U && rootOutput.Events.front().Result.error() == profile_startup_failure::root_unavailable,
			   "root effect rejection drains precisely one terminal");
		projection      detachedOutput;
		manual_executor detachedEffects;
		manual_executor detachedDecision;
		{
			profile_startup_host detached{ store, root, detachedEffects, detachedDecision, detachedOutput, core::application_session_id{ 12U } };
			expect(detached.start(core::bpr_profile_id{ 2U }).has_value(), "detached lifetime fixture starts");
			detachedEffects.drain();
		}
		detachedDecision.drain();
		expect(detachedOutput.Events.empty() && detachedEffects.pending() == 0U, "destroyed startup owner cannot publish or launch another phase");
	}
}

int main()
{
	test_two_phase_exact_admission();
	test_failure_isolation();
	test_cancellation_at_each_boundary();
	test_stale_callbacks_shutdown_and_post_failure();
	std::cout << "profile-startup-admission-tests:pass\n";
	return EXIT_SUCCESS;
}
