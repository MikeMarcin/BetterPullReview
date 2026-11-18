---
schema: "implement-rfc.card.v1"
id: "BPR-2215"
title: "Publish 0.5.0 prerelease and full release"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"external_state":"/Users/jaeger/.codex/release-runs/v0.5.0-20261116","baseline":"ce94dbe8d","authority":"Owner requested a minimally smoke-tested Mac-only prerelease, then full all-platform qualification and the 0.5.0 release, on 2026-11-16"}
acceptance: []
---

# Publish 0.5.0 prerelease and full release

- Prepare one committed 0.5.0 source, advance the Windows MSI floor to the published 0.4.2 build 60 installer, and freeze a numbered cohort.
- Publish a signed, notarized macOS-only numbered prerelease after a minimal smoke test, with the owner's explicit acceptance that full qualification follows.
- Classify the 0.4.2-to-0.5.0 delta as broad and run comprehensive native Release and Debug qualification on macOS, Windows and Ubuntu 26.04 WSL2, retaining actual failures and the standing Debug timing omissions.
- Correct blockers through successor cohorts, then package, audit, sign and publish the full release, verifying downloaded bytes and native trust on each producer.

No prior release failure, security disposition or native acceptance transfers without its exact input and authority checks. Existing releases and assets remain immutable.

## Risk-preflight repairs: `git_cache_contract` and `test_network_isolation_manifest` (2026-11-17)

The 0.5.0 native risk preflight failed two of its nine rows in Release and
Debug on Windows (source `66448e35a`) and on Ubuntu 26.04. Fixed on
`claude/release-0-5-0-windows-preflight`:

- **`git_cache_contract`, Windows.** `inject-cache-interrupted-pack-link`
  ended `repair_required` (code 13): BPR-2195's `settle_interrupted_index`
  removed a temporary name that is a second link to the expected pack only on
  POSIX. Windows now recognises it by the open handle's volume and file index
  (exactly two names) and removes that name through the same handle. The first
  native run still failed: Git for Windows leaves packs read-only (observed on
  the station), and a plain delete disposition refuses that, so the removal
  uses `FileDispositionInfoEx` with POSIX semantics and the read-only
  attribute ignored. The assertion is unchanged.
- **`git_cache_contract`, Ubuntu.** It failed earlier, at `create lineage
  baseline spool`: `/tmp` is tmpfs on Ubuntu 26.04, tmpfs does not answer
  `FS_IOC_GETVERSION` (probed: `ENOTTY`; ext4 answers), and the cache refuses
  durable artifacts there by design (BPR-2095, whose `/dev/shm` control the
  course keeps). A different cause from Windows. The course now makes its
  ordinary cache roots beside the test executable when the system temporary
  directory lacks generations, and prints `SCRATCH_WITHOUT_GENERATION` when it
  does. Any other course that builds a Git cache under a tmpfs temporary
  directory would refuse the same way on that host.
- **`test_network_isolation_manifest`.** Five rows registered
  `WINDOWS_UNCONTAINED` without a pin now carry one
  (`application_review_report_public_path` is registered only off Windows and
  keeps the unmeasured pin). Fifty Windows rows were measured contained on the
  station and pinned with their first contained observation; none passes
  contained. `local_changes` compiled its course out on Windows and printed
  nothing; it now prints its skip there and is registered contained. See
  [the measurement record](../../verification/release-v050-windows-containment.md).
  Twelve of those direct baselines fail natively on Windows, independent of
  containment, and remain open for comprehensive qualification.

Main was merged into the lane (`877f8091b`) and BPR-2245's new
`application_ask_setup_review_public_path` was measured and pinned the same
way. Native results, all in the station's Interactive Limited context on
Windows:

| Candidate, platform, config | Rows run | Result |
| --- | --- | --- |
| `15745dc64` Windows Release | four affected rows | 4 passed (`local_changes` contained) |
| `15745dc64` Windows Release risk preflight | 9 | 8 passed; `application_diff_host` failed its BPR-1485 10 ms stop budget at 12.56 ms. A fresh rerun passed 9, and the row alone passed 5 of 5 |
| `15745dc64` Windows Debug | four affected rows, then 9 | 4 passed; 9 passed |
| `15745dc64` Ubuntu 26.04 Release and Debug | 9 each | 9 passed each |
| `018a9c954` (final) Windows Release | four affected rows, then 9 | 4 passed; 9 passed |
| `018a9c954` (final) Windows Debug | four affected rows, then 9 | 4 passed; 9 passed |
| `018a9c954` (final) Ubuntu 26.04 Release and Debug | 9 each | 9 passed each; the course reported `SCRATCH_WITHOUT_GENERATION: /tmp` |
| `877f8091b` macOS dev | the 9 rows and the affected rows | 13 passed |

The intermittent `application_diff_host` Release stop-budget failure is kept
as an actual failure; this change does not touch it and does not disposition
it. The Windows producer checkout (`D:\BetterPullReview-release-v050-r1\source`)
and the Ubuntu guest checkout were moved to `018a9c954` and rebuilt there.
Evidence, receipts and dispatch scripts are under the external state's
`windows-preflight-fix-r1/`.

## Windows course failures, the diff-host stop budget and Linux tmpfs (2026-11-17)

On the lane with main merged (`f8df1e84d`):

- **The twelve native Windows failures** now pass, in the station's
  Interactive Limited context, in Release and in Debug, at `4adadc53c`. Their
  causes and fixes are listed in
  [the measurement record](../../verification/release-v050-windows-containment.md).
  Two were product defects, both confirmed on macOS with the author's Git made
  unavailable as it is on Windows: in Author, This update showed an empty
  viewer while it had no file of its own; and `local_draft_desktop` left the
  search panel's destroyed handler pointing at a deleted controller, an access
  violation when the window outlived it. The author-state course ends at Check
  Again on Windows, where the author's Git is refused as unavailable.
  `application_home_next_move_public_path` failed on every platform: it never
  left the account setup's Ready step.
- **`application_diff_host`'s 12.56 ms.** The BPR-1366 stop returned in
  4.03–6.41 ms (median 4.78) over 30 runs alone and 4.51–6.75 ms (median 5.03)
  over 12 runs of the nine preflight rows at `-j4`, in Release on the station;
  the BPR-1485 matcher stop stayed under 0.3 ms. No run reached 10 ms, so the
  12.56 ms is kept as a single load outlier; the latencies are in the external
  state's `windows-preflight-fix-r1/diff-host-latencies-t1.txt`.
- **Ubuntu 26.04 tmpfs.** The full Linux Release suite failed the epoch and
  trimmed-pack Git courses and every large-review course at the Git load:
  `/tmp` is tmpfs and the cache refuses it by design (BPR-2095). The shared
  helpers root Git-cache courses beside their executable when the temporary
  directory lacks inode generations, and configure gives every isolated
  Linux row `TMPDIR=/var/tmp` where `/tmp` lacks them and `/var/tmp` has them.
  The Git courses then pass and the large-review courses load in about a
  second. The rows that still fail on that host fail the same way with the
  override off, so tmpfs is not their cause.

| Candidate, platform, config | Rows | Result |
| --- | --- | --- |
| `4adadc53c` Windows Release | the twelve, `local_changes`, the manifest | 14 passed |
| `4adadc53c` Windows Debug | the same | 14 passed |
| `d8768775b` Windows Release risk preflight | 9 | 9 passed |
| `d8768775b` Windows Debug risk preflight | 9 | 9 passed |
| `2d8b2a9d8` Ubuntu Release, full suite | 1062 | 1021 passed, 41 failed; none on tmpfs |
| `d8768775b` Ubuntu Release and Debug risk preflight | 9 each | 9 passed each |
| `d8768775b` Ubuntu Release, full suite | 1062 | 1020 passed, 42 failed; none on tmpfs |

In the `d8768775b` suite `application_epoch_host` hung: one thread waiting
on a futex for 46 minutes with no output (it passed in 1.36 s at
`2d8b2a9d8`, and 150 runs on macOS were clean). It was aborted by PID so the
suite could finish, and stays an open failure.
`application_review_tour_publication_reconciliation` failed there and not
before.

Also fixed on the way, each failing on macOS as well as Ubuntu:
`coding_format_check` (an unformatted `github_window_read_host.cpp` on main),
`contained_temporary_usage_contract` (three opt-in scripts calling `tempfile`)
and `coding_standard_format_gate_contract` (CMake line wrapping, exposed by
the longer `/var/tmp` paths). Still failing at main on macOS and Ubuntu, not
addressed here: `application_composition_source_contract`,
`freeze_attended_build_contract`, `github_contract_fixture_security` and
`qualification_case_inventory_contract`.

## Main's new rows, main's red contracts and the Ubuntu full suite (2026-11-18)

Third round on `claude/release-0-5-0-windows-preflight`, from main at
`36a45ba49`.

- **Main's four Windows rows.** Measured contained at `b9a0915ec` in the
  Interactive Limited context and pinned (see
  `docs/verification/release-v050-windows-containment.md`):
  `returning_review_mark` `private-data-root-refused`, `review_seen_stays` and
  `review_seen_fails` `private-desktop-root-refused`. `commit_asks` is
  registered only off Windows and stays unmeasured, as `review_report` does.
- **Main's four red contracts.** `qualification_case_inventory_contract` and
  `application_composition_source_contract` were resealed;
  `freeze_attended_build_contract` failed on Linux only, where the launcher
  could not see the first start's helpers (it read macOS libproc only; it now
  reads `/proc`); `github_contract_fixture_security` passes at the lane.
  `composed_host_verifier_contract` also failed on Ubuntu: its stripped
  `python3` is rightly refused, and the course now expects that.
- **`application_epoch_host` hang.** gdb on the Ubuntu guest caught three
  hangs in about 30 parallel runs, each in
  `exact_historical_corpus_uses_its_pair_and_host_path`: the course counted
  three and then two effects, but the profile actor drains work queued while
  its effect runs inside that same effect, so a fast search worker needed
  fewer and the course waited forever. It now drives effects until the search
  ends; 400 parallel runs afterwards were clean.
- **A crash.** `application_tour_chat_queue_stop` and `_whole_tour` died with
  SIGSEGV on Ubuntu: Save on a held message re-presented the queue, which
  replaced the pending list's dispatch while its `std::function` ran, and the
  app read `this` from the freed closure. Each control now calls a copy.
- **Fixed failures that were on every platform:** the `discuss_top` pill
  (BPR-2171's centred column wrapped the band; the pill now gives up width
  and its caption elides), `thread_words` (newer agent-chat strings, each
  exempted where it appears), `source_pr_find` (Author's hidden tab),
  `review_cycle_publish` (an open account setup held Home over a local cycle),
  `home_waiting` (the fixture served the PR as a draft, which opens in
  Author), and the large-review `launch`, `responsiveness` and
  `provider_cancel` courses (they waited 90 s for the "Provisional metadata"
  sentence BPR-2216 removed).
- **Ubuntu-only fixes:** the GCC 15 `-Werror=dangling-pointer` build stop in
  `returning_review_tests.cpp`, the agent footer growing the status bar from
  19 to 21 px (which also cost the UIforETW capture its two pixels), the
  painted-wash measurement, and AC03 of `local_comments_composition` reading
  the cards before they were presented again.
- **Fonts are not the cause.** With `fonts-ubuntu` installed on the guest the
  Qt failures were unchanged; the package was removed again.

| Candidate, platform, config | Rows | Result |
| --- | --- | --- |
| `b9a0915ec` Windows Release, three new rows | 3 | direct pass, contained refused as pinned |
| `c2fda611c` Ubuntu, `application_epoch_host` 4 × 100 parallel | 400 | 400 passed (3 hangs at `36a45ba49`) |
| `7c13c2dd8` Ubuntu Release, full suite | 1064 | 1048 passed, 16 failed |
| `d3ff568b7` Windows Release, full suite (Interactive) | 1048 | 1015 passed, 33 failed |

Still failing on Ubuntu at `7c13c2dd8`:

- Eleven large-review courses miss Release timing budgets on the Lima guest
  (6 vCPUs on an Intel Mac): for example `accepted_metadata_render` 379 ms
  against 250 and an input gap of 107 ms against 100. The budgets are
  calibrated on the WSL2 i9-10900KF qualification host; they need a run
  there, on an idle PC, before they count as passed or missed.
- `qt_button_faces`: under `QT_STYLE_OVERRIDE=Windows` the Source search
  Details toggle's words are drawn in blue (`#2f72bc`, 4.19 to 4.6:1;
  6.3:1 in high-contrast light) and the About link reads 4.36:1 in dark.
  Fusion on macOS draws the toggle grey (9 to 11:1).
- `qt_conversation_cards_view`: at 260 px the draft's main action gets 116 px
  of its 118 px hint.
- `application_profile_window_layout_public_path` (all platforms): since
  BPR-2180 v004 a late layout restore opens no drawer over Review, and the
  course enters Review before the restore, so Repositories stays closed.
  The course's restore expectations need rewriting against that rule.
- `application_label_publication_recovery` and
  `application_agent_queue_finish_pauses_public_path` passed alone earlier
  and failed once in the suite.

The Windows suite ran at `d3ff568b7`, before the large-review and footer fixes.
Its 33 failures: the three large-review timeouts and `qt_agent_footer` (23
against 22 px) are fixed later on the lane; three contracts that configure
nested CMake trees stopped in `AitoCpm.cmake` or
`BprSyntaxHighlightingProvider.cmake`; four failed on access to the contained
temporary tree (`test_suite_runner_contract`, the RFC-0017 recorder,
`agent_scratch_folder`, `review_tour_writing_wrapper`); the rest are agent,
store-restart and review-flow courses not yet diagnosed. The external record
is `/Users/jaeger/.codex/release-runs/v0.5.0-20261116/windows-preflight-fix-r1/`.
