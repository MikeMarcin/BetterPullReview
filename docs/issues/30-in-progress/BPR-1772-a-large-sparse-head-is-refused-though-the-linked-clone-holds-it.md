---
schema: "implement-rfc.card.v1"
id: "BPR-1772"
title: "A large sparse head is refused as too large although the linked clone could lend it"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1772--BPR-1773--b27f9d2285df.json","kind":"defect","found_by":"owner photograph, 2026-10-26: Set up an agent, Check it works, on a large repository with a linked clone","related_cards":["BPR-1738","BPR-1751","BPR-1752"],"related_rfcs":["RFC-BPR-0092"]}
acceptance: []
---

# BPR-1772: A large sparse head is refused as too large although the linked clone could lend it

## Observed

The owner, 2026-10-26, in chat with a photograph and the words "First fix
this". Set up an agent, step Check it works, agent `declawd`, repository a
linked clone of a large game-engine repository:

- "declawd is available" passed.
- "A workspace with the history of `<clone>` at `ab9af71`" failed.
- The words: "The agent needs every file at this head, and this head is
  larger than the agent's private copy allows."
- Show the log: "The head's files were not downloaded for the agent: it is
  larger than the download allows (transport failure 11, step 9)."

The repository path and head are the owner's and are kept only as the
photograph showed them.

## Cause

The review is sparse: read over the gh transport, its cache holds commits and
trees and no blobs. Since [[BPR-1738]] the host
(`epoch_application_host::materialize_agent_workspace`) fetches the head's
whole snapshot from GitHub before the cache makes the workspace. That
download has the transport's response bounds, and a large repository's head
exceeds them (`response_limit`, mapped to `too_large`).

The download happened unconditionally, before the Git adapter was asked.
The adapter ([[BPR-1751]]) already takes the head from a linked clone that
holds it, copying nothing, so with the owner's clone the download was never
needed. The host never found out, because it asked the clone only after the
download had already failed.

## Fix

- **Request.** `agent_workspace_materialization_request::HeadFromClone`
  asks for the head from `HistoryClone` or not at all. It requires a clone.
- **Git adapter.** With `HeadFromClone`, a clone that does not hold the head
  is refused as `object_unavailable` before anything is copied. The cache
  adapter reports it as `object_mismatch`, apart from other failures.
- **Host.** A sparse review with a linked clone first asks for the clone's
  head. A workspace the clone lends is returned with no download. Only when
  the clone lacks the head does the host download the snapshot and make the
  workspace as before. The actor half of the materialization moved into
  `materialize_on_actor` so it can run twice.
- **Words.** When the clone lacks the head and the download is too large, the
  reason is `too_large_not_in_clone`: "This head is larger than the agent's
  private copy allows, and your clone doesn't have it yet. Fetch the pull
  request's branch in your clone, then check again." (Ask again in the Agent
  tab's stop banner.)

Not changed: the download bounds, and the private copy for a review with no
linked clone, which still refuses a head this large.

## Acceptance

- AC1. A clone-only request against a clone without the head is refused as an
  unavailable object, leaves no workspace and writes nothing to the clone;
  against a clone that holds the head it copies nothing and is complete
  (Git process contract, real Git).
- AC2. A sparse review whose linked clone holds the head makes the workspace
  with no snapshot request, even when the download would be too large; a
  clone without the head is asked first and the snapshot is then downloaded
  as before; a head the clone lacks and the download refuses for size says
  `too_large_not_in_clone` (epoch host tests).
- AC3. The Agent tab's stop banner and the setup's check show the new
  reason's words (Qt launch panel tests; setup view shares the reason).
- AC4. The owner's Check again on their repository passes when their clone
  holds the head. Owner-only: their repository and machine.
