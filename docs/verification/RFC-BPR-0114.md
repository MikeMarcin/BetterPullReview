# RFC-BPR-0114 verification

Mandate `v0-5-0-prerelease-targets-r1`, pulled forward from the
local-draft-authoring plan onto lane `claude/v0-5-0-prerelease-targets-52265d`
by the owner's redesign of 2026-11-12 ([accepted design](../design/local-draft-pr-window-v1/README.md);
amendment in the [RFC](../rfcs/RFC-BPR-0114-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md)).
Cards [[BPR-2065]], [[BPR-2066]], closeout [[BPR-2067]]. Frontend-design was
loaded before the Qt work. Debug timings are nongating; nothing here is a
timing claim.

## What was built

| Layer | What |
|---|---|
| Core (`core/local_changes.hpp`) | Status model; the three comparisons and their material keys; a file's changes (`change_hunks`) and the exact index text Stage Change (`apply_change`) and Unstage Change (`revert_change`) write, refused as `stale` or `inexact` (an equal line whose terminator differs) rather than guessed; `uncommitted_bands`; `decide_commit` (nothing staged, no subject, detached, unborn amend, unmerged, amend of a pushed commit) and the exact message; `decide_push` (unobserved, missing, ahead, up to date, behind, diverged, unknown tip) and `head_on_host`; `make_push_command` (`git push --no-follow-tags URL refs/heads/B:refs/heads/B`, refusing option-like URLs and malformed branches); the agent's commit-message request (96 KiB of staged diff, ten subjects, an exact note of what was cut) and answer reading |
| Git adapter | `installed_author_git`: the author's own Git, separate from the isolated reader; application environment without repository-redirecting variables, `GIT_TERMINAL_PROMPT=0`, new session with the descriptor sweep, bounded deadlines (30 s reads, 60 s `ls-remote`, 5 min commit and push), stop checks, group kill and reap; reads with no optional locks and no fsmonitor; `write_index_entry` (`hash-object -w --no-filters` and `update-index --cacheinfo`, keeping the entry's mode); push refuses any forcing argument; Windows answers `unavailable`. `make_worktree_files`: reads and writes one named regular file beneath the root with `openat`/`O_NOFOLLOW`, writes only when the disk still holds the bytes read, through a temporary file and one rename, keeping the mode |
| Application | `local_changes_host`: observations on the effect lane, projections on the decision lane; writes followed by a fresh observation and never repeated; the one-time question (`local_changes_consent`); Check Again; Save; the agent's message on its own lane |
| Qt | `local_changes_panel` (the pick with counts, Unstaged over Staged folder trees with letter boxes, Stage and Unstage, the pinned commit box with the agent's control in the Agent accent, Amend, Commit N Files, the push line with its state, Push/Stop/Check Again and the exact command, the refusal banner); Stage Change and Unstage Change on the code's menu in the window's own diff viewer (2026-11-13; `local_diff_view` deleted) |
| Settings | `local.commit-message` (Commit message, `local-commit-message-v1`): profile schema 29, the agent setup store's hook checks widened to 0..10 (narrow tables rebuilt), Built-in prompts › Local drafts, export and import |
| Boundaries | `architecture_boundaries` names the runner as a QProcess and environment owner and refuses force, stash, checkout, switch, reset, rebase, merge, tag, branch, clean, gc and prune as Git arguments in it (negative control: a `"stash"` literal fails the rule) |

## Criteria (as amended 2026-11-12)

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `local_changes` (real repositories made by the build's Git): Stage Change writes exactly one change to the index (`git diff --cached -U0` holds it and not the other), Unstage Change returns it, the worktree never changes; Stage and Unstage move files; a commit records the exact message, the repository's pre-commit hook ran, only what was staged is committed, the unstaged file is as it was. `application_local_draft_author_desktop_public_path`: the same through the window (Stage Change band, Commit 2 Files). `core_local_changes`: exact texts including CRLF and a missing final newline | Pass; signing configuration not exercised |
| AC02 | `core_local_changes`: every commit refusal. `local_changes`: a refusing `commit-msg` hook leaves no commit and its output is in the banner; Amend rewords while GitHub lacks the commit and is withdrawn once it has it | Pass |
| AC03 | `local_changes`: push to a local bare "hosted" repository; GitHub's tip equals the head; no upstream written, no remote-tracking ref moved. Desktop course: the same through Push, with the URL rewritten by the worktree's own `insteadOf` (the author's configuration applies). Live, 2026-11-12: a BPR-Test clone on disposable branch `bpr-local-changes-live-20261112084707`; stage, commit, Check Again (`missing`, Push offered, the exact command shown), Push through the owner's own Git credential helper (`gh auth git-credential`), then `up_to_date` with GitHub's tip `2dd171d1abad` equal to the head; the branch was deleted afterwards (HTTP 204) | Pass for the push; Create Draft PR… after a push is RFC-BPR-0111's and was not rerun here |
| AC04 | `local_changes`, desktop course: another author's push makes GitHub's tip unknown here; Push is refused before anything runs, the banner says so, GitHub's branch is unchanged. Boundaries rule and negative control above | Pass |
| AC05 | `local_changes`: the first Commit asks, nothing runs before the answer, the answer is remembered for the draft; declining shows the commands and runs nothing | Pass; the one-time question's wording is part of the owner's look |
| AC06 | Deadlines and stop checks are in the runner; a cancelled or timed-out call kills the group and the host observes again. A prompt needing a terminal fails because `GIT_TERMINAL_PROMPT=0` | Partly: no course stops a running push or forces a timeout |
| AC07 | Pushing from an associated draft uses the same press; the look associates a draft with #128 | Built; no course pushes from an associated draft to a real pull request |
| AC08 | Owner look filed (see the handoff) | Pending owner |

## Runs (dev, Debug, macOS)

`core_local_changes`, `local_changes`, `application_local_draft_author_desktop_public_path`,
`application_local_draft_desktop_public_path`, `application_local_draft_tour_desktop_public_path`,
`application_local_draft_publication_desktop_public_path`,
`sqlite_workbench_settings_storage` (with a new version 28 to 29 step),
`sqlite_agent_configuration_storage`, `core_agent_configuration`,
`core_local_draft_tour`, `qt_workbench_window_scale_100/125/200`,
`architecture_boundaries`, `architecture_boundary_contract`,
`test_network_isolation_manifest`,
`application_review_round_trip_composition_contract` (app pin resealed).
Not run: the whole suite, the core-only preset, Windows and Linux.

## Owner verdicts (attended session 6d00826a4d9c2ee08fe91321c2c1d521, handed back 2026-11-13T03:52:51Z)

Owner words verbatim. The frozen root `/Users/jaeger/local-draft-pr-window-v002.lyqc133b`
(from `be709f8e6c6a`) and host 75022 were removed after hand-back.

- `local-draft-pr-window`: **Reject** (decision `8fe04b44`, 2026-11-13T03:50:35Z, screenshot attached in the session)
> This whole local draft view just looks kind of amateur compared with the PR view, I thought we chose to stick with the PR view and just augment it. This seems like we tried to rebuild it and didn't do a great job.
- `home-1`: **Accept** (decision `753d927e`, 2026-11-13T03:41:14Z, screenshot attached in the session)
> Looks good except there's something weird with selection. I can select both local draft and recently viewed and escape doesnt clear selection of either.
- `view-picker-and-edit`: **Reject** (decision `a4b84680`, 2026-11-13T03:52:47Z, screenshot attached in the session)
> Seems like we have no syntax highlighting. This useless files tab doesn't even update the file in the edit pane.
- `local-changes-commit-push`: **Reject** (decision `2c74ac41`, 2026-11-13T03:49:58Z, screenshot attached in the session)
> I don't know why Local Changes tab doesn't show up untless I go to a drop down and say show local changes. That drop down option seems unnecessrry just have the tab always. I also don't know why we don't have the search tab for find in files.
>
> I actually don't know what this files tab is for, seems just strictly worse than the files tab we already have for PRs. And strictly worse than the Local Changes tab.
> The average user won't need htis information, can hide it behind a disclosure.
- `agent-commit-message`: **Reject** (decision `016d90c7`, 2026-11-13T03:39:45Z, screenshot attached in the session)
> Nib isn't using agent accent color. And I think it belongs next to commit not subject.
>
> This tooltip box feels extra long. Actually severaal tooltip boxes feel super long.

What follows: the separate local-draft page is replaced by the pull request
window itself, with author-state additions only (round "local draft in the PR
window", 2026-11-12). Home 1 is accepted except the selection fault, which the
same round fixes.

## Local draft in the pull request window (2026-11-13)

The Edit view, Local Changes, staging, commit and push are now the pull request window's additions; see the [RFC-BPR-0108 ledger](RFC-BPR-0108.md#local-draft-in-the-pull-request-window-2026-11-13). Edit has syntax colours and follows the file chosen in Files, Local Changes or Search; the agent's nib is beside Commit in the agent accent (`qt_local_draft_window` holds it, with an accent inventory of Local Changes and the one-sentence tooltips).

## Owner verdicts (attended session 767b92339e278ea303b3836f21426fd3, handed back 2026-11-13T06:41:17Z)

All five looks accepted, two with notes; verbatim in the
[RFC-BPR-0108 ledger](RFC-BPR-0108.md#owner-verdicts-attended-session-767b92339e278ea303b3836f21426fd3-handed-back-2026-11-13t064117z).
The notes are follow-up [[BPR-2156]].

## AC06 courses (2026-11-13)

The author's Git now runs on a worker lane of its own in the composition
(`local_draft_desktop`, before: the shared workbench effect lane), and a
failed, stopped or timed-out push reads the hosted branch again
(`local_changes_host::write`, before: only after a push that succeeded).

| Course | What it shows | Result |
|---|---|---|
| `local_changes` (`stopped_and_timed_out_pushes`, real repositories, a pre-push hook that marks it ran and sleeps 30 s) | Timed out: the hooks bound set to 1.5 s, the push ends `timeout` after 1,568 ms, the hosted branch is read again (not there), Push is offered again, the hook ran once. Stopped: the push runs on a thread lane, Stop ends it `cancelled` 94 ms after the press, the hosted branch is read again and holds nothing, the hook ran once (nothing repeated) | Pass |
| `application_dependable_session_public_path` step 2b (the composed desktop, fixture GitHub) | The author's Git lane held as a push in flight holds it (`hold_author_git_lane`), #18's Approve publishes | Pass |

Counterfactual recorded: holding the shared workbench effect lane instead also
lets #18's Approve publish, so review submissions never depended on the lane
the push used; the lane of its own keeps a push waiting on hooks or the network
from holding file loads and other workbench work.
