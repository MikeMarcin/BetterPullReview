---
schema: "implement-rfc.card.v1"
id: "BPR-1377"
title: "bpr tour add and show --source refuse every pinned-source read over the gh transport's sparse trees"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"BPR-1222's two-account course on MikeMarcin/BPR-Test#50 and #49, macOS, tour-cli build at e964b5a83 (2026-10-09)","reservation":"docs/issues/_reservations/BPR-1377--BPR-1378--58ecbdbf1b08.json","related_cards":["BPR-1222","BPR-1302","BPR-1327","BPR-1344","BPR-1359","BPR-1360"],"related_rfcs":["RFC-BPR-0044"]}
---

# BPR-1377: bpr tour add and show --source refuse every pinned-source read over the gh transport's sparse trees

## Observed

On macOS with the explicit gh transport, a fresh data root, and a draft
opened from a live pull request, every `bpr tour add PATH:FIRST-LAST
--transport gh` exits 5 `source_unavailable` ("The exact pinned source is
unavailable, changed, binary or not UTF-8; no working-tree text is
substituted.") after 28 to 44 seconds. No stop can be added, so no live tour
can be authored from the terminal over gh.

- Tests-off `tour-cli` build at `e964b5a83`, gh 2.101.0, account
  `MikeMarcin`. `connect --host github.com --transport gh` and `open
  https://github.com/MikeMarcin/BPR-Test/pull/50 --transport gh` succeed
  (draft `a0731b0a…`, revision 1, complete). `add
  bpr-2acct/tour/parser.py:13-21`, `:24-32`, `:1-3` and
  `bpr-2acct/tour/README.md:1-2` all refuse. The same on
  `MikeMarcin/BPR-Test#49` (`bpr-2acct/identity/review-me.txt:1-2`).
- The private Git cache holds the base and head commits and their trees
  (six objects) and none of the blobs: the gh smart transport fetched sparse
  commit trees.

## Where it fails

A throwaway diagnostic build (not committed) traced the refusal:

1. `materialize_review_tour_source` resolves the endpoints (`resolve` status
   available, blob, path and endpoint present).
2. `epoch_application_host::materialize` admits the read; its job returns
   `diff_blob_status::failure`.
3. In `begin_actor_blob_read` the blob is not cached (`object_mismatch`,
   selected-objects loading, sparse trees supported), and the request matches
   neither `SelectedBlobGrant` nor a `PreparedGrants` entry, so no remote read
   is set up (`read_remote_blob` sees `Remote` unset) and the read fails.

The desktop grants a blob when the reviewer selects the file; the tour CLI's
pinned-source read selects nothing, so on the sparse path it can never be
granted. BPR-1302's loopback course passed its pinned-source read on Linux
over native HTTPS, where the fetch carried the blobs, so the grant was never
needed there. The macOS gh rows of that course were not run.

## Acceptance

- `bpr tour add` and `show stop:ID --source` read exact pinned source over
  the gh transport when the cache holds sparse trees, without a desktop and
  without widening any other blob authority.
- A fixture or loopback row that serves sparse trees fails before the fix and
  passes after it.
- One live check on `MikeMarcin/BPR-Test` adds a stop and shows its source.
