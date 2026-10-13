---
schema: "implement-rfc.card.v1"
id: "BPR-1414"
title: "Repair Home role filters and grouping, lead with titles, and remove recent entries"
status: "Done"
rfcs: ["RFC-BPR-0026", "RFC-BPR-0032"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3eebd2d827cd3fdad6c257dbcd2be3919e96afe0","evidence":{"kind":"observation","detail":"Core roles, Qt filter model, native and CLI composed Home actions pass for Requested, Author, Reviewer, Any and overlapping/no roles; application_review_queue_enrollment also passes."}},{"criterion":"AC2","outcome":"pass","revision":"3eebd2d827cd3fdad6c257dbcd2be3919e96afe0","evidence":{"kind":"observation","detail":"Native and CLI personal-discovery public paths exercise real grouping actions, window-local state, selection, headers on/off and SQLite restart; repository and organization rows omit identity repeated by the group header."}},{"criterion":"AC3","outcome":"pass","revision":"3eebd2d827cd3fdad6c257dbcd2be3919e96afe0","evidence":{"kind":"observation","detail":"frontend-design loaded; title-first Home renders inspected in light, dark and high contrast, with long wrapped titles at minimum width; Qt Home/list/window scale 100/125/200 tests pass, including literal text, author and accessibility checks."}},{"criterion":"AC4","outcome":"pass","revision":"3eebd2d827cd3fdad6c257dbcd2be3919e96afe0","evidence":{"kind":"observation","detail":"Core/host tests preserve exact account identity and open reviews; Qt menu/Delete/Backspace tests pass; both composed account courses remove from all windows without provider work and restart with no removed recents restored."}},{"criterion":"AC5","outcome":"pass","revision":"3eebd2d827cd3fdad6c257dbcd2be3919e96afe0","evidence":{"kind":"observation","detail":"Before the fix, the first-open queue-composition check reported quiet for changed code. Afterward application_queue_composition_contract passes with changed_since_my_review; the accepted submitted review commit is the baseline on the first read as on refresh."}}]
---

# BPR-1414: Home filters, grouping and Recently viewed

The owner reported that a CLI account's Reviewer and Requested filters hid its
pull requests, Repository grouping appeared ineffective, and repository/number
identity dominated the PR title. They also requested removal from Recently
viewed. The 2026-10-12 takeover mandate is to finish Claude's work directly and
land it on main. The reserved ID and existing WIP are retained.

## Acceptance

- **AC1:** My role uses accepted membership reasons, including requested and
  reviewed discovery and overlapping roles, for native and CLI account rows.
  No role excludes rows with any such role. Exercise the composed filters.
- **AC2:** Repository grouping changes the real Home list, survives the host
  round trip and restart, and remains window-local. Verify state headers on
  and off without losing selection or hiding rows.
- **AC3:** A known PR title leads the row; repository and number are secondary
  and omit the part already named by a group. Unknown titles retain identity.
  Authors, literal wrapping, accessibility and native selection remain intact.
  Inspect offscreen application renders using the frontend-design skill.
- **AC4:** A recent row can be removed through its menu or Delete/Backspace.
  Removal updates the profile's windows, persists across restart, preserves
  other destinations and open reviews, and makes no GitHub request. Reopening
  the PR may add it again. The existing ten-entry, no-expiry bound remains.
- **AC5:** Opening a PR preserves its outstanding review work. A PR whose
  accepted head differs from the viewer's submitted review remains in Needs
  me on the first open after restart; the Reviewing chip changes no policy.
  This additional owner report arrived during the takeover on 2026-10-12.

## Execution

One direct milestone delivers AC1–AC5 together. Base before Claude's work:
`241c21087`. Takeover preserved all uncommitted edits at `b229fdf1b` and merged
current main without conflicts. No running command from this worktree remained.
The registered main project's direct preflight passes; this legacy worktree
is not separately registered with MASK. No agents or global settings are used.

## Evidence

Verified on candidate `3eebd2d82`, including main's header/source navigation
and scrollbar changes. The frontend-design skill was loaded from the owner's
specified Claude plugin path; the implementation retains the native Qt font,
palette, selection and accessibility.

- **AC1:** Core role cases cover discovery flags, overlapping roles, No role
  and accepted submitted-review activity. The native and CLI personal-discovery
  public paths exercise the real Requested, Author, Reviewer and Any actions.
  Observed roles do not change durable enrollment reasons.
- **AC2:** Both public paths switch repository, organization and account
  grouping through the actual menu actions, preserve selection and rows with
  state headers on and off, leave the sibling window independent, and restore
  repository grouping after SQLite restart. This course did not reproduce a
  broken grouping dispatch. Grouped rows now omit identity already in the
  group header, making the grouping visible without repeated repository text.
- **AC3:** Landing-model, Home-list and window tests pass at 100%, 125% and
  200% where registered. Inspected the full composed Home in light, dark and
  high contrast, including a long title at the minimum window width, and the
  grouped Home-list renders. Literal titles wrap; authors remain readable;
  identity is secondary and full identity remains available to accessibility.
  The render course waits for Qt's delayed row layout before capture.
- **AC4:** Core and host tests cover exact account/destination removal,
  retained ordering, no-op removal, open-review preservation, persistence and
  re-adding on a later open. Qt tests exercise the row menu and Delete/Backspace.
  Both composed account courses remove recent entries in every profile window,
  observe no provider operation, then restart again with the list still empty.
- **AC5:** The new first-open check failed before the correction with
  `bucket=quiet`; the queue-composition contract now passes with
  `changed_since_my_review`. The baseline comes from the accepted submitted
  review's commit, including this process's first read. The Reviewing chip
  remains a presentation fact.

The focused Home/host/public-path set is 17 tests, all passing after correcting
the CLI course's redundant explicit sign-in during remembered startup. Core-only
attention/restoration and both architecture boundary checks pass (4/4).
The application queue-enrollment suite also passes, preserving enrollment and
observation semantics with the projected submitted-review role.
After integration, the six composed Home/window/queue checks pass again.
Clang-format 22 passes all 22 changed C++ files; scoped clang-tidy passes with
existing warnings outside the changed lines. The documentation gate and its
67 tests pass. Logs and renders are local ignored build output, not committed.
