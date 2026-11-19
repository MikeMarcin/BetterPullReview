// [[BPR-2016]], RFC-BPR-0107. Agents and prompts shared between two
// profiles through the real stores: an export of one profile's selection is
// imported into a fresh profile after its programs are bound, unrelated
// settings are kept, a stale preview changes nothing, a crash between the two
// settings files is undone, and nothing is launched.
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "bpr/application/agent_configuration_exchange.hpp"
#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/prompt_features.hpp"
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

	[[nodiscard]] auto hooks_of(const std::filesystem::path& aRoot) -> core::writing_hook_settings
	{
		auto store  = adapters::sqlite::make_workbench_defaults_store({ .StorageRoot = aRoot });
		auto loaded = (*store)->load_preferences({ });
		expect(loaded.has_value(), "the workbench settings read");
		return loaded->WritingHooks;
	}

	void store_hook(const std::filesystem::path& aRoot, const core::writing_hook aHook, std::string aText)
	{
		auto store                                                      = adapters::sqlite::make_workbench_defaults_store({ .StorageRoot = aRoot });
		auto loaded                                                     = (*store)->load_preferences({ });
		loaded->WritingHooks.Overrides[static_cast<std::size_t>(aHook)] = std::move(aText);
		++loaded->WritingHooks.Revision;
		expect((*store)->store_preferences(*loaded, { }).has_value(), "a hook is stored");
	}

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
} // namespace

int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "agent-configuration" };
	tests::sqlite_contract::scratch_root second{ "agent-configuration-recipient" };
	const auto                           sender    = scratch.path();
	const auto                           recipient = second.path();

	// The sender: a custom agent with relaxed checks, builds and scratch off,
	// Claude Code, and two edited prompts.
	auto sender_agents            = application::agent_setup_preferences{ };
	sender_agents.AgentsMoved     = true;
	sender_agents.RunLimitMinutes = 30U;
	sender_agents.Chosen          = core::setup_agent::claude_code;
	auto work                     = core::agent_entry{ .Id = 1U, .Name = "Work Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/Users/sender/bin/work-claude" };
	work.Arguments                = { "--profile", "work" };
	work.Model                    = "opus";
	work.StrictRunChecks          = false;
	work.BuildsAndTests           = true;
	work.ScratchStarts            = false;
	work.Identity                 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	auto claude                   = core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, { });
	claude.Id                     = 2U;
	claude.Identity               = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
	sender_agents.Agents          = { work, claude };
	sender_agents.PreferredAgent  = 2U;
	expect(adapters::sqlite::make_agent_setup_store(sender)->store(sender_agents), "the sender's agents are stored");
	store_hook(sender, core::writing_hook::comment, "State the problem in two sentences. /team-style");
	const auto sender_store = adapters::sqlite::make_agent_configuration_store(sender);
	expect(sender_store->record_written_against(core::writing_hook::comment, "State the problem in two sentences. /team-style", application::writing_hook_version), "provenance");
	store_hook(sender, core::writing_hook::enhance_writing, "Make it kind.");

	auto sender_snapshot = sender_store->load();
	expect(sender_snapshot.has_value(), "the sender's snapshot reads");
	expect(sender_snapshot->WrittenAgainst[1] == std::string{ application::writing_hook_version } && !sender_snapshot->WrittenAgainst[4],
		   "the store says which built-in an override was written against, and nothing for one it never recorded");
	const auto agents  = std::vector<std::uint32_t>{ 1U, 2U };
	const auto prompts = std::vector<application::exported_prompt>{ { .Purpose = "tour.stop-comment" }, { .Purpose = "enhance.writing" }, { .Purpose = "review.feedback", .BuiltIn = true }, { .Purpose = "review.skill", .BuiltIn = true } };
	const auto bytes   = application::export_agent_configuration(*sender_snapshot, agents, prompts, "Team", "BetterPullReview test");
	expect(bytes.has_value(), "the export encodes");
	// AC02: nothing of the sender's machine.
	expect(bytes->find("/Users/sender") == std::string::npos && bytes->find("aaaaaaaa") == std::string::npos && bytes->find("run_limit") == std::string::npos
			   && bytes->find("\"preferred") == std::string::npos,
		   "no path, identity or unselected setting is exported");

	// The recipient: its own unrelated settings and a prompt it changed.
	auto mine            = application::agent_setup_preferences{ };
	mine.AgentsMoved     = true;
	mine.RunLimitMinutes = 5U;
	mine.ScratchLimitMiB = 512U;
	auto codex           = core::preset_agent_entry(core::setup_agent::codex, core::codex_protocol_choice::app_server, { });
	codex.Id             = 1U;
	codex.Identity       = "cccccccccccccccccccccccccccccccc";
	mine.Agents          = { codex };
	expect(adapters::sqlite::make_agent_setup_store(recipient)->store(mine), "the recipient's agents are stored");
	store_hook(recipient, core::writing_hook::outline, "My outline prompt.");

	auto       effects  = queue{ };
	auto       decision = queue{ };
	auto       launched = 0;
	const auto store    = adapters::sqlite::make_agent_configuration_store(recipient);
	const auto file     = (sender / "team.json").string();
	auto       host     = application::agent_configuration_host{ application::agent_configuration_dependencies{
		.Store        = store.get(),
		.ReadFile     = read_file,
		.WriteFile    = write_file,
		.MintIdentity = [&launched] { return std::string(32U, static_cast<char>('0' + (launched++ % 10))); },
		.Effects  = &effects,
		.Decision = &decision,
	} };
	auto       written  = false;
	host.write(file, *bytes, [&written](const bool aWritten) { written = aWritten; });
	effects.drain();
	decision.drain();
	expect(written, "the export is written");

	auto preview = std::optional<application::agent_configuration_preview>{ };
	host.read(file, [&preview](std::expected<application::agent_configuration_preview, application::agent_configuration_read_error> aPreview) {
		expect(aPreview.has_value(), "the file is offered for preview");
		preview = std::move(*aPreview);
	});
	effects.drain();
	decision.drain();
	expect(preview && preview->File.Agents.size() == 2U && preview->File.Prompts.size() == 4U, "both agents and four prompts are previewed");
	expect(adapters::sqlite::make_agent_setup_store(recipient)->load() == mine && hooks_of(recipient).Overrides[0] == "My outline prompt.", "a preview changes nothing");

	// AC01: bind the custom agent to this computer's program; Claude Code is
	// found by setup. Relaxed checks and builds are not accepted.
	const auto choices = std::vector<core::agent_import_choice>{
		{ .Item = 0U, .Action = core::agent_import_action::add, .Program = "/opt/recipient/claude-wrapper" },
		{ .Item = 1U, .Action = core::agent_import_action::add },
	};
	auto outcome = std::optional<application::agent_configuration_host::applied>{ };
	host.apply(*preview, choices, { 0U, 1U, 2U, 3U }, [&outcome](application::agent_configuration_host::applied aApplied) { outcome = std::move(aApplied); });
	effects.drain();
	decision.drain();
	expect(outcome && outcome->has_value() && (*outcome)->first == application::agent_configuration_outcome::applied, "the import applies");

	// A restart reads the result back from the stores.
	const auto after = adapters::sqlite::make_agent_setup_store(recipient)->load();
	expect(after && after->Agents.size() == 3U, "two agents added after the recipient's own");
	expect(after->Agents[0] == codex, "the existing agent and its order are kept");
	const auto& added = after->Agents[1];
	expect(added.Name == "Work Claude" && added.Program == "/opt/recipient/claude-wrapper" && added.Arguments == work.Arguments && added.Model == "opus", "equivalent configured behaviour");
	expect(added.StrictRunChecks && !added.BuildsAndTests && !added.ScratchStarts, "strict, no builds, scratch Off as requested");
	expect(added.Identity.size() == 32U && added.Identity != work.Identity && after->Agents[2].Identity != claude.Identity, "new identities, none carried over");
	expect(after->Agents[2].Preset == core::setup_agent::claude_code && after->Agents[2].Program.empty(), "Claude Code is the recipient's own, looked for at launch");
	expect(after->RunLimitMinutes == 5U && after->ScratchLimitMiB == 512U && after->PreferredAgent == 0U, "unrelated agent settings are kept");
	const auto hooks = hooks_of(recipient);
	expect(hooks.Overrides[1] == "State the problem in two sentences. /team-style" && hooks.Overrides[4] == "Make it kind." && hooks.Overrides[0] == "My outline prompt.",
		   "imported prompts applied; the recipient's own kept");
	expect(hooks.Overrides[2].empty(), "a shipped default imported as itself stays the default");
	const auto reloaded = store->load();
	expect(reloaded->WrittenAgainst[1] == std::string{ application::writing_hook_version } && !reloaded->WrittenAgainst[4], "provenance travels with the override");
	expect(!reloaded->ReviewSkill, "the shipped review skill imported as itself is no copy");
	expect(launched == 2, "only identities were minted; no process, check or network");

	// A file edited after a preview is not applied over the edit.
	host.read(file, [&preview](std::expected<application::agent_configuration_preview, application::agent_configuration_read_error> aPreview) { preview = std::move(*aPreview); });
	effects.drain();
	decision.drain();
	store_hook(recipient, core::writing_hook::feedback, "Edited through bpr meanwhile.");
	const auto replace = std::vector<core::agent_import_choice>{ { .Item = 0U, .Action = core::agent_import_action::replace, .Target = 2U } };
	outcome.reset();
	host.apply(*preview, replace, { }, [&outcome](application::agent_configuration_host::applied aApplied) { outcome = std::move(aApplied); });
	effects.drain();
	decision.drain();
	expect(outcome && (*outcome)->first == application::agent_configuration_outcome::changed_since, "a concurrent edit refuses the application");
	expect(adapters::sqlite::make_agent_setup_store(recipient)->load() == after, "and nothing changed");

	// A crash after the hooks were written and before the agents were: the
	// journal undoes the hooks at the next start.
	const auto before_crash = hooks_of(recipient);
	{
		auto* db = static_cast<sqlite3*>(nullptr);
		expect(sqlite3_open((recipient / "state/v1/agent-setup.sqlite3").string().c_str(), &db) == SQLITE_OK, "the file opens");
		for (std::size_t index = 0U; index < core::writing_hook_count; ++index) {
			const auto sql = "INSERT INTO configuration_import_journal VALUES(" + std::to_string(index) + "," + std::to_string(before_crash.Revision + 1U) + ",'"
				+ (index == 1U ? std::string{ "Before the import." } : before_crash.Overrides[index]) + "')";
			expect(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "a journal row");
		}
		sqlite3_close(db);
	}
	store_hook(recipient, core::writing_hook::comment, "Half-applied import text.");
	expect(store->recover(), "recovery runs");
	const auto recovered = hooks_of(recipient);
	expect(recovered.Overrides[1] == "Before the import." && recovered.Overrides[2] == "Edited through bpr meanwhile.", "the half-applied hooks are undone");
	expect(store->recover(), "an empty journal has nothing to do");

	// A file that is not a configuration is named before any change.
	expect(write_file(file, "{\"format\":\"something-else\",\"version\":1}"), "a foreign file");
	auto refused = std::optional<application::agent_configuration_read_error>{ };
	host.read(file, [&refused](std::expected<application::agent_configuration_preview, application::agent_configuration_read_error> aPreview) { refused = aPreview.error(); });
	effects.drain();
	decision.drain();
	expect(refused && refused->Refusal == core::configuration_file_refusal::not_a_configuration, "a foreign file is refused by name");

	// RFC-BPR-0110 AC04: the two local-authoring prompts are prompts like the
	// others: an edited one and a shipped one export with their versions and
	// are recognized on import.
	store_hook(sender, core::writing_hook::local_discussion, "Answer in two sentences, then propose.");
	const auto local_snapshot = sender_store->load();
	expect(local_snapshot.has_value(), "the sender's snapshot reads again");
	const auto local_prompts = std::vector<application::exported_prompt>{ { .Purpose = "local.discussion" }, { .Purpose = "local.description", .BuiltIn = true } };
	const auto local_bytes   = application::export_agent_configuration(*local_snapshot, { }, local_prompts, "Local", "BetterPullReview test");
	expect(local_bytes && local_bytes->find("local-description-v2") != std::string::npos && local_bytes->find("Answer in two sentences, then propose.") != std::string::npos,
		   "both export, the shipped one with its version");
	const auto local_decoded = core::decode_agent_configuration(*local_bytes, application::local_prompts(*store->load()));
	expect(local_decoded && local_decoded->Prompts.size() == 2U && !local_decoded->Prompts[0].Problem && !local_decoded->Prompts[1].Problem,
		   "this version recognizes both purposes on import");

	// [[BPR-2264]]. The Writing style Feature exports under its purpose ID
	// with its text as written (here naming a skill), and an import applies
	// it to the recipient's profile, where every request then sends it.
	store_hook(sender, core::writing_hook::writing_style, "Follow my house-style skill.");
	const auto style_snapshot = sender_store->load();
	const auto style_prompts  = std::vector<application::exported_prompt>{ { .Purpose = "feature.writing-style" } };
	const auto style_bytes    = application::export_agent_configuration(*style_snapshot, { }, style_prompts, "Style", "BetterPullReview test");
	expect(style_bytes && style_bytes->find("feature.writing-style") != std::string::npos && style_bytes->find("Follow my house-style skill.") != std::string::npos,
		   "the Writing style exports under its purpose ID with its text as written");
	auto style_written = false;
	host.write(file, *style_bytes, [&style_written](const bool aWritten) { style_written = aWritten; });
	effects.drain();
	decision.drain();
	auto style_preview = std::optional<application::agent_configuration_preview>{ };
	host.read(file, [&style_preview](std::expected<application::agent_configuration_preview, application::agent_configuration_read_error> aPreview) {
		if (aPreview) {
			style_preview = std::move(*aPreview);
		}
	});
	effects.drain();
	decision.drain();
	expect(style_written && style_preview && style_preview->File.Prompts.size() == 1U && !style_preview->File.Prompts[0].Problem, "this version recognizes the Feature on import");
	auto style_outcome = std::optional<application::agent_configuration_host::applied>{ };
	host.apply(*style_preview, { }, { 0U }, [&style_outcome](application::agent_configuration_host::applied aApplied) { style_outcome = std::move(aApplied); });
	effects.drain();
	decision.drain();
	const auto style_hooks = hooks_of(recipient);
	expect(style_outcome && style_outcome->has_value() && style_hooks.Overrides[static_cast<std::size_t>(core::writing_hook::writing_style)] == "Follow my house-style skill."
			   && application::writing_style_for(style_hooks, true).ends_with("Follow my house-style skill."),
		   "the import applies the override, and it is what a request sends");

	// [[BPR-2270]]. Comment labels exports under its own purpose ID, and a
	// blank override (the person's plain comments) travels as blank: the
	// recipient then sends no label guidance.
	store_hook(sender, core::writing_hook::comment_labels, std::string{ core::blank_feature_override });
	const auto labels_snapshot = sender_store->load();
	const auto labels_prompts  = std::vector<application::exported_prompt>{ { .Purpose = "feature.comment-labels" } };
	const auto labels_bytes    = application::export_agent_configuration(*labels_snapshot, { }, labels_prompts, "Labels", "BetterPullReview test");
	expect(labels_bytes && labels_bytes->find("feature.comment-labels") != std::string::npos, "Comment labels exports under its purpose ID");
	auto labels_written = false;
	host.write(file, *labels_bytes, [&labels_written](const bool aWritten) { labels_written = aWritten; });
	effects.drain();
	decision.drain();
	auto labels_preview = std::optional<application::agent_configuration_preview>{ };
	host.read(file, [&labels_preview](std::expected<application::agent_configuration_preview, application::agent_configuration_read_error> aPreview) {
		if (aPreview) {
			labels_preview = std::move(*aPreview);
		}
	});
	effects.drain();
	decision.drain();
	expect(labels_written && labels_preview && labels_preview->File.Prompts.size() == 1U && !labels_preview->File.Prompts[0].Problem, "this version recognizes Comment labels on import");
	auto labels_outcome = std::optional<application::agent_configuration_host::applied>{ };
	host.apply(*labels_preview, { }, { 0U }, [&labels_outcome](application::agent_configuration_host::applied aApplied) { labels_outcome = std::move(aApplied); });
	effects.drain();
	decision.drain();
	const auto labels_hooks = hooks_of(recipient);
	expect(labels_outcome && labels_outcome->has_value() && labels_hooks.Overrides[static_cast<std::size_t>(core::writing_hook::comment_labels)] == core::blank_feature_override
			   && application::comment_labels_instructions(labels_hooks).empty(),
		   "the blank override arrives blank, and the recipient sends no label guidance");

	// RFC-BPR-0110: a store an earlier build made checks hooks 0 to 7; the
	// next open rebuilds both hook tables, keeping their rows. [[BPR-2274]]:
	// the retired Problem row (0) goes, and the outline's (2) is now 0.
	{
		auto* db = static_cast<sqlite3*>(nullptr);
		expect(sqlite3_open((recipient / "state/v1/agent-setup.sqlite3").string().c_str(), &db) == SQLITE_OK, "the file opens");
		const auto narrow = std::string{ "DROP TABLE prompt_provenance; CREATE TABLE prompt_provenance(hook INTEGER PRIMARY KEY CHECK(hook BETWEEN 0 AND 7),"
										 "text_digest BLOB NOT NULL CHECK(length(text_digest)=32),version TEXT NOT NULL CHECK(length(CAST(version AS BLOB)) BETWEEN 1 AND 64)) STRICT;"
										 "INSERT INTO prompt_provenance VALUES(0, zeroblob(32), 'review-preparation-v1');"
										 "INSERT INTO prompt_provenance VALUES(2, zeroblob(32), 'review-preparation-v1');"
										 "DROP TABLE configuration_import_journal; CREATE TABLE configuration_import_journal(hook INTEGER PRIMARY KEY CHECK(hook BETWEEN 0 AND 7),"
										 "revision_after INTEGER NOT NULL CHECK(revision_after >= 1),text TEXT NOT NULL CHECK(length(CAST(text AS BLOB)) <= 8192)) STRICT" };
		expect(sqlite3_exec(db, narrow.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "an earlier build's narrow hook tables");
		sqlite3_close(db);
	}
	const auto widened = adapters::sqlite::make_agent_configuration_store(recipient);
	expect(widened->record_written_against(core::writing_hook::local_discussion, "Be brief.", application::local_discussion_hook_version), "the widened table takes hook 7");
	{
		auto* db = static_cast<sqlite3*>(nullptr);
		expect(sqlite3_open((recipient / "state/v1/agent-setup.sqlite3").string().c_str(), &db) == SQLITE_OK, "the file opens again");
		auto* statement = static_cast<sqlite3_stmt*>(nullptr);
		expect(sqlite3_prepare_v2(db, "SELECT count(*) FROM prompt_provenance WHERE hook IN (0, 7)", -1, &statement, nullptr) == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW
				   && sqlite3_column_int(statement, 0) == 2,
			   "the earlier row is kept beside the new one");
		sqlite3_finalize(statement);
		sqlite3_close(db);
	}

	host.shutdown();
	expect(host.is_quiescent(), "quiescent after shutdown");
	std::cout << "agent-configuration-storage:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "FAILED: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
