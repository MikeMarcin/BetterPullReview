---
schema: "implement-rfc.card.v1"
id: "BPR-0881"
title: "macOS: profile activation can never bind for a non-default profile, because the socket path exceeds sun_path"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable sqlite/agent/qt partition, by arithmetic against the code's own limit","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0881: macOS: profile activation can never bind for a non-default profile, because the socket path exceeds sun_path

## Defect

On macOS, opening a second copy of a non-default profile gives `owner_busy`
instead of activating the window that is already running. The handoff cannot
work, and it is arithmetic rather than a race:

`src/adapters/sqlite/src/profile_activation.cpp:89-93` refuses when
`(root / "activate.sock")` reaches `sizeof(sun_path)`, which is **104** on
Darwin. The app sets both organisation and application name to
"BetterPullReview" (`src/app/main.cpp:7608-7613`), so the base root is
`~/Library/Application Support/BetterPullReview/BetterPullReview`
(confirmed at `docs/development/build-and-test.md:2662`), and a non-legacy
profile root is `<base>/profiles/profile-N`
(`profile_storage_root.cpp:39-46`, `src/application/src/profile_registry.cpp:36-39`).

    7 + len(user) + 28 + 34 + 9 + 10 + 14 = 102 + len(user)

So **every username longer than one character overflows.**

The consequence is silent. `src/app/src/github_application.cpp:12578-12601`
calls `make_profile_activation_server`, gets `unavailable`, and skips it with
a bare `if (endpoint)`. Nothing is reported, so the feature simply does not
exist for these profiles.

The contract test passes because its fixtures assert "fixture socket path
fits" (`tests/sqlite_contract/profile_activation_tests.cpp:161`) — that is,
the test pins the one condition the real root cannot meet. This is the sort of
fixture that makes a green row actively misleading, and it is the reason the
defect survived.

**Reproduction:** on macOS, create a second profile, launch it, launch it
again. Expect the running window to come forward; observe owner-busy.

## Scope

- Give the runtime endpoint a short, owner-private location rather than
  placing it beside the data root — a `$TMPDIR`-style per-user runtime
  directory is the usual answer — while keeping the identity and ownership
  checks the current path relies on. The endpoint is a rendezvous, not durable
  state, so it does not have to live in Application Support.
- Do not solve it by shortening the profile directory names: that trades one
  arbitrary limit for another and breaks existing roots.
- Make the refusal **visible**. An activation endpoint that cannot bind is
  worth a diagnostic, not an `if (endpoint)`; had it reported, this would have
  been found the first time anyone made a second profile.
- Replace the fixture's "socket path fits" assertion with a realistic-length
  root, so the contract exercises the real geometry.

## Acceptance

- A second launch of a non-default macOS profile activates the running window.
- The contract uses a root of realistic length, and fails if the limit is
  exceeded again.
- An endpoint that cannot bind reports why, rather than being skipped
  silently.
- Linux and Windows activation are unaffected.
