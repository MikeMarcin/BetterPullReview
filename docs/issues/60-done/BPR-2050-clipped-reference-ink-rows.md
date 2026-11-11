---
schema: "implement-rfc.card.v1"
id: "BPR-2050"
title: "Clip retained reference ink rows before comparing gutter geometry"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-2047"]
priority: "P1"
platforms: ["linux", "mac", "win"]
metadata: {"milestone_plan":{"id":"bpr-2050-clipped-reference-ink","path":"docs/rfcs/plans/bpr-2050-clipped-reference-ink/milestone-plan.json"},"mandate_revision":"release55-sparse-reference-mask-r1","source_baseline":"f42e747cf9c067040052a51c6bccca9188ffef91","allocation_main":"085cf45dbb3058a0408c9e2d13d9afc1d1b706fc","reservation":"docs/issues/_reservations/BPR-2050--BPR-2050--d47ed154916e.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"96d8d57724ddc0ec4ce24ce326f419fd790adf02","evidence":{"kind":"observation","detail":"Original55and once-only diagnostic failures plus paired-frame mask rows prove bboxclamp invents ink in sparse holes. Retain/translate/clip exact reference rows; both classifiers/1pxpredicate byte-identical,590current575original messages/skips/grabs/gestures preserved, emptyvisible reference failsclosed.","ledger":"docs/verification/BPR-2050-clipped-reference-ink-rows.md","external_evidence":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer55-linux-diff-workbench-triage-r1"}},{"criterion":"AC2","outcome":"pass","revision":"96d8d57724ddc0ec4ce24ce326f419fd790adf02","evidence":{"kind":"observation","detail":"Same-code sparse100/125,dense/unclipped/fractional/missing/±2vs±1controls pass. Exact9a29/602d98Linux3scalesPASSonce8.904/12.211/14.911s, Macpeer3PASSonce7.962/8.153/10.002s, original120s/containment/rows/flags. NativeWX, format22/tidy22, static/docs/board/explicitplan pass. No qualification reuse.","ledger":"docs/verification/BPR-2050-clipped-reference-ink-rows.md","external_evidence":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer55-linux-diff-workbench-triage-r1"}},{"criterion":"AC3","outcome":"pass","revision":"96d8d57724ddc0ec4ce24ce326f419fd790adf02","evidence":{"kind":"observation","detail":"Ownedsource9a29landed main96d8d5772 with ownplan/claim, normalactual2050allocation/highwater2051and foreign source preserved.2372Linux+4671Macproducer inputs/candidate hashes exact, source55clean, taskReady0/idletrue/forests empty. Native leases returned; originalfailures retained and successor all-native gates remain root.","ledger":"docs/verification/BPR-2050-clipped-reference-ink-rows.md","external_evidence":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer55-linux-diff-workbench-triage-r1"}}]
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
