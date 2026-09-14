---
schema: "implement-rfc.card.v1"
id: "BPR-0769"
title: "Verify and close RFC-BPR-0031"
status: "Backlog"
rfcs: ["RFC-BPR-0030", "RFC-BPR-0031"]
depends_on: ["BPR-0757", "BPR-0766", "BPR-0767", "BPR-0768"]
closeout_for: ["RFC-BPR-0030", "RFC-BPR-0031"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"verification-closeout","acceptance":["A8","B12"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0031

RFC base: `9ce46e3576a44441cfcd932fca128091275e0068`.

This combined pack card retains the source's simplification and independent-review
work and tracks formal closeout for both RFCs. RFC-BPR-0030 must satisfy its own
criteria and canonical order before RFC-BPR-0031 can close. The title names the
last dependent RFC to satisfy the repository's closeout-card convention.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **A1 — Exact intake.** All ten immutable inputs, actual layouts, hashes, source/edit history and new derivative authority/disposition are recorded; no opaque concept-board crop, foreign mascot or guessed generator metadata is shipped. | Partial: ten exact source hashes/layouts and known history limits recorded; derivative disposition remains open | `57ced605`; [intake ledger](../../verification/RFC-BPR-0030-intake.md) |
| **A2 — One character.** The prop-free master and all ten sequences preserve one identity, resolve the documented drift, and have accepted neutral, body anchors and foot baseline. | Partial: exact neutral identity accepted at session sequence 379; canonical pixel-equal master and rest/idle study exist, but nine moving sequences and final anchors remain unaccepted | `aca551c0`; [art ledger](../../verification/RFC-BPR-0030-production-art.md) |
| **A3 — Complete choreography.** Ten real distinct sequences have the target contracts, closed idle seam, intended eyelid/limb/head motion and clean neutral handoffs; no static alias or whole-raster squash substitutes for a gesture. | Partial: twelve-frame local breath structurally screened; v003 greeting study rejected for limb-area discontinuity and shoulder crease; seven extended gestures unnormalized; no final motion verdict | [v003 study](../../verification/dif-production-art-v003/README.md) |
| **A4 — Canonical geometry/alpha.** Every final cell and both gutters per strip meet exact format, clipping, inventory, anchor, envelope and transparent-pixel checks. Adversarial drift/empty-frame/padding fixtures fail for the intended reason. | Partial: schema-2 synthetic negatives and staged rest/idle alpha checks pass; final ten-clip cells/gutters remain unproduced | `df3ee35e`; [compiler ledger](../../verification/RFC-BPR-0030-compiler-contract.md), [art ledger](../../verification/RFC-BPR-0030-production-art.md) |
| **A5 — Reproducible catalog.** Empty-output regeneration reproduces canonical pixels and metadata, with exact committed payload digest checks and no compressor pin regression. Generated descriptors/resources agree and malformed/unknown/excessive catalog input fails. | Partial: schema-2 synthetic catalog and portable regeneration pass; final committed catalog pending | `df3ee35e`; [compiler ledger](../../verification/RFC-BPR-0030-compiler-contract.md) |
| **A6 — Measured bounds.** The actual catalog stays within the revised limits; arithmetic includes padding and uses checked dimensions/counts. Ordinary builds neither generate art nor acquire creative dependencies/network resources. | Partial: synthetic 71-frame/22.75-MiB strip budget and limits pass; actual catalog/build proof pending | `df3ee35e`; [compiler ledger](../../verification/RFC-BPR-0030-compiler-contract.md) |
| **A7 — Bound visual decision.** Exact normalized bytes, timing and preview renderer are covered by the focused identity/motion/theme/scale/handoff/rest review and derivative disposition. Any changed subject is re-reviewed only to its necessary scope. | Partial: neutral identity accepted for original exact bytes; canonical derivative, complete motion/theme/scale/handoff/rest and rights disposition await final review | `aca551c0`; [art ledger](../../verification/RFC-BPR-0030-production-art.md) |
| **A8 — Handoff and history.** Mutable docs and live asset/verification ownership identify this new revision; historical decisions remain unchanged; RFC-0031 consumes the accepted new catalog. Existing prerequisites/formal status waits are explicit, not falsely closed. | Partial: live import/IDs and intake ledger exist; accepted new catalog and formal handoff remain open | `ee1e12df`, `57ced605`; [live import](../../rfcs/assets/bpr-dif-integration-v1.LIVE_IMPORT.md) |
| **B1 — Resource selection.** Normal tests-enabled and tests-disabled Qt apps select the accepted ten-clip catalog; raw inputs/review tools do not ship. Invalid assets fail to a functional art-free surface with bounded diagnostics. Core-only remains clean. | Partial: fixture ten-strip cache validates bounded assets; accepted production catalog and normal-app resource selection remain pending | `e337c1fa`; [runtime ledger](../../verification/RFC-BPR-0031-runtime-foundation.md) |
| **B2 — Playback.** Catalog-driven elapsed-time playback renders every clip at its declared rate, one-shots return correctly, idle seam/blink cadence/greeting-once behavior holds, and delayed/invalid time cannot burst, overflow or select gutters. | Partial: fixture ten-clip sequencer passes; real asset idle/blink/greeting path remains pending | `e337c1fa`; [runtime ledger](../../verification/RFC-BPR-0031-runtime-foundation.md) |
| **B3 — Preferences.** Real Appearance controls apply/migrate/persist/reset motion and visibility through the current profile authority; rejected/stale writes and session-only fallback are truthful, same-profile fan-out works and other profiles remain unchanged. | Passed for the profile preference path; actual mascot presentation remains with later integration | `6f07c343`; [runtime ledger](../../verification/RFC-BPR-0031-runtime-foundation.md) |
| **B4 — Observation.** Native System motion observation/change handling is proved per claimed target; unknown/unsupported resolves reduced, explicit Full is usable, and fixture/native claims and any changed OS setting are documented separately. | Partial: macOS Cocoa current read observed; injected change and shutdown pass; actual OS preference change/notification remains unproved | `e337c1fa`; [runtime ledger](../../verification/RFC-BPR-0031-runtime-foundation.md) |
| **B5 — Home.** Dif is reachable through the existing signed-out and signed-in Home, including Home covering an open review. Open/back actions, Recent/queue controls, compact layout, focus and return-to-reading state are unchanged. | Not run; import only | No producing revision |
| **B6 — Context placement.** One explicit visible AI tools slot uses static baseline and short cues, never overlays code/shared comments or opens surfaces itself; one sprite per workbench, no duplicate active slot, and controls retain space/accessibility. | Not run; import only | No producing revision |
| **B7 — All-ten product paths.** Each clip is shown by its specified ordinary UI action or accepted projection. Actual replies/findings/draft acceptance drive ready/attention/ acknowledgment; no process-only success, AI author substitution or autonomous effect. | Not run; import only | No producing revision |
| **B8 — Identity/dedup.** Duplicate, stale, replayed, switched-window/profile/PR/session, late invocation, rejected draft and superseded operation cases suppress incorrect cues; terminal coalescing and cooldowns never delay authoritative UI. | Not run; import only | No producing revision |
| **B9 — Lifecycle.** Hidden, inactive, reduced, modal-obstructed, minimized, suspended and destroyed presentations have no continuing mascot timers/repaints/late callbacks. Resume is neutral with no queued cues or repeated greet. Shutdown remains orderly. | Partial: fixture widget suppression, timer/paint and destruction pass; actual app/window lifecycle and greeting remain pending | `e337c1fa`; [runtime ledger](../../verification/RFC-BPR-0031-runtime-foundation.md) |
| **B10 — Performance.** One decoded retained catalog is shared across windows/profiles; measured pixels/overhead/conversion peak are bounded, no per-tick image allocations or layout/classification changes, and a focused comparison shows no diff-work regression. | Partial: fixture cache sharing and accounted bounds pass; accepted assets, native backend memory and diff-work comparison remain pending | `e337c1fa`; [runtime ledger](../../verification/RFC-BPR-0031-runtime-foundation.md) |
| **B11 — Rendered/accessibility evidence.** Actual app frames at shipping scales and supported appearance modes demonstrate readable alpha edges, calm motion, static reduced interval, noninterference with focus/status and approved visual integration. | Not run; import only | No producing revision |
| **B12 — Complete handoff.** Focused tests, resource packaging, platform claims, source/ mutable docs, shared-skill consumer context and verification records agree. All ten clips are real and reachable; unavailable evidence/formal closeout remains honestly open, with no fabricated native/art verdict or forced rewriting of settled RFCs. | Partial: plan/consumer context and intake documented; all app/resource/native/art work remains open | `ee1e12df`, `57ced605`; [live import](../../rfcs/assets/bpr-dif-integration-v1.LIVE_IMPORT.md) |

## Current frontier — authorized offline image-edit continuation, dif-takeover-20260914 d6 / mandate r4

The user removed the former `PYTHONDONTWRITEBYTECODE=1` helper requirement in
mandate revision 4; normal helper invocation is now allowed. Completed passing
checks were not repeated solely for this change.

The user explicitly answered yes to scoped image editing for the missing
connected arm poses. This d6 amendment overrides the earlier BPR-0755
no-model-run boundary for offline production-art candidate edits only.
The exact neutral acceptance at sequence 379, immutable incoming provenance,
ten-clip/71-frame target and final focused human art/derivative review still
apply. Ordinary builds and shipped runtime remain free of generation. The
integrator saved two exact-prompt edited full-body inputs at
`docs/verification/dif-production-art-v004/generated-inputs/`; both visibly
change body scale/face and are candidate arm material only. A deep worker's
first local composite preserved locked neutral regions and improved measured
limb-area ratios to 0.936–1.084, but 96/256-pixel inspection found a rounded
fist snapping to a narrow open palm and a straight shoulder join. It is not
promoted. The same owner is attempting one targeted correction, then will
report even if it fails. A separate worker is assessing seven raw extended
clips; its full-body registrations drift, and it is testing a local eyelid
extraction. The extended worker has now returned quiescent with a provisional
local-eye blink repair that locks all non-eye pixels and alpha exactly; it
retains a faint 256-pixel texture band. Its full-body seven-clip studies are
rejected. The greeting worker remains active in its separate folder; the
integrator retains cards, source/catalog, review and commits. V003 remains
rejected. BPR-0755/0756/0757
and both production-art exits are pending. No new human verdict, catalog or
runtime asset exists at this checkpoint.

## Previous frontier — connected greeting study rejected, dif-takeover-20260914 d5 / mandate r2

The d4 return was consumed as `continue-milestone` on `main` at `6b588724`.
The d5 assignment binds the same BPR-0755/0756/0757 production-art work and
both exits. The exact phase-one neutral identity accept remains valid. V002
holds a canonical pixel-equal master, exact rest and twelve local-breath idle
study frames; neither motion nor the canonical derivative has a final art
verdict. The seven extended gestures and final catalog are still absent.

The [v003 connected-anatomy study](../../verification/dif-production-art-v003/README.md)
retains a reproducible recipe, checks and 64/72/96-pixel light/dark plates. It
uses raw padded-paw pixels and passes exact neutral endpoints, fixed body,
alpha, baseline and clearance checks. Agent inspection rejects it as greeting
source: visible limb area rises 72.1% at frames 2→3 and falls 39.3% at 4→5,
with a dark shoulder crease. The shape discontinuity needs a consistently
authored upper-arm/elbow and forearm across the lift, with separate wrist/paw
articulation. More rigid rotation, whole-cell registration or feathering do
not repair that gap. No greeting source frame, source manifest, generated art,
runtime selection, model call or GitHub/GHES mutation was made. The old
attended verdict, original source and unrelated dirty files remain intact.

The deep worker returned quiescent; the integrator owns and committed only the
v003 staging and current records. Its recipe regenerated to retained hashes,
documentation and board checks passed. No timed playback, native Qt motion or
human motion decision is claimed. BPR-0755 remains In Progress; BPR-0756/0757
remain Backlog. Both production-art exits are pending, so no complete-catalog
review candidate exists. Once a continuous greeting and the seven other real
gestures are authored and compiled, prepare one focused Dif-only digest-bound
art/derivative review in a fresh external `--state`; the shared checkout
session's historical answers must not be rewritten. The next bounded step is
authored connected-limb illustration at the accepted master's scale, followed
by actual-speed local playback before extending the catalog. This is an
artwork dependency, not a completed milestone.

## Previous frontier — neutral identity accepted, production-art resumed, dif-takeover-20260914 d4 / mandate r2

The exact protected `BPR-0755-neutral-identity-v001` candidate was accepted by
the human in attended session `cbd48b91000b00f1e72e1ad62c7c384c` at sequence
`379`: fingerprint
`1bcdc33cbd9175db6ee843f60f90e3a70e9800470a19562bd4814b18d600ad81`,
decision `6eabb190-287d-4e7c-a19f-bd14f4075701`, outcome `accept`, empty
note. Sequence `380` handed the session back. This is approval of the prop-free
neutral identity only. BPR-0755 was promoted and claimed on `main`; its
rest/idle/greet motion, BPR-0756's seven extended gestures, BPR-0757's catalog,
derivative disposition and full-art verdict remain open. The independent
`runtime-foundation` milestone remains accepted at `3036364c`.

The unrelated older questions appeared because the shared attended-review
`init` merges candidates into one checkout session, and its page lists all
answered candidates; 30 prior and current answers were present. No historical
decision was altered. The hand-back watch has exited and the local host was
stopped. For the final Dif art decision, use a fresh external `--state`
session containing only its current candidates. The standard offline art leaf
returned quiescent with the [v002 staging study](../../verification/dif-production-art-v002/README.md):
canonical pixel-equal accepted master, exact rest and twelve local-breath idle
candidate frames, and a 71-cell provisional raw crop inventory. The attempted
greeting wave failed the shoulder seam and cheek-height requirements. A focused
second study used raw raised-paw pixels and reached cheek height but still had
shoulder/belly seams, a narrow forearm and ghosted entry/return, so it too was
rejected from source. The seven extended gestures remain unnormalized. No
shipped source, manifest, generated catalog or runtime selection changed.

The production compiler now clears hidden RGB from alpha-rounded-zero pixels
for schema 2 while preserving schema-1 generated bytes. A red-before-fix
regression, a separate schema-2 master-path test, 56-test focused suite before
that added test, direct shipped validate/verify, empty-output regeneration,
dev/core-only asset CTests, selected-graph boundary check, documentation gate
and v002 retained-hash check passed. This is structural preparation only;
the full art/derivative verdict and both production-art exits remain pending.
Browser policy refused local playback preview; static theme plates and APNG
frame/timing data were checked, without claiming observed motion.

All leaf writers and task commands have returned/stopped. Only unrelated
`.gitignore`, `.mask.json`, `.mask.lock.json` and the old BPR-0775/0776 filename
deletions remain outside this slice. No GitHub/GHES mutation or new model run
occurred. The next objective requires careful offline illustration/layer
retouch of the real greeting paw and seven contextual gestures at the accepted
master's scale, then normalization, exact catalog compilation and one complete
Dif-only digest-bound art/derivative review. BPR-0755 stays In Progress; its
partial source study cannot be called card or milestone completion.

Native delegation used `rfc-worker` with a fresh non-history-copying context,
then `followup_task` to the same slice owner for the focused greeting correction;
neither call overrode model or effort. The generated Codex route specifies
`gpt-6-astra` / `low`, and both worker returns self-reported that banner.
Serving telemetry and runtime enforcement of the child-role allowlist were not
exposed; neither is inferred from the banner or generated configuration.

## Previous frontier — runtime-foundation accepted, dif-takeover-20260914 d3 / mandate r1

The authored `runtime-foundation` binding covers BPR-0758/0759/0760 and both
exits, with accepted `intake-contract` as its only prerequisite. All three
cards are Done on `main`: BPR-0758 source `6f07c343` and finish `af8b574d`,
then integrated observer/player source `e337c1fa` and BPR-0759/0760 finishes
`64f38067`/`ddff59fb`. The [runtime ledger](../../verification/RFC-BPR-0031-runtime-foundation.md)
records accepted typed preferences/migration/fanout/failure behavior, a
fixture-only real Appearance gesture and restart, macOS current-state read,
injected observer delivery/shutdown, and a ten-clip synthetic retained Qt
player with bounded cache/timer/paint checks. A direct offscreen app gesture
after integration passed with an explicit 30-second budget. The independent
changed-surface reviewer found no blocking issue; two follow-ups were fixed
and retested. `DOCUMENTATION_OK` and selected focused checks pass.

The fixture catalog is never selected for shipping. Actual production art is
still pending the exact protected BPR-0755 neutral decision/session described
below. Actual OS reduced-motion change delivery is unproved; the native Cocoa
current-state read passed, while Windows/Linux support remains unsupported.
Native backend memory and real app/Home animation checks remain for later
authored milestones. The existing `application_appearance_gesture_contract`
timeout and architecture boundary gate failure are recorded in the ledger;
neither was misreported as a pass. No GitHub/GHES mutation, OS preference
change or production-art selection occurred.

Active source workers, reviewers and test commands: none; all dispatched
children returned. The next bounded objective is the authored
`home-integration` milestone once production art is accepted, or independent
later verification that can run without the art if the coordinator assigns it.
Formal RFC closeout and B1/B2/B4/B5–B12 remain open to their full scopes.

## Previous frontier — neutral decision pending, dif-takeover-20260914 d2 / mandate r1

The assigned `production-art` milestone binds BPR-0755/0756/0757 and both
authored exits. BPR-0755 was claimed on `main` at `b24c4817`. Its [current
candidate and evidence](../../verification/RFC-BPR-0030-production-art.md)
are an exact-hash 1024-square derivative of the immutable rest source and
identity/theme/shipping-size review plates. The candidate remains outside
runtime resources. It passed only structural/alpha/baseline screens; no
identity verdict or accepted master exists yet. The phase-one attended
candidate is `BPR-0755-neutral-identity-v001` in shared session
`cbd48b91000b00f1e72e1ad62c7c384c`, ready/protected at sequence `378`.
It is the exact first decision before broad retouch. BPR-0756/0757
and the 71-frame production catalog remain unstarted. The accepted historical
card-bearing catalog and all incoming sheets remain unchanged.

Owned changed paths are the BPR-0755 board record, this closeout, the
[art ledger](../../verification/RFC-BPR-0030-production-art.md), and
`docs/verification/dif-production-art-v001/`. The earlier intake evidence
remains valid. No task-owned model call, source replacement, runtime selection,
GitHub/GHES mutation or app build occurred. The first-phase identity decision
is required, then the remainder of `production-art` remains open. The authored
`runtime-foundation` milestone can proceed independently with fixtures after
this writer and its commands are quiescent. Formal RFC status and final
art/derivative/native evidence remain pending.

The attended local host is serving the exact candidate on loopback port
`51856` (process `40091`, command session `58012`). A read-only attendance
watch after sequence `378` is command session `28709`, writing any event to
`build/dif-production-art/wait-after-378.json`. Its session token remains
outside the repository. No art/source writer, leaf agent, build or test command
remains active. The successor must consume the exact verdict by session ID and
candidate fingerprint; a hand-back without a decision does not accept it.

## Previous frontier — intake-contract accepted, dif-takeover-20260914 d1 / mandate r1

The adopted plan's `intake-contract` milestone is complete on `main` at
`7beaf272d4318187c8250da06161f2c2290ee878`. BPR-0752, BPR-0753 and
BPR-0754 are Done with separate scoped commits and unchanged assigned
requirement fingerprints. The [live import](../../rfcs/assets/bpr-dif-integration-v1.LIVE_IMPORT.md)
binds the immutable base, ID receipt and source drift. The [incoming ledger](../../verification/RFC-BPR-0030-intake.md)
records ten exact hashes and actual geometry, including five fractional grids
and unapproved crops. The [compiler ledger](../../verification/RFC-BPR-0030-compiler-contract.md)
records a tested synthetic schema-2 ten-clip/71-frame contract, pixel-backed
registration, both gutters, 22.75-MiB decoded fixture and portable PNG/digest
semantics. The shipped schema-1 catalog remains valid. Fifty-five focused tests,
direct shipped validate/verify, the registered dev CTest asset contract,
documentation gate and whitespace check passed. These establish the three
authored exits for this intake milestone, not final production art or app
acceptance.

The two leaf workers returned; no task writer, descendant or test command is
active. Only unrelated pre-existing `.gitignore`, `.mask.json`, `.mask.lock.json`
and deleted BPR-0775/0776 filenames remain dirty. The next bounded authored
objective is `production-art` (BPR-0755/0756/0757): confirm one prop-free
master, normalize all ten real gestures, compile the 71-frame production
catalog and obtain the exact focused art/derivative decision. BPR-0755 is
dependency-ready. RFC-0030/0031 formal status, application integration and
native/human evidence remain open; no GitHub/GHES mutation or publication is
authorized by this handoff.

## Previous frontier — dif-takeover-20260914 d1 / rfc-integrator-d1 / mandate r1

The `intake-contract` milestone still binds BPR-0752/0753/0754 and all three
authored exits with unchanged requirement fingerprints. BPR-0752 is Done at
`ee1e12df`; BPR-0753 is Done at `57ced605`, with its ten-sheet audit in the [mutable
intake ledger](../../verification/RFC-BPR-0030-intake.md). All 55 archived
checksums, ten payload hashes, actual grid arithmetic, raw-audit equality,
rest structural screening and documentation gate passed. Five grids are
fractional. Raw idle frame 7 blinks; matte and identity corrections and the
new derivative decision remain for production-art, not this source audit.
The existing shipped three-clip compiler validates and its 42 focused tests
pass. Its audit worker has returned and has no active command/descendant.
BPR-0753 finished at `57ced605`; BPR-0754 is claimed at `7d438f16`.
Its sole worker returned candidate-ready and has no active commands or
descendants. The compiler/tests and new [compiler ledger](../../verification/RFC-BPR-0030-compiler-contract.md)
are committed at `df3ee35e`; BPR-0754's execution record and this closeout
are the pending bookkeeping paths. That revision adds a schema-2 ten-clip branch, locked pelvis/foot/head regions,
measured content envelopes, generated geometry/digests and portable
regeneration while retaining the shipped schema-1 path. All 55 focused tests,
a later vertical-foot control, shipped v1 validate/verify, documentation gate
and whitespace check passed. The registered dev CTest asset contract also
passed on the committed revision in 43.28 seconds. Parent is finalizing
BPR-0754 acceptance and the three bound exits; no task command is active.
No native or human art verdict is claimed.

## Prior frontier — dif-takeover-20260914 d1 / rfc-integrator-d1 / mandate r1

`intake-contract` binds BPR-0752/0753/0754 and its three authored exits.
BPR-0752 is claimed; the imported plan, immutable base, ID mapping and current
composition-path drift have been checked on `main`. The pack verifier passes on
a temporary exact source copy excluding two unmanaged `.DS_Store` files; direct
verification refuses the extra inventory as designed. Owned changed paths so far
are the BPR-0752 board/evidence card and this closeout. No child writer or long
command is active. Next: finish BPR-0752 with the focused documentation gate,
audit/freeze BPR-0753, then implement and test BPR-0754's schema-2 compiler.
Production art, app integration, native and human decisions remain open.

## Earlier frontier — import only, dif-import-20260913-1 / mandate r1

The pack has been imported on `main` with RFC-BPR-0030/0031 Proposed and all
eighteen mapped cards in Backlog. The immutable source, full alias mapping,
September 12 snapshot comparison, allocation receipt, and bounded plan-format
adjustment are recorded in the [live import record](../../rfcs/assets/bpr-dif-integration-v1.LIVE_IMPORT.md).
The [adopted plan](../../rfcs/plans/bpr-dif-integration-v1/milestone-plan.json)
retains six authored outcomes. Its `intake-contract` obligation and exit binding
resolves, but neither that milestone nor any card has been accepted or executed.
The next authorized implementation intake starts with BPR-0752–0754 and the
actual compiler/catalog/geometry contract after checking current source drift.
Production art, real-app behavior, native/platform evidence, focused human
decisions, independent review, and canonical formal closeout are all pending.
No task-owned application writer or test process remains active; the unrelated
`.gitignore`, `.mask.json`, and BPR-0617 work is outside this import.

## Outcome

Remove accidental duplicate systems and perform one risk-focused independent review of migration, resource selection and lifetime/cue identity. Reconcile every acceptance row and truthful formal status.

## Acceptance

- All A1–A8 and B1–B12 rows reference direct evidence or a specific unresolved criterion.
- All ten clips are real, accepted, reachable and within measured budgets; no placeholder or missing mapping is hidden.
- Formal RFC/index closeout obeys canonical order and actual platform/human results; no broad old-RFC rewrite.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
