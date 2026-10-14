#include "../support/test_expectation.hpp"
#include "product_diff.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "cycle_course.hpp"
#include "workspace_fixture.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using application::review_material_status;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::operation;
	using tests::workspace_contract::profile;
	using tests::workspace_contract::write_file;

	auto text(const std::vector<std::uint8_t>& aBytes) -> std::string
	{
		return { aBytes.begin(), aBytes.end() };
	}

	void sql(const std::filesystem::path& aDatabase, const char* aStatement)
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open(aDatabase.string().c_str(), &database) == SQLITE_OK, "open store for fault injection");
		expect(sqlite3_exec(database, aStatement, nullptr, nullptr, nullptr) == SQLITE_OK, "fault injection statement");
		sqlite3_close(database);
	}

	/// R0 and R1 of one folder, with the views over them.
	struct two_revisions
	{
		std::filesystem::path    Root;
		course                   Cycle;
		core::review_cycle_state State;

		two_revisions(application::workspace_source_port& aSource, const std::filesystem::path& aRoot)
			: Root{ written(aRoot) }
			, Cycle{ aSource, Root }
		{
			expect(Cycle.capture(aSource).Status == cycle_capture_status::captured, "R0");
			write_file(Root / "src" / "main.cpp", "int main()\n{\n\treturn 1;\n}\n");
			write_file(Root / "src" / "added.cpp", "// added\n");
			std::filesystem::remove(Root / "src" / "gone.cpp");
			std::filesystem::rename(Root / "docs" / "old-name.md", Root / "docs" / "new-name.md");
			expect(Cycle.capture(aSource).Status == cycle_capture_status::captured, "R1");
			State = Cycle.state();
		}

		static auto written(const std::filesystem::path& aRoot) -> std::filesystem::path
		{
			write_file(aRoot / "src" / "main.cpp", "int main()\n{\n\treturn 0;\n}\n");
			write_file(aRoot / "src" / "gone.cpp", "// removed later\n");
			write_file(aRoot / "docs" / "old-name.md", "# Same bytes, new name\n");
			write_file(aRoot / "docs" / "guide.md", "# Guide\n");
			return aRoot;
		}

		[[nodiscard]] auto all_cycle() const -> core::cycle_comparison_pair
		{
			return *core::resolve_comparison(State, core::all_cycle_view{ });
		}
		[[nodiscard]] auto full_source(const std::uint64_t aRevision) const -> core::cycle_comparison_pair
		{
			return *core::resolve_comparison(State, core::full_source_view{ core::cycle_source_revision_id{ aRevision } });
		}
	};

	auto find(const core::cycle_comparison_page& aPage, const std::string& aPath) -> const core::cycle_path_comparison*
	{
		for (const auto& item : aPage.Entries) {
			if (item.Path == aPath) {
				return &item;
			}
		}
		return nullptr;
	}

	void serves_full_source_and_exact_pairs(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		auto       views  = two_revisions{ *source, aScratch / "views" };
		auto       host   = application::review_cycle_comparison_host{ *views.Cycle.Store, views.Cycle.Catalog.Authority, bpr::tests::product_line_matcher() };
		const auto page   = host.page(views.Cycle.Subject, views.all_cycle(), std::nullopt, 64, true, { });
		expect(page.Status == review_material_status::available && page.Page, "all-cycle page");
		expect(find(*page.Page, "src/main.cpp")->Change == core::cycle_path_change::modified, "modified");
		expect(find(*page.Page, "src/added.cpp")->Change == core::cycle_path_change::added, "added");
		expect(find(*page.Page, "src/gone.cpp")->Change == core::cycle_path_change::deleted, "deleted from a complete capture");
		expect(find(*page.Page, "docs/guide.md") == nullptr, "unchanged paths skipped");
		const auto* renamed = find(*page.Page, "docs/new-name.md");
		expect(renamed != nullptr && find(*page.Page, "docs/old-name.md")->Left->Content == renamed->Right.Content, "content-equivalent paths share content identity");

		// Full source is the retained bytes of one revision, not a diff.
		const auto original = host.file(views.Cycle.Subject, views.full_source(1), "src/main.cpp", { }, { });
		expect(original.Status == review_material_status::available && !original.Diff && text(original.Source) == "int main()\n{\n\treturn 0;\n}\n", "R0 full source");
		const auto current = host.file(views.Cycle.Subject, views.full_source(2), "src/main.cpp", { }, { });
		expect(text(current.Source) == "int main()\n{\n\treturn 1;\n}\n", "R1 full source");

		// A pair goes through the shared engine.
		const auto diff = host.file(views.Cycle.Subject, views.all_cycle(), "src/main.cpp", { }, { });
		expect(diff.Status == review_material_status::available && diff.Diff && core::validate_computed_material_diff(*diff.Diff).valid(), "shared-engine diff");
		expect(diff.Diff->Alignment.LeftLineCount == 4 && diff.Diff->Alignment.RightLineCount == 4 && !diff.Diff->Alignment.Groups.empty(), "one changed line in four");
		const auto added = host.file(views.Cycle.Subject, views.all_cycle(), "src/added.cpp", { }, { });
		expect(added.Diff && !added.Diff->LeftDocument && added.Diff->RightDocument, "an added file has no left document");

		// Paging through the whole pair visits every path once.
		auto seen  = std::vector<std::string>{ };
		auto after = std::optional<std::string>{ };
		for (auto guard = 0; guard < 20; ++guard) {
			const auto next = host.page(views.Cycle.Subject, views.all_cycle(), after, 2, false, { });
			for (const auto& item : next.Page->Entries) {
				seen.push_back(item.Path);
			}
			if (!next.Page->Next) {
				break;
			}
			after = next.Page->Next;
		}
		expect(seen.size() == 6, "six paths across bounded pages");
		auto stop = std::stop_source{ };
		stop.request_stop();
		expect(host.page(views.Cycle.Subject, views.all_cycle(), std::nullopt, 64, false, stop.get_token()).Status == review_material_status::cancelled, "cancelled page");
		std::cout << "NATIVE: full source, exact pair, paging and cancellation over retained material\n";
	}

	void resolves_retained_bytes_only(const std::filesystem::path& aScratch)
	{
		auto source = adapters::workspace::make_local_workspace_source();
		auto views  = two_revisions{ *source, aScratch / "retained" };

		// External edits and deletion of the workspace change nothing shown.
		write_file(views.Root / "src" / "main.cpp", "edited after capture\n");
		std::filesystem::remove_all(views.Root / "docs");
		{
			auto host = application::review_cycle_comparison_host{ *views.Cycle.Store, views.Cycle.Catalog.Authority, bpr::tests::product_line_matcher() };
			expect(text(host.file(views.Cycle.Subject, views.full_source(1), "src/main.cpp", { }, { }).Source) == "int main()\n{\n\treturn 0;\n}\n", "not the edited file");
			expect(text(host.file(views.Cycle.Subject, views.full_source(1), "docs/guide.md", { }, { }).Source) == "# Guide\n", "not the deleted file");
		}

		// After a restart: a new store object on the same profile.
		views.Cycle.Store.reset();
		auto reopened = adapters::sqlite::make_review_cycle_store(views.Cycle.StoreRoot.path(), profile);
		{
			auto host = application::review_cycle_comparison_host{ *reopened, views.Cycle.Catalog.Authority, bpr::tests::product_line_matcher() };
			expect(text(host.file(views.Cycle.Subject, views.full_source(2), "src/main.cpp", { }, { }).Source) == "int main()\n{\n\treturn 1;\n}\n", "after restart");
		}

		// Corrupted and missing pins are reported, never replaced by live files.
		const auto database = views.Cycle.StoreRoot.path() / "state" / "v1" / "review-cycles.sqlite3";
		const auto guide    = tests::workspace_contract::digest_of("# Guide\n");
		auto       hex      = std::string{ };
		for (const auto byte : guide.bytes()) {
			constexpr auto digits = std::string_view{ "0123456789ABCDEF" };
			hex += digits[byte >> 4U];
			hex += digits[byte & 0x0FU];
		}
		write_file(views.Root / "docs" / "guide.md", "# Guide\n");
		sql(database, ("UPDATE object_chunks SET bytes=X'00' WHERE digest=X'" + hex + "'").c_str());
		auto       host    = application::review_cycle_comparison_host{ *reopened, views.Cycle.Catalog.Authority, bpr::tests::product_line_matcher() };
		const auto corrupt = host.file(views.Cycle.Subject, views.full_source(1), "docs/guide.md", { }, { });
		expect(corrupt.Status == review_material_status::corrupt && corrupt.Source.empty(), "corruption reported with no bytes");
		const auto main = tests::workspace_contract::digest_of("int main()\n{\n\treturn 1;\n}\n");
		hex.clear();
		for (const auto byte : main.bytes()) {
			constexpr auto digits = std::string_view{ "0123456789ABCDEF" };
			hex += digits[byte >> 4U];
			hex += digits[byte & 0x0FU];
		}
		sql(database, ("DELETE FROM object_chunks WHERE digest=X'" + hex + "'; DELETE FROM objects WHERE digest=X'" + hex + "'").c_str());
		const auto missing = host.file(views.Cycle.Subject, views.full_source(2), "src/main.cpp", { }, { });
		expect(missing.Status == review_material_status::missing && missing.Source.empty(), "a missing pin is reported, not read from disk");
		const auto diff = host.file(views.Cycle.Subject, views.all_cycle(), "src/main.cpp", { }, { });
		expect(diff.Status == review_material_status::missing && !diff.Diff, "no diff over missing material");
		std::cout << "NATIVE: retained bytes survive edits, deletion and restart; corrupt and missing pins reported\n";
	}

	void supersedes_late_work_through_the_runtime(const std::filesystem::path& aScratch)
	{
		auto source = adapters::workspace::make_local_workspace_source();
		auto views  = two_revisions{ *source, aScratch / "runtime" };
		views.Cycle.Store.reset();
		auto lanes   = application::manual_executor{ };
		auto runtime = application::review_cycle_runtime{ adapters::sqlite::make_review_cycle_store(views.Cycle.StoreRoot.path(), profile), lanes, lanes, nullptr, bpr::tests::product_line_matcher() };
		runtime.select(views.Cycle.Subject);

		auto page = std::optional<application::cycle_store_result<application::cycle_comparison_page_observation>>{ };
		expect(runtime.compare_selected(views.all_cycle(), std::nullopt, 16, true, [&](auto aValue) { page = std::move(aValue); }), "page admitted");
		lanes.drain();
		expect(page && *page && (*page)->Page && (*page)->Page->Entries.size() == 5, "page through the runtime");

		// A file request issued for one selection completes as cancelled
		// after the selection changes; the core view state drops it.
		auto state            = core::cycle_file_view_state{ };
		auto [loading, first] = core::request_file(state, { .Pair = views.all_cycle(), .Path = "src/main.cpp" });
		auto late             = std::optional<application::cycle_store_result<application::cycle_file_observation>>{ };
		expect(runtime.compare_file_selected(views.all_cycle(), "src/main.cpp", { }, [&](auto aValue) { late = std::move(aValue); }), "file admitted");
		runtime.select(core::review_subject{ views.Cycle.Subject });
		auto [switched, second] = core::request_file(loading, { .Pair = views.full_source(1), .Path = "src/main.cpp" });
		lanes.drain();
		expect(late && !*late && late->error().Failure == application::cycle_store_failure::cancelled, "superseded work completes cancelled");
		expect(core::complete_file(switched, first, true).second == core::cycle_file_view_result::stale, "and a late completion would be dropped");
		auto current = std::optional<application::cycle_store_result<application::cycle_file_observation>>{ };
		expect(runtime.compare_file_selected(views.full_source(1), "src/main.cpp", { }, [&](auto aValue) { current = std::move(aValue); }), "current request");
		lanes.drain();
		expect(current && *current && (*current)->Status == review_material_status::available, "current request served");
		expect(core::complete_file(switched, second, true).second == core::cycle_file_view_result::shown, "and shown");
		runtime.shutdown();
	}

	void widens_scope_without_inventing_additions(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "widen";
		write_file(root / "src" / "a.cpp", "a\n");
		write_file(root / "docs" / "b.md", "b\n");
		auto cycle = course{ *source, root, { "src" } };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "narrow R0");
		cycle.rescope({ });
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "wide R1");
		auto        host  = application::review_cycle_comparison_host{ *cycle.Store, cycle.Catalog.Authority, bpr::tests::product_line_matcher() };
		const auto  state = cycle.state();
		const auto  page  = host.page(cycle.Subject, *core::resolve_comparison(state, core::all_cycle_view{ }), std::nullopt, 64, false, { });
		const auto* docs  = find(*page.Page, "docs/b.md");
		expect(docs != nullptr && docs->Change == core::cycle_path_change::unknown && docs->Left->State == core::cycle_path_state::outside_scope, "newly in scope is unknown, not added");
		expect(host.file(cycle.Subject, *core::resolve_comparison(state, core::all_cycle_view{ }), "docs/b.md", { }, { }).Status == review_material_status::missing, "no diff against material never captured");
	}
}

auto main() -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-comparison" };
	serves_full_source_and_exact_pairs(scratch.path());
	resolves_retained_bytes_only(scratch.path());
	supersedes_late_work_through_the_runtime(scratch.path());
	widens_scope_without_inventing_additions(scratch.path());
	std::cout << "review-cycle-comparison-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return tests::failing_exit();
}
