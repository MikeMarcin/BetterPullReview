---
schema: "implement-rfc.card.v1"
id: "BPR-0143"
title: "Mixed-client acceptance against a designated repository"
status: "Parked"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0142"]
parked_on: "awaiting-live-github"
---

# Mixed-client acceptance against a designated repository

RFC-BPR-0008 requires that mixed-client edits, replies, deletion, resolution,
and reopening are preserved. BPR-0138 proves the reconciliation against a fake
provider, which establishes that the projection handles each shape correctly.
It does not establish that GitHub produces those shapes.

## Exactly what is missing

The designated repository and token from BPR-0142, plus a **second client**
acting on the same pull request — the web UI is sufficient — while the
workbench is open.

## Required direct evidence

- A comment created externally appears after refresh, with its GitHub id.
- A comment edited externally updates rather than duplicating.
- A comment deleted externally disappears rather than lingering.
- A thread resolved and reopened externally reaches the workbench in both
  directions.
- A local assumption is shown never to overwrite an unknown external change
  without an explicit user action.

## Current slice — 2026-09-05

BPR-0142 is Done. Claim this card to reconcile the existing external reply
evidence, identify the remaining bounded live operations and prepare their
exact continuation. Preserve remembered sign-in and reuse the existing test
thread; no further message or destructive action is claimed by preparation.

## Prepared mixed-client continuation — 2026-09-05

The existing external creation/reply-return evidence is retained in
[the reply result](../../verification/reviews/web-comment-reply-return-v001/attended-result.md).
A [bounded packet](../../verification/reviews/mixed-client-thread-state-v001/operator-packet.md)
now names the exact reply edit/deletion and parent resolve/reopen sequence.
No new remote operation or acceptance is claimed. The app's ordinary Refresh
replaces accepted server snapshots and can show each required visible state.
Its internal canonical-ID mapping and server-wins behavior have fixture
coverage; numeric-ID visibility and a live editable-local conflict remain
explicit observation limits. The closeout's database-loss/uninstall sequence
also remains separately unproved; do not substitute restart recovery tests.

Park this live checkpoint while continuing the independent BPR-0144 production
review workflow. Receive a plain owner instruction for the prepared remote
sequence, then revalidate the exact target and observe each actual result.

## Four actions approved; live execution unavailable — 2026-09-05

The owner answered “Approve the four test actions”. The
[recorded response and result](../../verification/reviews/mixed-client-thread-state-v001/validation-record.json)
bind the unchanged packet. Computer Use stopped when selecting Google Chrome
because its current URL was disallowed. No action ran and no alternate route
was attempted. Approval is complete; this card now parks on live execution.
Revalidate the exact identities when a permitted course becomes available.
Continue independent whole-review implementation meanwhile.
