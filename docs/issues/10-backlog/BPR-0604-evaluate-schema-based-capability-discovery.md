---
schema: "implement-rfc.card.v1"
id: "BPR-0604"
title: "Evaluate schema-based capability discovery"
status: "Backlog"
rfcs: ["RFC-BPR-0016"]
depends_on: ["BPR-0559"]
---

# Evaluate schema-based capability discovery

The owner explicitly keeps BPR-0559's nine-query repair for now and requests
later consideration of a schema-based method. This is an optional optimization,
not a prerequisite for that repair or RFC-BPR-0016's existing acceptance.
Do not implement it until this backlog work is selected.

## Question

Compare the current selected-type batch with root introspection and GitHub's
[official public schema download](https://docs.github.com/en/graphql/overview/public-schema).
`__schema { types { name } }` alone cannot replace the existing field, argument,
enum, input, payload, and wrapper checks. The current fixtures already derive
from the official schema; they did not model the live repetition policy.

## Acceptance

- Determine whether a complete root introspection projection avoids the live
  repetition limit while preserving every capability predicate. Compare actual
  response bytes, parse work, memory bounds, round trips and connection latency.
- Compare primary GraphQL point cost and secondary limits with the nine-part
  plan. GitHub documents a usual user allowance of 5,000 points/hour and a
  minimum one point per query; the actual nine-part live cost has not been
  measured. Use response metadata or an explicitly authorized measurement,
  rather than assuming request count equals primary point cost. Reference:
  [GitHub rate limits](https://docs.github.com/en/graphql/overview/rate-limits-and-query-limits-for-the-graphql-api).
- Evaluate download freshness, conditional retrieval and cache invalidation,
  offline/stale behavior, and exact GitHub.com versus supported GHES version
  matching. Distinguish a published schema from capabilities observed on the
  selected connected server; neither proves target mutation permission.
- Record a recommendation and tradeoffs, including retaining the current plan
  if alternatives are worse. Any implementation requires a bounded follow-up
  with regression checks for malformed/partial data, authority correlation,
  rate exhaustion and cancellation. Do not silently enlarge response limits,
  introduce unbounded schema retrieval, or promote stale/unverified facts.

## Status

Not started. No live calls, credential operations or implementation changes
are authorized by creating this card. The current nine-query plan remains.

The later [[BPR-0605]] observation completed the nine-query capability course
successfully in about 6.49 seconds. This is one live sample, not a benchmark.
Its post-discovery UI showed 4,989/5,000 GraphQL points remaining, but there is
no pre-run baseline from which to attribute this batch's actual point cost.
