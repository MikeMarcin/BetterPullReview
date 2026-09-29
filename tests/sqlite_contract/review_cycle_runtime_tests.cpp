#include "../support/review_tour_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "scratch_root.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using application::cycle_store_failure;
	using application::review_material_status;
	using tests::expect;

	constexpr auto profile = core::bpr_profile_id{ 1 };

	auto operation(const std::uint8_t aSeed) -> application::cycle_operation_id
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = 0x5A;
		bytes[31]  = aSeed;
		return application::cycle_operation_id{ bytes };
	}

	auto file_bytes(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream input{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
	}

	/// Runs one runtime call to completion on the manual lanes.
	template <typename T, typename TStart>
	auto run(application::manual_executor& aLanes, TStart aStart) -> application::cycle_store_result<T>
	{
		auto result = std::optional<application::cycle_store_result<T>>{ };
		expect(aStart([&](application::cycle_store_result<T> aValue) { result = std::move(aValue); }), "runtime admits the call");
		aLanes.drain();
		expect(result.has_value(), "completion delivered once");
		return std::move(*result);
	}

	struct local_runtime
	{
		application::manual_executor             Lanes;
		application::review_cycle_runtime        Runtime;
		core::review_cycle_catalog               Catalog;
		std::optional<core::local_cycle_subject> Subject;

		explicit local_runtime(const std::filesystem::path& aRoot)
			: Runtime{ adapters::sqlite::make_review_cycle_store(aRoot, profile), Lanes, Lanes }
		{
			auto opened = run<core::review_cycle_catalog>(Lanes, [&](auto aDone) { return Runtime.open(std::move(aDone)); });
			expect(opened.has_value(), "open without account, provider or agent");
			Catalog = *opened;
		}

		auto create(const std::uint8_t aOperation) -> core::local_cycle_subject
		{
			const auto receipt = run<application::cycle_write_receipt>(Lanes, [&](auto aDone) {
				return Runtime.create(operation(aOperation), { .Authority = Catalog.Authority, .Title = "Local module", .Scope = { .Paths = { "src" } } }, std::move(aDone));
			});
			expect(receipt && receipt->Changed, "create a cycle");
			return { .Authority = Catalog.Authority, .Cycle = receipt->Cycle, .Generation = 1 };
		}

		auto capture(const std::uint8_t aOperation, const std::uint64_t aExpected, std::vector<std::uint8_t> aManifest, const std::uint64_t aEntries)
			-> application::cycle_store_result<application::cycle_write_receipt>
		{
			return run<application::cycle_write_receipt>(Lanes, [&](auto aDone) {
				return Runtime.capture_selected(operation(aOperation), aExpected, { .ScopeVersion = 1, .Entries = aEntries, .Coverage = core::cycle_capture_coverage::complete }, std::move(aManifest), std::move(aDone));
			});
		}

		auto load() -> application::cycle_store_result<std::optional<core::review_cycle_state>>
		{
			return run<std::optional<core::review_cycle_state>>(Lanes, [&](auto aDone) { return Runtime.load_selected(std::move(aDone)); });
		}
	};

	void initializes_and_operates_without_account_or_agent()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-runtime" };
		{
			auto       local   = local_runtime{ scratch.path() };
			const auto subject = local.create(1);
			local.Runtime.select(subject);
			const auto captured = local.capture(2, 1, { }, 0);
			expect(captured && captured->Allocated == 1, "empty R0 through the runtime");
			local.Runtime.shutdown();
			expect(local.Runtime.is_quiescent() && !local.Runtime.open([](auto) { }), "shutdown refuses new work");
		}
		auto       reopened = local_runtime{ scratch.path() };
		const auto subject  = core::local_cycle_subject{ .Authority = reopened.Catalog.Authority, .Cycle = core::review_cycle_id{ 1 }, .Generation = 1 };
		reopened.Runtime.select(subject);
		const auto loaded = reopened.load();
		expect(loaded && *loaded && core::project(**loaded).InitialEmpty, "reopened runtime reads the empty R0");
	}

	void switches_subjects_without_crossing_authority()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-switch" };
		auto                                 local   = local_runtime{ scratch.path() };
		const auto                           subject = local.create(1);
		local.Runtime.select(subject);

		// Work issued for one selection completes as cancelled after a switch.
		auto stale = std::optional<application::cycle_store_result<std::optional<core::review_cycle_state>>>{ };
		expect(local.Runtime.load_selected([&](auto aValue) { stale = std::move(aValue); }), "queue a load");
		auto pullRequest        = core::pull_request_authority{ };
		pullRequest.PullRequest = core::pull_request_id{ 9 };
		pullRequest.Number      = core::pull_request_number{ 9 };
		local.Runtime.select(pullRequest);
		local.Lanes.drain();
		expect(stale && !*stale && stale->error().Failure == cycle_store_failure::cancelled, "stale selection's result is not delivered as the new subject's");

		// A pull-request selection never reads the cycle store.
		const auto refused = local.load();
		expect(!refused && refused.error().Refusal == core::cycle_refusal::invalid_identity, "pull request subject refused by the cycle runtime");
		expect(!local.capture(2, 1, { 1, 2 }, 1), "no capture under a pull request selection");
		expect(std::holds_alternative<core::pull_request_authority>(*local.Runtime.selected()), "selection kept whole");

		local.Runtime.select(subject);
		const auto back = local.load();
		expect(back && *back && !core::project(**back).Captured, "switching back reads the untouched cycle");
	}

	void leaves_existing_pull_request_state_untouched()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-migration" };
		const auto                           draft = tests::review_tour_fixture();
		{
			auto tours = adapters::sqlite::make_review_tour_store(scratch.path(), draft.Owner.Profile);
			expect(tours->save(draft, 0, { }).has_value(), "seed a pull-request tour draft");
		}
		const auto tourPath = scratch.path() / "state/v1/review-tours.sqlite3";
		const auto before   = file_bytes(tourPath);

		auto local = local_runtime{ scratch.path() };
		static_cast<void>(local.create(9));
		auto store  = adapters::sqlite::make_review_cycle_store(scratch.path(), profile);
		auto listed = store->list(std::nullopt, { });
		expect(listed && listed->Cycles.size() == 1 && listed->Cycles.front().Title == "Local module", "no cycle is fabricated from pull-request records");
		expect(file_bytes(tourPath) == before, "pull-request store bytes unchanged");
		auto tours = adapters::sqlite::make_review_tour_store(scratch.path(), draft.Owner.Profile);
		expect(tours->load(draft.Owner, draft.Id, { }).value() == draft, "pull-request draft reads back exactly");
	}

	class fixed_blobs final : public application::diff_blob_port
	{
	public:
		auto materialize(const application::diff_blob_request& aRequest, std::stop_token) -> application::diff_blob_observation override
		{
			++Calls;
			return { .Context = aRequest.Context, .Side = aRequest.Side, .Blob = aRequest.Blob, .Status = application::diff_blob_status::available, .Bytes = { 'p', 'r' } };
		}
		int Calls = 0;
	};

	void routes_material_by_authority()
	{
		tests::sqlite_contract::scratch_root scratch{ "cycle-bridge" };
		auto                                 local   = local_runtime{ scratch.path() };
		const auto                           subject = local.create(1);
		local.Runtime.select(subject);
		const auto manifest = std::vector<std::uint8_t>{ 'm', 'a', 'n' };
		expect(local.capture(2, 1, manifest, 1).has_value(), "capture");
		auto       store    = adapters::sqlite::make_review_cycle_store(scratch.path(), profile);
		const auto revision = store->revisions(subject, std::nullopt, { })->Revisions.front();
		const auto object   = revision.Manifest;
		const auto request  = application::cycle_material_request{ .Subject = subject, .Revision = revision.Revision.Id.Id, .Object = object, .MaximumBytes = 1024 };

		auto       localBridge = application::review_material_bridge{ nullptr, store.get(), local.Catalog.Authority };
		const auto found       = localBridge.materialize(request, { });
		expect(found.Status == review_material_status::available && found.Bytes == manifest, "cycle material by subject, revision and object");
		expect(localBridge.materialize(application::diff_blob_request{ }, { }).Status == review_material_status::wrong_authority, "local runtime serves no pull-request blob");

		auto foreign                          = request;
		foreign.Subject.Authority.Incarnation = core::cycle_catalog_incarnation{ std::array<std::uint8_t, 32>{ 3 } };
		expect(localBridge.materialize(foreign, { }).Status == review_material_status::wrong_authority, "another catalog's cycle refused");
		auto otherObject   = request;
		otherObject.Object = *store->install(std::vector<std::uint8_t>{ 'x' }, { });
		expect(localBridge.materialize(otherObject, { }).Status == review_material_status::wrong_authority, "an object the revision did not record is refused");
		auto absent     = request;
		absent.Revision = core::cycle_source_revision_id{ 5 };
		expect(localBridge.materialize(absent, { }).Status == review_material_status::missing, "unknown revision");
		auto bounded         = request;
		bounded.MaximumBytes = 2;
		expect(localBridge.materialize(bounded, { }).Status == review_material_status::too_large, "bounded material");

		auto       blobs    = fixed_blobs{ };
		auto       composed = application::review_material_bridge{ &blobs, store.get(), local.Catalog.Authority };
		const auto pr       = composed.materialize(application::diff_blob_request{ }, { });
		const auto cycle    = composed.materialize(request, { });
		expect(pr.Status == review_material_status::available && pr.Bytes == std::vector<std::uint8_t>{ 'p', 'r' } && blobs.Calls == 1, "pull-request blobs pass through the existing port");
		expect(cycle.Bytes == manifest && blobs.Calls == 1, "cycle material never reaches the pull-request port");
		local.Runtime.shutdown();
		local.Lanes.drain();
	}
}

int main()
{
	initializes_and_operates_without_account_or_agent();
	switches_subjects_without_crossing_authority();
	leaves_existing_pull_request_state_untouched();
	routes_material_by_authority();
	return EXIT_SUCCESS;
}
