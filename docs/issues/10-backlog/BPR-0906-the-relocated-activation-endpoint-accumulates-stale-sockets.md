---
schema: "implement-rfc.card.v1"
id: "BPR-0906"
title: "The relocated activation endpoint leaves stale sockets in the shared per-user runtime directory"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux"]
metadata: {"found_by":"bpr-linux, in its 369-row sweep after BPR-0881's relocation","found_at":"88dc42fc","run":"run-20260916-stations","dispatch":"dsp-007"}
---

# BPR-0906: The relocated activation endpoint leaves stale sockets in the shared per-user runtime directory

## Defect

[[BPR-0881]] moved the profile activation rendezvous out of the data root,
because the in-root path exceeded `sun_path`. That fix is right and measured.
This is its residual, and it is the exact mirror image of [[BPR-0895]].

The old endpoint lived at `<profile root>/activate.sock` and **died with its
root** — a test's temporary data root goes away and takes the socket with it.
The new endpoint lives in the per-user runtime directory, which **outlives
every root that uses it**.

Measured rather than reasoned: `bpr-linux`'s 369-row sweep left **two**
owner-private (`srw-------`), non-listening `bpr-activate-*.sock` files in
`/run/user/1000`. There were none before the sweep. The station removed its
own two so the host was left as it was found.

Harmless in practice on Linux — `/run/user` is tmpfs, cleared at logout; the
names are per-root digests so they do not collide; and stale-endpoint recovery
is proven. But they accumulate within a session, and nobody had recorded that
they do.

## The pair this makes, which is the interesting part

Two cards now describe the same relocation from opposite ends, and they pull
in opposite directions:

- **[[BPR-0895]]** — on macOS the runtime directory resolves to `$TMPDIR`,
  which the system **age-reaps**. A long-idle owner keeps running while its
  endpoint name disappears, and the next launch degrades to `owner_busy`: the
  symptom BPR-0881 fixed.
- **This card** — on Linux nothing reaps it, so dead endpoints accumulate.

So reaping is the hazard on one platform and the absence of reaping is the
hazard on the other. **Whatever is decided for BPR-0895 should be decided with
this in view**, because a fix that keeps the endpoint alive on macOS (touching
it, re-binding it) makes the Linux accumulation slightly worse, and a fix that
has the owner unlink on exit helps here and does nothing for a reaped name.
They are cheaper to answer together than separately.

## Scope

- Establish whether the owner already unlinks on an orderly exit, and if so
  what paths skip it — a crash, a `SIGKILL`, a test harness terminating the
  process, or a root removed underneath it. The sweep's two survivors say at
  least one path does.
- Decide between the plausible answers, preferring the smallest: unlink on
  orderly exit and leave the rest to stale recovery; or have a launch sweep
  its own directory for endpoints whose digest names a root that no longer
  exists; or accept the accumulation on Linux and record why.
- Keep every guarantee [[BPR-0881]] records: the private-storage root still
  gates the rendezvous, the socket is still `S_ISSOCK` plus owner plus `0600`,
  peer credentials are still checked, and the runtime directory is still
  verified owner-private without following links.
- Do not let a cleanup step become a way to unlink somebody else's endpoint.
  The digest names are per-root and the directory is shared by every profile
  the user runs; a sweep that is wrong here takes a *live* owner's name away,
  which is worse than the accumulation it fixes.

## Measured on Linux: scope item 1 is answered

`bpr-linux` (dispatch `dsp-007`, `bpr-linux/int-16`) produced every path
below on the host rather than reasoning about it. Full evidence, commands and
exit statuses:
[BPR-0906 Linux measurement](../../verification/BPR-0906-linux-endpoint-exit-paths.md).
**No fix was chosen or implemented** — that stays joint with [[BPR-0895]].

**A survivor is reclaimed, not refused,** so the accumulation is inert. The
next start on the same root proves the leftover is dead — socket, owner, `0600`,
and a probe connection that must be refused — and only then unlinks and rebinds
(`src/adapters/sqlite/src/profile_activation.cpp:413-427`). A live owner answers
that probe and is left alone, measured. Two boundaries qualify it: the reclaim
only helps when a root is *reused*, and the harness never reuses one, so the
debris is never reclaimed by anybody; and a leftover that is *not* a socket is
refused forever, which is the shape that would make this harmful.

The owner does unlink on an orderly exit, in `~native_server()`
(`profile_activation.cpp:263-280`). The paths that skip it:

- **A crash (`SIGABRT`, `SIGSEGV`), `SIGKILL`, `SIGTERM`, `SIGINT`, `SIGHUP`** —
  the unlink is a destructor and there is no signal handler anywhere in `src/**`.
- **A CTest `TIMEOUT`** — measured to kill the whole process group uncatchably;
  `TIMEOUT_SIGNAL_NAME` is set nowhere in this tree.
- **An orderly exit whose data root was removed, or replaced, underneath it** —
  the destructor runs, exits 0, prints `OWNER_RELEASED`, and still declines,
  because the lease guard can no longer prove it owns the root
  (`src/adapters/sqlite/src/catalog_database.cpp:4378-4392`). That guard is
  correct and is what stops an owner unlinking a name it no longer owns, but it
  means *orderly exit* alone does not imply cleanup.
- **A failed assertion inside a live owner**, which calls `std::exit` and so
  skips `main`'s automatic destructors
  (`tests/end_to_end/profile_activation_public_path_tests.cpp:22-27`).

**A second leaking row was found, and it is not an activation row.** Every
owner-only start binds an endpoint (`src/app/src/github_application.cpp:12765`),
so any row that starts a real owner and then signals it leaks one.
`application_attended_launch_contract` does exactly that: an attended launch is
designed never to exit on its own, so the test must signal it, at
`tests/end_to_end/attended_launch_contract.py:150`. Two consecutive **passing**
runs left two different endpoints. The activation rows themselves leave none.
So the sweep's two survivors should not be assumed to be the activation rows'
doing, and a fix aimed only at those rows would miss this.

## Acceptance

- A full test sweep leaves no more endpoints behind than it started with, or
  the card records why that is not worth buying.
- Whatever is chosen cannot unlink a live owner's endpoint, and a row shows
  that.
- The decision is consistent with [[BPR-0895]]'s, and each card says how.
- Windows is unaffected, or the card says why it is not.

## BPR-0895 decided: no keep-alive, and macOS accumulates too — 2026-09-23

[[BPR-0895]] observed that macOS's daily `$TMPDIR` age reap skips sockets, so
an idle owner keeps its endpoint and no product change was made there. Nothing
keeps an endpoint alive or re-binds it, so this card's choice (unlink on exit,
a launch sweep, or accepting the debris) is unconstrained by that one.

The same observation measured this card's residual on macOS. On 2026-09-23 the
Mac integrator's `$TMPDIR` held 20 `bpr-activate-*.sock` entries born between
2026-09-19 and 2026-09-22, none held by any process (`lsof -U`). They are
test-run debris that the reap leaves because it skips sockets, so macOS
accumulates them just as `/run/user/<uid>` does. A reboot clears both.
