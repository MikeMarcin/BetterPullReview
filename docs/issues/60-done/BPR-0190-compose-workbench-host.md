---
schema: "implement-rfc.card.v1"
id: "BPR-0190"
title: "Compose the workbench application host into the application"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0189","BPR-0192"]
---

# Compose the workbench application host into the application

`github_application` composes three hosts. `workbench_application_host` is not
one of them, which is why BPR-0188 measured zero of its symbols in the shipped
binary.

## What this slice does

Construct it alongside the other three, with the settings adapter from
BPR-0189 and the observation adapter from BPR-0192, the executors
the application already owns, and a projection port the window implements —
the shape `epoch_projection_port` already uses and `workbench_window` already
implements.

Call `restore()` at composition so a stored drawer layout is honoured at
launch, and honour the host's contract that restoring writes nothing back.

This card composes the **host**. BPR-0191 moves the window. Splitting them
keeps a large window change from being entangled with a host lifecycle change,
so that if the window move needs revising, the host composition is not
relitigated with it.

## Required direct evidence

- `nm` on the shipped binary reports `workbench_application_host` symbols. This
  is the measurement whose zero opened BPR-0188.
- A composition test asserts the application exposes the host and that it
  reaches quiescence on shutdown, as the other three hosts already are.
- The host's shutdown participates in the existing orderly-shutdown evidence.
  A host that never quiesces is a hang at exit, and this application already
  has an intermittent open-stage stall on record — do not add a second way to
  hang.
- A drawer layout selected in one run is the layout restored on the next,
  proven by running the application twice against one state directory rather
  than by reading the adapter back.
- A fresh state directory writes no layout record merely by launching.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What was done

`github_application` now constructs `workbench_application_host` alongside the
other three, with BPR-0189's settings store, BPR-0192's observation adapter, its
own `worker_executor`, the existing decision lane, and a projection sink. It
calls `restore()` at composition and `shutdown()` at close.

The measurement that opened BPR-0188:

| | before | after |
|---|---|---|
| `nm ... \| grep -cF workbench_application_host` | `0` | **1393** |

## The projection sink is intermediate, and says so

`github_window` does not implement `workbench_projection_port`; only
`workbench_window` does, and it is not the application's window until BPR-0191.
So the host publishes to `composition_workbench_projections`, which retains the
last projection and counts what it received.

That is a real intermediate state and it is named rather than disguised. The
host is genuinely composed — it restores a durable layout, runs its effects,
and quiesces on shutdown — but nothing renders what it publishes yet. The sink
decides nothing and draws nothing, so it is not a second presentation
authority, and BPR-0191 replaces it with the shell.

## A hang this card introduced and then caught

Composing the host without adding it to `begin_shutdown` made the application
**never exit**. `poll_shutdown` waits for every host to report quiescent, and a
host that was never told to stop never does.

It presents as a test timeout naming nothing. `application_diff_workbench_public_path`
simply stopped finishing, and the cause is three functions away from the
symptom. The comment at the call site now says so, because the next host added
to this composition will have the same failure mode.

## The gate this card leaves behind, and its limit

`scripts/verify_composed_hosts.py` asks the linker which application hosts the
shipped binary contains, and is registered as `composed_application_hosts`. It
enforces **both** directions: a name under `COMPOSED` must be present, and a
name under `NOT_COMPOSED` must be absent. The second rule is the load-bearing
one — a list of things that must exist is satisfied by adding more, while a
list of things that must not exist forces this file and the RFC ledgers to be
updated in the same commit that composes something.

**It cannot see whether a composed host is ever constructed.** That was
measured, not assumed: disabling the host's construction outright left the gate
reporting `COMPOSED_HOSTS_OK`, because the constructor's code is compiled and
its symbols are present whether or not any path reaches it. The docstring says
this plainly rather than claiming more than the check delivers.

The runtime half is the application's own `WORKBENCH_HOST_COMPOSED_OK` marker,
asserted by the fixture contract. The same break that the symbol gate could not
see produces `WORKBENCH_HOST_COMPOSED_FAILED`.

## Evidence

### Four breaks, each with 0 compile errors

| Break | Caught by | Assertion |
|---|---|---|
| A composed host declared but absent | symbol gate | "declared composed but has no symbols in the binary" |
| A not-yet-composed host appears | symbol gate | "now appears in the binary … move it to COMPOSED here and update its RFC verification ledger" |
| Host linked but never constructed | **runtime marker only** | `WORKBENCH_HOST_COMPOSED_FAILED` |
| (the same break) | symbol gate | **not caught** — recorded, and the reason the runtime marker exists |

The third and fourth rows are the same edit. Listing it twice is the point:
one check missed it and one caught it, and a reader deciding whether the symbol
gate is sufficient deserves to see that it is not.

### Gates

- `ctest --preset dev` — **122/122**, up from 121 with `composed_application_hosts`.
- `ctest --preset core-only` — **56/56**.
- `coding-format-check` — 283 files, passed.
- `verify_documentation.py` — `DOCUMENTATION_OK`. `git diff --check` clean.
