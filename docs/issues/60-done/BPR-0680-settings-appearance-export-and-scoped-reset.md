---
schema: "implement-rfc.card.v1"
id: "BPR-0680"
title: "Complete appearance export and scoped defaults"
status: "Done"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0592"]
acceptance: [{"criterion":"AC-03/04/05/06/09/10 scoped appearance export/reset","outcome":"pass","revision":"ae2729f3 plus finish","evidence":{"kind":"observation","detail":"Separate serialized host export and reset ingress; Qt selects destination only, composition exports accepted palette through bounded atomic QSaveFile with direct fallback disabled. Independent mode/interface/code/palette resets preserve unrelated fields. Palette reset retires pending imports and clears preview; format 3 persists PaletteStored=false. Transactional v1/v2 migrations retain legacy customization; newer/unavailable/corrupt store coverage retained."}},{"criterion":"Focused host, persistence, native controls and selected executable path","outcome":"pass","revision":"ae2729f3 plus finish","evidence":{"kind":"command","detail":"application_appearance_host and qt_appearance_settings_panel pass; reset restart regression failed before preserving PaletteStored and passes after. sqlite_appearance_settings_storage passes (final 0.44s), including v1/v2 migration and close/reopen reset. application_appearance_theme_customization_contract passes (35.73s): real exported bytes equal accepted fixture, directory refused, named palette reset clears preview, review identities unchanged, persisted theme reopens. architecture_boundaries passes (12.98s). App build, canonical tidy on seven changed TUs (composition rerun after making bounded size explicit), clang-format and git diff --check pass; documentation gate passes. No live mutations or human verdicts."}}]
---

# Complete appearance export and scoped defaults

## Scope

Extend the existing appearance host and exchange port for export and precise resets; preserve preview/keep/cancel and tuned defaults.

Owned paths: appearance core/host/ports; narrow file adapter; appearance panel; focused host/store/Qt tests.

## Acceptance

AC-03/04/05/06/09/10: export round-trip, exact defaults, refusal restore, cancelled preview, restart and nonregular-file refusal through the selected public path.

See [the inventory](../../development/settings-authority-inventory.md) for exact existing selectors and authority.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.

## Evidence

Implementation now includes explicit export and reset ingress, atomic bounded
file replacement, native controls, and format-3 appearance persistence. The first
restart test exposed the old unconditional `PaletteStored = true` restore; the
store now carries that flag and migrates legacy records as customized. Format-1
and format-2 records rebuild transactionally under the hardened SQLite policy.
Focused checks and the selected executable course pass; no human verdict is claimed.

## Selected implementation

Extend the existing appearance host's serialized ingress and request records.
Add a bounded optional document-write port and an export completion projection;
Qt chooses a destination and composition supplies the current accepted palette.
Reset names mode, interface font, code font or palette and restores only that
part of the initial composed defaults. Palette reset clears stored overrides
and preview through an accepted publication; it never deletes settings files.
Exercise export bytes, cancellation/refusal, independent resets and persistence
in host tests, then the composed appearance path. The public-preview attended
candidate was released under the owner's resume-development authorization;
its prior verdict history remains bound to its original build.
