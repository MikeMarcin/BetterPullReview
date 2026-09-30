---
schema: "implement-rfc.card.v1"
id: "BPR-1235"
title: "Windows: application_profile_owner_admission_public_path fails at its seed, which refuses the registry under the AppContainer's temporary directory"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json","related_cards":["BPR-1229"]}
---

# BPR-1235: Windows: application_profile_owner_admission_public_path fails at its seed, which refuses the registry under the AppContainer's temporary directory

Found by bpr-main-win on 2026-09-29: Windows 11 Home 10.0.26200, MSVC 19.51,
Qt 6.11.2, `build/windows-dev` Debug with warnings as errors, at `22d18416`
with `537f718d`'s build fixes and again at later `main`. The station logs
are in `C:\Users\mikem\bpr-main-helpers\run\station-0929\` and are not committed.

## Observation

`application_profile_owner_admission_public_path` fails on Windows, contained:

```
Failed to find real location of C:\Program Files\Python314\python.exe
SEED_REGISTRY_REFUSAL:0
FAILED: seed registry
subprocess.CalledProcessError: Command '[…profile_owner_admission_public_path.exe', 'seed', '…\Packages\bprautomatedtest\AC\Temp\own-56ac5daa85c56f73\data', …]' returned non-zero exit status 1.
```

The seed's registry is refused under the AppContainer's temporary directory,
the shape of the other contained store rows ([[BPR-1229]]). Python's first line
is also a contained-launch symptom. The row is not in the pinned uncontained
set.

## The decision, which is the owner's

As with [[BPR-1229]], either the row is uncontained with a measured pin, or it
keeps its boundary and its seed makes a root the container can use. That is a
containment decision, so this card does not make it.

## Acceptance

- The owner's choice is recorded here.
- The row passes on Windows.
- `test_network_isolation_manifest` names no row this card changes.
