---
schema: "implement-rfc.card.v1"
id: "BPR-0663"
title: "Public preview refuses the epoch smart acquire"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# Public preview refuses the epoch smart acquire

Opening `microsoft/terminal#20607` through Public preview in the signed
`app-release` bundle at the pack head (bc551240, 2026-09-07) loaded the pull
request (title, author, branches, 7 review comments in the conversations
drawer) and then failed the review history: `logs/diagnostics.log` records
`op=epoch outcome=failed epoch_stage=smart_acquire failure_domain=transport
failure=invalid_request` followed by `epoch_stage=revision_prepare
failure=unavailable`, the epoch line reads "No updates loaded." and the
status line says the workbench, the review history and the exact comparison
could not be updated. The only smart acquires that ever succeeded on this
Mac were signed in (2026-09-05).

Probable cause. `anonymous_git_smart_transport_factory::acquire` in
`src/app/src/github_application.cpp` builds the anonymous wire with a
hard-coded `github_session_request_context` (request 917, identity
generation 918). `native_https_transport`'s anonymous smart binding admits a
lease only when `aContext.Revision.Source.IdentityGeneration` equals that
context's identity generation (`credential_matches` for
`anonymous_smart_request`), and the accepted revision carries the host's
real generation, so `make_native_git_smart_transport` refuses with
`invalid_request` before any request is made. The composed public-path
courses run over fixture transports and did not catch it.

## Scope

- Make the anonymous factory's context carry the generation the lease will
  be judged by (or make the binding's admission independent of a generation
  the anonymous surface never established), whichever the RFC-BPR-0017
  contract intends; keep the refusal of an authenticated authority.
- A composed check that a public-preview pull request reaches
  `revision_prepare` over the production anonymous factory.
- Owned paths: `src/app/src/github_application.cpp` (the anonymous factory),
  `src/adapters/github/src/native_https_transport.cpp` if the admission is
  the defect, their tests and the application digests.

## Acceptance

- A public pull request opened through Public preview in the signed bundle
  shows its updates on the epoch line and an exact comparison, and
  `diagnostics.log` records `smart_acquire` succeeding.
- The anonymous binding still refuses an authenticated authority and a
  GHES profile.
