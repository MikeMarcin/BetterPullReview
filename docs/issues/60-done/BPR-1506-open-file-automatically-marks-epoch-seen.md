---
schema: "implement-rfc.card.v1"
id: "BPR-1506"
title: "Automatically mark an opened file Seen for its epoch"
status: "Done"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Seen should be automatically applied to a viewed file on an epoch when you open it","milestone_plan":{"id":"bpr1506-auto-seen-v1","path":"docs/rfcs/plans/bpr1506-auto-seen-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"348bf36ee000b7f8ac60ae0ae31d0aa5387b6bf0","evidence":{"kind":"command","detail":"Public file-open action persists and presents Seen; full-window offscreen render inspected."}},{"criterion":"AC2","outcome":"pass","revision":"348bf36ee000b7f8ac60ae0ae31d0aa5387b6bf0","evidence":{"kind":"command","detail":"Real hidden Overview load, visible Review reveal, exact stale/file/material/missing-result refusal and unavailable-store negative course pass."}},{"criterion":"AC3","outcome":"pass","revision":"348bf36ee000b7f8ac60ae0ae31d0aa5387b6bf0","evidence":{"kind":"command","detail":"Changed-material and unchanged Seen restart pass; unchanged restart performs zero receipt writes. Explicit unread survives until reopen; Read and Accept remain separate."}},{"criterion":"AC4","outcome":"pass","revision":"348bf36ee000b7f8ac60ae0ae31d0aa5387b6bf0","evidence":{"kind":"command","detail":"Seven focused foundation/composed/public tests pass; clang-format22 and 67 documentation regression checks pass. Global board dependency mismatch is unrelated and assigned to coordinator integration."}}]
---

# BPR-1506: Automatically mark an opened file Seen for its epoch

Opening a file in an epoch automatically applies Seen to that viewed file in that epoch. Reuse the existing receipt policy and durable workflow, with the core deciding, the host performing and Qt presenting.

## Acceptance

- AC1: Opening and successfully presenting a file in an epoch automatically records and projects Seen without a separate gesture.
- AC2: Seen binds to the exact viewed file and epoch; opening one cannot mark another file or epoch, and stale or failed loading cannot mark unseen content.
- AC3: Automatic Seen follows existing persistence and reload behavior and remains distinct from any stronger reviewed/verified state.
- AC4: Exercise the composed public open-file path and focused identity, stale-result and persistence checks.

## Implementation

Inspect the existing meaning and acceptance point for Seen before changing policy. Owner explicitly requests this behavior. Keep source ownership separate from BPR-1505's Home action layout. If visible UX changes are needed, load frontend-design:frontend-design first as required by AGENTS.md. Never operate or capture the owner's live desktop.

## Result and acceptance evidence

Implemented at `348bf36ee000b7f8ac60ae0ae31d0aa5387b6bf0` for the owner's 2026-10-14 instruction. A successfully displayed selected comparison in Review submits the ordinary core-decided Seen command after its material observation succeeds. The completion rechecks the exact file, material, generation, selected request, epoch context, ledger authority and visible surface. Hidden Home/Overview loading and unfinished tab restoration cannot mark Seen. One automatic attempt per open preserves an explicit Mark unread until reopening; Read, Accept and Exclude retain their existing core semantics.

- AC1: The composed public file gesture automatically persists Seen and shows the existing private progress mark. The full-window offscreen render `build/dev/accepted-overview-renders/review-automatic-seen.png` was inspected; the shown file alone carries Seen.
- AC2: The composed ledger course loads a file under Overview with zero receipt commands, then enters Review. It refuses stale generations, another file, wrong material and a missing result; the unavailable-store course records no receipt and disables progress actions.
- AC3: Seed, changed-material reopen and a second unchanged-material restart pass. The last process loads Seen with zero receipt commands/persistence requests. The public integration course retains explicit Mark unread while open, marks Seen on reopening, and preserves separate Read/Accept behavior.
- AC4: Seven focused tests pass: architecture boundaries; core, application host and SQLite ledger tests; composed ledger course; accepted-file Overview and Overview double-click public paths. Clang-format 22.1.8, `git diff --check` and all 67 documentation regression tests pass.

The global documentation gate reports the pre-existing BPR-1521 Ready dependency on BPR-1520 Ready in this branch. BPR-1506 scoped board validation passes; the coordinator owns reconciliation with its separate P0 integration. No human/native criterion is added to this behavior-only change. Frontend-design was loaded for the visible progress behavior; the existing presentation is reused.

See the [durable checkpoint](../_checkpoints/BPR-1506/auto-seen-r1.md) for exact commands, ownership and continuation.
