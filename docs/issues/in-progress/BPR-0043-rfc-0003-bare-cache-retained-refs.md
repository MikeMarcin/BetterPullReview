---
id: BPR-0043
title: Implement app-owned bare caches and retained private refs
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0042]
---

# Implement app-owned bare caches and retained private refs

Create and reuse versioned app-data bare repositories under exact validated
profile-revision/host/account/repository/object-format authority. Do not share
a physical object database across accounts or profile revisions. Reject
symlink, junction, ownership, manifest, configuration, object-format,
alternates, shallow/graft, worktree, or non-bare drift before use.

Request only the exact accepted base and pull-request-head advertisements
through the app-owned protocol-v2/native-HTTPS path, import only a bounded
pack through the credential-free Git process boundary, verify both expected
OIDs and complete commit connectivity, and atomically retain paired base/head
private refs. The catalog allocates one opaque repository-cache scope for the
complete physical partition and a separate opaque PR-authority scope for
every retained and staging ref, so PR numbers or ordinals cannot collide.
Never add a remote, fetch wildcard refs/tags, use provider URLs as authority,
or use remote `HeadRef` text in an argument or refspec.

This slice owns the app-owned protocol-v2 client, native-HTTPS smart
transport, request-owned spool creation and completed-spool validation, exact
pre-import pack inventory and preexisting-pack observations, cache locking,
cache reuse, local import handoff, connectivity checks, and retained private
refs. It must expose the exact validated spool/pack/inventory facts needed by
the durable BPR-0044 operation record. BPR-0044 owns persistence of those
facts; BPR-0045 owns the required journal-before-`index-pack` sequencing.

Prove cache reuse and force-pushed old-head residency after the fixture remote
removes and prunes the old object. Hard quota exhaustion must fail without
silently deleting retained history. Repository deletion and ref pruning remain
explicit, exact-target operations with documented consequences.
