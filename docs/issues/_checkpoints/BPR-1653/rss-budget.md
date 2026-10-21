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
   clang-format 22.1.8, documentation structure and all 67 documentation
   controls pass.
4. Rebuild the affected benchmark target, execute the complete registered course
   once, retain every result, self-review and commit explicit paths. Complete
   at `d6ac721037e242ef7ef4b934a003a26b9c60534a`; final tracker upkeep changes
   only this checkpoint and the card's status/evidence.
5. Return exact source/binary/cache/raw/JUnit identities and a process-drain
   receipt to M3. Complete: M3 explicitly accepts the returned PC/WSL lease;
   integration/release/package claims stay coordinator-owned.

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

The PC lease is returned to M3 after evidence collection/drain. Both independent
preflight and final process audits are empty; no active command receipt remains.
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

## Complete adjusted-course result

The benchmark-only rebuild at exact candidate
`d6ac721037e242ef7ef4b934a003a26b9c60534a` passes in 37.5041 seconds. Its log
shows only the benchmark dependency scan, benchmark translation unit and link.
Candidate benchmark source SHA-256:
`caf5c78a6534aa26f25ea9bae7a2a91f35bc12f4d461d1f4608f95331dde0595`.
Candidate binary SHA-256:
`20693051715f6a5e57f9193dc4027bc52f2e3899a401965c6df229242f4968e0`.
The CMake cache remains the exact original `a2c44545` hash recorded above.

One complete uninstrumented adjusted course passes in 51.3084 seconds, CTest
exit 0 with `LARGE_REVIEW_CACHE_BYTES_OK`, AllocationProfile false and
BudgetsEnforced true. Its 26 original resource samples include every later
cleanup/destruction phase. No retry, timing attribution qualification, metric
relabeling or diagnostic latching supplies the pass.

| Metric | Original 216 MiB course | Complete 240 MiB course |
|---|---:|---:|
| before_application absolute VmRSS | 29585408 | 30027776 |
| maximum sampled absolute VmRSS | 256098304 | 265170944 |
| maximum sampled VmRSS growth | 226512896 | 235143168 |
| maximum absolute process-lifetime VmHWM | 256098304 | 269336576 |
| maximum VmHWM growth above initial VmRSS | 226512896 | 239308800 |
| outcome | FAIL at selection 18; later phases unreached | PASS through destruction |

The complete sampled growth is 224.25 MiB; lifetime growth is 228.22265625 MiB,
11.77734375 MiB below the new bound. This later observed high-water replaces
neither the original failure nor the original baseline. The implementation
changes the existing Linux `#else` platform scalar; Ubuntu/WSL is the measured
station. Other Linux distributions/native environments remain unqualified.

The independent 1 ms live-heap sampler reports peak 166620864 minus baseline
269808, growth 166351056, against unchanged 226492416. Its sample intervals
remain lower-bound observations, not proof against unsampled peaks. Maximum
observed cache bytes are 66888888, below unchanged 67108864. The course completes
all 20 selections and 32 retained epochs, exact source/rendering, byte/index
eviction, revisit, held supersession, live loaded close with epoch storage,
shutdown and destruction. Its final resource row has zero cache bytes/entries
and diff/epoch jobs; final live-heap growth is 2388032 bytes. Unrequested
maintenance/resource-course fields are not claimed as passing this course.

Adjusted raw archive SHA-256:
`61fd69af1d46125643ec2c67cfbb1d1673cdc585e4ad2d68d684a67b65d53eb0`.
Native evidence: `/home/mmarcin/bpr1653-budget-evidence/adjusted-240-r1`.
Local evidence: `/tmp/bpr-qt612-run/ubuntu-rss/adjusted-240-r1`.
Both original and adjusted binaries, source/configuration snapshots and raw
receipts are preserved separately. All earlier failed controls and abandoned
candidate references remain preserved. Final bookkeeping changes do not alter
the tested benchmark or any production execution input.

Complete native evidence archive (45992134 bytes, including both ELF binaries,
source/cache/preset snapshots, build log, commands, raw courses, JUnit, metrics
and drain receipt) SHA-256:
`18cdf89582f10ec12855bbf277c841d978a37a22f94b5b07d21e95f254d9c929`.
Its persistent copy is under the canonical ignored session root
`.mask/sessions/bpr1653-postpublication-v031/rss-budget-20261020/`; local raw
archives and durable continuation remain under `/tmp/bpr-qt612-run/ubuntu-rss/`.
The native original ref is `bpr1653-budget-original-f25`; preserved native
evidence is `/home/mmarcin/bpr1653-budget-evidence`. M3 explicitly accepts
ownership of that clean source/build root and preservation obligation.

All task-owned native/local commands, collectors and copies have completed;
no scheduled task, review server or process was created for later handoff.
No process was stopped. Fresh Windows/WSL audits show no build/test/app process
before the lease returns. M3 may resume native work; this leaf will issue no
further native command. Only coordinator integration and any broader release
qualification remain outside this completed budget slice.
