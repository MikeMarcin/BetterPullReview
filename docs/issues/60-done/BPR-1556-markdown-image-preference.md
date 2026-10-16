---
schema: "implement-rfc.card.v1"
id: "BPR-1556"
title: "A preference to load images in descriptions and comments, on by default"
status: "Done"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1549"]
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"inline-markdown-images-r1","governing_rfc":"RFC-BPR-0084","amendment":"RFC-BPR-0084 owner amendment 1 (2026-10-16)","criteria":["AM1-03"],"origin":"owner accepted Q3 in attended session 8a5e3c244f674294366fd8796da2897e, cursor 17","reservation":"docs/issues/_reservations/BPR-1554--BPR-1556--0108f19c75d6.json"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"06a06a2ea08242a7d57e2caed74afb094962e4a1","evidence":{"kind":"command","detail":"sqlite_workbench_settings_storage test_markdown_images_migration_and_round_trip: a version-twenty profile migrates to 21 with Load and every other choice kept; Don't load round-trips and survives restart; an unknown value cannot be written and a stored one is refused. Every legacy fixture now seeds without the new column."}},{"criterion":"AC02","outcome":"pass","revision":"06a06a2ea08242a7d57e2caed74afb094962e4a1","evidence":{"kind":"command","detail":"qt_center_ribbon_width_scale_* test_markdown_images_preference_follows_the_projection: Don't load dispatches the whole record, nothing changes until published, a published Don't load hides the overview's images, Reset returns Load. qt_markdown_images_* check_images_shown: off renders every image (loaded or not) as its placeholder, removes the objects and asks nothing even for a new body; on asks again. application_markdown_images_public_path: in the composed app, Settings' Don't load turns the description to placeholders and a comment opened afterwards shows placeholders with MarkdownImageResolutions and MarkdownImageFetches both 0; Load shows the images again. Negative control: ignoring the preference in workbench_window fails both the Qt and the composed check."}},{"criterion":"AC03","outcome":"pass","revision":"06a06a2ea08242a7d57e2caed74afb094962e4a1","evidence":{"kind":"command","detail":"Settings > Review behaviour > Descriptions and comments: Images, Load / Don't load (accessible names 'Images: Load' and 'Images: Don't load'), an information glyph and Reset, laid out by the same row builder as Line details and Drawer controls."}}]
---

# BPR-1556: A preference to load images in descriptions and comments, on by default

Under [RFC-BPR-0084 owner amendment 1](../../rfcs/RFC-BPR-0084-inline-markdown-images.md#owner-amendment-1-2026-10-16-answers-to-q1-q3):
Settings > Review behaviour gets "Descriptions and comments: Images — Load / Don't load", Load by default, persisted in the profile and applied live.

## Acceptance

- **AC01 (AM1-03):** Storage tests show the preference persisted with schema version 21, a migrated row loading images, and an invalid value refused.
- **AC02:** Qt and composed tests show the preference applied live in an open window: Don't load turns every image into its placeholder and sends no request; Load shows them again.
- **AC03:** The settings row has an accessible name, a Reset and an information glyph, and follows the other Review behaviour rows.
