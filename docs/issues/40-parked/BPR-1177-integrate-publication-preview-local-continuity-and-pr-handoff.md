---
schema: "implement-rfc.card.v1"
id: "BPR-1177"
title: "Integrate publication preview, local continuity and PR handoff"
status: "Parked"
rfcs: ["RFC-BPR-0059", "RFC-BPR-0061"]
depends_on: ["BPR-1161", "BPR-1176"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-08", "RC59-09", "RC61-08"], "pack_alias": "PACK-CYCLE-35", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"RC59-08","outcome":"pass","revision":"875192071","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 review_cycle_publish_desk: after a squashed publication to a simulated GitHub and a reclaim, across a restart, R1-R3 and every captured byte, the frozen round, reply, decision, note and delivery remain and both links; only explicit removal removes them. application_review_cycle_publish_public_path resumes the cycle from Home with its note and links after Open Pull Request. No live GitHub."}},{"criterion":"RC59-09","outcome":"pending","revision":"875192071","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share passes: association from the journal link (#17); description is the Problem/Solution brief a Review Tour maps; only blob/tree/commit/branch/pull writes; no note, reply or unselected file uploaded; no review or comment written; external link read first and marked externally published; Open Pull Request enters the existing workflow. Pending owner: BPR-1177-promotion-scope-v001 (no note promotion in this build; BPR-1273) in session aed7c9e86b31c6f80225924cf9908d59."}},{"criterion":"RC61-08","outcome":"pending","revision":"875192071","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share passes through the profile desktop (application_review_cycle_publish_public_path) and the host course: P, T and R1-Rn named apart, each path in its own words, the ledger of what goes and what stays, linked pull request named. Pending owner: BPR-1177-publish-sheet-v001 in session aed7c9e86b31c6f80225924cf9908d59 on frozen build d292d8ca (875192071)."}}]
---

Pack alias: **PACK-CYCLE-35** (live `BPR-1177`, allocated at import).

# BPR-1177: Integrate publication preview, local continuity and PR handoff

## Outcome and bounded work

Deliver Publish as PR with actual candidate diff, base distinctions, attribution/title/body and explicit confirmation. Preserve pinned local history after linking. Reuse manual problem/solution and optional ordinary-comment Tour preparation; external publication/linking remains truthful.

## Acceptance and direct evidence

No unselected private history in public payload, R0/R1/R2 remain readable after squash, actual PR association, off-hunk promotion uses inherited policy, external link validation and no auto-tour/comment publication.

Criteria: RC59-08, RC59-09, RC61-08.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Implemented — macOS, 2026-10-04

At `875192071` (commits `75523222f`, `484b97265`, `875192071`; follow-up card
[[BPR-1273]] at `524813aba`). Share notes' menu holds **Publish as Pull
Request…**, a sheet over the code. It keeps the base commit, the target as the
checkout last saw it and the cycle's first and latest captures apart, lists
each path in its own words, names the repository, branches, author, title,
Problem, Solution and draft choice, and keeps a ledger of what goes to GitHub
beside what stays on this computer. Review Plan records the finite plan,
Publish Pull Request confirms exactly its digest, Change drops it. Once linked,
the sheet and the page's Updates line name the pull request, and Open Pull
Request hands it to the Home bar, which opens it in the existing workflow. A
pull request published with one's own tools is linked only after a read of
exactly it and is marked externally published (schema 10). Every revision,
round, reply, decision and delivery stays after publication and a reclaim,
until explicit removal. No live GitHub was contacted. Owned paths, checks,
nine negative controls, the compatibility decision on note promotion and the
limits are in the [execution ledger](../../verification/RFC-BPR-0053-0061.md),
under "BPR-1177: publication preview, local continuity and the pull request
handoff".

RC59-08 passes. RC59-09 and RC61-08 have their automated share and wait for
the owner.

## Pending owner judgment

One Accept/Reject question per criterion in a new session,
`aed7c9e86b31c6f80225924cf9908d59` (state in
`/Users/jaeger/.mask/attended-review/owner-review-20261004-publication-v001`),
both ready at sequence 6:

- `BPR-1177-publish-sheet-v001` (RC61-08): does the sheet keep the baselines
  apart, say what goes to GitHub and what stays, and read clearly enough to
  publish from?
- `BPR-1177-promotion-scope-v001` (RC59-09): is it acceptable that this build
  promotes no local note, with promotion left to [[BPR-1273]]?

Candidate: `/Users/jaeger/bpr1177-publish-review-v001.SLIgWZ`, `git archive`
of `875192071` built in its own `source/build/cand`, executable SHA-256
`d292d8caddd15ec4e80e30e8f21d3a67ffe2eaf5ed389dbb9ffb2d94262bc96a`, launched
with `python3 /Users/jaeger/bpr1177-publish-review-v001.SLIgWZ/launch.py`
(signed out, own profile in `home/`, `HOME` and `CFFIXED_USER_HOME` set). Its
media are that build's course rendered natively on cocoa.

## Owed platform runs

Not gating; the commands are in the ledger section above.

**Note: Linux run at `8fab05419` (station `bpr-main-linux`, WSL2, `build/dev`,
offscreen): 8 of 9 pass; `application_review_cycle_publish_public_path`
fails.** The build succeeded. These pass: `core_review_cycle_association`,
`sqlite_review_cycle_storage`, `review_cycle_publish_desk`,
`review_cycle_publisher`, `application_github_host`, `github_provider`,
`application_review_round_trip_composition_contract` and
`architecture_boundaries`. The course fails the same way three times, in the
selection and twice alone, at about 61 s each:

```text
FAILED: the window opens pull request #17 in the existing workflow: 
```

Every step before it passes: plan, publish, link as #17, link #21 by hand, and
Open Pull Request leaving the local cycle. After that the strip identity stays
empty for 60 s. A temporary, uncommitted diagnostic dumped the window's labels
at that point. The window was on Home, signed out (`serverCardState` "Signed
out", `workbenchStatus` "Not signed in. Public pull requests open
read-only."), with "Sign in to continue to github.com" on the Open field. A
`sharedSignInDialog` titled "Sign in to github.com" was open, so the pull
request never opened. On the same build,
`application_profile_multi_window_public_path`, which also starts
`profile_window_restoration_v1` signed out, passes. Not changed here.

**Note: the Linux failure fixed in the course (station `bpr-main-linux`).** The
cause was course setup that only worked on macOS, not a product bug. Open Pull
Request hands the address to Home while the course is signed out, so
`resolve_setup_destination` opens it only when the github.com profile has
`PreviewAvailable`. That comes from `PublicPreviewComposed`, which is
`shipped_public_preview_capability()`: `Available = true` on `__APPLE__` and
false on Linux and Windows, by design. The course never forced it, so macOS
opened #17 in public preview through the recorded fixture, while Linux (and
Windows) got the `sign_in` stage: "Sign in to continue to github.com" and the
shared sign-in dialog. `application_profile_multi_window_public_path` passed
because it opens its review with an account, not in preview.

The fix is the course setting `options.PublicPreviewEnabledCourse = true`, as
the other composed public-preview courses do (the [[BPR-1042]] precedent in
`personal_discovery_public_path_tests.cpp`). The product and
`github_application.cpp` are unchanged, so the round-trip pin needs no reseal.
The review candidate is unchanged.

Evidence on Linux (`build/dev`, offscreen):

- The course passes 3 of 3 alone (1.68, 1.69, 1.72 s), with `COURSE_NOTE the
  pull request opened: octo-org/review-repository #17 Keep review snapshots
  deterministic`.
- Negative control: the flag set to `false` fails again at "the window opens
  pull request #17 in the existing workflow" (61.2 s). It passes again after
  the revert.
- The BPR-1177 selection passes 9 of 9, including
  `application_review_round_trip_composition_contract` and
  `architecture_boundaries`.
- clang-format 22 and `git diff --check` are clean.

The Mac rerun is owed.

### Windows at `d8fa5bfc2` (bpr-main-win, 2026-10-04)

Windows 11 Home 10.0.26200, MSVC 14.51, Qt 6.11.2, `build/windows-dev` Debug.
`d8fa5bfc2` contains `8fab05419` and the getenv fix `eb775dff4`. The build
printed no `warning C` or `warning LNK` line.

**As registered, 8 of 9.** `application_review_cycle_publish_public_path`
failed contained: exit 3, no output, 0.08 s. Its first filesystem step, an
unchecked `std::filesystem::canonical` of its scratch root, aborts (the
`agent_parent_death` shape). It is now `WINDOWS_UNCONTAINED` and pinned
`dos-volume-name` with that line.

**`review_cycle_publish_desk`, measured contained.** It had been pinned by
shape. Run through the sandbox launcher, it stops at
`FAILED: fixture git command`, before `review_cycle_publisher`'s
`canonical: Access is denied` on `git.exe`. A probe in the same container
shows that cmd and Git both start, and that `git init` under `AC\Temp` exits
128 with `fatal: Invalid path 'C:/Users': Permission denied`. The cause stays
`dos-volume-name`; the pin's comment now gives this measured line.

**Uncontained, a second stop:** `FAILED: a lister catalog`. The course built
its installed-Git publication without `.WindowsSystemRoot`, which the adapter
needs on Windows. Its siblings (`publication_course.hpp`,
`review_cycle_publish_desk`) pass
`tests::git_contract::windows_system_root()`, and the course now does too.
After that fix it passes uncontained in 7.45 s.

**After the pins and the fix, 8 of 9 under `-j4`.**

- `application_review_round_trip_composition_contract` stopped with
  `REVIEW_ROUND_TRIP_DRAFT_STALLED waited_ms=5008 wait_cap_ms=5000`. Alone it
  passes in 11.41 s, as it did twice earlier that night (11.28 s and 12.24 s).
  Only test files changed, so the product binary was the one that passed. This
  is recorded as load against a 5 s cap, not changed.
- `github_provider` timed out once at 30.03 s under `-j4`. It passed at
  28.17 s and 28.14 s in the other `-j4` runs, and in 25.29 s alone, against a
  30 s timeout. Its Windows Debug margin is about 2–5 s.
- The other seven pass: the publish course uncontained in 8.93 s,
  `review_cycle_publish_desk` and `review_cycle_publisher`, and
  `test_network_isolation_manifest` with the new pin.
- clang-format is clean on the changed test.
