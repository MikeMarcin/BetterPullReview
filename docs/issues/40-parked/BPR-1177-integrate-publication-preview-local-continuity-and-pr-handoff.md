---
schema: "implement-rfc.card.v1"
id: "BPR-1177"
title: "Integrate publication preview, local continuity and PR handoff"
status: "Parked"
rfcs: ["RFC-BPR-0059", "RFC-BPR-0061"]
depends_on: ["BPR-1161", "BPR-1176"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-08", "RC59-09", "RC61-08"], "pack_alias": "PACK-CYCLE-35", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"RC59-08","outcome":"pass","revision":"875192071","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 review_cycle_publish_desk: after a squashed publication to a simulated GitHub and a reclaim, across a restart, R1-R3 and every captured byte, the frozen round, reply, decision, note and delivery remain and both links; only explicit removal removes them. application_review_cycle_publish_public_path resumes the cycle from Home with its note and links after Open Pull Request. No live GitHub."}},{"criterion":"RC59-09","outcome":"pending","revision":"875192071","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share passes: association from the journal link (#17); description is the Problem/Solution brief a Review Tour maps; only blob/tree/commit/branch/pull writes; no note, reply or unselected file uploaded; no review or comment written; external link read first and marked externally published; Open Pull Request enters the existing workflow. Pending owner: BPR-1177-promotion-scope-v001 (no note promotion in this build; BPR-1273) in session aed7c9e86b31c6f80225924cf9908d59."}},{"criterion":"RC61-08","outcome":"pending","revision":"875192071","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share passes through the profile desktop (application_review_cycle_publish_public_path) and the host course: P, T and R1-Rn named apart, each path in its own words, the ledger of what goes and what stays, linked pull request named. Pending owner: BPR-1177-publish-sheet-v001 in session aed7c9e86b31c6f80225924cf9908d59 on frozen build d292d8ca (875192071)."}}]
---

Pack alias: **PACK-CYCLE-35** (live `BPR-1177`, allocated at import).

# BPR-1177: Integrate publication preview, local continuity and PR handoff

## Outcome and bounded work

Deliver Publish as PR with actual candidate diff, base distinctions, attribution/title/body and explicit confirmation. Preserve pinned local history after linking. Reuse manual problem/solution and optional ordinary-comment Tour preparation; external publication/linking remains truthful.

## Acceptance and direct evidence

No unselected private history in public payload, R0/R1/R2 remain readable after squash, actual PR association, off-hunk promotion uses inherited policy, external link validation and no auto-tour/comment publication.

Criteria: RC59-08, RC59-09, RC61-08.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Implemented — macOS, 2026-10-04

At `875192071` (commits `75523222f`, `484b97265`, `875192071`; follow-up card
[[BPR-1273]] at `524813aba`). Share notes' menu holds **Publish as Pull
Request…**, a sheet over the code. It keeps the base commit, the target as the
checkout last saw it and the cycle's first and latest captures apart, lists
each path in its own words, names the repository, branches, author, title,
Problem, Solution and draft choice, and keeps a ledger of what goes to GitHub
beside what stays on this computer. Review Plan records the finite plan,
Publish Pull Request confirms exactly its digest, Change drops it. Once linked,
the sheet and the page's Updates line name the pull request, and Open Pull
Request hands it to the Home bar, which opens it in the existing workflow. A
pull request published with one's own tools is linked only after a read of
exactly it and is marked externally published (schema 10). Every revision,
round, reply, decision and delivery stays after publication and a reclaim,
until explicit removal. No live GitHub was contacted. Owned paths, checks,
nine negative controls, the compatibility decision on note promotion and the
limits are in the [execution ledger](../../verification/RFC-BPR-0053-0061.md),
under "BPR-1177: publication preview, local continuity and the pull request
handoff".

RC59-08 passes. RC59-09 and RC61-08 have their automated share and wait for
the owner.

## Pending owner judgment

One Accept/Reject question per criterion in a new session,
`aed7c9e86b31c6f80225924cf9908d59` (state in
`/Users/jaeger/.mask/attended-review/owner-review-20261004-publication-v001`),
both ready at sequence 6:

- `BPR-1177-publish-sheet-v001` (RC61-08): does the sheet keep the baselines
  apart, say what goes to GitHub and what stays, and read clearly enough to
  publish from?
- `BPR-1177-promotion-scope-v001` (RC59-09): is it acceptable that this build
  promotes no local note, with promotion left to [[BPR-1273]]?

Candidate: `/Users/jaeger/bpr1177-publish-review-v001.SLIgWZ`, `git archive`
of `875192071` built in its own `source/build/cand`, executable SHA-256
`d292d8caddd15ec4e80e30e8f21d3a67ffe2eaf5ed389dbb9ffb2d94262bc96a`, launched
with `python3 /Users/jaeger/bpr1177-publish-review-v001.SLIgWZ/launch.py`
(signed out, own profile in `home/`, `HOME` and `CFFIXED_USER_HOME` set). Its
media are that build's course rendered natively on cocoa.

## Owed platform runs

Not gating; the commands are in the ledger section above.
