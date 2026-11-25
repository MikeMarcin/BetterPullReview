---
schema: "implement-rfc.card.v1"
id: "BPR-2386"
title: "architecture_boundaries refuses the workspace move's checkout and diff"
status: "Done"
rfcs: ["RFC-BPR-0096"]
depends_on: ["BPR-2379"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"architecture_boundaries failing on main at 2e85a0a97","found_by":"BPR-2379","since":"575a992ea","reservation":"docs/issues/_reservations/BPR-2386--BPR-2386--21db3e7f88bb.json","revision":"2f153e7cb","related":["BPR-2379"],"acceptance":["BPR-2386:carve-out","BPR-2386:still-forbidden","BPR-2386:hardened-arguments"]}
---

# BPR-2386: architecture_boundaries refuses the workspace move's checkout and diff

## Report

On `main` at `2e85a0a97`, `architecture_boundaries` (dev and core-only)
fails with one violation, "Forbidden installed-Git process surface", on the
`"checkout"` and `"diff"` literals in
`src/adapters/git/src/installed_git_process.cpp`.

## Cause

`cmake/VerifyArchitectureBoundaries.cmake` forbids the quoted Git verbs
`fetch-pack|upload-pack|unpack-objects|clone|remote|submodule|checkout|merge|diff`
anywhere in the installed-Git runner. The runner was a store reader and
workspace seeder; it never needed a working-tree checkout or a porcelain diff.

[[BPR-2379]] (RFC-BPR-0096 continue-at-latest amendment, `575a992ea`) moves a
retained agent workspace to the latest head. The amendment's step 6 names the
commands: check the new head out detached, `rebase --onto <new> <old>`, and
on a conflict collect `diff --name-only --diff-filter=U`, abort, and check the
pre-move ref out. So the runner now spells `"checkout"` and `"diff"` four
times, every one as `workspace_git(<path>, { "<verb>", ... })`.

## Decision

The commands belong on the approved surface, but only in the retained
workspace. No existing helper does the same job: `read-tree`/`checkout-index`
would reimplement `checkout --detach` and the rebase's own restore. Every
call already goes through `workspace_git`, which builds on
`workspace_git_arguments(aWorkspace, HooksRoot)`: `-C <workspace>`, the private
hooks path, every protocol refused, `diff.external=` cleared, submodule
recursion and LFS filters off. `rebase`, `reset`, `add` and `commit`, which
the move also runs, were never on the forbidden list.

## Fix

- `cmake/VerifyArchitectureBoundaries.cmake`: before the forbidden scan, the
  rule removes `workspace_git(<identifier>, { "checkout"` and `... { "diff"`
  from a scanned copy of the runner. If it removed any, `workspace_git`
  must still begin with
  `auto arguments = workspace_git_arguments(aWorkspace, HooksRoot);`. If it
  does not, the rule fails with "the workspace move's checkout and diff run
  only through workspace_git ...". Every other forbidden verb, and
  `checkout`/`diff` anywhere else, is still refused. `merge`, `clone`,
  `remote` and the rest are not carved out.
- `tests/scripts/architecture_boundary_tests.py`:
  `test_workspace_move_checkout_and_diff_stay_in_the_workspace` checks that
  the real runner passes, and that an added `workspace_git(aWorkspace, { "diff"`
  passes too. It checks that a bare `"checkout"`, `run(aContext, { "diff"`,
  `workspace_git(path, { "merge"` and a verb after leading `-c` options in
  `workspace_git` are refused, and that `workspace_git` built on other
  arguments fails the hardened-arguments check.

No product source changes.

## Acceptance

- `BPR-2386:carve-out`: `architecture_boundaries` passes on the runner as
  BPR-2379 left it.
- `BPR-2386:still-forbidden`: `checkout`/`diff` outside a leading
  `workspace_git` argument, and `merge` inside one, are refused.
- `BPR-2386:hardened-arguments`: the carve-out holds only while
  `workspace_git` builds on `workspace_git_arguments(aWorkspace, HooksRoot)`.

## Evidence

At 2f153e7cb (candidate 2f153e7cb on main 2e85a0a97), macOS arm64, Debug:

- `ctest --preset dev -R '^(architecture_boundaries|architecture_boundary_contract|git_process_contract|agent_workspace_ownership)$'`:
  4/4 passed.
- `ctest --preset core-only -R '^architecture_boundaries$'`: passed.
- `python3 tests/scripts/architecture_boundary_tests.py -k test_workspace_move_checkout_and_diff_stay_in_the_workspace -k test_installed_git_runner_rejects_expanded_authority`:
  OK.
- `python3 scripts/verify_documentation.py --root .`: stops on a pre-existing tracker condition, "next_id: BPR-2385 rewinds historical high water BPR-2386"; closeout must run it again after the ID is allocated.
