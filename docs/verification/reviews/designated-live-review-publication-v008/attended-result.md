# v008 attended result — 2026-09-05

Result: **aborted for an agent UI-targeting mistake; no PR opened or comment
published; explicit Sign out and shutdown completed for both instances**.
This is not a clean one-launch or live-publication acceptance pass.

## Approved launch and observed boundary

The exact owner response is frozen in `operator-response.md`. Removing only
Markdown quote prefixes and blank-line presentation matched the packet exactly.
Its eight artifact hashes/lengths and all 37 current/committed source entries
revalidated before materialization. The namespace identity and historical spent
markers were unchanged; v008 was unspent before this invocation.

The private one-link mode-0600 approval contained 1,576 bytes, SHA-256
`560e7882cd70c01616f9aea5dd32dc743435858d6d3c3d73139e243c513fa189`.
The fresh static graph passed configured isolation: 281 tests, exactly one live
lane, fixed preloader/launcher/image/source/mount/approval arguments, 1,800-second
timeout, 24 environment removals and no verdict overrides.

The exact CTest command ran once at 2026-09-05T10:16:55.694371Z:

```sh
ctest --test-dir build/bpr0256-v008-live-manifest --output-on-failure -R '^live_review_mutation_designated_target$'
```

It launched approved PID 33958 from the stored image under
`build/bpr0256-v008-live-mount-parent/mount-204bcec221fc6656/capsule/BetterPullReview.app`.
The application consumed/unlinked the approval and created its 77-byte spent
marker. The owner-local sanitized log baseline was 6,877 bytes. This instance
recorded startup at 10:16:58.508361993Z and shutdown at 10:31:47.678624413Z;
there were no intervening viewer/capability records. Its final observed UI was
signed out, with empty locator/device fields and Open PR disabled.

## UI-targeting and capture deviations

The agent selected the UI with `cua.getApp("BetterPullReview")`. That name-based
selection launched/selected the ordinary build at
`build/app-release/src/app/BetterPullReview.app`, PID 34019, alongside the
approved mounted process. A process-name check before the getter proved an app
existed but did not bind the getter to that process. This violated the exact
build and one-launch procedure. No extra instance was authorized.

The agent initially monitored the approved app's log while observing this
ordinary instance's UI. It later requested a device code and browser handoff
in the ordinary instance and filled only the local repository owner/name fields.
Its UI reported a connected account, but the acting account was not verified.
This is an unauthorized wrong-build sign-in interaction, not sign-in evidence
for the approved capsule or a new MikeMarcin acceptance claim. The ordinary
instance's log/credential store was not separately inspected.

Based on the connected status, the agent also requested a screenshot while the
device-code panel remained present. The in-session capture therefore violated
the no-code-capture procedure. No screenshot or code value is saved in Git or
reproduced in this result. No code was copied into chat text or a browser by
the agent. Do not claim clean capture compliance.

The agent incorrectly suggested the connection label might be premature.
Read-only source inspection established that it represents an accepted session;
the decisive runtime explanation was the two different executable paths found
with `pgrep` and `lsof`. No application status or capability regression was
established. The mistake was disclosed before any PR open or Publish action.

## Cleanup and remaining scope

The agent aborted publication, clicked explicit Sign out in the ordinary
instance, observed disconnected status and disabled Open PR, then quit it.
Process inspection showed only approved PID 33958 remained. Selecting that
already-running application by its full mounted `.app` path reached its
signed-out UI without another instance. The agent clicked explicit Sign out
there, observed disconnected status, disabled Open PR and inactive device flow,
and quit normally. No app getter/handle was used after either quit.

The retained CTest process returned exit 0, 1/1 passing in 892.24 seconds, at
2026-09-05T10:31:47.956967Z. This proves normal lane termination/cleanup, not
publication success. Subsequent process and DiskImages inspection found zero
BetterPullReview instances, zero v008 attachments and an empty mount parent.
The log ends at 7,124 bytes and remains owner-local. Sign out was exercised;
credential-slot erasure was not independently probed.

No PR was opened, no source line/body selected, and Publish was never pressed.
No comment, reply, review or web publication observation occurred. v008 is
consumed and cannot be retried. BPR-0256, BPR-0235, BPR-0142 and RFC-BPR-0008
remain open. Future preparation must bind UI selection to the verified full
mounted application path, check exact process/executable identity immediately
before and after selection, and stop on a second instance before sign-in. Use
filtered accessibility output while device fields may be populated; never
screenshot them. The approved source/binary was not changed by this procedure
failure, so its unchanged preparation evidence remains reusable.
