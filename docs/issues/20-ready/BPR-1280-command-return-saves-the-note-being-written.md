---
schema: "implement-rfc.card.v1"
id: "BPR-1280"
title: "Command-Return (Control-Enter elsewhere) saves the note being written"
status: "Ready"
rfcs: ["RFC-BPR-0061", "RFC-BPR-0055"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner note on BPR-1182-agent-free-loop-v001 (attended session cbd48b91, event 947, Accept)","reservation":"docs/issues/_reservations/BPR-1280--BPR-1282--5bcc90f2edc8.json","related_cards":["BPR-1182","BPR-1157","BPR-1156"],"acceptance":["BPR-1280:save-key","BPR-1280:labels","BPR-1280:return-kept","BPR-1280:boundary","BPR-1280:owner-look"]}
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
