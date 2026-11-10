# Design and readiness record

> Carried into the 0.5.0 targets pack on 2026-11-10 from the local PR drafts pack, version 1. RFC and card numbers were mapped on the way in (RFC-DRAFT-0001 to 0006 became 0010 to 0015; DRAFT-0001 to 0020 became 0028 to 0047). The pack as it was is at `/Users/jaeger/Downloads/bpr-local-pr-drafts-rfc-pack-v1`.

## Scope and provenance

Version 1 of this portable pack was authored on 2026-11-07 for the owner's
secondary review. The requested source is the BetterPullReview checkout, plus
the preserved generalized enterprise overview and the user conversation. The
checkout was first observed at `154d307d765a87917c805fe1d4ac024eef0fd6c5` and
re-observed on 2026-11-10 at `b12269ea9ad1ff07afe8ec31812875b89d870e5a`, 344
commits later; the [third review](#third-review) records what that changed.
These are observation revisions, not an RFC baseline. Source identities are in [Provenance](PROVENANCE.json)
and decisive observations in [Current behavior](CURRENT-BEHAVIOR.md).

The author-rfc skill and implement-rfc offline milestone kit were used. The
frontend-design skill was loaded for the UX proposal, within the repository's
native Qt constraints. The authoring session dispatched no reviewer; the owner
later ran the reviews recorded below in separate sessions. No source
implementation, live-board import, ID allocation or external mutation was done.

## Decisions

| Decision | Authority and actual answer | Status | Requirement and consequence |
|---|---|---|---|
| D01: author perspective | The user corrected the topic to application users preparing their own PRs, not BPR's internal engineering process. | Settled direction | All six RFCs concern shipped author capabilities and their configuration. |
| D02: independent sharing first | The user twice requested agent/prompt import-export as a separate first slice. | Settled grouping | [Sharing RFC](../RFC-DRAFT-0010-agent-and-prompt-sharing.md) and first milestone have no later feature dependency. |
| D03: local PR draft | The user said “local PR draft makes sense” and asked to fold the discussed flow into the pack. | Settled terminology and flow | [Local draft entry](../RFC-DRAFT-0011-local-pr-drafts.md): Home, worktree, prospective diff, deliberate preparation, then draft creation. |
| D04: Home prominence | The user said Review Cycles should lose Home prominence and tentatively suggested leaving it separate. | Settled prominence direction; conservative preservation proposed | Preserve its behavior/data and menu entry, replace the prominent card with author preparation, add secondary access. No cycle-to-draft migration is authorized. |
| D05: deliberate interaction | The user explicitly preferred deliberation over one-shot drafting. | Settled direction | Save/resume, contextual discussion, accepted proposals and independently repeatable checks; no mandatory linear wizard. |
| D06: independent default product | The user requires BPR to work fully without enterprise dependencies. | Settled boundary | Built-in local flow precedes optional integrations. No Jira/project taxonomy or enterprise validator in core. |
| D07: companion over fork | The user lacks upstream control and prefers skills using installed upstream components over maintaining a fork. | Settled integration direction | [Extension RFC](../RFC-DRAFT-0015-optional-workflow-extensions.md) describes an external companion contract, not an upstream rewrite or guaranteed stage API. |
| D08: evidence and optional formal work | The user wants results/proof where possible before publication and questioned proof viability. | Settled scope; exact record behavior proposed | [Evidence RFC](../RFC-DRAFT-0012-author-evidence.md) separates methods, provenance, bounds and source identity; no universal formal gate or automatic approval. |
| D09: no import | The user instructed “don't import the pack yet”; later review and revision requests have not lifted that hold. | Current explicit stop boundary | Draft files only. Import/implementation needs a later explicit mandate. |
| P01: selective portable file | Single bounded JSON configuration, machine bindings and explicit conflict application. | Proposed, not owner-confirmed detail | [Sharing behavior](../RFC-DRAFT-0010-agent-and-prompt-sharing.md#proposed-behavior-and-boundaries). Independent of open later choices. |
| P02: distinct local subject | Reuse material/tour primitives while preserving separate local draft and GitHub authority. | Proposed architecture | [Local subject](../RFC-DRAFT-0011-local-pr-drafts.md#a-durable-authoring-subject) and [tour ownership](../RFC-DRAFT-0013-local-tours-and-description.md#source-identity-and-continuity). Avoid fake provider IDs. |
| D10: secondary review revisions | After reading the review findings the owner said “Yes, revise the pack”. | Settled: apply the reviewed changes | Recorded in [Secondary review](#secondary-review). Changes PUBLISH-01's recommendation, not its open status; adds no owner answer to either open choice. |
| D11: re-review fixes | After the three follow-up findings, the owner said “okay fix it in place”. | Settled: apply those three fixes | [Re-review fixes](#re-review-fixes) require captured candidate inputs, separate early preparation acceptance from end-to-end publication acceptance, and remove the hard sharing-closeout wait for independent local-draft engineering. Neither open product choice is answered. |
| P03: single full description owner | The new flow's accepted body is not silently overwritten by a later tour publish. | Proposed compatibility rule | [Publication ownership](../RFC-DRAFT-0013-local-tours-and-description.md#publication-ownership). Existing standalone PR-tour behavior remains. |
| D12: third review | On 2026-11-10 the owner asked to “Review and improve” this pack and said “Dont implement”. | Settled: revise in place, no implementation | Recorded in [Third review](#third-review). Answers neither open choice and does not lift D09. |
| D13: native branch behavior (PUBLISH-01) | Asked whether to settle PUBLISH-01 and ACTIONS-01 with the pack's recommendations, the owner answered “Yes, both” on 2026-11-10. | Settled: the author pushes; BPR creates the PR from the verified remote head | [Native branch decision](../RFC-DRAFT-0014-draft-publication-and-continuity.md#native-branch-decision). Affects DRAFT-0041 to DRAFT-0043. |
| D14: effectful action route (ACTIONS-01) | The same answer. | Settled: interactive handoff; no BPR-supervised effectful action in this version | [Effects and authorization](../RFC-DRAFT-0015-optional-workflow-extensions.md#effects-and-authorization). Affects DRAFT-0046 and DRAFT-0047. |
| P04: local-draft conversations follow the adopted session contract | RFC-BPR-0096 was adopted after this pack was written. A local draft's chats use its durable native sessions and private pinned workspace, scoped to the draft instead of an account and pull request; a refreshed candidate is a new source binding with a new session. | Settled. Asked whether a candidate refresh should start a new agent conversation, the owner answered “Keep P04” on 2026-11-10. The answer covers that rule and the session scope as written at this revision | [Resumable interaction](../RFC-DRAFT-0013-local-tours-and-description.md#resumable-interaction). Every accepted refresh ends the continuable conversation, as a changed head does for a pull request; the earlier one stays readable. Affects DRAFT-0038 and DRAFT-0039. |
| P05: import follows the agent-identity rule | An import that replaces what an entry runs detaches that agent's retained conversations, as the same edit does in Settings; a requested relaxation of run checks applies only when accepted for that entry. | Settled. Told that P05 and P06 were still unconfirmed, the owner answered “go with your suggestions” on 2026-11-10. Follows existing edit behavior | [Inspect, bind and apply](../RFC-DRAFT-0010-agent-and-prompt-sharing.md#inspect-bind-and-apply-an-import). Affects DRAFT-0028 and DRAFT-0029. |
| P06: one BPR-defined evidence import format | Evidence arrives as one versioned JSON document; third-party report formats are translated outside BPR. | Settled. Told that P05 and P06 were still unconfirmed, the owner answered “go with your suggestions” on 2026-11-10 | [Claims and checks](../RFC-DRAFT-0012-author-evidence.md#claims-and-checks). Avoids a general report parser in core; a later RFC can add formats. |

The table links authoritative behavior rather than restating every requirement.
Stable acceptance IDs are defined once in each owning RFC. The
coverage map (not carried into this pack) derives implementation and closeout
ownership from those definitions.

## Resolved product choices

No material product choice is open. Both choices below were open through three
reviews and were answered by the owner on 2026-11-10. Asked “Also settle
PUBLISH-01 and ACTIONS-01 with the pack's recommendations?”, the owner answered
“Yes, both”. Each answer covers the recommendation as written at this revision.

### PUBLISH-01 — built-in publication of an existing worktree branch

**Settled: create the pull request from an already-pushed exact branch.** The
conflict was the current new-branch/single-squash native publisher versus the
user's existing feature-branch worktree flow. BPR's installed Git is
credential-free and hook-free, and native publication writes through GitHub's
REST API, so preserving history natively would need a credentialed push or an
exact REST commit replay. Instead the author pushes with their own Git and BPR
creates the PR from that exact remote head. The push it names targets the
resolved hosted repository, because `origin` in an isolated local clone is the
primary clone. The behavior and the alternatives not chosen are in the
[publication RFC](../RFC-DRAFT-0014-draft-publication-and-continuity.md#native-branch-decision).
DRAFT-0041, DRAFT-0042, DRAFT-0043 and publish-prepared-draft no longer wait on
a choice.

### ACTIONS-01 — effectful external invocation

**Settled: interactive handoff.** The conflict was an interactive
enterprise/harness approval workflow versus a new BPR-supervised action route.
For the first effect-capable release BPR hands an effectful action to the
configured interactive harness or terminal, whose own approval interaction owns
execution, and takes structured import/status back where supported. BPR launches
no supervised effectful action, so version-one evidence results stay reported.
Supervised execution needs a later RFC with a qualified contract. The behavior
is in the [extension RFC](../RFC-DRAFT-0015-optional-workflow-extensions.md#effects-and-authorization).
DRAFT-0046, DRAFT-0047 and workflow-effectful-actions no longer wait on a
choice.

If implementation exposes another concrete material ambiguity, add it here with
its affected scope. Do not turn every internal mechanism or optional enhancement
into a new permission gate.

## Explicit deferrals

- A general build/test runner, automated proof system and proof of arbitrary
  PR correctness are outside this pack. Existing tools can supply evidence.
- Actual enterprise companion implementation/adoption is external. The pack
  specifies a generic contract and synthetic qualifying adapter only.
- Automatic organization configuration distribution, subscriptions and executable
  skill installation are outside the first sharing slice.
- Native GHES publication expansion, Git branch management, rebasing, force push,
  merging and automatic post-publication code pushes are not silently included.
- Non-Git folders/files retain Review Cycles; first-version local PR drafts
  start from Git checkouts/worktrees.
- An effect-capable agent session for a local draft, such as an Agent tab that
  edits the candidate workspace, is not in this pack. Local-draft conversations
  are read-only; the author changes code in their own editor or harness.
- A command-line form of configuration import/export, and third-party evidence
  report formats, are outside their first versions.
- Storage layout, internal helper types, CLI spelling and detailed widget
  arrangement can be designed during implementation within the stated behavior.
  They are not additional authoring prerequisites. Actual UX judgment uses the
  repository's normal focused frozen-candidate process later.

These boundaries do not waive any required acceptance inside the defined RFCs.

## Checks and confirmation

The following are four independent facts, not stages of a single approval:

1. **Structural validity.** Passed with Python 3.15.0 on 2026-11-10 against the
   revised files. The canonical offline validator returned exit 0: “Structurally
   valid; references and semantic coverage unchecked.” Its renderer generated the
   milestone view, and the pack checker confirmed that view matches the exact
   final JSON. Pack consistency checks cover six RFCs, twenty cards, forty
   uniquely defined criteria, seven milestones, local links, criterion-to-card
   ownership, one complete closeout per RFC, dependency references, the
   original-source/schema digests and, new in this revision, that all fifty-five
   recorded source anchors still sit at their cited lines in the checkout. An
   anchor match shows where text is, not that the claim made about it holds.
   These checks do not bind live selectors or prove semantic coverage. The reproducible commands
   and machine receipt (not carried into this pack) are below.
2. **Design readiness.** All seven milestones have coherent proposed behavior
   with no identified unanswered material choice: PUBLISH-01 and ACTIONS-01 were
   answered on 2026-11-10, and P04, P05 and P06 are settled. They remain review
   drafts, not approved implementations. The publication and extension RFCs
   were rewritten around those answers after the three reviews and have not
   themselves been reviewed in that form.
3. **Confirmation.** The owner confirmed the directions and grouping recorded
   above, authorized drafting, and authorized the two recorded rounds of fixes
   and the third review's revision. Secondary review, focused re-review and the
   third review have been performed. The owner confirmed P04, then P05 and P06,
   on 2026-11-10 after reading the third review's summary. That second answer is
   recorded for P05 and P06 only. Asked separately, the owner then answered
   “Yes, both” for PUBLISH-01 and ACTIONS-01. The two added criteria and the
   added milestone prerequisite were summarized to the owner but not separately
   confirmed. These authorizations do not confirm
   every sentence, optional recommendation or UI detail. No blanket confirmation is requested
   by this handoff.
4. **Execution eligibility.** Not evaluated. No live mappings, dependencies,
   claims, native qualification or acceptance were established. The current
   mandate prohibits import/implementation; later authorized implement-rfc work
   must resolve eligibility under the consumer's existing workflow.

Commands actually run from the authoring machine:

```sh
python3 /Users/jaeger/.agents/skills/implement-rfc/examples/milestone-plans/validate.py /Users/jaeger/Downloads/bpr-local-pr-drafts-rfc-pack-v1/milestone-plan.json
python3 /Users/jaeger/.agents/skills/implement-rfc/examples/milestone-plans/validate.py /Users/jaeger/Downloads/bpr-local-pr-drafts-rfc-pack-v1/milestone-plan.json --render > /Users/jaeger/Downloads/bpr-local-pr-drafts-rfc-pack-v1/MILESTONES.md
python3 /Users/jaeger/Downloads/bpr-local-pr-drafts-rfc-pack-v1/supporting/check_pack.py --milestone-validator /Users/jaeger/.agents/skills/implement-rfc/examples/milestone-plans/validate.py --repository /Users/jaeger/Documents/GitHub/BetterPullReview
```

`--repository` is optional and compares the recorded anchors with that checkout.
It fails once the checkout moves a cited line, which is the signal to re-observe
before relying on [Current behavior](CURRENT-BEHAVIOR.md).

The pack checker performs mechanical pointer/coverage checks and invokes the
canonical helper; it is not a replacement milestone validator. A reviewer can
supply their discovered implement-rfc helper with the same argument. Repository
product tests and documentation gates were not run: this draft is outside the
repository and changed no product, repository documentation, index or board.

## Secondary review

On 2026-11-07 a separate Claude Code session reviewed version 1 against the
repository at the pinned observation revision. All 35 cited anchors matched,
the quoted limits and the six prompt purposes matched source, and the pack
checker passed. The repository's `main` moved to `15d215651` during the review;
new citations stay pinned to `154d307d7`. The owner then asked for the revisions
below. They are the reviewer's proposals applied on that instruction, not owner
answers to PUBLISH-01 or ACTIONS-01.

| Finding | Change |
|---|---|
| PUBLISH-01 omitted that BPR's installed Git is credential-free and hook-free and that native publication uses the REST Git-data API ([S13](CURRENT-BEHAVIOR.md)). Native history preservation therefore needs a credentialed push or an exact REST replay without pre-push hooks or LFS. | Each [option](../RFC-DRAFT-0014-draft-publication-and-continuity.md#native-branch-decision) states its transport consequence. The recommendation is now option 3: the author pushes with their own Git; BPR creates the PR from the verified remote head. |
| The enterprise isolated worktree (a local clone whose `origin` is the primary clone) was listed as a recovery case, although it is the user's normal start. | [RFC-DRAFT-0011](../RFC-DRAFT-0011-local-pr-drafts.md#entry-from-home-or-an-authoring-harness) resolves bounded local-remote chains read-only; AC01, AC03 and DRAFT-0031 cover it. |
| RFC-DRAFT-0012's AC02 required observed executions, but nothing in that RFC produces one. | Observed is reserved for a later BPR-supervised action; [RFC-DRAFT-0012](../RFC-DRAFT-0012-author-evidence.md#claims-and-checks) records planned or reported results, and RFC-DRAFT-0015 and the UX flow say so. |
| Tour agents run read-only tools in the live workspace ([S14](CURRENT-BEHAVIOR.md)), so a proposal could describe unselected or newer files. | The initial revision offered materialization or post-run proposal checks. The live-worktree fallback was removed by the [re-review fixes](#re-review-fixes); [RFC-DRAFT-0013](../RFC-DRAFT-0013-local-tours-and-description.md#resumable-interaction) is authoritative. |
| The extension milestone put the read-only companion work (DRAFT-0044, DRAFT-0045) behind PUBLISH-01, contrary to this record. | Split into workflow-companion-contract after local-tour-preparation and workflow-effectful-actions after publish-prepared-draft. |
| No card owned the local-authoring prompt purposes RFC-DRAFT-0013 promised, and the sharing format did not define unknown purposes. | RFC-DRAFT-0013 AC04 and DRAFT-0038 own them; RFC-DRAFT-0010 refuses an unrecognized purpose per item and applies the recognized remainder. |
| DRAFT-0031 depended on the sharing closeout, which can wait on an owner look. | The initial revision removed the card dependency. The remaining milestone prerequisite was removed by the [re-review fixes](#re-review-fixes); sharing still comes first in the default order. |
| RFC-DRAFT-0010 saved incomplete imports as a pending configuration with its own staleness. | Incomplete items stay in the preview; the package file is re-imported to bind them later. |

## Re-review fixes

On 2026-11-07 the owner requested re-review of the revised pack, then authorized
the three resulting fixes with “okay fix it in place”. The pre-fix pack and the
earlier supplied original remain outside this directory at the locations in
[Provenance](PROVENANCE.json). The fixes are:

| Finding | Applied change |
|---|---|
| Valid source references cannot prove that prose was based on the candidate rather than an unselected live helper. | [Local authoring](../RFC-DRAFT-0013-local-tours-and-description.md#resumable-interaction) uses a materialized candidate or tool-free captured context. A runtime that supports neither cannot perform that agent operation; manual editing remains available. |
| The early companion card still owned acceptance of the complete flow, including native publication. | [Extension acceptance](../RFC-DRAFT-0015-optional-workflow-extensions.md#acceptance) distinguishes local preparation from the complete built-in journey. New AC08 belongs to DRAFT-0046 and the RFC closeout, after publication; DRAFT-0044 and DRAFT-0045 can finish without creating a PR. |
| Removing the card dependency did not remove the milestone's hard sharing-closeout prerequisite. | The [plan](../milestone-plan.json) retains sharing first in default order but gives local-draft-entry no hard milestone prerequisite. Tour prompt exchange retains real implementation dependencies on DRAFT-0028 and DRAFT-0029; independent work does not wait merely for the sharing closeout. |

PUBLISH-01 and ACTIONS-01, the no-import hold, original observation revision and
provisional IDs remain unchanged. The new criterion splits an existing obligation;
it does not add an enterprise dependency or expand the publication authority.

## Third review

On 2026-11-10 the owner asked a Claude Code session to review and improve the
pack without implementing it. The earlier reviews checked the pack against the
revision it was written at. This one re-observed the checkout 344 commits later
and found the pack's largest defects there. The pack as received is preserved
at `/Users/jaeger/Downloads/bpr-local-pr-drafts-rfc-pack-v1.before-third-review-2026-11-10.tar.gz`
(SHA-256 `e839c07995db4cacc8e448205e523f12d1e90c0dc4501e99218b264346031e69`).
Both backups named in [Provenance](PROVENANCE.json) for the earlier
rounds were under `/private/tmp` and no longer exist.

| Finding | Applied change |
|---|---|
| RFC-BPR-0096 was adopted after authoring. Tour chats are durable native sessions on a private workspace pinned to the pull request's commits. RFC-DRAFT-0013 still called conversations memory-only, specified its own durable history, and described the tour workspace as the author's live worktree ([S16](CURRENT-BEHAVIOR.md)). | P04. [RFC-DRAFT-0013](../RFC-DRAFT-0013-local-tours-and-description.md#resumable-interaction) now scopes the adopted session contract to a local draft, materializes the workspace from the captured candidate, and treats a refreshed candidate as a new source binding. S7 and S14 are corrected. |
| Agent entries gained a host-minted identity that owns retained conversations and is re-minted when protocol, program, arguments or strict-run setting change ([S15](CURRENT-BEHAVIOR.md)). RFC-DRAFT-0010's Replace said nothing about it, so an import could silently strand or wrongly keep an agent's conversations. | P05. Replacement follows the existing edit rule and the preview states the consequence; the identity is never exported. |
| RFC-DRAFT-0010 let an imported definition request relaxed run checks on a new entry with no acceptance step. | P05. Relaxation applies only when accepted for that entry. |
| RFC-DRAFT-0012 promised an assistant that proposes and discusses experiments in the third milestone, but the only contract for a candidate-bound agent run is RFC-DRAFT-0013's, in the fourth. | RFC-DRAFT-0012 starts no agent run and treats proposals as records; RFC-DRAFT-0013 adds conversation-proposed claims and checks with a new AC07 owned by DRAFT-0039. DRAFT-0036 is retitled. |
| The evidence import had a size bound but no format, although a companion skill must produce it. | P06, one versioned BPR-defined JSON document. |
| A local draft had no end of life: no discard, no retention bound, and no statement that profile resets cover the new private store. | [RFC-DRAFT-0011](../RFC-DRAFT-0011-local-pr-drafts.md#a-durable-authoring-subject) defines discard, bounded retention without silent eviction and reset coverage, with a new AC07 owned by DRAFT-0032. |
| Under PUBLISH-01's recommended option the named push would, in an isolated local clone, go to `origin`, which is the primary clone, so BPR would never observe the remote head. | [RFC-DRAFT-0014](../RFC-DRAFT-0014-draft-publication-and-continuity.md#native-branch-decision) names the resolved hosted repository. The choice stays open. |
| ACTIONS-01 said no approval bridge exists. Agent-tab native sessions now carry the agent's own approvals. | [RFC-DRAFT-0015](../RFC-DRAFT-0015-optional-workflow-extensions.md#effects-and-authorization) states what exists and why it is not yet the needed route. The choice stays open. |
| DRAFT-0038 depends on DRAFT-0028 and DRAFT-0029, but local-tour-preparation did not list share-agent-prompts as a prerequisite; DRAFT-0044 still depended on the sharing closeout the re-review meant to stop waiting on. | The [plan](../milestone-plan.json) adds the milestone prerequisite; DRAFT-0044 depends on DRAFT-0028 and DRAFT-0029. local-draft-entry and author-evidence still need no sharing work. |
| “A bounded number of hops”, “merge-base semantics appropriate to a PR” and “approved serialization infrastructure” were not exact. | Four hops under the existing clone-discovery bounds ([S17](CURRENT-BEHAVIOR.md)); the three-dot comparison; glaze and the 8 KiB hook bound ([S18](CURRENT-BEHAVIOR.md)). |
| Eight cited anchors had moved and BPR-1934 had been completed. Nothing in the pack could detect that. | Anchors re-observed; `check_pack.py --repository` now fails when one moves. |

Not changed: the six-RFC grouping, the milestone order, D01–D11, both open
choices and their recommendations, the no-import hold and all provisional IDs.
Criteria are still compound in places; they were left as authored except where a
finding above required a change.

That list describes the third review as delivered. The owner's later answers to
P04–P06 and to both product choices are recorded in [Decisions](#decisions) and
[Resolved product choices](#resolved-product-choices).

## Continuation

Next: no design question waits on the owner. Import remains unauthorized until
the owner names a scope to implement. Revisions should edit these unadopted
drafts in place, preserve the supplied original and update cards, decisions,
coverage and generated milestones together. After the owner authorizes import,
use [the implementation entrypoint](../IMPLEMENTATION_PROMPT.md), retaining the
first independent slice. Do not restart intake or imply that these draft checks
constitute implementation evidence.
