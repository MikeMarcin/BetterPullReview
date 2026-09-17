---
schema: "implement-rfc.card.v1"
id: "BPR-0884"
title: "The device binding store latches corrupt for the process lifetime, including on cancellation"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable sqlite partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0884: The device binding store latches corrupt for the process lifetime, including on cancellation

## Defect

`src/adapters/sqlite/src/device_binding_store.cpp:226-233` sets
`Failed = !established.has_value()` **before** checking
`aStop.stop_requested()`. And `establish` returns `corrupt()`/`unavailable`
for a `configure_connection` failure (`:270-271`) and for a cancelled or busy
`BEGIN IMMEDIATE`/migration (`:284-287`).

So a **cancellation** — or a transient busy database — latches the store as
corrupt. Every later `load`, `store` or `erase` then returns `corrupt` from
`ensure_open` (`:204-206`) without ever reopening, and the host turns that into
`binding_failure()`, failing the deferred session with `github_failure::vault`
(`src/application/src/github_application_host.cpp:4325-4340`). The user's
remembered device grant is unusable until the process restarts.

The two sibling stores do the opposite: `profile_registry_store.cpp:442-447`
and `window_review_restoration_store.cpp:516-521` close and retry.

The existing cancellation contract cannot see this, because it builds a fresh
store per call (`device_binding_storage_tests.cpp:132-138`) — the latch needs a
second call on the same store to appear.

**This needs a decision, not a silent change.** [[BPR-0841]] item 2
deliberately chose that "only a failure after admission latches", and
[[BPR-0541]]'s fail-closed rule is the reason this area is conservative. The
narrow claim here is only that *cancellation* and *busy* are not corruption
and should not latch, which is consistent with both.

This is adjacent to the open owner question on [[BPR-0644]] about whether a
remembered grant should survive a locked or denied Keychain — same feature,
different fault. Worth deciding together.

## Scope

- Do not latch on `cancelled`, and close the connection so a retry re-runs
  `establish`.
- Decide explicitly whether a transient `unavailable` (busy, locked) should
  latch; state the answer and its relation to BPR-0541's fail-closed rule.
- Extend the cancellation contract to call twice on the **same** store, which
  is what makes the latch observable.

## Acceptance

- A cancelled or busy open leaves the store usable on the next call, shown by a
  contract that reuses one store.
- A genuine post-admission corruption still latches, and that is still pinned.
- The card records how this interacts with BPR-0541 and BPR-0644.

## A second failure path in the same file, from the core+application review

Folded in here rather than carded separately, because it is the same file and
the same question — what a failure on this lane should do to the lane.

`src/adapters/sqlite/src/device_binding_store.cpp:139-157`: `submit()` catches
a failure from `Effects.post` and then calls `Decisions.post(...)` **outside
any try**. If that throws, the exception escapes `enqueue()` after the job has
already been pushed (`:112`). The job then sits at `Requests.front()` with no
scheduled completion; every later `enqueue` sees `Requests.size() != 1` and
never calls `submit()`, and `is_quiescent()` (`:189`) never returns true — so
host quiescence stalls for the rest of the process.

Only reachable on an executor allocation or refusal failure, which is why it is
not urgent. The repair is local: wrap the fallback post, and on failure pop the
job and complete it synchronously or mark it failed, so the lane stays live.

**Deliberately not taken as a quick fix in dsp-005.** The wrapping is trivial;
deciding what a failed job should report is the same decision this card already
owes for cancellation and `unavailable`, and making it twice in two passes is
how inconsistent failure semantics get built. Decide once, here.

Additional acceptance: a fallback `post` that throws leaves the queue drainable
and `is_quiescent()` reachable, shown by a test that injects the throw.
