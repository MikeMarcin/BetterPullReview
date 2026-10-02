---
schema: "implement-rfc.card.v1"
id: "BPR-1169"
title: "Expose scoped prepared-round retrieval and response-only grants"
status: "Done"
rfcs: ["RFC-BPR-0058", "RFC-BPR-0060"]
depends_on: ["BPR-1147", "BPR-1167", "BPR-1168"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "external-consumers", "acceptance": ["RC58-03", "RC58-04", "RC58-07", "RC58-09"], "pack_alias": "PACK-CYCLE-27", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC58-03","outcome":"pass","revision":"58e25ded7","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64: application_profile_cycle_handoff_public_path runs a real desktop owner and the shipped bpr; with the window moved to a second active cycle (RC-2), round read --round F1 in RC-1's folder reads RC-1's F1 through the owner (protocol 4) and from a folder bound to no cycle is not_found: no window selection is consulted. cycle_cli_public_path: every result names cycle, round, logical project and payload_sha256; pages and cursors are bound to the digest; a grant for RC-1 F2 reads neither RC-1 F1 nor RC-2 F1; RC-2's folder finds RC-2. Ambiguity between two cycles bound to one folder is still asserted by rule (BPR-1167 limit)."}},{"criterion":"RC58-04","outcome":"pass","revision":"58e25ded7","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"cycle_cli_public_path: without a stored grant a named round is policy_denied (not_granted); granting is the reviewer's export through external_retrieval (channel_unconfigured for a plain folder until bpr cycle policy allows it); a read returns only the frozen assignment and canonical items, byte for byte as frozen, and round source only the round's own attachments (whole or range); another capture file, a path for an attachment, a later private note and the next draft's goal are unreachable; revocation or a later denial reads nothing. No store scraping or file/HTTP server: the command runs here or at the owner over the existing same-user channel. Tests-disabled TourCLI install passes the same course. Negative controls fail (reviewer-authority read, revocation ignored, stored policy ignored)."}},{"criterion":"RC58-07","outcome":"pass","revision":"58e25ded7","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"cycle_cli_public_path: a 31-item round read at 4 KiB pages takes >=3 pages, each naming the same payload_sha256 and its first item; together they hold every item once, in order, equal to the one-page read; a cursor of another round, a forged digest or a position past the end is invalid_input; --budget-bytes below the instruction bytes is requires_partition with the measure and no items. core_review_cycle_retrieval: whole items only, an item over the largest page is item_over_page (requires_partition), cursors bound to round/digest/page size, bounded attachment listings and chunks. Negative controls fail (page drops an item, budget unchecked, cursor unbound)."}},{"criterion":"RC58-09","outcome":"pass","revision":"58e25ded7","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"cycle_cli_public_path: a read-only grant cannot append; a response grant appends one structured response recorded as local_interface with claimed provider/model kept as claims and no note, wording or round changed; a 'verified' disposition is refused; another round's digest is conflict, an ungranted round policy_denied. No verify/publish command exists (unsupported_command); with BPR_CYCLE_ROLE=consumer show, list, compare, export, grant, revoke, policy, review, freeze, capture, note and response import are policy_denied and change nothing. review_cycle_workbench: the desktop's Use in external agent stores the same response-only grant for exactly the round it froze; routes follow the stored policy. The role is cooperative confinement, not same-user containment (documented). Negative control fails (no consumer role)."}},{"criterion":"BPR-1169:boundary","outcome":"pass","revision":"58e25ded7","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"82 of 82 rows in build/dev-tour-cli incl. cycle and tour CLI, both handoff courses, owner channel, workbench, desktop, storage, round-trip composition contract (unchanged, passes), architecture_boundaries and test_network_isolation_manifest; core-only 24 of 24 core_review_cycle rows; tests-disabled TourCLI install passes cycle_cli_tests; clang-format clean; clang-tidy clean on changed units except eight existing review_tour_codec.hpp findings; seven negative controls fail; DOCUMENTATION_OK. Linux and Windows runs owed (commands in the ledger), not gating."}}]
---

Pack alias: **PACK-CYCLE-27** (live `BPR-1169`, allocated at import).

# BPR-1169: Expose scoped prepared-round retrieval and response-only grants

## Outcome and bounded work

Implement exact cycle/round selection, capabilities, complete read/page/source and optional response append. CWD resolves only unambiguous allowed metadata. Scope reads to ready payload/context; imported IDs are not grants. Add consistent policy decisions to GUI/CLI paths.

## Acceptance and direct evidence

Two active cycles, focus change, revoked scope, hidden draft/context, every-page completion, oversize partition refusal, response grant attempting verify/publish and unknown provider identity. No direct database or generic filesystem endpoint.

Criteria: RC58-03, RC58-04, RC58-07, RC58-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Implemented — macOS, 2026-10-02

A harness that can run `bpr` reads one frozen round the reviewer granted, and
may add a response to it, through the same local owner as every other `bpr
cycle` command. `bpr cycle round grant RC-n --round Fn` (or the desktop's Use in
external agent, host side) stores a grant for exactly that round and digest:
reading it and, unless `--read-only`, appending a response; `round revoke`
withdraws it. Granting is decided as the reviewer's export of the round
through `external_retrieval`, so a folder of unknown origin needs `bpr cycle
policy --classification unclassified --channel external_retrieval --permission
allowed` first. That policy is now the profile's, stored with its cycles (schema
9), and every decision combines it with the host's: desktop routes, Copy, Save,
`export`, granting and reads alike.

`round read` returns the round's identities, assignment, counts and a page of
whole canonical items; `round page --cursor` continues, every page bound to the
payload digest; `--budget-bytes` and an item larger than any page are
`requires_partition`. `round source` lists and reads only the round's own
attachments; `review --context` selects them. `response append` adds a
structured response recorded as `local_interface`. Every consumer command acts
on the stored grant through `core::authorize` as a granted consumer, never on
the reviewer's authority, and `BPR_CYCLE_ROLE=consumer` holds a declared harness
to those commands. The owner protocol is now 4.

Owned paths, compatibility decisions, the 82-row pass, the tests-disabled
install run, seven negative controls, limits and station commands are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under "BPR-1169:
one frozen round for a harness, under a stored grant"; usage is in
[the developer page](../../development/cycle-cli.md).

RC58-03, RC58-04, RC58-07 and RC58-09 pass on that evidence. No criterion asks
for the owner's judgment.

## For BPR-1161

Its Use in external agent route now has something to reach: `bpr cycle round
read` exists, and the route's host side stores the grant that command needs.
The desktop still passes no external-interface capability, so the route stays
hidden, as BPR-1161's frozen candidate and pending questions assume. Enabling
it is a change to that card's share page, to be made after its verdicts: with
the shipped default policy it would be listed refused for every local folder
until the reviewer allows `external_retrieval`, which has no desktop control
yet. Send to Agent stays hidden (no adapter).

## Owed platform runs

Not gating. Linux and Windows configure `build/*-tour-cli` with
`-DBPR_BUILD_TOUR_CLI=ON`; the exact commands are in the ledger section.
`cycle_cli_public_path`, `application_profile_cycle_handoff_public_path` and
`sqlite_tour_owner_channel` are registered contained and unmeasured on
Windows: a refusal inside the container is pinned `WINDOWS_UNCONTAINED` with
its measured line, not guessed.

## Linux: bpr-main-linux, 2026-10-02

At `7d8181527` (includes `58e25ded7`), on WSL2 with GCC 15.2. No remote mutation.

- **`build/dev-tour-cli` (`BPR_BUILD_TOUR_CLI=ON`).** The configure and build
  exit 0 with 0 `warning:` lines. The requested `ctest -j4` selection passes
  **19 of 19** in 34.49 s, including:
  - `core_review_cycle_{retrieval,command,policy,share,response,round}`
  - `sqlite_review_cycle_storage` (schema 9) and `sqlite_tour_owner_channel`
    (owner protocol 4)
  - `review_cycle_{workbench,export,response}` (workbench 34.40 s)
  - `cycle_cli_public_path` (14.02 s) and
    `application_profile_cycle_handoff_public_path` (9.73 s)
  - `application_review_cycle_desktop_public_path`
  - `qt_review_cycle_view_1_0` and `_2_0`
  - the round-trip contract, `architecture_boundaries` and
    `test_network_isolation_manifest`
- **Installed TourCLI.** The `tour-cli` preset configure and build exit 0
  with 0 `warning:` lines, and `cmake --install … --component TourCLI` into a
  fresh `build/tour-cli-install` exits 0. `python3
  tests/scripts/cycle_cli_tests.py build/tour-cli-install/bin/bpr` passes:
  `cycle-cli-public-path:pass`, its pseudo-terminal part included.
