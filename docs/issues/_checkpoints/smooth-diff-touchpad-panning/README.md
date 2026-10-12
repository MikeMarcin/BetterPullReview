# Smooth diff touchpad panning: initial checkpoint

Assignment `/root/smooth_panning_m1`, coordinator/implementer recipe revision 2.
The exact mandate, its digest, draft digests, baseline and command state are in
[checkpoint.json](checkpoint.json); the resolved selection is preserved in
[strategy-selection.json](strategy-selection.json).

Execution stopped before import, allocation, claim or product edits because the
required `frontend-design:frontend-design` skill was unavailable. Root
`AGENTS.md` requires: “Any UX or visual work (mockups, design candidates, icons,
layout, Qt surfaces the owner will look at) loads the
`frontend-design:frontend-design` skill first and says so in its report.”
No matching `SKILL.md` was found under the five roots recorded in the checkpoint,
and this runtime exposes no orchestrator skill lookup. The skill has not been
loaded; this checkpoint does not claim that requirement is satisfied.

The selected draft has no authored milestone plan. The bounded
[proposal](plan-proposal.json) preserves all AC01–AC08 in one integrated milestone,
including the owner’s explicit drift-suppression and discrete-keyboard amendments.
The [preparation receipt](plan-preparation.json) validates its structure, without
adopting it or proving coverage. A target-aware preparation refused the not-yet-live
RFC-DRAFT-0012; that [refusal](target-preparation-refusal.json) is retained. No live
RFC/card IDs were allocated and no acceptance evidence exists.

The verified immutable source baseline is
`268b87049592ca4643e5303ab77114e116e7b5f3`. RFC-BPR-0004 and RFC-BPR-0005 are
Auto-Verified in the live RFC index. The source draft and proposed amendment
remain unchanged and untracked, with their exact digests recorded.

The next implementer should read a trusted supplied or installed
`frontend-design:frontend-design` SKILL.md, reconcile concurrent writers and the
draft digests, then import with live allocated IDs and provenance on main, adopt
the proposal and bind/claim the milestone. The concrete Qt touchpoints are
`comparison_scroll_controller`, `measured_alignment_axis`, and `source_pane`
inside `diff_workbench.cpp`; the composed evidence surface is
`tests/adapters_qt/diff_workbench_tests.cpp`. Existing controller and workbench
scale courses run at 100%, 125%, and 200%. They are starting surfaces, not new
acceptance evidence. Exercise the shortest composed fractional-origin course
early, retaining actual pane/hit-test/ribbon geometry and keyboard behavior.

Finish all unattended engineering and focused verification before AC07. Prepare
its self-contained build with `scripts/freeze_attended_build.py`, then a fresh
short attended-review session and loopback server. Keep AC07 pending until the
owner gives the actual Accept/Reject verdict; no desktop automation or capture.

This assignment started no background command, server or worktree. Writer
ownership is released. Other agents’ connection work and initial `.gitignore`,
`.mask.json`, `.mask.lock.json` edits remain untouched. The only task-owned files
are this checkpoint directory. No product build or tests ran because no product
candidate exists. Resume this milestone after the required skill is resolved.

Validation: `python3 scripts/verify_documentation.py --root .` passed with
`DOCUMENTATION_OK`; the plan preparation passed. Product criteria remain untested.
