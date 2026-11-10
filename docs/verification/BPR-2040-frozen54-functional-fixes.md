# Frozen54 functional release fixes

[[BPR-2040]], [[BPR-2041]] and [[BPR-2042]] repair three observed Mac Release
failures from frozen source `899056e7de7187edc0ee206a7dc499a1f5487af4`.
The engineering candidate is `e4c0568d461b83d78acf47b14ee0325b6477f6e5`.
The product cutoff remains `5fd40876745bdd815b0c0ed16407826ca74753f1`;
no later foreign product is included in this release-fix slice.

## Tour retirement readiness

A Tour result is projected before its invocation host finishes cleanup. The
old composer then enabled New session and Send although the existing
`tour_session_idle()` admission predicate was false. BPR-2040 presents that
existing fact separately from Running. Final text, the clock and Stop keep
their existing meaning. An editor-, host- and generation-bound timer observes
retirement at most ten times a second, performs no provider effects, and stops
on idle, closing, editor destruction, host replacement or a newer run.

The coordinator approved two amendments within the same owner file: apply the
same admission guard to Resend and preserve its existing history-loading guard;
retain the existing queued whole-tour draft-feedback exception. The active plan
records these amendments. Frontend design instructions were read from
`/Users/jaeger/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`;
the layout and appearance are unchanged.

The deterministic regression late-holds the existing Tour effect queue after
final projection and before cleanup. The candidate withholds New session and
Send, releases the gate, then performs the original single follow-up and exact
backend-memory assertions. The Qt course exercises Stop, a newer Running state,
retained final text and composer input, Return, continuation, Update, Resend,
history loading, Delete and queued draft feedback. Lifecycle review binds the
observer to its exact editor and host, invalidates same-address replacement by
generation, and protects reentrant timer deletion with QPointer.

## Profile menu and custom-agent fixture

BPR-2041 updates the existing 28-entry View-menu oracle for the accepted Go to
line command. The 29-entry order, real window shortcut identity, unavailable
shell action and every previous shortcut/disabled-action assertion remain.
The first corrected attempt exposed later residual indices; that actual failed
run is retained, and commit `e4c0568d` corrects those remaining indices.

BPR-2042 follows a single old-source diagnostic that reproduced the empty-send
failure: Ask was disabled while both histories were loading, no refusal was
emitted and no process started. The existing custom-agent fixture now waits
for the chosen profile, actual legacy Loaded, combined loading false and actual
Ask enabled within its original 30-second agent budget. It then keeps the same
clear, one empty click, original event pump, empty-message/Not sent/zero-start
oracles. No production behavior, wording or budget changes.

## Exact engineering verification

A fresh normal Release graph rebuilt every implementation-header consumer with
four compile jobs: all four owning targets passed in 516.833 seconds. The
corrected menu target subsequently rebuilt in 8.428 seconds. LLVM 22 formatting,
secret scanning and the maintained source inventory passed. Architecture passed
in 19.781 seconds and was not replayed after the test-only index correction.

A fresh quiet-host guard preceded one serial six-row invocation. All passed:

| Original registered row | Seconds |
| --- | ---: |
| qt_profile_management_window | 1.094 |
| qt_tour_side_chat | 4.693 |
| application_composition_source_contract | 43.537 |
| application_native_agent_sessions | 37.280 |
| application_agent_chat_composition_contract | 3.534 |
| application_agent_chat_custom_agent_contract | 2.339 |

The source row ran all seven original source/compiler methods. The separate
original `application_composition_contract` passed its eighth method in 491.234
seconds, including a normal tests-disabled Release build, production composition,
fixture omission, binary guards and self-tests. It retained its 1,560-second
watchdog and four-job cap. The native-session row reached all six exact phase
markers and the standalone wrapper pass marker. All original assertions,
network denial, environment modifications, gestures and deadlines remained.

The old-UI-expression control kept the candidate layout, host observer, late
hold and regression. Its single original wrapper invocation failed the named
retirement assertion in 17.808 seconds with actual facts:
`held=1 quiescent=0 newSession=1 send=1 historyLoading=0 currentThread=1 nonblank=1 finalLive=0 startsUnchanged=1`.
Thus the old complete Send conjunction was true while admission remained busy.
No standalone wrapper pass was emitted; later wrapper phases were unreached.

That control has an explicit preparation deviation: formatting its external
failure-only diagnostic TU without the repository style changed whitespace and
six ordinary string-literal sites into adjacent literals, adding seven tokens.
Include/preprocessor order and concatenated literal contents match. Outside the
approved diagnostic block, all 28,921 significant tokens match after adjacent
literal concatenation. The coordinator accepted semantic regression evidence
with this limitation; this is not an exact minimal-source control or the original
normal binary. Raw diff and token/literal comparisons are retained. No extra
course was run. The exact positive source was restored and normally rebuilt in
13.208 seconds. All 120 protected release inputs remained identical throughout.

Scoped LLVM 22 tidy ran all eight changed translation units. Its actual exit was
1 for one unchanged pre-existing string-concatenation diagnostic in the native
fixture, baseline line 1770 to candidate line 1808, blame `7fbddb9180`. The line
hash is `c937c5283aee069627bb54cbe5b49959af03e92b889a0cd4c99b5dbbfcaec91b`.
No changed-line or other seven-TU finding occurred. No suppression, unrelated
cleanup or whole-TU tidy pass is claimed.

## Source binding and limits

The unchanged ordered 142-path manifest binds
`44ec961f0767c4cd2243bc9cd153d1030ad3262ccec50047674911d71344c926`.
Three pinned app paths changed; four app paths were independently reviewed,
including the fixture-access header in the complete 197-path inventory. The
lists, full-application pin and mutation pin are unchanged. Source-only negative,
maintained positive and the original seven adversarial/source methods are retained.

External evidence lives under
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/implementer54-pathological-release-triage-r1`.
It includes the original failures, first menu failure, exhausted first quiet
guard (zero courses), successful second quiet guard, source seals, original
commands/properties, native XML, control diff, restoration and protected inputs.
No credentials or authenticated session URLs are recorded.

These are scoped engineering results, not final release qualification. Original
optimized pathological/unavailable input-budget misses remain unresolved; the
single instrumented nonreproduction provides no Qt/host attribution or release
credit. Final common-cohort native qualification remains with the release
coordinator. Canonical main's later foreign BPR-2046 composition change requires
its own combined pin in a separate commit; that foreign change and pin must not
enter this release slice.
