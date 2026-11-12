# RFC-BPR-0108 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `local-draft-entry`
([[BPR-2019]], [[BPR-2020]], [[BPR-2021]], closeout [[BPR-2022]]).
RFC_BASE_SHA: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`. Product candidate:
`3585dedc7`. Frontend-design was loaded before the Qt surface work. Owner
amendments in force: "If you need it you need it go build it."; "main should
be the source of truth for the tracker"; "Continue working unattended, you can
file reviews but dont block on their results". Debug timings are nongating;
nothing below is a timing claim.

## What was built

| Layer | What |
|---|---|
| Core (`core/local_draft.hpp`) | Draft identity; the remote-chain walk (`step_remote_chain`: hosted remotes end it, one local remote is followed, two are refused, a revisit is a cycle, a fifth hop is refused); target choice from the reached clone's remote default branches (never the branch's upstream, never `origin` among several repositories, never an assumed `main`); the comparison from `merge-base --all` (one base, none, several); the committed change; the candidate's changes with only selected working files; worktree resolution by Git-directory identity and branch (`same`, `branch_switched`, `moved`, `other`); bounded, versioned transitions (candidates, notes, title, target, rebind, association); In preparation and the pull request mark; the stored JSON (glaze) |
| Git adapter | `make_installed_git_local_draft_reader`: the publication catalog's isolated store, read-only: `merge-base --all`, `ls-tree`, `cat-file blob`, with the worktree's objects and each chain clone's as alternates. No config, hook, ref or index of the worktree is read by Git; nothing is fetched or written |
| Application | `local_draft_discovery` (refs, config, index and working files as data through the workspace port; the scan hashes working files as Git would and lists edits apart; `capture_local_draft` reads both sides of every change and re-reads working bytes against what was listed); `open_local_draft` (the one open both the desktop and `bpr draft open` make); `local_draft_workbench` (host, lanes, projection) |
| SQLite | `state/v1/local-drafts.sqlite3`: versioned draft records, content-addressed objects, pins; a write names the version it read; 1 GiB and 64-draft bounds refuse with the amounts; discard removes the record and objects only it named |
| Qt | `local_draft_view` (title, source-into-target identity with exact commits in its tooltip, Candidate chooser, Refresh, Discard…, Close; Changes, Not committed, Notes, the diff); Home's author entry (Prepare Pull Request…, In preparation) above Home's sections; Review Cycles moved below them; the prepared chip and Open Local Preparation on a pull request row |
| App | `local_draft_desktop` in every window; `bpr draft open [PATH] [--context TEXT]`, run here under the profile's lease or forwarded to the owning desktop (owner channel operation `local_draft`); the attended look `--attended-local-draft PATH` |

Thresholds (AC07 asks for them to be recorded): 64 drafts, 32 candidates per
draft, 4096 changed paths and 512 included working files per candidate, 16 MiB
per kept file (a larger side is named and its bytes are not kept), 256 MiB per
candidate, 1 GiB in the store, 512 notes of 16 KiB, four local-remote hops; a
working-tree scan stops at 200,000 entries or 2 GiB read and says so.

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `local_draft` (real repositories by the build's Git, the product's workspace source, Git reader and store through `open_local_draft`): a primary clone whose `main` moved after the branch opens with source `feature/search-results`, target `acme/engine main` from `origin/HEAD`, the merge base recorded apart from the target tip, only the branch's two changes, both sides' exact bytes kept, and HEAD, index, `git status` and every ref byte-identical; a linked worktree and an isolated `clone --shared` clone open the same way. `application_local_draft_desktop_public_path` (real window and composition): Home's Prepare Pull Request…, signed out, to the draft page with "feature/search-results into acme/engine main", the title row's Local draft badge, the exact diff on the first screen, checkout unchanged. The shipped composition (dev app, offscreen, `--attended-local-draft`) reached `ATTENDED_LOCAL_DRAFT_SHOWN` on the generated worktree. Negative control: comparing against the target tip instead of the merge base fails the course at "only the branch's changes" | Pass |
| AC02 | `local_draft`: a modified tracked file and an untracked file are listed apart, an ignored one is not, nothing enters a candidate unselected; a selected file enters with its exact working bytes, origin `working` and the qualification "includes working files"; a filtered or over-bound file cannot be selected. `core_local_draft`: selection rules. Desktop course: the Not committed list, ticking a file offers Refresh, the successor shows the working bytes (italic) | Pass |
| AC03 | `local_draft`: an isolated shared clone resolves through its primary clone to the hosted remote, and the target tip is the hosted remote's, not the primary's local `main`; a `--no-local` clone that lacks the hosted tip reads it through the chain's objects; a cycle of local remotes is the `remote_chain` recovery and creates nothing; a fork with `origin` and `upstream` asks which (`several_repositories`) and the chosen parent compares correctly; no common history is `no_common_base`; a tip the objects lack is `target_unavailable`; no `refs/remotes/origin/HEAD` asks for the branch (`no_default_branch`); a detached head is `detached_head`; the tracking branch never sets the target; each leaves existing drafts as they were. `core_local_draft`: chain, fork, several bases | Pass |
| AC04 | `local_draft`: a successor candidate keeps the first; a write against the older version is `stale`; an external edit after capture does not move kept bytes, and bytes that changed since listing are refused (`changed`); a new store over the same profile reads candidates and notes back; a branch switch makes another draft and leaves the first; a moved folder is `moved` and nothing is rebound; another repository at the same path is not resumed. Desktop course: restart keeps two candidates and the note | Pass |
| AC05 | Desktop course: after a restart, the harness handoff (`local_draft_desktop::open_location` with context) resumes the same draft, adds the context as a harness note and makes no second draft. `bpr draft open` from a terminal (no desktop): created a draft (JSON result, context as a note), the second run resumed the same identity, a clone with no hosted remote answered `not_ready`/`remote_chain`, a plain folder `not_git`. Nothing publishes | Pass, except that `bpr draft open` forwarded to a running desktop (owner channel `local_draft`) has no course: it is built and runs the same `open_local_draft`, then shows the draft |
| AC06 | Desktop course: Prepare Pull Request… on Home signed out, above Home's sections; Review Cycles keeps File › Open Project… and its Home section, now below; `application_review_cycle_desktop_public_path` passes unchanged. In preparation opens by double-click or Return; the page opens on Changes with the title out of the tab order until clicked | Pass for reachability and keyboard; the look is the owner's ([Owner look](#owner-look)) |
| AC07 | `local_draft`: discard removes the draft and its material; the worktree, its branch and its working edits are unchanged; at 64 drafts a new open answers `full` with 64 of 64 and nothing is evicted. `core_profile_data_reset` and `sqlite_profile_data_reset_files`: `local-drafts` and `local-draft-git-v1` are review data, removed (and copied when asked) by Reset review data and Reset everything. Desktop course: Discard… removes the draft and leaves the worktree byte-identical | Pass; `application_profile_data_reset_public_path` was not run (its binary was removed for disk) |
| AC08 | `core_local_draft`: an associated draft leaves In preparation and is found for its pull request. Desktop course: an associated draft is not listed In preparation and its pull request (`acme/engine#7`) is in Home's prepared set, which the row's chip ("Prepared here", or "Draft, prepared here") and its Open Local Preparation item read; a resumed draft opens on Changes with no chat | Pass for the set, the list and the opening state; a queue row drawn with the chip has no course, and an association is recorded only by a verified read that this milestone does not compose (see [Limits](#limits)) |

## Runs

At `3585dedc7` (dev preset, Debug): `core_local_draft`, `local_draft`,
`core_profile_data_reset`, `sqlite_profile_data_reset_files`,
`application_review_cycle_desktop_public_path`,
`application_local_draft_desktop_public_path` (6/6). clang-format 22 on every
changed C++ file; clang-tidy (warnings as errors) on the eight new
translation units. Documentation gate: `DOCUMENTATION_OK`.

## Limits

- Association: `associate_draft` admits only a pull request whose head is the
  draft's branch and whose base is its target; nothing in this milestone reads
  a pull request to offer one. The publication RFC (RFC-BPR-0111) creates the
  pull request and records the association.
- Committed material is kept in the store when the candidate is captured, so
  a candidate stays readable after a rebase or garbage collection.
- `core.autocrlf` is noticed as a qualification, not applied; a filtered
  tracked file is not compared (the count is in the Not committed tooltip).
- A bare primary clone on the chain is unreadable through the workspace port
  and is the `remote_chain` recovery.

## Owner look

Filed, not waited on (session in the [handoff](../issues/_checkpoints/v0-5-0-prerelease-targets-v1/handoff.md)).

## Owner verdicts (attended session 5f049205ae4d7f19c1593afa81ea39df, handed back 2026-11-12T06:21:00Z)

- `BPR-2021-draft-page`: **Accept** (decision `6974c627-dede-4ab8-9c5a-5ef4b1f554a6`, 2026-11-12T06:17:03Z, actor human), no note
- `BPR-2021-home-author-entry`: **Reject** (decision `6f6e8d54-e3f0-4f34-9c4a-b3491e9643f0`, 2026-11-12T06:20:58Z, actor human), the owner: "Seems like a waste of vertical space to have it like this."

Root `/Users/jaeger/local-draft-v001.h8pe0ibf` removed with `freeze_attended_build.py remove` and the host
stopped (2026-11-12). The coordinator: a redesign from mockups comes first;
no new look is filed until then.
