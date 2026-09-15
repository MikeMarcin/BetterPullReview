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
| **A1 — Exact intake.** All ten immutable inputs, actual layouts, hashes, source/edit history and new derivative authority/disposition are recorded; no opaque concept-board crop, foreign mascot or guessed generator metadata is shipped. | Passed for the exact d10 set: all returned/input/archive hashes and supplied edit log are retained, missing generator metadata is disclosed, and sequence 122 accepts the exact derivative disposition | `98841656`; [v008 intake and verdict](../../verification/dif-production-art-v008/README.md) |
| **A2 — One character.** The prop-free master and all ten sequences preserve one identity, resolve the documented drift, and have accepted neutral, body anchors and foot baseline. | Passed: sequence 113 accepts the common no-catchlight identity; canonical master/endpoints, body anchors and baseline 223 agree across all ten clips | `98841656`; [production evidence](../../verification/dif-production-art-v008/production-evidence.json) |
| **A3 — Complete choreography.** Ten real distinct sequences have the target contracts, closed idle seam, intended eyelid/limb/head motion and clean neutral handoffs; no static alias or whole-raster squash substitutes for a gesture. | Passed: sequences 111–121 explicitly accept all ten declared-speed clip subjects; structural checks confirm distinct rows, seam and exact handoffs | `98841656`; [human verdict](../../verification/dif-production-art-v008/review/human-verdict.json) |
| **A4 — Canonical geometry/alpha.** Every final cell and both gutters per strip meet exact format, clipping, inventory, anchor, envelope and transparent-pixel checks. Adversarial drift/empty-frame/padding fixtures fail for the intended reason. | Passed: promoted 71 cells and 20 gutters validate with clean alpha, bounds, clearance and registration; both 57-test contract runs pass | `98841656`; [production evidence](../../verification/dif-production-art-v008/production-evidence.json) |
| **A5 — Reproducible catalog.** Empty-output regeneration reproduces canonical pixels and metadata, with exact committed payload digest checks and no compressor pin regression. Generated descriptors/resources agree and malformed/unknown/excessive catalog input fails. | Passed: fresh v008 reconstruction matched all 85 managed files byte for byte; direct validate/verify and narrow schema-2 ten-clip consumer pass | `98841656`; [production evidence](../../verification/dif-production-art-v008/production-evidence.json) |
| **A6 — Measured bounds.** The actual catalog stays within the revised limits; arithmetic includes padding and uses checked dimensions/counts. Ordinary builds neither generate art nor acquire creative dependencies/network resources. | Passed: 71 frames plus 20 gutters total 23,855,104 decoded bytes; selected dev/core-only/tests-disabled app graphs preserve the offline-tool/runtime boundary | `98841656`; [production evidence](../../verification/dif-production-art-v008/production-evidence.json) |
| **A7 — Bound visual decision.** Exact normalized bytes, timing and preview renderer are covered by the focused identity/motion/theme/scale/handoff/rest review and derivative disposition. Any changed subject is re-reviewed only to its necessary scope. | Passed: all twelve v008.3 candidates were accepted and handed back at sequence 123 with no pending subject | `98841656`; [human verdict](../../verification/dif-production-art-v008/review/human-verdict.json) |
| **A8 — Handoff and history.** Mutable docs and live asset/verification ownership identify this new revision; historical decisions remain unchanged; RFC-0031 consumes the accepted new catalog. Existing prerequisites/formal status waits are explicit, not falsely closed. | Passed for production-art handoff: accepted schema-2 source/products are shipped and history is retained; normal app selection remains explicit under B1/home integration | `98841656`; [art ledger](../../verification/RFC-BPR-0030-production-art.md) |
| **B1 — Resource selection.** Normal tests-enabled and tests-disabled Qt apps select the accepted ten-clip catalog; raw inputs/review tools do not ship. Invalid assets fail to a functional art-free surface with bounded diagnostics. Core-only remains clean. | Passed: both normal Qt app graphs embed exactly the accepted ten strips; core-only is clean, decoding validates the accepted straight-RGBA digests/end gutters, and refusal is latched once on a functional art-free Home | `4bbecb48`; [Home integration](../../verification/RFC-BPR-0031-home-integration.md) |
| **B2 — Playback.** Catalog-driven elapsed-time playback renders every clip at its declared rate, one-shots return correctly, idle seam/blink cadence/greeting-once behavior holds, and delayed/invalid time cannot burst, overflow or select gutters. | Passed for the retained player and accepted Home route: exact production descriptors feed the bounded all-clip engine; real greet/calm/blink/rest frames and delayed/invalid suppression pass. Ordinary contextual routes remain separately tracked by B7 | `4bbecb48`; [Home integration](../../verification/RFC-BPR-0031-home-integration.md) |
| **B3 — Preferences.** Real Appearance controls apply/migrate/persist/reset motion and visibility through the current profile authority; rejected/stale writes and session-only fallback are truthful, same-profile fan-out works and other profiles remain unchanged. | Passed: the prior migration/refusal coverage now reaches actual Dif; real Settings persists/fans out within a profile, stays isolated across profiles, and restores after restart | `4bbecb48`, `b299818e`; [runtime ledger](../../verification/RFC-BPR-0031-runtime-foundation.md), [Home integration](../../verification/RFC-BPR-0031-home-integration.md) |
| **B4 — Observation.** Native System motion observation/change handling is proved per claimed target; unknown/unsupported resolves reduced, explicit Full is usable, and fixture/native claims and any changed OS setting are documented separately. | Partial: macOS Cocoa current read observed; injected change and shutdown pass; actual OS preference change/notification remains unproved | `e337c1fa`; [runtime ledger](../../verification/RFC-BPR-0031-runtime-foundation.md) |
| **B5 — Home.** Dif is reachable through the existing signed-out and signed-in Home, including Home covering an open review. Open/back actions, Recent/queue controls, compact layout, focus and return-to-reading state are unchanged. | Passed: the current Home owns the decorative slot; compact-first/expand, input focus and hidden-art checks pass, and Home covers/returns to an open review with comparison, selection, scroll, epochs and focus intact | `4bbecb48`, `b299818e`; [Home integration](../../verification/RFC-BPR-0031-home-integration.md) |
| **B6 — Context placement.** One explicit visible AI tools slot uses static baseline and short cues, never overlays code/shared comments or opens surfaces itself; one sprite per workbench, no duplicate active slot, and controls retain space/accessibility. | Passed: one checkable AI tools and Findings selector owns the 64 px contextual slot without opening its drawer; the single window widget moves between Home and AI, yields on narrow layout, stays static when reduced and remains outside focus/pointer paths | `ad0bb67d`; [context integration](../../verification/RFC-BPR-0031-context-integration.md) |
| **B7 — All-ten product paths.** Each clip is shown by its specified ordinary UI action or accepted projection. Actual replies/findings/draft acceptance drive ready/attention/ acknowledgment; no process-only success, AI author substitution or autonomous effect. | Passed: accepted Home covers rest/greet/calm/blink; typed post-presentation fan-outs cover all six contextual clips. The composed application courses directly preserve delivered content, prove pre-process failure presentation, and observe the matching cues with zero GitHub mutations | `4bbecb48`, `3a009291`, `ad0bb67d`, `8353a4fe`, `8512a3d8`; [Home integration](../../verification/RFC-BPR-0031-home-integration.md), [context integration](../../verification/RFC-BPR-0031-context-integration.md) |
| **B8 — Identity/dedup.** Duplicate, stale, replayed, switched-window/profile/PR/session, late invocation, rejected draft and superseded operation cases suppress incorrect cues; terminal coalescing and cooldowns never delay authoritative UI. | Passed: exact application/review/session/revision/generation admission, duplicate and stale suppression, pre-start failure admission, workshop revision/body fencing, explicit acceptance dispatch, bounded terminal priority and context-reset cancellation pass; cooldown changes playback only and stores no animation queue | `8353a4fe`, `8512a3d8`; [context integration](../../verification/RFC-BPR-0031-context-integration.md) |
| **B9 — Lifecycle.** Hidden, inactive, reduced, modal-obstructed, minimized, suspended and destroyed presentations have no continuing mascot timers/repaints/late callbacks. Resume is neutral with no queued cues or repeated greet. Shutdown remains orderly. | Partial overall, with Home and context milestone scope passed: hidden/reduced/deactivated/inactive-window paths stop, context change cancels its one-shot synchronously, hidden terminal results do not replay, and resume has no queued cue or repeated greet. Fixture modal/minimized/suspended/destruction coverage remains valid; later native lifecycle qualification remains open | `e337c1fa`, `4bbecb48`, `b299818e`, `8353a4fe`, `8512a3d8`; [runtime ledger](../../verification/RFC-BPR-0031-runtime-foundation.md), [Home integration](../../verification/RFC-BPR-0031-home-integration.md), [context integration](../../verification/RFC-BPR-0031-context-integration.md) |
| **B10 — Performance.** One decoded retained catalog is shared across windows/profiles; measured pixels/overhead/conversion peak are bounded, no per-tick image allocations or layout/classification changes, and a focused comparison shows no diff-work regression. | Partial overall, with accepted shared-cache scope passed: one lazy decode serves multiple windows and three profiles; 23,855,104 pixel bytes, 23,872,384 accounted retained bytes and 26,196,828-byte conversion peak are bounded, and retained comparisons survive Home. Native backend memory remains for later qualification | `4bbecb48`, `b299818e`; [Home integration](../../verification/RFC-BPR-0031-home-integration.md) |
| **B11 — Rendered/accessibility evidence.** Actual app frames at shipping scales and supported appearance modes demonstrate readable alpha edges, calm motion, static reduced interval, noninterference with focus/status and approved visual integration. | Partial overall, with Home and contextual layout scope passed: accepted frames render at 1.0/1.25/2.0, Home motion and static reduced intervals pass, the AI slot remains outside focus/pointer paths, and its control retains an accessible name. Final supported-appearance/native rendered acceptance remains BPR-0768 | `4bbecb48`, `b299818e`, `ad0bb67d`; [Home integration](../../verification/RFC-BPR-0031-home-integration.md), [context integration](../../verification/RFC-BPR-0031-context-integration.md) |
| **B12 — Complete handoff.** Focused tests, resource packaging, platform claims, source/ mutable docs, shared-skill consumer context and verification records agree. All ten clips are real and reachable; unavailable evidence/formal closeout remains honestly open, with no fabricated native/art verdict or forced rewriting of settled RFCs. | Partial: intake, production resources, runtime, Home and contextual routes are recorded and all ten real clips are reachable. Native qualification, final supported-appearance evidence, packaging claims and formal closeout remain open | `57ced605`, `98841656`, `e337c1fa`, `b299818e`, `8353a4fe`, `8512a3d8`; [live import](../../rfcs/assets/bpr-dif-integration-v1.LIVE_IMPORT.md), [context integration](../../verification/RFC-BPR-0031-context-integration.md) |

## Current frontier — context integration accepted, verification closeout next, dif-takeover-20260914 d13 / mandate r7

Dispatch `d13`, producer `rfc-integrator-d13`, completed the authored
`context-integration` milestone on accepted intake `1c0d0c65`, runtime
foundation `3036364c`, production art `d7fc946c`, and Home integration
`9c9f6cde`. BPR-0763, BPR-0764, and BPR-0765 are Done. Source revisions
`3a009291`, `ad0bb67d`, `8353a4fe`, and the reviewed correction `8512a3d8`
add the bounded typed cue router, one
explicit contextual AI slot, exact revision-aware identity/dedup, and a
composed application course. Card acceptances are `290ac785`, `857f17b0`, with
BPR-0765 rebound by the final milestone record; the
[context integration ledger](../../verification/RFC-BPR-0031-context-integration.md)
retains commands, observed behavior, baseline limitations, and exact revisions.

The corrected seven-check network-isolated course passes: the cue router,
review-draft editor, 1.0/1.25/2.0 workbench courses, the composed finding-to-
workshop-to-local-draft path, and real workspace materialization failure. All
six contextual clips have application routes
after accepted presentation facts, and the four Home clips retain their prior
accepted routes. Dif remains presentation-only: hidden, stale, duplicate,
superseded, mismatched, switched-context, conflicted, and process-only outcomes
cannot create an incorrect cue or suppress the authoritative UI. Context reset
cancels one-shot playback, cooldown stores no queue, and no GitHub/GHES mutation
occurred.

No new human art review was needed because accepted pixels did not change.
Actual operating-system motion-setting delivery, native backend memory,
supported-appearance/native rendered qualification, packaging claims, and
formal sequential RFC closeout remain open. The next authored milestone is
`verification-closeout` (BPR-0766/0767/0768/0769); it must preserve the exact
immutable RFC base and carry unavailable native or human evidence honestly.

## Previous frontier — Home integration accepted, context integration next, dif-takeover-20260914 d12 / mandate r7

Dispatch `d12`, producer `rfc-integrator-d12`, completed the authored
`home-integration` milestone on accepted production revision `d7fc946c` and
runtime foundation `3036364c`. BPR-0761 and BPR-0762 are Done. Source revisions
`4bbecb48` and `b299818e` select the accepted catalog in normal Home, project
real Settings preferences, retain reading state, and prove shared cache/profile/
lifecycle behavior. Card acceptances are `cc9c97d6` and `dbbaafc1`; the
[Home integration ledger](../../verification/RFC-BPR-0031-home-integration.md)
retains commands, observed values, limitations and exact revisions.

The corrected seven-check network-isolated course passed after independent
review. Both tests-enabled and tests-disabled Qt applications compile exactly
the accepted ten strips; core-only stays clean. Actual Home greeting, breathing,
four-loop blink and static rest pass at 1.0/1.25/2.0. Home/input/return-to-review,
same-profile fan-out, cross-profile isolation/restart, one lazy decode/cache and
inactive timer suppression pass. The reviewer found eager decode and active
window-state defects; both were fixed with negative checks, and its final report
has no remaining finding.

No GitHub or GHES mutation occurred. No attended session was needed because the
accepted pixels and prior human art verdict were unchanged; this milestone added
runtime composition evidence. Actual OS preference switching, native backend
memory, contextual routes, final supported-appearance/native rendering and
formal RFC closeout remain in later authored milestones. The next combined
milestone is `context-integration` (BPR-0763/0764/0765), which must route every
contextual clip through the existing explicit AI UI without making Dif
authoritative.

## Previous frontier — production art accepted, home integration next, dif-takeover-20260914 d11 / mandate r7

The d11 integrator consumed the exact hand-back from attended session
`7e489c0dd362c256bc28715a16892b65`. Twelve explicit human accept decisions at
sequences 111–122 bind the changed neutral, every declared-speed clip and the
exact derivative disposition; sequence 123 reports `pending: []`. The
[sanitized verdict](../../verification/dif-production-art-v008/review/human-verdict.json)
retains every criterion, fingerprint and decision ID. The loopback host is
stopped, the watch exited, and the durable external session and protected
review package remain historical evidence.

Production commit `98841656ebde50cff92a74a955651f9d1aa9492f` promotes the
byte-exact v008 result to `src/adapters/qt/assets/dif/`. A fresh reconstruction
matched all 85 managed source/manifest/generated files. The production schema-2
catalog has ten clips, 71 substantive frames, 20 transparent gutters and
23,855,104 decoded RGBA bytes. Direct validate/verify, dev and core-only
57-test contracts, narrow catalog compilation and the selected
dev/core-only/tests-disabled application boundary pass. Known missing external
generator metadata remains disclosed; sequence 122 supplies the required exact
distribution/notice disposition without inventing it.

BPR-0755, BPR-0756 and BPR-0757 are Done. The
[milestone acceptance](../../verification/dif-production-art-v008/milestone-acceptance.json)
binds their final fingerprints and both authored `production-art` exits. The next eligible combined
milestone is `home-integration`, because `runtime-foundation` is already
accepted and its fixture-backed runtime can now select the real production
catalog. That work must prove normal Home/Settings greeting, breathing, blink
and static rest plus retained review/layout/profile/cache/inactive-timer
behavior. It owns normal application resource selection and native Qt
presentation; this production-art milestone does not claim those B1/B2/B5/B9–
B11 results or formal RFC closeout.

## Previous frontier — bounded art retouch, dif-takeover-20260914 d8 / mandate r5

The `production-art` milestone remains partial and blocked by unqualified art.
The d8 work starts from main checkpoint `95c40fa35bc676b82f6f78c236031816f74c6738`
and retains the offline v006 studies; no production source, manifest, generated
runtime catalog or normal app selection changed. BPR-0755 stays In Progress;
BPR-0756 and BPR-0757 stay Backlog behind their listed dependencies. The
accepted neutral exact bytes and shared attended decision sequence 379 are
unchanged. Both authored production-art exits remain pending.

One new [greeting input and eight-frame local study](../../verification/dif-production-art-v006/greet/README.md)
preserves exact neutral head/chin, opposite body, hips/feet, alpha, baseline
and endpoints. The bridge-to-open visible limb-area increase improves from
17.5% to 6.3%, but the near-vertical shoulder root and disappearing G1
underarm contour remain visible at shipping sizes, so the row is **rejected**.
The [six extended-donor graft screen](../../verification/dif-production-art-v006/extended/README.md)
finds useful E3/E6 paw material but demonstrates failed E1/E2/E4/E5 eye,
height and head grafts; none is a qualified entry/peak/return sequence. The
[blink continuation](../../verification/dif-production-art-v006/blink/README.md)
provides a six-frame local eye-only candidate with improved static lid material,
exact neutral body/alpha locks and true-size theme plates. Its timed visual
playback, native Qt behavior and human art verdict are still unobserved; it
has not entered production source.

Only static 64/72/96/256-pixel studies were inspected. A local timed HTML
player was prepared for greeting and blink, but the subagent browser blocked
the `file:` URL under its URL policy; no alternate browser route was used.
There is no actual-speed visual pass, complete 71-frame catalog, final source/
digest record, fresh Dif-only art/derivative decision or native Qt presentation.
The concrete next artwork is a single consistent rounded G1/G2 shoulder,
elbow and forearm with exposed-belly occlusion, plus corrected six-gesture
intermediate/return poses. The six-frame blink study can be reconsidered only
with timed inspection and complete-catalog review. Bounded image-edit and
graft attempts did not reach a production-art exit; do not fabricate catalog
frames from the rejected trials.

Native d8 dispatched `rfc-worker-deep` for blink material and `rfc-worker`
for the six extended donors with `fork_turns: none` and no optional model/
effort override; the integrator retained greeting, board and closeout.
Configured roles were `gpt-6-astra`/`high` and `gpt-6-astra`/`low` respectively;
their banners are not serving telemetry or proof of enforced child-role
allowlists. Task writers/commands are accounted for at the d8 handoff.

## Previous frontier — source-grounded pose prompts and edited candidates, dif-takeover-20260914 d7 / mandate r5

The user has no authored source poses and asked for detailed prompts for the
missing artwork. The prior offline image-edit authorization remains in force.
The integrator retained the [copyable exact-source prompt brief](../../verification/dif-production-art-v005/PROMPTS.md)
and made nine separate built-in image edits: a partly opening greeting bridge,
open-paw peak, closed-blink lid and one pose input for each of six other
extended gestures. The [input ledger](../../verification/dif-production-art-v005/generated-inputs/README.md)
records the actual reference roles, prompt blocks, native 1254-square output
dimensions and SHA-256. The exact accepted neutral and immutable incoming
sheets remain unchanged; no generated full body is accepted.

The [v005 greeting study](../../verification/dif-production-art-v005/study/README.md)
reconstructs distinct partly/open fingers with exact 17,539-pixel head/chin
lock but is **rejected** for the shoulder/elbow mismatch: an 87-pixel inner-root
wedge vanishes at the bridge-to-peak handoff and limb area changes 17.5%.
The [v005 blink study](../../verification/dif-production-art-v005/blink-study/README.md)
keeps outside-eye RGBA and alpha exact but is **rejected** for a pale lower-eye
seam and uneven purple shading at 256 pixels. The
[six-input extended screen](../../verification/dif-production-art-v005/extended-study/README.md)
rejects every full output for direct promotion: one-eye glance, wrong-height
consider/attention, unproved nod, opposite blocked tilt and broad neutral
registration/foot drift. Some paw shapes are useful donor material; no local
composite or timed sequence has passed. The accepted neutral decision, v002 partial rest/idle,
provisional v004 blink and rejected v003/v004 greetings remain historical
evidence. No new source, complete catalog, runtime resource or final review
candidate exists. BPR-0755/0756/0757 and both production-art exits remain
open. The final Dif-only review still requires a complete catalog and fresh
external `--state`, without rewriting the shared session's earlier decisions.
An independent read-only `rfc-reviewer` found no blocking changed-surface
issue after checking nine retained output hashes/bytes, dimensions, greeting
fault controls, blink masks and the pending extended screen. The reviewer did
not observe invocation transcripts, timed playback, native Qt or a human art
verdict, so those claims remain pending. The integrator adjudicated that
review as consistent with **no asset promotion**.

Native d7 leaf dispatch reused the existing `rfc-worker-deep` greeting owner,
then selected `rfc-worker` for the disjoint blink and extended-input screens
and `rfc-reviewer` for the read-only integration review. New calls used
`fork_turns: none` and no optional model/effort overrides; follow-ups kept
their original slice owners. The generated route known for the two worker
roles is Codex `gpt-6-astra` / `high` and `gpt-6-astra` / `low`, and the
reviewer self-reported `gpt-6-astra` / `medium`. Those banners/configuration
are separate from the dispatch arguments and are not effective serving
telemetry or proof of enforced child-role allowlists. All leaf tasks and
background study commands returned quiescent; the integrator retains the
cards, catalog, acceptance and any future review.

## Previous frontier — authorized offline image-edit continuation, dif-takeover-20260914 d6 / mandate r4

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
change body scale/face and supply arm material only. The deep worker used one
further targeted image edit and returned the reproducible
[v004 greeting study](../../verification/dif-production-art-v004/README.md).
Agent inspection rejects it: a closed-to-open paw jump at frames 1→2, a
sharp raised shoulder join and changed translucent head-edge/chin pixels
remain. It is not promoted. The separate extended worker returned a
[seven-gesture study](../../verification/dif-production-art-v004/extended/README.md):
all raw full-body registrations fail, while its local-eye blink repair keeps
neutral alpha and non-eye pixels exact and is retained as **provisional** with
faint 256-pixel texture bands. Both workers and commands are quiescent.

The integrator retains cards, source/catalog, review and commits. V003 and the
other rejected studies remain historical evidence. BPR-0755 stays In Progress;
BPR-0756/0757 stay Backlog. Both production-art exits are pending. No new
human verdict, complete catalog or runtime asset exists. The next artwork
operation is a shoulder-fixed hand/forearm sequence with a partially opening
paw and exact head-edge/chin mask, followed by local retouch of the six other
extended gestures. Only a compiled complete catalog can enter the fresh
external-state Dif-only review. Historical shared-session answers remain
untouched.

Native d6 dispatch arguments selected custom `rfc-worker-deep` for greeting
and `rfc-worker` for extended art, each with `fork_turns: none` and no per-call
model/effort override. The installed generated routing configuration specifies
Codex `gpt-6-astra` / `high` and `gpt-6-astra` / `low`, respectively; both leaf
banners self-reported those pairs. The extended leaf reported routing as
unknown. Effective serving telemetry and runtime enforcement of child-role
allowlists were not exposed, so neither is claimed from the banners/config.

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
