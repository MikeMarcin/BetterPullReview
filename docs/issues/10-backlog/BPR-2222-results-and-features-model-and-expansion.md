---
schema: "implement-rfc.card.v1"
id: "BPR-2222"
title: "Results and Features: the catalog, /bpr: expansion for Claude and Codex, and the actions they unlock"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2219"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2222--BPR-2226--052c4bca31ba.json","milestone":"M1","milestone_acceptance":"AC01-AC08, AC13"}
---

# Results and Features: the catalog, /bpr: expansion for Claude and Codex, and the actions they unlock

RFC-BPR-0129 milestone M1, the model half ([RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md)). It starts after
[[BPR-2219]], whose read-only review format contract becomes the Findings
report Result.

## Scope

- The core catalog: Answer and Findings report Results, with their
  contracts; the Suggested comment, Suggested change and Proposed fix
  Features; which places a Result allows, and which Features a Result
  accepts.
- `bpr:` as a reserved qualifier beside `repo:` and `personal:` in
  `core/skill_references`. Completion values for Features, so the picker
  card ([[BPR-2223]]) only presents them.
- The request assembly: the prompt's text, then the Result's contract, then
  each Feature's contract. Sent the same way for Claude and Codex, and
  rendered as one value the editor shows.
- Parsing each contract's blocks and deciding their actions, then the
  host's Draft comment, Keep local and Apply. Proposed fix's Apply follows
  the RFC's Q4 default unless the owner answers otherwise.
- Migration: `prompt_ask::question` becomes Answer and `prompt_ask::review`
  becomes Findings report. The shipped review prompts gain their Features.
- The Fixture agent answers with and without each Feature's block, and with
  a malformed block.

## Acceptance

RFC-BPR-0129 AC01 (for the model's names), AC02 (the core decision), AC03,
AC04 (Answer and Findings report prompts), AC06, AC07, AC08, AC13.

## Intended verification

Core tests of the catalog, compatibility, reference scanning, assembly and
block parsing; captured requests for a Claude and a Codex agent kind; a
composed Fixture-agent course for AC03 and AC07.
