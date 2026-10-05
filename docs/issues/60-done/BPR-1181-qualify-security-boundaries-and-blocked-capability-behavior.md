---
schema: "implement-rfc.card.v1"
id: "BPR-1181"
title: "Qualify security boundaries and blocked capability behavior"
status: "Done"
rfcs: ["RFC-BPR-0060"]
depends_on: ["BPR-1179", "BPR-1180", "BPR-1162"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "optional-adapters", "acceptance": ["RC60-04", "RC60-05", "RC60-06", "RC60-10"], "pack_alias": "PACK-CYCLE-39", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC60-04","outcome":"pass","revision":"3a21cafb6","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1181's share, macOS arm64 Debug tour-cli graph. application_review_cycle_security_qualification (shipped bpr and the composed profile desktop as real processes): one stored policy decides for the terminal, a file, a bundle, a granted harness read and the window's Copy, Save and Save as Bundle routes; a denial holds in each and a later denial closes an existing grant, with the desktop owning the profile and without it. BPR_CYCLE_ROLE other than consumer or reviewer is refused; provider variables and a copy of bpr named claude get the same refusals; another root reads nothing; an unknown profile is profile_unknown. No local source is classified (documented). The agent adapter channel is BPR-1180's evidence."}},{"criterion":"RC60-05","outcome":"pass","revision":"3a21cafb6","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1181's share, macOS arm64. A hostile Git checkout (fsmonitor, filter, textconv, external diff, pager and hook canaries) runs nothing and hashes the same, .git included, through open, note, compose, freeze and capture; Git then fires the canaries 12 times. Links are carried as their own text; outside paths name nothing; a control-character name is refused before freezing (unportable_path, the defect fixed in 278956f0a). Note and response text is shown as typed and opens no FIFO. Forged responses (verified, approved, grant, trust, duplicate keys, deep nesting, not UTF-8, over 16 MiB, a link, another round or cycle) are refused with the store unchanged; a well-formed one claiming certification is a marked claim that verifies nothing and leaves F1 byte for byte. Negative control: the assembler refusal removed fails the course and core_review_cycle_round."}},{"criterion":"RC60-06","outcome":"pass","revision":"3a21cafb6","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1181's share, macOS arm64. Data root, stores and owner socket are the user's and owner-private; a loosened root or store is refused. A same-user peer's genuine request after a wrong or foreign greeting, oversized, overlong, short, zero-length and garbage frames, and a silent peer (dropped at 5.0 s) are each dropped and the owner keeps serving; impostor owners of another root or profile receive no byte. No credential canary in any file, log or output; no content canary outside the store and exports; no refusal repeats response text. Same-user, encryption, clipboard, scrollback, harness and dump limits documented in the threat model. Negative controls: the owner's greeting check removed and terminal neutralization removed each fail the course."}},{"criterion":"RC60-10","outcome":"pass","revision":"3a21cafb6","evidence":{"kind":"freeform","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1181's share. Every result names its producer: native macOS real processes in a tests-enabled Debug graph, not the installed package (BPR-1182), not another platform, nothing cross-compiled; second-user refusal read from code, not observed; removal and omitted families are BPR-1179's and BPR-1180's unchanged audits. Linux and Windows owed to stations with commands in the ledger."}},{"criterion":"BPR-1181:boundary","outcome":"pass","revision":"3a21cafb6","evidence":{"kind":"command","detail":"Focused selection 20 of 20 (core round, review_cycle host rows, qt_review_cycle_view, cycle_cli_public_path, skill_bpr_cycle_package, handoff, two-round, parity, desktop, send, the new course, architecture_boundaries, round-trip composition contract with pins unchanged, test_network_isolation_manifest, coding_format_check); clang-format 22 and 23 agree; clang-tidy no finding on a changed line; DOCUMENTATION_OK. Implementer's critical audit recorded in the ledger; no independent reviewer ran."}}]
---

Pack alias: **PACK-CYCLE-39** (live `BPR-1181`, allocated at import).

# BPR-1181: Qualify security boundaries and blocked capability behavior

## Outcome and bounded work

Audit all channel/capability paths for consistent export policy, known-origin preservation, untrusted reports/code, IPC bounds/identity and log redaction. Document same-user/clipboard/external-harness residual risks and supported unsupported cases.

## Acceptance and direct evidence

Attack corpus, revoked grant/wrong owner, hidden history request, forged model/approval/verification, hostile Markdown/paths and content-free diagnostics. Critical review records code/version/environment, not just policy prose.

Criteria: RC60-04, RC60-05, RC60-06, RC60-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Result (2026-10-04)

Promoted at `6d9e71619`, claimed at `37fd91558`, landed as `278956f0a`
(fixes), `3a21cafb6` (course) and `20aaaf206` (threat model and CLI guide).
Owned paths, decisions, every command and its result are in the
[ledger](../../verification/RFC-BPR-0053-0061.md#bpr-1181-security-boundaries-attacked-through-real-processes-2026-10-04).

- `application_review_cycle_security_qualification` attacks the shipped
  `bpr` and the reviewer's composed desktop with real processes:
  - a hostile Git checkout whose canaries never fire, then are shown live;
  - hostile note text, including a FIFO that nothing opens;
  - forged responses;
  - one channel policy across the terminal, a file, a bundle, a harness's
    read and the window's routes, with and without the desktop;
  - a same-user peer that replays a genuine request after bad greetings and
    bad frames;
  - impostor owners;
  - credential and content canaries, kept out of every file, log and
    refusal.
- Two defects were fixed at their source. A note on a file whose name holds a
  control character froze a round its own reader refused, so the round could
  never be read again; such a round is now refused before freezing
  (`unportable_path`). A response file over its bound, or one that is a link,
  was reported as unreadable; it now says which.
- The threat model records what was shown and the limits it does not lift:
  same-user programs, an unobserved second OS user, no encryption, the
  clipboard, scrollback, harness transcripts, dumps, the reviewer's own Git,
  no observed classification, github.com-only links and no shipped bundle
  import.

macOS arm64 evidence: the course under the sandbox runner, a 20-row focused
selection, three product negative controls and two live detectors. Owed to
stations, not run here: the Linux and Windows runs, whose commands are in the
ledger. The Windows manifest will also name three earlier unpinned
`WINDOWS_UNCONTAINED` rows from BPR-1179 and BPR-1180.

**Note: Linux run at `f74c96080` (station `bpr-main-linux`, WSL2, GCC 15.2.0,
`build/bpr1181` with `BPR_BUILD_TOUR_CLI=ON`, offscreen; no remote
mutation).** The build succeeded with no warnings and 9 of 9 passed:
`application_review_cycle_security_qualification` (63.3 s; 64.1 s in a verbose
run), `core_review_cycle_round`, `cycle_cli_public_path`,
`review_cycle_workbench`, `review_cycle_response`, `qt_review_cycle_view_1_0`,
`application_profile_cycle_handoff_public_path`,
`test_network_isolation_manifest` and `architecture_boundaries`.

The POSIX course printed every marker:

- `SECURITY_HOSTILE_CHECKOUT_OK`, `SECURITY_FORGED_RESPONSES_OK` and
  `SECURITY_ONE_POLICY_OK`.
- `SECURITY_OWNER_CHANNEL_OK silent_peer_dropped_after=5.0s`, the raw Unix
  socket attacks.
- `SECURITY_WRONG_OWNER_OK`, and the diagnostic log with 2 records.
- `SECURITY_GIT_CANARIES_LIVE 12`: twelve live Git canaries (fsmonitor,
  pager, filters, textconv, external diff), and none fired.
- `REVIEW_CYCLE_SECURITY_QUALIFICATION_OK`.

`build/bpr1181` was removed afterwards. Done is not reopened.

### Windows at `72ca49f18` (bpr-main-win, 2026-10-04)

Windows 11 Home 10.0.26200, MSVC 14.51, Qt 6.11.2, `build/windows-bpr1181`
(`windows-dev`, `BPR_BUILD_TOUR_CLI=ON`) Debug. The build printed no
`warning C` or `warning LNK` line.

- **As registered, 8 of 9.** `application_review_cycle_security_qualification`
  failed contained in 0.29 s: its first `git init` (line 214 of the course)
  fails before any attack runs. That is Git's `getcwd` under `AC\Temp`, the
  [[BPR-1166]] shape. It is now `WINDOWS_UNCONTAINED` and pinned
  `dos-volume-name` with that line. The other eight pass:
  `core_review_cycle_round`, `review_cycle_response`, `cycle_cli_public_path`,
  `application_profile_cycle_handoff_public_path`, `qt_review_cycle_view_1_0`,
  `review_cycle_workbench`, `architecture_boundaries` and
  `test_network_isolation_manifest`.
- **Uncontained, the course passes** in 21.64 s with
  `REVIEW_CYCLE_SECURITY_QUALIFICATION_OK`. It names the four parts Windows
  skips, as the ledger expects:
  - POSIX modes, which the stores' own contracts cover on Windows;
  - raw owner-channel attacks, which `sqlite_tour_owner_channel` covers;
  - the impostor owner, because a Windows pipe owner can only be a real one;
  - Git canary liveness, which is shown on POSIX.

  `test_network_isolation_manifest` passes with the pin.
- **The three rows the ledger said the manifest would flag are already
  pinned:** `application_review_cycle_automation_off_two_round` (on
  [[BPR-1179]]) and `review_cycle_send` and
  `application_review_cycle_send_public_path` (on [[BPR-1180]]).
