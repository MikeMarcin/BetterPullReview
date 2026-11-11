---
schema: "implement-rfc.card.v1"
id: "BPR-2001"
title: "Verify and close RFC-BPR-0101"
status: "Backlog"
rfcs: ["RFC-BPR-0101"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0013-close-agent-chat-conventions.md","draft_title":"Close the agent chat conventions RFC"}
depends_on: ["BPR-1998", "BPR-1999", "BPR-2000"]
closeout_for: ["RFC-BPR-0101"]
---

# BPR-2001: Verify and close RFC-BPR-0101

## Scope and acceptance

Judge every acceptance criterion of
[RFC-BPR-0101](../../rfcs/RFC-BPR-0101-agent-chat-conventions.md) against actual
evidence from the cards this one depends on, including its live,
installed-agent and owner-attended criteria, the reconciliation of the live
RFCs it amends, and the documentation gate. Record failures and unreached
requirements exactly.

Leave live IDs, the implementation baseline and acceptance records to
authorized execution. This proposed closeout carries no passing evidence and
changes no live RFC's status.

This is part of `accepted-scope` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Each listed form, in prose, in emphasis and as a whole code span, becomes a link in an Agent tab reply, a selected-lines side chat reply and a tour side chat reply, and opens the named line with its range revealed; the same text inside fenced code does not. | Pending | Pending | Not implemented |
| **AC02:** A reference to an unchanged file of the head opens that file at the line or is not drawn as a link; a name shared by two files offers both; a path that matches nothing is plain text. | Pending | Pending | Not implemented |
| **AC03:** Return sends and Shift+Return adds a line in the Agent tab and the selected-lines side chat; Return during input-method composition does not send; a comment composer still requires Ctrl or Cmd with Return. | Pending | Pending | Not implemented |
| **AC04:** Reported costs of 0.4123567, 0.004 and 12 are shown as `$0.41`, `< $0.01` (amended 2026-11-10 from `under $0.01`, [[BPR-2038]]) and `$12.00`; a run with no reported cost shows none. | Pending | Pending | Not implemented |
| **AC05:** After three turns in one native session with an installed Claude, each turn's footer shows a cost, the session line shows their sum, and the record states how the runtime's figure was interpreted. | Pending | Pending | Not implemented |
| **AC06:** The tour side chat shows the turn and session costs under the same rules. | Pending | Pending | Not implemented |
| **AC07:** An inventory of every control that dispatches an agent run and every control that wears the accent shows the two sets are equal, in the Agent tab, the side chats, the tour, the comment workshop and Review Cycles. | Pending | Pending | Not implemented |
| **AC08:** The owner accepts the Agent tab's accent and footers on a frozen build. | Pending | Pending | Not implemented |
| **AC09:** Appearance › Colours lists Agent accent; with nothing customised every accented surface shows the shipped Light and Dark values; changing it for one theme changes every accented surface in that theme and no other theme; Reset restores the built-in value; and a theme document written before the role existed imports with the built-in value. | Pending | Pending | Not implemented |
