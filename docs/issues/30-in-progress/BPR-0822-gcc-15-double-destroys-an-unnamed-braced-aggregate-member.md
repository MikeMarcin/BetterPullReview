---
schema: "implement-rfc.card.v1"
id: "BPR-0822"
title: "GCC 15.2 destroys an unnamed braced aggregate member twice when a later initializer throws"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux under BPR-0816 (task-20260917-lnx-build)","found_at":"4883dbd9","run":"run-20260916-stations","dispatch":"dsp-002"}
---

# BPR-0822: GCC 15.2 destroys an unnamed braced aggregate member twice when a later initializer throws

## Defect

While fixing [[BPR-0816]], `bpr-linux` found a GCC 15.2 miscompile, not just
a warning. Take an aggregate whose member is itself an aggregate holding a
`std::string`. If that member is brace-initialized without naming its type
(`.Receipt = { .ServerCommentId = ... }`), and a later member's initializer
throws, GCC destroys the member twice. Outside the tree, AddressSanitizer
reports `attempting double-free` at `-O0`; at `-O2` the process aborts. Clang
21 and 22 are correct. Naming the type avoids it.

[[BPR-0816]] fixed the two sites GCC warned about at `-O3`:

- `src/adapters/github/src/github_mutation_contract.cpp`, where the later
  copies can throw `bad_alloc`;
- `src/adapters/sqlite/src/review_ledger_database.cpp`, where the edge cannot
  fire in practice.

**The warning appears only at `-O3`, so unwarned instances may remain.** No
survey was made. See
[the Linux stock build evidence](../../verification/BPR-0816-linux-gcc15-stock-build.md),
"The GCC miscompile". The reproducer is station-local on `bpr-linux`
(`/home/mmarcin/bpr-station-logs/task-20260917-lnx-build/repro/asan.cpp`) and
is not committed.

## Scope

- Survey `src/` for designated initializers that brace-initialize an
  aggregate-typed member without naming its type, followed by a member whose
  initializer can throw. A clang-query or tree-sitter matcher is fine; say
  which, and how false negatives were bounded.
- Name the type at each site that matters, as BPR-0816 did. Keep values,
  order and behaviour unchanged.
- Decide whether a durable guard is worth it: a GCC `-O3` warning build on
  the Linux station, a tidy/format rule, or a note in
  `_coding-standard/STANDARD.md`. Record the decision.

## Acceptance

- The survey's method, and every site it found, are listed here with their
  disposition.
- The committed reproducer passes (no ASan report) under GCC 15.2 on the Linux
  station once the pattern is avoided, and fails with the pattern present.
  It may live under `tests/` as a Linux-only compiler canary if kept.
- The Linux `dev` and `app-release` presets still build stock, and the macOS
  dev build still passes.

## Survey, fixes and guard, 2026-09-17 (bpr-linux, task-20260917-lnx-verify, dsp-001-r2)

The full record, including the ASan reproducer's source, is in
[the survey evidence](../../verification/BPR-0822-linux-gcc-aggregate-cleanup-survey.md).
The change is `de528ce3` on `linux-lane`, pushed.

### Method

The method is GCC-based detection, not grep or clang-query.

- **Why not those.** A grep cannot see an omitted member. Clang's AST carries
  Clang's lowering, which is correct. And `-O3` warned about only BPR-0816's
  two sites.
- **The shape, measured first.** Live-object counts over 48 shapes showed that
  GCC 15.2 double-destroys any multi-element aggregate member with a
  non-trivially destructible subobject when three things hold:
  - it is initialized from braces without naming its type;
  - that includes `{ }` and an omitted member;
  - a later member's initializer throws.
- **Where it holds.** Every aggregate-initialization context, including base
  classes, initializer lists and a braced aggregate inside a constructor's
  member-initializer list.
- **The tool.** `scripts/survey_gcc_aggregate_cleanups.py` first calibrates:
  it compiles and runs 25 embedded shapes and requires its detector to flag
  exactly the 16 that double-destroy. It then recompiles every translation
  unit of the given compile databases with GCC's GIMPLE and EH dumps. A finding
  is a nested pair of exception cleanups that one throw would run twice.

**How false negatives were bounded.**

- **Every GCC-built unit.** All 614 units of both Linux presets: `dev`, with
  `tests/`, and `app-release`, with its `!BPR_BUILD_TESTING` code. That is
  GCC's own lowering, with templates as instantiated. Windows and macOS
  branches never reach GCC.
- **Calibration.** The detector agrees with the run on all 48 exploratory
  shapes. The independent reviewer's roughly 60 more produced no miss.
- **Positive control.** With BPR-0816's fix reverted, the tool flags exactly
  its three sites.
- **Conservative filter.** Unmodelled guards widen the active region; only
  pairs with no exception path are dropped.

**Not by ASan.** ASan cannot find unexecuted exception edges. It was used on
the reproducer only.

### Sites at the base (`c19fa196` source) and dispositions

The base survey found 46 sites: 10 under `src/` and 36 under `tests/`. In each
case only an allocation failure reaches the edge in practice.

**Every `src/` site now names its type.** Values, order and behaviour are
unchanged:

| Base site | Change |
|---|---|
| `src/application/include/bpr/application/profile_registry.hpp:29`, the `Profiles` default | Names `core::bpr_profile_record`. This also covers the same default's uses at `src/adapters/sqlite/src/profile_registry_store.cpp:223` and `:411` and at `src/app/src/github_application.cpp:12487`. |
| `src/app/src/github_application.cpp:8283`, the shared draft edit (both presets) | `core::review_draft_command{ ... }` |
| `src/app/src/github_application.cpp:17881` and `:17885`, the round-trip probe (dev) | `core::review_draft_command{ ... }` |
| `src/application/src/github_application_host.cpp:1006`, the public-preview `Viewer` | `github_viewer_record{ }` |
| `src/application/src/review_mutation_application_host.cpp:798`, `request_record::Anchor` | `review_mutation_anchor_binding{ }` |
| `src/app/src/github_public_path_fixture.cpp:615`, the fixture exchange (dev) | `semantic_read_request{ }`. The enclosing `Request` is also named `wire_request{ ... }` so it does not depend on GCC's lowering. |

**`tests/` sites are left unchanged.** Each builds fixture values, and the
survey lists `tests/` without gating it.

- **Resolved by the header fix:** `tests/application/profile_registry_tests.cpp:207`,
  `tests/application/profile_storage_authority_tests.cpp:23` and `:46`,
  `tests/sqlite_contract/profile_registry_storage_tests.cpp:98` and `:198`,
  and `tests/sqlite_contract/profile_storage_root_tests.cpp:50`.
- **The other 30, left:**
  - `tests/adapters_qt/`: `composed_github_tests.cpp:5957`,
    `review_landing_model_tests.cpp:638` and `:640`,
    `settings_window_tests.cpp:129`, `workbench_window_tests.cpp:103` and
    `:6514` (6464 at the base).
  - `tests/application/`: `agent_invocation_host_tests.cpp:214`,
    `epoch_application_host_tests.cpp:3095`, `github_contract_tests.cpp:217`,
    `profile_registry_tests.cpp:209`, `profile_startup_admission_tests.cpp:28`,
    `profile_storage_authority_tests.cpp:25`,
    `review_mutation_application_host_tests.cpp:223`,
    `review_queue_enrollment_tests.cpp:73`.
  - `tests/core/`: `revisioned_review_draft_tests.cpp:39`.
  - `tests/end_to_end/`: `profile_activation_public_path_tests.cpp:82`,
    `profile_owner_admission_public_path_tests.cpp:82`,
    `profile_startup_public_path_tests.cpp:184`.
  - `tests/github_contract/`: `github_provider_tests.cpp:729` and `:1602`,
    `github_read_contract_tests.cpp:142`, `:237` and `:307`,
    `live_review_submission_provider_tests.cpp:135`.
  - `tests/sqlite_contract/`: `epoch_catalog_behavior_tests.cpp:1842`,
    `profile_storage_root_tests.cpp:52` and `:53`,
    `review_mutation_recovery_storage_tests.cpp:113`,
    `review_submission_recovery_storage_tests.cpp:66`,
    `window_review_restoration_storage_tests.cpp:98`.

### The durable guard: the survey, not a warning, rule or version gate

- **Not a compiler gate.** GCC 15.2 is the Linux toolchain, so a gate would
  stop Linux builds.
- **Not an `-O3` warning build.** It is measured insufficient: it built clean
  with ten `src/` sites present.
- **Not a tidy or clang-query rule.** Clang does not see GCC's lowering.
- **Not a `STANDARD.md` rule.** A style rule cannot cover omitted members.
- **The survey itself.** It is the only method that finds these sites, and
  its calibration keeps it honest across GCC upgrades. It fails on `src/`
  findings, and
  [build-and-test](../../development/build-and-test.md#gcc-15-can-destroy-a-braced-aggregate-member-twice)
  says when to run it: after a GCC change, and on a Linux stock build of a
  base that adds aggregate initializers under `src/`.
- **No separate ctest canary.** The calibration is the reproducer and runs with
  every survey.

### Acceptance, Linux half, at `de528ce3`

- **Criterion 1, the method and every site with its disposition: met.** See
  above.
- **Criterion 2, the reproducer: met.**
  - Under GCC 15.2 the ASan reproducer of both BPR-0816 shapes (source in the
    evidence) reports nothing with the type named. With the pattern present it
    gives `attempting double-free` at `-O0`, and `free(): double free` at `-O2`.
  - The tool's calibration nets 0 for each named shape and −1 for each unnamed
    one.
- **Criterion 3, stock builds: Linux half met.**
  - `dev` built fresh and clean: 1241 of 1241 steps. `app-release`: 429 of
    429. No first-party diagnostic in either.
  - The survey exits 0: 614 units, 30 `tests/` sites, 0 gated.
  - The focused courses for every touched site pass, apart from two. The
    round-trip contract fails only on its BPR-0819 digest pins, which this
    change and BPR-0824 move; with scratch-resealed pins it passes, and the
    reseal is left to main. `application_anonymous_public_preview_public_path`
    has failed since `26fd7f7c` on a stale regex, identically with this change
    reverted.
  - An `rfc-reviewer` found nothing blocking in the code. Its findings were
    applied in `de528ce3`.
  - **The macOS dev build was not run.** The card stays In Progress for main.
