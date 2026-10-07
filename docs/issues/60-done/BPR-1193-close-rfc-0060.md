---
schema: "implement-rfc.card.v1"
id: "BPR-1193"
title: "Verify and close RFC-BPR-0060"
status: "Done"
rfcs: ["RFC-BPR-0060"]
depends_on: ["BPR-1192"]
closeout_for: ["RFC-BPR-0060"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "closeout", "acceptance": ["RC60-01", "RC60-02", "RC60-03", "RC60-04", "RC60-05", "RC60-06", "RC60-07", "RC60-08", "RC60-09", "RC60-10"], "pack_alias": "PACK-CYCLE-51", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC60-01","outcome":"pass","revision":"d0967c80c","evidence":{"kind":"command","detail":"Pass: the agent-free and automation-off images and packages omit the agent launch adapter, protocol decoder, hosts, Send to Agent sender and MCP helper by file list, symbol audit and package record, and start with no child, socket, agent canary or vault selection; the supervisor stays by recorded decision. macOS packages (BPR-1179 c3c7d6b3c, BPR-1185 2f2eef6f5); Linux off graph 6/6 at 1d8b92642; Windows off graph 6/6 at 795d387cf and agent-free MSI RELEASE_PACKAGE_OK (d31ebf1f5, bd2f41995). Fresh: composed_application_hosts in both graphs, each image failing the other family's audit (9 anchors). Limit: the Linux agent-free DEB on the Ubuntu 24.04 baseline was not built, shared with BPR-0961. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1193 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC60 selection 27/27 and build/rc60-off (agents and automation OFF) 13/13 at 122e5d6b7's product tree."}},{"criterion":"RC60-02","outcome":"pass","revision":"d0967c80c","evidence":{"kind":"observation","detail":"Pass: the complete two-round course with integrations absent at composition level (BPR-1179), natively on cocoa and in the installed agent-free package (BPR-1182, BPR-1185), with the owner's installed GUI round trip accepted (session cbd48b91, event 947); Linux agent-free graph 5/5 at bf8da83c7; Windows native two-round course at bd2f41995. Fresh: automation-off two-round, publish and desktop courses in build/rc60-off. Limit: Linux ran offscreen only; the xcb course on the shared WSLg desktop was not run. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1193 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC60 selection 27/27 and build/rc60-off (agents and automation OFF) 13/13 at 122e5d6b7's product tree."}},{"criterion":"RC60-03","outcome":"pass","revision":"d0967c80c","evidence":{"kind":"command","detail":"Pass: local automation is its own switch and package variant; with it off no owner command endpoint is bound, the manual loop is whole and no cycle is lost; a full build's cycle is reopened by an agent-free automation-off build (BPR-1179, BPR-1185 installed swaps); Linux 1d8b92642 and Windows 795d387cf off graphs; Windows MSI full to agent-free and back keeps the cycle (bd2f41995). Fresh: the full two-round course with --reopen-application from build/rc60-off, natively on cocoa, reads back F1, F2, 10 notes and 1 response. Limit: no automation-off package built on Windows or Linux; Linux variant replacement unqualified. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1193 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC60 selection 27/27 and build/rc60-off (agents and automation OFF) 13/13 at 122e5d6b7's product tree."}},{"criterion":"RC60-04","outcome":"pass","revision":"d0967c80c","evidence":{"kind":"command","detail":"Pass: one stored policy decides for the terminal, file, bundle, granted harness read and the window's routes, with and without the desktop; provider variables and renamed bpr get the same refusals; the agent adapter channel decides first (BPR-1147, BPR-1180, BPR-1181). Linux f74c96080, Windows 72ca49f18 (uncontained). Fresh: security qualification and core policy, disclosure and share rows. Limit, as documented: no surface classifies a local folder, so every local cycle decides as unclassified. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1193 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC60 selection 27/27 and build/rc60-off (agents and automation OFF) 13/13 at 122e5d6b7's product tree."}},{"criterion":"RC60-05","outcome":"pass","revision":"d0967c80c","evidence":{"kind":"command","detail":"Pass: a hostile checkout runs nothing and is unchanged; names a round cannot carry are refused before freezing; forged responses (verified, approval, grant, oversized, link, other round) are refused with the store unchanged; claims stay claims (BPR-1181, BPR-1183). Linux f74c96080 and 018cb3c89; Windows 72ca49f18 and the whole recovery corpus at 1992ee1eb. Fresh: security qualification and recovery corpus. Limits: bundle import has no shipped entry point (its hostile corpus is the core's, host's and fuzz target's); Git canary liveness is shown on POSIX. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1193 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC60 selection 27/27 and build/rc60-off (agents and automation OFF) 13/13 at 122e5d6b7's product tree."}},{"criterion":"RC60-06","outcome":"pass","revision":"d0967c80c","evidence":{"kind":"command","detail":"Pass: owner-private data root and stores, owner socket and frame bounds, dropped wrong greetings and silent peers, impostor owners refused, credential and content canaries absent from files, logs and refusals, no encryption or sandbox claimed; kill points, owner killed mid-write, full disk and damaged stores recover (BPR-1181, BPR-1183). Linux all parts; Windows uncontained, skipping POSIX modes and raw socket attacks (covered by the stores' contracts and sqlite_tour_owner_channel) and full disk. Fresh: both courses. Limit: refusal of another OS user is read from the code, not observed. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1193 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC60 selection 27/27 and build/rc60-off (agents and automation OFF) 13/13 at 122e5d6b7's product tree."}},{"criterion":"RC60-07","outcome":"pass","revision":"d0967c80c","evidence":{"kind":"command","detail":"Pass: Send hands one selected profile exactly the round's readable document, the bytes Copy, Save and bpr cycle export give, and its reply goes through the same response model, checked against the sent round (BPR-1180). Linux a1f7a9fa7 13/13, Windows f198d4e09 13/13. Fresh: send course, host, launch and core rows. Limits: a stand-in harness, no real harness or model; Codex App Server and JSONL by launch framing, not a process. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1193 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC60 selection 27/27 and build/rc60-off (agents and automation OFF) 13/13 at 122e5d6b7's product tree."}},{"criterion":"RC60-08","outcome":"pass","revision":"d0967c80c","evidence":{"kind":"command","detail":"Pass: the existing profile under its existing host policy (a corporate account's host refuses Codex by name with nothing frozen or started), the reviewer's explicit session choice (new said as new, continuation only by a harness that can and by a reported name), only the profile's allowed environment names and no credential (BPR-1180). Linux a1f7a9fa7, Windows f198d4e09. Fresh: the same rows. Limit: stand-in harness. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1193 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC60 selection 27/27 and build/rc60-off (agents and automation OFF) 13/13 at 122e5d6b7's product tree."}},{"criterion":"RC60-09","outcome":"pass","revision":"d0967c80c","evidence":{"kind":"command","detail":"Pass: late output recorded against its round; a reply naming another round refused; stopped, timed-out and cancelled sends stay uncertain and a missing harness fails, with no resend, fallback, capture or verify; an agent-free build reopens a sent cycle and continues by hand (BPR-1180). Linux a1f7a9fa7 including removal across builds; Windows f198d4e09 and the agent-free tree 5/5. Fresh: removal across builds (full --profile-root, rc60-off --reopen) passes, and --reopen on an empty profile fails as it must. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1193 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC60 selection 27/27 and build/rc60-off (agents and automation OFF) 13/13 at 122e5d6b7's product tree."}},{"criterion":"RC60-10","outcome":"pass","revision":"d0967c80c","evidence":{"kind":"observation","detail":"Pass with recorded limits: each result names its producer (native cocoa processes, tests-disabled macOS packages started from outside, Windows MSIs installed natively over a full one and back, Linux offscreen in a tests-enabled graph); nothing cross-compiled or claimed native when it was not (BPR-1181, BPR-1182, BPR-1185, BPR-1179's Windows notes). Limits: packages are unsigned local candidates, trust not-assessed, not release qualification (RFC-BPR-0038's); the Linux agent-free DEB on Ubuntu 24.04 not built, shared with BPR-0961; no Linux installed-package or xcb run; second OS user not observed. Judged in docs/verification/RFC-BPR-0053-0061.md (BPR-1193 sections); closed under the owner's 2026-10-06 dependency amendment (\"Amend (Recommended)\"); fresh macOS build/dev (BPR_BUILD_TOUR_CLI=ON) RC60 selection 27/27 and build/rc60-off (agents and automation OFF) 13/13 at 122e5d6b7's product tree."}}]
---

Pack alias: **PACK-CYCLE-51** (live `BPR-1193`, allocated at import).

# BPR-1193: Verify and close RFC-BPR-0060

## Outcome and bounded work

Reconcile every criterion of RFC-BPR-0060 against its producing revision and direct evidence. Verify canonical predecessor closure and the imported ownership/remap. Leave unmet native/live/user gates open; do not relax criteria, recycle IDs or alter historical predecessor acceptance to close this RFC.

## Acceptance and direct evidence

Run the live documentation/board gates and record criterion/check/result/revision/environment. Close only this RFC through closeout_for; a complete pack validator or another RFC result is not substitute evidence.

Criteria: RC60-01, RC60-02, RC60-03, RC60-04, RC60-05, RC60-06, RC60-07, RC60-08, RC60-09, RC60-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

RFC base: `8466534f96f254d88fed412a587188b7b45f2580`.

All owned acceptance criteria remain **Not run** at intake.
## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| **RC60-01:** An agent-free build/package genuinely omits agent-specific libraries/resources/helpers and starts without agent/provider discovery or credentials. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC60-02:** The complete two-round source/suggestion/Copy-Save/capture/verification/publication-preparation course works with integrations absent. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC60-03:** Local automation is separately removable/disableable and does not remove core GUI/manual iteration or destroy stored cycles. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC60-04:** GUI/CLI/file/clipboard/adapter disclosure uses consistent scope/enterprise policy and never treats a skill/provider claim as permission. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC60-05:** Untrusted source/Markdown/imports cannot execute code, mint authority, alter original rounds or mark feedback verified. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC60-06:** Private data, pins, IPC bounds/identity and diagnostic redaction follow documented native boundaries without false same-user sandbox/encryption claims. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC60-07:** Optional Send uses the exact frozen payload, one selected assignment and the same response model as external/manual channels. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC60-08:** Existing provider/harness/account/host restrictions, explicit session selection and separate credentials remain in force. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC60-09:** Late, failed, uncertain or removed-adapter deliveries preserve the cycle and never auto-resend, auto-fallback, auto-capture or auto-verify. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC60-10:** Native installed-package, startup effects, IPC, attack corpus and removal evidence distinguish real qualification from fixtures/cross-compilation. | Focused production-path checks and applicable native or service course at closeout | Not run | None |

## Owner answer (2026-10-06): amend

RFC-BPR-0060's closure dependencies included RFC-BPR-0038, which is Proposed:
its closeout [[BPR-0973]] waits on [[BPR-0958]] to [[BPR-0962]], each Blocked
on an environment or input the owner has to supply. The coordinator put the
question to the owner in chat: "RFC-0060 … depends on RFC-0038 … Amend its
dependency and close on its own evidence?" The owner answered, verbatim:

> Amend (Recommended)

The answer came in chat. No attended candidate was prepared for it, and
session `cbd48b91000b00f1e72e1ad62c7c384c` was not used.

The amendment is recorded in RFC-BPR-0060's
[owner amendment](../../rfcs/RFC-BPR-0060-removable-agent-adapters-and-capability-isolation.md#owner-amendment-2026-10-06-dependencies-are-what-this-rfc-uses)
section and in the [RFC index](../../rfcs/README.md): RFC-BPR-0060 depends on
the RFC-BPR-0014 packaging drivers it uses, all of them landed (the package
graph [[BPR-0956]], the package audit [[BPR-0957]] and the macOS release path
[[BPR-0908]]), not on the closure of RFC-BPR-0038. Its `depends_on` is now
RFC-BPR-0053, 0056, 0057, 0058, 0011 and 0014, all Auto-Verified.
RFC-BPR-0038 is not closed by the amendment.

## Closeout result (2026-10-06)

Promoted at `74c50c04c` on the owner's amendment and claimed at `d0967c80c`.
RFC-BPR-0060's dependencies are RFC-BPR-0053, 0056, 0057, 0058, 0011 and
0014, all Auto-Verified; the card dependency [[BPR-1192]] is Done.

All ten criteria pass, each judged in the
[ledger](../../verification/RFC-BPR-0053-0061.md#bpr-1193-rfc-bpr-0060-closed-2026-10-06)
on the producing cards' evidence (BPR-1147, BPR-1179 to BPR-1183 and
BPR-1185) and their Linux and Windows station runs. The "Not run" cells in the
checklist above are from intake; the ledger section is the reconciliation.
Recorded limits, none narrowing a criterion: the Ubuntu 24.04 agent-free DEB
was not built, because no station runs the declared Linux baseline, which is
the same gap that blocks [[BPR-0961]] (RC60-01, RC60-10); Linux ran offscreen,
with no `xcb` course and no installed package (RC60-02, RC60-10); no
automation-off package was built on Windows or Linux (RC60-03); every local
cycle decides as unclassified (RC60-04); bundle import has no shipped entry
point (RC60-05); refusal of another OS user is read from the code
(RC60-06); Send ran against a stand-in harness, not a real one (RC60-07 to
RC60-09); the packages are unsigned local candidates, and release
qualification is RFC-BPR-0038's (RC60-10).

Fresh at `d0967c80c`, whose product tree is `122e5d6b7`'s, macOS 26.6.2
arm64, Apple clang 21, Qt 6.11.2, Debug, under the network-isolated runner:
`build/dev` (`-DBPR_BUILD_TOUR_CLI=ON`, both families) passes the 27-row RC60
selection 27 of 27, and `build/rc60-off` (agents and automation OFF) passes
its 13 rows 13 of 13. Across the two builds, the full two-round course with
the automation-off reviewer reopening it passes natively on cocoa, and the
Send course's removal case passes. Negative controls: each image fails the
other family's composition audit on nine anchors, and the agent-free reopen
of an empty profile fails. DOCUMENTATION_OK.

No row asks for a new owner judgment: RC60-02's installed loop was the
owner's, accepted on BPR-1182. The dependency answer came in chat, and
session `cbd48b91…` was not used.

Owed, not gating: Linux and Windows runs of the off graph and the Send rows
at the current tree (commands in the ledger). The Ubuntu 24.04 DEB stays with
BPR-0961's environment.

RFC-BPR-0060 is Auto-Verified. RFC-BPR-0038 stays Proposed. Docs and board
only; no product file changed.
