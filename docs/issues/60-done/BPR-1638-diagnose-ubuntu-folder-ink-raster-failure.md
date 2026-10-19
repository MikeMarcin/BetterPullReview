---
schema: "implement-rfc.card.v1"
id: "BPR-1638"
title: "Diagnose Ubuntu folder ink raster failure"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e40cd802676d1cc0bc10287b865aa23acce326f1","evidence":{"kind":"observation","detail":"Native root slash is visible at correct palette/layout but DPR1 lacks literal foreground pixels; unchanged original predicate passes the complete course at appDPR2. Native row/font/geometry/RGB failures retained, with no inferred product color defect.","artifact":".mask/sessions/release-v0.3.0/macos-focused-r6/ctest.xml"}},{"criterion":"AC2","outcome":"pass","revision":"e40cd802676d1cc0bc10287b865aa23acce326f1","evidence":{"kind":"observation","detail":"Approved assessment-only minimumDPR2 repaint preserves font/logicalgeometry/appDPR and original folderPixels>8/filePixels==0 predicate. Actual wrong-folder paint in both palettegroups yields21file pixels and must fail the identical reader; palette restoration must pass. A blended pixel in Dark is retained in control diagnostics, with no currentpaint tolerance change.","artifact":".mask/sessions/release-v0.3.0/macos-focused-r6/ctest.xml"}},{"criterion":"AC3","outcome":"pass","revision":"e40cd802676d1cc0bc10287b865aa23acce326f1","evidence":{"kind":"observation","detail":"Complete nativeLinux e40cd8026 folder course passes after source-bound build; Mac e40 course passes1.18s including wrongpaint/restoration in4variants. Updatedfile focusedtidy22Werror passes inmacos-focused-r6; all7changedTUs passedr5. Format22/architecture/docs/diff pass. HigherassessmentDPR does not qualify userDPR1legibility.","artifact":".mask/sessions/release-v0.3.0/macos-focused-r6/ctest.xml"}}]
---

# BPR-1638: Diagnose Ubuntu folder ink raster failure

Ubuntu normal source8eef fails qt_files_tree at Overview folder ink. Preserve the approved PlaceholderText distinction, exact existing palette/count assertions and full raw failure; no blind pixel tolerance or product change before observed geometry/color evidence.

The coordinator owns the still-running native Ubuntu suite and has not released
the shared PC. Current diagnosis is source-only; no product/test repair or native
reproduction is claimed. The canonical frontend-design skill was read earlier
in the same repair milestone; it applies before any visual surface change.

## Acceptance

- AC1: Bind the exact failing row/mode/geometry and observed folder/file ink counts to native or faithful offscreen evidence, separating font/raster, selection, palette and layout causes.
- AC2: Apply only a source-backed agreed repair, retaining meaningful wrong-color negative evidence and approved UX; load frontend-design before any visual edit and verify affected platform/render courses.
- AC3: Run scoped format/tidy/architecture/documentation checks, retain failures, commit explicit paths and reconcile commands/native qualification.

## Source diagnosis checkpoint

The folder delegate explicitly selects PlaceholderText; the test currently
requires more than eight near-color pixels and zero ordinary Text pixels in
each folder-name cell, including the one-character root path. No measured
row/mode/count is present in the original Linux log. Failure-only diagnostics
now retain those counts, path, cell, DPR, font and palette colors with the
exact original predicate unchanged. A thin root glyph is only a hypothesis,
not an accepted cause or a reason to lower the threshold. Native observations
and compilation of the new diagnostics remain pending the coordinator's slot.


## Native coverage control and assessment repair

At395ae2393 only the visible root slash lacks literal ink at DPR1: viewport719x512,
cell32,176,87,22, Sans Serif9. Its closest RGB differs by32 channels, with LCD
fringes in the retained actual raster. The unchanged complete course passes
at appDPR2 in1.43 seconds. Geometry/clipping and a wrong product brush are not
inferred from the first literal-pixel failure.

The coordinator approves an assessment-only raster target of at least DPR2.
A shared Qt helper repaints the actual widget with the same font/layout and
leaves application/widget DPR unchanged; physical crop coordinates are derived
from that target's DPR. Original folder-pixel greater-than8 and file-pixel zero
checks remain. In every variant, a real temporary wrong PlaceholderText palette
paints the root folder in file ink and must fail the identical reading; the
original palette is restored before any assertion. This is ink evidence,
not a claim of user DPR1 legibility from a higher-resolution target. Native
same-DPR/negative-control verification and scoped tidy remain pending.

## Assessment control correction

Atc3bc the original folder-ink reading passes on both nativeLinux andMac, but
the new wrong-paint control fails to change the displayed glyph. The delegate
selects Active or Inactive PlaceholderText from its actual item state, while
the control changed only Active. The fixture now sets both groups on that
one view, requires actual file-ink pixels with no folder-ink pixels, rejects
the wrong image with the unchanged original predicate, restores the palette
and requires the original predicate again. Native rerun remains pending.

Native22b4 control counts confirm actual wrong paint: Light has21file/0folder
pixels; Dark has21file/1folder pixels, because one blended pixel crosses the
folder-color tolerance. Requiring zero folder pixels in this newly added
wrong-paint precondition was unsupported. The precondition requires the21
literal file-ink pixels; the separate unchanged original predicate still
rejects that image, and the restored actual folder still must pass with zero
file pixels. Original positive thresholds and tolerances remain unchanged.

## Verified repair result

Native root slash is visible at correct palette/layout but DPR1 lacks literal foreground pixels; unchanged original predicate passes the complete course at appDPR2. Native row/font/geometry/RGB failures retained, with no inferred product color defect.

Approved assessment-only minimumDPR2 repaint preserves font/logicalgeometry/appDPR and original folderPixels>8/filePixels==0 predicate. Actual wrong-folder paint in both palettegroups yields21file pixels and must fail the identical reader; palette restoration must pass. A blended pixel in Dark is retained in control diagnostics, with no currentpaint tolerance change.

Complete nativeLinux e40cd8026 folder course passes after source-bound build; Mac e40 course passes1.18s including wrongpaint/restoration in4variants. Updatedfile focusedtidy22Werror passes inmacos-focused-r6; all7changedTUs passedr5. Format22/architecture/docs/diff pass. HigherassessmentDPR does not qualify userDPR1legibility.
