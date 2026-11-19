---
schema: "implement-rfc.card.v1"
id: "BPR-2264"
title: "Writing style: one built-in, editable Feature, /bpr:writing-style, that every request whose answer a person reads carries"
status: "In Progress"
rfcs: ["RFC-BPR-0129"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","release":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner, 2026-11-18: suggested comments are hard to read; one built-in but editable writing prompt rather than editing every prompt; it should be in 0.5.0; then: build it as a Feature, immutable by default, this the one editable Feature, edited in the Prompts library","reservation":"docs/issues/_reservations/BPR-2264--BPR-2264--c77bb219696e.json","related_rfcs":["RFC-BPR-0129","RFC-BPR-0107","RFC-BPR-0128"],"acceptance":["BPR-2264:every-flow","BPR-2264:override","BPR-2264:once","BPR-2264:formats-win","BPR-2264:exchange","BPR-2264:size"]}
---

# BPR-2264: Writing style, one built-in, editable Feature

## Request

The owner, 2026-11-18: "I'm finding the suggested comments difficult to
read. I feel like we need a bit more of our technical humanizer in the
writing or maybe it's better to have a hook where we can plug in a writing
skill for the prompts like a feature that is built-in but editable. Because
putting that everywhere we write for human audience is too many places to
change." Then: "It should definitely be in the 0.5.0 release." Then: "build
the writing style hook at a Feature I think. That way all the skills can
reference it easily. And then realistically I would probably just point that
hook at my existing skills. But there should be mostly immutable features
but we can have some editable features using the prompt library to edit
them. This being the only one I can think of right now".

This card ships ahead of RFC-BPR-0129, which adopts its Feature registry.

## Scope

- A Feature registry (`bpr:` names). Every Feature is immutable unless the
  registry marks it editable; `writing-style` is the one editable Feature.
- **Writing style** is edited in Settings › Prompts like the built-in
  prompts (override, compare, use built-in), and travels in the settings
  and prompt-pack exchange under the purpose ID `feature.writing-style`.
- One helper appends it to every agent request whose answer a person reads.
  A prompt or skill naming `/bpr:writing-style` gets the same text, once.
- Formats win: the style follows the request's own instructions and says
  that any format, schema or block rule wins, and that in a JSON answer it
  governs only the text inside string values.
- The text is sent as written. It may simply name a skill installed for the
  agent. Values substituted from pull request data never bring in a Feature.
- `suggested-comment.md` is tightened: one problem, its consequence and the
  fix, in plain words.

## Acceptance

- **every-flow**: each flow family's sent request carries the style text.
- **override**: an override changes what every flow sends.
- **once**: an explicit `/bpr:writing-style` reference sends the text once.
- **formats-win**: JSON-answer flows still parse with the fixture agent.
- **exchange**: export and import carry the override.
- **size**: the default is under 2 KiB; the override's bound holds and a
  launch with the largest override is still admitted.
