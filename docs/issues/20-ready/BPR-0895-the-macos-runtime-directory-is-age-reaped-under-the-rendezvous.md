---
schema: "implement-rfc.card.v1"
id: "BPR-0895"
title: "macOS: the per-user runtime directory is age-reaped, so a long-idle owner can lose its rendezvous"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"dsp-006, as a consequence of BPR-0881's rendezvous relocation","found_at":"68f8152e","run":"run-20260916-stations","dispatch":"dsp-006"}
---

# BPR-0895: macOS: the per-user runtime directory is age-reaped, so a long-idle owner can lose its rendezvous

## Defect

[[BPR-0881]] moved the profile activation rendezvous out of the data root,
because the in-root path exceeded `sun_path` for every user name longer than
one character. It now lives in the first per-user runtime directory that
verifies owner-private: `/run/user/<uid>`, then `$TMPDIR`, then an
owner-private `bpr-<uid>` child of `/tmp`.

On macOS that resolves to `$TMPDIR`
(`/var/folders/<a>/<b>/T/`), and **macOS age-reaps that directory**. Linux's
`/run/user/<uid>` has no equivalent exposure: it is cleared at logout, which is
also when the owner process dies.

So on macOS a long-running owner whose socket is reaped keeps running while its
endpoint file disappears. The next launch finds no socket, cannot hand off, and
degrades to `owner_busy` — the exact symptom BPR-0881 fixed — until the owner
restarts. The owner's listening file descriptor survives; the *name* does not.

This is a narrower window than the defect it replaces: it needs an owner idle
long enough to be reaped, rather than any user name of two characters or more.
It is recorded because it is a known residual of a change made deliberately,
not a discovery waiting to happen.

## Scope

- Establish the real exposure before choosing a fix. macOS reaps `$TMPDIR`
  contents by access time, and the reaper's threshold and whether it skips
  sockets should be measured, not assumed from folklore.
- Then decide between the plausible answers:
  - have the owner keep its endpoint alive (touch it on a long interval, or on
    each accepted activation), which is the smallest change and matches how
    other long-lived Unix rendezvous survive reaping;
  - or have the *client* treat a missing endpoint as a possible reap rather
    than a possible absence, and give the owner a way to re-bind;
  - or choose a macOS location that is not reaped, which would mean giving up
    the per-user 0700 guarantee `$TMPDIR` provides for free — the reason it was
    chosen.
- Whatever is chosen must keep every guarantee BPR-0881 records: the
  private-storage root still gates the rendezvous, the socket is still
  `S_ISSOCK` + owner + `0600`, peer credentials are still checked, and the
  runtime directory is still verified owner-private without following links.

## Acceptance

- The card states the measured reaping behaviour — threshold, and whether
  sockets are exempt — rather than reasoning about it.
- An owner that has been idle past that threshold still accepts a handoff from
  a second launch, or the card records why that is not worth buying and what
  the user sees instead.
- Linux and Windows are unaffected.
- The contract covers whatever is chosen, so a future change cannot quietly
  reintroduce the gap.
