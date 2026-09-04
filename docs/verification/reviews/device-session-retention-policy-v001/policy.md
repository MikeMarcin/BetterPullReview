# Device sign-in after quitting BetterPullReview

Decision owner: the product owner. Implementation card: BPR-0541.
Prepared from `98f06fbc` on 2026-09-04. This is a policy proposal, not a
claim that persistent sign-in is implemented or verified.

## Current behavior

The shipped composition selects `delete_retained_on_shutdown` in
`src/app/src/github_application.cpp`. The application host exact-deletes both
access and refresh credentials on orderly shutdown. The binding catalog is
process-local. The RFC-BPR-0016 closeout records restart persistence as an
owner product-rule change outside its completed acceptance scope.

## Option A: Remember device sign-in

A successful GitHub device authorization remains signed in after quitting and
reopening. Access and refresh tokens stay in the platform credential vault;
only the exact authority, device-grant provenance, and lifetime/binding
metadata persist outside token storage. A visible Sign out action deletes
both tokens and the binding. Manual-token enrollment remains session-only,
and replacing a device grant with a manual token deletes the prior grant.

On restart, the application validates the exact binding and vault material
before restoring a session. Missing, corrupt, expired, or mismatched material
fails visibly and requires sign-in; it cannot select another account or use
an orphan refresh token. A valid access token remains usable while the first
refresh is scheduled from its stored lifetime. Transient refresh failures
retry only within the remaining valid lifetime. Expiry requires sign-in.

Implementing this option includes the visible sign-out action, durable binding
ownership, vault lifecycle, and a composed restart/rotation/erasure course.
Credential secrets must never enter settings, logs, fixtures, argv, or the
review artifacts. Native platform evidence must describe the platform actually
run; macOS evidence alone does not establish Windows or Linux behavior.

## Option B: Keep sign-in session-only

Keep deleting both token slots on orderly quit. Reopening requires a new
device sign-in. Do not implement BPR-0541's persistent-session requirement;
record this as a deliberate product decision rather than a verified feature.

## What this decision authorizes

Choose one policy for subsequent implementation and tests. Neither option
performs sign-in, stores or retrieves a token, sends a GitHub request, publishes
a review, approves a release, or closes an RFC. Reject both if neither describes
the desired product behavior.
