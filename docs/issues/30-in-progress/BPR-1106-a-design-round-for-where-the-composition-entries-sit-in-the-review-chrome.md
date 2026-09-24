---
schema: "implement-rfc.card.v1"
id: "BPR-1106"
title: "A design round for where the composition entries sit in the review chrome"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin":"owner note on attended candidate BPR-0653-entry-row-v001, session cbd48b91000b00f1e72e1ad62c7c384c, decision 007a6eb5-adf3-427c-8473-547f9d0c4d9e, 2026-09-24","origin_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0037"],"related_cards":["BPR-0653","BPR-0929","BPR-0940","BPR-0795","BPR-1036","BPR-1021","BPR-0737"],"reservation":"docs/issues/_reservations/BPR-1106--BPR-1110--72437bc3b205.json","disposition":"standalone follow-up; not an RFC-BPR-0028 closeout dependency"}
---

# BPR-1106: A design round for where the composition entries sit in the review chrome

## Why

On 2026-09-24 the owner answered [[BPR-0653]]'s ask 3 in attended session
`cbd48b91000b00f1e72e1ad62c7c384c` (packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
candidate `BPR-0653-entry-row-v001`, decision `007a6eb5`, 18:54:09Z). He chose
to amend RFC-BPR-0028 AC-01 so that it admits the entry rows between the strip
and the epoch line, and added, verbatim:

> We should do a UX design iteration with the /frontend-design skill to figure
> out where best to put these. For now you can ammend the card but open a
> follow-up for the ux design.

The amendment is landed in
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md) AC-01 and
its chrome paragraph. This card is the design iteration.

## What sits there today

`build_central_surface` (`src/adapters/qt/src/workbench_window.cpp`) stacks,
between the strip and the epoch line:

- the composition entry row from RFC-BPR-0037 (`cc038748`, [[BPR-0929]]):
  Compose review and Draft comment or reply, 20 logical pixels on Linux and
  Windows and 22 on macOS;
- while a pull request loads, [[BPR-0795]]'s readiness line.

The inputs to the design:

- The owner's [[BPR-0246]] space budget: every surface other than the code
  panes and their gutter is on a strict budget in the review state. The
  UIforETW course measures the comparison at 842/1000 of the window on Linux
  and 835/1000 on macOS with the row, and 862 and 857 without it
  ([[BPR-1036]]).
- On Windows the strip is the title row and fits at its 902-pixel floor with
  nothing to spare ([[BPR-0653]]), so it cannot simply take two more controls.
- RFC-BPR-0037's own layout study put the Compose review entry "in toolbar"
  (`conversation-surface-layout-study.md`, its Read row).
- [[BPR-0737]]'s direction put the epoch line directly under the strip, above
  the drawers and their headings. The amendment keeps it above the drawers.
- The strip's existing review controls menu was one option on the ask. It
  costs the strip no width.

## Scope

- Run a design round with the `frontend-design` skill: at least three
  placements drawn on the accepted full review surface
  ([RFC-BPR-0028 full review surface](../../design/RFC-BPR-0028-full-review-surface-v1.html)),
  light and dark, at 1280x852 and 1920x1080, on macOS and Windows chrome. The
  current row is one of them, as the baseline.
- For each placement, state what it costs: height taken from the comparison,
  strip width at the Windows floor, clicks to Compose review and to Draft
  comment or reply, keyboard reach, and where the readiness line goes.
- Put the placements to the owner as one attended question: which placement,
  or the current row.
- If the owner picks a placement other than the current row, amend
  RFC-BPR-0028 AC-01 and the chrome paragraph again, and card the
  implementation, including [[BPR-1036]]'s floor and [[BPR-0940]]'s window
  course assertion. If the owner keeps the row, record that here and close.

## Acceptance

- AC1 The design page shows at least three placements with the current row
  as the baseline, each with its stated costs, rendered and inspected headless
  at both sizes in both appearances.
- AC2 The owner's choice is recorded here, one question on its own candidate,
  with the fingerprint-bound verdict.
- AC3 Whatever the owner chooses is reflected in RFC-BPR-0028 AC-01 and its
  chrome paragraph, and any implementation is carded, not done on this card.

## Boundaries

Design only: no product source changes on this card. The core decides, the
host performs, Qt presents; a placement that needs a new command or state is
named as such for the implementation card. This card is not linked to
RFC-BPR-0028 and does not hold [[BPR-1021]]'s closeout.
