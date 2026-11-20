---
schema: "implement-rfc.card.v1"
id: "BPR-2305"
title: "Codex cannot review a GHES pull request at all, and through a corporate sandbox launcher BPR's Codex JSONL launch would fail at line 2, drop its instructions and fail every granted write"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/BPR-2305--BPR-2305--095fcc48869d.json","acceptance":["AC1","AC2","AC3","AC4","AC5"],"found_by":"owner, 2026-11-19: Codex should be allowed on explicit GHES hosts, as a popular agent or a custom agent. An agent harness audit the same day (codex-cli 0.167.2 on macOS, run through a corporate sandbox launcher's Codex mode, the probes copying BPR's Codex exec options) found what the launch needs. The anonymized report travels with this card","related_cards":["BPR-1748","BPR-1753","BPR-2004","BPR-2142"],"related_rfcs":["RFC-BPR-0011","RFC-BPR-0092"]}
acceptance: []
---

# BPR-2305: Codex cannot review a GHES pull request at all, and through a corporate sandbox launcher BPR's Codex JSONL launch would fail at line 2, drop its instructions and fail every granted write

The owner decided on 2026-11-19 that Codex should be allowed on an explicit
GHES host. Today the host policy refuses it there, both as a popular agent
and as a custom agent. Lifting that refusal is Part A.

Lifting it is not enough on its own. On a corporate Mac, Codex reaches a GHES
pull request through a sandbox launcher whose `--codex` mode runs the real
Codex CLI. An audit of that route found three things that break BPR's Codex
JSONL launch. Those are Part B.

## Observed

### A. GHES refuses Codex, popular or custom

- `built_in_agent_policy` (`src/core/src/agent_profile.cpp`, lines 184–209)
  allows only `claude_stream_json_profile_id` and
  `external_terminal_profile_id` on an `explicit_ghes` host.
- A custom agent is allowed only where its protocol's built-in profile is
  ([[BPR-1753]], owner amendment 2026-10-23, same function). So a custom
  agent set up as "Codex (JSONL)" or "Codex (App Server)" is refused on GHES
  too, whatever its program or arguments.
- `docs/architecture/ai-agent-boundary.md` (line 33) says so: "an explicit
  GHES host refuses Codex before workspace or launcher effects and offers
  only the approved corporate alternatives".

### B. Codex through a sandbox launcher: what the audit found

The audit used the launcher's 0.7 prerelease, which has a pipe mode: piped
stdin is passed to the agent and stdout is relayed byte for byte. The 0.6
release still runs the agent under a pseudo-terminal, so neither BPR protocol
can read it. The custom agent the audit modelled is:

- **Program:** the launcher
- **Arguments:** `--no-extra-output`, `--codex`
- **Protocol:** Codex (JSONL)

BPR then launches
`<launcher> --no-extra-output --codex exec --json … -`, with the prompt on
stdin.

What works:

- **The plain call, in both forms.** The prompt on stdin after `-`, or as an
  argument with stdin closed: clean JSONL, no CRLF, no ANSI, logs on stderr.
- **Stopping.** SIGTERM or SIGKILL to the launcher stops the agent too, with
  no process left behind.
- **The untrusted-project override keeps out the checkout's `AGENTS.md`.**
  That is `-c projects={"<checkout>"={trust_level="untrusted"}}`. Without
  it, the probe's `AGENTS.md` canary was followed; with it, it wasn't.
- **The read-only options stop every write.** Those are
  `--sandbox read-only -c approval_policy="never" -c mcp_servers={} -c tools.web_search=false`.
- **`-c mcp_servers={}` is accepted.**

What breaks BPR's launch:

1. **Config warnings come before `turn.started`.** Every stream ran:
   - `thread.started`;
   - two `item.completed` items of `type: "error"`, which are config warnings
     ("Codex is ignoring 2 unrecognized configuration settings …
     `disable_response_storage` is ignored", from a machine-wide managed
     config and the user's config);
   - then `turn.started`.

   BPR's JSONL decoder (`src/application/src/agent_launch.cpp`, lines
   1270–1340) accepts `item.*` only after `turn.started` (phase 2). Anything
   else is `malformed_frame`. So BPR would reject the run at line 2 and show
   it as unreadable.

   This comes from reading the code; the decoder was not run on the captured
   stream. The warnings come from Codex's config files, not from the
   launcher, so plain Codex on such a machine likely hits it too.

   The decoder also fails a run on any top-level `error` event during the
   turn (line 1289). The audit saw no such event. Whether Codex sends one for
   a stream retry it then recovers from is unknown.
2. **`developer_instructions` had no effect.** `-c developer_instructions="<rules>"`
   (BPR's system prompt for Codex exec, lines 3741–3745) changed the first
   turn's prompt by −20 tokens, and the canary word in the rules never
   appeared. A code comment records the same option being followed on Codex
   0.167.0 run directly. The launcher probably sets its own and overrides the
   caller's; that is unconfirmed.

   The same rules pasted at the start of the prompt were followed. Because a
   fresh run's prompt starts with them, they were also read from cache on
   later runs: +3,985 cached tokens per call against no rules.
3. **A granted write fails.** BPR's grant (`append_codex_grant`, lines
   3547–3559) gave the `betterpullreview_grant` profile write access to one
   folder. Codex understood it: it said the folder was writable. Then its
   file change failed ("Failed to write file"), and nothing outside the
   folder was written. A Codex sandbox nested inside the launcher's sandbox
   is the likely cause; unconfirmed. So a writing run through the launcher
   cannot write.

Also measured:

- The caller's environment did not reach the agent's shell. BPR's launch
  passes only an allowlist anyway.
- The project-config probe proved nothing: its control never loaded a
  project `.codex/config.toml`, even with the folder marked trusted.

## Change

### 1. Allow Codex on an explicit GHES host

- `built_in_agent_policy` allows `codex_app_server_profile_id` and
  `codex_jsonl_profile_id` on `explicit_ghes`, so custom Codex agents follow
  through the existing [[BPR-1753]] rule.
- Update `docs/architecture/ai-agent-boundary.md` and the RFC-BPR-0011 /
  RFC-BPR-0092 text that states the refusal, and record the owner's decision
  with its date.
- Setup on a GHES host offers Codex like any other agent, and the checks
  before workspace or launcher effects no longer refuse it.

### 2. Read config warnings before the turn

- The JSONL decoder accepts `item.completed` items of `type: "error"`
  between `thread.started` and `turn.started`, and anywhere in the turn.
  They become diagnostics (logged, and shown as a warning the reviewer can
  open), not failures.
- A top-level `error` event fails the run only if the turn then fails or
  ends without `turn.completed`. If Codex recovers, the run goes on. Confirm
  with a captured stream before choosing; if no recovered stream can be
  produced, keep today's rule and record why.
- Check the App Server decoder for the same ordering.

### 3. Instructions that reach the model through a launcher

- A Codex custom agent gets an **Instructions** setting:
  - "As developer instructions" (`-c developer_instructions=…`), the default;
  - "At the start of the prompt".

  The prompt form puts BPR's rules first and the round after them, so they
  stay a cacheable prefix.
- The custom agent's setup check proves the chosen form is followed: one
  short run with a canary rule, its answer checked for the word. If
  developer instructions fail and the prompt form works, setup says so and
  offers to switch.

### 4. Writing runs through a launcher

Find out why the write fails before choosing a fix:

- Probe a write with Codex's own sandbox off inside the launcher
  (`--sandbox danger-full-access`, the launcher's sandbox still enforcing).
  Probe it again with the grant profile and the folder inside the launcher's
  workspace.
- If only Codex's nested sandbox fails, the owner decides whether a custom
  agent may opt out of it and rely on the launcher's. That is a security
  decision, not an agent's; the [[BPR-1918]] opt-out for custom agents is the
  precedent to weigh.
- Whatever the outcome, a writing run whose file changes all fail says so
  with the failed paths, instead of ending as a normal answer.

### 5. Name the launcher version that works

- A custom agent's check arguments can run the launcher's `--version`. A
  launcher before the pipe mode makes both Codex protocols unreadable, so
  setup names that version and the fix (update the launcher), not a decoder
  error.

### 6. Tests

- Core policy: `explicit_ghes` allows both Codex profiles and their custom
  entries; `github_com` is unchanged.
- Decoder: a captured stream with two leading `error` items decodes to the
  answer, with two diagnostics. A stream whose turn fails still fails.
- Launch: the "At the start of the prompt" setting produces the rules then
  the round on stdin, and no `developer_instructions`.
- Setup: a fixture agent that ignores developer instructions fails the
  canary check and offers the prompt form.
- Writing run: a fixture agent whose file changes all fail ends with a
  message naming them.

## Acceptance

- AC1. On an explicit GHES host, Codex (popular or custom, App Server or
  JSONL) can be set up and run on a pull request.
- AC2. A Codex JSONL stream that starts with config-warning `error` items
  decodes, shows the answer and logs the warnings.
- AC3. Through a launcher that drops `developer_instructions`, a Codex
  custom agent set to "At the start of the prompt" follows BPR's rules. Setup
  detects which form works.
- AC4. A writing run through a launcher either writes inside its grant, or
  ends with a message naming the writes that failed. Whether Codex's own
  sandbox may be turned off is recorded as an owner decision.
- AC5. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Out of scope

- Fixing the launcher. Its pipe mode, and whatever overrides
  `developer_instructions`, belong to its maintainers. A ticket with them is
  open.
- `exec resume` and the App Server protocol through the launcher. The audit
  did not cover them; probe them before relying on them.
- Codex's own config warnings. Removing `disable_response_storage` from the
  user's config clears one copy; the managed copy is not BPR's.

## Resolution

The audit report itself stays outside the repository; this card carries
what the work needs from it.

- **Change 1.** `core::built_in_agent_policy` permits both Codex profiles on
  `explicit_ghes`, so a custom Codex (JSONL or App Server) follows through
  the [[BPR-1753]] rule. The owner's 2026-11-19 decision is recorded in
  `docs/architecture/ai-agent-boundary.md` and as owner amendments in
  RFC-BPR-0011 and RFC-BPR-0092. A host policy that leaves a profile out
  still refuses it in core before any effect; the tests that proved the
  refusal now prove it against such a policy. A Review Cycle send to a
  corporate host offers Codex too, since it uses the same policy.
- **Change 2.** The JSONL decoder reads `item.completed` items of type
  `error` between `thread.started` and `turn.started` and in the turn as
  `harness_warning` events: the run's `HarnessWarnings` (at most 8, 512 bytes
  each) fold with the turn's activity in the Agent tab ("2 warnings", each
  row openable), and the diagnostic line counts them (`warnings=N`), never
  their text. **Top-level `error` rule kept:** no stream in which Codex
  recovered from a top-level `error` was captured (the audit saw none), so a
  top-level `error` during the turn still ends the run; revisit with a
  captured recovered stream. The App Server decoder already reads past
  `configWarning`, `warning` and `item/completed` in any order; no change.
- **Change 3.** The Instructions setting already existed ([[BPR-2140]]); a
  Codex agent now names its two places "As developer instructions" (default)
  and "At the start of the prompt". For Codex `exec` the prompt form puts the
  rules on stdin right after the fixed `BPR-CONTEXT/2 CODEX-EXEC` line and
  before the request, one cacheable prefix, with no `developer_instructions`.
  A custom Codex JSONL agent's check (after its own check arguments, when it
  names them) asks one short read-only, ephemeral, MCP-free question with a
  canary rule given its configured way; ignored as developer instructions and
  followed in the prompt, the check fails as `instructions_dropped` and
  Settings › Agents offers "Use the start of the prompt", which switches the
  agent and checks it again. The check wording now says a custom Codex JSONL
  agent is asked one short question. App Server custom agents keep today's
  check (the audit did not cover the App Server through a launcher).
- **Change 4.** A Codex `file_change` item that ended names each path and
  whether it was `completed` or `failed`. A turn whose file changes all
  failed ends in the Agent tab with "Codex changed files, but every change
  failed, so nothing was written: …" naming them after what it said, and the
  diagnostic counts them (`writes_failed=N writes_written=N`). The run stays
  a protocol success so its conversation continues. Whether Codex reports the
  launcher's refused write as a failed `file_change` (rather than only in a
  failed command) is unconfirmed: the audit's failure showed in Codex's own
  log. Nothing turns Codex's sandbox off.
- **Change 5.** The custom check's own arguments can run the launcher's
  `--version`; its first line is kept as the check's version ("Works,
  version …"). When the probe's output is not Codex's JSON lines the check
  fails as `output_unreadable`: "… not as Codex's JSON lines. If its program
  is a launcher that runs Codex in a terminal, update the launcher to a
  version that passes Codex's output through. Its check says it is …", not a
  decoder error.

## Open owner decision

- **May a custom agent turn Codex's own sandbox off and rely on the
  launcher's?** OPEN, a security decision for the owner, not an agent. The
  [[BPR-1918]] opt-out for custom agents' run checks is the precedent to
  weigh. Until it is answered no build passes `--sandbox danger-full-access`
  or any other opt-out.

## Owner steps (need the corporate Mac and its launcher)

These were not run here: the launcher is not on this machine, and no real
Codex, Claude or launcher run spends the owner's usage from an agent.

1. Change 4's probes: a granted write with `--sandbox danger-full-access`
   inside the launcher (its sandbox still enforcing), and again with the
   grant profile and the folder inside the launcher's workspace. They say
   whether Codex's nested sandbox is the cause, and inform the decision above.
2. Confirm on the launcher's pipe-mode release that a custom Codex (JSONL)
   agent with the launcher as program and `--no-extra-output`, `--codex` as
   arguments passes Settings › Agents › Check with "At the start of the
   prompt", fails with `instructions_dropped` set to developer instructions,
   and that one write-granted turn either writes or names the failed paths.
3. If Codex ever recovers after a top-level `error` event, keep that stream:
   it decides whether Change 2's second rule can relax.

## Evidence

Fixture streams and agents only (content invented); no real agent ran.
Candidate `d2e3ac56c` on main `f26254e55`, dev preset (core, application,
SQLite and Qt adapter targets), macOS:

| Criterion | Check | Result |
|---|---|---|
| AC1 | `core_agent_profile` (`explicit_ghes` permits both Codex profiles and every added-agent slot; same list as `github_com`), `core_agent_entry`, `core_agent_setup`, `core_review_cycle_send`, `application_agent_invocation_host` (GHES launches Codex App Server and JSONL; a host without Codex refuses before effects), `application_agent_setup_host` (GHES lists, permits and takes a chosen Codex), `application_agent_chat_reply` | Passed |
| AC1 | `application_agent_setup_host_policy_public_path` (Enterprise host allows Codex and an added Codex launcher) and `review_cycle_send_public_path` (a corporate account's send box offers Codex, unmarked) updated to the decision | Compiled with `-Werror` (`-fsyntax-only` from the dev compile database); not run: both link the full composition, which the machine's free disk (about 1.5 GB) could not build. First full dev run on main should include them |
| AC2 | `application_agent_launch` › `test_codex_jsonl_reads_warnings_before_the_turn` (two leading `error` items decode to the answer with two warnings; in-turn warning cut to its bound; `turn.failed` and a top-level `error` still fail), `application_agent_protocol_loop` › `test_codex_warnings_and_failed_writes_reach_the_result`, `qt_agent_activity_view` (warnings fold and open), `qt_agent_codex_launcher_turn` | Passed |
| AC3 | `application_agent_launch_policy` › `test_codex_rules_lead_the_prompt_at_the_start_of_the_prompt` and `test_the_codex_instructions_probe`; `application_agent_setup_host` › `test_a_custom_codex_check_proves_its_instructions_arrive` (fixture launcher ignores developer instructions: `instructions_dropped`, offer; switched: passes with the launcher's version; unreadable output named with its version); `qt_agent_setup_view` › `test_settings_offers_the_prompt_form` | Passed; live launcher confirmation is owner step 2 |
| AC4 | `application_agent_launch` › `test_codex_jsonl_names_failed_file_changes`, `application_agent_protocol_loop`, `qt_agent_codex_launcher_turn` (all-failed turn names both paths; a turn that wrote one does not); sandbox opt-out recorded above as OPEN | Passed; probes are owner step 1 |
| AC5 | the above, `sqlite_agent_setup_storage` (a kept check's new failures read back), `qt_agent_launch_panel`, `architecture_boundaries`, `architecture_boundary_contract` | Passed |
| AC5 | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |
| Standard | clang-format 22.1.8 on every changed C++ file; `tidy.py --changed --warnings-as-errors`: the one finding in new code fixed, the rest predate this card | Passed for new code |
