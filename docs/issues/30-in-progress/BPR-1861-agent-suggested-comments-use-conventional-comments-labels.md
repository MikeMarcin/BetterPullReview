---
schema: "implement-rfc.card.v1"
id: "BPR-1861"
title: "Agent suggested comments use Conventional Comments labels"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"feature","found_by":"owner request, 2026-11-04: \"Suggested comments from the agent should use the conventional comments labels\"","reservation":"docs/issues/_reservations/BPR-1861--BPR-1861--d5b136681c83.json","related_cards":["BPR-1077","BPR-1082","BPR-1083","BPR-1565","BPR-1566","BPR-1753","BPR-1773"],"frontend_design":"not loaded: no widget, layout or visual change; the existing label chip and toggles show the label"}
acceptance: []
---

# BPR-1861: Agent suggested comments use Conventional Comments labels

## Request

The owner, 2026-11-04: "Suggested comments from the agent should use the
conventional comments labels".

## Where an agent's comment suggestions come from

- **Structured findings.** The review skill
  (`packaging/agent-skills/bpr-pr-review/SKILL.md`, carried in every review
  request's brief as `brief.skill.text`) asks a review to end with a
  `bpr-findings` block whose findings may carry `suggestedComment`;
  `application::decode_agent_review_findings` (or a harness's `bprFindings`
  payload) puts it in `core::agent_finding::SuggestedComment`. A finding card's
  **Add as draft comment** (`act_on_agent_card` in the composition) and the
  queue's promotion (`core::promote_agent_finding`) draft it.
- **An answer's Draft comment** ([[BPR-1773]]): the whole reply, or the part
  selected, drafted into a thread's reply, a line comment or the pull
  request's comment (`draft_agent_answer` in the composition).
- The MCP context helper serves only read-only snapshot tools; it carries no
  instructions about comments and is unchanged.

The comment editor keeps no label of its own: its chip and toggles read the
body's leading header ([[BPR-1083]]), so a label "set in the control" is a
header the chip recognizes in the spelling the chip writes.

## Design

1. **Ask for the format.** The review skill, raised to `bpr-version: 3` (an
   edited copy is offered the new version, RFC-BPR-0092), asks every
   `suggestedComment` and any reply written for the reviewer to post to open
   with one plain `label (decorations): subject` header, naming the nine
   labels the owner listed and the three standard decorations, and its
   example suggestion is one.
2. **Read it deterministically in core.**
   `core::read_agent_comment_suggestion` reads the first line tolerantly
   (any case; `**`, `__`, `*` or `_` around the label, the label and its
   decorations, or the header through its colon; spaces around the list and
   colon; standard decorations with `-`, `_` or a space between words; the
   subject on the next line when the header stands alone) and writes the
   header again through `decide_feedback_composition_edit`, the chip's own
   edit, then checks the result reads back as that label. Any of the twelve
   catalog labels is read, as the editor reads them. A missing label stays
   missing; an unknown one (`nit:`, `bug:`, plurals) and a header the
   serializer would have to restructure (a list or fence subject, a broken
   decoration list) are kept byte for byte.
3. **Use it on every draft path.** Promotion, the finding card's draft and
   the answer's three draft places draft the result; Save as note keeps the
   agent's text as written.

## Acceptance

- **AC1:** Core parse cases: every catalog label in lowercase, title case,
  upper case, bold and with leading blank lines; each decoration alone and
  combined, in any case and separator, custom ones kept as written, one
  weight, duplicates once; emphasis forms; a header alone on its line; CRLF
  and UTF-8; missing, unknown, alias and plural labels left unlabelled and
  byte for byte; broken headers kept as written; every result round-trips
  through the recognizer and is a fixed point.
- **AC2:** A promoted finding drafts the canonical header and the agent's
  own words; an unknown label is not invented into one.
- **AC3:** The shipped skill is version 3 and asks for Conventional Comments
  with every label and decoration named; its example reads as a recognized
  `question` header.
- **AC4:** Composed course: an agent's answer on a review thread written as
  `**Suggestion (Non-Blocking):** …` drafts into the thread's reply as
  `suggestion (non-blocking): …` with the reply editor's label control on
  `suggestion` and non-blocking; without the composition's reading the same
  course fails.
- **AC5:** Format, documentation gate, and the focused suites that read the
  skill or the canary pass.
