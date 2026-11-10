# v0-5-0-prerelease-targets-v1: handoff

Mandate `v0-5-0-prerelease-targets-r1` (owner, 2026-11-10: "Implement the
pack … Start with the dependable-session milestone and follow the plan's
order"). Strategy: coordinator-implementer, preserved in
[selection.json](selection.json). Lane `claude/v0-5-0-prerelease-targets-52265d`,
base and RFC_BASE_SHA `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`. Nothing pushed.

## Commits

| Commit | What |
|---|---|
| `5d8210443` | Pack import: RFC-BPR-0098..0112, BPR-1989..2035, plan adopted ([LIVE_IMPORT](../../../rfcs/assets/v0-5-0-prerelease-targets-v1.LIVE_IMPORT.md)) |
| `1600de86c`, `df50b29dd` | Claim BPR-2010, BPR-2011 |
| `357df825d` | Stated, bounded, logged submissions; per-host lanes; RFC cause amended; course and unit tests |
| `31f900910` | Open resets the surface at dispatch; production app source pin resealed. Product candidate |
| `f0295294d` | BPR-2011 Done (AC06, AC07) |
| `6bf259a2d` | BPR-2010 parked `human-review` (AC01–AC05 pass, AC08 pending) |

## State

`dependable-session` is engineered; its first exit (cause recorded; no
submission can wait or vanish without a visible, bounded, logged condition)
and second exit (another pull request's effects do not block; every open shows
Comments with no tour side chat; filters and layout kept) have passing
public-path evidence ([verification](../../../verification/RFC-BPR-0105.md)).
The milestone is not accepted while RFC-BPR-0105 AC08 (the owner approves on
BPR-Test from a frozen build after a five-PR session with agent and tour use)
is pending. BPR-Test #151, authored by the test account, is open for it.

AC08 has no prepared session: `scripts/freeze_attended_build.py` freezes only
tests-on builds, and a tests-on composition refuses live review submission
(`compose_review_submission` is read-only without a fixture). A live frozen
build needs either a tests-off freeze path (a `scripts/` change outside this
lane's authority) or the next prerelease build. That choice is the
coordinator's or the owner's.

## Commands and helpers

- Course: `ctest --preset dev -R application_dependable_session_public_path`.
- Baseline recording: the same course on a detached `dc5453afa` checkout with
  only the course and the shared-lane hold added; that checkout is removed.
- `milestone_checkpoint.py bind` could not run: plan discovery scans every
  live plan and `docs/rfcs/plans/release-v041/milestone-plan.json` is invalid
  under the installed helper (`unknown fields ['amendments']`). Not this
  lane's file; the binding is recorded here by hand.
- `.agents/skills/implement-rfc` in this worktree is an ignored symlink to the
  installed skill, made so the repository's board helpers and documentation
  gate run here.
- The ID allocator refuses non-`main` branches; under the coordinator's
  instruction it ran once against the main checkout, whose counter and receipt
  were moved to this lane and the main checkout restored (see LIVE_IMPORT).

## Next

Next eligible milestones in plan order: `agent-tab-asks`, `tour-publishing`
and `home-next-move` (independent of this one and of each other). BPR-2012
(RFC-BPR-0105 closeout) belongs to `accepted-scope` and waits for AC08.

## agent-tab-asks (2026-11-10)

Engineered on the same lane; nothing pushed. Frontend-design was loaded before
the Qt work. Records: [RFC-BPR-0101](../../../verification/RFC-BPR-0101.md),
[RFC-BPR-0102](../../../verification/RFC-BPR-0102.md).

| Commit | What |
|---|---|
| `29f144959`, `0a126de53`, `259a58e57`, `044bf7a8f`, `23ee47ccc` | Ready the four cards; claim BPR-2002, 1998, 1999, 2000 |
| `dd3ea5ba9` | Question subject and attachment chip, Escape clears the selection, the core reference recogniser |
| `574958186` | NEXT_ID to BPR-2038: `main` reserved BPR-2036 and 2037 after the import, so the lane's counter rewound the high water and every board helper refused |
| `2c4372be5` | Return sends, cost per turn and session, Agent accent role and the run rule, live-claude course, RFC amendments (0012, 0034, 0101), ledgers, app pin resealed |
| `073cca272` | Tour accent tests follow the rule. Product candidate |
| `f6d06a4fa`, `3a4dd269d`, `b5bb93c46` | BPR-2002, BPR-1998, BPR-1999 Done |
| `c1d445720` | BPR-2000 parked `human-review` (AC07, AC09 pass; AC08 pending) |

State: both exits have passing public-path evidence (fixture course
`application_agent_chat_subject_contract`, Qt window courses, and the opt-in
installed-Claude course `live-claude`). The milestone is not accepted while
RFC-BPR-0101 AC08 (the owner's look at the Agent tab's accent and footers on a
frozen build) is pending.

Established facts: installed Claude Code 2.1.319 reports `total_cost_usd` as
the resumed session's running total; BPR accounts a resumed turn as the
difference, recorded at launch (`ResumedNative`).

Integration pass at `073cca272` (113 focused rows): only the two baseline
failures recorded under RFC-BPR-0105 remain
(`application_agent_chat_custom_agent_contract`,
`application_agent_finding_codex_found_contract`). The documentation gate's only
finding before `574958186` was the counter rewind.

Decisions made here for the coordinator or owner to see: the "selected-lines
side chat" is the panel the Agent tab presents, so its rules arrive through the
Agent tab; opening a chat, showing a run and Set up the agent are ordinary
controls now (owner look BPR-1914 v004 had chat toggles in the accent); Ask the
agent about these lines and Review Cycles' "Send to Agent…" only open a step,
so the accent is on the step's Send; unchanged-file references are plain text
because there is no unchanged-file view.

Next: AC08 attended look (see the coordinator report for its session), then
`agent-work-visible` (depends on this milestone), `tour-publishing`,
`home-next-move`. Closeouts BPR-2001 and BPR-2003 belong to `accepted-scope`.

AC08 attended look prepared: frozen build `/Users/jaeger/agent-tab-asks-v001.qgjq6axg`
(from `c1d445720`, executable SHA-256 `e02d77336f22…`, launcher `--check` passed on
README.md of nested-folders#21), session `fd2f7fdb4d5b89ca67e9793084a68d04`, state
`/Users/jaeger/.attended-review-state/agent-tab-asks-2026-11-10/state`, served over
Tailscale, event cursor 6, two candidates ready: `BPR-2000-agent-accent`
(fingerprint `40b96d3f8f1f…`) and `BPR-1999-agent-footers` (`10e13f7c7435…`).
The URL stays in the session state. Remove the root with
`scripts/freeze_attended_build.py remove` once decided.

## tour-publishing (2026-11-10)

Engineered on the same lane; nothing pushed. Frontend-design was loaded before
the Qt work. Record: [RFC-BPR-0099](../../../verification/RFC-BPR-0099.md).

| Commit | What |
|---|---|
| `c16cb7f7f`, `2b0140564` | Ready and claim BPR-1991 |
| `3d97ea548` | Three bands and the head (core `decide_tour_head`, shared with Overview); the plan chooses the form; steps from the plan's writes with per-write progress from the host; error-role banners; Overview Markdown, stop rows and head; courses, unit tests, resealed app pin. Product candidate |
| `d72b337ae` | RFC-BPR-0099 implementation notes and verification record |
| `2a8055583` | BPR-1991 Done (AC01, AC02) |
| `6ac50280e`, `74921a104`, `0903160f4` | Ready and claim BPR-1992, BPR-1993 (their product edits were already in `3d97ea548`: both depended on BPR-1991 being Done) |
| `9097343a8` | BPR-1993 Done (AC07) |
| `dceb3c333` | BPR-1992 parked `human-review` (AC03 to AC06 pass, AC08 pending) |
| `bce0d0eba` | Tour page unscrolled; route area out of the focus order. Look candidate |

State: exit 1 (bands, head) and exit 3 (Overview) have passing public-path
evidence. Exit 2's form, steps and banners pass on fixture courses; its live
mixed tour on BPR-Test (RFC-BPR-0099 AC08) is not run: a tests-on build holds
no publication grant, `freeze_attended_build.py` freezes only tests-on builds,
and `bpr tour publish` needs a person to type `publish`. It needs a tests-off
build the owner runs, the same route choice RFC-BPR-0105 AC08 waits on.

AC09 attended look prepared: frozen build `/Users/jaeger/tour-publishing-v001._2twcw6p`
(from `bce0d0eba`, executable SHA-256 `33c8788efc01…`, launcher `--check`
passed on README.md of nested-folders-v1), session
`5127270e92b1b77ceae8f9df8356c1f6`, state
`/Users/jaeger/.attended-review-state/tour-publishing-2026-11-10/state`, served
over Tailscale, event cursor 12, four candidates ready:
`BPR-1991-tour-tab-head`, `BPR-1992-publication-running`,
`BPR-1992-publication-stopped`, `BPR-1993-overview-tour`. The URL stays in the
session state. Remove the root with `scripts/freeze_attended_build.py remove`
once decided.

Runs: 74 focused tour, overview, staged-review and window tests; all fourteen
publication courses together at `-j6`; boundary and manifest tests; app source
contracts; documentation gate. Baseline failure kept:
`application_review_tour_cold_import_public_path` (also on `60d427a08`). The
shadowed local reported in `agent_reference.cpp` did not reproduce under
the build's `-Wshadow` or clang-tidy 22; left as is.

Disk: a scratch baseline build filled the shared disk for a while (since
removed). Builds by other agents during that window may have failed with
`No space left on device`.

Next: `home-next-move` (independent). `tour-stops` follows this milestone;
BPR-1994 (RFC-BPR-0099 closeout) belongs to `accepted-scope` and waits for
AC08 and AC09.

