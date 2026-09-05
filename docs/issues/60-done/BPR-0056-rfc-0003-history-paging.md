---
schema: "implement-rfc.card.v1"
id: "BPR-0056"
title: "Implement bounded epoch history paging"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0041","BPR-0044","BPR-0045"]
completed: "2026-08-01"
---

# Implement bounded epoch history paging

Complete the RFC-BPR-0003 history algebra that is absent from the initial
core slice. Add an exact `request_epoch_history_page` intent, core validation
of catalog identity/cursor/limit, a typed host request and matching terminal
events, and provider/Git/SQLite-free incremental page projections.

The application host must execute only bounded catalog pages, serialize each
completion, and reject stale authority, source, generation, incarnation,
revision, cursor, duplicate, failed, and cancelled results. Initial load and
continuation pages must preserve never-reused stable epoch identities and
must not copy the complete history into core state or reset an established Qt
model. The Qt page trigger and row model remain BPR-0046.

Use core transition tables and manual-executor fake-catalog tests for empty,
one-page, multi-page, exact-bound, invalid-cursor, revision-change,
identity-switch, cancellation, and reordered completion cases.

## Result

Implemented one core-owned revision-bound history cursor, page-size maximum,
intent, request, terminal-event family, transient page delta, and compact
pending/continuation state. The core validates exact application session,
pull-request authority, accepted source, epoch generation, catalog
incarnation/revision, cursor, request, and 1..100 limit. Pages must contain
valid stable epoch identities in strict descending ordinal order. A
continuation is accepted only on a full page and must equal its final ordinal;
the next page must remain strictly below it. Initial and in-flight duplicates,
skipped cursors, oversized or malformed pages, and stale or mismatched
terminals fail closed. Catalog conflict cancels the page and starts the same
bounded catalog-reload path used by the other epoch lanes.

The application host executes the existing bounded catalog `history` request
on the repository actor, converts success/failure/cancellation into typed core
events on the decision lane, and relies on core acceptance before publication.
The provider/Git/SQLite-free projection exposes page phase, requested cursor
and limit, sanitized stable rows, continuation, and reason. It never exposes a
source-observation identity and never sets the catalog projection's model
replacement flag for normal paging. Core state retains only the accepted
catalog and continuation, never page rows or complete history.

The transition audit found and corrected a pre-existing changed-domain
composition hazard exposed by paging: refresh and catalog activation/commit
could overwrite a cancellation's history bit. Those paths now combine changed
domains, so a pending page and the authority/catalog change are both visible
in the same deterministic transition.

## Simplification disposition

The required focused simplification pass removed the application layer's
competing cursor definition and page-size constant in favor of the single
core semantic types, reused one generic failure/cancellation terminal helper,
and retained explicit page validation and host terminal branches because they
encode different stale, catalog-conflict, and cancellation behavior. The test
tables remain deliberately explicit by page shape and identity failure; no
broader rewrite was justified.

## Completion evidence

- The focused core, application contract/host, and real SQLite storage/
  behavior matrix passed 6/6 in 21.58 seconds. It directly covers empty,
  one-row, multi-page, exact-bound, invalid/skipped cursor, malformed order and
  bounds, exact continuation, duplicate, failure, cancellation, catalog
  revision drift, authority/source/generation replacement, retry, and a loaded
  terminal deliberately reordered after source replacement.
- The complete Qt-enabled development graph rebuilt all 47 affected targets,
  including the application and Qt consumers of the changed public headers.
- The Qt-free graph rebuilt all 33 affected targets. Its direct core/host/
  SQLite/architecture matrix passed 8/8 in 169.33 seconds; both architecture
  scans passed, including the independent hostile-mutation contract.
- The projection contract statically rejects source-observation fields on
  `epoch_row_projection`. Manual-executor assertions require stable row IDs,
  exact request/cursor/limit, `Replace == false`, no changed-row replacement,
  and no stale page publication.
- `python3 scripts/verify_documentation.py --root .` reported
  `DOCUMENTATION_OK`; documentation structure and issue-graph contracts passed
  2/2. `git diff --check` passed.
- Tracked and on-disk GitHub Actions workflow counts remained 0/0. No GitHub
  mutation, private repository, credential, new dependency, schema change,
  package surface, feature option, or target-native Windows claim is involved.

## Documentation disposition and deferred boundaries

Updated the system, Git/epoch, and security architecture, build/test guide,
fixture catalog, issue board, and RFC-BPR-0003 verification ledger for the
bounded intent/request/terminal path, compact core state, revision-conflict
reload, sanitized incremental projection, and direct test matrix. Cache
operator guidance is `no-impact`: paging adds no credential, network, Git,
filesystem, process, deletion, or durable-schema authority.

BPR-0046 retains the Qt page trigger and stable row model. BPR-0047 retains
selection through public composition, BPR-0048 the broad combined fault
matrix, BPR-0055 core-authorized prune/cache-delete lifecycle work, and
BPR-0053 target-native Windows evidence. No RFC-BPR-0004 diff, RFC-BPR-0005
presentation-polish, RFC-BPR-0006 theme, or GitHub Actions work is claimed.
