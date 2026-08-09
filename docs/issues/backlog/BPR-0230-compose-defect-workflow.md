---
id: BPR-0230
title: Compose defect correlation and defect cards into the shipped workbench
status: Backlog
rfc: RFC-BPR-0009
depends_on: [BPR-0229, BPR-0238]
---

# Compose defect correlation and defect cards into the shipped workbench

The defect core, correlator, and Qt card are built and tested independently;
the application constructs none of them.

## Required final state

- Accepted ordinary GitHub threads, local classification, and epoch evidence
  reach the composed correlator under exact identity.
- Classification, severity, category, lifecycle, and retained anchor history
  persist through BPR-0238's identity-scoped store and survive restart.
- A visible defect card consumes the accepted projection and emits only
  explicit reviewer intents.
- No host or Qt path can reach `verified` without the semantic reviewer
  intent; nearby changes and author replies remain evidence only.
- Counts and navigation are proven against a composed multi-defect queue rather
  than a single isolated card.
- A public fixture trace proves classification is local until a distinct
  shared action enters BPR-0229's coordinator.
- The standing composition inventory covers both the correlator and visible
  defect-card/queue surface; symbol linkage alone is not accepted as use.
