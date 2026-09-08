#include "bpr/application/manual_executor.hpp"
#include "bpr/application/profile_management_host.hpp"

#include <cstdlib>
#include <iostream>
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
	struct store final : profile_registry_port
	{
		profile_registry_snapshot               Snapshot;
		std::optional<profile_registry_failure> Failure;
		std::size_t                             Saves = 0U;
		auto                                    load(std::stop_token) -> profile_registry_result<profile_registry_snapshot> override
		{
			return Failure ? profile_registry_result<profile_registry_snapshot>{ std::unexpected(*Failure) } : Snapshot;
		}
		auto save(const profile_registry_snapshot& aCandidate, std::uint64_t aRevision, std::stop_token) -> profile_registry_result<profile_registry_snapshot> override
		{
			++Saves;
			if (Failure) {
				return std::unexpected(*Failure);
			}
			if (Snapshot.Revision != aRevision) {
				return std::unexpected(profile_registry_failure::conflict);
			}
			expect(valid_profile_registry_update(Snapshot, aCandidate), "host writes a legal forward registry transition");
			Snapshot = aCandidate;
			return Snapshot;
		}
	};
	struct projection final : profile_management_projection_port
	{
		std::vector<profile_management_projection>                States;
		std::vector<profile_runtime_command>                      Commands;
		std::function<void(const profile_management_projection&)> OnPublish;
		void                                                      publish(const profile_management_projection& aProjection) override
		{
			States.push_back(aProjection);
			if (OnPublish) {
				OnPublish(aProjection);
			}
		}
		void               perform(const profile_runtime_command& aCommand) override { Commands.push_back(aCommand); }
		[[nodiscard]] auto last() const -> const profile_management_projection& { return States.back(); }
	};
	struct fixture
	{
		store                   Store;
		manual_executor         Effects;
		manual_executor         Decisions;
		projection              Output;
		profile_management_host Host{ Store, Effects, Decisions, Output, core::application_session_id{ 1U } };
		void                    drain()
		{
			Effects.drain();
			Decisions.drain();
		}
		void load()
		{
			Host.reload();
			drain();
			expect(Output.last().Ready, "registry load admits management");
		}
		[[nodiscard]] auto create() -> core::bpr_profile_id
		{
			Host.apply(create_bpr_profile_intent{ "Work" });
			drain();
			return Store.Snapshot.Profiles.back().Profile.Id;
		}
	};

	void test_durable_edit_acceptance_and_failures()
	{
		fixture test;
		test.Host.reload();
		expect(test.Output.last().Busy && !test.Output.last().Ready, "reload projects busy before durable admission");
		test.drain();
		expect(test.Output.last().Ready && test.Output.last().Profiles.size() == 1U, "accepted Default becomes visible after decision completion");
		test.Host.apply(create_bpr_profile_intent{ "Work" });
		expect(test.Output.last().Busy && test.Output.last().Profiles.size() == 1U && !test.Output.last().AcceptedEdit, "create never publishes an optimistic label or edit acknowledgement");
		test.Effects.drain();
		expect(test.Output.last().Profiles.size() == 1U, "worker durable completion cannot update management");
		test.Decisions.drain();
		const auto work = test.Store.Snapshot.Profiles.back().Profile.Id;
		expect(test.Output.last().Profiles.size() == 2U && test.Output.last().AcceptedEdit
				   && test.Output.last().AcceptedEdit->Profile == work && test.Output.Commands.empty(),
			   "durable create acknowledges its exact profile without implicitly opening it");
		test.Host.apply(rename_bpr_profile_intent{ work, "Personal" });
		expect(test.Output.last().Profiles.back().Label == "Work" && !test.Output.last().AcceptedEdit, "rename retains the accepted label until durable completion");
		test.drain();
		expect(test.Output.last().Profiles.back().Profile == work && test.Output.last().Profiles.back().Label == "Personal"
				   && test.Output.last().AcceptedEdit->Operation == profile_management_operation::rename,
			   "rename updates only accepted display data");
		test.Host.apply(rename_bpr_profile_intent{ work, "" });
		expect(test.Output.last().Failure && test.Output.last().Failure->Reason == profile_management_failure::invalid_label
				   && test.Output.last().Profiles.back().Label == "Personal" && !test.Output.last().AcceptedEdit,
			   "rejected label leaves accepted projection intact and emits no candidate-clearing acknowledgement");
		test.Store.Failure = profile_registry_failure::unavailable;
		test.Host.apply(rename_bpr_profile_intent{ work, "Refused candidate" });
		test.drain();
		expect(test.Output.last().Failure && test.Output.last().Failure->Profile == work && !test.Output.last().AcceptedEdit && !test.Output.last().Ready,
			   "persistence failure is scoped and never acknowledges refused candidate text");
		expect(test.Store.Snapshot.Profiles.back().Profile.Label == "Personal", "failed rename leaves durable label unchanged");
		expect(test.Output.last().Profiles.back().Label == "Personal", "failed persistence preserves the last accepted row for candidate retention");
	}

	void test_one_owner_pending_open_and_closing()
	{
		fixture test;
		test.load();
		const auto work = test.create();
		test.Host.apply(open_bpr_profile_intent{ core::default_bpr_profile_id });
		const auto initial = test.Output.Commands.back().Context;
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::start && test.Host.complete_open(initial), "already-admitted initial app can bind the first start reservation");
		test.Host.apply(open_bpr_profile_intent{ work });
		const auto context = test.Output.Commands.back().Context;
		const auto count   = test.Output.Commands.size();
		test.Host.apply(open_bpr_profile_intent{ work });
		expect(test.Output.Commands.size() == count && test.Output.last().Failure->Reason == profile_management_failure::busy, "pending startup cannot create another private owner");
		expect(test.Host.complete_open(context), "exact startup completion opens its reserved runtime");
		test.Host.apply(open_bpr_profile_intent{ work });
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::activate && test.Output.Commands.back().Context == context,
			   "opening an existing profile activates the same runtime identity");
		test.Host.apply(rename_bpr_profile_intent{ work, "Renamed Work" });
		test.drain();
		test.Host.apply(open_bpr_profile_intent{ work });
		expect(test.Output.Commands.back().Context == context && test.Output.Commands.back().Label == "Renamed Work", "registry synchronization preserves live context and adopts accepted label");
		expect(test.Host.begin_close(context), "closing reserves the runtime until its work drains");
		expect(!test.Host.complete_open(context, std::unexpected(profile_management_failure::runtime_unavailable)),
			   "duplicate startup failure cannot retire an already-open runtime during close");
		const auto closingCount = test.Output.Commands.size();
		test.Host.apply(open_bpr_profile_intent{ work });
		expect(test.Output.Commands.size() == closingCount && test.Output.last().Failure->Reason == profile_management_failure::busy, "closing cannot race a replacement owner");
		expect(test.Host.complete_close(context), "exact drained close retires its reservation");
		test.Host.apply(open_bpr_profile_intent{ work });
		const auto replacement = test.Output.Commands.back().Context;
		expect(replacement != context && test.Output.Commands.back().Kind == profile_runtime_command_kind::start, "reopening a fully drained profile receives a new context");
		expect(!test.Host.complete_open(context) && !test.Host.complete_close(context), "old completion cannot affect a replacement runtime");
		expect(test.Host.complete_open(replacement), "replacement remains completable after stale callbacks");
		expect(test.Host.begin_close(initial) && test.Host.complete_close(initial), "initial runtime can close independently");
		test.Host.apply(open_bpr_profile_intent{ work });
		expect(test.Output.Commands.back().Kind == profile_runtime_command_kind::activate && test.Output.Commands.back().Context == replacement, "another profile remains live after initial window closes");
	}

	void test_failed_and_foreign_completions()
	{
		fixture test;
		test.load();
		const auto work = test.create();
		test.Host.apply(open_bpr_profile_intent{ work });
		const auto context = test.Output.Commands.back().Context;
		const auto foreign = core::bpr_window_context{ core::default_bpr_profile_id, context.Window, context.Generation };
		expect(!test.Host.complete_open(foreign) && !test.Host.begin_close(foreign) && !test.Host.complete_close(foreign), "wrong-profile callbacks cannot affect the pending owner");
		expect(test.Host.complete_open(context, std::unexpected(profile_management_failure::runtime_unavailable)), "startup failure retires its exact pending reservation");
		expect(test.Output.last().Failure && test.Output.last().Failure->Profile == work && test.Output.last().Profiles.back().Runtime == profile_management_runtime::closed,
			   "failed startup leaves a scoped error and no live owner");
		const auto count = test.Output.Commands.size();
		test.Host.apply(open_bpr_profile_intent{ core::bpr_profile_id{ 99U } });
		expect(test.Output.Commands.size() == count && test.Output.last().Failure->Reason == profile_management_failure::unknown_profile, "unknown profile cannot produce a fallback start");
	}

	void test_shutdown_and_reentrant_projection()
	{
		fixture test;
		test.load();
		const auto work = test.create();
		test.Host.apply(open_bpr_profile_intent{ work });
		const auto opening = test.Output.Commands.back().Context;
		test.Host.apply(create_bpr_profile_intent{ "Cancelled" });
		test.Host.shutdown();
		const auto count = test.Output.Commands.size();
		test.drain();
		expect(!test.Host.complete_open(opening), "late successful startup after shutdown must be disposed by composition");
		test.Host.apply(open_bpr_profile_intent{ core::default_bpr_profile_id });
		expect(test.Output.Commands.size() == count && !test.Host.is_quiescent(), "late runtime still reserves its root while closing");
		expect(test.Host.complete_close(opening) && test.Host.is_quiescent(), "quiescence follows exact runtime and registry drainage");
		fixture reentrant;
		reentrant.load();
		reentrant.Output.OnPublish = [&reentrant](const auto& aState) {
			if (!aState.Closing && !aState.Profiles.empty() && aState.Profiles.front().Runtime == profile_management_runtime::opening) {
				reentrant.Host.shutdown();
			}
		};
		reentrant.Host.apply(open_bpr_profile_intent{ core::default_bpr_profile_id });
		expect(reentrant.Output.Commands.empty() && reentrant.Host.is_quiescent(), "reentrant shutdown before command delivery cannot start a runtime or strand an undelivered reservation");
	}
}

int main()
{
	test_durable_edit_acceptance_and_failures();
	test_one_owner_pending_open_and_closing();
	test_failed_and_foreign_completions();
	test_shutdown_and_reentrant_projection();
	std::cout << "profile-management-host-tests:pass\n";
	return EXIT_SUCCESS;
}
