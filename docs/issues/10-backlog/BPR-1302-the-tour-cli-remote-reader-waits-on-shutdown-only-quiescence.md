---
schema: "implement-rfc.card.v1"
id: "BPR-1302"
title: "The tour CLI's live remote reader waits on shutdown-only host quiescence, so online open and connect never return"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"found_by":"bpr-main-linux, BPR-1068's Linux run, 2026-10-06","reservation":"docs/issues/_reservations/BPR-1302--BPR-1302--bb2b61bce2e0.json","related_rfcs":["RFC-BPR-0044"],"related_cards":["BPR-1068","BPR-1055","BPR-1059","BPR-1222"],"blocks":"AC44-12 (first-run auth setup) and every live use of bpr tour open, refresh, publish and recover"}
---

# BPR-1302: The tour CLI's live remote reader waits on shutdown-only host quiescence, so online open and connect never return

Found by bpr-main-linux on 2026-10-06 during [[BPR-1068]], at `112665f81`. That commit
was published as `3632c4035`; the line numbers below are unchanged by the rename.

## Observation

`bpr tour open https://github.com/MikeMarcin/BPR-Test/pull/1 --json` on a
fresh profile that has never signed in, from the installed TourCLI tree
(Linux, WSL2, GCC 15.2, `tour-cli` preset, no display, no session bus),
printed nothing and did not exit for over 120 seconds. SIGINT ended it with
exit 130. It should have refused `authentication_required` (exit 5) at once.

A temporary instrumented build (not committed) showed where:

```
DBG restore start
DBG restored 1            # device_profile_restore_status::unavailable: no binding
DBG waiting restored=1 quiescent=0 effects_idle=1   # repeated until SIGINT
```

`remote_reader::open` in `src/app/src/tour_remote.cpp` waits for
`restored && Host->is_quiescent()`. `github_application_host::is_quiescent()`
begins `return Implementation->Closing && ...`: it is the "safe to destroy
after shutdown" predicate, and `Closing` is set only by `shutdown_now()`. So
while the host runs it is always false, and the wait ends only when SIGINT
shuts the host down. `epoch_application_host::is_quiescent()` has the same
`Closing` guard.

## Affected waits (all in `tour_remote.cpp`)

| Line at `112665f81` | Path | Effect |
|---|---|---|
| 94 | `connect --transport gh`, after `activate_host_profile_intent` | never returns (macOS, the only platform where gh is composed) |
| 120, 122 | `open` native: restore, then settle | never returns; observed on Linux |
| 127 | `open` (both transports), after `open_pull_request_intent` | never returns |
| 203 | first pinned-source read, after `Epoch->accept` | never returns |
| 327 | device `connect`, after the flow | inferred: would not return after a person authorized, although the grant and binding would already be stored |

Line 344, in `stop()` after `shutdown()`, is the one correct use.

The fixture courses (`tour_cli_composition`, `tour_cli_publication`,
`tour_cli_terminal_publication`) inject a reader factory and never construct
`remote_reader`, and no ledger records a live `bpr tour open` or `connect`
on any platform, so nothing has exercised these waits.

## Scope

- Wait on what each step is for: the restore callback; the session leaving
  `PendingSession` (accepted or failed); the pull request leaving
  `PendingPullRequest` with a snapshot or a failure; the epoch handoff
  accepted. If the host has no running-idle predicate, add one beside
  `is_quiescent()` rather than reusing the shutdown one.
- A course that constructs the real `remote_reader` against a loopback
  provider (the existing native HTTPS test seam, absent with
  `BUILD_TESTING=OFF`): a never-signed-in `open` refuses
  `authentication_required` promptly; a fixture session and pull request
  open and return; and the same course with the old waits hangs.
- No live GitHub is needed to fix it. Afterwards, live qualification uses
  only `MikeMarcin/BPR-Test`.

## Acceptance

- `bpr tour open URL --json` on a never-signed-in profile exits 5
  `authentication_required` within seconds, on Linux, macOS and Windows.
- The loopback course passes, and fails with the old waits restored.
- [[BPR-1068]]'s device-flow completion on Linux (owner enters the code)
  then returns `Connected account …`, and the following `open` of a
  `MikeMarcin/BPR-Test` pull request returns its draft.
