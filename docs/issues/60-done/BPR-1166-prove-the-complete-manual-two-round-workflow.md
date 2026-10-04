---
schema: "implement-rfc.card.v1"
id: "BPR-1166"
title: "Prove the complete manual two-round workflow"
status: "Done"
rfcs: ["RFC-BPR-0056", "RFC-BPR-0057", "RFC-BPR-0061"]
depends_on: ["BPR-1161", "BPR-1163", "BPR-1165"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "manual-iteration", "acceptance": ["RC56-03", "RC57-01", "RC61-07"], "pack_alias": "PACK-CYCLE-24", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC56-03","outcome":"pass","revision":"db3f749f50b6662e7210c9fa269ca71798be7a9d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share, macOS arm64 application_review_cycle_two_round_public_path in the application's own profile desktop: for F1 and F2 the clipboard bytes, the saved RC-1-Fn.md, the bundle's review.md and bpr cycle export --output - are identical; the bundle envelope's payload hashes canonically to the digest show lists; a granted harness's 4 KiB paged read (direct for F1, forwarded to the open desktop for F2) names the same payload_sha256 and holds every item once in payload order (8 with 2 suggestions; I5, I6, I10); no scratch note or local path in any channel. Negative control: a bundle document one byte longer fails. Direct adapter delivery does not exist in this build; its parity is RC60-07 (BPR-1180), checked at BPR-1189."}},{"criterion":"RC57-01","outcome":"pass","revision":"db3f749f50b6662e7210c9fa269ca71798be7a9d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Same course: after a restart R2 and after another R3 are captured only by Capture revision, with no response file, report, receipt, callback or BPR-managed agent (R3 edits committed with Git, R2 edits not); reopening captures nothing; the only response is one prose reply entered by hand. Negative control: a capture that waits for a response fails at the first capture."}},{"criterion":"RC61-07","outcome":"pass","revision":"db3f749f50b6662e7210c9fa269ca71798be7a9d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Same course: a pasted prose reply shows beside its note as a claim and captures nothing; This round for F1 and F2, All cycle changes R1->R3 and chosen pairs R1->R2, R2->R3 match course-computed diffs row by row (13 and 22 changed rows) and are drawn; verification records only the revision shown (R2, then R3); Next round (2 carried, 2 added, 6 resolved) and Save to File compose and freeze F2; no callback or protocol. Negative control: This round from the first round's start fails F2's pair. Passes offscreen in the network-deny sandbox and natively on cocoa."}},{"criterion":"BPR-1166:boundary","outcome":"pass","revision":"db3f749f50b6662e7210c9fa269ca71798be7a9d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS 26.6.2 arm64, Apple clang 21, Qt 6.11.2, build/dev-tour-cli: integration pass 52/52 (two-round, desktop, handoff, cycle CLI, skill, review_cycle_, qt_review_cycle_view_, core_review_cycle_, sqlite storage, network isolation manifest, architecture_boundaries, round-trip composition pin); coding-format-check passes; no product source changed. Linux and Windows runs owed with the card's commands."}}]
---

Pack alias: **PACK-CYCLE-24** (live `BPR-1166`, allocated at import).

# BPR-1166: Prove the complete manual two-round workflow

## Outcome and bounded work

Compose the real desktop path from source selection through eight notes/two suggestions, Copy, external edits, restart, Capture, diff, verify, Save next round and another capture. Keep optional agent components unavailable throughout this course.

## Acceptance and direct evidence

Record native interactions, producing build, frozen payloads and source hashes. Show no callback/report dependency, no lost notes, no hidden AI activity and correct R0/R1/R2 comparisons; this is the first end-to-end value milestone.

Criteria: RC56-03, RC57-01, RC61-07.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Implemented — macOS, 2026-10-03

Promoted to Ready at `009516352` under the 2026-10-03 mandate amendment (an
eligible, dependency-complete Backlog card may be promoted) and claimed at
`edbf93d46`. The course is `application_review_cycle_two_round_public_path`
(`600e5062e`, `db3f749f5`); the full record, with commands, source hashes,
negative controls and limits, is in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md) under
"BPR-1166: the complete manual two-round workflow".

Owned paths: `tests/end_to_end/review_cycle_two_round_public_path_tests.cpp`,
`tests/end_to_end/review_cycle_two_round_contract.py` and their row in
`tests/end_to_end/CMakeLists.txt`. No product source changed and no `src/app`
file was added; the round-trip composition pin passes unchanged.

The reviewer works in the application's own profile desktop, restarted twice on
one profile, with window-system gestures: Open Project on a Git checkout with
uncommitted work and a CRLF file, eight notes across four files with an exact
and an illustrative suggestion, a related location and a scratch note left out;
Copy, Save to File and Save as Bundle freeze and share F1; the shipped
`bpr cycle export` and a granted harness's paged read (as the bpr-cycle skill
teaches) give the same canonical round; edits made outside with nothing sent
back; restart, Capture revision, F1's This round, a pasted prose reply,
verification, Needs work, Withdraw, a new note, Next round, Save to File freezes
F2, with export and the harness read forwarded to the open desktop; a committed
second set of edits; restart, Capture revision, This round for F2, All cycle
changes and two chosen pairs, each checked row by row against diffs the course
computes, and the last notes verified. No agent route, no PATH canary run, no
child process, no write to the folder, no note lost.

Passes at `db3f749f5` under the macOS network-deny sandbox (offscreen) and
natively on cocoa with renders. Three product negative controls fail it: a
capture that waits for a response, This round always from the first round's
start, and a bundle document one byte longer. Integration pass: 52 of 52 cycle,
CLI, handoff, skill, guard and pin rows.

No criterion needs the owner, so no attended session was opened. Found and
recorded, not changed: [[BPR-1272]] (a terminal export is worded "Printed in a
terminal") and a width-floor note on [[BPR-1182]].

### Owed platform runs at `db3f749f5`

Not gating. Linux station (`bpr-main-linux`):

```sh
git pull --rebase --autostash && cmake --preset dev -B build/dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON && cmake --build build/dev-tour-cli && ctest --test-dir build/dev-tour-cli -j4 --output-on-failure -R "^application_review_cycle_two_round_public_path$|^application_review_cycle_desktop_public_path$|^application_profile_cycle_handoff_public_path$|^cycle_cli_public_path$|^test_network_isolation_manifest$|^architecture_boundaries$"
```

Windows station (`bpr-main-win`, Debug):

```powershell
git pull --rebase --autostash; cmake --preset windows-dev -B build/windows-dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON; cmake --build build/windows-dev-tour-cli --config Debug; ctest --test-dir build/windows-dev-tour-cli -C Debug -j4 --output-on-failure -R "^application_review_cycle_two_round_public_path$|^application_review_cycle_desktop_public_path$|^application_profile_cycle_handoff_public_path$|^cycle_cli_public_path$|^test_network_isolation_manifest$|^architecture_boundaries$"; cmake --build build/windows-dev-tour-cli --config Debug --target coding-format-check
```

The new row is registered contained. If Windows stops it inside the container
as it stops its sibling handoff course ("owner startup succeeds"), pin it
`WINDOWS_UNCONTAINED` with the measured line in
`scripts/verify_test_network_isolation.py`, rerun it uncontained and record
both here as a note; Done is not reopened.
