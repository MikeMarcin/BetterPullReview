---
id: BPR-0236
title: Keep appearance and syntax composed when settings storage fails
status: Done
rfc: RFC-BPR-0006
depends_on: []
---

# Keep appearance and syntax composed when settings storage fails

The composition promises default appearance when the SQLite settings store
cannot open, but constructs the appearance host, profile, dispatch, and syntax
path only when that store exists. A store-open failure therefore removes the
behavior instead of selecting defaults.

## Required final state

- Compose the appearance host over an in-memory/default settings port when
  durable storage is unavailable.
- Preserve System-mode defaults, syntax classification, profile switching for
  the session, and an explicit persistence-unavailable diagnostic.
- A composition fixture forces store-open failure and proves the window still
  receives appearance and syntax projections.
- Cutting the fallback port or guarding classification on the durable store
  makes distinct assertions fail.

## Completion evidence

- The composition now chooses the durable SQLite appearance port when it opens
  and a session-only default port otherwise. The host, profile, settings
  dispatch, and syntax classification path are constructed in both cases.
- The Qt settings panel states that storage is unavailable, changes apply only
  for the session, and a later unavailable launch returns to System defaults.
  Its focused adapter test proves a later adoption cannot hide that status.
- `application_appearance_store_fallback_contract` obstructs only the exact
  appearance database path. Its first real-executable process proves fallback
  composition, session switching, profile-to-window application, and nonzero
  syntax adoption with zero durable writes. Its second process reuses the same
  application-data root and proves the `system:4` fallback default with
  persistence still unavailable.
- Focused verification passes: the application and panel targets build; the Qt
  panel, public selected-diff, durable appearance gesture, and new fallback
  contracts pass 4/4; the coding-format, documentation, and diff checks pass.
- The conservative code-simplification review found no safe reduction beyond
  keeping the fallback and persistence provenance local to composition.

## Documentation ledger

- `README.md` now describes the session-only System fallback and restart truth.
- `docs/verification/RFC-BPR-0006.md` records the direct executable row and
  leaves BPR-0237, BPR-0220, BPR-0233, and attended/ordered closeout claims
  explicitly open.
