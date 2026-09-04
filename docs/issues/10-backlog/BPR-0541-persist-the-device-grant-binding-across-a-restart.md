---
id: BPR-0541
title: Persist the device-grant binding across a restart and defer the first refresh
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0540]
---

# Persist the device-grant binding across a restart and defer the first refresh

## Why

The second independent review of RFC-BPR-0016 ([[BPR-0285]], 2026-09-04)
found that the "restart resume" [[BPR-0536]] composed is unreachable: the
credential binding catalog is process-local, `resume_profile` has no caller
in the composed application, and an activation without a binding fails with
`vault`, so no session survives a restart and nothing re-arms renewal after
one. It also found that `resume_refresh` begins a refresh at once, so a
transient transport failure on resume becomes `reenrollment_required` while
the access token still works.

## Required final state

- A device-grant binding persists across a restart in the catalog the vault
  already anchors, so a launch with a stored grant resumes the session and,
  through [[BPR-0540]]'s provenance rule, arms renewal without a new code.
- The first refresh after a resume is scheduled by the stored lifetime, not
  begun at once; a transient failure retries within the lead before it
  reports re-enrollment.
- A composed course restarts the application over the fixture with a
  stored grant and proves the resumed session, `REFRESH_RESUMES:1`, and one
  rotation; the documents and the ledger row describe the persisted binding.

## Non-goals

- Persisting a pasted-token session.

## Owner-policy checkpoint — 2026-09-04

Claimed for the remaining-issues run. The RFC-BPR-0016 closeout explicitly
classifies this as a product-rule change for the owner: composition currently
selects `delete_retained_on_shutdown`, and BPR-0540 requires both access and
refresh slots to share that erasure path. Persistent bindings alone cannot
make a session resume when its tokens were intentionally deleted. A concrete
attended policy decision is prepared before changing that credential lifecycle.

## Decision prepared — 2026-09-04

The [offline policy review](../../verification/reviews/device-session-retention-policy-v001/offline-package/index.html)
binds two alternatives: remember device grants with a visible Sign out action
and exact restart validation, or retain session-only sign-in. Its exact
[policy proposal](../../verification/reviews/device-session-retention-policy-v001/policy.md)
names the affected shutdown, manual-token replacement, expiry, vault, and
restart-course behavior. The batch is
`bpr:/attended-review/betterpullreview/2ae811784b1f4aba21e7e09e`.

No verdict exists. No implementation, credential access, or network operation
was performed for this card. Once the owner exports a complete verdict, validate
it against `offline-package/attended-review-manifest.json` using the attended
review builder, record the response immutably, and apply only the chosen policy.
The card parks on this product decision; all its dependencies are terminal.

## Owner decision validated — 2026-09-04

The owner selected **Remember device sign-in** and left the session-only
alternative not chosen. The complete exported verdict validates against batch
`bpr:/attended-review/betterpullreview/2ae811784b1f4aba21e7e09e`; the
[immutable validation record](../../verification/reviews/device-session-retention-policy-v001/verdict-validation.json)
binds the manifest, received response, decoded JSON, and official validator.
Only HTML space entities and Markdown underscore escapes introduced in message
transport were decoded. No decision value changed.

The chosen policy is now authorized for implementation, including a visible
Sign out action and manual-token replacement erasure. Credential operations,
live mutations, implementation completion, and RFC closeout remain unclaimed.
The product-decision blocker is removed; this card returns to the dependency-
ready backlog while the integrator finishes the claimed BPR-0316 slice.
