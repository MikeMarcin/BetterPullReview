---
schema: "implement-rfc.card.v1"
id: "BPR-0701"
title: "Allow independent diagnostic logging for concurrent profiles"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0696"]
acceptance: [{"criterion":"All bounded card acceptance criteria","outcome":"pass","revision":"HEAD plus explicit finish paths","evidence":{"kind":"observation","detail":"Logging and bounded backpressure tests pass 0.61s including real overflow regression that failed before fix; changed logging TUs tidy pass. Rebuilt application_profile_management_public_path passes 1.38s and verifies startup/shutdown logs in three private profile roots. Medium review overflow finding fixed; no Windows sink claim."}}]
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

A bounded shared backend lease now admits separate logger identities and private
sinks; only the last owner stops Quill. Tests cover both close orders, reopen,
failed/full siblings and four concurrent admissions/drains. Independent medium
review identified informational overflow notifications poisoning shared status.
An actual bounded-queue overflow regression failed before the correction and
passes after filtering Quill's informational/nonfatal warning notifications.
Logging and backpressure tests pass together (0.61s); both changed translation
units pass warnings-as-errors tidy. No Windows sink capability is claimed.

Logging and bounded backpressure tests pass 0.61s including real overflow regression that failed before fix; changed logging TUs tidy pass. Rebuilt application_profile_management_public_path passes 1.38s and verifies startup/shutdown logs in three private profile roots. Medium review overflow finding fixed; no Windows sink claim.
