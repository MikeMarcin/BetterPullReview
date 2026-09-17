---
schema: "implement-rfc.card.v1"
id: "BPR-0880"
title: "The Git cache import refuses any pack carrying a warning-class fsck object that Git and GitHub accept"
status: "Ready"
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
