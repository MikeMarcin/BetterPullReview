---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ3"
title: "Clone folders find the reviewer's clone by its remote and remember it per repository"
status: "Done"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-PENDING-AQ1"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0092 adoption, 2026-10-22 (owner decision D2)","related_cards":["BPR-PENDING-AN"],"milestone":"RFC-BPR-0092 M3"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "869385850", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^(core_clone_discovery|workspace_clone_folder_scan|application_clone_link_host)$'", "detail": "core_clone_discovery: remote URLs read from config text with quoting, comments, other sections, include and an unterminated value skipped; bounds on remotes, size and URL length; HTTPS, scp-like, SSH with port, git:// normalized; a token or user:password dropped; local paths, file URLs, deeper or dotted paths and other schemes refused; matches ordered by link then folder, upstream-only and GHES cases. workspace_clone_folder_scan on real scratch folders: clones one and two levels down found, three levels down, hidden folders, a linked worktree's .git file, a link to a clone and a link to a folder not; the inventory of every file and mtime is unchanged by the scan; the canary token appears in no match; a config is read only to its bound; only existing usual folders are suggested. No process: the adapter runs nothing and the host test uses scripted reads."}}, {"criterion": "AC2", "outcome": "pass", "revision": "869385850", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^(application_clone_link_host|sqlite_agent_setup_storage|application_review_round_trip_composition_contract|command_correctness_registry_contract|architecture_boundaries)$'", "detail": "application_clone_link_host: listing, removing and refusing folders, discovery only in listed folders, choosing a clone of another repository or a non-clone refused with its reason, a matching clone linked and remembered; a second host over the same store uses the still-serving link, keeps it across a rename by identity, and reports a moved clone (not_a_clone) and a re-pointed one (no_matching_remote); the private copy replaces the link; Forget removes it. sqlite_agent_setup_storage stores folders in order and links across a restart, refuses a private copy with a path, and Forget removes one. Settings' rows for listing, changing and forgetting are BPR-PENDING-AQ6's presentation over this host."}}]
---

# BPR-PENDING-AQ3: Clone folders find the reviewer's clone by its remote and remember it per repository

RFC-BPR-0092 M3 ([RFC](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md#milestones)), owner decision D2.

## Scope

- The core's `.git/config` remote reading, URL normalization (HTTPS, SSH,
  scp-style), user-information removal and repository matching.
- The adapter's bounded read of listed folders: two levels, no links, at most
  2,000 directories per folder, `.git/config` (64 KiB) and `.git/HEAD` only.
- The stored clone folders and the per-repository link by GitHub identity,
  checked again before each use.

## Acceptance criteria

- AC1: RFC AC04's fixture folders give exactly the matches and order defined
  in the RFC, with no process, no write and no user information anywhere.
- AC2: RFC AC05's remembering, rename, moved clone and Forget behave as
  defined.
