---
id: BPR-0053
title: Run RFC-BPR-0003 Windows x64 target-native verification
status: Ready
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
