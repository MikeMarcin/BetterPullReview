---
schema: "implement-rfc.card.v1"
id: "BPR-1639"
title: "Diagnose Ubuntu shortcut column overflow"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4e0aace96da2dbfa0397c098e279f33b3093a6dc","evidence":{"kind":"observation","detail":"Actual Linux original minimum240x224 leaves title30 below required73 beside native keys195 and viewport198; all three failing scales retained.","artifact":".mask/sessions/release-v0.3.0/macos-focused-r5/ctest.xml"}},{"criterion":"AC2","outcome":"pass","revision":"4e0aace96da2dbfa0397c098e279f33b3093a6dc","evidence":{"kind":"observation","detail":"Measured delegate contents now determine minimum width while original title floor, labels, search, keycaps, accessibility, preferred size, resize and no horizontal scrollbar remain. Canonical frontend-design applied.","artifact":".mask/sessions/release-v0.3.0/macos-focused-r5/ctest.xml"}},{"criterion":"AC3","outcome":"pass","revision":"4e0aace96da2dbfa0397c098e279f33b3093a6dc","evidence":{"kind":"observation","detail":"Linux395ae actual scales100/125/200 pass0.43/0.46/0.50s and original GCC15 optimized object compiles; Mac4e0a scales pass0.66/0.59/0.63s. Source-bound7TU clang-tidy22 warnings-as-errors passes, format22/architecture/documentation/diff gates pass.","artifact":".mask/sessions/release-v0.3.0/macos-focused-r5/ctest.xml"}}]
---

# BPR-1639: Diagnose Ubuntu shortcut column overflow

Ubuntu source8eef fails keyboard shortcut scales100,125,200 at the complete title/key column fit assertion. Preserve title room, key visibility, resizability and no horizontal clipping; no assertion or scale bypass.

The coordinator owns the still-running native Ubuntu suite and has not released
the shared PC. Current diagnosis is source-only; no product/test repair or native
reproduction is claimed. The canonical frontend-design skill was read earlier
in the same repair milestone; it applies before any visual surface change.

## Acceptance

- AC1: Measure exact initial/minimum sizes, title/key columns, viewport, native label widths and fonts at the failing scales; bind the actual layout cause without assuming a font issue.
- AC2: Implement only a bounded agreed layout repair preserving approved native labels, key caps, search and accessibility; frontend-design applies and meaningful overflow controls remain.
- AC3: Verify focused native/faithful offscreen scale and font evidence plus scoped format/tidy/architecture/docs, commit explicit paths and reconcile commands/pending observations.

## Source diagnosis checkpoint

The initial/minimum size comes from 42/24 em, while the key column measures
actual native key-cap strings using ResizeToContents. A platform label/font
width mismatch is a hypothesis until observed. Failure-only diagnostics now
retain dialog/minimum sizes, title/key column and viewport widths, title floor,
DPR and font. The complete original fit predicate and subsequent widest-key
assertion remain unchanged. No layout or minimum-size repair is made; native
measurements and compilation of these diagnostics remain pending.


## Native measurement and bounded layout repair

Actual f8b8f3383 fails all three scales with a240x224 minimum, title column30
versus required native title width73, key column195 and viewport198. The
fixed24-em floor permits the contents to exceed the viewport. Initial size
passes; all failures are at the minimum. The coordinator authorizes a bounded
minimum-width repair based on these observed dimensions.

The existing delegate now measures its actual stored native key-cap strings
and combines their width with the same eight-line title floor used by its
sizeHint, row margins, list frame and reserved vertical scrollbar. The dialog
adds its layout margins and retains the existing24-em floor, preferred42-em
size, screen clamp, resizability and no horizontal scrollbar. Font/style refresh
recomputes that minimum through the existing coalesced layout pass. Labels,
keys, search, accessibility, paint and assertions are unchanged. The canonical
frontend-design skill remains applied to this established layout; no redesign.
Native focused scale verification and scoped tidy are pending on this candidate.

## Verified repair result

Actual Linux original minimum240x224 leaves title30 below required73 beside native keys195 and viewport198; all three failing scales retained.

Measured delegate contents now determine minimum width while original title floor, labels, search, keycaps, accessibility, preferred size, resize and no horizontal scrollbar remain. Canonical frontend-design applied.

Linux395ae actual scales100/125/200 pass0.43/0.46/0.50s and original GCC15 optimized object compiles; Mac4e0a scales pass0.66/0.59/0.63s. Source-bound7TU clang-tidy22 warnings-as-errors passes, format22/architecture/documentation/diff gates pass.

Exact receipts, raw failures, JUnit and source bindings remain under the ignored
release-v0.3.0 session; other release qualifications are separate obligations.
