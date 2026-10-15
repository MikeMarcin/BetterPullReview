# BPR-1510 closeout clarification, 2026-10-14

The owner mandate remains the 2026-10-14 testing note r1: remove the visible
Cmd+J button and preserve Threads and its shortcut. All four original card
criteria remain unchanged. The implementer had appended a human visual gate to
AC1's authored milestone exit, although the card and mandate contain no explicit
human-only criterion.

The coordinator explicitly instructed the same implementer to correct that added
gate using the passing offscreen evidence, stop its unnecessary review server and
run canonical finish. The live plan now states AC1's original absence/spacing
requirement without the extra gate. This corrects implementation-plan overreach;
it supplies no human verdict and removes no owner requirement. The original plan
and its pending interpretation remain in Git history at `dcb4c478c` and
`8bdf097a9`; the original source receipt remains unchanged.

The observational `check-amendment` helper refused the proposed text correction
because it preserves every old exit string, including pending decisions. That
mechanical result is retained at the runtime evidence root as
`plan-correction-check.json`. The explicit coordinator instruction reconciles this
changed plan text separately, as required by the workflow for changed exits. No
source obligation, work selector, milestone identity or authorized scope changes.
A new closeout binding is computed against the corrected plan.

Session `40c921d030490283d544fb88816130ed` was inspected with no decision. Its
only candidate was released at cursor 3, its server PID 95114 was stopped through
the companion, and its task-owned frozen root was removed through the repository
freeze helper. No review decision remains pending. Existing automated evidence
is reused because the product and tested input paths are unchanged.
