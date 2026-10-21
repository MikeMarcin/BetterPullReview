# BPR-1674: input-scoped qualification

Mandate `release-30min-portfolio-r1`; M2 `input-scoped-qualification`.
The engineering scope adds ordinary tests-enabled Release qualification through
existing presets, runner, bootstrap and native isolation, preserves Debug, and
refuses broader evidence reuse whose input closure is unproved. No publication,
version, security disposition or mandatory performance limit changes.

## Qualification and input ownership

`qualification-release` uses a separate Ninja Release tree; Windows selects
Release in the existing tests-enabled Visual Studio tree through
`windows-qualification-release`. Feature options are inherited unchanged.
`app-release`, distribution builds, Debug and optimized tests remain distinct.
The existing configuration-specific executable/object manifests remain authoritative.
All optimized Qt commands retain serial scheduling; M1's Debug declarations grant
no Release exemption. Python optimization is refused before selection.

The existing runner accepts an explicit bounded regex after validating the full
preset schedule. Its three actual bootstrap rows run fresh first, even for a
bounded selection. For their duplicate occurrence inside that same invocation,
complete passing proof can be reused with the live invocation ID and equal
repository/manifest/environment/tools/deployed-helper input seal. Reports separate
fresh execution from this same-invocation bootstrap proof. Failed/skipped/missing,
stale/interrupted or changed proof requests fresh execution; changed inputs across
execution refuse complete qualification. This is a new bounded proof owner,
not a relabeling of any historical whole-source receipt. No external receipt import
or cross-request bypass is provided.

Cross-request hostile-tool qualification remains **unready**: the architecture
contract copies many actual product/native source fragments into its hostile
fixtures, reads its checker/helper and invokes actual CMake under native path and
filesystem semantics. A narrow complete closure has not been proved. It therefore
runs fresh, as do candidate source/manifests, dependency/artifact/source/SPDX,
security/legal/native performance/trust obligations. No adversarial fixture or
fresh-owner course was deleted or merged.

## Eight-method assertion-to-image matrix

All methods in `tests/end_to_end/composition_contract_tests.py` remain required.
Only the last method configures and builds an application. A tests-on optimized
image cannot satisfy its tests-off absence claims.

| Method | Assertions and inputs | Owning configuration/image | Disposition |
|---|---|---|---|
| `test_production_mutation_authority_issuers_are_language_sealed` | Real public/private headers; positive compilable baselines; hostile construction/factory/copy/friend/derivation snippets and named access diagnostics | Actual selected native C++ compiler; production and explicit testing-macro snippets | Retain; runtime images do not prove language sealing. |
| `test_whole_review_authority_is_distinct_and_test_factory_is_private` | Nonconvertible authority types, testing overloads, forbidden factory and private authority construction | Native compiler/header snippets with and without `BPR_APPLICATION_TESTING` | Retain distinct whole-review authority. |
| `test_build_edges_keep_compiles_and_drop_only_regeneration` | Compile/link/custom edges survive parser filtering while regeneration/phony source edges do not create false fixture evidence | Synthetic Ninja graph and actual Python parser | Retain; no application build. |
| `test_language_seal_assertion_rejects_what_does_not_prove_a_seal` | A snippet that compiles, an unrelated typo and unrelated named diagnostics cannot satisfy the seal assertion | Actual native compiler diagnostics and assertion mechanism | Retain negative controls; no application build. |
| `test_concrete_github_selection_is_owned_by_composition` | Current source owns provider/vault/transport/executor selection; hostile consent/discovery mutations rejected; forbidden layers scanned | Actual current source across composition/adapters/core/application | Fresh source check; no application build. |
| `test_the_automated_test_boundary_has_one_reader` | Exactly one guarded reader; main/updater/removed/comment-only/unconditional/ignored-reader mutations rejected | Actual current source and synthetic mutated source maps | Fresh source check; no application build. |
| `test_async_mutation_host_inventory_rejects_every_direct_bypass` | Current reviewed manifest/source ownership, forbidden provider/coordinator/port bypasses and hostile helper variants | Actual current source plus adopted reviewed manifest/digests | Fresh; preserve the separate review/reseal procedure. |
| `test_tests_off_release_graph_omits_public_path_fixture` | Public layout independent of testing macro; tests-off cache; dependency audit excludes Python/Git; compile invocations/graph omit testing fixture/seams; actual production composed-host image; forbidden fixture bytes absent; platform CLI markers; default/GHES self-tests select real platform vault; render-error path refuses correctly | Generic `BUILD_TESTING=OFF`, `Release`, non-distribution Ninja application plus its actual runtime | Retain existing persistent incremental build; tests-on Release and restricted distribution images have different assertions. No duplicate build removal proved. |

## Configuration and residual Debug policy

The bounded inventory requires 237 completed ordered cases: 115 workbench,
15 core diff, 28 diff host, 77 GitHub host and two drawer cases. Source/helper seals,
actual `NDEBUG`, MSVC iterator level, CRT/compiler/Qt diagnostics, explicit workload
markers, four executed latency branches and injected-allocation counts bind the
receipt. List-only output never counts as behavioral qualification.

Debug keeps both 6,000-line workloads. Release keeps 40,000 core/30,000 host lines,
four `<100 ms` drawer assertions and both Windows allocation sweeps; neither
configuration replaces the other. A test-only negative control can tighten exactly
one existing latency bound to zero, never relax it. Unconditional expectation and
caught-failure backstop controls must fail in the actual optimized probe.

**Every release-candidate integration boundary still requires complete native
Debug.** Optimized normal-gate promotion remains unready until M3 supplies full
native optimized results and an accepted complete residual closure. The following
triggers invalidate any future scoped Debug proof: Qt/compiler/CRT/STL/ABI/platform/
flags/assertion changes require full affected-platform Debug; ownership, containers,
allocation, threading, cancellation, teardown and Qt callbacks require complete
known affected public compositions; test/fixture/assertion/scheduler/isolation/helper
changes require both configurations and negative controls; unknown closure,
failed/stale/interrupted evidence or broad integration requires full native Debug.

The owned steady-column `Q_ASSERT` and Debug Qt DLL/CRT/STL diagnostics remain
covered by that full Debug gate. No macro override or hybrid Qt link is introduced.
Strict optimized memory/cache budgets remain required. The unresolved Ubuntu RSS
failure remains a failure; v0.3.1's exception does not authorize another candidate.

## Native discriminator and checkpoint

The first native pilot at source `63a246c198053051d033cf4b7950cf7e852bae77`
passed all five commands. Configure: 39.177 seconds; selected-target Release build:
420.722 seconds at four jobs; actual bootstrap: 13.324 seconds; runner: 51.149
seconds, including about 37 seconds of serial test work. The cache lacked much of
the Release test dependency graph. Preparation recorded foreign host activity;
build duration is not an uncontended speed claim. Quiet-host admission preceded
its tests; the process observer started later, so continuous idleness is not claimed
for this preliminary run. Historical Debug durations only prioritized the experiment.

Final candidate controls, native results, exact identities and command quiescence
are recorded below when complete. The private runtime is
`.mask/sessions/release-qualification/m2/`; native evidence stays under
`C:/Users/mikem/bpr1670-suite/evidence/m2-*`. The first installation was refused
while foreign builds ran; two source-guard task failures and a malformed read-only
SSH quoting attempt are retained. No foreign process was stopped. The canonical
main's independently committed BPR-1676 counter/receipt were synchronized explicitly
from `9b9a4acc6ffca6d92c4ec2bae8bfef195590f267`; its unrelated promo changes remain
outside this native candidate. No source manifest seal was changed.

## M3 scheduling frontier

Release currently remains exclusive for every formerly Debug-only declaration.
For a subsequent source-audited Release registry, the default workbench scale
courses are the highest-value candidate: their only `NDEBUG`/iterator branches
print build facts, and M1 already separates the 100 ms conversation-reveal selector.
The preliminary Release 1.0x course executes all 115 cases. This is an assessed
candidate for admission, not an activated configuration declaration. The other
M1 family sources (diff workbench, overview summary, source find and review
submission composition) have no `NDEBUG`/`QT_NO_DEBUG`/iterator-selected test branches;
their waits and lifecycle assertions still need the same reachable-output/helper
review under the actual Release graph before admission. No row inherits an
exemption from the absence of a macro alone.

The composed inline-drawer selectors are **unsafe to admit as correctness**:
Release enforces four numeric 100 ms limits. Conversation reveal and strict
memory/cache/other timing selectors retain exclusive measurement intervals.
Unknown configurations, source/helper drift and unclassified commands stay serial.
M3 should first inspect the actual complete Release manifest/case lists, extend
only independently reviewed command/configuration declarations, run existing
registry/schedule negative controls, then a bounded matched same-binary serial versus
parallel workbench contrast before the one stabilized full native pass. Retain all
scales, ordered fresh owners, per-command outputs and optional-root unsets. Do not
classify Release while merely copying the Debug timing inventory.
