---
schema: "implement-rfc.card.v1"
id: "BPR-0648"
title: "Aligned comment cards in the Read drawer"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: ["BPR-0646"]
priority: "P1"
---

# Aligned comment cards in the Read drawer

RFC-BPR-0028 mockup state 2. The comments drawer shows this file's threads as
cards positioned at their lines (root anchored to the line like Code
Collaborator), replies on an avatar rail with a thread line, unread dots and
read-state dimming, Reply, Resolve and Mark read, and my unpublished notes as
dashed drafts with Publish, Edit and Delete. Gutter marks in the diff: amber
for a thread, blue for a new one; the strip's jump control walks the blue
ones. Whole PR is a filter listing every thread in file order. Uses the
existing conversation model and delegate; no new authority.

## Acceptance

- RFC-BPR-0028 AC-05 for Read, including cards that never overlap and follow
  the comparison's scroll.
- The twenty-post, three-branch recorded conversation (AC-08) reads correctly.
- Focused delegate and drawer tests; the round-trip composition contract stays
  green with its digests repinned.
