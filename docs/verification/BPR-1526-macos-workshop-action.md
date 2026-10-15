# BPR-1526: Workshop press during a first draft save

Mandate `authorized-release-continuation-native-gates-r2`; constituent of
`bpr-1525-windows-release-gates/native-gate-contracts`.

The original Mac gate at `8f0d25418cd4278065d5a043dabaa728feb202f1`
failed `application_agent_finding_workshop_press_contract`: process starts 0,
no invocation, not quiescent. The same public course reproduced twice in this
worktree (48.43 s and 48.32 s) while guidance siblings passed. A disposable
trace then demonstrated the exact cause: the action named runtime placeholder
`18446744073709551615` with a first save pending; by the recording callback the
same save had assigned durable draft `1`, so loading the placeholder failed.
The old-ID settlement also missed the newly identified editor, leaving “Asked
for: Clarify” on screen. Trace code was removed after diagnosis.

The application now holds that operation and exact placeholder revision/scope
until its own materialization acknowledgement. Only a successful accepted save
for the selected draft, same scope, anchor and reply target can supply the
durable identity. A refused, retired or empty save settles the pending press
without launching. The immutable private-history press queues on the existing
FIFO lane before editor acknowledgement can dispatch later edits. It uses the
same core admission and agent invocation path. No Qt surface, layout, action
wording, readiness delay, budget, invocation assertion or mutation authority
changes. The required frontend-design skill was loaded from the installed
Claude plugin before diagnosis; the repair is application lifecycle only.

The corrected public course passes in 4.75 s with all original one-process,
one-cleanup, quiescence, original draft/private-source and zero GitHub mutation
assertions. Default guidance (3.43 s), custom guidance (3.45 s), no-agent
(3.10 s) and workshop construction-cut (2.62 s) pass in the same focused run.
Format 22.1.8, whitespace and documentation checks pass. Final changed-source
tidy, broader relevant lifecycle checks and final-source native release gates
remain obligations of the combined milestone; no final release acceptance is
inferred here.


The regression now holds the existing private persistence lane before typing,
presses immediately and proves that placeholder, pending Clarify operation,
zero processes and disabled repeat control persist until release. The original
public contract then passes in 4.71 s. This barrier makes the race deterministic;
no timer, readiness sleep or weaker condition was introduced.

Changed-source strict tidy exposed nine inherited naming violations already in
`8f0d25418` in this same owned translation unit. Mechanical local/parameter
renames correct those diagnostics without behavior changes; strict tidy on this
TU and the deterministic public-path TU passes. The source review covers c3009b2b0 plus those renames. It is the only
changed production-app manifest path. The full application/manifest pins are
resealed to this reviewed source; the production mutation-branch digest remains
`e067befefeff198fdbd992259b2ee8ea105520d50ddf7125e3b091519cb8a5df`.
The rebuilt, resealed public round-trip course passes in 4.37 s; the held-save
regression repeats in 3.52 s. No visual surface changed for BPR-1526.
