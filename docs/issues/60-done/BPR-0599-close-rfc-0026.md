---
schema: "implement-rfc.card.v1"
id: "BPR-0599"
title: "Verify and close RFC-BPR-0026"
status: "Done"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0598","BPR-0987","BPR-0988","BPR-0989","BPR-0990"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0026"]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"planning_reconciliation":"docs/rfcs/plans/ux-redesign-pack-0028/rfc-0026-planning-reconciliation.md"}
acceptance: [{"criterion":"AC-01 surviving part: Home is reachable and opens by the profile/window startup contract; Recently viewed's Resume restores the exact retained reading without destroying other sessions (automatic restoration of other windows and tabs superseded, not passed)","outcome":"pass","revision":"269557cf","evidence":{"kind":"command","detail":"application_profile_window_review_restoration_public_path 563.89 s at the 0d56cee5 product (unchanged since); application_profile_multi_window_public_path, qt_home_list_scale_*, qt_workbench_home_discovery_scale_* in the 35/35 integration pass at 269557cf; qt_composed_github_scale_* Home courses before BPR-0940's stop. Limitation BPR-1017 (new window's sign-in card, RFC-BPR-0024) recorded, not claimed fixed.","path":"docs/issues/60-done/BPR-0599-close-rfc-0026.md#closeout-checklist"}},{"criterion":"AC-02: dedup and explained attention/waiting/quiet; authored-awaiting outside Needs me; unknown detail not quiet","outcome":"pass","revision":"269557cf","evidence":{"kind":"command","detail":"core_review_queue_attention, application_review_queue_enrollment, qt_review_landing_model, qt_home_list_scale_*, application_personal_discovery_public_path pass at 269557cf (#4001 Waiting on others not Needs me; detail_unknown 'Unknown' in Following, never Quiet, never notifies).","path":"docs/issues/60-done/BPR-0599-close-rfc-0026.md#closeout-checklist"}},{"criterion":"AC-03: authored and directly requested discovered in saved repositories without prior PR opening; reviewed and followed retained across request changes and restart","outcome":"pass","revision":"269557cf","evidence":{"kind":"command","detail":"github_read_contract, github_paged_read, github_provider, native_https_transport_contract, gh_cli_transport, application_review_queue_enrollment, sqlite_queue_enrollment_storage, application_personal_discovery_public_path (saved repository searched right after sign-in with no PR opened; #4001/#4003 found unopened) pass at 269557cf.","path":"docs/issues/60-done/BPR-0599-close-rfc-0026.md#closeout-checklist"}},{"criterion":"AC-04: separate ownership of saved repositories, recents, follow and window filters; external open enrolls/saves nothing and mutates no subscription","outcome":"pass","revision":"269557cf","evidence":{"kind":"command","detail":"application_personal_discovery_public_path: #17 opened by link is Recently viewed only until Save Repository/Follow; zero review mutations; application_review_queue_enrollment (forget withdraws only what was found there); qt_workbench_home_discovery_scale_* incl. Home's per-window Filter course; pass at 269557cf.","path":"docs/issues/60-done/BPR-0599-close-rfc-0026.md#closeout-checklist"}},{"criterion":"AC-05: discovery independent of PR loading, bounded, shared across windows; thousands of unrelated PRs need no broad fetch or hydration","outcome":"pass","revision":"269557cf","evidence":{"kind":"command","detail":"application_personal_discovery_public_path over 2,404 open PRs (2,400 unrelated), two windows: only personal searches (one page of 100) and requested-reviewers reads, no list/snapshot/file/diff read; second window shares queue and sentence with no extra request. Limitations recorded: continuation not followed and no narrowing beyond the partial sentence (BPR-0988 design); BPR-1017.","path":"docs/issues/60-done/BPR-0599-close-rfc-0026.md#closeout-checklist"}},{"criterion":"AC-06: limits, incomplete pages, offline, auth failures, removed repositories and stale completions stay honest without cross-account publication or false removal","outcome":"pass","revision":"269557cf","evidence":{"kind":"command","detail":"application_review_queue_enrollment manual-executor courses (limits with retry-after and refused Refresh, partial answers remove nothing, offline/auth stale, forgotten repository publishes nothing, earlier account/generation refused, refused store write rolled back) and github_paged_read/github_provider typed failures pass at 269557cf.","path":"docs/issues/60-done/BPR-0599-close-rfc-0026.md#closeout-checklist"}},{"criterion":"AC-07: canonical PR URLs and supported subpages resolve in the receiving window, reuse an existing PR, navigate exact supported locations","outcome":"pass","revision":"269557cf","evidence":{"kind":"command","detail":"core_setup_destination, application_setup_destination, application_setup_destination_public_path pass at 269557cf (BPR-0987 17e40dc8; pins corrected a9984645).","path":"docs/issues/60-done/BPR-0599-close-rfc-0026.md#closeout-checklist"}},{"criterion":"AC-08: unsupported or unavailable sublocations open the PR with an explanation; malformed or non-PR targets never guess","outcome":"pass","revision":"269557cf","evidence":{"kind":"command","detail":"The same three selectors pass at 269557cf: commit pages, base-side lines, absent files, unread reviews, unidentified and issue comments explained under the bar; malformed/non-PR input sends nothing.","path":"docs/issues/60-done/BPR-0599-close-rfc-0026.md#closeout-checklist"}},{"criterion":"AC-09: unknown-server and signed-out navigation keep the destination, cancel safely, send no credentials to unaccepted authorities","outcome":"pass","revision":"269557cf","evidence":{"kind":"command","detail":"No slice; core_setup_destination, application_setup_destination, application_setup_destination_public_path re-run at 269557cf (BPR-0689, BPR-0732, BPR-0914, BPR-0915).","path":"docs/issues/60-done/BPR-0599-close-rfc-0026.md#closeout-checklist"}},{"criterion":"AC-10: composed Home, filtering, Refresh, Resume, Open PR and onboarding keep keyboard/accessibility/responsive layout without Qt effects or weaker authority","outcome":"pass","revision":"269557cf","evidence":{"kind":"command","detail":"qt_workbench_focus_order_scale_*, qt_workbench_home_discovery_scale_*, qt_home_list_scale_*, core_boundary_contract, core_dependency_boundary, architecture_boundary_contract, github_anonymous_read_contract, application_anonymous_read_host pass at 269557cf; architecture_boundaries on a clean export. No VoiceOver or attended look at the new controls claimed.","path":"docs/issues/60-done/BPR-0599-close-rfc-0026.md#closeout-checklist"}},{"criterion":"Closeout gates: documentation, whitespace, board validation, RFC and index reconciled","outcome":"pass","revision":"269557cf","evidence":{"kind":"command","detail":"verify_documentation.py DOCUMENTATION_OK (build/bpr0599-docs.log) with 64 self-tests OK; git diff --check clean; issue_board validate --rfc RFC-BPR-0026 no errors over 1009 cards; RFC-BPR-0026 front matter and index row Auto-Verified for the reconciled scope.","path":"docs/issues/60-done/BPR-0599-close-rfc-0026.md#closeout-checklist"}}]
---

# Verify and close RFC-BPR-0026

## Objective

Reconcile the complete accepted behavior in [RFC-BPR-0026](../../rfcs/RFC-BPR-0026-personal-review-home-pr-url-navigation.md)
against direct evidence, all linked issue cards, and the final public path.
This closeout depends on the entry card now and must acquire all subsequently
allocated required slice dependencies before it can become startable.

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 4 of 4.
Planned cards: BPR-0598, BPR-0599, BPR-0987, BPR-0988, BPR-0989, BPR-0990 (allocated
by the 2026-09-22 planning reconciliation); BPR-0801 was answered by BPR-0990 and
BPR-1017 recorded as a finding outside this RFC. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

**Design accepted; registration only; implementation requires a later request.**
This unstarted backlog card records the 2026-09-04 registration, not an execution
grant or completed product work. The immutable base is the pre-registration
commit for this approved workstream; registration commits after it are planning
history, not evidence of interrupted product implementation.

## Closeout checklist

Planning reconciliation on 2026-09-22 ([record](../../rfcs/plans/ux-redesign-pack-0028/rfc-0026-planning-reconciliation.md#criterion-map)) allocated the four
slices below. The owner-locked RFC-BPR-0028 direction and the owner's
2026-09-11 Home answers supersede this RFC's Home presentation where they
conflict; each row names the surviving check or the superseding decision, and
a superseded part is recorded as such, never as passed. Closeout head
`269557cf`; product identical to `0d56cee5` (later commits change only test
courses and cards). "Integration pass" is the serial run of the 35 selectors in
`build/bpr0599-selector.txt` at `269557cf`: 35/35 in 297.03 s
(`build/bpr0599-integration.log`).

| Criterion | Executed direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: Home is reachable and opens according to the profile/window startup contract, with Resume restoring the exact retained review/tab without destroying other sessions. | `application_profile_window_review_restoration_public_path` (563.89 s at the `0d56cee5` product); `application_profile_multi_window_public_path`, `qt_home_list_scale_100/125/200`, `qt_workbench_home_discovery_scale_100/125/200` (integration pass); `qt_composed_github_scale_100/125/200` Home courses before [[BPR-0940]]'s stop | Surviving part pass. Home is the strip's first button (⌘0) and the window's ground at startup; Recently viewed's Resume restores the exact retained reading without touching sibling windows. Superseded, not passed: automatic restoration of other windows' reviews at startup (owner's 2026-09-11 direction, [BPR-0733](BPR-0733-home-lists-recently-viewed-pull-requests-instead-of-restoring-the-last-one.md)) and "tab" (RFC-BPR-0028: one review per window). Limitation, not claimed fixed: a new window of a signed-in profile presents the sign-in card until its first read ([[BPR-1017]], RFC-BPR-0024) | `0d56cee5`; earlier BPR-0733, BPR-0987 |
| AC-02: Home deduplicates PRs and explains attention, waiting, and quiet states; authored PRs merely awaiting another reviewer do not inflate the user's attention count, and unknown detail is not presented as quiet. | `core_review_queue_attention`, `application_review_queue_enrollment`, `qt_review_landing_model`, `qt_home_list_scale_*`, `application_personal_discovery_public_path` (integration pass) | Pass. One record per pull request carries every reason (role, rule, authored, requested, followed, reviewed); rows end in one reason word and a why sentence; an authored pull request awaiting reviewers is in Waiting on others, never Needs me (#4001 in the composed course); a pull request nothing has been read about is `detail unknown` ("Unknown", in Following), never Quiet, and never notifies | `b34542ed`, `0d56cee5` |
| AC-03: Authored and directly requested PRs are discovered in saved repositories without prior PR opening, while previously reviewed and explicitly followed PRs retain meaningful membership across request changes and restart. | `github_read_contract`, `github_paged_read`, `github_provider`, `native_https_transport_contract`, `gh_cli_transport`; `application_review_queue_enrollment`; `sqlite_queue_enrollment_storage`; `application_personal_discovery_public_path` (integration pass) | Pass. The composed course saves a repository right after sign-in and reads exactly its two personal searches with no pull request opened; #4001 (authored) and #4003 (requested by name) are found without being opened, a team-only request is not the viewer's. A followed or reviewed pull request stays when its request disappears; a complete answer alone withdraws a reason; the reasons persist (schema 7, migrated in place) and are restored with the queue | `8d1898e7`, `b34542ed`, `0d56cee5`, `80adc298` |
| AC-04: Saved repositories, recents, follow membership, and window filters preserve their separate ownership, and external PR opening does not silently enroll its repository or mutate GitHub subscriptions. | `application_personal_discovery_public_path`; `application_review_queue_enrollment`; `qt_workbench_home_discovery_scale_*` (with Home's per-window Filter course); `application_profile_multi_window_public_path` (integration pass) | Pass. #17 opened by link is Recently viewed only: not enrolled, its repository not saved, nothing searched, until the Pull Request menu's Save Repository or Follow; each keeps its own store (queue saved repositories, enrollment follow flag, restoration recents, per-window Home view). Forgetting a repository withdraws only what was found there. Zero review mutations; no request reaches a subscription or any mutation endpoint | `0d56cee5`, `269557cf` |
| AC-05: Discovery is independent of PR loading, paginated, bounded, and shared across profile windows; a fixture with thousands of unrelated open PRs requires no broad all-PR fetch or full-review hydration. | `application_personal_discovery_public_path` (2,404 open pull requests, 2,400 unrelated; two windows); `github_paged_read`; `application_review_queue_enrollment` (integration pass) | Pass. Every discovery read is a personal search (one page of 100) or a requested-reviewers read (at most 30 per answer); no list, snapshot, file or diff read; a longer or incomplete answer is reported partial, never complete. One scheduler per profile, two reads in flight: the second window shows the same queue and sentence and adds no request, and its Refresh runs the same bounded reads. Limitations: the continuation of a longer search is not followed and no narrowing is offered beyond the partial sentence ([[BPR-0988]]'s recorded design); the second window needs its first read to present its session ([[BPR-1017]]) | `8d1898e7`, `b34542ed`, `0d56cee5` |
| AC-06: Rate limits, incomplete pages, offline state, auth failures, removed repositories, and stale completions preserve honest availability and cached results without cross-profile/account publication or false membership removal. | `application_review_queue_enrollment` (manual executor); `github_paged_read`, `github_provider` (integration pass) | Pass. Primary and secondary limits honour retry-after (60 s default) and refuse Refresh inside it, saying how long; incomplete, over-limit and detail-limited answers are partial and remove nothing; offline and authentication failures are stale and named; a repository forgotten mid-flight publishes nothing; an earlier account, generation or refresh is refused by the host and again by the queue; cached rows stay through every failure; a refused store write is rolled back and reported | `8d1898e7`, `b34542ed`, `0d56cee5` |
| AC-07: Canonical PR URLs and supported PR subpages resolve in the receiving profile/window, reuse an existing PR there, and navigate exact supported locations without manual owner/repository/number entry. | `core_setup_destination`, `application_setup_destination`, `application_setup_destination_public_path` (integration pass) | Pass. Files, a right-side line under the full comparison, a review and (where the read carries its identity) a review comment are placed in the receiving window; a link to the pull request the window shows reads nothing again | `17e40dc8`, pins `a9984645` |
| AC-08: Unsupported or unavailable sublocations open the resolved PR with an explanation, while malformed or non-PR targets never guess a review, line, or revision. | The same three selectors (integration pass) | Pass. Commit pages, base-side lines, absent files, unread reviews, unidentified comments and issue comments open the pull request and say why under the bar; malformed and non-PR input sends nothing | `17e40dc8` |
| AC-09: Unknown-server and signed-out navigation preserve the pending destination through explicit server setup and supported sign-in, cancel safely, and send no credentials to unaccepted authorities. | No slice. `core_setup_destination`, `application_setup_destination`, `application_setup_destination_public_path` (integration pass) | Pass: [[BPR-0689]], [[BPR-0732]], [[BPR-0914]], [[BPR-0915]], re-run at the closeout head | BPR-0689, BPR-0732, BPR-0914, BPR-0915; rerun `269557cf` |
| AC-10: The composed Home, filtering, Refresh, Resume, Open PR, and onboarding flows preserve keyboard/accessibility and responsive layout behavior without adding Qt effects or weakening existing review/preview authority. | `qt_workbench_focus_order_scale_100/125/200`, `qt_workbench_home_discovery_scale_100/125/200`, `qt_home_list_scale_100/125/200`, `core_boundary_contract`, `core_dependency_boundary`, `architecture_boundary_contract`, `github_anonymous_read_contract`, `application_anonymous_read_host` (integration pass); `architecture_boundaries` on a clean export | Pass. Home's refresh glyph is a named, described focus stop after Filter at every scale; the sentence is a labelled text; Follow and Save Repository are keyboard-reachable menu commands; existing Home, Filter, Resume, Open and onboarding flows keep their owner-accepted RFC-BPR-0032 A5–A8 behaviour. Qt dispatches typed intents only; discovery refuses a public-preview authority. Not claimed: no VoiceOver or attended look at the new controls | `0d56cee5`, `269557cf` |

## Completion rules

- Replace planned checks with exact executed commands/courses, results, and
  producing commits as implementation completes; retain not-run rows honestly.
- Inspect all cards linked to this RFC, not just the initial planned pair.
  Every required card must be done and every canonical criterion satisfied.
- Exercise the selected public path and check critical changed-surface findings.
  Use focused evidence; do not infer a full matrix requirement from this card.
- Record target-native or attended evidence in the project's existing workflow.
  Park pending attended work and continue independent work; no approval or live
  GitHub authority comes from registration.
- Run the documentation structure gate and `git diff --check`, reconcile RFC
  and index status, and close only when canonical order and all dependencies
  permit Auto-Verified. No status changes are authorized by this registration.

## Resume

The owner activated the ordered workstream in the canonical index. RFC-BPR-0018,
RFC-BPR-0024 and RFC-BPR-0025 are Auto-Verified; [[BPR-0598]] recorded the
2026-09-22 planning reconciliation and allocated [[BPR-0987]], [[BPR-0988]],
[[BPR-0989]] and [[BPR-0990]], which this closeout now depends on.
[BPR-0598](BPR-0598-inventory-personal-discovery-url-flow-and-plan.md) reconciled
RFC-0028's superseding Home presentation, the owner's 2026-09-11 Home
direction and the existing URL/setup behavior before planning new authority
([record](../../rfcs/plans/ux-redesign-pack-0028/rfc-0026-planning-reconciliation.md)).
This closeout became startable when the four slices were Done; it re-ran the
no-slice rows at the closeout head, recorded superseded parts as superseded,
and reconciled the RFC and index (below).

## Closeout on main — 2026-09-23

Run `main-closeout-20260922`, segment `rfc-0026-implementer`, milestone
`close-rfc-0026` (bound in `build/bpr0599-bind.json`; obligations BPR-0987,
BPR-0988, BPR-0989, BPR-0990 and this card; both exits unchanged). Sole
non-delegating `rfc-implementer` on `main` under the coordinator-implementer
recipe; nothing pushed. Another writer committed `684858ef` (review-tour RFC
import) on `main` during the run; it touches none of this RFC's paths.

- **Slices.** [[BPR-0987]] Done `87635481` (exact link locations; its
  composition pins corrected at `a9984645`, because they had been taken before
  its last edit), [[BPR-0988]] Done `aed3cb34` (personal discovery read),
  [[BPR-0989]] Done `38651c6b` (scheduler and honest membership), [[BPR-0990]]
  Done `aef2c455` (composition, Home refresh, status, Follow and Save
  Repository). [[BPR-0801]] is finished from BPR-0990's evidence (`cea68f70`)
  and [[BPR-0781]]'s A7 row points at it. The closeout strengthened two test
  courses only: the composed discovery course now searches a saved repository
  before any pull request is opened (`80adc298`) and the home-discovery
  selector runs Home's per-window Filter course (`269557cf`).
- **Integration pass** at `269557cf`: 35/35 serial in 297.03 s
  (`build/bpr0599-integration.log`, selectors `build/bpr0599-selector.txt`).
  The restoration course is reused from the `0d56cee5` product (563.89 s): no
  product or course input changed after it. Known stops unchanged:
  `qt_composed_github_scale_100/125/200` all stop at [[BPR-0940]]'s
  "with GitHub CLI chosen the card speaks for it…" (`build/bpr0599-known-stops.log`),
  after their Home courses; `qt_anonymous_public_preview_scale_100` at the
  known Connect assertion; `application_composition_contract` on [[BPR-0983]]'s
  three.
- **Negative controls** (each restored and rebuilt): BPR-0987 file, reuse and
  Recently viewed; BPR-0988 author, direct-request and native drift; BPR-0989
  partial, stale generation and store rollback; BPR-0990 forget withdrawal,
  glyph, arrival activation and the composed reconcile, which now fails "a
  saved repository is searched with no pull request opened"
  (`build/bpr0990-negative-reconcile.log`). Each fails its named assertion.
- **Linked cards.** Fourteen cards name RFC-BPR-0026; the thirteen others are
  Done (`issue_board queue --rfc RFC-BPR-0026`: 14 selected, 13 done).
- **Findings recorded, not fixed here.** [[BPR-1017]] (RFC-BPR-0024): a new
  window of a signed-in profile presents the sign-in card until its first
  read. The discovery search does not follow a continuation and offers no
  narrowing beyond the partial sentence ([[BPR-0988]]'s design).
- **Not claimed.** No attended, VoiceOver, other-platform or live-GitHub
  evidence was taken: the criteria require none, and the RFC's subjective UX
  (the new glyph meaning, sentence and Pull Request menu) has had no owner
  look. The BPR-0988 request and authority change was self-reviewed only; an
  independent critical review is recommended.

## Current checkpoint

- Closeout head `269557cf`; `main`, not pushed.
- RFC-BPR-0026 front matter and index row reconciled to Auto-Verified for the
  reconciled scope; gates recorded with the finish.
- Pending obligations inside this RFC's scope: none. Outside it:
  [[BPR-1017]], [[BPR-0940]], [[BPR-0983]], and an optional owner look at the
  new Home controls through the existing attended-review workflow.
- Next action: none for this milestone; RFC-BPR-0026 is the last RFC of the
  requested workstream.
