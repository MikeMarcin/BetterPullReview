---
schema: "implement-rfc.card.v1"
id: "BPR-2387"
title: "BetterPullReview's own Git in a retained agent workspace runs programs the agent configured"
status: "Done"
rfcs: ["RFC-BPR-0096"]
depends_on: ["BPR-2379", "BPR-2386"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","security":true,"mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"found by the BPR-2386 implementer; owner approved investigating, 2026-11-24","found_by":"BPR-2386","since":"575a992ea","reservation":"docs/issues/_reservations/BPR-2387--BPR-2387--2f7dd02589df.json","revision":"642f08dec","related":["BPR-2379","BPR-2386"],"acceptance":["BPR-2387:isolated-move","BPR-2387:isolated-inspect","BPR-2387:configured-hook-refused","BPR-2387:core-policy","BPR-2387:boundary-rule"]}
---

# BPR-2387: BetterPullReview's own Git in a retained agent workspace runs programs the agent configured

## Report

The agent can write everything in its retained workspace, `.git` included:
`.git/config` and the files it includes, `.git/info/attributes`, the
in-tree `.gitattributes`, and `.git/hooks`. BetterPullReview's own Git runs
in that workspace through `workspace_git` and `workspace_git_arguments`
(`src/adapters/git/src/installed_git_process.cpp`): inspect, crash recovery
(rebase --abort, checkout --force, reset), the [[BPR-2379]] move (status,
rev-parse, add, commit, checkout, rebase, reset, diff, log, merge-base,
update-ref, symbolic-ref), the borrowed-store repair, and seeding. That Git is
outside the agent's sandbox and permission model, so any program the
workspace's configuration makes it start runs with BetterPullReview's
authority.

## What was exploitable

`workspace_git_arguments` set system and global configuration to the
isolation's own files and, at command scope, `core.hooksPath` (private),
`core.fsmonitor=false`, `credential.helper=`, every `protocol.*.allow=never`,
`submodule.recurse=false`, `diff.external=`, the three `filter.lfs.*` keys,
`gc.auto=0`, `maintenance.auto=false` and `--no-pager`. It did not touch the
repository's own configuration. Reproduced with the runner's exact arguments
and environment on Git 2.50.1 (Apple Git-155), every one a sentinel file
written by an agent-named script:

- `filter.<any>.clean`, named by `* filter=<any>` in `.git/info/attributes`:
  ran under the move's `status` (any stat-dirty file), `add`, `commit` and
  `reset --mixed`, and the recovery's `reset`.
- `filter.<any>.smudge` and `filter.<any>.process`: ran under the move's
  `checkout --detach`, the recovery's `checkout --force` and `rebase`.
- `merge.<any>.driver`, named by `merge=<any>`: ran under the reconcile's
  `rebase` on a conflicting path, and its exit 0 silently resolved the
  conflict the move would otherwise report.
- `log.showSignature=true` with `gpg.program`: ran under the move note's
  `log` for any signed commit in the listed range. Commits made on GitHub are
  signed, so this needs nothing from the pull request.
- `core.worktree=<elsewhere>`: `-C <workspace>` let Git discover `.git` and
  honour it, so the move's `checkout --force` wrote the head's files into any
  directory the agent named, and `status`/`add` read from there.
- The recovery runs whenever the agent writes `refs/bpr/move-target` itself,
  so the checkout and reset vectors were reachable from a plain inspect.

Not reachable before or after: `core.hooksPath`/`.git/hooks` (private hooks
path at command scope), `core.fsmonitor`, `diff.external`, `credential.helper`,
`core.pager`/`pager.*` (`--no-pager`); `core.editor`/`sequence.editor` (no verb
opens an editor: the rebase is not interactive, the commit has `--message`);
`commit.gpgSign` (`--no-gpg-sign`); `diff.<driver>.textconv` and
`.command` (every diff is `--name-only`); transport keys
(`core.sshCommand`, `core.gitProxy`, `core.askPass`, `remote.*`, `url.*`,
URL-scoped credential helpers, `uploadpack.*`, `core.alternateRefsCommand`):
every protocol is refused and no verb reaches a remote; `safe.directory` and
`uploadpack.packObjectsHook` are read only from protected configuration;
aliases cannot shadow built-in verbs. Seeding runs in a workspace whose `.git`
BetterPullReview has just written; the pull request's own `.gitattributes`
names only drivers no configuration defines.

## Decision

Git has no switch that skips the repository's configuration
(`GIT_CONFIG_NOSYSTEM` and `GIT_CONFIG_GLOBAL` cover the other two scopes,
which the runner already owns). Replacing `.git/config` around each command
would rewrite the agent's own file while it may be using it. Command scope
outranks the repository's, so the isolation sets values there, chosen so the
list does not grow with what an agent writes:

- Location is named, not discovered: `--git-dir=<workspace>/.git` and
  `--work-tree=<workspace>`. `core.worktree` and a `gitdir:` file no longer
  apply.
- Drivers by construction: an attribute only names a driver; Git runs a
  program only for a driver whose command the loaded configuration defines.
  Immediately before each command Git itself lists the configuration's key
  names (`config --list --name-only --includes -z`, which runs nothing), and
  every filter, diff and merge driver found there has its command keys set
  empty (`filter.<d>.clean/smudge/process=`, `filter.<d>.required=false`,
  `diff.<d>.textconv/command=`, `merge.<d>.driver=`). Git then runs nothing
  for it; an empty merge driver cannot start, so the path conflicts and the
  move reports it. Whatever names the agent chooses, every defined driver is
  covered, through includes and `config.worktree` too.
- A closed list of single keys, bounded by the verbs the runner may run (the
  boundary rule forbids the rest): `core.fsmonitor=false`, `core.editor=:`,
  `sequence.editor=:`, `core.askPass=`, `credential.helper=`,
  `log.showSignature=false`, `commit.gpgSign=false`, `rebase.backend=merge`,
  `rebase.updateRefs/autoStash/autoSquash=false`, `rerere.enabled/autoUpdate=false`,
  `color.ui=false`, `core.protectHFS/protectNTFS=true`, and the existing
  protocol, submodule, gc and maintenance values.
- Refused, not overridden: any `hook.<name>` key. Newer Git defines hooks in
  configuration, outside `core.hooksPath`, and an empty value is not known to
  disable one. More than 64 drivers, or a listing that is not Git's, is
  refused too.

The decision is the core's (`bpr/core/workspace_git_isolation.hpp`,
`plan_workspace_git_isolation`): a pure function of the listing. The runner
passes the result as `GIT_CONFIG_COUNT`/`GIT_CONFIG_KEY_<n>`/`GIT_CONFIG_VALUE_<n>`,
the same scope as `-c`, which keeps a subsection holding `=` one key and does
not spend the runner's 64-argument bound.

## Fix

- `src/core/include/bpr/core/workspace_git_isolation.hpp`,
  `src/core/src/workspace_git_isolation.cpp`: the fixed overrides and the plan.
- `src/adapters/git/src/installed_git_process.cpp`: `workspace_git_arguments`
  names `--git-dir` and `--work-tree`; new `workspace_git_environment` lists
  the key names and plans; `workspace_git` runs every command under it (and
  takes an optional object alternate). Inspect's object check and history
  walk, the move's object copy and the borrowed-store repair now go through
  `workspace_git` too, so from repair to move no Git starts any other way.
- `cmake/VerifyArchitectureBoundaries.cmake`: "Installed-Git workspace
  isolation is incomplete" unless `workspace_git_arguments` names both paths,
  `workspace_git_environment` lists, plans, refuses and returns the plan's
  environment, `workspace_git` starts with the arguments and that environment
  and submits it, and from `repair_borrowed_workspace` to
  `cleanup_agent_workspace` only those two build `workspace_git_arguments`.

## Acceptance

- `BPR-2387:isolated-move`: in a workspace whose `.git/config` (and a file it
  includes), `.git/hooks`, an agent `core.hooksPath` and
  `.git/info/attributes` name a sentinel-writing script for every key above
  (plus `core.worktree`, `rebase.backend=apply`, `rerere`, `color.ui=always`,
  `commit.gpgSign`), a move with no reconcile, rebase (clean and
  conflicting), save and move, and ask the agent writes no sentinel and
  nothing into `core.worktree`; the conflicting rebase still names its file.
- `BPR-2387:isolated-inspect`: inspect, the recovery a crash after
  `move-after-target` leaves, cleanup and seeding write no sentinel.
- `BPR-2387:configured-hook-refused`: a `hook.<name>.command` refuses the
  move with `invalid_repository` and runs nothing.
- `BPR-2387:core-policy`: the plan is deterministic, covers each defined
  driver once with subsections holding dots, refuses hooks, malformed
  listings and too many drivers.
- `BPR-2387:boundary-rule`: the rule passes on the runner and fails for each
  removed isolation step and for a direct `workspace_git_arguments` command
  in inspect.

## Evidence

At 642f08dec (candidate 642f08dec, on main 10ee349ff), macOS arm64, Debug, Git 2.50.1 (Apple Git-155):

- `ctest --preset dev -R '^(core_workspace_git_isolation|git_process_contract|agent_workspace_ownership|retained_workspace_expiry|architecture_boundaries|architecture_boundary_contract)$'`:
  6/6 passed.
- `better_pull_review_git_process_contract_tests --workspace-git-isolation`:
  `GIT_WORKSPACE_ISOLATION_OK`. Its control, the fixture's own Git run last
  in the same workspace, does run the agent's `smudge`, `fsmonitor` and
  `process` programs.
- The same course against the runner as it was (`installed_git_process.cpp`
  at the parent commit): the agent's `clean` filter ran at the first move's
  `status`; with `core.worktree` set, the clean move checked out elsewhere.
- `python3 tests/scripts/architecture_boundary_tests.py -k
  test_workspace_git_runs_isolated_from_the_agents_configuration -k
  test_workspace_move_checkout_and_diff_stay_in_the_workspace -k
  test_installed_git_runner_rejects_expanded_authority`: OK.
- Format and clang-tidy 22 on the changed translation units: clean.

Not run: Windows and Linux. The approach is portable: `GIT_CONFIG_COUNT`
(Git 2.31) is below the 2.39 floor, `--git-dir`/`--work-tree` take the
runner's UTF-8 paths, `:` is Git's own no-op editor on every platform, and an
empty driver command fails to start under Git for Windows' shell as under
`/bin/sh`. The sentinel course is `Q_OS_UNIX` (its programs are `/bin/sh`
scripts); on Windows only the core plan and the boundary rule run.

## Residual

- The listing and the command are two processes. An agent process still
  running between turns could define a new driver between them; the move
  holds the record's lock exclusive, so no turn runs during it.
- A configuration whose include names a FIFO or an endless file stalls or
  exhausts every Git that reads it, as before: bounded by the process
  timeout and output limit, it runs nothing.
- `extensions.refStorage` and other repository-format keys are read from the
  file and cannot be overridden; a workspace the agent migrated fails the
  move's loose-ref reads closed.
