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

## Read from the Linux side: bpr-main-linux, 2026-10-01

On Windows, `test_network_isolation_child` and `_external` now pass. This
card did not expect that. Reading `tests/scripts/network_isolation_counterexamples.py`
at main shows they cannot pass by skipping. The probe has no not-applicable
branch for either mode, and both rows are in `EXIT_VERDICT_TESTS`.

- **`external`** passes only if `connect_ex(("1.1.1.1", 443))` returns the
  backend's own denial errno and the backend's self-check confirms the
  isolation. On Windows the errno is WSAEACCES (10013), and the self-check is
  `windows_appcontainer_is_entered()`, which requires `TokenIsAppContainer ==
  1` on the process token. An uncontained process fails with
  `NETWORK_ISOLATION_EXTERNAL_NOT_ISOLATED:uncontained:0`. Both checks came
  with BPR-0347 (`d2ecc8d78`, 2026-08-17). So the "POSIX shape" this card
  describes was already gone at main. Every other path exits 1.
- **`child`** passes only if a grandchild Python, started with
  `subprocess.run`, prints `NETWORK_ISOLATION_EXTERNAL_DENIED`. So the
  grandchild was itself refused the network with an AppContainer token.

The two rows still prove the network denial, for the process and for its
child. Their recent Windows failures were the module failing to import
(`signal.SIGHUP`, [[BPR-1261]]), not the probe.

One thing for this card's premise. A pass means Windows let the contained
process start a child. [[BPR-0872]] recorded that as `child-process-denied`.
So either the launch policy has changed since then (for example, through the
`src/platform` launch restriction), or "denies a child" here meant the child's
network, which the row does prove. That can be settled only on Windows. The
evidence to look for is a run whose output carries
`NETWORK_ISOLATION_EXTERNAL_DENIED:10013` and `NETWORK_ISOLATION_CHILD_DENIED`.
The remaining acceptance item, a line explaining why these rows are not on
the uncontained pin list, is not in `tests/CMakeLists.txt` or
`scripts/verify_test_network_isolation.py` yet.
