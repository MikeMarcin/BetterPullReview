---
id: BPR-0602
title: Implement basic Quill logging for application and GitHub failures
status: In Progress
rfc: RFC-BPR-0027
depends_on: [BPR-0600]
---

# Implement basic Quill logging for application and GitHub failures

## Scope and ownership

Owner authorized the basic foundation now. Follow RFC-BPR-0027's phase-1
amendment. Composition owns lifetime and path selection, a dedicated logging
adapter owns Quill/file effects, an application port carries bounded events,
and the GitHub adapter produces sanitized capability-error detail. No logging
or clock dependency enters core or Qt presentation.

## Acceptance

- Pin and verify Quill with license/provenance; dev and tests-disabled Release
  compose it without upstream tests/examples or implicit package substitution.
- Write startup/shutdown and viewer/capability outcome records with useful
  local request correlation, status and sanitized capability error detail to a
  documented fixed file that survives quit and is directly readable.
- Unknown GraphQL error code/message prose survives as useful bounded detail,
  while reflected active credentials, token-shaped values, authentication
  bodies, headers, source/review bodies and arbitrary response fields do not
  enter the logging queue/file. The production failure policy is unchanged.
- Bound record/queue/file work, refuse unsafe file targets and make
  unavailable/full sink behavior explicit without blocking core decisions or
  preventing Sign out/shutdown. File rotation and retention remain deferred.
- Focused sanitizer/logging/provider tests plus one credential-free composed
  application lane prove the above. Run relevant architecture/composition
  checks for changed public seams and scoped format/tidy; no live sign-in.

## Non-goals

Rotation, anonymization, privacy/verbosity preferences, export, Settings UI,
recent-context capture, profile support, broad transport dumps, full RFC closeout
and live credential/publication operations. No new UI approval is needed merely
to acquire the dependency the owner explicitly requested.

## Verification record

Implementation in progress. Existing dev/app-release roots are reused. Record
focused selectors, producing commit, limitations and cleanup on completion.
