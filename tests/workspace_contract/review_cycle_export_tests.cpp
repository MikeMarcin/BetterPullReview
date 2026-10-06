#include "../support/test_expectation.hpp"
#include "bpr/adapters/workspace/local_export_destination.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_export.hpp"
#include "bpr/application/review_cycle_share.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"
#include "bpr/core/sha256.hpp"
#include "cycle_course.hpp"
#include "cycle_session.hpp"
#include "workspace_fixture.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>

	#include <chrono>
	#include <thread>
#endif

/// \file
/// \brief Copy and Save of a frozen round (RFC-BPR-0056, BPR-1159) through
/// the production runtime, store and local file destination: the clipboard
/// text and the saved file are the same complete document, read back from
/// the store after a restart; a save never replaces other bytes without
/// confirmation of exactly those bytes, and leaves nothing else behind.

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using application::cycle_export_failure;
	using core::cycle_round_save_action;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::session;
	using tests::workspace_contract::write_file;

	constexpr std::string_view body = "#include \"cache.hpp\"\r\n\r\nvoid Cache::insert(int key) {\r\n\tentries.push_back(key);\r\n}\r\n";

	auto bytes_of(const std::string& aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	auto digest_of(const std::string& aText) -> core::cycle_material_digest
	{
		return core::cycle_material_digest{ core::sha256(bytes_of(aText)) };
	}

	auto names_in(const std::filesystem::path& aFolder) -> std::set<std::string>
	{
		auto names = std::set<std::string>{ };
		for (const auto& entry : std::filesystem::directory_iterator{ aFolder }) {
			names.insert(entry.path().filename().string());
		}
		return names;
	}

	auto save(const std::filesystem::path& aFile, const std::string& aDocument, const std::optional<core::cycle_material_digest>& aConfirmed = std::nullopt)
		-> std::expected<application::cycle_round_save, cycle_export_failure>
	{
		const auto destination = adapters::workspace::make_local_export_destination(aFile);
		return application::save_round_document(*destination, bytes_of(aDocument), aConfirmed, { });
	}

	/// [[BPR-1167]]. `bpr cycle export` through the runtime: a named frozen
	/// round leaves exactly as it was frozen, even after an amendment, by the
	/// channel policy allows; a save records only the save.
	void exports_a_frozen_round_as_frozen(session& aNotes, const std::filesystem::path& aExports, const std::string& aFirst, const std::string& aSecond)
	{
		const auto request = [](const std::uint64_t aRound, const core::cycle_disclosure_channel aChannel, core::cycle_disclosure_policy aPolicy = { }) {
			return application::cycle_round_export_request{ .Round = core::cycle_round_id{ aRound }, .Channel = aChannel, .Policy = aPolicy };
		};
		const auto printed = aNotes.run<application::cycle_share_output>([&](auto aDone) { return aNotes.Runtime.export_round_selected(request(1, core::cycle_disclosure_channel::terminal_output), std::move(aDone)); });
		expect(printed && printed->Decision == core::cycle_authorization::allowed && printed->Round && printed->Round->Round.Round.Id.Id == core::cycle_round_id{ 1 } && printed->Bytes == aFirst,
			   "F1 prints as it was frozen after F2 amended it");
		const auto bundle = aNotes.run<application::cycle_share_output>([&](auto aDone) {
			auto asked   = request(2, core::cycle_disclosure_channel::local_file);
			asked.Format = core::cycle_round_file_kind::bundle;
			return aNotes.Runtime.export_round_selected(asked, std::move(aDone));
		});
		expect(bundle && bundle->Format == core::cycle_round_file_kind::bundle && !bundle->Bytes.empty() && bundle->Bytes != aSecond, "a bundle of F2");

		// A channel configured off renders nothing and stands nothing else in.
		auto denied                                                                                                = core::cycle_disclosure_policy{ };
		denied.Unclassified.Channels.at(static_cast<std::size_t>(core::cycle_disclosure_channel::terminal_output)) = core::cycle_channel_permission::denied;
		const auto refused                                                                                         = aNotes.run<application::cycle_share_output>([&](auto aDone) { return aNotes.Runtime.export_round_selected(request(1, core::cycle_disclosure_channel::terminal_output, denied), std::move(aDone)); });
		expect(refused && refused->Decision == core::cycle_authorization::channel_denied && refused->Bytes.empty() && !refused->Round, "denied terminal output renders nothing");
		const auto unknown = aNotes.run<application::cycle_share_output>([&](auto aDone) { return aNotes.Runtime.export_round_selected(request(9, core::cycle_disclosure_channel::terminal_output), std::move(aDone)); });
		expect(!unknown && unknown.error().Failure == application::cycle_store_failure::not_found, "no F9 to export");

		const auto deliveries = [&] {
			const auto records = aNotes.run<std::vector<core::cycle_delivery_record>>([&](auto aDone) { return aNotes.Runtime.deliveries_selected(std::move(aDone)); });
			expect(records.has_value(), "deliveries read");
			return *records;
		};
		const auto before = deliveries().size();
		const auto file   = aExports / "printed.md";
		write_file(file, "kept\n");
		const auto to_file = [&](const std::optional<core::cycle_material_digest>& aConfirmed) {
			auto destination = std::shared_ptr<application::cycle_export_destination>{ adapters::workspace::make_local_export_destination(file) };
			return aNotes.run<application::cycle_file_share>([&](auto aDone) {
				return aNotes.Runtime.export_round_to_file_selected(request(1, core::cycle_disclosure_channel::local_file), destination, "printed.md", aConfirmed,
																	tests::workspace_contract::session_operation(aNotes.NextOperation++), std::move(aDone));
			});
		};
		const auto kept = to_file(std::nullopt);
		expect(kept && kept->Save && kept->Save->Action == cycle_round_save_action::conflict && !kept->Recorded && read_file(file) == "kept\n" && deliveries().size() == before,
			   "other bytes are kept and nothing is recorded");
		const auto saved = to_file(digest_of("kept\n"));
		expect(saved && saved->Recorded && read_file(file) == aFirst, "F1 replaces exactly the confirmed bytes");
		const auto recorded = deliveries();
		expect(recorded.size() == before + 1U && recorded.back().Observation.State == core::cycle_delivery_state::saved && recorded.back().Observation.Destination == "printed.md"
				   && recorded.back().Observation.Round.Id == core::cycle_round_id{ 1 } && recorded.back().Observation.Output && recorded.back().Observation.Output->Digest == digest_of(aFirst),
			   "the save of F1 is recorded under the file's name");
		std::cout << "NATIVE: a named frozen round exported as frozen to the terminal and a file; a denied channel and an unknown round export nothing\n";
	}

	void copies_and_saves_one_document(const std::filesystem::path& aScratch)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "project";
		write_file(root / "src" / "cache.cpp", body);
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");

		auto       notes = session{ cycle.StoreRoot.path(), cycle.Subject };
		const auto first = notes.begin({ .Primary = notes.anchor(1, "src/cache.cpp", "\tentries.push_back(key);\r\n"), .Body = "issue: Duplicates.\n\n```cpp\nif (contains(key)) return;\n```" });
		const auto item  = core::cycle_feedback_item_ref{ .Cycle = cycle.Subject.Cycle, .Id = core::cycle_feedback_item_id{ first->Allocated } };
		const auto added = notes.run<application::cycle_write_receipt>([&](auto aDone) {
			return notes.Runtime.add_suggestion_selected(tests::workspace_contract::session_operation(notes.NextOperation++), item, 1,
														 { .Selection = notes.anchor(1, "src/cache.cpp", "\tentries.push_back(key);\r\n"), .Replacement = { } }, std::move(aDone));
		});
		expect(added.has_value(), "deletion suggestion");
		auto draft     = tests::workspace_contract::begin_round(notes);
		auto selection = core::cycle_round_selection{
			.Source  = core::cycle_source_revision_id{ 1 },
			.Goal    = "Remove duplicate insertion.",
			.Items   = { { .Item = item.Id, .Wording = core::private_review_turn_revision_id{ 1 }, .Suggestions = { { .Id = core::cycle_suggestion_id{ 1 }, .Revision = 1 } } } },
			.Context = { { .Revision = core::cycle_source_revision_id{ 1 }, .Path = "src/cache.cpp" } },
		};
		draft.save(selection);
		const auto frozen = draft.freeze(draft.flushed());
		expect(frozen.Frozen.has_value(), "F1 frozen");

		// Copy: the whole document within the clipboard budget.
		const auto clipboard = core::render_round_markdown(frozen.Frozen->Content, core::maximum_round_clipboard_bytes);
		expect(clipboard.has_value(), "clipboard text");
		expect(clipboard->find("## I1 \xE2\x80\x94 item revision 1") != std::string::npos && clipboard->find("(No bytes: the edit deletes the original.)") != std::string::npos,
			   "the item and its deletion");
		expect(clipboard->find(root.string()) == std::string::npos && clipboard->find(root.generic_string()) == std::string::npos && clipboard->find(cycle.StoreRoot.path().string()) == std::string::npos,
			   "no absolute local path");
		// A round too large for its channel is refused with its size, never cut.
		const auto tight = core::render_round_markdown(frozen.Frozen->Content, 64U);
		expect(!tight && tight.error().RequiredBytes == clipboard->size() && tight.error().Items == 1U, "over the channel budget, refused whole");

		// Save: the same bytes, read back from the store after a restart.
		const auto exports = aScratch / "exports";
		std::filesystem::create_directories(exports);
		const auto file = exports / core::round_file_name(frozen.Frozen->Content.Payload, core::cycle_round_file_kind::markdown);
		expect(file.filename() == "RC-1-F1.md", "default name from wire identities");
		auto reread = std::string{ };
		{
			auto reopened = session{ cycle.StoreRoot.path(), cycle.Subject };
			auto again    = tests::workspace_contract::round_drafts{ .Notes = reopened, .Draft = draft.Draft, .Revision = draft.Revision };
			reread        = core::render_round_markdown(again.read(1).Content);
		}
		expect(reread == *clipboard, "copy and save are the same document");
		const auto created = save(file, reread);
		expect(created && created->Action == cycle_round_save_action::create && !created->Existing && read_file(file) == *clipboard, "saved exactly");
		const auto before = std::filesystem::last_write_time(file);
		const auto repeat = save(file, reread);
		expect(repeat && repeat->Action == cycle_round_save_action::already_saved && std::filesystem::last_write_time(file) == before, "the same round again writes nothing");

		// An amendment to the same file: a conflict until exactly F1's bytes are confirmed.
		const auto edited = notes.change(item, 2, core::edit_feedback_turn{ .Latest = core::private_review_turn_revision_id{ 1 }, .Body = "issue: Duplicates; keep CRLF." });
		expect(edited.has_value(), "note edited");
		selection.Items[0].Wording = core::private_review_turn_revision_id{ edited->Allocated };
		draft.save(selection);
		const auto amended = draft.freeze(draft.flushed());
		expect(amended.Frozen && amended.Frozen->Round.Round.Id.Id == core::cycle_round_id{ 2 }, "F2 frozen");
		const auto second   = core::render_round_markdown(amended.Frozen->Content);
		const auto conflict = save(file, second);
		expect(conflict && conflict->Action == cycle_round_save_action::conflict && conflict->Existing && conflict->Existing->Digest == digest_of(*clipboard) && read_file(file) == *clipboard,
			   "conflict names the bytes there and writes nothing");
		write_file(file, "someone else's edit\n");
		const auto stale = save(file, second, digest_of(*clipboard));
		expect(stale && stale->Action == cycle_round_save_action::conflict && read_file(file) == "someone else's edit\n", "a file changed since confirmation is not replaced");
		const auto replaced = save(file, second, digest_of("someone else's edit\n"));
		expect(replaced && replaced->Action == cycle_round_save_action::replace && read_file(file) == second, "confirmed replacement");
		const auto destination = adapters::workspace::make_local_export_destination(file);
		const auto raced       = destination->replace(bytes_of(*clipboard), { .Digest = digest_of(*clipboard), .Size = clipboard->size() }, { });
		expect(!raced && raced.error() == cycle_export_failure::changed && read_file(file) == second, "replace re-checks the bytes there");

		// Special destinations are refused; a missing folder is unavailable.
		std::filesystem::create_directories(exports / "folder.md");
		expect(save(exports / "folder.md", second).error() == cycle_export_failure::not_regular, "a directory is not a file");
		if (tests::workspace_contract::try_symlink(file, exports / "link.md", false)) {
			expect(save(exports / "link.md", second).error() == cycle_export_failure::not_regular && read_file(file) == second, "a link is never followed");
		}
		expect(save(exports / "missing" / "RC-1-F1.md", second).error() == cycle_export_failure::unavailable, "missing folder");
		expect(save(std::filesystem::path{ "relative.md" }, second).error() == cycle_export_failure::unavailable, "a relative destination is refused");

		// Nothing else was written beside the chosen file, and the workspace is untouched.
		auto expected = std::set<std::string>{ "RC-1-F1.md", "folder.md" };
		if (std::filesystem::is_symlink(exports / "link.md")) {
			expected.insert("link.md");
		}
		expect(names_in(exports) == expected, "no temporary file left behind");
		expect(read_file(root / "src" / "cache.cpp") == body, "workspace bytes unchanged");
		std::cout << "NATIVE: one complete round document copied and saved; conflicts, links and folders refused; nothing else written\n";

		exports_a_frozen_round_as_frozen(notes, exports, *clipboard, second);
	}
#if defined(_WIN32)
	/// [[BPR-1239]]. A confirmed Replace that Windows refuses says why. A
	/// read-only destination is refused as read-only, its bytes and attribute
	/// unchanged. A destination another handle holds without FILE_SHARE_DELETE
	/// is waited out for a bounded moment: released within it, it is replaced;
	/// held past it, it is refused as in use and left as it was. No temporary
	/// file remains either way.
	void windows_refusals_name_their_reason(const std::filesystem::path& aFolder)
	{
		std::filesystem::create_directories(aFolder);
		const auto readOnly = aFolder / "read-only.md";
		write_file(readOnly, "locked\n");
		expect(::SetFileAttributesW(readOnly.c_str(), FILE_ATTRIBUTE_READONLY) != 0, "the test marks its file read-only");
		const auto refused    = save(readOnly, "replacement\n", digest_of("locked\n"));
		const auto attributes = ::GetFileAttributesW(readOnly.c_str());
		expect(!refused && refused.error() == cycle_export_failure::read_only && read_file(readOnly) == "locked\n" && attributes != INVALID_FILE_ATTRIBUTES
				   && (attributes & FILE_ATTRIBUTE_READONLY) != 0U,
			   "a read-only destination is refused as read-only and left as it was");
		expect(::SetFileAttributesW(readOnly.c_str(), FILE_ATTRIBUTE_NORMAL) != 0, "the test clears its own attribute");

		const auto hold = [](const std::filesystem::path& aFile) {
			return ::CreateFileW(aFile.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		};
		const auto briefly = aFolder / "held-briefly.md";
		write_file(briefly, "before\n");
		{
			const auto held = hold(briefly);
			expect(held != INVALID_HANDLE_VALUE, "the test holds the destination");
			auto release = std::jthread{ [held] {
				std::this_thread::sleep_for(std::chrono::milliseconds{ 150 });
				::CloseHandle(held);
			} };
			const auto replaced = save(briefly, "after\n", digest_of("before\n"));
			release.join();
			expect(replaced && replaced->Action == cycle_round_save_action::replace && read_file(briefly) == "after\n", "a destination released within the wait is replaced");
		}
		const auto stuck = aFolder / "held.md";
		write_file(stuck, "kept\n");
		const auto held = hold(stuck);
		expect(held != INVALID_HANDLE_VALUE, "the test holds the destination past the wait");
		const auto waited = save(stuck, "after\n", digest_of("kept\n"));
		::CloseHandle(held);
		expect(!waited && waited.error() == cycle_export_failure::in_use && read_file(stuck) == "kept\n", "a destination held past the wait is refused as in use and left as it was");
		expect(names_in(aFolder) == std::set<std::string>{ "held-briefly.md", "held.md", "read-only.md" }, "no temporary file left behind");
		std::cout << "NATIVE: a read-only destination and one held past the wait name their reason; a brief hold is waited out\n";
	}
#endif
}

auto main() -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-export" };
	copies_and_saves_one_document(scratch.path());
#if defined(_WIN32)
	windows_refusals_name_their_reason(scratch.path() / "windows-refusals");
#endif
	std::cout << "review-cycle-export-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle export failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
