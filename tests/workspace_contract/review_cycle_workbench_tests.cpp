#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
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
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief The desktop's local cycle host (BPR-1157) against a real folder and
/// the real store: Open Project with no account, R0, full-source tabs, the
/// first note and suggestion, resume by root, restart with the same tabs, and
/// recents. BPR-1163: the manual iteration course, in which Capture revision
/// is the only way back, and the named views resolve from captures and the
/// reviewer's own checkpoints.

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
			// The whole file edited in place: two separate changes are one
			// note's two exact suggestions (owner amendment to BPR-1156).
			const auto edited  = std::string{ "int parse(std::string_view aText)\r\n{\r\n\treturn validate(aText);\r\n}\r\n" };
			auto       request = application::review_cycle_suggestion_request{ .Rationale = "Name it for what it does." };
			for (const auto& hunk : core::derive_edit_hunks(text(bytes), edited)) {
				const auto lines = core::make_anchor(source.Source->Revision, "src/parser.cpp", bytes, hunk.Start, hunk.End);
				expect(lines.has_value(), "each change anchors whole lines of the shown bytes");
				request.Edits.push_back({ .Selection = *lines, .Replacement = hunk.Replacement });
			}
			expect(request.Edits.size() == 2U, "two separate changes");
			desk1.Host->suggest(std::move(request));
			const auto& suggested = desk1.view();
			expect(suggested.Message.empty() && suggested.Notes.size() == 2U && suggested.Notes[1].Suggestions == 2U && suggested.Notes[1].Body == "Name it for what it does.", "one note with both suggestions");
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
			// `resumed` is the live projection, which close() empties.
			const auto subject = resumed.Cycle->Subject;
			desk2.Host->close();
			expect(!desk2.view().Cycle && desk2.view().Tabs.empty(), "closed");
			desk2.Host->resume(subject);
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

namespace {
	/// Freezes one round of the cycle over its first note, through the
	/// production runtime, while no desktop host holds the store.
	void freeze_round(const std::filesystem::path& aProfileRoot, const core::local_cycle_subject& aSubject, const core::cycle_source_revision_id aSource,
					  const std::uint8_t aSeed)
	{
		application::manual_executor      lanes;
		application::review_cycle_runtime runtime{ adapters::sqlite::make_review_cycle_store(aProfileRoot, profile), lanes, lanes };
		runtime.select(aSubject);
		auto operation = [aSeed](const std::uint8_t aIndex) {
			auto bytes = std::array<std::uint8_t, 32>{ };
			bytes[0]   = 0xF0;
			bytes[1]   = aSeed;
			bytes[2]   = aIndex;
			return application::cycle_operation_id{ bytes };
		};
		auto state = std::optional<application::cycle_store_result<std::optional<core::review_cycle_state>>>{ };
		runtime.load_selected([&](auto aValue) { state = std::move(aValue); });
		lanes.drain();
		expect(state && *state && **state, "cycle for the round");
		auto begun = std::optional<application::cycle_store_result<application::cycle_write_receipt>>{ };
		runtime.begin_round_selected(operation(1), (**state)->Revision, [&](auto aValue) { begun = std::move(aValue); });
		lanes.drain();
		expect(begun && *begun, "round draft begun");
		const auto draft     = core::cycle_round_draft_ref{ .Cycle = aSubject.Cycle, .Id = core::cycle_round_draft_id{ (*begun)->Allocated } };
		const auto selection = core::cycle_round_selection{ .Source = aSource, .Goal = "Handle the empty input.", .Items = { { .Item = core::cycle_feedback_item_id{ 1 }, .Wording = core::private_review_turn_revision_id{ 1 } } } };
		auto       saved     = std::optional<application::cycle_store_result<application::cycle_write_receipt>>{ };
		runtime.save_round_selected(operation(2), draft, 1, selection, [&](auto aValue) { saved = std::move(aValue); });
		lanes.drain();
		expect(saved && *saved, "draft saved");
		auto frozen = std::optional<application::cycle_store_result<application::cycle_round_freeze_result>>{ };
		runtime.freeze_round_selected({ .Operation = operation(3), .Draft = draft, .ExpectedDraftRevision = (*saved)->Revision, .Editor = { .ShownRevision = (*saved)->Revision } },
									  [&](auto aValue) { frozen = std::move(aValue); });
		lanes.drain();
		expect(frozen && *frozen && (*frozen)->Frozen, "round frozen");
		runtime.shutdown();
		lanes.drain();
	}

	auto rows_text(const application::review_cycle_comparison_view& aView) -> std::string
	{
		auto text = std::string{ };
		for (const auto& row : aView.Rows) {
			text += row.Kind == core::cycle_diff_row_kind::added ? '+' : row.Kind == core::cycle_diff_row_kind::removed ? '-'
																														: ' ';
			text += row.Text;
			text += '\n';
		}
		return text;
	}

	/// R0, F1, the recipient's ordinary edits, R1, F2 on the same source, R2:
	/// no receipt, response, agent or completion callback anywhere.
	void iterates_manually(const std::filesystem::path& aScratch, const std::optional<tests::workspace_contract::git_runner>& aGit)
	{
		tests::sqlite_contract::scratch_root profileScratch{ "cycle-iteration-profile" };
		const auto&                          profileRoot = profileScratch.path();
		const auto                           project     = aScratch / "iteration-project";
		write_file(project / "src" / "parser.cpp", "int parse(const char* aText)\n{\n\treturn check(aText);\n}\n");
		write_file(project / "README.md", "# Parser\n");
		if (aGit) {
			(*aGit)(project, { "init", "-q" });
			(*aGit)(project, { "add", "-A" });
			(*aGit)(project, { "commit", "-q", "-m", "start" });
		}
		auto subject = core::local_cycle_subject{ };
		{
			auto first = desk{ profileRoot, 11 };
			first.Host->open_project(project.string());
			expect(first.view().Revisions.size() == 1U && first.view().Cycle, "R0 on open");
			subject = first.view().Cycle->Subject;
			first.Host->comment({ .Primary = core::cycle_whole_target{ }, .Body = "Handle the empty input." });
			expect(first.view().Notes.size() == 1U, "a note for the round");
		}
		// F1 is frozen against R0 and handed over by hand; BPR is closed.
		const auto r1 = core::cycle_source_revision_id{ 1 };
		const auto r2 = core::cycle_source_revision_id{ 2 };
		const auto r3 = core::cycle_source_revision_id{ 3 };
		const auto r4 = core::cycle_source_revision_id{ 4 };
		freeze_round(profileRoot, subject, r1, 1);

		// The recipient edits with ordinary tools and sends nothing back.
		write_file(project / "src" / "parser.cpp", "int parse(const char* aText)\n{\n\tif (aText == nullptr) {\n\t\treturn 0;\n\t}\n\treturn check(aText);\n}\n");
		if (aGit) {
			(*aGit)(project, { "commit", "-q", "-a", "-m", "empty input" });
		}

		auto work = desk{ profileRoot, 12 };
		work.Host->open_project(project.string());
		const auto& reopened = work.view();
		expect(reopened.Revisions.size() == 1U && reopened.Facts.Rounds.size() == 1U && reopened.Facts.Rounds[0].Start == r1, "reopened: F1 is bound to R0, nothing captured automatically");
		expect(!reopened.Facts.LastReview && reopened.History && reopened.History->Entries.size() == 1U, "no review marked yet");
		work.Host->open_file("src/parser.cpp", false);
		const auto before = snapshot(project);
		work.Host->capture_revision();
		const auto& captured = work.view();
		expect(snapshot(project) == before, "capturing never writes the folder");
		expect(captured.Revisions.size() == 2U && captured.Message.empty() && captured.Notice.empty(), "R1 captured by the reviewer's gesture alone");
		expect(captured.Source && captured.Source->Revision == r1 && captured.NewerAvailable, "the open tab keeps R0 and says newer material exists");

		// This round: F1's start against the capture.
		work.Host->compare(core::this_round_request{ .Round = core::cycle_round_id{ 1 } });
		const auto& round = work.view();
		expect(round.Comparison && round.Comparison->Pair.Left && round.Comparison->Pair.Left->Id == r1 && round.Comparison->Pair.Right.Id == r2 && !round.Comparison->Pinned, "this round resolves R0 to R1");
		expect(!round.Source && round.Comparison->Status == application::review_material_status::available && rows_text(*round.Comparison).find("+\tif (aText == nullptr) {\n") != std::string::npos,
			   "the shared engine's rows from retained bytes");
		expect(!round.NewerAvailable, "nothing newer than this round's view");
		work.Host->compare(core::since_last_review_request{ });
		expect(!work.view().Message.empty() && work.view().Comparison && work.view().Comparison->Pair.Right.Id == r2, "no review marked: refused in words, the view unchanged");
		work.Host->mark_reviewed();
		expect(work.view().Facts.LastReview && work.view().Facts.LastReview->Revision == r2, "the reviewer marks R1 reviewed");
		work.Host->compare(core::since_last_review_request{ });
		expect(work.view().Comparison && work.view().Comparison->Pair.Left->Id == r2 && work.view().Comparison->Rows.size() == 7U, "since my last review: nothing new");

		// Capturing unchanged bytes records nothing, and says so.
		work.Host->capture_revision();
		expect(work.view().Revisions.size() == 2U && work.view().Notice == "No change since R2; nothing new was recorded.", "no-change capture");
		work.Host->capture_revision(true);
		expect(work.view().Revisions.size() == 3U && work.view().Notice.empty(), "a deliberate handoff observation of the same bytes");

		// A pinned pair; then the recipient branches and edits again.
		work.Host->compare(core::chosen_pair_request{ .Left = r1, .Right = r2 });
		expect(work.view().Comparison && work.view().Comparison->Pinned, "chosen revisions are pinned");
		write_file(project / "src" / "parser.cpp", "int parse(const char* aText)\n{\n\treturn aText == nullptr ? 0 : check(aText);\n}\n");
		if (aGit) {
			(*aGit)(project, { "checkout", "-q", "-b", "feature" });
			(*aGit)(project, { "commit", "-q", "-a", "-m", "shorter" });
		}
		work.Host->capture_revision();
		const auto& pinned = work.view();
		expect(pinned.Revisions.size() == 4U && pinned.Comparison && pinned.Comparison->Pair.Left->Id == r1 && pinned.Comparison->Pair.Right.Id == r2 && pinned.NewerAvailable,
			   "a pinned pair never retargets; newer material is signalled");
		work.Host->refresh_view();
		expect(!work.view().Message.empty() && work.view().Comparison->Pair.Right.Id == r2, "refresh leaves a chosen pair where it is");
		work.Host->compare(core::all_cycle_request{ });
		expect(work.view().Comparison->Pair.Left->Id == r1 && work.view().Comparison->Pair.Right.Id == r4 && !work.view().NewerAvailable, "all cycle changes: R0 to R3");
		work.Host->compare(core::this_round_request{ .Round = core::cycle_round_id{ 1 }, .Result = r2 });
		expect(work.view().Comparison->Pair.Right.Id == r2 && work.view().Comparison->Pinned, "this round with a named result");

		// The chronologies stay apart.
		const auto& history = *work.view().History;
		expect(history.Entries.size() == 4U && !history.ProviderChronology && history.Unlisted == 0U, "four captures, no provider order");
		expect(history.Entries[2].SameMaterial && !history.Entries[1].SameMaterial, "the deliberate capture holds the same bytes");
		if (aGit) {
			expect(history.Entries[1].Step == core::cycle_revision_step::head_moved && history.Entries[2].Step == core::cycle_revision_step::same_head
					   && history.Entries[3].Step == core::cycle_revision_step::branch_changed && history.Entries[3].HeadReference == "refs/heads/feature",
				   "head moved (intermediate commits unobserved), same head, then another branch");
		} else {
			expect(history.Entries[1].Step == core::cycle_revision_step::no_repository, "a plain folder has no repository chronology");
		}

		// Full source again, at the right side it showed.
		work.Host->full_source();
		expect(work.view().Source && work.view().Source->Revision == r2 && !work.view().Comparison && work.view().NewerAvailable, "back to full source");
		work.Host->refresh_view();
		expect(work.view().Source && work.view().Source->Revision == r4 && !work.view().NewerAvailable, "refresh moves full source to the latest only on request");
		work.Host->compare(core::chosen_pair_request{ .Left = r2, .Right = r3 });
		const auto kept = work.view().Tabs;

		// A capture completing after the cycle closed changes nothing shown.
		work.Host->capture_revision(true);
		work.Host->close();
		expect(!work.view().Cycle && work.view().Tabs.empty(), "closed; the late capture completion is dropped");

		// Restart: the same views, the same checkpoint.
		auto again = desk{ profileRoot, 13 };
		again.Host->open_project(project.string());
		const auto& restored = again.view();
		expect(restored.Tabs.size() == kept.size() && restored.Tabs[0].View == kept[0].View, "tabs and their comparisons restored");
		expect(restored.Comparison && restored.Comparison->Pair.Left->Id == r2 && restored.Comparison->Pair.Right.Id == r3, "the pinned pair after a restart");
		expect(restored.Facts.LastReview && restored.Facts.LastReview->Revision == r2 && restored.Facts.Rounds.size() == 1U, "checkpoint and round after a restart");
		expect(restored.Revisions.size() == 4U, "the capture superseded by closing recorded nothing");
		std::cout << "NATIVE: manual iteration R0/F1/R1/R2 with capture, this round, since my last review, all cycle, pinned pair" << (aGit ? " and Git chronology\n" : "\n");
	}
}

auto main(const int aArgc, char** aArgv) -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-workbench" };
	opens_a_folder_to_a_first_note(scratch.path());
	iterates_manually(scratch.path(), tests::workspace_contract::git_from(aArgc, aArgv));
	std::cout << "review-cycle-workbench-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle workbench failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
