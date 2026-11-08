---
schema: "implement-rfc.card.v1"
id: "BPR-1942"
title: "Configure owned native runtime storage for the full test graph"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"kind":"test-tooling","found_by":"BPR-1940 native D-loop fixture I/O diagnosis","related_cards":["BPR-1940"],"milestone_plan":{"id":"bpr-1942-native-test-runtime-root","path":"docs/rfcs/plans/bpr-1942-native-test-runtime-root/milestone-plan.json"}}
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
