---
id: "RFC-BPR-0111"
title: "Publish a prepared draft and preserve its evidence and tour"
status: "Proposed"
metadata: {"origin":"absorbed 2026-11-10 from the local PR drafts pack v1, where it was RFC-DRAFT-0005","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
depends_on: ["RFC-BPR-0108", "RFC-BPR-0109", "RFC-BPR-0110"]
---

# Publish a prepared draft and preserve its evidence and tour

## Problem and grounded current behavior

Review Cycles currently publishes one squashed commit on a new branch through
GitHub.com's native Git-data path. Its publication journal and external-PR link
already distinguish exact plans, uncertain writes and observed remote identity.
This is not an existing feature-branch push workflow, and native GHES publication
is not established. The enterprise overview instead describes pushing the
author's branch through mandatory scripts. See [S8 and S10](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md).

The owner confirmed draft PR creation as the final publication action of local
preparation and requires a fully functional built-in flow without enterprise
dependencies. How the built-in publisher treats an existing branch's history was
a material choice between those two approaches; the owner settled it on
2026-11-10, as recorded below.

## Proposed behavior and boundaries

### Create draft PR is a deliberate boundary

The local draft offers **Create draft PR…** after the author has chosen a
destination and can preview a coherent candidate. Tests and formal proofs are
not universal mandatory gates. Show unresolved checks/questions and unavailable
capabilities honestly; an explicitly selected organizational integration may
apply its own additional requirements. Opening the preview, saving preparation,
accepting agent prose and importing a configuration grant no publication right.

The exact preview identifies source and target repositories/branches, account,
candidate, title/body, selected public evidence, intended tour/checklist operations
and any code/content transformations. It shows which material remains private.
Only an explicit author action authorizes that finite current plan. A candidate,
destination, authority or payload change requires reconciliation and a new plan.

The native first implementation creates GitHub draft PRs on the currently
qualified GitHub.com host path. It does not add merge, force push, automatic
rebase or automatic ready-for-review operations. Current native host/platform
limitations remain explicit; GHES native publishing needs its own scoped
capability qualification, not an inference from ordinary GHES reading. The
optional external publisher can serve an admitted supported host independently
under the extensions contract.

### Native branch decision

> **Reopened, 2026-11-10.** The owner reopened PUBLISH-01 after this section
> was written: on the author's press BPR itself commits and pushes through the
> author's own Git, and naming the push below remains the fallback. See
> [RFC-BPR-0114](RFC-BPR-0114-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md)
> and the [proposed amendment](assets/local-draft-authoring-v1/AMENDMENTS.md).
> This section's text is unchanged until that RFC's milestone reconciles it.

**PUBLISH-01 is settled.** On 2026-11-10 the owner chose to create native pull
requests from an already-pushed exact branch. The author pushes with their own
Git, so their credentials, hooks and remotes apply unchanged. BPR verifies that
the remote head equals the prepared candidate and then creates the draft PR
through the existing admitted REST authority. Plain Git is not an enterprise
dependency, so this satisfies the standalone requirement.

The decisive constraint was transport. BPR's installed Git runs isolated, with
no credential helper, hooks, network protocol or system configuration, and
remains local plumbing; native publication writes objects through GitHub's
Git-data REST API over the admitted HTTPS authority
([S13](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md)). The enterprise overview also
requires the push to run from the worktree that has the branch checked out, so
the repository's pre-push hooks check that tree. The chosen behavior keeps the
credential-free Git boundary, publishes the author's real commits and runs
their hooks.

Its cost is one step outside BPR: when the remote branch is missing or differs
from the candidate, preparation names the exact push to run and observes the
remote again rather than pushing. That push names the resolved hosted
repository, never a bare `origin`: in an isolated local clone `origin` is the
author's primary clone
([local-clone chain](RFC-BPR-0108-local-pr-drafts.md#entry-from-home-or-an-authoring-harness)),
and a push there would publish nothing BPR could observe. Observing the remote
head and creating the pull request need a signed-in account; signed-out
preparation stops at the named push.

Two alternatives were not chosen:

- Preserving the branch's history through a native publication path. A
  credentialed `git push` would cross the credential-free Git adapter boundary
  and raise whether the checkout's own hooks and configuration run. Replaying
  each commit through the Git-data API runs no pre-push hook, cannot carry LFS
  objects, uploads every changed blob of every commit, and keeps commit
  identities only if tree, parents, author, committer, message and any signature
  are reproduced exactly.
- Making an explicitly previewed squash onto a new branch the local draft's
  native publication. It reuses most of the current implementation but changes
  branch identity and runs none of the repository's pre-push hooks.

The existing isolated squash stays a separately selected operation. This answer
does not authorize exposing credentials, running arbitrary helpers or widening
the installed-Git ports, and the proposal installs no new general-purpose
library.

BPR does not guess which dirty files to stage or commit. Native publication
requires committed, pushed content, so when a candidate holds selected
uncommitted files or the selected publisher otherwise requires committed content, preparation hands the specific
remaining task to the author's editor/harness and recaptures the result. A
separately chosen isolated squash may include selected captured material only
through its exact preview. Local PR preparation is not a general Git client.

### Evidence describes the outgoing candidate

The local inspected candidate, the publication candidate and the actual remote
head are separately identified. Before publication, selected changes, base
movement, normalization, commit preparation or other transformations are
reconciled against evidence and tour material. A test of a workspace that differs
from the outgoing tree is not presented as a test of the PR. Byte equality may
preserve a narrowly scoped source check, but does not establish unchanged test
environment or equivalence of runtime behavior after a base merge.

After the write, read the actual PR identity, head and base through the applicable
admitted provider or qualifying external observation. Associate only the exact
result. If they differ from the authorized candidate, keep the local preparation
and expose the drift; do not remap evidence or tour stops by wishful similarity.
An already-existing matching PR can be explicitly associated without a second
creation. Uncertain or ambiguous matches do not become successful publication.

### Non-atomic publication and follow-up

Reuse the existing finite-plan and read-first journal principles for new
publication capabilities. Record remote effects and actual observations;
cancellation cannot undo an already-created branch or PR. Never blindly retry
creation after losing its answer. A failed checklist, image-hosting, tour or
ticket step after creation leaves the actual PR attached with that follow-up
incomplete. Resume only the remaining known action, under its needed authority.

The description, evidence, screenshots and tour can be prepared locally; final
PR-number-dependent links and remote uploads may be completed afterward. Image
hosting that changes remote commits must be reconciled with actual head/material
identity. It cannot silently upgrade or erase previous evidence. A tour's
publication is selected explicitly and honors the description ownership in the
local-tour RFC.

The local draft keeps its source history, private discussions, evidence and tour
after association. Home can show it as linked to a GitHub draft with the real
remote state. Reviewers receive ordinary GitHub material and need no BPR
installation. Local author verification is not GitHub review approval. Further
shared changes return through existing synchronization; this pack does not
introduce a second collaborative server or automatic post-publication pushes.

### Fitted to this pack's other decisions

These rules come from the [reconciliation record](assets/v0-5-0-prerelease-targets-v1/RECONCILIATION.md); its row numbers are given.

- **Reporting (X18).** Creating the draft pull request and each follow-up
  show their steps as waiting, sending, sent, failed or not sent, and report
  a failure as an error-role banner naming what stopped, why, what is on
  GitHub and what moves forward, as [RFC-BPR-0099](RFC-BPR-0099-tour-publishing-in-reach-and-reported.md) defines for tour publication. The
  follow-up tour publication chooses its own form under that RFC.
- **Unaccepted stops (X20; confirmed by the owner, 2026-11-10).** Stops
  the author has not accepted under [RFC-BPR-0106](RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md) block publishing the tour. They
  do not block Create draft PR…, whose preview lists them as unresolved.
- **Other work is not held up (X22).** The new creation and follow-up
  effects cannot hold up a review submission, and each shows a stated
  condition with a bounded wait, as [RFC-BPR-0105](RFC-BPR-0105-a-review-opens-fresh-and-a-submission-always-ends.md) requires. The read-first journal
  and the rule against blind retry are unaffected.
- **Home after association (X02).** The sentence above that Home can show a
  linked local draft is read with the one-row rule in
  [the local draft RFC](RFC-BPR-0108-local-pr-drafts.md#fitted-to-this-packs-other-decisions).

## Acceptance criteria

- **AC01:** From a branch the author has pushed, the built-in path creates a GitHub draft from a local preparation without enterprise tools, publishing the author's own commits unchanged. When the remote branch is missing or differs from the candidate, BPR names the push to the hosted repository and pushes nothing itself; checkout data is unchanged throughout.
- **AC02:** Publication previews bind exact candidate, destination, account and selected public content; changed inputs invalidate authorization and private unselected records remain local.
- **AC03:** Different inspected, outgoing or observed remote material cannot inherit current evidence or tour anchors without an explicit justified reconciliation; dirty material and base movement are represented honestly.
- **AC04:** Lost answers, cancellation, concurrent drift and a pre-existing PR are handled without blind retry, duplicate creation, force update or pretend rollback.
- **AC05:** A created PR with failed follow-up remains associated and recoverable; rerunning follow-up cannot recreate the PR or overwrite an independently edited description.
- **AC06:** The resulting ordinary GitHub draft, description and selected tour can be followed without BPR, while the author's local preparation remains resumable and native-host limitations remain explicit.
- **AC07:** Creating a draft pull request with follow-ups shows each step advancing and a failed step as an error-role banner with the recovering action; a tour with unaccepted stops can be created as a draft pull request but its tour cannot publish until they are accepted; and a review submission for another pull request completes while a creation is in flight.

## Proposed delivery

[Native publication](../issues/10-backlog/BPR-2029-create-the-draft-pr-from-the-pushed.md), [association and follow-up](../issues/10-backlog/BPR-2030-associate-publication-and-resume-remaining-follow-up.md)
and [closeout](../issues/10-backlog/BPR-2031-close-rfc-0111.md) form the fifth [milestone](plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
The [decision record](assets/v0-5-0-prerelease-targets-v1/local-drafts/DECISIONS.md#resolved-product-choices) holds the owner's
answer on native branch behavior.
