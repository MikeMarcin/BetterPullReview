#include "bpr/application/manual_executor.hpp"
#include "bpr/application/profile_registry.hpp"

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

	struct memory_store final : profile_registry_port
	{
		profile_registry_snapshot               Durable;
		std::size_t                             Loads = 0U;
		std::size_t                             Saves = 0U;
		std::optional<profile_registry_failure> Fail;
		bool                                    Throw = false;
		auto                                    load(std::stop_token) -> profile_registry_result<profile_registry_snapshot> override
		{
			++Loads;
			if (Throw) {
				throw std::runtime_error("fixture failure");
			}
			if (Fail) {
				return std::unexpected(*Fail);
			}
			return Durable;
		}
		auto save(const profile_registry_snapshot& aCandidate, const std::uint64_t aRevision,
				  std::stop_token) -> profile_registry_result<profile_registry_snapshot> override
		{
			++Saves;
			if (Throw) {
				throw std::runtime_error("fixture failure");
			}
			if (Fail) {
				return std::unexpected(*Fail);
			}
			if (Durable.Revision != aRevision) {
				return std::unexpected(profile_registry_failure::conflict);
			}
			if (!valid_profile_registry_update(Durable, aCandidate)) {
				return std::unexpected(profile_registry_failure::corrupt);
			}
			Durable = aCandidate;
			return Durable;
		}
	};

	struct projection final : profile_registry_projection_port
	{
		std::vector<profile_registry_publication> Events;
		bool                                      Throw = false;
		void                                      publish(const profile_registry_publication& aPublication) override
		{
			if (Throw) {
				throw std::runtime_error("projection failure");
			}
			Events.push_back(aPublication);
		}
	};

	struct replay_lane final : decision_executor
	{
		std::vector<std::function<void()>> Tasks;
		void                               post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
	};

	void test_durable_acceptance_and_cancellation()
	{
		memory_store          store;
		manual_executor       effects;
		manual_executor       decisions;
		projection            output;
		profile_registry_host host{ store, effects, decisions, output, core::application_session_id{ 1U } };
		expect(!host.create("Work") && !host.accepted(), "unloaded registry cannot synthesize Default or save");
		expect(host.reload().has_value() && store.Loads == 0U && !host.accepted(), "load is submitted off the decision lane");
		expect(!host.reload() && effects.pending() == 1U, "only one effect may be outstanding");
		effects.drain();
		expect(store.Loads == 1U && !host.accepted() && output.Events.empty(), "worker completion alone cannot publish or mutate acceptance");
		decisions.drain();
		expect(host.accepted() == store.Durable && output.Events.size() == 1U, "decision completion adopts durable Default");
		const auto before = *host.accepted();
		expect(host.create("../Work").has_value() && *host.accepted() == before, "create does not optimistically change accepted state");
		expect(!host.rename(core::default_bpr_profile_id, "Other") && effects.pending() == 1U, "busy edits are bounded and do not enqueue hidden writes");
		effects.drain();
		expect(*host.accepted() == before && store.Durable.Profiles.size() == 2U, "durable write and decision acceptance are distinct");
		decisions.drain();
		const auto work = host.accepted()->Profiles.back();
		expect(work.Profile.Label == "../Work" && work.RootIdentity == "profile-2" && work.RootMode == profile_root_mode::isolated, "new profile root authority is independent of path-looking label data");
		expect(host.rename(work.Profile.Id, "Personal").has_value(), "rename submits");
		effects.drain();
		decisions.drain();
		expect(host.accepted()->Profiles.back().Profile.Id == work.Profile.Id && host.accepted()->Profiles.back().RootIdentity == work.RootIdentity, "rename preserves identity and isolated root");
		expect(host.create("Committed before cancellation").has_value(), "next edit submits");
		effects.drain();
		host.cancel();
		expect(!host.accepted() && !host.reload(), "cancellation invalidates acceptance but waits for pending terminal");
		decisions.drain();
		expect(host.is_quiescent() && !host.accepted() && output.Events.back().Failure == profile_registry_failure::cancelled, "late committed completion cannot repopulate cancelled acceptance");
		expect(host.reload().has_value(), "fresh reload is allowed after terminal");
		effects.drain();
		decisions.drain();
		expect(host.accepted() == store.Durable && host.accepted()->Profiles.size() == 3U, "reload reconciles a write that committed before cancellation");
		const auto saves = store.Saves;
		expect(host.create("Cancelled before execution").has_value(), "cancellable edit submits");
		host.cancel();
		effects.drain();
		decisions.drain();
		expect(store.Saves == saves && !host.accepted(), "cancellation before execution never calls the store");
	}

	void test_failures_and_shutdown()
	{
		memory_store          store;
		manual_executor       effects;
		manual_executor       decisions;
		projection            output;
		profile_registry_host host{ store, effects, decisions, output, core::application_session_id{ 2U } };
		expect(host.reload().has_value(), "load submits");
		effects.drain();
		decisions.drain();
		++store.Durable.Revision;
		expect(host.rename(core::default_bpr_profile_id, "Default renamed").has_value(), "stale revision edit submits");
		effects.drain();
		decisions.drain();
		expect(!host.accepted() && output.Events.back().Failure == profile_registry_failure::conflict
				   && output.Events.back().Profile == core::default_bpr_profile_id,
			   "CAS conflict is profile-scoped and requires reload");
		store.Fail = profile_registry_failure::schema_mismatch;
		expect(host.reload().has_value(), "failed registry reload submits");
		effects.drain();
		decisions.drain();
		expect(!host.accepted() && output.Events.back().Failure == profile_registry_failure::schema_mismatch, "future storage failure cannot fall back to fresh Default");
		store.Fail.reset();
		store.Throw = true;
		expect(host.reload().has_value(), "exception fixture submits");
		effects.drain();
		decisions.drain();
		expect(host.is_quiescent() && output.Events.back().Failure == profile_registry_failure::unavailable, "effect exceptions produce a terminal");
		store.Throw  = false;
		output.Throw = true;
		expect(host.reload().has_value(), "projection exception fixture submits");
		effects.drain();
		decisions.drain();
		expect(host.accepted().has_value() && host.projection_delivery_failed(), "projection exceptions cannot undo durable acceptance or escape the lane");
		output.Throw     = false;
		const auto count = output.Events.size();
		expect(host.create("Pending").has_value(), "shutdown fixture submits");
		host.shutdown();
		effects.drain();
		decisions.drain();
		expect(host.is_quiescent() && !host.accepted() && output.Events.size() == count && !host.reload(), "shutdown drains terminal without late projection or restart");
	}

	void test_stale_terminals_and_catalog_validation()
	{
		memory_store          store;
		manual_executor       effects;
		replay_lane           lane;
		projection            output;
		profile_registry_host host{ store, effects, lane, output, core::application_session_id{ 3U } };
		expect(host.reload().has_value(), "replay load submits");
		effects.drain();
		lane.Tasks[0]();
		expect(host.create("Work").has_value(), "next request submits");
		const auto before = host.accepted();
		lane.Tasks[0]();
		expect(host.accepted() == before && !host.is_quiescent() && output.Events.size() == 1U, "duplicate old terminal cannot complete a later request");
		effects.drain();
		lane.Tasks[1]();
		lane.Tasks[0]();
		expect(host.accepted()->Profiles.size() == 2U && output.Events.size() == 2U, "old accepted load cannot overwrite a newer durable catalog");
		auto wrong                         = store.Durable;
		wrong.Profiles.back().RootMode     = profile_root_mode::legacy;
		wrong.Profiles.back().RootIdentity = "legacy";
		expect(!valid(wrong), "nonDefault cannot borrow legacy root authority");
		wrong                             = store.Durable;
		wrong.Profiles.front().Profile.Id = core::bpr_profile_id{ 99U };
		expect(!valid(wrong), "Default identity cannot be replaced");
		wrong = store.Durable;
		wrong.Profiles.push_back(wrong.Profiles.back());
		expect(!valid(wrong), "duplicate profile identities are refused");
		wrong                = store.Durable;
		wrong.DefaultAdopted = false;
		expect(!valid(wrong), "partial adoption is not an accepted registry");
		wrong = store.Durable;
		++wrong.Revision;
		wrong.Profiles.pop_back();
		expect(!valid_profile_registry_update(store.Durable, wrong), "save cannot delete an existing profile");
	}
}

int main()
{
	test_durable_acceptance_and_cancellation();
	test_failures_and_shutdown();
	test_stale_terminals_and_catalog_validation();
	std::cout << "profile-registry-tests:pass\n";
	return EXIT_SUCCESS;
}
