#ifndef BPR_TESTS_WORKSPACE_CONTRACT_CYCLE_COURSE_HPP
#define BPR_TESTS_WORKSPACE_CONTRACT_CYCLE_COURSE_HPP

/// \file
/// \brief A real cycle store with one cycle bound to one real root, for the
/// capture and comparison contracts.

#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/application/review_cycle_capture.hpp"
#include "bpr/core/sha256.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::tests::workspace_contract {
	inline constexpr auto profile = core::bpr_profile_id{ 3 };

	inline auto operation(const std::uint8_t aSeed) -> application::cycle_operation_id
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = 0x3C;
		bytes[31]  = aSeed;
		return application::cycle_operation_id{ bytes };
	}

	inline auto bytes_of(const std::string_view aText) -> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	inline auto digest_of(const std::string_view aText) -> core::cycle_material_digest
	{
		return core::cycle_material_digest{ core::sha256(bytes_of(aText)) };
	}

	/// A store, a cycle bound to one root, and helpers to capture it.
	struct course
	{
		/// The profile root: owner-private, as the store requires.
		tests::sqlite_contract::scratch_root             StoreRoot{ "cycle-capture-store" };
		std::unique_ptr<application::review_cycle_store> Store;
		core::review_cycle_catalog                       Catalog;
		core::local_cycle_subject                        Subject;
		std::uint8_t                                     NextOperation = 1;

		course(application::workspace_source_port& aSource, const std::filesystem::path& aWorkspace, const std::vector<std::string>& aScope = { },
			   const std::uint64_t aBudget = application::default_cycle_retained_budget_bytes)
			: Store{ adapters::sqlite::make_review_cycle_store(StoreRoot.path(), profile, { .RetainedBudgetBytes = aBudget }) }
		{
			auto catalog = Store->catalog({ });
			expect(catalog.has_value(), "catalog");
			Catalog            = *catalog;
			const auto created = Store->create(operation(NextOperation++), { .Authority = Catalog.Authority, .Title = "Capture", .Scope = { .Paths = aScope } }, { });
			expect(created.has_value(), "create cycle");
			Subject         = { .Authority = Catalog.Authority, .Cycle = created->Cycle, .Generation = 1 };
			auto       host = application::review_cycle_capture_host{ aSource, *Store };
			const auto root = host.observe(aWorkspace.string(), { });
			expect(root.has_value(), "observe root");
			const auto bound = Store->apply(operation(NextOperation++), { .Subject = Subject, .ExpectedRevision = 1, .Change = core::bind_cycle_workspace{ .Root = *root } }, { });
			expect(bound && bound->Changed, "bind root");
		}

		auto state() -> core::review_cycle_state
		{
			const auto loaded = Store->load(Subject, { });
			expect(loaded && *loaded, "load cycle");
			return **loaded;
		}

		auto capture(application::workspace_source_port& aSource, application::cycle_capture_request aRequest = { }, std::stop_token aStop = { }) -> application::cycle_capture_outcome
		{
			const auto current        = state();
			aRequest.Operation        = operation(NextOperation++);
			aRequest.ExpectedRevision = current.Revision;
			auto host                 = application::review_cycle_capture_host{ aSource, *Store };
			return host.capture(current, aRequest, std::move(aStop));
		}

		auto manifest(const application::cycle_capture_outcome& aOutcome) -> core::cycle_manifest
		{
			expect(aOutcome.Manifest.has_value(), "manifest recorded");
			const auto bytes = Store->read_object(*aOutcome.Manifest, { });
			expect(bytes.has_value(), "manifest readable");
			auto decoded = core::decode_manifest(*bytes);
			expect(decoded.has_value(), "manifest decodes");
			return std::move(*decoded);
		}

		auto content(const core::manifest_entry& aEntry) -> std::string
		{
			expect(aEntry.Content.has_value(), "entry has content");
			const auto bytes = Store->read_object({ .Digest = *aEntry.Content, .Size = aEntry.Size }, { });
			expect(bytes.has_value(), "content retained");
			return { bytes->begin(), bytes->end() };
		}

		auto revisions() -> std::size_t
		{
			const auto page = Store->revisions(Subject, std::nullopt, { });
			expect(page.has_value(), "revision page");
			return page->Revisions.size();
		}

		void rescope(const std::vector<std::string>& aPaths)
		{
			const auto current = state();
			const auto changed = Store->apply(operation(NextOperation++), { .Subject = Subject, .ExpectedRevision = current.Revision, .Change = core::change_cycle_scope{ .Paths = aPaths } }, { });
			expect(changed && changed->Changed, "rescope");
		}
	};

	inline auto entry(const core::cycle_manifest& aManifest, std::string_view aPath) -> const core::manifest_entry&
	{
		const auto* found = core::find_entry(aManifest, aPath);
		expect(found != nullptr, "manifest entry present");
		return *found;
	}

}

#endif
