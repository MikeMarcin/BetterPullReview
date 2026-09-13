---
schema: "implement-rfc.card.v1"
id: "BPR-0724"
title: "Read mode leaves too little horizontal room for the code"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_decision":"65f3f881-91a3-4388-8b8c-0c69fc0a57d6"}
acceptance: [{"criterion":"Choosing Read dispatches exact set-drawer intents: files collapsed, conversations expanded, repositories collapsed; the other presets are unchanged; qt_workbench_window_scale_100 pins it","outcome":"pass","revision":"working tree over dab5fa68","evidence":{"kind":"command","detail":"qt_workbench_window_scale_{100,125,200} and qt_composed_github_scale_100 pass with the preset test's Read row changed to repositories collapsed; check_boundaries, clang-format --Werror, strict tidy on workbench_window.cpp and git diff --check clean."}},{"criterion":"RFC-BPR-0028's modality paragraph says Read collapses the repositories drawer","outcome":"pass","revision":"working tree over dab5fa68","evidence":{"kind":"observation","detail":"docs/rfcs/RFC-BPR-0028-review-workbench-direction.md modality paragraph amended with the owner direction of 2026-09-11 and an answers note appended."}},{"criterion":"The owner chooses, on the signed bundle in the next attended session, which of the four options Read should take next, or accepts Read as it then is","outcome":"pass","revision":"d1ba791f","evidence":{"kind":"observation","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0724-read-width, revision app-release-cc2afa7f-2026-09-11.2 (bundle built at d1ba791f, executable sha256 cc2afa7fb84d443a1bba93373b990be83748be20451fa0b6adde45cc6fae4c1c), fingerprint 29aaf8afe6bc6929c43440f70328af89f9741067e5e5dc4e3ecc6f9d93ba6f3a, decision 205ac8b2-30fd-4339-8d08-51c621ab7fc8, actor human, 2026-09-11T18:42:22Z: accept. Note, verbatim: \"Toggle to single pane view like p4merge's layout would be good\" Outcome select, two options chosen: 'Keep the conversations drawer in Read but let it go narrower than 375 pixels' (carded as BPR-0728) and 'A single-pane Read layout (one document with the other side's changes marked) as a new slice' (carded as BPR-0729, with the owner's two p4merge screenshots, captures/owner-p4merge-single-pane-{1,2}-2026-09-11.png). Not chosen: closing the conversations drawer in Read (AC-05 stays as written) and leaving Read as it is."}}]
---

# Read mode leaves too little horizontal room for the code

The owner, in the v003 verdict on
[BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md):
"Even in read mode viewing experience is not great." Asked what
specifically was wrong, the owner answered on 2026-09-11, verbatim:

> it was difficult to read the code because of a lack of horizontal area
> for the text display

## What the code says

Read is a drawer preset (`workbench_window::choose_modality`,
`src/adapters/qt/src/workbench_window.cpp`): it collapses the files drawer,
expands the conversations drawer and leaves the repositories drawer in
whatever state it has. The comparison is two whole-document panes side by
side (`diff_workbench.cpp`, `Comparison->setSizes({ 500, 500 })`, no line
wrap). The conversations drawer has a 375-pixel minimum
(`conversation_drawer_default_width`) and the repositories drawer opens at
300 by default.

At the owner's window, about 1,390 points wide, Read with the repositories
drawer open leaves the comparison about 715 points, so each pane shows
roughly 45 columns of code at the default code font; with the repositories
drawer collapsed by hand (the owner's screenshots show it collapsed) each
pane gets roughly 490 points, about 60 columns. That is the lack of
horizontal area.

## Scope

Two parts. The first is bounded and lands here; the second is the owner's
choice, asked in the next attended session.

1. Read collapses the repositories drawer as Overview does. The repositories
   tree is not part of reading a file and was never part of the Read
   preset; RFC-BPR-0028 says Read "collapses the files drawer and keeps
   comments beside the code" and says nothing about repositories. The
   strip's toggle reopens it. The preset test pins the three exact intents.
2. What Read does with the remaining width is a requirement decision the
   RFC settles the other way today ("keeps comments beside the code",
   AC-05 "Read shows this file's threads aligned to their lines"), so it
   is not changed here. The attended candidate offers the owner:
   - Read also collapses the conversations drawer, so the comparison takes
     the whole width and the strip's toggle brings the threads back
     (amends the RFC's Read definition and AC-05's wording).
   - Keep the conversations drawer in Read but let it go narrower than
     375 pixels.
   - A single-pane layout for Read (one document with the other side's
     changes marked), which is a new diff surface and would be a new RFC
     slice.
   - Leave Read as it is after part 1.

Owned paths: `src/adapters/qt/src/workbench_window.cpp`,
`tests/adapters_qt/workbench_window_tests.cpp`, RFC-BPR-0028's modality
paragraph.

## Acceptance

- Choosing Read dispatches exact set-drawer intents: files collapsed,
  conversations expanded, repositories collapsed; the other presets are
  unchanged. `qt_workbench_window_scale_100` pins it.
- RFC-BPR-0028's modality paragraph says Read collapses the repositories
  drawer.
- The owner chooses, on the signed bundle in the next attended session,
  which of the four options above Read should take next, or accepts Read
  as it then is.

## Implementation, 2026-09-11

Part 1 landed: `choose_modality` collapses the repositories drawer for Read
as it does for Overview (`workbench_window.cpp`), the preset test in
`qt_workbench_window_scale_100` pins files collapsed, conversations
expanded, repositories collapsed, and RFC-BPR-0028's modality paragraph
says so. `qt_workbench_window_scale_{100,125,200}` and
`qt_composed_github_scale_100` pass; `check_boundaries`, `clang-format
--Werror` and `git diff --check` are clean. Part 2 is the options
candidate `BPR-0724-read-width` in the v005 attended packet; the card
stays parked on human-review until the owner picks.

## Attended candidate, 2026-09-11 (v005)

The pending criterion above is candidate `BPR-0724-read-width` in
[rfc-bpr-0028-attended-v005](../../verification/reviews/rfc-bpr-0028-attended-v005/operator-packet.md)
on the bundle built from the source at 25e1525d (executable sha256
`d238842d…`), candidate revision `app-release-d238842d-2026-09-11.1`;
prepared, undecided, no host running until the owner asks for the session.
The v004 packet is historical.

## Attended candidate, 2026-09-11 (v006)

The pending criterion above is candidate `BPR-0724-read-width` in
[rfc-bpr-0028-attended-v006](../../verification/reviews/rfc-bpr-0028-attended-v006/operator-packet.md)
on the bundle built from the source at d1ba791f (executable sha256
`cc2afa7f…`), candidate revision `app-release-cc2afa7f-2026-09-11.2`;
prepared, undecided, no host running until the owner asks for the session.
The ask is unchanged; the v005 packet is historical.

## Attended verdict, 2026-09-11 (v006): accepted

Session `cbd48b91…`, revision `app-release-cc2afa7f-2026-09-11.2` (bundle built at d1ba791f, executable sha256 `cc2afa7f…`), actor human; the session was handed back at sequence 278 with all fifteen candidates decided. Candidate `BPR-0724-read-width`, decision `205ac8b2`, 18:42:22Z: **select**, two options:
"Keep the conversations drawer in Read but let it go narrower than 375 pixels" and "A
single-pane Read layout (one document with the other side's changes marked) as a new slice".
Verbatim note: "Toggle to single pane view like p4merge's layout would be good", with two
screenshots of p4merge's single-pane diff
([1](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-p4merge-single-pane-1-2026-09-11.png),
[2](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-p4merge-single-pane-2-2026-09-11.png)): one document, removed lines struck
through on a green field, added lines on a purple field, both in place. Not chosen: closing the
conversations drawer in Read (AC-05 stays as written) and leaving Read as it is. The choice was
this card's last criterion; the work is
[BPR-0728](BPR-0728-the-conversations-drawer-goes-narrower-than-375-pixels-in-read.md)
and
[BPR-0729](../40-parked/BPR-0729-a-single-pane-read-layout-in-the-manner-of-p4merge.md). Done.
