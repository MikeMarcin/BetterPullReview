---
schema: "implement-rfc.card.v1"
id: "BPR-0685"
title: "Fit Settings controls at minimum width"
status: "Done"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0684"]
acceptance: [{"criterion":"AC-07/08/09","outcome":"pass","revision":"cade5522 plus this finish","evidence":{"kind":"observation","detail":"Four Qt Settings tests passed at 100/125/150/200% (2.78s total), each exercising seven pages and Light/Dark/HC Light/HC Dark at 760x480. Long server/account/repository/storage fixtures and zero horizontal page-scroll assertions pass; existing page, keyboard, close and focus checks remain passing."}},{"criterion":"Rendered evidence","outcome":"pass","revision":"cade5522 plus this finish","evidence":{"kind":"observation","detail":"112 rasters produced in build/dev/settings-review/matrix-{1,1.25,1.5,2}. Inspected corrected Accounts Light, Reviews Dark 125%, Agents HC Light 150% and Storage HC Dark 200%: category labels remain visible and page content fits. macOS arm64 Qt 6.10.3 offscreen; deterministic layout evidence, not a native accessibility verdict."}},{"criterion":"Quality","outcome":"pass","revision":"cade5522 plus this finish","evidence":{"kind":"observation","detail":"Canonical tidy for the two changed TUs passed. Documentation and diff checks passed. No attended protection conflicts. BPR-0683 retains native/attended integration acceptance."}}]
---

# Fit Settings controls at minimum width

## Scope

BPR-0683's actual 760x480 workbench Settings captures reveal 46px horizontal
scroll on Accounts and 21px on Reviews in all four appearances. Repair the
narrow Settings sidebar and control layout using measured label widths and
wrapping/stacking. Keep existing controls, actions and dispatch identities.

Owned paths: Qt Settings/session/workbench layouts and Settings render tests.
Routing: standard | Reason: default implementation.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

## Acceptance

AC-07/08/09: all seven pages fit 760x480 without horizontal scrolling at
100/125/150/200%, all four appearances; page/row identity and keyboard access
survive resizing. Long metadata remains accessible in bounded controls.

## Evidence

Initial deterministic macOS arm64 Qt 6.10.3 captures in build/dev/settings-review
were inspected and show the horizontal overflow. Those are discovery evidence,
not passing acceptance. BPR-0683 is paused in Backlog while this repair runs;
its render-test changes are now owned by this slice.

## Implemented repair

Measured sidebar label width replaces the fixed 196px width only in small
windows. All category names remain visible. No control or action changes were
needed. The new render course asserts zero horizontal scroll with long accepted
metadata across seven pages and all four appearances; CTest adds 125/150/200%
variants alongside the existing 100% test.
