# Codex handoff — 2026-10-17

The coordinator (Claude) stopped its implementers to save usage. This file
hands Codex the mechanical work that remains. Every decision listed here is
already made by the owner; do not re-ask, re-design or re-review it.

Read first: `AGENTS.md`, `CONTRIBUTING.md`, the card and checkpoint named
for each item.

## Rules for this handoff (owner, 2026-10-17)

- **Land one card at a time, in the order below.** Each item starts only
  after the previous one's `Finish BPR-…` commit is on `origin/main`.
- **Focused tests only to land.** No full `ctest --preset dev` per card. One
  integrated full suite runs at the end (item 9).
- Per landing: re-check `main` HEAD → merge `main` into the branch → build
  only the affected targets → run the card's focused rows → if `src/app`
  changed, run `application_composition_contract` and
  `application_review_round_trip_composition_contract` and reseal the
  round-trip pins on the merged tree with a dated note → `clang-format` 22
  check on touched files → `python3 scripts/verify_documentation.py --root .`
  (Python ≥ 3.10, no pipe) → fast-forward `main` → finish the card with the
  board tool → plain `git push origin main` (merge `origin/main` first if it
  moved; never force).
- Commit trailer on every commit:
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` is the
  coordinator's convention; use your own attribution if your harness requires
  it, but keep commits intentional and per card.
- AGENTS.md rules that bit us this week: never `git stash`; stop only
  processes you started, by PID (never `pkill -f`); never drive or capture the
  owner's desktop; keep builds lean (disk has repeatedly hit 0 free — check
  `df -h /` before any full build and delete your own build trees when done);
  live GitHub only on `MikeMarcin/BPR-Test`, watch `X-RateLimit-Remaining`.
- After landing each card: stop its attended-review server if one is still
  running (PID in its checkpoint), release protection
  (`attended_review.py … resume-development`), remove its frozen build with
  `scripts/freeze_attended_build.py remove`, remove its worktree and build.

## Landing queue

### 1. BPR-1577 — Overview Open/All switch v002 (accepted)
- Worktree `/Users/jaeger/.codex/worktrees/bpr1577-counts-switch/BetterPullReview`,
  branch `bpr-1577-quiet-counts-switch`, head `ec589f2a0` (main already merged;
  was mid focused tests when stopped).
- Card `docs/issues/40-parked/BPR-1577-the-overview-open-and-all-switch-is-quiet-like-its-row.md`.
- Focused: the Overview page test rows (incl. `--switch` render test).
- Session `cf892a3b…` (owner-review-20261017-bpr1577-counts-switch-v002), host PID 39316;
  frozen `/Users/jaeger/bpr1577-counts-switch-v002.4iietnlb`.

### 2. BPR-1578 — review bar progress track v002 (accepted)
- Worktree `/Users/jaeger/.codex/worktrees/bpr1578-review-bar-accept/BetterPullReview`,
  branch `bpr-1578-review-bar-accept`, head `5f3a96baa` (main merged, round-trip resealed over `ee9830062`).
- Card `docs/issues/40-parked/BPR-1578-…`; checkpoint `docs/issues/_checkpoints/BPR-1578/handoff.json`.
- Focused: `qt_review_state_icon`, `qt_option_keys_typing`, `qt_workbench_window_scale_{100,125,200}`, title-row/focus-order rows, composition contracts.
- Session `3760f12f…` (`/Users/jaeger/.codex/attended-review/bpr1578-v002-final-hB6QZ7`), host PID 2303;
  frozen `/Users/jaeger/.codex/attended-builds/bpr1578-progress-track-v002.bebcwn1p`. Also stop/release the
  earlier round sessions if still up (PIDs 51437, 33432, 53547).

### 3. BPR-1597 — Settings by task (accepted); closes BPR-1568
- Worktree `/Users/jaeger/Documents/GitHub/BetterPullReview/.claude/worktrees/bpr-1566-cc-link`,
  branch `claude/bpr-1597-settings-by-task`, head `501e5923f`.
- Pages: Home & queue · Review · Comments (owner renamed "Reviewing" → "Review").
  Repositories stays after Accounts & hosts.
- Card `docs/issues/40-parked/BPR-1597-settings-by-task-home-and-queue-review-comments.md`;
  move `BPR-1568-…` to Done as closed into BPR-1597.
- Focused: Settings tests (search finds moved rows), composed workshop courses, Home lists, composition contracts (reseal).
- Watch: settings schema number clash — BPR-1584 and BPR-1588 already on main; renumber this card's step if it adds one.
- Session `a090236d…` (`~/.mask/attended-review-sessions/bpr-1597-v001-20261017`), host PID 4027;
  frozen `/Users/jaeger/bpr-1597-v001.mu3ytfkc`.

### 4. BPR-1600 — ⌘R refreshes Home (owner decision, no review needed)
- Worktree `/Users/jaeger/.codex/worktrees/bpr1600-home-refresh-key/BetterPullReview`,
  branch `bpr-1600-home-refresh-key`. **Uncommitted WIP** in keyboard_shortcuts,
  workbench_window, the RFC-BPR-0026 amendment, the UX spec and two test files — review and finish it.
- Only the reservation exists (`docs/issues/_reservations/BPR-1600--…`); write the card.
- Requirement: ⌘R on Home runs Home's existing refresh with BPR-1575 feedback; on a PR it still refreshes the PR;
  steps aside while typing if the PR binding does; listed in the shortcuts catalogue (drift test must pass);
  record the owner's reversal of BPR-0990. Tests incl. a negative control.

### 5. BPR-1572 — stacked PRs compare from the recorded base's merge base (approved by Fable + Astra round 4)
- Worktree `/Users/jaeger/.codex/worktrees/bpr1572/BetterPullReview`, branch `bpr-1572-stacked-merge-base`,
  head `cb211c828`. Checkpoint `docs/issues/_checkpoints/BPR-1572/continuation.md`.
- Last report: Astra's round-4 condition is done (refusals `target_moved`/`stale_head` propagated through
  review submission, tours and source-backed comments; tests fail with it reverted). Confirm Fable's three
  non-blocking items are done: wrapper-path tests for a 500 and a cancellation on the wrapper's PR read;
  soften the comment at `github_provider.cpp` ~1093-1100; doc wording "every witnessed write bound to a pull request".
- Then land (focused tests + composition contracts, reseal, docs gate, card → 60-done), push, remove the 15 GB build.
- Skip the large-review timing reruns; item 9 covers them.

### 6. BPR-1598 — lint findings + load-robust timing tests
- Worktree `/Users/jaeger/.codex/worktrees/bpr1598-tidy-timing/BetterPullReview`,
  branch `bpr-1598-tidy-and-load-robust-timing`, head `aa7e17559` (evidence and tidy inventory recorded).
- Was writing the final card content when stopped. Fix `home_panel.cpp:324` and `server_card.cpp:738/739`
  tidy findings; make `coding-tidy` + `coding-format-check` green; make the `application_large_review_*`
  timing tests robust to machine load without simply raising budgets (CPU time / RUN_SERIAL / calibrated
  baseline), with a negative control (injected delay still fails). Write the card, land.

### 7. BPR-1599 — `application_private_review_native` stalls before Review (fix on main)
- Worktree `/Users/jaeger/Documents/GitHub/BetterPullReview/.claude/worktrees/bpr-1599`,
  branch `bpr-1599-private-review-native`. **Uncommitted changes** in `workbench_window.{hpp,cpp}` and
  `tests/adapters_qt/composed_github_tests.cpp`.
- CAUTION: when stopped, the implementer said it was about to *temporarily revert the two src files* to run a
  negative control. Inspect the diff carefully: decide whether the working tree holds the fix or the reverted
  state before committing.
- Goal: find the regressing commit (suspects: BPR-1588 ⌘2 first-unseen rule / Start review removal,
  BPR-1573 Close review, BPR-1581 navigation, BPR-1575/1576 pending veils), decide product bug vs. test
  update, fix with a negative control; also check `application_private_review_file` and
  `application_epoch_portable_occurrence_two_catalogues`. Only the reservation exists; write the card.
- May land any time between other items (it fixes main), after merging main.

### 7b. BPR-1601 — pull request comments get read state (owner decision, data model)
- Card `docs/issues/10-backlog/BPR-1601-pull-request-comments-get-read-state.md` (allocated `1202dff1e`);
  depends on BPR-1589, which is landed. No worktree yet: branch from `main`.
- Owner, 2026-10-17: comments that are not on a file (pull request conversation comments and review
  summaries) have read and unread state as review threads do. Scope, per the card: the review ledger's
  read state in core, persistence (backward compatible), Discuss's new markers, Threads' "Not on a
  file" row, Mark as read/unread, and ⌥N next new comment.
- The Overview's Status drawer already has the line, hidden: call
  `pull_request_overview_page::set_new_conversation_comments(count)` from the window when the host
  publishes the count; its Go to them already opens Discuss (`set_new_conversation_dispatch`).
- Focused: core ledger and storage courses for read, unread, seen-before and restart; public-path
  courses for Discuss, Threads' Not on a file row, the Status line and ⌥N; composition contracts if
  `src/app` changes. No owner review needed (no visual design beyond the existing markers).

### 7c. BPR-1593 — Tours as a route expanding to its page (ACCEPTED by owner 2026-10-18, no note)
- Worktree `/Users/jaeger/.codex/worktrees/bpr1574-drawer-redesign/BetterPullReview`, branch `bpr-1593-tours`,
  product `eb4990c98`, checkpoint `202e6b3d3`; also carries `4f0d4b69d` (mouse wheel over Notes scrolls the code).
- Checkpoint `docs/issues/_checkpoints/BPR-1593/handoff.md` (on the branch) has the landing steps.
- Land: merge main, run every `tour` ctest row SERIALLY (the tour composition test is serial-GUI), the window test
  binary + scale variants, notes panel, composition contracts (re-pin round-trip), format check, docs gate; finish
  BPR-1593 with the owner's Accept (session `9a9c8a15…`, `/Users/jaeger/.mask/attended-review/owner-review-20261018-bpr1593-tours-route-v001`,
  decision recorded at cursor 3+); push; stop review server PID 21499; remove frozen root
  `/Users/jaeger/bpr1593-tours-route-v001.fmh3hj33` with `scripts/freeze_attended_build.py remove`.
- Leave the worktree in place afterwards (Claude resumes BPR-1594 in it); delete only its build tree if disk is tight.

### 7d. BPR-1594 — Agent drawer tab (ACCEPTED by owner 2026-10-18, no note — land it)
- Owner review: session `5547b2341ac4b1228c7f89e5ed5110c1`,
  state `/Users/jaeger/.mask/attended-review/owner-review-20261018-bpr1594-agent-tab-v001`, candidate
  `BPR-1594-agent-tab-v001`. Read the session (`attended_review.py … status --after 3`). If there is no decision
  yet, SKIP this item. If Rejected, do NOT implement v002 (design work stays with Claude); just record the verdict
  on the card and leave it parked.
- If Accepted: worktree `/Users/jaeger/.codex/worktrees/bpr1594-agent-tab/BetterPullReview`, branch
  `bpr-1594-agent-tab` (pushed; head `3bda132f8`), built on `bpr-1593-tours` — land only AFTER item 7c (BPR-1593).
  Follow `docs/issues/_checkpoints/BPR-1594/handoff.md` on that branch: merge main, focused tests (workbench window
  ×3 scales, keyboard shortcuts, side chat, agent panels, finding queue, private notes, agent finding public path),
  composition contracts + re-pin, docs gate, finish BPR-1594, update RFC-BPR-0085's status section, push, stop
  review server PID 49516, remove frozen root `/Users/jaeger/bpr1594-agent-tab-v001.87u4icdl`.
  Note: the `.agents` symlink in that worktree must be set aside when running board commands.

### 7e. BPR-1603 — PR comments at the Comments drawer's edge: BUILD direction A (owner picked 2026-10-18, no note)
- Design: branch `bpr-1603-conversation-chips` (pushed), worktree `/Users/jaeger/.codex/worktrees/bpr1603-conversation-chips/BetterPullReview`,
  design note + mockups `docs/design/BPR-1603-conversation-chips-v1/` (A = "One chip, a list in place"), checkpoint
  `docs/issues/_checkpoints/BPR-1603/handoff.md`. Owner's pick is recorded in session `537cac7b…`
  (`/Users/jaeger/.mask/attended-review/owner-review-20261018-bpr1603-conversation-chips-design-v001`); its server is stopped.
- Build A exactly as mocked: top-edge chip "N on the pull request" + blue new count, beside the Notes chip (same shape);
  pressing it turns the drawer into the list of PR-level comments/review summaries oldest-first with a "New" divider;
  one opens in place; "Back to threads at their lines" returns; opened = read (BPR-1601); Show in Discuss; review
  summaries show verdict + links to their file comments. Start from the top-edge count chip in `private_notes_panel.cpp`,
  applied in `conversation_cards_view.cpp`. RFC D8 already matches A (no amendment).
- REPLY: owner CONFIRMED (2026-10-18) — add replying from the drawer. A reply posts a new pull-request
  (issue) comment that quotes the one it answers (GitHub's conversation is flat). Reuse the existing publication
  path (`staged_review_publication.cpp` can create a PR comment; check it) — core decides, host performs; draft first,
  explicit Send, never auto-post; drafts survive restart; failures shown with retry; respect the BPR-1560/1572
  publication safety rules (re-read the PR before the POST, refuse on change). Record the owner's scope extension on
  the card and in RFC-BPR-0085 D8. Test with stubs (contract) plus one live course on MikeMarcin/BPR-Test only
  (create a PR comment reply, then delete it; watch X-RateLimit-Remaining).
- Tests (focused), composition contracts, docs gate. Then freeze a build and present ONE Accept/Reject owner review in a
  fresh short session with Light/Dark renders against the A mockup — this is a look-and-judge criterion; do not land
  before the owner accepts. On Accept: land, finish BPR-1603, update RFC-BPR-0085 status, push. On Reject: stop and leave
  the note for Claude.

### 8. Housekeeping
- Stop any leftover attended-review servers from landed cards and remove their frozen roots.
- `git worktree prune`; delete merged branches.

### 9. One integrated full suite
- On final `main`: `cmake --preset dev && cmake --build --preset dev && ctest --preset dev --output-on-failure`
  on a quiet machine (nothing else building). Rerun failures alone; report every remaining failure with its
  cause. Then `cmake --build --preset dev --target coding-format-check coding-tidy`.

## Not for Codex (taste/design — stays with Claude)

- As of 2026-10-18: RFC-BPR-0085 milestones through BPR-1592 are landed; BPR-1593 is item 7c above. Claude keeps (only if 7e is rejected) **BPR-1603 (PR comments as drawer-edge chips; design note + review first)**, then the RFC closeout.

- **RFC-BPR-0085 drawer redesign resumed with Claude.** Milestones 1 and 2 (BPR-1587, BPR-1588,
  BPR-1589) are landed. Do not touch BPR-1590–BPR-1595 or the worktree
  `/Users/jaeger/.codex/worktrees/bpr1574-drawer-redesign`; BPR-1601 (item 7b) is Codex's.
- Any new owner look-and-judge question.

## Known open follow-ups (not in the queue)
- `release_update_native_https` intermittently fails (`inbound-header-truncated`, ~1 in 3 alone).
- Colours below contrast floor that predate BPR-1585 (Note text/"?", amber R badge/bubble, amber open digits in Light).
- Windows station: visually confirm BPR-1596's title-row start in Light and Dark.
- Export dialog doesn't take the app theme (BPR-1579 note).
