---
id: BPR-0062
title: Preserve review database identity in the read contract
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0040, BPR-0045]
---

# Preserve review database identity in the read contract

Repair the normalized GitHub read contract exposed by BPR-0050's fresh Linux
slow matrix and reproduced after a fresh macOS relink. The production
normalizer correctly retains the recorded review database ID `4001`, which the
epoch host requires as the stable core review identity, but the independent
C++ expected read still default-initializes that field to zero. The paired
host manifests therefore agree with each other while the supposedly
independent expected object is stale.

Set the exact recorded identity in the independent expectation and add an
explicit nonzero/exact assertion so aggregate defaults cannot hide this
semantic field again. Rerun the GitHub read, provider, host, epoch-host, and
architecture contracts under Clang and GCC; run documentation/workflow/diff
gates; and resume BPR-0050 only after the remediation is committed.
