---
id: BPR-0413
title: Render the deferred conversation-view mockups the owner asked for
status: In Progress
rfc: RFC-BPR-0005
depends_on: [BPR-0191]
---

# Render the deferred conversation-view mockups the owner asked for

[[BPR-0211]] and [[BPR-0212]] both record the same operator direction, verbatim,
on 2026-08-20:

> I would like to see mockups before deciding.

Both cards then draw the same conclusion in the same words — "the disposition
changes from *waiting on a decision* to *waiting on a deliverable that this
session owes*" — and both then left `parked_on: awaiting-product-decision`
unchanged and stayed in `40-parked/`.

That is the drift the board README warns about in the paragraph directly above
the frontier table: a card whose *next* action is unattended does not belong in
`40-parked/`, which is how [[BPR-0256]] came back to an active lane. Nothing was
waiting on the owner. A deliverable was owed and nobody was building it.

This card owns the deliverable. It is unattended work, executable from a macOS
session, and it is what the two parked cards are actually waiting on.

## The board move this forces

[[BPR-0211]] and [[BPR-0212]] now depend on this card, so they move to
`10-backlog/` and drop `parked_on`. That is not bookkeeping preference: it is
forced. `scripts/verify_documentation.py` fails a card outside
`10-backlog/`/`50-blocked/` that depends on a card outside `60-done/`, because
`40-parked/` is a frontier-root lane and a parked card with an unmet dependency
is a contradiction. They return to `40-parked/` when this card is done and the
only thing left is the owner reading the page.

## What the mockups must be faithful to

The cards require a design "this codebase could actually reach, not an
illustration of a design in general". Every fact below was read from live source
for this card, and the mockups are built from these and nothing else.

**The projection.** `github_conversation_projection`
(`src/application/include/bpr/application/github_projection.hpp:124-139`) carries
`Id`, `ParentId`, `Kind`, `AuthorLogin`, `Body`, `Path`, `Line`, `ReviewState`,
`Resolved`, `Outdated`, `Deleted`. [[BPR-0212]]'s central claim is confirmed
exactly: there is **no revision identity of any kind** on this record — no commit
OID, no epoch, no ordinal.

**The tree is two levels deep, by construction.**
`src/application/src/github_projection_mapping.cpp` sets `ParentId` in four
places. Three are `std::nullopt` (lines 148, 162, 192) and the fourth, line 175,
is `.ParentId = threadId` for every comment under a thread. So the "forest of
trees with a single optional parent" is real as a type and flat as populated.

**The reply edge exists one layer up and is discarded at projection time.**
`github_read_model.hpp:233` carries `std::optional<github_comment_id> ReplyTo`
per comment; the read contract requests and validates `replyTo`
(`src/adapters/github/src/github_read_contract.cpp:35,563,1259-1279`). At
`github_projection_mapping.cpp:175` that edge is in scope on `comment` and the
thread root is written instead. This is the single most decision-relevant fact
on this card and it is why the two alternatives are not equally expensive.

**What the drawer renders today.** `github_conversation_delegate.cpp:68-144`
paints three stacked runs per row: a heading (`Review thread`,
`Reply by <author>`, `Review comment by <author>`,
`Pull request comment by <author>`), the body, and a facts line joining
`Path:Line`, review state, `Open`/`Resolved`, `Outdated`, and `Deleted` with
`"  ·  "`. `github_conversation_panel::set_anchor_file` filters the forest to the
current file, keeping whole threads or none. There are no lane rails and no
git-graph rendering; the card text that promised them was narrowed by the owner
in [[BPR-0191]] to mean filtering by thread.

**The epoch axis.** `epoch_history_model.cpp:132-137` labels every anchor
`Epoch %1 · %2 — %3 — %4` — ordinal, `Base`/`Head`, primary cause
(`Author upload`, `Force-push/rebase`, `Base update`, …), availability
(`Available`, `Pruned`, `Cache deleted`, …). `epoch_comparison_control` supplies
two independent selectors plus free-OID editors and the named-comparison
actions. The mockup uses these exact strings rather than inventing labels.

**The palette.** `appearance_profile.cpp:76-103`, BetterPullReview Light:
`application_surface #F6F7F9`, `application_text #1F2328`, `code_background
#FFFFFF`, `code_gutter_background #F0F2F5`, `code_line_number_text #5B6570`,
`diff_added_background #E6FFEC`, `diff_removed_background #FFEBE9`,
`diff_group_boundary_rule #8C8C8C`, `state_unread #0969DA`, `state_accepted
#1A7F37`, `state_defect #CF222E`, `state_stale #5B6570`. No colour outside the
46-role vocabulary appears in a mockup.

## The honest asymmetry the owner should see

This is the finding that makes the question answerable, and it was not known
when the cards were written.

| Alternative | What it needs | Cost |
|---|---|---|
| A — whole-review conversation | nothing; the same projection, unfiltered and grouped by file | a view |
| B — Treeverse-style deep thread | `ParentId` carrying `ReplyTo` instead of the thread root, at one line | a view plus one projection line |
| [[BPR-0212]] — conversation × revision | a datum that exists nowhere: per-comment revision identity | a model and fetch question, correctly RFC-scale |

## What no mockup can honestly show

No recorded conversation in this repository is deep or wide enough to exercise
alternative B. Verified for this card: `directxmath-336/recording.json` has
top-level keys `commits, files, pull_request, recorded_from, recorded_on,
repository` and no conversation at all, which is what [[BPR-0211]] already
suspected; `uiforetw-74` is the same shape. The only conversation content in the
tree is the hand-written GraphQL contract fixture
`tests/fixtures/github/contracts/v1/github-com/responses/review_threads.json` —
three threads of one, two, and one comments, with exactly one `replyTo` edge, so
maximum observed depth is one reply.

So the deep-thread mockup is drawn against a **synthesized** conversation, and
the page says so in the subject text rather than in a footnote. That is not a
defect of the mockup: it restates [[BPR-0211]]'s own precondition — "a
conversation deep or wide enough to be a problem is recorded" — which is still
unmet, and the owner should decide knowing it.

## Required final state

- Four rendered mockups: the shipped drawer as it is today, alternative A,
  alternative B, and the [[BPR-0212]] revision join.
- Checked-in SVG sources plus a deterministic stdlib-only generator, so the
  mockups are regenerable rather than one-off pictures.
- One offline attended review page carrying three decision subjects: A and B as
  a choose-exactly-one selection group where rejecting both is the recordable
  "build neither" answer, and the [[BPR-0212]] join as an independent question.
- Every downstream claim a careless reader might infer declared false in the
  batch: no implementation exists, no RFC is closed, no card is closed.
- [[BPR-0211]] and [[BPR-0212]] back in `40-parked/` with the page referenced.

## Non-goals

This card writes no product code, changes no projection, and adds no widget. The
one-line `ParentId` change it describes is **not** made here — describing what an
alternative would cost is the deliverable; spending it before the owner chooses
is the mistake the two cards were deferred to avoid.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
python3 -m unittest discover -s .agents/skills/bpr-attended-review/scripts -p 'test_*.py'
python3 .agents/skills/bpr-attended-review/scripts/build_attended_review.py \
  build --input docs/verification/reviews/conversation-view-mockups-v001/offline-review-input.json \
  --out <package-dir>
git diff --check
```
