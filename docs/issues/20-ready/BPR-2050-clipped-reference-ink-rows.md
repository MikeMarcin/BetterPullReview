---
schema: "implement-rfc.card.v1"
id: "BPR-2050"
title: "Clip retained reference ink rows before comparing gutter geometry"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-2047"]
priority: "P1"
platforms: ["linux", "mac", "win"]
metadata: {"milestone_plan":{"id":"bpr-2050-clipped-reference-ink","path":"docs/rfcs/plans/bpr-2050-clipped-reference-ink/milestone-plan.json"},"mandate_revision":"release55-sparse-reference-mask-r1","source_baseline":"f42e747cf9c067040052a51c6bccca9188ffef91","allocation_main":"085cf45dbb3058a0408c9e2d13d9afc1d1b706fc","reservation":"docs/issues/_reservations/BPR-2050--BPR-2050--d47ed154916e.json"}
---

# Clip retained reference ink rows before comparing gutter geometry

Linux55 normal100/125courses and the one-shot failure-only diagnostic fail the
same geometry assertion. Their reference32masks have qualifying top/bottom
rows with empty interior rows. Clamping a bounding box to the heading invents
qualifying ink in those holes. Translating/clipping the actual reference mask
matches the captured current spans24..24and30..30 exactly. Keep both existing
classifiers,1devicepixel tolerance, original skips/gestures and clipped-row
source-line picking; no production or Qt defect is claimed.

## Acceptance

- AC1. Preserve raw55and diagnostic failures and paired-frame counts. Retain
  qualifying y rows from the existing reference scan; translate each by the
  existing native displacement/qRound, clip to the same heading/image bounds,
  and then compute expected spans. Keep both classifiers/32mask/1pxgeometry,
  original590current/575original messages, skips, captures and gestures. An
  unexpected empty visible reference fails closed.
- AC2. Same-code sparse/dense/unclipped/fractional/missing/shift controls pass;
  missing sole visible ink and>=2edge-row loss/shift reject, +/-1remains allowed.
  Corrected Linux100/125/200courses pass once each through original containment,
  properties/environment/working directory/120s, with only privateEXE and
  output-only PNG namespace changed. Bounded Mac cross-native scaling proof
  passes on the exact source. Format22/tidy22/static/docs/board pass. No release
  qualification reuse or extra old-oracle negative rerun.
- AC3. Land only owned test/workflow changes on main and return writers/leases.
  Preserve frozen55producer inputs, original failures, foreign work and NEXT
  highwater. Fresh common-source all-native release qualification remains root.
