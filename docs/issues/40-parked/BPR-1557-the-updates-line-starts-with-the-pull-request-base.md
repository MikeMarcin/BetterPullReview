---
schema: "implement-rfc.card.v1"
id: "BPR-1557"
title: "The Updates line starts with the pull request's base"
status: "Parked"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
parked_on: "human-review"
metadata: {"kind":"design-correction","found_by":"Owner chat, 2026-10-16","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0029","RFC-BPR-0003"],"related_cards":["BPR-0646","BPR-0944","BPR-1340","BPR-1553"],"reservation":"docs/issues/_reservations/BPR-1557--BPR-1557--c5ee30b1bbf9.json","acceptance":["BPR-1557:model","BPR-1557:line","BPR-1557:composed","BPR-1557:live","BPR-1557:gates","BPR-1557:owner"]}
acceptance: [{"criterion": "BPR-1557:model", "outcome": "pass", "revision": "1a76acd0f", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^core_(update_line|epoch_policy)$'", "detail": "core_update_line: places (base, an update's own base, heads), stops (left: base then heads incl. importable; right: heads), steps from any place, one update gives two places. core_epoch_policy test_resolution_names_the_pull_request_base: an exact pair's resolution names the newest live epoch's base after the base moved; Whole PR's left is that base; an imported LatestLive names none. Negative control: a resolution withholding the base for exact pairs fails at 'an exact pair's resolution still names the newest live epoch's base, after the base moved'."}}, {"criterion": "BPR-1557:line", "outcome": "pass", "revision": "1a76acd0f", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_epoch_comparison_scale_(100|125|200)$'", "detail": "test_the_line_starts_with_the_pull_request_base in Light, Dark, High Contrast Light and High Contrast Dark at 1.0/1.25/2.0: one update keeps the handles more than half the line apart; Whole PR puts the left handle on the base (post in the accent inside it, left/right handle sides and the span to the newest update in the accent, at least 60 sRGB units off the ground) and the post is faint once the left handle leaves; drags (left off the base onto the oldest head; right to the start stops on the oldest head), Left back onto the base, Shift+Left never onto it, click/Cmd-click, the base menu (Compare from here, Copy base commit copies the full OID), Alt+1's action, an older page and the accessible name. Negative control: with the published base withheld from the line all three scales fail."}}, {"criterion": "BPR-1557:composed", "outcome": "pass", "revision": "1a76acd0f", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_epoch_update_line_public_path$'", "detail": "Anonymous public-preview fixture through the shipped composition: stage 0 requires the whole-PR pair on the base and the newest update; PUBLIC_PATH_HANDLE_DRAG_STEPS prbase->head0 | head3->head2 | Shift+Right head2->head3; EPOCH_UPDATE_LINE_DRAG_OK:1."}}, {"criterion": "BPR-1557:live", "outcome": "pass", "revision": "fd6c65196", "evidence": {"kind": "command", "command": "QT_QPA_PLATFORM=offscreen build/dev/tests/end_to_end/live_second_account_course --course epoch --root <owner-private mkdtemp> --pull 74", "detail": "MikeMarcin/BPR-Test#74 (feature from main; base branch bpr-1557-base one commit ahead; two force pushes and an ordinary push; closed and both branches deleted afterwards), signed in with the active gh account. First open: left=prbase right=head2, 'Updates line: from the pull request base 75e2349 to update 3 of 3'; base 75e23497520d = GitHub base.sha, merge base e2a44aa8fef7 (compare API). Right drag imported the oldest force push (right=head0), left drag onto it, Left stepped back to prbase. X-RateLimit-Remaining 4996 before, 4926 after."}}, {"criterion": "BPR-1557:gates", "outcome": "pass", "revision": "1a76acd0f", "evidence": {"kind": "command", "command": "ctest --preset dev --output-on-failure (fd6c65196); focused rows, format, tidy and documentation on 1a76acd0f", "detail": "Full run on fd6c65196 (lean -g0 dev build, load average near 38 from two other agents' full runs): 731/740 passed; the 9 failures (application_diff_host 10 ms stop budget, qt_composed_long_thread_scale_100, release_update_native_https, application_large_review_active_work and _pathological input budgets, large_review_replay_contract timeout, application_agent_materialization_crash_recovery, application_review_submission_composition_public_path and _scale_125) all passed rerun in isolation on the same binaries (large_review_active_work twice). application_composition_contract passed in the full run after the round-trip reseal (production app source digest cae8d253...). On 1a76acd0f: core_update_line, core_epoch_policy, qt_epoch_comparison x3, application_epoch_host, workbench and round-trip composition contracts, anonymous preview, ordinary-update-recreated and update-line public paths pass; clang-format 22.1.8 --Werror clean; clang-tidy clean on update_line.cpp, epoch_line.cpp, epoch_comparison_control.cpp, epoch_transition.cpp, update_line_tests.cpp; DOCUMENTATION_OK and verify_documentation_tests OK on a trial merge with main 5df5bf89f (the branch alone trips NEXT_ID high water from later allocations on main)."}}]
---

# BPR-1557: The Updates line starts with the pull request's base

## Owner notes

Owner, chat, 2026-10-16, after [[BPR-1553]] made the handles selectable
again, on the first open's whole-PR pair, which drew both handles stacked on
the newest update at the line's right end: "Surely the left side should be at
whatever the PR's merge base is, no?"

## What exists

- RFC-BPR-0028's Updates line has one node per update and draws the two
  compared anchors as bracket handles. An anchor is an epoch's base or head.
- Whole PR resolves in the core to the latest live epoch's base and head
  (`select_pair`). That base is what GitHub records as the pull request's
  base (`baseRefOid`, the REST `base.sha`) when this install observed the
  update. `docs/architecture/git-and-epoch-model.md` names it the `Branch
  base` and says it is not a computed merge base; RFC-BPR-0029 Decision 4
  keeps any computed merge base separate and never shows it as a base.
  Whether Whole PR should compare from the merge base instead, as GitHub's
  Files changed does, is [[BPR-0944]]'s open, owner-deferred decision.
- The line had no place for that base. It drew an epoch's base seven pixels
  left of that epoch's node, so the whole-PR pair put the left handle beside
  the right one on the newest update; the left handle's first stop was the
  oldest update's own base.

## Design

- **The core names the base.** Every comparison resolution publishes
  `PullRequestBase`, the anchor Whole PR compares from, whatever is selected,
  from the one function Whole PR's left side also takes (`pull_request_base`):
  today the newest live epoch's base. When the base branch moved or a force push
  rebased the branch, the newest live epoch has a new base and the anchor
  follows it. An imported epoch is never it.
- **The core decides the line's places** (`bpr/core/update_line.hpp`):
  where a compared anchor sits (the base, an update's own base, an update's
  head), which stops each handle can take (left: the base, then every head
  it can take, an importable update GitHub recorded included; right: the
  heads), and where a step goes, in line order from wherever the handle is.
- **The line presents it.** The base is a post at the track's start, not a
  round node, because it is where the line starts rather than an update;
  "Base" under it. The updates share the rest of the track evenly, so a
  single update sits at the far end, apart from the base. The left handle
  holds the post (drawn through in the accent), the accent span runs to the
  right handle. With the left handle elsewhere the post is in the faint ink.
- **Older bases.** An older update's own base (Show this update alone) keeps
  its place just before that update and its identity; the node's tooltip
  still names its observed base. The base at the start is always the
  current one.
- **Gestures.** Click the base or its label, or its menu's Compare from
  here, to compare from it; Copy base commit; Left and Right reach it as the
  left handle's first stop; a drag of the left handle off it onto an update
  compares that update to the head (an interdiff); the right handle never
  takes it; Alt+1 returns to it through the core. An older history page keeps
  it at the line's start.
- **Words.** Tooltip: "Base of the pull request: commit … (seen here …).
  Whole PR compares from here.", which claims nothing about how the base was
  chosen. The
  line's accessible name says where both handles are ("Updates line: from the
  pull request base 1a2b3c4 to update 3 of 3, …"). The label is "Base", not
  "Merge base", because the anchor is the base GitHub records, not a computed
  merge base ([[BPR-0944]]).

## Acceptance

- `BPR-1557:model`: `core_update_line` and `core_epoch_policy` show the
  places, stops and steps, the base named with an exact pair's resolution
  after the base moved and never for an imported epoch; a resolution that
  withholds the base fails `core_epoch_policy`.
- `BPR-1557:line`: `qt_epoch_comparison_scale_{100,125,200}` in Light, Dark,
  High Contrast Light and High Contrast Dark: one update keeps the handles
  apart; the whole PR puts the left handle on the base, which is painted in
  the accent inside it with the span to the newest update, and faint when the
  left handle leaves; drag, click, Cmd-click, keys, the base's menu, Alt+1's
  action, an older page and the accessible name behave as designed. With the
  base withheld from the line the course fails.
- `BPR-1557:composed`: `application_epoch_update_line_public_path` opens the
  anonymous public-preview fixture through the shipped composition and finds
  the whole-PR pair on the base and the newest update before dragging.
- `BPR-1557:live`: `test_live_second_account_course --course epoch` on a
  BPR-Test pull request, signed in, shows the base and the handles, the drags
  and Left stepping back onto the base.
- `BPR-1557:gates`: one full `ctest --preset dev`, clang-format 22, tidy on
  the changed files, the documentation gate and the composition contracts.
- `BPR-1557:owner`: attended, Accept or Reject: the Updates line starts with
  the left handle on the pull request's base and the right on the newest
  update.

## How the comparison's base is chosen today (for the base-semantics work)

Found while placing the anchor, 2026-10-16. The coordinator then gave the
Whole PR base and merge-base semantics to a separate implementer (the owner
reported a wrong default diff on a pull request that merged main several
times); this card stays presentational.

- **Read.** Each live read takes GitHub's own base commit for the pull
  request: GraphQL `baseRefOid` / REST `base.sha`
  (`src/adapters/github/src/github_paged_read.cpp`, the `BaseOid` of the
  pull-request snapshot), and `github_window_read_host.cpp` hands it over as
  the accepted revision's `Base` with `BaseRef` (`refs/heads/<baseRefName>`).
  That is the base branch's tip as GitHub last recorded it for the pull
  request, not a merge base and not the first epoch's parent.
- **Captured per live epoch.** The catalogue records `Base` with each live
  epoch when it is observed (`review_epoch_summary::Base`); it is not
  recomputed later. A new head or a moved base appends a live epoch whose
  cause says so (`base_oid_changed`, `base_branch_changed`).
- **Whole PR.** The core resolves Whole PR as the latest live epoch's
  `epoch_base` anchor to its `epoch_head` (`select_pair` in
  `src/core/src/epoch_transition.cpp`); after this card both that and the
  line's base come from one function there, `pull_request_base`, so a change
  of basis moves the line's base with it. The selected diff then compares
  those two exact commits, two-dot; nothing substitutes a merge base
  (`docs/architecture/git-and-epoch-model.md`, "The friendly `Branch base`
  label resolves to the latest live epoch's accepted base-tip anchor. It is
  not a computed merge base").
- **Merge base exists only for comment numbering.** The host derives the
  unique merge base of the head and the base tip through the cache
  (`derive_comparison_base`, `epoch_application_host.cpp`, the
  `github_base_numbering` request) only to decide whether a base-side line can
  take a GitHub `LEFT` comment; it is unavailable on a sparse review.
  RFC-BPR-0029 Decision 4 keeps any computed merge base labelled as a derived
  comparison base, never as the historical base.
- **Measured.** On `MikeMarcin/BPR-Test#74` (feature branched from `main`,
  base branch `bpr-1557-base` one commit ahead): GitHub's `base.sha` was
  `75e23497520d`, the compare API's merge base `e2a44aa8fef7`; the shipped
  composition's Whole PR compared from `75e23497520d`. When the base has moved
  past the merge base, a two-dot diff from the tip shows the base branch's own
  changes reversed, which is the owner-deferred anomaly [[BPR-0944]]
  describes.

## Boundaries

The anchor is presentational: it shows whatever base the core's Whole PR
uses, and its tooltip claims nothing about how that base was chosen. Its
position and label follow the base-semantics fix.

The core decides, the host performs, Qt presents. No host, provider, Git or
storage behaviour changes: the comparison still compares exactly the anchors
it did. Whole PR's comparison basis (base tip or merge base) is
[[BPR-0944]]'s and unchanged here.

## Attended review v001

- Session `d62ac57667ce277091d8684de8c34f0b`, fresh state
  `/Users/jaeger/.mask/bpr-1557-base-anchor-v001-20261016/attended-review`,
  checkout `.claude/worktrees/bpr-1557-base-anchor`, event cursor 3.
- Candidate `BPR-1557-base-anchor-v001` (revision `frozen-1a76acd0f-v001`,
  fingerprint `bc56762e0c12…`), criterion `BPR-1557:owner`. One Accept or
  Reject: the Updates line starts with the left handle on the pull request's
  base and the right on the newest update. Inline 2x offscreen renders, before
  (main `15f0bff17`) and after, Light and Dark, one update and three.
- Frozen root `/Users/jaeger/bpr1557-base-anchor-v001.jaffeggk` from
  `1a76acd0f` (fixture `conventional-comments-v1`, file
  `src/engine/renderer.cpp`, `--next-push`); executable SHA-256
  `936f9aac5143e84cb9d84b21c26aee3674c33fbf43517525cbb04a5cba7867af`; its own
  check showed the file on head `b20cb443…` and the pushed head `cd24836c…`.
- Launch: `python3 /Users/jaeger/bpr1557-base-anchor-v001.jaffeggk/launch.py`
  (`--next-push` for the second update).

## Owner verdict on v001, 2026-10-16

Session `d62ac57667ce277091d8684de8c34f0b`, handed back at event 5; cursor 3
to 5 read. `BPR-1557-base-anchor-v001` (fingerprint `bc56762e0c12…`),
criterion `BPR-1557:owner`: **Reject** (decision
`60120f22-bc68-467a-8aa0-08028b69c28b`, 19:52:15Z). Note, verbatim: "I find
these handles quite difficult to see."

Diagnosis: the line painted its handles and span in the palette's Highlight,
a selection colour made to carry text. Measured against the line's ground it
stands 2.11:1 (Light, `#7fb0e8` on `#f6f7f9`), 2.06:1 (Dark), 1.23:1 (High
Contrast Light, `#ffe877` on white) and 1.59:1 (High Contrast Dark); the v001
handles were 7x13 hollow outlines in that colour.

## v002: brackets that stand off the line

- Solid brackets, `[` on the left and `]` on the right, 15 pixels tall with a
  5-pixel stem and 5-pixel arms reaching round the span, in the accent with
  an outline: the accent moved towards the ink only as far as it must to
  stand 4.5:1 off the ground and 3:1 off the track (7.2:1 and 3.1:1 in Light).
  Each sits on a ground-coloured halo; a held node is its bracket's stem.
- A filled 6-pixel span in the accent with outlined edges, on the half pixel
  so the edges are crisp at every scale.
- The base: a solid 4x13 post in the ink (13.8:1 to 21:1 off the ground).
- Hover fills a bracket with its outline colour under a sideways-arrows
  pointer; the hover clears on leaving and when a drag ends away from it.
  With the keyboard on the line the handle the arrows move wears a ring.
- No stack: Whole PR's handles are the line's two ends; an update's own base
  sits 18 pixels before its node, so Show this update alone draws two
  brackets that never touch; an identical pair is drawn either side of its
  update.
- Behaviour, keys, menus, accessibility and wording are v001's; the label
  stays "Base" and the tooltip neutral until BPR-1560 moves the base to the
  merge base through `pull_request_base`.
