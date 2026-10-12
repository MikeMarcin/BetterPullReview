---
schema: "implement-rfc.card.v1"
id: "BPR-1410"
title: "Verify and close RFC-BPR-0071"
status: "In Progress"
rfcs: ["RFC-BPR-0071"]
depends_on: ["BPR-1409"]
priority: "P1"
metadata: {"milestone_plan": {"id": "remember-cli-connections-v1", "path": "docs/rfcs/plans/remember-cli-connections-v1/milestone-plan.json"}, "mandate_revision": "remember-cli-r1"}
closeout_for: ["RFC-BPR-0071"]
---

# BPR-1410: Verify and close RFC-BPR-0071

RFC base: `46eb49b312b35ef37e69ca4ace7354c04295a6dd`.

## Acceptance

- Every RFC-BPR-0071 AC01–AC06 is demonstrated against the RFC baseline through composed macOS fixture courses, including fresh processes and last-window closure.
- Tokenless execution, publication consent, global CLI state, storage/refusal and profile maintenance canaries pass; all owning contracts describe implemented scope.
- Focused integration and documentation evidence cover the exact final candidate; required acceptance remains explicit.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** In a composed macOS course, choose a CLI account, accept read trust and connect, then quit and start a new process on the same profile. It accepts the same host/login/numeric account without discovery, account selection or another read-trust question. Repeat after explicitly closing the last window, with no retained review destination. | Pending composed qualification. |
| **AC02:** With two CLI accounts on one host, a native profile on that host and a second server, restart and exact-host PR navigation follow the saved choices despite changed discovery/configuration order and global CLI active account. Private content and connection identities remain correctly scoped; a sibling window shares the accepted connection. | Pending composed qualification. |
| **AC03:** A missing or mismatching saved account, unavailable tool, unsupported platform or failed validation cannot accept another authority or expose its private state. The failure is visible, the choice survives, and a subsequent successful retry restores that exact connection. | Pending composed qualification. |
| **AC04:** Sign out, Forget server and successful replacement have their defined restart results, including a previous device grant and late completions. Cancellation or unsuccessful replacement preserves the prior durable choice; another BPR profile is unaffected. | Pending composed qualification. |
| **AC05:** Real storage reopen, interrupted writes, refused saves and refused loads demonstrate the defined durability and session-only behavior. Fresh and upgraded profiles without a selection do not run CLI implicitly. Profile maintenance accounts for the new metadata. | Pending composed qualification. |

| **AC06:** Fixture canaries demonstrate that persistence and restored execution retain the existing tokenless boundary and global CLI state; restored read consent cannot bypass ordinary publication confirmation. | Pending composed qualification. |

## Continuation

Policy/storage BPR-1408 and shared runtime BPR-1409 are accepted. Final runtime source is `a09ff90c704f534c7e13c809d50b113042bd262a`; focused application-lifetime checks pass. The criteria above still require complete qualification. Durable checkpoint directory: [remember-cli-connections-v1](../_checkpoints/remember-cli-connections-v1/README.md).
