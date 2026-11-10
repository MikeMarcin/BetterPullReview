---
schema: "implement-rfc.card.v1"
id: "BPR-2051"
title: "Enhance on draft comments and notes, each surface with its own editable prompt"
status: "In Progress"
rfcs: ["RFC-BPR-0106", "RFC-BPR-0107"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2051--BPR-2051--efa66a8a01d3.json","origin":"owner request, 2026-11-10"}
depends_on: []
---

# Enhance on draft comments and notes, each surface with its own editable prompt

## Origin

The owner, 2026-11-10: "I want to add enhance to draft comment and notes like
we have it on tour stops. It might need an icon design to fit into the ux" and
"Enhance needs to be based on a prompt we can edit in agent settings".

## Scope and acceptance

1. The draft comment editor (a new comment and a reply) and the private note
   editor offer Enhance. It calls the operation a tour stop's Enhance uses
   (the writing contract, the structured rewrite result and its decoder), not
   a second one.
2. Enhance runs only when pressed. Its result is a suggestion shown beside
   the text, which the user takes or discards; it is never applied without
   that choice. Enhance writes nothing to GitHub.
3. The Enhance control wears the Agent accent (it starts a run), and the
   accent inventory stays one set with the run controls.
4. Each surface (draft comment, note, tour stop) has its own Enhance prompt
   in Settings › Agents & context › Built-in prompts, with a built-in default
   and a one-click restore. A tour stop's Enhance stops borrowing the
   Generate comment prompt.
5. A run uses the prompt saved when Enhance was pressed: saving a changed
   prompt while it runs neither changes nor discards it.
6. The prompts fit RFC-BPR-0107's exchange: they are writing-hook purposes in
   the same list and store, recorded in that RFC.
7. Owner look on a frozen build whose fixture already holds a draft comment
   and a note: one icon choice among two or three drawn options (light and
   dark), Accept/Reject for placement, Accept/Reject for the prompt editor.

Load frontend-design before the Qt work.
