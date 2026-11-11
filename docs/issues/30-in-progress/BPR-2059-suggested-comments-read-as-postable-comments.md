---
schema: "implement-rfc.card.v1"
id: "BPR-2059"
title: "Suggested comments read as postable comments, with an editable format prompt"
status: "In Progress"
rfcs: ["RFC-BPR-0092", "RFC-BPR-0107"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2058--BPR-2059--bd1353aea511.json","origin":"owner report, 2026-11-10 (screenshot of a private repository; nothing of it is kept)","related_cards":["BPR-1861","BPR-2051","BPR-2058"]}
depends_on: []
---

# Suggested comments read as postable comments, with an editable format prompt

## Origin

The owner, 2026-11-10, of a finding card's suggested comment: "just a big
wall of text". From the owner's screenshot (a private repository; its text
and image are not copied anywhere, and the tests use synthetic text):

- inline code showed raw backticks instead of rendering as code;
- the whole suggestion paragraph was italic;
- the header chips were cut ("possible · correctn", "mainta") because the
  file path squeezed them;
- summary, reasoning and suggestion ran together with no spacing;
- an internal line, "Continuation Replay omits 0 retained turns and cuts 0
  text bytes; earlier omissions are unknown.", showed in the reply;
- the agent restated its reasoning in the suggestion and led it with
  "suggestion (non-blocking):" though the card already shows severity.

## Scope and acceptance

1. A finding card renders its claim, evidence and suggested comment as
   Markdown (code spans, fences, line breaks), in separate spaced blocks; the
   suggestion is not italic.
2. The severity and kind chips keep their whole text; the file path elides
   or wraps instead.
3. The replay status line is not shown to the user; it stays available to
   diagnostics.
4. Agent settings › Built-in prompts gains an editable suggested-comment
   format: a new purpose ID in the scheme of `enhance.*`, a built-in default
   and version, and Reset. Its default asks for short, postable comments: one
   sentence on the problem then the fix, code in code spans, no severity
   prefix, no restated reasoning. Every Agent tab request carries it.
5. The path from reply to draft keeps the suggestion's Markdown (code spans,
   fences, line breaks).
6. Owner look on a frozen build: Accept or Reject the finding card's
   formatting (before and after), and the format prompt's default.

Load frontend-design before the Qt work.
