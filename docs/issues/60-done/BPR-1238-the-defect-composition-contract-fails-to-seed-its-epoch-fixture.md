---
schema: "implement-rfc.card.v1"
id: "BPR-1238"
title: "The defect composition contract fails to seed its epoch fixture"
status: "Done"
rfcs: []
depends_on: []
metadata: {"origin":"red at baseline in the Review Cycles snapshots integration pass (fails identically at 8374ea995); filed by the authoring milestone implementer on 2026-09-29","reservation":"docs/issues/_reservations/BPR-1238--BPR-1238--1bcc75d6c30d.json"}
acceptance: [{"criterion":"application_defect_composition_contract passes on macOS","outcome":"pass","revision":"6e1bf4ddc","evidence":{"kind":"command","detail":"ctest --test-dir build/dev -R ^application_defect_composition_contract$ pass (55 s), seed and reopen on github.com and GHES 3.18; controls without each half of the fix fail at the recorded steps."}},{"criterion":"The card records which step of the seed failed and why","outcome":"pass","revision":"6e1bf4ddc","evidence":{"kind":"observation","detail":"awaiting_shared_ready: the selection after reversal is left:29 and present_review_draft offers new roots only on the right (f55e39e72, BPR-0587, found by bisect); the reopen ZeroReopenWrites failure is an own-write reload dropping a correlated availability (BPR-1211 timing)."}},{"criterion":"Documentation gate","outcome":"pass","revision":"6e1bf4ddc","evidence":{"kind":"command","detail":"python3 scripts/verify_documentation.py --root . -> DOCUMENTATION_OK"}}]
---

# BPR-1238: The defect composition contract fails to seed its epoch fixture

## Observation

At `main` 95a30c5b0 on macOS arm64 (dev preset, Homebrew LLVM),
`ctest --test-dir build/dev -R "^application_defect_composition_contract$" --output-on-failure`
fails. The probe reaches a ready pull request and an exact epoch pair, then
reports `GITHUB_FIXTURE_FAILED:github.com` with
`PUBLIC_PATH_VERDICT:presentation=0` and
`GITHUB_FIXTURE_EVIDENCE:01111110:9:10:9`, followed by
`DEFECT_COMPOSITION_SEED_FAILED` and
`DEFECT_COMPOSITION_EVIDENCE:11111100011`
(`tests/end_to_end/application_defect_composition_contract.cmake:95`, called
from line 134). The snapshots milestone recorded the same failure at the
pre-milestone base 8374ea995, so no Review Cycles change introduced it.

## Outcome

The contract passes at the resulting `main` head, with the cause of the
presentation and seed failure fixed where it lives rather than the expected
evidence relaxed.

## Direct acceptance

- `ctest --test-dir build/dev -R "^application_defect_composition_contract$" --output-on-failure` passes on macOS at the candidate revision.
- The card records which step of the seed failed and why.
- `python3 scripts/verify_documentation.py --root .` passes.

## Resolution (main stabilization milestone, macOS arm64, 2026-09-30)

Candidate `6e1bf4ddc`. There were two causes; the second was hidden until the
first was fixed.

### Which step of the seed failed, and why

The seed course reaches `awaiting_shared_ready` and waits for a draft in the
editor. A trace of `adopt_review_selection` showed the selection after the
epoch reversal is `left:29`, the first thread's line, which the comparison
keeps. `present_review_draft` returns early for a left-side line: a new root
comment takes only a right-side line. A bisect over `beddf1f55..d25f17b69`
(the contract passed on 2026-09-26 at beddf1f55) named f55e39e72, "Finish
BPR-0587", which added that guard with RFC-BPR-0023's private drafts. The
guard is the product rule ([[BPR-1207]] keeps it: left replies, no left
roots), so the probe was relying on a left-side root. It now picks a
right-side line before the shared follow-up.

### The reopen course

With the seed passing, the reopen course failed `ZeroReopenWrites`
(evidence `11111111010`): reopening persisted the first defect as outdated. The
seed had written it back as available. The course refuses the correlated
write that marks both defects outdated after the reversal (the fail-closed
check), then verifies the second defect. That write's change notification
reloaded the window's defects from the ledger, which never took the refused
availability, so the next gesture (reclassifying the first defect) wrote
`available` back. Before [[BPR-1211]] the gestures ran in one turn and the
reload landed after them; with the ledger asynchronous the course waits for
each write, and the reload lands between them. A reviewer would hit the same
thing whenever a correlated write fails and they act on another defect.

`refresh_shared_ledger` now reloads a window's own private records with
correlation (`reload_private_records(own)`), as opening does. Correlation
writes only a changed history, so it converges; siblings still adopt the
records without correlating or persisting their view.

### Checks

| Check | Result |
|---|---|
| `application_defect_composition_contract` at `6e1bf4ddc` (seed and reopen, github.com and GHES 3.18) | Pass, 55 s |
| Probe fix alone, without the reload change | Seed passes, reopen fails `ZeroReopenWrites` (control) |
| Neither change (baseline) | Seed fails `DEFECT_SHARED_READY_STALLED` (control) |
| `application_review_ledger_composition_contract`, `_overview_since_review_contract`, `_threaded_reply_contract`, `_setup_destination_public_path`, `_profile_multi_window_public_path`, `_profile_second_window_close_public_path`, `_profile_two_account_isolation_public_path`, `_active_changed_scaling`, `_private_review_{native,file,retry}` | 12 of 12 pass with the change (worktree build of the same bytes) |
| `application_review_round_trip_composition_contract` after the reviewed reseal | Pass |

## Linux — bpr-main-linux, 2026-09-30

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2, clang-format 22.1.8 at `/usr/bin/clang-format`, Qt 6.11.2 offscreen with `QT_STYLE_OVERRIDE=Windows` (default font DejaVu Sans), `build/dev` (Debug, `-Werror`) freshly configured at `69493e235`, which contains the Windows station's `0dd1eb53f`.

`application_defect_composition_contract` passes, and so does `application_review_round_trip_composition_contract`.

The logs are station records in `build/logs/bpr-station-stabilization/` and are not committed.

## Windows — bpr-main-win, 2026-09-30

Windows 11 Home 10.0.26200, MSVC 19.51, clang-format 22.1.8, Qt 6.11.2, offscreen, `build/windows-dev` Debug with warnings as errors, at `69493e23`.

`application_defect_composition_contract` passes, 33.19 s, and again at the recheck, 32.51 s.
The details are in [main stabilization](../../verification/main-stabilization-2026-09-30.md).
