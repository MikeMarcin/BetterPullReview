---
schema: "implement-rfc.card.v1"
id: "BPR-0880"
title: "The Git cache import refuses any pack carrying a warning-class fsck object that Git and GitHub accept"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable github+git partition, with a local reproduction","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0880: The Git cache import refuses any pack carrying a warning-class fsck object that Git and GitHub accept

## Defect

A pull request whose fetched range contains an object that `git fsck` reports
as a **warning** — not an error — can never load. The user sees
`process_failure` and has no recourse.

`src/adapters/git/src/installed_git_process.cpp:3947-3979` runs `index-pack`
with `--strict` (and `--fsck-objects` on the sparse path) and **no per-msg-id
policy**, so Git applies its strict defaults, under which warning-class
findings are fatal. `:2464-2476` then treats an accepted exit that wrote *any*
stderr bytes as `malformed_output`, which closes the remaining escape.

**Reproduced locally** with Apple Git 2.50.1, not merely reasoned:

- A tree with a zero-padded file mode (`zeroPaddedFilemode`, WARN class by
  default) makes
  `index-pack --stdin --strict --check-self-contained-and-connected --threads=1 --no-rev-index`
  exit 128 with `fatal: fsck error in packed object`, leaving `tmp_pack_*`.
- The sparse path's `--fsck-objects` exits 128 on the same pack.
- `git clone` of the same repository **succeeds**, and `git fsck` prints only
  `warning in tree ...`.
- `--strict=zeroPaddedFilemode=warn` exits 0 but writes the warning to stderr,
  so the runner still refuses it as `malformed_output`. **Only `=ignore`
  imports it.**

Artifacts are in the reviewer's scratchpad; the pack is regenerable in a few
commands and the card should carry a committed fixture rather than depend on
them.

The affected classes are not exotic in old repositories:
`zeroPaddedFilemode`, `nulInCommit`, `hasDot`, `hasDotDot`, `emptyName`,
`fullPathname`, `largePathname`, `nullSha1`.

**The product is stricter than Git's own transfer path**, where
`fetch.fsckObjects` is false by default. [[BPR-0846]]'s review concluded that
"a warning-class finding exits 128", which was correct — but the consequence,
that such repositories become unopenable, was not drawn.

## Scope

- Decide the policy deliberately, because both directions are defensible and
  the current state is neither:
  - mirror Git's own WARN/INFO defaults with a **closed, explicit**
    `--strict=<msgid>=ignore` list, so the import accepts what Git accepts and
    nothing more; or
  - keep the strictness and surface an honest "unsupported repository"
    refusal that names the object class, instead of `process_failure`.
- Whichever is chosen, the stderr rule at `:2464-2476` needs to distinguish a
  warning line from malformed output, or the `=warn` route stays closed too.
- Add a contract row built on a pack containing a warning-class object, so the
  decision is pinned rather than rediscovered.
- Check the same question for the sparse path, which uses `--fsck-objects`
  rather than `--strict` and fails identically.

## Acceptance

- A pull request whose range contains a warning-class fsck object either loads,
  or is refused with a message naming the class and why.
- The chosen policy is written down on this card with its reasoning.
- A contract row covers a warning-class object on both the strict and sparse
  paths.
- [[BPR-0846]]'s exact-output import rule still holds: an `index-pack` exit 1
  is still accepted only with its exact output and verified closure.

## The decision

Both directions in Scope are taken, each for the class it fits: the import
**accepts** what Git's own fsck calls a warning or an information finding, and
**refuses an error-class object by name** instead of as `process_failure`.

`index-pack` is given a closed, written-out `--strict=<msg-id>=ignore` list —
not a wildcard and not `--no-strict` — naming exactly the identifiers whose
default severity in Git's fsck table is `WARN` or `INFO`. Error-class findings
stay fatal.

**Why accept them.** GitHub serves these repositories and `git clone` accepts
them; Git's own transfer path has `fetch.fsckObjects` false by default; and
this cache verifies pack closure separately with `verify_connectivity`.
Refusing a repository Git itself accepts costs the user the entire pull
request for integrity we do not actually rely on.

**Why `=ignore` and not `=warn`.** `=warn` still writes the finding to
standard error, and `src/adapters/git/src/installed_git_process.cpp:2464-2476`
turns any standard error on an accepted exit into `malformed_output`. With
`=ignore` standard error stays empty, so the repository opens and
[[BPR-0846]]'s exact-output rule is untouched rather than relaxed. That rule
was not modified: measured on Git 2.53.0, a trimmed pack under the ignore list
still exits 1, still prints exactly `pack\t<id>` on standard output, and still
writes nothing to standard error.

**The list, and what is left out.** The adapter's table
(`BPR_0880_FSCK_MESSAGE_TABLE` in `installed_git_process.cpp`) ships
`badFilemode`, `badTagName`, `emptyName`, `fullPathname`,
`gitattributesSymlink`, `gitignoreSymlink`, `gitmodulesParse`, `hasDot`,
`hasDotdot`, `hasDotgit`, `mailmapSymlink`, `missingTaggerEntry`,
`nulInCommit`, `nullSha1` and `zeroPaddedFilemode`. Each row records the oldest
Git measured to know it and is offered only to an installation at or above that
version, because an identifier Git does not know is answered with
`fatal: Unhandled message id` and would fail every import on that installation.
The list is encoded for the oldest supported Git, never for this station's.
Eleven rows are Git 2.2-era and carry the admitted floor, 2.39.0; the four
`.gitattributes`/`.gitignore`/`.mailmap` symlink checks carry 2.39.5, the
oldest admitted Git the change was actually measured against.

`largePathname` is **left out** below Git 2.53.0: it answers
`Unhandled message id` on 2.39.5 and its introduction could not be dated from
either station, so it is recorded conservatively rather than guessed at. A
pull request with a path component over 4096 bytes therefore still fails on a
Git older than 2.53; lowering that row needs a measurement on a station, which
`fsck_severity_fixture_contract` is the place to take. `badGpgsig` and
`badHeaderContinuation` are unknown at 2.39.5 too, but they are `ERROR` rows
that exist only so a refusal can be named. `nulInHeader` and
`unterminatedHeader` are `FATAL` and cannot be demoted at all — Git answers
`fatal: Cannot demote ... to ignore`.

**One acknowledged gap: `nullSha1` on the strict path.** Ignoring it lets fsck
accept a tree entry naming the null object, but
`--check-self-contained-and-connected` then answers
`fatal: did not receive expected object 0000...`. That is a closure failure,
not an fsck finding, so the refusal carries no class name and the pull request
still does not load on the strict path. The sparse path accepts it, which is
consistent with `sparse_commit_trees` residency: that path already tolerates
objects the pack does not carry and never runs `verify_connectivity`. This is
unchanged behaviour on the strict path rather than a regression, and it is
recorded here rather than left to be rediscovered.

**The sparse path.** `--fsck-objects` takes the same `<msg-id>=<severity>`
parameter, but only from Git 2.44; before that the option takes no argument and
the whole invocation is rejected. The adapter passes the list on
`--fsck-objects` when the probed installation is 2.44 or newer and keeps the
bare option otherwise, so an older Git keeps today's stricter sparse behaviour
instead of failing outright. `fsck.<msg-id>` configuration is **not** an
alternative: `index-pack` does not read it at all, which was measured directly.

**Naming the refusal.** An error-class refusal now returns
`git_process_failure::unsupported_object` / `git_cache_failure::unsupported_object`
whose detail is the fsck message identifier, taken only when it matches the
adapter's own table so no text the pack's author chose is ever echoed. It
reaches the reviewer as `epoch_reason_code::unsupported_object` rather than
`process_failure`, because nothing is broken locally and no retry helps.

**INFO is not benign either.** Measured while deciding this: an `INFO`-class
object exits 0 under `--strict` but writes a `warning:` line, so the
`:2464-2476` rule refused it as `malformed_output` — a second, undiagnosed
route to the same dead end that the original report did not see. The closed
list covers `INFO` for that reason, and `information-class.pack` (a symlinked
`.gitignore`) pins it alongside the warning class on both paths.

## Evidence and review

Measured on Git 2.53.0 (this station) and, by the critical review, on Git
2.39.5 in a container — the oldest admitted Git that could be obtained.
The independent critical review found one blocking defect: an earlier draft
excluded `gitattributesSymlink`, `gitignoreSymlink` and `mailmapSymlink` as
"newer than the floor", which the 2.39.5 measurement falsified. They are in the
list above, and `information-class.pack` now covers that class. Three further
findings were addressed: the fixture verifier no longer runs
`--fsck-objects=<policy>` on a Git below 2.44, `epoch_reason_code`'s new
enumerator is appended rather than inserted, and the verifier now refuses a
message table it cannot fully parse instead of silently probing fewer rows.
The review also confirmed, by materializing packs carrying `.git`, `..`, `.`,
an empty name, `.GIT`, `GIT~1` and absolute paths, that Git's always-on
`verify_path` in `read-tree` — not fsck — is what protects the agent workspace,
so ignoring `hasDotgit`, `hasDot`, `hasDotdot`, `emptyName` and `fullPathname`
does not open a checkout path.
