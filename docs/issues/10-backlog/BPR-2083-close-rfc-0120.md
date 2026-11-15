---
schema: "implement-rfc.card.v1"
id: "BPR-2083"
title: "Verify and close RFC-BPR-0120"
status: "Backlog"
rfcs: ["RFC-BPR-0120"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0011-close-agent-run-checks.md","draft_title":"Close agent-run checks"}
depends_on: ["BPR-2189"]
closeout_for: ["RFC-BPR-0120"]
---

# BPR-2083: Verify and close RFC-BPR-0120

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0120](../../rfcs/RFC-BPR-0120-the-agent-runs-a-planned-check-on-a-local-pr-draft.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

Acceptance assessment for planning and running a check on a local draft and reading its report, as rewritten without evidence on 2026-11-15.

## Intended verification

Judge every criterion on the composed application with the fixture agent, and once with an installed agent on a real local draft; AC07 is the owner's look.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `775bd2d764e1651750d06e2ea798ea4d66934996`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Plan a check…, with words or none, returns a plan card naming each command, what it shows and the grants it needs, and nothing executes before Run; Run is unavailable with the specific reason when a needed grant is off, another check is running in the draft, or there is no candidate. | Pending | Pending | Not implemented |
| **AC02:** A run executes in a check copy of the current candidate: the author's worktree, branch and index are unchanged by it, and editing, saving and capturing a new candidate during the run do not change what it checks. | Pending | Pending | Not implemented |
| **AC03:** The report opens as a file tab from its card and shows the outcome word, the candidate checked, each command with its exit status where carried and out-of-plan commands marked, the output explaining a failure, code references that open the candidate at the line, and what was not covered. | Pending | Pending | Not implemented |
| **AC04:** Failed, Couldn't tell, Didn't run and Stopped reports are kept like Passed ones; a check that changed tracked files in its copy lists them; a report on an older candidate says so and Run again checks the latest candidate with the same plan. | Pending | Pending | Not implemented |
| **AC05:** Reports survive restart with the conversation and a follow-up question there reads them; nothing from a check reaches the description, the tour, the public preview or GitHub; check copies count toward the storage limit and are removed with the draft. | Pending | Pending | Not implemented |
| **AC06:** Plans and runs are briefed by `local-draft.run-check`, whose edit reaches the next plan and run and no other prompt, and which round-trips through the sharing format. | Pending | Pending | Not implemented |
| **AC07:** On a frozen build with the fixture agent, the owner accepts, one question each, the plan card, a passing report, a failing report, and a report on an older candidate. | Pending | Pending | Not implemented |

## History

Blocked on `rejected-prerequisite` from 2026-11-15, when the evidence design
this RFC first depended on was rejected and its implementation card BPR-2082
closed as rejected. The same day the owner directed "Rewrite without
evidence". The RFC was rewritten to stand on built work only (RFC-BPR-0101,
0103, 0104, 0107, 0108, 0110 and 0128), its criteria replaced, and BPR-2189
filed as its implementation. This closeout waits on BPR-2189 in Backlog and
depends on no rejected work.
