---
schema: "implement-rfc.card.v1"
id: "BPR-1191"
title: "Verify and close RFC-BPR-0058"
status: "Done"
rfcs: ["RFC-BPR-0058"]
depends_on: ["BPR-1190"]
closeout_for: ["RFC-BPR-0058"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "closeout", "acceptance": ["RC58-01", "RC58-02", "RC58-03", "RC58-04", "RC58-05", "RC58-06", "RC58-07", "RC58-08", "RC58-09", "RC58-10"], "pack_alias": "PACK-CYCLE-49", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC58-01","outcome":"pass","revision":"0e8f83e44","evidence":{"kind":"command","detail":"Pass: cycle_cli_public_path runs every bpr cycle command as a real process with no HOME, account, provider, credential or desktop and an impossible QT_QPA_PLATFORM; the tests-disabled TourCLI install passes the same course and links only QtCore/QtNetwork; the terminal composes the desktop's runtime, store, workspace source and export destination. macOS, Linux (installed TourCLI at 322b6d67e), Windows. Producing: BPR-1167 2a79d08eb; installed packages BPR-1182, BPR-1185. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1191 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend and close (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC58 selection 28/28 at cd40f5f50's product tree."}},{"criterion":"RC58-02","outcome":"pass","revision":"0e8f83e44","evidence":{"kind":"command","detail":"Pass: prose by default, --json one bpr.cycle.cli-result.v1 line with nothing on stderr, human refusals on stderr with Next, typed codes and exits, noninteractive refusal; cycle_cli_documentation checks the pages against the built bpr. Limit: the pseudo-terminal half runs on POSIX only. Producing: BPR-1167 2a79d08eb, BPR-1185. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1191 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend and close (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC58 selection 28/28 at cd40f5f50's product tree."}},{"criterion":"RC58-03","outcome":"pass","revision":"0e8f83e44","evidence":{"kind":"command","detail":"Pass: every result names cycle, round, project and payload_sha256; a folder-resolved read is RC-1 while the window shows RC-2; an unbound folder is not_found; grants bind one round. Limit: two active cycles bound to one root is asserted by rule (Open Project never makes one). All three. Producing: BPR-1169 58e25ded7, BPR-1171 fd760e33f. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1191 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend and close (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC58 selection 28/28 at cd40f5f50's product tree."}},{"criterion":"RC58-04","outcome":"pass","revision":"0e8f83e44","evidence":{"kind":"command","detail":"Pass: reads return the frozen assignment and canonical items, round source only the round's own attachments; no store scraping or file/HTTP server; not_granted without a stored grant; another root reads nothing (BPR-1181). All three. Producing: BPR-1169 58e25ded7, BPR-1181. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1191 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend and close (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC58 selection 28/28 at cd40f5f50's product tree."}},{"criterion":"RC58-05","outcome":"pass","revision":"0e8f83e44","evidence":{"kind":"command","detail":"Pass with recorded limit: one owner (lease) with the desktop open, the same twelve answers run alone, stale endpoint replaced, other protocol versions refused before a byte, 5 MiB codec bound, Windows named pipe under MSVC, same-user peer attacks dropped (BPR-1181). Limit: refusal of another OS user is the kernel credential check read from code (getpeereid, SO_PEERCRED, pipe token), not observed, as the threat model states. All three (Windows uncontained, dos-volume-name). Producing: BPR-1168 7e5a093cc, BPR-1171 fd760e33f, BPR-1181. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1191 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend and close (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC58 selection 28/28 at cd40f5f50's product tree."}},{"criterion":"RC58-06","outcome":"pass","revision":"0e8f83e44","evidence":{"kind":"command","detail":"Pass: the window's unsaved note, focus and editor survive terminal writes; draft edits cross interfaces under the revision read and stale ones name the current revision; a typed goal is saved before a terminal freeze; IME composition makes review/freeze draft_busy, then the committed text freezes; a repeated operation ID applies once. Review Cycles have no undo; reversal under the revision read (BPR-1168 decision). Limit: offscreen, synthesized input-method events, no native Cocoa run. All three. Producing: BPR-1168 7e5a093cc, BPR-1171 fd760e33f. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1191 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend and close (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC58 selection 28/28 at cd40f5f50's product tree."}},{"criterion":"RC58-07","outcome":"pass","revision":"0e8f83e44","evidence":{"kind":"command","detail":"Pass: whole-item pages each naming the digest, together equal to the single read and the golden items; forged or foreign cursors refused; requires_partition with its measure, never cut. All three. Producing: BPR-1169 58e25ded7, BPR-1171 fd760e33f. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1191 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend and close (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC58 selection 28/28 at cd40f5f50's product tree."}},{"criterion":"RC58-08","outcome":"pass","revision":"0e8f83e44","evidence":{"kind":"observation","detail":"Pass: the package lint (no publisher, model launcher, installation or reviewer command taught); owner accepted distribution (session cbd48b91, event 861); AgentSkills b4a3357 and MASK selection dc503b791 with digest equal to the package; the observed Claude Code course started no agent or model and installed nothing. Producing: BPR-1170. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1191 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend and close (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC58 selection 28/28 at cd40f5f50's product tree."}},{"criterion":"RC58-09","outcome":"pass","revision":"0e8f83e44","evidence":{"kind":"command","detail":"Pass: a read-only grant cannot append; a response grant appends one local_interface response with claims kept as claims; verified refused; no verify or publish command; a declared consumer is policy_denied for every other command; forged responses refused (BPR-1181). Limit: BPR_CYCLE_ROLE is cooperative, not containment (documented). All three. Producing: BPR-1169 58e25ded7, BPR-1181. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1191 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend and close (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC58 selection 28/28 at cd40f5f50's product tree."}},{"criterion":"RC58-10","outcome":"pass","revision":"0e8f83e44","evidence":{"kind":"observation","detail":"Pass: a recipient with no bpr, skill or grant completes a round from the Markdown (BPR-1171); an observed Claude Code session followed the package, read the whole round before editing and appended one response (BPR-1170, a8fb079aa). Limit: one harness observed. Producing: BPR-1170, BPR-1171 fd760e33f. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1191 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend and close (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC58 selection 28/28 at cd40f5f50's product tree."}}]
---

Pack alias: **PACK-CYCLE-49** (live `BPR-1191`, allocated at import).

# BPR-1191: Verify and close RFC-BPR-0058

## Outcome and bounded work

Reconcile every criterion of RFC-BPR-0058 against its producing revision and direct evidence. Verify canonical predecessor closure and the imported ownership/remap. Leave unmet native/live/user gates open; do not relax criteria, recycle IDs or alter historical predecessor acceptance to close this RFC.

## Acceptance and direct evidence

Run the live documentation/board gates and record criterion/check/result/revision/environment. Close only this RFC through closeout_for; a complete pack validator or another RFC result is not substitute evidence.

Criteria: RC58-01, RC58-02, RC58-03, RC58-04, RC58-05, RC58-06, RC58-07, RC58-08, RC58-09, RC58-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

RFC base: `8466534f96f254d88fed412a587188b7b45f2580`.

All owned acceptance criteria remain **Not run** at intake.
## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| **RC58-01:** Agent-free headless cycle commands run without desktop/GUI/display/provider installation and share the desktop application services. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC58-02:** Normal terminal output is human-readable; explicit JSON, stderr separation, noninteractive refusal and typed errors meet the CLI contract. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC58-03:** Commands bind exact profile/workspace/cycle/round identities; ambiguous targets and active-window changes cannot redirect work. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC58-04:** External reads expose only admitted prepared payload/context and never require database scraping or a generic file/HTTP server. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC58-05:** Desktop-open and CLI-alone paths use one owner with qualified native IPC, bounded messages, peer checks and no competing writer. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC58-06:** Cross-interface edits/freeze/undo preserve dirty text/IME and revision conflicts without lost updates or duplicate application. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC58-07:** Consumers can obtain and verify every instruction page before implementing; counts/digests and limit outcomes are explicit. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC58-08:** The thin bpr-cycle skill uses the active harness, no second model/orchestrator, no direct provider publisher, and no silent skill installation. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC58-09:** Response-only automation appends reports but cannot verify, alter original instructions, export other history or publish. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC58-10:** Manual terminal/Markdown and observed supported harness paths remain useful; missing skill support/reachability does not block the cycle. | Focused production-path checks and applicable native or service course at closeout | Not run | None |

## Owner answer (2026-10-06): amend and close

RFC-BPR-0058's closure dependencies included RFC-BPR-0044, 0045 and 0046,
which are Proposed and close together on [[BPR-1071]]. The coordinator put
the question to the owner in chat: "RFC-0058 … formally depends on
RFC-0044/0045/0046 … amend its dependency and close on its own evidence?" The
owner answered, verbatim:

> Amend and close (Recommended)

The answer came in chat. No attended candidate was prepared for it, and
session `cbd48b91000b00f1e72e1ad62c7c384c` was not used.

The amendment is recorded in RFC-BPR-0058's
[owner amendment](../../rfcs/RFC-BPR-0058-headless-cycle-cli-and-portable-consumer-skill.md#owner-amendment-2026-10-06-dependencies-are-what-this-rfc-uses)
section and in the [RFC index](../../rfcs/README.md): RFC-BPR-0058 depends on
what it uses, all of it landed ([[BPR-1054]] and [[BPR-1057]] of
RFC-BPR-0044, [[BPR-1061]] of RFC-BPR-0045), not on the closure of
RFC-BPR-0044, 0045 and 0046. Its `depends_on` is now RFC-BPR-0053, 0056 and
0057, all Auto-Verified. Those three RFCs are not closed by the amendment.

## Closeout result (2026-10-06)

Promoted at `061f293a6` on the owner's amendment and claimed at `0e8f83e44`.
RFC-BPR-0058's dependencies are RFC-BPR-0053, 0056 and 0057, all
Auto-Verified; the card dependency [[BPR-1190]] is Done.

All ten criteria pass, each judged in the
[ledger](../../verification/RFC-BPR-0053-0061.md#bpr-1191-rfc-bpr-0058-closed-2026-10-06)
on the producing cards' evidence (BPR-1167 to BPR-1171, with BPR-1181,
BPR-1182 and BPR-1185) and their Linux and Windows station runs. Four rows keep
a recorded limit: RC58-05's refusal of another OS user is read from the code,
not observed; RC58-06's cross-interface IME ran offscreen, with no native Cocoa
run; RC58-09's consumer role is cooperative, not containment; RC58-10
observed one harness, Claude Code.

Fresh at `cd40f5f50`'s product tree, macOS 26.6.2 arm64, `build/dev` (Debug,
`-DBPR_BUILD_TOUR_CLI=ON`): the 28-row RC58 selection passes 28 of 28 under
the network-isolated runner. DOCUMENTATION_OK.

Owed, not gating: Linux and Windows runs of the owner-handoff and parity
courses at the current tree (commands in the ledger).

RFC-BPR-0058 is Auto-Verified. RFC-BPR-0044, 0045 and 0046 stay Proposed.
Docs and board only; no product file changed.


## Linux station run (bpr-main-linux, 2026-10-06)

At `707d2a04a`, WSL2, GCC 15.2.0, `build/dev-tour-cli` (`dev` preset,
Debug, `-DBPR_BUILD_TOUR_CLI=ON`), under the network-isolated runner: the
ledger's owner-handoff selection plus the parity, cycle CLI documentation and
skill package rows pass 13 of 13. `application_profile_cycle_handoff_public_path`
(5.8 s), `application_profile_tour_handoff_public_path`,
`application_review_cycle_parity_public_path` (36.4 s), `cycle_cli_public_path`,
`cycle_cli_documentation`, `skill_bpr_cycle_package`,
`sqlite_tour_owner_channel`, `core_review_cycle_command`,
`review_cycle_workbench`, `application_review_cycle_desktop_public_path`,
`application_review_round_trip_composition_contract`,
`architecture_boundaries` and `test_network_isolation_manifest`. This is the
first Linux run of the handoff course since the owner protocol moved to 5
([[BPR-1279]]). The Linux half of the owed runs is discharged; Windows is still owed.
