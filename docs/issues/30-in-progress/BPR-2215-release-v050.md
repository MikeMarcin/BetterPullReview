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

## Fourth round: Windows courses, Linux contrast and the two one-offs (2026-11-18)

Lane `claude/release-0-5-0-windows-preflight` from main at `d66d6a22e`;
stopped at the owner's word ("stop for now after fixes, we have one more
feature addendum to add into this build") with no cohort frozen.

**Host split (owner, 2026-11-18: "On the WSL2 i9").** The eleven Ubuntu
large-review Release timing courses are judged on the WSL2 i9-10900KF host
on the PC, in a checkout and build tree of their own (not `~/bpr-v050-r1`),
built Release at `-j8` at most, while the PC is otherwise idle. Ubuntu
functional qualification stays on the Intel Lima guest. The WSL2 timing run
has not been started: the scripts are staged at
`D:\BetterPullReview-release-v050-r1\preflight-fix-r1\wsl-timing.{sh,ps1}`
with the lane's bundle `full-f5343b1e1.bundle` beside them.

Fixed this round:

- **Linux contrast.** The Source search Details toggle draws three dots at the
  button's ink instead of the font's "…", which rendered as colour fringes
  (4.19–4.6:1). The readable link colour is chosen at 4.8:1, not exactly 4.5,
  and a card's signal words ("Draft") 15% above the text floor: under
  grayscale antialiasing thin words read about 10% below the colour chosen.
- **`qt_conversation_cards_view`'s 2 px.** In a narrow drawer the split draft
  row closes its two gaps (down to 2 px) before squeezing the main pill.
- **`profile_window_layout`** follows BPR-2180 v004: a review opens on
  Overview and enters Review on its preset, and a late restore opens no
  drawer, so the course opens Repositories by hand before the independence
  check and expects Review's preset after a restart. A window's drawer
  open/closed state therefore no longer survives a restart; everything else a
  window restores is still checked.
- **The two one-offs.** `agent_queue_finish_pauses` was a real race in the
  course (5 of 15 Ubuntu runs: it pressed Resume before the list showed it);
  it now waits for the shown button, and passed 12 of 12.
  `label_publication_recovery` failed 1 of 15 and then passed 12 of 12; its
  wait now says what the summary showed.
- **Windows:** nested configures find Git for Windows' Perl and the build's
  own CPM cache (`dependency_modernization`, `native_release`); three fixture
  generators write LF (`github_recorded_fixture`); the phase ledger's log
  directories inherit instead of taking a protected owner-only descriptor an
  AppContainer cannot enter (`test_suite_runner`); `fixture_agent_answers`
  finds `claude.exe`; the scratch folder course reads owner-only from the
  DACL and expects no grant path on Windows; the writing-wrapper course
  renames its stub claude instead of deleting a running program.

| Candidate, platform, config | Rows | Result |
| --- | --- | --- |
| `d66d6a22e` Windows Release, full suite (Interactive) | 1048 | 1021 passed, 27 failed |
| `2fbb54cc9` Windows Release, the 27 | 27 | 9 passed, 18 failed |
| `7c13c2dd8` Ubuntu, `agent_queue_finish_pauses` alone and under load | 15 | 10 passed, 5 failed |
| `ec87c8676` Ubuntu, `agent_queue_finish_pauses` / `label_publication_recovery` | 12 each | 12 passed each |
| `2fbb54cc9` Ubuntu, `^qt_conversation` and `qt_button_faces` | 6 | 6 passed |
| **`f5343b1e1` Ubuntu Release, full suite** | 1064 | **1052 passed, 12 failed** |
| **`f5343b1e1` Windows Release, full suite (Interactive)** | 1048 | **1027 passed, 21 failed** |

Still failing at `f5343b1e1`:

- Ubuntu: the ten large-review timing courses (held for the WSL2 i9 run);
  `local_comments_composition` AC03 again (the cards' local comments were
  empty after the last send, even after waiting); and
  `profile_window_review_restoration` ("reading epoch ready", new this run).
- Windows, agent workspace: `agent_invocation_host`,
  `agent_workspace_entry_limit`, `native_agent_sessions`,
  `agent_chat_custom_agent`, `agent_finding_codex_found` ("The agent's copy
  of this pull request could not be made").
- Windows, contained stores and temp: `qt_prompt_macro_chips` and
  `qt_agent_finding_cards_restart` (a reopened store reads nothing back),
  `qt_agent_report_view` (cannot write in its QTemporaryDir), the RFC-0017
  recorder (`git init` cannot create `.git` under the AppContainer's temp).
- Windows, review flows: `start_review`, `label_publication_recovery`
  ("Review recovery is unavailable"), `review_submission_composition` and
  its 125% scale, `discuss_top`, `home_approve_again`, `review_seen` and its
  `_stays`/`_fails`, `local_comments_composition`; several of these passed
  when run alone in the 27-row round. `review_tour_writing_wrapper` now
  stops at "writing child process and cleanup finish".
- Windows: `large_review_resources` (maintenance retains raw diff, index or
  source), and `application_composition_contract` timed out at 1800 s in the
  27-row round (its nested configure now passes Perl and builds).

Evidence: `windows-release-full-f5343b1e1-junit.xml` and
`ubuntu-release-full-f5343b1e1-junit.xml` in
`/Users/jaeger/.codex/release-runs/v0.5.0-20261116/windows-preflight-fix-r1/`.

## Fifth round: WSL2 i9 timing, prompt packs and the Windows tail (2026-11-18)

Lane recreated from main `9304efc4b` (BPR-2149 prompt packs), later merged
with main `975ebff1e` (build 66).

**WSL2 i9 timing (owner: "On the WSL2 i9").** Own tree
`~/bpr-v050-timing-r1` on the PC's WSL2 Ubuntu 26.04, Release at `-j8`, PC
otherwise idle, three runs of the eleven courses at `9304efc4b`:

| Course | Runs passed | Miss |
| --- | --- | --- |
| benchmark, unavailable, approximate, first tab, pathological, prepared open, history | 3 of 3 each | none |
| `application_large_review_responsiveness` | 0 of 3 | `composition_startup_render` / `main_to_home_render` 1083–1263 ms (5803 ms cold) against 1000 |
| `application_large_review_launch` | 0 of 3 | the same, 1127–1266 ms |
| `application_large_review_active_work` | 1 of 3 | `active_close_quiescence` 117.9 ms against 100 |
| `application_large_review_resources` | 0 of 3 | functional: maintenance kept the raw diff (fixed, below) |

The startup misses come from where the course's data root lives, not from a
change in the application. `/tmp` on the WSL2 host is tmpfs (fsync about
0.002 ms) and `/var/tmp` is the ext4 VHD (fsync about 2.5 ms). Since this
lane's BPR-2095 repair, isolated Linux rows set `TMPDIR=/var/tmp`, because
the Git cache refuses tmpfs. Run directly, the launch course's
`MainToHomeRenderMs` was 323 and 327 ms with the data root on `/tmp`, and 722
and 671 ms on `/var/tmp`. The budgets' basis (`e257b236a`) was measured with
the data root on tmpfs. **Owner decision needed:** re-base the startup and
close budgets on a disk-backed data root, or put only the Git cache on a
generation-answering filesystem (a product change) so the course's data root
can stay on tmpfs.

Fixed this round:

- `application_large_review_resources` (every platform): the composition kept
  the last accepted diff as a shared owner (BPR-2183), so prune and delete
  never released it; it is now a weak reference, read only while the surface
  shows that comparison. Production app digest resealed.
- `test_network_isolation_manifest`: BPR-2149's `sqlite_prompt_pack_storage`
  had no pin; it is pinned unmeasured until Windows measures it.
- `qt_agent_report_view` on Windows: QTemporaryDir's owner-only mode left the
  AppContainer unable to write; the course makes a folder that inherits.

| Candidate, platform | Rows | Result |
| --- | --- | --- |
| `9304efc4b` WSL2 i9 Release, the eleven ×3 | 33 | 22 passed, 11 failed |
| `9304efc4b` Ubuntu (Lima) Release, full suite | 1066 | 1051 passed, 15 failed |
| `bd4134653` Windows Release, the 23 failing | 23 | 6 passed, 17 failed |
| `1d6f85283` Windows Release, five of them | 5 | 3 passed, 2 failed |

At `9304efc4b` on Ubuntu, `test_network_isolation_manifest`, both composition
digests and `large_review_resources` are fixed on the lane. The ten other
large-review courses are judged on the WSL2 i9. `local_comments_composition`
AC03 fails every time on Ubuntu and is not yet diagnosed.

On Windows, the review-flow courses (`label_publication_recovery`,
`review_seen` and its variants, `review_submission_composition`,
`local_comments_composition`) all stop at "Review recovery is unavailable" in
the full suite, but `label_publication_recovery` and `review_seen_stays`
pass in a five-row round, so the cause depends on what ran before them.
These are still open on Windows:

- The contained store restarts (`qt_prompt_macro_chips`,
  `qt_agent_finding_cards_restart`: loaded, nothing read back).
- The contained native agent host (`agent_invocation_host`: workspace made,
  then `failed_to_start`, most likely `agent_checkout_spellings`'
  `canonical` refused inside the AppContainer).
- Native workspace creation in the uncontained courses.
- The RFC-0017 recorder's `git init` inside the AppContainer.
- `review_tour_writing_wrapper`.

## Sixth round: new startup and close basis, and the Windows store courses (2026-11-18)

- **Budget basis (owner option (a)).** The Linux startup and close budgets
  were measured again on the WSL2 i9 with the data root on disk: 1500 ms and
  160 ms. Evidence and margins are in
  `docs/verification/BPR-0967-station-budgets-2026-10-01.md`. The 100 ms
  input target is unchanged. `paged_review_and_git_load` exceeded it in 9 of
  30 on-disk runs (104–134 ms, mostly the GUI thread's own CPU time), which is
  a real miss still to locate.
- **Main merged** through `3314a7eb2` (BPR-2254, BPR-2255, build 68,
  BPR-2256), with the production app digest resealed for each app change.
- **Windows registrations.** `qt_prompt_macro_chips`,
  `qt_agent_finding_cards_restart` and the RFC-0017 recorder are now
  `WINDOWS_UNCONTAINED`, with their measured observations pinned (see the
  containment record). `sqlite_prompt_pack_storage` is still to be measured.
- **Windows review recovery.** The intermittent "Review recovery is
  unavailable" is the review-mutation-recovery store refusing its open with
  `invalid_permissions` (the app's own log). Which course fails varies from
  run to run (`scale_125` passes and `scale_200` fails; `review_seen` passes
  and `_stays` fails), which points to a race in the Windows private-store
  open, not to test order.

| Candidate, platform | Rows | Result |
| --- | --- | --- |
| `744b0e7fb` WSL2 i9, startup/close courses ×10 | 30 | basis recorded (see above) |
| `cb200dfa9` Ubuntu (Lima) Release, full suite | 1066 | 1054 passed, 12 failed (10 large-review timing judged on WSL2; `review_submission_scale_125`, `review_tour_publication_reconciliation`) |
| `cb200dfa9` Windows Release, full suite | 1050 | 1030 passed, 20 failed |
| `6afbf3268` Windows Release, 21 review and agent rows | 21 | 10 passed, 11 failed |

## Seventh round: stopped for more build changes (2026-11-19)

Stopped at the owner's word ("more changes are coming into the build"),
with no new suite started. Since the sixth round:

- **Review recovery race, confirmed.** A probe in
  `sqlite_review_mutation_recovery_storage` opens the one new journal from
  three threads at once (the mutation, off-hunk and submission stores a review
  opens together). On Windows at `166fe62f7`, two of the three openers failed
  with `integrity_failure` in the first of forty rounds: the schema was half
  made by another opener. The journal's opens and schema are now serialized
  in-process. The fix has not been run on Windows yet. The app's own log in
  the failing review courses showed `invalid_permissions` at open, which may
  be a second face of the same concurrent first open; that is unconfirmed.
- **Custom agent on Windows.** A grant names POSIX paths, so on Windows the
  turn runs without one; the course now expects that there.
- **Native agent workspace on Windows.** The course now relays the app's
  diagnostic log when the workspace check fails. The cause is still unknown.
- Not done: measuring `sqlite_prompt_pack_storage` on Windows, the WSL2 i9
  load-phase stall, and the full Release and Debug suites at the lane head.

## Eighth round: main 392dffc6f merged; recovery race fixed on Windows (2026-11-19)

Main `392dffc6f` merged (conflicts in the round-trip contract's digest and the
Windows manifest resolved; production app digest recomputed). Stopped at the
owner's word with no suite started.

Fixed:

- **Windows review recovery.** Taking the journal's first opens one at a time
  fixed every review-flow course. In the 31-row round at `d8b110483`, these
  passed: `start_review`, `label_publication_recovery`,
  `review_submission_composition` (100, 125 and 200%), `discuss_top`,
  `header_source_navigation`, `review_navigation_entry`,
  `review_submission_cli_ordinary`, `dependable_session`,
  `home_approve_again`, `review_seen` (and `_stays` and `_fails`),
  `local_comments_composition`, `agent_crash_recovery`, the new journal
  concurrency course, and the two agent setup store courses.
- **Main's known failures.**
  - `application_review_round_trip_composition_contract`: the full
    `github_application.cpp` digest was resealed over BPR-2281, and the
    stale-head probe reads BPR-2263's sentence for a moved head. Passes on
    Ubuntu and Windows.
  - `qualification_case_inventory_contract` and
    `command_correctness_registry_contract`: BPR-2262's case was added to the
    inventory. Both pass on both platforms.
  - `test_network_isolation_manifest`: `sqlite_prompt_pack_storage`
    (`prompt-pack-store-predicate-failed`) and the held-turn course
    (`private-test-root-refused`) were measured on Windows and pinned. The
    file-drop course is pinned unmeasured.
  - `dependable_session`, `start_review` and `agent_crash_recovery` passed on
    both platforms.
  - `review_cycle_parity` is registered only where `app_bpr` builds and was
    not run here.
- **Custom agent on Windows.** It now expects no grant there, and passes.

Still open on Windows (31-row round at `d8b110483`, then the 8-row round at
`2f3a99042`):

- The native agent's copy of the pull request is never made:
  `agent.workspace.failed action=workspace_materialize failure=copy_failed`
  (`native_agent_sessions`, `agent_invocation_composition_contract`).
- The contained native host still refuses: "The pull request's checkout could
  not be resolved", even after the GetFinalPathNameByHandleW change
  (`agent_invocation_host`, `agent_workspace_entry_limit`).
- `device_authorization`: the run's 8 s budget expires at stage 1 with the
  device flow cancelled (8092 ms).
- `review_tour_writing_wrapper`: "writing child process and cleanup finish".
- The RFC-0017 recorder, uncontained: its bindings are refused ("response
  does not bind this packet exactly once"), and `core.autocrlf=false` in its
  scratch Git did not change that.
- `application_window_file_drop_public_path` is still to be measured.

Not run: the WSL2 i9 load-phase profile, and the full Release and Debug
suites on both platforms and the eleven WSL2 timing courses at the lane head.

## Ninth round: main bafdd264f merged; Windows agent workspace courses (2026-11-19)

Main `bafdd264f` (BPR-2285 to BPR-2298) merged; production app digest
recomputed. Windows rounds t10 to t15 ran in the interactive producer context
(scheduled task, session 1); t10, t11 and t15 were probe builds on side
branches, not on the lane.

Fixed:

- **The native agent's copy (`copy_failed`).** Not the product: the native
  session course made its root with the contained helper, which on Windows
  inherits the temporary folder's AppContainer grants, so startup recovery
  refused the workspace folder beside its data root and every native turn
  found none. The course now narrows its root to the user, as the ownership
  course does. `native_agent_sessions` passes (t12, `6d2ceba96`).
- **The seed's exact context.** BPR-2298 moved the shipped review skill to
  version 6; the contract still asked for 5 and failed on every platform. The
  seed and reopen pass. A failure now names each term.
- **Checkout resolution in the contained host.** The host test's fake checkout
  was `/tmp/...`, the drive root on Windows, which a contained test cannot
  create. It now lives in the temporary folder there; grants, which are POSIX
  rules, are checked as absent on Windows. `agent_invocation_host` and
  `agent_workspace_entry_limit` pass. The product also falls back to the
  folder's opened name when Windows refuses to normalize it.
- **`git_process_contract`.** BPR-2294's 20,003-file course takes 214.65 s on
  Windows (Release, t12); the Windows timeout is 430 s.
- **RFC-0017 recorder.** Its fixtures now write LF. Uncontained it then
  refuses every capture: Python cannot open a directory on Windows to sync it,
  and the contract asserts that sync. The row is registered off Windows.
- **Manifest.** The file-drop course and main's three new rows were measured
  contained at `4d26a890d` and pinned (see the containment record).
  `test_network_isolation_manifest` should pass at the lane head.

Still open on Windows:

- `agent_invocation_composition_contract`, setup-wrapper course: the window's
  `agentSetup` widget is hidden (`visible=0 ... hidden=QWidget/agentSetup`)
  and its step says "Waiting for the pull request to load". The log has no
  warning or error. It passed on Ubuntu at the 2026-11-16 sweep.
- `review_tour_writing_wrapper`: with the scoped agent on PATH as claude, the
  writing request starts no process and logs no run; the two failed runs
  logged are the no-claude control's. Staging the copy early changed nothing,
  and no `failed_to_start` site in the invocation host or launcher fired (t15
  probe).
- `device_authorization`: the 8 s budget expires at stage 1; unchanged.

Not run: the WSL2 i9 load-phase profile, and the full Release and Debug
suites on both platforms and the eleven WSL2 timing courses at the lane head.

### Ninth round, continued (2026-11-19)

- **Setup-wrapper course, every platform.** Since BPR-2256 the agent setup is
  not open by itself in this course: the pull request loaded (`files=1`,
  "Pull request ready.") and the setup page stayed hidden behind the Agent tab,
  on Ubuntu and on Windows. The course now opens setup from the Agent tab, as
  the reviewer does. `agent_invocation_composition_contract` passes on Ubuntu
  (lane head) and Windows (t19, `31b46c022`). `test_network_isolation_manifest`
  passes on Windows at `31b46c022`.
- **`review_tour_writing_wrapper`** passes on Ubuntu at the lane head; the
  failure is Windows only and still open.
- **`device_authorization` on Windows.** Run directly with the test's
  environment and a 60 s budget, it passes: `PUBLIC_PATH_OK`, stage
  `cancelled`, 16,754 ms. The registered flow's round trip to GitHub takes
  about 17 s there against the 8 s self-test budget; the row passed in 2 to
  3 s in BPR-0820's Windows record. Where the time goes is not measured.
- **`git_process_contract` on Windows.** In t19 it failed at 20 s ("unexpected
  reverse index fails exact on-disk validation"), and the next run could not
  start it: Microsoft Defender detected `Trojan:Win32/Bearfoos.A!ml` (a
  machine-learning heuristic) in the contract binary and in its copy as
  `fake-exec\git.exe`, at 21:42 local during t19, and removed the binary. The
  same binary passed in full in t12. Whether to exclude the build tree or
  restore the file is the owner's.
- **`review_tour_writing_wrapper` on Windows: fixed.** The fixture agent
  admission split a claude search PATH on `:` as well as `;`, so on Windows
  `C:/...\claude-search` became `C` and `/...\claude-search`; the claude the
  course put on that PATH was found and then refused before the launcher,
  ending the run as `failed_to_start` with no words. It now splits on
  `QDir::listSeparator()` (testing builds only). Passes on Windows (t23,
  `3f6c4853e`), with `review_tour_composition` and `enhance_comment_note`.
- **`device_authorization`'s time is the PC's network.** With stage
  timestamps (probe, t22): `requested` at 0 ms, `awaiting_authorization` at
  8,558 ms, `cancelling` at 8,573 ms, `cancelled` at 8,606 ms; the run took
  8,737 ms. All of it is the device-code request to GitHub. From the PC,
  `curl` to `https://github.com/login/device/code` took 3.3 to 6.0 s (TCP
  connect 1.4 to 3.5 s); from the Mac, 0.16 to 0.20 s. The row passed at
  7.45 s in t21 and failed at 8.1 s before. The budget is unchanged.
- **`application_review_round_trip_composition_contract`, every platform.**
  BPR-2286 added `window_follow_up.cpp` on main without listing it in the
  contract's app source inventory or production manifest; both now name it,
  and the production digest is resealed.
- **WSL2 i9 load-phase input gap, located.** `application_large_review_responsiveness`
  at `82ed12f26` under `perf` (cpu-clock, 1 kHz, DWARF stacks; perf built from
  Ubuntu's `linux-perf` package unpacked without root), six runs: the phase's
  gap was 106 to 135 ms, 90 to 113 ms of it the GUI thread's own CPU. In each
  run the phase's longest main-thread stretches are single posted events of
  112 to 123 ms, twice per phase (about 36 ms in, and again later), all in
  `diff_workbench::rebuild_geometry` → `plain_text_pane_measurement::advance`
  → `pane_block_measurer::height`, which is Qt text layout and HarfBuzz shaping
  (`QTextLine::layout_helper`, `QTextEngine::shapeText`). `rebuild_geometry`
  checks its 4 ms slice only between groups of 16 lines, and a native block is
  measured whole, so one long block in the fixture's diff holds the thread for
  the whole layout. Not fixed: a change to how one long block is measured is a
  product change and is the owner's to schedule.
- The round-trip contract passes on the WSL2 i9 at `82ed12f26`.

### Owner decisions (2026-11-20)

- **Known Release timing exception for 0.5.0, waived by the owner:**
  `application_large_review_responsiveness`'s
  `paged_review_and_git_load_input_gap` on the WSL2 i9, 104 to 134 ms against
  the 100 ms input target in 9 of 30 runs (106 to 135 ms in the six profiled
  runs), from one long text block's layout in
  `diff_workbench::rebuild_geometry` → `pane_block_measurer::height` (profile
  above). The fix is [[BPR-2317]] in Backlog. The row's other checks still
  count.
- **`application_device_authorization_public_path` budget: 20 s on every
  platform** (owner). Its run waits for GitHub's device-code answer; on the
  Windows qualification PC that took 8,558 ms, and curl to the endpoint 3.3
  to 6.0 s against 0.16 to 0.20 s from a Mac. The row's CTest timeout is 30 s
  so the bound still holds three quarters of it.

## Tenth round: main 9e8deb514 merged; Release only (2026-11-20)

Main `9e8deb514` (BPR-2299 to BPR-2316) merged. Owner: Release only for now;
this lane owns Windows and the WSL2 timing courses, and a separate agent took
the Ubuntu guest over after its Release suite.

Build fixes main needed (BPR-2308 and others):
`helper_tour_round_diagnostics_tests.cpp` called BPR-2308's removed cycle
failure names; `toml_string` and `materialize_agent_environment` sat inside the
agent-integration block while the Codex instructions probe outside it called
them (tour-cli preset); `native_session_name` and BPR-2303's macOS-only bundle
helpers failed GCC's unused-function check. Inventories: the workbench and
qualification case inventories follow their sources (no case changed); the
round-trip contract's app inventory and production manifest name
`window_follow_up.cpp`.

| Candidate, platform | Rows | Result |
| --- | --- | --- |
| `78c6b3908` Ubuntu (Lima) Release, full | 1085 | 1071 passed, 14 failed |
| `2ca2bdf5f` Ubuntu (Lima) Release, full | 1046 | 1033 passed, 13 failed (10 large-review timing judged on WSL2; `qt_profiles_settings_panel`, `profile_data_reset`, `ask_report`) |
| `2ca2bdf5f` Windows Release, full | 1025 | 1018 passed, 7 failed |
| `afeb013b7` Windows Release, the 7 and neighbours | 6 | 6 passed |

Windows Release failures at `2ca2bdf5f` and their fixes:

- `qt_profiles_settings_panel` (also Ubuntu): BPR-2308 retired Open Project…;
  the course expects New Window, Quit and Close Tab.
- `application_profile_data_reset_public_path` (also Ubuntu): the profile made
  the cycles store at start and makes no review store now; the course makes
  the tours store (as a draft's target does) so the review reset and the cache
  recovery have one to check.
- `github_cli_process_runner` (Windows): the progress-bounded runner summed
  directory-entry sizes, which on Windows lag a file still open for writing,
  so a growing pack looked idle and a fetch was stopped at its idle bound. It
  now asks each file for its size.
- `application_discuss_top_public_path` (Windows, after any other course): a
  product bug. `relayout` kept the last timeline's folded counts when it laid
  out one file's threads outside the timeline, so Discuss counted 14
  conversations on a file with seven. Folded counts reset on every layout.
- `application_dependable_session_public_path` and
  `application_composition_contract` (timeout at 1800 s in the full suite)
  passed when rerun (13.9 s and 720 s).
- `application_large_review_launch`: `theme_dark` 160.6 ms against 100 in one
  run, passed alone twice. Recorded, not changed.

`git_process_contract` passes with the owner's Defender exclusion (199.6 s);
Defender made no detection after 2026-11-19 21:42. Two untracked copies of the
retired `bpr-cycle` skill in the PC's source tree were moved to
`preflight-fix-r1\stale-bpr-cycle-skill`.
