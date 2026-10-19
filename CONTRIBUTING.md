# Contributing to BetterPullReview

## Before changing code

Read the root `AGENTS.md`, the nearest nested `AGENTS.md`, the relevant RFC,
its live issue card, and `_coding-standard/STANDARD.md`. Folder placement under
`docs/issues/` is workflow state. Do not begin a dependent RFC while a
prerequisite is open unless the canonical RFC index records a narrow
product-owner exception. The current exception allows available-target
implementation to advance past an open real-Windows verification card; it
never waives that evidence, permits closing with unmet explicit dependencies,
or makes hosted workflow output a substitute.

Preserve unrelated work. Keep each issue-card slice narrow, move its card
through the board, verify the changed public/runtime surface, and commit the
completed slice before starting the next one.

## Architecture rules

- Core computation follows [external determinism](docs/architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md#21-external-determinism),
  with explicit semantic inputs/state and host-owned effects. Internal timing or
  layout variation does not disqualify a library. Qt independence, dependency
  approval and the [implementation inventory](docs/architecture/system-architecture.md#core-implementation-inventory)
  remain separate requirements.
- The host owns effects, authority sessions, jobs, caches, cancellation, and
  lifecycle; it must not duplicate core policy.
- Qt owns widgets, actions, focus, layout, accessibility, and event
  translation; it must not call effectful capabilities directly.
- Every asynchronous completion carries request, session, revision, and
  generation identity as applicable and returns to a serialized decision lane.
- Bounded changes update bounded projections. Do not reset an entire model or
  scan a whole review for an isolated interaction.

## Verification commands

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
cmake --build --preset dev --target coding-format-check coding-tidy

cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only --output-on-failure

python3 scripts/verify_documentation.py --root .
git diff --check
```

Choose focused commands from the affected RFC ledger for ordinary slices. At pack or milestone completion, run one integration-focused pass; full relevant suites and critical review are appropriate when warranted. The command list is a reference, not a mandatory matrix on every slice. Reuse unchanged evidence.
Generated screenshots, logs, build trees, credentials, private repository
content, and secret-bearing evidence must not be committed.

## Documentation impact

Review public APIs, provider/authentication behavior, identity, persistence,
threading/lifecycle, UI commands, build/package configuration, security, and
launch behavior. Update the owning semantic document or record an evidence-
backed `no-impact` disposition in the RFC ledger. A nearby documentation edit
does not waive a semantic obligation.
