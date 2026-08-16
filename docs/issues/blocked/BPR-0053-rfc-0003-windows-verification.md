---
id: BPR-0053
title: Run RFC-BPR-0003 Windows x64 target-native verification
status: Blocked
rfc: RFC-BPR-0003
depends_on: [BPR-0052]
blocked_by: platform-windows
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
