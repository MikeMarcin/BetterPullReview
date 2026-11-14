---
schema: "implement-rfc.card.v1"
id: "BPR-2178"
title: "Claude runs and Check work through the sandbox wrapper"
status: "Backlog"
rfcs: ["RFC-BPR-0104"]
depends_on: []
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2178--BPR-2178--ee5048d7ae4a.json", "origin": "owner's sandbox-wrapper harness audit, 2026-11-13 (claude-stream-json catalog v2, Claude Code 2.1.314); owner: \"Yes, file it\" (2026-11-14)"}
---

# BPR-2178: Claude runs and Check work through the sandbox wrapper

The owner ran a harness audit of a corporate wrapper ("sandbox-wrapper", Claude only) on 2026-11-13. Two findings break this app through it today; one changes how its sign-in failure must be reported.

## Findings that need work

1. **`-p` must come first** (probe `t-p-late`). The wrapper needs `-p` as the first argument after any fixed leading arguments; any other order fails with a 401 that looks like an expired token. `build_agent_launch` (`src/application/src/agent_launch.cpp`, Claude case) puts `-p` after the grant options, the restrictions and `--append-system-prompt`, so every Claude run through the wrapper fails and looks like a sign-out.
   Fix: `-p --input-format stream-json --output-format stream-json --verbose` first (after the profile's fixed arguments), then everything else, for every Claude launch (chat, resumed, writing-only, read-only, model catalog). Keep each multi-value option (`--add-dir`, `--allowedTools`, `--disallowedTools`) followed by another option (`t-variadic-swallow`); the prompt stays on stdin (`t-stdin-stream-json` works).
2. **`--version` and `--help` fail** (the harness always runs `-p`: "Input must be provided either through stdin or as a prompt argument when using --print"). Check's version probe (`build_agent_status_launches`) and effort detection (`build_agent_help_launch`) fail through it.
   Fix: when either fails that way, fall back to one tiny `-p` stream-json call and read the version and model from its `init` event; if that fails too, show the version as unknown, not Check as failed.
3. **Sign-in failure reads as a sign-in problem.** A 401 / `authentication_failed` (stream JSON or plain text with "Session exited with code 1") is reported as the agent's sign-in failing, and the run stops at the first `authentication_failed` retry instead of waiting minutes (follow-up from the 2026-11-11 wrapper results).

## Confirmed, no change

Inline `--append-system-prompt` applied and cached across changing questions (~$0.0035 per warm call vs $0.0047 pasted); `--append-system-prompt-file` dropped; `--setting-sources user`, empty `--strict-mcp-config`, `--add-dir`, `--model` kept; `Edit(...)` rules allow a scoped write (`Write(...)` alone is ignored: keep emitting both).

## To check before acting

`t-env`: the caller's environment may not reach the agent, but the probe relied on the agent's shell tool, which may have been refused. Recheck without a shell tool before moving the auto-memory hardening or `AllowedEnvironment` to flags.

## Outcome

- A Qt-free test fixes the argument order: `-p` first for every Claude launch kind, each multi-value option followed by another option.
- A stub wrapper that refuses any order but `-p` first, and fails `--version`/`--help` as the real one does, passes a chat run, a resumed run and Check (version from `init` or unknown).
- A stub 401 reports a sign-in failure within one retry.
- The owner reruns the harness audit (or Test E) through the wrapper and a run answers.
