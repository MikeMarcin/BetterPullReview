# BetterPullReview: Dif mascot integration

**RFC pack v1 · proposed RFC-BPR-0030 and RFC-BPR-0031 · 18 cards · 6 authored milestones**

Implement the prop-free Dif panda and all ten animation clips generated in this
conversation in the real BPR C++/Qt application. This is a source-grounded design
and implementation pack, not a claim that the app has already been changed.

## Start here

Read `IMPLEMENTATION-PROMPT.md`, `supporting/SOURCE-AUDIT.md`, and the two RFCs in
`repository-overlay/docs/rfcs/`. Import `milestone-plan.json` as the default
execution plan: allocate live BPR issue IDs, remap the pack-local aliases, validate
with the installed MASK-managed implement-rfc skill, and keep its milestone grouping.
Do not replace the plan with a new unbounded planning exercise.

The pack-local aliases are deliberately not allocated live issue IDs. RFC numbers
0030/0031 were free in the supplied September 12 snapshot; check for collisions at
import and record a mapping rather than overwriting anything.

## What this delivers

| Workstream | Scope |
|---|---|
| RFC-BPR-0030 | Preserve the exact ten image inputs; clean their mattes; reconcile character/neutral-pose drift; register actual frames; extend the existing deterministic asset compiler to a bounded ten-clip contract; obtain focused production-art acceptance. |
| RFC-BPR-0031 | Compose a retained Qt sprite player into Home and the explicitly opened AI tools area; add profile-owned visibility/motion controls; use accepted existing projections for truthful one-shot cues; prove lifecycle, performance, and real-app reachability. |

All ten clips are in scope: `calm_idle`, `greet`, `rest`, `blink`,
`compare_glance`, `consider`, `comment_ready`, `attention`, `acknowledge`, `blocked`.
The foundation is an incremental milestone, not permission to declare the
seven additional clips complete while leaving them unused.

Dif does not cover code, appear as a review author, imply AI approval, animate on
every token, or introduce another provider/process path. Home remains the current
Home with its paste-to-open, queue/recent items, and Back to the review behavior.

## Important input qualification

The bundled PNGs are **unmodified incoming candidates**, not final runtime strips.
All ten have real alpha. Five eight-frame sheets have 443.5-pixel nominal cells,
the idle uses 362-pixel cells, the three six-frame sheets use 512-pixel cells, and
the neutral source is 1254 square. Some clips differ in paws, mouth, proportions,
or chest markings. The actual idle also blinks, unlike the design's separate,
infrequent blink policy. These are concrete normalization tasks, not a reason to
discard the artwork or silently ship inconsistent frames.

See `verification/raw-asset-audit.json` and `supporting/ASSET-INTAKE.md`.
`review/raw-animation-viewer.html` plays the raw inputs using provisional crops;
open it locally after extracting the whole archive. It is a review aid, **not**
a runtime implementation or a passed animation-acceptance gate.

## Budgets

The intended catalog has 71 content frames and twenty transparent gutter cells.
At 256×256 RGBA, that is **22.75 MiB decoded pixels**. The new hard complete-pixel
ceiling is **24 MiB**, with separately measured Qt overhead. This explicitly
amends the old three-clip/13.5-MiB contract rather than disabling its validator.
The existing compressor-independent PNG comparison policy is preserved.

## Contents

- `repository-overlay/`: two additive RFCs and BPR's sprite-updater consumer context.
- `cards/`, `milestone-plan.json`: prepared issue work and dependency-ordered milestones.
- `assets/`: exact generated sheets and source hashes, not resources to compile directly.
- `specs/`: clip choreography/timings and contextual cue map.
- `supporting/`: current-source audit, supersession rules, asset intake, and integration map.
- `verification/`: acceptance matrix, focused review course, measured raw-image audit.
- `review/`: raw-input playback and theme-composite inspection material.
- `references/`: the earlier animation prompt pack, supplied updater skill, source baseline.
- `scripts/`: offline pack-integrity and raw-input audit helpers.

## Verify the handoff

```sh
python3 scripts/verify_pack.py
python3 -m unittest discover -s tests -v
```

The pack verifier needs only Python's standard library. The optional pixel audit
uses Pillow; it is a maintainer helper, never an ordinary build dependency.
Neither command changes the BPR repository, starts the app, contacts a provider,
or installs/updates the shared skill.

## Evidence and scope

Primary source: the supplied BPR dump generated 2026-09-12T19:50:30-07:00.
The older September 8 dump is not substituted for it. `references/SOURCE-BASELINE.json`
records the complete snapshot hash and inspected section hashes. The live checkout
must be inventoried once at import. Pack validation proves document/asset integrity
and plan consistency; it does not prove Qt behavior, native support, art approval,
or rights over new derivative bytes.
