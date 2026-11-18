---
schema: "implement-rfc.card.v1"
id: "BPR-2259"
title: "Enhance signs in through the custom agent that drafted the comment, and a failed sign-in names the agent that ran"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2259--BPR-2259--017776e450c6.json","found_by":"owner, 2026-11-18: \"Enhance hit this failure even though my custom agent just drafted the comment it's trying to enhance\"","related_cards":["BPR-1375","BPR-1896","BPR-2051","BPR-2242"],"acceptance":["BPR-2259:custom-agent-launch","BPR-2259:words"]}
acceptance: []
---

# BPR-2259: Enhance signs in through the custom agent that drafted the comment, and a failed sign-in names the agent that ran

## Observed

The owner, 2026-11-18. Their custom agent (a configured Claude stream JSON
agent: their own program and arguments) drafted a comment. Enhance on that
comment then said:

```text
Claude isn't signed in. Run claude auth login in a terminal, then try again.
Your text is unchanged.
```

## Cause

Enhance does pick the same agent as every other ask: `writing_agent()` is the
footer picker's profile, and its run goes through the same added-agent process
chain (`agent_entry_process`) with the entry's own program and arguments.
Nothing looks for or probes the stock `claude`. The two runs differ in
Claude's arguments.

1. A tool-free writing run (Enhance, field drafts, Draft Tour with the diff
   up front) and a read-only run add `claude_writing_restrictions` or
   `claude_read_only_restrictions`. Both end with `--settings
   claude_writing_settings`. Agent tab runs carry no `--settings`.
2. Claude keeps only the last `--settings` it is given and does not merge them
   (security threat model, [[BPR-1896]]). A custom agent's own settings come
   first: in its saved arguments, which `agent_entry_process` puts before the
   protocol's, or from a wrapper that adds its own before the arguments it
   passes on. This app's settings replaced them, and with them the
   `apiKeyHelper`, token or gateway the agent signs in with. The first request
   got a 401, and the run ended `not_signed_in` ([[BPR-2142]]).
3. The Enhance answer for `not_signed_in` was fixed text from [[BPR-1375]],
   when writing ran only the stock Claude: "Claude isn't signed in. Run claude
   auth login". It named a program that never ran.

[[BPR-2242]] made those runs read the user's settings source. That covers a
sign-in kept in `~/.claude/settings.json`, but not one carried in the agent's
own `--settings`.

## Fix

- `agent_entry_process` takes this app's `--settings` pair out of every launch
  of a custom Claude agent (an entry with no preset). The agent's own settings
  stand, as they always did in its Agent tab runs. Claude Code added as itself
  keeps them. Hooks, plugins and auto-memory are then the custom agent's own,
  as they already were for a wrapper whose settings replace this app's.
  `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1` still holds memory off, and the init
  check still refuses any tool or MCP server. `ai-agent-boundary.md` and the
  threat model say so.
- A writing run that ends on a failed sign-in (Enhance, and the tour's field
  writing) names the agent that ran, by its name in the agent menus: "NAME
  isn't signed in, or its wrapper couldn't get a sign-in token. Sign it in
  again in a terminal, then Enhance again. Your text is unchanged."

## Acceptance

- **BPR-2259:custom-agent-launch** Enhance's launch (`build_agent_launch` on
  `enhance_writing`) for a custom agent whose sign-in rides in its own
  `--settings` runs that agent's program with its arguments first, keeps its
  `--settings` as the only one, stays tool-free on the user's settings with
  auto-memory off, never looks for or runs the stock `claude`, and a stand-in
  that signs in only through the settings Claude kept answers. A custom
  wrapper gets no `--settings`, and Claude Code added as itself keeps this
  app's (`application_agent_program_resolution`,
  `test_enhance_runs_the_custom_agent_on_its_own_sign_in`). Without the fix
  the test fails at "Claude keeps the agent's own settings".
- **BPR-2259:words** The Enhance and tour field-writing `not_signed_in`
  answers are built from `agent_display_name` of the run's agent and name no
  CLI (`window_tours.cpp`, `sign_in_failure_words`; compiled, no course).

## Follow-up, not gating

The owner's own agent and sign-in are not on this machine, so the fix is
checked against a stand-in. On a build with it, Enhance on a comment with
their custom agent chosen should answer; if it still fails, the run's log line
("The line it ended at:") and whether the agent's arguments or its wrapper
name `--settings` would place the remaining difference.
