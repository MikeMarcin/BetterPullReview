# Sign-in-only diagnostic checkpoint

Status: **pending operator decision; no publication authority**.

## Decision

Authorize one attended browser/device sign-in as **MikeMarcin** using the exact
macOS application below, read its resulting connection status, use **Copy
message** if it fails, then explicitly **Sign out** and quit. This check
identifies the failing session-bootstrap step. It does not claim the original
sign-in failure fixed.

The v007 publication run is spent. This checkpoint starts the ordinary
application with no publication approval or command-line arguments. Do not
open a PR, publish, reply, edit, submit a review, or retry sign-in. Only the
normal device authorization and application session reads are authorized.

## Exact build and evidence

- Source commit: `5a616354ae6ecd7ebbea608a9523a8388f590ab0`.
- Executable: `/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview`.
- SHA-256: `5e27a59a8e4c0c663b36626860da9368ab62b2ca5bf56e09d63c25d92c0ac1ec`.
- Length: 10,656,768 bytes.
- Configuration: Release, `BUILD_TESTING=OFF`, designated live capsule `OFF`.
- Copy-message fix: BPR-0560, six Qt/preview selectors passed at three scales.
- Safe stage diagnostics: BPR-0559, host and composed Qt checks passed 4/4;
  integrator repeat 3.13 seconds. Dev/Release builds, scoped strict tidy,
  formatting and documentation pass. No parser or authority check was relaxed.

The normal production mutation path still requires an execution grant. This
run supplies none: it does not use the designated live CTest command, approval
file or supervisor. This is an attended instruction bound to this executable,
not a claim of a new machine-enforced one-use diagnostic gate. The agent must
not use this response for a second launch or a changed binary.

## Procedure after approval

1. Verify this packet and the executable digest again; abort on drift. Close
   any existing BetterPullReview instance. Launch the exact executable with no
   arguments, clearing the same 24 loader/Python/GitHub credential environment
   variables used by the earlier designated CTest lane. Never inspect or print
   their values. Do not rebuild or substitute an executable under this decision.
2. Select **GitHub.com — Native HTTPS**. Use **Get a code** and the browser
   control. The operator authorizes directly in GitHub as MikeMarcin. Do not
   copy device codes, tokens, browser cookies or credentials into chat or logs.
3. Stop at the first completed connection result. If it fails, use **Copy
   message** and return only that message. The prefix distinguishes local
   enrollment, account identity observation, capability discovery, and session
   acceptance. If it succeeds, record the displayed non-secret account name.
   Do not open a PR. A different account, unexpected prompt, or failure ends
   this attempt; do not request another code or use manual-token Connect.
4. Press **Sign out**, wait for its terminal status, and quit normally. Device
   grants otherwise persist on ordinary quit. If cleanup reports failure,
   record that failure without claiming credential erasure. Record only the
   safe UI outcome and normal process exit; retain no secret-bearing logs.

## Operator response

Return this exact statement:

> I approve one sign-in-only diagnostic run as MikeMarcin using BetterPullReview sha256:5e27a59a8e4c0c663b36626860da9368ab62b2ca5bf56e09d63c25d92c0ac1ec, followed by Copy message if needed, explicit Sign out and quit. No pull request or publication is authorized.

Or:

> I reject this sign-in-only diagnostic run.

This uses the repository's established verbatim operator workflow for an
attended credential operation. The answer binds this packet and executable;
it is separate from the answered [v007 publication packet](../designated-live-review-publication-v007/operator-packet.md).
