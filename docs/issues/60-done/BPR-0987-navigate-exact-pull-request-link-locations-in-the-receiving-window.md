---
schema: "implement-rfc.card.v1"
id: "BPR-0987"
title: "Navigate exact pull request link locations in the receiving window"
status: "Done"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0598"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0026-planning-reconciliation","criteria":["AC-07","AC-08"]}
acceptance: [{"criterion":"application_setup_destination pins a table of inputs (canonical, /files, /commits, /commits/<oid>, file fragments with and without a line, the three comment fragments, an unknown fragment, extra query parameters, a repository commit URL, other subpaths, malformed input) to their exact typed result or refusal","outcome":"pass","revision":"17e40dc8","evidence":{"kind":"command","detail":"ctest -R '^(core_setup_destination|application_setup_destination)$' passes in build/dev and build/core-only at 17e40dc8's tree: typed parse table incl. commit pages with fragments, uppercase digests lowercased, nine unread fragments as unrecognized, five malformed lines, dropped query parameters, five non-PR pages refused, every parsed location valid; setup host reports Opened without judging the location and Navigated for a shown pull request."}},{"criterion":"A core test pins the resolution: exact file by digest, a digest naming no changed file, a line only under the full comparison, a review or comment present and absent, and the commit subpage reported unavailable","outcome":"pass","revision":"17e40dc8","evidence":{"kind":"command","detail":"core_setup_destination: SHA-256 digest of the empty string; exact digest; absent and shared digests unavailable; R42 maps to zero-based 41 only when the comparison ends at the head, else line_needs_head; L3 base_side_line; review 4001 and comment 71001 revealed, 4002/71002 explained; issue comment, commit page and unrecognized explained; invalid typed input decides nothing; every refusal has an explanation."}},{"criterion":"application_setup_destination_public_path drives the Home bar in the composed application: file link selects the file (and line under the full comparison), a comment link reveals its conversation where the read carries its identity, an unavailable location opens the pull request with the explanation naming the requested location, a malformed or non-PR link sends nothing, and pasting the open pull request's link again issues no new snapshot read","outcome":"pass","revision":"17e40dc8","evidence":{"kind":"command","detail":"application_setup_destination_public_path passes 52.76 s with run_link_location_course: /files#diff-<secondary.cpp>R2 selects secondary.cpp right line index 1 in Read; same-PR #diff-<review.cpp> moves file with AccountReadCount unchanged; #pullrequestreview-4001 reveals review:PRR_fixture_1 in Discuss; #discussion_r123456 (authenticated read has node ids only), L2, commit page, absent digest and #issuecomment-99 explained under the bar and in the status bar with the requested location and no read; a repository commit link opens nothing. Negative controls (all digests refused; never reuse; no Recently viewed shortcut) fail their named assertions. application_profile_window_review_restoration_public_path passes all phases 591.97 s incl. the Recently viewed row of the shown pull request landing on Overview with no read."}},{"criterion":"Changed-surface upkeep: boundaries, format, tidy, documentation, composition digest reseal with review","outcome":"pass","revision":"17e40dc8","evidence":{"kind":"command","detail":"core_boundary_contract, core_dependency_boundary, architecture_boundary_contract pass; architecture_boundaries passes on a clean export (the checkout stops on the stray .claude worktree); clang-format check on 14 files; strict tidy clean on small TUs and on the changed lines of the large ones; application_review_round_trip_composition_contract passes after the reviewed BPR-0987 reseal; application_composition_contract fails only on BPR-0983's recorded assertions; qt_composed_github_scale_100 and qt_anonymous_public_preview_scale_100 stop at their known pre-existing assertions (BPR-0940 and the recorded Connect assertion) as at HEAD; docs/ux/workbench-specification.md updated; verify_documentation.py DOCUMENTATION_OK; git diff --check clean."}}]
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

## Implementation and verification — 2026-09-22

Source commit `17e40dc8` on `main`, claimed at `9289d193`.

- **Core.** `core::pull_request_location` (kind, commit, file digest, side,
  line, identity) and `core::resolve_pull_request_location`, which decides
  from `pull_request_location_facts` alone: a digest names a changed path only
  when exactly one changed path hashes to it (`pull_request_file_digest`, the
  core's SHA-256 in lowercase hexadecimal); an `R<n>` line is kept only when
  the comparison on screen ends at the pull request's head; an `L<n>` line
  never is (GitHub numbers it against a merge base the comparison does not
  use); a review or comment is revealed only when the read identified it by
  that number; a commit page, an `#issuecomment-` link and an unread fragment
  are explained. `setup_destination` keeps the requested text and gains the
  typed `Target`.
- **Application.** `parse_setup_destination` reads `/files`, `/commits`,
  `/commits/<oid>`, `#diff-<64 hex>[L|R<n>]`, `#discussion_r<n>`,
  `#pullrequestreview-<n>` and `#issuecomment-<n>` into the typed target; an
  unread fragment is `unrecognized` (the pull request is still named), query
  parameters stay dropped, and non-PR or other subpaths are still refused. The
  setup host no longer judges a location; `setup_destination_port::open`
  returns whether it read or navigated, and the projection carries
  `Navigated`. `pull_request_link_location_facts_of` gives the facts of one
  accepted snapshot with the conversation identities the projection uses.
- **Composition.** `open` keeps a link to the pull request the window already
  shows under the same profile (no second snapshot read) and otherwise opens
  it; the pending location is placed once that window accepts the pull
  request, then its file list, then the file's diff, through the ordinary
  paths (`select_workbench_file_intent` in Read, `restore_selection` on a
  semantic line, the conversation panel's `choose` in Discuss). A newer
  gesture, an account change, a different pull request or another file chosen
  first retires it. A Recently viewed row of the shown pull request lands on
  Overview without a read. Qt adds `present_link_location_notice` (beside the
  bar and in the status bar) and `reveal_conversation`, which widens Discuss's
  scope and filters only when they hide the linked conversation.

Checks ran on the working tree committed unchanged as `17e40dc8`:

- `core_setup_destination` and `application_setup_destination` pass
  (dev and core-only presets): the parse table (canonical, files, commits,
  commit pages with fragments, file digests with and without either line,
  the three comment fragments, nine unread fragments, five malformed lines,
  dropped query parameters, five non-PR pages) and the resolution cases
  (exact digest, absent digest, shared digest, right line with and without a
  head comparison, left line, review and comment present and absent, issue
  comment, commit page, unrecognized, invalid typed input, every refusal
  explained).
- `application_setup_destination_public_path` passes (52.76 s), now with
  `run_link_location_course` over the composed two-file pull request: a
  `/files#diff-<secondary.cpp>R2` link opens and selects that file's right
  line 2 in Read with no notice; a second link to the same pull request moves
  to `review.cpp` with `AccountReadCount` unchanged; `#pullrequestreview-4001`
  reveals `review:PRR_fixture_1` in Discuss with no read; `#discussion_r123456`
  (the authenticated read names comments by node identity), an `L2` line, a
  commit page, an absent digest and an `#issuecomment-` link each open or keep
  the pull request and say why, naming the requested location, with no read;
  a repository commit link opens nothing. The GHES course's
  `/files#diff-abcd` is explained the same way.
- `application_profile_window_review_restoration_public_path` passes every
  phase (591.97 s); its `anonymous-restore` phase now also shows that a
  Recently viewed row of the pull request the window shows returns to it on
  Overview from Read with no account or preview read.
- Negative controls, each restored afterwards: refusing every digest fails
  "a file link with a head-side line selects exactly that file and line"
  (`build/bpr0987-negative-file.log`); never reusing the shown pull request
  fails "a link to the pull request the window shows reads nothing again"
  (`build/bpr0987-negative-reuse.log`); removing the Recently viewed shortcut
  fails "a row of the shown pull request returns to it on Overview"
  (`build/bpr0987-negative-recent.log`).
- `application_anonymous_public_preview_public_path`,
  `application_queue_composition_contract`, `qt_review_landing_model`,
  `qt_home_list_scale_100`, `core_window_review_restoration`,
  `application_window_review_restoration`, `core_boundary_contract`,
  `core_dependency_boundary` and `architecture_boundary_contract` pass.
  `architecture_boundaries` passes on a clean export of the working tree
  (`build/bpr0987-architecture-boundaries.log`); in the checkout it stops on
  the stray `.claude/worktrees/agent-a17cae88115dbbb4e`, as before.
- `qt_composed_github_scale_100` stops at [[BPR-0940]]'s server-card
  assertion and `qt_anonymous_public_preview_scale_100` at its known
  Connect assertion, both before any course this card touches, as at HEAD.
  `application_composition_contract` fails only on [[BPR-0983]]'s three
  recorded assertions once the digest pin is resealed.
- `clang-format` check passes on all fourteen touched C++ files; strict tidy
  passes on the touched small translation units and reports nothing in the
  changed lines of `workbench_window.cpp`, `github_application.cpp` and the
  composed course (its findings there are pre-existing lines).
  `verify_documentation.py` is `DOCUMENTATION_OK`; `git diff --check` clean.

Composition digest reseal review (the `BPR-0987 reseal` note in
`application_review_round_trip_composition_contract.cmake`): the
`github_application.cpp` changes are the setup port's `open` (same authority
conditions as before, now returning whether it read), the pending link
location and its four scheduling hooks, `retire_setup_effects` retiring it,
and the Recently viewed shortcut, which requires the accepted session's
profile, mode and hostname to match the row and the window's accepted
snapshot to be that pull request. No credential, vault, provider, transport
or mutation path changed; the mutation-branch pin is unchanged. The
production manifest digest moves because `github_application.cpp` is in it.

Correction, 2026-09-23 (found while verifying [[BPR-0989]]): the pins above
were taken before this card's last `github_application.cpp` edit, so the
committed file never matched them and `application_review_round_trip_composition_contract`
failed from `17e40dc8` onward; the "passes after the reviewed reseal" evidence
was run before that edit. The edit adds a null guard on the accepted host in
`open`, explains a review link whose review was not read as `review_not_read`
rather than `comment_not_identified`, and places a file location only once the
epoch host holds the shown pull request's authority. Reviewed as the same link
location code: no credential, vault, provider, transport or mutation path. The
`BPR-0987 correction` note reseals both pins to the committed file
(`07c49397…`, manifest `9ad3bbe3…`); the mutation-branch pin is unchanged and
the contract passes again.

Limitations: comment links resolve only when the read carries the comment's
REST identity, which today is the signed-out read; an authenticated read's
`#discussion_r` links are explained, not revealed. `#issuecomment-` links are
always explained. Base-side lines are never placed. No attended check was
run or is claimed.
