#include "bpr/application/manual_executor.hpp"
#include "bpr/application/window_review_restoration_host.hpp"
#include <cstdlib>
#include <iostream>
namespace {
	using namespace bpr;
	using namespace bpr::application;
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(1);
		}
	}
	auto context(std::uint64_t aId, std::uint64_t aGeneration = 2) -> core::bpr_window_context { return { core::default_bpr_profile_id, core::bpr_window_id{ aId }, core::bpr_window_generation{ aGeneration } }; }
	auto destination(std::uint32_t aNumber = 27) -> core::window_review_destination { return { core::github_access_mode::authenticated, "github.com", { "owner", "repo" }, core::pull_request_number{ aNumber } }; }
	struct store final : window_review_restoration_port
	{
		window_review_snapshot                     Snapshot;
		std::optional<window_review_store_failure> Failure;
		std::size_t                                Saves        = 0;
		bool                                       MismatchSave = false;
		auto                                       load(std::stop_token) -> window_review_store_result<window_review_snapshot> override
		{
			if (Failure) {
				return std::unexpected(window_review_store_error{ *Failure, { } });
			}
			return Snapshot;
		}
		auto save(const window_review_snapshot& aCandidate, std::uint64_t aRevision, std::stop_token) -> window_review_store_result<window_review_snapshot> override
		{
			++Saves;
			if (Failure) {
				return std::unexpected(window_review_store_error{ *Failure, *Failure == window_review_store_failure::conflict ? std::optional{ Snapshot } : std::nullopt });
			}
			if (aRevision != Snapshot.Revision) {
				return std::unexpected(window_review_store_error{ window_review_store_failure::conflict, Snapshot });
			}
			expect(valid(aCandidate), "only valid bounded snapshots reach store");
			Snapshot = aCandidate;
			if (MismatchSave) {
				return window_review_snapshot{ Snapshot.Revision, { } };
			}
			return Snapshot;
		}
	};
	struct projection final : window_review_restoration_projection_port
	{
		std::vector<core::window_review_startup_request> Startups;
		std::vector<core::window_review_open_request>    Opens;
		std::vector<core::window_review_error>           Errors;
		std::function<void()>                            OnStartup;
		std::function<void()>                            OnOpen;
		void                                             select_startup(const core::window_review_startup_request& aRequest) override
		{
			Startups.push_back(aRequest);
			if (OnStartup) {
				OnStartup();
			}
		}
		void open_review(const core::window_review_open_request& aRequest) override
		{
			Opens.push_back(aRequest);
			if (OnOpen) {
				OnOpen();
			}
		}
		void fail(const core::window_review_error& aError) override { Errors.push_back(aError); }
	};
	struct fixture
	{
		store                          Store;
		manual_executor                Effects;
		manual_executor                Decisions;
		projection                     Output;
		window_review_restoration_host Host{ core::default_bpr_profile_id, { context(1), context(2) }, { "github.com" }, Store, Effects, Decisions, Output };
		void                           drain()
		{
			while (Effects.pending() || Decisions.pending()) {
				Effects.drain();
				Decisions.drain();
			}
		}
		void load()
		{
			Host.start();
			drain();
		}
	};
	void test_load_prunes_and_gates_restoration()
	{
		fixture f;
		f.Store.Snapshot.Records = { { core::bpr_window_id{ 1 }, destination() }, { core::bpr_window_id{ 2 }, destination(28) }, { core::bpr_window_id{ 9 }, destination(99) } };
		f.Host.start();
		expect(f.Output.Startups.empty(), "startup choice waits for load decision");
		f.Effects.drain();
		expect(f.Output.Startups.empty(), "effect cannot select auth");
		f.Decisions.drain();
		expect(f.Output.Startups.size() == 1 && f.Output.Opens.empty(), "one chosen shared authority; readiness and session gate reads");
		f.Host.mark_window_ready(context(1));
		f.Host.mark_window_ready(context(2));
		const auto request = f.Output.Startups[0].Request;
		f.Host.complete_startup(request, core::window_review_authority{ core::github_access_mode::authenticated, "github.com" });
		expect(f.Output.Opens.size() == 2, "exact matching session opens both distinct saved PRs");
		f.Host.complete_startup(request, core::window_review_authority{ core::github_access_mode::authenticated, "github.com" });
		expect(f.Output.Opens.size() == 2, "duplicate auth cannot reopen");
		f.Host.cancel_restore(context(1));
		const auto read = f.Output.Opens[0];
		f.Host.complete_restore(read.Context, read.Request, std::unexpected(core::window_review_failure::unavailable));
		expect(f.Output.Errors.empty(), "cancelled read terminal has no late projection");
		f.drain();
		expect(f.Store.Snapshot.Records.size() == 2, "startup prunes orphan without altering live destinations");
	}
	void test_coalesced_quit_close_and_conflict()
	{
		fixture f;
		f.load();
		f.Host.accepted_read(context(1), destination(1));
		f.Host.accepted_read(context(1), destination(2));
		f.Host.accepted_read(context(2), destination(3));
		expect(f.Effects.pending() == 1, "one active bounded write plus latest candidate");
		f.Host.shutdown_orderly();
		f.drain();
		expect(f.Host.is_quiescent() && f.Store.Saves == 2 && f.Store.Snapshot.Records[0].Destination.Number.value() == 2, "Quit flushes latest accepted destination snapshot");
		fixture c;
		c.load();
		c.Host.accepted_read(context(1), destination());
		c.Host.close(context(1));
		expect(!c.Host.window_quiescent(context(1)), "close waits prior writer and erase");
		c.drain();
		expect(c.Host.window_quiescent(context(1)) && c.Store.Snapshot.Records.empty(), "exact erase completes independently");
		c.Host.accepted_read(context(2), destination(30));
		expect(c.Host.window_quiescent(context(1)), "sibling later writer cannot hold closed window");
		c.drain();
		c.Store.Failure = window_review_store_failure::conflict;
		c.Host.accepted_read(context(2), destination(31));
		c.drain();
		expect(c.Output.Errors.back().Reason == core::window_review_failure::conflict && c.Store.Snapshot.Records[0].Destination.Number.value() == 30, "CAS conflict preserves durable and pending candidate without blind retry");
		const auto saves = c.Store.Saves;
		c.drain();
		expect(c.Store.Saves == saves, "conflict has no automatic overwrite");
		c.Store.Failure.reset();
		c.Host.retry_persistence();
		c.drain();
		expect(c.Store.Snapshot.Records[0].Destination.Number.value() == 31, "explicit reload retries retained accepted candidate with current revision");
	}
	void test_failure_cancellation_and_reentrancy()
	{
		fixture bad;
		bad.Store.Failure = window_review_store_failure::schema_mismatch;
		bad.load();
		expect(bad.Output.Startups.empty() && bad.Output.Errors.size() == 1, "future optional store never fabricates remembered startup");
		bad.Host.shutdown_orderly();
		expect(bad.Host.is_quiescent(), "failed optional store cannot hang quit");
		fixture cancelled;
		cancelled.Host.start();
		cancelled.Host.cancel_startup();
		cancelled.drain();
		expect(cancelled.Output.Startups.empty(), "user activity beats delayed load choice");
		fixture home;
		home.Store.Snapshot.Records = { { core::bpr_window_id{ 1 }, destination() }, { core::bpr_window_id{ 2 }, destination() } };
		home.Host.cancel_restore(context(1));
		home.load();
		expect(home.Output.Startups.size() == 1, "Home cancellation leaves sibling startup eligible");
		home.Output.OnStartup = { };
		home.Host.shutdown();
		home.drain();
		expect(home.Host.is_quiescent(), "forced cleanup drains");
		fixture reentrant;
		reentrant.Output.OnStartup = [&] { reentrant.Host.shutdown(); };
		reentrant.load();
		expect(reentrant.Host.is_quiescent() && reentrant.Output.Opens.empty(), "reentrant forced shutdown cannot issue late reads");
		fixture forced;
		forced.load();
		forced.Host.accepted_read(context(1), destination());
		forced.Host.shutdown_orderly();
		forced.Host.shutdown();
		forced.drain();
		expect(forced.Host.is_quiescent() && forced.Store.Saves == 0, "forced cancellation suppresses unstarted write");
	}
	void test_invalid_observation_and_delayed_load()
	{
		fixture invalid;
		invalid.Store.Snapshot.Revision = 0;
		invalid.load();
		expect(invalid.Output.Startups.empty() && invalid.Output.Errors.size() == 1
				   && invalid.Output.Errors[0].Reason == core::window_review_failure::corrupt,
			   "invalid load observation cannot select authority");
		fixture mismatch;
		mismatch.load();
		mismatch.Store.MismatchSave = true;
		mismatch.Host.accepted_read(context(1), destination());
		mismatch.drain();
		expect(mismatch.Output.Errors.back().Reason == core::window_review_failure::corrupt,
			   "successful save with wrong payload is not accepted");
		mismatch.Host.shutdown_orderly();
		expect(mismatch.Host.is_quiescent(), "invalid save terminal cannot hang Quit");

		fixture late;
		late.Store.Snapshot.Records = { { core::bpr_window_id{ 1 }, destination() }, { core::bpr_window_id{ 2 }, destination(28) } };
		late.Host.start();
		late.Host.cancel_startup();
		late.Host.accepted_read(context(2), destination(42));
		late.Host.close(context(1));
		late.drain();
		expect(late.Output.Startups.empty() && late.Store.Snapshot.Records.size() == 1
				   && late.Store.Snapshot.Records[0].Window == core::bpr_window_id{ 2 }
				   && late.Store.Snapshot.Records[0].Destination.Number.value() == 42,
			   "delayed load preserves newer accepted navigation and excludes closed window");
	}
	void test_open_reentrancy_and_capacity()
	{
		fixture reentrant;
		reentrant.Store.Snapshot.Records = { { core::bpr_window_id{ 1 }, destination() }, { core::bpr_window_id{ 2 }, destination(28) } };
		reentrant.load();
		reentrant.Host.mark_window_ready(context(1));
		reentrant.Host.mark_window_ready(context(2));
		reentrant.Output.OnOpen = [&] { reentrant.Host.shutdown(); };
		reentrant.Host.complete_startup(reentrant.Output.Startups[0].Request,
										core::window_review_authority{ core::github_access_mode::authenticated, "github.com" });
		expect(reentrant.Output.Opens.size() == 1 && reentrant.Host.is_quiescent(), "shutdown during first open prevents sibling publication");
		fixture bounded;
		for (std::uint64_t id = 3; id <= core::maximum_bpr_windows_per_profile; ++id) {
			expect(bounded.Host.attach(context(id, 1)), "window capacity admits each bounded fresh context");
		}
		expect(!bounded.Host.attach(context(core::maximum_bpr_windows_per_profile + 1, 1)), "window capacity rejects overflow");
		bounded.load();
		bounded.Host.accepted_read(context(1), destination());
		bounded.Host.close(context(1));
		expect(!bounded.Host.attach(context(33, 1)), "draining closed writer retains its capacity reservation");
		bounded.drain();
		expect(bounded.Host.attach(context(33, 1)), "completed erase releases capacity for a fresh window");
	}
}
int main()
{
	test_load_prunes_and_gates_restoration();
	test_coalesced_quit_close_and_conflict();
	test_failure_cancellation_and_reentrancy();
	test_invalid_observation_and_delayed_load();
	test_open_reentrancy_and_capacity();
	std::cout << "window-review-restoration-host-tests:pass\n";
}
