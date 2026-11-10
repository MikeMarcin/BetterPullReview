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
