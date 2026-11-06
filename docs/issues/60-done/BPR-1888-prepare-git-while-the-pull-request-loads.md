---
schema: "implement-rfc.card.v1"
id: "BPR-1888"
title: "Prepare Git while the pull request loads"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1888--BPR-1888--c401c5ea9d04.json","related_cards":["BPR-1842"],"milestone_plan":{"id":"responsiveness-latency-v5","path":"docs/rfcs/plans/responsiveness-latency-v5/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8e0e5ec722d8479d3c04382f62a64d8aaf336e21","evidence":{"kind":"observation","detail":"The admitted read starts non-authoritative Git preparation through the composed handoff, on the existing epoch effect lane. No review cache opens early; unit and composed negative/positive controls pass."}},{"criterion":"AC2","outcome":"pass","revision":"8e0e5ec722d8479d3c04382f62a64d8aaf336e21","evidence":{"kind":"observation","detail":"Native Git contract checks prove admission reuse, exact child-count preservation, concurrent preparation coalescing, pre-cancel absence of effects and ordinary failure diagnostics; host exception and executor-refusal fallback pass."}},{"criterion":"AC3","outcome":"pass","revision":"8e0e5ec722d8479d3c04382f62a64d8aaf336e21","evidence":{"kind":"observation","detail":"Host tests cover once-only work, one/two/three-slot foreground behavior, queued and in-flight shutdown cancellation and terminal quiescence. Composed opening and refresh preserve loading feedback, stale-result exclusion and first-file rendering."}},{"criterion":"AC4","outcome":"pass","revision":"8e0e5ec722d8479d3c04382f62a64d8aaf336e21","evidence":{"kind":"observation","detail":"Release three-run first-diff medians improve 7521 to 7116 ms and post-accept medians 1179 to 761 ms with the same 19 reads; the 9408 ms outlier and inherited gate failures remain recorded. Documentation, format and composition inventory controls pass; this source and its journal line are on main."}}]
---

# BPR-1888: Prepare Git while the pull request loads

The owner requested continued direct responsiveness work, especially independent
loads with clear pending states, and each round on main with a card and journal
line. BPR-1842 identified the first installed-Git admission as work still delayed
until snapshot acceptance. Its executable/version/capability checks are independent
of repository identity and can run while the GitHub read is in flight.

## Acceptance

- AC1: Start optional, bounded Git-runtime preparation with an admitted PR read;
  open no review repository or cache session and grant no snapshot authority early.
- AC2: Reuse the existing exact executable/isolation-root admission checks;
  serialize overlapping preparations, retain cancellation and foreground fallback,
  and keep preparation failures from changing the read's result.
- AC3: Account for preparation in host shutdown/quiescence, avoid repeated jobs
  and preserve constrained foreground admission; verify real adapter and composed
  opening paths, including clear loading states and unchanged read counts.
- AC4: Record optimized before/after latency, focused checks and inherited
  failures; complete the card and one-line journal entry and land on main.

## Implementation and evidence

The admitted window read asks its epoch handoff to prepare the Git runtime. The
application's handoff wrapper forwards that request to the existing epoch host;
its effect worker runs the cache adapter's optional preparation. This grants no
PR authority and opens no review cache session. The existing local SHA-1/SHA-256
capability probes use temporary probe repositories and clean them normally.
The accepted handoff still opens and validates the actual review cache.

Each window schedules at most one optional job, with the existing cancellation,
completion and quiescence accounting. A busy/closing host or one with fewer than
three slots keeps demand-driven setup, leaving both first-comparison and newest-
history-page capacity available. Failure, allocation or executor refusal is
best effort; the ordinary accepted handoff reports any mandatory setup failure.
The installed cache serializes runtime preparation and process construction with
a cancellable wait and reuses BPR-1842's exact executable/root admission record.

Release checks on macOS arm64 (M4 Pro), AppleClang 21, Qt 6.12:

- `application_github_window_read_host` passes: preparation accompanies an admitted
  read, grants no authority, preserves the accepted result even when it throws,
  and does not start when shared rate-limit admission refuses a read.
- `application_epoch_host` passes: one optional effect, no cache/session or
  projection before authority, throwing/executor-refused fallback, completion of
  the normal comparison, queued and in-flight shutdown cancellation, and
  capacity-one/two/three behavior with optional work held pending. An initial
  assertion incorrectly expected capacity one's existing history/comparison
  limitation to disappear; it was corrected against the documented fixture
  contract. The resulting control then caught the real two-slot interference;
  reserving two foreground slots fixes that failure.
- `git_cache_contract` and `git_process_contract` pass. Native Git preparation
  leaves no review repository or cache lock, pre-cancel does no work, repeated
  preparation launches no probes, concurrent preparations admit Git once, and
  preparation plus open launches exactly as many Git children as a cold open.
  The diagnostic candidate still reports its original redacted error. Existing
  executable-change invalidation and in-flight factory cancellation checks pass.
- `application_progressive_open_public_path` and
  `application_refresh_current_file_public_path` pass. The new composed opening
  assertion observes epoch work while the PR is still loading; it failed with
  the application handoff forwarder absent and passes with it present. Early
  title/files, stale-load exclusion, the pending file-opening plate and refresh
  feedback remain covered. The first timing run exposed that missing forwarding;
  its unchanged measurements are retained as an unwired control, not credited
  as an improvement.
- Changed C++ files pass clang-format 22 and whitespace checks. The six-unit
  clang-tidy 22 run passes the cache, read host and three unit/contract files;
  the epoch host reports only the three pre-existing parameter-name diagnostics
  at lines 7721, 7722 and 7724, originating in `3df9e220bd`. A final check of its
  changed lines passes. The six-line composition forwarder is compiled and
  reviewed; the mutation-inventory positive/hostile-control course passes after
  resealing only the reviewed application/manifest pins. The mutation branch
  digest remains `70fe8a7e`. No UI layout or presentation code changes.

## Optimized latency

The same composed Home-open course, fresh private root per process and native
Git, with 330 ms simulated latency per GitHub read, runs three times before and
three times after. Baseline product is `9c96d7a6a` (the allocation/claim successors
only change documentation). No task-owned builds, lint or other tests run during
these measurement sets. Values below are medians in milliseconds from the open
gesture; post-accept is computed per sample before taking the median.

| First PR milestone | Before | After |
|---|---:|---:|
| Title | 679 | 680 |
| Identity | 684 | 687 |
| Overview files | 1,007 | 1,009 |
| Accepted snapshot | 6,349 | 6,355 |
| First diff | 7,521 | 7,116 |
| Acceptance to first diff | 1,179 | 761 |
| GitHub reads | 19 | 19 |

The already-warm second PR remains 21 reads: first-diff medians 6,830 to 6,769 ms.
Before first-diff samples: 7521, 7714, 7521 ms.
After first-diff samples: 9408, 7116, 6991 ms.
The 9,408 ms after sample is retained. These are synthetic local Release
observations, not a universal wall-clock or live-network guarantee; timings are
nongating. No Debug performance course was run.

## Limitations and follow-up

`architecture_boundaries` still reports BPR-1884's managed-clone `.git` paths as
neutral-working-directory validation. The earlier architecture contract's
synthetic factory fixture and test-network manifest failures remain inherited;
no passing credit is taken for them. The owner explicitly asked to fix the
architecture issue and the three naming warnings next, in a separate card.

Evidence is external at
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-responsiveness-vvxyjjsj`:
`warmup-{functional,epoch-host-final,wired-public-paths,capacity-control,wiring-negative-control,format-final,tidy,host-tidy-changed-lines,composition-inventory,boundaries}.log`,
`warmup-before-{1,2,3}.log`, `warmup-after-{1,2,3}.log` (unwired control),
`warmup-wired-after-{1,2,3}.log`, and `warmup-timing-summary.json`.
The explicit v5 milestone binding is valid; global checkpoint discovery still
encounters an unrelated historical plan's unsupported `amendments` field.

Source revision `8e0e5ec722d8479d3c04382f62a64d8aaf336e21` is landed on local `main`. The final documentation gate passes; the one-line journal entry is committed with the implementation.
