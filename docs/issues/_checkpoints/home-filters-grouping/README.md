# Home filters and grouping takeover

User mandate (2026-10-12): finish the existing
`BetterPullReview-home-filters` worktree directly and land on main. Also
investigate the subsequently reported Needs me → Following change on opening
a PR. BPR-1414 has five acceptance criteria and one integrated outcome.

Claude's base is `241c21087`; WIP commits `d67da5f5e` and `773089f99`
plus uncommitted edits were preserved at `b229fdf1b`. No command was still
running in that worktree at takeover. Current main was merged without
conflicts. Discovery found no authored milestone plan; the card defines the
bounded outcome and exits. Strategy is direct, with no delegation.

The registered main project's direct workflow preflight passes. The legacy
Claude worktree is not independently registered; no MASK deployments or global
settings were changed. The frontend-design skill was loaded from the owner's
specified Claude plugin path. The design preserves Qt's font and palette,
leads with title, and puts identity/author/reason on the secondary line.

Observed before correction:

- Reviewer and Requested ignored discovery reasons; the old queue-composition
  test selected No role for a PR the viewer had reviewed.
- A new first-open-after-restart check failed with `bucket=quiet` although the
  accepted head had changed since the submitted review. The composition used
  a process-local first-read baseline instead of the accepted review commit.
- The existing accessible-description expectation omitted the newly separated
  author. It was updated to require the drawn author as well as the reason.

Verification and final evidence are recorded on BPR-1414. Local build logs and
offscreen rasters stay under the worktree's ignored `build` directory.

Candidate `3eebd2d82` includes the contemporary main changes and passes the
six-test integration course. The composed CLI restart now awaits remembered
sign-in instead of clicking Connect again: its exact-one transport assertion
and zero native-vault accesses pass. Recently viewed removal survives a second
restart for both native and CLI account courses. Full Home renders have been
inspected, with the capture waiting for every wrapped row to fit after resize.
No desktop capture, live provider write, or human-only criterion was required.
