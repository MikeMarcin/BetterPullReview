---
id: BPR-0043
title: Implement app-owned bare caches and retained private refs
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0042]
---

# Implement app-owned bare caches and retained private refs

Create and reuse versioned app-data bare repositories under exact validated
host/repository authority. Reject symlink, junction, ownership, manifest,
configuration, object-format, alternates, shallow/graft, worktree, or
non-bare drift before use.

Fetch only the exact accepted base and pull-request-head advertisements through
the credential-free smart transport, verify both expected OIDs and complete
commit connectivity, and atomically retain paired base/head private refs.
Every retained and staging ref is qualified by a validated nonreversible
authority-scope identity, so accounts sharing physical objects cannot collide
by PR number or epoch ordinal. Never add a remote, fetch wildcard refs/tags,
use provider URLs as authority, or use remote `HeadRef` text in an argument or
refspec.

Prove cache reuse and force-pushed old-head residency after the fixture remote
removes and prunes the old object. Hard quota exhaustion must fail without
silently deleting retained history. Repository deletion and ref pruning remain
explicit, exact-target operations with documented consequences.
