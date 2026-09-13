---
schema: "implement-rfc.card.v1"
id: "BPR-0750"
title: "Verify and close RFC-BPR-0029"
status: "In Progress"
rfcs: ["RFC-BPR-0029"]
depends_on: ["BPR-0743"]
priority: "P1"
closeout_for: ["RFC-BPR-0029"]
metadata: {"accepted_base":"5d5df7cd79115b9b4b2f8be6ebdb5392458b9e1d","owner_acceptance":"2026-09-12 mandate r17"}
---

# Verify and close RFC-BPR-0029

## Objective and immutable base

Verify the accepted [RFC-BPR-0029](../../rfcs/RFC-BPR-0029-machine-independent-epoch-identity-update-history.md)
against its nine acceptance criteria and the direct surfaces in its verification
matrix. The accepted revision is the immutable baseline for this closeout.

RFC base: `5d5df7cd79115b9b4b2f8be6ebdb5392458b9e1d`.

The owner accepted the revised proposal under mandate r17.
[[BPR-0743]] implements the product work and is Done. RFC-BPR-0028's
epoch-line handle criterion stays on [[BPR-0646]]; its v011 owner verdict
has been consumed and the card is Done. The protected v009/v010/v011
operator packets retained their original bytes during the board move.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| Two independent catalogues reading the same force-push history through the host assign equal identities and deterministic order to the **same evidenced occurrences**. A→B→A yields two distinct identities for A. A missing event never yields a fabricated second A, and the history declares the gap. | Epoch host over two independent catalogues and a recorded force-push timeline; compare exact identities and order. | Pass: composed two-catalogue A→B→A course, 1/1 at d28; exact rows, gap and distinct catalogue incarnations asserted. | `d1bcd1cf` |
| An ordinary live transition yields one occurrence per observed transition, not one per surviving commit. Its portable identity agrees across signed-in, signed-out and recreated catalogues when the same occurrence is unambiguous; ambiguous snapshots remain marked unresolved. | Host and provider fixture with one multi-commit push, observed head transitions, signed-in/signed-out and recreated catalogues. | Partial: host fakes compare the exact ordinary occurrence across three access/catalogue contexts and retain ambiguity; the pager proves three surviving commits supply no inferred events. A coupled multi-commit push and physical catalogue recreation remain unproved. | `ce6f1836` |
| A live read that finds a new head for the same pull request still appends an author-upload epoch, and an imported epoch advances neither the latest live ordinal nor the previous distinct head ordinal. | Second live handoff with a changed head and imported historical occurrence; inspect causes and live ordinals. | Pass: host changed-head author-upload and previous distinct ordinal; composed import leaves live ordinal, previous distinct ordinal and observed live base unchanged. | `2df4d110`, `d1bcd1cf`, `ce6f1836` |
| An update observed live and later overwritten by a force push stays on the install that observed it. The epoch line says when the history it holds may be partial: when the timeline read failed, was refused or was cut off by the page cap, or when a force push overwrote updates this install did not see. | Composed failure, refusal, cap and unseen-force-push cases. | Partial: composed failed/refused/capped courses retain the live epoch and incomplete catalogue state at exact page counts 1/1/10; an observed update survives reuse and an unavailable first historical head leaves a gap. The line's partial wording is not pinned for each course. | `ce6f1836` |
| Live epochs retain their observed base. An imported occurrence with no evidenced historical base shows `unknown`; a computed comparison base is separately labeled and comparison can be unavailable. No imported row claims the current base ref or merge base was its original historical base. | Core, cache and composed presentation cases for absent and rewritten bases. | Partial: composed disconnected historical head shows unknown base and unavailable comparison; stored imported rows keep historical base unknown, and Qt distinguishes derived from observed base. A rewritten selected base is not exercised directly. | `d1bcd1cf`, `ce6f1836` |
| Event dates are labeled GitHub event times, other dates commit dates, and first-seen remains local. Equal timestamps have a deterministic tie-break without a false push-order claim. | Host and presentation cases for mixed dates, equal timestamps and source labels. | Pass: host date-source cases and real workbench Qt labels/tooltip cover event time, commit date, local first-seen, equal-date portable-key order and the explicit no-push-order qualification. | `d1bcd1cf`, `ce6f1836` |
| A current-version catalogue migrates without changing any old epoch row's identity key, incarnation or ordinal. Unresolvable host or duplicate-head provenance remains explicitly unresolved; review-ledger, window-restoration, retained-ref, MCP and agent-launch references to old epochs still resolve. | Copies of current-version duplicate-head and GHES files; inspect local keys, incarnation, ordinals, ledger, restoration, retained refs, MCP and agent bindings. | Partial: v3/v4 copied catalogues retain complete old A→B→A rows, base/head anchors, selectors and unresolved provenance across restart. GHES provenance and the listed post-migration consumers remain unproved. | `ce6f1836` |
| The history read is anonymous REST and stays within a stated page cap, and an open that cannot make the read still produces today's live epoch. | Bounded transport and composed open path, with request counts and failure injection. | Pass: d27 anonymous REST pager bounds and d30 composed failed/refused/capped opens prove exact 1/1/10 timeline requests while retaining today's live epoch. | `2df4d110`, `ce6f1836` |
| BPR-0646's attended handle criterion is asked again, unchanged, on a signed bundle carrying this work. | Digest-bound attended candidate and actual owner verdict, recorded on BPR-0646. | Pass: v011 human accept, exact candidate `BPR-0646-workbench-chrome`, sequence 373, decision `4feeaa9e-5705-4c53-a516-6fc311de305d`, fingerprint `b6641835…`, signed executable SHA-256 `dfd1c4ca…`; hand-back 374 pending none. | `16e242a5`, `67679b1b` |

## Evidence audit, 2026-09-12 (d29, mandate r20)

The row-one composed course is
`application_epoch_portable_occurrence_two_catalogues` in
`build/logs/bpr-0743-d28-expanded-composed.log` (1/1 pass). It compares
three exact evidenced event identities and display order across independent
SQLite incarnations, distinguishes A→B→A, retains the gap, and keeps the
live ordinal at 1. The selected d28 host suite is 8/8 green in
`build/logs/bpr-0743-d28-final-focused-tests.log`. The d27
`github_anonymous_read_contract` and `github_paged_read` selectors passed
individually in `build/logs/bpr-0743-d27-focused-tests.log`, but that
19-selector batch failed three other tests; the corrected host set later
passed 9/9 in `build/logs/bpr-0743-d27-conservative-focused.log`. No whole
d27 batch pass is claimed.

Rows 2 and 4–7 remain open. Row 2 lacks the single multi-commit-push fixture
and an explicit signed-in/signed-out/recreated-catalogue comparison for the
same unambiguous live occurrence. Row 4 lacks the composed failure, refusal,
cap and unseen-force-push matrix. Row 5 lacks a composed missing or rewritten
base case with unavailable comparison asserted. Row 6 has host date and
tie-break tests but lacks the direct presentation source-label and no-false-
push-order check. Row 7 has a SQLite migration copy check but lacks the
specified duplicate-head/GHES copies and post-migration review-ledger,
restoration, retained-ref, MCP and agent-launch resolution. Rows 3 and 8
have the narrower evidence described in the table, so neither is passed.
The ninth row is the owner's actual v011 verdict, recorded on [[BPR-0646]]
and [[BPR-0743]]; the v011 candidate's protection was released after intake
at session sequence 375 without changing its decision.

The closeout remains open for these criteria and for canonical order:
RFC-BPR-0008 is still Proposed ahead of RFC-BPR-0029 in the index. This audit
does not promote RFC-BPR-0029 from Accepted or infer a new owner verdict.

## Direct-check advance, 2026-09-12 (d30, mandate r20)

Commit `ce6f1836` adds the missing host, pager, SQLite, Qt and composed
assertions. Host, SQLite migration, epoch Qt and GitHub pager selectors passed
4/4 in `build/d30-direct-checks.log`; its fifth composed selector initially
failed on fixture page-query order, so that batch is not claimed green. The
corrected ten-launch composed selector passed 1/1 in
`build/d30-composed-correction-test.log` (200.36 s). It checks exact page
counts, three identical portable identities across independent catalogues,
A→B→A, failed/refused/capped reads, disconnected historical head, live/base
ordinals, retained observed update and a later gap. The Qt selector exercises
the workbench date/source and unavailable-comparison presentation.

The milestone integration pass has seven Settings/workbench Qt selectors and
the workbench composition contract passing in
`build/d30-milestone-integration.log`. Its application composition contract
first found a source-manifest digest change and a collision with a separately
running packaged app's live profile. The digest was repinned to the reviewed
source; the packaged self-test and bare render-evidence paths now use a
short-lived private application-data root and assert its creation while
retaining native vault and public-path checks. The repaired application
composition contract passed 1/1 in
`build/d30-automated-root-regression-contract.log` (140.30 s). Earlier red
logs are retained; no single nine-selector green batch is claimed.
`check_boundaries`, the documentation gate, its 64 script tests, touched
clang-format, Python compilation and `git diff --check` pass.

Rows 3, 6 and 8 now pass. Rows 2, 4, 5 and 7 remain partial for the exact
limits in the table. RFC-BPR-0029 stays Accepted and this card stays In
Progress; canonical predecessor RFC-BPR-0008 remains Proposed. The v011
owner verdict is consumed on [[BPR-0646]], so no further attended ask is
pending for this RFC criterion.

## Completion rules

Record executed checks, results and producing commits against each row. Run the
documentation gate and `git diff --check` at closeout, reconcile RFC and index
status, and respect canonical RFC order; an Accepted RFC is not Auto-Verified
merely because implementation or an attended packet exists. No credential or
real pull-request mutation is permitted by this card.
