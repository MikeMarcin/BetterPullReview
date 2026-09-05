---
schema: "implement-rfc.card.v1"
id: "BPR-0335"
title: "Add the agent launcher and panel to the composition inventory"
status: "Done"
rfcs: ["RFC-BPR-0011"]
depends_on: []
---

# Add the agent launcher and panel to the composition inventory

[[BPR-0232]] requires: "Extend the standing composition inventory to the
launcher and panel, then prove construction and invocation separately from their
linked symbols." That card is blocked on [[BPR-0240]]'s target-native Windows
work, but this bullet is not. The inventory is a macOS-verifiable linkage check
over an already-composed graph, and nothing in it needs Windows.

`scripts/verify_composed_hosts.py` carries one agent entry — the invocation host
constructor. It has zero entries matching `launch` or `panel`. So two composed
surfaces that the application really does construct are absent from the standing
inventory:

- `bpr::adapters::agent::agent_process_launcher`, constructed at
  `src/app/src/github_application.cpp:2113` and retained as `AgentProcess`;
- `bpr::qt::agent_launch_panel`, reached through `window.agent_panel()`.

Both are present in the linked binary under both profiles, checked with `nm -C`
against `build/dev/src/app/better_pull_review` and
`build/app-release/.../BetterPullReview`. They belong in `SHARED_REQUIRED`
rather than a profile-specific list.

## Scope, and what this card does not claim

The verifier's own docstring is explicit that "a required symbol proves that the
selected binary contains the named implementation; it does not prove
construction or runtime reachability." This card lands the linkage half of
[[BPR-0232]]'s bullet and records the state of the other half rather than
claiming it:

- The **panel** already has separate construction-and-invocation evidence.
  `tests/end_to_end/agent_invocation_composition_public_path.cpp` reaches
  `window.agent_panel()`, offers profiles, clicks `start_button()`, and asserts
  `panel->statistics().StartIntents == 1U`, with a construction-cut negative.
- The **launcher** is constructed on the composed path but has no assertion
  naming it separately from its linked symbol. That remainder stays with
  [[BPR-0232]].

## Required final state

- `SHARED_REQUIRED` names the agent process launcher constructor and the Qt
  agent launch panel constructor.
- Both profiles still verify against the real built binaries.
- `[[BPR-0232]]` records which half of its inventory bullet is now discharged
  and which is not.
- No production or packaging behaviour changes. This card adds evidence, not
  code paths.

## Verification

```sh
python3 scripts/verify_composed_hosts.py --profile development-fixture \
  build/dev/src/app/better_pull_review
python3 scripts/verify_composed_hosts.py --profile production \
  build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview
python3 tests/scripts/verify_composed_hosts_tests.py
ctest --test-dir build/dev --output-on-failure \
  -R '^(application_composition_contract|application_agent_invocation_composition_contract)$'
python3 scripts/verify_documentation.py --root .
```

Removing either new anchor's symbol from the binary must fail the verifier;
that is the property the inventory exists to hold.

## Result

`SHARED_REQUIRED` carries both anchors. The required count rises from 18 to 20
for `development-fixture` and from 17 to 19 for `production`, and both profiles
verify against the real built binaries:

```
COMPOSED_HOSTS_OK: profile=development-fixture required=20 forbidden=2
COMPOSED_HOSTS_OK: profile=production          required=19 forbidden=3
```

The anchors are load-bearing rather than decorative. Filtering each symbol out
of the real `llvm-nm --demangle` output of the production binary and re-running
`audit_symbols` produces exactly one absence each:

```
intact binary        -> no absences
launcher removed     -> required exact demangled anchor is absent:
                        agent process launcher constructor
panel removed        -> required exact demangled anchor is absent:
                        Qt agent launch panel constructor
```

`verify_composed_hosts_tests.py` passes 42/42 once its synthetic symbol table
lists the two new anchors — which is the same guard working: the fixture table
is required to name every anchor, so adding one without it fails six tests
immediately. `application_composition_contract` and
`application_agent_invocation_composition_contract` both pass.

No production or packaging file was touched. This card added two lines of
inventory and the fixture entries that keep the verifier's own suite honest.
