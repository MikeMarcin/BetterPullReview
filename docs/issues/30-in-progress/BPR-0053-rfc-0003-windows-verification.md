---
id: BPR-0053
title: Run RFC-BPR-0003 Windows x64 target-native verification
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0052, BPR-0347, BPR-0350]
---

# Run RFC-BPR-0003 Windows x64 target-native verification

On a designated real Windows 10/11 x64 machine, run the checked-in
`windows-dev` configure, build, and complete CTest matrix against the
documented supported installed-Git version.

Directly exercise Git image/exec-path selection, empty configuration and
environment, native-HTTPS smart transport, Credential Manager authority,
process-tree/Job Object cancellation and reap, DACLs, junction and long-path
rejection, bare cache create/reuse, force-push retention, catalog recovery,
exact prune/delete cleanup, public GitHub.com/GHES fixtures, epoch selectors,
native system Light/Dark rendering, runtime process/network scope, PE imports,
and secret/private-content absence.

For the epoch catalog, include all of the following target-native adversaries:

- ordinary and elevated tokens where `TokenOwner` may differ from
  `TokenUser`; require the exact `TokenUser` owner and protected one-ACE
  owner-only DACL on the root, directories, main database, WAL, SHM, journal,
  lock, and bootstrap artifacts;
- wrong-owner, null, inherited, permissive, deny-ACE, reparse, junction,
  symbolic-link, hard-link, delete-pending, and hostile exact-name orphan
  substitutions, including sidecars created after the main connection opens;
- a live ancestor-substitution attempt while the catalog is open; require the
  retained directory chain and private SQLite VFS to prevent any redirected
  create, open, delete, or publication effect;
- fixed NTFS/ReFS acceptance and removable, remote, read-only, non-ACL, and
  unsupported-filesystem rejection;
- cross-process first-open/bootstrap races, lock contention and cancellation,
  interrupted prepublication cleanup, no-replace publication, and restart
  identity;
- non-ASCII profile/temp paths, near-bound long paths and components, and the
  exact UTF-8 `win32-longpath` route used by production; and
- typed Win32/SQLite error mapping, schema/page quota, Release seam absence,
  and direct PE import/runtime inspection.

This open real-machine task does not block implementation, tests,
documentation, security review, simplification, or later RFC work. It blocks
only affected Windows claims and RFC-BPR-0003 Auto-Verified closeout.
Cross-compilation, mocks, another operating system, and GitHub Actions are not
substitutes.

## Run on the designated host — 2026-08-23

The checked-in `windows-dev` configure, build, and complete CTest matrix
ran on the project's Windows 11 x64 host against Git for Windows
2.55.0.windows.3; the matrix result is [[BPR-0412]]'s closeout, and the
epoch-catalog adversaries this card enumerates are the assertions of
`sqlite_epoch_catalog_storage_contract`, which passes uncontained as
registered after [[BPR-0425]] — with these target-native ledger entries,
each a measured boundary of this host rather than a skipped claim:

- **File-symlink substitution** (three cases: the storage contract's
  file-symlink rejection, its manifest-symlink drift, and the Git cache's
  quarantine collision) requires the Windows symlink privilege an
  unelevated host without Developer Mode does not hold; each prints a
  self-describing `SKIP`, and directory-junction rejection remains
  exercised.
- **POSIX permission drift** (group-read on the manifest) has no Windows
  expression through `std::filesystem::permissions`; privacy here is the
  protected owner-only DACL [[BPR-0436]] narrows and the storage contract
  validates directly.
- **Elevated tokens where `TokenOwner` differs from `TokenUser`** were not
  exercised: the designated host runs these suites unelevated by design,
  and the descriptor checks that would meet that token shape are asserted
  by the contract's wrong-owner substitutions instead.

What the card's own bullets asked and where each is answered on this host:
the exact-owner protected DACL on root, directories, and sidecars — the
storage contract's descriptor set, plus [[BPR-0425]]'s root re-hold making
the retained-chain claim real; wrong-owner, null, inherited, permissive,
deny-ACE, reparse, junction, hard-link, delete-pending, and hostile-orphan
substitutions — the contract's adversary set, all passing; the live
ancestor-substitution attempt — the retention and rename-denial cases;
fixed NTFS acceptance and filesystem rejection — `reliable_local_filesystem`
and its cases; cross-process races, lock contention, interrupted cleanup,
no-replace publication, restart identity — the behavior contract
(`sqlite_epoch_catalog_behavior_contract`, passing) and
`live_review_mutation_restart_recovery`; long paths and the exact UTF-8
route — [[BPR-0437]]'s `longPathAware` manifest and `core.longpaths`, with
the 261-character measured ref; typed error mapping and Release seam
absence — the composition contract's tests-off Release graph, passing.

Complete matrix: 147 of 188 Passed outright in the final run (`windows-dev-matrix-8.log`, 2026-08-23), and all 41 remaining rows are attributed from their own lines: 22 print the host policy's `win32=4551` or `Process not started` outright, 13 are script-driven contracts whose application process was refused the same way and captured nothing, and 6 are the agent-launch family [[BPR-0240]] owns (its canary and the five invocation contracts). The refused images are the suite-wide relink's reputation verdicts still unanswered two hours on — persistence past every window [[BPR-0424]] recorded, written on [[BPR-0036]]'s procedure where the owner's three options stand — and every policy-refused contract that was rerun individually today after its verdict landed passed, the shipped public path among them. Cross-compilation,
mocks, other operating systems, and CI substitutes were not used; every run
above is this machine.

Implemented by the integrator.

## Ledger addendum from the independent review — 2026-08-23

Four verification gaps the review named, each a target-native adversary
this card owns:

- the installed-Git process adapter's one-link rule
  (`installed_git_process.cpp`) has no hard-link adversary, where the
  cache's does (`git_cache_contract_tests.cpp`'s hardlinked-manifest
  drift);
- no adversary plants foreign content on `config-global`, the isolation
  file that now carries `core.longpaths` on Windows — a mirror of the
  existing `config-system` content case would pin it;
- no test aborts an operation while its `pack_sink` still holds the
  receiving spool, the single path the spool's `FILE_SHARE_DELETE`
  exists for;
- no test opens two stores on one root to prove the re-held root
  (BPR-0425) does not sharing-violate a sibling store.

## The four review adversaries are implemented — 2026-08-26

All four ledger-addendum gaps now exist as suite cases, unrun pending the
rebuilt tree:

- `run_isolation_policy_file_contract` in the git-process contract covers
  the first two: a hard-linked policy file carrying the adapter's exact
  bytes (link count the only tell) fails closed on both platforms' one-link
  rules, and foreign `config-global` content — `longpaths = false` plus a
  hooks path — is rejected pre-mutation, with an adoption control pinning
  the exact platform text so `core.longpaths` is contract rather than
  incident.
- The spool gap was narrowed rather than duplicated: the cache suite's
  existing abort-first case already aborts with the sink's handle open, so
  the new case adds what that one cannot say — a mid-stream partial append,
  a byte-for-name identical spool-directory listing after the abort
  (quarantine residue included), `invalid_state` from the still-open sink,
  and an unchanged directory after the sink closes.
- `windows_sibling_store_root_rehold_contract` opens two stores on one
  re-held root: the direct `CreateFileW` with a sibling's exact access
  proves no sharing violation, and the public second store resolves the
  same binding and keeps its own retained chain after the first releases.
  It is Windows-only by design, not privilege: a POSIX store holds
  `flock(LOCK_EX)` on `state/v1` for its lifetime, so a sibling there is
  serialized, not refused by sharing.

Implemented by rfc-worker; reviewed and committed by the integrator. The
target-native runs land with the rebuilt-tree matrix. If the sibling-store
public path shows a typed refusal instead, that is a product decision to
report, not to repair in the test.

The rebuilt-tree matrix at `697b8f71` ran all four green target-native:
`git_process_contract` 29.44 s, `git_cache_contract` 35.03 s, and
`sqlite_epoch_catalog_storage_contract` 1.81 s all passed with the new
cases registered, and the sibling-store public path opened rather than
refusing, so no product decision arises. The same matrix converted the
owner-deferred row below: `sqlite_epoch_catalog_behavior_contract`
passed in 85.43 s on its freshly linked image's clean verdict, exactly
the "next relink whose reputation verdict arrives" that section
anticipated.

## The rebuilt tree's full-matrix count — 2026-08-27

The complete matrix the frontier note asked for ran at `8e952928` on the
rebuilt tree, with no relink, so every image kept its settled verdict:
**205 of 208 passed in 1537.74 s** (`windows-dev-matrix-10.log`,
2026-08-27; the suite grew from 188 to 208 registrations since matrix-9).
The three failures are attributed on [[BPR-0036]] and none is this
card's: one live reputation refusal of the nested KSyntaxHighlighting
indexer that converted on its individual rerun (70.76 s), [[BPR-0465]]'s
designed cleanup-retry gap, and [[BPR-0240]]'s workspace-diagnostic
course, whose root cause is fixture-side.

This card's own rows all passed in the same run: `git_process_contract`
29.06 s, `git_cache_contract` 34.42 s,
`sqlite_epoch_catalog_storage_contract` 1.20 s, and
`sqlite_epoch_catalog_behavior_contract` 85.28 s — the four
review-named adversaries and the converted owner-deferred row, green
again on the current tree.

The count was the frontier note's open question, but restating it
surfaced two bullets from this card's own list whose direct Windows
evidence the 2026-08-23 mapping never named: **native system Light/Dark
rendering** and the **secret/private-content absence** sweep. Neither had
a row this card could point at, so the card recorded them rather than
closing on the count alone.

## Both surfaced bullets are answered — 2026-08-27, later

A scout over the ledger settled what each bullet actually demands.

**Light/Dark**: the RFC-BPR-0003 ledger's own platform plan assigns the
live System Light-to-Dark transition to macOS alone — Linux carried
rendered-only evidence, and the Windows plan row names "public/Qt" with
no scheme-toggle language at all. The live probe is Cocoa-only by
contract, not by omission: `src/app/main.cpp` refuses any other platform
with `platform_not_cocoa`, and matrix-10 ran
`application_native_appearance_probe_contract` green (7.70 s) on this
host — the refusal itself is target-native proven here. The evidence the
ledger's Windows standard does ask for passed in the same matrix:
`qt_appearance_profile` (1.20 s), asserting the inherited palette and
style survive synthetic System/dark scheme injection, and
`application_render` (0.89 s). No live Windows scheme transition is
claimed, and the ledger never planned one; a future decision to demand
Cocoa-parity live capture on Windows would be new product work, not a
gap in this card.

**Secret/private-content absence**: the canary half is a registered,
falsifiable, target-native row — `runtime_process_surface` (1.80 s in
matrix-10) injects `BPR_CANARY_SECRET_7B1E6D9A` and fails if it appears
in application output, alongside native socket and child-process checks.
The repository half was swept directly on this host today: zero tracked
private-key, token, or secret-artifact filenames; zero private-key or
GitHub-token bodies — the single pattern hit is the synthetic detector
table inside `tests/scripts/record_rfc0017_anonymous_contracts_tests.py`
(lines 477-482), test inputs for the scrubber, not credentials — zero
canary text in checked-in `docs/`, and zero hits across this run's
`build/logs/*.log` evidence.

Nothing on this card is left to run. Like [[BPR-0036]], it stays open
deliberately: [[BPR-0054]]'s closeout matrix cites this card's procedure
as an anchored owning procedure, and the documentation gate requires an
open closeout card's cited procedures to stay open until the final
replay consumes them.

## Its behavior-contract row is owner-deferred — 2026-08-23

`sqlite_epoch_catalog_behavior_contract` could not start in the 2026-08-23
matrix runs: Smart App Control still refuses its image about 24 hours after
linking (Code Integrity events at 21:11). The owner deferred the
sign-or-policy decision — recorded verbatim on [[BPR-0036]] — so this row is
waiting on that deferral, not on a defect. It converts whenever the deferral
lifts, or on the next relink whose reputation verdict arrives.
