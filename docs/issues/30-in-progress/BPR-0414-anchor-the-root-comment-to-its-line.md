---
id: BPR-0414
title: Anchor the root comment to the line of code it comments on
status: In Progress
rfc: RFC-BPR-0005
depends_on: [BPR-0191]
---

# Anchor the root comment to the line of code it comments on

Owner direction, recorded on the [[BPR-0413]] verdict, 2026-08-21:

> The existing drawer today needs to anchor the root comment to the line of code
> it's commenting on like code collaborator does.

This is about the drawer that ships, not about the secondary view the same
verdict chose. [[BPR-0211]] owns that one.

## What ships today

Verified from live source for this card, because the gap is smaller than "no
anchoring exists" and larger than "it just needs a signal".

- The drawer filters threads to the current **file** and stops there.
  `github_conversation_panel::set_anchor_file` keeps whole threads or none, and
  `workbench_window.cpp:1815-1826` sets that anchor when it publishes a diff.
- The line is **inert text**. `github_conversation_delegate.cpp:99,115-142`
  renders `Path:Line` into the facts line. Nothing positions anything.
- Selecting a thread drives exactly one thing: `workbench_window.cpp:695-699`
  wires `currentChanged` to `update_defect_surface()`, which enables the
  defect-classification action. It never touches the panes.
- `diff_workbench::select_line` exists (`diff_workbench.cpp:1035-1053`) and is
  the mechanism that would scroll and select. Its only caller is
  `diff_workbench.cpp:894`, reacting to in-pane interaction. **No conversation
  path calls it.**
- Nothing runs the other way either: selecting a line does not highlight or
  filter the drawer.

So [[BPR-0191]] delivered file-grained filtering and a text label against a
request for line-anchored conversation, and said so plainly at the time. What is
missing was never built rather than broken.

## The geometry already exists

This is mostly wiring, not new infrastructure.
`measured_alignment_axis::left_line_origin` / `right_line_origin`
(`measured_alignment_axis.hpp:133-136`) map a line index to document Y, and
`diff_workbench.cpp:196-197` already calls that exact pair; document Y converts
to viewport Y at `diff_workbench.cpp:1018-1020`. An anchor marker needs those
functions called with a comment's line instead of a pane's own.

## Two data gaps that make a naive anchor wrong

Both were found while scoping this and neither is mentioned on [[BPR-0191]] or
[[BPR-0212]].

**There is no `Side`.** `github_conversation_projection`
(`github_projection.hpp:124-139`) carries `Path` and `Line` and nothing that
says whether the line is on the base or the head. With two panes, an anchor has
no way to know which axis to query. A comment on a deleted line is base-side
only, and today's data cannot express that.

**`Line` silently means two different things.**
`github_read_normalizer.cpp:156-165` computes
`selected = aPreferred.isNull() ? aFallback : aPreferred` and is called as
`optional_line(thread["line"], thread["originalLine"])`. GitHub nulls `line`
once a thread goes outdated, so an outdated thread's `Line` is an
**`originalLine` in a revision the pane may not be showing**, wearing the same
field name as a live thread's current-head line. `Outdated` records that this
happened; nothing records which revision would make the number meaningful. That
is [[BPR-0212]]'s axis, and this card must not pretend to fix it.

## The RFC constrains the answer, and may contradict the request

`RFC-BPR-0005:58` is explicit:

> Comments remain in the right drawer and synchronize with line anchors; they
> never become inline full-width rows in the code view.

The same RFC's required interactions already promise, at `:65`, that "selecting
a connector, hunk, line, **or conversation** synchronizes the top panes and
lower detail pane" — so the synchronization half of this card is an existing
unimplemented RFC requirement, not new scope.

The tension is the other half. SmartBear Collaborator does put the comment
beside the code. If what the owner wants is the comment body rendered against
the line *in the code view*, that is the shape the RFC forecloses, and the RFC
would have to change rather than be quietly worked around. **Do not resolve that
by choosing a reading.** Build the part that is unambiguous and RFC-compatible,
and put the remainder back to the owner as a bounded question with something to
look at.

## Required final state

- Selecting a thread in the drawer selects and scrolls to its anchored line in
  the correct pane, through the existing `select_line` path.
- The anchored line is visibly marked while its thread is selected, in the
  gutter or margin — not as a full-width inline row.
- Selecting a line surfaces the threads anchored to it, rather than leaving the
  drawer showing every thread in the file.
- A thread whose anchor cannot be placed — outdated, or base-side with no
  `Side` to disambiguate — is visibly *not* placed, and says why. Guessing a row
  is worse than declining to draw one.
- The `Side` gap is either closed or explicitly recorded as the reason a class
  of comments cannot be anchored.
- Whether the comment body belongs in the code view goes back to the owner with
  a rendered mockup, not decided here.

## Verification

Focused selectors for the touched surfaces, plus `clang-format` on the files
changed. A rendered check is required: this is a visual claim, and a passing
signal test would not show that the marker lands on the right row.

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'workbench_window|github_conversation'
```

## Implemented unattended slice — 2026-08-24

The ambiguous coordinate is no longer ambiguous. GitHub's thread-level
`diffSide` is requested as a non-null `DiffSide`, contract-checked as exactly
`LEFT` or `RIGHT`, normalized independently from `line` and `originalLine`, and
mapped into a provider-free application enum. A thread distinguishes its
current line from its historical line and carries an explicit current,
outdated-revision, or file-level placement state. Replies inherit that complete
root anchor; no reply invents its own side or revision, and GitHub may omit
both of a reply's own nullable line fields. Present reply coordinates remain
validated, but root placement is authoritative. Missing or null `diffSide`,
and a current `LINE` root without `line`, are malformed provider payloads
rejected atomically before projection; only an outdated root may survive on
`originalLine` alone. Qt retains defensive wording for synthetic incomplete
projections and converts any unknown projected side to visibly unplaced
`missing_side`; it never guesses the right pane. The composed provider path
cannot produce those states.

This matches GitHub's documented
[`PullRequestReviewThread`](https://docs.github.com/en/enterprise-cloud@latest/graphql/reference/pulls#pullrequestreviewthread)
contract: `diffSide` is non-null `DiffSide`, whose values are `LEFT` and
`RIGHT`. The capability proof also requires non-null
`PullRequest.headRefOid`. Every GraphQL page selects that head identity and the
reader rejects a REST/GraphQL head race before accepting a snapshot. The
fixture manifests bind the revised query and responses for GitHub.com, GHES
3.18, and the recorded UIforETW contract.

The two Qt directions now share one semantic route:

- selecting a current root or reply resolves the root, converts its one-based
  provider line to the diff's zero-based source line, calls the same
  `diff_workbench::select_line` transition used by an in-pane gesture, and
  reveals that exact line through the retained shared-scroll controller. A
  repeated physical click on an already selected line still reaches the Qt
  observer without duplicating the application selection sink, and a click on
  the exact conversation-anchored line retains the selected root and marker;
- selecting a source line publishes an interactive selection observation back
  to the drawer, which reverse-filters the retained forest by exact file, side,
  and line while keeping replies under their root;
- group, connector, navigation, and file-level selections clear the line
  filter, selected conversation, and marker together, and the initial line
  selected automatically when a diff is published is not misreported as a
  reviewer gesture;
- a four-pixel pane-margin marker distinguishes a conversation anchor from the
  ordinary selected-line wash. Clearing or replacing the comparison clears the
  marker with the rest of its exact request context.

The production path places a current `RIGHT` anchor only when the selected
comparison's right commit is the exact accepted GitHub head. GitHub's current
pull-request diff uses the documented
[three-dot merge-base-to-head comparison](https://docs.github.com/en/pull-requests/reference/branches#three-dot-and-two-dot-git-diff-comparisons),
while BetterPullReview currently compares the accepted base tip. We therefore
infer that the numeric `LEFT` coordinate cannot safely address our left blob;
the provider snapshot carries no merge-base identity to prove otherwise.
`LEFT` fails closed in the composed product even though the Qt adapter has a
direct exact-endpoint contract for a future caller. Outdated, file-level,
selected-endpoint-mismatch, and selected-line-out-of-bounds roots remain visible
at file scope and say why no marker can be placed. `OriginalLine` is historical
information and is never
reused as a current coordinate. This closes the current-head data gap without
taking [[BPR-0212]]'s revision-axis scope.

The public-path course found one test-only composition defect while this was
being connected. `request_fixture_diff` fabricated a file identity unrelated
to `core::file_identity_of("review.cpp")`; file synchronization therefore did
the correct thing and filtered every conversation out. The shortcut now uses
the same canonical file identity as the production selected-file path, and the
course remains a faithful test of the product boundary.

Hardening the defect action exposed a second historical mismatch. BPR-0230
classified GitHub's one-based line directly even though the durable core anchor
is zero-based, and its old course treated an outdated original line as a
current coordinate. The ledger now upgrades those legacy conversation-backed
anchors once and marks all of them outdated: SQLite alone cannot prove whether
the old forced-right coordinate came from a current right thread, a left thread,
an outdated original line, or an unrelated retained comparison. Availability
is restored only when the accepted thread and rendered comparison re-prove the
exact current-head file, side, line, comparison digest, and line bound. The
composed course classifies two current `RIGHT` threads, reacquires each current
Qt index by stable identity, and proves a reversed comparison can neither place
nor classify a third defect. It also refuses the corresponding ledger writes
and proves the live defect projection still adopts `outdated`; losing exact
placement proof fails closed in memory rather than depending on persistence
success.

## Rendered evidence and the bounded owner question

The composed fixture course now selects `PRRT_fixture_1`, asserts right-side
zero-based line 41, asserts that exactly that one root remains in the drawer,
asserts a non-empty margin-marker rectangle, and only then saves the rendered
window. It emits `CONVERSATION_ANCHOR_EVIDENCE_OK`; the registered
`application_diff_workbench_public_path` test requires that marker in its one
contiguous status contract.

The final capture and offline page live at
`docs/verification/reviews/conversation-code-boundary-v001/`. They ask exactly
the question this card deferred: accept keeps comment prose in the drawer with
the margin marker and selected-line band as the code indication; reject says
that boundary is insufficient and requires explicit successor/RFC scope before
prose enters the source view. The package must be regenerated from the final
implementation commit, bind the registered network-isolated CTest course and
capture provenance, and leave every downstream claim false until a returned
export is validated.

## Verification record

| Surface | Result |
|---|---|
| full `dev` build after formatting | passed |
| full `dev` CTest matrix | 197/197 passed in 1,560.45 seconds |
| `core-only` CTest matrix | 93/93 passed |
| GitHub capability, read-contract, and pagination suites | 3/3 passed |
| application GitHub projection host | passed |
| conversation panel | passed |
| diff workbench at 1.0x, 1.25x, 2.0x | 3/3 passed |
| composed workbench window at 1.0x, 1.25x, 2.0x | 3/3 passed |
| real `application_diff_workbench_public_path` | passed, including `CONVERSATION_ANCHOR_EVIDENCE_OK` and `RENDER_OK` |
| review-ledger and review-round-trip composition | 2/2 passed |
| defect composition, including reversed-comparison refusal | passed |
| legacy review-ledger anchor upgrade | passed |
| GitHub fixture security and RFC-0016 capture/runner suites | passed |
| current documentation and immutable RFC-base gates | passed |
| repository formatting and touched-source `clang-tidy` | passed |
| capability-critical independent review | clean; no implementation-level P0-P2 finding |

The whole-tree `coding-tidy` target also exposed one pre-existing diagnostic in
untouched `src/app/src/designated_live_macos_capsule_identity.cpp:168`
(`performance-inefficient-string-concatenation`), unchanged since BPR-0354.
That unrelated baseline finding is not folded into this card; every touched
translation unit passes warnings-as-errors analysis.

## Parked for the owner verdict — 2026-08-24

The registered network-isolated capture course passed again after the
implementation commit, and the attended page was regenerated against exact
commit `8c7504dd80b28308d29e4d37254004ba3708821b`. Capture provenance, the source
review input, every copied subject artifact, and the exported-decision contract
are digest-bound in batch
`bpr:/attended-review/betterpullreview/ab6145717475dfdfddd6206d`.

Open
`docs/verification/reviews/conversation-code-boundary-v001/offline-package/index.html`,
inspect the composed screenshot at native size, choose `Accept` or `Reject`, and
export the complete JSON verdict. Return that file without editing it. Validate
the returned export before recording any decision:

```sh
python3 .agents/skills/bpr-attended-review/scripts/build_attended_review.py \
  validate-verdict \
  --manifest docs/verification/reviews/conversation-code-boundary-v001/offline-package/attended-review-manifest.json \
  --verdict /path/to/exported-verdict.json
```

Until that exact batch validates, this card is parked, no owner verdict is
recorded, no inline-comment-body direction is authorized, and RFC-BPR-0005 is
not closed.
