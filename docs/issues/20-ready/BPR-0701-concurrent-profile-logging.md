---
schema: "implement-rfc.card.v1"
id: "BPR-0701"
title: "Allow independent diagnostic logging for concurrent profiles"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0696"]
---

# Allow independent diagnostic logging for concurrent profiles

## Scope

The logging adapter's process-wide backend_owned flag makes every logger after
the first unavailable. Replace exclusive ownership with explicit shared backend
lifetime and separate logger identities/sinks. Each profile retains its private
file/cap/failure state. Closing one logger must not stop another; the final owner
stops the backend. Preserve bounded nonblocking record ingress, private-path
refusal, file caps and sanitization. This fixes current different-profile
composition; later same-profile windows will share one profile logger port.
The existing unavailable Windows sink is outside this repair.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Primary owns logging adapter/tests and acceptance. Independent queue/workbench
workers continue within their separate paths.

## Acceptance

Two logger instances under separate private roots record only their own events;
closing either first leaves the other recording and final close permits reopen.
An unavailable/full sink does not disable a healthy sibling. Existing logging
and bounded-backpressure checks pass. Independent review covers shared backend
lifetime, failure routing and close/record concurrency.

## Evidence

Source inspection confirmed early refusal at backend_owned.test_and_set in
quill_log construction. No implementation or new check has run for this card.
