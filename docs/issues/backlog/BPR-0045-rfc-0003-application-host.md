---
id: BPR-0045
title: Implement RFC-BPR-0003 application host orchestration
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0041, BPR-0042, BPR-0043, BPR-0044]
---

# Implement RFC-BPR-0003 application host orchestration

Add a dedicated epoch application host rather than expanding the GitHub host
into a cross-feature controller. Feed it a typed accepted-pull-request
revision from the RFC-BPR-0002 application boundary, not a Qt projection.

Own Git/cache/catalog ports, workers, bounded observations, explicit observed
time, staging, serialized completion, cancellation, identity switching,
failure-atomic catalog/ref commit, cache residency, lifecycle, and incremental
presentation mapping. Every operation carries application session, profile,
host, account, repository, pull request, provider snapshot, identity/PR/epoch
generation, request, and expected catalog revision.

Manual-executor and fake-port tests must reorder every terminal path and prove
that only an exact core acceptance decision can commit or publish epoch state.
