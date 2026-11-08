---
schema: "implement-rfc.card.v1"
id: "BPR-1942"
title: "Configure owned native runtime storage for the full test graph"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"kind":"test-tooling","found_by":"BPR-1940 native D-loop fixture I/O diagnosis","related_cards":["BPR-1940"],"milestone_plan":{"id":"bpr-1942-native-test-runtime-root","path":"docs/rfcs/plans/bpr-1942-native-test-runtime-root/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9c3936cb5ad659a7487a44b7d17ebddadf1207ca","evidence":{"kind":"observation","detail":"Optional validated POSIX owner-private canonical root and unchanged empty/default registrations; Windows opt-in refuses; existing source/audit/binary/helper/output paths and product/compiler/budget/isolation policy preserved. Seven validator and nine CMake probes; local942existing-node equality plus one new contained contract."}},{"criterion":"AC2","outcome":"pass","revision":"9c3936cb5ad659a7487a44b7d17ebddadf1207ca","evidence":{"kind":"observation","detail":"Exact9c native affected build159.63s,79protected keys unchanged; actual927-node graph25 root argv+exact logical mirrors only; normal original invocation6.16s/roundtrip1.92s +5tooling and3preflight PASS, no skips/missing/unexpected. R1actual failure and R2diagnostic retained separately; no projection acceptance; guarded root removed and default cache restored."}},{"criterion":"AC3","outcome":"pass","revision":"9c3936cb5ad659a7487a44b7d17ebddadf1207ca","evidence":{"kind":"observation","detail":"Supported normal configure/suite operator docs added and source9c pushed authoritative main including latest8f card/reservation; final format/tidy/docs/board and exact source/binary/runtime/job receipts retained. Full frozen release and full-suite capacity forecast separately coordinator-owned."}}]
---

# BPR-1942: Configured native test runtime storage

## Acceptance

- AC1. Provide an optional validated `BPR_TEST_RUNTIME_ROOT` for the writable
  end-to-end `BPR_ROOT` leaves. Empty/default registrations remain byte-equivalent.
  Opt-in accepts an existing absolute canonical no-link POSIX root owned by the
  current user with mode 0700; refuse unsafe/list/control and source/build/home
  collisions, while permitting a dedicated child of home. Windows defaults are
  unchanged and opt-in refuses without inventing an ACL checker. No product,
  compiler, timeout, predicate, network-isolation or source-audit-root changes.
- AC2. Verify default/custom/refusal configuration and compare the actual full
  configured graph: only the expanded writable `BPR_ROOT` arguments may differ.
  Normal registered native invocation and round-trip courses and relevant
  source/inventory/preflight controls pass with original properties, sandbox,
  environment modifications, predicates and timeouts. Runtime storage is owned,
  bounded and guarded; no external projection supplies release credit.
- AC3. Document supported normal configure/full-suite usage and its ownership,
  environment and storage obligations; preserve all BPR-1940 negatives. Land the
  bounded tooling change on authoritative latest main, with exact source/config
  receipts and native ownership state. Frozen release qualification and
  packaging/publication remain coordinator obligations.

## Mandate and plan

The coordinator explicitly authorizes this follow-on after BPR-1940's main
integration. Its six-course external projection is engineering-only. The
original full graph still places persistent application data in the D-backed
build image; unchanged private review passes 4.63 s on native ext4 after journal
waits caused the original functional course to fail. `TMPDIR` alone does not
move hardcoded CMake `BPR_ROOT` arguments.

Use one optional cache string (to reject relative paths before CMake silently
normalizes a PATH value), a small CMake runtime-directory selector and a
read-only configure validator. Replace only the 24 writable E2E root expressions;
observe their expanded graph count. Source-audit roots, binaries, helpers,
committed fixture sources and normal output artifacts retain their paths.
The caller explicitly designates a dedicated private directory and keeps one
per graph. No marker/namespace manager or arbitrary directory cleanup is added.
The native operator retains exact root identity and capacity guards and records
inherited `TMPDIR`/`TMP`/`TEMP`; the option does not relax durability or budgets.


## Local implementation and direct proof

The optional cache value remains a STRING so a relative caller input is
refused rather than normalized into authority. The helper selects the existing
default directory unchanged when empty; opt-in reads the existing imported
`Python3::Interpreter` location and invokes a read-only POSIX validator. It
never creates, chmods or removes the designated root. Only the 24 writable E2E
root prefixes use the selector. The new contained Python validator contract is
the only added test row; no Windows uncontained exception is added.

Seven filesystem validation tests and nine real minimal-CMake default/custom/
refusal probes pass, including an empty default with no Python target, relative
and false-like inputs, separator, shared directory, missing directory, link and
protected build root. Shared mode and link identities remain unchanged.
The initial custom configure correctly failed when the implementation used a
scoped-away Python variable; the helper is corrected to the existing imported
target location. The first failure is retained in external
`candidate39/implementer1942/local-custom-configure.log`.

The actual complete local configured graph has 943 nodes. All 942 previous
nodes' commands and properties are byte-equivalent in the default graph; the
one added node is `test_runtime_root_contract`. The custom graph changes 25
expanded writable `BPR_ROOT` arguments. `BPR_LOGICAL_COMMAND` must mirror those
same path substitutions truthfully; the coordinator explicitly accepts that
exact derived metadata delta. Every other property, environment modification,
working directory, sandbox command, label, lock, predicate and timeout is
unchanged. Raw JSON and the strict comparator retain the full observations.

Original registered `test_runtime_root_contract`, `test_suite_runner_contract`,
`command_correctness_registry_contract`, `test_network_isolation_manifest` and
`test_network_isolation_structure_contract` all pass, as does the real
`application_review_round_trip_composition_contract` with the configured custom
root. Native normal registered invocation/round-trip and suite preflight
remain pending; no local or external projection is substituted for them.


## Actual native route and retained functional failure

Source 861be27005fd8190727bc488b742ddf611b1afe8 is checked out exactly on a
fresh private native development branch, preserving the earlier branch. The
normal affected app/invocation/helper build passes 255.78 s; both app and
invocation binaries are newly linked and source-bound. All 79 protected feature,
compiler/hardening/tool/cache keys remain unchanged. The native full graph has
927 nodes; custom configuration changes exactly 25 writable root arguments and
their exact logical mirrors. Compile commands are identical across root choice.
The normal suite runner's original three preflight rows pass. Its selected
rows have 6 PASS/1 FAIL: original invocation 12.76 s and all five tooling gates pass,
but round-trip fails 18.53 s at its 15 s fallback-draft predicate. Its 40 s outer
budget has not expired. This is an actual functional negative, not acceptance.
Raw native-final-r1 retains the real runner/CTest/JUnit/source/cache proofs.
The private runtime peaks at 4,165,632 allocated bytes; default cache is restored,
all processes reaped, source clean and the exact owned root removed.

The one approved diagnostic at source 22f9064ca passes and supplies no
uninstrumented credit. It observes the off-hunk gesture while private readiness
is absent, file rows zero, and publication generation/file matches false.
Later both matches/private readiness are true and the draft exists. The
fixture sends stale input that the presenter's guards correctly refuse;
no direct causality is inferred from BPR-1941's unrelated diff. Both diagnostic
source files are deliberately restored, and its binary remains explicitly
rebuild-required. BPR-1943 owns the separate bounded fixture readiness correction;
this card's failed original round-trip remains unmet until final normal native
verification. Full release acceptance remains with the coordinator.


## Final normal native acceptance and main integration

Final uninstrumented combined source 9c3936cb5ad659a7487a44b7d17ebddadf1207ca
is rebuilt and verified on the native Linux station, then intentionally pushed
to authoritative main including latest 8f3102c2a. The affected app, invocation
and helpers build passes 159.63 s with all 79 protected feature/compiler/hardening/
tool/cache keys unchanged. The source is clean; main and invocation binaries
contain no temporary diagnostic markers, and the app contains the real input
guard. The normal 927-node configured graph changes exactly 25 writable root
arguments and their exact logical-command mirrors; all other properties are
identical and compile commands do not change across root selection.

The ordinary suite runner passes all seven selected original registered rows:
invocation 6.16 s, round-trip 1.92 s, the new runtime-root contract and four existing
network-manifest/structure, suite-runner and command-registry tooling gates.
Its three original documentation/issue/allocation preflight rows also pass.
No projection, assertion/gesture/provider-sequence change or budget relaxation
supplies these results. The 14-bit composition evidence and source contract
remain enforced by the original registered course. The R1 functional negative
and R2 diagnostic positive remain separately retained; the latter is not acceptance.

The final full Git source is exact on both machines, with `src` tree
67f3e76a720956a9ae653e699bd48825610b54d3 and `tests` tree
eafc2a4942e4b48bfaf3cff919d8ec34d83c63e7. Native app SHA256 is
8b232d317fa9e52309d91ce6e254b7fd80d12bec9fe5baf1016a31a6cb430dbc;
invocation SHA256 is
f7402c3337b7b1352b31bc2deeb49de9ab9dbe2c9871e3262aea3fd0b5f22dc2.
Raw receipts/XML/stdout/cache/graph proofs are external
candidate39/implementer1942/native-final-r3 and verification-r3-results.json.
The guarded runtime peaks at 3,997,696 allocated bytes (256 MiB cap, original
D guard, C32 GiB/root8 GiB reserves). Every owned descendant is reaped,
normal empty-root cache restored and the exact owned runtime root removed.

Focused format22/tidy22, documentation and board checks pass. This closes the
bounded engineering criteria; selected 7+preflight 3 is not comprehensive or
frozen-source release qualification. The coordinator owns the final frozen
Mac/Windows/Linux graphs, full suites, security/trust/package/publication and
full-suite runtime forecasts. Native development ownership is returned only
after the final clean/idle handback.
