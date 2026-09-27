# RFC-0022 milestone A checkpoint

Assignment `/root/rfc0022_m1`, mandate `r2` (owner amendments retained in
`mandate-r2.json`), run `rfc0022-20260926`, segment `m1`.
Source/tracker: `/Users/jaeger/.codex/worktrees/6264/BetterPullReview`; branch
`codex/rfc0022-semantic-comments`. Original RFC baseline remains
`5a616354ae6ecd7ebbea608a9523a8388f590ab0`.

BPR-0561–0563 are Done. Core policy source is `1bc5b109`; host source is
`e96a62f4`, followed by `75328275` card acceptance. BPR-0564 is In Progress:
canonical compile/parse and hostile tests pass, with its own isolated
`github_review_comment_permalink` target now separated from legacy fixtures.
The real app rebuilt successfully after this final source arrangement.

The first integrated pass passed 6/6 (core, exact host, GitHub contract,
composition, architecture boundaries, isolation contract). A final pass checks
the standalone codec and core naming-only tidy correction. Targeted tidy and
core-only rebuild/replay are running; no provider or live process is running.
Documentation gate passes and its 64 regression tests pass. Formatting passes.

Audit corrections retained: recovery schema is 3; later migration must preserve
v1/v2 native and v3 whole-review rows. Left replies and ordinary one-use exact
user Publish grants remain supported. Production placement evidence remains
unknown until a complete provider observation is supplied; full-file/Qt geometry
never establishes off-hunk placement.

Shared historical counter advanced concurrently on another branch. Carrying
existing reservation commit `129432b3` as `7e3a3454` preserved both NEXT_ID
BPR-1220 and its genuine BPR-1215..1219 receipt. No new allocation was invented.

After card acceptance and command quiescence, coordinator integrates/pushes
milestone A to main before dispatching a fresh owner for milestone B
(`b-provider-recovery`, BPR-0565–0568). B–D and final canonical closeout remain
pending. Live acceptance is GitHub.com only by owner amendment; GHES fixtures
remain required and native GHES is unobserved. The coordinator designated
https://github.com/MikeMarcin/BPR-Test/pull/2; initial immutable refs are retained
in mandate r2 and require fresh authority at live use. This leaf performed no
live mutation. No subagents, other checkout writers, or review server.
