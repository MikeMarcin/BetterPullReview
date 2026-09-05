# Mixed-client edit, deletion and thread-state check

Status: **prepared; these additional remote operations are not yet authorized**.

The previous source-line comment and reply round trip are complete and the
owner accepted their web appearance. This checkpoint tests remaining
BPR-0143 behavior on that same disposable thread. It requires no new comment,
new sign-in diagnostic, digest paste or replacement publication capsule.

## Exact proposed operations

Use MikeMarcin's existing GitHub web session on MikeMarcin/BPR-Test#1 at head
`17990ba0c2fb50338141f35c0717f0f99639c7ba`.

1. Edit only [reply 3941498416](https://github.com/MikeMarcin/BPR-Test/pull/1#discussion_r3941498416)
   from `BetterPullReview web reply return verification.` to exactly
   `BetterPullReview web reply edit verification.`. Refresh BetterPullReview
   and verify replacement of the nested reply's text without duplication.
2. Delete that same disposable reply. Refresh BetterPullReview and verify
   its row disappears. Do not recreate it.
3. Resolve the existing [parent thread at comment 3940608913](https://github.com/MikeMarcin/BPR-Test/pull/1#discussion_r3940608913).
   Refresh BetterPullReview and verify `Resolved review thread`.
4. Reopen that same thread. Refresh BetterPullReview and verify
   `Open review thread`.

Final intended server state: the parent comment remains unchanged and its
thread is open; the disposable web reply is deleted. The parent body remains
`BetterPullReview attended round-trip verification.` on RIGHT line 1 of
`bpr-live-review-fixture.txt`.

## Execution and evidence

Use the ordinary Release app and remembered MikeMarcin session. Before editing,
confirm the browser account, designated PR/head, exact parent and reply, and
capture the current app baseline through its ordinary Refresh action. Stop on
identity drift or unavailable permission. Verify each exact editor value in a
fresh UI observation before submitting once. If submission is uncertain,
inspect the server state before taking another action.

After each web change, use the app's public Refresh pull request action and
observe body, author, parent nesting, row presence and thread heading. Record
actual GitHub permalinks/IDs separately from app-visible observations; the
current app retains canonical IDs in its model but does not display them.
Do not claim a visibly observed app numeric ID or an editable-local-conflict
case that the current UI cannot expose. Existing deterministic identity and
server-wins fixtures remain supporting evidence, not invented live output.

Reading sanitized logs and equivalent local diagnosis/rebuild/reopen are
covered by standing owner instructions. Do not post a new reply to diagnose a
failure. Quit without Sign out and preserve remembered sign-in. This packet
does not authorize other comments/reviews, deletion of the parent, local
storage removal, account changes or mutations on another PR.

## Decision

A plain instruction authorizing the four exact operations is sufficient.
The previous approval authorized creation of the reply; this sequence adds its
edit/deletion and thread-state changes. Record the actual owner response only
after it arrives. This authorization is separate from acceptance results and
does not by itself close BPR-0143 or RFC0008.

Database-loss/uninstall acceptance remains a separate scoped check; no local
state will be erased under this packet.
