---
id: BPR-0472
title: The review-ledger composition's epoch handoff never completes on WSL2
status: In Progress
rfc: RFC-BPR-0007
depends_on: []
---

# The review-ledger composition's epoch handoff never completes on WSL2

`application_review_ledger_composition_contract` fails on the Ubuntu 26.04
WSL2 host (kernel 6.18.33.2-microsoft-standard-WSL2 x86_64, GCC 15.2.0, Git
2.53.0, Qt 6.10.3, ext4 worktree) with `epoch_handoff_failed=1` on the
`unavailable` course (epoch phase `single`), `render=1`, and `elapsed_ms` at
budget+12..22 ms. The same contract passes on macOS arm64 and in the Windows
matrix-11 record, and every RFC-BPR-0003 epoch suite passes on the same WSL2
host in the same run.

[[BPR-0156]] recorded this row on 2026-08-17 as "Budget expired at
40012-40018 ms against a 40000 ms bound. A sub-0.05% overrun; this host is
slower on that path than BPR-0050's guest." The 2026-08-28 Linux closeout
run for [[BPR-0054]] disproved that characterization by measurement rather
than assertion:

| Head | Budget | Result |
|---|---|---|
| `17c8950d` | 40000 ms | expired at 40018 ms, `epoch_handoff_failed=1` |
| `17c8950d` | 150000 ms | expired at 150019 ms, `epoch_handoff_failed=1` |
| `82c7d1c1` | 40000 ms | expired at 40014 ms, `epoch_handoff_failed=1` |
| `82c7d1c1` | 150000 ms | expired at 150012 ms, `epoch_handoff_failed=1` |

A merely slow path completes when given 3.75x the budget; this one never
does — the budget is only what ends the wait. The `82c7d1c1` rows ran in a
throwaway worktree at [[BPR-0156]]'s own baseline, before `e355895f`
(BPR-0361), `4af48e02` (BPR-0390), `8666fb54` (BPR-0397), and `5cb734d4`
(BPR-0459) touched the epoch surfaces, so those commits neither caused nor
fixed it. The behavior is pre-existing, WSL2-specific as far as measured,
budget-independent, and stable across three isolated re-runs.

This is not a general epoch hang; it is the forced-obstruction path. The
failing course is the third and last `bpr_run_review_ledger_probe` call:
`seed` and `reopen` pass, and the `unavailable` course deliberately obstructs
the ledger by replacing `state/v1/review-ledger.sqlite3` with a directory and
narrowing permissions, then asserts the obstruction survived. Under exactly
that condition the handoff never returns. The failure signature is
`PUBLIC_PATH_VERDICT: presentation=0 shutdown_complete=1 cleanup_failed=0
epoch_projection_failed=0 epoch_handoff_failed=1` with `PUBLIC_PATH_GROUPS:
core=0 fixture=0 maintenance=0 render=1`, and
`REVIEW_LEDGER_COMPOSITION_EVIDENCE:00000000000000` against the course's
expected `11111101111111` — all-zero, so the course never gets going rather
than stalling midway. `WORKBENCH_HOST_COMPOSED_OK` and
`APPEARANCE_HOST_COMPOSED_OK` still print, so composition succeeds and the
hang is downstream. Start at the obstruction handling. The RFC-BPR-0003
closeout review located the flag's raise sites around
`deliver_epoch_handoff`/`Epochs->accept` in
`src/application/src/github_application_host.cpp` (~lines 912-944 at
`812487ec`), so the question this card must answer first is whether the
obstructed-ledger course blocks inside RFC-BPR-0003-owned epoch-host code or
in the RFC-BPR-0007 composition around it; the RFC-BPR-0003 ledger's Linux
record closes over that named question and reopens through this card if the
answer is epoch-host code.

Reproduce with `-DBPR_RUN_BUDGET_MS=150000` against
`tests/end_to_end/application_review_ledger_composition_contract.cmake` on the
WSL2 host. The 2026-08-28 run logs were transient session scratch and are not
checked in; the durable evidence is this card's tables and the
[[BPR-0054]]/RFC-BPR-0003 final-replay record. A distribution note for
whoever takes this, corrected 2026-08-28 by direct observation on the host:
the WSL2 side holds no origin credential of its own (its fetch fails
`Permission denied (publickey)`), but the shared JaegAW checkout is fetched
from origin by the Windows side or the owner — fetches landed at 14:19 and
14:59 that day and `origin/main` reached `7160c283` with this card present,
while the WSL2-side local `main` lagged behind it. A session there reads
this card from `origin/main` (catching local `main` up is a no-network
fast-forward, coordinated so no concurrent Windows-side operation is mid
flight on the shared checkout); what the WSL2 side still cannot do is fetch
or push under its own credentials, so its evidence returns through the
tracker on another host or through the owner.

This card is deliberately independent of the RFC-BPR-0003 closeout: no
RFC-BPR-0003 claim row cites this contract, the four epoch commits above are
proven non-causal at baseline, and the RFC-BPR-0003 Linux suites pass on the
same host. What it owns: root-cause the never-completing epoch handoff in the
review-ledger composition on WSL2 (product defect versus harness versus WSL2
environment), correct or confirm the fixture, and replace [[BPR-0156]]'s
"slower host" attribution with the measured truth. Whether it also reproduces
on a full-system kernel is an open question this card should answer first.
