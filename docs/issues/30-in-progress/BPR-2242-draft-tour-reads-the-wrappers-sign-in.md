---
schema: "implement-rfc.card.v1"
id: "BPR-2242"
title: "Draft Tour signs in through a wrapper as Ask in the Agent tab does"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","origin":"owner report, 2026-11-16, on the shipped 0.5.0 prerelease (v0.5.0-build.61, ee0ea7c9c)","reservation":"docs/issues/_reservations/BPR-2242--BPR-2242--9e4b933a1bf9.json","related":["BPR-1375","BPR-2142","BPR-2178"],"acceptance":["BPR-2242:parity","BPR-2242:words","BPR-2242:owner-retry"]}
---

# BPR-2242: Draft Tour signs in through a wrapper as Ask in the Agent tab does

## What the owner saw

The owner, 2026-11-16, on the 0.5.0 prerelease (`v0.5.0-build.61`): "I have
a PR open and I can ask questions from my wrapper in the agents tab but draft
tour fails on the tour tab." Draft Tour said:

```text
The agent isn't signed in, or its wrapper couldn't get a sign-in token. Sign
in again in a terminal, or try again later. What went wrong: It isn't signed
in. (not_signed_in). It wrote 1617 bytes of output and 0 bytes of errors.
First line this app couldn't read: {"type":"system","subtype":"api_retry",
"attempt":1,"max_retries":10,"retry_delay_ms":547,"error_status":401,
"error":"authentication_failed",...}
```

## Cause

Both runs launch the same agent the same way: the footer picker's profile
(`writing_agent()` is `agent_panel()->selected_profile()`), the same process
chain (added agent, then program, then Claude wrapper), the same environment
allowlist, the same entry arguments. They differ in Claude's arguments.

| | Ask in the Agent tab | Draft Tour (diff up front: tool-free) | Draft Tour reading the repository, tour chats (read-only) |
|---|---|---|---|
| settings sources | `--setting-sources user` in the pull request's checkout; none named (all) in the reviewer's own | `--setting-sources ""` | `--setting-sources ""` |
| `--settings` | none | `claude_writing_settings` | `claude_writing_settings` |
| tools | the agent's | `--tools ""` | four, read-only |
| working folder | the checkout | an empty `bpr-writing-XXXXXX` | the review's workspace |
| environment | allowlist | allowlist and `CLAUDE_CODE_DISABLE_AUTO_MEMORY=1` | the same |

`--setting-sources ""` stops Claude reading the user's own settings, which is
where an `apiKeyHelper`, or the token and gateway a wrapper's setup writes,
lives. Checked on installed Claude Code 2.1.319 with a scratch config folder
holding only `{"apiKeyHelper":"echo stub-not-a-real-key"}`, the API pointed at
a closed loopback port (no request left the machine, no usage spent):
`--setting-sources ""` starts with `apiKeySource: "none"`; `user`, or no flag,
with `"apiKeyHelper"`. With no key source Claude falls back to whatever else
it has and the first request gets a 401: the frame the owner saw.

Every restricted run had it: Draft Tour both ways, field drafts, Enhance,
tour side chats and the whole-tour conversation, local draft descriptions,
discussions and check plans, and the agent-assistance first answers. Only
Agent tab conversations read the user's settings. The difference has been
there since BPR-1375 (2026-10-09); lane HEAD (`f9f72704e`) still had it: the
launch code differs from the prerelease only by BPR-2219's paragraph joining.
The work that shares one agent host between the tour and the Agent tab
(BPR-2181 and around it) never touched these arguments.

### Not the cause, checked

- The `api_retry` stop is deliberate: BPR-2142 (owner, 2026-11-12) ends the
  run at the first `authentication_failed` retry instead of waiting minutes of
  retries that end in the same 401. Same code at the prerelease and HEAD.
  Its log line was misnamed, though: the line was read, and it is the line the
  run ended at.
- `-p` placement (BPR-2178 finding 1): Ask in the Agent tab also puts its
  MCP-off options and grant before `-p`, and answers through the wrapper, so
  it cannot be what separates the two. BPR-2178 stays as filed.

## Fix

- `application::claude_user_setting_source` (`"user"`) is the one source a
  Claude launch names. `claude_writing_restrictions` and
  `claude_read_only_restrictions` name it in place of `""`; the Agent tab's
  pull-request checkout launch uses the same constant. `project` and `local`
  stay unread, so the working folder's and a pull request's own `CLAUDE.md`,
  `.claude` settings and `.mcp.json` still never load.
- The failure log names a line the run ended at (a 401 retry, a usage limit,
  a refused tool, a failed turn) as "The line it ended at:", and keeps "First
  line this app couldn't read:" for a line it could not read.
- `ai-agent-boundary.md` and the threat model say what the user's settings add:
  the person's own `CLAUDE.md`, plugins (no tool or MCP server past the init
  check) and permission rules, which can widen a read-only run's approvals as
  a wrapper's already can.

## Owner decision, 2026-11-17

"Keep it." Read-only and tool-free Claude runs read the user's settings
(`--setting-sources user`): the person's own permission rules and their user
`CLAUDE.md` apply to them. This supersedes BPR-1375's `--setting-sources ""`
for those runs; `project` and `local` stay unread. BPR-2242:owner-retry stays
open until the owner retries Draft Tour on a build that holds `73a2a7420`.

## Acceptance

- **BPR-2242:parity** A Claude stand-in that signs in only when the user's
  settings load (as an `apiKeyHelper` there does) answers every harness run:
  Ask in the Agent tab (checkout and own), the assistance first answer, Draft
  Tour both ways, a field draft, Enhance on a stop and a comment, a side
  chat, the whole-tour conversation, a local draft's description, discussion
  and check plan. Before the fix the first tool-free run failed
  `not_signed_in` while the Agent tab passed
  (`application_agent_launch_policy`, `test_every_harness_run_reads_the_users_sign_in`).
- **BPR-2242:words** A failed sign-in's log names the 401 retry as the line it
  ended at (`qt_agent_launch_panel`).
- **BPR-2242:owner-retry** The owner, on a build with this fix, with their
  wrapper agent chosen in the footer picker and a pull request open, clicks
  Draft Tour on the Tour tab and gets a draft. Pending.
