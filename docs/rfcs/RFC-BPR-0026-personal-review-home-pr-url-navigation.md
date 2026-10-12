---
id: "RFC-BPR-0026"
title: "Personal Review Home and PR URL Navigation"
status: "Auto-Verified"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0002","RFC-BPR-0007","RFC-BPR-0010","RFC-BPR-0016","RFC-BPR-0018","RFC-BPR-0024","RFC-BPR-0025"]
pack_version: "post-v2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# RFC-BPR-0026: Personal Review Home and PR URL Navigation

## Status and scope

**Design accepted; closed on 2026-09-23 for the scope reconciled under
RFC-BPR-0028 and the owner's 2026-09-11 Home direction.**
The product owner approved the daily-review Home and URL workflow on
2026-09-04. This post-pack RFC is last in the requested sequence
RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026. It changes no existing
eligibility, dependency-order, platform, credential, or mutation requirement.

Registration baseline: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Entry: [[BPR-0598]] records the 2026-09-22 planning reconciliation below.
Closeout: [[BPR-0599]] records direct evidence for every criterion after the
four allocated slices, [[BPR-0987]], [[BPR-0988]], [[BPR-0989]] and
[[BPR-0990]], were Done; the superseded parts of AC-01 (automatic restoration
of other windows at startup, and tabs) are recorded as superseded, not passed. The
owner's ordered-pack implementation mandate and the planning amendment are
recorded in the
[2026-09-22 reconciliation](plans/ux-redesign-pack-0028/rfc-0026-planning-reconciliation.md);
the prior registration-only restriction is superseded by that mandate, not by
an inferred waiver. The immutable baseline and all ten criteria are unchanged.

## Planning reconciliation — 2026-09-22

[RFC-BPR-0028](RFC-BPR-0028-review-workbench-direction.md), owner-locked on
2026-09-06, supersedes this RFC's Home presentation where they conflict: Home
is a view with its button and the grouped action list, and browser-style
tabs stay unscheduled, so "tab" below reads as the receiving window's one
review. The owner's 2026-09-11 Home direction ([[BPR-0732]], [[BPR-0733]],
accepted in an attended session) makes the signed-out Home one paste field
and replaces automatic restoration at startup with a per-profile Recently
viewed list whose entries resume their saved reading; that list's Resume is
this RFC's Resume action, and no window restores a review by itself.
[RFC-BPR-0032](RFC-BPR-0032-calm-home-and-accounts-surfaces.md) arranges
Home's panels and leaves personal discovery to this RFC. Planning removes,
weakens or accepts no criterion: the closeout records, per row, direct
evidence at the closeout head or the superseding decision, and a superseded
part is never recorded as passed.

| Criterion | Reconciled disposition |
|---|---|
| AC-01 | No slice. Home's button, Recently viewed and Resume exist; automatic restoration of other windows is superseded by the owner's 2026-09-11 direction and tabs by RFC-0028. Re-verified at closeout. |
| AC-02 | Presentation shipped under RFC-0028/0032; discovery-backed roles and awaited counts ([[BPR-0988]]), the unknown-detail state ([[BPR-0989]]) and its rendered reason ([[BPR-0990]]). |
| AC-03 | [[BPR-0988]] reads authored and directly requested pull requests of saved repositories; [[BPR-0989]] keeps reviewed and followed membership across request changes and restart. |
| AC-04 | Saved repositories, recents and per-window filters exist; explicit local Follow ([[BPR-0989]], [[BPR-0990]]) and Save repository for an externally opened pull request ([[BPR-0990]]). |
| AC-05 | Bounded read without hydration ([[BPR-0988]]), one scheduler per profile ([[BPR-0989]]), and the composed thousands-of-pull-requests, two-window course ([[BPR-0990]]). |
| AC-06 | Typed provider failures ([[BPR-0988]]) and honest cached availability with generation rejection ([[BPR-0989]]). |
| AC-07 | Typed exact locations and reuse of the open pull request in the receiving window ([[BPR-0987]]). |
| AC-08 | Per-location explanations and refusals ([[BPR-0987]]) over the existing malformed/non-PR refusal. |
| AC-09 | No slice: [[BPR-0689]], [[BPR-0732]], [[BPR-0914]] and [[BPR-0915]]. Re-verified at closeout. |
| AC-10 | Existing Home, Open and onboarding flows are accepted under RFC-0032 A5–A8; the new controls are covered by [[BPR-0990]]. |

## Problem and outcome

Repositories with thousands of in-flight PRs cannot be a useful default review
list. Reviewers usually return to a small saved set, work on their own PRs or
direct review requests, and open occasional links from Slack, Jira, or a browser.
They need to discover relevant work without first loading an arbitrary PR or
reentering owner/repository/number fields.

At the registration baseline, `github_paged_read.cpp` performs a repository
search with `SearchTerms = "is:open"` as part of a loaded PR snapshot. This RFC
separates personal discovery from that path rather than merely filtering a
downloaded list of all open PRs. Re-audit the live provider before implementation.

## Home and attention

Home is always reachable within a window. On profile opening, its last-focused
window shows Home with a prominent **Resume review** action bound to the exact
retained PR/tab destination from RFC-BPR-0024. Other restored windows keep their
views; new windows open Home. Home selection never destroys those retained tabs.

Lead with **Needs your attention**, then **Waiting on others**, then collapsible
quiet items. Offer **My PRs**, **Requested**, and **Following** filters. Each PR
appears once, with repository, number, title, state, and a concrete reason such
as review requested, changed since your review, unseen discussion, or fixes
awaiting verification. State and reason do not depend on color alone.

Extend RFC-BPR-0010's core-owned attention projection to separate action by the
current user from waiting on another person. A PR the user authored that only
awaits another reviewer belongs in Waiting on others and contributes no
Needs your attention count. Preserve existing exact ledger observations and
stable core ordering within those sections; Qt does not invent urgency from
timestamps or unread-looking badges. Initial discovery alone does not prove
new commits or unseen comments. Unknown detail is labeled, not treated as quiet.

An open review and a queue item are distinct. Clearing attention does not close
the PR; closing its view does not delete its queue membership. Closed/merged
PRs leave active discovery with a reason while already-open windows retain
their stateful views until explicitly closed. Access loss, withdrawal, and
temporary refresh failure remain distinct, following the existing queue domain.

## Saved repositories and discovery

Saved repositories belong to the BPR profile and survive restart. Their identity
includes the configured server and accepted account authority. Display host
context where repository names collide. Adding/removing a saved repository
changes local discovery only, never GitHub watch/subscription or assignment.

Automatically search only the saved repositories for open authored PRs and
direct review requests, using the authenticated viewer on each configured
server. Union results by exact PR identity; preserve why the PR was discovered.
Retain already-reviewed enrollments for later activity even after the direct
request disappears. Failed or incomplete searches cannot establish removal.
Opening an external PR records a recent entry without silently saving its repo;
offer **Save repository** and explicit local **Follow** for continued tracking.

GitHub documents author and direct-review-request search qualifiers and a
paginated search API. Use the server contract's supported equivalents with the
resolved viewer identity. Validate GitHub.com and supported GHES contracts
through fixtures; do not assume a newer web-only qualifier exists on GHES.
Author results must be normalized against actual author identity so search
expansions do not silently relabel another actor as the current user.

- [GitHub issue and PR search qualifiers](https://docs.github.com/en/search-github/searching-on-github/searching-issues-and-pull-requests)
- [GitHub filtering and direct review requests](https://docs.github.com/en/issues/tracking-your-work-with-issues/using-issues/filtering-and-searching-issues-and-pull-requests)
- [GitHub REST search limits and incomplete results](https://docs.github.com/en/rest/search/search)

The discovery interface returns bounded metadata, reason, continuation, and
completion/availability facts independently of full PR snapshots. Opening a PR
does not require broad discovery to succeed. Discovery never fetches every
candidate's full diff, history, conversations, or changed-file bodies. Obtain
only bounded additional facts needed to resolve displayed attention, through
existing provider/ledger paths; defer heavy review material until requested.

One scheduler per profile shares discovery among its windows, partitions work
by server/account, refreshes on activation subject to freshness/backoff, and
provides an explicit Refresh action. Use existing conditional requests where
supported and honor rate limits, cancellation, offline state, and bounded
pagination. Show last successful refresh and partial/stale/error status per
affected scope, preserving cached rows. Offer continuation or narrowing when
limits are reached; never claim a partial list is complete. Loading or changing
an active file/tab must not restart profile-wide polling.

## URL opening and onboarding

Provide an always-reachable **Open PR** action with a pasteable URL field and
keyboard access. No owner/repository/number entry is needed for a valid PR URL.
Pasted text is explicit navigation input; this feature does not monitor the
clipboard or integrate Slack/Jira accounts.

Resolve a supported HTTPS PR URL into server, owner/repository, PR number, and
an optional location. Recognize the canonical PR, `/files`, `/commits`, a PR
commit subpage, and supported comment/file fragments. Unrelated query parameters
do not become provider request or credential-routing instructions. A repository
commit URL with no PR identity must not guess a PR.

Navigation always uses the receiving window's BPR profile. Activate the PR if
already open there, otherwise open it there using RFC-BPR-0025's tab/session
intents. Without a location, preserve the PR's retained active view. An exact
location navigates within its active tab through the same file/comparison
authority as an ordinary gesture; it creates the initial tab when necessary.

Map a file/commit/comment location only when accepted provider and retained
revision facts establish its exact target. If the PR resolves but its specific
location cannot, open the PR with a clear location-unavailable explanation;
never silently pick a nearby line or a different revision. Do not promise
historical comment mapping that depends on unimplemented RFC-BPR-0021. Preserve
the unresolved requested location as context for the explanation.

For an unconfigured enterprise server, show the parsed hostname with **Add
server and continue**. No unknown authority receives credentials or becomes
configured merely because a URL was parsed. This explicit action hands the
pending target to RFC-BPR-0018's server flow. A configured but signed-out server
offers direct sign-in and resumes the exact pending target only after accepted
authentication. Cancellation keeps the existing review usable. Account/profile
changes invalidate obsolete pending navigation rather than retargeting it.

GitHub.com onboarding exposes direct device sign-in through RFC-BPR-0016;
unsupported device flows, including the current GHES contract, retain the
supported manual-token path. Existing public-preview availability remains
explicit; a failed authenticated request never falls back to anonymous access.
Without viewer identity, personal discovery is unavailable with a sign-in action.

## Architecture, persistence, and interfaces

Core owns personal membership/attention decisions and typed navigation targets,
legality, and acceptance. Application hosts own URL normalization/validation,
discovery scheduling, provider observations, pending navigation, persistence,
and serialization. Provider URL construction remains in the narrow adapter;
Qt accepts text and presents results, never acquiring `QUrl`, a provider, vault,
network, or browser-opening effect.

Extend existing queue membership/storage with explicit discovery/follow reasons
without treating a saved repo as a watch-all observer rule. Store saved repos,
recents, and enrollment facts under RFC-BPR-0024's profile authority. Migrate
existing queue preferences and observer memberships intact. Window filters and
Resume destinations retain their window ownership; do not persist a derived
filtered queue as canonical truth.

Tag work by profile, server/account generation, query/page, and navigation
request identity. A stale result cannot reopen another window or revive a
removed repository. Storage failure reports precisely which updates are
session-only or unavailable; it never falls back to another profile's cache.

## Non-goals

Repository-wide all-PR dashboards, automatic mention or team-request discovery,
automatic discovery outside saved repositories, Slack/Jira integrations,
OS-wide URL-scheme registration, clipboard monitoring, cloud sync, new desktop
notification behavior, new publishing permissions, and guessed historical
anchors are outside this initial scope. The settings, profiles, and tab RFCs
retain their own ownership; this RFC composes their existing interfaces.

## Acceptance criteria

- AC-01: Home is reachable and opens according to the profile/window startup contract, with Resume restoring the exact retained review/tab without destroying other sessions.
- AC-02: Home deduplicates PRs and explains attention, waiting, and quiet states; authored PRs merely awaiting another reviewer do not inflate the user's attention count, and unknown detail is not presented as quiet.
- AC-03: Authored and directly requested PRs are discovered in saved repositories without prior PR opening, while previously reviewed and explicitly followed PRs retain meaningful membership across request changes and restart.
- AC-04: Saved repositories, recents, follow membership, and window filters preserve their separate ownership, and external PR opening does not silently enroll its repository or mutate GitHub subscriptions.
- AC-05: Discovery is independent of PR loading, paginated, bounded, and shared across profile windows; a fixture with thousands of unrelated open PRs requires no broad all-PR fetch or full-review hydration.
- AC-06: Rate limits, incomplete pages, offline state, auth failures, removed repositories, and stale completions preserve honest availability and cached results without cross-profile/account publication or false membership removal.
- AC-07: Canonical PR URLs and supported PR subpages resolve in the receiving profile/window, reuse an existing PR there, and navigate exact supported locations without manual owner/repository/number entry.
- AC-08: Unsupported or unavailable sublocations open the resolved PR with an explanation, while malformed or non-PR targets never guess a review, line, or revision.
- AC-09: Unknown-server and signed-out navigation preserve the pending destination through explicit server setup and supported sign-in, cancel safely, and send no credentials to unaccepted authorities.
- AC-10: The composed Home, filtering, Refresh, Resume, Open PR, and onboarding flows preserve keyboard/accessibility and responsive layout behavior without adding Qt effects or weakening existing review/preview authority.

## Direct verification scenarios

| Criteria | Direct surface and discriminating check |
|---|---|
| AC-01, AC-10 | Public application with retained windows: open profile, Resume, return Home, create another window, and traverse all primary actions by keyboard at narrow sizes and 100–200% scale. |
| AC-02, AC-03 | Deterministic queue fixtures: author waiting, first review request, changed reviewed head, unseen discussion, duplicate roles, missing detail, request disappearance, and closed/merged/access-lost transitions. Inspect both decided sections and rendered reasons/counts. |
| AC-04 | Saved-repository/follow gestures through profile store and restart: open a one-off external PR, save/remove its repo, follow/unfollow it, change one window's filter, and assert other windows' state plus zero subscription mutations. |
| AC-05 | Recording provider facade with thousands of unrelated PRs: verify personal query scope, bounded pages and detail requests, independent PR loading under discovery failure, and no duplicate scheduler work for two windows. |
| AC-06 | Manual executor and persisted cached rows: inject primary/secondary limits, partial search, page failure, offline, sign-out, repo removal, and out-of-order completions; verify truthful retained rows and exact generation rejection. |
| AC-07, AC-08 | Table-driven URL normalization plus real Open PR gestures: canonical, files, commits, PR commit, comment/file fragments, duplicate PR, extra query parameters, unavailable anchor, malformed URL, and non-PR commit URL. Assert exact selected target or explicit refusal/fallback. |
| AC-09 | Recording transport/vault and composed setup: paste unknown enterprise host, accept or cancel setup, sign in or cancel, change profile/account mid-flow, and verify destination retention plus zero wrong-authority requests. |

Record exact focused selectors and producing commits in the closeout card when
implemented. All product evidence is not run at registration. A rendered
application course and existing attended-review workflow cover subjective UX;
live authentication or GitHub mutations require their existing separate authority.

## Implementation entry and documentation impact

[[BPR-0598]] inventories provider search, queue attention/enrollment, server
capabilities, and URL-to-workspace routing before allocating bounded slices.
[[BPR-0599]] anchors acceptance. Later implementation updates the queue, startup,
URL-opening, and onboarding guides and revises only the affected architecture
contracts. Until then, the UX specification links this document as future design.

## Adopted authored Home amendment — 2026-10-11

[RFC-BPR-0066](RFC-BPR-0066-home-authored-pull-requests.md) supersedes this RFC's Home and attention and AC-02 where they use attention alone for Home placement. Accepted author-role or authored-discovery membership is scoped to each exact host/account/PR row. Authored rows whose current bucket is waiting-only remain in Waiting on others (the owner's settled Q1); all other authored rows belong to My pull requests and never appear or count in Needs me. The order is Needs me → My pull requests → Waiting on others → Following → Recently viewed, inside every named group and with headers hidden. Attention, unread facts, notification eligibility and repository-drawer counts remain unchanged.

My pull requests has a neutral header, existing state-colored row anatomy and a collapsed count/actionable/unread summary with unknown detail preserved. Its independent top-level collapse starts expanded on upgrade and persists per window; named group and band collapse remains session-only. Filters/counts, selection/opening authority, Recently viewed, the adopted onboarding correction and the later stable Home layout remain intact. The core owns the new deterministic section decision; the host persists its view state; Qt presents it. Historical Done cards and plan exits retain their exact evidence, and RFC0032 A8 human/native obligations are neither accepted nor waived by this amendment. Fresh changed-surface evidence belongs to BPR-1388.

## Adopted draft Home amendment — 2026-10-11

[RFC-BPR-0067](RFC-BPR-0067-home-draft-pull-requests.md) extends Home row anatomy with a compact neutral outlined Draft chip before identity/title and draft-first accessible wording. It carries last accepted exact-authority metadata through bounded discovery, retained reads and recent history; unknown remains unknown and displays no chip. Accepted ready changes remove the chip, including matching queue/recent rows. Attention colours, sections, counts, enrollment, row actions and native controls retain their contracts. No additional request or full PR hydration is introduced. Historical Done cards, plan exits and RFC0032 A8 human/native obligations are unchanged; fresh evidence belongs to BPR-1389.
