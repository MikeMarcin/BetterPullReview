---
schema: "implement-rfc.card.v1"
id: "BPR-1613"
title: "The Energy tab and the idle-cost tile"
status: "Done"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1609", "BPR-1612"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
acceptance: [{"criterion":"Background tiles and paired facts","outcome":"pass","revision":"ca8f399248b59af27b952013e56295816644b745","evidence":{"kind":"observation","detail":"Real retained public window shows background-only figures, separate foreground/background columns,1h/1d/3m ranges, budget reference and over-budget markers. Prior titlebar/chart accepts and r1 scale evidence remain unchanged.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-energy-recovery-3zsrq2oe/public-official-runtime.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-energy-r1-gjhn61hq/integration-exact.log","docs/verification/RFC-BPR-0086.md","docs/issues/_checkpoints/insights-window-v1/energy-r2-amendment.json"]}},{"criterion":"Summary idle tile and budget","outcome":"pass","revision":"ca8f399248b59af27b952013e56295816644b745","evidence":{"kind":"observation","detail":"Public course proves Summary last-hour background figure equals the Energy idle tile and marks the seeded budget miss.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-energy-recovery-3zsrq2oe/public-official-runtime.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-energy-r1-gjhn61hq/integration-exact.log","docs/verification/RFC-BPR-0086.md","docs/issues/_checkpoints/insights-window-v1/energy-r2-amendment.json"]}},{"criterion":"Unsupported child presentation","outcome":"pass","revision":"ca8f399248b59af27b952013e56295816644b745","evidence":{"kind":"observation","detail":"Both child-family fact cells and the background tile say Not reported, even with a partial measured sum; no-children and measured-zero states remain distinct. Offscreen render inspected.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-energy-recovery-3zsrq2oe/public-official-runtime.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-energy-r1-gjhn61hq/integration-exact.log","docs/verification/RFC-BPR-0086.md","docs/issues/_checkpoints/insights-window-v1/energy-r2-amendment.json"]}}]
---

# The Energy tab and the idle-cost tile

## Scope and acceptance

Implement the RFC-BPR-0086 Energy presentation and AC09.

- Background-only tiles, the stacked chart with the wakeups lane, the 3m
  calendar coloured by background intensity, and the paired fact table.
- Cost per idle hour against the budget line (2 s CPU per background hour, 6 wakeups per minute), with over-budget marking.
- The Summary "Cost while idle" tile.
- Unsupported exact child CPU shows “Not reported”, distinct from zero or no
  children, under RFC owner amendment r2; keep official pinned Qt binaries.
- Load `frontend-design:frontend-design` first.


## Independent presentation evidence

Source `263f45ca91d6b142f80a99c9d206882ac9baeb6a` implements all presentation criteria through the real profile
window and store: background-only tiles and Summary cost, paired facts on every
row, hour/day stack and wakeup lane,93-day local calendar, matched CPU budget
reference and explicit over-budget marking. The seeded public course passes,
including fixed/clamped shell, passive refresh, Light/Dark renders, unsupported
wakeups and a measured zero.1.25x/2x courses pass. `frontend-design` and
`code-simplifier` were read and applied. The accepted titlebar and shared chart
choices are preserved; no additional human criterion is introduced.

The independent criteria retain their passing evidence. BPR-1612 is Done under
owner amendment r2. Candidate `ca8f39924` passes the official-Qt public course,
including “Not reported” for unsupported child totals and each child-family fact.
Measured zero, no-children states and background-only known figures remain
distinct. The rendered unsupported state was inspected without clipping;
LLVM22 format/tidy pass. The accepted titlebar and charts remain intact. The [RFC ledger](../../verification/RFC-BPR-0086.md)
and current continuation retain candidate-specific evidence and that dependency.
