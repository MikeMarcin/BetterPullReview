---
schema: "implement-rfc.card.v1"
id: "BPR-0980"
title: "Observe native Windows profile activation and New Window on the desktop"
status: "In Progress"
rfcs: []
depends_on: ["BPR-0979"]
priority: "P2"
platforms: ["win"]
metadata: {"origin_rfc":"RFC-BPR-0024","origin_card":"BPR-0979","closeout_card":"BPR-0595","mandate_revision":"r3","disposition":"owner-deferred standalone follow-up; not an RFC0024 closeout dependency","source_candidate":"1cd7f2f1ae1410aaab38bd0610ecd0d2063032ef","attended_candidate":{"id":"BPR-0979-windows-activation-v001","fingerprint":"bc3d75f62152e8f2a6cb18e6c67222d4b20878bddfebeb00b8b681fd7a39c1e9","revision":"windows-dev-674c68dc-1cd7f2f1-2026-09-22","session":"8d2c2e32f7131447c24c2fd522f18fe6"},"station_build":{"station":"bpr-win","path":"C:/Users/mikem/bpr0979-044e8779/source/build/windows-dev/src/app/Debug/better_pull_review.exe","sha256":"674c68dc0f777c47fa7233bea0f21178bf24f9b52aa05b7fd23a418c18f3d4ef"},"transferred_criteria":["AC-06 native Windows desktop observation: repeated ordinary launch activates the owner's MRU window; --new-window opens another window without a second owner"],"related_cards":["BPR-0974","BPR-0653"]}
---

# Observe native Windows profile activation and New Window on the desktop

## Owner transfer, distinct from observations

Owner amendment r3 to run `rfc-0018-0026-closeout-20260920` explicitly
defers the one native Windows desktop observation from RFC-0024 terminal
acceptance to this standalone card, exactly as amendment r2 deferred
[[BPR-0974]] and [[BPR-0975]] from RFC-0018. This card remains **Blocked**,
not accepted. The [bounded mandate](../../rfcs/plans/ux-redesign-pack-0028/owner-closeout-r3.md),
the [amended RFC](../../rfcs/RFC-BPR-0024-bpr-profiles-multiple-windows.md#owner-amendment-r3--2026-09-22)
and [[BPR-0979]]'s Windows station record retain the original requirement
and every result. No automated pass below is called a native desktop pass.

## Exact candidate and retained evidence

The transferred observation is unchanged from BPR-0979's first acceptance
row: on the native Windows desktop, a repeated ordinary launch activates the
owner profile's most recently used window, and an explicit `--new-window`
launch opens another window of the same profile in the running instance
without a second `better_pull_review.exe` owner.

The candidate that was served for it, and never judged, is attended
candidate `BPR-0979-windows-activation-v001` (fingerprint
`bc3d75f62152e8f2a6cb18e6c67222d4b20878bddfebeb00b8b681fd7a39c1e9`, revision
`windows-dev-674c68dc-1cd7f2f1-2026-09-22`) in session
`8d2c2e32f7131447c24c2fd522f18fe6` of the closeout worktree. Its protection
was released when the amendment arrived; its packet, the four numbered
steps and the copy fields remain in that session's durable records. The
executable it names is the station build on `bpr-win` (JaegAW, Windows 11
10.0.26200, MSVC 19.51.36257, Qt 6.11.2, `windows-dev` Debug, tests
enabled): `C:/Users/mikem/bpr0979-044e8779/source/build/windows-dev/src/app/Debug/better_pull_review.exe`,
sha256 `674c68dc0f777c47fa7233bea0f21178bf24f9b52aa05b7fd23a418c18f3d4ef`,
built from branch head `1cd7f2f1ae1410aaab38bd0610ecd0d2063032ef` in a
task-owned clone; the live checkout `C:/code/git/bpr-windows` was not
edited. No tests-disabled `windows-app-release` image was built for this
candidate; BPR-0974 records that host's Defender quarantine of the last such
image, and Defender reported nothing during the BPR-0979 run.

Passing automated Windows evidence at the same commit, retained as the
transport's bounds/isolation and test-row proof and nothing more:
`ctest --preset windows-dev -R '^(sqlite_profile_activation|application_profile_activation_public_path)$'`
on `bpr-win`, `sqlite_profile_activation` Passed 5.83 s and
`application_profile_activation_public_path` Passed 12.98 s,
`PROFILE_ACTIVATION_OK`, 2/2 (`build/bpr0979-windows/ctest-final.log`;
`configure.log`, `build*.log`, and the two diagnosed fixture failures
`ctest2.log`/`ctest3.log` beside it). That course drives real Windows
processes over the named pipe under the offscreen platform and reaches the
owner's MRU window through `QApplication::activeWindow()`; it cannot stand
in for the desktop observation, and neither can macOS's passing activation
course.

## Acceptance

- AC1: On the native Windows desktop, signed in as an ordinary user, start
  the exact candidate above (or a rebuilt candidate whose source, build
  flags and digest are recorded first) once, then again with the first still
  running: no second window opens and the existing window comes to the
  front with focus. Open a second window through Profiles > New Window,
  make it most recently used, launch again: that window, not the first,
  comes to the front.
- AC2: Launch with `--new-window`: a third window of the same profile opens
  in the running instance and Task Manager shows exactly one
  `better_pull_review.exe` a moment later. Bind every observation to the
  image digest and PID; record the step that differed and what was seen on
  any rejection. A Defender block or removal is a finding, not a pass.
- AC3: Preserve private-profile isolation and unchanged live-station
  hashes; account for every application and observer process, release the
  station lease and retain no writer. No sign-in, provider mutation,
  updater installation or security/display-policy change is authorized.
  Package-level Windows release qualification remains RFC-0038/BPR-0960 and
  the title-row work remains [[BPR-0653]]; neither is a prerequisite here.

## Ready path

Recheck station identity and leases before use; the task clone
`C:/Users/mikem/bpr0979-044e8779` is inert and no Windows process or
MSBuild worker was left running. If the branch head has moved past
`1cd7f2f1` in the profile activation adapter, desktop or application
composition, rebuild and re-prepare a new attended candidate revision
rather than reusing the fingerprint above. Record the verdict through the
configured attended-review workflow bound to the exact digest reviewed; this
card supplies no native pass at creation.

### Unblocked on bpr-main-win — 2026-09-23

The block was the observer's reach, not a product or owner dependency: the
`bpr-win` station ran over SSH, outside the owner's interactive desktop, so
the observation needed someone at the machine. The `bpr-main-win` station is
that desktop: an agent session in the owner's signed-in interactive session
(session 1, user `JaegAW\mikem`, not elevated) on the same host JaegAW,
checkout `C:/code/git/BetterPullReview` on `main`. It can launch the
candidate as an ordinary user and read the result back objectively (the
foreground window, the keyboard focus, the top-level windows and the
process table), so the observation is now available unattended. The card
moves to Ready for that station under the Mac integrator's relay of the
owner's mandate (run `bpr-main-win-20260923`).
