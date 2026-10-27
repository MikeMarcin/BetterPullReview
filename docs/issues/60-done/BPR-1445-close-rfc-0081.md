---
schema: "implement-rfc.card.v1"
id: "BPR-1445"
title: "Verify and close RFC-BPR-0081"
status: "Done"
rfcs: ["RFC-BPR-0081"]
depends_on: ["BPR-1440", "BPR-1441", "BPR-1442"]
priority: "P2"
platforms: ["mac", "win", "linux"]
closeout_for: ["RFC-BPR-0081"]
metadata: {"mandate_revision":"cpp-identifier-parser-evaluation-r1","milestone_plan":{"id":"cpp-identifier-parser-evaluation-v1","path":"docs/rfcs/plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json"}}
acceptance: [{"criterion":"BPR-1445:AC1","outcome":"pass","revision":"223098b11","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1445-close-rfc-0081.md","detail":"AC01-AC07 each have direct evidence: macOS evaluation at 02da27cbd (BPR-1440/1441/1442/1474) plus 2026-10-27 current-source rechecks (option-on scratch build and link check, option-off link checks on build/dev and core-only, v0.3.4 SPDX, base and twin cancellation/contract rows, core-only ctest 329/329 accounted (327 pass in the first run; documentation_structure and release_source_assets_contract failed while another session's bundled-CLI edits were uncommitted and both pass on rerun at 223098b11), v0.3.4 native boundary and syntax rows on macOS/Windows/Ubuntu). AC03/AC04/AC06 as amended 2026-10-26."}},{"criterion":"BPR-1445:AC2","outcome":"pass","revision":"223098b11","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1445-close-rfc-0081.md","detail":"Outcome extend follows mechanically (T fails Contract, Bounded work, Memory at both pins); BPR-1474 cites the report; RFC front matter, index row and this card agree on Auto-Verified. Three-platform measurement amended to macOS by the owner on 2026-10-26. The fired 800-line re-run trigger is carried to BPR-1777."}}]
---

# BPR-1445: Verify and close RFC-BPR-0081

Mandate `cpp-identifier-parser-evaluation-r1` authorizes the evaluation in
[RFC-BPR-0081](../../rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md), adopted from RFC-DRAFT-0021.

RFC base: `530d92eaff1578413c0e669f476e740387441152`.

## Acceptance

- **AC1:** Every RFC criterion AC01 to AC07 has direct evidence on the exact
  candidate, reconciled against RFC_BASE_SHA, and the closeout checklist below
  agrees with it.
- **AC2:** The recorded outcome follows mechanically from the decision rules
  with all three platforms measured, and any follow-up card it requires cites the
  report. The RFC, its index row and this card agree. *(Amended by the owner's 2026-10-26
  [RFC amendment](../../rfcs/assets/cpp-identifier-parser-evaluation-v1/proposed-owner-amendment-2026-10-26.md):
  the measurements are macOS-only.)*

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** With the evaluation option on, the harness runs the baseline and each T pin over the whole corpus through the unchanged public classifier path. The committed report shows whether each gate passed, with its commands, counts and raw results. With the option off, no target acquires or links tree-sitter, as the configure log and a link-dependency check show. | Pass on macOS, `02da27cbd`: the harness ran the baseline and both pins over all 5,270 files through the unchanged classifier; the report shows each gate with commands, counts and raw results; option off: configure log and link check clean (BPR-1442). Recheck 2026-10-27 on current source (`223098b11`; v0.3.4 suite source `62bfc09f6` differs only in two unrelated test scripts): a scratch Release tree with the option on still configures and builds `bench_syntax_evaluation_v0_23_4`, both v0.23.4 twins and `bpr_syntax_candidate_master` after the BPR-1619 core move; `check_link_dependencies.py` on it is ok (17 tree-sitter link edges, all under `tests/syntax_evaluation`; adapter and core archives define no tree-sitter symbol). Option off: the same check is ok on `build/dev` and on a fresh `core-only` tree (no stage, no edge), and the v0.3.4 macOS, Windows and Ubuntu SPDX documents name no tree-sitter. |
| **AC02:** The labels are committed before any candidate results; third-party sources are named by digest. For every run, the report gives false positives and false negatives per category, and marks each of H1–H8 as confirmed or refuted on the baseline and on T. | Pass: labels committed in `309375c5a` before any candidate corpus run; third-party sources named by digest; per-category FP/FN for every run; H1-H8 marked for baseline and both pins (BPR-1440, BPR-1442). No recheck needed: labels and corpus digests are committed history and unchanged. |
| **AC03:** *Amended 2026-10-26.* The macOS measurements cover every size class and every item under "Measurements", and they measure the baseline in the same runs. Windows x64 and Linux x64 are not measured for this evaluation. | Pass as amended 2026-10-26 (macOS only): macOS covers every size class and item with the baseline in the same runs (BPR-1442). Windows x64 and Linux x64 are not measured, by the owner's amendment; BPR-1443 and BPR-1444 closed unrun. Amendment record: [owner amendment 2026-10-26](../../rfcs/assets/cpp-identifier-parser-evaluation-v1/proposed-owner-amendment-2026-10-26.md). |
| **AC04:** *Amended 2026-10-26.* A test stops the refinement partway through and observes `cancelled` with no spans, and the syntax fuzz target runs in the fuzz build. Both remain after the evaluation. The report gives sanitizer, fuzz, deep-nesting and 8 MiB results for the baseline and for T; the macOS 4 CPU-hour campaigns satisfy the fuzz results, and the Linux campaign is not run. | macOS pass: `syntax_refinement_cancellation` and `fuzz_syntax` stay (BPR-1441); sanitizer, 4 CPU-hour fuzz, deep-nesting and 8 MiB results for the baseline and T are in the report. The Linux fuzz campaign is not run, by the owner's 2026-10-26 amendment. Recheck 2026-10-27: `syntax_refinement_cancellation` passes in the scratch option-on tree and in v0.3.4's macOS Debug, Windows Release/Debug and Ubuntu Release/Debug suites; its T twin `syntax_refinement_cancellation@ts-v0.23.4` passes; `fuzz_syntax` remains registered under `BPR_BUILD_FUZZERS`. |
| **AC05:** The recorded outcome (adopt, extend, keep, or eligible pending Q1) follows mechanically from "Decision rules" and states when to run the evaluation again. Any deviation is recorded as an owner decision. | Pass: T fails Contract, Bounded work and Memory at both pins, so it is rejected and the outcome is **extend** (BPR-1474), with the re-run conditions stated in the report. No threshold was changed. The Bounded-work interpretation is recorded. Recheck 2026-10-27: unchanged. The re-run condition "if the refinement reaches 800 lines" has since fired through out-of-scope cards (847 lines); this fails no criterion and is carried to the owner on BPR-1777. On current source the contract twin `syntax_classification_contract@ts-v0.23.4` still fails (first on BPR-1710's `std::` assertion), consistent with T's rejection. |
| **AC06:** *Amended 2026-10-26.* The report records the licence of each shipped file (runtime, generated parser and scanner) and the notice, SPDX and corresponding-source changes adoption would need. It also records the archive digests, the size added per macOS architecture slice and the build cost; Windows and Linux sizes are not measured. | macOS pass: licences, notice/SPDX/corresponding-source needs, archive digests, size added per macOS slice and build cost are in the report. Windows and Linux executable sizes are not measured, by the owner's 2026-10-26 amendment. No recheck needed: the licence, notice, digest, size and build-cost records are committed history. |
| **AC07:** On the shipping configuration, the evaluation work leaves spans byte-identical over the corpus compared with the revision before it, and `core-only` and both architecture-boundary checks pass. | Pass: `bench_syntax_span_dump` at `530d92eaf` and on the branch gives byte-identical results for all 5,268 C++ corpus files; `core-only` and both architecture-boundary checks pass, apart from unrelated rows (BPR-1442). Recheck 2026-10-27: byte-identity is a property of the evaluation commit and stays as recorded (later span changes came from BPR-1474's intended fixes and BPR-1710/1711/1714). `core-only` configures, builds and its ctest gives 329/329 accounted (327 pass in the first run; documentation_structure and release_source_assets_contract failed while another session's bundled-CLI edits were uncommitted and both pass on rerun at 223098b11) on current source; `architecture_boundaries`, `architecture_boundary_contract`, `core_boundary_contract`, `core_dependency_boundary` and `core_computation_boundaries` pass in v0.3.4's macOS, Windows and Ubuntu suites. |

## Current frontier

The milestones `evaluation-ground` and `macos-evaluation` are done (BPR-1440,
BPR-1441 and BPR-1442 are Done). The outcome is extend, and BPR-1474 carries the
heuristic fixes. What remains is BPR-1443 (Windows x64) and BPR-1444 (Linux x64
and its fuzz campaign), both Ready for their stations. When they land, close
AC03, AC04 and AC06 and confirm the RFC as Auto-Verified. Q1 is unanswered and
moot.

## Owner amendment, 2026-10-26

The owner accepted RFC-BPR-0081's [2026-10-26 amendment](../../rfcs/assets/cpp-identifier-parser-evaluation-v1/proposed-owner-amendment-2026-10-26.md). AC03, AC04
and AC06 are macOS-only for this evaluation, and [[BPR-1443]] and
[[BPR-1444]] are closed without running. Every dependency of this card is
now Done. The checklist above has the amended rows, and closeout still has
to recheck AC01 to AC07 against current source and run the documentation
gate.

## Closeout, 2026-10-27

Every criterion, AC01 to AC07, is rechecked against current source (`223098b11`) above,
on top of the macOS evaluation evidence at `02da27cbd`. RFC-BPR-0081 is
Auto-Verified, and the RFC and its index row now say so. The outcome is
**extend**. The follow-ups BPR-1474 required are Done.

One finding is carried forward rather than hidden. The report's re-run
condition "if the refinement reaches 800 lines" has fired:
`src/core/src/cpp_identifier_context.cpp` is 847 lines. The RFC's own work
left it at 698 (`4add47f63`). Later out-of-scope cards took it to 776
([[BPR-1710]]), 788 ([[BPR-1711]]) and 847 ([[BPR-1714]]). No criterion fails
because of that. [[BPR-1777]] asks the owner whether to re-run the
evaluation on all three platforms or change the trigger.

Limit: another session had uncommitted edits in the shared checkout during
these builds (macOS release presets, the tour CLI and release scripts). None
of them touches the syntax, core or evaluation targets checked here.
