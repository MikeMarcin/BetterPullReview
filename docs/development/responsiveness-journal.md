# Responsiveness improvement journal

One line per verified improvement; linked cards hold scope, measurements and checks.

- 2026-11-06 — [[BPR-1882]]: Resolve reply ancestry once and reserve the full conversation projection up front; a 10,000-comment chain fell from 1,330 ms to 1.4 ms in the local Release benchmark, with parent identities, ordering and cycle fallback preserved.
- 2026-11-06 — [[BPR-1883]]: Show refreshed conversations before commit pagination and label the remaining loading; median conversation visibility improved from 1,999 ms to 1,673 ms with 330 ms simulated reads, the same 18 requests and an uninterrupted comparison.
