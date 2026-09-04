---
id: BPR-0541
title: Persist the device-grant binding across a restart and defer the first refresh
status: In Progress
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
