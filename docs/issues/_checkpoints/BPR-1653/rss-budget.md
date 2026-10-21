# BPR-1653 measured Ubuntu RSS budget

Owner: `/root/ubuntu_rss_fix`; mandate `bpr1653-measured-ubuntu-rss-budget-r1`.
The 2026-10-20 owner amendment on the live card replaces the causal-repair/
unchanged-RSS requirement with a measured modest RSS budget adjustment. This
is budget resolution, not evidence of reduced memory use or a repaired leak.

## Bounded plan and current frontier

1. Preserve abandoned ownership/storage candidates and adopt authoritative M3
   source without their runtime changes. Complete: base
   `f25a65274763066ed4519aa6aec6283ae1c0a4cb`; old unaccepted lineage remains at
   `codex/bpr1653-pre-budget-7830349`. The exact nine-path uncompiled storage
   candidate is externally sealed with SHA-256
   `3b8114a573ba096d2ac555f3c8b35ed68f51ccffba5491ee3be27ce75ca142e0`.
2. Under an exclusive quiet physical-PC lease, run one uninstrumented registered
   original-216-MiB course. Complete: actual CTest exit 8 in 48.8543 seconds;
   Qt 6.12.0, GCC 15.2, Ubuntu 26.04 x86_64, Release, style windows, scale 1.
3. If close, change only Linux cache-byte RSS/VmHWM growth to 240 MiB while
   retaining the independent 216 MiB live-heap ceiling. Implemented; focused
   clang-format 22.1.8 and all 67 documentation controls pass; complete native
   confirmation and final documentation gate pending.
4. Rebuild the affected benchmark target, execute the complete registered course
   once, retain every result, self-review and commit explicit paths. Pending.
5. Return exact source/binary/cache/raw/JUnit identities and a process-drain
   receipt to M3. Integration/release/package claims stay coordinator-owned.

## Original current-source observation

Binary SHA-256:
`4e5ed6c3f795652a64078aba1ca4b57280e88b3dc3ed45493fff709b52fe5f38`.
Cache SHA-256:
`a2c445454000aad224a245821dff46a1fe1dc005b5ccfcf75937a98c35e50618`.
The original registered row retains RUN_SERIAL true, TIMEOUT 900, skip 77 and
`LARGE_REVIEW_CACHE_BYTES_OK`. The original run has AllocationProfile false,
BudgetsEnforced true and no acceptance marker; later lifecycle assertions are
unreached, not passing.

The before-application VmRSS baseline is 29585408 bytes. At selection 18,
absolute sampled VmRSS and process-lifetime VmHWM are both 256098304 bytes;
growth above the same original VmRSS baseline is 226512896 bytes, against
226492416. The 20480-byte excess is 0.00904%. Linux memory observation is
unchanged `/proc/self/status` VmRSS/VmHWM; live heap remains glibc mallinfo2
uordblks+hblkhd. There is no allocator-free-byte subtraction or baseline shift.

The 21 original `resource-sample:` rows include the failed row because stderr
emission precedes the predicate. CTest repeats those identical 21 rows in its
verbose failure output; this is one course, not 42 observations or two runs.
The partial JSON stops before appending the failed sample.

Raw source/binary/config/command/JUnit/metrics/log receipt archive SHA-256:
`6373842563549ec3105bc90c5a637aded07bd72f1d5367909b64d26d2607b7db`.
Native evidence: `/home/mmarcin/bpr1653-budget-evidence/original-216-r1`.
Local evidence: `/tmp/bpr-qt612-run/ubuntu-rss/original-216-r1`.
Durable external continuation: `/tmp/bpr-qt612-run/ubuntu-rss-followup-handoff.md`.

## Preserved boundaries

240 MiB is 251658240 bytes, 24 MiB/11.11% above the old RSS bound. It leaves
23.9805 MiB over this failed sample and 20.4141 MiB over the published-source
uninstrumented 219.5859 MiB observation. The complete adjusted course must still
demonstrate the later high-water mark and every remaining assertion. No
instrumented R6/R6g or latched supplemental peak supplies budget calibration.

Mac/Windows ceilings, the ordinary resource-course ceiling, independent heap
216 MiB, cache 64 MiB/64 entries, admission/accounting, fixture/workload,
ownership/cancellation, selection/epoch outcomes, eviction/revisit/supersession,
close/shutdown/destruction, cleanup/input checks, registration and deadline
remain unchanged. No production/core/application/Qt source is changed.

The PC lease remains with this implementer through the bounded confirmation.
M3 has no competing jobs; both independent preflight process audits were empty.
Published tags/assets/producer roots and unrelated PID 37820 are untouched.
The two unrelated dirty MASK files retain their original hashes. No main
integration, publication, version change or broad qualification is claimed.

The documentation gate first refused the old M3 NEXT_ID1677 against canonical
historical high water 1678. Separate commit `5be0fe99f` copies only NEXT_ID and
the existing BPR-1677 reservation from canonical `7a96faade`; no allocation or
independent counter increment occurred. The current milestone-checkpoint helper
also refuses the historical release-v030 plan because two unrelated old
milestones lack its now-required boundary field. That refusal is retained;
the historical plan is not rewritten to fit this narrow budget slice. The
explicit bounded plan and owner amendment above govern this continuation.
