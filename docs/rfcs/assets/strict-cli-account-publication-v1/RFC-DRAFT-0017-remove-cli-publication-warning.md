---
id: "RFC-DRAFT-0017"
title: "Publish through the chosen CLI account without a second warning popup"
status: "Proposed"
---

# Publish through the chosen CLI account without a second warning popup

## Problem and grounded current behavior

The owner reported the extra warning while approving a review and asked
for RFCs to remove both credential fallback and the scary popup. The
installed application was not independently driven or captured.

In [the composition](../../../../src/app/src/github_application.cpp),
`confirm_cli_publication` checks the current exact account and PR and then
opens `Confirm GitHub CLI publication`. The text warns that the active
account can switch even after the identity check. The same function serves
whole reviews, inline comments and review tours.

[The review workflow](../../../../src/app/src/review_submission_workflow.cpp)
already presents the host, repository/PR, head and submitting account.
Submit validates the prepared disposition, summary and selected drafts
before the CLI callback and rechecks them afterwards. The separate CLI
modal therefore interrupts an already deliberate publication gesture.

The current [read consent](../../../../src/adapters/qt/src/github_cli_consent.cpp)
and [account trust text](../../../../src/adapters/qt/src/github_presentation_text.hpp)
also describe ambient configuration, account switching and never reading a
token. Those claims need reconciliation with the companion
[strict-credential draft](RFC-DRAFT-0016-strict-cli-account-credentials.md).
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
named account must be signed in to gh again, and the draft remains available.
Distinguish missing credentials from tool failure, denied credential access
and an attempted publication with an unknown result. An uncertain attempted
write keeps the existing reconciliation behavior and must not be labelled
definitively unpublished or retried automatically. Do not expose tokens,
raw process errors or Keychain internals in product text.

Example connection text: "Use alice's GitHub CLI sign-in on github.com?"
with "BetterPullReview uses this account's existing sign-in without saving
its token. This connection will be reused until you sign out or replace it."
Example missing-credential status: "Not published. Sign in to GitHub CLI as
alice on github.com, then try again. Your draft is kept."
These describe the proposed content and placement; implementation must bind
the actual typed account, target and outcome rather than hard-code examples.

Core keeps publication policy, the host performs credential/admission work,
and Qt presents typed status. Native HTTPS, read-only previews and local
Review Cycles gain no new confirmation behavior. The
[shared proposed amendment](proposed-cli-amendments.md) identifies the
historical per-write-warning requirements this changes.

## Acceptance

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
