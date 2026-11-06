---
schema: "implement-rfc.card.v1"
id: "BPR-1896"
title: "Tool-free writing runs when a wrapper's settings turn Claude's auto-memory back on"
status: "Done"
rfcs: []
depends_on: ["BPR-1891"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-06: after BPR-1891 shipped (0.4.2 build 26), review tour messages through a Claude wrapper still stopped with 'answered in a form this app can't read' (malformed_frame) on the init line; agent chat worked","related_cards":["BPR-1375","BPR-1827","BPR-1891"],"related_rfcs":[]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"fbf97e60fce65b90da5d5ef8d1700d745dd60968","evidence":{"kind":"command","detail":"application_agent_launch (test_claude_writing_reads_a_wrapper_that_turns_auto_memory_on): the card's fixture init frame, then one text assistant frame and a success result, finishes with the agent's text in a writing run. application_review_tour_writing runs MEMORY_CANARY, whose fixture writes that init frame, and the tour message finishes with the text."}},{"criterion":"AC2","outcome":"pass","revision":"fbf97e60fce65b90da5d5ef8d1700d745dd60968","evidence":{"kind":"command","detail":"application_agent_launch: with memory on, an init frame that reports a tool or an MCP server is refused before any output is read, and a user frame, a tool_use block or a finding payload is refused. The BPR-1375 tool and MCP refusals still hold."}},{"criterion":"AC3","outcome":"pass","revision":"fbf97e60fce65b90da5d5ef8d1700d745dd60968","evidence":{"kind":"command","detail":"application_agent_launch: the same stream in chat reads unchanged and reports neither plugins nor memory, and a chat launch's environment does not carry CLAUDE_CODE_DISABLE_AUTO_MEMORY; the writing launch's environment is the chat environment plus that one entry."}},{"criterion":"AC4","outcome":"pass","revision":"fbf97e60fce65b90da5d5ef8d1700d745dd60968","evidence":{"kind":"command","detail":"application_agent_launch: describe writes ' memory=on' after plugins= and parse_agent_run_diagnosis reads it back exactly, refusing memory=off, a path value or a reordered token. The event carries no text. application_review_tour_writing finds the agent.run line '... plugins=cc-plugin-sec-default@builtin,cc-plugin-agents-md@builtin,cc-plugin-plugin-authoring@builtin memory=on' and no '/fixture/' path in any log line."}},{"criterion":"AC5","outcome":"pass","revision":"fbf97e60fce65b90da5d5ef8d1700d745dd60968","evidence":{"kind":"command","detail":"application_agent_launch: a writing launch's environment carries CLAUDE_CODE_DISABLE_AUTO_MEMORY=1 exactly once, and permitted_agent_environment_name admits the name. The end-to-end writing fixture exits 7 unless the variable is set, and application_review_tour_writing and application_review_tour_writing_wrapper pass."}},{"criterion":"AC6","outcome":"pending","revision":"fbf97e60fce65b90da5d5ef8d1700d745dd60968","evidence":{"kind":"observation","detail":"Offscreen half passed: review_tour_composition_public_path --installed-writing /opt/homebrew/bin <settings-replacing test wrapper> claude-sonnet-5-5 once, on this Mac's Claude Code 2.1.310, wrote the Problem message three times out of three (Finished: success, text changed, one undo restored bytes and selection, redo ran no inference). A variant that also drops CLAUDE_CODE_DISABLE_AUTO_MEMORY had init report memory_paths, which the decoder read past; it wrote in two of three runs. The third ended protocol_failure on an assistant thinking block that the text-only check refuses. That refusal is unrelated to memory. The owner's half is pending: the owner confirms on the next prerelease, through their own wrapper (owner, 2026-11-06: no attended review, no frozen build)."}},{"criterion":"AC7","outcome":"pass","revision":"fbf97e60fce65b90da5d5ef8d1700d745dd60968","evidence":{"kind":"command","detail":"ai-agent-boundary.md (Auto-memory row, decoder paragraph) and security-threat-model.md (Fail closed on drift, a new auto-memory bullet, Authority, the wrapper bullets) describe plugins and auto-memory as reported, not refused, with the wrapper residual and the evidence below. The comments above claude_writing_settings and claude_writing_restrictions match. python3 scripts/verify_documentation.py --root . passes after rebasing onto main."}},{"criterion":"AC8","outcome":"pass","revision":"fbf97e60fce65b90da5d5ef8d1700d745dd60968","evidence":{"kind":"command","detail":"ctest --preset dev: architecture_boundaries, application_agent_launch, application_review_tour_writing and application_review_tour_writing_wrapper pass (4/4). coding-format-check passes. Owner direction 'dont wait for tests': no other suite was run."}}]
---

# BPR-1896: Tool-free writing runs when a wrapper's settings turn Claude's auto-memory back on


## Observed

The owner, 2026-11-06, BetterPullReview 0.4.2 build 26 (includes BPR-1891).
The agent profile runs Claude Code 2.1.314 through a wrapper program that
hands Claude settings of its own (an organization's managed launcher). A
review tour message stopped about a second after it started:

- Card: "… answered in a form this app can't read.", "A line of its output
  isn't in the format its profile names, or arrived out of order.
  (malformed_frame)", "It wrote 1716 bytes of output and 0 bytes of errors."
  The rejected line was the `system`/`init` frame.
- Session log: `agent.run.failed ... outcome=protocol_failure
  failure=malformed_frame stdout_bytes=1716 stderr_bytes=0`, twice. An agent
  chat run between them succeeded (`stdout_bytes=51511`).

A probe ran the wrapper with `claude_writing_restrictions` verbatim, in an
empty folder. The init frame reported:

```text
tools: []
mcp_servers: []
plugins: [cc-plugin-sec-default@builtin, cc-plugin-agents-md@builtin, cc-plugin-plugin-authoring@builtin]
memory_paths: {"auto": "/Users/<user>/.claude/projects/-private-tmp-probe-work-XXXXXX/memory/"}
skills: []
apiKeySource: "apiKeyHelper"
```

A full writing turn through the same wrapper and arguments wrote exactly three
frames, and nothing else the writing decoder refuses:

```text
1: system/init       memory_paths=set tools=0 mcp=0
2: assistant         blocks=['text']
3: result/success    is_error=False has_result=True
```

The same Claude run directly (no wrapper) with the same arguments reports no
`memory_paths`, only `cc-plugin-sec-default`, and `apiKeySource: "none"`.

## Cause

1. `decode_claude_frame` (`src/application/src/agent_launch.cpp`, the
   `isInit` block) refuses a writing init frame whose `memory_paths` is
   present and not null. Agent chat is not `WritingOnly`, so it skips this.
2. Under this wrapper, BPR's `--settings` (`claude_writing_settings`) does not
   take effect. Auto-memory is on although BPR passes
   `autoMemoryEnabled: false`. `agents-md` and `plugin-authoring` are loaded
   although BPR turns them off. Sign-in comes from an `apiKeyHelper` that BPR
   never passed. Most likely the wrapper passes its own `--settings` and that
   one wins. `disableAllHooks` very likely does not apply either.
3. So, as with policy-held plugins in BPR-1891, every writing request through
   such a wrapper fails, and no BetterPullReview setting can fix it.

What the refusal protects is small:

- The auto-memory folder is keyed to the working folder. A writing run's
  working folder is a fresh `bpr-writing-XXXXXX` made for that request, so the
  memory folder is new and empty: nothing from an earlier run or another
  project loads.
- With `tools: []` checked, Claude cannot write to the memory folder or
  anything else during the run.
- At most an empty folder is left under `~/.claude/projects/`.

The threat model already names the wider point as a residual ("a wrapper can
drop them, so a writing run is only as tool-free as the wrapper keeps it").
The checks that make a run tool-free (tools, MCP servers, a text-only answer,
no `user` frame, no finding payload) are all read from Claude's own output,
and they all pass here.

## Decision (owner, 2026-11-06)

Option 1 of three considered:

- **Report the auto-memory folder, do not refuse it.** Keep the `tools`,
  `mcp_servers`, text-only, `user`-frame and finding-payload checks unchanged.
- **Best-effort hardening:** set `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1` in the
  writing launch's environment (present in the 2.1.314 binary), next to
  `autoMemoryEnabled: false`. Whether a wrapper passes it through is
  step 1 of Evidence; the card does not depend on it.

Rejected: keep refusing (writing never works through such a wrapper), and
refuse only a memory folder outside the run's own folder key (depends on
Claude's path layout, which changes between releases).

## Change

### 1. Decoder

In `decode_claude_frame`, drop the `memory_paths` term from the
`aWritingOnly` init condition. The condition keeps `!empty("tools") ||
!empty("mcp_servers")`.

Update the [[BPR-1375]] / [[BPR-1891]] comment above it: the init check
refuses a tool or an MCP server. Plugins and an auto-memory folder are
reported, not refused, because a wrapper's or policy's settings can turn
them on over this app's.

### 2. Diagnostics

Report that auto-memory was on, as `memory=on` on the `agent.run` line next
to BPR-1891's `plugins=`, following the same path: a decoder event (or extend
`plugins_reported` into one init-report event, whichever reads cleaner), a
field on the run result, and `agent_run_diagnosis`. **Never log the path**:
it holds the user's home folder name. A boolean is enough.

### 3. Environment hardening

Add `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1` to the environment of a `WritingOnly`
Claude launch only (`build_agent_launch`, or wherever `WritingOnly`
arguments are appended). Agent chat is unchanged. Check that the profile
environment allowlist and the credential-word filter let it through, and that
`architecture_boundaries` accepts it.

### 4. Tests

- `tests/application/agent_launch_tests.cpp`, the BPR-1375 writing case
  (`memory_paths":{"auto":"/m"}` currently expected to refuse): it becomes an
  accepted writing run (one assistant item) that reports memory on. Keep the
  `tools` and MCP refusals.
- Add a decoder case for the wrapper stream: the three frames below.
- The end-to-end writing fixture (`tests/end_to_end/scoped_writing_agent.cpp`)
  gets a canary that emits `memory_paths` and the three plugins. The tour
  course (`review_tour_composition_public_path.cpp`) expects it to finish with
  text and the diagnostic to read `memory=on`.
- The writing launch's environment contains `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1`,
  and a chat launch's does not.

Fixture init frame (paths, IDs and `capabilities` replaced; the probe
recorded fields, not the raw line):

```json
{"type":"system","subtype":"init","cwd":"/fixture/bpr-writing","session_id":"fixture","tools":[],"mcp_servers":[],"model":"claude-opus-5-5","permissionMode":"default","slash_commands":[],"apiKeySource":"apiKeyHelper","claude_code_version":"2.1.314","output_style":"default","agents":["claude","Explore","general-purpose","Plan","statusline-setup"],"skills":[],"plugins":[{"name":"cc-plugin-sec-default","path":"builtin","source":"cc-plugin-sec-default@builtin"},{"name":"cc-plugin-agents-md","path":"builtin","source":"cc-plugin-agents-md@builtin"},{"name":"cc-plugin-plugin-authoring","path":"builtin","source":"cc-plugin-plugin-authoring@builtin"}],"memory_paths":{"auto":"/fixture/home/.claude/projects/-fixture-bpr-writing/memory/"},"capabilities":[],"analytics_disabled":true,"product_feedback_disabled":true,"uuid":"fixture-init","original_cwd":"/fixture/bpr-writing","additional_directories":["/fixture/bpr-writing"],"messaging_socket_path":"/fixture/cc.sock","fast_mode_state":"off","fast_mode_disabled_reason":"not_first_party","per_turn_effort_active":false,"view_mode":"default"}
```

Follow it with an `assistant` frame holding one `text` block and a `result`
frame with `subtype: "success"`, `is_error: false` and a `result` string,
all under the same `session_id`.

A test wrapper that reproduces the case without any third-party launcher: a
script that drops BPR's `--settings` value and passes
`--settings '{"autoMemoryEnabled":true}'` instead, then execs `claude`. Use it
for AC6 on any machine with Claude Code installed.

### 5. Documentation

- `docs/architecture/ai-agent-boundary.md`, "Tool-free writing on the
  person's own sign-in": the Auto-memory row of the `--bare` table and the
  paragraph on what the decoder refuses (as amended by BPR-1891).
- `docs/architecture/security-threat-model.md`, "Tool-free writing
  authentication (BPR-1375)": "Fail closed on drift" and the wrapper bullet.
  Record the observed case: a wrapper's settings can replace this app's
  (auto-memory on, builtins on, hooks very likely on). Record why the memory
  folder is harmless: keyed to a fresh empty folder, and no tool can write it.
- The comments above `claude_writing_settings` and
  `claude_writing_restrictions` in
  `src/application/include/bpr/application/agent_launch.hpp`: say that a
  wrapper may replace the settings, and that the init check enforces only tools
  and MCP servers.

## Acceptance

- AC1. A writing request whose stream is the fixture above finishes with the
  agent's text (decoder test and the end-to-end writing course).
- AC2. A writing init frame that reports any tool or any MCP server is still
  refused before any output is read. A `user` frame, a non-text block or a
  finding payload in a writing run is still refused.
- AC3. Agent chat (not `WritingOnly`) is unchanged, environment included.
- AC4. The `agent.run` diagnostic reads `memory=on` when init reports an
  auto-memory folder, and never contains its path.
- AC5. A writing launch's environment carries
  `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1`.
- AC6. The review tour writes a message through the settings-replacing test
  wrapper from Change §4, offscreen
  (`review_tour_composition_public_path --installed-writing` with the
  wrapper as the program). The owner confirms the same with their own
  wrapper, as an attended review question.
- AC7. The documentation above describes plugins and auto-memory as reported,
  not refused, with the wrapper residual stated.
  `python3 scripts/verify_documentation.py --root .` passes.
- AC8. `application_agent_launch`, `application_review_tour_writing`,
  `application_review_tour_writing_wrapper` and `architecture_boundaries`
  pass.

## Evidence to gather first

1. Through the test wrapper, with `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1` in the
   environment: does init still report `memory_paths`? If the variable wins
   over a settings file that turns memory on, record that in the threat model.
   If not, keep the variable anyway (it works without a wrapper), and say it
   does not override a wrapper's settings.
2. Confirm that Claude Code keeps only the last `--settings` value when two are
   given. That is the likely mechanism, so the threat model can state it as
   fact.

## Out of scope

- Whether BPR can get its writing settings past a wrapper that replaces them.
  That is up to the wrapper's maintainers.

## Evidence (2026-11-06)

Gathered on this Mac's Claude Code 2.1.310 in an empty scratch folder outside
the repository, with the card's writing arguments and a test wrapper that
swaps this app's `--settings` value for `{"autoMemoryEnabled":true}`. Only
frame fields were recorded.

| Run | Init `memory_paths` | Init plugins |
|---|---|---|
| Direct, this app's settings | absent | none |
| Test wrapper | set | the three builtins |
| Test wrapper, `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1` | absent | the three builtins |
| Direct, `{"autoMemoryEnabled":true}` and the variable | absent | the three builtins |
| Direct, this app's settings then `{"autoMemoryEnabled":true}` | set | the three builtins |
| Direct, `{"autoMemoryEnabled":true}` then this app's settings | absent | none |

1. The variable wins over a settings value that turns memory on, so it keeps
   memory off through a wrapper that replaces the settings, as long as the
   wrapper passes the environment through. The threat model records this.
2. Claude keeps only the last `--settings` it is given and does not merge the
   two: with this app's first, its plugin switches were lost too. The threat
   model states this as fact for 2.1.310. The 2.1.314 behaviour behind the
   managed launcher is inferred from the same symptoms.

## Result

- The decoder's writing init check refuses only tools and MCP servers. Plugins
  and an auto-memory folder come out as one init report (`reported_init`).
  The new `auto_memory_reported` event carries no text. It becomes
  `agent_run_result::ReportedAutoMemory` and `memory=on` on the `agent.run`
  line.
- `build_agent_launch` appends `claude_writing_environment`
  (`CLAUDE_CODE_DISABLE_AUTO_MEMORY=1`) to a writing launch only.
- The `--installed-writing` course now finishes the agent setup before
  writing, as the writing course has since BPR-1894. Without that it stopped
  at its first save.
- Pending: the owner confirms on the next prerelease that a review tour
  message writes through their own wrapper (AC6). No attended review or frozen
  build was made, on the owner's instruction.
- Follow-up candidate, not filed: a writing run whose Claude writes an
  assistant `thinking` block is refused by the unchanged text-only check. Seen
  once in three memory-on runs, and on both runs with the default model.
- Landed on the task branch as `fbf97e60f`, over `main` `f1ab0c5e6`. Not
  pushed or merged.
