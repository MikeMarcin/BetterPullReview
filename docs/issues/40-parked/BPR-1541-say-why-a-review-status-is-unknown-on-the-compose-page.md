---
schema: "implement-rfc.card.v1"
id: "BPR-1541"
title: "Say why a review's status is unknown, on the compose page"
status: "Parked"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-1540"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner decision 2026-10-15 on BPR-1540's open question (relayed by the coordinator): the compose page should show a short plain-language reason under 'Review status is unknown'","governing_rfc":"RFC-BPR-0008","related_cards":["BPR-1540","BPR-1538","BPR-1345"],"reservation":"docs/issues/_reservations/BPR-1541--BPR-1541--c23556c64e37.json","design_skill":"frontend-design:frontend-design"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d88fc46f89c4f45f397c364054cdc5a452a8200b","evidence":{"kind":"command","detail":"qt_review_submission_editor_scale_{100,125,200}: each of 20 reasons shows its own distinct line, equal to the status's accessible description, short, wrapped, no identity; counts singular/plural; 4xx/5xx/other distinct; status text colour with >= 4.5:1 in four themes."}},{"criterion":"AC2","outcome":"pass","revision":"d88fc46f89c4f45f397c364054cdc5a452a8200b","evidence":{"kind":"command","detail":"application_start_review_contract kept/restored courses: the restarted window shows the kept network line while Check status is held ('Checking the original review'); host and provider rows carry and read the kept reason by key."}},{"criterion":"AC3","outcome":"pass","revision":"d88fc46f89c4f45f397c364054cdc5a452a8200b","evidence":{"kind":"command","detail":"uncertain course: a 503 Check status replaces the line, a confirming Check status clears it; Qt rows: no line while drafting or without a reason."}},{"criterion":"AC4","outcome":"pass","revision":"d88fc46f89c4f45f397c364054cdc5a452a8200b","evidence":{"kind":"command","detail":"Negative controls: restored course fails at restored-reason-not-shown without the kept reason; Qt row fails without the accessible description; Qt and host rows' own controls."}},{"criterion":"AC5","outcome":"pass","revision":"d88fc46f89c4f45f397c364054cdc5a452a8200b","evidence":{"kind":"command","detail":"clang-format 22.1.8 passes; tidy clean on changed lines; verify_documentation OK; round-trip composition contract resealed on the branch tree (5996b22d.../740e0bc3...) and both composition contracts pass; reseal again on merge."}},{"criterion":"AC6","outcome":"pending","revision":"d88fc46f89c4f45f397c364054cdc5a452a8200b","evidence":{"kind":"freeform","detail":"Owner Accept/Reject pending in session 2aad932f341313c448a9e890b46139d5, candidate BPR-1541-reason-line-v001, fingerprint dd7b3d83d9e89afd2c807495c0ac0c29ab51dcf0110bf56da2b92844ab657815, cursor 4; frozen root /Users/jaeger/bpr-1541-unknown-reason-v001.tc7b6nl5 (executable 1d4eb8904f69...), with a 20-line Light/Dark sheet."}}]
---

# BPR-1541: Say why a review's status is unknown, on the compose page

[[BPR-1540]] records why a submitted review's outcome is unknown: one of 20
closed reasons with counts. It keeps the reason with the attempt and writes
it to the diagnostics log. The compose page still said only "Review status
is unknown. Check status to reconcile this attempt; do not submit it again."
On 2026-10-15 the owner decided the page should also show a short reason in
plain language under that sentence.

## Design

The `frontend-design` skill was loaded for this card. The page is a native
surface that inherits its palette and fonts, so there is no new visual
identity here. The decisions are the words and where the line sits.

- **One line under the status.** It reads as the status's second sentence. It
  uses the same type and text colour as the status, plain and wrapped, and the
  reader can select it. It has no icon or tint, so it reads the same in Light,
  Dark and both High Contrast themes.
- **What happened, then what to do.** Each line is written for a reviewer.
  It names the button the reviewer will press ("Check status again") or the
  place to look ("Look at the pull request on GitHub"). Lines that need a
  second account or a sign-in say so.
- **Counts only where they help.** A mismatch says "only 2 of 3 comments
  matched", or "it holds 2 comments and this one has 3 comments". An HTTP
  status says whether GitHub refused the read or its server failed. No
  other number reaches the page.
- **Nothing private.** No host, repository, account, path, URL or comment
  words appear in any line.
- **Accessible.** The line is the status label's accessible description.
  It is part of the page's polite announcement, and it has its own accessible
  name.

## Ownership

- **Core decides the text key.**
  `core::notice(review_submission_unknown_diagnosis)` returns a
  `review_submission_unknown_notice`. That is the reason, with
  matched-of-carried or held-of-carried comment counts only for
  `comments_mismatch`, and status class 4 or 5 only for `http_status`.
- **Qt presents.** `qt::review_submission_unknown_text(notice)` holds the 20
  lines. `review_submission_editor` shows the line under the status only for
  a review awaiting confirmation.
- **The host carries the kept reason across a restart.** The submission port
  gains a read-only `last_unknown(key)`, which the session reads from the
  attempt's record. Discovery now carries each restored attempt's kept
  reason. `review_submission_workflow` shows that reason when it restores the
  attempt. Each Check status answer replaces it, and the line disappears on
  confirmation, refusal, a new review or another pull request.

## Lines

| Reason | Line |
|---|---|
| `review_not_listed` | GitHub hasn't listed this review yet. Check status again in a moment. |
| `several_reviews_listed` | GitHub lists more than one matching review from you, so this one can't be picked out. Look at the pull request on GitHub. |
| `review_list_unreadable` | GitHub's list of reviews couldn't be read. Check status again; if this repeats, look at the pull request on GitHub. |
| `review_list_too_large` | This pull request's list of reviews is too large to read here. Look at the pull request on GitHub. |
| `review_list_paged` | This pull request has more than 100 reviews, more than can be read here. Look at the pull request on GitHub. |
| `comment_page_unreadable` | A page of this pull request's comments couldn't be read. Check status again; if this repeats, look at the pull request on GitHub. |
| `comment_page_too_large` | A page of this pull request's comments is too large to read here. Look at the pull request on GitHub. |
| `comment_page_link_refused` | GitHub pointed to a next page of comments that isn't this pull request's, so it wasn't read. Look at the pull request on GitHub. |
| `comment_collection_too_long` | This pull request has more than 1,000 comments, more than can be read here. Look at the pull request on GitHub. |
| `comments_mismatch` | A new review from you was found, but only 2 of 3 comments matched. Look at it on GitHub before submitting anything. |
| `comment_moved_without_original_position` | A comment moved after a new commit, and GitHub doesn't say where it was written. Look at the review on GitHub. |
| `http_status` (5xx) | GitHub had a server error. Check status again in a few minutes. |
| `rate_limited` | GitHub's rate limit for your account was reached. Check status again later. |
| `timeout` | GitHub took too long to answer. Check status again. |
| `network` | GitHub couldn't be reached. Check your connection, then check status again. |
| `account_mismatch` | The account signed in now isn't the one that submitted this review. Sign in with that account, then check status. |
| `pull_request_mismatch` | This pull request no longer matches the one the review was sent to. Look at the pull request on GitHub. |
| `connection_unavailable` | There's no connection to GitHub for this account right now. Sign in again, then check status. |
| `local_record_unavailable` | This computer's record of the submission couldn't be read. Look at the pull request on GitHub. |
| `cancelled` | The check stopped before it finished. Check status again. |

The variants of two of these lines:

- **`comments_mismatch`:** "…but it holds N comments and this one has M
  comments." when the new review holds another number. When no comment was
  carried, it says "…but its comments don't match this one."
- **`http_status`:** "GitHub refused to show this pull request's reviews.
  Make sure you can still open it, then check status again." for 4xx, and
  "GitHub answered with an error. Check status again." otherwise.

## Acceptance

- **AC1, every reason renders its line:** each of the 20 reasons shows its
  own distinct line under the status, which is also the status's accessible
  description. Each line is short, wraps inside the page, uses the status's
  text colour readably in all four themes, and names nothing private.
- **AC2, from the kept reason, across a restart:** a restarted window shows
  the reason the attempt's record kept, before Check status answers.
- **AC3, replaced and cleared:** each Check status answer replaces the line.
  It is absent once the review is confirmed, and absent while a review is
  being written.
- **AC4, negative controls:** no reason, or a draft being written, shows no
  line; a mismatched or missing link in the chain fails its row.
- **AC5, gates:** clang-format 22, tidy, documentation gate, composition
  contracts resealed on the merged tree.
- **AC6, owner (attended):** the reason line under "Review status is
  unknown" is clear and helpful. The question is asked on a frozen build,
  with a render sheet of all 20 lines in Light and Dark.

## Evidence

Candidate: branch `bpr-1541-unknown-line` at `d88fc46f8`, not merged. It
lands only after the owner accepts it, with the composition contracts
resealed again on the merged tree. macOS, Debug without debug information.

- **Core.** `core_review_submission` `test_unknown_notices`: each reason keeps
  its line, and carries counts only for `comments_mismatch` and a status
  class only for `http_status`. As many comments held as carried gives
  matched-of; a different number gives held-of; no carried comment gives no
  count; status classes 0, 2 and 3 are not told apart.
- **Qt.** `qt_review_submission_editor_scale_{100,125,200}`:
  - With no reason there is no line.
  - Each of the 20 reasons shows its own distinct line: non-empty, ending
    with a full stop, at most 170 characters, and equal to the status's
    accessible description. None names the host, owner, repository, account,
    head or summary in the projection, and each wraps inside a 300-pixel page.
  - The counts read "only 2 of 3 comments matched", "it holds 2 comments and
    this one has 3 comments" and "only 0 of 1 comment matched". A refused
    read and an unclassified error read differently from a server error.
  - In Light, Dark and both High Contrast themes the line uses the status's
    own text colour, with contrast of at least 4.5:1.
  - Negative controls: a cleared reason and a draft being written show no
    line.
  - Mutation control: without the accessible description the row fails at
    "the status's description says it".
- **Host.** `application_review_submission_application_host`: discovery
  carries the discovered attempt's kept reason, and only that one. An invalid
  kept reason is not carried; with none kept, none is carried.
- **Provider.** `live_review_submission_provider`: a restarted session reads
  the kept reason back by key. Another key reads none, and once Check status
  confirms the review there is none.
- **Composed application.** `application_start_review_contract` passes all
  seven courses:
  - `uncertain` (`1111111`): held, with the network line shown and part of
    the description. Check status with a 503 replaces it with the
    server-error line. Check status with the list readable confirms the
    review with one POST, and the line is cleared.
  - `kept` (`1111`): over its own kept data root.
  - `restored` (`11`): that root restarted with the review list's reads held.
    The restored review shows the kept network line while the status says
    "Checking the original review", and the answer replaces it.
  - Negative control: with the restored attempt not taking its kept reason,
    `restored` fails at `restored-reason-not-shown`.
  - Along the way the fixture's borrowed store was found not to forward
    `note_unknown`, so the composed fixture never kept a reason. It now
    forwards it; the live provider was unaffected.
- **Gates.**
  - `application_review_round_trip_composition_contract` resealed for this
    tree: full digest `5996b22d…`, production manifest `740e0bc3…`. It and
    `application_composition_contract` pass.
  - clang-format 22.1.8 check of the 22 changed C++ files passes.
  - clang-tidy reports nothing in changed lines. The fixture, host and host
    test report findings on lines that are on `main` unchanged.
  - `verify_documentation` passes.
- **Owner (AC6).** A frozen build: root
  `/Users/jaeger/bpr-1541-unknown-reason-v001.tc7b6nl5`, executable sha256
  `1d4eb8904f69…`. Its `--check` passed: `ATTENDED_UNKNOWN_REVIEW_SHOWN`.
  The look is `--attended-unknown-review-look`, which holds an unknown review
  open showing its network line. Session `2aad932f341313c448a9e890b46139d5`
  asks one Accept/Reject question, candidate `BPR-1541-reason-line-v001`, with
  a render sheet of all 20 lines in Light and Dark. The answer is pending.
