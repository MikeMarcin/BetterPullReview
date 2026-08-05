---
id: BPR-0166
title: Simplification pass for RFC-BPR-0010
status: Backlog
rfc: RFC-BPR-0010
depends_on: [BPR-0158]
---

# Simplification pass for RFC-BPR-0010

The behaviour-preserving pass this repository takes after each RFC's
implementation slices: remove what the delivered design made unnecessary, merge
what turned out to be one thing, and re-run every suite to show nothing moved.

## Required direct evidence

- Each removal or merge is named, with why the delivered design made it
  unnecessary.
- Every suite passes before and after, and the diff is shown to be
  behaviour-preserving rather than asserted to be.
