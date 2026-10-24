---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ5"
title: "Every review request carries the brief and the editable bpr-pr-review skill"
status: "Done"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-PENDING-AQ1"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0092 adoption, 2026-10-22 (owner decision D1, answer Q9 A)","related_cards":["BPR-PENDING-AN","BPR-PENDING-AQ4","BPR-PENDING-AQ6"],"milestone":"RFC-BPR-0092 M5","deviation":"The edited copy lives in agent-setup.sqlite3, not as files under agent-skills/; Open in editor is not offered yet; accepted by the owner in BPR-PENDING-AQ8's look","owner_confirmation":"accepted 2026-10-22, session 89855cd96f098cd74e28d271acd60d8a Q3"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "c3dda29b6", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^(core_review_skill|core_agent_context_supplement|application_agent_launch|application_agent_invocation_host|application_agent_invocation_composition_contract|git_process_contract|qt_agent_launch_canary)$'; ctest --preset core-only -R 'core_review_skill|core_agent_context_supplement'", "detail": "core_review_skill: the title cut to 1 KiB, the description cut at a line with 10001 bytes counted, a file without a path left out and counted, threads unresolved first with an oversized one left out and counted and a later one still fitting, an empty thread dropped uncounted, bounding deterministic, a brief outside its bounds refusing the supplement, version 3 carrying it unchanged. core_agent_context_supplement: the supplement type gains only the brief, whose fields hold prose, counts, the workspace kind and the skill text - no argv, environment or token. application_agent_launch: every brief field encoded after draft.count, the diff named, the skill text whole, no brief line without a brief, the workshop press's bytes equal the BPR-1091 pin with only its schema line read as version two. application_agent_invocation_host: the launched brief names linked_clone_history, private_copy or private_snapshot from the workspace that was made. application_agent_invocation_composition_contract: the composed Ask on the fixture carries schema 3, the fixture title and base branch, private_copy, git diff BASE...HEAD, the built-in skill of version 1, and no credential canary. git_process_contract: a snapshot reports Shallow, a borrowed parent clears it, the private copy holds the base."}}, {"criterion": "AC2", "outcome": "pass", "revision": "c3dda29b6", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^(core_review_skill|application_review_skill_host|sqlite_agent_setup_storage)$'", "detail": "core_review_skill: the shipped SKILL.md is bpr-pr-review version 1; front matter with bare, quoted and CRLF values; refusals for no front matter, unclosed, missing or changed name, missing or empty description, over 64 KiB and not UTF-8; a newer built-in offered to a copy, never to the default, Keep mine remembered for that version and version 4 offered again. application_review_skill_host: the built-in text by default and in the brief; the first edit stores the copy with base version and SHA-256; a renaming save refused with its reason keeping the copy; a refusing store leaves the copy; Restore default removes nothing when the Downloads copy fails, then writes it and removes the copy, and unticked writes none; version 2 offered and the edited copy kept; a broken stored copy reported with the built-in text sent. sqlite_agent_setup_storage: the copy and its record round-trip across a restart, Keep mine's version stored, oversize refused, Restore default removes it, preferences and links untouched. The Settings rows are BPR-PENDING-AQ6's; Open in editor is not offered (deviation pending the owner's confirmation)."}}]
---

# BPR-PENDING-AQ5: Every review request carries the brief and the editable bpr-pr-review skill

RFC-BPR-0092 M5 ([RFC](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md#milestones)), owner decision D1 and answer Q9 A.

## Scope

- Context supplement version three: `core::supplement_brief` with the
  title, description (16 KiB, cut at a line, the cut counted), base branch,
  changed files with their status, review threads (64 KiB, unresolved
  first, omissions counted), the workspace's kind and the review skill's
  text; `core::bound_review_brief` bounds it deterministically and
  `build_agent_supplement` refuses a brief outside its bounds. The brief's
  lines follow every version-two field and are written only when present.
- `application::read_review_brief` reads it from the accepted snapshot; the
  window gives it to every review request (each entry point but a
  comment-workshop press, never a writing request). The invocation host
  names the workspace that was made (`linked_clone_history`,
  `private_copy`, `private_snapshot`); the private copy now holds the base
  the cache has, so the named `git diff BASE...HEAD` works.
- The built-in `bpr-pr-review` skill, `packaging/agent-skills/bpr-pr-review/
  SKILL.md` with `metadata.bpr-version: 1`, built into the application.
  `core::inspect_review_skill` reads its front matter (a narrow subset, not
  a YAML reader) and refuses a skill over 64 KiB, not UTF-8, without front
  matter, renamed or without a description.
- `application::review_skill_host`: the built-in text until an edit; the
  first Save of other text stores the copy with the built-in version and
  SHA-256 it came from; Restore default and Use version N save the copy to
  Downloads first and remove nothing when that fails; a newer built-in is
  offered and never applied by itself; Keep mine is remembered for the
  offered version; a stored copy that is no longer a review skill is
  reported and requests carry the built-in text. The copy is stored by
  `adapters::sqlite::make_review_skill_store` in `agent-setup.sqlite3`.

## What changed from the RFC

- The edited copy lives in `agent-setup.sqlite3` (settings, which resets and
  purge already cover) rather than as files under the profile's
  `agent-skills/bpr-pr-review/`, because the profile maintenance inventory
  admits only paths it knows. **Open in editor** is not offered yet; the
  Settings editor is how the skill is edited.
- The Settings editor itself (Save, Restore default, Compare, Use version N,
  Keep mine) is [[BPR-PENDING-AQ6]]'s presentation over this host.

The owner is asked to confirm the storage and the missing **Open in
editor** in [[BPR-PENDING-AQ8]]'s look.

## Acceptance criteria

- AC1: RFC AC08.
- AC2: RFC AC09, the host's share (the Settings rows are AQ6's).

## Owner verdict

Accepted 2026-10-22 in [[BPR-PENDING-AQ8]]'s look v001 (session `89855cd96f098cd74e28d271acd60d8a`, Q3).
