---
schema: "implement-rfc.card.v1"
id: "BPR-0750"
title: "Verify and close RFC-BPR-0029"
status: "In Progress"
rfcs: ["RFC-BPR-0029"]
depends_on: ["BPR-0743"]
priority: "P1"
closeout_for: ["RFC-BPR-0029"]
metadata: {"accepted_base":"5d5df7cd79115b9b4b2f8be6ebdb5392458b9e1d","owner_acceptance":"2026-09-12 mandate r17","milestone_plan":{"id":"rfc-bpr-0029-closeout","path":"docs/rfcs/plans/rfc-bpr-0029-closeout/milestone-plan.json"}}
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

## Current frontier (rfc0029-r1/d1, mandate r1)

The bound milestone is `rfc-bpr-0029-closeout/verify-and-close-rfc-0029`.
The designated producer is `/root/rfc0029_closeout`; native dispatch `d1`
selected the generated `rfc-integrator` role with no per-call model/effort
override, and observed serving telemetry is unavailable.
The candidate starts from the immutable RFC base above and the d30 evidence
below. Rows 2 and 4 pass; rows 5 and 7 remain partial, while the other rows
retain their prior passing evidence. Commits `26fd7f7c`, `72d40b4f`,
`cf4fdbed`, `7c8631a2` and `776bc0d4` hold the plan, composed line,
retained-ref, ordinary-update, changed-base and migrated-binding checks. The twelve-launch composed
public-path selector passed 1/1 (224.24 s) in
`build/rfc0029-rewritten-base-composed-final.log`; its focused two-launch
rewritten-base course also passed in
`build/rfc0029-rewritten-base-focused.log`. The latter advances the selected
base between opens while three imported events retain unknown original
historical bases; an independent review found this is a fast-forward advance,
not the RFC's required base-force-push. A deep worker now owns the public
fixture/test paths for that exact correction. The migrated-binding course passes after a synthetic GHES
handoff and resolves the frozen MCP response's old ordinals and OIDs through
the migrated catalogue. Its agent check constructs the production launch
request but does not start a process; a hostless legacy GHES-origin copy is
still missing. The migration worker and independent reviewer released their
paths. The deep worker's true base-force-push candidate has reproduced a
product failure in `build/rfc0029-true-base-rewrite-focused-candidate.log`:
two public opens succeed, installed Git proves the replacement base is
unrelated and has no merge base with the same imported head, yet that
occurrence keeps its old derived comparison and local ordinal. Its original
historical base stays unknown and the earlier live epoch retains its observed
base. The candidate is uncommitted on the worker's four fixture/test paths;
the same worker is diagnosing a durable correction before any production edit
scope expands. No integrator command is active. No live GitHub or credential
mutation is in scope. RFC-BPR-0008 is still Proposed in canonical order, so
Auto-Verified and Done remain pending.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| Two independent catalogues reading the same force-push history through the host assign equal identities and deterministic order to the **same evidenced occurrences**. A→B→A yields two distinct identities for A. A missing event never yields a fabricated second A, and the history declares the gap. | Epoch host over two independent catalogues and a recorded force-push timeline; compare exact identities and order. | Pass: composed two-catalogue A→B→A course, 1/1 at d28; exact rows, gap and distinct catalogue incarnations asserted. | `d1bcd1cf` |
| An ordinary live transition yields one occurrence per observed transition, not one per surviving commit. Its portable identity agrees across signed-in, signed-out and recreated catalogues when the same occurrence is unambiguous; ambiguous snapshots remain marked unresolved. | Host and provider fixture with one multi-commit push, observed head transitions, signed-in/signed-out and recreated catalogues. | Pass: the host compares the same unambiguous ordinary occurrence across signed-in, signed-out and recreated catalogue contexts; the pager invents no occurrence from surviving commits. A six-launch composed anonymous course then observes three descendant commits in one update, one `author_upload` occurrence at ordinal 2, and exactly the same portable ID after physical catalogue deletion/recreation. Return-head ambiguity remains explicit. The signed-in leg is host evidence rather than a second composed app course. | `ce6f1836`, `cf4fdbed` |
| A live read that finds a new head for the same pull request still appends an author-upload epoch, and an imported epoch advances neither the latest live ordinal nor the previous distinct head ordinal. | Second live handoff with a changed head and imported historical occurrence; inspect causes and live ordinals. | Pass: host changed-head author-upload and previous distinct ordinal; composed import leaves live ordinal, previous distinct ordinal and observed live base unchanged. | `2df4d110`, `d1bcd1cf`, `ce6f1836` |
| An update observed live and later overwritten by a force push stays on the install that observed it. The epoch line says when the history it holds may be partial: when the timeline read failed, was refused or was cut off by the page cap, or when a force push overwrote updates this install did not see. | Composed failure, refusal, cap and unseen-force-push cases. | Pass: the ten-course composed test retains the live epoch and exact 1/1/10 page limits, preserves the earlier observed update, shows the unseen-head gap, and checks the real epoch line says `Incomplete history` and `display order is not push order` in every course. | `ce6f1836`, `26fd7f7c` |
| Live epochs retain their observed base. An imported occurrence with no evidenced historical base shows `unknown`; a computed comparison base is separately labeled and comparison can be unavailable. No imported row claims the current base ref or merge base was its original historical base. | Core, cache and composed presentation cases for absent and rewritten bases. | Partial with reproduced failure: missing-base and base-advance courses preserve unknown historical truth, but a new uncommitted true base-force-push candidate shows the same imported occurrence keeps its old derived comparison and local ordinal after the new selected base has no common ancestor. The original live base is retained. Correct and rerun this direct path before Pass. | `d1bcd1cf`, `ce6f1836`, `7c8631a2`; red candidate `build/rfc0029-true-base-rewrite-focused-candidate.log` |
| Event dates are labeled GitHub event times, other dates commit dates, and first-seen remains local. Equal timestamps have a deterministic tie-break without a false push-order claim. | Host and presentation cases for mixed dates, equal timestamps and source labels. | Pass: host date-source cases and real workbench Qt labels/tooltip cover event time, commit date, local first-seen, equal-date portable-key order and the explicit no-push-order qualification. | `d1bcd1cf`, `ce6f1836` |
| A current-version catalogue migrates without changing any old epoch row's identity key, incarnation or ordinal. Unresolvable host or duplicate-head provenance remains explicitly unresolved; review-ledger, window-restoration, retained-ref, MCP and agent-launch references to old epochs still resolve. | Copies of current-version duplicate-head and GHES files; inspect local keys, incarnation, ordinals, ledger, restoration, retained refs, MCP and agent bindings. | Partial: v3/v4 copied catalogues retain complete old A→B→A rows, anchors, selectors and unresolved provenance across restart; saved window positions and ledger binding resolve old anchors. A copied-v4 real-cache course proves physical retained refs. A synthetic GHES handoff and frozen MCP response preserve old ordinals/OIDs through the migrated catalogue; production agent supplement and launch-request framing preserve the same binding. A direct copied hostless legacy GHES-origin row and actual agent process consumption remain unproved. | `ce6f1836`, `26fd7f7c`, `72d40b4f`, `776bc0d4` |
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

## Direct-check advance, 2026-09-12 (rfc0029-r1/d1, mandate r1)

Commit `26fd7f7c` adopts the bounded `rfc-bpr-0029-closeout` plan, captures
the epoch line's actual history text in the self-test probe, and asserts it
through the ten-course composed public path. The network-isolated selector
`application_epoch_portable_occurrence_two_catalogues` passed 1/1 in
`build/rfc0029-d1-composed.log` (191.38 s). Failed, refused, capped and
unseen-force-push courses now assert `Incomplete history` and the no-push-order
qualification from the real line, alongside the existing retained live row,
gap, page count and catalogue assertions. Row 4 passes.

The same commit extends the copied v3/v4 SQLite migration course with three
persisted restoration positions and a ledger agent-session binding. Each old
base/head identity resolves to its original commit immediately after migration
and after restart; `sqlite_epoch_catalog_behavior_contract` passed 1/1
(40.88 s). Row 7 remains partial because the catalogue's opaque host field
does not prove GHES provenance and this course does not open physical retained
Git refs, MCP or an actual agent launch. Existing host and pager selectors
passed 2/2, but they do not couple the three surviving commits to one
multi-commit ordinary push across access modes and physically recreated
catalogues; row 2 remains partial. Row 5 still lacks a direct rewritten-base
course. The app build, documentation gate, touched clang-format and
`git diff --check` passed. The initial app compile error on `QTextStream` was
corrected before the passing build. RFC-BPR-0008 remains Proposed ahead of
this RFC, so this card and RFC retain In Progress and Accepted status.

Commit `72d40b4f` adds a real Git-cache integration check: after a live
append, it closes the catalogue, copies and converts only the catalogue to
the exact prior v4 schema, then migrates and restarts it. The original epoch
key and both exact catalogue anchors still match the physically retained Git
ref pair in the unchanged original cache. `epoch_maintenance_integration`
passed 1/1 (38.95 s). Row 7 still needs a GHES-origin copy and post-migration
MCP and launched-agent checks.

Commit `cf4fdbed` adds synthetic packs with three descendant commits after
one previously observed head and a composed anonymous fixture that reads
them as one update with no force-push timeline event. The network-isolated
`application_epoch_ordinary_update_recreated` selector passed 1/1 (63.06 s)
in `build/rfc0029-ordinary-test.log`; it checks one `author_upload` observed
occurrence at ordinal 2, the same portable ID
`3091B0E888E3321843734F3822628C4C6B6DA848190B07ABD7DD3AD54E755B30`
after physical catalogue deletion/recreation with different incarnations,
and explicit ambiguity on a return to the first head. Together with the
already passing host cross-mode identity and pager no-invention checks, row 2
passes. The signed-in cross-mode leg is host evidence; the existing signed-in
app course assumes `force_push_rebase` and was not repurposed for this
ordinary update. The correction from Python `assert` to an explicit failure
helper passed an optimized-Python negative control, so the acceptance checks
remain active under `python -O`. The first two fixture-data attempts failed
on duplicate commit node IDs and inconsistent PR commit count; the corrected
six-launch course passed. These failures are retained in
`build/rfc0029-ordinary-test-{first,count}-failure.log`.

Commit `7c8631a2` adds a changed-selected-base course to the composed
public path. A first open records one live epoch; the second open advertises
a different base, fetches a disconnected historical head and imports three
event occurrences. The focused two-launch run passed in
`build/rfc0029-rewritten-base-focused.log`, and the full twelve-launch
`application_epoch_portable_occurrence_two_catalogues` selector passed 1/1
(224.24 s) in `build/rfc0029-rewritten-base-composed-final.log`. The copied
catalogue has a new live base and three imported rows whose original
historical bases all remain null. The earliest disconnected event has no
derived base or comparison ordinal, while the later events may have separately
derived bases. Independent read-only review found the new base is a direct
descendant of the original base in the manifest: this course exercises a
fast-forward base advance, not the RFC's required base-force-push. The first
open's empty timeline also leaves no imported occurrence to compare across
the change. Row 5 remains partial pending that direct check. The first full run failed because the fixture rejected the valid
two-head fetch when the selected base changed; after that fixture-only fix,
the focused run caught an assertion that incorrectly treated a live-observed
row as an imported row. The corrected assertion distinguishes the two kinds.
The first red log remains at `build/rfc0029-rewritten-base-composed.log`.
The independent reviewer re-ran `epoch_maintenance_integration` 1/1
(38.86 s), found no product defect and upheld row 7's Partial boundary.

Commit `776bc0d4` extends the real-cache copied-v4 migration course. The
pre-migration GHES 3.18 handoff's authority is checked against the catalogue
key, and a frozen MCP binding is copied with the candidate. After migration
and restart, the production MCP `snapshot_binding` response is decoded and
its old ordinals and commit OIDs resolve through the migrated SQLite session;
a modified ordinal is refused. The production agent supplement and launch
request carry the same old binding. `epoch_maintenance_integration` passed
1/1 (37.39 s) on the committed candidate in
`build/rfc0029-d1-migration-mcp.log`; the first test revision failed on
omitting the GHES server contract and a later compile attempt used the wrong
Qt JSON array API, both corrected before this pass. This check does not run an
agent process. The v4 GHES course has occurrence evidence, whereas the RFC's
matrix asks for an old hostless GHES row; the safe copied-v4 helper refuses to
discard that evidence. The existing hostless-legacy migration test proves
generic unresolved provenance but does not prove GHES origin. Row 7 remains
partial with that concrete missing fixture and process-consumption boundary.

## Completion rules

Record executed checks, results and producing commits against each row. Run the
documentation gate and `git diff --check` at closeout, reconcile RFC and index
status, and respect canonical RFC order; an Accepted RFC is not Auto-Verified
merely because implementation or an attended packet exists. No credential or
real pull-request mutation is permitted by this card.
