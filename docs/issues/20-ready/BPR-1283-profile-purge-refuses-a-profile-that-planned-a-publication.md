---
schema: "implement-rfc.card.v1"
id: "BPR-1283"
title: "Profile purge refuses any profile that ever planned a publication: its inventory omits publication-git-v1"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1185 documentation pass (code reading)","reservation":"docs/issues/_reservations/BPR-1283--BPR-1283--9b8c97b80602.json","related_cards":["BPR-1177","BPR-1185"]}
---

# BPR-1283: Profile purge refuses any profile that ever planned a publication

## Observation

While documenting storage, [[BPR-1185]] read `release_maintenance_files.cpp` and
found that the profile purge inventory does not list `publication-git-v1/`, the
folder Publish as Pull Request ([[BPR-1177]]) creates in the profile root. Purge
preparation would therefore refuse any such profile with
`incomplete_inventory`. Seen by code reading only; no run reproduced it.

## Outcome

Reproduce it with a profile that has planned a publication, add the folder to
the purge inventory with the same ownership and safety rules as the other
profile stores, and pin it with a test and a negative control. Keep shared
updater and registry state out of purge, as now.

## Reproduced and fixed (station `bpr-main-linux`, 2026-10-04)

**Real.** A temporary, uncommitted listing at the end of
`application_review_cycle_publish_public_path` showed what a publish leaves in
the profile root: `publication-git-v1/` holding `.git-process-boundary.lock`,
`config-system`, `config-global`, `attributes-system`, the empty `home`,
`xdg`, `tmp`, `work`, `hooks` and `templates`, the empty `repositories/v1` and
`incoming/v1` skeleton, and the bare store `publication/v1/sha1.git` (`HEAD`,
`config`, loose objects, `objects/{info,pack}` and empty `refs/{heads,tags}`).
`test_publication_isolation_inventory` in
`tests/sqlite_contract/release_maintenance_files_tests.cpp` builds exactly that
layout. Before the fix it fails at "a profile that planned a publication is
inventoried for purge".

**Fix.** `release_maintenance_files.cpp` now classifies `publication-git-v1`
and its descendants, for purge only, by the same rules as the profile's own
isolation: the lock and the three config files as files; the work, home and
temp directories and the cache skeleton as directories that must be empty; and
`publication/v1/{sha1,sha256}.git` through the existing `repository_artifact`.
Anything else blocks the purge, and shared registry and updater state are
still excluded. The test also pins the refusals:

- an unknown file in the store gives `incomplete_inventory`;
- a file in `tmp` must be recovered by its owner;
- a store other than the catalog's own is refused;
- a confirmed purge removes `publication-git-v1` and keeps
  `state/v1/profile-registry.sqlite3`.

**Negative controls.** Without the fix, and with only the store name broken,
the row fails at its first check, and passes again once restored.

**Evidence on Linux** (`build/dev`, GCC 15.2.0):

- `sqlite_release_maintenance_files`, `core_release_maintenance`,
  `application_release_maintenance`,
  `application_release_maintenance_public_path`,
  `qt_release_maintenance_scale_100`,
  `application_review_cycle_publish_public_path` and `architecture_boundaries`
  pass, 7 of 7.
- `coding-format-check` (clang-format 22) passes, and `DOCUMENTATION_OK`.

The storage guide now says purge includes the folder. Closeout is left to the
main integrator.
