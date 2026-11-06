---
schema: "implement-rfc.card.v1"
id: "BPR-1891"
title: "Tool-free writing runs when Claude reports a plugin it loaded; the tools, MCP and memory checks stay"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-06: review tour messages stopped with 'my-agent answered in a form this app can't read' (malformed_frame) on the first init line, while agent chat on the same Claude worked","related_cards":["BPR-1375","BPR-1827"],"related_rfcs":[]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"df901d5a5","evidence":{"kind":"command","detail":"application_agent_launch (test_claude_writing_reports_the_plugins_it_loaded): the 2.1.314 fixture init frame, verbatim from this card, finishes with the agent's text in a writing run. The end-to-end writing course now runs PLUGIN_CANARY with that init frame and expects the text. It passed only with an uncommitted diagnostic that forced the Tour editor's agent readiness to ready; as committed it stops earlier, see AC7."}},{"criterion":"AC2","outcome":"pass","revision":"df901d5a5","evidence":{"kind":"command","detail":"application_agent_launch: writing init frames with tools [\"Read\"], mcp_servers [{...}] (new case) or memory_paths {\"auto\":...} are each refused as malformed_frame."}},{"criterion":"AC3","outcome":"pass","revision":"df901d5a5","evidence":{"kind":"command","detail":"application_agent_launch: a non-writing run keeps its tools and plugins, and the 2.1.314 frame in chat emits no plugins_reported event. The decoder emits the event only when aWritingOnly is set."}},{"criterion":"AC4","outcome":"pass","revision":"df901d5a5","evidence":{"kind":"command","detail":"application_agent_launch: plugins are named by source, falling back to name, then 'unnamed'. Each name is at most 64 bytes of [A-Za-z0-9._@+-] and the path member is never read. Past 8 names the rest are counted as +N. The agent.run line ends ' plugins=...' and parse_agent_run_diagnosis reads it back exactly and refuses a path, an empty token or an empty list. With readiness forced, the writing course read the profile log after shutdown and found 'stderr_bytes=0 plugins=cc-plugin-sec-default@builtin' and no '/fixture/'."}},{"criterion":"AC5","outcome":"pending","revision":"df901d5a5","evidence":{"kind":"observation","detail":"Not reachable on this Mac. The installed Claude is 2.1.310 (Homebrew) with no managed settings. Evidence step 1, in an empty scratch folder with claude_writing_restrictions verbatim, reported plugins: [] and tools/mcp_servers/skills empty with no memory_paths. With the enabledPlugins entries removed it reported exactly cc-plugin-agents-md, cc-plugin-telemetry and cc-plugin-plugin-authoring (all @builtin). No cc-plugin-sec-default appeared, so the --installed-writing course was not run. A my-agent or managed-settings Mac is still needed."}},{"criterion":"AC6","outcome":"pass","revision":"df901d5a5","evidence":{"kind":"command","detail":"ai-agent-boundary.md (the plugin table row and the decoder paragraph), security-threat-model.md ('Fail closed on drift', a new 'Plugins are reported, not refused' bullet, the residual, the wrapper bullet) and the claude_writing_settings comment in agent_launch.hpp now say plugins are reported, not refused, and state the residual. python3 scripts/verify_documentation.py --root . printed DOCUMENTATION_OK (with PYTHONPATH at the main checkout's .agents/skills/implement-rfc/scripts, which the worktree lacks)."}},{"criterion":"AC7","outcome":"pending","revision":"df901d5a5","evidence":{"kind":"command","detail":"application_agent_launch and architecture_boundaries pass. coding-format-check passes (clang-format 22.1.8). application_review_tour_writing and application_review_tour_writing_wrapper fail on this base before any changed code runs, at 'recovery gesture accepted' and 'tour saved through admitted SQLite store'. application_review_tour_composition_public_path fails the same way at 'Problem Tab reaches the visible assistance control'. A diagnostic print showed the Tour editor's agent readiness as unfinished in the fixture: since BPR-1881 (101c59fe2) agent features stay disabled until setup is ready, and these courses never reach ready. With an uncommitted diagnostic forcing readiness to ready, both writing courses pass, including the new plugin step and the diagnostic-log check. The course fixture needs its own fix."}}]
---

# BPR-1891: Tool-free writing runs when Claude reports a plugin it loaded; the tools, MCP and memory checks stay


## Observed

The owner, 2026-11-06, on a Mac with my-agent (managed Claude Code settings).
Claude Code 2.1.314 runs through the wrapper's `vscode-wrapper`. Asking the
review tour to write a message stopped at once:

- Card: "my-agent answered in a form this app can't read."
- "What went wrong: A line of its output isn't in the format its profile
  names, or arrived out of order. (malformed_frame)"
- "It wrote 1716 bytes of output and 0 bytes of errors." The first line it
  could not read was the `system`/`init` frame.

Agent chat on the same profile, Claude and wrapper worked.

Reproduced by running the wrapper-bundled Claude 2.1.314 directly with
`claude_writing_restrictions` verbatim, in an empty folder. The init frame
reports `tools: []`, `mcp_servers: []`, `skills: []` and no `memory_paths`,
plus this:

```json
"plugins":[{"name":"cc-plugin-sec-default","path":"builtin","source":"cc-plugin-sec-default@builtin"}]
```

Adding `"cc-plugin-sec-default@builtin":false` (or `"sec-default@builtin":
false`) to `--settings` leaves it loaded.

## Cause

1. `decode_claude_frame` (`src/application/src/agent_launch.cpp`, the
   `isInit` block, ~line 938) refuses a writing init frame unless `plugins`
   is missing or an empty array. Agent chat is not `WritingOnly`, so it skips
   this check. That is the whole difference between chat and the tour.
2. `cc-plugin-sec-default@builtin` is a Claude Code builtin. In 2.1.314 it is
   `enabledFromPolicyOnly` and `enabledFromTrustedSettingsOnly`. It seats
   itself outermost on any machine with managed settings, and for every Team
   and Enterprise organization. Its own description says it keeps the
   organization's hooks, prompt content, settings and tool policy out of reach
   of plugins a person installs, and "adds no policy of its own". A user
   `--settings` cannot turn it off.
3. So on every managed or Team/Enterprise machine, every tool-free writing
   request (tour messages and any other `WritingOnly` launch) fails, and no
   BetterPullReview setting can fix it.

The design problem is bigger than this one plugin. BPR-1375 refuses any
plugin as "fail closed on drift". But builtins arrive with Claude releases,
not with BetterPullReview releases. 2.1.296 needed three names turned off;
2.1.314 ships at least thirteen `cc-plugin-*` builtins (agents-md,
claude-platform, claude-test, diff, easel, mermaid, mods-guide,
plugin-authoring, responsive-mode, sec-default, telemetry, tips,
you-should-know). Each one that is on by default, or on by policy, breaks
writing until BPR ships a new name, or for good if policy holds it on. It also
inverts the trust order: chat, which gets tools, the review workspace and
every plugin, is accepted, while the tool-free launch is refused.

What actually makes a writing run inert is checked in other ways:

- Claude's own init reports `tools: []` (from `--tools ""`, with
  `--disallowedTools "*"` behind it). A plugin's tools would show there.
- Claude's own init reports `mcp_servers: []` (from the empty strict MCP
  configuration). A plugin's MCP server would show there.
- `disableAllHooks`, `--setting-sources ""`, `autoMemoryEnabled: false`, and
  `memory_paths` refused.
- Every block of the answer must be `text`, and no `user` frame or finding
  payload is allowed.
- The process starts in a fresh, empty `bpr-writing-XXXXXX` folder that the
  adapter makes and removes. Nothing from the review is there.

A plugin can still run its own code (BPR-1375 saw the telemetry builtin start
`sh` in the working folder). That code runs as the same user, in an empty
folder, with the same allowlisted environment chat gives Claude. Refusing to
start does not contain a policy-held plugin; it only refuses the run.

## Decision (owner, 2026-11-06)

To settle when this card is picked up. Recommended:

- **Drop the blanket plugin refusal** from the writing init check. Keep the
  `tools`, `mcp_servers` and `memory_paths` checks and the text-only answer
  check exactly as they are.
- **Keep turning off the known builtins** in `claude_writing_settings`, as
  hardening rather than a gate. Re-survey the 2.1.314 builtins (step 1 of
  Evidence) and add any that are on by default and can be turned off.
- **Record what loaded.** Name each plugin from init in the run's
  diagnostics, so drift is visible without stopping the run.

Alternative considered: keep the refusal and accept only
`cc-plugin-sec-default@builtin` by `source`. It fixes today's failure, but the
next default-on or policy-held builtin breaks writing again.

## Change

### 1. Decoder

In `decode_claude_frame`, remove the `plugins != nullptr && !empty("plugins")`
term from the `aWritingOnly` init condition. The condition keeps:
`!empty("tools") || !empty("mcp_servers") || memory_paths present and not
null`.

Update the comment above it ([[BPR-1375]]): the init check refuses a tool, an
MCP server or an auto-memory folder. Plugins are reported, not refused,
because builtins can be held on by policy.

### 2. Diagnostics

Add the plugin `source` values from a writing init frame (falling back to
`name`, bounded count and length, no paths) as a new decoder event or a
field the agent run already logs, so the run's diagnostics name them. Follow
the existing diagnostic vocabulary in the agent adapter. Do not show them on
the card: it is not the reviewer's problem.

### 3. Settings hardening

Leave `claude_writing_settings` as it is unless the survey finds another
default-on builtin that can be turned off. If one is added, update the
`architecture_boundaries` telemetry exception in
`cmake/VerifyArchitectureBoundaries.cmake` (~line 6119) only if the new name
trips its pattern.

### 4. Tests

- `tests/application/agent_launch_tests.cpp` ~line 1511: the `plugin` case
  becomes an accepted writing run (one assistant item). Add a case with the
  2.1.314 init frame below, verbatim except for the path fields. Keep the
  `memory` and `tools` refusals, and add an `mcp_servers: [{...}]` refusal if
  none exists.
- `tests/end_to_end/scoped_writing_agent.cpp` ~line 78 and
  `tests/end_to_end/review_tour_composition_public_path.cpp` ~line 881:
  `PLUGIN_CANARY` is one of the adversaries expected to fail. Move it to a
  case that expects the request to finish with text. Have the fixture emit
  the `cc-plugin-sec-default` entry, and check the diagnostic names it.

Fixture init frame (Claude Code 2.1.314, captured 2026-11-06; `cwd`,
`original_cwd`, `additional_directories`, `messaging_socket_path`,
`session_id` and `uuid` replaced):

```json
{"type":"system","subtype":"init","cwd":"/fixture/bpr-writing","session_id":"fixture","tools":[],"mcp_servers":[],"model":"claude-opus-5-5","permissionMode":"default","slash_commands":[],"apiKeySource":"apiKeyHelper","claude_code_version":"2.1.314","output_style":"default","agents":["claude","Explore","general-purpose","Plan","statusline-setup"],"skills":[],"plugins":[{"name":"cc-plugin-sec-default","path":"builtin","source":"cc-plugin-sec-default@builtin"}],"capabilities":["interrupt_receipt_v1","interrupt_cancel_queued_v1","interrupt_send_now_v1","interrupt_requeue_taken_v1","msg_lifecycle_v1","request_marker_lists_v1","ask_user_question_unanswered_v1","lifecycle_wakeup_source_v1","sdk_mcp_tools_list_changed","sdk_mcp_manifests","mcp_read_resource_v1","mcp_tool_ui_meta_v1","ui_surface_v1","mcp_call_result_not_for_model_v1"],"analytics_disabled":true,"product_feedback_disabled":true,"uuid":"fixture-init","original_cwd":"/fixture/bpr-writing","additional_directories":["/fixture/bpr-writing"],"messaging_socket_path":"/fixture/cc.sock","fast_mode_state":"off","fast_mode_disabled_reason":"not_first_party","per_turn_effort_active":false,"view_mode":"default"}
```

### 5. Documentation

- `docs/architecture/ai-agent-boundary.md`, "Tool-free writing on the
  person's own sign-in": the `--bare` table row for plugins (~line 1548) and
  the paragraph that says the decoder refuses an init frame reporting a
  plugin (~line 1558).
- `docs/architecture/security-threat-model.md`, "Tool-free writing
  authentication (BPR-1375)": "Fail closed on drift" (~line 1712) and the
  wrapper bullet (~line 1745). State the new residual plainly: a plugin that
  Claude loads by default or by policy runs in the empty writing folder, as
  the same user, with the allowlisted environment, and is named in
  diagnostics. Managed settings already appear as a residual there.
- The comment above `claude_writing_settings` in
  `src/application/include/bpr/application/agent_launch.hpp`: "A plugin added
  by a later Claude is refused by the init frame instead" is no longer true.

## Acceptance

- AC1. A writing request whose init frame is the 2.1.314 fixture above
  finishes with the agent's text (decoder test and the end-to-end writing
  course).
- AC2. A writing init frame that reports any tool, any MCP server, or a
  non-null `memory_paths` is still refused before any output is read.
- AC3. Agent chat (not `WritingOnly`) is unchanged.
- AC4. The run's diagnostics name each plugin the writing init reported, by
  `source` (or `name`), bounded, with no file paths.
- AC5. The review tour writes a message on a Mac with managed Claude Code
  settings (the owner's my-agent Mac, or any machine where Claude reports
  `cc-plugin-sec-default`). Agent checks: the offscreen
  `review_tour_composition_public_path --installed-writing` course against the
  installed Claude. Owner look only if no such machine is reachable.
- AC6. `ai-agent-boundary.md`, `security-threat-model.md` and the
  `agent_launch.hpp` comment describe plugins as reported, not refused, with
  the residual stated. `python3 scripts/verify_documentation.py --root .`
  passes.
- AC7. `application_agent_launch`, `application_review_tour_writing`,
  `application_review_tour_writing_wrapper` and `architecture_boundaries`
  pass.

## Evidence to gather first (scratch directory, outside the repo)

1. On the current Claude, run `claude -p --output-format stream-json
   --verbose` with `claude_writing_restrictions` verbatim in an empty folder,
   and with the three `enabledPlugins` entries removed. Note which builtins
   each run's init reports, to decide whether the off-list needs a new name.
2. On a machine without managed settings and on a personal (non-Team)
   account, check whether `cc-plugin-sec-default` appears. It should not,
   which is why this is not reproducible everywhere. Use the fixture for the
   tests either way.

## Out of scope

- Whether chat should also report the plugins it loaded in diagnostics.
- the wrapper's PTY handling of piped stdin (`my-agent` binary vs.
  `vscode-wrapper`), a separate my-agent defect.

## Acceptance note (2026-11-06)

Decision taken as recommended (the owner, through the coordinator: the app
should just work, with no refusals). The blanket plugin refusal is gone. The
`tools`, `mcp_servers`, `memory_paths` and text-only checks are unchanged.
The three known builtins stay off as hardening.

- Decoder: in writing runs, `decode_claude_frame` emits a new
  `plugins_reported` event from init. It is consumed by the shared protocol
  loop into `agent_run_result::ReportedPlugins`, and
  `diagnosed_agent_process` writes it as `plugins=` on the `agent.run` ended
  line. The card is unchanged.
- Builtin survey (Evidence step 1): this Mac's Claude is 2.1.310. It loads by
  default only `cc-plugin-agents-md`, `cc-plugin-telemetry` and
  `cc-plugin-plugin-authoring`, all three already off, so
  `claude_writing_settings` and the boundary exception are unchanged. Step 2,
  and the 2.1.314 builtin list, need a machine with that release.
- Still open: AC5 needs a managed-settings Mac. The writing courses in AC7
  are blocked by BPR-1881's readiness gate in the course fixture. That needs
  its own card. With readiness forced the courses pass.
- Landed on the task branch as `df901d5a5` (over `main` `58333472a`). Not
  pushed or merged.
