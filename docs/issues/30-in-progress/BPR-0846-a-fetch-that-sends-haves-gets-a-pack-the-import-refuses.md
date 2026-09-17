---
schema: "implement-rfc.card.v1"
id: "BPR-0846"
title: "A fetch that sends haves gets a pack the cache import refuses"
status: "In Progress"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-epoch-and-startup new defect 1 (BPR-0839 investigation)","found_at":"4762d716b2048fc74eadc85d3425528696daa625","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0846: A fetch that sends haves gets a pack the cache import refuses

## Suspected defect (not yet observed against a live server)

The Git cache imports a fetched pack with
`git index-pack --stdin --strict --check-self-contained-and-connected`
(`src/adapters/git/src/installed_git_process.cpp` ~3855-3866). Its process
runner accepts exit 1 only when a request sets `AllowExitOne` (~2376-2384),
and the import does not. `--check-self-contained-and-connected` exits 1 when
the pack is not self-contained. With Git 2.53, `bpr-linux` confirmed in a
local experiment that the pack is written and the exit is 1.

A server that honours a `have` omits the objects reachable from it. The pack
is then complete for the repository but not self-contained. According to
`bpr-linux`, once an epoch exists the live path's fetch for a new head
already sends haves. If so, an ordinary pull request update against a server
that trims with haves fails with `process_failure`, and the new head never
loads. No fixture honours a have: the public-path fixture transport matches
only `want` lines and serves recorded packs. That would explain why no
course sees this.

Evidence so far is code reading and a local `index-pack` experiment
([[BPR-0839]], "Why offering haves is not a local change either").

## Scope

- **Reproduce offline first.** A local `git upload-pack`, or a `file://`
  or `git daemon` remote, honours haves. Drive the real live-path fetch and
  import for a second head whose ancestors are resident, and record the
  outcome. No live GitHub or GHES access.
- If it reproduces, fix the import rule so a complete but not self-contained
  pack is accepted only when the result is verifiably connected (for example,
  accept exit 1 and rely on the existing connectivity verification). Keep the
  refusal of genuinely broken or unconnected packs. This is the Git cache
  import contract: give it a critical review.
- Add a fixture or contract course that honours haves, so the path stays
  covered.

## Acceptance

- The live-path update of a pull request whose earlier head is resident loads
  over a transport that honours haves, in a composed course on macOS, and on
  Linux and Windows through the stations.
- A pack with a missing object, or one that is not connected to resident
  history, is still refused, and a course shows it.
- If it does not reproduce, this card records why, with the experiment.

## Reproduction and fix (dsp-004, Mac)

**Reproduced offline.** It reproduced on macOS with Apple Git 2.50.1 through
a composed course. The course runs these parts for real:

- the epoch application host, including the live path's haves from `LatestLive`;
- the protocol v2 request and parser;
- the pack sink and spool;
- `InstalledGitCache` with its process runner (`index-pack`,
  `verify_connectivity`, refs);
- the SQLite epoch catalog.

Only the smart-transport port is replaced. It forwards the product's own
discovery, `ls-refs` and fetch bodies to a local
`git upload-pack --stateless-rpc` speaking protocol v2 over a bare fixture
repository, which honours haves. No GitHub, Qt or `src/app` is involved.

The first head loads. The update's fetch offers the resident head as a have,
and the server trims the pack. `index-pack --strict
--check-self-contained-and-connected` exits 1: the pack is written, every
reference resolves, and some resolve to resident objects. The import failed
with `EPOCH_CACHE_FAILURE:10` (`process_failure`) at `revision-prepare`, and
the epoch status became `failed`
(`build/r2-logs/dsp4-0846-phase1-update.log`).

With the same installed Git:

- a self-contained pack exits 0;
- the trimmed pack against a repository holding only the base exits 128
  (`did not receive expected object`).

**Cause.** The non-sparse import runs that `index-pack` without
`AllowExitOne` (`installed_git_process.cpp`), so exit 1 became
`nonzero_exit`, then `process_failure`, and the pack was rolled back.

**Fix.** `index_pack_impl` sets `.AllowExitOne = !aSparse`. An exit 1 is
accepted only when:

- standard output is exactly the expected `pack\t<oid>` line;
- standard error is empty;
- pack validation passes.

`prepare_revision` then runs `verify_connectivity(base, head)` before the
install is witnessed, before any staging ref is written and before the spool
is removed. The sparse import is unchanged. The port's `index_pack` comment
now says that success does not prove reachable closure.

**Courses.** Three courses are added in
`tests/git_contract/haves_trimmed_pack_integration_tests.cpp`. They are
registered on every platform, and uncontained on Windows with the same reason
as `epoch_maintenance_integration`, which is copied, not yet measured.

- `git_haves_trimmed_pack_update`: the update loads, and the latest live head
  is the update.
- `git_haves_trimmed_pack_missing-object`: a forged have leaves an object
  missing. `index-pack` exits 128, the pack is refused and the earlier head
  stays latest.
- `git_haves_trimmed_pack_unconnected`: an exit-1 pack over sparse,
  unconnected resident history is refused before the witness and rolled
  back. The same pack imports once that history is connected.

**Runs (macOS, `build/r2-dev`).**

- **New courses:** 3/3 each (`dsp4-0846-courses-x3.log`).
- **Negative control:** with `AllowExitOne` back to `false`, the update course
  fails with the reproduction's signature, and the unconnected course fails
  at its connected half. The missing-object course still passes, as it
  should (`dsp4-0846-negative-control.log`).
- **Contracts and courses:** all pass (`dsp4-0846-contracts.log`,
  `dsp4-0846-public-path.log`):
  - the git process, cache and sparse-cache contracts;
  - `epoch_maintenance_integration`, `epoch_kill_recovery`,
    `epoch_transaction_kill_recovery`, `git_anonymous_public_read`;
  - `architecture_boundaries`, `architecture_boundary_contract`,
    `test_network_isolation_manifest`;
  - `application_github_public_path_github_com`,
    `application_epoch_persistence_github-com`,
    `application_epoch_maintenance_public_path`, and the round-trip contract
    (no pins moved).
- **`git_protocol_boundary` fails, but not because of this change.** It fails
  identically on a clean export of `a5168c6d`; carded as [[BPR-0850]].

**Found alongside.** A pack with a missing object ends in `repair_required`
(code 13) with or without the fix. The likely cause is that `index-pack`
leaves a `tmp_pack_*` behind. Carded as [[BPR-0851]].

**Exposure note (not verified).** The gh CLI transport accepts haves but
serves a full local pack. The live exposure is therefore probably the native
HTTPS transport against a server that trims.

**Still open.** A critical review of the import rule, and the Linux
(Git 2.53) and Windows runs of the three courses, including a measurement of
the Windows containment reason.
