# Fixture Catalog

The implementation must maintain sanitized deterministic fixtures covering at least:

## GitHub hosts

- GitHub.com current contract
- GHES 3.18 REST and GraphQL contract recordings
- Missing GraphQL capability
- Permission denied
- Rate limited and secondary-rate-limited responses
- Proxy and corporate CA configuration
- Stale head during mutation

## Pull request histories

- Single upload, simple edit
- Multiple uploads with unchanged files
- Force-push/rebase with retained old head
- Base branch advance without head movement
- Base branch change
- Pure rename
- Rename with edits
- Added/deleted/binary/submodule files
- Review associated with historical commit

## Diffs

- Replace, insert, delete, and mixed hunks
- Unequal old/new group heights
- Multiple nearby groups
- Long lines and tabs
- Unicode and invalid byte replacement policy
- Multiline comment/string crossing hunk boundary
- Whitespace-only policy changes
- Large generated file and highlighting fallback

## Conversations

- New thread, replies, edits, deletion, resolution, reopening
- Outdated anchor
- File-level comment
- Pending review drafts
- External-client mutation between refreshes

## Local state

- Read and acceptance invalidation
- Account/repository isolation
- Schema migration
- Abrupt termination and WAL recovery
- Deleted local database

## AI

- Codex one-shot JSONL
- Codex App Server session and approval events
- Claude stream-json
- External terminal fallback
- Head changes during analysis
- Invalid finding anchor
- Corporate host mapped to forbidden personal provider

## Architecture and concurrency harnesses

- Core-only build environment with Qt, SQLite, GitHub, Git, AI, and platform provider packages unavailable
- Deterministic event/observation replay corpus for navigation, receipts, defects, queue state, and AI staleness
- Manual executor fixtures that reorder, delay, fail, cancel, and deliver stale completions
- Qt model harness asserting exact insert/remove/move/data-change signals and persistent-index behavior
- QAction projection harness covering enabled, checked, and disabled-reason state across capabilities and selections
- Large-review structural counters proving active-set/changed-set work and absence of routine whole-model resets
- Shutdown fixture with pending GitHub, Git, diff, syntax, store, queue, and AI operations
