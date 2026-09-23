---
schema: "implement-rfc.card.v1"
id: "BPR-0874"
title: "Windows: five rows refuse at startup with registry_unavailable off the benchmark's bare temporary data root"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-merged-head-runs, measuring the integrator's BPR-0856 decision","found_at":"2b2e5549","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"All five rows pass on Windows, contained and uncontained as each is registered","outcome":"pass","revision":"9e0f239e","evidence":{"kind":"command","detail":"bpr-main-win (Windows 11 10.0.26200, MSVC 19.51 Debug), serial, offscreen, as registered after the change: application_large_review_benchmark 38.25 s (LARGE_REVIEW_COMPOSED_BENCHMARK_OK), application_unavailable_diff_public_path 18.96 s, application_approximate_diff_public_path 28.27 s, application_review_ledger_composition_contract 21.58 s, application_defect_composition_contract 29.56 s; test_network_isolation_manifest TEST_NETWORK_ISOLATION_OK:configured=414:registrations=388. Before, at 470920d5, the three large-review rows failed 'application startup succeeds' contained and uncontained. The only change after the measurement is clang-format whitespace in the benchmark.","path":"docs/issues/60-done/BPR-0874-the-windows-large-review-data-root-needs-a-third-arrangement.md#one-arrangement-on-every-platform-three-rows-pinned-bpr-main-win-2026-09-23"}},{"criterion":"The data root arrangement is recorded here, with why Windows needs what it needs","outcome":"pass","revision":"9e0f239e","evidence":{"kind":"observation","detail":"Recorded with measurements of all three candidates: the bare QTemporaryDir is refused (registry_unavailable); a create_storage_root_owner_private child under it composes uncontained but leaves the agent-workspace neighbourhood in %TEMP%; the POSIX arrangement composes all three uncontained with no recovery refusal, so it now serves every platform through native_filesystem_path. Contained nothing passes (WRITE_DAC, BPR-0833), hence the dos-volume-name pins.","path":"docs/issues/60-done/BPR-0874-the-windows-large-review-data-root-needs-a-third-arrangement.md#one-arrangement-on-every-platform-three-rows-pinned-bpr-main-win-2026-09-23"}},{"criterion":"registry_unavailable no longer stands in for several distinct startup failures, or the card says why it must","outcome":"pass","revision":"9e0f239e","evidence":{"kind":"observation","detail":"The benchmark prints LARGE_REVIEW_STARTUP_REFUSAL:<profile_startup_refusal_name()> (BPR-0877's recorder of the first profile refusal); measured with the old root it names registry_unavailable. The card says why the registry arm itself stays one enumerator: every caller maps it to the same outcome, the underlying refusal is now named for any reader, and splitting it would move the EPOCH_FIXTURE_NONCANONICAL_ROOT_CAUSE_OK:registry pin; the sibling arms are BPR-0882's.","path":"docs/issues/60-done/BPR-0874-the-windows-large-review-data-root-needs-a-third-arrangement.md#one-arrangement-on-every-platform-three-rows-pinned-bpr-main-win-2026-09-23"}},{"criterion":"macOS and Linux keep passing all five","outcome":"pass","revision":"9e0f239e","evidence":{"kind":"observation","detail":"The POSIX branch's code is unchanged apart from naming the root through native_filesystem_path, which off Windows returns the same UTF-8 bytes as toStdString; WINDOWS_UNCONTAINED and the dictionary act only on the windows-appcontainer backend; the two composition rows were not changed. No macOS or Linux run was made from this station.","path":"docs/issues/60-done/BPR-0874-the-windows-large-review-data-root-needs-a-third-arrangement.md#one-arrangement-on-every-platform-three-rows-pinned-bpr-main-win-2026-09-23"}}]
---

# BPR-0874: Windows: five rows refuse at startup with registry_unavailable off the benchmark's bare temporary data root

## Defect

Five rows fail on the Windows station before they reach any assertion of
their own:

- `application_large_review_benchmark`
- `application_unavailable_diff_public_path`
- `application_approximate_diff_public_path`
- `application_review_ledger_composition_contract`
- `application_defect_composition_contract`

All refuse at startup with `profile_startup_failure 2`, which is
`registry_unavailable`, off the bare `QTemporaryDir` data root the Windows
branch keeps (for example
`C:/Users/mikem/AppData/Local/Temp/better_pull_review_large_review-REVlKc`).

**This corrects an integrator decision, and the correction was a measurement.**
[[BPR-0856]] gave the benchmark an owner-private data root parent **off
Windows only**, and the integrator kept that `#if` on the reasoning that
`%TEMP%` is per-user, so the shared-`/tmp` cause could not arise on Windows.
The cause does not arise — but a different one does, off the same bare root,
and the rows fail just the same.

The station then measured both alternatives, which is what makes this card
actionable rather than a report:

- With the **POSIX arrangement** applied on Windows, the row **composes**
  uncontained (`UNAVAILABLE_DIFF_COMPOSED_OK`).
- **Contained** with that same arrangement, it stops earlier at "private data
  root parent" — the ancestor-chain narrowing an AppContainer cannot perform,
  which is the half of the original reasoning that was right.

So neither of the two arrangements that exist today works on Windows in both
containment modes. **Windows needs a third**: a root created owner-private
*with the container's own entry*, which
`create_storage_root_owner_private` has provided since [[BPR-0833]].

The failure is also not about containment by itself: the benchmark fails
identically contained and uncontained, and two of the rows that share the
cause are registered `WINDOWS_UNCONTAINED`.

## Scope

- Give the Windows branch the third arrangement: create the data root
  owner-private with the container's entry, rather than either the bare
  `QTemporaryDir` or the POSIX narrowing.
- Verify it in **both** containment modes, since the two existing options each
  work in exactly one.
- Then reconsider whether the `#if` is still needed at all. If the third
  arrangement is correct everywhere, one path is better than two; if it is
  not, say on this card why each platform needs its own.
- `registry` is the catch-all arm of `github_application_startup_failure`, so
  the reported cause hides which profile-startup failure actually fired. The
  station needed a temporary print to find out. **Give that arm its own named
  failures** — it is the difference between one run and a bisect, and it is
  the same lesson as the cleanup cluster in [[BPR-0875]].

## Acceptance

- All five rows pass on Windows, contained and uncontained as each is
  registered.
- The data root arrangement is recorded here, with why Windows needs what it
  needs.
- `registry_unavailable` no longer stands in for several distinct startup
  failures, or the card says why it must.
- macOS and Linux keep passing all five.

## Two additions from the r5 critical review

**The sibling arm has the same disease, and a pin depends on this one.**

- The `unavailable` arm collapses `github_profile_runtime` construction,
  `start_services()` and `open_window` into one word
  (`src/app/src/github_application.cpp:12633-12655`), exactly as `registry`
  does for its seven causes. [[BPR-0882]] carries that arm and the two other
  boundaries that discard `what()`. Fix them in one pass if convenient; the
  work is the same shape.
- **`tests/end_to_end/CMakeLists.txt:2295-2296` pins
  `EPOCH_FIXTURE_NONCANONICAL_ROOT_CAUSE_OK:registry`.** When this card splits
  the `registry` arm into named causes, that pin must move with it, or it will
  fail for the right reason and look like a regression. Change them together.

## Classified (dsp-006 scout): a genuine defect; containment is a red herring here

The containment rule is recorded once on [[BPR-0872]]. It does **not** settle
this card. Two of the five rows —
`application_review_ledger_composition_contract` and
`application_defect_composition_contract` — are already in
`WINDOWS_UNCONTAINED_TESTS`, so they run **outside** the container and fail
anyway, which matches this card's own observation that the benchmark fails
identically contained and uncontained.

So the work is the third data-root arrangement plus splitting the `registry`
arm (`src/app/src/github_application.cpp:12633-12655`), not a registration
change. Containment adds only one extra constraint, already established by
[[BPR-0833]]: the third arrangement must carry the container's own SID, which
`create_storage_root_owner_private` writes at creation.

**Name the arm before fixing it**, exactly as [[BPR-0875]] must name its three
cleanup sources: say which of the seven `registry`/`unavailable` sub-causes
fires. That change lands in `src/app/**`, which no station owns. Note also that
this card couples to the pin at `tests/end_to_end/CMakeLists.txt:2295-2296`,
which has to move in the same commit.

## One arrangement on every platform, three rows pinned (bpr-main-win, 2026-09-23)

Run `bpr-main-win-20260923`. Windows 11 Home 10.0.26200, MSVC 19.51 Debug,
`build/dev` at `470920d5` and later, serial and offscreen. Logs are under
`build/logs/bpr-0872-0876-main-win/` and are not committed.

**The premise, rechecked.** Two of the five rows have moved.
`application_review_ledger_composition_contract` (21.30 s) and
`application_defect_composition_contract` (29.65 s) pass at `470920d5`,
uncontained as pinned. `df54d00b` (2026-09-17) fixed them. Their CMake courses
had narrowed the root with `file(CHMOD ...)`, which only clears the read-only
attribute on Windows. They now call the shared owner-private helper and take
the owned parent [[BPR-0875]] gave the other courses. The three large-review rows
still failed, contained and uncontained alike, at
`large-review-benchmark-failed: application startup succeeds`.

**Which refusal, named.** With the old bare `QTemporaryDir` root, printing
[[BPR-0877]]'s `profile_startup_refusal_name()` gives `registry_unavailable`.
The benchmark now prints that name itself when startup fails
(`LARGE_REVIEW_STARTUP_REFUSAL:<name>`), so the next failure needs no
temporary print.

**The arrangement.** There are three candidates:

- **The bare temporary folder.** It carries `%TEMP%`'s inherited entries, and
  the profile registry refuses it as a root.
- **A data child made by `create_storage_root_owner_private`, under the bare
  folder.** This composes all three courses uncontained once the root is named
  natively. Named with Qt's '/', the storage root validator refuses it
  ([[BPR-1031]]'s lesson). But it leaves the agent-workspace neighbourhood in
  `%TEMP%`, where recovery refuses at quit.
- **The POSIX arrangement.** Narrow the temporary folder to owner-private,
  then create `data` inside it and narrow that too. This composes all three
  uncontained (`LARGE_REVIEW_COMPOSED_BENCHMARK_OK`, `UNAVAILABLE_DIFF_COMPOSED_OK`,
  `APPROXIMATE_DIFF_COMPOSED_OK`). It prints no
  `AGENT_WORKSPACE_RECOVERY_REFUSED`, because the neighbourhood sits under the
  private parent.

So the `#if` is gone. One arrangement serves every platform: the POSIX one,
with the root named through `bpr::app::native_filesystem_path`. That is
Windows' native spelling there, and the same UTF-8 bytes as before on macOS
and Linux. Windows does not need a third arrangement. It needed the existing
one, spelled natively.

**Contained, nothing passes, which is why three rows are pinned.** Contained,
the course stops at its first step, "private data root parent": narrowing the
`QTemporaryDir` needs `WRITE_DAC`, which the container lacks. That is the
refusal [[BPR-0833]] measured and pinned. `create_storage_root_owner_private`
on a child is refused contained too. This matches BPR-0833's decision that no
choice of parent passes contained while the stores walk their ancestor chain.
The three rows are registered `WINDOWS_UNCONTAINED` and pinned
`dos-volume-name`, keywords and dictionary in one commit.

| Row | Before (`470920d5`) | After, as registered |
|---|---|---|
| `application_large_review_benchmark` | `application startup succeeds` (both ways) | Passes, 38.25 s |
| `application_unavailable_diff_public_path` | same | Passes, 18.96 s |
| `application_approximate_diff_public_path` | same | Passes, 28.27 s |
| `application_review_ledger_composition_contract` | Passes, 21.30 s | Passes, 21.58 s |
| `application_defect_composition_contract` | Passes, 29.65 s | Passes, 29.56 s |

`test_network_isolation_manifest` reports
`TEST_NETWORK_ISOLATION_OK:configured=414:registrations=388`, and
`test_network_isolation_structure_contract` passes.

**Why `registry` stays one arm of `github_application_startup_failure`.** The
defect was that the arm hid which profile refusal fired, and a station needed a
temporary print to learn it. [[BPR-0877]] records the first profile refusal
and names it (`profile_startup_refusal_name()`) for any reader. This course and
the device-binding seed (`SEED_STARTUP_PROFILE_REFUSAL:`) now print it. Every
caller of the arm maps it to the same outcome, so a new enumerator would add a
spelling, not a behaviour. Splitting it would also move the pinned
`EPOCH_FIXTURE_NONCANONICAL_ROOT_CAUSE_OK:registry`. The other startup
boundaries that discard their cause are [[BPR-0882]]'s. That card coordinates
the pin if either card ever splits an arm.
