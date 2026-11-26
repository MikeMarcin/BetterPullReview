---
schema: "implement-rfc.card.v1"
id: "BPR-2415"
title: "RFC-BPR-0130 closeout: every criterion an agent can check holds with its evidence on the RFC's ledger; AC09, the owner's look, waits for the final attended review"
status: "Parked"
rfcs: ["RFC-BPR-0130"]
depends_on: ["BPR-2404", "BPR-2405", "BPR-2406", "BPR-2407", "BPR-2409", "BPR-2410", "BPR-2411", "BPR-2412", "BPR-2413", "BPR-2414"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"closeout","reservation":"docs/issues/_reservations/BPR-2415--BPR-2415--b414537861ff.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"<revision>","closeout_for":["RFC-BPR-0130"],"park_reason":"human-review","origin":"RFC-BPR-0130 close (draft card DRAFT-0014 of the attended review 3cd822672efb50572b10ab10747889ba)","related_cards":["BPR-2384","BPR-2400","BPR-2404","BPR-2405","BPR-2406","BPR-2407","BPR-2409","BPR-2410","BPR-2411","BPR-2412","BPR-2413"],"related_rfcs":["RFC-BPR-0130"],"acceptance":["AC01","AC02","AC03","AC04","AC05","AC06","AC07","AC08","AC09"]}
acceptance: []
---

# BPR-2415: Close RFC-BPR-0130

## Why

DRAFT-0014: every criterion evidenced on the RFC's ledger, the open owner
questions answered or recorded as kept defaults, and the RFC's status and
index row saying so.

## Closeout checklist

| Criterion | Status | Evidence |
|---|---|---|
| AC01 | Holds | BPR-2404 AC1; stage 7 card AC1 (`core_chat_header`, `qt_item_side_chat`, composed course) |
| AC02 | Holds | BPR-2384, BPR-2404 AC2; stage 7 card AC1 (`core_chat_history`, `qt_item_side_chat`) |
| AC03 | Holds | BPR-2400, BPR-2404 AC3; composed course refuses the replaced words |
| AC04 | Holds | BPR-2404 AC4; `qt_drawer_surfaces_fit_scale_*`, `application_composed_drawer_fit_*` |
| AC05 | Holds | BPR-2405 |
| AC06 | Holds | BPR-2406 AC3, BPR-2407 AC1, BPR-2409 AC1 |
| AC07 | Holds | BPR-2411, BPR-2412 AC1 |
| AC08 | Holds | BPR-2413 AC1, AC2; stage 7 card AC4 |
| AC09 | Pending owner | Final attended review: frozen tests-on build, Fixture agent, a pull request with a tour, a report and at least two agent sessions |

## Open owner questions

1. The transcript's look: kept default (both looks; no chat changed
   appearance), BPR-2409.
2. Reset agents…: kept default (last in the Agent tab's History, after a
   separator).
3. Open on GitHub: kept default (untouched, the drawer's tool).
4. Answered 2026-11-25: an item's chat keeps sessions (stage 7 card).
5. The rest of the header note: unanswered; nothing is built from it.

## Change

- RFC-BPR-0130: the evidence ledger (AC01 to AC08 hold, AC09 pending).
- For the integrator: the RFC's status and index row to `In Progress`
  until AC09 is accepted, then `Auto-Verified` and this card to Done.

## Not done in stage 7 (no criterion names them)

- A question typed while an item's round runs is not queued: Send is Stop,
  as before. The core queue kind (`review_item`) was drafted and set aside.
- `side_chat_panel` still builds its hidden controls; the window maps the
  Agent tab's intents through them (BPR-2412).

## Verification

- `python3 scripts/verify_documentation.py --root .`: DOCUMENTATION_OK.
