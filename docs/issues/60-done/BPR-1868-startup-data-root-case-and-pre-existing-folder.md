---
schema: "implement-rfc.card.v1"
id: "BPR-1868"
title: "A data root in another letter case starts, and a refused pre-made root says why"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux"]
metadata: {"kind":"bug","found_by":"Owner, 2026-11-04: the application exited at startup with no window, printing STARTUP_FAILED_CAUSE: startup_failure=registry and STARTUP_PROFILE_REFUSAL:registry_unavailable, when --test-application-data-root named /Users/jaeger/BPR-attended/... over the folder /Users/jaeger/bpr-attended, and when the data root already existed before the first launch","reservation":"docs/issues/_reservations/BPR-1868--BPR-1868--cb09187aed59.json","related_cards":["BPR-0817","BPR-0843","BPR-0877","BPR-0882"],"frontend_design":"not loaded: no surface the owner looks at changed; the change is a startup admission and a developer diagnostic line"}
acceptance: [{"criterion":"AC1 A data root typed in another letter case than its folders starts on a case-insensitive volume","outcome":"pass","revision":"bac64e5f2","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(application_startup_data_root|sqlite_storage_root_contract)$' --output-on-failure","detail":"Before the fix the offscreen app with --test-application-data-root <scratch>/BPR-attended/data over <scratch>/bpr-attended exited 1 with startup_failure=registry and registry_unavailable; after it, it stays up. The contract runs --self-test over <root>/attended/data under the folder Attended: exit 0, the root is created as Attended/data; then <root>/ATTENDED/data (existing, third case) starts again. storage_root_on_disk_spelling: the folder's spelling for a missing and an existing miscased root; unchanged for a canonical root, a missing child of one, a root under or at a symbolic link, and a non-normal spelling; on a case-sensitive volume another case keeps its spelling. The stores still refuse the miscased spelling given directly."}},{"criterion":"AC2 A data root made beforehand starts when it is owner-private, and is refused with its cause named otherwise","outcome":"pass","revision":"bac64e5f2","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(application_startup_data_root|sqlite_storage_root_contract)$' --output-on-failure","detail":"A root made with mode 0700 before the first launch starts (self-test exit 0) and the catalog accepts it. A root made with 0755 (what mkdir -p gives) is still refused and not repaired, exit 1, stdout exactly STARTUP_FAILED, stderr keeps startup_failure=registry and adds STARTUP_FAILURE_SOURCE:registry-root-permissions: the data root folder already exists and is not owner-private (owned by this user, mode 0700); run chmod 700 on it, or name a folder that does not exist yet. Nothing is written into the refused folder."}},{"criterion":"AC3 Neighbouring startup, profile and boundary courses","outcome":"pass","revision":"bac64e5f2","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(architecture_boundaries|architecture_boundary_contract|application_profile_startup_admission|application_profile_startup_public_path|application_profile_management_public_path|application_startup_failure_source|application_startup_data_root|sqlite_storage_root_contract|sqlite_profile_storage_root|application_private_draft_restore)$'; cmake --build --preset dev --target coding-format-check","detail":"10 of 10 pass; clang-format 22 check passed on every file."}}]
---

# BPR-1868: A data root in another letter case starts, and a refused pre-made root says why

## Report

The owner, 2026-11-04: the application exited at startup with no window,
printing `STARTUP_FAILED_CAUSE: startup_failure=registry` and
`STARTUP_PROFILE_REFUSAL:registry_unavailable`, in two cases:

1. `--test-application-data-root /Users/jaeger/BPR-attended/...` while the
   folder on disk is `/Users/jaeger/bpr-attended` (APFS, case-insensitive).
2. The data root already existed before the first launch (`mkdir -p`).

Both reproduced offscreen on the unpatched build; a fresh root under an exactly
spelled parent started.

## Root cause

Every store admits a root only when `std::filesystem::canonical` of it equals
the spelling it was given. That rule is how a link anywhere in the path is
refused, and it stays. On macOS `canonical` (`realpath`) returns the letter
case the folders have on disk, so a root typed in another case failed it:
the startup host's ancestor check when the root was missing, and
`validate_private_storage_root` when it existed. Nothing was wrong with the
root; only its spelling differed.

The pre-existing root was refused on purpose: a folder made beforehand is never
repaired, and `mkdir -p` makes it `0755`, which the owner-private rule
(owned by this user, mode `0700`) refuses. A root made beforehand with `0700`
was already accepted. What was wrong was the diagnostic: every refusal on the
registry root reached the reader as `registry_unavailable`, so the two cases
could not be told apart.

## Built

- **SQLite adapter.** `storage_root_on_disk_spelling` (`storage_root.hpp`):
  the canonical spelling of the root's deepest existing folder, with the
  missing components appended as given, when every existing component is a
  real directory (no link), the canonical path has as many components and
  names the same device and inode. Otherwise, and always on Windows, the root
  is returned unchanged, so a spelling through a link still reaches the checks
  that refuse it. It creates and changes nothing.
- **Composition.** The startup host spells its registry root with it before
  anything else reads the root. Each refusal in the registry root's admission
  records a fixed-text cause on the existing startup-failure-source line
  ([[BPR-0882]]): `registry-root-path`, `registry-root-ancestor`,
  `registry-root-create`, `registry-root-link`, `registry-root-not-directory`,
  `registry-root-permissions`, `registry-root-spelling`,
  `registry-root-unavailable`. The mapped failure and the two existing lines
  are unchanged, so readers that match them still do.
- **Tests.** `sqlite_storage_root_contract` gains the pre-made private root
  and the spelling cases; `application_startup_data_root` (macOS and Linux)
  drives the shipped entry point through the three startup cases.

## Open

- A pre-made root with a shared mode is still refused; the owner-private
  rule is deliberate and a folder that existed is not repaired. The diagnostic
  now names it and says how to make it usable.
- Windows keeps its own root rule (`canonical_windows_root`); a data root
  typed in another case there was not examined, and the new course does not
  run on Windows.
- The cause line is printed by tests-enabled builds only, as the other
  startup-failure-source names are.
