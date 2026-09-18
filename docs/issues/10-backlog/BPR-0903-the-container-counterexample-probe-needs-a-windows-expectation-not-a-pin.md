---
schema: "implement-rfc.card.v1"
id: "BPR-0903"
title: "Windows: the container counterexample probe needs a platform expectation, and must never be pinned uncontained"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-win, while measuring BPR-0872's eleven rows uncontained","found_at":"d6dfca2b","run":"run-20260916-stations","dispatch":"dsp-006"}
---

# BPR-0903: Windows: the container counterexample probe needs a platform expectation, and must never be pinned uncontained

## Defect

`test_network_isolation_child` fails on Windows. It was **deliberately left out**
of [[BPR-0872]]'s containment batch, and this card exists so that decision is
recorded rather than re-litigated by whoever meets the red row next.

It must never be pinned `WINDOWS_UNCONTAINED`. Its whole job is to prove the
container **denies** a child process. Running it outside the container would
make it prove nothing, while turning the manifest gate green — the worst
possible outcome, because the row would then be actively misleading rather than
merely red. It is in `EXIT_VERDICT_TESTS` besides.

It was also deliberately **not measured** uncontained, which is the right call:
measuring it would have produced a number that invited exactly the wrong fix.

What it needs is a Windows-specific expectation. The denial it asserts is real
and is exactly what [[BPR-0872]] measured as `child-process-denied` — the row
and the eleven pinned rows are seeing the same fact from opposite sides. The row
simply expects the POSIX shape of that denial.

## Scope

- Give the probe a platform expectation for how an AppContainer refuses a child:
  `CreateProcess` failing with WinError 5, rather than whatever POSIX form is
  asserted today.
- Keep it contained. Keep it in `EXIT_VERDICT_TESTS`.
- While there, check its sibling `test_network_isolation_external` for the same
  platform assumption; the two are registered in the same `foreach`.
- Add a line to the row or the gate saying why it is not on the uncontained pin
  list, so the next person does not have to rediscover the argument.

## Acceptance

- The probe passes on Windows and still proves the denial rather than asserting
  nothing.
- It is still contained and still in `EXIT_VERDICT_TESTS`.
- The reason it is not pinned uncontained is written where a reader of the pin
  list will find it.
- macOS and Linux behaviour is unchanged.
