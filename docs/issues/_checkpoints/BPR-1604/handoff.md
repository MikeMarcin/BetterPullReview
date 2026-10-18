# BPR-1604 continuation

Mandate revision: owner-polish-2026-10-18-r1. Assignment: finish BPR-1604 and BPR-1605 together, preserve Claude's incomplete implementation, push meaningful steps, prepare ONE short attended session with two Accept/Reject decisions, stop before human acceptance. No landing on main before actual acceptance.

Source: /Users/jaeger/.codex/worktrees/bpr-polish-contrast-blocking/BetterPullReview; branch bpr-polish-contrast-blocking; takeover HEAD 8e1230941. Nine inherited dirty source/test paths are authorized task work. No prior checkpoint or process rooted in this worktree. Root checkout and all other agents/processes are outside scope.

Current action: preserve the inherited implementation as a WIP commit and merge main to reconcile the board's allocation high-water. No tests claimed yet; editor still paints the old split and band test permits known short colors.

Plan: complete the theme-derived quiet/amber adjustment and check all shared uses; show one open-blocking total with a pending split matching Approve; verify core, host and actual workflow/editor; self-review, format, focused composition and documentation gates; commit/push; freeze one self-contained source/build with launcher --check; embed actual before/after Light/Dark and HC contrast renders in two protected review candidates. Human AC3 for both remains pending.

Engineering owner: /root/finish_polish, non-delegating. No task-owned background commands.

## Candidate engineering update

frontend-design was loaded before visual work. Keep the existing native font, geometry and labels. The deliberate color direction is the inherited Light ground #FFFFFF, text #1F2328 and surface #F6F7F9, and Dark ground #0D1117 and text #DCDCDC; the attention seed is each theme's own amber. Move quiet grey only toward text until all shared normal surfaces read; move amber's weaker channels first so Light becomes burnt orange. No typography or layout redesign.

All QPalette::PlaceholderText consumers were inventoried across the Qt adapter, including native placeholders, text labels, folder paths, chrome marks, share/review sheets, Home and source-search delegates. They use the same derived role; ordinary Window/Button/ToolTipBase, Base and AlternateBase are the supplied grounds. Selected item branches keep HighlightedText. BPR-1529's existing four-theme pixel test verifies both folder surfaces use exactly that role and remain distinct from filenames; it passes. Disabled palette roles retain their existing treatment. Other state_attention consumers keep their original theme seed.

The contrast test now holds every measured ink to its real floor on both Base and band; no grandfathered short-color path remains. Actual Qt baseline substitution from 8e1230941 (appearance_profile, Overview and review_submission_editor), with the same render input, rejects the strict check: Light quiet #8f9193 is 3.16/2.92/2.95 on Base/band/Window; old Marker #d29922 is 2.52/2.33 on Base/band. Baseline and after PNGs are private runtime artifacts at /tmp/bpr-polish-render. Two R/Note/? rows cover plain and band; Light/Dark/HC Light/HC Dark are included. Sources were restored in a finally block.

Direct core, feedback source, Qt Files tree, Overview, appearance and editor checks pass. New carried-blocking real-app course passes inside application_label_publication_recovery (22.40 seconds). The broad review_submission_composition course fails its unchanged legacy Agent purpose/tab assertion before preflight; this exact prior failure and cause are documented in codex-handoff-2026-10-17.progress.md lines 349-374 and assigned to BPR-1616. No unrelated repair included. Composition contract and focused tidy are still running; final checkpoint will record outcomes. Documentation gate and its 67 tests pass.

Failures corrected: inherited core test locals shadowed global black/white; inherited least-move assertion used #8F9194 rather than the actual integer midpoint #8F9193. First abbreviated build target names were invalid, and an early CTest invocation found two executables not built yet; subsequent exact targets and affected suites passed.

Final portability review: renamed a new preflight-chip structured binding from `text` (a QAbstractButton member name) to `count`, in accordance with the Qt AGENTS Windows shadowing rule. The unpresented first freeze is superseded; the replacement uses the corrected source candidate. No behavior or acceptance change.

The integration contract correctly rejected the changed app-source manifest pin (729bd235… vs 601eaa0b…). Reviewed the only app-tree changes, review_submission_workflow.cpp/.hpp: carried-comment context and projection caching only, with no publication, capability or executor path changes. Updated the production app-manifest pin and retained all negative controls; the production mutation-host/full github_application digests stay unchanged. This test-only upkeep does not change the frozen d06e7400e product or rendered candidate.
