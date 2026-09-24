---
schema: "implement-rfc.card.v1"
id: "BPR-0906"
title: "The relocated activation endpoint leaves stale sockets in the shared per-user runtime directory"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux, in its 369-row sweep after BPR-0881's relocation","found_at":"88dc42fc","run":"run-20260916-stations","dispatch":"dsp-007"}
acceptance: [{"criterion":"AC1 A full test sweep leaves no more endpoints behind than it started with, or the card records why that is not worth buying","outcome":"pass","revision":"03117e63","evidence":{"kind":"command","detail":"Linux (bpr-main-linux, WSL2, GCC 15.2, Qt 6.11.2): full ctest sweep of 421 rows at -j8 with fresh HOME/XDG roots, offscreen; 0 bpr-activate-* added to /run/user/1000, 0 to the outer XDG_RUNTIME_DIR, 0 /tmp/bpr-run-* left. Trial T1 (no XDG candidate) leaked bpr-activate-e388a634121db7c1.sock from application_attended_launch_contract. macOS holds through its runner (5221f6ad); Windows uses named pipes.","path":"docs/issues/60-done/BPR-0906-the-relocated-activation-endpoint-accumulates-stale-sockets.md#linux-half-fixed-at-03117e63--bpr-main-linux-2026-09-23"}},{"criterion":"AC2 Whatever is chosen cannot unlink a live owner's endpoint, and a row shows that","outcome":"pass","revision":"03117e63","evidence":{"kind":"command","detail":"test_network_isolation_runtime_directory runs the launcher nested around a child that binds a socket and dies by SIGKILL, beside a stand-in user runtime directory holding a live endpoint: the child's directory is removed, the stand-in's entries are unchanged and the live endpoint still accepts a connection. Nothing new unlinks outside the launcher's own mkdtemp directory. Trials T2, T3, T4 fail this row at their own assertions.","path":"docs/issues/60-done/BPR-0906-the-relocated-activation-endpoint-accumulates-stale-sockets.md#verification-on-linux"}},{"criterion":"AC3 The decision is consistent with BPR-0895's, and each card says how","outcome":"pass","revision":"03117e63","evidence":{"kind":"freeform","detail":"No keep-alive or re-bind; the XDG_RUNTIME_DIR read is Linux-only, so macOS still resolves TMPDIR as BPR-0895 measured. This card's Decision section and BPR-0895's 'Consistent with BPR-0906' section both say how.","path":"docs/issues/60-done/BPR-0906-the-relocated-activation-endpoint-accumulates-stale-sockets.md#decision"}},{"criterion":"AC4 Windows is unaffected, or the card says why it is not","outcome":"pass","revision":"03117e63","evidence":{"kind":"freeform","detail":"Windows rendezvous is a named pipe with no filesystem entry; the product change is under __linux__ and the launcher is Linux-only (it #errors elsewhere).","path":"docs/issues/60-done/BPR-0906-the-relocated-activation-endpoint-accumulates-stale-sockets.md#decision"}}]
---

# BPR-0906: The relocated activation endpoint leaves stale sockets in the shared per-user runtime directory

## Defect

[[BPR-0881]] moved the profile activation rendezvous out of the data root,
because the in-root path exceeded `sun_path`. That fix is right and measured.
This is its residual, and it is the exact mirror image of [[BPR-0895]].

The old endpoint lived at `<profile root>/activate.sock` and **died with its
root** — a test's temporary data root goes away and takes the socket with it.
The new endpoint lives in the per-user runtime directory, which **outlives
every root that uses it**.

Measured rather than reasoned: `bpr-linux`'s 369-row sweep left **two**
owner-private (`srw-------`), non-listening `bpr-activate-*.sock` files in
`/run/user/1000`. There were none before the sweep. The station removed its
own two so the host was left as it was found.

Harmless in practice on Linux — `/run/user` is tmpfs, cleared at logout; the
names are per-root digests so they do not collide; and stale-endpoint recovery
is proven. But they accumulate within a session, and nobody had recorded that
they do.

## The pair this makes, which is the interesting part

Two cards now describe the same relocation from opposite ends, and they pull
in opposite directions:

- **[[BPR-0895]]** — on macOS the runtime directory resolves to `$TMPDIR`,
  which the system **age-reaps**. A long-idle owner keeps running while its
  endpoint name disappears, and the next launch degrades to `owner_busy`: the
  symptom BPR-0881 fixed.
- **This card** — on Linux nothing reaps it, so dead endpoints accumulate.

So reaping is the hazard on one platform and the absence of reaping is the
hazard on the other. **Whatever is decided for BPR-0895 should be decided with
this in view**, because a fix that keeps the endpoint alive on macOS (touching
it, re-binding it) makes the Linux accumulation slightly worse, and a fix that
has the owner unlink on exit helps here and does nothing for a reaped name.
They are cheaper to answer together than separately.

## Scope

- Establish whether the owner already unlinks on an orderly exit, and if so
  what paths skip it — a crash, a `SIGKILL`, a test harness terminating the
  process, or a root removed underneath it. The sweep's two survivors say at
  least one path does.
- Decide between the plausible answers, preferring the smallest: unlink on
  orderly exit and leave the rest to stale recovery; or have a launch sweep
  its own directory for endpoints whose digest names a root that no longer
  exists; or accept the accumulation on Linux and record why.
- Keep every guarantee [[BPR-0881]] records: the private-storage root still
  gates the rendezvous, the socket is still `S_ISSOCK` plus owner plus `0600`,
  peer credentials are still checked, and the runtime directory is still
  verified owner-private without following links.
- Do not let a cleanup step become a way to unlink somebody else's endpoint.
  The digest names are per-root and the directory is shared by every profile
  the user runs; a sweep that is wrong here takes a *live* owner's name away,
  which is worse than the accumulation it fixes.

## Measured on Linux: scope item 1 is answered

`bpr-linux` (dispatch `dsp-007`, `bpr-linux/int-16`) produced every path
below on the host rather than reasoning about it. Full evidence, commands and
exit statuses:
[BPR-0906 Linux measurement](../../verification/BPR-0906-linux-endpoint-exit-paths.md).
**No fix was chosen or implemented** — that stays joint with [[BPR-0895]].

**A survivor is reclaimed, not refused,** so the accumulation is inert. The
next start on the same root proves the leftover is dead — socket, owner, `0600`,
and a probe connection that must be refused — and only then unlinks and rebinds
(`src/adapters/sqlite/src/profile_activation.cpp:413-427`). A live owner answers
that probe and is left alone, measured. Two boundaries qualify it: the reclaim
only helps when a root is *reused*, and the harness never reuses one, so the
debris is never reclaimed by anybody; and a leftover that is *not* a socket is
refused forever, which is the shape that would make this harmful.

The owner does unlink on an orderly exit, in `~native_server()`
(`profile_activation.cpp:263-280`). The paths that skip it:

- **A crash (`SIGABRT`, `SIGSEGV`), `SIGKILL`, `SIGTERM`, `SIGINT`, `SIGHUP`** —
  the unlink is a destructor and there is no signal handler anywhere in `src/**`.
- **A CTest `TIMEOUT`** — measured to kill the whole process group uncatchably;
  `TIMEOUT_SIGNAL_NAME` is set nowhere in this tree.
- **An orderly exit whose data root was removed, or replaced, underneath it** —
  the destructor runs, exits 0, prints `OWNER_RELEASED`, and still declines,
  because the lease guard can no longer prove it owns the root
  (`src/adapters/sqlite/src/catalog_database.cpp:4378-4392`). That guard is
  correct and is what stops an owner unlinking a name it no longer owns, but it
  means *orderly exit* alone does not imply cleanup.
- **A failed assertion inside a live owner**, which calls `std::exit` and so
  skips `main`'s automatic destructors
  (`tests/end_to_end/profile_activation_public_path_tests.cpp:22-27`).

**A second leaking row was found, and it is not an activation row.** Every
owner-only start binds an endpoint (`src/app/src/github_application.cpp:12765`),
so any row that starts a real owner and then signals it leaks one.
`application_attended_launch_contract` does exactly that: an attended launch is
designed never to exit on its own, so the test must signal it, at
`tests/end_to_end/attended_launch_contract.py:150`. Two consecutive **passing**
runs left two different endpoints. The activation rows themselves leave none.
So the sweep's two survivors should not be assumed to be the activation rows'
doing, and a fix aimed only at those rows would miss this.

## Acceptance

- A full test sweep leaves no more endpoints behind than it started with, or
  the card records why that is not worth buying.
- Whatever is chosen cannot unlink a live owner's endpoint, and a row shows
  that.
- The decision is consistent with [[BPR-0895]]'s, and each card says how.
- Windows is unaffected, or the card says why it is not.

## BPR-0895 decided: no keep-alive, and macOS accumulates too — 2026-09-23

[[BPR-0895]] observed that macOS's daily `$TMPDIR` age reap skips sockets, so
an idle owner keeps its endpoint and no product change was made there. Nothing
keeps an endpoint alive or re-binds it, so this card's choice (unlink on exit,
a launch sweep, or accepting the debris) is unconstrained by that one.

The same observation measured this card's residual on macOS. On 2026-09-23 the
Mac integrator's `$TMPDIR` held 20 `bpr-activate-*.sock` entries born between
2026-09-19 and 2026-09-22, none held by any process (`lsof -U`). They are
test-run debris that the reap leaves because it skips sockets, so macOS
accumulates them just as `/run/user/<uid>` does. A reboot clears both.

## macOS: test sweeps leave nothing, and the residual is Linux's — 2026-09-23

The Mac main station checked the macOS half while running its batch. The
product's candidates for the runtime directory are `/run/user/<uid>`, then
`$TMPDIR`, then `/tmp` (`src/adapters/sqlite/src/profile_activation.cpp`,
`runtime_directory`). macOS has no `/run/user`, so the endpoint goes in
`$TMPDIR`. For every row it launches, `scripts/run_network_isolated_test.py`
sets `TMPDIR` to the row's own mode-0700 fixture root and removes that root
when the row ends. On macOS, then, a test's endpoint dies with its row.

Measured: the Mac ran more than 150 row executions through the launcher on
2026-09-23. They include the composed epoch, workbench, large-review and
profile courses, several of them repeated, and some that ended red. After
all of them, the user's real `$TMPDIR` holds the same 20
`bpr-activate-*.sock` files it held before. By their birth times, 1 is from
2026-09-19, 11 from 2026-09-21 and 8 from 2026-09-22, and none is from
2026-09-23. None of the 8 leftover `/tmp/bpr-net-*` roots, dated 2026-09-15
to 2026-09-20, holds a socket. So the Mac debris [[BPR-0895]] counted came
from owners started outside the launcher, with the user's own `TMPDIR`, and
then killed. It did not come from sweeps.

- **Acceptance line 1 holds on macOS through the runner.** No product change
  was involved.
- **Windows is unaffected.** Its rendezvous is a named pipe,
  `\\.\pipe\bpr-activate-<digest>` (`pipe_name`, same file). A named pipe
  has no filesystem entry, and it goes away when its last handle closes, which
  includes the process dying.
- **Linux is what remains.** `/run/user/<uid>` comes first there, so the
  launcher's `TMPDIR` never reaches it. One small direction to weigh with the
  options above: prefer `$XDG_RUNTIME_DIR` when it is set, verified with the
  same owner-private checks. That is the standard spelling of the same
  directory. The Linux launcher (`tests/network_isolation/linux_network_sandbox.cpp`)
  could then point it at the row's private root, as macOS does with `TMPDIR`.
  That would contain test debris without any sweep that could reach a live
  owner's name, and it would not change what a user's session resolves to.
  It has to be built and measured on Linux.

The card narrows to `platforms: ["linux"]` and stays in Backlog.

## Promoted to Ready for the Linux half — bpr-main-linux, 2026-09-23

Nothing defers this card any more. [[BPR-0895]] is Done, the macOS half is
recorded above, and the card has no dependencies. The station's
`/run/user/1000` held 56 `bpr-activate-*.sock` files at `7b173389`, dated
2026-09-21 to 2026-09-23, so the Linux residual is current. The direction
recorded above is taken: honour `$XDG_RUNTIME_DIR` on Linux, and have the
Linux launcher give each row its own runtime directory.

## Linux half fixed at `03117e63` — bpr-main-linux, 2026-09-23

Claimed at `073c7b09`. The product, launcher, tests and documentation are one
commit, `03117e63`. It was verified on the same tree before its rebase. The
commits the rebase brought in changed only two other cards.

### What changed

- **The product.** `runtime_directory()` in
  `src/adapters/sqlite/src/profile_activation.cpp` takes `$XDG_RUNTIME_DIR`
  first, on Linux only. The variable counts only when it names an absolute
  directory that is itself owner-private: mode `0700` and owned by the user,
  checked the way `/run/user/<uid>` is. Unlike a shared candidate, it earns no
  `bpr-<uid>` child. So an inherited value, as `sudo` can leave, cannot put
  the endpoint inside another user's directory. The other candidates and
  their order are unchanged.
- **The Linux launcher.** `tests/network_isolation/linux_network_sandbox.cpp`
  makes a fresh mode-0700 `/tmp/bpr-run-XXXXXX` for each row and names it in
  `XDG_RUNTIME_DIR`. Once the child is reaped, the launcher removes that
  directory and everything in it. A removal that fails prints
  `BPR_TEST_RUNTIME_DIRECTORY_LEFT:` and does not change the verdict.
- **The session bus.** With no bus address set, a D-Bus client looks for the
  bus at `$XDG_RUNTIME_DIR/bus`. So when `DBUS_SESSION_BUS_ADDRESS` is unset,
  the launcher first sets it to that socket, under GIO's own test: a socket
  this user owns. An address that is already set passes through unchanged.
  The row reaches the same bus it would have reached before, or none.
- **Tests.** There is a new Linux row, `test_network_isolation_runtime_directory`,
  and a new Linux case at the start of `sqlite_profile_activation`.
  `profile_activation_contract.py` mirrors the new candidate. The
  `msvc_secure_crt_contract` allowlist names the launcher, because it never
  builds on Windows.
- **Docs.** `docs/development/build-and-test.md` describes the launcher's
  runtime directory. The Linux measurement file now points here.

### Decision

The fix is the direction the macOS half recorded: contain test debris in the
launcher, the way the macOS runner already does with `TMPDIR`. The card's
three original options were not taken:

- **Unlink on orderly exit.** This was already implemented, and signals skip
  it.
- **A launch-time sweep of the shared directory.** This is the one option
  that can take a live owner's name, so it was ruled out.
- **Accepting the accumulation.** Test debris no longer accumulates.

Nothing new unlinks anything outside the launcher's own `mkdtemp` directory.

**What a user's own session keeps.** In a normal session `XDG_RUNTIME_DIR`
is `/run/user/<uid>`, so a user's owner resolves the same directory as
before. An owner killed by a signal still leaves its socket there. That
residual is accepted:

- There is at most one socket per profile root.
- The next start of that profile reclaims it, as the Linux measurement shows.
- `/run/user` is cleared at logout.

The accumulation measured on the stations came from test rows with fresh
roots, and the launcher now contains those.

**Consistent with [[BPR-0895]].** That card added no keep-alive or re-bind,
and neither does this one. The new read is Linux-only. So macOS still
resolves `$TMPDIR` exactly as BPR-0895 measured, and its runner still confines
test sockets there. BPR-0895's own "Consistent with BPR-0906" section still
holds.

**Windows is unaffected.** Its rendezvous is a named pipe, the product change
is under `__linux__`, and the launcher is Linux-only.

### Verification on Linux

WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2, `build/dev` (Debug), rebuilt with no
warnings. Every run went through `ctest`, offscreen, with `DISPLAY` and
`WAYLAND_DISPLAY` unset. Each run had fresh temporary `HOME` and
`XDG_CONFIG_HOME`/`DATA`/`CACHE`/`STATE`/`RUNTIME` roots. A wrapper recorded
the `bpr-activate-*` names each run added to `/run/user/1000` and to the
outer runtime directory, and any `/tmp/bpr-run-*` it left. The logs are
station records under `build/logs/bpr-0906/` and are not committed.

| Claim | Check | Result |
| --- | --- | --- |
| The defect is current | The station's `/run/user/1000` before any work | 56 `bpr-activate-*.sock`, dated 2026-09-21 to 2026-09-23 |
| A sweep leaves nothing behind | Full sweep, 421 rows, `-j8` (`full-sweep-candidate.log`) | 0 added to `/run/user/1000`, 0 added to the outer runtime directory, 0 `/tmp/bpr-run-*` left. 414 passed; the 7 failures are classified below |
| The known leaking row no longer leaks | `application_attended_launch_contract` in the focused pass, the final pass and 5 repeats | Passes and adds nothing. In trial T1 below, the same row added `bpr-activate-e388a634121db7c1.sock` to `/run/user/1000` |
| The product follows a private `XDG_RUNTIME_DIR` only | `sqlite_profile_activation`, new case: private followed and bound, then empty after the drain; widened to `0755` passed over and no child made; relative passed over | Pass: `runtime-directory:xdg-private=followed:xdg-shared=passed-over:xdg-relative=passed-over` |
| The cleanup cannot reach a live owner, and each row gets its own directory | `test_network_isolation_runtime_directory` runs the launcher nested, around a child that binds a socket, adds a directory and dies by `SIGKILL`. A stand-in user runtime directory holds a live endpoint and a `bus` socket | Pass: the child's directory differs and is gone afterwards. The stand-in's entries are unchanged, and its live endpoint still accepts a connection. With no address set, the child got the stand-in's bus; a set address passed through unchanged |
| No regression on the composed paths | Final pass, 22 rows (`integration-final.log`): both architecture boundary rows, `msvc_secure_crt_contract`, `coding_format_check`, both documentation rows, all 8 `test_network_isolation_*` rows, the 5 profile activation, owner admission and attended launch rows, and the 3 Linux Secret Service rows | 22/22 |
| Stable | `sqlite_profile_activation`, the new row and the attended launch row, each repeated 5 times | Pass, nothing added |
| Gates | clang-format 22 on the four touched C++ files. Scoped clang-tidy on the three translation units. `verify_documentation.py` | Format passes. Tidy is clean after one naming fix in the new test. `DOCUMENTATION_OK` |

**Negative controls.** Each trial broke one part of the change, rebuilt, and
failed at its own assertion. The candidate was then restored, and its diff
was compared byte for byte with the saved one.

- **T1: no `XDG_RUNTIME_DIR` candidate in the product.**
  `sqlite_profile_activation` fails at `a private XDG_RUNTIME_DIR holds the
  endpoint`. The attended launch row passes but leaves a socket in
  `/run/user/1000`. That socket was removed by name afterwards.
- **T2: the launcher does not export the directory.** The runtime row fails
  with `NOT_PER_ROW`, and the attended launch row's socket lands in the outer
  runtime directory.
- **T3: the launcher does not remove the directory.** The runtime row fails
  with `RUNTIME_LEFT`. The two directories it left were removed by name.
- **T4: no bus pin.** The runtime row fails with `BUS_CHANGED`.
- **T5: the product takes the variable unverified.** `sqlite_profile_activation`
  fails at `a shared XDG_RUNTIME_DIR is passed over`.

**The sweep's 7 failures.** None of them comes from this change:

- **Known reds.** `application_uiforetw_selected_diff_contract` ([[BPR-1036]]),
  `application_agent_finding_dif_context_contract` ([[BPR-0966]]'s row) and
  `application_threaded_reply_contract` ([[BPR-1046]]).
- **`qt_anonymous_public_preview_scale_100/125/200`.** All three fail at
  `GitHub.com's Connect, while it is not chosen, activates exactly its native
  HTTPS profile`, which is [[BPR-1023]]'s recorded failure. They fail the same
  way with the launcher rebuilt from `073c7b09`.
- **`msvc_secure_crt_contract`.** This one was caused by this change: the
  gate reads text, and the first candidate's `std::getenv` reads were not
  spelled for MSVC. It was fixed before the commit, and the row passes in the
  final pass.

### Not observed on this station

- **A native Linux desktop session.** This is WSL2. There `XDG_RUNTIME_DIR`
  is `/run/user/1000` and a session bus address is set. On a stock
  GNOME/systemd session the product should resolve the same
  `/run/user/<uid>` as before, but that was not observed. Acceptance does not
  depend on it.
- **A host with no bus address set, using the real Secret Service.** The pin
  was exercised in two ways. The new row uses a stand-in bus socket. And
  `env -u DBUS_SESSION_BUS_ADDRESS` on the launcher here gave the child
  `unix:path=/run/user/1000/bus`. The Secret Service rows were not run with
  the address unset. Doing that here would have meant using the real
  `/run/user/1000` as the outer runtime directory.

**No independent review.** The launcher's recursive removal and the product's
new candidate are the two surfaces a reviewer would examine. The removal is
bounded to the launcher's own `mkdtemp` path, and `std::filesystem::remove_all`
does not follow symlinks. Both surfaces are pinned by the rows and trials
above. The Linux lane ran as a leaf that cannot start a reviewer.

### Acceptance status

- **A full test sweep leaves no more endpoints behind than it started with.**
  Linux: pass at `03117e63`, 0 added in 421 rows. macOS holds through its
  runner (above). Windows has no filesystem endpoint.
- **Whatever is chosen cannot unlink a live owner's endpoint, and a row shows
  that.** Pass: `test_network_isolation_runtime_directory`.
- **The decision is consistent with BPR-0895's, and each card says how.**
  Pass: this section and BPR-0895's "Consistent with BPR-0906".
- **Windows is unaffected, or the card says why it is not.** Pass: named
  pipes, recorded above.

## Post-close owner decision, 2026-09-24: yes, an independent critical review (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589. Candidate `BPR-0906-independent-review-v001`, revision `03117e63`,
fingerprint `b1a8c022…`, decision `96421f40-a5d6-4cbf-90b4-e855e8650f8b`, sequence 576,
2026-09-24T18:59:20Z, `actor: human`: **select**, verbatim:

> Yes: run an independent read-only review of 03117e63

Note, verbatim:

> Yes double check with rfc-reviewer-critical

An `rfc-reviewer-critical`, dispatched by the coordinator and not by the lane
that wrote the change, reviews `03117e63` read-only and reports its findings. It
changes no code, board state or GitHub. The card stays Done. A finding that
needs work gets its own card, and the review's result is recorded here.

## Independent review (2026-09-24)

**Who and how.** An `rfc-reviewer-critical` (`claude-fable-5-1`) reviewed
`03117e63`. The coordinator dispatched it, as the owner decided above
(decision `96421f40`), and the lane that wrote the change did not. It worked
read-only, on macOS, from source. It changed no code, board state or GitHub.
This record replaces the "No independent review" paragraph above; that
paragraph stays as history.

**Verdict: no blocking finding.** Two things held:

- The launcher only ever deletes its own `mkdtemp` result.
- The product takes an `XDG_RUNTIME_DIR` only when the directory is owned by
  `geteuid()` with mode `0700`.

**Non-blocking findings, in the reviewer's order.** Line references are the
reviewer's.

1. An over-long private `XDG_RUNTIME_DIR`, 73 bytes or more, now fails with
   `endpoint-path-too-long` instead of falling through
   (`src/adapters/sqlite/src/profile_activation.cpp:185-187`, `:222`). The
   reviewer's fix: skip the XDG candidate when its size plus 35 is at least
   `sizeof(sun_path)`.
2. Two launches with different private `XDG_RUNTIME_DIR`s no longer meet.
   They degrade to the lease's owner-busy path, which is the [[BPR-0881]]
   symptom. It is intended, but this card does not document it.
3. The AC2 row cannot catch a cleanup that follows symlinks
   (`tests/scripts/network_isolation_counterexamples.py:214-230`). The
   reviewer suggests adding escape symlinks.
4. The other-owner claim is not pinned
   (`tests/sqlite_contract/profile_activation_tests.cpp:350-396`). The
   reviewer suggests testing `XDG_RUNTIME_DIR=/root` or `/var/lib/private`.
5. On a host that has no bus address but does have a live
   `$XDG_RUNTIME_DIR/bus`, the bus pin sends the Secret Service rows to the
   real bus (`tests/network_isolation/linux_network_sandbox.cpp:138-151`;
   `tests/github_contract/linux_secret_service_native_tests.cpp:1288-1291`).
6. The launcher has small `/tmp` leak windows
   (`linux_network_sandbox.cpp:328-331` and `:335`). A signal that arrives
   between `mkdtemp` at `:284` and `:328` leaves one too.
7. This card's wording is inaccurate. "Without following links" does not
   hold for the configured and top-level candidates, which are checked with
   `stat` (`profile_activation.cpp:185`, `:193`). Only the `bpr-<uid>` child
   uses `lstat` (`:197`).

**Disposition.** [[BPR-1113]] (Linux, Ready) takes findings 1, 3, 4 and 6 as
work, and findings 2, 5 and 7 as documentation. The reviewer judged every
finding non-blocking, and none of them contradicts one of the four
acceptance records, so the card stays Done. Finding 7 corrects this card's
wording. The text above is left as it was written, and this record is the
correction.

**Limitations.**

- The review read the Linux-only code, meaning the `__linux__` candidate and
  the Linux launcher, but did not compile or run it on Linux. No test run is
  reported for this review.
- This record was written from the coordinator's summary of the reviewer's
  report. The reviewer's full report is not in the repository. The summary
  does not name the revision the reviewer's checkout was at. The cited
  product and launcher files are unchanged from `03117e63` to `f985b4c8`,
  the revision this record was written on.
