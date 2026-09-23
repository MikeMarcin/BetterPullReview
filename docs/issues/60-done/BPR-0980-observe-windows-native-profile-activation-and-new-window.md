---
schema: "implement-rfc.card.v1"
id: "BPR-0980"
title: "Observe native Windows profile activation and New Window on the desktop"
status: "Done"
rfcs: []
depends_on: ["BPR-0979"]
priority: "P2"
platforms: ["win"]
metadata: {"origin_rfc":"RFC-BPR-0024","origin_card":"BPR-0979","closeout_card":"BPR-0595","mandate_revision":"r3","disposition":"owner-deferred standalone follow-up; not an RFC0024 closeout dependency","source_candidate":"1cd7f2f1ae1410aaab38bd0610ecd0d2063032ef","attended_candidate":{"id":"BPR-0979-windows-activation-v001","fingerprint":"bc3d75f62152e8f2a6cb18e6c67222d4b20878bddfebeb00b8b681fd7a39c1e9","revision":"windows-dev-674c68dc-1cd7f2f1-2026-09-22","session":"8d2c2e32f7131447c24c2fd522f18fe6"},"station_build":{"station":"bpr-win","path":"C:/Users/mikem/bpr0979-044e8779/source/build/windows-dev/src/app/Debug/better_pull_review.exe","sha256":"674c68dc0f777c47fa7233bea0f21178bf24f9b52aa05b7fd23a418c18f3d4ef"},"transferred_criteria":["AC-06 native Windows desktop observation: repeated ordinary launch activates the owner's MRU window; --new-window opens another window without a second owner"],"related_cards":["BPR-0974","BPR-0653"]}
acceptance: [{"criterion":"AC1 On the native Windows desktop, as an ordinary user, a repeated ordinary launch brings the running window to the front with focus and opens no second window; after Profiles > New Window makes a second window most recently used, a launch brings that window, not the first, to the front","outcome":"pass","revision":"2c9dc0c7","evidence":{"kind":"observation","detail":"JaegAW, Windows 11 10.0.26200, interactive session 1, JaegAW\\mikem not elevated; build/dev Debug (MSVC 19.51, Qt 6.11.2) built at 43f9a9e1, whose src, tests/end_to_end and CMake are identical to 2c9dc0c7; better_pull_review.exe sha256 6b35d6ee7a77dec293d23d94ec871773ed88e839513735240e9c6076ffbc217b recorded before the run. tests/scripts/windows_profile_activation_course.ps1, private owner-private root, launches from a foreground launcher that received the last input (the shell's condition after a click). Owner PID 22584: first launch one window in front with focus; second ordinary launch PID 40884 PROFILE_ACTIVATION_QUEUED exit 0, still one window and one better_pull_review.exe, W1 0x4E0296 in front with keyboard focus; Profiles > New Window opened W2 0x1430E4A in front; third ordinary launch PID 41360 queued exit 0, two windows, one process, W2 (not W1) in front with focus. At c52f5907 (digest 6f1b83e3...) and in a negative control without the new AllowSetForegroundWindow call (digest 1ecb5d04...) both front-and-focus steps failed with every other check passing: the launcher stayed in front. Corrected by f73aa135.","path":"build/logs/bpr-0980-main-win/course-43f9a9e1.json"}},{"criterion":"AC2 A --new-window launch opens a third window of the same profile in the running instance and exactly one better_pull_review.exe remains, bound to the image digest and PIDs","outcome":"pass","revision":"2c9dc0c7","evidence":{"kind":"observation","detail":"Same run and digest: --new-window PID 39984 PROFILE_ACTIVATION_QUEUED exit 0; third window W3 0x350DCE opened in owner 22584; two seconds later the process table holds only 22584; W3 came to the front. Each window's own close then ended 22584 with exit 0. The pre-check-in harness run observation-final.json (owner 8160, same digest) recorded the same outcome.","path":"build/logs/bpr-0980-main-win/course-43f9a9e1.json"}},{"criterion":"AC3 Private-profile isolation and unchanged live-station hashes; every application and observer process accounted for; no lease or writer retained; no sign-in, provider mutation, updater installation or security/display-policy change","outcome":"pass","revision":"2c9dc0c7","evidence":{"kind":"artifact","detail":"Real %LOCALAPPDATA%\\BetterPullReview tree fingerprint cd4a335e... (40 entries), live station C:/code/git/bpr-windows HEAD b9cf4edb and status digest 97623b91..., and zero BetterPullReview Credential Manager entries identical before the first run (isolation-before-1.json) and after the last (isolation-after-all.json). Every sender exited 0, the owner exited 0 through its windows' closes, course/helper processes ended, task roots removed; no station lease of its own and no writer. No sign-in, provider mutation, update installation, or Defender/security/display setting change; Defender recorded no detection. Automated rows at the candidate: sqlite_profile_activation passed; application_profile_activation_public_path passed 3/3 after 2c9dc0c7 closed the course's own registry reads (it had failed 4/4 at cleanup with WinError 32, the course's python.exe holding the registry); application_profile_owner_admission_public_path fails contained with and without the change (BPR-0907). Two findings outside this card (window-close crash in ~implementation deleting Realigner; strip-menu Quit deleting a stack QMenu) are recorded on the card for new cards.","path":"build/logs/bpr-0980-main-win/isolation-after-all.json"}}]
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

## Native desktop observation — bpr-main-win, 2026-09-23

**Card revision.** The ready path above asked for an attended verdict because
the only station then could not see the desktop. Nothing in AC1–AC3 is a
human judgement: each is a foreground window, a keyboard focus, a window count
or a process count. This station reads all four back itself, so the verdict
below is an agent-captured native observation, and no attended session was
prepared. The owner can still reject it; the evidence names every image, PID
and window.

### Candidate

- Source: `f73aa135` (the correction below) and `2c9dc0c7` (the course's
  cleanup) on `main`. Built and observed at `43f9a9e1`, the same two commits
  before a rebase; `git diff 43f9a9e1 2c9dc0c7 -- src tests/end_to_end
  CMakeLists.txt cmake` is empty.
- Build: `build/dev` (`dev` preset: Ninja, MSVC 19.51 / 14.51.36231, Debug,
  `BUILD_TESTING=ON`, `BPR_DISTRIBUTION_BUILD=OFF`), Qt 6.11.2 from the CPM
  cache. `better_pull_review.exe` SHA-256
  `6b35d6ee7a77dec293d23d94ec871773ed88e839513735240e9c6076ffbc217b`,
  recorded before it ran. Debug images here are not bit-reproducible, so each
  run below names the digest it ran.
- Host: JaegAW, Windows 11 Home 10.0.26200, interactive session 1,
  `JaegAW\mikem`, not elevated. The foreground lock timeout reads
  2147483647 ms, so no window takes the foreground merely because the desk
  was idle.

### Method

[`windows_profile_activation_course.ps1`](../../../tests/scripts/windows_profile_activation_course.ps1)
drives the candidate against a data root narrowed owner-private by
`bpr_narrow_owner_private.make_root_private` under a task directory; every
launch passes `--test-application-data-root` to it. A launcher window of the
course's own stands in for the shell: before each launch it is put in front
and receives one F24 key press, so it is the foreground process and the one
that received the last input, as Explorer is after a click. Each launch is
its direct child. Without that press Windows gave the launcher's child no
foreground right at all: in the first run at `c52f5907` even the first
launch's window stayed behind the launcher. The pointer never moved, and no
key went to any other window. The application's own menu is reached with a
posted click on the strip's Application menu control and UI Automation, never
with injected keys, so the application never received input that would hand
it the right under test. Results are read from user32 and the process table.

### Result

`build/logs/bpr-0980-main-win/course-43f9a9e1.json` on the station
(16:58:08Z–16:58:34Z), owner PID 22584, windows `0x4E0296` (W1),
`0x1430E4A` (W2), `0x350DCE` (W3):

- AC1. First launch: one window, in front with keyboard focus. Second
  ordinary launch, PID 40884: `PROFILE_ACTIVATION_QUEUED`, exit 0; still one
  window; the process table holds only 22584; W1 in front with keyboard
  focus. Profiles > New Window opened W2, in front. Third ordinary launch,
  PID 41360: queued, exit 0; still two windows and one process; **W2, not
  W1,** in front with keyboard focus.
- AC2. `--new-window`, PID 39984: queued, exit 0; W3 opened in 22584, and two
  seconds later the process table holds only 22584. W3 came to the front.
- Each window's own close then ended 22584 with exit 0.

The task harness's run before the script was checked in
(`observation-final.json`, owner 8160, same digest) recorded the same
outcome at every step.

### Rejection before the correction

At `c52f5907` (digest `6f1b83e3…`, `observation-baseline-2.json`, owner
10032) every launch was queued and exited 0 with one window and one process,
but the step that differed was AC1's front-and-focus: after the second launch
(PID 43200) and the third (PID 696) the launcher was still in front, and
neither W1 nor the most recently used W2 came forward. Windows gives the
right to take the foreground to the process a person launched; the owner
that shows the window never had it. `f73aa135` has the sender hand that right
to the pipe's authenticated server process, and to no other, with
`AllowSetForegroundWindow` before the request. Negative control with the
checked-in course at the same source minus that call (digest `1ecb5d04…`,
`course-negative-control.json`, owner 46780): both front-and-focus checks
false, every other check true.

### Automated rows

`sqlite_profile_activation` passes (5.8 s) and
`application_profile_activation_public_path` passes three runs in a row
(14.3, 14.1, 14.1 s) at the candidate. Before `2c9dc0c7` that row failed 4/4
at cleanup with WinError 32 whatever the product source: a Restart Manager
watch named the course's own `python.exe` as the holder of
`profile-registry.sqlite3`, because `with sqlite3.connect(...)` does not
close. `sqlite_profile_runtime_lease` passes.
`application_profile_owner_admission_public_path` fails contained at
`SEED_REGISTRY_REFUSAL:0` with and without the change; that is [[BPR-0907]].

### AC3

- Isolation: the fingerprint of the real
  `%LOCALAPPDATA%\BetterPullReview` tree (40 entries, `cd4a335e…`), the live
  station `C:/code/git/bpr-windows` (HEAD `b9cf4edb`, status digest
  `97623b91…`) and the count of BetterPullReview Credential Manager entries
  (0) are identical before the first run and after the last. No sign-in,
  provider mutation, update installation, or security, Defender or display
  setting change; Defender recorded no detection during the runs.
- Processes: every application process exited on its own — each sender 0,
  the owner 0 through its windows' closes — and every course and helper
  process ended. The task roots were removed. This dispatch took no station
  lease of its own and holds no writer.

### Found outside this card

Both reproduce on this station at `c52f5907` product source and at the
candidate; neither is caused by the correction. New cards are requested.

1. **Closing a second window can crash the owner.** `0xC0000409` (Control
   Flow Guard fast fail, subcode `0xA`) in `github_application::implementation`'s
   destructor (`github_application.cpp:7179`) deleting `Realigner`, a
   `std::unique_ptr<application::agent_realignment_port>` whose object is
   already invalid, reached from the `OnClosed` erase in `open_window`
   (`github_application.cpp:13686`). Closing the menu-opened window crashed
   in 5 of 5 `-Mode close-probe` runs, active or not, 3 s or 30 s after it
   opened. In full courses it is intermittent: the first of three closes
   crashed in 1 of 5 (`observation-fix-1.json`).
2. **File > Quit through the Windows strip menu.** The strip's Application
   menu (`c6cac3f0`, [[BPR-0653]]) is a stack `QMenu` parented to the window
   and run with `exec()`. Invoked through UI Automation, Quit completes while
   that menu is still executing; the window is destroyed under it and deletes
   the stack object, and the Debug CRT asserts `_CrtIsValidHeapPointer(block)`
   (`debug_heap.cpp:904`) in 4 of 4 probes. Choosing the item with the
   mouse or keyboard hides the menu before the action runs, so the exposure
   is the accessibility path and any other path that destroys the window
   while its menu is open.

### Shared desktop

The Linux station runs WSLg courses on this same desktop. Two early attempts
here took the foreground for under a second at about 16:32Z and 16:38Z,
while a [[BPR-0975]] course was starting, and then stopped. After that each
run waited for a quiet desktop: no Linux course process and no WSLg
BetterPullReview window in front for at least a minute.
