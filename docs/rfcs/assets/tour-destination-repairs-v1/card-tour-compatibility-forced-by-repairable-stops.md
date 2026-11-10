---
schema: "implement-rfc.card.v1"
id: "BPR-XXXX"
title: "A tour with one stale-head stop and one stop a line outside its hunk can only publish as compatibility comments, although both stops can be fixed and the other eight are valid line comments"
status: "Backlog"
rfcs: ["RFC-BPR-0041"]
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"enhancement","found_by":"owner, 2026-11-09: a 10-stop tour on a GitHub Enterprise Server 3.20.8 pull request offered only 'This route needs visible compatibility comments.' The owner chose compatibility, the whole tour went out as conversation comments, and it then had to be rebuilt by hand as one review","related_cards":["BPR-1715","BPR-1735","BPR-1737"],"related_rfcs":["RFC-BPR-0022","RFC-BPR-0041","RFC-BPR-0042"]}
acceptance: []
---

# BPR-XXXX: A tour with one stale-head stop and one stop a line outside its hunk can only publish as compatibility comments, although both stops can be fixed and the other eight are valid line comments

Allocate the real ID with `python3 scripts/allocate_issue_id.py` and replace
`BPR-XXXX` (file name, front matter, heading).

## Observed

The owner, 2026-11-09, on a GitHub Enterprise Server 3.20.8 pull request. A
10-stop tour was ready to publish. The tour editor reported:

```text
This route needs visible compatibility comments. Keep one review by relocating
destinations, or explicitly choose compatibility comments.
```

The editor did not say which stops caused this or what would fix them. The
owner chose compatibility comments. The overview and all 10 stops went out as
top-level conversation comments, each opening with a blob permalink. None was
attached to the diff, unlike the owner's earlier tours, which publish as one
review whose line comments sit in Files changed.

Rebuilding the tour as one review by hand afterwards needed only two changes:

1. **Stop 1** (a Markdown documentation range) was recorded against an
   earlier head of the pull request. The lines it covers are byte-identical at
   the current head, and the whole range lies inside a diff hunk there.
2. **Stop 9** (a single line in a C++ source file) was one line outside its
   hunk: the hunk's first line was one below it, four lines above the first
   changed line.

After stop 1 was re-pinned to the head and stop 9 moved down to the changed
line, all 10 stops were accepted as line comments of one COMMENT review.

## Cause

1. `plan_tour_destination` (`src/core/src/review_tour_destination.cpp`)
   classifies each stop by `publication_observation`'s `Subject`. Stop 1's
   source commit is not the head, so it is `tour_subject::historical`, and it
   falls through to `conversation_permalink` with `JoinsPendingReview` false.
   Stop 9 is `current_off_hunk`. Without a supported pending off-hunk or file
   thread, it also cannot join the review.
2. `assess_tour_publication` treats the tour as all-or-nothing. If any
   destination has `RequiresCompatibility` or does not join the pending
   review, the whole tour becomes `compatibility_choice`. In compatibility
   mode, every destination must be `conversation_permalink`. No mixed outcome
   exists.
3. `tour_publication_plan.cpp` turns `compatibility_choice` into the single
   generic status sentence above. The per-stop reasons
   (`compatibility_required`, `destination_unsupported`) are computed, but
   they are not shown as "stop N: why, and how to fix it".
4. A nearest-hunk mover already exists. `relocate_tour_stop`
   (`src/core/src/tour_agent_session.cpp`) clamps a stop into its file's
   closest hunk, and `tour_agent_panel` offers it as a button. It is wired
   only for pending agent proposals (`relocate_tour_proposal`), not for
   accepted tour stops at publication time.
5. The planner never asks whether a historical stop's lines are unchanged
   at the head. When they are, the stop could be re-pinned exactly and
   publish as a native line comment.

RFC-BPR-0041 ("Compatibility publication") already calls for two explicit
outcomes: **Keep single-review publication** by deliberately resolving,
removing or relocating the incompatible destinations, or **Publish with
compatibility comments**. The second is implemented. The first has no tooling
beyond the sentence, so in practice compatibility is the only path offered.

## Probe (2026-11-09)

The owner ran this on their own pull request with pending reviews only, each
deleted immediately and never submitted. The commit was the pull request's
head. The file's first changed line in that hunk was line `L`.

| Line | Distance from change | Result |
|---|---|---|
| `L` | on the change | 200, pending review created |
| `L-3` | 3 lines of context | 200, pending review created |
| `L-4` | 4 lines | 422 |
| `L-13` | 13 lines | 422 |

Both 422 responses carried
`"Pull request review thread line must be part of the diff and Pull request review thread diff hunk can't be blank"`
with `X-GitHub-Enterprise-Version: 3.20.8`.

- This answers [[BPR-1737]] AC2 for that host: a line comment is accepted up
  to three lines from a change and refused at four, as on GitHub.com.
- Native off-hunk line comments are not available through the REST API on
  this host. [[BPR-1737]] AC1 (file comments) was not probed, because a
  file comment cannot be created as a private pending item.

## Change

### 1. Explain the choice per stop

When the readiness is `compatibility_choice`, replace the generic sentence
with a list of the stops that force it. For each stop, show its number,
title, a plain reason and the available fixes, for example:

```text
Stop 1 "…" was written against an earlier head. Its lines are unchanged at the
current head.                                         [Re-pin to current head]
Stop 9 "…" is 4 lines from the nearest change; this host only accepts lines
within 3.                       [Move to nearest change]  [Publish as file comment]
```

Each reason comes from the stop's destination plan, not from text matching.
Selecting a stop's reason scrolls the editor to that stop.

### 2. Offer one-click repairs before publication

- **Re-pin to current head.** For a `historical` stop whose source lines are
  byte-identical at the head (exact mapping through the existing tour mapping
  logic), move its source to the head commit and same lines. Re-run the
  destination plan. Offer this only when the mapping is exact. A changed or
  ambiguous range keeps today's behavior and says why.
- **Move to nearest change.** For a `current_off_hunk` stop, reuse
  `relocate_tour_stop`'s nearest-hunk clamp on the accepted stop. Preview the
  new line range and its source text before applying it, and keep the
  explanation unchanged. Use the hunk bounds GitHub accepts (three context
  lines, per the probe), not only the changed lines.
- **Publish as file comment.** Offer it where the scope's off-hunk or file
  thread fact allows it ([[BPR-1735]]).
- Each repair is an ordinary, undoable tour edit. It needs the stop's
  explanation to be reaffirmed only if the stop's text refers to line
  numbers that changed. Otherwise the stop stays ready.

### 3. Allow a mixed tour (needs an RFC-BPR-0041 amendment)

Add a third outcome for stops that cannot be repaired: one COMMENT review for
every stop that can be a line or file comment, plus individual conversation
comments, each opening with a permalink, for the rest.

- The overview stays the review body and lists every stop in order, linking
  `files#r<id>` for review stops and `#issuecomment-<id>` for conversation
  stops.
- Previous and Next links cross between the two kinds in tour order.
- The staged-publication plan (RFC-BPR-0041) and published-tour maintenance
  (RFC-BPR-0042) record both kinds under one tour identity, so update,
  reconciliation and partial-publication recovery cover them.
- Preview the non-atomic sequence before the first visible write, as
  compatibility mode does today, and say which stops will not appear in the
  diff.

Rank the outcomes: repair first, mixed second, compatibility last. Never pick
any of them silently.

### 4. Tests

- `review_tour_destination` core tests: a tour with one historical-exact stop
  and one off-hunk stop reports both stops with their fix kinds. After both
  repairs it assesses as `single_review`. A historical stop whose lines
  changed offers no re-pin.
- `tour_agent_session` / tour core tests: the accepted-stop relocation clamps
  into the nearest hunk including three context lines, keeps the stop's
  length where the hunk allows, and is undoable.
- Staged publication tests: a mixed plan creates one pending review with the
  admitted stops, then the conversation stops, wires cross-kind navigation
  and the overview, and recovers from an interruption at each step without
  reposting.
- A composed Qt tour-editor course: the per-stop list appears, the repair
  buttons apply and re-plan, and a repaired tour publishes as one review.

### 5. Documentation

- RFC-BPR-0041 "Compatibility publication": the per-stop repair step and the
  mixed outcome.
- The tour user guide: why a stop cannot join the review (stale head, more
  than three lines from a change) and how to fix it.
- [[BPR-1737]]: record AC2 as answered by the probe above. AC1 stays open.

## Acceptance

- AC1. A tour that would be `compatibility_choice` names every stop that
  causes it, with a specific reason and its available fixes. The generic
  sentence no longer appears alone.
- AC2. A historical stop whose lines are unchanged at the head can be
  re-pinned in one action, after which it plans as a native line comment.
- AC3. An off-hunk stop can be moved to the nearest commentable range in one
  previewed, undoable action, after which it plans as a native line comment.
- AC4. A tour whose blocking stops are all repaired publishes as one COMMENT
  review with every stop as a line comment, with no visible compatibility
  write.
- AC5. With the RFC amendment accepted, a tour with stops that cannot be
  repaired can publish as one review plus conversation comments for only
  those stops, with a complete overview and working cross-kind navigation,
  and RFC-BPR-0042 maintenance can later update it.
- AC6. Compatibility comments remain available as an explicit last choice.
- AC7. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Out of scope

- Native off-hunk line comments. The host refuses them (see Probe).
- Re-anchoring a stop whose source lines changed between its commit and the
  head. It still needs the author's judgment.
- Converting an already-published compatibility tour into a review. The
  owner did that by hand. A tool for it would be a separate card.
