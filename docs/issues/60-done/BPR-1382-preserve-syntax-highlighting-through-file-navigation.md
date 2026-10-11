---
schema: "implement-rfc.card.v1"
id: "BPR-1382"
title: "Preserve syntax highlighting through file navigation"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"bpr-1382-syntax-navigation","path":"docs/rfcs/plans/bpr-1382-syntax-navigation/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c783b232eff9054ebb4445b400e3eead6c69b919","evidence":{"kind":"observation","detail":"The offscreen composed large-review-v1 course reproduces both plain panes on the first Next/Previous return to large/comparison.cpp after all work settles. The previous Current diff rejects synchronous cached spans; deferred document adoption misses the publication fallback."}},{"criterion":"AC2","outcome":"pass","revision":"c783b232eff9054ebb4445b400e3eead6c69b919","evidence":{"kind":"observation","detail":"Delivery matches the existing exact surface context. Seven transitions check manifest blob identity, both panes rendered colours, identical cached span objects and no new classification submissions; four large publications exercise staging. Focused host, syntax, Qt, architecture and composition checks pass."}},{"criterion":"AC3","outcome":"pass","revision":"c783b232eff9054ebb4445b400e3eead6c69b919","evidence":{"kind":"command","detail":"application_large_review_prepared_open fails in 13.00 seconds when only the old host-context choice is restored; after restoring and rebuilding the fix, the same course passes in 14.22 seconds. Inspected offscreen failure and success renders confirm plain versus coloured source on both sides."}},{"criterion":"AC4","outcome":"pass","revision":"c783b232eff9054ebb4445b400e3eead6c69b919","evidence":{"kind":"command","detail":"Eight focused dev tests, the round-trip composition contract and the targeted inventory mutation test pass. Both C++ files pass clang-format 22.1.8; git diff --check and DOCUMENTATION_OK pass; documentation verifier has 67 passing tests. The card records the canceled optional full Release-build check and limits of offscreen fixture evidence. Developer application rebuilt; owner installation untouched."}}]
---

# BPR-1382: Preserve syntax highlighting through file navigation

The owner observed syntax highlighting disappearing intermittently in the live
application, possibly after repeated Next/Previous file navigation, and asked
for direct debugging in the main checkout on 2026-10-10.

Baseline: `bee3f0dad`. Direct execution; no delegated writers. The unrelated
untracked `docs/proposals/` belongs to other work.

## Acceptance

- AC1: Reproduce and identify a concrete path that leaves a supported source
  file without its syntax classification after navigation.
- AC2: Repair that path while preserving exact source identity, bounded work,
  stale-result rejection and cached classifications.
- AC3: Add a regression that fails before the repair and passes after it;
  verify the composed application path with fixture data and offscreen output.
- AC4: Run focused existing checks and the documentation gate, and record
  evidence and the limits of reproducing the owner's live observation.

## Plan

Trace selection, classification cancellation and cached-result adoption; build
a deterministic regression; repair the proven failure; verify and commit this
slice. This does not reopen the RFC's existing palette or native acceptance.

## Diagnosis and correction

The owner clarified that the code became plain/uncoloured. The recorded
`large-review-v1` fixture reproduces that through the actual Next/Previous
actions: open `large/comparison.cpp`, open `src/file_0000.cpp`, then return to
the large file. All classification and diff work has finished when the check
fails, so waiting longer cannot recover it.

`diff_state::Current` deliberately retains the previous accepted result while
`Pending` names the new request. `adopt_classification` preferred `Current`, so
an immediate syntax-cache hit for the new file was discarded as belonging to
neither old blob. The display-time cache fallback repaired small synchronous
publications. Over 4,096 total source lines, Qt stages the replacement text
across turns; the fallback runs before `current_result()` exists and returns.
The later document adoption has no second host publication, leaving both panes
uncoloured.

Adoption now uses the surface's exact `current_context()`, which is installed
before classification is requested and remains bound throughout staging.
Matching cached spans survive until the text arrives; a departed context still
clears them and foreign blob publications remain rejected. No lexical parser,
palette, effect, request policy or Qt document staging rule changes.

## Verification

The existing prepared-open course now also follows seven real Next/Previous
actions between the small and 20,000-line C++ files. Each step checks exact
manifest blob IDs, nonempty spans on both sides and distinct foreground colours
in the actual first-line text layouts. Each large revisit must reuse the same
immutable span objects, with no new classification submission. Four staged
publications must occur.

- Before the repair, the course failed on the first large-file revisit.
- With the final regression in place, restoring just the old host-context
  choice failed again after 13.00 seconds with
  `file navigation left a source pane without syntax highlighting`.
  Its full-window offscreen render shows both panes plain. The fixed block was
  restored and rebuilt before the passing run.
- The focused dev selector passed 8/8: `architecture_boundaries`,
  `application_appearance_host`, `application_appearance_profile_consumers`,
  `qt_syntax_pane_composition`, `qt_diff_workbench_scale_100`,
  `syntax_classification_contract`, `application_large_review_prepared_open`
  and `application_workbench_composition_contract`. The regression took
  14.22 seconds; its offscreen render shows both panes coloured.
- `application_review_round_trip_composition_contract` passed after the
  reviewed reseal below.
- The composition inventory's direct-bypass mutation test passed against the
  final pins (1 test, 1.602 seconds). An optional broader
  `application_composition_contract` run saw the old pin before resealing;
  its separate fresh tests-disabled Release build was deliberately stopped.
  That broader run is not passing evidence; the final inventory and focused
  developer-build checks above are the completed scope.
- Clang-format 22.1.8 passed for both touched C++ files; `git diff --check`
  passed. `DOCUMENTATION_OK` and the documentation verifier's 67 tests passed.
- The developer application and focused test executables were rebuilt.

Machine evidence: macOS arm64, Qt 6.11.2, Debug, offscreen fixture application
with the installed Git/cache actor. These observations reproduce the reported
symptom; they do not inspect, replace or prove the state of the owner's running
installation. No live GitHub request or owner desktop capture was used. The
work corrects delivery of existing syntax roles, with no visual design change.

## Scoped self-review and composition pins

Reviewed the complete changed adoption function against loading, immediate
cache delivery, staged publication, clearing and foreign-blob rejection.
The implementation uses an existing exact presentation context and adds no
state, scan, effect or retry. The regression checks rendered output as well as
cache presence and was falsified by restoring the original decision.

The prior full application pin matched baseline `bee3f0dad`. The prior manifest
pin still matched `611abc31c`; its only intervening member change is
`636c885c2`'s tests-on mouse helper rename in `main.cpp`, from `deliver` to
`send_mouse_event`, with identical parameters, events and calls. That rename
was reviewed here together with this slice's adoption change. Independent
Python and CMake digest calculations agreed on full application
`bb5c510ed85e4a4cd2c90e6889fc830252e4031e164b9384801b790876d1a2c3`
and manifest
`52dfcf7d3b392e18125fce5e86530a3184d0f678ebdb825f1f8bda86fc4f9be7`.
The production mutation pin and source inventory remain unchanged. This is
scoped self-review, not an independent reviewer claim.

The architecture's syntax-delivery paragraph and RFC-0006 verification ledger
record the correction. Provider/authentication, publication, persistence,
packaging and appearance settings are no-impact: this changes only which
already-classified immutable spans reach the current comparison.
