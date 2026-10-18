# BPR-1602 continuation — 2026-10-18

- Mandate: owner request 2026-10-18 (Settings reset of local data and a
  recovery path). Design A picked (decision `47b87fa1…`), rows trimmed;
  coordinator: Reset settings keeps saved repositories, servers and observer
  rules; Reset everything clears them.
- Worktree `/Users/jaeger/Documents/GitHub/BetterPullReview/.claude/worktrees/bpr-1602-reset-local-data`,
  branch `claude/bpr-1602-reset-local-data`; candidate `234bdee3e` (main
  `cd80d1ab3` merged). Build tree `build/dev` there.
- Design session `ea1ed383…` handed back, protection released, host stopped.
- Final session `534aa242d644f5ab110f86fa14257fd6`, state
  `~/.mask/attended-review-sessions/bpr-1602-final-v001-20261018`, host PID
  93091 on port 51376 (log `~/.mask/attended-review-sessions/bpr-1602-final-v001-serve.log`),
  candidate `BPR-1602-reset-built-v001`, revision `frozen-234bdee3e-v001`,
  event cursor 7. Protects `install:/Users/jaeger/bpr-1602-reset-v001.6t1zlk_c`
  and `src/adapters/qt/src/profile_data_reset_views.cpp` / `.hpp`.
- Frozen root `/Users/jaeger/bpr-1602-reset-v001.6t1zlk_c` (remove with
  `scripts/freeze_attended_build.py remove`); test profile
  `/Users/jaeger/bpr-1602-reset-v001-profile.d` (plain `rm -rf` after review).
- Next: on Accept, record the verdict, `issue_board.py finish BPR-1602`,
  merge main again, rerun the composition contracts (reseal if src/app
  moved), fast-forward main, stop PID 93091, release protection, remove the
  frozen root, profile folder, worktree build and worktree. On Reject, read
  the note and revise in a new candidate revision.
