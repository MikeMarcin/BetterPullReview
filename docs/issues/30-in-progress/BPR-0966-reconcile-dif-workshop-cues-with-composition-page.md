---
schema: "implement-rfc.card.v1"
id: "BPR-0966"
title: "Reconcile Dif workshop cues with the dedicated composition page"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"origin_rfc":"RFC-BPR-0031","related_rfc":"RFC-BPR-0037","found_by":"BPR-0507 owner-amended closeout","mandate_revision":"r4-with-owner-compose-20260925-r3","found_at":"9479f1c3a7bd9e849e11fc64d3c156d0ede2a58d","scope":"Standalone later-UI contract follow-up; no RFC0012 product criterion is waived and no completed RFC0031 acceptance is rewritten","milestone_plan":{"id":"bpr0966-dif-cues","path":"docs/rfcs/plans/bpr0966-dif-cues/milestone-plan.json"},"accepted_platform_scope":"macOS exact candidate 388f814c with final human Dif Accept; historical Mac evidence retained","remaining_scope":"Deferred Windows consider-cue and exact-candidate composed-course verification; no waiver","owner_followup":"BPR-1212"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"388f814c7be796b91c4dd6779422be3d4321e4f3","evidence":{"kind":"observation","detail":"Owner decision dba44cfd from session06ad5734 selects visible Compose workshop slot; BPR1209 prime-slot reconciliation retained. No historical acceptance rewritten."}},{"criterion":"AC2","outcome":"pass","revision":"388f814c7be796b91c4dd6779422be3d4321e4f3","evidence":{"kind":"command","detail":"Baseline fails ready/acknowledge only. Candidate actual composed proposal and visible Accept button paint ready/acknowledge; exact session/action/draft router unchanged, one sprite, one acceptance intent and all publication counters zero. See compose-evidence.md."}},{"criterion":"AC3","outcome":"pending","revision":"388f814c7be796b91c4dd6779422be3d4321e4f3","evidence":{"kind":"observation","detail":"Mac behavior: six focused checks pass at 388f814c; main integration recorded in a773d803. Final human Accept 3bcafec9-7811-46dd-83dd-f7bda4cdac1b at session54fd75833481882c3bb47745f8e30b75 sequence5 accepts candidate BPR-0966-compose-slot-v002 fingerprint87eef088bdcccaf17caeaa52833756bca5bb4bee56287ccbe81a1bc888f8e3fd. Windows consider-cue and exact-candidate public-course outcome remain pending. Full Compose UX rethink deferred to BPR-1212; no native/cross-platform waiver."}}]
---

# Reconcile Dif workshop cues with the dedicated composition page

Current disposition (2026-09-26): the owner accepted Dif on the exact Mac
candidate. AC1 and AC2 pass; AC3 remains partial for the deferred Windows
course. Backlog now selects `win` for that remaining work, preserving all Mac
evidence below. There is no outstanding Dif visual question. The full Compose
UX rethink is the later, independent [[BPR-1212]] follow-up.

## Observation

On 2026-09-21, the RFC0012 closeout's broad agent selector reached
`application_agent_finding_dif_context_contract` and failed only
`dif-comment-ready-after-conversion` and `dif-acknowledge-after-acceptance`.
The exact diagnostic was `agent-finding-dif-context-evidence:1:1:1:0:0:1:0`.
The same failure repeats when the course runs alone (18.13 seconds):

```sh
ctest --test-dir /Users/jaeger/Documents/GitHub/BetterPullReview/build/dev --output-on-failure --no-tests=error -R '^application_agent_finding_dif_context_contract$'
```

Product/test candidate: `9479f1c3a7bd9e849e11fc64d3c156d0ede2a58d`, unchanged
through the documentation-only claim at `abd9baee670332b2c6fefd17d01ab6a84eabe9b3`.
Finding-course executable SHA-256:
`c26423eb82a5e676b681c3d31ca2187a61602c376a46522a9f393d9ab9c4b1ee`.
App executable SHA-256:
`a0f9c85cd58e4c9d486ae39c554b16f30f4bb73e867ebfbb6c17ad21ca775d90`.

The same test confirms comparison, consider and attention cues, one queued
finding, the exact proposed and accepted draft bytes, and zero GitHub mutation.
The original finding/chat/workshop courses pass. This is a failing later Dif
presentation contract, not evidence of a failed draft conversion or a passing
Dif course. No native observation or new human decision was obtained.

## Source explanation and remaining judgment

RFC0037 integration `cc03874861abd658cce11812f15d887607919d1b` added
`Window->open_review_composition(true)` before `present_dif_workshop` in
`github_application::present_finding_outcome`. That opens the dedicated
composition page. `workbench_window::apply_drawer_layout` then hides every
drawer while that page is active; `showing_dif_ai` requires a visible
conversations drawer, and `update_dif_presentation` disables the cue router
and hides the illustration when that condition is false. The course still
expects `comment_ready` and `acknowledge` immediately after conversion and
acceptance. This source path explains the observed mismatch; no first-bad
bisect or control build was run, and whether the course or product must change
requires reconciliation with the approved RFC0031/RFC0037 presentation contracts.

## Acceptance

- AC1: Resolve the workshop cue expectation against the dedicated editor page,
  documenting the approved behavior and any required owner amendment without
  silently rewriting terminal RFC0031 acceptance. Coordinate with BPR-0800's
  separate placement decision if the chosen behavior affects it.
- AC2: Correct the identified stale expectation or product regression with a
  focused failing-before check; preserve exact session/action/draft identity,
  single-sprite ownership, explicit acceptance and zero publication effects.
- AC3: Run the Dif context course and the affected router/editor/public
  composition checks at the exact candidate. Record native or human decisions
  separately if the resolved presentation contract requires them.

The r4 owner closeout leaves this as an independent follow-up. BPR-0507 does
not claim these two cues passed and does not modify their assertion or source.

## Windows also misses the consider cue (bpr-main-win, 2026-09-24)

Folded in by the coordinator as the reserved `dif-saw-consider` finding; this
card is not moved or claimed by it. Measured on the bpr-main-win station
(`build/windows-dev`, MSVC 19.51 Debug, Qt 6.11.2, offscreen, serial), and
recorded first on [[BPR-1050]]. The logs are station records under
`build/logs/bpr-1050-main-win/` (`difctx-*.log`, `rows32.log`, `sweep.log`)
and are not committed.

On Windows the course fails one assertion more than the macOS observation
above. As well as this card's two, it misses the consider cue:

```text
agent-finding-composition-failure:dif-context-evidence failed=dif-saw-consider dif-comment-ready-after-conversion dif-acknowledge-after-acceptance
agent-finding-dif-context-evidence:1:0:1:0:0:1:0
```

The second field is `dif-saw-consider`; macOS printed `1:1:1:0:0:1:0`.

| Candidate | Runs | Missed consider |
| --- | --- | --- |
| `bcd57840` | 9: 7 alone, 1 in the 32-row table, 1 in the 154-row `^application_` sweep | 9 of 9 |
| `bcd57840` with only `tests/end_to_end/agent_finding_composition_public_path.cpp` put back to `3db65162`, the version before [[BPR-1050]]'s `ff9bc9e2`, and its target rebuilt | 6 alone | 4 of 6; the other 2 printed the macOS shape, `1:1:1:0:0:1:0` |

So the miss predates BPR-1050's data-root change. Nine of nine after it
against four of six before is too few runs to say whether the change made it
more frequent. The course does not wait for the consider cue.
It samples Dif's current clip on each pass of `wait_until_reporting` while
the agent turn runs, and `difSawConsider` becomes true only if a pass happens
to land while the consider clip is showing. A miss means no pass saw it: the
clip was shown between passes, or not shown at all. These runs do not tell
the two apart. The attention cue after delivery (the third field) was seen
in every run, and no run printed a refusal line.

For AC2 and AC3: when this card reconciles the workshop cues, the consider
assertion should either record the clip when Dif shows it, rather than sample
it, or be named as a timing-dependent observation. Its Windows outcome is
wanted in AC3's run at the exact candidate.

## Isolated Mac follow-up (2026-09-25)

Under the coordinator's unattended Mac assignment, candidate
`12ea8646b9175a32abfa396eb502a2c09007c088` corrects the course's observation:
the Findings viewport now exposes Dif before a passive Paint-event observer
records consider. `isVisible()` had admitted a completely clipped sprite.
The focused failing-before probe records an empty visible region and no consider
paint; exposing the existing slot makes the region nonempty and records the cue.

The candidate retains the original two failing workshop assertions. Actual
proposal and acceptance show the editor while both Dif and the AI slot are hidden.
The exact original draft, retained draft ID, one acceptance command, single
sprite, proposed/accepted bytes and zero-publication checks pass. Router, editor,
ordinary finding composition and workshop courses pass; the Dif course fails
only ready/acknowledge. Full commands, identities and limitations are in
[the scoped evidence](../../rfcs/plans/bpr0966-dif-cues/evidence.md).

**Owner decision required for AC1:** On Compose, should the one Dif widget move
to a visible workshop slot, use the corner coordinated with BPR-0800, or remain
absent with an explicit amendment to RFC0031 B7's ordinary workshop
acknowledgment requirement? The choice must establish whether ready and
acknowledge remain required there. Accepted RFC0037 composition placement does
not resolve this. BPR-0800 reserves the slot/corner decision before code; no
product placement or historical RFC0031 acceptance was changed.

AC2 is partial, and AC3 retains the failed Dif course. Windows verification is
explicitly deferred by the user's direction conveyed in this assignment; the
earlier Windows results and follow-up obligation above remain intact. No new
native or human verdict was obtained. There is no new visual candidate ready
for attendance before the owner contract decision.

## Owner decision on AC1 — 2026-09-25 (attended session 06ad5734)

Packet [owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md),
session `06ad5734ed9f7c2fd6d0a62c8328fa0a`, candidate
`BPR-0966-compose-dif-v001`: `12ea8646` (its `src/` and `tests/` equal main's
`faf53c8f`), course `86de5254…85be`, canary `109315a6…cc`, fingerprint
`e331d3f8ccd44716d1963d48b40cc2b67bd952482db1cff46a1396a564fd6688`, equal to
the prepared one. Preparation reran the Dif context course on that candidate
(19.09 s, macOS arm64) and it failed as recorded above:
`proposal-editor=1 proposal-ai-slot=0 proposal-dif=0 accepted-editor=1
accepted-ai-slot=0 accepted-dif=0 painted-consider=1 painted-ready=0
painted-acknowledge=0`.

Decision `dba44cfd-0d53-4c01-bed1-14ef620ffe09`, sequence 37,
2026-09-26T04:40:42Z, `actor: human`, no note: the owner selected, verbatim,
"A visible workshop slot on Compose: Dif moves there and plays ready and
acknowledge".

**What that settles for AC1.** The Compose review page gains a visible
workshop slot, a small AI tools area of its own. While a workshop proposal is
shown there and after it is accepted, the one Dif widget moves into that slot
and plays `comment_ready` after the proposal and `acknowledge` after the
acceptance. RFC-BPR-0031 B7 is not amended: both cues stay required on
Compose, and the course's two assertions stay as they are. The corner was not
chosen for Compose, so this does not answer [[BPR-0800]]'s question for the
other review surfaces; its one-sprite rule still holds everywhere. RFC-BPR-0037's
page, drafts and publication semantics are unchanged apart from the slot.

**Remaining.** AC2: build the slot and route Dif to it on Compose, with a
failing-before check, keeping the exact session, action and draft identity,
the single sprite, the explicit acceptance and zero publication. AC3: the Dif
context course passes `painted-ready` and `painted-acknowledge` on the exact
candidate, with the router, editor and composition courses. The slot is a new
visible surface on Compose, so the owner's look at it is expected as one
attended question for that criterion alone. Windows stays deferred, and its
consider-cue observation above still belongs to AC3's run there.

The card moves from Blocked to Ready: the placement question that blocked it
is answered, and it has no unfinished dependency.

## The owner's direction of 2026-09-25 on Dif's home, and this card (BPR-1209)

Later the same day, in chat, relayed by the coordinator, the owner directed:
"Move Dif out of the right drawer entirely, he can live in the lower left
corner of the status bar by default just like AllTheThings unless he's got a
prime spot on a screen like on the first time experience home." [[BPR-1209]]
builds it. The direction and this card's AC1 verdict (decision `dba44cfd`,
the workshop slot chosen over the corner) could read as a conflict. The
coordinator relayed that the owner delegates it, and it was resolved under
that delegation, with the full rationale on BPR-1209:

- **Compose's workshop slot is a prime spot.** While a workshop proposal is
  shown on Compose and after it is accepted, Dif stays in this card's slot and
  plays ready and acknowledge there. This card's verdict, AC2 and AC3 are
  unchanged. Rationale: the owner chose this slot over the corner for Compose,
  and the direction keeps a prime spot wherever a screen has one.
- **Outside a workshop, Dif rests in the corner.** Before BPR-1209 lands, Dif
  leaves Compose's slot for the drawer's slot as today. After it lands, he
  returns to the status bar's corner instead.
- **No dependency either way.** Both cards change
  `application_agent_finding_dif_context_contract`, and whichever lands second
  updates it. Under BPR-1209, the consider observation that this card's Mac
  follow-up takes through the tray tab's slot moves to the corner. This
  card's ready and acknowledge assertions on Compose stay as they are.

## Approved Compose implementation — 2026-09-26

Candidate `388f814c7be796b91c4dd6779422be3d4321e4f3`, under coordinator
continuation `r4-with-owner-compose-20260925-r2`, builds the selected slot.
The fixed Comment workshop header presents the existing Accept/Reject revision
actions and the workbench's one 64-pixel Dif. It stays exposed when the editor
scrolls; the semantic controls remain when art is hidden. The same exact-draft
eligibility gates the illustration, and the existing router remains unchanged.

The baseline reproduced the original two failures. Six focused checks now
pass on this exact candidate, including actual ready/acknowledge Paint events,
clicking the visible Accept button after scrolling, exact draft identity,
one emitted acceptance intent, neutral navigation return and zero publication.
The separate architecture gate fails on unchanged MCP Windows containment
ownership and reproduces on the entry baseline; it is not claimed passing.
See [the Compose evidence](../../rfcs/plans/bpr0966-dif-cues/compose-evidence.md)
for commands, hashes, the isolated baseline reproduction and limitations.

AC1 remains satisfied by the owner's recorded decision. AC2's bounded product
correction is verified. AC3 has passing macOS fixture evidence and **one
pending attended visual judgment** of this slot. Park on `human-review` for
that question. Windows remains deferred, with its consider-cue obligation
preserved above; no Windows/Linux native or installed-release result is claimed.
The separate BPR-1209 owner received the slot API and will preserve this prime
placement when the default corner lands. Whichever lands second updates the
shared Dif course's initial placement.

## Final owner verdict and remaining native work — 2026-09-26

Attended session `54fd75833481882c3bb47745f8e30b75`, candidate
`BPR-0966-compose-slot-v002`, revision
`388f814c7be796b91c4dd6779422be3d4321e4f3`, fingerprint
`87eef088bdcccaf17caeaa52833756bca5bb4bee56287ccbe81a1bc888f8e3fd`:
final **Accept**, decision `3bcafec9-7811-46dd-83dd-f7bda4cdac1b`, sequence 5,
2026-09-26T18:03:54.922881+00:00; handed back at sequence 6. The owner wrote:

> Dif is fine.
> I utterly hate the compose UX and it needs a full rethink with $frontend-design as a later follow-up.

The preceding Reject `558218a7-99a6-43c7-8d7a-5ec76870d505`, sequence 4,
is explicitly superseded by this final decision on the same candidate. Both
remain in [the verdict record](../../rfcs/plans/bpr0966-dif-cues/owner-verdict-compose.json).
The Accept completes the scoped Mac visual judgment; it does not approve
Compose UX generally, erase the Windows observation, or change terminal RFC
acceptance. [[BPR-1212]] preserves the separate later redesign request.

AC1 and AC2 remain pass. AC3 has six passing exact-candidate Mac behavior checks,
the final human Dif acceptance, and coordinator main-integration evidence at
`de496298` recorded in `a773d803` (2/2 Dif/context and window checks, plus
2/2 architecture/contained-temporary checks at `ca23a9fe`). None supplies the
Windows outcome explicitly retained above. AC3 therefore stays pending, and
this card returns to Backlog with Windows eligibility rather than Done or an
answered human-review wait. The historical Mac scope stays accepted.

Next: an authorized Windows station owner should build the integrated candidate,
run `application_agent_finding_dif_context_contract` and the affected
router/editor/placement/public-composition checks, observe actual consider,
ready and acknowledge paints with the exact identity and zero-publication
assertions, and record the revision and native limits. Diagnose an actual
failure before changing the assertions. No new owner visual approval is owed
for the unchanged accepted Mac slot; a materially changed candidate may need
its own scoped review. No Linux or installed-release qualification is claimed.
