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
