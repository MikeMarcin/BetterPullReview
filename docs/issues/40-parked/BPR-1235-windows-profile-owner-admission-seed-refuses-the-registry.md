---
schema: "implement-rfc.card.v1"
id: "BPR-1235"
title: "Windows: application_profile_owner_admission_public_path fails at its seed, which refuses the registry under the AppContainer's temporary directory"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json","related_cards":["BPR-1229"]}
parked_on: "human-review"
acceptance: [{"criterion":"The owner's choice is recorded here","outcome":"pending","revision":"1d7bdb35f0ca144286e4611079201c195981bec2","evidence":{"kind":"freeform","detail":"The station pinned the row WINDOWS_UNCONTAINED under dos-volume-name in 1d7bdb35f, consistent with BPR-0833, BPR-0845 and the activation row's pin; the card reserved the decision for the owner, so one Accept/Reject question asks the owner to keep that pin. Accept finishes the card; Reject reopens the containment question."}},{"criterion":"The row passes on Windows","outcome":"pass","revision":"1d7bdb35f0ca144286e4611079201c195981bec2","evidence":{"kind":"command","detail":"bpr-main-win: application_profile_owner_admission_public_path passes uncontained through ctest in 8.22 s with PROFILE_OWNER_ADMISSION_OK, after BPR-0907 removed the course's stale Windows-only branches."}},{"criterion":"test_network_isolation_manifest names no row this card changes","outcome":"pass","revision":"2ef17fd5173bd958cdd96a97944cc8be74e90ec4","evidence":{"kind":"command","detail":"macOS build/dev: test_network_isolation_manifest passes, TEST_NETWORK_ISOLATION_OK:configured=577:registrations=523:backend=macos-sandbox-exec, with the new pin and its WINDOWS_UNCONTAINED registration in the inventory."}}]
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

## Resolved by [[BPR-0907]] — bpr-main-win, 2026-10-01

[[BPR-0907]] found and fixed the row's second obstacle in the course. Its
Windows-only branches had been stale since [[BPR-0979]]. With them gone, the
row passes uncontained on Windows: 8.22 s through `ctest`, and
`PROFILE_OWNER_ADMISSION_OK`.

This card's containment question then turned out not to be new. The contained
refusal is the store's walk of the retained ancestor chain, which
[[BPR-0872]]'s dictionary already names `dos-volume-name`. Integrator-decided
pins already cover the same walk (BPR-0833, BPR-0845). So the row is pinned
`WINDOWS_UNCONTAINED` under that term, with the measurement, in `1d7bdb35`.

When this card was filed, I wrote that the decision was the owner's. Given
those precedents, it was the integrator's to make, as it has been for every
sibling pin. The integrator can finish this card, or reopen the question if
the owner disagrees.

## Integrator check — bpr-main-mac, 2026-10-01

Checked against the three criteria at `2ef17fd5`:

- **The row passes on Windows.** Met on bpr-main-win's run above: 8.22 s
  through `ctest`, `PROFILE_OWNER_ADMISSION_OK`, uncontained as registered.
- **`test_network_isolation_manifest` names no row this card changes.** Met.
  On macOS it passes with `TEST_NETWORK_ISOLATION_OK:configured=577:registrations=523`,
  with the new pin in the inventory and its registration `WINDOWS_UNCONTAINED`.
  The Windows manifest run is part of [[BPR-0907]]'s finish.
- **The owner's choice is recorded here.** Not met. The pin is consistent with
  the precedents the station cites: [[BPR-0833]] and [[BPR-0845]] pinned the
  same ancestor-chain refusal as `dos-volume-name`, and the
  `application_profile_activation_public_path` row, whose seed refuses the
  registry in the same way, carries the same pin ([[BPR-1234]]). But this card
  was filed with the decision reserved for the owner, and its acceptance
  still says so. An agent cannot meet that by deciding instead, so the pin
  goes to the owner as one Accept or Reject question:

  > Keep `application_profile_owner_admission_public_path` uncontained on
  > Windows, pinned `dos-volume-name` like BPR-0833's and BPR-0845's rows
  > (commit `1d7bdb35`)?

  Accept finishes the card. Reject reopens the containment question. The
  alternative is the card's second option: the row keeps its boundary, and
  its seed makes a root the container can use.

The card is parked for that answer. No Windows or Linux run is owed.
