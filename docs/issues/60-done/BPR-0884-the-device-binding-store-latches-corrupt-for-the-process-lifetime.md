---
schema: "implement-rfc.card.v1"
id: "BPR-0884"
title: "The device binding store latches corrupt for the process lifetime, including on cancellation"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win", "mac"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable sqlite partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005","linux_done_at":"02e3d59d","remaining":"none: the owner-required independent read-only review of 02e3d59d (rfc-reviewer-critical, claude-fable-5-1, 2026-09-24) found no blocking issue; its residual is BPR-1111","related_cards":["BPR-0541","BPR-0644","BPR-0841","BPR-0883","BPR-0887","BPR-1048","BPR-1111"],"review":{"reviewer":"rfc-reviewer-critical","model":"claude-fable-5-1","mode":"read-only, macOS, from source","head":"2852dde0","candidate":"02e3d59d","verdict":"no blocking finding","decision":"8e1858ab-e9f7-4371-b854-71a6ab3e1946"}}
acceptance: [{"criterion":"AC1 A cancelled or busy open leaves the store usable on the next call, shown by a contract that reuses one store","outcome":"pass","revision":"02e3d59d","evidence":{"kind":"command","detail":"sqlite_device_binding_storage retries a cancelled quick_check preflight and an interrupted legacy migration on the same store, and asserts unavailable (never corrupt) for a busy migration, a locked first read and a writer racing the snapshot. Linux pass at 02e3d59d (trials C1-C3, C5, C6 fail at their pins); Windows pass at 78c8ac85 (record f7e93488) and bcd57840 (record 76a0ecdd, 6 of 6); macOS pass at 70c4974a (record 7d40ae8c, 6 of 6). Residual from the independent review, finding 1: an SQLite I/O, NOMEM, PROTOCOL or BUSY_RECOVERY failure after the first read still latches corrupt; ordinary lock holders cannot reach it; carded as BPR-1111.","path":"docs/issues/60-done/BPR-0884-the-device-binding-store-latches-corrupt-for-the-process-lifetime.md#closeout-2026-09-24"}},{"criterion":"AC2 A genuine post-admission corruption still latches, and that is still pinned","outcome":"pass","revision":"02e3d59d","evidence":{"kind":"command","detail":"sqlite_device_binding_storage: an unknown schema object gives corrupt; after repair the same store's load and erase still fail and a new store loads. Linux pass at 02e3d59d (trial C4, never latch, fails here); the same binary passes on Windows (78c8ac85, bcd57840) and macOS (70c4974a).","path":"docs/issues/60-done/BPR-0884-the-device-binding-store-latches-corrupt-for-the-process-lifetime.md#verification-on-linux"}},{"criterion":"AC3 The card records how this interacts with BPR-0541 and BPR-0644","outcome":"pass","revision":"02e3d59d","evidence":{"kind":"freeform","detail":"The section 'How this relates to BPR-0541 and BPR-0644' records that the store never reports success without a complete establish, that BPR-0541's host latch is unchanged, and that the change works with either answer to BPR-0644 (the owner since chose keep the grant, fail visibly and retry at next launch). The independent review found the change consistent with BPR-0841, BPR-0541 and BPR-0644.","path":"docs/issues/60-done/BPR-0884-the-device-binding-store-latches-corrupt-for-the-process-lifetime.md#how-this-relates-to-bpr-0541-and-bpr-0644"}},{"criterion":"AC4 A fallback post that throws leaves the queue drainable and is_quiescent() reachable, shown by a test that injects the throw","outcome":"pass","revision":"02e3d59d","evidence":{"kind":"command","detail":"application_github_host, test_device_binding_persistence_stays_drainable_when_its_posts_are_refused: withdrawal at admission, a stranded queued successor and a refused closing refusal, each followed by is_quiescent() and a served next request. Linux pass at 02e3d59d (trials A1-A3 fail at their assertions); Windows and macOS pass. The independent review (finding 3c) notes the test pins at least one terminal per request, not exactly one; BPR-1111 adds a per-request counter.","path":"docs/issues/60-done/BPR-0884-the-device-binding-store-latches-corrupt-for-the-process-lifetime.md#verification-on-linux"}},{"criterion":"AC5 Owner requirement (attended session cbd48b91, decision 8e1858ab): an independent read-only rfc-reviewer-critical review of 02e3d59d, with its findings dispositioned on the card","outcome":"pass","revision":"02e3d59d","evidence":{"kind":"observation","detail":"rfc-reviewer-critical (claude-fable-5-1), dispatched by the coordinator, read-only on macOS from source at 2852dde0 (product files unchanged since 02e3d59d); ran sqlite_device_binding_storage and application_github_host (2 of 2 pass) and the documentation gate. Verdict: no blocking finding. Finding 1 and 3c -> BPR-1111; finding 2 -> the stale 'Recorded, not changed' bullet now points at BPR-1048 (a670fd95); 3a and 3b informational. Recorded from the coordinator's summary; the reviewer's full report is not in the repository.","path":"docs/issues/60-done/BPR-0884-the-device-binding-store-latches-corrupt-for-the-process-lifetime.md#independent-review-2026-09-24"}}]
---

# BPR-0884: The device binding store latches corrupt for the process lifetime, including on cancellation

## Defect

`src/adapters/sqlite/src/device_binding_store.cpp:226-233` sets
`Failed = !established.has_value()` **before** checking
`aStop.stop_requested()`. And `establish` returns `corrupt()`/`unavailable`
for a `configure_connection` failure (`:270-271`) and for a cancelled or busy
`BEGIN IMMEDIATE`/migration (`:284-287`).

So a **cancellation** — or a transient busy database — latches the store as
corrupt. Every later `load`, `store` or `erase` then returns `corrupt` from
`ensure_open` (`:204-206`) without ever reopening, and the host turns that into
`binding_failure()`, failing the deferred session with `github_failure::vault`
(`src/application/src/github_application_host.cpp:4325-4340`). The user's
remembered device grant is unusable until the process restarts.

The two sibling stores do the opposite: `profile_registry_store.cpp:442-447`
and `window_review_restoration_store.cpp:516-521` close and retry.

The existing cancellation contract cannot see this, because it builds a fresh
store per call (`device_binding_storage_tests.cpp:132-138`) — the latch needs a
second call on the same store to appear.

**This needs a decision, not a silent change.** [[BPR-0841]] item 2
deliberately chose that "only a failure after admission latches", and
[[BPR-0541]]'s fail-closed rule is the reason this area is conservative. The
narrow claim here is only that *cancellation* and *busy* are not corruption
and should not latch, which is consistent with both.

This is adjacent to the open owner question on [[BPR-0644]] about whether a
remembered grant should survive a locked or denied Keychain — same feature,
different fault. Worth deciding together.

## Scope

- Do not latch on `cancelled`, and close the connection so a retry re-runs
  `establish`.
- Decide explicitly whether a transient `unavailable` (busy, locked) should
  latch; state the answer and its relation to BPR-0541's fail-closed rule.
- Extend the cancellation contract to call twice on the **same** store, which
  is what makes the latch observable.

## Acceptance

- A cancelled or busy open leaves the store usable on the next call, shown by a
  contract that reuses one store.
- A genuine post-admission corruption still latches, and that is still pinned.
- The card records how this interacts with BPR-0541 and BPR-0644.

## A second failure path in the same file, from the core+application review

Folded in here rather than carded separately, because it is the same file and
the same question — what a failure on this lane should do to the lane.

`src/adapters/sqlite/src/device_binding_store.cpp:139-157`: `submit()` catches
a failure from `Effects.post` and then calls `Decisions.post(...)` **outside
any try**. If that throws, the exception escapes `enqueue()` after the job has
already been pushed (`:112`). The job then sits at `Requests.front()` with no
scheduled completion; every later `enqueue` sees `Requests.size() != 1` and
never calls `submit()`, and `is_quiescent()` (`:189`) never returns true — so
host quiescence stalls for the rest of the process.

Only reachable on an executor allocation or refusal failure, which is why it is
not urgent. The repair is local: wrap the fallback post, and on failure pop the
job and complete it synchronously or mark it failed, so the lane stays live.

**Deliberately not taken as a quick fix in dsp-005.** The wrapping is trivial;
deciding what a failed job should report is the same decision this card already
owes for cancellation and `unavailable`, and making it twice in two passes is
how inconsistent failure semantics get built. Decide once, here.

Additional acceptance: a fallback `post` that throws leaves the queue drainable
and `is_quiescent()` reachable, shown by a test that injects the throw.

## Linux half at `02e3d59d` (bpr-main-linux, 2026-09-23)

Claimed at `0615cc67`. The product, tests and documentation are one commit,
`02e3d59d`. The card is back in Ready for Windows and macOS only.

### What was stale on the card

- The line numbers had moved ([[BPR-0841]], [[BPR-0883]], [[BPR-0887]]). At
  `097b8985` the latch was `src/adapters/sqlite/src/device_binding_store.cpp:227-233`
  and the migration's `corrupt()` was at `:285-288`. The defect itself was
  current. The new pins below fail on the unchanged product at their first
  assertion.
- The second failure path is not in the SQLite file. `submit()`,
  `enqueue()` and `is_quiescent()` are in
  `src/application/src/device_binding_store.cpp`, the persistence lane. The
  same lane had a third case of the same fault. A capacity or closing refusal
  counted `PendingRefusals` before a post that could throw, and the count then
  never came back down.

### Decision: what a failure does to the store

**Only a verdict on the admitted file latches.** A verdict means the file was
read and judged. Any of these is one: a wrong identity, version, schema,
object count or scope; a failed `quick_check`; an invalid or duplicate row;
the hostile-content step ceiling; or SQLite reporting at the first read that
the file is not a database. Those still return `corrupt` and latch for the
life of the store.

Three failures are not verdicts, and none of them latches.

- **Cancellation** returns `cancelled`.
- **A transient `unavailable`** returns `unavailable`. This covers busy,
  locked, a failed `configure_connection`, and a failed migration: a busy
  `BEGIN IMMEDIATE`, a migrator racing another one, or a full disk.

In every one of these cases the connection closes. The next call admits,
opens and judges the file again from the start. **So the answer to scope item
2 is no: a transient `unavailable` does not latch.**

Three further details:

- The judgement reads inside one read transaction, so it comes from a single
  snapshot. Once the first read holds that snapshot, no later read can meet a
  lock or see another writer's commit. Before this change, a racing migrator
  that committed between two reads could make a fresh file look corrupt, and
  that false verdict latched.
- A latched store also closes its connection now. Before, it kept its file
  open.
- An open refused **after** admission still latches, as [[BPR-0841]] decided.
  A file substituted between admission and open is a security signal, and
  busy or locked cannot arise at open.

**Narrowed by the independent review (2026-09-24).** The code classifies
only the first read's failure. A failure at a later read in `inspect()`, or
at `read_rows`' prepare or step, still reports `corrupt` and latches. So an
SQLite I/O, memory, protocol or WAL-recovery error at those points latches
the store. The single-snapshot detail above holds for `inspect()`'s reads,
but not for the `read_rows` re-read, which runs after the rollback. The
reviewer's probe showed that ordinary lock holders cannot reach either path.
The residual is [[BPR-1111]]. See "Independent review (2026-09-24)",
finding 1.

### How this relates to BPR-0541 and BPR-0644

**[[BPR-0541]].** Its fail-closed rule is that a store or vault failure
leaves a non-resumable diagnostic rather than a successful false grant. That
rule holds per call and on the host, and nothing here changes it:

- The store never reports success without a complete establish. A retry
  reruns every check, so no longer latching on a non-verdict cannot admit
  anything the failed attempt would have refused.
- A failed call still reaches the host as a failure. The host still publishes
  `vault_unavailable`. After a failed startup load it still sets
  `BindingLoadUnresolved` and refuses session establishment for the rest of
  that application lifetime. That host latch is BPR-0541's own, and this card
  leaves it alone.
- The only difference is that a later call on the same store now reaches the
  database. So a tombstone after a cancelled or contended open is written.
  That covers Sign out, manual replacement, shutdown cleanup and
  `prepare_device_rotation`. Before, the tombstone returned `corrupt` and the
  grant stayed live. **This moves toward fail-closed, not away from it.**

**[[BPR-0644]].** Its open owner question is about host policy. Should a
remembered grant survive a launch whose Keychain answers locked or denied, or
whose network is down? This card does not answer that question. It changes
no host behaviour for those faults; the store is not the vault. Either answer
works with this change:

- If fail-closed erasure stays, a store that does not latch is what lets that
  erasure's tombstone be written after a transient fault.
- If the owner chooses to keep the material and retry, the same store is what
  lets a retry within the process reach the database.

No joint decision is needed to close this card. The question stays open on
BPR-0644.

### Decision: what a failure does to the persistence lane

Every request the lane admits gets exactly one terminal. If a request's
effect cannot be posted, the request reports `unavailable`, as before. If the
decision lane refuses that terminal as well:

- **A new request** is withdrawn and the call throws. Nothing is retained and
  no terminal follows. This is the boundary `effect_executor::post` already
  gives when it throws. The copy of the job now happens inside the `try`.
  Before, a copy that threw escaped and left the job retained.
- **A queued request** gets its `unavailable` terminal in the lane turn that
  finished the request before it, after that request's own terminal. The next
  job then starts.
- **A capacity or closing refusal** whose terminal is refused is removed from
  the count, and the call throws.

The header contract on `device_binding_persistence` says this, and so does
`docs/architecture/github-host-implementation.md`.

**Recorded, not changed:**

- If the decision lane refuses the effect worker's own terminal post, there
  is no recovery that stays on the lane. The `decision_executor` contract
  forbids a live lane from refusing.
- Only one host call site catches a throw from the lane. The others do not:
  `persist_settled_device_binding`, `start_delete`, `prepare_device_rotation`
  and the launch-restore erase. After a double refusal the lane itself drains,
  but the host's own `OutstandingBindings` stays counted. That gap predates
  this card, and it is only reachable when both executors refuse (allocation
  failure). It is a follow-up candidate and needs an ID.

  **Since closed (2026-09-24).** [[BPR-1048]] was the follow-up. It fixed
  this gap at `a670fd95`, and each of the four sites now takes its own
  failure path on a withdrawal. The independent review's finding 2 found
  this bullet stale.

### Verification on Linux

Every run was offscreen, with DISPLAY and WAYLAND_DISPLAY unset and fresh
temporary HOME and XDG roots. Logs are station-local under
`build/logs/bpr-0884/`.

| Claim | Check | Result |
|---|---|---|
| The defect is current | New pins against the unchanged product (`negative-control-old-product.log`) | Both rows fail: `a cancelled open leaves the same store usable on its next call`; `a request whose effect and terminal are both refused is withdrawn` |
| A cancelled open leaves the same store usable | `sqlite_device_binding_storage`: cancelled `quick_check` preflight and interrupted legacy migration, each retried on the **same** store | Pass. Trials C5 (always latch) and C6 (latch ignoring the stop) fail at this pin |
| A busy open leaves the same store usable | A second connection holds `BEGIN IMMEDIATE` over a legacy file. The load is `unavailable` with version still 1, and the same store then migrates to version 2 | Pass. Trial C2 (migration failure is `corrupt`) fails here |
| A locked first read is not corruption | A rollback-journal file under `BEGIN EXCLUSIVE`. The load is `unavailable`, and the same store then reads | Pass. Trial C3 fails here |
| A verdict comes from one snapshot | A writer tries to commit between the store's first and third reads. It gets `SQLITE_BUSY`, and the store creates the fresh file it read | Pass. Trial C1 (no read transaction) fails: the writer commits and the fresh file is judged corrupt |
| A genuine corruption still latches, and is pinned | An unknown schema object gives `corrupt`. After repair, the same store's load and erase still fail, and a new store loads | Pass. Trial C4 (never latch) fails here |
| A refused fallback post leaves the lane drainable | `application_github_host`, `test_device_binding_persistence_stays_drainable_when_its_posts_are_refused`: withdrawal at admission, a stranded queued successor, and a refused closing refusal; `is_quiescent()` after each, then the next request is served | Pass. Trials A1, A2 and A3 each fail at their own assertion (`reverted-trials.log`) |
| No regression on the composed paths | 16 rows at `efd18ff0` (`covering.log`), including `architecture_boundary_contract`. The integration pass at the rebased `02e3d59d` is 15/15 (`integration-rebased.log`): the storage, private-store-open and release-maintenance contracts, the host and refresh units, `application_device_binding_restart`, `_launch_renewal`, `_relaunch`, the device authorization public path, refresh and orphan courses, profile startup and management public paths, and `architecture_boundaries` | Pass |
| Stable | The storage contract and host row repeated 5 times (`repeat.log`) | 10/10 |
| Gates | clang-format 22 check on the five touched C++ files. Scoped clang-tidy on the four translation units (`tidy.log`). `verify_documentation.py` | Format passes. Tidy reports two findings, both on lines 5711 and 5715 of `github_application_host_tests.cpp`, which were written in `3ad87ee4` and are not touched here. `DOCUMENTATION_OK` |

**No independent review.** This change touches persistence, migration and
concurrency. The Linux lane ran as a leaf that cannot start a reviewer.
Review the `02e3d59d` diff before this card goes to Done.

### Owed: Windows and macOS

The code is portable, but the contention courses depend on each platform's
SQLite lock layer. Windows uses `LockFileEx`, and its open helper binds the
handle SQLite holds. Run this at `02e3d59d` or later:

```sh
ctest --test-dir <build> --output-on-failure -R "^(sqlite_device_binding_storage|sqlite_private_store_open|application_github_host|application_device_binding_restart|application_device_binding_launch_renewal|application_device_binding_relaunch)$"
```

- **Windows (bpr-main-win).** All six pass.
  - `sqlite_device_binding_storage` prints `device-binding-storage-tests:pass`
    in about 3 seconds. Most of that is the busy migration waiting out its
    2-second timeout. The three contention checks must report
    `unavailable`, never `corrupt`.
  - `sqlite_private_store_open` still prints
    `private-store-open:admission-retry:refused=1:retried=1`.
- **macOS.** The same selector, with the same expected results.

### Acceptance status

- A cancelled or busy open leaves the store usable on the next call, shown by
  a contract that reuses one store: **Linux pass at `02e3d59d`.** Windows and
  macOS are owed.
- A genuine post-admission corruption still latches, and that is still
  pinned: **Linux pass at `02e3d59d`.** Windows and macOS are owed.
- The card records how this interacts with BPR-0541 and BPR-0644: **done
  above.**
- A fallback `post` that throws leaves the queue drainable and
  `is_quiescent()` reachable, shown by a test that injects the throw:
  **Linux pass at `02e3d59d`.** The code is platform-neutral, and the Windows
  and macOS runs of `application_github_host` confirm it.

### Windows: five of six pass; the sixth fails contained, for BPR-0876's reason (bpr-main-win, 2026-09-23)

**What was run.** The source is `78c8ac85`, which contains `02e3d59d`. The
checkout was at `2a30d786`, which is that tree plus a local card claim, so
the sources are identical. The build is `build/windows-dev` (Visual Studio
generator, MSVC 19.51, Debug, Qt 6.11.2), rebuilt incrementally first. It
exited 0 with no `error` line. The owed selector ran serially and verbose,
with `ctest --preset windows-dev -j1 --output-on-failure -V -R` over the six
rows.

| Row | Result |
| --- | --- |
| `application_github_host` | Passed, 0.10 s (`application github host tests passed`) |
| `sqlite_private_store_open` | Passed, 0.38 s |
| `sqlite_device_binding_storage` | Passed, 3.57 s (`device-binding-storage-tests:pass`) |
| `application_device_binding_restart` | Passed, 40.31 s |
| `application_device_binding_launch_renewal` | Passed, 38.41 s |
| `application_device_binding_relaunch` | **Failed**, 0.36 s: `seed failed (1)`, `SEED_STARTUP_PROFILE_REFUSAL:registry_unavailable`, `FAILED(seed): application startup succeeds` |

**The lock lines.** `sqlite_device_binding_storage` prints only
`device-binding-storage-tests:pass`. Its three contention checks print
nothing of their own when they hold. Each check asserts `unavailable`, and
any `corrupt` would have stopped the contract with its own `FAILED:` line. So
on Windows's `LockFileEx` lock layer, all three read `unavailable` and never
`corrupt`:

- a busy migration under a second connection's `BEGIN IMMEDIATE`;
- a locked first read under `BEGIN EXCLUSIVE`;
- a writer racing the store's snapshot.

The row's 3.57 s agrees with the busy migration waiting out its
2-second timeout.

**The admission-retry line.** `sqlite_private_store_open` still prints
`private-store-open:admission-retry:refused=1:retried=1`, together with
`non-ansi-root:pass` and `private-store-open-tests:pass`.

**The failing row.** `application_device_binding_relaunch` runs contained on
Windows, as registered, and there its seed is refused. That is
[[BPR-0876]]'s recorded contained failure for this course, and it happens
before any device-binding load, so it says nothing about this card's change.
Run uncontained, the same course passes every step on this station today:
[[BPR-1039]]'s closed-connection change passes it 3 of 3, and its runs are
recorded there. It was not re-run here, because the owed run was the
registered selector.

The logs are station records under `build/logs/bpr-0884-main-win/` and are
not committed. The card stays in Ready: Windows owes nothing more on the
five rows that exercise this change, and the owner may want to review the
diff first.

### macOS: all six pass (bpr-main-mac, Mac batch 5, 2026-09-23)

Run on `70c4974a`, which contains `02e3d59d`. Apple Silicon, AppleClang,
Qt 6.11.2, `build/dev` Debug, reconfigured and rebuilt first (exit 0, no
`error` or `warning:` line). The owed selector ran once, serially and
verbose:
`ctest --test-dir build/dev --output-on-failure -V -R "^(sqlite_device_binding_storage|sqlite_private_store_open|application_github_host|application_device_binding_restart|application_device_binding_launch_renewal|application_device_binding_relaunch)$"`.

| Row | Result |
| --- | --- |
| `application_github_host` | Passed, 0.55 s (`application github host tests passed`) |
| `sqlite_private_store_open` | Passed, 0.47 s |
| `sqlite_device_binding_storage` | Passed, 2.76 s (`device-binding-storage-tests:pass`) |
| `application_device_binding_restart` | Passed, 29.39 s |
| `application_device_binding_launch_renewal` | Passed, 20.13 s |
| `application_device_binding_relaunch` | Passed, 8.85 s |

6/6, ctest exit 0. The one-minute load was 24.2 at the start and 9.5 at the
end.

- **The lock checks.** As on Windows, `sqlite_device_binding_storage` prints
  only `device-binding-storage-tests:pass`. Each of the three contention
  checks asserts `unavailable`, and a `corrupt` would have stopped the
  contract with its own `FAILED:` line. So on macOS's POSIX lock layer all
  three read `unavailable`, never `corrupt`. The row's 2.76 s agrees with the
  busy migration waiting out its 2-second timeout. The same binary also runs
  [[BPR-0887]]'s nesting checks, which merged cleanly with this change.
- **No admission-retry line on macOS, as built.** `test_device_binding_admission_refusal_is_retried`
  and its `admission-retry` case are compiled only under `_WIN32`
  (`tests/sqlite_contract/private_store_open_tests.cpp`, the `#if
  defined(_WIN32)` blocks around the case list and its dispatch). macOS runs
  `non-ansi-root` (all twelve stores `opened=1`, then `pass`) and
  `owner-only` (33 files at `600`, then `pass`), then
  `private-store-open-tests:pass`. The expectation of
  `admission-retry:refused=1:retried=1` applies to Windows only.

The card stays in Ready, waiting on the Windows relaunch question and any
review, as the Windows section says.

### Windows: all six pass as registered; the relaunch question is closed (bpr-main-win, 2026-09-24)

The Windows run above left one row open. `application_device_binding_relaunch`
was contained then, and its seed was refused. [[BPR-0876]] has since
registered it `WINDOWS_UNCONTAINED`, pinned `dos-volume-name` (`56f7a836`,
after that run). So the owed selector was run again, as registered.

**What was run.** The head is `bcd57840`, which contains `02e3d59d`.
`src/adapters/sqlite/src/device_binding_store.cpp` and
`src/application/src/device_binding_store.cpp` are unchanged since the first
Windows run's source, `78c8ac85`. The build is `build/windows-dev` (Visual
Studio generator, MSVC 19.51, Debug, Qt 6.11.2), reconfigured and rebuilt at
this head. It exited 0 with no warning or error line. The command was
`ctest --preset windows-dev -j1 --output-on-failure -V -R "^(sqlite_device_binding_storage|sqlite_private_store_open|application_github_host|application_device_binding_restart|application_device_binding_launch_renewal|application_device_binding_relaunch)$"`,
run through a `.cmd` file. The log is
`build/logs/bpr-0884-main-win/rerun-rows.log`, a station record that is not
committed.

| Row | Result |
| --- | --- |
| `application_github_host` | Passed, 0.09 s (`application github host tests passed`) |
| `sqlite_private_store_open` | Passed, 0.41 s. It prints `private-store-open:admission-retry:refused=1:retried=1`, `non-ansi-root:pass` and `private-store-open-tests:pass` |
| `sqlite_device_binding_storage` | Passed, 3.41 s (`device-binding-storage-tests:pass`) |
| `application_device_binding_restart` | Passed, 37.48 s |
| `application_device_binding_launch_renewal` | Passed, 30.06 s |
| `application_device_binding_relaunch` | Passed, 15.57 s (`DEVICE_BINDING_RELAUNCH_OK`). It is uncontained as registered: its test command is the Python driver itself, not `better_pull_review_network_sandbox.exe` |

6 of 6 passed, and ctest exited 0. The log has no `corrupt` line. As before,
the three contention checks print nothing of their own when they hold. So
they read `unavailable`, never `corrupt`, on `LockFileEx`.

**The Windows record is now complete for every criterion that needs a run.**

- *A cancelled or busy open leaves the store usable on the next call:* pass on
  Windows, at `78c8ac85` and again at `bcd57840`.
- *A genuine post-admission corruption still latches, and that is still
  pinned:* pass on Windows. It is the same `sqlite_device_binding_storage`
  binary.
- *A fallback `post` that throws leaves the queue drainable:* pass on Windows
  (`application_github_host`).
- *The relaunch course:* it passes as registered. It was never evidence
  against this change, because its contained failure came before any
  device-binding load.

This note does not move the card. What remains is the independent review of
`02e3d59d`, which is waiting on the owner's decision.

## Owner decision, 2026-09-24: yes, an independent critical review (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589. Candidate `BPR-0884-independent-review-v001`,
revision `02e3d59d`, fingerprint `cf1da604…`, decision
`8e1858ab-e9f7-4371-b854-71a6ab3e1946`, sequence 574, 2026-09-24T18:58:04Z,
`actor: human`: **select**, verbatim:

> Yes: run an independent read-only review of 02e3d59d

Note, verbatim:

> Yes use a rfc-reviewer-critical to double check the work.

So the card goes to Done only after an `rfc-reviewer-critical` has reviewed
the `02e3d59d` diff read-only (persistence, migration and concurrency:
`src/adapters/sqlite/src/device_binding_store.cpp`,
`src/application/src/device_binding_store.cpp` and its header, their tests and
the host architecture note) and its findings are dispositioned here. The
review is dispatched by the coordinator, not by a lane that wrote the change.
It changes no code, board state or GitHub.

The same session answered [[BPR-0644]]'s policy question: keep the grant, fail
visibly and retry at the next launch. As "How this relates to BPR-0541 and
BPR-0644" says, this change works with that answer; the store that no longer
latches is what lets an in-process retry reach the database. The reviewer
should read it with that answer in mind.

## Independent review (2026-09-24)

**Who and how.** An `rfc-reviewer-critical` (`claude-fable-5-1`) reviewed
the `02e3d59d` change. The coordinator dispatched it, as the owner decided
above (decision `8e1858ab`), and the lane that wrote the change did not. It
worked read-only, on macOS, from source, at `2852dde0`. The product files are
unchanged there since `02e3d59d`. It ran `sqlite_device_binding_storage` and
`application_github_host`, and both passed. It also ran
`verify_documentation.py`. It changed no code, board state or GitHub. This
review meets the owner's review requirement. It replaces the "No independent
review" paragraph above, which stays as history.

**Verdict: no blocking finding.** These held:

- **The snapshot against writers.** `inspect()` reads inside `BEGIN` ...
  `ROLLBACK` (`src/adapters/sqlite/src/device_binding_store.cpp:260-264`).
  The migration's `BEGIN IMMEDIATE` at `:323` is unconditional, so a change
  made in the gap after the snapshot fails as `unavailable`.
- **Fail-closed** (`:233-235`).
- **Exactly one terminal per request**, under the executor contract
  (`src/application/src/device_binding_store.cpp:96-198`).
- **Migration failures.** The reviewer found no migration failure that
  should latch.
- **Consistency** with [[BPR-0841]], [[BPR-0541]] and [[BPR-0644]].

**Non-blocking findings, in the reviewer's order.** Line references are the
reviewer's, in `src/adapters/sqlite/src/device_binding_store.cpp` unless
another file is named.

1. "Only a verdict latches" is narrower in code than this card claims. Only
   the first read is classified (`:281-285`). Later failures (`:288-289`,
   `:305-306`, `:312-313`) and `read_rows`' prepare and step (`:145-146`,
   `:157-158`) map to `corrupt`. So `SQLITE_IOERR_*`, `NOMEM`, `PROTOCOL`
   and `BUSY_RECOVERY` latch the store as corrupt. The `:276` re-read runs
   after the snapshot's rollback, so the comment at `:253-257` holds only
   inside `inspect()`. A scratch probe showed that ordinary lock holders
   cannot reach these paths. The I/O and WAL-recovery class remains. It
   predates this card, and this card narrowed it. The reviewer suggests:
   - classify the step and prepare results in `read_rows` through
     `detail::classify_sqlite_result`
     (`src/adapters/sqlite/src/catalog_database.cpp:4291-4306`);
   - drop the `:276` re-read, or move it;
   - state the residual on this card.
2. The "Recorded, not changed" text about the `OutstandingBindings` gap is
   stale, because [[BPR-1048]] closed the gap. It should cross-reference
   BPR-1048.
3. Informational:
   - (a) A stop at `ensure_open()` (`:241-242`) can turn `corrupt` into
     `cancelled` without latching. The next call judges the file again.
   - (b) A stranded successor always gets `unavailable`
     (`src/application/src/device_binding_store.cpp:177`).
   - (c) `test_device_binding_persistence_stays_drainable_when_its_posts_are_refused`
     (`tests/application/github_application_host_tests.cpp:5492-5549`)
     checks that each request gets at least one terminal, not exactly one.
     The reviewer suggests a per-request counter.

**Disposition.**

- Finding 1 and finding 3(c) are [[BPR-1111]] (all platforms, Ready). The
  residual is also stated under "Decision: what a failure does to the
  store" above.
- Finding 2 is corrected in place. The stale bullet now says that BPR-1048
  closed the gap at `a670fd95`, and its original text is kept.
- Findings 3(a) and 3(b) are informational and need no change.

**Limitations.**

- The review ran on macOS only. The Windows and Linux lock layers are
  covered by the platform runs recorded above, not by the review.
- This record was written from the coordinator's summary of the reviewer's
  report. The reviewer's full report and its scratch probe are not in the
  repository.

## Closeout (2026-09-24)

Every criterion now has evidence, and the review the owner required is
complete, with no blocking finding. Each platform record is its own section
above.

| Criterion | Linux (`02e3d59d`) | Windows | macOS | Result |
| --- | --- | --- | --- | --- |
| A cancelled or busy open leaves the same store usable on its next call | Pass | Pass at `78c8ac85` (record `f7e93488`) and again at `bcd57840` (record `76a0ecdd`) | Pass at `70c4974a` (record `7d40ae8c`) | Pass. The residual I/O and recovery class is [[BPR-1111]] |
| A genuine post-admission corruption still latches, and is pinned | Pass | Pass, same binary | Pass, same binary | Pass |
| How this interacts with BPR-0541 and BPR-0644 | Recorded above | — | — | Pass |
| A throwing fallback `post` leaves the lane drainable and `is_quiescent()` reachable | Pass | Pass (`application_github_host`) | Pass (`application_github_host`) | Pass. [[BPR-1111]] tightens the test from at least one terminal to exactly one |
| The owner's independent read-only critical review of `02e3d59d` (decision `8e1858ab`) | — | — | Review at `2852dde0` | Pass: no blocking finding, and every finding is dispositioned above |

The Windows record `f7e93488` passed five of six rows. Its sixth,
`application_device_binding_relaunch`, was refused at its seed under
containment, for [[BPR-0876]]'s reason, before any device-binding load.
Record `76a0ecdd` then passed all six as registered.
