---
id: BPR-0302
title: Normalize anonymous REST review comments and unavailable state
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0300, BPR-0301]
---

# Normalize anonymous REST review comments and unavailable state

## Required final state

- One REST review-comment operation is added to the existing closed read
  contract, paging, fixtures, provider, and normalizer; no parallel path exists.
- Reply linkage reconstructs comment grouping without inventing resolution;
  viewer, viewed state, resolution, and actionability remain explicitly absent.
- The normalizer admits the exact anonymous operation set instead of requiring
  ten responses or hardcoding a full read shape.
- Malformed/cyclic/missing linkage, paging, 404 neutrality, rate limits,
  oversized bodies, and authenticated non-regression have direct tests.
