---
schema: "implement-rfc.card.v1"
id: "BPR-2304"
title: "Every Claude launch keeps the argument rules the harness audit established"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"hardening","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2304--BPR-2304--efe40d64f726.json","origin":"owner's harness audit of a sandboxing Claude launcher, 2026-11-19 (Claude Code 2.1.322), relayed by the coordinator: check the Claude stream-JSON launch against it and fix the gaps","related_cards":["BPR-2142","BPR-2178","BPR-2259"],"related_rfcs":["RFC-BPR-0104","RFC-BPR-0107"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"]}
---

# BPR-2304: Every Claude launch keeps the argument rules the harness audit established

## Asked

The owner audited a sandboxing launcher for Claude Code on 2026-11-19 (the
second such audit after the one [[BPR-2178]] records). The coordinator asked
for every Claude stream-JSON launch, both Claude Code itself and custom agents
that use the same protocol, to be checked against it, and for any gaps to be
fixed with deterministic core checks. The audit report stays outside the
repository. This card describes what it found in its own words.

## Findings against the launch as it stood

| Audit finding | Launch before this card | Gap | Change |
| --- | --- | --- | --- |
| Inline `--append-system-prompt` reaches the model and is served from the prompt cache on later calls; the `-file` form is dropped; instructions pasted into the message are written to the cache again on every call | Inline only (`build_agent_launch`, Claude case); pasted only when the agent's switch says so or past the Windows bound | No (no rule enforced it) | The core refuses either `-file` form |
| The caller's environment may not reach the agent | Every restriction is a flag. The one variable added, `CLAUDE_CODE_DISABLE_AUTO_MEMORY`, backs `--settings` up. A custom agent's restricted run has no `--settings` (it would replace the agent's sign-in, [[BPR-2259]]), so the variable is the only thing turning memory off | Residual | No option other than `--settings` turns auto-memory off. Recorded in the boundary; the init frame still reports `memory=on` |
| Options that take several values swallow a following prompt | Placed before `-p`, each followed by another option ([[BPR-2142]]); a test-only list of seven | No, but `--betas` and `--file` were missing and only tests checked it | The list moves to core with nine entries; every launch is checked before it starts |
| `--setting-sources user` and an empty `--strict-mcp-config` isolate | Given in a pull request's checkout, on restricted runs and where MCP is off | No | The core requires them where they apply and refuses a source other than `user` |
| A path-scoped `Write(...)` rule is not applied; `Edit(...)` is | The grant gives both rules for the scratch folder | No | The core refuses a `Write` rule without its `Edit` twin |
| Suggested order: fixed arguments, then `-p`, then the rest | Fixed arguments first (`agent_entry_process`); `-p` after the multi-value options, as RFC-BPR-0104 AC11 requires; prompt on stdin | Order differs, harmlessly | None. The audit found the order makes no difference; moving `-p` first stays with [[BPR-2178]] and would need AC11 amended |

The Agent tab in the reviewer's own checkout and a Review Cycle round read that
folder's settings, and an agent whose MCP setting is `own` keeps its servers.
These are deliberate and unchanged.

## Acceptance

- AC1: no Claude launch gives instructions as a file.
- AC2: every option that takes several values comes before `-p` and is
  closed by another option, in every launch kind.
- AC3: every scoped `Write(...)` rule has its `Edit(...)` twin.
- AC4: a launch in a pull request's checkout, and every tool-free or
  read-only run, reads only the user's settings; where MCP is off, an empty
  strict configuration is given.
- AC5: a custom agent's fixed arguments come first and this app's arguments
  follow unchanged, so a fixed option that takes several values is closed.
- AC6: the fixture agent still runs through the real launcher.

## Evidence

Candidate `3ccaac970` (core-only and dev Debug trees at that commit, macOS; first run at `c106f3203`, then rerun after rebasing onto [[BPR-2303]]):

- `core_claude_launch_arguments`: each rule passes and fails as expected
  (AC1 to AC4).
- `application_agent_launch_policy`: `test_every_claude_launch_keeps_the_audited_rules`
  covers writing, read-only, resumed writing, granted in a checkout, fresh
  granted, MCP `own` and both round kinds (AC1 to AC4).
- `application_agent_program_resolution`: `test_a_custom_agents_fixed_arguments_come_first`
  (AC5).
- `qt_agent_steering_fixture`, which runs the fixture agent as Claude Code
  through the real launcher, and `qt_claude_process_wrapper`, which records
  what a wrapper receives (AC6).
- 82 related core-only suites (agent, cycle, tour, writing, draft, claude,
  follow-up) pass. The application-wide public paths were not rebuilt, because
  the disk was too full to build the composition.
- The six boundary suites (`core_dependency_boundary`,
  `core_boundary_contract`, `architecture_boundaries`,
  `architecture_boundary_contract`, `git_protocol_boundary`,
  `core_computation_boundaries`) pass, and so does
  `python3 scripts/verify_documentation.py --root .`.
- clang-format 22 on the changed files; clang-tidy on the changed translation
  units reports only findings that were already there
  (`agent_launch.cpp:1769`, and `inline_`/`long_` in the policy test).
