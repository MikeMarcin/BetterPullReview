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
#include <array>
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
	/// [[BPR-2301]]. The Review section through the real workbench settings
	/// stores: one profile's Settings › Review exported, imported into
	/// another, written and read back; a file without Review changes nothing;
	/// values out of bounds are held, not refused.
	void review_settings_travel(const std::filesystem::path& aSender, const std::filesystem::path& aRecipient, const application::agent_configuration_snapshot& aSnapshot)
	{
		auto  senderStore              = adapters::sqlite::make_workbench_defaults_store({ .StorageRoot = aSender });
		auto  sender                   = (*senderStore)->load_preferences({ });
		auto& behaviour                = sender->Behaviour;
		behaviour.OpenModality         = application::review_open_modality::last_used;
		behaviour.MarkRead             = application::review_mark_read_behaviour::when_fully_seen;
		behaviour.CommentsFilter       = application::review_comments_filter::unresolved;
		behaviour.FeedbackMarks        = application::review_feedback_marks::letters;
		behaviour.LineDetails          = application::review_line_details::hidden;
		behaviour.CenterRibbonWidth    = 48U;
		behaviour.ReadingWidth         = core::reading_width::medium;
		behaviour.ReadingOverrides[1]  = core::reading_override_of(1480U);
		behaviour.ReadingOverrides[2]  = core::reading_override{ .Kind = core::reading_override_kind::full };
		behaviour.FinishReview.Approve = core::review_finish_return::pull_request;
		behaviour.OthersDrafts         = core::home_draft_placement::needs_me;
		const auto labels              = std::array{ core::feedback_label::issue, core::feedback_label::question };
		behaviour.LabelColumns         = core::make_feedback_label_columns(labels).value();
		expect((*senderStore)->store_preferences(*sender, { }).has_value(), "the sender's Review settings are stored");
		const auto stored = (*senderStore)->load_preferences({ });
		const auto review = application::portable_review_of(stored->Behaviour);
		const auto bytes  = application::export_agent_configuration(aSnapshot, { }, { }, "Review", "BetterPullReview test", { }, nullptr, { }, nullptr, nullptr, &review);
		expect(bytes && bytes->find("\"review\": {") != std::string::npos && bytes->find("\"discuss_width\": 1480") != std::string::npos, "the export carries the Review section");
		expect(bytes->find("\"others_drafts\": \"needs_me\"") != std::string::npos, "[[BPR-2326]] the export carries Home's Others' drafts");
		const auto decoded = core::decode_agent_configuration(*bytes, { });
		expect(decoded && decoded->Review && decoded->Review->Ignored.empty(), "the file reads back with its Review");

		auto       recipientStore = adapters::sqlite::make_workbench_defaults_store({ .StorageRoot = aRecipient });
		auto       recipient      = (*recipientStore)->load_preferences({ });
		const auto here           = recipient->Behaviour;
		const auto imported       = application::with_review(here, decoded->Review->Review);
		expect(application::valid(imported) && imported == application::edited(stored->Behaviour), "the import makes the recipient's Review settings the sender's");
		expect(application::review_differences(here, imported).size() == 12U, "each changed setting is named");
		expect(core::plan_review_import(*decoded->Review, application::review_differences(here, imported).size()).Standing == core::import_standing::updated, "it is an update");
		recipient->Behaviour = imported;
		expect((*recipientStore)->store_preferences(*recipient, { }).has_value(), "the imported settings are stored");
		expect((*recipientStore)->load_preferences({ })->Behaviour == imported, "and read back");

		// A file without Review: nothing to import, nothing changes.
		const auto bare = application::export_agent_configuration(aSnapshot, { }, { }, "Bare", "BetterPullReview test");
		expect(bare && !core::decode_agent_configuration(*bare, { })->Review, "a file without Review has none");

		// Out of bounds: held to Narrow's 720 and the ribbon's own bounds.
		const auto odd = core::decode_agent_configuration(
			R"({"format":"bpr.profile-settings","version":4,"exported_by":"","agents":[],"prompts":[],"review":{"threads_width":100,"center_ribbon_width":4000,"reading_width":"vast"}})", { });
		expect(odd && odd->Review, "an out-of-range Review still reads");
		const auto held = application::with_review(here, odd->Review->Review);
		expect(application::valid(held) && held.ReadingOverrides[0] == core::reading_override_of(720U) && held.CenterRibbonWidth == application::maximum_center_ribbon_width
				   && held.ReadingWidth == here.ReadingWidth,
			   "a narrow width is held to 720, the ribbon to its widest, an unknown preset leaves the Reading width as it was");
		// [[BPR-2326]]. A file from before Others' drafts leaves the choice alone.
		auto needsMe         = here;
		needsMe.OthersDrafts = core::home_draft_placement::needs_me;
		expect(application::with_review(needsMe, odd->Review->Review).OthersDrafts == core::home_draft_placement::needs_me, "a file without others_drafts leaves the placement as it was");
	}
} // namespace

int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "agent-configuration" };
	tests::sqlite_contract::scratch_root second{ "agent-configuration-recipient" };
	const auto&                          sender    = scratch.path();
	const auto&                          recipient = second.path();

	// The sender: a custom agent with relaxed checks, builds and scratch off,
	// Claude Code, and two edited prompts.
	auto senderAgents            = application::agent_setup_preferences{ };
	senderAgents.AgentsMoved     = true;
	senderAgents.RunLimitMinutes = 30U;
	senderAgents.Chosen          = core::setup_agent::claude_code;
	auto work                    = core::agent_entry{ .Id = 1U, .Name = "Work Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/Users/sender/bin/work-claude" };
	work.Arguments               = { "--profile", "work" };
	work.Model                   = "opus";
	work.StrictRunChecks         = false;
	work.BuildsAndTests          = true;
	work.ScratchStarts           = false;
	work.Identity                = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	auto claude                  = core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, { });
	claude.Id                    = 2U;
	claude.Identity              = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
	senderAgents.Agents          = { work, claude };
	senderAgents.PreferredAgent  = 2U;
	expect(adapters::sqlite::make_agent_setup_store(sender)->store(senderAgents), "the sender's agents are stored");
	store_hook(sender, core::writing_hook::comment, "State the problem in two sentences. /team-style");
	const auto senderStore = adapters::sqlite::make_agent_configuration_store(sender);
	expect(senderStore->record_written_against(core::writing_hook::comment, "State the problem in two sentences. /team-style", application::writing_hook_version), "provenance");
	store_hook(sender, core::writing_hook::enhance_writing, "Make it kind.");

	auto senderSnapshot = senderStore->load();
	expect(senderSnapshot.has_value(), "the sender's snapshot reads");
	expect(senderSnapshot->WrittenAgainst[1] == std::string{ application::writing_hook_version } && !senderSnapshot->WrittenAgainst[4],
		   "the store says which built-in an override was written against, and nothing for one it never recorded");
	const auto agents  = std::vector<std::uint32_t>{ 1U, 2U };
	const auto prompts = std::vector<application::exported_prompt>{ { .Purpose = "tour.stop-comment" }, { .Purpose = "enhance.writing" }, { .Purpose = "review.feedback", .BuiltIn = true }, { .Purpose = "review.skill", .BuiltIn = true } };
	const auto bytes   = application::export_agent_configuration(*senderSnapshot, agents, prompts, "Team", "BetterPullReview test");
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
	const auto beforeCrash = hooks_of(recipient);
	{
		auto* db = static_cast<sqlite3*>(nullptr);
		expect(sqlite3_open((recipient / "state/v1/agent-setup.sqlite3").string().c_str(), &db) == SQLITE_OK, "the file opens");
		for (std::size_t index = 0U; index < core::writing_hook_count; ++index) {
			const auto sql = "INSERT INTO configuration_import_journal VALUES(" + std::to_string(index) + "," + std::to_string(beforeCrash.Revision + 1U) + ",'"
				+ (index == 1U ? std::string{ "Before the import." } : beforeCrash.Overrides[index]) + "')";
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
	const auto localSnapshot = senderStore->load();
	expect(localSnapshot.has_value(), "the sender's snapshot reads again");
	const auto localPrompts = std::vector<application::exported_prompt>{ { .Purpose = "local.discussion" }, { .Purpose = "local.description", .BuiltIn = true } };
	const auto localBytes   = application::export_agent_configuration(*localSnapshot, { }, localPrompts, "Local", "BetterPullReview test");
	expect(localBytes && localBytes->find("local-description-v2") != std::string::npos && localBytes->find("Answer in two sentences, then propose.") != std::string::npos,
		   "both export, the shipped one with its version");
	const auto localDecoded = core::decode_agent_configuration(*localBytes, application::local_prompts(*store->load()));
	expect(localDecoded && localDecoded->Prompts.size() == 2U && !localDecoded->Prompts[0].Problem && !localDecoded->Prompts[1].Problem,
		   "this version recognizes both purposes on import");

	// [[BPR-2264]]. The Writing style Feature exports under its purpose ID
	// with its text as written (here naming a skill), and an import applies
	// it to the recipient's profile, where every request then sends it.
	store_hook(sender, core::writing_hook::writing_style, "Follow my house-style skill.");
	const auto styleSnapshot = senderStore->load();
	const auto stylePrompts  = std::vector<application::exported_prompt>{ { .Purpose = "feature.writing-style" } };
	const auto styleBytes    = application::export_agent_configuration(*styleSnapshot, { }, stylePrompts, "Style", "BetterPullReview test");
	expect(styleBytes && styleBytes->find("feature.writing-style") != std::string::npos && styleBytes->find("Follow my house-style skill.") != std::string::npos,
		   "the Writing style exports under its purpose ID with its text as written");
	auto styleWritten = false;
	host.write(file, *styleBytes, [&styleWritten](const bool aWritten) { styleWritten = aWritten; });
	effects.drain();
	decision.drain();
	auto stylePreview = std::optional<application::agent_configuration_preview>{ };
	host.read(file, [&stylePreview](std::expected<application::agent_configuration_preview, application::agent_configuration_read_error> aPreview) {
		if (aPreview) {
			stylePreview = std::move(*aPreview);
		}
	});
	effects.drain();
	decision.drain();
	expect(styleWritten && stylePreview && stylePreview->File.Prompts.size() == 1U && !stylePreview->File.Prompts[0].Problem, "this version recognizes the Feature on import");
	auto styleOutcome = std::optional<application::agent_configuration_host::applied>{ };
	host.apply(*stylePreview, { }, { 0U }, [&styleOutcome](application::agent_configuration_host::applied aApplied) { styleOutcome = std::move(aApplied); });
	effects.drain();
	decision.drain();
	const auto styleHooks = hooks_of(recipient);
	expect(styleOutcome && styleOutcome->has_value() && styleHooks.Overrides[static_cast<std::size_t>(core::writing_hook::writing_style)] == "Follow my house-style skill."
			   && application::writing_style_for(styleHooks, true).ends_with("Follow my house-style skill."),
		   "the import applies the override, and it is what a request sends");

	// [[BPR-2270]]. Comment labels exports under its own purpose ID, and a
	// blank override (the person's plain comments) travels as blank: the
	// recipient then sends no label guidance.
	store_hook(sender, core::writing_hook::comment_labels, std::string{ core::blank_feature_override });
	const auto labelsSnapshot = senderStore->load();
	const auto labelsPrompts  = std::vector<application::exported_prompt>{ { .Purpose = "feature.comment-labels" } };
	const auto labelsBytes    = application::export_agent_configuration(*labelsSnapshot, { }, labelsPrompts, "Labels", "BetterPullReview test");
	expect(labelsBytes && labelsBytes->find("feature.comment-labels") != std::string::npos, "Comment labels exports under its purpose ID");
	auto labelsWritten = false;
	host.write(file, *labelsBytes, [&labelsWritten](const bool aWritten) { labelsWritten = aWritten; });
	effects.drain();
	decision.drain();
	auto labelsPreview = std::optional<application::agent_configuration_preview>{ };
	host.read(file, [&labelsPreview](std::expected<application::agent_configuration_preview, application::agent_configuration_read_error> aPreview) {
		if (aPreview) {
			labelsPreview = std::move(*aPreview);
		}
	});
	effects.drain();
	decision.drain();
	expect(labelsWritten && labelsPreview && labelsPreview->File.Prompts.size() == 1U && !labelsPreview->File.Prompts[0].Problem, "this version recognizes Comment labels on import");
	auto labelsOutcome = std::optional<application::agent_configuration_host::applied>{ };
	host.apply(*labelsPreview, { }, { 0U }, [&labelsOutcome](application::agent_configuration_host::applied aApplied) { labelsOutcome = std::move(aApplied); });
	effects.drain();
	decision.drain();
	const auto labelsHooks = hooks_of(recipient);
	expect(labelsOutcome && labelsOutcome->has_value() && labelsHooks.Overrides[static_cast<std::size_t>(core::writing_hook::comment_labels)] == core::blank_feature_override
			   && application::comment_labels_instructions(labelsHooks).empty(),
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

	// [[BPR-2357]]. A profile with the current (post-tour-brief) hook IDs
	// already uses index 15 for Comment labels. Widen without renumbering it.
	{
		auto* db = static_cast<sqlite3*>(nullptr);
		expect(sqlite3_open((recipient / "state/v1/agent-setup.sqlite3").string().c_str(), &db) == SQLITE_OK, "the current profile opens");
		const auto earlier = "DROP TABLE prompt_provenance; CREATE TABLE prompt_provenance(hook INTEGER PRIMARY KEY CHECK(hook >= 0 AND hook <= 15),"
							 "text_digest BLOB NOT NULL CHECK(length(text_digest)=32),version TEXT NOT NULL CHECK(length(CAST(version AS BLOB)) BETWEEN 1 AND 64)) STRICT;"
							 "INSERT INTO prompt_provenance VALUES(15, zeroblob(32), 'comment-labels-v2');";
		expect(sqlite3_exec(db, earlier, nullptr, nullptr, nullptr) == SQLITE_OK, "the previous bound and its last hook are seeded");
		sqlite3_close(db);
	}
	const auto updated = adapters::sqlite::make_agent_configuration_store(recipient);
	expect(updated->record_written_against(core::writing_hook::review_update_from_target, "Run smoke checks.", application::review_update_from_target_hook_version),
		   "the new hook can record its version after migration");
	{
		auto* db = static_cast<sqlite3*>(nullptr);
		expect(sqlite3_open((recipient / "state/v1/agent-setup.sqlite3").string().c_str(), &db) == SQLITE_OK, "the widened profile opens");
		auto* statement = static_cast<sqlite3_stmt*>(nullptr);
		expect(sqlite3_prepare_v2(db, "SELECT count(*) FROM prompt_provenance WHERE (hook = 15 AND version = 'comment-labels-v2') OR (hook = 16 AND version = 'review-update-from-target-v1')", -1, &statement, nullptr) == SQLITE_OK
				   && sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 2,
			   "existing provenance retains its identity beside the update prompt");
		sqlite3_finalize(statement);
		sqlite3_close(db);
	}
	// RFC-BPR-0096, continue-at-latest amendment. A profile whose tables end
	// at the update prompt (16) widens to the reconcile prompt (17) in place.
	{
		auto* db = static_cast<sqlite3*>(nullptr);
		expect(sqlite3_open((recipient / "state/v1/agent-setup.sqlite3").string().c_str(), &db) == SQLITE_OK, "the profile opens again");
		const auto earlier = "DROP TABLE prompt_provenance; CREATE TABLE prompt_provenance(hook INTEGER PRIMARY KEY CHECK(hook >= 0 AND hook <= 16),"
							 "text_digest BLOB NOT NULL CHECK(length(text_digest)=32),version TEXT NOT NULL CHECK(length(CAST(version AS BLOB)) BETWEEN 1 AND 64)) STRICT;"
							 "INSERT INTO prompt_provenance VALUES(16, zeroblob(32), 'review-update-from-target-v1');";
		expect(sqlite3_exec(db, earlier, nullptr, nullptr, nullptr) == SQLITE_OK, "the update prompt's bound and its row are seeded");
		sqlite3_close(db);
	}
	const auto reconciled = adapters::sqlite::make_agent_configuration_store(recipient);
	expect(reconciled->record_written_against(core::writing_hook::reconcile_move, "Keep the saved branch.", application::reconcile_move_version),
		   "the reconcile prompt can record its version after migration");
	{
		auto* db = static_cast<sqlite3*>(nullptr);
		expect(sqlite3_open((recipient / "state/v1/agent-setup.sqlite3").string().c_str(), &db) == SQLITE_OK, "the widened profile opens");
		auto* statement = static_cast<sqlite3_stmt*>(nullptr);
		expect(sqlite3_prepare_v2(db, "SELECT count(*) FROM prompt_provenance WHERE (hook = 16 AND version = 'review-update-from-target-v1') OR (hook = 17 AND version = 'reconcile-move-v1')", -1, &statement, nullptr) == SQLITE_OK
				   && sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 2,
			   "existing provenance retains its identity beside the reconcile prompt");
		sqlite3_finalize(statement);
		sqlite3_close(db);
	}

	host.shutdown();
	expect(host.is_quiescent(), "quiescent after shutdown");
	review_settings_travel(sender, recipient, *senderSnapshot);
	std::cout << "agent-configuration-storage:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "FAILED: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
