---
id: BPR-0509
title: A station cannot relaunch without retyping --machine, and cannot start over
status: Done
rfc: none
depends_on: []
---

# A station cannot relaunch without retyping `--machine`, and cannot start over

## Why

Operator report from the Windows station, 2026-09-01. The argument-free
command [[BPR-0508]] delivered refused on this checkout:

```
starting fresh unified station bpr-win under station-writer lease
station refused: Codex task 01a05f77-61ce-7662-bbad-cef53be050c1 is named
'bpr-win-jaegaw', not 'bpr-win'; refusing to rename a task with unrelated history
```

Two separate gaps produced it.

First, inference stopped halfway. `.remote-station.json` records
`"station": "bpr-win-jaegaw"`, and [[BPR-0508]] taught the launcher to read
the *workstream* back out of that file — but not the machine suffix. So a
relaunch recomputed `bpr-win`, kept the recorded `codex.thread_id` from the
same file, and then hit the rename guard that exists precisely to stop a
station from adopting a task with unrelated history. The guard is right; the
name it was handed was not. Nothing about the checkout changed, so nothing
about its name should have, and the operator's only recourse was to remember
`--machine jaegaw` on every launch forever — which
`docs/development/remote-stations.md` did in fact instruct, and which is not
a contract anyone should have to carry in their head.

The suffix is not cosmetic here: `docs/issues/_reservations/` holds an
immutable id-reservation receipt naming `bpr-win-jaegaw`, and the station
registry lists it. Recomputing a shorter name silently is the wrong repair.

Second, there was no way out. Every identity in the station file is durable by
design — `station_id` "may not" change, `codex.thread_id` is "immutable" — and
that design has no counterpart for the case the operator actually had: a
station whose recorded bindings are wrong and which should simply start again.
`--continue` is stricter, not looser; `--new-claude-session` resets one half of
one runtime. Deleting the file by hand is not equivalent, because the Codex
task keeps the station name and the next launch adopts or collides with it.

## Required final state

- An argument-free relaunch of a checkout that recorded a machine suffix
  resolves to the recorded name. Inference reads `station` from the station
  file the way the workstream is read, and ignores a value that is not a well
  formed machine token.
- An explicit `--machine` still works, and refuses when it contradicts the
  recorded station rather than silently renaming it.
- `--fresh-station` starts the station over: it archives the recorded Codex
  task and anything else answering to the station name in this checkout,
  discards the recorded `station_id`, Codex task and Claude session, and
  provisions the workstream's canonical name — machine suffix dropped, so the
  flag is also the deliberate rename. Nothing is deleted.
- `--fresh-station` refuses next to `--continue`, which requires the session it
  discards, and next to `--no-server`, which reaches no App Server and so could
  only pretend the recorded task was released.
- A task belonging to another checkout is never archived, whatever the station
  file records.
- The launcher usage text, the root forwarder, `references/remote-routing.md`
  and `docs/development/remote-stations.md` describe both, and the operator doc
  stops telling JaegAW to keep passing `--machine`.

## What this slice delivered — 2026-09-01

`infer_machine(station_file, canonical_name)` in the portable launcher, read
in `resolve_context` exactly where `infer_workstream` is and skipped under
`--fresh-station`; the resolved token is written back onto the parsed
arguments so the supervised child is told the name rather than re-deriving it.
`release_named_threads` in `codex_station.py` archives the recorded task and
every task answering to the station name in this checkout, and refuses a task
belonging to another checkout. `provision_codex(..., fresh=True)` calls it
with the client it already holds — one place covering both the managed-daemon
and native-Windows transports — and then forgets the recorded id so
provisioning creates rather than adopts. `supervise` reads the station file
for that archiving and otherwise remembers nothing from it.

| Claim | Command | Result | Commit |
|---|---|---|---|
| Suffix inference, its rejection of a malformed record, the `--machine` contradiction refusal, the canonical name under `--fresh-station`, forwarding to the child, the archive-then-provision order, and that a normal relaunch archives nothing | `python3 -m unittest tests.test_launch_remote_station` in the skill root | 54/54, up from 42 | this commit |
| Archiving the recorded task under its own name, every task answering to the name, already-archived and missing tasks, and the other-checkout refusal | `python3 -m unittest tests.test_codex_station` in the skill root | 29/29, up from 24 | this commit |
| The reported refusal is gone on the station that hit it | `resolve_context` over this checkout's real `.remote-station.json` | argument-free launch resolves `bpr-win-jaegaw`, matching recorded task `01a05f77`; `--fresh-station` resolves `bpr-win`; `--machine nuc` refuses | this commit |
| The `--fresh-station` combination guards | `main(["--fresh-station", "--continue"])` and `main(["--fresh-station", "--no-server"])` | both exit 2 with the stated reason | this commit |
| The documentation gate | `py -3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| The whole skill suite and the portable manifest | `render_runtime_adapters.py` then `validate_portable_agents.py` | manifest regenerated; 139/140, with the one failure pre-existing and unrelated — see below | this commit |

`validate_portable_agents.py` fails one test on a native-Windows host, before
and after this change:
`test_render_runtime_adapters.test_validator_executes_absolute_paths_but_displays_stable_paths`
asserts the rendered `--repo-root` argument equals the literal `/explicit/repo-root`,
while the validator normalizes it through `Path`, which spells it
`\explicit\repo-root` here. It is a POSIX-only assertion in a test this slice
does not touch, and it needs its own card.

## Manifest regenerated — 2026-09-02

The launcher change landed without `generated/portable-manifest.json` being
re-rendered, so `validate_portable_agents.py` reported the manifest stale
on the integrator host. Re-rendered with `render_runtime_adapters.py`; the
suite passes (140 tests, 2 skipped) and `--compare` against the PlaceOfMind
copy reports `portable set identical`.
