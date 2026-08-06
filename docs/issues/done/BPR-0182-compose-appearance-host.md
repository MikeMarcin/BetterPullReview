---
id: BPR-0182
title: Compose the appearance application host into the application
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0181]
---

# Compose the appearance application host into the application

`github_application` composes `epoch_application_host`,
`diff_application_host`, and `github_application_host`. It does not compose
`appearance_application_host`, which is why BPR-0180 measured zero of its
symbols in the shipped binary.

## What this slice does

Construct the host alongside the other three, with the settings adapter from
BPR-0181, the packaged syntax classifier, the executors the application already
owns, and a projection port the window implements — the same shape
`epoch_projection_port` already uses.

Call `restore()` at composition so a stored selection is honoured at launch,
and honour the port's absent-record answer by keeping the composed defaults
rather than writing them back.

## Required direct evidence

- `nm` on the shipped binary reports `appearance_application_host` symbols.
  This is the measurement whose zero opened BPR-0180, and it is the one that
  says this card worked.
- A composition test asserts the application exposes an appearance host and
  that the host reaches quiescence on shutdown, in the manner the other three
  hosts are already asserted.
- A mode selected in one run is the mode resolved on the next, proven by
  running the application twice against one state directory rather than by
  reading the adapter back.
- A fresh state directory resolves System, and no record is written by merely
  launching. A launch that persists defaults would make every installation look
  deliberate.
- The host's shutdown participates in the existing orderly-shutdown evidence;
  a host that never quiesces is a hang at exit.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What was done

`github_application` now constructs `appearance_application_host` with
BPR-0181's settings store, its own `worker_executor`, the existing decision
lane, a projection sink, and a staged classification port. It calls `restore()`
at composition and `shutdown()` at close.

`nm ... | grep -cF appearance_application_host` goes from **0** to non-zero, and
`scripts/verify_composed_hosts.py` now reports `5 composed, 9 still absent`.

## Two staged pieces, both named rather than disguised

**`unlinked_syntax_classification`** answers every request `unavailable`. The
host cannot be constructed without a classification port, and BPR-0185 is what
links `BetterPullReview::AdapterSyntax`. `unavailable` is the port's own
default status and is simply true here: no classification is available, because
the classifier is not linked. It produces **no spans**, so nothing downstream
can mistake an empty answer for "this file has no tokens".

**`composition_appearance_projections`** holds what the host publishes.
`github_window` does not implement `appearance_projection_port`; BPR-0183 makes
the profile drive the composed window. The sink renders nothing and decides
nothing.

## The gate forced its own bookkeeping, as designed

BPR-0190's `composed_application_hosts` lists names that must be **absent** as
well as names that must be present. Composing the appearance host made the
absent rule fire:

```
COMPOSED_HOSTS_FAILED: appearance_application_host now appears in the binary.
If BPR-0182 composed it, move it to COMPOSED here and update its RFC
verification ledger in the same commit
```

That is the mechanism working. A list of things that must exist is satisfied by
adding more; a list of things that must not exist is what stops a ledger drifting
away from the product, which is the failure BPR-0180 and BPR-0188 documented
across six RFCs.

## Evidence

### Two breaks, each with 0 compile errors

| Break | Caught by | Assertion |
|---|---|---|
| Compose the host, leave the gate's lists alone | symbol gate | "now appears in the binary … update its RFC verification ledger in the same commit" |
| Host linked but never constructed | **runtime marker only** | `APPEARANCE_HOST_COMPOSED_FAILED` |

The second break leaves the symbol gate reporting `COMPOSED_HOSTS_OK`, exactly
as BPR-0190 measured for the workbench host. `nm` proves linkage; only the
running application proves construction.

### Gates

- `ctest --preset dev` — **123/123**. `ctest --preset core-only` — **57/57**.
- `coding-format-check` passed. `verify_documentation.py` — `DOCUMENTATION_OK`.
