# BPR-1616 verification — 2026-10-18

Product revision: `7c88aed2f74b0d94f16d267255af4380d967fea4`. Origin main was merged before implementation and
rechecked before committing. This repairs the eight deterministic failures and
three tidy findings recorded by the final integrated suite, preserving the
owner-accepted Agent design. Direct execution continues the original handoff;
no agent delegation, live GitHub calls, owner desktop access or stash was used.

## Changes and causes

- `contained_temporary_usage_contract`: profile recovery now allocates its owned
  scratch root through the shared contained-temporary helper, preserving cleanup.
- `qt_composed_github_scale_100`, `_125`, `_200`: replace the retired stacked panel
  assertions with the accepted Agent tab, its borrowed profile/question controls,
  visible finding card and its exact single queue promotion intent. Retain drawer
  collapse/reopen, raster bounds, width sweeps and Comments defect navigation.
- `application_review_submission_composition_public_path`,
  `application_review_submission_composition_scale_125`, `_200`: inspect the actual
  Agent tab and hidden legacy nested pages instead of demanding those retired
  pages remain visible. The later private-note promotion/publication course passes.
- `application_thread_words_public_path`: exempt the exact accepted Agent tooltip
  and the Agent history button's wording. Three negative controls still catch
  review-thread wording and extra offending text, including on the exempt button.
- Tidy: allow automatic move of the note label, remove the redundant tour excerpt
  copy, and copy the optional attachment line without optional-value conversion.

## Focused evidence

The focused four-target Debug build (`-g0`, six jobs) passed:
`app_better_pull_review`, `test_better_pull_review_qt_github_tests`,
`test_better_pull_review_review_submission_composition_public_path`,
`test_setup_destination_public_path`.

Serial CTest selector covering the eight rows above plus
`application_profile_recovery_public_path`: **9/9 passed**, 56.45 seconds.
The contained-temporary usage script also passed all six tests directly.

Clang-tidy 22, warnings as errors, all five changed C++ translation units:
**passed**, including all three originally reported findings. Clang-format 22.1.8
check on all five changed C++ files: **passed**. Documentation gate and its
67 regression tests: **passed**. No `src/app` change; composition pins are unchanged.
No second full integrated suite was requested or run for this focused repair.

Local raw evidence (not committed): `/tmp/bpr1616-build.log`,
`/tmp/bpr1616-tests.log`, `/tmp/bpr1616-tidy.log`, `/tmp/bpr1616-docs.log`,
`/tmp/bpr1616-doc-tests.log`. The task-owned `build/dev` tree is removed after checks;
the existing shared dependency cache is retained.

## Workflow state

The direct strategy snapshot is preserved beside this record. MASK readiness
reports `state.drift`, with host load not verified; it launched no delegated role.
This standalone repair has no authored milestone plan, so the observational
milestone binder reports that no authored plan is available. Card acceptance is
bound directly to the product revision and focused evidence above.
