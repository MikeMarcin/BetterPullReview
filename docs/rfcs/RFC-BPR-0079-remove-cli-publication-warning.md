---
id: "RFC-BPR-0079"
title: "Publish through the chosen CLI account without a second warning popup"
status: "Auto-Verified"
area: "BPR"
depends_on: ["RFC-BPR-0078"]
metadata: {"mandate_revision":"strict-cli-publication-r2","rfc_base_sha":"aa3d71fe470627e1e78488eb51a9114a788f33bd","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"}}
---

# Publish through the chosen CLI account without a second warning popup

## Adoption — 2026-10-13

The current owner mandate `strict-cli-publication-r2` authorizes implementation of the strict credential RFC followed by the publication companion, with independent critical plan and implementation reviews. It supersedes the earlier authoring-only statements retained below. The [live import](assets/strict-cli-account-publication-v1.LIVE_IMPORT.md) preserves originals, source identities, the reviewed proposal and findings. The new scope permits only the narrow transient credential exception described here; historical tokenless/per-write-warning criteria and BPR-1373 owner acceptance remain bound to their original candidates. The companion begins only after strict supported-macOS capability and evidence are accepted. No status is inferred from drafting.

## Problem and grounded current behavior

The owner reported the extra warning while approving a review and asked
for RFCs to remove both credential fallback and the scary popup. The
installed application was not independently driven or captured.

In [the composition](../../src/app/src/github_application.cpp),
`confirm_cli_publication` checks the current exact account and PR and then
opens `Confirm GitHub CLI publication`. The text warns that the active
account can switch even after the identity check. The same function serves
whole reviews, inline comments and review tours.

[The review workflow](../../src/app/src/review_submission_workflow.cpp)
already presents the host, repository/PR, head and submitting account.
Submit validates the prepared disposition, summary and selected drafts
before the CLI callback and rechecks them afterwards. The separate CLI
modal therefore interrupts an already deliberate publication gesture.

The current [read consent](../../src/adapters/qt/src/github_cli_consent.cpp)
and [account trust text](../../src/adapters/qt/src/github_presentation_text.hpp)
also describe ambient configuration, account switching and never reading a
token. Those claims need reconciliation with the companion
[strict-credential draft](RFC-BPR-0078-strict-cli-account-credentials.md).
This proposal removes the CLI-specific publication modal; it does not make
the existing connection-trust decision an automatic publication grant.

`frontend-design:frontend-design` was loaded for this proposal. Apply its
restraint and user-oriented wording within BPR's native Qt design: retain
the existing palette, fonts, controls and focus behavior. No new visual
system or mockup is proposed.

## Proposed behavior and boundaries

### One deliberate publication gesture

After the strict-credential behavior is qualified, a CLI-backed Comment,
Approve or Request changes submission proceeds from the ordinary Submit
review action without `Confirm GitHub CLI publication` or a replacement
CLI disclaimer. The inline Publish action follows the same rule. Review
tour publication retains its existing explicit immutable-plan preview and
separately selected description edit, without the additional CLI modal.

Preserve the visible exact target, account, review disposition and content
summary, the normal prepared-review checks and one-use publication grants.
Preserve exact-current validation at grant issuance even when there is no
modal event loop. Read trust, connecting, discovering accounts, reopening
and automatic connection validation never publish anything. Existing
operation-specific decisions unrelated to the transport retain their scope.

The CLI warning is removed only when the underlying operation uses the
strict credential capability. Any path that cannot establish that capability
refuses publication in the existing surface. It must not fall back to the
old warning as a substitute for strict selection. This companion dependency
is about actual runtime capability and evidence, not RFC index order.

### Explain connection behavior once, then report actual failures

Keep the explicit first-use CLI connection choice and its remembered-choice
semantics. Present it as an ordinary connection question with neutral text
naming the chosen account and server, rather than an account-switch warning.
Its primary explanation is that BPR uses that account's existing gh sign-in
and does not save its token. The details accurately explain transient token
handling and remaining executable, certificate, redirect and child-process
trust. They must not assert that BPR never reads/passes the token, that
ambient socket settings apply to the private configuration, or that switching
the global active account redirects a strictly bound request.

Accounts & hosts identifies the chosen login and retains its existing
connection/recovery controls. Technical trust details stay in the existing
connection disclosure, not a new publication modal, banner or toast.

If the chosen credential is missing, show an actionable status in the
existing review/comment/tour surface: the content was not published, the
reviewer should check the named account’s gh sign-in and credential access, and the draft remains available.
Report one honest chosen-account credential-unavailable status for missing or inaccessible credentials, distinct from tool failure and an attempted publication with an unknown result. An uncertain attempted
write keeps the existing reconciliation behavior and must not be labelled
definitively unpublished or retried automatically. Do not expose tokens,
raw process errors or Keychain internals in product text.

Example connection text: "Use alice's GitHub CLI sign-in on github.com?"
with "BetterPullReview uses this account's existing sign-in without saving
its token. This connection will be reused until you sign out or replace it."
Example missing-credential status: "Not published. Check alice’s GitHub CLI sign-in and credential access on github.com, then try again. Your draft is kept."
These describe the proposed content and placement; implementation must bind
the actual typed account, target and outcome rather than hard-code examples.

Core keeps publication policy, the host performs credential/admission work,
and Qt presents typed status. Native HTTPS, read-only previews and local
Review Cycles gain no new confirmation behavior. The
[shared proposed amendment](assets/strict-cli-account-publication-v1/proposed-cli-amendments.md) identifies the
historical per-write-warning requirements this changes.

## Acceptance criteria

- **AC01:** Composed CLI whole-review Comment, Approve and Request changes,
  inline Publish and explicit review-tour publication complete from their
  ordinary gestures without a CLI-specific modal. Their visible account,
  target, content and operation remain accurate; keyboard focus and status
  announcements follow the existing surface.
- **AC02:** Connection/read-trust acceptance, startup reuse and account
  discovery make zero writes. A changed account, head, target or frozen
  content refuses the original submission; a valid gesture still requires
  its one-use exact-operation grant.
- **AC03:** Missing or inaccessible chosen credentials and unavailable
  strict capability refuse before a write, keep drafts and show the defined
  actionable status without another-account fallback or a warning bypass.
  An ambiguous attempted publication still reconciles without a second POST.
- **AC04:** Connection and Accounts & hosts text reflects strict selection,
  transient credentials and remembered reuse without stale tokenless or
  ambient-account claims. Residual trust remains accessible in the existing
  disclosure and outside the publication flow.
- **AC05:** Focused composed publication and Qt accessibility/focus checks,
  offscreen presentation evidence and the documentation gate pass. If an
  owner visual judgement remains necessary, prepare its own short attended
  session with a checked frozen build; this draft supplies no such verdict.

## Owner outcome amendment — 2026-10-13

Mandate `strict-cli-publication-r2`: the owner answered “Yes, use credential unavailable” after the critical plan review established that stock gh does not expose missing versus denied named-Keychain failure. Use one honest chosen-account credential-unavailable outcome for missing or inaccessible credentials, preserve the draft, and direct the reviewer to check gh sign-in and credential access. Keep tool failure and an attempted indeterminate write distinct. No raw errors or fallback authority are permitted. This amendment changes that narrow status requirement only and supplies no implementation evidence.


## Qualification disposition — 2026-10-13

All AC01–AC05 are qualified on the lane candidate
`ff1143b5b3dd8d312e247eb37cc23237a8539b54`, whose production bytes are unchanged
from independently accepted M3 `34b6a43458659747648e4039a99ca52407d14624`.
[The criterion ledger](../verification/RFC-BPR-0079.md) and
[durable M4 evidence](../issues/_checkpoints/strict-cli-account-publication-v1/m4-evidence.json)
bind ordinary gestures, exact admission, strict failure and unknown-result
recovery, current disclosure, offscreen focus/accessibility, native predecessor
limits and the documentation gate. The tests-only remembered-disclosure repair
and its exact cancellation-account successor have independent no-finding receipts.
Historical failed P1/P2, native limits and BPR-1373 owner look remain intact.

At lane release, the RFC/index pair remained Proposed pending actual main
integration. No new human acceptance, remote push, release or installed-app
replacement is implied by the lane qualification.

## Main integration confirmation — 2026-10-13

Accepted product `34b6a43458659747648e4039a99ca52407d14624` and final clean lane
`1ded2e2f86b0a9c031198428d406825b6ce0ccc3` landed on local main at
`19a286231dff89b5ef71f11d5d3aafd546f855c3`. The ordered integration preserves
the accepted strict predecessor and the previously landed navigation and C++
appearance work. Its fixture-only composition digest refresh was independently
adjudicated without changing the mutation-branch pin, inventory or bypass guards.
Thirteen unique focused checks and all eight composition counterexamples pass;
the two initial remembered-connection failures and digest failure remain in the
receipts alongside their passing repairs. The documentation gate and all 67
documentation contracts pass. All 42 protected control/draft hashes are unchanged.
This terminal status confirms AC01–AC05 as recorded in the criterion ledger and
M4 handoff, preserving their native, offscreen and credential-lifetime limits.
Historical BPR-1373 owner judgment and any deferred attended review retain their
original scope. No remote push, release or installed-app replacement occurred.
Receipt: `/tmp/bpr-companion-main-landing.json`.
