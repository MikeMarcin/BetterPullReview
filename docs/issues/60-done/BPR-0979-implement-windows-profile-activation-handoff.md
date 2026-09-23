---
schema: "implement-rfc.card.v1"
id: "BPR-0979"
title: "Implement bounded Windows profile activation and New Window handoff"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0710", "BPR-0711", "BPR-0813", "BPR-0932"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0024-closeout-r2","mandate_revision":"r3","deferred_followups":["BPR-0980"]}
acceptance: [{"criterion":"Native Windows repeated ordinary launch activates the owner profile's MRU window; explicit New Window creates another window without a second owner. Under owner amendment r3 the native desktop observation of these gestures is transferred unaccepted to standalone BPR-0980; this row accepts only the real-process Windows course.","outcome":"pass","revision":"1cd7f2f1ae1410aaab38bd0610ecd0d2063032ef","evidence":{"kind":"command","detail":"Owner-amended r3 scope only (owner-closeout-r3.md). On the bpr-win station (JaegAW, Windows 11 10.0.26200, MSVC 19.51, Qt 6.11.2) application_profile_activation_public_path passes at this commit (12.98 s, PROFILE_ACTIVATION_OK, build/bpr0979-windows/ctest-final.log): real Windows processes, ordinary activation reaching the owner's exact MRU window through QApplication::activeWindow(), explicit New Window admitted without a second owner, an independent second profile, owner death/recovery and the closing-owner refusal, all under the offscreen platform. The same gestures on the native Windows desktop were served as attended candidate BPR-0979-windows-activation-v001 (fingerprint bc3d75f62152e8f2a6cb18e6c67222d4b20878bddfebeb00b8b681fd7a39c1e9, revision windows-dev-674c68dc-1cd7f2f1-2026-09-22; station build C:/Users/mikem/bpr0979-044e8779/source/build/windows-dev/src/app/Debug/better_pull_review.exe sha256 674c68dc0f777c47fa7233bea0f21178bf24f9b52aa05b7fd23a418c18f3d4ef) and received no verdict; that observation is preserved on BPR-0980 and is not passed here.","path":"docs/issues/60-done/BPR-0979-implement-windows-profile-activation-handoff.md#owner-amendment-r3--2026-09-22"}},{"criterion":"Concurrent profiles and owner shutdown/restart remain isolated; malformed, cross-profile, untrusted endpoint and timed-out requests fail closed within explicit bounds, with no credential or arbitrary-command transport.","outcome":"pass","revision":"1cd7f2f1ae1410aaab38bd0610ecd0d2063032ef","evidence":{"kind":"command","detail":"ctest --preset windows-dev -R '^(sqlite_profile_activation|application_profile_activation_public_path)$' --output-on-failure -V on bpr-win at this commit: sqlite_profile_activation Passed 5.83 s (profile-activation-tests:pass; the Windows branch's authority and lease refusals, duplicate listener, drained-name rebind, missing owner, two client processes routed by exact root, refused New Window, timed-out and closing-owner indeterminacy, six hostile frames, a split frame and a stalled peer) and application_profile_activation_public_path Passed 12.98 s (second profile independent, owner death/recovery, closing-owner refusal). The same bounds as POSIX: two seconds, one 16-byte message, two-byte reply, no credential or command. macOS regression at the same source, rerun at closeout head 4e58ab78: sqlite_profile_activation 4.23 s, activation course 4.64 s, window layout 23.69 s, 3/3 (build/rfc0024-r3-macos-regression.log).","path":"build/bpr0979-windows/ctest-final.log"}},{"criterion":"Focused native Windows adapter/public tests and POSIX regressions pass. Unrun native acceptance remains pending and cannot be inferred from compile success or another platform.","outcome":"pass","revision":"1cd7f2f1ae1410aaab38bd0610ecd0d2063032ef","evidence":{"kind":"artifact","detail":"Windows: windows-dev Debug build of test_profile_activation, test_profile_activation_public_path and app_better_pull_review at this commit; 2/2 rows pass (build/bpr0979-windows/ctest-final.log; configure.log, build*.log and the two diagnosed failures ctest2.log/ctest3.log retained). The two failures were the course's own Windows fixture narrowing (USERDOMAIN principal, %TEMP%-inherited parent), fixed in tests/end_to_end/bpr_narrow_owner_private.py and profile_activation_contract.py with no product change. POSIX: build/bpr0979-focused.log at d5a2426c plus the activation and window-layout courses rerun at this commit and again at 4e58ab78 (3/3). The unrun native desktop acceptance is not inferred from these; it is preserved, still pending, on standalone BPR-0980 under owner amendment r3.","path":"docs/issues/60-done/BPR-0979-implement-windows-profile-activation-handoff.md#windows-station--2026-09-22"}}]
---

# Implement bounded Windows profile activation and New Window handoff

## Scope

Complete RFC-0024 AC-06/09's native Windows single-owner launch contract.
Keep the existing exclusive profile runtime lease. Add a same-user local,
bounded activate/New Window handoff to that owner; never transport credentials
or arbitrary commands. Validate identity, request size/version, timeouts,
endpoint ownership and lifecycle; distinct profiles remain independent.
Preserve the existing POSIX short-rendezvous implementation and evidence.

Owned surfaces: profile activation adapter/contract and native Windows
implementation, minimal composition wiring if needed, adapter/public-path
tests and native candidate evidence. Never edit live station checkouts.

## Acceptance

- Native Windows repeated ordinary launch activates the owner profile's MRU
  window; explicit New Window creates another window without a second owner.
- Concurrent profiles and owner shutdown/restart remain isolated; malformed,
  cross-profile, untrusted endpoint and timed-out requests fail closed within
  explicit bounds, with no credential or arbitrary-command transport.
- Focused native Windows adapter/public tests and POSIX regressions pass.
  Unrun native acceptance remains pending and cannot be inferred from compile
  success or another platform.

## Evidence

Baseline: the Windows adapter returned unavailable; runtime leases alone did
not satisfy activation or explicit New Window.

### Implementation — 2026-09-22

Candidate: working tree over claim `9b9a267e`, macOS build only. The Windows
transport in `src/adapters/sqlite/src/profile_activation.cpp` is an
owner-private local named pipe (`\\.\pipe\bpr-activate-<digest>`, digested
from `canonical_windows_root`) bound under the same runtime lease, with the
same 16-byte frame, the same two-byte acknowledgement and the same
queued/refused/unavailable/indeterminate outcomes as the POSIX socket. The
descriptor is the adapter's existing owner-private one (user SID, plus the
package SID when contained, protected DACL); the first instance is created
with `FILE_FLAG_FIRST_PIPE_INSTANCE`, so an existing name refuses the owner
rather than being joined; a second instance keeps the name bound while a
delivered request travels with its reply, so the name is never free while an
owner lives. Message mode delimits the frame: a short, long or split message
is refused, not reassembled. Both ends check the peer process's token user;
the client additionally requires the owner-private descriptor on the pipe
and opens with `SECURITY_IDENTIFICATION` so the owner cannot impersonate it.
Server-side polling is overlapped and non-blocking on the effect lane; a
stalled peer expires after the same two-second bound; replies are written
with a bounded wait and the instance closed without discarding them. No
credential, provider command or general operation crosses the pipe. The
shared frame/digest helpers are hoisted; the POSIX section is otherwise
unchanged.

Windows tests, registered but not run here: the adapter contract's Windows
branch (`tests/sqlite_contract/profile_activation_tests.cpp`: authority and
lease refusals, duplicate listener, drained-name rebind, missing owner, two
independent client processes routed by exact root, refused New Window,
timed-out and closing-owner indeterminacy, six hostile frames, a split
frame, a stalled peer) and the public course
`application_profile_activation_public_path`, whose CMake gate is no longer
`UNIX`-only; `profile_activation_contract.py` gains a Windows branch that
narrows the root with the existing helper and skips only the POSIX endpoint
unlink/`unavailable` step, which has no named-pipe equivalent. Both rows and
`sqlite_profile_activation` are pinned `WINDOWS_UNCONTAINED`
(`dos-volume-name`, reasoned from the lease and profile rows, not measured).

POSIX regression on macOS (`build/bpr0979-focused.log`):
`sqlite_profile_activation` 4.49 s, `sqlite_profile_runtime_lease` 0.06 s,
`application_profile_activation_public_path` 5.61 s,
`application_profile_owner_admission_public_path` 2.66 s and
`test_network_isolation_structure_contract` pass; `test_network_isolation_manifest`
fails only on main's pre-existing [[BPR-0962]] rows. `check_boundaries`
passes with the existing exact local-listener exception
(`build/bpr0979-boundaries.log`); format check, `git diff --check` and the
documentation gate pass (`build/bpr0979-docs.log`); clang-tidy with
warnings as errors passes on the adapter and its contract
(`build/bpr0979-tidy.log`). Architecture, threat
model, release-readiness G9 and the authority inventory record the Windows
transport.

Not claimed: any Windows compile, test or native observation. The Windows
adapter branch, its contract and the public course need the Windows station
to build the branch head, run `sqlite_profile_activation` and
`application_profile_activation_public_path`, then observe a repeated
ordinary launch activating the owner's MRU window and an explicit
`--new-window` launch opening another window on the native desktop. Compile
success or another platform's pass cannot substitute; the `GetSecurityInfo`
read of the pipe's descriptor on the client side is an assumption the
station run will confirm or refute (a refutation surfaces as `unavailable`,
the pre-existing behavior, and fails the contract by name).

### Windows station — 2026-09-22

Station `bpr-win` (JaegAW, Windows 11 Home 10.0.26200, `windows-native`),
reached through the configured SSH `aito-main-win`; its live checkout
`C:/code/git/bpr-windows` (`windows-lane`, HEAD `b9cf4edb`, dirty MASK lock)
was read for objects only and not edited. The branch head travelled as a
git bundle into the task-owned clone
`C:/Users/mikem/bpr0979-044e8779/source`, configured with the repository's
`windows-dev` preset (Visual Studio 18 2026, MSVC 19.51.36257, Debug) and
`-DBPR_QT_ROOT=C:/Users/mikem/bpr0014-m9-qt6112-92c4e159/qt/6.11.2/msvc2022_64`
(the pinned Qt 6.11.2 already on the host); CMake 4.4.2, Python 3.14.6.
Windows Defender reported no detection during the run.

At `044e8779` the adapter contract's Windows branch passed at once:
`sqlite_profile_activation` **Passed 5.85 s**, `profile-activation-tests:pass`
(`build/bpr0979-windows/ctest.log`). So the named pipe, its owner-private
descriptor, the client's `GetSecurityInfo` read of it, the token-user checks,
message-mode framing, the six hostile frames, the split frame, the stalled
peer and the two client processes routed by exact root behave on Windows as
the source claimed; the `unavailable` refutation named above did not occur.

`application_profile_activation_public_path` failed twice before it passed,
neither time in the product:

- The first run could not launch `better_pull_review.exe`: only the two test
  targets had been built (`ctest.log`). Building `app_better_pull_review`
  fixed that.
- The second run (`ctest2.log`) failed in the course's own fixture
  narrowing: `bpr_narrow_owner_private.py` granted `USERDOMAIN\USERNAME`,
  and an OpenSSH session on this workgroup host reports
  `USERDOMAIN=WORKGROUP`, a principal icacls cannot map (exit 1332). The
  helper now grants the token user's SID through the same token read that
  already produced the AppContainer SID.
- The third run (`ctest3.log`) reached `OWNER_READY`, both activations,
  `MRU_FOCUSED`/`MRU_ACTIVATED` and the second profile, then the Default
  owner's orderly drain exited 1 with
  `PROFILE_QUIT_CLEANUP_SOURCE:agent-workspace-recovery:…:repair-required`.
  The course's Windows fixture parent inherits `%TEMP%`'s entries, and this
  host's `%TEMP%` grants `JaegAW\CodexSandboxUsers` and a second SID Modify,
  so the neighbourhood beside the Default data root refused its parent; the
  profile-2 owner under `profiles/` released cleanly because it inherits the
  narrowed root. The course now narrows its parent as mode 0700 does on
  POSIX. Product source is unchanged.

Both fixes are `1cd7f2f1`, fetched and checked out on the station; the rows
were rebuilt and rerun there at that exact commit
(`build/bpr0979-windows/ctest-final.log`): `sqlite_profile_activation`
**Passed 5.83 s**, `application_profile_activation_public_path` **Passed
12.98 s**, `PROFILE_ACTIVATION_OK`, 2/2. Executable digests at `1cd7f2f1`:
`profile_activation.exe` `8c2c3314…`, `profile_activation_public_path.exe`
`b4200f86…`, `better_pull_review.exe` (Debug, tests enabled)
`674c68dc0f777c47fa7233bea0f21178bf24f9b52aa05b7fd23a418c18f3d4ef`. On macOS
the same commit passes the activation course (5.6 s) and the window-layout
course (23.5 s), the other user of the narrowed descriptor.

What this proves: real Windows processes, the named pipe under the runtime
lease, ordinary activation reaching the owner's exact MRU window through Qt
(`QApplication::activeWindow()`), explicit New Window admitted by the owner
without a second owner, a second profile independent, owner death and
recovery, and the closing-owner refusal — under the offscreen platform. What
remains: the same repeated ordinary launch and `--new-window` observed on the
native Windows desktop, a human/native observation this run cannot
self-certify. The candidate for it is the Debug application above on the
station; a tests-disabled `windows-app-release` image was not built here
([[BPR-0974]] records that host's Defender quarantine of the last such
image). The task root is left in place, inert, for that observation.

### Owner amendment r3 — 2026-09-22

After about eight hours with no verdict on `BPR-0979-windows-activation-v001`,
the owner directed the Windows observation deferred. The
[r3 amendment](../../rfcs/plans/ux-redesign-pack-0028/owner-closeout-r3.md)
changes acceptance; it is not a verdict on that candidate and this card does
not call the native desktop observation passed. The first acceptance row's
native desktop half moves, unchanged, to standalone [[BPR-0980]]
(`rfcs: []`, Blocked on `native-desktop-observation`), which preserves the
exact candidate id, fingerprint `bc3d75f6…`, revision
`windows-dev-674c68dc-1cd7f2f1-2026-09-22`, the station build path and
digest `674c68dc…`, and the passing `ctest-final.log` rows above. The
candidate's protection was released (session event 14) and the server
stopped; the session's durable records are intact.

This card finishes Done on what was actually observed: the Windows
transport's bounds/isolation contract and public course on `bpr-win` at
`1cd7f2f1` (2/2), and the macOS regressions at the same source, rerun at
the closeout head `4e58ab78` (`sqlite_profile_activation` 4.23 s,
`application_profile_activation_public_path` 4.64 s,
`application_profile_window_layout_public_path` 23.69 s, 3/3,
`build/rfc0024-r3-macos-regression.log`). No product source changed after
`1cd7f2f1`.
