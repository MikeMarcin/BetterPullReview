---
id: BPR-0236
title: Keep appearance and syntax composed when settings storage fails
status: In Progress
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
