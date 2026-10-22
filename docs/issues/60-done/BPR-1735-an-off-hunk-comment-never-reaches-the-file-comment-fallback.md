---
schema: "implement-rfc.card.v1"
id: "BPR-1735"
title: "An off-hunk comment never reaches the file-comment fallback outside the designated PR"
status: "Done"
rfcs: ["RFC-BPR-0022"]
depends_on: []
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1735--BPR-1735--b13813e1d6a2.json","kind":"bug","origin":"Owner report 2026-10-22, after BPR-1715's build showed a review refused on GitHub Enterprise Server 3.20.8 because a comment sits outside every hunk","phase":"built under the owner's decision of 2026-10-22","related_cards":["BPR-1715","BPR-1716","BPR-1706","BPR-1728","BPR-1732","BPR-1345"],"related_rfcs":["RFC-BPR-0008","RFC-BPR-0089","RFC-BPR-0090"],"frontend_design":"frontend-design:frontend-design loaded before the held mark's words and layout; it reuses BPR-1728's hold","follow_up":"BPR-1737"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"ba7ed6b095ecc4893bcb90ebf5cf0c656da469d8","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_filtered_comment|application_review_mutation_application_host|qt_workbench_line_draft)$'; python3 tests/end_to_end/bpr1735_whole_pr_live_course.py --build build/dev --work DIR --prefix bpr1735-live --cleanup","detail":"core::place_on_github_diff: the changed line and lines exactly three before and after it are in on either side, four are out, BPR-Test #124's line 60 is out, a range must lie in one hunk, changes six lines apart share a hunk and seven apart leave the middle line out, an insertion's context on both sides, unknown with no diff read or an invalid one. place_whole_diff_comment names the head (right) or merge base (left) for the file comment and refuses a range; place_filtered_comment applies the rule to a filtered diff's own sides. The mutation host places from the source's WholeDiff before any effect (line 13 of #124's shape native, 14 a file comment, a range leaving the hunk refused, a base line only with merge-base numbering, another head stale_head, unread lines native). The editor says before Publish 'GitHub shows no change near this line at the head, so GitHub can't attach a comment to it. Publish adds a comment on the file instead, linking to line 7 of commit 61e8dbc.' Live (#127, github.com): the shipped composition offscreen placed head lines 60 and 14 as file comments at the head and line 13 on itself, with that sentence. Nothing reads the host's name or version. Failing before: the functions and the WholeDiff fact do not exist on main, and core_filtered_comment's own-side cases fail there (a right side that is the head was placed at the head unconditionally)."}},{"criterion":"AC2","outcome":"pass","revision":"ba7ed6b095ecc4893bcb90ebf5cf0c656da469d8","evidence":{"kind":"command","command":"ctest --preset dev -R '^(github_review_comment_permalink|application_review_submission_composition_public_path|qt_review_submission_editor_scale_(100|125|200)|qt_workbench_line_draft)$'; python3 tests/end_to_end/bpr1735_whole_pr_live_course.py --build build/dev --work DIR --prefix bpr1735-live --cleanup","detail":"Single Publish: test_an_off_hunk_whole_pr_line_is_a_file_comment_at_once sends one file POST (subject_type file, commit_id the head, no line or side, body the permalink to line 13 at the head, or at the comparison's left commit for a base line, a blank line and the words), journal unresolvable_line_admitted then succeeded_fallback, no native attempt; a replay and a restart send nothing. Review: the composed owner course holds the off-hunk comment, Publish as file comment publishes it once as a file comment (FilePosts +1, NativePosts unchanged, body 'https://github.com/octo-org/review-repository/blob/<head>/review.cpp#L86' + blank line + words) and takes it out of the review (8 carried), Submit by key sends nothing while held, then one review POST carries the eight and GitHub holds each once and not the file comment's text. Live #127: the review carrying line 60 is refused whole (422 'Line could not be resolved', no review left); line 60 alone as the composition's file comment is 201 subject_type file at the head; the rest (line 13) is one review, 200, one comment RIGHT 13; line 14 natively is 422 pull_request_review_thread.line 'could not be resolved' and as a file comment 201. Three comments in all. Failing before: no hold and no file comment on Whole PR exist on main (the owner course waits on held_comments)."}},{"criterion":"AC3","outcome":"pass","revision":"ba7ed6b095ecc4893bcb90ebf5cf0c656da469d8","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_review_round_trip|sqlite_review_mutation_recovery_storage|sqlite_review_submission_recovery_storage|github_review_comment_permalink)$'","detail":"A native single comment GitHub refuses with a fully received 4xx, its complete comment list holding none of it, is rejected and reported not published: unplaceable_line for GitHub's line words (recorded PR2-shaped 422), refused_not_published otherwise, through the GitHub CLI's answer and the native transport's error alike; Publish again sends one new native attempt, and a draft refused natively then placed off the diff is published once as the file comment after a restart. A lost answer or a 502 is not rejected, and a day later still only reconciles. Schema 8 keeps each child POST's answer; a version 7 journal migrates every logical publication as kept before answers, at the migration time; such a stuck comment settles (settled_not_published) only once ten minutes have passed and the list holds none of it, and a listed comment confirms it instead. The journal refuses a prepared child's rejection and a receipt on a rejected child, and restarts only under the same key. review_publication_outcome::rejected unlocks an unknown draft. Failing before: on main core plans a rejected record as previously_rejected and the journal refuses the state."}},{"criterion":"AC4","outcome":"pass","revision":"ba7ed6b095ecc4893bcb90ebf5cf0c656da469d8","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_review_submission_editor_scale_(100|125|200)|qt_workbench_line_draft|application_review_submission_composition_public_path)$'","detail":"After GitHub refuses the owner's review (the GitHub CLI's 422 naming an unplaceable line), this build marks exactly the comment the core places outside the diff: its row on Finish your review says 'GitHub shows no change near line 86, so it can't place this comment in a review.' in the reading amber with Publish as file comment beside Delete, and on the comment itself Publish as file comment takes the sending buttons' place. Every verdict waits with '1 comment GitHub can't place in a review is held. Publish it as a file comment or delete it first.' A held range offers no action."}},{"criterion":"AC5: the owner's stuck review","outcome":"pass","revision":"ba7ed6b095ecc4893bcb90ebf5cf0c656da469d8","evidence":{"kind":"command","command":"ctest --preset dev -R '^application_review_submission_composition_public_path$'","detail":"Nine comments (eight in hunks at head lines 46-51, 58, 59; one at head line 86 outside every hunk) written by a build that placed nothing on the Whole PR diff (set_whole_diff_placement_enabled false), refused in the GitHub CLI's 422 shape and settled by BPR-1715 with all nine kept; restarted on this build, the off-hunk comment alone is held, Submit is blocked, Publish as file comment publishes it once, and Submit sends the other eight as one review."}},{"criterion":"AC6: gates","outcome":"pass","revision":"ba7ed6b095ecc4893bcb90ebf5cf0c656da469d8","evidence":{"kind":"command","command":"_coding-standard/scripts/format.py; _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <25 changed units>; cmake --preset core-only && ctest --preset core-only; reseal_round_trip_composition.py --check; reseal_command_correctness.py --check; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"clang-format 22 applied to every changed C++ file; clang-tidy (LLVM 22) with warnings as errors passes on all 25 changed units. Focused and integration selections pass offscreen on macOS with the dev preset -O0 -g0: 143 core/sqlite/github/live/host rows, 37 Qt and transport rows, 89 application review/comment/draft/publication/tour/label/submission/round-trip/commit-filter rows after the course fixes below, and the seed users. core-only: 311 of 314 pass; application_diff_host failed once under load and passes alone, command_correctness_registry_contract passes after the reseal, documentation_structure refuses only BPR-1737's provisional ID. The full-application and manifest pins are resealed with a note; mutation 16d135419 stays exact."}}]
---

# BPR-1735: An off-hunk comment never reaches the file-comment fallback outside the designated PR

## Owner report

The owner, 2026-10-22, verbatim:

> Okay the new build clearly showed the failure to publish the review was
> caused by comments on lines outside of the PR hunks on the GHE server.
> However we were supposed to have created a fallback for those cases to
> allow them to publish anyways. So you're going to have to do a
> investigation into why that would fail to work as designed.

The host is GitHub Enterprise Server 3.20.8. [[BPR-1715]]'s build settles the
refused review and says one comment is on a line GitHub can't place.

## Answer

The fallback was never live for this case. RFC-BPR-0022's file-comment
fallback was designed and then deliberately left switched off in production,
and it only ever applied to a single comment's Publish, never to a review.
[[BPR-1706]]'s fallback is live, but only on a commit-filtered diff. Nothing
is broken against its own specification. The specification just doesn't cover
this case, and the single-comment path has a second defect besides.

## Each publish path, traced (at `20ee6d72a`)

### Single-comment Publish

`review_mutation_session::submit_publication` (`github_mutation_contract.cpp`)
holds RFC-BPR-0022's state machine. On an unfiltered diff the fallback cannot
fire, for three reasons, any one of which is enough:

1. **Placement is always `unknown` in production.**
   `observe_review_source` (`github_application.cpp`, about line 12713) sets
   `.Placement = core::review_placement::unknown` with the comment
   "Full-file alignment is not a provider patch-domain observation"
   (`e96a62f48`, 2026-09-26). With `unknown`, `plan_review_publication`
   always chooses `native_attempt`, so no first-attempt fallback is possible.
2. **The response contract exists only for BPR-Test PR2.**
   After the native 422, the fallback needs `definitive_off_hunk_unsupported`
   to match an `off_hunk_unsupported_contract`. The composition passes none:
   only `github_public_path_fixture.cpp` and the tests construct one. The
   session's default is `recorded_off_hunk_contract`, which returns
   `std::nullopt` unless the target is exactly MikeMarcin/BPR-Test PR2 at
   base `a939a85a`/head `61e8dbcd`, actor MikeMarcin (7605339) and profile 2.
   `recorded_anchor_matches` also pins the two fixture blobs and the 80-line
   file (`dde294fb2`, 2026-09-26, "Bind RFC0022 recorded fallback contract to
   approved PR2 scope"). On any other host the contract is absent.
3. **Even a recorded match would refuse a GHES answer.** The recorded
   classifier requires the GitHub.com `documentation_url`, exactly five
   response headers including `X-RateLimit-Resource: core`, and
   `error_on_unknown_keys`. GHES writes an `enterprise-server@…`
   documentation URL and often sends no rate-limit headers. The GitHub CLI
   transport also passes every header (up to 128), so the five-header check
   fails there too. The generic, unrecorded shape (`unsupported_response_json`)
   accepts only `message` and `errors`, so it refuses any real GitHub body,
   which also carries `documentation_url` and `status`. It is fixture-only.

**Second defect: the comment gets stuck.** With no contract, the native 422
is parsed as `invalid_status`. `map_contract_failure` turns that into
`unavailable`, and because `MutationAttempted` is true the window shows the
outcome as indeterminate. The journal child stays `native_possibly_transmitted`.
No production path ever moves a fully received refusal to
`publication_state::rejected` (nothing in `src/` writes it). Every later Publish
of the same text reconciles GET-only and never sends again
(`test_session_ambiguity_and_refusals` course 0: "production default has no
synthetic authority": one POST, no success, re-entry sends nothing). RFC-BPR-0022
permits retiring that child: "A fully received non-creation response may retire
a child attempt as rejected". The code never does it, so an off-hunk comment
published alone can't be published unless its text changes, and then it is
refused again.

**Confidence:** certain that the fallback cannot fire off PR2 (code, the
composition and course 0). High that the comment gets stuck (code reading plus
course 0; not run against GHES).

### Start a review / Finish your review

The whole review is one `POST /pulls/{n}/reviews` carrying every comment
([[BPR-1345]]). `review_submission_session` has no fallback, no per-comment
placement and no split. `review_submission_comment` carries only path, side,
line and body: no endpoint, no placement, no representation. GitHub refuses the
review whole and does not say which comment caused it. BPR-1715 reads the
refusal (`refusal_names_unplaceable_line`), settles it as not published and
says one comment can't be placed. That is all it does.

- RFC-BPR-0022's fallback is reachable only through
  `review_mutation_session::submit_publication`, after a native single-comment
  attempt. The review path never makes such an attempt and never calls it.
- On BPR-1706's commit-filtered diffs, `comment_destination` keeps an
  unresolvable line out of a review ("Start a review refuses such a line and
  says to use Publish"). On the Whole PR diff nothing does, because placement
  there is not computed.
- The REST review endpoint cannot carry a file comment at all; see the
  measurement below.

**Confidence:** certain.

### The GitHub CLI and native HTTPS transports

- **Single comment:** both transports deliver the 422 with its body. Native
  HTTPS admits 422 for `create_review_comment` (`AdmittedContractStatus`). The
  outcome is the same through either transport: no contract, stuck child.
- **Review:** the GitHub CLI returns the 422 as an answer with its body, so
  `UnplaceableLine` is read. Native HTTPS admits no 422 for a review: it
  arrives as `github_mutation_wire_error` with `HttpStatus` 422 and no body.
  BPR-1715 settles it, but as "GitHub refused this review", never "a line
  GitHub can't place". The owner saw the line sentence, so that profile
  publishes through the GitHub CLI (inference, high confidence).

### Is GHE's wording the cause?

No. BPR-1715's recognizer matched the GHES 3.20.8 refusal; the owner saw its
sentence. It matches "line must be part of the diff", "line could not be
resolved" or "diff hunk can't be blank". A perfect wording match would still
publish nothing, because neither path has a fallback that a review refusal can
reach. The BPR-1715 fixture's wording ("Pull request review thread line must be
part of the diff and Pull request review thread diff hunk can't be blank") is
the older GitHub form and was never observed from 3.20. GitHub.com's current
form is below.

## The owner's exact GHE response

The BPR-1722/1725 export can't supply it. For a review it records only the
ending line, `review_submission_ending=refused run=submit post=error_status
post_class=4 refusal=unplaceable_line stop=none comments=9 duplicate=0`, with
`HttpStatus` 422. GitHub's words are not logged for a review.
`rejection_diagnostic` logs closed `resource= field= code= message=` words only
for a single-comment POST. So the newest export would show GHES's
single-comment shape only if the owner also tried Publish on that comment. The
recommended design below doesn't need the response text. Ask for the export
only if the owner picks a contract-based option (C or N).

## GitHub.com comparison (BPR-Test #124, 2026-10-22)

The throwaway PR #124 added an 80-line file on the base branch and changed line 10
on the head, so line 60 was outside every hunk. The probe was closed with no
review or comment left, and both `bpr-pending-ak/20261022T162629Z/*` branches
were deleted. It made 21 REST/GraphQL calls; `X-RateLimit-Remaining` stayed at
4,960 or above.

| Request | GitHub.com's answer |
|---|---|
| Review (COMMENT) with line 10 and line 60 | 422 `{"message":"Unprocessable Entity","errors":["Line could not be resolved"],"documentation_url":"https://docs.github.com/rest/pulls/reviews#create-a-review-for-a-pull-request","status":"422"}`; the review list stayed empty |
| Review with line 60 only | the same 422 |
| Single comment, line 60 | 422 `Validation Failed`, one error `PullRequestReviewComment` / `pull_request_review_thread.line` / `custom` / "could not be resolved" (the recorded PR2 tuple) |
| Pending review, comment with `subject_type: file` | 422 "…invalid value for 0.subjectType (Field is not defined on DraftPullRequestReviewComment), 0.position (Expected value to not be null)": the REST review cannot carry a file comment |
| Pending review with line 10, then GraphQL `addPullRequestReviewThread(subjectType: FILE)` into it | accepted; the pending review held both comments; deleted without submitting |

So the review's refusal is whole and names no comment, on GitHub.com as on the
owner's host. A file comment can join a review only through GraphQL's pending
review. That is unverified on GHES 3.20.

## Intended versus shipped

| Design | Intended | Shipped | Verdict |
|---|---|---|---|
| RFC-BPR-0022 (single comment) | Native first; a file comment with a permalink after an exact unsupported rejection | Classification enabled only for PR2's exact scope (`dde294fb2`); production placement `unknown` (`e96a62f48`). Owner amendment r3, 2026-09-27: "this supplies no general GitHub.com or GHES guarantee". `docs/development/source-line-comments.md`: "permits fallback only in the designated PR2 validation course" | Designed, deliberately left off |
| RFC-BPR-0022 (review) | Not in scope: the RFC is one logical publication per comment | Nothing | Never designed |
| BPR-1706 | A file comment, with no native attempt, for a line GitHub resolves nowhere | Live (`e46cdedee`, `b8839c3d4`), only when a commit filter is active; Start a review refuses such a line | Works as designed, but not on Whole PR |
| BPR-1715 | Settle a refused review, say why, keep the drafts | Live (`7a00adb52`); the line sentence through the GitHub CLI only | Works as designed |
| RFC-BPR-0090 / BPR-1728 | Hold a suggestion GitHub can't apply and offer Convert | Live (`a7b8b0f6d`) on a filtered diff whose right side is not the head; "on a diff whose right side is the head nothing is held" | Not on Whole PR |
| BPR-1716 | Mark an off-hunk comment before Submit; offer a file comment | Backlog; AC3 waits on a response contract | Not built |

The workbench specification ("Production fallback remains unavailable until
an exact unsupported response contract is admitted", `ff0f7711e`) states the
gap in plain words.

## Design options and duplicate risk

**P. Placement evidence on the Whole PR diff (recommended, the smallest).**
Apply BPR-1706's measured rule to the unfiltered diff. When the merge base is
available, a line is placeable only within `github_comment_context_lines` (3)
of a change in the merge-base-to-head diff on its side (`line_in_change_context`).
RFC-BPR-0089 measured that rule against GitHub on BPR-Test #116 to #119; #124
agrees. A line outside it becomes `review_placement::unresolvable_line`, which
already exists.

- Single Publish: BPR-1706's `admit_unresolvable_line` publishes a file comment
  with a permalink at the head, with no native attempt and no response contract.
- A review carrying such a comment: the Finish your review page holds that
  row, as BPR-1728 holds a suggestion, says GitHub can't place it, and offers
  Publish as file comment. Choosing it publishes the comment alone, through the
  single-comment path, and takes it out of the review. Submit then sends the
  rest. Adding such a line to a review gets BPR-1706's existing refusal, which
  says to use Publish. This is [[BPR-1716]]'s AC1 and AC2.
- After a refusal with `refusal=unplaceable_line`, the same rule names the
  comment, where today the page has to say "GitHub doesn't say which".
- Duplicates: none new. The file comment is one RFC-BPR-0022 logical
  publication, with at most one comment and GET-only reconciliation when the
  outcome is in doubt. It leaves the review before the review's POST. A comment
  whose file child may have been sent stays held and never rejoins a review.
  A wrong placement can't duplicate either. If the core says placeable and the
  host refuses, the review is refused whole again; BPR-1715 settles it and
  nothing is published. If the core says off-hunk and the host would have
  placed it, it becomes an honest file comment.
- Owner decision needed: admit this placement evidence for the Whole PR diff.
  That amends the specification sentence above and BPR-1716's AC3: the offer
  rests on measured placement, never on the host's name or version. When the
  merge base is unavailable (the left side is then the base branch's tip),
  placement stays `unknown` and nothing changes.
- Unverified: that GHES 3.20 accepts `subject_type: file` on
  `POST /pulls/{n}/comments` (REST added it in 2023; very likely). If it
  refuses, the file child's 422 is a definite refusal and the draft is kept.

**R. Split after the refusal (the owner's candidate 2).** On
`refusal=unplaceable_line`, mark the comments P's rule places outside the diff
and offer "Publish N as file comments and submit the rest". The duplicate
controls are P's. The review that was refused was definitely not created
(BPR-1715 checks the review list first). The new review is a new attempt with
its own key, started by the reviewer: the core never retries by itself
(BPR-1715). Its only cost is one wasted submit. It is P's fallback for a
comment written before the upgrade, and P covers it if marking happens before
Submit.

**C. Admit an unsupported-response contract for the review 422 (the owner's
candidate 1).** Not sufficient alone. The review refusal names no comment, so
no contract can say which one to convert. Applied automatically, it would be
the automatic retry BPR-1715 rules out. The exact GHES text is also missing
(see above). It adds nothing that P and R lack.

**N. Widen the single-comment contract past PR2.** A new classifier revision
would accept the single-comment 422 tuple (field `line` or
`pull_request_review_thread.line`, the three recognized messages), any
`documentation_url`, and no rate-limit headers. It would require P's placement
proof as well, because "could not be resolved" can describe other validation
failures (the r3 proposal, `m4-owner-contract.md`). It needs GHES's exact
tuple, from an export after a single Publish. With P's evidence the native
attempt is pointless, so N is larger than P for the same result.

**G. One review holding file threads.** Create a pending review with the
placeable comments, add each off-hunk one with GraphQL
`addPullRequestReviewThread(subjectType: FILE)`, then submit. This works on
GitHub.com (#124). It keeps one review, but it replaces BPR-1345's atomic
single POST with a three-step protocol, which needs new recovery for a pending
review left behind. GHES 3.20 is unverified. A later improvement, not the
smallest fix.

**Separately, whatever is chosen: retire a refused native child.** A fully
received 4xx to a single comment's native POST should move the child to
`rejected` (or, under P or N, to the fallback). Then the window reports "not
published" and the draft is kept, instead of an indeterminate record that
reconciles for ever. No duplicate: GitHub validates before it creates, and
the child is no longer eligible to POST.

## Proposed acceptance (after the owner chooses)

- AC1: On the Whole PR diff with its merge base, a line comment outside every
  hunk GitHub resolves is marked before Publish or Submit as one GitHub can't
  place, from the core's placement, never from the host's name or version.
- AC2: Publish sends it as RFC-BPR-0022's file comment with a permalink to the
  line at the head, once, with no native attempt. In a review it is held with
  Publish as file comment, and Submit sends the rest as one review. Neither
  can publish it twice across retries or a restart.
- AC3: A single comment whose native POST GitHub refuses with a fully received
  4xx is reported as not published, and its draft is kept and can be published
  again.
- AC4: After a refusal that names an unplaceable line, the page names the
  comment the core places outside the diff.

## Owner's way through today

Keep the review. Delete the off-hunk comment from it (BPR-1715's Delete, after
copying its text), Submit the other eight, and post that text on the GHE web
page, as a comment on the file where the page offers one, otherwise in the
conversation.

## Owner decision, 2026-10-22

Verbatim: "Accept option P (hold off-hunk comments before Submit and publish
them as file comments; single Publish goes straight to a file comment), plus
marking a refused single comment rejected. Build it."

Recorded as RFC-BPR-0022 owner amendment r5, in the workbench
specification's "Source-line comments outside a hunk", and as BPR-1716's
restated AC3.

## The placement rule and its amendment

On the Whole PR diff, and on a commit-filtered diff's sides that are GitHub's
own (a right side that is the head, a left side that is the merge base), a
line is placeable as a native line comment only within three lines
(`core::github_comment_context_lines`) of a change of the merge base to the
head on its side. A line at exactly three lines is in; at four it is out.
Hunks whose context touches are one hunk, as Git's are, so changes six lines
apart share their context and seven apart leave the middle line out. A range
is placeable only when all of it lies in one hunk; a range outside has no file
comment. The rule reads the merge base to the head the epoch host computes
with the review's own engine and exact whitespace; with no merge base named,
or the file unreadable, nothing is inferred and Publish is as it always was.
It never reads the host's name or version.

The amended specification sentence (it replaced "Production fallback remains
unavailable until an exact unsupported response contract is admitted; the UI
does not infer support from host name or version."):

> The owner admitted measured placement in place of an exact unsupported
> response contract on 2026-10-22 ([[BPR-1735]] …). On the Whole PR diff, and
> on a filtered diff's sides that are GitHub's own, the core places a line as
> a line comment only within three lines of a change of the merge base to the
> head on its side, the rule measured on BPR-Test #116 to #119 and #124. …
> The UI does not infer support from host name or version.

## Change

- **Core.** `place_on_github_diff` (`in_diff`, `outside_diff`,
  `range_outside_diff`, `unknown`), `holds_review_comment`,
  `place_whole_diff_comment`; `place_filtered_comment` applies the rule to a
  filtered diff's own sides (a new refusal `range_outside_github_diff`, last).
  The publication policy plans a `rejected` publication as a new attempt, and
  `publication_definitely_unpublished` decides from a kept answer whether a
  possibly sent child is settled: a 4xx at once, a record kept before answers
  ten minutes after its migration, nothing else.
  `review_publication_outcome::rejected` unlocks a draft whose outcome was
  unknown.
- **Epoch host.** `resolve_github_diff_lines`: one file at the merge base
  (the accepted comparison's left) and the head, compared.
- **Mutation host.** `review_mutation_source_observation::WholeDiff`; the host
  places a Whole PR comment by the core before any effect: a line outside every
  hunk is bound as `unresolvable_line` at the head (a base line only when its
  document is the merge base's, with the left document for the permalink), a
  range outside is refused, and `comment_destination` refuses such a line.
- **Provider.** The file comment is admitted at once on the Whole PR diff
  too; the permalink names a Whole PR base line at the comparison's left
  commit. A fully received 4xx (an answer, or an error carrying the status),
  with GitHub's complete list holding no comment of it, rejects the child and
  reports `unplaceable_line` (GitHub's words name the line) or
  `refused_not_published`; a migrated stuck record settles as
  `settled_not_published`. Each child's POST answer is kept.
- **Journal.** `rejected` is a valid durable state (a sent native child, or a
  sent file child under its authorization); it starts again under the same key
  as `native_prepared`, `unsupported_evidence_admitted` or
  `unresolvable_line_admitted`. Recovery schema 8 adds `publication_post`;
  migration from 7 keeps every logical publication as answered before answers
  were, at the migration time.
- **Composition.** Reads GitHub's own diff for the selected line's file and
  for every file holding a pending review comment (no file need be shown),
  places the selected line, holds each pending comment the core places
  outside, and refuses Submit while one is held
  (`review_submission_workflow::hold_comments`).
- **Qt.** The editor's notice and the suggestion hold on the Whole PR diff;
  `present_unplaceable_review_comments` holds a pending comment on the
  comment itself and on Finish your review with **Publish as file comment**
  (`review_submission_editor::held_kind::unplaceable_line`), in the reading
  amber, every verdict waiting; Publish as file comment opens the comment at
  its line and presses its own Publish once the core has placed it on the
  file. A held range offers no action.

frontend-design:frontend-design was loaded before the held mark's words and
layout were written; the mark reuses BPR-1728's hold (reason under the row in
the reading amber, the action beside Delete, the verdicts waiting) with no new
visual language.

## Tests and live results

- Core, journal, provider, host, Qt and composed courses as recorded in the
  acceptance above. Each fails before the change: the new placement
  functions, the WholeDiff fact, the hold and the journal's `rejected` state
  do not exist on main, and the owner course waits on a hold main never sets.
- Live, github.com, BPR-Test #127 (2026-10-22, closed, both branches deleted):
  the composition's placements for head lines 60, 14 and 13; the review with
  line 60 refused whole with 422; line 60 published alone as a file comment
  (201, `subject_type` file at the head); the rest as one review (200, RIGHT
  13); line 14 natively 422, as a file comment 201.
  `X-RateLimit-Remaining` stayed at 4,844 or above. An earlier run, #126, made
  the same refusal, file comment and review, then stopped on a check of the
  wrong list (the review's own comment list reports positions); it was closed
  and its branches deleted. Six comments remain on the two closed pull
  requests.

## Remaining GitHub Enterprise Server assumptions

Recorded on BPR-1737: GHES 3.20 takes `subject_type: file` on
`POST /pulls/{n}/comments`, and resolves line comments by the same three-line
rule. Neither can be observed without the owner's host.

## What the owner does on the new build

Open the pull request whose review was refused on 2026-10-21 and open Finish
your review. The comment on the line outside every hunk is marked "GitHub
shows no change near line N, so it can't place this comment in a review." and
Submit waits. Choose Publish as file comment on that row: the comment opens at
its line and is published once on its own, as a comment on the file whose
first line links to that line. The page then carries the other eight; choose
Comment (or the verdict wanted) to submit them as one review.

[[BPR-1736]] (Backlog) designs how the application learns that a host takes a line comment beyond this rule, from GitHub's own answer, and places it natively there.
