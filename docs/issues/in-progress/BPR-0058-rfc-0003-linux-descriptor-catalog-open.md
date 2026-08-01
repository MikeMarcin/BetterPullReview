---
id: BPR-0058
title: Open the Linux epoch catalog through its retained directory
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0044, BPR-0047, BPR-0057]
---

# Open the Linux epoch catalog through its retained directory

Repair the target-native Linux catalog startup path exposed by BPR-0049. The
catalog deliberately addresses SQLite through `/proc/self/fd/<directory>/…`
so a retained, privately opened directory—not a replaceable pathname—anchors
bootstrap and live database opens. SQLite's generic `SQLITE_OPEN_NOFOLLOW`
flag rejects that descriptor path because `/proc/self` and its file-descriptor
entries are symlinks, so the real application currently fails closed before
rendering on Ubuntu 26.04.

Keep `SQLITE_OPEN_NOFOLLOW` on platforms that use ordinary paths. On Linux,
omit it only for the existing descriptor-anchored catalog opens, retain every
directory/file identity and permission check, and do not weaken the retained
ancestor-chain or one-link private-file contract.

## Completion evidence

- Fresh Linux SQLite/catalog tests cover bootstrap, reopen, failure cleanup,
  retained-directory identity, and hostile symlink substitution.
- The real GitHub.com fixture starts through XCB with no forced Qt platform,
  style, or platform theme, renders the epoch control, and exits cleanly.
- Runtime tracing shows SQLite opens through `/proc/self/fd/<retained-dir>`;
  no direct replaceable catalog path is substituted.
- Focused macOS catalog, composition, and public-path tests still pass;
  architecture/documentation/workflow-absence gates and `git diff --check`
  remain clean.
