---
schema: "implement-rfc.card.v1"
id: "BPR-1375"
title: "Tool-free tour writing cannot use a signed-in Claude account"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1225 installed-provider qualification, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1375--BPR-1375--187a76842652.json","found_at":"dab108c97","owner_question_needed":false,"owner_decision":{"2026-10-09":"no claude should use -p or the equivalent, the free version cli api same as codex"}}
---

# BPR-1375: Tool-free tour writing cannot use a signed-in Claude account

## What was observed

Found by the BPR-1225 qualification run on 2026-10-09 (Mac, main at
`dab108c97`, installed Claude Code 2.1.278). The run and its logs are in the
[review-tour ledger](../../verification/RFC-BPR-0039-0043.md#bpr-1225-installed-provider-qualification--2026-10-09).

Every tool-free writing launch (Problem, Solution, stop, outline, Settings
Test) appends `--bare` to Claude's arguments
(`src/application/src/agent_launch.cpp`, `build_agent_launch`). The installed
CLI's own help for `--bare` says: "Anthropic auth is strictly
ANTHROPIC_API_KEY or apiKeyHelper via --settings (OAuth and keychain are
never read)." The child environment is the profile allowlist (`HOME`, `PATH`,
`LANG`, `LC_ALL`, `TMPDIR`, `TEMP`, `TMP`), and names containing `API_KEY`
are refused even if listed, so `ANTHROPIC_API_KEY` can never reach it;
`--setting-sources ""` keeps a user `apiKeyHelper` out as well.

So a person who signs in to Claude Code with their Claude account (OAuth)
can never run tour writing through the shipped path. In the run, all seven
writing requests reached the installed `claude`, whose init frame reported
`tools: []`, `mcp_servers: []`, `apiKeySource: "none"`, and each answered
`Not logged in · Please run /login`. BetterPullReview showed "Assistance did
not complete; your text is unchanged." with agent status
`Finished: protocol_failure`. The text and the person's draft were untouched,
which is correct, but the message does not say that Claude is not signed in.

Two smaller observations from the same run:

- **No model choice for Claude.** `build_agent_launch` refuses a requested
  or configured model for every protocol except Codex App Server
  (`unsupported_runtime_settings`), so the only way to pick a Claude model
  is a process wrapper that appends `--model`.
- **The failure reads as a protocol failure.** Claude's "Not logged in"
  arrives as an ordinary assistant text frame with `is_error: true`; the
  decoder treats it as malformed output.

## Owner question

How should tool-free writing authenticate? Candidates, not yet chosen:

1. Drop `--bare` from the writing launch and keep the other restrictions
   (`--tools ""`, `--disallowedTools "*"`, empty strict MCP config,
   `--setting-sources ""`, `--disable-slash-commands`,
   `--no-session-persistence`, `disableAllHooks`). OAuth from the keychain is
   then read. What `--bare` also skipped (CLAUDE.md auto-discovery, plugin
   sync, auto-memory, background prefetches) needs checking against the
   no-tools/MCP/hooks/writes/credentials boundary.
2. Keep `--bare` and document that tour writing needs an API key supplied
   through a process wrapper (BPR-1301), which can add `apiKeyHelper` to
   `--settings` itself.
3. Something else the owner names.

## Owner decision, 2026-10-09: the CLI on the person's own sign-in

The owner answered in chat on 2026-10-09, relayed by the coordinator, verbatim:

```text
no claude should use -p or the equivalent, the free version cli api same as codex
```

Read as: tour writing runs the installed CLIs non-interactively on the
owner's own subscription sign-in (the Claude account for `claude -p`, the
ChatGPT account for `codex exec` or its non-interactive equivalent), not a
paid API key. This chooses a variant of candidate 1 above: drop `--bare`,
keep every other restriction, and neutralise what `--bare` used to skip
another way. Candidate 2 (an API key through a wrapper) is not chosen.

Scope given with the decision:

1. Claude: `claude -p` without `--bare`, no tools, `--disallowedTools "*"`,
   an empty strict MCP configuration, `--setting-sources ""`,
   `--disable-slash-commands`, `--no-session-persistence`, hooks off; check
   what `--bare` suppressed and close each another way (for example an empty
   working folder); prove it with the init frame and no files written in the
   working folder. A Claude model setting passed as `--model` (the owner
   chose `claude-sonnet-5-5`).
2. Codex: a tool-free non-interactive protocol on the ChatGPT account, or, if
   Codex cannot enforce "no tools", not shipped, with exactly what is missing
   reported, and the models the installed Codex lists for the account.
3. A signed-out CLI says it is not signed in, and the draft stays unchanged.
4. Launch-argument tests for both protocols, the two restricted-writing
   courses still passing, the decoder handling the not-signed-in frame, and
   the threat model and `ai-agent-boundary.md` updated.

## Acceptance

- **BPR-1375:auth** With the chosen design, a Claude Code signed in the
  owner's chosen way completes one tool-free writing request through the
  shipped path (no wrapper unless the design requires one), and the init
  frame still reports no tools and no MCP servers.
- **BPR-1375:message** A signed-out Claude produces a status that says Claude
  is not signed in, and the draft is unchanged.
- **BPR-1375:boundary** The existing restricted-writing courses
  (`application_review_tour_writing`, `application_review_tour_writing_wrapper`)
  still pass, and the threat model and `ai-agent-boundary.md` describe the
  chosen authentication.

## Completion evidence, 2026-10-09 (Mac, branch `bpr-1375`)

Code: commit `d9d5acd39` ("tool-free writing runs claude -p on the person's
own sign-in").

- **BPR-1375:auth — met.** Claude Code 2.1.296, signed in with the owner's
  Claude account. `QT_QPA_PLATFORM=offscreen build/dev/tests/end_to_end/review_tour_composition_public_path --installed-writing /opt/homebrew/bin - claude-sonnet-5-5 once`
  ran one Problem writing request through the shipped path, no wrapper:
  agent status `Finished: success`. Captured once through a pass-through
  wrapper, the init frame reported `tools: []`, `mcp_servers: []`,
  `plugins: []`, no `memory_paths`, `apiKeySource: "none"`,
  `model: "claude-sonnet-5-5"` and a fresh `bpr-writing-*` working folder,
  and the folder was empty and gone afterwards. Root cause found on the way:
  Claude files its macOS keychain sign-in under `USER`, which the profile
  allowlist lacked (`claude auth status` read `loggedIn: false` without it,
  `true` with it); the shipped Claude profile now passes `USER`.
- **BPR-1375:message — met.** Signed-out 2.1.296 answers with an assistant
  frame carrying `error: "authentication_failed"` (observed, no inference).
  The decoder maps it to `not_signed_in`; `application_review_tour_writing`
  checks the editor says "Claude isn't signed in. Run claude auth login in a
  terminal, then try again. Your text is unchanged." with text and history
  unchanged; `application_agent_launch` covers the decoder.
- **BPR-1375:boundary — met.** `application_review_tour_writing` and
  `application_review_tour_writing_wrapper` pass (the wrapper course also
  saves and restarts the Model setting and refuses an option-like model);
  `application_agent_launch`, `application_agent_protocol_loop`,
  `application_agent_finding_protocol`, `sqlite_workbench_settings_storage`,
  `qt_settings_window`, `qt_claude_process_wrapper`, `qt_agent_launch_canary`,
  `qt_agent_launch_panel`, `application_agent_invocation_host`,
  `application_writing_delivery`, `core_agent_profile`, `core_agent_wrapper`
  and `architecture_boundaries` pass. The threat model ("Tool-free writing
  authentication (BPR-1375)") and `ai-agent-boundary.md` ("Tool-free writing
  on the person's own sign-in") describe the design, what `--bare` skipped
  and how each is closed, and the residual risks.

What `--bare` skipped was checked on 2.1.296 against a loopback stand-in for
the Messages API and a debug log, with a planted `CLAUDE.md`, `AGENTS.md`,
`.claude/settings.json` hook and `.mcp.json` server in the working folder:
none loaded or ran. Without `enabledPlugins` off, the built-in reporting
plugin started `sh` in the working folder; with it, no process.

Codex: not shipped. Codex CLI 0.162.1 (and 0.155.1) declares `exec` (with
nested `apply_patch` and MCP resource reads), `wait`, `request_user_input`,
`request_user_input_async` and six `collaboration` tools for every model the
ChatGPT account lists, whatever flags are set; neither `codex exec` nor App
Server `thread/start` takes a tool list. Models the installed Codex lists for
the account (`codex debug models`, 0.162.1): `gpt-6.1-sol` (GPT-6.1-Sol, its
built-in default), `gpt-6-astra`, `gpt-6-sol`, `gpt-6-luna`, `gpt-5.6-sol`,
`gpt-5.6-terra`, `gpt-5.6-luna`, `gpt-daybreak-blue-latest` (Daybreak Blue).
`gpt-6.1-sol` answered on the ChatGPT account on 0.162.1.

Not covered: Review Cycle Send to Agent does not use the Model setting yet;
Windows and Linux were not run (USER is harmless there; Claude keeps its
sign-in in a file).
