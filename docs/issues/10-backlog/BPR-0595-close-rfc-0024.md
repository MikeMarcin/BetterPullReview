---
schema: "implement-rfc.card.v1"
id: "BPR-0595"
title: "Verify and close RFC-BPR-0024"
status: "Backlog"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0594","BPR-0593","BPR-0242","BPR-0690","BPR-0691","BPR-0693", "BPR-0694", "BPR-0695", "BPR-0696", "BPR-0697", "BPR-0698", "BPR-0699", "BPR-0700", "BPR-0701", "BPR-0702", "BPR-0703", "BPR-0704", "BPR-0705", "BPR-0706", "BPR-0707", "BPR-0708", "BPR-0709"]
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
Planned cards: BPR-0594, BPR-0595, BPR-0690, BPR-0691, BPR-0693, BPR-0694, BPR-0695, BPR-0696, BPR-0697, BPR-0698, BPR-0699, BPR-0700, BPR-0701, BPR-0702, BPR-0703, BPR-0704, BPR-0705, BPR-0706, BPR-0707, BPR-0708, BPR-0709. Allocate additional implementation cards only
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
