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
| AC07 | `application_local_draft_publication_desktop_public_path` (real window, real composition, real worktree, scripted catalog): Create draft PR shows Sending, then "Sent: Create the draft pull request" and "Sent: Link it to this draft"; a lost answer with nothing found is the error-role banner "The draft pull request was not created." with Preview again; a page edit (a note) is recorded while a creation is held in flight on the publication lane. `core_local_draft_publication`: an unaccepted stop does not block the plan and blocks the tour follow-up. A review submission's independence is by construction (its own per-host lane, RFC-BPR-0105) and has no course with a creation in flight | Pass on fixture except the submission clause, structural only |

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
