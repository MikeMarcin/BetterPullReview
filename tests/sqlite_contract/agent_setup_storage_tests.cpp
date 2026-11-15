// [[BPR-1749]], RFC-BPR-0092. A profile's agent setup preferences:
// defaults before any write, a restart reads them back, refused values are not
// written, an identity mismatch is refused, and the file is owner-private.
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "scratch_root.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#ifndef _WIN32
	#include <sys/stat.h>
#endif

namespace {
	using namespace bpr;
	using tests::expect;
} // namespace

int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "agent-setup" };
	const auto                           path  = scratch.path() / "state/v1/agent-setup.sqlite3";
	auto                                 store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(!std::filesystem::exists(path), "construction writes nothing");
	const auto first = store->load();
	expect(first && *first == application::agent_setup_preferences{ }, "a profile with nothing stored reads defaults");

	auto chosen           = application::agent_setup_preferences{ };
	chosen.Chosen         = core::setup_agent::codex;
	chosen.CodexProgram   = "/opt/tools/codex-wrapper";
	chosen.HarnessProgram = "/opt/corp/harness";
	chosen.CodexProtocol  = core::codex_protocol_choice::jsonl;
	expect(store->store(chosen), "preferences are stored");
	store.reset();
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() == chosen, "a restarted profile reads the same preferences back");

	auto none   = chosen;
	none.Chosen = std::nullopt;
	expect(store->store(none) && store->load() == none, "no chosen agent is kept as none");

	auto tooLong         = chosen;
	tooLong.CodexProgram = "/" + std::string(2000U, 'a');
	expect(!store->store(tooLong), "an over-long program is not written");
	auto control           = chosen;
	control.HarnessProgram = "/bin/harness\n--flag";
	expect(!store->store(control), "a program with a control character is not written");
	expect(store->load() == none, "a refused write leaves the stored preferences as they were");

	// [[BPR-1750]]. Clone folders and repository links in the same file.
	auto links = adapters::sqlite::make_clone_link_store(scratch.path());
	expect(links->load_clone_folders() == std::vector<std::string>{ } && links->load_links() == std::vector<application::repository_link>{ }, "no folders or links at first");
	const auto folders = std::vector<std::string>{ "/Users/me/Documents/GitHub", "/Users/me/src" };
	expect(links->store_clone_folders(folders) && links->load_clone_folders() == folders, "folders keep their order");
	expect(links->store_clone_folders(std::vector<std::string>{ "/Users/me/src" }) && links->load_clone_folders() == std::vector<std::string>{ "/Users/me/src" },
		   "storing folders replaces them");
	auto tooMany = std::vector<std::string>(core::maximum_clone_folders + 1U, "/x");
	expect(!links->store_clone_folders(tooMany), "more than the folder bound is refused");
	auto link       = application::repository_link{ };
	link.Host       = "github.com";
	link.Repository = 42U;
	link.Owner      = "bpr-fixtures";
	link.Name       = "engine";
	link.Path       = "/Users/me/src/engine";
	link.Remote     = "origin";
	expect(links->store_link(link), "a link is stored");
	auto copy        = link;
	copy.Repository  = 43U;
	copy.Name        = "other";
	copy.Path        = { };
	copy.Remote      = { };
	copy.PrivateCopy = true;
	expect(links->store_link(copy), "a private copy choice is stored");
	auto invalid        = link;
	invalid.PrivateCopy = true;
	expect(!links->store_link(invalid), "a private copy with a path is refused");
	links.reset();
	links          = adapters::sqlite::make_clone_link_store(scratch.path());
	const auto all = links->load_links();
	expect(all && all->size() == 2U && (*all)[0] == link && (*all)[1] == copy, "links read back after a restart");
	expect(links->forget_link("github.com", 42U, "bpr-fixtures", "engine") && links->load_links()->size() == 1U, "Forget removes one link");
	expect(store->load() == none, "the preferences are untouched by links");

	// [[BPR-1752]]. The edited review skill and its record.
	using loaded_skill = std::optional<std::optional<application::stored_review_skill>>;
	const auto noSkill = loaded_skill{ std::in_place, std::nullopt };
	auto       skills  = adapters::sqlite::make_review_skill_store(scratch.path());
	expect(skills->load_review_skill() == noSkill, "no edited skill reads as none");
	auto edited               = application::stored_review_skill{ .Text = "---\nname: bpr-pr-review\ndescription: d\n---\nMine.\n" };
	edited.Record.BaseVersion = 3U;
	edited.Record.BaseDigest.fill(0xABU);
	expect(skills->store_review_skill(edited), "an edited skill is stored");
	skills.reset();
	skills = adapters::sqlite::make_review_skill_store(scratch.path());
	expect(skills->load_review_skill() == loaded_skill{ std::in_place, edited }, "and reads back whole after a restart, with no kept version");
	edited.Record.KeptVersion = 4U;
	expect(skills->store_review_skill(edited) && skills->load_review_skill() == loaded_skill{ std::in_place, edited }, "Keep mine's version is stored");
	expect(!skills->store_review_skill({ .Text = std::string(core::maximum_review_skill_bytes + 1U, 'x') }), "a skill over its bound is refused");
	expect(skills->forget_review_skill() && skills->load_review_skill() == noSkill, "Restore default removes it");
	expect(store->load() == none && links->load_links()->size() == 1U, "the preferences and links are untouched by the skill");

	// [[BPR-1753]], owner decision (b). Each pull request's conversation.
	using turns       = std::vector<application::agent_conversation_turn>;
	auto       talks  = adapters::sqlite::make_agent_conversation_store(scratch.path());
	const auto pull   = application::agent_conversation_key{ .Host = "github.com", .Owner = "MikeMarcin", .Name = "BPR-Test", .Number = 7U };
	auto       other  = pull;
	other.Number      = 8U;
	const auto thread = turns{ { .Asked = "Does this work?", .Answered = "Yes: the loop ends.", .Ending = "Claude Code answered in 0:12." },
							   { .Asked = "Why?", .Answered = "Because the bound is checked.", .Ending = "Claude Code answered in 0:08." } };
	expect(talks->load(pull) == std::optional{ turns{ } }, "a pull request with no conversation reads as none");
	expect(talks->store(pull, thread) && talks->store(other, { thread.front() }), "conversations are stored per pull request");
	talks.reset();
	talks = adapters::sqlite::make_agent_conversation_store(scratch.path());
	expect(talks->load(pull) == std::optional{ thread } && talks->load(other) == std::optional{ turns{ thread.front() } }, "and read back in order after a restart");
	expect(talks->store(pull, { }) && talks->load(pull) == std::optional{ turns{ } } && talks->load(other)->size() == 1U, "Clear session removes only that pull request's");
	expect(!talks->store(pull, turns(application::maximum_conversation_turns + 1U)), "more turns than the bound are refused");
	expect(!talks->store(application::agent_conversation_key{ }, thread), "a conversation names its pull request");
	// Owner decision 2026-10-23: one conversation per agent, and the agent a
	// pull request was last asked with.
	auto workCodex  = other;
	workCodex.Agent = 2U;
	expect(talks->store(workCodex, thread) && talks->load(workCodex) == std::optional{ thread } && talks->load(other)->size() == 1U, "each agent keeps its own conversation");
	expect(!talks->load_agent(other) && talks->store_agent(workCodex) && talks->load_agent(other) == std::optional<std::uint32_t>{ 2U }, "the agent last asked with is kept per pull request");
	// [[BPR-1755]]: Reset agents forgets those choices, never a turn.
	expect(talks->forget_agents() && !talks->load_agent(other) && talks->load(workCodex) == std::optional{ thread }, "forgetting the agents' choices keeps every conversation");
	expect(talks->store_agent(workCodex), "a choice is kept again afterwards");
	expect(store->load() == none && links->load_links()->size() == 1U, "the preferences and links are untouched by conversations");
	// [[BPR-1847]]. How each turn ended is kept with it, and a turn stored
	// before outcomes were kept reads back from the ending this app wrote.
	auto failed      = pull;
	failed.Number    = 9U;
	const auto ended = turns{ { .Asked = "Review this pull request", .Answered = "my-agent answered in a form this app can't read.", .Ending = "Stopped after 0:15.", .Outcome = bpr::core::agent_turn_outcome::unreadable },
							  { .Asked = "Why?", .Answered = "Because.", .Ending = "my-agent answered in 0:08.", .Outcome = bpr::core::agent_turn_outcome::answered },
							  { .Asked = "Again", .Answered = "Stopped before my-agent answered.", .Ending = "Stopped after 0:02.", .Outcome = bpr::core::agent_turn_outcome::stopped } };
	expect(talks->store(failed, ended), "a thread with its turns' outcomes is stored");
	talks.reset();
	talks = adapters::sqlite::make_agent_conversation_store(scratch.path());
	expect(talks->load(failed) == std::optional{ ended }, "each turn reads back with how it ended");
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(path.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
		expect(sqlite3_exec(database, "UPDATE agent_conversation_outcomes SET outcome=10", nullptr, nullptr, nullptr) != SQLITE_OK, "an undeclared outcome is refused by the schema");
		expect(sqlite3_exec(database, "DELETE FROM agent_conversation_outcomes WHERE number=9", nullptr, nullptr, nullptr) == SQLITE_OK, "a thread stored before outcomes were kept");
		sqlite3_close_v2(database);
	}
	auto legacy       = ended;
	legacy[0].Outcome = bpr::core::agent_turn_outcome::ended_without_answer;
	legacy[2].Outcome = bpr::core::agent_turn_outcome::ended_without_answer;
	expect(talks->load(failed) == std::optional{ legacy }, "an old stopped turn reads back as one that ended without an answer, an old answer as an answer");
	expect(talks->store(failed, { }) && talks->load(failed) == std::optional{ turns{ } }, "Clear session removes the outcomes with the turns");

	// Owner report 2026-11-10, after BPR-2052. What was done with each finding
	// card of a stored answer, read back after a restart.
	{
		auto       cards   = adapters::sqlite::make_agent_finding_card_store(scratch.path());
		const auto block   = core::digest_agent_findings("{\"findings\":[]}");
		const auto drafted = core::agent_finding_card_record{ .Findings = block, .Ordinal = 1U, .State = core::agent_finding_card_disposition::drafted };
		const auto refused = core::agent_finding_card_record{ .Findings = block, .Ordinal = 3U, .State = core::agent_finding_card_disposition::refused, .Why = "Not drafted: resolve the draft open at that line first." };
		const auto noted   = core::agent_finding_card_record{ .Findings = block, .Ordinal = 2U, .State = core::agent_finding_card_disposition::noted, .Note = 42U };
		expect(cards->load() == std::optional{ std::vector<core::agent_finding_card_record>{ } }, "no card is kept at first");
		expect(cards->store(drafted) && cards->store(refused) && cards->store(noted), "drafted, refused and noted cards are kept");
		cards.reset();
		cards = adapters::sqlite::make_agent_finding_card_store(scratch.path());
		expect(cards->load() == std::optional{ std::vector{ drafted, refused, noted } }, "a restart reads them back, least recently changed first");
		auto undone  = refused;
		undone.State = core::agent_finding_card_disposition::open;
		expect(cards->store(undone) && cards->load() == std::optional{ std::vector{ drafted, noted } }, "an open card has no row");
		expect(cards->store(drafted) && cards->load() == std::optional{ std::vector{ noted, drafted } }, "storing a card again makes it the most recently changed");
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(path.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
		expect(sqlite3_exec(database, "UPDATE agent_finding_cards SET state=5", nullptr, nullptr, nullptr) != SQLITE_OK, "an undeclared disposition is refused by the schema");
		expect(sqlite3_exec(database, "UPDATE agent_finding_cards SET state=0", nullptr, nullptr, nullptr) != SQLITE_OK, "an open card is never a row");
		sqlite3_close_v2(database);
		expect(store->load() == none, "the preferences are untouched by finding cards");
	}

	// [[BPR-1753]], owner amendment 2026-10-23. The agent list.
	auto listed            = *store->load();
	listed.Agents          = *core::add_agent_entry({ }, { .Name = "Work Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/wrapper" });
	listed.Agents          = *core::add_agent_entry(listed.Agents, { .Name = "Work Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/work/bin/wrapper", .Arguments = { "--codex", "--profile=work" }, .Effort = "high", .CheckArguments = { "--codex", "login", "status" } });
	listed.Agents          = *core::add_agent_entry(listed.Agents, core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, { }));
	listed.AgentsMoved     = true;
	listed.RunLimitMinutes = 45U;
	listed.PreferredAgent  = 2U;
	// Owner look v004: the Agent tab's Suggestions toggle is kept.
	listed.SuggestionsHidden = true;
	// RFC-BPR-0126 Q4/Q5: Follow-up behaviour, Steer, is kept.
	listed.FollowUpSteer      = true;
	listed.Agents[0].Identity = std::string(32U, 'a');
	expect(store->load()->ScratchLimitMiB == core::default_agent_scratch_limit_mib && store->load()->ScratchKeepDays == core::default_agent_scratch_keep_days, "the scratch policy starts at 256 MiB and 30 days");
	// RFC-BPR-0104 as amended 2026-11-11: the scratch folder limit and how
	// long an unused folder is kept are settings.
	listed.ScratchLimitMiB = 1024U;
	listed.ScratchKeepDays = 0U;
	expect(store->store(listed), "an agent list is stored");
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() == listed, "and reads back in order with its fixed arguments, preset and choice after a restart");
	// [[BPR-1918]] AC3. A custom agent's strict run checks are its own,
	// stored through the host and read back after a restart.
	listed.Agents[1].StrictRunChecks = false;
	expect(store->store(listed), "a custom agent with strict run checks off is stored");
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() == listed && store->load()->Agents[0].StrictRunChecks && !store->load()->Agents[1].StrictRunChecks, "it reads back off, and only for that agent");
	auto popular                      = listed;
	popular.Agents[2].StrictRunChecks = false;
	expect(core::inspect_agent_entries(popular.Agents) == core::agent_entry_refusal::relaxed_popular_agent && !store->store(popular), "a popular agent keeps strict run checks");
	// [[BPR-2051]]. Each agent's last check is kept across a restart: a pass
	// with its version, a failure with why; a check of an agent no longer in
	// the list does not come back.
	listed.EntryChecks = { { .Id = listed.Agents[0].Id, .Check = { .State = core::setup_check_state::passed, .Version = "2.1.321" } },
						   { .Id = listed.Agents[1].Id, .Check = core::fail_check(core::agent_setup_failure::not_signed_in) },
						   { .Id = listed.Agents[2].Id, .Check = core::fail_check(core::agent_setup_failure::timed_out) } };
	expect(store->store(listed), "the agents' last checks are stored");
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() == listed, "and read back after a restart, with versions and failures");
	auto running                 = listed;
	running.EntryChecks[0].Check = { .State = core::setup_check_state::running };
	expect(!store->store(running), "a running check is not a result to keep");
	listed.Agents.pop_back();
	listed.EntryChecks.pop_back();
	expect(store->store(listed) && store->load()->Agents.size() == 2U && store->load()->EntryChecks.size() == 2U, "a removed agent is gone from the stored list, with its check");
	// [[BPR-2007]], [[BPR-2008]], RFC-BPR-0104 AC08. Scratch folder starts
	// and Build and run tests are each agent's, kept across a restart; an
	// agent stored before them reads back on and off.
	expect(store->load()->Agents[0].ScratchStarts && !store->load()->Agents[0].BuildsAndTests, "an agent carried over starts with the folder on and builds off");
	listed.Agents[0].ScratchStarts  = false;
	listed.Agents[1].BuildsAndTests = true;
	expect(store->store(listed), "the grant settings are stored");
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() == listed && !store->load()->Agents[0].ScratchStarts && store->load()->Agents[1].ScratchStarts && store->load()->Agents[1].BuildsAndTests,
		   "and read back per agent after a restart");
	// [[BPR-2139]], [[BPR-2140]], RFC-BPR-0104 AC10 (owner, 2026-11-12). MCP
	// servers and the instructions switch are each agent's; a profile written
	// before them (no such table) reads back Off and the system prompt.
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(path.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
		expect(sqlite3_exec(database, "DROP TABLE agent_entry_launch", nullptr, nullptr, nullptr) == SQLITE_OK, "a profile from before the setting");
		sqlite3_close_v2(database);
		store              = adapters::sqlite::make_agent_setup_store(scratch.path());
		const auto carried = store->load();
		expect(carried && carried->Agents.size() == 2U
				   && std::ranges::all_of(carried->Agents, [](const core::agent_entry& aEntry) { return aEntry.McpServers == core::agent_mcp_servers::off && aEntry.Instructions == core::agent_instruction_delivery::system_prompt; }),
			   "agents carried over have MCP servers off and the system prompt");
		listed.Agents[1].McpServers   = core::agent_mcp_servers::own;
		listed.Agents[0].Instructions = core::agent_instruction_delivery::message;
		expect(store->store(listed), "the settings are stored");
		store = adapters::sqlite::make_agent_setup_store(scratch.path());
		expect(store->load() == listed && store->load()->Agents[0].McpServers == core::agent_mcp_servers::off && store->load()->Agents[1].McpServers == core::agent_mcp_servers::own
				   && store->load()->Agents[0].Instructions == core::agent_instruction_delivery::message,
			   "and read back per agent after a restart");
	}
	// AC09. The choice made in one pull request is that pull request's and
	// that agent's alone; the build answer is the pull request's.
	{
		auto       grants    = adapters::sqlite::make_agent_conversation_store(scratch.path());
		const auto mine      = application::agent_conversation_key{ .Host = "github.com", .Owner = "MikeMarcin", .Name = "BPR-Test", .Number = 40U, .Agent = 1U };
		auto       elsewhere = mine;
		elsewhere.Number     = 41U;
		auto otherAgent      = mine;
		otherAgent.Agent     = 2U;
		expect(!grants->load_scratch_choice(mine) && !grants->load_build_answer(mine), "nothing chosen yet");
		expect(grants->store_scratch_choice(mine, false) && grants->store_build_answer(mine, false), "a choice and an answer are stored");
		grants = adapters::sqlite::make_agent_conversation_store(scratch.path());
		expect(grants->load_scratch_choice(mine) == std::optional{ false } && !grants->load_scratch_choice(elsewhere) && !grants->load_scratch_choice(otherAgent), "after a restart, only that pull request and agent");
		expect(grants->load_build_answer(otherAgent) == std::optional{ false } && !grants->load_build_answer(elsewhere), "the build answer holds for the pull request");
		expect(grants->store_scratch_choice(mine, true) && grants->load_scratch_choice(mine) == std::optional{ true }, "turned back on");
		// Re-review N3, N4: the folder an agent kept in a pull request is
		// known after a restart, for that pull request and agent only.
		expect(!grants->load_scratch_folder(mine), "no folder kept yet");
		expect(grants->store_scratch_folder(mine, "native-0123abcd") && !grants->store_scratch_folder(mine, "../x") && !grants->store_scratch_folder(mine, "-x"), "a folder name is stored; a path is refused");
		grants = adapters::sqlite::make_agent_conversation_store(scratch.path());
		expect(grants->load_scratch_folder(mine) == std::optional<std::string>{ "native-0123abcd" } && !grants->load_scratch_folder(elsewhere) && !grants->load_scratch_folder(otherAgent),
			   "after a restart, only that pull request and agent");
		expect(grants->forget_agents() && !grants->load_scratch_choice(mine) && !grants->load_scratch_folder(mine), "a reset's new agents inherit no choice and no folder");
	}

#ifndef _WIN32
	struct stat status{ };
	expect(::stat(path.c_str(), &status) == 0 && (status.st_mode & 0077) == 0, "the file is owner-private");
#endif

	// Native identity is independent of application/window and invocation catalogs.
	auto natives        = adapters::sqlite::make_native_agent_session_store(scratch.path());
	auto binding        = core::agent_revision_binding{ };
	binding.Host        = core::git_host_id{ 1 };
	binding.Repository  = core::repository_id{ 2 };
	binding.PullRequest = core::pull_request_id{ 3 };
	binding.BaseOid     = *core::parse_git_commit_oid(std::string(40U, '1'));
	binding.HeadOid     = *core::parse_git_commit_oid(std::string(40U, '2'));
	binding.LeftEpoch   = core::epoch_ordinal{ 1 };
	binding.RightEpoch  = core::epoch_ordinal{ 2 };
	auto native         = natives->select("authority:agent-a:agent-tab", "claude", binding);
	expect(native.has_value(), "native conversation is admitted durably");
	native->Handle                 = "exact-native-id";
	native->Pending                = true;
	auto workspace                 = application::owned_agent_workspace{ };
	workspace.Workspace.Path       = "/synthetic-retained-workspace";
	workspace.Workspace.Shallow    = true;
	workspace.SparseMissingObjects = "?" + std::string(40U, 'a') + "\n";
	native->Workspace              = workspace;
	expect(natives->store(*native), "pending send is stored before submission");
	auto competing = *native;
	natives        = adapters::sqlite::make_native_agent_session_store(scratch.path());
	auto restored  = natives->select("authority:agent-a:agent-tab", "claude", binding);
	expect(restored && restored->Id == native->Id && restored->Handle == native->Handle && restored->Pending,
		   "restart restores exact native identity and uncertain submission, without inference");
	expect(restored->Workspace && restored->Workspace->SparseMissingObjects == native->Workspace->SparseMissingObjects, "private sparse object evidence survives restart with its exact conversation");
	restored->Pending = false;
	++restored->Turn;
	expect(natives->store(*restored) && !natives->store(competing), "revision CAS permits one writer and rejects stale activation");
	expect(!natives->select("authority:agent-a:agent-tab", "changed-runtime", binding), "runtime identity mismatch never relabels a saved native session");
	auto otherNative = natives->select("authority:agent-b:agent-tab", "claude", binding);
	expect(otherNative && otherNative->Id != restored->Id, "another agent incarnation gets a distinct native conversation");
	// Shared source ownership is independent of native conversation/configuration.
	restored->WorkspaceKey = "exact-authority:base:head";
	expect(natives->store(*restored), "existing legacy owner publishes its retained tree atomically");
	const auto shared = natives->shared_workspace(restored->WorkspaceKey, *otherNative);
	expect(shared && *shared && (**shared).Workspace.Path == restored->Workspace->Workspace.Path
			   && otherNative->Handle.empty() && otherNative->Id != restored->Id,
		   "other agent reads shared files without acquiring native handle or conversation");
	otherNative->Workspace    = **shared;
	otherNative->WorkspaceKey = restored->WorkspaceKey;
	expect(natives->store(*otherNative), "second native scope retains the exact same source ownership");
	auto alternate                      = *otherNative;
	alternate.Workspace->Workspace.Path = "/different-legacy-workspace";
	expect(!natives->shared_workspace(restored->WorkspaceKey, alternate)
			   && !natives->store(alternate),
		   "a conflicting legacy tree cannot replace shared source ownership");
	auto changedSource            = *otherNative;
	changedSource.Binding.HeadOid = *core::parse_git_commit_oid(std::string(40U, '3'));
	expect(!natives->shared_workspace(restored->WorkspaceKey, changedSource), "a corrupted cross-revision reference refuses rather than retargeting files");
	const auto unrelated = natives->shared_workspace("other-authority:base:head", *native);
	expect(unrelated && *unrelated && (**unrelated).Workspace.Path == native->Workspace->Workspace.Path,
		   "another PR authority has independent legacy adoption, never another PR's shared reference");
	auto lease = natives->try_lock_workspace(restored->WorkspaceKey);
	expect(lease && !natives->try_lock_workspace(restored->WorkspaceKey) && natives->try_lock_workspace("other-authority:base:head"),
		   "one shared-tree turn at a time while unrelated PR work stays eligible");
	lease.reset();
	expect(natives->try_lock_workspace(restored->WorkspaceKey) != nullptr, "turn ownership releases explicitly without an age/PID override");

	const auto missing = natives->find("unopened-scope", "claude", binding);
	expect(missing && !*missing, "viewing absent native history admits nothing");
	auto changedBinding    = binding;
	changedBinding.HeadOid = *core::parse_git_commit_oid(std::string(40U, '3'));
	const auto stale       = natives->select("authority:agent-a:agent-tab", "claude", changedBinding);
	expect(stale && stale->Id == restored->Id && stale->Binding == binding, "head movement never silently retargets native ownership");
	const auto fresh = natives->select("authority:agent-a:agent-tab", "claude", changedBinding, true);
	expect(fresh && fresh->Id != restored->Id && fresh->Binding == changedBinding && fresh->Handle.empty()
			   && natives->store(*restored),
		   "explicit new revision creates a distinct generation while old native provenance stays stored");

	const auto scope = std::string{ "authority:agent-a:agent-tab" };
	expect(natives->append(restored->Id, { "Question", "Saved native answer", "Answered" }), "native transcript saved by original generation");
	const auto opened = natives->choose(scope, "claude", binding, restored->Id);
	expect(opened && opened->Id == restored->Id, "history opens exact earlier generation");
	const auto newSession = natives->choose(scope, "claude", binding, "");
	expect(newSession && newSession->Id != restored->Id && newSession->Handle.empty(), "same-head New session retains native origin");
	const auto unusedAgain = natives->choose(scope, "claude", binding, "");
	expect(unusedAgain && unusedAgain->Id == newSession->Id, "repeated New session reuses unused empty generation");
	expect(natives->append(restored->Id, { "Late turn", "Late original answer", "Answered" }), "late output records only its originating history");
	const auto list = natives->history(scope, "claude");
	expect(list && list->Selected == newSession->Id && list->Turns.empty() && list->Entries.size() == 3U, "new generation empty while all scoped history retained");
	expect(!natives->choose("authority:agent-b:agent-tab", "claude", binding, restored->Id), "history selection cannot cross agent scope");
	expect(natives->choose(scope, "claude", binding, restored->Id).has_value(), "earlier history reopens without inference");
	natives                   = adapters::sqlite::make_native_agent_session_store(scratch.path());
	const auto durableHistory = natives->history(scope, "claude");
	const auto reopenedShared = natives->shared_workspace("exact-authority:base:head", *newSession);
	expect(reopenedShared && *reopenedShared && (**reopenedShared).Workspace.Path == restored->Workspace->Workspace.Path,
		   "restart and New session retain shared files even while the original owner is unselected");

	expect(durableHistory && durableHistory->Selected == restored->Id && durableHistory->Turns.size() == 2U && durableHistory->Turns.back().Answered == "Late original answer", "restart preserves selected generation and originating late transcript");

	auto replayed = natives->replay(scope, "claude", changedBinding, restored->Id);
	expect(replayed && replayed->Replay && replayed->Id != restored->Id && replayed->ReplaySource == restored->Id && replayed->Handle.empty() && !replayed->Workspace && replayed->Binding == changedBinding, "explicit replay has separate generation, shown revision and no native tool/filesystem ownership");
	natives            = adapters::sqlite::make_native_agent_session_store(scratch.path());
	auto replayHistory = natives->history(scope, "claude");
	expect(replayHistory && replayHistory->Replay && replayHistory->Turns.size() == 2U && replayHistory->ReplaySource == restored->Id, "restart preserves replay mode, source link and retained prose");
	expect(!natives->replay("authority:agent-b:agent-tab", "claude", binding, restored->Id), "replay source cannot cross agent authority");
	expect(talks->store(pull, thread), "seed explicit legacy source after earlier clear");
	const auto legacyHistory = natives->history(scope, "claude", "", pull);
	expect(legacyHistory && !legacyHistory->Legacy.empty() && legacyHistory->Selected == replayed->Id, "legacy slots remain unassigned labelled sources alongside exact selected generation");
	const auto legacySource = legacyHistory->Legacy.front().Id;
	auto       legacyReplay = natives->replay(scope, "claude", binding, legacySource, pull);
	expect(legacyReplay && legacyReplay->Replay && legacyReplay->ReplaySource == legacySource && legacyReplay->Handle.empty() && !legacyReplay->Workspace, "legacy prose moves only to explicitly chosen destination without fake native handle");
	expect(!natives->replay(scope, "claude", binding, legacySource), "legacy replay requires explicit accepted PR source identity");
	const auto fromReplay = natives->choose(scope, "claude", binding, "");
	expect(fromReplay && !fromReplay->Replay && fromReplay->Id != legacyReplay->Id, "New session from replay starts empty native generation and preserves source");
	expect(natives->choose(scope, "claude", binding, restored->Id).has_value(), "restore original native provenance after replay");

	const auto removedScope = std::string{ "authority:agent-a:tour:71:0:3:9" };
	auto       removed      = natives->select(removedScope, "readonly", binding);
	expect(removed && natives->append(removed->Id, { "Removed stop question", "Retained answer", "Answered" }), "removed stop keeps a durable readable history");
	const auto resetScope      = std::string{ "authority:agent-a:tour:71:1:0:0" };
	auto       resetGeneration = natives->select(resetScope, "readonly", binding);
	const auto detached        = natives->history(resetScope, "readonly", removed->Id);
	expect(resetGeneration && detached && detached->ReadOnly && detached->Turns.size() == 1U && detached->Turns.front().Answered == "Retained answer", "detached history can be read across reset namespace without adopting it");
	const auto stillReset = natives->find(resetScope, "readonly", binding);
	expect(stillReset && *stillReset && (**stillReset).Id == resetGeneration->Id, "detached inspection preserves reset draft's current association");
	expect(!natives->history("authority:agent-b:tour:71:1:0:0", "readonly", removed->Id), "retained chat viewer cannot cross agent authority");
	auto refusedAt = std::size_t{ 0U };
	for (auto index = 0U; index <= application::maximum_conversations; ++index) {
		const auto admitted = natives->select("capacity-" + std::to_string(index), "claude", binding);
		if (!admitted) {
			expect(admitted.error() == application::native_agent_storage_failure::capacity, "full native catalog reports capacity");
			refusedAt = index;
			break;
		}
	}
	expect(refusedAt > 0U && refusedAt < application::maximum_conversations, "combined native/legacy catalog refuses at its shared bound");
	auto legacyNew   = pull;
	legacyNew.Number = 7000U;
	expect(!talks->store(legacyNew, thread), "legacy writer shares full native plus legacy capacity and cannot prune protected ownership");
	expect(talks->store(workCodex, thread) && talks->load(workCodex) == std::optional{ thread }, "existing legacy projection may be updated at full capacity without losing another history");
	expect(natives->history(scope, "claude")->Legacy.empty(), "legacy identity sources appear only for explicit accepted PR key");
	expect(!natives->replay(scope, "claude", binding, restored->Id), "full replay admission refuses without changing native source");
	const auto protectedHistory = natives->find("authority:agent-b:agent-tab", "claude", binding);
	expect(protectedHistory && *protectedHistory && (**protectedHistory).Id == otherNative->Id, "capacity refusal never prunes protected native history");

	expect(!natives->choose(scope, "claude", binding, ""), "full admission refuses New session");
	const auto keptAfterRefusal = natives->history(scope, "claude");
	expect(keptAfterRefusal && keptAfterRefusal->Selected == restored->Id && keptAfterRefusal->Turns.size() == 2U, "failed New session retains prior selected generation and transcript");

	// RFC-BPR-0125 milestone 3 ([[BPR-2146]]) and RFC-BPR-0128 ([[BPR-2163]]).
	// Suggested prompts: nothing stored is the shipped set at revision 0; a
	// write names its revision, Quick ask and scopes are kept, an older
	// revision is refused, a newer build's row is kept and never rewritten.
	{
		auto       macros = adapters::sqlite::make_prompt_macro_store(scratch.path());
		const auto empty  = macros->load();
		expect(empty && empty->Revision == 0U && empty->Differences == core::prompt_macro_differences{ } && empty->Newer.empty(), "no prompts stored: the shipped set, revision 0");
		auto body           = core::prompt_macro_body{ .Name = "What could this change break?", .Template = "What could {pr.title} break?", .Subject = core::prompt_subject::pull_request };
		body.QuickAsk       = false;
		auto own            = core::prompt_macro{ .Id = "own.security", .Place = core::prompt_place::agent_tab, .Body = { .Name = "Security pass on {file.name}", .Template = "Check {file}", .Subject = core::prompt_subject::open_file } };
		own.Body.Conditions = { { .Fact = core::prompt_fact::path, .Patterns = { "src/**", "*.cpp" } }, { .Fact = core::prompt_fact::draft, .Negated = true } };
		own.Body.QuickAsk   = true;
		// [[BPR-2179]]. Any of these, and a condition with what it counts.
		own.Body.Match = core::prompt_match::any;
		// Look prompts-v003: the prompt's own Hide it or Show it disabled.
		own.Body.WhenUnmet = core::prompt_when_unmet::disable;
		own.Body.Conditions.push_back({ .Fact = core::prompt_fact::thread_activity, .Activity = core::prompt_activity_value::replies });
		// Look prompts-v001: nested groups, None among them.
		own.Body.Groups        = { { .Match = core::prompt_match::none, .Conditions = { { .Fact = core::prompt_fact::draft } }, .Groups = { { .Match = core::prompt_match::all, .Conditions = { { .Fact = core::prompt_fact::checks_failing }, { .Fact = core::prompt_fact::thread_unresolved } } } } } };
		own.Body.Scopes        = { .File = true, .PullRequest = true };
		own.On                 = false;
		const auto differences = core::prompt_macro_differences{
			.Changes = { { .Id = "agent-tab.what-could-break", .Body = body, .WrittenAgainst = 1U }, { .Id = "agent-tab.review-pull-request", .On = false } },
			.Own     = { own },
			.Orders  = { { .Place = core::prompt_place::agent_tab, .Ids = { "own.security", "agent-tab.review-pull-request" } } },
			// [[BPR-2179]]. Surfaces as changed: one emptied, one reordered.
			.Surfaces = { { .Surface = core::prompt_surface::pull_request_chips, .Ids = { }, .Removed = { "agent-tab.review-pull-request", "agent-tab.what-could-break" } },
						  { .Surface = core::prompt_surface::file_menu, .Ids = { "own.security", "agent-tab.explain-file" }, .Removed = { } } },
			// [[BPR-2179]]. A repository's layer: an override, one of its own,
			// a surface with an addition, a removal and its own order.
			.Repositories = { { .Repository = "octo/render",
								.Overrides  = { { .Id = "agent-tab.what-could-break", .Template = "What could break here?", .Conditions = std::vector<core::prompt_condition>{ }, .Groups = std::vector<core::prompt_condition_group>{ { .Match = core::prompt_match::any, .Conditions = { { .Fact = core::prompt_fact::draft } } } }, .WhenUnmet = core::prompt_when_unmet::hide, .On = false } },
								.Own        = { { .Id = "own.only-here", .Body = { .Name = "Only here", .Template = "Ask here" } } },
								.Surfaces   = { { .Surface = core::prompt_surface::thread_menu, .Added = { "own.only-here" }, .Removed = { "conversation.summarize" }, .Order = std::vector<std::string>{ "own.only-here" } } } } },
		};
		expect(macros->store(differences, 0U) == application::prompt_macro_write::stored, "a write from the revision read is stored");
		macros.reset();
		macros          = adapters::sqlite::make_prompt_macro_store(scratch.path());
		const auto read = macros->load();
		expect(read && read->Revision == 1U && read->Differences == differences, "a restart reads the same prompts back, Quick ask, scopes and surfaces included");
		expect(macros->store({ }, 0U) == application::prompt_macro_write::stale && macros->load() == read, "a write from an older revision changes nothing");
		auto withBadPattern                        = differences;
		withBadPattern.Own.front().Body.Conditions = { { .Fact = core::prompt_fact::path, .Patterns = { "a\nb" } } };
		expect(macros->store(withBadPattern, 1U) == application::prompt_macro_write::failed && macros->load() == read, "a pattern with a control character is not written");
		{
			sqlite3* database = nullptr;
			expect(sqlite3_open_v2(path.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
			expect(sqlite3_exec(database,
								"INSERT INTO prompt_macros(kind,id,position,place,has_body,name,template,subject,ask,conditions,quick,scopes,on_state,written_against) "
								"VALUES(1,'own.future',5,'future-place',1,'Future','Future','pull-request','question','',0,0,1,0)",
								nullptr, nullptr, nullptr)
					   == SQLITE_OK,
				   "a newer build's row is written");
			sqlite3_close_v2(database);
		}
		// A row an earlier build wrote, without the choice: migrated from its
		// conditions' lean (a path condition hides).
		{
			sqlite3* database = nullptr;
			expect(sqlite3_open_v2(path.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
			expect(sqlite3_exec(database,
								"INSERT INTO prompt_macros(kind,id,position,place,has_body,name,template,subject,ask,conditions,quick,scopes,on_state,written_against) "
								"VALUES(1,'own.earlier',6,'agent-tab',1,'Earlier','Earlier','pull-request','question','path' || char(31) || 'is' || char(31) || 'any' || char(31) || 'src/**',0,0,1,0)",
								nullptr, nullptr, nullptr)
					   == SQLITE_OK,
				   "an earlier build's row is written");
			const auto earlier  = macros->load();
			auto       migrated = false;
			if (earlier) {
				for (const auto& macro : earlier->Differences.Own) {
					migrated = migrated || (macro.Id == "own.earlier" && macro.Body.WhenUnmet == core::prompt_when_unmet::hide);
				}
			}
			expect(migrated, "an earlier row's path condition migrates to Hide it");
			expect(sqlite3_exec(database, "DELETE FROM prompt_macros WHERE id='own.earlier'", nullptr, nullptr, nullptr) == SQLITE_OK, "and is taken out again");
			sqlite3_close_v2(database);
		}
		const auto withNewer = macros->load();
		expect(withNewer && withNewer->Newer == std::vector<std::string>{ "own.future" } && withNewer->Differences == differences, "a newer build's row is kept apart, never offered");
		expect(macros->store({ }, 1U) == application::prompt_macro_write::stored, "restoring everything is a write");
		const auto restoredSet = macros->load();
		expect(restoredSet && restoredSet->Revision == 2U && restoredSet->Differences == core::prompt_macro_differences{ } && restoredSet->Newer == std::vector<std::string>{ "own.future" },
			   "a write never rewrites a newer build's row");
	}

	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(path.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
		expect(sqlite3_exec(database, "UPDATE agent_setup SET chosen=7", nullptr, nullptr, nullptr) != SQLITE_OK, "an out-of-range agent is refused by the schema");
		expect(sqlite3_exec(database, "PRAGMA application_id=1112560199", nullptr, nullptr, nullptr) == SQLITE_OK, "another store's identity is written");
		sqlite3_close_v2(database);
	}
	expect(!store->load() && !store->store(chosen), "a file with another store's identity is refused, not read or overwritten");
	std::cout << "agent-setup-storage:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return EXIT_FAILURE;
}
