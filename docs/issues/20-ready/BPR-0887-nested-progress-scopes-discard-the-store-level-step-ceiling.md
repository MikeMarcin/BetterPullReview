---
schema: "implement-rfc.card.v1"
id: "BPR-0887"
title: "Nested progress scopes reset the handler to null, silently discarding the store-level step ceiling"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable sqlite partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0887: Nested progress scopes reset the handler to null, silently discarding the store-level step ceiling

## Defect

`binding_read_scope`, `registry_read_scope` and `bounded_read_scope` install a
step-bounded SQLite progress handler so an untrusted read cannot run away. But
every nested `detail::execute` installs its **own** `progress_scope`, and that
destructor resets the handler to **null** rather than restoring the previous
one (`src/adapters/sqlite/src/sqlite_connection.cpp:17-27`).

So after the first `execute` inside `establish` — `configure_connection`, for
instance — the ceiling is gone. The comment at
`src/adapters/sqlite/src/device_binding_store.cpp:55-56` says the bound "must
precede all untrusted reads"; from the second statement onward it does not
apply to any of them.

This is a bound that silently stops bounding, which is the failure mode a
bound exists to prevent. Nothing reports it and no test would notice, because
the handler's absence only shows up under a read that actually needs
stopping.

## Scope

- Make `progress_scope`'s destructor **restore the previous handler** rather
  than clearing it, so nesting composes. That is the smaller and more general
  fix, and it is why this is filed as a defect rather than a rearrangement of
  the call sites.
- Alternatively, install the store-level scope after `configure_connection` —
  but that only fixes the sites someone remembers, so prefer the first.
- Add a check that a nested `execute` leaves the outer ceiling in force. A
  bound with no test is how this happened.
- This was offered as a quick fix and is deliberately **not** being taken as
  one in this dispatch: it changes RAII semantics used across the SQLite
  adapter, and the safe version needs the test above plus a run of the SQLite
  contract suite. It is small, not trivial.

## Acceptance

- After a nested `execute` returns, the outer step ceiling is still installed,
  proven by a test.
- The SQLite contract suite passes on all three platforms.
- The comment at `device_binding_store.cpp:55-56` is true again.
