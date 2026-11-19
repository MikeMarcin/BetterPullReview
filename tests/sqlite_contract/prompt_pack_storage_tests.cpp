// RFC-BPR-0125 AC24 and AC26 ([[BPR-2149]]). A prompt pack through the real
// stores and the exchange host: an export records the pack on the exported
// prompts (a prompt made before UUIDs gets one), the file imports into a clean
// profile, a later version updates the imported copies in place and leaves the
// recipient's own prompt, On and placements as they were, an edit made after
// the preview refuses the import, and a store made before the pack table
// gains it and keeps its prompts.
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/application/agent_configuration_exchange.hpp"
#include "bpr/core/prompt_pack.hpp"
#include "scratch_root.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using tests::expect;

	class queue final : public application::effect_executor, public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		void drain()
		{
			while (!Tasks.empty()) {
				auto task = std::move(Tasks.front());
				Tasks.pop_front();
				task();
			}
		}

	private:
		std::deque<std::function<void()>> Tasks;
	};

	[[nodiscard]] auto read_file(const std::string& aPath, std::size_t aLimit) -> std::optional<std::string>
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		if (!stream) {
			return std::nullopt;
		}
		auto bytes = std::string{ std::istreambuf_iterator<char>{ stream }, { } };
		return bytes.size() > aLimit + 1U ? std::nullopt : std::optional{ bytes };
	}

	[[nodiscard]] auto write_file(const std::string& aPath, const std::string& aBytes) -> bool
	{
		auto stream = std::ofstream{ aPath, std::ios::binary | std::ios::trunc };
		stream << aBytes;
		return static_cast<bool>(stream);
	}

	[[nodiscard]] auto prompt(std::string aId, std::string aName, std::string aTemplate) -> core::prompt_macro
	{
		auto macro          = core::prompt_macro{ .Id = std::move(aId), .Place = core::prompt_place::agent_tab };
		macro.Body.Name     = std::move(aName);
		macro.Body.Template = std::move(aTemplate);
		return macro;
	}

	[[nodiscard]] auto own(const core::prompt_macro_differences& aStored, const std::string_view aId) -> std::optional<core::prompt_macro>
	{
		const auto found = std::ranges::find(aStored.Own, aId, &core::prompt_macro::Id);
		return found == aStored.Own.end() ? std::nullopt : std::optional{ *found };
	}

	/// One profile's exchange host over its real store.
	struct profile
	{
		explicit profile(const std::filesystem::path& aRoot)
			: Root{ aRoot }
			, Store{ adapters::sqlite::make_agent_configuration_store(aRoot) }
			, Host{ application::agent_configuration_dependencies{ .Store = Store.get(), .ReadFile = read_file, .WriteFile = write_file, .MintIdentity = { }, .Effects = &Effects, .Decision = &Decision } }
		{ }

		void settle()
		{
			Effects.drain();
			Decision.drain();
		}

		[[nodiscard]] auto snapshot() -> application::agent_configuration_snapshot
		{
			auto loaded = Store->load();
			expect(loaded.has_value(), "the snapshot reads");
			return *loaded;
		}

		[[nodiscard]] auto read(const std::string& aPath) -> application::agent_configuration_preview
		{
			auto preview = std::optional<application::agent_configuration_preview>{ };
			Host.read(aPath, [&preview](std::expected<application::agent_configuration_preview, application::agent_configuration_read_error> aPreview) {
				expect(aPreview.has_value(), "the file is offered for preview");
				if (aPreview) {
					preview = std::move(*aPreview);
				}
			});
			settle();
			return preview.value_or(application::agent_configuration_preview{ });
		}

		[[nodiscard]] auto apply(const application::agent_configuration_preview& aPreview, core::prompt_pack_choices aChoices) -> std::optional<application::agent_configuration_host::applied>
		{
			auto outcome = std::optional<application::agent_configuration_host::applied>{ };
			Host.apply(aPreview, { }, { }, [&outcome](application::agent_configuration_host::applied aApplied) { outcome = std::move(aApplied); }, { }, std::move(aChoices));
			settle();
			return outcome;
		}

		std::filesystem::path                                        Root;
		queue                                                        Effects;
		queue                                                        Decision;
		std::unique_ptr<application::agent_configuration_store_port> Store;
		application::agent_configuration_host                        Host;
	};

	/// Exports `aChosen` under `aPack` the way the composition does: the
	/// pack recorded first, then the file written.
	[[nodiscard]] auto export_pack(profile& aProfile, const core::prompt_pack_identity& aPack, const std::vector<std::string>& aChosen, const std::string& aPath) -> bool
	{
		const auto before = aProfile.snapshot();
		auto       fresh  = std::vector<std::string>{ };
		for (std::size_t index = 0U; index < aChosen.size(); ++index) {
			fresh.push_back(application::mint_prompt_uuid());
		}
		const auto exported = core::export_prompt_pack(before.PromptMacros.Differences, aPack, aChosen, fresh);
		if (!exported) {
			return false;
		}
		const auto bytes   = application::export_agent_configuration(before, { }, { }, "Team", "BetterPullReview test", { }, nullptr, { }, &*exported);
		auto       outcome = std::optional<application::agent_configuration_outcome>{ };
		aProfile.Host.record_export(before, application::snapshot_after_export(before, *exported), [&outcome](const application::agent_configuration_outcome aOutcome) { outcome = aOutcome; });
		aProfile.settle();
		auto written = false;
		aProfile.Host.write(aPath, bytes.value_or(std::string{ }), [&written](const bool aWritten) { written = aWritten; });
		aProfile.settle();
		return bytes && outcome == application::agent_configuration_outcome::applied && written;
	}
} // namespace

int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "prompt-pack-sender" };
	tests::sqlite_contract::scratch_root second{ "prompt-pack-recipient" };
	auto                                 sender    = profile{ scratch.path() };
	auto                                 recipient = profile{ second.path() };
	const auto                           file      = (scratch.path() / "team.json").string();

	// The sender: one prompt made before UUIDs, one with a UUID, both on a menu.
	const auto oldId    = std::string{ "own.0123456789abcdef0123456789abcdef" };
	const auto uuidId   = application::mint_prompt_uuid();
	auto       authored = core::prompt_macro_differences{ };
	authored.Own        = { prompt(oldId, "Look again", "Look again at {pr.title}."), prompt(uuidId, "Risks", "What could this break?") };
	authored.Surfaces   = { core::prompt_surface_change{ .Surface = core::prompt_surface::pull_request_menu, .Ids = { oldId, uuidId }, .Removed = { } } };
	const auto macros   = adapters::sqlite::make_prompt_macro_store(scratch.path());
	expect(macros->store(authored, 0U) == application::prompt_macro_write::stored, "the sender's prompts are stored");

	// Export version 1 of a new pack.
	const auto packId = application::mint_prompt_uuid();
	expect(export_pack(sender, { .Id = packId, .Label = "Team prompts", .Version = 1U }, { oldId, uuidId }, file), "version 1 is recorded and written");
	const auto sent = sender.snapshot().PromptMacros;
	expect(sent.Revision == 2U && sent.Differences.Own.size() == 2U, "the export moved the prompts on one revision");
	const auto renamed = sent.Differences.Own[0].Id;
	expect(core::valid_prompt_uuid(renamed) && renamed != oldId && sent.Differences.Surfaces[0].Ids[0] == renamed, "the prompt made before UUIDs has one now, its placement too");
	expect(std::ranges::all_of(sent.Differences.Own, [&packId](const core::prompt_macro& aMacro) { return aMacro.Pack && aMacro.Pack->Pack == packId && aMacro.Pack->Version == 1U; }),
		   "a restart reads both linked to the pack at version 1");
	const auto bytes = read_file(file, core::maximum_agent_configuration_bytes);
	expect(bytes && bytes->find("\"version\": 3") != std::string::npos && bytes->find(oldId) == std::string::npos, "the file is version 3 and carries no old identity");

	// The recipient has a prompt of their own and nothing from the pack.
	auto mine = core::prompt_macro_differences{ };
	mine.Own  = { prompt(application::mint_prompt_uuid(), "Mine", "My own prompt.") };
	expect(adapters::sqlite::make_prompt_macro_store(second.path())->store(mine, 0U) == application::prompt_macro_write::stored, "the recipient's prompt is stored");

	auto       preview = recipient.read(file);
	const auto plan    = core::plan_prompt_pack_import(*preview.File.PromptPack, preview.Snapshot.PromptMacros.Differences, core::shipped_prompt_macros());
	expect(plan.Items.size() == 2U && std::ranges::all_of(plan.Items, [](const core::prompt_pack_item_plan& aItem) { return aItem.Standing == core::prompt_pack_standing::added; }),
		   "into a clean profile every prompt is added");
	auto applied = recipient.apply(preview, core::default_prompt_pack_choices(plan));
	expect(applied && applied->has_value() && (*applied)->first == application::agent_configuration_outcome::applied, "the import applies");
	auto stored = recipient.snapshot().PromptMacros;
	expect(stored.Differences.Own.size() == 3U && stored.Differences.Own[0] == mine.Own[0], "both added after the recipient's own, which is unchanged");
	expect(own(stored.Differences, uuidId) && own(stored.Differences, uuidId)->Pack && own(stored.Differences, uuidId)->Pack->Version == 1U, "an added prompt is linked, read back from the store");

	// The recipient turns one off; the sender improves the other and exports
	// version 2.
	{
		auto off                                                        = stored.Differences;
		std::ranges::find(off.Own, uuidId, &core::prompt_macro::Id)->On = false;
		expect(adapters::sqlite::make_prompt_macro_store(second.path())->store(off, stored.Revision) == application::prompt_macro_write::stored, "the recipient turns one off");
	}
	{
		auto improved                                                                                = sender.snapshot().PromptMacros;
		std::ranges::find(improved.Differences.Own, renamed, &core::prompt_macro::Id)->Body.Template = "Look again, closely, at {pr.title}.";
		expect(macros->store(improved.Differences, improved.Revision) == application::prompt_macro_write::stored, "the sender edits one");
	}
	const auto here = core::prompt_packs_here(sender.snapshot().PromptMacros.Differences);
	expect(here.size() == 1U && here[0].Pack.Version == 1U, "the sender's profile holds the pack at version 1");
	expect(export_pack(sender, core::next_prompt_pack(here[0], { }), { renamed, uuidId }, file), "version 2 is recorded and written");

	preview        = recipient.read(file);
	const auto two = core::plan_prompt_pack_import(*preview.File.PromptPack, preview.Snapshot.PromptMacros.Differences, core::shipped_prompt_macros());
	expect(two.Items[0].Standing == core::prompt_pack_standing::updated && two.Items[1].Standing == core::prompt_pack_standing::unchanged && !two.Older, "one update, one unchanged");

	// An edit made after the preview refuses the import and changes nothing.
	{
		auto meanwhile                         = recipient.snapshot().PromptMacros;
		meanwhile.Differences.Own[0].Body.Name = "Mine, renamed";
		expect(adapters::sqlite::make_prompt_macro_store(second.path())->store(meanwhile.Differences, meanwhile.Revision) == application::prompt_macro_write::stored, "an edit in another window");
	}
	const auto edited = recipient.snapshot();
	applied           = recipient.apply(preview, core::default_prompt_pack_choices(two));
	expect(applied && applied->has_value() && (*applied)->first == application::agent_configuration_outcome::changed_since, "a concurrent edit refuses the import");
	expect(recipient.snapshot() == edited, "and nothing changed");

	preview = recipient.read(file);
	applied = recipient.apply(preview, core::default_prompt_pack_choices(core::plan_prompt_pack_import(*preview.File.PromptPack, preview.Snapshot.PromptMacros.Differences, core::shipped_prompt_macros())));
	expect(applied && applied->has_value() && (*applied)->first == application::agent_configuration_outcome::applied, "read again, the import applies");
	stored = recipient.snapshot().PromptMacros;
	expect(own(stored.Differences, renamed)->Body.Template == "Look again, closely, at {pr.title}." && own(stored.Differences, renamed)->Pack->Version == 2U, "updated in place at version 2");
	expect(!own(stored.Differences, uuidId)->On && own(stored.Differences, uuidId)->Pack->Version == 2U, "the recipient's Off stays");
	expect(stored.Differences.Own[0].Body.Name == "Mine, renamed" && !stored.Differences.Own[0].Pack && stored.Differences.Own.size() == 3U, "the recipient's own prompt is unchanged");

	// A file with no prompt chosen holds no pack.
	const auto plain = application::export_agent_configuration(sender.snapshot(), { }, { }, "Plain", "BetterPullReview test");
	expect(plain && plain->find("\"version\": 3") != std::string::npos && plain->find("prompt_pack") == std::string::npos, "no prompt chosen: no pack");

	// A store made before the pack table: the next open makes it, and the
	// prompts read as the reviewer's own; a link no prompt names is not read.
	{
		auto* db = static_cast<sqlite3*>(nullptr);
		expect(sqlite3_open((second.path() / "state/v1/agent-setup.sqlite3").string().c_str(), &db) == SQLITE_OK, "the file opens");
		expect(sqlite3_exec(db, "DROP TABLE prompt_macro_packs", nullptr, nullptr, nullptr) == SQLITE_OK, "an earlier build's store has no pack table");
		sqlite3_close(db);
	}
	const auto upgraded = adapters::sqlite::make_prompt_macro_store(second.path())->load();
	expect(upgraded && upgraded->Differences.Own.size() == 3U && std::ranges::none_of(upgraded->Differences.Own, [](const core::prompt_macro& aMacro) { return aMacro.Pack.has_value(); }),
		   "the table is made again and every prompt reads, unlinked");
	{
		auto* db = static_cast<sqlite3*>(nullptr);
		expect(sqlite3_open((second.path() / "state/v1/agent-setup.sqlite3").string().c_str(), &db) == SQLITE_OK, "the file opens again");
		const auto orphan = std::string{ "INSERT INTO prompt_macro_packs VALUES('gone','" } + packId + "','Team',1,'agent-tab','n','t','pull-request','question','',0,0)";
		expect(sqlite3_exec(db, orphan.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "a link whose prompt was deleted by an earlier build");
		sqlite3_close(db);
	}
	const auto orphaned = adapters::sqlite::make_prompt_macro_store(second.path())->load();
	expect(orphaned && orphaned->Differences.Own.size() == 3U, "a link no prompt names is not read");

	sender.Host.shutdown();
	recipient.Host.shutdown();
	std::cout << "prompt-pack-storage:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "FAILED: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
