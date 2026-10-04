---
schema: "implement-rfc.card.v1"
id: "BPR-1171"
title: "Verify desktop/CLI/external-consumer parity"
status: "Done"
rfcs: ["RFC-BPR-0058"]
depends_on: ["BPR-1166", "BPR-1170"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "external-consumers", "acceptance": ["RC58-03", "RC58-05", "RC58-06", "RC58-07", "RC58-10"], "pack_alias": "PACK-CYCLE-29", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC58-03","outcome":"pass","revision":"fd760e33f","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64, application_review_cycle_parity_public_path (network-deny sandbox, offscreen): every terminal, harness and window result names RC-1, F1 and its payload_sha256; with the window moved to RC-2 the harness's folder-resolved round read is RC-1's F1; F2 without a grant is not_granted; a consumer cannot freeze. Another interface's write no longer moves the window: the harness's append leaves it on RC-2 and Home stays in front while the terminal writes the cycle behind it (fix in review_cycle_desktop/workbench_window; control: page always brought forward fails the course)."}},{"criterion":"RC58-05","outcome":"pass","revision":"fd760e33f","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Same course: while the desktop owns the profile its lease cannot be taken (POSIX) and every terminal and harness command is served by the owner; after it quits the same twelve commands (human and JSON show, every 4 KiB page, default and human reads, source list, S1 whole, range and in words, requires_partition, human not_granted, consumer freeze refusal) give equal answers run alone, and export --output - the same bytes. Control: an owner answer differing only in source_bytes fails 'parity: pages 4096 differs'."}},{"criterion":"RC58-06","outcome":"pass","revision":"fd760e33f","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Same course: the window's unsaved note text, focus and editor survive a concurrent terminal note and JSON read, then save; terminal --context and --exclude edits reach the open sheet; a stale --include names the current revision; a typed-not-entered goal is saved before the terminal's freeze (stale_revision), the next freeze carries it; a draft the desktop left open and the terminal changed alone is restored on the next launch (revision, goal, exclusion); the harness's response shows in the open window. Control: a reload that drops the note being written fails the course."}},{"criterion":"RC58-07","outcome":"pass","revision":"fd760e33f","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Same course: the harness reads F1 at 4 KiB pages (two or more), each naming the digest and continuing where the last ended, together equal to the golden round.json items exactly (anchors holding the dragged bytes at R1, the exact suggestion's CRLF whole-line bytes); S1 equals the golden source and lines 4-9 of the CRLF file; --budget-bytes 64 is requires_partition; identical with the desktop open and closed."}},{"criterion":"RC58-10","outcome":"pass","revision":"fd760e33f","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Manual: a recipient whose PATH lacks bpr (FileNotFoundError) and who has no grant for F2 works from RC-1-F2.md alone (its canonical digest, every F2 item heading and words, equal to the terminal's export), replies in prose; the reviewer pastes it, Capture revision R3, This round shows the edit, I4 verified; control: Markdown omitting its last item fails. Observed harness: BPR-1170's Claude Code course (a8fb079aa) reused; package bytes unchanged since dc503b791 and consumer command code unchanged; this course shows consumer answers are the same with the desktop open. No new model session."}},{"criterion":"BPR-1171:boundary","outcome":"pass","revision":"fd760e33f","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS 26.6.2 arm64, Apple clang 21, Qt 6.11.2, isolated worktree build/dev-tour-cli: integration pass 66/66 at -j6 (parity, two-round, desktop path, both handoff courses, cycle CLI, skill package, review_cycle_/qt_review_cycle_view_/core_review_cycle_ rows, sqlite storage and owner channel, tour_cli_ rows, qt_workbench_window_scale_*, network isolation rows, architecture_boundaries/_contract, round-trip pin, documentation_structure); parity row 6/6 repeats under -j4 load; four product negative controls fail; clang-format clean; DOCUMENTATION_OK. Linux and Windows runs owed, not gating."}}]
---

Pack alias: **PACK-CYCLE-29** (live `BPR-1171`, allocated at import).

# BPR-1171: Verify desktop/CLI/external-consumer parity

## Outcome and bounded work

Run a common cycle through desktop -> human CLI -> external consumer -> manual capture and back. Verify same frozen payload/IDs, complete reads, source bindings, restored drafts and responses when desktop is open. Include remote/unreachable harness fallback.

## Acceptance and direct evidence

Cross-interface golden payloads, empty/no-JSON human terminal, concurrent focus/draft edits, supported actual harness observation and ordinary file/manual course. Tool reads are not reported as human acceptance.

Criteria: RC58-03, RC58-05, RC58-06, RC58-07, RC58-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Implemented: macOS, 2026-10-03

Promoted to Ready at `c44bba969` under the 2026-10-03 mandate amendment and
claimed at `736f89a4e`. The course is
`application_review_cycle_parity_public_path` (`fd760e33f`). The full record,
with the course, commands, controls and limits, is in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md) under "BPR-1171:
one cycle through every interface, and back".

Owned paths:

- `tests/end_to_end/review_cycle_parity_contract.py` and its row in
  `tests/end_to_end/CMakeLists.txt`;
- the reviewer gestures added to BPR-1166's
  `tests/end_to_end/review_cycle_two_round_public_path_tests.cpp`;
- the two fixes below;
- a paragraph of the [developer page](../../development/cycle-cli.md).

No `src/app` file was added; the round-trip composition pin passes unchanged.

One cycle runs through four interfaces and back:

- the application's own profile desktop;
- the shipped `bpr cycle` without `--json`, both forwarded to the open
  desktop and run alone;
- a declared harness running the bpr-cycle skill's lines;
- a recipient with no `bpr` and no grant, working from the Markdown.

Every interface carries one golden payload, the bundle's canonical
`round.json`. Twelve terminal and harness answers are equal with the desktop
open and closed. The window keeps its unsaved note, its keyboard and Home
while other interfaces write. A draft the terminal changed alone is restored
on the next launch. The harness's response shows in the window as a claim. The
remote recipient's prose reply is pasted, captured and verified. Four product
negative controls fail the course.

Two defects were fixed at their source. Any projection after another
interface's write (or any background step) brought the cycle page forward over
Home or a pull request; now only the reviewer's own Open Project or resume does
(`review_cycle_desktop`, `workbench_window::retitle_local_cycle`). A stale
`review` or `freeze` told a person to run `bpr cycle review RC-n --json`; it
now gives the plain command (`cycle_commands.cpp`).

RC58-10's observed harness half reuses BPR-1170's Claude Code course, whose
package and consumer code are unchanged. This course shows the consumer
answers are the same with the desktop open. No new model session was started,
so no owner authorization was needed and no attended session was opened.

The integration pass is 66 of 66 at the candidate. Recorded, not changed: the
notes list shows a suggestion line's own CR, which belongs to BPR-1182's look.

### Owed platform runs at `fd760e33f`

These do not gate the card. The commands are in the ledger section, for
`bpr-main-linux` and for `bpr-main-win` (Debug). The new row is registered
contained. If Windows stops it inside the container, as it stops the sibling
handoff course, pin it `WINDOWS_UNCONTAINED` with the measured line and record
it here as a note.

**Note: Linux run at `816b68f3b` (station `bpr-main-linux`, WSL2,
`build/dev-tour-cli` with `BPR_BUILD_TOUR_CLI=ON`).** Configure and build
succeeded and all 7 tests passed: `application_review_cycle_parity_public_path`
(40.7 s), `application_review_cycle_two_round_public_path`,
`application_review_cycle_desktop_public_path`,
`application_profile_cycle_handoff_public_path`, `cycle_cli_public_path`,
`test_network_isolation_manifest` and `architecture_boundaries`. Everything ran
contained with nothing pinned. Done is not reopened.

### Windows at `8fab05419` (bpr-main-win, 2026-10-04)

Windows 11 Home 10.0.26200, MSVC 14.51, Qt 6.11.2, `build/windows-dev-tour-cli`
Debug (`BPR_BUILD_TOUR_CLI=ON`). The tree first failed to build: BPR-1177's
`review_cycle_publish_public_path_tests.cpp` called `std::getenv`, which MSVC
fails as C4996. It was fixed in its own BPR-1177 commit, after which the build
printed no `warning C` or `warning LNK` line.

**As registered, 5 of 7.**

- `application_review_cycle_parity_public_path` failed contained in 0.31 s.
  Its first `bpr cycle list` exits 7 with "The application root is unavailable
  or is not owner-private." That is the `profile_unavailable` refusal
  `cycle_cli_public_path` measures (`dos-volume-name`). The row is now
  `WINDOWS_UNCONTAINED` and pinned with that line in
  `scripts/verify_test_network_isolation.py`.
- Like the two-round course ([[BPR-1166]]), it passed Qt
  `offscreen:configfile=C:\…`, which Qt splits at the drive letter's `:`. It
  now passes the file relative to the working directory the desktop inherits.
  The tests tree has no other `configfile`.
- `application_profile_cycle_handoff_public_path` ([[BPR-1168]]'s course),
  which passed three times earlier that night, failed after 123 s: "missing
  NOTES". The course handed the desktop the same `w+` file object it seeks and
  reads. On Windows the inherited handle shares one file pointer, so a desktop
  write landing between the course's `seek(0)` and `read()` went to offset 0.
  The log shows this: `NOTES 2|…` sits over the start of
  `OWNER_ENDPOINT \.\pipe\…`, with `\pipe\bpr-tourcmd--…` left behind it,
  below a reader already past that line. The desktop now appends through its
  own descriptor, and the course reads complete lines from the path, as the
  two-round course does.

**After the pin and the two test fixes: 7/7 in 82.49 s.** The parity course
passes uncontained in 82.43 s and the handoff course in 29.49 s.
`test_network_isolation_manifest`, `architecture_boundaries` and
`coding-format-check` (1215 files) pass. Only test files changed. Done is not
reopened.
