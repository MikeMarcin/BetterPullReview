---
schema: "implement-rfc.card.v1"
id: "BPR-1282"
title: "A shorter Send to Agent box that names the configured harness"
status: "Ready"
rfcs: ["RFC-BPR-0060", "RFC-BPR-0061"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner note on BPR-1182-send-box-v001 (attended session cbd48b91, event 951, Accept)","reservation":"docs/issues/_reservations/BPR-1280--BPR-1282--5bcc90f2edc8.json","related_cards":["BPR-1182","BPR-1180","BPR-1161"],"acceptance":["BPR-1282:less-text","BPR-1282:harness","BPR-1282:capabilities","BPR-1282:boundary","BPR-1282:owner-look"]}
---

# BPR-1282: A shorter Send to Agent box that names the configured harness

## Origin

The owner accepted `BPR-1182-send-box-v001` (RC61-09) with this note, quoted
exactly:

> The sned ot agent box is fine but there's too much damned text on this screen. It's overwhelming. Especailly the "starts an approved calude stream...." Also it shouldn't even be saying Claude there I don't think unless claude is what got configured. But we got claude codex and custom harnesses to support.

## Outcome

The Send box says much less: the essentials in one short line, with the
rest behind a disclosure the reviewer opens when they want it. Its words
name the harness of the agent profile selected (Claude, Codex, or a custom
harness by its own name), never a fixed "Claude" and never the built-in
profile's internal label. What is harness-specific follows the profile's
real capabilities: only a harness that can continue a session offers a
session to continue.

## Acceptance

- **BPR-1282:less-text.** Rendered before and after, the box's visible words
  are measurably fewer; the detail (same text as Copy, the harness's own
  settings and sign-in, no credentials passed, nothing else tried on
  failure, the full folder) is one action away and readable by keyboard and
  screen reader.
- **BPR-1282:harness.** With Claude, Codex and a custom harness each
  selected, the words and statuses name that harness; selecting Codex or the
  custom harness never shows "Claude" (negative control).
- **BPR-1282:capabilities.** The session chooser appears only for a profile
  whose harness continues sessions and has one to continue; otherwise no
  session wording appears.
- **BPR-1282:boundary.** The send course, the view rows, the send contract
  tests and `architecture_boundaries` pass; the round-trip pin is unchanged.
- **BPR-1282:owner-look.** The owner's Accept/Reject on a frozen build.
