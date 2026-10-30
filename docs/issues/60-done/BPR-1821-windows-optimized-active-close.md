---
schema: "implement-rfc.card.v1"
id: "BPR-1821"
title: "Diagnose the optimized Windows active-close budget miss"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v041-same-preview-waivers-r1", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.1-20261029", "milestone_plan": {"id": "release-v041", "path": "docs/rfcs/plans/release-v041/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e5b085565670f14c9df3f896a2ee94afee3cca75","evidence":{"kind":"command","command":"One unchanged-image native CTest application_large_review_active_work at quiet PC profile boundary","detail":"Original 241.9007/150 ms FAIL and decoded raw report retained; supplementary 45.6902/150 ms PASS; same image SHA before/after, actual b160/N0 build bridge, source/command seals, zero active jobs guard. Benchmark/course unchanged since published040."}},{"criterion":"AC2","outcome":"pass","revision":"e5b085565670f14c9df3f896a2ee94afee3cca75","evidence":{"kind":"artifact","detail":"BPR-1821-grounded-disposition-r1.json: grounded unchanged-source supplemental observation, all nine budgets and exact/drained functional predicates pass. Original failure remains, cause unproven; no source/predicate/threshold change or Debug waiver."}}]
---

# Diagnose the optimized Windows active-close budget miss

Frozen build-4 Windows Release `application_large_review_active_work` reports
active-close quiescence 241.9007 ms against the existing 150 ms platform limit.
The other eight completion budgets pass, the final file is exact and the close
fully drains. Closing starts with one diff and two epoch jobs. This is an
optimized Release result; Debug timing policy and the memory waiver do not
resolve it.

## Acceptance

- AC1: Preserve the exact original report and compare historical/source behavior.
  At the quiet PC profile boundary, run one authorized controlled follow-up on
  the unchanged actual image and maintained course before source changes. Bind
  the image, flags, source, host activity and exact predicate; do not infer a
  passing original result or blanket timing waiver.
- AC2: Diagnose a reproducible defect and correct it narrowly, preserving genuine
  cancellation, close/quiescence and ownership assertions, or document a grounded
  unchanged-source follow-up disposition if evidence supports it. Keep the 150 ms
  limit and every other gate unless the owner explicitly changes it. Run focused
  native proofs and meaningful controls; retain original and supplementary results
  separately and require a successor cohort for promoted changed inputs.

## Measured disposition

The one authorized quiet-PC follow-up passed on the unchanged Release image
`565480160c18f7e9073465fb00447847f441c91f7bbda3e7098c920b46d61574`: 45.6902 ms
against the unchanged 150 ms limit, with all nine enforced budgets passing,
exact final file and fully drained close, again with one diff and two epoch jobs.
The dispatch guard observed no active build/test jobs or idle foreign build nodes;
this is not a claim that every OS service was idle. The original 241.9007 ms FAIL
remains a separate observation. A cause is not proven. There is no performance
source change, relaxed predicate, Debug waiver or further retry.

The benchmark and maintained Python course are byte-identical between published
0.4.0 source 0941326a and frozen N4 fc0dd7bf1. The test image actually compiled
from b160/N0, with the accepted administrative source bridge to frozen N4.
The native follow-up receipt binds identical image hashes before/after, original
command/flags/environment, report and live capacity guard. Exact metrics, history
blob IDs and hashes are in external `BPR-1821-grounded-disposition-r1.json`.

The original full logical report was decoded from its retained qualification
stdout after the maintained course reused the output path. It is explicitly a
reconstruction; no original filesystem report hash was captured. Both original
and supplementary raw evidence remain external, with no passing-original claim.
