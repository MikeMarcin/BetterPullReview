---
schema: "implement-rfc.card.v1"
id: "BPR-1282"
title: "A shorter Send to Agent box that names the configured harness"
status: "Parked"
rfcs: ["RFC-BPR-0060", "RFC-BPR-0061"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner note on BPR-1182-send-box-v001 (attended session cbd48b91, event 951, Accept)","reservation":"docs/issues/_reservations/BPR-1280--BPR-1282--5bcc90f2edc8.json","related_cards":["BPR-1182","BPR-1180","BPR-1161"],"acceptance":["BPR-1282:less-text","BPR-1282:harness","BPR-1282:capabilities","BPR-1282:boundary","BPR-1282:owner-look"]}
parked_on: "human-review"
acceptance: [{"criterion":"BPR-1282:less-text","outcome":"pass","revision":"aa6a47104","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Send course on cocoa and offscreen: the box shows one line of 7 words (44 before) with 29 behind Details, a flat text button reading Hide details while open; details readable as the line's accessible description; the box returns to its one-line height; renders send-box-codex, send-box-details, send-box-custom-harness."}},{"criterion":"BPR-1282:harness","outcome":"pass","revision":"aa6a47104","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"core::harness_name and each offer's Harness/Protocol (core_review_cycle_send); the course lists Codex (app server), Codex (exec), Claude, External terminal harness; Codex and the custom harness show no 'Claude' in line or details; statuses 'Claude finished its turn', 'does not allow Codex' (review_cycle_send). Negative control: harness_name returning the profile label fails 'the agents by harness'."}},{"criterion":"BPR-1282:capabilities","outcome":"pass","revision":"aa6a47104","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Session row hidden for Codex, the custom harness and Claude with no session; shown with fixture-session-1 ('Continues Claude session fixture-session-1 in send-project.'); 'Sending F1 to Codex…' without session words; only claude_stream_json continues sessions (continues_sessions)."}},{"criterion":"BPR-1282:boundary","outcome":"pass","revision":"aa6a47104","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"build/dev at c1cd6b585: 68/68 cycle, send, share, suggestion, agent-launch, round-trip (pin unchanged) and architecture_boundaries rows, 77/77 agent/composed/workbench rows; 61/61 again at aa6a47104; agent-free build/dev-tour-cli 9/9 incl. two-round course, round-trip contract, composed_application_hosts, test_network_isolation_manifest, architecture_boundaries; clang-format clean; clang-tidy nothing on changed lines. Linux and Windows are station requests."}},{"criterion":"BPR-1282:owner-look","outcome":"pending","revision":"aa6a47104","evidence":{"kind":"freeform","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Owner Accept/Reject BPR-1282-send-box-v001 in session 2dc9fa2078312808fb2b48afcdd8ee0a (ready at sequence 9) on the frozen root /Users/jaeger/bpr1280-notes-review-v001.APXGtG (git archive of aa6a47104, executable a05b45db...)."}}]
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

## Result (2026-10-04): done unattended; the owner's look pending

Landed as `b81eed2d6`, `3aa25c574` and `aa6a47104`. `core::harness_name`
names Claude, Codex or a custom harness by its own name; the box says one
line built from the selected profile (7 words, down from 44) and keeps the
rest behind Details; the Session row shows only for a harness that continues
sessions and has one to continue; statuses and the reply label name the
harness. The send course, on cocoa with real harness processes, fails when
the built-in label is used instead. The rest of the sheet's text is
unchanged and the owner's question invites a note on it. Owner question
`BPR-1282-send-box-v001` in session `2dc9fa20…` (ready at sequence 9).
Station requests and the full record are in the ledger's "BPR-1280, BPR-1281
and BPR-1282" section of
[RFC-BPR-0053-0061.md](../../verification/RFC-BPR-0053-0061.md).
