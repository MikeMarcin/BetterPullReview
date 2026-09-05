# Repaired sign-in result

Result: **live sign-in succeeded as MikeMarcin; cleanup completed; one-launch
procedure deviation recorded below**. No PR opened or publication occurred.

The approved executable SHA-256
`3dd5c3bb794ae0ef6138e4921b21f4bde8dd7131874d8f5f01609464d5913a16`
launched at 2026-09-05T09:43:41.983192Z, PID 20562, after exact packet and binary
revalidation. The child had no arguments, the specified environment exclusions,
and discarded stdout/stderr. Local log baseline: 4,350 bytes.

## Functional observation

- The agent detected completion directly from new sanitized log records; the
  operator did not need to report completion or copy an error message.
- Viewer observation succeeded. Capability discovery started at
  09:43:58.724800182Z; nine response records reported HTTP 200 with absent error
  summaries, followed by capability success at 09:44:05.215650062Z. This is about
  6.49 seconds for this single capability course, not a latency benchmark.
- Native accessibility showed `GitHub account connected.` and empty terminal
  device-code/address fields. The connection-details screenshot showed
  `@MikeMarcin`, GitHub.com Native HTTPS, and all four capabilities supported.
  No pull request was open. The screenshot was inspected in-session, not saved
  as a repository artifact.
- The UI displayed GraphQL 4,989 of 5,000 remaining. No pre-run rate baseline
  was captured, so the point cost of this batch cannot be derived from that
  number. No extra query was made to measure cost.
- Explicit Sign out produced `No GitHub host is connected.` and disabled Open
  pull request. Normal close produced shutdown at 09:45:33.656325271Z and the
  launched child's observed exit code was 0. Vault erasure was not inspected
  through a credential API; the explicit UI cleanup path was exercised.

## Procedure deviation and recovery

After clicking Close, the agent called the UI automation handle's
`getAXState()` to verify closure. That inspection automatically relaunched the
app, contrary to the packet's one-launch limit. The new process was PID 20815;
the log recorded startup at 09:45:35.936394656Z. Its UI was signed out with empty
device fields; its log segment contained no viewer or capability operations.
No second sign-in, credential entry, PR action or publication was performed.

The agent disclosed the deviation immediately and closed that signed-out
instance through its ordinary Close control. Shutdown was logged at
09:46:04.962363377Z; process-name checks then found no BetterPullReview instance.
Its exit code is unavailable because it was launched by the UI tool, not the
retained child wrapper. Do not represent this course as strict one-launch
procedural compliance. The first instance's observed functional success and
explicit Sign out remain valid observations.

For future courses, verify process absence and logged shutdown after closing;
do not call app UI handles or getters after quit, because those can relaunch.
The owner's request to detect sign-in completion automatically is retained in
the operating instructions.

The local log ends at 6,877 bytes and remains owner-local. Only this minimal
finding is committed; the complete log, credentials and authentication screens
are not retained in Git. No further authority is created; v005 is consumed.
