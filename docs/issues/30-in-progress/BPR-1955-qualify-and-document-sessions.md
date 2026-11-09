---
schema: "implement-rfc.card.v1"
id: "BPR-1955"
title: "Qualify native conversations and document their ownership"
status: "In Progress"
rfcs: ["RFC-BPR-0096"]
metadata: {"mandate_revision":"native-agent-sessions-r1","reservation":"docs/issues/_reservations/BPR-1951--BPR-1956--ed8ca44cbe3b.json","milestone_plan":{"id":"native-agent-sessions-v1","path":"docs/rfcs/plans/native-agent-sessions-v1/milestone-plan.json"}}
depends_on: ["BPR-1951", "BPR-1952", "BPR-1953", "BPR-1954"]
acceptance: [{"criterion":"AC10-Codex","outcome":"pass","revision":"f98e14cd604a251a32e8cfeb9c33a5b0614a489a","evidence":{"kind":"observation","detail":"Installed0.164 AppServer native/replay3, exact reconnect and explicit interruption; JSONL exact handle3; actual full BPR guide explanations and offscreen/public controls on macOS arm64.","references":["docs/verification/RFC-BPR-0096.md","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-final-resource-check.log","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-qualification-public-raster.log","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-usage-summary.json","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-claude-cause-driver.log"]}},{"criterion":"AC10-Claude","outcome":"pending","revision":"f98e14cd604a251a32e8cfeb9c33a5b0614a489a","evidence":{"kind":"observation","detail":"Fresh post-owner-handback Claude2.1.310 native first turn still fails authenticated HTTP429 in3.032s; own output identifies exhausted weekly allowance, reset Nov10 at00:00 America/Los_Angeles. No tool activity or usable answer; replay/interruption not retried after failure. Exact Release binary d09aba96… unchanged. Fresh wizard94eafb5d-7d90-4146-9de5-f8802d8bade4 awaits actual availability at consumedrevision1; acknowledgement is not product acceptance.","references":["docs/verification/RFC-BPR-0096.md","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-final-resource-check.log","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-qualification-public-raster.log","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-usage-summary.json","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-claude-cause-driver.log","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-resume-claude-native.log","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-resume-claude-native-guard.json","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-resume-installed-identity.json","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-resume-intake.json"]}},{"criterion":"AC11-available-scope","outcome":"pass","revision":"f98e14cd604a251a32e8cfeb9c33a5b0614a489a","evidence":{"kind":"observation","detail":"Actual Release AppServer native/replay metrics reported with cost/model/platform limits; functional idle64KiB,+1,cache4eviction,16GiB sparse pressure/shared500/refusal and retained ownership courses pass. No all-platform/automatic-reclamation/fixed-savings claim.","references":["docs/verification/RFC-BPR-0096.md","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-final-resource-check.log","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-qualification-public-raster.log","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-usage-summary.json","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-claude-cause-driver.log"]}},{"criterion":"AC12-documentation","outcome":"pass","revision":"f98e14cd604a251a32e8cfeb9c33a5b0614a489a","evidence":{"kind":"observation","detail":"Live operator/product/architecture and scoped RFC0011/0012/0092 amendment updated; immutable packs/Done verdicts preserved, documentation gate passes and67unchanged script tests remain passing.","references":["docs/verification/RFC-BPR-0096.md","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-final-resource-check.log","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-qualification-public-raster.log","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-usage-summary.json","/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-claude-cause-driver.log"]}}]
---

# Qualify native conversations and document their ownership

## Scope and acceptance

Run the [RFC's](../../rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md)
focused composition and installed-agent courses with exact backend version,
protocol and platform identity. Exercise crash/reconnect, per-turn and idle
bounds, storage pressure, retained-context privacy and permission revalidation.
Obtain rendered offscreen evidence for the
changed controls and warning reasons. Compare equivalent Release native and
replay conversations using actually available usage and reuse metrics, with
Debug timing nongating. Reconcile the
[proposed amendment](../../rfcs/assets/native-agent-sessions-v1/proposed-session-amendment.md) and operator disclosures
through authorized live documentation changes. This completes
`qualified-continuation` in the [plan](../../rfcs/plans/native-agent-sessions-v1/milestone-plan.json).

Cover AC10, AC11 and the substantive documentation portion of AC12; integrate
the distinguishing functional evidence from the preceding cards. Missing
installed/native/human observations remain explicit; no authoring validation
or old fixture pass is substituted.


## Owner acceptance amendment, 2026-11-08

The owner's actual instruction is: “the claude is out of usage, just keep going
without verifying it works”. Amendment `native-agent-sessions-claude-waiver-r1`
under mandate `native-agent-sessions-r1` waives the remaining installed Claude
verification for this pack's progression and acceptance. It does not turn the
recorded HTTP429 weekly-limit failures into a pass. Installed Claude native,
replay, exact-ID resume, interruption/Stop, explanation and equivalent usage
remain unverified; implementation, functional fixture, installed Codex and all
unrelated criteria are unchanged. The live RFC and plan record this scoped acceptance change. The historical
AC10-Claude pending receipt is retained externally in
`m4-resume-1955-acceptance.json`; completion applies the owner amendment, not a
passing Claude observation.

Availability wizard `94eafb5d-7d90-4146-9de5-f8802d8bade4` is superseded by
this amendment. Its host was stopped through the wizard helper at consumed
revision1; the pending human acknowledgement remains unanswered. State/history
are preserved externally at
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m4-claude-weekly-pnwqk34v/state`.
No retry, inference or availability claim accompanies this amendment.
