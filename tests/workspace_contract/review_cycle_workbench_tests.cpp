#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_export_destination.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "bpr/application/review_cycle_workbench.hpp"
#include "bpr/core/review_cycle_bundle.hpp"
#include "bpr/core/sha256.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
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
/// reviewer's own checkpoints. BPR-1161: Compose feedback and Share
/// feedback, from the first draft to Copy, Save, a bundle, an amendment, a
/// refused channel, an optional route and output over the clipboard's limit.

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

namespace {
	/// The platform clipboard's double: it keeps what was written and
	/// answers as told.
	struct clipboard_double final : application::review_cycle_clipboard
	{
		outcome                  Answer = outcome::confirmed;
		std::vector<std::string> Written;
		auto                     write(const std::string_view aText) -> outcome override
		{
			Written.emplace_back(aText);
			return Answer;
		}
	};

	auto offer(const application::review_cycle_share_view& aShare, const core::cycle_share_route aRoute) -> std::optional<core::cycle_share_route_offer>
	{
		for (const auto& route : aShare.Routes) {
			if (route.Route == aRoute) {
				return route;
			}
		}
		return std::nullopt;
	}

	auto digest_of(const std::string_view aBytes) -> core::cycle_material_digest
	{
		return core::cycle_material_digest{ core::sha256({ reinterpret_cast<const std::uint8_t*>(aBytes.data()), aBytes.size() }) };
	}

	/// A desk whose Share feedback reaches a clipboard double and chosen files.
	void connect(desk& aDesk, clipboard_double& aClipboard)
	{
		aDesk.Host->set_clipboard(&aClipboard);
		aDesk.Host->set_destination_factory([](const std::string& aPath) { return adapters::workspace::make_local_export_destination(std::filesystem::path{ aPath }); });
	}

	/// RC56-07, RC56-10, RC61-05, RC61-06 through the desktop host, the real
	/// store and real files.
	void composes_and_shares_one_round(const std::filesystem::path& aScratch)
	{
		tests::sqlite_contract::scratch_root profileScratch{ "cycle-share-profile" };
		const auto&                          profileRoot = profileScratch.path();
		const auto                           project     = aScratch / "share-project";
		const auto                           handoff     = aScratch / "share-handoff";
		write_file(project / "README.md", "# Parser\n");
		write_file(project / "src" / "parser.cpp", "int parse(const char* aText)\n{\n\treturn check(aText);\n}\n");
		std::filesystem::create_directories(handoff);

		auto clipboard = clipboard_double{ };
		auto work      = desk{ profileRoot, 21 };
		connect(work, clipboard);
		work.Host->open_project(project.string());
		work.Lanes.drain();
		work.Host->open_file("src/parser.cpp", false);
		const auto source = work.view().Source->Bytes;
		expect(work.view().Source->Path == "src/parser.cpp", "the parser is shown");
		const auto at = text(source).find("check(aText)");
		work.Host->comment({ .Primary = *core::make_anchor(work.view().Source->Revision, "src/parser.cpp", source, at, at + 12U), .Body = "Why check here?" });
		work.Lanes.drain();
		work.Host->comment({ .Primary = core::cycle_whole_target{ }, .Body = "Keep the parser free of I/O." });
		work.Lanes.drain();
		auto request = application::review_cycle_suggestion_request{ .Rationale = "Name it for what it does." };
		for (const auto& hunk : core::derive_edit_hunks(text(source), "int parse(const char* aText)\n{\n\treturn validate(aText);\n}\n")) {
			request.Edits.push_back({ .Selection = *core::make_anchor(work.view().Source->Revision, "src/parser.cpp", source, hunk.Start, hunk.End), .Replacement = hunk.Replacement });
		}
		work.Host->suggest(std::move(request));
		expect(work.view().Notes.size() == 3U && work.view().Message.empty(), "three notes, one with a suggestion");
		const auto before = snapshot(project);

		// Compose feedback: one complete assignment of every open note, with no
		// setup, no second prompt and nothing frozen or sent.
		work.Host->compose();
		const auto& composed = *work.view().Share;
		expect(composed.Items == 3U && composed.Suggestions == 1U && composed.Selection.Items.size() == 3U && !composed.Frozen && !composed.Amends && composed.Round == core::cycle_round_id{ 1 },
			   "the first draft holds every open note, not yet frozen");
		expect(composed.DocumentShown && composed.DocumentBytes == composed.Document.size() && composed.Document.find("Why check here?") != std::string::npos
				   && composed.Document.find("Keep the parser free of I/O.") != std::string::npos && composed.Document.find("validate(aText)") != std::string::npos,
			   "the preview is the readable document, every note and the exact replacement");
		expect(composed.DocumentName == "RC-1-F1.md" && composed.BundleName == "RC-1-F1.bpr-review.zip", "file names from identities only");
		expect(composed.Routes.size() == 3U && offer(composed, core::cycle_share_route::copy_document)->offered() && offer(composed, core::cycle_share_route::save_document)->offered()
				   && offer(composed, core::cycle_share_route::save_bundle)->offered(),
			   "Copy and Save first-class, with no agent route and no disabled agent banner");
		expect(work.view().Facts.Rounds.empty() && composed.Deliveries.empty() && clipboard.Written.empty(), "composing froze and sent nothing");
		const auto preview = composed.Document;
		const auto f1      = *composed.Digest;

		// Copy: freeze and copy in one gesture; the bytes are the preview's.
		work.Host->share(core::cycle_share_route::copy_document);
		const auto& copied = *work.view().Share;
		expect(clipboard.Written.size() == 1U && clipboard.Written[0] == preview, "the clipboard holds exactly the previewed document");
		expect(copied.Frozen && copied.Round == core::cycle_round_id{ 1 } && copied.Digest == f1 && work.view().Facts.Rounds.size() == 1U, "F1 frozen by that one gesture");
		expect(copied.Deliveries.size() == 1U && copied.Deliveries[0].Observation.State == core::cycle_delivery_state::copied && copied.Deliveries[0].Observation.Channel == core::cycle_disclosure_channel::clipboard
				   && copied.Deliveries[0].Observation.Output && copied.Deliveries[0].Observation.Output->Digest == digest_of(preview) && copied.Deliveries[0].Observation.Output->Bytes == preview.size(),
			   "observed: copied, with the exact bytes' digest and size");
		expect(copied.Status.starts_with("Copied F1 to the clipboard") && copied.Status.find("read") == std::string::npos, "the words claim the copy and nothing more");

		// Save the identical round: the same identity and the same bytes.
		const auto saved = handoff / "RC-1-F1.md";
		work.Host->share(core::cycle_share_route::save_document, saved.string());
		const auto& filed = *work.view().Share;
		expect(read_file(saved) == preview && filed.Round == core::cycle_round_id{ 1 } && work.view().Facts.Rounds.size() == 1U, "saved: the same round, byte for byte");
		expect(filed.Deliveries.size() == 2U && filed.Deliveries[1].Observation.State == core::cycle_delivery_state::saved && filed.Deliveries[1].Observation.Destination == "RC-1-F1.md"
				   && filed.Deliveries[1].Observation.Output == filed.Deliveries[0].Observation.Output && filed.Deliveries[1].Observation.Digest == f1,
			   "observed: saved, under the file's name only, with the same bytes as the copy");
		work.Host->share(core::cycle_share_route::save_document, saved.string());
		work.Lanes.drain();
		expect(work.view().Share->Status.find("nothing was written") != std::string::npos && read_file(saved) == preview, "saving the same bytes again writes nothing");

		// Other bytes at the destination are never replaced unasked.
		const auto other = handoff / "notes.md";
		write_file(other, "someone else's notes\n");
		const auto observed = work.view().Share->Deliveries.size();
		work.Host->share(core::cycle_share_route::save_document, other.string());
		const auto& pending = *work.view().Share;
		expect(pending.Replace && pending.Replace->Name == "notes.md" && pending.Replace->Existing == digest_of("someone else's notes\n") && read_file(other) == "someone else's notes\n"
				   && pending.Deliveries.size() == observed,
			   "a conflict asks first, writes nothing and records nothing");
		work.Host->resolve_replace(false);
		expect(!work.view().Share->Replace && read_file(other) == "someone else's notes\n", "kept");
		work.Host->share(core::cycle_share_route::save_document, other.string());
		expect(work.view().Share->Replace.has_value(), "asked again");
		work.Host->resolve_replace(true);
		work.Lanes.drain();
		expect(read_file(other) == preview && work.view().Share->Deliveries.back().Observation.Destination == "notes.md", "replaced exactly the bytes named, on the reviewer's word");

		// The bundle carries the same round.
		const auto bundled = handoff / "RC-1-F1.bpr-review.zip";
		work.Host->share(core::cycle_share_route::save_bundle, bundled.string());
		work.Lanes.drain();
		const auto archive = read_file(bundled);
		const auto opened  = core::open_round_bundle({ reinterpret_cast<const std::uint8_t*>(archive.data()), archive.size() });
		expect(opened && opened->Round.Digest == f1 && opened->Review == preview, "the bundle holds F1 and its exact document");
		expect(work.view().Share->Deliveries.back().Observation.Output->Format == core::cycle_round_file_kind::bundle && work.view().Facts.Rounds.size() == 1U, "observed as a bundle of the same round");

		// RC56-07: nothing claims more than happened.
		for (const auto& record : work.view().Share->Deliveries) {
			const auto state = record.Observation.State;
			expect(state == core::cycle_delivery_state::copied || state == core::cycle_delivery_state::saved, "only copies and saves were observed: no retrieval, acceptance or acknowledgment");
		}
		expect(work.view().Notes.size() == 3U && std::ranges::all_of(work.view().Notes, [](const application::review_cycle_note_entry& aNote) { return !aNote.Reconciliation || aNote.Reconciliation->Status == core::cycle_item_status::open; }),
			   "and no note reads as addressed or verified because the round left");

		// An edit after sharing is an amendment: F1 stays exactly as shared.
		const auto second = work.view().Notes[1].Item.Id;
		work.Host->edit_composition(core::include_round_item{ .Item = second, .Include = false });
		const auto& amended = *work.view().Share;
		expect(!amended.Frozen && amended.Amends == core::cycle_round_id{ 1 } && amended.Round == core::cycle_round_id{ 2 } && amended.Items == 2U && amended.Digest != f1, "the edit is F2, amending F1");
		expect(amended.Document.find("Keep the parser free of I/O.") == std::string::npos && work.view().Facts.Rounds.size() == 1U, "the edit froze nothing");
		work.Host->edit_composition(core::move_round_item{ .Item = work.view().Share->Selection.Items[1].Item, .Earlier = true });
		work.Host->edit_composition(core::set_round_action{ core::cycle_round_action::plan });
		work.Host->edit_composition(core::set_round_goal{ "Make parsing total." });
		const auto& ordered = *work.view().Share;
		expect(ordered.Selection.Action == core::cycle_round_action::plan && ordered.Selection.Goal == "Make parsing total." && ordered.Document.find("Make parsing total.") != std::string::npos
				   && ordered.Document.find("Name it for what it does.") < ordered.Document.find("Why check here?"),
			   "order, action and goal are the reviewer's");
		clipboard.Answer = clipboard_double::outcome::unconfirmed;
		work.Host->share(core::cycle_share_route::copy_document);
		const auto& unconfirmed = *work.view().Share;
		expect(unconfirmed.Frozen && unconfirmed.Round == core::cycle_round_id{ 2 } && work.view().Facts.Rounds.size() == 2U, "F2 frozen");
		expect(unconfirmed.Deliveries.back().Observation.State == core::cycle_delivery_state::uncertain && unconfirmed.Status.find("did not confirm") != std::string::npos,
			   "a copy the platform did not confirm is uncertain, not copied");
		expect(unconfirmed.Deliveries.front().Observation.Digest == f1, "F1's observations still name F1");
		clipboard.Answer = clipboard_double::outcome::confirmed;

		// RC56-10: a refused channel shares nothing and is not routed around.
		auto policy                                                                                          = core::cycle_disclosure_policy{ };
		policy.Unclassified.Channels.at(static_cast<std::size_t>(core::cycle_disclosure_channel::clipboard)) = core::cycle_channel_permission::denied;
		work.Host->set_share_environment(policy, { });
		work.Host->edit_composition(core::include_round_item{ .Item = second });
		const auto& refused = *work.view().Share;
		expect(offer(refused, core::cycle_share_route::copy_document)->Decision == core::cycle_authorization::channel_denied && offer(refused, core::cycle_share_route::save_document)->offered(),
			   "Copy shown refused; Save decided on its own");
		const auto writes = clipboard.Written.size();
		const auto seen   = refused.Deliveries.size();
		work.Host->share(core::cycle_share_route::copy_document);
		const auto& denied = *work.view().Share;
		expect(clipboard.Written.size() == writes && denied.Deliveries.size() == seen && !denied.Frozen && work.view().Facts.Rounds.size() == 2U, "nothing copied, recorded or frozen");
		expect(denied.Status.find("does not allow copying") != std::string::npos && denied.Status.find("no other way was used") != std::string::npos, "said in words, with no fallback");
		expect(std::filesystem::directory_iterator{ handoff } != std::filesystem::directory_iterator{ } && !std::filesystem::exists(handoff / "RC-1-F3.md"), "and no file was written instead");

		// RC61-06: Use in external agent appears only when reachable, needs its
		// own allowance, adds nothing and starts nothing.
		work.Host->set_share_environment({ }, { .ExternalInterface = true });
		work.Host->edit_composition(core::set_round_goal{ "Make parsing total, and say why." });
		expect(offer(*work.view().Share, core::cycle_share_route::external_agent)->Decision == core::cycle_authorization::channel_unconfigured, "unknown source: not allowed until configured");
		work.Host->share(core::cycle_share_route::external_agent);
		expect(work.view().Share->Invocation.empty() && !work.view().Share->Frozen && work.view().Facts.Rounds.size() == 2U, "refused: no invocation, nothing frozen");
		auto open                                                                                                   = core::cycle_disclosure_policy{ };
		open.Unclassified.Channels.at(static_cast<std::size_t>(core::cycle_disclosure_channel::external_retrieval)) = core::cycle_channel_permission::allowed;
		work.Host->set_share_environment(open, { .ExternalInterface = true });
		const auto expected = *work.view().Share->Digest;
		const auto recorded = work.view().Share->Deliveries.size();
		work.Host->share(core::cycle_share_route::external_agent);
		const auto& external = *work.view().Share;
		expect(external.Invocation == "bpr cycle round read RC-1 --round F3" && external.Frozen && external.Digest == expected, "the exact round, as previewed, ready to be read");
		expect(external.Deliveries.size() == recorded && clipboard.Written.size() == writes, "no delivery claimed and nothing sent until something reads it");

		// Over the clipboard's limit: said with the full measure; Save keeps it all.
		work.Host->close_compose();
		expect(!work.view().Share && snapshot(project) == before, "closing Compose keeps the draft; the project was only read");
		std::cout << "NATIVE: Compose and Share through the host: copy, save, conflict, bundle, amendment, refused channel, external route\n";
	}

	/// Output over the clipboard's limit is refused with its measure and
	/// nothing cut short or frozen; Save to File stays offered.
	void refuses_a_copy_over_the_clipboard_limit(const std::filesystem::path& aScratch)
	{
		tests::sqlite_contract::scratch_root profileScratch{ "cycle-share-limit" };
		const auto&                          profileRoot = profileScratch.path();
		const auto                           project     = aScratch / "large-project";
		auto                                 large       = std::string{ };
		while (large.size() <= core::maximum_round_clipboard_bytes) {
			large += "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde\n";
		}
		write_file(project / "data.txt", large);
		auto clipboard = clipboard_double{ };
		auto subject   = core::local_cycle_subject{ };
		{
			auto first = desk{ profileRoot, 31 };
			first.Host->open_project(project.string());
			first.Lanes.drain();
			first.Host->comment({ .Primary = core::cycle_whole_target{ }, .Body = "Read all of it." });
			first.Lanes.drain();
			first.Host->compose();
			expect(first.view().Share && first.view().Share->Items == 1U, "a first draft");
			subject = first.view().Cycle->Subject;
		}
		// The whole file joins the draft as explicitly admitted large context.
		{
			application::manual_executor      lanes;
			application::review_cycle_runtime runtime{ adapters::sqlite::make_review_cycle_store(profileRoot, profile), lanes, lanes };
			runtime.select(subject);
			auto draft = std::optional<application::cycle_store_result<application::cycle_round_draft_content>>{ };
			runtime.load_round_selected({ .Cycle = subject.Cycle, .Id = core::cycle_round_draft_id{ 1 } }, [&](auto aValue) { draft = std::move(aValue); });
			lanes.drain();
			expect(draft && *draft, "the draft");
			auto selection         = (*draft)->Selection;
			selection.Context      = { { .Revision = selection.Source, .Path = "data.txt" } };
			selection.LargeContext = true;
			auto saved             = std::optional<application::cycle_store_result<application::cycle_write_receipt>>{ };
			auto operation         = std::array<std::uint8_t, 32>{ };
			operation[0]           = 0xC1;
			runtime.save_round_selected(application::cycle_operation_id{ operation }, (*draft)->Draft.Id, (*draft)->Draft.Revision, selection, [&](auto aValue) { saved = std::move(aValue); });
			lanes.drain();
			expect(saved && *saved, "context selected");
			runtime.shutdown();
			lanes.drain();
		}
		auto work = desk{ profileRoot, 32 };
		connect(work, clipboard);
		work.Host->open_project(project.string());
		work.Lanes.drain();
		work.Host->compose();
		const auto& composed = *work.view().Share;
		expect(composed.Sources == 1U && composed.DocumentBytes > core::maximum_round_clipboard_bytes && !composed.DocumentShown && composed.Document.empty(), "measured in full, not shown in part");
		work.Host->share(core::cycle_share_route::copy_document);
		const auto& refused = *work.view().Share;
		expect(clipboard.Written.empty() && refused.Deliveries.empty() && !refused.Frozen && work.view().Facts.Rounds.empty(), "nothing copied, frozen or recorded");
		expect(refused.Status.find(std::to_string(core::maximum_round_clipboard_bytes).substr(0, 2)) != std::string::npos && refused.Status.find("Nothing was cut short") != std::string::npos
				   && refused.Status.find("Save to File keeps all of it") != std::string::npos,
			   "the limit and the full measure, in words");
		expect(offer(refused, core::cycle_share_route::save_document)->offered(), "Save to File stays offered for the whole document");
		std::cout << "NATIVE: a round over the clipboard's limit refused with its measure, nothing cut short\n";
	}
}

namespace {
	/// [[BPR-1168]]. Another interface writes the cycle while this window
	/// holds it open (a forwarded terminal command runs on a session of its
	/// own beside the window): the reviewer's note is not dropped for a
	/// revision they could not see, a goal refused for a newer draft stays to
	/// save again, and the window reads the cycle again when told.
	void keeps_work_beside_another_interface(const std::filesystem::path& aScratch)
	{
		tests::sqlite_contract::scratch_root profileScratch{ "cycle-beside-profile" };
		const auto&                          profileRoot = profileScratch.path();
		const auto                           project     = aScratch / "beside-project";
		write_file(project / "README.md", "# Beside\n");

		auto window = desk{ profileRoot, 31 };
		window.Host->open_project(project.string());
		window.Lanes.drain();
		window.Host->comment({ .Primary = core::cycle_whole_target{ }, .Body = "From the window." });
		window.Host->compose();
		const auto shown = window.view().Share->Draft.Revision;
		expect(window.view().Notes.size() == 1U && window.view().Share, "the window composes over its note");

		// The other interface: a note, then the draft's goal.
		{
			auto other = desk{ profileRoot, 32 };
			other.Host->open_project(project.string());
			other.Lanes.drain();
			other.Host->comment({ .Primary = core::cycle_whole_target{ }, .Body = "From the terminal." });
			other.Host->compose();
			other.Lanes.drain();
			other.Host->edit_composition(core::set_round_goal{ .Goal = "Theirs" });
			expect(other.view().Notes.size() == 2U && other.view().Share->Selection.Goal == "Theirs" && other.view().Message.empty(), "the other interface wrote the cycle and the draft");
		}

		// The window still holds the revisions it read. Its note is added to the
		// cycle as it is now, and both notes stand.
		window.Host->comment({ .Primary = core::cycle_whole_target{ }, .Body = "Written while the terminal wrote." });
		const auto& noted = window.view();
		expect(noted.Message.empty() && noted.Notes.size() == 3U && std::ranges::any_of(noted.Notes, [](const auto& aNote) { return aNote.Body == "Written while the terminal wrote."; }),
			   "the window's note is kept, beside the terminal's");

		// A goal typed against the draft the window showed is refused, not
		// written over the newer draft, and kept for the reviewer.
		window.Host->edit_composition(core::set_round_goal{ .Goal = "Mine" });
		expect(window.Host->saving_draft(), "the edit is waiting to be saved");
		const auto& refused = window.view();
		expect(!window.Host->saving_draft() && refused.Share && refused.Share->UnsavedGoal == std::optional<std::string>{ "Mine" } && refused.Share->Selection.Goal == "Theirs"
				   && refused.Share->Draft.Revision > shown && refused.Share->Status.find("not saved") != std::string::npos,
			   "the terminal's goal stands, the window's is kept unsaved, and the draft is shown as it is now");
		window.Host->edit_composition(core::set_round_goal{ .Goal = "Mine" });
		const auto& saved = window.view();
		expect(saved.Share->Selection.Goal == "Mine" && !saved.Share->UnsavedGoal, "saved again by the reviewer, over the revision they now see");

		// Told that another interface wrote the cycle, the window reads it again.
		{
			auto other = desk{ profileRoot, 33 };
			other.Host->open_project(project.string());
			other.Lanes.drain();
			other.Host->comment({ .Primary = core::cycle_whole_target{ }, .Body = "Later, from the terminal." });
			other.Lanes.drain();
		}
		expect(window.view().Notes.size() == 3U, "nothing told the window yet");
		window.Host->observe_external_change(core::review_cycle_id{ 2 });
		expect(window.view().Notes.size() == 3U, "another cycle's change is not this one's");
		window.Host->observe_external_change(saved.Cycle->Subject.Cycle);
		expect(window.view().Notes.size() == 4U && window.view().Share && window.view().Share->Selection.Goal == "Mine", "read again, Compose feedback with it");
		std::cout << "NATIVE: a window keeps its note and its goal beside another interface's writes\n";
	}
}

auto main(const int aArgc, char** aArgv) -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-workbench" };
	opens_a_folder_to_a_first_note(scratch.path());
	iterates_manually(scratch.path(), tests::workspace_contract::git_from(aArgc, aArgv));
	composes_and_shares_one_round(scratch.path());
	refuses_a_copy_over_the_clipboard_limit(scratch.path());
	keeps_work_beside_another_interface(scratch.path());
	std::cout << "review-cycle-workbench-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle workbench failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
