---
id: BPR-0182
title: Compose the appearance application host into the application
status: Backlog
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
