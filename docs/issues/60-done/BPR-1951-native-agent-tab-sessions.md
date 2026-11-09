---
schema: "implement-rfc.card.v1"
id: "BPR-1951"
title: "Continue native sessions in the Agent tab"
status: "Done"
rfcs: ["RFC-BPR-0096"]
metadata: {"mandate_revision":"native-agent-sessions-r1","reservation":"docs/issues/_reservations/BPR-1951--BPR-1956--ed8ca44cbe3b.json","milestone_plan":{"id":"native-agent-sessions-v1","path":"docs/rfcs/plans/native-agent-sessions-v1/milestone-plan.json"}}
depends_on: []
acceptance: [{"criterion":"AC01-Agent-tab-native","outcome":"pass","revision":"26fdce665d7ff09f9ecd52e0df6765ce569b21ff","evidence":{"kind":"observation","detail":"Visible setup, composer and Ask production course24: both Claude stream-json and Codex App Server preserve supervised PID/handle across first/followup, recovering prior tool observation absent first prose. Real installed Codex0.164.0 live and exact-ID new-process resume passed guarded course14; installed Claude sign-in remains later BPR1955.","ledger":"docs/verification/RFC-BPR-0096.md","external_evidence":"/Users/jaeger/.codex/tmp/native-agent-sessions-execution/"}},{"criterion":"AC02-AC03-Agent-tab-ownership","outcome":"pass","revision":"26fdce665d7ff09f9ecd52e0df6765ce569b21ff","evidence":{"kind":"observation","detail":"SQLite functional14 preserves incarnation/scope, compare-before-write revision and opaque handle across restart; public24 application reopen sends with zero workspace rematerialization; retained Git18 preserves edits/untracked files and local base/head objects with lending clone moved away.","ledger":"docs/verification/RFC-BPR-0096.md","external_evidence":"/Users/jaeger/.codex/tmp/native-agent-sessions-execution/"}},{"criterion":"AC05-AC07-Agent-tab-fences","outcome":"pass","revision":"26fdce665d7ff09f9ecd52e0df6765ce569b21ff","evidence":{"kind":"observation","detail":"Invocation/protocol functional14 preserves existing cancellation and stale binding decisions; native storage tests preserve stale generations, Pending refuses retry, CAS excludes concurrent writers, missing retained workspace/required base refuses without cleanup. Completed native turns leave stdin/process open and late correlated RPC replies are refused.","ledger":"docs/verification/RFC-BPR-0096.md","external_evidence":"/Users/jaeger/.codex/tmp/native-agent-sessions-execution/"}},{"criterion":"AC11-native-admission-and-authority","outcome":"pass","revision":"26fdce665d7ff09f9ecd52e0df6765ce569b21ff","evidence":{"kind":"observation","detail":"Native admission checks combined stored native/legacy count, refuses full capacity preserving native ownership; retained workspaces are bounded and startup-protected; authority/configuration rechecked. Broader legacy writer quota/projection and installed resource/permission matrix remain BPR1954/1955, explicitly unaccepted full-RFC scope.","ledger":"docs/verification/RFC-BPR-0096.md","external_evidence":"/Users/jaeger/.codex/tmp/native-agent-sessions-execution/"}},{"criterion":"focused-integration-and-upkeep","outcome":"pass","revision":"26fdce665d7ff09f9ecd52e0df6765ce569b21ff","evidence":{"kind":"observation","detail":"Functional14 six courses, integration18 architecture/isolation/composition cuts three courses, direct Qt picker24, retained Git18, public24, clang-format22, scoped clang-tidy final18/Qt22/panel24, documentation gate and67 documentation tests passed. Frontend-design loaded; offscreen conversation renders inspected; no desktop capture.","ledger":"docs/verification/RFC-BPR-0096.md","external_evidence":"/Users/jaeger/.codex/tmp/native-agent-sessions-execution/"}}]
---

# Continue native sessions in the Agent tab

## Scope and acceptance

Implement the [RFC's](../../rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md)
native transports, scoped identities, session-owned workspaces, durable
resume, stale-revision choices and per-generation cancellation/completion
boundaries. Separate durable conversation identity from window activation,
retain ownership through startup recovery, handle uncertain sends without
retransmission and refuse admission when storage cannot be reclaimed safely.
Keep ordinary Agent-tab permissions and own-sign-in behavior.
Qualify both supported native backend families rather than introducing a
prose-only substitute. This is the `native-agent-tab` milestone in the
[plan](../../rfcs/plans/native-agent-sessions-v1/milestone-plan.json).

Cover the Agent-tab portions of AC01–AC03 and AC05, the native-session
portion of AC07, and the resource/authority requirements of AC11. Evidence
for cross-surface controls, tour context and installed-agent qualification is
completed by the dependent proposed cards; partial evidence does not accept
the full RFC.

## Verification and execution record

Mandate `native-agent-sessions-r1`, implementer `/root/native_sessions_m1`.
Import `819ff2708`, claim `f8eaca5be`. The product commit is recorded in the
[verification ledger](../../verification/RFC-BPR-0096.md) and acceptance records.
Native protocol/host/SQLite/setup/canary checks passed; the final production
Agent-tab course completed setup, sent through the visible composer/Ask, showed
current native first/follow-up answers and reopened the app with retained workspaces.
The picker refresh regression, retained Git object/edit/restart/refusal course,
architecture/composition/isolation checks, scoped clang-tidy 22, changed-file
formatting and documentation gates passed. Installed Codex 0.164.0 on macOS arm64
passed live same-process memory and exact-ID reconnect with a new process.
Claude fixture-native continuity passed; installed Claude 2.1.310 is signed out,
so its installed qualification remains on BPR-1955. No full RFC/platform or
Release cost qualification is inferred.

The external durable continuation is
`/Users/jaeger/.codex/tmp/native-agent-sessions-execution/m1-handoff.md`; the
bound plan checkpoint is `m1-checkpoint.json` there. No attended candidate exists.
M2 owns tour integration and New session/history controls. BPR-1954/1955 retain
legacy migration, bidirectional shared quota/projection failure handling and
full installed/resource/permission qualifications before AC03/AC11 acceptance.
