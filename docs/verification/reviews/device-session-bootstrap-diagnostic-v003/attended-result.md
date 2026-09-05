# V003 attended diagnostic result

The owner approved the exact sign-in-only build and then reported “done”.
The approved executable ran once as PID 67244, with no arguments and the
packet's filtered child environment. No PR was opened or publication attempted.

## Observed terminal message

```text
Connection unavailable
Capability discovery: GitHub returned data that BetterPullReview could not safely use. The capability response contained a GraphQL error envelope.
```

The native GitHub status and view-state accessibility text independently
showed this same generic error-envelope detail. No allowlisted error code,
empty/non-array category, malformed-entry category or mixed-code category was
reported. The actual server error text and code remain unknown. The generic
fallback does not establish why the server rejected the request; diagnostic
inspection exceptions also use that fallback.

## Cleanup

Copy GitHub message was pressed. Explicit Sign out was pressed, followed by
observed terminal UI “No GitHub host is connected.” and “Connect to GitHub
Choose a host profile above, then connect.” The window was closed normally;
the original process waiter reported exit 0. No vault inspection was performed,
and process exit is not evidence of vault-slot absence.

No device code, token, cookie, credential, raw server response or private
repository content was retained. This one-launch authority is consumed.
The root cause remains unresolved; no successful session, publication or RFC
closeout is claimed. Further blind expansion of the code allowlist would not
be a source-backed repair. The next investigation needs a concrete design
for a useful, bounded and safely redacted observation of the actual error.
