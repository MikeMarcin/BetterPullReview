---
schema: "implement-rfc.card.v1"
id: "BPR-1280"
title: "Command-Return (Control-Enter elsewhere) saves the note being written"
status: "Parked"
rfcs: ["RFC-BPR-0061", "RFC-BPR-0055"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner note on BPR-1182-agent-free-loop-v001 (attended session cbd48b91, event 947, Accept)","reservation":"docs/issues/_reservations/BPR-1280--BPR-1282--5bcc90f2edc8.json","related_cards":["BPR-1182","BPR-1157","BPR-1156"],"acceptance":["BPR-1280:save-key","BPR-1280:labels","BPR-1280:return-kept","BPR-1280:boundary","BPR-1280:owner-look"]}
parked_on: "human-review"
acceptance: [{"criterion":"BPR-1280:save-key","outcome":"pass","revision":"c1cd6b585","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"qt_review_cycle_view_1_0/_2_0 with window-system keys through Qt's shortcut map: Command-Return saves a two-line note whole (focus back to the code), keypad Enter saves, a reply and an edited suggestion are saved; nothing saved with focus outside the note, with nothing open, or for an unchanged suggestion. Negative control: key bound to F13 fails 'Command-Return (Control-Enter) saves the note'."}},{"criterion":"BPR-1280:labels","outcome":"pass","revision":"c1cd6b585","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Save note, Save suggestion and Save reply show the key on the button and in the tooltip: U+2318 U+21A9 on macOS (asserted on macOS), Ctrl+Enter elsewhere (asserted off macOS; station runs owed). Offscreen render desktop-cycle-return-note shows 'Save note ⌘↩'."}},{"criterion":"BPR-1280:return-kept","outcome":"pass","revision":"c1cd6b585","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Return is a line break in the note and the suggestion and still notes the caret's line in the read-only code (existing Return/Shift+Return rows pass). No other Control/Command-Return binding in src/; the editor's former Return-with-any-modifier line break now yields to the save key only while a composer is open; Meta+Return (Mac Control) still breaks the line."}},{"criterion":"BPR-1280:boundary","outcome":"pass","revision":"c1cd6b585","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"build/dev at c1cd6b585: 68/68 cycle, send, share, suggestion, agent-launch, round-trip (pin unchanged) and architecture_boundaries rows, 77/77 agent/composed/workbench rows; 61/61 again at aa6a47104; agent-free build/dev-tour-cli 9/9 incl. two-round course, round-trip contract, composed_application_hosts, test_network_isolation_manifest, architecture_boundaries; clang-format clean; clang-tidy nothing on changed lines. Linux and Windows are station requests."}},{"criterion":"BPR-1280:owner-look","outcome":"pending","revision":"aa6a47104","evidence":{"kind":"freeform","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Owner Accept/Reject BPR-1280-save-key-v001 in session 2dc9fa2078312808fb2b48afcdd8ee0a (ready at sequence 9) on the frozen root /Users/jaeger/bpr1280-notes-review-v001.APXGtG (git archive of aa6a47104, executable a05b45db...)."}}]
---

# BPR-1280: Command-Return (Control-Enter elsewhere) saves the note being written

## Origin

The owner accepted `BPR-1182-agent-free-loop-v001` (RC60-02) with this note,
quoted exactly:

> I want cmd+return to save the note so I don't have to click save note with a mouse

## Outcome

In the cycle workbench, Command-Return on macOS and Control-Enter on Windows
and Linux save the note being written, as Save note does. The same key saves
a reply being entered and a Suggest change edit when its Save is available.
Return keeps its existing behaviour everywhere: a new line in the note and in
the suggestion, and a note on the caret's line in the read-only code.

## Acceptance

- **BPR-1280:save-key.** A real key event (through Qt's shortcut override,
  not a slot call) saves the open note, the open reply and an editable
  suggestion; with nothing open, or a suggestion whose Save is unavailable,
  the key does nothing. Negative controls show the check fails without the
  binding.
- **BPR-1280:labels.** Save note, Save reply and Save suggestion name the key
  on the button and in their tooltips, in each platform's own words.
- **BPR-1280:return-kept.** Return still inserts a line break in the note
  and suggestion, and still notes the caret's line in the read-only code. No
  existing binding of the key in the cycle page collides.
- **BPR-1280:boundary.** The view rows, the desktop course and
  `architecture_boundaries` pass; the round-trip pin is unchanged.
- **BPR-1280:owner-look.** The owner's Accept/Reject on a frozen build.

## Result (2026-10-04): done unattended; the owner's look pending

Landed as `c1cd6b585` (candidate `aa6a47104`). Command-Return on macOS,
Control-Enter elsewhere, saves the open note, reply or suggestion as its Save
does, while focus is inside it; a suggestion with nothing to save stays open.
Each Save names the key (⌘↩, `Ctrl+Enter`). Return is unchanged. No other
binding of the key exists in `src/`. The view rows prove it with
window-system keys and fail when the key is bound elsewhere. Owner question
`BPR-1280-save-key-v001` in session `2dc9fa20…` (ready at sequence 9), frozen
root `/Users/jaeger/bpr1280-notes-review-v001.APXGtG`. Linux and Windows
station requests and the full record are in the ledger's "BPR-1280, BPR-1281
and BPR-1282" section of
[RFC-BPR-0053-0061.md](../../verification/RFC-BPR-0053-0061.md).

**Note: Linux run at `126940be4` (holds `126940be4`; station `bpr-main-linux`,
WSL2, GCC 15.2.0, Qt 6.11.2, offscreen; no remote mutation).** The `dev`
build succeeded with no warnings and 8 of 8 passed:
`qt_review_cycle_view_1_0` and `_2_0`,
`application_review_cycle_desktop_public_path`,
`application_review_cycle_send_public_path`, `review_cycle_send`,
`core_review_cycle_send`, `application_review_round_trip_composition_contract`
and `architecture_boundaries`.

The view rows check that Save note and Save suggestion end with
`QKeySequence(Ctrl+Enter).toString(NativeText)`. A separate probe against the
same Qt printed that as `Ctrl+Enter` on Linux. No pixel check failed at 1x or
2x, so there are no counts to report.
