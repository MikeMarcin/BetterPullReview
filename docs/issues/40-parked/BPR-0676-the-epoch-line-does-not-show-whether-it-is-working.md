---
schema: "implement-rfc.card.v1"
id: "BPR-0676"
title: "The epoch line does not show whether it is working"
status: "Parked"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
parked_on: "human-review"
acceptance: [{"criterion": "The line states its own state in words: whether a pair resolved and can be read", "outcome": "pass", "revision": "d212f7c2", "evidence": {"kind": "observation", "detail": "Three words on the line itself: 'no pair yet', 'pair ready', 'pair not readable', written in the same place as the long sentence the details region shows so the two cannot disagree, with that sentence as the tooltip. qt_epoch_comparison_control_scale_{100,125,200} pin all three states and the tooltip; a summary that always says ready, or that carries no detail, each fail a named assertion. Fifty-six Qt and composition selectors pass with boundaries, tidy, format and docs."}}, {"criterion": "The nodes, the span and the amber tick are legible at the widths the workbench supports, and the line says whether the pair is the default or a choice", "outcome": "fail", "revision": "d1ba791f", "evidence": {"kind": "observation", "detail": "Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0676-epoch-line-legibility, revision app-release-cc2afa7f-2026-09-11.2 (bundle built at d1ba791f, executable sha256 cc2afa7f…), fingerprint 21b9321e04a5cc162fc1cb145281aa1e388c1a7b7ded942e853b495c7ee6757f, decision 180fd1d8-99c5-4f04-818f-730a60dbf179, actor human, 2026-09-11T18:36:26Z: reject, 'No visible epoch line in the test'. The three-word state was visible ('pair ready' in the owner's screenshot on BPR-0646) but the nodes, span and tick could not be judged because the line had no updates to draw on the restored review (BPR-0725). Asked again on a bundle carrying BPR-0725."}}]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# The epoch line does not show whether it is working

The owner, 2026-09-07: "The epoch line is quite hard to see if it's working
properly or not."

Their capture shows the line as the word "Updates", a pair of small marks,
and "Sep 7, latest", above a "Compari…hole PR" control truncated to
illegibility. RFC-BPR-0028 AC-02 asks for a node per upload with labels
beneath, bracket handles, a blue span and an amber tick for the last
review. At this width almost none of that is legible, and nothing says
whether the two anchors are where the reviewer put them or where the
product defaulted.

## Scope

- The line states its own state in words: which two uploads are being
  compared and whether that is the default or a choice the reviewer made.
- The nodes, the span and the amber tick are legible at the widths the
  workbench supports, or the line says it is too narrow and offers the
  detail another way.
- Owned paths: `src/adapters/qt` epoch comparison control, its tests and
  the render evidence they pin.

## Acceptance

- At 1280 and 1920 wide the line names the compared pair in words and its
  marks are legible; the owner accepts it in an attended session.

## Notes (2026-09-07)

- **What was missing.** The line drew nodes and a named comparison and
  said nothing about whether a pair had actually resolved. The sentence
  that says it, "Exact epoch pair. Left available; right available. Ready
  for diff.", existed already but lived in the details region, behind a
  disclosure the owner had no reason to open.
- **What it says now.** Three words on the line itself, from the same
  facts and written in the same place as that sentence so the two cannot
  disagree: "no pair yet" before anything resolves, "pair ready" when the
  pair can be read, "pair not readable" when it is retained but cannot.
  The long sentence is its tooltip.
- **What is not done.** The card also asks whether the pair is the default
  or a choice the reviewer made, and for the nodes and the amber tick to
  be legible at every width. The first is knowable from the selection and
  is worth adding once the owner has seen whether the summary is enough;
  the second was mostly the drawers, which
  [[BPR-0674]] now collapses in Overview, and the rest is a judgement to
  make against a real window rather than a test.
- **Evidence.** `qt_epoch_comparison_control_scale_{100,125,200}` pin all
  three states and the tooltip. Fifty-six Qt and composition selectors
  pass, with `check_boundaries`, strict tidy, `clang-format --Werror` and
  `verify_documentation.py`.
- **Negative mutations.** A summary that always says ready fails "the line
  says when nothing is being compared yet"; a summary that carries no
  detail fails "the line says a readable pair is compared, and carries the
  detail".

## Attended verdict, 2026-09-11 (v006): rejected

Session `cbd48b91…`, revision `app-release-cc2afa7f-2026-09-11.2` (bundle built at d1ba791f, executable sha256 `cc2afa7f…`), actor human; handed back at sequence 278 with all fifteen candidates decided. Candidate `BPR-0676-epoch-line-legibility`, decision `180fd1d8`, 18:36:26Z: **reject**. Verbatim:
"No visible epoch line in the test".

The owner's screenshot on BPR-0646 shows the row as "Updates", "No updates loaded.", "pair
ready" and "Comparing exact anchors": the three-word state this card added is there, and the
nodes, span and amber tick are not, because the restored review published no updates to the
line (a cache-hit defect,
[BPR-0725](BPR-0725-the-epoch-line-shows-no-updates-when-the-catalogue-already-holds-the-head.md)).
Legibility cannot be judged on an empty line. This card stays parked on human-review; its
candidate is asked again on a bundle carrying BPR-0725.

## Attended candidate, 2026-09-11 (v007)

The pending criterion above is candidate `BPR-0676-epoch-line-legibility` in
[rfc-bpr-0028-attended-v007](../../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md)
on the bundle built from the source at 2cdc0e57 (executable sha256
`64d9e46a…`), candidate revision `app-release-64d9e46a-2026-09-11.3`;
prepared and protected at sequence 311, undecided, no host running until
the owner asks for the session. The ask: legibility of the nodes, span and tick on a line that now carries updates on a cache hit (BPR-0725).
