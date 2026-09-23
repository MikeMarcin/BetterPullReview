---
schema: "implement-rfc.card.v1"
id: "BPR-0880"
title: "The Git cache import refuses any pack carrying a warning-class fsck object that Git and GitHub accept"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable github+git partition, with a local reproduction","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"A pull request whose range contains a warning-class fsck object either loads, or is refused with a message naming the class and why","outcome":"pass","revision":"7fc4f6ab","evidence":{"kind":"command","detail":"macOS Apple Git 2.50.1 at 7fc4f6ab: git_process_contract passed (warning- and information-class packs import on strict and sparse paths; error-class duplicateEntries refused as unsupported_object naming the class); git_cache_contract passed; application_epoch_host passed at 58a2d885 with the unsupported_object reason.","path":"docs/issues/60-done/BPR-0880-the-cache-import-refuses-warning-class-fsck-objects-git-accepts.md#reconciled-and-finished-by-the-mac-main-station-2026-09-23-mac-batch-3"}},{"criterion":"The chosen policy is written down on this card with its reasoning","outcome":"pass","revision":"7fc4f6ab","evidence":{"kind":"observation","detail":"The decision section: accept WARN/INFO classes through a closed --strict=<msgid>=ignore list with measured version floors; refuse error class by name.","path":"docs/issues/60-done/BPR-0880-the-cache-import-refuses-warning-class-fsck-objects-git-accepts.md#reconciled-and-finished-by-the-mac-main-station-2026-09-23-mac-batch-3"}},{"criterion":"A contract row covers a warning-class object on both the strict and sparse paths","outcome":"pass","revision":"7fc4f6ab","evidence":{"kind":"command","detail":"git_process_contract over tests/fixtures/git/fsck-severity/v1 on both paths, and fsck_severity_fixture_contract (FSCK_SEVERITY_FIXTURE_OK) pinning the packs and the installed Git's identifiers; both pass at 7fc4f6ab. Linux measured Git 2.53.0/2.39.5/2.34.1; Windows sweep at 86b04901 (contains caeb98da) lists none of these rows among its failures.","path":"docs/issues/60-done/BPR-0880-the-cache-import-refuses-warning-class-fsck-objects-git-accepts.md#reconciled-and-finished-by-the-mac-main-station-2026-09-23-mac-batch-3"}},{"criterion":"BPR-0846's exact-output import rule still holds: an index-pack exit 1 is still accepted only with its exact output and verified closure","outcome":"pass","revision":"7fc4f6ab","evidence":{"kind":"command","detail":"Rule unmodified; git_process_contract exact-output cases pass at 7fc4f6ab; trimmed pack under the ignore list still exits 1 with exactly pack\\t<id> and empty stderr (measured on Git 2.53.0).","path":"docs/issues/60-done/BPR-0880-the-cache-import-refuses-warning-class-fsck-objects-git-accepts.md#reconciled-and-finished-by-the-mac-main-station-2026-09-23-mac-batch-3"}}]
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
All fifteen carry the admitted floor, 2.39.0.

**What a recorded floor means, and how to tell a measured one from a guess.**
A row recorded at 2.39.0 is one the Git 2.34.1 probe already knew, and Git does
not retire message identifiers, so every admitted Git knows it too. A row
recorded higher is one that answered `fatal: Unhandled message id` on the
oldest Git it was tried against; that version is where the evidence starts, not
where the identifier was introduced. Lowering such a row needs a measurement,
never an inference from a release note.

**The three Gits, by their own `git --version` output:**

| Reported version | Where |
|---|---|
| `git version 2.34.1` | `ubuntu:22.04` container, run from this station |
| `git version 2.39.5` | `debian:bookworm` container, run from this station |
| `git version 2.53.0` | `/usr/bin/git` on bpr-linux (stn-3f7fcc92) |

The encoding rule is "the oldest Git across the platforms", and three measured
versions are what make it checkable rather than asserted: Git answers an
identifier it does not know with `fatal: Unhandled message id` and exits 128,
so a single wrong row would fail **every** import on that installation, not
just the pull request that carries such an object.

Probed directly on Git 2.34.1: all fifteen shipped identifiers are known
(`nulInHeader` and `unterminatedHeader` answer `Cannot demote`, which is also
Git saying it knows them), while `largePathname` and a deliberately bogus
control identifier both answer `Unhandled message id`.

**Five rows were lowered from 2.39.5 to 2.39.0, each established by the same
2.34.1 probe:** `gitattributesSymlink`, `gitignoreSymlink` and `mailmapSymlink`
(shipped `INFO` rows — before the correction they were gated out of Git 2.39.0
through 2.39.4, where the symlinked-`.gitignore` dead end would have persisted
and the new information-class row would have failed), and `nulInHeader` and
`unterminatedHeader` (`FATAL` rows, never shipped; corrected so the table's
recorded evidence is accurate rather than pessimistic).

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
2.39.5 and Git 2.34.1 in containers — 2.39.5 being the oldest admitted Git that
could be obtained.
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

## The residue assertion this card removed, and what holds that guarantee

A first draft of the error-class row in
`tests/git_contract/git_process_contract_tests.cpp` asserted
`index_pack_residue(repository / "objects" / "pack") == 0U` after a refusal —
that a refused import leaves no `tmp_pack_XXXXXX` behind. It was wrong, and it
failed here on Git 2.53.0 the first time it ran. Main removed the same
assertion independently after it failed on Apple Git 2.50.1; this card reaches
the same place and leaves it out.

**What it was asserting.** That the *process* layer cleans up after a refused
`index-pack`. **Why that layer is not the place for it.** `index-pack` opens
its temporary pack before it fsck-checks any object and dies with that file
open, so whether the temporary survives is Git's behaviour, and it differs by
Git version and platform. The process adapter runs one command and reports its
result; it never promised to tidy what Git left. Pinning the absence there
turned a version difference into a product promise.

**What holds the guarantee.** The cache does, and is tested for it.
`installed_git_cache.cpp` calls `remove_failed_index_residue` on every failed
import — unconditionally, off the `!indexed.has_value()` branch, with the
single exception of `reap_failure`, which deliberately keeps the whole boundary
quarantined — and `git_cache_contract` pins it: "a refused strict import
reports index-pack's failure and leaves no temporary file" ([[BPR-0851]]).
That path is not conditioned on the failure code, so this card's new
`unsupported_object` refusal is cleaned up exactly like the refusals that came
before it. The guarantee is held; only the layer claiming it has moved.

A second review round on the first correction found one more: the three symlink
checks had been recorded against 2.39.5 when Git 2.34.1 already knows them,
which would have left the information class refused on Git 2.39.0 through
2.39.4 and failed the new rows there. All fifteen now carry the floor. That
round also found a defect outside this card's scope, in
`scripts/contained_temporary_support.py`: it calls `shutil.rmtree(onexc=...)`,
which needs Python 3.12, while `docs/development/build-and-test.md` promises
3.10 or newer. Every fixture verifier that uses it, including this card's,
inherits that requirement. It is reported to main with a patch rather than
fixed here, because this station may not edit `scripts/`.

## Reconciled and finished by the Mac main station (2026-09-23, Mac batch 3)

Idle since 2026-09-17. The decision landed on main in `caeb98da`, and the
floor correction in `9c19cab9`. The writer the dsp-006 handoff recorded as
still running on `bpr-linux` returned; its work is `9c19cab9`. No writer or
command remains, and nothing was left to build. The card closes on the
current evidence.

**macOS, Apple Git 2.50.1, at `7fc4f6ab`** (`build/mac3-stale-cards.log`):

| Row | Result |
|---|---|
| `git_process_contract` | Pass, 105.79 s, `Git process contract passed`, child exit 0 |
| `fsck_severity_fixture_contract` | Pass, 0.74 s, `FSCK_SEVERITY_FIXTURE_OK`: the frozen packs match their recipe, and this Git accepts every shipped identifier |
| `git_cache_contract` | Pass, 302.72 s, `Git cache contract passed` |
| `application_epoch_host` | Pass at `58a2d885` (same source), including the `unsupported_object` reason code |

Apple Git 2.50.1 is newer than 2.44, so the sparse path here runs with the
severity list on `--fsck-objects`, not the bare option.
`git_process_contract` checks both directions on both paths. The
warning-class and information-class packs import. The error-class pack
(`duplicateEntries`) is refused as `unsupported_object`, naming its class.
The refusal is the negative control built into the row.

**Against the acceptance:**

- *A warning-class object loads, or is refused naming its class.* It loads,
  on both the strict and the sparse path. An error-class object is refused,
  and the adapter log names its class. The reviewer sees `unsupported_object`
  rather than `process_failure`.
- *The policy is written down with its reasoning.* "The decision" above.
- *A contract row covers a warning-class object on both paths.*
  `git_process_contract` over the committed `fsck-severity/v1` packs, with
  `fsck_severity_fixture_contract` pinning the packs and the installed Git's
  identifiers.
- *BPR-0846's exact-output rule still holds.* The rule itself was not
  modified. Under the ignore list, a trimmed pack still exits 1 with exactly
  `pack\t<id>` and no standard error (measured on Git 2.53.0), and
  `git_process_contract`'s exact-output cases pass here.

**Other platforms, as their stations recorded them.** Linux: `bpr-linux`
measured Git 2.53.0 itself, and the critical review measured 2.39.5 and
2.34.1 in containers. Windows: `bpr-win`'s full sweep at `86b04901` contains
`caeb98da`, and none of these rows is among its 26 failures
(`docs/verification/dsp7-windows-full-sweep.md`). That sweep predates
`9c19cab9`, which changes the emitted list only on Git 2.39.0 through
2.39.4.

The recorded limits stand: `nullSha1` still fails closure on the strict
path, and `largePathname` is withheld below Git 2.53.0 until a station
measures an older Git that knows it.
