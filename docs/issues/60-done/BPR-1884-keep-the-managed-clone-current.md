---
schema: "implement-rfc.card.v1"
id: "BPR-1884"
title: "Keep the BetterPullReview-managed clone current, so agent deltas stay small"
status: "Done"
rfcs: ["RFC-BPR-0092"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"BPR-1879 implementer's open item; owner asked for the card 2026-11-06","related_cards":["BPR-1873","BPR-1879"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "fd47341d6", "evidence": {"kind": "command", "detail": "gh_cli_git_smart_transport (fake gh running real Git against a local source): refresh_managed_clone brings the kept clone the head with its files, names refs/bpr/pull/N/head and base, leaves no staged file and an owner-private pack, and every head object is present; a clone already current fetches nothing (with clone refused it still succeeds); after the upstream advances to a newer commit, the next workspace delta against the kept clone holds exactly the newer commit, tree and blob. application_epoch_host (kept_clone_is_brought_up_to_the_pull_request_before_it_lends): a kept clone lacking the head is refreshed once with the review's exact revision, then lends the head with no delta; the next question on that head fetches nothing."}}, {"criterion": "AC2", "outcome": "pass", "revision": "fd47341d6", "evidence": {"kind": "command", "detail": "gh_cli_git_smart_transport: refresh_managed_clone naming the reviewer-linked clone is refused invalid_request and its files (refs, objects, config) are byte-identical before and after; a folder in agent-clones-v1 that is not a kept clone is refused. application_epoch_host: a non-Managed linked clone lacking the head gets a private delta and no refresh request."}}, {"criterion": "AC3", "outcome": "pass", "revision": "fd47341d6", "evidence": {"kind": "command", "detail": "gh_cli_git_smart_transport: a refresh with the clone failing, and one already stopped (cancelled), leave the kept clone byte-identical, and the workspace delta still succeeds afterwards. application_epoch_host: refresh failures network, cancelled and unavailable each leave the question to succeed through the delta (HeadFromClone true then false), with an action=clone_refresh failure=refresh_failed or cancelled diagnostics line."}}, {"criterion": "AC4", "outcome": "pass", "revision": "fd47341d6", "evidence": {"kind": "observation", "detail": "The refresh runs in epoch_application_host::materialize_agent_workspace before the workspace exists, on the question's stop, through the review's gh lease transport (refresh_managed_clone in gh_cli_git_smart_transport.cpp), whose gh launch uses the pinned private configuration environment built from nothing and the macOS write confinement to its temporary root; the later pack copy and update-ref are this process and credential-free local Git. Nothing in the agent supervisor or its environment is involved; no widget performs it."}}, {"criterion": "AC5", "outcome": "pass", "revision": "fd47341d6", "evidence": {"kind": "command", "detail": "RFC-BPR-0092 Amendment BPR-1884 records fetch on miss into the kept clone (head and base only, never the default branch, owner 2026-11-06). python3 scripts/verify_documentation.py --root . printed DOCUMENTATION_OK (PYTHONPATH at the main checkout's .agents/skills/implement-rfc/scripts); coding-format-check passed."}}]
---

# BPR-1884: Keep the BetterPullReview-managed clone current, so agent deltas stay small

## Problem

BPR-1879 creates a clone owned by BetterPullReview under
`<profile>/agent-clones-v1/HOST-REPOID`. After that, nothing fetches into it.
Each agent question builds its private workspace with BPR-1873's delta fetch,
using the kept clone as its alternate, so the delta is everything the clone
lacks. As the clone ages that delta grows. On an active repository it
eventually exceeds the 32 MiB per-file bound, and the question fails as
`too_large`. The owner wants the app to just work.

## Change

- Fetch into the **managed** clone only, never into a reviewer-linked clone,
  which stays untouched (BPR-1755 Q2, RFC-BPR-0092 Q8). Use the transport's
  existing gh credential path with the same pinned private configuration.
  Fetch only what the pull request needs: its head OID and its base OID,
  each when the clone lacks it. Never fetch the default branch: a head fetch
  already brings in every ancestor the clone lacks (owner, 2026-11-06,
  superseding "fetch the default branch, plus the PR head").
- Choose when to fetch, and record the choice in the RFC amendment. Options:
  before an agent question when the clone is older than some threshold; in
  the background after a question; or on its own idle schedule while a review
  of that repository is open. The fetch must never block a question longer
  than today's delta fetch would. It must be cancellable, and must run on the
  serialized host lane, never in a widget.
- The fetch into the clone runs under its own runner without the 32 MiB
  per-file bound, like the initial clone (BPR-1879 v002): it is the clone's
  own maintenance, not a per-question download. A workspace delta still keeps
  its bounds.
- A fetch that fails leaves the clone usable. The question proceeds with the
  delta path, and the failure is recorded in diagnostics.
- Storage shows the clone's last-updated time. If it is cheap, also run
  `git maintenance` / `gc --auto` on the managed clone so its size stays
  bounded.

## Acceptance

- AC1. After an agent question on a repository with a managed clone, the
  clone is updated per the chosen policy, and the next question's workspace
  delta covers only commits newer than that update. Test this on a fixture
  where the upstream advances between questions.
- AC2. A reviewer-linked clone's refs, objects and config are byte-identical
  before and after (inventory check).
- AC3. A failed or cancelled maintenance fetch leaves the clone valid, and
  the question still succeeds through the delta path.
- AC4. No maintenance fetch runs inside the agent sandbox or with the agent's
  environment.
- AC5. The RFC-BPR-0092 amendment records the policy, and
  `python3 scripts/verify_documentation.py --root .` passes.

## Outcome

Built as **fetch on miss into the kept clone, before the question** (RFC-BPR-0092
Amendment BPR-1884). When the linked clone is the kept one and lacks the head,
`refresh_managed_clone` fetches the pull request's head and base, each only
when missing and never the default branch, as `delta`-form clones that borrow
the kept clone, on the kept clone's unbounded runner. The received
self-contained packs are copied in whole (index last), and
`refs/bpr/pull/N/head` and `base` name them. Then the head is lent from the
clone. A failure or cancel falls back to the delta and logs `clone_refresh`.
The fake `gh` runs real Git; no live BPR-Test run was made.

### Left

- No visible change: Storage does not show a last-updated time.
- No `git maintenance` / `gc --auto`. It can repack a large clone for
  minutes inside a question. Each refresh adds up to two packs, and they
  accumulate. Both are left to a later card.
