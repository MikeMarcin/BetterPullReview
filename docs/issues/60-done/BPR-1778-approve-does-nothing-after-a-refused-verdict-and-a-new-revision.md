---
schema: "implement-rfc.card.v1"
id: "BPR-1778"
title: "Approve does nothing after a refused verdict and a new revision"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"owner_direction":"2026-10-27","reported_on":"v0.3.4"}
acceptance: [{"criterion":"AC1 regression through the composed application","outcome":"pass","revision":"791822c7f+BPR-1778","evidence":{"kind":"command","detail":"application_review_submission_composition_public_path (and its 125/200 scale runs) gains a seed-phase course: GitHub refuses an Approve, the page says so, the PR gets a new head through the public Refresh, and the next Approve submits once on the new head. Before the repair the course fails at 'a review with no summary goes on at the new revision, and Approve is offered'; after it all three pass."}},{"criterion":"AC2 submission safety unchanged","outcome":"pass","revision":"791822c7f+BPR-1778","evidence":{"kind":"command","detail":"The existing courses in the same test still pass: a written summary still freezes on its revision and Submit refuses it, duplicate presses send one POST, unknown attempts reconcile without a second POST, restart recovery keeps the key. Focused runs of the CLI ordinary, label recovery, navigation entry, header source, source find, agent finding, setup destination, submission contract, application host and core submission tests pass."}},{"criterion":"AC3 no silent refusal","outcome":"pass","revision":"791822c7f+BPR-1778","evidence":{"kind":"artifact","detail":"Source review: every early return in review_submission_workflow::dispatch for a gesture the editor offered now re-presents the page, with a status where none was set; a held-review lookup re-presents the page when it starts, so the page never shows verdicts the workflow would refuse."}},{"criterion":"AC4 documentation gate","outcome":"pass","revision":"791822c7f+BPR-1778","evidence":{"kind":"command","detail":"python3 scripts/verify_documentation.py --root . prints DOCUMENTATION_OK."}}]
---

# Approve does nothing after a refused verdict and a new revision

The owner, 2026-10-27, on v0.3.4: "Sometimes I get no feedback or
functionality out of the finish review -> approve button until I
completely exit the app and restart it and then the approve goes to the
expected transition to posting and works."

## Cause

`review_submission_workflow` keeps one in-memory review draft per pull
request. When the PR's binding changes (a new head, or anything else in
the live binding), discovery decides whether the draft goes on at the new
revision or stays frozen on its old one. It froze any draft that
`core::kept` called kept: one with a summary, **or with a verdict other
than Comment**.

Pressing a verdict chooses it first (`choose_disposition`), then submits.
When that submission is refused (GitHub's definite refusal, a changed
preflight, a failed admission), the draft returns to drafting with Approve
still chosen and no summary. When the next revision then arrives, the
draft is frozen: `decided_controls()` turns off Edit and every verdict,
and the page's only words are "The pull request revision changed after
you wrote this summary", though nothing was written. Only Discard or a
restart (which drops the in-memory draft) starts a review on the current
revision, which is the owner's "until I restart".

Separately, `dispatch()` returned without a word on several refusals
(`!allowed`, no draft binding, a changed draft after admission), and
`resolve_held()` started a held-review lookup without re-presenting the
page. During that lookup the page showed verdicts the workflow refused,
and a press did nothing at all.

## Repair

- A draft freezes on its revision only when it holds a summary. A chosen
  verdict is not words to keep: each verdict button chooses itself
  again, and the new draft binds the revision shown, so nothing is
  published on a revision the reviewer did not see.
- Refused gestures re-present the page, with a status where none was set
  ("an earlier review's comments are still being checked" during a
  lookup; "the current account or pull request changed" for a changed
  binding).
- A held-review lookup re-presents the page when it starts.

## If it recurs

If Approve does nothing again, the useful observation is the text under
the verdict buttons on the Finish review page at that moment (or the
Support details export taken then). It tells a frozen summary, a pending
lookup and an authority change apart.
