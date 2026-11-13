# RFC-BPR-0111 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `publish-prepared-draft`
([[BPR-2029]], [[BPR-2030]], closeout [[BPR-2031]]). RFC_BASE_SHA:
`dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`. Frontend-design was loaded before
the Qt work. Owner amendments in force: "If you need it you need it go build
it."; "main should be the source of truth for the tracker"; "Continue working
unattended, you can file reviews but dont block on their results". Debug
timings are nongating; nothing below is a timing claim.

PUBLISH-01 is implemented as settled on 2026-11-10: the author pushes with
their own Git and BPR names the push. Its reopening (the local-draft
authoring pack's amendment A1) belongs to the `commit-and-push` milestone; as
that amendment says, the fallback built here is still required and nothing is
lost.

The cards have not moved: [[BPR-2029]] depends on [[BPR-2025]] and
[[BPR-2028]], the closeouts of RFC-BPR-0109 and 0110, whose looks the owner
rejected on 2026-11-12 (recorded on [[BPR-2021]], [[BPR-2023]], [[BPR-2024]]
and the RFC-BPR-0110 ledger). By the coordinator's direction this round kept
to the core, application and host, with a minimal Qt surface built from
existing controls, and filed no owner look.

## What was built

| Layer | What |
|---|---|
| Core (`core/local_draft_publication.hpp`) | `decide_hosted_branch` (matches, missing, differs, uncommitted, unobserved); `name_push` (the exact candidate commit to the draft's branch in the resolved hosted repository, run in the worktree, never `origin`, never forced); the plan (candidate, comparison base, target tip at capture and as observed, destination, account, title, body, draft, selected evidence, results that do not speak for the candidate, what stays private, the tour follow-up, unaccepted stops) and its SHA-256 digest; the preview in words; the journal (prepared, authorized, creating, linked, uncertain, refused, cancelled), its steps (waiting, sending, sent, failed, not sent) and follow-ups; authorize, begin, complete, restart, observe after uncertainty, adopt an existing pull request, cancel, record a follow-up; drift; `tour_follow_up_blocked` (X20). The journal is stored in the draft's record |
| Adapter (GitHub) | `acquire_pull_request_session`: the catalog's reads and `POST pulls` only. `gh_cli_transport::perform_publication` runs those five operations through `gh api` in its write containment and refuses every Git-data write before launch; the observed-account lease runs it under its scoped gh operation with the account read before and after. `acquire_publication_session` (Review Cycles) stays native-only |
| Application | `check_local_publication`, `create_local_pull_request` (confirm, re-read branch and open pull requests, record, send once, record the answer, read at once after a lost answer), `recover_local_publication` (reads only), `associate_local_pull_request`; every journal step re-reads and re-applies on a competing write. The workbench runs them on the desktop's own publication lane and shows the recorded creation before GitHub answers |
| Qt | A Pull request area from existing controls: the state line, Check GitHub, Create draft PR, the named push with Copy, Link to this draft for an open pull request, the tour follow-up, the plan's text, RFC-BPR-0099's step list and error-role banner. No new visual design (the coordinator, 2026-11-12) |
| App | `set_publishing` with the profile's accepted account; a tests-enabled build composes it only with `DesignatedLivePublication`, wrapped to refuse every repository but MikeMarcin/BPR-Test |

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | Live: `tests/end_to_end/live_draft_course.py` with `live_second_account_course --course live-draft` at `62f740fc6` (tests-enabled, offscreen, signed in through gh as MikeMarcin, the window's own Check GitHub and Create draft PR). Unpushed: "bpr-0111-live-1794465982 is not on GitHub. Push it, then check again." and the push `git -C …/work push https://github.com/MikeMarcin/BPR-Test.git fde47f6…:refs/heads/bpr-0111-live-1794465982`; the driver ran exactly that command, then the course created draft #179; `gh`: #179 draft, open, head `fde47f631b4e…` (the local commit), author MikeMarcin, base main. Earlier run #178 (same result). Both closed, branches deleted. `local_draft_publication` (real worktrees, a scripted catalog over a hosted bare repository): signed out stops at the named push; missing, different head, working files and an isolated local clone each name the push or refuse; one creation of the author's commit; the checkout byte for byte unchanged throughout | Pass |
| AC02 | `core_local_draft_publication`: the digest changes with the body and the account; a wrong digest or another account authorizes nothing; a successor candidate makes the plan stale; only the selected claim is public, the other claim and the note are counted as staying local and their text is not in the preview. `local_draft_publication`: a wrong digest and another account create nothing | Pass |
| AC03 | `core_local_draft_publication`: a candidate with working files is never the pushed branch; base movement is said in the preview; a selected result that does not speak for the candidate is listed as unresolved; the link records drift when the head or base is not the plan's, and the tour follow-up refuses a drifted pull request. `local_draft_publication`: dirty material refuses as uncommitted | Pass for creation; evidence and stops are not remapped by any path (no follow-up maps them yet) |
| AC04 | `local_draft_publication`: a lost answer after GitHub created it is found by reading, no second creation; one lost before GitHub had it reads nothing and returns to prepared, created only on the author's next press; a restart mid-creation is uncertain and recovered only by reading; a branch pushed after the preview sends nothing and names the push again; an existing pull request is offered and associated without a creation. Negative control: without the re-read before creating, the course fails at "concurrent drift sends nothing". `core_local_draft_publication`: no confirmation survives a restart; cancel works only before sending | Pass |
| AC05 | `core_local_draft_publication`: a failed tour follow-up leaves the pull request associated and a new plan is refused as linked. `local_draft_publication`: an associated existing pull request's description is untouched. Nothing here writes a description after creation | Pass for association; the tour follow-up is not built (below) |
| AC06 | Live #178 and #179 are ordinary GitHub drafts; the draft record keeps its candidates, evidence and tour after association and leaves In preparation for the pull request's Home mark (`core_local_draft_publication`). GHES stays unqualified (`admitted_publication_server`); fork heads are not supported (the head is the target repository's branch) | Pass for the draft; tour part open |
| AC07 | `application_local_draft_publication_desktop_public_path` (real window, real composition, real worktree, scripted catalog): Create draft PR shows Sending, then "Sent: Create the draft pull request" and "Sent: Link it to this draft"; a lost answer with nothing found is the error-role banner "The draft pull request was not created." with Preview again; a page edit (a note) is recorded while a creation is held in flight on the publication lane. `core_local_draft_publication`: an unaccepted stop does not block the plan and blocks the tour follow-up. A review submission's independence is by construction (its own per-host lane, RFC-BPR-0105) and has no course with a creation in flight | Pass on fixture except the submission clause, structural only (completed 2026-11-13, below) |

Not built: the tour follow-up. A plan records it, the step shows "Publish
the tour" waiting with its reason (unaccepted stops, drift, or "Publish it
from the pull request's Tour"), and nothing carries the local tour to the pull
request's tour: a pull request's tour lives in the profile's tour store under
owner identities only the window's read of that pull request knows, and
`map_local_publication` is still called by no publication. The author can
publish a tour on the pull request through RFC-BPR-0099 as for any pull
request.

## Runs

At `433068905` and after formatting (dev, Debug): `core_local_draft_publication`
(new), `core_local_draft`, `core_local_draft_evidence`, `core_local_draft_tour`,
`local_draft_publication` (new), `gh_cli_transport`,
`application_local_draft_desktop_public_path`,
`application_local_draft_evidence_desktop_public_path`,
`application_local_draft_tour_desktop_public_path`,
`application_local_draft_publication_desktop_public_path` (new),
`architecture_boundaries` (after keeping provider address fields out of the Qt area and `node_id` out of core),
`architecture_boundary_contract`,
`application_review_round_trip_composition_contract` (production app source pin resealed with its note: a publication change),
`application_composition_source_contract`, clang-format 22 and clang-tidy on every new and touched unit,
`test_network_isolation_manifest` and `verify_test_network_isolation_tests`
(green again: the opt-in live agent row registers through the isolation
owner, the push course and this milestone's rows are pinned). Live:
`live_draft_course.py` twice (#178, #179), `X-RateLimit-Remaining` 4,999 →
4,869 across five runs (three failed runs before the scoped-operation fix sent
no write and pushed nothing).

## Owner amendment: no evidence (2026-11-12)

The owner cut Evidence from 0.5.0; AC03 is amended to tour anchors only. The
plan no longer binds evidence (`Evidence`, `Stale` and `PrivateRecords` are
gone; the preview says what stays: notes and agent conversations), and a
stored plan that bound evidence is carried forward after its digest is
checked. `application_local_draft_evidence_desktop_public_path` and
`core_local_draft_evidence` no longer exist. Runs: see the
[RFC-BPR-0110 ledger](RFC-BPR-0110.md#owner-amendment-evidence-removed-2026-11-12);
the live BPR-Test course was not rerun (no network-facing change).

## In the pull request window (2026-11-13)

The accepted design's publish surfaces, at `6887928bd` (dev, Debug, macOS):

| Criterion | Evidence | Result |
|---|---|---|
| AC01, AC07 | `application_local_draft_publication_desktop_public_path`: Create Draft PR… stands in the review bar and opens Overview at the Status tab, whose page is the publish steps; the unpushed branch: "Not on GitHub", the named push with Copy, steps "Waiting: Push feature/cancel", "Waiting: Create the draft pull request", "Waiting: Publish the tour", no Create offered; after the push "Sent: Push feature/cancel" and What is sent (From, Into, As author, Version, Title, Kept here); a lost answer is the error-role banner with Preview again, nothing resent; a creation held in flight leaves the draft's lane free; released: "Sent: Create the draft pull request", "Waiting: Publish the tour (from the Tour tab)", headline "Draft pull request #7", no second creation. Renders `build/dev/tests/end_to_end/local-draft-publication-desktop/status-*.png` | Pass on fixture |
| On creation | Same course: the window is asked to open github.com acme/engine #7; the composition opens it with `open_review_destination` under the signed-in session | Pass on fixture (the opener); the full path has no course |
| AC03, AC05, AC06 (tour) | Same course: the draft's tour carried onto the pull request's tour draft at its head (anchored, Accepted) and at another head (no lines, kept) | Pass on the application path |

Live, not obtained this round: `live_draft_course.py` was updated to the new
surfaces (a stop in the Tour panel, Create Draft PR… to the Status tab, the
window becoming the pull request with the carried stop) and run twice on
BPR-Test (branches `bpr-0111-live-1794557551`, `-1794557752`); both stopped
before the course reached the draft: the live course's GitHub sign-in stays at
account-setup phase `idle` ("GitHub CLI discovery stopped at idle"), which no
change of this round touches. Nothing was pushed, no pull request was made,
both branches were deleted; `X-RateLimit-Remaining` 4,985 → 4,925. AC01's live
evidence remains #178 and #179 (earlier surfaces). The sign-in fault needs its
own investigation.

## Owner verdicts (attended session 411e2c7497822b381ac2601afe7909f3, handed back 2026-11-13T08:47:51Z)

Session state `/Users/jaeger/.attended-review-state/local-draft-tour-panel-v005-2026-11-13/state`, frozen build from `89156b02d99e` (product as `6887928bd`). Verbatim notes, each the candidate's final decision:

| Candidate | Verdict | Owner's note |
|---|---|---|
| `local-tour-in-tour-panel` | **Reject** (08:46:23Z) | "Seems like a lot of different button styles. I see 3 buttons that say accept and they all look totally different and I'm not sure why." |
| `local-description-in-overview` | **Reject** (08:44:19Z) | "Seems like the other mode's drawers aren't being closed when going to overview." (first worded "Seems like the other drawers aren't being closed when going to overview."; superseded by the owner's edit) |
| `publish-steps-in-status` | **Accept** (08:47:49Z), with a note | "Looks pretty good, but i cant really test it, I'm not sure if those circles are meant to be misaligned vertically with the text beside them." |

Each decision carries a screenshot attachment in the session state. Not fixed in this round (coordinator, 2026-11-13: the next round fixes them). Host 6331/6340 stopped with the attended-review `stop`.

## Live on BPR-Test: sign-in diagnosed, draft pull request created and the tour carried (2026-11-13)

The two live failures above (`GitHub CLI discovery stopped at idle`) were the
course's, not the application's. The account setup driver presses Connect on
the timer tick that first shows gh's accounts and Start reviewing once
connected, so the account step passed between two of the course's polls and
the setup was back at `idle`, signed in. Reproduced with the read-only
`--course reader --pull 1`; instrumented, the setup went `choosing_server`,
`inspecting_cli`, `connecting` and ended connected. Fixed in the course
(`0a85bc2b5`: wait for the outcome; the driver records the offered accounts).
The application's gh sign-in is unaffected; the same defect is in `main`'s
course since `f0dfd707c` (card BPR-2169).

With sign-in working, the live course found a product defect: the window
became the pull request but its tour draft had no stop (run on #182: "local
draft shown no, local words no, tour draft yes, at the link's head yes,
stops 0"). `carry_local_tour_to_pull_request` gave a destination-only stop
Mapping `exact_original` with no `Original`, which `core::apply` refuses as
`invalid_source`, and the composition discarded the refusal. Fixed in
`1237dbefd`; `application_local_draft_publication_desktop_public_path` now
applies the carry as the window does and failed (`invalid_source`) before
the fix.

| Run | Branch | Pull request | Result |
|---|---|---|---|
| reader, before the course fix | none | none | Sign-in "stopped at idle" (reproduced) |
| reader, after `0a85bc2b5` | none | none | Signed in: `cli-account:MikeMarcin (active in gh)`, `cli-account:MikeMarcinTest`, `setup-connects:1`, card `MikeMarcin, GitHub.com, GitHub CLI` |
| live-draft `r2026-11-13a` | `bpr-0111-live-1794559497` | #181, draft, head `1ff4c355db10`, author MikeMarcin | Push and creation pass; the window's tour carry timed out |
| live-draft `r2026-11-13b` | `bpr-0111-live-1794559756` | #182, draft, head `9c2edd941299` | Same; the diagnostic named stops 0 |
| live-draft `r2026-11-13c` (instrumented) | `bpr-0111-live-1794560068` | #183, draft | The carry reached `restore` with one stop at the head, refused |
| live-draft `r2026-11-13d`, at `1237dbefd` | `bpr-0111-live-1794560950` | #184, draft, head `05e1ad3a970e`, base `main`, author MikeMarcin | **Pass**: Check GitHub named the push, the author's Git ran exactly it, Create Draft PR… made #184, the window became #184 and its tour draft holds the carried stop, anchored at the head and Accepted (`live-draft:carried stops=1 anchored=yes accepted=yes`) |

Every pull request was closed and every branch deleted by the course's
`--cleanup` (checked after: no open `bpr-0111-live-*` pull request or
branch). `X-RateLimit-Remaining` 4,877 before the first run; 4,601 after the
last check. Evidence files stay in the session's scratch space (not tracked).

This is AC01's live evidence on the PR-window surfaces (Create Draft PR… to
the Status tab, the window becoming the pull request) and the live half of
"On creation", which had no course before. BPR-2029 is still not claimed: it
depends on BPR-2028, which waits for RFC-BPR-0110's reworked look.

## Publish steps: the marks beside their words (2026-11-13)

The owner accepted the publish steps in session `411e2c74…` with a note: "I'm
not sure if those circles are meant to be misaligned vertically with the text
beside them." They were: a plain step's label had lost height-for-width and
took a two-line hint. Fixed in `b8dca6417`; `qt_review_tour_publication_working`
`test_step_marks_centred` failed before and passes after. Details and the
owner's look: [RFC-BPR-0110 ledger](RFC-BPR-0110.md#rework-of-the-owners-verdicts-2026-11-13).

## AC07's submission clause and closeout (2026-11-13)

The clause "a review submission for another pull request completes while a
creation is in flight" had no course. `application_dependable_session_public_path`
step 2c (`0f444ac0a`) now holds Create Draft PR's own lane in the composed
desktop (`hold_local_draft_publication_lane`, the way a creation waiting on
GitHub holds it) and presses Approve on #18: the submission reaches the
fixture server (`step=second-while-creation-held`, one submission more) and
the lane is released after. The creation runs on that lane, held in flight,
in `application_local_draft_publication_desktop_public_path` (`Holding`), so
together the two courses show the clause on the composed path.

By the coordinator's decision the local-draft-authoring pack's amendment
record now names BPR-2029 and BPR-2031 as card references (`d3e7cb8a8`), so
the cards move without editing it.

Closeout runs at `0f444ac0a` (dev, Debug, macOS): `core_local_draft_publication`,
`core_local_draft_tour`, `local_draft_publication`,
`application_local_draft_desktop_public_path`,
`application_local_draft_tour_desktop_public_path`,
`application_local_draft_publication_desktop_public_path`,
`application_dependable_session_public_path`, `application_composition_source_contract`
and `architecture_boundaries` pass.

| Criterion | Judgment |
|---|---|
| AC01 | Pass: live #184 (and #178, #179); `local_draft_publication` |
| AC02 | Pass: `core_local_draft_publication`, `local_draft_publication` |
| AC03 (amended: tour anchors) | Pass: dirty material refuses, base movement is said, the tour carry anchors only at the candidate head and leaves other heads without lines |
| AC04 | Pass: `local_draft_publication` (lost answers, restart, drift, pre-existing pull request; negative control) |
| AC05 | Pass: a failed follow-up leaves the pull request associated and refuses a new plan; no path writes a description after creation; carrying the tour again creates nothing |
| AC06 | Pass: #184 is an ordinary GitHub draft with its tour carried; the draft stays resumable; GHES and fork heads are stated as unsupported |
| AC07 | Pass: the steps and banner (owner accepted the publish steps, sessions 411e2c74 and a0cbb9dd); unaccepted stops block only the tour; the submission clause above |
