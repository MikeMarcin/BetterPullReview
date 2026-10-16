---
schema: "implement-rfc.card.v1"
id: "BPR-1549"
title: "Keep inline images in a bounded on-disk cache in the profile"
status: "Done"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1548"]
metadata: {"mandate_revision":"inline-markdown-images-r1","milestone_plan":{"id":"inline-markdown-images-v1","path":"docs/rfcs/plans/inline-markdown-images-v1/milestone-plan.json"},"milestone":"image-disk-cache"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"04090b526abed820481eb6202602f3237641e73d","evidence":{"kind":"command","detail":"ctest sqlite_markdown_image_disk_cache: a new store instance reads the same bytes back (restart); another account, host and public scope miss; seven days without use expire an entry and a use restarts the seven days; thirteen 10 MiB images under the real 128 MiB bound evict the least recently used while the one just used stays; the file stays within the bound (auto_vacuum), is mode 0600 and holds no key text; stopped, empty and over-bound requests are refused. Negative controls (no eviction, no expiry, no touch on use) each fail it. application_markdown_image_host window_restart_from_disk: a restarted service shows the cached image with no resolution and no fetch, only decoded bytes are stored, no signed address in any key, a superseded probe publishes nothing. application_markdown_images_public_path restarts the composed application on the same profile: all eight cached description images load and only the never-cached SVG is fetched (fixture evidence MarkdownImageFetches == 1); with the disk cache unplugged this fails. sqlite_release_maintenance_files: profile purge removes markdown-images.sqlite3 and its WAL."}}]
---

# BPR-1549: Keep inline images in a bounded on-disk cache in the profile

A bounded owner-private on-disk image cache in the profile serves images
across restarts and is removed by profile purge
([RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md) AC06).

## Acceptance

- **AC01:** Tests prove the 128 MiB bound, host and account isolation,
  seven-day expiry, a restart showing a cached image without a request, and
  purge removal.
