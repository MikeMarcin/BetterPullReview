---
schema: "implement-rfc.card.v1"
id: "BPR-0508"
title: "The station launcher should infer its workstream and say what it opens"
status: "Done"
rfcs: []
depends_on: []
---

# The station launcher should infer its workstream and say what it opens

## Why

Product-owner feedback on the [[BPR-0491]] fix, 2026-09-01, in two parts.

First, the documented command still asked for `win` or `linux`. In this
project the workstreams *are* platforms, so on a `windows-lane` checkout the
argument says nothing the checkout does not already know; the Aito launcher's
positional is a project lane, not a platform, which is why it never felt
redundant there.

Second, the corrected launcher opens an interactive Claude session in the
terminal, and nothing on screen said that this is the station rather than an
accident. The interactive form is the only one that creates a named session
at launch — the [[BPR-0491]] probes measured every server mode creating none
— so the terminal becoming the session is the fix, but it has to be announced.

## Required final state

- `scripts/launch_remote_station.py` with no positional argument launches the
  right workstream: from `.remote-station.json` when the checkout was
  launched before, otherwise from the checkout's branch, since every
  workstream in `config/project.json` names exactly one branch. Nothing is
  inferred from the host platform, because a Windows host can hold the
  integrator checkout.
- An explicit workstream is still accepted and still refuses when it
  disagrees with the checkout; a branch no workstream names refuses with the
  list of workstreams and their branches.
- Before Claude takes the terminal, the launcher states that the interactive
  session it opens is the station, that it stays running, and how it is
  driven; the operator doc and the launcher's usage text say the same.

## What this slice delivered — 2026-09-01

`infer_workstream(station_file, current_branch)` in the portable launcher,
consulted only when the positional is omitted (`nargs="?"`); the usage text,
the pre-start notice, `docs/development/remote-stations.md`'s launch step, the
root forwarder's docstring, and the portable runtime reference now describe
the argument-free command and what the operator sees.

| Claim | Command | Result | Commit |
|---|---|---|---|
| Inference from the station file, from every workstream's branch, refusal on an unknown branch, and the no-argument parse | `python3 -m unittest tests.test_launch_remote_station` in the skill root | 42/42, including `test_workstream_is_inferred_from_the_station_file`, `test_workstream_is_inferred_from_the_branch`, `test_unknown_branch_without_a_station_file_refuses_to_guess`, `test_omitted_workstream_resolves_from_the_checkout` | this commit |
| The whole skill suite and the portable manifest | `render_runtime_adapters.py` then `validate_portable_agents.py` | suite OK; manifest regenerated | this commit |
| The documentation gate | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |

The same change is applied to the PlaceOfMind copy of the skill so
`validate_portable_agents.py --compare` keeps reporting the portable set
identical.
