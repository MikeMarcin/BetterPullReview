---
schema: "implement-rfc.card.v1"
id: "BPR-1375"
title: "Tool-free tour writing cannot use a signed-in Claude account"
status: "Ready"
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
