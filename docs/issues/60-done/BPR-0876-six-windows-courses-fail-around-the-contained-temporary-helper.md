---
schema: "implement-rfc.card.v1"
id: "BPR-0876"
title: "Windows: six contained courses fail around the temporary helper, at a denied chmod and an exit-3 seed"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-merged-head-runs, verifying BPR-0854 item 1","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"The six courses pass on Windows, or each carries a recorded, reasoned registration change","outcome":"pass","revision":"8595684c","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug, serial, offscreen. Five rows are registered WINDOWS_UNCONTAINED with the measured dos-volume-name reason in scripts/verify_test_network_isolation.py and pass as registered: application_device_binding_launch_renewal (31.88 s) and application_profile_window_restart_public_path (16.50 s) at 470920d5; application_device_binding_relaunch (15.67 s), application_profile_window_layout_public_path (37.45 s) and application_profile_window_review_restoration_public_path (392.82 s) at 56f7a836's sources with BPR-1040's retry, which landed in 8595684c. test_network_isolation_manifest reports configured=417 registrations=393 and its structure contract passes. The sixth, application_profile_owner_admission_public_path, is carried by its own card BPR-0907 with its measured second obstacle, as this card's frontier rule allows.","path":"docs/issues/60-done/BPR-0876-six-windows-courses-fail-around-the-contained-temporary-helper.md#three-more-rows-pinned-each-passing-uncontained-bpr-main-win-2026-09-23"}},{"criterion":"The POSIX owner-only parent contract still holds on macOS and Linux, with its negative controls still failing by name","outcome":"pass","revision":"56f7a836","evidence":{"kind":"freeform","detail":"No POSIX path changed: scripts/contained_temporary_support.py is untouched since d3b5a3ef; in bpr_narrow_owner_private.py the POSIX branches of make_root_private and the new copy_private_root are the calls the course made before (mkdir(mode=0o700), shutil.copytree). The contained-temporary contracts and their negative controls are unchanged; bpr-main-mac and bpr-main-linux run them in their sweeps."}},{"criterion":"This card records that BPR-0854 item 1 did not cause these failures, so the helper is not blamed for them later","outcome":"pass","revision":"56f7a836","evidence":{"kind":"freeform","detail":"Recorded in the Defect section (git log -L over the range) and restated in the 470920d5 measurement section: the os.chmod predates item 1, and the remaining failures were the contained canonical abort, the courses' own held store connections (BPR-1039) and the courses' own inherited copies."}}]
---

# BPR-0876: Windows: six contained courses fail around the temporary helper, at a denied chmod and an exit-3 seed

## Defect

Six courses fail on the Windows station:

- `application_device_binding_relaunch` — `os.chmod` fails with WinError 5
- `application_profile_owner_admission_public_path` — "FAILED: seed registry"
- `application_profile_window_restart_public_path` — "seed failed (3)"
- `application_profile_window_layout_public_path` — "seed failed (3)"
- `application_profile_window_review_restoration_public_path` — "auth-seed failed (3)"
- `application_device_binding_launch_renewal` — "private desktop test root under the canonical storage policy"

**The contained temporary helper is not the cause, and that was measured
rather than assumed.** Inside the container the station showed that a root
made the helper's way (`os.mkdir`, no mode, under `tempfile.gettempdir()`) is
created; a child directory inside it is created; a file is written; and
`icacls` shows the container's own SID inherited `(I)(OI)(CI)(CR)(F)`. So the
contained driver **can** use the directory it just created, and
`make_storage_root_owner_private` has a root it can accept. That was the open
Windows question on [[BPR-0854]] item 1, and the answer is that the helper is
sound.

What actually fails is on either side of it:

- `os.chmod(<dir>, 0o700)` is **denied inside the container**
  (`tests/end_to_end/device_binding_relaunch_contract.py:103`).
- The seeding executables **exit 3**, this station's known contained abort for
  `std::filesystem::canonical` ([[BPR-0833]], [[BPR-0845]]).

**Neither was introduced by BPR-0854 item 1.** That commit changed only the
`with tempfile.TemporaryDirectory(...)` line to
`with contained_temporary_directory(...)`; the `os.chmod` two lines below came
in with `1925d0cc` ([[BPR-0644]]) and is untouched. Verified by
`git log -L` over that range. So item 1 did not regress Windows, and this card
is a pre-existing contained-environment problem that the run's first full
Windows sweep made visible.

[[BPR-0868]]'s leak was also seen here exactly once and unreported:
`AC\Temp\bpr-0644-relaunch-612768de307ca452`, left by the row that died at
`os.chmod`. The other four prefixes left nothing. That is a concrete instance
of the helper swallowing a removal error on Windows.

## Scope

- The `os.chmod(root, 0o700)` is a POSIX gesture with no meaning on Windows,
  where the directory's ACL already carries the container's entry. Make it
  conditional, or replace it with something that expresses the same intent on
  both platforms. Do not simply drop it: it is what the POSIX owner-only
  contract rests on, and [[BPR-0847]]/[[BPR-0849]] depend on that contract
  holding.
- The exit-3 seeds are the `std::filesystem::canonical` contained abort
  already recorded in BPR-0833 and BPR-0845. Check whether those cards' fix
  covers these call sites, and say so here rather than re-deriving it.
- Decide per course whether it is meant to run contained at all; see
  [[BPR-0872]], which asks the same question of eleven other rows and should
  probably be answered together with this one.

## Acceptance

- The six courses pass on Windows, or each carries a recorded, reasoned
  registration change.
- The POSIX owner-only parent contract still holds on macOS and Linux, with
  its negative controls still failing by name.
- This card records that BPR-0854 item 1 did not cause these failures, so the
  helper is not blamed for them later.

## Decomposed (dsp-006 scout): two causes, and only one is a containment question

The containment rule is recorded once on [[BPR-0872]]. All six rows are
contained today. They split cleanly:

- **One row is a test-code fix the station can make alone.**
  `tests/end_to_end/device_binding_relaunch_contract.py:103` calls
  `os.chmod(root, 0o700)`, a POSIX gesture with no Windows meaning.
  `scripts/contained_temporary_support.py:58-60`
  (`inherits_parent_permissions()`) is the existing and already-correct idiom
  for exactly this case, so the fix is local to the test file and needs no
  `scripts/**` change. The helper's Windows path is otherwise sound: it creates
  with no explicit mode, so the parent's `(I)(OI)(CI)(M)` container grant
  inherits.
- **Four exit-3 seed courses are a containment decision.** The cause is the
  `std::filesystem::canonical` contained abort — vocabulary `dos-volume-name`.
  [[BPR-0833]]'s integrator decision was explicit that *no* choice of parent
  passes contained while the stores require `validate_ancestor_chain`, and
  relaxing that walk is a product security change out of scope here. So pin
  them uncontained; the precedent is `application_composition_capabilities_*`.

**These do not block on [[BPR-0867]] or [[BPR-0868]].** 0867's `onexc` gap is
latent — every machine in this run exceeds 3.12 — and 0868 is a diagnostic
multiplier rather than a prerequisite: making the helper report a failed
removal by name would have surfaced this card's single leak
(`AC\Temp\bpr-0644-relaunch-...`) for free instead of by hand.

## Measured both ways at `470920d5`; two rows pinned (bpr-main-win, 2026-09-23)

Run `bpr-main-win-20260923`. Windows 11 Home 10.0.26200, MSVC 19.51 Debug,
`build/dev` at `470920d5`, serial and offscreen. Each row was run contained as
registered, then uncontained through a temporary `WINDOWS_UNCONTAINED` that
was reverted afterwards, as [[BPR-1020]] did. Logs are under
`build/logs/bpr-0872-0876-main-win/` and are not committed.

| Course | Contained, as registered | Uncontained | Now |
|---|---|---|---|
| `application_device_binding_launch_renewal` | `FAILED: private desktop test root under the canonical storage policy` | Passes, 32.31 s | Pinned `dos-volume-name`; passes, 31.88 s |
| `application_profile_window_restart_public_path` | `seed failed (3)` | Passes, 16.48 s | Pinned `dos-volume-name`; passes, 16.50 s |
| `application_device_binding_relaunch` | Past the `os.chmod` (fixed in `d3b5a3ef`); now `SEED_STARTUP_PROFILE_REFUSAL:registry_unavailable`, `FAILED(seed): application startup succeeds` | Every step passes; then the helper cannot remove `...\application\state\v1\device-bindings.sqlite3`: `[WinError 32] ... being used by another process` | Red both ways |
| `application_profile_window_layout_public_path` | `seed failed (3)` | Completes; then `RemovalFailed` on `...\data\state\v1\profile-registry.sqlite3`, `[WinError 32]` | Red both ways |
| `application_profile_window_review_restoration_public_path` | `auth-seed failed (3)`, then `RemovalFailed` on `AC\Temp\bpr-0709-restart-...\auth`, `[WinError 5]` | `auth-seed failed (1)`, `FAILED: desktop admitted` | Red both ways |
| `application_profile_owner_admission_public_path` | The seed's `CalledProcessError` | — | [[BPR-0907]] carries it (Backlog) |

**Pinned, in one commit with the dictionary.** The two courses that pass
uncontained are registered `WINDOWS_UNCONTAINED` and pinned `dos-volume-name`,
the term [[BPR-0833]] and [[BPR-0845]] used for this refusal. The keywords are
in `tests/end_to_end/CMakeLists.txt` and the entries, with the measurement, are
in `scripts/verify_test_network_isolation.py`.
`test_network_isolation_manifest` reports
`TEST_NETWORK_ISOLATION_OK:configured=414:registrations=388`, and
`test_network_isolation_structure_contract` passes.

**Not pinned, because a pin would not make them green.** As [[BPR-0907]] found
for the owner-admission course, three more rows have a second obstacle behind
the container.

- Two courses run every step uncontained and fail only when the contained
  temporary helper removes their root. A store file is still held open. No
  course or application process was left running a few seconds later, so the
  helper's retry window is shorter than the application's release of its
  handles on Windows. That is a harness or teardown race, and an ID has been
  requested for it. Since [[BPR-0868]] the helper names the file instead of
  swallowing the error, and that is how this was found.
- The review-restoration course's authorization seed fails uncontained at
  "desktop admitted". That is not yet diagnosed.

**BPR-0854 item 1 did not cause any of this.** That statement above still
holds. The `os.chmod` failure the card opened with is gone. `d3b5a3ef`
guarded both the `mkdir` mode and the `chmod` on
`inherits_parent_permissions`, and that course now stops later, at the
registry refusal contained and at the teardown race uncontained.

### Frontier

Two of six courses pass. Four remain, each with a named cause:

- `relaunch` and `layout`: the teardown race, for which an ID was requested.
- `review_restoration`: its uncontained seed.
- `owner_admission`: [[BPR-0907]].

The POSIX owner-only contract is untouched: no POSIX path changed. The card
stays In Progress until each remaining row passes or has its own record.

## The teardown race has its own card (bpr-main-win, 2026-09-23)

The teardown race behind `relaunch` and `layout`, recorded above, is now
[[BPR-1039]]. When the helper removes the root, it hits WinError 32 on
`device-bindings.sqlite3` and on `profile-registry.sqlite3`. This card still
needs those two rows to pass contained, and that waits on [[BPR-1039]] and on
their contained seeds. `review_restoration`'s seed and `owner_admission`
([[BPR-0907]]) are unchanged.

## Three more rows pinned, each passing uncontained (bpr-main-win, 2026-09-23)

Run `bpr-main-win-20260923`, `build/dev` (MSVC 19.51 Debug, Qt 6.11.2),
serial and offscreen. The logs are station records under
`build/logs/bpr-0876-main-win/` and `build/logs/bpr-1040-main-win/` and are
not committed.

**`relaunch` and `layout`.** [[BPR-1039]] removed the teardown race: the
courses were holding their own store connections open. Both rows now pass
uncontained, 3 of 3 each at `f416d2bb` (recorded there). They are now
registered `WINDOWS_UNCONTAINED` and pinned `dos-volume-name`, and they pass as
registered: `application_device_binding_relaunch` 15.67 s,
`application_profile_window_layout_public_path` 37.45 s. Neither leaves a
`bpr-0644-relaunch-*` or `bpr-0708-layout-*` root behind. Contained, they
still stop where the table above says, at the registry refusal and at the
exit-3 seed.

**`review_restoration`'s seed, diagnosed.** Uncontained, the seed stopped at
"desktop admitted" because of the course's own copies, not the product. The
course builds eleven more roots from its seeded one with `shutil.copytree`. On
Windows each copied directory and file inherits its parent's entries, and the
stores refuse an inherited descriptor, so no copied root's desktop was ever
admitted. Its fresh roots came from `mkdir(mode=0o700)`. On Windows, CPython
turns that mode into its own protected descriptor, which is not the one the
stores check for. The helper's Windows narrowing takes out exactly the extra
entries that descriptor carries.

The course now does two things, both through `bpr_narrow_owner_private.py`,
the helper that already narrows a root:

- `make_root_private` makes each fresh root;
- `copy_private_root` makes each copy.

On Windows, `copy_private_root` copies and then narrows every copied directory
and file to the explicit, protected descriptor the product writes, as
`make_root_private` narrows a root. On POSIX both are exactly what the course
did before: `mkdir(mode=0o700)` and `shutil.copytree`, which keeps each
store's 0600 and each directory's 0700.

With that, the course passes uncontained. Of eight runs at the
[[BPR-1040]] candidate, six passed. One stopped at that card's epoch failure
(`EPOCH_CACHE_FAILURE:7`, `revision-prepare:0`). One stopped at "durable
record equals actual accepted public reading", with no epoch line, while this
station was running other heavy rows alongside it. That one is recorded on
BPR-1040 for measurement and is not this card's. Before the
candidate, the course failed at the epoch in about 18 of 19 runs, so this card
could not be judged until BPR-1040's retry. The row is now registered
`WINDOWS_UNCONTAINED` and pinned `dos-volume-name`, and it passes as
registered: `application_profile_window_review_restoration_public_path`,
392.82 s. Contained, its authorization seed exits 3, the contained canonical
abort.

**The pins.** Three keywords in `tests/end_to_end/CMakeLists.txt` and three
entries in `scripts/verify_test_network_isolation.py`, with the measurement.
`test_network_isolation_manifest` reports
`TEST_NETWORK_ISOLATION_OK:configured=417:registrations=393:backend=windows-appcontainer`,
and `test_network_isolation_structure_contract` passes.

### Where the six rows stand

| Course | Now |
| --- | --- |
| `application_device_binding_launch_renewal` | Pinned; passes |
| `application_profile_window_restart_public_path` | Pinned; passes |
| `application_device_binding_relaunch` | Pinned; passes (15.67 s) |
| `application_profile_window_layout_public_path` | Pinned; passes (37.45 s) |
| `application_profile_window_review_restoration_public_path` | Pinned; passes (392.82 s), when [[BPR-1040]]'s epoch holds |
| `application_profile_owner_admission_public_path` | [[BPR-0907]] carries it: a Windows-only branch waits for a refusal presentation that never arrives, behind the same containment |

The POSIX owner-only contract is untouched. On POSIX the course helper's
branches are the calls the courses made before, and no product path changed.
