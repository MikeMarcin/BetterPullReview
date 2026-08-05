---
id: BPR-0174
title: "Core: the context supplement and what it may carry"
status: Backlog
rfc: RFC-BPR-0011
depends_on: [BPR-0171]
---

# Core: the context supplement and what it may carry

BetterPullReview supplies only what GitHub cannot: selected epochs, the
last-reviewed head, changed-since-review files, local defect state, the current
selection, and explicitly included private notes.

## The rule this card must not erode

**Private notes and drafts are never included without explicit selection.** A
supplement that swept them in by default would send a reviewer's unfinished
thinking to a tool they may only have wanted to ask about a diff.

## Required direct evidence

- A supplement built without explicit selection contains no private note and no
  draft, and the test reads the whole supplement rather than checking for one
  absent field.
- Every supplement carries host, repository, pull request, base OID, head OID,
  epoch pair, and a schema version.
- No field of the supplement can hold a credential.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
