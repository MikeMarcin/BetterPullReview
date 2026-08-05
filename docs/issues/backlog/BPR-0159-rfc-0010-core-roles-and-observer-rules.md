---
id: BPR-0159
title: Core: role classification, observer rules, and exclusions
status: Backlog
rfc: RFC-BPR-0010
depends_on: [BPR-0158]
---

# Core: role classification, observer rules, and exclusions

The queue's first question is why a pull request is in it at all. Five roles
come from GitHub — author, reviewer, requested reviewer, participant — and one
comes from nowhere but this machine: a local observer rule on a repository, an
author, or a path.

## The rule this card must not erode

**Observing is local.** Matching a rule enrolls the pull request here and
changes nothing on GitHub. The correlation is a pure function with no provider,
so there is no path by which a match could issue a request. That is the shape
to build; a guarded path would be the wrong answer even if the guard held.

## Required direct evidence

- Every role is derived from a sanitized fixture observation and from nothing
  else, under `core-only`.
- Repository, author, and path rules each enroll, and exclusions keep generated
  and vendor paths out.
- A path rule and an exclusion that both match resolve one way, deliberately,
  with the reason recorded.
- The type carrying a rule match has no field for a request, an assignment, or
  a subscription — the absence is the guarantee.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
