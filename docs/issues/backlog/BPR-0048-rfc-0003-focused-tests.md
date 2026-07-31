---
id: BPR-0048
title: Add RFC-BPR-0003 focused tests and Git fixtures
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0041, BPR-0042, BPR-0043, BPR-0044, BPR-0045, BPR-0046, BPR-0047]
---

# Add RFC-BPR-0003 focused tests and Git fixtures

Check in secret-free deterministic Git history recipes and expected manifests
for initial observation, linear and multiple uploads, equal material under
distinct OIDs, non-fast-forward force-push, base-only update, base-branch
change, historical/current-viewer review association, never-observed removed
objects, explicit pruning/deletion, corruption, cancellation, and
cross-authority identity.

Add core replay/property tests, host failure/reordering/recovery tests, real
installed-Git and loopback smart-HTTP contracts, hostile configuration/path/
protocol/ref/pack fixtures, Qt model/selector signal tests, public-path
automation, resource bounds, and sanitizer coverage.

Each test must assert exact authority, OIDs, private refs, catalog revision,
request/generation, availability, changed-set work, and no unintended process,
network, credential, hook, filter, worktree, or full-model-reset behavior.
