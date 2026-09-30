#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_workbench.hpp"
#include "workspace_fixture.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief The desktop's local cycle host (BPR-1157) against a real folder and
/// the real store: Open Project with no account, R0, full-source tabs, the
/// first note and suggestion, resume by root, restart with the same tabs, and
/// recents.

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::write_file;

	constexpr auto profile = core::bpr_profile_id{ 4 };

	struct recording_port final : application::review_cycle_workbench_port
	{
		application::review_cycle_workbench_projection Last;
		std::vector<application::review_cycle_recent>  Recents;
		std::size_t                                    Publications = 0;
		void                                           publish(const application::review_cycle_workbench_projection& aProjection) override
		{
			Last = aProjection;
			++Publications;
		}
		void publish(const std::vector<application::review_cycle_recent>& aRecents) override
		{
			Recents = aRecents;
		}
	};

	/// A host over the profile root, drained on manual lanes.
	struct desk
	{
		application::manual_executor                         Lanes;
		std::unique_ptr<application::workspace_source_port>  Source = adapters::workspace::make_local_workspace_source();
		recording_port                                       Port;
		std::unique_ptr<application::review_cycle_workbench> Host;

		explicit desk(const std::filesystem::path& aProfileRoot, const std::uint8_t aSeed)
		{
			auto seed = std::array<std::uint8_t, 32>{ };
			seed[0]   = aSeed;
			Host      = std::make_unique<application::review_cycle_workbench>(adapters::sqlite::make_review_cycle_store(aProfileRoot, profile), *Source, Lanes, Lanes, Port, seed);
		}
		desk(const desk&)                    = delete;
		auto operator=(const desk&) -> desk& = delete;
		desk(desk&&)                         = delete;
		auto operator=(desk&&) -> desk&      = delete;
		~desk()
		{
			Host->shutdown();
			Lanes.drain();
		}
		auto view() -> const application::review_cycle_workbench_projection&
		{
			Lanes.drain();
			return Host->projection();
		}
	};

	auto snapshot(const std::filesystem::path& aRoot) -> std::map<std::string, std::string>
	{
		auto files = std::map<std::string, std::string>{ };
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ aRoot }) {
			if (entry.is_regular_file()) {
				files.emplace(entry.path().lexically_relative(aRoot).generic_string(), read_file(entry.path()));
			}
		}
		return files;
	}

	auto text(const std::vector<std::uint8_t>& aBytes) -> std::string
	{
		return { aBytes.begin(), aBytes.end() };
	}

	void opens_a_folder_to_a_first_note(const std::filesystem::path& aScratch)
	{
		// The profile root is owner-private, as the store requires.
		tests::sqlite_contract::scratch_root profileScratch{ "cycle-workbench-profile" };
		const auto&                          profileRoot = profileScratch.path();
		const auto                           project     = aScratch / "parser-project";
		write_file(project / "README.md", "# Parser\n");
		write_file(project / "src" / "parser.cpp", "int parse(const char* aText)\r\n{\r\n\treturn check(aText);\r\n}\r\n");
		write_file(project / "bin" / "blob.bin", std::string_view{ "\0\1\2", 3 });
		const auto before = snapshot(project);

		{
			auto desk1 = desk{ profileRoot, 1 };
			desk1.Host->open_project(project.string());
			const auto& opened = desk1.view();
			expect(opened.Message.empty() && opened.Activity == application::review_cycle_activity::idle, "opened without an account, provider or agent");
			expect(opened.Cycle && opened.Cycle->Title == "parser-project" && opened.Cycle->Captured && opened.Revisions.size() == 1U, "default title and R0");
			expect(opened.Files.size() == 3U && opened.Tabs.size() == 1U && opened.Source && opened.Source->Path == "README.md", "full source: the first readable file in one pane");
			expect(text(opened.Source->Bytes) == "# Parser\n" && opened.Source->Status == application::review_material_status::available, "retained bytes shown");

			// Open the parser in the active tab and note a range from its retained bytes.
			desk1.Host->open_file("src/parser.cpp", false);
			const auto& source = desk1.view();
			expect(source.Tabs.size() == 1U && source.Source && source.Source->Path == "src/parser.cpp", "same tab, other file");
			const auto bytes  = source.Source->Bytes;
			const auto start  = text(bytes).find("check(aText)");
			const auto anchor = core::make_anchor(source.Source->Revision, "src/parser.cpp", bytes, start, start + 12U);
			expect(anchor.has_value(), "anchor from the shown bytes");
			desk1.Host->comment({ .Primary = *anchor, .Body = "Why check here?" });
			expect(desk1.view().Notes.size() == 1U && desk1.view().Notes[0].Body == "Why check here?" && desk1.view().Notes[0].Suggestions == 0U, "first note");
			desk1.Host->suggest({ .Selection = *anchor, .Replacement = "validate(aText)", .Rationale = "Name it for what it does." });
			const auto& suggested = desk1.view();
			expect(suggested.Notes.size() == 2U && suggested.Notes[1].Suggestions == 1U && suggested.Notes[1].Body == "Name it for what it does.", "note with a suggestion");
			// One discussion across files: the README heading joins the first note.
			desk1.Host->open_file("README.md", false);
			const auto readme  = desk1.view().Source->Bytes;
			const auto heading = core::make_anchor(desk1.view().Source->Revision, "README.md", readme, 2, 8);
			desk1.Host->relate(desk1.view().Notes[0].Item, desk1.view().Notes[0].Version, { .Target = *heading });
			expect(desk1.view().Notes[0].Related.size() == 1U && desk1.view().Message.empty(), "related location added");
			desk1.Host->open_file("src/parser.cpp", false);
			desk1.Host->comment({ .Primary = core::cycle_whole_target{ }, .Body = "Overall fine." });
			expect(desk1.view().Notes.size() == 3U, "a cycle note needs no line");

			// A second tab, a reading line, and a gesture comparisons belong to later.
			desk1.Host->open_file("README.md", true);
			desk1.Host->observe_line(*desk1.view().ActiveTab, 2U);
			expect(desk1.view().Tabs.size() == 2U && desk1.view().Source->Path == "src/parser.cpp", "background tab");
			desk1.Host->tab(core::select_tab_comparison_intent{ });
			expect(desk1.view().Tabs.size() == 2U, "comparison gestures change nothing here");
		}
		expect(snapshot(project) == before, "opening, reading and noting never write the folder");

		// Restart: a new host finds the same cycle by its root and restores the tabs.
		{
			auto desk2 = desk{ profileRoot, 2 };
			desk2.Host->refresh_recents();
			desk2.Lanes.drain();
			expect(desk2.Port.Recents.size() == 1U && desk2.Port.Recents[0].Cycle.Title == "parser-project" && desk2.Port.Recents[0].Location == project.string(), "recents after restart");
			desk2.Host->open_project(project.string());
			const auto& resumed = desk2.view();
			expect(resumed.Cycle && resumed.Cycle->Subject.Cycle == core::review_cycle_id{ 1 } && resumed.Revisions.size() == 1U, "same cycle, no second capture");
			expect(resumed.Tabs.size() == 2U && resumed.Tabs[0].View.Path == "src/parser.cpp" && resumed.Tabs[0].View.Line == 2U && resumed.Source && resumed.Source->Path == "src/parser.cpp", "tabs and reading line restored");
			expect(resumed.Notes.size() == 3U, "notes restored");
			desk2.Host->close();
			expect(!desk2.view().Cycle && desk2.view().Tabs.empty(), "closed");
			desk2.Host->resume(resumed.Cycle->Subject);
			expect(desk2.view().Cycle && desk2.view().Notes.size() == 3U, "resumed from recents");
		}

		// A missing folder is explained, not created.
		{
			auto desk3 = desk{ profileRoot, 3 };
			desk3.Host->open_project((aScratch / "no-such-folder").string());
			expect(!desk3.view().Cycle && !desk3.view().Message.empty(), "missing folder refused with words");
		}
		std::cout << "NATIVE: no-account Open Project to R0 full source, first note and suggestion, restart with tabs\n";
	}
}

auto main() -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-workbench" };
	opens_a_folder_to_a_first_note(scratch.path());
	std::cout << "review-cycle-workbench-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle workbench failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
