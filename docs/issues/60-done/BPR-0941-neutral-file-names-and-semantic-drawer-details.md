---
schema: "implement-rfc.card.v1"
id: "BPR-0941"
title: "Keep file names neutral and color only change details"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"},"mandate":"r21-drawer-and-exact-material"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"50bc3d9c","evidence":{"kind":"command","detail":"Widget and actual composed delegate fail before repair; split marker/plus/minus rendering passes, names remain native text."}},{"criterion":"AC2","outcome":"pass","revision":"50bc3d9c","evidence":{"kind":"observation","detail":"Dark/light/both high-contrast selected and unselected pixel checks; accessible operation and numeric added/removed wording pass."}},{"criterion":"AC3","outcome":"pass","revision":"50bc3d9c","evidence":{"kind":"artifact","detail":"Actual Settings-driven production delegate course plus widget 2/2 39.85s; final accessibility 0.53s; safe dark/light rasters inspected in r23-rendered."}}]
---

# Keep file names neutral and color only change details

Owner-observed signed r22 defects extend the current selected-diff-exact-epoch-pair
milestone. Prior BPR-0935..0939 acceptance remains historical; these are new
follow-ups, not reopened Done cards. Preserve BPR-0934 Overview folder collapse
as separate work and do not request another owner loading test.

## Acceptance

- AC1: File names use ordinary native text; only added/removed markers and separate plus/minus counts use their semantic colors, matching Overview.
- AC2: Native selection/focus, accessible operation/count text and light/dark/high-contrast readability remain intact.
- AC3: Failure-before pixel checks and the actual composed drawer prove neutral names and independently colored details; rendered safe evidence is inspected.

## Evidence

`50bc3d9c` confines status ink to the operation marker and separate nonzero
plus/minus counts. Filenames retain ordinary palette text; selected details
retain native highlighted text. Accessible wording includes both counts.
The new widget and actual-composed-delegate checks both fail before repair
(0.63 s / 9.93 s). Repaired checks pass (2/2, 39.85 s); the final accessibility
check passes (0.53 s). Dark, light and both high-contrast rasters were directly
inspected under ignored `build/rfc0013-resume-20260919/r23-rendered/`.
Safe render artifacts are runtime evidence, not committed private screenshots.
