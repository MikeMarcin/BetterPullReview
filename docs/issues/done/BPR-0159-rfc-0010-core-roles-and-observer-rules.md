---
id: BPR-0159
title: Core: role classification, observer rules, and exclusions
status: Done
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

## What was done

`core::review_queue` holds `github_role`, `observer_rule`,
`pull_request_observation`, `queue_membership`, `enrollment_for`, and
`matches`. It builds and its suite runs under `core-only`, which links neither
Qt nor SQLite.

### A decision that was wrong the first time

The first implementation let an exclusion suppress a GitHub role: the reviewer
is the authority on their own queue, so a rule they wrote to keep something out
kept it out.

That is a way to miss work you are accountable for. A reviewer who excludes
`vendor/` and is then asked to review a pull request touching only vendored
files would never see the request, and — this is the part that makes it
serious — would have no reason to go looking, because as far as they knew they
were never asked.

Exclusions now beat other **rules** and never a role. That also matches what
the RFC asks exclusions for: handling generated and vendor paths, which is
about stopping a broad rule from over-enrolling, not about hiding assignments.
The excluded item still records the rule, so a reviewer can see what it did.

### Decisions worth stating

**`requested_reviewer` is distinct from `reviewer`.** Asked and not yet
answered is not the same as answered, and collapsing them would lose the
difference between work waiting and work done — most of what a queue is for.

**A path rule stops at a component boundary.** `src/a` matches `src/a` and
`src/a/x.cpp` and not `src/about.cpp`. A reviewer who wants the prefix writes
`src/a*`. Without the boundary a rule silently enrolls files nobody named, and
the reviewer has no way to tell which rule was responsible.

**A role and a rule are both retained.** An item enrolled twice keeps both
reasons, so deleting a rule cannot silently remove a pull request GitHub
assigned.

### Evidence

Nine tests. Four breaks, each landing on its own assertion:

| Break | First failure |
|---|---|
| path prefix without the component boundary | `a sibling sharing the prefix does not` |
| exclusions read after inclusions | `a change under the excluded path is not` |
| the matching rule is not recorded | `and the item records which rule matched it` |
| the observed role is dropped | `the observed role reaches the membership unchanged` |

### A false positive this slice exposed

`core_dependency_boundary` refused the new header. Its Qt-identifier pattern
was `Q[A-Z][A-Za-z0-9_]*` with no leading boundary, so it matched inside the
include guard `BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_QUEUE_HPP` — the `QUEUE`,
which is not a Qt type and never was.

The fix is a consistency repair rather than a judgement call. Every other
Qt-identifier pattern in the repository already carries the anchor: seven
occurrences in `VerifyArchitectureBoundaries.cmake` all read
`(^|[^A-Za-z0-9_])Q[A-Z][A-Za-z0-9_]*`. `VerifyCoreBoundary.cmake` was the lone
outlier, and it now matches them.

It still catches what it is for. Adding `using broken = QString;` to a core
header fails the scan; removing it passes. A real Qt name is always preceded by
a space, an angle bracket, a colon, or a pointer or reference token, so the
anchor costs nothing.

### On the structural claim

`queue_membership` has no field a request could occupy and `enrollment_for`
takes no provider, so "matching a rule changes nothing on GitHub" is not a rule
this code follows — it is one it cannot express. The test that names this
drives a match and asserts it happened, marking the moment a request would be
issued if one could be; there is nothing to watch for, which is the point.
