---
schema: "implement-rfc.card.v1"
id: "BPR-1918"
title: "A custom agent profile can opt out of the strict writing and read-only run checks"
status: "Done"
rfcs: ["RFC-BPR-0092"]
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, coordinator chat","related_cards":["BPR-1375","BPR-1891","BPR-1896","BPR-1899","BPR-1900","BPR-1912","BPR-1916"]}
acceptance: [{"criterion": "AC1. With strict checks off, a writing or read-only run whose init reports an extra tool, or that uses a tool outside the list, finishes; the transcript and diagnostics name what happened", "outcome": "pass", "revision": "3ea2736c3", "evidence": {"kind": "command", "detail": "application_agent_launch: test_relaxed_run_checks_report_instead_of_refusing (read-only init with Edit and an MCP server plus an Edit tool_use, and a writing init with Edit plus a tool_use and its result, both finish and answer 'done'; tool_activity items noted relaxed_check_note; check_relaxed counted init/tool/block; tour_transcript_of marks not_stopped; describe/parse round-trip of relaxed=on relaxed_init=1 relaxed_tool=2 relaxed_block=0 relaxed_skill=0) passes in build/dev."}}, {"criterion": "AC2. With strict checks on (the default), behaviour is unchanged and the existing refusal tests pass", "outcome": "pass", "revision": "3ea2736c3", "evidence": {"kind": "command", "detail": "application_agent_launch passes unchanged refusal cases (test_read_only_tour_drafting tool_refused for Edit, Write, WebFetch, extra init tool and MCP server; writing init and thinking tests) and the new strict counterparts in the relaxed test; core_agent_entry, sqlite_agent_setup_storage and qt_agent_setup_view pass; coding-format-check passes."}}, {"criterion": "AC3. The setting is per custom profile, stored through the host, and shown only for custom profiles", "outcome": "pass", "revision": "3ea2736c3", "evidence": {"kind": "command", "detail": "sqlite_agent_setup_storage: a custom entry stored with StrictRunChecks off reads back off after a restart, only for that entry (table agent_entry_relaxed_checks); a popular agent with it off is refused (relaxed_popular_agent). qt_agent_setup_view: the dialog shows the switch on by default for a custom agent, keeps it off when unchecked, and shows none for Claude Code."}}, {"criterion": "AC4. Owner look: an Accept/Reject on the setting's control and wording, in a frozen build", "outcome": "pending", "revision": "3ea2736c3", "evidence": {"kind": "observation", "detail": "Attended session f56d172edaab68eaf4bc91758a400032 (state /Users/jaeger/BPR-attended/BPR-1918-v001-review/state), candidate BPR-1918-strict-run-checks, fingerprint 89bf1d90df719891070c1fd0091ffe2df6812b4a1b545a5962b1b47ce2896375, ready and served; frozen root /Users/jaeger/BPR-attended/BPR-1918-v001.3azwjmgj from 3ea2736c3d856f9097aa79eb0879f04024e571bd, executable sha256 e4660e3a184ca0082dd17f426466a32572bc5e7df4dab97af976ce6a435f5659, launcher --check ATTENDED_PULL_REQUEST_OPENED."}}, {"criterion": "AC5. The documentation gate passes", "outcome": "pass", "revision": "3ea2736c3", "evidence": {"kind": "command", "detail": "python3 scripts/verify_documentation.py --root . prints DOCUMENTATION_OK; tests/scripts/verify_documentation_tests.py 67 tests OK."}}]
---

# BPR-1918: A custom agent profile can opt out of the strict writing and read-only run checks

## Owner (2026-11-06)

"right now I'm only going to run against trusted repos with the wrapper so im
not too worried so long as it works. If we're going to build something strict
which should have an opt out for custom agents at least."

Context: the owner's custom agent runs Claude Code through a managed wrapper
that replaces this app's `--settings` (BPR-1896), and it may widen
permission rules (BPR-1912: `awk` ran). Each strict check that a wrapper
trips has cost a card. The run checks are:

- the writing init checks (tools, MCP);
- the read-only init and tool-use checks (`tool_refused`);
- the text-only answer check;
- BPR-1916's unreferenced-skill check.

## Change

- A per-profile setting on custom agent profiles (Agent settings): **Strict
  run checks**, on by default. With it off, writing and read-only runs for
  that profile don't end on a check that a wrapper's or policy's settings
  can trip:
  - extra tools or MCP servers in init;
  - a tool_use outside the list;
  - non-text blocks in a writing answer;
  - unreferenced skills.
  Everything such a check would have refused is reported instead, in the
  transcript and in diagnostics (`relaxed=on`, with counts, never content).
- The app still passes its restrictive arguments, so a wrapper that honours
  them stays tool-free or read-only. Only this app's own enforcement relaxes.
- Things that stay, whatever the setting:
  - the private, disposable workspace or empty writing folder;
  - never using the reviewer's own checkout;
  - text answers are the only content that becomes tour text;
  - proposals still need acceptance.
- Built-in profiles keep strict checks. Record whether to offer the switch
  there later.
- The setting says plainly what it means in one or two lines (for example,
  "Use only with repositories you trust. The app reports what the agent did
  instead of stopping it."). Load `frontend-design:frontend-design` for the
  control and the wording.
- Threat model: record the opt-out and its residual.

## Acceptance

- AC1. With strict checks off, a writing or read-only run whose init reports
  an extra tool, or that uses a tool outside the list, finishes. The
  transcript and diagnostics name what happened.
- AC2. With strict checks on (the default), behaviour is unchanged. The
  existing refusal tests pass.
- AC3. The setting is per custom profile, stored through the host, and shown
  only for custom profiles.
- AC4. Owner look: an Accept/Reject on the setting's control and wording, in a
  frozen build.
- AC5. The documentation gate passes.

## Implementation (2026-11-06)

- **Switch.** `application::agent_run_checks` (`strict`, `relaxed`) rides
  `agent_invocation_request::RunChecks`, `agent_launch_options::RunChecks`
  and `agent_launch_request::RunChecks` into `agent_protocol_decoder`, which
  passes it to `decode_structured_frame`, `decode_claude_frame` and
  `read_only_tool_use`. The app sets it relaxed only for a tour run of a
  custom entry with `StrictRunChecks` off (`window_agent_chat.cpp`).
- **Relaxed checks.** Writing init with tools or MCP servers, read-only
  init beyond the four tools or with an MCP server, a read-only tool_use
  outside the list, and a non-text block in a writing answer (its tool
  results read past) call `relax()`: a `tool_activity` whose item starts
  `relaxed_check_note`, shown in the transcript as "not stopped, strict run
  checks are off", and a `check_relaxed` event counted by
  `relaxed_check` into `agent_run_result::RelaxedCounts` and the
  `agent.run` line (`relaxed=on relaxed_init=N relaxed_tool=N
  relaxed_block=N relaxed_skill=N`). A missing tool list, a payload outside
  the result and every other malformed frame still end the run; only text
  blocks become output.
- **BPR-1916 hook.** `relaxed_check::skill` exists and is counted. The
  unreferenced-Skill check, when it lands in `decode_claude_frame`, must
  test `relaxed` (that function's local, from `aChecks`) and call
  `relax(events, relaxed_check::skill, "Skill", ...)` instead of refusing.
- **Setting.** `core::agent_entry::StrictRunChecks` (default on), refused
  off for a popular agent (`relaxed_popular_agent`), stored in
  `agent_entry_relaxed_checks` by entry id. The Add/Edit agent dialog shows
  **Strict run checks** with two lines under it, for custom agents only.
  The control and wording were designed with the
  `frontend-design:frontend-design` skill loaded.
- **Built-in profiles.** They keep strict checks. Offering the switch there
  is left for a later card if the owner asks.
- **Threat model.** "Custom agent opt-out of strict run checks (BPR-1918)"
  in `docs/architecture/security-threat-model.md`.

## Parked on human-review (2026-11-06)

AC4 waits on the owner's Accept or Reject in attended session
`f56d172edaab68eaf4bc91758a400032`, state directory
`/Users/jaeger/BPR-attended/BPR-1918-v001-review/state`, host started
detached (`serve --tailscale`). One question: candidate
`BPR-1918-strict-run-checks`. The frozen build is
`/Users/jaeger/BPR-attended/BPR-1918-v001.3azwjmgj`, launched with
`python3 /Users/jaeger/BPR-attended/BPR-1918-v001.3azwjmgj/launch.py`.
On Accept the card moves to Done; on Reject the note says what to change.

The frozen build was exported from 3ea2736c3, before the branch was rebased
onto main 2261fa371 (BPR-1914/1915). The rebase was clean and left the
BPR-1918 change unchanged; it is 58b68530c on the branch. The rebase also
reseals the production-app manifest digest in
`tests/end_to_end/application_review_round_trip_composition_contract.cmake`
for the `window_agent_chat.cpp` edit. That contract and the focused tests
pass on the rebased tree.

## Accepted

The owner accepted v001 on 2026-11-06 (attended review).
Owner note on the look: "Wording talks about tour but is this only tour or agent as well?"
