---
id: BPR-0247
title: Compose portable agent invocation and a real pinned workspace
status: In Progress
rfc: RFC-BPR-0011
depends_on: [BPR-0226, BPR-0228, BPR-0229, BPR-0230]
---

# Compose portable agent invocation and a real pinned workspace

The product owner deferred target-native Linux and Windows environments until
the end of the remaining portable work. BPR-0240's committed portable context
and protocol lifecycle are available, but its card correctly remains open for
native proof. This card owns the independent available-target composition that
BPR-0232 cannot claim while that dependency is open.

## Required final state

- Ship a visible, accessible `agent_launch_panel` in the real workbench with
  immutable initial profiles for Codex App Server, Codex JSONL, approved Claude
  stream-JSON, and the documented external-terminal harness. Constructing or
  displaying profiles performs no executable discovery or launch.
- Apply deterministic host policy before any effect: GitHub.com may offer all
  four profiles; explicit GHES forbids the Codex profiles and permits only the
  corporate/external alternatives. A user gesture must traverse
  `core::decide_agent_invocation` before workspace or process work begins.
- Build the version-two context only from accepted PR/epoch/diff authorities,
  receipt/private-ledger observation, explicit draft/note inclusion, durable
  defect state, and the current semantic selection. No credential or
  presentation coordinate may cross the boundary.
- Replace the descriptive disposable-workspace record with a real
  actor-confined operation. Materialize the exact accepted head into one
  owner-private destination outside the private cache, detach and verify HEAD,
  remove remote/alternate/hook authority, preserve the private cache topology,
  and clean only that owned workspace after terminal delivery. Existing
  checkout remains an explicit per-invocation choice with visible dirty or
  unknown risk.
- Pass the accepted workspace as an explicit child working directory. Stream
  provisional output and every typed terminal through the application host to
  Qt; cancellation, staleness, failure, and shutdown may not leak a child,
  workspace, credential, or stale projection.
- Add a real-executable public canary from panel gesture through core policy,
  exact context, workspace, hostile local child, streamed projection,
  cancellation/staleness, and quiescent cleanup. Construction and invocation
  wires require independent cut negatives, and direct fixture-helper launch is
  forbidden.

## Explicit non-goals

- No live Codex, Claude, Bedrock, or GitHub invocation is required; the RFC's
  hostile local canary remains the direct portable surface.
- No Windows launch claim is added. Windows retains BPR-0240's pre-start typed
  refusal until the deferred native containment work is complete.
- Persisted arbitrary-command profiles, model API calls, provider credentials,
  and automatic invocation remain out of scope.

## Focused verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(core_agent_profile|core_agent_session|core_agent_context_supplement|application_agent_workspace|application_agent_launch|application_agent_invocation_host|qt_agent_launch_panel|qt_agent_launch_canary|application_agent_invocation_composition_contract)$'
python3 scripts/verify_documentation.py --root .
git diff --check
```

The card remains In Progress until the public canary, focused component matrix,
architecture boundaries, documentation gate, formatting/tidy, simplification,
and independent critical review are green at the committed implementation head.
