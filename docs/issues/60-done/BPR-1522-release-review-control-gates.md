---
schema: "implement-rfc.card.v1"
id: "BPR-1522"
title: "Repair review-control regressions exposed by the patch release gate"
status: "Done"
rfcs: []
depends_on: []
metadata: {"source":"0.2.1 normal Mac release gate","assigned_implementer":"/root/release021_review_controls","baseline":"40bc43a8f17e6871593c3dde539b546155af4d2c","milestone_plan":{"id":"bpr-1522-release-review-controls","path":"docs/rfcs/plans/bpr-1522-release-review-controls/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e182c6848d47fa19d7d5e19bf8efcbd2d4642b75","evidence":{"kind":"observation","detail":"Both original failure groups reproduced 3/3 at 100%, 125% and 200%. wholeReviewSavedComments fits=1 and focus=1 (TabFocus); narrow file label visible=1, text=File, tooltip=File 3 of 40. Original failing full gate and focused negatives retained.","artifact":"/Users/jaeger/.codex/evidence/bpr1522-release-controls-qihmuare/manifest.json"}},{"criterion":"AC2","outcome":"pass","revision":"e182c6848d47fa19d7d5e19bf8efcbd2d4642b75","evidence":{"kind":"observation","detail":"Saved comments now StrongFocus; all shown-button containment/focus assertions preserved. File counter admits exactly three documented forms and retains full tooltip/accessibility; all other visibility, overflow and font/floor checks retained. Editor and full width-floor offscreen renders inspected.","artifact":"/Users/jaeger/.codex/evidence/bpr1522-release-controls-qihmuare/manifest.json"}},{"criterion":"AC3","outcome":"pass","revision":"e182c6848d47fa19d7d5e19bf8efcbd2d4642b75","evidence":{"kind":"command","detail":"Affected Qt scale courses: 6/6 pass, 81.72 s. Architecture/round-trip composition/startup/multiwindow: 4/4 pass, 38.39 s. Format 22.1.8, touched production TU tidy via supported SDK-aware helper, and DOCUMENTATION_OK pass. Final clean source and exact producer digests are handed to coordinator for the required full normal gate before packaging; interrupted 734-row attempt remains failed/incomplete.","artifact":"/Users/jaeger/.codex/evidence/bpr1522-release-controls-qihmuare/manifest.json"}}]
---

# BPR-1522: Repair review-control regressions exposed by the patch release gate

The 0.2.1 full Mac gate exposes two three-scale failure groups. The Saved comments button fits its editor but has TabFocus (1), while the established editor contract requires StrongFocus. The added one-click Accept button also triggers the documented review bar's final numeric fallback at the width floor; an older assertion still requires visible numbers. BPR-1255's owner decision explicitly allows no numeric position in the fully yielded bar, with complete position tooltips.

## Acceptance

- AC1: Reproduce both failure groups on the release source and identify the failing button, geometry and focus policy. Retain negative evidence.
- AC2: Saved comments fits the narrow editor and takes StrongFocus. At the width floor the file label shows only one of the documented full, compact or group-only forms and retains its complete tooltip and accessible description. Preserve all other control/overflow checks.
- AC3: Run affected Qt courses at 100%, 125% and 200%, inspect real offscreen output where relevant, run focused composition/recovery checks plus format22 and documentation. Bind the release's subsequent normal gate to the corrected committed source; no failed gate is reported as passing.

## Ownership

The release coordinator implements this bounded correction as a leaf. Frontend-design is loaded; preserve the existing native palette, fonts, spacing and responsive policy. No new visual design or owner desktop operation is introduced. Original BPR-1507 and BPR-1520 Done evidence stays historical.

## Execution record

The owner amended the release workflow: “if there are fixes to perform use an implementer dont do them yourself”. The coordinator delegated complete BPR-1522 engineering ownership to `/root/release021_review_controls`, which adopted the three existing uncommitted files after the coordinator's source/build commands finished. This supersedes the initial ownership paragraph above. The implementer loaded frontend-design and independently inspected the patch, original failures, policy, rendered output and verification.

The product change gives Saved comments the same StrongFocus policy as the editor's other controls. The test retains every shown-button geometry/focus assertion and adds the failing control's identity to its diagnostics. The file counter test now admits exactly `File 3 of 40`, `File 3/40`, or `File`, requires visibility, and checks both the full tooltip and accessible description. Production responsive policy is unchanged.

Negative evidence reproduced each group at all three scales: Saved comments fits but reports focus 1 (TabFocus); the width-floor counter is visible as `File`, with full `File 3 of 40` tooltip. The corrected affected courses pass 6/6 (81.72 s), and focused architecture, production composition contract, startup and multiwindow courses pass 4/4 (38.39 s). Real offscreen editor and width-floor renders were inspected. Pinned clang-format 22.1.8, the supported tidy helper on the product translation unit and documentation pass. A raw tidy invocation initially failed because the SDK arguments were absent; the supported helper's SDK-bound run passes.

No public API, core policy, provider, persistence, palette, font or responsive-layout contract changes. The existing production application-source manifest excludes this Qt-only implementation, and its composition contract passes without changing pins. Original Done-card evidence remains historical.

Implementation candidate: `e182c6848d47fa19d7d5e19bf8efcbd2d4642b75`. The evidence reference and continuation are in `docs/issues/_checkpoints/BPR-1522/`. The release coordinator must rerun the normal full Mac gate from the final clean committed source before packaging/signing/publication; the interrupted failed 734-row attempt is not a passing gate. No release qualification, installed app change, live GitHub operation or owner-data access is claimed by this repair.
