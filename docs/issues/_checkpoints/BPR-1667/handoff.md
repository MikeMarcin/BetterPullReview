# Discuss blocker and New-card read milestone handoff

Assignment `/root/discuss_body_fix`; mandate `discuss-body-blocker-r2-auto-read`,
including the owner's Approved/Commented clarification and New-card activation
request. Plan `bpr-1667-discuss-body`, milestone `repair-discuss-body`:
BPR-1667 AC1–AC4, BPR-1668 AC1–AC3 and both original exits technically accepted.
Baseline `818de796b1f06b02cc5dda24e0ed86363c413154`; tested product candidate
`ffa429dd7af67de7ed2e5b0e5a2aedfeb8ba49f8`. Bug fix `140c298df`, read feature
`ffa429dd7`. Exclusive root
`/Users/jaeger/.codex/worktrees/discuss-comment-body-fix/BetterPullReview`,
branch `codex/discuss-comment-body-fix`.

## Result

The application now projects present raw Markdown, including explicitly empty
raw bodies; only absent raw observations use legacy display text. GraphQL had
stored plain bodyText in Body and original Markdown separately, while selected
file placement could recover raw reviewer prose in Review. The confirmed owner
summary case is distinct: empty Approved/Commented reviews now say no overall
review comment and state how many exact file comments are linked below. Actual
empty inline/PR comments retain their empty state. No body is borrowed.

Opening a New-badged Discuss card captures its exact identity/unread fact before
relayout and requests the existing local Mark read command. Pointer, Return,
Space and next-new use the same open path. The host owns persistence and reports
its durable result; unrelated selection, passive refresh and offscreen cards do
not advance read receipts. Draft, provider, credential, publication and read
identity policies retain their existing ownership. No dependency or schema change.

## Verification

macOS arm64, Qt6.12.0, offscreen, serial GUI checks; clang-format/tidy22.1.8.
Final7-row integration at ffa429dd7 passes37.94s: architecture, composed GitHub100,
cards, composed Markdown100, new raw/summary route100, real permalink placement,
and application_conversation_read_public_path (5.62s). Existing reader100/125/200
passes3/3 in4.27s; the new route additionally passes125/200 in1.63/2.09s through
the same configured network-deny wrapper. Source/copy equality, raw-only refresh,
present-empty versus absent fallback, summary versus inline identity, actual
empty bodies, durable badge timing, selection/refresh isolation, restart,
keyboard/pointer activation, explicit read/unread, edited-summary and existing
quoted-reply persistence/Send are asserted. Summary/body/full-application renders
were inspected; external manifest `discuss-render-manifest.json` binds selected
PNGs. Offscreen renders establish content/layout mechanics, not native skin.

Final exact-test negatives independently substitute only818de's projection and
only its open-card renderer: raw route1.04s and permalink3.41s fail (child1,
CTest8); old summary renderer1.10s fails the explicit empty-approval sentence.
The new auto-read application regression previously fails old choose after24.10s.
All original source bytes are restored; source hashes match tested-inputs.json,
and the3 affected tests pass again10.64s. Unchanged passing evidence is reused.

Strict format22 checks9 touched files. Strict tidy22 passes all8 touched TUs:
5 unchanged passing units from discuss-tidy-r2.log and3 final changed units in
discuss-tidy-final-changed.log. Initial const-reference-return and copied-local
diagnostics were corrected. Early pre-build test launch read a stale executable;
its failure is retained but unused. A fake-host summary-link immediate-pick
assertion lacked selected-source composition; exact link metadata plus existing
card-link dispatch remain checked. No production assertion was removed.
DOCUMENTATION_OK,67/67 documentation-verifier tests and git diff --check pass.

Logs/evidence are `/tmp/bpr-qt612-run/discuss-*`: final-integration.log,
reader-scales.log, route-scale-results.json, final-negative-results.json,
final-restored-check.log, summary-and-read-negative.log, format-final.log,
tidy-r2.log, tidy-final-changed.log, documentation-final.log and
 documentation-tests-final.log. Configure uses dev, offline existing canonical
CPM cache and `/tmp/bpr-qt612-run/qt/6.12.0/macos`. All builds use this own root.

## Upkeep and ownership

Implement-rfc and frontend-design were loaded first. MASK registered existing
pinned locks and preflight ready_pinned; configuration does not prove host load.
Canonical main counter/receipt-only commits a0f688b3f and1b4e1e531 were made in
authorized windows, then imported d62f0ee66/ea09a5ad7. Main stayed otherwise clean.
No integration into main, push, live GitHub call, owner-desktop/personal-app
operation, release/version edit or nested agent. Original adopted source.json
is immutable; authorized r2 requirements addition is recorded in amendment-r2.md.
The target-only checkpoint helper still sees an unrelated shared-action-buttons
plan missing-boundary refusal; explicit plan bind and canonical repository_state
are used, as with BPR-1665/1666. The checkpoint checker verifies final consistency.

All own configure/build/test/tidy/documentation commands have finished. The
negative-control wrapper has exited after restoration; no descendant, review
server, pending human/independent criterion or candidate protection remains.
No other source writer used this root. Retain the root/build and external evidence
for coordinator integration; writer ownership is released at final handoff.

Next: coordinator integrates the committed bounded milestone and hands the final
source freeze to the release owner for native qualification and v0.3.1 packaging
and publication. NativeWindows/Linux/package/trust acceptance belongs there;
none is claimed by this milestone. No further broad tests requested here.
