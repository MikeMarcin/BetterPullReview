#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_export_destination.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_export.hpp"
#include "bpr/application/review_cycle_import.hpp"
#include "bpr/core/review_cycle_bundle.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"
#include "cycle_course.hpp"
#include "cycle_session.hpp"
#include "workspace_fixture.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Portable bundles through the production path (RFC-BPR-0056,
/// BPR-1160): a frozen round saved as `.bpr-review.zip`, read back from the
/// chosen file and opened into another profile's quarantine with no account
/// or agent; a repeated import finds the same record, a damaged bundle admits
/// nothing, an interrupted import recovers once, and nothing is extracted to
/// any folder.

namespace {
	using namespace bpr;
	using application::cycle_capture_status;
	using tests::expect;
	using tests::workspace_contract::course;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::session;
	using tests::workspace_contract::write_file;

	constexpr std::string_view body = "#include \"cache.hpp\"\r\n\r\nvoid Cache::insert(int key) {\r\n\tentries.push_back(key);\r\n}\r\n";

	auto snapshot(const std::filesystem::path& aRoot) -> std::map<std::string, std::string>
	{
		auto files = std::map<std::string, std::string>{ };
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ aRoot }) {
			files.emplace(entry.path().lexically_relative(aRoot).generic_string(), entry.is_regular_file() ? read_file(entry.path()) : std::string{ "<dir>" });
		}
		return files;
	}

	auto span_of(const std::string& aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	auto import_into(application::review_cycle_store& aStore, const std::uint8_t aSeed, const std::vector<std::uint8_t>& aBundle) -> application::cycle_import_result
	{
		const auto imported = application::import_round_bundle(aStore, tests::workspace_contract::operation(aSeed), aBundle, { });
		expect(imported.has_value(), "import decided");
		return *imported;
	}

	void saves_and_opens_a_bundle(const std::filesystem::path& aScratch, const std::filesystem::path& aExamples)
	{
		auto       source = adapters::workspace::make_local_workspace_source();
		const auto root   = aScratch / "project";
		write_file(root / "src" / "cache.cpp", body);
		auto cycle = course{ *source, root };
		expect(cycle.capture(*source).Status == cycle_capture_status::captured, "R1");

		auto       notes = session{ cycle.StoreRoot.path(), cycle.Subject };
		const auto first = notes.begin({ .Primary = notes.anchor(1, "src/cache.cpp", "\tentries.push_back(key);\r\n"), .Body = "issue: Duplicates." });
		expect(first.has_value(), "note");
		auto draft = tests::workspace_contract::begin_round(notes);
		draft.save({ .Source = core::cycle_source_revision_id{ 1 }, .Goal = "Remove duplicates.", .Items = { { .Item = core::cycle_feedback_item_id{ first->Allocated }, .Wording = core::private_review_turn_revision_id{ 1 } } }, .Context = { { .Revision = core::cycle_source_revision_id{ 1 }, .Path = "src/cache.cpp" } } });
		const auto frozen = draft.freeze(draft.flushed());
		expect(frozen.Frozen.has_value(), "F1");

		// Save the bundle to a chosen file, then read that file back as a recipient would.
		const auto shared = aScratch / "shared";
		std::filesystem::create_directories(shared);
		const auto bundle = core::build_round_bundle(frozen.Frozen->Content);
		const auto file   = shared / core::round_file_name(frozen.Frozen->Content.Payload, core::cycle_round_file_kind::bundle);
		const auto saved  = application::save_round_document(*adapters::workspace::make_local_export_destination(file), span_of(bundle), std::nullopt, { });
		expect(saved && saved->Action == core::cycle_round_save_action::create && file.filename() == "RC-1-F1.bpr-review.zip", "bundle saved");
		const auto read = adapters::workspace::read_chosen_file(file, core::maximum_bundle_archive_bytes, { });
		expect(read && std::string(read->begin(), read->end()) == bundle, "bundle read back from the chosen file");

		// A second profile opens it into quarantine: no account, provider or agent.
		auto       recipient = tests::sqlite_contract::scratch_root{ "cycle-bundle-recipient" };
		auto       store     = adapters::sqlite::make_review_cycle_store(recipient.path(), core::bpr_profile_id{ 9 });
		const auto folders   = snapshot(aScratch);
		const auto opened    = import_into(*store, 1, *read);
		expect(opened.Import && !opened.Existing && !opened.Refusal, "admitted to quarantine");
		expect(opened.Import->Summary.ForeignCycle == "RC-1" && opened.Import->Summary.ForeignRound == "F1" && opened.Import->Summary.Digest == frozen.Frozen->Round.Round.Digest,
			   "the sender's identities and digest, unchanged");
		expect(opened.Import->Objects.size() == 1U, "the attachment pinned by digest");
		const auto content = application::read_imported_round(*store, opened.Import->Summary.Id, { });
		expect(content && content->Content.Canonical == frozen.Frozen->Content.Canonical && core::render_round_markdown(content->Content) == core::render_round_markdown(frozen.Frozen->Content),
			   "the same canonical round and document");
		expect(snapshot(aScratch) == folders, "nothing extracted beside the bundle or into the project");

		// The same bundle again, and after a restart of the recipient's store.
		expect(import_into(*store, 2, *read).Existing, "a repeated import finds the same record");
		store            = adapters::sqlite::make_review_cycle_store(recipient.path(), core::bpr_profile_id{ 9 });
		const auto again = import_into(*store, 3, *read);
		expect(again.Existing && again.Import->Summary.Id == opened.Import->Summary.Id, "and after a restart");
		const auto replay = application::import_round_bundle(*store, tests::workspace_contract::operation(3), *read, { });
		expect(replay && replay->Replayed, "a retried operation replays");

		// A damaged bundle admits nothing at all.
		const auto held    = store->inventory({ });
		auto       damaged = *read;
		damaged[damaged.size() / 2U] ^= 0x5AU;
		const auto refused = import_into(*store, 4, damaged);
		expect(refused.Refusal && !refused.Import, "damaged bundle refused");
		const auto after = store->inventory({ });
		expect(held && after && *after == *held, "no object installed for a refused bundle");

		// The pack's deflated example opens beside it.
		const auto pack = adapters::workspace::read_chosen_file(aExamples / "example.bpr-review.zip", core::maximum_bundle_archive_bytes, { });
		expect(pack.has_value(), "pack bundle read");
		const auto example = import_into(*store, 5, *pack);
		expect(example.Import && example.Import->Summary.ForeignCycle == "RC-17" && example.Import->Summary.ForeignProject == "example-cache", "pack bundle quarantined with its own identities");
		const auto listed = store->imports(std::nullopt, { });
		expect(listed && listed->Imports.size() == 2U, "two quarantined rounds");

		// A link where a bundle should be is not followed.
		if (tests::workspace_contract::try_symlink(file, shared / "link.bpr-review.zip", false)) {
			const auto linked = adapters::workspace::read_chosen_file(shared / "link.bpr-review.zip", core::maximum_bundle_archive_bytes, { });
			expect(!linked && linked.error() == application::cycle_export_failure::not_regular, "a link is never followed");
		}
		const auto large = adapters::workspace::read_chosen_file(file, 16U, { });
		expect(!large && large.error() == application::cycle_export_failure::too_large, "a file over the bound is not read");
		std::cout << "NATIVE: a frozen round saved as a bundle opens into another profile's quarantine; repeats reuse, damage admits nothing, nothing is extracted\n";
	}

	void recovers_an_interrupted_import(const std::filesystem::path& aExamples)
	{
		auto       recipient = tests::sqlite_contract::scratch_root{ "cycle-bundle-interrupted" };
		auto       store     = adapters::sqlite::make_review_cycle_store(recipient.path(), core::bpr_profile_id{ 9 });
		const auto pack      = adapters::workspace::read_chosen_file(aExamples / "example.bpr-review.zip", core::maximum_bundle_archive_bytes, { });
		expect(pack.has_value(), "pack bundle read");
		// A cancelled import commits nothing.
		auto stop = std::stop_source{ };
		stop.request_stop();
		const auto cancelled = application::import_round_bundle(*store, tests::workspace_contract::operation(10), *pack, stop.get_token());
		expect(!cancelled && cancelled.error().Failure == application::cycle_store_failure::cancelled, "cancelled import");
		// A process that installed the bytes and died before admitting them.
		const auto opened = core::open_round_bundle(*pack);
		expect(opened.has_value(), "bundle valid");
		expect(store->install(span_of(opened->Round.Canonical), { }).has_value(), "payload installed before the crash");
		const auto orphan = store->inventory({ });
		expect(orphan && orphan->UnreferencedObjects == 1U && store->imports(std::nullopt, { })->Imports.empty(), "only an unreferenced object, no import");
		const auto recovered = import_into(*store, 11, *pack);
		expect(recovered.Import && !recovered.Existing, "the retried import admits once");
		const auto settled = store->inventory({ });
		expect(settled && settled->UnreferencedObjects == 0U && store->imports(std::nullopt, { })->Imports.size() == 1U, "nothing unreferenced; one record");
	}
}

auto main(const int aCount, char** aArguments) -> int
try {
	expect(aCount == 2, "usage: test <examples directory>");
	tests::sqlite_contract::scratch_root scratch{ "cycle-bundle" };
	saves_and_opens_a_bundle(scratch.path(), aArguments[1]);
	recovers_an_interrupted_import(aArguments[1]);
	std::cout << "review-cycle-bundle-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle bundle failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
