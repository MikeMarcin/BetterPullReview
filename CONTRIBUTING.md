# Contributing to BetterPullReview

## Before changing code

Read the root `AGENTS.md`, the nearest nested `AGENTS.md`, the relevant RFC,
and its live issue card. Folder placement under `docs/issues/` is workflow
state. Do not begin a dependent RFC while a prerequisite is open.

Preserve unrelated work. Keep each issue-card slice narrow, move its card
through the board, verify the changed public/runtime surface, and commit the
completed slice before starting the next one.

## Architecture rules

- Core code is standard/domain C++ only and performs no I/O.
- The host owns effects, authority sessions, jobs, caches, cancellation, and
  lifecycle; it must not duplicate core policy.
- Qt owns widgets, actions, focus, layout, accessibility, and event
  translation; it must not call effectful capabilities directly.
- Every asynchronous completion carries request, session, revision, and
  generation identity as applicable and returns to a serialized decision lane.
- Bounded changes update bounded projections. Do not reset an entire model or
  scan a whole review for an isolated interaction.

## Required local verification

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure

cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only --output-on-failure

python3 scripts/verify_documentation.py --root .
git diff --check
```

Run more focused commands named by the affected RFC verification ledger.
Generated screenshots, logs, build trees, credentials, private repository
content, and secret-bearing evidence must not be committed.

## Documentation impact

Review public APIs, provider/authentication behavior, identity, persistence,
threading/lifecycle, UI commands, build/package configuration, security, and
launch behavior. Update the owning semantic document or record an evidence-
backed `no-impact` disposition in the RFC ledger. A nearby documentation edit
does not waive a semantic obligation.
