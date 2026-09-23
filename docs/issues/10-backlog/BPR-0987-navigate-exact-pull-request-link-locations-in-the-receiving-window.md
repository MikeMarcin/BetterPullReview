---
schema: "implement-rfc.card.v1"
id: "BPR-0987"
title: "Navigate exact pull request link locations in the receiving window"
status: "Backlog"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0598"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0026-planning-reconciliation","criteria":["AC-07","AC-08"]}
---

# Navigate exact pull request link locations in the receiving window

## Scope

RFC-BPR-0026 AC-07 and AC-08 on top of what [[BPR-0669]], [[BPR-0689]] and
[[BPR-0732]] already ship. At planning head `2722bd67` the Home bar hands
pasted text to `application::parse_setup_destination`
(`src/application/src/setup_destination.cpp:41-127`), which recognizes the
canonical pull request, `/files`, `/commits`, `/commits/<oid>` and a
fragment, drops the query, and refuses a repository commit URL or any other
subpath. The location survives only as a string, and every non-empty one is
reported "unavailable" after the open (`:260`), including `/files` and
`/commits`, which name no location at all. A paste of the pull request the
window already shows dispatches a fresh `open_pull_request_intent` and a
second snapshot read (`github_application.cpp`, setup `open`).

- **Typed location.** The parser yields a bounded typed location instead of
  text: the pull request (canonical, `/files`, `/commits`, which keep the
  retained view and say nothing), a PR commit subpage, a file fragment
  (`#diff-<sha256 hex of the path>`, optionally with an `L<n>`/`R<n>` line),
  and the comment fragments GitHub writes (`#discussion_r<id>`,
  `#pullrequestreview-<id>`, `#issuecomment-<id>`). The requested text is
  kept for the explanation. Unknown fragments are a recognized pull request
  with an unavailable location, never a refusal of the pull request.
  Query parameters stay dropped: they never route a request or a credential.
- **The core decides exactness.** A pure core decision maps a typed
  location against facts of the snapshot the receiving window accepted: the
  changed paths (hashed with the core's SHA-256, so a file fragment names
  exactly one changed file or none), the reviews and comments the read
  carries with their provider identity, and whether the window's comparison
  is the pull request's own base-to-head comparison (only then does a line
  map). Anything it cannot establish exactly is `unavailable` with a reason;
  it never picks a nearby line, another file or another revision. A PR
  commit subpage is unavailable with that explanation, since the epoch line
  compares uploads, not single commits (RFC-BPR-0021 is not assumed).
- **Receiving window.** A link to the pull request the window already has
  accepted navigates within it with no new snapshot read; otherwise the pull
  request opens there and the location applies once that window accepts
  the snapshot, through the same file-selection and conversation-reveal
  paths an ordinary gesture uses. A newer gesture, a different pull
  request, or an account or profile change retires the pending location.
- **Explanation.** A mapped location says nothing; an unavailable one opens
  the pull request and says, where Home reports refusals now, what was asked
  for and why it could not be shown.

Owned surfaces: `src/core` (typed location and its resolution), the
application setup destination and the host step that applies an accepted
location, `src/app/src/github_application.cpp` setup and reading wiring,
their tests. No provider, credential or Qt authority change; Qt keeps
passing text.

## Acceptance

- `application_setup_destination` pins a table of inputs: canonical,
  `/files`, `/commits`, `/commits/<oid>`, file fragments with and without a
  line, the three comment fragments, an unknown fragment, extra query
  parameters, a repository commit URL, other subpaths and malformed input,
  each to its exact typed result or refusal.
- A core test pins the resolution: exact file by digest, a digest naming no
  changed file, a line only under the full comparison, a review or comment
  present and absent, and the commit subpage reported unavailable.
- `application_setup_destination_public_path` drives the Home bar in the
  composed application: a file link selects that file (and its line under
  the full comparison), a comment link reveals its conversation where the
  read carries its identity, an unavailable location opens the pull request
  with the explanation naming the requested location, a malformed or
  non-PR link sends nothing, and pasting the open pull request's link again
  issues no new snapshot read.

## Evidence

Not run at allocation (planning head `2722bd67`).
