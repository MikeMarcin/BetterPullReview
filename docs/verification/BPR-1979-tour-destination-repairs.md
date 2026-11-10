# BPR-1979 tour destination repairs

Mandate: `tour-destination-repairs-r1`; immutable baseline/RFC_BASE_SHA
`cf9413b9f76dd34e299bc46518a52127b18b175c`. The imported card and
[adopted plan](../rfcs/plans/tour-destination-repairs-v1/milestone-plan.json)
preserve the owner's complete repair and mixed-publication request.

## Repair-policy milestone (BPR-1980)

The core now exposes `tour_destination_repairs` and `plan_tour_stop_repair`
from `bpr/core/review_tour_repair.hpp`. They return stable stop identity,
route number/title, typed problem, the destination plan, exact-head re-pin
and nearest-hunk range candidates, and an exactly qualified file-comment fact.
No message text is parsed to determine a destination problem. Callers supply
operation observations and current-head provider hunks under the tour's exact
observed-head context. Missing/mismatched source observations stay unavailable;
unknown/denied/rate-limited/indeterminate operations grant no fallback write.

`prepare_tour_source_repair` recomputes a selected repair and returns one
ordinary `tour_command_kind::restore` command with the captured draft owner,
revision and selected stop. The existing local host owns Undo, Redo and
autosave. Published tours/remote subjects remain outside this source-edit seam.
The immutable original and explanation are retained; the effective source is
an author-selected mapping. Changed explicit line citations (`line N`, `lines
N–M`, `LN`, source-link anchors and their ranges) require reaffirmation; ordinary
prose and unrelated numbers preserve readiness. No generated visitation or
publication authority is conferred.

Both accepted-stop relocation and agent proposal relocation use
`nearest_tour_hunk`, with provider context bounds, deterministic provider-order
ties, preserved length where it fits and shortening within a smaller hunk.
Malformed, deleted or unknown hunk evidence supplies no relocation.

The existing revision mapper additionally compares retained line bytes,
including terminators. A semantic exact-line match under ignored whitespace or
normalized line endings cannot claim `unchanged_material` and enable re-pin.
An exact historical mapping is offered only at the current head, on its right
side, with a complete blob/range and known commentable hunk.

A definitively unsupported off-hunk, pending-file and standalone-file sequence
may plan a supported conversation permalink. Unknown or failed operations do
not qualify that fallback. The author's explicit compatibility selection is
still required by `assess_tour_publication`; these policy values perform no
remote effect.

## Verification

The focused core course builds a ten-stop route with one historical exact stop
and one stop immediately outside a hunk. It identifies both, prepares and
applies two ordinary edits, retains all narratives and identities, and assesses
all ten as `single_review`. Negative cases retain changed/ambiguous/missing or
stale source mappings, mismatched observations, unknown/denied operation facts,
read-only protection and stale command rejection. Boundary cases cover context,
range length, deterministic ties, malformed ranges and upper-bound arithmetic.

The actual `review_tour_host` course dispatches the prepared accepted-stop
repair, preserves selection, undoes/redoes exactly, and drains the resulting
ordinary autosave on shutdown. No provider, network or desktop is composed.

Exact candidate and final command results are recorded in the
[checkpoint](../issues/_checkpoints/BPR-1979/handoff.md). Runtime-only logs live
under `/tmp/bpr-tour-repair-run`; no build output or transient log is committed.

## Remaining parent acceptance

BPR-1979 remains incomplete. The editor-repairs milestone is accepted for
source candidate `91830ea7edde0002c7d7a23f05903dbef0fe52df`: per-stop reasons and
selection, exact current-head mapping, previewed relocation, ordinary Undo/Redo,
replanning, and repaired single-review publication are composed and verified.
The user guide explains both repairs and the separate file-operation boundary.
No native appearance, live server, or other-platform acceptance is claimed.

`mixed-publication` must implement the explicitly chosen review/conversation
split, cross-kind overview/navigation, interruption reconciliation and published
maintenance, and publish the authorized RFC-BPR-0041/0042 amendment with its
verified behavior. No mutation sequence or recovery journal was changed here.
Existing compatibility-only behavior remains available; no outcome is selected
silently.

The owner-supplied GHES 3.20.8 pending-line probe answers BPR-1737's second
criterion only. Its separate file-comment criterion remains open, without an
invented build or live-host verdict.

## Editor-repairs milestone result

The desktop preview binds current provider hunk bounds and bounded pinned-source
comparisons to the accepted owner/head. Each historical pair is admitted and read
serially through the existing epoch actor, with 256 KiB per side and cancellable
core computation. Unavailable source stays unavailable. Results for a replaced
draft, owner or revision cannot apply an edit. The core's exact-byte mapper supplies
re-pin candidates; relocation shows the actual retained head text before Apply.
The ordinary review-tour host owns repair, selected identity, Undo, Redo and save.
Typed repair rows are refreshed on edits and history projections.

Frontend-design's installed desktop guidance was loaded before UI work. Repair
rows inherit native fonts/palette and present stop links, reasons and the relevant
actions above the explicit publication choice. No desktop was driven or captured.

A synthetic three-commit fixture, built by `scripts/build_tour_repair_fixture.py`,
retains review.cpp bytes from the historical commit to head; the provider patch
names the complete three-context-line hunk, lines 3–9. The composed ten-stop
`application_review_tour_publication_repairs` course reads its source through
installed Git/cache and uses the public editor controls through publication.
The exact candidate above passes that course in 4.47s, including a negative
control proving that an intervening edit expires the relocation preview. It
asserts one pending creation, one submission, ten native comments, zero ordinary
review shortcuts and zero compatibility comments. The core repair and real host
courses pass; architecture, network manifest, documentation and 67 documentation
regressions pass. LLVM22 format and tidy pass. Offscreen 1200×760 logical renders
were inspected at 1×, 1.25× and 2× (the scaled courses pass in 3.48s and 3.59s).
These are macOS arm64, Debug, Qt 6.12.0, synthetic ten-stop fixture observations,
not native system-appearance or performance qualification. Exact commands,
initial failures and correction evidence are retained in the checkpoint.

The supplied **Publish as file comment** requirement remains mandatory in the
mixed-publication milestone. Existing standalone-file capability facts must not
be mislabeled as pending-review membership. No exact file operation is newly
qualified or enabled in this milestone; unknown/denied facts remain unavailable.
The successor must implement an honest supported file route and its explicit
choice, or retain a concrete unmet obligation. It must not count a standalone
comment as one pending review. AC5/AC7 and both RFC amendments remain open.

### Selected acceptance at the editor boundary

All rows bind source candidate `91830ea7edde0002c7d7a23f05903dbef0fe52df`.

| Criterion | Result and direct evidence |
|---|---|
| AC1 | Pass for the editor: typed historical-exact and off-hunk rows, stop numbers/titles/reasons/actions, affected-stop selection; composed course plus inspected renders. |
| AC2 | Pass: the exact pinned Git comparison enables one re-pin; the native batch uses the repaired effective source. Changed/ambiguous/unknown negatives reuse the accepted core evidence. |
| AC3 | Pass: actual head text previews the nearest accepted context range, Apply preserves prose and selection, Undo/Redo restore/re-plan, intervening edit expires Apply. |
| AC4 | Pass: the repaired ten-stop composed route creates one pending review, submits once, retains ten native comments, and creates zero compatibility comments. |
| AC6 | Pass for the current choice set: compatibility is explicit and follows repairs; the existing composed compatibility course passes. M3 must put mixed publication before it. |

The three editor milestone exits pass through this composition, guide and scoped
verification. These selected records do not accept AC5, all of AC7, or the
remaining supplied file-comment control. The parent stays In Progress.

## Final mixed-publication and pack result

Mandate `tour-destination-repairs-r1`; exact final source/test/RFC/guide candidate
`aeed86510165247bb877136ff996fe584cfbe346` (44 explicit paths), descended from
immutable RFC_BASE_SHA above. This supersedes the earlier parent-incomplete
boundary records, preserving their accepted evidence and limitations.

The explicit outcome order is repair, mixed, compatibility. A mixed plan retains
route order and each stop's representation, creates one pending review for native
members, then the conversation remainder, wires returned cross-kind links and
submits that exact review once as COMMENT. Its overview stays the review body.
Separate complete baseline collections and typed identities protect review,
conversation and standalone-file writes. Stop, lost response and restart retain
confirmed receipts; reconciliation reads first, and only explicit preview/Resume
continues. Maintenance edits both kinds and updates the original overview last.

The mandatory file choice is implemented as an ordinary core restore edit with
Undo/Redo and persistence. The provider observes exact operation facts on the
effect lane; the host binds owner, draft, revision and head, and the planner binds
the source. General tour support supplies no file fact. Unknown/denied/uncertain
facts remain unavailable. A supported standalone file choice uses a separate
witnessed REST operation and baseline; it never joins the pending review, and
its preview states that separate visible write. The production provider's default
is Unknown until exact operations are separately qualified; the positive recorded
fixture is not a live-host qualification or permission to enable the operation.
The provider-owned `review_submission_port::tour_destinations` seam is where such
separately qualified facts must originate; no caller-populated production cache
is accepted. Pending-file staging remains unavailable without its own catalog
and live evidence. BPR-1737 AC1 and existing live/native follow-ups remain open.

The authorized amendments are published in RFC-BPR-0041 Compatibility publication
and RFC-BPR-0042 Mixed representation amendment. No already-published
compatibility route is converted. No source whose bytes changed is silently
reanchored. No owner desktop or live GitHub target was driven or mutated.

| Original criterion | Final acceptance and direct evidence |
|---|---|
| AC1 | Pass: accepted M2 per-stop findings/selection and final repaired course; exact supported-file course exposes the named file choice, while Unknown exposes none. |
| AC2 | Pass: accepted exact-byte mapping/re-pin negatives and composed repair course; final core repair/destination checks pass in dev and core-only. |
| AC3 | Pass: accepted nearest-provider-hunk source preview, ordinary history and stale-command refusal; final repaired course passes. |
| AC4 | Pass: final composed ten-stop repair course retains one pending creation, one COMMENT submission, ten native comments and zero compatibility comments. |
| AC5 | Pass: mixed session courses interrupt/reopen every one of seven writes under Stop and lost-response cases, then maintain both kinds without repost; composed mixed course verifies zero-write preview, exact outside-diff stop, returned route links, one root identity and later reorder. File course adds exact/Unknown facts, Undo/Redo, interrupted standalone file write, read-only restart, explicit Resume and file maintenance. |
| AC6 | Pass: three explicit choices put compatibility last; native and compatibility composed baselines pass, and the mixed course refuses implicit selection. |
| AC7 | Pass: scoped integration, final affected courses, architecture/network boundaries, core-only checks, LLVM22 format/tidy and documentation checks below. |

Verification on macOS arm64, Debug, Qt 6.12.0, offscreen synthetic fixtures:

- The 12-course integration passed in 23.37s: core destination/repair, application
  submission host, both SQLite stores, staged session/restart, and composed
  native/compatibility/repairs/mixed/file publication. The host course additionally
  refuses misbound/cancelled operation observations and proves effect/decision
  lane separation and zero writes.
- Final affected Qt preview/mixed/file courses passed 3/3 in 9.88s after layout,
  status and scoped-fact invalidation refinements. The preceding final staged
  session and repaired courses passed in 2.57s and 5.37s; their inputs are unchanged.
- Four boundary courses passed in 23.34s: `core_dependency_boundary`,
  `core_boundary_contract`, `architecture_boundaries`,
  `test_network_isolation_manifest`. The unchanged negative architecture contract
  reuses its accepted M1 evidence; no boundary verifier was changed.
- Core-only builds pass; destination/repair pass 2/2 in 0.61s. LLVM22 format passes
  all 38 changed C++ paths. Tidy passed 23 translation units initially, then all
  three affected units after correcting test parameter names; all 24 changed
  translation units have passing evidence. Documentation gate and 67 regression
  tests pass; the final board-closeout documentation observation is in the handoff.
- Full-window mixed and file previews at 1200×760 logical were inspected. The
  captured plan replaces the superseded local preview and repair list and retains
  a readable viewport; the Qt and composed courses assert that geometry. This is
  deterministic offscreen layout evidence, not native appearance. M2's unchanged
  repair typography/scaling evidence is reused; no new scale/native matrix or
  Debug timing course is claimed.

Retained initial failures and corrections: stale-preview parentheses and lambda
shadow warnings; post-confirmation description checking the old baseline;
fixture assertions assuming unchanged neighbors; a fixture native selection
accidentally outside its hunk; preview wording/partial-status selectors; remote
fixture overlay omitting conversation/file collections on refresh; collapsed
captured preview; test parameter naming. All affected functional and static
checks were rerun successfully. No failed functional obligation remains.

Runtime-only logs and rasters remain external; exact commands, candidate,
command quiescence and guarded closeout are bound in the existing checkpoint.
No human, independent-review, native-system or live-server verdict is inferred.
