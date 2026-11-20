---
schema: "implement-rfc.card.v1"
id: "BPR-2307"
title: "A custom Codex agent may rely on its launcher's sandbox"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"feature","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2307--BPR-2307--d43a5ca08639.json","origin":"owner decision, 2026-11-19: \"Sure\", to BPR-2305's open question \"May a custom agent turn off Codex's own sandbox and rely on the launcher's?\"","related_cards":["BPR-1918","BPR-2007","BPR-2008","BPR-2305"],"related_rfcs":["RFC-BPR-0011","RFC-BPR-0092","RFC-BPR-0104"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"]}
acceptance: []
---

# BPR-2307: A custom Codex agent may rely on its launcher's sandbox

## Owner (2026-11-19)

[[BPR-2305]] left one question open: "May a custom agent turn off Codex's
own sandbox and rely on the launcher's?" The owner answered: "Sure".

Context: through a corporate sandbox launcher, a granted Codex write failed.
Codex's own sandbox nested inside the launcher's is the likely cause. The
[[BPR-1918]] opt-out for custom agents is the precedent, and this card
follows its shape: a per-agent setting, words that say what it means, off
by default, never on a popular agent, and enforced in core and in the store.

## Change

- **Setting.** `core::agent_entry::RelyOnLauncherSandbox`, off by default.
  Core refuses it for a popular agent and for any agent that does not speak
  a Codex protocol (`launcher_sandbox_not_custom_codex`). The Add/Edit agent
  dialog shows **Rely on the launcher's sandbox** for a custom agent only,
  and only while its protocol is Codex (JSONL or App Server). The text under
  it reads: "For a launcher that runs Codex in its own sandbox, where Codex's
  sandbox makes every write fail. Codex's sandbox is then off whenever the
  agent may write, so the scratch folder and build limits are enforced only
  by the launcher. Read-only runs stay read-only." The
  `frontend-design:frontend-design` skill was loaded for the control and
  its wording.
- **The core decides the arguments.** `core::decide_codex_sandbox` returns
  one of four modes. `configured`: no grant and not read-only, so no sandbox
  is passed, as before. `read_only`. `grant_profile`: BetterPullReview's
  `betterpullreview_grant` profile. `launcher`: a granted run of an agent
  that relies on its launcher. For `launcher`,
  `core::codex_launcher_sandbox_arguments` gives `--sandbox
  danger-full-access -c approval_policy="never"` to `exec` and
  `-c sandbox_mode="danger-full-access" -c approval_policy="never"` to the App
  Server, which has no `--sandbox` option. `build_agent_launch` passes them in
  place of the grant profile. Then `core::inspect_codex_sandbox_arguments`
  checks BetterPullReview's own Codex arguments against the mode, matching
  whole arguments only. A launch whose arguments disagree with the mode is
  refused as `unsafe_arguments` and never started. The checks: sandbox off
  without the mode, the mode mixed with the profile or another sandbox, a
  read-only `exec` without `--sandbox read-only`, and a granted run without
  `approval_policy="never"`.
- **Read-only runs keep `--sandbox read-only`.** In the [[BPR-2305]] audit,
  the read-only options stopped every write inside the launcher. Only the
  write grant failed there. Nothing in that evidence says the nested
  read-only sandbox breaks, so read-only stays read-only, whether the agent
  relies on its launcher or not. An App Server read-only run is still told
  `readOnly` in its requests.
- **Unchanged.** `approval_policy="never"`, `mcp_servers={}` where MCP is
  off, the untrusted-project override for a pull request's checkout, every
  grant refusal ([[BPR-2007]] review finding 1: the reviewer's own Codex
  permissions profile or approvals, the agent's own sandbox arguments, the
  reviewer's own checkout), the agent's scratch and build instructions, and
  [[BPR-2305]]'s failed-writes message.
- **Who holds the grant, said where it is granted.** `agent_grant_facts` and
  `agent_grant` carry `LauncherSandbox` (Codex only). The Agent tab's scratch
  and builds tooltips then say that Codex's own sandbox is off and only the
  launcher's holds the writes, instead of "Codex's sandbox holds…". The
  window passes it to the invocation (`agent_invocation_request` to
  `agent_launch_options::RelyOnLauncherSandbox`). For any protocol other
  than Codex, the launch refuses it.
- **Stored and carried.** SQLite table `agent_entry_launcher_sandbox`, rows
  by entry id. A row is read back only for a custom Codex entry, so a stray
  row never refuses the whole list. A `.bprsettings` Agents item writes
  `"launcher_sandbox": true` only when the setting is on, so files written
  without it read the same. An import applies it only when the reviewer
  ticks "Let NAME rely on its launcher's sandbox"
  (`agent_import_choice::AcceptLauncherSandbox`), as with relaxed checks.
  It is its own `agent_field` (`launcher_sandbox`) in the preview's
  comparison.
- **Identity.** Unlike strict run checks, changing it keeps the agent's
  identity and conversations. It changes who enforces a granted turn's
  sandbox, not what the agent runs, and a continued Codex session runs under
  the new sandbox from its next turn.
- **Local drafts.** A local draft's runs, including a check run with grants
  (RFC-BPR-0120), take only a Claude stream-JSON agent, so they never carry
  this setting. If they ever take Codex, the safe default applies: the grant
  profile.
- **Recorded.** BPR-2305's open question is marked answered.
  `docs/architecture/ai-agent-boundary.md` (Host policy, "Codex through a
  sandbox launcher") and a threat-model entry, "Custom Codex agent relies on
  its launcher's sandbox (BPR-2307)", record the decision and its residual.
  So do owner amendments of 2026-11-19 in RFC-BPR-0092 and RFC-BPR-0011.

## Also landed: BPR-2305 regression

The workspace contract `review_cycle_send` failed on main ("a corporate
account: Codex listed, refused"). It still expected the refusal that the
owner's 2026-11-19 GHES decision removed. It now expects Codex offered on a
corporate host, with nothing frozen, sent or recorded. The host refusal
stays proved in `core_review_cycle_send` against a narrowed policy. No
source path still refused Codex on GHES. Noted on [[BPR-2305]].

## Acceptance

- AC1. A custom Codex agent with the setting on: a granted run passes
  Codex's sandbox off (`exec` and App Server) and no grant profile. Approvals
  stay never, MCP stays off and the checkout stays untrusted.
- AC2. Off (the default), granted runs pass the grant profile as before. A
  read-only run keeps `--sandbox read-only` either way. A run with no grant
  turns nothing off.
- AC3. Core decides and checks the arguments. A disagreeing launch is not
  started. The setting is refused for the popular Codex and for non-Codex
  agents, in core, the store and the dialog.
- AC4. The setting is stored per agent, read back after a restart, and
  carried in a settings file's Agents section. An import applies it only
  when accepted.
- AC5. The setting's words, and the grant tooltips while it is on, say that
  the write grant is then enforced only by the launcher.
- AC6. The decision is recorded in BPR-2305, the AI agent boundary, the
  threat model and the RFCs. The documentation gate passes.

## Evidence

Candidate on main `f4c3c0e54`, dev preset (Debug, macOS), only the targets
these tests need. Fixture code only; no real Codex, Claude or launcher ran.

| Criterion | Check | Result |
|---|---|---|
| AC1, AC2 | `application_agent_launch` › `test_codex_relies_on_the_launchers_sandbox`. `exec` gets `--sandbox danger-full-access -c approval_policy="never"` and no `default_permissions`/`permissions.`. The App Server's arguments are exactly `app-server`, untrusted, MCP off, `sandbox_mode="danger-full-access"`, approvals never. Off gives the profile. Read-only `exec` keeps `--sandbox read-only`, and the App Server gets no sandbox-off argument. No grant turns nothing off. Claude with it is refused. The existing grant cases are unchanged (`test_the_grant_crosses_exactly`) | Passed |
| AC3 | `core_agent_grant` › `test_launcher_sandbox` (the decision table, the arguments, each inspection problem, whole-argument matching against a path and instructions holding the words, entry refusals for Claude and the popular Codex), `application_agent_launch_policy` | Passed |
| AC4 | `sqlite_agent_setup_storage` (on, read back after a restart, only for that agent; a Claude entry with it refused), `core_agent_configuration` › `test_launcher_sandbox_travels` (written only when on, read back, applied only when accepted, its own difference, refused on a Claude item), `sqlite_agent_configuration_storage` | Passed |
| AC5 | `qt_agent_setup_view` (shown off for a custom Codex agent, with the words; kept when ticked; hidden and never kept for Claude; absent for the popular Codex), `core_agent_grant` (scratch and builds tooltips name only the launcher), `qt_agent_configuration_dialogs` | Passed |
| AC6 | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |
| BPR-2305 note | `review_cycle_send` (workspace contract), `core_review_cycle_send` | Passed |
| Standard | clang-format 22 on every changed C++ file. `tidy.py --changed`: no finding in new lines; the six it lists predate this card | Passed |
| App | `window_agent_setup.cpp` and `agent_invocation_host.cpp` compiled `-fsyntax-only` from the dev compile database (the full app was not linked: about 3 GB of disk free) | Passed |

Not run here: the full app link and its public paths. The
`application_composition_source_contract` already fails on main before this
card ("full LF-normalized github_application.cpp digest drifted", from
earlier `src/app` changes). This card does not touch `github_application.cpp`
and does not reseal those digests. A reseal pass should cover the
`window_agent_setup.cpp` change too.

## Owner steps (need the corporate Mac and its launcher)

These were not run here because no real launcher runs from an agent.

1. On the launcher's pipe-mode release, turn on **Rely on the launcher's
   sandbox** for the custom Codex (JSONL) agent. Then ask one write-granted
   turn: it writes inside the scratch folder, and the launcher's sandbox
   still refuses a write outside its workspace.
2. If a write still fails with the setting on, the cause is not the nested
   sandbox. Report it on [[BPR-2305]].
