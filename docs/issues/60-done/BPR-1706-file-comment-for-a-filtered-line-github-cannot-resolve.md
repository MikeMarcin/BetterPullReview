---
schema: "implement-rfc.card.v1"
id: "BPR-1706"
title: "Offer RFC-BPR-0022's file comment with a permalink for a filtered diff's line GitHub cannot resolve"
status: "Done"
rfcs: ["RFC-BPR-0089","RFC-BPR-0022"]
depends_on: ["BPR-1704"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1703--BPR-1708--2cc864d62b8c.json","origin":"RFC-BPR-0089 'Comments and line anchoring': a left-side line that does not map to the merge base 'takes RFC-BPR-0022's file comment with a permalink'. M4 refuses it before Publish instead.","related_cards":["BPR-1703","BPR-1705","BPR-1707"],"frontend_design":"frontend-design:frontend-design loaded before the editor's file-comment notice and the Start a review refusal were written"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e46cdedee26a429fa542190bf1773ff885fcf242","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_filtered_comment|application_review_mutation_application_host|github_review_comment_permalink|application_commit_filter_public_path)$'","detail":"Fixture, shipped composition, offscreen (commit-filter-comment-v1): with only C3 shown, src/notes.txt's base line 15, which C1 changed, reads before Publish 'This line changed after the merge base, where GitHub counts base lines, so GitHub can't attach a comment to it. Publish adds a comment on the file instead, linking to line 15 of commit 3a82b6d.', and Publish writes one file comment at the head (commit_id C3, the head's path, subject_type file) whose body starts with the permalink to C2's src/notes.txt#L15; it reconciles once (one stored comment more, Published). Provider: the left document's endpoint names the line at the filter's left commit; the journal records unresolvable_line_admitted, then the fallback child, then succeeded_fallback, and a replay or a restart returns the receipt with no POST. Live on BPR-Test #121 (closed, threads resolved, branches deleted): GitHub answered a current file subject at the head. macOS."}},{"criterion":"AC2","outcome":"pass","revision":"e46cdedee26a429fa542190bf1773ff885fcf242","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_filtered_comment|application_commit_filter_public_path)$'; python3 tests/end_to_end/rfc0089_commit_filter_live_course.py --build build/dev --work DIR --prefix bpr1706-live2 --comment --cleanup","detail":"With C3 hidden, C1's line 10 is unchanged at the head but outside every hunk of the merge base to the head (GitHub refused it with 422 on #117). The editor says 'GitHub shows no change near this line at the head, so GitHub can't attach a comment to it. Publish adds a comment on the file instead, linking to line 10 of commit 4613350.', and Publish writes one file comment at the head whose permalink names C1's src/notes.txt#L10. A changed line outside every hunk of the merge base to C1 (not_on_commit) is the same in the core and the host. Live #121: all six placements matched Git and GitHub (two line comments at the head, one outdated on C1, three file comments at the head, current), markers matched and Viewed was unchanged for 326 files."}},{"criterion":"AC3","outcome":"pass","revision":"e46cdedee26a429fa542190bf1773ff885fcf242","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_review_round_trip|core_filtered_comment|sqlite_review_mutation_recovery_storage|github_review_comment_permalink|application_commit_filter_public_path)$'","detail":"No native attempt: the composed course counts two file posts and the same three native posts as before; the provider test sees exactly one POST, the file representation, and no merge-base read for a left line. The core admits an unresolvable line only from the initial state, refuses a prepared native child for it, refuses a suggestion before any write and admits no other placement as unresolvable; the journal refuses the admission with a native child, with a freshness window or labelled as scoped evidence. The provider refuses an unresolvable source without a filter, with a moved line, on a left line without its document and as a range, with nothing written. Negative controls: with the core's file_comment placement turned back into a refusal and its admission back into a native attempt, core_filtered_comment and core_review_round_trip fail."}}]
---

# BPR-1706: Offer RFC-BPR-0022's file comment with a permalink for a filtered diff's line GitHub cannot resolve

RFC-BPR-0089 says a left-side line of a filtered diff that the merge base
does not have unchanged "takes RFC-BPR-0022's file comment with a permalink".
M4 refused such a line before Publish, saying why, and did the same for a
right-side line GitHub resolves nowhere (outside every hunk at the head or on
the diff's commit).

## Why it was not in M4

RFC-BPR-0022's publication policy reaches the file comment only after a
native attempt is definitively unsupported and that evidence is admitted
under the recorded off-hunk contract; it has no transition that starts at the
fallback. Its permalink also names the head with the selected line, which is
wrong for a line of `parent(first)` or of an earlier commit. A direct
fallback needs a core decision, a journal state and a permalink at the diff's
own commit, each with its reconciliation.

## Measurement

BPR-Test #119 (closed, its three threads resolved, both branches deleted),
shaped as #116. With `subject_type: file`:

- `commit_id` the head: accepted, current, shown on Files changed; GitHub
  reports `line` and `original_line` 1 on `RIGHT`. A permalink in the body to
  a line of C1, or of the base branch's commit, renders as that line's code.
- `commit_id` C1: accepted and moved to the head (`original_commit_id` C1).
- `commit_id` C1 on a path C1 did not change, and `commit_id` the base:
  refused with 422, `pull_request_review_thread.path` "could not be
  resolved".

Fourteen REST and GraphQL calls; `X-RateLimit-Remaining` stayed above 4,980.
So the file comment is written at the head, and its permalink names the line
at the diff's own commit on its side.

## Change

- **Core.** `place_filtered_comment` answers `file_comment` (the reason in
  `Refusal`; `Commit`, `Side` and `Line` what the permalink names) where it
  refused `not_on_commit`, `base_line_changed` and `outside_github_diff`.
  `review_placement::unresolvable_line` is the policy's input for it:
  `plan_review_publication` admits it straight to the file comment
  (`admit_unresolvable_line`, state `unresolvable_line_admitted`), from the
  initial state only and never with a native child. New enumerators are last,
  so stored values keep their meaning.
- **Journal.** `review_fallback_authorization_source::unresolvable_line` is a
  first-attempt authorization with no freshness window; the SQLite record
  bounds take the new values, and an earlier reader refuses them.
- **Host.** The mutation host binds such a line at the head with no moved
  line, marks its source `unresolvable_line`, and carries the left document
  for a left line (`review_mutation_source_binding::LeftEndpoint`).
  `comment_destination` refuses it: a review's comments are line comments.
- **Provider.** `submit_publication` admits the unresolvable line durably,
  binding the exact file-comment wire request and the authority, then runs
  the existing fallback child, witness, reconciliation and replay. It reads no
  merge base for a left line published this way.
- **Permalink.** On a filtered diff the codec names the line at the filter's
  commit on the endpoint's side; a filtered diff's left endpoint is admitted.
- **Qt and composition.** The editor says why GitHub can't attach a comment
  to the line and that Publish adds a comment on the file, linking to the line
  of the commit; Publish stays enabled. Start a review refuses such a line
  and says to use Publish.
- **Courses.** The composed course publishes the two lines M4 refused; the
  `--comment` live course posts the decided file comments and checks GitHub's
  answers.

## Acceptance

- **AC1.** With only C3 shown, a base line C1 changed publishes, after the
  editor says so, as a file comment whose permalink names that line at the
  diff's left commit; it reconciles once.
- **AC2.** A right-side line GitHub resolves nowhere does the same at the
  diff's right commit.
- **AC3.** No native attempt is made for either, and no existing admission
  check weakens.

## Not covered

- A file comment from a filtered diff stays a file-level conversation: the
  window projects permalinks only for an unfiltered diff, and on Whole PR the
  permalink is not the head's line.
- Windows and Linux are unmeasured.
