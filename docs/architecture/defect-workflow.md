# Defect Workflow

BetterPullReview restores inspection-style defect semantics as a **local
projection over ordinary GitHub review threads**. A defect is a thread someone
classified. The thread carries no marker, no label, and no metadata saying so,
so its shared representation remains an ordinary GitHub thread. Whether its
prose gives an unfamiliar collaborator enough context remains unobserved.

## The rule the rest of this document serves

**A fix claim is not a verification.** An author saying "fixed in abc123", code
moving beneath the defect, and someone resolving the thread in GitHub are all
*evidence*. Each produces `needs verification`. None produces `verified`.

Only an explicit reviewer intent verifies, and that rule is enforced
independently in all three layers — the core refuses it, the host refuses it,
and the Qt card refuses it — because a rule enforced in one layer is one the
other two can route around.

## Ownership

| Region | Owns |
|---|---|
| Functional core | Classification, the lifecycle, and what evidence may conclude |
| Application host | Correlating threads, replies, and epochs with defect records |
| Qt adapter | Defect cards, badges, evidence, and the reviewer's explicit actions |

The core reaches no thread and issues no request. The host correlator is a pure
function with no provider. The Qt card holds one dispatch callback and nothing
else. In each case the constraint is structural: there is no path to violate,
rather than a path guarded by a check.

## Typed vocabulary

- **Command** — `core::defect_command`: claim fix, verify, reopen, withdraw,
  accept as is, reclassify.
- **Decision** — `core::defect_outcome`, carrying the resulting record, a typed
  rejection, and whether anything changed.
- **Observation** — `core::defect_evidence` and
  `application::defect_thread_observation`.
- **Delta** — the updated `defect_record` a projection presents.

## State machine

```text
                 claim_fix
        open ──────────────► fix_claimed ──┐
         │                                 │ verify
         │ evidence                        ▼
         └──────────► needs_verification ─► verified
                              ▲                │
                              │ evidence       │ reopen
                              └── reopened ◄───┘

        withdraw / accept_as_is  →  withdrawn, accepted_as_is  (terminal)
```

`fix_claimed` and `needs_verification` both mean "somebody says this is done"
and neither is `verified`. They stay distinct because their *source* differs —
a person asserting the fix, versus a heuristic noticing a change — and a
reviewer needs to know which.

Verifying is legal only from those two states: verifying a defect nobody has
addressed would record a judgement about work that has not happened. Reopening
is legal only from `verified`, for the mirrored reason.

`withdrawn` and `accepted_as_is` refuse every further command. Reopening one is
a new defect rather than a transition, so an earlier decision is not
overwritten by a later change of mind.

## Evidence

Three kinds, all suggestions:

| Kind | What happened |
|---|---|
| `author_reply` | The author replied on the thread |
| `nearby_code_change` | The comparison the defect is anchored to moved |
| `thread_resolved_externally` | Someone resolved the thread in GitHub |

The third is the one worth stating twice. Resolving a thread in GitHub is
exactly what a verification looks like from outside — which is precisely why it
must not verify. The reviewer who owns the defect has said nothing.

Evidence against a decided defect changes nothing, and that is not an error:
things happen near settled defects. Further evidence on a defect already
awaiting the reviewer changes nothing either. More evidence does not ask
harder.

## Shared mapping

Publishing a defect creates an ordinary actionable review comment. Replies are
ordinary replies. A user may write prose such as `Fixed in <commit>`, and that
is prose — the tool reads no meaning out of it beyond treating a reply as
evidence.

Verification normally corresponds to a reviewer reply and optionally resolving
the thread. Reopening a verified item can unresolve the thread through a
**separate explicit action**; it never rides along with another.

Classifying an existing thread mutates nothing shared. The correlator has no
provider, so there is nothing that could issue a request; publishing and
resolving go through RFC-BPR-0008's mutation coordinator, which a user reaches
by a distinct action.

## Severity, category, and privacy

Severity and category are **local enrichment**. They never appear in the
published comment.

This is not a limitation to be worked around. A defect whose meaning lived in a
severity field would be unreadable to every collaborator not using this tool,
and the RFC's non-goals forbid hiding defect meaning in metadata unavailable to
GitHub-only users. The published prose must stand on its own.

The Qt card labels them *local classification* rather than showing them bare,
so a reviewer does not assume the person they are talking to has seen them.

## Anchors and history

Anchors are RFC-BPR-0004's semantic source-line identities. A defect that
remembered a visual row would point at the wrong code as soon as anything above
it moved.

A defect outlives the code it was filed against. When the anchor can no longer
be resolved, `defect_history` records whether it is `outdated` (the code moved
beyond where GitHub will show the comment) or `thread_deleted` (the shared
conversation is gone) — different facts with different next steps.

Two things are guaranteed when that happens:

**The lifecycle state does not move.** An anchor going out of date is a fact
about the code, not a judgement about the defect. Quietly withdrawing a defect
whose code vanished would discard a reviewer's finding because a file was
renamed.

**The original anchor is retained.** A defect whose context vanished still has
to be answerable; "somewhere in this file, once" is not an answer.

There is deliberately **no field for a substitute anchor**. Repairing a lost
anchor with a nearest-line guess would attach the defect to code nobody filed
it against, and the reviewer would have no way to tell — it would look
correctly placed while pointing at something unrelated. The absence of the
field is the guarantee.

## User-facing labels

| State | Badge | What the card says |
|---|---|---|
| open | `[open]` | Nobody has addressed this yet. |
| fix claimed | `[fix claimed]` | The author says this is fixed. You have not checked it. |
| needs verification | `[needs verification]` | Something suggests this was addressed. You have not checked it. |
| verified | `[verified]` | You checked this and accepted the fix. |
| reopened | `[reopened]` | You reopened this after verifying it. |
| withdrawn | `[withdrawn]` | You withdrew this. |
| accepted as is | `[accepted as is]` | You accepted this as it stands. |

Badges are textual and pairwise distinct, and lead the label, so a state reads
in greyscale and through a screen reader. Each carries a sentence as well,
because `[needs verification]` gives a reviewer the label but not what is being
asked of them.

Actions are named for what they do to *this* defect — "Verify this fix",
"Reopen this defect" — because a `QAction` carries no accessible name of its
own and assistive technology reads its text.

## Shipped composition

The workbench now composes this policy over the accepted pull-request snapshot.
Selecting an ordinary review-thread row and choosing **Classify selected
conversation as defect** binds the exact thread identity, selected file, and
comparison digest to a private `durable_defect_record`. The application rejects
file-level, outdated, non-current-endpoint, out-of-bounds, and mismatched
path/side/line selections rather than inventing an anchor. A current GitHub
right-side thread must match the accepted head and the exact rendered
comparison before it is classifiable. The application derives two independent
positive 63-bit values from SHA-256 of the canonical thread identity, persists
both in the existing conversation/defect identity columns, and requires both
on every correlation. This collision-resistant full identity prevents a
single lossy integer from rebinding a durable defect after its original thread
disappears.

Schema-v5 ledger migration marks every pre-BPR-0414 defect anchor outdated.
Those legacy rows stored a one-based line but did not retain enough provenance
to distinguish a current right thread from a forced-right left thread, an
outdated original line, or an unrelated retained comparison. Reconciliation
restores availability only after the accepted snapshot and rendered comparison
re-prove the exact file, right-side current line, comparison digest,
accepted-head endpoint, and line bound. Until then the immutable history stays
visible but unavailable and cannot enter an agent supplement as current code.
Losing that proof is fail-closed in the live projection: reconciliation adopts
`outdated` or `thread_deleted` in memory before republishing even when the
ledger write fails. A later restart reloads the durable row, rederives the same
safe state, and can retry persistence; an I/O failure cannot keep a stale
anchor placeable.

Author replies and provider-reported outdated state enter the pure correlator
as evidence. Outdated state names the defect's exact semantic source-line
anchor; sharing a comparison digest is insufficient. Merely browsing another
retained epoch produces no evidence. An accepted correlation is persisted
before the card moves, and no evidence path has a `verified` outcome. Verify,
reopen, withdraw, accept-as-is, and local reclassification leave the visible
card as semantic commands; the core decides them and the host persists an
accepted change before re-projecting it.

The conversations drawer owns a stable-ID ordered defect queue with an exact
position/count label and previous/next actions. Lost shared context remains
visible on the card while the immutable original semantic anchor stays in the
ledger. Restart loads the same two-level authority—ordinary GitHub thread as
shared state, private classification/history as local state—with no repair or
write when the schema and durable rows are already current.

The registered `application_defect_composition_contract` runs against both the
GitHub.com and GHES 3.18 profiles. It drives the shipped file row, line-thread
and file-thread conversation rows, epoch selectors, defect actions, and the
separate review Publish action. Its seed/reopen trace proves that file-level
classification is refused, retained-epoch browsing does not create evidence,
local classification causes zero fixture mutations, a refused ledger update
cannot keep a reversed-comparison anchor available in memory, and the durable
multi-defect projection returns unchanged after restart. RFC-BPR-0008's separate
round-trip contract remains the authority for the eventual shared
publication.

## What is not proven here

RFC-BPR-0009's original collaborator criterion requires that a person who has
never used BetterPullReview can understand and respond to every published
defect. No fixture settles that:
the fixture holds the words this tool wrote, and the question is whether a
person without local context can act on them.

BPR-0153 retains it as a standalone parked follow-up under the owner's r4
closeout amendment. It needs a concrete defect/disposable PR, a person who has
not used this tool, and a separately authorized publication/reply course;
BPR-0142's earlier round-trip comment does not supply that defect. No new token
request is needed. RFC status does not certify this observation.
The card records what must not be done instead —
not answered by reading the prose oneself, since knowing the local severity
cannot be un-known, and not answered by adding a legend, since a comment
needing one has already failed.
