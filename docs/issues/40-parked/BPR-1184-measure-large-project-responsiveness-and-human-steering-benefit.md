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
