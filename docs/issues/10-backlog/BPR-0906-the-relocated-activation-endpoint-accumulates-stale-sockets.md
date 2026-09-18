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

## Acceptance

- A full test sweep leaves no more endpoints behind than it started with, or
  the card records why that is not worth buying.
- Whatever is chosen cannot unlink a live owner's endpoint, and a row shows
  that.
- The decision is consistent with [[BPR-0895]]'s, and each card says how.
- Windows is unaffected, or the card says why it is not.
