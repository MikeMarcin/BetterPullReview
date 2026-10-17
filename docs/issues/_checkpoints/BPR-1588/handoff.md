# RFC-BPR-0085 milestone 1 handoff (BPR-1587, BPR-1588)

- Branch `bpr-1574-drawer-redesign` at the commit that records this file;
  product head `e5182fd8e` (milestone merged with `main`). Not on `main` yet:
  it lands after the owner accepts.
- Changed product paths: `src/adapters/qt/{include/bpr/qt,src}/{drawer_parts,overview_status_drawer}*`,
  `pull_request_overview_page*`, `workbench_window*`, `workbench_drawer*`,
  `keyboard_shortcuts*`; `src/application/.../workbench_runtime_ports.hpp`,
  `workbench_application_host*`; `src/adapters/sqlite/src/workbench_settings_database*`
  (schema 22); `src/app/main.cpp`, `src/app/src/github_application.cpp`; tests and
  docs as listed on the cards.
- Verification: see the cards. One full suite at `70d7d2de1` (763 of 775,
  every failure explained and all but `application_large_review_history`
  passing alone); 25 affected courses on the merged tree.
- Owed: the owner's two answers (session `e471b4b8…`); a quiet-machine rerun
  of `application_large_review_history`; schema 22 renumbering if
  [[BPR-1584]] lands first; the View-menu entry with [[BPR-1580]].
- Task-owned processes: the review host (`serve`, PID 93486) only. The
  worktree has an untracked `.agents/skills` mirror of main's MASK links for
  the documentation tests; remove it at cleanup.
