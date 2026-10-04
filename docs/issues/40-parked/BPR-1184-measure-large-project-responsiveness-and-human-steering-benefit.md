---
schema: "implement-rfc.card.v1"
id: "BPR-1184"
title: "Measure large-project responsiveness and human steering benefit"
status: "Parked"
rfcs: ["RFC-BPR-0054", "RFC-BPR-0061"]
depends_on: ["BPR-1166", "BPR-1171"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "qualification", "acceptance": ["RC54-08", "RC61-10"], "pack_alias": "PACK-CYCLE-42", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"RC54-08","outcome":"pending","revision":"397908dec918d2a78cb112ef5c1fc4b5a3060262","evidence":{"kind":"artifact","path":"docs/verification/BPR-1184-large-project-2026-10-04.md","detail":"macOS arm64 Release, application_review_cycle_large_project 1/1 in 181.83 s at 397908dec (0 tracked changes): drawer and compare page every listing (25 windows of 4,096 at 100,001 files, last file opened; changed-only pages exactly the 1,000 edited files plus the over-budget file as unknown); Cancel capture stops within 7.47 ms and records nothing; hashing on the capture lane (GUI longest turn during a capture 0.43 ms, during any listing 38.5 ms); core and host state hold one window. Report docs/verification/BPR-1184-large-project/mac-release-report.json sha256 4a649b44...947291c. Pending: Windows and Linux reports (station requests on the card)."}},{"criterion":"RC61-10","outcome":"pending","revision":"397908dec918d2a78cb112ef5c1fc4b5a3060262","evidence":{"kind":"artifact","path":"docs/verification/BPR-1184-large-project-2026-10-04.md","detail":"Latency and memory, macOS Release at 397908dec: typing while capturing p95 0.51/0.53 ms and navigation p95 4.93/6.32 ms (20,000/100,001 files), enforced against 100 ms; cold open 2.1/10.1 s, unchanged capture 1.5/6.0 s, warm open 97.7/162.8 ms, desktop footprint at most 96.6/165.8 MiB, terminal peak 36.6/92.5 MiB; 514-item round about 3 s per note change, 4.3 s to freeze, sheet opens with a 234.8 ms GUI turn (negative finding, not changed); limit probe names 250,000 entries. Pending: the human steering comparison (attended item BPR-1184-steering-benefit-v001) and the Windows and Linux reports."}},{"criterion":"BPR-1184:fixes","outcome":"pass","revision":"397908dec918d2a78cb112ef5c1fc4b5a3060262","evidence":{"kind":"command","path":"docs/verification/BPR-1184-large-project-2026-10-04.md","detail":"Capture lane (navigation during a 100,000-file capture 9,836.8 ms before, p95 6.32 ms after), progress and Cancel capture, windowed drawer reaching every file, over-budget file named, over-limit words name 250,000, tab saves deferred during a capture. review_cycle_capture and review_cycle_workbench contracts fail with the lane removed and with the deferred save removed; integration pass 64/64 plus the large-project row, round-trip pin, coding-format-check, DOCUMENTATION_OK."}}]
---

Pack alias: **PACK-CYCLE-42** (live `BPR-1184`, allocated at import).

# BPR-1184: Measure large-project responsiveness and human steering benefit

## Outcome and bounded work

Measure named-corpus cold/warm browse/capture/freeze/export/compare latency and memory. Compare one eight-item/two-suggestion round against manual context assembly in a chosen harness, recording fixed task/source/settings and preparation/steering/rework outcomes.

## Acceptance and direct evidence

Hardware/build/data-size p50/p95 report, bounded input responsiveness, no silent omission at limits, human turn/reconstruction counts and honest mixed/negative findings. No blanket token savings or benchmark superiority claim.

Criteria: RC54-08, RC61-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Measured and fixed — macOS, 2026-10-04

Promoted at `0bdec4309` under the 2026-10-03 unattended mandate (BPR-1166 and
BPR-1171 Done) and claimed at `c71ce1258`. Product candidate `397908dec`.
The [evidence](../../verification/BPR-1184-large-project-2026-10-04.md) has
the method, every figure and the controls; the
[RFC ledger](../../verification/RFC-BPR-0053-0061.md#bpr-1184-large-project-responsiveness-and-the-steering-question-2026-10-04)
the criterion summary.

**The course.** `application_review_cycle_large_project`
(`tests/end_to_end/review_cycle_large_project_contract.py`) drives the
production profile desktop through the [[BPR-1166]] reviewer and the shipped
`bpr cycle` on `small-files-1m-lines-v1` (20,000 files, 1,000,000 lines) and
`tree-100k-files-v1` (100,001 files, 1,000,000 lines, one 129 MiB file), a
514-item stress round and a 250,001-entry refusal. It writes a deterministic
JSON report, enforces the 100 ms typing and navigation targets in optimized
macOS builds, and in Debug runs a 2,000-file correctness pass. Timeout 720 s
on macOS (Release ran it in 225.31 s and 181.83 s), a provisional 2,400 s
elsewhere.

**Defects found by measuring, fixed at their source** (`397908dec`):

1. A capture held the cycle store's one lane and the shared workbench effect
   thread: Next file waited 9,836.8 ms during a 100,000-file capture. The
   desktop now gives the runtime a capture lane; reads and the tab record run
   beside it, and the first call that could move the cycle on waits for it.
   Now: navigation while capturing p95 6.32 ms, typing p95 0.53 ms.
2. No progress or cancel for a 6–10 s capture. The Updates line counts files
   read, checked and recorded; Capture revision becomes Cancel capture, which
   stops within 7.47 ms and records nothing.
3. The drawer stopped at 4,096 files and nothing reached the rest. It lists a
   window at a time with "files 1–4,096 listed of 100,001 captured entries",
   Previous/Next files rows, and Next/Previous file across a window's edge;
   all 25 windows and the last file were reached.
4. A file over the 128 MiB budget read as "not available" with no reason; it
   now says why and is marked "(not captured)".
5. The over-limit refusal did not name the limit; it names 250,000 entries.
6. Tab saves during a capture waited on its write lock in front of the next
   read (273.7 ms once); they are kept until the capture ends.

Contracts: `review_cycle_capture` (`captures_beside_reads`) and
`review_cycle_workbench` (`lists_and_captures_a_large_project`), each failing
with its fix removed. The desktop course's own deterministic lane is passed
in (`review_cycle_desktop`'s optional capture executor).

**Mixed and negative findings, recorded and not changed.** A 514-item round
takes about 3 s per note change and 4.3 s to freeze (each save reloads every
note through two store calls, each on a new SQLite connection), and opening
its Share notes sheet holds the GUI 234.8 ms, over the 100 ms target; at the
admitted 4,096 items both would grow about eightfold. This needs a follow-up
card (a batched note read and a lazily built sheet); allocation is the main
integrator's, so it is reported, not filed. Capture cost is file-system bound
(6.0 s unchanged at 100,001 files). A terminal `compare` pays about 100 ms of
manifest decode per process at that size.

**Verification.** Integration pass on the committed content, macOS arm64
Release: 64 of 64 cycle, CLI, handoff, publish, parity, two-round, desktop,
network-isolation and architecture rows; the large-project row 1/1 in
181.83 s at `397908dec` with 0 tracked changes; round-trip composition pin
green; `coding-format-check` clean on 1,224 files; DOCUMENTATION_OK.

### Station requests (owed, not gating the Mac result)

Run at `397908dec` or any later `main` holding it; return the Release report
byte for byte as
`docs/verification/BPR-1184-large-project/<platform>-release-report.json`
with host, load and both ctest times, as a note here. Nothing is enforced off
macOS yet; the next round sets each platform's targets and timeout from its
report. If the contained row stops before measuring, record the first failing
line, pin it in `scripts/verify_test_network_isolation.py` as BPR-1166's row
is, rerun uncontained and record both. On Windows expect that: the course's
first `bpr cycle open` should meet the container as the parity course's
`bpr cycle list` does (exit 7, "The application root is unavailable or is
not owner-private.", measured at `8fab05419`), after the desktop phases.

Linux (`bpr-main-linux`):

```sh
cd /home/mmarcin/BetterPullReview && git pull --rebase --autostash && git checkout --detach 397908dec && cmake --preset app-release -B /home/mmarcin/BetterPullReview/build/bpr1184-release -DBUILD_TESTING=ON -DBPR_BUILD_TOUR_CLI=ON -DBPR_DEPENDENCY_OFFLINE=ON && cmake --build /home/mmarcin/BetterPullReview/build/bpr1184-release --target test_review_cycle_two_round_public_path app_bpr tool_better_pull_review_network_sandbox && ctest --test-dir /home/mmarcin/BetterPullReview/build/bpr1184-release -R "^application_review_cycle_large_project$" --output-on-failure; git checkout main
cmake --preset dev -B /home/mmarcin/BetterPullReview/build/dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON && cmake --build /home/mmarcin/BetterPullReview/build/dev-tour-cli && ctest --test-dir /home/mmarcin/BetterPullReview/build/dev-tour-cli -R "^application_review_cycle_large_project$|^review_cycle_capture$|^review_cycle_workbench$|^application_review_cycle_desktop_public_path$|^application_review_cycle_two_round_public_path$|^application_review_cycle_parity_public_path$|^test_network_isolation_manifest$" --output-on-failure
```

Windows (`bpr-main-win`):

```powershell
cd C:\code\git\BetterPullReview; git pull --rebase --autostash; git checkout --detach 397908dec; cmake --preset windows-app-release -B C:\code\git\BetterPullReview\build\bpr1184-release -DBUILD_TESTING=ON -DBPR_BUILD_TOUR_CLI=ON -DBPR_DISTRIBUTION_BUILD=OFF -DBPR_DEPENDENCY_OFFLINE=ON; cmake --build C:\code\git\BetterPullReview\build\bpr1184-release --target test_review_cycle_two_round_public_path app_bpr tool_better_pull_review_network_sandbox; ctest --test-dir C:\code\git\BetterPullReview\build\bpr1184-release -R "^application_review_cycle_large_project$" --output-on-failure; git checkout main
cmake --preset windows-dev -B C:\code\git\BetterPullReview\build\windows-dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON; cmake --build C:\code\git\BetterPullReview\build\windows-dev-tour-cli --config Debug; ctest --test-dir C:\code\git\BetterPullReview\build\windows-dev-tour-cli -C Debug -R "^application_review_cycle_large_project$|^review_cycle_capture$|^review_cycle_workbench$|^application_review_cycle_desktop_public_path$|^application_review_cycle_two_round_public_path$|^application_review_cycle_parity_public_path$|^test_network_isolation_manifest$" --output-on-failure
```

Expected: `REVIEW_CYCLE_LARGE_PROJECT_OK`, 1/1, report with `run.enforced`
false, `failures` empty and `source.revision` `397908dec…`
(`build/bpr1184-release/tests/end_to_end/bpr1184-large-project-report.json`);
every Debug-tree row passes.

### Human steering benefit: attended item

RC61-10's comparison of the eight-item, two-suggestion round with the same
review assembled by hand needs a person and a harness. Asked as one Accept or
Reject question in a new attended session (not f18e0aba or cbd48b91):

- Item `BPR-1184-steering-benefit-v001` (criterion RC61-10, scenario S34),
  fingerprint `d6f24a56…3bd482b`, in session
  `db739d0eeb54eb201a5973d7c15629af` (event cursor 4). Its state lives
  outside every checkout, in
  `/Users/jaeger/bpr-cycles-steering-review.EonR2S/attended-review`, with
  `--repo-root /Users/jaeger/Documents/GitHub/BetterPullReview`.
- Frozen root `/Users/jaeger/bpr-cycles-steering-review.EonR2S`: `git archive`
  of `397908dec` in `source/`, built in `build/` with `app-release` (tests
  off); `BetterPullReview` SHA-256
  `0b7ba3e134dc18706511925ca238f77eb580d9f086ee02d10d85fca10939b4ed`.
  `python3 /Users/jaeger/bpr-cycles-steering-review.EonR2S/launch.py` sets
  `HOME` and `CFFIXED_USER_HOME` to the root's `home/`, checks the app and both
  projects first, and reports an exit within 10 s. An 8 s offscreen trial
  stayed up and wrote its profile under `home/` only.
- The task: `ITEMS.md` (BPR-1166's eight items, two suggestions, and the goal)
  on two identical checkouts, `project-bpr` and `project-manual`. Accept if the
  BPR round needed no more steering turns, reconstruction steps and
  corrections than the message typed by hand; Reject if more, or if it could
  not be run. The counts go in the note; a verdict without them is recorded as
  a verdict only, not as a measurement.

### Checkpoint

- Candidate: product `397908dec`; evidence and this card in the commit that
  parks it.
- Changed paths: `src/application/{include/bpr/application,src}/review_cycle_{runtime,capture,workbench,projection,comparison,words}.*`,
  `src/app/src/review_cycle_desktop.{cpp,hpp}`,
  `src/adapters/qt/{include/bpr/qt,src}/review_cycle_view.*`,
  `docs/architecture/domain-and-state-model.md`,
  `tests/end_to_end/{CMakeLists.txt,review_cycle_large_project_contract.py,review_cycle_two_round_public_path_tests.cpp,review_cycle_desktop_public_path.cpp}`,
  `tests/workspace_contract/review_cycle_{capture,workbench}_tests.cpp`,
  `docs/verification/BPR-1184-large-project-2026-10-04.md`,
  `docs/verification/BPR-1184-large-project/mac-release-report.json`,
  `docs/verification/RFC-BPR-0053-0061.md`.
- Pending: RC61-10's steering item (above); Windows and Linux reports
  (station requests above), then per-platform targets and timeouts from them;
  a follow-up card for the large-round costs.
- Next action: on hand-back, read the verdict and the counts in its note and
  record them here as RC61-10's steering evidence; on station returns, commit
  their reports and set their targets. Finish when both are in.

**Note: Linux station return (`bpr-main-linux`, 2026-10-04).** Host: Intel
i9-10900KF, 20 logical CPUs, 62.7 GiB, WSL2 kernel 6.18.33.2, Qt 6.11.2,
offscreen. The station was otherwise idle (load 0.11 before the run). Both
trees were run as the station request writes them:

- Release, `build/bpr1184-release` at `397908dec`:
  `application_review_cycle_large_project` passed 1/1 in 133.38 s. The report
  is [`linux-release-report.json`](../../verification/BPR-1184-large-project/linux-release-report.json),
  SHA-256 `8d084f35af9f0c70899c40a3dde308a7ead7a521bffdbae28842b5a7e9cae656`,
  byte for byte. It has `run.enforced` false, `failures` empty and
  `source.revision` `397908dec…` with 0 tracked changes.
- Debug, `build/dev-tour-cli` at `f8ce4cd3c`: 7/7 passed (123.53 s).

Typing p95 while capturing was 0.58 and 0.71 ms, and navigation p95 5.68 and
6.04 ms (20,000 and 100,001 files). Cold opens took 1,737.4 and 7,639.7 ms,
warm opens 92.9 and 207.5 ms, and unchanged captures 1,121.1 and 3,832.2 ms.
Share notes opening with 514 rows held the GUI thread for one turn of
**270.9 ms**, matching the macOS 234.8 ms finding. The 250,001-entry limit
was refused with its words in 4,779.1 ms (desktop) and 4,765.4 ms
(terminal). The course removed its corpora.

**Note: Windows station return (`bpr-main-win`, 2026-10-04). No Release report
yet.** Host: Intel i9-10900KF, 20 logical CPUs, 127.8 GB, Windows 11 Home
10.0.26200, MSVC 14.51, Qt 6.11.2, offscreen. There was 1.3 TB free, and every
corpus root was removed afterwards. The host also runs the Aito lane: it was
building when the request arrived and was idle (0 build processes, CPU about
3%) through every measured run below. The Release tree is
`build/bpr1184-release` (`windows-app-release`, Ninja, Release,
`BUILD_TESTING=ON`, `BPR_BUILD_TOUR_CLI=ON`, `BPR_DISTRIBUTION_BUILD=OFF`),
run on `main` holding `397908dec`.

- **Release build.** It first failed. `review_cycle_two_round_public_path_tests.cpp`
  includes `<windows.h>` without `NOMINMAX`, so `std::max` met the SDK's macro
  (C2589). The Debug tree compiled it only because an earlier header had
  defined `NOMINMAX`. Fixed at `e9c7eaa98`.
- **Contained, as registered** (11.96 s). After the first corpus, the first
  desktop exits 1 with `FAILED: the desktop starts with a window`, before any
  `bpr cycle open`. Pinned `WINDOWS_UNCONTAINED` (`dos-volume-name`) at
  `e9c7eaa98`.
- **Uncontained run 1, at `e9c7eaa98`: failed after 1,942.69 s.** Replies 1–90
  were sound: the 20,000-file course through capture, typing, navigation,
  cancel and the first All cycle changes view. Request 91, the next view, was
  lost. The desktop answered `REPLY 0 {"error":"unknown verb "…}` because its
  bare `std::ifstream` read the just-renamed `.command` as empty while a scan
  held it, and it deleted the file anyway. The course then waited out its
  1,800 s. Fixed at `dbd11196f`: a command that does not parse stays and is
  read again, failing loudly after 2 s, in the two-round and both handoff
  desktops. The large-project ask now waits 600 s.
- **Uncontained run 2, at `dbd11196f`: failed after 1,041.26 s** with
  `missing REPLY 1`, the cold Open Project on the 20,000-file corpus. The
  desktop printed `DESKTOP_READY` and nothing more within 600 s. 600 s is
  exactly `open_project`'s own scaled wait (30 s × wait scale 20), so it was
  most likely still waiting for "the folder opens as a cycle with its first
  capture".
- **Debug, `build/windows-dev-tour-cli` at `e9c7eaa98`: 6/7 passed** (capture,
  workbench, two-round, parity, desktop, manifest).
  `application_review_cycle_large_project`, in its 2,000-file quick pass on the
  tree corpus, got no reply to request 69 (navigate during capture), not even
  an error. It then did not release on quit. Thirty minutes later the stray
  desktop was still polling at about 16% of one core; it was stopped and its
  root removed. No debugger is installed, so there is no stack.

**Why the stalls said nothing.** The desktop's waits are scaled by 20 (up to
600,000 ms × 20 for "Next file reads the file after …"), so they outlasted the
course's ask, and the course gave up before the desktop's own refusal could
reach it. Every course command now has a budget, the ask limit minus a reply
margin (180 − 30 s, and 600 − 60 s for this course). The desktop caps every
scaled wait at what is left of it, so a stalled gesture replies with its
refusal and the window's state.

With that change the Debug large-project row passed, once, in 105.76 s; the
two-round and parity courses pass with it too. Two silent stalls, both waiting
on a capture (Debug navigate during capture, Release cold open), sit beside a
first Release run that captured and navigated for 90 replies. They are
intermittent. A Windows capture-lane stall is the leading suspicion, but it is
not shown: the next stall will name its own FAILED line and state.

**Note: Windows Release run 3, at `61c96b9a3` (bpr-main-win, 2026-10-04).**
The tree was clean, the Aito lane idle and every root removed afterwards. The
run failed after 991.03 s and wrote no report. With the per-command budget,
the stalled gesture now replied:

```
open-project {'path': '…\large-project-7c12d1d938712105\tree-100k-files-v1'}: the folder opens as a cycle with its first capture {"cycle": "RC-1", "editor": {"focused": false, "text": "", "visible": false}, "home": false, "label": "Not captured yet", "listed": [], "message": "", "newer": false, "notes": [], "notice": "Choose a file to read its captured source.", "responses": 0, "revisions": [], "rounds": [], "source": "", "title": "tree-100k-files-v1", "updates": "No revision captured · 0 files · Capturing: 86,016 files read"}
```

The 20,000-file corpus passed whole this time: both desktops, the stress round
and the terminal steps. Its result line (not a committed report) gives these
figures, as n / p50 / p95 ms:

| Phase | Desktop | Terminal (`bpr cycle`) |
|---|---:|---:|
| Open, cold | 1 / **110,983.6** | 1 / 7,865.1 |
| Capture, unchanged | 1 / 6,235.1 | 3 / 5,849.2 / 5,875.7 |
| Open, warm | 1 / 225.0 | - |
| Next file | 30 / 24.8 / 27.2 | - |
| Typing while capturing | 147 / 1.45 / 2.37 | - |
| Navigation while capturing | 30 / 15.5 / 25.8 | - |
| Compare one file | 20 / 49.4 / 58.9 | 20 / 173.5 / 177.5 |

The desktop's cold Open Project took 14 times as long as `bpr cycle open` on
the same folder (macOS desktop: 2.1 s). Its later captures do not: unchanged
captures run at 6.2 s desktop against 5.8 s terminal. At that rate the
100,001-file cold open outruns the gesture's 540 s, which fits "Capturing:
86,016 files read" being slow progress rather than a deadlock. It fits the
earlier silent stalls too: run 2's 20,000-file cold open got no reply in
600 s. Not yet shown: whether the cold desktop open is slow throughout or
stops for long stretches. The capture on Open Project, in the desktop on
Windows, is the place to look. Separately, every terminal peak-memory figure
reads 0.1 MiB on Windows, which looks like `child_peak_bytes` not measuring
there.

**Note: Windows station return (`bpr-main-win`, 2026-10-04).** Host: Intel
i9-10900KF, 20 logical CPUs, 127.8 GB, Windows 11 Home 10.0.26200, MSVC 14.51,
Qt 6.11.2, offscreen. The Aito lane was idle throughout (0 build processes,
CPU about 3%). The run used the Release tree `build/bpr1184-release` at
`c906b0e7e`, which holds `397908dec` and the Windows course fixes above
([[BPR-1275]], [[BPR-1276]]). `application_review_cycle_large_project` passed
1/1 in 1,629.14 s, uncontained as pinned. The report is
[`windows-release-report.json`](../../verification/BPR-1184-large-project/windows-release-report.json),
SHA-256 `e865a9a94518295a9c3243ad38398329542e6964f4df135ef31adb0648cd744e`,
byte for byte. It has `run.enforced` false, `failures` empty and
`source.revision` `c906b0e7e…` with 0 tracked changes. The earlier Debug tree
(`windows-dev-tour-cli`) passed the 2,000-file pass at `c906b0e7e`.

Since [[BPR-1275]], each corpus is read once, untimed, before it is measured.
That first read cost 97,974.5 ms for 20,000 new files and 559,402.4 ms for
100,001, about 5 ms a file under Defender. It is kept as
`corpus.first_read_ms`. With it out of the timed phases, figures in ms:

| Phase | 20,000 files | 100,001 files |
|---|---:|---:|
| Desktop open, cold, against `bpr cycle open` | 7,906.0 / 7,656.7 (ratio 1.033) | 45,750.0 / 44,563.2 (1.027) |
| Desktop open, warm | 226.9 | 352.4 |
| Capture unchanged, desktop / terminal p50 | 5,772.4 / 5,621.9 | 27,246.4 / 26,875.3 |
| Next file p50 / p95 | 24.9 / 26.5 | 25.3 / 39.0 |
| Typing while capturing p95 | 2.26 | 2.56 |
| Navigation while capturing p95 | 22.4 | 22.0 |
| `bpr cycle open` peak memory | 22.6 MiB | 88.8 MiB |

The 250,001-entry folder was refused with its words in 49,601.7 ms (desktop)
and 47,866.3 ms (terminal).

Recorded here, not changed:

- One of the twenty 20,000-file desktop compare samples took 100,077.2 ms
  (p95 51.8 ms). The cause has not been looked at.
- The 514-item stress round is slower than macOS. Share notes opened in
  12,736.6 ms, holding the GUI thread for 379.5 ms in one turn. Each desktop
  suggestion took about 16.1 s, freeze and Copy 27,144.7 ms, and each
  `bpr cycle note` 157.4 ms at p50. This is [[BPR-1274]]'s cost, larger on
  this host.
- The 100,001-file cold open takes about 45 s on both paths, against 10.1 s on
  macOS.
