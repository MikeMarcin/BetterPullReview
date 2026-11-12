---
schema: "implement-rfc.card.v1"
id: "BPR-2020"
title: "Persist local PR draft candidates and author notes"
status: "Done"
rfcs: ["RFC-BPR-0108"]
metadata: {"criteria":["AC02","AC04","AC07"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0005","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0032-persist-local-pr-draft-candidates-and-author.md"}
depends_on: ["BPR-2019"]
acceptance: [{"criterion":"AC02","outcome":"pass","revision":"3585dedc7f50ec7296cd345a44b2fa1fe87378cc","evidence":{"kind":"command","detail":"Modified and untracked files listed apart, ignored not; only a selected file enters, with exact working bytes and the includes-working-files qualification; filtered or over-bound files cannot be selected","command":"ctest --preset dev -R '^(core_local_draft|local_draft|application_local_draft_desktop_public_path)$'"}},{"criterion":"AC04","outcome":"pass","revision":"3585dedc7f50ec7296cd345a44b2fa1fe87378cc","evidence":{"kind":"command","detail":"Successor keeps earlier candidates; stale writes refused; external edits do not move kept bytes; restart reads candidates and notes; branch switch, moved folder and path reuse do not retarget","command":"ctest --preset dev -R '^(local_draft|application_local_draft_desktop_public_path)$'"}},{"criterion":"AC07","outcome":"pass","revision":"3585dedc7f50ec7296cd345a44b2fa1fe87378cc","evidence":{"kind":"command","detail":"Discard removes only private preparation (worktree, branch, edits unchanged); resets remove local-drafts and local-draft-git-v1 as review data; a full store answers full 64 of 64 and evicts nothing","command":"ctest --preset dev -R '^(local_draft|core_profile_data_reset|sqlite_profile_data_reset_files|application_local_draft_desktop_public_path)$'"}}]
---

# Persist local PR draft candidates and author notes

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0108](../../rfcs/RFC-BPR-0108-local-pr-drafts.md), criteria `AC02`, `AC04`, `AC07`.

Core local subject and candidate transitions; application capture/lifecycle and profile-private persistence, with shared material services; discard, bounded retention and coverage by the existing profile resets.

## Intended verification

Demonstrate dirty selection, restart, concurrent writer/late completion, branch switch and rebind without retargeting or partial ready material. Discard a draft and run a profile reset with the worktree unchanged; fill the store and observe the refusal.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

## Implementation record

Built with [[BPR-2019]] (commits `c56f0b638`, `1698f5b67`, `207aeb7d9`),
product candidate `3585dedc7`: `capture_local_draft` keeps both sides of
every changed file and re-reads included working files against the bytes
listed; `state/v1/local-drafts.sqlite3` installs a candidate's objects with
the record that names it, in one transaction, refuses a write against an
older version and refuses past its bounds; discard removes the record and the
objects only it named; `local-drafts` and `local-draft-git-v1` are review
data for the resets. Thresholds, results and limits:
[ledger](../../verification/RFC-BPR-0108.md).
