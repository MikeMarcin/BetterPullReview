---
id: BPR-0190
title: Compose the workbench application host into the application
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0189]
---

# Compose the workbench application host into the application

`github_application` composes three hosts. `workbench_application_host` is not
one of them, which is why BPR-0188 measured zero of its symbols in the shipped
binary.

## What this slice does

Construct it alongside the other three, with BPR-0189's adapters, the executors
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
