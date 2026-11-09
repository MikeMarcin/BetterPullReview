# Additional Go to line owner review

The owner requested an attended look after automated closeout. This is a new,
short session for the implemented dialog and navigation UX. BPR-1948 remains
Done and its existing AC01–AC04 acceptance is unchanged. No product, fixture or
feature test was edited during preparation. Interpret future verdicts in the
next engineering round; this preparation records no human acceptance.

## Candidate

- Frozen source: `2186ffdfbafd99a1cf29c20c6dddc1512f9194ac`.
- Frozen root: `/Users/jaeger/.codex/attended-builds/bpr1948-go-to-line-v002.z9849z6m`.
- Binary SHA-256: `f54aec27de6624f1d9bfa4ffa1e4b0a8be354a935ffc4af73f7dc02696d5659c`.
- Review revision: `2186ffdfb-v002-f54aec27de6624f1`.
- Committed synthetic fixture: `inline-drawer-v1`, opening
  `src/ui/conversation_cards_view.cpp` (Base1,464 / Head1,557 source lines).
- Launcher: `/opt/homebrew/bin/python3.14 /Users/jaeger/.codex/attended-builds/bpr1948-go-to-line-v002.z9849z6m/launch.py`.

`scripts/freeze_attended_build.py freeze` exported the exact commit and built
inside the root. Its own launcher `--check` passed:
`ATTENDED_FILE_SHOWN:src/ui/conversation_cards_view.cpp`, head
`89df5e592e5db97a526a05a445735733395e4832`. Source, build and served fixtures
remain under that root and are read-only. The pinned dependency source cache
is shared through a symlink; its targets were not made read-only or removed.
This is a tests-enabled Release development look, not a signed release claim.
The owner opens the native window themselves; preparation used only offscreen
launching and did not drive or capture the owner's desktop.

The first incomplete freeze was stopped by its task-owned Ninja PID after
checking that its automated-only `source-find-v1` fixture has no CLI selector.
That root was removed after its command tree stopped; no review candidate
was ever published for it. The successful v002 uses an existing supported CLI
fixture and adds no application selector or product change.

## Session and pending questions

Session: `f5fee13c7d9e648334040aebfb1e09d9`.
State: `/Users/jaeger/.codex/attended-review-sessions/bpr1948-go-to-line-w47r2fe5`.
Shared registry: `/Users/jaeger/.mask/attended-review-resources`.
Lifecycle: serving; both questions ready and protected as of consumed cursor7.
The [machine-readable handoff](review-session.json) records exact identities.

- `BPR-1948-go-to-line-dialog-v002`: keep the dialog layout, source-side choice
  and validation. Fingerprint
  `0c4083d897e5f99c9f593d6dc0f7959d869340871d0cf47cc6b21ce867109701`.
- `BPR-1948-go-to-line-navigation-v002`: keep source-line reveal and keyboard
  focus in split and unified views. Fingerprint
  `d8f710937b2fe662f88fd5b311a507d07fa45707b2236ee3da0a5fbaa6da1631`.

Each asks Accept or Reject independently. Both share the static frozen
installation under its exact binary hash and protect the relevant Qt source
paths. `init`, `prepare` and `start` completed. The private HTTPS status API was
successfully fetched and verified against this session and its two ready
questions. Capability URLs stay in the external `served.json`; none is tracked.

## Driver and command ownership

The coordinator `/root` owns user-facing delivery, intake and handback waiting.
Preparation leaf `/root/go_to_line` consumed only preparation/review-start
through cursor7, before sharing the URL. It started no decision waiter and
will not interpret subsequent human events.

The one handed-off foreground server is PID44087, exec session58123. Its owned
Tailscale child is PID44094, foreground session`ca5296055133470a`, port61213.
Both loopback listeners and the scoped private HTTPS route belong to this
helper. Keep it running; do not reset shared Serve settings. Python3.14.7 and
Tailscale1.102.3 match the versions in the companion's recorded awake-host
qualification; the coordinator owns the actual outer handback wait.

Use these exact common arguments for every command:

```sh
/opt/homebrew/bin/python3.14 /Users/jaeger/.mask/store/v1/9781ac9f67acb2b9aa9dcf53895fb73be527a05c9f1bcffec62168b19dec2c32/payload/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/.codex/worktrees/1567/BetterPullReview --state /Users/jaeger/.codex/attended-review-sessions/bpr1948-go-to-line-w47r2fe5 --registry /Users/jaeger/.mask/attended-review-resources status --after 7
```

Verify the expected session, reconcile any events, persist the consumed cursor,
then use `wait --after CHECKED_SEQUENCE --kind handed-back --timeout 43200`
under Driver waiting's qualified outer-call procedure. A handback already
saved must be consumed immediately. On later engineering, query `conflicts`
and release only affected candidates explicitly; use a fresh implementer round.
Use the same common arguments with `stop` to stop only this server after its
review is finished and command ownership is reconciled.

All freeze/configure/build/check commands completed. The stopped incomplete
root is gone. The ordinary development worktree and unrelated main writer were
preserved. Only this explicitly handed-off review server remains task-owned
and running; the owner may independently launch the frozen application.
