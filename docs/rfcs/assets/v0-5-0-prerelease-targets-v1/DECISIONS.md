# Design record

## Scope and provenance

Working draft v1, authored 2026-11-10 from the owner's notes of the same day
on the 0.4.2 prerelease. The owner asked for the notes to be designed, with
the frontend-design skill where it matters, and worked toward an author-rfc
pack addressing all of them for 0.5.0. The notes are preserved verbatim in
[SOURCE_NOTES.md](SOURCE_NOTES.md), numbered for citation only. The owner
added note 19 later the same day, and made remarks 20 to 23 in reply to
design proposals for it; all are appended there under their own headings.

The code was inspected at revision
`b12269ea9ad1ff07afe8ec31812875b89d870e5a` by six read-only scouts and by
direct reading of the decisive paths. The frontend-design skill was loaded
before the mockups in the
[design round](../../../design/v0-5-0-targets-v1/index.html) were drawn; they
use the shipped dark colour roles, the Home attention tones and the agent
accent, and were rendered and checked in a browser. No product source, live
RFC, board card or counter was changed.

On the same day the owner asked for this pack to be reconciled with the
local PR drafts pack and then chose to absorb that pack here (notes 24 to
26). Its six RFCs became RFC-DRAFT-0010 to 0015 and its twenty cards
DRAFT-0028 to 0047. [RECONCILIATION.md](RECONCILIATION.md) is the record of
that: what came in, what was left out and the rule at each point of contact.
The absorbed pack's own decision record is preserved, with numbers mapped,
in [local-drafts/DECISIONS.md](local-drafts/DECISIONS.md); the original
directory in Downloads was not changed.

The owner's words in this pack are the notes and the answers quoted in the
table below. Everything the RFCs add beyond them is a proposal until the
owner says otherwise.

## Decisions

| Decision | Requirement or acceptance | Status and actual answer | Rationale and consequences | Depends on or affects |
|---|---|---|---|---|
| Where a returned pull request shows | RFC-DRAFT-0001, Placement, order and wording; AC02 | **Settled, 2026-11-10.** The owner first wrote "I'm not sure which is better", then asked "do you have a suggestion"; given the recommendation of a reason inside My pull requests, the owner answered "Go with that" | That section already holds exactly the authored rows not waiting on a reviewer; a new section would separate Returned from Ready and Failing, which are also the author's move. The alternative changes only placement, so it can still be revisited after the owner's look on a frozen build | DRAFT-0001; amends RFC-BPR-0066, 0070 in reasons and order only |
| The next-move table and its order | RFC-DRAFT-0001, One next move per authored row | Proposed | The owner named "someone is waiting on me" and "ready to merge"; the remaining rows complete the set so no authored row is left Quiet with a known reason | DRAFT-0001 |
| Conversations awaiting my reply need a new bounded read | RFC-DRAFT-0001, Facts; AC04, AC05 | Proposed, with a stated fallback | Unresolved threads are not in the Home read today; without them only changes-requested can mark a row Returned | DRAFT-0001; GitHub read contract |
| Arriving at a stop shows its code | RFC-DRAFT-0009, Arriving at a stop shows its code; AC01 | Settled (note 19: "going to a tour stop should go to the code by default"). Removing the Show in code button and making the file-and-lines line the way back are proposed | The code navigates on a change of row but not on the row already current; which path cost the owner a click was not reproduced, so every arrival path is covered | DRAFT-0025 |
| Ready, next goes; the ring is not the accept control | RFC-DRAFT-0009, A stop is accepted, in the files' language | Settled (notes 20, 21 and 23: "Ready-next is awkward"; "Clicking the ring seems a bit dubious… we use that for navigation and opening/collapsing the stop already"; "The button, especially as currently conceived, is bad"). The authoring session had proposed the ring as the control and the owner declined it | The ring keeps showing state and keeps its navigation role | DRAFT-0025, DRAFT-0026 |
| Suggest an outline leaves the application | RFC-DRAFT-0009, Suggest an outline leaves the application; AC07 | Settled (notes 22 and 23: "Retire outline"). Keeping the CLI's outline commands and carrying old drafts across are proposed | It is the only source of stops that needed the Ready mark, and the owner uses only Draft the tour | DRAFT-0025; amends RFC-BPR-0043 |
| A stop is accepted in the files' language, and again when it changes | RFC-DRAFT-0009, A stop is accepted, in the files' language; AC02 to AC05 | Settled intent (note 23: "accept still has value, and we already have a visual language for that for the files"; accept again when "the review gets updated and the hunk its attached to or a link in the stop comment is changed"; "the yellow/green ring is good") | One acknowledgement replaces Ready, the Head changed chip and the separate confirmation of lines | DRAFT-0026; amends RFC-BPR-0042, 0043, 0096 |
| What counts as accepting, and what a new head does | RFC-DRAFT-0009, same section and What is settled and what is proposed | **Accepted with the mockup, 2026-11-10.** Save and the stop's own proposal Accept count; Accept all does not; accepting an agent-proposed stop confirms the lines shown; a stop whose material is unchanged stays Accepted across a new head. The stop mockup listed these as the owner's to judge; asked whether the mockup matched what they had in mind, the owner answered "Yes, keep it". The answer was for the mockup as a whole, not rule by rule | These follow from the owner's examples but go beyond them; the last two change what must be re-confirmed before publishing, so DRAFT-0026 still gets critical review and the frozen-build look stays | DRAFT-0026; RFC-DRAFT-0003's Accept all |
| A stop at rest offers only reading and going | RFC-DRAFT-0009, A stop at rest, and a stop being edited; AC06 | Settled intent (note 19: "too many options outside of edit mode"); the exact set at rest was accepted with the stop mockup on 2026-11-10 ("Yes, keep it") | The owner did not name which options were too many; the accepted set is Edit, the side chat and Accept while the stop is yellow | DRAFT-0025 |
| Publish action in a pinned head; agent bar pinned at the bottom | RFC-DRAFT-0002, Three bands | Settled intent (notes 6 and 7); the band design is proposed | Both controls scroll away today because the whole tab is one scroll area | DRAFT-0003 |
| The app chooses the publication form | RFC-DRAFT-0002, The plan chooses the form | Settled intent (note 2: "why don't we just pick the right one?"); the order and the Keep it to one review offer are proposed | The core already decides each stop's representation; the list asked the author to repeat that decision | DRAFT-0004; RFC-BPR-0041 presentation |
| Per-step progress and error-role failures | RFC-DRAFT-0002, Steps that advance; Failures that read as failures | Settled intent (note 2); states and banner content proposed | The projection has no per-step state and failures are placeholder-coloured sentences | DRAFT-0004 |
| What to fix on Overview | RFC-DRAFT-0002, The tour on Overview; AC07 | **Settled, 2026-11-10.** Note 3 does not say what is wrong. Shown the proposed fixes (Markdown rendering, navigable stop rows, the head's state and actions) and asked what the note meant, the owner answered "Your fixes cover it" | Those are the faults the code shows, and the owner confirmed they are the ones meant | DRAFT-0005 |
| Remove the permanent Update button; keep the blue link, in every thread | RFC-DRAFT-0003, What sits under the transcript; Asking for the update | Settled intent (note 10: the blue text "is sufficient"); extending the link to stop, Problem and Solution threads is proposed | Without the button the link is the only route to an update, so it must exist wherever an update does | DRAFT-0007 |
| Session rows fold into a menu; notices only when abnormal | RFC-DRAFT-0003, What sits under the transcript | Proposed | The owner called the bottom ugly without naming rows; these are the rows added since 2026-11-06 | DRAFT-0007; RFC-BPR-0096 presentation only |
| A change request yields its revision in the same turn, with a card in the chat | RFC-DRAFT-0003, A change request is answered with the change | Settled intent (note 10); the card and its three actions are proposed. Agent behaviour cannot be guaranteed, so the link remains | The contract already allows it; the prompt and the absence of an in-chat card push toward a second turn | DRAFT-0007 |
| Enhance runs in the part's side chat session | RFC-DRAFT-0003, Enhance runs in the part's conversation | Settled (note 8). Keeping the one-shot hooks for the CLI is proposed | Enhance is a tool-free one-shot today and knows nothing the conversation established | DRAFT-0007; RFC-BPR-0043, 0051 unchanged |
| References in tours are links or snippets; permalinks on GitHub | RFC-DRAFT-0003, Code references in stops | Settled intent (note 4); the snippet rule, the 12-line bound and the published form are proposed, the last pending a check on BPR-Test | GitHub's embedding of bare permalinks in each comment kind is not established here | DRAFT-0008, DRAFT-0010 |
| Do not narrate behaviour only moved behind a condition | RFC-DRAFT-0003, Moved and flag-gated code | Settled (note 9); the rule's wording is proposed | No guidance mentions moved code today | DRAFT-0008 |
| One reference recogniser for every agent reply, with wider forms and unchanged files | RFC-DRAFT-0004, Code references | Settled intent (note 5); forms and resolution proposed | The Agent tab already links some forms for changed files; the owner still meets unlinked ones. Which gap they met was not reproduced | DRAFT-0010 |
| Return sends, Shift+Return adds a line, in chats only | RFC-DRAFT-0004, Send key | Settled (note 12). Scope proposed: the Agent tab and the selected-lines side chat, since the tour side chat already behaves this way; save and publish composers keep their keys | A chat sends; a comment publishes or saves | DRAFT-0011 |
| Cost per turn in the turn footer and per session under the composer, in dollars and cents | RFC-DRAFT-0004, Cost | Format settled (note 11: "dollars and cents is sufficient"); placement **proposed**, since the owner was unsure between per turn and per session | Showing both answers either reading; which figure the runtime reports is established during implementation | DRAFT-0011 |
| Indigo exactly where a control starts a run | RFC-DRAFT-0004, Colour | Settled (note 14). Consequence proposed: Accept, Accept all, Undo reset and Move lose it; proposal outlines keep it | The owner's rule taken literally: mechanical and free means ordinary colours | DRAFT-0012 |
| The agent accent is a configurable Appearance colour; indigo is its default | RFC-DRAFT-0004, The accent is an Appearance colour; AC09 | **Settled, 2026-11-10.** The owner asked "Agent accent color is configurable in settings and just defaulted to indigo right"; told it is fixed in code today, and asked whether to make it configurable with indigo as the default, the owner answered "Yes, configurable". Following RFC-BPR-0034's existing role rules, as State attention did, is proposed | The accent now appears on many more surfaces, and the application already lets other per-theme colours be changed | DRAFT-0012; amends RFC-BPR-0034's role list |
| Replace the scope list with the attached-lines chip | RFC-DRAFT-0005, The attachment replaces the scope list | Proposed. The owner called the field confusing, wanted "no extra context" and selected lines as the default | In the inspected paths the list changes nothing that is sent; an honest control shows the one thing that differs | DRAFT-0014; amends RFC-BPR-0012 scope |
| Suggestions carry their subject; Review this pull request never needs a selection | RFC-DRAFT-0005, Suggestions carry their subject | Settled intent (note 17) | Every send uses the selection entry point today | DRAFT-0014 |
| Escape clears the diff's selection | RFC-DRAFT-0005, Clearing the selection | Settled as the owner's own suggestion ("Maybe if escape…"); the precedence rule is proposed | Nothing clears a selection today | DRAFT-0014 |
| A turn stays open while reported helpers run | RFC-DRAFT-0006, When a turn ends | Proposed, with a foreground-only fallback per agent | The likely cause of the silent failure, read from code and not reproduced | DRAFT-0016; amends RFC-BPR-0096 late-frame rule |
| Show activity and helpers; say when a run is quiet | RFC-DRAFT-0006, What a running turn shows; Quiet is said out loud | Settled intent (note 18: seeing active work "seems pretty important"); the block, the 120 seconds and the two choices are proposed | The status is one line and a clock today | DRAFT-0017 |
| Thinking text is not shown | RFC-DRAFT-0006, What a running turn shows | **Deferred** on the owner's own weighting ("I don't necessarily need that") | Thinking appears as a state; its text can be added later without changing the turn's lifecycle | None |
| The scratch folder starts on; its starting state is an agent setting; a change in a pull request is remembered | RFC-DRAFT-0007, On to begin with, and remembered per pull request; AC08, AC09 | **Settled, 2026-11-10.** Owner: "Should start on, can have that initial state be a setting in the agent settings. And should remember your setting as you change it within a PR." This overrode the draft's proposal of off until turned on. Remembering per pull request *and agent*, through restarts, and the grant line as the control, are proposed detail | A script the agent writes runs with the reviewer's rights; with the folder on to begin with, the always-visible grant line and the run details carry that disclosure | DRAFT-0019; amends RFC-BPR-0011 |
| Builds and tests, per agent, disposable checkout only, asked per pull request | RFC-DRAFT-0007, Builds and tests; AC04 | **Settled, 2026-11-10.** The owner had called it "cool" but "not as important"; asked whether it should be part of 0.5.0 as proposed, the owner answered "Include builds" | Building runs the pull request's code, so it stays off until turned on for an agent, never runs in a checkout the reviewer chose, and asks once per pull request | DRAFT-0020; amends RFC-BPR-0011 |
| Every submission is Waiting, Sending, Published or Not published; waiting is bounded at 20 seconds | RFC-DRAFT-0008, A submission is always in a stated condition | Proposed | A submission can wait or vanish silently in several places; the bound holds whatever the cause | DRAFT-0022 |
| Diagnose the stalled Approve before calling it fixed | RFC-DRAFT-0008, The cause is found before the fix is called done; AC01 | Proposed. The cause is **not confirmed** | The leading account is read from code; the owner's failure was not reproduced | DRAFT-0022 |
| Every opened pull request starts on Comments with no side chat | RFC-DRAFT-0008, A review opens fresh | Settled (note 13). Applying it to a reopened same pull request is proposed | Tray and chat state are window-lifetime today | DRAFT-0023 |

| One pack: absorb the local PR drafts pack here | [RECONCILIATION.md](RECONCILIATION.md), What was brought in | **Settled, 2026-11-10** (note 26: "the 0.5.0 pack being the one pack we import and implement here, just take the good from the other pack"). What counted as "the good" was the authoring session's judgement: all six RFCs, their cards, milestones and records came in; the enterprise overview, that pack's own tooling and its import hold did not | One entrypoint and one plan; the first nine RFCs decide shared surfaces | RFC-DRAFT-0010 to 0015; DRAFT-0028 to 0047 |
| The answers given for the local PR drafts pack | [local-drafts/DECISIONS.md](local-drafts/DECISIONS.md), D01 to D14 and P04 to P06 | **Settled there, 2026-11-07 to 2026-11-10**, and carried unchanged: author perspective; sharing first; local PR draft; Review Cycles loses Home prominence and keeps its data; deliberate interaction; no enterprise dependency; companion over fork; evidence without a universal formal gate; the author pushes and BPR creates the pull request from the verified remote head; interactive handoff for effectful actions; a candidate refresh starts a new conversation; import follows the agent-identity rule; one BPR-defined evidence format | These are that pack's recorded owner answers, not answers given in this authoring session | RFC-DRAFT-0010 to 0015 |
| The enterprise overview stays out of the repository | RECONCILIATION.md, What was left out | **Proposed.** The authoring session left it and the compatibility analysis in Downloads because they describe an employer's internal tooling | RFC-DRAFT-0015 specifies a generic contract and does not need them to be implemented | RFC-DRAFT-0015; local-drafts/PROVENANCE.json |
| How the absorbed RFCs take the first nine's decisions | RECONCILIATION.md, Where they meet; each absorbed RFC's fitted section | Nineteen rows inherit or resolve with no new choice. **Six were proposed and the owner confirmed them on 2026-11-10 ("Confirm all six"), after they were listed to the owner one by one:** one Home row for a local draft that has become a pull request (X02); the accent on extension actions (X11); exporting the scratch and build settings, with builds imported off unless accepted (X17); unaccepted stops block the tour but not draft creation (X20); a stop citing evidence that needs recheck turns yellow (X21); a local draft opens on Changes with no chat (X23) | Each follows both parts' stated intent but was said by neither, so each needed the owner's word | The cards named in RECONCILIATION.md |
| All seven absorbed milestones are in the 0.5.0 release | [RECONCILIATION.md](RECONCILIATION.md), Order of delivery | **Settled, 2026-11-10.** Asked whether the local PR draft milestones are part of 0.5.0, with "Sharing only for now" recommended and offered, the owner answered "All of them" | The release now includes the whole local-draft workflow, the largest part of the pack; the plan's order and prerequisites are unchanged | RFC-DRAFT-0010 to 0015; all sixteen milestones |

Requirements and acceptance live in the RFCs. Cards and milestones derive
from them.

## Open frontier and explicit deferrals

For RFC-DRAFT-0001 to 0009 no choice of the owner's is unanswered. Home
placement, the Overview faults, the scratch folder's default, the tour stop
design and builds in 0.5.0 were each asked and answered on 2026-11-10; see
the table above.

For the absorbed RFC-DRAFT-0010 to 0015, the six fitting rules are confirmed
and all seven milestones are in the 0.5.0 release. One thing remains the
owner's: whether the enterprise overview may be committed. It was left out
by default, RFC-DRAFT-0015 can be implemented without it, and it holds back
no card.

Three facts are not established and are assigned to implementation, each
with the behaviour that holds either way:

- why Approve stalls (RFC-DRAFT-0008, DRAFT-0022);
- how installed Claude and Codex report background work on BPR's transports
  (RFC-DRAFT-0006, DRAFT-0016);
- whether Claude's reported cost is per turn or cumulative on a resumed
  session (RFC-DRAFT-0004, DRAFT-0011).

Deferred, with the boundary each leaves: showing thinking text; any change
to the `bpr tour` CLI's publication options or its outline commands; any new notification for a
returned pull request; pricing tokens for a runtime that reports no cost.

Engineering detail left to implementation within the stated behaviour: the
lane arrangement that keeps one pull request's effects from blocking
another's submission; the reference recogniser's internals; wire and field
names; the exact launch flags for each agent, to be verified against the
installed versions rather than from memory.

## Checks and confirmation

- **Structural validity.** After absorption, `validate.py
  milestone-plan.json` from the discovered implement-rfc kit reported "Structurally valid; references and
  semantic coverage unchecked." [MILESTONES.md](MILESTONES.md) is that
  validator's `--render` output for the same JSON. The validator does not
  check that selectors resolve after import or that exits cover the RFCs.
- **Links and criteria.** Every relative link in the pack resolves to an
  existing file, and each RFC's acceptance labels are unique within it
  (checked by a script during authoring; not a product check).
- **Repository gate.** `python3 scripts/verify_documentation.py --root .`
  was run after the pack was written; its result is reported in the
  authoring reply, not here.
- **Design readiness.** All sixteen milestones and all cards are ready: no
  material choice is open. Three facts remain to be established by
  implementation, as listed under the open frontier. The absorbed
  publication and extension RFCs were rewritten around the owner's answers
  after that pack's reviews and were not reviewed again in that form, as
  their own record says.
- **Confirmation.** All 2026-11-10 and all for draft v1: Home placement,
  "Go with that"; the Overview faults, "Your fixes cover it"; the scratch
  folder's default, quoted in the table above; the tour stop directions in
  remarks 20 to 23; the stop mockup, "Yes, keep it", which covers the stop
  rules as drawn in `stop.html` at that time; builds in 0.5.0, "Include builds"; absorbing the local PR drafts pack,
  note 26; the configurable agent accent, note 27; the six fitting
  rules for the absorbed RFCs, "Confirm all six"; and the release scope,
  "All of them". Each covers only what the
  table names. The owner has not confirmed any other part of this draft, and
  no other recommended option here is an answer. None of these is acceptance
  of running behaviour.
- **Execution eligibility.** Not evaluated. Live IDs, the RFC baseline,
  mapping, dependencies and mandate belong to an authorized implement-rfc
  run.

No product behaviour was run or verified by authoring. Statements about
current behaviour are read from source at the inspected revision; where an
RFC says "likely" or "not reproduced" the claim is an inference.

## Continuation

Next action: none is owed by the owner for authoring. Implementation starts
under a later mandate.
If the owner changes an earlier answer, record it and redraw the affected
frame. The pack lives
in `docs/rfcs/assets/v0-5-0-prerelease-targets-v1/` and the mockups in
`docs/design/v0-5-0-targets-v1/`.
