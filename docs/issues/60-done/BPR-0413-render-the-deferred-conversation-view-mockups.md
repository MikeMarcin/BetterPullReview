---
id: BPR-0413
title: Render the deferred conversation-view mockups the owner asked for
status: Done
rfc: RFC-BPR-0005
depends_on: [BPR-0191]
---

# Render the deferred conversation-view mockups the owner asked for

[[BPR-0211]] and [[BPR-0212]] both record the same operator direction, verbatim,
on 2026-08-20:

> I would like to see mockups before deciding.

Both cards then draw the same conclusion in the same words — "the disposition
changes from *waiting on a decision* to *waiting on a deliverable that this
session owes*" — and both then left `parked_on: awaiting-product-decision`
unchanged and stayed in `40-parked/`.

That is the drift the board README warns about in the paragraph directly above
the frontier table: a card whose *next* action is unattended does not belong in
`40-parked/`, which is how [[BPR-0256]] came back to an active lane. Nothing was
waiting on the owner. A deliverable was owed and nobody was building it.

This card owns the deliverable. It is unattended work, executable from a macOS
session, and it is what the two parked cards are actually waiting on.

## The board move this forces

[[BPR-0211]] and [[BPR-0212]] now depend on this card, so they move to
`10-backlog/` and drop `parked_on`. That is not bookkeeping preference: it is
forced. `scripts/verify_documentation.py` fails a card outside
`10-backlog/`/`50-blocked/` that depends on a card outside `60-done/`, because
`40-parked/` is a frontier-root lane and a parked card with an unmet dependency
is a contradiction. They return to `40-parked/` when this card is done and the
only thing left is the owner reading the page.

## What the mockups must be faithful to

The cards require a design "this codebase could actually reach, not an
illustration of a design in general". Every fact below was read from live source
for this card, and the mockups are built from these and nothing else.

**The projection.** `github_conversation_projection`
(`src/application/include/bpr/application/github_projection.hpp:124-139`) carries
`Id`, `ParentId`, `Kind`, `AuthorLogin`, `Body`, `Path`, `Line`, `ReviewState`,
`Resolved`, `Outdated`, `Deleted`. [[BPR-0212]]'s central claim is confirmed
exactly: there is **no revision identity of any kind** on this record — no commit
OID, no epoch, no ordinal.

**The tree is two levels deep, by construction.**
`src/application/src/github_projection_mapping.cpp` sets `ParentId` in four
places. Three are `std::nullopt` (lines 148, 162, 192) and the fourth, line 175,
is `.ParentId = threadId` for every comment under a thread. So the "forest of
trees with a single optional parent" is real as a type and flat as populated.

**The reply edge exists one layer up and is discarded at projection time.**
`github_read_model.hpp:233` carries `std::optional<github_comment_id> ReplyTo`
per comment; the read contract requests and validates `replyTo`
(`src/adapters/github/src/github_read_contract.cpp:35,563,1259-1279`). At
`github_projection_mapping.cpp:175` that edge is in scope on `comment` and the
thread root is written instead.

**And the Qt model would reject the deeper parent anyway.**
`github_conversation_model.cpp:317-326` sets a parent only when that parent's
`Kind` is `review_thread`. Carrying `ReplyTo` through the mapping without
touching this filter promotes every reply to a root and flattens the drawer to
one level. The two facts together are what make the alternatives unequal; the
first one alone, which is all this card originally had, made them look further
apart than they are.

**What the drawer renders today.** `github_conversation_delegate.cpp:68-144`
paints three stacked runs per row: a heading (`Review thread`,
`Reply by <author>`, `Review comment by <author>`,
`Pull request comment by <author>`), the body, and a facts line joining
`Path:Line`, review state, `Open`/`Resolved`, `Outdated`, and `Deleted` with
`"  ·  "`. `github_conversation_panel::set_anchor_file` filters the forest to the
current file, keeping whole threads or none. There are no lane rails and no
git-graph rendering; the card text that promised them was narrowed by the owner
in [[BPR-0191]] to mean filtering by thread.

**The epoch axis.** `epoch_history_model.cpp:132-137` labels every anchor
`Epoch %1 · %2 — %3 — %4` — ordinal, `Base`/`Head`, primary cause
(`Author upload`, `Force-push/rebase`, `Base update`, …), availability
(`Available`, `Pruned`, `Cache deleted`, …). `epoch_comparison_control` supplies
two independent selectors plus free-OID editors and the named-comparison
actions. The mockup uses these exact strings rather than inventing labels.

**The palette.** `appearance_profile.cpp:76-103`, BetterPullReview Light:
`application_surface #F6F7F9`, `application_text #1F2328`, `code_background
#FFFFFF`, `code_gutter_background #F0F2F5`, `code_line_number_text #5B6570`,
`diff_added_background #E6FFEC`, `diff_removed_background #FFEBE9`,
`diff_group_boundary_rule #8C8C8C`, `state_unread #0969DA`, `state_accepted
#1A7F37`, `state_defect #CF222E`, `state_stale #5B6570`. No colour outside the
46-role vocabulary appears in a mockup.

## The honest asymmetry the owner should see

This is the finding that makes the question answerable, and it was not known
when the cards were written.

| Alternative | What it needs | Cost |
|---|---|---|
| A — whole-review conversation | nothing; the same projection, unfiltered and grouped by file | a view |
| B — Treeverse-style deep thread | the mapping to carry `ReplyTo`, **and** the Qt model to stop rejecting a non-thread parent, plus a null-`ReplyTo` fallback and the tests that encode today's shape | a view plus a small cross-layer change |
| [[BPR-0212]] — conversation × revision | a per-comment revision the read path does not request; a review's commit is already required and then dropped | a model and fetch question, correctly RFC-scale |

The B row is the corrected one. Its first version said "one line", which an
independent review falsified; see the reopening record below.

## What no mockup can honestly show

No recorded conversation in this repository is deep or wide enough to exercise
alternative B. Verified for this card: `directxmath-336/recording.json` has
top-level keys `commits, files, pull_request, recorded_from, recorded_on,
repository` and no conversation at all, which is what [[BPR-0211]] already
suspected; `uiforetw-74` is the same shape. The only conversation content in the
tree is the hand-written GraphQL contract fixture
`tests/fixtures/github/contracts/v1/github-com/responses/review_threads.json` —
three threads of one, two, and one comments, with exactly one `replyTo` edge, so
maximum observed depth is one reply.

So the deep-thread mockup is drawn against a **synthesized** conversation, and
the page says so in the subject text rather than in a footnote. That is not a
defect of the mockup: it restates [[BPR-0211]]'s own precondition — "a
conversation deep or wide enough to be a problem is recorded" — which is still
unmet, and the owner should decide knowing it.

## Required final state

- Four rendered mockups: the shipped drawer as it is today, alternative A,
  alternative B, and the [[BPR-0212]] revision join.
- Checked-in SVG sources plus a deterministic stdlib-only generator, so the
  mockups are regenerable rather than one-off pictures.
- One offline attended review page carrying three decision subjects: A and B as
  a choose-exactly-one selection group where rejecting both is the recordable
  "build neither" answer, and the [[BPR-0212]] join as an independent question.
- Every downstream claim a careless reader might infer declared false in the
  batch: no implementation exists, no RFC is closed, no card is closed.
- [[BPR-0211]] and [[BPR-0212]] back in `40-parked/` with the page referenced.

## Non-goals

This card writes no product code, changes no projection, and adds no widget. The
one-line `ParentId` change it describes is **not** made here — describing what an
alternative would cost is the deliverable; spending it before the owner chooses
is the mistake the two cards were deferred to avoid.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
python3 -m unittest discover -s .agents/skills/bpr-attended-review/scripts -p 'test_*.py'
python3 .agents/skills/bpr-attended-review/scripts/build_attended_review.py \
  build --input docs/verification/reviews/conversation-view-mockups-v001/offline-review-input.json \
  --out <package-dir>
git diff --check
```

## Built, then reopened — 2026-08-21

The page is built and committed at `01908c3`. It carries three decision
subjects under batch
`bpr:/attended-review/betterpullreview/40a5fd691f86739e55c6860a`:

| Subject | Shape | Question |
|---|---|---|
| `alternative-a-whole-review` | member of `secondary-view` | the whole review in one place |
| `alternative-b-treeverse` | member of `secondary-view` | a map of one deep thread |
| `revision-axis-rfc` | independent | is the revision join worth an RFC |

`secondary-view` is a `min_accept: 1, max_accept: 1` selection group, so the
two alternatives are one question and the loser is recorded as not chosen
rather than rejected. Rejecting both members is the recordable "build neither"
answer, which is the outcome [[BPR-0211]] deliberately left open. The
[[BPR-0212]] subject is deliberately outside the group: it is a different
decision, and folding it in would have forced a choice between three things
that are not alternatives to each other.

### What the work found

The mockups were the means; this was the result. The two alternatives are not
equally expensive, and nothing in either card said so:

- Alternative A needs no model change at all. Dropping the anchor filter and
  grouping by `Path` reads the projection exactly as it stands.
- Alternative B needs one line. `comment.ReplyTo` is requested and validated by
  the read contract, normalized into `github_read_model.hpp:233`, and is in
  scope at `github_projection_mapping.cpp:175` — where the thread root is
  written instead. The flat two-level tree is a projection decision, not a
  GitHub limitation.
- [[BPR-0212]] needs a datum that exists nowhere, which is what its own text
  already said and what this confirmed field by field.

The honest caveat that came with it: no recorded pull request in this
repository carries a conversation at all, so alternative B is drawn against a
synthesized thread. That is [[BPR-0211]]'s own unmet precondition, now measured
rather than suspected.

### Verification matrix

| claim | direct surface | command | what proves it | result | evidence |
|---|---|---|---|---|---|
| the batch is one choose-exactly-one question plus one independent decision | built manifest | `build_attended_review.py build --input offline-review-input.json --out offline-package` | the manifest carries `selection_groups` with `min_accept`/`max_accept` of 1 and exactly two members | pass | `01908c3`; batch `40a5fd69…5c6860a` |
| the page cannot be read as closing anything | built page | inspect the rendered scope banner | six downstream claims render false, including `implementation_exists` and both card-closed claims | pass | `offline-package/index.html` |
| the mockups regenerate from checked-in source | `render_mockups.py` | `python3 render_mockups.py --out mockups` | stdlib-only, no clock or randomness; SVG bytes reproduce | pass | `01908c3` |
| the projection carries no revision identity | `github_projection.hpp:124-139` | read the struct field by field | eleven fields, none of them a commit OID, epoch, or ordinal | pass | quoted on the card above |
| every comment's parent is the thread root | `github_projection_mapping.cpp` | `grep -n 'ParentId' src/application/src/github_projection_mapping.cpp` | four assignments; three `nullopt`, one `threadId` | pass | lines 148, 162, 175, 192 |
| the reply edge exists and is discarded | `github_read_model.hpp:233` | read the loop at mapping line 172-186 | `comment.ReplyTo` is in scope where `threadId` is written | pass | quoted on the card above |
| no recorded conversation exists to draw | recorded fixtures | `python3` load of both `recording.json` files | top-level keys carry no conversation; contract fixture peaks at one `replyTo` | pass | `directxmath-336`, `uiforetw-74` |
| the drawer prefixes every author with `@` | `github_conversation_delegate.cpp:44-45` | read `author_text` | it returns `QStringLiteral("@%1")`, so a mockup without the `@` misdraws every heading | pass, after correction | mockups regenerated at this commit |
| the Qt model rejects a non-thread parent | `github_conversation_model.cpp:317-326` | read the parent resolution | `parentIdentity` is set only when the parent's `Kind` is `review_thread` | pass | falsifies the earlier one-line cost |
| a review's commit is fetched and dropped | `github_read_contract.cpp:1214-1241`, `github_projection_mapping.cpp:145-156` | read the required-key set and the review node construction | `commit_id` is a required key; the projected node carries no revision | pass | corrects BPR-0212's concern text |
| the epoch label strings are real enumerator text | `epoch_history_model.cpp:26-137` | read `display_text` and its helpers | the format and every cause/availability word used in mockup 04 appear verbatim | pass | added after review found no row for it |
| the palette values match the Light profile | `appearance_profile.cpp:76-96` | compare the twelve constants in `render_mockups.py` | each hex equals the `srgb(0x…)` for its role | pass | added after review found no row for it |
| the conversation-fixture search was exhaustive | `tests/fixtures` | `find tests -name 'review_threads*.json'` | three fixtures, not one; depths 1/2/1, 1/2/1, and none | pass, after correction | corrects "the only conversation content in the tree" |
| the git-graph request was reinterpreted, not withdrawn | `BPR-0191:92-93,108-110,120-122` | read the three passages in order | the request is quoted; the narrowing is the card's own prose; the owner's quote is about file anchoring | pass | added subject `lane-rail-drawer` |
| the board gates still pass | `scripts/verify_documentation.py` | `python3 scripts/verify_documentation.py --root .` | lane, status, dependency, and frontier-table derivation | `DOCUMENTATION_OK` | every commit in this run |
| the review tooling is sound | attended-review tests | `python3 -m unittest discover -s .agents/skills/bpr-attended-review/scripts -p 'test_*.py'` | 17 cases including verdict validation | 17 tests, OK | `01908c3` |

### Bound artifacts

| path | bytes | digest |
|---|---|---|
| `render_mockups.py` | 25,590 | `sha256:225323dfc9bbb55a2bb463ec443ce5a1…` |
| `offline-review-input.json` | 14,159 | `sha256:2d1db035043031b03ad20ea80ddecc8b…` |
| `offline-package/attended-review-manifest.json` | 18,377 | `sha256:9c34a87d4ce84b881804568cbf0e6879…` |
| `offline-package/index.html` | 66,071 | `sha256:2af4d87199966a87e423638f27428eb8…` |

### Reopened by independent review — 2026-08-21

The page above was built, committed at `01908c3`, and then **not presented**. An
independent reviewer was asked to falsify what the owner would read, and it did.
Three findings were reproduced directly before any correction was made, and each
is a case of the card asserting a cost or a feasibility it had not measured.

**The "one line" claim is false, and it was this card's central argument.**
`comment.ReplyTo` really is fetched, validated, and overwritten at
`github_projection_mapping.cpp:175`. But `github_conversation_model.cpp:317-326`
accepts a parent only when that parent's `Kind` is `review_thread`. Change the
mapping alone and every reply's parent becomes a `review_comment`, which the
model rejects, so the node is promoted to a root: the drawer would go from two
levels to **one**, thread rows would lose every child, and
`github_conversation_delegate.cpp:81-83` would start printing "Review comment by"
where it prints "Reply by". The naive edit makes the product worse, not deeper.
The real change is the mapping, the model's parent-kind filter, a fallback for
the null `ReplyTo` that a thread's first comment carries, and the tests that
encode the current shape. The asymmetry between A and B is still real; it is
smaller than this card claimed, and it was claimed after reading one file.

**BPR-0212's feasibility concern is falsified by this repository.** The subject
told the owner that whether GitHub exposes the revision a comment was made
against "has not been checked" — "unauthenticated or at all". But
`github_read_model.hpp:223` carries `std::optional<commit_oid> Commit` per
review, `github_read_contract.cpp:1214-1241` **requires** `commit_id` on every
review object and rejects a response without it, and
`github_projection_mapping.cpp:145-156` then drops it — the same discard this
card made so much of for `ReplyTo`, and never noticed. When BPR writes a review
comment it sends `commit_id` itself
(`github_mutation_contract.cpp:117-122`). What is genuinely open is narrow: the
review-thread GraphQL selection asks for `originalLine` but not the comment's
commit, so the per-comment datum is not requested today. Presenting that as
"nobody has checked whether this is possible" understates a feature the owner is
being asked to fund.

**The git-graph drawer the owner asked for was settled in one clause.**
[[BPR-0191]]:92-93 records that the owner "asked for a threaded conversations
drawer drawn in a git-graph / DAGVIZ style, with filtering by lane". The
narrowing at :108-110 — "filtering by lane therefore means filtering by thread" —
is that card's own engineering argument, not an owner statement; the owner's
quoted words at :120-122 are about anchoring to the current file and say nothing
about rails. This card wrote that the language was "narrowed by the owner", which
the record does not support, and the baseline mockup told the owner the same
thing on its face. A request the owner made and never withdrew was being reported
back to them as already settled.

Smaller corrections the same review found: the delegate renders authors as
`@login` (`github_conversation_delegate.cpp:44-45`) and no mockup showed the `@`;
seven neutral chrome colours are not product palette roles, against this card's
claim that none appear; there are three `review_threads.json` fixtures, not one,
so "the only conversation content in the tree" is wrong even though the
maximum-depth statement it supports is right; and the Treeverse map legends a
per-node read state that `github_conversation_projection` does not carry.

The verification matrix below is also short: it proved the board gates, the
tooling, and byte-regeneration, and had no row for the palette, the label
format, the cost of alternative B, or the exhaustiveness of the fixture search.
All nine rows could pass with every one of those wrong — which is exactly what
happened. Rows for them are added with the correction.

### What this card does not claim

No product code changed. The one-line `ParentId` change is described and
costed, not made. No RFC status moved, and neither [[BPR-0211]] nor
[[BPR-0212]] is closed — they return to `40-parked/` because the owner is now
genuinely the only thing left, which is the first time that label has been
true for either of them.

## Corrected and re-closed — 2026-08-21

The batch was rebuilt with every reproduced finding addressed. It is now
`bpr:/attended-review/betterpullreview/c285f6948ed054de1342cfb1`
and carries **four** decision subjects:

| Subject | Shape | Question |
|---|---|---|
| `alternative-a-whole-review` | member of `secondary-view` | the whole review in one place |
| `alternative-b-treeverse` | member of `secondary-view` | a map of one deep thread |
| `revision-axis-rfc` | independent | is the revision join worth an RFC |
| `lane-rail-drawer` | independent | the git-graph drawer was never built — still wanted? |

The fourth is new, and it exists because the review found the page settling a
request the owner had made and never withdrawn. Mockup `05-lane-rails.svg`
draws it so it can be answered instead of inherited. It is deliberately outside
the selection group: it is the primary drawer, not a secondary view, so it is
compatible with either alternative and with neither.

What else changed: alternative B's cost is corrected and its flattening trap
stated on the subject; [[BPR-0212]]'s falsified concern is replaced with the
narrow question that is genuinely open, and its detail section now records that
a review's `commit_id` is required on read and dropped, and that the write path
already sends one; alternative A's concern no longer describes a decision
BPR-0191 made, because BPR-0191 re-homed the idea to [[BPR-0211]] rather than
refusing it; every author heading carries the `@` the delegate paints; the seven
neutral chrome colours are named as chrome rather than counted as palette roles;
the read/unread state the Treeverse map legends is disclosed as proposed; and
two new `downstream_claims` keys fence what the earlier fence missed —
`cost_estimates_verified` and `projection_field_authorized` are both false.

### Bound artifacts

| path | bytes | digest |
|---|---|---|
| `render_mockups.py` | 29,922 | `sha256:500ec73043a268540ff0885bc9d88886…` |
| `offline-review-input.json` | 22,211 | `sha256:54bfd97f8f6b536caecb9f8d06969b51…` |
| `offline-package/attended-review-manifest.json` | 26,503 | `sha256:879cc89e48fc8bc784b10d51f37e32b7…` |
| `offline-package/index.html` | 81,463 | `sha256:beb8c9a863c713b7239c1ed91385d4cd…` |

The first build's digests are superseded and are not carried forward: no verdict
was ever recorded against them, because the page was never presented.

## Answered — 2026-08-21

The owner returned a complete verdict the same day. It validates against its own
batch manifest, and the record is immutable at
`docs/verification/reviews/conversation-view-mockups-v001/verdict-validation.json`.

| Subject | Outcome |
|---|---|
| `alternative-b-treeverse` | **chosen** |
| `alternative-a-whole-review` | not chosen |
| `revision-axis-rfc` | **accepted** — the RFC is authorized |
| `lane-rail-drawer` | **rejected** — "Don't see the need at this time." |

Three things this card is worth remembering for.

**The selection group earned its keep.** The whole-review view is recorded as
*not chosen*, not rejected. It lost one question; it was not judged defective,
and if the Treeverse view is later withdrawn nothing has to distinguish a
runner-up from a failure. That distinction is exactly why `choice_group` and
`selection_group` exist.

**The subject the first build omitted is the one that produced a decision.**
`lane-rail-drawer` existed only because an independent review noticed the page
was settling a request the owner had made in [[BPR-0191]] and never withdrawn.
The answer was "Don't see the need at this time" — so the request is now
genuinely closed, by the person who made it, instead of by a card's reasoning.
Had the review not caught it, the page would have quietly carried BPR-0191's
narrowing forward as though the owner had agreed to it.

**The correction mattered to the answer.** The owner chose the alternative whose
cost this card originally understated by an order of magnitude. They chose it
against the corrected description, which named the flattening trap and the
cross-layer change. A verdict given against the first build would have been a
verdict against a claim that was false.

The verdict also produced work this card did not anticipate: [[BPR-0414]] for
the anchoring gap the owner named, and [[BPR-0415]] to author the RFC the
revision-axis acceptance authorizes.
