---
schema: "implement-rfc.card.v1"
id: "BPR-1188"
title: "Verify and close RFC-BPR-0055"
status: "Parked"
rfcs: ["RFC-BPR-0055"]
depends_on: ["BPR-1187"]
closeout_for: ["RFC-BPR-0055"]
parked_on: "human-review"
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "closeout", "acceptance": ["RC55-01", "RC55-02", "RC55-03", "RC55-04", "RC55-05", "RC55-06", "RC55-07", "RC55-08", "RC55-09", "RC55-10"], "pack_alias": "PACK-CYCLE-46", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-46** (live `BPR-1188`, allocated at import).

# BPR-1188: Verify and close RFC-BPR-0055

## Outcome and bounded work

Reconcile every criterion of RFC-BPR-0055 against its producing revision and direct evidence. Verify canonical predecessor closure and the imported ownership/remap. Leave unmet native/live/user gates open; do not relax criteria, recycle IDs or alter historical predecessor acceptance to close this RFC.

## Acceptance and direct evidence

Run the live documentation/board gates and record criterion/check/result/revision/environment. Close only this RFC through closeout_for; a complete pack validator or another RFC result is not substitute evidence.

Criteria: RC55-01, RC55-02, RC55-03, RC55-04, RC55-05, RC55-06, RC55-07, RC55-08, RC55-09, RC55-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

RFC base: `8466534f96f254d88fed412a587188b7b45f2580`.

All owned acceptance criteria remain **Not run** at intake.
## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| **RC55-01:** Author range/file/module/cycle notes and one multi-location discussion from full source or a diff without mandatory per-item metadata. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: range, file, module and cycle notes and one discussion with a related range, from retained full source, with only a body; the native authoring surface through the window; macOS, Linux, Windows | BPR-1153 `e5688006c`, BPR-1156, BPR-1157 |
| **RC55-02:** Original source and ranges remain immutable; current exact/ambiguous/deleted/unavailable correspondence is separately represented. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: the original attachment never rewritten; `exact_material`, `exact_mapped`, `ambiguous`, `deleted` and `unavailable` kept apart; forged or live-only bytes refused; all three | BPR-1153 `e5688006c` |
| **RC55-03:** Plain and all twelve shared labels preserve authored Markdown and do not imply patches, severity, permissions or verification. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass (2026-10-06): the cycle share as before, and now read through the shared BPR-1076 recognizer: every stored and disclosed cycle body gets the grammar's status and label (twelve in catalog order, Plain, malformed, quoted, code, unknown, CRLF, empty, emoji/NUL), spans index the authored bytes, and no recognized label or expectation changes suggestion, lifecycle or provenance. macOS arm64 core-only; the row is pure core | BPR-1154 `f269f88c3`; BPR-1076 `dca6d2772`; BPR-1188 `e82c50f75` |
| **RC55-04:** Suggestion authoring preserves original/replacement bytes, insertion/deletion, explicit intent, byte ranges and before/after preview; an in-place edit of a file becomes one exact suggestion per separate changed run of lines (owner amendment, 2026-10-01). | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: exact bytes, insertion and deletion, intent, byte ranges, in-memory preview; the 2026-10-01 amendment, one suggestion per separate changed run; all three | BPR-1155 `29c8262dc`; BPR-1156 `da459c4e1` |
| **RC55-05:** Exact application refuses stale whole-blob preconditions and overlapping/same-boundary edits; nonoverlapping edits use original coordinates. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: original-coordinate application of out-of-order edits; overlapping, contained, identical and same-boundary edits conflict and apply nothing; `stale_blob`; all three | BPR-1155 `29c8262dc` |
| **RC55-06:** Editing a suggestion does not write the checkout; actually applied edits are observed/labeled instead of dispatched for double application. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: suggesting, previewing and preflighting leave every workspace byte unchanged; an exact later result is `observed_already_applied` and preflight reports `already_applied`. No workspace application exists; all three | BPR-1155 `29c8262dc` |
| **RC55-07:** Native IME, undo, multi-caret/virtual-space and clipboard workflows preserve exact text and selections in the suggestion composer. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass with a judgment for the owner: the owner accepted `BPR-1156-editor-gestures-v004` (event 790, IME included) and `native-keyboard-v002` on macOS; the 15-row selection passes on all three, offscreen on Linux and Windows. Windows' native IME and `MSDEVColumnSelect` clipboard were not exercised on a desktop | BPR-1156 `fd5ca4412`, `caae4cc9e`; BPR-1157 `fa9f42381`; BPR-1182 |
| **RC55-08:** Human, adapter-observed, self-reported and unknown provenance remain distinct and private unless selected for disclosure. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: human, adapter-observed, self-reported and unknown revisions distinct; only current wording disclosed by default, the rest only when named; all three | BPR-1154 `f269f88c3` |
| **RC55-09:** Whole-round preparation preserves the reviewer wording and suggestions without a required model call, summarizer or second combined prompt. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: the composer hands exact bytes, intent and rationale with no model, summarizer or second prompt; rounds freeze the selected wording and suggestion bytes deterministically (BPR-1158) | BPR-1156, BPR-1158 |
| **RC55-10:** Cycle threads reuse the shared narrow private-thread/lineage seam and do not fork or falsely complete deferred RFC0023. | Focused production-path checks and applicable native or service course at closeout | Reconciled, pass: cycle threads use the shared `private_thread_lineage`; `core_private_review` unchanged; relabel and undo are successor revisions; RFC-BPR-0023's pull-request scope untouched; all three | BPR-1153 `e5688006c`, BPR-1154 `f269f88c3` |

## Closeout reconciliation (2026-10-04): not closable

BPR-1187 closed RFC-BPR-0054, so this card's only card dependency is met and
the reconciliation ran before promotion. The card was not promoted: it cannot
close, and it stays in Backlog.

**RFC dependencies.** RFC-BPR-0055 depends on RFC-BPR-0053 and 0054
(Auto-Verified) and on RFC-BPR-0036 (native Markdown), RFC-BPR-0047
(Conventional Comments semantics) and RFC-BPR-0048 (feedback authoring), which
are all Proposed. RFC-BPR-0047 and 0048 are under the owner's 2026-09-24
start hold for RFC-BPR-0044 to 0051 (session `cbd48b91…`, decision
`1d9104f8`), which the 2026-09-26 and 2026-09-27 amendments left in force.
RFCs close when their explicit dependencies are satisfied, so this RFC cannot
close until those three close or the owner amends the dependency, as the r2
closeout amendment did for RFC-BPR-0039 to 0043.

**Criteria.** Eight rows pass on their cards' recorded evidence, with runs on
macOS arm64, `bpr-main-linux` and `bpr-main-win`; the
[ledger](../../verification/RFC-BPR-0053-0061.md#bpr-1188-rfc-bpr-0055-reconciled-not-closed-2026-10-04)
judges each. RC55-03 is open: it names "all twelve shared labels", and the
shared recognizer is [[BPR-1076]] of RFC-BPR-0047, in Backlog under the same
hold; BPR-1154 kept the raw body authoritative so that recognizer reads the
same bytes, and deferred the whole judgment to it. RC55-07 passes on the
owner's macOS acceptance and offscreen rows elsewhere, which is a judgment
the coordinator should put to the owner.

To close: the owner releases or amends the hold so RFC-BPR-0036, 0047 and
0048 close (or amends RFC-BPR-0055's dependency on them), BPR-1076 lands so
RC55-03 can be judged whole, then promote this card, rerun the documentation
gate and close the RFC. The other rows need no rerun unless their inputs
change.

## Closeout reconciliation (2026-10-06): criteria met, dependencies not; parked

The owner lifted the RFC-BPR-0044–0051 start hold on 2026-10-05, and
[[BPR-1076]] is Done, so the reconciliation ran again on `origin/main`
`2e89f8cfc`.

**Criteria: all ten pass.** RC55-03 was the one open row. `e82c50f75` adds
`reads_labels_through_the_shared_recognizer` to `core_review_cycle_disclosure`:
cycle notes are created, disclosed and read by `recognize_feedback_prefix`.
Each of the 21 bodies gets the status and label the grammar gives it, and
every span falls inside the authored bytes. Stored and disclosed bytes read
the same. Nothing the recognizer reads, including `issue (blocking)` and a
`verified` decoration, changes the note's suggestion, lifecycle or
provenance. A negative control fails the row: changing the expected
`unknown_label` reading to `plain` fails with "shared recognizer reads the
authored label". Passing on macOS arm64 core-only Debug:
`core_review_cycle_disclosure`, `core_review_cycle_feedback` and
`core_feedback_convention`. clang-format and clang-tidy are clean on the
touched file. The other nine rows rest on unchanged evidence (RC55-07 keeps
its recorded judgment).

**Dependencies: not satisfied.** RFC-BPR-0055 depends on RFC-BPR-0053 and
0054 (Auto-Verified) and on 0036, 0047 and 0048, which are Proposed:

- RFC-BPR-0036: implementation accepted, and [[BPR-0917]] is Done. All its
  own dependencies (0005, 0006, 0008) are now Auto-Verified. The status-only
  close that BPR-0917 left for later has not been done, and no open card owns
  it.
- RFC-BPR-0047: [[BPR-1093]], [[BPR-1096]] and its closeout [[BPR-1097]] are
  in Backlog. It also depends on RFC-BPR-0021, which is **Deferred**, so it
  cannot close without its own amendment or 0021's activation.
- RFC-BPR-0048: [[BPR-1084]] is In Progress. [[BPR-1085]], [[BPR-1086]],
  [[BPR-1088]], [[BPR-1093]], [[BPR-1095]] (live GitHub interoperability),
  [[BPR-1096]] and its closeout [[BPR-1098]] are in Backlog. It depends on
  0047 and 0036.

RFCs close only when their explicit dependencies are satisfied. This card
therefore parks on one owner question, `BPR-1188-dependency-v001` in
attended session `cbd48b91000b00f1e72e1ad62c7c384c`.

**Question.** Close RFC-BPR-0055 now under a dependency amendment, or wait
for the RFC-BPR-0036, 0047 and 0048 closeouts?

**Recommendation: amend and close.** No RC55 criterion needs what is still
open in these three RFCs. RFC-0055 takes three things from them, and all
three have landed:

- the label recognizer from 0047 ([[BPR-1076]], Done), which RC55-03 above
  reads directly;
- the shared label authoring controls from 0048 ([[BPR-1082]] and
  [[BPR-1083]], Done);
- the native Markdown reader from 0036 (implementation accepted).

What remains open in 0047 and 0048 is GitHub native suggested changes,
review decisions, tracking and tour comments, preferences, live
interoperability and their verification. A cycle's suggestions are exact
local edits under RC55-04 to RC55-06. Waiting would hold RFC-BPR-0056 to
0061 and BPR-1189 to BPR-1194 behind 0048's live GitHub card and behind
0047's Deferred RFC-0021 dependency. An amendment would remove 0036, 0047 and
0048 as closure gates for this RFC only and leave 0036's status-only close as
a separate follow-up. It would not mark those RFCs closed.

**On an amend verdict:** record the amendment in the index and in this
RFC's dependency note, then promote, claim and finish this card. Set the RFC
and index row to Auto-Verified and rerun DOCUMENTATION_OK. **On wait:** move
this card to Backlog and leave it there until 0036, 0047 and 0048 close.

## Owner answer (2026-10-06): amend and close

The coordinator put the dependency question to the owner in chat: "RFC-0055
passes all 10 criteria but formally depends on RFC-0036/0047/0048 … Amend and
close / Wait". The owner answered, verbatim:

> Amend and close (Recommended)

The answer came in chat, not through attended review. The unanswered
candidate `BPR-1188-dependency-v001` in session
`cbd48b91000b00f1e72e1ad62c7c384c` was released with `resume-development
--candidate BPR-1188-dependency-v001` (event 960, no decision recorded) and
marked superseded by this answer with an `observe` note (event 961). Its host
was not started, and nothing was added to that session.

The amendment is recorded in RFC-BPR-0055's
[owner amendment](../../rfcs/RFC-BPR-0055-source-feedback-and-exact-suggested-edits.md#owner-amendment-2026-10-06-dependencies-are-what-this-rfc-uses)
section and in the [RFC index](../../rfcs/README.md): RFC-BPR-0055 depends on
what it uses ([[BPR-1076]], [[BPR-1082]], [[BPR-1083]] and the native Markdown
reader), not on the closure of RFC-BPR-0036, 0047 and 0048. Its `depends_on`
is now RFC-BPR-0053 and 0054, both Auto-Verified. Those three RFCs are not
closed by the amendment.

