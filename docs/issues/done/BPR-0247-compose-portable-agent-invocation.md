---
id: BPR-0247
title: Compose portable agent invocation and a real pinned workspace
status: Done
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
  receipt/private-ledger observation, durable defect state, and the current
  semantic selection. The generic context builder continues to admit only
  explicitly named notes and supplied drafts; this portable surface supplies
  neither until BPR-0232 adds a reviewer selection control. No credential,
  unselected prose, or presentation coordinate may cross the boundary.
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
- Selecting private notes or review drafts for inclusion remains BPR-0232
  scope. This slice proves their exact absence instead of silently including
  everything in the private ledger or current editor.

## Focused verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(core_agent_profile|core_agent_session|core_agent_context_supplement|application_agent_workspace|application_agent_launch|application_agent_invocation_host|qt_agent_launch_panel|qt_agent_launch_canary|application_agent_invocation_(composition|construction_cut|gesture_cut|cancellation|staleness|cleanup_retry)_contract|git_process_contract)$'
python3 scripts/verify_documentation.py --root .
git diff --check
```

## Implemented state

The shipped workbench now exposes the four immutable built-in profiles and
routes one explicit panel gesture through core host policy, exact accepted
application/PR/revision/epoch/receipt/defect/semantic context, a real retained-
cache materialization, the local process adapter, and identity-gated Qt
projection. The public canary decodes the child's exact standard-input envelope
and compares its complete key set and values against independently captured
facts. It includes one durable defect while proving that hostile unselected
note/draft prose, credentials, and presentation coordinates do not cross the
boundary.

Disposable workspaces and their parent root carry creation-time directory
identity. Cleanup removes only the exact owned directory and the exact empty
parent, never a same-path substitute or nonempty root. Cancellation and accepted
head movement stop descendants and clear provisional output. Cleanup refusal
retains the exact capability and a non-quiescent host; composed shutdown submits
one de-duplicated effects-actor retry after terminal completion. The hostile
course substitutes the workspace, observes refusal, restores the original,
then proves exact workspace/root removal, substitute preservation, and bounded
shutdown.

The final focused matrix passed 15/15 in 236.37 seconds, including construction
and gesture cuts, cancellation, staleness, cleanup retry, the real Git contract,
and the four-protocol canary. Architecture boundaries, core-only tests,
documentation, formatting, changed-file clang-tidy, simplification, and
`git diff --check` are green. Independent critical review found concrete
workspace-retargeting, cancellation, projection, destruction, context-proof,
and cleanup-retry defects across its correction rounds. The final rereview
returned CLEAR with no blocking or high-risk finding.

Native Linux and Windows evidence remains deliberately deferred. Windows still
refuses before process start, and BPR-0232 still owns persisted sessions plus an
explicit note/draft inclusion selector.
