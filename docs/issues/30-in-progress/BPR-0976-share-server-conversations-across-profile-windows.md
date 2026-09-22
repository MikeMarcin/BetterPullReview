---
schema: "implement-rfc.card.v1"
id: "BPR-0976"
title: "Share server conversations across exact-authority profile windows"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0697", "BPR-0699", "BPR-0706"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0024-closeout-r2"}
---

# Share server conversations across exact-authority profile windows

## Scope

Complete RFC-0024 AC-03/08/09: a fresh accepted server conversation snapshot
in either window reaches exact-profile/account/repository/PR siblings.
Added, edited, deleted, resolved and reopened conversation state is shared;
each window retains its own comparison, file, navigation and scroll state.
Application hosts coordinate delivery through the serialized decision lane;
Qt is not an inter-window bus. Authentication replacement, pending navigation,
late completions and detached windows cannot receive stale authority.

Owned surfaces: core GitHub window read policy; application GitHub read/profile
hosts and narrow contracts; application composition and fixture; Qt's
content-only update presentation; focused host, core, Qt and composed
multi-window tests. No live provider writes.

## Acceptance

- Exact-authority sibling windows receive accepted added/edited/deleted and
  resolve/reopen conversation state without moving either view.
- Different profiles/accounts/PRs, retired sessions and detached or newly
  navigating windows do not receive inapplicable updates; fanout is bounded
  and reentrant-safe, with no refresh loop or additional provider request.
- The shortest composed refresh path and discriminating stale/negative tests
  pass; the relevant core/host and architecture checks pass.

## Evidence

Baseline at `444d215c`: 18 focused profile/window selectors pass, including
the composed multi-window path; this alone does not prove server fanout.

### Implementation and self-review — 2026-09-21

Candidate: working tree over helper claim `66ede915`. The core admits an
already-accepted sibling observation only for the same profile/session,
repository/PR/locator/number, with no pending read and a fresh profile-ordered
sequence. The profile host snapshots its at-most-32 attachments; the window
host freezes the accepted payload and session before callbacks. Each recipient
rechecks lifetime, session and sequence. A reentrant close cannot invalidate
the iteration or receive a stale projection; a reentrant newer sequence
cannot be overwritten by the outer delivery.

Recipients install the complete validated server snapshot, renew their own
revocable mutation/body-link authority, and publish a content-only projection.
They do not issue a provider read, change request/navigation identity, or
deliver another epoch handoff. This shares server records without replacing
the recipient's retained comparison. The explicit presentation flag preserves
Home/modality, the reviewer's filters and named-comparison choice; Qt remains
only a presenter, not the sharing bus. Shared publication has no provider
AcceptedSnapshot delta, preventing feedback and restoration replay.

The code-simplifier pass consolidated identical snapshot/markdown/lease/rate
installation into one host helper without changing either acceptance path.
Self-review covered source/target lifetime, profile/account generation,
pending navigation, sequence ordering, mutation revocation and immutable
snapshot delivery. No provider grant or live mutation path was added.

The composition pin review covers only this card's changes after `66ede915`;
`git log fc38d1ce..66ede915 -- src/app/src/github_application.cpp` is empty,
so there is no unreviewed intervening composition edit. The production change
is the fresh-provider-acceptance flag passed to the profile observer. The
other application edit is a test-only fake-server staging helper, inside the
existing BUILD_TESTING scope; it changes wire replies, never accepted state.
The full application digest becomes
`c9fb87730e3e9edc7326e892e353fe74e03d1bb05cac3d32fc0bbcf0e2fa8ef4`;
production manifest
`b5b86e332f6190e77c6019bd4552de8a9cb493698f51312100131a6ae06f2c76`.
The production mutation-branch digest remains
`a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.

### Direct evidence

- Core, application GitHub host/read-host and composed multi-window selectors:
  **4/4 pass, 80.270 s** (`build/bpr0976-public.json`), including actual
  Refresh, rendered added/edited/deleted replies and resolve/reopen, retained
  exact comparison/selection/scroll, Home/Discuss and no model reset.
- The first composed attempts failed because the course inspected a thread
  outside its retained line filter (line 3 selected, thread on line 2).
  Both accepted snapshots already matched revision 4. The course now explicitly
  opens Discuss, verifies the existing root is rendered, then performs Refresh;
  no acceptance wait or product predicate was weakened. The investigation also
  identified and fixed the shared-projection first-open navigation side effect.
- A temporary removal of the profile host's `share_snapshot` call produces
  the exact host-test failure “added, edited, deleted and resolve/reopen server
  observations reach sibling” (CTest exit 8, 0.368 s,
  `build/bpr0976-negative.json`). The call is restored before final checks.
- Final core/host tests (including nested newer completion during publication),
  focused Qt navigation, and both shipped composition contracts: **6/6 pass,
  34.177 s** (`build/bpr0976-final-focused.json`). Architecture boundary checks
  pass. The focused Qt course independently passed in 1.045 s.
- The full `qt_workbench_window_scale_100` suite stops before this new course
  at its existing “epoch row is the first row” assertion, already tracked by
  [[BPR-0940]]. This run is not claimed passing; the new explicit selector runs
  the unchanged navigation assertions directly. No native/human acceptance is
  inferred from it.
- Milestone `close-rfc-0024`, mandate r2, remains bound to definition
  `29fe9331a3b49ac524e0888864f019c22c1a0213ca9fb53d2635dbe4f0323b99` and
  obligation set `d0795d9e329ca941a49781d0d92347ed1f98769fa479a72cbc87b0ba39ba83c8`.
  The observational binding is retained in
  `build/rfc0024-r2-milestone-checkpoint.json`; the authored plan remains the
  authority. Other slices and native/attended obligations remain unfinished.
