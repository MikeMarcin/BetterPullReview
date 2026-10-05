---
schema: "implement-rfc.card.v1"
id: "BPR-1222"
title: "Qualify live GitHub.com review tours"
status: "Blocked"
rfcs: []
depends_on: ["BPR-1013"]
blocked_by: "safe-github-tour-target"
metadata: {"standalone_follow_up":true,"owner_amendment":"r2-20260927-close-implemented-scope","originating_pack":"bpr-review-tour-v1","originating_rfcs":["RFC-BPR-0039","RFC-BPR-0040","RFC-BPR-0041","RFC-BPR-0042","RFC-BPR-0043"],"original_criteria":["AC40-07","AC41-04","AC41-06","AC42-08"],"source_card":"BPR-1013","excluded_from_terminal_scope":true}
---

# BPR-1222: Qualify live GitHub.com review tours

## Owner amendment and retained obligation

Standalone follow-up under the owner's 2026-09-27 instruction: “Just close and land it on main anyways, can file the GHE stuff as blocked follow-ups.” The [r2 mandate](../../verification/RFC-BPR-0039-0043.owner-closeout-r2.md) accepts only the verified implementation. This card remains Blocked; no external qualification is passed by that scope change.

## Required course and blocker

Run the existing manual qualification course on an explicitly approved disposable GitHub.com PR with author and reviewer accounts. Qualify all eleven operation rows separately for native HTTPS and admitted macOS `gh api`, including pending line/multiline and off-hunk batches, mixed pending file-thread support, standalone fallbacks, pending comment/root edits, exact COMMENT submission, ambiguous-result reconciliation, published maintenance and separately previewed description edits. Record actual unsupported results without enabling unavailable operations. Observe pending/submitted visibility, website-only navigation/replies, updates and clean-profile reimport. Preserve independent server IDs, counts, actors, heads and synthetic bodies. No safe target or accounts have been supplied.

Use the [existing qualification course](../../verification/RFC-BPR-0039-0043.manual-course.md) and preserve the [candidate/evidence ledger](../../verification/RFC-BPR-0039-0043.md). No live mutation, inference, platform activation or unrelated predecessor work is authorized by this card alone.

## Original criteria retained verbatim

- **AC40-07:** Website-only reviewers can follow Start here → comments → Next/Previous → overview and reply normally; no invented heading fragment is needed.
- **AC41-04:** Native HTTPS and admitted `gh api` operations share normalized semantics and preserve their existing credential/authority boundaries.
- **AC41-06:** File-subject staging is qualified per exact operation; unbatchable destinations require an explicit compatibility plan and never an atomicity claim.
- **AC42-08:** A website-only user can continue and reply throughout completed updates; BPR reader deltas never mutate review status.

These original criteria require the direct evidence described above for this card's host/platform/provider slice. Shared criterion IDs on the GitHub.com and GHES cards partition host scope; neither card closes the other.

## Completion evidence

Record exact candidate, environment/host/provider identities, command or observed action, result and relevant human verdict. Unsupported/not-run rows retain their limitations; do not generalize fixture or historical evidence. This card starts with no passing qualification record.

## Owner designation of BPR-Test — 2026-09-30

On 2026-09-30 the owner wrote: "You can do anything you need to with the
MikeMarcin/BPR-Test github repo". That supplies the disposable GitHub.com
repository, and with it the authority to create the draft PR, push, and later
advance or force-push its test branch, as the [manual course](../../verification/RFC-BPR-0039-0043.manual-course.md)
asks. The live target qualification milestone inspected the course and did
not run it, because it still needs two things the designation does not give:

- **A reviewer account.** Only `MikeMarcin` is signed in on the Mac. Step 3
  needs a browser with "only the reviewer account" to follow the tour and
  reply, and AC40-07 and AC42-08 are about that website-only reviewer. The
  author and reviewer must be different accounts.
- **An attended session.** Steps 1–8 drive the shipped application's tour
  gestures and the website by hand and record what a person sees: pending
  versus submitted visibility, website navigation and replies, B/L/R conflict
  choices, Stop and Resume. No unattended harness substitutes for them.

The card stays Blocked on `safe-github-tour-target`. Its next step is the
owner naming a second GitHub account for the reviewer role and scheduling an
attended course on BPR-Test. No live object was made for it.

## Reassessment after the BPR-Test designation — 2026-10-05

The owner's 2026-10-05 designation (root `AGENTS.md`: BPR-Test "for any
testing need") repeats the 2026-09-30 grant of the repository and does not
change either missing input. A read-only check through `gh api` as
`MikeMarcin` found `MikeMarcin` the only collaborator on BPR-Test and no
workflows (`actions/workflows` total 0), so a review there starts no Actions.

The course still cannot run with one account:

- Step 3, AC40-07 and AC42-08 need a browser signed in as a reviewer account
  that is not the PR's author. `MikeMarcin` is the only account and authors
  every PR it opens on BPR-Test.
- Steps 1–9 drive the shipped application's tour gestures and record what a
  person sees there and on the website. That is an attended course.

The card stays Blocked on `safe-github-tour-target`. No live object was made
for it.

**Owner question:** which second GitHub account (not `MikeMarcin`) should be
the reviewer on BPR-Test, signed into a browser during an attended run of the
course?
