---
schema: "implement-rfc.card.v1"
id: "BPR-2313"
title: "Resolve superseded and shipped RFCs: 0021 rejected; 0028, 0044 to 0052 closed to what shipped"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"tracker","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2310--BPR-2313--b73895df6230.json","origin":"owner, 2026-11-20: \"All, incl. notes check\" (vestigial items cut; no legacy support for old data)","related_rfcs":["RFC-BPR-0021","RFC-BPR-0028","RFC-BPR-0044","RFC-BPR-0045","RFC-BPR-0046","RFC-BPR-0047","RFC-BPR-0048","RFC-BPR-0049","RFC-BPR-0050","RFC-BPR-0051","RFC-BPR-0052"],"related_cards":["BPR-1021","BPR-1135","BPR-1068","BPR-1069","BPR-1070","BPR-1071","BPR-1094","BPR-1095","BPR-1096","BPR-1097","BPR-1098","BPR-1099","BPR-1100","BPR-1101","BPR-2214","BPR-2310"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
---

# BPR-2313: Resolve superseded and shipped RFCs

## Asked

The owner approved the vestigial-work list on 2026-11-20, verbatim: "All,
incl. notes check". The documentation items on that list:

- RFC-BPR-0028: mark it superseded by RFC-BPR-0052; resolve RFC-BPR-0052 and
  its closeout BPR-1135, closing it to what shipped or rejecting the rest.
- RFC-BPR-0047 to 0051 (comment labels): close the shipped scope, reject the
  rest; close BPR-1096 to BPR-1101.
- RFC-BPR-0044 to 0046 (tour CLI, shared drafts, thin skill): close to what
  shipped (`bpr tour`, `packaging/agent-skills/pr-tour`); BPR-1069 to 1071.
- RFC-BPR-0021 (conversation revision identity): reject.

## What changed

The repository's RFC statuses are Proposed, In Progress, Auto-Verified,
Deferred and Rejected; there is no Superseded. A delivered RFC that a later
one replaces in part closes Auto-Verified with a record of what was replaced,
as RFC-BPR-0025 did. Each RFC below gains a "Status" section under its title
naming the owner's decision, what shipped (the delivering Done cards), what
was withdrawn and which criteria are therefore not passed. No closeout pass
ran: every criterion stands on its delivering cards' recorded evidence, and
the closeout checklists' "Not run" rows stay as they were.

| RFC | Was | Now | Closeout | Withdrawn verification cards |
|---|---|---|---|---|
| RFC-BPR-0021 | Deferred | Rejected | none (no card was ever opened) | none |
| RFC-BPR-0028 | Proposed | Auto-Verified; chrome superseded by RFC-BPR-0052 | [[BPR-1021]] Done | none |
| RFC-BPR-0052 | Proposed | Auto-Verified (all 17 slices Done; the Notes tab in AC-09 retired by RFC-BPR-0127) | [[BPR-1135]] Done | none |
| RFC-BPR-0044, 0045, 0046 | Proposed | Auto-Verified | [[BPR-1071]] Done | [[BPR-1068]], [[BPR-1069]], [[BPR-1070]] |
| RFC-BPR-0047 | Proposed | Auto-Verified; RFC-BPR-0021 dropped from `depends_on` | [[BPR-1097]] Done | [[BPR-1096]] |
| RFC-BPR-0048 | Proposed | Auto-Verified | [[BPR-1098]] Done | [[BPR-1095]], [[BPR-1096]] |
| RFC-BPR-0049 | Proposed | Auto-Verified | [[BPR-1099]] Done | [[BPR-1094]], [[BPR-1095]], [[BPR-1096]] |
| RFC-BPR-0050 | Proposed | Auto-Verified | [[BPR-1100]] Done | [[BPR-1094]], [[BPR-1095]], [[BPR-1096]] |
| RFC-BPR-0051 | Proposed | Auto-Verified | [[BPR-1101]] Done | [[BPR-1094]], [[BPR-1096]] |

- The withdrawn verification cards are Done by owner amendment, as
  [[BPR-0746]] was, with `metadata.owner_amendment` and the owner's words:
  what they had not run is withdrawn, recorded as an amendment and not as a
  pass, and counts as no evidence. The board's `resolution: "rejected"` is
  reserved for cards of a Rejected RFC, which none of these RFCs is.
- [[BPR-1068]] and [[BPR-1069]] were outside the three cards named for the
  tour CLI pack but are its remaining verification (bundled macOS CLI and
  headless qualification; attended three-interface and second-machine
  acceptance), and [[BPR-1094]] and [[BPR-1095]] likewise for the
  comment-label pack. Closing the RFCs to what shipped withdraws them too.
- [[BPR-2214]] (record RFC-BPR-0037 once RFC-BPR-0028 closes) is now
  unblocked. It stays in Backlog for its own round.
- The RFC index rows carry the new statuses and this decision.

## Acceptance

- AC1. RFC-BPR-0021 is Rejected with the owner's decision and no open card
  names it.
- AC2. RFC-BPR-0028 and RFC-BPR-0052 are Auto-Verified with the supersession
  recorded, and their closeouts are Done.
- AC3. RFC-BPR-0044 to 0046 and 0047 to 0051 are Auto-Verified with what
  shipped and what was withdrawn recorded on each, their closeouts Done, and
  their remaining verification cards Done by owner amendment with their
  criteria withdrawn.
- AC4. Every moved card sits in the lane its status names.
- AC5. `verify_documentation.py` passes.

## Evidence

| Criterion | Check | Result |
|---|---|---|
| AC1 to AC4 | Read of each RFC's new Status section, the index rows, and the moved cards' front matter | As recorded above |
| AC5 | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |
