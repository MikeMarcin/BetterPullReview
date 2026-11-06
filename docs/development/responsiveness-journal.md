# Responsiveness improvement journal

One line per verified improvement; linked cards hold scope, measurements and checks.

- 2026-11-06 — [[BPR-1882]]: Resolve reply ancestry once and reserve the full conversation projection up front; a 10,000-comment chain fell from 1,330 ms to 1.4 ms in the local Release benchmark, with parent identities, ordering and cycle fallback preserved.
- 2026-11-06 — [[BPR-1883]]: Show refreshed conversations before commit pagination and label the remaining loading; median conversation visibility improved from 1,999 ms to 1,673 ms with 330 ms simulated reads, the same 18 requests and an uninterrupted comparison.
- 2026-11-06 — [[BPR-1885]]: Publish refreshed conversations before additional viewed-file status pages; a 201-file Release provider fixture with two 330 ms page waits improved from 673 ms to 17 ms, retaining all 20 requests, final validation and visible remaining loading.
- 2026-11-06 — [[BPR-1886]]: Classify conversation-model ancestry once per update; a 10,000-comment Release fixture with reply chains 250 levels deep fell from 39.6 ms to 9.0 ms while preserving hierarchy, row order, incremental signals and persistent indexes.
- 2026-11-06 — [[BPR-1887]]: Move the pull-request conversation link into the Comments toolbar so navigation and unread counts stay clear of line-aligned comment text, preserving keyboard access and refresh loading feedback.
