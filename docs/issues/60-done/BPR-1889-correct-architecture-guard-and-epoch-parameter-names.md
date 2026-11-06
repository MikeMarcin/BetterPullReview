---
schema: "implement-rfc.card.v1"
id: "BPR-1889"
title: "Correct architecture guard and epoch parameter names"
status: "Done"
rfcs: []
depends_on: ["BPR-1888"]
priority: "P1"
metadata: {"kind":"maintenance","reservation":"docs/issues/_reservations/BPR-1889--BPR-1889--426a20fdc16c.json","milestone_plan":{"id":"responsiveness-latency-v6","path":"docs/rfcs/plans/responsiveness-latency-v6/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c544dd6eb29be7f7a9f5e6849f486ca0b6f4d4df","evidence":{"kind":"observation","detail":"Managed-clone paths use the existing transport helper with identical path composition. The real architecture gate and all 163 contract tests pass, including hostile checks retaining the exact owner and managed-clone restrictions; no production guard was loosened. The local Git smart-transport final course passes."}},{"criterion":"AC2","outcome":"pass","revision":"c544dd6eb29be7f7a9f5e6849f486ca0b6f4d4df","evidence":{"kind":"observation","detail":"The synthetic runner and transport fixtures now represent the existing managed-clone factory/deadline and depth-plus-filter policies. The full architecture contract passes all 163 tests, with positive coverage and new mutation refusals."}},{"criterion":"AC3","outcome":"pass","revision":"c544dd6eb29be7f7a9f5e6849f486ca0b6f4d4df","evidence":{"kind":"observation","detail":"Only three materialize_on_actor parameter identifiers and their uses changed. Inverse renaming produces the exact original function bytes. Full epoch-host clang-tidy 22 with warnings as errors and the application_epoch_host functional course pass; both other changed C++ translation units also pass tidy."}},{"criterion":"AC4","outcome":"pass","revision":"c544dd6eb29be7f7a9f5e6849f486ca0b6f4d4df","evidence":{"kind":"observation","detail":"Documentation and format checks pass. The one-line journal entry, implementation, full verification evidence and retained initial native failure are committed and landed on main as c544dd6eb29be7f7a9f5e6849f486ca0b6f4d4df. The baseline and final native Git comparison both pass; the earlier failure remains unconfirmed and no unrelated fix or repository-wide green claim is made."}}]
---

# BPR-1889: Correct architecture guard and epoch parameter names

After BPR-1888's latency verification reported the inherited failures, the owner
asked what the architecture error and three naming warnings were and requested
that they be fixed next. This round preserves runtime behavior and restores the
relevant checks rather than carrying these known failures forward.

## Acceptance

- AC1: Keep managed-clone Git-directory path construction in the existing
  transport helper; the real architecture gate passes and focused
  positive/hostile controls preserve its ownership restriction.
- AC2: Update the architecture contract's stale synthetic runner fixture to
  include the existing managed-clone factory/deadline policy, keeping the
  production guard and its failure controls meaningful; the contract suite passes.
- AC3: Rename the three nonconforming `materialize_on_actor` parameters without
  changing behavior; the full epoch-host clang-tidy 22 check and focused host
  functional course pass.
- AC4: Record checks and any remaining inherited gaps, add a one-line journal
  entry, complete the card and land the round on main.

## Implementation

The architecture diagnostic was triggered by three literal `.git` paths in
BPR-1884's managed-clone refresh, rather than by neutral-working-directory
validation. The transport already owns `gh_cli_git_kept_clone_directory` for new
clones. An overload takes the kept working folder itself, and all three refresh
paths now use it; the original root/index overload delegates to the same helper.
The paths, validation, pack operations and clone policy are unchanged. No
architecture allowlist or enforcement rule is loosened.

The synthetic architecture runner still predated BPR-1879's managed-clone factory
and deadline branch. After restoring those, its transport fixture also needed
BPR-1873's depth-plus-blob-filter construction to reach the later checks. The
fixtures now include both existing production policies. New hostile controls
reject changes to the managed factory's limits, Git executable, marker and
conditional deadline, reject depth without its filter or in another owner, and
retain rejection of a `.git` marker in the smart-transport source itself.

`materialize_on_actor` now names its parameters `aActor`, `aAuthorization` and
`aExpected`. Inverting only these three identifier renames yields the exact
original function bytes; captures, authority checks, cache calls and lifetime
handling are preserved. No runtime policy or presentation changes.

## Evidence

Native macOS arm64, AppleClang 21, Qt 6.12, optimized `qualification-release`:

- The focused epoch-host and Git smart-transport targets build successfully.
- `architecture_boundaries` passes without changing its enforcement rules.
  `architecture_boundary_contract` passes all 163 tests, including the new
  hostile controls. The first suite run caught a line-wrap-sensitive substring
  in one new diagnostic assertion; the corrected assertion and final full suite
  pass.
- `application_epoch_host` passes. Full-file clang-tidy 22 with
  `--warnings-as-errors=*` passes for `epoch_application_host.cpp`,
  `gh_cli_git_smart_transport.cpp` and `gh_cli_transport.cpp`; the three named
  parameter warnings are gone. The inverse-rename equivalence check passes.
- `gh_cli_git_smart_transport` passes against both unchanged GitHub sources at
  `3e72284b6279ab0719b45394afc15eab2aced9d1` and the restored final changes.
  This exercises managed-clone creation, failed/cancelled refresh, refusal of
  linked clones, exact received objects, private pack installation, reference
  updates and the already-current path, using local Git and fake gh only.
  The first run stopped earlier in workspace delta acquisition with
  `response_limit` at `clone` (failure 11, step 9), before the changed managed
  clone paths. Neither comparison run reproduced it. Its cause is unconfirmed;
  no fix for that intermittent failure is claimed, and the failed run is retained.
- Changed-file clang-format 22, whitespace and the documentation gate pass.
  No UI, live GitHub, Debug timing or remote-push work was needed.

Raw command logs, the baseline/final file hashes and command outcomes, and the
inverse-rename proof are external in
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-responsiveness-vvxyjjsj`
as `gate-cleanup-*`. BPR-1888's separate network-isolation manifest gap and
whole-workbench Qt lint warnings remain outside this card; this is focused
acceptance, not a claim that every repository check passes.

The verified source and journal entry landed on local main as
`c544dd6eb29be7f7a9f5e6849f486ca0b6f4d4df`; card closeout follows in its own commit.
