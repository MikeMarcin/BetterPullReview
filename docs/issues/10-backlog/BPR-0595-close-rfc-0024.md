---
schema: "implement-rfc.card.v1"
id: "BPR-0595"
title: "Verify and close RFC-BPR-0024"
status: "Backlog"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0594","BPR-0593","BPR-0242","BPR-0690","BPR-0691","BPR-0693", "BPR-0694", "BPR-0695", "BPR-0696", "BPR-0697", "BPR-0698", "BPR-0699", "BPR-0700", "BPR-0701", "BPR-0702", "BPR-0703", "BPR-0704", "BPR-0705", "BPR-0706", "BPR-0707", "BPR-0708", "BPR-0709", "BPR-0710", "BPR-0711", "BPR-0712", "BPR-0713", "BPR-0714"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0024"]
---

# Verify and close RFC-BPR-0024

## Objective

Reconcile the complete accepted behavior in [RFC-BPR-0024](../../rfcs/RFC-BPR-0024-bpr-profiles-multiple-windows.md)
against direct evidence, all linked issue cards, and the final public path.
This closeout depends on the entry card now and must acquire all subsequently
allocated required slice dependencies before it can become startable.

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 2 of 4.
Planned cards: BPR-0594, BPR-0595, BPR-0690, BPR-0691, BPR-0693, BPR-0694, BPR-0695, BPR-0696, BPR-0697, BPR-0698, BPR-0699, BPR-0700, BPR-0701, BPR-0702, BPR-0703, BPR-0704, BPR-0705, BPR-0706, BPR-0707, BPR-0708, BPR-0709, BPR-0710, BPR-0711, BPR-0712, BPR-0713, BPR-0714. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

Owner activation is recorded in the canonical RFC index. The [authority
inventory](../../development/profile-window-authority-inventory.md) maps all
criteria and the first bounded foundations. Partial profile product checks are recorded below; full criteria remain open.
Canonical terminal prerequisites remain required.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: Default, profile creation/rename, profile identification, Open Profile, and New Window are reachable through the shipped application without replacing another profile's windows. | Public profile menu, create/rename, Open Profile and New Window course | Not run; planned | None |
| AC-02: Two profiles can concurrently use different accounts on the same server with isolated credentials, repositories, caches, epochs, private review data, and preferences. | Concurrent two-profile same-server fixture accounts and partition inspection | Not run; planned | None |
| AC-03: Windows in one profile share accepted review data and preferences while retaining independent open PRs, navigation, layout, and restoration records. | Two-window shared data and independent navigation/restore course | Not run; planned | None |
| AC-04: Shared draft/note edit conflicts retain the rejected candidate text and never silently overwrite accepted work; publishing still uses the existing exact-authority mutation path. | Concurrent expected-revision draft/note edits and existing publication-boundary checks | Not run; planned | None |
| AC-05: Default migration preserves existing appearance, sign-in bindings, private data, and retained history through interruption and restart without duplicate adoption or cross-profile fallback. | Seeded legacy Default migration with interrupted/repeated adoption and fake vault | Not run; planned | None |
| AC-06: Repeated launches admit one runtime owner per profile; explicit new windows and concurrent different profiles work, and closing one window leaves other consumers usable. | Repeated launches, explicit New Window, owner contention and consumer-aware close | Not run; planned | None |
| AC-07: Window restoration preserves independent sessions, distinguishes explicit window close from application quit, and exposes the retained Resume destination without fabricating unavailable content. | Quit/close/reopen multi-window sessions, retained Resume and unavailable-content cases | Not run; planned | None |
| AC-08: Settings and sign-out are scoped to the invoking profile, with one Settings window per profile and no effects or late completions reaching another profile. | Same-profile Settings singleton and sign-out with stale cross-profile completion attempts | Not run; planned | None |
| AC-09: Typed core/host/Qt boundaries, bounded persistence, stale-result rejection, and failure isolation hold through the composed multi-window application path. | Core replay, manual-executor host and composed multi-window failure/identity tests | Not run; planned | None |

## Completion rules

- Replace planned checks with exact executed commands/courses, results, and
  producing commits as implementation completes; retain not-run rows honestly.
- Inspect all cards linked to this RFC, not just the initial planned pair.
  Every required card must be done and every canonical criterion satisfied.
- Exercise the selected public path and check critical changed-surface findings.
  Use focused evidence; do not infer a full matrix requirement from this card.
- Record target-native or attended evidence in the project's existing workflow.
  Park pending attended work and continue independent work; no approval or live
  GitHub authority comes from registration.
- Run the documentation structure gate and `git diff --check`, reconcile RFC
  and index status, and close only when canonical order and all dependencies
  permit Auto-Verified. No status changes are authorized by this registration.

## Resume

BPR-0594 records the live authority inventory. BPR-0690 and BPR-0691 are the
first independent foundations alongside remaining Settings integration. Registry
and credential isolation are committed; BPR-0694 owns private root admission and
BPR-0695 will construct the selected runtime asynchronously. Shared services,
profile management, shared-edit conflicts, restoration and process admission
remain later work. Native and human criteria remain open; no full profile RFC
acceptance or closeout is claimed.

## Foundation evidence

BPR-0690 committed6c91f007 introduces only pure typed lifecycle policy.
Core-only configure/build and focused profile test pass; both architecture
checks passed after the setup parser boundary repair. The broader run had
125/128 passes; the two corrected boundaries passed narrow rechecks and an
unrelated native capsule cleanup timeout remains tracked in BPR-0692. Do not
report an entirely green core-only suite. Registry persistence passed focused host/storage contracts in BPR-0691
(commit22e88f0f), including bounded preflight and interrupted/CAS adoption.
BPR-0693 (commit69d4b587) isolates profile credentials and schema2 remembered
bindings while preserving Default vault namespaces. Host/storage/shared-authority
checks pass, and injected platform contracts compiled/executed successfully on
macOS, native Windows and Ubuntu WSL. Independent review findings in both
preflight paths were corrected and narrowly rechecked. No live vault operation
or full native application course was used.
Profile/window UI and shared services are not implemented by these foundations.

BPR-0694 committed97bfa79f establishes immutable registry-derived private roots;
BPR-0695 committed47de7ae8 integrates asynchronous exact-profile startup. Its
public course proves two-profile appearance/root isolation, rename/reopen,
independent close, no unknown/cancelled window, unchanged unsafe-root refusal
and optional queue failure isolation. Eight migrated courses and the isolated
tests-disabled Release contract pass. BPR-0696 now owns the management/menu and
desktop owner; BPR-0697 starts the auth/read split needed before same-profile
multiple windows. Full multi-window criteria above remain unverified.

BPR-0696 adds the actual profile menu, durable Create/Rename, Open/activate,
and independent desktop lifetime. Its rendered public course passes including
initial-profile closure, private preference reopen, Quit and last-management
close. The executable shutdown course passes. This covers the different-profile
portion of AC-01/06; same-profile New Window, shared services, restore and process
owner admission still require implementation and evidence.

BPR-0698 independently separates profile appearance preferences from window
classification alongside BPR-0697. Shared Settings and actual same-profile
window composition remain later slices; no full AC-03/08 acceptance is claimed.

BPR-0697 separates the actual GitHub PR read lane from profile authentication.
Focused shared-session/churn/reauthentication/shutdown tests and the reentrant
replacement regression pass; rebuilt setup and anonymous public paths pass.
This is the auth/read portion of AC-03/08/09, not complete multi-window UI.

BPR-0698 separates shared accepted appearance preferences from each window's
classification and native observation. Existing/new host tests, independent
review and composed appearance persistence/Settings effect checks pass. Actual
shared Settings presentation and multi-window runtime ownership remain open.

BPR-0699 owns bounded shared queue consumers; BPR-0700 separates profile
behaviour/defaults from window-keyed layout persistence. These prevent duplicate
queue owners and whole-record layout overwrites before actual multi-window
composition. They do not implement restoration or shared-edit conflict policy.

BPR-0701 fixes the logging adapter's existing single-backend-owner refusal so
concurrent different profiles each retain diagnostic logging. Same-profile
logger sharing remains part of runtime composition.

BPR-0699 introduces bounded queue consumers and singular fresh notification
observation; its composed queue course passes (3.68s) after reviewed reentrancy
fixes. BPR-0701 restores concurrent private profile logging with shared backend
lifetime; logging/backpressure tests (0.61s) and the three-profile public course
(1.38s) pass, including real overflow isolation. These are bounded service
foundations; the full multi-window criteria above remain open.

Next bounded services: BPR-0702 shares cache actors while preserving window
epoch warrants; BPR-0703 adds durable private-note expected-revision conflicts.
Shared editable drafts and their conflict UI remain separate implementation work.

BPR-0700 separates accepted profile behaviour/defaults from bounded schema5
window layout rows. Legacy migrations preserve defaults without inventing rows.
Focused host/consumer/storage tests, composed workbench course (25.18s), strict
mutation-source inventory (0.727s), tidy and documentation gate pass. Reviewed
refused-attachment authority failure is corrected with zero-write regressions.
Current app explicitly uses defaults view until actual window binding is integrated.

BPR-0704 owns shared in-memory draft revision admission and preserved editor
conflict candidates. It adds no new durable editable-draft storage.

BPR-0703 adds schema6 private-note expected-revision CAS and protects the first
rejected candidate until explicit dismissal. Two-connection race/ABA/migration
and hostile-observation tests pass; composed ledger40.86s and agent invocation
0.40s pass. Review finding on Current validation is corrected and narrowly
rechecked. Private-note editor UX is not claimed; shared drafts remain BPR-0704.

BPR-0702 shares profile cache actors while retaining window warrants/jobs. Exact
requests switch only through the shared FIFO; tests prove no overlapping session
and independent A/B/A selections, queued sibling survival and final destructor
drainage. Epoch host0.61s and public persistence seed/reopen52.01s pass; tidy
passes and independent review found no blocker. Actual multi-window composition
remains open. Public fixture root preparation now honors owner-private admission.

BPR-0704 integrates scope/revision-aware accepted drafts and protected editor
candidates into the current facade. Same-text acknowledgement and reentrant
admission review findings are corrected; detached publication returns to drafting
for surviving consumers. Core/host/Qt tests, all changed-TU tidy, both architecture
checks and the real editor/publication course13.12s pass. That course proves stale
revision/foreign scope never reach the mutation host. Dedicated exhaustive
isolation77.84s passes after building eleven missing dev artifacts; its duplicate
invocation was removed from the shorter editor course, preserving source guards
and independent isolation coverage. Actual multi-window construction is next.

BPR-0705 completes live New Window composition with one profile runtime and
independent window sessions. The rendered two-window course passes70.93s,
including close-during-sibling-read, fresh context, shared drafts and one-owner
Quit. Existing diff14.70s, ledger40.16s, epoch51.86s, appearance84.70s,
setup7.37s/startup1.87s/management1.80s pass. Corrected standalone queue activation
passes4.20s and exact review roundtrip13.49s. Independent medium review's lifetime,
auth-rebinding and fixture-scan findings are repaired and narrowly rechecked.
Strict bypass0.761s, architecture8.17s/80.83s, canonical tidy, tests-disabled
Release151.50s, dedicated mutation isolation81.21s and documentation gates pass.
Rendered window-a/window-b PNGs remain in build/dev/profile-multi-window-review.
Durable window destinations/restoration, cross-process admission and same-review
ledger projection fanout remain subsequent required work; native criteria remain
open. No provider mutations or live vault operations were used.

Restoration preparation during BPR-0705 confirmed that core lifecycle policy
already distinguishes explicit close from quit and increments generations on
restoration. Workbench schema5 already supports bounded window layout rows;
those rows must remain free of review content. The next bounded persistence
slice must retain the window allocation high-water mark and focus/retained order,
plus private per-profile destinations with exact comparison identities. Do not
serialize runtime leases, provider snapshots, request generations or credentials.
Default has no legacy active-review record: missing session storage must not
invent a PR or Resume target. The eventual restart course must cover quit versus
close, independent layouts, stale generations and unavailable comparisons without
substitution. Storage/API design and integration remain unimplemented.

Next: BPR-0706 adds durable window identities to schema2 of the existing global
registry CAS. It deliberately excludes private destinations and runtime lifecycle
admission; the subsequent host slice must save before start/close/quit effects.
This keeps allocator corruption distinct from optional restoration-payload errors.

Lifecycle integration preparation: current management emits only the activated
window from `launch.Created`, so restoration must emit each admitted context and
preserve the core least-to-most-focused order. The desktop already queues window
requests while a profile owner starts. Native close currently calls runtime close
immediately; it must instead hold the close event until an exact durable-close
command arrives. User Quit similarly requires a committed retained set before
runtime teardown, with storage failure keeping current windows usable. Forced
lifetime cleanup must remain distinct from successful orderly-Quit acceptance.
Registry profile edits must not erase durable window metadata, and synchronizing
accepted metadata must not duplicate retained IDs that are already live.

BPR-0706 completes schema2 durable window metadata in the existing registry CAS.
Application0.38s, SQLite0.59s and management0.35s pass, including schema1 preservation,
full64×32 bounds, identity/generation/ownership refusals and six transactional
interruption points. Canonical tidy passes and independent medium review found no
material finding. Native and full restart criteria stay open. BPR-0707 is reserved
for the required asynchronous lifecycle admission and desktop integration.

BPR-0707 integrates durable lifecycle admission before start, Close and Quit.
The four-process actual desktop course passes (6.78s final), including two-profile
retention, MRU generations, explicit Close versus Quit, global high-water and
real SQLite-lock failure/reload recovery. The read host now admits bounded
startup restoration in focus order while retaining stale-ID refusal; direct
host regression passes (0.31s final), independent medium review and strict source/
bypass guards pass. The inspected restored Home raster is retained under
`build/dev/profile-window-restart-review/`. This is identity/count/order
acceptance, not restoration of discarded private review contents.

Next bounded vertical slice: persist a profile-private active PR destination
under stable window ID, restore through the exact window read host and bind
already-supported window layout rows. Resolve only an existing configured server
and accepted profile authentication; never start setup, fabricate Resume or
substitute unavailable content. Comparison/file and semantic reading-location
restoration then need their own bounded acceptance. Cross-process activation,
same-review ledger fan-out and full two-account isolation remain open.

BPR-0707 live multi-window recheck passes70.43s and management1.47s; final
focused host regression also seals the legacy single-window alias after
restored ID1 retirement. Changed-TU tidy and documentation pass.

BPR-0708 is the next bounded vertical slice: bind existing window layout rows
to actual stable desktop IDs, flush before Quit and clean closed/orphan rows.
Private PR destinations and comparison/location restoration follow separately.


BPR-0708 integrates independent drawer layout persistence through the actual
desktop: lazy effect-local stores, complete-set orphan reconciliation, final
accepted-write flush on Quit and exact row erase after Close writers drain. The
four-process rendered public course passes18.96s, including fresh profile defaults
and an unsupported optional layout schema that remains intact while its visible
error leaves windows/allocator usable. Direct host0.41s and storage0.51s,
changed-TU tidy, independent medium review, strict bypass0.795s and tests-disabled
Release135.177s pass. Actual fresh windows now explicitly retain Home during a
layout load. Two inspected rasters remain in `build/dev/profile-window-layout-review/`.
Full PR/comparison/location and native-geometry restoration remain open; the next
vertical slice is the retained active PR destination through exact window reads.

Final existing lifecycle checks pass: live multi-window70.36s (pending sibling
read across Close), durable identity restart7.03s and management1.93s.

BPR-0709 is planned for accepted PR destinations, deterministic one-profile
startup authority, authenticated/anonymous reopen and stale-navigation refusal.


BPR-0709 is landed as an incomplete checkpoint at the user's request to stop.
Core, asynchronous host, SQLite and desktop restoration wiring are present;
focused core/host/storage/GitHub-host checks and independent medium review pass.
Its card remains In Progress pending actual authenticated/anonymous restart
acceptance and failure-path evidence. No RFC completion is claimed. Continue
from that card rather than starting the next UX slice.

## Prior continuation — 2026-09-08, D1

Dispatch UX-2026-09-08-D1, mandate 1 completes BPR-0709's accepted-destination
milestone at source/test candidate `ce63c67b1e82987a6cbec7ac5b60fc7f47300917`.
The fifteen-process course passes15.70s; Home and new navigation defeat held
restoration terminals while preserving prior accepted destinations. Authenticated
restart revalidates retained fixture grants without enrollment; anonymous restart
has zero raw vault/account/bootstrap attempts. Missing grant/server/read,
corrupt/future storage and GHES preview refusal leave usable Home without
fallback or destination loss. The card carries exact local evidence paths and
the bounded review's corrected instrumentation finding. No native or full
selected-comparison acceptance is inferred; the anonymous fixture's unselected
comparison is already unavailable before restart.

BPR-0683 is now parked for its actual native/attended requirements (commit
`93c3e182`); RFC-0028's three released attended candidates are waiting with no
verdict and need preparation again. Its superseding Home/layout direction and
unscheduled browser-style file tabs remain authoritative. Required Windows,
screen-reader, packaged inventory and canonical closeout dependencies stay open.

The next bounded objective is to inventory and implement exact per-window
comparison/file/semantic-location restoration under RFC-0024, allocating and
claiming its card before source changes. Resolve retained identities through
existing epoch/diff authority and present unavailable content explicitly; never
substitute a comparison or schedule browser tabs. Cross-process runtime admission,
same-review ledger projection fanout, complete two-account isolation and native
geometry remain required later work. No further card was started in this milestone.
Writers and commands must be quiescent before the next integrator takes ownership;
this local continuation grants no push, live provider mutation or publication.

## Prior continuation — 2026-09-08, D2

Dispatch UX-2026-09-08-D2, mandate 1 completes [[BPR-0710]] at source/fixture
candidate `60da0aee08da919c4df7e98395b1824eee238bfb` and final test candidate
`377f43523630a0c8d304d1c377c32dbff2fc8468`. Independent windows now retain exact
comparison anchors/OIDs, file and semantic selection across Quit/restart.
Version-one PR-only rows migrate transactionally; unavailable identities/content
fail explicitly without replacing the saved pair or selecting a nearby line.
Actual installed-Git processes prove distinct files and cross-upload pairs from
two sibling upload heads, including an older head absent from the replacement
pack. Full31 passes790.66s; final nine changed/unchanged/Home race processes pass
after the Qt notification fix. Registered34 is not claimed as a full34 run.
The card records focused core/host/storage, Qt, Release absence/inventory,
independent review and the six integration outcomes with exact local logs.

Important boundaries: missing catalog rows mean absence rather than retryable
authority mismatch; same-file restoration must request a diff refresh; accepted
PR replacement rebinds ownership after invalidation; Home suppresses late saves;
cancelled comparisons retire exact request/generation; unchanged native gestures
retain their position without an external selection notification. Controlled
public event ordering proves cancellation, not physical race timing. Copied
inode-bound caches are unavailable evidence, not successful restoration roots.
The multi-window integration wait now observes physical session retirement,
not merely hidden UI, and passes61.18s; production status semantics are unchanged.

Anonymous destination evidence still has the existing unselected-comparison
limitation. Held-comparison cancellation has direct core evidence, not a held
public comparison course. Native geometry, Windows/screen-reader/packaged
inventory, complete two-account isolation and same-review ledger fanout remain
open. BPR-0683 remains parked at `93c3e182`; its three released attended
candidates have no verdict and need fresh preparation. BPR-0593/BPR-0242 remain
canonical terminal dependencies. RFC-0028 Home/layout authority and unscheduled
browser-style file tabs are unchanged; no RFC or pack closure is claimed.

Next bounded objective: inventory and allocate/claim a host-owned cross-process
runtime admission slice under RFC-0024 AC-06 on available targets. Demonstrate
one profile owner across repeated launches, safe activation or explicit
new-window handoff and independent different-profile ownership; do not infer
native packaged acceptance from a fixture. Preserve the remaining fanout,
two-account and native obligations. No next card was started here. Task writers
and commands are quiescent before transfer; this continuation grants no push,
live provider mutation, credentials or publication authority.

## Prior continuation — 2026-09-08, D3

Dispatch UX-2026-09-08-D3, mandate 1 completes the bounded native ownership
foundation [[BPR-0711]] at source/test candidate
`3e7a26d61c1fed4d146f88d8c88b08f0e489213b`. Startup holds an exclusive native
profile-root lease before queue/domain services. Open Profile admits the runtime
before saving window generations; failed-start retry cannot bypass ownership.
The lease outlives profile workers/services, and startup completion is one-shot
even through shutdown on libc++. Native contention, independent roots, process
death and exec inheritance have macOS evidence. Six real process invocations
prove duplicate startup and synchronous retry leave the existing owner's durable
windows unchanged while a distinct profile runs. All 11 integration checks pass
174.06s; strengthened live-process lifetime check passes1.76s. The card records
critical-review corrections, final format/tidy, Release inventory and doc gates.

The full requested admission/activation objective was split at this demonstrable
safety boundary: there is no existing activation IPC to reuse, and both runtime
construction and durable window allocation first needed the ownership guard.
Next bounded objective: inventory/allocate/claim the narrow cross-process
activation/new-window handoff under AC-06, carrying stable profile identity and
only the two admitted intents. An ordinary launch must activate the owner's MRU
window; explicit New Window must route without creating a competing runtime or
mutating the sender's window registry. Preserve bounded authentication/framing,
owner-loss/closing outcomes, no timeout bypass and direct actual-process evidence.
The current duplicate-launch result is `PROFILE_ALREADY_OPEN`, not activation.
Different-profile registry CAS conflicts still use explicit Reload before retry.

Anonymous selected-comparison limitations, native geometry, Windows/accessibility/
packaged inventory, complete two-account isolation and same-review ledger fanout
remain open. BPR-0683 remains parked; RFC-0028's three released attended candidates
still require fresh preparation and actual verdicts. BPR-0593/BPR-0242 remain
canonical terminal dependencies. RFC-0028 Home/layout authority and unscheduled
browser-style file tabs are preserved. No RFC or pack closure is claimed.

The named rfc-integrator role was unavailable; the coordinator explicitly
designated this native integrator with requested Astra low. Bounded native worker
and critical-review routes were used without changing that mandate. All task
writers and commands are quiescent at transfer. Logs remain in ignored build
directories; no worktree or external mutation was created. Local continuation
grants no push, live GitHub mutation, credentials or publication authority.

## Prior continuation — 2026-09-08, D4

Dispatch UX-2026-09-08-D4, mandate 1 completes [[BPR-0712]] at implementation
candidate `a654669c7475d5b0254e071bbbdd25b3e4dfc20b`. On supported POSIX roots,
ordinary launches activate the existing owner's MRU window and explicit
`--new-window` launches allocate through that owner's durable management host.
`--bpr-profile ID` selects stable identity. The sender has no runtime/window;
fixed bounded authenticated framing carries no credentials or provider command.
Lost replies remain indeterminate without automatic retries or ownership bypass.

Native transport/lease tests pass2/2 in4.85s; the final actual-process activation
course4.93s and owner course2.18s pass with zero fixture provider mutations.
Existing startup1.71s, management1.63s, live multi-window62.42s, restart6.54s
and management-host0.07s pass. The initial integration listener-ban failure is
corrected by an exact local-only architecture exception; architecture7.25s and
four discriminating exception cases pass. Final format/tidy, tests-disabled
Release inventory, documentation and regression evidence is recorded on the card.

Critical review's synchronous-busy reply leak is corrected. Its trigger/recovery
has direct management-host evidence and the reply correction has source review;
the exact composed post-delivery busy sequence remains unexercised. The public
Close course proves pre-admission refusal during command delivery instead.
Do not reinterpret that course as a held shutdown/read-terminal regression.
Windows activation transport and overlong POSIX socket addresses remain explicitly
unavailable; native packaged focus and full AC-06 remain open.

Next bounded objective: inventory and implement same-review private-ledger
projection fanout across sibling windows under AC-03, allocating/claiming its
card first. Accepted receipt/note updates must reach another view of the exact
same review without changing its PR, epoch, file, selection or scroll position;
preserve exact profile/account/repository/PR authority and stale-result refusal.
Complete two-account isolation, native geometry, anonymous comparison limits,
Windows/accessibility/package evidence and canonical BPR-0593/BPR-0242 dependencies
remain outstanding. BPR-0683 and RFC-0028's three released attended candidates
still require fresh preparation and actual verdicts. RFC-0028 Home/layout and
unscheduled browser-style tabs remain authoritative; no RFC/pack closure is claimed.

All task writers and commands are quiescent at transfer. No worktree or external
mutation was created; logs remain ignored under build directories. Named integrator
adapter and opposite-family review were unavailable; requested explicit native
routes remain recorded without claiming serving telemetry. No push, live provider
mutation, credentials or publication authority is granted by this continuation.

## Prior continuation — 2026-09-08, D5

Dispatch UX-2026-09-08-D5, mandate 1 completes the bounded receipt and
note/overview fanout card [[BPR-0713]] at implementation candidate
`78d4fa5ca93b88e107d7c31c86a8d8f98d459ae4`. Accepted store writes coalesce onto
the profile decision lane; exact durable review and account authority admits
sibling refresh without transporting navigation state or candidate prose.
Receipt reload does not write carried material. Selected-diff invalidation
clears old retained receipts, and receipt changes refresh actionable order even
when a sibling views a different file. Rejected note candidates remain separate.

Final real two-file multi-window course passes83.73s with distinct selection/
scroll preservation, receipt and note/overview fanout, different-PR isolation,
pending-comparison receipt refusal and unselected actionable-file restoration.
Affected final composition/inventory checks pass2/2 in37.01s. Host tests,
architecture integration, format/tidy, tests-disabled Release inventory and
documentation evidence are recorded on the card. Review's stale-receipt finding
is corrected and directly exercised; narrow actionability recheck found no
demonstrated blocker. Two rendered historical epochs, queued Close/account
replacement and full rendered conversation/defect fanout are not claimed.

Next bounded objective: inventory and allocate/claim the remaining same-review
conversation and defect projections under AC-03. In particular, the window's
`DurableDefects` cache is not reloaded by the new invalidation. Demonstrate
accepted conversation/defect updates in independent views and reject stale
delivery; add bounded cross-comparison evidence where needed. Do not infer this
work is complete from receipt/note fanout or a cache-invalidation host test.

Windows activation transport, long POSIX socket paths, native geometry/focus,
package/accessibility evidence, complete two-account isolation and the anonymous
selected-comparison limitation remain open. BPR-0593/BPR-0242 remain canonical
terminal dependencies. BPR-0683 is parked and RFC-0028's three released attended
candidates need fresh preparation and actual verdicts. RFC-0028 Home/layout
authority and unscheduled browser-style tabs are unchanged. No RFC/pack closes.

Named integrator and opposite-family review adapters were unavailable; the
coordinator explicitly designated the requested Astra-low native integrator.
Native workers and critical review were used without claiming serving telemetry.
All task writers and commands are quiescent at transfer. No worktree or external
mutation was created; logs remain ignored under build directories. This local
continuation grants no push, live provider mutation, credentials or publication.

## Current continuation — 2026-09-08, D6, mandate revision 2

M6 completes the bounded private conversation-receipt and durable-defect
projection slice [[BPR-0714]] at implementation candidate
`4961ff64499e494f703ca05d15039dc24c51db50`. Exact-authority invalidation now
reloads `DurableDefects` and presents accepted records without calling correlation
or writing against a recipient's file/epoch. Failed reload keeps the prior
accepted projection. Existing window/account-generation guards remain unchanged.

The final real multi-window course passes84.64s. Public Mark read reaches the
sibling; classification and critical reclassification retain identical defect
and available-context projections while the sibling reads another file;
comparison, selection and scroll remain independent. Queued withdrawal before
PR replacement cannot expose old defects. The final card records exact logs,
two full-window comparisons and supplemental renders of the actual defect cards.
Those cards are clipped below the full-window viewport, an existing layout
limitation that this slice does not accept as native visibility or accessibility.
Independent medium review found no source blocker; its availability-coverage
gap is addressed by the rendered context-label assertions. Direct write counters,
failed-reload injection and two rendered historical epochs are not claimed.

Architecture, ledger composition and development inventory pass; the older
defect composition course passes54.00s after its fixture root adopts the same
owner-only permissions as the existing ledger course. The updated reading
fixture uses a non-viewer synthetic commenter so unread behavior is meaningful.
The affected three-process seed/advance/restore regression passes; it does not
claim the full34 restoration matrix. Final format/tidy, tests-disabled Release
build/inventory and documentation/62-test regression evidence pass.

User mandate revision 2 says stop after M6. No next milestone is started or
claimed. On a later authorized invocation, the next bounded objective is to
inventory and allocate/claim server-conversation snapshot sharing under AC-03:
new/edited/deleted posts and resolve/reopen observations still belong to each
window read host. Preserve independent comparison/navigation and exact current
authority; private receipt/defect fanout is not proof of provider snapshot fanout.

Full two-account isolation, anonymous selected-comparison limitations, Windows
activation transport, long POSIX socket paths, native geometry/focus,
package/accessibility and canonical BPR-0593/BPR-0242 dependencies remain open.
BPR-0683 remains parked; RFC-0028's three released attended candidates require
fresh preparation and actual verdicts. RFC-0028 Home/layout authority and
unscheduled browser-style tabs remain unchanged. No RFC or pack closes.

Named integrator and opposite-family adapters were unavailable; the designated
native integrator retained requested Astra low and used native worker/reviewer
routes, without observed serving telemetry. All task writers and commands are
quiescent. No worktree or external mutation was created. Copied temporary renders
were removed after byte comparison; retained artifacts/logs live in ignored build
directories. This handoff grants no push, publication or live-provider authority.
