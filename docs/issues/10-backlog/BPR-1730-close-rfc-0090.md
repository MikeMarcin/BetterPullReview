---
schema: "implement-rfc.card.v1"
id: "BPR-1730"
title: "Verify and close RFC-BPR-0090"
status: "Backlog"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-1726", "BPR-1727", "BPR-1728", "BPR-1729", "BPR-1733"]
closeout_for: ["RFC-BPR-0090"]
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1726--BPR-1733--4ee9bc97752e.json","origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2","milestone":"RFC-BPR-0090 closeout"}
---

# BPR-1730: Close RFC-BPR-0090 (provisional closeout)

The closeout card for [RFC-BPR-0090](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md). When its ID is allocated it is
renamed to `BPR-<n>-close-rfc-0090.md`, titled "Verify and close RFC-BPR-0090",
given `closeout_for`, the RFC base and the closeout checklist covering
AC01 to AC10. The provisional name keeps the documentation gate's closeout
rule from reading it before then.

## Acceptance criteria

- AC1: Every RFC-BPR-0090 criterion is recorded with its check, result and
  commit, and the RFC's status is set to match.

## Closeout checklist

RFC base: `3927fca2154f51fd85a9d32a181eb1f165479884`.

| Criterion | Direct check / evidence | Result at the reconciliation | Producing commit |
|---|---|---|---|
| **AC01:** From Shift+Return, the right-click item and the review bar, on a selection or the caret alone, the head pane becomes editable with unchanged text, scroll, numbers and colours, and the caret at the end of the last selected line. A base line, a range across changed regions, a single line outside every changed region and a range on a filtered diff whose right side is not head are refused at entry with their reason. | Pending | Pending | Pending |
| **AC02:** Inside the range, RC55-07's gestures (multi-caret, box with virtual space, IME, undo, clipboard, Enter indentation) work in the pane on the composed application. An edit that would change a fixed byte is refused whole with the status line's words; Enter grows the range and the rows below move; emptying the span is a deletion. | Pending | Pending | Pending |
| **AC03:** For fixture edits that replace, delete, grow and shrink the lines and hold backticks, the published comment's `start_line`, `line`, `side` and body match the core's plan byte for byte, in the fixture service and on BPR-Test. GitHub's "Commit suggestion" then gives exactly the edited file, for an LF file and a CRLF file (the line-ending measurement). | Pending | Pending | Pending |
| **AC04:** An unplaceable suggestion is marked with its reason on the comment and on the Finish your review page, cannot be published as a suggestion, and Convert gives the core's text body at the same lines, published where the core places a text comment. No suggestion is moved to another line. | Pending | Pending | Pending |
| **AC05:** The drawer shows the prose and "What it changes" and never a prefilled block. A label chip edit changes only the header's bytes of the composed body (`core_feedback_suggestion_preservation` extended to composed bodies). | Pending | Pending | Pending |
| **AC06:** A kept suggestion survives a file switch, closing the window and a killed process. Reopening rebuilds the same span exactly, and an unparsable block, a second block or another comparison is refused with its reason. | Pending | Pending | Pending |
| **AC07:** Undo, Delete, leaving the edit and typing back to the original behave as "Lifecycle" says. | Pending | Pending | Pending |
| **AC08:** With a new head arriving mid-edit, the edit ends and the draft stays on its comparison. Carry is offered exactly when every original line of its range is unchanged at the new head, and Copy change otherwise. | Pending | Pending | Pending |
| **AC09:** On a filtered diff whose right side is not head, a line unchanged at head is placeable, and a changed one is unplaceable with its reason. | Pending | Pending | Pending |
| **AC10:** The owner accepts the built in-pane edit on a frozen build, one question per look criterion: entry and continuity, the fixed lines' refusal, growth and deletion, the drawer link, the unplaceable mark and Convert, and one pane. | Pending | Pending | Pending |
