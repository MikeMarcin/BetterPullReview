---
id: BPR-0231
title: Compose an honest queue-enrollment input and landing surface
status: Ready
rfc: RFC-BPR-0010
depends_on: [BPR-0228]
---

# Compose an honest queue-enrollment input and landing surface

The queue components cannot currently produce a truthful item. Enrollment
requires an observed GitHub role or a matched local observer rule, and the
application supplies neither.

## Required final state

- Supply at least one real RFC-defined enrollment input. The bounded first
  slice should compose a user-configured local observer-rule surface, because
  repository, author, and path rules are mandatory RFC behavior and do not
  require inventing a provider role.
- Persist the rule and the exact reason it matched; exclusions remain local and
  never hide a GitHub-assigned review.
- Compose enrollment, the landing model, and incremental queue projection into
  the shipped application.
- Keep refresh scheduling a separately measurable effect path with identity,
  cancellation, offline, ETag, and rate-limit behavior.
- Prove through the public path that deleting the rule or cutting the observed
  match empties the queue instead of leaving a synthetic item.
- Extend the standing composition inventory to the landing model as well as
  refresh and enrollment, and pair it with a gesture-to-projection trace.

BPR-0223 owns notification delivery requests and activation selection after
this card supplies a real queue item and a real landing target.
