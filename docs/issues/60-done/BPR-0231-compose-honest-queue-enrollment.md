---
schema: "implement-rfc.card.v1"
id: "BPR-0231"
title: "Compose an honest queue-enrollment input and landing surface"
status: "Done"
rfcs: ["RFC-BPR-0010"]
depends_on: ["BPR-0228"]
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

## Implemented state

- The shipped workbench exposes accessible repository, author, and path rule
  controls. Qt emits semantic add/remove intents and incrementally applies the
  host projection while preserving the exact selected saved rule.
- The serialized queue host binds refresh terminals to host/account/session and
  pull-request identity, rejects post-cancel completions, and never infers a
  GitHub role from review or comment history.
- Observer-rule order, exact matched reasons, and enrollment departures persist
  in a canonical SQLite v2 schema. A rule change and every affected enrollment
  row commit in one transaction or roll back together.
- Removing a shared rule reconciles every retained enrollment it supplied while
  preserving any real GitHub role. No changed queue state is published before
  its durable terminal is accepted.
- The two-process public contract adds a real repository rule through the Qt
  action, observes its exact matched reason, reopens it without provider
  mutation, removes it, and observes an empty landing model.

## Verification

- `core_review_queue`
- `application_review_queue_refresh`
- `application_review_queue_enrollment`
- `sqlite_queue_enrollment_storage`
- `qt_review_landing_model`
- `qt_workbench_window_scale_100`
- `application_queue_composition_contract`

The focused card matrix passes 7/7. SQLite fault injection proves that first,
middle, and final enrollment-write failures roll back both the rule and every
row; canonical v2 tamper refusal, legal v1 migration, rule precedence across
restart, wrong-reference/cancelled terminals, two-rule Qt selection stability,
and separate-process add/reopen/remove are directly covered. Independent
closeout review reported no remaining blocking or high-risk finding.
