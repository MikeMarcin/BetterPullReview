---
schema: "implement-rfc.card.v1"
id: "BPR-1673"
title: "Audit command ownership and accelerate correctness qualification"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan": {"id": "release-qualification-portfolio", "path": "docs/rfcs/plans/release-qualification-portfolio/milestone-plan.json"}, "mandate_revision": "release-30min-portfolio-r1", "RFC_BASE_SHA": "a1a91fa68"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"6a57091f7ccf77475d52eafc51e7610fbf4ece4b","evidence":{"kind":"observation","detail":"All 44 actual Windows selector/scale invocations and 481 ordered entries verified; static macOS/Linux inventories retained; NDEBUG0/checked iterators observed. Missing/duplicate/unknown/order/zero-credit controls pass. See docs/verification/BPR-1673-command-owned-correctness.md; native engineering candidate a06e5bf3e; mandate release-30min-portfolio-r1."}},{"criterion":"AC2","outcome":"pass","revision":"6a57091f7ccf77475d52eafc51e7610fbf4ece4b","evidence":{"kind":"observation","detail":"46 Windows Debug command-owned courses across five families with exact source/helper/argv/config/feature/output guards; default115 calls at three scales retained.193 direct calls preserve original order and all function bodies; optimized/unknown/performance/native declarations stay exclusive. See docs/verification/BPR-1673-command-owned-correctness.md; native engineering candidate a06e5bf3e; mandate release-30min-portfolio-r1."}},{"criterion":"AC3","outcome":"pass","revision":"6a57091f7ccf77475d52eafc51e7610fbf4ece4b","evidence":{"kind":"observation","detail":"Native exact-binary four-command710.031→220.079s pilot saves489.951s/69.0%; summed709.915→729.679s, max1→4.42 further rows and exclusive drawer pass; real selector/scale failures propagate.13 runner/7 registry controls, source/docs/format gates pass. Output delta bounded258 scrollbar-thumb pixels; original serial hash recovered. See docs/verification/BPR-1673-command-owned-correctness.md; native engineering candidate a06e5bf3e; mandate release-30min-portfolio-r1."}},{"criterion":"AC4","outcome":"pass","revision":"6a57091f7ccf77475d52eafc51e7610fbf4ece4b","evidence":{"kind":"observation","detail":"Explicit candidate commits integrated main e9023554e and primary7591db7ff, preserving unrelated MASK hashes and main untracked file.121-entry private archive integrity passes, SHA36e7f3dcbe9e8b45a5c0060df5c8df3d3446b12ebc14a7c0074bc815a83a6fe2. Native own-process audit empty,8 exact tasks removed, durable M1 handoff prepared; follow-up1674 dependency frontier is now mechanically eligible. See docs/verification/BPR-1673-command-owned-correctness.md; native engineering candidate a06e5bf3e; mandate release-30min-portfolio-r1."}}]
---

# BPR-1673: Audit command ownership and accelerate correctness qualification

## Acceptance criteria

- AC1: Complete the selected workbench occurrence inventory including exact argv, platform, build configuration, scale, ownership and timing preconditions. Missing/duplicate/unknown selections and ignored case failures cannot earn passing credit.
- AC2: Implement meaningful source-audited concurrency or proven consolidation without reducing distinct behavior, fresh widget/profile/store ownership, ordered lifecycle groups, work counts, geometry/focus/input/scale evidence, deadlines or metrics. Keep unknown/native/global/performance scope exclusive.
- AC3: Measure matched native before/after wall and summed work on exact binaries; run focused case/scale/failure/output controls and the actual source/documentation gates. Preserve raw failures and qualification scope.
- AC4: Finish explicit commits, safe main/chat integration and durable command/ownership/evidence handoff. Leave the next milestone eligible without claiming the whole thirty-minute goal.

## Scope and provenance

Implements the owner’s thirty-minute release mandate `release-30min-portfolio-r1`, grounded in the retained Astra design (SHA-256 `ad0157d3693bf0ffda63d71504e7ee76c59a90c4fcb9185cfbff6ee0764812d1`). The [portfolio design](../../development/release-qualification-portfolio.md) and [milestone plan](../../rfcs/plans/release-qualification-portfolio/milestone-plan.json) retain boundaries and open choices. RFC-BPR-0038 remains open.

## Execution checkpoint

The [command-owned runtime ledger](../../verification/BPR-1673-command-owned-correctness.md) binds the preserved inventory/guards and native candidate `a06e5bf3e` to a 710.031 → 220.079 second same-binary pilot (69.0% wall reduction), 42 further passing correctness rows, the retained exclusive timing course and real failure/scale controls. All case occurrences, assertions, configurations and qualification limits remain explicit. Main/chat candidate integration and the final evidence/ownership handoff complete this milestone; RFC-BPR-0038 and the thirty-minute release goal remain open.
